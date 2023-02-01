
#include "imgui/imgui.h"
#include "ImGuiImplPlatform.h"

#ifdef IMGUI_XBOX

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <tchar.h>
#include "GUIResourceManager.h"
#include "Common/Utility/OsAbstraction.h"
#include "Visual/Device/Texture.h"
#include "Visual/Device/Device.h"
#include "Visual/Renderer/RendererSubsystem.h"


// Win32 Data
static INT64                g_Time = 0;
static INT64                g_TicksPerSecond = 0;

// Functions
bool ImGui_Platform_Init(Device::WindowDX* window)
{
    if (!::QueryPerformanceFrequency((LARGE_INTEGER *)&g_TicksPerSecond))
        return false;
    if (!::QueryPerformanceCounter((LARGE_INTEGER *)&g_Time))
        return false;

    // Setup back-end capabilities flags
    ImGuiIO& io = ImGui::GetIO();
    io.BackendPlatformName = "imgui_impl_xbox";

    io.SetClipboardTextFn = nullptr;
    io.GetClipboardTextFn = nullptr;

	if (auto* device = Renderer::System::Get().GetDevice())
		io.DisplaySize = ImVec2((float)device->GetBackBufferWidth(), (float)device->GetBackBufferHeight());

    return true;
}

void ImGui_Platform_Shutdown() {}

void ImGui_Platform_NewFrame()
{
    ImGuiIO& io = ImGui::GetIO();
    IM_ASSERT(io.Fonts->IsBuilt() && "Font atlas not built! It is generally built by the renderer back-end. Missing call to renderer _NewFrame() function? e.g. ImGui_ImplOpenGL3_NewFrame().");

    // Setup time step
    INT64 current_time;
    ::QueryPerformanceCounter((LARGE_INTEGER *)&current_time);
    io.DeltaTime = (float)(current_time - g_Time) / g_TicksPerSecond;
    g_Time = current_time;
}

void ImGui_Platform_EnableDpiAwareness() { }
float ImGui_Platform_GetDpiScaleForMonitor(void* monitor) { return 1.0f; }
float ImGui_Platform_GetDpiScaleForHwnd(void* hwnd){ return 1.0f; }

bool ImGui_Platform_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	return Device::GUI::GUIResourceManager::GetGUIResourceManager()->MsgProc(hWnd, msg, wParam, lParam);
}

#endif