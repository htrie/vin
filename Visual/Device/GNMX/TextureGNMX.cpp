
namespace Device
{

	static OwnerGNMX texture_owner("Texture");

	
	Gnm::TextureType ConvertDimension(Device::Dimension dimension)
	{
		switch (dimension)
		{
		case Dimension::One:		return Gnm::kTextureType1d;
		case Dimension::Two:		return Gnm::kTextureType2d;
		case Dimension::Three:		return Gnm::kTextureType3d;
		case Dimension::Cubemap:	return Gnm::kTextureTypeCubemap;
		default:					throw ExceptionGNMX("Unsupported");
		}
	}

	Gnm::DataFormat ConvertPixelFormat(PixelFormat format, bool useSRGB)
	{
		switch (format)
		{
		case PixelFormat::UNKNOWN:				return Gnm::kDataFormatInvalid;
		case PixelFormat::A8:					return Gnm::kDataFormatA8Unorm;
		case PixelFormat::L8:					return Gnm::kDataFormatL8Unorm;
		case PixelFormat::A8L8:					return Gnm::kDataFormatR8G8Unorm;
		case PixelFormat::R16:					return Gnm::kDataFormatR16Unorm;
		case PixelFormat::R16F:					return Gnm::kDataFormatR16Float;
		case PixelFormat::R32F:					return Gnm::kDataFormatR32Float;
		case PixelFormat::G16R16:				return Gnm::kDataFormatR16G16Unorm;
		case PixelFormat::G16R16_UINT:			return Gnm::kDataFormatR16G16Uint;
		case PixelFormat::G16R16F:				return Gnm::kDataFormatR16G16Float;
		case PixelFormat::G32R32F:				return Gnm::kDataFormatR32G32Float;
		case PixelFormat::A16B16G16R16:			return Gnm::kDataFormatR16G16B16A16Unorm;
		case PixelFormat::A16B16G16R16F:		return Gnm::kDataFormatR16G16B16A16Float;
		case PixelFormat::A32B32G32R32F:		return Gnm::kDataFormatR32G32B32A32Float;
		case PixelFormat::R11G11B10F:			return Gnm::kDataFormatR11G11B10Float;
		case PixelFormat::A8R8G8B8:				return useSRGB ? Gnm::kDataFormatR8G8B8A8UnormSrgb : Gnm::kDataFormatR8G8B8A8Unorm;
		case PixelFormat::A8B8G8R8:				return useSRGB ? Gnm::kDataFormatB8G8R8A8UnormSrgb : Gnm::kDataFormatB8G8R8A8Unorm;
		case PixelFormat::X8R8G8B8:				return useSRGB ? Gnm::kDataFormatB8G8R8X8UnormSrgb : Gnm::kDataFormatB8G8R8X8Unorm;
		case PixelFormat::BC1:					return useSRGB ? Gnm::kDataFormatBc1UnormSrgb : Gnm::kDataFormatBc1Unorm;
		case PixelFormat::BC2_Premultiplied:	return useSRGB ? Gnm::kDataFormatBc2UnormSrgb : Gnm::kDataFormatBc2Unorm;
		case PixelFormat::BC2:					return useSRGB ? Gnm::kDataFormatBc2UnormSrgb : Gnm::kDataFormatBc2Unorm;
		case PixelFormat::BC3:					return useSRGB ? Gnm::kDataFormatBc3UnormSrgb : Gnm::kDataFormatBc3Unorm;
		case PixelFormat::BC4:					return Gnm::kDataFormatBc4Unorm;
		case PixelFormat::BC5:					return Gnm::kDataFormatBc5Unorm;
		case PixelFormat::BC6UF:				return Gnm::kDataFormatBc6Uf16;
		case PixelFormat::BC7_Premultiplied:	return useSRGB ? Gnm::kDataFormatBc7UnormSrgb : Gnm::kDataFormatBc7Unorm;
		case PixelFormat::BC7:					return useSRGB ? Gnm::kDataFormatBc7UnormSrgb : Gnm::kDataFormatBc7Unorm;
		case PixelFormat::D32:					return Gnm::DataFormat::build(Gnm::kZFormat32Float);
		case PixelFormat::D24S8:				return Gnm::DataFormat::build(Gnm::kZFormat32Float);
		case PixelFormat::D24X8:				return Gnm::DataFormat::build(Gnm::kZFormat32Float);
		default:								throw ExceptionGNMX("Unsupported");
		}
	}

	Gnm::GpuMode GetGpuMode()
	{
		return sceKernelIsNeoMode() ? Gnm::kGpuModeNeo : Gnm::kGpuModeBase;
	}

	Memory::Tag DetermineTag(UsageHint usage_hint)
	{
		const bool render_target = ((unsigned)usage_hint & (unsigned)UsageHint::RENDERTARGET) > 0;
		const bool depth_stencil = ((unsigned)usage_hint & (unsigned)UsageHint::DEPTHSTENCIL) > 0;
		return render_target || depth_stencil ? Memory::Tag::RenderTarget : Memory::Tag::Texture;
	}

	GpuAddress::SurfaceType DetermineSurfaceType(UsageHint usage_hint)
	{
		const bool dynamic = ((unsigned)usage_hint & (unsigned)UsageHint::DYNAMIC) > 0;
		const bool write_only = ((unsigned)usage_hint & (unsigned)UsageHint::WRITEONLY) > 0;
		const bool render_target = ((unsigned)usage_hint & (unsigned)UsageHint::RENDERTARGET) > 0;
		const bool depth_stencil = ((unsigned)usage_hint & (unsigned)UsageHint::DEPTHSTENCIL) > 0;
		
		if (render_target) return GpuAddress::kSurfaceTypeColorTarget;
		if (depth_stencil) return GpuAddress::kSurfaceTypeDepthTarget;
		if ((!dynamic && !write_only)) return GpuAddress::kSurfaceTypeTextureFlat;
		return GpuAddress::kSurfaceTypeRwTextureFlat;
	}
	
	Gnm::TileMode AutoTileMode(UsageHint usage_hint, Gnm::DataFormat data_format)
	{
		Gnm::TileMode tile_mode;
		const auto surface_type = DetermineSurfaceType(usage_hint);
		auto status = GpuAddress::computeSurfaceTileMode(GetGpuMode(), &tile_mode, surface_type, data_format, 1);
		if (status != GpuAddress::kStatusSuccess)
			throw ExceptionGNMX("Failed to determine tile mode");
		return tile_mode;
	}

	Gnm::TileMode DetermineTileMode(UsageHint usage_hint, Pool pool, Gnm::DataFormat data_format)
	{
		const bool force_linear = (pool == Pool::MANAGED_WITH_SYSTEMMEM);
		const bool dynamic = ((unsigned)usage_hint & (unsigned)UsageHint::DYNAMIC) > 0;
		const bool write_only = ((unsigned)usage_hint & (unsigned)UsageHint::WRITEONLY) > 0;
		const bool render_target = ((unsigned)usage_hint & (unsigned)UsageHint::RENDERTARGET) > 0;

		if (render_target && dynamic && write_only) // Backbuffer. // TODO: Better way to distinguish backbuffer.
			return Gnm::kTileModeDisplay_2dThin;
		if (force_linear || dynamic || write_only)
			return Gnm::kTileModeDisplay_LinearAligned;
		return AutoTileMode(usage_hint, data_format);
	}

	static unsigned CountMips(unsigned width, unsigned height)
	{
		unsigned count = 0;
		while ((width > 0) && (height > 0))
		{
			count++;
			if ((width == 1) && (height == 1))
				break;
			width = width > 1 ? width / 2 : 1;
			height = height > 1 ? height / 2 : 1;
		}
		return count;
	}


	class TextureGNMX : public Texture, public ResourceGNMX
	{
		IDevice* device = nullptr;
		PixelFormat pixel_format = PixelFormat::UNKNOWN;
		bool use_srgb = false;
		bool use_mipmaps = false;

		RenderTarget* aliased_render_target = nullptr; // Used when render-to-texture.
		Gnm::SizeAlign size_align;
		std::array<AllocationGNMX, BackBufferCount> datas;
		std::array<Gnm::Texture, BackBufferCount> textures;
		std::vector<Gnm::Texture> textures_mips;
		std::vector<Gnm::Texture> textures_mips_single;
		unsigned count = 0;
		unsigned current = 0;
		unsigned mips_count = 0;

		uint64_t frame_index = (uint64_t)-1;

		Gnm::Texture InitTexture(unsigned Width, unsigned Height, unsigned Depth, unsigned Levels, UsageHint Usage, Pool pool, PixelFormat Format, Device::Dimension dimension);
		void InitFullTextures(unsigned Width, unsigned Height, unsigned Depth, unsigned Levels, UsageHint Usage, Pool pool, PixelFormat Format, Device::Dimension dimension, bool enable_gpu_write);
		void InitMipsTextures(unsigned Width, unsigned Height, unsigned Depth, unsigned Levels, UsageHint Usage, Pool pool, PixelFormat Format, Device::Dimension dimension);

		void CopyTiled(const std::vector<Mip>& sub_resources, unsigned array_count, unsigned mip_start, unsigned mip_count);

		void CreateFromMemoryDDS(const void* pSrcData, size_t srcDataSize, UsageHint Usage, Pool pool, TextureFilter mipFilter, bool premultiply_alpha);
		void CreateFromMemoryPNG(const void* pSrcData, size_t srcDataSize, UsageHint Usage, Pool pool, TextureFilter mipFilter, bool premultiply_alpha);
		void CreateFromMemoryJPG(const void* pSrcData, size_t srcDataSize, UsageHint Usage, Pool pool, TextureFilter mipFilter);

		void Create(unsigned Width, unsigned Height, unsigned Depth, unsigned Levels, UsageHint Usage, Pool pool, PixelFormat Format, Device::Dimension dimension, bool mipmaps, bool enable_gpu_write);
		void CreateFromMemory(const void* pSrcData, size_t srcDataSize, UsageHint Usage, Pool pool, TextureFilter mipFilter, ImageInfo* out_info, bool premultiply_alpha);
		void CreateFromFile(const wchar_t* pSrcFile, UsageHint Usage, Pool pool, TextureFilter mipFilter, ImageInfo* out_info);
		void CreateDefault();

		TextureGNMX(const Memory::DebugStringA<>& name, IDevice* device, bool srgb);

	public:
		TextureGNMX(const Memory::DebugStringA<>& name, IDevice* device, UINT Width, UINT Height, UINT Levels, UsageHint Usage, PixelFormat Format, Pool Pool, bool useSRGB, bool use_ESRAM, bool use_mipmaps, bool enable_gpu_write);
		TextureGNMX(const Memory::DebugStringA<>& name, IDevice* device, UINT Width, UINT Height, unsigned char* pixels, UINT bytes_per_pixel);
		TextureGNMX(const Memory::DebugStringA<>& name, IDevice* device, LPCWSTR pSrcFile, UINT width, UINT height, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool, TextureFilter filter, TextureFilter mipFilter, simd::color colorKey, ImageInfo* pSrcInfo, PaletteEntry* outPalette);
		TextureGNMX(const Memory::DebugStringA<>& name, IDevice* device, LPCVOID pSrcData, UINT srcDataSize, UINT width, UINT height, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool, TextureFilter filter, TextureFilter mipFilter, simd::color colorKey, ImageInfo* pSrcInfo, PaletteEntry* outPalette, bool useSRGB, bool premultiply_alpha);
		TextureGNMX(const Memory::DebugStringA<>& name, IDevice* device, UINT edgeLength, UINT levels, UsageHint usage, PixelFormat format, Pool pool, bool useSRGB, bool enable_gpu_write);
		TextureGNMX(const Memory::DebugStringA<>& name, IDevice* device, LPCWSTR pSrcFile);
		TextureGNMX(const Memory::DebugStringA<>& name, IDevice* device, LPCWSTR pSrcFile, UINT size, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool, TextureFilter filter, TextureFilter mipFilter, simd::color colorKey, ImageInfo* outSrcInfo, PaletteEntry* outPalette);
		TextureGNMX(const Memory::DebugStringA<>& name, IDevice* device, LPCVOID pSrcData, UINT srcDataSize, UINT size, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool, TextureFilter filter, TextureFilter mipFilter, simd::color colorKey, ImageInfo* pSrcInfo, PaletteEntry* outPalette, bool useSRGB);
		TextureGNMX(const Memory::DebugStringA<>& name, IDevice* device, LPCVOID pSrcData, UINT srcDataSize, UINT width, UINT height, UINT depth, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool, TextureFilter filter, TextureFilter mipFilter, simd::color colorKey, ImageInfo* pSrcInfo, PaletteEntry* outPalette, bool useSRGB);
		TextureGNMX(const Memory::DebugStringA<>& name, IDevice* device, UINT width, UINT height, UINT depth, UINT levels, UsageHint usage, PixelFormat format, Pool pool, bool useSRGB, bool enable_gpu_write);
		TextureGNMX(const Memory::DebugStringA<>& name, IDevice* device, LPCWSTR pSrcFile, UINT width, UINT height, UINT depth, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool, TextureFilter filter, TextureFilter mipFilter, simd::color colorKey, ImageInfo* outSrcInfo, PaletteEntry* outPalette);

		virtual void LockRect(UINT level, LockedRect* outLockedRect, Lock lock) final;
		virtual void UnlockRect(UINT level) final;
		virtual void LockBox(UINT level, LockedBox* outLockedVolume, Lock lock) final;
		virtual void UnlockBox(UINT level) final;

		virtual void GetLevelDesc(UINT level, SurfaceDesc* outDesc) final;
		virtual size_t GetMipsCount() final { assert(use_mipmaps); return mips_count; }

		virtual void Fill(void* data, unsigned pitch) final;
		virtual void TransferFrom(Texture* src_texture) final;
		virtual void TranscodeFrom(Texture* src_texture) final;
		virtual void SaveToFile(LPCWSTR pDestFile, ImageFileFormat DestFormat) final;

		virtual bool UseSRGB() const final { return use_srgb; }
		virtual bool UseMipmaps() const final { return use_mipmaps; }

		unsigned GetWidth() const { return textures[current].getWidth(); }
		unsigned GetHeight() const { return textures[current].getHeight(); }

		Gnm::Texture& GetTexture() { return textures[current]; }
		Gnm::Texture& GetTexture(unsigned index) { return textures[index]; }

		void* GetData() const { return datas[current].GetData(); }
		int GetPitch() const { return  textures[0].getPitch(); }

		void Copy(TextureGNMX& dest);

		void SetAliasedRenderTarget(RenderTarget* render_target) { aliased_render_target = render_target; } // TODO: Find a better way.
		RenderTarget* GetAliasedRenderTarget() { return aliased_render_target; }
		
		void Swap();

		PixelFormat GetPixelFormat() const { return pixel_format; }

		Gnm::Texture& GetTextureMip(unsigned index) { assert(use_mipmaps); assert(index < textures_mips.size()); return textures_mips[index]; }
	};

	TextureGNMX::TextureGNMX(const Memory::DebugStringA<>& name, IDevice* device, bool srgb)
		: Texture(name)
		, ResourceGNMX(texture_owner.GetHandle())
		, device(device)
		, use_srgb(srgb)
	{
	}

	TextureGNMX::TextureGNMX(const Memory::DebugStringA<>& name, IDevice* device, UINT Width, UINT Height, UINT Levels, UsageHint Usage, PixelFormat Format, Pool pool, bool use_srgb, bool use_ESRAM, bool use_mipmaps, bool enable_gpu_write)
		: TextureGNMX(name, device, use_srgb)
	{
		Create(Width, Height, 1, Levels, Usage, pool, Format, Dimension::Two, use_mipmaps, enable_gpu_write);
	}

	TextureGNMX::TextureGNMX(const Memory::DebugStringA<>& name, IDevice* device, UINT Width, UINT Height, unsigned char* pixels, UINT bytes_per_pixel)
		: TextureGNMX(name, device, false)
	{
		Create(Width, Height, 1, 1, UsageHint::DEFAULT, Pool::DEFAULT, PixelFormat::A8R8G8B8, Dimension::Two, false, false);

		Mip mip = {};
		mip.data = pixels;
		CopyTiled({ mip }, 1, 0, 1);
	}

	TextureGNMX::TextureGNMX(const Memory::DebugStringA<>& name, IDevice* device, LPCWSTR pSrcFile, UINT width, UINT height, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool, TextureFilter filter, TextureFilter mipFilter, simd::color colorKey, ImageInfo* pSrcInfo, PaletteEntry* outPalette)
		: TextureGNMX(name, device, false)
	{
		CreateFromFile(pSrcFile, usage, pool, mipFilter, pSrcInfo);
	}

	TextureGNMX::TextureGNMX(const Memory::DebugStringA<>& name, IDevice* device, LPCVOID pSrcData, UINT srcDataSize, UINT width, UINT height, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool, TextureFilter filter, TextureFilter mipFilter, simd::color colorKey, ImageInfo* pSrcInfo, PaletteEntry* outPalette, bool use_srgb, bool premultiply_alpha)
		: TextureGNMX(name, device, use_srgb)
	{
		CreateFromMemory(pSrcData, srcDataSize, usage, pool, mipFilter, pSrcInfo, premultiply_alpha);
	}

	TextureGNMX::TextureGNMX(const Memory::DebugStringA<>& name, IDevice* device, UINT edgeLength, UINT levels, UsageHint usage, PixelFormat format, Pool pool, bool use_srgb, bool enable_gpu_write)
		: TextureGNMX(name, device, use_srgb)
	{
		Create(edgeLength, edgeLength, 1, levels, usage, pool, format, Dimension::Cubemap, false, enable_gpu_write);
	}

	TextureGNMX::TextureGNMX(const Memory::DebugStringA<>& name, IDevice* device, LPCWSTR pSrcFile)
		: TextureGNMX(name, device,	false)
	{
		CreateFromFile(pSrcFile, UsageHint::DEFAULT, Pool::DEFAULT, TextureFilter::NONE, nullptr);
	}

	TextureGNMX::TextureGNMX(const Memory::DebugStringA<>& name, IDevice* device, LPCWSTR pSrcFile, UINT size, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool, TextureFilter filter, TextureFilter mipFilter, simd::color colorKey, ImageInfo* outSrcInfo, PaletteEntry* outPalette)
		: TextureGNMX(name, device, false)
	{
		CreateFromFile(pSrcFile, usage, pool, mipFilter, outSrcInfo);
	}

	TextureGNMX::TextureGNMX(const Memory::DebugStringA<>& name, IDevice* device, LPCVOID pSrcData, UINT srcDataSize, UINT size, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool, TextureFilter filter, TextureFilter mipFilter, simd::color colorKey, ImageInfo* pSrcInfo, PaletteEntry* outPalette, bool use_srgb)
		: TextureGNMX(name, device, use_srgb)
	{
		CreateFromMemory(pSrcData, srcDataSize, usage, pool, mipFilter, pSrcInfo, false);
	}

	TextureGNMX::TextureGNMX(const Memory::DebugStringA<>& name, IDevice* device, LPCVOID pSrcData, UINT srcDataSize, UINT width, UINT height, UINT depth, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool, TextureFilter filter, TextureFilter mipFilter, simd::color colorKey, ImageInfo* pSrcInfo, PaletteEntry* outPalette, bool use_srgb)
		: TextureGNMX(name, device, use_srgb)
	{
		CreateFromMemory(pSrcData, srcDataSize, usage, pool, mipFilter, pSrcInfo, false);
	}

	TextureGNMX::TextureGNMX(const Memory::DebugStringA<>& name, IDevice* device, UINT width, UINT height, UINT depth, UINT levels, UsageHint usage, PixelFormat format, Pool pool, bool use_srgb, bool enable_gpu_write)
		: TextureGNMX(name, device, use_srgb)
	{
		Create(width, height, depth, levels, usage, pool, format, Dimension::Three, false, enable_gpu_write);
	}

	TextureGNMX::TextureGNMX(const Memory::DebugStringA<>& name, IDevice* device, LPCWSTR pSrcFile, UINT width, UINT height, UINT depth, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool, TextureFilter filter, TextureFilter mipFilter, simd::color colorKey, ImageInfo* outSrcInfo, PaletteEntry* outPalette)
		: TextureGNMX(name, device, false)
	{
		CreateFromFile(pSrcFile, usage, pool, mipFilter, outSrcInfo);
	}

	Gnm::Texture TextureGNMX::InitTexture(unsigned Width, unsigned Height, unsigned Depth, unsigned Levels, UsageHint Usage, Pool pool, PixelFormat Format, Device::Dimension dimension)
	{
		Gnm::TextureSpec spec;
		spec.init();
		spec.m_textureType = ConvertDimension(dimension);
		spec.m_width = Width;
		spec.m_height = Height;
		spec.m_depth = Depth;
		spec.m_pitch = 0;
		spec.m_numMipLevels = std::max((unsigned)1, Levels);
		spec.m_numSlices = 1;
		spec.m_format = ConvertPixelFormat(Format, use_srgb);
		spec.m_tileModeHint = DetermineTileMode(Usage, pool, spec.m_format);
		spec.m_minGpuMode = GetGpuMode();
		spec.m_numFragments = Gnm::kNumFragments1;

		if (spec.m_format.m_asInt == Gnm::kDataFormatInvalid.m_asInt)
			throw ExceptionGNMX("Invalid data format");

		if (!Gnmx::isDataFormatValid(spec.m_format))
			throw ExceptionGNMX("Malformed data format");

		if ((Usage == UsageHint::RENDERTARGET) && !spec.m_format.supportsRenderTarget())
			throw ExceptionGNMX("Data format doesn't support RenderTarget");
		if ((Usage == UsageHint::DEPTHSTENCIL) && !spec.m_format.supportsDepthRenderTarget())
			throw ExceptionGNMX("Data format doesn't support DepthRenderTarget");

		Gnm::Texture texture;
		auto status = texture.init(&spec);
		if (status != SCE_GNM_OK)
			throw ExceptionGNMX("Failed to init texture");

		return texture;
	}

	void TextureGNMX::InitFullTextures(unsigned Width, unsigned Height, unsigned Depth, unsigned Levels, UsageHint Usage, Pool pool, PixelFormat Format, Device::Dimension dimension, bool enable_gpu_write)
	{
		const bool dynamic = ((unsigned)Usage & (unsigned)UsageHint::DYNAMIC) > 0;
		count = dynamic ? BackBufferCount : 1; // TODO: If DISCARD more than once per frame, then need more than double-buffering.
		
		for (unsigned i = 0; i < count; ++i)
		{
			textures[i] = InitTexture(Width, Height, Depth, Levels, Usage, pool, Format, dimension);

			size_align = textures[i].getSizeAlign();

			datas[i].Init(DetermineTag(Usage), DetermineAllocationType(Usage, pool), size_align.m_size, size_align.m_align);

			textures[i].setBaseAddress(datas[i].GetData());
			textures[i].setResourceMemoryType(DetermineMemoryType(Usage, enable_gpu_write));

			Register(Gnm::kResourceTypeTextureBaseAddress, datas[i].GetData(), size_align.m_size, GetName().c_str());
		}
	}

	void TextureGNMX::InitMipsTextures(unsigned Width, unsigned Height, unsigned Depth, unsigned Levels, UsageHint Usage, Pool pool, PixelFormat Format, Device::Dimension dimension)
	{
		textures_mips.resize(mips_count);
		textures_mips_single.resize(mips_count);

		unsigned width = Width;
		unsigned height = Height;

		for (unsigned i = 0; i < mips_count; ++i)
		{
			textures_mips[i] = textures[0];
			textures_mips[i].setMipLevelRange(i, i);

			// Create full mip-chain metadata for each mip to be able to query pitch.
			textures_mips_single[i] = InitTexture(width, height, Depth, mips_count - i, Usage, pool, Format, dimension);

			width = std::max(1u, width / 2);
			height = std::max(1u, height / 2);
		}
	}

	void TextureGNMX::Create(unsigned Width, unsigned Height, unsigned Depth, unsigned Levels, UsageHint Usage, Pool pool, PixelFormat Format, Device::Dimension dimension, bool mipmaps, bool enable_gpu_write)
	{
		assert((Width > 0) && (Height > 0));

		pixel_format = Format;
		use_mipmaps = mipmaps;

		if (use_mipmaps)
		{
			mips_count = CountMips(Width, Height);
		}

		InitFullTextures(Width, Height, Depth, use_mipmaps ? mips_count : Levels, Usage, pool, Format, dimension, enable_gpu_write);

		if (use_mipmaps)
		{
			InitMipsTextures(Width, Height, Depth, mips_count, Usage, pool, Format, dimension);
		}
	}
		
	void TextureGNMX::CreateFromMemoryDDS(const void* pSrcData, size_t srcDataSize, UsageHint Usage, Pool pool, TextureFilter mipFilter, bool premultiply_alpha)
	{
		DDS dds((uint8_t*)pSrcData, srcDataSize);

		unsigned width = dds.GetWidth();
		unsigned height = dds.GetHeight();
		unsigned mip_start = 0;
		unsigned mip_count = dds.GetMipCount();
		SkipMips(width, height, mip_start, mip_count, mipFilter, dds.GetPixelFormat());

		Create(width, height, dds.GetDepth(), mip_count, Usage, pool, dds.GetPixelFormat(), dds.GetDimension(), false, false);

		if (premultiply_alpha)
		{
			// TODO: Premultiply if not compressed.
		}

		CopyTiled(dds.GetSubResources(), dds.GetArraySize(), mip_start, mip_count);
	}

	void TextureGNMX::CreateFromMemoryPNG(const void* pSrcData, size_t srcDataSize, UsageHint Usage, Pool pool, TextureFilter mipFilter, bool premultiply_alpha)
	{
		PNG png(GetName(), (uint8_t*)pSrcData, srcDataSize);

		const unsigned array_size = 1;
		const unsigned mip_start = 0;
		const unsigned mip_count = 1;

		Create(png.GetWidth(), png.GetHeight(), array_size, mip_count, Usage, pool, png.GetPixelFormat(), png.GetDimension(), false, false);

		if (premultiply_alpha)
		{
			auto ptr = reinterpret_cast< simd::color* >( png.GetImageData() );
			const auto end = ptr + (png.GetWidth() * png.GetHeight());
			for( ; ptr != end; ++ptr )
			{
				const double a = ptr->a / 255.0;
				ptr->r = static_cast< uint8_t >( ptr->r * a );
				ptr->g = static_cast< uint8_t >( ptr->g * a );
				ptr->b = static_cast< uint8_t >( ptr->b * a );
			}
		}

		Mip mip = {};
		mip.data = png.GetImageData();
		CopyTiled({ mip }, array_size, mip_start, mip_count);
	}

	void TextureGNMX::CreateFromMemoryJPG(const void* pSrcData, size_t srcDataSize, UsageHint Usage, Pool pool, TextureFilter mipFilter)
	{
		JPG jpg(GetName(), (uint8_t*)pSrcData, srcDataSize);

		const unsigned array_size = 1;
		const unsigned mip_start = 0;
		const unsigned mip_count = 1;

		Create(jpg.GetWidth(), jpg.GetHeight(), array_size, mip_count, Usage, pool, jpg.GetPixelFormat(), jpg.GetDimension(), false, false);

		Mip mip = {};
		mip.data = jpg.GetImageData();
		CopyTiled({ mip }, array_size, mip_start, mip_count);
	}

	void TextureGNMX::CreateFromMemory(const void* pSrcData, size_t srcDataSize, UsageHint Usage, Pool pool, TextureFilter mipFilter, ImageInfo* out_info, bool premultiply_alpha)
	{
		ImageFileFormat format;

		try
		{
			uint32_t magic = *(const uint32_t*)pSrcData;
			if (magic == DDS::MAGIC)
			{
				format = ImageFileFormat::DDS;
				CreateFromMemoryDDS( pSrcData, srcDataSize, Usage, pool, mipFilter, premultiply_alpha);
			}
			else if (magic == PNG::MAGIC)
			{
				format = ImageFileFormat::PNG;
				CreateFromMemoryPNG( pSrcData, srcDataSize, Usage, pool, mipFilter, premultiply_alpha);
			}
			else if ((magic & JPG::MAGIC_MASK) == JPG::MAGIC)
			{
				format = ImageFileFormat::JPG;
				CreateFromMemoryJPG(pSrcData, srcDataSize, Usage, pool, mipFilter);
			}
			else
			{
				throw std::runtime_error("Unrecognized texture format");
			}
		}
		catch (std::exception& e)
		{
			format = ImageFileFormat::PNG;
			CreateDefault();
			LOG_WARN(L"[VULKAN] Failed to load texture " << StringToWstring(GetName().c_str()) << L"(Error: " << StringToWstring(e.what()) << L")");
		}

		if (out_info)
		{
			out_info->Width = textures[0].getWidth();
			out_info->Height = textures[0].getHeight();
			out_info->Depth = textures[0].getDepth();
			out_info->MipLevels = 1 + textures[0].getLastMipLevel() - textures[0].getBaseMipLevel();
			out_info->Format = GetPixelFormat();
			out_info->ImageFileFormat = format;
		}
	}

	void TextureGNMX::CreateFromFile(const wchar_t* pSrcFile, UsageHint Usage, Pool pool, TextureFilter mipFilter, ImageInfo* out_info)
	{
		File::InputFileStream stream(pSrcFile);

		CreateFromMemory(stream.GetFilePointer(), stream.GetFileSize(), Usage, pool, mipFilter, out_info, false);
	}
	
	void TextureGNMX::CreateDefault()
	{
		const uint32_t black = 0;
		const unsigned array_size = 1;
		const unsigned mip_start = 0;
		const unsigned mip_count = 1;

		Create(1, 1, array_size, mip_count, UsageHint::DEFAULT, Pool::DEFAULT, PixelFormat::A8R8G8B8, Dimension::Two, false, false);

		Mip mip = {};
		mip.data = reinterpret_cast<const uint8_t*>(&black);
		CopyTiled({ mip }, array_size, mip_start, mip_count);
	}

	void TextureGNMX::CopyTiled(const std::vector<Mip>& sub_resources, unsigned array_count, unsigned mip_start, unsigned mip_count)
	{
		for (unsigned array_index = 0; array_index < array_count; ++array_index)
		{
			unsigned dst_mip_index = 0;

			for (unsigned mip_index = mip_start; mip_index < (mip_start + mip_count); ++mip_index)
			{
				uint64_t surface_offset = 0;
				uint64_t surface_size = 0;
				auto status = GpuAddress::computeTextureSurfaceOffsetAndSize(&surface_offset, &surface_size, &textures[0], dst_mip_index, array_index);
				if (status != GpuAddress::kStatusSuccess)
					throw ExceptionGNMX("Failed to get surface infos");

				GpuAddress::TilingParameters tiling_params;
				status = tiling_params.initFromTexture(&textures[0], dst_mip_index, array_index);
				if (status != GpuAddress::kStatusSuccess)
					throw ExceptionGNMX("Failed to init tiling params");

				status = GpuAddress::tileSurface(datas[0].GetData() + surface_offset, sub_resources[array_index * (mip_start + mip_count) + mip_index].data, &tiling_params);
				if (status != GpuAddress::kStatusSuccess)
					throw ExceptionGNMX("Failed to tile surface");

				dst_mip_index++;
			}
		}
	}

	void TextureGNMX::Copy(TextureGNMX& dest)
	{
		assert(GetPixelFormat() == dest.GetPixelFormat());
		assert(GetWidth() == dest.GetWidth());
		assert(GetHeight() == dest.GetHeight());
		assert(!use_mipmaps && !dest.use_mipmaps);

		uint64_t src_offset = 0;
		uint64_t src_size = 0;
		auto status = GpuAddress::computeTextureSurfaceOffsetAndSize(&src_offset, &src_size, &textures[0], 0, 0);
		if (status != GpuAddress::kStatusSuccess)
			throw ExceptionGNMX("Failed to get surface infos");

		uint64_t dst_offset = 0;
		uint64_t dst_size = 0;
		status = GpuAddress::computeTextureSurfaceOffsetAndSize(&dst_offset, &dst_size, &dest.textures[0], 0, 0);
		if (status != GpuAddress::kStatusSuccess)
			throw ExceptionGNMX("Failed to get surface infos");

		const auto src_is_linear = (textures[0].getTileMode() == Gnm::kTileModeDisplay_LinearAligned);
		const auto dst_is_linear = (dest.textures[0].getTileMode() == Gnm::kTileModeDisplay_LinearAligned);

		auto* src = datas[0].GetData() + src_offset;
		auto* dst = dest.datas[0].GetData() + dst_offset;

		if (src_is_linear && !dst_is_linear) // Linear to tiled: tile.
		{
			GpuAddress::TilingParameters tiling_params;
			status = tiling_params.initFromTexture(&dest.textures[0], 0, 0);
			if (status != GpuAddress::kStatusSuccess)
				throw ExceptionGNMX("Failed to init tiling params");

			status = GpuAddress::tileSurface(dst, src, &tiling_params);
			if (status != GpuAddress::kStatusSuccess)
				throw ExceptionGNMX("Failed to tile surface");
		}
		else if (!src_is_linear && dst_is_linear) // Tiled to linear: detile.
		{
			GpuAddress::TilingParameters tiling_params;
			status = tiling_params.initFromTexture(&textures[0], 0, 0);
			if (status != GpuAddress::kStatusSuccess)
				throw ExceptionGNMX("Failed to init tiling params");

			status = GpuAddress::detileSurface(dst, src, &tiling_params);
			if (status != GpuAddress::kStatusSuccess)
				throw ExceptionGNMX("Failed to tile surface");
		}
		else // All linear/tiled: just copy.
		{
			assert(src_size == dst_size);

			memcpy(dst, src, src_size);
		}
	}

	void TextureGNMX::Fill(void* data, unsigned pitch)
	{
		LockedRect rect;
		LockRect(0, &rect, Lock::DISCARD);

		if (rect.pBits)
		{
			uint8_t* src = (uint8_t*)data;
			uint8_t* dst = (uint8_t*)rect.pBits;
			const auto src_pitch = pitch;
			const auto dst_pitch = rect.Pitch;
			const auto height = GetHeight();

			for (unsigned j = 0; j < height; ++j)
			{
				memcpy(dst, src, dst_pitch);
				src += src_pitch;
				dst += dst_pitch;
			}
		}

		UnlockRect(0);
	}

	void TextureGNMX::Swap()
	{
		current = (current + 1) % count;
	}

	void TextureGNMX::LockRect(unsigned level, LockedRect* outLockedRect, Lock lock)
	{
		assert(level == 0 || level < mips_count && "LockRect level out-of-range");
		assert(level <= textures[0].getLastMipLevel() && "LockRect level out-of-range");
		assert(textures[0].getTileMode() == Gnm::kTileModeDisplay_LinearAligned && "LockRect requires linear tiling");
		
		if (lock == Device::Lock::DISCARD)
		{
		#if defined(DEVELOPMENT_CONFIGURATION)
			assert(frame_index != device->GetFrameIndex() && "Cannot lock more than once per frame");
			frame_index = device->GetFrameIndex();
		#endif
			assert(count > 1 && "Lock::DISCARD requires UsageHint::DYNAMIC");
			Swap();
		}

		uint64_t offset = 0;
		uint64_t size = 0;
		auto status = GpuAddress::computeTextureSurfaceOffsetAndSize(&offset, &size, &textures[0], level, 0);
		if (status != GpuAddress::kStatusSuccess)
			throw ExceptionGNMX("Failed to get surface infos");

		GpuAddress::TilingParameters tiling_params;
		status = tiling_params.initFromTexture(&textures[0], level, 0);
		if (status != GpuAddress::kStatusSuccess)
			throw ExceptionGNMX("Failed to init tiling params");

		const auto bytes_per_pixel = tiling_params.m_bitsPerFragment / 8;
		const auto mip_pitch = mips_count > 0 ? textures_mips_single[level].getPitch() : textures[0].getPitch();

		outLockedRect->pBits = (uint8_t*)GetData() + offset;
		outLockedRect->Pitch = mip_pitch * bytes_per_pixel;
	}

	void TextureGNMX::UnlockRect(unsigned level)
	{
	}

	void TextureGNMX::LockBox(UINT level, LockedBox* outLockedVolume, Lock lock)
	{
	}

	void TextureGNMX::UnlockBox(UINT level)
	{
	}

	void TextureGNMX::TransferFrom(Texture* src_texture)
	{
	}
	void TextureGNMX::TranscodeFrom(Texture* src_texture)
	{
	}

	void TextureGNMX::GetLevelDesc(UINT level, SurfaceDesc* outDesc)
	{
		if (level > 0)
			throw ExceptionGNMX("LevelDesc > 0 not supported");

		if (outDesc)
		{
			outDesc->Format = GetPixelFormat();
			outDesc->Width = GetWidth();
			outDesc->Height = GetHeight();
		}
	}

	void TextureGNMX::SaveToFile(LPCWSTR pDestFile, ImageFileFormat DestFormat)
	{
	}

}
