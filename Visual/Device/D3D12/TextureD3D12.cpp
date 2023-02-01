
namespace Device
{
	constexpr size_t GetTextureDataPitchAlignment()
	{
#if !defined(_XBOX_ONE)
		return D3D12_TEXTURE_DATA_PITCH_ALIGNMENT;
#else
		return D3D12XBOX_TEXTURE_DATA_PITCH_ALIGNMENT;
#endif
	}

	bool NeedsStagingD3D12(UsageHint usage, Pool pool)
	{
		const bool dynamic = ((unsigned)usage & ((unsigned)UsageHint::DYNAMIC | (unsigned)UsageHint::ATOMIC_COUNTER)) > 0;
		return dynamic || pool == Pool::MANAGED_WITH_SYSTEMMEM || pool == Pool::SYSTEMMEM;
	}

	unsigned CountMipsD3D12(unsigned width, unsigned height)
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

	D3D12_RESOURCE_DIMENSION ConvertDimensionD3D12(Device::Dimension dimension)
	{
		switch (dimension)
		{
		case Dimension::One:		return D3D12_RESOURCE_DIMENSION_TEXTURE1D;
		case Dimension::Two:		return D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		case Dimension::Three:		return D3D12_RESOURCE_DIMENSION_TEXTURE3D;
		case Dimension::Cubemap:	return D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		default:					throw ExceptionD3D12("Unsupported dimension");
		}
	}

	D3D12_SRV_DIMENSION ConvertViewDimensionD3D12(Device::Dimension dimension)
	{
		switch (dimension)
		{
		case Dimension::One:		return D3D12_SRV_DIMENSION_TEXTURE1D;
		case Dimension::Two:		return D3D12_SRV_DIMENSION_TEXTURE2D;
		case Dimension::Three:		return D3D12_SRV_DIMENSION_TEXTURE3D;
		case Dimension::Cubemap:	return D3D12_SRV_DIMENSION_TEXTURECUBE;
		default:					throw ExceptionD3D12("Unsupported dimension");
		}
	}

	TextureD3D12::TextureD3D12(const Memory::DebugStringA<>& name, IDevice* device, bool srgb)
		: Texture(name)
		, device(static_cast<IDeviceD3D12*>(device))
		, use_srgb(srgb)
	{
	}

	TextureD3D12::TextureD3D12(const Memory::DebugStringA<>& name, IDevice* device, SwapChain* swap_chain)
		: TextureD3D12(name, device, true)
	{
		CreateFromSwapChain(swap_chain);
	}

	TextureD3D12::TextureD3D12(const Memory::DebugStringA<>& name, IDevice* device, UINT Width, UINT Height, UINT Levels, UsageHint Usage, PixelFormat Format, Pool pool, bool useSRGB, bool use_ESRAM, bool use_mipmaps, bool enable_gpu_write)
		: TextureD3D12(name, device, useSRGB)
	{
		CreateEmpty(Width, Height, 1, Levels, Usage, pool, Format, Dimension::Two, use_mipmaps, enable_gpu_write, use_ESRAM);
	}

	TextureD3D12::TextureD3D12(const Memory::DebugStringA<>& name, IDevice* device, UINT Width, UINT Height, unsigned char* pixels, UINT bytes_per_pixel)
		: TextureD3D12(name, device, false)
	{
		CreateFromBytes(Width, Height, pixels, bytes_per_pixel);
	}

	TextureD3D12::TextureD3D12(const Memory::DebugStringA<>& name, IDevice* device, LPCWSTR pSrcFile, UINT width, UINT height, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool, TextureFilter filter, TextureFilter mipFilter, simd::color colorKey, ImageInfo* pSrcInfo, PaletteEntry* outPalette)
		: TextureD3D12(name, device, true)
	{
		CreateFromFile(pSrcFile, usage, pool, mipFilter, pSrcInfo);
	}

	TextureD3D12::TextureD3D12(const Memory::DebugStringA<>& name, IDevice* device, LPCVOID pSrcData, UINT srcDataSize, UINT width, UINT height, UINT mipLevels, UsageHint usage,
		PixelFormat format, Pool pool, TextureFilter filter, TextureFilter mipFilter, simd::color colorKey, ImageInfo* pSrcInfo, PaletteEntry* outPalette, bool useSRGB, bool premultiply_alpha)
		: TextureD3D12(name, device, useSRGB)
	{
		CreateFromMemory(pSrcData, srcDataSize, usage, pool, mipFilter, pSrcInfo, premultiply_alpha);
	}

	TextureD3D12::TextureD3D12(const Memory::DebugStringA<>& name, IDevice* device, UINT edgeLength, UINT levels, UsageHint usage, PixelFormat format, Pool pool, bool useSRGB, bool enable_gpu_write)
		: TextureD3D12(name, device, useSRGB)
	{
		 CreateEmpty(edgeLength, edgeLength, 1, levels, usage, pool, format, Dimension::Cubemap, false, enable_gpu_write);
	}

	TextureD3D12::TextureD3D12(const Memory::DebugStringA<>& name, IDevice* device, LPCWSTR pSrcFile)
		: TextureD3D12(name, device, true)
	{
		 CreateFromFile(pSrcFile, UsageHint::DEFAULT, Pool::DEFAULT, TextureFilter::NONE, nullptr);
	}

	TextureD3D12::TextureD3D12(const Memory::DebugStringA<>& name, IDevice* device, LPCWSTR pSrcFile, UINT size, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool, TextureFilter filter, TextureFilter mipFilter, simd::color colorKey, ImageInfo* outSrcInfo, PaletteEntry* outPalette)
		: TextureD3D12(name, device, true)
	{
		CreateFromFile(pSrcFile, usage, pool, mipFilter, outSrcInfo);
	}

	TextureD3D12::TextureD3D12(const Memory::DebugStringA<>& name, IDevice* device, LPCVOID pSrcData, UINT srcDataSize, UINT size, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool, TextureFilter filter, TextureFilter mipFilter,
		simd::color colorKey, ImageInfo* pSrcInfo, PaletteEntry* outPalette, bool useSRGB)
		: TextureD3D12(name, device, useSRGB)
	{
		CreateFromMemory(pSrcData, srcDataSize, usage, pool, mipFilter, pSrcInfo, false);
	}

	TextureD3D12::TextureD3D12(const Memory::DebugStringA<>& name, IDevice* device, LPCVOID pSrcData, UINT srcDataSize, UINT width, UINT height, UINT depth, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool,
		TextureFilter filter, TextureFilter mipFilter, simd::color colorKey, ImageInfo* pSrcInfo, PaletteEntry* outPalette, bool useSRGB)
		: TextureD3D12(name, device, useSRGB)
	{
		CreateFromMemory(pSrcData, srcDataSize, usage, pool, mipFilter, pSrcInfo, false);
	}

	TextureD3D12::TextureD3D12(const Memory::DebugStringA<>& name, IDevice* device, UINT width, UINT height, UINT depth, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool, bool useSRGB, bool enable_gpu_write)
		: TextureD3D12(name, device, useSRGB)
	{
		CreateEmpty(width, height, depth, mipLevels, usage, pool, format, Dimension::Three, false, enable_gpu_write);
	}

	TextureD3D12::TextureD3D12(const Memory::DebugStringA<>& name, IDevice* device, LPCWSTR pSrcFile, UINT width, UINT height, UINT depth, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool, TextureFilter filter, TextureFilter mipFilter, simd::color colorKey, ImageInfo* outSrcInfo, PaletteEntry* outPalette)
		: TextureD3D12(name, device, true)
	{
		CreateFromFile(pSrcFile, usage, pool, mipFilter, outSrcInfo);
	}

	void TextureD3D12::CreateFromSwapChain(SwapChain* swap_chain)
	{
		depth = 1;
		is_backbuffer = true;
		use_mipmaps = false;
		mips_count = 1;
		use_srgb = false; // Swapchain texture format can't be srgb. Only swapchain RTV can.

		count = IDeviceD3D12::FRAME_QUEUE_COUNT;
		texture_resources.resize(IDeviceD3D12::FRAME_QUEUE_COUNT, nullptr);

		width = (uint32_t)swap_chain->GetWidth();
		height = (uint32_t)swap_chain->GetHeight();
		pixel_format = UnconvertPixelFormat(SwapChainD3D12::SWAP_CHAIN_FORMAT);
		auto dxgi_swap_chain = static_cast<SwapChainD3D12*>(swap_chain)->GetSwapChain();

		for (UINT n = 0; n < IDeviceD3D12::FRAME_QUEUE_COUNT; n++)
		{
			dxgi_swap_chain->GetBuffer(n, IID_GRAPHICS_PPV_ARGS(texture_resources[n].GetAddressOf()));
			IDeviceD3D12::DebugName(texture_resources[n], GetName() + " " + std::to_string(n));
		}
	}

	D3D12_SHADER_RESOURCE_VIEW_DESC CreateTextureSRVDesc(PixelFormat Format, bool use_srgb, Dimension dimension, uint32_t mips_count, uint32_t mip_index)
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = GetSRVFormatD3D12(ConvertPixelFormatD3D12(Format, use_srgb));
		srvDesc.ViewDimension = ConvertViewDimensionD3D12(dimension);

		switch (srvDesc.ViewDimension)
		{
		case D3D12_SRV_DIMENSION_TEXTURE1D:
			srvDesc.Texture1D.MipLevels = mips_count;
			srvDesc.Texture1D.MostDetailedMip = mip_index;
			srvDesc.Texture1D.ResourceMinLODClamp = 0;
			break;
		case D3D12_SRV_DIMENSION_TEXTURE2D:
			srvDesc.Texture2D.MipLevels = mips_count;
			srvDesc.Texture2D.MostDetailedMip = mip_index;
			srvDesc.Texture2D.ResourceMinLODClamp = 0;
			srvDesc.Texture2D.PlaneSlice = 0;
			break;
		case D3D12_SRV_DIMENSION_TEXTURE3D:
			srvDesc.Texture3D.MipLevels = mips_count;
			srvDesc.Texture3D.MostDetailedMip = mip_index;
			srvDesc.Texture3D.ResourceMinLODClamp = 0;
			break;
		case D3D12_SRV_DIMENSION_TEXTURECUBE:
			srvDesc.TextureCube.MipLevels = mips_count;
			srvDesc.TextureCube.MostDetailedMip = mip_index;
			srvDesc.TextureCube.ResourceMinLODClamp = 0;
			break;
		default:
			throw ExceptionD3D12("Unsupported view dimension");
		}

		return srvDesc;
	}

	void TextureD3D12::Create(unsigned Width, unsigned Height, unsigned Depth, unsigned Levels, UsageHint Usage, Pool pool, PixelFormat Format, Device::Dimension Dimension, bool mipmaps, bool enable_gpu_write, bool use_ESRAM)
	{
		width = Width;
		height = Height;
		depth = Depth;
		is_backbuffer = false;
		use_mipmaps = mipmaps;
		mips_count = mipmaps ? CountMipsD3D12(Width, Height) : std::max((unsigned)1, Levels);
		pixel_format = Format;
		dimension = Dimension;

		const uint32_t render_target_flag = (uint32_t)Usage & ((uint32_t)UsageHint::RENDERTARGET) ? D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET : 0;
		const uint32_t depth_stencil_flag = (uint32_t)Usage & ((uint32_t)UsageHint::DEPTHSTENCIL) ? D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL : 0;
		const uint32_t uav_flag = enable_gpu_write ? (uint32_t)D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : 0;

		D3D12_RESOURCE_DESC texture_desc = {};
		texture_desc.MipLevels = mips_count;
		texture_desc.Format = ConvertPixelFormatD3D12(pixel_format, use_srgb);
		texture_desc.Width = width;
		texture_desc.Height = height;
		texture_desc.Flags = (D3D12_RESOURCE_FLAGS)(render_target_flag | depth_stencil_flag | uav_flag);
		texture_desc.DepthOrArraySize = depth;
		texture_desc.SampleDesc.Count = 1;
		texture_desc.SampleDesc.Quality = 0;
		texture_desc.Dimension = ConvertDimensionD3D12(dimension);

		const bool render_target = ((unsigned)Usage & (unsigned)UsageHint::RENDERTARGET) > 0;
		const bool dynamic = ((unsigned)Usage & (unsigned)UsageHint::DYNAMIC) > 0;
		count = dynamic ? IDeviceD3D12::FRAME_QUEUE_COUNT : 1;

		ComPtr<ID3D12Resource> texture;

		const auto initial_resource_state = depth_stencil_flag ? ResourceState::ShaderReadMask : ResourceState::ShaderRead;
		const auto initial_state = ConvertResourceStateD3D12(initial_resource_state);

		const auto allocator_type = ((uint32_t)Usage & ((uint32_t)UsageHint::RENDERTARGET | (uint32_t)UsageHint::DEPTHSTENCIL)) ? AllocatorD3D12::Type::RenderTarget : AllocatorD3D12::Type::Texture;
		handle = device->GetAllocator()->CreateResource(GetName(), allocator_type, texture_desc, CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), initial_state, use_ESRAM, texture);

		if (!handle)
			throw ExceptionD3D12("TextureD3D12::Create() CreatePlacedResourceX failed");

		allocation_size = handle->GetAllocationInfo().GetSize();

		IDeviceD3D12::DebugName(texture, GetName());

		for (unsigned i = 0; i < count; ++i)
		{
			texture_resources.push_back(texture);
			auto image_view = device->GetCBViewSRViewUAViewDescriptorHeap().Allocate();
			const D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = CreateTextureSRVDesc(Format, use_srgb, dimension, mips_count, 0);
			device->GetDevice()->CreateShaderResourceView(texture_resources[i].Get(), &srvDesc, (D3D12_CPU_DESCRIPTOR_HANDLE)image_view);
			image_views.push_back(std::move(image_view));
		}

		if (use_mipmaps)
		{
			const auto bytes_per_pixel = GetBitsPerPixel(pixel_format) / 8; // Wrong to use for block compressed types (pitch is per block count, not pixels).

			unsigned mip_width = width;
			unsigned mip_height = height;
			size_t offset = 0;

			for (unsigned i = 0; i < mips_count; ++i)
			{
				auto image_view = device->GetCBViewSRViewUAViewDescriptorHeap().Allocate();
				const D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = CreateTextureSRVDesc(Format, use_srgb, dimension, mips_count - i, i);
				device->GetDevice()->CreateShaderResourceView(texture_resources.front().Get(), &srvDesc, (D3D12_CPU_DESCRIPTOR_HANDLE)image_view);
				mips_image_views.emplace_back(std::move(image_view));
				mips_sizes.emplace_back(mip_width, mip_height, offset);
				offset += mip_width * mip_height * bytes_per_pixel;
				mip_width = mip_width > 1 ? mip_width / 2 : 1;
				mip_height = mip_height > 1 ? mip_height / 2 : 1;
			}
		}

		if (enable_gpu_write)
		{
			unordered_access_view = device->GetCBViewSRViewUAViewDescriptorHeap().Allocate();
			
			D3D12_UNORDERED_ACCESS_VIEW_DESC uavdesc = {};
			uavdesc.Format = (texture_desc.Format == DXGI_FORMAT_R24G8_TYPELESS) ? DXGI_FORMAT_R24_UNORM_X8_TYPELESS : texture_desc.Format;
			uavdesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
			uavdesc.Texture2D.MipSlice = 0;
			uavdesc.Texture2D.PlaneSlice = 0;

			device->GetDevice()->CreateUnorderedAccessView(texture_resources.front().Get(), nullptr, &uavdesc, (D3D12_CPU_DESCRIPTOR_HANDLE)unordered_access_view);
		}

		if (NeedsStagingD3D12(Usage, pool))
		{
			const auto subresource_count = GetSubresourceCount();
			const auto staging_total_size = GetRequiredIntermediateSize(texture_resources.front().Get(), 0, subresource_count);

			D3D12_HEAP_PROPERTIES heap_props = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

			// Using host cached memory if there a chance that memory will be read by the cpu
			if (pool == Pool::MANAGED_WITH_SYSTEMMEM || pool == Pool::SYSTEMMEM)
				heap_props = CD3DX12_HEAP_PROPERTIES(D3D12_CPU_PAGE_PROPERTY_WRITE_BACK, D3D12_MEMORY_POOL_L0, 0, 0);

			for (unsigned i = 0; i < count; ++i)
				datas.emplace_back(std::make_shared<AllocatorD3D12::Data>(Memory::DebugStringA<>("Texture Data Lock ") + GetName(), device, AllocatorD3D12::Type::Staging, staging_total_size, heap_props));
		}
	}

	void TextureD3D12::CreateEmpty(unsigned Width, unsigned Height, unsigned Depth, unsigned Levels, UsageHint Usage, Pool pool, PixelFormat Format, Dimension dimension, bool mipmaps, bool enable_gpu_write, bool use_ESRAM)
	{
		Create(Width, Height, Depth, Levels, Usage, pool, Format, dimension, mipmaps, enable_gpu_write, use_ESRAM);
	}

	void TextureD3D12::CreateFromBytes(UINT Width, UINT Height, unsigned char* pixels, UINT bytes_per_pixel)
	{
		Create(Width, Height, 1, 1, UsageHint::DEFAULT, Pool::DEFAULT, PixelFormat::A8R8G8B8, Dimension::Two, false, false);

		Fill(pixels, bytes_per_pixel * Width);
	}

	void TextureD3D12::CreateFromMemoryDDS(const void* pSrcData, size_t srcDataSize, UsageHint Usage, Pool pool, TextureFilter mipFilter, bool premultiply_alpha)
	{
		DDS dds((uint8_t*)pSrcData, srcDataSize);

		unsigned width = dds.GetWidth();
		unsigned height = dds.GetHeight();
		unsigned mip_start = 0;
		unsigned mip_count = dds.GetMipCount();
		SkipMips(width, height, mip_start, mip_count, mipFilter, dds.GetPixelFormat());

		Create(width, height, dds.GetDepth() * dds.GetArraySize(), mip_count, Usage, pool, dds.GetPixelFormat(), dds.GetDimension(), false, false);

		if (premultiply_alpha)
		{
			// TODO: Premultiply if not compressed.
		}

		Copy(dds.GetSubResources(), dds.GetArraySize(), mip_start, mips_count);
	}

	void TextureD3D12::CreateFromMemoryPNG(const void* pSrcData, size_t srcDataSize, UsageHint Usage, Pool pool, TextureFilter mipFilter, bool premultiply_alpha)
	{
		PNG png(GetName(), (uint8_t*)pSrcData, srcDataSize);

		const unsigned array_size = 1;
		const unsigned mip_start = 0;
		const unsigned mip_count = 1;

		Create(png.GetWidth(), png.GetHeight(), array_size, mip_count, Usage, pool, png.GetPixelFormat(), png.GetDimension(), false, false);

		if (premultiply_alpha)
		{
			auto ptr = reinterpret_cast<simd::color*>(png.GetImageData());
			const auto end = ptr + (png.GetWidth() * png.GetHeight());
			for (; ptr != end; ++ptr)
			{
				const double a = ptr->a / 255.0;
				ptr->r = static_cast<uint8_t>(ptr->r * a);
				ptr->g = static_cast<uint8_t>(ptr->g * a);
				ptr->b = static_cast<uint8_t>(ptr->b * a);
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

	void TextureD3D12::CreateFromMemoryJPG(const void* pSrcData, size_t srcDataSize, UsageHint Usage, Pool pool, TextureFilter mipFilter)
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

	void TextureD3D12::CreateDefault()
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

	void TextureD3D12::CreateFromMemory(const void* pSrcData, size_t srcDataSize, UsageHint Usage, Pool pool, TextureFilter mipFilter, ImageInfo* out_info, bool premultiply_alpha)
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
			LOG_WARN(L"[D3D12] Failed to load texture " << StringToWstring(GetName().c_str()) << L"(Error: " << StringToWstring(e.what()) << L")");
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

	void TextureD3D12::CreateFromFile(const wchar_t* pSrcFile, UsageHint Usage, Pool pool, TextureFilter mipFilter, ImageInfo* out_info)
	{
		File::InputFileStream stream(pSrcFile);

		CreateFromMemory(stream.GetFilePointer(), stream.GetFileSize(), Usage, pool, mipFilter, out_info, false);
	}

	void TextureD3D12::CopyTo(TextureD3D12& dest)
	{
		assert(width == dest.width);
		assert(height == dest.height);
		assert(depth == dest.depth);

		{
			auto* src_resource = texture_resources.front().Get();
			auto* dst_resource = dest.GetResource(0);
			auto& subresource_footprints = GetFootprints();
			auto& footprints = subresource_footprints.footprints;
			const uint32_t subresource_index = 0;

			BufferImageCopiesD3D12 copies;
			BufferImageCopyD3D12 copy;
			copy.src = CD3DX12_TEXTURE_COPY_LOCATION(src_resource, subresource_index);
			copy.dst = CD3DX12_TEXTURE_COPY_LOCATION(dst_resource, subresource_index);
			copies.push_back(copy);

			auto upload = std::make_unique<CopyImageToImageLoadD3D12>(GetName(), src_resource, dst_resource, copies, is_backbuffer);
			device->GetTransfer().Add(std::move(upload));
		}

		if (auto data = dest.GetData())
		{
			auto src_resource = texture_resources.front();
			auto dst_resource = dest.texture_resources.front();
			auto& subresource_footprints = GetFootprints();
			auto& footprints = subresource_footprints.footprints;
			const uint32_t subresource_index = 0;

			BufferImageCopiesD3D12 copies;
			BufferImageCopyD3D12 copy;
			copy.src = CD3DX12_TEXTURE_COPY_LOCATION(src_resource.Get(), subresource_index);
			copy.dst = CD3DX12_TEXTURE_COPY_LOCATION(data->GetBuffer(), footprints[subresource_index]);
			copies.push_back(copy);

			auto download = std::make_unique<CopyImageToDataLoadD3D12>(GetName(), src_resource, data, copies, is_backbuffer);
			device->GetTransfer().Add(std::move(download));
		}

		device->GetTransfer().Flush(); // Need the result right now if LockRect. // TODO: Remove.
		device->GetTransfer().Wait();
	}

	void TextureD3D12::LockRect(unsigned level, LockedRect* outLockedRect, Lock lock)
	{
		assert(level == 0 || use_mipmaps);
		assert(level < mips_count);

		if (lock == Device::Lock::DISCARD)
		{
			assert(count > 1 && "Lock::DISCARD requires UsageHint::DYNAMIC");
			current = (current + 1) % count;
		}

		const uint32_t subresource_index = level;

		auto& subresource_footprints = GetFootprints();
		auto& footprints = subresource_footprints.footprints;
		auto& row_count = subresource_footprints.row_count;
		auto& row_sizes = subresource_footprints.row_sizes;

		D3D12_MEMCPY_DEST DestData = { GetData()->GetMap() + footprints[subresource_index].Offset, footprints[subresource_index].Footprint.RowPitch, SIZE_T(footprints[subresource_index].Footprint.RowPitch) * SIZE_T(row_count[subresource_index]) };

		outLockedRect->pBits = DestData.pData;
		outLockedRect->Pitch = (int)DestData.RowPitch;
	}

	void TextureD3D12::UnlockRect(unsigned level)
	{
		auto* dst_resource = texture_resources.front().Get();
		auto& subresource_footprints = GetFootprints();
		auto& footprints = subresource_footprints.footprints;
		const uint32_t subresource_index = level;

		BufferImageCopiesD3D12 copies;
		BufferImageCopyD3D12 copy;
		copy.dst = CD3DX12_TEXTURE_COPY_LOCATION(dst_resource, subresource_index);
		copy.src = CD3DX12_TEXTURE_COPY_LOCATION(GetData()->GetBuffer(), footprints[subresource_index]);
		copies.push_back(copy);

		auto upload = std::make_unique<CopyDataToImageLoadD3D12>(GetName(), texture_resources.front(), GetData(), copies);
		device->GetTransfer().Add(std::move(upload));
	}

	void TextureD3D12::LockBox(UINT level, LockedBox* outLockedVolume, Lock lock)
	{
	}

	void TextureD3D12::UnlockBox(UINT level)
	{
	}

	TextureD3D12::Footprints& TextureD3D12::GetFootprints()
	{
		if (subresource_footprints.footprints.empty())
		{
			const auto subresource_count = GetSubresourceCount();

			subresource_footprints.footprints.resize(subresource_count, D3D12_PLACED_SUBRESOURCE_FOOTPRINT());
			subresource_footprints.row_count.resize(subresource_count, 0);
			subresource_footprints.row_sizes.resize(subresource_count, 0);

			auto desc = texture_resources.front().Get()->GetDesc();
			device->GetDevice()->GetCopyableFootprints(
				&desc, 0, GetSubresourceCount(), 0, subresource_footprints.footprints.data(), subresource_footprints.row_count.data(), subresource_footprints.row_sizes.data(), nullptr
			);
		}

		return subresource_footprints;
	}

	void TextureD3D12::Fill(void* data, unsigned pitch)
	{
		const auto staging_size = GetRequiredIntermediateSize(texture_resources.front().Get(), 0, mips_count * depth);
		auto staging_data = std::make_shared<AllocatorD3D12::Data>(Memory::DebugStringA<>("Texture Data Fill ") + GetName(), device, AllocatorD3D12::Type::Staging, staging_size, CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD));

		uint8_t* map = staging_data->GetMap();
		const auto pitch_in_bytes = Memory::AlignSize(width * GetBitsPerPixel(pixel_format) / 8, (uint32_t)GetTextureDataPitchAlignment());

		for (unsigned i = 0; i < height; ++i)
		{
			memcpy(map + pitch_in_bytes * i, (uint8_t*)data + pitch * i, pitch);
		}

		BufferImageCopiesD3D12 copies;
		BufferImageCopyD3D12 copy;

		copy.src = CD3DX12_TEXTURE_COPY_LOCATION(staging_data->GetBuffer(), { 0, CD3DX12_SUBRESOURCE_FOOTPRINT(ConvertPixelFormatD3D12(pixel_format, use_srgb), width, height, depth, pitch_in_bytes) });
		copy.dst = CD3DX12_TEXTURE_COPY_LOCATION(texture_resources.front().Get(), 0);
		copies.emplace_back(copy);

		auto upload = std::make_unique<CopyDataToImageLoadD3D12>(GetName(), texture_resources.front(), std::move(staging_data), copies);

		device->GetTransfer().Add(std::move(upload));
	}

	void TextureD3D12::Copy(const std::vector<Mip>& mips, unsigned array_count, unsigned mip_start, unsigned mip_count)
	{
		auto* dst_resource = texture_resources.front().Get();

		const auto subresource_count = array_count * mip_count;
		const auto total_size = GetRequiredIntermediateSize(dst_resource, 0, subresource_count);
		if (total_size == -1)
			throw ExceptionD3D12("TextureD3D12::Copy() GetRequiredIntermediateSize Failed");

		auto staging_data = std::make_shared<AllocatorD3D12::Data>(Memory::DebugStringA<>("Texture Data Copy ") + GetName(), device, AllocatorD3D12::Type::Staging, total_size, CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD));

		uint8_t* map = staging_data->GetMap();

		UINT64 required_size = 0;

		auto& subresource_footprints = GetFootprints();
		auto& footprints = subresource_footprints.footprints;
		auto& row_count = subresource_footprints.row_count;
		auto& row_sizes = subresource_footprints.row_sizes;

		BufferImageCopiesD3D12 copies;

		uint32_t subresource_index = 0;
		for (unsigned array_index = 0; array_index < array_count; ++array_index)
		{
			for (unsigned mip_index = mip_start; mip_index < (mip_start + mip_count); ++mip_index)
			{
				const auto& mip = mips[array_index * (mip_start + mip_count) + mip_index];

				D3D12_MEMCPY_DEST DestData = { map + footprints[subresource_index].Offset, footprints[subresource_index].Footprint.RowPitch, SIZE_T(footprints[subresource_index].Footprint.RowPitch) * SIZE_T(row_count[subresource_index]) };

				const LONG_PTR row_size = mip.row_size;
				const LONG_PTR slice_size = mip.height * mip.row_size;
				D3D12_SUBRESOURCE_DATA subresource_data{ mip.data, row_size, slice_size };
				MemcpySubresource(&DestData, &subresource_data, static_cast<SIZE_T>(row_sizes[subresource_index]), row_count[subresource_index], footprints[subresource_index].Footprint.Depth);

				for (unsigned i = 0; i < std::min(count, (uint32_t)datas.size()); i++)
				{
					auto* staging_map = datas[i]->GetMap();
					D3D12_MEMCPY_DEST DestData = { staging_map + footprints[subresource_index].Offset, footprints[subresource_index].Footprint.RowPitch, SIZE_T(footprints[subresource_index].Footprint.RowPitch) * SIZE_T(row_count[subresource_index]) };
					MemcpySubresource(&DestData, &subresource_data, static_cast<SIZE_T>(row_sizes[subresource_index]), row_count[subresource_index], footprints[subresource_index].Footprint.Depth);
				}

				BufferImageCopyD3D12 copy;
				copy.dst = CD3DX12_TEXTURE_COPY_LOCATION(dst_resource, subresource_index);
				copy.src = CD3DX12_TEXTURE_COPY_LOCATION(staging_data->GetBuffer(), footprints[subresource_index]);
				copies.push_back(copy);
				
				subresource_index++;
			}
		}

		auto upload = std::make_unique<CopyDataToImageLoadD3D12>(GetName(), texture_resources.front(), std::move(staging_data), copies);
		device->GetTransfer().Add(std::move(upload));
	}

	void TextureD3D12::TransferFrom(Texture* src_texture)
	{
	}
	void TextureD3D12::TranscodeFrom(Texture* src_texture)
	{
	}

	void TextureD3D12::Swap()
	{
		current = (current + 1) % count;
	}

	void TextureD3D12::GetLevelDesc(UINT level, SurfaceDesc* outDesc)
	{
		if (level > 0)
			throw ExceptionD3D12("LevelDesc > 0 not supported");

		if (outDesc)
		{
			outDesc->Format = GetPixelFormat();
			outDesc->Width = width;
			outDesc->Height = height;
		}
	}

	void TextureD3D12::SaveToFile(LPCWSTR pDestFile, ImageFileFormat DestFormat)
	{
#if !defined(CONSOLE)
		if (DestFormat == ImageFileFormat::PNG)
		{
			assert(pixel_format == PixelFormat::A8R8G8B8);
			assert(datas[0]);

			auto& subresource_footprints = GetFootprints();
			auto& footprints = subresource_footprints.footprints;
			assert(footprints.size());
			auto& footprint0 = footprints[0];
			auto& row_sizes = subresource_footprints.row_sizes;
			auto& row_count = subresource_footprints.row_count;

			std::vector<uint8_t> rgba(width * height * sizeof(simd::color));
			const uint8_t* const src = datas[0]->GetMap();

			D3D12_MEMCPY_DEST DestData = { rgba.data(), width * sizeof(simd::color), rgba.size() };

			const LONG_PTR row_size = footprint0.Footprint.RowPitch;
			const LONG_PTR slice_size = footprint0.Footprint.Height * row_size;
			D3D12_SUBRESOURCE_DATA subresource_data{ src + footprint0.Offset, row_size, slice_size };
			MemcpySubresource(&DestData, &subresource_data, static_cast<SIZE_T>(row_sizes[0]), row_count[0], footprints[0].Footprint.Depth);

			std::vector< unsigned char > buffer;
			const unsigned error = lodepng::encode(buffer, rgba.data(), width, height, LodePNGColorType::LCT_RGBA, 8);
			if (error || buffer.empty())
				throw ExceptionD3D12("Failed to save PNG image");

			std::ofstream fout(PathStr(pDestFile), std::ios::binary);
			fout.write((const char*)buffer.data(), buffer.size());
			if (fout.fail())
				throw ExceptionD3D12("Failed to save PNG image");
		}
		else if (DestFormat == ImageFileFormat::DDS)
		{
			// [TODO] 
			throw ExceptionD3D12("Image format not supported yet");
		}
		else
		{

			throw ExceptionD3D12("Image format not supported yet");
		}
#endif
	}

}
