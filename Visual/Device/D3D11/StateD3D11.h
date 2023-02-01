
#pragma once

struct IDXGISwapChain;

namespace Device
{
	class Shader;
	class RenderTarget;
	class VertexDeclaration;
	class StructuredBufferD3D11;

	DXGI_FORMAT ConvertPixelFormat(PixelFormat format, bool useSRGB);
	PixelFormat UnconvertPixelFormat(DXGI_FORMAT format);

	struct ClearD3D11
	{
		DirectX::PackedVector::XMCOLOR color;
		UINT flags;
		float z;
		bool clear_color;
		bool clear_depth_stencil;
	};

	struct PassD3D11 : public Pass
	{
		std::array<ID3D11RenderTargetView*, RenderTargetSlotCount> rtvs;
		ID3D11DepthStencilView* dsv = nullptr;

		ClearD3D11 clear;

		PassD3D11(const Memory::DebugStringA<>& name, IDevice* device, const ColorRenderTargets& color_render_targets, RenderTarget* depth_stencil, ClearTarget clear_flags, simd::color clear_color, float clear_z);
	};

	class BindingSetD3D11 : public BindingSet {
	public:
		BindingSetD3D11(const Memory::DebugStringA<>& name, IDevice* device, Shader* vertex_shader, Shader* pixel_shader, const Inputs& inputs)
			: BindingSet(name, device, vertex_shader, pixel_shader, inputs) 
		{}

		BindingSetD3D11(const Memory::DebugStringA<>& name, IDevice* device, Shader* compute_shader, const Inputs& inputs)
			: BindingSet(name, device, compute_shader, inputs)
		{}
	};

	struct PipelineD3D11 : public Pipeline
	{
		D3D11_PRIMITIVE_TOPOLOGY primitive_topology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
		ID3D11VertexShader* _vertex_shader = nullptr;
		ID3D11PixelShader* _pixel_shader = nullptr;
		ID3D11ComputeShader* _compute_shader = nullptr;
		std::unique_ptr<ID3D11InputLayout, Utility::GraphicsComReleaser> input_layout;
		std::unique_ptr<ID3D11BlendState, Utility::GraphicsComReleaser> blend_state;
		std::unique_ptr<ID3D11RasterizerState, Utility::GraphicsComReleaser> rasterizer_state;
		std::unique_ptr<ID3D11DepthStencilState, Utility::GraphicsComReleaser> depth_stencil_state;
		DWORD stencil_ref = 0;

		PipelineD3D11(const Memory::DebugStringA<>& name, IDevice* device, const Pass* pass, PrimitiveType primitive_type, const VertexDeclaration* vertex_declaration, Shader* vertex_shader, Shader* pixel_shader, const BlendState& blend_state, const RasterizerState& rasterizer_state, const DepthStencilState& depth_stencil_state);
		PipelineD3D11(const Memory::DebugStringA<>& name, IDevice* device, Shader* compute_shader);

		virtual bool IsValid() const { return IsGraphicPipeline() ? _vertex_shader && _pixel_shader : _compute_shader; }
	};

	struct UniformOffsetsD3D11 : public UniformOffsets
	{
		struct BufferD3D11
		{
			ID3D11Buffer* buffer = nullptr;
			unsigned offset = 0;
			unsigned count = 0;
		};

		static Offset Allocate(IDevice* device, size_t size);
	};

	struct DescriptorSetD3D11: public DescriptorSet
	{
		std::array<ID3D11ShaderResourceView*, TextureSlotCount> vs_srvs;
		std::array<ID3D11ShaderResourceView*, TextureSlotCount> ps_srvs;
		std::array<ID3D11ShaderResourceView*, TextureSlotCount> cs_srvs;
		std::array<ID3D11UnorderedAccessView*, UAVSlotCount> graphics_uavs; // UAVs are shared between VS and PS
		std::array<ID3D11UnorderedAccessView*, UAVSlotCount> cs_uavs;
		std::array<StructuredBufferD3D11*, UAVSlotCount> uav_counter_buffers;

		DescriptorSetD3D11(const Memory::DebugStringA<>& name, IDevice* device, Pipeline* pipeline, const BindingSets& binding_sets, const DynamicBindingSets& dynamic_binding_sets);
	};

	class CommandBufferD3D11 : public CommandBuffer
	{
		IDevice* device = nullptr;
		unsigned num_bound_rtvs;
        CommandListD3D11 command_list;

		std::unique_ptr<ID3D11DeviceContext, Utility::GraphicsComReleaser> deferred_context;
		std::unique_ptr<ID3D11CommandList, Utility::GraphicsComReleaser> deferred_command_list;

		static_assert(UniformBufferSlotCount <= D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT);
		static_assert(SamplerSlotCount <= D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT);
		static_assert(TextureSlotCount <= D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT);
		
		void PushDynamicUniformBuffers(DynamicUniformBuffer* uniform_buffer);
		void PushUniformBuffers(Shader* vertex_shader, Shader* pixel_shader, const UniformOffsets* uniform_offsets);
		void PushUniformBuffers(Shader* compute_shader, const UniformOffsets* uniform_offsets);

	public:
		CommandBufferD3D11(const Memory::DebugStringA<>& name, IDevice* device);

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

		void Flush();
		void UpdateSubresource(ID3D11Resource* resource, UINT level, const void* pSrcData, UINT SrcRowPitch, UINT SrcDepthPitch);
	};

}
