
namespace Device
{

	class DescriptorHeapD3D12;
	class TransferD3D12;

	class IDeviceD3D12 : public IDevice
	{
	public:
		static constexpr unsigned FRAME_QUEUE_COUNT = 3;

	private:
		struct SamplerGPUDescriptors;

		ComPtr<ID3D12Device> device;
		ComPtr<ID3D12CommandQueue> graphics_queue;
		ComPtr<IDXGIAdapter> adapter;
		ComPtr<IDXGIFactory2> factory;
		uint32_t buffer_index;
		Memory::Pointer<AllocatorD3D12::ConstantBuffer, Memory::Tag::Device> constant_buffer;
		Memory::Pointer<DescriptorHeapCacheD3D12, Memory::Tag::Device> rendertarget_view_heap;
		Memory::Pointer<DescriptorHeapCacheD3D12, Memory::Tag::Device> depthstencil_view_heap;
		Memory::Pointer<DescriptorHeapCacheD3D12, Memory::Tag::Device> cbv_srv_uav_heap;
		Memory::Pointer<DescriptorHeapCacheD3D12, Memory::Tag::Device> sampler_heap;
		Memory::Pointer<ShaderVisibleRingBuffer, Memory::Tag::Device> dynamic_descriptor_heap;
		Memory::Pointer<ShaderVisibleDescriptorHeapD3D12, Memory::Tag::Device> sampler_dynamic_descriptor_heap;

		Memory::Pointer<AllocatorD3D12::GPUAllocator, Memory::Tag::Device> allocator;
		Memory::Pointer<TransferD3D12, Memory::Tag::Device> transfer;
		std::array<Memory::Pointer<RootSignatureD3D12, Memory::Tag::Device>, magic_enum::enum_count<RootSignatureD3D12Preset::SignatureType>()> root_signatures;

		Memory::SmallVector<ID3D12CommandList*, 64, Memory::Tag::Device> frame_cmd_lists;
		Handle<SamplerGPUDescriptors> sampler_gpu_descriptors;
		DescriptorHeapD3D12::Handle null_descriptor;

		bool tearing_supported = false;
		unsigned adapter_index = 0;
		bool vsync = false;
		float fence_wait_time = 0;
		bool is_integrated_gpu = false;

		HANDLE idle_fence_event;
		ComPtr<ID3D12Fence> idle_fence;
		uint64_t idle_fence_value = 1;
		Memory::Mutex idle_mutex;

		Memory::UnorderedSet<std::string, Memory::Tag::Profile> markers;
		Memory::Mutex markers_mutex;

		uint64_t device_vram_size = -1;
		uint64_t shared_vram_size = -1;
		uint64_t total_vram = 0;
		uint64_t current_vram_usage = 0;
		uint64_t vram_budget = 0;
		uint64_t wave_lane_count = 0;
		double time_stamp_frequency = 1.0;

		bool fullscreen;

		HANDLE fence_event;
		ComPtr<ID3D12Fence> fence;
		uint64_t fence_value;
		std::array<uint64_t, FRAME_QUEUE_COUNT> frame_fence_values;

		static inline bool	fullscreen_failed = false;

	private:
		void WaitFence();
		const AdapterInfo* GetSelectedAdapter();
		void UpdateVRAM();
		void SetFullscreenMode(bool is_fullscreen);

	public:
		ID3D12Device* GetDevice() const { return device.Get(); }
		IDXGIAdapter* GetAdapter() const { return adapter.Get(); }
		ID3D12CommandQueue* GetGraphicsQueue() const { return graphics_queue.Get(); }
		ID3D12CommandQueue* GetTransferQueue() const { return graphics_queue.Get(); }
		IDXGIFactory2* GetFactory() const { return factory.Get(); }
		AllocatorD3D12::GPUAllocator* GetAllocator() const { return allocator.get(); }

		DescriptorHeapCacheD3D12& GetRTVDescriptorHeap() const { return *rendertarget_view_heap; }
		DescriptorHeapCacheD3D12& GetDSVDescriptorHeap() const { return *depthstencil_view_heap; }
		DescriptorHeapCacheD3D12& GetCBViewSRViewUAViewDescriptorHeap() const { return *cbv_srv_uav_heap; }
		DescriptorHeapCacheD3D12& GetSamplerDescriptorHeap() const { return *sampler_heap; }
		ShaderVisibleRingBuffer& GetDynamicDescriptorHeap() const { return *dynamic_descriptor_heap; }
		ShaderVisibleDescriptorHeapD3D12& GetSamplerDynamicDescriptorHeap() const { return *sampler_dynamic_descriptor_heap; }
		TransferD3D12& GetTransfer() const { return *transfer; }
		RootSignatureD3D12& GetRootSignature(RootSignatureD3D12Preset::SignatureType type) { return *root_signatures.at(type); }
		CD3DX12_GPU_DESCRIPTOR_HANDLE GetSamplersGPUAddress() const;
		const DescriptorHeapD3D12::Handle& GetNullDescriptor() const { return null_descriptor; };

		template<typename T> static void DebugName(const ComPtr<T>& object, const Memory::DebugStringA<>& name)
		{
			DebugName(object.Get(), name.c_str());
		}
		static void DebugName(ID3D12Object* object, const char* name);

	public:
		IDeviceD3D12(DeviceInfo* _device_info, DeviceType device_type, HWND focus_window, const Renderer::Settings& renderer_settings);
		virtual ~IDeviceD3D12();

		virtual void Suspend() final;
		virtual void Resume() final;

		virtual void WaitForIdle() final;

		virtual void RecreateSwapChain(HWND hwnd, UINT Output, const Renderer::Settings& renderer_settings) final;
		virtual bool CheckFullScreenFailed() final;

		virtual void BeginEvent(CommandBuffer& command_buffer, const std::wstring& text ) final;
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

		virtual void Swap() override final;

		virtual void ResourceFlush() final;

		virtual void SetVSync(bool enabled) final;
		virtual bool GetVSync() final;

		virtual HRESULT CheckForDXGIBufferChange() final;
		virtual HRESULT ResizeDXGIBuffers(UINT Width, UINT Height, BOOL bFullScreen, BOOL bBorderless = false) final;

		virtual bool IsWindowed() const final;

		virtual float GetFrameWaitTime() const final { return fence_wait_time; }

		virtual void ReinitGlobalSamplers() override;

		uint32_t GetBufferIndex() const { return buffer_index; }
		uint64_t GetLastCompletedFence() const { return fence->GetCompletedValue(); }
		uint64_t GetCurrentFrameFence() const { return fence_value; }
		bool GetTearingSupported() const { return tearing_supported; }
		double GetTimestampFrequency() const { return time_stamp_frequency; }

		AllocatorD3D12::ConstantBuffer& GetConstantBuffer() const { return *constant_buffer; }
		std::pair<uint8_t*, size_t> AllocateFromConstantBuffer(size_t size);

		PROFILE_ONLY(virtual std::vector<std::vector<std::string>> GetStats() final;)
		PROFILE_ONLY(virtual size_t GetSize() final;)
	};
}
