
namespace Device
{
	class IndexBufferD3D11 : public IndexBuffer, public BufferD3D11
	{
	private:
		IndexFormat format; //only used when setting index buffer IASetIndexBuffer 

	public:
		IndexBufferD3D11(const Memory::DebugStringA<>& name, IDevice* device, UINT Length, UsageHint Usage, IndexFormat Format, Pool pool, MemoryDesc* pInitialData);
		virtual ~IndexBufferD3D11();

		void Lock(UINT OffsetToLock, UINT SizeToLock, void** ppbData, Device::Lock lock) override { BufferD3D11::Lock(OffsetToLock, SizeToLock, ppbData, lock); }
		void Unlock() override { BufferD3D11::Unlock(); }

		void GetDesc(IndexBufferDesc* pDesc) override;

		IndexFormat GetFormat() const { return format; }
	};

	IndexBufferD3D11::IndexBufferD3D11(const Memory::DebugStringA<>& name, IDevice* device, UINT Length, UsageHint Usage, IndexFormat Format, Pool pool, MemoryDesc* pInitialData)
		: IndexBuffer(name), BufferD3D11(name, device, Length, D3D11_BIND_INDEX_BUFFER, 0, Usage, pool, pInitialData)
		, format(Format)
	{
	}

	IndexBufferD3D11::~IndexBufferD3D11()
	{}

	void IndexBufferD3D11::GetDesc( IndexBufferDesc *pDesc )
	{
		if( format == IndexFormat::_16 )
			pDesc->Format =	PixelFormat::INDEX16;
		else if(format == IndexFormat::_32 )
			pDesc->Format = PixelFormat::INDEX32;
		else 
			throw ExceptionD3D11("Invalid index format");
		pDesc->Size = GetSize();
	}

}
