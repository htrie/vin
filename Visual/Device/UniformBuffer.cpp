#include "Common/Utility/Logger/Logger.h"

#include "Visual/Device/Device.h"
#include "Visual/Device/UniformBuffer.h"
#include "Visual/Renderer/DrawCalls/DrawDataNames.h"
#include "Visual/Renderer/Scene/SceneSystem.h"

namespace Device
{
	namespace
	{
		static inline size_t LocalBlockSize = 128 * Memory::KB;
		static inline thread_local size_t local_offset = 0;
		static inline thread_local size_t local_available = 0;
		static inline thread_local unsigned local_frame_index = 0;
	}

	size_t UniformBuffer::AllocateSlow(size_t size)
	{
		Memory::SpinLock lock(mutex);
		if (offset + size > max_size)
		{
			LOG_DEBUG(L"Uniform buffer overflow");
			return (size_t)-1;
		}
		const auto current = offset;
		offset = offset + size;
		return current;
	}

	size_t UniformBuffer::AllocateLocal(size_t size)
	{
		if (size > LocalBlockSize)
		{
			LOG_DEBUG(L"Uniform buffer allocation too big");
			return (size_t)-1;
		}

		if (local_frame_index != frame_index)
		{
			local_offset = 0;
			local_available = 0;
			local_frame_index = frame_index;
		}

		if (size > local_available)
		{
			const auto block_start = AllocateSlow(LocalBlockSize);
			if (block_start == (size_t)-1)
				return (size_t)-1;

			local_offset = block_start;
			local_available = LocalBlockSize;
		}

		const auto current = local_offset;
		local_offset += size;
		local_available -= size;
		return current;
	}

	void UniformBuffer::Init(bool tight_buffers)
	{
	#if defined(MOBILE)
		max_size = 2 * Memory::MB;
	#else
		max_size = (tight_buffers ? 16 : 64) * Memory::MB;
	#endif

		max_count = max_size / sizeof(simd::vector4);
	}

	void UniformBuffer::OnCreateDevice(Device::IDevice* _device)
	{
		data = Device::StructuredBuffer::Create("Uniforms", _device, sizeof(simd::vector4), max_count, Device::FrameUsageHint(), Device::Pool::MANAGED, nullptr, false);
	}

	void UniformBuffer::OnDestroyDevice()
	{
		data.Reset();
	}

	Device::UniformAlloc UniformBuffer::Allocate(size_t size)
	{
		const auto aligned_size = Memory::AlignSize(size, sizeof(simd::vector4));
		const auto alloc_start = AllocateLocal(aligned_size);
		if (alloc_start == (size_t)-1)
			return {};

		const auto current = alloc_start / sizeof(simd::vector4);
		const auto count = aligned_size / sizeof(simd::vector4);
		return { &values[current], (uint32_t)current, (uint32_t)count };
	}

	void UniformBuffer::UpdateStats(Renderer::Scene::Stats& stats) const
	{
		stats.uniforms_size = offset;
		stats.uniforms_max_size = max_size;
	}

	void UniformBuffer::Lock()
	{
		data->Lock(0, max_count * sizeof(simd::vector4), (void**)&values, Device::Lock::DISCARD);
		offset = 0;
		frame_index++;
	}

	void UniformBuffer::Unlock()
	{
		data->Unlock();
	}

	void UniformBuffer::AddBindingInputs(Device::BindingInputs& binding_inputs)
	{
		binding_inputs.push_back(Device::BindingInput(Device::IdHash(Renderer::DrawDataNames::Uniforms)).SetStructuredBuffer(data));
	}

	size_t UniformBuffer::GetMaxSize() { return LocalBlockSize; }
}