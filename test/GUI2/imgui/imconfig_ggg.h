#pragma once

#define IMGUI_INCLUDE_IMGUI_USER_H

#define IMGUI_DEFINE_MATH_OPERATORS
#ifndef GGG_CUSTOM
#	define GGG_CUSTOM
#endif

#define IMGUI_IMPL_WIN32_DISABLE_GAMEPAD
#define IMGUI_DISABLE_WIN32_DEFAULT_IME_FUNCTIONS
#define IMGUI_DISABLE_OBSOLETE_KEYIO

#if defined(_XBOX_ONE)
#define IMGUI_DISABLE_WIN32_DEFAULT_CLIPBOARD_FUNCTIONS   // [Win32] Don't implement default clipboard handler. Won't use and link with OpenClipboard/GetClipboardData/CloseClipboard etc.
#define IMGUI_DISABLE_WIN32_DEFAULT_IME_FUNCTIONS         // [Win32] Don't implement default IME handler. Won't use and link with ImmGetContext/ImmSetCompositionWindow.
#define IMGUI_DISABLE_WIN32_FUNCTIONS                     // [Win32] Won't use and link with any Win32 function.
#endif

#if defined(__APPLE__)
#define IMGUI_DISABLE_OSX_FUNCTIONS                       // [OSX] Won't use and link with any OSX function (clipboard).
#endif

#define IM_VEC2_CLASS_EXTRA                                                 \
        static ImVec2 Zero() { return ImVec2(0,0); }                        \
        bool IsZero() { return x == 0.0f && y == 0.0f; }                    \
        ImVec2(const simd::vector2& f) { x = f.x; y = f.y; }                \
        operator simd::vector2() const { return simd::vector2(x,y); }

#define IM_VEC4_CLASS_EXTRA                                                        \
        static ImVec4 Zero() { return ImVec4(0,0,0,0); }                           \
        ImVec4(const simd::vector4& f) { x = f.x; y = f.y; z = f.z; w = f.w; }     \
        operator simd::vector4() const { return simd::vector4(x,y,z,w); }

#define ImDrawIdx unsigned int

#include "assert.h" // Replace global assert with custom assert (allows us to disable it when generating bundles for instance).
#define IM_ASSERT(_EXPR) assert(_EXPR)

// Define custom callback function so we can pass around the command buffer
namespace Device 
{ 
    class CommandBuffer; 
    class Pass;
}

struct ImDrawList;
struct ImDrawCmd;

typedef bool (*ImDrawCallbackEx)(const ImDrawList* parent_list, const ImDrawCmd* cmd, Device::Pass* pass, Device::CommandBuffer* command_buffer);
#define ImDrawCallback ImDrawCallbackEx

/**
* @brief Sets the current rendering pass. The viewport will always match the passes first render target size. Use 
* ImDrawCallback_ResetRenderState to reset the current rendering pass back to default settings
* @param UserData should be a pointer to a valid Device::Pass, or nullptr to indicate to reset the pass back to default
*/
#define ImDrawCallback_SetPass     (ImDrawCallback)(-2)
