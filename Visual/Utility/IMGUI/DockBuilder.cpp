#include "DockBuilder.h"

namespace Device
{
	namespace GUI
	{
		namespace
		{
		#if DEBUG_GUI_ENABLED
			ImGuiID SplitNodeBySize(ImGuiID node, ImGuiDir dir, float size, ImGuiID* out_other)
			{
				constexpr float DOCKING_SPLITTER_SIZE = 2.0f;
				const size_t split_axis = dir == ImGuiDir_Down || dir == ImGuiDir_Up ? 1 : 0;
				float ratio = 0.5f;

				if (auto parent_node = ImGui::DockBuilderGetNode(node))
				{
					float size_avail = (parent_node->SizeRef[split_axis] - DOCKING_SPLITTER_SIZE);
					size_avail = std::max(size_avail, ImGui::GetStyle().WindowMinSize[split_axis] * 2.0f);
					ratio = std::clamp(size / size_avail, 0.0f, 1.0f);
				}

				return ImGui::DockBuilderSplitNode(node, dir, ratio, nullptr, out_other);
			}
		#endif
		}

		bool DockBuilder::DockExists()
		{
#if DEBUG_GUI_ENABLED
			return ImGui::DockBuilderGetNode( GetDockspaceId() ) != nullptr;
#else
			return false;
#endif
		}

		DockBuilder::DockBuilder()
#if DEBUG_GUI_ENABLED
			: io( ImGui::GetIO() )
#endif
		{
#if DEBUG_GUI_ENABLED
			const auto dockspace_id = GetDockspaceId();

			// Remove old dockspace
			ImGui::DockBuilderRemoveNode( dockspace_id );

			// Create new dockspace
			ImGui::DockSpace( dockspace_id, ImVec2( 0, 0 ), ImGuiDockNodeFlags_PassthruCentralNode );

			// Create trays
			ImGuiID node = dockspace_id;
			ImGui::DockBuilderSetNodeSize(node, ImGui::GetMainViewport()->WorkSize);
			ImGui::DockBuilderSplitNode(SplitNodeBySize(node, ImGuiDir_Left,	300, &node), ImGuiDir_Up,	0.5f, &tray_ids[uint32_t(Trays::LeftTop)],		&tray_ids[uint32_t(Trays::LeftBottom)]);
			ImGui::DockBuilderSplitNode(SplitNodeBySize(node, ImGuiDir_Right,	300, &node), ImGuiDir_Up,	0.5f, &tray_ids[uint32_t(Trays::RightTop)],		&tray_ids[uint32_t(Trays::RightBottom)]);
			ImGui::DockBuilderSplitNode(SplitNodeBySize(node, ImGuiDir_Down,	300, &node), ImGuiDir_Left, 0.5f, &tray_ids[uint32_t(Trays::BottomLeft)],	&tray_ids[uint32_t(Trays::BottomRight)]);
			ImGui::DockBuilderSplitNode(SplitNodeBySize(node, ImGuiDir_Up,		300, &node), ImGuiDir_Left, 0.5f, &tray_ids[uint32_t(Trays::TopLeft)],		&tray_ids[uint32_t(Trays::TopRight)]);
			tray_ids[uint32_t(Trays::Center)] = node;
#endif
		}

		DockBuilder::~DockBuilder()
		{
#if DEBUG_GUI_ENABLED
			ImGui::DockBuilderFinish( GetDockspaceId() );
#endif
		}

		ImGuiID DockBuilder::GetDockspaceId()
		{
#if DEBUG_GUI_ENABLED
			static ImGuiID dockspace_id = ImGui::GetID( "GGG_Dockspace" );
			return dockspace_id;
#else
			return 0;
#endif
		}

		ImGuiID DockBuilder::GetTray( Trays::TrayTypes tray ) const
		{
#if DEBUG_GUI_ENABLED
			return tray_ids[tray];
#else
			return 0;
#endif
		}
	}
}