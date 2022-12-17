#pragma once


namespace Device
{
	class IDeviceD3D12;
}

namespace Device::AllocatorD3D12
{
	constexpr uint64_t AllocatorD3D12VirtualAllocAlignment = 64 * Memory::KB;
	constexpr uint64_t AllocatorD3D12ChunkSize = 1 * Memory::MB; // NO MORE THAN 1MB (to reduce fragmentation).
	class AllocatorD3D12Chunk;

	struct AllocationInfoD3D12
	{
		GPUAllocatorBase<AllocationInfoD3D12>::Type type;

		Memory::Type memory_type = Memory::Type::GPU_WC;
		D3D12_RESOURCE_ALLOCATION_INFO mem_reqs = {};
		std::shared_ptr<AllocatorD3D12Chunk> chunk;
		D3D12MA::VirtualAllocation allocation;

		auto GetType() const { return type; }
		size_t GetSize() const { return mem_reqs.SizeInBytes; }
		size_t GetAlignment() const { return mem_reqs.Alignment; }
		size_t MinSize() const { return Memory::AlignSize(mem_reqs.SizeInBytes, mem_reqs.Alignment); }
	};

	class GPUAllocator : public GPUAllocatorBase<AllocationInfoD3D12>
	{
	public:
		using AllocationInfo = AllocationInfoD3D12;
		using Handle = GPUAllocatorBase::Handle;

		static uint64_t GetAvailableTitleMemory();
		static bool CanAllocateBlock(uint64_t available_title_memory);
		GPUAllocator(IDevice* device);
		~GPUAllocator();

		PROFILE_ONLY(std::vector<std::vector<std::string>> GetStats();)
		PROFILE_ONLY(size_t GetSize();)

	private:
		class Stats : public GPUAllocatorBase::Stats
		{
		public:
			enum AllocationType { Default = 0, Committed, ESRAM };

			struct StatsKey
			{
				Memory::Type memory_type = Memory::Type::GPU_WC;
				AllocationType allocation_type = Default;

				bool operator==(const StatsKey& other) const 
				{
					if (allocation_type != other.allocation_type) return false;
					if (allocation_type == Default) return memory_type == other.memory_type;
					return true;
				};
				explicit StatsKey(AllocationType type) : allocation_type(type) {}
				explicit StatsKey(Memory::Type memory_type) : memory_type(memory_type), allocation_type(Default) {}
				std::string ToString();
			};

		private:
			struct StatsHasher
			{
				std::size_t operator()(const StatsKey& key) const
				{
					return key.allocation_type != Default ? ((1 << 16) + key.allocation_type) : (size_t)key.memory_type;
				}
			};

			struct PropsStats
			{
				unsigned count;
				size_t size;
			};

			Memory::FlatMap<StatsKey, PropsStats, Memory::Tag::Device, StatsHasher> props_stats;
			Memory::Mutex props_mutex;

		public:
			void Add(const Memory::DebugStringA<>& name, size_t size, Type type, StatsKey key);
			void Remove(const Memory::DebugStringA<>& name, size_t size, Type type, StatsKey key);
			uint64_t GetCommittedSize() const;
			uint64_t GetTotalAllocationCount() const;


			void PrintProps(std::vector<std::string>& lines);
		};

		class ChunkList : public NonCopyable
		{
			GPUAllocator& allocator;
			std::shared_ptr<AllocatorD3D12Chunk> CreateChunk();
			std::unique_ptr<GPUAllocator::Handle> AllocateFromChunk(const Memory::DebugStringA<>& name, std::shared_ptr<AllocatorD3D12Chunk>& chunk, const AllocationInfoD3D12& info);

			Memory::Mutex chunk_mutex;
			Memory::Type memory_type;
			uint64_t preferred_chunk_size = 0;
			Memory::Deque<std::shared_ptr<AllocatorD3D12Chunk>, Memory::Tag::Device> chunks;
		
		public:
			ChunkList(GPUAllocator* allocator, Memory::Type memory_type, uint64_t preferred_chunk_size)
				: allocator(*allocator), memory_type(memory_type), preferred_chunk_size(preferred_chunk_size)
			{
			}

			uint32_t GetChunkCount() const { Memory::Lock lock(chunk_mutex); return (uint32_t)chunks.size(); }

			void RemoveChunkIfEmpty(const std::shared_ptr<AllocatorD3D12Chunk>& chunk);
			std::unique_ptr<GPUAllocator::Handle> Allocate(const Memory::DebugStringA<>& name, const AllocationInfoD3D12& info);
			void IncrementalSort();
		};

		ChunkList* GetChunkList(Memory::Type memory_type);

		Memory::FlatMap<Memory::Type, Memory::Pointer<ChunkList, Memory::Tag::Device>, Memory::Tag::Device> chunk_types;

	public:
		PROFILE_ONLY(Stats stats;)
		std::unique_ptr<GPUAllocator::Handle> CreateResource(Memory::DebugStringA<> name, Type type, const D3D12_RESOURCE_DESC& desc, const D3D12_HEAP_PROPERTIES& heap_props, D3D12_RESOURCE_STATES initial_state, bool use_esram, ComPtr<ID3D12Resource>& out_resurce);

	private:
		class ESRAM;
		std::unique_ptr<ESRAM> esram;
		D3D12MA::ALLOCATION_CALLBACKS allocation_callbacks;

	protected:
		void PrintLeaks();
		std::unique_ptr<GPUAllocator::Handle> Allocate(const Memory::DebugStringA<>& name, const AllocationInfoD3D12& info);
		void Free(const GPUAllocator::Handle& handle) override;
	};

	using Handle = GPUAllocator::Handle;
	using Type = GPUAllocator::Type;

}