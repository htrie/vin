
namespace Device
{

	bool Texture::IsValidTextureMagic(uint32_t magic)
	{
		if (magic == DDS::MAGIC || magic == PNG::MAGIC || ((magic & JPG::MAGIC_MASK) == JPG::MAGIC) || magic == KTX::MAGIC)
		{
			return true;
		}

		return false;
	}

	Device::Handle<Texture> Texture::CreateTexture(const Memory::DebugStringA<>& name, IDevice* device, UINT Width, UINT Height, UINT Levels, UsageHint Usage, PixelFormat Format, Pool pool, bool useSRGB, bool use_ESRAM, bool use_mipmaps, bool enable_gpu_write)
	{
		PROFILE_ONLY(Memory::StackTag tag(Memory::Tag::Texture);)
		switch (IDevice::GetType())
		{
		#if defined(ENABLE_VULKAN)
		case Type::Vulkan: return Device::Handle<Texture>(new TextureVulkan(name, device, Width, Height, Levels, Usage, Format, pool, useSRGB, use_ESRAM, use_mipmaps, enable_gpu_write));
		#endif
		#if defined(ENABLE_D3D11)
		case Type::DirectX11: return Device::Handle<Texture>(new TextureD3D11(name, device, Width, Height, Levels, Usage, Format, pool, useSRGB, use_ESRAM, use_mipmaps, enable_gpu_write));
		#endif
		#if defined(ENABLE_D3D12)
		case Type::DirectX12: return Device::Handle<Texture>(new TextureD3D12(name, device, Width, Height, Levels, Usage, Format, pool, useSRGB, use_ESRAM, use_mipmaps, enable_gpu_write));
		#endif
		#if defined(ENABLE_GNMX)
		case Type::GNMX: return Device::Handle<Texture>(new TextureGNMX(name, device, Width, Height, Levels, Usage, Format, pool, useSRGB, use_ESRAM, use_mipmaps, enable_gpu_write));
		#endif
		#if defined(ENABLE_NULL)
		case Type::Null: return Device::Handle<Texture>(new TextureNull(name, device, Width, Height, Levels, Usage, Format, pool, useSRGB, use_ESRAM, use_mipmaps, enable_gpu_write));
		#endif
		default: throw std::runtime_error("Unknow device type");
		}
	}

	Device::Handle<Texture> Texture::CreateTextureFromMemory(const Memory::DebugStringA<>& name, IDevice* device, UINT Width, UINT Height, unsigned char* pixels, UINT bytes_per_pixel)
	{
		PROFILE_ONLY(Memory::StackTag tag(Memory::Tag::Texture);)
		switch (IDevice::GetType())
		{
		#if defined(ENABLE_VULKAN)
		case Type::Vulkan: return Device::Handle<Texture>(new TextureVulkan(name, device, Width, Height, pixels, bytes_per_pixel));
		#endif
		#if defined(ENABLE_D3D11)
		case Type::DirectX11: return Device::Handle<Texture>(new TextureD3D11(name, device, Width, Height, pixels, bytes_per_pixel));
		#endif
		#if defined(ENABLE_D3D12)
		case Type::DirectX12: return Device::Handle<Texture>(new TextureD3D12(name, device, Width, Height, pixels, bytes_per_pixel));
		#endif
		#if defined(ENABLE_GNMX)
		case Type::GNMX: return Device::Handle<Texture>(new TextureGNMX(name, device, Width, Height, pixels, bytes_per_pixel));
		#endif
		#if defined(ENABLE_NULL)
		case Type::Null: return Device::Handle<Texture>(new TextureNull(name, device, Width, Height, pixels, bytes_per_pixel));
		#endif
		default: throw std::runtime_error("Unknow device type");
		}
	}

	Device::Handle<Texture> Texture::CreateTextureFromFileEx(const Memory::DebugStringA<>& name, IDevice* device, LPCWSTR pSrcFile, UINT width, UINT height, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool, TextureFilter filter, TextureFilter mipFilter, simd::color colorKey, ImageInfo* pSrcInfo, PaletteEntry* outPalette)
	{
		PROFILE_ONLY(Memory::StackTag tag(Memory::Tag::Texture);)
		switch (IDevice::GetType())
		{
		#if defined(ENABLE_VULKAN)
		case Type::Vulkan: return Device::Handle<Texture>(new TextureVulkan(name, device, pSrcFile, width, height, mipLevels, usage, format, pool, filter, mipFilter, colorKey, pSrcInfo, outPalette));
		#endif
		#if defined(ENABLE_D3D11)
		case Type::DirectX11: return Device::Handle<Texture>(new TextureD3D11(name, device, pSrcFile, width, height, mipLevels, usage, format, pool, filter, mipFilter, colorKey, pSrcInfo, outPalette));
		#endif
		#if defined(ENABLE_D3D12)
		case Type::DirectX12: return Device::Handle<Texture>(new TextureD3D12(name, device, pSrcFile, width, height, mipLevels, usage, format, pool, filter, mipFilter, colorKey, pSrcInfo, outPalette));
		#endif
		#if defined(ENABLE_GNMX)
		case Type::GNMX: return Device::Handle<Texture>(new TextureGNMX(name, device, pSrcFile, width, height, mipLevels, usage, format, pool, filter, mipFilter, colorKey, pSrcInfo, outPalette));
		#endif
		#if defined(ENABLE_NULL)
		case Type::Null: return Device::Handle<Texture>(new TextureNull(name, device, pSrcFile, width, height, mipLevels, usage, format, pool, filter, mipFilter, colorKey, pSrcInfo, outPalette));
		#endif
		default: throw std::runtime_error("Unknow device type");
		}
	}

	Device::Handle<Texture> Texture::CreateTextureFromFileInMemoryEx(const Memory::DebugStringA<>& name, IDevice* device, LPCVOID pSrcData, UINT srcDataSize, UINT width, UINT height, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool, TextureFilter filter, TextureFilter mipFilter, simd::color colorKey, ImageInfo* pSrcInfo, PaletteEntry* outPalette, bool useSRGB, bool premultiply_alpha)
	{
		PROFILE_ONLY(Memory::StackTag tag(Memory::Tag::Texture);)
		switch (IDevice::GetType())
		{
		#if defined(ENABLE_VULKAN)
		case Type::Vulkan: return Device::Handle<Texture>(new TextureVulkan(name, device, pSrcData, srcDataSize, width, height, mipLevels, usage, format, pool, filter, mipFilter, colorKey, pSrcInfo, outPalette, useSRGB, premultiply_alpha));
		#endif
		#if defined(ENABLE_D3D11)
		case Type::DirectX11: return Device::Handle<Texture>(new TextureD3D11(name, device, pSrcData, srcDataSize, width, height, mipLevels, usage, format, pool, filter, mipFilter, colorKey, pSrcInfo, outPalette, useSRGB, premultiply_alpha));
		#endif
		#if defined(ENABLE_D3D12)
		case Type::DirectX12: return Device::Handle<Texture>(new TextureD3D12(name, device, pSrcData, srcDataSize, width, height, mipLevels, usage, format, pool, filter, mipFilter, colorKey, pSrcInfo, outPalette, useSRGB, premultiply_alpha));
		#endif
		#if defined(ENABLE_GNMX)
		case Type::GNMX: return Device::Handle<Texture>(new TextureGNMX(name, device, pSrcData, srcDataSize, width, height, mipLevels, usage, format, pool, filter, mipFilter, colorKey, pSrcInfo, outPalette, useSRGB, premultiply_alpha));
		#endif
		#if defined(ENABLE_NULL)
		case Type::Null: return Device::Handle<Texture>(new TextureNull(name, device, pSrcData, srcDataSize, width, height, mipLevels, usage, format, pool, filter, mipFilter, colorKey, pSrcInfo, outPalette, useSRGB, premultiply_alpha));
		#endif
		default: throw std::runtime_error("Unknow device type");
		}
	}

	Device::Handle<Texture> Texture::CreateCubeTexture(const Memory::DebugStringA<>& name, IDevice* device, UINT edgeLength, UINT levels, UsageHint usage, PixelFormat format, Pool pool, bool useSRGB, bool enable_gpu_write)
	{
		PROFILE_ONLY(Memory::StackTag tag(Memory::Tag::Texture);)
		switch (IDevice::GetType())
		{
		#if defined(ENABLE_VULKAN)
		case Type::Vulkan: return Device::Handle<Texture>(new TextureVulkan(name, device, edgeLength, levels, usage, format, pool, useSRGB, enable_gpu_write));
		#endif
		#if defined(ENABLE_D3D11)
		case Type::DirectX11: return Device::Handle<Texture>(new TextureD3D11(name, device, edgeLength, levels, usage, format, pool, useSRGB, enable_gpu_write));
		#endif
		#if defined(ENABLE_D3D12)
		case Type::DirectX12: return Device::Handle<Texture>(new TextureD3D12(name, device, edgeLength, levels, usage, format, pool, useSRGB, enable_gpu_write));
		#endif
		#if defined(ENABLE_GNMX)
		case Type::GNMX: return Device::Handle<Texture>(new TextureGNMX(name, device, edgeLength, levels, usage, format, pool, useSRGB, enable_gpu_write));
		#endif
		#if defined(ENABLE_NULL)
		case Type::Null: return Device::Handle<Texture>(new TextureNull(name, device, edgeLength, levels, usage, format, pool, useSRGB, enable_gpu_write));
		#endif
		default: throw std::runtime_error("Unknow device type");
		}
	}

	Device::Handle<Texture> Texture::CreateCubeTextureFromFile(const Memory::DebugStringA<>& name, IDevice* device, LPCWSTR pSrcFile)
	{
		PROFILE_ONLY(Memory::StackTag tag(Memory::Tag::Texture);)
		switch (IDevice::GetType())
		{
		#if defined(ENABLE_VULKAN)
		case Type::Vulkan: return Device::Handle<Texture>(new TextureVulkan(name, device, pSrcFile));
		#endif
		#if defined(ENABLE_D3D11)
		case Type::DirectX11: return Device::Handle<Texture>(new TextureD3D11(name, device, pSrcFile));
		#endif
		#if defined(ENABLE_D3D12)
		case Type::DirectX12: return Device::Handle<Texture>(new TextureD3D12(name, device, pSrcFile));
		#endif
		#if defined(ENABLE_GNMX)
		case Type::GNMX: return Device::Handle<Texture>(new TextureGNMX(name, device, pSrcFile));
		#endif
		#if defined(ENABLE_NULL)
		case Type::Null: return Device::Handle<Texture>(new TextureNull(name, device, pSrcFile));
		#endif
		default: throw std::runtime_error("Unknow device type");
		}
	}

	Device::Handle<Texture> Texture::CreateCubeTextureFromFileEx(const Memory::DebugStringA<>& name, IDevice* device, LPCWSTR pSrcFile, UINT size, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool, TextureFilter filter, TextureFilter mipFilter, simd::color colorKey, ImageInfo* outSrcInfo, PaletteEntry* outPalette)
	{
		PROFILE_ONLY(Memory::StackTag tag(Memory::Tag::Texture);)
		switch (IDevice::GetType())
		{
		#if defined(ENABLE_VULKAN)
		case Type::Vulkan: return Device::Handle<Texture>(new TextureVulkan(name, device, pSrcFile, size, mipLevels, usage, format, pool, filter, mipFilter, colorKey, outSrcInfo, outPalette));
		#endif
		#if defined(ENABLE_D3D11)
		case Type::DirectX11: return Device::Handle<Texture>(new TextureD3D11(name, device, pSrcFile, size, mipLevels, usage, format, pool, filter, mipFilter, colorKey, outSrcInfo, outPalette));
		#endif
		#if defined(ENABLE_D3D12)
		case Type::DirectX12: return Device::Handle<Texture>(new TextureD3D12(name, device, pSrcFile, size, mipLevels, usage, format, pool, filter, mipFilter, colorKey, outSrcInfo, outPalette));
		#endif
		#if defined(ENABLE_GNMX)
		case Type::GNMX: return Device::Handle<Texture>(new TextureGNMX(name, device, pSrcFile, size, mipLevels, usage, format, pool, filter, mipFilter, colorKey, outSrcInfo, outPalette));
		#endif
		#if defined(ENABLE_NULL)
		case Type::Null: return Device::Handle<Texture>(new TextureNull(name, device, pSrcFile, size, mipLevels, usage, format, pool, filter, mipFilter, colorKey, outSrcInfo, outPalette));
		#endif
		default: throw std::runtime_error("Unknow device type");
		}
	}

	Device::Handle<Texture> Texture::CreateCubeTextureFromFileInMemoryEx(const Memory::DebugStringA<>& name, IDevice* device, LPCVOID pSrcData, UINT srcDataSize, UINT size, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool, TextureFilter filter, TextureFilter mipFilter, simd::color colorKey, ImageInfo* pSrcInfo, PaletteEntry* outPalette, bool useSRGB)
	{
		PROFILE_ONLY(Memory::StackTag tag(Memory::Tag::Texture);)
		switch (IDevice::GetType())
		{
		#if defined(ENABLE_VULKAN)
		case Type::Vulkan: return Device::Handle<Texture>(new TextureVulkan(name, device, pSrcData, srcDataSize, size, mipLevels, usage, format, pool, filter, mipFilter, colorKey, pSrcInfo, outPalette, useSRGB));
		#endif
		#if defined(ENABLE_D3D11)
		case Type::DirectX11: return Device::Handle<Texture>(new TextureD3D11(name, device, pSrcData, srcDataSize, size, mipLevels, usage, format, pool, filter, mipFilter, colorKey, pSrcInfo, outPalette, useSRGB));
		#endif
		#if defined(ENABLE_D3D12)
		case Type::DirectX12: return Device::Handle<Texture>(new TextureD3D12(name, device, pSrcData, srcDataSize, size, mipLevels, usage, format, pool, filter, mipFilter, colorKey, pSrcInfo, outPalette, useSRGB));
		#endif
		#if defined(ENABLE_GNMX)
		case Type::GNMX: return Device::Handle<Texture>(new TextureGNMX(name, device, pSrcData, srcDataSize, size, mipLevels, usage, format, pool, filter, mipFilter, colorKey, pSrcInfo, outPalette, useSRGB));
		#endif
		#if defined(ENABLE_NULL)
		case Type::Null: return Device::Handle<Texture>(new TextureNull(name, device, pSrcData, srcDataSize, size, mipLevels, usage, format, pool, filter, mipFilter, colorKey, pSrcInfo, outPalette, useSRGB));
		#endif
		default: throw std::runtime_error("Unknow device type");
		}
	}

	Device::Handle<Texture> Texture::CreateVolumeTextureFromFileInMemoryEx(const Memory::DebugStringA<>& name, IDevice* device, LPCVOID pSrcData, UINT srcDataSize, UINT width, UINT height, UINT depth, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool, TextureFilter filter, TextureFilter mipFilter, simd::color colorKey, ImageInfo* pSrcInfo, PaletteEntry* outPalette, bool useSRGB)
	{
		PROFILE_ONLY(Memory::StackTag tag(Memory::Tag::Texture);)
		switch (IDevice::GetType())
		{
		#if defined(ENABLE_VULKAN)
		case Type::Vulkan: return Device::Handle<Texture>(new TextureVulkan(name, device, pSrcData, srcDataSize, width, height, depth, mipLevels, usage, format, pool, filter, mipFilter, colorKey, pSrcInfo, outPalette, useSRGB));
		#endif
		#if defined(ENABLE_D3D11)
		case Type::DirectX11: return Device::Handle<Texture>(new TextureD3D11(name, device, pSrcData, srcDataSize, width, height, depth, mipLevels, usage, format, pool, filter, mipFilter, colorKey, pSrcInfo, outPalette, useSRGB));
		#endif
		#if defined(ENABLE_D3D12)
		case Type::DirectX12: return Device::Handle<Texture>(new TextureD3D12(name, device, pSrcData, srcDataSize, width, height, depth, mipLevels, usage, format, pool, filter, mipFilter, colorKey, pSrcInfo, outPalette, useSRGB));
		#endif
		#if defined(ENABLE_GNMX)
		case Type::GNMX: return Device::Handle<Texture>(new TextureGNMX(name, device, pSrcData, srcDataSize, width, height, depth, mipLevels, usage, format, pool, filter, mipFilter, colorKey, pSrcInfo, outPalette, useSRGB));
		#endif
		#if defined(ENABLE_NULL)
		case Type::Null: return Device::Handle<Texture>(new TextureNull(name, device, pSrcData, srcDataSize, width, height, depth, mipLevels, usage, format, pool, filter, mipFilter, colorKey, pSrcInfo, outPalette, useSRGB));
		#endif
		default: throw std::runtime_error("Unknow device type");
		}
	}

	Device::Handle<Texture> Texture::CreateVolumeTexture(const Memory::DebugStringA<>& name, IDevice* device, UINT width, UINT height, UINT depth, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool, bool useSRGB, bool enable_gpu_write)
	{
		PROFILE_ONLY(Memory::StackTag tag(Memory::Tag::Texture);)
		switch (IDevice::GetType())
		{
		#if defined(ENABLE_VULKAN)
		case Type::Vulkan: return Device::Handle<Texture>(new TextureVulkan(name, device, width, height, depth, mipLevels, usage, format, pool, useSRGB, enable_gpu_write));
		#endif
		#if defined(ENABLE_D3D11)
		case Type::DirectX11: return Device::Handle<Texture>(new TextureD3D11(name, device, width, height, depth, mipLevels, usage, format, pool, useSRGB, enable_gpu_write));
		#endif
		#if defined(ENABLE_D3D12)
		case Type::DirectX12: return Device::Handle<Texture>(new TextureD3D12(name, device, width, height, depth, mipLevels, usage, format, pool, useSRGB, enable_gpu_write));
		#endif
		#if defined(ENABLE_GNMX)
		case Type::GNMX: return Device::Handle<Texture>(new TextureGNMX(name, device, width, height, depth, mipLevels, usage, format, pool, useSRGB, enable_gpu_write));
		#endif
		#if defined(ENABLE_NULL)
		case Type::Null: return Device::Handle<Texture>(new TextureNull(name, device, width, height, depth, mipLevels, usage, format, pool, useSRGB, enable_gpu_write));
		#endif
		default: throw std::runtime_error("Unknow device type");
		}
	}

	Device::Handle<Texture> Texture::CreateVolumeTextureFromFileEx(const Memory::DebugStringA<>& name, IDevice* device, LPCWSTR pSrcFile, UINT width, UINT height, UINT depth, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool, TextureFilter filter, TextureFilter mipFilter, simd::color colorKey, ImageInfo* outSrcInfo, PaletteEntry* outPalette)
	{
		PROFILE_ONLY(Memory::StackTag tag(Memory::Tag::Texture);)
		switch (IDevice::GetType())
		{
		#if defined(ENABLE_VULKAN)
		case Type::Vulkan: return Device::Handle<Texture>(new TextureVulkan(name, device, pSrcFile, width, height, depth, mipLevels, usage, format, pool, filter, mipFilter, colorKey, outSrcInfo, outPalette));
		#endif
		#if defined(ENABLE_D3D11)
		case Type::DirectX11: return Device::Handle<Texture>(new TextureD3D11(name, device, pSrcFile, width, height, depth, mipLevels, usage, format, pool, filter, mipFilter, colorKey, outSrcInfo, outPalette));
		#endif
		#if defined(ENABLE_D3D12)
		case Type::DirectX12: return Device::Handle<Texture>(new TextureD3D12(name, device, pSrcFile, width, height, depth, mipLevels, usage, format, pool, filter, mipFilter, colorKey, outSrcInfo, outPalette));
		#endif
		#if defined(ENABLE_GNMX)
		case Type::GNMX: return Device::Handle<Texture>(new TextureGNMX(name, device, pSrcFile, width, height, depth, mipLevels, usage, format, pool, filter, mipFilter, colorKey, outSrcInfo, outPalette));
		#endif
		#if defined(ENABLE_NULL)
		case Type::Null: return Device::Handle<Texture>(new TextureNull(name, device, pSrcFile, width, height, depth, mipLevels, usage, format, pool, filter, mipFilter, colorKey, outSrcInfo, outPalette));
		#endif
		default: throw std::runtime_error("Unknow device type");
		}
	}

}
