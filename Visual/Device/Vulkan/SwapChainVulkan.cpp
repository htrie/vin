
namespace Device
{
	SwapChainVulkan::SwapChainVulkan(HWND hwnd, IDevice* _device, int _width, int _height, const bool fullscreen, const bool vsync, UINT Output)
		: device((IDeviceVulkan*)_device)
		, width((uint32_t)_width)
		, height((uint32_t)_height)
		, fullscreen(fullscreen)
		, vsync(vsync)
	{
		WindowDX* window = WindowDX::GetWindow();
	#if defined(WIN32)
		const auto surface_info = vk::Win32SurfaceCreateInfoKHR()
			.setHinstance(window->GetInstance())
			.setHwnd(hwnd);
		surface = device->GetInstance().createWin32SurfaceKHRUnique(surface_info);
	#elif defined(__APPLE__macosx)
		const auto surface_info = vk::MacOSSurfaceCreateInfoMVK()
			.setPView(window->GetInstance());
		surface = device->GetInstance().createMacOSSurfaceMVKUnique(surface_info);
	#elif defined(__APPLE__iphoneos)
		const auto surface_info = vk::IOSSurfaceCreateInfoMVK()
			.setPView(window->GetInstance());
		surface = device->GetInstance().createIOSSurfaceMVKUnique(surface_info);
	#else
		return; // Not supported
	#endif

		if (!device->GetPhysicalDevice().getSurfaceSupportKHR(device->GetGraphicsQueueFamilyIndex(), surface.get()))
			throw ExceptionVulkan("getSurfaceSupportKHR");

		InitSwapChain();
	}

	void SwapChainVulkan::InitSwapChain()
	{
		if (!surface)
			return;

		const auto surface_caps = device->GetPhysicalDevice().getSurfaceCapabilitiesKHR(surface.get());

		if(IDeviceVulkan::FRAME_QUEUE_COUNT < surface_caps.minImageCount || IDeviceVulkan::FRAME_QUEUE_COUNT > surface_caps.maxImageCount)
			throw ExceptionVulkan("unsupported backbuffer image count");

		vk::Extent2D extent = surface_caps.currentExtent;
		if (surface_caps.currentExtent.width == 0xFFFFFFFF) {
			extent = vk::Extent2D(
				Clamp(width, surface_caps.minImageExtent.width, surface_caps.maxImageExtent.width),
				Clamp(height, surface_caps.minImageExtent.height, surface_caps.maxImageExtent.height));
		}

		width = extent.width;
		height = extent.height;
		LOG_INFO(L"[VULKAN] SwapChain = " << std::to_wstring(width) << L"x" << std::to_wstring(height));

		const auto transform = surface_caps.supportedTransforms & vk::SurfaceTransformFlagBitsKHR::eIdentity ?
			vk::SurfaceTransformFlagBitsKHR::eIdentity : surface_caps.currentTransform;

		vk::CompositeAlphaFlagBitsKHR composite_alpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
		vk::CompositeAlphaFlagBitsKHR composite_alpha_flags[4] = {
			vk::CompositeAlphaFlagBitsKHR::eOpaque,
			vk::CompositeAlphaFlagBitsKHR::ePreMultiplied,
			vk::CompositeAlphaFlagBitsKHR::ePostMultiplied,
			vk::CompositeAlphaFlagBitsKHR::eInherit,
		};
		for (unsigned i = 0; i < sizeof(composite_alpha_flags); i++) {
			if (surface_caps.supportedCompositeAlpha & composite_alpha_flags[i]) {
				composite_alpha = composite_alpha_flags[i];
				break;
			}
		}

		auto swap_chain_info = vk::SwapchainCreateInfoKHR()
			.setSurface(surface.get())
			.setMinImageCount((uint32_t)IDeviceVulkan::FRAME_QUEUE_COUNT)
			.setImageFormat(GetBackbufferFormat())
			.setImageColorSpace(vk::ColorSpaceKHR::eSrgbNonlinear)
			.setImageExtent(extent)
			.setImageArrayLayers(1)
			.setImageUsage(vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eColorAttachment)
			.setImageSharingMode(vk::SharingMode::eExclusive)
			.setPreTransform(transform)
			.setCompositeAlpha(composite_alpha)
			.setPresentMode(GetPresentMode())
			.setClipped(true);

	#if defined(WIN32)
		vk::SurfaceFullScreenExclusiveInfoEXT fullscreen_info(fullscreen ? vk::FullScreenExclusiveEXT::eAllowed : vk::FullScreenExclusiveEXT::eDisallowed);
		if (device->SupportsFullscreen())
			swap_chain_info.setPNext(&fullscreen_info);
	#endif

		render_target.reset();
		swap_chain.reset(); // Always destroy old swap chain before attempting to create new one

		swap_chain = device->GetDevice().createSwapchainKHRUnique(swap_chain_info);
		device->DebugName((uint64_t)swap_chain.get().operator VkSwapchainKHR(), VK_DEBUG_REPORT_OBJECT_TYPE_SWAPCHAIN_KHR_EXT, "Swap Chain");

		render_target = std::make_unique<RenderTargetVulkan>("Swap Chain Render Target", device, this);

		for (unsigned i = 0; i < IDeviceVulkan::FRAME_QUEUE_COUNT; ++i)
		{
			acquired_semaphores[i] = device->GetDevice().createSemaphoreUnique(vk::SemaphoreCreateInfo());
			device->DebugName((uint64_t)acquired_semaphores[i].get().operator VkSemaphore(), VK_DEBUG_REPORT_OBJECT_TYPE_SEMAPHORE_EXT, "Acquired Semaphore " + std::to_string(i));
			rendered_semaphores[i] = device->GetDevice().createSemaphoreUnique(vk::SemaphoreCreateInfo());
			device->DebugName((uint64_t)rendered_semaphores[i].get().operator VkSemaphore(), VK_DEBUG_REPORT_OBJECT_TYPE_SEMAPHORE_EXT, "Rendered Semaphore " + std::to_string(i));
		}
	}

	vk::Format SwapChainVulkan::GetBackbufferFormat() const
	{
		const auto preferred_formats = device->GetPhysicalDevice().getSurfaceFormatsKHR(surface.get());
		if (preferred_formats.size() == 0)
			throw ExceptionVulkan("getSurfaceFormatsKHR");

		auto backbuffer_format = vk::Format::eUndefined;
		for (const auto& preferred_format : preferred_formats) {
			if (preferred_format.format == vk::Format::eB8G8R8A8Srgb) { // Same as main render target.
				backbuffer_format = preferred_format.format;
				break;
			}
		}
		if (backbuffer_format == vk::Format::eUndefined)
			throw ExceptionVulkan("eB8G8R8A8Srgb not a preferred format");

		return backbuffer_format;
	}

	vk::PresentModeKHR SwapChainVulkan::GetPresentMode() const
	{
	#if defined(__APPLE__iphoneos)
		const auto present_mode = vk::PresentModeKHR::eImmediate;
	#else
		const auto present_modes = device->GetPhysicalDevice().getSurfacePresentModesKHR(surface.get());
		if (present_modes.size() == 0)
			throw ExceptionVulkan("getSurfacePresentModesKHR");

		auto present_mode = vk::PresentModeKHR::eFifo; // This is the only value of presentMode that is required to be supported.

		if (Contains(present_modes, vk::PresentModeKHR::eMailbox))
			present_mode = vk::PresentModeKHR::eMailbox;

		if (!vsync)
		{
			if (Contains(present_modes, vk::PresentModeKHR::eFifoRelaxed))
				present_mode = vk::PresentModeKHR::eFifoRelaxed;

			if (Contains(present_modes, vk::PresentModeKHR::eImmediate))
				present_mode = vk::PresentModeKHR::eImmediate;
		}
	#endif

		LOG_INFO(L"[VULKAN] Present mode = " << StringToWstring(vk::to_string(present_mode)));

		return present_mode;
	}

	void SwapChainVulkan::Recreate(HWND hwnd, IDevice* device, int _width, int _height, const bool _fullscreen, const bool _vsync, UINT Output)
	{
		fullscreen = _fullscreen;
		vsync = _vsync;
		width = (uint32_t)_width;
		height = (uint32_t)_height;
		InitSwapChain();
	}

	void SwapChainVulkan::Resize(int _width, int _height)
	{
		if ((uint32_t)_width != width || (uint32_t)_height != height)
		{
			width = (uint32_t)_width;
			height = (uint32_t)_height;
			InitSwapChain();
		}
	}

	bool SwapChainVulkan::Present()
	{
		if (!swap_chain)
			return false;

		Memory::SmallVector<vk::Semaphore, 1, Memory::Tag::Device> wait_semaphores;
		wait_semaphores.emplace_back(GetRenderedSemaphore(device->GetBufferIndex()));

		const auto present_info = vk::PresentInfoKHR()
			.setWaitSemaphoreCount((uint32_t)wait_semaphores.size())
			.setPWaitSemaphores(wait_semaphores.data())
			.setSwapchainCount(1)
			.setPSwapchains(&swap_chain.get())
			.setPImageIndices(&image_index);
		const auto result = device->GetGraphicsQueue().presentKHR(present_info);

		if (result != vk::Result::eSuccess)
		{
			LOG_INFO(L"[VULKAN] presentKHR: " << StringToWstring(to_string(result)));
			return false;
		}

		return true;
	}

	void SwapChainVulkan::WaitForBackbuffer(unsigned buffer_index)
	{
		if (!swap_chain)
			return;

		image_index = device->GetDevice().acquireNextImageKHR(swap_chain.get(), std::numeric_limits<uint64_t>::max(), GetAcquiredSemaphore(buffer_index), vk::Fence()).value;
	}
}
