#pragma once

#include "Visual/Device/State.h"
#include "Visual/Device/Shader.h"
#include "Visual/Device/VertexDeclaration.h"

namespace Device
{
	class IDevice;
	class RenderTarget;
	class Shader;
	class VertexBuffer;
	class VertexDeclaration;
}

namespace Renderer
{
	class TargetResampler
	{
	public:
		TargetResampler();

		void OnCreateDevice(Device::IDevice* device);
		void OnResetDevice(Device::IDevice* device);
		void OnLostDevice();
		void OnDestroyDevice();
		
		enum struct FilterTypes
		{
			Avg,
			Max,
			MinMax,
			Nearest,
			Mixed
		};

		enum GBufferUpsamplingBlendMode
		{
			Default,
			Additive
		};

		typedef Memory::Vector<Device::RenderTarget*, Memory::Tag::Render> RenderTargetList;

		void DepthAwareDownsample(
			Device::CommandBuffer& command_buffer,
			Device::RenderTarget* zero_depth,
			const RenderTargetList& pixel_offset_targets,
			simd::vector2 frame_to_dynamic_scale);

		void BuildDownsampled(
			Device::CommandBuffer& command_buffer,
			Device::RenderTarget* zero_mip_data,
			Device::Handle<Device::Texture> pixel_offsets,
			Device::RenderTarget* dst_mip_target,
			simd::vector2 frame_to_dynamic_scale);

		void BuildDownsampled(
			Device::CommandBuffer& command_buffer,
			Device::RenderTarget* zero_mip_data,
			Device::RenderTarget* dst_mip_target,
			simd::vector2 frame_to_dynamic_scale);

		void DepthAwareUpsample(
			Device::CommandBuffer& command_buffer,
			Device::RenderTarget* zero_depth,
			const RenderTargetList& pixel_offset_targets,
			const RenderTargetList& color_targets,
			simd::vector2 frame_to_dynamic_scale,
			simd::matrix inv_proj_matrix,
			GBufferUpsamplingBlendMode final_target_blendmode,
			size_t src_mip, size_t dst_mip);

		void DepthAwareUpsample(
			Device::CommandBuffer& command_buffer,
			Device::RenderTarget* zero_depth,
			Device::RenderTarget* zero_normal,
			const RenderTargetList& color_targets,
			simd::vector2 frame_to_dynamic_scale,
			simd::matrix inv_proj_matrix,
			GBufferUpsamplingBlendMode final_target_blendmode,
			size_t src_mip, size_t dst_mip);

		void DepthAwareUpsample(
			Device::CommandBuffer& command_buffer,
			Device::RenderTarget* zero_depth,
			const RenderTargetList& color_targets,
			simd::vector2 frame_to_dynamic_scale,
			simd::matrix inv_proj_matrix,
			GBufferUpsamplingBlendMode final_target_blendmode,
			size_t src_mip, size_t dst_mip);

		//supports resampling mips of the same texture, but only on dx11
		void ResampleColor(
			Device::CommandBuffer& command_buffer,
			Device::RenderTarget * src_target,
			Device::RenderTarget * dst_target,
			simd::vector2 dynamic_resolution_scale = simd::vector2(1.0f, 1.0f),
			FilterTypes filter_type = FilterTypes::Nearest,
			float gamma = 1.0f);

		//does not support resampling of mips of the same texture, but works on dx9
		void ResampleMip(
			Device::CommandBuffer& command_buffer,
			Device::RenderTarget * src_target,
			Device::RenderTarget * dst_target,
			simd::vector2 dynamic_resolution_scale = simd::vector2(1.0f, 1.0f),
			FilterTypes filter_type = FilterTypes::Nearest,
			float gamma = 1.0f);

		void ResampleDepth(
			Device::CommandBuffer& command_buffer,
			Device::RenderTarget * src_target,
			Device::RenderTarget * dst_target,
			simd::vector2 dynamic_resolution_scale = simd::vector2(1.0f, 1.0f),
			FilterTypes filter_type = FilterTypes::Nearest);

		void reload();
	private:

		void DepthAwareDownsampleInternal(
			Device::CommandBuffer& command_buffer,
			Device::RenderTarget* zero_depth,
			Device::Handle<Device::Texture> pixel_offset_src,
			Device::RenderTarget* pixel_offset_dst,
			simd::vector2 frame_to_dynamic_scale);

		void DepthAwareUpsampleInternal(
			Device::CommandBuffer& command_buffer,
			Device::RenderTarget* zero_depth,
			Device::RenderTarget* zero_normal,
			Device::Handle<Device::Texture> src_pixel_offsets,
			Device::Handle<Device::Texture> dst_pixel_offsets,
			Device::Handle<Device::Texture> src_data,
			Device::RenderTarget* dst_data,
			simd::vector2 frame_to_dynamic_scale,
			simd::matrix inv_proj_matrix,
			GBufferUpsamplingBlendMode blendmode
		);

		void ResampleInternal(
			Device::CommandBuffer& command_buffer,
			Device::RenderTarget * src_target,
			Device::RenderTarget * dst_target,
			simd::vector2 dynamic_resolution_scale,
			int src_mip_level,
			FilterTypes filter_type,
			float gamma);

		struct DepthAwareUpsamplerDesc
		{
			DepthAwareUpsamplerDesc(bool use_offset_tex, bool use_normal, size_t dst_mip_level) :
				use_offset_tex(use_offset_tex), use_normal(use_normal), dst_mip_level(dst_mip_level)
			{
			}
			bool use_offset_tex;
			bool use_normal;
			size_t dst_mip_level;
			bool operator < (const DepthAwareUpsamplerDesc &other) const
			{
				return std::tie(use_offset_tex, use_normal, dst_mip_level) < std::tie(other.use_offset_tex, other.use_normal, other.dst_mip_level);
			}
		};

		struct DepthAwareDownsamplerOperationDesc
		{
			bool out_depth = false;
			
			bool operator < (const DepthAwareDownsamplerOperationDesc &op) const
			{
				return std::tie(out_depth) <
					   std::tie(op.out_depth);
			}
		};

		struct BuildDownsampledOperationDesc
		{
			bool out_depth = false;
			bool use_offset_tex = false;

			bool operator < (const BuildDownsampledOperationDesc& op) const
			{
				return std::tie(out_depth, use_offset_tex) <
					std::tie(op.out_depth, op.use_offset_tex);
			}
		};

		struct OperationDesc
		{
			enum struct ResamplingTypes
			{
				Upsample,
				Downsample,
				Copy
			};
			ResamplingTypes resampling_type;
			FilterTypes filter_type;
			bool out_depth;
			bool is_dst_fp32_r = false;
			bool is_dst_fp32_gr = false;


			bool operator < (const OperationDesc &op) const
			{
				if (resampling_type != op.resampling_type)
					return resampling_type < op.resampling_type;
				if (filter_type != op.filter_type)
					return filter_type < op.filter_type;
				if (out_depth != op.out_depth)
					return out_depth < op.out_depth;
				if (is_dst_fp32_r != op.is_dst_fp32_r)
					return is_dst_fp32_r < op.is_dst_fp32_r;
				if (is_dst_fp32_gr != op.is_dst_fp32_gr)
					return is_dst_fp32_gr < op.is_dst_fp32_gr;
				return false;
			}
		};

		struct BuildDownsampledShader
		{
			Device::Handle<Device::Shader> pixel_shader;
		};

		struct DepthAwareUpsamplerShader
		{
			Device::Handle<Device::Shader> pixel_shader;
		};

		struct OperationShader
		{
			Device::Handle<Device::Shader> pixel_shader;
		};

		struct DepthAwareDownsamplerShader
		{
			Device::Handle<Device::Shader> pixel_shader;
		};

		void LoadDepthAwareUpsamplerShader(DepthAwareUpsamplerShader &depth_aware_upsampler, const DepthAwareUpsamplerDesc& desc);
		void LoadOperationShader(OperationShader &out_shader, OperationDesc desc);
		void LoadDepthAwareDownsamplerShader(DepthAwareDownsamplerShader& out_shader, const DepthAwareDownsamplerOperationDesc& desc);
		void LoadBuildDownsampledShader(BuildDownsampledShader& out_shader, const BuildDownsampledOperationDesc& desc);

		std::map<DepthAwareUpsamplerDesc, DepthAwareUpsamplerShader> depth_aware_upsampler_shaders;
		std::map<DepthAwareDownsamplerOperationDesc, DepthAwareDownsamplerShader> depth_aware_downsampler_shaders;
		std::map<BuildDownsampledOperationDesc, BuildDownsampledShader> build_downsampled_shaders;
		std::map<OperationDesc, OperationShader> shaders;

	#pragma pack(push)
	#pragma pack(1)
		struct PassKey
		{
			Device::ColorRenderTargets render_targets;
			Device::PointerID<Device::RenderTarget> depth_stencil;

			PassKey() {}
			PassKey(Device::ColorRenderTargets render_targets, Device::RenderTarget* depth_stencil);
		};
	#pragma pack(pop)
		Device::Cache<PassKey, Device::Pass> passes;
		Memory::Mutex passes_mutex;
		Device::Pass* FindPass(const Memory::DebugStringA<>& name, Device::ColorRenderTargets render_targets, Device::RenderTarget* depth_stencil);

	#pragma pack(push)	
	#pragma pack(1)
		struct PipelineKey
		{
			Device::PointerID<Device::Pass> pass;
			Device::PointerID<Device::Shader> pixel_shader;
			Device::BlendState blend_state;
			Device::RasterizerState rasterizer_state;
			Device::DepthStencilState depth_stencil_state;

			PipelineKey() {}
			PipelineKey(Device::Pass* pass, Device::Shader* pixel_shader, const Device::BlendState& blend_state, const Device::RasterizerState& rasterizer_state, const Device::DepthStencilState& depth_stencil_state);
		};
	#pragma pack(pop)
		Device::Cache<PipelineKey, Device::Pipeline> pipelines;
		Memory::Mutex pipelines_mutex;
		Device::Pipeline* FindPipeline(const Memory::DebugStringA<>& name, Device::Pass* pass, Device::Shader* pixel_shader, const Device::BlendState& blend_state, const Device::RasterizerState& rasterizer_state, const Device::DepthStencilState& depth_stencil_state);

	#pragma pack(push)
	#pragma pack(1)
		struct BindingSetKey
		{
			Device::PointerID<Device::Shader> shader;
			uint32_t inputs_hash = 0;

			BindingSetKey() {}
			BindingSetKey(Device::Shader* shader, uint32_t inputs_hash);
		};
	#pragma pack(pop)
		Device::Cache<BindingSetKey, Device::DynamicBindingSet> binding_sets;
		Memory::Mutex binding_sets_mutex;
		Device::DynamicBindingSet* FindBindingSet(const Memory::DebugStringA<>& name, Device::IDevice* device, Device::Shader* pixel_shader, const Device::DynamicBindingSet::Inputs& inputs);

	#pragma pack(push)
	#pragma pack(1)
		struct DescriptorSetKey
		{
			Device::PointerID<Device::Pipeline> pipeline;
			Device::PointerID<Device::DynamicBindingSet> pixel_binding_set;
			uint32_t samplers_hash = 0;

			DescriptorSetKey() {}
			DescriptorSetKey(Device::Pipeline* pipeline, Device::DynamicBindingSet* pixel_binding_set, uint32_t samplers_hash);
		};
	#pragma pack(pop)
		Device::Cache<DescriptorSetKey, Device::DescriptorSet> descriptor_sets;
		Memory::Mutex descriptor_sets_mutex;
		Device::DescriptorSet* FindDescriptorSet(const Memory::DebugStringA<>& name, Device::Pipeline* pipeline, Device::DynamicBindingSet* pixel_binding_set);

	#pragma pack(push)
	#pragma pack(1)
		struct UniformBufferKey
		{
			Device::PointerID<Device::Shader> pixel_shader;
			Device::PointerID<Device::RenderTarget> dst;

			UniformBufferKey() {}
			UniformBufferKey(Device::Shader* pixel_shader, Device::RenderTarget* dst);
		};
	#pragma pack(pop)
		Device::Cache<UniformBufferKey, Device::DynamicUniformBuffer> uniform_buffers;
		Memory::Mutex uniform_buffers_mutex;
		Device::DynamicUniformBuffer* FindUniformBuffer(const Memory::DebugStringA<>& name, Device::IDevice* device, Device::Shader* pixel_shader, Device::RenderTarget* dst);

		Device::IDevice *device;

		Device::VertexDeclaration vertex_declaration;

		Device::Handle<Device::VertexBuffer> vertex_buffer;
		Device::Handle<Device::Shader> vertex_shader;
	};
}
