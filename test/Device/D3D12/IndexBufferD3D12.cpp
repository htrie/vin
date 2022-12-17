
namespace Device
{

	uint32_t ComputeIndexSizeD3D12(IndexFormat index_format)
	{
		switch (index_format)
		{
		case IndexFormat::_16:	return 2;
		case IndexFormat::_32:	return 4;
		default:				throw ExceptionD3D12("Unsupported index format");
		}
	}

	class IndexBufferD3D12 : public IndexBuffer, public BufferD3D12
	{
		IndexFormat index_format = IndexFormat::UNKNOWN;

	public:
		IndexBufferD3D12(const Memory::DebugStringA<>& name, IDevice* device, UINT size, UsageHint usage, IndexFormat format, Pool pool, MemoryDesc* memory_desc);
		virtual ~IndexBufferD3D12();

		virtual void Lock(UINT OffsetToLock, UINT SizeToLock, void** ppbData, Device::Lock lock) final;
		virtual void Unlock() final;

		virtual void GetDesc(IndexBufferDesc* pDesc) final;

		IndexFormat GetFormat() const { return index_format; }
		unsigned GetIndexSize() const { return ComputeIndexSizeD3D12(index_format); }
	};

	IndexBufferD3D12::IndexBufferD3D12(const Memory::DebugStringA<>& name, IDevice* device, UINT size, UsageHint usage, IndexFormat format, Pool pool, MemoryDesc* memory_desc)
		: IndexBuffer(name) , BufferD3D12(name, device, AllocatorD3D12::Type::Index, size, usage, pool, memory_desc), index_format(format)
	{
	}

	IndexBufferD3D12::~IndexBufferD3D12()
	{}

	void IndexBufferD3D12::Lock( UINT OffsetToLock, UINT SizeToLock, void** ppbData, Device::Lock lock)
	{
		BufferD3D12::Lock(OffsetToLock, SizeToLock, ppbData, lock);
	}

	void IndexBufferD3D12::Unlock()
	{
		BufferD3D12::Unlock();
	}

	void IndexBufferD3D12::GetDesc( IndexBufferDesc *pDesc )
	{
		if (index_format == IndexFormat::_16)
			pDesc->Format = PixelFormat::INDEX16;
		else if (index_format == IndexFormat::_32)
			pDesc->Format = PixelFormat::INDEX32;
		else
			throw ExceptionD3D12("Invalid index format");
		pDesc->Size = (UINT)GetSize();
	}

}
