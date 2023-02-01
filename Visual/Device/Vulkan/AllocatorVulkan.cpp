namespace Device
{
	void AllocatorVulkan::Stats::PrintMemoryType(const vk::PhysicalDeviceMemoryProperties& device_memory_props, uint32_t memory_type_index, std::vector<std::string>& lines)
	{
		lines.push_back((std::string("heap = ") + std::to_string(device_memory_props.memoryTypes[memory_type_index].heapIndex) +
			", properties " + vk::to_string(device_memory_props.memoryTypes[memory_type_index].propertyFlags)).c_str());
	}

	void AllocatorVulkan::Stats::PrintStatInfo(const VmaDetailedStatistics& stat, std::vector<std::string>& lines)
	{
		lines.push_back(std::string("- Allocations = ") + std::to_string(stat.statistics.allocationCount) + " ( " + Memory::ReadableSize(stat.statistics.allocationBytes)
			+ " ), Num Blocks = " + std::to_string(stat.statistics.blockCount));
		lines.push_back("- Unused size = " + Memory::ReadableSize(stat.statistics.blockBytes - stat.statistics.allocationBytes) + ", Unused range count = " + std::to_string(stat.unusedRangeCount));
	}

	void AllocatorVulkan::Stats::PrintMemoryTypes(IDevice* device, const VmaTotalStatistics& vma_stats, std::vector<std::string>& lines)
	{
		lines.push_back("Total");
		PrintStatInfo(vma_stats.total, lines);
		lines.push_back("\n");

		const auto& device_memory_props = static_cast<IDeviceVulkan*>(device)->GetDeviceMemoryProps();
		for (unsigned i = 0; i < device_memory_props.memoryTypeCount; i++)
		{
			PrintMemoryType(device_memory_props, i, lines);
			PrintStatInfo(vma_stats.memoryType[i], lines);
			lines.push_back("\n");
		}
	}

	void AllocatorVulkan::Stats::PrintHeaps(IDevice* device, const VmaTotalStatistics& vma_stats, std::vector<std::string>& lines) const
	{
		const auto& device_memory_props = static_cast<IDeviceVulkan*>(device)->GetDeviceMemoryProps();

		lines.push_back("Memory types:");
		for (unsigned i = 0; i < device_memory_props.memoryTypeCount; i++)
		{
			lines.push_back((std::string("- heap = ") + std::to_string(device_memory_props.memoryTypes[i].heapIndex) +
				", properties " + vk::to_string(device_memory_props.memoryTypes[i].propertyFlags)).c_str());
		}
		lines.push_back("\n");
		lines.push_back("Memory heaps:");
		for (unsigned i = 0; i < device_memory_props.memoryHeapCount; i++)
		{
			auto& stat = vma_stats.memoryHeap[i];
			lines.push_back((std::string("- size = ") + Memory::ReadableSize(device_memory_props.memoryHeaps[i].size) +
				", flags " + vk::to_string(device_memory_props.memoryHeaps[i].flags)).c_str());
			lines.push_back(std::string("- Allocations = ") + std::to_string(stat.statistics.allocationCount) + "( " + Memory::ReadableSize(stat.statistics.allocationBytes) + " )");
		}
	}

	void AllocatorVulkan::Stats::PrintProps(std::vector<std::string>& lines)
	{
		Memory::Lock lock(props_mutex);
		lines.push_back("Actual memory properties:");
		for (auto& iter : actual_props_stats)
		{
			lines.push_back((std::string("- ") + vk::to_string(iter.first) + " = " +
				std::to_string(iter.second.count) + ": " + Memory::ReadableSize(iter.second.size)).c_str());
		}
	}

	void AllocatorVulkan::Stats::Add(const Memory::DebugStringA<>& name, const AllocationInfoVulkan info)
	{
		GPUAllocatorBase::Stats::Add(name, info.GetSize(), info.GetType());

		VkMemoryPropertyFlags mem_flags;
		vmaGetMemoryTypeProperties(allocator, info.allocation_info.memoryType, &mem_flags);

		{
			Memory::Lock lock(props_mutex);
			actual_props_stats[(vk::MemoryPropertyFlags)mem_flags].count++;
			actual_props_stats[(vk::MemoryPropertyFlags)mem_flags].size += info.GetSize();
		}
	}

	void AllocatorVulkan::Stats::Remove(const Memory::DebugStringA<>& name, const AllocationInfoVulkan info)
	{
		GPUAllocatorBase::Stats::Remove(name, info.GetSize(), info.GetType());

		VkMemoryPropertyFlags mem_flags;
		vmaGetMemoryTypeProperties(allocator, info.allocation_info.memoryType, &mem_flags);

		{
			Memory::Lock lock(props_mutex);
			actual_props_stats[(vk::MemoryPropertyFlags)mem_flags].count--;
			actual_props_stats[(vk::MemoryPropertyFlags)mem_flags].size -= info.GetSize();
		}
	}

	namespace
	{
		void* VmaAllocationFunction(void* pUserData, size_t size, size_t alignment, VkSystemAllocationScope allocationScope)
		{
			return Memory::Allocate(Memory::Tag::Device, size, alignment);
		}

		void* VmaReallocationFunction(void* pUserData, void* pOriginal, size_t size, size_t alignment, VkSystemAllocationScope allocationScope)
		{
			return Memory::Reallocate(Memory::Tag::Device, pOriginal, size, alignment);
		}

		void VmaFreeFunction(void* pUserData, void* pMemory)
		{
			Memory::Free(pMemory);
		}
	}


	// On PC PoE2 character selection screen (Act2):
	//
	//   64MB: 720MB / 2.7GB,  45# /  46#
	//   32MB: 716MB / 2.7GB,  83# /  90#
	//   16MB: 720MB / 2.7GB, 149# / 169#
	//    8MB: 716MB / 2.5GB, 134# / 370#
	//    4MB: 720MB / 1.2GB, 210# / 539#
	//    2MB: 716MB / 960MB, 226# / 818#
	//    1MB: 720MB / 868MB, 280# / 1080#
	//
	// Use small size to make it easier to reclaim empty blocks (larger allocations will use their own dedicated block).
	//
#if defined(MOBILE)
	static const size_t block_size = 1 * Memory::MB;
#else
	static const size_t block_size = 2 * Memory::MB;
#endif


	AllocatorVulkan::AllocatorVulkan(IDevice* device) : GPUAllocatorBase<AllocationInfo>(device)
#if defined(PROFILING)
		, stats(allocator)
#endif
	{
		auto device_vulkan = static_cast<IDeviceVulkan*>(device);

#if defined(DEBUG_LOW_DEVICE_MEMORY)
		static std::array<VkDeviceSize, 256> max_vram;
		max_vram.fill(VK_WHOLE_SIZE);
		const auto& device_memory_props = static_cast<IDeviceVulkan*>(device)->GetDeviceMemoryProps();
		for (unsigned i = 0; i < device_memory_props.memoryHeapCount; i++)
		{
			if ((VkMemoryHeapFlags)(device_memory_props.memoryHeaps[i].flags) & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
			{
				max_vram[i] = 100 * Memory::MB;
			}
		}
#endif

		{
			allocation_callbacks.pfnAllocation = &VmaAllocationFunction;
			allocation_callbacks.pfnFree = &VmaFreeFunction;
			allocation_callbacks.pfnReallocation = &VmaReallocationFunction;

			VmaAllocatorCreateInfo allocatorInfo = {};
			allocatorInfo.physicalDevice = device_vulkan->GetPhysicalDevice();
			allocatorInfo.device = device_vulkan->GetDevice();
			allocatorInfo.instance = device_vulkan->GetInstance();
			allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_0;
			allocatorInfo.flags = device_vulkan->GetSupportsMemoryBudget() ? VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT : 0;
			allocatorInfo.pAllocationCallbacks = &allocation_callbacks;
			allocatorInfo.preferredLargeHeapBlockSize = block_size;
		#if defined(DEBUG_LOW_DEVICE_MEMORY)
			allocatorInfo.pHeapSizeLimit = max_vram.data();
		#endif
			const auto res = vmaCreateAllocator(&allocatorInfo, &allocator);
			if (res != VK_SUCCESS)
				throw ExceptionVulkan("Failed to create VMA Allocator");
		}
	}

	AllocatorVulkan::~AllocatorVulkan()
	{
		if (allocator)
			vmaDestroyAllocator(allocator);
		PrintLeaks();
	}

	VmaAllocationCreateInfo AllocatorVulkan::GetAllocationInfoForGPUOnly(const vk::PhysicalDeviceProperties& device_props)
	{
		VmaAllocationCreateInfo create_info = {};
		create_info.flags = VMA_ALLOCATION_CREATE_STRATEGY_BEST_FIT_BIT;
		create_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	#if defined(WIN32)
		if (device_props.deviceType == vk::PhysicalDeviceType::eIntegratedGpu) // Prefer using host memory when integrated GPU (AMD APUs declare way less device local memory, and it's the same physical memory anyway).
		{
			create_info.preferredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
			create_info.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
		}
	#endif
		return create_info;
	}

	VmaAllocationCreateInfo AllocatorVulkan::GetAllocationInfoForMappedMemory(VkMemoryPropertyFlags preferred_flags, VkMemoryPropertyFlags required_flags)
	{
		VmaAllocationCreateInfo create_info = {};
		create_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_STRATEGY_BEST_FIT_BIT;
		create_info.usage = VMA_MEMORY_USAGE_UNKNOWN;
		create_info.preferredFlags = preferred_flags;
		create_info.requiredFlags = required_flags;
		return create_info;
	}

	void AllocatorVulkan::PrintLeaks()
	{
		PROFILE_ONLY(stats.LogLeaks());
	}

	std::unique_ptr<AllocatorVulkan::Handle> AllocatorVulkan::AllocateForBuffer(const Memory::DebugStringA<>& name, Type type, VkBuffer buffer, VmaAllocationCreateInfo allocation_create_info)
	{
		VmaAllocation allocation;
		VmaAllocationInfo vma_allocation_info;
		const auto success = vmaAllocateMemoryForBuffer(allocator, buffer, &allocation_create_info, &allocation, &vma_allocation_info);
		if (success != VK_SUCCESS)
			return nullptr;

		VkMemoryPropertyFlags mem_props;
		vmaGetMemoryTypeProperties(allocator, vma_allocation_info.memoryType, &mem_props);

		const AllocationInfoVulkan info{ type, allocation, vma_allocation_info, vma_allocation_info.size, vk::MemoryPropertyFlags(mem_props) };
		auto handle = std::make_unique<Handle>(name, this, info, vma_allocation_info.offset);
		PROFILE_ONLY(stats.Add(handle->GetName(), handle->GetAllocationInfo());)
		total_used_vram.fetch_add(info.GetSize());

		if (vmaBindBufferMemory(allocator, allocation, buffer) != VK_SUCCESS)
			return nullptr;

		return handle;
	}

	std::unique_ptr<AllocatorVulkan::Handle> AllocatorVulkan::AllocateForImage(const Memory::DebugStringA<>& name, Type type, VkImage image, VmaAllocationCreateInfo allocation_create_info)
	{
		VmaAllocation allocation;
		VmaAllocationInfo vma_allocation_info;
		const auto success = vmaAllocateMemoryForImage(allocator, image, &allocation_create_info, &allocation, &vma_allocation_info);
		if (success != VK_SUCCESS)
			return nullptr;

		VkMemoryPropertyFlags mem_props;
		vmaGetMemoryTypeProperties(allocator, vma_allocation_info.memoryType, &mem_props);

		const AllocationInfoVulkan info{ type, allocation, vma_allocation_info, vma_allocation_info.size, vk::MemoryPropertyFlags(mem_props) };
		auto handle = std::make_unique<Handle>(name, this, info, vma_allocation_info.offset);
		PROFILE_ONLY(stats.Add(handle->GetName(), handle->GetAllocationInfo());)
		total_used_vram.fetch_add(info.GetSize());

		if (vmaBindImageMemory(allocator, allocation, image) != VK_SUCCESS)
			return nullptr;

		return handle;
	}

	void AllocatorVulkan::Free(const GPUAllocatorBase::Handle& handle)
	{
		PROFILE;
		vmaFreeMemory(allocator, handle.GetAllocationInfo().allocation);
		total_used_vram.fetch_sub(handle.GetAllocationInfo().GetSize());
		PROFILE_ONLY(stats.Remove(handle.GetName(), handle.GetAllocationInfo());)
	}

	VRAM AllocatorVulkan::GetVRAM() const
	{
		if (!allocator)
			return GPUAllocatorBase<AllocationInfoVulkan>::GetVRAM();

		const auto mem_props = static_cast<IDeviceVulkan*>(device)->GetDeviceMemoryProps();
		const auto heap = vk::MemoryHeapFlagBits::eDeviceLocal; // Check only device local to get true VRAM.

		Memory::Vector<VmaBudget, Memory::Tag::Device> vma_budgets(mem_props.memoryHeapCount);
		vmaGetHeapBudgets(allocator, vma_budgets.data());

		VRAM vram;
		for (uint32_t i = 0; i < mem_props.memoryHeapCount; i++)
		{
			if ((mem_props.memoryHeaps[i].flags & heap) == heap)
			{
				vram.allocation_count += vma_budgets[i].statistics.allocationCount;
				vram.block_count += vma_budgets[i].statistics.blockCount;
				vram.used += vma_budgets[i].statistics.allocationBytes;
				vram.reserved += vma_budgets[i].statistics.blockBytes;
			}
		}
		return vram;
	}

#if defined(PROFILING)
	std::vector<std::vector<std::string>> AllocatorVulkan::GetStats()
	{
		std::vector<std::vector<std::string>> out(3);

		VmaTotalStatistics vma_stats;
		vmaCalculateStatistics(allocator, &vma_stats);

		{
			std::vector<std::string>& lines = out[0];

			lines.push_back("\n");
			lines.push_back("\n");
			stats.PrintMemoryTypes(device, vma_stats, lines);
		}

		stats.PrintProps(out[1]);
		out[1].push_back("\n");
		stats.PrintHeaps(device, vma_stats, out[1]);
		out[1].push_back("\n");
		stats.PrintTypes(out[1]);
		out[1].push_back("\n");
		stats.PrintConstants(out[1]);
		out[1].push_back("\n");

		stats.PrintFiles(out[2]);

		return out;
	}

	size_t AllocatorVulkan::GetSize()
	{
		return total_used_vram.load();
	}
#endif


	AllocatorVulkan::Data::Data(const Memory::DebugStringA<>& name, IDevice* device, Type type, size_t size, vk::BufferUsageFlags usages, vk::MemoryPropertyFlags memory_props)
		: name(name), size(size)
	{
		assert(size > 0);

		const auto buf_info = vk::BufferCreateInfo()
			.setSize(size)
			.setUsage(usages)
			.setSharingMode(vk::SharingMode::eExclusive);
		buffer = static_cast<IDeviceVulkan*>(device)->GetDevice().createBufferUnique(buf_info);
		static_cast<IDeviceVulkan*>(device)->DebugName((uint64_t)buffer.get().operator VkBuffer(), VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT, name);

		mem_reqs = static_cast<IDeviceVulkan*>(device)->GetDevice().getBufferMemoryRequirements(buffer.get());
		const auto create_info = static_cast<IDeviceVulkan*>(device)->GetAllocator().GetAllocationInfoForMappedMemory((VkMemoryPropertyFlags)memory_props, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		handle = static_cast<IDeviceVulkan*>(device)->GetAllocator().AllocateForBuffer(name, type, *buffer, create_info);
		
		map = (uint8_t*)handle->GetAllocationInfo().allocation_info.pMappedData;
	}


	AllocatorVulkan::ConstantBuffer::ConstantBuffer(const Memory::DebugStringA<>& name, IDevice* device, size_t max_size)
		: name(name), max_size(max_size), alignment((size_t)static_cast<IDeviceVulkan*>(device)->GetDeviceProps().limits.minUniformBufferOffsetAlignment)
	{
		for (unsigned i = 0; i < IDeviceVulkan::FRAME_QUEUE_COUNT; ++i)
		{
			datas[i] = Data(name, device, Type::Uniform, max_size, vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCached | vk::MemoryPropertyFlagBits::eHostCoherent);
			maps[i] = datas[i].GetMap();
		}
	}

	void AllocatorVulkan::ConstantBuffer::Reset()
	{
		offset = 0;
		frame_index++;
	}

	size_t AllocatorVulkan::ConstantBuffer::AllocateSlow(size_t size)
	{
		Memory::SpinLock lock(mutex);
		const auto current = offset;
		offset = Memory::AlignSize(offset + size, alignment);
		if (offset > max_size)
			throw ExceptionVulkan("Constant Buffer overflow");
		return current;
	}


	size_t AllocatorVulkan::ConstantBuffer::AllocateLocal(size_t size)
	{
		const auto aligned_size = Memory::AlignSize(size, alignment);
		if (aligned_size > LocalBlockSize)
			throw ExceptionVulkan("Constant Buffer allocation too big");

		if (local_frame_index != frame_index)
		{
			local_offset = 0;
			local_available = 0;
			local_frame_index = frame_index;
		}

		if (aligned_size > local_available)
		{
			local_offset = AllocateSlow(LocalBlockSize);
			local_available = LocalBlockSize;
		}

		const auto current = local_offset;
		local_offset += aligned_size;
		local_available -= aligned_size;
		return current;
	}

	std::pair<uint8_t*, size_t> AllocatorVulkan::ConstantBuffer::Allocate(size_t size, unsigned backbuffer_index)
	{
		const auto current = AllocateLocal(size);
		return { &GetMap(backbuffer_index)[current], current };
	}
}
