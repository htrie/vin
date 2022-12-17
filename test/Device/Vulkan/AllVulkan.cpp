
#if defined(WIN32)
	#define VK_USE_PLATFORM_WIN32_KHR
	#define VULKAN_HPP_ASSERT(x) 
	#include <vulkan/vulkan.hpp>
	#pragma comment(lib, "vulkan-1.lib")
	#if defined(ALLOW_DEBUG_LAYER)
		#pragma comment(lib, "VkLayer_utils.lib") // Delay loaded ('--debug-layer' requires the Vulkan SDK to be installed to access the validation layers dll).
	#endif
#elif defined(__APPLE__)
	#include <MoltenVK/mvk_vulkan.h>
	#include <MoltenVK/vk_mvk_moltenvk.h>
	#include <vulkan/vulkan.hpp>
#else
	#pragma error "Unsupported Vulkan platform"
#endif

#if defined(__APPLE__)
#include "Common/Bridging/Bridge.h"
#endif

#define VMA_VULKAN_VERSION 1000000
#define VMA_STATS_STRING_ENABLED 0
#define VMA_BIND_MEMORY2 0
#define VMA_DEDICATED_ALLOCATION 0
#include "vk_mem_alloc.h"

#include "Visual/Device/Win32/CompilerVulkan.h"

#include "UtilityVulkan.h"
#include "StateVulkan.h"
#include "AllocatorVulkan.h"
#include "DeviceVulkan.h"
#include "SwapChainVulkan.h"
#include "AllocatorVulkan.cpp"
#include "TransferVulkan.cpp"
#include "BufferVulkan.cpp"
#include "IndexBufferVulkan.cpp"
#include "VertexBufferVulkan.cpp"
#include "TextureVulkan.cpp"
#include "RenderTargetVulkan.cpp"
#include "SwapChainVulkan.cpp"
#include "SamplerVulkan.cpp"
#include "ShaderVulkan.cpp"
#include "ProfileVulkan.cpp"
#include "StateVulkan.cpp"
#include "DeviceVulkan.cpp"
