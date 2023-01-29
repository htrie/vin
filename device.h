#pragma once

static const unsigned CharacterMaxCount = 8192;

struct Uniforms {
	Matrix view_proj;
	Matrix model[CharacterMaxCount];
	Vec4 color[CharacterMaxCount];
	Vec4 uv_origin[CharacterMaxCount];
	Vec4 uv_sizes[CharacterMaxCount];
	Vec4 font_index[CharacterMaxCount];
};

struct Viewport {
	unsigned w = 0;
	unsigned h = 0;
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
	vk::UniqueDescriptorSet descriptor_set;
	vk::UniquePipelineLayout pipeline_layout;
	vk::UniquePipeline pipeline;

	struct Font {
		vk::UniqueSampler sampler;
		vk::UniqueImage image;
		vk::UniqueDeviceMemory image_memory;
		vk::UniqueImageView image_view;
		unsigned width = 0;
		unsigned height = 0;
		FontGlyphs glyphs;
	};
	Font font_regular;
	Font font_bold;

	vk::UniqueBuffer uniform_buffer;
	vk::UniqueDeviceMemory uniform_memory;
	void* uniform_ptr = nullptr;

	vk::UniqueFence fence;
	vk::UniqueSemaphore image_acquired_semaphore;
	vk::UniqueSemaphore draw_complete_semaphore;

	vk::UniqueSwapchainKHR swapchain;
	std::vector<vk::UniqueImageView> image_views;
	std::vector<vk::UniqueFramebuffer> framebuffers;
	std::vector<vk::UniqueCommandBuffer> cmds;

	unsigned width = 0;
	unsigned height = 0;

	void upload_font(const vk::CommandBuffer& cmd_buf, Font& font,
		unsigned font_width, unsigned font_height, 
		const FontGlyphs& font_glyphs,
		const uint8_t* font_pixels_data, size_t font_pixels_size) {
		font.width = font_width;
		font.height = font_height;
		font.glyphs = font_glyphs;
		font.sampler = create_sampler(device.get());
		font.image = create_image(gpu, device.get(), font_width, font_height);
		font.image_memory = create_image_memory(gpu, device.get(), font.image.get());
		copy_image_data(device.get(), font.image.get(), font.image_memory.get(), font_pixels_data, font_pixels_size, font_width);
		font.image_view = create_image_view(device.get(), font.image.get(), vk::Format::eR8Unorm);
		add_image_barrier(cmd_buf, font.image.get());
	}

	void upload_fonts(const vk::CommandBuffer& cmd_buf) {
		upload_font(cmd_buf, font_regular, font_regular_width, font_regular_height, font_regular_glyphs, font_regular_pixels, sizeof(font_regular_pixels));
		upload_font(cmd_buf, font_bold, font_bold_width, font_bold_height, font_bold_glyphs, font_bold_pixels, sizeof(font_bold_pixels));
		descriptor_set = create_descriptor_set(device.get(), desc_pool.get(), desc_layout.get());
		update_descriptor_set(device.get(), descriptor_set.get(), uniform_buffer.get(), sizeof(Uniforms),
 			font_regular.sampler.get(), font_regular.image_view.get(),
 			font_bold.sampler.get(), font_bold.image_view.get());
	}

	const FontGlyph* find_glyph(const FontGlyphs& glyphs, uint16_t id) {
		for (auto& glyph : glyphs) {
			if (glyph.id == id)
				return &glyph;
		}
		return nullptr;
	}

	Matrix compute_viewproj() {
		const auto view = Matrix::look_at({ 0.0f, 0.0f, 10.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f });
		const auto proj = Matrix::ortho(0.0f, (float)width, 0.0f, (float)height, -100.0f, 100.0f);
		return view * proj;
	}

	unsigned accumulate_uniforms(const Characters& characters) {
		auto& uniforms = *(Uniforms*)uniform_ptr;
		uniforms.view_proj = compute_viewproj();

		unsigned index = 0;

		const auto add = [&](const auto& font, float font_index, const auto& character, const auto& glyph) {
			uniforms.model[index] = {
				{ spacing().zoom * glyph.w * font.width, 0.0f, 0.0f, 0.0f },
				{ 0.0f, spacing().zoom * glyph.h * font.height, 0.0f, 0.0f },
				{ 0.0f, 0.0f, 1.0f, 0.0f },
				{
					spacing().zoom * (character.col * spacing().character + glyph.x_off * font.width),
					spacing().zoom * (character.row * spacing().line + glyph.y_off * font.height),
					0.0f, 1.0f }
			};
			uniforms.color[index] = character.color.rgba();
			uniforms.uv_origin[index] = { glyph.x, glyph.y, 0.0f, 0.0f };
			uniforms.uv_sizes[index] = { glyph.w, glyph.h, 0.0f, 0.0f };
			uniforms.font_index[index] = { font_index, 0.0f, 0.0f, 0.0f };
			index++;
		};

		for (auto& character : characters) {
			const auto& font = character.bold ? font_bold : font_regular;
			const float font_index = character.bold ? 1.0f : 0.0f;
			const auto* glyph = find_glyph(font.glyphs, character.index);
			if (glyph == nullptr)
				glyph = find_glyph(font.glyphs, Glyph::UNKNOWN);
			if (glyph)
				add(font, font_index, character, *glyph);
		}

		return index;
	}

public:
	Device(WNDPROC proc, HINSTANCE hInstance, unsigned width, unsigned height) {
		instance = create_instance();
		gpu = pick_gpu(instance.get());
		hWnd = create_window(proc, hInstance, this, width, height);
		surface = create_surface(instance.get(), hInstance, hWnd);
		surface_format = select_format(gpu, surface.get());
		auto family_index = find_queue_family(gpu, surface.get());
		device = create_device(gpu, family_index);
		queue = fetch_queue(device.get(), family_index);

		cmd_pool = create_command_pool(device.get(), family_index);
		desc_pool = create_descriptor_pool(device.get());
		desc_layout = create_descriptor_layout(device.get());
		pipeline_layout = create_pipeline_layout(device.get(), desc_layout.get());
		render_pass = create_render_pass(device.get(), surface_format);
		pipeline = create_pipeline(device.get(), pipeline_layout.get(), render_pass.get());

		uniform_buffer = create_uniform_buffer(device.get(), sizeof(Uniforms));
		uniform_memory = create_uniform_memory(gpu, device.get(), uniform_buffer.get());
		bind_memory(device.get(), uniform_buffer.get(), uniform_memory.get());
		uniform_ptr = map_memory(device.get(), uniform_memory.get());

		fence = create_fence(device.get());
		image_acquired_semaphore = create_semaphore(device.get());
		draw_complete_semaphore = create_semaphore(device.get());

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

			image_views.clear();
			framebuffers.clear();
			cmds.clear();

			swapchain = create_swapchain(gpu, device.get(), surface.get(), surface_format, swapchain.get(), width, height, 3);
			for (auto& swapchain_image : get_swapchain_images(device.get(), swapchain.get())) {
				image_views.emplace_back(create_image_view(device.get(), swapchain_image, surface_format.format));
				framebuffers.emplace_back(create_framebuffer(device.get(), render_pass.get(), image_views.back().get(), width, height));
				cmds.emplace_back(create_command_buffer(device.get(), cmd_pool.get()));
			}
		}
	}

	void redraw(const Characters& characters) {
		wait(device.get(), fence.get());
		const auto frame_index = acquire(device.get(), swapchain.get(), image_acquired_semaphore.get());
		const auto& cmd = cmds[frame_index].get();

		begin(cmd);
		if (!font_regular.image) upload_fonts(cmd);
		begin_pass(cmd, render_pass.get(), framebuffers[frame_index].get(), colors().clear, width, height);

		set_viewport(cmd, (float)width, (float)height);
		set_scissor(cmd, width, height);

		bind_pipeline(cmd, pipeline.get());
		bind_descriptor_set(cmd, pipeline_layout.get(), descriptor_set.get());

		draw(cmd, 6, accumulate_uniforms(characters));

		end_pass(cmd);
		end(cmd);

		submit(queue, image_acquired_semaphore.get(), draw_complete_semaphore.get(), cmd, fence.get());
		present(swapchain.get(), queue, draw_complete_semaphore.get(), frame_index);
	}

	Viewport viewport() const {
		return {
			(unsigned)((float)width / (spacing().zoom * spacing().character) - 0.5f),
			(unsigned)((float)height / (spacing().zoom * spacing().line) - 0.5f)
		};
	}

	HWND get_hwnd() const { return hWnd; }
};

