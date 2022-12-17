
#include <thread>

#include "Common/Utility/Logger/Logger.h"
#include "Common/Job/JobSystem.h"

#include "Visual/Device/WindowDX.h"

#ifdef _XBOX_ONE

#include "Common/Console/Xbox/Common.h"

#include "Visual/Device/Texture.h"
#include "Visual/Device/DeviceInfo.h"
#include "Visual/Device/Device.h"
#include "Visual/Device/Timer.h"
#include "Visual/Device/State.h"
#include "Visual/Profiler/JobProfile.h"
#include "Visual/Device/WindowDX.h"

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
	assert( user_context_ptr != nullptr );
	const auto xbox_app = reinterpret_cast< Xbox::IApp* >( user_context_ptr );
	const auto dispatcher = Windows::UI::Core::CoreWindow::GetForCurrentThread()->Dispatcher;

	// Process events in another thread from now onwards.  This is necessary in order to immediately respond to the Suspending event, even if the main thread is frozen (e.g while loading.)
	std::thread process_events_thread( [ xbox_app, dispatcher ]()
	{
		while( xbox_app->IsRunning() )
		{
			{
				PROFILE_NAMED("Windows::UI::Core::CoreWindow::Dispatcher::ProcessEvents");

				dispatcher->ProcessEvents( Windows::UI::Core::CoreProcessEventsOption::ProcessAllIfPresent );
			}
			
			xbox_app->UpdateCachedGamepads();

			Sleep( 16 );
		}
	} );

	is_ready = true;

	while( xbox_app->IsRunning() )
	{
		if( !xbox_app->BeginFrame() )
		{
			Sleep( 16 );
			continue;
		}

		PROFILE_NAMED("Device::WindowDX::MainLoop");

		double fTime = device->GetTimer()->GetTime();
		double fElapsedTime = device->GetTimer()->GetElapsedTime();

		device->SetElapsedTime(fElapsedTime);
		device->SetUpTime(fTime);

		JOB_QUEUE_BEGIN(Job2::Profile::Swap);
		Engine::System::Get().Swap();
		JOB_QUEUE_END();

		JOB_QUEUE_BEGIN(Job2::Profile::DeviceBeginFrame);
		BeginFrame();
		JOB_QUEUE_END();

		const bool is_running = xbox_app->UpdateAndRender( device );

		JOB_QUEUE_BEGIN(Job2::Profile::DeviceEndFrame);
		EndFrame();
		JOB_QUEUE_END();

		if (is_running)
		{
			PROFILE_NAMED("Device::WindowDX::Present");
			
			JOB_QUEUE_BEGIN(Job2::Profile::DevicePresent);
			Present();
			JOB_QUEUE_END();
		}

		xbox_app->EndFrame();
	}

	process_events_thread.join();
}

HRESULT Device::WindowDX::GetClientRect(Rect* rect)
{
	return S_OK;
}

bool Device::WindowDX::RestoreDisplayResolution()
{
	return false;
}

bool Device::WindowDX::SetDisplayResolution(IDevice* device, int width, int height)
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
	return false;
}

SHORT Device::WindowDX::GetKeyState( int key )
{
	const auto xbox_app = reinterpret_cast< Xbox::IApp* >( user_context_ptr );
	return xbox_app != nullptr ? xbox_app->GetKeyState( key ) : 0;
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

}

HCURSOR Device::WindowDX::GetCursor()
{
	return NULL;
}

INT Device::WindowDX::ShowCursor(BOOL show)
{
	return 0;
}

void Device::WindowDX::SetCursorSettings(bool bShowCursorWhenFullScreen, bool bClipCursorWhenFullScreen, bool bClipCursorWhenWindowed, Rect cursor_clip_rect )
{

}

void Device::WindowDX::SetCursorClipStates(bool bClipCursorWhenFullScreen, bool bClipCursorWhenWindowed, Rect cursor_clip_rect )
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
	assert( pUserContext != nullptr );
	user_context_ptr = pUserContext;
	const auto xbox_app = reinterpret_cast< Xbox::IApp* >( user_context_ptr );
	xbox_app->SetFrameRenderCallback( func );
}

void Device::WindowDX::SetFrameMoveCallback(FrameMoveCallback func, void* pUserContext)
{
	assert( pUserContext != nullptr );
	user_context_ptr = pUserContext;
	const auto xbox_app = reinterpret_cast< Xbox::IApp* >( user_context_ptr );
	xbox_app->SetFrameMoveCallback( func );
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
	return false;
}

HRESULT Device::WindowDX::SetRect(Rect* rect, LONG x, LONG y, LONG width, LONG Height)
{
	return E_FAIL;
}

bool Device::WindowDX::AdjustWindowRect(Rect* rect, BOOL bMenu)
{
	return false;
}

bool Device::WindowDX::IsPointInRect(const Rect* rect, const Point& point)
{
	return false;
}

LRESULT Device::WindowDX::WindowDXProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	return 0;
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
	return 0;
}

LRESULT Device::WindowDX::PostMessage( HWND hwnd, UINT Msg, WPARAM wParam, LPARAM lParam )
{
	return 0;
}

INT Device::WindowDX::MessageBox( LPCSTR text, LPCSTR caption, UINT )
{
	LOG_DEBUG( "MessageBox: " << text << ", " << caption );
	return 0;
}

INT Device::WindowDX::MessageBox( LPCWSTR text, LPCWSTR caption, UINT )
{
	LOG_DEBUG( L"MessageBox: " << text << L", " << caption );
	return 0;
}

void Device::WindowDX::TakeScreenshot( const std::wstring& screenshot_path )
{
}

#endif