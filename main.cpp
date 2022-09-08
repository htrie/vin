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

    void draw_build_cmd(vk::CommandBuffer);

    void wait_idle() const;

    void wait() const;
    void acquire();
    void update_data_buffer();
    void submit() const;
    void present();

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
    surface_format = select_format(gpu, surface.get());
    auto family_index = find_queue_family(gpu, surface.get());
    device = create_device(gpu, family_index);
    queue = fetch_queue(device.get(), family_index);
    cmd_pool = create_command_pool(device.get(), family_index);
    desc_layout = create_descriptor_layout(device.get());
    pipeline_layout = create_pipeline_layout(device.get(), desc_layout.get());
    render_pass = create_render_pass(device.get(), surface_format);
    pipeline = create_pipeline(device.get(), pipeline_layout.get(), render_pass.get());
    desc_pool = create_descriptor_pool(device.get());

    for (uint32_t i = 0; i < frame_lag; i++) {
        fences[i] = create_fence(device.get());
        image_acquired_semaphores[i] = create_semaphore(device.get());
        draw_complete_semaphores[i] = create_semaphore(device.get());
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

void App::resize() {
    if (!device) // [TODO] Remove.
        return;

    wait_idle();

    swapchain = create_swapchain(gpu, device.get(), surface.get(), surface_format, swapchain.get(), window.width, window.height);

    uint32_t swapchain_image_count = 0;
    auto result = device->getSwapchainImagesKHR(swapchain.get(), &swapchain_image_count, static_cast<vk::Image*>(nullptr));
    VERIFY(result == vk::Result::eSuccess);

    std::unique_ptr<vk::Image[]> swapchainImages(new vk::Image[swapchain_image_count]);
    result = device->getSwapchainImagesKHR(swapchain.get(), &swapchain_image_count, swapchainImages.get());
    VERIFY(result == vk::Result::eSuccess);

    swapchain_image_resources.reset(new SwapchainImageResources[swapchain_image_count]);

    for (uint32_t i = 0; i < swapchain_image_count; ++i) {
        swapchain_image_resources[i].image = swapchainImages[i];
        swapchain_image_resources[i].view = create_image_view(device.get(), swapchainImages[i], surface_format);

        swapchain_image_resources[i].uniform_buffer = create_uniform_buffer(device.get(), sizeof(Uniforms));
        swapchain_image_resources[i].uniform_memory = create_uniform_memory(gpu, device.get(), swapchain_image_resources[i].uniform_buffer.get());
        bind_memory(device.get(), swapchain_image_resources[i].uniform_buffer.get(), swapchain_image_resources[i].uniform_memory.get());
        swapchain_image_resources[i].uniform_memory_ptr = map_memory(device.get(), swapchain_image_resources[i].uniform_memory.get());

        swapchain_image_resources[i].cmd = create_command_buffer(device.get(), cmd_pool.get());
        swapchain_image_resources[i].descriptor_set = create_descriptor_set(device.get(), desc_pool.get(), desc_layout.get());
        update_descriptor_set(device.get(), swapchain_image_resources[i].descriptor_set.get(), swapchain_image_resources[i].uniform_buffer.get(), sizeof(Uniforms));
        swapchain_image_resources[i].framebuffer = create_framebuffer(device.get(), render_pass.get(), swapchain_image_resources[i].view.get(), window.width, window.height);

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

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) {
    App app(hInstance, nCmdShow);
    app.run();
    return 0;
}

