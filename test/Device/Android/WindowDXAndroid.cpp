#include "Common/Utility/Logger/Logger.h"
#include "Common/Utility/HighResTimer.h"
#include "Common/Utility/StringManipulation.h"
#include "Common/Job/JobSystem.h"

#include "Visual/Device/WindowDX.h"
#include "Visual/Device/Texture.h"
#include "Visual/Device/DeviceInfo.h"
#include "Visual/Device/Device.h"
#include "Visual/Device/Timer.h"
#include "Visual/Profiler/JobProfile.h"

#if defined(ANDROID)
#include "Common/Bridging/Android.h"
#endif

namespace
{
	size_t cursor_id = 0;
}

bool Device::WindowDX::ShowWindow(unsigned showWindowCommand)
{
	return true;
}

bool Device::WindowDX::FetchMonitorInfo(DWORD dwFlags, MonitorInfo* monitorInfo)
{
	return true;
}

void Device::WindowDX::MainLoop()
{
	is_ready = true;

	double fTime = device->GetTimer()->GetTime();
	double fElapsedTime = device->GetTimer()->GetElapsedTime();

	device->SetElapsedTime(fElapsedTime);
	device->SetUpTime(fTime);

	frame_start_time = HighResTimer::Get().GetTimeUs();

	JOB_QUEUE_BEGIN(Job2::Profile::Swap);
	Engine::System::Get().Swap();
	JOB_QUEUE_END();

	frame_move_callback(fTime, fElapsedTime, user_context_ptr);

	JOB_QUEUE_BEGIN(Job2::Profile::DeviceBeginFrame);
	BeginFrame();
	JOB_QUEUE_END();

	frame_render_callback(device, fTime, fElapsedTime, user_context_ptr);

	JOB_QUEUE_BEGIN(Job2::Profile::DeviceEndFrame);
	EndFrame();
	JOB_QUEUE_END();

	JOB_QUEUE_BEGIN(Job2::Profile::DevicePresent);
	Present();
	JOB_QUEUE_END();

	JOB_QUEUE_BEGIN( Job2::Profile::Limiter );
	SleepUntilNextFrame();
	JOB_QUEUE_END();

	if (size_changed) // Modify renderer settings one frame later since vulkan resize is deferred.
	{
		size_changed = false;

		if (window_size_changed_callback)
			window_size_changed_callback();
	}
}

HRESULT Device::WindowDX::GetClientRect(Rect* rect)
{
	return S_OK;
}

bool Device::WindowDX::SetDisplayResolution(IDevice* device, int width, int height)
{
	return true;
}

bool Device::WindowDX::RestoreDisplayResolution()
{
	return false;
}

LONG Device::WindowDX::GetWindowStyle()
{
	return 0;
}

HRESULT Device::WindowDX::ToggleFullScreen()
{
	return S_OK;
}

void Device::WindowDX::SetWindowStyle(LONG new_style)
{

}

BOOL Device::WindowDX::FlashWindow( UINT count, bool tray_icon_only )
{
	return FALSE;
}

void Device::WindowDX::StopWindowFlash()
{

}

UINT Device::WindowDX::GetCaretBlinkTime()
{
	return 530;
}

HRESULT Device::WindowDX::GetWindowRect(Rect* rect)
{
	return E_FAIL;
}

bool Device::WindowDX::GetMonitorDimensions( unsigned int adapter_index, unsigned int* out_width, unsigned int* out_height )
{
	return E_FAIL;
}

SHORT Device::WindowDX::GetKeyState( int key )
{
    return 0;
}

UINT Device::WindowDX::MapVirtualKey(UINT uCode, UINT uMapType)
{
	return 0;
}

int Device::WindowDX::GetKeyNameText(LONG lParam, LPWSTR lpString, int cchSize)
{
	return 0;
}

HWND Device::WindowDX::SetCapture()
{
	return nullptr;
}

HRESULT Device::WindowDX::ReleaseCapture()
{
	return S_FALSE;
}

HRESULT Device::WindowDX::GetCursorPos(Point* point)
{
	return 0;
}

HRESULT Device::WindowDX::SetCursorPos(INT x, INT y)
{
	return 0;
}

void Device::WindowDX::SetCursor(HCURSOR hCursor)
{
	cursor_id = (size_t)hCursor;
}

HCURSOR Device::WindowDX::GetCursor()
{
	return (HCURSOR)cursor_id;
}

INT Device::WindowDX::ShowCursor(BOOL show)
{
	return 0;
}

void Device::WindowDX::SetCursorSettings(bool bShowCursorWhenFullScreen, bool bClipCursorWhenFullScreen, bool bClipCursorWhenWindowed, Rect cursor_clip_rect )
{

}

void Device::WindowDX::SetCursorClipStates(bool bClipCursorWhenFullScreen, bool bClipCursorWhenWindowed, Rect cursor_clip_rect)
{

}

bool Device::WindowDX::PathFileExists(LPCWSTR path)
{
	std::string filename = WstringToString(path);
	FILE *fp = fopen(filename.c_str(), "r");
	if (fp != NULL)
		fclose(fp);

	return (fp != NULL);
}

std::wstring Device::WindowDX::GetFilename(const wchar_t* const deftype, const wchar_t* const filter, const bool file_must_exist)
{
	return std::wstring();
}

void Device::WindowDX::SetOnDeviceCreateCallback(DeviceCreatedCallback func, void* pUserContext)
{
	user_context_ptr = pUserContext;
	on_device_created_callback = func;
}

void Device::WindowDX::SetOnDeviceResetCallback(DeviceResetCallback func, void* pUserContext)
{
	user_context_ptr = pUserContext;
	on_device_reset_callback = func;
}

void Device::WindowDX::SetOnDeviceLostCallback(DeviceLostCallback func, void* pUserContext)
{
	user_context_ptr = pUserContext;
	on_device_lost_callback = func;
}

void Device::WindowDX::SetOnDeviceDestroyCallback(DeviceDestroyedCallback func, void* pUserContext)
{
	user_context_ptr = pUserContext;
	on_device_destroyed_callback = func;
}

void Device::WindowDX::SetToggleFullscreenCallback(ToggleFullscreenCallback func, void* pUserContext)
{
	user_context_ptr = pUserContext;
	toggle_fullscreen_callback = func;
}

void Device::WindowDX::SetWindowSizeChangedCallback(WindowSizeChangedCallback func, void* pUserContext)
{
	user_context_ptr = pUserContext;
	window_size_changed_callback = func;
}

void Device::WindowDX::SetFrameRenderCallback(FrameRenderCallback func, void* pUserContext)
{
	user_context_ptr = pUserContext;
	frame_render_callback = func;
}

void Device::WindowDX::SetFrameMoveCallback(FrameMoveCallback func, void* pUserContext)
{
	user_context_ptr = pUserContext;
	frame_move_callback = func;
}

bool Device::WindowDX::IsActive()
{
	return true;
}

bool Device::WindowDX::IsForeground()
{
	return true;
}

bool Device::WindowDX::SetForegroundWindow()
{
	return FALSE;
}

bool Device::WindowDX::SetWindowPos(int x, int y, int cx, int cy, UINT uFlags, bool fullscreen, int client_width, int client_height)
{
	return true;
}

HRESULT Device::WindowDX::SetRect(Rect* rect, LONG x, LONG y, LONG width, LONG Height)
{
	return E_FAIL;
}

bool Device::WindowDX::AdjustWindowRect(Rect* rect, BOOL bMenu)
{
	return E_FAIL;
}

bool Device::WindowDX::IsPointInRect(const Rect* rect, const Point& point)
{
	return false;
}

LRESULT Device::WindowDX::WindowDXProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		case WM_SIZE:
		{
			if (device)
			{
				const auto width = (unsigned)wParam;
				const auto height = (unsigned)lParam;
				
				device->ResizeDXGIBuffers(width, height, false);
				
				size_changed = true;
			}
			break;
		}
	}
	
	bool no_further_processing = false;
	return WindowMessageProc(hWnd, message, wParam, lParam, &no_further_processing, nullptr);
}

HWND Device::WindowDX::GetForegroundWindow()
{
	return nullptr;
}

/* Static methods */

HRESULT Device::WindowDX::GetClientRect(HWND hwd, Rect* rect)
{
	return S_OK;
}

/* Static send message functions in WINAPI */

LRESULT Device::WindowDX::SendMessage( HWND hwnd, UINT Msg, WPARAM wParam, LPARAM lParam )
{
	LOG_DEBUG( "MessageBox: " << Msg << ", " << wParam << ":" << lParam );
    return 0;
}

LRESULT Device::WindowDX::PostMessage( HWND hwnd, UINT Msg, WPARAM wParam, LPARAM lParam )
{
    return SendMessage( hwnd, Msg, wParam, lParam );
}

INT Device::WindowDX::MessageBox( LPCSTR text, LPCSTR caption, UINT )
{
	LOG_DEBUG( "MessageBox: " << text << ", " << caption );
	Bridging::AndroidMessageBox(text, false);
	return 0;
}

INT Device::WindowDX::MessageBox( LPCWSTR text, LPCWSTR caption, UINT )
{
	LOG_DEBUG( L"MessageBox: " << text << L", " << caption );
	Bridging::AndroidMessageBox(WstringToString(text).c_str(), false);
	return 0;
}

void Device::WindowDX::TakeScreenshot( const std::wstring& screenshot_path )
{
}
