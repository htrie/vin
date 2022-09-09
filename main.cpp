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

    Window() {}
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

struct Frame {
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
    vk::UniqueInstance instance; // [TODO] Move most to Device.
    vk::UniqueSurfaceKHR surface;
    vk::UniqueDevice device;
    vk::UniqueCommandPool cmd_pool;
    vk::UniqueDescriptorPool desc_pool;

    vk::UniqueRenderPass render_pass;
    vk::UniqueDescriptorSetLayout desc_layout;
    vk::UniquePipelineLayout pipeline_layout;
    vk::UniquePipeline pipeline;

    vk::UniqueSwapchainKHR swapchain; // [TODO] Move most to Frames.
    std::vector<Frame> frames;

    static const unsigned frame_lag = 2;
    vk::UniqueFence fences[frame_lag];
    vk::UniqueSemaphore image_acquired_semaphores[frame_lag];
    vk::UniqueSemaphore draw_complete_semaphores[frame_lag];
    uint32_t frame_index = 0;

    vk::PhysicalDevice gpu;
    vk::Queue queue;
    vk::SurfaceFormatKHR surface_format;

    Matrices matrices;

    void patch(void* mem) {
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

        memcpy(mem, &uniforms, sizeof uniforms);
    }

    void resize() {
        if (!device) // [TODO] Remove.
            return;

        wait_idle(device.get());

        swapchain = create_swapchain(gpu, device.get(), surface.get(), surface_format, swapchain.get(), window.width, window.height);

        const auto swapchain_images = get_swapchain_images(device.get(), swapchain.get());

        frames.clear();
        frames.resize(swapchain_images.size());

        for (uint32_t i = 0; i < swapchain_images.size(); ++i) {
            frames[i].image = swapchain_images[i];
            frames[i].view = create_image_view(device.get(), swapchain_images[i], surface_format);

            frames[i].uniform_buffer = create_uniform_buffer(device.get(), sizeof(Uniforms));
            frames[i].uniform_memory = create_uniform_memory(gpu, device.get(), frames[i].uniform_buffer.get());
            bind_memory(device.get(), frames[i].uniform_buffer.get(), frames[i].uniform_memory.get());
            frames[i].uniform_memory_ptr = map_memory(device.get(), frames[i].uniform_memory.get());

            frames[i].cmd = create_command_buffer(device.get(), cmd_pool.get());
            frames[i].descriptor_set = create_descriptor_set(device.get(), desc_pool.get(), desc_layout.get());
            update_descriptor_set(device.get(), frames[i].descriptor_set.get(), frames[i].uniform_buffer.get(), sizeof(Uniforms));
            frames[i].framebuffer = create_framebuffer(device.get(), render_pass.get(), frames[i].view.get(), window.width, window.height);
        }
    }

    void redraw() {
        wait(device.get(), fences[frame_index].get());
        const auto index = acquire(device.get(), swapchain.get(), image_acquired_semaphores[frame_index].get());

        patch(frames[index].uniform_memory_ptr);

        begin(frames[index].cmd.get());
        const auto clear_value = vk::ClearColorValue(std::array<float, 4>({ {0.2f, 0.2f, 0.2f, 0.2f} }));
        begin_pass(frames[index].cmd.get(), render_pass.get(), frames[index].framebuffer.get(), clear_value, window.width, window.height);

        bind_pipeline(frames[index].cmd.get(), pipeline.get());
        bind_descriptor_set(frames[index].cmd.get(), pipeline_layout.get(), frames[index].descriptor_set.get());

        set_viewport(frames[index].cmd.get(), (float)window.width, (float)window.height);
        set_scissor(frames[index].cmd.get(), window.width, window.height);

        draw(frames[index].cmd.get(), 12 * 3);

        end_pass(frames[index].cmd.get());
        end(frames[index].cmd.get());

        submit(queue, image_acquired_semaphores[frame_index].get(), draw_complete_semaphores[frame_index].get(), frames[index].cmd.get(), fences[frame_index].get());
        present(swapchain.get(), queue, draw_complete_semaphores[frame_index].get(), index);

        frame_index += 1;
        frame_index %= frame_lag;
    }

    bool proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        switch (uMsg) {
        case WM_PAINT: redraw(); return true;
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

    static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
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

public:
    Window window;

    App(HINSTANCE hInstance, int nCmdShow) {
        instance = create_instance();
        gpu = pick_gpu(instance.get());
        window = Window(WndProc, hInstance, nCmdShow, this); // Create as late as possible to avoid seeing empty window.
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

    ~App() {
        wait_idle(device.get());
    }

    void run() {
        MSG msg = {};
        while (GetMessage(&msg, nullptr, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) {
    App app(hInstance, nCmdShow);
    app.run();
    return 0;
}

