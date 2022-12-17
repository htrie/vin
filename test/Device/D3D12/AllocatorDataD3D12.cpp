namespace Device::AllocatorD3D12
{
	Data::Data(Memory::DebugStringA<> name, IDevice* device, GPUAllocator::Type type, size_t size, D3D12_HEAP_PROPERTIES heap_props)
		: heap_props(heap_props)
		, size(size)
	{
		auto* d3ddevice = static_cast<IDeviceD3D12*>(device)->GetDevice();
		auto buffer_desk = CD3DX12_RESOURCE_DESC::Buffer(size, D3D12_RESOURCE_FLAG_NONE, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
		auto mem_reqs = d3ddevice->GetResourceAllocationInfo(0, 1, &buffer_desk);

		const bool is_upload = (heap_props.Type == D3D12_HEAP_TYPE_UPLOAD) || (heap_props.Type == D3D12_HEAP_TYPE_CUSTOM);
		const auto initial_state = is_upload ? D3D12_RESOURCE_STATE_GENERIC_READ : D3D12_RESOURCE_STATE_COMMON;
		handle = static_cast<IDeviceD3D12*>(device)->GetAllocator()->CreateResource(name, type, buffer_desk, heap_props, initial_state, false, buffer);

		if (!handle)
			throw ExceptionD3D12("GPUAllocator::Data::Data() CreateResource failed");

		if (is_upload)
		{
			buffer->Map(0, nullptr, reinterpret_cast<void**>(&map));
		}
	}

	Data::~Data()
	{
		const bool is_upload = heap_props.Type == D3D12_HEAP_TYPE_UPLOAD;
		if (buffer && is_upload)
		{
			buffer->Unmap(0, nullptr);
		}
	}

	ConstantBuffer::ConstantBuffer(const Memory::DebugStringA<>& name, IDeviceD3D12* device, size_t max_size)
		: name(name), max_size(max_size), alignment(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT)
	{
		for (uint32_t i = 0; i < IDeviceD3D12::FRAME_QUEUE_COUNT; ++i)
		{
			datas[i] = Data(name, device, GPUAllocator::Type::Uniform, max_size, CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD));
			maps[i] = datas[i].GetMap();
		}
	}

	void ConstantBuffer::Reset()
	{
		offset = 0;
		frame_index++;
	}

	size_t ConstantBuffer::AllocateSlow(size_t size)
	{
		Memory::SpinLock lock(mutex);
		const auto current = offset;
		offset = Memory::AlignSize(offset + size, alignment);
		if (offset > max_size)
			throw ExceptionD3D12("Constant Buffer overflow");
		return current;
	}


	size_t ConstantBuffer::AllocateLocal(size_t size)
	{
		const auto aligned_size = Memory::AlignSize(size, alignment);
		if (aligned_size > LocalBlockSize)
			throw ExceptionD3D12("Constant Buffer allocation too big");

		if (local_frame_index != frame_index)
		{
			local_offset = 0;
			local_available = 0;
			local_frame_index = (uint32_t)frame_index;
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

	std::pair<uint8_t*, size_t> ConstantBuffer::Allocate(size_t size, unsigned backbuffer_index)
	{
		const auto current = AllocateLocal(size);
		return { &GetMap(backbuffer_index)[current], current };
	}
}