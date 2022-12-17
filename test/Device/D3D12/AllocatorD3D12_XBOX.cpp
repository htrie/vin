
namespace Device::AllocatorD3D12
{
	namespace
	{
		Memory::Type GetMemoryTypeD3D12(const D3D12_HEAP_PROPERTIES& heap_props)
		{
			auto memory_type = Memory::Type::GPU_WC;

			if (heap_props.Type == D3D12_HEAP_TYPE_CUSTOM && heap_props.CPUPageProperty == D3D12_CPU_PAGE_PROPERTY_WRITE_BACK)
				memory_type = Memory::Type::GPU_CACHABLE;

			return memory_type;
		}
	}

	std::string GPUAllocator::Stats::StatsKey::ToString()
	{
		if (allocation_type == Committed)
			return "[COMMITTED]";
		else if (allocation_type == ESRAM)
			return "[ESRAM]";

		switch (memory_type)
		{
			case Memory::Type::CPU_WB: return "[CPU_WB]";
			case Memory::Type::GPU_WC: return "[GPU_WC]";
			case Memory::Type::GPU_CACHABLE: return "[GPU_CACHABLE]";
			default: return "[Unknown]";
		}
	}

	void GPUAllocator::Stats::PrintProps(std::vector<std::string>& lines)
	{
		Memory::Lock lock(props_mutex);
		lines.push_back("Memory properties:\n");
		for (auto& iter : props_stats)
		{
			lines.push_back((std::string("\t") + iter.first.ToString() + " = " +
				std::to_string(iter.second.count) + ": " + Memory::ReadableSize(iter.second.size) + "\n").c_str());
		}
	}

	uint64_t GPUAllocator::Stats::GetCommittedSize() const
	{
		Memory::Lock lock(props_mutex);
		uint64_t result = 0;

		auto it = props_stats.find(StatsKey(Committed));
		if (it != props_stats.end())
			result = it->second.size;

		return result;
	}

	uint64_t GPUAllocator::Stats::GetTotalAllocationCount() const
	{
		Memory::Lock lock(props_mutex);

		uint64_t result = 0;

		for (auto& it : props_stats)
			result += it.second.count;

		return result;
	}

	void GPUAllocator::Stats::Add(const Memory::DebugStringA<>& name, size_t size, Type type, StatsKey key)
	{
		GPUAllocatorBase::Stats::Add(name, size, type);

		{
			Memory::Lock lock(props_mutex);
			props_stats[key].count++;
			props_stats[key].size += size;
		}
	}

	void GPUAllocator::Stats::Remove(const Memory::DebugStringA<>& name, size_t size, Type type, StatsKey key)
	{
		GPUAllocatorBase::Stats::Remove(name, size, type);

		{
			Memory::Lock lock(props_mutex);
			props_stats[key].count--;
			props_stats[key].size -= size;
		}
	}

	class AllocatorD3D12Chunk
	{
		const D3D12_GPU_VIRTUAL_ADDRESS gpu_address = 0;

		Memory::Type memory_type;
		Memory::Mutex chunk_mutex;

		uint64_t chunk_size = 0;
		uint64_t allocated_bytes = 0;
		ComPtr<D3D12MA::VirtualBlock> allocator_block;

		bool is_esram = false;

	public:
		AllocatorD3D12Chunk(D3D12_GPU_VIRTUAL_ADDRESS base_address, uint64_t chunk_size, const D3D12MA::ALLOCATION_CALLBACKS* allocation_callbacks, bool is_esram)
			: is_esram(is_esram)
			, chunk_size(chunk_size)
			, gpu_address(base_address)
		{
			D3D12MA::VIRTUAL_BLOCK_DESC desk = {};
			desk.pAllocationCallbacks = allocation_callbacks;
			desk.Size = chunk_size;
			D3D12MA::CreateVirtualBlock(&desk, &allocator_block);
		}

		AllocatorD3D12Chunk(Memory::Type memory_type, uint64_t chunk_size, const D3D12MA::ALLOCATION_CALLBACKS* allocation_callbacks)
			: AllocatorD3D12Chunk(
					(D3D12_GPU_VIRTUAL_ADDRESS)Memory::Allocate(Memory::Tag::Device, chunk_size,
					AllocatorD3D12VirtualAllocAlignment, memory_type),
					chunk_size,
					allocation_callbacks, 
					false
			)
		{
			this->memory_type = memory_type;
		}

		bool IsESRAM() const { return is_esram; }


		~AllocatorD3D12Chunk()
		{
			if (!is_esram)
				Memory::Free((void*)gpu_address);
		}

		std::optional<std::pair<D3D12MA::VirtualAllocation, uint64_t>> Allocate(size_t size, size_t alignment)
		{
			assert(alignment <= AllocatorD3D12VirtualAllocAlignment);

			D3D12MA::VIRTUAL_ALLOCATION_DESC desc = {};
			desc.Alignment = alignment;
			desc.Size = size;
			desc.Flags = D3D12MA::VIRTUAL_ALLOCATION_FLAG_NONE;

			D3D12MA::VirtualAllocation allocation;
			uint64_t offset;

			Memory::Lock lock(chunk_mutex);

			if (FAILED(allocator_block->Allocate(&desc, &allocation, &offset)))
				return std::nullopt;

			allocated_bytes += size;

			return std::make_pair(allocation, offset);
		}

		bool Free(D3D12MA::VirtualAllocation allocation)
		{
			Memory::Lock lock(chunk_mutex);
			D3D12MA::VIRTUAL_ALLOCATION_INFO info;
			allocator_block->GetAllocationInfo(allocation, &info);
			allocated_bytes -= info.Size;
			allocator_block->FreeAllocation(allocation);
			return allocator_block->IsEmpty();
		}

		const Memory::Type& GetMemoryType() const { return memory_type; }

		D3D12_GPU_VIRTUAL_ADDRESS GetGPUAddress() const { return gpu_address; }
		uint64_t GetAvailableSize() const { return chunk_size - allocated_bytes; }
		uint64_t GetChunkSize() const { return chunk_size; }
		bool Empty() const { return allocator_block->IsEmpty(); }
	};

	class GPUAllocator::ESRAM
	{
		bool failed = false;
		void* base_ptr = nullptr;
		std::shared_ptr<AllocatorD3D12Chunk> chunk;
		GPUAllocator& allocator;
	public:
		static constexpr size_t ESRAM_PAGES = 512;
		static constexpr size_t ESRAM_PAGE_SIZE = 64 * Memory::KB;
		static constexpr size_t ESRAM_SIZE = ESRAM_PAGES * ESRAM_PAGE_SIZE;

		ESRAM(GPUAllocator& allocator)
			: allocator(allocator)
		{
			std::array<UINT, ESRAM_PAGES> page_list;
			for (UINT i = 0; i < page_list.size(); i++)
				page_list[i] = i;

			base_ptr = VirtualAlloc(nullptr, ESRAM_SIZE, MEM_LARGE_PAGES | MEM_GRAPHICS | MEM_RESERVE, PAGE_READWRITE);

			if (SUCCEEDED(D3DMapEsramMemory(D3D11_MAP_ESRAM_LARGE_PAGES, base_ptr, (UINT)page_list.size(), page_list.data())))
			{
				chunk = std::make_shared<AllocatorD3D12Chunk>((D3D12_GPU_VIRTUAL_ADDRESS)base_ptr, ESRAM_SIZE, &allocator.allocation_callbacks, true);
			}
			else
			{
				failed = true;
			}
		}

		~ESRAM()
		{
			if (!failed)
				D3DUnmapEsramMemory(0, base_ptr, ESRAM_PAGES);
			VirtualFree(base_ptr, 0, MEM_RELEASE);
		}

		std::unique_ptr<GPUAllocator::Handle> Allocate(const Memory::DebugStringA<>& name, const AllocationInfoD3D12& info)
		{
			auto allocation = chunk->Allocate(info.mem_reqs.SizeInBytes, info.mem_reqs.Alignment);
			if (allocation)
			{
				LOG_DEBUG(L"[ESRAM] Allocated " << name.c_str() << L": " << info.mem_reqs.SizeInBytes << L" bytes with alignment " << info.mem_reqs.Alignment
					<< L". Remaining ESRAM: " << Memory::ReadableSize(chunk->GetAvailableSize()).c_str());

				auto actual_info = info;
				actual_info.chunk = chunk;
				actual_info.allocation = allocation->first;
				return std::make_unique<Handle>(name, &allocator, actual_info, (size_t)allocation->second);
			}
			
			LOG_DEBUG(L"[ESRAM] Allocation failed " << name.c_str() << L": No more ESRAM space.");
			return nullptr;
		}

		bool IsFailed() const { return failed; }
	};

	GPUAllocator::GPUAllocator(IDevice* device) : GPUAllocatorBase<AllocationInfoD3D12>(device)
	{
		allocation_callbacks.pAllocate = &AllocatorUtilsD3D12::D3D12MAAllocationFunction;
		allocation_callbacks.pFree = &AllocatorUtilsD3D12::D3D12MAFreeFunction;

		chunk_types.insert({ Memory::Type::GPU_WC, Memory::Pointer<ChunkList, Memory::Tag::Device>::make(this, Memory::Type::GPU_WC, AllocatorD3D12ChunkSize) });
		chunk_types.insert({ Memory::Type::GPU_CACHABLE, Memory::Pointer<ChunkList, Memory::Tag::Device>::make(this, Memory::Type::GPU_CACHABLE, AllocatorD3D12ChunkSize) });

		if (!IsScorpioOrEquivalent()) // ESRAM is not available on scorpio
		{
			esram = std::make_unique<ESRAM>(*this);
			if (esram->IsFailed())
				esram = nullptr;
		}
	}

	GPUAllocator::~GPUAllocator()
	{
		PrintLeaks();
	}

	void GPUAllocator::PrintLeaks()
	{
		PROFILE_ONLY(stats.LogLeaks();)
	}

	uint64_t GPUAllocator::GetAvailableTitleMemory()
	{
		TITLEMEMORYSTATUS status = { sizeof(TITLEMEMORYSTATUS) };
		if (TitleMemoryStatus(&status))
			return status.ullAvailMem;

		return 0;
	}

	bool GPUAllocator::CanAllocateBlock(uint64_t available_title_memory)
	{
		return available_title_memory >= AllocatorD3D12ChunkSize * 2;
	}

	std::unique_ptr<GPUAllocator::Handle> GPUAllocator::CreateResource(Memory::DebugStringA<> name, Type type, const D3D12_RESOURCE_DESC& desc, const D3D12_HEAP_PROPERTIES& heap_props, D3D12_RESOURCE_STATES initial_state, bool use_esram, ComPtr<ID3D12Resource>& out_resurce)
	{
		PROFILE;

		Memory::StackTag tag(Memory::Tag::Device);

		auto* d3d12_device = static_cast<IDeviceD3D12*>(device)->GetDevice();
		auto mem_reqs = d3d12_device->GetResourceAllocationInfo(0, 1, &desc);
		// TODO: would be nice to understand why GetResourceAllocationInfo sometimes 
		// reports alignment = 4 and it's triggering validation errors saying alignment must be 0x100 (256)
		// For now using minimum alignment 256 to avoid these errors
		mem_reqs.Alignment = std::max((uint64_t)256, mem_reqs.Alignment);
		const AllocationInfoD3D12 info{ type, GetMemoryTypeD3D12(heap_props), mem_reqs };
		std::unique_ptr<GPUAllocatorBase::Handle> handle = nullptr;

		// Create committed resource if any of these is true
		// - size is greater than half of the chunk size
		// - alignment is greater than chunks alignment
		bool create_committed = (Memory::AlignSize(mem_reqs.SizeInBytes, mem_reqs.Alignment) >= AllocatorD3D12ChunkSize / 2) || mem_reqs.Alignment > AllocatorD3D12VirtualAllocAlignment;

		Stats::StatsKey stats_key(GetMemoryTypeD3D12(heap_props));

		if (!create_committed)
		{
			if (use_esram && esram)
			{
				handle = esram->Allocate(name, info);
				if (handle)
					stats_key = Stats::StatsKey(Stats::ESRAM);
			}

			if (!handle)
				handle = Allocate(name, info);

			if (handle)
			{
				if (FAILED(d3d12_device->CreatePlacedResourceX(handle->GetAllocationInfo().chunk->GetGPUAddress() + handle->GetOffset(), &desc, initial_state, nullptr, IID_GRAPHICS_PPV_ARGS(out_resurce.GetAddressOf()))))
					return nullptr;
			}
			// If failed creating a block, try committed allocation
			else
				create_committed = true;
		}

		if (create_committed)
		{
			if (FAILED(d3d12_device->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE, &desc, initial_state, nullptr, IID_GRAPHICS_PPV_ARGS(out_resurce.GetAddressOf()))))
				return nullptr;

			stats_key = Stats::StatsKey(Stats::Committed);
			handle = std::make_unique<Handle>(name, this, info, 0);
		}

		total_used_vram.fetch_add(handle->GetAllocationInfo().MinSize());

		PROFILE_ONLY(stats.Add(handle->GetName(), info.mem_reqs.SizeInBytes, info.GetType(), stats_key);)
		return handle;
	}

	GPUAllocator::ChunkList* GPUAllocator::GetChunkList(Memory::Type memory_type)
	{
		auto& it = chunk_types.at(memory_type);
		return it.get();
	}

	std::shared_ptr<AllocatorD3D12Chunk> GPUAllocator::ChunkList::CreateChunk()
	{
		auto chunk = std::make_shared<AllocatorD3D12Chunk>(memory_type, preferred_chunk_size, &allocator.allocation_callbacks);
		chunks.push_back(chunk);
		return chunk;
	}

	std::unique_ptr<GPUAllocator::Handle> GPUAllocator::ChunkList::AllocateFromChunk(const Memory::DebugStringA<>& name, std::shared_ptr<AllocatorD3D12Chunk>& chunk, const AllocationInfoD3D12& info)
	{
		auto allocation = chunk->Allocate(info.mem_reqs.SizeInBytes, info.mem_reqs.Alignment);
		if (allocation)
		{
			auto actual_info = info;
			actual_info.chunk = chunk;
			actual_info.allocation = allocation->first;
			return std::make_unique<Handle>(name, &allocator, actual_info, (size_t)allocation->second);
		}
			
		return nullptr;
	}

	std::unique_ptr<GPUAllocator::Handle> GPUAllocator::ChunkList::Allocate(const Memory::DebugStringA<>& name, const AllocationInfoD3D12& info)
	{
		Memory::Lock lock(chunk_mutex);

		// Try to allocate from existing chunks. Chunks are sorted so that smallest free space goes first.
		for (auto& chunk : chunks)
		{
			auto handle = AllocateFromChunk(name, chunk, info);
			if (handle)
				return handle;
		}

		// Check budget and return null if too small free space to force a committed allocation
		const auto available_title_memory = GetAvailableTitleMemory();
		if (!CanAllocateBlock(available_title_memory))
			return nullptr;

		// Try to create a new chunk
		{
			auto chunk = CreateChunk();
			auto handle = AllocateFromChunk(name, chunk, info);
			return handle;
		}
	}

	void GPUAllocator::ChunkList::RemoveChunkIfEmpty(const std::shared_ptr<AllocatorD3D12Chunk>& chunk)
	{
		Memory::Lock lock(chunk_mutex);
		if (chunk->Empty())
		{
			auto it = std::find(chunks.begin(), chunks.end(), chunk);
			if (it != chunks.end())
				chunks.erase(it);
		}
	}

	// Chunks with lesser free space go first
	void GPUAllocator::ChunkList::IncrementalSort()
	{
		Memory::Lock lock(chunk_mutex);

		auto it = chunks.begin();
		while (it != chunks.end() && std::next(it) != chunks.end())
		{
			auto next = std::next(it);

			// 1 iteration of bubble sort
			if ((*it)->GetAvailableSize() > (*next)->GetAvailableSize())
			{
				std::swap(*it, *next);
				return;
			}

			it = next;
		}
	}

	std::unique_ptr<GPUAllocator::Handle> GPUAllocator::Allocate(const Memory::DebugStringA<>& name, const AllocationInfoD3D12& info)
	{
		auto* chunk_list = GetChunkList(info.memory_type);
		return chunk_list->Allocate(name, info);
	}

	void GPUAllocator::Free(const GPUAllocatorBase::Handle& handle)
	{
		PROFILE;
		auto& chunk = handle.GetAllocationInfo().chunk;
		Stats::StatsKey stats_key(Stats::Committed);
		if (chunk)
		{
			const bool empty = chunk->Free(handle.GetAllocationInfo().allocation);

			if (!chunk->IsESRAM())
			{
				auto chunk_list = GetChunkList(chunk->GetMemoryType());
				stats_key = Stats::StatsKey(chunk->GetMemoryType());

				if (empty)
					chunk_list->RemoveChunkIfEmpty(chunk);
				chunk_list->IncrementalSort(); // blocks with smaller free size go first
			}
			else
				stats_key = Stats::StatsKey(Stats::ESRAM);
		}
		
		const auto& info = handle.GetAllocationInfo();
		total_used_vram.fetch_sub(info.MinSize());

		PROFILE_ONLY(stats.Remove(handle.GetName(), info.MinSize(), info.GetType(), stats_key);)
	}

#if defined(PROFILING)
	std::vector<std::vector<std::string>> GPUAllocator::GetStats()
	{
		std::vector<std::vector<std::string>> out(3);

		{
			std::vector<std::string>& lines = out[0];

			uint32_t chunk_count = 0;
			for (auto& chunk_type : chunk_types)
				chunk_count += chunk_type.second->GetChunkCount();

			const size_t chunk_size = chunk_count * AllocatorD3D12ChunkSize;

			const auto available_title_memory = GetAvailableTitleMemory();
			const bool can_allocate_block = CanAllocateBlock(available_title_memory);
			const uint64_t committed_size = stats.GetCommittedSize();

			lines.push_back("\n");
			lines.push_back((std::string("Total = ") + std::to_string(stats.GetTotalAllocationCount()) + ": " + Memory::ReadableSize(GetSize()) + "\n").c_str());
			lines.push_back((std::string("Chunks = ") + std::to_string(chunk_count) + ": " + Memory::ReadableSize(chunk_size) + "\n").c_str());
			lines.push_back((std::string("Overhead = ") + Memory::ReadableSize(chunk_size - (GetSize() - committed_size)) + "\n").c_str());
			lines.push_back((std::string("Available title memory = ") + Memory::ReadableSize(available_title_memory) + "\n").c_str());
			lines.push_back((std::string("Can allocate block = ") + std::to_string(can_allocate_block) + "\n").c_str());
		}

		stats.PrintProps(out[1]);
		out[1].push_back("\n");
		stats.PrintTypes(out[1]);
		out[1].push_back("\n");
		stats.PrintConstants(out[1]);
		out[1].push_back("\n");

		stats.PrintFiles(out[2]);

		return out;
	}

	size_t GPUAllocator::GetSize()
	{
		return total_used_vram.load();
	}
#endif

}