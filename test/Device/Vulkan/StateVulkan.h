#pragma once

namespace Device
{
	class Buffer;
	struct VertexInput;

	struct BufferSlot {
		vk::Buffer Buffer;
		size_t Offset = 0;
		size_t Size = 0;

		BufferSlot() = default;
		BufferSlot(vk::Buffer buffer, size_t offset, size_t size) : Buffer(buffer), Offset(offset), Size(size) {}
	};

	struct DependentBuffer {
		vk::Buffer Buffer = nullptr;
		bool RequireWriteAccess = false;

		DependentBuffer(vk::Buffer buffer = nullptr, bool write_access = false) : Buffer(buffer), RequireWriteAccess(write_access) {}
	};

	struct DependentImage {
		vk::Image Image = nullptr;
		bool RequireWriteAccess = false;

		DependentImage(vk::Image image = nullptr, bool write_access = false) : Image(image), RequireWriteAccess(write_access) {}
	};

	typedef Memory::SmallVector<vk::BufferImageCopy, 64, Memory::Tag::Device> BufferImageCopies;
	typedef Memory::SmallVector<vk::AttachmentDescription, RenderTargetSlotCount + 1, Memory::Tag::Device> Attachments;
	typedef Memory::SmallVector<vk::AttachmentReference, RenderTargetSlotCount, Memory::Tag::Device> ColorReferences;
	typedef Memory::SmallVector<vk::AttachmentReference, 1, Memory::Tag::Device> DepthReference;
	typedef Memory::SmallVector<vk::SubpassDependency, RenderTargetSlotCount + 1, Memory::Tag::Device> SubpassDependencies;
	typedef Memory::SmallVector<vk::ClearValue, RenderTargetSlotCount + 1, Memory::Tag::Device> ClearValues;
	typedef Memory::SmallVector<vk::ClearAttachment, RenderTargetSlotCount, Memory::Tag::Device> ClearAttachments;
	typedef Memory::SmallVector<vk::ClearRect, 1, Memory::Tag::Device> ClearRects;
	typedef std::array<vk::ImageView, TextureSlotCount> ImageViewSlots;
	typedef std::array<vk::BufferView, TextureSlotCount> BufferViewSlots;
	typedef std::array<BufferSlot, TextureSlotCount> BufferSlots;
	typedef std::array<DependentBuffer, TextureSlotCount> DependentBuffers;
	typedef std::array<DependentImage, TextureSlotCount> DependentImages;

	DeclUsage UnconvertDeclUsageVulkan(const char* decl_usage);

	class PassVulkan : public Pass
	{
		vk::UniqueRenderPass render_pass;
		Memory::SmallVector<vk::UniqueFramebuffer, 3, Memory::Tag::Device> frame_buffers;

		vk::Rect2D render_area;
		ClearValues clear_values;
		unsigned color_attachment_count = 0;

	public:
		PassVulkan(const Memory::DebugStringA<>& name, IDevice* device, const ColorRenderTargets& color_render_targets, RenderTarget* depth_stencil, ClearTarget clear_flags, simd::color clear_color, float clear_z);

		vk::RenderPassBeginInfo Begin(unsigned backbuffer_index);

		const vk::RenderPass& GetRenderPass() const { return render_pass.get(); }
		const vk::Rect2D& GetRenderArea() const { return render_area; }
		unsigned GetColorAttachmentCount() const { return color_attachment_count; }
	};

	class PipelineVulkan : public Pipeline
	{
		Memory::Pointer<VertexInput, Memory::Tag::Device> vertex_input;
		vk::UniqueDescriptorSetLayout vertex_desc_set_layout;
		vk::UniqueDescriptorSetLayout pixel_desc_set_layout;
		vk::UniqueDescriptorSetLayout compute_desc_set_layout;

		vk::UniquePipelineLayout pipeline_layout;
		vk::UniquePipeline pipeline;

		void Create(IDevice* device, const Pass* pass, PrimitiveType primitive_type, Shader* vertex_shader, Shader* pixel_shader, const BlendState& blend_state, const RasterizerState& rasterizer_state, const DepthStencilState& depth_stencil_state);
		void Create(IDevice* device, Shader* compute_shader);

	public:
		PipelineVulkan(const Memory::DebugStringA<>& name, IDevice* device, const Pass* pass, PrimitiveType primitive_type, const VertexDeclaration* vertex_declaration, Shader* vertex_shader, Shader* pixel_shader, const BlendState& blend_state, const RasterizerState& rasterizer_state, const DepthStencilState& depth_stencil_state);
		PipelineVulkan(const Memory::DebugStringA<>& name, IDevice* device, Shader* compute_shader);
		~PipelineVulkan();

		virtual bool IsValid() const { return !!pipeline; }

		const vk::DescriptorSetLayout& GetVertexDescriptorSetLayout() const { return vertex_desc_set_layout.get(); }
		const vk::DescriptorSetLayout& GetPixelDescriptorSetLayout() const { return pixel_desc_set_layout.get(); }
		const vk::DescriptorSetLayout& GetComputeDescriptorSetLayout() const { return compute_desc_set_layout.get(); }
		const vk::PipelineLayout& GetPipelineLayout() const { return pipeline_layout.get(); }
		const vk::Pipeline& GetPipeline() const { return pipeline.get(); }
	};

	struct UniformOffsetsVulkan : public UniformOffsets
	{
		static Offset Allocate(IDevice* device, size_t size);
	};

	class DescriptorSetVulkan: public DescriptorSet
	{
		static constexpr size_t MAX_DESC_SETS = magic_enum::enum_count<ShaderType>();
		vk::UniqueDescriptorPool desc_pool;
		std::array<std::array<vk::UniqueDescriptorSet, MAX_DESC_SETS>, 3> desc_sets;
		std::array<std::array<DependentBuffers, MAX_DESC_SETS>, 3> dependent_buffers;
		std::array<DependentImages, MAX_DESC_SETS> dependent_images;


	public:
		DescriptorSetVulkan(const Memory::DebugStringA<>& name, IDevice* device, Pipeline* pipeline, const BindingSets& binding_sets, const DynamicBindingSets& dynamic_binding_sets);

		std::array<vk::DescriptorSet, 2> GetGraphicDescritorSets(unsigned backbuffer_index) { return { desc_sets[backbuffer_index][VERTEX_SHADER].get(), desc_sets[backbuffer_index][PIXEL_SHADER].get() }; }
		std::array<vk::DescriptorSet, 1> GetComputeDescritorSets(unsigned backbuffer_index) { return { desc_sets[backbuffer_index][COMPUTE_SHADER].get() }; }

		const DependentBuffers& GetDependentBuffers(unsigned backbuffer_index,ShaderType type) const { return dependent_buffers[backbuffer_index][size_t(type)]; }
		const DependentImages& GetDependentImages(ShaderType type) const { return dependent_images[size_t(type)]; }
	};

	class CommandBufferVulkan : public CommandBuffer
	{
		IDevice* device = nullptr;
		
		vk::UniqueCommandPool cmd_pool;
		Memory::SmallVector<vk::UniqueCommandBuffer, 3, Memory::Tag::Device> cmd_bufs;
		Memory::VectorSet<vk::Image, Memory::Tag::Device> barrier_images;
		Memory::VectorSet<vk::Buffer, Memory::Tag::Device> barrier_buffers;

	#if defined(DEBUG)
		// Just for validation
		enum {Graphics_Pipeline, Compute_Pipeline, No_Pipeline} pipeline_type = No_Pipeline;
	#endif

		void FlushMemoryBarriers();

	public:
		CommandBufferVulkan(const Memory::DebugStringA<>& name, IDevice* device);

		virtual bool Begin() override final;
		virtual void End() override final;

		virtual void BeginPass(Pass* pass, Rect viewport_rect, Rect scissor_rect, ClearTarget clear_flags, simd::color clear_color, float clear_z) final;
		virtual void EndPass() final;

		virtual void BeginComputePass() override final;
		virtual void EndComputePass() override final;

		virtual void SetScissor(Rect rect) override final;

		virtual bool BindPipeline(Pipeline* pipeline) override final;
		virtual void BindDescriptorSet(DescriptorSet* descriptor_set, const UniformOffsets* uniform_offsets = nullptr, const DynamicUniformBuffers& dynamic_uniform_buffers = DynamicUniformBuffers()) override final;
		virtual void BindBuffers(IndexBuffer* index_buffer, VertexBuffer* vertex_buffer, unsigned offset, unsigned stride, VertexBuffer* instance_vertex_buffer, unsigned instance_offset, unsigned instance_stride) override final;

		virtual void Draw(unsigned vertex_count, unsigned instance_count, unsigned vertex_start, unsigned instance_start) override final;
		virtual void DrawIndexed(unsigned index_count, unsigned instance_count, unsigned index_start, unsigned vertex_offset, unsigned instance_start) override final;
		virtual void Dispatch(unsigned group_count_x, unsigned group_count_y = 1, unsigned group_count_z = 1) override final;

		//virtual void SetCounter(StructuredBuffer* buffer, uint32_t value) override final;

		vk::CommandBuffer& GetCommandBuffer();
	};

}
