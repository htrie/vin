#include "ClientInstanceCommon/Animation/AnimatedObject.h"
#include "ClientInstanceCommon/Animation/Components/AnimationControllerComponent.h"
#include "Visual/Animation/Components/AnimatedRenderComponent.h"
#include "Visual/Renderer/DrawCalls/EffectGraphNode.h"
#include "TimelineParametersComponent.h"
#include "magic_enum/magic_enum.hpp"

namespace Animation
{
	namespace Components
	{
		namespace
		{
			struct ParameterInfo
			{
				unsigned data_id = 0;
				float value = 0;
			};
			typedef Memory::Vector<ParameterInfo, Memory::Tag::Effects> ParameterInfos;
			typedef std::pair<ParameterInfos, ParameterInfos> ParameterInfosPair;

			void UpdateParameters(AnimatedObject& object, const ParameterInfosPair& parameters, bool& prev_parent_active, bool& prev_child_active)
			{
				// Note: We want to reset the parameters back to 0 when they are suddenly not active thus the "prev_" parameters

				if (!parameters.first.empty() || prev_parent_active)
				{			
					if (auto* animated_render = object.GetComponent< Animation::Components::AnimatedRender >())
					{
						animated_render->ProcessCustomDynamicParams([&](auto& param_info)
						{
							float value = 0.f;
							for (const auto& parameter : parameters.first)
							{
								if (parameter.data_id == param_info.id)
									value = parameter.value;
							}
							param_info.param.Set(value);
						});
					}
				}

				if (!parameters.second.empty() || prev_child_active)
				{			
					if (const auto* attached_object_component = object.GetComponent< AttachedAnimatedObject >())
					{
						std::for_each(attached_object_component->AttachedObjectsBegin(), attached_object_component->AttachedObjectsEnd(), [&](const AttachedAnimatedObject::AttachedObject& object)
						{
							UpdateParameters(*object.object, std::make_pair(parameters.second, parameters.second), prev_child_active, prev_child_active);
						});
					}
				}
			}

			ParameterInfosPair GetTimelineParameters(AnimatedObject& object, const unsigned animation_controller_index, std::vector<const TimelineParametersStatic::TimelineParameterEvent*>& active_events)
			{
				ParameterInfosPair result;

				const auto* animation_controller = object.GetComponent< AnimationController >(animation_controller_index);
				const unsigned current_animation = animation_controller->GetCurrentAnimation();
				const float time = animation_controller->GetCurrentAnimationPosition();

				RemoveIf(active_events, [&](const TimelineParametersStatic::TimelineParameterEvent* ev)
					{
						return current_animation != ev->animation_index || time < ev->track.GetStartTime() || time > ev->track.GetEndTime();
					});

				for (const auto* event : active_events)
				{
					if (event->apply_on_children)
						result.second.push_back({ event->data_id, event->track.Interpolate(time) });
					result.first.push_back({ event->data_id, event->track.Interpolate(time) });
				}

				return result;
			}
		}

		TimelineParametersStatic::TimelineParametersStatic( const Objects::Type& parent )
			: AnimationObjectStaticComponent( parent )
			, animation_controller_index( parent.GetStaticComponentIndex< AnimationControllerStatic >() )
			, current_animation( -1 )
		{
		}

		void TimelineParametersStatic::CopyAnimationData( const int source_animation, const int destination_animation, std::function<bool( const Animation::Event& )> predicate, std::function<void( const Animation::Event&, Animation::Event& )> transform )
		{
			std::vector<TimelineParameterEvent> copied;
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
				events.emplace_back( std::make_unique<TimelineParameterEvent>( std::move( e ) ) );
				animation_controller->AddEvent( destination_animation, *events.back() );
			}
		}

		COMPONENT_POOL_IMPL(TimelineParametersStatic, TimelineParameters, 64)

		bool TimelineParametersStatic::SetValue( const std::string& name, const std::wstring& value )
		{
			if ( name == "animation" )
			{
				assert( animation_controller_index != -1 );
				auto* animation_controller = parent_type.GetStaticComponent< AnimationControllerStatic >( animation_controller_index );
				const unsigned animation_index = animation_controller->GetAnimationIndexByName( WstringToString( value ) );
				if ( animation_index == -1 )
					throw Resource::Exception() << value << L" animation not found when setting timeline parameter event.";

				current_animation = animation_index;
				return true;
			}
			else if( name == "event" )
			{
				std::unique_ptr< TimelineParameterEvent > event( new TimelineParameterEvent );

				assert( animation_controller_index != -1 );
				if ( current_animation == -1 )
					throw Resource::Exception() << L"No current animation set";

				event->animation_index = current_animation;

				std::wstring name;
				unsigned num_points = 0;
				std::wstringstream stream( value );
				stream >> name >> num_points;
				event->name = WstringToString( name );
				event->data_id = Device::HashStringExpr(Renderer::DrawCalls::EffectGraphUtil::AddDynamicParameterPrefix(event->name).c_str());
				if( num_points < 2 || num_points > event->curve.NumControlPoints )
					throw Resource::Exception() << L"Unable to parse timeline parameter event";

				for( unsigned i = 0; i < num_points; ++i )
				{
					simd::CurveControlPoint p;
					std::wstring type;
					stream >> p.time >> p.value >> type;

					auto mode = magic_enum::enum_cast< simd::CurveControlPoint::Interpolation >( WstringToString( type ) );
					if( stream.fail() || !mode )
						throw Resource::Exception() << "Unable to parse timeline parameter event";
					p.type = *mode;

					if( p.type == simd::CurveControlPoint::Interpolation::Custom )
						stream >> p.left_dt >> p.right_dt;
					else if( p.type == simd::CurveControlPoint::Interpolation::CustomSmooth )
						stream >> p.left_dt;
					event->curve.points.push_back( std::move( p ) );
				}
				if( stream.fail() )
					throw Resource::Exception() << "Unable to parse timeline parameter event";

				event->time = event->curve.GetStartTime();
				event->track = event->curve.MakeTrack( 0.0f );

				if (!stream.eof())
					stream >> event->apply_on_children;

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

		const char* TimelineParametersStatic::GetComponentName()
		{
			return "TimelineParameters";
		}

		void TimelineParametersStatic::SaveComponent( std::wostream& stream ) const
		{
#ifndef PRODUCTION_CONFIGURATION
			const auto* parent = parent_type.parent_type->GetStaticComponent< TimelineParametersStatic >();
			assert( parent );

			const auto* animation_controller = parent_type.GetStaticComponent< AnimationControllerStatic >( animation_controller_index );
			unsigned current_animation = -1;

			for ( const auto& event : events )
			{
				if( event->copy_of )
					continue;
				if( event->name.empty() )
					continue;
				if( event->origin != parent_type.current_origin )
					continue;

				if ( current_animation != event->animation_index )
				{
					current_animation = event->animation_index;
					WriteValueNamed( L"animation", animation_controller->GetMetadataByIndex( current_animation ).name.c_str() );
				}

				PushIndent();
				std::wstringstream ss;
				ss << StringToWstring( event->name ) << L" " << event->curve.points.size();
				for( const auto& p : event->curve.points )
				{
					ss << L" " << p.time << L" " << p.value << L" " << StringToWstring( std::string( magic_enum::enum_name( p.type ) ) );
					if( p.type == simd::CurveControlPoint::Interpolation::Custom )
						ss << L" " << p.left_dt << L" " << p.right_dt;
					else if( p.type == simd::CurveControlPoint::Interpolation::CustomSmooth )
						ss << L" " << p.left_dt;
				}
				ss << L" " << event->apply_on_children;
				WriteValueNamed( L"event", ss.str() );
			}
#endif
		}

		void TimelineParametersStatic::AddTimelineParameterEvent( std::unique_ptr< TimelineParameterEvent > e )
		{
			const unsigned animation_index = e->animation_index;
			events.emplace_back( std::move( e ) );

			//Add the event
			auto* animation_controller = parent_type.GetStaticComponent< AnimationControllerStatic >( animation_controller_index );
			animation_controller->AddEvent( animation_index, *events.back() );

			//Needs sorting now
			animation_controller->SortEvents();
		}

		void TimelineParametersStatic::RemoveTimelineParameterEvent( const TimelineParameterEvent& e )
		{
			auto* animation_controller = parent_type.GetStaticComponent< AnimationControllerStatic >();
			animation_controller->RemoveEvent( e );

			const auto found = std::find_if( events.begin(), events.end(), [ & ]( const std::unique_ptr< TimelineParameterEvent >& iter ) { return iter.get() == &e; } );
			events.erase( found );
		}

		TimelineParameters::TimelineParameters( const TimelineParametersStatic& static_ref, Objects::Object& object )
			: AnimationObjectComponent( object ), static_ref( static_ref )
		{
			auto *animation_controller = GetObject().GetComponent< AnimationController >( static_ref.animation_controller_index );
			anim_cont_list_ptr = animation_controller->GetListener( *this );
		}

		TimelineParameters::~TimelineParameters()
		{
		}

		void TimelineParameters::RenderFrameMove(const float elapsed_time)
		{
			RenderFrameMoveInternal(elapsed_time);
		}

		void TimelineParameters::RenderFrameMoveNoVirtual(AnimationObjectComponent* component, const float elapsed_time)
		{
			static_cast<TimelineParameters*>(component)->RenderFrameMoveInternal(elapsed_time);
		}

		void TimelineParameters::RenderFrameMoveInternal(const float elapsed_time)
		{
			const auto& params = GetTimelineParameters(GetObject(), static_ref.animation_controller_index, active_events);
			UpdateParameters(GetObject(), params, prev_parent_active, prev_child_active);
			prev_parent_active = !params.first.empty();
			prev_child_active = !params.second.empty();
		}

		void TimelineParameters::RemoveEvent( const TimelineParametersStatic::TimelineParameterEvent& e )
		{
			RemoveFirst( active_events, &e );
		}

		void TimelineParameters::OnAnimationEvent( const AnimationController& controller, const Event& e )
		{
			if ( e.event_type != TimelineParameterEventType )
				return;

			const auto& event = static_cast< const TimelineParametersStatic::TimelineParameterEvent& >( e );
			RemoveIf( active_events, [&]( const TimelineParametersStatic::TimelineParameterEvent* ev )
			{
				return &event == ev || event.name == ev->name;
			} );
			active_events.push_back( &event );
		}
	}
}
