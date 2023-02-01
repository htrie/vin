#pragma once

#include "DebugGUIDefines.h"

#if DEBUG_GUI_ENABLED

#include "Visual/GUI2/GUIResourceManager.h"
#include "Visual/GUI2/imgui/imgui.h"

namespace Device::GUI::Debug
{
	struct WindowState
	{
		float width;
		float height;
	};

	class UIDebugGUI : public Device::GUI::GUIInstance
	{
	public:
		UIDebugGUI();
		~UIDebugGUI();

		virtual void OnRender( const float elapsed_time ) override;

	private:
		bool draw_mouse_lines = false;
		bool draw_mouse_coords = false;
		bool hide_cursor = false;
		float line_color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	};
}

#endif
