#pragma once

#include "Visual/Device/State.h"
#include "Visual/Device/Shader.h"
#include "Visual/Device/VertexBuffer.h"
#include "Visual/Device/VertexDeclaration.h"
#include "Visual/Texture/TextureSystem.h"

namespace Device
{
	class Effect;
	class CommandBuffer;
};

namespace Renderer
{
	namespace Scene
	{
		class Camera;
	}
	
	class PostProcess
	{
	public:
		enum struct OverlayTypes
		{
			Blood,
			Poison,
			Royale,
			Incursion,
			Max
		};
		struct BlurParams
		{
			BlurParams();
			BlurParams operator *(float val);
			BlurParams operator +(const BlurParams &other);
			simd::vector2 intensity;
			float wobbliness;
			float curvature;
		};

		struct DoFParams
		{
			Device::Texture* background;
			Device::Texture* foreground;
			float focus_distance;
			float focus_range;
			float background_transition;
			float foreground_transition;

			DoFParams() = default;
			DoFParams(Device::Texture* background, Device::Texture* foreground, float focus_distance, float focus_range, float background_transition, float foreground_transition)
				: background(background), foreground(foreground), focus_distance(focus_distance), focus_range(focus_range)
				, background_transition(background_transition), foreground_transition(foreground_transition) {}
		};

	private:
		::Texture::Handle post_transforms[2];
		float post_transform_ratio = 0.0f;

		float fade_duration;
		float fade_delay;
		float elapsed_fade_time;
		float breach_time;
		float curr_time;
		bool fade_in;
		bool breach_effect_on;

		simd::vector4 breach_sphere_info;
		simd::vector4 sphere_muddle_vals;

		float original_intensity;

		Device::VertexDeclaration vertex_declaration;

		Device::Handle<Device::VertexBuffer> vb_post_process;

		Device::Handle<Device::Shader> vertex_shader;

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
		Device::Pass* FindPass(const Memory::DebugStringA<>& name, Device::IDevice* device, Device::ColorRenderTargets render_targets, Device::RenderTarget* depth_stencil);

	#pragma pack(push)	
	#pragma pack(1)
		struct PipelineKey
		{
			Device::PointerID<Device::Pass> pass;
			Device::PointerID<Device::Shader> pixel_shader;

			PipelineKey() {}
			PipelineKey(Device::Pass* pass, Device::Shader* pixel_shader);
		};
	#pragma pack(pop)
		Device::Cache<PipelineKey, Device::Pipeline> pipelines;
		Memory::Mutex pipelines_mutex;
		Device::Pipeline* FindPipeline(const Memory::DebugStringA<>& name, Device::IDevice* device, Device::Pass* pass, Device::Shader* pixel_shader);

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
		Device::DescriptorSet* FindDescriptorSet(const Memory::DebugStringA<>& name, Device::IDevice* device, Device::Pipeline* pipeline, Device::DynamicBindingSet* pixel_binding_set);

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

		struct PostProcessShader
		{
			Device::Handle<Device::Shader> pixel_shader;
		};
		struct PostProcessKey
		{
			PostProcessKey(bool _is_basic) : is_basic(_is_basic) {}
			bool operator < (const PostProcessKey &other) const
			{
				return std::tie(is_basic) < std::tie(other.is_basic);
			}
			bool is_basic;
		};
		std::map<PostProcessKey, PostProcessShader> post_process_shaders;

		Device::Handle<Device::Shader> upscale_shader;

		::Texture::Handle desaturation_post_transform;

		// breach specific
		::Texture::Handle breach_post_transform;
		::Texture::Handle breach_edge;
		::Texture::Handle breach_muddle;
		::Texture::Handle post_transform_identity;
		::Texture::Handle overlay_texture[( unsigned ) OverlayTypes::Max];
		::Texture::Handle crack_sdm_texture;
		::Texture::Handle space_texture;
		::Texture::Handle palette_texture;
		::Texture::Handle decay_background_texture;
		Scene::Camera* camera;

		OverlayTypes overlay_type;
		float overlay_intensity;
		float overlay_target;
		float overlay_speed;

		float exposure;

		BlurParams start_blur_params;
		BlurParams target_blur_params;
		float blur_transition_ratio;
		float blur_transition_duration;

	public:
		void Update( const float elapsed_time );

		void SetOriginalIntensity(float intensity) { original_intensity = intensity; }
		void SetPostTransformVolumes(::Texture::Handle textures[2], float ratio);
		void SetExposure(float exposure) { this->exposure = exposure; }

		void FadeIn( const float fade_time, const float delay = 0.0f );
		void FadeOut( const float fade_time, const float delay = 0.0f );

		void SetBlurParams( BlurParams params, const float time = 0.0f );
		BlurParams GetCurrBlurParams();

		void SetOverlayType( OverlayTypes overlay_type, float intenstiy, float time = 0.0f );
		float GetOverlayIntensity() const { return overlay_intensity; }

		void SetCamera( Scene::Camera* _camera ) { camera = _camera;  }
		void SetBreachSphereInfo( const simd::vector3& position, const float radius );
		void ToggleBreachEffect( bool enable );
		bool IsBreachEffectEnabled() const { return breach_effect_on && camera; }

		PostProcess();

		struct SynthesisMapInfo
		{
			Device::Handle<Device::Texture> decay_tex;
			Device::Handle<Device::Texture> stable_tex;
			simd::vector2_int size;
			simd::vector2 min_point, max_point;
			float decay_time;
			int decay_type;
			float creation_time;
			float global_stability;
			simd::vector4 player_position;
			simd::vector4 stabiliser_position;
		};

		void RenderPostProcessedImage(
			Device::IDevice* device,
			Device::CommandBuffer& command_buffer,
			Device::Texture* src,
			Device::Texture* shimmer,
			Device::Texture* desaturation,
			Device::Texture* bloom,
			SynthesisMapInfo synthesis_map_info,
			Device::Texture* depth,
			const DoFParams* dof_params,
			Device::RenderTarget* dst,
			bool base_effects_only,
			bool time_manipulation_effects,
			simd::vector2 src_frame_to_dynamic_scale,
			const simd::vector4& ritual_data,
			float ritual_ratio );

		void UpscalePostProcessedImage(
			Device::IDevice* device,
			Device::CommandBuffer& command_buffer,
			Device::Texture* src,
			Device::RenderTarget* dst,
			simd::vector2 src_frame_to_dynamic_scale);

		void OnCreateDevice(Device::IDevice* device);
		void OnResetDevice(Device::IDevice* device);
		void OnLostDevice();
		void OnDestroyDevice();

		float GetFadeDuration() { return fade_duration; }

		void reload(Device::IDevice *device);
	};

}

