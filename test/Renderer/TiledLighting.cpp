
#include "Visual/Device/Device.h"
#include "Visual/Device/DynamicResolution.h"
#include "Visual/Renderer/Scene/SceneSystem.h"
#include "Visual/Renderer/Scene/Camera.h"
#include "Visual/Renderer/Scene/View.h"
#include "Visual/Renderer/Scene/Light.h"

#include "TiledLighting.h"
#include "Utility.h"
#include "CachedHLSLShader.h"

namespace Renderer
{
	namespace Scene
	{
		static const unsigned PointLightInfoMaxCount = Scene::PointLightDataMaxCount * Scene::SubSceneMaxCount;
	}


	Device::Pipeline* TiledLighting::FindPipeline(const Memory::DebugStringA<>& name, Device::IDevice* device, Device::Shader* compute_shader)
	{
		Memory::Lock lock(pipelines_mutex);
		return pipelines.FindOrCreate(PipelineKey(compute_shader), [&]()
		{
			return Device::Pipeline::Create(name, device, compute_shader);
		}).Get();
	}

	Device::BindingSet* TiledLighting::FindBindingSet(const Memory::DebugStringA<>& name, Device::IDevice* device, Device::Shader* compute_shader, const Device::BindingSet::Inputs& inputs)
	{
		Memory::Lock lock(binding_sets_mutex);
		return binding_sets.FindOrCreate(BindingSetKey(compute_shader, inputs.Hash()), [&]()
		{
			return Device::BindingSet::Create(name, device, compute_shader, inputs);
		}).Get();
	}

	Device::DescriptorSet* TiledLighting::FindDescriptorSet(const Memory::DebugStringA<>& name, Device::IDevice* device, Device::Pipeline* pipeline, Device::BindingSet* binding_set)
	{
		Memory::Lock lock(descriptor_sets_mutex);
		return descriptor_sets.FindOrCreate(DescriptorSetKey(pipeline, binding_set), [&]()
		{
			return Device::DescriptorSet::Create(name, device, pipeline, { binding_set });
		}).Get();
	}

	Device::DynamicUniformBuffer* TiledLighting::FindUniformBuffer(const Memory::DebugStringA<>& name, Device::IDevice* device, Device::Shader* compute_shader)
	{
		Memory::Lock lock(uniform_buffers_mutex);
		return uniform_buffers.FindOrCreate(UniformBufferKey(compute_shader), [&]()
		{
			return Device::DynamicUniformBuffer::Create(name, device, compute_shader);
		}).Get();
	}

	TiledLighting::TiledLighting()
	{

	}

	void TiledLighting::UpdatePointLightInfos(const Scene::PointLights& point_lights)
	{
		Scene::PointLightInfo* infos;
		point_light_infos->Lock(0, Scene::PointLightInfoMaxCount * sizeof(Scene::PointLightInfo), (void**)(&infos), Device::Lock::DISCARD);
		if (infos)
		{
			for (size_t i = 0; i < point_lights.size(); i++)
			{
				if (i < Scene::PointLightDataMaxCount)
				{
					const auto& sub_scenes = Scene::System::Get().GetSubScenes();

					for (unsigned j = 0; j < Scene::SubSceneMaxCount; ++j)
					{
						const auto light_sub_scene_transform = Scene::System::Get().GetSubSceneTransform(point_lights[i]->GetBoundingBox());
						const auto transform = sub_scenes[j].GetTransform().inverse() * light_sub_scene_transform;
						infos[i * Scene::SubSceneMaxCount + j] = point_lights[i]->BuildPointLightInfo(transform);
					}
				}
			}
		}
		point_light_infos->Unlock();
	}

	void TiledLighting::UpdatePointLightData(const Scene::PointLights& point_lights)
	{
		Scene::PointLightInfo* infos;
		light_data->Lock(0, sizeof(Scene::PointLightInfo) * point_lights.size(), (void**)(&infos), Device::Lock::DISCARD);
		if (infos)
		{
			for (size_t i = 0; i < point_lights.size(); i++)
			{
				if (i < Scene::PointLightDataMaxCount)
				{
					infos[i] = point_lights[i]->BuildPointLightInfo(Scene::System::Get().GetSubSceneTransform(point_lights[i]->GetBoundingBox()));
				}
			}
		}
		light_data->Unlock();
	}

	void TiledLighting::UpdateIndexCounter()
	{
		void* raw_index_counter;
		index_counter->Lock(0, 4, (void**)&raw_index_counter, Device::Lock::DISCARD);
		if (raw_index_counter)
		{
			uint32_t* indexCounter = (uint32_t*)raw_index_counter;
			*indexCounter = 0;
		}
		index_counter->Unlock();
	}

	void TiledLighting::Update(Device::IDevice* device, const Scene::PointLights& point_lights)
	{
		light_count = (unsigned int)point_lights.size();

		UpdatePointLightInfos(point_lights);
		UpdatePointLightData(point_lights);
		UpdateIndexCounter();
	}

	void TiledLighting::ComputeTiles(Device::IDevice* device, const Scene::Camera& camera, Device::CommandBuffer& command_buffer)
	{
		device->BeginEvent(command_buffer, L"Tiled Light Assign");

		const auto dyn_frame_size = frame_size * Device::DynamicResolution::Get().GetFrameToDynamicScale();
		float dyn_num_tiles_x = std::min((float)num_tiles_x, (dyn_frame_size.x + TILE_SIZE - 1) / TILE_SIZE);
		float dyn_num_tiles_y = std::min((float)num_tiles_y, (dyn_frame_size.y + TILE_SIZE - 1) / TILE_SIZE);

		auto shader = tiles_assign_shader.Get();
		auto uniform_buffer = FindUniformBuffer("Tiled Light Assign", device, shader);
		uniform_buffer->SetFloat("num_tiles_x", dyn_num_tiles_x);
		uniform_buffer->SetFloat("num_tiles_y", dyn_num_tiles_y);
		uniform_buffer->SetUInt("max_index_count", max_num_indices);
		uniform_buffer->SetUInt("light_count", light_count);
		
		{
			const auto proj = camera.GetViewProjectionMatrix().inverse();

			// Position
			{
				auto& p = camera.GetPosition();
				auto tmp = simd::vector4(p.x, p.y, p.z, 0.0f);
				uniform_buffer->SetVector("camera_pos", &tmp);
			}

			{
				auto points = camera.GetFrustum().GetPoints();

				// Top Left
				{
					auto& p = points[Frustum::TOP_LEFT_FAR];
					auto tmp = simd::vector4(p.x, p.y, p.z, 0.0f);
					uniform_buffer->SetVector("frustrum_tl", &tmp);
				}

				// Top Right
				{
					auto& p = points[Frustum::TOP_RIGHT_FAR];
					auto tmp = simd::vector4(p.x, p.y, p.z, 0.0f);
					uniform_buffer->SetVector("frustrum_tr", &tmp);
				}

				// Bottom Left
				{
					auto& p = points[Frustum::BOTTOM_LEFT_FAR];
					auto tmp = simd::vector4(p.x, p.y, p.z, 0.0f);
					uniform_buffer->SetVector("frustrum_bl", &tmp);
				}

				// Bottom Right
				{
					auto& p = points[Frustum::BOTTOM_RIGHT_FAR];
					auto tmp = simd::vector4(p.x, p.y, p.z, 0.0f);
					uniform_buffer->SetVector("frustrum_br", &tmp);
				}
			}
		}

		auto* pipeline = FindPipeline("Tiled Light Assign", device, shader);
		if (command_buffer.BindPipeline(pipeline))
		{
			Device::BindingInputs inputs;
			inputs.push_back(Device::BindingInput(Device::IdHash(DrawDataNames::LightDataBuffer)).SetTexture(texture));
			inputs.push_back(Device::BindingInput(Device::IdHash(DrawDataNames::LightIndicesBuffer)).SetTexelBuffer(indices));
			inputs.push_back(Device::BindingInput(Device::IdHash(DrawDataNames::TiledLightIndex)).SetByteAddressBuffer(index_counter));
			inputs.push_back(Device::BindingInput(Device::IdHash(DrawDataNames::PointLightData)).SetStructuredBuffer(light_data));

			auto* binding_set = FindBindingSet("Tiled Light Assign", device, shader, inputs);
			auto* descriptor_set = FindDescriptorSet("Tiled Light Assign", device, pipeline, binding_set);

			command_buffer.BindDescriptorSet(descriptor_set, {}, { uniform_buffer });
			command_buffer.DispatchThreads((unsigned)(dyn_num_tiles_x + 0.5f), (unsigned)(dyn_num_tiles_y + 0.5f));
		}

		device->EndEvent(command_buffer);
	}

	void TiledLighting::BindInputs(Device::BindingInputs& pass_binding_inputs) const
	{
		pass_binding_inputs.push_back(Device::BindingInput(Device::IdHash(DrawDataNames::LightDataBuffer)).SetTexture(texture));
		pass_binding_inputs.push_back(Device::BindingInput(Device::IdHash(DrawDataNames::LightIndicesBuffer)).SetTexelBuffer(indices));
		pass_binding_inputs.push_back(Device::BindingInput(Device::IdHash(DrawDataNames::PointLightData)).SetStructuredBuffer(light_data));
		pass_binding_inputs.push_back(Device::BindingInput(Device::IdHash(DrawDataNames::PointLightInfos)).SetStructuredBuffer(point_light_infos));
	}

	void TiledLighting::OnCreateDevice(Device::IDevice* device)
	{
		tiles_assign_shader = CreateCachedHLSLAndLoad(device, "TiledLightAssign", LoadShaderSource(L"Shaders/TiledLightAssign.hlsl"), nullptr, "mainCS", Device::COMPUTE_SHADER);
		light_data = Device::StructuredBuffer::Create("Point Light Positions", device, sizeof(Scene::PointLightInfo), Scene::PointLightDataMaxCount, Device::FrameUsageHint(), Device::Pool::MANAGED, nullptr, false);
		point_light_infos = Device::StructuredBuffer::Create("Point Light Infos", device, sizeof(Scene::PointLightInfo), Scene::PointLightInfoMaxCount, Device::FrameUsageHint(), Device::Pool::MANAGED, nullptr, false);
		index_counter = Device::ByteAddressBuffer::Create("Point Light Atomic Counter", device, 4, Device::UsageHint::ATOMIC_COUNTER, Device::Pool::MANAGED, nullptr, true);
	}

	void TiledLighting::OnResetDevice(Device::IDevice* device)
	{
		const auto _frame_size = device->GetFrameSize();
		num_tiles_x = (_frame_size.x + TILE_SIZE - 1) / TILE_SIZE;
		num_tiles_y = (_frame_size.y + TILE_SIZE - 1) / TILE_SIZE;
		frame_size = simd::vector2((float)_frame_size.x, (float)_frame_size.y);
		max_num_indices = std::min(MAX_LIGHTS_PER_TILE * num_tiles_x * num_tiles_y, (unsigned int)(1 << 16)); // Point Light Tiles Texture is in G16G16_UINT format, so it can't store an index offset above 2^16-1

		texture = Device::Texture::CreateTexture("Point Light Tiles", device, (UINT)num_tiles_x, (UINT)num_tiles_y, 1, Device::UsageHint::DEFAULT, Device::PixelFormat::G16R16_UINT, Device::Pool::DEFAULT, false, false, false, true);
		indices = Device::TexelBuffer::Create("Point Light Indices", device, (UINT)max_num_indices, Device::TexelFormat::R16_UInt, Device::UsageHint::DEFAULT, Device::Pool::DEFAULT, nullptr, true);
	}

	void TiledLighting::OnLostDevice()
	{
		texture.Reset();
		indices.Reset();

		uniform_buffers.Clear();
		descriptor_sets.Clear();
		binding_sets.Clear();
		pipelines.Clear();
	}

	void TiledLighting::OnDestroyDevice()
	{
		light_data.Reset();
		point_light_infos.Reset();
		index_counter.Reset();
		tiles_assign_shader.Reset();
	}
}