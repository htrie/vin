#pragma once

#include "Common/Utility/System.h"

#include "Visual/Device/State.h"
#include "Visual/Device/Resource.h"
#include "Visual/Device/VertexDeclaration.h"
#include "Visual/Renderer/DrawCalls/EffectGraphInstance.h"
#include "Visual/Renderer/Scene/Camera.h"
#include "Visual/GpuParticles/GpuEmitterTemplate.h"

namespace Renderer
{
	namespace DrawCalls
	{
		class Desc;
	}
}

namespace Device
{
	class IDevice;
	class VertexBuffer;
	class IndexBuffer;
	class StructuredBuffer;
}

namespace Particles
{
	class EmitterTemplate;
}

namespace GpuParticles
{
	class Impl;

	struct Stats
	{
		size_t num_particles = 0;
		size_t max_particles = 0;
		size_t num_emitters = 0;
		size_t num_bones = 0;
		size_t max_bones = 0;
		size_t num_visible_emitters = 0;
		size_t num_allocated_emitters = 0;
		size_t num_allocated_slots = 0;
		size_t num_used_slots = 0;
	};

	typedef std::function<float(const AABB&)> CullPriorityCallback; // Larger value cause it to be culled first. Negative value disables culling

	class System : public ImplSystem<Impl, Memory::Tag::Particle>
	{
		System();

	public:
		void OnCreateDevice(Device::IDevice* _device);
		void OnDestroyDevice();

		static System& Get();

		void Swap() override final;
		void Clear() override final;

		Stats GetStats() const;

		void SetEnableConversion(bool enable); // TODO: This is just for debugging and will be removed once all CPU particles were converted

		using EmitterId = Memory::SparseId<uint32_t>;
		using AllocationId = Memory::SparseId<uint32_t>;
		
		struct BonePosition
		{
			simd::vector3 pos;
			float distance = 0.0f;

			BonePosition() = default;
			BonePosition(const BonePosition&) = default;
			BonePosition(const simd::vector3& p) : pos(p), distance(0.0f) {}
		};

		struct EmitterData
		{
			simd::matrix transform;
			float emitter_duration = 1.0f;
			float seed = 0.0f;
			float animation_speed = 1.0f;
			float bounding_size = 500.0f; // in cm
			float emitter_time = -1.0f;
			float start_time = -100.0f;
			bool is_alive = true;
			bool is_active = true;
			bool is_visible = true;
			bool is_teleported = false;

			AllocationId instance_block_id;
			Memory::SmallVector<BonePosition, 8, Memory::Tag::Unknown> bone_positions;
		};

		void Warmup(const EmitterTemplate& templ);
		void Warmup(const Particles::EmitterTemplate& templ);

		void KillOrphaned();

		EmitterId AddEmitter(const EmitterTemplate& templ, float duration = -1.0f, float delay = 0.0f);
		EmitterId AddEmitter(const Particles::EmitterTemplate& templ, float duration = -1.0f, float delay = 0.0f);
		void RemoveEmitter(const EmitterId& emitter_id);
		EmitterData* GetEmitter(const EmitterId& emitter_id);

		void CreateDrawCalls(uint8_t scene_layers, const EmitterTemplate& templ, const EmitterId& emitter_id, Renderer::DynamicParameterLocalStorage& dynamic_parameters_storage,
							 const Renderer::DrawCalls::Bindings& static_bindings, const Renderer::DrawCalls::Uniforms& static_uniforms, Device::CullMode cull_mode = Device::CullMode::CCW);

		void CreateDrawCalls(uint8_t scene_layers, const Particles::EmitterTemplate& templ, const EmitterId& emitter_id, Renderer::DynamicParameterLocalStorage& dynamic_parameters_storage,
							 const Renderer::DrawCalls::Bindings& static_bindings, const Renderer::DrawCalls::Uniforms& static_uniforms, Device::CullMode cull_mode = Device::CullMode::CCW);

		void DestroyDrawCalls(const EmitterId& emitter_id);

		void FrameMove(float delta_time, const CullPriorityCallback& compute_visibility);
	};

	using EmitterData = System::EmitterData;
	using Emitter = System::EmitterId;
}
