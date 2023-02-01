
namespace Device
{

	class VertexBufferD3D12 : public VertexBuffer, public BufferD3D12
	{
	public:
		VertexBufferD3D12(const Memory::DebugStringA<>& name, IDevice* device, size_t size, UsageHint usage, Pool pool, MemoryDesc* memory_desc);
		virtual ~VertexBufferD3D12();

		virtual void Lock(UINT OffsetToLock, UINT SizeToLock, void** ppbData, Device::Lock lock) final;
		virtual void Unlock() final;

		virtual UINT GetSize() const final;
	};

	VertexBufferD3D12::VertexBufferD3D12(const Memory::DebugStringA<>& name, IDevice* device, size_t size, UsageHint usage, Pool pool, MemoryDesc* memory_desc)
		: VertexBuffer(name), BufferD3D12(name, device, AllocatorD3D12::Type::Vertex, size, usage, pool, memory_desc)
	{}

	VertexBufferD3D12::~VertexBufferD3D12()
	{}

	void VertexBufferD3D12::Lock( UINT OffsetToLock, UINT SizeToLock, void** ppbData, Device::Lock lock)
	{
		BufferD3D12::Lock(OffsetToLock, SizeToLock, ppbData, lock);
	}

	void VertexBufferD3D12::Unlock()
	{
		BufferD3D12::Unlock();
	}

	UINT VertexBufferD3D12::GetSize() const
	{
		return (UINT)BufferD3D12::GetSize();
	}

}
