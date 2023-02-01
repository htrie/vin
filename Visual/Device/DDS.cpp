
DDS::DDS(const uint8_t* data, const size_t size)
{
	// Validate DDS file in memory
	if (size < (sizeof(uint32_t) + sizeof(DDS_HEADER)))
		throw std::runtime_error("Data size is smaller than header size");

	uint32_t dwMagicNumber = *(const uint32_t*)(data);
	if (dwMagicNumber != MAGIC)
		throw std::runtime_error("Invalid magic number");

	header = reinterpret_cast<const DDS_HEADER*>(data + sizeof(uint32_t));

	// Verify header to validate DDS file
	if (header->size != sizeof(DDS_HEADER) || header->ddspf.size != sizeof(DDS_PIXELFORMAT))
		throw std::runtime_error("Invalid header size");

	// Check for DX10 extension
	if ((header->ddspf.flags & DDS_FOURCC) && (MAKEFOURCC('D', 'X', '1', '0') == header->ddspf.fourCC))
	{
		// Must be long enough for both headers and magic value
		if (size < (sizeof(DDS_HEADER) + sizeof(uint32_t) + sizeof(DDS_HEADER_DXT10)))
			throw std::runtime_error("Invalid DX10 header size");
		
		header_dxt10 = reinterpret_cast<const DDS_HEADER_DXT10*>(data + sizeof(uint32_t) + sizeof(DDS_HEADER));
	}

	const size_t header_total_size = sizeof(uint32_t) + sizeof(DDS_HEADER) + (header_dxt10 ? sizeof(DDS_HEADER_DXT10) : 0);
	pixels = data + header_total_size;
	pixels_size = size - header_total_size;

	width = header->width;
	height = header->height;
	depth = header->depth;

	resDim = D3D11_RESOURCE_DIMENSION_UNKNOWN;
	arraySize = 1;
	dxgi_format = DXGI_FORMAT_UNKNOWN;
	isCubeMap = false;

	mipCount = header->mipMapCount;
	if (0 == mipCount)
	{
		mipCount = 1;
	}

	if ((header->ddspf.flags & DDS_FOURCC) && (MAKEFOURCC('D', 'X', '1', '0') == header->ddspf.fourCC))
	{
		auto d3d10ext = reinterpret_cast<const DDS_HEADER_DXT10*>((const char*)header + sizeof(DDS_HEADER));

		arraySize = d3d10ext->arraySize;
		if (arraySize == 0)
			throw std::runtime_error("Invalid array size");

		switch (d3d10ext->dxgiFormat)
		{
		case DXGI_FORMAT_AI44:
		case DXGI_FORMAT_IA44:
		case DXGI_FORMAT_P8:
		case DXGI_FORMAT_A8P8:
			throw std::runtime_error("Invalid header size");

		default:
			if (BitsPerPixel(d3d10ext->dxgiFormat) == 0)
				throw std::runtime_error("Invalid DXT10 pixel format");
		}

		dxgi_format = d3d10ext->dxgiFormat;

		switch (d3d10ext->resourceDimension)
		{
		case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
			// D3DX writes 1D textures with a fixed Height of 1
			if ((header->flags & DDS_HEIGHT) && height != 1)
				throw std::runtime_error("Invalid 1D height");
			height = depth = 1;
			break;

		case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
			if (d3d10ext->miscFlag & D3D11_RESOURCE_MISC_TEXTURECUBE)
			{
				arraySize *= 6;
				isCubeMap = true;
			}

			if (arraySize == 6 && !isCubeMap)
				throw std::runtime_error("Missing cubemap flag");

			depth = 1;
			break;

		case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
			if (!(header->flags & DDS_HEADER_FLAGS_VOLUME))
				throw std::runtime_error("Missing volume flag");

			if (arraySize > 1)
				throw std::runtime_error("Invalid array size");
			break;

		default:
			throw std::runtime_error("Invalid dimension");
		}

		resDim = d3d10ext->resourceDimension;
	}
	else
	{
		dxgi_format = GetDXGIFormat(header->ddspf);

		if (dxgi_format == DXGI_FORMAT_UNKNOWN)
			throw std::runtime_error("Invalid pixel format");

		if (header->flags & DDS_HEADER_FLAGS_VOLUME)
		{
			resDim = D3D11_RESOURCE_DIMENSION_TEXTURE3D;
		}
		else
		{
			if (header->caps2 & DDS_CUBEMAP)
			{
				// We require all six faces to be defined
				if ((header->caps2 & DDS_CUBEMAP_ALLFACES) != DDS_CUBEMAP_ALLFACES)
					throw std::runtime_error("Invalid cubemap faces flag");

				arraySize = 6;
				isCubeMap = true;
			}

			depth = 1;
			resDim = D3D11_RESOURCE_DIMENSION_TEXTURE2D;

			// Note there's no way for a legacy Direct3D 9 DDS to express a '1D' texture
		}

		assert(BitsPerPixel(dxgi_format) != 0);
	}

	// Bound sizes (for security purposes we don't trust DDS file metadata larger than the D3D 11.x hardware requirements)
	if (mipCount > D3D11_REQ_MIP_LEVELS)
		throw std::runtime_error("Too many mip levels");

	switch (resDim)
	{
	case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
		if ((arraySize > D3D11_REQ_TEXTURE1D_ARRAY_AXIS_DIMENSION) ||
			(width > D3D11_REQ_TEXTURE1D_U_DIMENSION))
			throw std::runtime_error("Invalid 1D size");
		break;

	case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
		if (isCubeMap)
		{
			// This is the right bound because we set arraySize to (NumCubes*6) above
			if ((arraySize > D3D11_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION) ||
				(width > D3D11_REQ_TEXTURECUBE_DIMENSION) ||
				(height > D3D11_REQ_TEXTURECUBE_DIMENSION))
				throw std::runtime_error("Invalid 2D size");
		}
		else if ((arraySize > D3D11_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION) ||
			(width > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION) ||
			(height > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION))
			throw std::runtime_error("Invalid 2D size");
		break;

	case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
		if ((arraySize > 1) ||
			(width > D3D11_REQ_TEXTURE3D_U_V_OR_W_DIMENSION) ||
			(height > D3D11_REQ_TEXTURE3D_U_V_OR_W_DIMENSION) ||
			(depth > D3D11_REQ_TEXTURE3D_U_V_OR_W_DIMENSION))
			throw std::runtime_error("Invalid 3D size");
		break;

	default:
		throw std::runtime_error("Invalid dimension");
	}

	FillInitData(width, height, depth, mipCount, arraySize, dxgi_format, 0, pixels_size, pixels);
}

Device::Dimension DDS::GetDimension() const
{
	switch (resDim)
	{
	case D3D11_RESOURCE_DIMENSION_TEXTURE1D:	return Device::Dimension::One;
	case D3D11_RESOURCE_DIMENSION_TEXTURE2D:	return isCubeMap ? Device::Dimension::Cubemap : Device::Dimension::Two;
	case D3D11_RESOURCE_DIMENSION_TEXTURE3D:	return Device::Dimension::Three;
	default:									throw std::runtime_error("Unsupported resource dimension");
	}
}

Device::PixelFormat DDS::GetPixelFormat() const
{
	switch (dxgi_format)
	{
	case DXGI_FORMAT_R8G8B8A8_UNORM:		return Device::PixelFormat::A8R8G8B8;
	case DXGI_FORMAT_B8G8R8A8_UNORM:		return Device::PixelFormat::A8B8G8R8;
	case DXGI_FORMAT_B8G8R8X8_UNORM:		return Device::PixelFormat::X8R8G8B8;
	case DXGI_FORMAT_R8_UNORM:				return Device::PixelFormat::L8;
	case DXGI_FORMAT_BC1_UNORM:				return Device::PixelFormat::BC1;
	case DXGI_FORMAT_BC2_UNORM:				return Device::PixelFormat::BC2;
	case DXGI_FORMAT_BC3_UNORM:				return Device::PixelFormat::BC3;
	case DXGI_FORMAT_BC4_UNORM:				return Device::PixelFormat::BC4;
	case DXGI_FORMAT_BC5_UNORM:				return Device::PixelFormat::BC5;
	case DXGI_FORMAT_BC6H_UF16:				return Device::PixelFormat::BC6UF;
	case DXGI_FORMAT_BC7_UNORM:				return Device::PixelFormat::BC7;
	case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:	return Device::PixelFormat::A8R8G8B8;
	case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:	return Device::PixelFormat::A8B8G8R8;
	case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:	return Device::PixelFormat::X8R8G8B8;
	case DXGI_FORMAT_BC1_UNORM_SRGB:		return Device::PixelFormat::BC1;
	case DXGI_FORMAT_BC2_UNORM_SRGB:		return Device::PixelFormat::BC2;
	case DXGI_FORMAT_BC3_UNORM_SRGB:		return Device::PixelFormat::BC3;
	case DXGI_FORMAT_BC7_UNORM_SRGB:		return Device::PixelFormat::BC7;
	case DXGI_FORMAT_D24_UNORM_S8_UINT:		return Device::PixelFormat::D24S8; // Dummy type.
	case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:	return Device::PixelFormat::D24S8; // Dummy type.
	case DXGI_FORMAT_R32G8X24_TYPELESS:		return Device::PixelFormat::D24X8; // Dummy type.
	case DXGI_FORMAT_R24G8_TYPELESS:		return Device::PixelFormat::D24X8; // Dummy type.
	case DXGI_FORMAT_R8G8_UNORM:			return Device::PixelFormat::A8L8;
	case DXGI_FORMAT_R16_UNORM:				return Device::PixelFormat::R16;
	case DXGI_FORMAT_R16G16_UNORM:			return Device::PixelFormat::G16R16;
	case DXGI_FORMAT_R16_FLOAT:				return Device::PixelFormat::R16F;
	case DXGI_FORMAT_B4G4R4A4_UNORM:		return Device::PixelFormat::A4R4G4B4;
	case DXGI_FORMAT_R16_TYPELESS:			return Device::PixelFormat::D16;
	case DXGI_FORMAT_R16G16B16A16_FLOAT:	return Device::PixelFormat::A16B16G16R16F;
	case DXGI_FORMAT_R32G32B32A32_FLOAT:	return Device::PixelFormat::A32B32G32R32F;
	case DXGI_FORMAT_R32_FLOAT:				return Device::PixelFormat::R32F;
	case DXGI_FORMAT_R16G16_FLOAT:			return Device::PixelFormat::G16R16F;
	case DXGI_FORMAT_R32G32_FLOAT:			return Device::PixelFormat::G32R32F;
	default:								throw std::runtime_error("Unsupported pixel format");
	}
}


size_t DDS::BitsPerPixel(DXGI_FORMAT fmt)
{
	switch (fmt)
	{
	case DXGI_FORMAT_R32G32B32A32_TYPELESS:
	case DXGI_FORMAT_R32G32B32A32_FLOAT:
	case DXGI_FORMAT_R32G32B32A32_UINT:
	case DXGI_FORMAT_R32G32B32A32_SINT:
		return 128;

	case DXGI_FORMAT_R32G32B32_TYPELESS:
	case DXGI_FORMAT_R32G32B32_FLOAT:
	case DXGI_FORMAT_R32G32B32_UINT:
	case DXGI_FORMAT_R32G32B32_SINT:
		return 96;

	case DXGI_FORMAT_R16G16B16A16_TYPELESS:
	case DXGI_FORMAT_R16G16B16A16_FLOAT:
	case DXGI_FORMAT_R16G16B16A16_UNORM:
	case DXGI_FORMAT_R16G16B16A16_UINT:
	case DXGI_FORMAT_R16G16B16A16_SNORM:
	case DXGI_FORMAT_R16G16B16A16_SINT:
	case DXGI_FORMAT_R32G32_TYPELESS:
	case DXGI_FORMAT_R32G32_FLOAT:
	case DXGI_FORMAT_R32G32_UINT:
	case DXGI_FORMAT_R32G32_SINT:
	case DXGI_FORMAT_R32G8X24_TYPELESS:
	case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
	case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
	case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
	case DXGI_FORMAT_Y416:
	case DXGI_FORMAT_Y210:
	case DXGI_FORMAT_Y216:
		return 64;

	case DXGI_FORMAT_R10G10B10A2_TYPELESS:
	case DXGI_FORMAT_R10G10B10A2_UNORM:
	case DXGI_FORMAT_R10G10B10A2_UINT:
	case DXGI_FORMAT_R11G11B10_FLOAT:
	case DXGI_FORMAT_R8G8B8A8_TYPELESS:
	case DXGI_FORMAT_R8G8B8A8_UNORM:
	case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
	case DXGI_FORMAT_R8G8B8A8_UINT:
	case DXGI_FORMAT_R8G8B8A8_SNORM:
	case DXGI_FORMAT_R8G8B8A8_SINT:
	case DXGI_FORMAT_R16G16_TYPELESS:
	case DXGI_FORMAT_R16G16_FLOAT:
	case DXGI_FORMAT_R16G16_UNORM:
	case DXGI_FORMAT_R16G16_UINT:
	case DXGI_FORMAT_R16G16_SNORM:
	case DXGI_FORMAT_R16G16_SINT:
	case DXGI_FORMAT_R32_TYPELESS:
	case DXGI_FORMAT_D32_FLOAT:
	case DXGI_FORMAT_R32_FLOAT:
	case DXGI_FORMAT_R32_UINT:
	case DXGI_FORMAT_R32_SINT:
	case DXGI_FORMAT_R24G8_TYPELESS:
	case DXGI_FORMAT_D24_UNORM_S8_UINT:
	case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
	case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
	case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
	case DXGI_FORMAT_R8G8_B8G8_UNORM:
	case DXGI_FORMAT_G8R8_G8B8_UNORM:
	case DXGI_FORMAT_B8G8R8A8_UNORM:
	case DXGI_FORMAT_B8G8R8X8_UNORM:
	case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM:
	case DXGI_FORMAT_B8G8R8A8_TYPELESS:
	case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
	case DXGI_FORMAT_B8G8R8X8_TYPELESS:
	case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
	case DXGI_FORMAT_AYUV:
	case DXGI_FORMAT_Y410:
	case DXGI_FORMAT_YUY2:
		return 32;

	case DXGI_FORMAT_P010:
	case DXGI_FORMAT_P016:
		return 24;

	case DXGI_FORMAT_R8G8_TYPELESS:
	case DXGI_FORMAT_R8G8_UNORM:
	case DXGI_FORMAT_R8G8_UINT:
	case DXGI_FORMAT_R8G8_SNORM:
	case DXGI_FORMAT_R8G8_SINT:
	case DXGI_FORMAT_R16_TYPELESS:
	case DXGI_FORMAT_R16_FLOAT:
	case DXGI_FORMAT_D16_UNORM:
	case DXGI_FORMAT_R16_UNORM:
	case DXGI_FORMAT_R16_UINT:
	case DXGI_FORMAT_R16_SNORM:
	case DXGI_FORMAT_R16_SINT:
	case DXGI_FORMAT_B5G6R5_UNORM:
	case DXGI_FORMAT_B5G5R5A1_UNORM:
	case DXGI_FORMAT_A8P8:
	case DXGI_FORMAT_B4G4R4A4_UNORM:
		return 16;

	case DXGI_FORMAT_NV12:
	case DXGI_FORMAT_420_OPAQUE:
	case DXGI_FORMAT_NV11:
		return 12;

	case DXGI_FORMAT_R8_TYPELESS:
	case DXGI_FORMAT_R8_UNORM:
	case DXGI_FORMAT_R8_UINT:
	case DXGI_FORMAT_R8_SNORM:
	case DXGI_FORMAT_R8_SINT:
	case DXGI_FORMAT_A8_UNORM:
	case DXGI_FORMAT_AI44:
	case DXGI_FORMAT_IA44:
	case DXGI_FORMAT_P8:
		return 8;

	case DXGI_FORMAT_R1_UNORM:
		return 1;

	case DXGI_FORMAT_BC1_TYPELESS:
	case DXGI_FORMAT_BC1_UNORM:
	case DXGI_FORMAT_BC1_UNORM_SRGB:
	case DXGI_FORMAT_BC4_TYPELESS:
	case DXGI_FORMAT_BC4_UNORM:
	case DXGI_FORMAT_BC4_SNORM:
		return 4;

	case DXGI_FORMAT_BC2_TYPELESS:
	case DXGI_FORMAT_BC2_UNORM:
	case DXGI_FORMAT_BC2_UNORM_SRGB:
	case DXGI_FORMAT_BC3_TYPELESS:
	case DXGI_FORMAT_BC3_UNORM:
	case DXGI_FORMAT_BC3_UNORM_SRGB:
	case DXGI_FORMAT_BC5_TYPELESS:
	case DXGI_FORMAT_BC5_UNORM:
	case DXGI_FORMAT_BC5_SNORM:
	case DXGI_FORMAT_BC6H_TYPELESS:
	case DXGI_FORMAT_BC6H_UF16:
	case DXGI_FORMAT_BC6H_SF16:
	case DXGI_FORMAT_BC7_TYPELESS:
	case DXGI_FORMAT_BC7_UNORM:
	case DXGI_FORMAT_BC7_UNORM_SRGB:
		return 8;

	default:
		throw std::runtime_error("Unsupported");
	}
}

void DDS::GetSurfaceInfo(size_t width, size_t height, DXGI_FORMAT fmt, size_t* outBpe, size_t* outNumBlocksWide, size_t* outNumBlocksHigh, size_t* outNumBytes, size_t* outRowBytes, size_t* outNumRows, size_t* outBlockWidth, size_t* outBlockHeight)
{
	size_t bpe = BitsPerPixel(fmt);
	size_t numBlocksWide = 0;
	size_t numBlocksHigh = 0;
	size_t numBytes = 0;
	size_t rowBytes = 0;
	size_t numRows = 0;
	size_t blockWidth = 1;
	size_t blockHeight = 1;

	bool bc = false;
	switch (fmt)
	{
	case DXGI_FORMAT_BC1_TYPELESS:
	case DXGI_FORMAT_BC1_UNORM:
	case DXGI_FORMAT_BC1_UNORM_SRGB:
	case DXGI_FORMAT_BC4_TYPELESS:
	case DXGI_FORMAT_BC4_UNORM:
	case DXGI_FORMAT_BC4_SNORM:
		bc = true;
		bpe = 8;
		break;

	case DXGI_FORMAT_BC2_TYPELESS:
	case DXGI_FORMAT_BC2_UNORM:
	case DXGI_FORMAT_BC2_UNORM_SRGB:
	case DXGI_FORMAT_BC3_TYPELESS:
	case DXGI_FORMAT_BC3_UNORM:
	case DXGI_FORMAT_BC3_UNORM_SRGB:
	case DXGI_FORMAT_BC5_TYPELESS:
	case DXGI_FORMAT_BC5_UNORM:
	case DXGI_FORMAT_BC5_SNORM:
	case DXGI_FORMAT_BC6H_TYPELESS:
	case DXGI_FORMAT_BC6H_UF16:
	case DXGI_FORMAT_BC6H_SF16:
	case DXGI_FORMAT_BC7_TYPELESS:
	case DXGI_FORMAT_BC7_UNORM:
	case DXGI_FORMAT_BC7_UNORM_SRGB:
		bc = true;
		bpe = 16;
		break;

	default:
		break;
	}

	if (bc)
	{
		blockWidth = 4;
		blockHeight = 4;
		if (width > 0)
		{
			numBlocksWide = std::max<size_t>(1, (width + 3) / 4);
		}
		if (height > 0)
		{
			numBlocksHigh = std::max<size_t>(1, (height + 3) / 4);
		}
		rowBytes = numBlocksWide * bpe;
		numRows = numBlocksHigh;
		numBytes = rowBytes * numBlocksHigh;
	}
	else
	{
		size_t bpp = BitsPerPixel(fmt);
		numBlocksWide = width;
		numBlocksHigh = height;
		rowBytes = (width * bpp + 7) / 8; // round up to nearest byte
		numRows = height;
		numBytes = rowBytes * height;
		bpe = bpp;
	}

	if (outBpe)
	{
		*outBpe = bpe;
	}
	if (outNumBlocksWide)
	{
		*outNumBlocksWide = numBlocksWide;
	}
	if (outNumBlocksHigh)
	{
		*outNumBlocksHigh = numBlocksHigh;
	}
	if (outNumBytes)
	{
		*outNumBytes = numBytes;
	}
	if (outRowBytes)
	{
		*outRowBytes = rowBytes;
	}
	if (outNumRows)
	{
		*outNumRows = numRows;
	}
	if (outBlockWidth)
	{
		*outBlockWidth = blockWidth;
	}
	if (outBlockHeight)
	{
		*outBlockHeight = blockHeight;
	}
}

DDS::DXGI_FORMAT DDS::GetDXGIFormat(const DDS_PIXELFORMAT& ddpf)
{
	if (ddpf.flags & DDS_RGB)
	{
		// Note that sRGB formats are written using the "DX10" extended header

		switch (ddpf.RGBBitCount)
		{
		case 32:
			if (ISBITMASK(0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000))
			{
				return DXGI_FORMAT_R8G8B8A8_UNORM;
			}

			if (ISBITMASK(0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000))
			{
				return DXGI_FORMAT_B8G8R8A8_UNORM;
			}

			if (ISBITMASK(0x00ff0000, 0x0000ff00, 0x000000ff, 0x00000000))
			{
				return DXGI_FORMAT_B8G8R8X8_UNORM;
			}

			// No DXGI format maps to ISBITMASK(0x000000ff,0x0000ff00,0x00ff0000,0x00000000) aka D3DFMT_X8B8G8R8

			// Note that many common DDS reader/writers (including D3DX) swap the
			// the RED/BLUE masks for 10:10:10:2 formats. We assume
			// below that the 'backwards' header mask is being used since it is most
			// likely written by D3DX. The more robust solution is to use the 'DX10'
			// header extension and specify the DXGI_FORMAT_R10G10B10A2_UNORM format directly

			// For 'correct' writers, this should be 0x000003ff,0x000ffc00,0x3ff00000 for RGB data
			if (ISBITMASK(0x3ff00000, 0x000ffc00, 0x000003ff, 0xc0000000))
			{
				return DXGI_FORMAT_R10G10B10A2_UNORM;
			}

			// No DXGI format maps to ISBITMASK(0x000003ff,0x000ffc00,0x3ff00000,0xc0000000) aka D3DFMT_A2R10G10B10

			if (ISBITMASK(0x0000ffff, 0xffff0000, 0x00000000, 0x00000000))
			{
				return DXGI_FORMAT_R16G16_UNORM;
			}

			if (ISBITMASK(0xffffffff, 0x00000000, 0x00000000, 0x00000000))
			{
				// Only 32-bit color channel format in D3D9 was R32F
				return DXGI_FORMAT_R32_FLOAT; // D3DX writes this out as a FourCC of 114
			}
			break;

		case 24:
			// No 24bpp DXGI formats aka D3DFMT_R8G8B8
			break;

		case 16:
			if (ISBITMASK(0x7c00, 0x03e0, 0x001f, 0x8000))
			{
				return DXGI_FORMAT_B5G5R5A1_UNORM;
			}
			if (ISBITMASK(0xf800, 0x07e0, 0x001f, 0x0000))
			{
				return DXGI_FORMAT_B5G6R5_UNORM;
			}

			// No DXGI format maps to ISBITMASK(0x7c00,0x03e0,0x001f,0x0000) aka D3DFMT_X1R5G5B5

			if (ISBITMASK(0x0f00, 0x00f0, 0x000f, 0xf000))
			{
				return DXGI_FORMAT_B4G4R4A4_UNORM;
			}

			// No DXGI format maps to ISBITMASK(0x0f00,0x00f0,0x000f,0x0000) aka D3DFMT_X4R4G4B4

			// No 3:3:2, 3:3:2:8, or paletted DXGI formats aka D3DFMT_A8R3G3B2, D3DFMT_R3G3B2, D3DFMT_P8, D3DFMT_A8P8, etc.
			break;
		}
	}
	else if (ddpf.flags & DDS_LUMINANCE)
	{
		if (8 == ddpf.RGBBitCount)
		{
			if (ISBITMASK(0x000000ff, 0x00000000, 0x00000000, 0x00000000))
			{
				return DXGI_FORMAT_R8_UNORM; // D3DX10/11 writes this out as DX10 extension
			}

			// No DXGI format maps to ISBITMASK(0x0f,0x00,0x00,0xf0) aka D3DFMT_A4L4

			if (ISBITMASK(0x000000ff, 0x00000000, 0x00000000, 0x0000ff00))
			{
				return DXGI_FORMAT_R8G8_UNORM; // Some DDS writers assume the bitcount should be 8 instead of 16
			}
		}

		if (16 == ddpf.RGBBitCount)
		{
			if (ISBITMASK(0x0000ffff, 0x00000000, 0x00000000, 0x00000000))
			{
				return DXGI_FORMAT_R16_UNORM; // D3DX10/11 writes this out as DX10 extension
			}
			if (ISBITMASK(0x000000ff, 0x00000000, 0x00000000, 0x0000ff00))
			{
				return DXGI_FORMAT_R8G8_UNORM; // D3DX10/11 writes this out as DX10 extension
			}
		}
	}
	else if (ddpf.flags & DDS_ALPHA)
	{
		if (8 == ddpf.RGBBitCount)
		{
			return DXGI_FORMAT_A8_UNORM;
		}
	}
	else if (ddpf.flags & DDS_BUMPDUDV)
	{
		if (16 == ddpf.RGBBitCount)
		{
			if (ISBITMASK(0x00ff, 0xff00, 0x0000, 0x0000))
			{
				return DXGI_FORMAT_R8G8_SNORM; // D3DX10/11 writes this out as DX10 extension
			}
		}

		if (32 == ddpf.RGBBitCount)
		{
			if (ISBITMASK(0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000))
			{
				return DXGI_FORMAT_R8G8B8A8_SNORM; // D3DX10/11 writes this out as DX10 extension
			}
			if (ISBITMASK(0x0000ffff, 0xffff0000, 0x00000000, 0x00000000))
			{
				return DXGI_FORMAT_R16G16_SNORM; // D3DX10/11 writes this out as DX10 extension
			}

			// No DXGI format maps to ISBITMASK(0x3ff00000, 0x000ffc00, 0x000003ff, 0xc0000000) aka D3DFMT_A2W10V10U10
		}
	}
	else if (ddpf.flags & DDS_FOURCC)
	{
		if (MAKEFOURCC('D', 'X', 'T', '1') == ddpf.fourCC)
		{
			return DXGI_FORMAT_BC1_UNORM;
		}
		if (MAKEFOURCC('D', 'X', 'T', '3') == ddpf.fourCC)
		{
			return DXGI_FORMAT_BC2_UNORM;
		}
		if (MAKEFOURCC('D', 'X', 'T', '5') == ddpf.fourCC)
		{
			return DXGI_FORMAT_BC3_UNORM;
		}

		// While pre-multiplied alpha isn't directly supported by the DXGI formats,
		// they are basically the same as these BC formats so they can be mapped
		if (MAKEFOURCC('D', 'X', 'T', '2') == ddpf.fourCC)
		{
			return DXGI_FORMAT_BC2_UNORM;
		}
		if (MAKEFOURCC('D', 'X', 'T', '4') == ddpf.fourCC)
		{
			return DXGI_FORMAT_BC3_UNORM;
		}

		if (MAKEFOURCC('A', 'T', 'I', '1') == ddpf.fourCC)
		{
			return DXGI_FORMAT_BC4_UNORM;
		}
		if (MAKEFOURCC('B', 'C', '4', 'U') == ddpf.fourCC)
		{
			return DXGI_FORMAT_BC4_UNORM;
		}
		if (MAKEFOURCC('B', 'C', '4', 'S') == ddpf.fourCC)
		{
			return DXGI_FORMAT_BC4_SNORM;
		}

		if (MAKEFOURCC('A', 'T', 'I', '2') == ddpf.fourCC)
		{
			return DXGI_FORMAT_BC5_UNORM;
		}
		if (MAKEFOURCC('B', 'C', '5', 'U') == ddpf.fourCC)
		{
			return DXGI_FORMAT_BC5_UNORM;
		}
		if (MAKEFOURCC('B', 'C', '5', 'S') == ddpf.fourCC)
		{
			return DXGI_FORMAT_BC5_SNORM;
		}

		// BC6H and BC7 are written using the "DX10" extended header

		if (MAKEFOURCC('R', 'G', 'B', 'G') == ddpf.fourCC)
		{
			return DXGI_FORMAT_R8G8_B8G8_UNORM;
		}
		if (MAKEFOURCC('G', 'R', 'G', 'B') == ddpf.fourCC)
		{
			return DXGI_FORMAT_G8R8_G8B8_UNORM;
		}

		if (MAKEFOURCC('Y', 'U', 'Y', '2') == ddpf.fourCC)
		{
			return DXGI_FORMAT_YUY2;
		}

		// Check for D3DFORMAT enums being set here
		switch (ddpf.fourCC)
		{
		case 36: // D3DFMT_A16B16G16R16
			return DXGI_FORMAT_R16G16B16A16_UNORM;

		case 110: // D3DFMT_Q16W16V16U16
			return DXGI_FORMAT_R16G16B16A16_SNORM;

		case 111: // D3DFMT_R16F
			return DXGI_FORMAT_R16_FLOAT;

		case 112: // D3DFMT_G16R16F
			return DXGI_FORMAT_R16G16_FLOAT;

		case 113: // D3DFMT_A16B16G16R16F
			return DXGI_FORMAT_R16G16B16A16_FLOAT;

		case 114: // D3DFMT_R32F
			return DXGI_FORMAT_R32_FLOAT;

		case 115: // D3DFMT_G32R32F
			return DXGI_FORMAT_R32G32_FLOAT;

		case 116: // D3DFMT_A32B32G32R32F
			return DXGI_FORMAT_R32G32B32A32_FLOAT;
		}
	}

	return DXGI_FORMAT_UNKNOWN;
}

DDS::DXGI_FORMAT DDS::MakeSRGB(DXGI_FORMAT format)
{
	switch (format)
	{
	case DXGI_FORMAT_R8G8B8A8_UNORM:
		return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

	case DXGI_FORMAT_BC1_UNORM:
		return DXGI_FORMAT_BC1_UNORM_SRGB;

	case DXGI_FORMAT_BC2_UNORM:
		return DXGI_FORMAT_BC2_UNORM_SRGB;

	case DXGI_FORMAT_BC3_UNORM:
		return DXGI_FORMAT_BC3_UNORM_SRGB;

	case DXGI_FORMAT_B8G8R8A8_UNORM:
		return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;

	case DXGI_FORMAT_B8G8R8X8_UNORM:
		return DXGI_FORMAT_B8G8R8X8_UNORM_SRGB;

	case DXGI_FORMAT_BC7_UNORM:
		return DXGI_FORMAT_BC7_UNORM_SRGB;

	default:
		return format;
	}
}

void DDS::FillInitData(size_t width, size_t height, size_t depth, unsigned mipCount, unsigned arraySize, DXGI_FORMAT format, size_t maxsize, size_t bitSize, const uint8_t* bitData)
{
	sub_resources.resize(mipCount * arraySize);

	const uint8_t* pSrcBits = bitData;
	const uint8_t* pEndBits = bitData + bitSize;

	size_t index = 0;

	for (unsigned j = 0; j < arraySize; j++)
	{
		size_t w = width;
		size_t h = height;
		size_t d = depth;

		for (unsigned i = 0; i < mipCount; i++)
		{
			size_t bpe = 0;
			size_t numBlocksWide = 0;
			size_t numBlocksHigh = 0;
			size_t NumBytes = 0;
			size_t RowBytes = 0;
			size_t NumRows = 0;
			size_t BlockWidth = 0;
			size_t BlockHeight = 0;

			GetSurfaceInfo(w, h, format, &bpe, &numBlocksWide, &numBlocksHigh, &NumBytes, &RowBytes, &NumRows, &BlockWidth, &BlockHeight); // TODO: Use Mip.

			NumBytes *= d;

			if ((mipCount <= 1) || !maxsize || (w <= maxsize && h <= maxsize && d <= maxsize))
			{
				sub_resources[index].data = pSrcBits;
				sub_resources[index].size = NumBytes;
				sub_resources[index].row_size = RowBytes;
				sub_resources[index].width = (unsigned)w;
				sub_resources[index].height = (unsigned)h;
				sub_resources[index].depth = (unsigned)d;
				sub_resources[index].block_width = (unsigned)BlockWidth;
				sub_resources[index].block_height = (unsigned)BlockHeight;

				++index;
			}

			if (pSrcBits + NumBytes > pEndBits)
				throw std::runtime_error("EOF");

			pSrcBits += NumBytes;

			w = w >> 1;
			h = h >> 1;
			d = d >> 1;
			if (w == 0)
			{
				w = 1;
			}
			if (h == 0)
			{
				h = 1;
			}
			if (d == 0)
			{
				d = 1;
			}
		}
	}
}
