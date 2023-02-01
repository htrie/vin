#pragma once

struct IDXGISwapChain;
struct ID3D11RasterizerState;
struct ID3D11DepthStencilState;
struct ID3D11BlendState;
struct ID3D11RenderTargetView;
struct ID3D11DepthStencilView;
struct ID3D11SamplerState;
struct ID3D11InputLayout;
struct ID3D11Buffer;
struct ID3D11VertexShader;
struct ID3D11PixelShader;

namespace Device
{

	struct ConstantBuffers1
	{
		ID3D11Buffer* buffers[UniformBufferSlotCount];
		UINT offsets[UniformBufferSlotCount];
		UINT counts[UniformBufferSlotCount];
		unsigned int count;

		ConstantBuffers1()
			: count(0) {}

		void Add(unsigned index, ID3D11Buffer* buffer, unsigned offset, unsigned count)
		{
			buffers[index] = buffer;
			offsets[index] = offset;
			counts[index] = count;
			this->count++;
		}
	};
	

	class CommandListD3D11
	{
	public:
		CommandListD3D11() {}
		CommandListD3D11(ID3D11Device* device);

		void PushUpdateSubresource(ID3D11Resource* resource, UINT level, const void* pSrcData, UINT SrcRowPitch, UINT SrcDepthPitch);
		void PushSetRenderTargets(unsigned rtv_count, ID3D11RenderTargetView** rtvs, ID3D11DepthStencilView* dsv, bool clear_color, bool clear_depth_stencil, DirectX::PackedVector::XMCOLOR color, UINT depth_stencil_flags, float Z);
		void PushSetViewports(unsigned int count, D3D11_VIEWPORT* viewports);
		void PushSetScissorRects(unsigned int count, D3D11_RECT* rects);
		void PushSetPipeline(D3D11_PRIMITIVE_TOPOLOGY primitive_type, ID3D11InputLayout* layout, ID3D11VertexShader* vertex_shader, ID3D11PixelShader* pixel_shader, ID3D11BlendState* blend_state, ID3D11RasterizerState* rasterizer_state, ID3D11DepthStencilState* depth_stencil_state, DWORD stencil_ref);
		void PushSetPipeline(ID3D11ComputeShader* compute_shader);
		void PushSetSamplerStateVS(ID3D11SamplerState* sampler_state, unsigned index);
		void PushSetSamplerStatePS(ID3D11SamplerState* sampler_state, unsigned index);
		void PushSetSamplerStateCS(ID3D11SamplerState* sampler_state, unsigned index);
		void PushSetSRVsVS(unsigned count, ID3D11ShaderResourceView** srvs);
		void PushSetSRVsPS(unsigned count, ID3D11ShaderResourceView** srvs);
		void PushSetSRVsCS(unsigned count, ID3D11ShaderResourceView** srvs);
		void PushSetUAVsGraphic(unsigned start_slot, unsigned count, ID3D11UnorderedAccessView** uavs, const UINT* initial_counts);
		void PushSetUAVsCS(unsigned count, ID3D11UnorderedAccessView** uavs, const UINT* initial_counts);
		void PushSetConstantBufferVS(ID3D11Buffer* buffer, unsigned index);
		void PushSetConstantBufferPS(ID3D11Buffer* buffer, unsigned index);
		void PushSetConstantBufferCS(ID3D11Buffer* buffer, unsigned index);
		void PushSetConstantBuffersVS1(ConstantBuffers1& constant_buffers1);
		void PushSetConstantBuffersPS1(ConstantBuffers1& constant_buffers1);
		void PushSetConstantBuffersCS1(ConstantBuffers1& constant_buffers1);
		void PushSetVertexBuffer(ID3D11Buffer* vertex_buffer, unsigned offset, unsigned stride, ID3D11Buffer* instance_vertex_buffer, unsigned instance_offset, unsigned instance_stride);
		void PushSetIndexBuffer(ID3D11Buffer* index_buffer, DXGI_FORMAT index_format);
		void PushDraw(UINT vertex_count, UINT instance_count, UINT start_vertex, UINT start_instance);
		void PushDrawIndexedInstanced(UINT index_count, UINT instance_count, UINT start_index, INT base_vertex, UINT start_instance);
		void PushDispatch(UINT group_count_x, UINT group_count_y, UINT group_count_z);
		void PushClearState();

		void Clear();

		void Flush(ID3D11DeviceContext* context);

	private:
		Utility::StackList commands;
	};

}
