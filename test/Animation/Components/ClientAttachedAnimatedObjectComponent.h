#pragma once

#include <optional>

#include "Common/Utility/Memory/Memory.h"
#include "Common/Simd/Curve.h"
#include "ClientInstanceCommon/Animation/AnimationEvent.h"
#include "ClientInstanceCommon/Animation/Components/AttachedAnimatedObjectComponent.h"
#include "Visual/Animation/Components/ClientAnimationControllerComponentListener.h"

namespace Animation
{
	namespace Components
	{
		using Objects::Components::ComponentType;
		
		class ClientAttachedAnimatedObjectStatic : public AttachedAnimatedObjectStatic
		{
		public:
			COMPONENT_POOL_DECL()

			ClientAttachedAnimatedObjectStatic( const Objects::Type& parent );

			void SaveComponent( std::wostream& stream ) const override;
			bool SetValue( const std::string& name, const std::wstring& value ) override;
			bool SetValue( const std::string& name, const float value ) override;
			bool SetValue( const std::string& name, const int value ) override;

			void CopyAnimationData( const int source_animation, const int destination_animation, std::function<bool( const Animation::Event& )> predicate, std::function<void( const Animation::Event&, Animation::Event& )> transform ) override;

			// TEMP -- Converts bone index to bone name.
			void FixupBoneName( std::string& name ) final;

			enum class SpeedType
			{
				Fixed,		// Constant speed
				Inherit,
				Stretch,	// Linear curve
				Curve,		// Custom curve
			};

			struct AttachmentEndpoint
			{
				std::string bone_name;
				std::string child_bone_name; // If specified, attach to this bone on any child AO that is attached to bone_name
				std::optional< unsigned > index_within_bone_group; // If specified, bone_name is a bone group
				std::optional< unsigned > index_within_child_bone_group; // If specified, child_bone_name is a bone group
			};

			struct AttachedAnimatedObjectEvent : public Animation::Event
			{
				AttachedAnimatedObjectEvent() : Event( AttachedAnimatedObjectEventType, 0 ) { }
				AnimatedObjectTypeHandle type;
				unsigned animation = -1;
				float length = 0.0f;
				float speed = 1.0f;
				SpeedType speed_type = SpeedType::Fixed;
				simd::matrix offset = simd::matrix::identity();  // Not allowed with beam events
				AttachmentEndpoint from = { "<root>" };
				AttachmentEndpoint to;  // If specified (bone_name is not empty), attach as a beam

				simd::GenericCurve< 3 > playback_curve;
				simd::GenericCurve< 3 >::Track playback_track;

#if defined( DEVELOPMENT_CONFIGURATION ) || defined( TESTING_CONFIGURATION )
				// Asset Viewer only - keep track of the most recently played AO to allow live editing
				mutable std::weak_ptr< AnimatedObject > active_event_parent;
				mutable std::weak_ptr< AnimatedObject > active_event_ao;
#endif
			};

			std::vector< std::unique_ptr< AttachedAnimatedObjectEvent > > attached_object_events;

			void RemoveAttachedAnimatedObjectEvent( const AttachedAnimatedObjectEvent& event );
			void AddAttachedAnimatedObjectEvent( std::unique_ptr< AttachedAnimatedObjectEvent > event );
		};

		class ClientAttachedAnimatedObject : public AttachedAnimatedObject
		{
		public:
			using StaticType = ClientAttachedAnimatedObjectStatic;
			using SpeedType = StaticType::SpeedType;

			ClientAttachedAnimatedObject( const ClientAttachedAnimatedObjectStatic& static_ref, Objects::Object& );

			void SetAttachedObjectVisibilityString( const Memory::Vector<std::wstring, Memory::Tag::Mesh>& names, const bool show ) override;
			void SetAttachedObjectVisibilityString( const AttachedObject& object, const Memory::Vector<std::wstring, Memory::Tag::Mesh>& names, const bool show ) override;

			using ChildAttachedCallback = std::function< void( const std::shared_ptr< AnimatedObject >& parent, const std::shared_ptr< AnimatedObject >& ao ) >;  // If parent is null, the AO was attached to the current object
			void AddChildAttachedObjects( const AnimatedObjectTypeHandle& type, const simd::matrix& transform, const StaticType::AttachmentEndpoint& from_endpoint, const StaticType::AttachmentEndpoint& to_endpoint, ChildAttachedCallback callback );

			void UpdateBeam( const AttachedObject& object );

			void RenderFrameMove( const float elapsed_time ) final;
			static void RenderFrameMoveNoVirtual( AnimationObjectComponent* component, const float elapsed_time );
			RenderFrameMoveFunc GetRenderFrameMoveFunc() const final { return RenderFrameMoveNoVirtual; }

		private:
			void RenderFrameMoveInternal( const float elapsed_time );

			void UpdateAttachedRenderEnabled();

			void OnAnimationStart( const AnimationController& controller, const CurrentAnimation& animation, const bool blend ) override;
			void OnAnimationEnd( const AnimationController& controller, const unsigned animation_index, const unsigned animation_unique_id ) override;
			void OnAnimationLooped( const AnimationController& controller, const unsigned animation_index ) override;
			void OnAnimationEvent( const AnimationController& controller, const Event& ) final;
			void OnAnimationEventTriggered( AnimatedObject& object, const AttachedAnimatedObjectStatic::AnimatedObjectEvent& e ) override;
			void OnObjectRemoved( const AttachedObject& object ) override;

			void RemoveInterruptibleEffects();

			const ClientAttachedAnimatedObjectStatic& static_ref;

			struct PlayingBeam
			{
				std::weak_ptr< AnimatedObject > from;
				std::weak_ptr< AnimatedObject > to;
				unsigned from_bone_index;
				unsigned to_bone_index;
				simd::vector3 scale;
				std::optional< unsigned > from_index_within_bone_group;
				std::optional< unsigned > to_index_within_bone_group;
			};

			struct PlaybackControlledEffect
			{
				std::weak_ptr< AnimatedObject > parent;
				std::weak_ptr< AnimatedObject > effect;
				const StaticType::AttachedAnimatedObjectEvent* attach_event;
			};

			Memory::VectorMap< AnimatedObject*, PlayingBeam, Memory::Tag::Components > playing_beams;
			Memory::Vector< PlaybackControlledEffect, Memory::Tag::Components > playback_controlled_effects;
		};

		template< typename T >
		bool IsWeakPtrNull( const std::weak_ptr< T >& weak )
		{
			return !weak.owner_before( std::weak_ptr< T >() ) && !std::weak_ptr< T >().owner_before( weak );
		}
	}
}
