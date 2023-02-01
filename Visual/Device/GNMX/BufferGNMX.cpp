
namespace Device
{

	static OwnerGNMX buffer_owner("Buffer");

	Gnm::DataFormat GetGNMXTexelFormat(TexelFormat format)
	{
		switch (format)
		{
			case Device::TexelFormat::R8_UInt: return Gnm::kDataFormatR8Uint;
			case Device::TexelFormat::R8G8_UInt: return Gnm::kDataFormatR8G8Uint;
			case Device::TexelFormat::R8G8B8A8_UInt: return Gnm::kDataFormatR8G8B8A8Uint;
			case Device::TexelFormat::R16_UInt: return Gnm::kDataFormatR16Uint;
			case Device::TexelFormat::R16G16_UInt: return Gnm::kDataFormatR16G16Uint;
			case Device::TexelFormat::R16G16B16A16_UInt: return Gnm::kDataFormatR16G16B16A16Uint;
			case Device::TexelFormat::R32_UInt: return Gnm::kDataFormatR32Uint;
			case Device::TexelFormat::R32G32_UInt: return Gnm::kDataFormatR32G32Uint;
			case Device::TexelFormat::R32G32B32_UInt: return Gnm::kDataFormatR32G32B32Uint;
			case Device::TexelFormat::R32G32B32A32_UInt: return Gnm::kDataFormatR32G32B32A32Uint;
			case Device::TexelFormat::R8_UNorm: return Gnm::kDataFormatR8Unorm;
			case Device::TexelFormat::R8G8_UNorm: return Gnm::kDataFormatR8G8Unorm;
			case Device::TexelFormat::R8G8B8A8_UNorm: return Gnm::kDataFormatR8G8B8A8Unorm;
			case Device::TexelFormat::R16_UNorm: return Gnm::kDataFormatR16Unorm;
			case Device::TexelFormat::R16G16_UNorm: return Gnm::kDataFormatR16G16Unorm;
			case Device::TexelFormat::R16G16B16A16_UNorm: return Gnm::kDataFormatR16G16B16A16Unorm;
			case Device::TexelFormat::R8_SInt: return Gnm::kDataFormatR8Sint;
			case Device::TexelFormat::R8G8_SInt: return Gnm::kDataFormatR8G8Sint;
			case Device::TexelFormat::R8G8B8A8_SInt: return Gnm::kDataFormatR8G8B8A8Sint;
			case Device::TexelFormat::R16_SInt: return Gnm::kDataFormatR16Sint;
			case Device::TexelFormat::R16G16_SInt: return Gnm::kDataFormatR16G16Sint;
			case Device::TexelFormat::R16G16B16A16_SInt: return Gnm::kDataFormatR16G16B16A16Sint;
			case Device::TexelFormat::R32_SInt: return Gnm::kDataFormatR32Sint;
			case Device::TexelFormat::R32G32_SInt: return Gnm::kDataFormatR32G32Sint;
			case Device::TexelFormat::R32G32B32_SInt: return Gnm::kDataFormatR32G32B32Sint;
			case Device::TexelFormat::R32G32B32A32_SInt: return Gnm::kDataFormatR32G32B32A32Sint;
			case Device::TexelFormat::R8_SNorm: return Gnm::kDataFormatR8Snorm;
			case Device::TexelFormat::R8G8_SNorm: return Gnm::kDataFormatR8G8Snorm;
			case Device::TexelFormat::R8G8B8A8_SNorm: return Gnm::kDataFormatR8G8B8A8Snorm;
			case Device::TexelFormat::R16_SNorm: return Gnm::kDataFormatR16Snorm;
			case Device::TexelFormat::R16G16_SNorm: return Gnm::kDataFormatR16G16Snorm;
			case Device::TexelFormat::R16G16B16A16_SNorm: return Gnm::kDataFormatR16G16B16A16Snorm;
			case Device::TexelFormat::R16_Float: return Gnm::kDataFormatR16Float;
			case Device::TexelFormat::R16G16_Float: return Gnm::kDataFormatR16G16Float;
			case Device::TexelFormat::R16G16B16A16_Float: return Gnm::kDataFormatR16G16B16A16Float;
			case Device::TexelFormat::R32_Float: return Gnm::kDataFormatR32Float;
			case Device::TexelFormat::R32G32_Float: return Gnm::kDataFormatR32G32Float;
			case Device::TexelFormat::R32G32B32_Float: return Gnm::kDataFormatR32G32B32Float;
			case Device::TexelFormat::R32G32B32A32_Float: return Gnm::kDataFormatR32G32B32A32Float;
			default: return Gnm::kDataFormatInvalid;
		}
	}
	
	Memory::Type DetermineAllocationType(UsageHint usage_hint, Pool pool)
	{
		const bool render_target = ((unsigned)usage_hint & (unsigned)UsageHint::RENDERTARGET) > 0;
		const bool dynamic = ((unsigned)usage_hint & (unsigned)UsageHint::DYNAMIC) > 0;
		const bool write_only = ((unsigned)usage_hint & (unsigned)UsageHint::WRITEONLY) > 0;
		const bool immutable = ((unsigned)usage_hint & (unsigned)UsageHint::IMMUTABLE) > 0;
		const bool system_mem = (((unsigned)pool & (unsigned)Pool::MANAGED_WITH_SYSTEMMEM) > 0) || (((unsigned)pool & (unsigned)Pool::SYSTEMMEM) > 0);
		if (render_target && dynamic && write_only) // Backbuffer only.
			return Memory::Type::GPU_WC_CONTINUOUS;
		if (system_mem || (dynamic && !write_only && !immutable))
			return Memory::Type::CPU_WB;
		return Memory::Type::GPU_WC;
	}

	Gnm::ResourceMemoryType DetermineMemoryType(UsageHint usage_hint, bool enable_gpu_write = false)
	{
		const bool dynamic = ((unsigned)usage_hint & ((unsigned)UsageHint::DYNAMIC | (unsigned)UsageHint::ATOMIC_COUNTER)) > 0;
		const bool write_only = ((unsigned)usage_hint & (unsigned)UsageHint::WRITEONLY) > 0;
		const bool immutable = ((unsigned)usage_hint & (unsigned)UsageHint::IMMUTABLE) > 0;
		return dynamic && !write_only && !immutable ? Gnm::ResourceMemoryType::kResourceMemoryTypeUC : (enable_gpu_write ? Gnm::ResourceMemoryType::kResourceMemoryTypeGC : Gnm::ResourceMemoryType::kResourceMemoryTypeRO);
	}

	class BufferGNMX : public ResourceGNMX
	{
	protected:
		IDevice* device = nullptr;
		size_t size = 0;
		std::array<AllocationGNMX, BackBufferCount> datas;
		std::array<Gnm::Buffer, BackBufferCount> buffers;
		unsigned count = 0;
		unsigned current = 0;

	public:
		BufferGNMX(const Memory::DebugStringA<>& name, Memory::Tag tag, IDevice* device, size_t size, UsageHint usage, Pool pool, MemoryDesc* memory_desc);
		
		void InitAsTexelBuffer(Gnm::DataFormat format, unsigned element_count, UsageHint usage, bool enable_gpu_write);
		void InitAsStructuredBuffer(unsigned element_size, unsigned element_count, UsageHint usage, bool enable_gpu_write);
		void InitAsByteAddressBuffer(UsageHint usage, bool enable_gpu_write);
		
		void Lock(unsigned OffsetToLock, unsigned SizeToLock, void** ppbData, Device::Lock lock);
		void Unlock();
		
		size_t GetSize() const { return size; }
		uint8_t* GetData() const { return datas[current].GetData(); }
		Gnm::Buffer& GetBuffer() { return buffers[current]; }
	};

	class TexelBufferGNMX : public BufferGNMX, public TexelBuffer
	{
	public:
		TexelBufferGNMX(const Memory::DebugStringA<>& name, IDevice* device, size_t element_count, TexelFormat format, UsageHint Usage, Pool pool, MemoryDesc* pInitialData = nullptr, bool enable_gpu_write = true);

		void Lock(size_t OffsetToLock, size_t SizeToLock, void** ppbData, Device::Lock lock) override { BufferGNMX::Lock(OffsetToLock, SizeToLock, ppbData, lock); }
		void Unlock() override { BufferGNMX::Unlock(); }

		size_t GetSize() const override { return BufferGNMX::GetSize(); }
	};

	class ByteAddressBufferGNMX : public BufferGNMX, public ByteAddressBuffer
	{
	public:
		ByteAddressBufferGNMX(const Memory::DebugStringA<>& name, IDevice* device, size_t size, UsageHint Usage, Pool pool, MemoryDesc* pInitialData = nullptr, bool enable_gpu_write = true);

		void Lock(size_t OffsetToLock, size_t SizeToLock, void** ppbData, Device::Lock lock) override { BufferGNMX::Lock(OffsetToLock, SizeToLock, ppbData, lock); }
		void Unlock() override { BufferGNMX::Unlock(); }

		size_t GetSize() const override { return BufferGNMX::GetSize(); }
	};

	class StructuredBufferGNMX : public BufferGNMX, public StructuredBuffer
	{
	public:
		StructuredBufferGNMX(const Memory::DebugStringA<>& name, IDevice* device, size_t element_size, size_t element_count, UsageHint Usage, Pool pool, MemoryDesc* pInitialData = nullptr, bool enable_gpu_write = true);

		void Lock(size_t OffsetToLock, size_t SizeToLock, void** ppbData, Device::Lock lock) override { BufferGNMX::Lock(OffsetToLock, SizeToLock, ppbData, lock); }
		void Unlock() override { BufferGNMX::Unlock(); }

		size_t GetSize() const override { return BufferGNMX::GetSize(); }
	};


	BufferGNMX::BufferGNMX(const Memory::DebugStringA<>& name, Memory::Tag tag, IDevice* device, size_t size, UsageHint usage, Pool pool, MemoryDesc* memory_desc)
		: ResourceGNMX(buffer_owner.GetHandle()), device(device) , size(size)
	{
		const bool dynamic = ((unsigned)usage & ((unsigned)UsageHint::DYNAMIC | (unsigned)UsageHint::ATOMIC_COUNTER)) > 0;

		count = dynamic ? BackBufferCount : 1; // TODO: If DISCARD more than once per frame, then need more than double-buffering.

		for (unsigned i = 0; i < count; ++i)
		{
			datas[i].Init(tag, DetermineAllocationType(usage, pool), size, Gnm::kAlignmentOfBufferInBytes);

			Register(Gnm::kResourceTypeVertexBufferBaseAddress, datas[i].GetData(), size, name.c_str());

			if (memory_desc && memory_desc->pSysMem)
				memcpy(datas[i].GetData(), memory_desc->pSysMem, size);
		}
	}

	void BufferGNMX::InitAsTexelBuffer(Gnm::DataFormat format, unsigned element_count, UsageHint usage, bool enable_gpu_write)
	{
		for (unsigned i = 0; i < count; ++i)
		{
			buffers[i].initAsDataBuffer(datas[i].GetData(), format, element_count);
			buffers[i].setResourceMemoryType(DetermineMemoryType(usage, enable_gpu_write));
		}
	}

	void BufferGNMX::InitAsStructuredBuffer(unsigned element_size, unsigned element_count, UsageHint usage, bool enable_gpu_write)
	{
		for (unsigned i = 0; i < count; ++i)
		{
			buffers[i].initAsRegularBuffer(datas[i].GetData(), element_size, element_count);
			buffers[i].setResourceMemoryType(DetermineMemoryType(usage, enable_gpu_write));
		}
	}

	void BufferGNMX::InitAsByteAddressBuffer(UsageHint usage, bool enable_gpu_write)
	{
		for (unsigned i = 0; i < count; ++i)
		{
			buffers[i].initAsByteBuffer(datas[i].GetData(), size);
			buffers[i].setResourceMemoryType(DetermineMemoryType(usage, enable_gpu_write));
		}
	}

	void BufferGNMX::Lock(unsigned OffsetToLock, unsigned SizeToLock, void** ppbData, Device::Lock lock)
	{
		if (lock == Device::Lock::DISCARD)
		{
			assert(count > 1 && "Lock::DISCARD requires UsageHint::DYNAMIC");
			current = (current + 1) % count;
		}

		*ppbData = GetData() + OffsetToLock;
	}

	void BufferGNMX::Unlock()
	{
	}

	TexelBufferGNMX::TexelBufferGNMX(const Memory::DebugStringA<>& name, IDevice* device, size_t element_count, TexelFormat format, UsageHint Usage, Pool pool, MemoryDesc* pInitialData, bool enable_gpu_write)
		: TexelBuffer(name)
		, BufferGNMX(name, Memory::Tag::Resource, device, element_count* GetTexelFormatSize(format), Usage, pool, pInitialData) // TODO: Requires better selection of Memory::Tag
	{
		InitAsTexelBuffer(GetGNMXTexelFormat(format), element_count, Usage, enable_gpu_write);
	}

	ByteAddressBufferGNMX::ByteAddressBufferGNMX(const Memory::DebugStringA<>& name, IDevice* device, size_t size, UsageHint Usage, Pool pool, MemoryDesc* pInitialData, bool enable_gpu_write)
		: ByteAddressBuffer(name)
		, BufferGNMX(name, Memory::Tag::Resource, device, size, Usage, pool, pInitialData) // TODO: Requires better selection of Memory::Tag
	{
		InitAsByteAddressBuffer(Usage, enable_gpu_write);
	}

	StructuredBufferGNMX::StructuredBufferGNMX(const Memory::DebugStringA<>& name, IDevice* device, size_t element_size, size_t element_count, UsageHint Usage, Pool pool, MemoryDesc* pInitialData, bool enable_gpu_write)
		: StructuredBuffer(name, element_count, element_size)
		, BufferGNMX(name, Memory::Tag::Resource, device, element_size * element_count, Usage, pool, pInitialData) // TODO: Requires better selection of Memory::Tag
	{
		InitAsStructuredBuffer(element_size, element_count, Usage, enable_gpu_write);
		//TODO: Counter buffers need to be manually allocated in the GDS Memory
	}
}
