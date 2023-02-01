
namespace Device
{

	namespace
	{
		constexpr size_t CONSTANT_BUFFER_MAX_SIZE_D3D12 = 4 * Memory::MB; // Needs to be bigger than Vulkan because uniform alignment is 256 instead of 4.
		// We'll probably crash with TooManyObjects errors if we go above this limit
		constexpr uint64_t MAX_VRAM_USAGE_D3D12 = 8 * Memory::GB;

		constexpr uint32_t SHADER_VISIBLE_DESCRIPTOR_COUNT = 1000000;

		
#if !defined(_XBOX_ONE)
		typedef HRESULT(WINAPI* PFN_CreateDXGIFactory2)(UINT Flags, _In_ REFIID, _COM_Outptr_ void**);

		// On win7 get pointer to CreateDXGIFactory2 dynamically.
		// CreateDXGIFactory2 doesn't exist in dxgi.dll on win7 and referencing it in code causes startup failure.
		struct D3D12Functions
		{
			PFN_CreateDXGIFactory2 create_dxgi_factory2 = nullptr;

			D3D12Functions()
			{
				static HINSTANCE dxgi_dll = LoadLibrary(L"dxgi.dll");
				create_dxgi_factory2 = (PFN_CreateDXGIFactory2)GetProcAddress(dxgi_dll, "CreateDXGIFactory2");
				if (!create_dxgi_factory2)
					LOG_CRIT(L"Can't find CreateDXGIFactory2 function in dxgi.dll");
			}
		};

		const D3D12Functions& GetD3D12Functions()
		{
			static D3D12Functions functions;
			return functions;
		}
#endif
	}

	const AdapterInfo* IDeviceD3D12::GetSelectedAdapter()
	{
		Device::AdapterInfo* selected_adapter = nullptr;
		if (auto* window = WindowDX::GetWindow())
		{
			auto& info_list = window->GetInfo()->GetAdapterInfoList();
			if (adapter_index >= info_list.size())
				throw ExceptionD3D12("Out-of-range adapter index");
			selected_adapter = &info_list[adapter_index];
		}
		return selected_adapter;
	}

#ifndef _XBOX_ONE
	Memory::SmallVector<ComPtr<IDXGIAdapter1>, 4, Memory::Tag::Device> GetDXGIAdapters(IDXGIFactory1* factory)
	{
		Memory::SmallVector<ComPtr<IDXGIAdapter1>, 4, Memory::Tag::Device> result;

		ComPtr<IDXGIFactory6> factory6;
		ComPtr<IDXGIAdapter1> adapter;
		if (SUCCEEDED(factory->QueryInterface(IID_PPV_ARGS(&factory6))))
		{
			for (UINT adapter_index = 0; DXGI_ERROR_NOT_FOUND != factory6->EnumAdapterByGpuPreference(adapter_index, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter)); ++adapter_index)
			{
				if (!adapter) break;

				DXGI_ADAPTER_DESC1 desc;
				if (FAILED(adapter->GetDesc1(&desc)))
					continue;

				if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
					continue;

				if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device4), nullptr)))
					result.push_back(adapter);
			}
		}
		else
		{
			for (UINT adapter_index = 0; DXGI_ERROR_NOT_FOUND != factory->EnumAdapters1(adapter_index, &adapter); ++adapter_index)
			{
				DXGI_ADAPTER_DESC1 desc;
				if (FAILED(adapter->GetDesc1(&desc)))
					continue;

				if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
					continue;

				if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device4), nullptr)))
					result.push_back(adapter);
			}
		}

		return result;
	}

	ComPtr<IDXGIAdapter1> FindSelectedDXGIAdapter(const Memory::SmallVector<ComPtr<IDXGIAdapter1>, 4, Memory::Tag::Device>& adapters, const AdapterInfo* adapter_info)
	{
		for (auto& adapter : adapters)
		{
			DXGI_ADAPTER_DESC1 desc;
			static_assert(sizeof(desc.AdapterLuid) == sizeof(adapter_info->luid));
			if (FAILED(adapter->GetDesc1(&desc)))
				continue;
			
			if (memcmp(&desc.AdapterLuid, &adapter_info->luid, sizeof(desc.AdapterLuid)) == 0)
			{
				LOG_INFO(L"[D3D12] Found matching adapter using LUID " << adapter_info->luid << L" (" << adapter_info->display_name << L")");
				return adapter;
			}
		}

		for (auto& adapter : adapters)
		{
			DXGI_ADAPTER_DESC1 desc;
			if (FAILED(adapter->GetDesc1(&desc)))
				continue;

			if (adapter_info->display_name == desc.Description)
			{
				LOG_INFO(L"[D3D12] Found matching adapter using name (" << adapter_info->display_name << L")");
				return adapter;
			}
		}

		if (adapter_info->index < adapters.size())
		{
			LOG_WARN(L"[D3D12] Found physical device at #" << adapter_info->index << L" (" << adapter_info->display_name << L")");
			return adapters[adapter_info->index];
		}

		if (adapters.size() > 0)
		{
			std::wstring name = L"default";
			DXGI_ADAPTER_DESC1 desc;
			if (SUCCEEDED(adapters[0]->GetDesc1(&desc)))
				name = desc.Description;

			LOG_WARN(L"[D3D12] Failed to match adapter, falling back to #0 (" << name << L")");
			return adapters[0];
		}

		return nullptr;
	}

	bool CheckTearingSupport(IDXGIFactory1* factory)
	{
		ComPtr<IDXGIFactory6> factory6;
		bool result = false;

		if (SUCCEEDED(factory->QueryInterface(IID_PPV_ARGS(&factory6))))
		{
			BOOL allow_tearing = FALSE;
			auto hr = factory6->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allow_tearing, sizeof(allow_tearing));

			result = SUCCEEDED(hr) && allow_tearing;
		}

		return result;
	}

#endif

	UINT GetWaveLaneCount(ID3D12Device* device)
	{
		UINT result = 4;

		D3D12_FEATURE_DATA_D3D12_OPTIONS1 options;
		if (SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS1, &options, sizeof(options))))
		{
			result = options.WaveLaneCountMin;
		}

		return result;
	}

	simd::vector2_int GetSwapchainResolutionD3D12(DeviceSettings& device_settings, UINT Width, UINT Height, bool fullscreen, bool borderless)
	{
		WindowDX* window = WindowDX::GetWindow();
		if (window == nullptr)
			return { (int)Width, (int)Height };

#ifndef _XBOX_ONE
		if (!fullscreen)
		{
			if (borderless)
			{
				// Swapchain dimensions in borderless mode should always equals the monitor resolution
				window->GetMonitorDimensions(device_settings.AdapterOrdinal, &Width, &Height);
			}
			else
			{
				// Swapchain dimensions in windowed mode should always equals the client rectangle
				RECT rect;
				GetClientRect(window->GetWnd(), &rect);
				Width = rect.right - rect.left;
				Height = rect.bottom - rect.top;
			}
		}
		else
#endif
		if (Width == 0 || Height == 0)
		{
			Width = device_settings.BackBufferWidth;
			Height = device_settings.BackBufferHeight;
		}

		return { (int)Width, (int)Height };
	}

	std::pair<uint8_t*, size_t> IDeviceD3D12::AllocateFromConstantBuffer(size_t size)
	{
		return constant_buffer->Allocate(size, buffer_index);
	}

	IDeviceD3D12::IDeviceD3D12(DeviceInfo* _device_info, DeviceType device_type, HWND focus_window, const Renderer::Settings& renderer_settings)
		: IDevice(renderer_settings)
		, adapter_index(renderer_settings.adapter_index)
		, fullscreen(renderer_settings.fullscreen)
		, vsync(renderer_settings.vsync)
	{
		SetFullscreenMode(fullscreen);

		UINT dxgiFactoryFlags = 0;

#if !defined(_XBOX_ONE)

#if defined(ALLOW_DEBUG_LAYER)
		// Enable the debug layer (requires the Graphics Tools "optional feature").
		if (debug_layer)
		{
			ComPtr<ID3D12Debug> debugController;
			if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
			{
				debugController->EnableDebugLayer();
				// Enable additional debug layers.
				dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
			}
			else
			{
				LOG_CRIT(L"[D3D12] Failed enabling debug layer");
				debug_layer = false;
			}
		}
#endif
		
		if (!GetD3D12Functions().create_dxgi_factory2)
			throw ExceptionD3D12("CreateDXGIFactory2 function not found");

		if (FAILED(GetD3D12Functions().create_dxgi_factory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory))))
			throw ExceptionD3D12("CreateDXGIFactory2 failed");

		tearing_supported = CheckTearingSupport(factory.Get());

		auto selected_adapter = GetSelectedAdapter();
		auto adapters = GetDXGIAdapters(factory.Get());
		adapter = FindSelectedDXGIAdapter(adapters, selected_adapter);
		if (!adapter)
			throw ExceptionD3D12("Finding adapter failed");

		if (FAILED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_GRAPHICS_PPV_ARGS(device.GetAddressOf()))))
			throw ExceptionD3D12("D3D12CreateDevice failed");

#if defined(ALLOW_DEBUG_LAYER)
		ComPtr<ID3D12InfoQueue> info_queue;
		if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&info_queue))))
		{
			D3D12_MESSAGE_ID hide[] =
			{
				D3D12_MESSAGE_ID_CLEARDEPTHSTENCILVIEW_MISMATCHINGCLEARVALUE,
				D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
				D3D12_MESSAGE_ID_CREATEGRAPHICSPIPELINESTATE_RENDERTARGETVIEW_NOT_SET,
				D3D12_MESSAGE_ID_CREATEGRAPHICSPIPELINESTATE_DEPTHSTENCILVIEW_NOT_SET
			};

			D3D12_INFO_QUEUE_FILTER filter = {};
			filter.DenyList.NumIDs = (UINT)std::size(hide);
			filter.DenyList.pIDList = hide;
			info_queue->AddStorageFilterEntries(&filter);
		}
#endif
#else

		// Create the DX12 API device object.
		D3D12XBOX_CREATE_DEVICE_PARAMETERS params = {};
		params.Version = D3D12_SDK_VERSION;

#if defined(ALLOW_DEBUG_LAYER)
		if (debug_layer)
			params.ProcessDebugFlags = D3D12_PROCESS_DEBUG_FLAG_DEBUG_LAYER_ENABLED;
		else
			params.ProcessDebugFlags = D3D12XBOX_PROCESS_DEBUG_FLAG_INSTRUMENTED;
#endif
		params.ProcessDebugFlags |= D3D12XBOX_PROCESS_DEBUG_FLAG_ENABLE_COMMON_STATE_PROMOTION;

		params.GraphicsCommandQueueRingSizeBytes = static_cast<UINT>(D3D12XBOX_DEFAULT_SIZE_BYTES);
		params.GraphicsScratchMemorySizeBytes = static_cast<UINT>(D3D12XBOX_DEFAULT_SIZE_BYTES);
		params.ComputeScratchMemorySizeBytes = static_cast<UINT>(D3D12XBOX_DEFAULT_SIZE_BYTES);

		D3D12XboxSetProcessDebugFlags((D3D12XBOX_PROCESS_DEBUG_FLAGS)params.ProcessDebugFlags);

		if (FAILED(D3D12XboxCreateDevice(nullptr, &params, IID_GRAPHICS_PPV_ARGS(device.GetAddressOf()))))
			throw ExceptionD3D12("D3D12XboxCreateDevice failed");


		// First, retrieve the underlying DXGI device from the D3D device.
		ComPtr<IDXGIDevice1> dxgiDevice;
		if (FAILED(device.As(&dxgiDevice)))
			throw ExceptionD3D12("Failed getting IDXGIDevice1");

		// Identify the physical adapter (GPU or card) this device is running on.
		if (FAILED((dxgiDevice->GetAdapter(adapter.GetAddressOf()))))
			throw ExceptionD3D12("Failed getting IDXGIAdapter");


		// And obtain the factory object that created it.
		if (FAILED(adapter->GetParent(IID_GRAPHICS_PPV_ARGS(factory.GetAddressOf()))))
			throw ExceptionD3D12("Failed getting IDXGIFactory2");

#endif

		{
			DXGI_ADAPTER_DESC adapter_desc;
			if (SUCCEEDED(adapter->GetDesc(&adapter_desc)))
			{
				device_vram_size = adapter_desc.DedicatedVideoMemory;
				shared_vram_size = adapter_desc.SharedSystemMemory;
			}

			D3D12_FEATURE_DATA_ARCHITECTURE feature_architecture;
			if (SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_ARCHITECTURE, &feature_architecture, sizeof(feature_architecture))))
			{
				is_integrated_gpu = feature_architecture.UMA;
			}

			LOG_INFO(L"[D3D12] GPU type: " << (is_integrated_gpu ? L"integrated" : L"discrete"));

			total_vram = is_integrated_gpu
				? std::min(shared_vram_size, (uint64_t)4 * Memory::GB)
				: device_vram_size;

			LOG_DEBUG(L"[D3D12] VRAM size = " << (is_integrated_gpu ? Memory::ReadableSize(shared_vram_size).c_str() : Memory::ReadableSize(device_vram_size).c_str()));
			LOG_INFO(L"[D3D12] VRAM limit: " << Memory::ReadableSize(total_vram).c_str());

			UpdateVRAM();
		}

		wave_lane_count = GetWaveLaneCount(device.Get());

		allocator = Memory::Pointer<AllocatorD3D12::GPUAllocator, Memory::Tag::Device>::make(this);
		rendertarget_view_heap = Memory::Pointer<DescriptorHeapCacheD3D12, Memory::Tag::Device>::make(this, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 256);
		depthstencil_view_heap = Memory::Pointer<DescriptorHeapCacheD3D12, Memory::Tag::Device>::make(this, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 256);
		cbv_srv_uav_heap = Memory::Pointer<DescriptorHeapCacheD3D12, Memory::Tag::Device>::make(this, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 2048);
		sampler_heap = Memory::Pointer<DescriptorHeapCacheD3D12, Memory::Tag::Device>::make(this, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 32);
		sampler_dynamic_descriptor_heap = Memory::Pointer<ShaderVisibleDescriptorHeapD3D12, Memory::Tag::Device>::make(this, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 2048);

		dynamic_descriptor_heap = Memory::Pointer<ShaderVisibleRingBuffer, Memory::Tag::Device>::make(this, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, SHADER_VISIBLE_DESCRIPTOR_COUNT);
		constant_buffer = Memory::Pointer<AllocatorD3D12::ConstantBuffer, Memory::Tag::Device>::make("Global Constant Buffer", this, CONSTANT_BUFFER_MAX_SIZE_D3D12);

		idle_fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_GRAPHICS_PPV_ARGS(idle_fence.GetAddressOf()))))
			throw ExceptionD3D12("CreateFence failed");
		idle_fence_value = 1;

		// Describe and create the command queue.
		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		if (FAILED(device->CreateCommandQueue(&queueDesc, IID_GRAPHICS_PPV_ARGS(graphics_queue.GetAddressOf()))))
			throw ExceptionD3D12("CreateCommandQueue failed");

		UINT64 frequency;
		if (SUCCEEDED(graphics_queue->GetTimestampFrequency(&frequency)))
			time_stamp_frequency = (double)frequency;

		transfer = Memory::Pointer<TransferD3D12, Memory::Tag::Device>::make(this, GetTransferQueue());

		IDevice::Init(renderer_settings); // After VRAM init.

		// Describe and create a render target view (RTV) descriptor heap.
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
		rtvHeapDesc.NumDescriptors = FRAME_QUEUE_COUNT;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

		swap_chain = SwapChain::Create(focus_window, this, device_settings.BackBufferWidth, device_settings.BackBufferHeight, fullscreen, renderer_settings.vsync, 0);

		DynamicResolution::Get().Reset(GetBackBufferWidth(), GetBackBufferHeight());

		buffer_index = 0;

		// Create synchronization objects and wait until assets have been uploaded to the GPU.
		{
			if (FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_GRAPHICS_PPV_ARGS(fence.GetAddressOf()))))
				throw ExceptionD3D12("CreateFence failed");
			fence_value = 1;
			frame_fence_values.fill(0);

			// Create an event handle to use for frame synchronization.
			fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
			if (fence_event == nullptr)
				throw ExceptionD3D12("CreateEvent failed");
		}

		for (auto type : magic_enum::enum_values<RootSignatureD3D12Preset::SignatureType>())
		{
			auto& preset = RootSignatureD3D12Presets[type];
			root_signatures[type] = Memory::Pointer<RootSignatureD3D12, Memory::Tag::Device>::make(this, preset);
		}

		null_descriptor = GetCBViewSRViewUAViewDescriptorHeap().Allocate();
		D3D12_SHADER_RESOURCE_VIEW_DESC null_desk = {};
		null_desk.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		null_desk.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		null_desk.Buffer.NumElements = 0;
		null_desk.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		device->CreateShaderResourceView(nullptr, &null_desk, (D3D12_CPU_DESCRIPTOR_HANDLE)null_descriptor);
	}

	void IDeviceD3D12::DebugName(ID3D12Object* object, const char* name)
	{
	#if defined(ALLOW_DEBUG_LAYER)
		object->SetName(StringToWstring(name).c_str());
	#endif
	}

	IDeviceD3D12::~IDeviceD3D12()
	{
		WaitForIdle();

		SetFullscreenMode(false);

		CloseHandle(idle_fence_event);

		swap_chain.reset();
		IDevice::Quit();

		null_descriptor = DescriptorHeapD3D12::Handle();
		transfer.reset();
		constant_buffer.reset();
		allocator.reset();
		dynamic_descriptor_heap.reset();
		rendertarget_view_heap.reset();
		depthstencil_view_heap.reset();
		cbv_srv_uav_heap.reset();
		device = nullptr;
	}

	void IDeviceD3D12::Suspend()
	{
#if defined(_XBOX_ONE)
		LOG_DEBUG(L"IDeviceD3D12::Suspend()");
		graphics_queue->SuspendX(0);
#endif
	}

	void IDeviceD3D12::Resume()
	{
#if defined(_XBOX_ONE)
		LOG_DEBUG(L"IDeviceD3D12::Resume()");
		graphics_queue->ResumeX();
#endif
	}

	void IDeviceD3D12::UpdateVRAM()
	{
		current_vram_usage = 0;
		vram_budget = 0;
#if !defined(_XBOX_ONE)
		ComPtr<IDXGIAdapter3> adapter3;
		if (SUCCEEDED(adapter->QueryInterface(IID_PPV_ARGS(&adapter3))))
		{
			DXGI_QUERY_VIDEO_MEMORY_INFO info = {};
			if (SUCCEEDED(adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &info)))
			{
				current_vram_usage = info.CurrentUsage;
				vram_budget = info.Budget;
			}
		}
#endif
	}

	CD3DX12_GPU_DESCRIPTOR_HANDLE IDeviceD3D12::GetSamplersGPUAddress() const
	{
		return GetSamplerDynamicDescriptorHeap().GetGPUAddress(0);
	}

	void IDeviceD3D12::ReinitGlobalSamplers()
	{
		IDevice::ReinitGlobalSamplers();
		
		auto& samplers = GetSamplers();
		const uint32_t descriptor_count = (uint32_t)samplers.size();
		auto& sampler_heap = GetSamplerDynamicDescriptorHeap();
		Memory::SmallVector<D3D12_CPU_DESCRIPTOR_HANDLE, 16, Memory::Tag::Device> src_descriptors;
		for (auto& sampler : samplers)
		{
			src_descriptors.push_back(static_cast<SamplerD3D12*>(sampler.Get())->GetDescriptor());
		}

		const Memory::SmallVector<uint32_t, 16, Memory::Tag::Device> count_array(descriptor_count, 1);
		device->CopyDescriptors(1, &sampler_heap.GetCPUAddress(0), &descriptor_count, descriptor_count, src_descriptors.data(), count_array.data(), D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
	}

	void IDeviceD3D12::Swap()
	{
		IDevice::Swap();
		UpdateVRAM();
	}

	void IDeviceD3D12::WaitForIdle()
	{
		Memory::Lock lock(idle_mutex);

		transfer->Flush();
		transfer->Wait();

		const auto previous_fence_value = idle_fence_value;
		idle_fence_value++;

		if (FAILED(graphics_queue->Signal(idle_fence.Get(), previous_fence_value)))
			throw ExceptionD3D12("graphics_queue->Signal failed");

		// Wait until the previous frame is finished.
		if (idle_fence->GetCompletedValue() < previous_fence_value)
		{
			if (FAILED(idle_fence->SetEventOnCompletion(previous_fence_value, idle_fence_event)))
				throw ExceptionD3D12("fence->SetEventOnCompletion failed");

			WaitForSingleObject(idle_fence_event, INFINITE);
		}
	}

	void IDeviceD3D12::RecreateSwapChain(HWND hwnd, UINT Output, const Renderer::Settings& renderer_settings)
	{
		WaitForIdle();
		swap_chain->Recreate(hwnd, this, renderer_settings.resolution.width, renderer_settings.resolution.height, renderer_settings.fullscreen, renderer_settings.vsync, Output);
		ResizeDXGIBuffers(renderer_settings.resolution.width, renderer_settings.resolution.height, renderer_settings.fullscreen);
		fullscreen = renderer_settings.fullscreen;
		SetFullscreenMode(fullscreen);
	}

	bool IDeviceD3D12::CheckFullScreenFailed()
	{
#ifdef _XBOX_ONE
		return false;
#else
		return fullscreen_failed;
#endif
	}

	bool IDeviceD3D12::IsWindowed() const
	{
		return !fullscreen;
	}

	bool IDeviceD3D12::SupportsSubPasses() const
	{
		return false;
	}

	bool IDeviceD3D12::SupportsParallelization() const
	{
		return true;
	}

	bool IDeviceD3D12::SupportsCommandBufferProfile() const
	{
		return true;
	}

	unsigned IDeviceD3D12::GetWavefrontSize() const
	{
		return (unsigned)wave_lane_count;
	}

	VRAM IDeviceD3D12::GetVRAM() const
	{
		VRAM vram;
		vram.used = std::min(allocator->GetVRAM().used, MAX_VRAM_USAGE_D3D12);
		vram.reserved = vram.used;

	#if defined(_XBOX_ONE)
		vram.available = Memory::GetMaxVRAM();
		vram.total = Memory::GetMaxVRAM();
	#else
		if (!is_integrated_gpu && vram_budget) vram.available = std::min(vram_budget, MAX_VRAM_USAGE_D3D12);
		else vram.available = std::min(uint64_t((total_vram * 7) / 10), MAX_VRAM_USAGE_D3D12);
		vram.total = total_vram;
	#endif
		return vram;
	}

	void IDeviceD3D12::SetFullscreenMode(bool is_fullscreen)
	{
		if (auto* window = WindowDX::GetWindow())
		{
			if (is_fullscreen)
			{
				if (!window->SetDisplayResolution(this, device_settings.BackBufferWidth, device_settings.BackBufferHeight))
					fullscreen_failed = true;
			}
			else
				window->RestoreDisplayResolution();
		}
	}

	void IDeviceD3D12::BeginFlush()
	{
		transfer->Flush();
	}

	void IDeviceD3D12::Flush(CommandBuffer& command_buffer)
	{
		frame_cmd_lists.push_back(static_cast<CommandBufferD3D12&>(command_buffer).GetCommandList());
	}

	void IDeviceD3D12::EndFlush()
	{
	}

	void IDeviceD3D12::BeginFrame( )
	{
		PROFILE;

		WaitFence();

		PROFILE_ONLY(allocator->stats.GlobalConstantsSize(constant_buffer->GetSize(), constant_buffer->GetMaxSize());)
		constant_buffer->Reset();
	}

	void IDeviceD3D12::EndFrame( )
	{
	}

	void IDeviceD3D12::WaitForBackbuffer(CommandBuffer& command_buffer)
	{
		static_cast<SwapChainD3D12*>(swap_chain.get())->WaitForBackbuffer();
	}

	void IDeviceD3D12::WaitFence()
	{
		PROFILE;

		Timer timer;
		timer.Start();

		const auto next_frame_fence_value = frame_fence_values[buffer_index];

		// Wait until the previous frame is finished.
		if (next_frame_fence_value && fence->GetCompletedValue() < next_frame_fence_value)
		{
			if (FAILED(fence->SetEventOnCompletion(next_frame_fence_value, fence_event)))
				throw ExceptionD3D12("fence->SetEventOnCompletion failed");
			WaitForSingleObject(fence_event, INFINITE);
		}
		
#if !defined(_XBOX_ONE)
		static_cast<SwapChainD3D12*>(swap_chain.get())->UpdateBufferIndex();
#endif

		fence_wait_time = (float)timer.GetElapsedTime();
	}

	void IDeviceD3D12::Submit()
	{
		if (frame_cmd_lists.size())
		{
			Memory::Lock lock(transfer->GetMutex());
			graphics_queue->ExecuteCommandLists((UINT)frame_cmd_lists.size(), frame_cmd_lists.data());
			frame_cmd_lists.clear();
		}
		
		// Signal and increment the fence value.
		if (FAILED(graphics_queue->Signal(fence.Get(), fence_value)))
			throw ExceptionD3D12("graphics_queue->Signal failed");

		frame_fence_values[buffer_index] = fence_value;
		fence_value++;
	}

	void IDeviceD3D12::Present( const Rect* pSourceRect, const Rect* pDestRect, HWND hDestWindowOverride )
	{
		if (!swap_chain->Present())
			throw ExceptionD3D12("swap_chain->Present() failed");
#if defined(_XBOX_ONE)
		static_cast<SwapChainD3D12*>(swap_chain.get())->UpdateBufferIndex();
#endif
		buffer_index = (buffer_index + 1) % FRAME_QUEUE_COUNT;
	}

	HRESULT IDeviceD3D12::CheckForDXGIBufferChange()
	{
		HRESULT hr = S_OK;
		if (swap_chain != nullptr)
		{
			WindowDX* window = WindowDX::GetWindow();
			if (window)
			{
#ifndef _XBOX_ONE
				if (device_settings.Windowed > 0)
				{
					//get the correct rect depending if we are in window mode or fullscreen mode
					RECT rc_current_client;
					HWND hwnd = window->GetWnd();
					GetClientRect(hwnd, &rc_current_client);

					if ((GetBackBufferWidth() != rc_current_client.right) ||
						(GetBackBufferHeight() != rc_current_client.bottom))
					{
						hr = ResizeDXGIBuffers(rc_current_client.right, rc_current_client.bottom, fullscreen);
					}

					UINT flag = window->IsMaximized() ? SW_SHOWMAXIMIZED : SW_SHOW;
					window->ShowWindow(flag);
				}
#else
				hr = ResizeDXGIBuffers(0, 0, fullscreen);
#endif
			}
		}

		return hr;
	}

	HRESULT IDeviceD3D12::ResizeDXGIBuffers( UINT Width, UINT Height, BOOL bFullScreen, BOOL bBorderless /*=false*/ )
	{
		HRESULT hr = S_OK;
		WindowDX* window = WindowDX::GetWindow();
		if (window == nullptr)
			return hr;

		const auto size = GetSwapchainResolutionD3D12(device_settings, Width, Height, bFullScreen, bBorderless);
		Width = size.x;
		Height = size.y;

		device_settings.BackBufferWidth = Width;
		device_settings.BackBufferHeight = Height;

		WaitForIdle();

		buffer_index = (buffer_index + 1) % FRAME_QUEUE_COUNT;

		window->TriggerDeviceLost();

#ifdef _XBOX_ONE
		releaser->Clear();
#endif

		UINT Flags = static_cast<SwapChainD3D12*>(swap_chain.get())->GetFlags(bFullScreen);
		hr = static_cast<SwapChainD3D12*>(swap_chain.get())->ResizeBuffers(Width, Height, Flags);
#ifndef _XBOX_ONE
		if (FAILED(hr))
		{
			LOG_CRIT(L"Failed to resize swap chain, possibly during windowed <-> fullscreen toggling");
			return hr;
		}
#endif

		DynamicResolution::Get().Reset(GetBackBufferWidth(), GetBackBufferHeight());

		window->TriggerDeviceReset(this);

		Pause(false, false);

		if (FAILED(hr))
		{
#ifndef _XBOX_ONE
			PostQuitMessage(0);
#endif
		}

		return hr;
	}

	void IDeviceD3D12::ResourceFlush()
	{
		WaitForIdle();

		releaser->Clear();

		transfer->Flush();
	}
	
	void IDeviceD3D12::BeginEvent( CommandBuffer& command_buffer, const std::wstring& text )
	{
#if defined(ALLOW_DEBUG_LAYER)
		Memory::Lock lock(markers_mutex);
		const auto marker = WstringToString(text);
		auto found = markers.insert(marker);
		
#if !defined(_XBOX_ONE)
		static_cast<CommandBufferD3D12&>(command_buffer).GetCommandList()->BeginEvent(1, found.first->data(), (uint32_t)found.first->length() + 1);
#else
		PIXBeginEvent(static_cast<CommandBufferD3D12&>(command_buffer).GetCommandList(), 0, found.first->data());
#endif
#endif
	}
	
	void IDeviceD3D12::EndEvent(CommandBuffer& command_buffer)
	{
#if defined(ALLOW_DEBUG_LAYER)
#if !defined(_XBOX_ONE)
		static_cast<CommandBufferD3D12&>(command_buffer).GetCommandList()->EndEvent();
#else
		PIXEndEvent(static_cast<CommandBufferD3D12&>(command_buffer).GetCommandList());
#endif
#endif
	}
	
	void IDeviceD3D12::SetMarker(CommandBuffer& command_buffer, const std::wstring& text)
	{
#if defined(ALLOW_DEBUG_LAYER)
		Memory::Lock lock(markers_mutex);
		const auto marker = WstringToString(text);
		auto found = markers.insert(marker);
#if !defined(_XBOX_ONE)
		static_cast<CommandBufferD3D12&>(command_buffer).GetCommandList()->SetMarker(1, found.first->data(), (uint32_t)found.first->length() + 1);
#else
		PIXSetMarker(static_cast<CommandBufferD3D12&>(command_buffer).GetCommandList(), 0, found.first->data());
#endif
#endif
	}

	void IDeviceD3D12::SetVSync(bool enabled) 
	{
		vsync = enabled;
		if (swap_chain)
			((SwapChainD3D12*)swap_chain.get())->SetVSync(enabled);
	}
	bool IDeviceD3D12::GetVSync() { return vsync; }

	PROFILE_ONLY(std::vector<std::vector<std::string>> IDeviceD3D12::GetStats() { auto strings = allocator->GetStats(); GetDynamicDescriptorHeap().AppendStats(strings); return strings; })
	PROFILE_ONLY(size_t IDeviceD3D12::GetSize() { return allocator->GetSize(); })
}
