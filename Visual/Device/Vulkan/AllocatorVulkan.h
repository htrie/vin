namespace Device
{
	struct AllocationInfoVulkan
	{
		GPUAllocatorBase<AllocationInfoVulkan>::Type type;
		VmaAllocation allocation = nullptr;
		VmaAllocationInfo allocation_info;
		VkDeviceSize size;
		vk::MemoryPropertyFlags mem_props;

		auto GetType() const { return type; }
		size_t GetSize() const { return size; }
		size_t MinSize() const { return size; }
		uint64_t ComputeChunkKey() const { return 0; }
	};

	class AllocatorVulkan : public GPUAllocatorBase<AllocationInfoVulkan>
	{

	public:
		using AllocationInfo = AllocationInfoVulkan;
		using Handle = GPUAllocatorBase::Handle;

		class Data
		{
			Memory::DebugStringA<> name;
			vk::MemoryRequirements mem_reqs;
			vk::UniqueBuffer buffer;
			std::unique_ptr<Handle> handle;
			size_t size = 0;
			uint8_t* map = nullptr;

		public:
			Data() {}
			Data(const Memory::DebugStringA<>& name, IDevice* device, Type type, size_t size, vk::BufferUsageFlags usages, vk::MemoryPropertyFlags memory_props);

			const vk::UniqueBuffer& GetBuffer() const { return buffer; }
			size_t GetSize() const { return size; }
			uint8_t* GetMap() const { return map; }
		};

		class ConstantBuffer
		{
			Memory::SpinMutex mutex;
			Memory::DebugStringA<> name;
			std::array<uint8_t*, 3> maps = {};
			std::array<Data, 3> datas;
			size_t max_size = 0;
			size_t alignment = 0;
			size_t offset = 0;
			uint32_t frame_index = 0;

			static inline size_t LocalBlockSize = 64 * Memory::KB;
			static inline thread_local size_t local_offset = 0;
			static inline thread_local size_t local_available = 0;
			static inline thread_local unsigned local_frame_index = 0;

			size_t AllocateSlow(size_t size);
			size_t AllocateLocal(size_t size);

		public:
			ConstantBuffer(const Memory::DebugStringA<>& name, IDevice* device, size_t max_size);

			void Reset();

			std::pair<uint8_t*, size_t> Allocate(size_t size, unsigned backbuffer_index);

			const vk::UniqueBuffer& GetBuffer(unsigned backbuffer_index) const { return datas[backbuffer_index].GetBuffer(); }
			uint8_t* GetMap(unsigned backbuffer_index) { return maps[backbuffer_index]; }

			size_t GetSize() const { return offset; }
			size_t GetMaxSize() const { return max_size; }
		};

		AllocatorVulkan(IDevice* device);
		~AllocatorVulkan();

		VmaAllocationCreateInfo GetAllocationInfoForGPUOnly(const vk::PhysicalDeviceProperties& device_props);
		VmaAllocationCreateInfo GetAllocationInfoForMappedMemory(VkMemoryPropertyFlags preferred_flags, VkMemoryPropertyFlags required_flags);

		std::unique_ptr<Handle> AllocateForBuffer(const Memory::DebugStringA<>& name, Type type, VkBuffer buffer, VmaAllocationCreateInfo allocation_create_info);
		std::unique_ptr<Handle> AllocateForImage(const Memory::DebugStringA<>& name, Type type, VkImage image, VmaAllocationCreateInfo allocation_create_info);

		PROFILE_ONLY(std::vector<std::vector<std::string>> GetStats();)
		PROFILE_ONLY(size_t GetSize();)

	private:
		class Stats : public GPUAllocatorBase::Stats
		{
			struct PropsStats
			{
				unsigned count;
				size_t size;
			};

			struct PropsHasher
			{
				std::size_t operator()(const vk::MemoryPropertyFlags& mem_props) const
				{
					return (std::size_t)mem_props.operator unsigned int();
				}
			};

			VmaAllocator& allocator;
			Memory::FlatMap<vk::MemoryPropertyFlags, PropsStats, Memory::Tag::Device, PropsHasher> actual_props_stats;
			Memory::Mutex props_mutex;

		public:
			Stats(VmaAllocator& allocator) : allocator(allocator) {}

			void Add(const Memory::DebugStringA<>& name, const AllocationInfoVulkan info);
			void Remove(const Memory::DebugStringA<>& name, const AllocationInfoVulkan info);

			void PrintMemoryType(const vk::PhysicalDeviceMemoryProperties& device_memory_props, uint32_t memory_type_index, std::vector<std::string>& lines);
			void PrintStatInfo(const VmaDetailedStatistics& stat, std::vector<std::string>& lines);
			void PrintMemoryTypes(IDevice* device, const VmaTotalStatistics& vma_stats, std::vector<std::string>& lines);
			void PrintHeaps(IDevice* Device, const VmaTotalStatistics& vma_stats, std::vector<std::string>& lines) const;
			void PrintProps(std::vector<std::string>& lines);
		};

	public:
		PROFILE_ONLY(Stats stats;)

		virtual VRAM GetVRAM() const final;

	private:
		VkAllocationCallbacks allocation_callbacks = {};
		VmaAllocator allocator = nullptr;

	protected:
		void PrintLeaks();
		void Free(const GPUAllocatorBase::Handle& handle) override;

	};
}