#include "ProfilePlugin.h"
#include "ProfileSystemsPlugin.h"
#include "ProfileAllocatorPlugin.h"
#include "ProfileDrawCallsPlugin.h"
#include "ProfileFileAccessPlugin.h"
#include "ProfileDevicePlugin.h"
#include "ProfileResourcesPlugin.h"
#include "Visual/GUI2/imgui/imgui.h"
#include "Visual/GUI2/Icons.h"

#include "Visual/Renderer/RendererSubsystem.h"
#include "Visual/Device/Device.h"
#include "Visual/Utility/JsonUtility.h"
#include "Common/Engine/Status.h"
#include "Common/Engine/NetworkStatus.h"
#include "Common/Utility/HighResTimer.h"
#include "Include/magic_enum/magic_enum.hpp"

#if defined(_XBOX_ONE)
	#define PROFILE_PLUGIN_VRAM_STATS_INCLUDED
#endif

namespace Engine::Plugins
{
#if DEBUG_GUI_ENABLED && defined(PROFILING)
	namespace {
		class TimeBar {
		private:
			static constexpr float PixelsPer60FPS = 400.0f;
			static constexpr float Scale = (60 * PixelsPer60FPS) / 1'000'000;
			static constexpr int64_t FrameLength = 1'000'000 / 60;

			ImRect render_region;
			ImVec2 padding;
			int64_t time_start;
			int64_t time_end;
			float scale_factor;
			bool render_scale;
			bool has_border;
			bool hovered;

		public:
			bool Begin(float scale_f, int64_t start, int64_t end, bool scale = true, bool border = false)
			{
				if (end <= start)
					return false;

				auto visual_end = end;
				if (scale)
				{
					if (const auto r = (visual_end - start) % FrameLength; r > 0)
						visual_end += FrameLength - r;
				}

				ImGuiWindow* window = ImGui::GetCurrentWindow();
				if (window->SkipItems)
					return false;

				const float width = std::min((visual_end - start) * Scale * scale_f, ImGui::GetContentRegionAvail().x);
				const auto size = ImVec2(width, ImGui::GetFontSize());
				const auto frame_bb = ImRect(window->DC.CursorPos, window->DC.CursorPos + size);

				ImGui::ItemSize(size);
				if (!ImGui::ItemAdd(frame_bb, 0))
					return false;

				hovered = ImGui::ItemHoverable(frame_bb, 0);

				padding = ImGui::GetStyle().FramePadding;
				time_start = start;
				time_end = end;
				render_region = frame_bb;
				render_scale = scale;
				has_border = border;
				scale_factor = Scale * scale_f;

				return true;
			}

			void End()
			{
				if (render_scale)
				{
					const ImU32 col = ImGui::GetColorU32(ImGuiCol_Text, 0.5f);
					ImGuiWindow* window = ImGui::GetCurrentWindow();
					window->DrawList->AddLine(render_region.Min, ImVec2(render_region.Min.x, render_region.Max.y), col, 2.0f);

					for (int64_t t = 0; time_start + t + (FrameLength / 8) < time_end; t += FrameLength)
					{
						const auto x = ((t + FrameLength) * scale_factor) + render_region.Min.x;
						window->DrawList->AddLine(ImVec2(x, render_region.Min.y), ImVec2(x, render_region.Max.y), col, 2.0f);
					}
				}
			}

			bool Render(int64_t start, int64_t end, ImU32 color)
			{
				start = std::max(start, time_start);
				end = std::min(end, time_end);

				if (end <= start)
					return false;

				const float lx = std::clamp(((start - time_start) * scale_factor) + render_region.Min.x, render_region.Min.x, render_region.Max.x);
				const float rx = std::clamp(((end - time_start) * scale_factor) + render_region.Min.x, render_region.Min.x, render_region.Max.x);
				const auto p_min = ImVec2(lx, render_region.Min.y + padding.y);
				const auto p_max = ImVec2(rx, render_region.Max.y - padding.y);

				ImGuiContext& g = *GImGui;
				ImGuiWindow* window = g.CurrentWindow;
				window->DrawList->AddRectFilled(p_min, p_max, color, 0.0f);
				if (has_border)
				{
					window->DrawList->AddRect(p_min + ImVec2(1, 1), p_max + ImVec2(1, 1), ImGui::GetColorU32(ImGuiCol_BorderShadow), 0.0f, 0, 1.0f);
					window->DrawList->AddRect(p_min, p_max, ImGui::GetColorU32(ImGuiCol_Border), 0.0f, 0, 1.0f);
				}

				return hovered && ImGui::GetMousePos().x >= lx && ImGui::GetMousePos().x < rx;
			}
		};

		class JobProfiler : public ProfilePlugin::Page {
		private:
			static constexpr size_t NumPages = 2;
			static constexpr size_t MaxNumCaptures = 2048;
			static inline std::atomic<JobProfiler*> instance = nullptr;

			enum Group
			{
				Group_None = 0,
				Group_GPU,
				Group_Game,
				Group_Entities,
				Group_Physics,
				Group_Render,
				Group_Shader,
				Group_Texture,
				Group_Sync,
				Group_Loading,
				Group_Other,
			};

			enum class SpecialTrack {
				Server,
				GPU,
			};

			struct Category {
				ImU32 color;
				Group group;

				Category() : color(0), group(Group_None) {}
				Category(const ImColor& color, Group group) : color(color), group(group) {}
			};

			struct Capture {
				Job2::Profile hash;
				int64_t time;

				Capture() : hash(Job2::Profile::Unknown), time(0) {}
				Capture(Job2::Profile hash, int64_t time) : hash(hash), time(time) {}
			};

			class Track {
			private:
				const Job2::Type type;
				const unsigned type_index;

				Job2::Profile last = Job2::Profile::Unknown;
				int64_t last_time = 0;

				std::array<Memory::Array<Capture, MaxNumCaptures>, NumPages> captures;

			public:
				Track(Job2::Type type, unsigned type_index, size_t current_page) : type(type), type_index(type_index)
				{
					Swap(HighResTimer::Get().GetTimeUs(), current_page);
				}

				std::string_view GetName() const 
				{
					if (type < Job2::Type::Count)
						return Job2::TypeName(type);
					else
						return magic_enum::enum_name(SpecialTrack(type_index));
				}

				Job2::Type GetType() const { return type; }
				unsigned GetIndex() const { return type_index; }

				void Swap(int64_t now, size_t current_page)
				{
					captures[current_page].clear();
					captures[current_page].emplace_back(last, now);
				}

				const auto& GetCaptures(size_t current_page) const { return captures[current_page]; }

				void Push(Job2::Profile hash, uint64_t time, size_t current_page)
				{
					last = hash;
					last_time = int64_t(time);

					if (captures[current_page].size() < captures[current_page].max_size())
						captures[current_page].emplace_back(hash, int64_t(time));
				}
			};

			struct GroupProfileMap {
				std::array<Job2::Profile, magic_enum::enum_count<Job2::Profile>()> profiles;
				std::array<std::pair<size_t, size_t>, magic_enum::enum_count<Group>()> groups;
			};

			std::atomic<bool> enabled = true;
			std::array<Category, magic_enum::enum_count<Job2::Profile>()> categories;
			GroupProfileMap group_map;
			size_t current_page = 0;
			Memory::Vector<Track, Memory::Tag::Profile> tracks;
			std::array<int64_t, NumPages> frame_start;
			std::array<int64_t, NumPages> frame_end;

			typedef std::array<std::pair<int64_t, unsigned>, magic_enum::enum_count<Job2::Profile>()> Accumulated;

			Accumulated GatherAccumulated() const
			{
				const auto page = GetLastPage();
				Accumulated accumulated;
				for (auto& a : accumulated)
				{
					a.first = 0;
					a.second = 0;
				}

				for (const auto& track : tracks)
				{
					const auto& captures = track.GetCaptures(page);
					if (captures.empty())
						continue;

					int64_t start = frame_start[page];
					const int64_t end = frame_end[page];
					if (start >= end)
						continue;

					Job2::Profile last_tag = Job2::Profile::Unknown;
					for (const auto& a : captures)
					{
						if (a.time > start)
						{
							const auto duration = std::min(a.time, end) - start;
							if (track.GetType() != Job2::Type::Count || last_tag != Job2::Profile::Unknown)
							{
								accumulated[size_t(last_tag)].first += duration;
								accumulated[size_t(last_tag)].second++;
							}
						}

						start = a.time;
						last_tag = a.hash;

						if (start >= end)
							break;
					}

					if (start < end)
					{
						const auto duration = end - start;
						if (track.GetType() != Job2::Type::Count || last_tag != Job2::Profile::Unknown)
						{
							accumulated[size_t(last_tag)].first += duration;
							accumulated[size_t(last_tag)].second++;
						}
					}
				}

				return accumulated;
			}

			void SetupCategories()
			{
				categories[size_t(Job2::Profile::Unknown)]					= Category(ImColor( 64,  64,  64), Group_None);
				categories[size_t(Job2::Profile::Server)]					= Category(ImColor(  0, 128,   0), Group_None);
				categories[size_t(Job2::Profile::PassShadow)]				= Category(ImColor(128, 128,   0), Group_GPU);
				categories[size_t(Job2::Profile::PassZPrepass)]				= Category(ImColor(128,   0, 128), Group_GPU);
				categories[size_t(Job2::Profile::PassMainOpaque)]			= Category(ImColor(160, 160,   0), Group_GPU);
				categories[size_t(Job2::Profile::PassMainTransparent)]		= Category(ImColor(255, 255,   0), Group_GPU);
				categories[size_t(Job2::Profile::PassWater)]				= Category(ImColor(  0,   0, 255), Group_GPU);
				categories[size_t(Job2::Profile::PassGlobalIllumination)]	= Category(ImColor(255,   0,   0), Group_GPU);
				categories[size_t(Job2::Profile::PassScreenspaceShadows)]	= Category(ImColor(128, 128,   0), Group_GPU);
				categories[size_t(Job2::Profile::PassColorResampler)]		= Category(ImColor(255,   0, 255), Group_GPU);
				categories[size_t(Job2::Profile::PassPostProcess)]			= Category(ImColor(128,   0,   0), Group_GPU);
				categories[size_t(Job2::Profile::PassCompute)]				= Category(ImColor(  0,   0, 255), Group_GPU);
				categories[size_t(Job2::Profile::PassVSync)]				= Category(ImColor(  0, 128,   0), Group_GPU);
				categories[size_t(Job2::Profile::Debug)]					= Category(ImColor(  0, 255,   0), Group_Game);
				categories[size_t(Job2::Profile::FrameLoopMove)]			= Category(ImColor(128,   0,   0), Group_Game);
				categories[size_t(Job2::Profile::UserInterfaceInit)]		= Category(ImColor(  0, 255, 255), Group_Game);
				categories[size_t(Job2::Profile::UserInterfaceUpdate)]		= Category(ImColor(  0, 255, 255), Group_Game);
				categories[size_t(Job2::Profile::UserInterfaceRecord)]		= Category(ImColor(  0, 128, 128), Group_Game);
				categories[size_t(Job2::Profile::Simulation)]				= Category(ImColor(  0, 128,   0), Group_Game);
				categories[size_t(Job2::Profile::Interpolate)]				= Category(ImColor(128,   0, 128), Group_Game);
				categories[size_t(Job2::Profile::Preloading)]				= Category(ImColor(255, 255, 255), Group_Loading);
				categories[size_t(Job2::Profile::Doodads)]					= Category(ImColor(128,   0,   0), Group_Other);
				categories[size_t(Job2::Profile::Shader)]					= Category(ImColor(  0, 255,   0), Group_Shader);
				categories[size_t(Job2::Profile::ShaderProgram)]			= Category(ImColor(  0, 255,   0), Group_Shader);
				categories[size_t(Job2::Profile::ShaderPipeline)]			= Category(ImColor(255,   0, 255), Group_Shader);
				categories[size_t(Job2::Profile::ShaderUncompress)]			= Category(ImColor(255, 255,   0), Group_Shader);
				categories[size_t(Job2::Profile::ShaderAccess)]				= Category(ImColor(255,   0,   0), Group_Shader);
				categories[size_t(Job2::Profile::ShaderCompilation)]		= Category(ImColor(  0, 255, 255), Group_Shader);
				categories[size_t(Job2::Profile::ShaderSave)]				= Category(ImColor(128, 128, 128), Group_Shader);
				categories[size_t(Job2::Profile::TileLighting)]				= Category(ImColor(255,   0,   0), Group_Render);
				categories[size_t(Job2::Profile::Entities)]					= Category(ImColor(128,   0, 255), Group_Entities);
				categories[size_t(Job2::Profile::EntitiesFlush)]			= Category(ImColor(128,   0, 228), Group_Entities);
				categories[size_t(Job2::Profile::EntitiesParticles)]		= Category(ImColor(128,   0, 208), Group_Entities);
				categories[size_t(Job2::Profile::EntitiesTrails)]			= Category(ImColor(128,   0, 168), Group_Entities);
				categories[size_t(Job2::Profile::EntitiesCull)]				= Category(ImColor(128,   0, 138), Group_Entities);
				categories[size_t(Job2::Profile::EntitiesRender)]			= Category(ImColor(128,   0, 108), Group_Entities);
				categories[size_t(Job2::Profile::EntitiesSort)]				= Category(ImColor(128,   0,  58), Group_Entities);
				categories[size_t(Job2::Profile::File)]						= Category(ImColor(255, 255,   0), Group_Other);
				categories[size_t(Job2::Profile::Animation)]				= Category(ImColor(255, 255, 255), Group_Entities);
				categories[size_t(Job2::Profile::Physics)]					= Category(ImColor(236, 240, 241), Group_Physics);
				categories[size_t(Job2::Profile::FlowMap)]					= Category(ImColor(  0, 128, 128), Group_Physics);
				categories[size_t(Job2::Profile::ControlManager)]			= Category(ImColor(  0, 255,   0), Group_Game);
				categories[size_t(Job2::Profile::HighlightedObject)]		= Category(ImColor(255, 255,   0), Group_Game);
				categories[size_t(Job2::Profile::DeviceBeginFrame)]			= Category(ImColor(255,   0, 255), Group_Render);
				categories[size_t(Job2::Profile::DeviceEndFrame)]			= Category(ImColor(255,   0, 255), Group_Render);
				categories[size_t(Job2::Profile::DevicePresent)]			= Category(ImColor(255,   0,   0), Group_Sync);
				categories[size_t(Job2::Profile::Swap)]						= Category(ImColor(128,   0, 128), Group_Sync);
				categories[size_t(Job2::Profile::Limiter)]					= Category(ImColor(  0,   0, 128), Group_Sync);
				categories[size_t(Job2::Profile::Prepare)]					= Category(ImColor(255, 128,   0), Group_Render);
				categories[size_t(Job2::Profile::RecordOther)]				= Category(ImColor(128, 128,   0), Group_Render);
				categories[size_t(Job2::Profile::RecordShadow)]				= Category(ImColor( 88,  88,   0), Group_Render);
				categories[size_t(Job2::Profile::RecordZPrepass)]			= Category(ImColor(108, 108,   0), Group_Render);
				categories[size_t(Job2::Profile::RecordOpaque)]				= Category(ImColor(168, 168,   0), Group_Render);
				categories[size_t(Job2::Profile::RecordAlpha)]				= Category(ImColor(208, 208,   0), Group_Render);
				categories[size_t(Job2::Profile::RecordPostProcess)]		= Category(ImColor(228, 228,   0), Group_Render);
				categories[size_t(Job2::Profile::Render)]					= Category(ImColor(255, 255,   0), Group_Render);
				categories[size_t(Job2::Profile::Sound)]					= Category(ImColor(  0,   0, 128), Group_Other);
				categories[size_t(Job2::Profile::LoadShaders)]				= Category(ImColor(  0, 128,   0), Group_Loading);
				categories[size_t(Job2::Profile::Registration)]				= Category(ImColor(  0, 255, 255), Group_Other);
				categories[size_t(Job2::Profile::ItemFilter)]				= Category(ImColor(  0, 128,   0), Group_Game);
				categories[size_t(Job2::Profile::AssetStreaming)]			= Category(ImColor(255, 128,   0), Group_Loading);
				categories[size_t(Job2::Profile::Texture)]					= Category(ImColor(255, 128,   0), Group_Texture);
				categories[size_t(Job2::Profile::TextureLevel)]				= Category(ImColor(255, 128,   0), Group_Texture);
				categories[size_t(Job2::Profile::Storage)]					= Category(ImColor(  0, 128, 255), Group_Loading);
				categories[size_t(Job2::Profile::StorageSave)]				= Category(ImColor(  0, 128, 255), Group_Loading);
				categories[size_t(Job2::Profile::ResourceCache)]			= Category(ImColor(128,   0, 128), Group_Loading);
				categories[size_t(Job2::Profile::Trail)]					= Category(ImColor(  0, 240, 128), Group_Render);
				categories[size_t(Job2::Profile::Particle)]					= Category(ImColor(  0, 140, 128), Group_Render);
			}

			void BuildGroupMap()
			{
				std::array<size_t, magic_enum::enum_count<Group>()> counts;
				for (auto& a : counts)
					a = 0;

				for (const auto& a : categories)
					if (a.color != 0 && size_t(a.group) < counts.size())
						counts[size_t(a.group)]++;

				for (size_t a = 1; a < counts.size(); a++)
					counts[a] += counts[a - 1];

				for (size_t a = 0; a < counts.size(); a++)
					group_map.groups[a].second = counts[a];

				for (const auto hash : magic_enum::enum_values<Job2::Profile>())
				{
					const auto& c = categories[size_t(hash)];
					if (c.color != 0 && size_t(c.group) < counts.size())
						group_map.profiles[--counts[size_t(c.group)]] = hash;
				}

				for (size_t a = 0; a < counts.size(); a++)
					group_map.groups[a].first = counts[a];
			}

			size_t GetLastPage() const { return (current_page + NumPages - 1) % NumPages; }

			void RenderMemoryBar() const
			{
				ImGuiWindow* window = ImGui::GetCurrentWindow();
				if (window->SkipItems)
					return;

				const auto size = ImVec2(ImGui::GetFontSize(), ImGui::GetContentRegionAvail().y);
				const auto frame_bb = ImRect(window->DC.CursorPos, window->DC.CursorPos + size);

				ImGui::ItemSize(size);
				if (!ImGui::ItemAdd(frame_bb, 0))
					return;

				const auto& memory_stats = Memory::GetStats();

				const auto num_segments = (memory_stats.total_size + Memory::GB - 1) / Memory::GB;
				const auto total_size = num_segments * Memory::GB;
				if (total_size == 0)
					return;

				const auto storage_size = memory_stats.tag_used_sizes[(unsigned)Memory::Tag::StorageData].load(std::memory_order_relaxed);
				const auto texture_size = memory_stats.tag_used_sizes[(unsigned)Memory::Tag::Texture].load(std::memory_order_relaxed);
				auto remaining_size = memory_stats.used_size - storage_size - texture_size;

				const auto video_size = GetDevice()->GetSize();

#if defined(PROFILE_PLUGIN_VRAM_STATS_INCLUDED)
				// Subtract video memory if it's already included into memory_stats.used_size to draw it again with different color
				remaining_size -= video_size;
#endif

				float start = frame_bb.Min.y;
				const auto RenderRegion = [&](uint64_t value, const ImColor& color)
				{
					const auto h = std::clamp(float(static_cast<long double>(value) / total_size), 0.0f, 1.0f) * frame_bb.GetHeight();
					if (h > 0.5f)
					{
						const auto top = std::clamp(start, frame_bb.Min.y, frame_bb.Max.y);
						const auto bottom = std::clamp(start + h, frame_bb.Min.y, frame_bb.Max.y);
						start += h;

						window->DrawList->AddRectFilled(ImVec2(frame_bb.Min.x, top), ImVec2(frame_bb.Max.x, bottom), color);
					}
				};

				RenderRegion(storage_size, ImColor(128, 0, 0));
				RenderRegion(texture_size, ImColor(0, 0, 128));
				RenderRegion(remaining_size, ImColor(255, 128, 0));
				RenderRegion(video_size, ImColor(0, 128, 0));

				const auto border_color = ImGui::GetColorU32(ImGuiCol_Text, 0.5f);
				window->DrawList->AddRect(frame_bb.Min, frame_bb.Max, border_color);
				for (uint64_t a = 1; a < num_segments; a++)
				{
					float h = std::clamp(float(static_cast<long double>(a * Memory::GB) / total_size), 0.0f, 1.0f);
					h = ImLerp(frame_bb.Min.y, frame_bb.Max.y, h);
					window->DrawList->AddLine(ImVec2(frame_bb.Min.x, h), ImVec2(frame_bb.Max.x, h), border_color);
				}
			}

		public:
			static JobProfiler* Get() { return instance.load(std::memory_order_consume); }

			float GPUScale = 1.0f;
			float CPUScale = 1.0f;

			JobProfiler() : Page("Jobs")
			{
				SetupCategories();
				BuildGroupMap();

				const auto job_stats = Job2::System::Get().GetStats();
				size_t total_track_count = 2;
				for (unsigned a = 0; a < (unsigned)Job2::Type::Count; a++)
					total_track_count += std::max(job_stats.worker_counts[a], unsigned(1));

				tracks.reserve(total_track_count);
				tracks.emplace_back(Job2::Type::Count, unsigned(SpecialTrack::Server), current_page);
				tracks.emplace_back(Job2::Type::Count, unsigned(SpecialTrack::GPU), current_page);
				for (unsigned a = 0; a < unsigned(Job2::Type::Count); a++)
					for (unsigned b = 0; b < std::max(job_stats.worker_counts[a], unsigned(1)); b++)
						tracks.emplace_back(Job2::Type(a), b, current_page);

				for (size_t a = 0; a < NumPages; a++)
					frame_start[a] = frame_end[a] = HighResTimer::Get().GetTimeUs();

				instance.store(this, std::memory_order_release);
			}

			void PushGPU(Job2::Profile hash, uint64_t begin, uint64_t end)
			{
				tracks[1].Push(hash, frame_start[current_page] + begin, current_page);
				tracks[1].Push(Job2::Profile::Unknown, frame_start[current_page] + end, current_page);
			}

			void PushServer(Job2::Profile hash, uint64_t begin, uint64_t end)
			{
				tracks[0].Push(hash, frame_start[current_page] + begin, current_page);
				tracks[0].Push(Job2::Profile::Unknown, frame_start[current_page] + end, current_page);
			}

			void Push(Job2::Profile hash, unsigned index, bool end)
			{
				tracks[2 + index].Push(end ? Job2::Profile::Unknown : hash, HighResTimer::Get().GetTimeUs(), current_page);
			}

			void Swap()
			{
				const auto now = HighResTimer::Get().GetTimeUs();
				const auto last_page = current_page;
				current_page = (current_page + 1) % NumPages;

				for (auto& track : tracks)
					track.Swap(now, current_page);

				frame_end[last_page] = now;
				frame_start[current_page] = now;
			}

			void RenderTimeline(float scale_factor) const
			{
				RenderMemoryBar();
				ImGui::SameLine();

				ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 0));
				ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(ImGui::GetStyle().CellPadding.x, 0));

				if (ImGui::BeginTable("##TableWrapper", 2, ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoPadOuterX))
				{
					ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);
					ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);

					TimeBar context;
					const auto page = GetLastPage();

					const auto start = frame_start[page];
					const auto end = frame_end[page];

					for (const auto& track : tracks)
					{
						ImGui::TableNextRow();

						// Name
						{
							ImGui::TableNextColumn();
							if (track.GetType() == Job2::Type::Count)
								ImGui::Text("%.*s", unsigned(track.GetName().length()), track.GetName().data());
							else
								ImGui::Text("%.*s %u", unsigned(track.GetName().length()), track.GetName().data(), track.GetIndex());
						}

						// Bar
						{
							ImGui::TableNextColumn();
							if (context.Begin(scale_factor, start, end))
							{
								const auto& captures = track.GetCaptures(page);
								int64_t skipped = 0;

								for (size_t a = 0; a < captures.size(); a++)
								{
									const int64_t rstart = captures[a].time - skipped;
									const int64_t rend = (a + 1 < captures.size() ? captures[a + 1].time : end) - skipped;
									const auto hash = captures[a].hash;
									if (hash == Job2::Profile::PassVSync)
									{
										skipped += rend - rstart;
										continue;
									}

									if (hash != Job2::Profile::Unknown && context.Render(rstart, rend, categories[unsigned(hash)].color))
									{
										const auto name = magic_enum::enum_name(hash);
										ImGui::SetTooltip("%.*s: %.1f ms", unsigned(name.length()), name.data(), float(rend - rstart) / 1000.0f);
									}
								}

								context.End();
							}
						}
					}

					ImGui::EndTable();
				}

				ImGui::PopStyleVar(2);
			}

			void RenderJobs()
			{
				if (Begin(800.0f))
				{
					char buffer[256] = { '\0' };
					const auto acc = GatherAccumulated();

					int64_t max_duration = 0;
					for (const auto hash : magic_enum::enum_values<Job2::Profile>())
						max_duration = std::max(max_duration, acc[size_t(hash)].first);

					const float time_width = ImGui::CalcTextSize("999.9 ms").x;
					const float count_width = ImGui::CalcTextSize("9999").x;
					
					auto RenderGroupRange = [&](size_t first_group, size_t last_group, float scale_factor)
					{
						const auto outer_size = ImVec2(ImGui::GetContentRegionAvail().x, 0.0f);
						if (ImGui::BeginTable("##GroupRangeTableWrapper", 4, ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoPadOuterX, outer_size))
						{
							float label_width = 0.0f;
							for (size_t group = first_group; group < last_group && group < magic_enum::enum_count<Group>(); group++)
							{
								const auto range = group_map.groups[group];
								const auto group_name = magic_enum::enum_name(Group(group)).substr(6);
								ImFormatString(buffer, std::size(buffer), "%.*s", unsigned(group_name.length()), group_name.data());
								label_width = std::max(label_width, ImGui::CalcTextSize(buffer).x + ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.x);

								for (size_t a = range.first; a < range.second; a++)
								{
									const auto hash = group_map.profiles[a];
									const auto name = magic_enum::enum_name(hash);
									ImFormatString(buffer, std::size(buffer), "%.*s", unsigned(name.length()), name.data());
									label_width = std::max(label_width, ImGui::CalcTextSize(buffer).x + ImGui::GetStyle().IndentSpacing);
								}
							}

							ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, label_width);
							ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, time_width);
							ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, count_width);
							ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);
							
							TimeBar context;
							for(size_t group = first_group; group < last_group && group < magic_enum::enum_count<Group>(); group++)
							{
								const auto range = group_map.groups[group];

								size_t group_jobs = 0;
								int64_t group_total = 0;
								for (size_t a = range.first; a < range.second; a++)
								{
									const auto hash = size_t(group_map.profiles[a]);
									group_total += acc[hash].first;
									group_jobs += acc[hash].second;
								}

								ImGui::TableNextRow();
								bool showGroup = false;

								ImGui::PushID(int(group) + 1);

								// Name
								{
									ImGui::TableNextColumn();
									const auto name = magic_enum::enum_name(Group(group)).substr(6);
									ImFormatString(buffer, std::size(buffer), "%.*s", unsigned(name.length()), name.data());
									showGroup = BeginSection("##GroupSection", buffer);
								}

								// Time
								{
									ImGui::TableNextColumn();
									ImGui::Text("%.1f ms", float(group_total) / 1000);
								}

								// Count
								{
									ImGui::TableNextColumn();
									ImGui::Text("%u", unsigned(group_jobs));
								}

								// Bar
								{
									ImGui::TableNextColumn();
									if (!showGroup && context.Begin(0, max_duration, false, IsPoppedOut()))
									{
										context.Render(0, group_total, ImColor(64, 64, 64));
										context.End();
									}
								}

								if (showGroup)
								{
									for (size_t a = range.first; a < range.second; a++)
									{
										const auto hash = group_map.profiles[a];
										ImGui::TableNextRow();

										// Name
										{
											ImGui::TableNextColumn();
											const auto name = magic_enum::enum_name(hash);
											ImGui::Text("%.*s", unsigned(name.length()), name.data());
										}

										// Time
										{
											ImGui::TableNextColumn();
											ImGui::Text("%.1f ms", float(acc[size_t(hash)].first) / 1000);
										}

										// Count
										{
											ImGui::TableNextColumn();
											ImGui::Text("%u", acc[size_t(hash)].second);
										}

										// Bar
										{
											ImGui::TableNextColumn();
											if (context.Begin(scale_factor, 0, max_duration, false, IsPoppedOut()))
											{
												context.Render(0, acc[size_t(hash)].first, categories[size_t(hash)].color);
												context.End();
											}
										}
									}

									EndSection();
								}

								ImGui::PopID();
							}

							ImGui::EndTable();
						}
					};

					ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 0));
					ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(ImGui::GetStyle().CellPadding.x, 0));

					if (ImGui::BeginTable("##OuterTableWrapper", 2, ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoPadOuterX))
					{
						ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);
						ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);

						ImGui::TableNextRow();
						if (ImGui::TableSetColumnIndex(0))
						{
							if (IsPoppedOut() && !(GetWindowFlags() & ImGuiWindowFlags_NoInputs))
							{
								ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
								ImGui::SliderFloat("##GPUScaleSlider", &GPUScale, 0.1f, 10.0f, "Scale: %.2f", ImGuiSliderFlags_AlwaysClamp);
							}

							RenderGroupRange(Group_GPU, Group_GPU + 1, GPUScale);
						}

						if (ImGui::TableSetColumnIndex(1))
						{
							if (IsPoppedOut() && !(GetWindowFlags() & ImGuiWindowFlags_NoInputs))
							{
								ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
								ImGui::SliderFloat("##CPUScaleSlider", &CPUScale, 0.1f, 10.0f, "Scale: %.2f", ImGuiSliderFlags_AlwaysClamp);
							}

							if constexpr (Group_GPU > 0)
								RenderGroupRange(0, Group_GPU, CPUScale);

							RenderGroupRange(Group_GPU + 1, magic_enum::enum_count<Group>(), CPUScale);
						}

						ImGui::EndTable();
					}

					ImGui::PopStyleVar(2);
					End();
				}
			}

			void SetEnabled(bool _enabled) { enabled.store(_enabled, std::memory_order_release); }
			bool IsEnabled() const { return enabled.load(std::memory_order_consume); }
		};

		thread_local class FixedStack
		{
			Memory::Array<Job2::Profile, 64> hashes;

		public:
			void push(Job2::Profile hash)
			{
				if (!hashes.full())
					hashes.push_back(hash);
			}

			void pop()
			{
				if (!hashes.empty())
					hashes.erase(&hashes.back());
			}

			Job2::Profile top()
			{
				if (!hashes.empty())
					return hashes.back();
				return Job2::Profile::Unknown;
			}

			bool empty() const { return hashes.empty(); }
		} profile_stack;

		void ProfilePush(Job2::Profile hash, unsigned index, bool end)
		{
			if (auto profiler = JobProfiler::Get(); profiler && profiler->IsEnabled())
				profiler->Push(hash, index, end);
		}

		void ProfileSave(unsigned index, Job2::Profile hash)
		{
			if (!profile_stack.empty())
				ProfilePush(profile_stack.top(), index, true);
			profile_stack.push(hash);
			ProfilePush(hash, index, false);
		}

		void ProfileRestore(unsigned index, Job2::Profile hash)
		{
			profile_stack.pop();
			ProfilePush(hash, index, true);
			if (!profile_stack.empty())
				ProfilePush(profile_stack.top(), index, false);
		}

		bool pages_clickthrough = false;
		float timeline_scale_factor = 1.0f;
	}

	class ProfilePlugin::Impl
	{
	private:
		static constexpr const char* SettingsName = "Engine.ProfilePlugin";
		static constexpr unsigned SettingsVersion = 2;

		Plugin* const parent;
		bool summary_visible = false;
		bool summary_popped_out = false;
		bool summary_main_viewport = false;
		bool popout_visible = false;
		bool timeline_visible = false;
		bool timeline_popped_out = false;
		bool timeline_main_viewport = false;
		size_t current_page = 0;
		Memory::SmallVector<Page*, 8, Memory::Tag::Unknown> pages;
		JobProfiler job_profiler;

		size_t toggle_enum_ids[magic_enum::enum_count<ProfilePlugin::PageType>()] = { 0 };

		void HideAll()
		{
			summary_visible = false;
			for (const auto& a : pages)
				a->SetPageVisible(false);
		}

		void SetPopoutVisible(bool v)
		{
			if (v != popout_visible)
			{
				popout_visible = v;
				for (const auto& a : pages)
					a->SetPopoutVisible(v);
			}
		}

		void TogglePage(size_t id)
		{
			bool is_visible = false;
			if (id > 0)
				is_visible = !pages[id - 1]->IsPoppedOut() && pages[id - 1]->IsVisible();
			else
				is_visible = summary_visible;

			if (popout_visible)
			{
				is_visible = is_visible || timeline_popped_out;
				for (const auto& page : pages)
					is_visible = is_visible || page->IsPoppedOut();
			}

			HideAll();
			current_page = id;

			if (is_visible)
			{
				timeline_visible = false;
				SetPopoutVisible(false);
			}
			else
			{
				SetPopoutVisible(true);
				timeline_visible = true;
				if (id > 0)
					pages[id - 1]->SetPageVisible(true);
				else
					summary_visible = true;
			}
		}

		template<typename T, typename... Args> T* AddPage(uint8_t key, Args&&... args)
		{
			auto r = PluginManager::Get().RegisterPlugin<T>(std::forward<Args>(args)...);
			pages.push_back(r);

			const auto action_name = std::string("Show ") + r->GetName();
			parent->AddAction(action_name, key, Input::Modifier_Ctrl, [this, r, id = pages.size()]()
			{ 
				const bool show = r->IsPoppedOut() || !r->IsVisible();
				HideAll();
				current_page = id;
				r->SetPageVisible(show);
				timeline_visible = (r->IsPoppedOut() ? !timeline_visible : show) || timeline_popped_out;
				SetPopoutVisible(true);
			});

			return r;
		}

		void RenderTimeline()
		{
			if (timeline_popped_out)
			{
				if (popout_visible)
				{
					const auto flags = pages_clickthrough && timeline_main_viewport ? ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoFocusOnAppearing : ImGuiWindowFlags_None;

					timeline_main_viewport = false;
					if (ImGui::Begin("Job Timeline##ProfilePlugin", &timeline_popped_out, flags))
					{
						timeline_main_viewport = ImGui::GetCurrentWindow()->Viewport == ImGui::GetMainViewport();

						if (!(flags & ImGuiWindowFlags_NoInputs))
							ImGui::SliderFloat("##ScaleSlider", &timeline_scale_factor, 0.1f, 10.0f, "Scale: %.2f", ImGuiSliderFlags_AlwaysClamp);

						job_profiler.RenderTimeline(timeline_scale_factor);
					}

					ImGui::End();
				}
			}
			else
			{
				timeline_main_viewport = false;
				auto viewport = ImGui::GetMainViewport();
				if (!viewport)
					return;

				const auto flags = ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking;
				const auto offset = ImVec2(0.0f, ImGui::GetFrameHeightWithSpacing());

				ImGui::SetNextWindowViewport(viewport->ID);
				ImGui::SetNextWindowPos(viewport->WorkPos + offset, ImGuiCond_Always, ImVec2(0.0f, 0.0f));
				ImGui::SetNextWindowSize(viewport->WorkSize - offset, ImGuiCond_Always);
				if (ImGui::Begin("##ProfileJobs", nullptr, flags))
					job_profiler.RenderTimeline(timeline_scale_factor);

				ImGui::End();
			}
		}

		void RenderPopoutButtons()
		{
		#if !defined(CONSOLE)
			auto viewport = ImGui::GetMainViewport();
			if (!viewport)
				return;

			const auto config_flags = ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking;

			ImGui::SetNextWindowPos(viewport->WorkPos + ImVec2(0, 0), ImGuiCond_Always, ImVec2(0.0f, 0.0f));
			if (ImGui::Begin("##ProfilePageConfig", nullptr, config_flags))
			{
				auto PushStyleColorOpaque = [](ImGuiCol col) 
				{
					const auto v = ImGui::GetStyle().Colors[col];
					ImGui::PushStyleColor(col, ImVec4(v.x, v.y, v.z, 1.0f));
				};

				PushStyleColorOpaque(ImGuiCol_Button);
				PushStyleColorOpaque(ImGuiCol_ButtonActive);
				PushStyleColorOpaque(ImGuiCol_ButtonHovered);
				PushStyleColorOpaque(ImGuiCol_FrameBg);
				PushStyleColorOpaque(ImGuiCol_FrameBgActive);
				PushStyleColorOpaque(ImGuiCol_FrameBgHovered);
				PushStyleColorOpaque(ImGuiCol_CheckMark);

				bool first_button = true;

				if (timeline_visible && !timeline_popped_out)
				{
					if (ImGui::Button(IMGUI_ICON_OUTDENT "##PopOutTimelineButton"))
						timeline_popped_out = true;

					if (ImGui::IsItemHovered())
						ImGui::SetTooltip("Open the job timeline in a separate window");

					first_button = false;
				}

				if ((current_page > 0 && pages[current_page - 1]->IsVisible() && !pages[current_page - 1]->IsPoppedOut()) || (current_page == 0 && summary_visible && !summary_popped_out))
				{
					if (!first_button)
						ImGui::SameLine();

					if (ImGui::Button(IMGUI_ICON_SHARE "##PopOutPageButton"))
					{
						if (current_page > 0)
							pages[current_page - 1]->SetPoppedOut(true);
						else
							summary_popped_out = true;
					}

					if (ImGui::IsItemHovered())
						ImGui::SetTooltip("Open this profile page in a separate window");

					first_button = false;
				}

				if (!first_button)
					ImGui::SameLine();

				ImGui::Checkbox("##ClickThroughPages", &pages_clickthrough);
				if (ImGui::IsItemHovered())
				{
					ImGui::BeginTooltip();
					ImGui::PushTextWrapPos(35 * ImGui::GetFontSize());
					ImGui::TextWrapped("If active, popped out profile pages can be clicked through. This always affects all popped out pages at once. "
					"Due to technical limitations, this does not affect pages that are not inside the game window.");
					ImGui::PopTextWrapPos();
					ImGui::EndTooltip();
				}

				ImGui::PopStyleColor(7);
			}

			ImGui::End();
		#endif
		}

		void RenderSummaryWindow()
		{
			char buffer[256] = { '\0' };
			char buffer2[256] = { '\0' };
			const auto& memory_stats = Memory::GetStats();
			const auto video_size = Engine::PluginManager::Get().GetDevice()->GetSize();
			const auto draw_count = Renderer::System::Get().GetDevice()->GetPreviousCounters().draw_count.load(std::memory_order_relaxed);
			const auto blend_mode_draw_count = Renderer::System::Get().GetDevice()->GetPreviousCounters().blend_mode_draw_count.load(std::memory_order_relaxed);
			const auto prim_count = Renderer::System::Get().GetDevice()->GetPreviousCounters().primitive_count.load(std::memory_order_relaxed);
			const auto status = Engine::Status::Get().GetLastSample();

			uint64_t total_used_count = 0;
			uint64_t total_frame_count = 0;
			for (unsigned i = 0; i < (unsigned) Memory::Tag::Count; ++i)
			{
				total_used_count += memory_stats.tag_used_counts[i].load(std::memory_order_relaxed);
				total_frame_count += memory_stats.frame_tag_counts[i].load(std::memory_order_relaxed);
			}

			const auto simd_type = magic_enum::enum_name(simd::cpuid::current());

			ImGui::Text("Draw Calls = %u / %u", unsigned(blend_mode_draw_count), unsigned(draw_count));
			ImGui::Text("Primitives = %u", unsigned(prim_count));
			ImGui::Text("Allocations = %u (%u)", unsigned(total_used_count), unsigned(total_frame_count));
			ImGui::Text("Main Memory = %s", ImGuiEx::FormatMemory(buffer, memory_stats.used_size));
			ImGui::Text("Video Memory = %s", ImGuiEx::FormatMemory(buffer, video_size));
			ImGui::Text("Latency = %.1f ms", status.Latency);
			ImGui::Text("simd = %.*s", unsigned(simd_type.length()), simd_type.data());
			if (Engine::NetworkStatus::HasBandwidth())
			{
				ImGui::PushStyleColor(ImGuiCol_Text, status.Bandwidth < 20 * Memory::KB ? ImVec4(0.0f, 1.0f, 0.0f, 1.0f) : status.Bandwidth < 100 * Memory::KB ? ImVec4(1.0f, 1.0f, 0.0f, 1.0f) : ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
				ImGui::Text("Bandwidth = %s/s (Peak: %s/s) %s", ImGuiEx::FormatMemory(buffer, uint64_t(status.Bandwidth)), ImGuiEx::FormatMemory(buffer2, uint64_t(Engine::NetworkStatus::GetPeakBandwidth())), Engine::NetworkStatus::IsDownloading() ? IMGUI_ICON_IMPORT : "");
				ImGui::PopStyleColor();
			}
		}

		void RenderSummary()
		{
			if (summary_popped_out)
			{
				if (popout_visible)
				{
					const auto flags = pages_clickthrough && summary_main_viewport ? ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoFocusOnAppearing : ImGuiWindowFlags_None;

					summary_main_viewport = false;
					if (ImGui::Begin("Summary##ProfilePlugin", &summary_popped_out, flags))
					{
						summary_main_viewport = ImGui::GetCurrentWindow()->Viewport == ImGui::GetMainViewport();
						RenderSummaryWindow();
					}

					ImGui::End();
				}
			}
			else
			{
				summary_main_viewport = false;
				auto viewport = ImGui::GetMainViewport();
				if (!viewport)
					return;

				const auto flags = ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking;

				ImGui::SetNextWindowViewport(viewport->ID);
				ImGui::SetNextWindowSize(ImVec2(300.0f, 0.0f));
				ImGui::SetNextWindowPos(viewport->WorkPos + ImVec2(viewport->WorkSize.x, ImGui::GetFrameHeightWithSpacing()), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
				if (ImGui::Begin("##ProfileSummary", nullptr, flags))
					RenderSummaryWindow();

				ImGui::End();
			}
		}

	public:
		Impl(Plugin* parent) : parent(parent)
		{
			parent->AddAction("Show Summary", VK_F1, Input::Modifier_Ctrl, [this]()
			{
				const bool show = !summary_visible;
				HideAll();
				current_page = 0;
				summary_visible = show;
				timeline_visible = show || timeline_popped_out;
				SetPopoutVisible(true);
			});

			pages.push_back(&job_profiler);
			toggle_enum_ids[size_t(ProfilePlugin::PageType::Jobs)] = pages.size();
			parent->AddAction("Show Jobs", VK_F2, Input::Modifier_Ctrl, [this, id = pages.size()]()
			{
				const bool show = job_profiler.IsPoppedOut() || !job_profiler.IsVisible();
				HideAll();
				current_page = id;
				job_profiler.SetPageVisible(show);
				timeline_visible = (job_profiler.IsPoppedOut() ? !timeline_visible : show) || timeline_popped_out;
				SetPopoutVisible(true);
			});

			AddPage<ProfileSystemPlugin>(VK_F3);
			toggle_enum_ids[size_t(ProfilePlugin::PageType::System)] = pages.size();

			AddPage<ProfileAllocatorPlugin>(VK_F4);
			toggle_enum_ids[size_t(ProfilePlugin::PageType::Memory)] = pages.size();

			AddPage<ProfileDrawCallsPlugin>(VK_F5);
			toggle_enum_ids[ size_t( ProfilePlugin::PageType::DrawCalls ) ] = pages.size();

			AddPage<ProfileDevicePlugin>(VK_F6);
			toggle_enum_ids[ size_t( ProfilePlugin::PageType::Device ) ] = pages.size();

			AddPage<ProfileFileAccessPlugin>(VK_F7);
			toggle_enum_ids[ size_t( ProfilePlugin::PageType::FileAccess ) ] = pages.size();

			AddPage<ProfileResourcePlugin>(VK_F8);
			toggle_enum_ids[ size_t( ProfilePlugin::PageType::Resource ) ] = pages.size();

			parent->AddAction("Toggle current page", VK_OEM_3, Input::Modifier_Ctrl, [this]()
			{
				bool is_visible = false;
				if (current_page > 0)
					is_visible = !pages[current_page - 1]->IsPoppedOut() && pages[current_page - 1]->IsVisible();
				else
					is_visible = summary_visible;

				if (popout_visible)
				{
					is_visible = is_visible || timeline_popped_out;
					for (const auto& page : pages)
						is_visible = is_visible || page->IsPoppedOut();
				}

				if (is_visible)
				{
					HideAll();
					timeline_visible = false;
					SetPopoutVisible(false);
				}
				else
				{
					SetPopoutVisible(true);
					timeline_visible = true;
					if (current_page > 0)
						pages[current_page - 1]->SetPageVisible(true);
					else
						summary_visible = true;
				}
			});
		}

		void CyclePages()
		{
			static PageType next_page = PageType::Count;
			next_page = ((size_t)next_page < (size_t)PageType::Count) ?
				static_cast< PageType >((size_t)next_page + 1) :
				static_cast< PageType >((size_t)0);
			TogglePage( next_page );
		}

		void TogglePage(ProfilePlugin::PageType type)
		{
			if (size_t(type) < std::size(toggle_enum_ids))
				TogglePage(toggle_enum_ids[size_t(type)]);
		}

		void OnRender()
		{
			current_page = std::min(current_page, pages.size() - 1); // Avoid error when removing pages, but saved current_page is greater.

			const bool page_visible = current_page > 0 && pages[current_page - 1]->IsVisible() && !pages[current_page - 1]->IsPoppedOut();
			const bool is_visible = current_page == 0 ? summary_visible && !summary_popped_out : page_visible;
			bool any_visible = timeline_visible || (current_page == 0 && summary_visible) || (popout_visible && (summary_popped_out || timeline_popped_out));

			for (const auto& a : pages)
				if (a->IsVisible() && (!a->IsPoppedOut() || popout_visible))
					any_visible = true;

			if (timeline_visible || timeline_popped_out || is_visible)
			{
				timeline_visible = true;
				RenderTimeline();
			}

			if (any_visible)
				RenderPopoutButtons();

			if (summary_visible || summary_popped_out)
				RenderSummary();

			job_profiler.RenderJobs();
		}

		void OnRenderSettings()
		{
			char buffer[256] = { '\0' };

			ImGui::MenuItem("Click Through", nullptr, &pages_clickthrough);
			ImGui::SliderFloat("##TimelineScale", &timeline_scale_factor, 0.1f, 10.0f, "Timeline Scale: %.2f", ImGuiSliderFlags_AlwaysClamp);
			ImGui::Separator();
			ImGui::MenuItem("Timeline popped out", nullptr, &timeline_popped_out);
			ImGui::MenuItem("Summary popped out", nullptr, &summary_popped_out);
			for (size_t a = 0; a < pages.size(); a++)
			{
				ImGui::PushID(int(a));
				ImFormatString(buffer, std::size(buffer), "%s popped out", pages[a]->GetName().c_str());
				bool pop_out = pages[a]->IsPoppedOut();
				if (ImGui::MenuItem(buffer, nullptr, &pop_out))
					pages[a]->SetPoppedOut(pop_out);

				ImGui::PopID();
			}
		}

		void OnEnabled()
		{
			job_profiler.SetEnabled(true);
			for (const auto& a : pages)
				a->SetEnabled(true);
		}

		void OnDisabled()
		{
			job_profiler.SetEnabled(false);
			for (const auto& a : pages)
				a->SetEnabled(false);
		}

		void OnLoadSettings(const rapidjson::Value& plugin_settings)
		{
			using namespace rapidjson;

			unsigned version = 1;
			if (const auto f = plugin_settings.FindMember("version"); f != plugin_settings.MemberEnd() && f->value.IsUint())
				version = f->value.GetUint();

			if (const auto f = plugin_settings.FindMember("current"); f != plugin_settings.MemberEnd() && f->value.IsUint())
				current_page = f->value.GetUint();

			if (auto f = plugin_settings.FindMember("timeline_scale"); f != plugin_settings.MemberEnd() && f->value.IsFloat())
				timeline_scale_factor = f->value.GetFloat();

			if (auto f = plugin_settings.FindMember("cpu_scale"); f != plugin_settings.MemberEnd() && f->value.IsFloat())
				job_profiler.CPUScale = f->value.GetFloat();

			if (auto f = plugin_settings.FindMember("gpu_scale"); f != plugin_settings.MemberEnd() && f->value.IsFloat())
				job_profiler.GPUScale = f->value.GetFloat();

			if (const auto f = plugin_settings.FindMember("popped_out"); f != plugin_settings.MemberEnd() && f->value.IsArray())
			{
				for (size_t a = 0; a < f->value.Size() && a <= pages.size(); a++)
				{
					const auto& state = f->value[SizeType(a)];
					if (!state.IsBool())
						continue;

					if (a == 0)
						timeline_popped_out = state.GetBool();
					else if (version == 1)
						pages[a - 1]->SetPoppedOut(state.GetBool());
					else if (a == 1)
						summary_popped_out = state.GetBool();
					else
						pages[a - 2]->SetPoppedOut(state.GetBool());
				}
			}

			if (const auto f = plugin_settings.FindMember("click_through"); f != plugin_settings.MemberEnd() && f->value.IsBool())
				pages_clickthrough = f->value.GetBool();
		}

		void OnSaveSettings(rapidjson::Value& plugin_settings, rapidjson::Document::AllocatorType& allocator)
		{
			using namespace rapidjson;

			JsonUtility::Write(plugin_settings, unsigned(current_page), "current", allocator);
			JsonUtility::Write(plugin_settings, unsigned(SettingsVersion), "version", allocator);

			Value page_settings(kArrayType);
			page_settings.PushBack(timeline_popped_out, allocator);
			page_settings.PushBack(summary_popped_out, allocator);
			for (const auto& page : pages)
				page_settings.PushBack(page->IsPoppedOut(), allocator);

			plugin_settings.AddMember("popped_out", page_settings, allocator);
			plugin_settings.AddMember("click_through", pages_clickthrough, allocator);
			plugin_settings.AddMember("timeline_scale", timeline_scale_factor, allocator);
			plugin_settings.AddMember("cpu_scale", job_profiler.CPUScale, allocator);
			plugin_settings.AddMember("gpu_scale", job_profiler.GPUScale, allocator);
		}
	};

	ProfilePlugin::ProfilePlugin() : Plugin(Feature_Settings)
	{
		impl = new Impl(this);
	}

	ProfilePlugin::~ProfilePlugin()
	{
		delete impl;
	}

	void ProfilePlugin::CyclePages(){ impl->CyclePages(); }
	void ProfilePlugin::TogglePage(PageType type) { impl->TogglePage(type); }
	void ProfilePlugin::OnRender(float elapsed_time) { impl->OnRender(); }
	void ProfilePlugin::OnRenderSettings(float elapsed_time) { impl->OnRenderSettings(); }
	void ProfilePlugin::OnEnabled() { impl->OnEnabled(); }
	void ProfilePlugin::OnDisabled() { impl->OnDisabled(); }
	void ProfilePlugin::OnLoadSettings(const rapidjson::Value& settings) { impl->OnLoadSettings(settings); }
	void ProfilePlugin::OnSaveSettings(rapidjson::Value& settings, rapidjson::Document::AllocatorType& allocator) { impl->OnSaveSettings(settings, allocator); }

	void ProfilePlugin::PushGPU(Job2::Profile hash, uint64_t begin, uint64_t end)
	{
		if (auto profiler = JobProfiler::Get(); profiler && profiler->IsEnabled())
			profiler->PushGPU(hash, begin, end);
	}

	void ProfilePlugin::PushServer(Job2::Profile hash, uint64_t begin, uint64_t end)
	{
		if (auto profiler = JobProfiler::Get(); profiler && profiler->IsEnabled())
			profiler->PushServer(hash, begin, end);
	}

	void ProfilePlugin::QueueBegin(Job2::Profile hash)
	{
		ProfileSave(0, hash);
	}

	void ProfilePlugin::QueueEnd()
	{
		ProfileRestore(0, (Job2::Profile)0);
	}

	void ProfilePlugin::Swap()
	{
		if (auto profiler = JobProfiler::Get(); profiler && profiler->IsEnabled())
			profiler->Swap();
	}

	void ProfilePlugin::Hook()
	{
		Job2::System::Get().SetCaptures(ProfileSave, ProfileRestore);
	}

	void ProfilePlugin::Unhook()
	{
		Job2::System::Get().SetCaptures(nullptr, nullptr);
	}

	ProfilePlugin::Page::Page(const std::string& title) : Plugin(Feature_Visible | Feature_NoMenu), title(title) {}

	bool ProfilePlugin::Page::Begin(float width)
	{
		char label[256] = { '\0' };
		ImFormatString(label, std::size(label), "%s##ProfilePlugin", title.c_str());

		if (popped_out)
		{
			if (!popout_visible)
				return false;

			window_flags = pages_clickthrough && in_main_viewport ? ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoScrollbar : ImGuiWindowFlags_None;

			bool vis = true;
			const bool res = ImGui::Begin(label, &vis, window_flags);
			in_main_viewport = false;

			if (!vis)
			{
				popped_out = false;
				SetVisible(page_visible);
				End();

				return false;
			}
			else if (!res)
				End();

			in_main_viewport = ImGui::GetCurrentWindow()->Viewport == ImGui::GetMainViewport();
			return res;
		}
		else
		{
			in_main_viewport = false;
			if (!IsVisible())
				return false;

			auto viewport = ImGui::GetMainViewport();
			if (!viewport)
				return false;

			window_flags = ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking;

			ImGui::SetNextWindowViewport(viewport->ID);
			ImGui::SetNextWindowPos(viewport->WorkPos + ImVec2(viewport->WorkSize.x / 2, 0), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
			if (ImGui::Begin("##ProfilePageTitle", nullptr, window_flags))
				ImGui::Text("< %s >", title.c_str());

			ImGui::End();

			ImGui::SetNextWindowViewport(viewport->ID);
			ImGui::SetNextWindowSize(ImVec2(width, 0.0f));
			ImGui::SetNextWindowPos(viewport->WorkPos + ImVec2(viewport->WorkSize.x, ImGui::GetFrameHeightWithSpacing()), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
			const bool res = ImGui::Begin("##ProfilePageContent", nullptr, window_flags);
			if (!res)
				End();

			return res;
		}
	}

	void ProfilePlugin::Page::End()
	{
		ImGui::End();
	}

	bool ProfilePlugin::Page::BeginSection(const char* str_id, const char* label, bool default_open, int flags)
	{
		if (popped_out)
		{
			if (default_open)
				flags |= ImGuiTreeNodeFlags_DefaultOpen;

			return ImGui::TreeNodeEx(str_id, flags, "%s", label);
		}
		else
		{
			if (flags & ImGuiTreeNodeFlags_Bullet)
				ImGui::BulletText("%s", label);
			else
				ImGui::Text("%s", label);

			if (default_open)
				ImGui::TreePushOverrideID(ImGui::GetCurrentWindow()->GetID(str_id));

			return default_open;
		}
	}

	void ProfilePlugin::Page::EndSection()
	{
		ImGui::TreePop();
	}

	void ProfilePlugin::Page::SetPageVisible(bool visible)
	{
		page_visible = visible;
		if (!popped_out || visible)
			SetVisible(visible);
	}

#endif
}
