
namespace Device
{
	SwapChainD3D11::SwapChainD3D11(HWND hwnd, IDevice* device, int width, int height, const bool fullscreen, const bool vsync, UINT Output)
		: device(device)
		, width(width)
		, height(height)
		, vsync(vsync)
	{
		Recreate(hwnd, device, width, height, fullscreen, vsync, Output);
	}

	SwapChainD3D11::~SwapChainD3D11()
	{
		render_target.reset();
		swap_chain.reset();
	}

	HRESULT SwapChainD3D11::CreateSwapChain(HWND hwnd, IDevice* _device, int _width, int _height, const bool fullscreen, const bool _vsync, UINT Output)
	{
		width = _width;
		height = _height;
		vsync = _vsync;
		fullscreen_failed = false;

		if (render_target)
			render_target.reset();

		auto device = ((IDeviceD3D11*)_device)->GetDevice();
		IDXGIDevice1* dxgiDevice1;
		auto hr = device->QueryInterface(IID_GRAPHICS_PPV_ARGS(&dxgiDevice1));
		if (FAILED(hr))
			throw ExceptionD3D11("QueryInterface", hr, device);

		IDXGIAdapter* dxgiAdapter;
		hr = dxgiDevice1->GetAdapter(&dxgiAdapter);
		if (FAILED(hr))
			throw ExceptionD3D11("GetAdapter", hr, device);
		SAFE_RELEASE(dxgiDevice1);

		if (swap_chain)
			swap_chain->SetFullscreenState(false, nullptr);

		DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
		UINT sample_count = 1;
		UINT sample_quality = 0;
		DXGI_MODE_DESC SwapChainBufferDesc;

		if (device->GetFeatureLevel() == D3D_FEATURE_LEVEL_11_1)
		{
			DXGI_SWAP_CHAIN_DESC1 swapChainDesc1;
			ZeroMemory(&swapChainDesc1, sizeof(DXGI_SWAP_CHAIN_DESC1));

		#ifndef _XBOX_ONE
			RECT rect;
			if (!fullscreen)
			{
				// Swapchain dimensions in windowed mode should always equals the client rectangle
				GetClientRect(hwnd, &rect);
				width = rect.right - rect.left;
				height = rect.bottom - rect.top;
			}
			swapChainDesc1.Width = width;
			swapChainDesc1.Height = height;
		#else
			swapChainDesc1.Width = (UINT)DynamicResolution::Get().GetBackbufferResolution().x;
			swapChainDesc1.Height = (UINT)DynamicResolution::Get().GetBackbufferResolution().y;
		#endif
			swapChainDesc1.BufferCount = (UINT)BACK_BUFFER_COUNT;
			swapChainDesc1.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			swapChainDesc1.Format = SWAP_CHAIN_FORMAT;
			swapChainDesc1.SampleDesc.Count = sample_count;
			swapChainDesc1.SampleDesc.Quality = sample_quality;
			swapChainDesc1.Scaling = DXGI_SCALING_STRETCH;
			swapChainDesc1.Flags = fullscreen ? DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH : 0;
			swapChainDesc1.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

		#ifdef _XBOX_ONE
			swapChainDesc1.Flags |= DXGIX_SWAP_CHAIN_MATCH_XBOX360_AND_PC;
		#endif

			IDXGIFactory2* dxgiFactory2;
			hr = dxgiAdapter->GetParent(IID_GRAPHICS_PPV_ARGS(&dxgiFactory2));
			if (FAILED(hr))
				throw ExceptionD3D11("GetParent", hr, device);

			IDXGISwapChain1* swapChain1 = nullptr;
		#ifdef _XBOX_ONE
			hr = dxgiFactory2->CreateSwapChainForCoreWindow(
				device,
				reinterpret_cast<IUnknown*>(Windows::UI::Core::CoreWindow::GetForCurrentThread()), //< obviously not awesome.
				&swapChainDesc1,
				nullptr,
				&swapChain1);
			if (FAILED(hr))
				throw ExceptionD3D11("CreateSwapChainForCoreWindow", hr, device);
		#else
			hr = dxgiFactory2->CreateSwapChainForHwnd(
				device,
				hwnd,
				&swapChainDesc1,
				nullptr,
				nullptr,
				&swapChain1);
			if (FAILED(hr))
				throw ExceptionD3D11("CreateSwapChainForHwnd", hr, device);
		#endif
			swap_chain.reset(swapChain1);

			format = swapChainDesc1.Format;

			// Setup the buffer desc
			SwapChainBufferDesc.Format = SWAP_CHAIN_FORMAT;
			SwapChainBufferDesc.Width = swapChainDesc1.Width;
			SwapChainBufferDesc.Height = swapChainDesc1.Height;
			SwapChainBufferDesc.RefreshRate.Numerator = 0;
			SwapChainBufferDesc.RefreshRate.Denominator = 0;
			SwapChainBufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
			SwapChainBufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
			SAFE_RELEASE(dxgiFactory2);
		}
	#ifndef _XBOX_ONE
		else
		{
			DXGI_SWAP_CHAIN_DESC swapChainDesc;
			ZeroMemory(&swapChainDesc, sizeof(DXGI_SWAP_CHAIN_DESC));

			RECT rect;
			if (!fullscreen)
			{
				// Swapchain dimensions in windowed mode should always equals the client rectangle
				GetClientRect(hwnd, &rect);
				width = rect.right - rect.left;
				height = rect.bottom - rect.top;
			}
			swapChainDesc.BufferCount = (UINT)BACK_BUFFER_COUNT;
			swapChainDesc.BufferDesc.Width = width;
			swapChainDesc.BufferDesc.Height = height;
			swapChainDesc.BufferDesc.Format = SWAP_CHAIN_FORMAT;
			swapChainDesc.BufferDesc.RefreshRate.Numerator = 0;
			swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
			swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
			swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
			swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			swapChainDesc.SampleDesc.Count = sample_count;
			swapChainDesc.SampleDesc.Quality = sample_quality;
			swapChainDesc.OutputWindow = hwnd;
			swapChainDesc.Windowed = !fullscreen;
			swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
			swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

			IDXGIFactory* dxgiFactory;
			hr = dxgiAdapter->GetParent(IID_GRAPHICS_PPV_ARGS(&dxgiFactory));
			if (FAILED(hr))
				throw ExceptionD3D11("GetParent", hr, device);

			IDXGISwapChain* swapChain = nullptr;
			hr = dxgiFactory->CreateSwapChain(device, &swapChainDesc, &swapChain);
			if (FAILED(hr))
				throw ExceptionD3D11("CreateSwapChain", hr, device);

			swap_chain.reset(swapChain);
			format = swapChainDesc.BufferDesc.Format;

			// Setup the buffer desc
			SwapChainBufferDesc = swapChainDesc.BufferDesc;
			SAFE_RELEASE(dxgiFactory);
		}
	#endif

		IDXGIOutput* dxgiOutput = nullptr;
		if (fullscreen)
			dxgiAdapter->EnumOutputs(Output, &dxgiOutput);

	#ifndef _XBOX_ONE
		if (fullscreen)
		{
			hr = swap_chain->ResizeTarget(&SwapChainBufferDesc);
			if (FAILED(hr))
			{
				SAFE_RELEASE(dxgiAdapter);
				return hr;
			}
		}

		LOG_DEBUG("CreateSwapChain: SetFullScreenState " << fullscreen);
		hr = swap_chain->SetFullscreenState(fullscreen, dxgiOutput);
		if (FAILED(hr))
		{
			fullscreen_failed = true;
		}
		else
		{
		#ifndef _XBOX_ONE
			IDXGIFactory* dxgiFactory;
			hr = dxgiAdapter->GetParent(IID_GRAPHICS_PPV_ARGS(&dxgiFactory));
			if (FAILED(hr))
				throw ExceptionD3D11("GetParent", hr, device);

			dxgiFactory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_WINDOW_CHANGES);

			SAFE_RELEASE(dxgiFactory);
		#endif
		}

		SAFE_RELEASE(dxgiAdapter);

		//From microsoft documentation: After calling SetFullscreenState, 
		//it is advisable to call ResizeTarget again with the RefreshRate member of DXGI_MODE_DESC zeroed out. 
		//This amounts to a no-operation instruction in DXGI, 
		//but it can avoid issues with the refresh rate, which are discussed next.
		if (fullscreen)
		{
			SwapChainBufferDesc.RefreshRate.Numerator = 0;
			SwapChainBufferDesc.RefreshRate.Denominator = 0;
			hr = swap_chain->ResizeTarget(&SwapChainBufferDesc);
			if (FAILED(hr))
				return hr;
		}
	#endif

		SAFE_RELEASE(dxgiOutput);

		return S_OK;
	}

	void SwapChainD3D11::Recreate(HWND hwnd, IDevice* device, int width, int height, const bool fullscreen, const bool vsync, UINT Output)
	{
		const auto hr = CreateSwapChain(hwnd, device, width, height, fullscreen, vsync, Output);
		if (FAILED(hr))
			throw ExceptionD3D11("CreateSwapChain", hr, ((IDeviceD3D11*)device)->GetDevice());

		CreateRenderTarget();
	}

	void SwapChainD3D11::Resize(int _width, int _height)
	{
		if (swap_chain && (_width != width || _height != height))
		{
			render_target.reset();
			width = _width;
			height = _height;
			swap_chain->ResizeBuffers(0, (UINT)width, (UINT)height, DXGI_FORMAT_UNKNOWN, 0);
			CreateRenderTarget();
		}
	}

	bool SwapChainD3D11::Present()
	{
	#if defined(_XBOX_ONE)
		std::array<IDXGISwapChain1*, 1> swap_chains =
		{
			(IDXGISwapChain1*)swap_chain.get()
		};

		DXGIX_PRESENTARRAY_PARAMETERS present_parameters[1];

		present_parameters[0].Disable = FALSE;
		present_parameters[0].UsePreviousBuffer = FALSE;
		present_parameters[0].SourceRect = { 0, 0, (INT)DynamicResolution::Get().GetBackbufferResolution().x, (INT)DynamicResolution::Get().GetBackbufferResolution().y };
		present_parameters[0].DestRectUpperLeft = { 0, 0 };
		present_parameters[0].ScaleFactorHorz = 1.0f;
		present_parameters[0].ScaleFactorVert = 1.0f;
		present_parameters[0].Cookie = nullptr;
		present_parameters[0].Flags = DXGIX_PRESENT_UPSCALE_FILTER_DEFAULT;

		int sync_interval = 1;
		UINT flag = 0;

		// It allows for presents that are synchronized to vsync (no tearing) when the target frame-rate is being met; 
		// and it allows for immediate presents (with tearing) when the target frame-rate is not being met.
		// The value of the parameter controls what portion of the screen is allowed to have tearing. 
		UINT PresentImmediateThreshold = 100; // TODO: Lower this value around ~25% to keep tearing in the top part of the screen where it's less noticeable.

		auto hr = DXGIXPresentArray(sync_interval, PresentImmediateThreshold, flag, (UINT)swap_chains.size(), swap_chains.data(), present_parameters);
		return SUCCEEDED(hr);
	#else
		int sync_interval = vsync ? 1 : 0;
		UINT flag = 0;
		const auto result = swap_chain->Present(sync_interval, flag);
		return SUCCEEDED(result);
	#endif
	}

	void SwapChainD3D11::CreateRenderTarget()
	{
		render_target.reset();
		render_target = RenderTarget::Create("Swap Chain", device, this);
	}

	bool SwapChainD3D11::SetFullscreenState(bool fullscreen)
	{
		if (swap_chain)
			return SUCCEEDED(swap_chain->SetFullscreenState(fullscreen ? TRUE : FALSE, nullptr));

		return false;
	}

	HRESULT SwapChainD3D11::RecreateBuffers(UINT Width, UINT Height, UINT Flags)
	{
		UINT buffer_count = 0;
		DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;

		if (((IDeviceD3D11*)device)->GetDevice()->GetFeatureLevel() == D3D_FEATURE_LEVEL_11_1)
		{
			IDXGISwapChain1* swap_chain1 = (IDXGISwapChain1*)swap_chain.get();

			DXGI_SWAP_CHAIN_DESC1 _swapChainDesc1;
			swap_chain1->GetDesc1(&_swapChainDesc1);

			format = _swapChainDesc1.Format;
			buffer_count = _swapChainDesc1.BufferCount;
		}
		else
		{
			DXGI_SWAP_CHAIN_DESC _swapChainDesc;
			swap_chain->GetDesc(&_swapChainDesc);

			format = _swapChainDesc.BufferDesc.Format;
			buffer_count = _swapChainDesc.BufferCount;
		}

		render_target.reset();
		const auto hr = swap_chain->ResizeBuffers(buffer_count, Width, Height, format, Flags);
		CreateRenderTarget();

		return hr;
	}
}
