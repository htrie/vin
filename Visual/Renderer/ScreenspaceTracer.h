#pragma once

#include "Visual/Device/Shader.h"
#include "Visual/Device/Texture.h"
#include "Visual/Renderer/Scene/PointLine.h"
#include "Visual/Renderer/Scene/WaterMarkupData.h"

namespace TexUtil
{
	struct Color128;
}

namespace Physics
{
	struct FluidSystem;
}

namespace Renderer
{
	namespace Scene
	{
		class Camera;
		class ShoreLine;
		struct WaterFlowConstraint;

		struct OceanData;
		struct RiverData;
		struct FogBlockingData;
	}

	struct PointPhase
	{
		float dist;
		float longitude;
		float intensity;
	};

	class PhaseLine
	{
	public:
		void LoadLineData(const PointLine& line_data, float smooth_ratio, int smooth_iterations, float min_curvature_radius);
		void LoadLineData(const PointLine& line_data);
		PointPhase GetPointPhase(Vector2d plane_point, bool attenuate_longitude_discontinuity) const;
		bool IsLongitudeContinuous(float longitude0, float longitude1) const;
		void RenderWireframe(const simd::matrix& view_projection, float veritcal_offset, simd::color color) const;
		float GetSumLongitude() const;
		size_t GetPointsCount() const;
		PointLine& GetBaseLine();
	private:
		PointPhase GetPointPhaseInternal(Vector2d plane_point, bool attenuate_longitude_discontinuity, size_t line_index) const;
		PointLine base_line_data;

		std::vector<float> longitudes;
		float sum_longitude;
	};

	class ScreenspaceTracer
	{
	public:
		ScreenspaceTracer();
		~ScreenspaceTracer();

		//Device state change functions
		void OnCreateDevice(Device::IDevice *device);
		void OnResetDevice(Device::IDevice *device);
		void OnLostDevice();
		void OnDestroyDevice();
		void SetCamera(Scene::Camera* camera);

		void LoadSceneData(
			Device::IDevice *device,
			simd::vector2 scene_size,
			simd::vector2_int scene_tiles_count,
			const Scene::OceanData &ocean_data,
			const Scene::RiverData &river_data,
			const Scene::FogBlockingData &fog_blocking_data);

		simd::vector4 GetSceneData(simd::vector2 planar_pos);

		void UpdateAsyncResources();
		void StopAsyncThreads();

		void RenderWireframe(const simd::matrix & view_projection);

		void OverrideDepth(
			Device::IDevice* device, 
			Device::CommandBuffer& command_buffer, 
			Device::Pass* pass, 
			float depth_bias, 
			float slope_scale);

		void ComputeGlobalIllumination(
			Device::IDevice * device,
			Device::CommandBuffer& command_buffer,
			Device::Handle<Device::Texture> color_tex,
			Device::Handle<Device::Texture> depth_mip_tex,
			Device::Handle<Device::Texture> linear_depth_tex,
			Device::Handle<Device::Texture> normal_mip_tex,
			simd::vector2_int viewport_size,
			simd::vector2 frame_to_dynamic_scale,
			size_t mip_level,
			float indirect_light_area,
			float indirect_light_rampup,
			float ambient_occlusion_power,
			float thickness_angle,
			float curr_time,
			int detail_level,
			Device::RenderTarget *dst_ambient_data);

		void ComputeRays(
			Device::IDevice * device,
			Device::CommandBuffer& command_buffer,
			Device::Handle<Device::Texture> zero_depth_tex,
			Device::Handle<Device::Texture> zero_ray_dir_tex,
			Device::Handle<Device::Texture> linear_depth_tex,
			Device::Handle<Device::Texture> color_tex,
			Device::RenderTarget *dst_hit_mip,
			simd::vector2_int viewport_size, size_t mip_level, size_t downscale_mip_level, simd::vector2 frame_to_dynamic_scale, bool clear);

		void ComputeScreenspaceShadowsChannel(
			Device::IDevice * device,
			Device::CommandBuffer& command_buffer,
			Device::Handle<Device::Texture> depth_mip_tex,
			Device::Handle<Device::Texture> pixel_offset_tex,
			Device::Handle<Device::Texture> linear_depth_tex,
			simd::vector2 frame_to_dynamic_scale,
			simd::vector4 *position_data, simd::vector4 *direction_data, simd::vector4 *color_data, size_t lights_count,
			Device::RenderTarget *dst_shadowmap, simd::vector2_int viewport_size, size_t mip_level, size_t downscale_mip_level, float mip_offset, int target_channel, bool clear);

		void ComputeBloomCutoff(
			Device::IDevice* device,
			Device::CommandBuffer& command_buffer,
			Device::RenderTarget* src_tex,
			Device::RenderTarget* dst_bloom,
			float cutoff,
			float intensity,
			simd::vector2 frame_to_dynamic_scale);

		void ComputeDoFLayers(
			Device::IDevice* device,
			Device::CommandBuffer& command_buffer,
			Device::Texture* src_texture,
			Device::Texture* depth_mip_tex,
			Device::RenderTarget* foreground,
			Device::RenderTarget* background,
			float focus_range,
			float focus_distance,
			float transition_background,
			float transition_foreground,
			uint32_t mip_level,
			simd::vector2 frame_to_dynamic_scale);

		void ComputeDoFBlur(
			Device::IDevice* device,
			Device::CommandBuffer& command_buffer,
			Device::Texture* src_tex,
			Device::RenderTarget* dst_target,
			simd::vector2 radius,
			simd::vector2 frame_to_dynamic_scale);

		void ComputeBloomGathering(
			Device::IDevice* device,
			Device::CommandBuffer& command_buffer,
			Device::Handle<Device::Texture> bloom_mips_tex,
			float weight_mult,
			simd::vector2 frame_to_dynamic_scale,
			Device::RenderTarget* dst_bloom);

		void ApplyDepthAwareBlur(
			Device::IDevice * device,
			Device::CommandBuffer& command_buffer,
			Device::Handle<Device::Texture> data_mip_tex,
			Device::Handle<Device::Texture> depth_mip_tex,
			Device::Handle<Device::Texture> normal_mip_tex,
			Device::RenderTarget * dst_data_mip,
			simd::vector2 frame_to_dynamic_scale,
			simd::vector2_int radii,
			simd::vector2_int base_size,
			size_t mip_level,
			float gamma = 1.0f);

		void ApplyBlur(
			Device::IDevice * device,
			Device::CommandBuffer& command_buffer,
			Device::RenderTarget *src_data,
			Device::RenderTarget *dst_data,
			float gamma,
			float exp_mult,
			simd::vector2 frame_to_dynamic_scale,
			simd::vector2_int radii);


		void LinearizeDepth(
			Device::IDevice * device,
			Device::CommandBuffer& command_buffer,
			Device::Handle<Device::Texture> depth_tex,
			Device::RenderTarget *dst_linear_depth,
			simd::vector2 frame_to_dynamic_scale,
			simd::vector2_int base_size,
			int mip_level, int downscale);

		void Reproject(
			Device::IDevice * device,
			Device::CommandBuffer& command_buffer,
			Device::Handle<Device::Texture> curr_depth_tex,
			Device::Handle<Device::Texture> prev_depth_tex,
			Device::Handle<Device::Texture> prev_data_tex,
			Device::RenderTarget *dst_data,
			simd::vector2 curr_frame_to_dynamic_scale,
			simd::vector2 prev_frame_to_dynamic_scale,
			simd::matrix proj_matrix,
			simd::matrix curr_viewproj_matrix,
			simd::matrix prev_viewproj_matrix,
			bool use_high_quality,
			simd::vector2_int base_size, int mip_level, int downscale);

		bool AddDepthProjectionDrawData(
			Device::UniformInputs& pass_uniform_inputs,
			Device::BindingInputs& pass_binding_inputs,
			Device::Handle<Device::Texture> depth_texture,
			bool use_depth_driver_hack = false);

		bool AddNullWaterDrawData(
			Device::UniformInputs& pass_uniform_inputs,
			Device::BindingInputs& pass_binding_inputs,
			simd::vector4 ambient_light_color,
			simd::vector4 ambient_light_dir,
			float curr_time);

		bool AddShorelineDrawData(
			Device::UniformInputs& pass_uniform_inputs,
			Device::BindingInputs& type_binding_inputs);

		bool AddWaterDrawData(
			Device::UniformInputs& pass_uniform_inputs,
			Device::BindingInputs& pass_binding_inputs,
			Device::Handle<Device::Texture> opaque_framebuffer_tex,
			simd::vector4 ambient_light_color,
			simd::vector4 ambient_light_dir,
			float curr_time,
			bool use_downsampling);

		bool AddScreenspaceDrawData(
			Device::UniformInputs& pass_uniform_inputs,
			Device::BindingInputs& pass_binding_inputs,
			Device::Handle<Device::Texture> gi_ambient_texture,
			Device::Handle<Device::Texture> ss_shadowmap_texture,
			Device::Handle<Device::Texture> ray_hit_texture,
			int ssgi_detail);

		void reload(Device::IDevice * device);

		float GetMaxDoFBlurSigma() const { return 4.0f; }

	private:
		::Texture::Handle ripple_texture;
		::Texture::Handle wave_profile_texture;
		::Texture::Handle wave_profile_gradient_texture;
		::Texture::Handle distorton_texture;
		::Texture::Handle surface_foam_texture;
		::Texture::Handle caustics_texture;
		::Texture::Handle photometric_profiles_texture;

		Device::VertexDeclaration vertex_declaration;

		Device::Handle<Device::VertexBuffer> vertex_buffer;
		Device::Handle<Device::Shader> vertex_shader;

		Scene::Camera* camera;


		PhaseLine shallow_phaseline;
		PhaseLine rocky_phaseline;
		PhaseLine deep_phaseline;

		std::vector<::Renderer::Scene::WaterFlowConstraint> water_flow_constraints;
		std::vector<::Renderer::Scene::WaterSourceConstraint> water_source_constraints;
		std::vector<::Renderer::Scene::WaterBlockingConstraint> water_blocking_constraints;

		struct RiverFlowmapData;

		struct FlowmapData
		{
			std::vector<TexUtil::Color128> data;
			simd::vector2_int tex_size;
			std::atomic_bool is_pending = { false };
			std::atomic_bool is_running;
			Device::Handle<Device::Texture> texture;
			simd::vector4 tex_size_draw_data;
			simd::vector4 world_size_draw_data;

			std::unique_ptr<RiverFlowmapData> river_data;
			std::unique_ptr<Physics::FluidSystem> fluid_system;

			int version;
		} river_flowmap_data, fog_flowmap_data;

		int river_flowmap_version;
		int fog_flowmap_version;

		std::unique_ptr<RiverFlowmapData> GetFlowmapNodeData(float shoreline_offset, float initial_turbulence);
		void BuildShorelineTexture(Device::IDevice *device, simd::vector2 scene_size, simd::vector2_int scene_tiles_count, size_t texel_per_tile_count, float shoreline_offset);
		void BuildRiverFlowmapTexture(Device::IDevice *device, simd::vector2 scene_size, simd::vector2_int scene_tiles_count, size_t texels_per_tile_count, float shoreline_offset, float initial_turbulence, bool wait_for_initialization);
		std::unique_ptr<Physics::FluidSystem> InitializeFluidSystem(RiverFlowmapData& data);
		void FluidSystemIteration(int index, RiverFlowmapData& data, Physics::FluidSystem* fluid_system);
		void BuildFogFlowmapTexture(Device::IDevice *device, const Scene::FogBlockingData &fog_blocking_data);
		void BuildFogFlowmap(const Scene::FogBlockingData &fog_blocking_data);

		void AddWaterDrawDataUniforms(
			Device::UniformInputs& pass_uniform_inputs,
			simd::vector4 ambient_light_color,
			simd::vector4 ambient_light_dir,
			float curr_time,
			bool use_downsampling);

		float sum_longitude;
		float dist_to_color;

		PhaseLine river_bank_phaseline;

		simd::vector2 scene_size;
		int ocean_data_id;
		int river_data_id;

		float flow_to_color;

		Device::IDevice* device;

	#pragma pack(push)
	#pragma pack(1)
		struct PassKey
		{
			Device::ColorRenderTargets render_targets;
			Device::PointerID<Device::RenderTarget> depth_stencil;
			bool clear = false;
			simd::color clear_color;

			PassKey() {}
			PassKey(Device::ColorRenderTargets render_targets, Device::RenderTarget* depth_stencil, bool clear, simd::color clear_color);
		};
	#pragma pack(pop)
		Device::Cache<PassKey, Device::Pass> passes;
		Memory::Mutex passes_mutex;
		Device::Pass* FindPass(const Memory::DebugStringA<>& name, Device::ColorRenderTargets render_targets, Device::RenderTarget* depth_stencil, bool clear = false, simd::color clear_color = simd::color(0));

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

		struct DepthOverrider
		{
			Device::Handle<Device::Shader> pixel_shader;
		}
		depth_overrider;

		struct GlobalIllumination
		{
			Device::Handle<Device::Shader> pixel_shader;
		}
		global_illumination;

		struct ScreenpsaceShadowsKey
		{
			ScreenpsaceShadowsKey(bool use_offset_tex, size_t mip_level) :
				use_offset_tex(use_offset_tex), mip_level(mip_level){}
			bool operator <(const ScreenpsaceShadowsKey &other) const
			{
				return std::tie(use_offset_tex, mip_level) < std::tie(other.use_offset_tex, other.mip_level);
			}
			bool use_offset_tex;
			size_t mip_level;
		};
		struct ScreenspaceShadows
		{
			Device::Handle<Device::Shader> pixel_shader;
		};
		std::map<ScreenpsaceShadowsKey, ScreenspaceShadows> screenspace_shadows;


		struct ScreenspaceRaysKey
		{
			ScreenspaceRaysKey(size_t mip_level) :
				mip_level(mip_level) {}
			bool operator <(const ScreenspaceRaysKey &other) const
			{
				return std::tie(mip_level) < std::tie(other.mip_level);
			}
			size_t mip_level;
		};
		struct ScreenspaceRays
		{
			Device::Handle<Device::Shader> pixel_shader;
		};
		std::map<ScreenspaceRaysKey, ScreenspaceRays> screenspace_rays;


		struct DepthAwareBlur
		{
			Device::Handle<Device::Shader> pixel_shader;
		};

		DepthAwareBlur depth_aware_blurs[2];
		DepthAwareBlur *depth_aware_blur_linear;
		DepthAwareBlur *depth_aware_blur_square;

		struct Blur
		{
			Device::Handle<Device::Shader> pixel_shader;
		};
		
		Blur blurs[2];
		Blur* blur_8;
		Blur* blur_fp32;

		struct DepthLinearizerKey
		{
			DepthLinearizerKey(int _downscale) :
				downscale(_downscale)
			{
			}
			int downscale;
			bool operator < (const DepthLinearizerKey &other) const
			{
				return std::tie(downscale) < std::tie(other.downscale);
			}
		};
		struct DepthLinearizer
		{
			Device::Handle<Device::Shader> pixel_shader;
		};
		std::map<DepthLinearizerKey, DepthLinearizer> depth_linearizers;

		struct ReprojectorKey
		{
			ReprojectorKey(bool _use_world_space, bool _use_view_space) :
				use_world_space(_use_world_space), use_view_space(_use_view_space)
			{
			}
			bool use_world_space;
			bool use_view_space;
			bool operator < (const ReprojectorKey &other) const
			{
				return std::tie(use_world_space, use_view_space) < std::tie(other.use_world_space, other.use_view_space);
			}
		};
		struct Reprojector
		{
			Device::Handle<Device::Shader> pixel_shader;
		};
		std::map<ReprojectorKey, Reprojector> reprojectors;

		struct BloomCutoff
		{
			Device::Handle<Device::Shader> pixel_shader;
		};
		BloomCutoff bloom_cutoff;

		struct DoFLayers
		{
			Device::Handle<Device::Shader> pixel_shader;
		};
		DoFLayers dof_layers;
		
		struct DoFBlur
		{
			Device::Handle<Device::Shader> pixel_shader;
		};
		DoFBlur dof_blur;

		struct BloomGather
		{
			Device::Handle<Device::Shader> pixel_shader;
		};
		BloomGather bloom_gather;

		Device::Handle<Device::Texture> default_ambient_tex;
		float global_illumination_intensity;
		float global_illumination_debug;

		float ssgi_detail;
		simd::vector4 prev_frame_to_viewport_scale;

		struct ReprojectionDrawData
		{
			float type_0;
			float type_1;
			float type_2;
			simd::vector4 resampled_dynamic_resolution;
			simd::vector4 backbuffer_resolution;
		} reprojection_draw_data;

		struct WaterDrawData
		{
			simd::matrix projection_matrix;
			simd::matrix inv_projection_matrix;
			simd::matrix view_matrix;
			simd::matrix inv_view_matrix;

			Device::Handle<Device::Texture> shoreline_coords_tex;

			float current_time;
			float numerical_normal;
			float sum_longitude;
			float dist_to_color;
			float flow_to_color;

			simd::vector4 ambient_light_color;
			simd::vector4 ambient_light_dir;

			simd::vector4 scene_size;
			simd::vector4 shoreline_coords_tex_size;

			std::vector<simd::vector4> shoreline_tex_data;
			simd::vector2_int shoreline_tex_data_size = simd::vector2_int(0, 0);
		} water_draw_data;

		bool bool_true;
		bool bool_false;
	};
}