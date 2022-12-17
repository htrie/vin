#include <iomanip>
#include <tuple>

#include "Visual/Device/Device.h"
#include "Visual/Trails/TrailsEffectInstance.h"
#include "Visual/Animation/Components/ComponentsStatistics.h"
#include "Visual/Entity/EntitySystem.h"
#include "Visual/Renderer/Scene/SceneSystem.h"
#include "Visual/Renderer/DrawCalls/DrawCall.h"
#include "Visual/Effects/EffectsManager.h"

#include "ClientInstanceCommon/Animation/AnimatedObject.h"
#include "ClientInstanceCommon/Animation/Components/AnimationControllerComponent.h"

#include "BoneGroupsComponent.h"
#include "AnimatedRenderComponent.h"
#include "ClientAnimationControllerComponent.h"
#include "TrailsEffectsComponent.h"

namespace Animation
{
	namespace Components
	{
		TrailsEffectsStatic::TrailsEffectsStatic( const Objects::Type& parent )
			: AnimationObjectStaticComponent( parent )
			, animation_controller_index( parent.GetStaticComponentIndex< AnimationControllerStatic >() )
			, bone_groups_index( parent.GetStaticComponentIndex< BoneGroupsStatic >() )
			, animated_render_index( parent.GetStaticComponentIndex< AnimatedRenderStatic >() )
			, client_animation_controller_index( parent.GetStaticComponentIndex< ClientAnimationControllerStatic >() )
			, current_animation( -1 )
		{
		}

		TrailsEffectsStatic::~TrailsEffectsStatic() = default;

		COMPONENT_POOL_IMPL(TrailsEffectsStatic, TrailsEffects, 1024)

		const char* TrailsEffectsStatic::GetComponentName( )
		{
			return "TrailsEffects";
		}

		struct TrailsEffectsStatic::ParsedEffects
		{
			Memory::SmallVector<std::pair<std::string, Trails::TrailsEffectHandle>, 8, Memory::Tag::Components> continuous;
			Memory::SmallVector<std::pair<std::string, TrailsEffectEvent*>, 8, Memory::Tag::Components> events;
		};

		bool TrailsEffectsStatic::SetValue(const std::string &name, const std::wstring &value)
		{
			if (!parsed_effects)
				parsed_effects = Memory::Pointer<ParsedEffects, Memory::Tag::Components>::make();

			//Function to parse value into bone group name and filename
			const auto ParseTrailsEffect = [&]( ) -> std::pair< std::string, Trails::TrailsEffectHandle >
			{
				std::wstringstream stream( value );
				std::wstring wide_bone_group;
				stream >> wide_bone_group;
				std::ws( stream );
				std::wstring filename;
				std::getline( stream , filename );

				return std::make_pair(WstringToString(wide_bone_group), Trails::TrailsEffectHandle( filename ) );
			};

			if( name == "continuous_effect" )
			{
				parsed_effects->continuous.push_back(ParseTrailsEffect());

				return true;
			}
			else if( name == "animation" )
			{
				assert( animation_controller_index != -1 );
				auto* animation_controller = parent_type.GetStaticComponent< AnimationControllerStatic >( animation_controller_index );
				const unsigned animation_index = animation_controller->GetAnimationIndexByName( WstringToString( value ) );
				if( animation_index == -1 )
					throw Resource::Exception( ) << value << L" animation not found when setting trails effects.";
				
				current_animation = animation_index;
				return true;
			}
			else
			{
				std::unique_ptr< TrailsEffectEvent > effect_event( new TrailsEffectEvent );

				//Assume that the name is actually a float value indicating the time into an animation that the event is
				if( sscanf_s( name.c_str(), "%f,%f", &effect_event->time, &effect_event->length ) == 0 )
					return false; //not a float value

				assert( animation_controller_index != -1 );

				if( current_animation == -1 )
					throw Resource::Exception( ) << L"No current animation set";
				
				auto parsed_effect = ParseTrailsEffect();
				effect_event->effect = parsed_effect.second;
				effect_event->animation_index = current_animation;

#ifndef PRODUCTION_CONFIGURATION
				effect_event->origin = parent_type.current_origin;
#endif
				trails_effect_events.push_back( std::move( effect_event ) );
				AnimationControllerStatic* animation_controller = parent_type.GetStaticComponent< AnimationControllerStatic >(animation_controller_index);
				animation_controller->AddEvent(trails_effect_events.back()->animation_index, *trails_effect_events.back());
				parsed_effects->events.push_back(std::make_pair(std::move(parsed_effect.first), trails_effect_events.back().get()));

				return true;
			}
			
			return false;
		}

		void TrailsEffectsStatic::CopyAnimationData( const int source_animation, const int destination_animation, std::function<bool( const::Animation::Event& )> predicate, std::function<void( const::Animation::Event&, ::Animation::Event& )> transform )
		{
			auto* animation_controller = parent_type.GetStaticComponent< AnimationControllerStatic >( animation_controller_index );

			std::vector< TrailsEffectEvent > copied;
			for( auto& e : trails_effect_events )
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
				trails_effect_events.emplace_back( std::make_unique< TrailsEffectEvent >( std::move( e ) ) );
				animation_controller->AddEvent( destination_animation, *trails_effect_events.back() );
			}
		}

		void TrailsEffectsStatic::AddTrailsEvent( std::unique_ptr< TrailsEffectEvent > trails_event )
		{
			assert( animation_controller_index != -1 );
			const unsigned animation_index = trails_event->animation_index;
			trails_effect_events.emplace_back( std::move( trails_event ) );
			//Add the event
			auto* animation_controller = parent_type.GetStaticComponent< AnimationControllerStatic >( animation_controller_index );
			animation_controller->AddEvent( animation_index, *trails_effect_events.back() );
			//Needs sorting now
			animation_controller->SortEvents();
		}

		void TrailsEffectsStatic::SaveComponent( std::wostream& stream ) const
		{
#ifndef PRODUCTION_CONFIGURATION
			const auto* parent = parent_type.parent_type->GetStaticComponent< TrailsEffectsStatic >();
			assert( parent );

			const auto* bone_groups = parent_type.GetStaticComponent< BoneGroupsStatic >( bone_groups_index );

			for( const ContinuousTrailsEffect& effect : continuous_trails_effects )
			{
				if( effect.bone_group_index == -1 )
					continue;
				if( effect.origin != parent_type.current_origin )
					continue;

				WriteCustom( stream << L"continuous_effect = \""
					<< bone_groups->BoneGroupName( effect.bone_group_index ).c_str() << L" "
					<< effect.effect.GetFilename() << L"\"" );
			}
			
			if( animation_controller_index != -1 )
			{
				const auto* animation_controller = parent_type.GetStaticComponent< AnimationControllerStatic >( animation_controller_index );

				unsigned current_animation = -1;
				for( const auto& effect_event : trails_effect_events )
				{
					if( effect_event->copy_of )
						continue;
					if( effect_event->bone_group_indices.empty() )
						continue;
					if( effect_event->origin != parent_type.current_origin )
						continue;

					if( current_animation != effect_event->animation_index )
					{
						current_animation = effect_event->animation_index;
						WriteValueNamed( L"animation", animation_controller->GetMetadataByIndex( current_animation ).name.c_str() );
					}

					std::string bone_group_names;
					SortAndRemoveDuplicates( effect_event->bone_group_indices );
					RemoveAll( effect_event->bone_group_indices, -1 );
					for( const unsigned bone_group_index : effect_event->bone_group_indices )
					{
						if( !bone_group_names.empty() )
							bone_group_names += ',';
						bone_group_names += bone_groups->BoneGroupName( bone_group_index );
					}

					PushIndent();
					WriteCustom( stream << effect_event->time << L',' << effect_event->length << L" = \""
						<< bone_group_names.c_str() << L" "
						<< effect_event->effect.GetFilename() << L"\"" );
				}
			}
#endif
		}

		void TrailsEffectsStatic::RemoveTrailsEvent( const TrailsEffectEvent& trails_event )
		{
			auto* animation_controller = parent_type.GetStaticComponent< AnimationControllerStatic >();
			animation_controller->RemoveEvent( trails_event );

			const auto found = std::find_if( trails_effect_events.begin(), trails_effect_events.end(), [&]( const std::unique_ptr< TrailsEffectEvent >& iter ) { return iter.get() == &trails_event; } );
			trails_effect_events.erase( found );
		}

		TrailsEffects::TrailsEffects( const TrailsEffectsStatic& static_ref, Objects::Object& object )
			: AnimationObjectComponent( object ), static_ref( static_ref )
		{
			if( static_ref.client_animation_controller_index != -1 )
			{
				auto *client_animation_controller = GetObject().GetComponent< ClientAnimationController >( static_ref.client_animation_controller_index);
				client_animation_controller->AddListener( *this );
			}
		}

		TrailsEffects::~TrailsEffects()
		{
			for (auto& trails_effect : continuous_trails_effects)
				trails_effect->StopEmitting();

			for (size_t i = 0; i < trails_event_effects.size(); ++i)
			{
				EffectEventInstance& instance = trails_event_effects[i];
				if (!instance.effect->IsComplete())
				{
					instance.effect->StopEmitting();
				}
			}

			for (size_t i = 0; i < bone_group_effects.size(); ++i)
			{
				BoneGroupEffectInstance& instance = bone_group_effects[i];
				if (!instance.effect->IsComplete())
				{
					instance.effect->StopEmitting();
				}
			}

			for( size_t i = 0; i < bone_effects.size(); ++i )
			{
				BoneEffectInstance& instance = bone_effects[i];
				if( !instance.effect->IsComplete() )
				{
					instance.effect->StopEmitting();
				}
			}

			if(static_ref.client_animation_controller_index != -1)
			{
				auto *client_animation_controller = GetObject().GetComponent< ClientAnimationController >(static_ref.client_animation_controller_index);
				client_animation_controller->RemoveListener(*this);
			}

			OrphanEffects();
		}

		void TrailsEffects::OrphanEffects()
		{
			if (GetObject().GetComponent< AnimatedRender >(static_ref.animated_render_index)->IsOrphanedEffectsEnabled())
			{
				Renderer::Scene::System::Get().GetOrphanedEffectsManager().Add(continuous_trails_effects);
				Renderer::Scene::System::Get().GetOrphanedEffectsManager().Add(OtherEffects());
			}

			trails_event_effects.clear();
			bone_group_effects.clear();
			bone_effects.clear();
		}

		void TrailsEffects::SetupSceneObjects(uint8_t scene_layers, Renderer::DynamicParameterLocalStorage& dynamic_parameters_storage,
			const Renderer::DrawCalls::Bindings& static_bindings, const Renderer::DrawCalls::Uniforms& static_uniforms)
		{
			if (!GetObject().GetComponent< AnimatedRender >(static_ref.animated_render_index)->IsOrphanedEffectsEnabled()) // Tools only (hijacking IsOrphanedEffectsEnabled to detect tools).
			{
				for (auto& effect : continuous_trails_effects)
				{
					effect->DestroyDrawCalls();  // TODO: remove
					effect->CreateDrawCalls(scene_layers, dynamic_parameters_storage, static_bindings, static_uniforms);
				}

				for (auto& effect : OtherEffects())
				{
					effect->DestroyDrawCalls();  // TODO: remove
					effect->CreateDrawCalls(scene_layers, dynamic_parameters_storage, static_bindings, static_uniforms);
				}
			}
		}

		void TrailsEffects::DestroySceneObjects()
		{
			if (!GetObject().GetComponent< AnimatedRender >(static_ref.animated_render_index)->IsOrphanedEffectsEnabled()) // Tools only (hijacking IsOrphanedEffectsEnabled to detect tools).
			{
				for (auto& effect : continuous_trails_effects)
					effect->DestroyDrawCalls();

				for (auto& effect : OtherEffects())
					effect->DestroyDrawCalls();
			}
		}

		void TrailsEffects::RemoveAllEffects()
		{
			continuous_trails_effects.clear();
			trails_event_effects.clear();
			bone_group_effects.clear();
			bone_effects.clear();
		}

		void TrailsEffects::PauseAllEffects()
		{
			for (auto& instance : continuous_trails_effects)
				instance->Pause();

			for (auto& event : trails_event_effects)
				event.effect->Pause();

			for (auto& group : bone_group_effects)
				group.effect->Pause();

			for (auto& bone : bone_effects)
				bone.effect->Pause();
		}

		void TrailsEffects::ResumeAllEffects()
		{
			for (auto& instance : continuous_trails_effects)
				instance->Resume();

			for (auto& event : trails_event_effects)
				event.effect->Resume();

			for (auto& group : bone_group_effects)
				group.effect->Resume();

			for (auto& bone : bone_effects)
				bone.effect->Resume();
		}

		void TrailsEffects::OnTeleport()
		{
			PauseAllEffects();
			ResumeAllEffects();
		}

		void TrailsEffects::RegenerateEmitters( )
		{
			if (auto* animated_render = GetObject().GetComponent< AnimatedRender >(static_ref.animated_render_index))
			{
				animated_render->RecreateSceneObjects([&]()
				{
					RemoveAllEffects();

					continuous_trails_effects.resize(static_ref.continuous_trails_effects.size());
					std::transform(static_ref.continuous_trails_effects.begin(), static_ref.continuous_trails_effects.end(), continuous_trails_effects.begin(),
						[&](const TrailsEffectsStatic::ContinuousTrailsEffect& effect_description) -> std::shared_ptr< Trails::TrailsEffectInstance >
						{
							const auto* bone_groups = GetObject().GetComponent< BoneGroups >(static_ref.bone_groups_index);
							if (bone_groups->static_ref.NumBoneGroups() <= effect_description.bone_group_index)
								return std::unique_ptr< Trails::TrailsEffectInstance >();
							auto positions = bone_groups->GetBonePositions(effect_description.bone_group_index);
							return std::make_shared< Trails::TrailsEffectInstance >(effect_description.effect, positions, animated_render->GetTransform(), nullptr, GetObject().IsHighPriority(), GetObject().UseNoDefaultShader(), 0.0f, 0.0f);
						}
					);
				});
			}
		}

		void TrailsEffects::ReinitialiseEmitters( )
		{
			for( auto& trails_effect : continuous_trails_effects )
			{
				if( trails_effect.get() )
					trails_effect->Reinitialise();
			}

			std::for_each( trails_event_effects.begin(), trails_event_effects.end(), [&]( EffectEventInstance& event_instance )
			{
				if( !event_instance.effect->IsStopped() )
					event_instance.effect->Reinitialise();
			});

			std::for_each( bone_group_effects.begin(), bone_group_effects.end(), [&]( BoneGroupEffectInstance& bone_group_instance )
			{
				if( !bone_group_instance.effect->IsStopped() )
					bone_group_instance.effect->Reinitialise();
			} );

			std::for_each( bone_effects.begin(), bone_effects.end(), [&]( BoneEffectInstance& bone_instance )
				{
					if( !bone_instance.effect->IsStopped() )
						bone_instance.effect->Reinitialise();
				} );
		}

		void TrailsEffects::RemoveContinuousEffects()
		{
			continuous_trails_effects.clear();
		}

		void TrailsEffects::RemoveOtherEffects()
		{
			auto* animated_render = GetObject().GetComponent< AnimatedRender >( static_ref.animated_render_index );

			if( static_ref.animation_controller_index != -1 )
			{
				const auto event_finished_begin = std::remove_if( trails_event_effects.begin( ), trails_event_effects.end( ), [&]( EffectEventInstance& instance ) -> bool
				{
					return instance.effect->IsComplete();
				} );
				trails_event_effects.erase( event_finished_begin, trails_event_effects.end( ) );
			}

			const auto bone_group_effects_finished_begin = std::remove_if( bone_group_effects.begin(), bone_group_effects.end(), [&]( BoneGroupEffectInstance& instance ) -> bool
			{
				return instance.effect->IsComplete();
			} );
			bone_group_effects.erase( bone_group_effects_finished_begin, bone_group_effects.end() );

			const auto bone_effects_finished_begin = std::remove_if( bone_effects.begin(), bone_effects.end(), [&]( BoneEffectInstance& instance ) -> bool
				{
					return instance.effect->IsComplete();
				} );
			bone_effects.erase( bone_effects_finished_begin, bone_effects.end() );
		}

		void TrailsEffects::RenderFrameMove( const float elapsed_time )
		{
			RenderFrameMoveInternal(elapsed_time);
		}

		void TrailsEffects::RenderFrameMoveNoVirtual( AnimationObjectComponent* component, const float elapsed_time )
		{
			static_cast<TrailsEffects*>(component)->RenderFrameMoveInternal(elapsed_time);
		}

		void TrailsEffects::RenderFrameMoveInternal( const float elapsed_time )
		{
			PROFILE;

			auto* animated_render = GetObject().GetComponent< AnimatedRender >( static_ref.animated_render_index );

			if (animated_render->IsEffectsPaused())
				return;

			bool high_res = false;
			float multiplier = 1.0f;

			if( static_ref.animation_controller_index != -1 )
			{
				const auto* animation_controller = GetObject().GetComponent< AnimationController >( static_ref.animation_controller_index );
				if (animation_controller)
				{
					high_res = true;
					if (animation_controller->UsesAnimSpeedRenderComponents())
						multiplier = animation_controller->GetCurrentAnimationSpeed() * animation_controller->GetGlobalSpeedMultiplier();
				}
			}

			const auto* bone_groups = GetObject().GetComponent< BoneGroups >(static_ref.bone_groups_index);

			const auto TickEffectPositions = [&](auto& effect, const auto& positions, float emitter_time)
			{
				effect.SetPosition(positions);
				effect.SetMultiplier(multiplier);
				effect.SetEmitterTime(emitter_time);

			#if defined(PROFILING)
				Stats::Tick(Type::TrailsEffects);
			#endif
			};

			const auto TickEffect = [&]( auto& effect, unsigned bone_group, float emitter_time)
			{
				TickEffectPositions(effect, bone_groups->GetBonePositions(bone_group), emitter_time);
				if (high_res)
					effect.SetAnimationStorage(animation_storage_id, bone_groups->static_ref.GetBoneGroup(bone_group).bone_indices);
			};

			//Tick continuous emitters
			unsigned effect_index = 0;
			for( auto& trails_effect : continuous_trails_effects )
			{
				if (trails_effect.get())
					TickEffect(*trails_effect, static_ref.continuous_trails_effects[effect_index].bone_group_index, 0.0f);
				++effect_index;
			}

			//Tick event emitters.
			//These only exist if we have animations
			if( static_ref.animation_controller_index != -1 && static_ref.client_animation_controller_index != -1 )
			{
				const auto& animation_controler = GetObject().GetComponent< AnimationController >(static_ref.animation_controller_index);
				const auto& client_animation_controler = GetObject().GetComponent< ClientAnimationController >(static_ref.client_animation_controller_index);

				const auto current_animation_position = client_animation_controler->GetCurrentAnimationPosition( );
				const auto current_animation_speed = client_animation_controler->GetCurrentAnimationSpeed() * animation_controler->GetGlobalSpeedMultiplier();
				float anim_length = animation_controler->GetCurrentAnimationLength();

				for( auto& instance : trails_event_effects )
				{
					float emitter_time = 0.0f;
					float stop_time = -1.0f;
					bool is_stopped = false;

					if( !instance.effect->IsStopped( ) )
					{
						if (instance.is_stopped)
						{
							is_stopped = true;
							stop_time = instance.time_until_stop;
							emitter_time = 1.0f + elapsed_time - stop_time;
						}
						else if (instance.trails_event->length != 0.0f)
						{
							const float end_time = instance.trails_event->time + instance.trails_event->length;
							if ((anim_length > 0.f && anim_length < instance.trails_event->length) || current_animation_position < instance.trails_event->time)
							{
								is_stopped = true;
								emitter_time = 1.0f + elapsed_time;
							}
							else
							{
								emitter_time = current_animation_position - instance.trails_event->time;
								if (current_animation_position >= end_time)
								{
									stop_time = current_animation_speed > 0.0f ? elapsed_time - ((current_animation_position - end_time) / current_animation_speed) : 0.0f;
									is_stopped = true;
								}
							}
						}
					}

					if (is_stopped)
						instance.effect->StopEmitting(stop_time);

					TickEffect( *instance.effect, instance.bone_group, emitter_time );
				}
			}

			//Tick manually added trail effects attached to bone groups.
			for( auto& instance : bone_group_effects )
				TickEffect( *instance.effect, instance.attach_index, 0.0f );

			//Tick manually added emitters attached to bones.
			for( auto& instance : bone_effects )
			{
				//Work out position of bone.
				const auto* client_animation_controller = GetObject().GetComponent< ClientAnimationController >( static_ref.client_animation_controller_index );
				Memory::SmallVector<simd::vector3, 16, Memory::Tag::Particle> positions( 2 );
				positions[0] = client_animation_controller->GetBonePosition( instance.attach_index );
				positions[1] = instance.at;

				TickEffectPositions(*instance.effect, positions, 0.0f);
			}

			RemoveOtherEffects();
		}

		void TrailsEffects::OnSleep()
		{
			OnSleepInternal();
		}
		
		void TrailsEffects::OnSleepNoVirtual(AnimationObjectComponent* component)
		{
			static_cast<TrailsEffects*>(component)->OnSleepInternal();
		}

		void TrailsEffects::OnSleepInternal()
		{
			OrphanEffects();
		}

		void TrailsEffects::OnAnimationEvent( const ClientAnimationController& controller, const Event& e, float time_until_trigger)
		{
			PROFILE;
			if( e.event_type != TrailsEffectEventType )
				return;

			const auto& effect_event = static_cast< const TrailsEffectsStatic::TrailsEffectEvent& >( e );

			for( const unsigned bone_group_index : effect_event.bone_group_indices )
			{
				//Check to see if this event is already playing
				const bool found = AnyOfIf( trails_event_effects, [&]( const EffectEventInstance& event_instance )
				{
					return event_instance.trails_event == &effect_event && event_instance.bone_group == bone_group_index && !event_instance.effect->IsStopped();
				} );
				
				if( !found )
					PlayEffect( bone_group_index, effect_event, effect_event.effect, time_until_trigger );
			}
		}

		void TrailsEffects::OnAnimationStart( const CurrentAnimation& animation, const bool blend)
		{
			const float global_speed_multiplier = GetObject().GetComponent< AnimationController >(static_ref.animation_controller_index)->GetGlobalSpeedMultiplier();
			trail_event_anim_speed = animation.speed_multiplier * global_speed_multiplier;

			std::for_each( trails_event_effects.begin(), trails_event_effects.end(), []( EffectEventInstance& event_instance )
			{
				event_instance.is_stopped = true;
			});
		}

		void TrailsEffects::OnAnimationEnd(const ClientAnimationController& controller, const unsigned animation_index, float time_until_trigger)
		{
			std::for_each(trails_event_effects.begin(), trails_event_effects.end(), [time_until_trigger](EffectEventInstance& event_instance)
			{
				event_instance.is_stopped = true;
				event_instance.time_until_stop = time_until_trigger;
			});
		}

		void TrailsEffects::OnAnimationSpeedChange(const CurrentAnimation& animation)
		{
			const float global_speed_multiplier = GetObject().GetComponent< AnimationController >(static_ref.animation_controller_index)->GetGlobalSpeedMultiplier();
			trail_event_anim_speed = animation.speed_multiplier * global_speed_multiplier;

			std::for_each(trails_event_effects.begin(), trails_event_effects.end(), [&](EffectEventInstance& event_instance)
			{
				event_instance.effect->SetAnimationSpeedMultiplier(trail_event_anim_speed);
			});
		}
		
		void TrailsEffects::OnAnimationPositionProgressed( const CurrentAnimation& animation, const float previous_time, const float new_time )
		{
			RenderFrameMove( new_time - previous_time );
		}

		std::shared_ptr< Trails::TrailsEffectInstance > TrailsEffects::PlayEffect( const unsigned bone_group_index, const Trails::TrailsEffectHandle& effect, float delay)
		{
			PROFILE;

			if (!Renderer::Scene::System::Get().GetDevice() || Renderer::Scene::System::Get().GetDevice()->RenderingPaused()) // Do not push new trails when rendering is paused to avoid accumulation.
				return std::shared_ptr<Trails::TrailsEffectInstance>();

			auto* animated_render = GetObject().GetComponent< AnimatedRender >( static_ref.animated_render_index );
			const auto* bone_groups = GetObject().GetComponent< BoneGroups >( static_ref.bone_groups_index );

			assert( bone_groups->static_ref.NumBoneGroups() > bone_group_index );
			auto positions = bone_groups->GetBonePositions( bone_group_index );

			BoneGroupEffectInstance bone_group_effect_instance;
			bone_group_effect_instance.attach_index = bone_group_index;
			bone_group_effect_instance.effect = std::make_shared< Trails::TrailsEffectInstance >(effect, positions, animated_render->GetTransform(), nullptr, GetObject().IsHighPriority(), GetObject().UseNoDefaultShader(), delay, 0.0f);
			bone_group_effects.push_back( std::move( bone_group_effect_instance ) );
			return bone_group_effects.back().effect;
		}

		std::shared_ptr< Trails::TrailsEffectInstance > TrailsEffects::PlayEffectOnBone( const unsigned bone_index, const Trails::TrailsEffectHandle& effect, float delay)
		{
			PROFILE;

			assert( static_ref.client_animation_controller_index != -1 /* Tried to play an effect on a bone of a mesh without bones. */ );

			if (!Renderer::Scene::System::Get().GetDevice() || Renderer::Scene::System::Get().GetDevice()->RenderingPaused()) // Do not push new trails when rendering is paused to avoid accumulation.
				return std::shared_ptr<Trails::TrailsEffectInstance>();

			auto* animated_render = GetObject().GetComponent< AnimatedRender >( static_ref.animated_render_index );
			const auto* bone_groups = GetObject().GetComponent< BoneGroups >( static_ref.bone_groups_index );

			const auto* client_animation_controller = GetObject().GetComponent< ClientAnimationController >( static_ref.client_animation_controller_index );
			Memory::SmallVector<simd::vector3, 16, Memory::Tag::Particle> positions( 1 );
			positions[0] = client_animation_controller->GetBonePosition( bone_index );

			BoneEffectInstance bone_effect_instance;
			bone_effect_instance.attach_index = bone_index;
			bone_effect_instance.effect = std::make_shared< Trails::TrailsEffectInstance >(effect, positions, animated_render->GetTransform(), nullptr, GetObject().IsHighPriority(), GetObject().UseNoDefaultShader(), delay, 0.0f);
			bone_effect_instance.at = positions[0];
			bone_effects.push_back( std::move( bone_effect_instance ) );
			return bone_effects.back().effect;
		}

		void TrailsEffects::PlayEffect( const unsigned bone_group_index, const TrailsEffectsStatic::TrailsEffectEvent& trails_event, const Trails::TrailsEffectHandle& effect, float delay)
		{
			PROFILE;

			if (!Renderer::Scene::System::Get().GetDevice() || Renderer::Scene::System::Get().GetDevice()->RenderingPaused()) // Do not push new trails when rendering is paused to avoid accumulation.
				return;

			auto* animated_render = GetObject().GetComponent< AnimatedRender >( static_ref.animated_render_index );
			const auto* bone_groups = GetObject().GetComponent< BoneGroups >( static_ref.bone_groups_index );

			if( bone_groups->static_ref.NumBoneGroups() <= bone_group_index )
				return;
			auto positions = bone_groups->GetBonePositions( bone_group_index );

			EffectEventInstance effect_event_instance;
			effect_event_instance.trails_event = &trails_event;
			effect_event_instance.bone_group = bone_group_index;
			effect_event_instance.effect = std::make_shared< Trails::TrailsEffectInstance >(effect, positions, animated_render->GetTransform(), nullptr, GetObject().IsHighPriority(), GetObject().UseNoDefaultShader(), delay, trails_event.length);
			effect_event_instance.effect->SetAnimationSpeedMultiplier(trail_event_anim_speed);
			trails_event_effects.push_back( std::move( effect_event_instance ) );
		}

		Memory::SmallVector<float, 8, Memory::Tag::Trail> TrailsEffects::GetEffectEndTimes()
		{
			PROFILE;

			Memory::SmallVector<float, 8, Memory::Tag::Trail> res;
			for (const auto& a : trails_event_effects)
				res.push_back(a.trails_event->time + a.trails_event->length);

			return res;
		}

		void TrailsEffectsStatic::OnTypeInitialised()
		{
			// BoneGroupIndexByName calls must be delayed until OnTypeInitialised() because 
			// BoneGroupsStatic adds mesh-defined bone groups in OnTypeInitialised().
			if (parsed_effects)
			{
				const auto* bone_groups = parent_type.GetStaticComponent< BoneGroupsStatic >(bone_groups_index);

				for (auto& effect : parsed_effects->continuous)
				{
					unsigned bone_group_index = bone_groups->BoneGroupIndexByName(effect.first);
					if (bone_group_index == -1)
						throw Resource::Exception() << L"No bone group with name " << effect.first.c_str();

					continuous_trails_effects.push_back(ContinuousTrailsEffect{ bone_group_index, effect.second });

#ifndef PRODUCTION_CONFIGURATION
					continuous_trails_effects.back().origin = parent_type.current_origin;
#endif
				}

				for (auto& effect : parsed_effects->events)
				{
					std::vector< std::string > bone_group_names;
					SplitString( bone_group_names, effect.first, ',' );
					for( const auto& group : bone_group_names )
					{
						unsigned bone_group_index = bone_groups->BoneGroupIndexByName( group );

						if( bone_group_index == -1 )
							throw Resource::Exception() << L"No bone group with name " << group.c_str();

						effect.second->bone_group_indices.push_back( bone_group_index );
					}
				}

				for (auto& event : trails_effect_events)
					if( event->copy_of && event->bone_group_indices.empty() )
						event->bone_group_indices = static_cast< const TrailsEffectEvent* >( event->copy_of )->bone_group_indices;

				parsed_effects = nullptr;
			}
		}
	}

}
