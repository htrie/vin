
#include "Common/Job/JobSystem.h"
#include "Common/FileReader/TGTReader.h"

#include "Visual/Device/Device.h"
#include "Visual/Device/RenderTarget.h"
#include "Visual/Device/DynamicResolution.h"
#include "Visual/Renderer/Scene/SceneSystem.h"
#include "Visual/GUI2/GUIResourceManager.h"
#include "Visual/Engine/Plugins/PerformancePlugin.h"
#include "Visual/Environment/EnvironmentSettings.h"

#include "RendererSubsystem.h"
#include "RenderGraph.h"

namespace Renderer
{
	namespace ConstData
	{
		const bool bool_false(false);
		const bool bool_true(true);
		const int int_one(1);
	}


	SubPass::SubPass(
		const Memory::DebugStringW<>& debug_name,
		const Callback& callback)
		: callback(callback)
		, debug_name(Memory::DebugStringW<>(L"SUB-PASS ") + debug_name.c_str())
	{}

	SubPass::SubPass(
		DrawCalls::BlendMode blend_mode,
		Entity::ViewType view_type,
		bool inverted_culling,
		float depth_bias,
		float slope_scale)
		: blend_mode(blend_mode)
		, debug_name(Memory::DebugStringW<>(L"SUB-PASS ") + DrawCalls::GetBlendModeName(blend_mode))
		, view_type(view_type)
		, inverted_culling(inverted_culling)
		, depth_bias(depth_bias)
		, slope_scale(slope_scale)
	{}


	Pass::Pass(
		const Memory::DebugStringW<>& debug_name,
		const Callback& callback)
		: callback(callback)
		, debug_name(Memory::DebugStringW<>(L"PASS ") + debug_name.c_str())
		, compute_pass(false)
	{}

	Pass::Pass(
		const Memory::DebugStringW<>& debug_name,
		Device::Handle<Device::Pass>& pass,
		const ShaderPass shader_pass,
		const Device::UniformInputs& uniform_inputs,
		const Device::BindingInputs& binding_inputs,
		Device::Rect viewport_rect,
		Device::Rect scissor_rect, 
		Device::ClearTarget clear_flags,
		simd::color clear_color,
		float clear_z,
		bool z_prepass)
		: pass(pass)
		, debug_name(Memory::DebugStringW<>(L"PASS ") + debug_name.c_str())
		, shader_pass(shader_pass)
		, uniform_inputs(uniform_inputs)
		, binding_inputs(binding_inputs)
		, viewport_rect(viewport_rect)
		, scissor_rect(scissor_rect)
		, clear_flags(clear_flags)
		, clear_color(clear_color)
		, clear_z(clear_z)
		, z_prepass(z_prepass)
		, compute_pass(false)
	{}

	void Pass::AddSubPass(const SubPass&& sub_pass)
	{
		sub_passes.emplace_back(sub_pass);
	}


	MetaPass::MetaPass(
		unsigned int index,
		const Memory::DebugStringW<>& debug_name,
		Job2::Profile gpu_hash,
		Job2::Profile cpu_hash,
		unsigned bucket_count,
		unsigned bucket_index)
		: command_buffer(System::Get().FindCommandBuffer(index))
		, debug_name(Memory::DebugStringW<>(L"META-PASS ") + debug_name.c_str())
		, gpu_hash(gpu_hash)
		, cpu_hash(cpu_hash)
		, bucket_count(bucket_count)
		, bucket_index(bucket_index)
	{}

	MetaPass& MetaPass::AddPass(const Pass&& pass)
	{
		passes.emplace_back(pass);
		return *this;
	}


	RenderGraph::RenderGraph()
	{
		for (unsigned i = 0; i < PassType::NumPasses; ++i)
			pass_types[i] = (PassType)i;
	}

	void RenderGraph::OnCreateDevice(Device::IDevice* device)
	{
		this->device = device;

		screenspace_tracer.OnCreateDevice(device);
		resampler.OnCreateDevice(device);
		post_process.OnCreateDevice(device);
		smaa.OnCreateDevice(device);

		environment_ggx_tex = ::Texture::Handle(::Texture::LinearTextureDesc(L"Art/2DArt/Lookup/environment_ggx.dds"));
	}

	void RenderGraph::OnResetDevice(Device::IDevice* device, bool gi_enabled, bool use_wide_pixel_format, size_t downscaling_mip, bool dof_enabled, bool smaa_enabled, bool smaa_high)
	{
		screenspace_tracer.OnResetDevice(device);
		resampler.OnResetDevice(device);
		post_process.OnResetDevice(device);
		smaa.OnResetDevice(device, smaa_enabled, smaa_high);

		const auto frame_size = device->GetFrameSize();
		const auto target_format = use_wide_pixel_format ? Device::PixelFormat::A16B16G16R16F : Device::PixelFormat::R11G11B10F;

		depth = Device::RenderTarget::Create("Depth", device, frame_size.x, frame_size.y, Device::PixelFormat::D24S8, Device::Pool::DEFAULT, false, true);
		srgbHelper[0] = Device::RenderTarget::Create("Srgb Helper 0", device, frame_size.x, frame_size.y, target_format, Device::Pool::DEFAULT, true, true);
		srgbHelper[1] = Device::RenderTarget::Create("Srgb Helper 1", device, frame_size.x, frame_size.y, target_format, Device::Pool::DEFAULT, true, true);
		linearHelper = Device::RenderTarget::Create("Linear Helper", device, frame_size.x, frame_size.y, Device::PixelFormat::A8R8G8B8, Device::Pool::DEFAULT, false, false);
		desaturation_target = Device::RenderTarget::Create("Desaturation", device, frame_size.x, frame_size.y, Device::PixelFormat::A8R8G8B8, Device::Pool::DEFAULT, true, false);

		if (gi_enabled)
		{
			gbuffer_normals = Device::RenderTarget::Create("Gbuffer Normals", device, frame_size.x, frame_size.y, Device::PixelFormat::A8R8G8B8, Device::Pool::DEFAULT, false, false);
			gbuffer_direct_light = Device::RenderTarget::Create("Gbuffer Direct Light", device, frame_size.x, frame_size.y, target_format, Device::Pool::DEFAULT, true, false);

			size_t downscale = (static_cast< size_t >(1) << downscaling_mip);
			int width = static_cast< int >( frame_size.x / downscale );
			int height = static_cast< int >( frame_size.y / downscale );
			global_illumination_data.start_level = downscaling_mip;

			global_illumination_data.gi_pyramid = std::make_unique<DownsampledPyramid>(device, frame_size, static_cast< uint32_t >( downscaling_mip + 1 ), Device::PixelFormat::A8R8G8B8, true, "GI Mip Pyramid");
			global_illumination_data.tmp_gi_mip = Device::RenderTarget::Create("Tmp GI mip", device, width, height, Device::PixelFormat::A8R8G8B8, Device::Pool::DEFAULT, true, false, false);
		}

		{
			screenspace_shadows_data.start_level = downscaling_mip;
			screenspace_shadows_data.finish_level = screenspace_shadows_data.start_level + 3;
			screenspace_shadows_data.mip_pyramid = std::make_unique<DownsampledPyramid>(device, frame_size, static_cast< uint32_t >( screenspace_shadows_data.finish_level + 1), Device::PixelFormat::A8R8G8B8, true, "Screenspace shadow mips");
			screenspace_shadows_data.tmp_mip_pyramid = std::make_unique<DownsampledPyramid>(device, frame_size, static_cast< uint32_t >( screenspace_shadows_data.finish_level + 1), Device::PixelFormat::A8R8G8B8, true, "Screenspace shadow downsampled mips");
		}

		{
			int width = frame_size.x;
			int height = frame_size.y;

			rays_data.start_level = downsampled_gbuffer_data.downscaling_mip;
			rays_data.finish_level = screenspace_shadows_data.start_level + 3;
			/*rays_data.ray_hit_mip_pyramid= std::make_unique<DownsampledPyramid>(device, frame_size, rays_data.finish_level + 1, Device::PixelFormat::A8R8G8B8, true, "Ray hit mips");
			rays_data.ray_dir_target = Device::RenderTarget::Create("Ray dir darget", device, frame_size.x, frame_size.y, Device::PixelFormat::A16B16G16R16F, Device::Pool::DEFAULT, false, false, false);*/
		}

		{
			size_t gbuffer_mips_count = 8; //maybe needs an actual calculation how many mips we need
			bool use_pixel_offsets = false; //figure out later whether we want them

			downsampled_gbuffer_data.downscaling_mip = downscaling_mip;

			downsampled_gbuffer_data.pixel_offset_pyramid.reset();
			downsampled_gbuffer_data.offset_depth_pyramid.reset();
			if (use_pixel_offsets)
			{
				downsampled_gbuffer_data.pixel_offset_pyramid = std::make_unique<DownsampledPyramid>(device, frame_size, static_cast< uint32_t >( gbuffer_mips_count ), Device::PixelFormat::G16R16_UINT, false, "Pixel offset pyramid");
				downsampled_gbuffer_data.offset_depth_pyramid = std::make_unique<DownsampledPyramid>(device, depth.get(), static_cast< uint32_t >( gbuffer_mips_count ), "Downsampled offset depth pyramid");
			}
			downsampled_gbuffer_data.depth_pyramid = std::make_unique<DownsampledPyramid>(device, depth.get(), static_cast< uint32_t >( gbuffer_mips_count ), "Downsampled depth pyramid");
			if (gbuffer_normals)
			{
				downsampled_gbuffer_data.normal_pyramid = std::make_unique<DownsampledPyramid>(device, frame_size, static_cast< uint32_t >( gbuffer_mips_count ), Device::PixelFormat::A8R8G8B8, false, "Normal pyramid");
			}

			size_t downsample = (size_t(1) << downsampled_gbuffer_data.downscaling_mip);
			UINT downsampled_width = static_cast< UINT >( frame_size.x / downsample );
			UINT downsampled_height = static_cast< UINT >( frame_size.y / downsample );

			downsampled_gbuffer_data.blurred_linear_depth = Device::RenderTarget::Create("Linear depth blurred", device, downsampled_width, downsampled_height, Device::PixelFormat::G32R32F, Device::Pool::DEFAULT, false, false, true);
			downsampled_gbuffer_data.tmp_linear_depth= Device::RenderTarget::Create("Linear depth tmp", device, downsampled_width, downsampled_height, Device::PixelFormat::G32R32F, Device::Pool::DEFAULT, false, false, true);
			size_t levels_count = downsampled_gbuffer_data.blurred_linear_depth->GetTexture() ? downsampled_gbuffer_data.blurred_linear_depth->GetTexture()->GetMipsCount() : 0;
			downsampled_gbuffer_data.blurred_linear_depth_mips.resize(levels_count);
			downsampled_gbuffer_data.tmp_linear_depth_mips.resize(levels_count);
			for (size_t level = 0; level < levels_count; level++)
			{
				downsampled_gbuffer_data.blurred_linear_depth_mips[level] = Device::RenderTarget::Create("Linear Depth Mip", device, downsampled_gbuffer_data.blurred_linear_depth->GetTexture(), static_cast< int >( level ) );
				downsampled_gbuffer_data.tmp_linear_depth_mips[level] = Device::RenderTarget::Create("Linear Depth tmp Mip", device, downsampled_gbuffer_data.tmp_linear_depth->GetTexture(), static_cast< int >( level ) );
			}

			if (gi_enabled)
			{
				downsampled_gbuffer_data.blurred_direct_light = Device::RenderTarget::Create("Direct light blurred", device, downsampled_width, downsampled_height, target_format, Device::Pool::DEFAULT, false, false, true);
				downsampled_gbuffer_data.tmp_direct_light = Device::RenderTarget::Create("Direct light tmp", device, downsampled_width, downsampled_height, target_format, Device::Pool::DEFAULT, false, false, true);
				downsampled_gbuffer_data.blurred_direct_light_mips.resize(levels_count);
				downsampled_gbuffer_data.tmp_direct_light_mips.resize(levels_count);

				for (size_t level = 0; level < levels_count; level++)
				{
					downsampled_gbuffer_data.blurred_direct_light_mips[level] = Device::RenderTarget::Create("Direct light mip", device, downsampled_gbuffer_data.blurred_direct_light->GetTexture(), static_cast< int >( level ) );
					downsampled_gbuffer_data.tmp_direct_light_mips[level] = Device::RenderTarget::Create("Direct light tmp mip", device, downsampled_gbuffer_data.tmp_direct_light->GetTexture(), static_cast< int >( level ) );
				}
			}
		}

		{
			downsampled_pass_data.downsample_mip = 2;
			downsampled_pass_data.color_pyramid = std::make_unique<DownsampledPyramid>(device, GetMainTarget(), static_cast<uint32_t>(downsampled_pass_data.downsample_mip + 1), Device::PixelFormat::A8R8G8B8, true, "Downsampled pass color pyramid");
			auto size = downsampled_gbuffer_data.depth_pyramid->GetLevelData( static_cast< uint32_t >( downsampled_pass_data.downsample_mip)).render_target->GetSize();
			downsampled_pass_data.depth_stencil = Device::RenderTarget::Create("Downsampled Depth", device, size.x, size.y, Device::PixelFormat::D24S8, Device::Pool::DEFAULT, false, true);

			downsampled_pass_data.wetness = Device::RenderTarget::Create("Downscaled Wetness", device, size.x, size.y, Device::PixelFormat::A16B16G16R16F, Device::Pool::DEFAULT, true, false);
		}

		{
			int water_downsample_scale = 2;
			downscaled_water_data.target_color = Device::RenderTarget::Create("Downscaled Water Color", device, frame_size.x / water_downsample_scale, frame_size.y / water_downsample_scale, Device::PixelFormat::A16B16G16R16F, Device::Pool::DEFAULT, true, false);
			downscaled_water_data.downsampled_color = Device::RenderTarget::Create("Downscaled Water Downsampled Color", device, frame_size.x / water_downsample_scale, frame_size.y / water_downsample_scale, target_format, Device::Pool::DEFAULT, true, false);
			downscaled_water_data.downsampled_depth = Device::RenderTarget::Create("Downscaled Water Downsampled Depth", device, frame_size.x / water_downsample_scale, frame_size.y / water_downsample_scale, Device::PixelFormat::D24S8, Device::Pool::DEFAULT, false, false);
			downscaled_water_data.downsample_scale_data = water_downsample_scale;
		}

		{
			for (auto& layer : reprojection_data.layers)
			{
				layer.direct_light_data = Device::RenderTarget::Create("Reprojection Color", device, frame_size.x, frame_size.y, target_format, Device::Pool::DEFAULT, true, false, false);
				layer.normal_data = Device::RenderTarget::Create("Reprojection Normal", device, frame_size.x, frame_size.y, Device::PixelFormat::A8R8G8B8, Device::Pool::DEFAULT, false, false, false);
				layer.depth_data = Device::RenderTarget::Create("Reprojection Depth", device, frame_size.x, frame_size.y, Device::PixelFormat::R32F, Device::Pool::DEFAULT, false, false, false);
			}

			reprojection_data.curr_layer = &(reprojection_data.layers[0]);
			reprojection_data.prev_layer = &(reprojection_data.layers[1]);
		}

		{
			bloom_data.downsample = 2;
			bloom_data.cutoff = 0.0f;
			int width = frame_size.x / bloom_data.downsample;
			int height = frame_size.y / bloom_data.downsample;

			bloom_data.bloom = Device::RenderTarget::Create("Bloom", device, width, height, Device::PixelFormat::R11G11B10F, Device::Pool::DEFAULT, true, false, true);
			size_t levels_count = bloom_data.bloom->GetTexture() ? bloom_data.bloom->GetTexture()->GetMipsCount() : 0;
			bloom_data.bloom_mips.resize(levels_count);
			for (size_t level = 0; level < levels_count; level++)
			{
				bloom_data.bloom_mips[level] = Device::RenderTarget::Create("Bloom Mips " + std::to_string(level), device, bloom_data.bloom->GetTexture(), static_cast< int >( level));
			}

			bloom_data.tmp_bloom = Device::RenderTarget::Create("Tmp Bloom", device, width, height, Device::PixelFormat::R11G11B10F, Device::Pool::DEFAULT, true, false, true);
			bloom_data.tmp_bloom_mips.resize(levels_count);
			for (size_t level = 0; level < levels_count; level++)
			{
				bloom_data.tmp_bloom_mips[level] = Device::RenderTarget::Create("Tmp Bloom Mips " + std::to_string(level), device, bloom_data.tmp_bloom->GetTexture(), static_cast< int >( level));
			}
		}

		if (dof_enabled)
		{
			dof_data.downsample_mip = 2;
			assert(dof_data.downsample_mip > 0);

			const uint32_t downsample = (uint32_t)1 << (uint32_t)dof_data.downsample_mip;
			dof_data.downsample = downsample;
			const auto frame_size = device->GetFrameSize();
			const int width = frame_size.x / downsample;
			const int height = frame_size.y / downsample;

			dof_data.background = Device::RenderTarget::Create("DoF Background", device, width, height, Device::PixelFormat::A16B16G16R16F, Device::Pool::DEFAULT, false, false, false);
			dof_data.foreground = Device::RenderTarget::Create("DoF Foreground", device, width, height, Device::PixelFormat::A16B16G16R16F, Device::Pool::DEFAULT, false, false, false);
			dof_data.foreground_tmp = Device::RenderTarget::Create("Tmp DoF Blur", device, width, height, Device::PixelFormat::A16B16G16R16F, Device::Pool::DEFAULT, false, false, false);
			dof_data.background_tmp = Device::RenderTarget::Create("Tmp DoF Blur 2", device, width, height, Device::PixelFormat::A16B16G16R16F, Device::Pool::DEFAULT, false, false, false);
		}
	}

	void RenderGraph::OnLostDevice()
	{
		device_passes.Clear();

		meta_passes.clear();

		screenspace_tracer.OnLostDevice();
		resampler.OnLostDevice();
		post_process.OnLostDevice();
		smaa.OnLostDevice();

		depth.reset();
		srgbHelper[0].reset();
		srgbHelper[1].reset();
		linearHelper.reset();
		gbuffer_normals.reset();
		gbuffer_direct_light.reset();
		desaturation_target.reset();

		global_illumination_data = GlobalIlluminationData();
		screenspace_shadows_data = ScreenspaceShadowsData();
		rays_data = RaysData();
		downsampled_gbuffer_data = DownsampledGBufferData();
		downsampled_pass_data = DownsampledPassData();
		downscaled_water_data = DownscaledWaterData();
		reprojection_data.layers[0] = Reprojection::Layer();
		reprojection_data.layers[1] = Reprojection::Layer();
		bloom_data = BloomData();
		dof_data = DOFData();
	}

	void RenderGraph::OnDestroyDevice()
	{
		device = nullptr;

		screenspace_tracer.OnDestroyDevice();
		resampler.OnDestroyDevice();
		post_process.OnDestroyDevice();
		smaa.OnDestroyDevice();

		environment_ggx_tex = ::Texture::Handle();
	}

	void RenderGraph::Update(float frame_length)
	{
		post_process.Update(frame_length);
	}

	void RenderGraph::Reload()
	{
		resampler.reload();
		post_process.reload(device);
		smaa.Reload(device);
		screenspace_tracer.reload(device);
	}

	void RenderGraph::Record(
		const std::function<void(MetaPass&)>& record,
		simd::color clear_color, 
		bool shadows_enabled,
		bool gi_enabled,
		bool gi_high,
		bool ss_high,
		bool dof_enabled,
		bool water_downscaled,
		float bloom_strength,
		bool time_manipulation_effects,
		std::function<void()> callback_render_to_texture, std::function<void(Device::Pass*)> callback_render)
	{
		this->callback_render_to_texture = callback_render_to_texture;
		this->callback_render = callback_render;

		screenspace_tracer.UpdateAsyncResources();

		frame_to_dynamic_scale.x = Device::DynamicResolution::Get().GetFrameToDynamicScale().x;
		frame_to_dynamic_scale.y = Device::DynamicResolution::Get().GetFrameToDynamicScale().y;
		const auto backbuffer_resolution = Device::DynamicResolution::Get().GetBackbufferResolution();
		const auto backbuffer_to_frame_scale = Device::DynamicResolution::Get().GetBackbufferToFrameScale();
		frame_resolution = simd::vector4(
			backbuffer_resolution.x * backbuffer_to_frame_scale.x,
			backbuffer_resolution.y * backbuffer_to_frame_scale.y, 0.0f, 0.0f);

		std::swap(reprojection_data.prev_layer, reprojection_data.curr_layer);

		if (auto* camera = Scene::System::Get().GetCamera())
		{
			simd::matrix proj_matrix = camera->GetFinalProjectionMatrix();
			simd::matrix view_matrix = camera->GetViewMatrix();
			simd::matrix viewproj_matrix = view_matrix * proj_matrix;

			reprojection_data.curr_layer->viewproj_matrix = viewproj_matrix;
			reprojection_data.curr_layer->inv_viewproj_matrix = viewproj_matrix.inverse();

			reprojection_data.curr_layer->proj_matrix = proj_matrix;
			reprojection_data.curr_layer->inv_proj_matrix = proj_matrix.inverse();

			reprojection_data.curr_layer->frame_to_dynamic_scale = simd::vector4(
				Device::DynamicResolution::Get().GetFrameToDynamicScale().x,
				Device::DynamicResolution::Get().GetFrameToDynamicScale().y,
				0.0f, 0.0f);
		}

		std::shared_ptr<CubemapTask> cubemap_task;
		if (auto cubemap_manager = Scene::System::Get().GetCubemapManager())
			cubemap_task = cubemap_manager->PopCubemapTask();

		meta_passes.clear();
		meta_passes.reserve(16);

		AddComputeMetaPass(record, cubemap_task);
		AddShadowsMetaPasses(record, shadows_enabled);
		AddZPrePassMetaPass(record);
		AddCubemapMetaPasses(record, cubemap_task);
		AddScreenSpaceShadowsMetaPass(record);
		AddGIMetaPass(record, gi_enabled, gi_high);
		AddOpaqueMetaPass(record, clear_color, ss_high);
		AddWaterMetaPass(record, water_downscaled, ss_high);
		AddAlphaMetaPasses(record, gi_enabled, ss_high);
		AddPostProcessMetaPass(record, dof_enabled, bloom_strength, time_manipulation_effects);
	}

	void RenderGraph::Flush(const std::function<void(MetaPass&)>& flush)
	{
		for (auto& meta_pass : meta_passes)
			flush(*meta_pass);

		callback_render_to_texture = nullptr;
		callback_render = nullptr;
	}

	void RenderGraph::SetGlobalIlluminationSettings(const Environment::Data& settings)
	{
		indirect_light_area = settings.Value(Environment::EnvParamIds::Float::GiIndirectLightArea);
		indirect_light_rampup = settings.Value(Environment::EnvParamIds::Float::GiIndirectLightRampup);
		ambient_occlusion_power = settings.Value(Environment::EnvParamIds::Float::GiAmbientOcclusionPower);
		screenspace_thickness_angle = settings.Value(Environment::EnvParamIds::Float::GiThicknessAngle);
	}

	void RenderGraph::SetRainSettings(const Environment::Data& settings)
	{
		rain.dist = settings.Value(Environment::EnvParamIds::Float::RainDist);
		rain.amount = settings.Value(Environment::EnvParamIds::Float::RainAmount);
		rain.intensity = settings.Value(Environment::EnvParamIds::Float::RainIntensity);
		float hor_ang = settings.Value(Environment::EnvParamIds::Float::RainHorAngle);
		simd::vector3 hor_dir = simd::vector3(cos(hor_ang), sin(hor_ang), 0.0f);
		float vert_ang = settings.Value(Environment::EnvParamIds::Float::RainVertAngle);
		rain.fall_dir = simd::vector4(hor_dir * sin(vert_ang) + simd::vector3(0.0f, 0.0f, 1.0f) * cos(vert_ang), 0.0f);
		rain.turbulence = settings.Value(Environment::EnvParamIds::Float::RainTurbulence);
	}

	void RenderGraph::SetCloudsSettings(const Environment::Data& settings)
	{
		clouds.scale = settings.Value(Environment::EnvParamIds::Float::CloudsScale);
		clouds.intensity= settings.Value(Environment::EnvParamIds::Float::CloudsIntensity);
		clouds.midpoint = settings.Value(Environment::EnvParamIds::Float::CloudsMidpoint);
		clouds.sharpness = settings.Value(Environment::EnvParamIds::Float::CloudsSharpness);
		float ang = settings.Value(Environment::EnvParamIds::Float::CloudsAngle);
		simd::vector2 velocity = simd::vector2(cos(ang), sin(ang)) * settings.Value(Environment::EnvParamIds::Float::CloudsSpeed);
		clouds.velocity = simd::vector4(velocity.x, velocity.y, 0.0f, 0.0f);
		clouds.fade_radius = settings.Value(Environment::EnvParamIds::Float::CloudsFadeRadius);
		clouds.pre_fade = settings.Value(Environment::EnvParamIds::Float::CloudsPreFade);
		clouds.post_fade = settings.Value(Environment::EnvParamIds::Float::CloudsPostFade);
	}

	void RenderGraph::SetWaterSettings(const Environment::Data& settings)
	{
		water.color_open = simd::vector4(
			settings.Value(Environment::EnvParamIds::Vector3::WaterOpenWaterColor).x,
			settings.Value(Environment::EnvParamIds::Vector3::WaterOpenWaterColor).y,
			settings.Value(Environment::EnvParamIds::Vector3::WaterOpenWaterColor).z,
			settings.Value(Environment::EnvParamIds::Float::WaterOpenWaterMurkiness));

		water.color_terrain = simd::vector4(
			settings.Value(Environment::EnvParamIds::Vector3::WaterTerrainWaterColor).x,
			settings.Value(Environment::EnvParamIds::Vector3::WaterTerrainWaterColor).y,
			settings.Value(Environment::EnvParamIds::Vector3::WaterTerrainWaterColor).z,
			settings.Value(Environment::EnvParamIds::Float::WaterTerrainWaterMurkiness));


		simd::vector4 skybox_angles_f4;
		water.subsurface_scattering = settings.Value(Environment::EnvParamIds::Float::WaterSubsurfaceScattering);
		water.caustics_mult = settings.Value(Environment::EnvParamIds::Float::WaterCausticsMult);

		water.refraction_index = settings.Value(Environment::EnvParamIds::Float::WaterRefractionIndex);

		water.reflectiveness = settings.Value(Environment::EnvParamIds::Float::WaterReflectiveness);

		water.wind_direction.x = cosf(settings.Value(Environment::EnvParamIds::Float::WaterWindDirectionAngle));
		water.wind_direction.y = sinf(settings.Value(Environment::EnvParamIds::Float::WaterWindDirectionAngle));
		water.wind_direction.z = 0.0f;
		water.wind_direction.w = 0.0f;

		water.wind_intensity = settings.Value(Environment::EnvParamIds::Float::WaterWindIntensity);
		water.wind_speed = settings.Value(Environment::EnvParamIds::Float::WaterWindSpeed);
		water.directionness = settings.Value(Environment::EnvParamIds::Float::WaterDirectionness);

		water.swell_intensity = settings.Value(Environment::EnvParamIds::Float::WaterSwellIntensity);
		water.swell_height = settings.Value(Environment::EnvParamIds::Float::WaterSwellHeight);
		water.swell_angle = settings.Value(Environment::EnvParamIds::Float::WaterSwellAngle);
		water.swell_period = settings.Value(Environment::EnvParamIds::Float::WaterSwellPeriod);

		water.flow_intensity = settings.Value(Environment::EnvParamIds::Float::WaterFlowIntensity);
		water.flow_foam = settings.Value(Environment::EnvParamIds::Float::WaterFlowFoam);
		water.flow_turbulence = settings.Value(Environment::EnvParamIds::Float::WaterFlowTurbulence);

		water.height_offset = settings.Value(Environment::EnvParamIds::Float::WaterHeightOffset);
	}

	void RenderGraph::SetAreaSettings(const Environment::Data& settings, const float time_scale, const simd::vector2* tile_scroll_override )
	{
		const auto ground_scale = settings.Value(Environment::EnvParamIds::Float::GroundScale);
		const auto ground_scrollspeed_x = settings.Value(Environment::EnvParamIds::Float::GroundScrollSpeedX);
		const auto ground_scrollspeed_y = settings.Value(Environment::EnvParamIds::Float::GroundScrollSpeedY);
		const auto tile_scroll = tile_scroll_override ? *tile_scroll_override : ( simd::vector2(ground_scrollspeed_x, ground_scrollspeed_y) * time_scale * Scene::System::Get().GetFrameTime() );

		dust_color = simd::vector4(settings.Value(Environment::EnvParamIds::Vector3::DustColor), 0.0f);
		ground_scalemove_uv = simd::vector4(
				ground_scale,
				ground_scale,
				tile_scroll.x * FileReader::TGTReader::WORLD_SPACE_TILE_SIZE,
				tile_scroll.y * FileReader::TGTReader::WORLD_SPACE_TILE_SIZE);
	}

	void RenderGraph::SetBloomSettings(const Environment::Data& settings)
	{
		//we're fixing bloom intensity to 0.3 until things settle
		//bloom_data.intensity = settings.Value(Environment::EnvParamIds::Float::PostProcessingBloomIntensity);
		bloom_data.intensity = 0.3f * settings.Value(Environment::EnvParamIds::Float::CameraExposure);
		bloom_data.cutoff = settings.Value(Environment::EnvParamIds::Float::PostProcessingBloomCutoff);
		bloom_data.weight_mult = settings.Value(Environment::EnvParamIds::Float::PostProcessingBloomRadius);
	}

	void RenderGraph::SetDofSettings(const Environment::Data& settings)
	{
		dof_data.active = settings.Value(Environment::EnvParamIds::Bool::PostProcessingDepthOfFieldIsEnabled);
		if (auto* camera = Scene::System::Get().GetCamera())
		{
			const auto camera_dof_settings = camera->GetDoFSettings();
			float max_focus_distance = camera_dof_settings.max_focus_distance > 0.001f ? camera_dof_settings.max_focus_distance : std::numeric_limits<float>::max();
			dof_data.focus_distance = std::min(max_focus_distance, settings.Value(Environment::EnvParamIds::Float::PostProcessingDoFFocusDistance));
			dof_data.focus_range = settings.Value(Environment::EnvParamIds::Float::PostProcessingDoFFocusRange);
			dof_data.blur_scale = settings.Value(Environment::EnvParamIds::Float::PostProcessingDoFBlurScaleBackground);
			dof_data.blur_foreground_scale = settings.Value(Environment::EnvParamIds::Float::PostProcessingDoFBlurScaleForeground);
			dof_data.background_transition = settings.Value(Environment::EnvParamIds::Float::PostProcessingDoFTransitionBackground);
			dof_data.foreground_transition = settings.Value(Environment::EnvParamIds::Float::PostProcessingDoFTransitionForeground);
			if (camera_dof_settings.focus_distance > 0.001f)
				dof_data.focus_distance = camera_dof_settings.focus_distance;
			if (camera_dof_settings.focus_range > 0.001f)
				dof_data.focus_range = camera_dof_settings.focus_range;
		}
	}

	void RenderGraph::SetPostProcessSettings(const Environment::Data& settings)
	{
		post_process.SetOriginalIntensity(settings.Value(Environment::EnvParamIds::Float::PostProcessingOriginalIntensity));
		post_process.SetExposure(settings.Value(Environment::EnvParamIds::Float::CameraExposure));

		std::wstring names[2];
		float ratio = 1.0f - settings.Value(Environment::EnvParamIds::String::PostTransformTex).weights[0];
		::Texture::Handle volume_handles[2];
		for (int i = 0; i < 2; i++)
		{
			std::wstring name = settings.Value(Environment::EnvParamIds::String::PostTransformTex).Get(i);
			if (name != L"")
				volume_handles[i] = ::Texture::Handle(::Texture::VolumeTextureDesc(name));
		}
		post_process.SetPostTransformVolumes(volume_handles, ratio);
	}

	void RenderGraph::SetEnvironmentSettings(const Environment::Data& settings, const float time_scale, const simd::vector2* tile_scroll_override )
	{
		SetGlobalIlluminationSettings(settings);
		SetRainSettings(settings);
		SetCloudsSettings(settings);
		SetWaterSettings(settings);
		SetAreaSettings(settings, time_scale, tile_scroll_override);
		SetBloomSettings(settings);
		SetDofSettings(settings);
		SetPostProcessSettings(settings);
	}

	Device::Handle<Device::Pass> RenderGraph::FindPass(const Memory::DebugStringA<>& name, Device::ColorRenderTargets render_targets, Device::RenderTarget* depth_stencil, Device::ClearTarget clear_target, simd::color clear_color, float clear_z)
	{
		Memory::Lock lock(device_passes_mutex);
		return device_passes.FindOrCreate(PassKey(render_targets, depth_stencil, clear_target, clear_color, clear_z), [&]()
		{
			return Device::Pass::Create(name, device, render_targets, depth_stencil, clear_target, clear_color, clear_z);
		});
	}

	Device::Handle<Device::Pass> RenderGraph::GetZPrePass()
	{
		if (depth)
			return FindPass("ZPrepass", Device::ColorRenderTargets(), depth.get(), (Device::ClearTarget)((UINT)Device::ClearTarget::DEPTH | (UINT)Device::ClearTarget::STENCIL), simd::color(0), 1.0f);
		return {};
	}

	Device::Handle<Device::Pass> RenderGraph::GetMainPass(bool use_gi)
	{
		Device::ColorRenderTargets rts;
		rts.push_back(GetMainTarget());
		if (use_gi && gbuffer_direct_light.get())
		{
			rts.push_back(gbuffer_direct_light.get());
			rts.push_back(gbuffer_normals.get());
		}
		if (rts[0])
			return FindPass("Main", rts, depth.get(), Device::ClearTarget::NONE, simd::color(0), 1.0f); // Always use dynamic clear on main passes (so we can warmup/skip them).
		return {};
	}

	Pass RenderGraph::BuildMipBuilderPass(std::vector<Device::RenderTarget*> dst_mips, TargetResampler::FilterTypes filter_type, float gamma)
	{
		return Pass(L"MipBuilder", [=](auto& command_buffer)
		{
			const auto resolution_scale = simd::vector2(1.0f, 1.0f);

			for (size_t level = 1; level < dst_mips.size(); level++)
			{
				resampler.ResampleColor(command_buffer, dst_mips[level - 1], dst_mips[level], resolution_scale, filter_type, gamma);
			}
		});
	}

	Pass RenderGraph::BuildDownsampledPass(Device::RenderTarget *zero_data, TargetResampler::RenderTargetList mip_targets)
	{
		return Pass(L"Downsampler", [=](auto& command_buffer)
		{
			const auto resolution_scale = simd::vector2(1.0f, 1.0f);

			for (size_t mip_index = 1; mip_index < mip_targets.size(); mip_index++)
			{
				resampler.BuildDownsampled(command_buffer, zero_data, mip_targets[mip_index], resolution_scale);
			}
		});
	}

	Pass RenderGraph::BuildDepthResamplerPass(Device::RenderTarget* src_target, Device::RenderTarget* dst_depth_target, TargetResampler::FilterTypes filter_type)
	{
		return Pass(L"DepthResampler", [=](auto& command_buffer)
		{
			const auto resolution_scale = simd::vector2(1.0f, 1.0f);

			resampler.ResampleDepth(command_buffer, src_target, dst_depth_target, resolution_scale, filter_type);
		});
	}

	Pass RenderGraph::BuildComputePass(std::shared_ptr<CubemapTask> cubemap_task)
	{
		Device::UniformInputs pass_uniform_inputs;
		Device::BindingInputs pass_binding_inputs;
		pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::PassType)).SetInt(&pass_types[WaterPass]));
		pass_binding_inputs.push_back(Device::BindingInput(Device::IdHash(DrawDataNames::DepthSampler)).SetTexture(depth->GetTexture()));

		auto device_pass = FindPass("Compute", {}, depth.get(), Device::ClearTarget::NONE, simd::color(0), 1.0f);
		auto pass = Pass(L"Compute", device_pass, ShaderPass::Color, pass_uniform_inputs, pass_binding_inputs);
		pass.compute_pass = true;

		pass.AddSubPass({ L"Tiled Lighting",[=](auto& command_buffer) { Scene::System::Get().ComputeTiles(command_buffer, cubemap_task); }});
	#if WIP_PARTICLE_SYSTEM
		pass.AddSubPass({ DrawCalls::BlendMode::Compute, Entity::ViewType::Camera });
	#endif

		return pass;
	}

	Pass RenderGraph::BuildComputePostPass()
	{
		Device::UniformInputs pass_uniform_inputs;
		Device::BindingInputs pass_binding_inputs;
		pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::PassType)).SetInt(&pass_types[WaterPass]));
		pass_binding_inputs.push_back(Device::BindingInput(Device::IdHash(DrawDataNames::DepthSampler)).SetTexture(depth->GetTexture()));

		auto device_pass = FindPass("Compute Post", {}, depth.get(), Device::ClearTarget::NONE, simd::color(0), 1.0f);
		auto pass = Pass(L"Compute Post", device_pass, ShaderPass::Color, pass_uniform_inputs, pass_binding_inputs);
		pass.compute_pass = true;

	#if WIP_PARTICLE_SYSTEM
		pass.AddSubPass({ DrawCalls::BlendMode::ComputePost, Entity::ViewType::Camera });
	#endif

		return pass;
	}

	Pass RenderGraph::BuildShadowPass(Scene::Light* light, unsigned light_index)
	{
		Device::UniformInputs pass_uniform_inputs;
		Device::BindingInputs pass_binding_inputs;
		pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::PassType)).SetInt(&pass_types[ShadowPass]));
		pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::ViewStartIndex)).SetUInt(&light->GetStartIndex()));

		auto* depth_stencil = Scene::System::Get().GetShadowManager().GetShadowRenderTarget();
		Device::ClearTarget clear_target = light_index == 0 ? (Device::ClearTarget)((UINT)Device::ClearTarget::DEPTH | (UINT)Device::ClearTarget::STENCIL) : Device::ClearTarget::NONE;
		const auto shadow_atlas_rect = Scene::System::Get().GetShadowManager().GetShadowAtlasRect(light_index);
		auto device_pass = FindPass("Shadow", Device::ColorRenderTargets(), depth_stencil, clear_target, simd::color(0), 1.0f);
		auto pass = Pass(Memory::DebugStringW<>(L"Shadow_") + std::to_wstring(light_index), device_pass, ShaderPass::DepthOnly, pass_uniform_inputs, pass_binding_inputs, shadow_atlas_rect, shadow_atlas_rect);

		const auto view_type = (Entity::ViewType)((unsigned)Entity::ViewType::Light0 + light_index);
		const auto depth_bias_and_slope_scale = Scene::System::Get().GetLightDepthBiasAndSlopeScale(*light);
		const auto depth_bias = depth_bias_and_slope_scale.first;
		const auto slope_scale = depth_bias_and_slope_scale.second;

		pass.AddSubPass({ DrawCalls::BlendMode::OpaqueShadowOnly, view_type, false, depth_bias, slope_scale });
		pass.AddSubPass({ DrawCalls::BlendMode::ShadowOnlyAlphaTest, view_type, false, depth_bias, slope_scale });
		pass.AddSubPass({ DrawCalls::BlendMode::Opaque, view_type, false, depth_bias, slope_scale });
		pass.AddSubPass({ DrawCalls::BlendMode::DepthOverride, view_type, false, depth_bias, slope_scale });
		pass.AddSubPass({ L"OverrideDepth", [=](auto& command_buffer) { screenspace_tracer.OverrideDepth(device, command_buffer, pass.pass.Get(), depth_bias, slope_scale); } });
		pass.AddSubPass({ DrawCalls::BlendMode::AlphaTestWithShadow, view_type, false, depth_bias, slope_scale });

		return pass;
	}

	Pass RenderGraph::BuildZPrePass()
	{
		Device::UniformInputs pass_uniform_inputs;
		Device::BindingInputs pass_binding_inputs;
		pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::PassType)).SetInt(&pass_types[ZPrepass]));

		auto device_pass = GetZPrePass();
		const auto display_size = (simd::vector2(depth->GetSize()) * Device::DynamicResolution::Get().GetFrameToDynamicScale()).round();
		auto pass = Pass(L"ZPrePass", device_pass, ShaderPass::DepthOnly, pass_uniform_inputs, pass_binding_inputs, display_size, display_size, Device::ClearTarget::NONE, 0, 0.0f, true/*z_prepass*/);

		pass.AddSubPass({ DrawCalls::BlendMode::Opaque, Entity::ViewType::Camera });
		pass.AddSubPass({ DrawCalls::BlendMode::DepthOverride, Entity::ViewType::Camera });
		pass.AddSubPass({ L"OverrideDepth", [=](auto& command_buffer) { screenspace_tracer.OverrideDepth(device, command_buffer, pass.pass.Get(), 0.0f, 0.0f); } });
		pass.AddSubPass({ DrawCalls::BlendMode::AlphaTestWithShadow, Entity::ViewType::Camera });
		pass.AddSubPass({ DrawCalls::BlendMode::OpaqueNoShadow, Entity::ViewType::Camera });
		pass.AddSubPass({ DrawCalls::BlendMode::AlphaTest, Entity::ViewType::Camera });
		pass.AddSubPass({ DrawCalls::BlendMode::AlphaTestNoGI, Entity::ViewType::Camera });
		pass.AddSubPass({ DrawCalls::BlendMode::OutlineZPrepass, Entity::ViewType::Camera });

		return pass;
	}

	Pass RenderGraph::BuildCubemapPass(std::shared_ptr<CubemapTask> cubemap_task, Device::RenderTarget* target, Device::RenderTarget* depth_target, unsigned int face_id)
	{
		Device::UniformInputs pass_uniform_inputs;
		Device::BindingInputs pass_binding_inputs;
		pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::PassType)).SetInt(&pass_types[LightingPass]));
		pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::PassDownscale)).SetInt(&(downsampled_gbuffer_data.depth_pyramid->GetLevelData(0).downsample_draw_data)));
		pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::ViewStartIndex)).SetUInt(&cubemap_task->face_cameras[face_id].GetStartIndex()));
		Scene::System::Get().GetCubemapManager()->BindInputs(face_id, pass_binding_inputs);

		auto device_pass = FindPass("Cubemap", { target }, depth_target, (Device::ClearTarget)((UINT)Device::ClearTarget::DEPTH | (UINT)Device::ClearTarget::COLOR), simd::color(0), 1.0f);
		auto pass = Pass(Memory::DebugStringW<>(L"CubemapFace") + std::to_wstring(face_id), device_pass, ShaderPass::Color, pass_uniform_inputs, pass_binding_inputs, target->GetSize(), target->GetSize());

		const auto view_type = (Entity::ViewType)((unsigned)Entity::ViewType::Face0 + face_id);

		pass.AddSubPass({ L"CubemapTask", [cubemap_task](auto& command_buffer) {} }); // Keep cubemap_task alive somehow.
		pass.AddSubPass({ DrawCalls::BlendMode::Opaque, view_type });
		pass.AddSubPass({ DrawCalls::BlendMode::AlphaTestWithShadow, view_type });
		pass.AddSubPass({ DrawCalls::BlendMode::OpaqueNoShadow, view_type });
		pass.AddSubPass({ DrawCalls::BlendMode::AlphaTest, view_type });
		pass.AddSubPass({ DrawCalls::BlendMode::DepthOnlyAlphaTest, view_type });
		pass.AddSubPass({ DrawCalls::BlendMode::NoZWriteAlphaBlend, view_type });
		pass.AddSubPass({ DrawCalls::BlendMode::BackgroundMultiplicitiveBlend, view_type });
		pass.AddSubPass({ DrawCalls::BlendMode::AlphaBlend, view_type });
		pass.AddSubPass({ DrawCalls::BlendMode::DownScaledAlphaBlend, view_type });
		pass.AddSubPass({ DrawCalls::BlendMode::DownScaledAddablend, view_type });
		pass.AddSubPass({ DrawCalls::BlendMode::PremultipliedAlphaBlend, view_type });
		pass.AddSubPass({ DrawCalls::BlendMode::ForegroundAlphaBlend, view_type });
		pass.AddSubPass({ DrawCalls::BlendMode::MultiplicitiveBlend, view_type });
		pass.AddSubPass({ DrawCalls::BlendMode::Addablend, view_type });
		pass.AddSubPass({ DrawCalls::BlendMode::Additive, view_type });
		pass.AddSubPass({ DrawCalls::BlendMode::Subtractive, view_type });
		pass.AddSubPass({ DrawCalls::BlendMode::AlphaTestNoGI, view_type });
		pass.AddSubPass({ DrawCalls::BlendMode::AlphaBlendNoGI, view_type });
		pass.AddSubPass({ DrawCalls::BlendMode::RoofFadeBlend, view_type });

		return pass;
	}

	Pass RenderGraph::BuildWetnessPass(bool ss_high)
	{
		Device::UniformInputs pass_uniform_inputs;
		Device::BindingInputs pass_binding_inputs;
		pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::PassType)).SetInt(&pass_types[LightingPass]));
		pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::PassDownscale)).SetInt(&(downsampled_gbuffer_data.depth_pyramid->GetLevelData(0).downsample_draw_data)));
		AddResolvedDepthDrawData(pass_uniform_inputs, pass_binding_inputs, 0);
		AddScreenspaceDrawData(pass_uniform_inputs, pass_binding_inputs, ss_high);

		auto device_pass = FindPass("Wetness", { downsampled_pass_data.wetness.get() }, downsampled_pass_data.depth_stencil.get(), Device::ClearTarget::COLOR, simd::color(0), 1.0f);
		const auto display_size = (simd::vector2(device_pass->GetColorRenderTargets()[0].get()->GetSize()) * Device::DynamicResolution::Get().GetFrameToDynamicScale()).round();
		auto pass = Pass(L"Wetness", device_pass, ShaderPass::Color, pass_uniform_inputs, pass_binding_inputs, display_size, display_size);

		pass.AddSubPass({ DrawCalls::BlendMode::Wetness, Entity::ViewType::Camera });

		return pass;
	}

	Pass RenderGraph::BuildDownscaledPass(bool ss_high)
	{
		Device::UniformInputs pass_uniform_inputs;
		Device::BindingInputs pass_binding_inputs;
		pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::PassType)).SetInt(&pass_types[LightingPass]));
		pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::PassDownscale)).SetInt(&(downsampled_gbuffer_data.depth_pyramid->GetLevelData((uint32_t)downsampled_pass_data.downsample_mip).downsample_draw_data)));
		AddResolvedDepthDrawData(pass_uniform_inputs, pass_binding_inputs, (uint32_t)downsampled_pass_data.downsample_mip);
		AddScreenspaceDrawData(pass_uniform_inputs, pass_binding_inputs, ss_high);

		auto device_pass = FindPass("Downscaled", { downsampled_pass_data.color_pyramid->GetLevelData(static_cast<uint32_t>(downsampled_pass_data.downsample_mip)).render_target }, downsampled_pass_data.depth_stencil.get(), Device::ClearTarget::COLOR, simd::color(0), 1.0f);
		const auto display_size = (simd::vector2(device_pass->GetColorRenderTargets()[0].get()->GetSize()) * Device::DynamicResolution::Get().GetFrameToDynamicScale()).round();
		auto pass = Pass(L"Downscaled", device_pass, ShaderPass::Color, pass_uniform_inputs, pass_binding_inputs, display_size, display_size);

		pass.AddSubPass({ DrawCalls::BlendMode::DownScaledAlphaBlend, Entity::ViewType::Camera });
		pass.AddSubPass({ DrawCalls::BlendMode::DownScaledAddablend, Entity::ViewType::Camera });

		return pass;
	}

	Pass RenderGraph::BuildColorPass(const Memory::SmallVector<DrawCalls::BlendMode, 8, Memory::Tag::Render>& blend_modes, bool ss_high, bool clear, simd::color clear_color)
	{
		Device::UniformInputs pass_uniform_inputs;
		Device::BindingInputs pass_binding_inputs;
		pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::PassType)).SetInt(&pass_types[LightingPass]));
		pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::PassDownscale)).SetInt(&(downsampled_gbuffer_data.depth_pyramid->GetLevelData(0).downsample_draw_data)));
		AddResolvedDepthDrawData(pass_uniform_inputs, pass_binding_inputs, 0);
		AddScreenspaceDrawData(pass_uniform_inputs, pass_binding_inputs, ss_high);

		auto device_pass = GetMainPass(UseGI(blend_modes[0]));
		const auto display_size = (simd::vector2(device_pass->GetColorRenderTargets()[0].get()->GetSize()) * Device::DynamicResolution::Get().GetFrameToDynamicScale()).round();
		auto pass = Pass(Memory::DebugStringW<>(L"Color_") + DrawCalls::GetBlendModeName(blend_modes[0]), device_pass, ShaderPass::Color, pass_uniform_inputs, pass_binding_inputs, display_size, display_size, GetMainClearFlags(clear), clear_color, 1.0f);

		for (auto& blend_mode : blend_modes)
		{
			assert(!gbuffer_direct_light || UseGI(blend_mode) == UseGI(blend_modes[0]));

			pass.AddSubPass({ blend_mode, Entity::ViewType::Camera });

			if (blend_mode == DrawCalls::BlendMode::DepthOverride)
				pass.AddSubPass({ L"OverrideDepth", [=](auto& command_buffer) { screenspace_tracer.OverrideDepth(device, command_buffer, pass.pass.Get(), 0.0f, 0.0f); } });

		#if (!defined( CONSOLE ) && !defined( __APPLE__ )) && (defined(TESTING_CONFIGURATION) || defined(DEVELOPMENT_CONFIGURATION)) && defined(ENABLE_DEBUG_VIEWMODES)
			if (Utility::GetCurrentViewMode() == Utility::ViewMode::DoubleSided) // Push a second draw with inverted culling.
				pass.AddSubPass({ blend_mode, Entity::ViewType::Camera, true });
		#endif
		}

		return pass;
	}

	Pass RenderGraph::BuildDownscaledWaterPass(DrawCalls::BlendMode blend_mode, bool ss_high)
	{
		Device::UniformInputs pass_uniform_inputs;
		Device::BindingInputs pass_binding_inputs;
		pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::PassType)).SetInt(&pass_types[WaterPass]));
		pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::PassDownscale)).SetInt(&downscaled_water_data.downsample_scale_data));
		AddWaterDrawData(pass_uniform_inputs, pass_binding_inputs, true);
		AddScreenspaceDrawData(pass_uniform_inputs, pass_binding_inputs, ss_high);

		auto device_pass = FindPass("Main Water", { downscaled_water_data.target_color.get() }, nullptr, Device::ClearTarget::COLOR, simd::color(0), 1.0f);
		const auto display_size = (simd::vector2(device_pass->GetColorRenderTargets()[0].get()->GetSize()) * Device::DynamicResolution::Get().GetFrameToDynamicScale()).round();
		auto pass = Pass(L"Downscaled Water", device_pass, ShaderPass::Color, pass_uniform_inputs, pass_binding_inputs, display_size, display_size);

		pass.AddSubPass({ blend_mode, Entity::ViewType::Camera });

		return pass;
	}

	Pass RenderGraph::BuildMainWaterPass(DrawCalls::BlendMode blend_mode, bool ss_high)
	{
		Device::UniformInputs pass_uniform_inputs;
		Device::BindingInputs pass_binding_inputs;
		pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::PassType)).SetInt(&pass_types[WaterPass]));
		AddWaterDrawData(pass_uniform_inputs, pass_binding_inputs, false);
		AddScreenspaceDrawData(pass_uniform_inputs, pass_binding_inputs, ss_high);

		auto device_pass = GetMainPass(UseGI(blend_mode));
		const auto display_size = (simd::vector2(device_pass->GetColorRenderTargets()[0].get()->GetSize()) * Device::DynamicResolution::Get().GetFrameToDynamicScale()).round();
		auto pass = Pass(L"Main Water", device_pass, ShaderPass::Color, pass_uniform_inputs, pass_binding_inputs, display_size, display_size);

		pass.AddSubPass({ blend_mode, Entity::ViewType::Camera });

		return pass;
	}

	Pass RenderGraph::BuildColorResamplerPass(Device::RenderTarget* src_target, Device::RenderTarget* dst_target)
	{
		return Pass(L"ColorResampler", [=](auto& command_buffer)
		{
			const auto resolution_scale = simd::vector2(1.0f, 1.0f);
			resampler.ResampleColor(command_buffer, src_target, dst_target, resolution_scale, TargetResampler::FilterTypes::Nearest, 1.0f);
		});
	}

	Pass RenderGraph::BuildLinearizeDepthPass(Device::RenderTarget* src_target, Device::RenderTarget* dst_target, int downscale)
	{
		return Pass(L"LinearizeDepth", [=](auto& command_buffer)
		{
			const simd::vector2 resolution_scale = Device::DynamicResolution::Get().GetFrameToDynamicScale();
			screenspace_tracer.LinearizeDepth(device, command_buffer, src_target->GetTexture(), dst_target, resolution_scale, src_target->GetSize(), 0, downscale);
		});
	}

	Pass RenderGraph::BuildBlurPass(std::vector<Device::RenderTarget*> src_mips, std::vector<Device::RenderTarget*> tmp_mips, float gamma)
	{
		return Pass(L"Blur", [=](auto& command_buffer)
		{
			const auto resolution_scale = simd::vector2(1.0f, 1.0f);

			assert(src_mips.size() == tmp_mips.size());
			int radius = 2;
			for (size_t level = 0; level < src_mips.size(); level++)
			{
				screenspace_tracer.ApplyBlur(device, command_buffer, src_mips[level], tmp_mips[level], gamma, 0.0f, resolution_scale, simd::vector2_int(radius, 0));
				screenspace_tracer.ApplyBlur(device, command_buffer, tmp_mips[level], src_mips[level], gamma, 0.0f, resolution_scale, simd::vector2_int(0, radius));
			}
		});
	}

	Pass RenderGraph::BuildDirectLightPostProcessPass(Device::RenderTarget* src_target, Device::RenderTarget* dst_target, int downscale)
	{
		return Pass(L"DirectLightPostProcess", [=](auto& command_buffer)
		{
			auto frame_to_dynamic_scale = Device::DynamicResolution::Get().GetFrameToDynamicScale();

			PostProcess::SynthesisMapInfo synthesis_map_info = Scene::System::Get().GetSynthesisMapInfo();
			post_process.RenderPostProcessedImage(device, command_buffer,
				src_target->GetTexture().Get(), nullptr, nullptr, nullptr, synthesis_map_info, 
				reprojection_data.curr_layer->depth_data->GetTexture().Get(), nullptr, dst_target, true, false,
				frame_to_dynamic_scale,
				Scene::System::Get().GetRitualEncounterSphere(),
				Scene::System::Get().GetRitualEncounterRatio() );
		});
	}

	Pass RenderGraph::BuildDepthAwareDownsamplerPass()
	{
		return Pass(L"DepthAwareDownsampler", [=](auto& command_buffer)
		{
			Device::RenderTarget *zero_depth = depth.get();
			TargetResampler::RenderTargetList pixel_offsets_targets = downsampled_gbuffer_data.pixel_offset_pyramid ? downsampled_gbuffer_data.pixel_offset_pyramid->GetTargetList() : TargetResampler::RenderTargetList();
			TargetResampler::RenderTargetList depth_mip_targets = downsampled_gbuffer_data.depth_pyramid->GetTargetList();
			TargetResampler::RenderTargetList offset_depth_mip_targets = downsampled_gbuffer_data.offset_depth_pyramid ? downsampled_gbuffer_data.offset_depth_pyramid->GetTargetList() : TargetResampler::RenderTargetList();
			const simd::vector2 resolution_scale = simd::vector2(1, 1);

			if (pixel_offsets_targets.size() > 0)
				resampler.DepthAwareDownsample(command_buffer, zero_depth, pixel_offsets_targets, resolution_scale);
			for (size_t mip_index = 1; mip_index < depth_mip_targets.size(); mip_index++)
			{
				resampler.BuildDownsampled(command_buffer, zero_depth, depth_mip_targets[mip_index], resolution_scale);
				if (pixel_offsets_targets.size() > 0 && offset_depth_mip_targets.size() > 0)
					resampler.BuildDownsampled(command_buffer, zero_depth, pixel_offsets_targets[mip_index]->GetTexture(), offset_depth_mip_targets[mip_index], resolution_scale);
			}
		});
	}

	Pass RenderGraph::BuildUpsampleGBufferPass()
	{
		return Pass(L"UpsampleGBuffer", [=](auto& command_buffer)
		{
			Device::RenderTarget* zero_depth = reprojection_data.curr_layer->depth_data.get();
			const TargetResampler::RenderTargetList pixel_offsets = downsampled_gbuffer_data.pixel_offset_pyramid ? downsampled_gbuffer_data.pixel_offset_pyramid->GetTargetList() : TargetResampler::RenderTargetList();
			const TargetResampler::RenderTargetList color_targets = downsampled_pass_data.color_pyramid->GetTargetList();
			const simd::vector2 resolution_scale = simd::vector2(1, 1);
			const TargetResampler::GBufferUpsamplingBlendMode blend_mode = TargetResampler::GBufferUpsamplingBlendMode::Additive;
			const uint32_t src_index = static_cast<uint32_t>(downsampled_pass_data.downsample_mip);
			const uint32_t dst_index = 0;
			const auto inv_proj_matrix = Scene::System::Get().GetCamera()->GetFinalProjectionMatrix().inverse();

			if(pixel_offsets.size() > 0)
				resampler.DepthAwareUpsample(command_buffer, zero_depth, pixel_offsets, color_targets, resolution_scale, inv_proj_matrix, blend_mode, src_index, dst_index);
			else
				resampler.DepthAwareUpsample(command_buffer, zero_depth, color_targets, resolution_scale, inv_proj_matrix, blend_mode, src_index, dst_index);
		});
	}

	Pass RenderGraph::BuildDesaturationPass()
	{
		Device::UniformInputs pass_uniform_inputs;
		Device::BindingInputs pass_binding_inputs;
		pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::PassType)).SetInt(&pass_types[LightingPass]));
		AddResolvedDepthDrawData(pass_uniform_inputs, pass_binding_inputs);

		auto device_pass = FindPass("Desaturation", { desaturation_target.get() }, depth.get(), Device::ClearTarget::COLOR, simd::color(0), 1.0f);
		const auto display_size = (simd::vector2(depth->GetSize()) * Device::DynamicResolution::Get().GetFrameToDynamicScale()).round();
		auto pass = Pass(L"Desaturation", device_pass, ShaderPass::Color, pass_uniform_inputs, pass_binding_inputs, display_size, display_size);

		pass.AddSubPass({ DrawCalls::BlendMode::Desaturate, Entity::ViewType::Camera });

		return pass;
	}

	Pass RenderGraph::BuildShimmerPass()
	{
		Device::UniformInputs pass_uniform_inputs;
		Device::BindingInputs pass_binding_inputs;
		pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::PassType)).SetInt(&pass_types[LightingPass]));
		AddResolvedDepthDrawData(pass_uniform_inputs, pass_binding_inputs);

		auto device_pass = FindPass("Shimmer", { linearHelper.get() }, depth.get(), Device::ClearTarget::COLOR, simd::color(0), 1.0f);
		const auto display_size = (simd::vector2(depth->GetSize()) * Device::DynamicResolution::Get().GetFrameToDynamicScale()).round();
		auto pass = Pass(L"Shimmer", device_pass, ShaderPass::Color, pass_uniform_inputs, pass_binding_inputs, display_size, display_size);

		pass.AddSubPass({ DrawCalls::BlendMode::Shimmer, Entity::ViewType::Camera });

		return pass;
	}

	Pass RenderGraph::BuildSMAAPass()
	{
		return Pass(L"SMAA", [=](auto& command_buffer)
		{
			if (smaa.Enabled())
			{
				auto* smaa_src = srgbHelper[0].get();
				auto* smaa_dst = srgbHelper[1].get();
				smaa.Render(device, command_buffer, smaa_src->GetTexture().Get(), simd::vector2(1, 1), depth.get(), linearHelper.get(), smaa_dst);
			}
		});
	}

	Pass RenderGraph::BuildReprojectorPass(Device::RenderTarget* src_target, Device::RenderTarget* dst_target, int downscale, bool use_high_quality)
	{
		return Pass(L"Reprojector", [=](auto& command_buffer)
		{
			simd::vector2 curr_frame_to_dynamic_scale = simd::vector2(reprojection_data.curr_layer->frame_to_dynamic_scale.x, reprojection_data.curr_layer->frame_to_dynamic_scale.y);
			simd::vector2 prev_frame_to_dynamic_scale = simd::vector2(reprojection_data.prev_layer->frame_to_dynamic_scale.x, reprojection_data.prev_layer->frame_to_dynamic_scale.y);

			screenspace_tracer.Reproject(
				device,
				command_buffer,
				reprojection_data.curr_layer->depth_data->GetTexture(),
				reprojection_data.prev_layer->depth_data->GetTexture(),
				src_target->GetTexture(),
				dst_target,
				curr_frame_to_dynamic_scale,
				prev_frame_to_dynamic_scale,
				reprojection_data.curr_layer->proj_matrix,
				reprojection_data.curr_layer->viewproj_matrix,
				reprojection_data.prev_layer->viewproj_matrix,
				use_high_quality,
				src_target->GetSize(), 0, downscale);
		});
	}

	Pass RenderGraph::BuildGlobalIlluminationPass()
	{
		return Pass(L"GlobalIllumination", [=](auto& command_buffer)
		{
			auto frame_to_dynamic_scale = Device::DynamicResolution::Get().GetFrameToDynamicScale();
			size_t mip_level = downsampled_gbuffer_data.downscaling_mip;

			screenspace_tracer.ComputeGlobalIllumination(
				device,
				command_buffer,
				downsampled_gbuffer_data.blurred_direct_light->GetTexture(),
				downsampled_gbuffer_data.depth_pyramid->GetLevelData( static_cast< uint32_t >( mip_level ) ).render_target->GetTexture(),
				downsampled_gbuffer_data.blurred_linear_depth->GetTexture(),
				downsampled_gbuffer_data.normal_pyramid->GetLevelData( static_cast< uint32_t >( mip_level ) ).render_target->GetTexture(),
				reprojection_data.curr_layer->depth_data->GetSize(),
				frame_to_dynamic_scale,
				downsampled_gbuffer_data.downscaling_mip,
				1.0f,
				indirect_light_rampup,
				ambient_occlusion_power,
				screenspace_thickness_angle,
				Scene::System::Get().GetFrameTime(),
				1,
				global_illumination_data.tmp_gi_mip.get());

			screenspace_tracer.ApplyDepthAwareBlur(
				device,
				command_buffer,
				global_illumination_data.tmp_gi_mip->GetTexture(),
				downsampled_gbuffer_data.depth_pyramid->GetLevelData( static_cast< uint32_t >( mip_level)).render_target->GetTexture(),
				downsampled_gbuffer_data.normal_pyramid->GetLevelData( static_cast< uint32_t >( mip_level)).render_target->GetTexture(),
				global_illumination_data.gi_pyramid->GetLevelData( static_cast< uint32_t >( downsampled_gbuffer_data.downscaling_mip )).render_target,
				frame_to_dynamic_scale,
				simd::vector2_int(2, 2),
				reprojection_data.curr_layer->depth_data->GetSize(),
				downsampled_gbuffer_data.downscaling_mip);

			if (downsampled_gbuffer_data.downscaling_mip > 0)
			{
				auto inv_proj_matrix = Scene::System::Get().GetCamera()->GetFinalProjectionMatrix().inverse();

				resampler.DepthAwareUpsample(command_buffer, depth.get(), reprojection_data.curr_layer->normal_data.get(), global_illumination_data.gi_pyramid->GetTargetList(), frame_to_dynamic_scale, inv_proj_matrix, TargetResampler::GBufferUpsamplingBlendMode::Default, downsampled_gbuffer_data.downscaling_mip, 0);
			}
		});
	}

	Pass RenderGraph::BuildScreenspaceShadowsPass()
	{
		return Pass(L"ScreenspaceShadows", [=](auto& command_buffer)
		{
			auto frame_to_dynamic_scale = Device::DynamicResolution::Get().GetFrameToDynamicScale();

			auto depth_tex = depth->GetTexture();
			auto inv_proj_matrix = Scene::System::Get().GetCamera()->GetFinalProjectionMatrix().inverse();

			struct PointLightsGroup
			{
				size_t lights_count;
				Memory::SmallVector<simd::vector4, 16, Memory::Tag::DrawCalls> position_data;
				Memory::SmallVector<simd::vector4, 16, Memory::Tag::DrawCalls> direction_data;
				Memory::SmallVector<simd::vector4, 16, Memory::Tag::DrawCalls> color_data;
			};
			std::vector<PointLightsGroup> channel_data;

			auto GetPointLightChannelIndex = [&](float channel_index)
			{
				if (channel_index > -0.5f && channel_index < 0.5f) return 0;
				if (channel_index >  0.5f && channel_index < 1.5f) return 1;
				if (channel_index >  1.5f && channel_index < 2.5f) return 2;
				if (channel_index >  2.5f && channel_index < 3.5f) return 3;
				return -1;
			};

			const auto total_point_lights = Scene::System::Get().GetVisiblePointLights();
			size_t total_lights_count = total_point_lights.size();
			channel_data.resize(4);
			for (size_t light_index = 0; light_index < total_lights_count; light_index++)
			{
				auto point_light = total_point_lights[light_index];
				const auto info = point_light->BuildPointLightInfo(Scene::System::Get().GetSubSceneTransform(point_light->GetBoundingBox()));

				int channel_index = GetPointLightChannelIndex(info.channel_index);
				if (channel_index >= 0 && channel_index < 4)
				{
					const auto position = simd::vector4(info.position, info.dist_threshold);
					const auto direction = simd::vector4(info.direction, (float)point_light->GetProfile());
					const auto color = simd::vector4(info.color, info.cutoff_radius);

					channel_data[channel_index].position_data.push_back(position);
					channel_data[channel_index].direction_data.push_back(direction);
					channel_data[channel_index].color_data.push_back(color);
					channel_data[channel_index].lights_count++;
				}
			}

			bool use_blur = true;
			float mip_offset = use_blur ? -2.0f : 0.0f;
			simd::vector2_int viewport_size = screenspace_shadows_data.mip_pyramid->GetLevelData(0).render_target->GetSize();
			//size_t finish_mip = 5;
			for (size_t level_number = 0; level_number < screenspace_shadows_data.mip_pyramid->GetTotalLevelsCount(); level_number++)
			{
				size_t level_index = screenspace_shadows_data.mip_pyramid->GetTotalLevelsCount() - 1 - level_number;
				if (level_index > screenspace_shadows_data.finish_level)
					continue;

				const auto &zero_depth = depth;

				bool use_offset_tex = false;// GetAsyncKeyState(VK_SPACE);

				const auto pixel_offset_tex = use_offset_tex ?
					downsampled_gbuffer_data.pixel_offset_pyramid->GetLevelData( static_cast< uint32_t >( level_index )).render_target->GetTexture() :
					Device::Handle<Device::Texture>();
				const auto depth_mip_tex = use_offset_tex ?
					downsampled_gbuffer_data.offset_depth_pyramid->GetLevelData( static_cast< uint32_t >( level_index)).render_target->GetTexture() :
					downsampled_gbuffer_data.depth_pyramid->GetLevelData( static_cast< uint32_t >( level_index)).render_target->GetTexture();

				const auto &shadows_target_list = screenspace_shadows_data.mip_pyramid->GetTargetList();

				const auto &shadow_level_data = screenspace_shadows_data.mip_pyramid->GetLevelData( static_cast< uint32_t >( level_index));
				const auto &tmp_level_data = screenspace_shadows_data.tmp_mip_pyramid->GetLevelData( static_cast< uint32_t >( level_index));

				auto linear_depth_tex = downsampled_gbuffer_data.blurred_linear_depth->GetTexture();

				if (level_index != screenspace_shadows_data.finish_level)
				{
					if (use_offset_tex)
					{
						resampler.DepthAwareUpsample(command_buffer, zero_depth.get(), downsampled_gbuffer_data.pixel_offset_pyramid->GetTargetList(), shadows_target_list, frame_to_dynamic_scale, inv_proj_matrix, TargetResampler::GBufferUpsamplingBlendMode::Default, level_index + 1, level_index);
					}
					else
					{
						resampler.DepthAwareUpsample(command_buffer, zero_depth.get(), shadows_target_list, frame_to_dynamic_scale, inv_proj_matrix, TargetResampler::GBufferUpsamplingBlendMode::Default, level_index + 1, level_index);
					}
				}

				if (level_index >= screenspace_shadows_data.start_level)
				{
					bool clear = level_index == screenspace_shadows_data.finish_level;

					for (int channel_index = 0; channel_index < 4; channel_index++)
					{
						if (channel_data[channel_index].lights_count <= 0)
							continue;

						auto& curr_channel_data = channel_data[channel_index];
						screenspace_tracer.ComputeScreenspaceShadowsChannel(
							device,
							command_buffer,
							depth_mip_tex,
							pixel_offset_tex,
							linear_depth_tex,
							frame_to_dynamic_scale,
							channel_data[channel_index].position_data.data(), channel_data[channel_index].direction_data.data(), channel_data[channel_index].color_data.data(), channel_data[channel_index].lights_count,
							shadow_level_data.render_target,
							viewport_size, level_index, screenspace_shadows_data.start_level, mip_offset,
							channel_index,
							clear);

						clear = false;
					}

					if (level_index > screenspace_shadows_data.start_level) //not blurring mip0
					{
						screenspace_tracer.ApplyDepthAwareBlur(
							device,
							command_buffer,
							shadow_level_data.render_target->GetTexture(),
							depth_mip_tex,
							reprojection_data.curr_layer->normal_data->GetTexture(),
							tmp_level_data.render_target,
							frame_to_dynamic_scale,
							simd::vector2_int(1, 0),
							viewport_size,
							level_index,
							1.0f);

						screenspace_tracer.ApplyDepthAwareBlur(
							device,
							command_buffer,
							tmp_level_data.render_target->GetTexture(),
							depth_mip_tex,
							reprojection_data.curr_layer->normal_data->GetTexture(),
							shadow_level_data.render_target,
							frame_to_dynamic_scale,
							simd::vector2_int(0, 1),
							viewport_size,
							level_index,
							1.0f);
					}
				}
			}
		});
	}

	Pass RenderGraph::BuildBloomPass(float bloom_strength)
	{
		return Pass(L"Bloom", [=](auto& command_buffer)
		{
			Device::RenderTarget* src_target = GetMainTarget();

			const float cutoff = bloom_data.cutoff;
			const float intensity = bloom_data.intensity * bloom_strength;
			const float weight_mult = bloom_data.weight_mult;
			const simd::vector2 resolution_scale = Device::DynamicResolution::Get().GetFrameToDynamicScale();

			std::vector<Device::RenderTarget*> dst_mips;
			std::vector<Device::RenderTarget*> tmp_mips;
			for (size_t mip_level = 0; mip_level < bloom_data.bloom_mips.size(); mip_level++)
			{
				dst_mips.push_back(bloom_data.bloom_mips[mip_level].get());
				tmp_mips.push_back(bloom_data.tmp_bloom_mips[mip_level].get());
			}

			if ((dst_mips.size() > 0) && (tmp_mips.size() > 0))
			{
				resampler.ResampleColor(
					command_buffer,
					src_target,
					dst_mips[0],
					simd::vector2(1.0f, 1.0f),
					TargetResampler::FilterTypes::Avg,
					1.0f);

				screenspace_tracer.ComputeBloomCutoff(
					device,
					command_buffer,
					dst_mips[0],
					tmp_mips[0], cutoff, intensity, resolution_scale);

				for (size_t level = 1; level < dst_mips.size(); level++)
				{
					resampler.ResampleColor(
						command_buffer,
						tmp_mips[level - 1],
						tmp_mips[level],
						simd::vector2(1.0f, 1.0f), //resolution scale is not used for mips
						TargetResampler::FilterTypes::Avg,
						0.0f);
				}
				for (size_t level = 0; level < dst_mips.size(); level++)
				{
					int radius = 4;
					screenspace_tracer.ApplyBlur(
						device,
						command_buffer,
						tmp_mips[level],
						dst_mips[level],
						1.0f,
						0.2f,
						simd::vector2(1.0f, 1.0f), //resolution scale is not used for mips
						simd::vector2_int(radius, 0));
					screenspace_tracer.ApplyBlur(
						device,
						command_buffer,
						dst_mips[level],
						tmp_mips[level],
						1.0f,
						0.2f,
						simd::vector2(1.0f, 1.0f), //resolution scale is not used for mips
						simd::vector2_int(0, radius));

				}

				screenspace_tracer.ComputeBloomGathering(
					device,
					command_buffer,
					tmp_mips[0]->GetTexture(),
					weight_mult,
					simd::vector2(1.0f, 1.0f),
					dst_mips[0]);
			}
		});
	}

	Pass RenderGraph::BuildDoFPass(bool dof_enabled, bool time_manipulation_effects)
	{
		return Pass(L"DoF", [=](auto& command_buffer)
		{
			if (!dof_enabled || !dof_data.active)
				return;

			const simd::vector2 resolution_scale = Device::DynamicResolution::Get().GetFrameToDynamicScale();

			screenspace_tracer.ComputeDoFLayers(
				device,
				command_buffer,
				GetMainTarget()->GetTexture().Get(),
				downsampled_gbuffer_data.depth_pyramid->GetLevelData(0).render_target->GetTexture().Get(),
				dof_data.foreground_tmp.get(),
				dof_data.background_tmp.get(),
				dof_data.focus_range, dof_data.focus_distance, dof_data.background_transition, dof_data.foreground_transition,
				(uint32_t)dof_data.downsample_mip, resolution_scale
			);

			PostProcess::SynthesisMapInfo synthesis_map_info = Scene::System::Get().GetSynthesisMapInfo();

			post_process.RenderPostProcessedImage(device, command_buffer, dof_data.foreground_tmp->GetTexture().Get(), nullptr, nullptr,
				bloom_data.bloom->GetTexture().Get(), synthesis_map_info, reprojection_data.curr_layer->depth_data->GetTexture().Get(), nullptr, dof_data.foreground.get(), true, time_manipulation_effects,
				resolution_scale, Scene::System::Get().GetRitualEncounterSphere(), Scene::System::Get().GetRitualEncounterRatio());

			post_process.RenderPostProcessedImage(device, command_buffer, dof_data.background_tmp->GetTexture().Get(), nullptr, nullptr,
				bloom_data.bloom->GetTexture().Get(), synthesis_map_info, reprojection_data.curr_layer->depth_data->GetTexture().Get(), nullptr, dof_data.background.get(), true, time_manipulation_effects,
				resolution_scale, Scene::System::Get().GetRitualEncounterSphere(), Scene::System::Get().GetRitualEncounterRatio());

			const auto blur_target_size = dof_data.foreground->GetSize();
			const float max_width = 3840 / 4;
			const float max_sigma_background = screenspace_tracer.GetMaxDoFBlurSigma() * dof_data.blur_scale * 0.01f;
			const float max_sigma_foreground = screenspace_tracer.GetMaxDoFBlurSigma() * dof_data.blur_foreground_scale * 0.01f;

			const float sigma_background = std::max(0.4f, max_sigma_background * blur_target_size.x / max_width);
			const float sigma_foreground = std::max(0.4f, max_sigma_foreground * blur_target_size.x / max_width);

			screenspace_tracer.ComputeDoFBlur(
				device,
				command_buffer,
				dof_data.foreground->GetTexture().Get(),
				dof_data.foreground_tmp.get(),
				simd::vector2(sigma_foreground, 0),
				resolution_scale);

			screenspace_tracer.ComputeDoFBlur(
				device,
				command_buffer,
				dof_data.foreground_tmp->GetTexture().Get(),
				dof_data.foreground.get(),
				simd::vector2(0, sigma_foreground),
				resolution_scale);

			screenspace_tracer.ComputeDoFBlur(
				device,
				command_buffer,
				dof_data.background->GetTexture().Get(),
				dof_data.foreground_tmp.get(),
				simd::vector2(sigma_background, 0),
				resolution_scale);

			screenspace_tracer.ComputeDoFBlur(
				device,
				command_buffer,
				dof_data.foreground_tmp->GetTexture().Get(),
				dof_data.background.get(),
				simd::vector2(0, sigma_background),
				resolution_scale);
		});
	}

	Pass RenderGraph::BuildPostProcessPass(bool dof_enabled, bool time_manipulation_effects)
	{
		return Pass(L"PostProcess", [=](auto& command_buffer)
		{
			const auto resolution_scale = Device::DynamicResolution::Get().GetFrameToDynamicScale();

			auto* post_process_src = smaa.Enabled() ? srgbHelper[1].get() : srgbHelper[0].get();
			auto* post_process_dst = smaa.Enabled() ? srgbHelper[0].get() : srgbHelper[1].get();

			const auto local_dof_params = dof_enabled && dof_data.active ? 
				PostProcess::DoFParams(
					dof_data.background->GetTexture().Get(), dof_data.foreground->GetTexture().Get(),
					dof_data.focus_distance, dof_data.focus_range,
					dof_data.background_transition, dof_data.foreground_transition)
				: PostProcess::DoFParams();

			post_process.RenderPostProcessedImage(
				device,
				command_buffer,
				post_process_src->GetTexture().Get(),
				linearHelper->GetTexture().Get(),
				desaturation_target->GetTexture().Get(),
				bloom_data.bloom->GetTexture().Get(),
				Scene::System::Get().GetSynthesisMapInfo(),
				reprojection_data.curr_layer->depth_data->GetTexture().Get(),
				dof_enabled && dof_data.active ? &local_dof_params : nullptr,
				post_process_dst,
				false,
				time_manipulation_effects,
				Device::DynamicResolution::Get().GetFrameToDynamicScale(),
				Scene::System::Get().GetRitualEncounterSphere(),
				Scene::System::Get().GetRitualEncounterRatio());

			if (post_process_dst->GetTexture().Get())
			{
				post_process.UpscalePostProcessedImage(device, command_buffer, post_process_dst->GetTexture().Get(), GetBackBuffer(), resolution_scale);
			}
		});
	}

	Pass RenderGraph::BuildWaitPass()
	{
		return Pass(L"Wait", [=](auto& command_buffer)
		{
			device->WaitForBackbuffer(command_buffer);
		});
	}

	Pass RenderGraph::BuildUIRenderToTexturePass()
	{
		return Pass(L"UIRenderToTexture", [=](auto& command_buffer)
		{
			device->SetCurrentUICommandBuffer(&command_buffer); // TODO: Remove.

			if (callback_render_to_texture)
				callback_render_to_texture();

			device->SetCurrentUICommandBuffer(nullptr); // TODO: Remove.
		});
	}

	Pass RenderGraph::BuildUIPass()
	{
		auto* renderTarget = GetBackBuffer();
		auto device_pass = FindPass("UI", { renderTarget }, nullptr, Device::ClearTarget::NONE, simd::color(0), 0.0f);
		auto pass = Pass(L"UI", device_pass, ShaderPass::Color, {}, {}, renderTarget->GetSize(), renderTarget->GetSize());

		pass.AddSubPass({ L"UIRender", [=] (auto& command_buffer)
		{
			device->SetCurrentUICommandBuffer(&command_buffer); // TODO: Remove.
			device->SetCurrentUIPass(device_pass.Get()); // TODO: Remove.

			if (callback_render)
				callback_render(device_pass.Get());

			device->SetCurrentUIPass(nullptr); // TODO: Remove.
			device->SetCurrentUICommandBuffer(nullptr); // TODO: Remove.
		} });

		return pass;
	}

	Pass RenderGraph::BuildDebugUIPass()
	{
		return Pass(L"DebugUI", [=](auto& command_buffer)
		{
		#if DEBUG_GUI_ENABLED
			Device::GUI::GUIResourceManager::GetGUIResourceManager()->Flush(&command_buffer);
		#endif
		});
	}

	template <typename... ARGS>
	MetaPass& RenderGraph::AddMetaPass(ARGS... args)
	{
		meta_passes.emplace_back(std::make_unique<MetaPass>((unsigned)meta_passes.size(), args...));
		return *meta_passes.back();
	}

	void RenderGraph::KickMetaPass(const std::function<void(MetaPass&)>& record, MetaPass& meta_pass)
	{
		record(meta_pass);
	}

	void RenderGraph::AddComputeMetaPass(const std::function<void(MetaPass&)>& record, std::shared_ptr<CubemapTask> cubemap_task)
	{
		KickMetaPass(record, AddMetaPass(L"Compute", Job2::Profile::PassCompute, Job2::Profile::RecordOther)
			.AddPass(BuildComputePass(cubemap_task))
			.AddPass(BuildComputePostPass()));
	}

	void RenderGraph::AddShadowsMetaPasses(const std::function<void(MetaPass&)>& record, bool shadows_enabled)
	{
		if (shadows_enabled)
		{
			Scene::Lights lights, shadow_lights;
			std::tie(lights, shadow_lights) = Scene::System::Get().GetVisibleLightsNoPoint();

			for (unsigned int i = 0; i < shadow_lights.size(); ++i)
			{
				KickMetaPass(record, AddMetaPass(Memory::DebugStringW<>(L"Shadow ") + std::to_wstring(i), Job2::Profile::PassShadow, Job2::Profile::RecordShadow)
					.AddPass(BuildShadowPass(shadow_lights[i], i)));
			}
		}
	}

	void RenderGraph::AddCubemapMetaPasses(const std::function<void(MetaPass&)>& record, std::shared_ptr<CubemapTask> cubemap_task)
	{
		if (auto* cubemap_manager = Scene::System::Get().GetCubemapManager())
		{
			if (cubemap_task)
			{
				auto cubemap_target = cubemap_manager->GetRenderTarget();
				cubemap_task->result = cubemap_target->cubemap_texture;

				KickMetaPass(record, AddMetaPass(Memory::DebugStringW<>(L"Cubemap"), Job2::Profile::PassMainOpaque, Job2::Profile::RecordOther)
					.AddPass(BuildCubemapPass(cubemap_task, cubemap_target->render_targets[0].get(), cubemap_target->depth.get(), 0))
					.AddPass(BuildCubemapPass(cubemap_task, cubemap_target->render_targets[1].get(), cubemap_target->depth.get(), 1))
					.AddPass(BuildCubemapPass(cubemap_task, cubemap_target->render_targets[2].get(), cubemap_target->depth.get(), 2))
					.AddPass(BuildCubemapPass(cubemap_task, cubemap_target->render_targets[3].get(), cubemap_target->depth.get(), 3))
					.AddPass(BuildCubemapPass(cubemap_task, cubemap_target->render_targets[4].get(), cubemap_target->depth.get(), 4))
					.AddPass(BuildCubemapPass(cubemap_task, cubemap_target->render_targets[5].get(), cubemap_target->depth.get(), 5)));
			}
		}
	}

	void RenderGraph::AddZPrePassMetaPass(const std::function<void(MetaPass&)>& record)
	{
		size_t target_mip = downsampled_pass_data.downsample_mip;
		auto* src_depth_target = downsampled_gbuffer_data.offset_depth_pyramid ?
			downsampled_gbuffer_data.offset_depth_pyramid->GetLevelData( static_cast< uint32_t >( target_mip)).render_target :
			downsampled_gbuffer_data.depth_pyramid->GetLevelData( static_cast< uint32_t >( target_mip)).render_target;
		auto* dst_low_res_depth = downsampled_pass_data.depth_stencil.get();

		std::vector<Device::RenderTarget*> build_linear_depth_mips;
		for (size_t mip_level = 0; mip_level < downsampled_gbuffer_data.blurred_linear_depth_mips.size(); mip_level++) //starts from 0
		{
			build_linear_depth_mips.push_back(downsampled_gbuffer_data.blurred_linear_depth_mips[mip_level].get());
		}

		std::vector<Device::RenderTarget*> blur_linear_depth_mips;
		std::vector<Device::RenderTarget*> blur_tmp_depth_mips;
		for (size_t mip_level = 1; mip_level < downsampled_gbuffer_data.blurred_linear_depth_mips.size(); mip_level++) // starts from 1
		{
			blur_linear_depth_mips.push_back(downsampled_gbuffer_data.blurred_linear_depth_mips[mip_level].get());
			blur_tmp_depth_mips.push_back(downsampled_gbuffer_data.tmp_linear_depth_mips[mip_level].get());
		}

		auto& meta_pass = AddMetaPass(L"ZPrePass", Job2::Profile::PassZPrepass, Job2::Profile::RecordZPrepass)
			.AddPass(BuildZPrePass())
			.AddPass(BuildColorResamplerPass(depth.get(), reprojection_data.curr_layer->depth_data.get()))
			.AddPass(BuildDepthAwareDownsamplerPass())
			.AddPass(BuildDepthResamplerPass(src_depth_target, dst_low_res_depth));

		if (downsampled_gbuffer_data.blurred_linear_depth_mips.size() > 0)
			meta_pass.AddPass(BuildLinearizeDepthPass(depth.get(), downsampled_gbuffer_data.blurred_linear_depth_mips[0].get(), 1 << downsampled_gbuffer_data.downscaling_mip));

		if (build_linear_depth_mips.size() > 0)
			meta_pass.AddPass(BuildMipBuilderPass(build_linear_depth_mips, TargetResampler::FilterTypes::Avg, 1.0f));

		if (blur_linear_depth_mips.size() > 0)
			meta_pass.AddPass(BuildBlurPass(blur_linear_depth_mips, blur_tmp_depth_mips, 1.0f));

		KickMetaPass(record, meta_pass);
	}

	void RenderGraph::AddOpaqueMetaPass(const std::function<void(MetaPass&)>& record, simd::color clear_color, bool ss_high)
	{
		KickMetaPass(record, AddMetaPass(L"Opaque", Job2::Profile::PassMainOpaque, Job2::Profile::RecordOpaque)
			.AddPass(BuildWetnessPass(ss_high))
			.AddPass(BuildColorPass({ 
				DrawCalls::BlendMode::Opaque,
				DrawCalls::BlendMode::DepthOverride,
				DrawCalls::BlendMode::AlphaTestWithShadow,
				DrawCalls::BlendMode::OpaqueNoShadow,
				DrawCalls::BlendMode::AlphaTest,
				DrawCalls::BlendMode::DepthOnlyAlphaTest,
				DrawCalls::BlendMode::NoZWriteAlphaBlend,
				DrawCalls::BlendMode::BackgroundMultiplicitiveBlend }, ss_high, true, clear_color)));
	}

	void RenderGraph::AddWaterMetaPass(const std::function<void(MetaPass&)>& record, bool water_downscaled, bool ss_high)
	{
		if (water_downscaled)
		{
			KickMetaPass(record, AddMetaPass(L"Water Downscaled", Job2::Profile::PassWater, Job2::Profile::RecordOther)
				.AddPass(BuildColorResamplerPass(GetMainTarget(), downscaled_water_data.downsampled_color.get()))
				.AddPass(BuildDepthResamplerPass(depth.get(), downscaled_water_data.downsampled_depth.get()))
				.AddPass(BuildDownscaledWaterPass(DrawCalls::BlendMode::BackgroundGBufferSurfaces, ss_high))
				.AddPass(BuildColorResamplerPass(downscaled_water_data.target_color.get(), GetMainTarget()))
				.AddPass(BuildColorResamplerPass(GetMainTarget(), srgbHelper[1].get()))
				.AddPass(BuildMainWaterPass(DrawCalls::BlendMode::GBufferSurfaces, ss_high)));
		}
		else
		{
			KickMetaPass(record, AddMetaPass(L"Water", Job2::Profile::PassWater, Job2::Profile::RecordOther)
				.AddPass(BuildColorResamplerPass(GetMainTarget(), srgbHelper[1].get()))
				.AddPass(BuildMainWaterPass(DrawCalls::BlendMode::BackgroundGBufferSurfaces, ss_high))
				.AddPass(BuildColorResamplerPass(GetMainTarget(), srgbHelper[1].get()))
				.AddPass(BuildMainWaterPass(DrawCalls::BlendMode::GBufferSurfaces, ss_high)));
		}
	}

	void RenderGraph::AddAlphaMetaPasses(const std::function<void(MetaPass&)>& record, bool gi_enabled, bool ss_high)
	{
		if (gi_enabled)
		{
			KickMetaPass(record, AddMetaPass(L"AlphaBlend", Job2::Profile::PassMainTransparent, Job2::Profile::RecordAlpha)
				.AddPass(BuildColorPass({ 
					DrawCalls::BlendMode::AlphaBlend }, ss_high))
				.AddPass(BuildDownscaledPass(ss_high))
				.AddPass(BuildUpsampleGBufferPass())
				.AddPass(BuildColorPass({
					DrawCalls::BlendMode::PremultipliedAlphaBlend,
					DrawCalls::BlendMode::ForegroundAlphaBlend }, ss_high)));

			KickMetaPass(record, AddMetaPass(L"ColorResampler GI Normals", Job2::Profile::PassColorResampler, Job2::Profile::RecordOther)
				.AddPass(BuildColorResamplerPass(gbuffer_normals.get(), reprojection_data.curr_layer->normal_data.get())));

			KickMetaPass(record, AddMetaPass(L"Additive Alpha", Job2::Profile::PassMainTransparent, Job2::Profile::RecordAlpha)
				.AddPass(BuildColorPass({
					DrawCalls::BlendMode::MultiplicitiveBlend,
					DrawCalls::BlendMode::Addablend,
					DrawCalls::BlendMode::Additive,
					DrawCalls::BlendMode::Subtractive }, ss_high)));

			KickMetaPass(record, AddMetaPass(L"ColorResampler GI DirectLight", Job2::Profile::PassColorResampler, Job2::Profile::RecordOther)
				.AddPass(BuildColorResamplerPass(gbuffer_direct_light.get(), reprojection_data.curr_layer->direct_light_data.get())));

			KickMetaPass(record, AddMetaPass(L"Misc Alpha", Job2::Profile::PassMainTransparent, Job2::Profile::RecordAlpha)
				.AddPass(BuildColorPass({ // Separate pass because it's not using GI.
					DrawCalls::BlendMode::AlphaTestNoGI,
					DrawCalls::BlendMode::AlphaBlendNoGI }, ss_high))
				.AddPass(BuildColorPass({
					DrawCalls::BlendMode::Outline,
					DrawCalls::BlendMode::OutlineZPrepass,
					DrawCalls::BlendMode::RoofFadeBlend }, ss_high)));
		}
		else
		{
			KickMetaPass(record, AddMetaPass(L"Alpha", Job2::Profile::PassMainTransparent, Job2::Profile::RecordAlpha)
				.AddPass(BuildColorPass({ 
					DrawCalls::BlendMode::AlphaBlend }, ss_high))
				.AddPass(BuildDownscaledPass(ss_high))
				.AddPass(BuildUpsampleGBufferPass())
				.AddPass(BuildColorPass({
					DrawCalls::BlendMode::PremultipliedAlphaBlend,
					DrawCalls::BlendMode::ForegroundAlphaBlend,
					DrawCalls::BlendMode::MultiplicitiveBlend,
					DrawCalls::BlendMode::Addablend,
					DrawCalls::BlendMode::Additive,
					DrawCalls::BlendMode::Subtractive,
					DrawCalls::BlendMode::AlphaTestNoGI,
					DrawCalls::BlendMode::AlphaBlendNoGI,
					DrawCalls::BlendMode::Outline,
					DrawCalls::BlendMode::OutlineZPrepass,
					DrawCalls::BlendMode::RoofFadeBlend }, ss_high)));
		}
	}

	void RenderGraph::AddGIMetaPass(const std::function<void(MetaPass&)>& record, bool gi_enabled, bool gi_high)
	{
		if (gi_enabled)
		{
			std::vector<Device::RenderTarget*> color_mips;
			for (auto& mip : downsampled_gbuffer_data.blurred_direct_light_mips)
				color_mips.push_back(mip.get());

			std::vector<Device::RenderTarget*> tmp_color_mips;
			for (auto& mip : downsampled_gbuffer_data.tmp_direct_light_mips)
				tmp_color_mips.push_back(mip.get());

			auto& meta_pass = AddMetaPass(L"GI", Job2::Profile::PassGlobalIllumination, Job2::Profile::RecordOther)
				.AddPass(BuildReprojectorPass(reprojection_data.prev_layer->direct_light_data.get(), reprojection_data.curr_layer->direct_light_data.get(), 1, gi_high))
				.AddPass(BuildReprojectorPass(reprojection_data.prev_layer->normal_data.get(), reprojection_data.curr_layer->normal_data.get(), 1, gi_high))
				.AddPass(BuildColorResamplerPass(reprojection_data.curr_layer->normal_data.get(), downsampled_gbuffer_data.normal_pyramid->GetTargetList()[0]))
				.AddPass(BuildDownsampledPass(reprojection_data.curr_layer->normal_data.get(), downsampled_gbuffer_data.normal_pyramid->GetTargetList()));

			if (color_mips.size() > 0)
			{
				meta_pass
					.AddPass(BuildColorResamplerPass(reprojection_data.curr_layer->direct_light_data.get(), tmp_color_mips[0]))
					.AddPass(BuildDirectLightPostProcessPass(tmp_color_mips[0], color_mips[0], 1))
					.AddPass(BuildMipBuilderPass(color_mips, TargetResampler::FilterTypes::Mixed, indirect_light_area))
					.AddPass(BuildBlurPass(color_mips, tmp_color_mips, indirect_light_area));
			}

			meta_pass.AddPass(BuildGlobalIlluminationPass());
			KickMetaPass(record, meta_pass);
		}
	}

	void RenderGraph::AddScreenSpaceShadowsMetaPass(const std::function<void(MetaPass&)>& record)
	{
		KickMetaPass(record, AddMetaPass(L"ScreenspaceShadows", Job2::Profile::PassScreenspaceShadows, Job2::Profile::RecordOther)
			.AddPass(BuildScreenspaceShadowsPass()));
	}

	void RenderGraph::AddPostProcessMetaPass(const std::function<void(MetaPass&)>& record, bool dof_enabled, float bloom_strength, bool time_manipulation_effects)
	{
		KickMetaPass(record, AddMetaPass(L"Post Process", Job2::Profile::PassPostProcess, Job2::Profile::RecordPostProcess)
			.AddPass(BuildBloomPass(bloom_strength))
			.AddPass(BuildDoFPass(dof_enabled, time_manipulation_effects))
			.AddPass(BuildSMAAPass())
			.AddPass(BuildDesaturationPass())
			.AddPass(BuildShimmerPass())
			.AddPass(BuildWaitPass())
			.AddPass(BuildPostProcessPass(dof_enabled, time_manipulation_effects))
			.AddPass(BuildUIRenderToTexturePass())
			.AddPass(BuildUIPass())
			.AddPass(BuildDebugUIPass()));
	}

	Device::UniformInputs RenderGraph::BuildUniformInputs()
	{
		auto pass_uniform_inputs = Scene::System::Get().BuildUniformInputs();

		pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::FrameResolution)).SetVector(&frame_resolution));
		pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::FrameToDynamicScale)).SetVector(&frame_to_dynamic_scale));
		pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::PassDownscale)).SetInt(&ConstData::int_one));

		pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::RainFallDir)).SetVector(&rain.fall_dir));
		pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::RainDist)).SetFloat(&rain.dist));
		pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::RainAmount)).SetFloat(&rain.amount));
		pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::RainIntensity)).SetFloat(&rain.intensity));
		pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::RainTurbulence)).SetFloat(&rain.turbulence));

		pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::CloudsIntensity)).SetFloat(&clouds.intensity));
		pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::CloudsMidpoint)).SetFloat(&clouds.midpoint));
		pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::CloudsScale)).SetFloat(&clouds.scale));
		pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::CloudsSharpness)).SetFloat(&clouds.sharpness));
		pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::CloudsVelocity)).SetVector(&clouds.velocity));
		pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::CloudsFadeRadius)).SetFloat(&clouds.fade_radius));
		pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::CloudsPreFade)).SetFloat(&clouds.pre_fade));
		pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::CloudsPostFade)).SetFloat(&clouds.post_fade));

		pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::WaterColorOpen)).SetVector(&water.color_open));
		pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::WaterColorTerrain)).SetVector(&water.color_terrain));
		pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::WaterWindDirection)).SetVector(&water.wind_direction));
		pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::WaterSubsurfaceScattering)).SetFloat(&water.subsurface_scattering));
		pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::WaterCausticsMult)).SetFloat(&water.caustics_mult));
		pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::WaterRefractionIndex)).SetFloat(&water.refraction_index));
		pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::WaterDispersion)).SetFloat(&water.dispersion));
		pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::WaterClarity)).SetFloat(&water.clarity));
		pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::WaterReflectiveness)).SetFloat(&water.reflectiveness));
		pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::WaterWindIntensity)).SetFloat(&water.wind_intensity));
		pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::WaterWindSpeed)).SetFloat(&water.wind_speed));
		pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::WaterDirectionness)).SetFloat(&water.directionness));
		pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::WaterSwellIntensity)).SetFloat(&water.swell_intensity));
		pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::WaterSwellHeight)).SetFloat(&water.swell_height));
		pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::WaterSwellAngle)).SetFloat(&water.swell_angle));
		pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::WaterSwellPeriod)).SetFloat(&water.swell_period));
		pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::WaterFlowIntensity)).SetFloat(&water.flow_intensity));
		pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::WaterFlowFoam)).SetFloat(&water.flow_foam));

		pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::WaterHeightOffset)).SetFloat(&water.height_offset));

		pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::DustColor)).SetVector(&dust_color));
		pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::GroundScaleMoveUV)).SetVector(&ground_scalemove_uv));

		return pass_uniform_inputs;
	}

	Device::BindingInputs RenderGraph::BuildBindingInputs()
	{
		auto pass_binding_inputs =Scene::System::Get().BuildBindingInputs();
		pass_binding_inputs.push_back(Device::BindingInput(Device::IdHash(DrawDataNames::EnvironmentGGXLookup, 0)).SetTextureResource(environment_ggx_tex));
		return pass_binding_inputs;
	}

	std::pair<Device::UniformInputs, Device::BindingInputs> RenderGraph::BuildGlobalInputs()
	{
		auto global_pass_uniform_inputs = BuildUniformInputs();
		auto global_pass_binding_inputs = BuildBindingInputs();
		AddNullDepthDrawData(global_pass_uniform_inputs, global_pass_binding_inputs);
		AddNullGlobalIlluminationDrawData(global_pass_uniform_inputs, global_pass_binding_inputs);
		AddNullWaterDrawData(global_pass_uniform_inputs, global_pass_binding_inputs);
		return { global_pass_uniform_inputs, global_pass_binding_inputs };
	}

	void RenderGraph::AddNullDepthDrawData(Device::UniformInputs& pass_uniform_inputs, Device::BindingInputs& pass_binding_inputs)
	{
		screenspace_tracer.AddDepthProjectionDrawData(pass_uniform_inputs, pass_binding_inputs, Device::Handle <Device::Texture>());
	}

	void RenderGraph::AddNullWaterDrawData(Device::UniformInputs& pass_uniform_inputs, Device::BindingInputs& pass_binding_inputs)
	{
		simd::vector4 light_color, light_dir;
		GetAmbientLightColor(light_color, light_dir);

		screenspace_tracer.AddWaterDrawData(
			pass_uniform_inputs,
			pass_binding_inputs,
			downscaled_water_data.downsampled_color->GetTexture(),
			light_color,
			light_dir,
			Scene::System::Get().GetFrameTime(),
			false);

		screenspace_tracer.AddShorelineDrawData(pass_uniform_inputs, pass_binding_inputs);
	}

	void RenderGraph::AddNullGlobalIlluminationDrawData(Device::UniformInputs& pass_uniform_inputs, Device::BindingInputs& pass_binding_inputs)
	{
		screenspace_tracer.AddScreenspaceDrawData(
			pass_uniform_inputs,
			pass_binding_inputs,
			Device::Handle<Device::Texture>(),
			Device::Handle<Device::Texture>(),
			Device::Handle<Device::Texture>(),
			0);
	}

	void RenderGraph::AddResolvedDepthDrawData(Device::UniformInputs& pass_uniform_inputs, Device::BindingInputs& pass_binding_inputs, uint32_t downsample_mip)
	{
		auto depth_draw_data = reprojection_data.curr_layer->depth_data->GetTexture();
		if (downsample_mip > 0)
		{
			auto &target_level_data = downsampled_gbuffer_data.offset_depth_pyramid ? downsampled_gbuffer_data.offset_depth_pyramid->GetLevelData(downsample_mip) : downsampled_gbuffer_data.depth_pyramid->GetLevelData(downsample_mip);
			depth_draw_data = target_level_data.render_target->GetTexture();
		}
		
		screenspace_tracer.AddDepthProjectionDrawData(pass_uniform_inputs, pass_binding_inputs, depth_draw_data);
	}

	void RenderGraph::AddScreenspaceDrawData(Device::UniformInputs& pass_uniform_inputs, Device::BindingInputs& pass_binding_inputs, bool ss_high)
	{
		pass_binding_inputs.push_back(Device::BindingInput(Device::IdHash(DrawDataNames::WetnessTex, 0)).SetTexture(downsampled_pass_data.wetness->GetTexture()));

		screenspace_tracer.AddScreenspaceDrawData(
			pass_uniform_inputs,
			pass_binding_inputs,
			global_illumination_data.gi_pyramid ? global_illumination_data.gi_pyramid->GetLevelData(0).render_target->GetTexture() : Device::Handle<Device::Texture>(),
			screenspace_shadows_data.mip_pyramid->GetLevelData(0).render_target->GetTexture(),
			rays_data.ray_hit_mip_pyramid ? rays_data.ray_hit_mip_pyramid->GetLevelData(0).render_target->GetTexture() : Device::Handle <Device::Texture>(),
			ss_high ? 1 : 0);
	}

	void RenderGraph::AddWaterDrawData(Device::UniformInputs& pass_uniform_inputs, Device::BindingInputs& pass_binding_inputs, bool use_downsampled_data)
	{
		simd::vector4 light_color, light_dir;
		GetAmbientLightColor(light_color, light_dir);

		screenspace_tracer.AddShorelineDrawData(pass_uniform_inputs, pass_binding_inputs);

		if (!use_downsampled_data)
		{
			screenspace_tracer.AddDepthProjectionDrawData(
				pass_uniform_inputs,
				pass_binding_inputs,
				reprojection_data.curr_layer->depth_data->GetTexture());

			screenspace_tracer.AddWaterDrawData(
				pass_uniform_inputs,
				pass_binding_inputs,
				srgbHelper[1]->GetTexture(),
				light_color,
				light_dir,
				Scene::System::Get().GetFrameTime(),
				false);
		}
		else
		{
			screenspace_tracer.AddDepthProjectionDrawData(
				pass_uniform_inputs,
				pass_binding_inputs,
				downscaled_water_data.downsampled_depth->GetTexture());

			screenspace_tracer.AddWaterDrawData(
				pass_uniform_inputs,
				pass_binding_inputs,
				downscaled_water_data.downsampled_color->GetTexture(),
				light_color,
				light_dir,
				Scene::System::Get().GetFrameTime(),
				true);
		}
	}

	void RenderGraph::GetAmbientLightColor(simd::vector4& out_light_color, simd::vector4& out_light_dir)
	{
		auto ambient_light = Scene::System::Get().GetAmbientLight();
		out_light_color = simd::vector4(0.7f, 0.7f, 0.7f, 1.0f);
		out_light_dir = simd::vector4(0.0f, 0.0f, 1.0f, 1.0f);
		if (ambient_light)
		{
			out_light_color = ambient_light->GetColor();
			out_light_dir = ambient_light->GetDirection();
		}
	}

	Device::RenderTarget* RenderGraph::GetMainTarget()
	{
		return srgbHelper[0].get();
	}

	Device::ClearTarget RenderGraph::GetMainClearFlags(bool clear) const
	{
	#if defined(MOBILE) // Dirty workaround for mobile z-fighting: clear depth and stencil. // TODO: Remove after render graph refactoring.
		//
		// The depth artefacts go away when clearing the depth buffer between the z-prepass and main passes (this has a small GPU cost, so it's not amazing). 
		// My guess is that there is a synchronization issue between the passes. This will most likely be fixed with the upcoming render graph refactoring.
		//
		return clear ? (Device::ClearTarget)((unsigned)Device::ClearTarget::COLOR | (unsigned)Device::ClearTarget::DEPTH | (unsigned)Device::ClearTarget::STENCIL) : Device::ClearTarget::NONE;
	#else
		return clear ? Device::ClearTarget::COLOR : Device::ClearTarget::NONE;
	#endif
	}

	void RenderGraph::SetPostProcessCamera(Scene::Camera* camera)
	{
		post_process.SetCamera(camera);
		screenspace_tracer.SetCamera(camera);
	}

	void RenderGraph::SetSceneData(simd::vector2 scene_size, simd::vector2_int scene_tiles_count, const Scene::OceanData& ocean_data, const Scene::RiverData& river_data, const Scene::FogBlockingData& fog_blocking_data)
	{
		screenspace_tracer.LoadSceneData(device, scene_size, scene_tiles_count, ocean_data, river_data, fog_blocking_data);
	}

	simd::vector4 RenderGraph::GetSceneData(simd::vector2 planar_pos)
	{
		return screenspace_tracer.GetSceneData(planar_pos);
	}

	void RenderGraph::RenderDebugInfo(const simd::matrix & view_projection)
	{
		screenspace_tracer.RenderWireframe(view_projection);
	}

	Device::RenderTarget* RenderGraph::GetBackBuffer() const
	{
		auto* main = device ? device->GetMainSwapChain() : nullptr;
		return main ? main->GetRenderTarget() : nullptr;
	}

	TargetResampler& RenderGraph::GetResampler()
	{
		return resampler;
	}

	PostProcess& RenderGraph::GetPostProcess()
	{
		return post_process;
	}

}
