
#include "imgui/imgui.h"
#include "ImGuiImplPlatform.h"

#ifdef IMGUI_PS4

#include "GUIResourceManager.h"
#include "Visual/Device/Constants.h"
#include "Visual/Device/Device.h"
#include "Visual/Renderer/RendererSubsystem.h"

#include <kernel.h>

static long long g_frequency = 0;
static long long g_time = 0;

static void ImGui_Platform_InitPlatformInterface();
static void ImGui_Platform_ShutdownPlatformInterface();

bool ImGui_Platform_Init(Device::WindowDX* window)
{
	g_frequency = sceKernelGetProcessTimeCounterFrequency();
	g_time = sceKernelGetProcessTimeCounter();

	// Setup back-end capabilities flags
	ImGuiIO& io = ImGui::GetIO();
	io.BackendPlatformName = "imgui_impl_ps4";

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

	long long current_time = sceKernelGetProcessTimeCounter();
	io.DeltaTime = (float)(current_time - g_time) / g_frequency;
	g_time = current_time;
}

void  ImGui_Platform_EnableDpiAwareness() {}
float ImGui_Platform_GetDpiScaleForHwnd(void* hwnd) { return 1.0f; }
float ImGui_Platform_GetDpiScaleForMonitor(void* monitor) { return 1.0f; }

bool ImGui_Platform_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	return Device::GUI::GUIResourceManager::GetGUIResourceManager()->MsgProc(hWnd, msg, wParam, lParam);
}

#endif