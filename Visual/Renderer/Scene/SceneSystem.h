#pragma once

#include "Common/Utility/NonCopyable.h"
#include "Common/Utility/Event.h"
#include "Common/Utility/System.h"

#include "Visual/Device/Shader.h"
#include "Visual/Device/VertexDeclaration.h"
#include "Visual/Renderer/RendererSettings.h"
#include "Visual/Renderer/TiledLighting.h"
#include "Visual/Renderer/PostProcess.h"
#include "Visual/Renderer/CubemapManager.h"
#include "Visual/Renderer/ShadowManager.h"
#include "Visual/Renderer/DynamicParameterManager.h"
#include "Visual/Particles/ParticleEffect.h"

#include "Light.h"

namespace Device
{
	class RenderTarget;
}

namespace Particles
{
	class EffectInstance;
}

namespace Visual
{
	class OrphanedEffectsManager;
}

namespace Environment
{
	struct Data;
}

namespace Renderer
{
	struct CubemapTask;
	class ShadowManager;

	namespace DrawCalls
	{
		class DrawCallTypeManager;
	}

	namespace Lighting
	{
		class ILight;
		class Graph;
	}


	namespace Scene
	{
		class SceneDataSource;
		class Camera;
		struct SynthesisMapData;
		struct WaterMapData;
		struct RoomOcclusionData;

		struct Stats
		{
			unsigned light_count = 0;
			unsigned point_light_count = 0;
			unsigned point_light_shadow_count = 0;
			unsigned spot_light_count = 0;
			unsigned spot_light_shadow_count = 0;
			unsigned dir_light_count = 0;
			unsigned dir_light_shadow_count = 0;

			unsigned visible_point_light_count = 0;
			unsigned visible_non_point_light_count = 0;
			unsigned visible_non_point_shadow_light_count = 0;

			size_t uniforms_size = 0;
			size_t uniforms_max_size = 0;

			uint8_t visible_layers = 0;
		};

		enum Layer
		{
			Main = 1 << 0,
			Shop = 1 << 1,

			All = 0xFF // Max uint8_t.
		};


		class SubScene
		{
			BoundingBox box = BoundingBox(-std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
			simd::matrix transform = simd::matrix::identity();

		public:
			SubScene() noexcept {}
			SubScene(const BoundingBox& box, const simd::matrix& transform);

			const BoundingBox& GetBox() const { return box; }
			const simd::matrix& GetTransform() const { return transform; }
		};

		class Impl;

		class System : public ImplSystem<Impl, Memory::Tag::Render>
		{
			System();

		public:
			static System& Get();

		public:
			void Init(const Settings& settings, bool tight_buffers);

			void Swap() final;
			void Clear() final;

			Stats GetStats() const;

			void OnCreateDevice( Device::IDevice *device );
			void OnResetDevice( Device::IDevice *_device );
			void OnLostDevice( );
			void OnDestroyDevice( );

			void SetRendererSettings(const Settings& settings);

			/// Sets the scene input source where all scene data will come from
			void SetSceneDataSource( SceneDataSource* scene_data_source );
			
			void RestartDataSource();

#ifdef PROFILING
			void ToggleForceDisableMerging() { force_disable_merging = !force_disable_merging; }
			void SetDebugOverlayStrength(float v) { debug_overlay_strength = v; }
			float GetDebugOverlayStrength() const { return debug_overlay_strength; }
#endif

			void ComputeTiles(Device::CommandBuffer& command_buffer, std::shared_ptr<CubemapTask> cubemap_task);

			void FrameMoveBegin(float delta_time, bool render_world);
			void FrameMoveEnd(float delta_time);

			void FrameRenderBegin();
			void FrameRenderEnd();

			Device::UniformAlloc AllocateUniforms(size_t size);

			void UpdateBurnTexture(float dt);

			/// Sets the primary data that the scene will be rendered from
			void SetCamera(Camera* camera);
			const Camera* GetCamera() const;
			Camera* GetCamera();

			void SetPlayerPosition(simd::vector3 pos);

			Device::IDevice *GetDevice() const { return device; }

			float GetFrameTime( ) const { return frame_time; }
			void ResetFrameTime();
			void ResetToolTime(); // Tool only function, globally resets per-object start_time to current time

			PointLights GetVisiblePointLights(const View& view) const;
			PointLights GetVisiblePointLights() const;
			std::pair<Lights, Lights> GetVisibleLightsNoPoint() const;

			void AddLight(const std::shared_ptr<Light>& light);
			void RemoveLight(const std::shared_ptr<Light>& light);

			MaterialHandle GetGroundMaterial(const simd::vector3& pos);
			const std::shared_ptr<Light> GetAmbientLight();
			void RemoveIrradienceCube();

#if defined( ENVIRONMENT_EDITING_ENABLED ) || GGG_CHEATS == 1
			void DisableBreachClipping();
#endif
			void ResetBreachSpheres();
			void UpdateBreachSphere(unsigned index, simd::vector3 sphere_position, float sphere_radius );
			const simd::matrix& GetBreachInfo() const;

			void SetRoofFadeAmount(float fade) { roof_fade = fade; }

			void SetEnvironmentSettings(const Environment::Data& settings, const simd::matrix& root_transform, const Vector2d& player_position, const float player_height, const float colour_percent, const float radius_percent, const int light_radius_pluspercent, const simd::vector3* base_colour_override, const float far_plane_multiplier, const float custom_z_rotation);

			std::pair<float, float> GetLightDepthBiasAndSlopeScale(const Light& light) const;
			
			const simd::vector2 GetSceneSize();
			const simd::vector2_int GetSceneTilesCount();

			const float GetSceneMaxHeight();

			PostProcess::SynthesisMapInfo GetSynthesisMapInfo();

			Device::Handle<Device::Texture> GetDecayMapTexture() const { return synthesis_data.decay_data_texture; }
			Device::Handle<Device::Texture> GetStableMapTexture() const { return synthesis_data.stable_data_texture; }
			simd::vector2_int GetSynthesisMapSize() const { return synthesis_data.texture_size; }
			simd::vector2 GetSynthesisMapMinPoint() const { return synthesis_data.min_point; }
			simd::vector2 GetSynthesisMapMaxPoint() const { return synthesis_data.max_point; }
			void SetDecayTime(float _decay_time) { synthesis_data.decay_time = _decay_time; }
			float GetDecayTime() const { return synthesis_data.decay_time; }

			void SetCreationTime(float _creation_time) { synthesis_data.creation_time = _creation_time; }
			float GetCreationTime() const { return synthesis_data.creation_time; }
			void SetGlobalStability(float _global_stability) { synthesis_data.global_stability = _global_stability; }
			float GetGlobalStability() const { return synthesis_data.global_stability; }
			void SetSynthesisDecayType(int decay_type) { synthesis_data.decay_type = decay_type; }
			int GetSynthesisDecayType() { return synthesis_data.decay_type; }
			simd::vector4 GetPlayerPosition() const { return player_position; }
			simd::vector4 GetSynthesisStabiliserPosition() const { return simd::vector4( synthesis_data.stabiliser_position, 1.0f ); }
			void SetSynthesisStabiliserPosition( const simd::vector3& pos ) { synthesis_data.stabiliser_position = pos; }

			void SetFloodWaveTime( const float time ) { flood_wave_data.texture_size.z = time; }

			void SetAfflictionInnerRadius( float radius ) { affliction_data.radii.x = radius; }
			void SetAfflictionOuterRadius( float radius ) { affliction_data.radii.y = radius; }
			void SetAfflictionCentrePosition( const simd::vector3& pos ) { affliction_data.centre_pos = simd::vector4(pos.x, pos.y, pos.z, 0.0f); }
			void SetAfflictionFogIntensity( float intensity ) { affliction_data.radii.z = intensity; }
			simd::vector3 GetAfflictionCentrePosition() const { simd::vector4 v = affliction_data.centre_pos; return simd::vector3(v.x, v.y, v.z); }

			void SetTrackedKills( const float kills ) { tracked_kills = kills; }
			float GetTrackedKills() const { return tracked_kills; }

			// Ritual data
			void SetRitualEncounterData( const simd::vector4& sphere, float ratio );
			simd::vector4 GetRitualEncounterSphere() const;
			float GetRitualEncounterRatio() const;

			// Royale circle data for shaders
			void SetRoyaleCircleData( const simd::vector4& damage_circle, const simd::vector4& safe_circle ) { royale_data.damage_circle = damage_circle; royale_data.safe_circle = safe_circle; }
			std::pair< simd::vector4, simd::vector4 > GetRoyaleCircleData() const { return std::make_pair( royale_data.damage_circle, royale_data.safe_circle ); }

			void SetInfiniteHungerArenaData( float height, float velocity ) { infinite_hunger_arena_data.water_level = simd::vector4( height, velocity, 0.0f, 0.0f ); }

			// Lake mist data
			void SetLakeMistIntensity( float intensity ) { lake_mist_data.intensity = intensity; }

			void SetRoomOcclusionData( const RoomOcclusionData& data );

			void SetWaterDebug(bool water_debug) { this->water_debug = water_debug; }

			void SetSubScene(unsigned index, const BoundingBox& box, const simd::matrix& transform);
			const std::array<SubScene, SubSceneMaxCount>& GetSubScenes() const;
			simd::matrix GetSubSceneTransform(const BoundingBox& box) const;

			simd::color GetFinalFogColor();
			void DisableFog();
			void ResetFog();

			Device::UniformInputs BuildUniformInputs();
			Device::BindingInputs BuildBindingInputs();

			void SetVisibleLayers(uint8_t layers) { visible_layers = layers; }

			Visual::OrphanedEffectsManager& GetOrphanedEffectsManager() { return *orphaned_effects_manager.get(); } // [TODO] Remove.
			CubemapManager* GetCubemapManager() const { return cubemap_manager.get(); } // [TODO] Remove.
			ShadowManager& GetShadowManager() { return shadow_manager; } // [TODO] Remove.

		private:
			void SetShadowType( Settings::ShadowType _shadow_type ) { shadow_type = _shadow_type; }

			void SetDirectionalLightSettings(const Environment::Data& settings);
			void SetGlobalParticleEffectSettings(const Environment::Data& settings, float player_height);
			void SetPostProcessingSettings( const Environment::Data& settings);
			void SetEnvironmentMappingSettings( const Environment::Data& settings );
			void UpdateDynamicIrradienceProperties(const Environment::Data& settings);
			void SetFogSettings(const Environment::Data& fog_settings);
			void SetCameraSettings(const Environment::Data& settings, const float far_plane_multiplier, const float custom_z_rotation);
			void SetPlayerLightSettings(const Environment::Data& settings, const simd::matrix& root_transform, const Vector2d& player_position, const float player_height, const float colour_percent, const float radius_percent, const int light_radius_pluspercent, const simd::vector3* base_colour_override);

			void InitialiseSource( );
			void DeinitialiseSource( );

			void CreateTerrainHeightmapTexture();
			void CreateBlightTexture();

			void Cull(float delta_time, bool render_world, CubemapTask* cubemap_task);

			void CreateSynthesisMap(const SynthesisMapData& synthesis_map_data);
			void CreateFloodWaveMap(const WaterMapData& water_map_data);

			ShadowManager shadow_manager;

			uint8_t visible_layers = Layer::All;

#ifdef PROFILING
			bool force_disable_merging = false;
#endif

			float screen_width = 0.0f;
			float screen_height = 0.0f;

			SceneDataSource* scene_data_source = nullptr;
			Device::IDevice* device = nullptr;

			bool scene_ready = false;

			int ocean_data_id = -1;
			int river_data_id = -1;

			std::unique_ptr<Visual::OrphanedEffectsManager> orphaned_effects_manager;
			std::unique_ptr<CubemapManager> cubemap_manager;

			std::shared_ptr<DirectionalLight> directional_light;
			std::shared_ptr<PointLight> player_spotlight;

			TiledLighting tiled_lighting;

			Settings::ShadowType shadow_type;

			::Particles::ParticleEffectHandle global_particle_effect_handle;
			std::shared_ptr< ::Particles::EffectInstance > global_particle_effect;
			::Renderer::Scene::Camera* global_particle_effect_camera = nullptr;

			Renderer::DynamicParameterLocalStorage dynamic_parameters_storage;

			::Texture::Handle diffuse_tex_handle;
			::Texture::Handle specular_tex_handle;

			struct ScreenSpaceFogDrawData
			{
				simd::vector4 layercount_thickness_turbulence;
				simd::vector4 color;
				simd::vector4 disperse_radius_feathering;
			} screenspace_fog_data;

			bool diffuse_irradience_enabled;
			::Texture::Handle diffuse_irradience_cube;
			bool specular_irradience_enabled;
			simd::vector4 specular_irradience_size;
			::Texture::Handle specular_irradience_cube;
			simd::vector4 irradience_cube_brightness;

			simd::matrix env_map_rotation;
			float direct_light_env_ratio;

		#ifdef PROFILING
			float debug_overlay_strength = 0.7f;
		#endif

			unsigned frame_index = 0;
			float frame_time = 0.0f;
			float tool_time = -100.0f;

			simd::vector4 player_position;

			float exposure;

			bool burn_texture_needs_clear;
			simd::vector4 terrain_tex_size;
			simd::vector4 burn_tex_size;
			simd::vector4 terrain_world_size;
			::Texture::Handle noise_sampler;
			Device::Handle<Device::Texture> terrain_sampler;
			Device::Handle<Device::Texture> burn_sampler;

			simd::matrix breach_sphere_infos;

			float roof_fade = 1.f;

			Device::Handle<Device::Texture> terrain_data_texture;

			Device::Handle<Device::Texture> burn_intensity_texture;
			std::unique_ptr<Device::RenderTarget> burn_intensity_target;

			Device::Handle<Device::Texture> burn_intensity_texture_prev;
			std::unique_ptr<Device::RenderTarget> burn_intensity_target_prev;

			Device::Handle<Device::VertexBuffer> imagespace_vertices;
			Device::VertexDeclaration imagespace_vertex_declaration;

			::Texture::Handle distorton_texture;

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

		#pragma pack(push)	
		#pragma pack(1)
			struct PipelineKey
			{
				Device::PointerID<Device::Pass> pass;

				PipelineKey() {}
				PipelineKey(Device::Pass* pass);
			};
		#pragma pack(pop)
			Device::Cache<PipelineKey, Device::Pipeline> pipelines;

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

			struct BurnIntensityShader
			{
				Device::Handle<Device::Shader> vertex_shader;
				Device::Handle<Device::Shader> pixel_shader;

				Device::Handle<Device::DynamicUniformBuffer> vertex_uniform_buffer;
				Device::Handle<Device::DynamicUniformBuffer> pixel_uniform_buffer;
			} burn_intensity_shader;

			struct SynthesisMapDataInternal
			{
				Device::Handle<Device::Texture> decay_data_texture;
				Device::Handle<Device::Texture> stable_data_texture;
				simd::vector2_int texture_size;
				simd::vector2 min_point;
				simd::vector2 max_point;

				float decay_time = 0.0f;
				float creation_time = 0.0f;
				float global_stability = 0.0f;

				int decay_type = 0;

				simd::vector3 stabiliser_position;
			}synthesis_data;

			struct FloodWaveDataInternal
			{
				Device::Handle<Device::Texture> flood_wave_data_texture;
				simd::vector4 texture_size;
			}flood_wave_data;

			struct BlightMapData
			{
				Device::Handle<Device::Texture> blight_tex;
				simd::vector4 blight_tex_size;
			}blight_data;

			struct AfflictionData
			{
				simd::vector4 centre_pos;
				simd::vector4 radii;
			} affliction_data;

			struct RitualData
			{
				simd::vector4 sphere;
				float ratio = 0.0f;
			} ritual_data;

			struct LakeMistData
			{
				float intensity = 0.0f;
			} lake_mist_data;

			struct RoomOcclusionDataInternal
			{
				struct Room
				{
					float x1, y1, x2, y2;
					float opacity;
				};
				std::vector< Room > rooms;
			} room_occlusion_data;

			struct RoyaleData
			{
				simd::vector4 damage_circle;
				simd::vector4 safe_circle;
			} royale_data;

			struct InfiniteHungerArenaData
			{
				simd::vector4 water_level;
			} infinite_hunger_arena_data;

			float tracked_kills = 0.0f;

			bool water_debug = false;
		};
	}
}
