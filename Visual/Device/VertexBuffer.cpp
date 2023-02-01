
namespace Device
{

	Handle<VertexBuffer> VertexBuffer::Create(const Memory::DebugStringA<>& name, IDevice* device, UINT Length, UsageHint Usage, Pool pool, MemoryDesc* pInitialData)
	{
		PROFILE_ONLY(Memory::StackTag tag(Memory::Tag::VertexBuffer);)
		switch (IDevice::GetType())
		{
		#if defined(ENABLE_VULKAN)
		case Type::Vulkan: return Handle<VertexBuffer>(new VertexBufferVulkan(name, device, Length, Usage, pool, pInitialData));
		#endif
		#if defined(ENABLE_D3D11)
		case Type::DirectX11: return Handle<VertexBuffer>(new VertexBufferD3D11(name, device, Length, Usage, pool, pInitialData));
		#endif
		#if defined(ENABLE_D3D12)
		case Type::DirectX12: return Handle<VertexBuffer>(new VertexBufferD3D12(name, device, Length, Usage, pool, pInitialData));
		#endif
		#if defined(ENABLE_GNMX)
		case Type::GNMX: return Handle<VertexBuffer>(new VertexBufferGNMX(name, device, Length, Usage, pool, pInitialData));
		#endif
		#if defined(ENABLE_NULL)
		case Type::Null: return Handle<VertexBuffer>(new VertexBufferNull(name, device, Length, Usage, pool, pInitialData));
		#endif
		default: throw std::runtime_error("Unknow device type");
		}
	}

}