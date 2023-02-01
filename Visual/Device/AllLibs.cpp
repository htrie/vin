#include "lodepng/lodepng.cpp"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_MALLOC(sz)           Memory::Allocate(Memory::Tag::Texture, sz)
#define STBI_REALLOC(p,newsz)     Memory::Reallocate(Memory::Tag::Texture, p, newsz)
#define STBI_FREE(p)              Memory::Free(p)
#include "stb_image.h"
#include "stb_image_resize.h"
#include "stb_image_write.h"

#define RR_DDS_IMPLEMENTATION
#include "rr_dds.h"

#include "AllDefines.h"

#if defined(ENABLE_VULKAN)
	#define VMA_IMPLEMENTATION
	#define VMA_ASSERT(expr) assert(expr)
	#define VMA_VULKAN_VERSION 1000000
	#define VMA_STATS_STRING_ENABLED 0
	#define VMA_BIND_MEMORY2 0
	#define VMA_DEDICATED_ALLOCATION 0
	#include "Vulkan/vk_mem_alloc.h"
#endif

#if defined(ENABLE_D3D12)
	#if defined(_XBOX_ONE)
		#define D3D12MA_D3D12_HEADERS_ALREADY_INCLUDED
		#include <d3d12_x.h>
	#endif

	#define D3D12MA_ASSERT(expr) assert(expr)
	#include "D3D12/D3D12MemAlloc.cpp"
#endif
