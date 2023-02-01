
namespace Device
{

	enum class DeviceRuntimeVersion
	{
		RUNTIME_VERSION_NONE = 0,
		RUNTIME_VERSION_11_0,
		RUNTIME_VERSION_11_1,
	};

	enum class DeviceFeatureLevel
	{
		FEATURE_LEVEL_NONE = 0,
		FEATURE_LEVEL_10_0,
		FEATURE_LEVEL_10_1,
		FEATURE_LEVEL_11_0,
		FEATURE_LEVEL_11_1,
	};

	DXGI_FORMAT SWAP_CHAIN_FORMAT = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

	class IDeviceD3D11 : public IDevice
	{
	public:
		static constexpr unsigned FRAME_QUEUE_COUNT = 2;

	private:
		std::unique_ptr<ID3D11Device, Utility::GraphicsComReleaser> _device;
		std::unique_ptr<ID3D11DeviceContext, Utility::GraphicsComReleaser> _context;
		std::unique_ptr<ID3DUserDefinedAnnotation, Utility::GraphicsComReleaser> annotation;

		ConstantBufferD3D11 constant_buffer;
		Memory::Mutex constant_buffer_mutex;

		DeviceRuntimeVersion runtime_version;
		DeviceFeatureLevel feature_level;

		UINT _adapter_ordinal;

		bool fullscreen = false;
		bool vsync = false;
		bool driver_command_lists = false;
		bool driver_concurrent_creates = false;

		std::atomic<bool> deferred_context_used = false;

		Memory::FlatSet<std::wstring, Memory::Tag::Profile> markers;

	#if !defined(_XBOX_ONE)
		ID3D11Debug* d3dDebug = nullptr;
		uint64_t available_vram = 0;
	#endif

	public:
		IDeviceD3D11(DeviceInfo* device_info, DeviceType device_type, HWND focus_window, const Renderer::Settings& renderer_settings);
		virtual ~IDeviceD3D11();

		virtual void Suspend() final;
		virtual void Resume() final;

		virtual void WaitForIdle() final;

		virtual void RecreateSwapChain(HWND hwnd, UINT Output, const Renderer::Settings& renderer_settings) final;

		virtual bool CheckFullScreenFailed() final;

		virtual void BeginEvent(CommandBuffer& command_buffer, const std::wstring& text) final;
		virtual void EndEvent(CommandBuffer& command_buffer) final;
		virtual void SetMarker(CommandBuffer& command_buffer, const std::wstring& text) final;

		virtual bool SupportsSubPasses() const final;
		virtual bool SupportsParallelization() const final;
		virtual bool SupportsCommandBufferProfile() const final;
		virtual unsigned GetWavefrontSize() const final;

		virtual VRAM GetVRAM() const override final;

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

		virtual float GetFrameWaitTime() const final { return 0; }

		PROFILE_ONLY(virtual std::vector<std::vector<std::string>> GetStats() final;)
		PROFILE_ONLY(virtual size_t GetSize() final;)

		void Swap() override final;

		ConstantBufferD3D11::Buffer& LockFromConstantBuffer(size_t size, ID3D11Buffer*& out_buffer, uint8_t*& out_map);
		void UnlockFromConstantBuffer(ConstantBufferD3D11::Buffer& buffer);
		uint8_t* AllocateFromConstantBuffer(const size_t size, ID3D11Buffer*& out_buffer, UINT& out_constant_offset, UINT& out_constant_count);

		ID3D11Device* GetDevice() const { return _device.get(); }
		ID3D11DeviceContext* GetContext() const { return _context.get(); }

		bool SupportsCommandLists() const { return driver_command_lists; }

		void SetDebugName(ID3D11Resource* resource, const Memory::DebugStringA<>& name);
	};

}
