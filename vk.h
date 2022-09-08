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

