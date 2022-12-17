#pragma once

#include "ClientInstanceCommon/Animation/AnimationEvent.h"
#include "ClientInstanceCommon/Animation/AnimationObjectComponent.h"
#include "Visual/Animation/Components/ClientAnimationControllerComponentListener.h"
#include "Visual/Animation/AnimationSystem.h"
#include "Visual/Trails/TrailsEffect.h"
#include "Visual/Trails/TrailsEffectInstance.h"

namespace Renderer
{
	class DynamicParameterLocalStorage;
}

namespace Trails
{
	typedef Memory::Vector<std::shared_ptr<TrailsEffectInstance>, Memory::Tag::Effects> TrailsEffectInstances;
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
		
		class TrailsEffectsStatic : public AnimationObjectStaticComponent
		{
		public:
			COMPONENT_POOL_DECL()

			TrailsEffectsStatic( const Objects::Type& parent );
			~TrailsEffectsStatic();

			static const char* GetComponentName( );
			virtual ComponentType GetType() const override { return ComponentType::TrailsEffects; }
			std::vector< ComponentType > GetDependencies() const override { return { ComponentType::BoneGroups, ComponentType::AnimatedRender }; }
			void SaveComponent( std::wostream& stream ) const override;
			bool SetValue(const std::string &name, const std::wstring &value) override;
			void OnTypeInitialised() override;

			virtual void CopyAnimationData( const int source_animation, const int destination_animation, std::function<bool( const ::Animation::Event& )> predicate, std::function<void( const ::Animation::Event&, ::Animation::Event& )> transform ) override;

			struct ContinuousTrailsEffect
			{
				unsigned bone_group_index;
				Trails::TrailsEffectHandle effect;

#ifndef PRODUCTION_CONFIGURATION
				Objects::Origin origin = Objects::Origin::Client;
#endif
			};

			std::vector< ContinuousTrailsEffect > continuous_trails_effects;

			struct TrailsEffectEvent : public Animation::Event
			{
				TrailsEffectEvent( ) : Event( TrailsEffectEventType, 0 ) { }
				Memory::SmallVector< unsigned, 1, Memory::Tag::Components > bone_group_indices;
				Trails::TrailsEffectHandle effect;
				float length;
			};

			std::vector< std::unique_ptr< TrailsEffectEvent > > trails_effect_events;

			void RemoveTrailsEvent( const TrailsEffectEvent& trails_event );
			void AddTrailsEvent( std::unique_ptr< TrailsEffectEvent > trails_event );

			unsigned animation_controller_index;
			unsigned bone_groups_index;
			unsigned animated_render_index;
			unsigned client_animation_controller_index; ///< Used for trails attached to bones rather than bone groups.

			unsigned current_animation; ///< Current animation that events will be added to. Only used during loading.

		private:
			struct ParsedEffects;
			Memory::Pointer<ParsedEffects, Memory::Tag::Components> parsed_effects;
		};

		class TrailsEffects : public AnimationObjectComponent, ClientAnimationControllerListener
		{
		public:
			typedef TrailsEffectsStatic StaticType;

			TrailsEffects( const TrailsEffectsStatic& static_ref, Objects::Object& );
			~TrailsEffects();

			void SetupSceneObjects(uint8_t scene_layers, Renderer::DynamicParameterLocalStorage& dynamic_parameters_storage, 
				const Renderer::DrawCalls::Bindings& static_bindings, const Renderer::DrawCalls::Uniforms& static_uniforms);
			void DestroySceneObjects();

			void RenderFrameMove( const float elapsed_time ) override;
			static void RenderFrameMoveNoVirtual( AnimationObjectComponent* component, const float elapsed_time );
			RenderFrameMoveFunc GetRenderFrameMoveFunc() const final { return RenderFrameMoveNoVirtual; }

			void OnSleep() override;
			static void OnSleepNoVirtual(AnimationObjectComponent* component);
			OnSleepFunc GetOnSleepFunc() const final { return OnSleepNoVirtual; }

			bool Empty() const { return continuous_trails_effects.empty() && trails_event_effects.empty() && bone_group_effects.empty() && bone_effects.empty(); }

			///Destroys and recreates any continuous emitters on the object
			///Particle event emitters are destroyed but not recreated
			void RegenerateEmitters( );

			///Reinitialises all continuous emitters on the object.
			///Also reinitialises any infinite emitters from events.
			void ReinitialiseEmitters( );

			///Removes all continuous effects playing on the object.
			///Particle events will continue
			///If RegenerateEmitters is called, the continous effects will come back
			void RemoveContinuousEffects( );
			void RemoveOtherEffects( );

			void SetAnimationStorageID(const Animation::DetailedStorageID& id) { animation_storage_id = id; }

			void RemoveAllEffects();

			///Causes all effects to stop emitting new trails or trail segments. Call ResumeAllEffects to 
			///start emitting again.
			///This will not apply to emitters that were added after this function call
			void PauseAllEffects();
			void ResumeAllEffects();

			// Call this if the AO was teleportated or otherwise moved in a way that should not be connected via trails
			void OnTeleport();

			template <typename F>
			void ProcessAllEffects( F func )
			{
				for( auto& instance : continuous_trails_effects )
					func( instance );

				for( auto& event : trails_event_effects )
					func( event.effect );

				for( auto& group : bone_group_effects )
					func( group.effect );

				for( auto& bone : bone_effects )
					func( bone.effect );
			}

			///Manually plays a trail effect on a bone group
			///@return The created TrailEffectInstance. This can be used for controlling later.
			/// Note: can return effect that wasn't actually added if game is minimized
			std::shared_ptr< Trails::TrailsEffectInstance > PlayEffect( const unsigned bone_group_index, const Trails::TrailsEffectHandle& effect, float delay = 0.0f );
			std::shared_ptr< Trails::TrailsEffectInstance > PlayEffectOnBone( const unsigned bone_index, const Trails::TrailsEffectHandle& effect, float delay = 0.0f);

			Memory::SmallVector<float, 8, Memory::Tag::Trail> GetEffectEndTimes();

		private:
			void RenderFrameMoveInternal( const float elapsed_time );

			void OnSleepInternal();

			void OnConstructionComplete() override { RegenerateEmitters(); }

			///Plays a Trails event effect
			void PlayEffect( const unsigned bone_group_index, const TrailsEffectsStatic::TrailsEffectEvent& trails_event, const Trails::TrailsEffectHandle& effect, float delay = 0.0f);

			void OnAnimationEvent( const ClientAnimationController& controller, const Event&, float time_until_trigger) override final;
			void OnAnimationStart( const CurrentAnimation& animation, const bool blend ) override final;
			void OnAnimationEnd(const ClientAnimationController& controller, const unsigned animation_index, float time_until_trigger) override final;
			void OnAnimationSpeedChange(const CurrentAnimation& animation) override final;
			void OnAnimationPositionProgressed( const CurrentAnimation& animation, const float previous_time, const float new_time ) override final;

			void OrphanEffects();

			Trails::TrailsEffectInstances& ContinuousEffects( ) { return continuous_trails_effects; }

			Trails::TrailsEffectInstances OtherEffects()
			{
				Trails::TrailsEffectInstances instances;

				for( auto& event : trails_event_effects )
					instances.push_back( event.effect );

				for( auto& group : bone_group_effects )
					instances.push_back( group.effect );

				for( auto& bone : bone_effects )
					instances.push_back( bone.effect );

				return instances;
			}

			///Continuous Trails effects. Indices are the same as in the static class.
			Trails::TrailsEffectInstances continuous_trails_effects;

			///A playing effect event.
			struct EffectEventInstance
			{
				EffectEventInstance() { }
				EffectEventInstance( EffectEventInstance&& other ) : bone_group( other.bone_group ), trails_event( other.trails_event ), effect( std::move( other.effect ) ), is_stopped( other.is_stopped ) { }
				EffectEventInstance& operator=( EffectEventInstance&& other ) { bone_group = other.bone_group; trails_event = other.trails_event; effect = std::move( other.effect ); is_stopped = other.is_stopped; return *this; }

				const TrailsEffectsStatic::TrailsEffectEvent* trails_event;
				unsigned bone_group;
				std::shared_ptr< Trails::TrailsEffectInstance > effect;
				float time_until_stop = -1.0f;
				bool is_stopped = false;
			};

			std::vector< EffectEventInstance > trails_event_effects;

			///Represents an effect that is currently playing on a bone group
			struct BoneGroupEffectInstance
			{
				BoneGroupEffectInstance() { }
				BoneGroupEffectInstance( BoneGroupEffectInstance&& other ) : attach_index( other.attach_index ), effect( std::move( other.effect ) ) { }
				BoneGroupEffectInstance& operator=( BoneGroupEffectInstance&& other ) { attach_index = other.attach_index; effect = std::move( other.effect ); return *this; }

				unsigned attach_index;
				std::shared_ptr< Trails::TrailsEffectInstance > effect;
			};

			///Effects playing on bone groups.
			std::vector< BoneGroupEffectInstance > bone_group_effects;

			struct BoneEffectInstance
			{
				BoneEffectInstance() { }
				BoneEffectInstance( BoneEffectInstance&& other ) : attach_index( other.attach_index ), effect( std::move( other.effect ) ), at( other.at ) { }
				BoneEffectInstance& operator=( BoneEffectInstance&& other ) { attach_index = other.attach_index; effect = std::move( other.effect ); at = other.at; return *this; }

				simd::vector3 at; ///< Indicates the position that the effect should face towards in model space.
				unsigned attach_index;
				std::shared_ptr< Trails::TrailsEffectInstance > effect;
			};

			///Effects playing on bones.
			std::vector< BoneEffectInstance > bone_effects;

			const TrailsEffectsStatic& static_ref;

			Animation::DetailedStorageID animation_storage_id;
			float trail_event_anim_speed = 1.f;
		};
	}
}
