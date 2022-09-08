#pragma once

#define VULKAN_HPP_NO_EXCEPTIONS
#include <vulkan/vulkan.hpp>

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

