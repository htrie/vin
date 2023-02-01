
namespace Device
{
	class IndexBufferNull : public IndexBuffer, public BufferNull
	{
		IndexFormat index_format = IndexFormat::UNKNOWN;
		unsigned index_length = 0; //TODO: Rename. Length/Size is terrible wording.

	public:
		IndexBufferNull(const Memory::DebugStringA<>& name, IDevice* device, UINT size, UsageHint usage, IndexFormat format, Pool pool, MemoryDesc* memory_desc);
		virtual ~IndexBufferNull();

		virtual void Lock(UINT OffsetToLock, UINT SizeToLock, void** ppbData, Device::Lock lock) final;
		virtual void Unlock() final;

		virtual void GetDesc(IndexBufferDesc* pDesc) final;

		IndexFormat GetIndexFormat() const { return index_format; }
		unsigned GetIndexLength() const { return index_length; }
	};

	IndexBufferNull::IndexBufferNull(const Memory::DebugStringA<>& name, IDevice* device, UINT size, UsageHint usage, IndexFormat format, Pool pool, MemoryDesc* memory_desc)
		: IndexBuffer(name) , BufferNull(name, device, size, usage, pool, memory_desc), index_format(format)
	{
	}

	IndexBufferNull::~IndexBufferNull()
	{}

	void IndexBufferNull::Lock( UINT OffsetToLock, UINT SizeToLock, void** ppbData, Device::Lock lock)
	{
		BufferNull::Lock(OffsetToLock, SizeToLock, ppbData, lock);
	}

	void IndexBufferNull::Unlock()
	{
		BufferNull::Unlock();
	}

	void IndexBufferNull::GetDesc( IndexBufferDesc *pDesc )
	{
	}

}
