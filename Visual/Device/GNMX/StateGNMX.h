#pragma once

namespace Device
{
	class VertexDeclaration;
	class RenderTarget;
	class VertexBuffer;
	class IndexBuffer;
	class Shader;
	class BufferGNMX;

	static const unsigned BackBufferCount = 2;
	static unsigned current_back_buffer;

	static void Init(IDevice* device);
	static void Prepare();

	DeclUsage UnconvertDeclUsage(const char* decl_usage);

	struct PassGNMX : public Pass
	{
		std::array<RenderTarget*, RenderTargetSlotCount> color_render_targets;
		RenderTarget* depth_stencil_render_target = nullptr;
		
		ClearTarget clear_flags;
		simd::color clear_color;
		float clear_z;
		unsigned stencil_ref = 0;

		Gnm::ZFormat z_format;

		PassGNMX(const Memory::DebugStringA<>& name, IDevice* device, const ColorRenderTargets& color_render_targets, RenderTarget* depth_stencil, ClearTarget clear_flags, simd::color _clear_color, float clear_z);
	};

	struct PipelineGNMX : public Pipeline
	{
		Gnm::PrimitiveType primitive_type;
		Gnm::BlendControl blend_control[RenderTargetSlotCount];
		Gnm::ClipControl clip_control;
		Gnm::PrimitiveSetup primitive_setup;
		Gnm::RenderOverrideControl render_override_control;
		Gnm::DepthStencilControl depth_stencil_control;
		Gnm::StencilControl stencil_control;
		Gnm::StencilOpControl stencil_op_control;
		Gnm::DbRenderControl db_render_control;

		bool polygon_enable = false;
		float polygon_clamp = 0.0f;
		float polygon_scale = 0.0f;
		float polygon_offset = 0.0f;

		Gnm::ZFormat z_format = Gnm::kZFormatInvalid;

		uint32_t shader_modifier = 0;
		AllocationGNMX fs_mem;

		PipelineGNMX(const Memory::DebugStringA<>& name, IDevice* device, const Pass* pass, PrimitiveType primitive_type, const VertexDeclaration* vertex_declaration, Shader* vertex_shader, Shader* pixel_shader, const BlendState& blend_state, const RasterizerState& rasterizer_state, const DepthStencilState& depth_stencil_state);
		PipelineGNMX(const Memory::DebugStringA<>& name, IDevice* device, Shader* compute_shader);

		virtual bool IsValid() const { return true; }
	};

	struct UniformOffsetsGNMX : public UniformOffsets
	{
		static Offset Allocate(IDevice* device, size_t size);
	};

	struct DescriptorSetGNMX: public DescriptorSet
	{
		DescriptorSetGNMX(const Memory::DebugStringA<>& name, IDevice* device, Pipeline* pipeline, const BindingSets& binding_sets, const DynamicBindingSets& dynamic_binding_sets);
	};

	class CommandBufferGNMX : public CommandBuffer
	{
		struct TextureSlotGNMX // TODO: Remove.
		{
			Texture* texture = nullptr;
			unsigned mip_level = (unsigned)-1;
			
			TextureSlotGNMX() {}
			TextureSlotGNMX(Texture* texture, unsigned mip_level) : texture(texture), mip_level(mip_level) {}

			bool operator!=(const TextureSlotGNMX& other) const { return (texture != other.texture) || (mip_level != other.mip_level); }
		};

		typedef std::array<BufferGNMX*, TextureSlotCount + UAVSlotCount> BoundBuffers;
		typedef std::array<TextureSlotGNMX, TextureSlotCount + UAVSlotCount> BoundTextures;

		IDevice* device = nullptr;

		PipelineGNMX* pipeline = nullptr; // TODO: Remove.

		IndexBuffer* index_buffer = nullptr; // TODO: Remove.

		Memory::FlatSet<const RenderTarget*, Memory::Tag::Device> waited_render_targets;

		bool is_reset = false;

		static_assert(UniformBufferSlotCount <= Gnm::kSlotCountConstantBuffer);
		static_assert(SamplerSlotCount <= Gnm::kSlotCountSampler);
		static_assert(TextureSlotCount <= Gnmx::LightweightConstantUpdateEngine::kMaxResourceCount);

		std::array<Gnmx::LightweightGfxContext, BackBufferCount> gfx_contexts;

		std::array<AllocationGNMX, BackBufferCount> dcb_buffer_mem;

		static uint32_t* ResourceBufferCallback(Gnmx::BaseConstantUpdateEngine* lwcue, uint32_t sizeInDwords, uint32_t* resultSizeInDwords, void* userData);

		void ClearColor(Gnmx::LightweightGfxContext& gfx, std::array<RenderTarget*, RenderTargetSlotCount>& color_render_targets, simd::color clear_color);
		void ClearDepthStencil(Gnmx::LightweightGfxContext& gfx, RenderTarget* depth_stencil_render_target, bool depth, bool stencil, float clear_z, uint8_t clear_stencil);

		bool ClearRenderTargets(Gnmx::LightweightGfxContext& gfx, std::array<RenderTarget*, RenderTargetSlotCount>& color_render_targets, RenderTarget* depth_stencil_render_target, ClearTarget clear_target, simd::color clear_color, float clear_z, uint8_t clear_stencil);
		void SetRenderTargets(Gnmx::LightweightGfxContext& gfx, std::array<RenderTarget*, RenderTargetSlotCount>& color_render_targets, RenderTarget* depth_stencil_render_target);
		void SetViewport(Gnmx::LightweightGfxContext& gfx, const Rect& viewport_rect);
		void SetScissor(Gnmx::LightweightGfxContext& gfx, const Rect& rect);
		void SetShaders(Gnmx::LightweightGfxContext& gfx, const PipelineGNMX* pipeline);
		void SetSamplers(Gnmx::LightweightGfxContext& gfx, Shader* vertex_shader, Shader* pixer_shader);
		void SetBuffers(Gnmx::LightweightGfxContext& gfx, Gnm::ShaderStage stage, Device::Shader* shader, const BoundBuffers& buffers);
		void SetTextures(Gnmx::LightweightGfxContext& gfx, Gnm::ShaderStage stage, Device::Shader* shader, const BoundTextures& textures);

		void PushDynamicUniformBuffers(DynamicUniformBuffer* uniform_buffer);
		void PushUniformBuffers(Shader* vertex_shader, Shader* pixel_shader, const UniformOffsets* uniform_offsets);
		void PushUniformBuffers(Shader* compute_shader, const UniformOffsets* uniform_offsets);

		void WaitForAliasedRenderTarget(RenderTarget* render_target);

	public:
		CommandBufferGNMX(const Memory::DebugStringA<>& name, IDevice* device);

		virtual bool Begin() override final;
		virtual void End() override final;

		virtual void BeginPass(Pass* pass, Rect viewport_rect, Rect scissor_rect, ClearTarget clear_flags, simd::color clear_color, float clear_z) override final;
		virtual void EndPass() override final;

		virtual void BeginComputePass() override final;
		virtual void EndComputePass() override final;

		virtual void SetScissor(Rect rect) override final;

		virtual bool BindPipeline(Pipeline* pipeline) override final;
		virtual void BindDescriptorSet(DescriptorSet* descriptor_set, const UniformOffsets* uniform_offsets = nullptr, const DynamicUniformBuffers& dynamic_uniform_buffers = DynamicUniformBuffers()) override final;
		virtual void BindBuffers(IndexBuffer* index_buffer, VertexBuffer* vertex_buffer, unsigned offset, unsigned stride, VertexBuffer* instance_vertex_buffer, unsigned instance_offset, unsigned instance_stride) override final;

		virtual void Draw(unsigned vertex_count, unsigned instance_count, unsigned vertex_start, unsigned instance_start) override final;
		virtual void DrawIndexed(unsigned index_count, unsigned instance_count, unsigned index_start, unsigned vertex_offset, unsigned instance_start) override final;
		virtual void Dispatch(unsigned group_count_x, unsigned group_count_y = 1, unsigned group_count_z = 1) override final;

		Gnmx::LightweightGfxContext& GetGfxContext() { return gfx_contexts[current_back_buffer]; }

		void Reset();
		void Flush();
	};

}
