
#include "imgui/imgui.h"
#include "ImGuiImplPlatform.h"

#ifdef IMGUI_iphoneos

#include "GUIResourceManager.h"
#include "Visual/Device/Constants.h"
#include "Visual/Device/Device.h"
#include "Visual/Renderer/RendererSubsystem.h"

bool ImGui_Platform_Init( Device::WindowDX* window )
{
	// Setup back-end capabilities flags
	ImGuiIO& io = ImGui::GetIO();
	io.BackendPlatformName = "imgui_impl_iphoneos";

	io.SetClipboardTextFn = nullptr;
	io.GetClipboardTextFn = nullptr;

	Device::SurfaceDesc desc;
	if( SUCCEEDED( Renderer::System::Get().GetDevice()->GetBackBufferDesc( 0, 0, &desc ) ) )
		io.DisplaySize = ImVec2( ( float )desc.Width, ( float )desc.Height );

	return true;
}

void ImGui_Platform_Shutdown() {}

void ImGui_Platform_NewFrame()
{
	ImGuiIO& io = ImGui::GetIO();
	IM_ASSERT( io.Fonts->IsBuilt() && "Font atlas not built! It is generally built by the renderer back-end. Missing call to renderer _NewFrame() function? e.g. ImGui_ImplOpenGL3_NewFrame()." );

	io.DeltaTime = 1.f / 30.f; // Extreme jank
}

void  ImGui_Platform_EnableDpiAwareness() {}
float ImGui_Platform_GetDpiScaleForHwnd( void* hwnd ) { return 1.0f; }
float ImGui_Platform_GetDpiScaleForMonitor( void* monitor ) { return 1.0f; }

bool ImGui_Platform_WndProcHandler( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
	return Device::GUI::GUIResourceManager::GetGUIResourceManager()->MsgProc( hWnd, msg, wParam, lParam );
}

#endif