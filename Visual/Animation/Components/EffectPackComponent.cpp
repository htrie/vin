#include "EffectPackComponent.h"

#include "Common/Utility/OsAbstraction.h"
#include "Common/Utility/BufferStream.h"
#include "Common/Utility/Logger/Logger.h"
#include "ClientInstanceCommon/Animation/AnimatedObject.h"
#include "AnimatedRenderComponent.h"
#include "ParticleEffectsComponent.h"
#include "BoneGroupsComponent.h"
#include "ClientAnimationControllerComponent.h"
#include "TrailsEffectsComponent.h"
#include "ClientInstanceCommon/Utility/EffectsUtility.h"
#include "Common/Game/IsClient.h"
#include "Common/File/Exceptions.h"

namespace Animation
{
	namespace Components
	{
		namespace {
			uint64_t GetSourceID(const void* source, unsigned pass_index)
			{
				if (pass_index == 0)
					return 0;

				uint64_t data[] = { reinterpret_cast<uintptr_t>(source), pass_index };
				return MurmurHash2_64(data, sizeof(data), 0xde59dbf86f8bd67c);
			}
		}

		COMPONENT_POOL_IMPL(EffectPackStatic, EffectPack, 256)

		const char* EffectPackStatic::GetComponentName()
		{
			return "EffectPack";
		}

		EffectPackStatic::EffectPackStatic( const Objects::Type& parent ) 
			: AnimationObjectStaticComponent( parent )
			, render_index( parent.GetStaticComponentIndex< AnimatedRenderStatic >() )
			, particle_effects_index( parent.GetStaticComponentIndex< ParticleEffectsStatic >() )
			, bone_groups_index( parent.GetStaticComponentIndex< BoneGroupsStatic >() )
			, trail_effects_index( parent.GetStaticComponentIndex< TrailsEffectsStatic >() )
			, animation_controller_index( parent.GetStaticComponentIndex< AnimationControllerStatic >() )
			, client_animation_controller_index( parent.GetStaticComponentIndex< ClientAnimationControllerStatic >() )
		{
			
		}

		void EffectPackStatic::CopyAnimationData( const int source_animation, const int destination_animation, std::function<bool( const Animation::Event& )> predicate, std::function<void( const Animation::Event&, Animation::Event& )> transform )
		{
			auto* animation_controller = parent_type.GetStaticComponent< AnimationControllerStatic >( animation_controller_index );

			std::vector< EffectPackEvent > copied;
			for( auto& e : effect_pack_events )
			{
				if( e->animation_index != source_animation )
					continue;
				if( predicate( *e.get() ) )
				{
					copied.push_back( *e );
					transform( *e, copied.back() );

					const auto& view = animation_controller->GetAnimationViewMetadata( destination_animation );
					const float end_time = static_cast< float >( view->begin + view->length ) / 1000.0f;
					copied.back().length = std::min( copied.back().length, end_time - e->time );
				}
			}

			for( auto& e : copied )
			{
				effect_pack_events.emplace_back( std::make_unique< EffectPackEvent >( std::move( e ) ) );
				animation_controller->AddEvent( destination_animation, *effect_pack_events.back() );
			}
		}

		void EffectPackStatic::SaveComponent( std::wostream& stream) const
		{
#ifndef PRODUCTION_CONFIGURATION
			const auto* parent = parent_type.parent_type->GetStaticComponent< EffectPackStatic >();
			assert( parent );

			for( const Visual::EffectPackHandle& epk : effect_packs )
			{
				if( Contains( parent->effect_packs, epk ) )
					continue;

				WriteValueNamed( L"effect_pack", epk.GetFilename() );
			}

			if( animation_controller_index != -1 )
			{
				const auto* animation_controller = parent_type.GetStaticComponent< AnimationControllerStatic >( animation_controller_index );

				unsigned current_animation = -1;
				for( const std::unique_ptr< EffectPackEvent >& effect_event : effect_pack_events )
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
					WriteCustom( stream << effect_event->time << L',' << effect_event->length << L" = \"" << effect_event->effect_pack.GetFilename() << L"\"" );
					
					{
						PushIndent();
						if( effect_event->length == 0.0f )
							WriteValueNamedConditionalV( L"event_is_permanent", !effect_event->remove_on_animation_end );
						else
							WriteValueNamedConditionalV( L"event_is_interruptible", effect_event->remove_on_animation_end );
						WriteValueNamedConditional( L"event_bone_name", effect_event->bone_name, !effect_event->bone_name.empty() );
					}
				}
			}

			WriteValueConditional( disable_effect_packs, disable_effect_packs != parent->disable_effect_packs );
#endif
		}

		void EffectPackStatic::RemoveEffectPackEvent( const EffectPackEvent& effect_pack_event )
		{
			auto* animation_controller = parent_type.GetStaticComponent< AnimationControllerStatic >();
			animation_controller->RemoveEvent( effect_pack_event );

			const auto found = std::find_if( effect_pack_events.begin(), effect_pack_events.end(), [&]( const std::unique_ptr< EffectPackEvent >& iter ) { return iter.get() == &effect_pack_event; } );
			effect_pack_events.erase( found );
		}

		void EffectPackStatic::AddEffectPackEvent( std::unique_ptr<EffectPackEvent> effect_pack_event )
		{
			assert( animation_controller_index != -1 );
			const unsigned animation_index = effect_pack_event->animation_index;
			effect_pack_events.emplace_back( std::move( effect_pack_event ) );
			//Add the event
			auto* animation_controller = parent_type.GetStaticComponent< AnimationControllerStatic >( animation_controller_index );
			animation_controller->AddEvent( animation_index, *effect_pack_events.back() );
			//Needs sorting now
			animation_controller->SortEvents();
		}

		bool EffectPackStatic::SetValue( const std::string& name, const std::wstring& value )
		{
			if( name == "effect_pack" )
			{
				effect_packs.emplace_back( Visual::EffectPackHandle( value ) );
				return true;
			}
			else if( name == "animation" )
			{
				assert( animation_controller_index != -1 );
				auto* animation_controller = parent_type.GetStaticComponent< AnimationControllerStatic >( animation_controller_index );
				const unsigned animation_index = animation_controller->GetAnimationIndexByName( WstringToString( value ) );
				if( animation_index == -1 )
					throw Resource::Exception() << value << L" animation not found when setting effect pack events.";

				current_animation = animation_index;
				return true;
			}
			else if( name == "event_bone_name" )
			{
				assert( !effect_pack_events.empty() );
				effect_pack_events.back()->bone_name = WstringToString( value );
#ifndef PRODUCTION_CONFIGURATION
				auto* animation_controller = parent_type.GetStaticComponent< ClientAnimationControllerStatic >( client_animation_controller_index );
				if( animation_controller->GetBoneIndexByName( effect_pack_events.back()->bone_name ) == Bones::Invalid )
					throw Resource::Exception() << L"No bone found with name " << value << L" when setting effect pack events";
#endif
				return true;
			}
			else
			{
				if( current_animation == -1 )
					throw Resource::Exception() << L"No current animation set";

				auto effect_event = std::make_unique< EffectPackEvent >();

				//Assume that the name is actually a float value indicating the time into an animation that the event is
				if( sscanf_s( name.c_str(), "%f,%f", &effect_event->time, &effect_event->length ) == 0 )
					return false; //not a float value

				effect_event->animation_index = current_animation;
				effect_event->effect_pack = value;
				effect_event->remove_on_animation_end = ( effect_event->length == 0.0f );

#ifndef PRODUCTION_CONFIGURATION
				effect_event->origin = parent_type.current_origin;
#endif
				effect_pack_events.emplace_back( std::move( effect_event ) );

				//Add the event
				assert( animation_controller_index != -1 );
				auto* animation_controller = parent_type.GetStaticComponent< AnimationControllerStatic >( animation_controller_index );
				animation_controller->AddEvent( current_animation, *effect_pack_events.back() );

				return true;
			}
			return false;
		}

		bool EffectPackStatic::SetValue( const std::string & name, const bool value )
		{
			if( name == "disable_effect_packs" )
			{
				disable_effect_packs = value;
				return true;
			}
			else if( name == "event_is_permanent" )
			{
				assert( !effect_pack_events.empty() );
				effect_pack_events.back()->remove_on_animation_end = !value;
				return true;
			}
			else if( name == "event_is_interruptible" )
			{
				assert( !effect_pack_events.empty() );
				effect_pack_events.back()->remove_on_animation_end = value;
				return true;
			}
			return false;
		}

		void EffectPack::AddEffectPack( const Visual::EffectPackHandle& effect_pack, const bool ignore_bad, const bool filter_submesh, const float remove_after_duration, const simd::matrix& attached_transform, const bool remove_on_animation_end )
		{
			PROFILE;

			if( static_ref.disable_effect_packs )
				return;

			const bool ignore_bad_final = ignore_bad || always_ignore_bad;

			const auto existing = std::find_if( effect_packs.begin(), effect_packs.end(), [&]( const RefCountedEffect& e ) { return e.effect_pack == effect_pack; } );
			if( existing != effect_packs.end() )
			{
				if( remove_after_duration > 0.0f )
				{
					auto& existing_remove_after_duration = remove_on_animation_end ? existing->remove_after_duration_or_animation_end : existing->remove_after_duration;
					if( existing_remove_after_duration == 0.0f )
						++existing->ref_count;
					existing_remove_after_duration = std::max( existing_remove_after_duration, remove_after_duration );
				}
				else if( remove_on_animation_end )
				{
					if( !existing->remove_on_animation_end )
					{
						++existing->ref_count;
						existing->remove_on_animation_end = true;
					}
				}
				else ++existing->ref_count;
			}
			else
			{
				RefCountedEffect ref_counted_effect;
				ref_counted_effect.effect_pack = effect_pack;
				ref_counted_effect.ref_count = 1;
				ref_counted_effect.play_start_epk_after_delay = effect_pack->GetPlayStartEpkAfterDelay();
				ref_counted_effect.attached_transform = attached_transform;
				ref_counted_effect.filter_submesh = filter_submesh;

				if( remove_after_duration > 0.0f && remove_on_animation_end )
					ref_counted_effect.remove_after_duration_or_animation_end = remove_after_duration;
				else if( remove_after_duration > 0.0f )
					ref_counted_effect.remove_after_duration = remove_after_duration;
				else
					ref_counted_effect.remove_on_animation_end = remove_on_animation_end;

				AddEffectPackVisuals( ref_counted_effect, ignore_bad_final );

				effect_packs.emplace_back(std::move(ref_counted_effect));

				if( effect_pack->GetPlayStartEpkAfterDelay() == 0.0f )
					EffectsUtility::AddEffectPack( GetObject(), effect_pack->GetBeginEffectPack() );
			}
		}

		void EffectPack::HandleError( const RefCountedEffect& effect, const std::string& error_msg ) const
		{
#if defined(DEVELOPMENT_CONFIGURATION) || defined(TESTING_CONFIGURATION) || defined(STAGING_CONFIGURATION)
			const auto message = fmt::format( L"{}. From AO: {}. From EPK: {}", StringToWstring( error_msg ), GetObject().GetType().GetFilename(), effect.effect_pack.GetFilename() );

			if( Game::IsTool() )
				throw File::EPKError( WstringToString( message ) );
			else
				LOG_CRIT( message );
#endif
		}

		bool EffectPack::TestBone( 
			const RefCountedEffect& effect
			, const bool ignore_bad
			, const BoneGroups* bone_groups_component
			, const ClientAnimationController* controller
			, const std::string_view bone_group_name
			, std::function< bool( const std::string& name, int, bool ) > add_entry
			, bool default_to_specific_bone /*= false*/
			, bool include_aux /*= true*/
			, bool include_sockets /*= false*/
		)
		{
			bool bone_found = false;
			Memory::SmallVector<std::string, 16, Memory::Tag::Effects> bone_names;
			SplitString( std::back_inserter( bone_names ), bone_group_name, ',' );

			for( const auto& bone_name : bone_names )
			{
				// Handle exclusions by splitting on the special stynax '&!' and then continuing to use the bone name as the first entry
				// Exclusions are actually checked inside try_add
				Memory::SmallVector<std::string, 4, Memory::Tag::Effects> exclusions;
				SplitString( std::back_inserter( exclusions ), bone_name, "&!" );
				const auto main_search = exclusions.front();

				const auto try_add = [&]( const std::string& name, std::optional< unsigned > known_index, bool is_bone_group ) -> bool
				{
					for( size_t i = 1; i < exclusions.size(); ++i )
						if( WildcardSearch( name, exclusions[i] ) )
							return false;

					if( !is_bone_group && !include_aux && BeginsWith( name, "aux_" ) )
						return false;

					if( !known_index )
					{
						known_index = is_bone_group
							? bone_groups_component->static_ref.BoneGroupIndexByName( name )
							: controller->GetBoneIndexByName( name );
					}

					if( ( is_bone_group && *known_index == -1 ) ||
						( !is_bone_group && *known_index == Bones::Invalid ) )
						return false;

					bool result = add_entry( name, ( int )*known_index, is_bone_group );
					bone_found |= result;
					return true;
				};

				bool specific_bone = default_to_specific_bone;
				bool ignore_root = false;
				const char* bone = main_search.c_str();

				if( BeginsWith( bone, "!" ) )
				{
					++bone;
					ignore_root = true;
				}

				if( BeginsWith( bone, "bone-" ) )
				{
					bone += 5;
					specific_bone = true;
				}
				else if( BeginsWith( bone, "group-" ) )
				{
					bone += 6;
					specific_bone = false;
				}

				if( StringCompare( bone, "random" ) == 0 )
				{
					unsigned safety = 0;
					while( safety++ < 10 && !bone_found )
					{
						if( specific_bone )
						{
							if( !controller )
							{
								if( !ignore_bad )
									HandleError( effect, fmt::format( "Unable to add particle effect because using a named bone doesn't work on a fixed mesh: {}", bone_group_name ) );
								return false;
							}

							const unsigned bone_index = generate_random_number( ( unsigned )( include_sockets ? controller->GetNumBonesAndSockets() : controller->GetNumInfluenceBones() ) - ( ignore_root ? 1u : 0u ) ) + ( ignore_root ? 1u : 0u );
							try_add( controller->GetBoneNameByIndex( bone_index ), bone_index, false );
						}
						else
						{
							const unsigned bone_group_index = generate_random_number( ( unsigned )bone_groups_component->static_ref.NumBoneGroups() );
							try_add( bone_groups_component->static_ref.BoneGroupName( bone_group_index ), bone_group_index, true );
						}
					}

					if( !bone_found )
						continue;

					// Found a valid bone, break out of the loop
					break;
				}

				const bool all = StringCompare( bone, "all" ) == 0;

				if( all || ( Contains( main_search, '*' ) && main_search.length() > 1 ) )
				{
					if( specific_bone )
					{
						if( !controller )
						{
							if( !ignore_bad )
								HandleError( effect, fmt::format( "Unable to add particle effect because using a named bone doesn't work on a fixed mesh: {}", bone_group_name ) );
							return false;
						}

						for( unsigned i = ( ignore_root ? 1 : 0 ); i < ( unsigned )( include_sockets ? controller->GetNumBonesAndSockets() : controller->GetNumInfluenceBones() ); ++i )
						{
							const auto& name = controller->GetBoneNameByIndex( i );
							if( all || WildcardSearch( name, main_search ) )
								try_add( name, i, false );
						}
					}
					else
					{
						for( unsigned i = 0; i < bone_groups_component->static_ref.NumBoneGroups(); ++i )
							if( all || WildcardSearch( bone_groups_component->static_ref.BoneGroupName( i ), main_search ) )
								try_add( bone_groups_component->static_ref.BoneGroupName( i ), i, true );
					}

					if( !bone_found )
						continue;

					// Found a valid bone, break out of the loop
					break;
				}

				unsigned bone_index = -1;

				if( specific_bone )
				{
					if( StringCompare( bone, "<root>" ) == 0 )
						bone_index = Bones::Root;
					else if( StringCompare( bone, "*" ) == 0 || StringCompare( bone, "<lock>" ) == 0 )
						bone_index = Bones::Lock;
					else if( controller )
						bone_index = controller->GetBoneIndexByName( bone );
				}
				else
					bone_index = bone_groups_component->static_ref.BoneGroupIndexByName( bone );
					
				if( ( !specific_bone && bone_index != -1 ) ||
					( specific_bone && bone_index != Bones::Invalid ) )
				{
					bool valid_add = try_add( std::string( bone ), bone_index, !specific_bone );

					if( !valid_add && !bone_found )
					{
						if( !ignore_bad )
							HandleError( effect, fmt::format( "Unable to add particle effect because a named bone/group was directly invalidated by an exclusion, almost certainly a mistake: {}", bone_group_name ) );
						return false;
					}

					if( !bone_found )
						continue;

					// Found a valid bone, break out of the loop
					break;
				}
			}

			return bone_found;
		}

		void EffectPack::AddEffectPackVisuals( RefCountedEffect& effect, const bool ignore_bad_final )
		{
			if( !IsEffectsEnabled() && !Contains( enabled_epk_exceptions, effect.effect_pack ) )
				return;

			// do not add particles/trails/objects if the epk is for specific submeshes
			if( !effect.filter_submesh )
			{
				// Check pre-conditions first so that we don't end up in a bad state later (i.e. partially adding an invalid .epk)
				auto* attached_object_component = GetObject().GetComponent< ClientAttachedAnimatedObject >();
				auto* particle_effects_component = ( ( int )static_ref.particle_effects_index >= 0 ) ? GetObject().GetComponent< ParticleEffects >( static_ref.particle_effects_index ) : nullptr;
				auto* bone_groups_component = ( ( int )static_ref.bone_groups_index >= 0 ) ? GetObject().GetComponent< BoneGroups >( static_ref.bone_groups_index ) : nullptr;
				auto* trail_effects_component = ( ( int )static_ref.trail_effects_index >= 0 ) ? GetObject().GetComponent< TrailsEffects >( static_ref.trail_effects_index ) : nullptr;
				const auto* client_animation_controller = GetObject().GetComponent< ClientAnimationController >();

				bool ignore_epk = false;
				auto handle_error = [&]( const std::string& error_msg )
				{
					HandleError( effect, error_msg );
					ignore_epk = true;
				};

				// PRE CONDITION TEST BEGIN
				Visual::EffectPack::ParticleEffects particle_effects;
				Visual::EffectPack::AttachedObjects attached_objects;
				Visual::EffectPack::AttachedObjectsEx attached_objects_ex;
				Visual::EffectPack::TrailEffects trail_effects;

				// Particles
				for( const auto& particle_effect : effect.effect_pack->GetParticles() )
				{
					const auto bone_found = TestBone( effect, ignore_bad_final, bone_groups_component, client_animation_controller, particle_effect.bone_name, [&]( const std::string& name, int index, bool is_bone_group )
						{
							particle_effects.push_back( { name, index, is_bone_group, particle_effect.ignore_errors, particle_effect.handle } );
							return true;
						}, !particle_effect.name_is_bone_group );

					if( !bone_found && !particle_effect.ignore_errors )
					{
						if( ignore_bad_final )
							return;
						handle_error( "Unable to add particle effect to bone group or bone named " + particle_effect.bone_name );
					}
				}

				// Attached ao's (both string bone name or index)
				for( const auto& attached : effect.effect_pack->GetAttachedObjects() )
				{
					const auto bone_to_search = attached.bone_index != Bones::Invalid
						? client_animation_controller->GetBoneNameByIndex( ( unsigned )attached.bone_index )
						: ( attached.bone_name.empty() ? "<root>" : attached.bone_name );

					const auto bone_found = TestBone( effect, ignore_bad_final, bone_groups_component, client_animation_controller, bone_to_search, [&]( const std::string& name, int index, bool is_bone_group )
					{
						if( is_bone_group )
						{
							if( bone_groups_component->static_ref.fixed_mesh_index != -1 )
								attached_objects.push_back( { name, Bones::Unset, true, attached.ignore_errors, attached.handle } );
							else for( const auto bone : bone_groups_component->static_ref.GetBoneGroup( ( unsigned )index ).bone_indices )
								attached_objects.push_back( { name, ( int )bone, false, attached.ignore_errors, attached.handle } );
						}
						else
						{
							attached_objects.push_back( { name, index, false, attached.ignore_errors, attached.handle } );
						}
						return !attached_objects.empty();
					}, !attached.name_is_bone_group );

					// Empty is allowed for attached objects (it just defaults to root bone)
					if( !bone_found && !attached.bone_name.empty() && !attached.ignore_errors )
					{
						if( ignore_bad_final )
							return;
						handle_error( fmt::format( "Unable to attach attached object to bone or bone group named \"{}\"", bone_to_search ) );
					}
				}

				// Extended attached ao's (allow for more customisability)
				for( const auto& attached : effect.effect_pack->GetAttachedObjectsEx() )
				{
					const auto bone_to_search = attached.bone_name.empty() ? "all" : attached.bone_name;
					Memory::SmallVector< std::pair< unsigned, std::string >, 16, Memory::Tag::Effects> bone_options;

					const auto bone_found = TestBone( effect, ignore_bad_final, bone_groups_component, client_animation_controller, bone_to_search, [&]( const std::string& name, int index, bool is_bone_group )
					{
						if( is_bone_group )
						{
							if( bone_groups_component->static_ref.fixed_mesh_index != -1 )
								bone_options.emplace_back( Bones::Unset, name );
							else for( const auto bone : bone_groups_component->static_ref.GetBoneGroup( ( unsigned )index ).bone_indices )
							{
								const auto& bone_name = client_animation_controller->GetBoneNameByIndex( bone );
								if( !attached.include_aux && BeginsWith( bone_name, "aux_" ) )
									continue;
								bone_options.push_back( std::pair( bone, bone_name ) );
							}
						}
						else
						{
							// Non group case will already be correctly filtered for aux in TestBone
							bone_options.push_back( std::pair( ( unsigned )index, name ) );
						}

						return !bone_options.empty();
					}, !attached.name_is_bone_group, attached.include_aux );

					if( !bone_found || bone_options.empty() )
					{
						if( attached.ignore_errors )
							continue;
						if( ignore_bad_final )
							return;
						handle_error( "Unable to add attached object ex effect to bone or bone group group named " + bone_to_search );
					}
					else
					{
						const auto multi_attach = attached.multi_attach;
						if( multi_attach )
							JumbleVector( bone_options );

						const auto min_spawn = multi_attach ? attached.min_spawn : std::min( attached.min_spawn, ( unsigned )bone_options.size() );
						const auto max_spawn = multi_attach ? attached.max_spawn : std::min( attached.max_spawn, ( unsigned )bone_options.size() );
						const auto num_spawns = generate_random_number( min_spawn, max_spawn );
						attached_objects_ex.reserve( ( size_t )num_spawns );

						for( unsigned i = 0; i < num_spawns; ++i )
						{
							const auto random_bone_index = multi_attach ? generate_random_number( 0, ( unsigned )bone_options.size() - 1 ) : i;
							attached_objects_ex.emplace_back();
							auto& new_entry = attached_objects_ex.back();
							new_entry.attached_objects = { RandomElement( attached.attached_objects ) };
							new_entry.name_is_bone_group = false;
							new_entry.bone_index = ( int )bone_options[ random_bone_index ].first;
							new_entry.bone_name = bone_options[ random_bone_index ].second;
							new_entry.min_scale = attached.min_scale;
							new_entry.max_scale = attached.max_scale;
							new_entry.min_rotation = attached.min_rotation;
							new_entry.max_rotation = attached.max_rotation;
						}
					}
				}

				// Trails
				for( const auto& trail_effect : effect.effect_pack->GetTrailEffects() )
				{
					const auto bone_found = TestBone( effect, ignore_bad_final, bone_groups_component, client_animation_controller, trail_effect.bone_name, [&]( const std::string& name, int index, bool is_bone_group )
						{
							trail_effects.push_back( { name, index, is_bone_group, trail_effect.ignore_errors, trail_effect.handle } );
							return true;
						}, !trail_effect.name_is_bone_group );

					if( !bone_found && !trail_effect.ignore_errors )
					{
						if( ignore_bad_final )
							return;
						handle_error( "Unable to add trail effect to bone or bone group named " + trail_effect.bone_name );
					}
				}

				if( particle_effects.size() > 0 && !particle_effects_component )
					handle_error( "Trying to add EPK with particles to AO without ParticleEffects component" );

				if( trail_effects.size() > 0 && !trail_effects_component )
					handle_error( "Trying to add EPK with trails to AO without TrailsEffects component" );

				if( ( effect.effect_pack->HasAttachedObjectsEx() || effect.effect_pack->HasAttachedObjects() ) && !attached_object_component )
					handle_error( "Trying to add EPK with attached objects to AO without AttachedAnimatedObject component" );

				// PRE CONDITION TEST END

				if( ignore_epk )
					return;

				// From this point onward, the .epk we are trying to add MUST be valid!


				// Particles adding
				for( const auto& particle_effect : particle_effects )
				{
					if( !particle_effect.name_is_bone_group )
					{
						if( auto instance = particle_effects_component->PlayEffectOnBone( ( unsigned )particle_effect.bone_index, particle_effect.handle ) )
							effect.particle_effects.push_back( instance );
					}
					else if( auto instance = particle_effects_component->PlayEffect( ( unsigned )particle_effect.bone_index, particle_effect.handle ) )
						effect.particle_effects.push_back( instance );
				}
				
				// Attached objects adding
				for( const auto& attached : attached_objects )
				{
					auto attached_object = std::make_shared< Animation::AnimatedObject >( attached.handle, GetObject().global_variables );
					attached_object_component->AddAttachedObject( attached_object, attached.bone_name.c_str(), effect.attached_transform, false, false, 0U, 0U, false, nullptr, false, std::numeric_limits< float >::infinity(), attached.bone_index );
					effect.attached_objects.push_back( std::move( attached_object ) );
				}

				// Extended attached objects adding
				for( const auto& attached : attached_objects_ex )
				{
					auto attached_object = std::make_shared< Animation::AnimatedObject >( attached.attached_objects[0], GetObject().global_variables );
					const auto min_scale = std::max( attached.min_scale, 1.f );
					const auto max_scale = std::max( attached.max_scale, attached.min_scale );
					const auto random_scale = simd::RandomFloatLocal( min_scale, max_scale );
					const auto random_rotation = simd::RandomFloatLocal(attached.min_rotation, attached.max_rotation) * PI / 180.f;
					const auto scale_matrix = simd::matrix::scale( simd::vector3( random_scale, random_scale, random_scale ) );
					const auto rotation_matrix = simd::matrix::yawpitchroll( random_rotation, random_rotation, random_rotation );
					const auto final_matrix = scale_matrix * rotation_matrix;
					attached_object_component->AddAttachedObject( attached_object, attached.bone_name.c_str(), final_matrix, false, false, 0U, 0U, false, nullptr, false, std::numeric_limits< float >::infinity(), attached.bone_index );
					effect.attached_objects.push_back( std::move( attached_object ) );
				}

				// Child attached objects adding
				for( const auto& attached : effect.effect_pack->GetChildAttachedObjects() )
				{
					attached_object_component->AddChildAttachedObjects( attached.type, simd::matrix::identity(), attached.from, attached.to, [&]( const std::shared_ptr< AnimatedObject >& parent, const std::shared_ptr< AnimatedObject >& ao )
					{
						effect.child_attached_objects.push_back( { parent, ao } );
					} );
					}

				// Trails adding
				for( const auto& trail_effect : trail_effects )
				{
					if( !trail_effect.name_is_bone_group )
					{
						if( auto instance = trail_effects_component->PlayEffectOnBone( trail_effect.bone_index, trail_effect.handle ) )
							effect.trail_effects.push_back( instance );
					}
					else if( auto instance = trail_effects_component->PlayEffect( trail_effect.bone_index, trail_effect.handle ) )
						effect.trail_effects.push_back( instance );
				}
			}

			auto* animated_render = GetObject().GetComponent< AnimatedRender >( static_ref.render_index );

			for (const auto& material_pass : effect.effect_pack->GetMaterialPasses())
			{
				animated_render->AddMaterialPass(
					material_pass.material,
					material_pass.apply_mode,
					GetSourceID(effect.effect_pack.GetResource().GetResource(), material_pass.index),
					effect.filter_submesh ? effect.effect_pack.GetResource().GetResource() : nullptr,
					effect.effect_pack->IsParentOnlyEffects() || !material_pass.apply_on_children);
			}
		}

		bool EffectPack::RemoveEffectPack( const Visual::EffectPackHandle& effect_pack, const bool ignore_bad /*= false*/ )
		{
			PROFILE;

			if( static_ref.disable_effect_packs )
				return true;

			const bool ignore_bad_final = ignore_bad || always_ignore_bad;

			const auto existing = std::find_if( effect_packs.begin(), effect_packs.end(), [&]( const RefCountedEffect& e ) { return e.effect_pack == effect_pack; } );
			
			//TODO: remove this when we can fix the actual cause of the Piety buff bug.
			//this happens sometimes with piety, seems to be latency related, but is causing a lot of client crashes on production. fail gracefully but log it.
			if( existing == effect_packs.end() )
			{
				if( !ignore_bad_final )
				{
					LOG_CRIT( "Attempted to remove an effect pack, \"" << effect_pack.GetFilename() << " \", from animated object \"" << GetObject().GetType().GetFilename() << "\" but the effect pack is not on the object."  );
				}
				return false;
			}

			return RemoveEffectPackInternal( existing, ignore_bad_final );
		}

		void EffectPack::RemoveAllEffectPacks()
		{
			PROFILE;

			bool should_continue = true;

			while( !effect_packs.empty() && should_continue )
			{
				should_continue = false;

				for( auto& epk : effect_packs )
				{
					if( RemoveEffectPack( epk.effect_pack ) )
					{
						should_continue = true;
						break;
					}
				}
			}
		}

		bool EffectPack::RemoveEffectPackInternal( const Memory::Vector< RefCountedEffect, Memory::Tag::Effects >::iterator existing, const bool ignore_bad /*= false*/ )
		{
			PROFILE;

			if( existing == effect_packs.end() )
				return false;

			if( existing->ref_count > 1 )
			{
				--existing->ref_count;
			}
			else
			{
				RemoveEffectPackVisuals( *existing );

				const auto end_effect_pack = existing->effect_pack->GetEndEffectPack();

				effect_packs.erase( existing );

				EffectsUtility::AddEffectPack( GetObject(), end_effect_pack );
			}

			return true;
		}

		void EffectPack::RemoveEffectPackVisuals( RefCountedEffect& effect )
		{
			auto* animated_render = GetObject().GetComponent< AnimatedRender >( static_ref.render_index );

			for (const auto& material_pass : effect.effect_pack->GetMaterialPasses())
			{
				animated_render->RemoveMaterialPass(material_pass.material, GetSourceID(effect.effect_pack.GetResource().GetResource(), material_pass.index));
			}

			for( auto particle_effect = effect.particle_effects.begin(); particle_effect != effect.particle_effects.end(); ++particle_effect )
			{
				( *particle_effect )->StopEmitting();
			}

			auto* attached_component = GetObject().GetComponent< AttachedAnimatedObject >();

			for( const auto& attached : Utility::make_range_r( effect.child_attached_objects ) )
			{
				if( IsWeakPtrNull( attached.first ) )  // Is attached to this object
				{
					if( attached_component->IsObjectAttached( attached.second ) )
						attached_component->RemoveAttachedObject( attached.second );
				}
				else if( const auto parent = attached.first.lock() )
				{
					if( auto* parent_attached_component = parent->GetComponent< AttachedAnimatedObject >() )
					{
						if( parent_attached_component->IsObjectAttached( attached.second ) )
							parent_attached_component->RemoveAttachedObject( attached.second );
					}
				}
			}

			if( attached_component )
			{
				for( auto attached = effect.attached_objects.begin(); attached != effect.attached_objects.end(); ++attached )
				{
					//need to check this here as some monsters (e.g. the Scavenger boss) have on_death = "DetachAll();" in the .otc file, and
					//currently that detaches all attached objects, including the ones added at runtime. If we find a way in the future to remove
					//only those attached objects which weren't added at runtime, we can take this out.
					//For now, this prevents a crash if Hailrake or the Weaver die with an elementally proliferating status effect on them.
					if( attached_component->IsObjectAttached( *attached ) )
						attached_component->RemoveAttachedObject( *attached );
				}
			}

			for( auto trail_effect = effect.trail_effects.begin(); trail_effect != effect.trail_effects.end(); ++trail_effect )
			{
				( *trail_effect )->StopEmitting();
			}

			effect.particle_effects.clear();
			effect.attached_objects.clear();
			effect.child_attached_objects.clear();
			effect.trail_effects.clear();
		}

		void EffectPack::OnAnimationEvent( const ClientAnimationController& controller, const Event& e, float time_until_trigger)
		{
			if( e.event_type == EffectPackEventType )
			{
				const auto& effect_event = static_cast< const EffectPackStatic::EffectPackEvent& >( e );

				const float duration = effect_event.length / controller.GetCurrentAnimationSpeed();
				
				try
				{
					if( effect_event.bone_name.empty() )
					{
						AddEffectPack( effect_event.effect_pack, false, false, duration, simd::matrix::identity(), effect_event.remove_on_animation_end );
					}
					else
					{
						AddChildEffectPack( effect_event.effect_pack, effect_event.bone_name, duration, effect_event.remove_on_animation_end );
					}
				}
				catch( const File::EPKError& e )
				{
#if defined(DEVELOPMENT_CONFIGURATION) || defined(TESTING_CONFIGURATION) || defined(STAGING_CONFIGURATION)
					LOG_CRIT( e.what() );
#endif
				}
			}
		}

		void EffectPack::OnAnimationStart( const CurrentAnimation& animation, const bool blend )
		{
			RemoveEffectPacksOnAnimationEnd();
		}

		void EffectPack::OnAnimationEnd( const ClientAnimationController& controller, const unsigned animation_index, float time_until_trigger)
		{
			RemoveEffectPacksOnAnimationEnd();
		}

		void EffectPack::RemoveEffectPacksOnAnimationEnd()
		{
			for( size_t i = effect_packs.size(); i-- > 0; )
			{
				auto& epk = effect_packs[ i ];

				const unsigned num_to_remove = std::min< unsigned >( epk.ref_count, epk.remove_on_animation_end + ( epk.remove_after_duration_or_animation_end > 0.0f ) );
				if( num_to_remove > 0 )
				{
					epk.remove_on_animation_end = false;
					epk.remove_after_duration_or_animation_end = 0.0f;
					for( unsigned count = 0; count < num_to_remove; ++count )
						RemoveEffectPackInternal( effect_packs.begin() + i );
				}
			}

			for( size_t i = child_effect_packs.size(); i-- > 0; )
			{
				auto& child_epk = child_effect_packs[ i ];

				child_epk.remove_on_animation_end = false;
				child_epk.remove_after_duration_or_animation_end = 0.0f;

				if( child_epk.IsFinished() )
					child_effect_packs.erase( child_effect_packs.begin() + i );
			}
		}

		void EffectPack::AddChildEffectPack( const Visual::EffectPackHandle& effect_pack, const std::string& bone_name, const float remove_after_duration, const bool remove_on_animation_end )
		{
			auto* attached_object_component = GetObject().GetComponent< AttachedAnimatedObject >();
			if( !attached_object_component )
				return;

			assert( remove_after_duration >= 0.0f );

			for( auto& attached_ao : attached_object_component->GetAttachedObjects() )
			{
				if( attached_ao.bone_name != bone_name )
					continue;

				for( auto& child_epk : child_effect_packs )
				{
					if( child_epk.applied_epk.GetEffectPack() == effect_pack && child_epk.applied_epk.GetAO() == attached_ao.object )
					{
						if( remove_after_duration > 0.0f )
						{
							auto& existing_remove_after_duration = remove_on_animation_end ? child_epk.remove_after_duration_or_animation_end : child_epk.remove_after_duration;
							existing_remove_after_duration = std::max( existing_remove_after_duration, remove_after_duration );
							return;
						}
						else if( remove_on_animation_end )
						{
							child_epk.remove_on_animation_end = true;
							return;
						}
					}
				}

				if( auto* child_epk_component = attached_ao.object->GetComponent< EffectPack >() )
				{
					if( remove_after_duration <= 0.0f && !remove_on_animation_end )
					{
						child_epk_component->AddEffectPack( effect_pack );  // Indefinite duration
					}
					else
					{
						child_effect_packs.push_back( { AppliedEffectPackHandle( attached_ao.object, effect_pack ) } );
						if( remove_after_duration > 0.0f && remove_on_animation_end )
							child_effect_packs.back().remove_after_duration_or_animation_end = remove_after_duration;
						else if( remove_after_duration > 0.0f )
							child_effect_packs.back().remove_after_duration = remove_after_duration;
						else
							child_effect_packs.back().remove_on_animation_end = remove_on_animation_end;
					}
					return;
				}
			}

#if defined(DEVELOPMENT_CONFIGURATION) || defined(TESTING_CONFIGURATION) || defined(STAGING_CONFIGURATION)
			LOG_CRIT( L"Failed to find target AO for child effect pack " << effect_pack.GetFilename() << L" at bone " << StringToWstring( bone_name ) << L" of AO " << GetObject().GetType().GetFilename() );
#endif
		}

		bool EffectPack::HasEffectPack( const Visual::EffectPackHandle& effect_pack )
		{
			const auto existing = std::find_if( effect_packs.begin(), effect_packs.end(), [&]( const RefCountedEffect& e ) { return e.effect_pack == effect_pack; } );
			return existing != effect_packs.end();
		}

		float EffectPack::GetEffectPackDuration( const Visual::EffectPackHandle& effect_pack )
		{
			const auto existing = std::find_if( effect_packs.begin(), effect_packs.end(), [&]( const RefCountedEffect& e ) { return e.effect_pack == effect_pack; } );
			if( existing != effect_packs.end() )
				return existing->remove_after_duration;
			else
				return 0.0f;
		}

		void EffectPack::FrameMove( const float elapsed_time )
		{
			for( size_t i = effect_packs.size(); i-- > 0; )
			{
				auto& epk = effect_packs[ i ];

				if( !enabled && !Contains( enabled_epk_exceptions, epk.effect_pack ) )
					continue;

				bool removed = false;
				if( epk.remove_after_duration != 0.0f )
				{
					epk.remove_after_duration -= elapsed_time;

					if( epk.remove_after_duration <= 0.0f )
					{
						epk.remove_after_duration = 0.0f;
						removed = epk.ref_count == 1;
						RemoveEffectPackInternal( effect_packs.begin() + i );
					}
				}

				if( !removed && epk.remove_after_duration_or_animation_end != 0.0f )
				{
					epk.remove_after_duration_or_animation_end -= elapsed_time;

					if( epk.remove_after_duration_or_animation_end <= 0.0f )
					{
						epk.remove_after_duration_or_animation_end = 0.0f;
						removed = epk.ref_count == 1;
						RemoveEffectPackInternal( effect_packs.begin() + i );
					}
				}

				if( !removed && epk.play_start_epk_after_delay > 0.0f )
				{
					epk.play_start_epk_after_delay -= elapsed_time;

					if( epk.play_start_epk_after_delay < 0.0f )
						EffectsUtility::AddEffectPack( GetObject(), epk.effect_pack->GetBeginEffectPack() );
				}
			}

			for( size_t i = child_effect_packs.size(); i-- > 0; )
			{
				auto& child_epk = child_effect_packs[ i ];

				child_epk.remove_after_duration = std::max( 0.0f, child_epk.remove_after_duration - elapsed_time );
				child_epk.remove_after_duration_or_animation_end = std::max( 0.0f, child_epk.remove_after_duration_or_animation_end - elapsed_time );

				if( child_epk.IsFinished() )
					child_effect_packs.erase( child_effect_packs.begin() + i );
			}
		}

		void EffectPack::GetComponentInfo( std::wstringstream & stream, const bool rescursive, const unsigned recursive_depth ) const
		{
			for( const auto& effect_pack : effect_packs )
			{
				stream << StreamHelp::NTabs( recursive_depth ) << L"Effect Pack: " << effect_pack.effect_pack.GetFilename() << L"\n";
			}
		}

		void EffectPack::EnableEffects()
		{
			if( enabled )
				return;

			enabled = true;

			for( auto& entry : effect_packs )
				if( !Contains( enabled_epk_exceptions, entry.effect_pack ) )
					AddEffectPackVisuals( entry, true );

			enabled_epk_exceptions = {};
		}

		void EffectPack::DisableEffects( const std::vector< Visual::EffectPackHandle >& exceptions )
		{
			enabled_epk_exceptions = exceptions;

			if( !enabled )
				return;

			for( auto& entry : effect_packs )
				if( !Contains( enabled_epk_exceptions, entry.effect_pack ) )
					RemoveEffectPackVisuals( entry );

			enabled = false;
		}

		EffectPack::EffectPack( const EffectPackStatic& static_ref, Objects::Object& object )
			: AnimationObjectComponent( object ), static_ref( static_ref )
		{
			if( static_ref.client_animation_controller_index != -1 )
			{
				auto* client_animation_controller = GetObject().GetComponent< ClientAnimationController >( static_ref.client_animation_controller_index );
				client_animation_controller->AddListener( *this );
			}
		}

		EffectPack::~EffectPack()
		{
			if( static_ref.client_animation_controller_index != -1 )
			{
				auto* client_animation_controller = GetObject().GetComponent< ClientAnimationController >( static_ref.client_animation_controller_index );
				client_animation_controller->RemoveListener( *this );
			}
		}

		void EffectPack::OnConstructionComplete()
		{
			for( const auto& epk : static_ref.effect_packs )
				AddEffectPack( epk );
		}
	}

	AppliedEffectPackHandle::AppliedEffectPackHandle( const std::shared_ptr< AnimatedObject >& ao, const Visual::EffectPackHandle& effect_pack ) : ao( ao ), effect_pack( effect_pack )
	{
		if( effect_pack && ao && !ao->IsDestructing() )
			ao->GetComponent< Components::EffectPack >()->AddEffectPack( effect_pack );
	}

	AppliedEffectPackHandle& AppliedEffectPackHandle::operator=( AppliedEffectPackHandle&& rhs )
	{
		if( this != &rhs )
		{
			if( effect_pack )
			{
				auto ao_shared = GetAO();
				if( ao_shared && !ao_shared->IsDestructing() )
					ao_shared->GetComponent< Components::EffectPack >()->RemoveEffectPack( effect_pack );
			}

			effect_pack = std::move( rhs.effect_pack );
			ao = std::move( rhs.ao );
		}
		return *this;
	}

	AppliedEffectPackHandle::~AppliedEffectPackHandle()
	{
		if( !effect_pack )
			return;

		auto ao_shared = GetAO();
		if( ao_shared && !ao_shared->IsDestructing() )
			ao_shared->GetComponent< Components::EffectPack >()->RemoveEffectPack( effect_pack );
	}
}