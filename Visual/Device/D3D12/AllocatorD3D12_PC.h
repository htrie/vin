#pragma once

namespace Device
{
	class IDeviceD3D12;
}

namespace Device::AllocatorD3D12
{

	struct AllocationInfoD3D12
	{
		enum class ResourceType
		{
			RenderTarget = 0,
			Texture,
			Buffer
		};

		GPUAllocatorBase<AllocationInfoD3D12>::Type type;

		ResourceType resource_type = ResourceType::RenderTarget;
		D3D12_HEAP_PROPERTIES heap_props = {};
		uint64_t size = 0;
		ComPtr<D3D12MA::Allocation> allocation;

		auto GetType() const { return type; }
		size_t GetSize() const { return size; }
		size_t MinSize() const { return size; }
		size_t GetAlignment() const { return 0; }
	};

	class GPUAllocator : public GPUAllocatorBase<AllocationInfoD3D12>
	{

	public:
		using AllocationInfo = AllocationInfoD3D12;
		using Handle = GPUAllocatorBase::Handle;

		GPUAllocator(IDevice* device);
		~GPUAllocator();

		std::unique_ptr<Handle> CreateResource(Memory::DebugStringA<> name, Type type, const D3D12_RESOURCE_DESC& desc, const D3D12_HEAP_PROPERTIES& heap_props, D3D12_RESOURCE_STATES initial_state, bool use_esram, ComPtr<ID3D12Resource>& out_resurce);

		PROFILE_ONLY(std::vector<std::vector<std::string>> GetStats();)
		PROFILE_ONLY(size_t GetSize();)

	private:
		struct PropsHasher
		{
			std::size_t operator()(const D3D12_HEAP_PROPERTIES& heap_props) const
			{
				const uint64_t a = (uint64_t)heap_props.Type << 0;
				const uint64_t b = (uint64_t)heap_props.CPUPageProperty << 4;
				const uint64_t c = (uint64_t)heap_props.MemoryPoolPreference << 8;
				return (size_t)(a | b | c);
			}
		};

		class Stats : public GPUAllocatorBase::Stats
		{
			std::string HeapPropsToString(const D3D12_HEAP_PROPERTIES& heap_props);
			std::string HeapTypeToString(const D3D12_HEAP_TYPE& heap_type);

			struct PropsStats
			{
				unsigned count;
				size_t size;
			};

			Memory::FlatMap<D3D12_HEAP_PROPERTIES, PropsStats, Memory::Tag::Device, PropsHasher> props_stats;
			Memory::Mutex props_mutex;

		public:
			void Add(const Memory::DebugStringA<>& name, size_t size, Type type, const D3D12_HEAP_PROPERTIES& heap_props);
			void Remove(const Memory::DebugStringA<>& name, size_t size, Type type, const D3D12_HEAP_PROPERTIES& heap_props);

			void PrintStatInfo(const D3D12MA::DetailedStatistics& stat, std::vector<std::string>& lines);
			void PrintHeapTypesInfo(const D3D12MA::TotalStatistics& total_stat, std::vector<std::string>& lines);
			void PrintProps(std::vector<std::string>& lines);
		};

	public:
		PROFILE_ONLY(Stats stats;)

	private:

		typedef std::pair<D3D12_HEAP_PROPERTIES, D3D12_HEAP_FLAGS> PoolKey;

		struct PoolHasher
		{
			std::size_t operator()(const PoolKey& key) const
			{
				return PropsHasher()(key.first) | ((size_t)key.second << 16);
			}
		};

		// D3D32MA only allows to specify D3D12_HEAP_TYPE for the allocations.
		// Using the arbitrary D3D12_HEAP_PROPERTIES is possible with custom pools.
		// All allocations created from such pools are committed (CreateCommittedResource).
		mutable Memory::FlatMap<PoolKey, ComPtr<D3D12MA::Pool>, Memory::Tag::Device, PoolHasher> custom_pools;
		Memory::SpinMutex pool_mutex;
		D3D12MA::ALLOCATION_CALLBACKS allocation_callbacks;
		ComPtr<D3D12MA::Allocator> allocator;
		bool resource_heap_tear_2 = false; // If true we can store all the resources in a single heap

	protected:
		AllocationInfoD3D12::ResourceType GetResourceType(Type type);
		D3D12_HEAP_FLAGS GetHeapFlags(AllocationInfoD3D12::ResourceType type) const;
		D3D12MA::Pool* GetPool(const D3D12_HEAP_PROPERTIES& heap_props, const D3D12_HEAP_FLAGS heap_flags) const;
		void PrintLeaks();
		void Free(const GPUAllocatorBase::Handle& handle) override;

	};

	using Handle = GPUAllocator::Handle;
	using Type = GPUAllocator::Type;
}