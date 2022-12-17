namespace Device
{

	template<typename AllocationInfo>
	std::string GPUAllocatorBase<AllocationInfo>::Stats::TypeName(Type type)
	{
		switch (type)
		{
		case Type::RenderTarget: return "RenderTarget";
		case Type::Texture: return "Texture";
		case Type::Index: return "Index";
		case Type::Vertex: return "Vertex";
		case Type::Uniform: return "Uniform";
		case Type::Staging: return "Staging";
		case Type::Structured: return "Structured";
		case Type::Texel: return "Texel";
		case Type::ByteAddress: return "ByteAddress";
		default: throw std::runtime_error("Unknown allocation type");
		};
	}

	template<typename AllocationInfo>
	GPUAllocatorBase<AllocationInfo>::Stats::Stats()
	{
		global_constants_size.store(0);
		max_global_constants_size.store(0);
		peak_global_constants_size.store(0);

		for (auto& count : type_counts) count.store(0);
		for (auto& size : type_sizes) size.store(0);
		for (auto& count : peak_type_counts) count.store(0);
		for (auto& size : peak_type_sizes) size.store(0);
	}

	template<typename AllocationInfo>
	void GPUAllocatorBase<AllocationInfo>::Stats::GlobalConstantsSize(size_t size, size_t max_size)
	{
		global_constants_size.store(size);
		max_global_constants_size.store(max_size);
		peak_global_constants_size.store(std::max(peak_global_constants_size, global_constants_size));
	}

	template<typename AllocationInfo>
	void GPUAllocatorBase<AllocationInfo>::Stats::Add(const Memory::DebugStringA<>& name, size_t size, AllocationInfoType type)
	{
		type_counts[type]++;
		type_sizes[type] += size;
		peak_type_counts[type].store(std::max(peak_type_counts[type], type_counts[type]));
		peak_type_sizes[type].store(std::max(peak_type_sizes[type], type_sizes[type]));
		
		{
			Memory::Lock lock(file_mutex);
			file_sizes[name].count++;
			file_sizes[name].size += size;
		}
	}

	template<typename AllocationInfo>
	void GPUAllocatorBase<AllocationInfo>::Stats::Remove(const Memory::DebugStringA<>& name, size_t size, AllocationInfoType type)
	{
		type_counts[type]--;
		type_sizes[type] -= size;

		{
			Memory::Lock lock(file_mutex);
			file_sizes[name].count--;
			file_sizes[name].size -= size;
		}
	}

	template<typename AllocationInfo>
	void GPUAllocatorBase<AllocationInfo>::Stats::PrintConstants(std::vector<std::string>& lines)
	{
		lines.push_back((std::string("Constants: ") +
			" = " + Memory::ReadableSize(global_constants_size) + " / " + Memory::ReadableSize(max_global_constants_size) +
			"   Peak  " + Memory::ReadableSize(peak_global_constants_size) +
			"\n").c_str());
	}

	template<typename AllocationInfo>
	void GPUAllocatorBase<AllocationInfo>::Stats::PrintTypes(std::vector<std::string>& lines)
	{
		lines.push_back("Types:\n");
		for (unsigned i = 0; i < AllocationInfoType::Count; ++i)
		{
			lines.push_back((std::string("- ") + TypeName((AllocationInfoType)i) +
				" = " + std::to_string(type_counts[i]) + ": " + Memory::ReadableSize(type_sizes[i]) +
				"   Peak  " + std::to_string(peak_type_counts[i]) + ": " + Memory::ReadableSize(peak_type_sizes[i]) +
				"\n").c_str());
		}
	}

	template<typename AllocationInfo>
	void GPUAllocatorBase<AllocationInfo>::Stats::PrintFiles(std::vector<std::string>& lines)
	{
		lines.push_back("Files (Top 50 usage > 1MB):\n");

		std::vector<std::pair<std::string, FileSize>> files;
		files.reserve(64);
		{
			Memory::Lock lock(file_mutex);
			for (auto& iter : file_sizes)
			{
				if (iter.second.size > 1024 * 1024)
					files.push_back({ iter.first.c_str(), iter.second });
			}
		}

		std::sort(files.begin(), files.end(), [](auto& left, auto& right) { return left.second.size > right.second.size; });

		unsigned count = 0;
		for (auto& iter : files)
		{
			if (count++ > 50)
				break;
			lines.push_back((std::string("- ") + Memory::ReadableSize(iter.second.size) +
				" (" + std::to_string(iter.second.count) + ") " + iter.first + "\n").c_str());
		}
	}

	template<typename AllocationInfo>
	void GPUAllocatorBase<AllocationInfo>::Stats::LogLeaks()
	{
		Memory::Lock lock(file_mutex);

		const bool has_live_allocations = std::any_of(type_counts.begin(), type_counts.end(), [](const std::atomic_uint& value) { return value.load() > 0; });
		if (!has_live_allocations)
			return;

		for (auto& file : file_sizes)
		{
			if (file.second.count != 0)
			{
				LOG_CRIT(L"GPU Allocator leak: " << file.first.c_str() << L"(" << file.second.count << L")");
			}
		}
	}

	template<typename AllocationInfo>
	GPUAllocatorBase<AllocationInfo>::GPUAllocatorBase(IDevice* device)
		: device(device)
	{}

	template<typename AllocationInfo>
	GPUAllocatorBase<AllocationInfo>::Handle::~Handle()
	{
		allocator->Free(*this);
	}

	template<typename AllocationInfo>
	GPUAllocatorBase<AllocationInfo>::Handle::Handle(const Memory::DebugStringA<>& name, GPUAllocatorBase* allocator, AllocationInfo allocation_info, size_t offset)
		: name(name), allocator(allocator), allocation_info(allocation_info), offset(offset)
	{
	}

	template<typename AllocationInfo>
	VRAM GPUAllocatorBase<AllocationInfo>::GetVRAM() const
	{
		VRAM vram;
		vram.used = total_used_vram.load();
		vram.reserved = vram.used;
		return vram;
	}

}
