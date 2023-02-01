
namespace Device::AllocatorD3D12
{

	std::string GPUAllocator::Stats::HeapTypeToString(const D3D12_HEAP_TYPE& heap_type)
	{
		switch (heap_type)
		{
		case D3D12_HEAP_TYPE_DEFAULT: return "Default"; break;
		case D3D12_HEAP_TYPE_UPLOAD: return "Upload"; break;
		case D3D12_HEAP_TYPE_READBACK: return "Readback"; break;
		case D3D12_HEAP_TYPE_CUSTOM: return "Custom"; break;
		default: return "Error"; break;
		}
	}

	std::string GPUAllocator::Stats::HeapPropsToString(const D3D12_HEAP_PROPERTIES& heap_props)
	{
		std::string heap_type = "Unknown";
		switch (heap_props.Type)
		{
		case D3D12_HEAP_TYPE_DEFAULT: heap_type = "[Default, "; break;
		case D3D12_HEAP_TYPE_UPLOAD: heap_type = "[Upload, "; break;
		case D3D12_HEAP_TYPE_READBACK: heap_type = "[Readback, "; break;
		case D3D12_HEAP_TYPE_CUSTOM: heap_type = "[Custom, "; break;
		default: heap_type = "[Error, "; break;
		}

		std::string memory_pool = "";
		switch (heap_props.MemoryPoolPreference)
		{
		case D3D12_MEMORY_POOL_UNKNOWN: memory_pool = "Unknown, "; break;
		case D3D12_MEMORY_POOL_L0: memory_pool = "L0, "; break;
		case D3D12_MEMORY_POOL_L1: memory_pool = "L1, "; break;
		default: memory_pool = "Error, "; break;
		}

		std::string cpu_page_prop = "";
		switch (heap_props.CPUPageProperty)
		{
		case D3D12_CPU_PAGE_PROPERTY_NOT_AVAILABLE: cpu_page_prop = "Not Available]"; break;
		case D3D12_CPU_PAGE_PROPERTY_WRITE_BACK: cpu_page_prop = "Writeback]"; break;
		case D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE: cpu_page_prop = "Writecombine]"; break;
		case D3D12_CPU_PAGE_PROPERTY_UNKNOWN: cpu_page_prop = "Unknown]"; break;
		default: cpu_page_prop = "Error]"; break;
		}

		return heap_type + memory_pool + cpu_page_prop;
	}

	void GPUAllocator::Stats::PrintStatInfo(const D3D12MA::DetailedStatistics& stat, std::vector<std::string>& lines)
	{
		lines.push_back(std::string("- Allocations = ") + std::to_string(stat.Stats.AllocationCount) + " ( " + Memory::ReadableSize(stat.Stats.AllocationBytes)
			+ " ), Num Blocks = " + std::to_string(stat.Stats.BlockCount));
		lines.push_back("- Peak Unused size = " + Memory::ReadableSize(stat.UnusedRangeSizeMax) + ", Unused range count = " + std::to_string(stat.UnusedRangeCount));
	}

	void GPUAllocator::Stats::PrintHeapTypesInfo(const D3D12MA::TotalStatistics& total_stat, std::vector<std::string>& lines)
	{
		for (uint32_t i = 0; i < D3D12MA::HEAP_TYPE_COUNT; i++)
		{
			lines.push_back("- [ " + HeapTypeToString((D3D12_HEAP_TYPE)(D3D12_HEAP_TYPE_DEFAULT + i)) + " ]: ");
			PrintStatInfo(total_stat.HeapType[i], lines);
		}
	}

	void GPUAllocator::Stats::PrintProps(std::vector<std::string>& lines)
	{
		Memory::Lock lock(props_mutex);
		lines.push_back("Memory properties:\n");
		for (auto& iter : props_stats)
		{
			lines.push_back((std::string("- ") + HeapPropsToString(iter.first) + " = " +
				std::to_string(iter.second.count) + ": " + Memory::ReadableSize(iter.second.size) + "\n").c_str());
		}
	}

	void GPUAllocator::Stats::Add(const Memory::DebugStringA<>& name, size_t size, Type type, const D3D12_HEAP_PROPERTIES& heap_props)
	{
		GPUAllocatorBase::Stats::Add(name, size, type);

		{
			Memory::Lock lock(props_mutex);
			props_stats[heap_props].count++;
			props_stats[heap_props].size += size;
		}
	}

	void GPUAllocator::Stats::Remove(const Memory::DebugStringA<>& name, size_t size, Type type, const D3D12_HEAP_PROPERTIES& heap_props)
	{
		GPUAllocatorBase::Stats::Remove(name, size, type);

		{
			Memory::Lock lock(props_mutex);
			props_stats[heap_props].count--;
			props_stats[heap_props].size -= size;
		}
	}

	GPUAllocator::GPUAllocator(IDevice* device) : GPUAllocatorBase<AllocationInfo>(device)
	{
		auto device_d3d12 = static_cast<IDeviceD3D12*>(device);
		
		D3D12MA::ALLOCATOR_DESC allocatorDesc = {};
		allocatorDesc.pDevice = device_d3d12->GetDevice();
		allocatorDesc.pAdapter = device_d3d12->GetAdapter();
		allocation_callbacks.pAllocate = &AllocatorUtilsD3D12::D3D12MAAllocationFunction;
		allocation_callbacks.pFree = &AllocatorUtilsD3D12::D3D12MAFreeFunction;
		allocatorDesc.pAllocationCallbacks = &allocation_callbacks;
		D3D12MA::CreateAllocator(&allocatorDesc, &allocator);

		resource_heap_tear_2 = allocator->GetD3D12Options().ResourceHeapTier == D3D12_RESOURCE_HEAP_TIER_2;
	}

	GPUAllocator::~GPUAllocator()
	{
		custom_pools.clear();
		allocator.Reset();
		PrintLeaks();
	}

	void GPUAllocator::PrintLeaks()
	{
		PROFILE_ONLY(stats.LogLeaks());
	}

	D3D12MA::Pool* GPUAllocator::GetPool(const D3D12_HEAP_PROPERTIES& heap_props, const D3D12_HEAP_FLAGS heap_flags) const
	{
		Memory::SpinLock lock(pool_mutex);

		auto it = custom_pools.find(PoolKey{ heap_props, heap_flags });
		if (it != custom_pools.end())
			return it->second.Get();

		ComPtr<D3D12MA::Pool> result;

		D3D12MA::POOL_DESC desk = {};
		desk.BlockSize = 0;
		desk.HeapFlags = heap_flags;
		desk.HeapProperties = heap_props;
		allocator->CreatePool(&desk, &result);
		
		custom_pools.insert({ PoolKey{ heap_props, heap_flags }, result });
		return result.Get();
	}

	D3D12_HEAP_FLAGS GPUAllocator::GetHeapFlags(AllocationInfoD3D12::ResourceType type) const
	{
		if (resource_heap_tear_2)
			return D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES;

		switch (type)
		{
			case AllocationInfoD3D12::ResourceType::RenderTarget:	return D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES;
			case AllocationInfoD3D12::ResourceType::Texture:		return D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES;
			default:												return D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS;
		}
	}

	AllocationInfoD3D12::ResourceType GPUAllocator::GetResourceType(Type type)
	{
		switch (type)
		{
			case RenderTarget: return AllocationInfoD3D12::ResourceType::RenderTarget;
			case Texture: return AllocationInfoD3D12::ResourceType::Texture;

			case Index:
			case Vertex:
			case Uniform:
			case Staging:
			case Structured:
			case Texel:
			case ByteAddress:
				return AllocationInfoD3D12::ResourceType::Buffer;
		}

		throw ExceptionD3D12("Unknown resource type");
	}

	std::unique_ptr<GPUAllocator::Handle> GPUAllocator::CreateResource(Memory::DebugStringA<> name, Type type, const D3D12_RESOURCE_DESC& desc, const D3D12_HEAP_PROPERTIES& heap_props, D3D12_RESOURCE_STATES initial_state, bool use_esram, ComPtr<ID3D12Resource>& out_resurce)
	{
		Memory::StackTag tag(Memory::Tag::Device);

		const AllocationInfoD3D12::ResourceType resource_type = GetResourceType(type);

		D3D12MA::ALLOCATION_DESC allocation_desc = {};
		allocation_desc.HeapType = heap_props.Type;
		allocation_desc.CustomPool = nullptr;
		allocation_desc.ExtraHeapFlags = D3D12_HEAP_FLAG_NONE;
		allocation_desc.Flags = D3D12MA::ALLOCATION_FLAG_NONE;

		if (heap_props.Type == D3D12_HEAP_TYPE_CUSTOM)
		{
			allocation_desc.CustomPool = GetPool(heap_props, GetHeapFlags(resource_type));
			allocation_desc.Flags = D3D12MA::ALLOCATION_FLAG_COMMITTED;
		}

		ComPtr<D3D12MA::Allocation> allocation;
		if (FAILED(allocator->CreateResource(&allocation_desc, &desc, initial_state, nullptr, &allocation, IID_GRAPHICS_PPV_ARGS(out_resurce.GetAddressOf()))))
			return nullptr;

		const AllocationInfoD3D12 info{ type, resource_type, heap_props, allocation->GetSize(), allocation };
		std::unique_ptr<Handle> handle = std::make_unique<Handle>(name, this, info, allocation->GetOffset());

		total_used_vram.fetch_add(info.GetSize());
		PROFILE_ONLY(stats.Add(handle->GetName(), info.size, info.GetType(), heap_props);)
		return handle;
	}

	void GPUAllocator::Free(const GPUAllocatorBase::Handle& handle)
	{
		auto info = handle.GetAllocationInfo();
		total_used_vram.fetch_sub(info.GetSize());
		PROFILE_ONLY(stats.Remove(handle.GetName(), info.GetSize(), info.GetType(), info.heap_props);)
	}

#if defined(PROFILING)
	std::vector<std::vector<std::string>> GPUAllocator::GetStats()
	{
		std::vector<std::vector<std::string>> out(3);
		stats.PrintProps(out[1]);

		D3D12MA::TotalStatistics total_stats;
		allocator->CalculateStatistics(&total_stats);
		
		out[0].push_back("Memory types (from D3D12MA):");
		out[0].push_back("- [ Total ]");
		stats.PrintStatInfo(total_stats.Total, out[0]);

		stats.PrintHeapTypesInfo(total_stats, out[0]);

		stats.PrintTypes(out[1]);
		stats.PrintConstants(out[1]);
		stats.PrintFiles(out[2]);
		return out;
	}

	size_t GPUAllocator::GetSize()
	{
		return total_used_vram.load();
	}
#endif

}