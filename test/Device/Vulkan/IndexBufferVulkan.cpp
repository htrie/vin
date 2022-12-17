
namespace Device
{
	vk::IndexType ConvertIndexFormatVulkan(IndexFormat index_format)
	{
		switch (index_format)
		{
		case IndexFormat::_16:	return vk::IndexType::eUint16;
		case IndexFormat::_32:	return vk::IndexType::eUint32;
		default:				throw ExceptionVulkan("Unsupported index format");
		}
	}

	unsigned ComputeIndexSizeVulkan(IndexFormat index_format)
	{
		switch (index_format)
		{
		case IndexFormat::_16:	return 2;
		case IndexFormat::_32:	return 4;
		default:				throw ExceptionVulkan("Unsupported index format");
		}
	}

	vk::Format ConvertFormatVulkan(IndexFormat index_format)
	{
		switch (index_format)
		{
		case IndexFormat::_16:	return vk::Format::eR16Uint;
		case IndexFormat::_32:	return vk::Format::eR32Uint;
		default:				throw ExceptionVulkan("Unsupported index format");
		}
	}

	PixelFormat ConvertIndexFormatToPixelFormatVulkan(IndexFormat index_format)
	{
		switch (index_format)
		{
		case IndexFormat::_16:	return PixelFormat::INDEX16;
		case IndexFormat::_32:	return PixelFormat::INDEX32;
		default:				throw ExceptionVulkan("Unsupported index format");
		}
	}

	class IndexBufferVulkan : public IndexBuffer, public BufferVulkan
	{
		IndexFormat index_format = IndexFormat::UNKNOWN;

	public:
		IndexBufferVulkan(const Memory::DebugStringA<>& name, IDevice* device, UINT size, UsageHint usage, IndexFormat format, Pool pool, MemoryDesc* memory_desc);
		virtual ~IndexBufferVulkan();

		virtual void Lock(UINT OffsetToLock, UINT SizeToLock, void** ppbData, Device::Lock lock) final;
		virtual void Unlock() final;

		virtual void GetDesc(IndexBufferDesc* pDesc) final;

		IndexFormat GetIndexFormat() const { return index_format; }
		unsigned GetIndexSize() const { return ComputeIndexSizeVulkan(index_format); }
	};

	IndexBufferVulkan::IndexBufferVulkan(const Memory::DebugStringA<>& name, IDevice* device, UINT size, UsageHint usage, IndexFormat format, Pool pool, MemoryDesc* memory_desc)
		: IndexBuffer(name)
		, BufferVulkan(name, device, AllocatorVulkan::Type::Index, size, usage, pool,memory_desc, vk::BufferUsageFlagBits::eIndexBuffer)
		, index_format(format)
	{
	}

	IndexBufferVulkan::~IndexBufferVulkan()
	{}


	void IndexBufferVulkan::Lock( UINT OffsetToLock, UINT SizeToLock, void** ppbData, Device::Lock lock)
	{
		BufferVulkan::Lock(OffsetToLock, SizeToLock, ppbData, lock);
	}

	void IndexBufferVulkan::Unlock()
	{
		BufferVulkan::Unlock();
	}

	void IndexBufferVulkan::GetDesc( IndexBufferDesc *pDesc )
	{
		pDesc->Format = ConvertIndexFormatToPixelFormatVulkan(GetIndexFormat());
		pDesc->Size = (UINT)GetSize();
	}

}
