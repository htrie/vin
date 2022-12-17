namespace Device
{

	AllocationGNMX::~AllocationGNMX()
	{
		Clear();
	}

	void AllocationGNMX::Init(Memory::Tag tag, Memory::Type type, unsigned size, unsigned alignment)
	{
		this->tag = tag;
		this->type = type;
		this->size = size;
		this->alignment = alignment;

		size_t total_size = size;
		mem = (uint8_t*)Memory::Allocate(tag, total_size, alignment, type);
	}

	void AllocationGNMX::Clear()
	{
		if (mem)
		{
			Memory::Free(mem);
			mem = nullptr;
		}
		type = Memory::Type::GPU_WC;
		size = 0;
		alignment = 0;
	}


	AllocatorGNMX::ConstantBuffer::ConstantBuffer(const Memory::DebugStringA<>& name, IDevice* device, size_t max_size)
		: name(name), max_size(max_size), alignment((size_t)Gnm::kAlignmentOfBufferInBytes)
	{
		assert(BackBufferCount <= 2);
		for (unsigned i = 0; i < BackBufferCount; ++i)
		{
			datas[i].Init(Memory::Tag::ConstantBuffer, Memory::Type::CPU_WB, max_size, alignment);
		}
	}

	void AllocatorGNMX::ConstantBuffer::Reset()
	{
		offset = 0;
		frame_index++;
	}

	size_t AllocatorGNMX::ConstantBuffer::AllocateSlow(size_t size)
	{
		Memory::SpinLock lock(mutex);
		const auto current = offset;
		offset = Memory::AlignSize(offset + size, alignment);
		if (offset > max_size)
			throw ExceptionGNMX("Constant Buffer overflow");
		return current;
	}


	size_t AllocatorGNMX::ConstantBuffer::AllocateLocal(size_t size)
	{
		const auto aligned_size = Memory::AlignSize(size, alignment);
		if (aligned_size > LocalBlockSize)
			throw ExceptionGNMX("Constant Buffer allocation too big");

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

	std::pair<uint8_t*, size_t> AllocatorGNMX::ConstantBuffer::Allocate(size_t size, unsigned backbuffer_index)
	{
		const auto current = AllocateLocal(size);
		return { &datas[backbuffer_index].GetData()[current], current };
	}

}
