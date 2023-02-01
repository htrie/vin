#pragma once

#include "DebugGUIDefines.h"

#if DEBUG_GUI_ENABLED
#include "Visual/GUI2/imgui/imgui.h"
#else
typedef unsigned int ImGuiID;
typedef int ImGuiDir;
#endif

#include <magic_enum/magic_enum.hpp>

namespace Device
{
	namespace GUI
	{
		struct Trays
		{
			enum TrayTypes
			{
				LeftTop,
				LeftBottom,
				RightTop,
				RightBottom,
				TopLeft,
				TopRight,
				BottomLeft,
				BottomRight,
				Center,

				Left = LeftTop,
				Right = RightTop,
				Top = TopLeft,
				Bottom = BottomLeft,

				// Legacy tray names:

				LeftTray = Left,
				RightTray1 = RightTop,
				RightTray2 = RightBottom,
				TopTray = Top,
				BottomTray1 = BottomLeft,
				BottomTray2 = BottomRight,
				CentreTray = Center,
			};
		};

		class DockBuilder
		{
		public:
			DockBuilder();
			~DockBuilder();

			static ImGuiID GetDockspaceId();
			static bool DockExists();

			ImGuiID GetTray( Trays::TrayTypes tray ) const;

		protected:
#if DEBUG_GUI_ENABLED
			ImGuiIO& io;
#endif
			ImGuiID tray_ids[magic_enum::enum_count<Trays::TrayTypes>()];
		};
	}
}