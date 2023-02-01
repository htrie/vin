#include "ClientInstanceCommon/Game/GameObject.h"
#include "ClientInstanceCommon/Animation/AnimatedObject.h"
#include "ClientInstanceCommon/Animation/Components/AnimationControllerComponent.h"
#include "ClientInstanceCommon/Game/Components/Player.h"
#include "ClientInstanceCommon/Game/Components/PlayerClass.h"
#include "ClientCommon/Game/ClientGameWorld.h"
#include "Visual/Audio/SoundSystem.h"
#include "SoundEventsComponent.h"
#include "SoundParamComponent.h"

namespace Animation
{
	namespace Components
	{
		namespace
		{
			const auto hash_cue = Audio::CustomParameterHash("cue");
			const auto hash_instance_count = Audio::CustomParameterHash("instance_count");
			const auto hash_isplayer = Audio::CustomParameterHash("is_player");
			const auto hash_gender = Audio::CustomParameterHash("Gender");
			const auto hash_gender2 = Audio::CustomParameterHash("gender");
		}

		SoundParamsStatic::SoundParamsStatic( const Objects::Type& parent )
			: AnimationObjectStaticComponent( parent )
			, animation_controller_index( parent.GetStaticComponentIndex< AnimationControllerStatic >() )
			, sound_events_index( parent.GetStaticComponentIndex< SoundEventsStatic >() )
			, current_animation( -1 )
		{
		}

		void SoundParamsStatic::CopyAnimationData( const int source_animation, const int destination_animation, std::function<bool( const Animation::Event& )> predicate, std::function<void( const Animation::Event&, Animation::Event& )> transform )
		{
			std::vector<SoundParamEvent> copied;
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
				events.emplace_back( std::make_unique<SoundParamEvent>( std::move( e ) ) );
				animation_controller->AddEvent( destination_animation, *events.back() );
			}
		}

		COMPONENT_POOL_IMPL(SoundParamsStatic, SoundParams, 64)

		bool SoundParamsStatic::SetValue( const std::string& name, const std::wstring& value )
		{
			if ( name == "animation" )
			{
				assert( animation_controller_index != -1 );
				auto* animation_controller = parent_type.GetStaticComponent< AnimationControllerStatic >( animation_controller_index );
				const unsigned animation_index = animation_controller->GetAnimationIndexByName( WstringToString( value ) );
				if ( animation_index == -1 )
					throw Resource::Exception() << value << L" animation not found when setting sound param event.";

				current_animation = animation_index;
				return true;
			}
			else
			{
				std::unique_ptr< SoundParamEvent > event( new SoundParamEvent );

				//Get the time into an animation that the event is
				if ( sscanf_s( name.c_str(), "%f", &event->time ) == 0 )
					return false;

				assert( animation_controller_index != -1 );
				if ( current_animation == -1 )
					throw Resource::Exception() << L"No current animation set";

				event->animation_index = current_animation;

				std::wstringstream stream( value );
				stream >> event->name >> event->value;
				if ( stream.fail() )
					throw Resource::Exception() << "Unable to parse sound param event";

				event->hash = Audio::CustomParameterHash(WstringToString(event->name));

#ifndef PRODUCTION_CONFIGURATION
				event->origin = parent_type.current_origin;
#endif
				events.push_back( std::move( event ) );

				auto* animation_controller = parent_type.GetStaticComponent< AnimationControllerStatic >( animation_controller_index );
				animation_controller->AddEvent( current_animation, *events.back() );

				return true;
			}

			return false;
		}

		const char* SoundParamsStatic::GetComponentName()
		{
			return "SoundParams";
		}

		void SoundParamsStatic::SaveComponent( std::wostream& stream ) const
		{
#ifndef PRODUCTION_CONFIGURATION
			const auto* parent = parent_type.parent_type->GetStaticComponent< SoundParamsStatic >();
			assert( parent );

			const auto* animation_controller = parent_type.GetStaticComponent< AnimationControllerStatic >( animation_controller_index );
			unsigned current_animation = -1;

			for( const auto& sound_event : events )
			{
				if( sound_event->copy_of )
					continue;
				if( sound_event->origin != parent_type.current_origin )
					continue;

				if( current_animation != sound_event->animation_index )
				{
					current_animation = sound_event->animation_index;
					WriteValueNamed( L"animation", animation_controller->GetMetadataByIndex( current_animation ).name.c_str() );
				}

				PushIndent();
				WriteCustom( stream << sound_event->time << L" = \"" << sound_event->name << L' ' << sound_event->value << L"\"" );
			}
#endif
		}

		void SoundParamsStatic::AddSoundParamEvent( std::unique_ptr< SoundParamEvent > e )
		{
			const unsigned animation_index = e->animation_index;
			events.emplace_back( std::move( e ) );

			//Add the event
			auto* animation_controller = parent_type.GetStaticComponent< AnimationControllerStatic >( animation_controller_index );
			animation_controller->AddEvent( animation_index, *events.back() );

			//Needs sorting now
			animation_controller->SortEvents();
		}

		void SoundParamsStatic::RemoveSoundParamEvent( const SoundParamEvent& e )
		{
			auto* animation_controller = parent_type.GetStaticComponent< AnimationControllerStatic >();
			animation_controller->RemoveEvent( e );

			const auto found = std::find_if( events.begin(), events.end(), [ & ]( const std::unique_ptr< SoundParamEvent >& iter ) { return iter.get() == &e; } );
			events.erase( found );
		}

		SoundParams::SoundParams( const SoundParamsStatic& static_ref, Objects::Object& object )
			: AnimationObjectComponent( object ), static_ref( static_ref )
		{
			auto *animation_controller = GetObject().GetComponent< AnimationController >( static_ref.animation_controller_index );
			anim_cont_list_ptr = animation_controller->GetListener( *this );
			++static_ref.instance_count;
		}

		SoundParams::~SoundParams()
		{
			--static_ref.instance_count;
		}

		void SoundParams::OnAnimationEvent( const AnimationController& controller, const Event& e )
		{
			if ( e.event_type != SoundParamEventType )
				return;

			auto* sound_events = GetObject().GetComponent< SoundEvents >(static_ref.sound_events_index);
			if (!sound_events)
				return;

			const auto& event = static_cast< const SoundParamsStatic::SoundParamEvent& >( e );
			auto sound_id = sound_events->GetUniqueID();
			const auto param_hash = event.hash;
			
			if (param_hash == hash_cue)
				sound_events->TriggerSoundCue();
			else if (param_hash == hash_instance_count)
				sound_events->SetSoundParameter(param_hash, static_ref.instance_count);
			else if (param_hash == hash_isplayer)
			{
				sound_id = 0;				
				if (const auto* object = GetObject().GetVariable<Game::GameObject>(GEAL::GameObjectId))
				{
					const auto* player = static_cast<Game::ClientGameWorld&>(object->game_world).GetPlayer();
					auto is_player = (player == object);
					if (!is_player)
					{
						if (auto* parent = object->GetVariable<Game::GameObject>(GEAL::EffectOwnerId))
							is_player = (player == parent);
						else if (auto* parent = object->GetVariable<Game::GameObject>(GEAL::OwnerId))
							is_player = (player == parent);
					}
					sound_events->SetSoundParameter(param_hash, is_player ? 1.f : 0.f);
				}
			}
			else if( param_hash == hash_gender || param_hash == hash_gender2 )
			{
				sound_id = 0;
				auto* game_object = GetObject().GetVariable<Game::GameObject>( GEAL::GameObjectId );
				if( game_object )
				{
					if( const auto* player_class = game_object->GetComponent< Game::Components::PlayerClass >() )
					{
						sound_events->SetSoundParameter( param_hash, player_class->GetClassAscendancyInfo().IsMale() ? 2.f : 1.f );
					}
				}
			}
			else
				sound_events->SetSoundParameter(param_hash, event.value);
		}
	}
}
