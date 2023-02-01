
#define XSF_USE_DX_11_1

#if defined(_XBOX_ONE)
	#if defined(_TITLE)
		#if defined(_RELEASE)
			#define D3DCOMPILE_NO_DEBUG 1
		#endif
		#include <d3d11_x.h>
		#include <d3dcompiler_x.h>
		#include <xg.h>
	#else
		#include <d3dcompiler.h>
		#include <d3d11_1.h>
		#include <dxgi1_2.h>
	#endif
#else
	#include <d3d11_1.h>
	#include <d3dcommon.h>
	#include <dxgi.h>
	#include <d3dcompiler.h>
#endif

#if defined(_XBOX_ONE)
#pragma comment( lib, "d3d11_x.lib" )
#else
#pragma comment( lib, "dxgi.lib" )
#pragma comment( lib, "d3d11.lib" )
#endif

#if !defined(NO_D3D11_DEBUG_NAME) && ( defined(_DEBUG) || defined(PROFILE) )
#if !defined(_XBOX_ONE) || !defined(_TITLE)
#pragma comment(lib,"dxguid.lib")
#endif
#endif

#if defined(_XBOX_ONE)
#include <xgmemory.h>
#endif

#ifdef _WIN32
#include "wincodec.h"
#endif

#include <DirectXMath.h>
#include <DirectXPackedVector.h>

#include "../D3DCommon/DirectXTex.h"

#ifndef SAFE_DELETE_ARRAY
#define SAFE_DELETE_ARRAY(p) { if(p) { delete[] (p);   (p)=NULL; } }
#endif 
#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)      { if(p) { (p)->Release(); (p)=NULL; } }
#endif

#if !defined(_XBOX_ONE)
	#ifdef IID_GRAPHICS_PPV_ARGS
		#undef IID_GRAPHICS_PPV_ARGS
	#endif
	#define IID_GRAPHICS_PPV_ARGS(ppType) __uuidof(**(ppType)), (reinterpret_cast<void**>(ppType))
#endif

struct ExceptionD3D11 : public std::runtime_error
{
	std::string OptionalDeviceError(ID3D11Device* device)
	{
		if (device)
		{
			const auto hr = device->GetDeviceRemovedReason();
			if (hr != S_OK)
			{
				return " (GetDeviceRemovedReason: " + std::system_category().message(hr) + ")";
			}
		}
		return "";
	}

	std::string Error(HRESULT hr)
	{
		if (hr != S_OK)
		{
			return std::system_category().message(hr);
		}
		return "";
	}

	std::string Format(const std::string text, HRESULT hr, ID3D11Device* device)
	{
		return text + ": " + Error(hr) + OptionalDeviceError(device);
	}

	ExceptionD3D11(const std::string text, HRESULT hr = S_OK, ID3D11Device* device = nullptr) : std::runtime_error(Format(text, hr, device))
	{
		LOG_CRIT(L"[D3D11] " << StringToWstring(what()));
	}
};

namespace Device
{
	extern Memory::ReentrantMutex buffer_context_mutex;
}

#include "Visual/Device/Win32/CompilerDirectX11.h"
#include "../D3DCommon/ReflectionD3D.h"

#if !defined(_XBOX_ONE)
#include "EnumerationDXGI.h"
#endif
#include "ESRAM.h"
#include "../D3DCommon/PixelFormatD3D.h"
#include "ConstantBufferD3D11.h"
#include "CommandListD3D11.h"
#include "StateD3D11.h"
#include "DeviceD3D11.h"
#include "SwapChainD3D11.h"
#include "BufferD3D11.cpp"
#include "CommandListD3D11.cpp"
#include "ConstantBufferD3D11.cpp"
#include "DeviceInfoDXGI.cpp"
#include "ShaderD3D11.cpp"
#include "SamplerD3D11.cpp"
#include "DeviceD3D11.cpp"
#include "EnumerationDXGI.cpp"
#include "ESRAM.cpp"
#include "IndexBufferD3D11.cpp"
#include "VertexBufferD3D11.cpp"
#include "TextureD3D11.cpp"
#include "RenderTargetD3D11.cpp"
#include "StateD3D11.cpp"
#include "SwapChainD3D11.cpp"
#include "ProfileD3D11.cpp"
