
namespace Device
{

	class VertexBufferNull : public VertexBuffer, public BufferNull
	{
	public:
		VertexBufferNull(const Memory::DebugStringA<>& name, IDevice* device, size_t size, UsageHint usage, Pool pool, MemoryDesc* memory_desc);
		virtual ~VertexBufferNull();

		virtual void Lock(UINT OffsetToLock, UINT SizeToLock, void** ppbData, Device::Lock lock) final;
		virtual void Unlock() final;

		virtual UINT GetSize() const final;
	};

	VertexBufferNull::VertexBufferNull(const Memory::DebugStringA<>& name, IDevice* device, size_t size, UsageHint usage, Pool pool, MemoryDesc* memory_desc)
		: VertexBuffer(name), BufferNull(name, device, size, usage, pool, memory_desc)
	{}

	VertexBufferNull::~VertexBufferNull()
	{}

	void VertexBufferNull::Lock( UINT OffsetToLock, UINT SizeToLock, void** ppbData, Device::Lock lock)
	{
		BufferNull::Lock(OffsetToLock, SizeToLock, ppbData, lock);
	}

	void VertexBufferNull::Unlock()
	{
		BufferNull::Unlock();
	}

	UINT VertexBufferNull::GetSize() const
	{
		return (UINT)BufferNull::GetSize();
	}

}
