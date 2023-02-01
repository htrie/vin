
namespace Device
{
	SwapChainD3D12::SwapChainD3D12(HWND hwnd, IDevice* device, int width, int height, const bool fullscreen, const bool vsync, UINT Output)
		: device(static_cast<IDeviceD3D12*>(device))
		, width(width)
		, height(height)
	{
		CreateSwapChain(hwnd, width, height, fullscreen, vsync, Output);
	}

	UINT SwapChainD3D12::GetFlags(bool fullscreen)
	{
#if !defined(_XBOX_ONE)
		// Alternate between 0 and DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH when resizing buffers.
		// When in windowed mode, we want 0 since this allows the app to change to the desktop
		// resolution from windowed mode during alt+enter.  However, in fullscreen mode, we want
		// the ability to change display modes from the Device Settings dialog.  Therefore, we
		// want to set the DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH flag.

		UINT flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
		if (device->GetTearingSupported()) flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
		return flags;
#else
		return 0;
#endif
	}

	DXGI_SWAP_EFFECT GetSwapEffect()
	{
#if !defined(_XBOX_ONE)
		return DXGI_SWAP_EFFECT_FLIP_DISCARD;
#else
		return DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
#endif
	}

	void SwapChainD3D12::CreateSwapChain(HWND hwnd, int _width, int _height, const bool fullscreen, const bool _vsync, UINT Output)
	{
		width = _width;
		height = _height;
		vsync = _vsync;

		render_target.reset();

		auto* command_queue = device->GetGraphicsQueue();
		auto* factory = device->GetFactory();
		auto adapter = device->GetAdapter();

		flags = GetFlags(fullscreen);

		DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
		swapChainDesc.BufferCount = IDeviceD3D12::FRAME_QUEUE_COUNT;
		swapChainDesc.Width = width;
		swapChainDesc.Height = height;
		swapChainDesc.Format = SWAP_CHAIN_FORMAT;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
		swapChainDesc.SwapEffect = GetSwapEffect();
		swapChainDesc.SampleDesc.Count = 1;
		swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
		swapChainDesc.Flags = flags;

#if !defined(_XBOX_ONE)

		if (FAILED(factory->CreateSwapChainForHwnd(
			command_queue,
			hwnd,
			&swapChainDesc,
			nullptr,
			nullptr,
			&swap_chain)))
			throw ExceptionD3D12("CreateSwapChainForHwnd failed");

		if (FAILED(swap_chain.As(&swap_chain4)))
			throw ExceptionD3D12("IDXGISwapChain4 conversion failed");
#else
		swapChainDesc.Flags |= DXGIX_SWAP_CHAIN_MATCH_XBOX360_AND_PC;

		if (FAILED(factory->CreateSwapChainForCoreWindow(
			device->GetDevice(),
			reinterpret_cast<IUnknown*>(Windows::UI::Core::CoreWindow::GetForCurrentThread()), //< obviously not awesome.
			&swapChainDesc,
			nullptr,
			&swap_chain)))
			throw ExceptionD3D12("CreateSwapChainForCoreWindow failed");
#endif

#if !defined(_XBOX_ONE)
		waitable_object = swap_chain4->GetFrameLatencyWaitableObject();
		swap_chain4->SetMaximumFrameLatency(2); // TODO: figure out the optimal value
#else
		// Don't use async present for now
		//DXGIXSetVLineNotification(VLINECOUNTER0, 0, waitable_object);
#endif

		render_target = std::make_unique<RenderTargetD3D12>("Swap Chain Render Target", device, this);

		UpdateBufferIndex();
	}

	void SwapChainD3D12::UpdateBufferIndex()
	{
#if !defined(_XBOX_ONE)
		current_backbuffer_index = swap_chain4->GetCurrentBackBufferIndex();
		static_cast<RenderTargetD3D12*>(render_target.get())->Swap();
#else
		current_backbuffer_index = (current_backbuffer_index + 1) % IDeviceD3D12::FRAME_QUEUE_COUNT;
		static_cast<RenderTargetD3D12*>(render_target.get())->Swap();
#endif
	}

	void SwapChainD3D12::Recreate(HWND hwnd, IDevice* device, int _width, int _height, const bool fullscreen, const bool vsync, UINT Output)
	{
		this->vsync = vsync;

		UINT flags = GetFlags(fullscreen);
		ResizeBuffers(_width, _height, flags);
	}

	SwapChainD3D12::~SwapChainD3D12()
	{
#if !defined(_XBOX_ONE)
		swap_chain->SetFullscreenState(false, nullptr);
#endif
		render_target.reset();
		swap_chain.Reset();
	}

	void SwapChainD3D12::Resize(int _width, int _height)
	{
		ResizeBuffers(_width, _height, flags);
	}

	HRESULT SwapChainD3D12::ResizeBuffers(UINT Width, UINT Height, UINT Flags)
	{
#if defined(_XBOX_ONE)
		if (flags == Flags && width == Width && height == Height)
		{
			return S_OK;
		}
#endif

		flags = Flags;
		width = Width;
		height = Height;

		render_target = nullptr;
		releaser->Clear();

		auto hr = swap_chain->ResizeBuffers(IDeviceD3D12::FRAME_QUEUE_COUNT, width, height, SWAP_CHAIN_FORMAT, flags);

		render_target = std::make_unique<RenderTargetD3D12>("Swap Chain Render Target", device, this);
		UpdateBufferIndex();
		return hr;
	}

	bool SwapChainD3D12::Present()
	{
#if defined(_XBOX_ONE)
		std::array<IDXGISwapChain1*, 1> swap_chains =
		{
			(IDXGISwapChain1*)swap_chain.Get()
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

		int sync_interval = vsync ? 1 : 0;
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

	int SwapChainD3D12::GetWidth() const
	{
		return width;
	}

	int SwapChainD3D12::GetHeight() const
	{
		return height;
	}

	void SwapChainD3D12::WaitForBackbuffer()
	{
		PROFILE;

#if !defined(_XBOX_ONE)
		DWORD result = WaitForSingleObjectEx(waitable_object, 1000, true);
#endif
	}
}
