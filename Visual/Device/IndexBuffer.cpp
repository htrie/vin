
namespace Device
{

	Handle<IndexBuffer> IndexBuffer::Create(const Memory::DebugStringA<>& name, IDevice* device, UINT Length, UsageHint Usage, IndexFormat Format, Pool pool, MemoryDesc* pInitialData)
	{
		PROFILE_ONLY(Memory::StackTag tag(Memory::Tag::IndexBuffer);)
		switch (IDevice::GetType())
		{
		#if defined(ENABLE_VULKAN)
		case Type::Vulkan: return Handle<IndexBuffer>(new IndexBufferVulkan(name, device, Length, Usage, Format, pool, pInitialData));
		#endif
		#if defined(ENABLE_D3D11)
		case Type::DirectX11: return Handle<IndexBuffer>(new IndexBufferD3D11(name, device, Length, Usage, Format, pool, pInitialData));
		#endif
		#if defined(ENABLE_D3D12)
		case Type::DirectX12: return Handle<IndexBuffer>(new IndexBufferD3D12(name, device, Length, Usage, Format, pool, pInitialData));
		#endif
		#if defined(ENABLE_GNMX)
		case Type::GNMX: return Handle<IndexBuffer>(new IndexBufferGNMX(name, device, Length, Usage, Format, pool, pInitialData));
		#endif
		#if defined(ENABLE_NULL)
		case Type::Null: return Handle<IndexBuffer>(new IndexBufferNull(name, device, Length, Usage, Format, pool, pInitialData));
		#endif
		default: throw std::runtime_error("Unknow device type");
		}
	}

}
