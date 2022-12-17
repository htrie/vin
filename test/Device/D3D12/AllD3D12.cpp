#if defined(_XBOX_ONE)
#if defined(_RELEASE)
#define D3DCOMPILE_NO_DEBUG 1
#endif
#include <d3d12_x.h>
#include <d3dx12_x.h>

#if defined(ALLOW_DEBUG_LAYER)
#define USE_PIX
#define USE_PIX_ON_ALL_ARCHITECTURES
#endif
#include <pix3.h>
#pragma comment(lib, "pixEvt.lib")
#else
#include <d3d12.h>
#include "d3dx12.h"
#include <dxgi1_6.h>
#if defined(ALLOW_DEBUG_LAYER)
#include <D3d12SDKLayers.h>
#endif
#endif

using Microsoft::WRL::ComPtr;

#if !defined(_XBOX_ONE)
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#else
#pragma comment(lib, "d3d12_x.lib")
#endif
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "dxguid.lib")

// TODO: remove after ClearD3D12 doesn't use XMCOLOR
#include <DirectXPackedVector.h>

// TODO: Need this because UserProfileResource::MakeAvatarCircular uses DirectX:: functions
#include "../D3DCommon/DirectXTex.h"

#include "Visual/Device/D3D11/DeviceInfoDXGI.cpp"
#include "../D3DCommon/PixelFormatD3D.h"
#include "../D3DCommon/ReflectionD3D.h"
#include "UtilityD3D12.h"
#include "AllocatorUtilsD3D12.h"

#define D3D12MA_D3D12_HEADERS_ALREADY_INCLUDED
#include "D3D12MemAlloc.h"

#if defined(_XBOX_ONE)
	#include "AllocatorD3D12_XBOX.h"
#else
	#include "AllocatorD3D12_PC.h"
#endif
#include "AllocatorDataD3D12.h"
#include "DescriptorCacheD3D12.h"
#include "RootSignatureD3D12.h"
#include "StateD3D12.h"
#include "DeviceD3D12.h"
#include "TextureD3D12.h"
#include "SwapChainD3D12.h"
#include "DescriptorCacheD3D12.cpp"
#if defined(_XBOX_ONE)
	#include "AllocatorD3D12_XBOX.cpp"
#else
	#include "AllocatorD3D12_PC.cpp"
#endif
#include "AllocatorDataD3D12.cpp"
#include "TransferD3D12.cpp"
#include "SamplerD3D12.cpp"
#include "BufferD3D12.cpp"
#include "IndexBufferD3D12.cpp"
#include "VertexBufferD3D12.cpp"
#include "ShaderD3D12.cpp"
#include "TextureD3D12.cpp"
#include "RootSignatureD3D12.cpp"
#include "DeviceD3D12.cpp"
#include "RenderTargetD3D12.cpp"
#include "SwapChainD3D12.cpp"
#include "ProfileD3D12.cpp"
#include "StateD3D12.cpp"
