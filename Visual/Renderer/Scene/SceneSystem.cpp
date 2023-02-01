#include "Common/Utility/Profiler/ScopedProfiler.h"
#include "Common/Utility/LoadingScreen.h"
#include "Common/Job/JobSystem.h"
#include "Common/Geometry/Bounding.h"

#include "Visual/Animation/AnimationSystem.h"
#include "Visual/Entity/EntitySystem.h"
#include "Visual/Renderer/Scene/SceneDataSource.h"
#include "Visual/Renderer/Scene/View.h"
#include "Visual/Renderer/Scene/Camera.h"
#include "Visual/Renderer/Scene/Light.h"
#include "Visual/Renderer/DrawCalls/DrawCall.h"
#include "Visual/Renderer/TextureMapping.h"
#include "Visual/Renderer/CachedHLSLShader.h"
#include "Visual/Renderer/TargetResampler.h"
#include "Visual/Renderer/Utility.h"
#include "Visual/Renderer/ShaderSystem.h"
#include "Visual/Renderer/RendererSubsystem.h"
#include "Visual/Environment/EnvironmentSettings.h"
#include "Visual/Environment/PlayerSpotLight.h"
#include "Visual/Effects/EffectsManager.h"
#include "Visual/Device/RenderTarget.h"
#include "Visual/Device/VertexDeclaration.h"
#include "Visual/Device/Buffer.h"
#include "Visual/Particles/Particle.h"
#include "Visual/Particles/EffectInstance.h"
#include "Visual/Particles/ParticleEmitter.h"
#include "Visual/Trails/TrailsTrail.h"
#include "Visual/Trails/TrailsEffectInstance.h"
#include "Visual/Trails/TrailSystem.h"
#include "Visual/Particles/ParticleSystem.h"
#include "Visual/Device/Device.h"
#include "Visual/Device/State.h"
#include "Visual/Device/IndexBuffer.h"
#include "Visual/Device/VertexBuffer.h"
#include "Visual/Device/DynamicCulling.h"
#include "Visual/Device/DynamicResolution.h"
#include "Visual/Device/UniformBuffer.h"
#include "Visual/Profiler/JobProfile.h"
#include "Visual/Renderer/CubemapManager.h"
#include "Visual/Animation/Components/ClientAnimationControllerComponent.h"
#include "Visual/Renderer/ShadowManager.h"
#include "Visual/GpuParticles/GpuParticleSystem.h"
#include "Visual/Engine/Plugins/PerformancePlugin.h"
#include "Visual/LUT/LUTSystem.h"

#include "ClientInstanceCommon/Terrain/TileMap.h"

#include "SceneSystem.h"


namespace Renderer
{
	using namespace Lighting;

	namespace ConstData
	{
		const bool bool_false(false);
		const bool bool_true(true);
	}

	namespace Scene
	{
		static const unsigned ViewInfoMaxCount = 16 * SubSceneMaxCount;
		static const unsigned LightInfoMaxCount = 32 * SubSceneMaxCount;


		SubScene::SubScene(const BoundingBox& box, const simd::matrix& transform)
			: box(box), transform(transform)
		{}


		System::PassKey::PassKey(Device::ColorRenderTargets render_targets, Device::RenderTarget* depth_stencil)
			: render_targets(render_targets), depth_stencil(depth_stencil)
		{}

		System::PipelineKey::PipelineKey(Device::Pass* pass)
			: pass(pass)
		{}

		System::BindingSetKey::BindingSetKey(Device::Shader * shader, uint32_t inputs_hash)
			: shader(shader), inputs_hash(inputs_hash)
		{}

		System::DescriptorSetKey::DescriptorSetKey(Device::Pipeline* pipeline, Device::DynamicBindingSet* pixel_binding_set, uint32_t samplers_hash)
			: pipeline(pipeline), pixel_binding_set(pixel_binding_set), samplers_hash(samplers_hash)
		{}


	#pragma pack( push, 1 )
		struct ImagespaceVertex
		{
			simd::vector4 viewport_pos;
			simd::vector2 tex_pos;
		};
	#pragma pack( pop )

		const Device::VertexElements imagespace_vertex_elements =
		{
			{ 0, offsetof(ImagespaceVertex, viewport_pos), Device::DeclType::FLOAT4, Device::DeclUsage::POSITION, 0 },
			{ 0, offsetof(ImagespaceVertex, tex_pos), Device::DeclType::FLOAT2, Device::DeclUsage::TEXCOORD, 0 },
		};

		const ImagespaceVertex vertex_data[4] =
		{
			{ simd::vector4(-1.f, -1.f, 0.0f, 1.0f), simd::vector2(0.0f, 1.0f) },
			{ simd::vector4(-1.f, 1.f, 0.0f, 1.0f), simd::vector2(0.0f, 0.0f) },
			{ simd::vector4(1.f, -1.f, 0.0f, 1.0f), simd::vector2(1.0f, 1.0f) },
			{ simd::vector4(1.f, 1.f, 0.0f, 1.0f), simd::vector2(1.0f, 0.0f) }
		};

		class Impl
		{
			Camera* camera = nullptr;

			Settings::LightQuality light_quality = Settings::LightQuality::Low;

			Memory::Set<std::shared_ptr<Scene::Light>, Memory::Tag::Scene> lights;
			Memory::Vector<std::shared_ptr<Scene::Light>, Memory::Tag::Scene> garbage_lights;
			Memory::Mutex lights_mutex;

			PointLights visible_point_lights;
			Lights visible_lights;
			Lights visible_shadowed_lights;

			std::array<SubScene, SubSceneMaxCount> sub_scenes;

			std::array<simd::matrix, SubSceneMaxCount> sub_transform_inverses;
			std::array<ViewBounds, SubSceneMaxCount> camera_sub_bounds;
			std::array<BoundingVolume, SubSceneMaxCount> camera_sub_volumes;
			std::array<Memory::SmallVector<BoundingVolume, 4, Memory::Tag::Scene>, SubSceneMaxCount> visible_shadowed_light_sub_volumes;
			std::array<Memory::SmallVector<BoundingVolume, 64, Memory::Tag::Scene>, SubSceneMaxCount> visible_light_sub_volumes;

			Device::Handle<Device::StructuredBuffer> view_infos;
			Device::Handle<Device::StructuredBuffer> light_infos;

			Device::UniformBuffer uniform_buffer;

			PointLights GatherVisiblePointLights(const View& view) const
			{
				PointLights out_lights;
				for (auto& light : lights)
				{
					if (out_lights.size() < PointLightDataMaxCount)
					{
						if (light->GetType() == Scene::Light::Type::Point &&
							!light->GetShadowed() &&
							light->GetLightFrequency() <= (unsigned char)light_quality &&
							simd::vector3(light->GetColor().x, light->GetColor().y, light->GetColor().z).sqrlen() > FLT_EPSILON &&
							view.Compare(light->GetBoundingBox()) != 0) // [TODO] Use sub-scene.
						{
							out_lights.emplace_back(static_cast<PointLight*>(light.get()));
						}
					}
				}
				return out_lights;
			}

			PointLights GatherVisiblePointLights() const
			{
				PointLights out_lights;
				for (auto& light : lights)
				{
					if (out_lights.size() < PointLightDataMaxCount)
					{
						if (light->GetType() == Scene::Light::Type::Point &&
							!light->GetShadowed() &&
							light->GetLightFrequency() <= (unsigned char)light_quality &&
							simd::vector3(light->GetColor().x, light->GetColor().y, light->GetColor().z).sqrlen() > FLT_EPSILON)
						{
							if (camera_sub_volumes[GetSubSceneIndex(light->GetBoundingBox())].Compare(light->GetBoundingBox()) != 0)
								out_lights.emplace_back(static_cast<PointLight*>(light.get()));
						}
					}
				}
				return out_lights;
			}

			std::pair<Lights, Lights> GatherVisibleLightsNoPoint() const
			{
				Lights normal_lights;
				Lights high_priority_lights;

				for (auto& light : lights)
				{
					if (light->GetType() == Scene::Light::Type::Point && light->GetShadowed())
						high_priority_lights.emplace_back(light.get());

					if (light->GetType() == Scene::Light::Type::Point ||
						simd::vector3(light->GetColor().x, light->GetColor().y, light->GetColor().z).sqrlen() <= FLT_EPSILON)
						continue;

					if (camera_sub_volumes[GetSubSceneIndex(light->GetBoundingBox())].Compare(light->GetBoundingBox()) == 0)
						continue;

					if (light->GetType() == Light::Type::Directional || light->GetType() == Light::Type::DirectionalLimited)
						high_priority_lights.emplace_back(light.get());
					else
						normal_lights.emplace_back(light.get());
				}

				Lights out_lights;
				out_lights.append(high_priority_lights.begin(), high_priority_lights.end());
				out_lights.append(normal_lights.begin(), normal_lights.end());

				Lights out_shadow_lights;
				for (auto& light : out_lights)
				{
					if(light->GetShadowed())
						out_shadow_lights.emplace_back(light);
				}

				// Sort lights from camera position to avoid random light selection when more then 4 shadow casting lights.
				const BoundingBox bounding_box(camera->GetPosition() - 1.0f, camera->GetPosition() + 1.0f);
				std::stable_sort(out_shadow_lights.begin(), out_shadow_lights.end(), [&](const auto* a, const auto* b) -> bool
				{
					const auto ta = a->GetType(), tb = b->GetType();
					if (ta == tb)
					{
						const float ia = a->Importance(bounding_box), ib = b->Importance(bounding_box);
						return ia > ib;
					}
					return ta > tb; // Keep directional lights first.
				});
				out_shadow_lights.resize(std::min((size_t)4u, out_shadow_lights.size()));

				return { out_lights, out_shadow_lights };
			}

			void UpdateCameraTransient()
			{
				for (unsigned i = 0; i < SubSceneMaxCount; ++i)
				{
					sub_transform_inverses[i] = sub_scenes[i].GetTransform().inverse();
					camera_sub_bounds[i] = camera->BuildViewBounds(sub_scenes[i].GetTransform());
					camera_sub_volumes[i] = camera->BuildBoundingVolume(sub_scenes[i].GetTransform());
				}
			}

			void UpdateLightTransient()
			{
				for (unsigned i = 0; i < SubSceneMaxCount; ++i)
				{
					visible_shadowed_light_sub_volumes[i].clear();
					visible_light_sub_volumes[i].clear();
				}

				for (auto& light : visible_shadowed_lights)
				{
					const auto light_sub_scene_transform = Scene::System::Get().GetSubSceneTransform(light->GetBoundingBox());

					for (unsigned i = 0; i < SubSceneMaxCount; ++i)
					{
						const auto transform = sub_transform_inverses[i] * light_sub_scene_transform;
						visible_shadowed_light_sub_volumes[i].emplace_back(light->BuildBoundingVolume(transform, true));
					}
				}

				for (auto& light : visible_lights)
				{
					const auto light_sub_scene_transform = Scene::System::Get().GetSubSceneTransform(light->GetBoundingBox());

					for (unsigned i = 0; i < SubSceneMaxCount; ++i)
					{
						const auto transform = sub_transform_inverses[i] * light_sub_scene_transform;
						visible_light_sub_volumes[i].emplace_back(light->BuildBoundingVolume(transform, false));
					}
				}
			}

			void UpdateViewInfos(CubemapTask* cubemap_task)
			{
				ViewInfo* infos = nullptr;
				view_infos->Lock(0, ViewInfoMaxCount * sizeof(ViewInfo), (void**)&infos, Device::Lock::DISCARD);

				if (infos)
				{
					unsigned index = 0;

					static View null_view(View::Type::Frustum);

					const auto update_view = [&](auto& view)
					{
						if ((index + SubSceneMaxCount) < ViewInfoMaxCount)
						{
							view.SetStartIndex(index);

							for (unsigned i = 0; i < SubSceneMaxCount; ++i)
							{
								infos[index + i] = view.BuildViewInfo(sub_scenes[i].GetTransform());
							}

							index += SubSceneMaxCount;
						}
						else
						{
							view.SetStartIndex(0); // Use null view when too many.
						}
					};

					update_view(null_view); // Null view always first.
					update_view(*camera);

					for (auto& shadowed_light : visible_shadowed_lights)
						update_view(*shadowed_light);

					if (cubemap_task)
						for( size_t i = 0; i < std::min(CubemapTask::FACE_COUNT, (size_t)Entity::ViewType::FaceMax); i++)
							update_view(cubemap_task->face_cameras[i]);
				}

				view_infos->Unlock();
			}

			void UpdateLightInfos()
			{
				LightInfo* infos = nullptr;
				light_infos->Lock(0, LightInfoMaxCount * sizeof(LightInfo), (void**)&infos, Device::Lock::DISCARD);

				if (infos)
				{
					int index = 0;

					static NullLight null_light;

					const auto update_light = [&](auto& light)
					{
						if (index < LightInfoMaxCount)
						{
							light.SetIndex(index);

							for (unsigned i = 0; i < SubSceneMaxCount; ++i)
							{
								infos[index + i] = light.BuildLightInfo(sub_scenes[i].GetTransform());
							}

							index += SubSceneMaxCount;
						}
						else
						{
							light.SetIndex(0); // Use null light when too many.
						}
					};

					update_light(null_light); // Null light always first.

					for (auto* light : visible_lights)
						update_light(*light);
				}

				light_infos->Unlock();
			}

		public:
			void Init(bool tight_buffers)
			{
				uniform_buffer.Init(tight_buffers);
			}

			void OnCreateDevice(Device::IDevice *_device)
			{
				view_infos = Device::StructuredBuffer::Create("View Infos", _device, sizeof(ViewInfo), ViewInfoMaxCount, Device::FrameUsageHint(), Device::Pool::MANAGED, nullptr, false);
				light_infos = Device::StructuredBuffer::Create("Light Infos", _device, sizeof(LightInfo), LightInfoMaxCount, Device::FrameUsageHint(), Device::Pool::MANAGED, nullptr, false);

				uniform_buffer.OnCreateDevice(_device);
			}

			void OnDestroyDevice()
			{
				garbage_lights.clear();

				view_infos.Reset();
				light_infos.Reset();

				uniform_buffer.OnDestroyDevice();
			}

			void UpdateStats(Stats& stats) const
			{
				{
					Memory::Lock lock(lights_mutex);
					stats.light_count = (unsigned)lights.size();
					for (auto& light : lights)
					{
						if (light->GetType() == Light::Type::Point)
						{
							stats.point_light_count++;
							if (light->GetShadowed())
								stats.point_light_shadow_count++;
						}
						else if (light->GetType() == Light::Type::Spot)
						{
							stats.spot_light_count++;
							if (light->GetShadowed())
								stats.spot_light_shadow_count++;
						}
						else // Dir.
						{
							stats.dir_light_count++;
							if (light->GetShadowed())
								stats.dir_light_shadow_count++;
						}
					}
				}

				stats.visible_point_light_count = (unsigned)visible_point_lights.size();
				stats.visible_non_point_light_count = (unsigned)visible_lights.size();
				stats.visible_non_point_shadow_light_count = (unsigned)visible_shadowed_lights.size();

				uniform_buffer.UpdateStats(stats);
			}

			void FrameRenderBegin()
			{
				uniform_buffer.Lock();
			}

			void FrameRenderEnd()
			{
				uniform_buffer.Unlock();
			}

			Device::UniformAlloc AllocateUniforms(size_t size)
			{
				return uniform_buffer.Allocate(size);
			}

			void SetCamera(Camera* camera)
			{
				static DummyCamera dummy_camera; // So that we always have a camera for the render graph to work with.
				this->camera = camera ? camera : &dummy_camera;
				Renderer::System::Get().SetPostProcessCamera(this->camera);
			}

			const Camera* GetCamera() const { return camera; }
			Camera* GetCamera() { return camera; }

			void AddLight(const std::shared_ptr<Light>& light)
			{
				if (light)
				{
					Memory::Lock lock(lights_mutex);
					lights.insert( light );
				}
			}

			void RemoveLight(const std::shared_ptr<Light>& light)
			{
				if (light)
				{
					Memory::Lock lock(lights_mutex);
					garbage_lights.push_back(light);
					lights.erase(light);
				}
			}

			void SetLightQuality(Settings::LightQuality light_quality) { this->light_quality = light_quality; }

			void UpdateLights(ShadowManager& shadow_manager, CubemapTask* cubemap_task)
			{
				Memory::Lock lock(lights_mutex);

				garbage_lights.clear();

				visible_point_lights.clear();
				visible_lights.clear();
				visible_shadowed_lights.clear();

				if (camera)
				{

					for (auto& light : lights)
					{
						light->UpdateLightMatrix(*camera, shadow_manager.GetShadowAtlasCellSize());
						light->SetShadowAtlasOffsetScale(simd::vector4(0));
					}

					UpdateCameraTransient(); // Before light visibility.

					visible_point_lights = GatherVisiblePointLights();
					std::tie(visible_lights, visible_shadowed_lights) = GatherVisibleLightsNoPoint();

					UpdateLightTransient(); // After light visibility.

					for (uint32_t i = 0; i < visible_shadowed_lights.size(); i++)
					{
						visible_shadowed_lights[i]->SetShadowAtlasOffsetScale(shadow_manager.GetShadowAtlasOffsetScale(i));
					}

					UpdateViewInfos(cubemap_task);
					UpdateLightInfos();
				}
			}

			const std::shared_ptr<Light> GetAmbientLight()
			{
				Memory::Lock lock(lights_mutex);
				for(auto & it : lights)
				{
					if(it->GetType() == Light::Type::Directional)
						return it;
				}
				return nullptr;
			}

			const PointLights& GetVisiblePointLights() const { return visible_point_lights; }
			const Lights& GetVisibleLights() const { return visible_lights; }
			const Lights& GetVisibleShadowedLights() const { return visible_shadowed_lights; }

			PointLights GetVisiblePointLights(const View& view) const
			{
				Memory::Lock lock(lights_mutex);
				return GatherVisiblePointLights(view);
			}

			void AddBindingInputs(Device::BindingInputs& binding_inputs)
			{
				binding_inputs.push_back(Device::BindingInput(Device::IdHash(DrawDataNames::ViewInfos)).SetStructuredBuffer(view_infos));
				binding_inputs.push_back(Device::BindingInput(Device::IdHash(DrawDataNames::LightInfos)).SetStructuredBuffer(light_infos));
				uniform_buffer.AddBindingInputs(binding_inputs);
			}

			unsigned GetSubSceneIndex(const BoundingBox& box) const
			{
				for (unsigned i = 0; i < SubSceneMaxCount; ++i)
				{
					if (box.FastIntersect(sub_scenes[i].GetBox()))
						return i;
				}
				assert("Object not within any sub-scenes bounds. Assigning sub-scene 0 by default.");
				return 0;
			}

			void SetSubScene(unsigned index, const BoundingBox& box, const simd::matrix& transform)
			{
				if (index >= SubSceneMaxCount)
				{
					LOG_DEBUG(L"[SCENE] Sub-scene index too big");
					return;
				}

				sub_scenes[index] = SubScene(box, transform);
			}

			const std::array<SubScene, SubSceneMaxCount>& GetSubScenes() const { return sub_scenes; }

			simd::matrix GetSubSceneTransform(const BoundingBox& box) const { return sub_scenes[GetSubSceneIndex(box)].GetTransform(); }

			const simd::matrix& GetSubTransformInverse(unsigned index) const { return sub_transform_inverses[index]; }
			const ViewBounds& GetCameraSubBounds(unsigned index) const { return camera_sub_bounds[index]; }
			const Memory::SmallVector<BoundingVolume, 4, Memory::Tag::Scene>& GetVisibleShadowedLightSubVolumes(unsigned index) const { return visible_shadowed_light_sub_volumes[index]; }
			const Memory::SmallVector<BoundingVolume, 64, Memory::Tag::Scene>& GetVisibleLightSubVolumes(unsigned index) const { return visible_light_sub_volumes[index]; }
		};


		System& System::Get()
		{
			static System instance;
			return instance;
		}

		System::System()
			: ImplSystem()
			, imagespace_vertex_declaration(imagespace_vertex_elements)
		{
			orphaned_effects_manager = std::make_unique<Visual::OrphanedEffectsManager>();
			if (CubemapManager::Enabled()) cubemap_manager = std::make_unique<CubemapManager>();

			player_position = simd::vector4(0.0f, 0.0f, 0.0f, 0.0f);
			ResetBreachSpheres();

			SetCamera(nullptr); // Init with DummyCamera.
		}

		void System::Init(const Settings& settings, bool tight_buffers)
		{
			SetRendererSettings(settings);

			impl->Init(tight_buffers);
		}

		void System::Swap()
		{
		}

		void System::Clear()
		{
			assert(!scene_ready);
			SetSceneDataSource(NULL);

			global_particle_effect_handle = Particles::ParticleEffectHandle();
			global_particle_effect.reset();
			global_particle_effect_camera = nullptr;

			dynamic_parameters_storage.Clear();
		}

		Stats System::GetStats() const
		{
			Stats stats;
			stats.visible_layers = visible_layers;
			impl->UpdateStats(stats);
			return stats;
		}

		void System::ResetFrameTime()
		{
			frame_time = 0.f;
			tool_time = -100.0f;
		}

		void System::ResetToolTime()
		{
			tool_time = frame_time;
		}

		void System::OnCreateDevice(Device::IDevice *_device)
		{
			impl->OnCreateDevice(_device);

			Device::MemoryDesc init_mem = { vertex_data, 0, 0 };
			imagespace_vertices = Device::VertexBuffer::Create("VB ImageSpace", _device, 4 * sizeof(ImagespaceVertex), Device::UsageHint::IMMUTABLE, Device::Pool::DEFAULT, &init_mem);

			burn_intensity_shader.vertex_shader = Renderer::CreateCachedHLSLAndLoad(_device, "Imagespace_Vertex", Renderer::LoadShaderSource(L"Shaders/FullScreenQuad_VertexShader.hlsl"), nullptr, "VShad", Device::VERTEX_SHADER);
			burn_intensity_shader.pixel_shader = Renderer::CreateCachedHLSLAndLoad(_device, "BurnIntensity", Renderer::LoadShaderSource(L"Shaders/TerrainBurn.hlsl"), nullptr, "ComputeBurnIntensity", Device::PIXEL_SHADER);

			burn_intensity_shader.vertex_uniform_buffer = Device::DynamicUniformBuffer::Create("BurnIntensity", _device, burn_intensity_shader.vertex_shader.Get());
			burn_intensity_shader.pixel_uniform_buffer = Device::DynamicUniformBuffer::Create("BurnIntensity", _device, burn_intensity_shader.pixel_shader.Get());

			tiled_lighting.OnCreateDevice(_device);
			if (cubemap_manager) cubemap_manager->OnCreateDevice(_device);
			shadow_manager.OnCreateDevice(_device);

		}

		void System::OnDestroyDevice()
		{
			impl->OnDestroyDevice();

			tiled_lighting.OnDestroyDevice();
			if (cubemap_manager) cubemap_manager->OnDestroyDevice();
			shadow_manager.OnDestroyDevice();

			terrain_data_texture.Reset();
			burn_intensity_texture.Reset();
			burn_intensity_texture_prev.Reset();
			terrain_sampler.Reset();
			burn_sampler.Reset();
			noise_sampler = ::Texture::Handle();
			burn_intensity_shader.vertex_shader.Reset();
			burn_intensity_shader.vertex_uniform_buffer.Reset();
			burn_intensity_shader.pixel_shader.Reset();
			burn_intensity_shader.pixel_uniform_buffer.Reset();

			imagespace_vertices.Reset();
		}

		void System::OnResetDevice(Device::IDevice *_device)
		{
			device = _device;
			distorton_texture = ::Texture::Handle(::Texture::LinearTextureDesc(L"Art/particles/distortion/muddle_double.dds"));

			InitialiseSource();

			tiled_lighting.OnResetDevice(device);
			if (cubemap_manager) cubemap_manager->OnResetDevice(device);
			shadow_manager.OnResetDevice(device, shadow_type == Settings::ShadowType::HardwareShadowMappingHigh);

			screen_width = float(device->GetBackBufferWidth());
			screen_height = float(device->GetBackBufferHeight());
			//TODO: The back buffer size might be larger than the actual screen size, especially in case of borderless fullscreen!
		}

		void System::RestartDataSource()
		{
			PROFILE;
			DeinitialiseSource();
			InitialiseSource();
		}

		void System::OnLostDevice()
		{
			tiled_lighting.OnLostDevice();
			if (cubemap_manager) cubemap_manager->OnLostDevice();
			shadow_manager.OnLostDevice();

			descriptor_sets.Clear();
			binding_sets.Clear();
			pipelines.Clear();
			passes.Clear();

			distorton_texture = {};
			DeinitialiseSource();
			device = NULL;
		}

		void System::SetRendererSettings(const Settings& settings)
		{
			impl->SetLightQuality(settings.light_quality);
			SetShadowType(settings.shadow_type);

			Device::DynamicResolution::Get().Enable(settings.use_dynamic_resolution);
			Device::DynamicResolution::Get().SetTargetFps(settings.target_fps);
			Device::DynamicResolution::Get().SetFactor(settings.dynamic_resolution_factor_enabled, settings.dynamic_resolution_factor);

			Device::DynamicCulling::Get().Enable(settings.use_dynamic_particle_culling);
			Device::DynamicCulling::Get().SetTargetFps(settings.target_fps);
		}

		void System::SetSubScene(unsigned index, const BoundingBox& box, const simd::matrix& transform)
		{
			impl->SetSubScene(index, box, transform);
		}

		const std::array<SubScene, SubSceneMaxCount>& System::GetSubScenes() const
		{
			return impl->GetSubScenes();
		}

		simd::matrix System::GetSubSceneTransform(const BoundingBox& box) const
		{
			return impl->GetSubSceneTransform(box);
		}

		void System::ComputeTiles(Device::CommandBuffer& command_buffer, std::shared_ptr<CubemapTask> cubemap_task)
		{
			if (auto* camera = GetCamera())
			{
				tiled_lighting.ComputeTiles(device, *camera, command_buffer);
				if (cubemap_manager) cubemap_manager->ComputeTiles(command_buffer, cubemap_task.get());
			}
		}

		void System::FrameMoveBegin(float delta_time, bool render_world)
		{
			PROFILE;

			frame_time = frame_time + delta_time;
			frame_index++;

			CubemapTask* cubemap_task = nullptr;
			if (cubemap_manager)
				cubemap_task = cubemap_manager->Update();

			impl->UpdateLights(shadow_manager, cubemap_task);

			Job2::System::Get().Add(Job2::Type::High, { Memory::Tag::Scene, Job2::Profile::TileLighting, [=]() {
				tiled_lighting.Update(GetDevice(), impl->GetVisiblePointLights());
			}});

			Job2::System::Get().Add(Job2::Type::High, { Memory::Tag::Scene, Job2::Profile::Entities, [=]() {
				Cull(delta_time, render_world, cubemap_task);
			}});
		}

		void System::FrameMoveEnd(float delta_time)
		{
			// Avoid accumulating transfer requests while game is minimized
			if (device->RenderingPaused())
			{
				device->ResourceFlush();
			}
		}

		void System::FrameRenderBegin() { impl->FrameRenderBegin(); }
		void System::FrameRenderEnd() { impl->FrameRenderEnd(); }

		Device::UniformAlloc System::AllocateUniforms(size_t size) { return impl->AllocateUniforms(size); }

		void System::UpdateBurnTexture(float dt)
		{
			if (!burn_intensity_target)
				return;

			const size_t max_sources_count = 10;
			Physics::FireSource fire_sources[max_sources_count];
			
			size_t curr_sources_count = max_sources_count;
			Physics::System::Get().GetWindSystem()->GetFireSources(fire_sources, curr_sources_count);
			int start_count = static_cast< int >( curr_sources_count );

			auto& command_buffer = *device->GetCurrentUICommandBuffer();

			Renderer::System::Get().GetResampler().ResampleColor(command_buffer, burn_intensity_target.get(), burn_intensity_target_prev.get());

			auto* pass = passes.FindOrCreate(PassKey({ burn_intensity_target.get() }, nullptr), [&]()
			{
				return Device::Pass::Create("Burn Intensity", device, { burn_intensity_target.get() }, nullptr, Device::ClearTarget::NONE, simd::color(0), 0.0f);
			}).Get();
			command_buffer.BeginPass(pass, burn_intensity_target->GetSize(), burn_intensity_target->GetSize());

			simd::vector4 fire_sources_data[max_sources_count];
			for (size_t source_index = 0; source_index < max_sources_count; source_index++)
			{
				fire_sources_data[source_index] = simd::vector4(0.0f, 0.0f, 0.0f, 0.0f);
			}
			curr_sources_count = std::min(curr_sources_count, max_sources_count);
			for (int source_index = 0; source_index < curr_sources_count; source_index++)
			{
				fire_sources_data[source_index].x = fire_sources[source_index].pos.x;
				fire_sources_data[source_index].y = fire_sources[source_index].pos.y;
				fire_sources_data[source_index].z = fire_sources[source_index].radius;
				fire_sources_data[source_index].w = fire_sources[source_index].intensity;
			}
			burn_intensity_shader.pixel_uniform_buffer->SetInt("fire_sources_count", static_cast< int >( curr_sources_count));
			burn_intensity_shader.pixel_uniform_buffer->SetVectorArray("fire_sources_data", fire_sources_data, static_cast< unsigned >( curr_sources_count));
			burn_intensity_shader.pixel_uniform_buffer->SetFloat("time_step", dt);
			burn_intensity_shader.pixel_uniform_buffer->SetFloat("reset_data", burn_texture_needs_clear ? 1.0f : 0.0f);
			burn_texture_needs_clear = false;
			simd::vector4 tex_size = simd::vector4( static_cast< float >( burn_intensity_target->GetWidth() ), static_cast< float >( burn_intensity_target->GetHeight() ), 0.0f, 0.0f);
			burn_intensity_shader.pixel_uniform_buffer->SetVector("tex_size", &tex_size);
			simd::vector4 scene_size = simd::vector4((float)GetSceneSize().x, (float)GetSceneSize().y, 0.0f, 0.0f);
			burn_intensity_shader.pixel_uniform_buffer->SetVector("scene_size", &scene_size);

			auto* pipeline = pipelines.FindOrCreate(PipelineKey(pass), [&]()
			{
				return Device::Pipeline::Create("Burn Intensity", device, pass, Device::PrimitiveType::TRIANGLESTRIP, &imagespace_vertex_declaration, burn_intensity_shader.vertex_shader.Get(), burn_intensity_shader.pixel_shader.Get(),
					Device::DisableBlendState(), Device::RasterizerState(Device::CullMode::NONE, Device::FillMode::SOLID, 0, 0), Device::DisableDepthStencilState());
			}).Get();
			if (command_buffer.BindPipeline(pipeline))
			{
				Device::DynamicBindingSet::Inputs inputs;
				inputs.push_back({ "prev_burn_sampler", burn_intensity_texture_prev.Get() });
				auto* pixel_binding_set = binding_sets.FindOrCreate(BindingSetKey(burn_intensity_shader.pixel_shader.Get(), inputs.Hash()), [&]()
				{
					return Device::DynamicBindingSet::Create("Burn Intensity", device, burn_intensity_shader.pixel_shader.Get(), inputs);
				}).Get();

				auto* descriptor_set = descriptor_sets.FindOrCreate(DescriptorSetKey(pipeline, pixel_binding_set, device->GetSamplersHash()), [&]()
				{
					return Device::DescriptorSet::Create("Burn Intensity", device, pipeline, {}, { pixel_binding_set });
				}).Get();

				command_buffer.BindDescriptorSet(descriptor_set, {}, { burn_intensity_shader.vertex_uniform_buffer.Get(), burn_intensity_shader.pixel_uniform_buffer.Get() });

				command_buffer.BindBuffers(nullptr, imagespace_vertices.Get(), 0, sizeof(ImagespaceVertex));
				command_buffer.Draw(4, 0);
			}

			command_buffer.EndPass();

			Physics::System::Get().GetWindSystem()->ResetFireSources();
		}

		std::pair<float, float> System::GetLightDepthBiasAndSlopeScale(const Light& light) const
		{
			auto depth_bias = light.GetType() == Scene::Light::Type::Directional || light.GetType() == Scene::Light::Type::DirectionalLimited ? 0.0003f : 0.0f;
		#ifdef _XBOX_ONE
			depth_bias *= pow(2, 16) / pow(2, 24);
		#endif
			const auto slope_scale = 4.0f;
			return { depth_bias, slope_scale };
		}

		Entity::Lights CullEntityLights(const BoundingBox& bounding_box, const Scene::Lights& all_lights, 
			const Memory::SmallVector<BoundingVolume, 64, Memory::Tag::Scene>& all_light_sub_volumes)
		{
			Scene::Lights sort_lights;

			assert(all_lights.size() == all_light_sub_volumes.size());
			for (unsigned i = 0; i < all_lights.size(); ++i)
			{
				if (all_light_sub_volumes[i].Compare(bounding_box) > 0)
					sort_lights.push_back(all_lights[i]);
			}

			const unsigned light_count = (unsigned)sort_lights.size();
			
			std::sort(sort_lights.begin(), sort_lights.end(), [&](const Scene::Light* a, const Scene::Light* b) -> bool
			{
				const float ia = a->Importance(bounding_box), ib = b->Importance(bounding_box);
				return ia > ib;
			});

			sort_lights.resize(std::min((size_t)4u, sort_lights.size()));

			std::sort(sort_lights.begin(), sort_lights.end(), [](const Scene::Light* a, const Scene::Light* b) -> bool
			{
				const auto ta = a->GetType(), tb = b->GetType();
				if (ta == tb)
					return a->GetShadowed() > b->GetShadowed(); //Swap order of shadow / non shadowed so shadowed is first
				return ta < tb;
			});

			Entity::Lights out_lights;
			for (auto* light : sort_lights)
				out_lights.push_back(light);
			return out_lights;
		}

		Entity::Visibility ComputeVisibility(const BoundingBox& bounding_box, const Frustum& view_frustum,
			const Memory::SmallVector<BoundingVolume, 4, Memory::Tag::Scene>& visible_shadowed_lights_sub_volumes, CubemapTask* cubemap_task)
		{
			Entity::Visibility visibility;

			visibility.set((unsigned)Entity::ViewType::Camera, view_frustum.TestBoundingBox(bounding_box) > 0);

			for( size_t i = 0; i < std::min(visible_shadowed_lights_sub_volumes.size(), (size_t)Entity::ViewType::LightMax); ++i)
				visibility.set((size_t)Entity::ViewType::Light0 + i, visible_shadowed_lights_sub_volumes[i].Compare(bounding_box) > 0);

			if (cubemap_task)
				for( size_t i = 0; i < std::min(CubemapTask::FACE_COUNT, (size_t)Entity::ViewType::FaceMax); i++)
					visibility.set((size_t)Entity::ViewType::Face0 + i, cubemap_task->face_cameras[i].Compare(bounding_box) > 0);

			return visibility;
		}

		float ComputeOnScreenSize(const BoundingBox& bounding_box, float tan_fov2, const simd::vector3& camera_position, float screen_height)
		{
			if (tan_fov2 < 1e-5f)
				return screen_height;

			const auto distance = (((bounding_box.maximum_point + bounding_box.minimum_point) * 0.5f) - camera_position).len();
			const auto radius = (bounding_box.maximum_point - bounding_box.minimum_point).len() * 0.5f;

			const float ratio = radius / distance;
			if (ratio > 1.0f - 1e-5f)
				return FLT_MAX; // Bounding box larger than screen

			const float view_height = ratio / std::sqrt(1.0f - (ratio * ratio)); // = tan(asin(ratio))
			const float view_ratio = std::min(2.0f * view_height / tan_fov2, 1.0f); // Taking ratio times two to account for Nyquist Rate issue
			return view_ratio * screen_height;
		}

		void System::Cull(float delta_time, bool render_world, CubemapTask* cubemap_task)
		{
			PROFILE;

			if (LoadingScreen::IsActive())
			{
				orphaned_effects_manager->Clear();
			}
			else
			{
				orphaned_effects_manager->GarbageCollect();
				orphaned_effects_manager->Update();
			}

			Device::DynamicCulling::Get().Reset();

			if (!render_world)
				return;

			auto update_func = [=](auto& templ, auto& entity)
			{
				if (entity.emitter)
					entity.bounding_box.Merge(entity.emitter->FrameMove(delta_time));
				if (entity.trail)
					entity.bounding_box.Merge(entity.trail->FrameMove(delta_time));

				const auto scene_index = impl->GetSubSceneIndex(entity.bounding_box);
				const auto view_bounds = impl->GetCameraSubBounds(scene_index);
				const auto visibility = (entity.layers & visible_layers) && entity.visible ? ComputeVisibility(entity.bounding_box, view_bounds.frustum, impl->GetVisibleShadowedLightSubVolumes(scene_index), cubemap_task) : 0;

				if (visibility.none())
					return Entity::Visibility();

				const auto type = templ.GetType();
				if ((type == DrawCalls::Type::Particle) || (type == DrawCalls::Type::Trail))
					if (!visibility.test((unsigned)Entity::ViewType::Camera))
						return Entity::Visibility();

				Entity::Lights lights;
				if (visibility.test((unsigned)Entity::ViewType::Camera))
				{
					lights = CullEntityLights(entity.bounding_box, impl->GetVisibleLights(), impl->GetVisibleLightSubVolumes(scene_index));

					if (const auto id = entity.draw_call->GetGpuEmitter())
						GpuParticles::System::Get().SetDrawCallVisible(id);
				}

			#ifdef PROFILING
				if (auto plugin = Engine::PluginManager::Get().GetPlugin<Engine::Plugins::PerformancePlugin>(); plugin && plugin->IsCulled(type))
					return Entity::Visibility();
			#endif

				const auto on_screen_size = ComputeOnScreenSize(entity.bounding_box, view_bounds.tan_fov2, view_bounds.position, screen_height);
				if (on_screen_size > 0.0f && 
					(type != DrawCalls::Type::GpuParticle &&
					type != DrawCalls::Type::Trail &&
					type != DrawCalls::Type::Doodad))
				{
					for (const auto& a : templ.GetMergedBindingInputs())
					{
						if (a.type == Device::BindingInput::Type::Texture && !a.texture_handle.IsNull())
							a.texture_handle.SetOnScreenSize(on_screen_size);
					}
				}

				auto* draw_call = entity.draw_call.get();
				draw_call->Update(lights, templ.GetBlendMode() >= DrawCalls::BlendModeExtents::BEGIN_COLOUR, scene_index);

				if (type == DrawCalls::Type::Particle)
				{
					const Particles::ParticleEmitter* particles[] = { draw_call->GetEmitter() };
					const auto draw = Particles::System::Get().Merge(particles, impl->GetCamera()->GetPosition()); // [TODO] Use proper sub-scene position.
					draw_call->SetBuffers(draw);
				}
				else if (type == DrawCalls::Type::Trail)
				{
					const Trails::Trail* trails[] = { draw_call->GetTrail() };
					const auto draw = Trails::System::Get().Merge(trails);
					draw_call->SetVertexCount(draw);
				}

				return visibility;
			};

			Entity::System::Get().Update(update_func);
		}

		void System::SetCamera(Camera* camera) { impl->SetCamera(camera); }
		const Camera* System::GetCamera() const { return impl->GetCamera(); }
		Camera* System::GetCamera() { return impl->GetCamera(); }

		void System::SetPlayerPosition(simd::vector3 pos)
		{
			this->player_position = simd::vector4(pos.x, pos.y, pos.z, 1.0f);
		}

		void System::SetSceneDataSource( SceneDataSource* _scene_data_source )
		{
			PROFILE;

			PROFILE_ONLY(Memory::StackTag tag(Memory::Tag::Scene));

			DeinitialiseSource();
			ResetFrameTime();
			ResetBreachSpheres();

			scene_data_source = _scene_data_source;

			InitialiseSource( );
		}

		void System::CreateTerrainHeightmapTexture()
		{
			PROFILE;

			int terrain_nodes_per_tile = 5;

			int terrain_texture_width = GetSceneTilesCount().x * terrain_nodes_per_tile;
			int terrain_texture_height = GetSceneTilesCount().y * terrain_nodes_per_tile;

			terrain_data_texture = Device::Texture::CreateTexture("Terrain Data", device,
				terrain_texture_width,
				terrain_texture_height,
				0, Device::UsageHint::DEFAULT, Device::PixelFormat::G16R16F, Device::Pool::MANAGED_WITH_SYSTEMMEM, false, false, true);

			std::vector<Device::LockedRect> rects;
			std::vector<TexUtil::LockedData<simd::vector2_half> > locked_datum;

			size_t curr_mip_level = 0;
			size_t curr_mip_width = terrain_texture_width;
			size_t curr_mip_height = terrain_texture_height;
			size_t last_mip_width = curr_mip_width;
			size_t last_mip_height = curr_mip_height;
			while ((curr_mip_width > 0) && (curr_mip_height > 0))
			{
				rects.push_back(Device::LockedRect());
				terrain_data_texture->LockRect( static_cast< UINT >( curr_mip_level ), &rects.back(), Device::Lock::DEFAULT);
				locked_datum.push_back(TexUtil::LockedData<simd::vector2_half>(rects.back().pBits, rects.back().Pitch));

				if (rects.back().pBits)
				{
					if (curr_mip_level == 0)
					{
						float min = 1e5f;
						float max = -1e5f;
						for (int y = 0; y < curr_mip_height; y++)
						{
							for (int x = 0; x < curr_mip_width; x++)
							{
								const Location loc = Location( x, y ) * Terrain::TileSize / terrain_nodes_per_tile;

								const float world_height = scene_data_source->GetTileMapWorldHeight(loc);
								min = std::min(min, world_height);
								max = std::max(max, world_height);

								auto heightmap_sample = simd::vector2_half( world_height, scene_data_source->GetTileMapCustomData(loc) );
								locked_datum.back().Get(simd::vector2_int(x, y)) = heightmap_sample;
							}
						}
					}
					else
					{
						auto &src_mip_data = locked_datum[curr_mip_level - 1];
						for (int y = 0; y < curr_mip_height; y++)
						{
							for (int x = 0; x < curr_mip_width; x++)
							{
								float samples = 0.0f;
								simd::vector2 sum = 0.0f;
								for (int box_y = 0; box_y < 2; box_y++)
								{
									for (int box_x = 0; box_x < 2; box_x++)
									{
										int src_x = x * 2 + box_x;
										int src_y = y * 2 + box_y;
										if (src_x < last_mip_width && src_y < last_mip_height)
										{
											simd::vector2_half src_sample = src_mip_data.Get(simd::vector2_int(src_x, src_y));
											sum += simd::vector2(src_sample.x.tofloat(), src_sample.y.tofloat());
											samples += 1.0f;
										}
									}
								}
								simd::vector2 avg_sample = samples > 0.0f ? simd::vector2(sum.x, sum.y) / samples : 0.0f;
								locked_datum.back().Get(simd::vector2_int(x, y)) = simd::vector2_half(avg_sample.x, avg_sample.y);
							}
						}
					}
				}

				if ((curr_mip_width == 1) && (curr_mip_height == 1))
					break;
				curr_mip_level++;
				last_mip_width = curr_mip_width;
				last_mip_height = curr_mip_height;
				curr_mip_width = curr_mip_width > 1 ? curr_mip_width / 2 : 1;
				curr_mip_height = curr_mip_height > 1 ? curr_mip_height / 2 : 1;
			}

			for (size_t mip_level = 0; mip_level < rects.size(); mip_level++)
			{
				terrain_data_texture->UnlockRect( static_cast< UINT >( mip_level ) );
			}

			terrain_tex_size = simd::vector4(
				(float)terrain_texture_width,
				(float)terrain_texture_height,
				0.0f,
				0.0f);
		}

		void System::CreateBlightTexture()
		{
			auto blight_map_data = scene_data_source->GetBlightMapData();
			if (blight_map_data.blight_data.size() > 0)
			{
				PROFILE;

				this->blight_data.blight_tex = Device::Texture::CreateTexture("Blight Data", device,
					blight_map_data.size.x,
					blight_map_data.size.y,
					1, Device::UsageHint::DEFAULT, Device::PixelFormat::R32F, Device::Pool::MANAGED_WITH_SYSTEMMEM, false, false, false);
					
				Device::LockedRect rect;
				this->blight_data.blight_tex->LockRect(0, &rect, Device::Lock::DEFAULT);
				assert(rect.pBits);
				auto locked_data = TexUtil::LockedData<float>(rect.pBits, rect.Pitch);
				for (int y = 0; y < blight_map_data.size.y; y++)
				{
					for (int x = 0; x < blight_map_data.size.x; x++)
					{
						locked_data.Get(simd::vector2_int(x, y)) = blight_map_data.blight_data[x + (size_t)y * blight_map_data.size.x];
					}
				}
				this->blight_data.blight_tex->UnlockRect(0);
				this->blight_data.blight_tex_size = simd::vector4( static_cast< float >( blight_map_data.size.x ), static_cast< float >( blight_map_data.size.y ), -1.0f, -1.0f);
			}
			else
			{
				this->blight_data.blight_tex.Reset();
				this->blight_data.blight_tex_size = simd::vector4(-1.0f, -1.0f, -1.0f, -1.0f);
			}
		}

		void System::InitialiseSource( )
		{
			PROFILE;

			if( scene_data_source && device )
			{
				scene_data_source->OnCreateDevice( device );
				scene_data_source->SetupSceneObjects();
				scene_ready = true;

				int burn_nodes_per_tile = 5;

				int burn_texture_width = GetSceneTilesCount().x * burn_nodes_per_tile;
				int burn_texture_height = GetSceneTilesCount().y * burn_nodes_per_tile;

				CreateTerrainHeightmapTexture();
				CreateBlightTexture();


				#if defined(_XBOX_ONE)
				{
					// Seems to prevent driver crash on Xbox.
					burn_texture_width = std::max(128, burn_texture_width);
					burn_texture_height = std::max(128, burn_texture_height);
				}
				#endif

				{
					burn_intensity_texture = Device::Texture::CreateTexture("Burn Intensity", device,
						burn_texture_width,
						burn_texture_height,
						1, Device::UsageHint::RENDERTARGET, Device::PixelFormat::A16B16G16R16F, Device::Pool::DEFAULT, false, false, false);

					burn_intensity_texture_prev = Device::Texture::CreateTexture("Burn Intensity Prev", device,
						burn_texture_width,
						burn_texture_height,
						1, Device::UsageHint::RENDERTARGET, Device::PixelFormat::A16B16G16R16F, Device::Pool::DEFAULT, false, false, false);


					burn_texture_needs_clear = true;

					burn_intensity_target = Device::RenderTarget::Create("Burn Intensity", device, burn_intensity_texture, 0);
					burn_intensity_target_prev = Device::RenderTarget::Create("Burn Intensity Prev", device, burn_intensity_texture_prev, 0);
				}

				CreateSynthesisMap(scene_data_source->GetSynthesisMapData());
				synthesis_data.decay_time = 0.0f;
				synthesis_data.creation_time = 0.0f;
				synthesis_data.global_stability = 0.0f;

				CreateFloodWaveMap(scene_data_source->GetWaterMapData());


				noise_sampler = distorton_texture;
				terrain_sampler = terrain_data_texture;
				burn_sampler = burn_intensity_texture;

				burn_tex_size = simd::vector4(
					(float)burn_texture_width,
					(float)burn_texture_height,
					0.0f,
					0.0f);

				terrain_world_size = simd::vector4(
					(float)GetSceneSize().x,
					(float)GetSceneSize().y,
					GetSceneMaxHeight(),
					0.0f);
			}
		}

		void System::DeinitialiseSource( )
		{
			if( scene_data_source && scene_ready )
			{
				scene_ready = false;
				scene_data_source->DestroySceneObjects();
				scene_data_source->OnDestroyDevice();
			}

			descriptor_sets.Clear();
			pipelines.Clear();
			passes.Clear();

			terrain_data_texture.Reset();
			burn_intensity_texture.Reset();
			burn_intensity_target.reset();
			burn_intensity_texture_prev.Reset();
			burn_intensity_target_prev.reset();

			flood_wave_data.flood_wave_data_texture.Reset();

			synthesis_data.decay_data_texture.Reset();
			synthesis_data.stable_data_texture.Reset();

			RemoveIrradienceCube();

			if (orphaned_effects_manager)
				orphaned_effects_manager->Clear();
		}

		const std::shared_ptr<Light> System::GetAmbientLight() { return impl->GetAmbientLight(); }
		PointLights System::GetVisiblePointLights(const View& view) const { return impl->GetVisiblePointLights(view); }
		PointLights System::GetVisiblePointLights() const { return impl->GetVisiblePointLights(); }
		std::pair<Lights, Lights> System::GetVisibleLightsNoPoint() const { return { impl->GetVisibleLights(), impl->GetVisibleShadowedLights() }; }
		void System::AddLight(const std::shared_ptr<Light>& light) { impl->AddLight(light); }
		void System::RemoveLight(const std::shared_ptr<Light>& light) { impl->RemoveLight(light); }

		void System::SetEnvironmentSettings(const Environment::Data& settings, const simd::matrix& root_transform, const Vector2d& player_position, const float player_height, const float colour_percent, const float radius_percent, const int light_radius_pluspercent, const simd::vector3* base_colour_override, const float far_plane_multiplier, const float custom_z_rotation)
		{
			SetDirectionalLightSettings(settings);
			SetGlobalParticleEffectSettings(settings, player_height);
			SetEnvironmentMappingSettings(settings);
			SetFogSettings(settings);
			SetCameraSettings(settings, far_plane_multiplier, custom_z_rotation);
			SetPlayerLightSettings(settings, root_transform, player_position, player_height, colour_percent, radius_percent, light_radius_pluspercent, base_colour_override);
		}

		void System::SetDirectionalLightSettings(const Environment::Data& settings)
		{
			const bool enabled = (settings.Value(Environment::EnvParamIds::Float::DirectionalLightMultiplier) > 1e-5f);
			const bool shadows_enabled = settings.Value(Environment::EnvParamIds::Bool::DirectionalLightShadowsEnabled);

			if (directional_light.get() && (!enabled || directional_light->GetShadowed() != shadows_enabled))
			{
				RemoveLight(directional_light);
				directional_light.reset();
			}

			const auto light_dir_and_color = Environment::EnvironmentSettingsV2::ComputeLightDirectionAndColour(settings, GetFrameTime());
			const auto direction = light_dir_and_color.first; 
			const auto color = light_dir_and_color.second;

			if (!directional_light.get() && enabled)
			{
				directional_light = std::make_shared<DirectionalLight>(
					direction,
					color,
					settings.Value(Environment::EnvParamIds::Float::DirectionalLightCamNearPercent),
					settings.Value(Environment::EnvParamIds::Float::DirectionalLightCamFarPercent),
					settings.Value(Environment::EnvParamIds::Float::DirectionalLightCamExtraZ),
					settings.Value(Environment::EnvParamIds::Bool::DirectionalLightShadowsEnabled),
					settings.Value(Environment::EnvParamIds::Float::DirectionalLightMinOffset),
					settings.Value(Environment::EnvParamIds::Float::DirectionalLightMaxOffset),
					settings.Value(Environment::EnvParamIds::Bool::DirectionalLightCinematicMode)
				);
				AddLight(directional_light);
			}

			if (directional_light.get())
			{
				directional_light->SetDirection(direction);
				directional_light->SetColour(color);
				directional_light->SetCamProperties(
					settings.Value(Environment::EnvParamIds::Float::DirectionalLightCamNearPercent),
					settings.Value(Environment::EnvParamIds::Float::DirectionalLightCamFarPercent),
					settings.Value(Environment::EnvParamIds::Float::DirectionalLightCamExtraZ),
					settings.Value(Environment::EnvParamIds::Float::DirectionalLightMinOffset),
					settings.Value(Environment::EnvParamIds::Float::DirectionalLightMaxOffset),
					settings.Value(Environment::EnvParamIds::Bool::DirectionalLightCinematicMode),
					settings.Value(Environment::EnvParamIds::Float::DirectionalLightPenumbraRadius),
					settings.Value(Environment::EnvParamIds::Float::DirectionalLightPenumbraDist));
			}
		}

		void System::SetPlayerLightSettings(const Environment::Data& settings, const simd::matrix& root_transform, const Vector2d& player_position, const float player_height, const float colour_percent, const float radius_percent, const int light_radius_pluspercent, const simd::vector3* base_colour_override)
		{
			const bool enabled = (settings.Value(Environment::EnvParamIds::Float::PlayerLightIntensity) > 1e-5f);
			const bool shadows_enabled = settings.Value(Environment::EnvParamIds::Bool::PlayerLightShadowsEnabled);

			if (player_spotlight.get() && !enabled)
			{
				RemoveLight(player_spotlight);
				player_spotlight.reset();
			}

			if (!player_spotlight.get() && enabled)
			{
				player_spotlight = Environment::CreatePlayerLight(settings);
				AddLight(player_spotlight);
			}

			if (player_spotlight.get())
			{
				Environment::UpdatePlayerLight(*player_spotlight, settings, root_transform, player_position, player_height, colour_percent, radius_percent, light_radius_pluspercent, base_colour_override);
			}
		}

		void System::SetGlobalParticleEffectSettings(const Environment::Data& settings, float player_height)
		{
			const auto& global_particle_effect_name = settings.Value(Environment::EnvParamIds::String::GlobalParticleEffect).Get();

			if (global_particle_effect_handle.IsNull() || global_particle_effect_handle.GetFilename() != global_particle_effect_name
			#ifdef ENABLE_DEBUG_CAMERA
				|| GetCamera() != global_particle_effect_camera
			#endif
				)
			{
				global_particle_effect_handle = Particles::ParticleEffectHandle();
				global_particle_effect.reset();
				global_particle_effect_camera = nullptr;
				
				if (global_particle_effect_name != L"")
				{
					global_particle_effect_handle = Particles::ParticleEffectHandle(global_particle_effect_name);
					global_particle_effect = std::make_shared<Particles::EffectInstance>(global_particle_effect_handle, Memory::SmallVector<simd::vector3, 16, Memory::Tag::Particle>(), simd::matrix::identity(), GetCamera(), false, false, 0.0f, -1.0f);
				#ifdef ENABLE_DEBUG_CAMERA
					global_particle_effect_camera = GetCamera();
				#endif
				}
			}

			if (global_particle_effect)
			{
				global_particle_effect->SetMultiplier(1.0f);

				const BoundingBox global_particles_box = BoundingBox(
					simd::vector3(player_position.x - 1.0f, player_position.y + 1.0f, player_height - 1.0f), 
					simd::vector3(player_position.x + 1.0f, player_position.y + 1.0f, player_height + 1.0f));

				global_particle_effect->SetTransform(Layer::Main, simd::matrix::identity(), dynamic_parameters_storage, {}, {});
				global_particle_effect->SetBoundingBox(global_particles_box, true);
			}
		}

		void System::SetFogSettings(const Environment::Data& settings)
		{
			using Params = Environment::EnvParamIds;

			screenspace_fog_data.layercount_thickness_turbulence = simd::vector4(
				settings.Value(Params::Float::SSFLayerCount),
				settings.Value(Params::Float::SSFThickness),
				settings.Value(Params::Float::SSFTurbulence),
				0.f);
			screenspace_fog_data.color = simd::vector4(settings.Value(Params::Vector3::SSFColor), settings.Value(Params::Float::SSFColorAlpha));
			screenspace_fog_data.disperse_radius_feathering = simd::vector4(
				settings.Value(Params::Float::SSFDisperseRadius),
				settings.Value(Params::Float::SSFFeathering),
				0.f, 0.f);
		}

		void System::SetCameraSettings(const Environment::Data& settings, const float far_plane_multiplier, const float custom_z_rotation)
		{
			using Params = Environment::EnvParamIds;

			if (auto* camera = GetCamera())
			{
				camera->SetNearFarPlanes(settings.Value(Params::Float::CameraNearPlane), settings.Value(Params::Float::CameraFarPlane) * far_plane_multiplier);

				if (custom_z_rotation != 0.f)
					camera->SetZRotationOffset(custom_z_rotation);
				else
					camera->SetZRotationOffset(settings.Value(Params::Float::CameraZRotation));
			}

			exposure = settings.Value(Params::Float::CameraExposure);
		}

		void System::SetEnvironmentMappingSettings( const Environment::Data& settings)
		{
			using Params = Environment::EnvParamIds;

			///TODO need conversion operators on handle
			std::wstring diffuse_map_name = settings.Value(Params::String::EnvMappingDiffuseCubeTex).Get();
			if (diffuse_map_name == L"")
				diffuse_map_name = L"Art/2DArt/Cubemaps/black_cube.dds";
			diffuse_tex_handle = ::Texture::Handle(::Texture::CubeTextureDesc(diffuse_map_name));

			std::wstring specular_map_name = settings.Value(Params::String::EnvMappingSpecularCubeTex).Get();
			if (specular_map_name == L"")
				specular_map_name = L"Art/2DArt/Cubemaps/black_cube.dds";
			specular_tex_handle = ::Texture::Handle(::Texture::CubeTextureDesc(specular_map_name));

			diffuse_irradience_cube = diffuse_tex_handle;
			specular_irradience_cube = specular_tex_handle;

			UpdateDynamicIrradienceProperties(settings);

			const bool use_diffuse_irradience_cube = !diffuse_tex_handle.IsNull();
			const bool use_specular_irradience_cube = !specular_tex_handle.IsNull();

			if (use_diffuse_irradience_cube)
			{
				diffuse_irradience_enabled = true;
			}
			else
			{
				diffuse_irradience_enabled = false;
				::Texture::Handle textureCubeHandle(::Texture::CubeTextureDesc(L"Art/2DArt/Cubemaps/black_cube.dds"));
				diffuse_irradience_cube = textureCubeHandle;
			}

			if (use_specular_irradience_cube)
			{
				specular_irradience_enabled = true;
				float size = float(specular_irradience_cube.GetWidth());
				specular_irradience_size = simd::vector4(size, size, size, 0.0f);
			}
			else
			{
				specular_irradience_enabled = false;
				::Texture::Handle textureCubeHandle(::Texture::CubeTextureDesc(L"Art/2DArt/Cubemaps/black_cube.dds"));
				specular_irradience_cube = textureCubeHandle;
			}
		}

		Device::UniformInputs System::BuildUniformInputs()
		{
			Device::UniformInputs uniform_inputs;
			uniform_inputs.reserve(64);

		#ifdef PROFILING
			uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::DebugOverlayStrength)).SetFloat(&debug_overlay_strength));
		#endif

			uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::FrameIndex)).SetUInt(&frame_index));
			uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::Time)).SetFloat(&frame_time));
			uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::ToolTime)).SetFloat(&tool_time));
			uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::PlayerPosition)).SetVector(&player_position));

			uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::SSFLayerCountThicknessTurbulence)).SetVector(&screenspace_fog_data.layercount_thickness_turbulence));
			uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::SSFColor)).SetVector(&screenspace_fog_data.color));
			uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::SSFDisperseRadiusFeathering)).SetVector(&screenspace_fog_data.disperse_radius_feathering));

			uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::BlightTexSize)).SetVector(&blight_data.blight_tex_size));

			uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::AfflictionPos)).SetVector(&affliction_data.centre_pos));
			uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::AfflictionRadii)).SetVector(&affliction_data.radii));

			uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::InfiniteHungerArenaWaterLevel)).SetVector( &infinite_hunger_arena_data.water_level ) );

			uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::Exposure)).SetFloat(&exposure));

			uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::HeightmapWorldSize)).SetVector(&terrain_world_size));
			uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::HeightmapTexSize)).SetVector(&terrain_tex_size));
			uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::BurnTexSize)).SetVector(&burn_tex_size));
			uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::FloodWaveTexSize)).SetVector(&flood_wave_data.texture_size));

			uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::LakeMistIntensity)).SetFloat(&lake_mist_data.intensity));

			uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::DiffuseIrradienceCubeEnabled)).SetBool(&diffuse_irradience_enabled));
			uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::SpecularIrradienceCubeEnabled)).SetBool(&specular_irradience_enabled));
			uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::SpecularIrradienceCubeSize)).SetVector(&specular_irradience_size));
			uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::IrradienceCubeBrightness)).SetVector(&irradience_cube_brightness));

			uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::EnvMapRotation)).SetMatrix(&env_map_rotation));
			uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::DirectLightEnvRatio)).SetFloat(&direct_light_env_ratio));

			uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::BreachSphereInfos)).SetMatrix(&breach_sphere_infos));

			uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::RoofFade)).SetFloat(&roof_fade));

			uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::WaterDebug, 0)).SetBool(water_debug ? &::Renderer::ConstData::bool_true : &::Renderer::ConstData::bool_false));

			uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::ViewStartIndex)).SetUInt(&GetCamera()->GetStartIndex()));

			uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::SplineLUTWidth)).SetFloat(&LUT::System::Get().GetBufferWidth()));

			return uniform_inputs;
		}

		Device::BindingInputs System::BuildBindingInputs()
		{
			Device::BindingInputs binding_inputs;
			binding_inputs.reserve(64);

			impl->AddBindingInputs(binding_inputs);

			if (!noise_sampler.IsNull())
				binding_inputs.push_back(Device::BindingInput(Device::IdHash(DrawDataNames::NoiseSampler)).SetTextureResource(noise_sampler));
			if (!diffuse_tex_handle.IsNull())
				binding_inputs.push_back(Device::BindingInput(Device::IdHash(DrawDataNames::DiffuseIrradienceCube)).SetTextureResource(diffuse_irradience_cube));
			if (!specular_tex_handle.IsNull())
				binding_inputs.push_back(Device::BindingInput(Device::IdHash(DrawDataNames::SpecularIrradienceCube)).SetTextureResource(specular_irradience_cube));

			binding_inputs.push_back(Device::BindingInput(Device::IdHash(DrawDataNames::HeightmapSampler)).SetTexture(terrain_sampler));
			binding_inputs.push_back(Device::BindingInput(Device::IdHash(DrawDataNames::FloodWaveTex)).SetTexture(flood_wave_data.flood_wave_data_texture));
			binding_inputs.push_back(Device::BindingInput(Device::IdHash(DrawDataNames::TerrainBurnSampler)).SetTexture(burn_sampler));
			binding_inputs.push_back(Device::BindingInput(Device::IdHash(DrawDataNames::BlightSampler)).SetTexture(blight_data.blight_tex));
			binding_inputs.push_back(Device::BindingInput(Device::IdHash(DrawDataNames::AnimationMatrices)).SetStructuredBuffer(Animation::System::Get().GetBuffer()));
			binding_inputs.push_back(Device::BindingInput(Device::IdHash(DrawDataNames::ShadowMapAtlas)).SetTexture(shadow_manager.GetShadowAtlas()));
			binding_inputs.push_back(Device::BindingInput(Device::IdHash(DrawDataNames::TrailVertexBuffer)).SetStructuredBuffer(Trails::System::Get().GetBuffer()));
			binding_inputs.push_back(Device::BindingInput(Device::IdHash(DrawDataNames::SplineLUT)).SetTexture(LUT::System::Get().GetBuffer()));

			tiled_lighting.BindInputs(binding_inputs);

			return binding_inputs;
		}

		MaterialHandle System::GetGroundMaterial(const simd::vector3& pos)
		{
			if (!scene_data_source)
				return MaterialHandle();

			const auto node_size = scene_data_source->GetNodeSize();
			const auto map_size = scene_data_source->GetSceneSize() / node_size;
			const auto map_offset = scene_data_source->GetSceneLocation();
			const auto location = (pos.xy() - map_offset) / node_size;
			const unsigned x = unsigned(std::clamp(location.x - 0.5f, 0.0f, map_size.x));
			const unsigned y = unsigned(std::clamp(location.y - 0.5f, 0.0f, map_size.y));
			return scene_data_source->GetGroundMaterial(x, y);
		}

		void System::RemoveIrradienceCube()
		{
			diffuse_irradience_cube = ::Texture::Handle();
			specular_irradience_cube = ::Texture::Handle();

			specular_tex_handle = ::Texture::Handle();
			diffuse_tex_handle = ::Texture::Handle();

			diffuse_irradience_enabled = false;
			specular_irradience_enabled = false;
			specular_irradience_size = simd::vector4(-1.0f, -1.0f, -1.0f, -1.0f);
		}

		void System::UpdateDynamicIrradienceProperties(const Environment::Data& settings)
		{
			using Params = Environment::EnvParamIds;

			irradience_cube_brightness = simd::vector4( 
				settings.Value(Params::Float::EnvMappingEnvBrightness), 
				settings.Value(Params::Float::EnvMappingSolidSpecularAttenuation),
				settings.Value(Params::Float::EnvMappingWaterSpecularAttenuation),
				settings.Value(Params::Float::EnvMappingGiAdditionalEnvLight));

			direct_light_env_ratio = settings.Value(Params::Float::EnvMappingDirectLightEnvRatio);
			auto def_cube_matrix = simd::matrix(
				simd::vector4(1.0f, 0.0f, 0.0f, 0.0f),
				simd::vector4(0.0f, 0.0f, 1.0f, 0.0f),
				simd::vector4(0.0f, -1.0f, 0.0f, 0.0f),
				simd::vector4(0.0f, 0.0f, 0.0f, 1.0f));
			float hor_angle = settings.Value(Params::Float::EnvMappingHorAngle);
			auto hor_rot_matrix = simd::matrix(
				simd::vector4(cos(hor_angle), sin(hor_angle), 0.0f, 0.0f),
				simd::vector4(-sin(hor_angle), cos(hor_angle), 0.0f, 0.0f),
				simd::vector4(0.0f, 0.0f, 1.0f, 0.0f),
				simd::vector4(0.0f, 0.0f, 0.0f, 1.0f));
			float vert_angle = settings.Value(Params::Float::EnvMappingVertAngle);
			auto vert_rot_matrix = simd::matrix(
				simd::vector4(1.0f, 0.0f, 0.0f, 0.0f),
				simd::vector4(0.0f, cos(vert_angle), sin(vert_angle), 0.0f),
				simd::vector4(0.0f, -sin(vert_angle), cos(vert_angle), 0.0f),
				simd::vector4(0.0f, 0.0f, 0.0f, 1.0f));
			env_map_rotation = hor_rot_matrix * vert_rot_matrix * def_cube_matrix;
		}

		PostProcess::SynthesisMapInfo System::GetSynthesisMapInfo()
		{
			PostProcess::SynthesisMapInfo synthesis_map_info;
			synthesis_map_info.decay_tex = GetDecayMapTexture();
			synthesis_map_info.stable_tex = GetStableMapTexture();
			synthesis_map_info.size = GetSynthesisMapSize();
			synthesis_map_info.min_point = GetSynthesisMapMinPoint();
			synthesis_map_info.max_point = GetSynthesisMapMaxPoint();
			synthesis_map_info.decay_time = GetDecayTime();
			synthesis_map_info.decay_type = GetSynthesisDecayType();
			synthesis_map_info.creation_time = GetCreationTime();
			synthesis_map_info.global_stability = GetGlobalStability();
			synthesis_map_info.stabiliser_position = GetSynthesisStabiliserPosition();
			return synthesis_map_info;
		}

		void System::ResetBreachSpheres()
		{
			breach_sphere_infos = simd::matrix(
					simd::vector4( 0.f, 0.f, 0.f, -1.f ),
					simd::vector4( 0.f, 0.f, 0.f, -1.f ),
					simd::vector4( 0.f, 0.f, 0.f, -1.f ),
					simd::vector4( 0.f, 0.f, 0.f, -1.f ) );
			}

		void System::UpdateBreachSphere( unsigned index, simd::vector3 sphere_position, float sphere_radius )
		{
			assert(index < 4);
			breach_sphere_infos[ index ] = simd::vector4( sphere_position.x, sphere_position.y, sphere_position.z, sphere_radius );
		}

		const simd::matrix& System::GetBreachInfo() const
		{
			return breach_sphere_infos;
		}

#if defined( ENVIRONMENT_EDITING_ENABLED ) || GGG_CHEATS == 1
		void System::DisableBreachClipping()
		{
			// just have a large breach clip radius
			ResetBreachSpheres();
			breach_sphere_infos[0] = simd::vector4( 0.f, 0.f, 0.f, 20000.f );
			}
#endif

		const simd::vector2 System::GetSceneSize()
		{
			return this->scene_data_source ? this->scene_data_source->GetSceneSize() : simd::vector2(0.0f, 0.0f);
		}

		const simd::vector2_int System::GetSceneTilesCount()
		{
			simd::vector2_int scene_tiles_count;
			if(this->scene_data_source)
			{
				scene_tiles_count.x = int(this->GetSceneSize().x / this->scene_data_source->GetNodeSize() + 0.5f);
				scene_tiles_count.y = int(this->GetSceneSize().y / this->scene_data_source->GetNodeSize() + 0.5f);
			}else
			{
				scene_tiles_count = simd::vector2_int(1, 1);
			}
			return scene_tiles_count;
		}

		const float System::GetSceneMaxHeight()
		{
			return (float)(255 * 24 * Terrain::UnitToWorldHeightScale);
		}

		void System::CreateSynthesisMap( const SynthesisMapData& synthesis_map_data )
		{
			if( !synthesis_map_data.decay_data )
				return;

			PROFILE;

			synthesis_data.texture_size = synthesis_map_data.size;
			synthesis_data.min_point = synthesis_map_data.min_point;
			synthesis_data.max_point = synthesis_map_data.max_point;

			synthesis_data.decay_data_texture = Device::Texture::CreateTexture("Synthesis Decay", device,
				synthesis_map_data.size.x,
				synthesis_map_data.size.y,
				1, Device::UsageHint::DYNAMIC, Device::PixelFormat::A32B32G32R32F, Device::Pool::DEFAULT, false, false, false);

			synthesis_data.stable_data_texture = Device::Texture::CreateTexture("Synthesis Stable", device,
				synthesis_map_data.size.x,
				synthesis_map_data.size.y,
				1, Device::UsageHint::DYNAMIC, Device::PixelFormat::R32F, Device::Pool::DEFAULT, false, false, false);

			//need to confirm it works on xbox
#if defined(_XBOX_ONE)
			{
				// Seems to prevent driver crash on Xbox.
				//burn_texture_width = std::max(128, burn_texture_width);
				//burn_texture_height = std::max(128, burn_texture_height);
			}
#endif
			std::vector<simd::vector4> tmp_color;
			tmp_color.resize(synthesis_data.texture_size.x * synthesis_data.texture_size.y);

			Device::LockedRect rect;
			synthesis_data.decay_data_texture->LockRect(0, &rect, Device::Lock::DEFAULT);
			if (rect.pBits)
			{
				TexUtil::LockedData<simd::vector4> decay_locked_data(rect.pBits, rect.Pitch);
				simd::vector2_int size = synthesis_data.texture_size;
				for (int y = 0; y < size.y; y++)
				{
					for (int x = 0; x < size.x; x++)
					{
						tmp_color[x + size.x * y] = simd::vector4(synthesis_map_data.decay_data[x + y * size.x], 0.0f, 0.0f, 0.0f);
					}
				}
				for (int y = 0; y < size.y; y++)
				{
					for (int x = 0; x < size.x; x++)
					{
						tmp_color[x + size.x * y].y = (
							tmp_color[std::min(x + 1, size.x - 1) + y * size.x].x -
							tmp_color[std::max(x - 1, 0) + y * size.x].x) / 2.0f;
						tmp_color[x + size.x * y].z = (
							tmp_color[x + std::min(y + 1, size.y - 1) * size.x].x -
							tmp_color[x + std::max(y - 1, 0) * size.x].x) / 2.0f;
							
						/*decay_locked_data.Get(simd::vector2_int(x, y)).y = (
							decay_locked_data.Get(simd::vector2_int(std::min(x + 1, decay_texture_size.x - 1), y)).x -
							decay_locked_data.Get(simd::vector2_int(std::max(x - 1, 0),                        y)).x) / 2.0f;
						decay_locked_data.Get(simd::vector2_int(x, y)).z = (
							decay_locked_data.Get(simd::vector2_int(x, std::min(y + 1, decay_texture_size.y - 1))).x -
							decay_locked_data.Get(simd::vector2_int(x, std::max(y - 1, 0                       ))).x) / 2.0f;*/
					}
				}

				for (int y = 0; y < size.y; y++)
				{
					for (int x = 0; x < size.x; x++)
					{
						simd::vector4 x_gradient =
							(tmp_color[std::max(x - 1, 0) + y * size.x] -
							 tmp_color[std::min(x + 1, size.x - 1) + y * size.x]) / 2.0f;

						simd::vector4 y_gradient =
							(tmp_color[x + std::min(y + 1, size.y - 1) * size.x] -
							 tmp_color[x + std::max(y - 1, 0) * size.x]) / 2.0f;

						tmp_color[x + y * size.x].w = simd::vector2(x_gradient.y, x_gradient.z).len() + simd::vector2(y_gradient.y, y_gradient.z).len();
					}
				}

				for (int y = 0; y < size.y; y++)
				{
					for (int x = 0; x < size.x; x++)
					{
						decay_locked_data.Get(simd::vector2_int(x, y)) = tmp_color[x + y * size.x];
					}
				}
			}
			synthesis_data.decay_data_texture->UnlockRect(0);

			synthesis_data.stable_data_texture->LockRect(0, &rect, Device::Lock::DEFAULT);
			if (rect.pBits)
			{
				TexUtil::LockedData<float> stable_locked_data(rect.pBits, rect.Pitch);
				simd::vector2_int size = synthesis_data.texture_size;
				for (int y = 0; y < size.y; y++)
				{
					for (int x = 0; x < size.x; x++)
					{
						stable_locked_data.Get(simd::vector2_int(x, y)) = synthesis_map_data.stable_data[x + y * size.x];
					}
				}
			}
			synthesis_data.stable_data_texture->UnlockRect(0);
		}

		Memory::SmallVector<float, 256, Memory::Tag::Scene> DiffuseFloodWave(const float *data, simd::vector2_int size)
		{
			Memory::SmallVector<float, 256, Memory::Tag::Scene> diffuse_data(size_t(size.x * size.y));

			for (int y = 0; y < size.y; y++)
			{
				for (int x = 0; x < size.x; x++)
				{
					if (data[x + y * size.x] < 0.0f)
					{
						float total_weight = 1e-5f;
						float avg_dist = 0.0f;
						for (int y_offset = -1; y_offset <= 1; y_offset++)
						{
							for (int x_offset = -1; x_offset <= 1; x_offset++)
							{
								int res_x = x + x_offset;
								int res_y = y + y_offset;
								if (res_x < 0 || res_y < 0 || res_x >= size.x || res_y >= size.y) continue;
								float dist = data[res_x + res_y * size.x];
								if (dist < 0.0f) continue;
								avg_dist += dist;
								total_weight += 1.0f;
							}
						}

						diffuse_data[x + size.x * y] = avg_dist / total_weight;
					}
					else
					{
						diffuse_data[x + size.x * y] = data[x + y * size.x];
					}
				}
			}
			return diffuse_data;
		}

		void System::CreateFloodWaveMap(const WaterMapData& water_map_data)
		{
			if (!water_map_data.flow_data)
				return;

			PROFILE;

			#if defined(_XBOX_ONE)
			{
				// In case of a driver crash on xbox, having a texture less than 128 can be the culprit
				//burn_texture_width = std::max(128, burn_texture_width);
				//burn_texture_height = std::max(128, burn_texture_height);
			}
			#endif

			flood_wave_data.texture_size = simd::vector4(float(water_map_data.size.x), float(water_map_data.size.y), 0.0f, 0.0f);

			flood_wave_data.flood_wave_data_texture = Device::Texture::CreateTexture("Flood Wave", device,
				water_map_data.size.x,
				water_map_data.size.y,
				1, Device::UsageHint::DYNAMIC, Device::PixelFormat::A32B32G32R32F, Device::Pool::DEFAULT, false, false, false);

			auto diffuse_map = DiffuseFloodWave(water_map_data.flow_data, water_map_data.size);

			std::vector<simd::vector4> tmp_color;
			tmp_color.resize(water_map_data.size.x * water_map_data.size.y);

			Device::LockedRect rect;
			flood_wave_data.flood_wave_data_texture->LockRect(0, &rect, Device::Lock::DEFAULT);
			if (rect.pBits)
			{
				TexUtil::LockedData<simd::vector4> flood_wave_locked_data(rect.pBits, rect.Pitch);
				simd::vector2_int size = water_map_data.size;
				for (int y = 0; y < size.y; y++)
				{
					for (int x = 0; x < size.x; x++)
					{
						tmp_color[x + size.x * y] = simd::vector4(diffuse_map[x + y * size.x], 0.0f, 0.0f, 1.0f);
						if (tmp_color[x + size.x * y].x < 0.0f) tmp_color[x + size.x * y] = 1e5f;
					}
				}
				for (int y = 0; y < size.y; y++)
				{
					for (int x = 0; x < size.x; x++)
					{
						tmp_color[x + size.x * y].y = (
							tmp_color[std::min(x + 1, size.x - 1) + y * size.x].x -
							tmp_color[std::max(x - 1, 0) + y * size.x].x) / 2.0f;
						tmp_color[x + size.x * y].z = (
							tmp_color[x + std::min(y + 1, size.y - 1) * size.x].x -
							tmp_color[x + std::max(y - 1, 0) * size.x].x) / 2.0f;
					}
				}


				for (int y = 0; y < size.y; y++)
				{
					for (int x = 0; x < size.x; x++)
					{
						if (water_map_data.flow_data[x + size.x * y] < 0.0f)
						{
							tmp_color[x + size.x * y].y = 0.0f;
						}
						else
						{
							tmp_color[x + size.x * y].y = 1.0f;
						}

						if (diffuse_map[x + size.x * y] < 0.0f)
						{
							tmp_color[x + size.x * y].x = 1e5f;
						}
						flood_wave_locked_data.Get(simd::vector2_int(x, y)) = tmp_color[x + y * size.x];
					}
				}
			}
			flood_wave_data.flood_wave_data_texture->UnlockRect(0);
		}

		void System::SetRoomOcclusionData( const RoomOcclusionData& data )
		{
			room_occlusion_data.rooms.resize( data.rooms.size() );
			for( size_t i = 0; i < data.rooms.size(); ++i )
			{
				room_occlusion_data.rooms[ i ].x1 = data.rooms[ i ].x1;
				room_occlusion_data.rooms[ i ].y1 = data.rooms[ i ].y1;
				room_occlusion_data.rooms[ i ].x2 = data.rooms[ i ].x2;
				room_occlusion_data.rooms[ i ].y2 = data.rooms[ i ].y2;
				room_occlusion_data.rooms[ i ].opacity = data.rooms[ i ].opacity;
			}
		}

		void System::SetRitualEncounterData( const simd::vector4& sphere, float ratio )
		{
			ritual_data.sphere = sphere;
			ritual_data.ratio = ratio;
		}

		simd::vector4 System::GetRitualEncounterSphere() const
		{
			return ritual_data.sphere;
		}

		float System::GetRitualEncounterRatio() const
		{
			constexpr float cull_range = Terrain::ToWorldUnits( 200 );
			const auto dist_sq = ( simd::vector2( ritual_data.sphere.x, ritual_data.sphere.y ) - simd::vector2( player_position.x, player_position.y ) ).sqrlen();
			const bool is_inside = dist_sq < std::pow( ritual_data.sphere.w, 2.0f );

			if( dist_sq > std::pow( cull_range + ritual_data.sphere.w, 2.0f ) )
				return 0.0f;

			return is_inside ? ritual_data.ratio : -ritual_data.ratio;
		}

		simd::color System::GetFinalFogColor()
		{
			// old fog removed, getting color from the screen space fog settings
			simd::vector4 final_color = screenspace_fog_data.color;
			final_color = final_color * 255;
			unsigned r = std::min(static_cast< unsigned >( final_color.x ), unsigned(255));
			unsigned g = std::min(static_cast< unsigned >( final_color.y ), unsigned(255));
			unsigned b = std::min(static_cast< unsigned >( final_color.z ), unsigned(255));
			return simd::color::argb( 0, r, g, b );
		}

		void System::DisableFog()
		{
			// old fog removed, need to do this another way
		}

		void System::ResetFog()
		{
			// old fog removed, need to do this another way
		}
		

}
}