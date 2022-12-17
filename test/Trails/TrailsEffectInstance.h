#pragma once

#include "Visual/Renderer/DynamicParameterManager.h"
#include "Visual/Animation/AnimationSystem.h"

#include "TrailsEffect.h"
#include "TrailSegment.h"

class BoundingBox;

namespace Renderer 
{ 
	namespace DrawCalls
	{
		class Desc;
	}

	namespace Scene { class Camera; }
}

namespace simd
{
	class vector3;
	class matrix;
}

namespace Trails
{
	struct Trail;
	struct TrailDraw;

	class TrailsEffectInstance
	{
	public:
		TrailsEffectInstance( const TrailsEffectHandle& effect, const Memory::SmallVector<simd::vector3, 16, Memory::Tag::Particle>& position, const simd::matrix& world, const Renderer::Scene::Camera* camera, bool high_priority, bool no_default_shader, float delay, float emitter_duration);
		~TrailsEffectInstance( );

		static unsigned GetCount() { return count.load(); }

		void CreateDrawCalls(uint8_t scene_layers, Renderer::DynamicParameterLocalStorage& dynamic_parameters_storage, const Renderer::DrawCalls::Bindings& static_bindings, const Renderer::DrawCalls::Uniforms& static_uniforms);
		void DestroyDrawCalls();
		const bool HasDrawCalls() const { return entities.size() > 0; }
		
		void SetPosition( const Memory::SmallVector<simd::vector3, 16, Memory::Tag::Particle>& position );
		void SetMultiplier(float multiplier);
		void SetEmitterTime(float emitter_time);
		void SetTransform(uint8_t scene_layers, const simd::matrix& transform, Renderer::DynamicParameterLocalStorage& dynamic_parameters_storage, 
			const Renderer::DrawCalls::Bindings& static_bindings, const Renderer::DrawCalls::Uniforms& static_uniforms);
		void SetBoundingBox(const BoundingBox& bounding_box, bool visible);
		void SetVisible(bool visible);

		void SetAnimationStorage(const Animation::DetailedStorageID& id, const Memory::Span<const unsigned>& bone_indices);

		void FrameMove(const float elapsed_time);

		///Returns true if all trails have stopped emitting and no longer have any trail segments alive
		bool IsComplete( ) const;

		///Starts all emitters again
		void Reinitialise();

		void Pause();
		void Resume();
		void OnOrphaned();

		///Causes all emitters to stop emitting new trail segments
		void StopEmitting(float stop_time = -1.0f);
		bool IsStopped() const { return is_stopped; }

		void SetAnimationSpeedMultiplier(float animspeed_multiplier);

		const TrailsEffectHandle effect;

	private:
		static inline std::atomic_uint count = { 0 };

		struct EntityInfo
		{
			uint64_t entity_id;
			Renderer::DynamicParameters dynamic_parameters;
			std::shared_ptr<Trail> trail; // Just for tools to easier update some uniforms

			EntityInfo(uint64_t entity_id)
				: entity_id(entity_id) {}
		};
		Memory::SmallVector<EntityInfo, 4, Memory::Tag::DrawCalls> entities;

		simd::vector3 position;
		std::vector< SegmentVertex > offsets;

		simd::matrix emitter_transform;

		std::vector< std::shared_ptr< Trail > > trails;

		Memory::SmallVector<simd::vector3, 16, Memory::Tag::Particle> last_position;
		Animation::DetailedStorageID animation_storage_id;
		Memory::SmallVector<unsigned, 16, Memory::Tag::Trail> animation_bone_indices;
		bool is_stopped;

		bool high_priority = false;
		bool no_default_shader = false;
	};

}