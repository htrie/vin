#pragma once

vk::UniqueInstance create_instance() {
	Array<const char*, 4> layers;
#if !defined(NDEBUG)
	layers.push_back("VK_LAYER_KHRONOS_validation");
#endif

	Array<const char*, 4> extensions = {
		VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
		VK_KHR_SURFACE_EXTENSION_NAME,
		VK_KHR_WIN32_SURFACE_EXTENSION_NAME };

	const auto app_info = vk::ApplicationInfo()
		.setApplicationVersion(0)
		.setEngineVersion(0)
		.setApiVersion(VK_API_VERSION_1_0);
	const auto inst_info = vk::InstanceCreateInfo()
		.setPApplicationInfo(&app_info)
		.setEnabledLayerCount((uint32_t)layers.size())
		.setPpEnabledLayerNames(layers.data())
		.setEnabledExtensionCount((uint32_t)extensions.size())
		.setPpEnabledExtensionNames(extensions.data());

	auto instance_handle = vk::createInstanceUnique(inst_info);
	verify(instance_handle.result == vk::Result::eSuccess);
	return std::move(instance_handle.value);
}

vk::PhysicalDevice pick_gpu(const vk::Instance& instance) {
	const auto gpus_handle = instance.enumeratePhysicalDevices();
	verify(gpus_handle.result == vk::Result::eSuccess);
	verify(gpus_handle.value.size() > 0);

	const auto gpu = gpus_handle.value[0];
	verify(gpu.getProperties().deviceType== vk::PhysicalDeviceType::eDiscreteGpu || 
		gpu.getProperties().deviceType == vk::PhysicalDeviceType::eIntegratedGpu);
	return gpu;
}

vk::UniqueSurfaceKHR create_surface(const vk::Instance& instance, HINSTANCE hInstance, HWND hWnd) {
	const auto surf_info = vk::Win32SurfaceCreateInfoKHR()
		.setHinstance(hInstance)
		.setHwnd(hWnd);

	auto surface_handle = instance.createWin32SurfaceKHRUnique(surf_info);
	verify(surface_handle.result == vk::Result::eSuccess);
	return std::move(surface_handle.value);
}

uint32_t find_queue_family(const vk::PhysicalDevice& gpu, const vk::SurfaceKHR& surface) {
	const auto queue_props = gpu.getQueueFamilyProperties();
	verify(queue_props.size() >= 1);
	verify(!!(queue_props[0].queueFlags & vk::QueueFlagBits::eGraphics));
	return 0;
}

vk::UniqueDevice create_device(const vk::PhysicalDevice& gpu, uint32_t family_index) {
	Array<float, 1> const priorities = { 0.0 };

	Array<vk::DeviceQueueCreateInfo, 1> queues = { vk::DeviceQueueCreateInfo()
		.setQueueFamilyIndex(family_index)
		.setQueueCount((uint32_t)priorities.size())
		.setPQueuePriorities(priorities.data()) };

	Array<const char*, 1> extensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME };

	auto device_info = vk::DeviceCreateInfo()
		.setQueueCreateInfoCount((uint32_t)queues.size())
		.setPQueueCreateInfos(queues.data())
		.setEnabledLayerCount(0)
		.setPpEnabledLayerNames(nullptr)
		.setEnabledExtensionCount((uint32_t)extensions.size())
		.setPpEnabledExtensionNames(extensions.data())
		.setPEnabledFeatures(nullptr);

	auto device_handle = gpu.createDeviceUnique(device_info);
	verify(device_handle.result == vk::Result::eSuccess);
	return std::move(device_handle.value);
}

vk::SurfaceFormatKHR select_format(const vk::PhysicalDevice& gpu, const vk::SurfaceKHR& surface) {
	auto formats_handle = gpu.getSurfaceFormatsKHR(surface);
	verify(formats_handle.result == vk::Result::eSuccess);
	verify(formats_handle.value.size() > 0);
	return formats_handle.value[0].format == vk::Format::eUndefined ? vk::Format::eB8G8R8A8Unorm : formats_handle.value[0];
}

vk::Queue fetch_queue(const vk::Device& device, uint32_t family_index) {
	vk::Queue queue;
	device.getQueue(family_index, 0, &queue);
	return queue;
}

vk::UniqueCommandPool create_command_pool(const vk::Device& device, uint32_t family_index) {
	const auto cmd_pool_info = vk::CommandPoolCreateInfo()
		.setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer)
		.setQueueFamilyIndex(family_index);

	auto cmd_pool_handle = device.createCommandPoolUnique(cmd_pool_info);
	verify(cmd_pool_handle.result == vk::Result::eSuccess);
	return std::move(cmd_pool_handle.value);
}

vk::UniqueDescriptorSetLayout create_descriptor_layout(const vk::Device& device) {
	Array<vk::DescriptorSetLayoutBinding, 2> const layout_bindings = {
		vk::DescriptorSetLayoutBinding()
		   .setBinding(0)
		   .setDescriptorType(vk::DescriptorType::eUniformBuffer)
		   .setDescriptorCount(1)
		   .setStageFlags(vk::ShaderStageFlagBits::eVertex)
		   .setPImmutableSamplers(nullptr),
		vk::DescriptorSetLayoutBinding()
			.setBinding(1)
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			.setDescriptorCount(1)
			.setStageFlags(vk::ShaderStageFlagBits::eFragment)
			.setPImmutableSamplers(nullptr) };

	const auto desc_layout_info = vk::DescriptorSetLayoutCreateInfo()
		.setBindingCount((uint32_t)layout_bindings.size())
		.setPBindings(layout_bindings.data());

	auto desc_layout_handle = device.createDescriptorSetLayoutUnique(desc_layout_info);
	verify(desc_layout_handle.result == vk::Result::eSuccess);
	return std::move(desc_layout_handle.value);
}

vk::UniquePipelineLayout create_pipeline_layout(const vk::Device& device, const vk::DescriptorSetLayout& desc_layout, uint32_t constants_size) {
	Array<vk::PushConstantRange, 1> const push_constants = {
		vk::PushConstantRange()
			.setOffset(0)
			.setSize(constants_size)
			.setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment) };

	const auto pipeline_layout_info = vk::PipelineLayoutCreateInfo()
		.setPushConstantRangeCount((uint32_t)push_constants.size())
		.setPPushConstantRanges(push_constants.data())
		.setSetLayoutCount(1)
		.setPSetLayouts(&desc_layout);

	auto pipeline_layout_handle = device.createPipelineLayoutUnique(pipeline_layout_info);
	verify(pipeline_layout_handle.result == vk::Result::eSuccess);
	return std::move(pipeline_layout_handle.value);
}

vk::UniqueRenderPass create_render_pass(const vk::Device& device, const vk::SurfaceFormatKHR& surface_format) {
	const Array<vk::AttachmentDescription, 1> attachments = {
		vk::AttachmentDescription()
			.setFormat(surface_format.format)
			.setSamples(vk::SampleCountFlagBits::e1)
			.setLoadOp(vk::AttachmentLoadOp::eClear)
			.setStoreOp(vk::AttachmentStoreOp::eStore)
			.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
			.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
			.setInitialLayout(vk::ImageLayout::eUndefined)
			.setFinalLayout(vk::ImageLayout::ePresentSrcKHR) };

	Array<vk::AttachmentReference, 1> const color_references = {
		vk::AttachmentReference()
			.setAttachment(0)
			.setLayout(vk::ImageLayout::eColorAttachmentOptimal) };

	const auto subpass = vk::SubpassDescription()
		.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
		.setInputAttachmentCount(0)
		.setPInputAttachments(nullptr)
		.setColorAttachmentCount((uint32_t)color_references.size())
		.setPColorAttachments(color_references.data())
		.setPResolveAttachments(nullptr)
		.setPreserveAttachmentCount(0)
		.setPPreserveAttachments(nullptr);

	Array<vk::SubpassDependency, 1> const dependencies = {
		vk::SubpassDependency()
			.setSrcSubpass(VK_SUBPASS_EXTERNAL)
			.setDstSubpass(0)
			.setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
			.setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
			.setSrcAccessMask(vk::AccessFlagBits())
			.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eColorAttachmentRead)
			.setDependencyFlags(vk::DependencyFlags()), };

	const auto rp_info = vk::RenderPassCreateInfo()
		.setAttachmentCount((uint32_t)attachments.size())
		.setPAttachments(attachments.data())
		.setSubpassCount(1)
		.setPSubpasses(&subpass)
		.setDependencyCount((uint32_t)dependencies.size())
		.setPDependencies(dependencies.data());

	auto render_pass_handle = device.createRenderPassUnique(rp_info);
	verify(render_pass_handle.result == vk::Result::eSuccess);
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
	verify(module_handle.result == vk::Result::eSuccess);
	return std::move(module_handle.value);
}

vk::UniquePipeline create_pipeline(const vk::Device& device, const vk::PipelineLayout& pipeline_layout, const vk::RenderPass& render_pass) {
	const auto vert_shader_module = create_module(device, vert_bytecode, sizeof(vert_bytecode));
	const auto frag_shader_module = create_module(device, frag_bytecode, sizeof(frag_bytecode));

	Array<vk::PipelineShaderStageCreateInfo, 2> const shader_stage_infos = {
		vk::PipelineShaderStageCreateInfo()
			.setStage(vk::ShaderStageFlagBits::eVertex)
			.setModule(vert_shader_module.get())
			.setPName("main"),
		vk::PipelineShaderStageCreateInfo()
			.setStage(vk::ShaderStageFlagBits::eFragment)
			.setModule(frag_shader_module.get())
			.setPName("main") };

	vk::PipelineVertexInputStateCreateInfo const vertex_input_info;

	const auto input_assembly_info = vk::PipelineInputAssemblyStateCreateInfo()
		.setTopology(vk::PrimitiveTopology::eTriangleList);

	const auto viewport_info = vk::PipelineViewportStateCreateInfo()
		.setViewportCount(1)
		.setScissorCount(1);

	const auto rasterization_info = vk::PipelineRasterizationStateCreateInfo()
		.setDepthClampEnable(VK_FALSE)
		.setRasterizerDiscardEnable(VK_FALSE)
		.setPolygonMode(vk::PolygonMode::eFill)
		.setCullMode(vk::CullModeFlagBits::eBack)
		.setFrontFace(vk::FrontFace::eCounterClockwise)
		.setDepthBiasEnable(VK_FALSE)
		.setLineWidth(1.0f);

	const auto multisample_info = vk::PipelineMultisampleStateCreateInfo();

	const auto depth_stencil_info = vk::PipelineDepthStencilStateCreateInfo()
		.setDepthTestEnable(VK_FALSE)
		.setDepthWriteEnable(VK_FALSE)
		.setDepthBoundsTestEnable(VK_FALSE)
		.setStencilTestEnable(VK_FALSE);

	Array<vk::PipelineColorBlendAttachmentState, 1> const color_blend_attachments = {
		vk::PipelineColorBlendAttachmentState()
			.setBlendEnable(true)
			.setSrcColorBlendFactor(vk::BlendFactor::eSrcAlpha)
			.setDstColorBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha)
			.setColorBlendOp(vk::BlendOp::eAdd)
			.setSrcAlphaBlendFactor(vk::BlendFactor::eOneMinusDstAlpha)
			.setDstAlphaBlendFactor(vk::BlendFactor::eOne)
			.setAlphaBlendOp(vk::BlendOp::eAdd)
			.setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA) };

	const auto color_blend_info = vk::PipelineColorBlendStateCreateInfo()
		.setAttachmentCount((uint32_t)color_blend_attachments.size())
		.setPAttachments(color_blend_attachments.data());

	Array<vk::DynamicState, 2> const dynamic_states = {
		vk::DynamicState::eViewport,
		vk::DynamicState::eScissor };

	const auto dynamic_state_info = vk::PipelineDynamicStateCreateInfo()
		.setDynamicStateCount((uint32_t)dynamic_states.size())
		.setPDynamicStates(dynamic_states.data());

	const auto pipeline_info = vk::GraphicsPipelineCreateInfo()
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
	verify(pipeline_handles.result == vk::Result::eSuccess);
	return std::move(pipeline_handles.value[0]);
}

vk::UniqueDescriptorPool create_descriptor_pool(const vk::Device& device) {
	Array<vk::DescriptorPoolSize, 2> const pool_sizes = {
		vk::DescriptorPoolSize()
			.setType(vk::DescriptorType::eUniformBuffer)
			.setDescriptorCount(16),
        vk::DescriptorPoolSize()
            .setType(vk::DescriptorType::eCombinedImageSampler)
            .setDescriptorCount(16)
	};

	const auto desc_pool_info = vk::DescriptorPoolCreateInfo()
		.setMaxSets(16)
		.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
		.setPoolSizeCount((uint32_t)pool_sizes.size())
		.setPPoolSizes(pool_sizes.data());

	auto desc_pool_handle = device.createDescriptorPoolUnique(desc_pool_info);
	verify(desc_pool_handle.result == vk::Result::eSuccess);
	return std::move(desc_pool_handle.value);
}

vk::UniqueFence create_fence(const vk::Device& device) {
	const auto fence_info = vk::FenceCreateInfo()
		.setFlags(vk::FenceCreateFlagBits::eSignaled);

	auto fence_handle = device.createFenceUnique(fence_info);
	verify(fence_handle.result == vk::Result::eSuccess);
	return std::move(fence_handle.value);
}

vk::UniqueSemaphore create_semaphore(const vk::Device& device) {
	const auto semaphore_info = vk::SemaphoreCreateInfo();

	auto semaphore_handle = device.createSemaphoreUnique(semaphore_info);
	verify(semaphore_handle.result == vk::Result::eSuccess);
	return std::move(semaphore_handle.value);
}

vk::UniqueCommandBuffer create_command_buffer(const vk::Device& device, const vk::CommandPool& cmd_pool) {
	const auto cmd_info = vk::CommandBufferAllocateInfo()
		.setCommandPool(cmd_pool)
		.setLevel(vk::CommandBufferLevel::ePrimary)
		.setCommandBufferCount(1);

	auto cmd_handles = device.allocateCommandBuffersUnique(cmd_info);
	verify(cmd_handles.result == vk::Result::eSuccess);
	return std::move(cmd_handles.value[0]);
}

vk::UniqueSwapchainKHR create_swapchain(const vk::PhysicalDevice& gpu, const vk::Device& device, const vk::SurfaceKHR& surface, const vk::SurfaceFormatKHR& surface_format, const vk::SwapchainKHR& old_swapchain, int32_t window_width, int32_t window_height, uint32_t image_count) {
	vk::SurfaceCapabilitiesKHR surf_caps;
	const auto result = gpu.getSurfaceCapabilitiesKHR(surface, &surf_caps);
	verify(result == vk::Result::eSuccess);

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

	const auto swapchain_info = vk::SwapchainCreateInfoKHR()
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
	verify(swapchain_handle.result == vk::Result::eSuccess);
	return std::move(swapchain_handle.value);
}

vk::UniqueImageView create_image_view(const vk::Device& device, const vk::Image& image, const vk::Format& format) {
	auto view_info = vk::ImageViewCreateInfo()
		.setViewType(vk::ImageViewType::e2D)
		.setFormat(format)
		.setImage(image)
		.setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));

	auto view_handle = device.createImageViewUnique(view_info);
	verify(view_handle.result == vk::Result::eSuccess);
	return std::move(view_handle.value);
}

vk::UniqueFramebuffer create_framebuffer(const vk::Device& device, const vk::RenderPass& render_pass, const vk::ImageView& image_view, int32_t window_width, int32_t window_height) {
	const Array<vk::ImageView, 1> attachments = { image_view };

	const auto fb_info = vk::FramebufferCreateInfo()
		.setRenderPass(render_pass)
		.setAttachmentCount((uint32_t)attachments.size())
		.setPAttachments(attachments.data())
		.setWidth((uint32_t)window_width)
		.setHeight((uint32_t)window_height)
		.setLayers(1);

	auto framebuffer_handle = device.createFramebufferUnique(fb_info);
	verify(framebuffer_handle.result == vk::Result::eSuccess);
	return std::move(framebuffer_handle.value);
}

vk::UniqueSampler create_sampler(const vk::Device& device) {
	const auto sampler_info = vk::SamplerCreateInfo()
		.setMagFilter(vk::Filter::eNearest)
		.setMinFilter(vk::Filter::eNearest)
		.setMipmapMode(vk::SamplerMipmapMode::eNearest)
		.setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
		.setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
		.setAddressModeW(vk::SamplerAddressMode::eClampToEdge)
		.setMipLodBias(0.0f)
		.setAnisotropyEnable(VK_FALSE)
		.setMaxAnisotropy(1)
		.setCompareEnable(VK_FALSE)
		.setCompareOp(vk::CompareOp::eNever)
		.setMinLod(0.0f)
		.setMaxLod(0.0f)
		.setBorderColor(vk::BorderColor::eFloatOpaqueWhite)
		.setUnnormalizedCoordinates(VK_FALSE);

	auto sampler_handle = device.createSamplerUnique(sampler_info);
	verify(sampler_handle.result == vk::Result::eSuccess);
	return std::move(sampler_handle.value);
}

vk::UniqueImage create_image(const vk::PhysicalDevice& gpu, const vk::Device& device, unsigned font_width, unsigned font_height) {
	vk::FormatProperties props;
	gpu.getFormatProperties(vk::Format::eR8Unorm, &props);
	verify((props.linearTilingFeatures & vk::FormatFeatureFlagBits::eSampledImage) == vk::FormatFeatureFlagBits::eSampledImage);

	const auto image_info = vk::ImageCreateInfo()
		.setImageType(vk::ImageType::e2D)
		.setFormat(vk::Format::eR8Unorm)
		.setExtent({ font_width, font_height, 1 })
		.setMipLevels(1)
		.setArrayLayers(1)
		.setSamples(vk::SampleCountFlagBits::e1)
		.setTiling(vk::ImageTiling::eLinear)
		.setUsage(vk::ImageUsageFlagBits::eSampled)
		.setSharingMode(vk::SharingMode::eExclusive)
		.setQueueFamilyIndexCount(0)
		.setPQueueFamilyIndices(nullptr)
		.setInitialLayout(vk::ImageLayout::ePreinitialized);

	auto image_handle = device.createImageUnique(image_info);
	verify(image_handle.result == vk::Result::eSuccess);
	return std::move(image_handle.value);
}

vk::UniqueBuffer create_uniform_buffer(const vk::Device& device, size_t size) {
	const auto buf_info = vk::BufferCreateInfo()
		.setSize(size)
		.setUsage(vk::BufferUsageFlagBits::eUniformBuffer);

	auto buffer_handle = device.createBufferUnique(buf_info);
	verify(buffer_handle.result == vk::Result::eSuccess);
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

vk::UniqueDeviceMemory create_image_memory(const vk::PhysicalDevice& gpu, const vk::Device& device, const vk::Image& image) {
	vk::MemoryRequirements mem_reqs;
	device.getImageMemoryRequirements(image, &mem_reqs);

	auto mem_info = vk::MemoryAllocateInfo()
		.setAllocationSize(mem_reqs.size)
		.setMemoryTypeIndex(0);

	auto pass = memory_type_from_properties(gpu, mem_reqs.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, &mem_info.memoryTypeIndex);
	verify(pass == true);

	auto memory_handle = device.allocateMemoryUnique(mem_info);
	verify(memory_handle.result == vk::Result::eSuccess);
	return std::move(memory_handle.value);
}

vk::UniqueDeviceMemory create_uniform_memory(const vk::PhysicalDevice& gpu, const vk::Device& device, const vk::Buffer& buffer) {
	vk::MemoryRequirements mem_reqs;
	device.getBufferMemoryRequirements(buffer, &mem_reqs);

	auto mem_info = vk::MemoryAllocateInfo()
		.setAllocationSize(mem_reqs.size)
		.setMemoryTypeIndex(0);

	bool const pass = memory_type_from_properties(gpu, mem_reqs.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, &mem_info.memoryTypeIndex);
	verify(pass);

	auto memory_handle = device.allocateMemoryUnique(mem_info);
	verify(memory_handle.result == vk::Result::eSuccess);
	return std::move(memory_handle.value);
}

void* map_memory(const vk::Device& device, const vk::DeviceMemory& memory) {
	void* mem = nullptr;
	const auto result = device.mapMemory(memory, 0, VK_WHOLE_SIZE, vk::MemoryMapFlags(), &mem);
	verify(result == vk::Result::eSuccess);
	return mem;
}

void unmap_memory(const vk::Device& device, const vk::DeviceMemory& memory) {
	device.unmapMemory(memory);
}

void bind_memory(const vk::Device& device, const vk::Buffer& buffer, const vk::DeviceMemory& memory) {
	const auto result = device.bindBufferMemory(buffer, memory, 0);
	verify(result == vk::Result::eSuccess);
}

void copy_image_data(const vk::Device& device, const vk::Image& image, const vk::DeviceMemory& image_memory, const uint8_t* image_pixels, size_t image_size, unsigned font_width) {
	auto result = device.bindImageMemory(image, image_memory, 0);
	verify(result == vk::Result::eSuccess);

	const auto image_subres = vk::ImageSubresource()
		.setAspectMask(vk::ImageAspectFlagBits::eColor)
		.setMipLevel(0)
		.setArrayLayer(0);

	vk::SubresourceLayout layout;
	device.getImageSubresourceLayout(image, &image_subres, &layout);
	verify(layout.rowPitch == font_width);

	void* image_ptr = map_memory(device, image_memory);
	memcpy(image_ptr, image_pixels, image_size);
	unmap_memory(device, image_memory);
}
vk::UniqueDescriptorSet create_descriptor_set(const vk::Device& device, const vk::DescriptorPool& desc_pool, const vk::DescriptorSetLayout& desc_layout) {
	const auto alloc_info = vk::DescriptorSetAllocateInfo()
		.setDescriptorPool(desc_pool)
		.setDescriptorSetCount(1)
		.setPSetLayouts(&desc_layout);

	auto descriptor_set_handles = device.allocateDescriptorSetsUnique(alloc_info);
	verify(descriptor_set_handles.result == vk::Result::eSuccess);
	return std::move(descriptor_set_handles.value[0]);
}

void update_descriptor_set(const vk::Device& device, const vk::DescriptorSet& desc_set, const vk::Buffer& buffer, size_t range, const vk::Sampler& sampler, const vk::ImageView& image_view) {
	const auto buffer_info = vk::DescriptorBufferInfo()
		.setOffset(0)
		.setRange(range)
		.setBuffer(buffer);

    const auto image_info = vk::DescriptorImageInfo()
        .setSampler(sampler)
        .setImageView(image_view)
        .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

	const Array<vk::WriteDescriptorSet, 2> writes = {
		vk::WriteDescriptorSet()
			.setDescriptorCount(1)
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setPBufferInfo(&buffer_info)
			.setDstSet(desc_set),
		vk::WriteDescriptorSet()
			.setDescriptorCount(1)
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			.setPImageInfo(&image_info)
			.setDstBinding(1)
			.setDstSet(desc_set) };

	device.updateDescriptorSets((uint32_t)writes.size(), writes.data(), 0, nullptr);
}

void wait_idle(const vk::Device& device) {
	const auto result = device.waitIdle();
	verify(result == vk::Result::eSuccess);
}

void wait(const vk::Device& device, const vk::Fence& fence) {
	const auto result = device.waitForFences(1, &fence, VK_TRUE, UINT64_MAX);
	verify(result == vk::Result::eSuccess);
	device.resetFences({ fence });
}

uint32_t acquire(const vk::Device& device, const vk::SwapchainKHR& swapchain, const vk::Semaphore& signal_sema) {
	uint32_t image_index = 0;
	const auto result = device.acquireNextImageKHR(swapchain, UINT64_MAX, signal_sema, vk::Fence(), &image_index);
	verify(result == vk::Result::eSuccess);
	return image_index;
}

void submit(const vk::Queue& queue, const vk::Semaphore& wait_sema, const vk::Semaphore& signal_sema, const vk::CommandBuffer& cmd_buf, const vk::Fence& fence) {
	vk::PipelineStageFlags const pipe_stage_flags = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	const auto submit_info = vk::SubmitInfo()
		.setPWaitDstStageMask(&pipe_stage_flags)
		.setWaitSemaphoreCount(1)
		.setPWaitSemaphores(&wait_sema)
		.setSignalSemaphoreCount(1)
		.setPSignalSemaphores(&signal_sema)
		.setCommandBufferCount(1)
		.setPCommandBuffers(&cmd_buf);

	const auto result = queue.submit(1, &submit_info, fence);
	verify(result == vk::Result::eSuccess);
}

void present(const vk::SwapchainKHR& swapchain, const vk::Queue& queue, const vk::Semaphore& wait_sema, uint32_t image_index) {
	const auto present_info = vk::PresentInfoKHR()
		.setWaitSemaphoreCount(1)
		.setPWaitSemaphores(&wait_sema)
		.setSwapchainCount(1)
		.setPSwapchains(&swapchain)
		.setPImageIndices(&image_index);

	const auto result = queue.presentKHR(&present_info);
	verify(result == vk::Result::eSuccess);
}

void begin(const vk::CommandBuffer& cmd_buf) {
	const auto command_buffer_info = vk::CommandBufferBeginInfo()
		.setFlags(vk::CommandBufferUsageFlagBits::eSimultaneousUse);

	auto result = cmd_buf.begin(&command_buffer_info);
	verify(result == vk::Result::eSuccess);
}

void end(const vk::CommandBuffer& cmd_buf) {
	auto result = cmd_buf.end();
	verify(result == vk::Result::eSuccess);
}

void begin_pass(const vk::CommandBuffer& cmd_buf, const vk::RenderPass& render_pass, const vk::Framebuffer& framebuffer, const Color& clear_color, uint32_t width, uint32_t height) {
	const auto clear_value = vk::ClearColorValue(clear_color.rgba());

	const auto pass_info = vk::RenderPassBeginInfo()
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

void add_image_barrier(const vk::CommandBuffer& cmd_buf, const vk::Image& image) {
	const auto barrier = vk::ImageMemoryBarrier()
		.setSrcAccessMask(vk::AccessFlagBits())
		.setDstAccessMask(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eInputAttachmentRead)
		.setOldLayout(vk::ImageLayout::ePreinitialized)
		.setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
		.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
		.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
		.setImage(image)
		.setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));

	cmd_buf.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eFragmentShader, vk::DependencyFlagBits(), 0, nullptr, 0, nullptr, 1, &barrier);
}

void set_viewport(const vk::CommandBuffer& cmd_buf, float width, float height) {
	const auto viewport = vk::Viewport()
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

Array<vk::Image, 3> get_swapchain_images(const vk::Device& device, const vk::SwapchainKHR& swapchain) {
	uint32_t count = 0;
	auto result = device.getSwapchainImagesKHR(swapchain, &count, static_cast<vk::Image*>(nullptr));
	verify(result == vk::Result::eSuccess);

	Array<vk::Image, 3> images(count);
	result = device.getSwapchainImagesKHR(swapchain, &count, images.data());
	verify(result == vk::Result::eSuccess);

	return images;
}



struct Constants {
	Matrix model;
	Vec4 color;
	Vec2 uv_origin;
	Vec2 uv_sizes;
};

struct Uniforms {
	Matrix view_proj;
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
	vk::UniquePipelineLayout pipeline_layout;
	vk::UniquePipeline pipeline;

    vk::UniqueSampler sampler;

	struct Font {
		vk::UniqueImage image;
		vk::UniqueDeviceMemory image_memory;
		vk::UniqueImageView image_view;
		vk::UniqueDescriptorSet descriptor_set;
		unsigned width = 0;
		unsigned height = 0;
		FontGlyphs glyphs;
	};
	Font font_regular;
	Font font_bold;
	Font font_italic;

	vk::UniqueBuffer uniform_buffer;
	vk::UniqueDeviceMemory uniform_memory;
	void* uniform_ptr = nullptr;

	Array<vk::UniqueFence, 2> fences;
	Array<vk::UniqueSemaphore, 2> image_acquired_semaphores;
	Array<vk::UniqueSemaphore, 2> draw_complete_semaphores;

	vk::UniqueSwapchainKHR swapchain;
	Array<vk::UniqueImageView, 3> image_views;
	Array<vk::UniqueFramebuffer, 3> framebuffers;
	Array<vk::UniqueCommandBuffer, 3> cmds;

	unsigned width = 0;
	unsigned height = 0;

	unsigned fence_index = 0;

	Font upload_font(const vk::CommandBuffer& cmd_buf, const uint8_t* image_pixels, size_t image_size, unsigned width, unsigned height, FontGlyphs glyphs) {
		Font font;
		font.width = width;
		font.height = height;
		font.glyphs = glyphs;
		font.image = create_image(gpu, device.get(), width, height);
		font.image_memory = create_image_memory(gpu, device.get(), font.image.get());
		copy_image_data(device.get(), font.image.get(), font.image_memory.get(), image_pixels, image_size, width);
		font.image_view = create_image_view(device.get(), font.image.get(), vk::Format::eR8Unorm);
		add_image_barrier(cmd_buf, font.image.get());
		font.descriptor_set = create_descriptor_set(device.get(), desc_pool.get(), desc_layout.get());
		update_descriptor_set(device.get(), font.descriptor_set.get(), uniform_buffer.get(), sizeof(Uniforms), sampler.get(), font.image_view.get());
		return font;
	}

	void upload_fonts(const vk::CommandBuffer& cmd_buf) {
		sampler = create_sampler(device.get());
		font_regular = upload_font(cmd_buf, font_regular_pixels, sizeof(font_regular_pixels), font_regular_width, font_regular_height, font_regular_glyphs);
		font_bold = upload_font(cmd_buf, font_bold_pixels, sizeof(font_bold_pixels), font_bold_width, font_bold_height, font_bold_glyphs);
		font_italic = upload_font(cmd_buf, font_italic_pixels, sizeof(font_italic_pixels), font_italic_width, font_italic_height, font_italic_glyphs);
	}

	const FontGlyph* find_glyph(const FontGlyphs& glyphs, uint16_t id) {
		for (auto& glyph : glyphs) {
			if (glyph.id == id)
				return &glyph;
		}
		return nullptr;
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
		pipeline_layout = create_pipeline_layout(device.get(), desc_layout.get(), sizeof(Constants));
		render_pass = create_render_pass(device.get(), surface_format);
		pipeline = create_pipeline(device.get(), pipeline_layout.get(), render_pass.get());

		uniform_buffer = create_uniform_buffer(device.get(), sizeof(Uniforms));
		uniform_memory = create_uniform_memory(gpu, device.get(), uniform_buffer.get());
		bind_memory(device.get(), uniform_buffer.get(), uniform_memory.get());
		uniform_ptr = map_memory(device.get(), uniform_memory.get());

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

			image_views.clear();
			framebuffers.clear();
			cmds.clear();

			swapchain = create_swapchain(gpu, device.get(), surface.get(), surface_format, swapchain.get(), width, height, 3);
			for (auto& swapchain_image : get_swapchain_images(device.get(), swapchain.get())) {
				image_views.emplace_back(create_image_view(device.get(), swapchain_image, surface_format.format));
				framebuffers.emplace_back(create_framebuffer(device.get(), render_pass.get(), image_views.back().get(), width, height));
				cmds.emplace_back(create_command_buffer(device.get(), cmd_pool.get()));
			}

			const auto view = Matrix::look_at({ 0.0f, 0.0f, 10.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f });
			const auto proj = Matrix::ortho(0.0f, (float)width, 0.0f, (float)height, -100.0f, 100.0f);
			auto& uniforms = *(Uniforms*)uniform_ptr;
			uniforms.view_proj = view * proj;
		}
	}

	void redraw(const Characters& characters) {
		wait(device.get(), fences[fence_index].get());
		const auto frame_index = acquire(device.get(), swapchain.get(), image_acquired_semaphores[fence_index].get());
		const auto& cmd = cmds[frame_index].get();

		begin(cmd);
		if (!font_regular.image) upload_fonts(cmd);
		begin_pass(cmd, render_pass.get(), framebuffers[frame_index].get(), colors().clear, width, height);

		set_viewport(cmd, (float)width, (float)height);
		set_scissor(cmd, width, height);

		bind_pipeline(cmd, pipeline.get());

		for (auto& character : characters) {
			const auto& font = character.bold ? font_bold : character.italic ? font_italic : font_regular;
			if (auto* glyph = find_glyph(font.glyphs, character.index)) {
				const float trans_x = character.col * spacing().character + glyph->x_off * font.width;
				const float trans_y = character.row * spacing().line + glyph->y_off * font.height;

				Constants constants;
				constants.model = {
					{ glyph->w * font.width, 0.0f, 0.0f, 0.0f },
					{ 0.0f, glyph->h * font.height, 0.0f, 0.0f },
					{ 0.0f, 0.0f, 1.0f, 0.0f },
					{ trans_x, trans_y, 0.0f, 1.0f } };
				constants.color = character.color.rgba();
				constants.uv_origin = { glyph->x, glyph->y };
				constants.uv_sizes = { glyph->w, glyph->h };

				bind_descriptor_set(cmd, pipeline_layout.get(), font.descriptor_set.get());

				push(cmd, pipeline_layout.get(), sizeof(Constants), &constants);
				draw(cmd, 6);
			}
		}

		end_pass(cmd);
		end(cmd);

		submit(queue, image_acquired_semaphores[fence_index].get(), draw_complete_semaphores[fence_index].get(), cmd, fences[fence_index].get());
		present(swapchain.get(), queue, draw_complete_semaphores[fence_index].get(), frame_index);

		fence_index += 1;
		fence_index %= fences.size();
	}

	Viewport viewport() const {
		return {
			(unsigned)((float)width / spacing().character),
			(unsigned)((float)height / spacing().line) - 1 // Remove 1 line for half-lines.
		};
	}

	HWND get_hwnd() const { return hWnd; }
};

