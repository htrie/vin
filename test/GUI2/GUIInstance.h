#pragma once

#include "Visual/Device/Constants.h"
#include "Visual/Utility/ToolSettings.h"
#include "Visual/Device/Constants.h"
#include "../Utility/IMGUI/DockBuilder.h"

namespace Device
{
	class IDevice;
	struct SurfaceDesc;

	namespace GUI
	{
		class GUIResourceManager;
		class GUIInstance
		{
		public:
			enum ListenFlags
			{
				DeviceCallbacks = 1,
				RenderCallbacks = 2,
				MsgProcCallbacks = 4,
				LayoutCallbacks = 8,
				InputCallback = 16,

				AllCallbacks = DeviceCallbacks | RenderCallbacks | MsgProcCallbacks | LayoutCallbacks | InputCallback,
			};

			bool IsListenFlagSet( const ListenFlags flag ) const;

			// When set to true, this GUIInstance will defer to the GUIResourceManager for Render/MsgProc callbacks for the lifetime of this instance
			GUIInstance( const bool listen = false, unsigned listen_flags = AllCallbacks );
			virtual ~GUIInstance();

		//	std::shared_ptr<Utility::ToolSettings> GetSettings();
		//	GUIResourceManager* GetManager();

			virtual void Initialise() {}
			virtual void Destroy() {}
			virtual void OnCreateDevice( IDevice* device ) {}
			virtual void OnResetDevice( IDevice* device, const SurfaceDesc* backbuffer_surface_desc ) {}
			virtual void OnLostDevice() {}
			virtual void OnDestroyDevice() {}
			virtual void OnRender( const float elapsed_time ) {}
			virtual void OnRenderMenu( const float elapsed_time ) {}
			virtual void OnPostRender() {}
			virtual bool MsgProc( HWND hwnd, UINT umsg, WPARAM wParam, LPARAM lParam ) { return false; }
			virtual bool OnProcessInput() { return false; }
			virtual void OnResetLayout( const DockBuilder& dock_builder ) {}
			virtual void OnLoadSettings(Utility::ToolSettings* settings) {}
			virtual void OnSaveSettings(Utility::ToolSettings* settings) {}

			void NewFrame();

			bool open = false;
			static bool UpdateInput();
			static void ClearInput();
			static bool IsKeyPressed(unsigned user_key);
			static bool IsKeyDown(unsigned user_key);
			static bool IsMouseButtonPressed(unsigned user_key);
			static bool IsMouseButtonDown(unsigned user_key);
			static float GetMouseWheel();

			static void SetMouseWheel(const float delta);
			static void AddKeyEvent(const unsigned user_key, const bool down);
			static void AddMouseButtonEvent(const unsigned mouse_button, const bool down);

		private:
			unsigned listen_flags = AllCallbacks;
			bool listening;
			bool loaded;
		};
	}
}