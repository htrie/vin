
#include "Common/Utility/Profiler/ScopedProfiler.h"

#include "Visual/Utility/DXHelperFunctions.h"
#include "Visual/Device/Constants.h"
#include "Visual/Device/Device.h"
#include "Visual/Device/State.h"
#include "Visual/Device/VertexDeclaration.h"
#include "Visual/Device/VertexBuffer.h"
#include "Visual/Device/RenderTarget.h"
#include "Visual/Device/Texture.h"
#include "Visual/Device/Shader.h"
#include "Visual/Device/Buffer.h"
#include "Visual/Device/Compiler.h"

#include "Scene/Camera.h"
#include "PostProcess.h"
#include "RendererSettings.h"
#include "CachedHLSLShader.h"
#include "Utility.h"

namespace Renderer
{
	namespace
	{
		const static Device::VertexElements vertex_elements =
		{
			{ 0, 0, Device::DeclType::FLOAT4, Device::DeclUsage::POSITION, 0 },
			{ 0, sizeof(float) * 4, Device::DeclType::FLOAT2, Device::DeclUsage::TEXCOORD, 0 },
		};

#pragma pack( push, 1 )
		struct Vertex
		{
			simd::vector4 p;
			simd::vector2 t;
		};
#pragma pack( pop )

		const Vertex vertex_data[4] =
		{
			{ simd::vector4(-1.f, -1.f, 0.0f, 1.0f), simd::vector2(0.0f, 1.0f) },
			{ simd::vector4(-1.f, 1.f, 0.0f, 1.0f), simd::vector2(0.0f, 0.0f) },
			{ simd::vector4(1.f, -1.f, 0.0f, 1.0f), simd::vector2(1.0f, 1.0f) },
			{ simd::vector4(1.f, 1.f, 0.0f, 1.0f), simd::vector2(1.0f, 0.0f) }
		};
	}

	PostProcess::PassKey::PassKey(Device::ColorRenderTargets render_targets, Device::RenderTarget* depth_stencil)
		: render_targets(render_targets), depth_stencil(depth_stencil)
	{}

	PostProcess::PipelineKey::PipelineKey(Device::Pass* pass, Device::Shader* pixel_shader)
		: pass(pass), pixel_shader(pixel_shader)
	{}

	PostProcess::BindingSetKey::BindingSetKey(Device::Shader * shader, uint32_t inputs_hash)
		: shader(shader), inputs_hash(inputs_hash)
	{}

	PostProcess::DescriptorSetKey::DescriptorSetKey(Device::Pipeline* pipeline, Device::DynamicBindingSet* pixel_binding_set, uint32_t samplers_hash)
		: pipeline(pipeline), pixel_binding_set(pixel_binding_set), samplers_hash(samplers_hash)
	{}

	PostProcess::UniformBufferKey::UniformBufferKey(Device::Shader* pixel_shader, Device::RenderTarget* dst)
		: pixel_shader(pixel_shader), dst(dst)
	{}

	
	Device::Pass* PostProcess::FindPass(const Memory::DebugStringA<>& name, Device::IDevice* device, Device::ColorRenderTargets render_targets, Device::RenderTarget * depth_stencil)
	{
		Memory::Lock lock(passes_mutex);
		return passes.FindOrCreate(PassKey(render_targets, depth_stencil), [&]()
		{
			return Device::Pass::Create(name, device, render_targets, depth_stencil, Device::ClearTarget::NONE, simd::color(0), 0.0f);
		}).Get();
	}

	Device::Pipeline* PostProcess::FindPipeline(const Memory::DebugStringA<>& name, Device::IDevice* device, Device::Pass * pass, Device::Shader * pixel_shader)
	{
		Memory::Lock lock(pipelines_mutex);
		return pipelines.FindOrCreate(PipelineKey(pass, pixel_shader), [&]()
		{
			return Device::Pipeline::Create(name, device, pass, Device::PrimitiveType::TRIANGLESTRIP, &vertex_declaration, vertex_shader.Get(), pixel_shader, Device::DisableBlendState(), Device::DefaultRasterizerState(), Device::DisableDepthStencilState());
		}).Get();
	}

	Device::DynamicBindingSet* PostProcess::FindBindingSet(const Memory::DebugStringA<>& name, Device::IDevice* device, Device::Shader* pixel_shader, const Device::DynamicBindingSet::Inputs& inputs)
	{
		Memory::Lock lock(binding_sets_mutex);
		return binding_sets.FindOrCreate(BindingSetKey(pixel_shader, inputs.Hash()), [&]()
		{
			return Device::DynamicBindingSet::Create(name, device, pixel_shader, inputs);
		}).Get();
	}

	Device::DescriptorSet* PostProcess::FindDescriptorSet(const Memory::DebugStringA<>& name, Device::IDevice* device, Device::Pipeline * pipeline, Device::DynamicBindingSet* pixel_binding_set)
	{
		Memory::Lock lock(descriptor_sets_mutex);
		return descriptor_sets.FindOrCreate(DescriptorSetKey(pipeline, pixel_binding_set, device->GetSamplersHash()), [&]()
		{
			return Device::DescriptorSet::Create(name, device, pipeline, {}, { pixel_binding_set });
		}).Get();
	}

	Device::DynamicUniformBuffer* PostProcess::FindUniformBuffer(const Memory::DebugStringA<>& name, Device::IDevice* device, Device::Shader* pixel_shader, Device::RenderTarget* dst)
	{
		Memory::Lock lock(uniform_buffers_mutex);
		return uniform_buffers.FindOrCreate(UniformBufferKey(pixel_shader, dst), [&]()
		{
			return Device::DynamicUniformBuffer::Create(name, device, pixel_shader);
		}).Get();
	}

	void PostProcess::OnCreateDevice(Device::IDevice *device)
	{
		Device::MemoryDesc init_mem = { vertex_data, 0, 0 };
		vb_post_process = Device::VertexBuffer::Create("VB PostProcess", device, 4 * sizeof(Vertex), Device::UsageHint::IMMUTABLE, Device::Pool::DEFAULT, &init_mem );

		vertex_shader = CreateCachedHLSLAndLoad(device, "FullscreenQuad_VertexShader", LoadShaderSource(L"Shaders/FullScreenQuad_VertexShader.hlsl"), nullptr, "VShad", Device::VERTEX_SHADER);

		// desaturation stuff
		{
			desaturation_post_transform = ::Texture::Handle(::Texture::VolumeTextureDesc(L"Metadata/EnvironmentSettings/desaturation.post.dds"));
		}

		// overlay stuff
		{
			overlay_texture[(int)OverlayTypes::Blood] = ::Texture::Handle(::Texture::SRGBTextureDesc(L"Art/2DArt/UIEffects/Overlays/blood_overlay.dds"));
			overlay_texture[(int)OverlayTypes::Poison] = ::Texture::Handle(::Texture::SRGBTextureDesc(L"Art/2DArt/UIEffects/Overlays/BloodPoisonOverlay.dds"));
			overlay_texture[(int)OverlayTypes::Royale] = ::Texture::Handle(::Texture::SRGBTextureDesc(L"Art/2DArt/UIEffects/Overlays/royale_overlay.dds"));
			overlay_texture[(int)OverlayTypes::Incursion] = ::Texture::Handle(::Texture::SRGBTextureDesc(L"Art/2DArt/UIEffects/Overlays/incursion_inThePast.dds"));
		}

		// textures
		{
			//crack_sdm_texture = Linear::Texture::Handle();
			crack_sdm_texture = ::Texture::Handle(::Texture::LinearTextureDesc(L"Art/2DArt/Decay/crack_sdm.dds"));
			//crack_sdm_texture = ::Texture::Handle(::Texture::LinearTextureDesc(L"Art/2DArt/Lookup/crack_sdm.png"));
			//palette_texture = ::Texture::Handle(::Texture::SRGBTextureDesc(L"Art/2DArt/Decay/palette.png"));
			palette_texture = ::Texture::Handle(::Texture::SRGBTextureDesc(L"Art/2DArt/Decay/palette.dds"));
			breach_post_transform = ::Texture::Handle(::Texture::VolumeTextureDesc(L"Metadata/EnvironmentSettings/breach.post.dds"));
			breach_edge = ::Texture::Handle(::Texture::SRGBTextureDesc(L"Art/2DArt/Breach/Edge.dds"));
			breach_muddle = ::Texture::Handle(::Texture::LinearTextureDesc(L"Art/particles/distortion/muddle_double.dds"));
			post_transform_identity = ::Texture::Handle(::Texture::VolumeTextureDesc(L"Art/2DArt/Lookup/identity_post.dds"));
			space_texture = ::Texture::Handle(::Texture::SRGBTextureDesc(L"Art/particles/environment_effects/shaper_fragments/space-01.dds"));
		}

		post_process_shaders[PostProcessKey(false)] = PostProcessShader();
		post_process_shaders[PostProcessKey(true)] = PostProcessShader();

		upscale_shader = CreateCachedHLSLAndLoad(device, "PostProcess", LoadShaderSource(L"Shaders/PostProcessing.hlsl"), nullptr, "PostProcessUpscalePS", Device::PIXEL_SHADER);

		for(auto &it : post_process_shaders)
		{
			auto &shader = it.second;
			Device::Macro post_process_macros[] =
			{
				{ "DETAIL_LEVEL", it.first.is_basic ? "0" : "1" },
				{ NULL, NULL }
			};
			shader.pixel_shader = CreateCachedHLSLAndLoad(device, "PostProcess", LoadShaderSource(L"Shaders/PostProcessing.hlsl"), post_process_macros, "PostProcessPS", Device::PIXEL_SHADER);
		}
	}

	void PostProcess::OnResetDevice(Device::IDevice *device)
	{
	}

	void PostProcess::OnLostDevice()
	{
		uniform_buffers.Clear();
		descriptor_sets.Clear();
		binding_sets.Clear();
		pipelines.Clear();
		passes.Clear();
	}

	void PostProcess::OnDestroyDevice()
	{
		vb_post_process.Reset();

		for (auto &it : post_process_shaders)
		{
			it.second.pixel_shader.Reset();
		}
		upscale_shader.Reset();
		vertex_shader.Reset();

		desaturation_post_transform = ::Texture::Handle();
		breach_post_transform = ::Texture::Handle();
		breach_edge = ::Texture::Handle();
		breach_muddle = ::Texture::Handle();
		post_transform_identity = ::Texture::Handle();

		for( auto& tex : overlay_texture )
			tex = ::Texture::Handle();

		for (auto& tex : post_transforms)
			tex = ::Texture::Handle();

		crack_sdm_texture = ::Texture::Handle();
		space_texture = ::Texture::Handle();
		palette_texture = ::Texture::Handle();
	}

	void PostProcess::RenderPostProcessedImage(
		Device::IDevice *device, 
		Device::CommandBuffer& command_buffer,
		Device::Texture *src, 
		Device::Texture *shimmer, 
		Device::Texture *desaturation, 
		Device::Texture *bloom, 
		SynthesisMapInfo synthesis_map_info, 
		Device::Texture *depth, 
		const DoFParams* dof_params,
		Device::RenderTarget *dst, 
		bool base_effects_only, 
		bool time_manipulation_effects,
		simd::vector2 src_frame_to_dynamic_scale, 
		const simd::vector4& ritual_data,
		float ritual_ratio )
	{
		PROFILE;

		Device::ColorRenderTargets render_targets = { dst };
		Device::RenderTarget* depth_stencil = nullptr;
		auto* pass = FindPass("Composition", device, render_targets, depth_stencil);
		
		const bool dof_enabled = dof_params != nullptr;

		const auto viewport_size = (simd::vector2(dst->GetSize()) * src_frame_to_dynamic_scale).round();
		command_buffer.BeginPass(pass, viewport_size, viewport_size);

		auto &shader = post_process_shaders[PostProcessKey(base_effects_only)];

		auto* pixel_uniform_buffer = FindUniformBuffer("PostProcess", device, shader.pixel_shader.Get(), dst);

		pixel_uniform_buffer->SetInt("viewport_width", viewport_size.x);
		pixel_uniform_buffer->SetInt("viewport_height", viewport_size.y);

		simd::vector4 src_frame_to_dynamic_scale4(src_frame_to_dynamic_scale, 0.0f, 0.0f);
		pixel_uniform_buffer->SetVector("src_frame_to_dynamic_scale", &src_frame_to_dynamic_scale4);
	
		simd::vector4 camera_world_pos = simd::vector4(camera->GetPosition(), 1.0f);
		pixel_uniform_buffer->SetVector("camera_world_pos", &camera_world_pos);
	
		auto viewproj_matrix = camera->GetViewProjectionMatrix();
		pixel_uniform_buffer->SetMatrix("viewproj_matrix", &viewproj_matrix);
		
		auto inv_viewproj_matrix = viewproj_matrix.inverse();
		pixel_uniform_buffer->SetMatrix("inv_viewproj_matrix", &inv_viewproj_matrix);
		auto inv_proj_matrix = camera->GetProjectionMatrix().inverse();
		pixel_uniform_buffer->SetMatrix("inv_proj_matrix", &inv_proj_matrix);

		pixel_uniform_buffer->SetVector("sphere_info", &breach_sphere_info);
		pixel_uniform_buffer->SetVector("muddle_sampler_info", &sphere_muddle_vals);
		pixel_uniform_buffer->SetFloat("breach_time", breach_time);
		pixel_uniform_buffer->SetFloat("time", curr_time);

		simd::vector4 minmax_vec = simd::vector4(
			synthesis_map_info.min_point.x, synthesis_map_info.min_point.y,
			synthesis_map_info.max_point.x, synthesis_map_info.max_point.y);
		pixel_uniform_buffer->SetVector("decay_map_minmax", &minmax_vec);
	
		simd::vector4 size = simd::vector4(
			(float)synthesis_map_info.size.x, (float)synthesis_map_info.size.y, 
			0.0f, 0.0f);
		pixel_uniform_buffer->SetVector("decay_map_size", &size);

		pixel_uniform_buffer->SetFloat("decay_map_time", synthesis_map_info.decay_time);
		pixel_uniform_buffer->SetFloat("decay_map_type", float(synthesis_map_info.decay_type));
		pixel_uniform_buffer->SetFloat("creation_time", synthesis_map_info.creation_time);
		pixel_uniform_buffer->SetFloat("global_stability", synthesis_map_info.global_stability);
		pixel_uniform_buffer->SetVector("stabiliser_position", &synthesis_map_info.stabiliser_position);

		pixel_uniform_buffer->SetFloat("original_intensity", original_intensity);

		pixel_uniform_buffer->SetFloat("exposure", base_effects_only ? -1.0f : exposure);

		bool fade_enable;
		float fade_amount = 0.0f;
		if (fade_in && fade_duration == 0.0f)
		{
			fade_enable = false;
		}
		else
		{
			fade_enable = true;
			if (fade_duration == 0.0f)
			{
				fade_amount = 0.0f;
			}
			else
			{
				fade_amount = elapsed_fade_time / fade_duration;
				if (!fade_in)
					fade_amount = (1.0f - fade_amount);
			}
		}

		pixel_uniform_buffer->SetBool("fade_enable", fade_enable);
		pixel_uniform_buffer->SetFloat("fade_amount", fade_amount);
		pixel_uniform_buffer->SetFloat("overlay_intensity", overlay_intensity);
		BlurParams blur_params = GetCurrBlurParams();
		simd::vector2 res_blur = (blur_params.intensity + simd::vector2(sin(curr_time), cos(curr_time)) * blur_params.intensity.len() * blur_params.wobbliness).normalize() * blur_params.intensity.len();
		res_blur *= time_manipulation_effects ? 1.0f : 0.0f;
		simd::vector4 blur_intensity4 = simd::vector4(res_blur.x, res_blur.y, blur_params.curvature, 0.0f);
		pixel_uniform_buffer->SetVector("blur_params", &blur_intensity4);
		
		pixel_uniform_buffer->SetBool("shimmer_enable", shimmer != nullptr);
		pixel_uniform_buffer->SetFloat("desaturation_enable", desaturation != nullptr);
		pixel_uniform_buffer->SetBool("post_transform_enable", !post_transforms[0].IsNull() || !post_transforms[1].IsNull());
		pixel_uniform_buffer->SetFloat("post_transform_ratio", post_transform_ratio);
		pixel_uniform_buffer->SetBool("breach_enable", IsBreachEffectEnabled());
		pixel_uniform_buffer->SetBool("decay_enable", (!!synthesis_map_info.decay_tex) && (synthesis_map_info.decay_time > 0.0f));
		pixel_uniform_buffer->SetBool("dof_enable", dof_enabled);

		pixel_uniform_buffer->SetFloat( "ritual_sphere_ratio", ritual_ratio );

		if( ritual_ratio != 0.0f )
			pixel_uniform_buffer->SetVector( "ritual_sphere_info", &ritual_data );

		if (dof_enabled)
		{
			pixel_uniform_buffer->SetFloat("dof_focus_range", dof_params->focus_range);
			pixel_uniform_buffer->SetFloat("dof_focus_distance", dof_params->focus_distance);
			simd::vector4 transition = simd::vector4(dof_params->foreground_transition, dof_params->background_transition, 0, 0);
			pixel_uniform_buffer->SetVector("dof_transition_region", &transition);
		}

		auto* pipeline = FindPipeline("PostProcess", device, pass, shader.pixel_shader.Get());
		if (command_buffer.BindPipeline(pipeline))
		{
			auto GetTexture = [&](const ::Texture::Handle& texture, Device::Texture* fallback = nullptr) -> Device::Texture*
			{
				if (!texture.IsNull())
					return texture.GetTexture().Get();
				return fallback;
			};

			Device::Texture* overlay_tex = nullptr;
			switch (overlay_type)
			{
			default:
			{
				unsigned index = static_cast<unsigned>(overlay_type);
				overlay_tex = GetTexture(overlay_texture[index]);
			}break;
			case OverlayTypes::Max:
			{
				overlay_tex = nullptr;
			}
			}
			auto fallback_tex = GetTexture(post_transform_identity);

			Device::DynamicBindingSet::Inputs inputs;
			inputs.push_back({ "source_sampler", src });
			inputs.push_back({ "shimmer_sampler", shimmer });
			inputs.push_back({ "desaturation_sampler", desaturation });
			inputs.push_back({ "desaturation_transform_sampler", GetTexture(desaturation_post_transform) });
			inputs.push_back({ "bloom_sampler", bloom });
			inputs.push_back({ "crack_sdm_sampler", GetTexture(crack_sdm_texture) });
			inputs.push_back({ "palette_sampler", GetTexture(palette_texture) });
			inputs.push_back({ "decay_map_sampler", synthesis_map_info.decay_tex.Get() });
			inputs.push_back({ "stable_map_sampler", synthesis_map_info.stable_tex.Get() });
			inputs.push_back({ "space_tex_sampler", GetTexture(space_texture) });
			inputs.push_back({ "overlay_sampler", overlay_tex });
			inputs.push_back({ "post_transform_sampler0", GetTexture(post_transforms[0], fallback_tex) });
			inputs.push_back({ "post_transform_sampler1", GetTexture(post_transforms[1], fallback_tex) });
			inputs.push_back({ "depth_sampler", depth });
			inputs.push_back({ "breach_transform_sampler", GetTexture(breach_post_transform) });
			inputs.push_back({ "breach_edge_sampler", GetTexture(breach_edge) });
			inputs.push_back({ "muddle_sampler", GetTexture(breach_muddle) });
			inputs.push_back({ "dof_foreground_sampler", dof_enabled ? dof_params->foreground : nullptr });
			inputs.push_back({ "dof_background_sampler", dof_enabled ? dof_params->background : nullptr });

			auto* pixel_binding_set = FindBindingSet("PostProcess", device, shader.pixel_shader.Get(), inputs);

			auto* descriptor_set = FindDescriptorSet("PostProcess", device, pipeline, pixel_binding_set);

			command_buffer.BindDescriptorSet(descriptor_set, {}, { pixel_uniform_buffer });

			command_buffer.BindBuffers(nullptr, vb_post_process.Get(), 0, sizeof(Vertex));
			command_buffer.Draw(4, 0);
		}

		command_buffer.EndPass();
	}

	void PostProcess::UpscalePostProcessedImage(
		Device::IDevice* device,
		Device::CommandBuffer& command_buffer,
		Device::Texture* src,
		Device::RenderTarget* dst,
		simd::vector2 src_frame_to_dynamic_scale)
	{
		PROFILE;

		Device::ColorRenderTargets render_targets = { dst };
		Device::RenderTarget* depth_stencil = nullptr;
		auto* pass = FindPass("Upscale", device, render_targets, depth_stencil);
		command_buffer.BeginPass(pass, dst->GetSize(), dst->GetSize());

		auto& shader = upscale_shader;

		auto* pixel_uniform_buffer = FindUniformBuffer("PostProcess", device, shader.Get(), dst);

		pixel_uniform_buffer->SetInt("viewport_width", dst->GetWidth());
		pixel_uniform_buffer->SetInt("viewport_height", dst->GetHeight());

		Device::SurfaceDesc src_desc;
		src->GetLevelDesc(0, &src_desc);

		simd::vector4 src_frame_to_dynamic_scale4(src_frame_to_dynamic_scale, src_frame_to_dynamic_scale.x - (0.5f / float(src_desc.Width)), src_frame_to_dynamic_scale.y - (0.5f / float(src_desc.Height)));
		pixel_uniform_buffer->SetVector("src_frame_to_dynamic_scale", &src_frame_to_dynamic_scale4);

		auto viewproj_matrix = camera->GetViewProjectionMatrix();
		pixel_uniform_buffer->SetMatrix("viewproj_matrix", &viewproj_matrix);

		auto* pipeline = FindPipeline("Upscale", device, pass, shader.Get());
		if (command_buffer.BindPipeline(pipeline))
		{
			Device::DynamicBindingSet::Inputs inputs;
			inputs.push_back({ "source_sampler", src });
			
			auto* pixel_binding_set = FindBindingSet("Upscale", device, shader.Get(), inputs);
			auto* descriptor_set = FindDescriptorSet("Upscale", device, pipeline, pixel_binding_set);

			command_buffer.BindDescriptorSet(descriptor_set, {}, { pixel_uniform_buffer });

			command_buffer.BindBuffers(nullptr, vb_post_process.Get(), 0, sizeof(Vertex));
			command_buffer.Draw(4, 0);
		}

		command_buffer.EndPass();
	}

	PostProcess::PostProcess( ) 
		: original_intensity(1)
		, fade_duration( 0.0f )
		, elapsed_fade_time( 0.0f )
		, fade_delay( 0.0f )
		, fade_in( true )
		, camera( nullptr )
		, breach_effect_on( false )
		, breach_sphere_info( 0.f, 0.f, 0.f, 0.f )
		, sphere_muddle_vals( 1.2f, 0.3f, 0.075f, 0.125f )
		, breach_time( 0.f )
		, overlay_type( OverlayTypes::Max )
		, overlay_intensity( 0.0f )
		, blur_transition_ratio(0.0f)
		, blur_transition_duration(0.0f)
		, curr_time(0.0f)
		, exposure(1.0f)
		, vertex_declaration(vertex_elements)
	{
	}

	void PostProcess::SetPostTransformVolumes(::Texture::Handle textures[2], float ratio)
	{
		for (int i = 0; i < 2; i++)
		{
			post_transforms[i] = std::move(textures[i]);
			post_transform_ratio = ratio;
		}
	}

	void PostProcess::SetBlurParams( BlurParams params, const float time )
	{
		this->start_blur_params = GetCurrBlurParams();
		this->target_blur_params = params;
		this->blur_transition_ratio = 0.0f;
		this->blur_transition_duration = time;
	}

	void PostProcess::SetBreachSphereInfo( const simd::vector3& position, const float radius )
	{
		breach_sphere_info.x = position.x;
		breach_sphere_info.y = position.y;
		breach_sphere_info.z = position.z;
		breach_sphere_info.w = radius;
	}

	void PostProcess::ToggleBreachEffect( bool enable )
	{ 
		breach_effect_on = enable; 

		// reset the breach time if disabled
		if ( !enable )
			breach_time = 0.f;
	}

	void PostProcess::FadeIn( const float fade_time, const float delay )
	{
		fade_duration = fade_time;
		fade_delay = delay;
		fade_in = true;
		elapsed_fade_time = 0.0f;
	}

	void PostProcess::FadeOut( const float fade_time, const float delay )
	{
		fade_duration = fade_time;
		fade_delay = delay;
		fade_in = false;
		elapsed_fade_time = 0.0f;
	}

	PostProcess::BlurParams PostProcess::GetCurrBlurParams()
	{
		if (blur_transition_duration < 1e-5f)
			return target_blur_params;
		//linear transition
		//return start_blur_params + (target_blur_params + start_blur_params * (-1.0f)) * std::min(1.0f, std::max(0.0f, blur_transition_ratio));
		//exponential transition
		return start_blur_params + (target_blur_params + start_blur_params * (-1.0f)) * (1.0f - exp(-blur_transition_ratio));
	}

	void PostProcess::SetOverlayType( OverlayTypes overlay_type, float intensity, float time )
	{
		this->overlay_type = overlay_type;

		if( time > 0.0f )
		{
			overlay_target = intensity;

			const float dist = overlay_target - overlay_intensity;
			overlay_speed = dist / time;
		}
		else
		{
			overlay_intensity = intensity;
			overlay_target = intensity;
			overlay_speed = 0.0f;
		}
	}

	void PostProcess::Update( const float elapsed_time )
	{
		curr_time += elapsed_time;
		if( fade_delay > 0.0f )
		{
			fade_delay -= elapsed_time;
		}
		else if( fade_duration > 0.0f )
		{
			elapsed_fade_time += elapsed_time;
			if( elapsed_fade_time >= fade_duration )
				fade_duration = 0.0f;
		}

		if( overlay_speed != 0.0f )
		{
			const float dist = overlay_target - overlay_intensity;
			const float move = elapsed_time * overlay_speed;

			if( dist < move )
			{
				overlay_intensity = overlay_target;
				overlay_speed = 0.0f;
			}
			else
			{
				overlay_intensity += move;
			}
		}

		if (blur_transition_duration > 0.0f)
			blur_transition_ratio += elapsed_time / blur_transition_duration;
		else
			blur_transition_ratio = 0.0f; //unused


		if ( breach_effect_on )
		{
			breach_time += elapsed_time;
		}
	}

	void PostProcess::reload(Device::IDevice *device)
	{
		OnLostDevice();
		OnDestroyDevice();
		OnCreateDevice(device);
		OnResetDevice(device);
	}

	PostProcess::BlurParams::BlurParams()
	{
		intensity = simd::vector2(0.0f, 0.0f);
		wobbliness = 0.0f;
		curvature = 2.0f;
	}
	PostProcess::BlurParams PostProcess::BlurParams::operator *(float val)
	{
		PostProcess::BlurParams res;
		res.intensity = this->intensity * val;
		res.wobbliness = this->wobbliness * val;
		res.curvature = curvature * val;
		return res;
	}
	PostProcess::BlurParams PostProcess::BlurParams::operator +(const PostProcess::BlurParams &other)
	{
		PostProcess::BlurParams res;
		res.intensity = this->intensity + other.intensity;
		res.wobbliness = this->wobbliness + other.wobbliness;
		res.curvature = curvature + other.curvature;
		return res;
	}

}
