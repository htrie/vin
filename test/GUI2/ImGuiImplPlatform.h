// dear imgui: Platform Binding for Windows (standard windows API for 32 and 64 bits applications)
// This needs to be used along with a Renderer (e.g. DirectX11, OpenGL3, Vulkan..)

// Implemented features:
//  [X] Platform: Clipboard support (for Win32 this is actually part of core imgui)
//  [X] Platform: Mouse cursor shape and visibility. Disable with 'io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange'.
//  [X] Platform: Keyboard arrays indexed using VK_* Virtual Key Codes, e.g. ImGui::IsKeyPressed(VK_SPACE).
//  [X] Platform: Gamepad support. Enabled with 'io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad'.
//  [X] Platform: Multi-viewport support (multiple windows). Enable with 'io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable'.

#pragma once

#include "Visual/Device/WindowDX.h"
#include "Visual/Device/Constants.h"

#if defined( PS4 )
#	define IMGUI_PS4
#elif defined(_XBOX_ONE)
#	define IMGUI_XBOX
#elif defined(__APPLE__macosx)
#	define IMGUI_APPLE
#elif defined(__APPLE__iphoneos)
#	define IMGUI_iphoneos
#elif defined(WIN32)
#	define IMGUI_WIN32
#endif

#if defined(IMGUI_PS4) || defined(IMGUI_XBOX) || defined(IMGUI_APPLE) || defined(IMGUI_WIN32) || defined(IMGUI_iphoneos)
#	define IMGUI_PLATFORM
#endif

#ifdef IMGUI_PLATFORM
IMGUI_IMPL_API bool     ImGui_Platform_Init(Device::WindowDX* window);
IMGUI_IMPL_API void     ImGui_Platform_Shutdown();
IMGUI_IMPL_API void     ImGui_Platform_NewFrame();

// DPI-related helpers (which run and compile without requiring 8.1 or 10, neither Windows version, neither associated SDK)
IMGUI_IMPL_API void     ImGui_Platform_EnableDpiAwareness();
IMGUI_IMPL_API float    ImGui_Platform_GetDpiScaleForHwnd(void* hwnd);       // HWND hwnd
IMGUI_IMPL_API float    ImGui_Platform_GetDpiScaleForMonitor(void* monitor); // HMONITOR monitor

IMGUI_IMPL_API bool     ImGui_Platform_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif
