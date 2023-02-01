#include "ScreenShakeComponent.h"

#include "ClientInstanceCommon/Animation/Components/AnimationControllerComponent.h"
#include "ClientInstanceCommon/Animation/AnimatedObject.h"
#include "Visual/Renderer/Scene/SceneSystem.h"

#include "AnimatedRenderComponent.h"
#include "ClientAnimationControllerComponent.h"
#include "BoneGroupsComponent.h"

namespace Animation
{
	namespace Components
	{
		

		ScreenShakeStatic::ScreenShakeStatic( const Objects::Type& parent )
			: AnimationObjectStaticComponent( parent )
			, render_index( parent.GetStaticComponentIndex< AnimatedRenderStatic >() )
			, animation_controller_index( parent.GetStaticComponentIndex< AnimationControllerStatic >() )
			, client_animation_controller_index(parent.GetStaticComponentIndex< ClientAnimationControllerStatic >())
			, bone_groups_index( parent.GetStaticComponentIndex< BoneGroupsStatic >() )
			, current_animation( -1 )
		{

		}

		COMPONENT_POOL_IMPL(ScreenShakeStatic, ScreenShake, 16)

		bool ScreenShakeStatic::SetValue( const std::string& name, const std::wstring& value )
		{
			if( name == "animation" )
			{
				assert( animation_controller_index != -1 );
				auto* animation_controller = parent_type.GetStaticComponent< AnimationControllerStatic >( animation_controller_index );
				const unsigned animation_index = animation_controller->GetAnimationIndexByName( WstringToString( value ) );
				if( animation_index == -1 )
					throw Resource::Exception( ) << value << L" animation not found when setting screen shake.";

				current_animation = animation_index;
				return true;
			}
			else
			{
				std::unique_ptr< ScreenShakeEffect > effect_event( new ScreenShakeEffect );

				//Assume that the name is actually a float value indicating the time into an animation that the event is
				if( sscanf_s( name.c_str(), "%f", &effect_event->time ) == 0 )
					return false; //not a float value

				assert( animation_controller_index != -1 );

				if( current_animation == -1 )
					throw Resource::Exception( ) << L"No current animation set";

				effect_event->animation_index = current_animation;

				std::wstringstream stream( value );
				std::wstring damp_mode;
				stream >> effect_event->frequency >> effect_event->amplitude >> effect_event->duration >> effect_event->lateral >> damp_mode;
				if( stream.fail() )
					throw Resource::Exception( ) << "Unable to parse screen shake event";

				if( damp_mode == L"0" || damp_mode == L"none" )
					effect_event->damp = Renderer::Scene::Camera::ShakeDampMode::NoDampening;
				else if( damp_mode == L"1" || damp_mode == L"linear_down" )
					effect_event->damp = Renderer::Scene::Camera::ShakeDampMode::LinearDown;
				else if( damp_mode == L"linear_up" )
					effect_event->damp = Renderer::Scene::Camera::ShakeDampMode::LinearUp;
				else
					throw Resource::Exception() << "Unrecognised dampening mode: " << damp_mode;

				// optional flags
				effect_event->global = false;
				effect_event->always_on = false;
				if( !stream.eof() )
					stream >> effect_event->global;
				if( !stream.eof() )
					stream >> effect_event->always_on;

				//This feature has been disabled intentionally
				effect_event->always_on = false;

				effect_event->bone_group_index = -1;
				if( !stream.eof() )
				{
					std::wstring bone_group_name;
					stream >> bone_group_name;
					if( bone_group_name != L"-1" && bone_group_name.length() >= 1 )
					{
						if( bone_groups_index == -1 )
							throw Resource::Exception() << "Bone group name/index specified, but AO has no BoneGroups component";

						const auto IsDigit = []( const wchar_t c ) { return c >= L'0' && c <= L'9'; };
						if( IsDigit( bone_group_name[0] ) )
						{
							//probably a number
							if( std::all_of( bone_group_name.cbegin(), bone_group_name.cend(), IsDigit ) )
							{
								const auto* bone_groups = parent_type.GetStaticComponent< BoneGroupsStatic >( bone_groups_index );
								if( auto index = std::stoul( bone_group_name ); index < bone_groups->NumBoneGroups() )
									effect_event->bone_group_index = index;
							}
							else
							{
								const auto* bone_groups = parent_type.GetStaticComponent< BoneGroupsStatic >( bone_groups_index );
								effect_event->bone_group_index = bone_groups->BoneGroupIndexByName( WstringToString( bone_group_name ) );
								if( effect_event->bone_group_index == -1 )
									throw Resource::Exception() << L"Error: No such bone group named '" << bone_group_name << L"'";
							}
						}
						else
						{
							const auto* bone_groups = parent_type.GetStaticComponent< BoneGroupsStatic >( bone_groups_index );
							effect_event->bone_group_index = bone_groups->BoneGroupIndexByName( WstringToString( bone_group_name ) );
							if( effect_event->bone_group_index == -1 )
								throw Resource::Exception() << L"Error: No such bone group named '" << bone_group_name << L"'";
						}
					}
				}

#ifndef PRODUCTION_CONFIGURATION
				effect_event->origin = parent_type.current_origin;
#endif
				events.push_back( std::move( effect_event ) );
			
				//Add the event
				auto* animation_controller = parent_type.GetStaticComponent< AnimationControllerStatic >( animation_controller_index );
				animation_controller->AddEvent( current_animation, *events.back() );

				return true;
			}

			return false;
		}

		const char* ScreenShakeStatic::GetComponentName()
		{
			return "ScreenShake";
		}

		void ScreenShakeStatic::SaveComponent( std::wostream& stream ) const
		{
#ifndef PRODUCTION_CONFIGURATION
			const auto* parent = parent_type.parent_type->GetStaticComponent< ScreenShakeStatic >();
			assert( parent );

			const auto* animation_controller = parent_type.GetStaticComponent< AnimationControllerStatic >( animation_controller_index );
			const auto* client_animation_controller = parent_type.GetStaticComponent< ClientAnimationControllerStatic >( client_animation_controller_index );
			const auto* bone_groups = parent_type.GetStaticComponent< BoneGroupsStatic >( bone_groups_index );
			unsigned current_animation = -1;

			stream << std::noboolalpha;
			
			for( const auto& effect_event : events )
			{
				if( effect_event->copy_of )
					continue;
				if( effect_event->origin != parent_type.current_origin )
					continue;

				if( current_animation != effect_event->animation_index )
				{
					current_animation = effect_event->animation_index;
					WriteValueNamed( L"animation", animation_controller->GetMetadataByIndex( current_animation ).name.c_str() );
				}

				PushIndent();

				WriteCustom(
					{
					stream << effect_event->time << L" = \"" << effect_event->frequency << L' ' << effect_event->amplitude << L' ' << effect_event->duration << L' ' << effect_event->lateral << L' ';
					switch( effect_event->damp )
					{
					case Renderer::Scene::Camera::ShakeDampMode::LinearDown:
						stream << L"linear_down";
						break;
					case Renderer::Scene::Camera::ShakeDampMode::LinearUp:
						stream << L"linear_up";
						break;
					default:
					case Renderer::Scene::Camera::ShakeDampMode::NoDampening:
						stream << L"none";
						break;
					}
					stream << L' ' << effect_event->global << L' ' << effect_event->always_on << L' ';
					if( bone_groups && effect_event->bone_group_index < bone_groups->NumBoneGroups() )
						stream << StringToWstring( bone_groups->BoneGroupName( effect_event->bone_group_index ) );
					else
						stream << L"-1";
					stream << L"\"";
				} );
			}
#endif
		}

		void ScreenShakeStatic::CopyAnimationData( const int source_animation, const int destination_animation, std::function<bool( const Animation::Event& )> predicate, std::function<void( const Animation::Event&, Animation::Event& )> transform )
		{
			std::vector<ScreenShakeEffect> copied;
			for( auto& e : events )
			{
				if( e->animation_index != source_animation )
					continue;
				if( predicate( *e.get() ) )
				{
					copied.push_back( *e );
					transform( *e, copied.back() );
				}
			}

			auto* animation_controller = parent_type.GetStaticComponent< AnimationControllerStatic >( animation_controller_index );

			for( auto& e : copied )
			{
				events.emplace_back( std::make_unique<ScreenShakeEffect>( std::move( e ) ) );
				animation_controller->AddEvent( destination_animation, *events.back() );
			}
		}

		void ScreenShakeStatic::AddScreenShakeEvent( std::unique_ptr< ScreenShakeEffect > e )
		{
			const unsigned animation_index = e->animation_index;
			events.emplace_back( std::move( e ) );
			//Add the event
			auto* animation_controller = parent_type.GetStaticComponent< AnimationControllerStatic >( animation_controller_index );
			animation_controller->AddEvent( animation_index, *events.back() );
			//Needs sorting now
			animation_controller->SortEvents();
		}

		void ScreenShakeStatic::RemoveScreenStakeEvent( const ScreenShakeEffect& e )
		{
			auto* animation_controller = parent_type.GetStaticComponent< AnimationControllerStatic >();
			animation_controller->RemoveEvent( e );

			const auto found = std::find_if( events.begin(), events.end(), [&]( const std::unique_ptr< ScreenShakeEffect >& iter ) { return iter.get() == &e; } );
			events.erase( found );
		}

		ScreenShake::ScreenShake( const ScreenShakeStatic& static_ref, Objects::Object& object )
			: AnimationObjectComponent( object ), static_ref( static_ref )
		{
			if (static_ref.client_animation_controller_index != -1)
			{
				auto *client_animation_controller = GetObject().GetComponent< ClientAnimationController >(static_ref.client_animation_controller_index);
				client_animation_controller->AddListener(*this);
			}
		}

		void ScreenShake::OnAnimationEvent( const ClientAnimationController& controller, const Event& e, float time_until_trigger)
		{
			if( e.event_type != ScreenShakeEventType )
				return;

			const auto& effect_event = static_cast< const ScreenShakeStatic::ScreenShakeEffect& >( e );

			auto *animated_render = GetObject().GetComponent< AnimatedRender >( static_ref.render_index );
			const auto& transform(animated_render->GetTransform());

			simd::vector3 axis;
			simd::vector2 origin( transform[3][0], transform[3][1] );
			if( effect_event.bone_group_index != -1 )
			{
				assert( static_ref.bone_groups_index != -1 );
				const auto* bone_groups = GetObject().GetComponent< BoneGroups >( static_ref.bone_groups_index );
				const auto positions = bone_groups->GetBoneWorldPositions( effect_event.bone_group_index );
				assert( positions.size() > 1 && "Bone group used for ScreenShake event doesn't have at least 2 bones" );

				origin.x = positions[0].x;
				origin.y = positions[0].y;
				axis = ( positions[0] - positions[1] ).normalize();
			}
			else if( effect_event.lateral )
			{
				const simd::vector3 normal( 0.0f, -1.0f, 0.0f );
				axis = transform.muldir(normal);
			}
			else 
			{
				axis = simd::vector3(0.0f, 0.0f, 1.0f);
			}

            auto* camera = Renderer::Scene::System::Get().GetCamera();
            if( camera == nullptr )
                return;

            camera->DoScreenShake( effect_event.frequency, effect_event.amplitude, effect_event.duration, 
				axis, origin, effect_event.damp, effect_event.global, effect_event.always_on );
		}

	}
}
