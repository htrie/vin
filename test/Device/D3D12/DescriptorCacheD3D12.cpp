#pragma once

namespace Device
{

	DescriptorHeapD3D12::Handle::Handle(D3D12_CPU_DESCRIPTOR_HANDLE address, DescriptorHeapD3D12& heap)
		: heap(&heap)
		, address(address)
	{}

	DescriptorHeapD3D12::Handle::Handle(Handle&& other)
		: heap(other.heap)
		, address(other.address)
	{
		other.heap = nullptr;
		other.address = { 0 };
	}

	DescriptorHeapD3D12::Handle& DescriptorHeapD3D12::Handle::operator=(Handle&& other)
	{
		FreeHandle();
		heap = other.heap;
		address = other.address;
		other.heap = nullptr;
		other.address = { 0 };
		
		return *this;
	}

	DescriptorHeapD3D12::Handle::~Handle()
	{
		FreeHandle();
	}

	DescriptorHeapD3D12::DescriptorHeapD3D12(IDeviceD3D12* device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t descriptor_count)
		: free_range_allocator((uint64_t)0, (uint64_t)descriptor_count)
	{
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.Type = type;
		desc.NumDescriptors = descriptor_count;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE; // None as this heap is offline

		if (FAILED(device->GetDevice()->CreateDescriptorHeap(&desc, IID_GRAPHICS_PPV_ARGS(heap.GetAddressOf()))))
			throw ExceptionD3D12("CreateDescriptorHeap failed");

		descriptor_size = device->GetDevice()->GetDescriptorHandleIncrementSize(type);

		heap_start = heap->GetCPUDescriptorHandleForHeapStart();
	}

	std::optional<DescriptorHeapD3D12::Handle> DescriptorHeapD3D12::Allocate()
	{
		Memory::SpinLock lock(mutex);

		auto value = free_range_allocator.Allocate();

		if (!value)
			return std::nullopt;

		return Handle(CD3DX12_CPU_DESCRIPTOR_HANDLE(heap_start).Offset((INT)*value, (UINT)descriptor_size), *this);
	}

	void DescriptorHeapD3D12::Free(Handle& handle)
	{
		Memory::SpinLock lock(mutex);

		const uint64_t descriptor_index = ((uint64_t)handle.address.ptr - (uint64_t)heap_start.ptr) / descriptor_size;

		free_range_allocator.Free((uint32_t)descriptor_index);
	}


	DescriptorHeapCacheD3D12::DescriptorHeapCacheD3D12(IDeviceD3D12* device, D3D12_DESCRIPTOR_HEAP_TYPE heap_type, uint32_t heap_size)
		: heap_type(heap_type)
		, heap_size(heap_size)
		, device(device)
	{
		CreateHeap();
	}

	DescriptorHeapD3D12* DescriptorHeapCacheD3D12::CreateHeap()
	{
		Memory::WriteLock write_lock(mutex);

		heaps.emplace_back(device, heap_type, heap_size);
		return &heaps.back();
	}

	DescriptorHeapD3D12::Handle DescriptorHeapCacheD3D12::Allocate()
	{
		{
			Memory::ReadLock read_lock(mutex);
			for (auto& heap : heaps)
			{
				auto handle = heap.Allocate();
				if (handle)
					return std::move(*handle);
			}
		}

		auto heap = CreateHeap();
		return *heap->Allocate();
	}

	ShaderVisibleDescriptorHeapD3D12::ShaderVisibleDescriptorHeapD3D12(IDeviceD3D12* device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t descriptor_count)
	{
		this->descriptor_count = descriptor_count;

		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.Type = type;
		desc.NumDescriptors = (UINT)this->descriptor_count;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

		handle_size = device->GetDevice()->GetDescriptorHandleIncrementSize(type);

		if (FAILED(device->GetDevice()->CreateDescriptorHeap(&desc, IID_GRAPHICS_PPV_ARGS(dynamic_heap.GetAddressOf()))))
			throw ExceptionD3D12("CreateDescriptorHeap failed");
		IDeviceD3D12::DebugName(dynamic_heap, "Dynamic Descriptor Heap");

		gpu_start = GetHeap()->GetGPUDescriptorHandleForHeapStart();
		cpu_start = GetHeap()->GetCPUDescriptorHandleForHeapStart();
	}

	ShaderVisibleDescriptorHeapD3D12::~ShaderVisibleDescriptorHeapD3D12()
	{
	}

	ShaderVisibleRingBuffer::ShaderVisibleRingBuffer(IDeviceD3D12* device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t descriptor_count)
		: heap(device, type, descriptor_count)
		, ring_buffer(descriptor_count, IDeviceD3D12::FRAME_QUEUE_COUNT)
		, device(device)
	{
	}

	std::pair<CD3DX12_CPU_DESCRIPTOR_HANDLE, CD3DX12_GPU_DESCRIPTOR_HANDLE> ShaderVisibleRingBuffer::Allocate(uint32_t count)
	{
		Memory::SpinLock lock(mutex);
		const auto offset = (uint32_t)ring_buffer.Allocate(device->GetCurrentFrameFence(), device->GetLastCompletedFence(), count);
		return { heap.GetCPUAddress(offset), heap.GetGPUAddress(offset) };
	}

#if defined(PROFILING)
	void ShaderVisibleRingBuffer::AppendStats(std::vector<std::vector<std::string>>& strings)
	{
		auto& stats = ring_buffer.GetStats();
		auto& lines = strings[0];
		lines.push_back("");
		lines.push_back("Shader visible descriptors: ");
		lines.push_back("-    Current: " + std::to_string(stats.current_count) + " / " + std::to_string(ring_buffer.GetCapacity())
			+ " (" + std::to_string((uint32_t)(stats.current_count / (float)ring_buffer.GetCapacity() * 100)) + "%)");
		lines.push_back("-    Peak: " + std::to_string(stats.max_count) + " / " + std::to_string(ring_buffer.GetCapacity())
			+ " (" + std::to_string((uint32_t)(stats.max_count / (float)ring_buffer.GetCapacity() * 100)) + "%)");
	}
#endif

}