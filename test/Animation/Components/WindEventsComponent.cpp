#include "WindEventsComponent.h"

#include <iomanip>
#include <tuple>

#include "ClientInstanceCommon/Game/Components/Positioned.h"
#include "ClientInstanceCommon/Animation/AnimatedObject.h"
#include "ClientInstanceCommon/Animation/Components/AnimationControllerComponent.h"
#include "Visual/Animation/Components/ComponentsStatistics.h"

#include "BoneGroupsComponent.h"
#include "AnimatedRenderComponent.h"
#include "ClientAnimationControllerComponent.h"

namespace Animation
{
	namespace Components
	{
		WindEventsStatic::WindEventsStatic( const Objects::Type& parent )
			: AnimationObjectStaticComponent( parent )
			, animation_controller_index( parent.GetStaticComponentIndex< AnimationControllerStatic >() )
			, animated_render_index( parent.GetStaticComponentIndex< AnimatedRenderStatic >() )
			, client_animation_controller_index( parent.GetStaticComponentIndex< ClientAnimationControllerStatic >() )
			, bone_groups_index ( parent.GetStaticComponentIndex< BoneGroupsStatic >() )
			, current_animation( -1 )
			, current_event(nullptr)
		{
		}

		COMPONENT_POOL_IMPL(WindEventsStatic, WindEvents, 64)
		
		const char* WindEventsStatic::GetComponentName( )
		{
			return "WindEvents";
		}

		WindEventsStatic::WindEventDescriptor::WindEventDescriptor( ) : 
			Event( WindEventType, 0 ),
			bone_index(-1),
			bone_group_index(-1),
			size(Physics::Vector3f(300.0f, 200.0f, 300.0f)),
			duration(1.0f),
			initial_phase(0.0f),
			local_coords(Physics::Coords3f::defCoords()),
			wind_shape_type(Physics::WindSourceTypes::Explosion),
			is_attached(true),
			is_persistent(false),
			is_continuous(false),
			scales_with_object(false)
		{ 
		}

		bool WindEventsStatic::SetValue(const std::string &name, const std::wstring &value)
		{
			if( name == "continuous_effect")
			{
				auto newbie = std::unique_ptr<WindEventDescriptor>(new WindEventDescriptor);
				newbie->animation_index = -1;
				newbie->time = -1.0f;
				newbie->is_continuous = true;
				current_event = AddEventDesc(std::move(newbie));
				return true;
			}else
			if( name == "animation" )
			{
				auto* animation_controller = parent_type.GetStaticComponent< AnimationControllerStatic >( animation_controller_index );
				const unsigned animation_index = animation_controller->GetAnimationIndexByName( WstringToString( value ) );
				if( animation_index == -1 )
					throw Resource::Exception( ) << value << L" animation not found when setting particle effects.";
				
				current_animation = animation_index;
				return true;
			}else
			if( name == "shape" )
			{
				assert(current_event);

				if(value == L"Explosion")
				{
					current_event->wind_shape_type = Physics::WindSourceTypes::Explosion;
				}else
				if(value == L"Vortex")
				{
					current_event->wind_shape_type = Physics::WindSourceTypes::Vortex;
				}else
				if(value == L"Wake")
				{
					current_event->wind_shape_type = Physics::WindSourceTypes::Wake;
				}else
				if(value == L"Directional")
				{
					current_event->wind_shape_type = Physics::WindSourceTypes::Directional;
				}else
				if(value == L"Turbulence")
				{
					current_event->wind_shape_type = Physics::WindSourceTypes::Turbulence;
				}else
				if(value == L"FireSource")
				{
					current_event->wind_shape_type = Physics::WindSourceTypes::FireSource;
				}else
				{
					return false;
				}
				return true;
			}else
			if(name == "bone_name")
			{
				assert(current_event);
				auto animation_controller = parent_type.GetStaticComponent< ClientAnimationControllerStatic >();
				if(animation_controller)
				{
					int bone_index = animation_controller->skeleton->GetBoneIndexByName(WstringToString( value ));
					current_event->bone_index = bone_index;
				}
				return true;
			}else
			if(name == "bone_group_name")
			{
				assert(current_event);
				auto bone_groups = parent_type.GetStaticComponent< BoneGroupsStatic >();
				if(bone_groups)
				{
					int bone_group_index = bone_groups->BoneGroupIndexByName(WstringToString( value ));
					current_event->bone_group_index = bone_group_index;
				}
				return true;
			}else
			if( name == "size" )
			{
				assert(current_event);
				Physics::Vector3f size = Physics::Vector3f(1.0f, 1.0f, 1.0f);
				std::wstringstream vec_stream(value);
				vec_stream >> size.x >> size.y >> size.z;
				current_event->size = size;
				return true;
			}
			
			return false;
		}
		bool WindEventsStatic::SetValue(const std::string &name, const float value)
		{
			if(name == "time")
			{
				assert(current_animation != -1);
				auto newbie = std::unique_ptr<WindEventDescriptor>(new WindEventDescriptor);
				newbie->animation_index = current_animation;
				newbie->time = value;
				current_event = AddEventDesc(std::move(newbie));

				return true;
			}else
			if( name == "duration" )
			{
				assert(current_event);
				current_event->duration = value;
				return true;
			}else
			if( name == "initial_phase" )
			{
				assert(current_event);
				current_event->initial_phase = value;
				return true;
			}else
			if( name == "primary_velocity" )
			{
				assert(current_event);
				current_event->primary_field.velocity = value;
				return true;
			}else
			if( name == "secondary_velocity" )
			{
				assert(current_event);
				current_event->secondary_field.velocity = value;
				return true;
			}else
			if( name == "primary_wavelength" )
			{
				assert(current_event);
				current_event->primary_field.wavelength = value;
				return true;
			}else
			if( name == "secondary_wavelength" )
			{
				assert(current_event);
				current_event->secondary_field.wavelength = value;
				return true;
			}else
			{
				return false;
			}
			return false;
		}
		bool WindEventsStatic::SetValue(const std::string &name, const int value)
		{
			if(name == "bone_index")
			{
				assert(current_event);
				current_event->bone_index = value;
				return true;
			}else
			if(name == "bone_group_index")
			{
				assert(current_event);
				current_event->bone_group_index = value;
				return true;
			}else
			{
				return false;
			}
			return false;
		}
		bool WindEventsStatic::SetValue(const std::string &name, const bool value)
		{
			if(name == "is_attached")
			{
				assert(current_event);
				current_event->is_attached = value;
				return true;
			} else
			if(name == "is_persistent")
			{
				assert(current_event);
				current_event->is_persistent = value;
				return true;
			} else
			if(name == "scales_with_object")
			{
				assert(current_event);
				current_event->scales_with_object = value;
				return true;
			}else
			{
				return false;
			}
			return false;
		}

		void WindEventsStatic::SaveComponent( std::wostream& stream ) const
		{
#ifndef PRODUCTION_CONFIGURATION
			const auto* parent = parent_type.parent_type->GetStaticComponent< WindEventsStatic >();
			assert( parent );

			stream << std::fixed;
			stream << std::setprecision(3);

			const auto default_event = WindEventDescriptor();

			int prev_animation_index = -1;
			for(size_t event_index = 0; event_index < wind_event_descriptors.size(); event_index++)
			{
				auto& curr_event = wind_event_descriptors[event_index];
				if( curr_event->copy_of )
					continue;
				if( curr_event->origin != parent_type.current_origin )
					continue;

				int curr_animation_index = curr_event->animation_index;
				unsigned num_tabs = 0;

				if(!wind_event_descriptors[event_index]->is_continuous)
				{
					if(curr_animation_index != prev_animation_index)
					{

						assert( animation_controller_index != -1 );
						auto* animation_controller = parent_type.GetStaticComponent< AnimationControllerStatic >( animation_controller_index );
						std::string animation_name = animation_controller->GetMetadataByIndex( curr_animation_index ).name;
						WriteValueNamed( L"animation", StringToWstring( animation_name ) );
					}

					++num_tabs;
					PushIndents( num_tabs );
					WriteValueNamed( L"time", curr_event->time );
					++num_tabs;
				}
				else
				{
					WriteValueNamed( L"continuous_effect", "\"this effect runs permanently\"" );
					++num_tabs;
				}

				PushIndents( num_tabs );

				prev_animation_index = curr_animation_index;
				{
					switch(curr_event->wind_shape_type)
					{
					case Physics::WindSourceTypes::Explosion:	WriteValueNamed( L"shape", "\"Explosion\"" ); break;
					case Physics::WindSourceTypes::Vortex:		WriteValueNamed( L"shape", "\"Vortex\"" ); break;
					case Physics::WindSourceTypes::Wake:		WriteValueNamed( L"shape", "\"Wake\"" ); break;
					case Physics::WindSourceTypes::Directional:	WriteValueNamed(L"shape", "\"Directional\""); break;
					case Physics::WindSourceTypes::Turbulence:	WriteValueNamed(L"shape", "\"Turbulence\""); break;
					case Physics::WindSourceTypes::FireSource:	WriteValueNamed( L"shape", "\"FireSource\"" ); break;
					}

					WriteCustomConditional( stream << L"size = \"" << curr_event->size.x << " " << curr_event->size.y << " " << curr_event->size.z << L"\"", ( curr_event->size - default_event.size ).Len() > 1e-4f );
					WriteValueNamedConditional( L"duration", curr_event->duration, curr_event->duration != default_event.duration );
					WriteValueNamedConditional( L"initial_phase", curr_event->initial_phase, curr_event->initial_phase != default_event.initial_phase );
					WriteValueNamedConditional( L"primary_velocity", curr_event->primary_field.velocity, curr_event->primary_field.velocity != default_event.primary_field.velocity );
					WriteValueNamedConditional( L"primary_wavelength", curr_event->primary_field.wavelength, curr_event->primary_field.wavelength != default_event.primary_field.wavelength );
					WriteValueNamedConditional( L"secondary_velocity", curr_event->secondary_field.velocity, curr_event->secondary_field.velocity != default_event.secondary_field.velocity );
					WriteValueNamedConditional( L"secondary_wavelength", curr_event->secondary_field.wavelength, curr_event->secondary_field.wavelength != default_event.secondary_field.wavelength );
					WriteValueNamedConditional( L"is_attached", curr_event->is_attached, curr_event->is_attached != default_event.is_attached );
					WriteValueNamedConditional( L"is_persistent", curr_event->is_persistent, curr_event->is_persistent != default_event.is_persistent );
					WriteValueNamedConditional( L"scales_with_object", curr_event->scales_with_object, curr_event->scales_with_object != default_event.scales_with_object );

					WriteValueNamedConditional( L"bone_group_index", curr_event->bone_group_index, curr_event->bone_group_index != -1 );
					WriteValueNamedConditional( L"bone_index", curr_event->bone_index, curr_event->bone_group_index == -1 && curr_event->bone_index != -1 );
				}
			}
#endif
		}

		WindEventsStatic::WindEventDescriptor *WindEventsStatic::AddEventDesc( std::unique_ptr< WindEventsStatic::WindEventDescriptor > wind_event_desc )
		{
#ifndef PRODUCTION_CONFIGURATION
			wind_event_desc->origin = parent_type.current_origin;
#endif
			wind_event_descriptors.emplace_back( std::move( wind_event_desc ) );
			if(!wind_event_descriptors.back()->is_continuous)
			{
				int animation_index = wind_event_descriptors.back()->animation_index;
				assert( animation_controller_index != -1 );
				auto* animation_controller = parent_type.GetStaticComponent< AnimationControllerStatic >( animation_controller_index );
				animation_controller->AddEvent( animation_index, *wind_event_descriptors.back() );
				animation_controller->SortEvents();
			}
			return wind_event_descriptors.back().get();
		}


		void WindEventsStatic::RemoveEventDesc( const WindEventsStatic::WindEventDescriptor *wind_event_desc )
		{
			auto* animation_controller = parent_type.GetStaticComponent< AnimationControllerStatic >();
			animation_controller->RemoveEvent( *wind_event_desc ); //removes by taking a pointer to *wind_event_desc

			const auto found = std::find_if( 
				wind_event_descriptors.begin(), 
				wind_event_descriptors.end(), 
				[&]( const std::unique_ptr< WindEventsStatic::WindEventDescriptor >& iter ) { return iter.get() == wind_event_desc; } );
			wind_event_descriptors.erase( found );
		}

		WindEvents::WindEvents( const WindEventsStatic& static_ref, Objects::Object& object )
			: AnimationObjectComponent( object ), static_ref( static_ref ), wind_system(nullptr)
		{
			if( static_ref.animation_controller_index != -1 )
			{
				auto *animation_controller = GetObject().GetComponent< AnimationController >( static_ref.animation_controller_index );
				animation_controller->AddListener( *this );
			}
			for(size_t event_index = 0; event_index < static_ref.wind_event_descriptors.size(); event_index++)
			{
				auto &event_descriptor = static_ref.wind_event_descriptors[event_index];
				if(event_descriptor->is_continuous)
				{
					wind_event_queue.push_back(event_descriptor.get()); //will be added when wind system is initialized
				}
			}
		}

		WindEvents::~WindEvents()
		{
			auto* animated_render = GetObject().GetComponent< AnimatedRender >( static_ref.animated_render_index );

			for ( size_t wind_index = 0; wind_index < wind_event_instances.size(); ++wind_index )
			{
				auto *wind_source = wind_event_instances[wind_index].wind_source.Get();
				if(wind_source && (wind_event_instances[wind_index].wind_event_desc->is_persistent || wind_event_instances[wind_index].wind_event_desc->is_continuous)) //remove all infinite animations
				{
					assert(wind_event_instances[wind_index].wind_event_desc->duration >= 0.0f);
					wind_source->SetDuration(wind_event_instances[wind_index].wind_event_desc->duration);
					wind_source->ResetElapsedTime();
				}
				assert( wind_source->GetDuration() >= 0.0f);
				if(wind_source && wind_source->GetDuration() > 0.0f)
				{
					wind_event_instances[wind_index].wind_source.Detach();
				}// else it will be removed
			}
		}

		void WindEvents::SetWindSystem(Physics::WindSystem * wind_system)
		{
			this->wind_system = wind_system;
		}

		void WindEvents::ClearActiveWindEventInstances( )
		{
			wind_event_instances.clear();
		}

		void WindEvents::FrameMove(const float elapsed_time)
		{
			PROFILE;

			if (wind_system)
			{
				for (size_t event_index = 0; event_index < wind_event_queue.size(); event_index++)
				{
					ExecuteEvent(*wind_event_queue[event_index]);
				}
				wind_event_queue.clear();
			}
		}

		void WindEvents::RenderFrameMove( const float elapsed_time )
		{
			RenderFrameMoveInternal(elapsed_time);
		}

		void WindEvents::RenderFrameMoveNoVirtual( AnimationObjectComponent* component, const float elapsed_time )
		{
			static_cast<WindEvents*>(component)->RenderFrameMoveInternal(elapsed_time);
		}

		void WindEvents::RenderFrameMoveInternal( const float elapsed_time )
		{
			PROFILE;

		#if defined(PROFILING)
			Stats::Tick(Type::WindEvents);
		#endif

			auto* animated_render = GetObject().GetComponent< AnimatedRender >( static_ref.animated_render_index );

			Physics::Coords3f anim_world_coords = Physics::MakeCoords3f(animated_render->GetTransform());
			for(size_t wind_index = 0; wind_index < wind_event_instances.size();)
			{
				auto* wind_source = wind_event_instances[wind_index].wind_source.Get();
				if(wind_source && !wind_source->IsPlaying()) //removing animations that finished playing from the list
				{
					wind_event_instances[wind_index] = std::move(wind_event_instances.back());
					wind_event_instances.pop_back();
				}
				else
				{
					if(wind_event_instances[wind_index].wind_event_desc->is_attached)
					{
						Physics::Vector3f size;
						Physics::Coords3f coords;
						GetSourceSizeAndCoords(*(wind_event_instances[wind_index].wind_event_desc), size, coords);

						if (wind_source)
							wind_source->SetCoords(coords);
					}
					wind_index++;
				}
			}
		}

		void WindEvents::ExecuteEvent( const WindEventsStatic::WindEventDescriptor& event_descriptor )
		{
			assert(event_descriptor.duration >= 0.0f);
			if( event_descriptor.is_persistent)
			{
				int found_index = -1;
				for(size_t event_index = 0; event_index < wind_event_instances.size(); event_index++)
				{
					if(wind_event_instances[event_index].wind_event_desc == &event_descriptor)
					{
						found_index = int(event_index);
					}
				}
				if(found_index != -1)
				{
					auto *wind_source = wind_event_instances[found_index].wind_source.Get();
					if(wind_source)
						wind_source->SetDuration(-1.0f); //in case it was fading off already
					return;
				}
			}

			this->AddWindEventInstance(event_descriptor);
		}

		void WindEvents::OnAnimationEvent( const AnimationController& controller, const Event& e )
		{
			PROFILE;
			if( e.event_type != WindEventType )
				return;

			const auto& event_descriptor = static_cast< const WindEventsStatic::WindEventDescriptor& >( e );
			if(!!wind_system)
			{
				ExecuteEvent(event_descriptor);
			}else
			{
				wind_event_queue.push_back(&event_descriptor);
			}
		}

		void WindEvents::GetSourceGeom(const WindEventsStatic::WindEventDescriptor &event_desc, Physics::Vector3f &size, Physics::Coords3f &coords, Physics::Vector3f &dir)
		{
			auto* animated_render = GetObject().GetComponent< AnimatedRender >( static_ref.animated_render_index );
			assert(animated_render);
			Physics::Coords3f anim_world_coords = Physics::MakeCoords3f(animated_render->GetTransform());

			size = event_desc.size;
			coords = anim_world_coords;
			dir = Physics::Vector3f(0.0f, 0.0f, 1.0f);

			if(event_desc.bone_group_index != -1)
			{
				auto bone_groups = GetObject().GetComponent< BoneGroups >( static_ref.bone_groups_index );
				auto bone_positions = bone_groups->GetBonePositions( event_desc.bone_group_index );
				
				if( bone_positions.size() < 3 )
					return;

				Physics::Vector3f box_size = Physics::MakeVector3f(bone_positions[ 1 ]) - Physics::MakeVector3f(bone_positions[ 0 ]);
				Physics::Vector3f direction = (Physics::MakeVector3f(bone_positions[ 2 ]) - Physics::MakeVector3f(bone_positions[ 1 ])).GetNorm();

				box_size.x = std::abs(box_size.x);
				box_size.y = std::abs(box_size.y);
				box_size.z = std::abs(box_size.z);


				Physics::Coords3f local_coords = Physics::Coords3f::defCoords();
				local_coords.pos = (Physics::MakeVector3f(bone_positions[ 1 ]) + Physics::MakeVector3f(bone_positions[ 0 ])) * 0.5f;

				dir = direction;
				coords = anim_world_coords.GetWorldCoords(local_coords);
				size = box_size * 0.5f;
			}else
			if(event_desc.bone_index != -1)
			{
				const auto* client_animation_controller = GetObject().GetComponent< ClientAnimationController >( static_ref.client_animation_controller_index );
				assert(client_animation_controller);

				int bone_index = event_desc.bone_index;
				Physics::Coords3f animated_bone_coords = Physics::MakeCoords3f(client_animation_controller->GetBoneTransform(bone_index));
				Physics::Coords3f local_coords = event_desc.local_coords;
				coords = anim_world_coords.GetWorldCoords(animated_bone_coords).GetWorldCoords(local_coords);
				size = event_desc.size;
			}
		}



		void WindEvents::GetSourceSizeAndCoords(const WindEventsStatic::WindEventDescriptor &event_desc, Physics::Vector3f &size, Physics::Coords3f &coords)
		{
			Physics::Vector3f dir;
			GetSourceGeom(event_desc, size, coords, dir);
		}

		void WindEvents::AddWindEventInstance(const WindEventsStatic::WindEventDescriptor &event_desc)
		{
			if(!wind_system)
				return;
			assert(event_desc.duration >= 0.0f);

			Physics::Vector3f size;
			Physics::Coords3f coords;
			Physics::Vector3f dir;
			GetSourceGeom(event_desc, size, coords, dir);

			if (event_desc.wind_shape_type == Physics::WindSourceTypes::FireSource)
			{
				if( event_desc.scales_with_object )
				{
					const auto* game_object = GEAL::GetValue< Game::GameObject >( GetObject().GetVariable( GEAL::GameObjectId ) );
					const auto* render = GetObject().GetComponent< Animation::Components::AnimatedRender >();
					const auto scaled_size = size.x
						* ( render ? render->GetScale().x : 1.0f )
						* ( game_object ? game_object->GetPositioned().GetScale() * game_object->GetPositioned().GetXScale() : 1.0f );
					wind_system->AddFireSource(coords.pos, scaled_size, event_desc.initial_phase);
				}
				else
					wind_system->AddFireSource(coords.pos, size.x, event_desc.initial_phase);
			}
			else
			{
				this->wind_event_instances.push_back(WindEventInstance());
				wind_event_instances.back().wind_event_desc = &event_desc; //getting pointer from here which isnt terribly safe but particle events code uses it

				wind_event_instances.back().wind_source = wind_system->AddWindSourceByType(
					coords,
					size,
					(event_desc.is_persistent || event_desc.is_continuous) ? -1.0f : event_desc.duration,
					event_desc.initial_phase,
					event_desc.primary_field.velocity,
					event_desc.primary_field.wavelength,
					event_desc.secondary_field.velocity,
					event_desc.secondary_field.wavelength,
					dir,
					event_desc.wind_shape_type);
			}
			PROFILE;
		}

		void WindEvents::OnAnimationStart( const CurrentAnimation& animation, const bool blend)
		{
			for(size_t wind_index = 0; wind_index < wind_event_instances.size();)
			{
				//if(wind_event_instances[wind_index].wind_event_desc->duration < 0.0f) //remove all infinite animations
				auto *wind_source = wind_event_instances[wind_index].wind_source.Get();

				if(!wind_event_instances[wind_index].wind_event_desc->is_continuous && 
					wind_source && wind_source->GetDuration() < 0.0f && 
					wind_event_instances[wind_index].wind_event_desc->is_persistent) //play fade-out time for persistent animations
				{
					wind_source->SetDuration(wind_event_instances[wind_index].wind_event_desc->duration);
					wind_source->ResetElapsedTime();
				}else
				{
					wind_index++;
				}
			}
		}
	}
}
