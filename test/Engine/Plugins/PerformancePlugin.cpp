#include "PerformancePlugin.h"

#include "Common/Utility/StringManipulation.h"

#include "Visual/GUI2/imgui/imgui.h"
#include "Visual/GUI2/imgui/imgui_ex.h"
#include "Visual/GUI2/Icons.h"
#include "Visual/Renderer/Scene/SceneSystem.h"
#include "Visual/Renderer/Scene/View.h"
#include "Visual/Renderer/DrawCalls/DrawCall.h"
#include "Visual/Device/DynamicCulling.h"
#include "Visual/Entity/EntitySystem.h"

#include "Include/magic_enum/magic_enum.hpp"

namespace Utility
{
	namespace
	{
		static std::atomic<ViewMode> current_view_mode = ViewMode::DefaultViewmode;

		struct ViewModeInfo
		{
			std::string_view name;
			std::string_view desc;
			std::wstring_view graph;
			Renderer::DrawCalls::Type filter;
		};

		static constexpr ViewModeInfo VIEW_MODE_INFOS[(unsigned)ViewMode::NumViewModes] = {
			/* DefaultViewmode		*/ { "Default", "", L"", Renderer::DrawCalls::Type::All },
			/* LightComplexity		*/ { "Light Complexity", "How many spot and directional lights affect the object:\n 0 lights : No color\n 1 light: Green\n 2 lights: Yellow\n 3 lights: Blue\n 4 lights: Red", L"Metadata/EngineGraphs/debug_light.fxgraph", Renderer::DrawCalls::Type::All },
			/* TiledLights  		*/ { "Tiled Lights", "Blue channel contains the number of point lights for that tile, times 64 (for better visualization)", L"Metadata/EngineGraphs/debug_tiled.fxgraph", Renderer::DrawCalls::Type::All },
			/* LightModel			*/ { "Lighting Model", "Red: SpecGlossPbrMaterial\nGreen: Anisotropy\nBlue: PhongMaterial", L"Metadata/EngineGraphs/debug_lighting_model.fxgraph", Renderer::DrawCalls::Type::All },
			/* DetailLighting		*/ { "Detail Lighting", "Shows objects with only the lighting on them, using the normal maps of the original material", L"Metadata/EngineGraphs/debug_detail_lighting.fxgraph", Renderer::DrawCalls::Type::All },
			/* LightingOnly			*/ { "Lighting Only", "Shows objects with only the lighting on them, without using the normal maps of the original material", L"Metadata/EngineGraphs/debug_lighting_only.fxgraph", Renderer::DrawCalls::Type::All },
			/* MipLevel				*/ { "Mip Level", "", L"Metadata/EngineGraphs/debug_miplevel.fxgraph", Renderer::DrawCalls::Type::All },
			/* SceneIndex			*/ { "Scene Index", "Green: scene 0\nRed: scene 1\nBlue: scene 2\nYellow: scene 3", L"Metadata/EngineGraphs/debug_sceneindex.fxgraph", Renderer::DrawCalls::Type::All },
			/* Overdraw 			*/ { "Overdraw", "",L"Metadata/EngineGraphs/debug_overdraw.fxgraph", Renderer::DrawCalls::Type::All },
			/* DoubleSided			*/ { "Double Sided", "Orange: Double sided\nBlue: Single sided",L"Metadata/EngineGraphs/debug_doublesided.fxgraph", Renderer::DrawCalls::Type::All },
			/* ShaderComplexity		*/ { "Shader Complexity", "Shader cost from good to bad:\n Green -> Yellow -> Red", L"Metadata/EngineGraphs/debug_shadercomplexity.fxgraph", Renderer::DrawCalls::Type::All },
			/* PerformanceOverlay	*/ { "Performance Overlay", "", L"Metadata/EngineGraphs/debug_performance.fxgraph", Renderer::DrawCalls::Type::All },
			/* Polygons				*/ { "Polygons", "", L"Metadata/EngineGraphs/debug_polygons.fxgraph", Renderer::DrawCalls::Type::All },
			/* TrailWireframe		*/ { "Trail Wireframe", "", L"Metadata/EngineGraphs/debug_trail_wireframe.fxgraph", Renderer::DrawCalls::Type::Trail },
			/* Albedo				*/ { "Albedo", "", L"Metadata/EngineGraphs/debug_albedo.fxgraph", Renderer::DrawCalls::Type::All },
			/* Gloss				*/ { "Gloss", "", L"Metadata/EngineGraphs/debug_gloss.fxgraph", Renderer::DrawCalls::Type::All },
			/* Roughness			*/ { "Roughness", "", L"Metadata/EngineGraphs/debug_roughness.fxgraph", Renderer::DrawCalls::Type::All },
			/* Normal				*/ { "World Normal (Fit)", "Normals are fitted in the color space, so black = (-1, -1, -1) and white = (1, 1, 1)", L"Metadata/EngineGraphs/debug_normal.fxgraph", Renderer::DrawCalls::Type::All },
			/* Normal2				*/ { "World Normal (True)", "Normals are displayed as is (the color value equals the corresponding normal value, negative values are black)", L"Metadata/EngineGraphs/debug_normal2.fxgraph", Renderer::DrawCalls::Type::All },
			/* SurfaceNormal		*/ { "Surface Normal", "", L"Metadata/EngineGraphs/debug_surface_normal.fxgraph", Renderer::DrawCalls::Type::All },
			/* Specular				*/ { "Specular", "", L"Metadata/EngineGraphs/debug_specular.fxgraph", Renderer::DrawCalls::Type::All },
			/* Indirect				*/ { "Indirect Color", "", L"Metadata/EngineGraphs/debug_indirect.fxgraph", Renderer::DrawCalls::Type::All },
			/* SubSurface			*/ { "Subsurface", "", L"Metadata/EngineGraphs/debug_subsurface.fxgraph", Renderer::DrawCalls::Type::All },
			/* Glow					*/ { "Glow", "", L"Metadata/EngineGraphs/debug_glow.fxgraph", Renderer::DrawCalls::Type::All },
		};

		bool IsViewModeAvailable(ViewMode mode)
		{
			switch (mode)
			{
				case ViewMode::NumViewModes:
				#ifdef CONSOLE
				case ViewMode::DoubleSided:
				#endif
					return false;
				default:
					return true;
			}
		}
	}

	std::string_view GetViewModeName( const ViewMode mode )
	{
		if( (int)mode >= 0 && mode < ViewMode::NumViewModes )
			return VIEW_MODE_INFOS[ (int)mode ].name;
		else
			return std::string_view();
	}

#ifdef ENABLE_DEBUG_VIEWMODES
	std::wstring_view GetViewModeGraph( const ViewMode mode, Renderer::DrawCalls::Type type)
	{
		if ((unsigned)mode >= std::size(VIEW_MODE_INFOS))
			return L"";

		const auto filter = VIEW_MODE_INFOS[(unsigned)mode].filter;
		if (filter != Renderer::DrawCalls::Type::All && filter != type)
			return L"";

		return VIEW_MODE_INFOS[(unsigned)mode].graph;
	}

	ViewMode GetCurrentViewMode()
	{
		return current_view_mode.load(std::memory_order_acquire);
	}
#endif
}

namespace Engine::Plugins
{
#ifdef PROFILING
	namespace
	{
		// Listed in order of decreasing importance
		enum class Quota
		{
			TextureMemory,
			AvgTexture,
			MaxTexture,
			PolyCount,
			TextureCount,
		};

		typedef Memory::BitSet<magic_enum::enum_count<Quota>()> QuotaFlags;

		QuotaFlags CheckQuotas(const Renderer::DrawCalls::DrawCallStats& stats, Renderer::DrawCalls::Type type)
		{
			QuotaFlags res;

			if (stats.texture_memory > 8 * Memory::MB)
				res.set(unsigned(Quota::TextureMemory));

			if (stats.texture_count > 0 && uint64_t(stats.avg_texture.x) * stats.avg_texture.y > 4 << 20) // avg_texture > 2k x 2k
				res.set(unsigned(Quota::AvgTexture));

			if (stats.texture_count > 0 && uint64_t(stats.max_texture.x) * stats.max_texture.y > 16 << 20)// max_texture > 4k x 4k
				res.set(unsigned(Quota::MaxTexture));

			if (stats.polygon_count > 50000)
				res.set(unsigned(Quota::PolyCount));

			if (stats.texture_count > 12)
				res.set(unsigned(Quota::TextureCount));

			return res;
		}

		int CompareQuota(const QuotaFlags& a, const QuotaFlags& b)
		{
			for (const auto& v : magic_enum::enum_values<Quota>())
				if (a.test(unsigned(v)) != b.test(unsigned(v)))
					return a.test(unsigned(v)) ? 1 : -1;

			return 0;
		}

		struct RecordedStats
		{
			std::string debug_name;
			Renderer::DrawCalls::DrawCall* draw_call;
			Renderer::DrawCalls::DrawCallStats stats;
			simd::vector4 overlay_data;
			QuotaFlags quotas;
			Renderer::DrawCalls::Type type;
			ImVec2 pos;
			bool visible;
			bool rect_hovered;

			bool operator<(const RecordedStats& o) const
			{
				if (visible != o.visible)
					return visible;

				for (const auto& v : magic_enum::enum_values<Quota>())
					if (quotas.test(unsigned(v)) != o.quotas.test(unsigned(v)))
						return quotas.test(unsigned(v));

				if(quotas.test(unsigned(Quota::AvgTexture)) || quotas.test(unsigned(Quota::MaxTexture)))
					if ((stats.texture_count > 0) != (o.stats.texture_count > 0))
						return stats.texture_count > 0;

				if (stats.texture_count > 0)
				{
					if (quotas.test(unsigned(Quota::AvgTexture)))
					{
						const auto ma = uint64_t(stats.avg_texture.x) * stats.avg_texture.y;
						const auto mb = uint64_t(o.stats.avg_texture.x) * o.stats.avg_texture.y;
						if (ma != mb)
							return ma > mb;
					}

					if (quotas.test(unsigned(Quota::MaxTexture)))
					{
						const auto la = uint64_t(stats.max_texture.x) * stats.max_texture.y;
						const auto lb = uint64_t(o.stats.max_texture.x) * o.stats.max_texture.y;
						if (la != lb)
							return la > lb;
					}
				}

				if (quotas.test(unsigned(Quota::PolyCount)))
					if (stats.polygon_count != o.stats.polygon_count)
						return stats.polygon_count > o.stats.polygon_count;

				if (quotas.test(unsigned(Quota::TextureCount)))
					if (stats.texture_count != o.stats.texture_count)
						return stats.texture_count > o.stats.texture_count;

				if (type != o.type)
					return type < o.type;

				return false;
			}
		};

	#if DEBUG_GUI_ENABLED
		void RedText(bool red, bool show_all, const char* fmt, ...)
		{
			if (!show_all && !red)
				return;

			if (red && show_all)
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));

			va_list args;
			va_start(args, fmt);
			ImGui::TextV(fmt, args);
			va_end(args);

			if (red && show_all)
				ImGui::PopStyleColor();
		}
	#endif
	}

	PerformancePlugin::PerformancePlugin() : Plugin(Feature_Visible | Feature_Settings)
	{
	#if DEBUG_GUI_ENABLED
		AddAction("Toggle Performance Mode", VK_D, Input::Modifier_Ctrl, [this]() { ToggleVisible(); });
	#endif

	#ifdef ENABLE_DEBUG_VIEWMODES
		AddAction("Cycle Lighting Viewmodes", VK_D, Input::Modifier_Ctrl | Input::Modifier_Shift, [this]()
		{
			// Cycle thru the lighting debug viewmodes
			auto view_mode = Utility::GetCurrentViewMode();
			const auto last_view_mode = view_mode;
			do {
				if (view_mode < Utility::ViewMode::LIGHT_BEGIN)
					view_mode = Utility::ViewMode::LIGHT_BEGIN;
				else if (view_mode >= Utility::ViewMode::LIGHT_END)
					view_mode = Utility::ViewMode::DefaultViewmode;
				else
					view_mode = (Utility::ViewMode)((unsigned)view_mode + 1);
			} while (!SetViewMode(view_mode));
		});

		AddAction("Cycle Shading Viewmodes", VK_A, Input::Modifier_Ctrl | Input::Modifier_Shift, [this]()
		{
			// Cycle thru the lighting debug viewmodes
			auto view_mode = Utility::GetCurrentViewMode();
			const auto last_view_mode = view_mode;
			do {
				if (view_mode < Utility::ViewMode::SHADING_BEGIN)
					view_mode = Utility::ViewMode::SHADING_BEGIN;
				else if (view_mode >= Utility::ViewMode::SHADING_END)
					view_mode = Utility::ViewMode::DefaultViewmode;
				else
					view_mode = (Utility::ViewMode)((unsigned)view_mode + 1);
			} while (!SetViewMode(view_mode));
		});

		AddAction("Toggle Miplevel Viewmode", VK_M, Input::Modifier_Ctrl | Input::Modifier_Shift, [this]()
		{
			// Toggle miplevel viewmode
			bool mipmap_mode = Utility::GetCurrentViewMode() == Utility::ViewMode::MipLevel;
			mipmap_mode = !mipmap_mode;
			if (mipmap_mode)
				SetViewMode(Utility::ViewMode::MipLevel);
			else
				SetViewMode(Utility::ViewMode::DefaultViewmode);
		});

		AddAction("Toggle Shader Complexity Viewmode", VK_S, Input::Modifier_Ctrl | Input::Modifier_Shift, [this]()
		{
			// Toggle shader complexity viewmode
			bool shader_complexity_mode = Utility::GetCurrentViewMode() == Utility::ViewMode::ShaderComplexity;
			shader_complexity_mode = !shader_complexity_mode;
			if (shader_complexity_mode)
				SetViewMode(Utility::ViewMode::ShaderComplexity);
			else
				SetViewMode(Utility::ViewMode::DefaultViewmode);
		});

		AddAction("Toggle Overdraw Viewmode", VK_O, Input::Modifier_Ctrl | Input::Modifier_Shift, [this]()
		{
			// Toggle overdraw viewmode
			bool overdraw_mode = Utility::GetCurrentViewMode() == Utility::ViewMode::Overdraw;
			overdraw_mode = !overdraw_mode;
			if (overdraw_mode)
				SetViewMode(Utility::ViewMode::Overdraw);
			else
				SetViewMode(Utility::ViewMode::DefaultViewmode);
		});

		AddAction("Toggle Double-Sided Viewmode", VK_X, Input::Modifier_Ctrl | Input::Modifier_Shift, [this]()
		{
			// Toggle double-sided viewmode
			bool double_sided_mode = Utility::GetCurrentViewMode() == Utility::ViewMode::DoubleSided;
			double_sided_mode = !double_sided_mode;
			if (double_sided_mode)
				SetViewMode(Utility::ViewMode::DoubleSided);
			else
				SetViewMode(Utility::ViewMode::DefaultViewmode);
		});

		AddAction("Cycle Dynamic Culling Modes", VK_9, Input::Modifier_Ctrl | Input::Modifier_Shift, [this]()
		{
			unsigned int filter = 0;
			if (Device::DynamicCulling::Get().Enabled())
				filter = 1 + (unsigned int)Device::DynamicCulling::Get().GetViewFilter();

			const auto next_filter = (filter + 1) % (((unsigned int)Device::DynamicCulling::ViewFilter::NumViewFilters) + 1);

			Device::DynamicCulling::Get().Enable(next_filter > 0);
			Device::DynamicCulling::Get().ForceActive(next_filter > 1);
			if (next_filter > 0)
				Device::DynamicCulling::Get().SetViewFilter((Device::DynamicCulling::ViewFilter)(next_filter - 1));
			else
				Device::DynamicCulling::Get().SetViewFilter(Device::DynamicCulling::ViewFilter::Default);
		});
	#endif
	}

	bool PerformancePlugin::SetViewMode(const Utility::ViewMode mode)
	{
		if (!Utility::IsViewModeAvailable(mode))
			return false;

		SetVisible(mode == Utility::ViewMode::PerformanceOverlay);
		Utility::current_view_mode.store(mode, std::memory_order_release);
		return true;
	}

	void PerformancePlugin::OnRender(float elapsed_time)
	{
		if (!IsVisible())
		{
			auto exp = Utility::ViewMode::PerformanceOverlay;
			Utility::current_view_mode.compare_exchange_strong(exp, Utility::ViewMode::DefaultViewmode, std::memory_order_acq_rel);
			return;
		}

	#if DEBUG_GUI_ENABLED
		SetViewMode(Utility::ViewMode::PerformanceOverlay);

		auto* camera = Renderer::Scene::System::Get().GetCamera();
		if (camera)
		{
			char buffer[256] = { '\0' };
			unsigned id = 0;

			const auto& view_proj = camera->GetViewProjectionMatrix();
			const auto& view_frustum = camera->GetFrustum();

			const auto viewport = ImGui::GetMainViewport();
			const auto workRect = ImRect(viewport->WorkPos, viewport->WorkPos + viewport->WorkSize);

			const auto tooltip_flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize;

			Memory::SmallVector<RecordedStats, 32, Memory::Tag::Unknown> final_stats;

			Entity::System::Get().Process([&](auto& cull, auto& templ, auto& render) 
			{
				const auto type = templ.GetType();
				if (IsCulled(type))
					return;

				const auto& bounding_box = cull.bounding_box;
				if (view_frustum.TestBoundingBox(bounding_box) == 0)
					return;

				const auto stats = render.draw_call->GetDrawStats();
				if (stats.polygon_count == 0)
					return;

				const auto mid_point = (bounding_box.minimum_point + bounding_box.maximum_point) / 2;
				const auto screen_point = view_proj * simd::vector4(mid_point, 1.0f);
				
				final_stats.emplace_back();
				final_stats.back().draw_call = render.draw_call.get();
				final_stats.back().stats = stats;
				final_stats.back().quotas = CheckQuotas(final_stats.back().stats, type);
				final_stats.back().type = type;
				final_stats.back().visible = false;
				final_stats.back().rect_hovered = false;
				final_stats.back().overlay_data = simd::vector4(0);
				final_stats.back().debug_name = WstringToString(std::wstring(templ.GetDebugName()));

				if (final_stats.back().quotas.any())
					final_stats.back().overlay_data.y = 1.0f;

				if (screen_point.z / screen_point.w > 0)
				{
					final_stats.back().pos = (ImVec2((screen_point.x / screen_point.w / 2.0f) + 0.5f, 0.5f - (screen_point.y / screen_point.w / 2.0f)) * viewport->WorkSize) + viewport->WorkPos;
					final_stats.back().visible = true;
				}
			});

			if (final_stats.size() > 0)
			{
				std::sort(final_stats.begin(), final_stats.end());

				bool any_hovered = false;
				for (size_t a = 0, b = 0; a < final_stats.size(); a++)
				{
					auto& entry = final_stats[a];
					if (!entry.visible)
						break;

					if (!show_all_overlays && !entry.quotas.any())
						continue;

					if (!show_all_overlays && max_overlays > 0 && b++ > max_overlays)
						break;

					ImGui::SetNextWindowViewport(viewport->ID);
					ImGui::SetNextWindowPos(entry.pos);
					ImGui::PushStyleColor(ImGuiCol_WindowBg, ImLerp(ImGui::GetStyleColorVec4(ImGuiCol_WindowBg), ImVec4(1.0f, 0.0f, 0.0f, 1.0f), show_all_overlays && entry.quotas.any() ? 0.1f : 0.0f));

					ImFormatString(buffer, std::size(buffer), "Performance##PerfWindow%u", id++);
					if (ImGui::Begin(buffer, nullptr, tooltip_flags))
					{
						const auto name = Renderer::DrawCalls::GetTypeName(entry.type);
						if (name.length() > 0)
							ImGui::Text("Type: %.*s", unsigned(name.length()), name.data());
						else
							ImGui::Text("Type: Unknown");

						RedText(entry.quotas.test(unsigned(Quota::PolyCount)), show_all_overlays, "Polygon Count: %u", entry.stats.polygon_count);
						RedText(entry.quotas.test(unsigned(Quota::TextureCount)), show_all_overlays, "Texture Count: %u", entry.stats.texture_count);
						RedText(entry.quotas.test(unsigned(Quota::TextureMemory)), show_all_overlays, "Texture Memory: %s", ImGuiEx::FormatMemory(buffer, entry.stats.texture_memory));
						if (entry.stats.texture_count > 0)
						{
							RedText(entry.quotas.test(unsigned(Quota::AvgTexture)), show_all_overlays, "Avg Texture: %d x %d", entry.stats.avg_texture.x, entry.stats.avg_texture.y);
							RedText(entry.quotas.test(unsigned(Quota::MaxTexture)), show_all_overlays, "Max Texture: %d x %d", entry.stats.max_texture.x, entry.stats.max_texture.y);
						}

						if (ImGui::IsMouseHoveringRect(ImGui::GetWindowPos(), ImGui::GetWindowPos() + ImGui::GetWindowSize()))
							entry.rect_hovered = true;

						if (ImGui::IsWindowHovered())
							any_hovered = true;
					}

					ImGui::End();
					ImGui::PopStyleColor();
				}

				if (any_hovered)
				{
					for (auto& a : final_stats)
					{
						if (a.rect_hovered)
						{
							a.overlay_data.x = 1.0f;
							if (a.debug_name.length() > 0)
							{
								ImGui::BeginTooltip();
								ImGui::Text("%s", a.debug_name.c_str());
								ImGui::EndTooltip();
							}
						}
					}
				}

				bool is_visible = true;
				if (ImGui::Begin("Performance Overlay", &is_visible))
				{
					auto flags = ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_BordersV | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti | ImGuiTableFlags_SortTristate;

					enum TableID : ImU32
					{
						ID_Quota,
						ID_Type,
						ID_PolyCount,
						ID_TextureCount,
						ID_TextureMemory,
						ID_AvgTexture,
						ID_MaxTexture,
					};

					if (ImGui::BeginTable("##StatTable", int(magic_enum::enum_count<TableID>()), flags))
					{
						ImGui::TableSetupColumn("", ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed, 0.0f, ID_Quota);
						ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch, 0.0f, ID_Type);
						ImGui::TableSetupColumn("Polygon Count", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_PreferSortDescending, 0.0f, ID_PolyCount);
						ImGui::TableSetupColumn("Texture Count", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_PreferSortDescending, 0.0f, ID_TextureCount);
						ImGui::TableSetupColumn("Texture Memory", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_PreferSortDescending, 0.0f, ID_TextureMemory);
						ImGui::TableSetupColumn("Average Texture", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_PreferSortDescending, 0.0f, ID_AvgTexture);
						ImGui::TableSetupColumn("Largest Texture", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_PreferSortDescending, 0.0f, ID_MaxTexture);
						ImGui::TableSetupScrollFreeze(0, 1); // Make row always visible
						ImGui::TableHeadersRow();

						if (auto sortSpecs = ImGui::TableGetSortSpecs(); sortSpecs && sortSpecs->SpecsCount > 0)
						{
							auto SortCompare = [&](const RecordedStats& a, const RecordedStats& b, TableID id) -> int
							{
								switch (id)
								{
									case ID_Quota:
									{
										if (a.quotas.any() == b.quotas.any())
											return 0;

										return a.quotas.any() ? 1 : -1;
									}
									case ID_Type:
									{
										if (a.type == b.type)
											return 0;

										return a.type > b.type ? 1 : -1;
									}
									case ID_PolyCount:
									{
										if (a.stats.polygon_count == b.stats.polygon_count)
											return 0;

										return a.stats.polygon_count > b.stats.polygon_count ? 1 : -1;
									}
									case ID_TextureCount:
									{
										if (a.stats.texture_count == b.stats.texture_count)
											return 0;

										return a.stats.texture_count > b.stats.texture_count ? 1 : -1;
									}
									case ID_TextureMemory:
									{
										if (a.stats.texture_memory == b.stats.texture_memory)
											return 0;

										return a.stats.texture_memory > b.stats.texture_memory ? 1 : -1;
									}
									case ID_AvgTexture:
									{
										if ((a.stats.texture_count > 0) != (b.stats.texture_count > 0))
											return a.stats.texture_count > 0 ? 1 : -1;

										if (a.stats.texture_count == 0)
											return 0;

										const auto ma = uint64_t(a.stats.avg_texture.x) * a.stats.avg_texture.y;
										const auto mb = uint64_t(b.stats.avg_texture.x) * b.stats.avg_texture.y;

										if (ma == mb)
											return 0;

										return ma > mb ? 1 : -1;
									}
									case ID_MaxTexture:
									{
										if ((a.stats.texture_count > 0) != (b.stats.texture_count > 0))
											return a.stats.texture_count > 0 ? 1 : -1;

										if (a.stats.texture_count == 0)
											return 0;

										const auto ma = uint64_t(a.stats.max_texture.x) * a.stats.max_texture.y;
										const auto mb = uint64_t(b.stats.max_texture.x) * b.stats.max_texture.y;

										if (ma == mb)
											return 0;

										return ma > mb ? 1 : -1;
									}
									default:
										break;
								}

								return 0;
							};

							auto SortFunc = [&](const RecordedStats& a, const RecordedStats& b)
							{
								for (int i = 0; i < sortSpecs->SpecsCount; i++)
								{
									const auto& specs = sortSpecs->Specs[i];
									const auto order = SortCompare(a, b, (TableID)specs.ColumnUserID);

									if (order == 0)
										continue;

									return (order < 0) == (specs.SortDirection == ImGuiSortDirection_Ascending);
								}

								return false;
							};

							std::sort(final_stats.begin(), final_stats.end(), SortFunc);
						}

						const float row_height = ImGui::GetTextLineHeight() + (ImGui::GetStyle().CellPadding.y * 2);
						ImGuiListClipper clipper;
						clipper.Begin(int(final_stats.size()), row_height);
						while (clipper.Step())
						{
							for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
							{
								auto& entry = final_stats[i];
								ImGui::PushID(i);
								ImGui::TableNextRow();

								// Quota
								{
									ImGui::TableNextColumn();
									ImGui::Selectable("##HiddenSelectable", entry.overlay_data.x > 0.5f, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap);
									if (ImGui::IsItemHovered())
									{
										entry.overlay_data.x = 1.0f;
										if (entry.debug_name.length() > 0)
										{
											ImGui::BeginTooltip();
											ImGui::Text("File: %s", entry.debug_name.c_str());
											ImGui::EndTooltip();
										}
									}

									ImGui::SameLine(0.0f, 0.0f);
									ImGui::PushStyleColor(ImGuiCol_Text, entry.quotas.any() ? ImVec4(1.0f, 0.0f, 0.0f, 1.0f) : ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
									ImGui::Text("%s", entry.quotas.any() ? IMGUI_ICON_NO : IMGUI_ICON_YES);
									ImGui::PopStyleColor();
								}

								// Type
								{
									ImGui::TableNextColumn();
									const auto name = Renderer::DrawCalls::GetTypeName(entry.type);
									if (name.length() > 0)
										ImGui::Text("%.*s", unsigned(name.length()), name.data());
									else
										ImGui::Text("Unknown");
								}

								// Polygon Count
								{
									ImGui::TableNextColumn();
									RedText(entry.quotas.test(unsigned(Quota::PolyCount)), true, "%u", entry.stats.polygon_count);
								}

								// Texture Count
								{
									ImGui::TableNextColumn();
									RedText(entry.quotas.test(unsigned(Quota::TextureCount)), true, "%u", entry.stats.texture_count);
								}

								// Texture Memory
								{
									ImGui::TableNextColumn();
									if (entry.stats.texture_count > 0)
										RedText(entry.quotas.test(unsigned(Quota::TextureMemory)), true, "%s", ImGuiEx::FormatMemory(buffer, entry.stats.texture_memory));
								}

								// Avg Texture
								{
									ImGui::TableNextColumn();
									if (entry.stats.texture_count > 0)
										RedText(entry.quotas.test(unsigned(Quota::AvgTexture)), true, "%d x %d", entry.stats.avg_texture.x, entry.stats.avg_texture.y);
								}

								// Max Texture
								{
									ImGui::TableNextColumn();
									if (entry.stats.texture_count > 0)
										RedText(entry.quotas.test(unsigned(Quota::MaxTexture)), true, "%d x %d", entry.stats.max_texture.x, entry.stats.max_texture.y);
								}

								ImGui::PopID();
							}
						}

						ImGui::EndTable();
					}
				}

				ImGui::End();
				if (!is_visible)
				{
					SetVisible(false);
					SetViewMode(Utility::ViewMode::DefaultViewmode);
				}

				for (const auto& a : final_stats)
					a.draw_call->SetPerformanceOverlay(a.overlay_data);
			}
		}
	#endif
	}

	void PerformancePlugin::OnRenderSettings(float elapsed_time)
	{
	#if DEBUG_GUI_ENABLED
		ImGui::Checkbox("Show all popups", &show_all_overlays);
		
		if (!show_all_overlays)
		{
			bool limit_overlays = max_overlays > 0;
			if (ImGui::Checkbox("Limit popup count", &limit_overlays))
				max_overlays = limit_overlays ? 32 : 0;

			if (max_overlays > 0)
			{
				int num_overlays = int(max_overlays);
				if (ImGui::SliderInt("##PopupCount", &num_overlays, 1, 64, "%d", ImGuiSliderFlags_AlwaysClamp))
					max_overlays = size_t(num_overlays);
			}
		}
	#endif
	}
	
	void PerformancePlugin::OnLoadSettings(const rapidjson::Value& settings)
	{
		if (const auto f = settings.FindMember("show_all_overlays"); f != settings.MemberEnd() && f->value.IsBool())
			show_all_overlays = f->value.GetBool();

		if (const auto f = settings.FindMember("max_overlays"); f != settings.MemberEnd() && f->value.IsInt())
			max_overlays = f->value.GetInt() > 0 ? size_t(f->value.GetInt()) : 0;
	}

	void PerformancePlugin::OnSaveSettings(rapidjson::Value& settings, rapidjson::Document::AllocatorType& allocator)
	{
		settings.AddMember("show_all_overlays", show_all_overlays, allocator);
		settings.AddMember("max_overlays", int(max_overlays), allocator);
	}

	bool PerformancePlugin::IsCulled(Renderer::DrawCalls::Type type) const
	{
		if (!IsEnabled() || type == Renderer::DrawCalls::Type::All || type == Renderer::DrawCalls::Type::None)
			return false;

		return culling_mask.test(unsigned(type));
	}

	void PerformancePlugin::RenderUI()
	{
	#if DEBUG_GUI_ENABLED
		char buffer[256] = { '\0' };

		const float item_width = ImGui::CalcItemWidth();
		const float inner_spacing = ImGui::GetStyle().ItemInnerSpacing.x;
		const float frame_height = ImGui::GetFrameHeight();

	#ifdef ENABLE_DEBUG_VIEWMODES
		const auto view_mode = Utility::GetCurrentViewMode();
		const std::string_view view_mode_label = unsigned(view_mode) < std::size(Utility::VIEW_MODE_INFOS) ? Utility::VIEW_MODE_INFOS[unsigned(view_mode)].name : "";
		const std::string_view view_mode_desc = unsigned(view_mode) < std::size(Utility::VIEW_MODE_INFOS) ? Utility::VIEW_MODE_INFOS[unsigned(view_mode)].desc : "";

		if (view_mode_desc.length() > 0)
			ImGui::SetNextItemWidth(item_width - (inner_spacing + frame_height));

		if (view_mode != Utility::ViewMode::DefaultViewmode && view_mode_label.length() > 0)
			ImFormatString(buffer, std::size(buffer), "View Mode: %.*s", unsigned(view_mode_label.length()), view_mode_label.data());
		else
			ImFormatString(buffer, std::size(buffer), "View Mode");

		if (ImGui::BeginCombo("##ViewModes", buffer))
		{
			for (unsigned mode = 0; mode < unsigned(Utility::ViewMode::NumViewModes); mode++)
			{
				if (!Utility::IsViewModeAvailable(Utility::ViewMode(mode)))
					continue;

				std::string_view name = mode < std::size(Utility::VIEW_MODE_INFOS) ? Utility::VIEW_MODE_INFOS[mode].name : "";
				if (name.empty())
					continue;

				std::string_view desc = mode < std::size(Utility::VIEW_MODE_INFOS) ? Utility::VIEW_MODE_INFOS[mode].desc : "";
				ImFormatString(buffer, std::size(buffer), "%.*s", unsigned(name.length()), name.data());
				if (ImGui::Selectable(buffer, unsigned(view_mode) == mode))
					SetViewMode(Utility::ViewMode(mode));

				if (desc.length() > 0 && ImGui::IsItemHovered())
				{
					ImGui::BeginTooltip();
					ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
					ImGui::Text("%.*s", unsigned(desc.length()), desc.data());
					ImGui::PopTextWrapPos();
					ImGui::EndTooltip();
				}

				if (unsigned(view_mode) == mode)
					ImGui::SetItemDefaultFocus();
			}

			ImGui::EndCombo();
		}

		if (view_mode_desc.length() > 0)
		{
			ImGui::SameLine(0, inner_spacing);
			ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
			ImGui::Button("?", ImVec2(frame_height, frame_height));
			ImGui::PopItemFlag();
			if (ImGui::IsItemHovered())
			{
				ImGui::BeginTooltip();
				ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
				ImGui::Text("%.*s", unsigned(view_mode_desc.length()), view_mode_desc.data());
				ImGui::PopTextWrapPos();
				ImGui::EndTooltip();
			}
		}
	#endif

		ImFormatString(buffer, std::size(buffer), "Visible: None");
		unsigned visible_count = 0;
		unsigned mask_count = 0;
		for (const auto& a : magic_enum::enum_values<Renderer::DrawCalls::Type>())
		{
			const auto name = Renderer::DrawCalls::GetTypeName(a);
			if (name.length() > 0)
			{
				mask_count++;
				if (!culling_mask.test(unsigned(a)))
				{
					visible_count++;
					ImFormatString(buffer, std::size(buffer), "Visible: %.*s", unsigned(name.length()), name.data());
				}
			}
		}

		if(visible_count == mask_count)
			ImFormatString(buffer, std::size(buffer), "Visible: All");
		else if(visible_count > 1)
			ImFormatString(buffer, std::size(buffer), "Visible: %u Types", visible_count);

		ImGuiEx::PushDisable(!IsEnabled());
		if (ImGui::BeginCombo("##CullingModes", buffer,ImGuiComboFlags_HeightLargest))
		{
			for (const auto& a : magic_enum::enum_values<Renderer::DrawCalls::Type>())
			{
				if (a == Renderer::DrawCalls::Type::None || a == Renderer::DrawCalls::Type::All)
					continue;

				ImGui::PushID(int(a));
				const auto name = Renderer::DrawCalls::GetTypeName(a);
				if (name.length() > 0)
				{
					bool enabled = !culling_mask.test(unsigned(a));
					ImFormatString(buffer, std::size(buffer), "%.*s###VisibleCheckBox", unsigned(name.length()), name.data());
					if (ImGui::Checkbox(buffer, &enabled))
					{
						if (ImGui::GetIO().KeyCtrl)
						{
							bool set = false;
							for (const auto& b : magic_enum::enum_values<Renderer::DrawCalls::Type>())
								if (a != b && Renderer::DrawCalls::GetTypeName(b).length() > 0 && !culling_mask.test(unsigned(b)))
									set = true;

							if (set)
							{
								for (const auto& b : magic_enum::enum_values<Renderer::DrawCalls::Type>())
									if (Renderer::DrawCalls::GetTypeName(b).length() > 0)
										culling_mask.set(unsigned(b), a != b);
							}
							else
								culling_mask.reset();
						}
						else
							culling_mask.set(unsigned(a), !enabled);
					}
				}

				ImGui::PopID();
			}

			ImGui::EndCombo();
		}

		ImGuiEx::PopDisable();
	#endif
	}
#endif
}