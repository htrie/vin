#include "Common/Utility/HighResTimer.h"
#include "Common/Engine/Status.h"
#include "Common/Utility/Logger/Logger.h"

#include "Visual/Device/Device.h"
#include "Visual/Device/DeviceInfo.h"
#include "Visual/Device/DynamicResolution.h"
#include "Visual/Device/DynamicCulling.h"
#include "Visual/GUI2/GUIResourceManager.h"
#include "Visual/Engine/EngineSystem.h"

#include "WindowDX.h"

Device::WindowDX::WindowDX(const WCHAR* strClassName, const WCHAR* strWindowTitle, HINSTANCE hInstance, HICON hIcon, HMENU hMenu, int x, int y, int width, int height)
	: hInstance(hInstance)
	, device_info(DeviceInfo::Create())
{
	assert(instance == nullptr);
	instance = this;

#if !defined( CONSOLE ) && !defined( __APPLE__ ) && !defined( ANDROID )
	CreateWindowHandler(strClassName, strWindowTitle, hInstance, hIcon, hMenu, x, y, width, height);
#endif
}

Device::WindowDX::~WindowDX()
{
	if( instance == this )
		instance = nullptr;
}

/* methods exposed to Xbox one*/

void Device::WindowDX::SetDevice(Device::IDevice* new_device)
{
	if (new_device)
		new_device->Pause(true, true);

	device = new_device;

	if (device)
		device->Pause(false, false);
}


std::wstring Device::WindowDX::CreateRelativePath(const std::wstring& file)
{
	const size_t pos = file.rfind(L"Client");

	if (pos != std::wstring::npos)
	{
		return file.substr(pos + 7);
	}

	return file;
}

void Device::WindowDX::TriggerDeviceCreate(IDevice* device)
{
	if (on_device_created_callback)
	{
		LOG_DEBUG(L"[WINDOW] TrigggerDeviceCreate");
		on_device_created_callback(device, user_context_ptr);
#if !defined( CONSOLE ) && !defined( __APPLE__ ) && !defined( ANDROID )
		SetupCursor();
#endif
	}
}

void Device::WindowDX::TriggerDeviceReset(IDevice* device, const SurfaceDesc* desc)
{
	if (on_device_reset_callback)
	{
		LOG_DEBUG(L"[WINDOW] TrigggerDeviceReset");
		on_device_reset_callback(device, desc, user_context_ptr);
#if !defined( CONSOLE ) && !defined( __APPLE__ ) && !defined( ANDROID )
		SetupCursor();
#endif
	}
}

void Device::WindowDX::TriggerDeviceLost()
{
	if (on_device_lost_callback)
	{
		LOG_DEBUG(L"[WINDOW] TrigggerDeviceLost");
		on_device_lost_callback(user_context_ptr);
	}
}

void Device::WindowDX::TriggerDeviceDestroyed()
{
	if (on_device_destroyed_callback)
	{
		LOG_DEBUG(L"[WINDOW] TrigggerDeviceDestroyed");
		on_device_destroyed_callback(user_context_ptr);
	}
}

Device::DeviceInfo* Device::WindowDX::GetInfo()
{
	return device_info.get();
}

/* Static methods */

Device::WindowDX* Device::WindowDX::GetWindow()
{
	return instance;
}

LRESULT Device::WindowDX::SendMessageFocused( UINT msg, WPARAM wparam, LPARAM lparam )
{
	HWND hwnd = nullptr;
	const auto window = GetWindow();
	if( window != nullptr )
		hwnd = window->GetWnd();
	return ::Device::WindowDX::SendMessage( hwnd, msg, wparam, lparam );
}

LRESULT Device::WindowDX::PostMessageFocused( UINT msg, WPARAM wparam, LPARAM lparam )
{
	HWND hwnd = nullptr;
	const auto window = GetWindow();
	if( window != nullptr )
		hwnd = window->GetWnd();
	return ::Device::WindowDX::PostMessage( hwnd, msg, wparam, lparam );
}

void Device::WindowDX::SleepUntilNextFrame()
{
	last_framerate_limit_wait = 0;
	const bool is_foreground = IsForeground();
	if ( !device || ( !is_foreground && !device->GetBackgroundFramerateLimitEnabled() ) )
		return;

	if ( is_foreground && ( !device->GetFramerateLimitEnabled() || device->GetVSync() ) )
		return;

	const auto framerate_limit = is_foreground ? device->GetFramerateLimit() : device->GetBackgroundFramerateLimit();
	if ( framerate_limit == 0 )
		return;

	const auto duration = 1000000 / static_cast<long long>(framerate_limit);
	const auto frame_end_time = frame_start_time + duration;
	unsigned long long remaining_time = 0;

	const auto start = HighResTimer::Get().GetTimeUs();

	do 
	{
		const auto current_time = static_cast<unsigned long long>(HighResTimer::Get().GetTimeUs());
		remaining_time = current_time <	frame_end_time ? frame_end_time - current_time : 0;

		if (is_foreground && (remaining_time < 2000))
		{
			/*Do not yield, macOS doesn't like it (low/high efficiency core scheduling issues)*/
			// Exact timing.
		}
		else
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1)); // Wildly imprecise.
		}
	}
	while (remaining_time > 0);

	last_framerate_limit_wait = float(static_cast<long double>(HighResTimer::Get().GetTimeUs() - start) / 1000000.0);
}

void Device::WindowDX::BeginFrame()
{
	device->BeginFrame();
	device->GetProfile()->BeginFrame();
}

void Device::WindowDX::EndFrame()
{
	device->GetProfile()->EndFrame();
	device->EndFrame();
}

LRESULT Device::WindowDX::WindowMessageProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, bool* nofurtherprocessing, void* context)
{
	LRESULT retval = 0;

#if DEBUG_GUI_ENABLED
	if (is_ready && Device::GUI::GUIResourceManager::GetGUIResourceManager()->MainMsgProc(hWnd, message, wParam, lParam))
	{
		*nofurtherprocessing = true;
		return retval;
	}
#endif

	if (msg_proc_callback)
		retval = msg_proc_callback(hWnd, message, wParam, lParam, nofurtherprocessing, context);

	return retval;
}

void Device::WindowDX::SetMsgProcCallback(MsgProcCallback func, void* pUserContext)
{
	user_context_ptr = pUserContext;
	msg_proc_callback = func;
}

void Device::WindowDX::SetFullscreenHackCallback(FullscreenHackCallback func)
{
	fullscreen_hack_callback = func;
}

void Device::WindowDX::Present()
{
	float present_wait_time = 0;
	{
		Timer timer;
		timer.Start();

		device->Submit();

	#if DEBUG_GUI_ENABLED
		Device::GUI::GUIResourceManager::GetGUIResourceManager()->Present();
	#endif

		device->Present(NULL, NULL, 0);

		present_wait_time = (float)timer.GetElapsedTime();
	}

	device->GetProfile()->Collect();

#if defined(PROFILING)
	stats.cpu_frame_time = (float)device->GetElapsedTime();
	stats.gpu_frame_time = device->GetProfile()->DtFrame();
	stats.device_wait_time = device->GetFrameWaitTime();
	stats.present_wait_time = present_wait_time;
	stats.limit_wait_time = last_framerate_limit_wait;
#endif

	const bool is_foreground = IsForeground();
	const auto framerate_limit = (is_foreground ? device->GetFramerateLimitEnabled() : device->GetBackgroundFramerateLimitEnabled()) ? (is_foreground ? device->GetFramerateLimit() : device->GetBackgroundFramerateLimit()) : 0;

	const float cpu_time = (float)(device->GetElapsedTime() - (device->GetFrameWaitTime() + present_wait_time + last_framerate_limit_wait));
	const float gpu_time = device->GetProfile()->DtFrame();
	const float elapsed_time = (float)device->GetElapsedTime();

#if defined(__APPLE__iphoneos)
	const float dyn_res_cpu_time = frame_time;
	const float dyn_res_gpu_time = frame_time; // We don't have access to GPU counters yet, so we use CPU frame time instead.
#elif defined(_XBOX_ONE)
	const float dyn_res_cpu_time = cpu_time;
	const float dyn_res_gpu_time = gpu_time;
#else
	const float dyn_res_cpu_time = elapsed_time;
	const float dyn_res_gpu_time = gpu_time;
#endif

	DynamicResolution::Get().Determine(dyn_res_cpu_time, dyn_res_gpu_time, device->GetProfile()->GetCollectDelay(), framerate_limit);
	DynamicCulling::Get().Update(cpu_time, gpu_time, elapsed_time, framerate_limit);
	Engine::Status::Get().FrameMove(cpu_time, gpu_time, elapsed_time);

	device->Swap();
}

Device::WindowDX::Stats Device::WindowDX::GetStats() const
{
	return stats;
}


#if defined(__APPLE__)
    #include "Apple/WindowDXApple.cpp"
#elif defined(ANDROID)
    #include "Android/WindowDXAndroid.cpp"
#elif defined(_XBOX_ONE)
	#include "Durango/WindowDXXboxOne.cpp"
#elif defined(PS4)
	#include "PS4/WindowDXPS4.cpp"
#elif defined(WIN32)
	#include "Win32/WindowDXWin32.cpp"
#endif
