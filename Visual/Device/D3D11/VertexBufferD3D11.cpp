
namespace Device
{

	class VertexBufferD3D11 : public VertexBuffer, public BufferD3D11
	{
	public:
		VertexBufferD3D11(const Memory::DebugStringA<>& name, IDevice* device, UINT Length, UsageHint Usage, Pool pool, MemoryDesc* pInitialData);
		virtual ~VertexBufferD3D11();

		void Lock(UINT OffsetToLock, UINT SizeToLock, void** ppbData, Device::Lock lock) override { BufferD3D11::Lock(OffsetToLock, SizeToLock, ppbData, lock); }
		void Unlock() override { BufferD3D11::Unlock(); }

		UINT GetSize() const override { return BufferD3D11::GetSize(); }
	};

	VertexBufferD3D11::VertexBufferD3D11(const Memory::DebugStringA<>& name, IDevice* device, UINT Length, UsageHint Usage, Pool pool, MemoryDesc* pInitialData)
		: VertexBuffer(name), BufferD3D11(name, device, Length, D3D11_BIND_VERTEX_BUFFER, 0, Usage, pool, pInitialData)
	{
	}

	VertexBufferD3D11::~VertexBufferD3D11()
	{
	}

}
