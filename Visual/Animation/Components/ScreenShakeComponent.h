#pragma once

#include "ClientInstanceCommon/Animation/AnimationEvent.h"
#include "ClientInstanceCommon/Animation/AnimationObjectComponent.h"
#include "Visual/Animation/Components/ClientAnimationControllerComponentListener.h"
#include "Visual/Renderer/Scene/Camera.h"

namespace Animation
{
	namespace Components
	{
		using Objects::Components::ComponentType;
		
		class ScreenShakeStatic : public AnimationObjectStaticComponent
		{
		public:
			COMPONENT_POOL_DECL()

			ScreenShakeStatic( const Objects::Type& parent );

			bool SetValue( const std::string& name, const std::wstring& value ) override;
			std::vector< ComponentType > GetDependencies() const override { return { ComponentType::AnimationController, ComponentType::AnimatedRender }; }
			static const char* GetComponentName( );
			virtual ComponentType GetType() const override { return ComponentType::ScreenShake; }
			void SaveComponent( std::wostream& stream ) const override;

			virtual void CopyAnimationData( const int source_animation, const int destination_animation, std::function<bool( const Animation::Event& )> predicate, std::function<void( const Animation::Event&, Animation::Event& )> transform ) override;

			struct ScreenShakeEffect : public Animation::Event
			{
				ScreenShakeEffect( ) : Event( ScreenShakeEventType, 0 ) { }
				float frequency, amplitude, duration;
				bool lateral, global, always_on;
				Renderer::Scene::Camera::ShakeDampMode damp;
				unsigned bone_group_index;
			};

			std::vector< std::unique_ptr< ScreenShakeEffect > > events;

			void AddScreenShakeEvent( std::unique_ptr< ScreenShakeEffect > e );
			void RemoveScreenStakeEvent( const ScreenShakeEffect& e );

			unsigned render_index;
			unsigned animation_controller_index;
			unsigned client_animation_controller_index;
			unsigned bone_groups_index;

			unsigned current_animation;
		};


		class ScreenShake : public AnimationObjectComponent, ClientAnimationControllerListener
		{
		public:
			typedef ScreenShakeStatic StaticType;

			ScreenShake( const ScreenShakeStatic& static_ref, Objects::Object& );

		private:
			void OnAnimationEvent( const ClientAnimationController& controller, const Event& e, float time_until_trigger) override;

			const StaticType& static_ref;
		};
	}
}
