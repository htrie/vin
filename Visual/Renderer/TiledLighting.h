#pragma once

#include "Visual/Device/Shader.h"
#include "Visual/Device/Buffer.h"
#include "Visual/Device/Texture.h"
#include "Visual/Device/State.h"

namespace Renderer
{
	namespace Scene
	{
		class Camera;
		class PointLight;
		typedef Memory::SmallVector<PointLight*, 64, Memory::Tag::DrawCalls> PointLights;

		static const unsigned SubSceneMaxCount = 4; // IMPORTANT: Change it here and in Common.ffx as well.

#if defined(MOBILE)
	static const unsigned PointLightDataMaxCount = 64;
#elif defined(CONSOLE)
	static const unsigned PointLightDataMaxCount = 128;
#else
	static const unsigned PointLightDataMaxCount = 256;
#endif
	}

	class TiledLighting
	{
		static constexpr unsigned int MAX_LIGHTS_PER_TILE = 64;
		static constexpr unsigned int TILE_SIZE = 64;

		Device::Handle<Device::StructuredBuffer> light_data;
		Device::Handle<Device::StructuredBuffer> point_light_infos;
		Device::Handle<Device::Texture> texture;
		Device::Handle<Device::TexelBuffer> indices;
		Device::Handle<Device::ByteAddressBuffer> index_counter;
		Device::Handle<Device::Shader> tiles_assign_shader;

		unsigned int light_count = 0;
		unsigned int max_num_indices = 0;
		unsigned int num_tiles_x = 0;
		unsigned int num_tiles_y = 0;

		simd::vector2 frame_size;

	#pragma pack(push)	
	#pragma pack(1)
		struct PipelineKey
		{
			Device::PointerID<Device::Shader> compute_shader;

			PipelineKey() {}
			PipelineKey(Device::Shader* compute_shader) : compute_shader(compute_shader) {}
		};
	#pragma pack(pop)
		Device::Cache<PipelineKey, Device::Pipeline> pipelines;
		Memory::Mutex pipelines_mutex;
		Device::Pipeline* FindPipeline(const Memory::DebugStringA<>& name, Device::IDevice* device, Device::Shader* compute_shader);

	#pragma pack(push)
	#pragma pack(1)
		struct BindingSetKey
		{
			Device::PointerID<Device::Shader> shader;
			uint32_t inputs_hash = 0;

			BindingSetKey() {}
			BindingSetKey(Device::Shader* shader, uint32_t inputs_hash) : shader(shader), inputs_hash(inputs_hash) {}
		};
	#pragma pack(pop)
		Device::Cache<BindingSetKey, Device::BindingSet> binding_sets;
		Memory::Mutex binding_sets_mutex;
		Device::BindingSet* FindBindingSet(const Memory::DebugStringA<>& name, Device::IDevice* device, Device::Shader* compute_shader, const Device::BindingSet::Inputs& inputs);

	#pragma pack(push)
	#pragma pack(1)
		struct DescriptorSetKey
		{
			Device::PointerID<Device::Pipeline> pipeline;
			Device::PointerID<Device::BindingSet> binding_set;

			DescriptorSetKey() {}
			DescriptorSetKey(Device::Pipeline* pipeline, Device::BindingSet* binding_set) : pipeline(pipeline), binding_set(binding_set) {}
		};
	#pragma pack(pop)
		Device::Cache<DescriptorSetKey, Device::DescriptorSet> descriptor_sets;
		Memory::Mutex descriptor_sets_mutex;
		Device::DescriptorSet* FindDescriptorSet(const Memory::DebugStringA<>& name, Device::IDevice* device, Device::Pipeline* pipeline, Device::BindingSet* binding_set);

	#pragma pack(push)
	#pragma pack(1)
		struct UniformBufferKey
		{
			Device::PointerID<Device::Shader> compute_shader;

			UniformBufferKey() {}
			UniformBufferKey(Device::Shader* compute_shader) : compute_shader(compute_shader) {}
		};
	#pragma pack(pop)
		Device::Cache<UniformBufferKey, Device::DynamicUniformBuffer> uniform_buffers;
		Memory::Mutex uniform_buffers_mutex;
		Device::DynamicUniformBuffer* FindUniformBuffer(const Memory::DebugStringA<>& name, Device::IDevice* device, Device::Shader* compute_shader);

		void UpdatePointLightInfos(const Scene::PointLights& point_lights);
		void UpdatePointLightData(const Scene::PointLights& point_lights);
		void UpdateIndexCounter();

	public:
		TiledLighting();

		void Update(Device::IDevice* device, const Scene::PointLights& point_lights);
		void ComputeTiles(Device::IDevice* device, const Scene::Camera& camera, Device::CommandBuffer& command_buffer);
		void BindInputs(Device::BindingInputs& pass_binding_inputs) const;
		
		void OnCreateDevice(Device::IDevice* device);
		void OnResetDevice(Device::IDevice* device);
		void OnLostDevice();
		void OnDestroyDevice();
	};
}