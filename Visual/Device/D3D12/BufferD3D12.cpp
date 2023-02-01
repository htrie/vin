
namespace Device
{

	class BufferD3D12
	{
	protected:
		IDevice* device = nullptr;
		size_t size = 0;
		unsigned count = 0;
		unsigned current = 0;
		std::unique_ptr<AllocatorD3D12::Handle> handle;
		ComPtr<ID3D12Resource> buffer;
		AllocatorD3D12::Type type;

		Memory::SmallVector<std::shared_ptr<AllocatorD3D12::Data>, 3, Memory::Tag::Device> datas;

		void Copy(MemoryDesc* memory_desc);

	public:
		BufferD3D12(Memory::DebugStringA<> name, IDevice* device, AllocatorD3D12::Type type, size_t size, UsageHint usage, Pool pool, MemoryDesc* memory_desc, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);
	
		void Lock(size_t OffsetToLock, size_t SizeToLock, void** ppbData, Device::Lock lock);
		void Unlock();
		
		size_t GetResourceSize() const 
		{
			auto desk = buffer->GetDesc();
			return desk.Width;
		}

		ID3D12Resource* GetResource() const { return buffer.Get(); }
		D3D12_GPU_VIRTUAL_ADDRESS GetGPUAddress() const { return buffer->GetGPUVirtualAddress(); }

		size_t GetSize() const { return size; }
	};

	class TexelBufferD3D12 : public TexelBuffer, public BufferD3D12 {
		DescriptorHeapD3D12::Handle unordered_access_view;
		DescriptorHeapD3D12::Handle shader_resource_view;

	public:
		TexelBufferD3D12(const Memory::DebugStringA<>& name, IDevice* device, size_t element_count, TexelFormat format, UsageHint Usage, Pool pool, MemoryDesc* pInitialData = nullptr, bool enable_gpu_write = true);

		void Lock(size_t OffsetToLock, size_t SizeToLock, void** ppbData, Device::Lock lock) override { BufferD3D12::Lock(OffsetToLock, SizeToLock, ppbData, lock); }
		void Unlock() override { BufferD3D12::Unlock(); }

		size_t GetSize() const override { return BufferD3D12::GetSize(); }

		D3D12_CPU_DESCRIPTOR_HANDLE GetUAV() const { return (D3D12_CPU_DESCRIPTOR_HANDLE)unordered_access_view; }
		D3D12_CPU_DESCRIPTOR_HANDLE GetSRV() const { return (D3D12_CPU_DESCRIPTOR_HANDLE)shader_resource_view; }
	};

	class ByteAddressBufferD3D12 : public ByteAddressBuffer, public BufferD3D12 {
		DescriptorHeapD3D12::Handle unordered_access_view;
		DescriptorHeapD3D12::Handle shader_resource_view;

	public:
		ByteAddressBufferD3D12(const Memory::DebugStringA<>& name, IDevice* device, size_t size, UsageHint Usage, Pool pool, MemoryDesc* pInitialData = nullptr, bool enable_gpu_write = true);

		void Lock(size_t OffsetToLock, size_t SizeToLock, void** ppbData, Device::Lock lock) override { BufferD3D12::Lock(OffsetToLock, SizeToLock, ppbData, lock); }
		void Unlock() override { BufferD3D12::Unlock(); }

		size_t GetSize() const override { return BufferD3D12::GetSize(); }

		D3D12_CPU_DESCRIPTOR_HANDLE GetUAV() const { return (D3D12_CPU_DESCRIPTOR_HANDLE)unordered_access_view; }
		D3D12_CPU_DESCRIPTOR_HANDLE GetSRV() const { return (D3D12_CPU_DESCRIPTOR_HANDLE)shader_resource_view; }
	};

	class StructuredBufferD3D12 : public StructuredBuffer, public BufferD3D12 {
		DescriptorHeapD3D12::Handle unordered_access_view;
		DescriptorHeapD3D12::Handle shader_resource_view;

	public:
		StructuredBufferD3D12(const Memory::DebugStringA<>& name, IDevice* device, size_t element_size, size_t element_count, UsageHint Usage, Pool pool, MemoryDesc* pInitialData = nullptr, bool enable_gpu_write = true);

		void Lock(size_t OffsetToLock, size_t SizeToLock, void** ppbData, Device::Lock lock) override { BufferD3D12::Lock(OffsetToLock, SizeToLock, ppbData, lock); }
		void Unlock() override { BufferD3D12::Unlock(); }

		size_t GetSize() const override { return BufferD3D12::GetSize(); }

		D3D12_CPU_DESCRIPTOR_HANDLE GetUAV() const { return (D3D12_CPU_DESCRIPTOR_HANDLE)unordered_access_view; }
		D3D12_CPU_DESCRIPTOR_HANDLE GetSRV() const { return (D3D12_CPU_DESCRIPTOR_HANDLE)shader_resource_view; }
	};

	BufferD3D12::BufferD3D12(Memory::DebugStringA<> name, IDevice* device, AllocatorD3D12::Type type, size_t size, UsageHint usage, Pool pool, MemoryDesc* memory_desc, D3D12_RESOURCE_FLAGS flags)
		: size(size)
		, device(device)
		, type(type)
	{
		auto* d3ddevice = static_cast<IDeviceD3D12*>(device)->GetDevice();
		const auto buffer_desk = CD3DX12_RESOURCE_DESC::Buffer(size, flags);
		auto allocation_info = d3ddevice->GetResourceAllocationInfo(0, 1, &buffer_desk);
		
		const auto initial_state = ConvertResourceStateD3D12(ResourceState::ShaderRead);
		handle = static_cast<IDeviceD3D12*>(device)->GetAllocator()->CreateResource(name, type, buffer_desk, CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), initial_state, false, buffer);
		
		if (!handle)
			throw ExceptionD3D12("BufferD3D12::BufferD3D12() CreatePlacedResourceX failed");

		IDeviceD3D12::DebugName(buffer, handle->GetName());

		const bool dynamic = ((unsigned)usage & ((unsigned)UsageHint::DYNAMIC | (unsigned)UsageHint::ATOMIC_COUNTER)) > 0;
		if (dynamic || pool == Pool::MANAGED_WITH_SYSTEMMEM || pool == Pool::SYSTEMMEM)
		{
			D3D12_HEAP_PROPERTIES heap_props = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

			// Using host cached memory if there a chance that memory will be read by the cpu
			if (pool == Pool::MANAGED_WITH_SYSTEMMEM || pool == Pool::SYSTEMMEM)
				heap_props = CD3DX12_HEAP_PROPERTIES(D3D12_CPU_PAGE_PROPERTY_WRITE_BACK, D3D12_MEMORY_POOL_L0, 0, 0);

			count = dynamic ? IDeviceD3D12::FRAME_QUEUE_COUNT : 1;
			for (unsigned i = 0; i < count; ++i)
			{
				datas.emplace_back(std::make_shared<AllocatorD3D12::Data>(Memory::DebugStringA<>("Buffer Data Lock ") + name, device, AllocatorD3D12::Type::Staging, allocation_info.SizeInBytes, heap_props));

				if (memory_desc && memory_desc->pSysMem)
				{
					void* map = datas.back()->GetMap();
					memcpy(map, memory_desc->pSysMem, size);
				}
			}
		}

		if (memory_desc && memory_desc->pSysMem)
			Copy(memory_desc);
	}
	
	void BufferD3D12::Lock(size_t OffsetToLock, size_t SizeToLock, void** ppbData, Device::Lock lock)
	{
		assert(count > 0 && "Buffer::Lock requires UsageHint::DYNAMIC or system-owned memory pool");
		assert(datas[current]);
		assert(OffsetToLock + SizeToLock <= size);

		if (lock == Device::Lock::DISCARD)
		{
			assert(count > 1 && "Lock::DISCARD requires UsageHint::DYNAMIC");
			current = (current + 1) % count;
		}

		*ppbData = datas[current]->GetMap() + OffsetToLock;
	}

	void BufferD3D12::Unlock()
	{
		BufferCopyD3D12 buffer_copy{ 0, 0, size };
		auto upload = std::make_unique<CopyDataToBufferLoadD3D12>(handle->GetName(), buffer, datas[current], buffer_copy);
		static_cast<IDeviceD3D12*>(device)->GetTransfer().Add(std::move(upload));
	}

	void BufferD3D12::Copy(MemoryDesc* memory_desc)
	{
		auto staging_data = std::make_shared<AllocatorD3D12::Data>(Memory::DebugStringA<>("Buffer Data Init ") + handle->GetName(), device, AllocatorD3D12::Type::Staging, size, CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD));

		void* map = staging_data->GetMap();
		memcpy(map, memory_desc->pSysMem, size);

		BufferCopyD3D12 copy{ 0, 0, size };

		auto upload = std::make_unique<CopyDataToBufferLoadD3D12>(handle->GetName(), buffer, staging_data, copy);

		static_cast<IDeviceD3D12*>(device)->GetTransfer().Add(std::move(upload));
	}

	TexelBufferD3D12::TexelBufferD3D12(const Memory::DebugStringA<>& name, IDevice* device, size_t element_count, TexelFormat format, UsageHint Usage, Pool pool, MemoryDesc* pInitialData, bool enable_gpu_write)
		: TexelBuffer(name)
		, BufferD3D12(name, device, AllocatorD3D12::Type::Texel, element_count* GetTexelFormatSize(format), Usage, pool, pInitialData, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
	{
		auto* device_D3D12 = static_cast<IDeviceD3D12*>(device);

		D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
		SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		SRVDesc.Format = GetDXGITexelFormat(format);
		SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		SRVDesc.Buffer.NumElements = (uint32_t)element_count;
		SRVDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

		shader_resource_view = device_D3D12->GetCBViewSRViewUAViewDescriptorHeap().Allocate();
		device_D3D12->GetDevice()->CreateShaderResourceView(buffer.Get(), &SRVDesc, (D3D12_CPU_DESCRIPTOR_HANDLE)shader_resource_view);

		D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
		UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		UAVDesc.Format = GetDXGITexelFormat(format);
		UAVDesc.Buffer.NumElements = (uint32_t)element_count;
		UAVDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

		unordered_access_view = device_D3D12->GetCBViewSRViewUAViewDescriptorHeap().Allocate();
		device_D3D12->GetDevice()->CreateUnorderedAccessView(buffer.Get(), nullptr, &UAVDesc, (D3D12_CPU_DESCRIPTOR_HANDLE)unordered_access_view);
	}

	ByteAddressBufferD3D12::ByteAddressBufferD3D12(const Memory::DebugStringA<>& name, IDevice* device, size_t size, UsageHint Usage, Pool pool, MemoryDesc* pInitialData, bool enable_gpu_write)
		: ByteAddressBuffer(name)
		, BufferD3D12(name, device, AllocatorD3D12::Type::ByteAddress, size, Usage, pool, pInitialData, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
	{
		auto* device_D3D12 = static_cast<IDeviceD3D12*>(device);

		D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
		SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		SRVDesc.Format = DXGI_FORMAT_R32_TYPELESS;
		SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		SRVDesc.Buffer.NumElements = (UINT)(size / 4);
		SRVDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;

		shader_resource_view = device_D3D12->GetCBViewSRViewUAViewDescriptorHeap().Allocate();
		device_D3D12->GetDevice()->CreateShaderResourceView(buffer.Get(), &SRVDesc, (D3D12_CPU_DESCRIPTOR_HANDLE)shader_resource_view);

		D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
		UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		UAVDesc.Format = DXGI_FORMAT_R32_TYPELESS;
		UAVDesc.Buffer.NumElements = (UINT)(size / 4);
		UAVDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;

		unordered_access_view = device_D3D12->GetCBViewSRViewUAViewDescriptorHeap().Allocate();
		device_D3D12->GetDevice()->CreateUnorderedAccessView(buffer.Get(), nullptr, &UAVDesc, (D3D12_CPU_DESCRIPTOR_HANDLE)unordered_access_view);
	}

	StructuredBufferD3D12::StructuredBufferD3D12(const Memory::DebugStringA<>& name, IDevice* device, size_t element_size, size_t element_count, UsageHint Usage, Pool pool, MemoryDesc* pInitialData, bool enable_gpu_write)
		: StructuredBuffer(name, element_count, element_size)
		, BufferD3D12(name, device, AllocatorD3D12::Type::Structured, element_size * element_count, Usage, pool, pInitialData, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
	{
		auto* device_D3D12 = static_cast<IDeviceD3D12*>(device);

		D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
		SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		SRVDesc.Format = DXGI_FORMAT_UNKNOWN;
		SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		SRVDesc.Buffer.NumElements = (uint32_t)element_count;
		SRVDesc.Buffer.StructureByteStride = (uint32_t)element_size;
		SRVDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

		shader_resource_view = device_D3D12->GetCBViewSRViewUAViewDescriptorHeap().Allocate();
		device_D3D12->GetDevice()->CreateShaderResourceView(buffer.Get(), &SRVDesc, (D3D12_CPU_DESCRIPTOR_HANDLE)shader_resource_view);

		D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
		UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		UAVDesc.Format = DXGI_FORMAT_UNKNOWN;
		UAVDesc.Buffer.CounterOffsetInBytes = 0;
		UAVDesc.Buffer.NumElements = (uint32_t)element_count;
		UAVDesc.Buffer.StructureByteStride = (uint32_t)element_size;
		UAVDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

		unordered_access_view = device_D3D12->GetCBViewSRViewUAViewDescriptorHeap().Allocate();
		device_D3D12->GetDevice()->CreateUnorderedAccessView(buffer.Get(), nullptr, &UAVDesc, (D3D12_CPU_DESCRIPTOR_HANDLE)unordered_access_view);
	}

	class ReadbackBufferD3D12
	{
	protected:
		IDeviceD3D12* device = nullptr;
		size_t size = 0;
		std::unique_ptr<AllocatorD3D12::Handle> handle;
		ComPtr<ID3D12Resource> buffer;
		uint8_t* map = nullptr;
		bool is_locked = false;

	public:
		ReadbackBufferD3D12(Memory::DebugStringA<> name, IDevice* device, size_t size)
			: size(size)
			, device(static_cast<IDeviceD3D12*>(device))
		{
			const auto buffer_desk = CD3DX12_RESOURCE_DESC::Buffer(size);
			auto allocation_info = this->device->GetDevice()->GetResourceAllocationInfo(0, 1, &buffer_desk);

			const auto initial_state = D3D12_RESOURCE_STATE_COPY_DEST;
			handle = static_cast<IDeviceD3D12*>(device)->GetAllocator()->CreateResource(name, AllocatorD3D12::Type::ByteAddress, buffer_desk, CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK), initial_state, false, buffer);

			if (!handle)
				throw ExceptionD3D12("BufferD3D12::BufferD3D12() CreatePlacedResourceX failed");

			D3D12_RANGE dataRange = { 0, size };
			buffer->Map(0, &dataRange, reinterpret_cast<void**>(&map));
		}

		~ReadbackBufferD3D12()
		{
			buffer->Unmap(0, &CD3DX12_RANGE(0, 0));
			assert(!is_locked);
		}

		void Lock(size_t OffsetToLock, size_t SizeToLock, void** ppbData)
		{
			assert(!is_locked);
			assert(OffsetToLock + SizeToLock <= size);

			*ppbData = map + OffsetToLock;
			is_locked = true;
		}

		void Unlock()
		{
			assert(is_locked);
			is_locked = false;
		}

		ID3D12Resource* GetResource() const { return buffer.Get(); }

		size_t GetSize() const { return size; }
	};
}
