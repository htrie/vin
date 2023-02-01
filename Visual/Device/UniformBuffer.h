#pragma once

#include "Visual/Device/State.h"
#include "Visual/Device/Buffer.h"

namespace Renderer
{
	namespace Scene
	{
		struct Stats;
	}
}

namespace Device
{
	class UniformBuffer
	{
		Device::Handle<Device::StructuredBuffer> data;
		simd::vector4* values = nullptr;
		size_t offset = 0;
		uint32_t frame_index = 0;
		Memory::SpinMutex mutex;

		size_t max_size = 0;
		size_t max_count = 0;

		size_t AllocateSlow(size_t size);
		size_t AllocateLocal(size_t size);

	public:
		void Init(bool tight_buffers);
		void OnCreateDevice(Device::IDevice* _device);
		void OnDestroyDevice();
		Device::UniformAlloc Allocate(size_t size);

		void UpdateStats(Renderer::Scene::Stats& stats) const;

		void Lock();
		void Unlock();
		void AddBindingInputs(Device::BindingInputs& binding_inputs);

		static size_t GetMaxSize();
	};
}