#pragma once

#include "ClientInstanceCommon/Animation/AnimationEvent.h"
#include "ClientInstanceCommon/Animation/AnimationObjectComponent.h"
#include "ClientInstanceCommon/Animation/Components/AnimationControllerComponentListener.h"
#include "Common/Utility/Listenable.h"
#include "ClientInstanceCommon/Animation/Components/AnimationControllerComponent.h"
#include "Common/Simd/Curve.h"

namespace Animation
{
	namespace Components
	{
		using Objects::Components::ComponentType;
		
		class TimelineParametersStatic : public AnimationObjectStaticComponent
		{
		public:
			COMPONENT_POOL_DECL()

			TimelineParametersStatic( const Objects::Type& parent );

			virtual void CopyAnimationData( const int source_animation, const int destination_animation, std::function<bool( const Animation::Event& )> predicate, std::function<void( const Animation::Event&, Animation::Event& )> transform ) override;

			bool SetValue( const std::string& name, const std::wstring& value ) override;
			std::vector< ComponentType > GetDependencies() const override { return { ComponentType::AnimationController }; }
			static const char* GetComponentName();
			virtual ComponentType GetType() const override { return ComponentType::TimelineParameters; }
			void SaveComponent( std::wostream& stream ) const override;

			struct TimelineParameterEvent : public Animation::Event
			{
				TimelineParameterEvent() : Event( TimelineParameterEventType, 0 ) { }
				std::string name;
				unsigned data_id = 0;
				simd::GenericCurve< 3 > curve;
				simd::GenericCurve< 3 >::Track track;
				bool apply_on_children = false;

#ifndef PRODUCTION_CONFIGURATION
				Objects::Origin origin = Objects::Origin::Client;
#endif
			};

			std::vector< std::unique_ptr< TimelineParameterEvent > > events;

			void AddTimelineParameterEvent( std::unique_ptr< TimelineParameterEvent > e );
			void RemoveTimelineParameterEvent( const TimelineParameterEvent& e );

			unsigned animation_controller_index;
			unsigned current_animation;
		};

		class TimelineParameters : public AnimationObjectComponent, AnimationControllerListener
		{
		public:
			typedef TimelineParametersStatic StaticType;

			TimelineParameters( const TimelineParametersStatic& static_ref, Objects::Object& );
			~TimelineParameters();

			void RenderFrameMove(const float elapsed_time);
			static void RenderFrameMoveNoVirtual(AnimationObjectComponent* component, const float elapsed_time);
			RenderFrameMoveFunc GetRenderFrameMoveFunc() const final { return RenderFrameMoveNoVirtual; }

			void RemoveEvent( const TimelineParametersStatic::TimelineParameterEvent& e );

		private:
			void RenderFrameMoveInternal(const float elapsed_time);
			void OnAnimationEvent( const AnimationController& controller, const Event& ) override;

			const StaticType& static_ref;

			std::vector< const TimelineParametersStatic::TimelineParameterEvent* > active_events;
			Utility::DontCopy< Animation::Components::AnimationController::ListenerPtr > anim_cont_list_ptr;
			bool prev_parent_active = false;
			bool prev_child_active = false;
		};
	}
}
