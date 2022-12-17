

#include "Visual/Utility/DXHelperFunctions.h"
#include "Visual/Device/Device.h"
#include "Visual/Device/Texture.h"
#include "Visual/Device/VertexDeclaration.h"
#include "Visual/Device/VertexBuffer.h"
#include "Visual/Device/RenderTarget.h"
#include "Visual/Device/Shader.h"
#include "Visual/Device/Buffer.h"
#include "Visual/Device/Compiler.h"

#include "TargetResampler.h"
#include "CachedHLSLShader.h"
#include "Utility.h"


namespace Renderer
{
	namespace
	{
		const static Device::VertexElements vertex_elements =
		{
			{ 0, 0,					Device::DeclType::FLOAT4, Device::DeclUsage::POSITION, 0 },
		};

		const char* MAIN_RESAMPLE = "Resample";

#pragma pack( push, 1 )
		struct Vertex
		{
			simd::vector4 pos;
		};
#pragma pack( pop )

		const Vertex vertex_data[4] =
		{
			{ simd::vector4(-1.f, -1.f, 0.0f, 1.0f) },
			{ simd::vector4(-1.f, 1.f, 0.0f, 1.0f) },
			{ simd::vector4(1.f, -1.f, 0.0f, 1.0f) },
			{ simd::vector4(1.f, 1.f, 0.0f, 1.0f) }
		};
	}

	TargetResampler::PassKey::PassKey(Device::ColorRenderTargets render_targets, Device::RenderTarget* depth_stencil)
		: render_targets(render_targets), depth_stencil(depth_stencil)
	{}

	TargetResampler::PipelineKey::PipelineKey(Device::Pass* pass, Device::Shader* pixel_shader, const Device::BlendState& blend_state, const Device::RasterizerState& rasterizer_state, const Device::DepthStencilState& depth_stencil_state)
		: pass(pass), pixel_shader(pixel_shader), blend_state(blend_state), rasterizer_state(rasterizer_state), depth_stencil_state(depth_stencil_state)
	{}

	TargetResampler::BindingSetKey::BindingSetKey(Device::Shader * shader, uint32_t inputs_hash)
		: shader(shader), inputs_hash(inputs_hash)
	{}

	TargetResampler::DescriptorSetKey::DescriptorSetKey(Device::Pipeline* pipeline, Device::DynamicBindingSet* pixel_binding_set, uint32_t samplers_hash)
		: pipeline(pipeline), pixel_binding_set(pixel_binding_set), samplers_hash(samplers_hash)
	{}

	TargetResampler::UniformBufferKey::UniformBufferKey(Device::Shader * pixel_shader, Device::RenderTarget* dst)
		: pixel_shader(pixel_shader), dst(dst)
	{}

	TargetResampler::TargetResampler()
		: vertex_declaration(vertex_elements)
	{
		device=NULL;
	}

	Device::Pass* TargetResampler::FindPass(const Memory::DebugStringA<>& name, Device::ColorRenderTargets render_targets, Device::RenderTarget* depth_stencil)
	{
		Memory::Lock lock(passes_mutex);
		return passes.FindOrCreate(PassKey(render_targets, depth_stencil), [&]()
		{
			return Device::Pass::Create(name, device, render_targets, depth_stencil, Device::ClearTarget::NONE, simd::color(0), 0.0f);
		}).Get();
	}

	Device::Pipeline* TargetResampler::FindPipeline(const Memory::DebugStringA<>& name, Device::Pass* pass, Device::Shader* pixel_shader, const Device::BlendState& blend_state, const Device::RasterizerState& rasterizer_state, const Device::DepthStencilState& depth_stencil_state)
	{
		Memory::Lock lock(pipelines_mutex);
		return pipelines.FindOrCreate(PipelineKey(pass, pixel_shader, blend_state, rasterizer_state, depth_stencil_state), [&]()
		{
			return Device::Pipeline::Create(name, device, pass, Device::PrimitiveType::TRIANGLESTRIP, &vertex_declaration, vertex_shader.Get(), pixel_shader, blend_state, rasterizer_state, depth_stencil_state);
		}).Get();
	}

	Device::DynamicBindingSet* TargetResampler::FindBindingSet(const Memory::DebugStringA<>& name, Device::IDevice* device, Device::Shader* pixel_shader, const Device::DynamicBindingSet::Inputs& inputs)
	{
		Memory::Lock lock(binding_sets_mutex);
		return binding_sets.FindOrCreate(BindingSetKey(pixel_shader, inputs.Hash()), [&]()
		{
			return Device::DynamicBindingSet::Create(name, device, pixel_shader, inputs);
		}).Get();
	}

	Device::DescriptorSet* TargetResampler::FindDescriptorSet(const Memory::DebugStringA<>& name, Device::Pipeline* pipeline, Device::DynamicBindingSet* pixel_binding_set)
	{
		Memory::Lock lock(descriptor_sets_mutex);
		return descriptor_sets.FindOrCreate(DescriptorSetKey(pipeline, pixel_binding_set, device->GetSamplersHash()), [&]()
		{
			return Device::DescriptorSet::Create(name, device, pipeline, {}, { pixel_binding_set });
		}).Get();
	}

	Device::DynamicUniformBuffer* TargetResampler::FindUniformBuffer(const Memory::DebugStringA<>& name, Device::IDevice* device, Device::Shader* pixel_shader, Device::RenderTarget* dst)
	{
		Memory::Lock lock(uniform_buffers_mutex);
		return uniform_buffers.FindOrCreate(UniformBufferKey(pixel_shader, dst), [&]()
		{
			return Device::DynamicUniformBuffer::Create(name, device, pixel_shader);
		}).Get();
	}

	void TargetResampler::OnCreateDevice(Device::IDevice* device)
	{
		this->device = device;

		Device::MemoryDesc init_mem = { vertex_data, 0, 0 };
		vertex_buffer = Device::VertexBuffer::Create("VB TargetResampler", device, sizeof(Vertex) * 4, Device::UsageHint::IMMUTABLE, Device::Pool::DEFAULT, &init_mem);

		reload();
	}

	void TargetResampler::OnResetDevice(Device::IDevice* device)
	{
	}

	void TargetResampler::OnLostDevice()
	{
		uniform_buffers.Clear();
		binding_sets.Clear();
		descriptor_sets.Clear();
		pipelines.Clear();
		passes.Clear();
	}

	void TargetResampler::OnDestroyDevice()
	{
		device=NULL;

		vertex_buffer.Reset();
		vertex_shader.Reset();

		depth_aware_downsampler_shaders.clear();
		depth_aware_upsampler_shaders.clear();
		shaders.clear();
		build_downsampled_shaders.clear();
	}

	void TargetResampler::ResampleColor(
		Device::CommandBuffer& command_buffer,
		Device::RenderTarget * src_target,
		Device::RenderTarget * dst_target,
		simd::vector2 dynamic_resolution_scale,
		FilterTypes filter_type,
		float gamma)
	{
		ResampleInternal(command_buffer, src_target, dst_target, dynamic_resolution_scale, 0, filter_type, gamma);
	}

	void TargetResampler::ResampleMip(
		Device::CommandBuffer& command_buffer,
		Device::RenderTarget * src_target,
		Device::RenderTarget * dst_target,
		simd::vector2 dynamic_resolution_scale,
		FilterTypes filter_type,
		float gamma)
	{
		ResampleInternal(command_buffer, src_target, dst_target, dynamic_resolution_scale, static_cast< int >( src_target->GetTextureMipLevel() ), filter_type, gamma);
	}

	void TargetResampler::ResampleDepth(
		Device::CommandBuffer& command_buffer,
		Device::RenderTarget * src_target,
		Device::RenderTarget * dst_target,
		simd::vector2 dynamic_resolution_scale,
		FilterTypes filter_type)
	{
		ResampleInternal(command_buffer, src_target, dst_target, dynamic_resolution_scale, 0, filter_type, 1.0f);
	}

	void TargetResampler::DepthAwareDownsample(
		Device::CommandBuffer& command_buffer,
		Device::RenderTarget* zero_depth,
		const RenderTargetList& pixel_offset_targets,
		simd::vector2 frame_to_dynamic_scale)
	{
		for (size_t i = 1; i < pixel_offset_targets.size(); i++)
		{
			DepthAwareDownsampleInternal(command_buffer, zero_depth, pixel_offset_targets[i - 1]->GetTexture(), pixel_offset_targets[i], frame_to_dynamic_scale);
		}
	}

	size_t CalcMipFromSize(simd::vector2_int zero_size, simd::vector2_int mip_size)
	{
		size_t mip_index = 0;
		for (; zero_size.x / (1 << mip_index) > mip_size.x; mip_index++);
		assert(zero_size.y / (1 << mip_index) == mip_size.y);
		return mip_index;
	}

	void TargetResampler::BuildDownsampled(
		Device::CommandBuffer& command_buffer,
		Device::RenderTarget *zero_mip_data,
		Device::Handle<Device::Texture> pixel_offsets,
		Device::RenderTarget* dst_mip_target,
		simd::vector2 frame_to_dynamic_scale)
	{
		BuildDownsampledOperationDesc op_desc;
		assert(dst_mip_target->IsDepthTexture() == zero_mip_data->IsDepthTexture());
		op_desc.out_depth = zero_mip_data->IsDepthTexture();
		op_desc.use_offset_tex = !!pixel_offsets.Get();

		Device::ColorRenderTargets render_targets;
		if (!dst_mip_target->IsDepthTexture())
			render_targets.push_back(dst_mip_target);
		Device::RenderTarget* depth_stencil = dst_mip_target->IsDepthTexture() ? dst_mip_target : nullptr;
		auto* pass = FindPass("Downsample", render_targets, depth_stencil);
		command_buffer.BeginPass(pass, dst_mip_target->GetSize(), dst_mip_target->GetSize());

		auto it = build_downsampled_shaders.find(op_desc);
		assert(it != build_downsampled_shaders.end());
		auto& shader = it->second;

		auto* pixel_uniform_buffer = FindUniformBuffer("Downsample", device, shader.pixel_shader.Get(), dst_mip_target);

		auto* pipeline = FindPipeline("Downsample", pass, shader.pixel_shader.Get(),
				Device::DisableBlendState(), Device::RasterizerState(Device::CullMode::NONE, Device::FillMode::SOLID, 0, 0),
				Device::DepthStencilState(Device::WriteOnlyDepthState(), Device::DisableStencilState()));
		if (command_buffer.BindPipeline(pipeline))
		{
			size_t dst_mip_level = CalcMipFromSize(zero_mip_data->GetSize(), dst_mip_target->GetSize());
			pixel_uniform_buffer->SetInt("dst_mip_level", uint32_t(dst_mip_level));
			simd::vector4 frame_to_dynamic_scale4 = { frame_to_dynamic_scale.x, frame_to_dynamic_scale.y, 0.0f, 0.0f };
			pixel_uniform_buffer->SetVector("frame_to_dynamic_scale", &frame_to_dynamic_scale4);
			pixel_uniform_buffer->SetFloat("viewport_width", float(zero_mip_data->GetWidth()));
			pixel_uniform_buffer->SetFloat("viewport_height", float(zero_mip_data->GetHeight()));

			Device::DynamicBindingSet::Inputs inputs;
			inputs.push_back({ "zero_data_tex", zero_mip_data->GetTexture().Get() });
			inputs.push_back({ "pixel_offset_tex", pixel_offsets.Get() });
			auto* pixel_binding_set = FindBindingSet("Downsample", device, shader.pixel_shader.Get(), inputs);

			auto* descriptor_set = FindDescriptorSet("Downsample", pipeline, pixel_binding_set);
			command_buffer.BindDescriptorSet(descriptor_set, {}, { pixel_uniform_buffer });

			command_buffer.BindBuffers(nullptr, vertex_buffer.Get(), 0, sizeof(Vertex));

			command_buffer.Draw(4, 0);
		}

		command_buffer.EndPass();
	}

	void TargetResampler::BuildDownsampled(
		Device::CommandBuffer& command_buffer,
		Device::RenderTarget* zero_mip_data,
		Device::RenderTarget* dst_mip_target,
		simd::vector2 frame_to_dynamic_scale)
	{
		BuildDownsampled(command_buffer, zero_mip_data, Device::Handle<Device::Texture>(), dst_mip_target, frame_to_dynamic_scale);
	}

	void TargetResampler::DepthAwareDownsampleInternal(
		Device::CommandBuffer& command_buffer,
		Device::RenderTarget* zero_depth,
		Device::Handle<Device::Texture>  pixel_offset_src,
		Device::RenderTarget* pixel_offset_dst,
		simd::vector2 frame_to_dynamic_scale)
	{
		DepthAwareDownsamplerOperationDesc op_desc;
		assert(zero_depth);
		assert(zero_depth->IsDepthTexture());

		op_desc.out_depth = false;

		Device::ColorRenderTargets render_targets = { pixel_offset_dst };
		Device::RenderTarget* depth_stencil = nullptr;
		auto* pass = FindPass("DepthAwareDownsample", render_targets, depth_stencil);
		command_buffer.BeginPass(pass, pixel_offset_dst->GetSize(), pixel_offset_dst->GetSize());

		auto it = depth_aware_downsampler_shaders.find(op_desc);
		assert(it != depth_aware_downsampler_shaders.end());
		auto &shader = it->second;

		auto* pixel_uniform_buffer = FindUniformBuffer("DepthAwareDownsample", device, shader.pixel_shader.Get(), pixel_offset_dst);

		auto* pipeline = FindPipeline("DepthAwareDownsample", pass, shader.pixel_shader.Get(),
			Device::DisableBlendState(), Device::RasterizerState(Device::CullMode::NONE, Device::FillMode::SOLID, 0, 0),
			Device::DepthStencilState(Device::WriteOnlyDepthState(), Device::DisableStencilState()));
		if (command_buffer.BindPipeline(pipeline))
		{
			size_t dst_mip_level = CalcMipFromSize(zero_depth->GetSize(), pixel_offset_dst->GetSize());
			pixel_uniform_buffer->SetInt("dst_mip_level", uint32_t(dst_mip_level));
			simd::vector4 dynamic_resolution_scale4 = { frame_to_dynamic_scale.x, frame_to_dynamic_scale.y, 0.0f, 0.0f };
			pixel_uniform_buffer->SetVector("frame_to_dynamic_scale", &dynamic_resolution_scale4);
			pixel_uniform_buffer->SetFloat("viewport_width", float(zero_depth->GetWidth()));
			pixel_uniform_buffer->SetFloat("viewport_height", float(zero_depth->GetHeight()));

			Device::DynamicBindingSet::Inputs inputs;
			inputs.push_back({ "zero_depth_tex", zero_depth->GetTexture().Get() });
			inputs.push_back({ "src_pixel_offset_tex", pixel_offset_src.Get() });
			auto* pixel_binding_set = FindBindingSet("DepthAwareDownsample", device, shader.pixel_shader.Get(), inputs);

			auto* descriptor_set = FindDescriptorSet("DepthAwareDownsample", pipeline, pixel_binding_set);
			command_buffer.BindDescriptorSet(descriptor_set, {}, { pixel_uniform_buffer });

			command_buffer.BindBuffers(nullptr, vertex_buffer.Get(), 0, sizeof(Vertex));

			command_buffer.Draw(4, 0);
		}

		command_buffer.EndPass();
	}

	void TargetResampler::DepthAwareUpsample(
		Device::CommandBuffer& command_buffer,
		Device::RenderTarget *zero_depth,
		const RenderTargetList& pixel_offset_targets,
		const RenderTargetList& color_targets,
		simd::vector2 frame_to_dynamic_scale,
		simd::matrix inv_proj_matrix,
		GBufferUpsamplingBlendMode final_target_blendmode,
		size_t src_mip, size_t dst_mip)
	{
		assert(pixel_offset_targets.size() > src_mip && pixel_offset_targets.size() > dst_mip);
		assert(src_mip > dst_mip); // source is higher downscale level
		
		for (size_t mip_index = 0; mip_index < src_mip - dst_mip; mip_index++)
		{
			size_t curr_src_mip = src_mip - mip_index;
			size_t curr_dst_mip = curr_src_mip - 1;
			auto blendmode = curr_dst_mip == dst_mip ? final_target_blendmode : GBufferUpsamplingBlendMode::Default; // todo: always blend
			DepthAwareUpsampleInternal(
				command_buffer,
				zero_depth,
				nullptr,
				pixel_offset_targets[curr_src_mip]->GetTexture(),
				pixel_offset_targets[curr_dst_mip]->GetTexture(),
				color_targets[curr_src_mip]->GetTexture(),
				color_targets[curr_dst_mip],
				frame_to_dynamic_scale, inv_proj_matrix, blendmode);
		}
	}

	void TargetResampler::DepthAwareUpsample(
		Device::CommandBuffer& command_buffer,
		Device::RenderTarget *zero_depth,
		Device::RenderTarget *zero_normal,
		const RenderTargetList& color_targets,
		simd::vector2 frame_to_dynamic_scale,
		simd::matrix inv_proj_matrix,
		GBufferUpsamplingBlendMode final_target_blendmode,
		size_t src_mip, size_t dst_mip)
	{
		assert(src_mip > dst_mip); // source is higher downscale level

		for (size_t mip_index = 0; mip_index < src_mip - dst_mip; mip_index++)
		{
			size_t curr_src_mip = src_mip - mip_index;
			size_t curr_dst_mip = curr_src_mip - 1;
			auto blendmode = curr_dst_mip == dst_mip ? final_target_blendmode : GBufferUpsamplingBlendMode::Default; // todo: always blend
			DepthAwareUpsampleInternal(
				command_buffer,
				zero_depth,
				zero_normal,
				Device::Handle<Device::Texture>(),
				Device::Handle<Device::Texture>(),
				color_targets[curr_src_mip]->GetTexture(),
				color_targets[curr_dst_mip],
				frame_to_dynamic_scale, inv_proj_matrix, blendmode);
		}
	}

	void TargetResampler::DepthAwareUpsample(
		Device::CommandBuffer& command_buffer,
		Device::RenderTarget* zero_depth,
		const RenderTargetList& color_targets,
		simd::vector2 frame_to_dynamic_scale,
		simd::matrix inv_proj_matrix,
		GBufferUpsamplingBlendMode final_target_blendmode,
		size_t src_mip, size_t dst_mip)
	{
		assert(src_mip > dst_mip); // source is higher downscale level

		for (size_t mip_index = 0; mip_index < src_mip - dst_mip; mip_index++)
		{
			size_t curr_src_mip = src_mip - mip_index;
			size_t curr_dst_mip = curr_src_mip - 1;
			auto blendmode = curr_dst_mip == dst_mip ? final_target_blendmode : GBufferUpsamplingBlendMode::Default; // todo: always blend
			DepthAwareUpsampleInternal(
				command_buffer,
				zero_depth,
				nullptr,
				Device::Handle<Device::Texture>(),
				Device::Handle<Device::Texture>(),
				color_targets[curr_src_mip]->GetTexture(),
				color_targets[curr_dst_mip],
				frame_to_dynamic_scale, inv_proj_matrix, blendmode);
		}
	}

	void TargetResampler::DepthAwareUpsampleInternal(
		Device::CommandBuffer& command_buffer,
		Device::RenderTarget* zero_depth,
		Device::RenderTarget* zero_normal,
		Device::Handle<Device::Texture> src_pixel_offsets,
		Device::Handle<Device::Texture> dst_pixel_offsets,
		Device::Handle<Device::Texture> src_data,
		Device::RenderTarget* dst_data,
		simd::vector2 frame_to_dynamic_scale,
		simd::matrix inv_proj_matrix,
		GBufferUpsamplingBlendMode blend_mode
	)
	{
		bool use_normal_tex = !!zero_normal;
		bool use_offset_tex = src_pixel_offsets && dst_pixel_offsets;
		size_t dst_mip_level = CalcMipFromSize(zero_depth->GetSize(), dst_data->GetSize());
		auto it = depth_aware_upsampler_shaders.find(DepthAwareUpsamplerDesc(use_offset_tex, use_normal_tex, dst_mip_level));
		if (it == depth_aware_upsampler_shaders.end())
		{
			it = depth_aware_upsampler_shaders.find(DepthAwareUpsamplerDesc(use_offset_tex, false, size_t(-1)));
		}
		assert(it != depth_aware_upsampler_shaders.end());
		auto &depth_aware_upsampler = it->second;

		const auto viewport_size = (simd::vector2(dst_data->GetSize()) * frame_to_dynamic_scale).round();
		Device::ColorRenderTargets render_targets = { dst_data };
		Device::RenderTarget* depth_stencil = nullptr;
		auto* pass = FindPass("DepthAwareUpsample", render_targets, depth_stencil);
		command_buffer.BeginPass(pass, viewport_size, viewport_size);

		auto* pixel_uniform_buffer = FindUniformBuffer("DepthAwareUpsample", device, depth_aware_upsampler.pixel_shader.Get(), dst_data);
		
		auto base_size = zero_depth->GetSize();
		pixel_uniform_buffer->SetInt("viewport_width", base_size.x);
		pixel_uniform_buffer->SetInt("viewport_height", base_size.y);
		simd::vector4 float4_scale = simd::vector4(frame_to_dynamic_scale, 0.0f, 0.0f);
		pixel_uniform_buffer->SetVector("frame_to_dynamic_scale", &float4_scale);
		pixel_uniform_buffer->SetMatrix("inv_proj_matrix", &inv_proj_matrix);
		float debug_val = 0.0f;// GetAsyncKeyState(VK_SPACE) != 0 ? 1.0f : 0.0f;
		pixel_uniform_buffer->SetFloat("debug_value", debug_val);
		pixel_uniform_buffer->SetInt("dst_mip_level", int(dst_mip_level));

		const bool additive = blend_mode == GBufferUpsamplingBlendMode::Additive;
		auto* pipeline = FindPipeline("DepthAwareUpsample", pass, depth_aware_upsampler.pixel_shader.Get(),
			additive ?
				Device::BlendState(
					Device::BlendChannelState(Device::BlendMode::ONE, Device::BlendOp::ADD, Device::BlendMode::INVSRCALPHA),
					Device::BlendChannelState(Device::BlendMode::INVDESTALPHA, Device::BlendOp::ADD, Device::BlendMode::ONE)) : 
				Device::DisableBlendState(),
			Device::DefaultRasterizerState(), 
			Device::DisableDepthStencilState());
		if (command_buffer.BindPipeline(pipeline))
		{
			Device::DynamicBindingSet::Inputs inputs;
			inputs.push_back({ "zero_depth_tex", zero_depth ? zero_depth->GetTexture().Get() : nullptr });
			inputs.push_back({ "zero_normal_tex", zero_normal ? zero_normal->GetTexture().Get() : nullptr });
			inputs.push_back({ "src_pixel_offset_tex", src_pixel_offsets.Get() });
			inputs.push_back({ "dst_pixel_offset_tex", dst_pixel_offsets.Get() });
			inputs.push_back({ "src_data_tex", src_data.Get() });
			auto* pixel_binding_set = FindBindingSet("DepthAwareUpsample", device, depth_aware_upsampler.pixel_shader.Get(), inputs);

			auto* descriptor_set = FindDescriptorSet("DepthAwareUpsample", pipeline, pixel_binding_set);
			command_buffer.BindDescriptorSet(descriptor_set, {}, { pixel_uniform_buffer });
			
			command_buffer.BindBuffers(nullptr, vertex_buffer.Get(), 0, sizeof(Vertex));
			command_buffer.Draw(4, 0);
		}

		command_buffer.EndPass();
	}

	void TargetResampler::ResampleInternal(
		Device::CommandBuffer& command_buffer,
		Device::RenderTarget * src_target,
		Device::RenderTarget * dst_target,
		simd::vector2 dynamic_resolution_scale,
		int src_mip_level,
		FilterTypes filter_type,
		float gamma)
	{
		OperationDesc op_desc;

		op_desc.out_depth = dst_target->IsDepthTexture();

	#if defined(PS4)
		op_desc.is_dst_fp32_r =dst_target->GetFormat() == Device::PixelFormat::R32F;
		op_desc.is_dst_fp32_gr = dst_target->GetFormat() == Device::PixelFormat::G32R32F;
	#endif

		if (src_target->GetWidth() > dst_target->GetWidth())
			op_desc.resampling_type = OperationDesc::ResamplingTypes::Downsample;
		else
		if (src_target->GetWidth() < dst_target->GetWidth())
			op_desc.resampling_type = OperationDesc::ResamplingTypes::Upsample;
		else
			op_desc.resampling_type = OperationDesc::ResamplingTypes::Copy;

		op_desc.filter_type = filter_type;

		Device::ColorRenderTargets render_targets;
		if (!op_desc.out_depth)
			render_targets.push_back(dst_target);
		Device::RenderTarget* depth_stencil = op_desc.out_depth ? dst_target : nullptr;
		auto* pass = FindPass("Resample", render_targets, depth_stencil);
		command_buffer.BeginPass(pass, dst_target->GetSize(), dst_target->GetSize());

		assert(shaders.find(op_desc) != shaders.end());
		OperationShader &shader = shaders[op_desc];

		auto* pixel_uniform_buffer = FindUniformBuffer("Resample", device, shader.pixel_shader.Get(), dst_target);

		const bool premultiplied = op_desc.resampling_type == OperationDesc::ResamplingTypes::Upsample; //upsample color uses premultiplied alpha
		auto* pipeline = FindPipeline("Resample", pass, shader.pixel_shader.Get(),
			premultiplied ?
			Device::BlendState(
				Device::BlendChannelState(Device::BlendMode::ONE, Device::BlendOp::ADD, Device::BlendMode::INVSRCALPHA), // colors are already multiplied by alpha
				Device::BlendChannelState(Device::BlendMode::INVDESTALPHA, Device::BlendOp::ADD, Device::BlendMode::ONE)) :
			Device::DisableBlendState(),
			Device::RasterizerState(Device::CullMode::NONE, Device::FillMode::SOLID, 0, 0),
			Device::DepthStencilState(Device::WriteOnlyDepthState(op_desc.out_depth), Device::DisableStencilState()));
		if (command_buffer.BindPipeline(pipeline))
		{
			simd::vector4 gamma_vec = { gamma, 0.0f, 0.0f, 0.0f };
			pixel_uniform_buffer->SetVector("gamma_vec", &gamma_vec);

			pixel_uniform_buffer->SetFloat("src_mip_level", float(src_mip_level));

			simd::vector4 dynamic_resolution_scale4 = { dynamic_resolution_scale.x, dynamic_resolution_scale.y, 0.0f, 0.0f };
			pixel_uniform_buffer->SetVector("dynamic_resolution_scale", &dynamic_resolution_scale4);

			{
				simd::vector4 texture_size_vector;
				texture_size_vector.x = (float)src_target->GetWidth();
				texture_size_vector.y = (float)src_target->GetHeight();
				pixel_uniform_buffer->SetVector("src_size", &texture_size_vector);
			}

			{
				simd::vector4 texture_size_vector;
				texture_size_vector.x = (float)dst_target->GetWidth();
				texture_size_vector.y = (float)dst_target->GetHeight();
				pixel_uniform_buffer->SetVector("dst_size", &texture_size_vector);
			}

			Device::DynamicBindingSet::Inputs inputs;
			if (src_target->IsDepthTexture() || !src_target->GetTexture()->UseMipmaps()) //depth render targets have a weird shader resource view, use their texture instead
				inputs.push_back({ "tex_sampler", src_target->GetTexture().Get() });
			else
				inputs.push_back({ "tex_sampler", src_target->GetTexture().Get(), (unsigned)src_target->GetTextureMipLevel() });
			auto* pixel_binding_set = FindBindingSet("Resample", device, shader.pixel_shader.Get(), inputs);

			auto* descriptor_set = FindDescriptorSet("Resample", pipeline, pixel_binding_set);
			command_buffer.BindDescriptorSet(descriptor_set, {}, { pixel_uniform_buffer });

			command_buffer.BindBuffers(nullptr, vertex_buffer.Get(), 0, sizeof(Vertex));

			command_buffer.Draw(4, 0);
		}

		command_buffer.EndPass();
	}

	void TargetResampler::reload()
	{
		{
			const std::string src = LoadShaderSource(L"Shaders/RendertargetScaler.hlsl");
			vertex_shader = CreateCachedHLSLAndLoad(device, "RendertargetScalerVs", src, nullptr, "VShad", Device::VERTEX_SHADER);
		}

		shaders.clear();
		depth_aware_downsampler_shaders.clear();
		depth_aware_upsampler_shaders.clear();

		FilterTypes filter_types[] = { FilterTypes::Avg, FilterTypes::Max, FilterTypes::MinMax, FilterTypes::Nearest, FilterTypes::Mixed };
		OperationDesc::ResamplingTypes resampling_types[] = { OperationDesc::ResamplingTypes::Copy, OperationDesc::ResamplingTypes::Downsample, OperationDesc::ResamplingTypes::Upsample };

		OperationDesc op_desc;
			for (auto resampling_type : resampling_types)
			{
				for (auto filter_type : filter_types)
				{
					op_desc.resampling_type = resampling_type;
					op_desc.filter_type = filter_type;
					
					op_desc.out_depth = false;
					op_desc.is_dst_fp32_r = false;
					op_desc.is_dst_fp32_gr = false;
					shaders[op_desc] = OperationShader();
				#if defined(PS4)
					op_desc.is_dst_fp32_r = true;
					op_desc.is_dst_fp32_gr = false;
					shaders[op_desc] = OperationShader();
					op_desc.is_dst_fp32_r = false;
					op_desc.is_dst_fp32_gr = true;
					shaders[op_desc] = OperationShader();
				#endif

					op_desc.out_depth = true;
					op_desc.is_dst_fp32_r = false; // No DST_FP32 if out_depth.
					op_desc.is_dst_fp32_gr = false; // No DST_FP32 if out_depth.
					shaders[op_desc] = OperationShader();
				}
			}

		DepthAwareDownsamplerOperationDesc downsample_op_desc;
		for (int i = 0; i < 2; i++)
		{
			downsample_op_desc.out_depth = i;
			depth_aware_downsampler_shaders[downsample_op_desc] = DepthAwareDownsamplerShader();
		}

		size_t max_prebuilt_mip = 3;
		for (size_t mip_level = 0; mip_level <= max_prebuilt_mip; mip_level++)
		{
			depth_aware_upsampler_shaders[{false, false, mip_level}] = DepthAwareUpsamplerShader();
			depth_aware_upsampler_shaders[{false, true, mip_level}] = DepthAwareUpsamplerShader();
			depth_aware_upsampler_shaders[{true, false, mip_level}] = DepthAwareUpsamplerShader();
			depth_aware_upsampler_shaders[{true, true, mip_level}] = DepthAwareUpsamplerShader();
		}
		depth_aware_upsampler_shaders[{false, false, size_t(-1)}] = DepthAwareUpsamplerShader();
		depth_aware_upsampler_shaders[{false, true, size_t(-1)}] = DepthAwareUpsamplerShader();
		depth_aware_upsampler_shaders[{true, false, size_t(-1)}] = DepthAwareUpsamplerShader();
		depth_aware_upsampler_shaders[{true, true, size_t(-1)}] = DepthAwareUpsamplerShader();

		build_downsampled_shaders[{false, false}] = BuildDownsampledShader();
		build_downsampled_shaders[{false, true}] = BuildDownsampledShader();
		build_downsampled_shaders[{true, false}] = BuildDownsampledShader();
		build_downsampled_shaders[{true, true}] = BuildDownsampledShader();

		for (auto& shader : shaders)
			LoadOperationShader(shader.second, shader.first);

		for (auto& shader : depth_aware_downsampler_shaders)
			LoadDepthAwareDownsamplerShader(shader.second, shader.first);

		for (auto& shader : depth_aware_upsampler_shaders)
			LoadDepthAwareUpsamplerShader(shader.second, shader.first);

		for (auto& shader : build_downsampled_shaders)
			LoadBuildDownsampledShader(shader.second, shader.first);
	}

	void TargetResampler::LoadDepthAwareUpsamplerShader(DepthAwareUpsamplerShader &depth_aware_upsampler, const DepthAwareUpsamplerDesc& desc)
	{
		const std::string src = LoadShaderSource(L"Shaders/DepthAwareUpsampler.hlsl");

		std::vector<Device::Macro> macros;
		std::string dst_mip_level_str = (desc.dst_mip_level != size_t(-1)) ? std::to_string(desc.dst_mip_level) : "dst_mip_level";
		macros.push_back({ "DST_MIP_LEVEL", dst_mip_level_str.c_str() });
		if (desc.use_normal)
			macros.push_back({ "USE_NORMAL_TEX", "1" });
		if (desc.use_offset_tex)
			macros.push_back({ "USE_OFFSET_TEX", "1" });
		macros.push_back({ NULL, NULL });

		depth_aware_upsampler.pixel_shader = CreateCachedHLSLAndLoad(device, "DepthAwareUpsampler", src.c_str(), macros.data(), "DepthAwareUpsample", Device::PIXEL_SHADER);
	}

	void TargetResampler::LoadDepthAwareDownsamplerShader(DepthAwareDownsamplerShader& out_shader, const DepthAwareDownsamplerOperationDesc& desc)
	{
		const std::string src = LoadShaderSource(L"Shaders/DepthAwareDownsampler.hlsl");

		std::string name = "DepthAwareDownsampler";

		std::vector<Device::Macro> macros;
		macros.push_back({ nullptr, nullptr });

		out_shader.pixel_shader = CreateCachedHLSLAndLoad(device, name.c_str(), src, macros.data(), "DepthAwareDownsample", Device::PIXEL_SHADER);
	}

	void TargetResampler::LoadBuildDownsampledShader(BuildDownsampledShader& out_shader, const BuildDownsampledOperationDesc& desc)
	{
		const std::string src = LoadShaderSource(L"Shaders/BuildDownsampled.hlsl");

		std::string name = "BuildDownsampled";

		std::vector<Device::Macro> macros;
		if (desc.out_depth)
			macros.push_back({ "OUT_DEPTH", "1" });
		if (desc.use_offset_tex)
			macros.push_back({ "USE_OFFSET_TEX", "1" });

		macros.push_back({ nullptr, nullptr });

		out_shader.pixel_shader = CreateCachedHLSLAndLoad(device, name.c_str(), src, macros.data(), "BuildDownsampled", Device::PIXEL_SHADER);
	}

	void TargetResampler::LoadOperationShader(OperationShader & out_shader, OperationDesc desc)
	{
		const std::string src = LoadShaderSource(L"Shaders/RendertargetScaler.hlsl");

		std::string name = "Resampler";

		std::vector<Device::Macro> macros;

		if (desc.out_depth)
		{
			name = name + "_OutDepth";
			macros.push_back({"OUT_DEPTH", "1"});
		}
		else
		{
			name = name + "_OutColor";
			macros.push_back({ "OUT_COLOR", "1" });
		}

		if (desc.resampling_type == OperationDesc::ResamplingTypes::Copy)
		{
			name = name + "_Copy";
			macros.push_back({ "COPY", "1" });
		}
		if (desc.resampling_type == OperationDesc::ResamplingTypes::Downsample)
		{
			name = name + "_Downsample";
			macros.push_back({ "DOWNSAMPLE", "1" });
		}
		if (desc.resampling_type == OperationDesc::ResamplingTypes::Upsample)
		{
			name = name + "_Upsample";
			macros.push_back({ "UPSAMPLE", "1" });
		}

		if (desc.filter_type == FilterTypes::Avg)
		 {
			name = name + "_Avg";
			macros.push_back({ "AVG_FILTER", "1" });
		}
		if (desc.filter_type == FilterTypes::Max)
		{
			name = name + "_Max";
			macros.push_back({ "MAX_FILTER", "1" });
		}
		if (desc.filter_type == FilterTypes::MinMax)
		{
			name = name + "_MinMax";
			macros.push_back({ "MINMAX_FILTER", "1" });
		}
		if (desc.filter_type == FilterTypes::Nearest)
		{
			name = name + "_Nearest";
			macros.push_back({ "NEAREST_FILTER", "1" });
		}
		if (desc.filter_type == FilterTypes::Mixed)
		{
			name = name + "_Mixed";
			macros.push_back({ "MIXED_FILTER", "1" });
		}

		if (desc.is_dst_fp32_r)
		{
			name = name + "_DstFP32R";
			macros.push_back({"DST_FP32_R", "1"});
		}
		if (desc.is_dst_fp32_gr)
		{
			name = name + "_DstFP32GR";
			macros.push_back({"DST_FP32_GR", "1"});
		}

		macros.push_back({ nullptr, nullptr });

		out_shader.pixel_shader = CreateCachedHLSLAndLoad(device, name.c_str(), src, macros.data(), MAIN_RESAMPLE, Device::PIXEL_SHADER);
	}
}
