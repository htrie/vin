#pragma once

#include <functional>
#include <memory>

#include "Visual/Device/Constants.h"

namespace Device
{
	class IDevice;
	class DeviceInfo;
	struct MonitorInfo;
	struct Rect;
	struct Point;
	enum class Type : unsigned;

	class WindowDX
	{
	public:
		typedef HRESULT(CALLBACK *DeviceCreatedCallback)(IDevice* pd3dDevice, void* pUserContext);
		typedef HRESULT(CALLBACK *DeviceResetCallback)(IDevice* pd3dDevice, void* pUserContext);
		typedef void    (CALLBACK *DeviceLostCallback)(void* pUserContext);
		typedef void    (CALLBACK *DeviceDestroyedCallback)(void* pUserContext);

		typedef HRESULT(CALLBACK *ToggleFullscreenCallback)(bool fullscreen);
		typedef void	(CALLBACK *WindowSizeChangedCallback)();

		typedef void    (CALLBACK *FrameRenderCallback)(IDevice* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext);
		typedef void    (CALLBACK *FrameMoveCallback)(double fTime, float fElapsedTime, void* pUserContext);
		typedef LRESULT(CALLBACK *MsgProcCallback)(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing, void* pUserContext);
		typedef void (CALLBACK* FullscreenHackCallback)(WindowDX* window);

		struct Stats
		{
			float cpu_frame_time = 0.0f;
			float gpu_frame_time = 0.0f;
			float device_wait_time = 0.0f;
			float present_wait_time = 0.0f;
			float limit_wait_time = 0.0f;
		};

	protected:
		HWND hwnd = nullptr;
		HINSTANCE hInstance = nullptr;
		bool in_size_move = false;					// if true, app is inside a WM_ENTERSIZEMOVE
		bool minimized = false;						// if true, the HWND is minimized
		bool maximized = false;						// if true, the HWND is maximized
		bool minimized_while_fullscreen = false;	// if true, the HWND is minimized while it was in fullscreen
		int default_width = 0;
		int default_height = 0;
		float last_framerate_limit_wait = 0;
	#if defined(__APPLE__iphoneos)
		float frame_time = 0.0f;
	#endif
		bool device_lost = false;
		bool is_active = true;
		bool is_ready = false;
		bool update_window_cursor_clipping = true;	// if true, we need to update the clipping rectangle of the cursor for the window
		bool skip_resize_message = false;
		bool size_changed = false;
		int current_resolution_width = 0;
		int current_resolution_height = 0;
		bool cursor_visible = true;
		std::wstring current_resolution_adapter_name;
		bool is_tool_window = false;

		std::wstring commandLineArguments;
		std::wstring extraCommandLineArguments;

		Device::IDevice* device = nullptr; // [TODO] Move to renderer system.

		//call backs
		ToggleFullscreenCallback toggle_fullscreen_callback = nullptr;
		WindowSizeChangedCallback window_size_changed_callback = nullptr;
		FrameMoveCallback frame_move_callback = nullptr;
		FrameRenderCallback frame_render_callback = nullptr;
		MsgProcCallback msg_proc_callback = nullptr;
		FullscreenHackCallback fullscreen_hack_callback = nullptr;

		//OnDevice* callbacks
		DeviceCreatedCallback on_device_created_callback = nullptr;
		DeviceResetCallback on_device_reset_callback = nullptr;
		DeviceLostCallback on_device_lost_callback = nullptr;
		DeviceDestroyedCallback on_device_destroyed_callback = nullptr;

		void* user_context_ptr = nullptr;

		std::unique_ptr<DeviceInfo > device_info; // [TODO] Move to renderer system.

		bool clip_cursor_fullscreen = false;
		bool clip_cursor_windowed = false;
		Rect clip_cursor_rect = {};
		bool ignore_size_changed = false;
		unsigned back_buffer_width_at_mode_change;
		unsigned back_buffer_height_at_mode_change;
		LONG window_style_at_mode_change;
		bool top_most_while_windowed;
		HMONITOR adapter_monitor = nullptr;
		bool clip_window_to_single_adapter = true;
		bool allow_keys;
		bool allow_shortcut_keys_windowed = true;
		bool allow_shortcut_keys_fullscreen = true;
		HHOOK keyboard_hook = nullptr; 

#ifndef CONSOLE
		WINDOWPLACEMENT windowed_placement;
		tagSTICKYKEYS sticky_keys;
		tagTOGGLEKEYS toggle_keys;
		tagFILTERKEYS filter_keys;

		void SetupCursor();
		void AllowShortcutKeys(bool allowkeys);
		void SetShortcutKeySettings(const bool& allow_when_fullscreen, const bool& allow_when_windowed);

		void CreateWindowHandler(const WCHAR* strClassName, const WCHAR* strWindowTitle, HINSTANCE hInstance, HICON hIcon, HMENU hMenu, int x, int y, int width, int height);
#endif

		void Present();
		void BeginFrame();
		void EndFrame();

	public:
		WindowDX(const WCHAR* strClassName, const WCHAR* strWindowTitle = L"Direct3D Window", HINSTANCE hInstance = nullptr, HICON hIcon = nullptr, HMENU hMenu = nullptr, int x = CW_USEDEFAULT, int y = CW_USEDEFAULT, int width = 800, int height = 600);
		~WindowDX();

		Stats GetStats() const;

		void MainLoop();
		bool ShowWindow(unsigned showWindowCommand);

		void* GetUserContextPtr() { return user_context_ptr; }

		//WIN32 API related call
		HWND GetWnd() const { return hwnd; }
		HINSTANCE GetInstance() const { return hInstance; }
		LONG GetWindowStyle() ;
		void SetWindowStyle(LONG new_style);
		BOOL FlashWindow( UINT count, bool tray_icon_only = true ); // set count to 0 to flash until the window comes into focus
		void StopWindowFlash();

		UINT GetCaretBlinkTime() ;

		HRESULT GetWindowRect(Rect* rect) ;
		HRESULT GetClientRect(Rect* rect);
		bool FetchMonitorInfo(DWORD dwFlags, MonitorInfo* monitorInfo);
		HRESULT ToggleFullScreen();

		bool GetMonitorDimensions( unsigned int adapter_index, unsigned int* out_width, unsigned int* out_height );

		bool IsForeground() ;
		bool SetForegroundWindow() ;

		bool IsActive() ;
		bool SetWindowPos(int x, int y, int cx, int cy, UINT uFlags, bool fullscreen, int client_width, int client_height) ;
		HRESULT SetRect(Rect* rect, LONG x, LONG y, LONG width, LONG Height) ;
		bool AdjustWindowRect(Rect* rect, BOOL bMenu) ;
		bool IsPointInRect(const Rect* rect, const Point& point) ;
		LRESULT WindowDXProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) ;

		//device accessor
		void SetDevice(Device::IDevice* new_device);

		bool GetDeviceLost() { return device_lost; }
		void SetDeviceLost(const bool& val) { device_lost = val; }

		//triggering callbacks
		void TriggerDeviceCreate(IDevice* device);
		void TriggerDeviceReset(IDevice* device);
		void TriggerDeviceLost();
		void TriggerDeviceDestroyed();

		//input related
		SHORT GetKeyState(int nVirtKey) ;
		UINT MapVirtualKey(UINT uCode, UINT uMapType) ;
		int GetKeyNameText(LONG lParam, LPWSTR lpString, int cchSize) ;
		HWND SetCapture() ;
		HRESULT ReleaseCapture() ;

		//mouse/cursor related
		HRESULT GetCursorPos(Point* point) ;
		HRESULT SetCursorPos(INT x, INT y) ;
		void SetCursor(HCURSOR hCursor) ;
		HCURSOR GetCursor() ;
		INT ShowCursor(BOOL show) ;
		bool GetCursorVisible() { return cursor_visible; }
		void SetCursorSettings(bool bShowCursorWhenFullScreen, bool bClipCursorWhenFullScreen, bool bClipCursorWhenWindowed, Rect cursor_clip_rect );
		void SetCursorClipStates(bool bClipCursorWhenFullScreen, bool bClipCursorWhenWindowed, Rect cursor_clip_rect);
		bool SetDisplayResolution(IDevice* device, int width, int height);
		bool RestoreDisplayResolution();

		//file system
		bool PathFileExists(LPCWSTR path) ;
		std::wstring CreateRelativePath(const std::wstring& file);
		std::wstring GetFilename(const wchar_t* const deftype, const wchar_t* const filter, const bool file_must_exist);

		//set callbacks
		void SetOnDeviceCreateCallback(DeviceCreatedCallback func, void* pUserContext = nullptr);
		void SetOnDeviceResetCallback(DeviceResetCallback func, void* pUserContext = nullptr);
		void SetOnDeviceLostCallback(DeviceLostCallback func, void* pUserContext = nullptr);
		void SetOnDeviceDestroyCallback(DeviceDestroyedCallback func, void* pUserContext = nullptr);

		void SetToggleFullscreenCallback(ToggleFullscreenCallback func, void* pUserContext = nullptr);
		void SetMsgProcCallback(MsgProcCallback func, void* pUserContext = nullptr);
		void SetFullscreenHackCallback(FullscreenHackCallback func);
		void SetFrameRenderCallback(FrameRenderCallback func, void* pUserContext = nullptr);
		void SetFrameMoveCallback(FrameMoveCallback func, void* pUserContext = nullptr);
		void SetWindowSizeChangedCallback(WindowSizeChangedCallback func, void* pUserContext);

		//Get/Set window states
		void SetInSizeMove(bool bInSizeMove) { in_size_move = bInSizeMove; }
		bool IsInSizeMove() const { return in_size_move; }
		void SetMinimized(bool bMinimized) { minimized = bMinimized; }
		bool IsMinimized() const { return minimized; }
		void SetMaximized(bool bMaximized) { maximized = bMaximized; }
		bool IsMaximized() const { return maximized; }
		void SetMinimizedInFullscreen(bool bMinimizedInFullscreen) { minimized_while_fullscreen = bMinimizedInFullscreen; }
		bool IsMinimizedInFullscreen() const { return minimized_while_fullscreen; }
		void SkipOnSizeMessage( bool skip ) { skip_resize_message = skip; }

		void SetToolWindow( const bool bIsTool ){ is_tool_window = bIsTool; }

		MsgProcCallback GetMsgProcCallBack() { return msg_proc_callback; }

		DeviceInfo* GetInfo();
		
		LRESULT WindowMessageProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, bool* nofurtherprocessing, void* context);

#ifndef CONSOLE
		HHOOK GetKeyboardHook() { return keyboard_hook; }
		bool GetAllowedShortcutKeys() { return allow_keys; }

		void SetCurrentMonitor(HMONITOR monitor) { adapter_monitor = monitor; }
		HMONITOR GetCurrentMonitor() { return adapter_monitor; }
#endif

		void SleepUntilNextFrame();

		void TakeScreenshot( const std::wstring& screenshot_path );

	private:
		static inline WindowDX* instance = nullptr;			 // weak ptr of the window instance

		unsigned long long frame_start_time = 0;

		Stats stats;

	public:
		static WindowDX* GetWindow();
		static HWND GetForegroundWindow();
		static HRESULT GetClientRect(HWND hwd, Rect* rect);

		//message box
		static LRESULT SendMessage( HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam );
		static LRESULT PostMessage( HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam );
		static LRESULT SendMessageFocused( UINT msg, WPARAM wparam, LPARAM lparam );
		static LRESULT PostMessageFocused( UINT msg, WPARAM wparam, LPARAM lparam );
		static INT MessageBox(LPCSTR lpText, LPCSTR lpCaption, UINT uType);
		static INT MessageBox(LPCWSTR lpText, LPCWSTR lpCaption, UINT uType);
	};
}
