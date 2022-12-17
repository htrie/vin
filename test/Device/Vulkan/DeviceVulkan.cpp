
namespace Device
{
	namespace
	{
		// We'll probably crash with TooManyObjects errors if we go above this limit
		constexpr uint64_t MAX_VRAM_USAGE = 8 * Memory::GB;

	#if defined(MOBILE)
		constexpr size_t CONSTANT_BUFFER_MAX_SIZE = 1 * Memory::MB;
	#else
		constexpr size_t CONSTANT_BUFFER_MAX_SIZE = 2 * Memory::MB;
	#endif
	}

	VKAPI_ATTR VkBool32 VKAPI_CALL IDeviceVulkan::DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
	{
		std::string text;
		if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
		{
			LOG_CRIT(StringToWstring(pCallbackData->pMessage));
			text += "ERROR: ";
		}
		else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
		{
			LOG_WARN(StringToWstring(pCallbackData->pMessage));
			text += "WARNING: ";
		}
		else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
		{
			LOG_INFO(StringToWstring(pCallbackData->pMessage));
			text += "INFO: ";
		}
		else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
		{
			LOG_DEBUG(StringToWstring(pCallbackData->pMessage));
			text += "VERBOSE: ";
		}
		text += std::string(pCallbackData->pMessage);
	#if defined(WIN32)
		OutputDebugStringA(std::string("[VULKAN] " + text + "\n").c_str());
	#endif
		return VK_FALSE;
	}

	void IDeviceVulkan::CreateDebugMessenger()
	{
	#if defined(ALLOW_DEBUG_LAYER)
		if (auto create_callback = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance.get(), "vkCreateDebugUtilsMessengerEXT"))
		{
			VkDebugUtilsMessengerCreateInfoEXT debug_messenger_info;
			debug_messenger_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
			debug_messenger_info.pNext = nullptr;
			debug_messenger_info.flags = 0;
			debug_messenger_info.messageSeverity =
				//VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | 
				//VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | 
				VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
			debug_messenger_info.messageType =
				VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
			debug_messenger_info.pfnUserCallback = DebugCallback;
			debug_messenger_info.pUserData = this;
			create_callback(instance.get(), &debug_messenger_info, nullptr, &debug_messenger);
		}
	#endif
	}

	void IDeviceVulkan::DestroyDebugMessenger()
	{
	#if defined(ALLOW_DEBUG_LAYER)
		if (auto destroy_callback = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance.get(), "vkDestroyDebugUtilsMessengerEXT"))
		{
			destroy_callback(instance.get(), debug_messenger, nullptr);
		}
	#endif
	}

	void IDeviceVulkan::InitDebugMarker()
	{
	#if defined(ALLOW_DEBUG_LAYER)
		object_name_callback = (PFN_vkDebugMarkerSetObjectNameEXT)vkGetInstanceProcAddr(instance.get(), "vkDebugMarkerSetObjectNameEXT");
		marker_begin_callback = (PFN_vkCmdDebugMarkerBeginEXT)vkGetInstanceProcAddr(instance.get(), "vkCmdDebugMarkerBeginEXT");
		marker_end_callback = (PFN_vkCmdDebugMarkerEndEXT)vkGetInstanceProcAddr(instance.get(), "vkCmdDebugMarkerEndEXT");
		marker_insert_callback = (PFN_vkCmdDebugMarkerInsertEXT)vkGetInstanceProcAddr(instance.get(), "vkCmdDebugMarkerInsertEXT");
	#endif
	}

	void IDeviceVulkan::DebugName(uint64_t object, VkDebugReportObjectTypeEXT type, const Memory::DebugStringA<>& name)
	{
	#if defined(ALLOW_DEBUG_LAYER)
		if (object_name_callback)
		{
			VkDebugMarkerObjectNameInfoEXT nameInfo = {};
			nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT;
			nameInfo.objectType = type;
			nameInfo.object = object;
			nameInfo.pObjectName = name.c_str();
			object_name_callback(device.get(), &nameInfo);
		}
	#endif
	}

	void IDeviceVulkan::BeginEvent(CommandBuffer& command_buffer, const std::wstring& text)
	{
	#if defined(ALLOW_DEBUG_LAYER)
		if (marker_begin_callback)
		{
			Memory::Lock lock(markers_mutex);
			const auto marker = WstringToString(text);
			auto found = markers.insert(marker);
			const auto marker_info = vk::DebugMarkerMarkerInfoEXT()
				.setColor({ { 1.0f } })
				.setPMarkerName(found.first->data());
			marker_begin_callback(static_cast<CommandBufferVulkan&>(command_buffer).GetCommandBuffer(), (VkDebugMarkerMarkerInfoEXT*)&marker_info);
		}
	#endif
	}

	void IDeviceVulkan::EndEvent(CommandBuffer& command_buffer)
	{
	#if defined(ALLOW_DEBUG_LAYER)
		if (marker_end_callback)
		{
			marker_end_callback(static_cast<CommandBufferVulkan&>(command_buffer).GetCommandBuffer());
		}
	#endif
	}

	void IDeviceVulkan::SetMarker(CommandBuffer& command_buffer, const std::wstring& text)
	{
	#if defined(ALLOW_DEBUG_LAYER)
		if (marker_insert_callback)
		{
			Memory::Lock lock(markers_mutex);
			const auto marker = WstringToString(text);
			auto found = markers.insert(marker);
			const auto marker_info = vk::DebugMarkerMarkerInfoEXT()
				.setColor({ { 1.0f } })
				.setPMarkerName(found.first->data());
			marker_insert_callback(static_cast<CommandBufferVulkan&>(command_buffer).GetCommandBuffer(), (VkDebugMarkerMarkerInfoEXT*)&marker_info);
		}
	#endif
	}

	IDeviceVulkan::IDeviceVulkan(DeviceInfo* _device_info, DeviceType device_type, HWND focus_window, const Renderer::Settings& renderer_settings)
		: IDevice(renderer_settings)
		, adapter_index(renderer_settings.adapter_index)
		, fullscreen(renderer_settings.fullscreen)
		, width(renderer_settings.resolution.width)
		, height(renderer_settings.resolution.height)
		, vsync(renderer_settings.vsync)
	{
		SetFullscreenMode(fullscreen);

		InitInstance();
	#if defined(__APPLE__)
		InitMoltenVK();
	#endif
		InitDevice();
	#if defined(__APPLE__)
		PrintMoltenVK();
	#endif
		
		allocator = Memory::Pointer<AllocatorVulkan, Memory::Tag::Device>::make(this);

		transfer = Memory::Pointer<TransferVulkan, Memory::Tag::Device>::make(this);
		constant_buffer = Memory::Pointer<AllocatorVulkan::ConstantBuffer, Memory::Tag::Device>::make("Global Constant Buffer", this, CONSTANT_BUFFER_MAX_SIZE);

		InitSwapChain();
		InitFences();

		IDevice::Init(renderer_settings);
	}

	IDeviceVulkan::~IDeviceVulkan()
	{
		PROFILE;

		WaitForIdle();

		SetFullscreenMode(false);

		IDevice::Quit();

		if (debug_messenger)
			DestroyDebugMessenger();

		transfer.reset();
		constant_buffer.reset();
	}

	void IDeviceVulkan::InitInstance()
	{
		PROFILE;

		std::vector<const char*> validation_layers;
	#if defined(ALLOW_DEBUG_LAYER) && !defined(__APPLE__)
		if (debug_layer)
		{
			validation_layers.emplace_back("VK_LAYER_KHRONOS_validation");
		}
	#endif

		for (auto& extension : vk::enumerateInstanceExtensionProperties())
		{
			LOG_INFO(L"[VULKAN] Supported instance extension: " + StringToWstring((char*)extension.extensionName));
		#if defined(WIN32)
			if (strcmp(extension.extensionName, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME) == 0)
				supports_get_physical_device_properties2 = true;
			if (strcmp(extension.extensionName, VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME) == 0)
				supports_get_surface_capabilities2 = true;
			if (strcmp(extension.extensionName, VK_KHR_EXTERNAL_FENCE_CAPABILITIES_EXTENSION_NAME) == 0)
				supports_external_fence_capabilities = true;
		#endif
		}
		std::vector<const char *> instance_extension_names = {
			VK_KHR_SURFACE_EXTENSION_NAME,
		#if defined(WIN32)
			VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
		#elif defined(__APPLE__macosx)
			VK_MVK_MOLTENVK_EXTENSION_NAME,
			VK_MVK_MACOS_SURFACE_EXTENSION_NAME,
		#elif defined(__APPLE__iphoneos)
			VK_MVK_MOLTENVK_EXTENSION_NAME,
			VK_MVK_IOS_SURFACE_EXTENSION_NAME,
		#endif
		};
	#if defined(ALLOW_DEBUG_LAYER)
		if (debug_layer)
		{
			instance_extension_names.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
			instance_extension_names.emplace_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
			instance_extension_names.emplace_back(VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME);
		}
	#endif
	#if defined(WIN32)
		if (supports_get_physical_device_properties2)
			instance_extension_names.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
		if (supports_get_surface_capabilities2)
			instance_extension_names.push_back(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME);
		if (supports_external_fence_capabilities)
			instance_extension_names.push_back(VK_KHR_EXTERNAL_FENCE_CAPABILITIES_EXTENSION_NAME);
	#endif

	#if defined(ALLOW_DEBUG_LAYER) && !defined(__APPLE__)
		std::vector<vk::ValidationFeatureEnableEXT> validation_features_ids = {
		#if defined(WIN32)
			//vk::ValidationFeatureEnableEXT::eBestPractices,
			//vk::ValidationFeatureEnableEXT::eGpuAssisted
		#endif
		};
		const auto validation_features = vk::ValidationFeaturesEXT()
			.setEnabledValidationFeatureCount((uint32_t)validation_features_ids.size())
			.setPEnabledValidationFeatures(validation_features_ids.data());
	#endif

		const auto app_info = vk::ApplicationInfo()
		#if defined(MOBILE)
			.setApiVersion(VK_API_VERSION_1_0) // Our current version of MoltenVK only supports 1.0.
		#else
			.setApiVersion(VK_API_VERSION_1_1)
		#endif
			.setPApplicationName("")
			.setApplicationVersion(1)
			.setPEngineName("")
			.setEngineVersion(1);

		const auto inst_info = vk::InstanceCreateInfo()
			.setPApplicationInfo(&app_info)
		#if defined(ALLOW_DEBUG_LAYER) && !defined(__APPLE__)
			.setPNext(&validation_features)
		#endif
			.setEnabledLayerCount((uint32_t)validation_layers.size())
			.setPpEnabledLayerNames(validation_layers.data())
			.setEnabledExtensionCount((uint32_t)instance_extension_names.size())
			.setPpEnabledExtensionNames(instance_extension_names.data());
		instance = vk::createInstanceUnique(inst_info);

	#if defined(ALLOW_DEBUG_LAYER)
		if (debug_layer)
		{
			CreateDebugMessenger();
			InitDebugMarker();
		}
	#endif
	}

#if defined(__APPLE__)
	void IDeviceVulkan::InitMoltenVK()
	{
		LOG_INFO(L"[VULKAN] MoltenVK version: " + std::to_wstring(MVK_VERSION));
		
		const auto get_config_callback = (PFN_vkGetMoltenVKConfigurationMVK)vkGetInstanceProcAddr(instance.get(), "vkGetMoltenVKConfigurationMVK");
		const auto set_config_callback = (PFN_vkSetMoltenVKConfigurationMVK)vkGetInstanceProcAddr(instance.get(), "vkSetMoltenVKConfigurationMVK");

		if (get_config_callback && set_config_callback)
		{
			MVKConfiguration config;
			auto config_size = sizeof(MVKConfiguration);
			get_config_callback(instance.get(), &config, &config_size);

			// If enabled, where possible, a Metal command buffer will be created and filled when each
			// Vulkan command buffer is filled. For applications that parallelize the filling of Vulkan
			// commmand buffers across multiple threads, this allows the Metal command buffers to also
			// be filled on the same parallel thread. Because each command buffer is filled separately,
			//this requires that each Vulkan command buffer requires a dedicated Metal command buffer.
			config.prefillMetalCommandBuffers = MVK_CONFIG_PREFILL_METAL_COMMAND_BUFFERS_STYLE_DEFERRED_ENCODING;

			LOG_INFO(L"[VULKAN] MoltenVK config: debugMode = " + std::wstring(config.debugMode ? L"ON" : L"OFF"));
			LOG_INFO(L"[VULKAN] MoltenVK config: shaderConversionFlipVertexY = " + std::wstring(config.shaderConversionFlipVertexY ? L"ON" : L"OFF"));
			LOG_INFO(L"[VULKAN] MoltenVK config: synchronousQueueSubmits = " + std::wstring(config.synchronousQueueSubmits ? L"ON" : L"OFF"));
			LOG_INFO(L"[VULKAN] MoltenVK config: prefillMetalCommandBuffers = " + std::wstring(config.prefillMetalCommandBuffers ? L"ON" : L"OFF"));
			LOG_INFO(L"[VULKAN] MoltenVK config: maxActiveMetalCommandBuffersPerQueue = " + std::to_wstring(config.maxActiveMetalCommandBuffersPerQueue));
			LOG_INFO(L"[VULKAN] MoltenVK config: supportLargeQueryPools = " + std::wstring(config.supportLargeQueryPools ? L"ON" : L"OFF"));
			LOG_INFO(L"[VULKAN] MoltenVK config: presentWithCommandBuffer = " + std::wstring(config.presentWithCommandBuffer ? L"ON" : L"OFF"));
			LOG_INFO(L"[VULKAN] MoltenVK config: swapchainMagFilterUseNearest = " + std::wstring(config.swapchainMagFilterUseNearest ? L"ON" : L"OFF"));
			LOG_INFO(L"[VULKAN] MoltenVK config: metalCompileTimeout = " + std::to_wstring(config.metalCompileTimeout));
			LOG_INFO(L"[VULKAN] MoltenVK config: performanceTracking = " + std::wstring(config.performanceTracking ? L"ON" : L"OFF"));
			LOG_INFO(L"[VULKAN] MoltenVK config: performanceLoggingFrameCount = " + std::to_wstring(config.performanceLoggingFrameCount));
			LOG_INFO(L"[VULKAN] MoltenVK config: displayWatermark = " + std::wstring(config.displayWatermark ? L"ON" : L"OFF"));
			LOG_INFO(L"[VULKAN] MoltenVK config: specializedQueueFamilies = " + std::wstring(config.specializedQueueFamilies ? L"ON" : L"OFF"));
			LOG_INFO(L"[VULKAN] MoltenVK config: switchSystemGPU = " + std::wstring(config.switchSystemGPU ? L"ON" : L"OFF"));
			LOG_INFO(L"[VULKAN] MoltenVK config: fullImageViewSwizzle = " + std::wstring(config.fullImageViewSwizzle ? L"ON" : L"OFF"));
			LOG_INFO(L"[VULKAN] MoltenVK config: defaultGPUCaptureScopeQueueFamilyIndex = " + std::to_wstring(config.defaultGPUCaptureScopeQueueFamilyIndex));
			LOG_INFO(L"[VULKAN] MoltenVK config: defaultGPUCaptureScopeQueueIndex = " + std::to_wstring(config.defaultGPUCaptureScopeQueueIndex));

			set_config_callback(instance.get(), &config, &config_size);
		}
	}
		
	void IDeviceVulkan::PrintMoltenVK()
	{
		const auto get_features_callback = (PFN_vkGetPhysicalDeviceMetalFeaturesMVK)vkGetInstanceProcAddr(instance.get(), "vkGetPhysicalDeviceMetalFeaturesMVK");

		if (get_features_callback)
		{
			MVKPhysicalDeviceMetalFeatures features;
			auto features_size = sizeof(MVKPhysicalDeviceMetalFeatures);
			get_features_callback(physical_device, &features, &features_size);

			LOG_INFO(L"[VULKAN] MoltenVK features: mslVersion = " + std::to_wstring(features.mslVersion));
			LOG_INFO(L"[VULKAN] MoltenVK features: baseVertexInstanceDrawing = " + std::wstring(features.baseVertexInstanceDrawing ? L"ON" : L"OFF"));
		}
	}
#endif

	void IDeviceVulkan::InitDevice()
	{
		PROFILE;

		physical_device = GetSelectedDevice();

		device_props = physical_device.getProperties();
		device_memory_props = physical_device.getMemoryProperties();
		LOG_INFO(L"[VULKAN] GPU type: " << vk::to_string(device_props.deviceType).c_str());

		PrintHeaps();

		const auto astc_format_props = physical_device.getFormatProperties(vk::Format::eAstc4x4UnormBlock); // Any ASTC ldr formats.
		supports_astc = !!(astc_format_props.optimalTilingFeatures & (vk::FormatFeatureFlags)vk::FormatFeatureFlagBits::eSampledImage);
		if (!supports_astc)
			LOG_INFO(L"[VULKAN] ASTC unsupported");

		PrintQueues();

		if (const auto family_index = FindFamily(vk::QueueFlagBits::eTransfer, vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute); family_index != (uint32_t)-1) // Transfer exclusive.
		{
			transfer_queue_family_index = family_index;
			transfer_queue_index = 0;
			graphics_queue_family_index = FindFamily(vk::QueueFlagBits::eGraphics, vk::QueueFlagBits());
			graphics_queue_index = 0;
			queue_family_indices.push_back(graphics_queue_family_index);
			queue_family_indices.push_back(transfer_queue_family_index);
		}
		else if (const auto family_index = FindFamily(vk::QueueFlagBits::eTransfer, vk::QueueFlagBits::eGraphics); family_index != (uint32_t)-1) // Fallback to Transfer/Compute.
		{
			transfer_queue_family_index = family_index;
			transfer_queue_index = 0;
			graphics_queue_family_index = FindFamily(vk::QueueFlagBits::eGraphics, vk::QueueFlagBits());
			graphics_queue_index = 0;
			queue_family_indices.push_back(graphics_queue_family_index);
			queue_family_indices.push_back(transfer_queue_family_index);
		}
		else // Fallback to Graphics/Transfer.
		{
			LOG_INFO(L"[VULKAN] Separate transfer queue unsupported");
			graphics_queue_family_index = FindFamily(vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eTransfer, vk::QueueFlagBits());
			graphics_queue_index = 0;
			transfer_queue_family_index = graphics_queue_family_index;
			transfer_queue_index = graphics_queue_index;
			queue_family_indices.push_back(graphics_queue_family_index);
		}

		LOG_INFO(L"[VULKAN] Graphics queue: family = " << graphics_queue_family_index << L", index = " << graphics_queue_index);
		LOG_INFO(L"[VULKAN] Transfer queue: family = " << transfer_queue_family_index << L", index = " << transfer_queue_index);

		subgroup_size = 4;
	#if defined(WIN32)
		if (supports_get_physical_device_properties2)
		{
			auto subgroup_props = vk::PhysicalDeviceSubgroupProperties();
			auto device_props2 = vk::PhysicalDeviceProperties2();
			device_props2.pNext = &subgroup_props;
			physical_device.getProperties2(&device_props2);

			if (subgroup_props.subgroupSize >= 64)		subgroup_size = 64;
			else if (subgroup_props.subgroupSize >= 32)	subgroup_size = 32;
		}
	#endif

		const auto device_features = vk::PhysicalDeviceFeatures()
		#if WIP_PARTICLE_SYSTEM
			.setFragmentStoresAndAtomics(true)
		#endif
		#if !defined(__APPLE__) // Fails to create device on mobile/mac with this enabled.
			.setGeometryShader(true)
		#endif
			.setSamplerAnisotropy(true);

	#if !defined(__APPLE__) // Doesn't compile on mobile/mac
		const auto device_draw_param_features = vk::PhysicalDeviceShaderDrawParameterFeatures()
			.setShaderDrawParameters(true);// Needed for SV_InstanceID, apparently?
	#endif

		std::vector<float> queue_priorities = { 1.0f };
		std::vector<vk::DeviceQueueCreateInfo> queue_infos;
		queue_infos.push_back(vk::DeviceQueueCreateInfo()
			.setQueueFamilyIndex(graphics_queue_family_index)
			.setQueueCount((uint32_t)1)
			.setPQueuePriorities(queue_priorities.data()));
		if (transfer_queue_family_index != graphics_queue_family_index)
		{
			queue_infos.push_back(vk::DeviceQueueCreateInfo()
				.setQueueFamilyIndex(transfer_queue_family_index)
				.setQueueCount((uint32_t)1)
				.setPQueuePriorities(queue_priorities.data()));
		}

		std::vector<const char*> validation_layers;
	#if defined(ALLOW_DEBUG_LAYER) && !defined(__APPLE__)
		if (debug_layer)
			validation_layers.emplace_back("VK_LAYER_KHRONOS_validation");
	#endif

		// These extensions (apparently) are needed on AMD for our shaders (probably comming from DXC when trying to compile hlsl to spir-v).
		// The shaders work without them, but the debug layer will spam validation errors when uploading the shaders
		// For some reason, NVidia cards doen't seem to care at all - they don't list these extensions as supported, and don't care if the shader
		// uses them anyways. So, just to silence the debug layer, we add them if we detect that we support them
		bool google_hlsl_functionality1 = false;
		bool google_user_type = false;

		for (auto& extension : physical_device.enumerateDeviceExtensionProperties())
		{
			LOG_INFO(L"[VULKAN] Supported device extension: " + StringToWstring((char*)extension.extensionName));
		#if defined(WIN32)
			if (strcmp(extension.extensionName, VK_EXT_FULL_SCREEN_EXCLUSIVE_EXTENSION_NAME) == 0)
				supports_fullscreen_exclusive = true;
			if (strcmp(extension.extensionName, VK_EXT_MEMORY_BUDGET_EXTENSION_NAME) == 0)
				supports_memory_budget_query = true;
		#endif
			if (strcmp(extension.extensionName, VK_GOOGLE_HLSL_FUNCTIONALITY1_EXTENSION_NAME) == 0)
				google_hlsl_functionality1 = true;
			if (strcmp(extension.extensionName, VK_GOOGLE_USER_TYPE_EXTENSION_NAME) == 0)
				google_user_type = true;
		}
		std::vector<const char *> device_extension_names = {
			VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		};
	#if defined(ALLOW_DEBUG_LAYER)
		if (debug_layer)
			device_extension_names.emplace_back(VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
	#endif
	#if defined(WIN32)
		if (supports_fullscreen_exclusive)
			device_extension_names.push_back(VK_EXT_FULL_SCREEN_EXCLUSIVE_EXTENSION_NAME);
		if (supports_memory_budget_query)
			device_extension_names.push_back(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME);
	#endif

		if(google_hlsl_functionality1)
			device_extension_names.push_back(VK_GOOGLE_HLSL_FUNCTIONALITY1_EXTENSION_NAME);
		if (google_user_type)
			device_extension_names.push_back(VK_GOOGLE_USER_TYPE_EXTENSION_NAME);

	#ifdef LOG_DEBUG_ENABLED
		for (const auto& a : device_extension_names)
			LOG_DEBUG(L"[VULKAN] Requesting device extension: " + StringToWstring(a));
	#endif

		const auto device_info = vk::DeviceCreateInfo()
			.setQueueCreateInfoCount((uint32_t)queue_infos.size())
			.setPQueueCreateInfos(queue_infos.data())
			.setEnabledLayerCount((uint32_t)validation_layers.size())
			.setPpEnabledLayerNames(validation_layers.data())
			.setEnabledExtensionCount((uint32_t)device_extension_names.size())
			.setPpEnabledExtensionNames(device_extension_names.data())
		#if !defined(__APPLE__)
			.setPNext(&device_draw_param_features)
		#endif
			.setPEnabledFeatures(&device_features);
		device = physical_device.createDeviceUnique(device_info);
		DebugName((uint64_t)device.get().operator VkDevice(), VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_EXT, "Device");

		graphics_queue = device->getQueue(graphics_queue_family_index, graphics_queue_index);
		DebugName((uint64_t)graphics_queue.operator VkQueue(), VK_DEBUG_REPORT_OBJECT_TYPE_QUEUE_EXT, "Graphics Queue");

		transfer_queue = device->getQueue(transfer_queue_family_index, transfer_queue_index);
		DebugName((uint64_t)transfer_queue.operator VkQueue(), VK_DEBUG_REPORT_OBJECT_TYPE_QUEUE_EXT, "Transfer Queue");
	}

	void IDeviceVulkan::InitSwapChain()
	{
		PROFILE;

		WindowDX* window = WindowDX::GetWindow();

		if (swap_chain)
			swap_chain->Recreate(window->GetWnd(), this, width, height, fullscreen, vsync, 0);
		else
			swap_chain = SwapChain::Create(window->GetWnd(), this, width, height, fullscreen, vsync, 0);

		width = (uint32_t)swap_chain->GetWidth();
		height = (uint32_t)swap_chain->GetHeight();

		DynamicResolution::Get().Reset(width, height);
	}

	void IDeviceVulkan::InitFences()
	{
		PROFILE;

		for (unsigned i = 0; i < FRAME_QUEUE_COUNT; ++i)
		{
			draw_fences[i] = device->createFenceUnique(vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled));
			DebugName((uint64_t)draw_fences[i].get().operator VkFence(), VK_DEBUG_REPORT_OBJECT_TYPE_FENCE_EXT, "Draw Fence " + std::to_string(i));
		}
	}

	void IDeviceVulkan::ReinitSwapChain()
	{
		PROFILE;

		LOG_DEBUG(L"[VULKAN] ReinitSwapChain");

		WaitForIdle();

		InitSwapChain();

		if (auto* window = WindowDX::GetWindow())
		{
			SurfaceDesc desc;
			GetBackBufferDesc(0, 0, &desc);

			window->TriggerDeviceLost();

			transfer->Wait();
			transfer->Flush(); // Prevents crash when resizing the window.
			transfer->WaitAll();

			releaser->Clear(); // Very important to avoid peak memory usage.

			window->TriggerDeviceReset(this, &desc); // TODO: Avoid recreating everything, only need to resize main render target.
		}
	}

	void IDeviceVulkan::SetFullscreenMode(bool is_fullscreen)
	{
		if (auto* window = WindowDX::GetWindow())
		{
			if (is_fullscreen)
			{
				if (!window->SetDisplayResolution(this, width, height))
					fullscreen_failed = true;
			}
			else
				window->RestoreDisplayResolution();
		}
	}

	void IDeviceVulkan::RecreateSwapChain(HWND hwnd, UINT Output, const Renderer::Settings& renderer_settings)
	{
		reinit_swap_chain = true;
		width = renderer_settings.resolution.width;
		height = renderer_settings.resolution.height;
		adapter_index = Output;
		fullscreen = renderer_settings.fullscreen;
		SetFullscreenMode(fullscreen);
	}

	void IDeviceVulkan::Swap()
	{
		markers.clear();
		IDevice::Swap();

		transfer->Wait(); // Wait first before any lock/unlock (in FrameMove for particles/trails for instance).
	}

	bool IDeviceVulkan::WaitFence()
	{
		PROFILE;

		Timer timer;
		timer.Start();

		std::array<vk::Fence, 1> fences = { draw_fences[buffer_index].get() };
		auto res = device->waitForFences(fences, VK_TRUE, std::numeric_limits<uint64_t>::max());

		fence_wait_time = (float)timer.GetElapsedTime();

		return res == vk::Result::eSuccess;
	}

	void IDeviceVulkan::ResetFence()
	{
		std::array<vk::Fence, 1> fences = { draw_fences[buffer_index].get() };
		device->resetFences(fences);
	}
	
	void IDeviceVulkan::BeginFlush()
	{
		transfer->Flush(); // Matches Wait in Swap.

		frame_cmd_bufs.clear();
	}

	void IDeviceVulkan::Flush(CommandBuffer& command_buffer)
	{
		frame_cmd_bufs.push_back(static_cast<CommandBufferVulkan&>(command_buffer).GetCommandBuffer());
	}

	void IDeviceVulkan::EndFlush()
	{
	}

	void IDeviceVulkan::BeginFrame()
	{
		PROFILE;

		WaitFence();

		if (reinit_swap_chain)
		{
			reinit_swap_chain = false;
			ReinitSwapChain();
		}

		PROFILE_ONLY(allocator->stats.GlobalConstantsSize(constant_buffer->GetSize(), constant_buffer->GetMaxSize());)
		constant_buffer->Reset();
	}

	void IDeviceVulkan::EndFrame()
	{
		
	}

	void IDeviceVulkan::WaitForBackbuffer(CommandBuffer& command_buffer)
	{
		PROFILE;

		Timer timer;
		timer.Start();
		
		((SwapChainVulkan*)swap_chain.get())->WaitForBackbuffer(buffer_index);

	#if DEBUG_GUI_ENABLED
		Device::GUI::GUIResourceManager::GetGUIResourceManager()->ForEachSwapChain([&](SwapChain* swap_chain)
		{
			((SwapChainVulkan*)swap_chain)->WaitForBackbuffer(buffer_index);
		});
	#endif

		ResetFence();

		backbuffer_wait_time = (float)timer.GetElapsedTime();
	}

	void IDeviceVulkan::Submit()
	{
		PROFILE;

		Memory::SmallVector<vk::Semaphore, 8, Memory::Tag::Device> wait_semaphores;
		Memory::SmallVector<vk::PipelineStageFlags, 8, Memory::Tag::Device> wait_stages;
		Memory::SmallVector<vk::Semaphore, 8, Memory::Tag::Device> signal_semaphores;

		wait_semaphores.emplace_back(((SwapChainVulkan*)swap_chain.get())->GetAcquiredSemaphore(buffer_index));
		wait_stages.push_back(vk::PipelineStageFlagBits::eColorAttachmentOutput);
		signal_semaphores.emplace_back(((SwapChainVulkan*)swap_chain.get())->GetRenderedSemaphore(buffer_index));

	#if DEBUG_GUI_ENABLED
		Device::GUI::GUIResourceManager::GetGUIResourceManager()->ForEachSwapChain([&](SwapChain* swap_chain)
		{
			wait_semaphores.emplace_back(((SwapChainVulkan*)swap_chain)->GetAcquiredSemaphore(buffer_index));
			wait_stages.push_back(vk::PipelineStageFlagBits::eColorAttachmentOutput);
			signal_semaphores.emplace_back(((SwapChainVulkan*)swap_chain)->GetRenderedSemaphore(buffer_index));
		});
	#endif


		try
		{
			Memory::Lock lock(transfer->GetMutex()); // Lock graphics_queue and transfer_queue that can be the same underlying object.

			assert(wait_semaphores.size() == wait_stages.size());
			std::array<vk::SubmitInfo, 1> submit_infos;
			submit_infos[0]
				.setWaitSemaphoreCount((uint32_t)wait_semaphores.size())
				.setPWaitSemaphores(wait_semaphores.data())
				.setPWaitDstStageMask(wait_stages.data())
				.setSignalSemaphoreCount((uint32_t)signal_semaphores.size())
				.setPSignalSemaphores(signal_semaphores.data())
				.setCommandBufferCount((uint32_t)frame_cmd_bufs.size())
				.setPCommandBuffers(frame_cmd_bufs.data());
			graphics_queue.submit(submit_infos, draw_fences[buffer_index].get()); // TODO: Submit command buffers that do not use the framebuffer before waiting.
		}
		catch (const vk::OutOfDateKHRError& error)
		{
			(void)error;
			LOG_INFO(L"[VULKAN] vk::OutOfDateKHRError");
			reinit_swap_chain = true;
		}
	}

	void IDeviceVulkan::Present(const Rect* pSourceRect, const Rect* pDestRect, HWND hDestWindowOverride)
	{
		PROFILE;

		try
		{
			if(!swap_chain->Present())
				reinit_swap_chain = true;
		}
		catch (const vk::OutOfDateKHRError& error)
		{
			(void)error;
			LOG_INFO(L"[VULKAN] vk::OutOfDateKHRError");
			reinit_swap_chain = true;
		}

		buffer_index = (buffer_index + 1) % FRAME_QUEUE_COUNT;
	}

	void IDeviceVulkan::WaitForIdle()
	{
		PROFILE;

		transfer->Wait();
		transfer->Flush();
		transfer->WaitAll();

		device->waitIdle();
	}

	void IDeviceVulkan::PrintQueues() const
	{
		const auto queue_props = physical_device.getQueueFamilyProperties();
		for (unsigned i = 0; i < queue_props.size(); i++)
		{
			LOG_INFO(L"[VULKAN] Queue " <<
				L": family = " + std::to_wstring(i) +
				L", count = " + std::to_wstring(queue_props[i].queueCount) +
				L", flags = " + StringToWstring(vk::to_string(queue_props[i].queueFlags)));
		}
	}

	uint32_t IDeviceVulkan::FindFamily(vk::QueueFlags needed_queue_flags, vk::QueueFlags forbidden_queue_flags) const
	{
		const auto queue_props = physical_device.getQueueFamilyProperties();
		for (unsigned i = 0; i < queue_props.size(); i++)
		{
			if (queue_props[i].queueFlags & needed_queue_flags)
				if (!(queue_props[i].queueFlags & forbidden_queue_flags))
					return i;
		}
		return (uint32_t)-1;
	}

	void IDeviceVulkan::PrintHeaps() const
	{
		for (unsigned i = 0; i < device_memory_props.memoryTypeCount; i++)
		{
			LOG_INFO(L"[VULKAN] Memory type" <<
				L": heap index = " + std::to_wstring(device_memory_props.memoryTypes[i].heapIndex) +
				L", property = " + StringToWstring(vk::to_string(device_memory_props.memoryTypes[i].propertyFlags)));
		}
		for (unsigned i = 0; i < device_memory_props.memoryHeapCount; i++)
		{
			LOG_INFO(L"[VULKAN] Memory heap" <<
				L": size = " + Memory::ReadableSizeW(device_memory_props.memoryHeaps[i].size) +
				L", flags = " + StringToWstring(vk::to_string(device_memory_props.memoryHeaps[i].flags)));
		}
	}

	uint32_t IDeviceVulkan::MemoryType(uint32_t type_bits, vk::MemoryPropertyFlags mem_props) const
	{
		for (uint32_t i = 0; i < device_memory_props.memoryTypeCount; i++)
		{
			if ((type_bits & 1) == 1)
			{
				if ((device_memory_props.memoryTypes[i].propertyFlags & mem_props) == mem_props)
					return i;
			}
			type_bits >>= 1;
		}
		return (uint32_t)-1;
	}

	size_t IDeviceVulkan::MemoryMaxSize(vk::MemoryPropertyFlags mem_props) const
	{
		size_t size = 0;
		for (uint32_t i = 0; i < device_memory_props.memoryTypeCount; i++)
		{
			if (device_memory_props.memoryTypes[i].propertyFlags == mem_props)
				size += device_memory_props.memoryHeaps[device_memory_props.memoryTypes[i].heapIndex].size;
		}
		return size;
	}

	size_t IDeviceVulkan::MemoryHeapSize(vk::MemoryHeapFlags heap) const
	{
		size_t size = 0;
		for (uint32_t i = 0; i < device_memory_props.memoryHeapCount; i++)
		{
			if ((device_memory_props.memoryHeaps[i].flags & heap) == heap)
				size += device_memory_props.memoryHeaps[i].size;
		}
		if (size == 0)
			throw ExceptionVulkan("MemoryHeap " + vk::to_string(heap) + " not supported");
		return size;
	}

	size_t IDeviceVulkan::MemoryHeapBudgetAvailable(const vk::PhysicalDeviceMemoryBudgetPropertiesEXT& budget, vk::MemoryHeapFlags heap) const
	{
		size_t available = 0;
		for (uint32_t i = 0; i < device_memory_props.memoryHeapCount; i++)
		{
			if ((device_memory_props.memoryHeaps[i].flags & heap) == heap)
			{
				available += budget.heapBudget[i];
			}
		}
		return available;
	}

	vk::ImageTiling IDeviceVulkan::Tiling(vk::Format format, vk::FormatFeatureFlags feature) const
	{
		const auto format_props = physical_device.getFormatProperties(format);
		if (format_props.optimalTilingFeatures & feature)
			return vk::ImageTiling::eOptimal;
		if (format_props.linearTilingFeatures & feature)
			return vk::ImageTiling::eLinear;
		return vk::ImageTiling::eOptimal;
	}

	std::pair<uint8_t*, size_t> IDeviceVulkan::AllocateFromConstantBuffer(size_t size)
	{
		return constant_buffer->Allocate(size, buffer_index);
	}

#if defined(WIN32)
	std::vector<IDeviceVulkan::PhysicalDeviceData> IDeviceVulkan::EnumeratePhysicalDevices()
	{
		std::vector<PhysicalDeviceData> device_datas;
		auto physical_devices = instance->enumeratePhysicalDevices();
		for (auto& device : physical_devices)
		{
			auto& data = device_datas.emplace_back();
			data.device = device;
			if (supports_get_physical_device_properties2 && supports_external_fence_capabilities)
			{
				data.props.pNext = &data.id_props;
				device.getProperties2(&data.props);
			}
		}
		return device_datas;
	}

	const Device::AdapterInfo* IDeviceVulkan::GetSelectedAdapter()
	{
		Device::AdapterInfo* selected_adapter = nullptr;
		static_assert(sizeof(selected_adapter->luid) == VK_LUID_SIZE);
		if (auto* window = WindowDX::GetWindow())
		{
			auto& info_list = window->GetInfo()->GetAdapterInfoList();
			if (adapter_index >= info_list.size())
				throw ExceptionVulkan("Out-of-range adapter index");
			selected_adapter = &info_list[adapter_index];
		}
		return selected_adapter;
	}

	const IDeviceVulkan::PhysicalDeviceData* IDeviceVulkan::FindPhysicalDeviceLUID(const std::vector<IDeviceVulkan::PhysicalDeviceData>& device_datas, const std::wstring& name, const uint8_t luid[8])
	{
		for (auto& device_data : device_datas)
		{
			if (device_data.id_props.deviceLUIDValid && (memcmp(luid, &device_data.id_props.deviceLUID, VK_LUID_SIZE) == 0))
			{
				LOG_INFO(L"[VULKAN] Found matching physical device using LUID " << luid << " (" << name << ")");
				return &device_data;
			}
		}
		return nullptr;
	}

	const IDeviceVulkan::PhysicalDeviceData* IDeviceVulkan::FindPhysicalDeviceName(const std::vector<IDeviceVulkan::PhysicalDeviceData>& device_datas, const std::wstring& name)
	{
		for (auto& device_data : device_datas)
		{
			if (StringToWstring((char*)device_data.props.properties.deviceName.data()) == name)
			{
				LOG_INFO(L"[VULKAN] Found matching physical device using name (" << name << ")");
				return &device_data;
			}
		}
		return nullptr;
	}

	const IDeviceVulkan::PhysicalDeviceData* IDeviceVulkan::FindPhysicalDeviceIndex(const std::vector<IDeviceVulkan::PhysicalDeviceData>& device_datas, const std::wstring& name, unsigned index)
	{
		if (index < device_datas.size())
		{
			LOG_WARN(L"[VULKAN] Found physical device at #" << index << " (" << name << ")");
			return &device_datas[index];
		}
		return nullptr;
	}
#endif

	vk::PhysicalDevice IDeviceVulkan::GetSelectedDevice()
	{
	#if defined(WIN32)
		const PhysicalDeviceData* selected_device_data = nullptr;
		const auto device_datas = EnumeratePhysicalDevices();
		if (device_datas.size() > 0)
		{
			if (const auto selected_adapter = GetSelectedAdapter())
			{
				if (!selected_device_data) selected_device_data = FindPhysicalDeviceLUID(device_datas, selected_adapter->display_name, selected_adapter->luid);
				if (!selected_device_data) selected_device_data = FindPhysicalDeviceName(device_datas, selected_adapter->display_name);
				if (!selected_device_data) selected_device_data = FindPhysicalDeviceIndex(device_datas, selected_adapter->display_name, selected_adapter->index);
			}

			if (!selected_device_data)
			{
				LOG_WARN(L"[VULKAN] Failed to match physical device, falling back to #0");
				selected_device_data = &device_datas[0]; // TODO: Find discrete GPU based on name?
			}
		}

		if (!selected_device_data)
			throw ExceptionVulkan("No physical device found");

		return selected_device_data->device;
	#else
		const auto devices = instance->enumeratePhysicalDevices();
		for (auto& device : devices)
		{
			const auto props = device.getProperties();
			LOG_INFO(L"[VULKAN] Physical device: name = \"" << StringToWstring((char*)props.deviceName.data()) << L"\"" <<  
				L", type = " << StringToWstring(to_string(props.deviceType)));
		}
		return devices[0];
	#endif
	}

	void IDeviceVulkan::Suspend()
	{
	}

	void IDeviceVulkan::Resume()
	{
	}

	bool IDeviceVulkan::IsWindowed() const
	{
		return !fullscreen;
	}

	HRESULT IDeviceVulkan::GetBackBufferDesc( UINT iSwapChain, UINT iBackBuffer, SurfaceDesc* pBBufferSurfaceDesc )
	{
		memset(pBBufferSurfaceDesc, 0, sizeof(SurfaceDesc));
		pBBufferSurfaceDesc->Width = width;
		pBBufferSurfaceDesc->Height = height;
		pBBufferSurfaceDesc->Format = PixelFormat::A8R8G8B8;
		return S_OK;
	}

	bool IDeviceVulkan::SupportsSubPasses() const
	{
		return true;
	}

	bool IDeviceVulkan::SupportsParallelization() const
	{
		return true;
	}

	bool IDeviceVulkan::SupportsCommandBufferProfile() const
	{
		return true;
	}

	unsigned IDeviceVulkan::GetWavefrontSize() const
	{
		return subgroup_size;
	}

	VRAM IDeviceVulkan::GetVRAM() const
	{
		const auto allocator_vram = allocator->GetVRAM();

		VRAM vram;
		vram.allocation_count = allocator_vram.allocation_count;
		vram.block_count = allocator_vram.block_count;
		vram.used = std::min(allocator_vram.used, MAX_VRAM_USAGE);
		vram.reserved = std::min(allocator_vram.reserved, MAX_VRAM_USAGE);

	#if defined(MOBILE)
		{
			vram.available = std::min(uint64_t(MemoryHeapSize(vk::MemoryHeapFlagBits::eDeviceLocal) / 5), MAX_VRAM_USAGE); // Divide heap by 2 at least since app can safely access only half the RAM.

			vram.total = vram.available; // Need to be the same to have no extra headroom in the streamer.
		}
	#else
		{
			const auto heap =
			#if defined(WIN32)
				device_props.deviceType == vk::PhysicalDeviceType::eIntegratedGpu ? vk::MemoryHeapFlags(0) : // Use main memory (non device-local heap).
			#endif
				vk::MemoryHeapFlagBits::eDeviceLocal;

		#if defined(WIN32)
			if (supports_memory_budget_query)
			{
				const auto memory_props2 = physical_device.getMemoryProperties2<vk::PhysicalDeviceMemoryProperties2, vk::PhysicalDeviceMemoryBudgetPropertiesEXT>();
				const auto budget_available = MemoryHeapBudgetAvailable(memory_props2.get<vk::PhysicalDeviceMemoryBudgetPropertiesEXT>(), heap);
				vram.available = std::min(budget_available, MAX_VRAM_USAGE);
			}
			else
		#endif
			{
				// We can't query the exact amount of available and used vram by the GPU, so we just
				// limit ourselves to use up to about 70% of what the GPU has, as recommended by the GDC 2018 talk
				// "Memory management in Vulkan and DX12" from Adam Sawicki
				vram.available = std::min(uint64_t((MemoryHeapSize(heap) * 7) / 10), MAX_VRAM_USAGE);
			}

			vram.total = std::min(uint64_t(MemoryHeapSize(heap)), MAX_VRAM_USAGE);
		}
	#endif

		return vram;
	}

	HRESULT IDeviceVulkan::CheckForDXGIBufferChange() // TODO: Cleanup.
	{
		HRESULT hr = S_OK;

	#if defined(WIN32)
		WindowDX* window = WindowDX::GetWindow();
		if (window == nullptr)
			return hr;
		
		const bool windowed = device_settings.Windowed > 0;
		if (windowed)
		{
			//get the correct rect depending if we are in window mode or fullscreen mode
			RECT rc_current_client;
			HWND hwnd = window->GetWnd();
			GetClientRect(hwnd, &rc_current_client);

			if ((width != rc_current_client.right) ||
				(height != rc_current_client.bottom))
			{
				hr = ResizeDXGIBuffers(rc_current_client.right, rc_current_client.bottom, fullscreen);
			}

			UINT flag = window->IsMaximized() ? SW_SHOWMAXIMIZED : SW_SHOW;
			window->ShowWindow(flag);
		}
	#endif

		return hr;
	}

	HRESULT IDeviceVulkan::ResizeDXGIBuffers(UINT Width, UINT Height, BOOL bFullScreen, BOOL bBorderless) // TODO: Cleanup.
	{
		LOG_DEBUG(L"[VULKAN] ResizeBuffers");

		HRESULT hr = S_OK;

	#if defined(WIN32)
		WindowDX* window = WindowDX::GetWindow();
		if (window == nullptr)
			return hr;

		if (!bFullScreen)
		{
			if (bBorderless)
			{
				// Swapchain dimensions in borderless mode should always equals the monitor resolution
				window->GetMonitorDimensions(device_settings.AdapterOrdinal, &Width, &Height);
			}
			else
			{
				// Swapchain dimensions in windowed mode should always equals the client rectangle
				RECT rect;
				GetClientRect(window->GetWnd(), &rect);
				Width = rect.right - rect.left;
				Height = rect.bottom - rect.top;
			}
		}
		else if (Width == 0 || Height == 0)
		{
			Width = device_settings.BackBufferWidth;
			Height = device_settings.BackBufferHeight;
		}

		width = Width;
		height = Height;

		//reinit_swap_chain = true; // Do it synchronously here so that renderer settings are updated and marked for save when resizing the window.
		ReinitSwapChain();

		Pause(false, false);
	#elif defined(__APPLE__macosx)
		width = Width;
		height = Height;

		reinit_swap_chain = true;
	#endif

		return hr;
	}

	void IDeviceVulkan::ResourceFlush()
	{
		WaitForIdle();

		releaser->Clear();

		transfer->Wait();
		transfer->Flush();
	}

	void IDeviceVulkan::SetVSync(bool enabled) { vsync = enabled; }
	bool IDeviceVulkan::GetVSync() { return vsync; }

	PROFILE_ONLY(std::vector<std::vector<std::string>> IDeviceVulkan::GetStats()
	{
		auto lines = GetAllocator().GetStats();
		return lines;
	})

	PROFILE_ONLY(size_t IDeviceVulkan::GetSize() { return GetAllocator().GetSize(); })

	bool IDeviceVulkan::CheckFullScreenFailed()
	{
		return IDeviceVulkan::fullscreen_failed;
	}
}
