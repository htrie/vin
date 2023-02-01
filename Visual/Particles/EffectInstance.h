#pragma once

#include "Visual/Animation/AnimationSystem.h"
#include "Visual/GpuParticles/GpuParticleSystem.h"
#include "Visual/Renderer/DynamicParameterManager.h"

class BoundingBox;

namespace Device
{
	enum class CullMode : unsigned;
}

namespace Renderer 
{ 
	namespace DrawCalls
	{
		class DrawCall;
		class Desc;
		class Uniforms;
	}

	namespace Scene { class Camera; }
}

namespace simd
{
	class vector3;
	class matrix;
}

namespace Particles
{
	class ParticleEffect;
	class ParticleEmitter;

	typedef Resource::Handle< ParticleEffect > ParticleEffectHandle;

	class EffectInstance
	{
	public:
		EffectInstance( const ParticleEffectHandle& effect, const Memory::SmallVector<simd::vector3, 16, Memory::Tag::Particle>& position, const simd::matrix& world, const Renderer::Scene::Camera* camera, bool high_priority, bool no_default_shader, float delay, float emitter_duration, bool allow_infinite = true, bool allow_finite = true);
		~EffectInstance( );

		static unsigned GetCount() { return count.load(); }

		void CreateDrawCalls(uint8_t scene_layers, Renderer::DynamicParameterLocalStorage& dynamic_parameters_storage,
			const Renderer::DrawCalls::Bindings& static_bindings, const Renderer::DrawCalls::Uniforms& static_uniforms);
		void DestroyDrawCalls();
		const bool HasDrawCalls() const { return entities.size() > 0 || has_gpu_draw_calls; }

		void SetPosition( const Memory::SmallVector<simd::vector3, 16, Memory::Tag::Particle>& );
		void SetMultiplier(float multiplier);
		void SetTransform(uint8_t scene_layers, const simd::matrix& transform, Renderer::DynamicParameterLocalStorage& dynamic_parameters_storage, 
			const Renderer::DrawCalls::Bindings& static_bindings, const Renderer::DrawCalls::Uniforms& static_uniforms);
		void SetBoundingBox(const BoundingBox& bounding_box, bool visible);
		void SetVisible(bool visible);

		void FrameMove(float elapsed_time);

		void SetAnimationStorage(const Animation::DetailedStorageID& id, const Memory::Span<const unsigned>& bone_indices);

		///Returns true if all emitters have stopped emitting and no longer have any particles alive
		bool EffectComplete( ) const;
		bool IsEmitting() const;

		///Starts all emitters again
		void Reinitialise( );

		void OnTeleport();

		void DetachGpuEmitters();

		///Causes all emitters to stop emitting new particles.
		void StopEmitting();
		///Only use this after StopEmitting. Effects are already emitting by default
		void StartEmitting( );

		void SetAnimationSpeedMultiplier( float animspeed_multiplier );

		const ParticleEffectHandle& GetType() const { return effect; }

	private:
		void SetTransform(const simd::matrix& transform);

		void CreateGpuDrawCalls(uint8_t scene_layers, Renderer::DynamicParameterLocalStorage& dynamic_parameters_storage,
			const Renderer::DrawCalls::Bindings& static_bindings, const Renderer::DrawCalls::Uniforms& static_uniforms);

		void CreateCpuDrawCalls(uint8_t scene_layers, Renderer::DynamicParameterLocalStorage& dynamic_parameters_storage,
			const Renderer::DrawCalls::Bindings& static_bindings, const Renderer::DrawCalls::Uniforms& static_uniforms); // TODO: Remove

		void CreateGpuEmitters(float delay, float emitter_duration);
		void DestroyGpuEmitters();

		GpuParticles::EmitterId CreateGpuEmitter(size_t i, float delay, float emitter_duration, GpuParticles::EmitterId emitter_id = GpuParticles::EmitterId());
		GpuParticles::EmitterId CreateCpuEmitter(size_t i, float delay, float emitter_duration, GpuParticles::EmitterId emitter_id = GpuParticles::EmitterId());

		static inline std::atomic_uint count = { 0 };

		struct EntityInfo
		{
			ParticleEmitter& emitter;
			uint64_t entity_id;
			Renderer::DynamicParameters dynamic_parameters;

			EntityInfo(ParticleEmitter& emitter, uint64_t entity_id)
				: emitter(emitter), entity_id(entity_id) {}
		};
		Memory::SmallVector<EntityInfo, 4, Memory::Tag::DrawCalls> entities; // TODO: Remove

		struct EmitterInfo
		{
			size_t index = 0;
			GpuParticles::EmitterId emitter;
			bool converted = false;

			EmitterInfo() = default;
			EmitterInfo(size_t index, const GpuParticles::EmitterId& id, bool converted) : index(index), emitter(id), converted(converted) {}
		};

		ParticleEffectHandle effect;
		Device::CullMode cull_mode;

		Memory::Vector<std::shared_ptr<ParticleEmitter>, Memory::Tag::Particle> emitters; // TODO: Remove
		Memory::Vector<EmitterInfo, Memory::Tag::Particle> gpu_emitters;
		float emit_duration;
		float animation_speed;
		const float seed;
		const bool allow_infinite;
		const bool allow_finite;
		const bool high_priority;
		bool has_gpu_draw_calls;
	};

}