#pragma once

namespace Device
{
	struct PassNull : public Pass
	{
		PassNull(const Memory::DebugStringA<>& name, IDevice* device, const ColorRenderTargets& color_render_targets, RenderTarget* depth_stencil, ClearTarget clear_flags, simd::color clear_color, float clear_z);
	};

	struct PipelineNull : public Pipeline
	{
		PipelineNull(const Memory::DebugStringA<>& name, IDevice* device, const Pass* pass, PrimitiveType primitive_type, const VertexDeclaration* vertex_declaration, Shader* vertex_shader, Shader* pixel_shader, const BlendState& blend_state, const RasterizerState& rasterizer_state, const DepthStencilState& depth_stencil_state);
		PipelineNull(const Memory::DebugStringA<>& name, IDevice* device, Shader* compute_shader);

		virtual bool IsValid() const { return true; }
	};

	class BindingSetNull : public BindingSet {
	public:
		BindingSetNull(const Memory::DebugStringA<>& name, IDevice* device, Shader* vertex_shader, Shader* pixel_shader, const Inputs& inputs)
			: BindingSet(name, device, vertex_shader, pixel_shader, inputs)
		{}

		BindingSetNull(const Memory::DebugStringA<>& name, IDevice* device, Shader* compute_shader, const Inputs& inputs)
			: BindingSet(name, device, compute_shader, inputs)
		{}
	};

	struct UniformOffsetsNull : public UniformOffsets
	{
		static Offset Allocate(IDevice* device, size_t size) { return {}; }
	};

	struct DescriptorSetNull: public DescriptorSet
	{
		DescriptorSetNull(const Memory::DebugStringA<>& name, IDevice* device, Pipeline* pipeline, const BindingSets& binding_sets, const DynamicBindingSets& dynamic_binding_sets);
	};

	class CommandBufferNull : public CommandBuffer
	{
		IDevice* device = nullptr;

	public:
		CommandBufferNull(const Memory::DebugStringA<>& name, IDevice* device);

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
	};

}