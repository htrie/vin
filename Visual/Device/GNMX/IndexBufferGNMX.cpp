
namespace Device
{

	Gnm::IndexSize ConvertIndexSize(IndexFormat index_format)
	{
		switch (index_format)
		{
		case IndexFormat::_16:	return Gnm::kIndexSize16;
		case IndexFormat::_32:	return Gnm::kIndexSize32;
		default:				throw ExceptionGNMX("Unsupported");
		}
	}

	Gnm::DataFormat ConvertIndexFormat(IndexFormat index_format)
	{
		switch (index_format)
		{
		case IndexFormat::_16:	return Gnm::kDataFormatR16Uint;
		case IndexFormat::_32:	return Gnm::kDataFormatR32Uint;
		default:				throw ExceptionGNMX("Unsupported");
		}
	}

	unsigned ComputeIndexLength(IndexFormat index_format)
	{
		switch (index_format)
		{
		case IndexFormat::_16:	return 2;
		case IndexFormat::_32:	return 4;
		default:				throw ExceptionGNMX("Unsupported");
		}
	}

	PixelFormat ConvertIndexFormatToPixelFormat(IndexFormat index_format)
	{
		switch (index_format)
		{
		case IndexFormat::_16:	return PixelFormat::INDEX16;
		case IndexFormat::_32:	return PixelFormat::INDEX32;
		default:				throw ExceptionGNMX("Unsupported");
		}
	}

	class IndexBufferGNMX : public IndexBuffer, public BufferGNMX
	{
		IndexFormat index_format = IndexFormat::UNKNOWN;
		Gnm::IndexSize index_size = Gnm::kIndexSize16;
		unsigned index_length = 0; //TODO: Rename. Length/Size is terrible wording.

	public:
		IndexBufferGNMX(const Memory::DebugStringA<>& name, IDevice* device, UINT size, UsageHint usage, IndexFormat format, Pool pool, MemoryDesc* memory_desc);
		virtual ~IndexBufferGNMX();

		virtual void Lock(UINT OffsetToLock, UINT SizeToLock, void** ppbData, Device::Lock lock) final;
		virtual void Unlock() final;

		virtual void GetDesc(IndexBufferDesc* pDesc) final;

		IndexFormat GetIndexFormat() const { return index_format; }
		Gnm::IndexSize GetIndexSize() const { return index_size; }
		unsigned GetIndexLength() const { return index_length; }
	};

	IndexBufferGNMX::IndexBufferGNMX(const Memory::DebugStringA<>& name, IDevice* device, UINT size, UsageHint usage, IndexFormat format, Pool pool, MemoryDesc* memory_desc)
		: IndexBuffer(name), BufferGNMX(name, Memory::Tag::IndexBuffer, device, size, usage, pool, memory_desc)
		, index_format(format)
		, index_size(ConvertIndexSize(format))
		, index_length(ComputeIndexLength(format))
	{
	}

	IndexBufferGNMX::~IndexBufferGNMX()
	{}

	void IndexBufferGNMX::Lock( UINT OffsetToLock, UINT SizeToLock, void** ppbData, Device::Lock lock)
	{
		BufferGNMX::Lock(OffsetToLock, SizeToLock, ppbData, lock);
	}

	void IndexBufferGNMX::Unlock()
	{
		BufferGNMX::Unlock();
	}

	void IndexBufferGNMX::GetDesc( IndexBufferDesc *pDesc )
	{
		pDesc->Format = ConvertIndexFormatToPixelFormat(GetIndexFormat());
		pDesc->Size = GetSize();
	}

}
