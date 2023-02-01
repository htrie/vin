#include <fstream>
namespace Device
{

	static bool IsDepthFormatVulkan(PixelFormat format)
	{
		return format == PixelFormat::D24X8 || format == PixelFormat::D24S8 || format == PixelFormat::D16 || format == PixelFormat::D32;
	}

	static bool FormatHasStencilVulkan(PixelFormat format)
	{
		return format == PixelFormat::D24X8 || format == PixelFormat::D24S8;
	}

	vk::ImageType ConvertDimensionVulkan(Device::Dimension dimension)
	{
		switch (dimension)
		{
		case Dimension::One:		return vk::ImageType::e1D;
		case Dimension::Two:		return vk::ImageType::e2D;
		case Dimension::Three:		return vk::ImageType::e3D;
		case Dimension::Cubemap:	return vk::ImageType::e2D;
		default:					throw ExceptionVulkan("Unsupported dimension");
		}
	}

	vk::ImageViewType ConvertViewDimensionVulkan(Device::Dimension dimension)
	{
		switch (dimension)
		{
		case Dimension::One:		return vk::ImageViewType::e1D;
		case Dimension::Two:		return vk::ImageViewType::e2D;
		case Dimension::Three:		return vk::ImageViewType::e3D;
		case Dimension::Cubemap:	return vk::ImageViewType::eCube;
		default:					throw ExceptionVulkan("Unsupported dimension");
		}
	}

	vk::Format ConvertPixelFormatVulkan(PixelFormat format, bool useSRGB)
	{
		switch (format)
		{
		case PixelFormat::UNKNOWN:				return vk::Format::eUndefined;
		case PixelFormat::A8:					return vk::Format::eR8Unorm;
		case PixelFormat::L8:					return vk::Format::eR8Unorm;
		case PixelFormat::A8L8:					return vk::Format::eR8G8Unorm;
		case PixelFormat::R16:					return vk::Format::eR16Unorm;
		case PixelFormat::R16F:					return vk::Format::eR16Sfloat;
		case PixelFormat::R32F:					return vk::Format::eR32Sfloat;
		case PixelFormat::G16R16:				return vk::Format::eR16G16Unorm;
		case PixelFormat::G16R16_UINT:			return vk::Format::eR16G16Uint;
		case PixelFormat::G16R16F:				return vk::Format::eR16G16Sfloat;
		case PixelFormat::G32R32F:				return vk::Format::eR32G32Sfloat;
		case PixelFormat::A16B16G16R16:			return vk::Format::eR16G16B16A16Unorm;
		case PixelFormat::A16B16G16R16F:		return vk::Format::eR16G16B16A16Sfloat;
		case PixelFormat::A32B32G32R32F:		return vk::Format::eR32G32B32A32Sfloat;
		case PixelFormat::R11G11B10F:			return vk::Format::eB10G11R11UfloatPack32;
		case PixelFormat::A8R8G8B8:				return useSRGB ? vk::Format::eR8G8B8A8Srgb : vk::Format::eR8G8B8A8Unorm;
		case PixelFormat::A8B8G8R8:				return useSRGB ? vk::Format::eB8G8R8A8Srgb : vk::Format::eB8G8R8A8Unorm;
		case PixelFormat::X8R8G8B8:				return useSRGB ? vk::Format::eB8G8R8A8Srgb : vk::Format::eB8G8R8A8Unorm;
		case PixelFormat::BC1:					return useSRGB ? vk::Format::eBc1RgbaSrgbBlock : vk::Format::eBc1RgbaUnormBlock;
		case PixelFormat::BC2_Premultiplied:	return useSRGB ? vk::Format::eBc2SrgbBlock : vk::Format::eBc2UnormBlock;
		case PixelFormat::BC2:					return useSRGB ? vk::Format::eBc2SrgbBlock : vk::Format::eBc2UnormBlock;
		case PixelFormat::BC3:					return useSRGB ? vk::Format::eBc3SrgbBlock : vk::Format::eBc3UnormBlock;
		case PixelFormat::BC4:					return vk::Format::eBc4UnormBlock;
		case PixelFormat::BC5:					return vk::Format::eBc5UnormBlock;
		case PixelFormat::BC6UF:				return vk::Format::eBc6HUfloatBlock;
		case PixelFormat::BC7_Premultiplied:	return useSRGB ? vk::Format::eBc7SrgbBlock : vk::Format::eBc7UnormBlock;
		case PixelFormat::BC7:					return useSRGB ? vk::Format::eBc7SrgbBlock : vk::Format::eBc7UnormBlock;
		case PixelFormat::ASTC_4x4:				return useSRGB ? vk::Format::eAstc4x4SrgbBlock : vk::Format::eAstc4x4UnormBlock;
		case PixelFormat::ASTC_6x6:				return useSRGB ? vk::Format::eAstc6x6SrgbBlock : vk::Format::eAstc6x6UnormBlock;
		case PixelFormat::ASTC_8x8:				return useSRGB ? vk::Format::eAstc8x8SrgbBlock : vk::Format::eAstc8x8UnormBlock;
		case PixelFormat::D32:					return vk::Format::eD32Sfloat;
		case PixelFormat::D24S8:				return vk::Format::eD32SfloatS8Uint; // eD24UnormS8Uint not supported on iOS and AMD
		case PixelFormat::D24X8:				return vk::Format::eD32SfloatS8Uint;
		default:								throw ExceptionVulkan("Unsupported pixel format");
		}
	}

	vk::ComponentMapping GetComponentMappingVulkan(PixelFormat format, bool useSRGB)
	{
		return { vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA };
	}

	vk::ImageCreateFlags DetermineFlagsVulkan(Device::Dimension dimension)
	{
		if (dimension == Dimension::Cubemap) return vk::ImageCreateFlagBits::eCubeCompatible;
		return vk::ImageCreateFlags();
	}

	unsigned DetermineArrayLayersVulkan(Device::Dimension dimension)
	{
		return dimension == Dimension::Cubemap ? 6 : 1;
	}

	vk::ImageAspectFlags DetermineImageAspectVulkan(UsageHint usage_hint)
	{
		const bool render_target = ((unsigned)usage_hint & (unsigned)UsageHint::RENDERTARGET) > 0;
		const bool depth_stencil = ((unsigned)usage_hint & (unsigned)UsageHint::DEPTHSTENCIL) > 0;
		if (depth_stencil) return vk::ImageAspectFlagBits::eDepth; // TODO: Handle vk::ImageAspectFlagBits::eStencil.
		if (render_target) return vk::ImageAspectFlagBits::eColor;
		return vk::ImageAspectFlagBits::eColor;
	}

	vk::ImageUsageFlags DetermineImageUsageVulkan(UsageHint usage_hint, bool enable_gpu_write)
	{
		const bool render_target = ((unsigned)usage_hint & (unsigned)UsageHint::RENDERTARGET) > 0;
		const bool depth_stencil = ((unsigned)usage_hint & (unsigned)UsageHint::DEPTHSTENCIL) > 0;

		vk::ImageUsageFlags flags = vk::ImageUsageFlagBits::eSampled;
		if (render_target)
			flags |= vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eColorAttachment;
		else if (depth_stencil)
			flags |= vk::ImageUsageFlagBits::eDepthStencilAttachment;
		else
			flags |= vk::ImageUsageFlagBits::eTransferDst;

		if (enable_gpu_write)
			flags |= vk::ImageUsageFlagBits::eStorage;

		return flags;
	}

	vk::FormatFeatureFlags DetermineFormatFeatureVulkan(UsageHint usage_hint)
	{
		const bool render_target = ((unsigned)usage_hint & (unsigned)UsageHint::RENDERTARGET) > 0;
		const bool depth_stencil = ((unsigned)usage_hint & (unsigned)UsageHint::DEPTHSTENCIL) > 0;
		if (render_target) return vk::FormatFeatureFlagBits::eColorAttachment;
		if (depth_stencil) return vk::FormatFeatureFlagBits::eDepthStencilAttachment;
		return vk::FormatFeatureFlagBits::eColorAttachment;
	}

	vk::MemoryPropertyFlags DetermineMemoryPropertiesVulkan(UsageHint usage_hint)
	{
		return vk::MemoryPropertyFlagBits::eDeviceLocal;
	}

	unsigned CountMipsVulkan(unsigned width, unsigned height)
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


	class TextureVulkan : public Texture
	{
		IDevice* device = nullptr;

		bool use_srgb = false;
		bool use_mipmaps = false;
		bool is_backbuffer = false;

		unsigned width = 0;
		unsigned height = 0;
		unsigned depth = 0;
		unsigned mips_count = 0;

		struct MipSize
		{
			unsigned width = 0;
			unsigned height = 0;
			size_t size = 0;

			MipSize(unsigned width, unsigned height, size_t size)
				: width(width), height(height), size(size) {}
		};
		std::vector<MipSize> mips_sizes;

		PixelFormat pixel_format = PixelFormat::UNKNOWN;

		VkDeviceSize allocation_size = 0;
		Memory::Array<std::unique_ptr<AllocatorVulkan::Handle>, 3> handles;
		Memory::Array<vk::UniqueImage, 3> images_backing;
		Memory::Array<vk::Image, 3> images;
		Memory::Array<vk::UniqueImageView, 3> image_views;
		Memory::SmallVector<vk::UniqueImageView, 32, Memory::Tag::Device> mips_image_views;
		Memory::Array<std::shared_ptr<AllocatorVulkan::Data>, 3> datas;

		unsigned host_count = 0;
		unsigned remote_count = 0;

		void Copy(const std::vector<Mip>& mips, unsigned array_count, unsigned mip_start, unsigned mip_count);

		void Create(unsigned Width, unsigned Height, unsigned Depth, unsigned Levels, UsageHint Usage, Pool pool, PixelFormat Format, Device::Dimension dimension, bool mipmaps, bool enable_gpu_write);
		void CreateFromMemoryDDS(const void* pSrcData, size_t srcDataSize, UsageHint Usage, Pool pool, TextureFilter mipFilter, bool premultiply_alpha);
		void CreateFromMemoryPNG(const void* pSrcData, size_t srcDataSize, UsageHint Usage, Pool pool, TextureFilter mipFilter, bool premultiply_alpha);
		void CreateFromMemoryJPG(const void* pSrcData, size_t srcDataSize, UsageHint Usage, Pool pool, TextureFilter mipFilter);
		void CreateFromMemoryKTX(const void* pSrcData, size_t srcDataSize, UsageHint Usage, Pool pool, TextureFilter mipFilter, bool premultiply_alpha);
		void CreateDefault();
	#if defined(DEVELOPMENT_CONFIGURATION) && defined(MOBILE_REALM)
		void CreateFromMemoryKTXDecompress(const void* pSrcData, size_t srcDataSize, UsageHint Usage, Pool pool, TextureFilter mipFilter);
	#endif

		void CreateFromSwapChain(SwapChain* swapChain);
		void CreateEmpty(unsigned Width, unsigned Height, unsigned Depth, unsigned Levels, UsageHint Usage, Pool pool, PixelFormat Format, Device::Dimension dimension, bool mipmaps, bool enable_gpu_write);
		void CreateFromBytes(UINT Width, UINT Height, unsigned char* pixels, UINT bytes_per_pixel);
		void CreateFromMemory(const void* pSrcData, size_t srcDataSize, UsageHint Usage, Pool pool, TextureFilter mipFilter, ImageInfo* out_info, bool premultiply_alpha);
		void CreateFromFile(const wchar_t* pSrcFile, UsageHint Usage, Pool pool, TextureFilter mipFilter, ImageInfo* out_info);

		TextureVulkan(const Memory::DebugStringA<>& name, IDevice* device, bool srgb);

	public:
		TextureVulkan(const Memory::DebugStringA<>& name, IDevice* device, SwapChain* swapChain);
		TextureVulkan(const Memory::DebugStringA<>& name, IDevice* device, UINT Width, UINT Height, UINT Levels, UsageHint Usage, PixelFormat Format, Pool Pool, bool useSRGB, bool use_ESRAM, bool use_mipmaps, bool enable_gpu_write);
		TextureVulkan(const Memory::DebugStringA<>& name, IDevice* device, UINT Width, UINT Height, unsigned char* pixels, UINT bytes_per_pixel);
		TextureVulkan(const Memory::DebugStringA<>& name, IDevice* device, LPCWSTR pSrcFile, UINT width, UINT height, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool, TextureFilter filter, TextureFilter mipFilter, simd::color colorKey, ImageInfo* pSrcInfo, PaletteEntry* outPalette);
		TextureVulkan(const Memory::DebugStringA<>& name, IDevice* device, LPCVOID pSrcData, UINT srcDataSize, UINT width, UINT height, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool, TextureFilter filter, TextureFilter mipFilter, simd::color colorKey, ImageInfo* pSrcInfo, PaletteEntry* outPalette, bool useSRGB, bool premultiply_alpha);
		TextureVulkan(const Memory::DebugStringA<>& name, IDevice* device, UINT edgeLength, UINT levels, UsageHint usage, PixelFormat format, Pool pool, bool useSRGB, bool enable_gpu_write);
		TextureVulkan(const Memory::DebugStringA<>& name, IDevice* device, LPCWSTR pSrcFile);
		TextureVulkan(const Memory::DebugStringA<>& name, IDevice* device, LPCWSTR pSrcFile, UINT size, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool, TextureFilter filter, TextureFilter mipFilter, simd::color colorKey, ImageInfo* outSrcInfo, PaletteEntry* outPalette);
		TextureVulkan(const Memory::DebugStringA<>& name, IDevice* device, LPCVOID pSrcData, UINT srcDataSize, UINT size, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool, TextureFilter filter, TextureFilter mipFilter, simd::color colorKey, ImageInfo* pSrcInfo, PaletteEntry* outPalette, bool useSRGB);
		TextureVulkan(const Memory::DebugStringA<>& name, IDevice* device, LPCVOID pSrcData, UINT srcDataSize, UINT width, UINT height, UINT depth, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool, TextureFilter filter, TextureFilter mipFilter, simd::color colorKey, ImageInfo* pSrcInfo, PaletteEntry* outPalette, bool useSRGB);
		TextureVulkan(const Memory::DebugStringA<>& name, IDevice* device, UINT width, UINT height, UINT depth, UINT levels, UsageHint usage, PixelFormat format, Pool pool, bool useSRGB, bool enable_gpu_write);
		TextureVulkan(const Memory::DebugStringA<>& name, IDevice* device, LPCWSTR pSrcFile, UINT width, UINT height, UINT depth, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool, TextureFilter filter, TextureFilter mipFilter, simd::color colorKey, ImageInfo* outSrcInfo, PaletteEntry* outPalette);

		virtual void LockRect(UINT level, LockedRect* outLockedRect, Lock lock) final;
		virtual void UnlockRect(UINT level) final;
		virtual void LockBox(UINT level, LockedBox* outLockedVolume, Lock lock) final;
		virtual void UnlockBox(UINT level) final;

		virtual void GetLevelDesc(UINT level, SurfaceDesc* outDesc) final;
		virtual size_t GetMipsCount() final { return mips_count; }

		virtual void Fill(void* data, unsigned pitch) final;
		virtual void TransferFrom(Texture* src_texture) final;
		virtual void TranscodeFrom(Texture* src_texture) final;
		virtual void SaveToFile(LPCWSTR pDestFile, ImageFileFormat DestFormat) final;

		virtual bool UseSRGB() const final { return use_srgb; }
		virtual bool UseMipmaps() const final { return use_mipmaps; }

		unsigned GetWidth() const { return width; }
		unsigned GetHeight() const { return height; }
		unsigned GetMipWidth(unsigned index) const { assert(use_mipmaps); assert(index < mips_sizes.size()); return mips_sizes[index].width; }
		unsigned GetMipHeight(unsigned index) const { assert(use_mipmaps); assert(index < mips_sizes.size()); return mips_sizes[index].height; }
		size_t GetMipOffset(unsigned index) const { assert(use_mipmaps); assert(index < mips_sizes.size()); return mips_sizes[index].size; }

		void CopyTo(TextureVulkan& dest);

		PixelFormat GetPixelFormat() const { return pixel_format; }

		std::shared_ptr<AllocatorVulkan::Data> GetData(unsigned backbuffer_index) const { return datas.size() > 0 ? datas[backbuffer_index % host_count] : std::shared_ptr<AllocatorVulkan::Data>(); }

		const vk::Image GetImage(unsigned backbuffer_index) const { return images[backbuffer_index % remote_count]; }

		unsigned GetImageViewCount() const { return remote_count; }
		const vk::UniqueImageView& GetImageView(unsigned i = (unsigned)-1) const { return image_views[i != (unsigned)-1 ? i : static_cast<IDeviceVulkan*>(device)->GetBufferIndex() % remote_count]; }
		const vk::UniqueImageView& GetMipImageView(unsigned index) const { assert(use_mipmaps); assert(index < mips_image_views.size()); return mips_image_views[index]; }

		Memory::DebugStringA<> GetName(unsigned mip = (unsigned)-1, unsigned layer = 0) const { return Resource::GetName() +
			(mip != (unsigned)-1 ? " Mip " + std::to_string(mip) : "") +
			(layer != 0 ? " Layer " + std::to_string(layer) : ""); }
	};
	
	TextureVulkan::TextureVulkan(const Memory::DebugStringA<>& name, IDevice* device, bool srgb)
		: Texture(name)
		, device(device)
		, use_srgb(srgb)
	{
	}

	TextureVulkan::TextureVulkan(const Memory::DebugStringA<>& name, IDevice* device, SwapChain* swapChain)
		: TextureVulkan(name, device, true)
	{
		CreateFromSwapChain(swapChain);
	}

	TextureVulkan::TextureVulkan(const Memory::DebugStringA<>& name, IDevice* device, UINT Width, UINT Height, UINT Levels, UsageHint Usage, PixelFormat Format, Pool pool, bool use_srgb, bool use_ESRAM, bool use_mipmaps, bool enable_gpu_write)
		: TextureVulkan(name, device, use_srgb)
	{
		CreateEmpty(Width, Height, 1, Levels, Usage, pool, Format, Dimension::Two, use_mipmaps, enable_gpu_write);
	}

	TextureVulkan::TextureVulkan(const Memory::DebugStringA<>& name, IDevice* device, UINT Width, UINT Height, unsigned char* pixels, UINT bytes_per_pixel)
		: TextureVulkan(name, device, false)
	{
		CreateFromBytes(Width, Height, pixels, bytes_per_pixel);
	}

	TextureVulkan::TextureVulkan(const Memory::DebugStringA<>& name, IDevice* device, LPCWSTR pSrcFile, UINT width, UINT height, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool, TextureFilter filter, TextureFilter mipFilter, simd::color colorKey, ImageInfo* pSrcInfo, PaletteEntry* outPalette)
		: TextureVulkan(name, device, false)
	{
		CreateFromFile(pSrcFile, usage, pool, mipFilter, pSrcInfo);
	}

	TextureVulkan::TextureVulkan(const Memory::DebugStringA<>& name, IDevice* device, LPCVOID pSrcData, UINT srcDataSize, UINT width, UINT height, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool, TextureFilter filter, TextureFilter mipFilter, simd::color colorKey, ImageInfo* pSrcInfo, PaletteEntry* outPalette, bool use_srgb, bool premultiply_alpha)
		: TextureVulkan(name, device, use_srgb)
	{
		CreateFromMemory(pSrcData, srcDataSize, usage, pool, mipFilter, pSrcInfo, premultiply_alpha);
	}

	TextureVulkan::TextureVulkan(const Memory::DebugStringA<>& name, IDevice* device, UINT edgeLength, UINT levels, UsageHint usage, PixelFormat format, Pool pool, bool use_srgb, bool enable_gpu_write)
		: TextureVulkan(name, device, use_srgb)
	{
		CreateEmpty(edgeLength, edgeLength, 1, levels, usage, pool, format, Dimension::Cubemap, false, enable_gpu_write);
	}

	TextureVulkan::TextureVulkan(const Memory::DebugStringA<>& name, IDevice* device, LPCWSTR pSrcFile)
		: TextureVulkan(name, device, false)
	{
		CreateFromFile(pSrcFile, UsageHint::DEFAULT, Pool::DEFAULT, TextureFilter::NONE, nullptr);
	}

	TextureVulkan::TextureVulkan(const Memory::DebugStringA<>& name, IDevice* device, LPCWSTR pSrcFile, UINT size, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool, TextureFilter filter, TextureFilter mipFilter, simd::color colorKey, ImageInfo* outSrcInfo, PaletteEntry* outPalette)
		: TextureVulkan(name, device, false)
	{
		CreateFromFile(pSrcFile, usage, pool, mipFilter, outSrcInfo);
	}

	TextureVulkan::TextureVulkan(const Memory::DebugStringA<>& name, IDevice* device, LPCVOID pSrcData, UINT srcDataSize, UINT size, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool, TextureFilter filter, TextureFilter mipFilter, simd::color colorKey, ImageInfo* pSrcInfo, PaletteEntry* outPalette, bool use_srgb)
		: TextureVulkan(name, device, use_srgb)
	{
		CreateFromMemory(pSrcData, srcDataSize, usage, pool, mipFilter, pSrcInfo, false);
	}

	TextureVulkan::TextureVulkan(const Memory::DebugStringA<>& name, IDevice* device, LPCVOID pSrcData, UINT srcDataSize, UINT width, UINT height, UINT depth, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool, TextureFilter filter, TextureFilter mipFilter, simd::color colorKey, ImageInfo* pSrcInfo, PaletteEntry* outPalette, bool use_srgb)
		: TextureVulkan(name, device, use_srgb)
	{
		CreateFromMemory(pSrcData, srcDataSize, usage, pool, mipFilter, pSrcInfo, false);
	}

	TextureVulkan::TextureVulkan(const Memory::DebugStringA<>& name, IDevice* device, UINT width, UINT height, UINT depth, UINT levels, UsageHint usage, PixelFormat format, Pool pool, bool use_srgb, bool enable_gpu_write)
		: TextureVulkan(name, device, use_srgb)
	{
		CreateEmpty(width, height, depth, levels, usage, pool, format, Dimension::Three, false, enable_gpu_write);
	}

	TextureVulkan::TextureVulkan(const Memory::DebugStringA<>& name, IDevice* device, LPCWSTR pSrcFile, UINT width, UINT height, UINT depth, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool, TextureFilter filter, TextureFilter mipFilter, simd::color colorKey, ImageInfo* outSrcInfo, PaletteEntry* outPalette)
		: TextureVulkan(name, device, false)
	{
		CreateFromFile(pSrcFile, usage, pool, mipFilter, outSrcInfo);
	}

	void TextureVulkan::Create(unsigned Width, unsigned Height, unsigned Depth, unsigned Levels, UsageHint Usage, Pool pool, PixelFormat Format, Device::Dimension dimension, bool mipmaps, bool enable_gpu_write)
	{
		width = Width;
		height = Height;
		depth = Depth;
		is_backbuffer = false;
		use_mipmaps = mipmaps;
		mips_count = mipmaps ? CountMipsVulkan(Width, Height) : std::max((unsigned)1, Levels);
		pixel_format = Format;


		const bool buffered = ((unsigned)Usage & (unsigned)UsageHint::BUFFERED) > 0;
		remote_count = buffered ? IDeviceVulkan::FRAME_QUEUE_COUNT : 1;
		for (unsigned i = 0; i < remote_count; ++i)
		{
			const auto image_info = vk::ImageCreateInfo()
				.setFlags(DetermineFlagsVulkan(dimension))
				.setImageType(ConvertDimensionVulkan(dimension))
				.setFormat(ConvertPixelFormatVulkan(pixel_format, use_srgb))
				.setExtent(vk::Extent3D(width, height, depth))
				.setMipLevels(mips_count)
				.setArrayLayers(DetermineArrayLayersVulkan(dimension))
				.setSamples(vk::SampleCountFlagBits::e1)
				.setTiling(static_cast<IDeviceVulkan*>(device)->Tiling(ConvertPixelFormatVulkan(pixel_format, use_srgb), DetermineFormatFeatureVulkan(Usage)))
				.setUsage(DetermineImageUsageVulkan(Usage, enable_gpu_write))
				.setSharingMode(vk::SharingMode::eExclusive)
				.setInitialLayout(vk::ImageLayout::eUndefined);
			images_backing.emplace_back(static_cast<IDeviceVulkan*>(device)->GetDevice().createImageUnique(image_info));
			static_cast<IDeviceVulkan*>(device)->DebugName((uint64_t)images_backing.back().get().operator VkImage(), VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT, GetName());
			images.push_back(images_backing.back().get());

			const bool render_target = ((unsigned)Usage & (unsigned)UsageHint::RENDERTARGET) > 0;
			const auto type = render_target ? AllocatorVulkan::Type::RenderTarget : AllocatorVulkan::Type::Texture;
			const auto create_info = static_cast<IDeviceVulkan*>(device)->GetAllocator().GetAllocationInfoForGPUOnly(static_cast<IDeviceVulkan*>(device)->GetDeviceProps());
			handles.emplace_back(static_cast<IDeviceVulkan*>(device)->GetAllocator().AllocateForImage(GetName(), type, images.back(), create_info));

			const auto image_view_info = vk::ImageViewCreateInfo()
				.setImage(images.back())
				.setViewType(ConvertViewDimensionVulkan(dimension))
				.setFormat(ConvertPixelFormatVulkan(pixel_format, use_srgb))
				.setComponents(GetComponentMappingVulkan(pixel_format, use_srgb))
				.setSubresourceRange(vk::ImageSubresourceRange{ DetermineImageAspectVulkan(Usage), 0, mips_count, 0, DetermineArrayLayersVulkan(dimension) });
			image_views.emplace_back(static_cast<IDeviceVulkan*>(device)->GetDevice().createImageViewUnique(image_view_info));
			static_cast<IDeviceVulkan*>(device)->DebugName((uint64_t)image_views[i].get().operator VkImageView(), VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT, Memory::DebugStringA<>("View ") + GetName());
		}

		allocation_size = handles.back()->GetAllocationInfo().GetSize();

		if (use_mipmaps)
		{
			const auto bytes_per_pixel = GetBitsPerPixel(pixel_format) / 8; // Wrong to use for block compressed types (pitch is per block count, not pixels).

			unsigned mip_width = width;
			unsigned mip_height = height;
			size_t offset = 0;

			for (unsigned i = 0; i < mips_count; ++i)
			{
				const auto image_view_info = vk::ImageViewCreateInfo()
					.setImage(images.back())
					.setViewType(ConvertViewDimensionVulkan(dimension))
					.setFormat(ConvertPixelFormatVulkan(pixel_format, use_srgb))
					.setComponents(GetComponentMappingVulkan(pixel_format, use_srgb))
					.setSubresourceRange(vk::ImageSubresourceRange{ DetermineImageAspectVulkan(Usage), i, 1, 0, DetermineArrayLayersVulkan(dimension) });
				mips_image_views.emplace_back(static_cast<IDeviceVulkan*>(device)->GetDevice().createImageViewUnique(image_view_info));
				static_cast<IDeviceVulkan*>(device)->DebugName((uint64_t)mips_image_views.back().get().operator VkImageView(), VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT, Memory::DebugStringA<>("Mip View ") + GetName());

				mips_sizes.emplace_back(mip_width, mip_height, offset);
				offset += mip_width * mip_height * bytes_per_pixel;
				mip_width = mip_width > 1 ? mip_width / 2 : 1;
				mip_height = mip_height > 1 ? mip_height / 2 : 1;
			}
		}

		if (NeedsStaging(Usage, pool))
		{
			const bool dynamic = ((unsigned)Usage & (unsigned)UsageHint::DYNAMIC) > 0;
			host_count = dynamic ? IDeviceVulkan::FRAME_QUEUE_COUNT : 1; // TODO: If DISCARD more than once per frame, then need more than N-buffering.
			for (unsigned i = 0; i < host_count; ++i)
				datas.emplace_back(std::make_shared<AllocatorVulkan::Data>(Memory::DebugStringA<>("Staging ") + GetName(), device, AllocatorVulkan::Type::Staging, allocation_size, vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eHostCached | vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent));
		}
		
	}

	void TextureVulkan::CreateFromSwapChain(SwapChain* swapChain)
	{
		depth = 1;
		is_backbuffer = true;
		use_mipmaps = false;
		mips_count = 1;

		const auto backbuffer_images = static_cast<IDeviceVulkan*>(device)->GetDevice().getSwapchainImagesKHR(static_cast<SwapChainVulkan*>(swapChain)->GetSwapChain());

		width = (unsigned)swapChain->GetWidth();
		height = (unsigned)swapChain->GetHeight();
		pixel_format = Device::PixelFormat::A8B8G8R8;
		remote_count = (unsigned)backbuffer_images.size();

		for (unsigned i = 0; i < remote_count; ++i)
		{
			images.emplace_back(backbuffer_images[i]);

			const auto image_view_info = vk::ImageViewCreateInfo()
				.setImage(images[i])
				.setViewType(ConvertViewDimensionVulkan(Dimension::Two))
				.setFormat(ConvertPixelFormatVulkan(pixel_format, use_srgb))
				.setComponents(GetComponentMappingVulkan(pixel_format, use_srgb))
				.setSubresourceRange(vk::ImageSubresourceRange{ DetermineImageAspectVulkan(UsageHint::RENDERTARGET), 0, mips_count, 0, DetermineArrayLayersVulkan(Dimension::Two) });
			image_views.emplace_back(static_cast<IDeviceVulkan*>(device)->GetDevice().createImageViewUnique(image_view_info));
			static_cast<IDeviceVulkan*>(device)->DebugName((uint64_t)image_views[i].get().operator VkImageView(), VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT, GetName());
		}

		for (unsigned i = 0; i < remote_count; ++i)
		{
			auto upload = std::make_unique<TransitionImageLoadVulkan>(GetName(), images[i], vk::ImageAspectFlagBits::eColor, mips_count, 1, true);
			static_cast<IDeviceVulkan*>(device)->GetTransfer().Add(std::move(upload));
		}
	}

	void TextureVulkan::CreateEmpty(unsigned Width, unsigned Height, unsigned Depth, unsigned Levels, UsageHint Usage, Pool pool, PixelFormat Format, Dimension dimension, bool mipmaps, bool enable_gpu_write)
	{
		Create(Width, Height, Depth, Levels, Usage, pool, Format, dimension, mipmaps, enable_gpu_write);

		const auto aspect = (Usage == UsageHint::DEPTHSTENCIL) ?
			(FormatHasStencilVulkan(pixel_format) ? vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil : vk::ImageAspectFlagBits::eDepth) :
			vk::ImageAspectFlagBits::eColor;

		for (unsigned i = 0; i < remote_count; ++i)
		{
			auto upload = std::make_unique<TransitionImageLoadVulkan>(GetName(), GetImage(i), aspect, mips_count, dimension == Dimension::Cubemap ? 6 : 1, false);
			static_cast<IDeviceVulkan*>(device)->GetTransfer().Add(std::move(upload));
		}
	}

	void TextureVulkan::CreateFromBytes(UINT Width, UINT Height, unsigned char* pixels, UINT bytes_per_pixel)
	{
		Create(Width, Height, 1, 1, UsageHint::DEFAULT, Pool::DEFAULT, PixelFormat::A8R8G8B8, Dimension::Two, false, false);

		Fill(pixels, bytes_per_pixel * Width);
	}

	void TextureVulkan::CreateFromMemoryDDS(const void* pSrcData, size_t srcDataSize, UsageHint Usage, Pool pool, TextureFilter mipFilter, bool premultiply_alpha)
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

		Copy(dds.GetSubResources(), dds.GetArraySize(), mip_start, mips_count);
	}

	void TextureVulkan::CreateFromMemoryPNG(const void* pSrcData, size_t srcDataSize, UsageHint Usage, Pool pool, TextureFilter mipFilter, bool premultiply_alpha)
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

		std::vector<Mip> sub_resources(1);
		Mip& mip = sub_resources.back();
		mip.data = png.GetImageData();
		mip.size = png.GetImageSize();
		mip.row_size = png.GetImageSize() / png.GetHeight();
		mip.width = png.GetWidth();
		mip.height = png.GetHeight();
		mip.depth = 1;
		mip.block_width = 1;
		mip.block_height = 1;

		Copy(sub_resources, array_size, mip_start, mips_count);
	}

	void TextureVulkan::CreateFromMemoryJPG(const void* pSrcData, size_t srcDataSize, UsageHint Usage, Pool pool, TextureFilter mipFilter)
	{
		JPG jpg(GetName(), (uint8_t*)pSrcData, srcDataSize);

		const unsigned array_size = 1;
		const unsigned mip_start = 0;
		const unsigned mip_count = 1;

		Create(jpg.GetWidth(), jpg.GetHeight(), array_size, mip_count, Usage, pool, jpg.GetPixelFormat(), jpg.GetDimension(), false, false);

		std::vector<Mip> sub_resources(1);
		Mip& mip = sub_resources.back();
		mip.data = jpg.GetImageData();
		mip.size = jpg.GetImageSize();
		mip.row_size = jpg.GetImageSize() / jpg.GetHeight();
		mip.width = jpg.GetWidth();
		mip.height = jpg.GetHeight();
		mip.depth = 1;
		mip.block_width = 1;
		mip.block_height = 1;

		Copy(sub_resources, array_size, mip_start, mips_count);
	}

	void TextureVulkan::CreateFromMemoryKTX(const void* pSrcData, size_t srcDataSize, UsageHint Usage, Pool pool, TextureFilter mipFilter, bool premultiply_alpha)
	{
		KTX ktx((uint8_t*)pSrcData, srcDataSize);

		unsigned width = ktx.GetWidth();
		unsigned height = ktx.GetHeight();
		unsigned mip_start = 0;
		unsigned mip_count = ktx.GetMipCount();
		SkipMips(width, height, mip_start, mip_count, mipFilter, ktx.GetPixelFormat());

		Create(width, height, ktx.GetDepth(), mip_count, Usage, pool, ktx.GetPixelFormat(), ktx.GetDimension(), false, false);

		if (premultiply_alpha)
		{
			// TODO: Premultiply if not compressed.
		}

		Copy(ktx.GetMips(), ktx.GetArrayCount(), mip_start, mips_count);
	}

	void TextureVulkan::CreateDefault()
	{
		const unsigned dimensions = 1;
		const unsigned array_size = 1;
		const unsigned mip_start = 0;
		const unsigned mip_count = 1;

		Create(dimensions, dimensions, array_size, mip_count, UsageHint::DEFAULT, Pool::DEFAULT, PixelFormat::A8R8G8B8, Dimension::Two, false, false);
		
		const uint32_t black = 0;

		std::vector<Mip> sub_resources(1);
		Mip& mip = sub_resources.back();
		mip.data = reinterpret_cast<const uint8_t*>(&black);
		mip.size = sizeof(black);
		mip.row_size = mip.size;
		mip.width = dimensions;
		mip.height = dimensions;
		mip.depth = 1;
		mip.block_width = 1;
		mip.block_height = 1;

		Copy(sub_resources, 1, mip_start, mip_count);
	}

#if defined(DEVELOPMENT_CONFIGURATION) && defined(MOBILE_REALM)
	void TextureVulkan::CreateFromMemoryKTXDecompress(const void* pSrcData, size_t srcDataSize, UsageHint Usage, Pool pool, TextureFilter mipFilter)
	{
		KTX ktx((uint8_t*)pSrcData, srcDataSize);
		assert(ktx.GetMipCount() >= 1);

		unsigned width = ktx.GetWidth();
		unsigned height = ktx.GetHeight();
		unsigned mip_start = 0;
		unsigned mip_count = ktx.GetMipCount();
		SkipMips(width, height, mip_start, mip_count, mipFilter, Device::PixelFormat::A8R8G8B8 );

		Create(width, height, ktx.GetDepth(), mip_count, Usage, pool, Device::PixelFormat::A8R8G8B8, ktx.GetDimension(), false, false);

		ASTC::Context context(ASTC::BlockModeFromPixelFormat(ktx.GetPixelFormat()), ASTC::Profile::LDR);

		std::vector<Device::Mip> new_mips;
		Memory::Vector<std::unique_ptr<uint8_t[]>, Memory::Tag::Texture> mem;
		const auto& old_mips = ktx.GetMips();
		new_mips.resize(ktx.GetArrayCount() * mip_count);
		mem.resize(new_mips.size());

		for (unsigned int array_index = 0; array_index < ktx.GetArrayCount(); array_index++)
		{
			for (unsigned int mip_index = mip_start; mip_index < mip_start + mip_count; mip_index++)
			{
				const auto src_index = (array_index * (mip_start + mip_count)) + mip_index;
				const auto dst_index = (array_index * mip_count) + mip_index;

				const auto& src_mip = old_mips[src_index];

				auto& dst_mem = mem[dst_index];
				dst_mem = std::make_unique<uint8_t[]>(4 * src_mip.width * src_mip.height);

				auto& dst_mip = new_mips[dst_index];
				dst_mip.data = dst_mem.get();
				dst_mip.block_height = 1;
				dst_mip.block_width = 1;
				dst_mip.depth = 1;
				dst_mip.height = src_mip.height;
				dst_mip.width = src_mip.width;
				dst_mip.row_size = 4 * src_mip.width;
				dst_mip.size = dst_mip.row_size * src_mip.height;

				ASTC::Image image(context, src_mip.width, src_mip.height, Device::PixelFormat::A8R8G8B8);
				ASTC::DecompressImage(context, src_mip.data, src_mip.size, image);
				for (unsigned int y = 0; y < image.GetHeight(); y++)
				{
					for (unsigned int x = 0; x < image.GetWidth(); x++)
					{
						uint8_t* dst = dst_mem.get() + (((y * image.GetWidth()) + x) * 4);
						const auto src = image.GetPixel(x, y);
						for (unsigned int c = 0; c < 4; c++)
							dst[c] = (uint8_t)std::max(0.0f, std::min(((src[c] * 255.0f) + 0.5f), 255.5f));
					}
				}
			}
		}

		Copy(new_mips, ktx.GetArrayCount(), 0, mip_count);
	}
#endif

	void TextureVulkan::CreateFromMemory(const void* pSrcData, size_t srcDataSize, UsageHint Usage, Pool pool, TextureFilter mipFilter, ImageInfo* out_info, bool premultiply_alpha)
	{
		ImageFileFormat format;

		try
		{
			uint32_t magic = *(const uint32_t*)pSrcData;
			if (magic == DDS::MAGIC)
			{
				format = ImageFileFormat::DDS;
				CreateFromMemoryDDS(pSrcData, srcDataSize, Usage, pool, mipFilter, premultiply_alpha);
			}
			else if (magic == PNG::MAGIC)
			{
				format = ImageFileFormat::PNG;
				CreateFromMemoryPNG(pSrcData, srcDataSize, Usage, pool, mipFilter, premultiply_alpha);
			}
			else if ((magic & JPG::MAGIC_MASK) == JPG::MAGIC)
			{
				format = ImageFileFormat::JPG;
				CreateFromMemoryJPG(pSrcData, srcDataSize, Usage, pool, mipFilter);
			}
			else if ((magic == KTX::MAGIC) && (static_cast<IDeviceVulkan*>(device)->SupportsASTC()))
			{
				format = ImageFileFormat::KTX;
				CreateFromMemoryKTX(pSrcData, srcDataSize, Usage, pool, mipFilter, premultiply_alpha);
			}
		#if defined(DEVELOPMENT_CONFIGURATION) && defined(MOBILE_REALM)
			else if (magic == KTX::MAGIC)
			{
				format = ImageFileFormat::KTX;
				CreateFromMemoryKTXDecompress(pSrcData, srcDataSize, Usage, pool, mipFilter);
			}
		#endif
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
			out_info->Width = width;
			out_info->Height = height;
			out_info->Depth = depth;
			out_info->MipLevels = mips_count;
			out_info->Format = pixel_format;
			out_info->ImageFileFormat = format;
		}
	}

	void TextureVulkan::CreateFromFile(const wchar_t* pSrcFile, UsageHint Usage, Pool pool, TextureFilter mipFilter, ImageInfo* out_info)
	{
		File::InputFileStream stream(pSrcFile);

		CreateFromMemory(stream.GetFilePointer(), stream.GetFileSize(), Usage, pool, mipFilter, out_info, false);
	}

	void TextureVulkan::Copy(const std::vector<Mip>& mips, unsigned array_count, unsigned mip_start, unsigned mip_count)
	{
		assert(remote_count == 1);

		const auto total_size = allocation_size + 16 * mip_count; // Add extra space for mip alignments (Metal requires each copy offset to be 16 bytes aligned).
		auto staging_data = std::make_shared<AllocatorVulkan::Data>(Memory::DebugStringA<>("Texture Data Copy ") + GetName(), device, AllocatorVulkan::Type::Staging, total_size, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
		
		uint8_t* map = staging_data->GetMap();
		uint8_t* maps[3];
		for (unsigned i = 0; i < host_count; ++i)
			maps[i] = i < datas.size() && datas[i] ? datas[i]->GetMap() : nullptr;

		uint8_t* start = map;

		BufferImageCopies copies;

		for (unsigned array_index = 0; array_index < array_count; ++array_index)
		{
			unsigned dst_mip_index = 0;

			for (unsigned mip_index = mip_start; mip_index < (mip_start + mip_count); ++mip_index)
			{
				const auto& mip = mips[array_index * (mip_start + mip_count) + mip_index];

				memcpy(map, mip.data, mip.size);

				for (unsigned i = 0; i < host_count; ++i)
					if (maps[i])
						memcpy(maps[i], mip.data, mip.size);

				const auto mip_width = (uint32_t)Memory::AlignSize(mip.width, mip.block_width);
				const auto mip_height = (uint32_t)Memory::AlignSize(mip.height, mip.block_height);
				const auto mip_size = (uint32_t)Memory::AlignSize((mip_height / mip.block_height) * mip.row_size, (size_t)16) * mip.depth;

				const auto copy = vk::BufferImageCopy()
					.setBufferOffset(map - start)
					.setBufferRowLength(mip_width)
					.setBufferImageHeight(mip_height)
					.setImageSubresource(vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, dst_mip_index, array_index, 1))
					.setImageExtent(vk::Extent3D(mip.width, mip.height, mip.depth))
					.setImageOffset(vk::Offset3D(0, 0, 0));
				copies.emplace_back(copy);

				map += mip_size;
				for (unsigned i = 0; i < host_count; ++i)
					if (maps[i])
						maps[i] += mip_size;
				dst_mip_index++;
			}
		}

		auto upload = std::make_unique<CopyStaticDataToImageLoadVulkan>(GetName(), GetImage(static_cast<IDeviceVulkan*>(device)->GetBufferIndex()), 0, mip_count, array_count, std::move(staging_data), copies);
		static_cast<IDeviceVulkan*>(device)->GetTransfer().Add(std::move(upload));
	}

	void TextureVulkan::Fill(void* data, unsigned pitch)
	{
		assert(remote_count == 1);

		auto staging_data = std::make_shared<AllocatorVulkan::Data>(Memory::DebugStringA<>("Texture Data Fill ") + GetName(), device, AllocatorVulkan::Type::Staging, allocation_size, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

		uint8_t* map = staging_data->GetMap();
		const auto pitch_in_bytes = width * GetBitsPerPixel(pixel_format) / 8; // Wrong to use for block compressed types (pitch is per block count, not pixels).

		for (unsigned i = 0; i < height; ++i)
		{
			memcpy(map + pitch_in_bytes * i, (uint8_t*)data + pitch * i, pitch);
		}

		BufferImageCopies copies;
		const auto copy = vk::BufferImageCopy()
			.setBufferOffset(0)
			.setBufferRowLength(width)
			.setBufferImageHeight(height)
			.setImageSubresource(vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1))
			.setImageExtent(vk::Extent3D(width, height, depth))
			.setImageOffset(vk::Offset3D(0, 0, 0));
		copies.emplace_back(copy);

		auto upload = std::make_unique<CopyDynamicDataToImageLoadVulkan>(GetName(), GetImage(static_cast<IDeviceVulkan*>(device)->GetBufferIndex()), 0, 1, 1, std::move(staging_data), copies);
		static_cast<IDeviceVulkan*>(device)->GetTransfer().Add(std::move(upload));
	}

	void TextureVulkan::CopyTo(TextureVulkan& dest)
	{
		assert(width == dest.width);
		assert(height == dest.height);
		assert(depth == dest.depth);

		{
			const auto copy = vk::ImageCopy()
				.setSrcOffset(0)
				.setSrcSubresource(vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1))
				.setDstOffset(0)
				.setDstSubresource(vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1))
				.setExtent(vk::Extent3D(width, height, depth));

			auto download = std::make_unique<CopyImageToImageLoadVulkan>(GetName(), GetImage(static_cast<IDeviceVulkan*>(device)->GetBufferIndex()), dest.GetImage(static_cast<IDeviceVulkan*>(device)->GetBufferIndex()), copy, is_backbuffer);

			static_cast<IDeviceVulkan*>(device)->GetTransfer().Add(std::move(download));
		}

		if (auto data = dest.GetData(static_cast<IDeviceVulkan*>(device)->GetBufferIndex()))
		{
			BufferImageCopies copies;
			const auto copy = vk::BufferImageCopy()
				.setBufferOffset(0)
				.setBufferRowLength(width)
				.setBufferImageHeight(height)
				.setImageSubresource(vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1))
				.setImageExtent(vk::Extent3D(width, height, depth))
				.setImageOffset(vk::Offset3D(0, 0, 0));
			copies.emplace_back(copy);

			auto download = std::make_unique<CopyImageToDataLoadVulkan>(GetName(), GetImage(static_cast<IDeviceVulkan*>(device)->GetBufferIndex()), 1, 1, data, copies, is_backbuffer);

			static_cast<IDeviceVulkan*>(device)->GetTransfer().Add(std::move(download));
		}

		static_cast<IDeviceVulkan*>(device)->GetTransfer().Wait();
		static_cast<IDeviceVulkan*>(device)->GetTransfer().Flush(); // Need the result right now if LockRect. // TODO: Remove.
		static_cast<IDeviceVulkan*>(device)->GetTransfer().Wait();
	}

	void TextureVulkan::LockRect(unsigned level, LockedRect* outLockedRect, Lock lock)
	{
		assert(level == 0 || use_mipmaps);

		if (lock == Device::Lock::DISCARD)
		{
			assert(host_count > 1 && "Lock::DISCARD requires UsageHint::DYNAMIC");
		}

		const auto bytes_per_pixel = GetBitsPerPixel(pixel_format) / 8; // Wrong to use for block compressed types (pitch is per block count, not pixels).
		const auto offset = level == 0 ? 0 : GetMipOffset(level);
		const auto w = level == 0 ? width : GetMipWidth(level);

		outLockedRect->pBits = GetData(static_cast<IDeviceVulkan*>(device)->GetBufferIndex())->GetMap() + offset;
		outLockedRect->Pitch = w * bytes_per_pixel;
	}

	void TextureVulkan::UnlockRect(unsigned level)
	{
		const auto offset = level == 0 ? 0 : GetMipOffset(level);
		const auto w = level == 0 ? width : GetMipWidth(level);
		const auto h = level == 0 ? height : GetMipHeight(level);

		BufferImageCopies copies;
		const auto copy = vk::BufferImageCopy()
			.setBufferOffset(offset)
			.setBufferRowLength(w)
			.setBufferImageHeight(h)
			.setImageSubresource(vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, level, 0, 1))
			.setImageExtent(vk::Extent3D(w, h, depth))
			.setImageOffset(vk::Offset3D(0, 0, 0));
		copies.emplace_back(copy);

		auto upload = std::make_unique<CopyStaticDataToImageLoadVulkan>(GetName(), GetImage(static_cast<IDeviceVulkan*>(device)->GetBufferIndex()), level, 1, 1, GetData(static_cast<IDeviceVulkan*>(device)->GetBufferIndex()), copies);
		static_cast<IDeviceVulkan*>(device)->GetTransfer().Add(std::move(upload));
	}

	void TextureVulkan::LockBox(UINT level, LockedBox* outLockedVolume, Lock lock)
	{
	}

	void TextureVulkan::UnlockBox(UINT level)
	{
	}

	void TextureVulkan::TransferFrom(Texture* pSrcResource)
	{
	}

	void TextureVulkan::TranscodeFrom(Texture* pSrcResource)
	{
	}

	void TextureVulkan::GetLevelDesc(UINT level, SurfaceDesc* outDesc)
	{
		if (level > 0)
			throw ExceptionVulkan("LevelDesc > 0 not supported");

		if (outDesc)
		{
			outDesc->Format = GetPixelFormat();
			outDesc->Width = GetWidth();
			outDesc->Height = GetHeight();
		}
	}

	void TextureVulkan::SaveToFile(LPCWSTR pDestFile, ImageFileFormat DestFormat)
	{
		if (DestFormat == ImageFileFormat::PNG)
		{
			assert(pixel_format == PixelFormat::A8R8G8B8);
			assert(datas[0]);

			std::vector<uint8_t> rgba(datas[0]->GetSize());
			const uint8_t* const src = datas[0]->GetMap();

			for (unsigned j = 0; j < height; ++j)
			{
				for (unsigned i = 0; i < width; ++i)
				{
					const auto offset = (j * width + i) * 4;
					rgba[offset + 0] = src[offset + 2];
					rgba[offset + 1] = src[offset + 1];
					rgba[offset + 2] = src[offset + 0];
					rgba[offset + 3] = src[offset + 3];
				}
			}

			//manually encode and write the data to file, because lodepng doesn't accept wide filenames and WstringToString() will garble certain unicode characters
			//previously: auto error = lodepng::encode(WstringToString(pDestFile), rgba.data(), width, height, LodePNGColorType::LCT_RGBA, 8);

			std::vector< unsigned char > buffer;
			const unsigned error = lodepng::encode( buffer, rgba.data(), width, height, LodePNGColorType::LCT_RGBA, 8 );
			if( error || buffer.empty() )
				throw ExceptionVulkan("Failed to save PNG image");

			std::ofstream fout( PathStr( pDestFile ), std::ios::binary );
			fout.write( (const char*)buffer.data(), buffer.size() );
			if( fout.fail() )
				throw ExceptionVulkan( "Failed to save PNG image" );
		}
		else if (DestFormat == ImageFileFormat::DDS)
		{
			LOG_WARN(L"Image format not supported yet");
		}
		else
		{
			LOG_WARN(L"Image format not supported yet");
		}
	}

}
