#pragma once

#include "ClientInstanceCommon/Animation/AnimationEvent.h"
#include "ClientInstanceCommon/Animation/AnimationObjectComponent.h"
#include "ClientInstanceCommon/Animation/Components/AnimationControllerComponentListener.h"
#include "Common/Utility/Listenable.h"
#include "ClientInstanceCommon/Animation/Components/AnimationControllerComponent.h"

namespace Animation
{
	namespace Components
	{
		using Objects::Components::ComponentType;
		
		class SoundParamsStatic : public AnimationObjectStaticComponent
		{
		public:
			COMPONENT_POOL_DECL()

			SoundParamsStatic( const Objects::Type& parent );

			virtual void CopyAnimationData( const int source_animation, const int destination_animation, std::function<bool( const Animation::Event& )> predicate, std::function<void( const Animation::Event&, Animation::Event& )> transform ) override;

			bool SetValue( const std::string& name, const std::wstring& value ) override;
			std::vector< ComponentType > GetDependencies() const override { return { ComponentType::AnimationController, ComponentType::AnimatedRender }; }
			static const char* GetComponentName();
			virtual ComponentType GetType() const override { return ComponentType::SoundParams; }
			void SaveComponent( std::wostream& stream ) const override;

			struct SoundParamEvent : public Animation::Event
			{
				SoundParamEvent() : Event( SoundParamEventType, 0 ) { }
				std::wstring name;
				uint64_t hash = 0;
				float value;
			};

			std::vector< std::unique_ptr< SoundParamEvent > > events;

			void AddSoundParamEvent( std::unique_ptr< SoundParamEvent > e );
			void RemoveSoundParamEvent( const SoundParamEvent& e );

			unsigned animation_controller_index;
			unsigned sound_events_index;
			unsigned current_animation;
			mutable float instance_count = 0.f;
		};


		class SoundParams : public AnimationObjectComponent, AnimationControllerListener
		{
		public:
			typedef SoundParamsStatic StaticType;

			SoundParams( const SoundParamsStatic& static_ref, Objects::Object& );
			~SoundParams();

		private:
			void OnAnimationEvent( const AnimationController& controller, const Event& ) override;

			const StaticType& static_ref;

			Utility::DontCopy<Animation::Components::AnimationController::ListenerPtr> anim_cont_list_ptr;
		};
	}
}
