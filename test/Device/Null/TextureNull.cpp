
namespace Device
{
	class TextureNull : public Texture
	{
		IDevice* device = nullptr;
		PixelFormat pixel_format = PixelFormat::UNKNOWN;
		bool use_srgb = false;
		bool use_mipmaps = false;

		TextureNull(const Memory::DebugStringA<>& name, IDevice* device, bool srgb);

	public:
		TextureNull(const Memory::DebugStringA<>& name, IDevice* device, UINT Width, UINT Height, UINT Levels, UsageHint Usage, PixelFormat Format, Pool Pool, bool useSRGB, bool use_ESRAM, bool use_mipmaps, bool enable_gpu_write);
		TextureNull(const Memory::DebugStringA<>& name, IDevice* device, UINT Width, UINT Height, unsigned char* pixels, UINT bytes_per_pixel);
		TextureNull(const Memory::DebugStringA<>& name, IDevice* device, LPCWSTR pSrcFile, UINT width, UINT height, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool, TextureFilter filter, TextureFilter mipFilter, simd::color colorKey, ImageInfo* pSrcInfo, PaletteEntry* outPalette);
		TextureNull(const Memory::DebugStringA<>& name, IDevice* device, LPCVOID pSrcData, UINT srcDataSize, UINT width, UINT height, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool, TextureFilter filter, TextureFilter mipFilter, simd::color colorKey, ImageInfo* pSrcInfo, PaletteEntry* outPalette, bool useSRGB, bool premultiply_alpha);
		TextureNull(const Memory::DebugStringA<>& name, IDevice* device, UINT edgeLength, UINT levels, UsageHint usage, PixelFormat format, Pool pool, bool useSRGB, bool enable_gpu_write);
		TextureNull(const Memory::DebugStringA<>& name, IDevice* device, LPCWSTR pSrcFile);
		TextureNull(const Memory::DebugStringA<>& name, IDevice* device, LPCWSTR pSrcFile, UINT size, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool, TextureFilter filter, TextureFilter mipFilter, simd::color colorKey, ImageInfo* outSrcInfo, PaletteEntry* outPalette);
		TextureNull(const Memory::DebugStringA<>& name, IDevice* device, LPCVOID pSrcData, UINT srcDataSize, UINT size, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool, TextureFilter filter, TextureFilter mipFilter, simd::color colorKey, ImageInfo* pSrcInfo, PaletteEntry* outPalette, bool useSRGB);
		TextureNull(const Memory::DebugStringA<>& name, IDevice* device, LPCVOID pSrcData, UINT srcDataSize, UINT width, UINT height, UINT depth, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool, TextureFilter filter, TextureFilter mipFilter, simd::color colorKey, ImageInfo* pSrcInfo, PaletteEntry* outPalette, bool useSRGB);
		TextureNull(const Memory::DebugStringA<>& name, IDevice* device, UINT width, UINT height, UINT depth, UINT levels, UsageHint usage, PixelFormat format, Pool pool, bool useSRGB, bool enable_gpu_write);
		TextureNull(const Memory::DebugStringA<>& name, IDevice* device, LPCWSTR pSrcFile, UINT width, UINT height, UINT depth, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool, TextureFilter filter, TextureFilter mipFilter, simd::color colorKey, ImageInfo* outSrcInfo, PaletteEntry* outPalette);

		virtual void LockRect(UINT level, LockedRect* outLockedRect, Lock lock) final;
		virtual void UnlockRect(UINT level) final;
		virtual void LockBox(UINT level, LockedBox* outLockedVolume, Lock lock) final;
		virtual void UnlockBox(UINT level) final;

		virtual void GetLevelDesc(UINT level, SurfaceDesc* outDesc) final;
		virtual size_t GetMipsCount() final { return 0; }

		virtual void Fill(void* data, unsigned pitch) final;
		virtual void CopyResource(Texture* texture) final;
		virtual void SaveToFile(LPCWSTR pDestFile, ImageFileFormat DestFormat) final;

		virtual bool UseSRGB() const final { return use_srgb; }
		virtual bool UseMipmaps() const final { return use_mipmaps; }

		void SetDebugName(const char* pDebugName);

		PixelFormat GetPixelFormat() const { return pixel_format; }
	};

	TextureNull::TextureNull(const Memory::DebugStringA<>& name, IDevice* device, bool srgb)
		: Texture(name)
		, device(device)
		, use_srgb(srgb)
	{
	}

	TextureNull::TextureNull(const Memory::DebugStringA<>& name, IDevice* device, UINT Width, UINT Height, UINT Levels, UsageHint Usage, PixelFormat Format, Pool pool, bool useSRGB, bool use_ESRAM, bool use_mipmaps, bool enable_gpu_write)
		: TextureNull(name, device, useSRGB)
	{
	}

	TextureNull::TextureNull(const Memory::DebugStringA<>& name, IDevice* device, UINT Width, UINT Height, unsigned char* pixels, UINT bytes_per_pixel)
		: TextureNull(name, device, false)
	{
	}

	TextureNull::TextureNull(const Memory::DebugStringA<>& name, IDevice* device, LPCWSTR pSrcFile, UINT width, UINT height, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool, TextureFilter filter, TextureFilter mipFilter, simd::color colorKey, ImageInfo* pSrcInfo, PaletteEntry* outPalette)
		: TextureNull(name, device, true)
	{
	}

	TextureNull::TextureNull(const Memory::DebugStringA<>& name, IDevice* device, LPCVOID pSrcData, UINT srcDataSize, UINT width, UINT height, UINT mipLevels, UsageHint usage,
		PixelFormat format, Pool pool, TextureFilter filter, TextureFilter mipFilter, simd::color colorKey, ImageInfo* pSrcInfo, PaletteEntry* outPalette, bool useSRGB, bool premultiply_alpha)
		: TextureNull(name, device, useSRGB)
	{
	}

	TextureNull::TextureNull(const Memory::DebugStringA<>& name, IDevice* device, UINT edgeLength, UINT levels, UsageHint usage, PixelFormat format, Pool pool, bool useSRGB, bool enable_gpu_write)
		: TextureNull(name, device, useSRGB)
	{
	}

	TextureNull::TextureNull(const Memory::DebugStringA<>& name, IDevice* device, LPCWSTR pSrcFile)
		: TextureNull(name, device, true)
	{
	}

	TextureNull::TextureNull(const Memory::DebugStringA<>& name, IDevice* device, LPCWSTR pSrcFile, UINT size, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool, TextureFilter filter, TextureFilter mipFilter, simd::color colorKey, ImageInfo* outSrcInfo, PaletteEntry* outPalette)
		: TextureNull(name, device, true)
	{
	}

	TextureNull::TextureNull(const Memory::DebugStringA<>& name, IDevice* device, LPCVOID pSrcData, UINT srcDataSize, UINT size, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool, TextureFilter filter, TextureFilter mipFilter,
		simd::color colorKey, ImageInfo* pSrcInfo, PaletteEntry* outPalette, bool useSRGB)
		: TextureNull(name, device, useSRGB)
	{
	}

	TextureNull::TextureNull(const Memory::DebugStringA<>& name, IDevice* device, LPCVOID pSrcData, UINT srcDataSize, UINT width, UINT height, UINT depth, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool,
		TextureFilter filter, TextureFilter mipFilter, simd::color colorKey, ImageInfo* pSrcInfo, PaletteEntry* outPalette, bool useSRGB)
		: TextureNull(name, device, useSRGB)
	{
	}

	TextureNull::TextureNull(const Memory::DebugStringA<>& name, IDevice* device, UINT width, UINT height, UINT depth, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool, bool useSRGB, bool enable_gpu_write)
		: TextureNull(name, device, useSRGB)
	{
	}

	TextureNull::TextureNull(const Memory::DebugStringA<>& name, IDevice* device, LPCWSTR pSrcFile, UINT width, UINT height, UINT depth, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool, TextureFilter filter, TextureFilter mipFilter, simd::color colorKey, ImageInfo* outSrcInfo, PaletteEntry* outPalette)
		: TextureNull(name, device, true)
	{
	}

	void TextureNull::LockRect(UINT level, LockedRect* outLockedRect, Lock lock)
	{
		outLockedRect->pBits = nullptr;
		outLockedRect->Pitch = 0;
	}

	void TextureNull::UnlockRect(UINT level)
	{
	}

	void TextureNull::LockBox(UINT level, LockedBox* outLockedVolume, Lock lock)
	{
		outLockedVolume->pBits = nullptr;
		outLockedVolume->RowPitch = 0;
		outLockedVolume->SlicePitch = 0;
	}

	void TextureNull::UnlockBox(UINT level)
	{
	}

	void TextureNull::Fill(void* data, unsigned pitch)
	{
	}

	void TextureNull::CopyResource(Texture* pSrcResource)
	{
	}

	void TextureNull::GetLevelDesc(UINT level, SurfaceDesc* outDesc)
	{
	}

	void TextureNull::SaveToFile(LPCWSTR pDestFile, ImageFileFormat DestFormat)
	{
	}

}
