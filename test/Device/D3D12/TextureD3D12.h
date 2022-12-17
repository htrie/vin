#pragma once

namespace Device
{
	class IDevice;
	class IDeviceD3D12;

	class TextureD3D12 : public Texture
	{
		IDeviceD3D12* device = nullptr;
		PixelFormat pixel_format = PixelFormat::UNKNOWN;
		bool use_srgb = false;
		bool use_mipmaps = false;
		bool is_backbuffer = false;

		unsigned width = 0;
		unsigned height = 0;
		unsigned depth = 0;
		unsigned mips_count = 0;

		uint64_t allocation_size = 0;
		std::unique_ptr<AllocatorD3D12::Handle> handle;

		struct Footprints
		{
			Memory::SmallVector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT, 16, Memory::Tag::Device> footprints;
			Memory::SmallVector<UINT, 16, Memory::Tag::Device> row_count;
			Memory::SmallVector<UINT64, 16, Memory::Tag::Device> row_sizes;
		};

		struct MipSize
		{
			unsigned width = 0;
			unsigned height = 0;
			size_t size = 0;

			MipSize(unsigned width, unsigned height, size_t size)
				: width(width), height(height), size(size) {}
		};
		std::vector<MipSize> mips_sizes;

		unsigned count = 1;
		unsigned current = 0;

		Memory::SmallVector<DescriptorHeapD3D12::Handle, 3, Memory::Tag::Device> image_views;
		Memory::SmallVector<DescriptorHeapD3D12::Handle, 32, Memory::Tag::Device> mips_image_views;
		Memory::SmallVector<ComPtr<ID3D12Resource>, 3, Memory::Tag::Device> texture_resources;
		Memory::SmallVector<std::shared_ptr<AllocatorD3D12::Data>, 3, Memory::Tag::Device> datas;
		DescriptorHeapD3D12::Handle unordered_access_view;
		Device::Dimension dimension;
		Footprints subresource_footprints;

	private:
		TextureD3D12(const Memory::DebugStringA<>& name, IDevice* device, bool srgb);
		void CreateFromSwapChain(SwapChain* swapChain);
		void Create(unsigned Width, unsigned Height, unsigned Depth, unsigned Levels, UsageHint Usage, Pool pool, PixelFormat Format, Device::Dimension dimension, bool mipmaps, bool enable_gpu_write, bool use_ESRAM = false);
		void CreateEmpty(unsigned Width, unsigned Height, unsigned Depth, unsigned Levels, UsageHint Usage, Pool pool, PixelFormat Format, Dimension dimension, bool mipmaps, bool enable_gpu_write, bool use_ESRAM = false);
		void CreateFromBytes(UINT Width, UINT Height, unsigned char* pixels, UINT bytes_per_pixel);
		void CreateFromMemoryDDS(const void* pSrcData, size_t srcDataSize, UsageHint Usage, Pool pool, TextureFilter mipFilter, bool premultiply_alpha);
		void CreateFromMemoryPNG(const void* pSrcData, size_t srcDataSize, UsageHint Usage, Pool pool, TextureFilter mipFilter, bool premultiply_alpha);
		void CreateFromMemoryJPG(const void* pSrcData, size_t srcDataSize, UsageHint Usage, Pool pool, TextureFilter mipFilter);
		void CreateDefault();
		void CreateFromMemory(const void* pSrcData, size_t srcDataSize, UsageHint Usage, Pool pool, TextureFilter mipFilter, ImageInfo* out_info, bool premultiply_alpha);
		void CreateFromFile(const wchar_t* pSrcFile, UsageHint Usage, Pool pool, TextureFilter mipFilter, ImageInfo* out_info);

		void Copy(const std::vector<Mip>& mips, unsigned array_count, unsigned mip_start, unsigned mip_count);
		Footprints& GetFootprints();
		uint32_t GetSubresourceCount() const { return mips_count * (dimension == Dimension::Three ? 1 : depth); }

	public:
		ID3D12Resource* GetResource(uint32_t index) const { assert(index < texture_resources.size()); return texture_resources[index].Get(); }

	public:
		TextureD3D12(const Memory::DebugStringA<>& name, IDevice* device, SwapChain* swap_chain);
		TextureD3D12(const Memory::DebugStringA<>& name, IDevice* device, UINT Width, UINT Height, UINT Levels, UsageHint Usage, PixelFormat Format, Pool Pool, bool useSRGB, bool use_ESRAM, bool use_mipmaps, bool enable_gpu_write);
		TextureD3D12(const Memory::DebugStringA<>& name, IDevice* device, UINT Width, UINT Height, unsigned char* pixels, UINT bytes_per_pixel);
		TextureD3D12(const Memory::DebugStringA<>& name, IDevice* device, LPCWSTR pSrcFile, UINT width, UINT height, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool, TextureFilter filter, TextureFilter mipFilter, simd::color colorKey, ImageInfo* pSrcInfo, PaletteEntry* outPalette);
		TextureD3D12(const Memory::DebugStringA<>& name, IDevice* device, LPCVOID pSrcData, UINT srcDataSize, UINT width, UINT height, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool, TextureFilter filter, TextureFilter mipFilter, simd::color colorKey, ImageInfo* pSrcInfo, PaletteEntry* outPalette, bool useSRGB, bool premultiply_alpha);
		TextureD3D12(const Memory::DebugStringA<>& name, IDevice* device, UINT edgeLength, UINT levels, UsageHint usage, PixelFormat format, Pool pool, bool useSRGB, bool enable_gpu_write);
		TextureD3D12(const Memory::DebugStringA<>& name, IDevice* device, LPCWSTR pSrcFile);
		TextureD3D12(const Memory::DebugStringA<>& name, IDevice* device, LPCWSTR pSrcFile, UINT size, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool, TextureFilter filter, TextureFilter mipFilter, simd::color colorKey, ImageInfo* outSrcInfo, PaletteEntry* outPalette);
		TextureD3D12(const Memory::DebugStringA<>& name, IDevice* device, LPCVOID pSrcData, UINT srcDataSize, UINT size, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool, TextureFilter filter, TextureFilter mipFilter, simd::color colorKey, ImageInfo* pSrcInfo, PaletteEntry* outPalette, bool useSRGB);
		TextureD3D12(const Memory::DebugStringA<>& name, IDevice* device, LPCVOID pSrcData, UINT srcDataSize, UINT width, UINT height, UINT depth, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool, TextureFilter filter, TextureFilter mipFilter, simd::color colorKey, ImageInfo* pSrcInfo, PaletteEntry* outPalette, bool useSRGB);
		TextureD3D12(const Memory::DebugStringA<>& name, IDevice* device, UINT width, UINT height, UINT depth, UINT levels, UsageHint usage, PixelFormat format, Pool pool, bool useSRGB, bool enable_gpu_write);
		TextureD3D12(const Memory::DebugStringA<>& name, IDevice* device, LPCWSTR pSrcFile, UINT width, UINT height, UINT depth, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool, TextureFilter filter, TextureFilter mipFilter, simd::color colorKey, ImageInfo* outSrcInfo, PaletteEntry* outPalette);

		void CopyTo(TextureD3D12& dest);
		virtual void LockRect(UINT level, LockedRect* outLockedRect, Lock lock) final;
		virtual void UnlockRect(UINT level) final;
		virtual void LockBox(UINT level, LockedBox* outLockedVolume, Lock lock) final;
		virtual void UnlockBox(UINT level) final;

		virtual void GetLevelDesc(UINT level, SurfaceDesc* outDesc) final;
		virtual size_t GetMipsCount() final { return mips_count; }

		virtual void Fill(void* data, unsigned pitch) final;
		virtual void CopyResource(Texture* texture) final;
		virtual void SaveToFile(LPCWSTR pDestFile, ImageFileFormat DestFormat) final;

		virtual bool UseSRGB() const final { return use_srgb; }
		virtual bool UseMipmaps() const final { return use_mipmaps; }

		unsigned GetWidth() const { return width; }
		unsigned GetHeight() const { return height; }
		unsigned GetMipWidth(unsigned index) const { assert(use_mipmaps); assert(index < mips_sizes.size()); return mips_sizes[index].width; }
		unsigned GetMipHeight(unsigned index) const { assert(use_mipmaps); assert(index < mips_sizes.size()); return mips_sizes[index].height; }

		void Swap();

		std::shared_ptr<AllocatorD3D12::Data> GetData() const { return current < datas.size() ? datas[current] : std::shared_ptr<AllocatorD3D12::Data>(); }
		unsigned GetImageViewCount() const { return count; }
		unsigned GetCurrentImageView() const { return current; }

		const D3D12_CPU_DESCRIPTOR_HANDLE GetUAV() const { return (D3D12_CPU_DESCRIPTOR_HANDLE)unordered_access_view; }
		const D3D12_CPU_DESCRIPTOR_HANDLE GetImageView(unsigned i = (unsigned)-1) const { assert(!is_backbuffer); return (D3D12_CPU_DESCRIPTOR_HANDLE)image_views[i != (unsigned)-1 ? i : current]; }
		const D3D12_CPU_DESCRIPTOR_HANDLE GetMipImageView(unsigned index) const { assert(use_mipmaps); assert(index < mips_image_views.size()); return (D3D12_CPU_DESCRIPTOR_HANDLE)mips_image_views[index]; }

		PixelFormat GetPixelFormat() const { return pixel_format; }
	};
}