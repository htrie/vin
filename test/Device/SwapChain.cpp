
namespace Device
{

	std::unique_ptr<SwapChain> SwapChain::Create(HWND hwnd, IDevice* device, int width, int height, const bool fullscreen, const bool vsync, UINT Output)
	{
		PROFILE_ONLY(Memory::StackTag tag(Memory::Tag::Device);)
		switch (IDevice::GetType())
		{
		#if defined(ENABLE_VULKAN)
		case Type::Vulkan: return std::unique_ptr<SwapChain>(new SwapChainVulkan(hwnd, device, width, height, fullscreen, vsync, Output));
		#endif
		#if defined(ENABLE_D3D11)
		case Type::DirectX11: return std::unique_ptr<SwapChain>(new SwapChainD3D11(hwnd, device, width, height, fullscreen, vsync, Output));
		#endif
		#if defined(ENABLE_D3D12)
		case Type::DirectX12: return std::unique_ptr<SwapChain>(new SwapChainD3D12(hwnd, device, width, height, fullscreen, vsync, Output));
		#endif
		#if defined(ENABLE_GNMX)
		case Type::GNMX: return std::unique_ptr<SwapChain>(new SwapChainGNMX(hwnd, device, width, height, fullscreen, vsync, Output));
		#endif
		#if defined(ENABLE_NULL)
		case Type::Null: return std::unique_ptr<SwapChain>(new SwapChainNull(hwnd, device, width, height, fullscreen, vsync, Output));
		#endif
		default: throw std::runtime_error("Unknow device type");
		}
	}

}
