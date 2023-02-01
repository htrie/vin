#pragma once
#include <memory>
#include "Common/Objects/StaticComponent.h"
#include "ClientInstanceCommon/Animation/AnimationEvent.h"
#include "ClientInstanceCommon/Animation/Components/AnimationControllerComponentListener.h"
#include "ClientInstanceCommon/Animation/Components/AnimationControllerComponent.h"
#include "ClientInstanceCommon/Animation/AnimationObjectComponent.h"
#include "Visual/Material/MaterialAtlas.h"
#include "Common/Utility/Listenable.h"

namespace Decals
{
	class DecalManager;
}

namespace Mesh
{
	class AnimatedSkeleton;
	typedef Resource::Handle< AnimatedSkeleton > AnimatedSkeletonHandle;
}


namespace Animation
{
	namespace Components
	{
		using Objects::Components::ComponentType;
		
		class ClientAnimationControllerStatic;
		class DecalEventsStatic : public AnimationObjectStaticComponent
		{
		public:
			COMPONENT_POOL_DECL()

			DecalEventsStatic( const Objects::Type& parent );

			std::vector< ComponentType > GetDependencies() const override { return { ComponentType::AnimatedRender }; }
			void SaveComponent( std::wostream& stream ) const override;

			static const char* GetComponentName( );
			virtual ComponentType GetType() const override { return ComponentType::DecalEvents; }

			virtual void CopyAnimationData( const int source_animation, const int destination_animation, std::function<bool( const Animation::Event& )> predicate, std::function<void( const Animation::Event&, Animation::Event& )> transform ) override;

			bool SetValue( const std::string& name, const std::wstring& value ) override;
			bool SetValue(const std::string &name, const float value) override;
			bool SetValue(const std::string &name, const bool value) override;

			ClientAnimationControllerStatic* GetController() const;

			struct DecalEvent : public Animation::Event
			{
				DecalEvent( const float time, const unsigned animation_index, const Materials::AtlasGroupHandle& texture, const float size, const unsigned bone_index, const float fade_in_time, const std::wstring& filename, const bool scale_with_actor = true );
				Materials::AtlasGroupHandle texture;
				float size = 1.0f;
				float fade_in_time = 0.0f;
				unsigned bone_index = -1;
				bool scale_with_actor = true;
				std::wstring filename;
			};

			std::vector< std::unique_ptr< DecalEvent > > decal_events;
			std::unique_ptr< DecalEvent > create_event;

			const unsigned animation_controller;
			const unsigned animated_render;
			const unsigned client_animation_controller;

			float size_variation_maximum_increase = 0.0f;
			bool random_rotation = true;
			bool jitter = true;

			///Used only during loading of the type. Indicates which animation we are currently setting sound events on.
			unsigned current_animation = -1;

			void RemoveEvent( const DecalEvent& sound_event );
			void AddEvent( const float time, const unsigned animation_index, const Materials::AtlasGroupHandle& texture, const float size, const unsigned bone_index, const float fade_in_time, const std::wstring& filename, const bool scale_with_actor = true );
			void AddEvent( std::unique_ptr< DecalEventsStatic::DecalEvent > decal_event );
			std::unique_ptr< DecalEventsStatic::DecalEvent > GetDecalEvent( const std::wstring& str, const float time ) const;
		};

		class DecalEvents : public AnimationObjectComponent, AnimationControllerListener
		{
		public:
			typedef DecalEventsStatic StaticType;

			DecalEvents( const DecalEventsStatic& static_ref, Objects::Object& object );

			void SetDecalManager( Decals::DecalManager& decal_manager );
			void CreateDecal( const Event& e ) const;

			void ResetCreated() { created = false; }

			const DecalEventsStatic& static_ref;

		private:
			virtual void OnAnimationEvent( const AnimationController& controller, const Event& e ) override { CreateDecal( e ); }

			Decals::DecalManager* decal_manager = nullptr;
			bool created = false;

			Utility::DontCopy<Animation::Components::AnimationController::ListenerPtr> anim_cont_list_ptr;
		};
	}
}