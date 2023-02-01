#include <variant>

#include "magic_enum/magic_enum.hpp"
#include "lodepng/lodepng.h"
#include "stb_image.h"
#include "stb_image_resize.h"
#include "stb_image_write.h"

#include "rr_dds.h"

#include "Common/Utility/MurmurHash2.h"
#include "Common/Utility/Profiler/ScopedProfiler.h"
#include "Common/Utility/ScopedTimer.h"
#include "Common/Utility/Logger/Logger.h"
#include "Common/Utility/StackList.h"
#include "Common/Utility/HighResTimer.h"
#include "Common/Utility/StringManipulation.h"
#include "Common/Utility/LoadingScreen.h"
#include "Common/File/File.h"
#include "Common/File/FileSystem.h"
#include "Common/File/PathHelper.h"
#include "Common/File/InputFileStream.h"
#include "Common/Resource/ResourceCache.h"
#include "Common/Job/JobSystem.h"

#include "ClientConfiguration/Config.h"
#include "ClientConfiguration/Settings.h"

#include "Visual/Utility/WindowsUtility.h"
#include "Visual/Utility/DXHelperFunctions.h"
#include "Visual/Profiler/JobProfile.h"
#include "Visual/Renderer/Utility.h"
#include "Visual/Renderer/ShaderCache.h"
#include "Visual/Renderer/RendererSettings.h"
#include "Visual/Renderer/CachedHLSLShader.h"
#include "Visual/Renderer/GlobalSamplersReader.h"
#include "Visual/GUI2/GUIResourceManager.h"

#include "Resource.h"
#include "Device.h"
#include "DeviceInfo.h"
#include "Constants.h"
#include "State.h"
#include "Buffer.h"
#include "VertexDeclaration.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "Shader.h"
#include "Texture.h"
#include "RenderTarget.h"
#include "Line.h"
#include "Font.h"
#include "Profile.h"
#include "SwapChain.h"
#include "Compiler.h"
#include "WindowDX.h"
#include "DynamicResolution.h"
#include "DynamicCulling.h"
#include "binktextures.h"
#include "Timer.h"
#include "DDS.h"
#include "PNG.h"
#include "KTX.h"
#include "JPG.h"
#include "ASTC.h"

#include "Internal/Allocator.h"
#include "Internal/Allocator.cpp"

#include "AllDefines.h"

#if defined(ENABLE_D3D11)
	#include "D3D11/AllD3D11.cpp"
#endif
#if defined(ENABLE_D3D12)
	#include "D3D12/AllD3D12.cpp"
#endif
#if defined(ENABLE_VULKAN)
	#include "Vulkan/AllVulkan.cpp"
#endif
#if defined(ENABLE_GNMX)
	#include "GNMX/AllGNMX.cpp"
#endif
#if defined(ENABLE_NULL)
	#include "Null/AllNull.cpp"
#endif

#include "VertexDeclaration.cpp"
#include "Buffer.cpp"
#include "IndexBuffer.cpp"
#include "VertexBuffer.cpp"
#include "Texture.cpp"
#include "RenderTarget.cpp"
#include "DeviceInfo.cpp"
#include "Device.cpp"
#include "Shader.cpp"
#include "State.cpp"
#include "SwapChain.cpp"
#include "Line.cpp"
#include "Font.cpp"
#include "Profile.cpp"
#include "Compiler.cpp"
#include "DynamicResolution.cpp"
#include "binktextures.cpp"
#include "Resource.cpp"
#include "Timer.cpp"
#include "KTX.cpp"
#include "DDS.cpp"
#include "JPG.cpp"