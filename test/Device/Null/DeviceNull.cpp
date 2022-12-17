
namespace Device
{

	class IDeviceNull : public IDevice
	{
		std::unique_ptr<SwapChain> swap_chain;

	public:
		IDeviceNull(DeviceInfo* _device_info, DeviceType device_type, HWND focus_window, const Renderer::Settings& renderer_settings);
		virtual ~IDeviceNull();

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

		virtual SwapChain* GetMainSwapChain() const final;
		virtual float GetFrameWaitTime() const final { return 0; }

		PROFILE_ONLY(virtual std::vector<std::vector<std::string>> GetStats() final;)
		PROFILE_ONLY(virtual size_t GetSize() final;)

		bool fullscreen;

		unsigned width;
		unsigned height;
	};

	IDeviceNull::IDeviceNull(DeviceInfo* _device_info, DeviceType device_type, HWND focus_window, const Renderer::Settings& renderer_settings)
		: IDevice(renderer_settings)
		, fullscreen(renderer_settings.fullscreen)
		, width(renderer_settings.resolution.width)
		, height(renderer_settings.resolution.height)
	{
		IDevice::Init(renderer_settings);

		swap_chain = SwapChain::Create(focus_window, this, width, height, fullscreen, renderer_settings.vsync, 0);

		DynamicResolution::Get().Reset(width, height);
	}

	IDeviceNull::~IDeviceNull()
	{
		WaitForIdle();

		IDevice::Quit();
	}

	void IDeviceNull::Suspend()
	{
	}

	void IDeviceNull::Resume()
	{
	}

	void IDeviceNull::WaitForIdle()
	{
	}

	void IDeviceNull::RecreateSwapChain(HWND hwnd, UINT Output, const Renderer::Settings& renderer_settings)
	{
	}

	bool IDeviceNull::IsWindowed() const
	{
		return false;
	}

	SwapChain* IDeviceNull::GetMainSwapChain() const
	{
		return swap_chain.get();
	}

	HRESULT IDeviceNull::GetBackBufferDesc( UINT iSwapChain, UINT iBackBuffer, SurfaceDesc* pBBufferSurfaceDesc )
	{
		memset(pBBufferSurfaceDesc, 0, sizeof(SurfaceDesc));
		pBBufferSurfaceDesc->Width = width;
		pBBufferSurfaceDesc->Height = height;
		pBBufferSurfaceDesc->Format = PixelFormat::A8R8G8B8;
		return S_OK;
	}

	bool IDeviceNull::SupportsSubPasses() const
	{
		return true;
	}

	bool IDeviceNull::SupportsParallelization() const
	{
		return true;
	}

	bool IDeviceNull::SupportsCommandBufferProfile() const
	{
		return true;
	}

	unsigned IDeviceNull::GetWavefrontSize() const
	{
		return 4;
	}

	VRAM IDeviceNull::GetVRAM() const
	{
		VRAM vram;
		vram.used = 0;
		vram.reserved = vram.used;
		vram.available = 4 * Memory::GB; // Keep under max allowed usage (8 GB) to avoid streaming system soft locking.
		vram.total = 4 * Memory::GB;
		return vram;
	}

	void IDeviceNull::BeginFlush()
	{
	}

	void IDeviceNull::Flush(CommandBuffer& command_buffer)
	{
	}

	void IDeviceNull::EndFlush()
	{
	}

	void IDeviceNull::BeginFrame( )
	{
		
	}

	void IDeviceNull::EndFrame( )
	{
		
	}

	void IDeviceNull::WaitForBackbuffer(CommandBuffer& command_buffer)
	{
	}

	void IDeviceNull::Submit()
	{
	}

	void IDeviceNull::Present( const Rect* pSourceRect, const Rect* pDestRect, HWND hDestWindowOverride )
	{
	}

	HRESULT IDeviceNull::CheckForDXGIBufferChange()
	{
		return S_OK;
	}

	HRESULT IDeviceNull::ResizeDXGIBuffers( UINT Width, UINT Height, BOOL bFullScreen, BOOL bBorderless /*=false*/ )
	{
		return S_OK;
	}

	void IDeviceNull::ResourceFlush()
	{
	}
	
	void IDeviceNull::BeginEvent(CommandBuffer& command_buffer, const std::wstring& text)
	{
	}
	
	void IDeviceNull::EndEvent(CommandBuffer& command_buffer)
	{
	}
	
	void IDeviceNull::SetMarker(CommandBuffer& command_buffer, const std::wstring& text)
	{
	}

	void IDeviceNull::SetVSync(bool enabled) {}
	bool IDeviceNull::GetVSync() { return true; }

	PROFILE_ONLY(std::vector<std::vector<std::string>> IDeviceNull::GetStats() { return std::vector<std::vector<std::string>>(); })
	PROFILE_ONLY(size_t IDeviceNull::GetSize() { return 0; })

}
