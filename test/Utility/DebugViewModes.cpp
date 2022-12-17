#include "DebugViewModes.h"
#include "Visual/Device/DynamicCulling.h"

#ifdef ENABLE_DEBUG_VIEWMODES
#include <atomic>

namespace Utility
{
	static std::atomic<ViewMode> CurrentViewMode = ViewMode::DefaultViewmode;

	static constexpr const char* VIEW_MODE_NAMES[(unsigned)ViewMode::NumViewModes] =
	{
		/* DefaultViewmode		*/ "Default",
		/* LightComplexity		*/ "Light Complexity",
		/* TiledLights  		*/ "Tiled Lights",
		/* LightModel			*/ "Lighting Model",
		/* MipLevel				*/ "Mip Level",
		/* DoubleSided			*/ "Double Sided",
		/* ShaderComplexity		*/ "Shader Complexity",
		/* Albedo				*/ "Albedo",
		/* Gloss				*/ "Gloss",
		/* Normal				*/ "World Normal (Fit)",
		/* Normal2				*/ "World Normal (True)",
		/* SurfaceNormal		*/ "Surface Normal",
		/* Specular				*/ "Specular",
		/* Indirect				*/ "Indirect Color",
		/* SubSurface			*/ "Subsurface",
		/* Glow					*/ "Glow",
	};

	static constexpr const char* VIEW_MODE_DESC[(unsigned)ViewMode::NumViewModes] =
	{
		/* DefaultViewmode		*/ "",
		/* LightComplexity		*/ "How many spot and directional lights affect the object:\n 0 lights : No color\n 1 light: Green\n 2 lights: Yellow\n 3 lights: Blue\n 4 lights: Red",
		/* TiledLights			*/ "Blue channel contains the number of point lights for that tile, times 64 (for better visualization)",
		/* LightModel			*/ "Red: SpecGlossPbrMaterial\nGreen: Anisotropy\nBlue: PhongMaterial",
		/* MipLevel				*/ "",
		/* DoubleSided			*/ "Orange: Double sided\nBlue: Single sided",
		/* ShaderComplexity		*/ "Shader cost from good to bad:\n Green -> Yellow -> Red",
		/* Albedo				*/ "",
		/* Gloss				*/ "",
		/* Normal				*/ "Normals are fitted in the color space, so black = (-1, -1, -1) and white = (1, 1, 1)",
		/* Normal2				*/ "Normals are displayed as is (the color value equals the corresponding normal value, negative values are black)",
		/* SurfaceNormal		*/ "",
		/* Specular				*/ "",
		/* Indirect				*/ "",
		/* SubSurface			*/ "",
		/* Glow					*/ "",
	};

	static constexpr const wchar_t* VIEW_MODE_GRAPHS[(unsigned)ViewMode::NumViewModes] =
	{
		/* DefaultViewmode		*/ L"",
		/* LightComplexity		*/ L"Metadata/EngineGraphs/debug_light.fxgraph",
		/* TiledLights			*/ L"Metadata/EngineGraphs/debug_tiled.fxgraph",
		/* LightModel			*/ L"Metadata/EngineGraphs/debug_lighting_model.fxgraph",
		/* MipLevel				*/ L"Metadata/EngineGraphs/debug_miplevel.fxgraph",
		/* DoubleSided			*/ L"Metadata/EngineGraphs/debug_doublesided.fxgraph",
		/* ShaderComplexity		*/ L"Metadata/EngineGraphs/debug_shadercomplexity.fxgraph",
		/* Albedo				*/ L"Metadata/EngineGraphs/debug_albedo.fxgraph",
		/* Gloss				*/ L"Metadata/EngineGraphs/debug_gloss.fxgraph",
		/* Normal				*/ L"Metadata/EngineGraphs/debug_normal.fxgraph",
		/* Normal2				*/ L"Metadata/EngineGraphs/debug_normal2.fxgraph",
		/* SurfaceNormal		*/ L"Metadata/EngineGraphs/debug_surface_normal.fxgraph",
		/* Specular				*/ L"Metadata/EngineGraphs/debug_specular.fxgraph",
		/* Indirect				*/ L"Metadata/EngineGraphs/debug_indirect.fxgraph",
		/* SubSurface			*/ L"Metadata/EngineGraphs/debug_subsurface.fxgraph",
		/* Glow					*/ L"Metadata/EngineGraphs/debug_glow.fxgraph",
	};

	std::wstring GetViewModeGraph(ViewMode mode)
	{
		const unsigned i = (unsigned)mode;
		if (i < (unsigned)ViewMode::NumViewModes)
			return VIEW_MODE_GRAPHS[i];

		return L"";
	}

	std::string GetViewModeName(ViewMode mode)
	{
		const unsigned i = (unsigned)mode;
		if (i < (unsigned)ViewMode::NumViewModes)
		{
			std::string r = VIEW_MODE_NAMES[i];
			if (r.length() > 0)
				return r;
		}
		return "Unnamed View Mode " + std::to_string(i);
	}

	std::string GetViewModeDescription(ViewMode mode)
	{
		const unsigned i = (unsigned)mode;
		if (i < (unsigned)ViewMode::NumViewModes)
			return VIEW_MODE_DESC[i];

		return "";
	}

	ViewMode GetCurrentViewMode()
	{
		return CurrentViewMode;
	}

	bool SetCurrentViewMode(ViewMode mode)
	{ 
		if (IsViewModeAvailable(mode))
			return CurrentViewMode.exchange(mode) != mode;

		return false;
	}

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

	bool ProcessViewModeHotKeys(unsigned long long wParam, const bool ctrl_down, const bool shift_down)
	{
		if (ctrl_down && shift_down)
		{
			switch (wParam)
			{
				case 'D':
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
					} while (!Utility::SetCurrentViewMode(view_mode));
					return last_view_mode != view_mode;
				}
				case 'A':
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
					} while (!Utility::SetCurrentViewMode(view_mode));
					return last_view_mode != view_mode;
				}
				case 'M':
				{
					// Toggle miplevel viewmode
					bool mipmap_mode = Utility::GetCurrentViewMode() == Utility::ViewMode::MipLevel;
					mipmap_mode = !mipmap_mode;
					if (mipmap_mode)
						return Utility::SetCurrentViewMode(Utility::ViewMode::MipLevel);
					else
						return Utility::SetCurrentViewMode(Utility::ViewMode::DefaultViewmode);
				}
				case 'S':
				{
					// Toggle shader complexity viewmode
					bool shader_complexity_mode = Utility::GetCurrentViewMode() == Utility::ViewMode::ShaderComplexity;
					shader_complexity_mode = !shader_complexity_mode;
					if (shader_complexity_mode)
						return Utility::SetCurrentViewMode(Utility::ViewMode::ShaderComplexity);
					else
						return Utility::SetCurrentViewMode(Utility::ViewMode::DefaultViewmode);
				}
				case 'X':
				{
					// Toggle double-sided viewmode
					bool double_sided_mode = Utility::GetCurrentViewMode() == Utility::ViewMode::DoubleSided;
					double_sided_mode = !double_sided_mode;
					if (double_sided_mode)
						return Utility::SetCurrentViewMode(Utility::ViewMode::DoubleSided);
					else
						return Utility::SetCurrentViewMode(Utility::ViewMode::DefaultViewmode);
				}
				case '9':
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

					return true;
				}
				default:
					break;
			}
		}
		return false;
	}
}

#endif