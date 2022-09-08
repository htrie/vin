#define NOMINMAX
#define VC_EXTRALEAN
#define WIN32_LEAN_AND_MEAN
#define _HAS_EXCEPTIONS 0

#ifndef NDEBUG
#define VERIFY(x) assert(x)
#else
#define VERIFY(x) ((void)(x))
#endif

#define ERR_EXIT(err_msg, err_class) \
    do { \
        MessageBox(nullptr, err_msg, err_class, MB_OK); \
        exit(1); \
    } while (0)

#include "linmath.h"
#include "vk.h"

const uint32_t vert_bytecode[] =
#include "shader.vert.inc"
;
const uint32_t frag_bytecode[] =
#include "shader.frag.inc"
;

struct Uniforms{
    float mvp[4][4];
    float position[12 * 3][4];

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
};

struct Matrices {
    mat4x4 projection_matrix;
    mat4x4 view_matrix;
    mat4x4 model_matrix;

    Matrices() {
        vec3 eye = { 0.0f, 3.0f, 5.0f };
        vec3 origin = { 0, 0, 0 };
        vec3 up = { 0.0f, 1.0f, 0.0 };

        memset(projection_matrix, 0, sizeof(projection_matrix));
        memset(view_matrix, 0, sizeof(view_matrix));
        memset(model_matrix, 0, sizeof(model_matrix));

        mat4x4_perspective(projection_matrix, (float)degreesToRadians(45.0f), 1.0f, 0.1f, 100.0f);
        mat4x4_look_at(view_matrix, eye, origin, up);
        mat4x4_identity(model_matrix);

        projection_matrix[1][1] *= -1.0f; // Flip projection matrix from GL to Vulkan orientation.
    }
};

struct Window {
    HINSTANCE hinstance = nullptr;
    HWND hwnd = nullptr;
    int32_t width = 800;
    int32_t height = 600;

    Window(WNDPROC proc, HINSTANCE hInstance, int nCmdShow, void* data)
        : hinstance(hInstance) {

        const char* name = "vin";

        WNDCLASSEX win_class;
        win_class.cbSize = sizeof(WNDCLASSEX);
        win_class.style = CS_HREDRAW | CS_VREDRAW;
        win_class.lpfnWndProc = proc;
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

        RECT wr = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
        AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);

        hwnd = CreateWindowEx(0, name, name, WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, wr.right - wr.left, wr.bottom - wr.top,
            nullptr, nullptr, hinstance, data);
        if (!hwnd) {
            ERR_EXIT("Cannot create a window in which to draw!\n", "CreateWindow Failure");
        }

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

class App {
    vk::UniqueInstance instance;
    vk::UniqueSurfaceKHR surface;
    vk::UniqueDevice device;
    vk::UniqueCommandPool cmd_pool;
    vk::UniqueDescriptorPool desc_pool;

    vk::UniqueRenderPass render_pass;
    vk::UniqueDescriptorSetLayout desc_layout;
    vk::UniquePipelineLayout pipeline_layout;
    vk::UniquePipeline pipeline;

    vk::UniqueSwapchainKHR swapchain;
    std::unique_ptr<SwapchainImageResources[]> swapchain_image_resources;
    uint32_t current_buffer = 0;

    static const unsigned frame_lag = 2;
    vk::UniqueFence fences[frame_lag];
    vk::UniqueSemaphore image_acquired_semaphores[frame_lag];
    vk::UniqueSemaphore draw_complete_semaphores[frame_lag];
    uint32_t frame_index = 0;

    vk::PhysicalDevice gpu;
    vk::Queue queue;
    vk::SurfaceFormatKHR surface_format;

    Matrices matrices;

    uint32_t find_queue_family() const;
    vk::SurfaceFormatKHR select_format() const;
    vk::UniqueDevice create_device(uint32_t family_index) const;
    vk::Queue fetch_queue(uint32_t family_index) const;
    vk::UniqueCommandPool create_command_pool(uint32_t family_index) const;
    vk::UniqueCommandBuffer create_command_buffer() const;
    vk::UniqueSwapchainKHR create_swapchain() const;
    vk::UniqueFence create_fence() const;
    vk::UniqueSemaphore create_semaphore() const;
    vk::UniqueImageView create_image_view(const vk::Image& image) const;
    vk::UniqueFramebuffer create_framebuffer(const vk::ImageView& image_view) const;
    vk::UniqueBuffer create_uniform_buffer() const;
    vk::UniqueDeviceMemory create_uniform_memory(const vk::Buffer& buffer) const;
    void* map_memory(const vk::DeviceMemory& memory) const;
    void bind_memory(const vk::Buffer& buffer, const vk::DeviceMemory& memory) const;
    vk::UniqueDescriptorSetLayout create_descriptor_layout() const;
    vk::UniquePipelineLayout create_pipeline_layout() const;
    vk::UniqueDescriptorPool create_descriptor_pool() const;
    vk::UniqueDescriptorSet create_descriptor_set() const;
    void update_descriptor_set(vk::DescriptorSet& desc_set, const vk::Buffer& buffer) const;
    vk::UniqueShaderModule create_module(const uint32_t*, size_t) const;
    vk::UniquePipeline create_pipeline() const;
    vk::UniqueRenderPass create_render_pass() const;

    void draw_build_cmd(vk::CommandBuffer);

    void wait_idle() const;

    void wait() const;
    void acquire();
    void update_data_buffer();
    void submit() const;
    void present();

    bool memory_type_from_properties(uint32_t, vk::MemoryPropertyFlags, uint32_t*) const;

    void resize();
    void draw();

    bool proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

public:
    Window window;

    App(HINSTANCE hInstance, int nCmdShow);
    ~App();

    void run();
};

LRESULT CALLBACK App::WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE: {
        LPCREATESTRUCT create_struct = reinterpret_cast<LPCREATESTRUCT>(lParam);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create_struct->lpCreateParams));
        return 0;
    }
    case WM_CLOSE: PostQuitMessage(0); break;
    case WM_GETMINMAXINFO: {
        // Window client area size must be at least 1 pixel high, to prevent crash.
        const POINT minsize = { GetSystemMetrics(SM_CXMINTRACK), GetSystemMetrics(SM_CYMINTRACK) + 1 };
        ((MINMAXINFO*)lParam)->ptMinTrackSize = minsize;
        return 0;
    }
    }

    if (auto* app = reinterpret_cast<App*>(GetWindowLongPtr(hWnd, GWLP_USERDATA)))
        if (app->proc(hWnd, uMsg, wParam, lParam))
            return 0;
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

bool App::proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_PAINT: draw(); return true;
    case WM_SIZE: {
            // Resize the application to the new window size, except when
            // it was minimized. Vulkan doesn't support images or swapchains
            // with width=0 and height=0.
            if (wParam != SIZE_MINIMIZED) {
                window.width = lParam & 0xffff;
                window.height = (lParam & 0xffff0000) >> 16;
                resize();
            }
            return true;
        }
    }
    return false;
}

App::App(HINSTANCE hInstance, int nCmdShow)
    : window(WndProc, hInstance, nCmdShow, this) {
    instance = create_instance();
    gpu = pick_gpu(instance.get());
    surface = create_surface(instance.get(), window.hinstance, window.hwnd);
    auto family_index = find_queue_family();
    device = create_device(family_index);
    surface_format = select_format();
    queue = fetch_queue(family_index);
    cmd_pool = create_command_pool(family_index);
    desc_layout = create_descriptor_layout();
    pipeline_layout = create_pipeline_layout();
    render_pass = create_render_pass();
    pipeline = create_pipeline();
    desc_pool = create_descriptor_pool();

    for (uint32_t i = 0; i < frame_lag; i++) {
        fences[i] = create_fence();
        image_acquired_semaphores[i] = create_semaphore();
        draw_complete_semaphores[i] = create_semaphore();
    }

    resize();
}

App::~App() {
    wait_idle();
}

void App::run() {
    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

void App::draw() {
    wait();
    acquire();
    update_data_buffer(); // [TODO] Rename to record.
    submit();
    present();

    frame_index += 1;
    frame_index %= frame_lag;
}

void App::wait_idle() const {
    const auto result = device->waitIdle();
    VERIFY(result == vk::Result::eSuccess);
}

void App::wait() const {
    // Ensure no more than frame lag renderings are outstanding
    const auto result = device->waitForFences(1, &fences[frame_index].get(), VK_TRUE, UINT64_MAX);
    VERIFY(result == vk::Result::eSuccess);
    device->resetFences({ fences[frame_index].get() });
}

void App::acquire() {
    vk::Result result;
    do {
        result = device->acquireNextImageKHR(swapchain.get(), UINT64_MAX, image_acquired_semaphores[frame_index].get(), vk::Fence(), &current_buffer);
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
            surface.reset();
            surface = create_surface(instance.get(), window.hinstance, window.hwnd);
            resize();
        }
        else {
            VERIFY(result == vk::Result::eSuccess);
        }
    } while (result != vk::Result::eSuccess);
}

void App::submit() const {
    // Wait for the image acquired semaphore to be signaled to ensure
    // that the image won't be rendered to until the presentation
    // engine has fully released ownership to the application, and it is
    // okay to render to the image.
    vk::PipelineStageFlags const pipe_stage_flags = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    auto const submit_info = vk::SubmitInfo()
        .setPWaitDstStageMask(&pipe_stage_flags)
        .setWaitSemaphoreCount(1)
        .setPWaitSemaphores(&image_acquired_semaphores[frame_index].get())
        .setCommandBufferCount(1)
        .setPCommandBuffers(&swapchain_image_resources[current_buffer].cmd.get())
        .setSignalSemaphoreCount(1)
        .setPSignalSemaphores(&draw_complete_semaphores[frame_index].get());

    const auto result = queue.submit(1, &submit_info, fences[frame_index].get());
    VERIFY(result == vk::Result::eSuccess);
}

void App::present() {
    // wait for draw complete
    auto const present_info = vk::PresentInfoKHR()
        .setWaitSemaphoreCount(1)
        .setPWaitSemaphores(&draw_complete_semaphores[frame_index].get())
        .setSwapchainCount(1)
        .setPSwapchains(&swapchain.get())
        .setPImageIndices(&current_buffer);

    const auto result = queue.presentKHR(&present_info);
    if (result == vk::Result::eErrorOutOfDateKHR) {
        // swapchain is out of date (e.g. the window was resized) and must be recreated:
        resize();
    }
    else if (result == vk::Result::eSuboptimalKHR) {
        // SUBOPTIMAL could be due to resize
        vk::SurfaceCapabilitiesKHR surf_caps;
        const auto result = gpu.getSurfaceCapabilitiesKHR(surface.get(), &surf_caps);
        VERIFY(result == vk::Result::eSuccess);

        if (surf_caps.currentExtent.width != static_cast<uint32_t>(window.width) ||
            surf_caps.currentExtent.height != static_cast<uint32_t>(window.height)) {
            resize();
        }
    }
    else if (result == vk::Result::eErrorSurfaceLostKHR) {
        surface.reset();
        surface = create_surface(instance.get(), window.hinstance, window.hwnd);
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
        .setFramebuffer(swapchain_image_resources[current_buffer].framebuffer.get())
        .setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D((uint32_t)window.width, (uint32_t)window.height)))
        .setClearValueCount(2)
        .setPClearValues(clearValues);

    auto result = commandBuffer.begin(&command_buffer_info);
    VERIFY(result == vk::Result::eSuccess);

    commandBuffer.beginRenderPass(&pass_info, vk::SubpassContents::eInline);
    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.get());
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline_layout.get(), 0, 1, &swapchain_image_resources[current_buffer].descriptor_set.get(), 0, nullptr);
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

uint32_t App::find_queue_family() const {
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
        const auto result = gpu.getSurfaceSupportKHR(i, surface.get(), &supportsPresent[i]);
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

vk::UniqueDevice App::create_device(uint32_t family_index) const {
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

vk::SurfaceFormatKHR App::select_format() const {
    // Get the list of VkFormat's that are supported:
    uint32_t format_count = 0;
    auto result = gpu.getSurfaceFormatsKHR(surface.get(), &format_count, static_cast<vk::SurfaceFormatKHR*>(nullptr));
    VERIFY(result == vk::Result::eSuccess);
    VERIFY(format_count > 0);

    std::unique_ptr<vk::SurfaceFormatKHR[]> surface_formats(new vk::SurfaceFormatKHR[format_count]);
    result = gpu.getSurfaceFormatsKHR(surface.get(), &format_count, surface_formats.get());
    VERIFY(result == vk::Result::eSuccess);

    // If the format list includes just one entry of VK_FORMAT_UNDEFINED,
    // the surface has no preferred format.  Otherwise, at least one
    // supported format will be returned.
    vk::SurfaceFormatKHR res = surface_formats[0];
    if (surface_formats[0].format == vk::Format::eUndefined)
        res.format = vk::Format::eB8G8R8A8Unorm;
    return res;
}

vk::Queue App::fetch_queue(uint32_t family_index) const {
    vk::Queue queue;
    device->getQueue(family_index, 0, &queue);
    return queue;
}

vk::UniqueCommandPool App::create_command_pool(uint32_t family_index) const {
    auto const cmd_pool_info = vk::CommandPoolCreateInfo()
        .setQueueFamilyIndex(family_index);

    auto cmd_pool_handle = device->createCommandPoolUnique(cmd_pool_info);
    VERIFY(cmd_pool_handle.result == vk::Result::eSuccess);
    return std::move(cmd_pool_handle.value);
}

vk::UniqueFence App::create_fence() const {
    auto const fence_info = vk::FenceCreateInfo()
        .setFlags(vk::FenceCreateFlagBits::eSignaled);

    auto fence_handle = device->createFenceUnique(fence_info);
    VERIFY(fence_handle.result == vk::Result::eSuccess);
    return std::move(fence_handle.value);
}

vk::UniqueSemaphore App::create_semaphore() const {
    auto const semaphore_info = vk::SemaphoreCreateInfo();

    auto semaphore_handle = device->createSemaphoreUnique(semaphore_info);
    VERIFY(semaphore_handle.result == vk::Result::eSuccess);
    return std::move(semaphore_handle.value);
}

vk::UniqueCommandBuffer App::create_command_buffer() const {
    auto const cmd_info = vk::CommandBufferAllocateInfo()
        .setCommandPool(cmd_pool.get())
        .setLevel(vk::CommandBufferLevel::ePrimary)
        .setCommandBufferCount(1);

    auto cmd_handles = device->allocateCommandBuffersUnique(cmd_info);
    VERIFY(cmd_handles.result == vk::Result::eSuccess);
    return std::move(cmd_handles.value[0]);
}

vk::UniqueSwapchainKHR App::create_swapchain() const {
    vk::SurfaceCapabilitiesKHR surf_caps;
    const auto result = gpu.getSurfaceCapabilitiesKHR(surface.get(), &surf_caps);
    VERIFY(result == vk::Result::eSuccess);

    vk::Extent2D extent;
    // width and height are either both -1, or both not -1.
    if (surf_caps.currentExtent.width == (uint32_t)-1) {
        // If the surface size is undefined, the size is set to the size of the images requested.
        extent.width = window.width;
        extent.height = window.height;
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
        .setSurface(surface.get())
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
        .setOldSwapchain(swapchain.get());

    auto swapchain_handle = device->createSwapchainKHRUnique(swapchain_info);
    VERIFY(swapchain_handle.result == vk::Result::eSuccess);
    return std::move(swapchain_handle.value);
}

vk::UniqueImageView App::create_image_view(const vk::Image& image) const {
    auto view_info = vk::ImageViewCreateInfo()
        .setViewType(vk::ImageViewType::e2D)
        .setFormat(surface_format.format)
        .setImage(image)
        .setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));

    auto view_handle = device->createImageViewUnique(view_info);
    VERIFY(view_handle.result == vk::Result::eSuccess);
    return std::move(view_handle.value);
}

vk::UniqueBuffer App::create_uniform_buffer() const {
    auto const buf_info = vk::BufferCreateInfo()
        .setSize(sizeof(Uniforms))
        .setUsage(vk::BufferUsageFlagBits::eUniformBuffer);

    auto buffer_handle = device->createBufferUnique(buf_info);
    VERIFY(buffer_handle.result == vk::Result::eSuccess);
    return std::move(buffer_handle.value);
}

vk::UniqueDeviceMemory App::create_uniform_memory(const vk::Buffer& buffer) const {
    vk::MemoryRequirements mem_reqs;
    device->getBufferMemoryRequirements(buffer, &mem_reqs);

    auto mem_info = vk::MemoryAllocateInfo()
        .setAllocationSize(mem_reqs.size)
        .setMemoryTypeIndex(0);

    bool const pass = memory_type_from_properties(mem_reqs.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, &mem_info.memoryTypeIndex);
    VERIFY(pass);

    auto memory_handle = device->allocateMemoryUnique(mem_info);
    VERIFY(memory_handle.result == vk::Result::eSuccess);
    return std::move(memory_handle.value);
}

void* App::map_memory(const vk::DeviceMemory& memory) const {
    void* mem = nullptr;
    const auto result = device->mapMemory(memory, 0, VK_WHOLE_SIZE, vk::MemoryMapFlags(), &mem);
    VERIFY(result == vk::Result::eSuccess);
    return mem;
}

void App::bind_memory(const vk::Buffer& buffer, const vk::DeviceMemory& memory) const {
    const auto result = device->bindBufferMemory(buffer, memory, 0);
    VERIFY(result == vk::Result::eSuccess);
}

vk::UniqueDescriptorSetLayout App::create_descriptor_layout() const {
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
    return std::move(desc_layout_handle.value);
}

vk::UniquePipelineLayout App::create_pipeline_layout() const {
    auto const pipeline_layout_info = vk::PipelineLayoutCreateInfo()
        .setSetLayoutCount(1)
        .setPSetLayouts(&desc_layout.get());

    auto pipeline_layout_handle = device->createPipelineLayoutUnique(pipeline_layout_info);
    VERIFY(pipeline_layout_handle.result == vk::Result::eSuccess);
    return std::move(pipeline_layout_handle.value);
}

vk::UniqueDescriptorPool App::create_descriptor_pool() const {
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

    auto desc_pool_handle = device->createDescriptorPoolUnique(desc_pool_info);
    VERIFY(desc_pool_handle.result == vk::Result::eSuccess);
    return std::move(desc_pool_handle.value);
}

vk::UniqueDescriptorSet App::create_descriptor_set() const {
    auto const alloc_info = vk::DescriptorSetAllocateInfo()
        .setDescriptorPool(desc_pool.get())
        .setDescriptorSetCount(1)
        .setPSetLayouts(&desc_layout.get());

    auto descriptor_set_handles = device->allocateDescriptorSetsUnique(alloc_info);
    VERIFY(descriptor_set_handles.result == vk::Result::eSuccess);
    return std::move(descriptor_set_handles.value[0]);
}

void App::update_descriptor_set(vk::DescriptorSet& desc_set, const vk::Buffer& buffer) const {
    const auto buffer_info = vk::DescriptorBufferInfo()
        .setOffset(0)
        .setRange(sizeof(struct Uniforms))
        .setBuffer(buffer);

    const vk::WriteDescriptorSet writes[1] = { vk::WriteDescriptorSet()
        .setDescriptorCount(1)
        .setDescriptorType(vk::DescriptorType::eUniformBuffer)
        .setPBufferInfo(&buffer_info)
        .setDstSet(desc_set)
    };

    device->updateDescriptorSets(1, writes, 0, nullptr);
}

vk::UniqueFramebuffer App::create_framebuffer(const vk::ImageView& image_view) const {
    const vk::ImageView attachments[1] = { image_view };

    auto const fb_info = vk::FramebufferCreateInfo()
        .setRenderPass(render_pass.get())
        .setAttachmentCount(1)
        .setPAttachments(attachments)
        .setWidth((uint32_t)window.width)
        .setHeight((uint32_t)window.height)
        .setLayers(1);

    auto framebuffer_handle = device->createFramebufferUnique(fb_info);
    VERIFY(framebuffer_handle.result == vk::Result::eSuccess);
    return std::move(framebuffer_handle.value);
}

vk::UniquePipeline App::create_pipeline() const {
    const auto vert_shader_module = create_module(vert_bytecode, sizeof(vert_bytecode));
    const auto frag_shader_module = create_module(frag_bytecode, sizeof(frag_bytecode));

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
    return std::move(pipeline_handles.value[0]);
}

vk::UniqueRenderPass App::create_render_pass() const {
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

    auto render_pass_handle = device->createRenderPassUnique(rp_info);
    VERIFY(render_pass_handle.result == vk::Result::eSuccess);
    return std::move(render_pass_handle.value);
}

vk::UniqueShaderModule App::create_module(const uint32_t* code, size_t size) const {
    const auto module_info = vk::ShaderModuleCreateInfo()
        .setCodeSize(size)
        .setPCode(code);

    auto module_handle = device->createShaderModuleUnique(module_info);
    VERIFY(module_handle.result == vk::Result::eSuccess);
    return std::move(module_handle.value);
}

void App::resize() {
    if (!device) // [TODO] Remove.
        return;

    wait_idle();

    swapchain = create_swapchain();

    uint32_t swapchain_image_count = 0;
    auto result = device->getSwapchainImagesKHR(swapchain.get(), &swapchain_image_count, static_cast<vk::Image*>(nullptr));
    VERIFY(result == vk::Result::eSuccess);

    std::unique_ptr<vk::Image[]> swapchainImages(new vk::Image[swapchain_image_count]);
    result = device->getSwapchainImagesKHR(swapchain.get(), &swapchain_image_count, swapchainImages.get());
    VERIFY(result == vk::Result::eSuccess);

    swapchain_image_resources.reset(new SwapchainImageResources[swapchain_image_count]);

    for (uint32_t i = 0; i < swapchain_image_count; ++i) {
        swapchain_image_resources[i].image = swapchainImages[i];
        swapchain_image_resources[i].view = create_image_view(swapchainImages[i]);

        swapchain_image_resources[i].uniform_buffer = create_uniform_buffer();
        swapchain_image_resources[i].uniform_memory = create_uniform_memory(swapchain_image_resources[i].uniform_buffer.get());
        bind_memory(swapchain_image_resources[i].uniform_buffer.get(), swapchain_image_resources[i].uniform_memory.get());
        swapchain_image_resources[i].uniform_memory_ptr = map_memory(swapchain_image_resources[i].uniform_memory.get());

        swapchain_image_resources[i].cmd = create_command_buffer();
        swapchain_image_resources[i].descriptor_set = create_descriptor_set();
        update_descriptor_set(swapchain_image_resources[i].descriptor_set.get(), swapchain_image_resources[i].uniform_buffer.get());
        swapchain_image_resources[i].framebuffer = create_framebuffer(swapchain_image_resources[i].view.get());

        current_buffer = i;
        draw_build_cmd(swapchain_image_resources[i].cmd.get()); // [TODO] Do every frame.
    }

    current_buffer = 0;
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

    Uniforms uniforms;
    memcpy(uniforms.mvp, MVP, sizeof(MVP));

    for (int32_t i = 0; i < 12 * 3; i++) {
        uniforms.position[i][0] = uniforms.vertex_data[i * 3];
        uniforms.position[i][1] = uniforms.vertex_data[i * 3 + 1];
        uniforms.position[i][2] = uniforms.vertex_data[i * 3 + 2];
        uniforms.position[i][3] = 1.0f;
    }

    memcpy(swapchain_image_resources[current_buffer].uniform_memory_ptr, &uniforms, sizeof uniforms);
}

bool App::memory_type_from_properties(uint32_t typeBits, vk::MemoryPropertyFlags requirements_mask, uint32_t* typeIndex) const {
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

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) {
    App app(hInstance, nCmdShow);
    app.run();
    return 0;
}

