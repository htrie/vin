
namespace Device
{
	inline bool NeedsStaging(UsageHint usage, Pool pool)
	{
		const bool dynamic = ((unsigned)usage & ((unsigned)UsageHint::DYNAMIC | (unsigned)UsageHint::ATOMIC_COUNTER)) > 0;
		return dynamic || pool == Pool::MANAGED_WITH_SYSTEMMEM || pool == Pool::SYSTEMMEM;
	}

	vk::Format GetVKTexelFormat(TexelFormat format)
	{
		switch (format)
		{
			case Device::TexelFormat::R8_UInt:				return vk::Format::eR8Uint;
			case Device::TexelFormat::R8G8_UInt:			return vk::Format::eR8G8Uint;
			case Device::TexelFormat::R8G8B8A8_UInt:		return vk::Format::eR8G8B8A8Uint;
			case Device::TexelFormat::R16_UInt:				return vk::Format::eR16Uint;
			case Device::TexelFormat::R16G16_UInt:			return vk::Format::eR16G16Uint;
			case Device::TexelFormat::R16G16B16A16_UInt:	return vk::Format::eR16G16B16A16Uint;
			case Device::TexelFormat::R32_UInt:				return vk::Format::eR32Uint;
			case Device::TexelFormat::R32G32_UInt:			return vk::Format::eR32G32Uint;
			case Device::TexelFormat::R32G32B32_UInt:		return vk::Format::eR32G32B32Uint;
			case Device::TexelFormat::R32G32B32A32_UInt:	return vk::Format::eR32G32B32A32Uint;
			case Device::TexelFormat::R8_UNorm:				return vk::Format::eR8Unorm;
			case Device::TexelFormat::R8G8_UNorm:			return vk::Format::eR8G8Unorm;
			case Device::TexelFormat::R8G8B8A8_UNorm:		return vk::Format::eR8G8B8A8Unorm;
			case Device::TexelFormat::R16_UNorm:			return vk::Format::eR16Unorm;
			case Device::TexelFormat::R16G16_UNorm:			return vk::Format::eR16G16Unorm;
			case Device::TexelFormat::R16G16B16A16_UNorm:	return vk::Format::eR16G16B16A16Unorm;
			case Device::TexelFormat::R8_SInt:				return vk::Format::eR8Sint;
			case Device::TexelFormat::R8G8_SInt:			return vk::Format::eR8G8Sint;
			case Device::TexelFormat::R8G8B8A8_SInt:		return vk::Format::eR8G8B8A8Sint;
			case Device::TexelFormat::R16_SInt:				return vk::Format::eR16Sint;
			case Device::TexelFormat::R16G16_SInt:			return vk::Format::eR16G16Sint;
			case Device::TexelFormat::R16G16B16A16_SInt:	return vk::Format::eR16G16B16A16Sint;
			case Device::TexelFormat::R32_SInt:				return vk::Format::eR32Sint;
			case Device::TexelFormat::R32G32_SInt:			return vk::Format::eR32G32Sint;
			case Device::TexelFormat::R32G32B32_SInt:		return vk::Format::eR32G32B32Sint;
			case Device::TexelFormat::R32G32B32A32_SInt:	return vk::Format::eR32G32B32A32Sint;
			case Device::TexelFormat::R8_SNorm:				return vk::Format::eR8Snorm;
			case Device::TexelFormat::R8G8_SNorm:			return vk::Format::eR8G8Snorm;
			case Device::TexelFormat::R8G8B8A8_SNorm:		return vk::Format::eR8G8B8A8Snorm;
			case Device::TexelFormat::R16_SNorm:			return vk::Format::eR16Snorm;
			case Device::TexelFormat::R16G16_SNorm:			return vk::Format::eR16G16Snorm;
			case Device::TexelFormat::R16G16B16A16_SNorm:	return vk::Format::eR16G16B16A16Snorm;
			case Device::TexelFormat::R16_Float:			return vk::Format::eR16Sfloat;
			case Device::TexelFormat::R16G16_Float:			return vk::Format::eR16G16Sfloat;
			case Device::TexelFormat::R16G16B16A16_Float:	return vk::Format::eR16G16B16A16Sfloat;
			case Device::TexelFormat::R32_Float:			return vk::Format::eR32Sfloat;
			case Device::TexelFormat::R32G32_Float:			return vk::Format::eR32G32Sfloat;
			case Device::TexelFormat::R32G32B32_Float:		return vk::Format::eR32G32B32Sfloat;
			case Device::TexelFormat::R32G32B32A32_Float:	return vk::Format::eR32G32B32A32Sfloat;
			default:										return vk::Format::eUndefined;
		}
	}

	class BufferVulkan {
	private:
		Memory::DebugStringA<> name;
		IDevice* device = nullptr;
		size_t size = 0;
		VkDeviceSize allocation_size = 0;

		Memory::Array<std::unique_ptr<AllocatorVulkan::Handle>, 3> handles;
		Memory::Array<vk::UniqueBuffer, 3> buffers;
		Memory::Array<std::shared_ptr<AllocatorVulkan::Data>, 3> datas;

		unsigned host_count = 0;

	protected:
		unsigned remote_count = 0;

	public:
		BufferVulkan(const Memory::DebugStringA<>& name, IDevice* device, AllocatorVulkan::Type type, size_t size, UsageHint usage, Pool pool, MemoryDesc* memory_desc, vk::BufferUsageFlags usage_flags);

		void Lock(size_t OffsetToLock, size_t SizeToLock, void** ppbData, Device::Lock lock);
		void Unlock();

		void Copy(MemoryDesc* memory_desc);

		const vk::UniqueBuffer& GetBuffer(unsigned backbuffer_index) const { return buffers[backbuffer_index % remote_count]; }

		const Memory::DebugStringA<>& GetName() const { return name; }

		size_t GetSize() const { return size; }
	};

	class TexelBufferVulkan : public TexelBuffer, public BufferVulkan {
	private:
		Memory::Array<vk::UniqueBufferView, 3> buffer_views;

	public:
		TexelBufferVulkan(const Memory::DebugStringA<>& name, IDevice* device, size_t element_count, TexelFormat format, UsageHint Usage, Pool pool, MemoryDesc* pInitialData = nullptr, bool enable_gpu_write = true);

		const vk::UniqueBufferView& GetBufferView(unsigned backbuffer_index) const { return buffer_views[backbuffer_index % remote_count]; }

		void Lock(size_t OffsetToLock, size_t SizeToLock, void** ppbData, Device::Lock lock) override { BufferVulkan::Lock(OffsetToLock, SizeToLock, ppbData, lock); }
		void Unlock() override { BufferVulkan::Unlock(); }

		size_t GetSize() const override { return BufferVulkan::GetSize(); }
	};

	class ByteAddressBufferVulkan : public ByteAddressBuffer, public BufferVulkan {
	public:
		ByteAddressBufferVulkan(const Memory::DebugStringA<>& name, IDevice* device, size_t size, UsageHint Usage, Pool pool, MemoryDesc* pInitialData = nullptr, bool enable_gpu_write = true);

		void Lock(size_t OffsetToLock, size_t SizeToLock, void** ppbData, Device::Lock lock) override { BufferVulkan::Lock(OffsetToLock, SizeToLock, ppbData, lock); }
		void Unlock() override { BufferVulkan::Unlock(); }

		size_t GetSize() const override { return BufferVulkan::GetSize(); }
	};

	class StructuredBufferVulkan : public StructuredBuffer, public BufferVulkan {
	private:
		std::unique_ptr<ByteAddressBufferVulkan> counter_buffer;

	public:
		StructuredBufferVulkan(const Memory::DebugStringA<>& name, IDevice* device, size_t element_size, size_t element_count, UsageHint Usage, Pool pool, MemoryDesc* pInitialData = nullptr, bool enable_gpu_write = true);

		void Lock(size_t OffsetToLock, size_t SizeToLock, void** ppbData, Device::Lock lock) override { BufferVulkan::Lock(OffsetToLock, SizeToLock, ppbData, lock); }
		void Unlock() override { BufferVulkan::Unlock(); }

		size_t GetSize() const override { return BufferVulkan::GetSize(); }

		ByteAddressBufferVulkan* GetCounterBuffer() { return counter_buffer.get(); }
	};

	BufferVulkan::BufferVulkan(const Memory::DebugStringA<>& name, IDevice* device, AllocatorVulkan::Type type, size_t size, UsageHint usage, Pool pool, MemoryDesc* memory_desc, vk::BufferUsageFlags usage_flags)
		: name(name), device(device), size(size)
	{
		assert(size > 0);

		const auto buf_info = vk::BufferCreateInfo()
			.setSize(size)
			.setUsage(usage_flags | vk::BufferUsageFlagBits::eTransferDst)
			.setSharingMode(vk::SharingMode::eExclusive);

		const auto create_info = static_cast<IDeviceVulkan*>(device)->GetAllocator().GetAllocationInfoForGPUOnly(static_cast<IDeviceVulkan*>(device)->GetDeviceProps());

		const bool buffered = ((unsigned)usage & ((unsigned)UsageHint::BUFFERED | (unsigned)UsageHint::ATOMIC_COUNTER)) > 0;
		remote_count = buffered ? IDeviceVulkan::FRAME_QUEUE_COUNT : 1;
		for (unsigned i = 0; i < remote_count; ++i)
		{
			buffers.emplace_back(static_cast<IDeviceVulkan*>(device)->GetDevice().createBufferUnique(buf_info));
			static_cast<IDeviceVulkan*>(device)->DebugName((uint64_t)buffers.back().get().operator VkBuffer(), VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT, name);

			handles.emplace_back(static_cast<IDeviceVulkan*>(device)->GetAllocator().AllocateForBuffer(name, type, *buffers.back(), create_info));
		}

		allocation_size = handles.back()->GetAllocationInfo().GetSize();

		if (NeedsStaging(usage, pool))
		{
			const bool dynamic = ((unsigned)usage & ((unsigned)UsageHint::DYNAMIC | (unsigned)UsageHint::ATOMIC_COUNTER)) > 0;
			host_count = dynamic ? IDeviceVulkan::FRAME_QUEUE_COUNT : 1; // TODO: If DISCARD more than once per frame, then need more than N-buffering.
			for (unsigned i = 0; i < host_count; ++i)
			{
				datas.emplace_back(std::make_shared<AllocatorVulkan::Data>(Memory::DebugStringA<>("Staging ") + name, device, AllocatorVulkan::Type::Staging, allocation_size, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostCached | vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent));

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

	void BufferVulkan::Lock(size_t OffsetToLock, size_t SizeToLock, void** ppbData, Device::Lock lock)
	{
		const auto backbuffer_index = static_cast<IDeviceVulkan*>(device)->GetBufferIndex();

		assert(host_count > 0 && "Buffer::Lock requires UsageHint::DYNAMIC or system-owned memory pool");
		assert(datas[backbuffer_index % host_count]);
		assert(OffsetToLock + SizeToLock <= allocation_size);

		if (lock == Device::Lock::DISCARD)
		{
			assert(host_count > 1 && "Lock::DISCARD requires UsageHint::DYNAMIC");
		}

		*ppbData = datas[backbuffer_index % host_count]->GetMap() + OffsetToLock;
	}

	void BufferVulkan::Unlock()
	{
		const auto backbuffer_index = static_cast<IDeviceVulkan*>(device)->GetBufferIndex();

		const auto buffer_copy = vk::BufferCopy()
			.setSrcOffset(0)
			.setDstOffset(0)
			.setSize(size);

		auto upload = std::make_unique<CopyStaticDataToBufferLoadVulkan>(name, buffers[backbuffer_index % remote_count].get(), datas[backbuffer_index % host_count], buffer_copy);
		static_cast<IDeviceVulkan*>(device)->GetTransfer().Add(std::move(upload));
	}

	void BufferVulkan::Copy(MemoryDesc* memory_desc)
	{
		assert(remote_count == 1 && "Buffer::Copy requires single remote");

		auto staging_data = std::make_shared<AllocatorVulkan::Data>(Memory::DebugStringA<>("Buffer Data Init ") + name, device, AllocatorVulkan::Type::Staging, allocation_size, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

		void* map = staging_data->GetMap();
		memcpy(map, memory_desc->pSysMem, size);

		const auto copy = vk::BufferCopy()
			.setSrcOffset(0)
			.setDstOffset(0)
			.setSize(size);

		auto upload = std::make_unique<CopyStaticDataToBufferLoadVulkan>(name, buffers[0].get(), staging_data, copy);
		static_cast<IDeviceVulkan*>(device)->GetTransfer().Add(std::move(upload));
	}

	TexelBufferVulkan::TexelBufferVulkan(const Memory::DebugStringA<>& name, IDevice* device, size_t element_count, TexelFormat format, UsageHint Usage, Pool pool, MemoryDesc* pInitialData, bool enable_gpu_write)
		: TexelBuffer(name)
		, BufferVulkan(name, device, AllocatorVulkan::Type::Texel, element_count * GetTexelFormatSize(format), Usage, pool, pInitialData, vk::BufferUsageFlagBits::eUniformTexelBuffer | (enable_gpu_write ? vk::BufferUsageFlagBits::eStorageTexelBuffer : vk::BufferUsageFlagBits(0)))
	{
		const auto resource_format = GetVKTexelFormat(format);
		if (resource_format != vk::Format::eUndefined)
		{
			for (unsigned i = 0; i < remote_count; ++i)
			{
				const auto buf_view_info = vk::BufferViewCreateInfo()
					.setBuffer(BufferVulkan::GetBuffer(i).get())
					.setFormat(resource_format)
					.setOffset(0)
					.setRange(BufferVulkan::GetSize());

				buffer_views.emplace_back(static_cast<IDeviceVulkan*>(device)->GetDevice().createBufferViewUnique(buf_view_info));
				static_cast<IDeviceVulkan*>(device)->DebugName((uint64_t)buffer_views.back().get().operator VkBufferView(), VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_VIEW_EXT, name);
			}
		}
	}

	ByteAddressBufferVulkan::ByteAddressBufferVulkan(const Memory::DebugStringA<>& name, IDevice* device, size_t size, UsageHint Usage, Pool pool, MemoryDesc* pInitialData, bool enable_gpu_write)
		: ByteAddressBuffer(name)
		, BufferVulkan(name, device, AllocatorVulkan::Type::ByteAddress, size, Usage, pool, pInitialData, vk::BufferUsageFlagBits::eStorageBuffer)
	{}

	StructuredBufferVulkan::StructuredBufferVulkan(const Memory::DebugStringA<>& name, IDevice* device, size_t element_size, size_t element_count, UsageHint Usage, Pool pool, MemoryDesc* pInitialData, bool enable_gpu_write)
		: StructuredBuffer(name, element_count, element_size)
		, BufferVulkan(name, device, AllocatorVulkan::Type::Structured,element_size * element_count,Usage, pool, pInitialData, vk::BufferUsageFlagBits::eStorageBuffer)
	{
		if (enable_gpu_write)
		{
			uint32_t counter = 0;

			MemoryDesc initCounterData;
			initCounterData.pSysMem = &counter;
			initCounterData.SysMemPitch = sizeof(counter);
			initCounterData.SysMemSlicePitch = sizeof(counter);

			//counter_buffer = std::make_unique<ByteAddressBufferVulkan>(name + " Counter", device, 4, UsageHint::DEFAULT, Pool::DEFAULT, &initCounterData, true);
		}
	}
}
