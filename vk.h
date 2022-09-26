#pragma once

vk::UniqueInstance create_instance() {
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

    int32_t gpu_number = -1;
    {
        uint32_t count_device_type[VK_PHYSICAL_DEVICE_TYPE_CPU + 1];
        memset(count_device_type, 0, sizeof(count_device_type));

        for (uint32_t i = 0; i < gpu_count; i++) {
            const auto physicalDeviceProperties = physical_devices[i].getProperties();
            const auto device_type = static_cast<int>(physicalDeviceProperties.deviceType);
            if (device_type <= VK_PHYSICAL_DEVICE_TYPE_CPU)
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
    uint32_t queue_family_count = 0;
    gpu.getQueueFamilyProperties(&queue_family_count, static_cast<vk::QueueFamilyProperties*>(nullptr));
    VERIFY(queue_family_count >= 1);

    std::unique_ptr<vk::QueueFamilyProperties[]> queue_props;
    queue_props.reset(new vk::QueueFamilyProperties[queue_family_count]);
    gpu.getQueueFamilyProperties(&queue_family_count, queue_props.get());

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
        for (uint32_t i = 0; i < queue_family_count; ++i) {
            if (supportsPresent[i] == VK_TRUE) {
                present_queue_family_index = i;
                break;
            }
        }
    }

    if (graphics_queue_family_index == UINT32_MAX || present_queue_family_index == UINT32_MAX) {
        ERR_EXIT("Could not find both graphics and present queues\n", "Swapchain Initialization Failure");
    }
    if (graphics_queue_family_index != present_queue_family_index) {
        ERR_EXIT("Separate graphics and present queues not supported\n", "Swapchain Initialization Failure");
    }
    return graphics_queue_family_index;
}

vk::UniqueDevice create_device(const vk::PhysicalDevice& gpu, uint32_t family_index) {
    std::array<float, 1> const priorities = { 0.0 };

    std::array<vk::DeviceQueueCreateInfo, 1> queues = { vk::DeviceQueueCreateInfo()
        .setQueueFamilyIndex(family_index)
        .setQueueCount((uint32_t)priorities.size())
        .setPQueuePriorities(priorities.data()) };

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
        .setQueueCreateInfoCount((uint32_t)queues.size())
        .setPQueueCreateInfos(queues.data())
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
    uint32_t format_count = 0;
    auto result = gpu.getSurfaceFormatsKHR(surface, &format_count, static_cast<vk::SurfaceFormatKHR*>(nullptr));
    VERIFY(result == vk::Result::eSuccess);
    VERIFY(format_count > 0);

    std::unique_ptr<vk::SurfaceFormatKHR[]> surface_formats(new vk::SurfaceFormatKHR[format_count]);
    result = gpu.getSurfaceFormatsKHR(surface, &format_count, surface_formats.get());
    VERIFY(result == vk::Result::eSuccess);

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
    std::array<vk::DescriptorSetLayoutBinding, 1> const layout_bindings = { 
        vk::DescriptorSetLayoutBinding()
           .setBinding(0)
           .setDescriptorType(vk::DescriptorType::eUniformBuffer)
           .setDescriptorCount(1)
           .setStageFlags(vk::ShaderStageFlagBits::eVertex)
           .setPImmutableSamplers(nullptr) };

    auto const desc_layout_info = vk::DescriptorSetLayoutCreateInfo()
        .setBindingCount((uint32_t)layout_bindings.size())
        .setPBindings(layout_bindings.data());

    auto desc_layout_handle = device.createDescriptorSetLayoutUnique(desc_layout_info);
    VERIFY(desc_layout_handle.result == vk::Result::eSuccess);
    return std::move(desc_layout_handle.value);
}

vk::UniquePipelineLayout create_pipeline_layout(const vk::Device& device, const vk::DescriptorSetLayout& desc_layout, uint32_t constants_size) {
    std::array<vk::PushConstantRange, 1> const push_constants = {
        vk::PushConstantRange()
            .setOffset(0)
            .setSize(constants_size)
            .setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment) };

    auto const pipeline_layout_info = vk::PipelineLayoutCreateInfo()
        .setPushConstantRangeCount((uint32_t)push_constants.size())
        .setPPushConstantRanges(push_constants.data())
        .setSetLayoutCount(1)
        .setPSetLayouts(&desc_layout);

    auto pipeline_layout_handle = device.createPipelineLayoutUnique(pipeline_layout_info);
    VERIFY(pipeline_layout_handle.result == vk::Result::eSuccess);
    return std::move(pipeline_layout_handle.value);
}

vk::UniqueRenderPass create_render_pass(const vk::Device& device, const vk::SurfaceFormatKHR& surface_format) {
    const std::array<vk::AttachmentDescription, 1> attachments = {
        vk::AttachmentDescription()
            .setFormat(surface_format.format)
            .setSamples(vk::SampleCountFlagBits::e1)
            .setLoadOp(vk::AttachmentLoadOp::eClear)
            .setStoreOp(vk::AttachmentStoreOp::eStore)
            .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
            .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
            .setInitialLayout(vk::ImageLayout::eUndefined)
            .setFinalLayout(vk::ImageLayout::ePresentSrcKHR) };

    std::array<vk::AttachmentReference, 1> const color_references = {
        vk::AttachmentReference()
            .setAttachment(0)
            .setLayout(vk::ImageLayout::eColorAttachmentOptimal) };

    auto const subpass = vk::SubpassDescription()
        .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
        .setInputAttachmentCount(0)
        .setPInputAttachments(nullptr)
        .setColorAttachmentCount((uint32_t)color_references.size())
        .setPColorAttachments(color_references.data())
        .setPResolveAttachments(nullptr)
        .setPreserveAttachmentCount(0)
        .setPPreserveAttachments(nullptr);

    std::array<vk::SubpassDependency, 1> const dependencies = {
        vk::SubpassDependency()
            .setSrcSubpass(VK_SUBPASS_EXTERNAL)
            .setDstSubpass(0)
            .setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
            .setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
            .setSrcAccessMask(vk::AccessFlagBits())
            .setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eColorAttachmentRead)
            .setDependencyFlags(vk::DependencyFlags()), };

    auto const rp_info = vk::RenderPassCreateInfo()
        .setAttachmentCount((uint32_t)attachments.size())
        .setPAttachments(attachments.data())
        .setSubpassCount(1)
        .setPSubpasses(&subpass)
        .setDependencyCount((uint32_t)dependencies.size())
        .setPDependencies(dependencies.data());

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

    std::array<vk::PipelineShaderStageCreateInfo, 2> const shader_stage_infos = {
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

    std::array<vk::PipelineColorBlendAttachmentState, 1> const color_blend_attachments = {
        vk::PipelineColorBlendAttachmentState()
            .setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA) };

    auto const color_blend_info = vk::PipelineColorBlendStateCreateInfo()
        .setAttachmentCount((uint32_t)color_blend_attachments.size())
        .setPAttachments(color_blend_attachments.data());

    std::array<vk::DynamicState, 2> const dynamic_states = {
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor };

    auto const dynamic_state_info = vk::PipelineDynamicStateCreateInfo()
        .setDynamicStateCount((uint32_t)dynamic_states.size())
        .setPDynamicStates(dynamic_states.data());

    auto const pipeline_info = vk::GraphicsPipelineCreateInfo()
        .setStageCount((uint32_t)shader_stage_infos.size())
        .setPStages(shader_stage_infos.data())
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
    std::array<vk::DescriptorPoolSize, 1> const pool_sizes = { 
        vk::DescriptorPoolSize()
            .setType(vk::DescriptorType::eUniformBuffer)
            .setDescriptorCount(16) };

    auto const desc_pool_info = vk::DescriptorPoolCreateInfo()
        .setMaxSets(16)
        .setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
        .setPoolSizeCount((uint32_t)pool_sizes.size())
        .setPPoolSizes(pool_sizes.data());

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

vk::UniqueSwapchainKHR create_swapchain(const vk::PhysicalDevice& gpu, const vk::Device& device, const vk::SurfaceKHR& surface, const vk::SurfaceFormatKHR& surface_format, const vk::SwapchainKHR& old_swapchain, int32_t window_width, int32_t window_height, uint32_t image_count) {
    vk::SurfaceCapabilitiesKHR surf_caps;
    const auto result = gpu.getSurfaceCapabilitiesKHR(surface, &surf_caps);
    VERIFY(result == vk::Result::eSuccess);

    vk::Extent2D extent;
    if (surf_caps.currentExtent.width == (uint32_t)-1) {
        extent.width = window_width;
        extent.height = window_height;
    }
    else {
        extent = surf_caps.currentExtent;
    }

    uint32_t min_image_count = image_count;
    if (min_image_count < surf_caps.minImageCount) {
        min_image_count = surf_caps.minImageCount;
    }

    if ((surf_caps.maxImageCount > 0) && (min_image_count > surf_caps.maxImageCount)) {
        min_image_count = surf_caps.maxImageCount;
    }

    const auto preTransform = surf_caps.supportedTransforms & vk::SurfaceTransformFlagBitsKHR::eIdentity ?
        vk::SurfaceTransformFlagBitsKHR::eIdentity : surf_caps.currentTransform;

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
    const std::array<vk::ImageView, 1> attachments = { image_view };

    auto const fb_info = vk::FramebufferCreateInfo()
        .setRenderPass(render_pass)
        .setAttachmentCount((uint32_t)attachments.size())
        .setPAttachments(attachments.data())
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

    for (uint32_t i = 0; i < VK_MAX_MEMORY_TYPES; i++) {
        if ((typeBits & 1) == 1) {
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

void unmap_memory(const vk::Device& device, const vk::DeviceMemory& memory) {
    device.unmapMemory(memory);
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

    const std::array<vk::WriteDescriptorSet, 1> writes = {
        vk::WriteDescriptorSet()
            .setDescriptorCount(1)
            .setDescriptorType(vk::DescriptorType::eUniformBuffer)
            .setPBufferInfo(&buffer_info)
            .setDstSet(desc_set) };

    device.updateDescriptorSets((uint32_t)writes.size(), writes.data(), 0, nullptr);
}

void wait_idle(const vk::Device& device) {
    const auto result = device.waitIdle();
    VERIFY(result == vk::Result::eSuccess);
}

void wait(const vk::Device& device, const vk::Fence& fence) {
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
    cmd_buf.endRenderPass();
}

void set_viewport(const vk::CommandBuffer& cmd_buf, float width, float height) {
    auto const viewport = vk::Viewport()
        .setX(0.0f)
        .setY(0.0f)
        .setWidth(width)
        .setHeight(height)
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

void push(const vk::CommandBuffer& cmd_buf, const vk::PipelineLayout& pipeline_layout, uint32_t size, const void* data) {
    cmd_buf.pushConstants(pipeline_layout, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, size, data);
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


constexpr float pi = 3.141592654f;

constexpr unsigned pack(unsigned a, unsigned b, unsigned c, unsigned d)
{
    return (a << 6) | (b << 4) | (c << 2) | (d);
}

class vector2
{
public:
    float x = 0.0f, y = 0.0f;

    vector2() noexcept {}
    vector2(const float& f) noexcept { x = y = f; }
    vector2(const float& X, const float& Y) noexcept { x = X; y = Y; }

    using V = float[2];
    float& operator[](const unsigned int i) { return ((V&)*this)[i]; }
    const float& operator[](const unsigned int i) const { return ((V&)*this)[i]; }

    V& as_array() { return ( V& )*this; }

    vector2 operator+(const vector2& o) const { return vector2(x + o[0], y + o[1]); }
    vector2 operator-(const vector2& o) const { return vector2(x - o[0], y - o[1]); }
    vector2 operator*(const vector2& o) const { return vector2(x * o[0], y * o[1]); }
    vector2 operator/(const vector2& o) const { return vector2(x / o[0], y / o[1]); }

    vector2& operator+=(const vector2& o) { x += o[0]; y += o[1]; return *this; }
    vector2& operator-=(const vector2& o) { x -= o[0]; y -= o[1]; return *this; }
    vector2& operator*=(const vector2& o) { x *= o[0]; y *= o[1]; return *this; }
    vector2& operator/=(const vector2& o) { x /= o[0]; y /= o[1]; return *this; }

    vector2 operator*(const float f) const { return vector2(x * f, y * f); }
    vector2 operator/(const float f) const { return vector2(x / f, y / f); }

    vector2 operator-() const { return vector2(-x, -y); };

    bool operator==( const vector2& o ) const { return x == o[0] && y == o[1]; }
    bool operator!=( const vector2& o ) const { return !( *this == o ); }

    vector2 normalize() const { const float s = len(); const float inv_s = s > 0.0f ? 1.0f / s : s; return *this * inv_s; }

    vector2 lerp(const vector2& o, float t) const { return *this + (o - *this) * t; }
    
    vector2 abs() const { return vector2(fabs(x), fabs(y)); }

    float sqrlen() const { return x * x + y * y; }
    float len() const { return sqrt(sqrlen()); }
    float dot(const vector2& o) const { return x * o[0] + y * o[1]; }
};

class vector3
{
public:
    float x = 0.0f, y = 0.0f, z = 0.0f;

    vector3() noexcept {}
    vector3(const float& f) noexcept { x = y = z = f; }
    vector3(const float& X, const float& Y, const float& Z) noexcept { x = X; y = Y; z = Z; }
    vector3( const float( &v )[3] ) noexcept { *this = ( vector3& )v; }
    explicit vector3(const vector2& v, const float& z) noexcept : vector3(v.x, v.y, z) {}

    vector2 xy() const { return vector2(x, y); };

    using V = float[3];
    float& operator[](const unsigned int i) { return ((V&)*this)[i]; }
    const float& operator[](const unsigned int i) const { return ((V&)*this)[i]; }

    V& as_array() { return ( V& )*this; }

    vector3 operator+(const vector3& o) const { return vector3(x + o[0], y + o[1], z + o[2]); }
    vector3 operator-(const vector3& o) const { return vector3(x - o[0], y - o[1], z - o[2]); }
    vector3 operator*(const vector3& o) const { return vector3(x * o[0], y * o[1], z * o[2]); }
    vector3 operator/(const vector3& o) const { return vector3(x / o[0], y / o[1], z / o[2]); }

    vector3& operator+=(const vector3& o) { x += o[0]; y += o[1]; z += o[2]; return *this; }
    vector3& operator-=(const vector3& o) { x -= o[0]; y -= o[1]; z -= o[2]; return *this; }
    vector3& operator*=(const vector3& o) { x *= o[0]; y *= o[1]; z *= o[2]; return *this; }
    vector3& operator/=(const vector3& o) { x /= o[0]; y /= o[1]; z /= o[2]; return *this; }

    vector3 operator+(const float f) const { return vector3(x + f, y + f, z + f); }
    vector3 operator-(const float f) const { return vector3(x - f, y - f, z - f); }
    vector3 operator*(const float f) const { return vector3(x * f, y * f, z * f); }
    vector3 operator/(const float f) const { return vector3(x / f, y / f, z / f); }

    bool operator>=(const vector3& o) const { return (x >= o[0]) && (y >= o[1]) && (z >= o[2]); }
    bool operator<=(const vector3& o) const { return (x <= o[0]) && (y <= o[1]) && (z <= o[2]); }

    vector3 operator-() const { return vector3(-x, -y, -z); };

    bool operator==( const vector3& o ) const { return x == o[0] && y == o[1] && z == o[2]; }
    bool operator!=( const vector3& o ) const { return !( *this == o ); }

    float dot(const vector3& o) const { return x * o[0] + y * o[1] + z * o[2]; }
    vector3 cross(const vector3& o) const { return vector3(y * o[2] - z * o[1], z * o[0] - x * o[2], x * o[1] - y * o[0]); }

    vector3 normalize() const { const float s = len(); const float inv_s = s > 0.0f ? 1.0f / s : s; return *this * inv_s; }

    vector3 lerp(const vector3& o, float t) const { return *this + (o - *this) * t; }
    
    vector3 min(const vector3& o) const { return vector3(std::min(x, o[0]), std::min(y, o[1]), std::min(z, o[2])); }
    vector3 max(const vector3& o) const { return vector3(std::max(x, o[0]), std::max(y, o[1]), std::max(z, o[2])); }

    float sqrlen() const { return x * x + y * y + z * z; }
    float len() const { return sqrt(sqrlen()); }

    static vector3 catmullrom(const vector3& p0, const vector3& p1, const vector3& p2, const vector3& p3, const float t) {
        const float t2 = t*t;
        const float t3 = t2*t;
        return ((p1 * 2.0f) + (-p0 + p2) * t + (p0 * 2.0f - p1 * 5.0f + p2 * 4.0f - p3) * t2 + (-p0 + p1 * 3.0f - p2 * 3.0f + p3) * t3) * 0.5f;
    }

    static vector3 bezier(const vector3& p0, const vector3& p1, const vector3& p2, const vector3& p3, const float t) {
        const vector3 a = p0.lerp( p1, t);
        const vector3 b = p1.lerp( p2, t);
        const vector3 c = p2.lerp( p3, t);
        const vector3 d = a.lerp(b, t);
        const vector3 e = d.lerp(c, t);
        return d.lerp(e, t);
    }

    static void barycentric(const vector3& p, const vector3& a, const vector3& b, const vector3& c, float& u, float& v)
    {
        const auto v0 = b - a, v1 = c - a, v2 = p - a;
        const float d00 = v0.dot(v0);
        const float d01 = v0.dot(v1);
        const float d11 = v1.dot(v1);
        const float d20 = v2.dot(v0);
        const float d21 = v2.dot(v1);
        const float denom = d00 * d11 - d01 * d01;
        const float w = (d00 * d21 - d01 * d20) / denom;
        v = (d11 * d20 - d01 * d21) / denom;
        u = 1.0f - v - w;
    }
};

struct uint4
{
    uint32_t v[4];

    friend struct uint4_sse2;
    friend struct uint4_sse4;
    friend struct uint4_avx;
    friend struct uint4_avx2;
    friend struct uint4_neon;
    friend struct float4_std;

public:
    uint4() {}
    explicit uint4(const uint32_t& a) { v[0] = a; v[1] =a; v[2] = a; v[3] = a; }
    explicit uint4(const uint32_t& x, const uint32_t& y, const uint32_t& z, const uint32_t& w) { v[0] = x; v[1] = y; v[2] = z; v[3] = w; }
    explicit uint4(const uint32_t* p, const uint4& i) { v[0] = p[i[0]]; v[1] = p[i[1]]; v[2] = p[i[2]]; v[3] = p[i[3]]; }
    uint4(const uint4& o) { v[0] = o[0]; v[1] = o[1]; v[2] = o[2]; v[3] = o[3]; }

    uint32_t& operator[](const unsigned int i) { return v[i]; }
    const uint32_t& operator[](const unsigned int i) const { return v[i]; }

    uint4 operator+(const uint4& o) const { return uint4(v[0] + o[0], v[1] + o[1], v[2] + o[2], v[3] + o[3]); }
    uint4 operator-(const uint4& o) const { return uint4(v[0] - o[0], v[1] - o[1], v[2] - o[2], v[3] - o[3]); }
    uint4 operator*(const uint4& o) const { return uint4(v[0] * o[0], v[1] * o[1], v[2] * o[2], v[3] * o[3]); }

    uint4 operator==(const uint4& o) const { return uint4(v[0] == o[0] ? (uint32_t)-1 : 0, v[1] == o[1] ? (uint32_t)-1 : 0, v[2] == o[2] ? (uint32_t)-1 : 0, v[3] == o[3] ? (uint32_t)-1 : 0); }

    uint4 operator&(const uint4& o) const { return uint4(v[0] & o[0], v[1] & o[1], v[2] & o[2], v[3] & o[3]); }
    uint4 operator|(const uint4& o) const { return uint4(v[0] | o[0], v[1] | o[1], v[2] | o[2], v[3] | o[3]); }

    template <unsigned S> uint4 shiftleft() const { return uint4(v[0] << S, v[1] << S, v[2] << S, v[3] << S); }
    template <unsigned S> uint4 shiftright()const { return uint4(v[0] >> S, v[1] >> S, v[2] >> S, v[3] >> S); }

    static uint4 load(const uint4& a) { return a; }

    static void store(uint4& dst, const uint4& src) { dst = src; }

    static uint4 one() { return uint4((uint32_t)-1); }
};
struct float4
{
public:
    union
    {
        float v[4];
        struct { float x, y, z, w; };
    };

    float4() {}
    explicit float4(const float& a) { v[0] = a; v[1] =a; v[2] = a; v[3] = a; }
    explicit float4(const float& x, const float& y, const float& z, const float& w) { v[0] = x; v[1] = y; v[2] = z; v[3] = w; }
    float4(const float4& o) { v[0] = o[0]; v[1] = o[1]; v[2] = o[2]; v[3] = o[3]; }

    bool validate() const { return !std::isnan(v[0]) && !std::isnan(v[1]) && !std::isnan(v[2]) && !std::isnan(v[3]); }
    
    float& operator[](const unsigned int i) { return v[i]; }
    const float& operator[](const unsigned int i) const { return v[i]; }

    float4 operator+(const float4& o) const { return float4(v[0] + o[0], v[1] + o[1], v[2] + o[2], v[3] + o[3]); }
    float4 operator-(const float4& o) const { return float4(v[0] - o[0], v[1] - o[1], v[2] - o[2], v[3] - o[3]); }
    float4 operator*(const float4& o) const { return float4(v[0] * o[0], v[1] * o[1], v[2] * o[2], v[3] * o[3]); }
    float4 operator/(const float4& o) const { return float4(v[0] / o[0], v[1] / o[1], v[2] / o[2], v[3] / o[3]); }

    float4 operator-() const { return float4(-v[0], -v[1], -v[2], -v[3]); };

    static float4 load(const float4& a) { return a; }

    static void store(float4& dst, const float4& src) { dst = src; }
    static void stream(float4& dst, const float4& src) { dst = src; }

    static float dot(const float4& a, const float4& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3]; }

    static float4 zero() { return float4(0.0f); }

    static float4 sin(const float4& a) { return float4(sinf(a[0]), sinf(a[1]), sinf(a[2]), sinf(a[3])); }
    static float4 cos(const float4& a) { return float4(cosf(a[0]), cosf(a[1]), cosf(a[2]), cosf(a[3])); }
    static float4 acos(const float4& a) { return float4(acosf(a[0]), acosf(a[1]), acosf(a[2]), acosf(a[3])); }

    static float4 mod(const float4& a, const float4& b) { return float4(fmod(a[0], b[0]), fmod(a[1], b[1]), fmod(a[2], b[2]), fmod(a[3], b[3])); }

    static float4 sqrt(const float4& a) { return float4(sqrtf(a[0]), sqrtf(a[1]), sqrtf(a[2]), sqrtf(a[3])); }
    static float4 invsqrt(const float4& a) { return float4(1.0f / sqrtf(a[0]), 1.0f / sqrtf(a[1]), 1.0f / sqrtf(a[2]), 1.0f / sqrtf(a[3])); }

    static float4 min(const float4& l, const float4& r) { return float4(std::min(l[0], r[0]), std::min(l[1], r[1]), std::min(l[2], r[2]), std::min(l[3], r[3])); }
    static float4 max(const float4& l, const float4& r) { return float4(std::max(l[0], r[0]), std::max(l[1], r[1]), std::max(l[2], r[2]), std::max(l[3], r[3])); }

    static float4 lerp(const float4& a, const float4& b, const float4& t) { return a + (b - a) * t; }

    static float4 muladd(const float4& a, const float4& b, const float4& c) { return (a * b) + c; };
    static float4 mulsub(const float4& a, const float4& b, const float4& c) { return (a * b) - c; };

    static float4 floor(const float4& o) { return float4(floorf(o[0]), floorf(o[1]), floorf(o[2]), floorf(o[3])); }

    template<unsigned I> static float4 shuffle(const float4& a, const float4& b) {
        const unsigned _0 = (I >> 6) & 0x3;
        const unsigned _1 = (I >> 4) & 0x3;
        const unsigned _2 = (I >> 2) & 0x3;
        const unsigned _3 = (I >> 0) & 0x3;
        return float4(a[_3], a[_2], b[_1], b[_0]);
    }
    template<unsigned I> static float4 permute(const float4& a) { return shuffle<I>(a, a); }
};

class vector4 : public float4
{
public:
    vector4() noexcept {}
    vector4(const float& f) : float4(f) {}
    vector4(const float& x, const float& y, const float& z, const float& w) : float4(x, y, z, w) {}
    vector4(const std::array<float, 4>& v) : float4((float4&)v) {}
    vector4(const float4& o) { (float4&)*this = o; }
    vector4(const vector4& o) { *this = o; }

    using V = float[4];
    V& as_array() { return ( V& )*this; }

    vector4 operator+(const float f) const { return (float4&)*this + float4(f); }
    vector4 operator-(const float f) const { return (float4&)*this - float4(f); }

    vector4 operator+(const vector4& o) const { return (float4&)*this + o; }
    vector4 operator-(const vector4& o) const { return (float4&)*this - o; }
    vector4 operator*(const vector4& o) const { return (float4&)*this * o; }
    vector4 operator/(const vector4& o) const { return (float4&)*this / o; }

    bool operator==( const vector4& o ) const { return x == o.x && y == o.y && z == o.z && w == o.w; }
    bool operator!=( const vector4& o ) const { return !( *this == o ); }

    vector4 lerp(const vector4& o, float t) const { return *this + (o - *this) * t; }

    float sqrlen() const { return dot(*this, *(this)); }
    float len() const { return sqrtf(sqrlen()); }
};

class quaternion : public float4
{
public:
    quaternion() noexcept {}
    quaternion(const float& x, const float& y, const float& z, const float& w) : float4(x, y, z, w) {}
    quaternion(const float4& o) { (float4&)*this = o; }
    quaternion(const quaternion& o) { *this = o; }
    explicit quaternion(const vector3& v, const float& w) : float4(v.x, v.y, v.z, w) {}

    using V = float[4];
    V& as_array() { return ( V& )*this; }

    quaternion operator*(const float f) const { return (float4&)*this * float4(f); }
    quaternion operator/(const float f) const { return (float4&)*this / float4(f); }

    quaternion operator*(const quaternion& o) const {
        return quaternion(
            (o.w * x) + (o.x * w) + (o.y * z) - (o.z * y),
            (o.w * y) - (o.x * z) + (o.y * w) + (o.z * x),
            (o.w * z) + (o.x * y) - (o.y * x) + (o.z * w),
            (o.w * w) - (o.x * x) - (o.y * y) - (o.z * z));
    }

    bool operator==( const quaternion& o ) const { return x == o.x && y == o.y && z == o.z && w == o.w; }
    bool operator!=( const quaternion& o ) const { return !( *this == o ); }

    quaternion operator-() const { return float4::operator-(); };

    quaternion normalize() const { const float s = len(); const float inv_s = s > 0.0f ? 1.0f / s : s; return *this * inv_s; }

    quaternion conjugate() const { return quaternion(-x, -y, -z, w); }

    quaternion inverse() const {
        const float sq = sqrlen();
        const float inv_sq = sq > 0.0f ? 1.0f / sq : sq;
        return conjugate() * inv_sq;
    }

    vector3 transform(const vector3& v) const {
        const vector3 u(x, y, z);
        const vector3 t = u.cross(v) * 2.0f;
        return v + t * w + u.cross(t);
    }

    float sqrlen() const { return dot(*this, *(this)); }
    float len() const { return sqrtf(sqrlen()); }

    quaternion slerp(const quaternion& o, float t) const { 
        assert (t >= 0.0f && t <= 1.0f);

        float cosine = dot(*this, o);
        const quaternion& qi = (cosine < 0.0f) ? -o : o;
        cosine =  (cosine < 0.0f) ? -cosine : cosine;
        cosine = std::min(cosine, 1.0f);

        const float angle = (float)acosf(cosine);
        const float inv_sine = (angle > 0.0001f) ? 1.0f / sinf(angle) : 0.0f;

        const vector4 A = vector4(sinf((1.0f - t) * angle) * inv_sine);
        const vector4 B = vector4(sinf(t * angle) * inv_sine);

        const quaternion qn = (angle > 0.0001f) ? A * (vector4&)(*this) + (vector4&)B * (vector4&)qi : (vector4&)(o);
        return qn.normalize();
    }

    static quaternion axisangle(const vector3& axis, float angle)
    {
        const float a(angle);
        const float s = sinf(a * 0.5f);

        quaternion result;
        result.w = cosf(a * 0.5f);
        result.x = axis.x * s;
        result.y = axis.y * s;
        result.z = axis.z * s;

        return result;
    }

    static quaternion identity() {
        return quaternion(0.0f, 0.0f, 0.0f, 1.0f);
    }

    static quaternion yawpitchroll(const float yaw, const float pitch, const float roll)
    {
        quaternion q;

        const float t0 = cosf(yaw * 0.5f);
        const float t1 = sinf(yaw * 0.5f);
        const float t2 = cosf(roll * 0.5f);
        const float t3 = sinf(roll * 0.5f);
        const float t4 = cosf(pitch * 0.5f);
        const float t5 = sinf(pitch * 0.5f);

        q.w = t0 * t2 * t4 + t1 * t3 * t5;
        q.x = t0 * t2 * t5 + t1 * t3 * t4;
        q.y = t1 * t2 * t4 - t0 * t3 * t5;
        q.z = t0 * t3 * t4 - t1 * t2 * t5;

        return q;
    }
};

class float4x4
{
protected:
    float4 a[4];

public:
    float4x4() noexcept {}
    explicit float4x4(const float4& c) { a[0] = c; a[1] = c; a[2] = c; a[3] = c; }
    explicit float4x4(const float4& c0, const float4& c1, const float4& c2, const float4& c3) { a[0] = c0; a[1] = c1; a[2] = c2; a[3] = c3; }
    float4x4(const float4x4& o) { a[0] = o[0]; a[1] = o[1]; a[2] = o[2]; a[3] = o[3]; }

    float4& operator[](const unsigned int i) { return a[i]; }
    const float4& operator[](const unsigned int i) const { return a[i]; }

    float4x4 operator+(const float4x4& o) const { return float4x4(a[0] + o[0], a[1] + o[1], a[2] + o[2], a[3] + o[3]); }

    float4x4 operator-() const { return float4x4(-a[0], -a[1], -a[2], -a[3]); }

    static float4x4 mul(const float4x4& a, const float4& f) { return float4x4(a[0] * f, a[1] * f, a[2] * f, a[3] * f); }
    static float4x4 div(const float4x4& a, const float4& f) { return float4x4(a[0] / f, a[1] / f, a[2] / f, a[3] / f); }

    //this one calculates transpose(c) = transpose(a) * transpose(b)
    //which is the same as c = b * a
    static float4x4 mulmat(const float4x4& a, const float4x4& b) {
        float4x4 t;
        for (int i=0; i < 4; i++)
            for (int j=0; j < 4; j++)
                t[i][j] = a[i][0] * b[0][j] + a[i][1] * b[1][j] + a[i][2] * b[2][j] + a[i][3] * b[3][j];
        return t;
    }
};

class matrix : public float4x4
{
public:
    matrix() noexcept {}
    matrix(const vector4& c0, const vector4& c1, const vector4& c2, const vector4& c3) { a[0] = c0; a[1] = c1; a[2] = c2; a[3] = c3; }
    matrix(const matrix& o) { a[0] = o[0]; a[1] = o[1]; a[2] = o[2]; a[3] = o[3]; }
    matrix(const float* v) {
        a[0] = vector4(v[ 0], v[ 1], v[ 2], v[ 3]);
        a[1] = vector4(v[ 4], v[ 5], v[ 6], v[ 7]);
        a[2] = vector4(v[ 8], v[ 9], v[10], v[11]);
        a[3] = vector4(v[12], v[13], v[14], v[15]);
    }

    bool validate() const { return a[0].validate() && a[1].validate() && a[2].validate() && a[3].validate(); }

    vector4& operator[](const unsigned int i) { return (vector4&)a[i]; }
    const vector4& operator[](const unsigned int i) const { return (vector4&)a[i]; }

    matrix operator*(const matrix& o) const { const auto m = mulmat(*this, o); return *(matrix*)&m; }
    
    vector4 operator*(const vector4& o) const {
        const auto t = transpose();
        const float a0 = vector4::dot(o, t[0]);
        const float a1 = vector4::dot(o, t[1]); 
        const float a2 = vector4::dot(o, t[2]); 
        const float a3 = vector4::dot(o, t[3]);
        return vector4(a0, a1, a2, a3);
    }

    bool operator==( const matrix& o ) const
    {
        const auto& res = *this;
        return res[0] == o[0] && res[1] == o[1] && res[2] == o[2] && res[3] == o[3];
    }

    bool operator!=( const matrix& o ) const { return !( *this == o ); }

    vector3 operator*(const vector3& o) const { auto res = (*this) * vector4(o[0], o[1], o[2], 0.0f); return vector3(res[0], res[1], res[2]); } // TODO: Optimize.

    vector3 mulpos(const vector3& o) const { auto res = (*this) * vector4(o[0], o[1], o[2], 1.0f); return vector3(res[0], res[1], res[2]); } // TODO: Optimize.
    vector3 muldir(const vector3& o) const { auto res = (*this) * vector4(o[0], o[1], o[2], 0.0f); return vector3(res[0], res[1], res[2]); } // TODO: Optimize.

    vector2 mulpos(const vector2& o) const { auto res = (*this) * vector4(o[0], o[1], 0.0f, 1.0f); return vector2(res[0], res[1]); } // TODO: Optimize.
    vector2 muldir(const vector2& o) const { auto res = (*this) * vector4(o[0], o[1], 0.0f, 0.0f); return vector2(res[0], res[1]); } // TODO: Optimize.
    
    vector3 scale() const {
        const vector3 c0(a[0][0], a[0][1], a[0][2]);
        const vector3 c1(a[1][0], a[1][1], a[1][2]);
        const vector3 c2(a[2][0], a[2][1], a[2][2]);
        return vector3(c0.len(), c1.len(), c2.len());
    }

    quaternion rotation() const {
        quaternion q;
        q.w = std::sqrt(std::max(0.0f, 1.0f + a[0][0] + a[1][1] + a[2][2])) * 0.5f;
        q.x = std::sqrt(std::max(0.0f, 1.0f + a[0][0] - a[1][1] - a[2][2])) * 0.5f;
        q.y = std::sqrt(std::max(0.0f, 1.0f - a[0][0] + a[1][1] - a[2][2])) * 0.5f;
        q.z = std::sqrt(std::max(0.0f, 1.0f - a[0][0] - a[1][1] + a[2][2])) * 0.5f;
        q.x = std::copysign(q.x, a[1][2] - a[2][1]);
        q.y = std::copysign(q.y, a[2][0] - a[0][2]);
        q.z = std::copysign(q.z, a[0][1] - a[1][0]);
        return q;
    }

    vector3 translation() const {
        return vector3(a[3][0], a[3][1], a[3][2]);
    }

    void decompose(vector3 &translation, matrix &rotation, vector3 &scale) const //*this = translation * rotation * scale. but in code operator * is transposed
    {
        scale = this->scale();
        translation = this->translation();

        constexpr float eps = 1e-7f;
        const vector3 inv_scale = vector3(1.0f / std::max(eps, scale.x), 1.0f / std::max(eps, scale.y), 1.0f / std::max(eps, scale.z));
        const vector3 inv_translation = -translation;

        rotation = (matrix::scale(inv_scale) * (*this) * matrix::translation(inv_translation)); //matrix multiplication order is reversed. it's actually translate * rotate * scale * vec
    }

    matrix transpose() const { 
        return matrix(
            vector4(a[0][0], a[1][0], a[2][0], a[3][0]),
            vector4(a[0][1], a[1][1], a[2][1], a[3][1]),
            vector4(a[0][2], a[1][2], a[2][2], a[3][2]),
            vector4(a[0][3], a[1][3], a[2][3], a[3][3]));
    }

    vector3 yawpitchroll() const {
        // Returns a vector v of rotations equivalent to rotationZ( v.z ) * rotationX( v.y ) * rotationY( v.x )
        // Algorithm taken from https://www.geometrictools.com/Documentation/EulerAngles.pdf#subsection.2.3
        vector3 output;
        if( a[ 2 ][ 1 ] < 1.0f )
        {
            if( a[ 2 ][ 1 ] > -1.0f )
            {
                output.y = asin( -a[ 2 ][ 1 ] );
                output.x = atan2( a[ 2 ][ 0 ], a[ 2 ][ 2 ] );
                output.z = atan2( a[ 0 ][ 1 ], a[ 1 ][ 1 ] );
            }
            else
            {
                output.y = pi / 2.0f;
                output.x = -atan2( -a[ 1 ][ 0 ], a[ 0 ][ 0 ] );
                output.z = 0.0f;
            }
        }
        else
        {
            output.y = -pi / 2.0f;
            output.x = atan2( -a[ 1 ][ 0 ], a[ 0 ][ 0 ] );
            output.z = 0.0f;
        }
        return output;
    }

    matrix inverse() const {
        const matrix MT = transpose();

        vector4 V00 = vector4::permute<pack(1, 1, 0, 0)>(MT.a[2]);
        vector4 V10 = vector4::permute<pack(3, 2, 3, 2)>(MT.a[3]);
        vector4 V01 = vector4::permute<pack(1, 1, 0, 0)>(MT.a[0]);
        vector4 V11 = vector4::permute<pack(3, 2, 3, 2)>(MT.a[1]);
        vector4 V02 = vector4::shuffle<pack(2, 0, 2, 0)>(MT.a[2], MT.a[0]);
        vector4 V12 = vector4::shuffle<pack(3, 1, 3, 1)>(MT.a[3], MT.a[1]);

        vector4 D0 = V00 * V10;
        vector4 D1 = V01 * V11;
        vector4 D2 = V02 * V12;

        V00 = vector4::permute<pack(3, 2, 3, 2)>(MT.a[2]);
        V10 = vector4::permute<pack(1, 1, 0, 0)>(MT.a[3]);
        V01 = vector4::permute<pack(3, 2, 3, 2)>(MT.a[0]);
        V11 = vector4::permute<pack(1, 1, 0, 0)>(MT.a[1]);
        V02 = vector4::shuffle<pack(3, 1, 3, 1)>(MT.a[2], MT.a[0]);
        V12 = vector4::shuffle<pack(2, 0, 2, 0)>(MT.a[3], MT.a[1]);

        V00 = V00 * V10;
        V01 = V01 * V11;
        V02 = V02 * V12;
        D0 = D0 - V00;
        D1 = D1 - V01;
        D2 = D2 - V02;

        V11 = vector4::shuffle<pack(1, 1, 3, 1)>(D0, D2);
        V00 = vector4::permute<pack(1, 0, 2, 1)>(MT.a[1]);
        V10 = vector4::shuffle<pack(0, 3, 0, 2)>(V11, D0);
        V01 = vector4::permute<pack(0, 1, 0, 2)>(MT.a[0]);
        V11 = vector4::shuffle<pack(2, 1, 2, 1)>(V11, D0);
        
        vector4 V13 = vector4::shuffle<pack(3, 3, 3, 1)>(D1, D2);
        V02 = vector4::permute<pack(1, 0, 2, 1)>(MT.a[3]);
        V12 = vector4::shuffle<pack(0, 3, 0, 2)>(V13, D1);
        vector4 V03 = vector4::permute<pack(0, 1, 0, 2)>(MT.a[2]);
        V13 = vector4::shuffle<pack(2, 1, 2, 1)>(V13, D1);

        vector4 C0 = V00 * V10;
        vector4 C2 = V01 * V11;
        vector4 C4 = V02 * V12;
        vector4 C6 = V03 * V13;

        V11 = vector4::shuffle<pack(0, 0, 1, 0)>(D0, D2);
        V00 = vector4::permute<pack(2, 1, 3, 2)>(MT.a[1]);
        V10 = vector4::shuffle<pack(2, 1, 0, 3)>(D0, V11);
        V01 = vector4::permute<pack(1, 3, 2, 3)>(MT.a[0]);
        V11 = vector4::shuffle<pack(0, 2, 1, 2)>(D0, V11);

        V13 = vector4::shuffle<pack(2, 2, 1, 0)>(D1, D2);
        V02 = vector4::permute<pack(2, 1, 3, 2)>(MT.a[3]);
        V12 = vector4::shuffle<pack(2, 1, 0, 3)>(D1, V13);
        V03 = vector4::permute<pack(1, 3, 2, 3)>(MT.a[2]);
        V13 = vector4::shuffle<pack(0, 2, 1, 2)>(D1, V13);

        V00 = V00 * V10;
        V01 = V01 * V11;
        V02 = V02 * V12;
        V03 = V03 * V13;
        C0 = C0 - V00;
        C2 = C2 - V01;
        C4 = C4 - V02;
        C6 = C6 - V03;

        V00 = vector4::permute<pack(0, 3, 0, 3)>(MT.a[1]);
        
        V10 = vector4::shuffle<pack(1, 0, 2, 2)>(D0, D2);
        V10 = vector4::permute<pack(0, 2, 3, 0)>(V10);
        V01 = vector4::permute<pack(2, 0, 3, 1)>(MT.a[0]);
        
        V11 = vector4::shuffle<pack(1, 0, 3, 0)>(D0, D2);
        V11 = vector4::permute<pack(2, 1, 0, 3)>(V11);
        V02 = vector4::permute<pack(0, 3, 0, 3)>(MT.a[3]);
        
        V12 = vector4::shuffle<pack(3, 2, 2, 2)>(D1, D2);
        V12 = vector4::permute<pack(0, 2, 3, 0)>(V12);
        V03 = vector4::permute<pack(2, 0, 3, 1)>(MT.a[2]);

        V13 = vector4::shuffle<pack(3, 2, 3, 0)>(D1, D2);
        V13 = vector4::permute<pack(2, 1, 0, 3)>(V13);

        V00 = V00 * V10;
        V01 = V01 * V11;
        V02 = V02 * V12;
        V03 = V03 * V13;
        const vector4 C1 = C0 - V00;
        C0 = C0 + V00;
        const vector4 C3 = C2 + V01;
        C2 = C2 - V01;
        const vector4 C5 = C4 - V02;
        C4 = C4 + V02;
        const vector4 C7 = C6 + V03;
        C6 = C6 - V03;

        C0 = vector4::shuffle<pack(3, 1, 2, 0)>(C0, C1);
        C2 = vector4::shuffle<pack(3, 1, 2, 0)>(C2, C3);
        C4 = vector4::shuffle<pack(3, 1, 2, 0)>(C4, C5);
        C6 = vector4::shuffle<pack(3, 1, 2, 0)>(C6, C7);
        C0 = vector4::permute<pack(3, 1, 2, 0)>(C0);
        C2 = vector4::permute<pack(3, 1, 2, 0)>(C2);
        C4 = vector4::permute<pack(3, 1, 2, 0)>(C4);
        C6 = vector4::permute<pack(3, 1, 2, 0)>(C6);

        const vector4 det = vector4(vector4::dot(C0, MT.a[0]));
        const vector4 inv_det = vector4(1.0f) / det;

        return matrix(
            vector4(C0 * inv_det),
            vector4(C2 * inv_det),
            vector4(C4 * inv_det),
            vector4(C6 * inv_det));
    }

    vector4 determinant() const {
        vector4 V0 = vector4::permute<pack(0, 0, 0, 1)>(a[2]);
        vector4 V1 = vector4::permute<pack(1, 1, 2, 2)>(a[3]);
        vector4 V2 = vector4::permute<pack(0, 0, 0, 1)>(a[2]);
        vector4 V3 = vector4::permute<pack(2, 3, 3, 3)>(a[3]);
        vector4 V4 = vector4::permute<pack(1, 1, 2, 2)>(a[2]);
        vector4 V5 = vector4::permute<pack(2, 3, 3, 3)>(a[3]);

        vector4 P0 = V0 * V1;
        vector4 P1 = V2 * V3;
        vector4 P2 = V4 * V5;

        V0 = vector4::permute<pack(1, 1, 2, 2)>(a[2]);
        V1 = vector4::permute<pack(0, 0, 0, 1)>(a[3]);
        V2 = vector4::permute<pack(2, 3, 3, 3)>(a[2]);
        V3 = vector4::permute<pack(0, 0, 0, 1)>(a[3]);
        V4 = vector4::permute<pack(2, 3, 3, 3)>(a[2]);
        V5 = vector4::permute<pack(1, 1, 2, 2)>(a[3]);

        P0 = -vector4::mulsub(V0, V1, P0);
        P1 = -vector4::mulsub(V2, V3, P1);
        P2 = -vector4::mulsub(V4, V5, P2);

        V0 = vector4::permute<pack(2, 3, 3, 3)>(a[1]);
        V1 = vector4::permute<pack(1, 1, 2, 2)>(a[1]);
        V2 = vector4::permute<pack(0, 0, 0, 1)>(a[1]);

        const vector4 Sign(1.0f, -1.0f, 1.0f, -1.0f);
        const vector4 S = a[0] * Sign;
        vector4 R = V0 * P0;
        R = -vector4::mulsub(V1, P1, R);
        R = vector4::muladd(V2, P2, R);

        return vector4(vector4::dot(S, R));
    }

    static matrix zero() { 
        return matrix(
            vector4(0.0f, 0.0f, 0.0f, 0.0f),
            vector4(0.0f, 0.0f, 0.0f, 0.0f),
            vector4(0.0f, 0.0f, 0.0f, 0.0f),
            vector4(0.0f, 0.0f, 0.0f, 0.0f));
    }

    static matrix identity() { 
        return matrix(
            vector4(1.0f, 0.0f, 0.0f, 0.0f),
            vector4(0.0f, 1.0f, 0.0f, 0.0f),
            vector4(0.0f, 0.0f, 1.0f, 0.0f),
            vector4(0.0f, 0.0f, 0.0f, 1.0f));
    }

    static matrix flip_x( const bool flipped = true ) {
        return matrix(
            vector4( flipped ? -1.0f : 1.0f, 0.0f, 0.0f, 0.0f ),
            vector4( 0.0f, 1.0f, 0.0f, 0.0f ),
            vector4( 0.0f, 0.0f, 1.0f, 0.0f ),
            vector4( 0.0f, 0.0f, 0.0f, 1.0f ) );
    }

    static matrix flip( const bool flipped = true ) { return flip_x( flipped ); }

    static matrix translation(const float x, const float y, const float z) {
        return matrix(
            vector4(1.0f, 0.0f, 0.0f, 0.0f),
            vector4(0.0f, 1.0f, 0.0f, 0.0f),
            vector4(0.0f, 0.0f, 1.0f, 0.0f),
            vector4(x,    y,    z   , 1.0f));
    }

    static matrix translation(const vector3& v3) {
        return translation(v3[0], v3[1], v3[2]);
    }

    static matrix scale(const float x, const float y, const float z) {
        return matrix(
            vector4(x   , 0.0f, 0.0f, 0.0f),
            vector4(0.0f, y   , 0.0f, 0.0f),
            vector4(0.0f, 0.0f, z   , 0.0f),
            vector4(0.0f, 0.0f, 0.0f, 1.0f));
    }

    static matrix scale( const float s ) {
        return scale( s, s, s );
    }

    static matrix scale(const vector3& v3) {
        return scale( v3.x, v3.y, v3.z );
    }

    static matrix rotationX(const float angle) {
        const float cos = cosf(angle);
        const float sin = sinf(angle);
        return matrix(
            vector4(1.0f, 0.0f, 0.0f , 0.0f),
            vector4(0.0f, cos, sin, 0.0f),
            vector4(0.0f, -sin, cos, 0.0f),
            vector4(0.0f, 0.0f, 0.0f, 1.0f));
    }

    static matrix rotationY(const float angle) {
        const float cos = cosf(angle);
        const float sin = sinf(angle);
        return matrix(
            vector4(cos, 0.0f, -sin, 0.0f),
            vector4(0.0f, 1.0f, 0.0f, 0.0f),
            vector4(sin, 0.0f, cos, 0.0f),
            vector4(0.0f, 0.0f, 0.0f, 1.0f));
        }

    static matrix rotationZ(const float angle) {
        const float cos = cosf(angle);
        const float sin = sinf(angle);
        return matrix(
            vector4(cos, sin, 0.0f, 0.0f),
            vector4(-sin, cos, 0.0f, 0.0f),
            vector4(0.0f, 0.0f, 1.0f, 0.0f),
            vector4(0.0f, 0.0f, 0.0f, 1.0f));
    }

    static matrix rotationaxis(const vector3& axis, const float angle) {
        const vector3 v = axis.normalize();
        const float cos = cosf(angle);
        const float sin = sinf(angle);

        const vector4 N(v[0], v[1], v[2], 0.0f);

        const vector4 C2(1.0f - cos);
        const vector4 C1(cos);
        const vector4 C0(sin);

        const vector4 N0 = vector4::permute<pack(3, 0, 2, 1)>(N);
        const vector4 N1 = vector4::permute<pack(3, 1, 0, 2)>(N);

        vector4 V0 = C2 * N0;
        V0 = V0 * N1;

        vector4 R0 = C2 * N;
        R0 = R0 * N;
        R0 = R0 + C1;

        vector4 R1 = C0 * N;
        R1 = R1 + V0;
        vector4 R2 = C0 * N;
        R2 = V0 - R2;
        
        const auto mR0 = (uint4&)R0 & uint4(0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000);
        V0 = *(vector4*)&mR0;
        vector4 V1 = vector4::shuffle<pack(2, 1, 2, 0)>(R1, R2);
        V1 = vector4::permute<pack(0, 3, 2, 1)>(V1);
        vector4 V2 = vector4::shuffle<pack(0, 0, 1, 1)>(R1, R2);
        V2 = vector4::permute<pack(2, 0, 2, 0)>(V2);

        R2 = vector4::shuffle<pack(1, 0, 3, 0)>(V0, V1);
        R2 = vector4::permute<pack(1, 3, 2, 0)>(R2);

        vector4 R3 = vector4::shuffle<pack(3, 2, 3, 1)>(V0, V1);
        R3 = vector4::permute<pack(1, 3, 0, 2)>(R3);
        V2 = vector4::shuffle<pack(3, 2, 1, 0)>(V2, V0);

        return matrix(
            R2,
            R3,
            V2,
            vector4(0.0f, 0.0f, 0.0f, 1.0f));
    }

    static matrix rotationquaternion(const quaternion& rotation) {
        const float xx = rotation[0] * rotation[0];
        const float yy = rotation[1] * rotation[1];
        const float zz = rotation[2] * rotation[2];
        const float xy = rotation[0] * rotation[1];
        const float xz = rotation[0] * rotation[2];
        const float xw = rotation[0] * rotation[3];
        const float yz = rotation[1] * rotation[2];
        const float yw = rotation[1] * rotation[3];
        const float zw = rotation[2] * rotation[3];

        const float yypzz = 2.0f * (yy + zz);
        const float xxpzz = 2.0f * (xx + zz);
        const float xxpyy = 2.0f * (xx + yy);
        const float xypzw = 2.0f * (xy + zw);
        const float xymzw = 2.0f * (xy - zw);
        const float xzpyw = 2.0f * (xz + yw);
        const float xzmyw = 2.0f * (xz - yw);
        const float yzpxw = 2.0f * (yz + xw);
        const float yzmxw = 2.0f * (yz - xw);

        const float omyypzz = (1.0f - yypzz);
        const float omxxpzz = (1.0f - xxpzz);
        const float omxxpyy = (1.0f - xxpyy);

        return matrix(
            vector4(omyypzz, xypzw, xzmyw, 0.0f),
            vector4(xymzw, omxxpzz, yzpxw, 0.0f),
            vector4(xzpyw, yzmxw, omxxpyy, 0.0f),
            vector4(0.0f, 0.0f, 0.0f, 1.0f));
    }

    // Returns a matrix equivalent to rotationZ( roll ) * rotationX( pitch ) * rotationY( yaw )
    static matrix yawpitchroll(const float yaw, const float pitch, const float roll) { 
        const vector4 ypr = vector4(yaw, pitch, roll, 0.0f);
        const vector4 cos = vector4::cos(ypr);
        const vector4 sin = vector4::sin(ypr);
        return matrix(
            vector4(cos[2] * cos[0] + sin[2] * sin[1] * sin[0], sin[2] * cos[1], cos[2] * -sin[0] + sin[2] * sin[1] * cos[0], 0.0f),
            vector4(-sin[2] * cos[0] + cos[2] * sin[1] * sin[0], cos[2] * cos[1], sin[2] * sin[0] + cos[2] * sin[1] * cos[0], 0.0f),            
            vector4(cos[1] * sin[0], -sin[1], cos[1] * cos[0], 0.0f),
            vector4(0.0f, 0.0, 0.0, 1.0f));
    }

    static matrix yawpitchroll(vector3 v3) { 
        return yawpitchroll( v3.x, v3.y, v3.z );
    }

    static matrix scalerotationtranslation(const vector3& scale, const quaternion& rotation, const vector3& translation) { 
        const float xx = rotation[0] * rotation[0];
        const float yy = rotation[1] * rotation[1];
        const float zz = rotation[2] * rotation[2];
        const float xy = rotation[0] * rotation[1];
        const float xz = rotation[0] * rotation[2];
        const float xw = rotation[0] * rotation[3];
        const float yz = rotation[1] * rotation[2];
        const float yw = rotation[1] * rotation[3];
        const float zw = rotation[2] * rotation[3];

        const float yypzz = 2.0f * (yy + zz);
        const float xxpzz = 2.0f * (xx + zz);
        const float xxpyy = 2.0f * (xx + yy);
        const float xypzw = 2.0f * (xy + zw);
        const float xymzw = 2.0f * (xy - zw);
        const float xzpyw = 2.0f * (xz + yw);
        const float xzmyw = 2.0f * (xz - yw);
        const float yzpxw = 2.0f * (yz + xw);
        const float yzmxw = 2.0f * (yz - xw);

        const float omyypzz = (1.0f - yypzz);
        const float omxxpzz = (1.0f - xxpzz);
        const float omxxpyy = (1.0f - xxpyy);

        const float sx = scale[0];
        const float sy = scale[1];
        const float sz = scale[2];

        return matrix(
            vector4(omyypzz * sx, xypzw * sx, xzmyw * sx, 0.0f),
            vector4(xymzw * sy, omxxpzz * sy, yzpxw * sy, 0.0f),
            vector4(xzpyw * sz, yzmxw * sz, omxxpyy * sz, 0.0f),
            vector4(translation[0], translation[1], translation[2], 1.0f));
    }

    static matrix affinescalerotationtranslation(const vector3& scale, const vector3& origin, const quaternion& rotation, const vector3& translation) {
        const matrix MScaling = matrix::scale(scale);
        const vector4 VRotationOrigin(origin[0], origin[1], origin[2], 0.0f);
        const matrix MRotation = rotationquaternion(rotation);
        const vector4 VTranslation(translation[0], translation[1], translation[2], 0.0f);

        matrix M = MScaling;
        M.a[3] = M.a[3] - VRotationOrigin;
        M = M * MRotation;
        M.a[3] = M.a[3] + VRotationOrigin;
        M.a[3] = M.a[3] + VTranslation;
        return M;
    }

    static matrix orthographic( float ViewLeft, float ViewRight, float ViewBottom, float ViewTop, float NearZ, float FarZ )
    {
        float ReciprocalWidth = 1.0f / ( ViewRight - ViewLeft );
        float ReciprocalHeight = 1.0f / ( ViewTop - ViewBottom );
        float fRange = 1.0f / ( FarZ - NearZ );

        matrix M;
        M[ 0 ][ 0 ] = ReciprocalWidth + ReciprocalWidth;
        M[ 0 ][ 1 ] = 0.0f;
        M[ 0 ][ 2 ] = 0.0f;
        M[ 0 ][ 3 ] = 0.0f;

        M[ 1 ][ 0 ] = 0.0f;
        M[ 1 ][ 1 ] = ReciprocalHeight + ReciprocalHeight;
        M[ 1 ][ 2 ] = 0.0f;
        M[ 1 ][ 3 ] = 0.0f;

        M[ 2 ][ 0 ] = 0.0f;
        M[ 2 ][ 1 ] = 0.0f;
        M[ 2 ][ 2 ] = fRange;
        M[ 2 ][ 3 ] = 0.0f;

        M[ 3 ][ 0 ] = -( ViewLeft + ViewRight ) * ReciprocalWidth;
        M[ 3 ][ 1 ] = -( ViewTop + ViewBottom ) * ReciprocalHeight;
        M[ 3 ][ 2 ] = -fRange * NearZ;
        M[ 3 ][ 3 ] = 1.0f;

        return M;
    }

    static matrix orthographic(float width, float height, float nearZ, float farZ)
    {
        const float fRange = 1.0f / (farZ - nearZ);

        matrix M;
        M[0][0] = 2.0f / width;
        M[0][1] = 0.0f;
        M[0][2] = 0.0f;
        M[0][3] = 0.0f;

        M[1][0] = 0.0f;
        M[1][1] = 2.0f / height;
        M[1][2] = 0.0f;
        M[1][3] = 0.0f;

        M[2][0] = 0.0f;
        M[2][1] = 0.0f;
        M[2][2] = fRange;
        M[2][3] = 0.0f;

        M[3][0] = 0.0f;
        M[3][1] = 0.0f;
        M[3][2] = -fRange * nearZ;
        M[3][3] = 1.0f;
        return M;
    }

    static matrix perspectivefov(float fov, float aspect, float nearZ, float farZ)
    {
        const float half_fov = 0.5f * fov;
        const float SinFov = sinf(half_fov);
        const float CosFov = cosf(half_fov);

        const float Height = CosFov / SinFov;
        const float Width = Height / aspect;
        const float fRange = farZ / (farZ - nearZ);

        matrix M;
        M[0][0] = Width;
        M[0][1] = 0.0f;
        M[0][2] = 0.0f;
        M[0][3] = 0.0f;

        M[1][0] = 0.0f;
        M[1][1] = Height;
        M[1][2] = 0.0f;
        M[1][3] = 0.0f;

        M[2][0] = 0.0f;
        M[2][1] = 0.0f;
        M[2][2] = fRange;
        M[2][3] = 1.0f;

        M[3][0] = 0.0f;
        M[3][1] = 0.0f;
        M[3][2] = -fRange * nearZ;
        M[3][3] = 0.0f;
        return M;
    }

    static matrix lookat(const vector3& eye, const vector3& target, const vector3& up)
    {
        const vector3 R2 = (target - eye).normalize();
        const vector3 R0 = up.cross(R2).normalize();
        const vector3 R1 = R2.cross(R0);

        const vector3 neg_eye = -eye;

        const float D0 = R0.dot(neg_eye);
        const float D1 = R1.dot(neg_eye);
        const float D2 = R2.dot(neg_eye);

        return matrix(
            vector4(R0[0], R0[1], R0[2], D0),
            vector4(R1[0], R1[1], R1[2], D1),
            vector4(R2[0], R2[1], R2[2], D2),
            vector4(0.0f, 0.0f, 0.0f, 1.0f)).transpose();
    }
};

struct Constants {
    matrix model;
    vector4 color;
    uint32_t char_index;
};

struct Uniforms {
    mat4x4 view_proj;
};

class Device {
    HWND hWnd = NULL;

    vk::UniqueInstance instance;
    vk::PhysicalDevice gpu;
    vk::UniqueSurfaceKHR surface;
    vk::SurfaceFormatKHR surface_format;
    vk::UniqueDevice device;
    vk::Queue queue;

    vk::UniqueCommandPool cmd_pool;
    vk::UniqueDescriptorPool desc_pool;
    vk::UniqueRenderPass render_pass;
    vk::UniqueDescriptorSetLayout desc_layout;
    vk::UniquePipelineLayout pipeline_layout;
    vk::UniquePipeline pipeline;
    vk::UniqueDescriptorSet descriptor_set;

    vk::UniqueBuffer uniform_buffer;
    vk::UniqueDeviceMemory uniform_memory;
    void* uniform_ptr = nullptr;

    std::vector<vk::UniqueFence> fences;
    std::vector<vk::UniqueSemaphore> image_acquired_semaphores;
    std::vector<vk::UniqueSemaphore> draw_complete_semaphores;

    vk::UniqueSwapchainKHR swapchain;
    std::vector<vk::UniqueImageView> views;
    std::vector<vk::UniqueFramebuffer> framebuffers;
    std::vector<vk::UniqueCommandBuffer> cmds;

    unsigned width = 0;
    unsigned height = 0;

    unsigned fence_index = 0;

public:
    Device(WNDPROC proc, HINSTANCE hInstance, int nCmdShow, unsigned width, unsigned height) {
        instance = create_instance();
        gpu = pick_gpu(instance.get());
        hWnd = create_window(proc, hInstance, nCmdShow, this, width, height);
        surface = create_surface(instance.get(), hInstance, hWnd);
        surface_format = select_format(gpu, surface.get());
        auto family_index = find_queue_family(gpu, surface.get());
        device = create_device(gpu, family_index);
        queue = fetch_queue(device.get(), family_index);

        cmd_pool = create_command_pool(device.get(), family_index);
        desc_pool = create_descriptor_pool(device.get());
        desc_layout = create_descriptor_layout(device.get());
        pipeline_layout = create_pipeline_layout(device.get(), desc_layout.get(), sizeof(Constants));
        render_pass = create_render_pass(device.get(), surface_format);
        pipeline = create_pipeline(device.get(), pipeline_layout.get(), render_pass.get());

        uniform_buffer = create_uniform_buffer(device.get(), sizeof(Uniforms));
        uniform_memory = create_uniform_memory(gpu, device.get(), uniform_buffer.get());
        bind_memory(device.get(), uniform_buffer.get(), uniform_memory.get());
        uniform_ptr = map_memory(device.get(), uniform_memory.get());

        descriptor_set = create_descriptor_set(device.get(), desc_pool.get(), desc_layout.get());
        update_descriptor_set(device.get(), descriptor_set.get(), uniform_buffer.get(), sizeof(Uniforms));

        for (auto i = 0; i < 2; ++i) {
            fences.emplace_back(create_fence(device.get()));
            image_acquired_semaphores.emplace_back(create_semaphore(device.get()));
            draw_complete_semaphores.emplace_back(create_semaphore(device.get()));
        }

        resize(width, height);
    }

    ~Device() {
        wait_idle(device.get());

        destroy_window(hWnd);
    }

    void resize(unsigned w, unsigned h) {
        width = w;
        height = h;

        if (device) {
            wait_idle(device.get());

            views.clear();
            framebuffers.clear();
            cmds.clear();

            swapchain = create_swapchain(gpu, device.get(), surface.get(), surface_format, swapchain.get(), width, height, 3);
            const auto swapchain_images = get_swapchain_images(device.get(), swapchain.get());
            for (uint32_t i = 0; i < swapchain_images.size(); ++i) {
                views.emplace_back(create_image_view(device.get(), swapchain_images[i], surface_format));
                framebuffers.emplace_back(create_framebuffer(device.get(), render_pass.get(), views.back().get(), width, height));
                cmds.emplace_back(create_command_buffer(device.get(), cmd_pool.get()));
            }

            auto& uniforms = *(Uniforms*)uniform_ptr;

            // [TODO] Use simd::matrix like math code.
            const vec3 eye = { 0.0f, 0.0f, 10.0f };
            const vec3 origin = { 0.0f, 0.0f, 0.0f };
            const vec3 up = { 0.0f, 1.0f, 0.0f };
            mat4x4 view;
            mat4x4_look_at(view, eye, origin, up);

            mat4x4 proj;
            const float l = 0.0f;
            const float r = (float)width;
            const float b = 0.0f;
            const float t = (float)height;
            const float n = -100.0f;
            const float f = 100.0f;
            mat4x4_ortho(proj, l, r, b, t, n, f);

            mat4x4_mul(uniforms.view_proj, proj, view);
        }
    }

    void redraw(const Color& clear_color, const Characters& characters) {
        wait(device.get(), fences[fence_index].get());
        const auto frame_index = acquire(device.get(), swapchain.get(), image_acquired_semaphores[fence_index].get());
        const auto& cmd = cmds[frame_index].get();

        begin(cmd);
        begin_pass(cmd, render_pass.get(), framebuffers[frame_index].get(), clear_color.rgba(), width, height);

        set_viewport(cmd, (float)width, (float)height);
        set_scissor(cmd, width, height);

        bind_pipeline(cmd, pipeline.get());
        bind_descriptor_set(cmd, pipeline_layout.get(), descriptor_set.get());

        for (auto& character : characters) {
            const float scale = 14.0f; // [TODO] Move to character.
            const float char_width = scale * 0.5f;
            const float char_height = scale * 1.05f;
            const float trans_x = (1.0f + character.col) * char_width;
            const float trans_y = (1.0f + character.row) * char_height;

            Constants constants;
            // [TODO] Use simd::matrix like math code.
            constants.model[0] = vector4(scale, 0.0f, 0.0f, 0.0f);
            constants.model[1] = vector4(0.0f, scale, 0.0f, 0.0f);
            constants.model[2] = vector4(0.0f, 0.0f, scale, 0.0f);
            constants.model[3] = vector4(trans_x, trans_y, 0.0f, 1.0f);
            constants.color = character.color.rgba();
            constants.char_index = character.index;

            push(cmd, pipeline_layout.get(), sizeof(Constants), &constants);
            draw(cmd, vertex_counts[character.index]);
        }

        end_pass(cmd);
        end(cmd);

        submit(queue, image_acquired_semaphores[fence_index].get(), draw_complete_semaphores[fence_index].get(), cmd, fences[fence_index].get());
        present(swapchain.get(), queue, draw_complete_semaphores[fence_index].get(), frame_index);

        fence_index += 1;
        fence_index %= fences.size();
    }
};

