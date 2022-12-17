#include "Common/Job/JobSystem.h"
#include "Common/File/StorageSystem.h"
#include "Common/Engine/NetworkStatus.h"

#include "Visual/GUI2/imgui/imgui.h"
#include "Visual/GUI2/imgui/imgui_ex.h"
#include "Visual/GUI2/imgui/implot.h"
#include "Visual/GUI2/Icons.h"
#include "Visual/Renderer/DrawCalls/DrawCall.h"
#include "Visual/Renderer/ShaderSystem.h"
#include "Visual/Renderer/RendererSubsystem.h"
#include "Visual/Renderer/EffectGraphSystem.h"
#include "Visual/Renderer/Scene/SceneSystem.h"
#include "Visual/Animation/AnimationSystem.h"
#include "Visual/Texture/TextureSystem.h"
#include "Visual/Physics/PhysicsSystem.h"
#include "Visual/Audio/SoundSystem.h"
#include "Visual/Entity/EntitySystem.h"
#include "Visual/Device/Device.h"
#include "Visual/Device/RenderTarget.h"
#include "Visual/Device/IndexBuffer.h"
#include "Visual/Particles/EffectInstance.h"
#include "Visual/Trails/TrailsEffectInstance.h"
#include "Visual/Trails/TrailSystem.h"
#include "Visual/Particles/ParticleSystem.h"
#include "Visual/Effects/EffectsManager.h"
#include "Visual/Renderer/RendererSubsystem.h"
#include "Visual/Engine/EngineSystem.h"
#include "Visual/LUT/LUTSystem.h"
#include "Visual/GpuParticles/GpuParticleSystem.h"
#include "Visual/Device/DynamicResolution.h"

#include "ProfileSystemsPlugin.h"

namespace Engine::Plugins
{
#if DEBUG_GUI_ENABLED && defined(PROFILING)

	static const unsigned SampleCount = 1024;

	class Plotter
	{
		Memory::FlatMap<std::string, std::array<float, SampleCount>, Memory::Tag::Profile> plots;

		std::array<float, SampleCount> axis = {};

		unsigned offset = 0;

	public:
		Plotter()
		{
			for (unsigned i = 0; i < (unsigned)axis.size(); ++i)
				axis[i] = (float)i;
		}

		void Swap()
		{
			offset = (offset + 1) % (unsigned)axis.size();
		}

		void Remove(const std::string_view name)
		{
			plots.erase(name.data());
		}

		void Update(const std::string_view name, float value)
		{
			plots[name.data()][offset] = value;
		}

		void Draw()
		{
			if (ImPlot::BeginPlot(""))
			{
				ImPlot::SetupAxesLimits(0.0f, (float)axis.size(), 0.0f, 1.0f);
				for (const auto& plot : plots)
				{
					ImPlot::PlotLine(plot.first.c_str(), axis.data(), plot.second.data(), (int)axis.size(), ImPlotLineFlags_None);
				}
				ImPlot::PlotInfLines("##Cursor", &axis[offset], 1);
				ImPlot::EndPlot();
			}
		}
	};

	Plotter plotter;

#define PLOT_DELTA(NAME, VALUE, MAX) \
	if (IsPoppedOut()) \
	{ \
		static size_t old = 0; \
		int64_t delta = (int64_t)VALUE - (int64_t)old; \
		old = VALUE; \
		static bool plot = false; \
		if (ImGui::Checkbox("##" #NAME, &plot)) { if (!plot) { plotter.Remove(NAME); } } \
		if (plot) { plotter.Update(NAME " Delta", 0.5f + 0.5f * std::clamp((float)delta, -MAX, MAX) / MAX); } \
		ImGui::SameLine(); \
	}

#define PLOT_TEXT(NAME, FORMAT, VALUE, MAX) \
	if (IsPoppedOut()) \
	{ \
		static bool plot = false; \
		if (ImGui::Checkbox("##" #NAME, &plot)) { if (!plot) { plotter.Remove(NAME); } } \
		if (plot) { plotter.Update(NAME, std::clamp((float)VALUE, 0.0f, MAX) / MAX); } \
		ImGui::SameLine(); \
		ImGui::Text(FORMAT, VALUE); \
	}

#define PLOT(NAME, VALUE, MAX) \
	if (IsPoppedOut()) \
	{ \
		static bool plot = false; \
		if (ImGui::Checkbox("##" #NAME, &plot)) { if (!plot) { plotter.Remove(NAME); } } \
		if (plot) { plotter.Update(NAME, std::clamp((float)VALUE, 0.0f, MAX) / MAX); } \
		ImGui::SameLine(); \
	}


	void ProfileSystemPlugin::OnRender(float elapsed_time)
	{
		plotter.Swap();

		if (Begin(1400.0f))
		{
			if (ImGui::IsWindowAppearing())
				current_time = 0.0f;
			else
				current_time += elapsed_time;

			char buffer[256] = { '\0' };
			char buffer2[256] = { '\0' };
			char buffer3[256] = { '\0' };

			const auto& memory_stats = Memory::GetStats();

			bool collapse_all = false;

			const auto begin_system = [&](const char* name)
			{
				if (!IsPoppedOut())
					ImGui::NewLine();

				ImFormatString(buffer, std::size(buffer), "%s", name);

				if (IsPoppedOut() && collapse_all)
					ImGui::SetNextItemOpen(false, ImGuiCond_Always);

				return BeginSection(name, buffer, true);
			};

			if (IsPoppedOut() || ImGui::BeginTable("##TableWrapper", 4, ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoPadOuterX))
			{
				if (!IsPoppedOut())
				{
					ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);
					ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);
					ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);
					ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);
					ImGui::TableNextRow();
				}

				if (IsPoppedOut() || ImGui::TableSetColumnIndex(0))
				{
					if (IsPoppedOut() && ImGui::Button("Collapse All"))
						collapse_all = true;

					if (IsPoppedOut() && collapse_all)
						ImGui::SetNextItemOpen(false, ImGuiCond_Always);

					if (IsPoppedOut() && BeginSection("Plots"))
					{
						plotter.Draw();

						EndSection();
					}

					if (IsPoppedOut() && collapse_all)
						ImGui::SetNextItemOpen(false, ImGuiCond_Always);

					if (BeginSection("Engine"))
					{
						const auto stats = Engine::System::Get().GetStats();

						if (BeginSection("Times"))
						{
							if (IsPoppedOut())
							{
								if (auto* window = Device::WindowDX::GetWindow())
								{
									const auto window_stats = window->GetStats();

									const auto round = [](float t) { return (uint64_t)((double)t * 1'000'000); };

									PLOT_TEXT("CPU Frame Time", "CPU Frame: %lu us", round(window_stats.cpu_frame_time), 30'000.0f);
									PLOT_TEXT("GPU Frame Time", "GPU Frame: %lu us", round(window_stats.gpu_frame_time), 30'000.0f);
									PLOT_TEXT("Device Wait Time", "Device Wait: %lu us", round(window_stats.device_wait_time), 30'000.0f);
									PLOT_TEXT("Present Wait Time", "Present Wait: %lu us", round(window_stats.present_wait_time), 30'000.0f);
									PLOT_TEXT("Limit Wait Time", "Limit Wait: %lu us", round(window_stats.limit_wait_time), 30'000.0f);
								}
							}

							EndSection();
						}

						if (BeginSection("VRAM"))
						{
							auto* device = Renderer::Scene::System::Get().GetDevice();
							const auto vram = device ? device->GetVRAM() : Device::VRAM();
							ImGui::Text("Sizes: %s / %s / %s",
								Memory::ReadableSize(vram.used).c_str(),
								Memory::ReadableSize(vram.reserved).c_str(),
								Memory::ReadableSize(vram.available).c_str());
							ImGui::Text("Allocations: %u / %u", vram.allocation_count, vram.block_count);

							if (IsPoppedOut())
							{
								static float vram_modifier = 1.0f;
								ImGui::SliderFloat("Modifier", &vram_modifier, 0.0f, 1.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
								Engine::System::Get().SetVRAMBudgetModifier(vram_modifier);

								ImGui::Text("Pressure: %s", std::to_string(stats.vram_pressure).c_str());
								ImGui::Text("Factor: %s", std::to_string(stats.vram_factor).c_str());
								ImGui::Text("Budget: %s", ImGuiEx::FormatMemory(buffer, stats.vram_budget));
							}

							EndSection();
						}

						if (BeginSection("RAM"))
						{
							ImGui::Text("Sizes: %s / %s",
								Memory::ReadableSize(Memory::GetUsedMemory()).c_str(),
								Memory::ReadableSize(stats.ram_max).c_str());

							if (IsPoppedOut())
							{
								static float ram_modifier = 1.0f;
								ImGui::SliderFloat("Modifier", &ram_modifier, 0.0f, 1.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
								Engine::System::Get().SetRAMBudgetModifier(ram_modifier);

								ImGui::Text("Pressure: %s", std::to_string(stats.ram_pressure).c_str());
								ImGui::Text("Factor: %s", std::to_string(stats.ram_factor).c_str());
								ImGui::Text("Budget: %s", ImGuiEx::FormatMemory(buffer, stats.ram_budget));
							}

							EndSection();
						}

						ImGui::Text("Simd: %s", simd::cpuid::name(simd::cpuid::current()));

						ImGui::Text("Loading active: %s", stats.loading_active ? "YES" : "NO");

						EndSection();
					}

					if (begin_system("Storage"))
					{
						const auto& stats = Storage::System::Get().GetStats();

						if (IsPoppedOut() && BeginSection("Settings", false))
						{
							ImGui::Text("Potato: %s", stats.potato_mode ? "ON" : "OFF");
							EndSection();
						}

						if (BeginSection("Bundle"))
						{
							char buffer0[256] = { '\0' };
							char buffer1[256] = { '\0' };
							static size_t peak_read_speed = 0;
							static size_t peak_uncompress_speed = 0;
							static size_t peak_access_speed = 0;
							peak_read_speed = std::max(stats.bundle.read_speed, peak_read_speed);
							peak_uncompress_speed = std::max(stats.bundle.uncompress_speed, peak_uncompress_speed);
							peak_access_speed = std::max(stats.bundle.access_speed, peak_access_speed);

							if (stats.bundle.count > 0)
							{
								ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
								ImGui::Text(IMGUI_ICON_YES);
								ImGui::PopStyleColor();
								ImGui::SameLine(0.0f, 0.0f);
								ImGui::Text(" Present");

								ImGui::Text("Read Speed = %s/s (Peak: %s/s)", ImGuiEx::FormatMemory(buffer0, stats.bundle.read_speed), ImGuiEx::FormatMemory(buffer1, peak_read_speed));
								ImGui::Text("Uncompress Speed = %s/s (Peak: %s/s)", ImGuiEx::FormatMemory(buffer0, stats.bundle.uncompress_speed), ImGuiEx::FormatMemory(buffer1, peak_uncompress_speed));
								ImGui::Text("Access Speed = %s/s (Peak: %s/s)", ImGuiEx::FormatMemory(buffer0, stats.bundle.access_speed), ImGuiEx::FormatMemory(buffer1, peak_access_speed));

								ImGui::Text("Cache: %s", ImGuiEx::FormatMemory(buffer1, stats.bundle.cache_size));
							}
							else
							{
								ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
								ImGui::Text(IMGUI_ICON_NO);
								ImGui::PopStyleColor();
								ImGui::SameLine(0.0f, 0.0f);
								ImGui::Text(" Absent");
							}

							if (IsPoppedOut() && BeginSection("Assets", false))
							{
								ImGui::Text("Count = %u / %u", unsigned(stats.bundle.count), unsigned(stats.bundle.total_count));
								ImGui::Text("Files = %u", unsigned(stats.bundle.total_files));

								ImGui::Text("Loaded Bundles = %u", unsigned(stats.bundle.io_count));
								ImGui::Text("Total IO Read = %s", ImGuiEx::FormatMemory(buffer0, stats.bundle.io_read));
								EndSection();
							}

							if (IsPoppedOut() && BeginSection("File Cache", false))
							{
								const auto hit_miss_total = stats.bundle.hit + stats.bundle.miss;
								const auto hit_ratio = hit_miss_total > 0 ? 100.0f * (float)stats.bundle.hit / (float)hit_miss_total : 0;
								const auto miss_ratio = hit_miss_total > 0 ? 100.0f * (float)stats.bundle.miss / (float)hit_miss_total : 0;
								const auto disk_ratio = stats.bundle.total > 0 ? 100.0f * (float)stats.bundle.disk / (float)stats.bundle.total : 0;
								const auto wait_ratio = stats.bundle.total > 0 ? 100.0f * (float)stats.bundle.wait / (float)stats.bundle.total : 0;

								ImGui::Text("Hit = %u / %u (%.1f %%)", unsigned(stats.bundle.hit), unsigned(hit_miss_total), hit_ratio);
								ImGui::Text("Miss = %u / %u (%.1f %%)", unsigned(stats.bundle.miss), unsigned(hit_miss_total), miss_ratio);
								ImGui::Text("Disk = %u / %u (%.1f %%)", unsigned(stats.bundle.disk), unsigned(stats.bundle.total), disk_ratio);
								ImGui::Text("Wait = %u / %u (%.1f %%)", unsigned(stats.bundle.wait), unsigned(stats.bundle.total), wait_ratio);
								EndSection();
							}

							if (IsPoppedOut() && BeginSection("History", false))
							{
								float history_bundles[MaxSceneStats + 1];
								float history_reads[MaxSceneStats + 1];
								for (size_t a = 0; a < scene_stats.size(); a++)
								{
									history_bundles[a] = float(scene_stats[a].loaded_bundles);
									history_reads[a] = float(scene_stats[a].loaded_bytes);
								}

								history_bundles[scene_stats.size()] = float(stats.bundle.io_count);
								history_reads[scene_stats.size()] = float(stats.bundle.io_read);

								float peak_bundles = 0;
								float peak_read = 0;
								for (size_t a = 0; a <= scene_stats.size(); a++)
								{
									peak_bundles = std::max(peak_bundles, history_bundles[a]);
									peak_read = std::max(peak_read, history_reads[a]);
								}

								const auto histogram_height = ImGui::GetFrameHeightWithSpacing() * 8;
								const auto histogram_width = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) / 2;

								ImGui::BeginGroup();
								ImGui::AlignTextToFramePadding();
								ImGui::Text("Loaded Bundles (Peak: %.0f)", peak_bundles);
								ImGui::PlotHistogram("##LoadedBundles", history_bundles, int(scene_stats.size() + 1), 0, nullptr, 0, float(uint64_t(3) << 10), ImVec2(histogram_width, histogram_height));
								ImGui::EndGroup();
								ImGui::SameLine();
								ImGui::BeginGroup();
								ImGui::AlignTextToFramePadding();
								ImGui::Text("Loaded Bytes (Peak: %s)", ImGuiEx::FormatMemory(buffer0, uint64_t(peak_read + 0.5f)));
								ImGui::PlotHistogram("##LoadedBytes", history_reads, int(scene_stats.size() + 1), 0, nullptr, 0, float(uint64_t(3) << 30), ImVec2(histogram_width, histogram_height));
								ImGui::EndGroup();

								EndSection();
							}

							if (IsPoppedOut() && BeginSection("Bundle Files", false))
							{ 
								auto infos = Storage::System::Get().GetInfos();
								const auto histogram_height = ImGui::GetFrameHeightWithSpacing() * 10;

								if (IsPoppedOut())
								{
									ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);

									static ImGuiTextFilter bundle_filter;
									bundle_filter.Draw("##BundleSearch");

									constexpr ImGuiTableFlags tableFlags = ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti | ImGuiTableFlags_SortTristate;

									enum ColumnID : ImU32
									{
										ID_Name,
										ID_Size,
										ID_Utilisation,
									};

									const auto table_height = std::max(ImGui::GetFrameHeightWithSpacing() * 5, ImGui::GetContentRegionAvail().y - (histogram_height + ImGui::GetStyle().ItemSpacing.y + ImGui::GetTextLineHeightWithSpacing()));
									if (ImGui::BeginTable("##BundleTable", 3, tableFlags, ImVec2(0.0f, table_height)))
									{
										ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_NoHide, 0.0f, ID_Name);
										ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_None, 0.0f, ID_Size);
										ImGui::TableSetupColumn("Utilisation", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_PreferSortAscending, 0.0f, ID_Utilisation);
										ImGui::TableSetupScrollFreeze(0, 1); // Make row always visible
										ImGui::TableHeadersRow();

										if (bundle_filter.IsActive())
										{
											for (auto a = infos.begin(); a != infos.end();)
											{
												if (bundle_filter.PassFilter(a->name.c_str()))
													++a;
												else
													a = infos.erase(a);
											}
										}

										if (auto sortSpecs = ImGui::TableGetSortSpecs(); sortSpecs && sortSpecs->SpecsCount > 0)
										{
											const auto SortCompare = [](const Storage::Info& a, const Storage::Info& b, ColumnID id) -> int
											{
												switch (id)
												{
													case ID_Name:
														return strcmp(a.name.c_str(), b.name.c_str());
													case ID_Size:
														return a.size == b.size ? 0 : a.size > b.size ? 1 : -1;
													case ID_Utilisation:
														return a.utilisation == b.utilisation ? 0 : a.utilisation > b.utilisation ? 1 : -1;
													default:
														return 0;
												}
											};

											const auto SortFunc = [&](const Storage::Info& a, const Storage::Info& b)
											{
												for (int i = 0; i < sortSpecs->SpecsCount; i++)
												{
													const auto& specs = sortSpecs->Specs[i];
													const auto order = SortCompare(a, b, (ColumnID)specs.ColumnUserID);

													if (order == 0)
														continue;

													return (order < 0) == (specs.SortDirection == ImGuiSortDirection_Ascending);
												}

												return false;
											};

											std::sort(infos.begin(), infos.end(), SortFunc);
										}

										const float row_height = ImGui::GetFontSize() + (2 * ImGui::GetStyle().CellPadding.y);
										ImGuiListClipper clipper;
										clipper.Begin(int(infos.size()), row_height);

										while (clipper.Step())
										{
											for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
											{
												ImGui::PushID(i);
												ImGui::TableNextRow();

												// Name
												{
													ImGui::TableNextColumn();

													ImGuiEx::FormatStringEllipsis(ImGui::GetContentRegionAvail().x, false, buffer0, std::size(buffer0), "%s", infos[i].name.c_str());
													ImGui::Text("%s", buffer0);
												}

												// Size
												{
													ImGui::TableNextColumn();
													ImGui::Text("%s", ImGuiEx::FormatMemory(buffer0, infos[i].size));
												}

												// Utilisation
												{
													ImGui::TableNextColumn();
													ImGui::Text("%.1f %%", 100.0f * infos[i].utilisation);
												}

												ImGui::PopID();
											}
										}

										ImGui::EndTable();
									}
								}
								else
								{
									std::sort(infos.begin(), infos.end(), [](const auto& a, const auto& b)
									{
										const bool sa = a.name.length() > 0 && a.name.c_str()[0] == '_';
										const bool sb = b.name.length() > 0 && b.name.c_str()[0] == '_';
										if (sa != sb)
											return sb;

										return a.utilisation < b.utilisation;
									});

									const size_t max_count = 8;
									for (size_t a = 0; a < max_count && a < infos.size(); a++)
										ImGui::Text("%.1f%% (%s) %s", 100.0f * infos[a].utilisation, ImGuiEx::FormatMemory(buffer0, infos[a].size), infos[a].name.c_str());

									if (infos.size() > max_count)
										ImGui::Text("%u more", unsigned(infos.size() - max_count));

									ImGui::NewLine();
								}

								float histogram[11];
								for (auto& a : histogram)
									a = 0.0f;

								for (const auto& a : infos)
									if (a.name.length() == 0 || a.name.c_str()[0] != '_')
										histogram[int(std::floor(std::clamp(a.utilisation * 10.0f, 0.0f, 10.0f)) + 0.5f)] += 1.0f;

								ImGui::Text("Utilisation %u", unsigned(infos.size()));
								ImGui::PlotHistogram("##UtilisationPlot", histogram, int(std::size(histogram)), 0, nullptr, 0.0f, float(infos.size()), ImVec2(ImGui::GetContentRegionAvail().x, histogram_height));

								EndSection();
							}

							EndSection();
						}

						if (BeginSection("Download"))
						{
							char buffer0[256] = { '\0' };
							char buffer1[256] = { '\0' };
							char buffer2[256] = { '\0' };

							static size_t peak_download_speed = 0;
							static size_t peak_read_speed = 0;
							static size_t peak_write_speed = 0;
							peak_download_speed = std::max(stats.download.download_speed, peak_download_speed);
							peak_read_speed = std::max(stats.download.read_speed, peak_read_speed);
							peak_write_speed = std::max(stats.download.write_speed, peak_write_speed);

							if (stats.download.enabled)
							{
								ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
								ImGui::Text(IMGUI_ICON_YES);
								ImGui::PopStyleColor();
								ImGui::SameLine(0.0f, 0.0f);
								ImGui::Text(" Enabled");

								ImGui::Text("Download Size = %s", ImGuiEx::FormatMemory(buffer0, stats.download.download_size));
								ImGui::Text("Download Speed = %s/s (Peak: %s/s)", ImGuiEx::FormatMemory(buffer0, stats.download.download_speed), ImGuiEx::FormatMemory(buffer1, peak_download_speed));
								ImGui::Text("Read Speed = %s/s (Peak: %s/s)", ImGuiEx::FormatMemory(buffer0, stats.download.read_speed), ImGuiEx::FormatMemory(buffer1, peak_read_speed));
								ImGui::Text("Write Speed = %s/s (Peak: %s/s)", ImGuiEx::FormatMemory(buffer0, stats.download.write_speed), ImGuiEx::FormatMemory(buffer1, peak_write_speed));

								ImGui::Text("Cache: %s", ImGuiEx::FormatMemory(buffer1, stats.download.cache_size));
							}
							else
							{
								ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
								ImGui::Text(IMGUI_ICON_NO);
								ImGui::PopStyleColor();
								ImGui::SameLine(0.0f, 0.0f);
								ImGui::Text(" Disabled");
							}

							EndSection();
						}

						if (BeginSection("Pack"))
						{
							if (stats.pack.pack)
							{
								ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
								ImGui::Text(IMGUI_ICON_YES);
								ImGui::PopStyleColor();
								ImGui::SameLine(0.0f, 0.0f);
								ImGui::Text(" Present");
							}
							else
							{
								ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
								ImGui::Text(IMGUI_ICON_NO);
								ImGui::PopStyleColor();
								ImGui::SameLine(0.0f, 0.0f);
								ImGui::Text(" Absent");
							}

							EndSection();
						}

						if (BeginSection("Disk"))
						{
							char buffer0[256] = { '\0' };
							char buffer1[256] = { '\0' };

							static size_t disk_peak_read_speed = 0;
							disk_peak_read_speed = std::max(stats.disk.read_speed, disk_peak_read_speed);

							PLOT_DELTA("Read Count", stats.disk.read_count, 10.0f);
							ImGui::Text("Read = %u", stats.disk.read_count);
							ImGui::Text("Size = %s", ImGuiEx::FormatMemory(buffer0, stats.disk.read_size));

							ImGui::Text("Read = %s/s (Peak: %s/s)", ImGuiEx::FormatMemory(buffer0, stats.disk.read_speed), ImGuiEx::FormatMemory(buffer1, disk_peak_read_speed));

							EndSection();
						}

						EndSection();
					}
				}

				if (IsPoppedOut() || ImGui::TableSetColumnIndex(1))
				{
					if (begin_system("Job"))
					{
						const auto stats = Job2::System::Get().GetStats();

						if (IsPoppedOut() && BeginSection("Settings", false))
						{
							ImGui::Text("Potato: %s", stats.enable_potato ? "ON" : "OFF");
							ImGui::Text("Unlock: %s", stats.unlock ? "ON" : "OFF");
							ImGui::Text("High Jobs: %s", stats.high_jobs ? "ON" : "OFF");
							EndSection();
						}

						if (IsPoppedOut() && BeginSection("Type", false))
						{
							for (unsigned i = 0; i < (unsigned) Job2::Type::Count; ++i)
								ImGui::Text("%s: %u", Job2::TypeName((Job2::Type) i), unsigned(stats.job_counts[i]));

							EndSection();
						}

						EndSection();
					}

					if (begin_system("Scene"))
					{
						const auto stats = Renderer::Scene::System::Get().GetStats();

						if (IsPoppedOut() && BeginSection("Lights", false))
						{
							ImGui::Text("Total: %u", unsigned(stats.light_count));

							if (IsPoppedOut() && BeginSection("Types", true))
							{
								ImGui::Text("Point: %u (shadow: %u)", unsigned(stats.point_light_count), unsigned(stats.point_light_shadow_count));
								ImGui::Text("Spot: %u (shadow: %u)", unsigned(stats.spot_light_count), unsigned(stats.spot_light_shadow_count));
								ImGui::Text("Dir: %u (shadow: %u)", unsigned(stats.dir_light_count), unsigned(stats.dir_light_shadow_count));
								EndSection();
							}

							if (IsPoppedOut() && BeginSection("Visible", true))
							{
								ImGui::Text("Point: %u", unsigned(stats.visible_point_light_count));
								ImGui::Text("Non-Point: %u (shadow: %u)", unsigned(stats.visible_non_point_light_count), unsigned(stats.visible_non_point_shadow_light_count));
								EndSection();
							}

							EndSection();
						}

						{
							static size_t uniforms_peak_size = 0;
							uniforms_peak_size = std::max(uniforms_peak_size, stats.uniforms_size);
							ImGuiEx::FormatMemory(buffer, stats.uniforms_size);
							ImGuiEx::FormatMemory(buffer2, stats.uniforms_max_size);
							ImGuiEx::FormatMemory(buffer3, uniforms_peak_size);
							const double usage = (double)stats.uniforms_size / (double)stats.uniforms_max_size;
							ImGui::PushStyleColor(ImGuiCol_Text, usage < 0.6 ? ImGui::GetColorU32(ImGuiCol_Text) : ImU32(usage < 0.9 ? ImColor(1.0f, 1.0f, 0.0f) : ImColor(1.0f, 0.0f, 0.0f)));
							ImGui::Text("Uniforms: %s / %s (peak %s)", buffer, buffer2, buffer3);
							ImGui::PopStyleColor();
						}

						ImGui::Text("Visible layers: %02x", unsigned(stats.visible_layers));

						EndSection();
					}

					if (begin_system("Entity"))
					{
						const auto stats = Entity::System::Get().GetStats();

						if (IsPoppedOut() && BeginSection("Settings", false))
						{
							EndSection();
						}

						ImGui::Text("Entity: %u", unsigned(stats.entity_count));
						ImGui::Text("Template: %u", unsigned(stats.template_count));

						{
							static size_t command_peak_size = 0;
							command_peak_size = std::max(command_peak_size, stats.command_used_size);
							ImGuiEx::FormatMemory(buffer, stats.command_used_size);
							ImGuiEx::FormatMemory(buffer2, stats.command_reserved_size);
							ImGuiEx::FormatMemory(buffer3, command_peak_size);
							ImGui::Text("Commands: %s / %s (peak %s)", buffer, buffer2, buffer3);
						}

						{
							static size_t temp_peak_size = 0;
							temp_peak_size = std::max(temp_peak_size, stats.temp_used_size);
							ImGuiEx::FormatMemory(buffer, stats.temp_used_size);
							ImGuiEx::FormatMemory(buffer2, stats.temp_reserved_size);
							ImGuiEx::FormatMemory(buffer3, temp_peak_size);
							ImGui::Text("Temps: %s / %s (peak %s)", buffer, buffer2, buffer3);
						}

						if (IsPoppedOut() && BeginSection("Components", false))
						{
							ImGui::Text("Cull: %u", unsigned(stats.cull_count));
							ImGui::Text("Render: %u", unsigned(stats.render_count));
							ImGui::Text("Particle: %u", unsigned(stats.particle_count));
							ImGui::Text("Trail: %u", unsigned(stats.trail_count));
							EndSection();
						}

						if (IsPoppedOut() && BeginSection("Render List", false))
						{
							ImGui::Text("Batch Count:  %u", unsigned(stats.batch_count));
							ImGui::Text("SubBatch Count:  %u", unsigned(stats.sub_batch_count));
							ImGui::Text("DrawCall Count:  %u", unsigned(stats.draw_call_count));
							ImGui::Text("Batch Size:  %.1f / %.1f / %.1f", stats.batch_size_min, stats.batch_size_avg, stats.batch_size_max);
							ImGui::PlotHistogram("", stats.batch_size_histogram.data(), (int)stats.batch_size_histogram.size(), 0, nullptr, FLT_MAX, 100.0f, ImVec2(), sizeof(float));
							ImGui::Text("SubBatch Size:  %.1f / %.1f / %.1f", stats.sub_batch_size_min, stats.sub_batch_size_avg, stats.sub_batch_size_max);
							ImGui::PlotHistogram("", stats.sub_batch_size_histogram.data(), (int)stats.sub_batch_size_histogram.size(), 0, nullptr, FLT_MAX, 100.0f, ImVec2(), sizeof(float));
							EndSection();
						}

						if (IsPoppedOut() && BeginSection("Orphaned Effects", false))
						{
							auto& orphaned_effects_manager = Renderer::Scene::System::Get().GetOrphanedEffectsManager();
							ImGui::Text("Particles: %u (%u)", unsigned(Particles::EffectInstance::GetCount()), unsigned(orphaned_effects_manager.GetParticleCount()));
							ImGui::Text("Trails: %u (%u)", unsigned(Trails::TrailsEffectInstance::GetCount()), unsigned(orphaned_effects_manager.GetTrailCount()));
							EndSection();
						}

						EndSection();
					}

					if (begin_system("Audio"))
					{
						auto stats = Audio::System::Get().GetStats();
						char buffer[128] = { '\0' };

						if (IsPoppedOut() && BeginSection("Settings", false))
						{
							ImGui::Text("Buffer Size: %s", ImGuiEx::FormatMemory(buffer, stats.buffer_size));
							ImGui::Text("Buffer Size Threshold: %d%%", int(stats.buffer_size_threshold * 100.f));
							ImGui::Text("LRU Banks max size: %u", stats.lru_bank_max_size);
							ImGui::Text("Channels count: %u", unsigned(stats.channel_count));
							ImGui::Text("Max sounds count: %u", stats.max_num_sources);
							EndSection();
						}

						if (BeginSection("Memory Stats"))
						{
							ImGui::Text("Memory Current: %s %d%%", ImGuiEx::FormatMemory(buffer, stats.memory_usage), int((float)stats.memory_usage / (float)stats.buffer_size * 100.f));
							ImGui::Text("Memory Peak: %s %d%%", ImGuiEx::FormatMemory(buffer, stats.memory_peak), int((float)stats.memory_peak / (float)stats.buffer_size * 100.f));
							EndSection();
						}

						if (BeginSection("Performance Stats"))
						{
							ImGui::Text("Banks Churn Rate: %d%%", int(stats.banks_churn_rate * 100.f));
							EndSection();
						}

						if (IsPoppedOut() && BeginSection("System Objects", false))
						{
							ImGui::Text("Instances: %u", unsigned(stats.instances_count));
							ImGui::Text("Sounds: %u", unsigned(stats.sounds_count));
							ImGui::Text("Banks: %u", unsigned(stats.banks_count));
							ImGui::Text("Play Commands: %u", unsigned(stats.play_command_count));
							ImGui::Text("Move Commands: %u", unsigned(stats.move_command_count));
							ImGui::Text("Parameters Commands: %u", unsigned(stats.parameters_command_count));
							ImGui::Text("Pseudovariables Commands: %u", unsigned(stats.pseudovariables_command_count));
							ImGui::Text("Global Parameters Commands: %u", unsigned(stats.global_params_command_count));
							ImGui::Text("Trigger Cues Commands: %u", unsigned(stats.trigger_cues_command_count));
							ImGui::Text("Stop Commands: %u", unsigned(stats.stop_command_count));
							EndSection();
						}

						EndSection();
					}

					if (begin_system("Animation"))
					{
						EndSection();
					}

					if (begin_system("Physics"))
					{
						EndSection();
					}
				}

				if (IsPoppedOut() || ImGui::TableSetColumnIndex(2))
				{
					if (begin_system("Trails"))
					{
						const auto stats = Trails::System::Get().GetStats();

						ImGui::Text("Merged Draw Calls: %u", stats.num_merged_trails);
						
						ImGuiEx::FormatMemory(buffer, stats.vertex_buffer_used);
						ImGuiEx::FormatMemory(buffer2, stats.vertex_buffer_size);
						ImGuiEx::FormatMemory(buffer3, stats.vertex_buffer_desired);
						ImGui::Text("Vertex Buffer: %s / %s (%s)", buffer, buffer2, buffer3);
						ImGui::Text("Segments: %u", stats.num_segments);
						ImGui::Text("Vertices: %u", stats.num_vertices);

						EndSection();
					}

					if (begin_system("Particles"))
					{
						const auto stats = Particles::System::Get().GetStats();

						ImGui::Text("Merged Draw Calls: %u", stats.num_merged_particles);

						ImGuiEx::FormatMemory(buffer, stats.vertex_buffer_used);
						ImGuiEx::FormatMemory(buffer2, stats.vertex_buffer_size);
						ImGuiEx::FormatMemory(buffer3, stats.vertex_buffer_desired);
						ImGui::Text("Vertex Buffer: %s / %s (%s)", buffer, buffer2, buffer3);
						ImGui::Text("Particles: %u", stats.num_particles);

						EndSection();
					}

					if (begin_system("GPU Particles"))
					{
						const auto stats = GpuParticles::System::Get().GetStats();

						ImGui::Text("Particles: %u / %u (Active: %u)", unsigned(stats.num_particles), unsigned(stats.max_particles), unsigned(stats.num_used_slots));
						ImGui::Text("Allocated: %u (Overhead: %.1f%% | Free: %.1f%%)", unsigned(stats.num_allocated_slots), stats.num_used_slots > 0 ? 100.0f * (float(stats.num_allocated_slots - stats.num_used_slots) / float(stats.num_used_slots)) : 0.0f, 100.0f * (float(stats.max_particles - stats.num_allocated_slots) / float(stats.max_particles)));
						ImGui::Text("Emitters: %u (Visible: %u)", unsigned(stats.num_emitters), unsigned(stats.num_visible_emitters));
						ImGui::Text("Bones: %u / %u", unsigned(stats.num_bones), unsigned(stats.max_bones));

						EndSection();
					}

					if (begin_system("Spline LUT"))
					{
						const auto stats = LUT::System::Get().GetStats();

						ImGui::Text("Used slots: %u", unsigned(stats.num_unique));
						ImGui::Text("Num splines: %u", unsigned(stats.num_entries));
						EndSection();
					}
				}

				if (IsPoppedOut() || ImGui::TableSetColumnIndex(3))
				{
					if (begin_system("Render"))
					{
						const auto stats = Renderer::System::Get().GetStats();

						if (IsPoppedOut() && BeginSection("Settings", false))
						{
							ImGui::Text("Device: %s", Device::IDevice::GetTypeName(Device::IDevice::GetType()));
							ImGui::Text("Debug Layer: %s", Device::IDevice::IsDebugLayerEnabled() ? "ON" : "OFF");
							ImGui::Text("Async: %s", stats.enable_async ? "ON" : "OFF");
							ImGui::Text("Budget: %s", stats.enable_budget ? "ON" : "OFF");
							ImGui::Text("Warmup: %s", stats.enable_warmup ? "ON" : "OFF");
							ImGui::Text("Skip: %s", stats.enable_skip ? "ON" : "OFF");
							ImGui::Text("Throttling: %s", stats.enable_throttling ? "ON" : "OFF");
							ImGui::Text("Instancing: %s", stats.enable_instancing ? "ON" : "OFF");
							ImGui::Text("Potato: %s", stats.potato_mode ? "ON" : "OFF");
							EndSection();
						}

						if (IsPoppedOut() && BeginSection("Device Objects", false))
						{
							ImGui::Text("CommandBuffer = %u", unsigned(Device::CommandBuffer::GetCount()));
							ImGui::Text("Pass = %u", unsigned(Device::Pass::GetCount()));
							ImGui::Text("Pipeline = %u", unsigned(Device::Pipeline::GetCount()));
							ImGui::Text("Shader = %u", unsigned(Device::Shader::GetCount()));
							ImGui::Text("Sampler = %u", unsigned(Device::Sampler::GetCount()));
							ImGui::Text("Texture = %u", unsigned(Device::Texture::GetCount()));
							ImGui::Text("Structured Buffer = %u", unsigned(Device::StructuredBuffer::GetCount()));
							ImGui::Text("Texel Buffer = %u", unsigned(Device::TexelBuffer::GetCount()));
							ImGui::Text("Byte Address Buffer = %u", unsigned(Device::ByteAddressBuffer::GetCount()));
							ImGui::Text("Render Target = %u", unsigned(Device::RenderTarget::GetCount()));
							ImGui::Text("Index Buffer = %u", unsigned(Device::IndexBuffer::GetCount()));
							ImGui::Text("Vertex Buffer = %u", unsigned(Device::VertexBuffer::GetCount()));
							ImGui::Text("Dynamic Uniform Buffer = %u", unsigned(Device::DynamicUniformBuffer::GetCount()));
							ImGui::Text("Binding Set = %u", unsigned(Device::BindingSet::GetCount()));
							ImGui::Text("Dynamic Binding Set = %u", unsigned(Device::DynamicBindingSet::GetCount()));
							ImGui::Text("Descriptor Set = %u", unsigned(Device::DescriptorSet::GetCount()));
							EndSection();
						}

						if (IsPoppedOut() && BeginSection("Device Resources", false))
						{
							ImGui::Text("Binding Set: %u (%u)", unsigned(stats.device_binding_set_count), unsigned(stats.device_binding_set_frame_count));
							ImGui::Text("Descriptor Set: %u (%u)", unsigned(stats.device_descriptor_set_count), unsigned(stats.device_descriptor_set_frame_count));
							ImGui::Text("Pipeline: %u (%u)", unsigned(stats.device_pipeline_count), unsigned(stats.device_pipeline_frame_count));
							EndSection();
						}

						const float types_warmed_percentage = stats.type_count > 0 ? float(100.0 * double(stats.warmed_type_count) / double(stats.type_count)) : 0.0f;
						const float types_active_percentage = stats.type_count > 0 ? float(100.0 * double(stats.active_type_count) / double(stats.type_count)) : 0.0f;
						ImFormatString(buffer, std::size(buffer), "Types: %u (%.1f%% warmed, %.1f%% active)", unsigned(stats.type_count), types_warmed_percentage, types_active_percentage);
						PLOT_DELTA("DrawCallType Count", stats.type_count, 10.0f);
						if (BeginSection("Types", buffer, false))
						{
							ImGui::Text("QoS:  %.3f / %.3f / %.3f", stats.type_qos_min, stats.type_qos_avg, stats.type_qos_max);
							ImGui::PlotHistogram("", stats.type_qos_histogram.data(), (int)stats.type_qos_histogram.size(), 0, nullptr, FLT_MAX, FLT_MAX, ImVec2(), sizeof(float));
							for (unsigned i = (unsigned)Renderer::DrawCalls::Type::Default; i < (unsigned)Renderer::DrawCalls::Type::Count; ++i)
							{
								const auto count = stats.type_counts[i].first;
								const auto warmed_count = stats.type_counts[i].second;
								const float warmed_percentage = count > 0 ? float(100.0 * double(warmed_count) / double(count)) : 0.0f;
								const auto type_name = Renderer::DrawCalls::GetTypeName((Renderer::DrawCalls::Type)i);
								ImGui::Text("%.*s = %u (%.1f%% warmed)", unsigned(type_name.length()), type_name.data(), count, warmed_percentage);
							}
							EndSection();
						}

						const float pipelines_warmed_percentage = stats.pipeline_count > 0 ? float(100.0 * double(stats.warmed_pipeline_count) / double(stats.pipeline_count)) : 0.0f;
						const float pipelines_active_percentage = stats.pipeline_count > 0 ? float(100.0 * double(stats.active_pipeline_count) / double(stats.pipeline_count)) : 0.0f;
						ImFormatString(buffer, std::size(buffer), "Pipelines: %u (%.1f%% warmed, %.1f%% active)", unsigned(stats.pipeline_count), pipelines_warmed_percentage, pipelines_active_percentage);
						PLOT_DELTA("Pipeline Count", stats.pipeline_count, 10.0f);
						if (BeginSection("Pipelines", buffer, false))
						{
							ImGui::Text("QoS:  %.3f / %.3f / %.3f", stats.pipeline_qos_min, stats.pipeline_qos_avg, stats.pipeline_qos_max);
							ImGui::PlotHistogram("", stats.pipeline_qos_histogram.data(), (int)stats.pipeline_qos_histogram.size(), 0, nullptr, FLT_MAX, FLT_MAX, ImVec2(), sizeof(float));

							ImGui::Text("Budget: %s", Memory::ReadableSize(stats.budget).c_str());
							ImGui::Text("Usage: %s", Memory::ReadableSize(stats.usage).c_str());
							ImGui::Text("Cpass Buffer Fetches: %u", unsigned(stats.cpass_fetches));

							EndSection();
						}

						{
							const auto backbuffer_resolution = Device::DynamicResolution::Get().GetBackbufferResolution();
							const auto dynamic_scale = Device::DynamicResolution::Get().GetBackbufferToDynamicScale();
							const auto dynamic_resolution = backbuffer_resolution * dynamic_scale;

							ImGui::Text("Resolution = %u x %u", unsigned(backbuffer_resolution.x), unsigned(backbuffer_resolution.y));
							ImGui::Text("Scale = %.4f x %.4f", dynamic_scale.x, dynamic_scale.y);
							PLOT("Resolution Width", dynamic_resolution.x, 4096.0f);
							PLOT("Resolution Height", dynamic_resolution.y, 2160.0f);
							ImGui::Text("Dynamic = %u x %u", unsigned(dynamic_resolution.x), unsigned(dynamic_resolution.y));
						}

						EndSection();
					}

					if (begin_system("Shader"))
					{
						const auto stats = ::Shader::System::Get().GetStats();

						if (IsPoppedOut() && BeginSection("Settings", false))
						{
							ImGui::Text("Address:");
							ImGui::SameLine();
							ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
							ImGuiEx::TextAnimated(stats.address.c_str(), current_time);

							ImGui::Text("Async: %s", stats.enable_async ? "ON" : "OFF");
							ImGui::Text("Budget: %s", stats.enable_budget ? "ON" : "OFF");
							ImGui::Text("Delay: %s", stats.enable_delay ? "ON" : "OFF");
							ImGui::Text("Limit: %s", stats.enable_limit ? "ON" : "OFF");
							ImGui::Text("Warmup: %s", stats.enable_warmup ? "ON" : "OFF");
							ImGui::Text("Parallel: %s", stats.enable_parallel ? "ON" : "OFF");
							ImGui::Text("Potato: %s", stats.potato_mode ? "ON" : "OFF");
							EndSection();
						}

						if (IsPoppedOut() && BeginSection("Transient", false))
						{
							ImGui::Text("Uncompress: %u (cached %u)", unsigned(stats.transient_uncompress_count), unsigned(stats.cached_uncompress_count));
							ImGui::Text("Access: %u (cached %u)", unsigned(stats.transient_access_count), unsigned(stats.cached_access_count));
							ImGui::Text("Compilation: %u (cached %u)", unsigned(stats.transient_compilation_count), unsigned(stats.cached_compilation_count));
							ImGui::Text("Request: %u (cached %u)", unsigned(stats.transient_request_count), unsigned(stats.cached_request_count));
							EndSection();
						}

						const float programs_warmed_percentage = stats.program_count > 0 ? float(100.0 * double(stats.warmed_program_count) / double(stats.program_count)) : 0.0f;
						const float programs_active_percentage = stats.program_count > 0 ? float(100.0 * double(stats.active_program_count) / double(stats.program_count)) : 0.0f;
						ImFormatString(buffer, std::size(buffer), "Programs: %u (%.1f%% warmed, %.1f%% active)", unsigned(stats.program_count), programs_warmed_percentage, programs_active_percentage);
						PLOT_DELTA("Program Count", stats.program_count, 10.0f);
						if (BeginSection("Programs", buffer, false))
						{
							ImGui::Text("QoS:  %.3f / %.3f / %.3f", stats.program_qos_min, stats.program_qos_avg, stats.program_qos_max);
							ImGui::PlotHistogram("", stats.program_qos_histogram.data(), (int)stats.program_qos_histogram.size(), 0, nullptr, FLT_MAX, FLT_MAX, ImVec2(), sizeof(float));

							ImGui::Text("Budget: %s", Memory::ReadableSize(stats.budget).c_str());
							ImGui::Text("Usage: %s", Memory::ReadableSize(stats.usage).c_str());

							ImGui::Text("Color / Depth / Rays: %u / %u / %u", 
								unsigned(stats.shader_pass_program_counts[(unsigned)Renderer::ShaderPass::Color]),
								unsigned(stats.shader_pass_program_counts[(unsigned)Renderer::ShaderPass::DepthOnly]),
								unsigned(stats.shader_pass_program_counts[(unsigned)Renderer::ShaderPass::Rays]));
							EndSection();
						}

						ImFormatString(buffer, std::size(buffer), "Shaders: %u", unsigned(stats.shader_count));
						PLOT_DELTA("Shader Count", stats.shader_count, 10.0f);
						if (BeginSection("Shaders", buffer, false))
						{
							ImGui::Text("Cache: %u", unsigned(stats.loaded_from_cache));
							ImGui::Text("Drive: %u", unsigned(stats.loaded_from_drive));
							ImGui::Text("Compile: %u", unsigned(stats.compiled));
							ImGui::Text("Fetch: %u", unsigned(stats.fetched));
							ImGui::Text("Failed: %u", unsigned(stats.failed));
							EndSection();
						}

						EndSection();
					}

					if (begin_system("Texture"))
					{
						const auto stats = Texture::System::Get().GetStats();

						if (IsPoppedOut() && BeginSection("Settings", false))
						{
							ImGui::Text("Async: %s", stats.enable_async ? "ON" : "OFF");
							ImGui::Text("Potato: %s", stats.potato_mode ? "ON" : "OFF");
							ImGui::Text("Throttling: %s", stats.enable_throttling ? "ON" : "OFF");
							ImGui::Text("Budget: %s", stats.enable_budget ? "ON" : "OFF");
							EndSection();
						}

						const float levels_active_percentage = stats.level_count > 0 ? float(100.0 * double(stats.active_level_count) / double(stats.level_count)) : 0.0f;
						ImFormatString(buffer, std::size(buffer), "Levels: %u (%.1f%% active)", unsigned(stats.level_count),
							levels_active_percentage);
						PLOT_DELTA("Level Count", stats.level_count, 10.0f);
						if (BeginSection("Levels", buffer, false))
						{
							ImGui::Text("Created: %u", unsigned(stats.created_level_count));
							ImGui::Text("Touched: %u", unsigned(stats.touched_level_count));
							ImGui::Text("QoS:  %.3f / %.3f / %.3f", stats.level_qos_min, stats.level_qos_avg, stats.level_qos_max);
							ImGui::PlotHistogram("", stats.level_qos_histogram.data(), (int)stats.level_qos_histogram.size(), 0, nullptr, FLT_MAX, FLT_MAX, ImVec2(), sizeof(float));

							ImGui::Text("Budget: %s", Memory::ReadableSize(stats.budget).c_str());
							ImGui::Text("Usage: %s", Memory::ReadableSize(stats.usage).c_str());

							ImGui::Text("Total Size: %s (active %s)", Memory::ReadableSize(stats.levels_total_size).c_str(), Memory::ReadableSize(stats.levels_active_size).c_str());
							ImGui::Text("Interface Size: %s (active %s)", Memory::ReadableSize(stats.levels_interface_size).c_str(), Memory::ReadableSize(stats.levels_interface_active_size).c_str());
							ImGui::PlotHistogram("", stats.total_size_histogram.data(), (int)stats.total_size_histogram.size(), 0, nullptr, FLT_MAX, FLT_MAX, ImVec2(), sizeof(float));
							EndSection();
						}

						EndSection();
					}

					if (begin_system("Graph"))
					{
						const auto stats = EffectGraph::System::Get().GetStats();

						if (IsPoppedOut() && BeginSection("Settings", false))
						{
							EndSection();
						}

						const float graphs_active_percentage = stats.graph_count > 0 ? float(100.0 * double(stats.active_graph_count) / double(stats.graph_count)) : 0.0f;
						ImFormatString(buffer, std::size(buffer), "Graphs: %u (%.1f%% active)", unsigned(stats.graph_count), graphs_active_percentage);
						PLOT_DELTA("Graph Count", stats.graph_count, 10.0f);
						if (BeginSection("Graphs", buffer, false))
						{
							EndSection();
						}

						ImGui::Text("Instance: %u", unsigned(stats.instance_count));

						EndSection();
					}

				}

				if (!IsPoppedOut())
				{
					ImGui::EndTable();
				}
			}

			End();
		}

		
	}

	void ProfileSystemPlugin::OnGarbageCollect()
	{
		const auto& stats = Storage::System::Get().GetStats();

		auto& dst = scene_stats.push_back(SceneEndStats());
		dst.loaded_bundles = stats.bundle.io_count;
		dst.loaded_bytes = stats.bundle.io_read;
	}
#endif
}
