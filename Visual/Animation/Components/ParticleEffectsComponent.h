#pragma once

#include "ClientInstanceCommon/Animation/AnimationEvent.h"
#include "ClientInstanceCommon/Animation/AnimationObjectComponent.h"
#include "Visual/Animation/Components/ClientAnimationControllerComponentListener.h"
#include "Visual/Animation/AnimationSystem.h"
#include "Visual/Particles/ParticleEffect.h"
#include "Visual/Particles/EffectInstance.h"
#include "Visual/GpuParticles/GpuParticleSystem.h"

namespace Renderer
{
	class DynamicParameterLocalStorage;
}

namespace Particles
{
	typedef Memory::Vector<std::shared_ptr<EffectInstance>, Memory::Tag::Effects> EffectInstances;
}

namespace Physics
{
	class WindSystem;
}

namespace Pool
{
	class Pool;
}

namespace Animation
{
	namespace Components
	{
		using Objects::Components::ComponentType;
		
		class ParticleEffectsStatic : public AnimationObjectStaticComponent
		{
		public:
			COMPONENT_POOL_DECL()

			ParticleEffectsStatic( const Objects::Type& parent );
			~ParticleEffectsStatic();

			virtual void CopyAnimationData( const int source_animation, const int destination_animation, std::function<bool( const Animation::Event& )> predicate, std::function<void( const Animation::Event&, Animation::Event& )> transform ) override;

			static const char* GetComponentName( );
			virtual ComponentType GetType() const override { return ComponentType::ParticleEffects; }
			std::vector< ComponentType > GetDependencies() const override { return { ComponentType::BoneGroups, ComponentType::AnimatedRender }; }
			void SaveComponent( std::wostream& stream ) const override;
			bool SetValue(const std::string &name, const std::wstring &value) override;
			bool SetValue(const std::string &name, const bool value) override;
			void OnTypeInitialised() override;

			struct ContinuousParticleEffect
			{
				unsigned bone_group_index;
				Particles::ParticleEffectHandle effect;

#ifndef PRODUCTION_CONFIGURATION
				Objects::Origin origin = Objects::Origin::Client;
#endif
			};

			std::vector< ContinuousParticleEffect > continuous_particle_effects;

			struct ParticleEffectEvent : public Animation::Event
			{
				ParticleEffectEvent( ) : Event( ParticleEffectEventType, 0 ) { }
				Memory::SmallVector< unsigned, 1, Memory::Tag::Components > bone_group_indices;
				float length = 0.0f;
				Particles::ParticleEffectHandle effect;
			};

			std::vector< std::unique_ptr< ParticleEffectEvent > > particle_effect_events;

			void RemoveParticleEvent(const ParticleEffectEvent& particle_event);
			void AddParticleEvent(std::unique_ptr< ParticleEffectEvent > particle_event);

			unsigned animation_controller_index;
			unsigned bone_groups_index;
			unsigned animated_render_index;
			unsigned client_animation_controller_index; ///< Used for particles attached to bones rather than bone groups.

			unsigned current_animation; ///< Current animation that events will be added to. Only used during loading.

			bool tick_when_not_visible;

		private:
			struct ParsedEffects;
			Memory::Pointer<ParsedEffects, Memory::Tag::Components> parsed_effects;
		};

		class ParticleEffects : public AnimationObjectComponent, ClientAnimationControllerListener
		{
		public:
			typedef ParticleEffectsStatic StaticType;

			ParticleEffects( const ParticleEffectsStatic& static_ref, Objects::Object& );
			~ParticleEffects();

			void SetupSceneObjects(uint8_t scene_layers, Renderer::DynamicParameterLocalStorage& dynamic_parameters_storage,
				const Renderer::DrawCalls::Bindings& static_bindings, const Renderer::DrawCalls::Uniforms& static_uniforms);
			void DestroySceneObjects();

			void RenderFrameMove( const float elapsed_time );
			static void RenderFrameMoveNoVirtual( AnimationObjectComponent* component, const float elapsed_time );
			RenderFrameMoveFunc GetRenderFrameMoveFunc() const final { return RenderFrameMoveNoVirtual; }

			void OnSleep() override;
			static void OnSleepNoVirtual(AnimationObjectComponent* component);
			OnSleepFunc GetOnSleepFunc() const final { return OnSleepNoVirtual; }

			bool Empty() const { return continuous_particle_effects.empty() && particle_event_effects.empty() && infinite_particle_event_effects.empty() && bone_group_effects.empty() && bone_effects.empty(); }

			void SetAnimationStorageID(const Animation::DetailedStorageID& id) { animation_storage_id = id; }

			///Destroys and recreates any continuous emitters on the object
			///Particle event emitters are destroyed but not recreated
			void RegenerateEmitters();

			///Reinitialises all continuous emitters on the object.
			///Also reinitialises any infinite emitters from events.
			void ReinitialiseEmitters();

			///Removes all continuous effects playing on the object.
			///Particle events will continue
			///If RegenerateEmitters is called, the continous effects will come back
			void RemoveContinuousEffects();
			void RemoveOtherEffects();

			void RemoveAllEffects();

			// Call this if the AO was teleportated or otherwise moved in a way that should reset particle effects
			void OnTeleport();

			template <typename F>
			void ProcessAllEffects(F func)
			{
				for (auto& instance : continuous_particle_effects)
					func(instance);

				for (auto& event : particle_event_effects)
					func(event.effect);

				for (auto& event : infinite_particle_event_effects)
					func(event.effect);

				for (auto& group : bone_group_effects)
					func(group.effect);

				for (auto& bone : bone_effects)
					func(bone.effect);
			}

			///Manually plays an effect on a bone group by name.
			void PlayEffect( const std::string& bone_group_name, const Particles::ParticleEffectHandle& effect, float delay = 0.0f);
			///Manually plays an effect on a bone group
			///@return The created EffectInstance. This can be used for controlling later.
			/// Note: can return effect that wasn't actually added if game is minimized
			std::shared_ptr< Particles::EffectInstance > PlayEffect( const unsigned bone_group_index, const Particles::ParticleEffectHandle& effect, float delay = 0.0f);
			std::shared_ptr< Particles::EffectInstance > PlayEffectOnBone( const unsigned bone_index, const Particles::ParticleEffectHandle& effect, float delay = 0.0f);
			///Manually plays an effect on a bone.
			///@param at is the direction in world space that the particle effect is oriented towards.
			///If the object moves during the effect the direction of the effect 
			///continues to the point in the same direction relative to the model that it started at.
			void PlayEffectInDirection( const unsigned bone_index, const Particles::ParticleEffectHandle& effect, const simd::vector3& direction_world, const float dynamic_scale = 1.0f, float delay = 0.0f);

		private:
			void RenderFrameMoveInternal( const float elapsed_time );

			void OnSleepInternal();

			void OnConstructionComplete() override { RegenerateEmitters(); }

			///Plays a particle event effect
			void PlayEffect( const unsigned bone_group_index, const ParticleEffectsStatic::ParticleEffectEvent& particle_event, const Particles::ParticleEffectHandle& effect, float delay, bool allow_finite, bool allow_infinite);

			void OnAnimationEvent( const ClientAnimationController& controller, const Event&, float time_until_trigger) final;
			void OnAnimationStart( const CurrentAnimation& animation, const bool blend ) final;
			void OnAnimationSpeedChange( const CurrentAnimation& animation ) final;
			void OnAnimationPositionProgressed( const CurrentAnimation& animation, const float previous_time, const float new_time ) final;

			void OrphanEffects();

			Particles::EffectInstances& ContinuousEffects() { return continuous_particle_effects; }

			Particles::EffectInstances OtherEffects()
			{
				Particles::EffectInstances instances;

				for (auto& event : particle_event_effects)
					instances.push_back(event.effect);

				for (auto& event : infinite_particle_event_effects)
					instances.push_back(event.effect);

				for (auto& group : bone_group_effects)
					instances.push_back(group.effect);

				for (auto& bone : bone_effects)
					instances.push_back(bone.effect);

				return instances;
			}

			///Continuous particle effects. Indices are the same as in the static class.
			Particles::EffectInstances continuous_particle_effects;

			///A playing effect event.
			struct EffectEventInstance
			{
				EffectEventInstance() { }
				EffectEventInstance( EffectEventInstance&& other ) : bone_group( other.bone_group ), particle_event( other.particle_event ), effect( std::move( other.effect ) ) { }
				EffectEventInstance& operator=( EffectEventInstance&& other ) { bone_group = other.bone_group; particle_event = other.particle_event; effect = std::move( other.effect ); return *this; }

				const ParticleEffectsStatic::ParticleEffectEvent* particle_event;
				unsigned bone_group;
				std::shared_ptr< Particles::EffectInstance > effect;
			};

			std::vector< EffectEventInstance > particle_event_effects;
			std::vector< EffectEventInstance > infinite_particle_event_effects;

			///Represents an effect that is currently playing on a bone group
			struct BoneGroupEffectInstance
			{
				BoneGroupEffectInstance() { }
				BoneGroupEffectInstance( BoneGroupEffectInstance&& other ) : attach_index( other.attach_index ), effect( std::move( other.effect ) ) { }
				BoneGroupEffectInstance& operator=( BoneGroupEffectInstance&& other ) { attach_index = other.attach_index; effect = std::move( other.effect ); return *this; }

				unsigned attach_index;
				std::shared_ptr< Particles::EffectInstance > effect;
			};

			///Effects playing on bone groups.
			std::vector< BoneGroupEffectInstance > bone_group_effects;

			///Represents an effect that is currently playing on a bone
			struct BoneEffectInstance
			{
				BoneEffectInstance() { }
				BoneEffectInstance( BoneEffectInstance&& other ) : attach_index( other.attach_index ), effect( std::move( other.effect ) ), at( other.at ), dynamic_scale( other.dynamic_scale ) { }
				BoneEffectInstance& operator=( BoneEffectInstance&& other ) { attach_index = other.attach_index; effect = std::move( other.effect ); at = other.at; dynamic_scale = other.dynamic_scale; return *this; }

				simd::vector3 at; ///< Indicates the position that the effect should face towards in model space.
				unsigned attach_index;
				float dynamic_scale = 1.0f; // Only for use with DynamicPointEmitter
				std::shared_ptr< Particles::EffectInstance > effect;
			};

			///Effects playing on bones.
			std::vector< BoneEffectInstance > bone_effects;

			const ParticleEffectsStatic& static_ref;

			Animation::DetailedStorageID animation_storage_id;
			float particle_event_anim_speed;
		};
	}
}
