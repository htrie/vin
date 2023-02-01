#pragma once

#include "Visual/Device/State.h"
#include "Visual/Entity/Component.h"
#include "Visual/Renderer/DownsampledRenderTargets.h"
#include "Visual/Renderer/SMAA.h"
#include "Visual/Renderer/PostProcess.h"
#include "Visual/Renderer/ScreenspaceTracer.h"
#include "Visual/Renderer/TargetResampler.h"


namespace Job2
{
	enum class Profile;
}

namespace Environment
{
	struct Data;
}

namespace Renderer
{
	struct CubemapTask;

	static inline bool UseGI(DrawCalls::BlendMode blend_mode)
	{
		switch (blend_mode)
		{
			case DrawCalls::BlendMode::AlphaTestNoGI:
			case DrawCalls::BlendMode::AlphaBlendNoGI:
				return false;
			default:
				return true;
		}
	}

	enum PassType
	{
		ShadowPass = 0,
		ComputePass,
		ZPrepass,
		LightingPass,
		WaterPass,
		NumPasses
	};


	#pragma pack(push)
	#pragma pack(1)
		struct PassKey
		{
			Device::ColorRenderTargets render_targets;
			Device::PointerID<Device::RenderTarget> depth_stencil;
			Device::ClearTarget clear_target = Device::ClearTarget::NONE;
			simd::color clear_color = 0;
			float clear_z = 0.0f;

			PassKey() {}
			PassKey(Device::ColorRenderTargets render_targets, Device::RenderTarget* depth_stencil, Device::ClearTarget clear_target, simd::color clear_color, float clear_z)
				: render_targets(render_targets), depth_stencil(depth_stencil), clear_target(clear_target), clear_color(clear_color), clear_z(clear_z)
			{}
		};
	#pragma pack(pop)


	struct SubPass
	{
		typedef std::function<void(Device::CommandBuffer&)> Callback;
		Callback callback; // For non blend-mode sub-passes. // [TODO] Remove once we use SubPass for everything.

		Memory::DebugStringW<> debug_name;
		DrawCalls::BlendMode blend_mode = DrawCalls::BlendMode::Opaque;
		Entity::ViewType view_type = Entity::ViewType::Camera;
		bool inverted_culling = false;
		float depth_bias = 0.0f;
		float slope_scale = 0.0f;

		SubPass(
			const Memory::DebugStringW<>& debug_name,
			const Callback& callback);
		SubPass(
			DrawCalls::BlendMode blend_mode,
			Entity::ViewType view_type,
			bool inverted_culling = false,
			float depth_bias = 0.0f,
			float slope_scale = 0.0f);
	};


	struct Pass
	{
		typedef std::function<void(Device::CommandBuffer&)> Callback;
		Callback callback; // For non sub-passes passes. // [TODO] Remove once we use Pass for everything.

		Memory::Vector<SubPass, Memory::Tag::Render> sub_passes;
		Device::Handle<Device::Pass> pass;
		Memory::DebugStringW<> debug_name;
		ShaderPass shader_pass;
		Device::UniformInputs uniform_inputs;
		Device::BindingInputs binding_inputs;
		Device::Rect viewport_rect;
		Device::Rect scissor_rect; 
		Device::ClearTarget clear_flags = Device::ClearTarget::NONE;
		simd::color clear_color = 0;
		float clear_z = 0.0f;
		bool z_prepass = false;
		bool compute_pass = false;

		Pass() {}
		Pass(
			const Memory::DebugStringW<>& debug_name,
			const Callback& callback);
		Pass(
			const Memory::DebugStringW<>& debug_name,
			Device::Handle<Device::Pass>& pass,
			ShaderPass shader_pass,
			const Device::UniformInputs& uniform_inputs,
			const Device::BindingInputs& binding_inputs,
			Device::Rect viewport_rect = Device::Rect(),
			Device::Rect scissor_rect = Device::Rect(),
			Device::ClearTarget clear_flags = Device::ClearTarget::NONE,
			simd::color clear_color = 0,
			float clear_z = 0.0f,
			bool z_prepass = false);

		void AddSubPass(const SubPass&& sub_pass);
	};


	struct MetaPass
	{
		Memory::Vector<Pass, Memory::Tag::Render> passes;
		Device::Handle<Device::CommandBuffer> command_buffer;
		Memory::DebugStringW<> debug_name;
		Job2::Profile gpu_hash;
		Job2::Profile cpu_hash;
		unsigned bucket_count = 0;
		unsigned bucket_index = 0;

		MetaPass() {}
		MetaPass(
			unsigned int index,
			const Memory::DebugStringW<>& debug_name,
			Job2::Profile gpu_hash,
			Job2::Profile cpu_hash,
			unsigned bucket_count = 1,
			unsigned bucket_index = 0);

		MetaPass& AddPass(const Pass&& pass);
	};


	class RenderGraph
	{
		Device::IDevice* device = nullptr;

		ScreenspaceTracer screenspace_tracer;
		TargetResampler resampler;
		PostProcess post_process;
		SMAA smaa;

		std::function<void()> callback_render_to_texture;
		std::function<void(Device::Pass*)> callback_render;

		typedef Memory::SmallVector<std::unique_ptr<MetaPass>, 64, Memory::Tag::DrawCalls> MetaPasses;
		MetaPasses meta_passes;

		Device::Cache<PassKey, Device::Pass> device_passes;
		Memory::Mutex device_passes_mutex;

		simd::vector4 frame_resolution = 0.0f;
		simd::vector4 frame_to_dynamic_scale = 0.0f;

		Texture::Handle environment_ggx_tex;

		std::array<int, PassType::NumPasses> pass_types;

		std::shared_ptr<Device::RenderTarget> depth;
		std::shared_ptr<Device::RenderTarget> srgbHelper[2];
		std::shared_ptr<Device::RenderTarget> linearHelper;
		std::shared_ptr<Device::RenderTarget> gbuffer_normals;
		std::shared_ptr<Device::RenderTarget> gbuffer_direct_light;
		std::shared_ptr<Device::RenderTarget> desaturation_target;

		struct GlobalIlluminationData
		{
			std::unique_ptr<DownsampledPyramid> gi_pyramid;
			std::unique_ptr<Device::RenderTarget> tmp_gi_mip;

			size_t start_level = 0;
		} global_illumination_data;

		struct ScreenspaceShadowsData
		{
			std::unique_ptr<DownsampledPyramid> mip_pyramid;
			std::unique_ptr<DownsampledPyramid> tmp_mip_pyramid;
			size_t start_level = 0;
			size_t finish_level = 0;
		} screenspace_shadows_data;

		struct RaysData
		{
			std::unique_ptr<Device::RenderTarget> ray_dir_target;
			std::unique_ptr<DownsampledPyramid> ray_hit_mip_pyramid;
			size_t start_level = 0;
			size_t finish_level = 0;
		} rays_data; 

		struct DownsampledGBufferData
		{
			std::unique_ptr<DownsampledPyramid> pixel_offset_pyramid;
			std::unique_ptr<DownsampledPyramid> depth_pyramid;
			std::unique_ptr<DownsampledPyramid> offset_depth_pyramid;
			std::unique_ptr<DownsampledPyramid> normal_pyramid;

			std::unique_ptr<Device::RenderTarget> blurred_linear_depth;
			std::vector<std::unique_ptr<Device::RenderTarget> > blurred_linear_depth_mips;

			std::unique_ptr<Device::RenderTarget> tmp_linear_depth;
			std::vector<std::unique_ptr<Device::RenderTarget> > tmp_linear_depth_mips;

			std::unique_ptr<Device::RenderTarget> blurred_direct_light;
			std::vector<std::unique_ptr<Device::RenderTarget> > blurred_direct_light_mips;

			std::unique_ptr<Device::RenderTarget> tmp_direct_light;
			std::vector<std::unique_ptr<Device::RenderTarget> > tmp_direct_light_mips;

			size_t downscaling_mip = 0;
		} downsampled_gbuffer_data;

		struct DownsampledPassData
		{
			std::unique_ptr<Device::RenderTarget> depth_stencil;
			std::unique_ptr<DownsampledPyramid> color_pyramid;

			std::unique_ptr<Device::RenderTarget> wetness;

			size_t downsample_mip = 0;
		} downsampled_pass_data;

		struct DownscaledWaterData
		{
			std::unique_ptr<Device::RenderTarget> downsampled_depth;
			std::unique_ptr<Device::RenderTarget> downsampled_color;
			std::unique_ptr<Device::RenderTarget> target_color;

			int downsample_scale_data;
		} downscaled_water_data;

		struct Reprojection
		{
			struct Layer
			{
				std::unique_ptr<Device::RenderTarget> direct_light_data;
				std::unique_ptr<Device::RenderTarget> normal_data;
				std::unique_ptr<Device::RenderTarget> depth_data;

				simd::matrix viewproj_matrix;
				simd::matrix inv_viewproj_matrix;
				simd::matrix proj_matrix;
				simd::matrix inv_proj_matrix;
				simd::vector4 frame_to_dynamic_scale = 0.0f;
			};
			Layer layers[2];
			Layer *curr_layer, *prev_layer;
		} reprojection_data;

		struct BloomData
		{
			std::unique_ptr<Device::RenderTarget> bloom;
			std::vector<std::unique_ptr<Device::RenderTarget> > bloom_mips;

			std::unique_ptr<Device::RenderTarget> tmp_bloom;
			std::vector<std::unique_ptr<Device::RenderTarget> > tmp_bloom_mips;
			int downsample;

			float cutoff = 0.0f;
			float intensity = 0.0f;
			float weight_mult = 0.0f;
		} bloom_data;

		struct DOFData
		{
			std::unique_ptr<Device::RenderTarget> foreground_tmp;
			std::unique_ptr<Device::RenderTarget> background_tmp;
			std::unique_ptr<Device::RenderTarget> foreground;
			std::unique_ptr<Device::RenderTarget> background;

			simd::vector4 distances = 0.0f;
			bool active = false;
			float focus_distance = 0.0f;
			float focus_range = 0.0f;
			float blur_scale = 0.0f;
			float blur_foreground_scale = 0.0f;
			float background_transition = 0.0f;
			float foreground_transition = 0.0f;
			size_t downsample_mip = 0;
			size_t downsample = 0;
		} dof_data;

		struct RainData
		{
			float dist = 0.0f;
			float amount = 0.0f;
			float intensity = 1.0f;
			simd::vector4 fall_dir = simd::vector4(0.0f, 0.0f, 1.0f, 0.0f);
			float turbulence = 0.1f;
		} rain;

		struct CloudsData
		{
			float scale = 0.0f;
			float intensity = 0.0f;
			float midpoint = 0.0f;
			float sharpness = 0.0f;
			simd::vector4 velocity = 0.0f;
			float fade_radius = 0.0f;
			float pre_fade = 0.0f;
			float post_fade = 0.0f;
		} clouds;

		struct WaterData
		{
			simd::vector4 color_open = 0.0f;
			simd::vector4 color_terrain = 0.0f;
			simd::vector4 wind_direction = 0.0f;
			float subsurface_scattering = 0.0f;
			float caustics_mult = 0.0f;
			float refraction_index = 0.0f;
			float dispersion = 0.0f;
			float clarity = 0.0f;
			float skybox_exposure = 0.0f;
			float reflectiveness = 0.0f;
			float wind_intensity = 0.0f;
			float wind_speed = 0.0f;
			float directionness = 0.0f;
			float swell_intensity = 0.0f;
			float swell_height = 0.0f;
			float swell_angle = 0.0f;
			float swell_period = 0.0f;
			float flow_intensity = 0.0f;
			float flow_foam = 0.0f;
			float flow_turbulence = 0.0f;

			float height_offset = 0.0f;
		} water;

		float indirect_light_rampup = 0.0f;
		float indirect_light_area = 0.0f;
		float ambient_occlusion_power = 0.0f;
		float screenspace_thickness_angle = 0.0f;

		simd::vector4 ground_scalemove_uv = simd::vector4(0.005f, 0.005f, 0.f, 0.f);
		simd::vector4 dust_color = simd::vector4(0.0f, 0.0f, 0.0f, 0.0f);

		void SetGlobalIlluminationSettings( const Environment::Data& settings );
		void SetRainSettings(const Environment::Data& settings);
		void SetCloudsSettings(const Environment::Data& settings);
		void SetWaterSettings(const Environment::Data& settings);
		void SetAreaSettings(const Environment::Data& settings, const float time_scale, const simd::vector2* tile_scroll_override );
		void SetBloomSettings(const Environment::Data& settings);
		void SetDofSettings(const Environment::Data& settings);
		void SetPostProcessSettings(const Environment::Data& settings);

		template <typename... ARGS>
		MetaPass& AddMetaPass(ARGS... args);
		void KickMetaPass(const std::function<void(MetaPass&)>& record, MetaPass& meta_pass);

		void AddComputeMetaPass(const std::function<void(MetaPass&)>& record, std::shared_ptr<CubemapTask> cubemap_task);
		void AddShadowsMetaPasses(const std::function<void(MetaPass&)>& record, bool shadows_enabled);
		void AddCubemapMetaPasses(const std::function<void(MetaPass&)>& record, std::shared_ptr<CubemapTask> cubemap_task);
		void AddZPrePassMetaPass(const std::function<void(MetaPass&)>& record);
		void AddOpaqueMetaPass(const std::function<void(MetaPass&)>& record, simd::color clear_color, bool ss_high);
		void AddWaterMetaPass(const std::function<void(MetaPass&)>& record, bool water_downscaled, bool ss_high);
		void AddAlphaMetaPasses(const std::function<void(MetaPass&)>& record, bool gi_enabled, bool ss_high);
		void AddGIMetaPass(const std::function<void(MetaPass&)>& record, bool gi_enabled, bool gi_high);
		void AddScreenSpaceShadowsMetaPass(const std::function<void(MetaPass&)>& record);
		void AddPostProcessMetaPass(const std::function<void(MetaPass&)>& record, bool dof_enabled, float bloom_strength, bool time_manipulation_effects);
		void AddUIOnlyMetaPass(const std::function<void(MetaPass&)>& record, simd::color clear_color);

		Pass BuildMipBuilderPass(std::vector<Device::RenderTarget*> dst_mips, TargetResampler::FilterTypes filter_type, float gamma);
		Pass BuildDownsampledPass(Device::RenderTarget *zero_data, TargetResampler::RenderTargetList mip_targets);
		Pass BuildDepthResamplerPass(Device::RenderTarget* src_target, Device::RenderTarget* dst_depth_target, TargetResampler::FilterTypes filter_type = TargetResampler::FilterTypes::Nearest);
		Pass BuildComputePass(std::shared_ptr<CubemapTask> cubemap_task);
		Pass BuildComputePostPass();
		Pass BuildShadowPass(Scene::Light* light, unsigned light_index);
		Pass BuildZPrePass();
		Pass BuildCubemapPass(std::shared_ptr<CubemapTask> cubemap_task, Device::RenderTarget *target, Device::RenderTarget *depth_target, unsigned int face_id);
		Pass BuildWetnessPass(bool ss_high);
		Pass BuildDownscaledPass(bool ss_high);
		Pass BuildColorPass(const Memory::SmallVector<DrawCalls::BlendMode, 8, Memory::Tag::Render>& blend_modes, bool ss_high, bool clear = false, simd::color clear_color = 0);
		Pass BuildDownscaledWaterPass(DrawCalls::BlendMode blend_mode, bool ss_high);
		Pass BuildMainWaterPass(DrawCalls::BlendMode blend_mode, bool ss_high);
		Pass BuildColorResamplerPass(Device::RenderTarget* src_target, Device::RenderTarget* dst_target);
		Pass BuildLinearizeDepthPass(Device::RenderTarget* src_target, Device::RenderTarget* dst_target, int downscale);
		Pass BuildBlurPass(std::vector<Device::RenderTarget*> src_mips, std::vector<Device::RenderTarget*> tmp_mips, float _gamma);
		Pass BuildDepthAwareDownsamplerPass();
		Pass BuildUpsampleGBufferPass();
		Pass BuildDirectLightPostProcessPass(Device::RenderTarget* src_target, Device::RenderTarget* dst_target, int downscale);
		Pass BuildDesaturationPass();
		Pass BuildShimmerPass();
		Pass BuildSMAAPass();
		Pass BuildReprojectorPass(Device::RenderTarget* src_target, Device::RenderTarget* dst_target, int downscale, bool use_high_quality);
		Pass BuildGlobalIlluminationPass();
		Pass BuildScreenspaceShadowsPass();
		Pass BuildBloomPass(float bloom_strength);
		Pass BuildDoFPass(bool dof_active, bool time_manipulation_effects);
		Pass BuildPostProcessPass(bool dof_active, bool time_manipulation_effects);
		Pass BuildWaitPass();
		Pass BuildUIRenderToTexturePass();
		Pass BuildUIPass();
		Pass BuildDebugUIPass();

		Device::UniformInputs BuildUniformInputs();
		Device::BindingInputs BuildBindingInputs();

		void AddNullDepthDrawData(Device::UniformInputs& pass_uniform_inputs, Device::BindingInputs& pass_binding_inputs);
		void AddNullWaterDrawData(Device::UniformInputs& pass_uniform_inputs, Device::BindingInputs& pass_binding_inputs);
		void AddNullGlobalIlluminationDrawData(Device::UniformInputs& pass_uniform_inputs, Device::BindingInputs& pass_binding_inputs);
		void AddResolvedDepthDrawData(Device::UniformInputs& pass_uniform_inputs, Device::BindingInputs& pass_binding_inputs, uint32_t downsample_mip = 1);
		void AddScreenspaceDrawData(Device::UniformInputs& pass_uniform_inputs, Device::BindingInputs& pass_binding_inputs, bool ss_high);
		void AddWaterDrawData(Device::UniformInputs& pass_uniform_inputs, Device::BindingInputs& pass_binding_inputs, bool use_downsampled_data);

		void GetAmbientLightColor(simd::vector4& out_light_color, simd::vector4& out_light_dir);

		Device::RenderTarget* GetMainTarget();

		Device::ClearTarget GetMainClearFlags(bool clear) const;

	public:
		RenderGraph();

		void OnCreateDevice(Device::IDevice* const device);
		void OnResetDevice(Device::IDevice* const device, bool gi_enabled, bool use_wide_pixel_format, size_t downscaling_mip, bool dof_enabled, bool smaa_enabled, bool smaa_high);
		void OnLostDevice();
		void OnDestroyDevice();

		void Update(float frame_length);

		void Reload(); // [TODO] Remove.

		void Record(
			const std::function<void(MetaPass&)>& record,
			simd::color clear_color, 
			bool shadows_enabled,
			bool gi_enabled,
			bool gi_high,
			bool ss_high,
			bool dof_active,
			bool water_downscaled,
			float bloom_strength,
			bool time_manipulation_effects,
			std::function<void()> callback_render_to_texture,
			std::function<void(Device::Pass*)> callback_render);
		void Flush(const std::function<void(MetaPass&)>& flush);

		void SetEnvironmentSettings(const Environment::Data& settings, const float time_scale, const simd::vector2* tile_scroll_override );

		Device::Handle<Device::Pass> FindPass(const Memory::DebugStringA<>& name, Device::ColorRenderTargets render_targets, Device::RenderTarget* depth_stencil, Device::ClearTarget clear_target, simd::color clear_color, float clear_z);

		Device::Handle<Device::Pass> GetZPrePass();
		Device::Handle<Device::Pass> GetMainPass(bool use_gi);

		std::pair<Device::UniformInputs, Device::BindingInputs> BuildGlobalInputs();

		void SetPostProcessCamera( Scene::Camera* camera );
		void SetSceneData(simd::vector2 scene_size, simd::vector2_int scene_tiles_count, const Scene::OceanData &ocean_data, const Scene::RiverData &river_data, const Scene::FogBlockingData &fog_blocking_data);
		simd::vector4 GetSceneData(simd::vector2 planar_pos);
		void RenderDebugInfo(const simd::matrix & view_projection);

		Device::RenderTarget* GetBackBuffer() const;
		TargetResampler& GetResampler();
		PostProcess& GetPostProcess();

	};

}
