
namespace Device
{

	inline bool IsPowerOf2(const size_t value)
	{
		return (value != 0) && ((value & (value - 1)) == 0);
	}

	class TextureD3D11 : public Texture
	{
		IDevice* pDevice;
		
		bool useSRGB = false;
		bool staging_modified = false;

		void SkipMips(const DirectX::Image*& images, size_t& image_count, DirectX::TexMetadata& metadata, TextureFilter mipFilter);

		TextureD3D11(const Memory::DebugStringA<>& name, IDevice* device, bool useSRGB);

	public:
		TextureD3D11(const Memory::DebugStringA<>& name, IDevice* device, UINT Width, UINT Height, UINT Levels, UsageHint Usage, PixelFormat Format, Pool Pool, bool useSRGB, bool use_ESRAM, bool use_mipmaps, bool enable_gpu_write);
		TextureD3D11(const Memory::DebugStringA<>& name, IDevice* device, UINT Width, UINT Height, unsigned char* pixels, UINT bytes_per_pixel);
		TextureD3D11(const Memory::DebugStringA<>& name, IDevice* device, LPCWSTR pSrcFile, UINT width, UINT height, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool, TextureFilter filter, TextureFilter mipFilter, simd::color colorKey, ImageInfo* pSrcInfo, PaletteEntry* outPalette);
		TextureD3D11(const Memory::DebugStringA<>& name, IDevice* device, LPCVOID pSrcData, UINT srcDataSize, UINT width, UINT height, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool, TextureFilter filter, TextureFilter mipFilter, simd::color colorKey, ImageInfo* pSrcInfo, PaletteEntry* outPalette, bool useSRGB, bool premultiply_alpha);
		TextureD3D11(const Memory::DebugStringA<>& name, IDevice* device, UINT edgeLength, UINT levels, UsageHint usage, PixelFormat format, Pool pool, bool useSRGB, bool enable_gpu_write);
		TextureD3D11(const Memory::DebugStringA<>& name, IDevice* device, LPCWSTR pSrcFile);
		TextureD3D11(const Memory::DebugStringA<>& name, IDevice* device, LPCWSTR pSrcFile, UINT size, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool, TextureFilter filter, TextureFilter mipFilter, simd::color colorKey, ImageInfo* outSrcInfo, PaletteEntry* outPalette);
		TextureD3D11(const Memory::DebugStringA<>& name, IDevice* device, LPCVOID pSrcData, UINT srcDataSize, UINT size, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool, TextureFilter filter, TextureFilter mipFilter, simd::color colorKey, ImageInfo* pSrcInfo, PaletteEntry* outPalette, bool useSRGB);
		TextureD3D11(const Memory::DebugStringA<>& name, IDevice* device, LPCVOID pSrcData, UINT srcDataSize, UINT width, UINT height, UINT depth, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool, TextureFilter filter, TextureFilter mipFilter, simd::color colorKey, ImageInfo* pSrcInfo, PaletteEntry* outPalette, bool useSRGB);
		TextureD3D11(const Memory::DebugStringA<>& name, IDevice* device, UINT width, UINT height, UINT depth, UINT levels, UsageHint usage, PixelFormat format, Pool pool, bool useSRGB, bool enable_gpu_write);
		TextureD3D11(const Memory::DebugStringA<>& name, IDevice* device, LPCWSTR pSrcFile, UINT width, UINT height, UINT depth, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool, TextureFilter filter, TextureFilter mipFilter, simd::color colorKey, ImageInfo* outSrcInfo, PaletteEntry* outPalette);

		virtual void LockRect(UINT level, LockedRect* outLockedRect, Lock lock) final;
		virtual void UnlockRect(UINT level) final;
		virtual void LockBox(UINT level, LockedBox* outLockedVolume, Lock lock) final;
		virtual void UnlockBox(UINT level) final;

		virtual void GetLevelDesc(UINT level, SurfaceDesc* outDesc) final;
		virtual size_t GetMipsCount() final;

		virtual void Fill(void* data, unsigned pitch) final;
		virtual void CopyResource(Texture* texture) final;
		virtual void SaveToFile(LPCWSTR pDestFile, ImageFileFormat DestFormat) final;

		virtual bool UseSRGB() const final { return useSRGB; }
		virtual bool UseMipmaps() const final { return _mip_resource_views.size() > 0; }

		HRESULT LoadTextureFromFile(LPCWSTR pSrcFile, TextureFilter mipFilter, UsageHint usage, Pool pool, bool useSRGB);
		HRESULT LoadTextureFromMemory(const Memory::DebugStringA<>& name, LPCVOID pSrcData, UINT srcDataSize, TextureFilter mipFilter, UsageHint usage, Pool pool, bool useSRGB, bool premultiply_alpha);
		HRESULT CreateStagingTexture(ID3D11Resource* principle, ID3D11Texture2D** staging);

		ID3D11Resource* GetTextureResource() const { return _texture.get(); }
		ID3D11ShaderResourceView* GetShaderResourceView() const { return _resource_view.get(); }
		ID3D11UnorderedAccessView* GetUAV() const { return _unordered_access_view.get(); }
		ID3D11ShaderResourceView* GetShaderMipResourceView(size_t layer) const { assert(layer <_mip_resource_views.size());  return _mip_resource_views[layer].get(); }
		ID3D11Resource* GetStagingResource() const { return _staging.get(); }

		void CopySubResource(UINT level, UINT x, UINT y, UINT z, Texture* pSrcResource, UINT SrcSubResource, const Rect* pSrcRect);

		std::unique_ptr<ID3D11Resource, Utility::GraphicsComReleaser  > _texture;
		std::unique_ptr<ID3D11Resource, Utility::GraphicsComReleaser  > _staging;

		std::unique_ptr<ID3D11ShaderResourceView, Utility::GraphicsComReleaser > _resource_view;
		std::unique_ptr<ID3D11UnorderedAccessView, Utility::GraphicsComReleaser > _unordered_access_view;
		std::vector< std::unique_ptr<ID3D11ShaderResourceView, Utility::GraphicsComReleaser > > _mip_resource_views;
	};

	TextureD3D11::TextureD3D11(const Memory::DebugStringA<>& name, IDevice* device, bool useSRGB)
		: Texture(name)
		, pDevice(device)
		, useSRGB(useSRGB)
	{}

	TextureD3D11::TextureD3D11(const Memory::DebugStringA<>& name, IDevice* device, UINT Width, UINT Height, UINT Levels, UsageHint Usage, PixelFormat Format, Pool pool, bool useSRGB, bool use_ESRAM, bool use_mipmaps, bool enable_gpu_write)
		: TextureD3D11(name, device, useSRGB)
	{
		ID3D11Texture2D* pTexture2D = NULL;
		ID3D11ShaderResourceView* pResourceView;

		// Create Texture
		D3D11_TEXTURE2D_DESC desc;
		ZeroMemory(&desc, sizeof(desc));
		desc.Width = Width;
		desc.Height = Height;
		desc.MipLevels = Levels;
		desc.ArraySize = 1;
		desc.Format = ConvertPixelFormat(Format, useSRGB);
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Usage = (desc.Format == DXGI_FORMAT_R24G8_TYPELESS || Usage == UsageHint::DEPTHSTENCIL || Usage == UsageHint::RENDERTARGET || Usage == UsageHint::DEFAULT) ? D3D11_USAGE_DEFAULT : D3D11_USAGE_DYNAMIC;
		desc.BindFlags = (Usage == UsageHint::RENDERTARGET ? D3D11_BIND_RENDER_TARGET : 0) |
			((desc.Format == DXGI_FORMAT_R24G8_TYPELESS || Usage == UsageHint::DEPTHSTENCIL) ? D3D11_BIND_DEPTH_STENCIL : 0) |
			(enable_gpu_write ? D3D11_BIND_UNORDERED_ACCESS : 0) |
			D3D11_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlags = ((Usage == UsageHint::RENDERTARGET || Usage == UsageHint::DEPTHSTENCIL || Usage == UsageHint::DEFAULT) || desc.Format == DXGI_FORMAT_R24G8_TYPELESS) ? 0 : D3D11_CPU_ACCESS_WRITE;
		desc.MiscFlags = 0;

	#if defined(_XBOX_ONE)
		if (!use_ESRAM || !ESRAM::Get() || !ESRAM::Get()->CreateTexture2D(((ID3D11DeviceX*)static_cast<IDeviceD3D11*>(device)->GetDevice()), desc, &pTexture2D))
	#endif
		{
			const auto hr = static_cast<IDeviceD3D11*>(device)->GetDevice()->CreateTexture2D(&desc, NULL, &pTexture2D);
			if (FAILED(hr))
				throw ExceptionD3D11("CreateTexture2D", hr, static_cast<IDeviceD3D11*>(device)->GetDevice());
		}

		_texture.reset(pTexture2D);

		static_cast<IDeviceD3D11*>(device)->SetDebugName(_texture.get(), name);

		// Create Resource View.
		D3D11_SHADER_RESOURCE_VIEW_DESC resourceviewdesc;
		ZeroMemory(&resourceviewdesc, sizeof(resourceviewdesc));
		resourceviewdesc.Format = (desc.Format == DXGI_FORMAT_R24G8_TYPELESS) ? DXGI_FORMAT_R24_UNORM_X8_TYPELESS : desc.Format; // NOTE: Needs typeless to avoid Debug Layer error.
		resourceviewdesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		resourceviewdesc.Texture2D.MipLevels = use_mipmaps ? -1 : desc.MipLevels;
		resourceviewdesc.Texture2D.MostDetailedMip = 0;

		auto hr = static_cast<IDeviceD3D11*>(device)->GetDevice()->CreateShaderResourceView(pTexture2D, &resourceviewdesc, &pResourceView);
		if (FAILED(hr))
			throw ExceptionD3D11("CreateShaderResourceView", hr, static_cast<IDeviceD3D11*>(device)->GetDevice());
		_resource_view.reset(pResourceView);

		if (enable_gpu_write)
		{
			// Create UAV
			D3D11_UNORDERED_ACCESS_VIEW_DESC uavdesc;
			ZeroMemory(&uavdesc, sizeof(uavdesc));
			uavdesc.Format = (desc.Format == DXGI_FORMAT_R24G8_TYPELESS) ? DXGI_FORMAT_R24_UNORM_X8_TYPELESS : desc.Format;
			uavdesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
			uavdesc.Texture2D.MipSlice = 0;

			ID3D11UnorderedAccessView* pUnorderedView = nullptr;
			hr = static_cast<IDeviceD3D11*>(device)->GetDevice()->CreateUnorderedAccessView(pTexture2D, &uavdesc, &pUnorderedView);
			if (FAILED(hr))
				throw ExceptionD3D11("CreateUnorderedAccessView", hr, static_cast<IDeviceD3D11*>(device)->GetDevice());
			_unordered_access_view.reset(pUnorderedView);
		}

		if (use_mipmaps)
		{
			size_t mips_count = 0;

			ID3D11Texture2D* texture2d = nullptr;
			hr = pTexture2D->QueryInterface(IID_GRAPHICS_PPV_ARGS(&texture2d));
			if (FAILED(hr))
				throw ExceptionD3D11("QueryInterface", hr, static_cast<IDeviceD3D11*>(device)->GetDevice());

			D3D11_TEXTURE2D_DESC desc;
			texture2d->GetDesc(&desc);
			mips_count = desc.MipLevels;

			SAFE_RELEASE(texture2d);

			_mip_resource_views.resize(mips_count);
			for (size_t layer = 0; layer < mips_count; layer++)
			{
				D3D11_SHADER_RESOURCE_VIEW_DESC layer_resource_view = resourceviewdesc;
				layer_resource_view.Texture2D.MipLevels = 1;
				layer_resource_view.Texture2D.MostDetailedMip = (UINT)layer;
				hr = static_cast<IDeviceD3D11*>(device)->GetDevice()->CreateShaderResourceView(pTexture2D, &layer_resource_view, &pResourceView);
				if (FAILED(hr))
					throw ExceptionD3D11("CreateShaderResourceView", hr, static_cast<IDeviceD3D11*>(device)->GetDevice());
				_mip_resource_views[layer].reset(pResourceView);
			}
		}

		// Copy the source to the staging resource if required. Note that a staging resource would only be
		// created when the pool flag is not POOL_DEFAULT
		if (pool != Pool::DEFAULT)
		{
			ID3D11Texture2D* pStaging = nullptr;
			CreateStagingTexture(pTexture2D, &pStaging);
			{
				Memory::ReentrantLock lock(buffer_context_mutex);

				static_cast<IDeviceD3D11*>(device)->GetContext()->CopyResource(pStaging, pTexture2D);
			}
			_staging.reset(pStaging);
		}
	}

	TextureD3D11::TextureD3D11(const Memory::DebugStringA<>& name, IDevice* device, UINT Width, UINT Height, unsigned char* pixels, UINT bytes_per_pixel)
		: TextureD3D11(name, device, false)
	{
		ID3D11Texture2D* pTexture2D = NULL;
		ID3D11ShaderResourceView* pResourceView;

		// Create Texture
		D3D11_TEXTURE2D_DESC desc;
		ZeroMemory(&desc, sizeof(desc));
		desc.Width = Width;
		desc.Height = Height;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlags = 0;

		D3D11_SUBRESOURCE_DATA subResource;
		subResource.pSysMem = pixels;
		subResource.SysMemPitch = desc.Width * 4;
		subResource.SysMemSlicePitch = 0;

		auto hr = static_cast<IDeviceD3D11*>(device)->GetDevice()->CreateTexture2D(&desc, &subResource, &pTexture2D);
		if (FAILED(hr))
			throw ExceptionD3D11("CreateTexture2D", hr, static_cast<IDeviceD3D11*>(device)->GetDevice());

		_texture.reset(pTexture2D);

		static_cast<IDeviceD3D11*>(device)->SetDebugName(_texture.get(), name);

		// Create Resource View.
		D3D11_SHADER_RESOURCE_VIEW_DESC resourceviewdesc;
		ZeroMemory(&resourceviewdesc, sizeof(resourceviewdesc));
		resourceviewdesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		resourceviewdesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		resourceviewdesc.Texture2D.MipLevels = desc.MipLevels;
		resourceviewdesc.Texture2D.MostDetailedMip = 0;

		hr = static_cast<IDeviceD3D11*>(device)->GetDevice()->CreateShaderResourceView(pTexture2D, &resourceviewdesc, &pResourceView);
		if (FAILED(hr))
			throw ExceptionD3D11("CreateShaderResourceView", hr, static_cast<IDeviceD3D11*>(device)->GetDevice());
		_resource_view.reset(pResourceView);
	}

	TextureD3D11::TextureD3D11(const Memory::DebugStringA<>& name, IDevice* device, LPCWSTR pSrcFile, UINT width, UINT height, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool, TextureFilter filter, TextureFilter mipFilter, simd::color colorKey, ImageInfo* pSrcInfo, PaletteEntry* outPalette)
		: TextureD3D11(name, device, true)
	{
		auto hr = LoadTextureFromFile(pSrcFile, mipFilter, usage, pool, useSRGB);
		if (FAILED(hr))
		{
			if (pSrcInfo)
			{
				pSrcInfo->Width = 0;
				pSrcInfo->Height = 0;
				pSrcInfo->Format = PixelFormat::UNKNOWN;
			}
			return;
		}

		static_cast<IDeviceD3D11*>(device)->SetDebugName(_texture.get(), WstringToString(pSrcFile).c_str());

		if (pSrcInfo)
		{
			ID3D11Texture2D* texture2d = nullptr;
			hr = _texture->QueryInterface(IID_GRAPHICS_PPV_ARGS(&texture2d));
			if (FAILED(hr))
				throw ExceptionD3D11("QueryInterface", hr, static_cast<IDeviceD3D11*>(device)->GetDevice());

			D3D11_TEXTURE2D_DESC desc;
			texture2d->GetDesc(&desc);
			pSrcInfo->Width = desc.Width;
			pSrcInfo->Height = desc.Height;
			pSrcInfo->Format = UnconvertPixelFormat(desc.Format);

			SAFE_RELEASE(texture2d);
		}
	}

	TextureD3D11::TextureD3D11(const Memory::DebugStringA<>& name, IDevice* device, LPCVOID pSrcData, UINT srcDataSize, UINT width, UINT height, UINT mipLevels, UsageHint usage,
		PixelFormat format, Pool pool, TextureFilter filter, TextureFilter mipFilter, simd::color colorKey, ImageInfo* pSrcInfo, PaletteEntry* outPalette, bool useSRGB, bool premultiply_alpha)
		: TextureD3D11(name, device, useSRGB)
	{
		auto hr = LoadTextureFromMemory(name, pSrcData, srcDataSize, mipFilter, usage, pool, useSRGB, premultiply_alpha);
		if (FAILED(hr))
		{
			if (pSrcInfo)
			{
				pSrcInfo->Width = 0;
				pSrcInfo->Height = 0;
				pSrcInfo->Format = PixelFormat::UNKNOWN;
			}
			return;
		}

		static_cast<IDeviceD3D11*>(device)->SetDebugName(_texture.get(), name);

		if (pSrcInfo)
		{
			ID3D11Texture2D* texture2d;
			hr = _texture->QueryInterface(IID_GRAPHICS_PPV_ARGS(&texture2d));
			if (FAILED(hr))
				throw ExceptionD3D11("QueryInterface", hr, static_cast<IDeviceD3D11*>(device)->GetDevice());

			D3D11_TEXTURE2D_DESC desc;
			texture2d->GetDesc(&desc);
			pSrcInfo->Width = desc.Width;
			pSrcInfo->Height = desc.Height;
			pSrcInfo->Format = UnconvertPixelFormat(desc.Format);

			SAFE_RELEASE(texture2d);
		}
	}

	TextureD3D11::TextureD3D11(const Memory::DebugStringA<>& name, IDevice* device, UINT edgeLength, UINT levels, UsageHint usage, PixelFormat format, Pool pool, bool useSRGB, bool enable_gpu_write)
		: TextureD3D11(name, device, useSRGB)
	{
		ID3D11Texture2D* pTexture2D;
		ID3D11ShaderResourceView* pResourceView;

		//
		// Create Texture
		//
		D3D11_TEXTURE2D_DESC desc;
		ZeroMemory(&desc, sizeof(desc));
		desc.Height = edgeLength;
		desc.Width = edgeLength;
		desc.MipLevels = levels;
		desc.ArraySize = 6;
		desc.Format = ConvertPixelFormat(format, useSRGB);
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = (usage == UsageHint::RENDERTARGET ? D3D11_BIND_RENDER_TARGET : 0) |
			((desc.Format == DXGI_FORMAT_R24G8_TYPELESS || usage == UsageHint::DEPTHSTENCIL) ? D3D11_BIND_DEPTH_STENCIL : 0) |
			D3D11_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlags = ((usage == UsageHint::RENDERTARGET || usage == UsageHint::DEPTHSTENCIL || usage == UsageHint::DEFAULT) || desc.Format == DXGI_FORMAT_R24G8_TYPELESS) ? 0 : D3D11_CPU_ACCESS_WRITE;
		desc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;
		desc.SampleDesc.Count = 1;

		auto hr = static_cast<IDeviceD3D11*>(device)->GetDevice()->CreateTexture2D(&desc, nullptr, &pTexture2D);
		if (FAILED(hr))
			throw ExceptionD3D11("CreateTexture2D", hr, static_cast<IDeviceD3D11*>(device)->GetDevice());

		_texture.reset(pTexture2D);

		static_cast<IDeviceD3D11*>(device)->SetDebugName(_texture.get(), name);

		//
		// Create Shader View
		//
		D3D11_SHADER_RESOURCE_VIEW_DESC resourceviewdesc;
		ZeroMemory(&resourceviewdesc, sizeof(resourceviewdesc));
		resourceviewdesc.Format = desc.Format;
		resourceviewdesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
		resourceviewdesc.TextureCube.MipLevels = desc.MipLevels;
		resourceviewdesc.TextureCube.MostDetailedMip = 0;

		hr = static_cast<IDeviceD3D11*>(device)->GetDevice()->CreateShaderResourceView(pTexture2D, &resourceviewdesc, &pResourceView);
		if (FAILED(hr))
			throw ExceptionD3D11("CreateShaderResourceView", hr, static_cast<IDeviceD3D11*>(device)->GetDevice());
		_resource_view.reset(pResourceView);
	}

	TextureD3D11::TextureD3D11(const Memory::DebugStringA<>& name, IDevice* device, LPCWSTR pSrcFile)
		: TextureD3D11(name, device, true)
	{
		auto hr = LoadTextureFromFile(pSrcFile, TextureFilter::NONE, UsageHint::DEFAULT, Pool::DEFAULT, useSRGB);
		if (FAILED(hr))
			return;

		static_cast<IDeviceD3D11*>(device)->SetDebugName(_texture.get(), name);
	}

	TextureD3D11::TextureD3D11(const Memory::DebugStringA<>& name, IDevice* device, LPCWSTR pSrcFile, UINT size, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool, TextureFilter filter, TextureFilter mipFilter, simd::color colorKey, ImageInfo* outSrcInfo, PaletteEntry* outPalette)
		: TextureD3D11(name, device, true)
	{
		auto hr = LoadTextureFromFile(pSrcFile, mipFilter, usage, pool, useSRGB);
		if (FAILED(hr))
			return;

		static_cast<IDeviceD3D11*>(device)->SetDebugName(_texture.get(), name);
	}

	TextureD3D11::TextureD3D11(const Memory::DebugStringA<>& name, IDevice* device, LPCVOID pSrcData, UINT srcDataSize, UINT size, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool, TextureFilter filter, TextureFilter mipFilter,
		simd::color colorKey, ImageInfo* pSrcInfo, PaletteEntry* outPalette, bool useSRGB)
		: TextureD3D11(name, device, useSRGB)
	{
		auto hr = LoadTextureFromMemory(name, pSrcData, srcDataSize, mipFilter, usage, pool, useSRGB, false);
		if (FAILED(hr))
		{
			if (pSrcInfo)
			{
				pSrcInfo->Width = 0;
				pSrcInfo->Height = 0;
				pSrcInfo->Format = PixelFormat::UNKNOWN;
			}
			return;
		}

		static_cast<IDeviceD3D11*>(device)->SetDebugName(_texture.get(), name);

		if (pSrcInfo)
		{
			ID3D11Texture2D* texture2d;
			hr = _texture->QueryInterface(IID_GRAPHICS_PPV_ARGS(&texture2d));
			if (FAILED(hr))
				throw ExceptionD3D11("QueryInterface", hr, static_cast<IDeviceD3D11*>(device)->GetDevice());

			D3D11_TEXTURE2D_DESC desc;
			texture2d->GetDesc(&desc);
			pSrcInfo->Width = desc.Width;
			pSrcInfo->Height = desc.Height;
			pSrcInfo->Format = UnconvertPixelFormat(desc.Format);

			SAFE_RELEASE(texture2d);
		}
	}

	TextureD3D11::TextureD3D11(const Memory::DebugStringA<>& name, IDevice* device, LPCVOID pSrcData, UINT srcDataSize, UINT width, UINT height, UINT depth, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool,
		TextureFilter filter, TextureFilter mipFilter, simd::color colorKey, ImageInfo* pSrcInfo, PaletteEntry* outPalette, bool useSRGB)
		: TextureD3D11(name, device, useSRGB)
	{
		auto hr = LoadTextureFromMemory(name, pSrcData, srcDataSize, mipFilter, usage, pool, useSRGB, false);
		if (FAILED(hr))
		{
			if (pSrcInfo)
			{
				pSrcInfo->Width = 0;
				pSrcInfo->Height = 0;
				pSrcInfo->Format = PixelFormat::UNKNOWN;
			}
			return;
		}

		static_cast<IDeviceD3D11*>(device)->SetDebugName(_texture.get(), name);

		if (pSrcInfo)
		{
			ID3D11Texture3D* texture3d;
			hr = _texture->QueryInterface(IID_GRAPHICS_PPV_ARGS(&texture3d));
			if (FAILED(hr))
				throw ExceptionD3D11("QueryInterface", hr, static_cast<IDeviceD3D11*>(device)->GetDevice());

			D3D11_TEXTURE3D_DESC desc;
			texture3d->GetDesc(&desc);
			pSrcInfo->Width = desc.Width;
			pSrcInfo->Height = desc.Height;
			pSrcInfo->Depth = desc.Depth;
			pSrcInfo->Format = UnconvertPixelFormat(desc.Format);

			SAFE_RELEASE(texture3d);
		}
	}

	TextureD3D11::TextureD3D11(const Memory::DebugStringA<>& name, IDevice* device, UINT width, UINT height, UINT depth, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool, bool useSRGB, bool enable_gpu_write)
		: TextureD3D11(name, device, useSRGB)
	{
		// Note: this will only create a cpu access only volume texture

		D3D11_TEXTURE3D_DESC desc;
		desc.Height = height;
		desc.Width = width;
		desc.Depth = depth;
		desc.MipLevels = mipLevels;
		desc.Format = ConvertPixelFormat(format, useSRGB);
		desc.Usage = D3D11_USAGE_STAGING;
		desc.BindFlags = 0;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
		desc.MiscFlags = 0;

		ID3D11Texture3D* pTexture3D;
		auto hr = static_cast<IDeviceD3D11*>(device)->GetDevice()->CreateTexture3D(&desc, nullptr, &pTexture3D);
		if (FAILED(hr))
			throw ExceptionD3D11("CreateTexture3D", hr, static_cast<IDeviceD3D11*>(device)->GetDevice());

		_texture.reset(pTexture3D);

		static_cast<IDeviceD3D11*>(device)->SetDebugName(_texture.get(), name);
	}

	TextureD3D11::TextureD3D11(const Memory::DebugStringA<>& name, IDevice* device, LPCWSTR pSrcFile, UINT width, UINT height, UINT depth, UINT mipLevels, UsageHint usage, PixelFormat format, Pool pool, TextureFilter filter, TextureFilter mipFilter, simd::color colorKey, ImageInfo* outSrcInfo, PaletteEntry* outPalette)
		: TextureD3D11(name, device, true)
	{
		auto hr = LoadTextureFromFile(pSrcFile, mipFilter, usage, pool, useSRGB);
		if (FAILED(hr))
			return;

		static_cast<IDeviceD3D11*>(device)->SetDebugName(_texture.get(), name);
	}

	void TextureD3D11::SkipMips(const DirectX::Image*& images, size_t& image_count, DirectX::TexMetadata& metadata, TextureFilter mipFilter)
	{
		unsigned w = unsigned(metadata.width);
		unsigned h = unsigned(metadata.height);
		unsigned start = 0;
		unsigned count = unsigned(image_count);

		Device::SkipMips(w, h, start, count, mipFilter, UnconvertPixelFormat(metadata.format));

		image_count = size_t(count);
		images += start;
		metadata.width = w;
		metadata.height = h;
		metadata.mipLevels -= start;
	}

	HRESULT TextureD3D11::LoadTextureFromFile(LPCWSTR pSrcFile, TextureFilter mipFilter, UsageHint usageFlags, Pool pool, bool useSRGB)
	{
		File::InputFileStream stream(pSrcFile);

		auto hr = LoadTextureFromMemory(WstringToString(pSrcFile), stream.GetFilePointer(), (unsigned)stream.GetFileSize(), mipFilter, usageFlags, pool, useSRGB, false);

		return hr;
	}

	void LoadNonDDS(const Memory::DebugStringA<>& name, LPCVOID pSrcData, UINT srcDataSize, DirectX::ScratchImage& out_scratch)
	{
		uint32_t magic = *(const uint32_t*)pSrcData;

		try
		{
			if (magic == PNG::MAGIC)
			{
				PNG image(name, (uint8_t*)pSrcData, srcDataSize);
				out_scratch.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, image.GetWidth(), image.GetHeight(), 1, 1, 0);
				assert(image.GetImageSize() == out_scratch.GetPixelsSize());
				memcpy_s(out_scratch.GetPixels(), out_scratch.GetPixelsSize(), image.GetImageData(), image.GetImageSize());
			}
			else if ((magic & JPG::MAGIC_MASK) == JPG::MAGIC)
			{
				JPG image(name, (uint8_t*)pSrcData, srcDataSize);
				out_scratch.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, image.GetWidth(), image.GetHeight(), 1, 1, 0);
				assert(image.GetImageSize() == out_scratch.GetPixelsSize());
				memcpy_s(out_scratch.GetPixels(), out_scratch.GetPixelsSize(), image.GetImageData(), image.GetImageSize());
			}
		#if defined(DEVELOPMENT_CONFIGURATION) && defined(MOBILE_REALM)
			else if (magic == KTX::MAGIC)
			{
				KTX ktx((uint8_t*)pSrcData, srcDataSize);
				assert(ktx.GetMipCount() >= 1);
				if (ktx.GetDimension() != Device::Dimension::Two)
					throw ExceptionD3D11(std::string("Unsupported texture dimensions for KTX ") + name.c_str());

				out_scratch.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, ktx.GetWidth(), ktx.GetHeight(), ktx.GetArrayCount(), ktx.GetMipCount(), 0);

				ASTC::Context context(ASTC::BlockModeFromPixelFormat(ktx.GetPixelFormat()), ASTC::Profile::LDR);
				for (unsigned int array_index = 0; array_index < ktx.GetArrayCount(); array_index++)
				{
					for (unsigned int mip_index = 0; mip_index < ktx.GetMipCount(); mip_index++)
					{
						const auto src_index = (array_index * ktx.GetMipCount()) + mip_index;
						const auto& src_mip = ktx.GetMips()[src_index];

						auto dst_mip = out_scratch.GetImage(mip_index, array_index, 0);

						ASTC::Image image(context, src_mip.width, src_mip.height, Device::PixelFormat::A8R8G8B8);
						ASTC::DecompressImage(context, src_mip.data, src_mip.size, image);
						for (unsigned int y = 0; y < image.GetHeight(); y++)
						{
							for (unsigned int x = 0; x < image.GetWidth(); x++)
							{
								uint8_t* dst = dst_mip->pixels + (((y * dst_mip->width) + x) * 4);
								const auto src = image.GetPixel(x, y);
								for (unsigned int c = 0; c < 4; c++)
									dst[c] = (uint8_t)std::max(0.0f, std::min(((src[c] * 255.0f) + 0.5f), 255.5f));
							}
						}
					}
				}
			}
		#endif
			else
			{
				throw std::runtime_error("Unrecognized texture format");
			}
		}
		catch (std::exception& e)
		{
			const uint32_t black = 0;
			out_scratch.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, 1, 1, 0);
			memcpy_s(out_scratch.GetPixels(), out_scratch.GetPixelsSize(), &black, sizeof(black));
			LOG_WARN(L"Failed to load texture " << StringToWstring(name.c_str()) << L"(Error: " << StringToWstring(e.what()) << L")");
		}
	}

	HRESULT TextureD3D11::LoadTextureFromMemory(const Memory::DebugStringA<>& name, LPCVOID pSrcData, UINT srcDataSize, TextureFilter mipFilter, UsageHint usageFlags, Pool pool, bool useSRGB, bool premultiply_alpha)
	{
		// Generate Resource Description.
		ResourceDesc resourceDesc;
		CreateResourceDesc(0, usageFlags, pool, &resourceDesc);

		// Create the target resource.
		DirectX::ScratchImage out_scratch;

		auto hr = DirectX::LoadFromDDSMemory(pSrcData, srcDataSize, DirectX::DDS_FLAGS_NONE, nullptr, out_scratch);
		if (FAILED(hr))
		{
			LoadNonDDS(name, pSrcData, srcDataSize, out_scratch);
		}

		if (premultiply_alpha &&
			!DirectX::IsCompressed(out_scratch.GetMetadata().format) &&
			DirectX::HasAlpha(out_scratch.GetMetadata().format))
		{
			DirectX::ScratchImage tmp_scratch;

			hr = PremultiplyAlpha(out_scratch.GetImages(), out_scratch.GetImageCount(), out_scratch.GetMetadata(), DirectX::TEX_PMALPHA_DEFAULT, tmp_scratch);
			if (FAILED(hr))
				throw ExceptionD3D11("PremultiplyAlpha", hr, static_cast<IDeviceD3D11*>(pDevice)->GetDevice());

			out_scratch = std::move(tmp_scratch);
		}

		// Handle mips skip.
		const auto* images = out_scratch.GetImages();
		auto image_count = out_scratch.GetImageCount();
		auto metadata = out_scratch.GetMetadata();
		SkipMips(images, image_count, metadata, mipFilter);

		// Create API objects.
		ID3D11ShaderResourceView* pResourceView = nullptr;
		hr = DirectX::CreateShaderResourceViewEx(static_cast<IDeviceD3D11*>(pDevice)->GetDevice(), images, image_count, metadata,
			resourceDesc.usage, resourceDesc.bindFlags | D3D11_BIND_SHADER_RESOURCE, resourceDesc.cpuAccessFlags, resourceDesc.miscFlags, useSRGB,
			&pResourceView);
		if (FAILED(hr))
			throw ExceptionD3D11("CreateShaderResourceViewEx", hr, static_cast<IDeviceD3D11*>(pDevice)->GetDevice());
		_resource_view.reset(pResourceView);

		ID3D11Resource* pResource = nullptr;
		pResourceView->GetResource(&pResource);

		_texture.reset(pResource);
		useSRGB = useSRGB;

		// Create the staging resource if required, with the same data.
		if (resourceDesc.copyMemory || resourceDesc.createStaging)
		{
			ID3D11Resource* pStaging = nullptr;

			hr = DirectX::CreateTextureEx(static_cast<IDeviceD3D11*>(pDevice)->GetDevice(), images, image_count, metadata,
				D3D11_USAGE_STAGING, 0, D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE, 0, useSRGB,
				&pStaging);
			if (FAILED(hr))
				throw ExceptionD3D11("CreateTextureEx", hr, static_cast<IDeviceD3D11*>(pDevice)->GetDevice());

			_staging.reset(pStaging);
		}

		out_scratch.Release();

		return hr;
	}

	HRESULT TextureD3D11::CreateStagingTexture(ID3D11Resource* pResource, ID3D11Texture2D** ppStaging)
	{
		ID3D11Texture2D* texture2d = nullptr;
		auto hr = pResource->QueryInterface(IID_GRAPHICS_PPV_ARGS(&texture2d));
		if (FAILED(hr))
			throw ExceptionD3D11("QueryInterface", hr, static_cast<IDeviceD3D11*>(pDevice)->GetDevice());

		D3D11_TEXTURE2D_DESC desc;
		texture2d->GetDesc(&desc);

		SAFE_RELEASE(texture2d);

		// The requirements for CopyResource are pretty strict.  Everything has to be indentical to the resource from
		// which we are copying.  Flag this resouce as USAGE_STAGING, remove BindFlags (since staging resources cannot
		// be bound) and grant read and write access.  Read access is required, else why are we doing this, write access
		// because we can not guarantee that the user is not going to want to write back to teh principle resource.
		desc.Usage = D3D11_USAGE_STAGING;
		desc.BindFlags = 0;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
		desc.MiscFlags = 0;

		hr = static_cast<IDeviceD3D11*>(pDevice)->GetDevice()->CreateTexture2D(&desc, nullptr, ppStaging);
		return hr;
	}

	void TextureD3D11::CopySubResource(UINT level, UINT x, UINT y, UINT z, Texture* pSrcResource, UINT SrcSubResource, const Rect* pSrcRect)
	{
		PROFILE;

		Memory::ReentrantLock lock(buffer_context_mutex);

		DirectX::ScratchImage in_scratch;
		auto hr = DirectX::CaptureTexture(static_cast<IDeviceD3D11*>(pDevice)->GetDevice(), static_cast<IDeviceD3D11*>(pDevice)->GetContext(), static_cast<TextureD3D11*>(pSrcResource)->_texture.get(), in_scratch);
		if (FAILED(hr))
			throw ExceptionD3D11("CaptureTexture", hr, static_cast<IDeviceD3D11*>(pDevice)->GetDevice());
		const DirectX::Image* in = &(in_scratch.GetImages())[0];
		if (DirectX::IsCompressed(in->format))
		{
			DirectX::ScratchImage tmp_scratch;
			hr = DirectX::Decompress(*in, DXGI_FORMAT_UNKNOWN, tmp_scratch);
			if (FAILED(hr))
				throw ExceptionD3D11("Decompress", hr, static_cast<IDeviceD3D11*>(pDevice)->GetDevice());
			in = &(tmp_scratch.GetImages())[0];
			assert(!DirectX::IsCompressed(in->format));
			in_scratch = std::move(tmp_scratch);
		}

		DirectX::ScratchImage out_scratch;
		hr = DirectX::CaptureTexture(static_cast<IDeviceD3D11*>(pDevice)->GetDevice(), static_cast<IDeviceD3D11*>(pDevice)->GetContext(), _texture.get(), out_scratch);
		if (FAILED(hr))
			throw ExceptionD3D11("CaptureTexture", hr, static_cast<IDeviceD3D11*>(pDevice)->GetDevice());
		const DirectX::Image* out = &(out_scratch.GetImages())[0];
		DXGI_FORMAT final_format = out->format;
		if (DirectX::IsCompressed(out->format))
		{
			DirectX::ScratchImage tmp_scratch;
			hr = DirectX::Decompress(*out, DXGI_FORMAT_UNKNOWN, tmp_scratch);
			if (FAILED(hr))
				throw ExceptionD3D11("Decompress", hr, static_cast<IDeviceD3D11*>(pDevice)->GetDevice());
			out = &(tmp_scratch.GetImages())[0];
			assert(!DirectX::IsCompressed(out->format));
			out_scratch = std::move(tmp_scratch);
		}

		DirectX::Rect rect;
		if (pSrcRect)
			rect = DirectX::Rect(pSrcRect->left, pSrcRect->top, pSrcRect->right - pSrcRect->left, pSrcRect->bottom - pSrcRect->top);
		else
			rect = DirectX::Rect(0, 0, in->width, in->height);
		hr = DirectX::CopyRectangle(*in, rect, *out, DirectX::TEX_FILTER_DEFAULT, 0, 0);
		if (FAILED(hr))
			throw ExceptionD3D11("CopyRectangle", hr, static_cast<IDeviceD3D11*>(pDevice)->GetDevice());
		in_scratch.Release();

		if (DirectX::IsCompressed(final_format))
		{
			DirectX::ScratchImage tmp_scratch;
			const DWORD compress_flags = DirectX::TEX_COMPRESS_SRGB | DirectX::TEX_COMPRESS_UNIFORM;
			hr = DirectX::Compress(out_scratch.GetImages(), out_scratch.GetImageCount(), out_scratch.GetMetadata(), final_format, compress_flags, DirectX::TEX_THRESHOLD_DEFAULT, tmp_scratch);
			if (FAILED(hr))
				throw ExceptionD3D11("Compress", hr, static_cast<IDeviceD3D11*>(pDevice)->GetDevice());
			out = &(tmp_scratch.GetImages())[0];
			out_scratch = std::move(tmp_scratch);
		}

		ID3D11ShaderResourceView* resource_view = nullptr;
		hr = DirectX::CreateShaderResourceView(static_cast<IDeviceD3D11*>(pDevice)->GetDevice(), out, 1, out_scratch.GetMetadata(), &resource_view);
		if (FAILED(hr))
			throw ExceptionD3D11("CreateShaderResourceView", hr, static_cast<IDeviceD3D11*>(pDevice)->GetDevice());
		_resource_view.reset(resource_view);

		ID3D11Resource* resource = nullptr;
		resource_view->GetResource(&resource);

		_texture.reset(resource);
		out_scratch.Release();
	}

	void TextureD3D11::LockRect(UINT level, LockedRect* outLockedRect, Lock lock)
	{
		PROFILE;

		D3D11_MAP mapFlags;
		if (_staging)
		{
			staging_modified = true;
		}

		switch (lock)
		{
		case Lock::DEFAULT:
		{
			// Determining the default behaviour for locking is a little difficult. I've decided to go with this;
			// If we are going to attempt to lock a staging resource then grant read and write access, if we are not
			// a staging resource then we ASSUME that the resource that we are attempting to lock is a DYNAMIC resource
			// in which case we must either WRITE_DISCARD, WRITE_NO_OVERWRITE or some combination of the too
			mapFlags = _staging ? D3D11_MAP_READ_WRITE : D3D11_MAP_WRITE_DISCARD;
			break;
		}
		case Lock::DISCARD:
		{
			assert(!_staging);
			mapFlags = D3D11_MAP_WRITE_DISCARD;
			break;
		}
		case Lock::NOOVERWRITE:
		{
			assert(!_staging);
			mapFlags = D3D11_MAP_WRITE_NO_OVERWRITE;
			break;
		}
		case Lock::READONLY:
		{
			staging_modified = false;
			mapFlags = D3D11_MAP_READ;
			break;
		}
		default: throw std::runtime_error("Unknown");
		}

		ID3D11Resource* resourceToMap = nullptr;

		// Check to make sure that the staging &/or _scratch buffers are synced with the primary buffer unless no_overwrite
		// was specified.
		bool no_overwrite = (mapFlags & D3D11_MAP_WRITE_NO_OVERWRITE) > 0;
		//if( !no_overwrite )
		//	_touch();

		if (_staging)
		{
			resourceToMap = _staging.get();
		}
		else /*if ( !_scratchBuffer )*/
		{
			resourceToMap = _texture.get();
		}

		ZeroMemory(outLockedRect, sizeof(LockedRect));

		Memory::ReentrantLock _lock(buffer_context_mutex);

		D3D11_MAPPED_SUBRESOURCE mapped;
		const auto hr = static_cast<IDeviceD3D11*>(pDevice)->GetContext()->Map(resourceToMap, level, mapFlags, 0, &mapped);
		if (FAILED(hr))
			throw ExceptionD3D11("Map", hr, static_cast<IDeviceD3D11*>(pDevice)->GetDevice());

		outLockedRect->Pitch = mapped.RowPitch;
		outLockedRect->pBits = mapped.pData;
	}

	void TextureD3D11::UnlockRect(UINT level)
	{
		PROFILE;

		ID3D11Resource* resourceToUnmap = nullptr;

		if (_staging)
		{
			resourceToUnmap = _staging.get();
		}
		else /*if ( !_scratchBuffer )*/
		{
			resourceToUnmap = _texture.get();
		}

		if (resourceToUnmap)
		{
			Memory::ReentrantLock lock(buffer_context_mutex);

			static_cast<IDeviceD3D11*>(pDevice)->GetContext()->Unmap(resourceToUnmap, level);
			if (_staging && staging_modified)
			{
				//if we have modified the staging texture on the CPU, we need to modify
				//the original texture on the GPU to be the same as the staging texture
				ID3D11Texture2D* texture2d = nullptr;
				const auto hr = _texture->QueryInterface(IID_GRAPHICS_PPV_ARGS(&texture2d));
				if (FAILED(hr))
					throw ExceptionD3D11("QueryInterface", hr, static_cast<IDeviceD3D11*>(pDevice)->GetDevice());

				D3D11_TEXTURE2D_DESC desc;
				texture2d->GetDesc(&desc);
				static_cast<IDeviceD3D11*>(pDevice)->GetContext()->CopyResource(_texture.get(), _staging.get());
				assert(_resource_view);
				SAFE_RELEASE(texture2d);
				staging_modified = false;
			}
		}
	}

	void TextureD3D11::LockBox(UINT level, LockedBox* outLockedVolume, Lock lock)
	{
		D3D11_MAP mapFlags = D3D11_MAP_READ_WRITE;

		ID3D11Resource* resourceToMap = _texture.get();

		ZeroMemory(outLockedVolume, sizeof(LockedBox));

		Memory::ReentrantLock _lock(buffer_context_mutex);
		D3D11_MAPPED_SUBRESOURCE mapped;
		const auto hr = static_cast<IDeviceD3D11*>(pDevice)->GetContext()->Map(resourceToMap, level, mapFlags, 0, &mapped);
		if (FAILED(hr))
			throw ExceptionD3D11("Map", hr, static_cast<IDeviceD3D11*>(pDevice)->GetDevice());

		outLockedVolume->RowPitch = mapped.RowPitch;;
		outLockedVolume->SlicePitch = mapped.DepthPitch;
		outLockedVolume->pBits = mapped.pData;
	}

	void TextureD3D11::UnlockBox(UINT level)
	{
		ID3D11Resource* resourceToUnmap = _texture.get();
		if (resourceToUnmap)
		{
			Memory::ReentrantLock lock(buffer_context_mutex);
			static_cast<IDeviceD3D11*>(pDevice)->GetContext()->Unmap(resourceToUnmap, level);
		}
	}

	void TextureD3D11::GetLevelDesc(UINT level, SurfaceDesc* outDesc)
	{
		if (_texture)
		{
			ID3D11Texture2D* texture2d = nullptr;
			auto hr = _texture->QueryInterface(IID_GRAPHICS_PPV_ARGS(&texture2d));
			if (SUCCEEDED(hr))
			{
				D3D11_TEXTURE2D_DESC desc;
				texture2d->GetDesc(&desc);
				outDesc->Width = std::max(UINT(1), desc.Width / (1 << level));
				outDesc->Height = std::max(UINT(1), desc.Height / (1 << level));
				outDesc->Format = UnconvertPixelFormat(desc.Format);
				SAFE_RELEASE(texture2d);
				return;
			}

			ID3D11Texture3D* texture3d = nullptr;
			hr = _texture->QueryInterface(IID_GRAPHICS_PPV_ARGS(&texture3d));
			if (SUCCEEDED(hr))
			{
				D3D11_TEXTURE3D_DESC desc;
				texture3d->GetDesc(&desc);
				outDesc->Width = std::max(UINT(1), desc.Width / (1 << level));
				outDesc->Height = std::max(UINT(1), desc.Height / (1 << level));
				outDesc->Format = UnconvertPixelFormat(desc.Format);
				SAFE_RELEASE(texture3d);
				return;
			}

			ID3D11Texture1D* texture1d = nullptr;
			hr = _texture->QueryInterface(IID_GRAPHICS_PPV_ARGS(&texture1d));
			if (SUCCEEDED(hr))
			{
				D3D11_TEXTURE1D_DESC desc;
				texture1d->GetDesc(&desc);
				outDesc->Width = std::max(UINT(1), desc.Width / (1 << level));
				outDesc->Height = 1;
				outDesc->Format = UnconvertPixelFormat(desc.Format);
				SAFE_RELEASE(texture1d);
				return;
			}

			throw ExceptionD3D11("QueryInterface", hr, static_cast<IDeviceD3D11*>(pDevice)->GetDevice());
		}
		else
		{
			outDesc->Width = 0;
			outDesc->Height = 0;
			outDesc->Format = PixelFormat::UNKNOWN;
		}
	}

	size_t TextureD3D11::GetMipsCount()
	{
		return _mip_resource_views.size();
	}

	void TextureD3D11::Fill(void* data, unsigned pitch)
	{
		Memory::ReentrantLock lock(Device::buffer_context_mutex);

		static_cast<IDeviceD3D11*>(pDevice)->GetContext()->UpdateSubresource(GetTextureResource(), 0, 0, data, pitch, 0);
	}

	void TextureD3D11::CopyResource(Texture* pSrcResource)
	{
		CopySubResource(0, 0, 0, 0, pSrcResource, 0, nullptr);
	}

	void TextureD3D11::SaveToFile(LPCWSTR pDestFile, ImageFileFormat DestFormat)
	{
		if (_texture)
		{
			Memory::ReentrantLock lock(buffer_context_mutex);
			DirectX::ScratchImage image;
			auto hr = DirectX::CaptureTexture(static_cast<IDeviceD3D11*>(pDevice)->GetDevice(), static_cast<IDeviceD3D11*>(pDevice)->GetContext(), _texture.get(), image);
			if (FAILED(hr))
				throw ExceptionD3D11("CaptureTexture", hr, static_cast<IDeviceD3D11*>(pDevice)->GetDevice());

			hr = DirectX::SaveToDDSFile(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::DDS_FLAGS_NONE, pDestFile);
			if (FAILED(hr))
				throw ExceptionD3D11("SaveToDDSFile", hr, static_cast<IDeviceD3D11*>(pDevice)->GetDevice());
		}
	}

}
