
namespace Device
{

	RenderTarget::RenderTarget(const Memory::DebugStringA<>& name)
		: Resource(name)
	{
		count++;
	}

	RenderTarget::~RenderTarget()
	{
		count--;
	};

	std::unique_ptr<RenderTarget> RenderTarget::Create(const Memory::DebugStringA<>& name, IDevice* device, SwapChain* swap_chain)
	{
		PROFILE_ONLY(Memory::StackTag tag(Memory::Tag::RenderTarget);)
		switch (IDevice::GetType())
		{
		#if defined(ENABLE_VULKAN)
		case Type::Vulkan: return std::make_unique<RenderTargetVulkan>(name, device, swap_chain);
		#endif
		#if defined(ENABLE_D3D11)
		case Type::DirectX11: return std::make_unique<RenderTargetD3D11>(name, device, swap_chain);
		#endif
		#if defined(ENABLE_D3D12)
		case Type::DirectX12: return std::make_unique<RenderTargetD3D12>(name, device, swap_chain);
		#endif
		#if defined(ENABLE_GNMX)
		case Type::GNMX: return std::make_unique<RenderTargetGNMX>(name, device, swap_chain);
		#endif
		#if defined(ENABLE_NULL)
		case Type::Null: return std::make_unique<RenderTargetNull>(name, device, swap_chain);
		#endif
		default: throw std::runtime_error("Unknow device type");
		}
	}

	std::unique_ptr<RenderTarget> RenderTarget::Create(const Memory::DebugStringA<>& name, IDevice* device, UINT width, UINT height, PixelFormat format_type, Pool pool, bool use_srgb, bool use_ESRAM, bool use_mipmaps)
	{
		PROFILE_ONLY(Memory::StackTag tag(Memory::Tag::RenderTarget);)
		switch (IDevice::GetType())
		{
		#if defined(ENABLE_VULKAN)
		case Type::Vulkan: return std::make_unique<RenderTargetVulkan>(name, device, width, height, format_type, pool, use_srgb, use_ESRAM, use_mipmaps);
		#endif
		#if defined(ENABLE_D3D11)
		case Type::DirectX11: return std::make_unique<RenderTargetD3D11>(name, device, width, height, format_type, pool, use_srgb, use_ESRAM, use_mipmaps);
		#endif
		#if defined(ENABLE_D3D12)
		case Type::DirectX12: return std::make_unique<RenderTargetD3D12>(name, device, width, height, format_type, pool, use_srgb, use_ESRAM, use_mipmaps);
		#endif
		#if defined(ENABLE_GNMX)
		case Type::GNMX: return std::make_unique<RenderTargetGNMX>(name, device, width, height, format_type, pool, use_srgb, use_ESRAM, use_mipmaps);
		#endif
		#if defined(ENABLE_NULL)
		case Type::Null: return std::make_unique<RenderTargetNull>(name, device, width, height, format_type, pool, use_srgb, use_ESRAM, use_mipmaps);
		#endif
		default: throw std::runtime_error("Unknow device type");
		}
	}

	std::unique_ptr<RenderTarget> RenderTarget::Create(const Memory::DebugStringA<>& name, IDevice* device, Device::Handle<Texture> texture, int level, int array_slice)
	{
		PROFILE_ONLY(Memory::StackTag tag(Memory::Tag::RenderTarget);)
		switch (IDevice::GetType())
		{
		#if defined(ENABLE_VULKAN)
		case Type::Vulkan: return std::make_unique<RenderTargetVulkan>(name, device, texture, level, array_slice);
		#endif
		#if defined(ENABLE_D3D11)
		case Type::DirectX11: return std::make_unique<RenderTargetD3D11>(name, device, texture, level, array_slice);
		#endif
		#if defined(ENABLE_D3D12)
		case Type::DirectX12: return std::make_unique<RenderTargetD3D12>(name, device, texture, level, array_slice);
		#endif
		#if defined(ENABLE_GNMX)
		case Type::GNMX: return std::make_unique<RenderTargetGNMX>(name, device, texture, level, array_slice);
		#endif
		#if defined(ENABLE_NULL)
		case Type::Null: return std::make_unique<RenderTargetNull>(name, device, texture, level, array_slice);
		#endif
		default: throw std::runtime_error("Unknow device type");
		}
	}

}
