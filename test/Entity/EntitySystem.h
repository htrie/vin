#pragma once

#include "Common/Utility/System.h"

#include "Visual/Entity/Component.h"

class BoundingBox;

namespace simd
{
	class matrix;
}

namespace Device
{
	class IDevice;
	struct UniformInput;
}

namespace Renderer
{
	namespace DrawCalls
	{
		class DrawCall;
	}
}

namespace Utility
{
	enum class ViewMode : unsigned;
}

namespace Entity
{
	struct Draw
	{
		uint64_t sub_hash = 0;
		Visibility visibility = 0;
		Renderer::DrawCalls::DrawCall* call = nullptr;

		Draw() {}
		Draw(uint64_t sub_hash, Visibility visibility, Renderer::DrawCalls::DrawCall* call)
			: sub_hash(sub_hash), visibility(visibility), call(call) {}
	};

	struct Batch
	{
		Memory::SmallVector<Draw, 64, Memory::Tag::Entities> draws;
		Visibility visibility;
	};

	typedef std::function<void(const std::shared_ptr<Template>&, const Batch&)> BatchDrawFunc;
	typedef std::function<void(Cull&, const Template&, Render&)> DrawCallFunc;
	typedef std::function<void(Particle&, Cull&)> ParticleFunc;
	typedef std::function<void(Trail&, Cull&)> TrailFunc;
	typedef std::function<void(Cull&, Render&)> CullFunc;
	typedef std::function<bool(const Template&, Render&)> RenderFunc;


	struct Stats
	{
		size_t entity_count = 0;
		size_t cull_count = 0;
		size_t template_count = 0;
		size_t render_count = 0;
		size_t particle_count = 0;
		size_t trail_count = 0;

		size_t batch_count = 0;
		size_t sub_batch_count = 0;
		size_t draw_call_count = 0;

		float batch_size_min = 0.0f;
		float batch_size_avg = 0.0f;
		float batch_size_max = 0.0f;
		std::array<float, 16> batch_size_histogram;

		float sub_batch_size_min = 0.0f;
		float sub_batch_size_avg = 0.0f;
		float sub_batch_size_max = 0.0f;
		std::array<float, 16> sub_batch_size_histogram;

		size_t command_used_size = 0;
		size_t command_reserved_size = 0;
		size_t temp_used_size = 0;
		size_t temp_reserved_size = 0;
	};

	template <typename T>
	class Temp;

	class Impl;

	class System : public ImplSystem<Impl, Memory::Tag::Entities>
	{
		System();

	public:
		static System& Get();

		void DisableAsyncCommands(unsigned frame_count);

		void Swap() final;
		void GarbageCollect() final;
		void Clear() final;

		void OnCreateDevice(Device::IDevice* device);
		void OnResetDevice(Device::IDevice* device);
		void OnLostDevice();
		void OnDestroyDevice();

		void Regather();

		Stats GetStats() const;

		uint64_t GetNextEntityID();

		std::pair<void*, bool> Bump(uint64_t entity_id, size_t size); // Internal only.

		void Create(uint64_t entity_id, Temp<Desc>&& desc);
		void Destroy(uint64_t entity_id);
		void Move(uint64_t entity_id, const BoundingBox& bounding_box, bool visible, Temp<Renderer::DrawCalls::Uniforms>&& uniforms, unsigned instance_count_x = 1, unsigned instance_count_y = 1, unsigned instance_count_z = 1);
		void SetVisible(uint64_t entity_id, bool visible);

		void Tweak(unsigned hash, const Renderer::DrawCalls::Uniform& uniform); // Slow (tools only).

		std::atomic_uint& Update(ParticleFunc particle_func, TrailFunc trail_func, CullFunc cull_func, RenderFunc render_func);
		void Process(DrawCallFunc draw_call_func); // Slow, debug only.

		void ListSort();
		void ListProcess(Renderer::DrawCalls::BlendMode blend_mode, BatchDrawFunc draw_func);
	};


	template <typename T>
	class Temp
	{
		void* storage = nullptr;
		bool owned = false;

	public:
		Temp(uint64_t entity_id)
		{
			std::tie(storage, owned) = System::Get().Bump(entity_id, sizeof(T));
			new (storage) T();
		}

		explicit Temp(T* storage) = delete;

		~Temp()
		{
			reset();
		}

		Temp()
		{}
		Temp(std::nullptr_t)
		{}
		Temp& operator=(std::nullptr_t)
		{
			reset();
			return *this;
		}

		Temp(Temp&& moving) noexcept
		{
			moving.swap(*this);
		}
		Temp& operator=(Temp&& moving) noexcept
		{
			moving.swap(*this);
			return *this;
		}

		Temp(Temp const&) = delete;
		Temp& operator=(Temp const&) = delete;

		T* operator->() const { return (T*)storage; }
		T& operator*() const { return *(T*)storage; }

		T* get() const { return storage; }
		explicit operator bool() const { return !!storage; }

		void swap(Temp& src) noexcept
		{
			std::swap(storage, src.storage);
			std::swap(owned, src.owned);
		}

		void reset()
		{
			if (storage)
			{
				((T*)storage)->~T();
				if (owned)
					Memory::Free(storage);
				storage = nullptr;
				owned = false;
			}
		}
	};

	template<typename T>
	void swap(Temp<T>& lhs, Temp<T>& rhs)
	{
		lhs.swap(rhs);
	}


}
