
namespace Device
{
	extern Memory::ReentrantMutex buffer_context_mutex;

	extern DWORD ConvertLock(Lock lock);

	class RenderTargetD3D11 : public RenderTarget
	{
	public:
		RenderTargetD3D11(const Memory::DebugStringA<>& name, IDevice* device, SwapChain* swap_chain);
		RenderTargetD3D11(const Memory::DebugStringA<>& name, IDevice* device, UINT width, UINT height, PixelFormat format_type, Pool pool, bool use_srgb, bool use_ESRAM, bool use_mipmaps);
		RenderTargetD3D11(const Memory::DebugStringA<>& name, IDevice* device, Device::Handle<Texture> texture, int level, int array_slice);
		virtual ~RenderTargetD3D11();

		virtual Device::Handle<Texture> GetTexture() final { return texture; }
		virtual size_t GetTextureMipLevel() const final { return texture_mip_level; }
		virtual bool IsDepthTexture() const final { return is_depth_texture; }
		virtual UINT GetWidth() const final { return texture_width; }
		virtual UINT GetHeight() const final { return texture_height; }
		virtual simd::vector2_int GetSize() const final { return simd::vector2_int(GetWidth(), GetHeight()); }
		virtual PixelFormat GetFormat() const final { return format; }
		virtual bool UseSRGB() const final { return is_using_srgb; }

		virtual void CopyTo(RenderTarget* dest, bool applyTexFilter) final;
		virtual void CopyToSysMem(RenderTarget* dest) final;

		virtual void LockRect(LockedRect* pLockedRect, Lock lock) final;
		virtual void UnlockRect() final;
		virtual void SaveToFile(LPCWSTR pDestFile, ImageFileFormat DestFormat, const Rect* pSrcRect, bool want_alpha = false) final;
		virtual void GetDesc(SurfaceDesc* pDesc, int level = 0) final;

		std::unique_ptr<ID3D11Resource, Utility::GraphicsComReleaser> GetResource();

		IDevice* device;
		Device::Handle<Texture > texture;
		SwapChainD3D11* swap_chain;
		std::unique_ptr<ID3D11RenderTargetView, Utility::GraphicsComReleaser> render_target_view;
		std::unique_ptr<ID3D11DepthStencilView, Utility::GraphicsComReleaser> depth_stencil_view;
		PixelFormat format;
		UINT texture_width;
		UINT texture_height;
		UINT texture_mip_level;
		bool is_depth_texture;
		bool is_using_srgb;
		bool is_using_mipmaps;
	};

	RenderTargetD3D11::RenderTargetD3D11(const Memory::DebugStringA<>& name, IDevice* _device, SwapChain* _swap_chain)
		: RenderTarget(name)
		, device( _device )
		, render_target_view( nullptr )
		, depth_stencil_view( nullptr )
		, format( PixelFormat::A8B8G8R8 )
		, texture_width( 0 )
		, texture_height( 0 )
		, texture_mip_level( 0 )
		, is_depth_texture( false )
		, is_using_srgb(false)
		, is_using_mipmaps(false)
		, swap_chain((SwapChainD3D11*)_swap_chain)
	{
		HRESULT hr = S_FALSE;

		ID3D11Device* native_device = static_cast<IDeviceD3D11*>(_device)->GetDevice();

		if( swap_chain )
	    {
			// Get swapchain's backbuffer and its description
			ID3D11Texture2D* pBackBuffer;
			hr = swap_chain->GetSwapChain()->GetBuffer(0, IID_GRAPHICS_PPV_ARGS(&pBackBuffer));

			D3D11_TEXTURE2D_DESC texturedesc;
			pBackBuffer->GetDesc(&texturedesc);

			D3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc;
			ZeroMemory(&renderTargetViewDesc, sizeof(renderTargetViewDesc));
			renderTargetViewDesc.Format = texturedesc.Format;
			renderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
			renderTargetViewDesc.Texture2D.MipSlice = 0;

			// Create the render target view.
			ID3D11RenderTargetView* render_target_view_ptr = nullptr;
			native_device->CreateRenderTargetView(pBackBuffer, NULL, &render_target_view_ptr);
			render_target_view.reset(render_target_view_ptr);

			// Save the dimension and format
			texture_width = texturedesc.Width;
			texture_height = texturedesc.Height;
			format = UnconvertPixelFormat(texturedesc.Format);

			pBackBuffer->Release();
	    }
	}

	RenderTargetD3D11::RenderTargetD3D11(const Memory::DebugStringA<>& name, IDevice* _device, UINT width, UINT height, PixelFormat format_type, Pool pool, bool use_srgb, bool use_ESRAM, bool use_mipmaps)
		: RenderTarget(name)
		, device( _device )
		, render_target_view( nullptr )
		, depth_stencil_view( nullptr )
		, format( format_type )
		, texture_width( width )
		, texture_height( height )
		, texture_mip_level( 0 )
		, is_depth_texture( false )
		, is_using_srgb(use_srgb)
		, is_using_mipmaps(use_mipmaps)
		, swap_chain(nullptr)
	{
		HRESULT hr = S_FALSE;

		ID3D11Device* native_device = static_cast<IDeviceD3D11*>(_device)->GetDevice();

		//TODO: need to check if this is a complete enough check, can you e.g. have D16F/D24S8/etc. with USAGE_RENDERTARGET ?
		if (IsDepthFormat(format_type))
		{
			is_depth_texture = true;

			// Create the texture for the depth buffer using the filled out description.
			texture = Texture::CreateTexture("RenderTarget (Depth)", device, width, height, 1, UsageHint::DEPTHSTENCIL, PixelFormat::D24X8, Pool::DEFAULT, use_srgb, use_ESRAM, false);

			// Set up the depth stencil view description.
			D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc;
			ZeroMemory(&depthStencilViewDesc, sizeof(depthStencilViewDesc));
			depthStencilViewDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
			depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
			depthStencilViewDesc.Texture2D.MipSlice = 0;

			// Create the depth stencil view.
			ID3D11DepthStencilView* depth_stencil_view_ptr = nullptr;
			HRESULT hr = native_device->CreateDepthStencilView(static_cast<TextureD3D11*>(texture.Get())->GetTextureResource(), &depthStencilViewDesc, &depth_stencil_view_ptr);
			depth_stencil_view.reset(depth_stencil_view_ptr);
		}
		else
		{
			// Create the render target texture.	
			texture = Texture::CreateTexture("RenderTarget (Color)", device, texture_width, texture_height, use_mipmaps ? 0 : 1, UsageHint::RENDERTARGET, format_type, pool, use_srgb, use_ESRAM, use_mipmaps);

			auto native_texture = static_cast<TextureD3D11*>(texture.Get())->GetTextureResource();
			DXGI_FORMAT converted_format = ConvertPixelFormat(format_type, use_srgb);

			// Setup the description of the render target view.
			D3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc;
			ZeroMemory(&renderTargetViewDesc, sizeof(renderTargetViewDesc));
			renderTargetViewDesc.Format = converted_format;
			renderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
			renderTargetViewDesc.Texture2D.MipSlice = 0;

			// Create the render target view.
			ID3D11RenderTargetView* render_target_view_ptr = nullptr;
			hr = native_device->CreateRenderTargetView(native_texture, &renderTargetViewDesc, &render_target_view_ptr);
			render_target_view.reset(render_target_view_ptr);
		}
	}

	RenderTargetD3D11::RenderTargetD3D11(const Memory::DebugStringA<>& name, IDevice* _device, Device::Handle<Texture> _texture, int level, int array_slice)
		: RenderTarget(name)
		, device( _device )
		, texture( _texture )
		, render_target_view( nullptr )
		, depth_stencil_view( nullptr )
		, format(PixelFormat::UNKNOWN )
		, texture_width( 0 )
		, texture_height( 0 )
		, texture_mip_level(0)
		, is_depth_texture( false )
		, is_using_mipmaps( false )
		, swap_chain(nullptr)
	{
		is_using_srgb = _texture->UseSRGB();
		texture_mip_level = UINT(level);

		SurfaceDesc desc;
		texture->GetLevelDesc(level, &desc);
		texture_width = desc.Width;
		texture_height = desc.Height;

		format = desc.Format;
		is_depth_texture = IsDepthFormat(format);
		ID3D11Device* native_device = static_cast<IDeviceD3D11*>(device)->GetDevice();
		auto native_texture = static_cast<TextureD3D11*>(texture.Get())->GetTextureResource();
		DXGI_FORMAT converted_format = DXGI_FORMAT_UNKNOWN;// device->ConvertPixelFormat( format );

		bool support_msaa = native_device->GetFeatureLevel() > D3D_FEATURE_LEVEL_10_0;

		if (is_depth_texture)
		{
			// Set up the depth stencil view description.
			D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc;
			ZeroMemory(&depthStencilViewDesc, sizeof(depthStencilViewDesc));
			depthStencilViewDesc.Format = format == PixelFormat::D16 ? DXGI_FORMAT_D16_UNORM : DXGI_FORMAT_D24_UNORM_S8_UINT;
			depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
			depthStencilViewDesc.Texture2DArray.ArraySize = 1;
			depthStencilViewDesc.Texture2DArray.FirstArraySlice = array_slice;
			depthStencilViewDesc.Texture2D.MipSlice = level;

			// Create the depth stencil view.
			ID3D11DepthStencilView* depth_stencil_view_ptr = nullptr;
			const auto hr = native_device->CreateDepthStencilView(native_texture, &depthStencilViewDesc, &depth_stencil_view_ptr);
			if (FAILED(hr))
				throw ExceptionD3D11("CreateDepthStencilView", hr, static_cast<IDeviceD3D11*>(device)->GetDevice());
			depth_stencil_view.reset(depth_stencil_view_ptr);
		}
		else
		{
			// Setup the description of the render target view.
			D3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc;
			ZeroMemory(&renderTargetViewDesc, sizeof(renderTargetViewDesc));
			renderTargetViewDesc.Format = converted_format;			
			renderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
			renderTargetViewDesc.Texture2DArray.ArraySize = 1;
			renderTargetViewDesc.Texture2DArray.FirstArraySlice = array_slice;
			renderTargetViewDesc.Texture2D.MipSlice = level;

			// Create the render target view.
			ID3D11RenderTargetView* render_target_view_ptr = nullptr;
			const auto hr = native_device->CreateRenderTargetView(native_texture, &renderTargetViewDesc, &render_target_view_ptr);
			if (FAILED(hr))
				throw ExceptionD3D11("CreateRenderTargetView", hr, static_cast<IDeviceD3D11*>(device)->GetDevice());
			render_target_view.reset(render_target_view_ptr);
		}
	}

	RenderTargetD3D11::~RenderTargetD3D11()
	{
		depth_stencil_view.reset();
		render_target_view.reset();
		texture.Reset();
	}

	std::unique_ptr<ID3D11Resource, Utility::GraphicsComReleaser> RenderTargetD3D11::GetResource()
	{
		ID3D11Resource* resource = nullptr;
		render_target_view->GetResource(&resource);
		return std::unique_ptr<ID3D11Resource, Utility::GraphicsComReleaser>(resource);
	}

	void RenderTargetD3D11::CopyTo( RenderTarget* dest, bool applyTexFilter )
	{
		Memory::ReentrantLock lock(buffer_context_mutex);

		auto* context = static_cast<IDeviceD3D11*>(device)->GetContext();
		ID3D11Resource* texture = nullptr;
		bool get_back_buffer = false;
		if (swap_chain)
		{
			const auto hr = swap_chain->GetSwapChain()->GetBuffer(0, IID_GRAPHICS_PPV_ARGS(&texture));
			if (FAILED(hr))
				throw ExceptionD3D11("GetBuffer", hr, static_cast<IDeviceD3D11*>(device)->GetDevice());
			get_back_buffer = true;
		}
		else
		{
			texture = static_cast<TextureD3D11*>(GetTexture().Get())->GetTextureResource();
		}

		{
			Memory::ReentrantLock lock(buffer_context_mutex);

			context->CopyResource(static_cast<TextureD3D11*>(dest->GetTexture().Get())->GetTextureResource(), texture);

			// update the destination's staging texture if there's any
			if (static_cast<TextureD3D11*>(dest->GetTexture().Get())->GetStagingResource())
			{
				context->CopyResource(static_cast<TextureD3D11*>(dest->GetTexture().Get())->GetStagingResource(), texture );
			}
		}

		if (get_back_buffer)
		{
			SAFE_RELEASE(texture);
		}
	}

	void RenderTargetD3D11::CopyToSysMem( RenderTarget* dest )
	{
		this->CopyTo(dest, false);
	}

	void RenderTargetD3D11::LockRect( LockedRect* pLockedRect, Lock lock)
	{
		auto source_texture = GetTexture();
		if ( source_texture )
		{
			source_texture->LockRect( 0, pLockedRect, lock );
		}
	}

	void RenderTargetD3D11::UnlockRect( )
	{
		auto source_texture = GetTexture();
		if ( source_texture )
		{
			source_texture->UnlockRect( 0 );
		}
	}

	void RenderTargetD3D11::SaveToFile( LPCWSTR pDestFile, ImageFileFormat DestFormat, const Rect* pSrcRect, bool want_alpha /*= false*/ )
	{
#ifndef _XBOX_ONE
	#ifdef _WIN32
		Memory::ReentrantLock lock(buffer_context_mutex);

		auto* context = static_cast<IDeviceD3D11*>(device)->GetContext();
		GUID format = GUID_ContainerFormatPng;
		switch (DestFormat)
		{
		case ImageFileFormat::BMP:
			format = GUID_ContainerFormatBmp;
			break;
		case ImageFileFormat::DDS:
			format = GUID_ContainerFormatDds;
			break;
		case ImageFileFormat::PNG:
			format = GUID_ContainerFormatPng;
			break;
		default:
			assert(0);
			return;
		}

		ID3D11Resource* texture = nullptr;
		bool texture_from_backbuffer = false;
		if (swap_chain)
		{
			const auto hr = swap_chain->GetSwapChain()->GetBuffer(0, IID_GRAPHICS_PPV_ARGS(&texture));
			if (FAILED(hr))
				throw ExceptionD3D11("GetBuffer", hr, static_cast<IDeviceD3D11*>(device)->GetDevice());

			texture_from_backbuffer = true;
		}
		else
			texture = static_cast<TextureD3D11*>(GetTexture().Get())->GetTextureResource();

		ScratchImage image;
		auto hr = DirectX::CaptureTexture(static_cast<IDeviceD3D11*>(device)->GetDevice(), context, texture, image);

		if (texture_from_backbuffer)
			SAFE_RELEASE(texture);

		if (FAILED(hr))
			throw ExceptionD3D11("CaptureTexture", hr, static_cast<IDeviceD3D11*>(device)->GetDevice());

		hr = DirectX::SaveToWICFile(image.GetImages(), image.GetImageCount(), DDS_FLAGS_NONE, format, pDestFile, want_alpha ? &GUID_WICPixelFormat32bppBGRA : &GUID_WICPixelFormat24bppBGR);
		if (FAILED(hr))
			throw ExceptionD3D11("SaveToWICFile", hr, static_cast<IDeviceD3D11*>(device)->GetDevice());
	#endif
#endif
	}

	void RenderTargetD3D11::GetDesc( SurfaceDesc* pDesc, int level )
	{
		ZeroMemory( pDesc, sizeof( SurfaceDesc ) );
		pDesc->Width = GetWidth();
		pDesc->Height = GetHeight();
		pDesc->Format = GetFormat();
	}

}
