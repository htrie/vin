#pragma once

#include "D3DCommon/DXConfig.h"

#if defined(GGG_PC_ENABLE_DX12) || defined(GGG_XBOX_USE_DX12)
#define ENABLE_D3D12
#endif
#if defined(__APPLE__) || (defined(_WIN64) && !defined(_XBOX_ONE))
#define ENABLE_VULKAN
#endif
#if defined(WIN32) && !defined(GGG_XBOX_USE_DX12)
#define ENABLE_D3D11
#endif
#if defined(PS4)
#define ENABLE_GNMX
#endif
#define ENABLE_NULL