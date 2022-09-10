#define NOMINMAX
#define VC_EXTRALEAN
#define WIN32_LEAN_AND_MEAN
#define _HAS_EXCEPTIONS 0

#ifndef NDEBUG
#define VERIFY(expr) \
    if (!(expr)) { \
        MessageBox(nullptr, #expr, "Error", MB_OK); \
        abort(); \
    }
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
#include "win.h"

struct Uniforms {
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
    mat4x4 MVP;

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
        mat4x4_identity(MVP);

        projection_matrix[1][1] *= -1.0f; // Flip projection matrix from GL to Vulkan orientation.
    }

    void update() {
        mat4x4 VP;
        mat4x4_mul(VP, projection_matrix, view_matrix);

        mat4x4 M;
        mat4x4_dup(M, model_matrix);
        mat4x4_rotate_Y(model_matrix, M, (float)degreesToRadians(1.5f));
        mat4x4_orthonormalize(model_matrix, model_matrix);

        mat4x4_mul(MVP, VP, model_matrix);
    }
};

struct Frame { // [TODO] Use class. // [TODO] Move to vk.
    vk::Image image;
    vk::UniqueCommandBuffer cmd;
    vk::UniqueImageView view;
    vk::UniqueBuffer uniform_buffer; // [TODO] Move outside.
    vk::UniqueDeviceMemory uniform_memory;
    void* uniform_memory_ptr = nullptr;
    vk::UniqueFramebuffer framebuffer;
    vk::UniqueDescriptorSet descriptor_set; // [TODO] Move outside.

    Frame(const vk::PhysicalDevice& gpu, const vk::Device& device,
        const vk::CommandPool& cmd_pool, 
        const vk::DescriptorPool& desc_pool, const vk::DescriptorSetLayout& desc_layout,
        const vk::RenderPass& render_pass,
        const vk::Image& swapchain_image, const vk::SurfaceFormatKHR& surface_format,
        uint32_t width, uint32_t height) {
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
};

struct Swapchain { // [TODO] Use class.
    vk::UniqueCommandPool cmd_pool;
    vk::UniqueDescriptorPool desc_pool;
    vk::UniqueRenderPass render_pass;
    vk::UniqueDescriptorSetLayout desc_layout;
    vk::UniquePipelineLayout pipeline_layout;
    vk::UniquePipeline pipeline;
    vk::UniqueSwapchainKHR swapchain;
    std::vector<Frame> frames;

    Swapchain() {}
    Swapchain(const vk::Device& device, const vk::SurfaceFormatKHR& surface_format, uint32_t family_index) {
        cmd_pool = create_command_pool(device, family_index);
        desc_pool = create_descriptor_pool(device);
        desc_layout = create_descriptor_layout(device);
        pipeline_layout = create_pipeline_layout(device, desc_layout.get());
        render_pass = create_render_pass(device, surface_format);
        pipeline = create_pipeline(device, pipeline_layout.get(), render_pass.get());
    }

    void resize(const vk::PhysicalDevice gpu, const vk::Device& device, const vk::SurfaceKHR& surface, const vk::SurfaceFormatKHR& surface_format, unsigned width, unsigned height) {
        swapchain = create_swapchain(gpu, device, surface, surface_format, swapchain.get(), width, height);
        const auto swapchain_images = get_swapchain_images(device, swapchain.get());
        frames.clear();
        for (uint32_t i = 0; i < swapchain_images.size(); ++i) {
            frames.emplace_back(gpu, device, cmd_pool.get(), desc_pool.get(), desc_layout.get(), render_pass.get(), swapchain_images[i], surface_format, width, height);
        }
    }

    void redraw(const std::array<float, 4>& color, unsigned vertex_count, unsigned index, unsigned width, unsigned height) {
        begin(frames[index].cmd.get());
        begin_pass(frames[index].cmd.get(), render_pass.get(), frames[index].framebuffer.get(), color, width, height);

        bind_pipeline(frames[index].cmd.get(), pipeline.get());
        bind_descriptor_set(frames[index].cmd.get(), pipeline_layout.get(), frames[index].descriptor_set.get());

        set_viewport(frames[index].cmd.get(), (float)width, (float)height);
        set_scissor(frames[index].cmd.get(), width, height);

        draw(frames[index].cmd.get(), vertex_count);

        end_pass(frames[index].cmd.get());
        end(frames[index].cmd.get());
    }
};

struct Device { // [TODO] Use class.

};

class App {
    unsigned width = 800;
    unsigned height = 600;

    vk::UniqueInstance instance; // [TODO] Move most to Device.
    vk::PhysicalDevice gpu;
    vk::UniqueSurfaceKHR surface;
    vk::SurfaceFormatKHR surface_format;
    vk::UniqueDevice device;
    vk::Queue queue;

    static const unsigned frame_lag = 2; // [TODO] Move to ?.
    vk::UniqueFence fences[frame_lag];
    vk::UniqueSemaphore image_acquired_semaphores[frame_lag];
    vk::UniqueSemaphore draw_complete_semaphores[frame_lag];
    uint32_t frame_index = 0;

    Swapchain swapchain2; // [TODO] Rename.

    Matrices matrices;

    void patch(void* mem) {
        matrices.update();

        Uniforms uniforms;
        memcpy(uniforms.mvp, matrices.MVP, sizeof(mat4x4));

        for (int32_t i = 0; i < 12 * 3; i++) {
            uniforms.position[i][0] = uniforms.vertex_data[i * 3];
            uniforms.position[i][1] = uniforms.vertex_data[i * 3 + 1];
            uniforms.position[i][2] = uniforms.vertex_data[i * 3 + 2];
            uniforms.position[i][3] = 1.0f;
        }

        memcpy(mem, &uniforms, sizeof uniforms);
    }

    void resize(unsigned w, unsigned h) {
        width = w;
        height = h;

        if (!device) // [TODO] Remove.
            return;

        wait_idle(device.get());

        swapchain2.resize(gpu, device.get(), surface.get(), surface_format, width, height);
    }

    void redraw() {
        wait(device.get(), fences[frame_index].get());
        const auto index = acquire(device.get(), swapchain2.swapchain.get(), image_acquired_semaphores[frame_index].get());

        patch(swapchain2.frames[index].uniform_memory_ptr);

        const std::array<float, 4> color = { 0.2f, 0.2f, 0.2f, 1.0f };
        const auto vertex_count = 12 * 3;
        swapchain2.redraw(color, vertex_count, index, width, height);

        submit(queue, image_acquired_semaphores[frame_index].get(), draw_complete_semaphores[frame_index].get(), swapchain2.frames[index].cmd.get(), fences[frame_index].get());
        present(swapchain2.swapchain.get(), queue, draw_complete_semaphores[frame_index].get(), index);

        frame_index += 1;
        frame_index %= frame_lag;
    }

    static LRESULT CALLBACK proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        switch (uMsg) {
        case WM_CREATE: {
            LPCREATESTRUCT create_struct = reinterpret_cast<LPCREATESTRUCT>(lParam);
            SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create_struct->lpCreateParams));
            return 0;
        }
        case WM_DESTROY:
        case WM_CLOSE: {
            PostQuitMessage(0);
            return 0;
        }
        case WM_GETMINMAXINFO: {
            // Window client area size must be at least 1 pixel high, to prevent crash.
            const POINT minsize = { GetSystemMetrics(SM_CXMINTRACK), GetSystemMetrics(SM_CYMINTRACK) + 1 };
            ((MINMAXINFO*)lParam)->ptMinTrackSize = minsize;
            return 0;
        }
        case WM_PAINT: {
            if (auto* app = reinterpret_cast<App*>(GetWindowLongPtr(hWnd, GWLP_USERDATA)))
                app->redraw();
            return 0;
        }
        case WM_SIZE: {
            if (auto* app = reinterpret_cast<App*>(GetWindowLongPtr(hWnd, GWLP_USERDATA))) {
                const unsigned width = lParam & 0xffff;
                const unsigned height = (lParam & 0xffff0000) >> 16;
                app->resize(width, height);
            }
            return 0;
        }
        }
        return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }

public:
    App(HINSTANCE hInstance, int nCmdShow) {
        instance = create_instance();
        gpu = pick_gpu(instance.get());
        auto hWnd = create_window(proc, hInstance, nCmdShow, this, width, height);
        surface = create_surface(instance.get(), hInstance, hWnd);
        surface_format = select_format(gpu, surface.get());
        auto family_index = find_queue_family(gpu, surface.get());
        device = create_device(gpu, family_index);
        queue = fetch_queue(device.get(), family_index);

        swapchain2 = Swapchain(device.get(), surface_format, family_index);

        for (uint32_t i = 0; i < frame_lag; i++) {
            fences[i] = create_fence(device.get());
            image_acquired_semaphores[i] = create_semaphore(device.get());
            draw_complete_semaphores[i] = create_semaphore(device.get());
        }

        resize(width, height);
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

