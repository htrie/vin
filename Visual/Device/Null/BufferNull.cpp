
namespace Device
{

	class BufferNull
	{
	protected:
		IDevice* device = nullptr;
		size_t size = 0;
		unsigned count = 0;
		unsigned current = 0;
		std::vector<uint8_t> data;

	public:
		BufferNull(const Memory::DebugStringA<>& name, IDevice* device, size_t size, UsageHint usage, Pool pool, MemoryDesc* memory_desc);
	
		void Lock(size_t OffsetToLock, size_t SizeToLock, void** ppbData, Device::Lock lock);
		void Unlock();
		
		size_t GetSize() const { return size; }
		uint8_t* GetData() const { return (uint8_t *)data.data(); }
	};

	class TexelBufferNull : public TexelBuffer, public BufferNull {
	public:
		TexelBufferNull(const Memory::DebugStringA<>& name, IDevice* device, size_t element_count, TexelFormat format, UsageHint Usage, Pool pool, MemoryDesc* pInitialData = nullptr, bool enable_gpu_write = true);

		void Lock(size_t OffsetToLock, size_t SizeToLock, void** ppbData, Device::Lock lock) override { BufferNull::Lock(OffsetToLock, SizeToLock, ppbData, lock); }
		void Unlock() override { BufferNull::Unlock(); }

		size_t GetSize() const override { return BufferNull::GetSize(); }
	};

	class ByteAddressBufferNull : public ByteAddressBuffer, public BufferNull {
	public:
		ByteAddressBufferNull(const Memory::DebugStringA<>& name, IDevice* device, size_t size, UsageHint Usage, Pool pool, MemoryDesc* pInitialData = nullptr, bool enable_gpu_write = true);

		void Lock(size_t OffsetToLock, size_t SizeToLock, void** ppbData, Device::Lock lock) override { BufferNull::Lock(OffsetToLock, SizeToLock, ppbData, lock); }
		void Unlock() override { BufferNull::Unlock(); }

		size_t GetSize() const override { return BufferNull::GetSize(); }
	};

	class StructuredBufferNull : public StructuredBuffer, public BufferNull {
	public:
		StructuredBufferNull(const Memory::DebugStringA<>& name, IDevice* device, size_t element_size, size_t element_count, UsageHint Usage, Pool pool, MemoryDesc* pInitialData = nullptr, bool enable_gpu_write = true);

		void Lock(size_t OffsetToLock, size_t SizeToLock, void** ppbData, Device::Lock lock) override { BufferNull::Lock(OffsetToLock, SizeToLock, ppbData, lock); }
		void Unlock() override { BufferNull::Unlock(); }

		size_t GetSize() const override { return BufferNull::GetSize(); }
	};

	BufferNull::BufferNull(const Memory::DebugStringA<>& name, IDevice* device, size_t size, UsageHint usage, Pool pool, MemoryDesc* memory_desc)
	{
		data.resize(size);
	}
	
	void BufferNull::Lock(size_t OffsetToLock, size_t SizeToLock, void** ppbData, Device::Lock lock)
	{
		*ppbData = GetData();
	}

	void BufferNull::Unlock()
	{
	}

	TexelBufferNull::TexelBufferNull(const Memory::DebugStringA<>& name, IDevice* device, size_t element_count, TexelFormat format, UsageHint Usage, Pool pool, MemoryDesc* pInitialData, bool enable_gpu_write)
		: TexelBuffer(name)
		, BufferNull(name, device, element_count* GetTexelFormatSize(format), Usage, pool, pInitialData)
	{}

	ByteAddressBufferNull::ByteAddressBufferNull(const Memory::DebugStringA<>& name, IDevice* device, size_t size, UsageHint Usage, Pool pool, MemoryDesc* pInitialData, bool enable_gpu_write)
		: ByteAddressBuffer(name)
		, BufferNull(name, device, size, Usage, pool, pInitialData)
	{}

	StructuredBufferNull::StructuredBufferNull(const Memory::DebugStringA<>& name, IDevice* device, size_t element_size, size_t element_count, UsageHint Usage, Pool pool, MemoryDesc* pInitialData, bool enable_gpu_write)
		: StructuredBuffer(name, element_count, element_size)
		, BufferNull(name, device, element_size* element_count, Usage, pool, pInitialData)
	{}
}
