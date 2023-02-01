#pragma once

#include <array>
#include <list>

#include "Visual/Device/Constants.h" // TODO: Remove.
#include "Visual/Device/Resource.h" 

namespace simd
{
	class color;
}

namespace Device
{
	class IDevice;
	class Texture;

	enum class Pool : unsigned;
	enum class ImageFileFormat : unsigned;

	const UINT SizeDefault = (UINT)-1;
	const UINT SizeDefaultNonPow2 = (UINT)-2;
	const UINT SizeDefaultFromFile = (UINT)-3;

	enum class Lock : unsigned
	{
		DEFAULT,
		READONLY,
		DISCARD,
		NOOVERWRITE,
	};

	constexpr DWORD MakePixelFormat(char ch0, char ch1, char ch2, char ch3)
	{
		return (DWORD)(BYTE)ch0 | ((DWORD)(BYTE)ch1 << 8) | ((DWORD)(BYTE)ch2 << 16) | ((DWORD)(BYTE)ch3 << 24);
	}

	enum class Dimension : unsigned
	{
		One,
		Two,
		Three,
		Cubemap,
	};
	
	enum class PixelFormat : unsigned
	{
		// IMPORTANT: Matches D3D9 D3DFMT_XXX values (exported value).
		FROM_FILE = (unsigned)-3,
		UNKNOWN = 0,
		R8G8B8 = 20,
		A8R8G8B8 = 21,
		X8R8G8B8 = 22,
		R5G6B5 = 23,
		X1R5G5B5 = 24,
		A1R5G5B5 = 25,
		A4R4G4B4 = 26,
		R3G3B2 = 27,
		A8 = 28,
		A8R3G3B2 = 29,
		X4R4G4B4 = 30,
		A2B10G10R10 = 31,
		A8B8G8R8 = 32,
		X8B8G8R8 = 33,
		G16R16 = 34,
		A2R10G10B10 = 35,
		A16B16G16R16 = 36,
		A8P8 = 40,
		P8 = 41,
		L8 = 50,
		A8L8 = 51,
		A4L4 = 52,
		V8U8 = 60,
		L6V5U5 = 61,
		X8L8V8U8 = 62,
		Q8W8V8U8 = 63,
		V16U16 = 64,
		A2W10V10U10 = 67,
		UYVY = MakePixelFormat('U', 'Y', 'V', 'Y'),
		R8G8_B8G8 = MakePixelFormat('R', 'G', 'B', 'G'),
		YUY2 = MakePixelFormat('Y', 'U', 'Y', '2'),
		G8R8_G8B8 = MakePixelFormat('G', 'R', 'G', 'B'),
		D16_LOCKABLE = 70,
		D32 = 71,
		D15S1 = 73,
		D24S8 = 75,
		D24X8 = 77,
		D24X4S4 = 79,
		D16 = 80,
		D32F_LOCKABLE = 82,
		D24FS8 = 83,
		L16 = 81,
		VERTEXDATA = 100,
		INDEX16 = 101,
		INDEX32 = 102,
		Q16W16V16U16 = 110,
		MULTI2_ARGB8 = MakePixelFormat('M', 'E', 'T', '1'),
		R16F = 111,
		G16R16F = 112,
		A16B16G16R16F = 113,
		R32F = 114,
		G32R32F = 115,
		A32B32G32R32F = 116,
		CxV8U8 = 117,
		R11G11B10F = 118,
		DF24 = MakePixelFormat('D', 'F', '2', '4'),
		DF16 = MakePixelFormat('D', 'F', '1', '6'),

		// NOTE: Doesn't match D3D9 D3DFMT_XXX values.
		// Can use any value not used in D3D9Types.
		G16R16_UINT = 200,

		ASTC_4x4 = 201,
		ASTC_6x6 = 202,
		ASTC_8x8 = 203,
		R16 = 204,

		BC1 = 205,
		BC2_Premultiplied = 206,
		BC2 = 207,
		BC3 = 208,
		BC4 = 209,
		BC5 = 210,
		BC6UF = 211,
		BC7_Premultiplied = 212,
		BC7 = 213,
	};

	enum class UsageHint : unsigned
	{
		DEFAULT = 0,
		RENDERTARGET = 0x1,
		DEPTHSTENCIL = 0x2,
		IMMUTABLE = 0x4,
		DYNAMIC = 0x8,
		WRITEONLY = 0x10,
		QUERY_VERTEXTEXTURE = 0x20,
		QUERY_POSTPIXELSHADER_BLENDING = 0x40,
		ATOMIC_COUNTER = 0x80,
		BUFFERED = 0x100, // Used in Vulkan to enable remote buffering.
		STAGING = 0x200,
	};

	enum class TextureFilter : unsigned
	{
		NONE				= 1 << 0,
		POINT				= 1 << 1,
		LINEAR				= 1 << 2,
		TRIANGLE			= 1 << 3,
		BOX					= 1 << 4,
		MIRROR_U			= 1 << 5,
		MIRROR_V			= 1 << 6,
		MIRROR_W			= 1 << 7,
		MIRROR				= 1 << 8,
		DITHER				= 1 << 9,
		DITHER_DIFFUSION	= 1 << 10,
		SRGB_IN				= 1 << 11,
		SRGB_OUT			= 1 << 12,
		SRGB				= 1 << 13,
	};

	enum class TexResourceFormat : unsigned
	{
		BC1,
		BC4,
		BC5,
		BC7_Premultiplied,
		BC7,
		R16,
		R16G16,
		Uncompressed,
		NumTexResourceFormat
	};

	struct PaletteEntry
	{
		BYTE peRed;
		BYTE peGreen;
		BYTE peBlue;
		BYTE peFlags;
	};

	struct Rect;

	struct LockedRect
	{
		INT                 Pitch;
		void*               pBits;
	};

	struct LockedBox
	{
		INT                 RowPitch;
		INT                 SlicePitch;
		void*               pBits;
	};

	struct ImageInfo
	{
		UINT                    Width;
		UINT                    Height;
		UINT                    Depth;
		UINT                    MipLevels;
		PixelFormat             Format;
		ImageFileFormat			ImageFileFormat;
	};

	struct SurfaceDesc
	{
		PixelFormat			Format;
		UINT                Width;
		UINT                Height;
	};

	struct Mip
	{
		const uint8_t* data = nullptr;

		size_t size = 0;
		size_t row_size = 0;
	
		unsigned width = 0;
		unsigned height = 0;
		unsigned depth = 0;
	
		unsigned block_width = 0;
		unsigned block_height = 0;
	};

	inline UsageHint FrameUsageHint() // For resources written/uploaded every frame.
	{
		return (Device::UsageHint)((unsigned)Device::UsageHint::DYNAMIC | (unsigned)Device::UsageHint::WRITEONLY | (unsigned)Device::UsageHint::BUFFERED);
	}

	inline bool IsCompressedFormat(PixelFormat format)
	{
		switch (format)
		{
			case PixelFormat::BC1:
			case PixelFormat::BC2_Premultiplied:
			case PixelFormat::BC2:
			case PixelFormat::BC3:
			case PixelFormat::BC4:
			case PixelFormat::BC5:
			case PixelFormat::BC6UF:
			case PixelFormat::BC7_Premultiplied:
			case PixelFormat::BC7:
			case PixelFormat::ASTC_4x4:
			case PixelFormat::ASTC_6x6:
			case PixelFormat::ASTC_8x8:
				return true;
			default:
				return false;
		}
	}

	inline unsigned MaxSkipAxis(unsigned size)
	{
		unsigned skip = 0;
		while (size > 16)
		{
			size >>= 1;
			skip++;
		}
		return skip;
	}

	inline void SkipMips(unsigned& width, unsigned& height, unsigned& mip_start, unsigned& mip_count, TextureFilter mipFilter, PixelFormat format)
	{
		if (width < 64 || height < 64) // No skip if one axis is too small.
			return;

		const auto max_skip_mip = mip_count > 6u ? mip_count - 6u : 0u; // Min 64 pixels size.
		const auto max_skip_axis = std::min(MaxSkipAxis(width), MaxSkipAxis(height)); // Limit skip to min 16 pixels on one side.
		const auto max_skip = std::min(max_skip_mip, max_skip_axis);
		const auto asked_skip = unsigned((size_t)mipFilter >> 26);
		auto skip = std::min(max_skip, asked_skip);
		if (skip == 0)
			return;

		if (IsCompressedFormat(format)) // resulting size of compressed formats must be multiple of DXT block size
			while ((width >> skip) % 4 != 0 || (height >> skip) % 4 != 0)
				if (--skip == 0)
					return;

		mip_start += unsigned(skip);
		mip_count -= unsigned(skip);
		width >>= skip;
		height >>= skip;
		width = std::max(1u, width);
		height = std::max(1u, height);
	}

	class Texture : public Resource
	{
		static inline std::atomic_uint count = { 0 };

	protected:
		Texture(const Memory::DebugStringA<>& name) : Resource(name) { count++; }

	public:
		virtual ~Texture() { count--; }

		static Handle<Texture> CreateTexture(const Memory::DebugStringA<>& name, IDevice* device, UINT Width, UINT Height, UINT Levels, UsageHint Usage, PixelFormat Format, Pool Pool, bool useSRGB, bool use_ESRAM, bool use_mipmaps, bool enable_gpu_write = false);
		static Handle<Texture> CreateTextureFromMemory(const Memory::DebugStringA<>& name, IDevice* device, UINT Width, UINT Height, unsigned char* pixels, UINT bytes_per_pixel);
		static Handle<Texture> CreateTextureFromFileEx(const Memory::DebugStringA<>& name, IDevice* device, LPCWSTR pSrcFile, UINT width, UINT height, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool, TextureFilter filter, TextureFilter mipFilter, simd::color colorKey, ImageInfo* pSrcInfo, PaletteEntry* outPalette);
		static Handle<Texture> CreateTextureFromFileInMemoryEx(const Memory::DebugStringA<>& name, IDevice* device, LPCVOID pSrcData, UINT srcDataSize, UINT width, UINT height, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool, TextureFilter filter, TextureFilter mipFilter, simd::color colorKey, ImageInfo* pSrcInfo, PaletteEntry* outPalette, bool useSRGB, bool premultiply_alpha);
		static Handle<Texture> CreateCubeTexture(const Memory::DebugStringA<>& name, IDevice* device, UINT edgeLength, UINT levels, UsageHint usage, PixelFormat format, Pool pool, bool useSRGB, bool enable_gpu_write = false);
		static Handle<Texture> CreateCubeTextureFromFile(const Memory::DebugStringA<>& name, IDevice* device, LPCWSTR pSrcFile);
		static Handle<Texture> CreateCubeTextureFromFileEx(const Memory::DebugStringA<>& name, IDevice* device, LPCWSTR pSrcFile, UINT size, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool, TextureFilter filter, TextureFilter mipFilter, simd::color colorKey, ImageInfo* outSrcInfo, PaletteEntry* outPalette);
		static Handle<Texture> CreateCubeTextureFromFileInMemoryEx(const Memory::DebugStringA<>& name, IDevice* device, LPCVOID pSrcData, UINT srcDataSize, UINT size, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool, TextureFilter filter, TextureFilter mipFilter, simd::color colorKey, ImageInfo* pSrcInfo, PaletteEntry* outPalette, bool useSRGB);
		static Handle<Texture> CreateVolumeTextureFromFileInMemoryEx(const Memory::DebugStringA<>& name, IDevice* device, LPCVOID pSrcData, UINT srcDataSize, UINT width, UINT height, UINT depth, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool, TextureFilter filter, TextureFilter mipFilter, simd::color colorKey, ImageInfo* pSrcInfo, PaletteEntry* outPalette, bool useSRGB);
		static Handle<Texture> CreateVolumeTexture(const Memory::DebugStringA<>& name, IDevice* device, UINT width, UINT height, UINT depth, UINT levels, UsageHint usage, PixelFormat format, Pool pool, bool useSRGB, bool enable_gpu_write = false);
		static Handle<Texture> CreateVolumeTextureFromFileEx(const Memory::DebugStringA<>& name, IDevice* device, LPCWSTR pSrcFile, UINT width, UINT height, UINT depth, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool, TextureFilter filter, TextureFilter mipFilter, simd::color colorKey, ImageInfo* outSrcInfo, PaletteEntry* outPalette);

		static unsigned GetCount() { return count.load(); }
		static bool IsValidTextureMagic(uint32_t magic);

		virtual void LockRect(UINT level, LockedRect* outLockedRect, Lock lock) = 0;
		virtual void UnlockRect(UINT level) = 0;
		virtual void LockBox(UINT level, LockedBox *outLockedVolume, Lock lock) = 0;
		virtual void UnlockBox(UINT level) = 0;

		virtual void GetLevelDesc(UINT level, SurfaceDesc *outDesc) = 0;
		virtual size_t GetMipsCount() = 0;

		virtual void Fill(void* data, unsigned pitch) = 0;
		virtual void TransferFrom(Texture* src_texture) = 0;
		virtual void TranscodeFrom(Texture* src_texture) = 0;
		virtual void SaveToFile(LPCWSTR pDestFile, ImageFileFormat DestFormat) = 0;

		virtual bool UseSRGB() const = 0;
		virtual bool UseMipmaps() const = 0;
	};

	constexpr unsigned GetBitsPerPixel(PixelFormat format)
	{
		switch (format)
		{
			case PixelFormat::UNKNOWN:				return 0;
			case PixelFormat::A8:					return 8;
			case PixelFormat::L8:					return 8;
			case PixelFormat::A8L8:					return 16;
			case PixelFormat::R16:					return 16;
			case PixelFormat::R16F:					return 16;
			case PixelFormat::R32F:					return 32;
			case PixelFormat::G16R16:				return 32;
			case PixelFormat::G16R16_UINT:			return 32;
			case PixelFormat::G16R16F:				return 32;
			case PixelFormat::G32R32F:				return 64;
			case PixelFormat::A16B16G16R16:			return 64;
			case PixelFormat::A16B16G16R16F:		return 64;
			case PixelFormat::A32B32G32R32F:		return 128;
			case PixelFormat::R11G11B10F:			return 32;
			case PixelFormat::A8R8G8B8:				return 32;
			case PixelFormat::A8B8G8R8:				return 32;
			case PixelFormat::X8R8G8B8:				return 32;
			case PixelFormat::BC1:					return 4;
			case PixelFormat::BC2_Premultiplied:	return 8;
			case PixelFormat::BC2:					return 8;
			case PixelFormat::BC3:					return 8;
			case PixelFormat::BC4:					return 4;		
			case PixelFormat::BC5:					return 8;
			case PixelFormat::BC6UF:				return 8;
			case PixelFormat::BC7_Premultiplied:	return 8;
			case PixelFormat::BC7:					return 8;
			case PixelFormat::D32:					return 32;
			case PixelFormat::D24S8:				return 32;
			case PixelFormat::D24X8:				return 32;
			default:								return 0;
		}
	}

	constexpr bool IsCompressed(PixelFormat format)
	{
		switch (format)
		{
			case PixelFormat::BC1:
			case PixelFormat::BC2_Premultiplied:
			case PixelFormat::BC2:
			case PixelFormat::BC3:
			case PixelFormat::BC4:
			case PixelFormat::BC5:
			case PixelFormat::BC6UF:
			case PixelFormat::BC7_Premultiplied:
			case PixelFormat::BC7:
				return true;
			default:		
				return false;
		}
	}

	constexpr TextureFilter SkipMipLevels(unsigned levels, TextureFilter filter)
	{ 
		return (Device::TextureFilter)( 
			(unsigned)(1 << 25) | // Safe to skip mips.
			(unsigned)( (levels & 0x1F) << 26) | // Mips count to skip.
			(unsigned)(filter) ); // Actual filter.
	}
}
