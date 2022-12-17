#pragma once

#include "ClientInstanceCommon/Animation/Components/AnimationControllerComponentListener.h"
#include "ClientInstanceCommon/Animation/AnimationObjectComponent.h"
#include "Visual/Mesh/FixedLight.h"

namespace Renderer
{
	namespace Scene 
	{ 
		class PointLight;
		class Light;
	}
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
		
		class LightsStatic : public AnimationObjectStaticComponent
		{
		public:
			COMPONENT_POOL_DECL()

			LightsStatic( const Objects::Type& parent );
			static const char* GetComponentName( );
			virtual ComponentType GetType() const override { return ComponentType::Lights; }
			std::vector< ComponentType > GetDependencies() const override { return { ComponentType::BoneGroups, ComponentType::AnimatedRender }; }

			virtual bool SetValue( const std::string& name, const bool value ) override;
			virtual bool SetValue( const std::string& name, const float value ) override;
			virtual bool SetValue( const std::string& name, const int value ) override;
			virtual bool SetValue( const std::string& name, const std::wstring& value ) override;

			void SaveComponent( std::wostream& stream ) const override;

			struct DynamicLight
			{
				enum Style
				{
					Normal = 0,
					Flicker,
					Pulse,
					Strobe,
					NumStyles
				};

				struct State
				{
					std::string name;
					std::string custom_style;
					simd::vector3 colour;
					simd::vector3 position;
					simd::vector3 direction;
					float radius = 0.0f;
					float penumbra = 0.0f;
					float umbra = 0.0f;
					float dist_threshold = 0.0f;

					float penumbra_dist = 0.5f;
					float penumbra_radius = 0.01f;

					unsigned style = 0;
				};

				struct LightInstance
				{
					State current_state;
					unsigned target_state = 0;
					float time_to_transition = 0.0f;
					float total_transition_time = 0.0f;
					float intensity = 1.0f;
					float elapsed_time = 0.0f;
				};

				std::vector< State > states;
				Renderer::Scene::Light::Type type = Renderer::Scene::Light::Type::Point;
				Renderer::Scene::Light::UsageFrequency usage_frequency = Renderer::Scene::Light::UsageFrequency::Low;
				uint32_t profile = 0u;
				bool casts_shadow = false;
				unsigned default_state = 0;
				float style_speed = 10.0f;
				bool randomise_start_time = false;

#ifndef PRODUCTION_CONFIGURATION
				Objects::Origin origin = Objects::Origin::Client;
#endif

				std::shared_ptr< Renderer::Scene::Light > CreateLight( const LightInstance& light, const simd::matrix& world ) const;
				void UpdateLight( const LightInstance& light, const simd::matrix& world, Renderer::Scene::Light& scene_light ) const;
				void FrameMove( LightInstance& light, const float elapsed_time ) const;
			};

			std::vector< DynamicLight > dynamic_lights;

			unsigned bone_groups_index;
			unsigned animated_render_index;
			unsigned fixed_mesh_index;
			unsigned client_animation_controller_index;
			bool disable_mesh_lights;
		};

		class Lights : public AnimationObjectComponent
		{
		public:
			typedef LightsStatic StaticType;
			Lights( const LightsStatic& static_ref, Objects::Object& );
			~Lights();

			void SetupSceneObjects();
			void DestroySceneObjects();

			void SetDynamicLightState( const std::string& state_name, const float time_to_transition );
			void SetDynamicLightState( const unsigned dynamic_light_index, const std::string& state_name, const float time_to_transition );

			void RenderFrameMove( const float elapsed_time ) override;
			static void RenderFrameMoveNoVirtual( AnimationObjectComponent* component, const float elapsed_time );
			RenderFrameMoveFunc GetRenderFrameMoveFunc() const final { return RenderFrameMoveNoVirtual; }

			auto& GetDynamicLights() { return dynamic_lights; }

			//force lights to be black (colour 0,0,0). will apply next update
			void ForceBlack( const bool black = true ) { force_black = black; }

			const LightsStatic& static_ref;

		private:

			void RenderFrameMoveInternal( const float elapsed_time );

			std::vector< LightsStatic::DynamicLight::LightInstance > dynamic_lights;
			std::vector< std::shared_ptr< Renderer::Scene::Light > > lights;

			bool force_black = false;
		};
	}
}
