#include <iomanip>
#include <tuple>

#include "Common/Utility/Logger/Logger.h"

#include "Visual/Device/Device.h"
#include "Visual/Particles/EffectInstance.h"
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
#include "ParticleEffectsComponent.h"

namespace Animation
{
	namespace Components
	{
		ParticleEffectsStatic::ParticleEffectsStatic( const Objects::Type& parent )
			: AnimationObjectStaticComponent( parent )
			, animation_controller_index( parent.GetStaticComponentIndex< AnimationControllerStatic >() )
			, bone_groups_index( parent.GetStaticComponentIndex< BoneGroupsStatic >() )
			, animated_render_index( parent.GetStaticComponentIndex< AnimatedRenderStatic >() )
			, client_animation_controller_index( parent.GetStaticComponentIndex< ClientAnimationControllerStatic >() )
			, current_animation( -1 )
			, tick_when_not_visible( true )
		{
		}

		ParticleEffectsStatic::~ParticleEffectsStatic() = default;

		COMPONENT_POOL_IMPL(ParticleEffectsStatic, ParticleEffects, 1024)

		void ParticleEffectsStatic::CopyAnimationData( const int source_animation, const int destination_animation, std::function<bool( const Animation::Event& )> predicate, std::function<void( const Animation::Event&, Animation::Event& )> transform )
		{
			std::vector<ParticleEffectEvent> copied;
			for( auto& e : particle_effect_events )
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
				particle_effect_events.emplace_back( std::make_unique<ParticleEffectEvent>( std::move( e ) ) );
				animation_controller->AddEvent( destination_animation, *particle_effect_events.back() );
			}
		}

		const char* ParticleEffectsStatic::GetComponentName( )
		{
			return "ParticleEffects";
		}

		struct ParticleEffectsStatic::ParsedEffects
		{
			Memory::SmallVector<std::pair<std::string, ContinuousParticleEffect>, 8, Memory::Tag::Components> continuous;
			Memory::SmallVector<std::pair<std::string, ParticleEffectEvent*>, 8, Memory::Tag::Components> events;
		};

		bool ParticleEffectsStatic::SetValue(const std::string &name, const std::wstring &value)
		{
			if (!parsed_effects)
				parsed_effects = Memory::Pointer<ParsedEffects, Memory::Tag::Components>::make();

			//Function to parse value into bone group name and filename
			const auto ParseParticleEffect = [&]( ) -> std::pair< std::string, Particles::ParticleEffectHandle >
			{
				std::wstringstream stream( value );
				std::wstring wide_bone_group;
				stream >> wide_bone_group;
				std::ws( stream );
				std::wstring filename;
				std::getline( stream , filename );

				return std::make_pair(WstringToString(wide_bone_group), Particles::ParticleEffectHandle( filename ) );
			};

			if( name == "continuous_effect" )
			{
				auto parsed_effect = ParseParticleEffect();
				ContinuousParticleEffect continuous_effect;
				continuous_effect.effect = parsed_effect.second;
#ifndef PRODUCTION_CONFIGURATION
				continuous_effect.origin = parent_type.current_origin;
#endif
				parsed_effects->continuous.push_back( std::make_pair( std::move( parsed_effect.first ), std::move( continuous_effect ) ) );

				return true;
			}
			else if (name == "animation")
			{
				assert(animation_controller_index != -1);
				auto* animation_controller = parent_type.GetStaticComponent< AnimationControllerStatic >(animation_controller_index);
				const unsigned animation_index = animation_controller->GetAnimationIndexByName(WstringToString(value));
				if (animation_index == -1)
					throw Resource::Exception() << value << L" animation not found when setting particle effects.";

				current_animation = animation_index;
				return true;
			}
			else
			{
				if (current_animation == -1)
					throw Resource::Exception() << L"No current animation set";

				auto effect_event = std::make_unique< ParticleEffectEvent >();

				//Assume that the name is actually a float value indicating the time into an animation that the event is
				if (sscanf_s(name.c_str(), "%f,%f", &effect_event->time, &effect_event->length) == 0)
				{
					if (sscanf_s(name.c_str(), "%f", &effect_event->time) == 0)
						return false; //not a float value

					effect_event->length = 0.0f;
				}

				assert(animation_controller_index != -1);

				auto parsed_effect = ParseParticleEffect();
				effect_event->effect = parsed_effect.second;
				effect_event->animation_index = current_animation;

#ifndef PRODUCTION_CONFIGURATION
				effect_event->origin = parent_type.current_origin;
#endif
				particle_effect_events.push_back(std::move(effect_event));
				AnimationControllerStatic* animation_controller = parent_type.GetStaticComponent< AnimationControllerStatic >(animation_controller_index);
				animation_controller->AddEvent(particle_effect_events.back()->animation_index, *particle_effect_events.back());
				parsed_effects->events.push_back(std::make_pair(std::move(parsed_effect.first), particle_effect_events.back().get()));

				return true;
			}
			
			return false;
		}

		bool ParticleEffectsStatic::SetValue( const std::string &name, const bool value )
		{
			if ( name == "tick_when_not_visible" )
			{
				tick_when_not_visible = value;
				return true;
			}

			return false;
		}




		void ParticleEffectsStatic::AddParticleEvent( std::unique_ptr< ParticleEffectEvent > particle_event )
		{
			assert( animation_controller_index != -1 );
			const unsigned animation_index = particle_event->animation_index;
			particle_effect_events.emplace_back( std::move( particle_event ) );
			//Add the event
			auto* animation_controller = parent_type.GetStaticComponent< AnimationControllerStatic >( animation_controller_index );
			animation_controller->AddEvent( animation_index, *particle_effect_events.back() );
			//Needs sorting now
			animation_controller->SortEvents();
		}

		void ParticleEffectsStatic::SaveComponent( std::wostream& stream ) const
		{
#ifndef PRODUCTION_CONFIGURATION
			const auto* parent = parent_type.parent_type->GetStaticComponent< ParticleEffectsStatic >();
			assert( parent );

			stream << std::fixed; //so that floats never end up saved as ints
			const auto* bone_groups = parent_type.GetStaticComponent< BoneGroupsStatic >( bone_groups_index );

			for( const ContinuousParticleEffect& effect : continuous_particle_effects )
			{
				if( effect.bone_group_index == -1 )
					continue;
				if( effect.origin != parent_type.current_origin )
					continue;
				WriteCustom( stream << L"continuous_effect = \""
					<< bone_groups->BoneGroupName( effect.bone_group_index ).c_str() << L" "
					<< effect.effect.GetFilename() << L"\"" );
			}
			
			if (animation_controller_index != -1)
			{
				const auto* animation_controller = parent_type.GetStaticComponent< AnimationControllerStatic >(animation_controller_index);

				unsigned current_animation = -1;
				for( const auto& effect_event : particle_effect_events)
				{
					if (effect_event->bone_group_indices.empty())
						continue;
					if (effect_event->copy_of)
						continue;
					if( effect_event->origin != parent_type.current_origin )
						continue;

					if (current_animation != effect_event->animation_index)
					{
						current_animation = effect_event->animation_index;
						WriteValueNamed(L"animation", animation_controller->GetMetadataByIndex(current_animation).name.c_str());
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
					WriteCustom(stream << effect_event->time << L',' << effect_event->length << L" = \""
						<< bone_group_names.c_str() << L" "
						<< effect_event->effect.GetFilename() << L"\"");
				}
			}

			WriteValueConditional( tick_when_not_visible, tick_when_not_visible != parent->tick_when_not_visible );
#endif
		}

		void ParticleEffectsStatic::RemoveParticleEvent( const ParticleEffectEvent& particle_event )
		{
			auto* animation_controller = parent_type.GetStaticComponent< AnimationControllerStatic >();
			animation_controller->RemoveEvent( particle_event );

			const auto found = std::find_if( particle_effect_events.begin(), particle_effect_events.end(), [&]( const std::unique_ptr< ParticleEffectEvent >& iter ) { return iter.get() == &particle_event; } );
			particle_effect_events.erase( found );
		}


		ParticleEffects::ParticleEffects( const ParticleEffectsStatic& static_ref, Objects::Object& object )
			: AnimationObjectComponent( object ), static_ref( static_ref ), particle_event_anim_speed( 1.f )
		{
			if( static_ref.client_animation_controller_index != -1 )
			{
				auto *client_animation_controller = GetObject().GetComponent< ClientAnimationController >( static_ref.client_animation_controller_index);
				client_animation_controller->AddListener( *this );
			}
		}

		ParticleEffects::~ParticleEffects()
		{
			for (auto& effect : continuous_particle_effects)
				if(effect)
					effect->StopEmitting();

			for ( size_t i = 0; i < particle_event_effects.size(); ++i )
			{
				EffectEventInstance& instance = particle_event_effects[i];
				if ( !instance.effect->EffectComplete() )
				{
					instance.effect->StopEmitting();
				}
			}

			for (size_t i = 0; i < infinite_particle_event_effects.size(); ++i)
			{
				EffectEventInstance& instance = infinite_particle_event_effects[i];
				if (!instance.effect->EffectComplete())
				{
					instance.effect->StopEmitting();
				}
			}

			for ( size_t i = 0; i < bone_group_effects.size(); ++i )
			{
				BoneGroupEffectInstance& instance = bone_group_effects[i];
				if ( !instance.effect->EffectComplete() )
				{
					instance.effect->StopEmitting();
				}
			}

			for ( size_t i = 0; i < bone_effects.size(); ++i )
			{
				BoneEffectInstance& instance = bone_effects[i];
				if ( !instance.effect->EffectComplete() )
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

		void ParticleEffects::OrphanEffects()
		{
			if (GetObject().GetComponent< AnimatedRender >(static_ref.animated_render_index)->IsOrphanedEffectsEnabled())
			{
				auto other_effects = OtherEffects();
				for (auto& effect : other_effects)
				{
					effect->DetachGpuEmitters(); //GPU particles don't need to be orphaned because their lifetime is managed in the GpuParticleSystem, so they need to be just detached from the effect instance
				}
				Renderer::Scene::System::Get().GetOrphanedEffectsManager().Add(other_effects);
			}

			particle_event_effects.clear();
			infinite_particle_event_effects.clear();
			bone_group_effects.clear();
			bone_effects.clear();
		}

		void ParticleEffects::SetupSceneObjects(uint8_t scene_layers, Renderer::DynamicParameterLocalStorage& dynamic_parameters_storage,
			const Renderer::DrawCalls::Bindings& static_bindings, const Renderer::DrawCalls::Uniforms& static_uniforms)
		{
			for (auto& effect : continuous_particle_effects)
			{
				effect->DestroyDrawCalls();  // TODO: remove
				effect->CreateDrawCalls(scene_layers, dynamic_parameters_storage, static_bindings, static_uniforms);
			}

			if (!GetObject().GetComponent< AnimatedRender >(static_ref.animated_render_index)->IsOrphanedEffectsEnabled()) // Tools only (hijacking IsOrphanedEffectsEnabled to detect tools).
			{
				for (auto& effect : OtherEffects())
				{
					effect->DestroyDrawCalls();  // TODO: remove
					effect->CreateDrawCalls(scene_layers, dynamic_parameters_storage, static_bindings, static_uniforms);
				}
			}
		}

		void ParticleEffects::DestroySceneObjects()
		{
			for (auto& effect : continuous_particle_effects)
				effect->DestroyDrawCalls();

			if (!GetObject().GetComponent< AnimatedRender >(static_ref.animated_render_index)->IsOrphanedEffectsEnabled()) // Tools only (hijacking IsOrphanedEffectsEnabled to detect tools).
			{
				for (auto& effect : OtherEffects())
					effect->DestroyDrawCalls();
			}
		}

		void ParticleEffects::RemoveAllEffects()
		{
			continuous_particle_effects.clear();
			particle_event_effects.clear();
			infinite_particle_event_effects.clear();
			bone_group_effects.clear();
			bone_effects.clear();
		}

		void ParticleEffects::OnTeleport()
		{
			ProcessAllEffects([](const auto& effect)  { effect->OnTeleport(); });
		}

		void ParticleEffects::RegenerateEmitters( )
		{
			if (auto* animated_render = GetObject().GetComponent< AnimatedRender >(static_ref.animated_render_index))
			{
				animated_render->RecreateSceneObjects([&]()
				{
					RemoveAllEffects();

					continuous_particle_effects.resize(static_ref.continuous_particle_effects.size());
					std::transform(static_ref.continuous_particle_effects.begin(), static_ref.continuous_particle_effects.end(), continuous_particle_effects.begin(),
						[&](const ParticleEffectsStatic::ContinuousParticleEffect& effect_description) -> std::shared_ptr< Particles::EffectInstance >
						{
							const auto* bone_groups = GetObject().GetComponent< BoneGroups >(static_ref.bone_groups_index);
							if (bone_groups->static_ref.NumBoneGroups() <= effect_description.bone_group_index)
								return std::unique_ptr< Particles::EffectInstance >();
							auto positions = bone_groups->GetBonePositions(effect_description.bone_group_index);
							return std::make_shared< Particles::EffectInstance >(effect_description.effect, positions, animated_render->GetTransform(), nullptr, GetObject().IsHighPriority(), GetObject().UseNoDefaultShader(), 0.0f, -1.0f);
						}
					);
				});
			}
		}

		void ParticleEffects::ReinitialiseEmitters( )
		{
			for( auto& particle_effect : continuous_particle_effects )
			{
				if( particle_effect.get() )
					particle_effect->Reinitialise();
			}

			std::for_each(infinite_particle_event_effects.begin(), infinite_particle_event_effects.end(), [](EffectEventInstance& event_instance)
			{
				event_instance.effect->Reinitialise();
			});

			for (auto& instance : bone_group_effects)
				instance.effect->Reinitialise();	
		}

		void ParticleEffects::RemoveContinuousEffects()
		{
			continuous_particle_effects.clear();
		}

		void ParticleEffects::RemoveOtherEffects()
		{
			const auto event_finished_begin = std::remove_if( particle_event_effects.begin(), particle_event_effects.end(), [&]( EffectEventInstance& instance ) -> bool
			{
				return instance.effect->EffectComplete();
			});
			particle_event_effects.erase( event_finished_begin, particle_event_effects.end() );

			const auto infinite_event_finished_begin = std::remove_if(infinite_particle_event_effects.begin(), infinite_particle_event_effects.end(), [&](EffectEventInstance& instance) -> bool
			{
				return instance.effect->EffectComplete();
			});
			infinite_particle_event_effects.erase(infinite_event_finished_begin, infinite_particle_event_effects.end());

			const auto bone_group_effects_finished_begin = std::remove_if( bone_group_effects.begin(), bone_group_effects.end(), [&]( BoneGroupEffectInstance& instance ) -> bool
			{
				return instance.effect->EffectComplete();
			});
			bone_group_effects.erase( bone_group_effects_finished_begin, bone_group_effects.end() );

			const auto bone_effects_finished_begin = std::remove_if( bone_effects.begin(), bone_effects.end(), [&]( BoneEffectInstance& instance ) -> bool
			{
				return instance.effect->EffectComplete();
			});
			bone_effects.erase( bone_effects_finished_begin, bone_effects.end() );
		}

		void ParticleEffects::RenderFrameMove( const float elapsed_time )
		{
			RenderFrameMoveInternal(elapsed_time);
		}
		
		void ParticleEffects::RenderFrameMoveNoVirtual( AnimationObjectComponent* component, const float elapsed_time )
		{
			static_cast<ParticleEffects*>(component)->RenderFrameMoveInternal(elapsed_time);
		}

		void ParticleEffects::RenderFrameMoveInternal( const float elapsed_time )
		{
			PROFILE;

			auto* animated_render = GetObject().GetComponent< AnimatedRender >(static_ref.animated_render_index);
			
			if (animated_render->IsEffectsPaused())
				return;

			if( ( !static_ref.tick_when_not_visible && !animated_render->IsVisible() ) )
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

			const auto TickEffectPositions = [&](auto& effect, const auto& positions)
			{
				effect.SetPosition(positions);
				effect.SetMultiplier(multiplier);

			#if defined(PROFILING)
				Stats::Tick(Type::ParticleEffects);
			#endif
			};

			const auto TickEffect = [&](auto& effect, unsigned bone_group)
			{
				TickEffectPositions(effect, bone_groups->GetBonePositions(bone_group));
				if (high_res)
					effect.SetAnimationStorage(animation_storage_id, bone_groups->static_ref.GetBoneGroup(bone_group).bone_indices);
			};

			//Tick continuous emitters
			unsigned effect_index = 0;
			for( auto& particle_effect : continuous_particle_effects )
			{
				if( particle_effect.get() )
					TickEffect( *particle_effect, static_ref.continuous_particle_effects[ effect_index ].bone_group_index ); 
				++effect_index;
			}

			//Tick event emitters.
			for( auto& instance : particle_event_effects )
				TickEffect( *instance.effect, instance.bone_group );

			for (auto& instance : infinite_particle_event_effects)
				TickEffect(*instance.effect, instance.bone_group);

			//Tick manually added emitters attached to bone groups.
			for( auto& instance : bone_group_effects )
				TickEffect( *instance.effect, instance.attach_index );
			
			//Tick manually added emitters attached to bones.
			for( auto& instance : bone_effects )
			{
				//Work out position of bone.
				const auto* client_animation_controller = GetObject().GetComponent< ClientAnimationController >( static_ref.client_animation_controller_index );
				Memory::SmallVector<simd::vector3, 16, Memory::Tag::Particle> positions( 2 );
				positions[ 0 ] = client_animation_controller->GetBonePosition( instance.attach_index );
				positions[ 1 ] = (simd::vector3&)instance.at;
				if( instance.dynamic_scale != 1.0f )
					positions.emplace_back( instance.dynamic_scale, 0.0f, 0.0f );

				TickEffectPositions( *instance.effect, positions );
			}

			RemoveOtherEffects();
		}

		void ParticleEffects::OnSleep()
		{
			OnSleepInternal();
		}
		
		void ParticleEffects::OnSleepNoVirtual(AnimationObjectComponent* component)
		{
			static_cast<ParticleEffects*>(component)->OnSleepInternal();
		}

		void ParticleEffects::OnSleepInternal()
		{
			OrphanEffects();
		}

		void ParticleEffects::OnAnimationEvent(const ClientAnimationController& controller, const Event& e, float time_until_trigger)
		{
			PROFILE;
			if (e.event_type == ParticleEffectEventType)
			{
				const auto& effect_event = static_cast<const ParticleEffectsStatic::ParticleEffectEvent&>(e);

				bool allow_infinite = true;

				//Check to see if this event is already playing
				//But only if its infinite
				if (effect_event.effect->HasInfiniteEmitter())
				{
					for( auto& infinite_effect : infinite_particle_event_effects )
					{
						if( infinite_effect.particle_event == &effect_event )
						{
							infinite_effect.effect->StartEmitting();
							allow_infinite = false;
						}
					}
				}

				for( const unsigned bone_group_index : effect_event.bone_group_indices )
					PlayEffect( bone_group_index, effect_event, effect_event.effect, time_until_trigger, true, allow_infinite);
			}
		}

		void ParticleEffects::OnAnimationStart( const CurrentAnimation& animation, const bool blend)
		{
			const float global_speed_multiplier = GetObject().GetComponent< AnimationController >(static_ref.animation_controller_index)->GetGlobalSpeedMultiplier();
			particle_event_anim_speed = animation.speed_multiplier * global_speed_multiplier;

			std::for_each( particle_event_effects.begin(), particle_event_effects.end(), [&]( EffectEventInstance& event_instance )
			{
				event_instance.effect->StopEmitting();
			});

			std::for_each(infinite_particle_event_effects.begin(), infinite_particle_event_effects.end(), [&](EffectEventInstance& event_instance)
			{
				event_instance.effect->StopEmitting();
			});
		}

		void ParticleEffects::OnAnimationSpeedChange( const CurrentAnimation& animation )
		{
			const float global_speed_multiplier = GetObject().GetComponent< AnimationController >( static_ref.animation_controller_index )->GetGlobalSpeedMultiplier();
			particle_event_anim_speed = animation.speed_multiplier * global_speed_multiplier;

			std::for_each( particle_event_effects.begin(), particle_event_effects.end(), [&]( EffectEventInstance& event_instance )
			{
				event_instance.effect->SetAnimationSpeedMultiplier( particle_event_anim_speed );
			} );

			std::for_each(infinite_particle_event_effects.begin(), infinite_particle_event_effects.end(), [&](EffectEventInstance& event_instance)
			{
				event_instance.effect->SetAnimationSpeedMultiplier(particle_event_anim_speed);
			});
		}

		void ParticleEffects::OnAnimationPositionProgressed( const CurrentAnimation& animation, const float previous_time, const float new_time )
		{
			RenderFrameMove( new_time - previous_time );
		}

		void ParticleEffects::PlayEffect( const std::string& bone_group_name, const Particles::ParticleEffectHandle& effect, float delay)
		{
			PROFILE;

			const auto* bone_groups = GetObject().GetComponent< BoneGroups >( static_ref.bone_groups_index );
			const unsigned bone_group_index = bone_groups->static_ref.BoneGroupIndexByName( bone_group_name );
			assert( bone_group_index != -1 );
			if( bone_group_index == -1 )
				return;
			PlayEffect( bone_group_index, effect, delay );
		}

		std::shared_ptr< Particles::EffectInstance > ParticleEffects::PlayEffect( const unsigned bone_group_index, const Particles::ParticleEffectHandle& effect, float delay)
		{	
			PROFILE;

			if ((!Renderer::Scene::System::Get().GetDevice() || Renderer::Scene::System::Get().GetDevice()->RenderingPaused())) // Do not push new particles when rendering is paused to avoid accumulation.
				return std::shared_ptr<Particles::EffectInstance>();
			
			const auto* animated_render = GetObject().GetComponent< AnimatedRender >(static_ref.animated_render_index);
			const auto* bone_groups = GetObject().GetComponent< BoneGroups >( static_ref.bone_groups_index );
			assert( bone_groups->static_ref.NumBoneGroups() > bone_group_index );
			auto positions = bone_groups->GetBonePositions( bone_group_index );

			BoneGroupEffectInstance bone_group_effect_instance;
			bone_group_effect_instance.attach_index = bone_group_index;
			bone_group_effect_instance.effect = std::make_shared< Particles::EffectInstance >(effect, positions, animated_render->GetTransform(), nullptr, GetObject().IsHighPriority(), GetObject().UseNoDefaultShader(), delay, -1.0f);
			bone_group_effect_instance.effect->SetAnimationSpeedMultiplier(particle_event_anim_speed);
			bone_group_effects.push_back( std::move( bone_group_effect_instance ) );
			return bone_group_effects.back().effect;
		}

		std::shared_ptr< Particles::EffectInstance > ParticleEffects::PlayEffectOnBone( const unsigned bone_index, const Particles::ParticleEffectHandle& effect, float delay)
		{	
			PROFILE;

			assert( static_ref.client_animation_controller_index != -1 /* Tried to play an effect on a bone of a mesh without bones. */ );

			if ((!Renderer::Scene::System::Get().GetDevice() || Renderer::Scene::System::Get().GetDevice()->RenderingPaused())) // Do not push new particles when rendering is paused to avoid accumulation.
				return std::shared_ptr<Particles::EffectInstance>();

			const auto* animated_render = GetObject().GetComponent< AnimatedRender >(static_ref.animated_render_index);
			const auto* client_animation_controller = GetObject().GetComponent< ClientAnimationController >( static_ref.client_animation_controller_index );
			Memory::SmallVector<simd::vector3, 16, Memory::Tag::Particle> positions( 1 );
			positions[ 0 ] = client_animation_controller->GetBonePosition( bone_index );

			BoneEffectInstance bone_effect_instance;
			bone_effect_instance.attach_index = bone_index;
			bone_effect_instance.effect = std::make_shared< Particles::EffectInstance >(effect, positions, animated_render->GetTransform(), nullptr, GetObject().IsHighPriority(), GetObject().UseNoDefaultShader(), delay, -1.0f );
			bone_effect_instance.at = positions[ 0 ];
			bone_effect_instance.effect->SetAnimationSpeedMultiplier(particle_event_anim_speed);
			bone_effects.push_back( std::move( bone_effect_instance ) );
			return bone_effects.back().effect;
		}

		void ParticleEffects::PlayEffect( const unsigned bone_group_index, const ParticleEffectsStatic::ParticleEffectEvent& particle_event, const Particles::ParticleEffectHandle& effect, float delay, bool allow_finite, bool allow_infinite)
		{
			PROFILE;

			if ((!Renderer::Scene::System::Get().GetDevice() || Renderer::Scene::System::Get().GetDevice()->RenderingPaused())) // Do not push new particles when rendering is paused to avoid accumulation.
				return;

			const auto* animated_render = GetObject().GetComponent< AnimatedRender >(static_ref.animated_render_index);
			const auto* bone_groups = GetObject().GetComponent< BoneGroups >( static_ref.bone_groups_index );
			if( bone_groups->static_ref.NumBoneGroups() <= bone_group_index )
				return;
			auto positions = bone_groups->GetBonePositions( bone_group_index );

			if (allow_finite && effect->HasFiniteEmitter())
			{
				EffectEventInstance effect_event_instance;
				effect_event_instance.particle_event = &particle_event;
				effect_event_instance.bone_group = bone_group_index;
				effect_event_instance.effect = std::make_shared< Particles::EffectInstance >(effect, positions, animated_render->GetTransform(), nullptr, GetObject().IsHighPriority(), GetObject().UseNoDefaultShader(), delay, particle_event.length > 0.0f ? particle_event.length : -1.0f, false, true);
				effect_event_instance.effect->SetAnimationSpeedMultiplier(particle_event_anim_speed);
				particle_event_effects.push_back(std::move(effect_event_instance));
			}

			if (allow_infinite && effect->HasInfiniteEmitter())
			{
				EffectEventInstance effect_event_instance;
				effect_event_instance.particle_event = &particle_event;
				effect_event_instance.bone_group = bone_group_index;
				effect_event_instance.effect = std::make_shared< Particles::EffectInstance >(effect, positions, animated_render->GetTransform(), nullptr, GetObject().IsHighPriority(), GetObject().UseNoDefaultShader(), delay, particle_event.length > 0.0f ? particle_event.length : -1.0f, true, false);
				effect_event_instance.effect->SetAnimationSpeedMultiplier(particle_event_anim_speed);
				infinite_particle_event_effects.push_back(std::move(effect_event_instance));
			}
		}

		void ParticleEffects::PlayEffectInDirection( const unsigned bone_index, const Particles::ParticleEffectHandle& effect, const simd::vector3& direction_world, const float dynamic_scale/* = 1.0f*/, float delay /*= 0.0f*/)
		{
			PROFILE;

			assert( static_ref.client_animation_controller_index != -1 /* Tried to play an effect on a bone of a mesh without bones. */ );

			if ((!Renderer::Scene::System::Get().GetDevice() || Renderer::Scene::System::Get().GetDevice()->RenderingPaused())) // Do not push new particles when rendering is paused to avoid accumulation.
				return;

			const auto* animated_render = GetObject().GetComponent< AnimatedRender >( static_ref.animated_render_index );
			const auto* client_animation_controller = GetObject().GetComponent< ClientAnimationController >( static_ref.client_animation_controller_index );

			BoneEffectInstance bone_effect_instance;
			bone_effect_instance.attach_index = bone_index;

			//Put the at position into model space.
			simd::vector3 transformed_dir;
			auto world_to_model = animated_render->GetTransform().inverse();
			transformed_dir = world_to_model.muldir(direction_world );
			
			Memory::SmallVector<simd::vector3, 16, Memory::Tag::Particle> positions( 2 );
			positions[ 0 ] = client_animation_controller->GetBonePosition( bone_index );
			positions[ 1 ] = positions[ 0 ] + transformed_dir;
			if( dynamic_scale != 1.0f )
				positions.emplace_back( dynamic_scale, 0.0f, 0.0f );
			bone_effect_instance.at = positions[ 1 ];
			bone_effect_instance.dynamic_scale = dynamic_scale;
			bone_effect_instance.effect = std::make_shared< Particles::EffectInstance >( effect, positions, animated_render->GetTransform(), nullptr, GetObject().IsHighPriority(), GetObject().UseNoDefaultShader(), delay, -1.0f );
			bone_effect_instance.effect->SetAnimationSpeedMultiplier(particle_event_anim_speed);
			bone_effects.push_back( std::move( bone_effect_instance ) );
		}

		void ParticleEffectsStatic::OnTypeInitialised()
		{
			// BoneGroupIndexByName calls must be delayed until OnTypeInitialised() because 
			// BoneGroupsStatic adds mesh-defined bone groups in OnTypeInitialised().
			if (parsed_effects)
			{
				const auto* bone_groups = parent_type.GetStaticComponent< BoneGroupsStatic >(bone_groups_index);

				for (auto& effect : parsed_effects->continuous)
				{
					effect.second.bone_group_index = bone_groups->BoneGroupIndexByName(effect.first);
					if (effect.second.bone_group_index == -1)
						throw Resource::Exception() << L"No bone group with name " << effect.first.c_str();

					continuous_particle_effects.push_back(std::move(effect.second));
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

				for (auto& event : particle_effect_events)
					if (event->copy_of && event->bone_group_indices.empty())
						event->bone_group_indices = static_cast<const ParticleEffectEvent*>(event->copy_of)->bone_group_indices;

				parsed_effects = nullptr;
			}
		}
	}
}
