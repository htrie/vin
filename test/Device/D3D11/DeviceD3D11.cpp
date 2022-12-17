
namespace Device
{
	extern Memory::ReentrantMutex buffer_context_mutex;

	IDeviceD3D11::IDeviceD3D11(DeviceInfo* device_info, DeviceType DeviceType, HWND hFocusWindow, const Renderer::Settings& renderer_settings)
		: IDevice(renderer_settings)
		, _adapter_ordinal(renderer_settings.adapter_index)
		, fullscreen(renderer_settings.fullscreen)
		, vsync(renderer_settings.vsync)
	{
		DynamicResolution::Get().Reset(renderer_settings.resolution.width, renderer_settings.resolution.height);

		// Force WIC factory creation on main thread.
		bool iswic2 = false;
		auto* pWIC = DirectX::GetWICFactory(iswic2);
		if (pWIC == nullptr)
			throw ExceptionD3D11("Failed to create WIC Factory.");

		// Determine the Device Creation flags.
		UINT creationFlags = 0;
	#if defined(ALLOW_DEBUG_LAYER)
		if (debug_layer)
			creationFlags |= D3D11_CREATE_DEVICE_DEBUG; // includes D3D11_CREATE_DEVICE_VALIDATED
	#endif
	#if defined(_XBOX_ONE)
		creationFlags |= D3D11_CREATE_DEVICE_FAST_KICKOFFS; // Eliminates costly transitions to and from the host OS for kickoffs, by using a local graphics primary ring buffer.
		creationFlags |= D3D11_CREATE_DEVICE_WRITE_COMBINED_COMMAND_BUFFERS; // Causes Direct3D to create all of its internal command buffers in write-combined memory.
		creationFlags |= D3D11_CREATE_DEVICE_WRITE_COMBINED_DYNAMIC_BUFFERS; // Causes Direct3D to create all of its Dynamic Resource buffers in write-combined memory.
	#if defined(DEVELOPMENT_CONFIGURATION)
		creationFlags |= D3D11_CREATE_DEVICE_INSTRUMENTED; // Ensures instrumentation in the graphics library, for use with PIX.
	#endif
	#endif

		IDXGIAdapter* pAdapter = nullptr;
		D3D_DRIVER_TYPE driver_type = D3D_DRIVER_TYPE_HARDWARE;

	#ifndef _XBOX_ONE
		auto enumeration = static_cast<DeviceInfoDXGI*>(device_info)->GetEnumeration();
		if (enumeration == nullptr)
		{
			throw ExceptionD3D11("GetEnumeration");
		}

		unsigned adapter_ordinal = device_info->GetCurrentAdapterOrdinal(renderer_settings.adapter_index);

		auto adapter_info = enumeration->GetAdapterInfo(adapter_ordinal);
		if (adapter_info == nullptr)
		{
			throw ExceptionD3D11("GetAdapterInfo");
		}
		pAdapter = adapter_info->m_pAdapter;
		driver_type = D3D_DRIVER_TYPE_UNKNOWN;
	#endif

			//
			// Create the DirectX device and context.
			//
		runtime_version = DeviceRuntimeVersion::RUNTIME_VERSION_11_1;
		HRESULT hr;
		ID3D11Device* native_device;
		ID3D11DeviceContext* context;

		{
			std::array<D3D_FEATURE_LEVEL, 4> feature_levels = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0 };

		#if defined(_XBOX_ONE)
			D3D11X_CREATE_DEVICE_PARAMETERS params;
			params.Version = D3D11_SDK_VERSION;
			params.Flags = creationFlags;
			params.pOffchipTessellationBuffer = nullptr;
			params.pTessellationFactorsBuffer = nullptr;
			params.DeferredDeletionThreadAffinityMask = 0x20; // Core 5
			params.ImmediateContextDeRingSizeBytes = 0;
			params.ImmediateContextCeRingSizeBytes = 0;
			params.ImmediateContextDeSegmentSizeBytes = 0;
			params.ImmediateContextCeSegmentSizeBytes = 0;
			params.DisableGraphicsScratchMemoryAllocation = true;
			params.DisableComputeScratchMemoryAllocation = false;
			params.DisableGeometryShaderAllocations = true;
			params.DisableTessellationShaderAllocations = true;

			hr = D3D11XCreateDeviceX(
				&params,
				(ID3D11DeviceX**)&native_device,
				(ID3D11DeviceContextX**)&context);
		#else
			hr = D3D11CreateDevice(
				pAdapter,
				driver_type,
				(HMODULE)0,
				creationFlags,
				feature_levels.data(),
				(UINT)feature_levels.size(),
				D3D11_SDK_VERSION,
				&native_device,
				nullptr,
				&context);

			if ((feature_levels.size() > 1) && (hr == E_INVALIDARG))
			{
				runtime_version = DeviceRuntimeVersion::RUNTIME_VERSION_11_0;

				hr = D3D11CreateDevice(
					pAdapter,
					driver_type,
					(HMODULE)0,
					creationFlags,
					&feature_levels[1],
					(UINT)feature_levels.size() - 1,
					D3D11_SDK_VERSION,
					&native_device,
					nullptr,
					&context);
			}

			DXGI_ADAPTER_DESC desc;
			hr = pAdapter->GetDesc(&desc);
			if (SUCCEEDED(hr))
				available_vram = desc.DedicatedVideoMemory;
			
			available_vram = std::max<uint64_t>(available_vram, 1400 * Memory::MB);
		#endif
		}

		if (FAILED(hr))
		{
			throw ExceptionD3D11("D3D11CreateDevice", hr);
		}

		_device.reset(native_device);
		_context.reset(context);

		feature_level = DeviceFeatureLevel::FEATURE_LEVEL_NONE;
		switch (native_device->GetFeatureLevel())
		{
			case D3D_FEATURE_LEVEL_11_1: feature_level = DeviceFeatureLevel::FEATURE_LEVEL_11_1; break;
			case D3D_FEATURE_LEVEL_11_0: feature_level = DeviceFeatureLevel::FEATURE_LEVEL_11_0; break;
			case D3D_FEATURE_LEVEL_10_1: feature_level = DeviceFeatureLevel::FEATURE_LEVEL_10_1; break;
			case D3D_FEATURE_LEVEL_10_0: feature_level = DeviceFeatureLevel::FEATURE_LEVEL_10_0; break;
		}

		UINT output = 0;
	#ifndef _XBOX_ONE
		output = device_info->GetCurrentAdapterMonitorIndex(renderer_settings.adapter_index);
	#endif
		_swap_chain = Device::SwapChain::Create(hFocusWindow, this, renderer_settings.resolution.width, renderer_settings.resolution.height, fullscreen, vsync, output);

	#if defined(ALLOW_DEBUG_LAYER) && !defined(_XBOX_ONE)
		if (debug_layer && SUCCEEDED(native_device->QueryInterface(IID_GRAPHICS_PPV_ARGS(&d3dDebug))))
		{
			ID3D11InfoQueue* d3dInfoQueue = nullptr;
			if (SUCCEEDED(d3dDebug->QueryInterface(IID_GRAPHICS_PPV_ARGS(&d3dInfoQueue))))
			{
			#if defined(DEVELOPMENT_CONFIGURATION)
				d3dInfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, true);
				d3dInfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, true);
				//d3dInfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_WARNING, true);
			#endif

				D3D11_MESSAGE_ID hide[] =
				{
					D3D11_MESSAGE_ID_SETPRIVATEDATA_CHANGINGPARAMS,
					D3D11_MESSAGE_ID_DEVICE_DRAW_RENDERTARGETVIEW_NOT_SET,
					D3D11_MESSAGE_ID_DEVICE_OMSETRENDERTARGETS_HAZARD,
					D3D11_MESSAGE_ID_DEVICE_PSSETSHADERRESOURCES_HAZARD,
					// Add more message IDs here as needed
				};

				D3D11_INFO_QUEUE_FILTER filter;
				memset(&filter, 0, sizeof(filter));
				filter.DenyList.NumIDs = _countof(hide);
				filter.DenyList.pIDList = hide;
				d3dInfoQueue->AddStorageFilterEntries(&filter);
				d3dInfoQueue->Release();
			}
		}
	#endif

		{
			device_settings.AdapterOrdinal = renderer_settings.adapter_index;
			device_settings.Windowed = !fullscreen;
			device_settings.BackBufferWidth = renderer_settings.resolution.width;
			device_settings.BackBufferHeight = renderer_settings.resolution.height;
		}

	#if defined(ALLOW_DEBUG_LAYER)
		if (debug_layer)
		{
			if (runtime_version == DeviceRuntimeVersion::RUNTIME_VERSION_11_1)
			{
				ID3DUserDefinedAnnotation* pAnnotation;
				HRESULT hr = GetContext()->QueryInterface(IID_GRAPHICS_PPV_ARGS(&pAnnotation));
				annotation.reset(pAnnotation);
			}
		}
	#endif

		{
			std::wstring runtime_version_name = L"None";
			switch (runtime_version)
			{
				case DeviceRuntimeVersion::RUNTIME_VERSION_11_1: runtime_version_name = L"11.1"; break;
				case DeviceRuntimeVersion::RUNTIME_VERSION_11_0: runtime_version_name = L"11.0"; break;
			}

			std::wstring feature_level_name = L"None";
			switch (feature_level)
			{
				case DeviceFeatureLevel::FEATURE_LEVEL_11_1: feature_level_name = L"11.1"; break;
				case DeviceFeatureLevel::FEATURE_LEVEL_11_0: feature_level_name = L"11.0"; break;
				case DeviceFeatureLevel::FEATURE_LEVEL_10_1: feature_level_name = L"10.1"; break;
				case DeviceFeatureLevel::FEATURE_LEVEL_10_0: feature_level_name = L"10.0"; break;
			}

		#ifdef _XBOX_ONE // CheckFeatureSupport not implemented on XBox One.
			driver_command_lists = true;
			driver_concurrent_creates = true;
			const bool driver_constant_buffer_offsetting = true;
		#else
			D3D11_FEATURE_DATA_THREADING threadingCaps = { FALSE, FALSE };
			hr = _device->CheckFeatureSupport(D3D11_FEATURE_THREADING, &threadingCaps, sizeof(threadingCaps));
			driver_command_lists = SUCCEEDED(hr) && threadingCaps.DriverCommandLists;
			driver_concurrent_creates = SUCCEEDED(hr) && threadingCaps.DriverConcurrentCreates;

			D3D11_FEATURE_DATA_D3D11_OPTIONS optionsCaps;
			hr = _device->CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS, &optionsCaps, sizeof(optionsCaps));
			const bool driver_constant_buffer_offsetting = SUCCEEDED(hr) && optionsCaps.ConstantBufferOffsetting;
			if (!driver_constant_buffer_offsetting)
				throw ExceptionD3D11("Driver doesn't support Constant Buffer Offsetting", hr);
		#endif
			LOG_INFO("[D3D11] Runtime version = " << runtime_version_name);
			LOG_INFO("[D3D11] Feature Level = " << feature_level_name);
			LOG_INFO("[D3D11] Driver Command Lists = " << (driver_command_lists ? "YES" : "NO"));
			LOG_INFO("[D3D11] Driver Concurrent Creates = " << (driver_concurrent_creates ? "YES" : "NO"));
			LOG_INFO("[D3D11] Driver Constant Buffer Offsetting = " << (driver_constant_buffer_offsetting ? "YES" : "NO"));
		}

		IDevice::Init(renderer_settings);
	}

	IDeviceD3D11::~IDeviceD3D11()
	{
		WaitForIdle();

		IDevice::Quit();

		annotation.reset();

		((SwapChainD3D11*)_swap_chain.get())->SetFullscreenState(false);
		_swap_chain.reset();
		_context->ClearState();
		_context->Flush();
		_context.reset();
		_device.reset();

	#if !defined(_XBOX_ONE)
		if (d3dDebug)
		{
			d3dDebug->ReportLiveDeviceObjects(D3D11_RLDO_SUMMARY | D3D11_RLDO_DETAIL);
			d3dDebug->Release();
		}
	#endif
	}

	void IDeviceD3D11::SetDebugName(ID3D11Resource* resource, const Memory::DebugStringA<>& name)
	{
	#if defined(ALLOW_DEBUG_LAYER) && !defined(_XBOX_ONE)
		resource->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)name.size(), name.c_str());
	#endif
	}

	void IDeviceD3D11::RecreateSwapChain(HWND hwnd, UINT Output, const Renderer::Settings& renderer_settings)
	{
		//there should already be an existing swap chain, this function is for recreating for resize
		assert(_swap_chain);

		_swap_chain->Recreate(hwnd, this, renderer_settings.resolution.width, renderer_settings.resolution.height, renderer_settings.fullscreen, renderer_settings.vsync, Output);

		fullscreen = renderer_settings.fullscreen;

		ResizeDXGIBuffers(renderer_settings.resolution.width, renderer_settings.resolution.height, renderer_settings.fullscreen);
	}

	bool IDeviceD3D11::CheckFullScreenFailed()
	{
	#ifdef _XBOX_ONE
		return false;
	#else
		return ((SwapChainD3D11*)_swap_chain.get())->GetFullscreenFailed();
	#endif
	}

	void IDeviceD3D11::Suspend()
	{
	#if defined(_XBOX_ONE)
		if (auto context = ((ID3D11DeviceContextX*)GetContext()))
			context->Suspend(0);
	#endif
	}

	void IDeviceD3D11::Resume()
	{
	#if defined(_XBOX_ONE)
		if (auto context = ((ID3D11DeviceContextX*)GetContext()))
			context->Resume();
	#endif
	}

	void IDeviceD3D11::WaitForIdle()
	{
		// TODO
	}

	void IDeviceD3D11::BeginEvent(CommandBuffer& command_buffer, const std::wstring& text)
	{
	#if defined(ALLOW_DEBUG_LAYER)
		if (annotation)
		{
			Memory::ReentrantLock lock(buffer_context_mutex);
			auto found = markers.insert(text.data());
			annotation->BeginEvent(found.first->data());
		}
	#endif
	}

	void IDeviceD3D11::EndEvent(CommandBuffer& command_buffer)
	{
	#if defined(ALLOW_DEBUG_LAYER)
		if (annotation)
		{
			Memory::ReentrantLock lock(buffer_context_mutex);
			annotation->EndEvent();
		}
	#endif
	}

	void IDeviceD3D11::SetMarker(CommandBuffer& command_buffer, const std::wstring& text)
	{
	#if defined(ALLOW_DEBUG_LAYER)
		if (annotation)
		{
			Memory::ReentrantLock lock(buffer_context_mutex);
			auto found = markers.insert(text);
			annotation->SetMarker(found.first->data());
		}
	#endif
	}

	bool IDeviceD3D11::SupportsSubPasses() const
	{
		return false;
	}

	bool IDeviceD3D11::SupportsParallelization() const
	{
		return true;
	}

	bool IDeviceD3D11::SupportsCommandBufferProfile() const
	{
		return false;
	}

	unsigned IDeviceD3D11::GetWavefrontSize() const
	{
	#ifdef _XBOX_ONE
		return 64;
	#else
		return 4; // Lowest value because DX11 doesn't want to tell us the real value
	#endif
	}

	VRAM IDeviceD3D11::GetVRAM() const
	{
		VRAM vram;
	#if defined(_XBOX_ONE)
		vram.used = Memory::GetUsedVRAM();
		vram.reserved = vram.used;
		vram.available = Memory::GetMaxVRAM();
		vram.total = Memory::GetMaxVRAM();
	#else
		vram.used = 0;
		vram.reserved = vram.used;
		vram.available = available_vram;
		vram.total = available_vram;
	#endif
		return vram;
	}

	bool IDeviceD3D11::IsWindowed() const
	{
		return !fullscreen;
	}

	void IDeviceD3D11::Swap()
	{
		IDevice::Swap();
		markers.clear();
	}

	HRESULT IDeviceD3D11::GetBackBufferDesc( UINT iSwapChain, UINT iBackBuffer, SurfaceDesc* pBBufferSurfaceDesc )
	{
		DXGI_SWAP_CHAIN_DESC swapChainDesc;
		HRESULT hr = ((SwapChainD3D11*)_swap_chain.get())->GetSwapChain()->GetDesc(&swapChainDesc);
		if( SUCCEEDED( hr ) )
		{
			ZeroMemory( pBBufferSurfaceDesc, sizeof( SurfaceDesc ) );
			pBBufferSurfaceDesc->Width = swapChainDesc.BufferDesc.Width;
			pBBufferSurfaceDesc->Height = swapChainDesc.BufferDesc.Height;
			pBBufferSurfaceDesc->Format = UnconvertPixelFormat( swapChainDesc.BufferDesc.Format );
		}

		return hr;
	}

	void IDeviceD3D11::BeginFrame( )
	{
		
	}

	void IDeviceD3D11::EndFrame( )
	{
		
	}

	void IDeviceD3D11::BeginFlush()
	{
		constant_buffer.Finalize(GetContext());

	#if _XBOX_ONE
		// This saves significant CPU performance: Direct3D doesn’t have to reset all of the API state on every ExecuteCommandList;
		// rather, it does so only once at EndCommandListExecution time.
		// Note that other Draw methods cannot be called while in a BeginCommandListExecution/EndCommandListExecution bracket.
		{
			Memory::ReentrantLock lock(buffer_context_mutex);
			((ID3D11DeviceContextX*)GetContext())->BeginCommandListExecution(0);
		}
	#endif
	}

	void IDeviceD3D11::Flush(CommandBuffer& command_buffer)
	{
		static_cast<CommandBufferD3D11&>(command_buffer).Flush();
	}

	void IDeviceD3D11::EndFlush()
	{
	#if _XBOX_ONE
		{
			Memory::ReentrantLock lock(buffer_context_mutex);
			((ID3D11DeviceContextX*)GetContext())->EndCommandListExecution();
		}
	#endif

		constant_buffer.Reset();
	}

	void IDeviceD3D11::WaitForBackbuffer(CommandBuffer& command_buffer)
	{
	}

	void IDeviceD3D11::Submit()
	{
	}

	void IDeviceD3D11::Present(const Rect* pSourceRect, const Rect* pDestRect, HWND hDestWindowOverride)
	{
		Memory::ReentrantLock lock(buffer_context_mutex);

		if (deferred_context_used)
		{
			deferred_context_used = false;

			ID3D11CommandList* command_list = NULL;
			auto hr = GetContext()->FinishCommandList(FALSE, &command_list);
			if (FAILED(hr))
				throw ExceptionD3D11("FinishCommandList", hr, _device.get());

			GetContext()->ExecuteCommandList(command_list, FALSE);

			command_list->Release();
		}
		//DEBUG: This thing fixes dynamic resolution with vsync but might have performance impact. Use 'T' key to test the impact with and without it.
#if !defined(_XBOX_ONE)
		if(vsync)
		{
			GetContext()->Flush();
		}
#endif

	#ifdef EPIC_PLATFORM_INTEGRATION
		// We need to set the viewport for the epic games overlay to render
		D3D11_VIEWPORT default_viewport = { 0, 0, DynamicResolution::Get().GetBackbufferResolution().x, DynamicResolution::Get().GetBackbufferResolution().y, 0, 1.0f };
		GetContext()->RSSetViewports(1, &default_viewport);
	#endif

		if (!_swap_chain->Present())
			return;

		/*{
			std::wstringstream debug_output;
			debug_output << "[Renderer] delay : " << profile->GetCollectDelay();
			OutputDebugStringW(debug_output.str().c_str());
		}*/
	}

	void IDeviceD3D11::ResourceFlush()
	{
		WaitForIdle();

		releaser->Clear();

		GetContext()->Flush();
	}

	HRESULT IDeviceD3D11::CheckForDXGIBufferChange()
	{
		HRESULT hr = S_OK;
		if(_swap_chain != nullptr )
		{
			WindowDX* window = WindowDX::GetWindow();
			if (window)
			{
#ifndef _XBOX_ONE
				//find out whether we are in windowed mode or not
				bool windowed = device_settings.Windowed > 0;
				if ( windowed )
				{
					//get the current back buffer width and height
					SurfaceDesc desc;
					hr = GetBackBufferDesc( 0, 0, &desc );
					if ( FAILED( hr ) )
					{
						LOG_CRIT( L"Failed to Check BackBuffer Description in CheckForDXGIBufferChange " );
						return hr;
					}
					unsigned width = desc.Width;
					unsigned height = desc.Height;

					//get the correct rect depending if we are in window mode or fullscreen mode
					RECT rc_current_client;
					HWND hwnd = window->GetWnd();
					GetClientRect( hwnd, &rc_current_client );

					if ( ( width != rc_current_client.right ) ||
						( height != rc_current_client.bottom ) )
					{
						hr = ResizeDXGIBuffers( rc_current_client.right, rc_current_client.bottom, fullscreen );
					}

					UINT flag = window->IsMaximized() ? SW_SHOWMAXIMIZED : SW_SHOW;
					window->ShowWindow(flag);
				}
#else
				hr = ResizeDXGIBuffers( 0, 0, fullscreen );
#endif
			}
		}

		return hr;
	}
	
	HRESULT IDeviceD3D11::ResizeDXGIBuffers( UINT Width, UINT Height, BOOL bFullScreen, BOOL bBorderless /*=false*/ )
	{
		HRESULT hr = S_OK;
		WindowDX* window = WindowDX::GetWindow();
		if (window == nullptr)
			return hr;

#ifndef _XBOX_ONE
		if ( !bFullScreen )
		{
			if ( bBorderless )
			{
				// Swapchain dimensions in borderless mode should always equals the monitor resolution
				window->GetMonitorDimensions( device_settings.AdapterOrdinal, &Width, &Height );
			}
			else
			{
				// Swapchain dimensions in windowed mode should always equals the client rectangle
				RECT rect;
				GetClientRect( window->GetWnd(), &rect );
				Width = rect.right - rect.left;
				Height = rect.bottom - rect.top;
			}
		}
		else
#endif
		if ( Width == 0 || Height == 0 )
		{
			Width = device_settings.BackBufferWidth;
			Height = device_settings.BackBufferHeight;
		}

		// Update dynamic resolution bounds.
		DynamicResolution::Get().Reset(Width, Height);

		GetContext()->Flush();
		GetContext()->ClearState();
		ID3D11RenderTargetView* nullViews[] = { nullptr };
		GetContext()->OMSetRenderTargets( ARRAYSIZE( nullViews ), nullViews, nullptr );

		// Call releasing
		window->TriggerDeviceLost();

	#ifdef _XBOX_ONE
		releaser->Clear();
	#endif

		// Alternate between 0 and DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH when resizing buffers.
		// When in windowed mode, we want 0 since this allows the app to change to the desktop
		// resolution from windowed mode during alt+enter.  However, in fullscreen mode, we want
		// the ability to change display modes from the Device Settings dialog.  Therefore, we
		// want to set the DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH flag.
		UINT Flags = 0;
	#ifndef _XBOX_ONE
		if ( bFullScreen )
			Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	#endif

		GetContext()->Flush();

		// Resize buffers.
		hr = ((SwapChainD3D11*)_swap_chain.get())->RecreateBuffers(Width, Height, Flags);
	#ifndef _XBOX_ONE
		if (FAILED(hr))
		{
			LOG_CRIT(L"Failed to resize swap chain, possibly during windowed <-> fullscreen toggling");
			return hr;
		}
	#endif

		// Call the app's SwapChain reset callback
		SurfaceDesc desc;
		hr = GetBackBufferDesc( 0, 0, &desc );
		if ( FAILED( hr ) )
		{
			LOG_CRIT( L"Failed to get back buffer, possibly during windowed <-> fullscreen toggling" );
			return hr;
		}

		window->TriggerDeviceReset( this, &desc );

		Pause(false, false);

		if ( FAILED( hr ) )
		{
#ifndef _XBOX_ONE
			PostQuitMessage( 0 );
#endif
		}

		return hr;
	}

	ConstantBufferD3D11::Buffer& IDeviceD3D11::LockFromConstantBuffer(size_t size, ID3D11Buffer*& out_buffer, uint8_t*& out_map)
	{
		Memory::Lock lock(constant_buffer_mutex);
		return constant_buffer.Lock(GetDevice(), GetContext(), size, out_buffer, out_map);
	}

	void IDeviceD3D11::UnlockFromConstantBuffer(ConstantBufferD3D11::Buffer& buffer)
	{
		Memory::Lock lock(constant_buffer_mutex);
		constant_buffer.Unlock(GetContext(), buffer);
	}

	uint8_t* IDeviceD3D11::AllocateFromConstantBuffer(const size_t size, ID3D11Buffer*& out_buffer, UINT& out_constant_offset, UINT& out_constant_count)
	{
		Memory::Lock lock(constant_buffer_mutex);
		return constant_buffer.Allocate(GetDevice(), GetContext(), size, out_buffer, out_constant_offset, out_constant_count);
	}


	void IDeviceD3D11::SetVSync(bool enabled) 
	{ 
		vsync = enabled; 
		if (_swap_chain)
			((SwapChainD3D11*)_swap_chain.get())->SetVSync(enabled);
	}

	bool IDeviceD3D11::GetVSync() { return vsync; }

	PROFILE_ONLY(std::vector<std::vector<std::string>> IDeviceD3D11::GetStats() { return std::vector<std::vector<std::string>>(); })
	PROFILE_ONLY(size_t IDeviceD3D11::GetSize() { return 0; })

}
