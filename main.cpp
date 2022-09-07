#define _HAS_EXCEPTIONS 0

#define VULKAN_HPP_NO_EXCEPTIONS
#include <vulkan/vulkan.hpp>

#include "linmath.h"

#ifndef NDEBUG
#define VERIFY(x) assert(x)
#else
#define VERIFY(x) ((void)(x))
#endif

#define WINDOW_NAME "vin"
#define APP_SHORT_NAME "vin"
#define APP_NAME_STR_LEN 80

#define FRAME_LAG 2

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

#define ERR_EXIT(err_msg, err_class) \
    do { \
        MessageBox(nullptr, err_msg, err_class, MB_OK); \
        exit(1); \
    } while (0)

static int validation_error = 0;

struct Uniforms{
    float mvp[4][4];
    float position[12 * 3][4];
};

static const float g_vertex_buffer_data[] = {
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

struct Matrices {
    mat4x4 projection_matrix;
    mat4x4 view_matrix;
    mat4x4 model_matrix;
};

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

struct Window {
    HINSTANCE hinstance = nullptr;
    HWND hwnd = nullptr;
    char name[APP_NAME_STR_LEN];
    POINT minsize = { 0, 0 };
    int32_t width = 800;
    int32_t height = 600;

    Window(HINSTANCE hInstance)
        : hinstance(hInstance) {
        memset(name, '\0', APP_NAME_STR_LEN);
        strncpy(name, WINDOW_NAME, APP_NAME_STR_LEN);

        WNDCLASSEX win_class;
        win_class.cbSize = sizeof(WNDCLASSEX);
        win_class.style = CS_HREDRAW | CS_VREDRAW;
        win_class.lpfnWndProc = WndProc;
        win_class.cbClsExtra = 0;
        win_class.cbWndExtra = 0;
        win_class.hInstance = hinstance;
        win_class.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
        win_class.hCursor = LoadCursor(nullptr, IDC_ARROW);
        win_class.hbrBackground = CreateSolidBrush(0);
        win_class.lpszMenuName = nullptr;
        win_class.lpszClassName = name;
        win_class.hIconSm = LoadIcon(nullptr, IDI_WINLOGO);

        if (!RegisterClassEx(&win_class)) {
            ERR_EXIT("Unexpected error trying to start the application!\n", "RegisterClass Failure");
        }
    }

    void create(int nCmdShow) {
        RECT wr = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
        AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);

        hwnd = CreateWindowEx(0, name, name, WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_SYSMENU,
            CW_USEDEFAULT, CW_USEDEFAULT, wr.right - wr.left, wr.bottom - wr.top,
            nullptr, nullptr, hinstance, nullptr);
        if (!hwnd) {
            ERR_EXIT("Cannot create a window in which to draw!\n", "CreateWindow Failure");
        }

        // Window client area size must be at least 1 pixel high, to prevent crash.
        minsize.x = GetSystemMetrics(SM_CXMINTRACK);
        minsize.y = GetSystemMetrics(SM_CYMINTRACK) + 1;

        ShowWindow(hwnd, nCmdShow);
    }
};

struct SwapchainImageResources {
    vk::Image image;
    vk::UniqueCommandBuffer cmd;
    vk::UniqueImageView view;
    vk::UniqueBuffer uniform_buffer;
    vk::UniqueDeviceMemory uniform_memory;
    void* uniform_memory_ptr = nullptr;
    vk::UniqueFramebuffer framebuffer;
    vk::UniqueDescriptorSet descriptor_set;
};

struct Chain {
    uint32_t swapchain_image_count = 0;
    vk::UniqueSwapchainKHR swapchain;
    std::unique_ptr<SwapchainImageResources[]> swapchain_image_resources;
    vk::PresentModeKHR presentMode = vk::PresentModeKHR::eFifo;
    vk::UniqueFence fences[FRAME_LAG];
    vk::UniqueSemaphore image_acquired_semaphores[FRAME_LAG];
    vk::UniqueSemaphore draw_complete_semaphores[FRAME_LAG];
    uint32_t frame_index = 0;

    Chain(vk::Device& device) {
        for (uint32_t i = 0; i < FRAME_LAG; i++) {
            auto const fence_info = vk::FenceCreateInfo()
                .setFlags(vk::FenceCreateFlagBits::eSignaled);

            auto fence_handle = device.createFenceUnique(fence_info);
            VERIFY(fence_handle.result == vk::Result::eSuccess);
            fences[i] = std::move(fence_handle.value);

            auto const semaphore_info = vk::SemaphoreCreateInfo();

            auto semaphore_handle = device.createSemaphoreUnique(semaphore_info);
            VERIFY(semaphore_handle.result == vk::Result::eSuccess);
            image_acquired_semaphores[i] = std::move(semaphore_handle.value);

            semaphore_handle = device.createSemaphoreUnique(semaphore_info);
            VERIFY(semaphore_handle.result == vk::Result::eSuccess);
            draw_complete_semaphores[i] = std::move(semaphore_handle.value);
        }

        frame_index = 0;
    }
};

class App {
    std::unique_ptr<Chain> chain; // [TODO] Rename to SwapChain.

    Matrices matrices;

    vk::UniqueSurfaceKHR surface; // [TODO] Move to Surface.

    vk::UniqueInstance instance; // [TODO] Move to Device.
    vk::PhysicalDevice gpu;
    vk::UniqueDevice device;
    vk::Queue queue;

    vk::Format format; // [TODO] Move to Surface.
    vk::ColorSpaceKHR color_space;

    vk::UniqueCommandPool cmd_pool;

    vk::UniquePipelineLayout pipeline_layout; // [TODO] Move to Pipeline.
    vk::UniqueDescriptorSetLayout desc_layout;
    vk::UniqueRenderPass render_pass;
    vk::UniquePipeline pipeline;

    vk::UniqueDescriptorPool desc_pool;

    uint32_t current_frame = 0;
    uint32_t frame_count = UINT32_MAX;
    uint32_t current_buffer = 0;

    void create_instance();
    void pick_gpu();
    void create_surface();
    void create_device();

    void prepare_buffers();
    void prepare_uniforms();
    void prepare_descriptor_layout();
    void prepare_descriptor_pool();
    void prepare_descriptor_set();
    void prepare_framebuffers();
    vk::UniqueShaderModule prepare_shader_module(const uint32_t*, size_t);
    vk::UniqueShaderModule prepare_vs();
    vk::UniqueShaderModule prepare_fs();
    void prepare_pipeline();
    void prepare_render_pass();

    void draw_build_cmd(vk::CommandBuffer);

    void update_data_buffer();

    bool memory_type_from_properties(uint32_t, vk::MemoryPropertyFlags, uint32_t*);

    void draw();

public:
    Window window;

    App(HINSTANCE hInstance);
    ~App();

    void init_vk_swapchain(); // [TODO] Remove and make part of SwapChain constructor.
    void prepare();
    void resize();
    void run();
};


App::App(HINSTANCE hInstance)
    : window(hInstance) {
    memset(matrices.projection_matrix, 0, sizeof(matrices.projection_matrix));
    memset(matrices.view_matrix, 0, sizeof(matrices.view_matrix));
    memset(matrices.model_matrix, 0, sizeof(matrices.model_matrix));

    create_instance();

    pick_gpu();

    vec3 eye = { 0.0f, 3.0f, 5.0f };
    vec3 origin = { 0, 0, 0 };
    vec3 up = { 0.0f, 1.0f, 0.0 };

    mat4x4_perspective(matrices.projection_matrix, (float)degreesToRadians(45.0f), 1.0f, 0.1f, 100.0f);
    mat4x4_look_at(matrices.view_matrix, eye, origin, up);
    mat4x4_identity(matrices.model_matrix);

    matrices.projection_matrix[1][1] *= -1; // Flip projection matrix from GL to Vulkan orientation.
}

App::~App() {
    auto result = device->waitIdle();
    VERIFY(result == vk::Result::eSuccess);

    // Wait for fences from present operations
    for (uint32_t i = 0; i < FRAME_LAG; i++) {
        result = device->waitForFences(1, &chain->fences[i].get(), VK_TRUE, UINT64_MAX);
        VERIFY(result == vk::Result::eSuccess);
    }

    for (uint32_t i = 0; i < chain->swapchain_image_count; i++) {
        device->unmapMemory(chain->swapchain_image_resources[i].uniform_memory.get());
    }

    chain.reset();

    desc_pool.reset();

    pipeline.reset();
    render_pass.reset();
    pipeline_layout.reset();
    desc_layout.reset();

    cmd_pool.reset();

    device.reset();
    surface.reset();
    instance.reset();
}

void App::draw() {
    // Ensure no more than FRAME_LAG renderings are outstanding
    auto result = device->waitForFences(1, &chain->fences[chain->frame_index].get(), VK_TRUE, UINT64_MAX);
    VERIFY(result == vk::Result::eSuccess);

    device->resetFences({ chain->fences[chain->frame_index].get() });

    do {
        result = device->acquireNextImageKHR(chain->swapchain.get(), UINT64_MAX, chain->image_acquired_semaphores[chain->frame_index].get(), vk::Fence(), &current_buffer);
        if (result == vk::Result::eErrorOutOfDateKHR) {
            // swapchain is out of date (e.g. the window was resized) and must be recreated:
            resize();
        }
        else if (result == vk::Result::eSuboptimalKHR) {
            // swapchain is not as optimal as it could be, but the platform's
            // presentation engine will still present the image correctly.
            break;
        }
        else if (result == vk::Result::eErrorSurfaceLostKHR) {
            instance->destroySurfaceKHR(surface.get(), nullptr);
            create_surface();
            resize();
        }
        else {
            VERIFY(result == vk::Result::eSuccess);
        }
    } while (result != vk::Result::eSuccess);

    update_data_buffer();

    // Wait for the image acquired semaphore to be signaled to ensure
    // that the image won't be rendered to until the presentation
    // engine has fully released ownership to the application, and it is
    // okay to render to the image.
    vk::PipelineStageFlags const pipe_stage_flags = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    auto const submit_info = vk::SubmitInfo()
        .setPWaitDstStageMask(&pipe_stage_flags)
        .setWaitSemaphoreCount(1)
        .setPWaitSemaphores(&chain->image_acquired_semaphores[chain->frame_index].get())
        .setCommandBufferCount(1)
        .setPCommandBuffers(&chain->swapchain_image_resources[current_buffer].cmd.get())
        .setSignalSemaphoreCount(1)
        .setPSignalSemaphores(&chain->draw_complete_semaphores[chain->frame_index].get());

    result = queue.submit(1, &submit_info, chain->fences[chain->frame_index].get());
    VERIFY(result == vk::Result::eSuccess);

    // If we are using separate queues we have to wait for image ownership,
    // otherwise wait for draw complete
    auto const present_info = vk::PresentInfoKHR()
        .setWaitSemaphoreCount(1)
        .setPWaitSemaphores(&chain->draw_complete_semaphores[chain->frame_index].get())
        .setSwapchainCount(1)
        .setPSwapchains(&chain->swapchain.get())
        .setPImageIndices(&current_buffer);

    result = queue.presentKHR(&present_info);
    chain->frame_index += 1;
    chain->frame_index %= FRAME_LAG;
    if (result == vk::Result::eErrorOutOfDateKHR) {
        // swapchain is out of date (e.g. the window was resized) and must be recreated:
        resize();
    }
    else if (result == vk::Result::eSuboptimalKHR) {
        // SUBOPTIMAL could be due to resize
        vk::SurfaceCapabilitiesKHR surfCapabilities;
        result = gpu.getSurfaceCapabilitiesKHR(surface.get(), &surfCapabilities);
        VERIFY(result == vk::Result::eSuccess);
        if (surfCapabilities.currentExtent.width != static_cast<uint32_t>(window.width) || surfCapabilities.currentExtent.height != static_cast<uint32_t>(window.height)) {
            resize();
        }
    }
    else if (result == vk::Result::eErrorSurfaceLostKHR) {
        instance->destroySurfaceKHR(surface.get(), nullptr);
        create_surface();
        resize();
    }
    else {
        VERIFY(result == vk::Result::eSuccess);
    }
}

void App::draw_build_cmd(vk::CommandBuffer commandBuffer) {
    auto const command_buffer_info = vk::CommandBufferBeginInfo()
        .setFlags(vk::CommandBufferUsageFlagBits::eSimultaneousUse);

    vk::ClearValue const clearValues[1] = { vk::ClearColorValue(std::array<float, 4>({{0.2f, 0.2f, 0.2f, 0.2f}})) };

    auto const pass_info = vk::RenderPassBeginInfo()
        .setRenderPass(render_pass.get())
        .setFramebuffer(chain->swapchain_image_resources[current_buffer].framebuffer.get())
        .setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D((uint32_t)window.width, (uint32_t)window.height)))
        .setClearValueCount(2)
        .setPClearValues(clearValues);

    auto result = commandBuffer.begin(&command_buffer_info);
    VERIFY(result == vk::Result::eSuccess);

    commandBuffer.beginRenderPass(&pass_info, vk::SubpassContents::eInline);
    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.get());
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline_layout.get(), 0, 1, &chain->swapchain_image_resources[current_buffer].descriptor_set.get(), 0, nullptr);
    float viewport_dimension;
    float viewport_x = 0.0f;
    float viewport_y = 0.0f;
    if (window.width < window.height) {
        viewport_dimension = (float)window.width;
        viewport_y = (window.height - window.width) / 2.0f;
    }
    else {
        viewport_dimension = (float)window.height;
        viewport_x = (window.width - window.height) / 2.0f;
    }
    auto const viewport = vk::Viewport()
        .setX(viewport_x)
        .setY(viewport_y)
        .setWidth((float)viewport_dimension)
        .setHeight((float)viewport_dimension)
        .setMinDepth((float)0.0f)
        .setMaxDepth((float)1.0f);
    commandBuffer.setViewport(0, 1, &viewport);

    vk::Rect2D const scissor(vk::Offset2D(0, 0), vk::Extent2D(window.width, window.height));
    commandBuffer.setScissor(0, 1, &scissor);
    commandBuffer.draw(12 * 3, 1, 0, 0);
    // Note that ending the renderpass changes the image's layout from COLOR_ATTACHMENT_OPTIMAL to PRESENT_SRC_KHR
    commandBuffer.endRenderPass();

    result = commandBuffer.end();
    VERIFY(result == vk::Result::eSuccess);
}

void App::create_instance() {
    // Look for validation layers
    uint32_t enabled_layer_count = 0;
    char const* enabled_layers[64];

#if !defined(NDEBUG)
    {
        uint32_t instance_layer_count = 0;
        auto result = vk::enumerateInstanceLayerProperties(&instance_layer_count, static_cast<vk::LayerProperties*>(nullptr));
        VERIFY(result == vk::Result::eSuccess);

        if (instance_layer_count > 0) {
            enabled_layer_count = 1;
            enabled_layers[0] = "VK_LAYER_KHRONOS_validation";
        }
    }
#endif

    // Look for instance extensions
    uint32_t instance_extension_count = 0;
    auto result = vk::enumerateInstanceExtensionProperties(nullptr, &instance_extension_count, static_cast<vk::ExtensionProperties*>(nullptr));
    VERIFY(result == vk::Result::eSuccess);

    uint32_t enabled_extension_count = 0;
    char const* extension_names[64];
    memset(extension_names, 0, sizeof(extension_names));

    vk::Bool32 surfaceExtFound = VK_FALSE;
    vk::Bool32 platformSurfaceExtFound = VK_FALSE;

    if (instance_extension_count > 0) {
        std::unique_ptr<vk::ExtensionProperties[]> instance_extensions(new vk::ExtensionProperties[instance_extension_count]);
        result = vk::enumerateInstanceExtensionProperties(nullptr, &instance_extension_count, instance_extensions.get());
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
        .setPApplicationName(APP_SHORT_NAME)
        .setApplicationVersion(0)
        .setPEngineName(APP_SHORT_NAME)
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
    instance = std::move(instance_handle.value);
}

void App::pick_gpu() {
    // Make initial call to query gpu_count, then second call for gpu info
    uint32_t gpu_count = 0;
    auto result = instance->enumeratePhysicalDevices(&gpu_count, static_cast<vk::PhysicalDevice*>(nullptr));
    VERIFY(result == vk::Result::eSuccess);

    if (gpu_count <= 0) {
        ERR_EXIT(
            "vkEnumeratePhysicalDevices reported zero accessible devices.\n\n"
            "Do you have a compatible Vulkan installable client driver (ICD) installed?\n"
            "Please look at the Getting Started guide for additional information.\n",
            "vkEnumeratePhysicalDevices Failure");
    }

    std::unique_ptr<vk::PhysicalDevice[]> physical_devices(new vk::PhysicalDevice[gpu_count]);
    result = instance->enumeratePhysicalDevices(&gpu_count, physical_devices.get());
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
    gpu = physical_devices[gpu_number];
    physical_devices.reset();
}

void App::create_surface() {
    auto const surf_info = vk::Win32SurfaceCreateInfoKHR().setHinstance(window.hinstance).setHwnd(window.hwnd);

    auto surface_handle = instance->createWin32SurfaceKHRUnique(surf_info);
    VERIFY(surface_handle.result == vk::Result::eSuccess);
    surface = std::move(surface_handle.value);
}

void App::create_device() {
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
        auto result = gpu.getSurfaceSupportKHR(i, surface.get(), &supportsPresent[i]);
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

    float const priorities[1] = { 0.0 };

    vk::DeviceQueueCreateInfo queues[2];
    queues[0].setQueueFamilyIndex(graphics_queue_family_index);
    queues[0].setQueueCount(1);
    queues[0].setPQueuePriorities(priorities);

    // Look for device extensions
    uint32_t enabled_extension_count = 0;
    char const* extension_names[64];

    uint32_t device_extension_count = 0;
    vk::Bool32 swapchainExtFound = VK_FALSE;
    enabled_extension_count = 0;
    memset(extension_names, 0, sizeof(extension_names));

    auto result = gpu.enumerateDeviceExtensionProperties(nullptr, &device_extension_count, static_cast<vk::ExtensionProperties*>(nullptr));
    VERIFY(result == vk::Result::eSuccess);

    if (device_extension_count > 0) {
        std::unique_ptr<vk::ExtensionProperties[]> device_extensions(new vk::ExtensionProperties[device_extension_count]);
        result = gpu.enumerateDeviceExtensionProperties(nullptr, &device_extension_count, device_extensions.get());
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
    device = std::move(device_handle.value);

    device->getQueue(graphics_queue_family_index, 0, &queue);

    auto const cmd_pool_info = vk::CommandPoolCreateInfo()
        .setQueueFamilyIndex(graphics_queue_family_index);

    auto cmd_pool_handle = device->createCommandPoolUnique(cmd_pool_info);
    VERIFY(cmd_pool_handle.result == vk::Result::eSuccess);
    cmd_pool = std::move(cmd_pool_handle.value);
}

void App::init_vk_swapchain() {
    create_surface();
    create_device();

    // Get the list of VkFormat's that are supported:
    uint32_t format_count;
    auto result = gpu.getSurfaceFormatsKHR(surface.get(), &format_count, static_cast<vk::SurfaceFormatKHR*>(nullptr));
    VERIFY(result == vk::Result::eSuccess);

    std::unique_ptr<vk::SurfaceFormatKHR[]> surface_formats(new vk::SurfaceFormatKHR[format_count]);
    result = gpu.getSurfaceFormatsKHR(surface.get(), &format_count, surface_formats.get());
    VERIFY(result == vk::Result::eSuccess);

    // If the format list includes just one entry of VK_FORMAT_UNDEFINED,
    // the surface has no preferred format.  Otherwise, at least one
    // supported format will be returned.
    if (format_count == 1 && surface_formats[0].format == vk::Format::eUndefined) {
        format = vk::Format::eB8G8R8A8Unorm;
    }
    else {
        VERIFY(format_count >= 1);
        format = surface_formats[0].format;
    }
    color_space = surface_formats[0].colorSpace;
}

void App::prepare() {
    auto const cmd_info = vk::CommandBufferAllocateInfo()
        .setCommandPool(cmd_pool.get())
        .setLevel(vk::CommandBufferLevel::ePrimary)
        .setCommandBufferCount(1);

    chain = std::make_unique<Chain>(device.get());

    prepare_buffers();
    prepare_uniforms();

    prepare_descriptor_layout();
    prepare_render_pass();
    prepare_pipeline();

    for (uint32_t i = 0; i < chain->swapchain_image_count; ++i) {
        auto cmd_handles = device->allocateCommandBuffersUnique(cmd_info);
        VERIFY(cmd_handles.result == vk::Result::eSuccess);
        chain->swapchain_image_resources[i].cmd = std::move(cmd_handles.value[0]);
    }

    prepare_descriptor_pool();
    prepare_descriptor_set();

    prepare_framebuffers();

    for (uint32_t i = 0; i < chain->swapchain_image_count; ++i) {
        current_buffer = i;
        draw_build_cmd(chain->swapchain_image_resources[i].cmd.get());
    }

    current_buffer = 0;
}

void App::prepare_buffers() {
    vk::SurfaceCapabilitiesKHR surfCapabilities;
    auto result = gpu.getSurfaceCapabilitiesKHR(surface.get(), &surfCapabilities);
    VERIFY(result == vk::Result::eSuccess);

    uint32_t presentModeCount;
    result = gpu.getSurfacePresentModesKHR(surface.get(), &presentModeCount, static_cast<vk::PresentModeKHR*>(nullptr));
    VERIFY(result == vk::Result::eSuccess);

    std::unique_ptr<vk::PresentModeKHR[]> presentModes(new vk::PresentModeKHR[presentModeCount]);
    result = gpu.getSurfacePresentModesKHR(surface.get(), &presentModeCount, presentModes.get());
    VERIFY(result == vk::Result::eSuccess);

    vk::Extent2D swapchainExtent;
    // width and height are either both -1, or both not -1.
    if (surfCapabilities.currentExtent.width == (uint32_t)-1) {
        // If the surface size is undefined, the size is set to
        // the size of the images requested.
        swapchainExtent.width = window.width;
        swapchainExtent.height = window.height;
    }
    else {
        // If the surface size is defined, the swap chain size must match
        swapchainExtent = surfCapabilities.currentExtent;
        window.width = surfCapabilities.currentExtent.width;
        window.height = surfCapabilities.currentExtent.height;
    }

    // The FIFO present mode is guaranteed by the spec to be supported
    // and to have no tearing.  It's a great default present mode to use.
    vk::PresentModeKHR swapchainPresentMode = vk::PresentModeKHR::eFifo;

    //  There are times when you may wish to use another present mode.  The
    //  following code shows how to select them, and the comments provide some
    //  reasons you may wish to use them.
    //
    // It should be noted that Vulkan 1.0 doesn't provide a method for
    // synchronizing rendering with the presentation engine's display.  There
    // is a method provided for throttling rendering with the display, but
    // there are some presentation engines for which this method will not work.
    // If an application doesn't throttle its rendering, and if it renders much
    // faster than the refresh rate of the display, this can waste power on
    // mobile devices.  That is because power is being spent rendering images
    // that may never be seen.

    // VK_PRESENT_MODE_IMMEDIATE_KHR is for applications that don't care
    // about
    // tearing, or have some way of synchronizing their rendering with the
    // display.
    // VK_PRESENT_MODE_MAILBOX_KHR may be useful for applications that
    // generally render a new presentable image every refresh cycle, but are
    // occasionally early.  In this case, the application wants the new
    // image
    // to be displayed instead of the previously-queued-for-presentation
    // image
    // that has not yet been displayed.
    // VK_PRESENT_MODE_FIFO_RELAXED_KHR is for applications that generally
    // render a new presentable image every refresh cycle, but are
    // occasionally
    // late.  In this case (perhaps because of stuttering/latency concerns),
    // the application wants the late image to be immediately displayed,
    // even
    // though that may mean some tearing.

    if (chain->presentMode != swapchainPresentMode) {
        for (size_t i = 0; i < presentModeCount; ++i) {
            if (presentModes[i] == chain->presentMode) {
                swapchainPresentMode = chain->presentMode;
                break;
            }
        }
    }

    if (swapchainPresentMode != chain->presentMode) {
        ERR_EXIT("Present mode specified is not supported\n", "Present mode unsupported");
    }

    // Determine the number of VkImages to use in the swap chain.
    // Application desires to acquire 3 images at a time for triple
    // buffering
    uint32_t desiredNumOfSwapchainImages = 3;
    if (desiredNumOfSwapchainImages < surfCapabilities.minImageCount) {
        desiredNumOfSwapchainImages = surfCapabilities.minImageCount;
    }

    // If maxImageCount is 0, we can ask for as many images as we want, otherwise we're limited to maxImageCount
    if ((surfCapabilities.maxImageCount > 0) && (desiredNumOfSwapchainImages > surfCapabilities.maxImageCount)) {
        // Application must settle for fewer images than desired:
        desiredNumOfSwapchainImages = surfCapabilities.maxImageCount;
    }

    vk::SurfaceTransformFlagBitsKHR preTransform;
    if (surfCapabilities.supportedTransforms & vk::SurfaceTransformFlagBitsKHR::eIdentity) {
        preTransform = vk::SurfaceTransformFlagBitsKHR::eIdentity;
    }
    else {
        preTransform = surfCapabilities.currentTransform;
    }

    // Find a supported composite alpha mode - one of these is guaranteed to be set
    vk::CompositeAlphaFlagBitsKHR compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    vk::CompositeAlphaFlagBitsKHR compositeAlphaFlags[4] = {
        vk::CompositeAlphaFlagBitsKHR::eOpaque,
        vk::CompositeAlphaFlagBitsKHR::ePreMultiplied,
        vk::CompositeAlphaFlagBitsKHR::ePostMultiplied,
        vk::CompositeAlphaFlagBitsKHR::eInherit,
    };
    for (uint32_t i = 0; i < ARRAY_SIZE(compositeAlphaFlags); i++) {
        if (surfCapabilities.supportedCompositeAlpha & compositeAlphaFlags[i]) {
            compositeAlpha = compositeAlphaFlags[i];
            break;
        }
    }

    auto const swapchain_info = vk::SwapchainCreateInfoKHR()
        .setSurface(surface.get())
        .setMinImageCount(desiredNumOfSwapchainImages)
        .setImageFormat(format)
        .setImageColorSpace(color_space)
        .setImageExtent({ swapchainExtent.width, swapchainExtent.height })
        .setImageArrayLayers(1)
        .setImageUsage(vk::ImageUsageFlagBits::eColorAttachment)
        .setImageSharingMode(vk::SharingMode::eExclusive)
        .setQueueFamilyIndexCount(0)
        .setPQueueFamilyIndices(nullptr)
        .setPreTransform(preTransform)
        .setCompositeAlpha(compositeAlpha)
        .setPresentMode(swapchainPresentMode)
        .setClipped(true)
        .setOldSwapchain(chain->swapchain.get());

    auto swapchain_handle = device->createSwapchainKHRUnique(swapchain_info);
    VERIFY(swapchain_handle.result == vk::Result::eSuccess);
    chain->swapchain = std::move(swapchain_handle.value);

    result = device->getSwapchainImagesKHR(chain->swapchain.get(), &chain->swapchain_image_count, static_cast<vk::Image*>(nullptr));
    VERIFY(result == vk::Result::eSuccess);

    std::unique_ptr<vk::Image[]> swapchainImages(new vk::Image[chain->swapchain_image_count]);
    result = device->getSwapchainImagesKHR(chain->swapchain.get(), &chain->swapchain_image_count, swapchainImages.get());
    VERIFY(result == vk::Result::eSuccess);

    chain->swapchain_image_resources.reset(new SwapchainImageResources[chain->swapchain_image_count]);

    for (uint32_t i = 0; i < chain->swapchain_image_count; ++i) {
        chain->swapchain_image_resources[i].image = swapchainImages[i];

        auto view_info = vk::ImageViewCreateInfo()
            .setViewType(vk::ImageViewType::e2D)
            .setFormat(format)
            .setImage(chain->swapchain_image_resources[i].image)
            .setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));

        auto view_handle = device->createImageViewUnique(view_info);
        VERIFY(view_handle.result == vk::Result::eSuccess);
        chain->swapchain_image_resources[i].view = std::move(view_handle.value);
    }
}

void App::prepare_uniforms() {
    mat4x4 VP;
    mat4x4_mul(VP, matrices.projection_matrix, matrices.view_matrix);

    mat4x4 MVP;
    mat4x4_mul(MVP, VP, matrices.model_matrix);

    Uniforms data;
    memcpy(data.mvp, MVP, sizeof(MVP));

    for (int32_t i = 0; i < 12 * 3; i++) {
        data.position[i][0] = g_vertex_buffer_data[i * 3];
        data.position[i][1] = g_vertex_buffer_data[i * 3 + 1];
        data.position[i][2] = g_vertex_buffer_data[i * 3 + 2];
        data.position[i][3] = 1.0f;
    }

    auto const buf_info = vk::BufferCreateInfo()
        .setSize(sizeof(data))
        .setUsage(vk::BufferUsageFlagBits::eUniformBuffer);

    for (unsigned int i = 0; i < chain->swapchain_image_count; i++) {
        auto buffer_handle = device->createBufferUnique(buf_info);
        VERIFY(buffer_handle.result == vk::Result::eSuccess);
        chain->swapchain_image_resources[i].uniform_buffer = std::move(buffer_handle.value);

        vk::MemoryRequirements mem_reqs;
        device->getBufferMemoryRequirements(chain->swapchain_image_resources[i].uniform_buffer.get(), &mem_reqs);

        auto mem_info = vk::MemoryAllocateInfo()
            .setAllocationSize(mem_reqs.size)
            .setMemoryTypeIndex(0);

        bool const pass = memory_type_from_properties(mem_reqs.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, &mem_info.memoryTypeIndex);
        VERIFY(pass);

        auto memory_handle = device->allocateMemoryUnique(mem_info);
        VERIFY(memory_handle.result == vk::Result::eSuccess);
        chain->swapchain_image_resources[i].uniform_memory = std::move(memory_handle.value);

        auto result = device->mapMemory(chain->swapchain_image_resources[i].uniform_memory.get(), 0, VK_WHOLE_SIZE, vk::MemoryMapFlags(), &chain->swapchain_image_resources[i].uniform_memory_ptr);
        VERIFY(result == vk::Result::eSuccess);

        memcpy(chain->swapchain_image_resources[i].uniform_memory_ptr, &data, sizeof data);

        result = device->bindBufferMemory(chain->swapchain_image_resources[i].uniform_buffer.get(), chain->swapchain_image_resources[i].uniform_memory.get(), 0);
        VERIFY(result == vk::Result::eSuccess);
    }
}

void App::prepare_descriptor_layout() {
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

    auto desc_layout_handle = device->createDescriptorSetLayoutUnique(desc_layout_info);
    VERIFY(desc_layout_handle.result == vk::Result::eSuccess);
    desc_layout = std::move(desc_layout_handle.value);

    auto const pipeline_layout_info = vk::PipelineLayoutCreateInfo()
        .setSetLayoutCount(1)
        .setPSetLayouts(&desc_layout.get());

    auto pipeline_layout_handle = device->createPipelineLayoutUnique(pipeline_layout_info);
    VERIFY(pipeline_layout_handle.result == vk::Result::eSuccess);
    pipeline_layout = std::move(pipeline_layout_handle.value);
}

void App::prepare_descriptor_pool() {
    vk::DescriptorPoolSize const pool_sizes[1] = { 
        vk::DescriptorPoolSize()
            .setType(vk::DescriptorType::eUniformBuffer)
            .setDescriptorCount(chain->swapchain_image_count)
    };

    auto const desc_pool_info = vk::DescriptorPoolCreateInfo()
        .setMaxSets(chain->swapchain_image_count)
        .setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
        .setPoolSizeCount(1)
        .setPPoolSizes(pool_sizes);

    auto desc_pool_handle = device->createDescriptorPoolUnique(desc_pool_info);
    VERIFY(desc_pool_handle.result == vk::Result::eSuccess);
    desc_pool = std::move(desc_pool_handle.value);
}

void App::prepare_descriptor_set() {
    auto const alloc_info = vk::DescriptorSetAllocateInfo()
        .setDescriptorPool(desc_pool.get())
        .setDescriptorSetCount(1)
        .setPSetLayouts(&desc_layout.get());

    auto buffer_info = vk::DescriptorBufferInfo()
        .setOffset(0)
        .setRange(sizeof(struct Uniforms));

    vk::WriteDescriptorSet writes[1];

    writes[0].setDescriptorCount(1);
    writes[0].setDescriptorType(vk::DescriptorType::eUniformBuffer);
    writes[0].setPBufferInfo(&buffer_info);

    for (unsigned int i = 0; i < chain->swapchain_image_count; i++) {
        auto descriptor_set_handles = device->allocateDescriptorSetsUnique(alloc_info);
        VERIFY(descriptor_set_handles.result == vk::Result::eSuccess);
        chain->swapchain_image_resources[i].descriptor_set = std::move(descriptor_set_handles.value[0]);

        buffer_info.setBuffer(chain->swapchain_image_resources[i].uniform_buffer.get());
        writes[0].setDstSet(chain->swapchain_image_resources[i].descriptor_set.get());
        device->updateDescriptorSets(1, writes, 0, nullptr);
    }
}

void App::prepare_framebuffers() {
    vk::ImageView attachments[1];

    auto const fb_info = vk::FramebufferCreateInfo()
        .setRenderPass(render_pass.get())
        .setAttachmentCount(1)
        .setPAttachments(attachments)
        .setWidth((uint32_t)window.width)
        .setHeight((uint32_t)window.height)
        .setLayers(1);

    for (uint32_t i = 0; i < chain->swapchain_image_count; i++) {
        attachments[0] = chain->swapchain_image_resources[i].view.get();
        auto framebuffer_handle = device->createFramebufferUnique(fb_info);
        VERIFY(framebuffer_handle.result == vk::Result::eSuccess);
        chain->swapchain_image_resources[i].framebuffer = std::move(framebuffer_handle.value);
    }
}

vk::UniqueShaderModule App::prepare_fs() {
    const uint32_t fragShaderCode[] =
#include "shader.frag.inc"
        ;
    return prepare_shader_module(fragShaderCode, sizeof(fragShaderCode));
}

void App::prepare_pipeline() {
    auto vert_shader_module = prepare_vs();
    auto frag_shader_module = prepare_fs();

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
        .setLayout(pipeline_layout.get())
        .setRenderPass(render_pass.get());

    auto pipeline_handles = device->createGraphicsPipelinesUnique(nullptr, pipeline_info);
    VERIFY(pipeline_handles.result == vk::Result::eSuccess);
    pipeline = std::move(pipeline_handles.value[0]);
}

void App::prepare_render_pass() {
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
          .setFormat(format)
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

    auto render_pass_handle = device->createRenderPassUnique(rp_info);
    VERIFY(render_pass_handle.result == vk::Result::eSuccess);
    render_pass = std::move(render_pass_handle.value);
}

vk::UniqueShaderModule App::prepare_shader_module(const uint32_t* code, size_t size) {
    const auto module_info = vk::ShaderModuleCreateInfo()
        .setCodeSize(size)
        .setPCode(code);

    auto module_handle = device->createShaderModuleUnique(module_info);
    VERIFY(module_handle.result == vk::Result::eSuccess);
    return std::move(module_handle.value);
}

vk::UniqueShaderModule App::prepare_vs() {
    const uint32_t vertShaderCode[] =
#include "shader.vert.inc"
        ;
    return prepare_shader_module(vertShaderCode, sizeof(vertShaderCode));
}

void App::resize() {
    uint32_t i;

    // Don't react to resize until after first initialization.
    if (!device) {
        return;
    }

    // In order to properly resize the window, we must re-create the swapchain
    // AND redo the command buffers, etc.
    //
    // First, perform part of the cleanup() function:
    auto result = device->waitIdle();
    VERIFY(result == vk::Result::eSuccess);

    for (i = 0; i < chain->swapchain_image_count; i++) {
        chain->swapchain_image_resources[i].framebuffer.reset();
        chain->swapchain_image_resources[i].descriptor_set.reset();
    }

    desc_pool.reset();

    pipeline.reset();
    render_pass.reset();
    pipeline_layout.reset();
    desc_layout.reset();

    chain.reset();

    // Second, re-perform the prepare() function, which will re-create the
    // swapchain.
    prepare();
}

void App::update_data_buffer() {
    mat4x4 VP;
    mat4x4_mul(VP, matrices.projection_matrix, matrices.view_matrix);

    // Rotate around the Y axis
    mat4x4 Model;
    mat4x4_dup(Model, matrices.model_matrix);
    mat4x4_rotate_Y(matrices.model_matrix, Model, (float)degreesToRadians(1.5f));
    mat4x4_orthonormalize(matrices.model_matrix, matrices.model_matrix);

    mat4x4 MVP;
    mat4x4_mul(MVP, VP, matrices.model_matrix);

    memcpy(chain->swapchain_image_resources[current_buffer].uniform_memory_ptr, (const void*)&MVP[0][0], sizeof(MVP));
}

bool App::memory_type_from_properties(uint32_t typeBits, vk::MemoryPropertyFlags requirements_mask, uint32_t* typeIndex) {
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

void App::run() {
    draw();
    current_frame++;

    if (frame_count != UINT32_MAX && current_frame == frame_count) {
        PostQuitMessage(validation_error);
    }
}

std::unique_ptr<App> app;

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CLOSE:
        PostQuitMessage(validation_error);
        break;
    case WM_PAINT:
        app->run();
        break;
    case WM_GETMINMAXINFO:  // set window's minimum size
        ((MINMAXINFO*)lParam)->ptMinTrackSize = app->window.minsize;
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_SIZE:
        // Resize the application to the new window size, except when
        // it was minimized. Vulkan doesn't support images or swapchains
        // with width=0 and height=0.
        if (wParam != SIZE_MINIMIZED) {
            app->window.width = lParam & 0xffff;
            app->window.height = (lParam & 0xffff0000) >> 16;
            app->resize();
        }
        break;
    case WM_KEYDOWN:
        switch (wParam) {
        case VK_ESCAPE:
            PostQuitMessage(validation_error);
            break;
        }
        return 0;
    default:
        break;
    }

    return (DefWindowProc(hWnd, uMsg, wParam, lParam));
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) {
    MSG msg;
    msg.wParam = 0;

    app = std::make_unique<App>(hInstance);

    app->window.create(nCmdShow); // [TODO] Remove.
    app->init_vk_swapchain();

    app->prepare();

    while (true) {
        PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE);
        if (msg.message == WM_QUIT) {
            break;
        }
        else {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        RedrawWindow(app->window.hwnd, nullptr, nullptr, RDW_INTERNALPAINT);
    }

    app.reset();

    return (int)msg.wParam;
}

