
namespace Device
{

	std::unique_ptr<Profile> Profile::Create(IDevice* device)
	{
		PROFILE_ONLY(Memory::StackTag tag(Memory::Tag::Device);)
		switch (IDevice::GetType())
		{
		#if defined(ENABLE_VULKAN)
		case Type::Vulkan: return std::make_unique<ProfileVulkan>(device);
		#endif
		#if defined(ENABLE_D3D11)
		case Type::DirectX11: return std::make_unique<ProfileD3D11>(device);
		#endif
		#if defined(ENABLE_D3D12)
		case Type::DirectX12: return std::make_unique<ProfileD3D12>(device);
		#endif
		#if defined(ENABLE_GNMX)
		case Type::GNMX: return std::make_unique<ProfileGNMX>(device);
		#endif
		#if defined(ENABLE_NULL)
		case Type::Null: return std::make_unique<ProfileNull>(device);
		#endif
		default: throw std::runtime_error("Unknow device type");
		}
	}

}

