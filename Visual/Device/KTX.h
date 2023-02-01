#pragma once

#include "Texture.h"

class KTX
{
	#define KTX_IDENTIFIER_REF  { 0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31, 0x31, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A }
	#define KTX_ENDIAN_REF      (0x04030201)
	#define KTX_HEADER_SIZE		(64)
	
	#define GL_RGBA   0x1908
	#define GL_COMPRESSED_RGBA_ASTC_4x4_KHR   0x93B0
	#define GL_COMPRESSED_RGBA_ASTC_5x4_KHR   0x93B1
	#define GL_COMPRESSED_RGBA_ASTC_5x5_KHR   0x93B2
	#define GL_COMPRESSED_RGBA_ASTC_6x5_KHR   0x93B3
	#define GL_COMPRESSED_RGBA_ASTC_6x6_KHR   0x93B4
	#define GL_COMPRESSED_RGBA_ASTC_8x5_KHR   0x93B5
	#define GL_COMPRESSED_RGBA_ASTC_8x6_KHR   0x93B6
	#define GL_COMPRESSED_RGBA_ASTC_8x8_KHR   0x93B7
	#define GL_COMPRESSED_RGBA_ASTC_10x5_KHR  0x93B8
	#define GL_COMPRESSED_RGBA_ASTC_10x6_KHR  0x93B9
	#define GL_COMPRESSED_RGBA_ASTC_10x8_KHR  0x93BA
	#define GL_COMPRESSED_RGBA_ASTC_10x10_KHR 0x93BB
	#define GL_COMPRESSED_RGBA_ASTC_12x10_KHR 0x93BC
	#define GL_COMPRESSED_RGBA_ASTC_12x12_KHR 0x93BD
	#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR 0x93D0
	#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR 0x93D1
	#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR 0x93D2
	#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR 0x93D3
	#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR 0x93D4
	#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR 0x93D5
	#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR 0x93D6
	#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR 0x93D7
	#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR 0x93D8
	#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR 0x93D9
	#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR 0x93DA
	#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR 0x93DB
	#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR 0x93DC
	#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR 0x93DD

	struct KTXHeader
	{
		uint8_t  identifier[12];
		uint32_t endianness;
		uint32_t glType;
		uint32_t glTypeSize;
		uint32_t glFormat;
		uint32_t glInternalFormat;
		uint32_t glBaseInternalFormat;
		uint32_t pixelWidth;
		uint32_t pixelHeight;
		uint32_t pixelDepth;
		uint32_t numberOfArrayElements;
		uint32_t numberOfFaces;
		uint32_t numberOfMipmapLevels;
		uint32_t bytesOfKeyValueData;
	};
	static_assert(sizeof(KTXHeader) == KTX_HEADER_SIZE);

	KTXHeader header;

	unsigned width = 0;
	unsigned height = 0;
	unsigned depth = 0;
	unsigned array_count = 1;
	unsigned mip_count = 0;

	size_t pixels_size = 0;

	Device::Dimension dimension = Device::Dimension::Two;
	Device::PixelFormat pixel_format = Device::PixelFormat::UNKNOWN;

	std::vector<Device::Mip> mips;
	
	Device::PixelFormat ConvertPixelFormat(unsigned gl_format);
	Device::Dimension ConvertDimension(const KTXHeader& header);

public:
	static inline std::array<uint8_t, 12> MAGIC_FULL = KTX_IDENTIFIER_REF;
	static const uint32_t MAGIC = 0x58544bab; // Beginning of KTX_IDENTIFIER_REF.

	KTX(const uint8_t* data, const size_t size);

	unsigned GetWidth() const { return width; }
	unsigned GetHeight() const { return height; }
	unsigned GetDepth() const { return depth; }
	unsigned GetArrayCount() const { return array_count; }
	unsigned GetMipCount() const { return mip_count; }

	size_t GetImageSize() const { return pixels_size; }

	Device::Dimension GetDimension() const { return dimension; }
	Device::PixelFormat GetPixelFormat() const { return pixel_format; }

	const std::vector<Device::Mip>& GetMips() const { return mips; }

	void SkipMip();

	std::vector<char> Write();
};
