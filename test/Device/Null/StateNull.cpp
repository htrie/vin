namespace Device
{

	PassNull::PassNull(const Memory::DebugStringA<>& name, IDevice* device, const ColorRenderTargets& color_render_targets, RenderTarget* depth_stencil, ClearTarget clear_flags, simd::color clear_color, float _clear_z)
		: Pass(name, color_render_targets, depth_stencil)
	{
	}

	PipelineNull::PipelineNull(const Memory::DebugStringA<>& name, IDevice* device, const Pass* pass, PrimitiveType primitive_type, const VertexDeclaration* vertex_declaration, Shader* vertex_shader, Shader* pixel_shader, const BlendState& blend_state, const RasterizerState& rasterizer_state, const DepthStencilState& depth_stencil_state)
		: Pipeline(name, vertex_declaration, vertex_shader, pixel_shader)
	{
	}

	PipelineNull::PipelineNull(const Memory::DebugStringA<>& name, IDevice* device, Shader* compute_shader)
		: Pipeline(name, compute_shader)
	{
	}

	DescriptorSetNull::DescriptorSetNull(const Memory::DebugStringA<>& name, IDevice* device, Pipeline* pipeline, const BindingSets& binding_sets, const DynamicBindingSets& dynamic_binding_sets)
		: DescriptorSet(name, pipeline, binding_sets, dynamic_binding_sets)
	{
		PROFILE_ONLY(ComputeStats();)
	}

	CommandBufferNull::CommandBufferNull(const Memory::DebugStringA<>& name, IDevice* device)
		: CommandBuffer(name), device(device)
	{
	}

	bool CommandBufferNull::Begin()
	{
		return true;
	}

	void CommandBufferNull::End()
	{
	}

	void CommandBufferNull::BeginPass(Pass* pass, Rect viewport_rect, Rect scissor_rect, ClearTarget clear_flags, simd::color clear_color, float clear_z)
	{
	}

	void CommandBufferNull::EndPass()
	{
	}

	void CommandBufferNull::BeginComputePass()
	{
	}

	void CommandBufferNull::EndComputePass()
	{
	}

	bool CommandBufferNull::BindPipeline(Pipeline* pipeline)
	{
		return false;
	}

	void CommandBufferNull::BindDescriptorSet(DescriptorSet* descriptor_set, const UniformOffsets* uniform_offsets, const DynamicUniformBuffers& dynamic_uniform_buffers)
	{
	}

	void CommandBufferNull::BindBuffers(IndexBuffer* index_buffer, VertexBuffer* vertex_buffer, unsigned offset, unsigned stride, VertexBuffer* instance_vertex_buffer, unsigned instance_offset, unsigned instance_stride)
	{
	}

	void CommandBufferNull::Draw(unsigned vertex_count, unsigned instance_count, unsigned vertex_start, unsigned instance_start)
	{
	}

	void CommandBufferNull::DrawIndexed(unsigned index_count, unsigned instance_count, unsigned index_start, unsigned vertex_offset, unsigned instance_start)
	{
	}

	void CommandBufferNull::Dispatch(unsigned group_count_x, unsigned group_count_y, unsigned group_count_z)
	{
	}

	//void CommandBufferNull::SetCounter(StructuredBuffer* buffer, uint32_t value)
	//{
	//}

	void CommandBufferNull::SetScissor(Rect rect)
	{
	}

}

