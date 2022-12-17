
namespace Device
{

	class VertexBufferVulkan : public VertexBuffer, public BufferVulkan
	{
	public:
		VertexBufferVulkan(const Memory::DebugStringA<>& name, IDevice* device, size_t size, UsageHint usage, Pool pool, MemoryDesc* memory_desc);
		virtual ~VertexBufferVulkan();

		virtual void Lock(UINT OffsetToLock, UINT SizeToLock, void** ppbData, Device::Lock lock) final;
		virtual void Unlock() final;

		virtual UINT GetSize() const final;
	};

	VertexBufferVulkan::VertexBufferVulkan(const Memory::DebugStringA<>& name, IDevice* device, size_t size, UsageHint usage, Pool pool, MemoryDesc* memory_desc)
		: VertexBuffer(name), BufferVulkan(name, device, AllocatorVulkan::Type::Vertex, size, usage, pool, memory_desc, vk::BufferUsageFlagBits::eVertexBuffer)
	{}

	VertexBufferVulkan::~VertexBufferVulkan()
	{}

	void VertexBufferVulkan::Lock( UINT OffsetToLock, UINT SizeToLock, void** ppbData, Device::Lock lock)
	{
		BufferVulkan::Lock(OffsetToLock, SizeToLock, ppbData, lock);
	}

	void VertexBufferVulkan::Unlock()
	{
		BufferVulkan::Unlock();
	}

	UINT VertexBufferVulkan::GetSize() const
	{
		return (UINT)BufferVulkan::GetSize();
	}

}
