#pragma once

#include "Deployment/configs/Configuration.h" //To make sure we know wether or not debug view modes are enabled

#include <string>

namespace Utility
{
	enum class ViewMode : unsigned
	{
		DefaultViewmode = 0,

		LIGHT_BEGIN,
		LightComplexity = LIGHT_BEGIN,
		TiledLights,
		LightModel,
		LIGHT_END = LightModel,

		MipLevel,
		DoubleSided,
		ShaderComplexity,

		SHADING_BEGIN,
		Albedo = SHADING_BEGIN,
		Gloss,
		Normal,
		Normal2,
		SurfaceNormal,
		Specular,
		Indirect,
		SubSurface,
		Glow,
		SHADING_END = Glow,

		NumViewModes
	};

#ifdef ENABLE_DEBUG_VIEWMODES
	std::wstring GetViewModeGraph(ViewMode mode);
	std::string GetViewModeName(ViewMode mode);
	std::string GetViewModeDescription(ViewMode mode);
	ViewMode GetCurrentViewMode();
	bool SetCurrentViewMode(ViewMode mode);
	bool IsViewModeAvailable(ViewMode mode);
	bool ProcessViewModeHotKeys(unsigned long long wParam, const bool ctrl_down, const bool shift_down);
#endif
}
