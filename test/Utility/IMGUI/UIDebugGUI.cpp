#include "UIDebugGUI.h"

#if DEBUG_GUI_ENABLED

#include "Common/Utility/Format.h"

namespace Device::GUI::Debug
{
	UIDebugGUI::UIDebugGUI()
		: Device::GUI::GUIInstance( false )
	{
		if(const auto& manager = GUIResourceManager::GetGUIResourceManager())
		{
			line_color[0] = manager->ui_debug.line_color.r / 255.0f;
			line_color[1] = manager->ui_debug.line_color.g / 255.0f;
			line_color[2] = manager->ui_debug.line_color.b / 255.0f;
			line_color[3] = manager->ui_debug.line_color.a / 255.0f;

			draw_mouse_lines = manager->ui_debug.draw_mouse_lines;
			draw_mouse_coords = manager->ui_debug.draw_mouse_coords;
			hide_cursor = manager->ui_debug.hide_cursor;
		}
	}

	UIDebugGUI::~UIDebugGUI()
	{

	}

	void UIDebugGUI::OnRender( const float elapsed_time )
	{
		if( !open )
			return;

		if( ImGui::Begin( "UI Debug Settings", &open, ImGuiWindowFlags_NoFocusOnAppearing ) )
		{
			const auto& manager = GUIResourceManager::GetGUIResourceManager();
			if( ImGui::CollapsingHeader( "Mouse", ImGuiTreeNodeFlags_DefaultOpen ) )
			{
				ImGui::Text("X: %d   Y: %d", manager->ui_debug.mouse_x, manager->ui_debug.mouse_y);

				if( ImGui::Checkbox( "Draw Mouse Lines", &draw_mouse_lines ) )
				{
					manager->ui_debug.draw_mouse_lines = draw_mouse_lines;
					if( !draw_mouse_lines && hide_cursor )
					{
						hide_cursor = false;
						manager->ui_debug.hide_cursor = false;
					}
				}
				ImGui::SameLine();
				if( ImGui::Checkbox( "Hide Cursor", &hide_cursor ) )
				{
					manager->ui_debug.hide_cursor = hide_cursor;
					if( hide_cursor && !draw_mouse_lines )
					{
						draw_mouse_lines = true;
						manager->ui_debug.draw_mouse_lines = true;
					}
				}

				if( ImGui::Checkbox( "Draw Mouse Coordinates", &draw_mouse_coords ) )
				{
					manager->ui_debug.draw_mouse_coords = draw_mouse_coords;
				}

				if( ImGui::ColorEdit3( "Color", line_color, ImGuiColorEditFlags_AlphaPreview ) )
					manager->ui_debug.line_color = { line_color[0], line_color[1], line_color[2], line_color[3] };
			}
		}

		ImGui::End();
	}
}

#endif