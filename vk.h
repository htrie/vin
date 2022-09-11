#pragma once

vk::UniqueInstance create_instance() {
    // Look for validation layers
    uint32_t enabled_layer_count = 0;
    char const* enabled_layers[64];

#if !defined(NDEBUG)
    {
        uint32_t instance_layer_count = 0;
        const auto result = vk::enumerateInstanceLayerProperties(&instance_layer_count, static_cast<vk::LayerProperties*>(nullptr));
        VERIFY(result == vk::Result::eSuccess);

        if (instance_layer_count > 0) {
            enabled_layer_count = 1;
            enabled_layers[0] = "VK_LAYER_KHRONOS_validation";
        }
    }
#endif

    // Look for instance extensions
    uint32_t instance_extension_count = 0;
    const auto result = vk::enumerateInstanceExtensionProperties(nullptr, &instance_extension_count, static_cast<vk::ExtensionProperties*>(nullptr));
    VERIFY(result == vk::Result::eSuccess);

    uint32_t enabled_extension_count = 0;
    char const* extension_names[64];
    memset(extension_names, 0, sizeof(extension_names));

    vk::Bool32 surfaceExtFound = VK_FALSE;
    vk::Bool32 platformSurfaceExtFound = VK_FALSE;

    if (instance_extension_count > 0) {
        std::unique_ptr<vk::ExtensionProperties[]> instance_extensions(new vk::ExtensionProperties[instance_extension_count]);
        const auto result = vk::enumerateInstanceExtensionProperties(nullptr, &instance_extension_count, instance_extensions.get());
        VERIFY(result == vk::Result::eSuccess);

        for (uint32_t i = 0; i < instance_extension_count; i++) {
            if (!strcmp(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME, instance_extensions[i].extensionName)) {
                extension_names[enabled_extension_count++] = VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME;
            }
            if (!strcmp(VK_KHR_SURFACE_EXTENSION_NAME, instance_extensions[i].extensionName)) {
                surfaceExtFound = 1;
                extension_names[enabled_extension_count++] = VK_KHR_SURFACE_EXTENSION_NAME;
            }
            if (!strcmp(VK_KHR_WIN32_SURFACE_EXTENSION_NAME, instance_extensions[i].extensionName)) {
                platformSurfaceExtFound = 1;
                extension_names[enabled_extension_count++] = VK_KHR_WIN32_SURFACE_EXTENSION_NAME;
            }
            VERIFY(enabled_extension_count < 64);
        }
    }

    if (!surfaceExtFound) {
        ERR_EXIT("vkEnumerateInstanceExtensionProperties failed to find the " VK_KHR_SURFACE_EXTENSION_NAME
            " extension.\n\n"
            "Do you have a compatible Vulkan installable client driver (ICD) installed?\n"
            "Please look at the Getting Started guide for additional information.\n",
            "vkCreateInstance Failure");
    }

    if (!platformSurfaceExtFound) {
        ERR_EXIT("vkEnumerateInstanceExtensionProperties failed to find the " VK_KHR_WIN32_SURFACE_EXTENSION_NAME
            " extension.\n\n"
            "Do you have a compatible Vulkan installable client driver (ICD) installed?\n"
            "Please look at the Getting Started guide for additional information.\n",
            "vkCreateInstance Failure");
    }
    auto const app_info = vk::ApplicationInfo()
        .setApplicationVersion(0)
        .setEngineVersion(0)
        .setApiVersion(VK_API_VERSION_1_0);
    auto const inst_info = vk::InstanceCreateInfo()
        .setPApplicationInfo(&app_info)
        .setEnabledLayerCount(enabled_layer_count)
        .setPpEnabledLayerNames(enabled_layers)
        .setEnabledExtensionCount(enabled_extension_count)
        .setPpEnabledExtensionNames(extension_names);

    auto instance_handle = vk::createInstanceUnique(inst_info);
    if (instance_handle.result == vk::Result::eErrorIncompatibleDriver) {
        ERR_EXIT(
            "Cannot find a compatible Vulkan installable client driver (ICD).\n\n"
            "Please look at the Getting Started guide for additional information.\n",
            "vkCreateInstance Failure");
    }
    else if (instance_handle.result == vk::Result::eErrorExtensionNotPresent) {
        ERR_EXIT(
            "Cannot find a specified extension library.\n"
            "Make sure your layers path is set appropriately.\n",
            "vkCreateInstance Failure");
    }
    else if (instance_handle.result != vk::Result::eSuccess) {
        ERR_EXIT(
            "vkCreateInstance failed.\n\n"
            "Do you have a compatible Vulkan installable client driver (ICD) installed?\n"
            "Please look at the Getting Started guide for additional information.\n",
            "vkCreateInstance Failure");
    }
    return std::move(instance_handle.value);
}

vk::PhysicalDevice pick_gpu(const vk::Instance& instance) {
    // Make initial call to query gpu_count, then second call for gpu info
    uint32_t gpu_count = 0;
    auto result = instance.enumeratePhysicalDevices(&gpu_count, static_cast<vk::PhysicalDevice*>(nullptr));
    VERIFY(result == vk::Result::eSuccess);

    if (gpu_count <= 0) {
        ERR_EXIT(
            "vkEnumeratePhysicalDevices reported zero accessible devices.\n\n"
            "Do you have a compatible Vulkan installable client driver (ICD) installed?\n"
            "Please look at the Getting Started guide for additional information.\n",
            "vkEnumeratePhysicalDevices Failure");
    }

    std::unique_ptr<vk::PhysicalDevice[]> physical_devices(new vk::PhysicalDevice[gpu_count]);
    result = instance.enumeratePhysicalDevices(&gpu_count, physical_devices.get());
    VERIFY(result == vk::Result::eSuccess);

    // Try to auto select most suitable device
    int32_t gpu_number = -1;
    {
        uint32_t count_device_type[VK_PHYSICAL_DEVICE_TYPE_CPU + 1];
        memset(count_device_type, 0, sizeof(count_device_type));

        for (uint32_t i = 0; i < gpu_count; i++) {
            const auto physicalDeviceProperties = physical_devices[i].getProperties();
            VERIFY(static_cast<int>(physicalDeviceProperties.deviceType) <= VK_PHYSICAL_DEVICE_TYPE_CPU);
            count_device_type[static_cast<int>(physicalDeviceProperties.deviceType)]++;
        }

        const vk::PhysicalDeviceType device_type_preference[] = {
            vk::PhysicalDeviceType::eDiscreteGpu,
            vk::PhysicalDeviceType::eIntegratedGpu,
            vk::PhysicalDeviceType::eVirtualGpu,
            vk::PhysicalDeviceType::eCpu,
            vk::PhysicalDeviceType::eOther
        };
        auto search_for_device_type = vk::PhysicalDeviceType::eDiscreteGpu;
        for (uint32_t i = 0; i < sizeof(device_type_preference) / sizeof(vk::PhysicalDeviceType); i++) {
            if (count_device_type[static_cast<int>(device_type_preference[i])]) {
                search_for_device_type = device_type_preference[i];
                break;
            }
        }

        for (uint32_t i = 0; i < gpu_count; i++) {
            const auto physicalDeviceProperties = physical_devices[i].getProperties();
            if (physicalDeviceProperties.deviceType == search_for_device_type) {
                gpu_number = i;
                break;
            }
        }
    }
    if (gpu_number == (uint32_t)-1) {
        ERR_EXIT("physical device auto-select failed.\n", "Device Selection Failure");
    }
    return physical_devices[gpu_number];
}

vk::UniqueSurfaceKHR create_surface(const vk::Instance& instance, HINSTANCE hInstance, HWND hWnd) {
    auto const surf_info = vk::Win32SurfaceCreateInfoKHR()
        .setHinstance(hInstance)
        .setHwnd(hWnd);

    auto surface_handle = instance.createWin32SurfaceKHRUnique(surf_info);
    VERIFY(surface_handle.result == vk::Result::eSuccess);
    return std::move(surface_handle.value);
}

uint32_t find_queue_family(const vk::PhysicalDevice& gpu, const vk::SurfaceKHR& surface) {
    // Call with nullptr data to get count
    uint32_t queue_family_count = 0;
    gpu.getQueueFamilyProperties(&queue_family_count, static_cast<vk::QueueFamilyProperties*>(nullptr));
    VERIFY(queue_family_count >= 1);

    std::unique_ptr<vk::QueueFamilyProperties[]> queue_props;
    queue_props.reset(new vk::QueueFamilyProperties[queue_family_count]);
    gpu.getQueueFamilyProperties(&queue_family_count, queue_props.get());

    // Iterate over each queue to learn whether it supports presenting:
    std::unique_ptr<vk::Bool32[]> supportsPresent(new vk::Bool32[queue_family_count]);
    for (uint32_t i = 0; i < queue_family_count; i++) {
        const auto result = gpu.getSurfaceSupportKHR(i, surface, &supportsPresent[i]);
        VERIFY(result == vk::Result::eSuccess);
    }

    uint32_t graphics_queue_family_index = UINT32_MAX;
    uint32_t present_queue_family_index = UINT32_MAX;
    for (uint32_t i = 0; i < queue_family_count; i++) {
        if (queue_props[i].queueFlags & vk::QueueFlagBits::eGraphics) {
            if (graphics_queue_family_index == UINT32_MAX) {
                graphics_queue_family_index = i;
            }

            if (supportsPresent[i] == VK_TRUE) {
                graphics_queue_family_index = i;
                present_queue_family_index = i;
                break;
            }
        }
    }

    if (present_queue_family_index == UINT32_MAX) {
        // If didn't find a queue that supports both graphics and present, then
        // find a separate present queue.
        for (uint32_t i = 0; i < queue_family_count; ++i) {
            if (supportsPresent[i] == VK_TRUE) {
                present_queue_family_index = i;
                break;
            }
        }
    }

    // Generate error if could not find both a graphics and a present queue
    if (graphics_queue_family_index == UINT32_MAX || present_queue_family_index == UINT32_MAX) {
        ERR_EXIT("Could not find both graphics and present queues\n", "Swapchain Initialization Failure");
    }
    if (graphics_queue_family_index != present_queue_family_index) {
        ERR_EXIT("Separate graphics and present queues not supported\n", "Swapchain Initialization Failure");
    }
    return graphics_queue_family_index;
}

vk::UniqueDevice create_device(const vk::PhysicalDevice& gpu, uint32_t family_index) {
    float const priorities[1] = { 0.0 };

    vk::DeviceQueueCreateInfo queues[2];
    queues[0].setQueueFamilyIndex(family_index);
    queues[0].setQueueCount(1);
    queues[0].setPQueuePriorities(priorities);

    // Look for device extensions
    uint32_t enabled_extension_count = 0;
    char const* extension_names[64];

    uint32_t device_extension_count = 0;
    vk::Bool32 swapchainExtFound = VK_FALSE;
    enabled_extension_count = 0;
    memset(extension_names, 0, sizeof(extension_names));

    const auto result = gpu.enumerateDeviceExtensionProperties(nullptr, &device_extension_count, static_cast<vk::ExtensionProperties*>(nullptr));
    VERIFY(result == vk::Result::eSuccess);

    if (device_extension_count > 0) {
        std::unique_ptr<vk::ExtensionProperties[]> device_extensions(new vk::ExtensionProperties[device_extension_count]);
        const auto result = gpu.enumerateDeviceExtensionProperties(nullptr, &device_extension_count, device_extensions.get());
        VERIFY(result == vk::Result::eSuccess);

        for (uint32_t i = 0; i < device_extension_count; i++) {
            if (!strcmp(VK_KHR_SWAPCHAIN_EXTENSION_NAME, device_extensions[i].extensionName)) {
                swapchainExtFound = 1;
                extension_names[enabled_extension_count++] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
            }
            if (!strcmp("VK_KHR_portability_subset", device_extensions[i].extensionName)) {
                extension_names[enabled_extension_count++] = "VK_KHR_portability_subset";
            }
            VERIFY(enabled_extension_count < 64);
        }
    }

    if (!swapchainExtFound) {
        ERR_EXIT("vkEnumerateDeviceExtensionProperties failed to find the " VK_KHR_SWAPCHAIN_EXTENSION_NAME
            " extension.\n\n"
            "Do you have a compatible Vulkan installable client driver (ICD) installed?\n"
            "Please look at the Getting Started guide for additional information.\n",
            "vkCreateInstance Failure");
    }

    auto device_info = vk::DeviceCreateInfo()
        .setQueueCreateInfoCount(1)
        .setPQueueCreateInfos(queues)
        .setEnabledLayerCount(0)
        .setPpEnabledLayerNames(nullptr)
        .setEnabledExtensionCount(enabled_extension_count)
        .setPpEnabledExtensionNames((const char* const*)extension_names)
        .setPEnabledFeatures(nullptr);

    auto device_handle = gpu.createDeviceUnique(device_info);
    VERIFY(device_handle.result == vk::Result::eSuccess);
    return std::move(device_handle.value);
}

vk::SurfaceFormatKHR select_format(const vk::PhysicalDevice& gpu, const vk::SurfaceKHR& surface) {
    // Get the list of VkFormat's that are supported:
    uint32_t format_count = 0;
    auto result = gpu.getSurfaceFormatsKHR(surface, &format_count, static_cast<vk::SurfaceFormatKHR*>(nullptr));
    VERIFY(result == vk::Result::eSuccess);
    VERIFY(format_count > 0);

    std::unique_ptr<vk::SurfaceFormatKHR[]> surface_formats(new vk::SurfaceFormatKHR[format_count]);
    result = gpu.getSurfaceFormatsKHR(surface, &format_count, surface_formats.get());
    VERIFY(result == vk::Result::eSuccess);

    // If the format list includes just one entry of VK_FORMAT_UNDEFINED,
    // the surface has no preferred format.  Otherwise, at least one
    // supported format will be returned.
    vk::SurfaceFormatKHR res = surface_formats[0];
    if (surface_formats[0].format == vk::Format::eUndefined)
        res.format = vk::Format::eB8G8R8A8Unorm;
    return res;
}

vk::Queue fetch_queue(const vk::Device& device, uint32_t family_index) {
    vk::Queue queue;
    device.getQueue(family_index, 0, &queue);
    return queue;
}

vk::UniqueCommandPool create_command_pool(const vk::Device& device, uint32_t family_index) {
    auto const cmd_pool_info = vk::CommandPoolCreateInfo()
        .setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer)
        .setQueueFamilyIndex(family_index);

    auto cmd_pool_handle = device.createCommandPoolUnique(cmd_pool_info);
    VERIFY(cmd_pool_handle.result == vk::Result::eSuccess);
    return std::move(cmd_pool_handle.value);
}

vk::UniqueDescriptorSetLayout create_descriptor_layout(const vk::Device& device) {
    vk::DescriptorSetLayoutBinding const layout_bindings[1] = { 
        vk::DescriptorSetLayoutBinding()
           .setBinding(0)
           .setDescriptorType(vk::DescriptorType::eUniformBuffer)
           .setDescriptorCount(1)
           .setStageFlags(vk::ShaderStageFlagBits::eVertex)
           .setPImmutableSamplers(nullptr) };

    auto const desc_layout_info = vk::DescriptorSetLayoutCreateInfo()
        .setBindingCount(1)
        .setPBindings(layout_bindings);

    auto desc_layout_handle = device.createDescriptorSetLayoutUnique(desc_layout_info);
    VERIFY(desc_layout_handle.result == vk::Result::eSuccess);
    return std::move(desc_layout_handle.value);
}

vk::UniquePipelineLayout create_pipeline_layout(const vk::Device& device, const vk::DescriptorSetLayout& desc_layout) {
    auto const pipeline_layout_info = vk::PipelineLayoutCreateInfo()
        .setSetLayoutCount(1)
        .setPSetLayouts(&desc_layout);

    auto pipeline_layout_handle = device.createPipelineLayoutUnique(pipeline_layout_info);
    VERIFY(pipeline_layout_handle.result == vk::Result::eSuccess);
    return std::move(pipeline_layout_handle.value);
}

vk::UniqueRenderPass create_render_pass(const vk::Device& device, const vk::SurfaceFormatKHR& surface_format) {
    // The initial layout for the color and depth attachments will be LAYOUT_UNDEFINED
    // because at the start of the renderpass, we don't care about their contents.
    // At the start of the subpass, the color attachment's layout will be transitioned
    // to LAYOUT_COLOR_ATTACHMENT_OPTIMAL and the depth stencil attachment's layout
    // will be transitioned to LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL.  At the end of
    // the renderpass, the color attachment's layout will be transitioned to
    // LAYOUT_PRESENT_SRC_KHR to be ready to present.  This is all done as part of
    // the renderpass, no barriers are necessary.
    const vk::AttachmentDescription attachments[1] = {
        vk::AttachmentDescription()
          .setFormat(surface_format.format)
          .setSamples(vk::SampleCountFlagBits::e1)
          .setLoadOp(vk::AttachmentLoadOp::eClear)
          .setStoreOp(vk::AttachmentStoreOp::eStore)
          .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
          .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
          .setInitialLayout(vk::ImageLayout::eUndefined)
          .setFinalLayout(vk::ImageLayout::ePresentSrcKHR)
    };

    auto const color_reference = vk::AttachmentReference()
        .setAttachment(0)
        .setLayout(vk::ImageLayout::eColorAttachmentOptimal);

    auto const subpass = vk::SubpassDescription()
        .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
        .setInputAttachmentCount(0)
        .setPInputAttachments(nullptr)
        .setColorAttachmentCount(1)
        .setPColorAttachments(&color_reference)
        .setPResolveAttachments(nullptr)
        .setPreserveAttachmentCount(0)
        .setPPreserveAttachments(nullptr);

    vk::SubpassDependency const dependencies[1] = {
        vk::SubpassDependency()  // Image layout transition
            .setSrcSubpass(VK_SUBPASS_EXTERNAL)
            .setDstSubpass(0)
            .setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
            .setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
            .setSrcAccessMask(vk::AccessFlagBits())
            .setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eColorAttachmentRead)
            .setDependencyFlags(vk::DependencyFlags()),
    };

    auto const rp_info = vk::RenderPassCreateInfo()
        .setAttachmentCount(1)
        .setPAttachments(attachments)
        .setSubpassCount(1)
        .setPSubpasses(&subpass)
        .setDependencyCount(1)
        .setPDependencies(dependencies);

    auto render_pass_handle = device.createRenderPassUnique(rp_info);
    VERIFY(render_pass_handle.result == vk::Result::eSuccess);
    return std::move(render_pass_handle.value);
}

const uint32_t vert_bytecode[] =
#include "shader.vert.inc"
;
const uint32_t frag_bytecode[] =
#include "shader.frag.inc"
;

vk::UniqueShaderModule create_module(const vk::Device& device, const uint32_t* code, size_t size) {
    const auto module_info = vk::ShaderModuleCreateInfo()
        .setCodeSize(size)
        .setPCode(code);

    auto module_handle = device.createShaderModuleUnique(module_info);
    VERIFY(module_handle.result == vk::Result::eSuccess);
    return std::move(module_handle.value);
}

vk::UniquePipeline create_pipeline(const vk::Device& device, const vk::PipelineLayout& pipeline_layout, const vk::RenderPass& render_pass) {
    const auto vert_shader_module = create_module(device, vert_bytecode, sizeof(vert_bytecode));
    const auto frag_shader_module = create_module(device, frag_bytecode, sizeof(frag_bytecode));

    vk::PipelineShaderStageCreateInfo const shader_stage_info[2] = {
        vk::PipelineShaderStageCreateInfo()
            .setStage(vk::ShaderStageFlagBits::eVertex)
            .setModule(vert_shader_module.get())
            .setPName("main"),
        vk::PipelineShaderStageCreateInfo()
            .setStage(vk::ShaderStageFlagBits::eFragment)
            .setModule(frag_shader_module.get())
            .setPName("main") };

    vk::PipelineVertexInputStateCreateInfo const vertex_input_info;

    auto const input_assembly_info = vk::PipelineInputAssemblyStateCreateInfo()
        .setTopology(vk::PrimitiveTopology::eTriangleList);

    auto const viewport_info = vk::PipelineViewportStateCreateInfo()
        .setViewportCount(1)
        .setScissorCount(1);

    auto const rasterization_info = vk::PipelineRasterizationStateCreateInfo()
        .setDepthClampEnable(VK_FALSE)
        .setRasterizerDiscardEnable(VK_FALSE)
        .setPolygonMode(vk::PolygonMode::eFill)
        .setCullMode(vk::CullModeFlagBits::eBack)
        .setFrontFace(vk::FrontFace::eCounterClockwise)
        .setDepthBiasEnable(VK_FALSE)
        .setLineWidth(1.0f);

    auto const multisample_info = vk::PipelineMultisampleStateCreateInfo();

    auto const depth_stencil_info = vk::PipelineDepthStencilStateCreateInfo()
        .setDepthTestEnable(VK_FALSE)
        .setDepthWriteEnable(VK_FALSE)
        .setDepthBoundsTestEnable(VK_FALSE)
        .setStencilTestEnable(VK_FALSE);

    vk::PipelineColorBlendAttachmentState const color_blend_attachments[1] = {
        vk::PipelineColorBlendAttachmentState()
            .setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA) };

    auto const color_blend_info = vk::PipelineColorBlendStateCreateInfo()
        .setAttachmentCount(1)
        .setPAttachments(color_blend_attachments);

    vk::DynamicState const dynamic_states[2] = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };

    auto const dynamic_state_info = vk::PipelineDynamicStateCreateInfo()
        .setPDynamicStates(dynamic_states)
        .setDynamicStateCount(2);

    auto const pipeline_info = vk::GraphicsPipelineCreateInfo()
        .setStageCount(2)
        .setPStages(shader_stage_info)
        .setPVertexInputState(&vertex_input_info)
        .setPInputAssemblyState(&input_assembly_info)
        .setPViewportState(&viewport_info)
        .setPRasterizationState(&rasterization_info)
        .setPMultisampleState(&multisample_info)
        .setPDepthStencilState(&depth_stencil_info)
        .setPColorBlendState(&color_blend_info)
        .setPDynamicState(&dynamic_state_info)
        .setLayout(pipeline_layout)
        .setRenderPass(render_pass);

    auto pipeline_handles = device.createGraphicsPipelinesUnique(nullptr, pipeline_info);
    VERIFY(pipeline_handles.result == vk::Result::eSuccess);
    return std::move(pipeline_handles.value[0]);
}

vk::UniqueDescriptorPool create_descriptor_pool(const vk::Device& device) {
    vk::DescriptorPoolSize const pool_sizes[1] = { 
        vk::DescriptorPoolSize()
            .setType(vk::DescriptorType::eUniformBuffer)
            .setDescriptorCount(16)
    };

    auto const desc_pool_info = vk::DescriptorPoolCreateInfo()
        .setMaxSets(16)
        .setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
        .setPoolSizeCount(1)
        .setPPoolSizes(pool_sizes);

    auto desc_pool_handle = device.createDescriptorPoolUnique(desc_pool_info);
    VERIFY(desc_pool_handle.result == vk::Result::eSuccess);
    return std::move(desc_pool_handle.value);
}

vk::UniqueFence create_fence(const vk::Device& device) {
    auto const fence_info = vk::FenceCreateInfo()
        .setFlags(vk::FenceCreateFlagBits::eSignaled);

    auto fence_handle = device.createFenceUnique(fence_info);
    VERIFY(fence_handle.result == vk::Result::eSuccess);
    return std::move(fence_handle.value);
}

vk::UniqueSemaphore create_semaphore(const vk::Device& device) {
    auto const semaphore_info = vk::SemaphoreCreateInfo();

    auto semaphore_handle = device.createSemaphoreUnique(semaphore_info);
    VERIFY(semaphore_handle.result == vk::Result::eSuccess);
    return std::move(semaphore_handle.value);
}

vk::UniqueCommandBuffer create_command_buffer(const vk::Device& device, const vk::CommandPool& cmd_pool) {
    auto const cmd_info = vk::CommandBufferAllocateInfo()
        .setCommandPool(cmd_pool)
        .setLevel(vk::CommandBufferLevel::ePrimary)
        .setCommandBufferCount(1);

    auto cmd_handles = device.allocateCommandBuffersUnique(cmd_info);
    VERIFY(cmd_handles.result == vk::Result::eSuccess);
    return std::move(cmd_handles.value[0]);
}

vk::UniqueSwapchainKHR create_swapchain(const vk::PhysicalDevice& gpu, const vk::Device& device, const vk::SurfaceKHR& surface, const vk::SurfaceFormatKHR& surface_format, const vk::SwapchainKHR& old_swapchain, int32_t window_width, int32_t window_height) {
    vk::SurfaceCapabilitiesKHR surf_caps;
    const auto result = gpu.getSurfaceCapabilitiesKHR(surface, &surf_caps);
    VERIFY(result == vk::Result::eSuccess);

    vk::Extent2D extent;
    // width and height are either both -1, or both not -1.
    if (surf_caps.currentExtent.width == (uint32_t)-1) {
        // If the surface size is undefined, the size is set to the size of the images requested.
        extent.width = window_width;
        extent.height = window_height;
    }
    else {
        // If the surface size is defined, the swap chain size must match
        extent = surf_caps.currentExtent;
    }

    // Determine the number of VkImages to use in the swap chain.
    // Application desires to acquire 3 images at a time for triple
    // buffering
    uint32_t min_image_count = 3;
    if (min_image_count < surf_caps.minImageCount) {
        min_image_count = surf_caps.minImageCount;
    }

    // If maxImageCount is 0, we can ask for as many images as we want, otherwise we're limited to maxImageCount
    if ((surf_caps.maxImageCount > 0) && (min_image_count > surf_caps.maxImageCount)) {
        // Application must settle for fewer images than desired:
        min_image_count = surf_caps.maxImageCount;
    }

    const auto preTransform = surf_caps.supportedTransforms & vk::SurfaceTransformFlagBitsKHR::eIdentity ?
        vk::SurfaceTransformFlagBitsKHR::eIdentity : surf_caps.currentTransform;

    // Find a supported composite alpha mode - one of these is guaranteed to be set
    vk::CompositeAlphaFlagBitsKHR compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    vk::CompositeAlphaFlagBitsKHR compositeAlphaFlags[4] = {
        vk::CompositeAlphaFlagBitsKHR::eOpaque,
        vk::CompositeAlphaFlagBitsKHR::ePreMultiplied,
        vk::CompositeAlphaFlagBitsKHR::ePostMultiplied,
        vk::CompositeAlphaFlagBitsKHR::eInherit,
    };
    for (uint32_t i = 0; i < 4; i++) {
        if (surf_caps.supportedCompositeAlpha & compositeAlphaFlags[i]) {
            compositeAlpha = compositeAlphaFlags[i];
            break;
        }
    }

    auto const swapchain_info = vk::SwapchainCreateInfoKHR()
        .setSurface(surface)
        .setMinImageCount(min_image_count)
        .setImageFormat(surface_format.format)
        .setImageColorSpace(surface_format.colorSpace)
        .setImageExtent({ extent.width, extent.height })
        .setImageArrayLayers(1)
        .setImageUsage(vk::ImageUsageFlagBits::eColorAttachment)
        .setImageSharingMode(vk::SharingMode::eExclusive)
        .setQueueFamilyIndexCount(0)
        .setPQueueFamilyIndices(nullptr)
        .setPreTransform(preTransform)
        .setCompositeAlpha(compositeAlpha)
        .setPresentMode(vk::PresentModeKHR::eFifo)
        .setClipped(true)
        .setOldSwapchain(old_swapchain);

    auto swapchain_handle = device.createSwapchainKHRUnique(swapchain_info);
    VERIFY(swapchain_handle.result == vk::Result::eSuccess);
    return std::move(swapchain_handle.value);
}

vk::UniqueImageView create_image_view(const vk::Device& device, const vk::Image& image, const vk::SurfaceFormatKHR& surface_format) {
    auto view_info = vk::ImageViewCreateInfo()
        .setViewType(vk::ImageViewType::e2D)
        .setFormat(surface_format.format)
        .setImage(image)
        .setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));

    auto view_handle = device.createImageViewUnique(view_info);
    VERIFY(view_handle.result == vk::Result::eSuccess);
    return std::move(view_handle.value);
}

vk::UniqueFramebuffer create_framebuffer(const vk::Device& device, const vk::RenderPass& render_pass, const vk::ImageView& image_view, int32_t window_width, int32_t window_height) {
    const vk::ImageView attachments[1] = { image_view };

    auto const fb_info = vk::FramebufferCreateInfo()
        .setRenderPass(render_pass)
        .setAttachmentCount(1)
        .setPAttachments(attachments)
        .setWidth((uint32_t)window_width)
        .setHeight((uint32_t)window_height)
        .setLayers(1);

    auto framebuffer_handle = device.createFramebufferUnique(fb_info);
    VERIFY(framebuffer_handle.result == vk::Result::eSuccess);
    return std::move(framebuffer_handle.value);
}

vk::UniqueBuffer create_uniform_buffer(const vk::Device& device, size_t size) {
    auto const buf_info = vk::BufferCreateInfo()
        .setSize(size)
        .setUsage(vk::BufferUsageFlagBits::eUniformBuffer);

    auto buffer_handle = device.createBufferUnique(buf_info);
    VERIFY(buffer_handle.result == vk::Result::eSuccess);
    return std::move(buffer_handle.value);
}

bool memory_type_from_properties(const vk::PhysicalDevice& gpu, uint32_t typeBits, vk::MemoryPropertyFlags requirements_mask, uint32_t* typeIndex) {
    vk::PhysicalDeviceMemoryProperties memory_properties;
    gpu.getMemoryProperties(&memory_properties);

    // Search memtypes to find first index with those properties
    for (uint32_t i = 0; i < VK_MAX_MEMORY_TYPES; i++) {
        if ((typeBits & 1) == 1) {
            // Type is available, does it match user properties?
            if ((memory_properties.memoryTypes[i].propertyFlags & requirements_mask) == requirements_mask) {
                *typeIndex = i;
                return true;
            }
        }
        typeBits >>= 1;
    }
    return false;
}

vk::UniqueDeviceMemory create_uniform_memory(const vk::PhysicalDevice& gpu, const vk::Device& device, const vk::Buffer& buffer) {
    vk::MemoryRequirements mem_reqs;
    device.getBufferMemoryRequirements(buffer, &mem_reqs);

    auto mem_info = vk::MemoryAllocateInfo()
        .setAllocationSize(mem_reqs.size)
        .setMemoryTypeIndex(0);

    bool const pass = memory_type_from_properties(gpu, mem_reqs.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, &mem_info.memoryTypeIndex);
    VERIFY(pass);

    auto memory_handle = device.allocateMemoryUnique(mem_info);
    VERIFY(memory_handle.result == vk::Result::eSuccess);
    return std::move(memory_handle.value);
}

void* map_memory(const vk::Device& device, const vk::DeviceMemory& memory) {
    void* mem = nullptr;
    const auto result = device.mapMemory(memory, 0, VK_WHOLE_SIZE, vk::MemoryMapFlags(), &mem);
    VERIFY(result == vk::Result::eSuccess);
    return mem;
}

void bind_memory(const vk::Device& device, const vk::Buffer& buffer, const vk::DeviceMemory& memory) {
    const auto result = device.bindBufferMemory(buffer, memory, 0);
    VERIFY(result == vk::Result::eSuccess);
}

vk::UniqueDescriptorSet create_descriptor_set(const vk::Device& device, const vk::DescriptorPool& desc_pool, const vk::DescriptorSetLayout& desc_layout) {
    auto const alloc_info = vk::DescriptorSetAllocateInfo()
        .setDescriptorPool(desc_pool)
        .setDescriptorSetCount(1)
        .setPSetLayouts(&desc_layout);

    auto descriptor_set_handles = device.allocateDescriptorSetsUnique(alloc_info);
    VERIFY(descriptor_set_handles.result == vk::Result::eSuccess);
    return std::move(descriptor_set_handles.value[0]);
}

void update_descriptor_set(const vk::Device& device, const vk::DescriptorSet& desc_set, const vk::Buffer& buffer, size_t range) {
    const auto buffer_info = vk::DescriptorBufferInfo()
        .setOffset(0)
        .setRange(range)
        .setBuffer(buffer);

    const vk::WriteDescriptorSet writes[1] = { vk::WriteDescriptorSet()
        .setDescriptorCount(1)
        .setDescriptorType(vk::DescriptorType::eUniformBuffer)
        .setPBufferInfo(&buffer_info)
        .setDstSet(desc_set)
    };

    device.updateDescriptorSets(1, writes, 0, nullptr);
}

void wait_idle(const vk::Device& device) {
    const auto result = device.waitIdle();
    VERIFY(result == vk::Result::eSuccess);
}

void wait(const vk::Device& device, const vk::Fence& fence) {
    // Ensure no more than frame lag renderings are outstanding
    const auto result = device.waitForFences(1, &fence, VK_TRUE, UINT64_MAX);
    VERIFY(result == vk::Result::eSuccess);
    device.resetFences({ fence });
}

uint32_t acquire(const vk::Device& device, const vk::SwapchainKHR& swapchain, const vk::Semaphore& signal_sema) {
    uint32_t image_index = 0;
    const auto result = device.acquireNextImageKHR(swapchain, UINT64_MAX, signal_sema, vk::Fence(), &image_index);
    VERIFY(result == vk::Result::eSuccess);
    return image_index;
}

void submit(const vk::Queue& queue, const vk::Semaphore& wait_sema, const vk::Semaphore& signal_sema, const vk::CommandBuffer& cmd_buf, const vk::Fence& fence) {
    // Wait for the image acquired semaphore to be signaled to ensure
    // that the image won't be rendered to until the presentation
    // engine has fully released ownership to the application, and it is
    // okay to render to the image.
    vk::PipelineStageFlags const pipe_stage_flags = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    auto const submit_info = vk::SubmitInfo()
        .setPWaitDstStageMask(&pipe_stage_flags)
        .setWaitSemaphoreCount(1)
        .setPWaitSemaphores(&wait_sema)
        .setSignalSemaphoreCount(1)
        .setPSignalSemaphores(&signal_sema)
        .setCommandBufferCount(1)
        .setPCommandBuffers(&cmd_buf);

    const auto result = queue.submit(1, &submit_info, fence);
    VERIFY(result == vk::Result::eSuccess);
}

void present(const vk::SwapchainKHR& swapchain, const vk::Queue& queue, const vk::Semaphore& wait_sema, uint32_t image_index) {
    auto const present_info = vk::PresentInfoKHR()
        .setWaitSemaphoreCount(1)
        .setPWaitSemaphores(&wait_sema)
        .setSwapchainCount(1)
        .setPSwapchains(&swapchain)
        .setPImageIndices(&image_index);

    const auto result = queue.presentKHR(&present_info);
    VERIFY(result == vk::Result::eSuccess);
}

void begin(const vk::CommandBuffer& cmd_buf) {
    auto const command_buffer_info = vk::CommandBufferBeginInfo()
        .setFlags(vk::CommandBufferUsageFlagBits::eSimultaneousUse);

    auto result = cmd_buf.begin(&command_buffer_info);
    VERIFY(result == vk::Result::eSuccess);
}

void end(const vk::CommandBuffer& cmd_buf) {
    auto result = cmd_buf.end();
    VERIFY(result == vk::Result::eSuccess);
}

void begin_pass(const vk::CommandBuffer& cmd_buf, const vk::RenderPass& render_pass, const vk::Framebuffer& framebuffer, const vk::ClearColorValue& clear_value, uint32_t width, uint32_t height) {
    auto const pass_info = vk::RenderPassBeginInfo()
        .setRenderPass(render_pass)
        .setFramebuffer(framebuffer)
        .setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(width, height)))
        .setClearValueCount(1)
        .setPClearValues((vk::ClearValue*)&clear_value);

    cmd_buf.beginRenderPass(&pass_info, vk::SubpassContents::eInline);
}

void end_pass(const vk::CommandBuffer& cmd_buf) {
    cmd_buf.endRenderPass(); // Note that ending the renderpass changes the image's layout from COLOR_ATTACHMENT_OPTIMAL to PRESENT_SRC_KHR
}

void set_viewport(const vk::CommandBuffer& cmd_buf, float width, float height) {
    float viewport_dimension;
    float viewport_x = 0.0f;
    float viewport_y = 0.0f;
    if (width < height) {
        viewport_dimension = width;
        viewport_y = (height - width) / 2.0f;
    }
    else {
        viewport_dimension = height;
        viewport_x = (width - height) / 2.0f;
    }
    auto const viewport = vk::Viewport()
        .setX(viewport_x)
        .setY(viewport_y)
        .setWidth((float)viewport_dimension)
        .setHeight((float)viewport_dimension)
        .setMinDepth((float)0.0f)
        .setMaxDepth((float)1.0f);
    cmd_buf.setViewport(0, 1, &viewport);
}

void set_scissor(const vk::CommandBuffer& cmd_buf, uint32_t width, uint32_t height) {
    vk::Rect2D const scissor(vk::Offset2D(0, 0), vk::Extent2D(width, height));
    cmd_buf.setScissor(0, 1, &scissor);
}

void bind_pipeline(const vk::CommandBuffer& cmd_buf, const vk::Pipeline& pipeline) {
    cmd_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
}

void bind_descriptor_set(const vk::CommandBuffer& cmd_buf, const vk::PipelineLayout& pipeline_layout, const vk::DescriptorSet& desc_set) {
    cmd_buf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline_layout, 0, 1, &desc_set, 0, nullptr);
}

void draw(const vk::CommandBuffer& cmd_buf, uint32_t vertex_count) {
    cmd_buf.draw(vertex_count, 1, 0, 0);
}

std::vector<vk::Image> get_swapchain_images(const vk::Device& device, const vk::SwapchainKHR& swapchain) {
    uint32_t count = 0;
    auto result = device.getSwapchainImagesKHR(swapchain, &count, static_cast<vk::Image*>(nullptr));
    VERIFY(result == vk::Result::eSuccess);

    std::vector<vk::Image> images(count);
    result = device.getSwapchainImagesKHR(swapchain, &count, images.data());
    VERIFY(result == vk::Result::eSuccess);

    return images;
}


static inline const float vertex_data[] = {
    -1.0f,-1.0f,-1.0f,  // -X side
    -1.0f,-1.0f, 1.0f,
    -1.0f, 1.0f, 1.0f,
    -1.0f, 1.0f, 1.0f,
    -1.0f, 1.0f,-1.0f,
    -1.0f,-1.0f,-1.0f,

    -1.0f,-1.0f,-1.0f,  // -Z side
     1.0f, 1.0f,-1.0f,
     1.0f,-1.0f,-1.0f,
    -1.0f,-1.0f,-1.0f,
    -1.0f, 1.0f,-1.0f,
     1.0f, 1.0f,-1.0f,

    -1.0f,-1.0f,-1.0f,  // -Y side
     1.0f,-1.0f,-1.0f,
     1.0f,-1.0f, 1.0f,
    -1.0f,-1.0f,-1.0f,
     1.0f,-1.0f, 1.0f,
    -1.0f,-1.0f, 1.0f,

    -1.0f, 1.0f,-1.0f,  // +Y side
    -1.0f, 1.0f, 1.0f,
     1.0f, 1.0f, 1.0f,
    -1.0f, 1.0f,-1.0f,
     1.0f, 1.0f, 1.0f,
     1.0f, 1.0f,-1.0f,

     1.0f, 1.0f,-1.0f,  // +X side
     1.0f, 1.0f, 1.0f,
     1.0f,-1.0f, 1.0f,
     1.0f,-1.0f, 1.0f,
     1.0f,-1.0f,-1.0f,
     1.0f, 1.0f,-1.0f,

    -1.0f, 1.0f, 1.0f,  // +Z side
    -1.0f,-1.0f, 1.0f,
     1.0f, 1.0f, 1.0f,
    -1.0f,-1.0f, 1.0f,
     1.0f,-1.0f, 1.0f,
     1.0f, 1.0f, 1.0f,
};


class Uniforms {
    float mvp[4][4];
    float position[12 * 3][4];

public:
    Uniforms(const mat4x4& MVP) {
        memcpy(mvp, MVP, sizeof(mat4x4));

        for (int32_t i = 0; i < 12 * 3; i++) {
            position[i][0] = vertex_data[i * 3];
            position[i][1] = vertex_data[i * 3 + 1];
            position[i][2] = vertex_data[i * 3 + 2];
            position[i][3] = 1.0f;
        }
    }
};

class Matrices {
    mat4x4 projection_matrix;
    mat4x4 view_matrix;
    mat4x4 model_matrix;

public:
    Matrices() {
        mat4x4_identity(model_matrix);
        mat4x4_identity(view_matrix);
        mat4x4_identity(projection_matrix);
    }

    void recompute(mat4x4& out_mvp) {
        vec3 eye = { 0.0f, 3.0f, 5.0f };
        vec3 origin = { 0, 0, 0 };
        vec3 up = { 0.0f, 1.0f, 0.0 };
        mat4x4_look_at(view_matrix, eye, origin, up);

        mat4x4_perspective(projection_matrix, (float)degreesToRadians(45.0f), 1.0f, 0.1f, 100.0f);
        projection_matrix[1][1] *= -1.0f; // Flip projection matrix from GL to Vulkan orientation.

        mat4x4 M;
        mat4x4_dup(M, model_matrix);
        mat4x4_rotate_Y(model_matrix, M, (float)degreesToRadians(1.5f));
        mat4x4_orthonormalize(model_matrix, model_matrix);

        mat4x4 VP;
        mat4x4_mul(VP, projection_matrix, view_matrix);

        mat4x4_mul(out_mvp, VP, model_matrix);
    }
};

class Frame {
    vk::Image image;
    vk::UniqueCommandBuffer cmd;
    vk::UniqueImageView view;
    vk::UniqueBuffer uniform_buffer;
    vk::UniqueDeviceMemory uniform_memory;
    void* uniform_memory_ptr = nullptr;
    vk::UniqueFramebuffer framebuffer;
    vk::UniqueDescriptorSet descriptor_set;

public:
    Frame(const vk::PhysicalDevice& gpu, const vk::Device& device, const vk::CommandPool& cmd_pool,
        const vk::DescriptorPool& desc_pool, const vk::DescriptorSetLayout& desc_layout, const vk::RenderPass& render_pass,
        const vk::Image& swapchain_image, const vk::SurfaceFormatKHR& surface_format, uint32_t width, uint32_t height) {
        image = swapchain_image;
        view = create_image_view(device, swapchain_image, surface_format);

        uniform_buffer = create_uniform_buffer(device, sizeof(Uniforms));
        uniform_memory = create_uniform_memory(gpu, device, uniform_buffer.get());
        bind_memory(device, uniform_buffer.get(), uniform_memory.get());
        uniform_memory_ptr = map_memory(device, uniform_memory.get());

        cmd = create_command_buffer(device, cmd_pool);
        descriptor_set = create_descriptor_set(device, desc_pool, desc_layout);
        update_descriptor_set(device, descriptor_set.get(), uniform_buffer.get(), sizeof(Uniforms));
        framebuffer = create_framebuffer(device, render_pass, view.get(), width, height);
    }

    void patch(const mat4x4& MVP) {
        new(uniform_memory_ptr) Uniforms(MVP);
    }

    void record(const vk::RenderPass& render_pass, const vk::Pipeline& pipeline, const vk::PipelineLayout& pipeline_layout,
        const std::array<float, 4>& color, unsigned vertex_count, unsigned width, unsigned height) {
        begin(cmd.get());
        begin_pass(cmd.get(), render_pass, framebuffer.get(), color, width, height);

        bind_pipeline(cmd.get(), pipeline);
        bind_descriptor_set(cmd.get(), pipeline_layout, descriptor_set.get());

        set_viewport(cmd.get(), (float)width, (float)height);
        set_scissor(cmd.get(), width, height);

        draw(cmd.get(), vertex_count);

        end_pass(cmd.get());
        end(cmd.get());
    }

    void finish(const vk::Queue& queue, const vk::Semaphore& wait_sema, const vk::Semaphore& signal_sema, const vk::Fence& fence) {
        submit(queue, wait_sema, signal_sema, cmd.get(), fence);
    }
};

class Swapchain {
    vk::UniqueCommandPool cmd_pool;
    vk::UniqueDescriptorPool desc_pool;
    vk::UniqueRenderPass render_pass;
    vk::UniqueDescriptorSetLayout desc_layout;
    vk::UniquePipelineLayout pipeline_layout;
    vk::UniquePipeline pipeline;
    vk::UniqueSwapchainKHR swapchain;

    std::vector<Frame> frames;

    static const unsigned frame_lag = 2;
    vk::UniqueFence fences[frame_lag];
    vk::UniqueSemaphore image_acquired_semaphores[frame_lag];
    vk::UniqueSemaphore draw_complete_semaphores[frame_lag];
    uint32_t fence_index = 0;

public:
    Swapchain() {}
    Swapchain(const vk::Device& device, const vk::SurfaceFormatKHR& surface_format, uint32_t family_index) {
        cmd_pool = create_command_pool(device, family_index);
        desc_pool = create_descriptor_pool(device);
        desc_layout = create_descriptor_layout(device);
        pipeline_layout = create_pipeline_layout(device, desc_layout.get());
        render_pass = create_render_pass(device, surface_format);
        pipeline = create_pipeline(device, pipeline_layout.get(), render_pass.get());

        for (uint32_t i = 0; i < frame_lag; i++) {
            fences[i] = create_fence(device);
            image_acquired_semaphores[i] = create_semaphore(device);
            draw_complete_semaphores[i] = create_semaphore(device);
        }
    }

    void resize(const vk::PhysicalDevice gpu, const vk::Device& device, const vk::SurfaceKHR& surface, const vk::SurfaceFormatKHR& surface_format, unsigned width, unsigned height) {
        swapchain = create_swapchain(gpu, device, surface, surface_format, swapchain.get(), width, height);
        const auto swapchain_images = get_swapchain_images(device, swapchain.get());
        frames.clear();
        for (uint32_t i = 0; i < swapchain_images.size(); ++i) {
            frames.emplace_back(gpu, device, cmd_pool.get(), desc_pool.get(), desc_layout.get(), render_pass.get(), swapchain_images[i], surface_format, width, height);
        }
    }

    void redraw(const vk::Device& device, const vk::Queue& queue, const mat4x4& MVP, unsigned width, unsigned height, const std::array<float, 4>& color, unsigned vertex_count) {
        wait(device, fences[fence_index].get());
        const auto frame_index = acquire(device, swapchain.get(), image_acquired_semaphores[fence_index].get());
        frames[frame_index].patch(MVP);
        frames[frame_index].record(render_pass.get(), pipeline.get(), pipeline_layout.get(), color, vertex_count, width, height);
        frames[frame_index].finish(queue, image_acquired_semaphores[fence_index].get(), draw_complete_semaphores[fence_index].get(), fences[fence_index].get());
        present(swapchain.get(), queue, draw_complete_semaphores[fence_index].get(), frame_index);
        fence_index += 1;
        fence_index %= frame_lag;
    }
};

class Device {
    vk::UniqueInstance instance;
    vk::PhysicalDevice gpu;
    vk::UniqueSurfaceKHR surface;
    vk::SurfaceFormatKHR surface_format;
    vk::UniqueDevice device;
    vk::Queue queue;

    Swapchain swapchain;

    Matrices matrices;

    unsigned width = 800;
    unsigned height = 600;

public:
    Device(WNDPROC proc, HINSTANCE hInstance, int nCmdShow) {
        instance = create_instance();
        gpu = pick_gpu(instance.get());
        auto hWnd = create_window(proc, hInstance, nCmdShow, this, width, height);
        surface = create_surface(instance.get(), hInstance, hWnd);
        surface_format = select_format(gpu, surface.get());
        auto family_index = find_queue_family(gpu, surface.get());
        device = create_device(gpu, family_index);
        queue = fetch_queue(device.get(), family_index);

        swapchain = Swapchain(device.get(), surface_format, family_index);

        resize(width, height);
    }

    ~Device() {
        wait_idle(device.get());
    }

    void resize(unsigned w, unsigned h) {
        width = w;
        height = h;

        if (device) {
            wait_idle(device.get());
            swapchain.resize(gpu, device.get(), surface.get(), surface_format, width, height);
        }
    }

    void redraw() {
        const std::array<float, 4> color = { 0.2f, 0.2f, 0.2f, 1.0f };
        const auto vertex_count = 12 * 3;
        mat4x4 mvp;
        matrices.recompute(mvp);
        swapchain.redraw(device.get(), queue, mvp, width, height, color, vertex_count);
    }
};

