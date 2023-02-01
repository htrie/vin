#include "SoundEventsComponent.h"

#include <random>
#include "Common/Loaders/Language.h"
#include "Common/Utility/BiasedSelector.h"
#include "Common/Utility/Converter.h"
#include "Common/Utility/BufferStream.h"
#include "Common/Utility/Logger/Logger.h"
#include "ClientInstanceCommon/Animation/AnimatedObject.h"
#include "ClientInstanceCommon/Animation/Components/AnimationControllerComponent.h"
#include "Visual/Animation/Components/ClientAnimationControllerComponent.h"

#include "Visual/Audio/SoundSystem.h"
#include "Visual/Audio/SoundEffects.h"
#include "Visual/Animation/Components/ComponentsStatistics.h"

#include "AnimatedRenderComponent.h"
#include "Common/Utility/ContainerOperations.h"

namespace Audio
{
	typedef signed char SoundPriority;
	enum
	{
		LowPriority = 10,
		NormalPriority = 15,
		HighPriority = 20
	};
}

namespace Animation
{
	namespace
	{
		float RandomFloat( const float _min, const float _max )
		{
			const unsigned gran = unsigned( ( _max - _min ) * 1024 + 0.5f );
			return ( gran == 0 ) ? _min : static_cast<float>( generate_random_number( gran ) ) * ( _max - _min ) / static_cast<float>( gran ) + _min;
		}

		namespace FMOD
		{
			const float NotMinion = 0;
			const float PlayerMinion = 1;
			const float RaisedSpectre = 2; //for now, both player spectres and enemy spectres use this flag		
			const auto is_minion_param_hash = Audio::CustomParameterHash("IsMinion");
			const auto empty_event = Components::SoundEvent(0.f, 0, std::wstring(), std::wstring(), 1.f, 0);
		};	
	}

	namespace Components
	{
		SoundEvent::SoundEvent( const float time, const unsigned animation_index, const std::wstring& _filename, const std::wstring& alternate_sound_file, const float volume, const signed char sound_priority, const float volume_variance /*= 0.0f*/, const float min_pitch /*= 0.0f*/, const float max_pitch /*= 0.0f*/, const float delay /*= 0.0f */, const int bone_index /*= -1 */, const bool stream /*= false */, const float rolloff /*= 1.0f */, const float ref_dist /*= 1.0f */, const bool dialogue_sfx /*= false*/)
			: Event( SoundEventType, animation_index ), filename( _filename ), volume( volume ), sound_priority( sound_priority ), rolloff( rolloff )
			, volume_variance( volume_variance ), min_pitch( min_pitch ), max_pitch( max_pitch ), delay( delay ), bone_index( bone_index ), ref_dist( ref_dist )
			, global( false ), loop( false ), play_once( false ), sound_type( Audio::SoundEffect ), dialogue_sfx(dialogue_sfx)
#ifndef PRODUCTION_CONFIGURATION
			, muted( false )
#endif
		{
			this->time = time;

			if( BeginsWith( filename, L"NPCTextAudio:" ) )
			{
				if( EndsWith( filename, L"SG" ) )
					filename = filename.substr( 0, filename.length() - 2 );

				const auto no_header = filename.substr( 13 );
				const auto colon = no_header.find( L':' );
				if( colon == no_header.npos )
					throw std::runtime_error("Bad formatting of NPCTextAudio animation event");

				const auto id = no_header.substr( colon + 1 );
				npc_talk = Loaders::NPCTextAudio::GetDataRowById( id );
				if( npc_talk.IsNull() )
					throw std::runtime_error("Unknown NPCTextAudio in animation event");

				const auto npc_id = no_header.substr( 0, colon );
				npc = Loaders::NPCs::GetDataRowByKey( npc_id );
				if( npc.IsNull() )
					throw std::runtime_error("Unknown NPC in NPCTextAudio animation event");

				sound_type = Audio::Dialogue;
			}
			else
			{
				auto dot_pos = filename.rfind( L'.' );
				if( dot_pos != filename.npos && dot_pos > 0 )
				{
					while( true ) 
					{
						const auto next_dot = filename.rfind( L'.', dot_pos - 1 );

						if( next_dot == filename.npos )
							break;

						if( next_dot + 1 == dot_pos )
							continue;

						const auto tag = filename.substr( next_dot + 1, dot_pos - next_dot - 1 );
							 if( tag == L"global" )	  global = true;
						else if( tag == L"loop" )	  loop = true;
						else if( tag == L"once" )	  play_once = true;
						else if( tag == L"dialogue" ) sound_type = Audio::Dialogue;
						else if( tag == L"ambient" )  sound_type = Audio::Ambient;
						else if( tag == L"music" )	  sound_type = Audio::Music;

						dot_pos = next_dot;
					}
				}

				SetupSound( stream, alternate_sound_file );
			}
		}

		void SoundEvent::SetupSound( const bool stream, const std::wstring& alternate_sound_file )
		{
			pseudovariable_names.clear();
			pseudovariable_streams.clear();

			if( sound_type == Audio::Dialogue )
			{
				auto index = filename.find( L"SG" );
				if ( index != std::wstring::npos )
					filename.replace( index, 2, L"" );

				index = filename.rfind( L'%' );
				if ( index != std::wstring::npos )
					filename = filename.substr( 0, index );

				if( filename.find( L"$(" ) != filename.npos )
				{
					pseudovariable_names = Audio::FindPseudoVariableFiles( filename, [&]( const std::wstring& file, const Audio::PseudoVariableSet& psv )
					{
						pseudovariable_streams[ psv ].push_back( file );
					} );
				}
			}
			else
			{
				sound_group = Audio::SoundHandle( filename );
				if (!alternate_sound_file.empty())
					alternate_sound = Audio::SoundHandle( alternate_sound_file );
			}
		}

		const std::wstring& SoundEvent::GetStreamFilename( const Audio::PseudoVariableSet& psv ) const
		{
			static std::wstring empty;

			if( pseudovariable_names.empty() )
				return filename;

			//Map the variables from the state we got to a relevant set for this group
			Audio::PseudoVariableSet relevant_variable_set;

			for( size_t i = 0; i < pseudovariable_names.size(); ++i )
			{
				//Skip the sequence number
				if( pseudovariable_names[ i ] == Audio::PseudoVariableTypes::SequenceNumber )
					continue;

				//Set the relevant variable type
				relevant_variable_set.variables[ pseudovariable_names[ i ] ] = psv.variables[ pseudovariable_names[ i ] ];
			}

			//Find the group
			const auto found_group = pseudovariable_streams.find( relevant_variable_set );
			if( found_group == pseudovariable_streams.end() )
				return empty;

			return RandomElement( found_group->second );
		}

		SoundEventsStatic::SoundEventsStatic( const Objects::Type& parent ) 
			: AnimationObjectStaticComponent( parent ), current_animation( -1 )
			, animation_controller(parent.GetStaticComponentIndex< AnimationControllerStatic >())
			, client_animation_controller(parent.GetStaticComponentIndex< ClientAnimationControllerStatic >())
			, animated_render( parent.GetStaticComponentIndex< AnimatedRenderStatic >( ) )
		{

		}


		COMPONENT_POOL_IMPL(SoundEventsStatic, SoundEvents, 64)

		ClientAnimationControllerStatic& SoundEventsStatic::GetController() const
		{
			auto animation_controller = parent_type.GetStaticComponent< ClientAnimationControllerStatic >();
			assert(animation_controller);
			return *animation_controller;
		}

		void SoundEventsStatic::CopyAnimationData( const int source_animation, const int destination_animation, std::function<bool( const Event& )> predicate, std::function<void( const Event&, Event& )> transform )
		{
			Memory::Vector<SoundEvent, Memory::Tag::Sound> copied;
			for( auto& e : sound_events )
			{
				if( e->animation_index != source_animation )
					continue;
				if( predicate( *e.get() ) )
				{
					copied.push_back( *e );
					transform( *e, copied.back() );
				}
			}

			auto* animation_controller = parent_type.GetStaticComponent< AnimationControllerStatic >();

			for( auto& e : copied )
			{
				sound_events.emplace_back( Memory::Pointer<SoundEvent, Memory::Tag::Sound>::make( std::move( e ) ) );
				animation_controller->AddEvent( destination_animation, *sound_events.back() );
			}
		}

		bool SoundEventsStatic::SetValue( const std::string& name, const std::wstring& value )
		{
			auto* animation_controller = parent_type.GetStaticComponent< AnimationControllerStatic >();
			
			if( name == "animation" )
			{
				if (!animation_controller)
					throw Resource::Exception() << L"Sound event has animation property but there is no animation controller.";

				const unsigned animation_index = animation_controller->GetAnimationIndexByName( WstringToString( value ) );
				if( animation_index == -1 )
					throw Resource::Exception( ) << value << L" animation not found when setting sound event.";
				
				current_animation = animation_index;
				return true;
			}
			else if( name == "soundbank" )
			{
				// deprecated
				return true;
			}
			else if (name == "sound")
			{
				attached_sounds.push_back( { value } );
#ifndef PRODUCTION_CONFIGURATION
				attached_sounds.back().origin = parent_type.current_origin;
#endif
				return true;
			}
			else
			{
				//Assume that the name is actually a float value indicating the time into an animation that the event is
				float time;
				if( sscanf_s( name.c_str(), "%f", &time ) == 0 )
					return false; //not a float value

				//Parse the value
				std::wstringstream stream( value );

				std::wstring sound_filename;
				std::getline( stream, sound_filename, L'@' );

				if( stream.fail() )
					throw Resource::Exception( ) << L" unable to read sound filename.";

				//Optionally read volume
				float volume;
				stream >> volume;
				if( stream.fail() )
					volume = 1.0f;

				//Optionally read priority
				int priority;
				stream >> priority;
				if( stream.fail() )
					priority = Audio::NormalPriority;
				else if( priority >= 0 && priority < 3 )
					priority = priority * 5 + Audio::LowPriority;
				else priority -= 100;

				//Optionally read volume variance
				float volume_var;
				stream >> volume_var;
				if( stream.fail() )
					volume_var = 0.0f;

				//Optionally read min and max pitch
				float min_pitch, max_pitch;
				stream >> min_pitch >> max_pitch;
				if( stream.fail() )
					min_pitch = max_pitch = 0.0f;

				//Optionally read delay
				float delay;
				stream >> delay;
				if( stream.fail() )
					delay = 0.0f;

				//Optionally read the bone name/index
				int bone_index = -1;
				std::wstring bone_name;
				stream >> bone_name;
				if( stream.fail() || bone_name == L"-1" || bone_name.length() < 1 )
				{
					bone_index = -1;
				}
				else
				{
					const auto IsDigit = []( const wchar_t c ) { return c >= L'0' && c <= L'9'; };
					if( IsDigit( bone_name[0] ) )
					{
						//probably a number
						if( std::all_of( bone_name.cbegin(), bone_name.cend(), IsDigit ) )
							bone_index = std::stol( bone_name );
						else
						{
							bone_index = GetController().GetBoneIndexByName( WstringToString( bone_name ) );
						}
					}
					else
					{
						bone_index = GetController().GetBoneIndexByName( WstringToString( bone_name ) );
						if( bone_index == Bones::Invalid )
						{
#ifdef DEVELOPMENT_CONFIGURATION
							assert( "Invalid bone specified" );
							return true; //continue
#else
							throw Resource::Exception() << L"Error: No such bone named '" << bone_name << L"'";
#endif
						}
					}
				}

				//Optionally read the stream flag
				int stream_flag;
				stream >> stream_flag;
				if( stream.fail() )
					stream_flag = 0;

				//Optionally read rolloff factor
				float rolloff;
				stream >> rolloff;
				if( stream.fail() )
					rolloff = 1.0f;

				//Optionally read ref dist
				float ref_dist;
				stream >> ref_dist;
				if( stream.fail() )
					ref_dist = 1.0f;

				//All sound events should just have "SG" ( Removed the "stream" flag dependency since it is not needed anymore  )
				const std::wstring filename = sound_filename + L"SG";

				sound_events.emplace_back( Memory::Pointer< SoundEvent, Memory::Tag::Sound >::make( time, current_animation, filename, L"", volume, priority, volume_var, min_pitch, max_pitch, delay, bone_index, !!stream_flag, rolloff, ref_dist ) );

				//Read the custom sound parameters
				int params_num;
				stream >> params_num;
				if ( stream.fail() )
					params_num = 0;
				if ( params_num )
				{
					SoundEvent& sound_event = *sound_events.back();
					sound_event.sound_params.resize( params_num );
					for ( int i = 0; i < params_num; ++i )
						stream >> sound_event.sound_params[ i ].first >> sound_event.sound_params[ i ].second;
				}

				SoundEvent& sound_event = *sound_events.back();

				//Read chance
				float chance;
				stream >> chance;
				if (!stream.fail())
					sound_event.chance = chance;

				//Read fmod event
				std::wstring fmod_event;
				if (ExtractString(stream, fmod_event))
					sound_event.fmod_event = fmod_event;

				//Read alternate sound
				std::wstring alternate_sound;
				if (ExtractString(stream, alternate_sound))
					sound_event.alternate_sound = Audio::SoundHandle(alternate_sound);

				//Read dialogue_sfx
				bool dialogue_sfx;
				stream >> dialogue_sfx;
				if (!stream.fail())
					sound_event.dialogue_sfx = dialogue_sfx;

#ifndef PRODUCTION_CONFIGURATION
				sound_event.origin = parent_type.current_origin;
#endif
				animation_controller->AddEvent( current_animation, *sound_events.back() );

				return true;
			}

			return false;
		}

		void SoundEventsStatic::SaveComponent( std::wostream& stream ) const
		{
#ifndef PRODUCTION_CONFIGURATION
			const auto* parent = parent_type.parent_type->GetStaticComponent< SoundEventsStatic >();
			assert( parent );

			for( const auto& attached_sound : attached_sounds )
			{
				if( attached_sound.origin != parent_type.current_origin )
					continue;

				WriteValueNamed( L"sound", attached_sound.sound );
			}

			const auto* animation_controller = parent_type.GetStaticComponent< AnimationControllerStatic >( );
			const auto* client_animation_controller = parent_type.GetStaticComponent< ClientAnimationControllerStatic >( );

			Memory::Map< std::string, Memory::Vector< unsigned, Memory::Tag::Sound >, Memory::Tag::Sound > sound_event_map;
			for( unsigned i = 0; i < sound_events.size(); ++i )
			{
				const unsigned index = sound_events[ i ]->animation_index;
				const auto& metadata = animation_controller->GetMetadataByIndex( index );
				sound_event_map[ metadata.name ].push_back( i );
			}

			for( auto iter2 = sound_event_map.begin(); iter2 != sound_event_map.end(); ++iter2 )
			{
				if( AllOfIf( iter2->second, [&]( const unsigned idx ) { return sound_events[ idx ]->copy_of || sound_events[ idx ]->origin != parent_type.current_origin; } ) )
					continue;

				WriteValueNamed( L"animation", iter2->first.c_str() );

				std::sort( iter2->second.begin(), iter2->second.end(), [&]( const unsigned a, const unsigned b )
				{
					return sound_events[ a ]->time < sound_events[ b ]->time;
				} );

				PushIndent();

				for( auto iter = iter2->second.begin(); iter != iter2->second.end(); ++iter )
				{
					const auto& sound_event = sound_events[ *iter ];
					if( sound_event->copy_of )
						continue;
					if( sound_event->origin != parent_type.current_origin )
						continue;

					const auto& filename = sound_event->filename;
					assert( filename.size() > 2 );

					const auto& path = sound_event->sound_group.empty() ? filename : filename.substr(0, filename.size() - 2);
					const auto sound_group_empty = (int)sound_event->sound_group.empty();
					const auto alternate_empty = sound_event->alternate_sound.empty();
					const auto& alternate_filename = sound_event->alternate_sound;
					const auto dialogue_sfx = sound_event->dialogue_sfx;

					WriteCustom(
					{
						stream << sound_event->time << L" = \"" << path;
						stream << L"@" << sound_event->volume << L' ' << ( int )( sound_event->sound_priority + 100 ) << L' ' << sound_event->volume_variance << L' ' << sound_event->min_pitch << L' ' << sound_event->max_pitch << L' ' << sound_event->delay << L' ';
						if( client_animation_controller && !client_animation_controller->skeleton.IsNull() && sound_event->bone_index >= 0 && ( unsigned )sound_event->bone_index < client_animation_controller->GetNumBonesAndSockets() )
							stream << StringToWstring( client_animation_controller->GetBoneNameByIndex( sound_event->bone_index ) );
						else
							stream << sound_event->bone_index;
						stream << L' ' << sound_group_empty << L' ' << sound_event->rolloff << L' ' << sound_event->ref_dist;

						stream << L' ' << ( int )sound_event->sound_params.size();
						for( auto param = sound_event->sound_params.begin(); param != sound_event->sound_params.end(); ++param )
							stream << L' ' << param->first << L' ' << param->second;

						if (sound_event->chance != 1.f || !sound_event->fmod_event.empty() || !alternate_empty || dialogue_sfx)
							stream << L' ' << sound_event->chance;

						if (!sound_event->fmod_event.empty() || !alternate_empty || dialogue_sfx)
							stream << L" \"" << sound_event->fmod_event << L"\"";

						if (!alternate_empty || dialogue_sfx)
							stream << L" \"" << alternate_filename << L"\"";

						if (dialogue_sfx)
							stream << L' ' << dialogue_sfx;

						stream << L"\"";
					} );
				}
			}
#endif
		}

		SoundEvents::SoundEvents( const SoundEventsStatic& static_ref, Objects::Object& object ) 
			: AnimationObjectComponent( object ), static_ref( static_ref ), 
			sound_source(std::make_shared< Audio::SoundSource >()),
			sound_enabled( true )
		{ 
			if (static_ref.client_animation_controller != -1)
			{
				auto *client_animation_controller = GetObject().GetComponent< ClientAnimationController >(static_ref.client_animation_controller);
				client_animation_controller->AddListener(*this);
			}

			if(!static_ref.attached_sounds.empty())
			{
				for (const auto& attached_sound : static_ref.attached_sounds)
				{
					PlaySound(Audio::Desc(attached_sound.sound), FMOD::empty_event, sound_source);
				}
			}
		}

		SoundEvents::~SoundEvents()
		{
			StopAllSounds();

			if (static_ref.client_animation_controller != -1)
			{
				auto *client_animation_controller = GetObject().GetComponent< ClientAnimationController >(static_ref.client_animation_controller);
				client_animation_controller->RemoveListener(*this);
			}
		}

		void SoundEvents::OnAnimationEvent(const ClientAnimationController& controller, const Event& e, float time_until_trigger)
		{
			PROFILE;

			if( e.event_type != SoundEventType || !sound_enabled )
				return;
			
			const auto& sound_event = static_cast< const SoundEvent& >( e );
#ifndef PRODUCTION_CONFIGURATION
			if( sound_event.muted )
				return;
#endif

			if (sound_event.dialogue_sfx && !dialogue_sfx_enabled)
				return;

			if( sound_event.play_once )
			{
				if( std::find( play_once_sounds.cbegin(), play_once_sounds.cend(), &sound_event ) != play_once_sounds.cend() )
					return;

				play_once_sounds.push_back( &sound_event );
			}

			PlayAnimationEventSound(sound_event);
				}

		void SoundEvents::PlaySound( const Audio::SoundHandle& sound, const Audio::CustomParameters& instance_parameters /*= Audio::CustomParameters()*/ )
		{
			PlaySound(Audio::Desc(sound).SetPseudoVariables(pseudovariables).SetAllowStop(false), FMOD::empty_event, sound_source, instance_parameters);
		}

		void SoundEvents::OnAnimationStart(const CurrentAnimation& animation, const bool blend)
		{
			StopAllLoopingSounds();
			play_once_sounds.clear();
		}

		void SoundEvents::OnAnimationSpeedChange(const CurrentAnimation& animation)
		{
			const float global_speed_multiplier = GetObject().GetComponent< AnimationController >( static_ref.animation_controller )->GetGlobalSpeedMultiplier();
			const float anim_speed = animation.speed_multiplier * global_speed_multiplier;
			
			// update the animation speed of the object's sound source
			sound_source->anim_speed = anim_speed;

			// update the animation speed of the attached sound sources
			for ( auto iter = attached_sources.begin(); iter != attached_sources.end(); ++iter )
			{
				iter->second->anim_speed = anim_speed;
			}
		}

		int SoundEvents::GetSoundPseudovariable( const Audio::PseudoVariableTypes::Name type ) const
		{
			return pseudovariables.variables[ type ];
		}

		void SoundEvents::SetSoundPseudovariable( const Audio::PseudoVariableTypes::Name type, const int value )
		{
			pseudovariables.variables[ type ] = value;
		}

		void SoundEventsStatic::RemoveSoundEvent( const SoundEvent& sound_event )
		{
			auto found = std::find_if( sound_events.begin(), sound_events.end(), [&]( const auto& val ) { return val.get() == &sound_event; } );
			parent_type.GetStaticComponent< AnimationControllerStatic >()->RemoveEvent( **found );
			sound_events.erase( found );
		}

		void SoundEventsStatic::DuplicateSoundEvent( const SoundEvent& sound_event )
		{
			sound_events.emplace_back( Memory::Pointer< SoundEvent, Memory::Tag::Sound >::make( sound_event ) );

			auto* animation_controller = parent_type.GetStaticComponent< AnimationControllerStatic >();
			animation_controller->AddEvent( sound_event.animation_index, *sound_events.back() );
			animation_controller->SortEvents();
		}

		void SoundEventsStatic::AddSoundEvent( const std::wstring& sound_group, const std::wstring& alternate_sound_file, const float time, const unsigned animation_index )
		{
			sound_events.emplace_back( Memory::Pointer< SoundEvent, Memory::Tag::Sound >::make( time, animation_index, sound_group + L"SG", alternate_sound_file, 1.0f, Audio::NormalPriority ) );

			auto* animation_controller = parent_type.GetStaticComponent< AnimationControllerStatic >();
			animation_controller->AddEvent( animation_index, *sound_events.back() );
			animation_controller->SortEvents();
		}

		const char* SoundEventsStatic::GetComponentName()
		{
			return "SoundEvents";
		}

		void UpdatePosVel(simd::vector3 new_world_pos, const Audio::SoundSourcePtr& source, const float elapsed_time )
		{
			//convert to audio space (in metres, z-axis flipped)
			new_world_pos /= 100.0f;
			new_world_pos.z = -new_world_pos.z;

			simd::vector3& sound_source_pos = *reinterpret_cast< simd::vector3* >( source->position );

			//if time has passed, update velocity
			if( elapsed_time > 0 )
			{
				simd::vector3& sound_source_vel = *reinterpret_cast< simd::vector3* >( source->velocity );
				sound_source_vel = ( new_world_pos - sound_source_pos ) / elapsed_time;
			}

			sound_source_pos = new_world_pos;
		}

		void SoundEvents::RenderFrameMove( const float elapsed_time )
		{
			RenderFrameMoveInternal(elapsed_time);
		}

		void SoundEvents::RenderFrameMoveNoVirtual( AnimationObjectComponent* component, const float elapsed_time )
		{
			static_cast<SoundEvents*>(component)->RenderFrameMoveInternal(elapsed_time);
		}

		void SoundEvents::RenderFrameMoveInternal(const float elapsed_time)
		{
		#if defined(PROFILING)
			Stats::Tick(Type::SoundEvents);
		#endif

			//Update the position and velocity of the sound source.
			const auto* animated_render = GetObject().GetComponent< AnimatedRender >(static_ref.animated_render);
			UpdatePosVel(animated_render->GetWorldPosition(), sound_source, elapsed_time);

			//LOG_DEBUG( L"Root: (" << sound_source->position[0] << L", " << sound_source->position[1] << L", " << sound_source->position[2] << L")" );

			const auto* animation_controller = GetObject().GetComponent< ClientAnimationController >();

			for (auto iter = attached_sources.begin(); iter != attached_sources.end(); ++iter)
			{
				UpdatePosVel(animated_render->TransformModelSpaceToWorldSpace(animation_controller->GetBonePosition(iter->first)), iter->second, elapsed_time);

				//LOG_DEBUG( L"Bone: (" << iter->second->position[0] << L", " << iter->second->position[1] << L", " << iter->second->position[2] << L")" );
			}

			auto update_instance = [&](const SoundEvent& sound_event, const uint64_t id)
			{
				if (!sound_event.global)
				{
					const auto& source = sound_event.bone_index < 0 ? sound_source : attached_sources[sound_event.bone_index];
					assert(source);
					const simd::vector3 world_pos(source->position[0], source->position[1], source->position[2]);
					const simd::vector3 world_vel(source->velocity[0], source->velocity[1], source->velocity[2]);
					Audio::System::Get().Move(id, world_pos, world_vel, source->anim_speed);
				}
			};

			for (auto iter = sounds.begin(); iter != sounds.end();)
			{
				if (iter->second)
				{
					const auto& info = Audio::System::Get().GetInfo(iter->second);
					if (info.IsFinished())
					{
						pending_looping_sounds.erase(iter->first);
						iter = sounds.erase(iter);			
					}
					else
					{
						if (!info.IsPending() && !info.IsLooping())
							pending_looping_sounds.erase(iter->first);
						update_instance(*iter->first, iter->second);
						++iter;
					}
				}
				else
					++iter;
			}

			if (is_player_minion || is_raised_spectre)
				SetSoundParameter(FMOD::is_minion_param_hash, is_raised_spectre ? FMOD::RaisedSpectre : FMOD::PlayerMinion);
		}

		void SoundEvents::SetIsPlayerMinion( const bool value )
		{
			is_player_minion = value;

			if( !is_raised_spectre )
				SetSoundParameter(FMOD::is_minion_param_hash, is_player_minion ? FMOD::PlayerMinion : FMOD::NotMinion);
		}

		void SoundEvents::SetIsRaisedSpectre( const bool value )
		{
			is_raised_spectre = value;
			SetSoundParameter(FMOD::is_minion_param_hash, is_raised_spectre ? FMOD::RaisedSpectre : is_player_minion ? FMOD::PlayerMinion : FMOD::NotMinion);
		}

		void SoundEvents::PlaySound(const Audio::Desc& desc, const SoundEvent& sound_event, const Audio::SoundSourcePtr source, const Audio::CustomParameters& instance_parameters /*= Audio::CustomParameters()*/)
		{
			Audio::Desc final_desc = desc;

			Audio::CustomParameters parameters;
			for (const auto& param : sound_event.sound_params)
			{
				const auto param_hash = Audio::CustomParameterHash(WstringToString(param.first));  // TODO: store hashes instead
				parameters.emplace_back(param_hash, param.second);
			}
			if (!this->parameters.empty())
				Append(parameters, this->parameters);
			if (!instance_parameters.empty())
				Append(parameters, instance_parameters);
			final_desc.SetParameters(parameters);

			if (source)
				final_desc = final_desc.SetPosition(simd::vector3(source->position)).SetVelocity(simd::vector3(source->velocity)).SetAnimSpeed(source->anim_speed);

			const auto id = Audio::System::Get().Play(final_desc);

			pending_looping_sounds.insert(&sound_event);
			sounds.push_back(std::make_pair(&sound_event, id));
		}

		void SoundEvents::PlayAnimationEventStream( const SoundEvent& sound_event, const Audio::SoundSourcePtr source, const float volume, const float pitch )
		{
			for( auto iter = sounds.cbegin(); iter != sounds.cend(); ++iter )
				if( iter->first == &sound_event )
					return;

			if( !sound_event.npc_talk.IsNull() )
			{
				Audio::ListenerManager::Get().OnNPCTextAudioEvent( *this, sound_event, source, volume, pitch );
			}
			else
			{
				const float delay = RandomFloat( 0, sound_event.delay );
				auto desc = Audio::Desc(sound_event.GetStreamFilename(pseudovariables)).SetType(sound_event.sound_type).SetAlternatePath(sound_event.fmod_event).SetDelay(delay);
				PlaySound(desc, sound_event, source);
			}
		}

		void SoundEvents::PlayAnimationEventSound(const SoundEvent& sound_event)
		{
			if (sound_event.sound_group.empty() && sound_event.filename.empty())
				return;

			// If the same sound event is being played and is a pending/looping sound, then don't play it again
			if (pending_looping_sounds.find(&sound_event) != pending_looping_sounds.end())
				return;

			auto sound_type = sound_event.sound_type;
			const auto chance = RandomFloat(0.f, 1.f);

			const auto get_source = [&]()
			{
				if (sound_event.global)
					return Audio::SoundSourcePtr();

				if (sound_event.bone_index >= 0)
					return CreateBoneSoundSource(sound_event.bone_index);

				const auto* animated_render = GetObject().GetComponent< AnimatedRender >(static_ref.animated_render);
				UpdatePosVel(animated_render->GetWorldPosition(), sound_source, 0);
				return sound_source;
			};

			const auto get_sound = [&]()
			{
				if (chance > sound_event.chance)
				{
					if (!sound_event.alternate_sound.empty())
					{
						sound_type = Audio::SoundEffect; // Maintaining old behaviour
						return sound_event.alternate_sound;
					}
				}
				return (!weapon_swing_override.empty() && StringContains(sound_event.filename, L"WeaponSwinging")) ? weapon_swing_override : sound_event.sound_group;
			};

			auto sound = get_sound();
			auto source = get_source();

			if (sound.empty())
				PlayAnimationEventStream(sound_event, nullptr, 1.f, 1.f);
			else
			{
				auto desc = Audio::Desc(sound).SetType(sound_type).SetPseudoVariables(pseudovariables);
				PlaySound(desc, sound_event, source);
			}
		}

		void SoundEvents::StopAllLoopingSounds()
		{
			for (auto sound = sounds.begin(); sound != sounds.end(); )
			{
				if (pending_looping_sounds.find(sound->first) != pending_looping_sounds.end())
				{
					Audio::System::Get().Stop(sound->second);
					sound = sounds.erase(sound);
				}
				else
					++sound;
			}

			pending_looping_sounds.clear();
		}

		void SoundEvents::StopAllStreamingSounds()
		{
			for (auto sound = sounds.begin(); sound != sounds.end(); )
			{
				const auto sound_event = sound->first;
				if (sound_event && sound_event->sound_group.empty())
				{
					Audio::System::Get().Stop(sound->second);
					sound = sounds.erase(sound);
				}
				else
					++sound;
			}
		}

		void SoundEvents::StopAllDialogueSfx()
		{
			for (auto sound = sounds.begin(); sound != sounds.end(); )
			{
				const auto sound_event = sound->first;
				if (sound_event && sound_event->dialogue_sfx)
				{
					Audio::System::Get().Stop(sound->second);
					sound = sounds.erase(sound);
				}
				else
					++sound;
			}
		}

		void SoundEvents::StopAllSounds()
		{
			StopAllLoopingSounds();
			StopAllStreamingSounds();

			sounds.clear();
		}

		Audio::SoundSourcePtr SoundEvents::CreateBoneSoundSource( const int bone_index )
		{
			auto iter = attached_sources.find( bone_index );
			if( iter == attached_sources.end() )
			{
				iter = attached_sources.insert(std::make_pair(bone_index, std::make_shared< Audio::SoundSource >())).first;
			}
			const auto* animated_render = GetObject().GetComponent< AnimatedRender >( static_ref.animated_render );
			const auto* animation_controller = GetObject().GetComponent< ClientAnimationController >();
			const auto bone_position = animation_controller->GetBonePosition( bone_index );

			UpdatePosVel( animated_render->TransformModelSpaceToWorldSpace( bone_position ), iter->second, 0 );
			
			return iter->second;
		}

		void SoundEvents::SetEnabled( const bool enabled )
		{
			if( enabled == sound_enabled )
				return;

			sound_enabled = enabled;

			if( !sound_enabled )
				StopAllSounds();
		}

		bool SoundEvents::IsEnabled() const
		{
			return sound_enabled;
		}

		void SoundEvents::SetEnableDialogueSfx(const bool enabled)
		{
			if (enabled == dialogue_sfx_enabled)
				return;

			dialogue_sfx_enabled = enabled;

			if (!dialogue_sfx_enabled)
				StopAllDialogueSfx();
		}

		size_t SoundEvents::GetUniqueID() const
		{
			// just return the address as the unique id
			return (size_t)this;
		}

		void SoundEvents::SetSoundParameter(const uint64_t param_hash, const float param_value)
		{
			for (const auto& sound : sounds)
				Audio::System::Get().SetParameter(sound.second, param_hash, param_value);

			// We store parameters to be applied for future sounds to be played in this sound event component
			parameters.push_back(std::make_pair(param_hash, param_value));
		}

		void SoundEvents::TriggerSoundCue()
		{
			for (const auto& sound : sounds)
				Audio::System::Get().SetTriggerCue(sound.second);
		}
	} //namespace Components
} //namespace Animation
