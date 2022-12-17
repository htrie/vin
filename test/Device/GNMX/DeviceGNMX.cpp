
namespace Device
{
	constexpr size_t CONSTANT_BUFFER_MAX_SIZE = 2 * Memory::MB;

	extern void RenderTargetWait(Gnmx::LightweightGfxContext& gfx, RenderTarget& target);
	extern void RenderTargetSwap(RenderTarget& target);

	class IDeviceGNMX : public IDevice
	{
		class Fence
		{
			SceKernelEqueue eop_equeue;

			AllocationGNMX eop_label;
			AllocationGNMX eocbp_label;

			volatile uint64_t* eop_label_addr = nullptr;
			volatile uint64_t* eocbp_label_addr = nullptr;

		public:
			enum Event
			{
				Unsignaled,
				Signaled,
			};

			void Init();
			void Deinit();

			void Wait();

			void* GetDCBLabelAddr() const { return (void*)eop_label_addr; }
			void* GetFlipLabelAddr() const { return (void*)eocbp_label_addr; }
		};

		std::unique_ptr<SwapChain> swap_chain;
		std::array<Fence, BackBufferCount> cb_fences;

		int32_t video_out_handle;
		SceKernelEqueue video_out_equeue;

		std::array<Gnmx::LightweightGfxContext, BackBufferCount> gfx_contexts;
		std::array<AllocationGNMX, BackBufferCount> dcr_buffer_mem;

		Memory::Pointer<AllocatorGNMX::ConstantBuffer, Memory::Tag::Device> constant_buffer;

		bool fullscreen;

		Timer flush_timer;
		float flush_wait_time = 0;
		float cb_wait_time = 0;
		float backbuffer_wait_time = 0;

		unsigned width;
		unsigned height;

		Memory::FlatSet<std::string, Memory::Tag::Profile> markers;
		Memory::Mutex markers_mutex;

	public:
		IDeviceGNMX(DeviceInfo* device_info, DeviceType device_type, HWND focus_window, const Renderer::Settings& renderer_settings);
		virtual ~IDeviceGNMX();

		virtual void Suspend() final;
		virtual void Resume() final;

		virtual void WaitForIdle() final;

		virtual void RecreateSwapChain(HWND hwnd, UINT Output, const Renderer::Settings& renderer_settings) final;

		virtual void BeginEvent(CommandBuffer& command_buffer, const std::wstring& text) final;
		virtual void EndEvent(CommandBuffer& command_buffer) final;
		virtual void SetMarker(CommandBuffer& command_buffer, const std::wstring& text) final;

		virtual bool SupportsSubPasses() const final;
		virtual bool SupportsParallelization() const final;
		virtual bool SupportsCommandBufferProfile() const final;
		virtual unsigned GetWavefrontSize() const final;

		virtual VRAM GetVRAM() const override final;

		virtual HRESULT GetBackBufferDesc(UINT iSwapChain, UINT iBackBuffer, SurfaceDesc* pBBufferSurfaceDesc) final;

		virtual void BeginFlush() final;
		virtual void Flush(CommandBuffer& command_buffer) final;
		virtual void EndFlush() final;

		virtual void BeginFrame() final;
		virtual void EndFrame() final;

		virtual void WaitForBackbuffer(CommandBuffer& command_buffer) final;
		virtual void Submit() final;
		virtual void Present(const Rect* pSourceRect, const Rect* pDestRect, HWND hDestWindowOverride) final;

		virtual void ResourceFlush() final;

		virtual void SetVSync(bool enabled) final;
		virtual bool GetVSync() final;

		virtual HRESULT CheckForDXGIBufferChange() final;
		virtual HRESULT ResizeDXGIBuffers(UINT Width, UINT Height, BOOL bFullScreen, BOOL bBorderless = false) final;

		virtual bool IsWindowed() const final;
		virtual float GetFrameWaitTime() const final { return cb_wait_time + backbuffer_wait_time + flush_wait_time; }
		virtual void Swap() override final;

		virtual SwapChain* GetMainSwapChain() const final { return swap_chain.get(); }

		PROFILE_ONLY(virtual std::vector<std::vector<std::string>> GetStats() final;)
		PROFILE_ONLY(virtual size_t GetSize() final;)

		unsigned GetWidth() const { return width; }
		unsigned GetHeight() const { return height; }

		std::pair<uint8_t*, size_t> AllocateFromConstantBuffer(size_t size);

		Gnmx::LightweightGfxContext& GetGfxContext() { return gfx_contexts[current_back_buffer]; }
		int32_t GetVideoHandle() const { return video_out_handle; }
	};

	void IDeviceGNMX::Fence::Init()
	{
		int32_t ret = sceKernelCreateEqueue(&eop_equeue, "EOP queue");
		if (ret < 0)
			throw ExceptionGNMX("Failed to create EOP queue");

		ret = Gnm::addEqEvent(eop_equeue, Gnm::kEqEventGfxEop, NULL);
		if (ret < 0)
			throw ExceptionGNMX("Failed to add EOP event");

		eop_label.Init(Memory::Tag::Device, Memory::Type::CPU_WB, 8, 8);
		eocbp_label.Init(Memory::Tag::Device, Memory::Type::CPU_WB, 8, 8);

		eop_label_addr = (uint64_t*)eop_label.GetData();
		eocbp_label_addr = (uint64_t*)eocbp_label.GetData();

		*eop_label_addr = Event::Signaled;
		*eocbp_label_addr = Event::Signaled;
	}

	void IDeviceGNMX::Fence::Deinit()
	{
		sceKernelDeleteEqueue(eop_equeue);
	}

	void IDeviceGNMX::Fence::Wait()
	{
		while (*eop_label_addr != Event::Signaled)
		{
			SceKernelEvent event;
			int count;
			auto ret = sceKernelWaitEqueue(eop_equeue, &event, 1, &count, NULL);
			if (ret < 0)
				throw ExceptionGNMX("Failed to wait for EOP event");
		}

		while (*eocbp_label_addr != Event::Signaled);

		*eop_label_addr = Event::Unsignaled;
		*eocbp_label_addr = Event::Unsignaled;
	}

	IDeviceGNMX::IDeviceGNMX(DeviceInfo* device_info, DeviceType device_type, HWND focus_window, const Renderer::Settings& renderer_settings)
		: IDevice(renderer_settings)
		, fullscreen(renderer_settings.fullscreen)
		, width(renderer_settings.resolution.width)
		, height(renderer_settings.resolution.height)
	{
		video_out_handle = sceVideoOutOpen(0, SCE_VIDEO_OUT_BUS_TYPE_MAIN, 0, NULL);
		if (video_out_handle < 0)
			throw ExceptionGNMX("Failed to open video out");

		SceVideoOutResolutionStatus status;
		int32_t ret = sceVideoOutGetResolutionStatus(video_out_handle, &status);
		if (ret < 0)
			throw ExceptionGNMX("Failed to get resolution status");

		DynamicResolution::Get().Reset((float)width, (float)height);

		ret = sceKernelCreateEqueue(&video_out_equeue, "flip queue");
		if (ret < 0)
			throw ExceptionGNMX("Failed to create flip queue");

		ret = sceVideoOutAddFlipEvent(video_out_equeue, video_out_handle, NULL);
		if (ret < 0)
			throw ExceptionGNMX("Failed to add flip event");

		IDevice::Init(renderer_settings);
		::Device::Init(this); // Loading the clear_shader

		constant_buffer = Memory::Pointer<AllocatorGNMX::ConstantBuffer, Memory::Tag::Device>::make("Global Constant Buffer", this, CONSTANT_BUFFER_MAX_SIZE);

		for (unsigned i = 0; i < BackBufferCount; i++)
		{
			const size_t CommandBufferSize = 512;
			dcr_buffer_mem[i].Init(Memory::Tag::CommandBuffer, Memory::Type::CPU_WB, CommandBufferSize, Gnm::kAlignmentOfBufferInBytes);

			gfx_contexts[i].init(dcr_buffer_mem[i].GetData(), CommandBufferSize, nullptr, 0, nullptr);
			gfx_contexts[i].setResourceBufferFullCallback(nullptr, this);

			cb_fences[i].Init();
		}

		swap_chain = SwapChain::Create(focus_window, this, width, height, fullscreen, renderer_settings.vsync, 0);
	}

	IDeviceGNMX::~IDeviceGNMX()
	{
		WaitForIdle();

		for (unsigned i = 0; i < BackBufferCount; i++)
		{
			cb_fences[i].Deinit();
		}

		constant_buffer.reset();

		sceKernelDeleteEqueue(video_out_equeue);

		sceVideoOutClose(video_out_handle);

		sceSysmoduleUnloadModule(SCE_SYSMODULE_PNG_DEC);

		IDevice::Quit();
	}

	void IDeviceGNMX::BeginFrame()
	{
		{
			Timer timer;
			timer.Start();

			cb_fences[current_back_buffer].Wait();

			cb_wait_time = timer.GetElapsedTime();
		}

		::Device::Prepare();

		constant_buffer->Reset();
	}

	void IDeviceGNMX::EndFrame()
	{
		
	}

	void IDeviceGNMX::WaitForBackbuffer(CommandBuffer& command_buffer)
	{
		Timer timer;
		timer.Start();

		{
			const auto meta_pass_index = GetProfile()->BeginPass(command_buffer, Job2::Profile::PassVSync); // Measure VSync to remove it when evaluation dynamic resolution.

			static_cast<CommandBufferGNMX&>(command_buffer).GetGfxContext().waitUntilSafeForRendering(video_out_handle, current_back_buffer);

			GetProfile()->EndPass(command_buffer, meta_pass_index);
		}

		backbuffer_wait_time = timer.GetElapsedTime();
	}

	void IDeviceGNMX::Submit()
	{
	}

	void IDeviceGNMX::Present(const Rect* pSourceRect, const Rect* pDestRect, HWND hDestWindowOverride)
	{
		auto& gfx = GetGfxContext();

		gfx.reset();

		gfx.writeAtEndOfPipeWithInterrupt(Gnm::kEopFlushCbDbCaches, Gnm::kEventWriteDestMemory, (void*)cb_fences[current_back_buffer].GetDCBLabelAddr(), Gnm::kEventWriteSource64BitsImmediate, Fence::Event::Signaled, Gnm::kCacheActionNone, Gnm::kCachePolicyLru);

	#if defined(ALLOW_DEBUG_LAYER)
		if (debug_layer)
			gfx.validate();
	#endif

		{
			swap_chain->Present();

			auto res = gfx.submitAndFlip(video_out_handle, current_back_buffer, SCE_VIDEO_OUT_FLIP_MODE_WINDOW_2, 0, (void*)cb_fences[current_back_buffer].GetFlipLabelAddr(), Fence::Event::Signaled);
			if (res != SCE_OK)
				throw ExceptionGNMX("Failed to submit and flip");

			Gnm::submitDone();
		}

		current_back_buffer = (current_back_buffer + 1) % BackBufferCount;
	}

	void IDeviceGNMX::Suspend()
	{
	}

	void IDeviceGNMX::Resume()
	{
	}

	void IDeviceGNMX::WaitForIdle()
	{
	}

	void IDeviceGNMX::RecreateSwapChain(HWND hwnd, UINT Output, const Renderer::Settings& renderer_settings)
	{
	}

	bool IDeviceGNMX::IsWindowed() const
	{
		return false;
	}

	void IDeviceGNMX::Swap()
	{
		IDevice::Swap();

		markers.clear();
	}

	std::pair<uint8_t*, size_t> IDeviceGNMX::AllocateFromConstantBuffer(size_t size)
	{
		return constant_buffer->Allocate(size, current_back_buffer);
	}

	HRESULT IDeviceGNMX::GetBackBufferDesc( UINT iSwapChain, UINT iBackBuffer, SurfaceDesc* pBBufferSurfaceDesc )
	{
		memset(pBBufferSurfaceDesc, 0, sizeof(SurfaceDesc));
		pBBufferSurfaceDesc->Width = width;
		pBBufferSurfaceDesc->Height = height;
		pBBufferSurfaceDesc->Format = PixelFormat::A8R8G8B8;
		return S_OK;
	}

	bool IDeviceGNMX::SupportsSubPasses() const
	{
		return true;
	}

	bool IDeviceGNMX::SupportsParallelization() const
	{
		return true;
	}

	bool IDeviceGNMX::SupportsCommandBufferProfile() const
	{
		return true;
	}

	unsigned IDeviceGNMX::GetWavefrontSize() const
	{
		return 64;
	}

	VRAM IDeviceGNMX::GetVRAM() const
	{
		VRAM vram;
		vram.used = Memory::GetUsedVRAM();
		vram.reserved = vram.used;
		vram.available = Memory::GetMaxVRAM();
		vram.total = Memory::GetMaxVRAM();
		return vram;
	}

	void IDeviceGNMX::BeginFlush()
	{
		flush_timer.Start();
	}

	void IDeviceGNMX::Flush(CommandBuffer& command_buffer)
	{
		static_cast<CommandBufferGNMX&>(command_buffer).Flush();
	}

	void IDeviceGNMX::EndFlush()
	{
		flush_wait_time = flush_timer.GetElapsedTime();
	}

	void IDeviceGNMX::ResourceFlush()
	{
	}

	HRESULT IDeviceGNMX::CheckForDXGIBufferChange()
	{
		return S_OK;
	}

	HRESULT IDeviceGNMX::ResizeDXGIBuffers( UINT Width, UINT Height, BOOL bFullScreen, BOOL bBorderless /*=false*/ )
	{
		return S_OK;
	}
	
	void IDeviceGNMX::BeginEvent(CommandBuffer& command_buffer, const std::wstring& text)
	{
	#if defined(ALLOW_DEBUG_LAYER)
		if (debug_layer)
		{
			Memory::Lock lock(markers_mutex);
			const auto marker = WstringToString(text);
			auto found = markers.insert(marker);
			static_cast<CommandBufferGNMX&>(command_buffer).GetGfxContext().pushMarker(found.first->data());
		}
	#endif
	}
	
	void IDeviceGNMX::EndEvent(CommandBuffer& command_buffer)
	{
	#if defined(ALLOW_DEBUG_LAYER)
		if (debug_layer)
			static_cast<CommandBufferGNMX&>(command_buffer).GetGfxContext().popMarker();
	#endif
	}
	
	void IDeviceGNMX::SetMarker(CommandBuffer& command_buffer, const std::wstring& text)
	{
	#if defined(ALLOW_DEBUG_LAYER)
		if (debug_layer)
		{
			Memory::Lock lock(markers_mutex);
			const auto marker = WstringToString(text);
			auto found = markers.insert(marker);
			static_cast<CommandBufferGNMX&>(command_buffer).GetGfxContext().setMarker(found.first->data());
		}
	#endif
	}

	void IDeviceGNMX::SetVSync(bool enabled) {}
	bool IDeviceGNMX::GetVSync() { return true; }

	PROFILE_ONLY(std::vector<std::vector<std::string>> IDeviceGNMX::GetStats() { return std::vector<std::vector<std::string>>(); })
	PROFILE_ONLY(size_t IDeviceGNMX::GetSize() { return 0; })

}
