#pragma once

#include "ClientInstanceCommon/Animation/AnimationEvent.h"
#include "ClientInstanceCommon/Animation/AnimationObjectComponent.h"
#include "ClientInstanceCommon/Animation/Components/AnimationControllerComponentListener.h"
#include "Visual/Physics/WindSource.h"

namespace Pool
{
	class Pool;
}

namespace Animation
{
	namespace Components
	{
		class WindEventsStatic : public AnimationObjectStaticComponent
		{
		public:
			COMPONENT_POOL_DECL()

			WindEventsStatic( const Objects::Type& parent );

			static const char* GetComponentName( );
			virtual Objects::Components::ComponentType GetType() const override { return Objects::Components::ComponentType::WindEvents; }
			std::vector< Objects::Components::ComponentType > GetDependencies() const override { return { Objects::Components::ComponentType::BoneGroups, Objects::Components::ComponentType::AnimatedRender }; }
			void SaveComponent( std::wostream& stream ) const override;
			bool SetValue( const std::string& name, const std::wstring& value ) override;
			bool SetValue( const std::string& name, const float value ) override;
			bool SetValue( const std::string& name, const int value ) override;
			bool SetValue( const std::string& name, const bool value ) override;

			struct WindEventDescriptor : public Animation::Event
			{
				WindEventDescriptor( );
				~WindEventDescriptor(){}
				WindEventDescriptor(WindEventDescriptor &&) = default;
				WindEventDescriptor& operator=(WindEventDescriptor &&) = default;

				int bone_index;
				int bone_group_index;
				int animation_index;
				Physics::Vector3f size;
				struct Field
				{
					Field(){velocity = 1000.0f; wavelength = 500.0f;}
					float velocity;
					float wavelength;
				};
				Field primary_field;
				Field secondary_field;
				float duration;
				float initial_phase;

				Physics::Coords3f local_coords;

				bool is_attached;
				bool is_persistent;
				bool is_continuous;
				bool scales_with_object;
				Physics::WindSourceTypes::Type wind_shape_type;
				//wind source description
			};

			std::vector< std::unique_ptr< WindEventDescriptor > > wind_event_descriptors;
			std::vector< std::unique_ptr< WindEventDescriptor > > wind_continuous_descriptors;
			
			void RemoveEventDesc( const WindEventDescriptor* wind_event );
			WindEventsStatic::WindEventDescriptor *AddEventDesc( std::unique_ptr< WindEventsStatic::WindEventDescriptor > wind_event_desc );

			unsigned animation_controller_index;
			unsigned animated_render_index;
			unsigned client_animation_controller_index; ///< Used for particles attached to bones rather than bone groups.
			unsigned bone_groups_index;

			WindEventDescriptor *current_event;
			unsigned current_animation; ///< Current animation that events will be added to. Only used during loading.
		};

		class WindEvents : public AnimationObjectComponent, AnimationControllerListener
		{
		public:
			typedef WindEventsStatic StaticType;

			WindEvents( const WindEventsStatic& static_ref, Objects::Object& );
			~WindEvents();

			void SetWindSystem(Physics::WindSystem *wind_system);

			void ClearActiveWindEventInstances();

			virtual bool HasFrameMove() override { return true; }
			void FrameMove(const float elapsed_time) override;

			void RenderFrameMove( const float elapsed_time );
			static void RenderFrameMoveNoVirtual( AnimationObjectComponent* component, const float elapsed_time );
			virtual RenderFrameMoveFunc GetRenderFrameMoveFunc() const final { return RenderFrameMoveNoVirtual; }

		private:

			void RenderFrameMoveInternal( const float elapsed_time );

			void ExecuteEvent(const WindEventsStatic::WindEventDescriptor & event_descriptor);
			
			///Plays a particle event effect

			void OnAnimationEvent( const AnimationController& controller, const Event& );
			void GetSourceGeom(const WindEventsStatic::WindEventDescriptor & event_desc, Physics::Vector3f & size, Physics::Coords3f & coords, Physics::Vector3f & dir);
			void GetSourceSizeAndCoords(const WindEventsStatic::WindEventDescriptor & event_desc, Physics::Vector3f &size, Physics::Coords3f &coords);
			void AddWindEventInstance(const WindEventsStatic::WindEventDescriptor & event_desc);
			void OnAnimationStart( const CurrentAnimation& animation, const bool blend );


			///Represents an effect that is currently playing on a bone
			struct WindEventInstance
			{
				WindEventInstance() { }
				WindEventInstance( WindEventInstance&& other ) : wind_event_desc( other.wind_event_desc ), wind_source( std::move( other.wind_source ) ) { }
				WindEventInstance& operator=( WindEventInstance&& other ) { wind_event_desc = other.wind_event_desc; wind_source = std::move( other.wind_source ); return *this; }

				const WindEventsStatic::WindEventDescriptor *wind_event_desc;
				Physics::WindSourceHandle wind_source;
			};

			void DetachEventInstance(WindEventInstance &instance);

			///Effects playing on bones.
			std::vector< WindEventInstance > wind_event_instances;
			std::vector< const typename WindEventsStatic::WindEventDescriptor *> wind_event_queue;

			Physics::WindSystem *wind_system;

			const WindEventsStatic& static_ref;
		};
	}
}
