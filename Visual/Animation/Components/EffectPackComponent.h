#pragma once

#include "ClientInstanceCommon/Animation/AnimationObjectComponent.h"
#include "ClientInstanceCommon/Animation/AnimationEvent.h"

#include "Visual/Animation/Components/ClientAnimationControllerComponentListener.h"
#include "Visual/Effects/EffectPack.h"

namespace Particles
{
	class EffectInstance;
}

namespace Trails
{
	class TrailsEffectInstance;
}

namespace Mesh
{
	class AnimatedSkeleton;
	typedef Resource::Handle< AnimatedSkeleton > AnimatedSkeletonHandle;
}

namespace Animation
{
	class AppliedEffectPackHandle
	{
	public:
		AppliedEffectPackHandle( const std::shared_ptr< AnimatedObject >& ao, const Visual::EffectPackHandle& effect_pack );
		AppliedEffectPackHandle( AppliedEffectPackHandle&& ) = default;
		AppliedEffectPackHandle& operator=( AppliedEffectPackHandle&& );
		~AppliedEffectPackHandle();

		std::shared_ptr< AnimatedObject > GetAO() const { return ao.lock(); }
		const Visual::EffectPackHandle& GetEffectPack() const { return effect_pack; }

	private:
		AppliedEffectPackHandle( const AppliedEffectPackHandle& ) = delete;
		AppliedEffectPackHandle& operator=( const AppliedEffectPackHandle& ) = delete;

		std::weak_ptr< AnimatedObject > ao;
		Visual::EffectPackHandle effect_pack;
	};

	namespace Components
	{
		class BoneGroups;
		class ClientAnimationController;

		using Objects::Components::ComponentType;
		
		class EffectPackStatic : public AnimationObjectStaticComponent
		{
		public:
			COMPONENT_POOL_DECL()

			EffectPackStatic( const Objects::Type& parent );

			virtual void CopyAnimationData( const int source_animation, const int destination_animation, std::function<bool( const Animation::Event& )> predicate, std::function<void( const Animation::Event&, Animation::Event& )> transform ) override;

			bool SetValue( const std::string& name, const std::wstring& value ) override;
			bool SetValue( const std::string& name, const bool value ) override;
			std::vector< ComponentType > GetDependencies() const override { return { ComponentType::AnimatedRender, ComponentType::ParticleEffects, ComponentType::BoneGroups }; }
			static const char* GetComponentName( );
			virtual ComponentType GetType() const override { return ComponentType::EffectPack; }
			void SaveComponent( std::wostream& stream ) const override;

			unsigned render_index;
			unsigned particle_effects_index;
			unsigned bone_groups_index;
			unsigned trail_effects_index;
			unsigned animation_controller_index;
			unsigned client_animation_controller_index;

			unsigned current_animation; ///< Current animation that events will be added to. Only used during loading.

			bool disable_effect_packs = false;

			std::vector< Visual::EffectPackHandle > effect_packs;

			struct EffectPackEvent : public Animation::Event
			{
				EffectPackEvent() : Event( EffectPackEventType, 0 ) { }
				Visual::EffectPackHandle effect_pack;
				float length = 0.0f;
				bool remove_on_animation_end = true;
				std::string bone_name;  // If specified, the EPK is applied to the first object attached to this bone instead
			};

			std::vector< std::unique_ptr< EffectPackEvent > > effect_pack_events;

			void RemoveEffectPackEvent( const EffectPackEvent& effect_pack_event );
			void AddEffectPackEvent( std::unique_ptr< EffectPackEvent > effect_pack_event );
		};


		class EffectPack : public AnimationObjectComponent, ClientAnimationControllerListener
		{
		public:
			typedef EffectPackStatic StaticType;

			EffectPack( const EffectPackStatic& static_ref, Objects::Object& object );
			~EffectPack();

			void OnConstructionComplete() override;

			// ignore_bad used for cheats, shader gathering.
			void AddEffectPack( const Visual::EffectPackHandle& effect_pack, const bool ignore_bad = false, const bool filter_submesh = false, const float remove_after_duration = 0.0f, const simd::matrix& attached_transform = simd::matrix::identity(), const bool remove_on_animation_end = false );
			bool RemoveEffectPack( const Visual::EffectPackHandle& effect_pack, const bool ignore_bad = false );
			void RemoveAllEffectPacks();
			bool HasEffectPack( const Visual::EffectPackHandle& effect_pack );
			float GetEffectPackDuration( const Visual::EffectPackHandle& effect_pack );
			unsigned NumEffectPacks() const { return (unsigned)effect_packs.size(); }

			Visual::EffectPackHandle GetEffectPack( const unsigned index ) const { return effect_packs[ index ].effect_pack; }
			unsigned GetEffectPackRefCount( const unsigned index ) const { return effect_packs[ index ].ref_count; }
			float GetEffectPackDuration( const unsigned index ) const { return effect_packs[ index ].remove_after_duration; }

			virtual bool HasFrameMove() override { return true; }
			virtual void FrameMove( const float elapsed_time ) override;

			void GetComponentInfo( std::wstringstream& stream, const bool rescursive, const unsigned recursive_depth ) const override;

			// Metamorphosis monsters, don't use it in other cases!
			void SetIgnoreBad( const bool _ignore_bad = true ) { always_ignore_bad = _ignore_bad; }
	
			bool IsEffectsEnabled() const { return enabled; }
			void EnableEffects();
			void DisableEffects( const std::vector< Visual::EffectPackHandle >& exceptions );

		private:
			friend class AnimatedObjectViewer;

			struct RefCountedEffect
			{
				Visual::EffectPackHandle effect_pack;
				unsigned ref_count;
				Memory::Vector< std::shared_ptr< Particles::EffectInstance >, Memory::Tag::Effects > particle_effects;
				Memory::Vector< std::shared_ptr< AnimatedObject >, Memory::Tag::Effects > attached_objects;
				Memory::Vector< std::pair< std::weak_ptr< AnimatedObject >, std::shared_ptr< AnimatedObject > >, Memory::Tag::Effects > child_attached_objects;  // Parent, Child
				Memory::Vector< std::shared_ptr< Trails::TrailsEffectInstance >, Memory::Tag::Effects > trail_effects;
				float remove_after_duration = 0.0f;
				float remove_after_duration_or_animation_end = 0.0f;
				float play_start_epk_after_delay = 0.0f;
				bool filter_submesh = false;
				bool remove_on_animation_end = false;
				simd::matrix attached_transform;

				RefCountedEffect( ) { }
				RefCountedEffect( RefCountedEffect&& o ) 
					: effect_pack( std::move( o.effect_pack ) )
					, ref_count( o.ref_count )
					, particle_effects( std::move( o.particle_effects ) )
					, attached_objects( std::move( o.attached_objects ) )
					, child_attached_objects( std::move( o.child_attached_objects ) )
					, trail_effects(std::move(o.trail_effects) ) 
					, remove_after_duration( o.remove_after_duration )
					, play_start_epk_after_delay( o.play_start_epk_after_delay )
					, attached_transform( o.attached_transform )
					, filter_submesh( o.filter_submesh )
					, remove_on_animation_end( o.remove_on_animation_end )
				{ 
				}

				RefCountedEffect& operator= (RefCountedEffect && o)
				{
					if (this != &o)
					{
						effect_pack = std::move(o.effect_pack);
						ref_count = std::move(o.ref_count);
						particle_effects = std::move(o.particle_effects);
						attached_objects = std::move(o.attached_objects);
						child_attached_objects = std::move(o.child_attached_objects);
						trail_effects = std::move(o.trail_effects);
						remove_after_duration = o.remove_after_duration;
						attached_transform = o.attached_transform;
						filter_submesh = o.filter_submesh;
						remove_on_animation_end = o.remove_on_animation_end;
					}

					return *this;
				}
			};

			struct ChildEffectPack
			{
				AppliedEffectPackHandle applied_epk;
				float remove_after_duration = 0.0f;
				float remove_after_duration_or_animation_end = 0.0f;
				bool remove_on_animation_end = false;

				bool IsFinished() const { return remove_after_duration == 0.0f && remove_after_duration_or_animation_end == 0.0f && !remove_on_animation_end; }
			};

			void HandleError( const RefCountedEffect& effect, const std::string& error_msg ) const;
			bool TestBone( 
				const RefCountedEffect& effect
				, const bool ignore_bad
				, const BoneGroups* bone_groups_component
				, const ClientAnimationController* controller
				, const std::string_view bone_group_name
				, std::function< bool( const std::string& name, int, bool ) > add_entry
				, bool default_to_specific_bone = false
				, bool include_aux = true
				, bool include_sockets = true
			);

			bool RemoveEffectPackInternal( const Memory::Vector< RefCountedEffect, Memory::Tag::Effects >::iterator existing, const bool ignore_bad = false );
			void AddEffectPackVisuals( RefCountedEffect& effect, const bool ignore_bad_final );
			void RemoveEffectPackVisuals( RefCountedEffect& effect );

			void OnAnimationEvent( const ClientAnimationController& controller, const Event&, float time_until_trigger) final;
			void OnAnimationStart( const CurrentAnimation& animation, const bool blend ) override;
			void OnAnimationEnd( const ClientAnimationController& controller, const unsigned animation_index, float time_until_trigger) override;
			void RemoveEffectPacksOnAnimationEnd();

			void AddChildEffectPack( const Visual::EffectPackHandle& effect_pack, const std::string& bone_name, const float remove_after_duration = 0.0f, const bool remove_on_animation_end = false );

			Memory::Vector< RefCountedEffect, Memory::Tag::Effects > effect_packs;
			Memory::Vector< ChildEffectPack, Memory::Tag::Effects > child_effect_packs;

			const StaticType& static_ref;

			// This should never really be set, it is used for metamorphing monsters because of the common occurence of adding a buff specific to a variety, 
			// then switching animated objects and the failing to re-apply the buff visuals
			bool always_ignore_bad = false;
			
			bool enabled = true;
			// Storage of epks that are allowed to be enabled, even when the above enabled bool is set to false (which removes all visuals from epks)
			std::vector< Visual::EffectPackHandle > enabled_epk_exceptions;
		};

	}
}