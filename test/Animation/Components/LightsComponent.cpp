#include "LightsComponent.h"

#include "Common/Game/IsClient.h"
#include "ClientInstanceCommon/Animation/AnimatedObject.h"
#include "ClientInstanceCommon/Animation/Components/AnimationControllerComponent.h"
#include "Visual/Renderer/Scene/Light.h"
#include "Visual/Renderer/Scene/SceneSystem.h"
#include "Visual/Animation/Components/ComponentsStatistics.h"
#include "BoneGroupsComponent.h"
#include "AnimatedRenderComponent.h"
#include "FixedMeshComponent.h"
#include "ClientAnimationControllerComponent.h"

namespace Animation
{
	namespace Components
	{
		//-----Lights Static-----


		LightsStatic::LightsStatic( const Objects::Type& parent )
			: AnimationObjectStaticComponent( parent ),
			bone_groups_index( parent.GetStaticComponentIndex< BoneGroupsStatic >() ),
			animated_render_index( parent.GetStaticComponentIndex< AnimatedRenderStatic >() ),
			fixed_mesh_index( parent.GetStaticComponentIndex< FixedMeshStatic >() ),
			client_animation_controller_index( parent.GetStaticComponentIndex< ClientAnimationControllerStatic >( ) ),
			disable_mesh_lights( false )
		{
		}

		bool LightsStatic::SetValue( const std::string& name, const bool value )
		{
			if( name == "disabled" || name == "disable_mesh_lights" )
			{
				disable_mesh_lights = value;
				return true;
			}
			if( name == "default_state" && !dynamic_lights.empty() && !dynamic_lights.back().states.empty() )
			{
				if( value )
					dynamic_lights.back().default_state = (unsigned)dynamic_lights.back().states.size() - 1;
				return true;
			}
			if( name == "casts_shadow" && !dynamic_lights.empty() )
			{
				dynamic_lights.back().casts_shadow = value;
				return true;
			}
			if( name == "randomise_start_time" && !dynamic_lights.empty() )
			{
				dynamic_lights.back().randomise_start_time = value;
				return true;
			}
			return false;
		}

		bool LightsStatic::SetValue( const std::string& name, const float value )
		{
			if( name == "radius" && !dynamic_lights.empty() && !dynamic_lights.back().states.empty() )
			{
				dynamic_lights.back().states.back().radius = value;
				return true;
			}
			if( name == "penumbra" && !dynamic_lights.empty() && !dynamic_lights.back().states.empty() )
			{
				dynamic_lights.back().states.back().penumbra = value;
				return true;
			}
			if( name == "umbra" && !dynamic_lights.empty() && !dynamic_lights.back().states.empty() )
			{
				dynamic_lights.back().states.back().umbra = value;
				return true;
			}
			if (name == "penumbra_dist" && !dynamic_lights.empty() && !dynamic_lights.back().states.empty())
			{
				dynamic_lights.back().states.back().penumbra_dist = value;
				return true;
			}
			if (name == "penumbra_radius" && !dynamic_lights.empty() && !dynamic_lights.back().states.empty())
			{
				dynamic_lights.back().states.back().penumbra_radius = value;
				return true;
			}

			if( name == "distance_threshold" && !dynamic_lights.empty() && !dynamic_lights.back().states.empty() )
			{
				dynamic_lights.back().states.back().dist_threshold = value;
				return true;
			}
			if( name == "style_speed" && !dynamic_lights.empty() )
			{
				dynamic_lights.back().style_speed = value;
				return true;
			}
			return false;
		}

		bool LightsStatic::SetValue( const std::string& name, const int value )
		{
			if( name == "usage_frequency" && !dynamic_lights.empty() )
			{
				dynamic_lights.back().usage_frequency = static_cast< Renderer::Scene::PointLight::UsageFrequency >( value );
				return true;
			}
			if( name == "profile" && !dynamic_lights.empty() )
			{
				dynamic_lights.back().profile = value;
				return true;
			}
			if( name == "style" && !dynamic_lights.empty() && !dynamic_lights.back().states.empty() )
			{
				dynamic_lights.back().states.back().style = value;
				return true;
			}
			return SetValue( name, float( value ) );
		}

		bool LightsStatic::SetValue( const std::string& name, const std::wstring& value )
		{
			if( name == "point_light" )
			{
				std::wstringstream ss( value );

				DynamicLight light;
				light.type = Renderer::Scene::Light::Type::Point;
				light.casts_shadow = true;
				light.states.emplace_back();

				auto& state = light.states.back();
				ss >> state.position.x >> state.position.y >> state.position.z;
				ss >> state.direction.x >> state.direction.y >> state.direction.z;
				ss >> state.colour.x >> state.colour.y >> state.colour.z;
				ss >> state.radius >> state.dist_threshold;
				ss >> light.profile;
				unsigned usage_freq = 0;
				ss >> usage_freq;
				light.usage_frequency = static_cast< Renderer::Scene::Light::UsageFrequency >( usage_freq );

				if( state.dist_threshold == 0.0f )
					state.dist_threshold = state.radius * 0.1f;

				if( ss.fail() )
					throw Resource::Exception() << L"Failed to parse fixed light parameters";

#ifndef PRODUCTION_CONFIGURATION
				light.origin = parent_type.current_origin;
#endif
				dynamic_lights.push_back( std::move( light ) );
				return true;
			}
			if( name == "spot_light" )
			{
				std::wstringstream ss( value );

				DynamicLight light;
				light.type = Renderer::Scene::Light::Type::Spot;
				light.casts_shadow = true;
				light.states.emplace_back();

				auto& state = light.states.back();
				ss >> state.position.x >> state.position.y >> state.position.z;
				ss >> state.direction.x >> state.direction.y >> state.direction.z;
				ss >> state.colour.x >> state.colour.y >> state.colour.z;
				ss >> state.radius >> state.penumbra >> state.umbra;
				ss >> state.penumbra_dist >> state.penumbra_radius;
				ss >> light.casts_shadow;

				if( ss.fail() )
					throw Resource::Exception() << L"Failed to parse fixed light parameters";

#ifndef PRODUCTION_CONFIGURATION
				light.origin = parent_type.current_origin;
#endif
				dynamic_lights.push_back( std::move( light ) );
				return true;
			}
			if( name == "light" )
			{
				if( value == L"point_light" )
				{
					DynamicLight light;
					light.type = Renderer::Scene::Light::Type::Point;
					light.casts_shadow = true;
#ifndef PRODUCTION_CONFIGURATION
					light.origin = parent_type.current_origin;
#endif
					dynamic_lights.push_back( std::move( light ) );
					return true;
				}
				if( value == L"spot_light" )
				{
					DynamicLight light;
					light.type = Renderer::Scene::Light::Type::Spot;
#ifndef PRODUCTION_CONFIGURATION
					light.origin = parent_type.current_origin;
#endif
					dynamic_lights.push_back( std::move( light ) );
					return true;
				}
				throw Resource::Exception() << L"Invalid light type: " << value;
			}
			if( name == "state" && !dynamic_lights.empty() )
			{
				const std::string state_name = WstringToString( value );
				if( AnyOfIf( dynamic_lights.back().states, [&]( const DynamicLight::State& state ) { return state.name == state_name; } ) )
					throw Resource::Exception() << L"Light state name is already used";
				dynamic_lights.back().states.emplace_back();
				dynamic_lights.back().states.back().name = state_name;
				return true;
			}
			if( name == "colour" && !dynamic_lights.empty() && !dynamic_lights.back().states.empty() )
			{
				std::wstringstream ss( value );
				ss >> dynamic_lights.back().states.back().colour.x >> dynamic_lights.back().states.back().colour.y >> dynamic_lights.back().states.back().colour.z;
				return true;
			}
			if( name == "position" && !dynamic_lights.empty() && !dynamic_lights.back().states.empty() )
			{
				std::wstringstream ss( value );
				ss >> dynamic_lights.back().states.back().position.x >> dynamic_lights.back().states.back().position.y >> dynamic_lights.back().states.back().position.z;
				return true;
			}
			if( name == "direction" && !dynamic_lights.empty() && !dynamic_lights.back().states.empty() )
			{
				std::wstringstream ss( value );
				ss >> dynamic_lights.back().states.back().direction.x >> dynamic_lights.back().states.back().direction.y >> dynamic_lights.back().states.back().direction.z;
				return true;
			}
			if( name == "custom_style" && !dynamic_lights.empty() && !dynamic_lights.back().states.empty() )
			{
				dynamic_lights.back().states.back().style = DynamicLight::NumStyles;
				dynamic_lights.back().states.back().custom_style = WstringToString( value );
				return true;
			}
			return false;
		}

		COMPONENT_POOL_IMPL(LightsStatic, Lights, 64)

		const char* LightsStatic::GetComponentName( )
		{
			return "Lights";
		}

		void LightsStatic::SaveComponent( std::wostream& stream ) const
		{
#ifndef PRODUCTION_CONFIGURATION
			const auto* parent = parent_type.parent_type->GetStaticComponent< LightsStatic >();
			assert( parent );

			WriteValueConditional( disable_mesh_lights, disable_mesh_lights != parent->disable_mesh_lights );

			for( const auto& light : dynamic_lights )
			{
				if( light.origin != parent_type.current_origin )
					continue;

				if( light.type == Renderer::Scene::Light::Type::Point )
					WriteCustom( stream << L"light = \"point_light\"" );
				else if( light.type == Renderer::Scene::Light::Type::Spot )
					WriteCustom( stream << L"light = \"spot_light\"" );
				else
					continue;

				PushIndent();

				if( light.type == Renderer::Scene::Light::Type::Point )
				{
					WriteValueNamedConditional( L"usage_frequency", static_cast< unsigned >( light.usage_frequency ), light.usage_frequency != Renderer::Scene::PointLight::UsageFrequency::Low );
					WriteValueNamedConditional( L"profile", light.profile, light.profile > 0 );
				}
				if( light.type == Renderer::Scene::Light::Type::Spot )
				{
					WriteValueNamedConditionalV( L"casts_shadow", light.casts_shadow );
				}

				WriteValueNamedConditional( L"style_speed", light.style_speed, light.style_speed != 10.0f );
				WriteValueNamedConditionalV( L"randomise_start_time", light.randomise_start_time );

				for( const auto& [idx, state] : enumerate( light.states ) )
				{
					WriteValueNamed( L"state", state.name );

					PushIndent();
					WriteValueNamedConditional( L"default_state", true, idx > 0 && idx == light.default_state );
					WriteCustomConditional( stream << L"colour = \"" << state.colour.x << L" " << state.colour.y << L" " << state.colour.z << L"\"", state.colour.sqrlen() > 0.0f );
					WriteCustomConditional( stream << L"position = \"" << state.position.x << L" " << state.position.y << L" " << state.position.z << L"\"", state.position.sqrlen() > 0.0f );
					WriteCustomConditional( stream << L"direction = \"" << state.direction.x << L" " << state.direction.y << L" " << state.direction.z << L"\"", state.direction.sqrlen() > 0.0f );
					WriteValueNamed(L"radius", state.radius);
					WriteValueNamed(L"penumbra_dist", state.penumbra_dist);
					WriteValueNamed(L"penumbra_radius", state.penumbra_radius);

					if( state.style > 0 && state.style < DynamicLight::NumStyles )
						WriteValueNamed( L"style", state.style );
					else if( !state.custom_style.empty() && state.style == DynamicLight::NumStyles )
						WriteValueNamed( L"custom_style", state.custom_style );

					if( light.type == Renderer::Scene::Light::Type::Point )
					{
						WriteValueNamedConditional( L"distance_threshold", state.dist_threshold, state.dist_threshold > 0.0f );
					}
					if( light.type == Renderer::Scene::Light::Type::Spot )
					{
						WriteValueNamedConditional( L"penumbra", state.penumbra, state.penumbra != 0.0f );
						WriteValueNamedConditional( L"umbra", state.umbra, state.umbra != 0.0f );
					}
				}
			}
#endif
		}

		std::shared_ptr< Renderer::Scene::Light > LightsStatic::DynamicLight::CreateLight( const LightInstance& light, const simd::matrix& world ) const
		{
			const auto& state = light.current_state;
			const auto world_position = world.mulpos( state.position );
			if( type == Renderer::Scene::Light::Type::Point )
			{
				char channel_index = ( ( Renderer::Scene::curr_point_light_index++ ) % Renderer::Scene::point_light_cannels_count ) + 1; //channel 0 is reserved for player light, use 3 other channels in alternating fashion
				auto point_light = std::make_shared< Renderer::Scene::PointLight >( world_position, state.colour, state.radius, profile == Renderer::Scene::parabolic_light_profile, usage_frequency, channel_index );
				point_light->SetProfile( profile );
				point_light->SetDistThreshold( state.dist_threshold );
				point_light->SetPenumbra(state.penumbra_radius, state.penumbra_dist);
				return point_light;
			}
			else if( type == Renderer::Scene::Light::Type::Spot )
			{
				const auto world_direction = world.mulpos( state.direction );
				auto spotlight = std::make_shared< Renderer::Scene::SpotLight >( world_position, world_direction, state.colour, state.radius, state.penumbra, state.umbra, casts_shadow );
				return spotlight;
			}
			assert( false && "Invalid light type specified" );
			return nullptr;
		}

		void LightsStatic::DynamicLight::UpdateLight( const LightsStatic::DynamicLight::LightInstance& light, const simd::matrix& world, Renderer::Scene::Light& scene_light ) const
		{
			const auto world_position = world.mulpos( light.current_state.position );
			const auto world_direction = world.muldir( light.current_state.direction );

			if( type == Renderer::Scene::Light::Type::Point )
			{
				auto& point_light = static_cast< Renderer::Scene::PointLight& >( scene_light );
				point_light.SetPosition( world_position, world_direction, light.current_state.radius, light.current_state.colour * light.intensity );
				point_light.SetDistThreshold( light.current_state.dist_threshold );
				point_light.SetPenumbra(light.current_state.penumbra_radius, light.current_state.penumbra_dist);
			}
			else if( type == Renderer::Scene::Light::Type::Spot )
			{
				auto& spot_light = static_cast< Renderer::Scene::SpotLight& >( scene_light );
				spot_light.SetLightParams( world_position, world_direction, light.current_state.colour * light.intensity, light.current_state.radius, light.current_state.penumbra, light.current_state.umbra );
			}
		}

		void LightsStatic::DynamicLight::FrameMove( LightsStatic::DynamicLight::LightInstance& light, const float elapsed_time ) const
		{
			if( light.target_state >= states.size() )
				light.target_state = 0;

			static const std::string style_presets[] =
			{
				"z", // Normal
				"ssusvssussussvzvusvssussv", // Flicker
				"abcdefghijklmnopqrstuvwxyzyxwvutsrqponmlkjihgfedcba", // Pulse
				"AZ", // Strobe
			};
			static_assert( std::size( style_presets ) == NumStyles );

			light.elapsed_time += elapsed_time;

			const auto GetStateIntensity = [&]( const State& state )
			{
				const auto& style = state.custom_style.empty() ? style_presets[ state.style >= NumStyles ? Normal : state.style ] : state.custom_style;
				const unsigned idx = unsigned( light.elapsed_time * style_speed ) % style.size();
				if( isupper( style[ idx ] ) )
					return ( style[ idx ] - 'A' ) * 0.04f;
				else
				{
					const unsigned next_idx = ( idx + 1 ) % style.size();
					const float intensity = ( style[ idx ] - 'a' ) * 0.04f;
					const float next_intensity = ( isupper( style[ next_idx ] ) ? style[ next_idx ] - 'A' : style[ next_idx ] - 'a' ) * 0.04f;
					const float t = std::fmod( light.elapsed_time * style_speed, 1.0f );
					return Lerp( intensity, next_intensity, t );
				}
			};

			if( elapsed_time > light.time_to_transition || light.time_to_transition == 0.0f )
			{
				light.time_to_transition = 0.0f;
				light.current_state = states[ light.target_state ];
				light.intensity = GetStateIntensity( light.current_state );
				const float style_length = ( light.current_state.custom_style.empty() ? style_presets[ light.current_state.style ].size() : light.current_state.custom_style.size() ) / style_speed;
				light.elapsed_time = std::fmod( light.elapsed_time, style_length );

			}
			else
			{
				auto& current = light.current_state;
				const auto& target = states[ light.target_state ];
				const float t = elapsed_time / light.time_to_transition;

				current.colour = current.colour.lerp( target.colour, t );
				current.direction = current.direction.lerp( target.direction, t );
				current.position = current.position.lerp( target.position, t );
				current.dist_threshold = Lerp( current.dist_threshold, target.dist_threshold, t );
				current.radius = Lerp( current.radius, target.radius, t );
				current.penumbra = Lerp( current.penumbra, target.penumbra, t );
				current.umbra = Lerp( current.umbra, target.umbra, t );

				light.time_to_transition -= elapsed_time;
				const float t2 = light.time_to_transition / light.total_transition_time;
				const float current_intensity = GetStateIntensity( light.current_state );
				const float target_intensity = GetStateIntensity( target );
				light.intensity = Lerp( target_intensity, current_intensity, t2 );
			}
		}

		//----Lights-----

		Lights::Lights( const LightsStatic& static_ref, Objects::Object& object )
			: AnimationObjectComponent( object ), static_ref( static_ref )
		{
			for( const auto& light : static_ref.dynamic_lights )
			{
				dynamic_lights.emplace_back();
				dynamic_lights.back().target_state = light.default_state;
				dynamic_lights.back().current_state = light.states[ light.default_state ];
				if( light.randomise_start_time )
					dynamic_lights.back().elapsed_time = RandomFloat() * 1024.0f;
			}
		}

		Lights::~Lights()
		{
			DestroySceneObjects();
		}

		void Lights::RenderFrameMove( const float elapsed_time )
		{
			RenderFrameMoveInternal(elapsed_time);
		}

		void Lights::SetDynamicLightState( const std::string& state_name, const float time_to_transition )
		{
			for( unsigned idx = 0; idx < dynamic_lights.size(); ++idx )
				SetDynamicLightState( idx, state_name, time_to_transition );
		}

		void Lights::SetDynamicLightState( const unsigned dynamic_light_index, const std::string& state_name, const float time_to_transition )
		{
			if( dynamic_light_index >= dynamic_lights.size() )
				return;

			const auto& static_light = static_ref.dynamic_lights[ dynamic_light_index ];
			auto& light = dynamic_lights[ dynamic_light_index ];
			auto target_state = FindIf( static_light.states, [&]( const LightsStatic::DynamicLight::State& state ) { return state.name == state_name; } );
			if( target_state == static_light.states.end() )
				return;

			// If we haven't finished interpolating the previous state change, set the current style to whichever is closer
			if( light.time_to_transition > 0.0f && light.time_to_transition < light.total_transition_time * 0.5f && light.target_state < static_light.states.size() )
			{
				light.current_state.style = static_light.states[ light.target_state ].style;
				light.current_state.custom_style = static_light.states[ light.target_state ].custom_style;
			}

			light.target_state = (unsigned)std::distance( static_light.states.begin(), target_state );
			light.total_transition_time = light.time_to_transition = std::max( 0.0f, time_to_transition );
		}

		void Lights::RenderFrameMoveNoVirtual( AnimationObjectComponent* component, const float elapsed_time )
		{
			static_cast<Lights*>(component)->RenderFrameMoveInternal(elapsed_time);
		}

		void Lights::RenderFrameMoveInternal( const float elapsed_time )
		{
			if( dynamic_lights.empty() && lights.empty() )
				return;

#if defined(PROFILING)
			Stats::Tick( Type::Lights );
#endif
			assert( dynamic_lights.size() == static_ref.dynamic_lights.size() );
			for( unsigned i = 0; i < dynamic_lights.size(); ++i )
				static_ref.dynamic_lights[ i ].FrameMove( dynamic_lights[ i ], elapsed_time );

			if( lights.empty() )
				return;

			const auto* animated_render = GetObject().GetComponent< AnimatedRender >( static_ref.animated_render_index );
			const auto& world = animated_render->GetTransform();

			auto light_instance = lights.begin();
			if( !static_ref.dynamic_lights.empty() )
			{
				const auto& world = animated_render->GetTransform();
				for( unsigned i = 0; i < dynamic_lights.size(); ++i )
				{
					static_ref.dynamic_lights[ i ].UpdateLight( dynamic_lights[ i ], world, **light_instance++ );
				}
			}

			if( static_ref.disable_mesh_lights )
				return;

			if( static_ref.fixed_mesh_index != -1 )
			{
				const auto* fixed_mesh = GetObject().GetComponent< FixedMesh >( static_ref.fixed_mesh_index );

				const auto& fixed_meshes = fixed_mesh->GetFixedMeshes();
				for( auto mesh = fixed_meshes.begin(); mesh != fixed_meshes.end(); ++mesh )
				{
					const auto& mesh_lights = mesh->mesh->GetLights();
					for( auto light = mesh_lights.begin(); light != mesh_lights.end(); ++light, ++light_instance )
					{
						light->UpdateLight( world, **light_instance );
					}
				}
			}
			else
			{
				const auto* client_animation_controller = GetObject().GetComponent< ClientAnimationController >( static_ref.client_animation_controller_index );
				const auto& light_states = client_animation_controller->GetCurrentLightState();
				for( auto iter = light_states.begin(); iter != light_states.end(); ++iter, ++light_instance )
				{
					iter->UpdateLight( world, **light_instance );
				}
			}

			if( force_black )
			{
				const simd::vector3 black( 0, 0, 0 );
				for( auto& light : lights )
					light->SetColour( black );
			}
		}

		void Lights::SetupSceneObjects()
		{
			const auto* animated_render = GetObject().GetComponent< AnimatedRender >( static_ref.animated_render_index );

#if defined( DEVELOPMENT_CONFIGURATION ) || defined( TESTING_CONFIGURATION )
			// Allow lights to be added/removed
			{
				if( dynamic_lights.size() > static_ref.dynamic_lights.size() )
					dynamic_lights.resize( static_ref.dynamic_lights.size() );
				else if( dynamic_lights.size() < static_ref.dynamic_lights.size() )
				{
					for( auto i = dynamic_lights.size(); i < static_ref.dynamic_lights.size(); ++i )
					{
						dynamic_lights.emplace_back();
						dynamic_lights.back().target_state = static_ref.dynamic_lights[ i ].default_state;
						dynamic_lights.back().current_state = static_ref.dynamic_lights[ i ].states[ static_ref.dynamic_lights[ i ].default_state ];
					}
				}
			}
#endif
			if( !static_ref.dynamic_lights.empty() )
			{
				const auto& world = animated_render->GetTransform();
				for( unsigned i = 0; i < static_ref.dynamic_lights.size(); ++i )
				{
					lights.emplace_back( static_ref.dynamic_lights[ i ].CreateLight( dynamic_lights[ i ], world ) );
					Renderer::Scene::System::Get().AddLight( lights.back() );
				}
			}

			if( static_ref.disable_mesh_lights )
				return;

			if( static_ref.fixed_mesh_index != -1 )
			{
				const auto& world = animated_render->GetTransform();
				const auto* fixed_mesh = GetObject().GetComponent< FixedMesh >( static_ref.fixed_mesh_index );

				const auto& fixed_meshes = fixed_mesh->GetFixedMeshes();
				for( auto mesh = fixed_meshes.begin(); mesh != fixed_meshes.end(); ++mesh )
				{
					const auto& mesh_lights = mesh->mesh->GetLights();
					for( auto light = mesh_lights.begin(); light != mesh_lights.end(); ++light )
					{
						auto light_instance = light->CreateLight( world );
						lights.push_back( std::move( light_instance ) );
						Renderer::Scene::System::Get().AddLight( lights.back() );
					}
				}
			}
			else if( static_ref.client_animation_controller_index != -1 )
			{
				const auto& world = animated_render->GetTransform();
				const auto* client_animation_controller = GetObject().GetComponent< ClientAnimationController >( static_ref.client_animation_controller_index );
				const auto& light_states = client_animation_controller->GetCurrentLightState();
				for( auto iter = light_states.begin(); iter != light_states.end(); ++iter )
				{
					auto light_instance = iter->CreateLight( world );
					lights.push_back( std::move( light_instance ) );
					Renderer::Scene::System::Get().AddLight( lights.back() );
				}
			}

			if( force_black )
			{
				const simd::vector3 black( 0, 0, 0 );
				for( auto& light : lights )
					light->SetColour( black );
			}
		}

		void Lights::DestroySceneObjects( )
		{
			if( !lights.empty() )
			{
				for( auto light = lights.begin(); light != lights.end(); ++light )
				{
					Renderer::Scene::System::Get().RemoveLight( *light );
				}
				lights.clear();
			}
		}
	}
}
