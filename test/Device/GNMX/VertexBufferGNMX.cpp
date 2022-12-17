
namespace Device
{

	class VertexBufferGNMX : public VertexBuffer, public BufferGNMX
	{
	public:
		VertexBufferGNMX(const Memory::DebugStringA<>& name, IDevice* device, size_t size, UsageHint usage, Pool pool, MemoryDesc* memory_desc);
		virtual ~VertexBufferGNMX();

		virtual void Lock(UINT OffsetToLock, UINT SizeToLock, void** ppbData, Device::Lock lock) final;
		virtual void Unlock() final;

		virtual UINT GetSize() const final;
	};

	VertexBufferGNMX::VertexBufferGNMX(const Memory::DebugStringA<>& name, IDevice* device, size_t size, UsageHint usage, Pool pool, MemoryDesc* memory_desc)
		: VertexBuffer(name), BufferGNMX(name, Memory::Tag::VertexBuffer, device, size, usage, pool, memory_desc)
	{}

	VertexBufferGNMX::~VertexBufferGNMX()
	{}

	void VertexBufferGNMX::Lock( UINT OffsetToLock, UINT SizeToLock, void** ppbData, Device::Lock lock)
	{
		BufferGNMX::Lock(OffsetToLock, SizeToLock, ppbData, lock);
	}

	void VertexBufferGNMX::Unlock()
	{
		BufferGNMX::Unlock();
	}

	UINT VertexBufferGNMX::GetSize() const
	{
		return (UINT)BufferGNMX::GetSize();
	}

}
