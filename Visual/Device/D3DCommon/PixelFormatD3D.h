#pragma once

namespace Device
{

	bool IsDepthFormat(PixelFormat format)
	{
		return format == PixelFormat::D32 || format == PixelFormat::D24X8 || format == PixelFormat::D24S8 || format == PixelFormat::D16;
	}

	DXGI_FORMAT GetDXGITexelFormat(TexelFormat format)
	{
		switch (format)
		{
		case Device::TexelFormat::R8_UInt: return DXGI_FORMAT_R8_UINT;
		case Device::TexelFormat::R8G8_UInt: return DXGI_FORMAT_R8G8_UINT;
		case Device::TexelFormat::R8G8B8A8_UInt: return DXGI_FORMAT_R8G8B8A8_UINT;

		case Device::TexelFormat::R16_UInt: return DXGI_FORMAT_R16_UINT;
		case Device::TexelFormat::R16G16_UInt: return DXGI_FORMAT_R16G16_UINT;
		case Device::TexelFormat::R16G16B16A16_UInt: return DXGI_FORMAT_R16G16B16A16_UINT;

		case Device::TexelFormat::R32_UInt: return DXGI_FORMAT_R32_UINT;
		case Device::TexelFormat::R32G32_UInt: return DXGI_FORMAT_R32G32_UINT;
		case Device::TexelFormat::R32G32B32_UInt: return DXGI_FORMAT_R32G32B32_UINT;
		case Device::TexelFormat::R32G32B32A32_UInt: return DXGI_FORMAT_R32G32B32A32_UINT;

		case Device::TexelFormat::R8_UNorm: return DXGI_FORMAT_R8_UNORM;
		case Device::TexelFormat::R8G8_UNorm: return DXGI_FORMAT_R8G8_UNORM;
		case Device::TexelFormat::R8G8B8A8_UNorm: return DXGI_FORMAT_R8G8B8A8_UNORM;

		case Device::TexelFormat::R16_UNorm: return DXGI_FORMAT_R16_UNORM;
		case Device::TexelFormat::R16G16_UNorm: return DXGI_FORMAT_R16G16_UNORM;
		case Device::TexelFormat::R16G16B16A16_UNorm: return DXGI_FORMAT_R16G16B16A16_UNORM;

		case Device::TexelFormat::R8_SInt: return DXGI_FORMAT_R8_SINT;
		case Device::TexelFormat::R8G8_SInt: return DXGI_FORMAT_R8G8_SINT;
		case Device::TexelFormat::R8G8B8A8_SInt: return DXGI_FORMAT_R8G8B8A8_SINT;

		case Device::TexelFormat::R16_SInt: return DXGI_FORMAT_R16_SINT;
		case Device::TexelFormat::R16G16_SInt: return DXGI_FORMAT_R16G16_SINT;
		case Device::TexelFormat::R16G16B16A16_SInt: return DXGI_FORMAT_R16G16B16A16_SINT;

		case Device::TexelFormat::R32_SInt: return DXGI_FORMAT_R32_SINT;
		case Device::TexelFormat::R32G32_SInt: return DXGI_FORMAT_R32G32_SINT;
		case Device::TexelFormat::R32G32B32_SInt: return DXGI_FORMAT_R32G32B32_SINT;
		case Device::TexelFormat::R32G32B32A32_SInt: return DXGI_FORMAT_R32G32B32A32_SINT;

		case Device::TexelFormat::R8_SNorm: return DXGI_FORMAT_R8_SNORM;
		case Device::TexelFormat::R8G8_SNorm: return DXGI_FORMAT_R8G8_SNORM;
		case Device::TexelFormat::R8G8B8A8_SNorm: return DXGI_FORMAT_R8G8B8A8_SNORM;

		case Device::TexelFormat::R16_SNorm: return DXGI_FORMAT_R16_SNORM;
		case Device::TexelFormat::R16G16_SNorm: return DXGI_FORMAT_R16G16_SNORM;
		case Device::TexelFormat::R16G16B16A16_SNorm: return DXGI_FORMAT_R16G16B16A16_SNORM;

		case Device::TexelFormat::R16_Float: return DXGI_FORMAT_R16_FLOAT;
		case Device::TexelFormat::R16G16_Float: return DXGI_FORMAT_R16G16_FLOAT;
		case Device::TexelFormat::R16G16B16A16_Float: return DXGI_FORMAT_R16G16B16A16_FLOAT;

		case Device::TexelFormat::R32_Float: return DXGI_FORMAT_R32_FLOAT;
		case Device::TexelFormat::R32G32_Float: return DXGI_FORMAT_R32G32_FLOAT;
		case Device::TexelFormat::R32G32B32_Float: return DXGI_FORMAT_R32G32B32_FLOAT;
		case Device::TexelFormat::R32G32B32A32_Float: return DXGI_FORMAT_R32G32B32A32_FLOAT;
		default:
			return DXGI_FORMAT_UNKNOWN;
		}
	}

	PixelFormat UnconvertPixelFormat(DXGI_FORMAT format)
	{
		switch (format)
		{
		case DXGI_FORMAT_R8G8B8A8_UNORM:		return PixelFormat::A8R8G8B8;
		case DXGI_FORMAT_B8G8R8A8_UNORM:		return PixelFormat::A8B8G8R8;
		case DXGI_FORMAT_B8G8R8X8_UNORM:		return PixelFormat::X8R8G8B8;
		case DXGI_FORMAT_BC1_UNORM:				return PixelFormat::BC1;
		case DXGI_FORMAT_BC2_UNORM:				return PixelFormat::BC2;
		case DXGI_FORMAT_BC3_UNORM:				return PixelFormat::BC3;
		case DXGI_FORMAT_BC4_UNORM:				return PixelFormat::BC4;
		case DXGI_FORMAT_BC5_UNORM:				return PixelFormat::BC5;
		case DXGI_FORMAT_BC6H_UF16:				return PixelFormat::BC6UF;
		case DXGI_FORMAT_BC7_UNORM:				return PixelFormat::BC7;
		case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:	return PixelFormat::A8R8G8B8;
		case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:	return PixelFormat::A8B8G8R8;
		case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:	return PixelFormat::X8R8G8B8;
		case DXGI_FORMAT_BC1_UNORM_SRGB:		return PixelFormat::BC1;
		case DXGI_FORMAT_BC2_UNORM_SRGB:		return PixelFormat::BC2;
		case DXGI_FORMAT_BC3_UNORM_SRGB:		return PixelFormat::BC3;
		case DXGI_FORMAT_BC7_UNORM_SRGB:		return PixelFormat::BC7;
		case DXGI_FORMAT_D24_UNORM_S8_UINT:		return PixelFormat::D24S8; // Dummy type.
		case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:	return PixelFormat::D24S8; // Dummy type.
		case DXGI_FORMAT_R32G8X24_TYPELESS:		return PixelFormat::D24X8; // Dummy type.
		case DXGI_FORMAT_R24G8_TYPELESS:		return PixelFormat::D24X8; // Dummy type.
		case DXGI_FORMAT_R8G8_UNORM:			return PixelFormat::A8L8;
		case DXGI_FORMAT_R16_FLOAT:				return PixelFormat::R16F;
		case DXGI_FORMAT_R16_UNORM:				return PixelFormat::R16;
		case DXGI_FORMAT_B4G4R4A4_UNORM:		return PixelFormat::A4R4G4B4;
		case DXGI_FORMAT_R16_TYPELESS:			return PixelFormat::D16;
		case DXGI_FORMAT_R11G11B10_FLOAT:		return PixelFormat::R11G11B10F;
		case DXGI_FORMAT_R32G32B32A32_FLOAT:	return PixelFormat::A32B32G32R32F;
		case DXGI_FORMAT_R16G16B16A16_FLOAT:	return PixelFormat::A16B16G16R16F;
		case DXGI_FORMAT_R32_FLOAT:				return PixelFormat::R32F;
		case DXGI_FORMAT_R16G16_UNORM:				return PixelFormat::G16R16;
		case DXGI_FORMAT_R16G16_FLOAT:			return PixelFormat::G16R16F;
		case DXGI_FORMAT_R32G32_FLOAT:			return PixelFormat::G32R32F;
		case DXGI_FORMAT_R8_UNORM:				return PixelFormat::L8;
		case DXGI_FORMAT_R16G16_UINT:			return PixelFormat::G16R16_UINT;
		default:								throw std::runtime_error("Unsupported (UnconvertPixelFormat)");
		}
	}

	DXGI_FORMAT ConvertPixelFormat(PixelFormat format, bool useSRGB)
	{
		switch (format)
		{
		case PixelFormat::UNKNOWN:				return DXGI_FORMAT_UNKNOWN;
		case PixelFormat::A8R8G8B8:				return (useSRGB) ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
		case PixelFormat::X8R8G8B8:				return (useSRGB) ? DXGI_FORMAT_B8G8R8X8_UNORM_SRGB : DXGI_FORMAT_B8G8R8X8_UNORM;
		case PixelFormat::R5G6B5:				return DXGI_FORMAT_B5G6R5_UNORM;
		case PixelFormat::A1R5G5B5:				return DXGI_FORMAT_B5G5R5A1_UNORM;
		case PixelFormat::A4R4G4B4:				return DXGI_FORMAT_B4G4R4A4_UNORM;
		case PixelFormat::A8:					return DXGI_FORMAT_A8_UNORM;
		case PixelFormat::A8L8:					return DXGI_FORMAT_R8G8_UNORM;
		case PixelFormat::A2B10G10R10:			return DXGI_FORMAT_R10G10B10A2_UNORM;
		case PixelFormat::A8B8G8R8:				return (useSRGB) ? DXGI_FORMAT_B8G8R8A8_UNORM_SRGB : DXGI_FORMAT_B8G8R8A8_UNORM;
		case PixelFormat::G16R16:				return DXGI_FORMAT_R16G16_UNORM;
		case PixelFormat::G16R16_UINT:			return DXGI_FORMAT_R16G16_UINT;
		case PixelFormat::G16R16F:				return DXGI_FORMAT_R16G16_FLOAT;
		case PixelFormat::G32R32F:				return DXGI_FORMAT_R32G32_FLOAT;
		case PixelFormat::A16B16G16R16:			return DXGI_FORMAT_R16G16B16A16_UNORM;
		case PixelFormat::L8:					return DXGI_FORMAT_R8_UNORM;
		case PixelFormat::V8U8:					return DXGI_FORMAT_R8G8_SNORM;
		case PixelFormat::Q8W8V8U8:				return DXGI_FORMAT_R8G8B8A8_SNORM;
		case PixelFormat::R16F:					return DXGI_FORMAT_R16_FLOAT;
		case PixelFormat::R16:					return DXGI_FORMAT_R16_UNORM;
		case PixelFormat::V16U16:				return DXGI_FORMAT_R16G16_SNORM;
		case PixelFormat::R8G8_B8G8:			return DXGI_FORMAT_G8R8_G8B8_UNORM;
		case PixelFormat::BC1:					return (useSRGB) ? DXGI_FORMAT_BC1_UNORM_SRGB : DXGI_FORMAT_BC1_UNORM;
		case PixelFormat::BC2_Premultiplied:	return (useSRGB) ? DXGI_FORMAT_BC2_UNORM_SRGB : DXGI_FORMAT_BC2_UNORM;
		case PixelFormat::BC2:					return (useSRGB) ? DXGI_FORMAT_BC2_UNORM_SRGB : DXGI_FORMAT_BC2_UNORM;
		case PixelFormat::BC3:					return (useSRGB) ? DXGI_FORMAT_BC3_UNORM_SRGB : DXGI_FORMAT_BC3_UNORM;
		case PixelFormat::BC4:					return DXGI_FORMAT_BC4_UNORM;
		case PixelFormat::BC5:					return DXGI_FORMAT_BC5_UNORM;
		case PixelFormat::BC6UF:				return DXGI_FORMAT_BC6H_UF16;
		case PixelFormat::BC7_Premultiplied:	return (useSRGB) ? DXGI_FORMAT_BC7_UNORM_SRGB : DXGI_FORMAT_BC7_UNORM;
		case PixelFormat::BC7:					return (useSRGB) ? DXGI_FORMAT_BC7_UNORM_SRGB : DXGI_FORMAT_BC7_UNORM;
		case PixelFormat::D16:					return DXGI_FORMAT_R16_TYPELESS;
		case PixelFormat::D32F_LOCKABLE:		return DXGI_FORMAT_D32_FLOAT;
		case PixelFormat::D24S8:				return DXGI_FORMAT_R24G8_TYPELESS;// DXGI_FORMAT_D24_UNORM_S8_UINT; // NOTE: Typeless needed on x64 to avoid Debug Layer error.
		case PixelFormat::D24X8:				return DXGI_FORMAT_R24G8_TYPELESS;
		case PixelFormat::R11G11B10F:			return DXGI_FORMAT_R11G11B10_FLOAT;
		case PixelFormat::A32B32G32R32F:		return DXGI_FORMAT_R32G32B32A32_FLOAT;
		case PixelFormat::A16B16G16R16F:		return DXGI_FORMAT_R16G16B16A16_FLOAT;
		case PixelFormat::R32F:					return DXGI_FORMAT_R32_FLOAT;
		default:								throw std::runtime_error("Unsupported");
		}
	}

	DXGI_FORMAT ConvertDeclTypeD3D(DeclType decl_type)
	{
		switch (decl_type)
		{
		case DeclType::FLOAT4:		return DXGI_FORMAT_R32G32B32A32_FLOAT;
		case DeclType::FLOAT3:		return DXGI_FORMAT_R32G32B32_FLOAT;
		case DeclType::FLOAT2:		return DXGI_FORMAT_R32G32_FLOAT;
		case DeclType::FLOAT1:		return DXGI_FORMAT_R32_FLOAT;
		case DeclType::D3DCOLOR:	return DXGI_FORMAT_B8G8R8A8_UNORM;
		case DeclType::UBYTE4N:		return DXGI_FORMAT_R8G8B8A8_UNORM;
		case DeclType::BYTE4N:		return DXGI_FORMAT_R8G8B8A8_SNORM;
		case DeclType::UBYTE4:		return DXGI_FORMAT_R8G8B8A8_UINT;
		case DeclType::FLOAT16_2:	return DXGI_FORMAT_R16G16_FLOAT;
		case DeclType::FLOAT16_4:	return DXGI_FORMAT_R16G16B16A16_FLOAT;
		default:					return DXGI_FORMAT_UNKNOWN;
		}
	}

	LPCSTR ConvertDeclUsageD3D(DeclUsage decl_usage)
	{
		switch (decl_usage)
		{
		case DeclUsage::POSITION:		return "POSITION";
		case DeclUsage::BLENDWEIGHT:	return "BLENDWEIGHT";
		case DeclUsage::BLENDINDICES:	return "BLENDINDICES";
		case DeclUsage::NORMAL:			return "NORMAL";
		case DeclUsage::PSIZE:			return "PSIZE";
		case DeclUsage::TEXCOORD:		return "TEXCOORD";
		case DeclUsage::TANGENT:		return "TANGENT";
		case DeclUsage::BINORMAL:		return "BINORMAL";
		case DeclUsage::TESSFACTOR:		return "TESSFACTOR";
		case DeclUsage::POSITIONT:		return "POSITIONT";
		case DeclUsage::COLOR:			return "COLOR";
		case DeclUsage::FOG:			return "FOG";
		case DeclUsage::DEPTH:			return "DEPTH";
		case DeclUsage::SAMPLE:			return "SAMPLE";
		default:						return "UNKNOWN";
		}
	}

	DXGI_FORMAT ConvertPixelFormatD3D12(PixelFormat format, bool useSRGB)
	{
		switch (format)
		{
		case PixelFormat::D24S8:				return DXGI_FORMAT_D24_UNORM_S8_UINT;
		case PixelFormat::D24X8:				return DXGI_FORMAT_D24_UNORM_S8_UINT;
		default:								return ConvertPixelFormat(format, useSRGB);
		}
	}

	DXGI_FORMAT GetSRVFormatD3D12(DXGI_FORMAT format)
	{
		switch (format)
		{
		case DXGI_FORMAT_R24G8_TYPELESS:
		case DXGI_FORMAT_D24_UNORM_S8_UINT:		return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
		default:								return format;
		}
	}

	bool IsCompressedDXGIFormat(DXGI_FORMAT fmt)
	{
		switch (fmt)
		{
		case DXGI_FORMAT_BC1_TYPELESS:
		case DXGI_FORMAT_BC1_UNORM:
		case DXGI_FORMAT_BC1_UNORM_SRGB:
		case DXGI_FORMAT_BC2_TYPELESS:
		case DXGI_FORMAT_BC2_UNORM:
		case DXGI_FORMAT_BC2_UNORM_SRGB:
		case DXGI_FORMAT_BC3_TYPELESS:
		case DXGI_FORMAT_BC3_UNORM:
		case DXGI_FORMAT_BC3_UNORM_SRGB:
		case DXGI_FORMAT_BC4_TYPELESS:
		case DXGI_FORMAT_BC4_UNORM:
		case DXGI_FORMAT_BC4_SNORM:
		case DXGI_FORMAT_BC5_TYPELESS:
		case DXGI_FORMAT_BC5_UNORM:
		case DXGI_FORMAT_BC5_SNORM:
		case DXGI_FORMAT_BC6H_TYPELESS:
		case DXGI_FORMAT_BC6H_UF16:
		case DXGI_FORMAT_BC6H_SF16:
		case DXGI_FORMAT_BC7_TYPELESS:
		case DXGI_FORMAT_BC7_UNORM:
		case DXGI_FORMAT_BC7_UNORM_SRGB:
			return true;

		default:
			return false;
		}
	}

}