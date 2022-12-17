
namespace Device
{
	class TransferVulkan;

	class IDeviceVulkan : public IDevice
	{
	public:
	#if defined(MOBILE)
		static constexpr unsigned FRAME_QUEUE_COUNT = 3; // Triple-buffering recommended on iOS.
	#else
		static constexpr unsigned FRAME_QUEUE_COUNT = 3;
	#endif

	private:
		std::string pipeline_cache_path;

		vk::UniqueInstance instance;
		vk::UniqueDevice device;
		vk::Queue graphics_queue;
		vk::Queue transfer_queue;
		std::unique_ptr<SwapChain> swap_chain;

		Memory::SmallVector<vk::CommandBuffer, 64, Memory::Tag::Device> frame_cmd_bufs;

		std::array<vk::UniqueFence, FRAME_QUEUE_COUNT> draw_fences;

		Memory::Pointer<AllocatorVulkan, Memory::Tag::Device> allocator;
		Memory::Pointer<TransferVulkan, Memory::Tag::Device> transfer;
		Memory::Pointer<AllocatorVulkan::ConstantBuffer, Memory::Tag::Device> constant_buffer;

		vk::PhysicalDevice physical_device;
		vk::PhysicalDeviceProperties device_props;
		vk::PhysicalDeviceMemoryProperties device_memory_props;
		bool supports_astc = false;

		Memory::Array<uint32_t, 2> queue_family_indices;
		uint32_t graphics_queue_family_index = (uint32_t)-1;
		uint32_t transfer_queue_family_index = (uint32_t)-1;
		uint32_t graphics_queue_index = 0;
		uint32_t transfer_queue_index = 0;
		uint32_t min_image_count = 0;

		unsigned subgroup_size = 4;

		float fence_wait_time = 0;
		float backbuffer_wait_time = 0;

		unsigned adapter_index = 0;

		unsigned width = 0;
		unsigned height = 0;

		unsigned buffer_index = 0;

		bool fullscreen = false;
		bool vsync = false;
		bool reinit_swap_chain = false;
		bool supports_get_physical_device_properties2 = false;
		bool supports_get_surface_capabilities2 = false;
		bool supports_external_fence_capabilities = false;
		bool supports_fullscreen_exclusive = false;
		bool supports_memory_budget_query = false;

		VkDebugUtilsMessengerEXT debug_messenger = VK_NULL_HANDLE;
		PFN_vkDebugMarkerSetObjectNameEXT object_name_callback = VK_NULL_HANDLE;
		PFN_vkCmdDebugMarkerBeginEXT marker_begin_callback = VK_NULL_HANDLE;
		PFN_vkCmdDebugMarkerEndEXT marker_end_callback = VK_NULL_HANDLE;
		PFN_vkCmdDebugMarkerInsertEXT marker_insert_callback = VK_NULL_HANDLE;

		Memory::FlatSet<std::string, Memory::Tag::Profile> markers;
		Memory::Mutex markers_mutex;
			
		void CreateDebugMessenger();
		void DestroyDebugMessenger();
		void InitDebugMarker();

		static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);
		
		void InitInstance();
	#if defined(__APPLE__)
		void InitMoltenVK();
	#endif
		void InitDevice();
		void InitSwapChain();
		void InitFences();

		void ReinitSwapChain();

	#if defined(__APPLE__)
		void PrintMoltenVK();
	#endif
		void PrintQueues() const;
		void PrintHeaps() const;

	#if defined(WIN32)
		struct PhysicalDeviceData
		{
			vk::PhysicalDevice device;
			vk::PhysicalDeviceIDProperties id_props;
			vk::PhysicalDeviceProperties2 props;
		};
		std::vector<PhysicalDeviceData> EnumeratePhysicalDevices();
		const Device::AdapterInfo* GetSelectedAdapter();
		static const PhysicalDeviceData* FindPhysicalDeviceLUID(const std::vector<IDeviceVulkan::PhysicalDeviceData>& device_datas, const std::wstring& name, const uint8_t luid[8]);
		static const PhysicalDeviceData* FindPhysicalDeviceName(const std::vector<IDeviceVulkan::PhysicalDeviceData>& device_datas, const std::wstring& name);
		static const PhysicalDeviceData* FindPhysicalDeviceIndex(const std::vector<IDeviceVulkan::PhysicalDeviceData>& device_datas, const std::wstring& name, unsigned index);
	#endif
		vk::PhysicalDevice GetSelectedDevice();
		void SetFullscreenMode(bool fullscreen);
		
		bool WaitFence();
		void ResetFence();

	public:
		IDeviceVulkan(DeviceInfo* _device_info, DeviceType device_type, HWND focus_window, const Renderer::Settings& renderer_settings);
		virtual ~IDeviceVulkan();

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
		bool SupportsFullscreen() const { return supports_fullscreen_exclusive && supports_get_physical_device_properties2 && supports_get_surface_capabilities2; }
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

		virtual SwapChain* GetMainSwapChain() const final { return swap_chain.get(); }
		virtual float GetFrameWaitTime() const final { return fence_wait_time + backbuffer_wait_time; }

		PROFILE_ONLY(virtual std::vector<std::vector<std::string>> GetStats() final;)
		PROFILE_ONLY(virtual size_t GetSize() final;)

		void Swap() override final;

		bool SupportsASTC() const { return supports_astc; }

		unsigned GetWidth() const { return width; }
		unsigned GetHeight() const { return height; }

		unsigned GetBufferIndex() const { return buffer_index; }

		uint32_t FindFamily(vk::QueueFlags needed_queue_flags, vk::QueueFlags forbidden_queue_flags) const;

		uint32_t MemoryType(uint32_t type_bits, vk::MemoryPropertyFlags mem_props) const;
		size_t MemoryMaxSize(vk::MemoryPropertyFlags mem_props) const;
		size_t MemoryHeapSize(vk::MemoryHeapFlags heap) const;
		size_t MemoryHeapBudgetAvailable(const vk::PhysicalDeviceMemoryBudgetPropertiesEXT& budget, vk::MemoryHeapFlags heap) const;

		vk::ImageTiling Tiling(vk::Format format, vk::FormatFeatureFlags feature) const;

		std::pair<uint8_t*, size_t> AllocateFromConstantBuffer(size_t size);

		uint32_t GetGraphicsQueueFamilyIndex() const { return graphics_queue_family_index; }
		uint32_t GetTransferQueueFamilyIndex() const { return transfer_queue_family_index; }
		const Memory::Array<uint32_t, 2>& GetQueueFamilyIndices() const { return queue_family_indices; }

		vk::Queue& GetGraphicsQueue() { return graphics_queue; }
		vk::Queue& GetTransferQueue() { return transfer_queue; }
		vk::Device& GetDevice() { return device.get(); }
		vk::PhysicalDeviceProperties& GetDeviceProps() { return device_props; }
		vk::PhysicalDeviceMemoryProperties& GetDeviceMemoryProps() { return device_memory_props; }
		vk::Instance& GetInstance() { return instance.get(); }
		vk::PhysicalDevice& GetPhysicalDevice() { return physical_device; }

		AllocatorVulkan& GetAllocator() { return *allocator; }
		TransferVulkan& GetTransfer() { return *transfer.get(); }
		AllocatorVulkan::ConstantBuffer& GetConstantBuffer() { return *constant_buffer.get(); }
		bool GetSupportsMemoryBudget() const { return supports_memory_budget_query; }

		void DebugName(uint64_t object, VkDebugReportObjectTypeEXT objectType, const Memory::DebugStringA<>& name);

		static inline bool	fullscreen_failed = false;
	};

}
