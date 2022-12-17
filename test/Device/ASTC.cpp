#include "ASTC.h"

#ifdef WIN32
#if DEBUG
#pragma comment( lib, "astcenc-sse2-libd.lib" )
#else
#pragma comment( lib, "astcenc-sse2-lib.lib" )
#endif

namespace ASTC
{
	namespace
	{
		unsigned int GetBlockModeWidth(BlockMode mode)
		{
			switch (mode)
			{
				case BlockMode::Mode_4x4: return 4;
				case BlockMode::Mode_5x4: return 5;
				case BlockMode::Mode_5x5: return 5;
				case BlockMode::Mode_6x5: return 6;
				case BlockMode::Mode_6x6: return 6;
				case BlockMode::Mode_8x5: return 8;
				case BlockMode::Mode_8x6: return 8;
				case BlockMode::Mode_8x8: return 8;
				case BlockMode::Mode_10x5: return 10;
				case BlockMode::Mode_10x6: return 10;
				case BlockMode::Mode_10x8: return 10;
				case BlockMode::Mode_10x10: return 10;
				case BlockMode::Mode_12x10: return 12;
				case BlockMode::Mode_12x12: return 12;
				default: throw ASTCException("Unknown ASTC Block Mode");
			}
		}

		unsigned int GetBlockModeHeight(BlockMode mode)
		{
			switch (mode)
			{
				case BlockMode::Mode_4x4: return 4;
				case BlockMode::Mode_5x4: return 4;
				case BlockMode::Mode_5x5: return 5;
				case BlockMode::Mode_6x5: return 5;
				case BlockMode::Mode_6x6: return 6;
				case BlockMode::Mode_8x5: return 5;
				case BlockMode::Mode_8x6: return 6;
				case BlockMode::Mode_8x8: return 8;
				case BlockMode::Mode_10x5: return 5;
				case BlockMode::Mode_10x6: return 6;
				case BlockMode::Mode_10x8: return 8;
				case BlockMode::Mode_10x10: return 10;
				case BlockMode::Mode_12x10: return 10;
				case BlockMode::Mode_12x12: return 12;
				default: throw ASTCException("Unknown ASTC Block Mode");
			}
		}

		astcenc_profile GetProfile(Profile profile)
		{
			switch (profile)
			{
				case Profile::LDR_SRGB: return ASTCENC_PRF_LDR_SRGB;
				case Profile::LDR: return ASTCENC_PRF_LDR;
				case Profile::HDR: return ASTCENC_PRF_HDR;
				default: throw ASTCException("Unknown ASTC Profile");
			}
		}

		bool IsActiveSwizzle(astcenc_swz swz)
		{
			return swz == ASTCENC_SWZ_R || swz == ASTCENC_SWZ_G || swz == ASTCENC_SWZ_B || swz == ASTCENC_SWZ_A;
		}

		size_t ActiveSwizzleCount(const astcenc_swizzle& swizzle)
		{
			size_t res = 0;
			if (IsActiveSwizzle(swizzle.r)) res++;
			if (IsActiveSwizzle(swizzle.g)) res++;
			if (IsActiveSwizzle(swizzle.b)) res++;
			if (IsActiveSwizzle(swizzle.a)) res++;
			return res;
		}

		template<typename T> T& GetTexelNoPadding(T*** dst, unsigned int x, unsigned int y, unsigned int c) { return dst[0][y][(4 * x) + c]; }
		template<typename T> T& GetTexel(T*** dst, unsigned int x, unsigned int y, unsigned int c, const astcenc_image& astcenc) { return GetTexelNoPadding(dst, x + astcenc.dim_pad, y + astcenc.dim_pad, c); }

		template<typename T> void FillPadding(T*** dst, const astcenc_image& astcenc)
		{
			if (astcenc.dim_pad == 0)
				return;

			// Top Padding
			for (unsigned int y = 0; y < astcenc.dim_pad; y++)
			{
				// Left Corner
				for (unsigned int x = 0; x < astcenc.dim_pad; x++)
					for (unsigned int c = 0; c < 4; c++)
						GetTexelNoPadding(dst, x, y, c) = GetTexel(dst, 0, 0, c, astcenc);

				// Middle
				for (unsigned int x = 0; x < astcenc.dim_x; x++)
					for (unsigned int c = 0; c < 4; c++)
						GetTexelNoPadding(dst, x + astcenc.dim_pad, y, c) = GetTexel(dst, x, 0, c, astcenc);

				// Right Corner
				for (unsigned int x = 0; x < astcenc.dim_pad; x++)
					for (unsigned int c = 0; c < 4; c++)
						GetTexelNoPadding(dst, x + astcenc.dim_pad + astcenc.dim_x, y, c) = GetTexel(dst, astcenc.dim_x - 1, 0, c, astcenc);
			}

			// Bottom Padding
			for (unsigned int y = 0; y < astcenc.dim_pad; y++)
			{
				// Left Corner
				for (unsigned int x = 0; x < astcenc.dim_pad; x++)
					for (unsigned int c = 0; c < 4; c++)
						GetTexelNoPadding(dst, x, y + astcenc.dim_pad + astcenc.dim_y, c) = GetTexel(dst, 0, astcenc.dim_y - 1, c, astcenc);

				// Middle
				for (unsigned int x = 0; x < astcenc.dim_x; x++)
					for (unsigned int c = 0; c < 4; c++)
						GetTexelNoPadding(dst, x + astcenc.dim_pad, y + astcenc.dim_pad + astcenc.dim_y, c) = GetTexel(dst, x, astcenc.dim_y - 1, c, astcenc);

				// Right Corner
				for (unsigned int x = 0; x < astcenc.dim_pad; x++)
					for (unsigned int c = 0; c < 4; c++)
						GetTexelNoPadding(dst, x + astcenc.dim_pad + astcenc.dim_x, y + astcenc.dim_pad + astcenc.dim_y, c) = GetTexel(dst, astcenc.dim_x - 1, astcenc.dim_y - 1, c, astcenc);
			}

			// Left Padding
			for (unsigned int x = 0; x < astcenc.dim_pad; x++)
			{
				for (unsigned y = 0; y < astcenc.dim_y; y++)
				{
					for (unsigned int c = 0; c < 4; c++)
						GetTexelNoPadding(dst, x, y + astcenc.dim_pad, c) = GetTexel(dst, 0, y, c, astcenc);
				}
			}

			// Right Padding
			for (unsigned int x = 0; x < astcenc.dim_pad; x++)
			{
				for (unsigned y = 0; y < astcenc.dim_y; y++)
				{
					for (unsigned int c = 0; c < 4; c++)
						GetTexelNoPadding(dst, x + astcenc.dim_pad + astcenc.dim_x, y + astcenc.dim_pad, c) = GetTexel(dst, astcenc.dim_x - 1, y, c, astcenc);
				}
			}
		}

		template<typename T> void CopyMip(T*** dst, const Device::Mip& dds, const astcenc_image& astcenc, unsigned int numDstChannels, unsigned int numSrcChannels = 0)
		{
			if (numSrcChannels == 0)
				numSrcChannels = numDstChannels;

			const T* src = reinterpret_cast<const T*>(dds.data);
			const auto block_height = (dds.height + dds.block_height - 1) / dds.block_height;
			const auto block_width = (dds.width + dds.block_width - 1) / dds.block_width;
			for (unsigned int by = 0; by < block_height; by++)
			{
				for (unsigned int bx = 0; bx < block_width; bx++)
				{
					for (unsigned int iy = 0; iy < dds.block_height; iy++)
					{
						const auto y = iy + (by * dds.block_height);
						if (y >= dds.height)
							continue;

						for (unsigned int ix = 0; ix < dds.block_width; ix++)
						{
							const auto x = ix + (bx * dds.block_width);
							if (x >= dds.width)
								continue;

							const auto idx = numSrcChannels * ((by * dds.row_size / (sizeof(T) * numSrcChannels)) + (bx * dds.block_width * dds.block_height) + (iy * dds.block_width) + ix);
							for (unsigned int c = 0; c < numDstChannels; c++)
								GetTexel(dst, x, y, c, astcenc) = src[(unsigned int)(idx + c)];

							for (unsigned int c = numDstChannels; c < 4; c++)
								GetTexel(dst, x, y, c, astcenc) = 0;
						}
					}
				}
			}
		}

		template<typename T> void MoveForPadding(T*** dst, const astcenc_image& astcenc)
		{
			if (astcenc.dim_pad == 0)
				return;

			for (unsigned int y = astcenc.dim_y; y > 0; y--)
			{
				for (unsigned int x = astcenc.dim_x; x > 0; x--)
				{
					for (unsigned int c = 0; c < 4; c++)
						GetTexel(dst, x - 1, y - 1, c, astcenc) = GetTexelNoPadding(dst, x - 1, y - 1, c);
				}
			}

			FillPadding(dst, astcenc);
		}
	}

	Context::Context(BlockMode mode, Profile profile)
	{
		astcenc_config c;
		//TODO: Flags and their usage:
		//	- For Color images, we should use ASTCENC_FLG_USE_ALPHA_WEIGHT and maybe ASTCENC_FLG_USE_PERCEPTUAL?
		//	- For Normal Maps, we should use ASTCENC_FLG_MAP_NORMAL and ASTCENC_FLG_USE_PERCEPTUAL
		//	- For masking textures, we should use ASTCENC_FLG_MAP_MASK
		unsigned int flags = 0;
		auto err = astcenc_config_init(GetProfile(profile), GetBlockModeWidth(mode), GetBlockModeHeight(mode), 1, ASTCENC_PRE_FAST, flags, c);
		if (err != ASTCENC_SUCCESS)
			throw ASTCException("Error when initializing ASTC config: " + std::string(astcenc_get_error_string(err)));

		err = astcenc_context_alloc(c, 1, &astcenc);
		if (err != ASTCENC_SUCCESS)
			throw ASTCException("Error when initializing ASTC: " + std::string(astcenc_get_error_string(err)));

		padding = std::max(c.v_rgba_radius, c.a_scale_radius);
		blocks_x = c.block_x;
		blocks_y = c.block_y;
		blocks_z = c.block_z;
	}

	Context::~Context()
	{
		astcenc_context_free(astcenc);
	}

	uint8_t*** Image::AllocateU8()
	{
		auto r = Allocate<uint8_t>();
		astcenc.data_type = ASTCENC_TYPE_U8;
		astcenc.data = r;
		return r;
	}

	uint16_t*** Image::AllocateF16()
	{
		auto r = Allocate<uint16_t>();
		astcenc.data_type = ASTCENC_TYPE_F16;
		astcenc.data = r;
		return r;
	}

	float*** Image::AllocateF32()
	{
		auto r = Allocate<float>();
		astcenc.data_type = ASTCENC_TYPE_F32;
		astcenc.data = r;
		return r;
	}

#define INIT_SWIZZLE(R, G, B, A) do { swizzle.r = ASTCENC_SWZ_##R; swizzle.g = ASTCENC_SWZ_##G; swizzle.b = ASTCENC_SWZ_##B; swizzle.a = ASTCENC_SWZ_##A; } while(false)
	void Image::LoadImage_R8G8B8A8_UNORM(const Device::Mip& dds)
	{
		auto dst = AllocateU8();
		INIT_SWIZZLE(R, G, B, A);
		CopyMip(dst, dds, astcenc, 4);
		FillPadding(dst, astcenc);
	}

	void Image::LoadImage_B8G8R8A8_UNORM(const Device::Mip& dds)
	{
		auto dst = AllocateU8();
		INIT_SWIZZLE(B, G, R, A);
		CopyMip(dst, dds, astcenc, 4);
		FillPadding(dst, astcenc);
	}

	void Image::LoadImage_B8G8R8X8_UNORM(const Device::Mip& dds)
	{
		auto dst = AllocateU8();
		INIT_SWIZZLE(B, G, R, 1);
		CopyMip(dst, dds, astcenc, 3, 4);
		FillPadding(dst, astcenc);
	}

	void Image::LoadImage_R8_UNORM(const Device::Mip& dds)
	{
		auto dst = AllocateU8();
		INIT_SWIZZLE(R, 0, 0, 1);
		CopyMip(dst, dds, astcenc, 1);
		FillPadding(dst, astcenc);
	}

	void Image::LoadImage_R8G8_UNORM(const Device::Mip& dds)
	{
		auto dst = AllocateU8();
		INIT_SWIZZLE(R, G, 0, 1);
		CopyMip(dst, dds, astcenc, 2);
		FillPadding(dst, astcenc);
	}

	void Image::LoadImage_R16_FLOAT(const Device::Mip& dds)
	{
		auto dst = AllocateF16();
		INIT_SWIZZLE(R, 0, 0, 1);
		CopyMip(dst, dds, astcenc, 1);
		FillPadding(dst, astcenc);
	}

	void Image::LoadImage_R32G32B32A32_FLOAT(const Device::Mip& dds)
	{
		auto dst = AllocateF32();
		INIT_SWIZZLE(R, G, B, A);
		CopyMip(dst, dds, astcenc, 4);
		FillPadding(dst, astcenc);
	}

	void Image::LoadImage_R32_FLOAT(const Device::Mip& dds)
	{
		auto dst = AllocateF32();
		INIT_SWIZZLE(R, 0, 0, 1);
		CopyMip(dst, dds, astcenc, 1);
		FillPadding(dst, astcenc);
	}

	void Image::LoadImage_R16G16_FLOAT(const Device::Mip& dds)
	{
		auto dst = AllocateF16();
		INIT_SWIZZLE(R, G, 0, 1);
		CopyMip(dst, dds, astcenc, 2);
		FillPadding(dst, astcenc);
	}

	void Image::LoadImage_R32G32_FLOAT(const Device::Mip& dds)
	{
		auto dst = AllocateF32();
		INIT_SWIZZLE(R, G, 0, 1);
		CopyMip(dst, dds, astcenc, 2);
		FillPadding(dst, astcenc);
	}

	Image::Image(const Context& config, unsigned int width, unsigned int height, Device::PixelFormat format)
	{
		astcenc.dim_x = width;
		astcenc.dim_y = height;
		astcenc.dim_z = 1;
		astcenc.dim_pad = config.padding;

		switch (format)
		{
			case Device::PixelFormat::A8R8G8B8:			AllocateU8();	INIT_SWIZZLE(R, G, B, A);	break;
			case Device::PixelFormat::A8B8G8R8:			AllocateU8();	INIT_SWIZZLE(B, G, R, A);	break;
			case Device::PixelFormat::X8R8G8B8:			AllocateU8();	INIT_SWIZZLE(B, G, R, 1);	break;
			case Device::PixelFormat::L8:				AllocateU8();	INIT_SWIZZLE(R, 0, 0, 1);	break;
			case Device::PixelFormat::A8L8:				AllocateU8();	INIT_SWIZZLE(R, G, 0, 1);	break;
			case Device::PixelFormat::R16F:				AllocateF16();	INIT_SWIZZLE(R, 0, 0, 1);	break;
			case Device::PixelFormat::A32B32G32R32F:	AllocateF32();	INIT_SWIZZLE(R, G, B, A);	break;
			case Device::PixelFormat::R32F:				AllocateF32();	INIT_SWIZZLE(R, 0, 0, 1);	break;
			case Device::PixelFormat::G16R16F:			AllocateF16();	INIT_SWIZZLE(R, G, 0, 1);	break;
			case Device::PixelFormat::G32R32F:			AllocateF32();	INIT_SWIZZLE(R, G, 0, 1);	break;
			default:									throw ASTCException("Unsupported Pixel Format");
		}
	}

	Image::Image(const Context& config, const Device::Mip& mip, Device::PixelFormat format)
	{
		astcenc.dim_x = mip.width;
		astcenc.dim_y = mip.height;
		astcenc.dim_z = 1;
		astcenc.dim_pad = config.padding;

		switch (format)
		{
			case Device::PixelFormat::A8R8G8B8:			LoadImage_R8G8B8A8_UNORM(mip);			break;
			case Device::PixelFormat::A8B8G8R8:			LoadImage_B8G8R8A8_UNORM(mip);			break;
			case Device::PixelFormat::X8R8G8B8:			LoadImage_B8G8R8X8_UNORM(mip);			break;
			case Device::PixelFormat::L8:				LoadImage_R8_UNORM(mip);				break;
			case Device::PixelFormat::A8L8:				LoadImage_R8G8_UNORM(mip);				break;
			case Device::PixelFormat::R16F:				LoadImage_R16_FLOAT(mip);				break;
			case Device::PixelFormat::A32B32G32R32F:	LoadImage_R32G32B32A32_FLOAT(mip);		break;
			case Device::PixelFormat::R32F:				LoadImage_R32_FLOAT(mip);				break;
			case Device::PixelFormat::G16R16F:			LoadImage_R16G16_FLOAT(mip);			break;
			case Device::PixelFormat::G32R32F:			LoadImage_R32G32_FLOAT(mip);			break;
			default:									throw ASTCException("Unsupported Pixel Format");
		}
	}

	Image::Image(const Context& config, const DDS& dds, size_t mipMap, size_t arrayIndex) 
		: Image(config, dds.GetSubResources()[(arrayIndex * dds.GetMipCount()) + mipMap], dds.GetPixelFormat())
	{}

	simd::vector4 Image::GetPixel(unsigned int x, unsigned int y) const
	{
		simd::vector4 result = simd::vector4(0.0f, 0.0f, 0.0f, 0.0f);
		if (x < astcenc.dim_x && y < astcenc.dim_y)
		{
			switch (astcenc.data_type)
			{
				case ASTCENC_TYPE_U8:
				{
					auto*** data = static_cast<const uint8_t***>(astcenc.data);
					for (unsigned int c = 0; c < 4; c++)
						result[c] = static_cast<float>(GetTexel(data, x, y, c, astcenc)) / 255.0f;
				}
				break;

				case ASTCENC_TYPE_F16:
				{
					auto*** data = static_cast<const uint16_t***>(astcenc.data);
					for (unsigned int c = 0; c < 4; c++)
						result[c] = simd::half(GetTexel(data, x, y, c, astcenc)).tofloat();
				}
				break;

				case ASTCENC_TYPE_F32:
				{
					auto*** data = static_cast<const float***>(astcenc.data);
					for (unsigned int c = 0; c < 4; c++)
						result[c] = GetTexel(data, x, y, c, astcenc);
				}
				break;

				default:
					break;
			}
		}

		return result;
	}

	void DecompressImage(const Context& context, const uint8_t* data, size_t data_len, Image& image)
	{
		auto err = astcenc_decompress_image(context.astcenc, data, data_len, image.GetImage(), image.GetSwizzle());
		if (err != ASTCENC_SUCCESS)
			throw ASTCException("Failed to decompress image: " + std::string(astcenc_get_error_string(err)));

		// Decompression does not take padding into account, so we move the pixels according to the padding, such that the Image object is always in a valid state
		// This will end up as a no-op in most cases, as padding is usually zero
		switch (image.GetImage().data_type)
		{
			case ASTCENC_TYPE_U8:
			{
				uint8_t*** data = static_cast<uint8_t***>(image.GetImage().data);
				MoveForPadding(data, image.GetImage());
			}
			break;

			case ASTCENC_TYPE_F16:
			{
				uint16_t*** data = static_cast<uint16_t***>(image.GetImage().data);
				MoveForPadding(data, image.GetImage());
			}
			break;

			case ASTCENC_TYPE_F32:
			{
				float*** data = static_cast<float***>(image.GetImage().data);
				MoveForPadding(data, image.GetImage());
			}
			break;
		}
	}
}

#endif