
namespace Device
{
	Handle<TexelBuffer> TexelBuffer::Create(const Memory::DebugStringA<>& name, IDevice* device, size_t element_count, TexelFormat format, UsageHint Usage, Pool pool, MemoryDesc* pInitialData, bool enable_gpu_write)
	{
		PROFILE_ONLY(Memory::StackTag tag(Memory::Tag::Buffer);)
		switch (IDevice::GetType())
		{
		#if defined(ENABLE_VULKAN)
		case Type::Vulkan: return Handle<TexelBuffer>(new TexelBufferVulkan(name, device, element_count, format, Usage, pool, pInitialData, enable_gpu_write));
		#endif
		#if defined(ENABLE_D3D11)
		case Type::DirectX11: return Handle<TexelBuffer>(new TexelBufferD3D11(name, device, element_count, format, Usage, pool, pInitialData, enable_gpu_write));
		#endif
		#if defined(ENABLE_D3D12)
		case Type::DirectX12: return Handle<TexelBuffer>(new TexelBufferD3D12(name, device, element_count, format, Usage, pool, pInitialData, enable_gpu_write));
		#endif
		#if defined(ENABLE_GNMX)
		case Type::GNMX: return Handle<TexelBuffer>(new TexelBufferGNMX(name, device, element_count, format, Usage, pool, pInitialData, enable_gpu_write));
		#endif
		#if defined(ENABLE_NULL)
		case Type::Null: return Handle<TexelBuffer>(new TexelBufferNull(name, device, element_count, format, Usage, pool, pInitialData, enable_gpu_write));
		#endif
		default: throw std::runtime_error("Unknow device type");
		}
	}

	Handle<ByteAddressBuffer> ByteAddressBuffer::Create(const Memory::DebugStringA<>& name, IDevice* device, size_t size, UsageHint Usage, Pool pool, MemoryDesc* pInitialData, bool enable_gpu_write)
	{
		PROFILE_ONLY(Memory::StackTag tag(Memory::Tag::Buffer);)
		switch (IDevice::GetType())
		{
		#if defined(ENABLE_VULKAN)
		case Type::Vulkan: return Handle<ByteAddressBuffer>(new ByteAddressBufferVulkan(name, device, size, Usage, pool, pInitialData, enable_gpu_write));
		#endif
		#if defined(ENABLE_D3D11)
		case Type::DirectX11: return Handle<ByteAddressBuffer>(new ByteAddressBufferD3D11(name, device, size, Usage, pool, pInitialData, enable_gpu_write));
		#endif
		#if defined(ENABLE_D3D12)
		case Type::DirectX12: return Handle<ByteAddressBuffer>(new ByteAddressBufferD3D12(name, device, size, Usage, pool, pInitialData, enable_gpu_write));
		#endif
		#if defined(ENABLE_GNMX)
		case Type::GNMX: return Handle<ByteAddressBuffer>(new ByteAddressBufferGNMX(name, device, size, Usage, pool, pInitialData, enable_gpu_write));
		#endif
		#if defined(ENABLE_NULL)
		case Type::Null: return Handle<ByteAddressBuffer>(new ByteAddressBufferNull(name, device, size, Usage, pool, pInitialData, enable_gpu_write));
		#endif
		default: throw std::runtime_error("Unknow device type");
		}
	}

	Handle<StructuredBuffer> StructuredBuffer::Create(const Memory::DebugStringA<>& name, IDevice* device, size_t element_size, size_t element_count, UsageHint Usage, Pool pool, MemoryDesc* pInitialData, bool enable_gpu_write)
	{
		PROFILE_ONLY(Memory::StackTag tag(Memory::Tag::Buffer);)
		switch (IDevice::GetType())
		{
		#if defined(ENABLE_VULKAN)
		case Type::Vulkan: return Handle<StructuredBuffer>(new StructuredBufferVulkan(name, device, element_size, element_count, Usage, pool, pInitialData, enable_gpu_write));
		#endif
		#if defined(ENABLE_D3D11)
		case Type::DirectX11: return Handle<StructuredBuffer>(new StructuredBufferD3D11(name, device, element_size, element_count, Usage, pool, pInitialData, enable_gpu_write));
		#endif
		#if defined(ENABLE_D3D12)
		case Type::DirectX12: return Handle<StructuredBuffer>(new StructuredBufferD3D12(name, device, element_size, element_count, Usage, pool, pInitialData, enable_gpu_write));
		#endif
		#if defined(ENABLE_GNMX)
		case Type::GNMX: return Handle<StructuredBuffer>(new StructuredBufferGNMX(name, device, element_size, element_count, Usage, pool, pInitialData, enable_gpu_write));
		#endif
		#if defined(ENABLE_NULL)
		case Type::Null: return Handle<StructuredBuffer>(new StructuredBufferNull(name, device, element_size, element_count, Usage, pool, pInitialData, enable_gpu_write));
		#endif
		default: throw std::runtime_error("Unknow device type");
		}
	}
}