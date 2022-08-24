
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

#define LOG(txt) OutputDebugString(txt)

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

typedef struct {
    vk::Image image;
    vk::CommandBuffer cmd;
    vk::CommandBuffer graphics_to_present_cmd;
    vk::ImageView view;
    vk::Buffer uniform_buffer;
    vk::DeviceMemory uniform_memory;
    void* uniform_memory_ptr;
    vk::Framebuffer framebuffer;
    vk::DescriptorSet descriptor_set;
} SwapchainImageResources;

struct Window {
    HINSTANCE hinstance = nullptr;
    HWND hwnd = nullptr;
    char name[APP_NAME_STR_LEN];

    POINT minsize = { 0, 0 }; // [TODO] Make private.
    int32_t width = 800; // [TODO] Make private.
    int32_t height = 600; // [TODO] Make private.
};

class App {
    vk::SurfaceKHR surface;
    bool prepared = false;
    bool separate_present_queue = false;
    int32_t gpu_number = -1;

    vk::Instance inst;
    vk::PhysicalDevice gpu;
    vk::Device device;
    vk::Queue graphics_queue;
    vk::Queue present_queue;
    uint32_t graphics_queue_family_index = 0;
    uint32_t present_queue_family_index = 0;
    vk::Semaphore image_acquired_semaphores[FRAME_LAG];
    vk::Semaphore draw_complete_semaphores[FRAME_LAG];
    vk::Semaphore image_ownership_semaphores[FRAME_LAG];
    vk::PhysicalDeviceProperties gpu_props;
    std::unique_ptr<vk::QueueFamilyProperties[]> queue_props;
    vk::PhysicalDeviceMemoryProperties memory_properties;

    uint32_t enabled_extension_count = 0;
    uint32_t enabled_layer_count = 0;
    char const* extension_names[64];
    char const* enabled_layers[64];

    vk::Format format;
    vk::ColorSpaceKHR color_space;

    uint32_t swapchainImageCount = 0;
    vk::SwapchainKHR swapchain;
    std::unique_ptr<SwapchainImageResources[]> swapchain_image_resources;
    vk::PresentModeKHR presentMode = vk::PresentModeKHR::eFifo;
    vk::Fence fences[FRAME_LAG];
    uint32_t frame_index = 0;

    vk::CommandPool cmd_pool;
    vk::CommandPool present_cmd_pool;

    struct {
        vk::Format format;
        vk::Image image;
        vk::MemoryAllocateInfo mem_alloc;
        vk::DeviceMemory mem;
        vk::ImageView view;
    } depth;

    struct {
        vk::Buffer buf;
        vk::MemoryAllocateInfo mem_alloc;
        vk::DeviceMemory mem;
        vk::DescriptorBufferInfo buffer_info;
    } uniform_data;

    vk::CommandBuffer cmd;
    vk::PipelineLayout pipeline_layout;
    vk::DescriptorSetLayout desc_layout;
    vk::PipelineCache pipelineCache;
    vk::RenderPass render_pass;
    vk::Pipeline pipeline;

    mat4x4 projection_matrix;
    mat4x4 view_matrix;
    mat4x4 model_matrix;

    vk::DescriptorPool desc_pool;
    vk::DescriptorSet desc_set;

    std::unique_ptr<vk::Framebuffer[]> framebuffers;

    uint32_t curFrame = 0;
    uint32_t frameCount = UINT32_MAX;

    uint32_t current_buffer = 0;
    uint32_t queue_family_count = 0;

    vk::Bool32 check_layers(uint32_t, const char* const*, uint32_t, vk::LayerProperties*);

    void create_device();

    void init_vk();

    void prepare_buffers();
    void prepare_uniforms();
    void prepare_depth();
    void prepare_descriptor_layout();
    void prepare_descriptor_pool();
    void prepare_descriptor_set();
    void prepare_framebuffers();
    vk::ShaderModule prepare_shader_module(const uint32_t*, size_t);
    vk::ShaderModule prepare_vs();
    vk::ShaderModule prepare_fs();
    void prepare_pipeline();
    void prepare_render_pass();

    void build_image_ownership_cmd(uint32_t const&);
    void draw_build_cmd(vk::CommandBuffer);
    void flush_init_cmd();

    void create_surface();

    void set_image_layout(vk::Image, vk::ImageAspectFlags, vk::ImageLayout, vk::ImageLayout, vk::AccessFlags, vk::PipelineStageFlags, vk::PipelineStageFlags);

    void update_data_buffer();

    bool memory_type_from_properties(uint32_t, vk::MemoryPropertyFlags, uint32_t*);

    void draw();

public:
    Window window;

    App();

    void cleanup();
    void init();
    void init_vk_swapchain();
    void prepare();
    void resize();
    void run();
    void create_window();
};

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);


App::App()
{
    memset(window.name, '\0', APP_NAME_STR_LEN);
    memset(projection_matrix, 0, sizeof(projection_matrix));
    memset(view_matrix, 0, sizeof(view_matrix));
    memset(model_matrix, 0, sizeof(model_matrix));
}

void App::build_image_ownership_cmd(uint32_t const& i) {
    auto const cmd_buf_info = vk::CommandBufferBeginInfo().setFlags(vk::CommandBufferUsageFlagBits::eSimultaneousUse);
    auto result = swapchain_image_resources[i].graphics_to_present_cmd.begin(&cmd_buf_info);
    VERIFY(result == vk::Result::eSuccess);

    auto const image_ownership_barrier =
        vk::ImageMemoryBarrier()
        .setSrcAccessMask(vk::AccessFlags())
        .setDstAccessMask(vk::AccessFlags())
        .setOldLayout(vk::ImageLayout::ePresentSrcKHR)
        .setNewLayout(vk::ImageLayout::ePresentSrcKHR)
        .setSrcQueueFamilyIndex(graphics_queue_family_index)
        .setDstQueueFamilyIndex(present_queue_family_index)
        .setImage(swapchain_image_resources[i].image)
        .setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));

    swapchain_image_resources[i].graphics_to_present_cmd.pipelineBarrier( vk::PipelineStageFlagBits::eBottomOfPipe, vk::PipelineStageFlagBits::eBottomOfPipe, vk::DependencyFlagBits(), 0, nullptr, 0, nullptr, 1, &image_ownership_barrier);

    result = swapchain_image_resources[i].graphics_to_present_cmd.end();
    VERIFY(result == vk::Result::eSuccess);
}

vk::Bool32 App::check_layers(uint32_t check_count, char const* const* const check_names, uint32_t layer_count,
    vk::LayerProperties* layers) {
    for (uint32_t i = 0; i < check_count; i++) {
        vk::Bool32 found = VK_FALSE;
        for (uint32_t j = 0; j < layer_count; j++) {
            if (!strcmp(check_names[i], layers[j].layerName)) {
                found = VK_TRUE;
                break;
            }
        }
        if (!found) {
            fprintf(stderr, "Cannot find layer: %s\n", check_names[i]);
            return 0;
        }
    }
    return VK_TRUE;
}

void App::cleanup() {
    LOG("cleanup\n");
    prepared = false;
    device.waitIdle();

    // Wait for fences from present operations
    for (uint32_t i = 0; i < FRAME_LAG; i++) {
        device.waitForFences(1, &fences[i], VK_TRUE, UINT64_MAX);
        device.destroyFence(fences[i], nullptr);
        device.destroySemaphore(image_acquired_semaphores[i], nullptr);
        device.destroySemaphore(draw_complete_semaphores[i], nullptr);
        if (separate_present_queue) {
            device.destroySemaphore(image_ownership_semaphores[i], nullptr);
        }
    }

    for (uint32_t i = 0; i < swapchainImageCount; i++) {
        device.destroyFramebuffer(swapchain_image_resources[i].framebuffer, nullptr);
    }
    device.destroyDescriptorPool(desc_pool, nullptr);

    device.destroyPipeline(pipeline, nullptr);
    device.destroyPipelineCache(pipelineCache, nullptr);
    device.destroyRenderPass(render_pass, nullptr);
    device.destroyPipelineLayout(pipeline_layout, nullptr);
    device.destroyDescriptorSetLayout(desc_layout, nullptr);

    device.destroySwapchainKHR(swapchain, nullptr);

    device.destroyImageView(depth.view, nullptr);
    device.destroyImage(depth.image, nullptr);
    device.freeMemory(depth.mem, nullptr);

    for (uint32_t i = 0; i < swapchainImageCount; i++) {
        device.destroyImageView(swapchain_image_resources[i].view, nullptr);
        device.freeCommandBuffers(cmd_pool, { swapchain_image_resources[i].cmd });
        device.destroyBuffer(swapchain_image_resources[i].uniform_buffer, nullptr);
        device.unmapMemory(swapchain_image_resources[i].uniform_memory);
        device.freeMemory(swapchain_image_resources[i].uniform_memory, nullptr);
    }

    device.destroyCommandPool(cmd_pool, nullptr);

    if (separate_present_queue) {
        device.destroyCommandPool(present_cmd_pool, nullptr);
    }
    device.waitIdle();
    device.destroy(nullptr);
    inst.destroySurfaceKHR(surface, nullptr);
    inst.destroy(nullptr);
}

void App::create_device() {
    float const priorities[1] = { 0.0 };

    vk::DeviceQueueCreateInfo queues[2];
    queues[0].setQueueFamilyIndex(graphics_queue_family_index);
    queues[0].setQueueCount(1);
    queues[0].setPQueuePriorities(priorities);

    auto deviceInfo = vk::DeviceCreateInfo()
        .setQueueCreateInfoCount(1)
        .setPQueueCreateInfos(queues)
        .setEnabledLayerCount(0)
        .setPpEnabledLayerNames(nullptr)
        .setEnabledExtensionCount(enabled_extension_count)
        .setPpEnabledExtensionNames((const char* const*)extension_names)
        .setPEnabledFeatures(nullptr);

    if (separate_present_queue) {
        queues[1].setQueueFamilyIndex(present_queue_family_index);
        queues[1].setQueueCount(1);
        queues[1].setPQueuePriorities(priorities);
        deviceInfo.setQueueCreateInfoCount(2);
    }

    auto result = gpu.createDevice(&deviceInfo, nullptr, &device);
    VERIFY(result == vk::Result::eSuccess);
}

void App::draw() {
    // Ensure no more than FRAME_LAG renderings are outstanding
    device.waitForFences(1, &fences[frame_index], VK_TRUE, UINT64_MAX);
    device.resetFences({ fences[frame_index] });

    vk::Result result;
    do {
        result = device.acquireNextImageKHR(swapchain, UINT64_MAX, image_acquired_semaphores[frame_index], vk::Fence(), &current_buffer);
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
            inst.destroySurfaceKHR(surface, nullptr);
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
        .setPWaitSemaphores(&image_acquired_semaphores[frame_index])
        .setCommandBufferCount(1)
        .setPCommandBuffers(&swapchain_image_resources[current_buffer].cmd)
        .setSignalSemaphoreCount(1)
        .setPSignalSemaphores(&draw_complete_semaphores[frame_index]);

    result = graphics_queue.submit(1, &submit_info, fences[frame_index]);
    VERIFY(result == vk::Result::eSuccess);

    if (separate_present_queue) {
        // If we are using separate queues, change image ownership to the
        // present queue before presenting, waiting for the draw complete
        // semaphore and signalling the ownership released semaphore when
        // finished
        auto const present_submit_info = vk::SubmitInfo()
            .setPWaitDstStageMask(&pipe_stage_flags)
            .setWaitSemaphoreCount(1)
            .setPWaitSemaphores(&draw_complete_semaphores[frame_index])
            .setCommandBufferCount(1)
            .setPCommandBuffers(&swapchain_image_resources[current_buffer].graphics_to_present_cmd)
            .setSignalSemaphoreCount(1)
            .setPSignalSemaphores(&image_ownership_semaphores[frame_index]);

        result = present_queue.submit(1, &present_submit_info, vk::Fence());
        VERIFY(result == vk::Result::eSuccess);
    }

    // If we are using separate queues we have to wait for image ownership,
    // otherwise wait for draw complete
    auto const presentInfo = vk::PresentInfoKHR()
        .setWaitSemaphoreCount(1)
        .setPWaitSemaphores(separate_present_queue ? &image_ownership_semaphores[frame_index] : &draw_complete_semaphores[frame_index])
        .setSwapchainCount(1)
        .setPSwapchains(&swapchain)
        .setPImageIndices(&current_buffer);

    result = present_queue.presentKHR(&presentInfo);
    frame_index += 1;
    frame_index %= FRAME_LAG;
    if (result == vk::Result::eErrorOutOfDateKHR) {
        // swapchain is out of date (e.g. the window was resized) and must be recreated:
        resize();
    }
    else if (result == vk::Result::eSuboptimalKHR) {
        // SUBOPTIMAL could be due to resize
        vk::SurfaceCapabilitiesKHR surfCapabilities;
        result = gpu.getSurfaceCapabilitiesKHR(surface, &surfCapabilities);
        VERIFY(result == vk::Result::eSuccess);
        if (surfCapabilities.currentExtent.width != static_cast<uint32_t>(window.width) || surfCapabilities.currentExtent.height != static_cast<uint32_t>(window.height)) {
            resize();
        }
    }
    else if (result == vk::Result::eErrorSurfaceLostKHR) {
        inst.destroySurfaceKHR(surface, nullptr);
        create_surface();
        resize();
    }
    else {
        VERIFY(result == vk::Result::eSuccess);
    }
}

void App::draw_build_cmd(vk::CommandBuffer commandBuffer) {
    auto const commandInfo = vk::CommandBufferBeginInfo().setFlags(vk::CommandBufferUsageFlagBits::eSimultaneousUse);

    vk::ClearValue const clearValues[2] = { vk::ClearColorValue(std::array<float, 4>({{0.2f, 0.2f, 0.2f, 0.2f}})), vk::ClearDepthStencilValue(1.0f, 0u) };

    auto const passInfo = vk::RenderPassBeginInfo()
        .setRenderPass(render_pass)
        .setFramebuffer(swapchain_image_resources[current_buffer].framebuffer)
        .setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D((uint32_t)window.width, (uint32_t)window.height)))
        .setClearValueCount(2)
        .setPClearValues(clearValues);

    auto result = commandBuffer.begin(&commandInfo);
    VERIFY(result == vk::Result::eSuccess);

    commandBuffer.beginRenderPass(&passInfo, vk::SubpassContents::eInline);
    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline_layout, 0, 1, &swapchain_image_resources[current_buffer].descriptor_set, 0, nullptr);
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

    if (separate_present_queue) {
        // We have to transfer ownership from the graphics queue family to the
        // present queue family to be able to present.  Note that we don't have
        // to transfer from present queue family back to graphics queue family at
        // the start of the next frame because we don't care about the image's
        // contents at that point.
        auto const image_ownership_barrier =
            vk::ImageMemoryBarrier()
            .setSrcAccessMask(vk::AccessFlags())
            .setDstAccessMask(vk::AccessFlags())
            .setOldLayout(vk::ImageLayout::ePresentSrcKHR)
            .setNewLayout(vk::ImageLayout::ePresentSrcKHR)
            .setSrcQueueFamilyIndex(graphics_queue_family_index)
            .setDstQueueFamilyIndex(present_queue_family_index)
            .setImage(swapchain_image_resources[current_buffer].image)
            .setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));

        commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eBottomOfPipe, vk::PipelineStageFlagBits::eBottomOfPipe, vk::DependencyFlagBits(), 0, nullptr, 0, nullptr, 1, &image_ownership_barrier);
    }

    result = commandBuffer.end();
    VERIFY(result == vk::Result::eSuccess);
}

void App::flush_init_cmd() {
    auto result = cmd.end();
    VERIFY(result == vk::Result::eSuccess);

    auto const fenceInfo = vk::FenceCreateInfo();
    vk::Fence fence;
    result = device.createFence(&fenceInfo, nullptr, &fence);
    VERIFY(result == vk::Result::eSuccess);

    vk::CommandBuffer const commandBuffers[] = { cmd };
    auto const submitInfo = vk::SubmitInfo().setCommandBufferCount(1).setPCommandBuffers(commandBuffers);

    result = graphics_queue.submit(1, &submitInfo, fence);
    VERIFY(result == vk::Result::eSuccess);

    result = device.waitForFences(1, &fence, VK_TRUE, UINT64_MAX);
    VERIFY(result == vk::Result::eSuccess);

    device.freeCommandBuffers(cmd_pool, 1, commandBuffers);
    device.destroyFence(fence, nullptr);

    cmd = vk::CommandBuffer();
}

void App::init() {
    LOG("init\n");

    init_vk();

    vec3 eye = { 0.0f, 3.0f, 5.0f };
    vec3 origin = { 0, 0, 0 };
    vec3 up = { 0.0f, 1.0f, 0.0 };

    mat4x4_perspective(projection_matrix, (float)degreesToRadians(45.0f), 1.0f, 0.1f, 100.0f);
    mat4x4_look_at(view_matrix, eye, origin, up);
    mat4x4_identity(model_matrix);

    projection_matrix[1][1] *= -1; // Flip projection matrix from GL to Vulkan orientation.
}

void App::init_vk() {

    uint32_t instance_extension_count = 0;
    uint32_t instance_layer_count = 0;
    char const* const instance_validation_layers[] = { "VK_LAYER_KHRONOS_validation" };
    enabled_extension_count = 0;
    enabled_layer_count = 0;

    // Look for validation layers
    vk::Bool32 validation_found = VK_FALSE;
    if (true) {
        auto result = vk::enumerateInstanceLayerProperties(&instance_layer_count, static_cast<vk::LayerProperties*>(nullptr));
        VERIFY(result == vk::Result::eSuccess);

        if (instance_layer_count > 0) {
            std::unique_ptr<vk::LayerProperties[]> instance_layers(new vk::LayerProperties[instance_layer_count]);
            result = vk::enumerateInstanceLayerProperties(&instance_layer_count, instance_layers.get());
            VERIFY(result == vk::Result::eSuccess);

            validation_found = check_layers(ARRAY_SIZE(instance_validation_layers), instance_validation_layers, instance_layer_count, instance_layers.get());
            if (validation_found) {
                enabled_layer_count = ARRAY_SIZE(instance_validation_layers);
                enabled_layers[0] = "VK_LAYER_KHRONOS_validation";
            }
        }

        if (!validation_found) {
            ERR_EXIT(
                "vkEnumerateInstanceLayerProperties failed to find required validation layer.\n\n"
                "Please look at the Getting Started guide for additional information.\n",
                "vkCreateInstance Failure");
        }
    }

    // Look for instance extensions
    vk::Bool32 surfaceExtFound = VK_FALSE;
    vk::Bool32 platformSurfaceExtFound = VK_FALSE;
    memset(extension_names, 0, sizeof(extension_names));

    auto result = vk::enumerateInstanceExtensionProperties(nullptr, &instance_extension_count, static_cast<vk::ExtensionProperties*>(nullptr));
    VERIFY(result == vk::Result::eSuccess);

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
            assert(enabled_extension_count < 64);
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
    auto const app = vk::ApplicationInfo()
        .setPApplicationName(APP_SHORT_NAME)
        .setApplicationVersion(0)
        .setPEngineName(APP_SHORT_NAME)
        .setEngineVersion(0)
        .setApiVersion(VK_API_VERSION_1_0);
    auto const inst_info = vk::InstanceCreateInfo()
        .setPApplicationInfo(&app)
        .setEnabledLayerCount(enabled_layer_count)
        .setPpEnabledLayerNames(instance_validation_layers)
        .setEnabledExtensionCount(enabled_extension_count)
        .setPpEnabledExtensionNames(extension_names);

    result = vk::createInstance(&inst_info, nullptr, &inst);
    if (result == vk::Result::eErrorIncompatibleDriver) {
        ERR_EXIT(
            "Cannot find a compatible Vulkan installable client driver (ICD).\n\n"
            "Please look at the Getting Started guide for additional information.\n",
            "vkCreateInstance Failure");
    }
    else if (result == vk::Result::eErrorExtensionNotPresent) {
        ERR_EXIT(
            "Cannot find a specified extension library.\n"
            "Make sure your layers path is set appropriately.\n",
            "vkCreateInstance Failure");
    }
    else if (result != vk::Result::eSuccess) {
        ERR_EXIT(
            "vkCreateInstance failed.\n\n"
            "Do you have a compatible Vulkan installable client driver (ICD) installed?\n"
            "Please look at the Getting Started guide for additional information.\n",
            "vkCreateInstance Failure");
    }

    // Make initial call to query gpu_count, then second call for gpu info
    uint32_t gpu_count = 0;
    result = inst.enumeratePhysicalDevices(&gpu_count, static_cast<vk::PhysicalDevice*>(nullptr));
    VERIFY(result == vk::Result::eSuccess);

    if (gpu_count <= 0) {
        ERR_EXIT(
            "vkEnumeratePhysicalDevices reported zero accessible devices.\n\n"
            "Do you have a compatible Vulkan installable client driver (ICD) installed?\n"
            "Please look at the Getting Started guide for additional information.\n",
            "vkEnumeratePhysicalDevices Failure");
    }

    std::unique_ptr<vk::PhysicalDevice[]> physical_devices(new vk::PhysicalDevice[gpu_count]);
    result = inst.enumeratePhysicalDevices(&gpu_count, physical_devices.get());
    VERIFY(result == vk::Result::eSuccess);

    if (gpu_number >= 0 && !((uint32_t)gpu_number < gpu_count)) {
        fprintf(stderr, "GPU %d specified is not present, GPU count = %u\n", gpu_number, gpu_count);
        ERR_EXIT("Specified GPU number is not present", "User Error");
    }
    // Try to auto select most suitable device
    if (gpu_number == -1) {
        uint32_t count_device_type[VK_PHYSICAL_DEVICE_TYPE_CPU + 1];
        memset(count_device_type, 0, sizeof(count_device_type));

        for (uint32_t i = 0; i < gpu_count; i++) {
            const auto physicalDeviceProperties = physical_devices[i].getProperties();
            assert(static_cast<int>(physicalDeviceProperties.deviceType) <= VK_PHYSICAL_DEVICE_TYPE_CPU);
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
    assert(gpu_number >= 0);
    gpu = physical_devices[gpu_number];
    {
        auto physicalDeviceProperties = gpu.getProperties();
        fprintf(stderr, "Selected GPU %d: %s, type: %s\n", gpu_number, physicalDeviceProperties.deviceName.data(), to_string(physicalDeviceProperties.deviceType).c_str());
    }
    physical_devices.reset();

    // Look for device extensions
    uint32_t device_extension_count = 0;
    vk::Bool32 swapchainExtFound = VK_FALSE;
    enabled_extension_count = 0;
    memset(extension_names, 0, sizeof(extension_names));

    result = gpu.enumerateDeviceExtensionProperties(nullptr, &device_extension_count, static_cast<vk::ExtensionProperties*>(nullptr));
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
            assert(enabled_extension_count < 64);
        }
    }

    if (!swapchainExtFound) {
        ERR_EXIT("vkEnumerateDeviceExtensionProperties failed to find the " VK_KHR_SWAPCHAIN_EXTENSION_NAME
            " extension.\n\n"
            "Do you have a compatible Vulkan installable client driver (ICD) installed?\n"
            "Please look at the Getting Started guide for additional information.\n",
            "vkCreateInstance Failure");
    }

    gpu.getProperties(&gpu_props);

    // Call with nullptr data to get count
    gpu.getQueueFamilyProperties(&queue_family_count, static_cast<vk::QueueFamilyProperties*>(nullptr));
    assert(queue_family_count >= 1);

    queue_props.reset(new vk::QueueFamilyProperties[queue_family_count]);
    gpu.getQueueFamilyProperties(&queue_family_count, queue_props.get());

    // Query fine-grained feature support for this device.
    //  If app has specific feature requirements it should check supported features based on this query
    vk::PhysicalDeviceFeatures physDevFeatures;
    gpu.getFeatures(&physDevFeatures);
}

void App::create_surface() {
    auto const createInfo = vk::Win32SurfaceCreateInfoKHR().setHinstance(window.hinstance).setHwnd(window.hwnd);

    auto result = inst.createWin32SurfaceKHR(&createInfo, nullptr, &surface);
    VERIFY(result == vk::Result::eSuccess);
}

void App::init_vk_swapchain() {
    create_surface();
    // Iterate over each queue to learn whether it supports presenting:
    std::unique_ptr<vk::Bool32[]> supportsPresent(new vk::Bool32[queue_family_count]);
    for (uint32_t i = 0; i < queue_family_count; i++) {
        gpu.getSurfaceSupportKHR(i, surface, &supportsPresent[i]);
    }

    uint32_t graphicsQueueFamilyIndex = UINT32_MAX;
    uint32_t presentQueueFamilyIndex = UINT32_MAX;
    for (uint32_t i = 0; i < queue_family_count; i++) {
        if (queue_props[i].queueFlags & vk::QueueFlagBits::eGraphics) {
            if (graphicsQueueFamilyIndex == UINT32_MAX) {
                graphicsQueueFamilyIndex = i;
            }

            if (supportsPresent[i] == VK_TRUE) {
                graphicsQueueFamilyIndex = i;
                presentQueueFamilyIndex = i;
                break;
            }
        }
    }

    if (presentQueueFamilyIndex == UINT32_MAX) {
        // If didn't find a queue that supports both graphics and present, then
        // find a separate present queue.
        for (uint32_t i = 0; i < queue_family_count; ++i) {
            if (supportsPresent[i] == VK_TRUE) {
                presentQueueFamilyIndex = i;
                break;
            }
        }
    }

    // Generate error if could not find both a graphics and a present queue
    if (graphicsQueueFamilyIndex == UINT32_MAX || presentQueueFamilyIndex == UINT32_MAX) {
        ERR_EXIT("Could not find both graphics and present queues\n", "Swapchain Initialization Failure");
    }

    graphics_queue_family_index = graphicsQueueFamilyIndex;
    present_queue_family_index = presentQueueFamilyIndex;
    separate_present_queue = (graphics_queue_family_index != present_queue_family_index);

    create_device();

    device.getQueue(graphics_queue_family_index, 0, &graphics_queue);
    if (!separate_present_queue) {
        present_queue = graphics_queue;
    }
    else {
        device.getQueue(present_queue_family_index, 0, &present_queue);
    }

    // Get the list of VkFormat's that are supported:
    uint32_t formatCount;
    auto result = gpu.getSurfaceFormatsKHR(surface, &formatCount, static_cast<vk::SurfaceFormatKHR*>(nullptr));
    VERIFY(result == vk::Result::eSuccess);

    std::unique_ptr<vk::SurfaceFormatKHR[]> surfFormats(new vk::SurfaceFormatKHR[formatCount]);
    result = gpu.getSurfaceFormatsKHR(surface, &formatCount, surfFormats.get());
    VERIFY(result == vk::Result::eSuccess);

    // If the format list includes just one entry of VK_FORMAT_UNDEFINED,
    // the surface has no preferred format.  Otherwise, at least one
    // supported format will be returned.
    if (formatCount == 1 && surfFormats[0].format == vk::Format::eUndefined) {
        format = vk::Format::eB8G8R8A8Unorm;
    }
    else {
        assert(formatCount >= 1);
        format = surfFormats[0].format;
    }
    color_space = surfFormats[0].colorSpace;

    curFrame = 0;

    // Create semaphores to synchronize acquiring presentable buffers before
    // rendering and waiting for drawing to be complete before presenting
    auto const semaphoreCreateInfo = vk::SemaphoreCreateInfo();

    // Create fences that we can use to throttle if we get too far ahead of the image presents
    auto const fence_ci = vk::FenceCreateInfo().setFlags(vk::FenceCreateFlagBits::eSignaled);
    for (uint32_t i = 0; i < FRAME_LAG; i++) {
        result = device.createFence(&fence_ci, nullptr, &fences[i]);
        VERIFY(result == vk::Result::eSuccess);

        result = device.createSemaphore(&semaphoreCreateInfo, nullptr, &image_acquired_semaphores[i]);
        VERIFY(result == vk::Result::eSuccess);

        result = device.createSemaphore(&semaphoreCreateInfo, nullptr, &draw_complete_semaphores[i]);
        VERIFY(result == vk::Result::eSuccess);

        if (separate_present_queue) {
            result = device.createSemaphore(&semaphoreCreateInfo, nullptr, &image_ownership_semaphores[i]);
            VERIFY(result == vk::Result::eSuccess);
        }
    }
    frame_index = 0;

    gpu.getMemoryProperties(&memory_properties);
}

void App::prepare() {
    LOG("prepare\n");

    auto const cmd_pool_info = vk::CommandPoolCreateInfo().setQueueFamilyIndex(graphics_queue_family_index);
    auto result = device.createCommandPool(&cmd_pool_info, nullptr, &cmd_pool);
    VERIFY(result == vk::Result::eSuccess);

    auto const cmd = vk::CommandBufferAllocateInfo()
        .setCommandPool(cmd_pool)
        .setLevel(vk::CommandBufferLevel::ePrimary)
        .setCommandBufferCount(1);

    result = device.allocateCommandBuffers(&cmd, &this->cmd);
    VERIFY(result == vk::Result::eSuccess);

    auto const cmd_buf_info = vk::CommandBufferBeginInfo().setPInheritanceInfo(nullptr);

    result = this->cmd.begin(&cmd_buf_info);
    VERIFY(result == vk::Result::eSuccess);

    prepare_buffers();
    prepare_depth();
    prepare_uniforms();

    prepare_descriptor_layout();
    prepare_render_pass();
    prepare_pipeline();

    for (uint32_t i = 0; i < swapchainImageCount; ++i) {
        result = device.allocateCommandBuffers(&cmd, &swapchain_image_resources[i].cmd);
        VERIFY(result == vk::Result::eSuccess);
    }

    if (separate_present_queue) {
        auto const present_cmd_pool_info = vk::CommandPoolCreateInfo().setQueueFamilyIndex(present_queue_family_index);

        result = device.createCommandPool(&present_cmd_pool_info, nullptr, &present_cmd_pool);
        VERIFY(result == vk::Result::eSuccess);

        auto const present_cmd = vk::CommandBufferAllocateInfo()
            .setCommandPool(present_cmd_pool)
            .setLevel(vk::CommandBufferLevel::ePrimary)
            .setCommandBufferCount(1);

        for (uint32_t i = 0; i < swapchainImageCount; i++) {
            result = device.allocateCommandBuffers(&present_cmd, &swapchain_image_resources[i].graphics_to_present_cmd);
            VERIFY(result == vk::Result::eSuccess);

            build_image_ownership_cmd(i);
        }
    }

    prepare_descriptor_pool();
    prepare_descriptor_set();

    prepare_framebuffers();

    for (uint32_t i = 0; i < swapchainImageCount; ++i) {
        current_buffer = i;
        draw_build_cmd(swapchain_image_resources[i].cmd);
    }

    flush_init_cmd();

    current_buffer = 0;
    prepared = true;
}

void App::prepare_buffers() {
    vk::SwapchainKHR oldSwapchain = swapchain;

    vk::SurfaceCapabilitiesKHR surfCapabilities;
    auto result = gpu.getSurfaceCapabilitiesKHR(surface, &surfCapabilities);
    VERIFY(result == vk::Result::eSuccess);

    uint32_t presentModeCount;
    result = gpu.getSurfacePresentModesKHR(surface, &presentModeCount, static_cast<vk::PresentModeKHR*>(nullptr));
    VERIFY(result == vk::Result::eSuccess);

    std::unique_ptr<vk::PresentModeKHR[]> presentModes(new vk::PresentModeKHR[presentModeCount]);
    result = gpu.getSurfacePresentModesKHR(surface, &presentModeCount, presentModes.get());
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

    if (presentMode != swapchainPresentMode) {
        for (size_t i = 0; i < presentModeCount; ++i) {
            if (presentModes[i] == presentMode) {
                swapchainPresentMode = presentMode;
                break;
            }
        }
    }

    if (swapchainPresentMode != presentMode) {
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

    auto const swapchain_ci = vk::SwapchainCreateInfoKHR()
        .setSurface(surface)
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
        .setOldSwapchain(oldSwapchain);

    result = device.createSwapchainKHR(&swapchain_ci, nullptr, &swapchain);
    VERIFY(result == vk::Result::eSuccess);

    // If we just re-created an existing swapchain, we should destroy the old swapchain at this point.
    // Note: destroying the swapchain also cleans up all its associated presentable images once the platform is done with them.
    if (oldSwapchain) {
        device.destroySwapchainKHR(oldSwapchain, nullptr);
    }

    result = device.getSwapchainImagesKHR(swapchain, &swapchainImageCount, static_cast<vk::Image*>(nullptr));
    VERIFY(result == vk::Result::eSuccess);

    std::unique_ptr<vk::Image[]> swapchainImages(new vk::Image[swapchainImageCount]);
    result = device.getSwapchainImagesKHR(swapchain, &swapchainImageCount, swapchainImages.get());
    VERIFY(result == vk::Result::eSuccess);

    swapchain_image_resources.reset(new SwapchainImageResources[swapchainImageCount]);

    for (uint32_t i = 0; i < swapchainImageCount; ++i) {
        auto color_image_view = vk::ImageViewCreateInfo()
            .setViewType(vk::ImageViewType::e2D)
            .setFormat(format)
            .setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));

        swapchain_image_resources[i].image = swapchainImages[i];

        color_image_view.image = swapchain_image_resources[i].image;

        result = device.createImageView(&color_image_view, nullptr, &swapchain_image_resources[i].view);
        VERIFY(result == vk::Result::eSuccess);
    }
}

void App::prepare_uniforms() {
    mat4x4 VP;
    mat4x4_mul(VP, projection_matrix, view_matrix);

    mat4x4 MVP;
    mat4x4_mul(MVP, VP, model_matrix);

    Uniforms data;
    memcpy(data.mvp, MVP, sizeof(MVP));

    for (int32_t i = 0; i < 12 * 3; i++) {
        data.position[i][0] = g_vertex_buffer_data[i * 3];
        data.position[i][1] = g_vertex_buffer_data[i * 3 + 1];
        data.position[i][2] = g_vertex_buffer_data[i * 3 + 2];
        data.position[i][3] = 1.0f;
    }

    auto const buf_info = vk::BufferCreateInfo().setSize(sizeof(data)).setUsage(vk::BufferUsageFlagBits::eUniformBuffer);

    for (unsigned int i = 0; i < swapchainImageCount; i++) {
        auto result = device.createBuffer(&buf_info, nullptr, &swapchain_image_resources[i].uniform_buffer);
        VERIFY(result == vk::Result::eSuccess);

        vk::MemoryRequirements mem_reqs;
        device.getBufferMemoryRequirements(swapchain_image_resources[i].uniform_buffer, &mem_reqs);

        auto mem_alloc = vk::MemoryAllocateInfo().setAllocationSize(mem_reqs.size).setMemoryTypeIndex(0);

        bool const pass = memory_type_from_properties( mem_reqs.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, &mem_alloc.memoryTypeIndex);
        VERIFY(pass);

        result = device.allocateMemory(&mem_alloc, nullptr, &swapchain_image_resources[i].uniform_memory);
        VERIFY(result == vk::Result::eSuccess);

        result = device.mapMemory(swapchain_image_resources[i].uniform_memory, 0, VK_WHOLE_SIZE, vk::MemoryMapFlags(), &swapchain_image_resources[i].uniform_memory_ptr);
        VERIFY(result == vk::Result::eSuccess);

        memcpy(swapchain_image_resources[i].uniform_memory_ptr, &data, sizeof data);

        result = device.bindBufferMemory(swapchain_image_resources[i].uniform_buffer, swapchain_image_resources[i].uniform_memory, 0);
        VERIFY(result == vk::Result::eSuccess);
    }
}

void App::prepare_depth() {
    depth.format = vk::Format::eD16Unorm;

    auto const image = vk::ImageCreateInfo()
        .setImageType(vk::ImageType::e2D)
        .setFormat(depth.format)
        .setExtent({ (uint32_t)window.width, (uint32_t)window.height, 1 })
        .setMipLevels(1)
        .setArrayLayers(1)
        .setSamples(vk::SampleCountFlagBits::e1)
        .setTiling(vk::ImageTiling::eOptimal)
        .setUsage(vk::ImageUsageFlagBits::eDepthStencilAttachment)
        .setSharingMode(vk::SharingMode::eExclusive)
        .setQueueFamilyIndexCount(0)
        .setPQueueFamilyIndices(nullptr)
        .setInitialLayout(vk::ImageLayout::eUndefined);

    auto result = device.createImage(&image, nullptr, &depth.image);
    VERIFY(result == vk::Result::eSuccess);

    vk::MemoryRequirements mem_reqs;
    device.getImageMemoryRequirements(depth.image, &mem_reqs);

    depth.mem_alloc.setAllocationSize(mem_reqs.size);
    depth.mem_alloc.setMemoryTypeIndex(0);

    auto const pass = memory_type_from_properties(mem_reqs.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal, &depth.mem_alloc.memoryTypeIndex);
    VERIFY(pass);

    result = device.allocateMemory(&depth.mem_alloc, nullptr, &depth.mem);
    VERIFY(result == vk::Result::eSuccess);

    result = device.bindImageMemory(depth.image, depth.mem, 0);
    VERIFY(result == vk::Result::eSuccess);

    auto const view = vk::ImageViewCreateInfo()
        .setImage(depth.image)
        .setViewType(vk::ImageViewType::e2D)
        .setFormat(depth.format)
        .setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1));
    result = device.createImageView(&view, nullptr, &depth.view);
    VERIFY(result == vk::Result::eSuccess);
}

void App::prepare_descriptor_layout() {
    vk::DescriptorSetLayoutBinding const layout_bindings[1] = { 
        vk::DescriptorSetLayoutBinding()
           .setBinding(0)
           .setDescriptorType(vk::DescriptorType::eUniformBuffer)
           .setDescriptorCount(1)
           .setStageFlags(vk::ShaderStageFlagBits::eVertex)
           .setPImmutableSamplers(nullptr) };

    auto const descriptor_layout = vk::DescriptorSetLayoutCreateInfo().setBindingCount(1).setPBindings(layout_bindings);

    auto result = device.createDescriptorSetLayout(&descriptor_layout, nullptr, &desc_layout);
    VERIFY(result == vk::Result::eSuccess);

    auto const pPipelineLayoutCreateInfo = vk::PipelineLayoutCreateInfo().setSetLayoutCount(1).setPSetLayouts(&desc_layout);

    result = device.createPipelineLayout(&pPipelineLayoutCreateInfo, nullptr, &pipeline_layout);
    VERIFY(result == vk::Result::eSuccess);
}

void App::prepare_descriptor_pool() {
    vk::DescriptorPoolSize const poolSizes[1] = { vk::DescriptorPoolSize().setType(vk::DescriptorType::eUniformBuffer).setDescriptorCount(swapchainImageCount) };

    auto const descriptor_pool = vk::DescriptorPoolCreateInfo().setMaxSets(swapchainImageCount).setPoolSizeCount(1).setPPoolSizes(poolSizes);

    auto result = device.createDescriptorPool(&descriptor_pool, nullptr, &desc_pool);
    VERIFY(result == vk::Result::eSuccess);
}

void App::prepare_descriptor_set() {
    auto const alloc_info = vk::DescriptorSetAllocateInfo().setDescriptorPool(desc_pool).setDescriptorSetCount(1).setPSetLayouts(&desc_layout);

    auto buffer_info = vk::DescriptorBufferInfo().setOffset(0).setRange(sizeof(struct Uniforms));

    vk::WriteDescriptorSet writes[1];

    writes[0].setDescriptorCount(1);
    writes[0].setDescriptorType(vk::DescriptorType::eUniformBuffer);
    writes[0].setPBufferInfo(&buffer_info);

    for (unsigned int i = 0; i < swapchainImageCount; i++) {
        auto result = device.allocateDescriptorSets(&alloc_info, &swapchain_image_resources[i].descriptor_set);
        VERIFY(result == vk::Result::eSuccess);

        buffer_info.setBuffer(swapchain_image_resources[i].uniform_buffer);
        writes[0].setDstSet(swapchain_image_resources[i].descriptor_set);
        device.updateDescriptorSets(1, writes, 0, nullptr);
    }
}

void App::prepare_framebuffers() {
    vk::ImageView attachments[2];
    attachments[1] = depth.view;

    auto const fb_info = vk::FramebufferCreateInfo()
        .setRenderPass(render_pass)
        .setAttachmentCount(2)
        .setPAttachments(attachments)
        .setWidth((uint32_t)window.width)
        .setHeight((uint32_t)window.height)
        .setLayers(1);

    for (uint32_t i = 0; i < swapchainImageCount; i++) {
        attachments[0] = swapchain_image_resources[i].view;
        auto const result = device.createFramebuffer(&fb_info, nullptr, &swapchain_image_resources[i].framebuffer);
        VERIFY(result == vk::Result::eSuccess);
    }
}

vk::ShaderModule App::prepare_fs() {
    const uint32_t fragShaderCode[] =
#include "shader.frag.inc"
        ;
    return prepare_shader_module(fragShaderCode, sizeof(fragShaderCode));
}

void App::prepare_pipeline() {
    vk::PipelineCacheCreateInfo const pipelineCacheInfo;
    auto result = device.createPipelineCache(&pipelineCacheInfo, nullptr, &pipelineCache);
    VERIFY(result == vk::Result::eSuccess);

    vk::ShaderModule vert_shader_module = prepare_vs();
    vk::ShaderModule frag_shader_module = prepare_fs();

    vk::PipelineShaderStageCreateInfo const shaderStageInfo[2] = {
        vk::PipelineShaderStageCreateInfo().setStage(vk::ShaderStageFlagBits::eVertex).setModule(vert_shader_module).setPName("main"),
        vk::PipelineShaderStageCreateInfo().setStage(vk::ShaderStageFlagBits::eFragment).setModule(frag_shader_module).setPName("main") };

    vk::PipelineVertexInputStateCreateInfo const vertexInputInfo;

    auto const inputAssemblyInfo = vk::PipelineInputAssemblyStateCreateInfo()
        .setTopology(vk::PrimitiveTopology::eTriangleList);

    auto const viewportInfo = vk::PipelineViewportStateCreateInfo()
        .setViewportCount(1)
        .setScissorCount(1);

    auto const rasterizationInfo = vk::PipelineRasterizationStateCreateInfo()
        .setDepthClampEnable(VK_FALSE)
        .setRasterizerDiscardEnable(VK_FALSE)
        .setPolygonMode(vk::PolygonMode::eFill)
        .setCullMode(vk::CullModeFlagBits::eBack)
        .setFrontFace(vk::FrontFace::eCounterClockwise)
        .setDepthBiasEnable(VK_FALSE)
        .setLineWidth(1.0f);

    auto const multisampleInfo = vk::PipelineMultisampleStateCreateInfo();

    auto const stencilOp = vk::StencilOpState()
        .setFailOp(vk::StencilOp::eKeep)
        .setPassOp(vk::StencilOp::eKeep)
        .setCompareOp(vk::CompareOp::eAlways);

    auto const depthStencilInfo = vk::PipelineDepthStencilStateCreateInfo()
        .setDepthTestEnable(VK_TRUE)
        .setDepthWriteEnable(VK_TRUE)
        .setDepthCompareOp(vk::CompareOp::eLessOrEqual)
        .setDepthBoundsTestEnable(VK_FALSE)
        .setStencilTestEnable(VK_FALSE)
        .setFront(stencilOp)
        .setBack(stencilOp);

    vk::PipelineColorBlendAttachmentState const colorBlendAttachments[1] = {
        vk::PipelineColorBlendAttachmentState()
            .setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA) };

    auto const colorBlendInfo = vk::PipelineColorBlendStateCreateInfo()
        .setAttachmentCount(1)
        .setPAttachments(colorBlendAttachments);

    vk::DynamicState const dynamicStates[2] = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };

    auto const dynamicStateInfo = vk::PipelineDynamicStateCreateInfo()
        .setPDynamicStates(dynamicStates)
        .setDynamicStateCount(2);

    auto const pipeline = vk::GraphicsPipelineCreateInfo()
        .setStageCount(2)
        .setPStages(shaderStageInfo)
        .setPVertexInputState(&vertexInputInfo)
        .setPInputAssemblyState(&inputAssemblyInfo)
        .setPViewportState(&viewportInfo)
        .setPRasterizationState(&rasterizationInfo)
        .setPMultisampleState(&multisampleInfo)
        .setPDepthStencilState(&depthStencilInfo)
        .setPColorBlendState(&colorBlendInfo)
        .setPDynamicState(&dynamicStateInfo)
        .setLayout(pipeline_layout)
        .setRenderPass(render_pass);

    result = device.createGraphicsPipelines(pipelineCache, 1, &pipeline, nullptr, &this->pipeline);
    VERIFY(result == vk::Result::eSuccess);

    device.destroyShaderModule(frag_shader_module, nullptr);
    device.destroyShaderModule(vert_shader_module, nullptr);
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
    const vk::AttachmentDescription attachments[2] = {
        vk::AttachmentDescription()
          .setFormat(format)
          .setSamples(vk::SampleCountFlagBits::e1)
          .setLoadOp(vk::AttachmentLoadOp::eClear)
          .setStoreOp(vk::AttachmentStoreOp::eStore)
          .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
          .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
          .setInitialLayout(vk::ImageLayout::eUndefined)
          .setFinalLayout(vk::ImageLayout::ePresentSrcKHR),
      vk::AttachmentDescription()
          .setFormat(depth.format)
          .setSamples(vk::SampleCountFlagBits::e1)
          .setLoadOp(vk::AttachmentLoadOp::eClear)
          .setStoreOp(vk::AttachmentStoreOp::eDontCare)
          .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
          .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
          .setInitialLayout(vk::ImageLayout::eUndefined)
          .setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal) };

    auto const color_reference = vk::AttachmentReference().setAttachment(0).setLayout(vk::ImageLayout::eColorAttachmentOptimal);

    auto const depth_reference = vk::AttachmentReference()
        .setAttachment(1)
        .setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

    auto const subpass = vk::SubpassDescription()
        .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
        .setInputAttachmentCount(0)
        .setPInputAttachments(nullptr)
        .setColorAttachmentCount(1)
        .setPColorAttachments(&color_reference)
        .setPResolveAttachments(nullptr)
        .setPDepthStencilAttachment(&depth_reference)
        .setPreserveAttachmentCount(0)
        .setPPreserveAttachments(nullptr);

    vk::PipelineStageFlags stages = vk::PipelineStageFlagBits::eEarlyFragmentTests | vk::PipelineStageFlagBits::eLateFragmentTests;
    vk::SubpassDependency const dependencies[2] = {
        vk::SubpassDependency()  // Depth buffer is shared between swapchain images
            .setSrcSubpass(VK_SUBPASS_EXTERNAL)
            .setDstSubpass(0)
            .setSrcStageMask(stages)
            .setDstStageMask(stages)
            .setSrcAccessMask(vk::AccessFlagBits::eDepthStencilAttachmentWrite)
            .setDstAccessMask(vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite)
            .setDependencyFlags(vk::DependencyFlags()),
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
        .setAttachmentCount(2)
        .setPAttachments(attachments)
        .setSubpassCount(1)
        .setPSubpasses(&subpass)
        .setDependencyCount(2)
        .setPDependencies(dependencies);

    auto result = device.createRenderPass(&rp_info, nullptr, &render_pass);
    VERIFY(result == vk::Result::eSuccess);
}

vk::ShaderModule App::prepare_shader_module(const uint32_t* code, size_t size) {
    const auto moduleCreateInfo = vk::ShaderModuleCreateInfo()
        .setCodeSize(size)
        .setPCode(code);

    vk::ShaderModule module;
    auto result = device.createShaderModule(&moduleCreateInfo, nullptr, &module);
    VERIFY(result == vk::Result::eSuccess);

    return module;
}

vk::ShaderModule App::prepare_vs() {
    const uint32_t vertShaderCode[] =
#include "shader.vert.inc"
        ;
    return prepare_shader_module(vertShaderCode, sizeof(vertShaderCode));
}

void App::resize() {
    uint32_t i;

    // Don't react to resize until after first initialization.
    if (!prepared) {
        return;
    }

    // In order to properly resize the window, we must re-create the swapchain
    // AND redo the command buffers, etc.
    //
    // First, perform part of the cleanup() function:
    prepared = false;
    auto result = device.waitIdle();
    VERIFY(result == vk::Result::eSuccess);

    for (i = 0; i < swapchainImageCount; i++) {
        device.destroyFramebuffer(swapchain_image_resources[i].framebuffer, nullptr);
    }

    device.destroyDescriptorPool(desc_pool, nullptr);

    device.destroyPipeline(pipeline, nullptr);
    device.destroyPipelineCache(pipelineCache, nullptr);
    device.destroyRenderPass(render_pass, nullptr);
    device.destroyPipelineLayout(pipeline_layout, nullptr);
    device.destroyDescriptorSetLayout(desc_layout, nullptr);

    device.destroyImageView(depth.view, nullptr);
    device.destroyImage(depth.image, nullptr);
    device.freeMemory(depth.mem, nullptr);

    for (i = 0; i < swapchainImageCount; i++) {
        device.destroyImageView(swapchain_image_resources[i].view, nullptr);
        device.freeCommandBuffers(cmd_pool, { swapchain_image_resources[i].cmd });
        device.destroyBuffer(swapchain_image_resources[i].uniform_buffer, nullptr);
        device.unmapMemory(swapchain_image_resources[i].uniform_memory);
        device.freeMemory(swapchain_image_resources[i].uniform_memory, nullptr);
    }

    device.destroyCommandPool(cmd_pool, nullptr);
    if (separate_present_queue) {
        device.destroyCommandPool(present_cmd_pool, nullptr);
    }

    // Second, re-perform the prepare() function, which will re-create the
    // swapchain.
    prepare();
}

void App::set_image_layout(vk::Image image, vk::ImageAspectFlags aspectMask, vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
    vk::AccessFlags srcAccessMask, vk::PipelineStageFlags src_stages, vk::PipelineStageFlags dest_stages) {
    assert(cmd);

    auto DstAccessMask = [](vk::ImageLayout const& layout) {
        vk::AccessFlags flags;

        switch (layout) {
        case vk::ImageLayout::eTransferDstOptimal: flags = vk::AccessFlagBits::eTransferWrite; break;
        case vk::ImageLayout::eColorAttachmentOptimal: flags = vk::AccessFlagBits::eColorAttachmentWrite; break;
        case vk::ImageLayout::eDepthStencilAttachmentOptimal: flags = vk::AccessFlagBits::eDepthStencilAttachmentWrite; break;
        case vk::ImageLayout::eShaderReadOnlyOptimal: flags = vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eInputAttachmentRead; break;
        case vk::ImageLayout::eTransferSrcOptimal: flags = vk::AccessFlagBits::eTransferRead; break;
        case vk::ImageLayout::ePresentSrcKHR: flags = vk::AccessFlagBits::eMemoryRead; break;
        default: break;
        }

        return flags;
    };

    auto const barrier = vk::ImageMemoryBarrier()
        .setSrcAccessMask(srcAccessMask)
        .setDstAccessMask(DstAccessMask(newLayout))
        .setOldLayout(oldLayout)
        .setNewLayout(newLayout)
        .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
        .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
        .setImage(image)
        .setSubresourceRange(vk::ImageSubresourceRange(aspectMask, 0, 1, 0, 1));

    cmd.pipelineBarrier(src_stages, dest_stages, vk::DependencyFlagBits(), 0, nullptr, 0, nullptr, 1, &barrier);
}

void App::update_data_buffer() {
    mat4x4 VP;
    mat4x4_mul(VP, projection_matrix, view_matrix);

    // Rotate around the Y axis
    mat4x4 Model;
    mat4x4_dup(Model, model_matrix);
    mat4x4_rotate_Y(model_matrix, Model, (float)degreesToRadians(0.1f));
    mat4x4_orthonormalize(model_matrix, model_matrix);

    mat4x4 MVP;
    mat4x4_mul(MVP, VP, model_matrix);

    memcpy(swapchain_image_resources[current_buffer].uniform_memory_ptr, (const void*)&MVP[0][0], sizeof(MVP));
}

bool App::memory_type_from_properties(uint32_t typeBits, vk::MemoryPropertyFlags requirements_mask, uint32_t* typeIndex) {
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
    if (!prepared) {
        return;
    }

    draw();
    curFrame++;

    if (frameCount != UINT32_MAX && curFrame == frameCount) {
        PostQuitMessage(validation_error);
    }
}

void App::create_window() {
    WNDCLASSEX win_class;

    win_class.cbSize = sizeof(WNDCLASSEX);
    win_class.style = CS_HREDRAW | CS_VREDRAW;
    win_class.lpfnWndProc = WndProc;
    win_class.cbClsExtra = 0;
    win_class.cbWndExtra = 0;
    win_class.hInstance = window.hinstance;
    win_class.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    win_class.hCursor = LoadCursor(nullptr, IDC_ARROW);
    win_class.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    win_class.lpszMenuName = nullptr;
    win_class.lpszClassName = window.name;
    win_class.hIconSm = LoadIcon(nullptr, IDI_WINLOGO);

    if (!RegisterClassEx(&win_class)) {
        printf("Unexpected error trying to start the application!\n");
        fflush(stdout);
        exit(1);
    }

    RECT wr = { 0, 0, static_cast<LONG>(window.width), static_cast<LONG>(window.height) };
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);
    window.hwnd = CreateWindowEx(0,
        window.name,                  // class name
        window.name,                  // app name
        WS_OVERLAPPEDWINDOW |  // window style
        WS_VISIBLE | WS_SYSMENU,
        100, 100,            // x/y coords
        wr.right - wr.left,  // width
        wr.bottom - wr.top,  // height
        nullptr,             // handle to parent
        nullptr,             // handle to menu
        window.hinstance,          // hInstance
        nullptr);            // no extra parameters

    if (!window.hwnd) {
        // It didn't work, so try to give a useful error:
        printf("Cannot create a window in which to draw!\n");
        fflush(stdout);
        exit(1);
    }

    // Window client area size must be at least 1 pixel high, to prevent crash.
    window.minsize.x = GetSystemMetrics(SM_CXMINTRACK);
    window.minsize.y = GetSystemMetrics(SM_CYMINTRACK) + 1;
}

App app;

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CLOSE:
        PostQuitMessage(validation_error);
        break;
    case WM_PAINT:
        app.run();
        break;
    case WM_GETMINMAXINFO:  // set window's minimum size
        ((MINMAXINFO*)lParam)->ptMinTrackSize = app.window.minsize;
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_SIZE:
        // Resize the application to the new window size, except when
        // it was minimized. Vulkan doesn't support images or swapchains
        // with width=0 and height=0.
        if (wParam != SIZE_MINIMIZED) {
            app.window.width = lParam & 0xffff;
            app.window.height = (lParam & 0xffff0000) >> 16;
            app.resize();
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

    bool done = false;

    app.init();

    app.window.hinstance = hInstance;
    strncpy(app.window.name, WINDOW_NAME, APP_NAME_STR_LEN);
    app.create_window();
    app.init_vk_swapchain();

    app.prepare();

    while (!done) {
        PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE);
        if (msg.message == WM_QUIT) {
            done = true;
        }
        else {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        RedrawWindow(app.window.hwnd, nullptr, nullptr, RDW_INTERNALPAINT);
    }

    app.cleanup();

    return (int)msg.wParam;
}

