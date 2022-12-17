#pragma once
#include "Visual/Utility/IMGUI/DebugGUIDefines.h"

#if DEBUG_GUI_ENABLED

#include "ClientInstanceCommon/Animation/AnimatedObject.h"

namespace Device
{
	class Line;
}

namespace LightEditor
{
	inline bool visualise_lights = false;
	inline unsigned light_visualisation_density = 3;

	using StateChangeCallback = std::function< void( const std::string&, const float ) >;
	
	bool RenderLightEditor( Animation::AnimatedObject& ao, bool& refresh_ao, const StateChangeCallback& callback = nullptr );
	void RenderLightVisualisation( Device::Line& line );
}

#endif