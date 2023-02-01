#include "ClientAttachedAnimatedObjectComponent.h"

#include "Common/Game/IsClient.h"
#include "Common/Utility/LockFreeFifo.h"
#include "Common/Utility/StringManipulation.h"
#include "ClientInstanceCommon/Animation/AnimatedObject.h"
#include "ClientInstanceCommon/Animation/Components/AnimationControllerComponent.h"
#include "ClientInstanceCommon/Animation/Components/BaseAnimationEvents.h"
#include "ClientAnimationControllerComponent.h"
#include "Visual/Animation/Components/AnimatedRenderComponent.h"
#include "Visual/Animation/Components/BoneGroupsComponent.h"

namespace Animation
{
	namespace Components
	{

		ClientAttachedAnimatedObjectStatic::ClientAttachedAnimatedObjectStatic( const Objects::Type& parent )
			: AttachedAnimatedObjectStatic( parent )
		{

		}

		COMPONENT_POOL_IMPL(ClientAttachedAnimatedObjectStatic, ClientAttachedAnimatedObject, 1024)

		void ClientAttachedAnimatedObjectStatic::SaveComponent( std::wostream& stream ) const
		{
#ifndef PRODUCTION_CONFIGURATION
			const auto* parent = parent_type.parent_type->GetStaticComponent< ClientAttachedAnimatedObjectStatic >();
			assert( parent );

			if( parent_type.current_origin == Objects::Origin::Shared )
				WriteValueConditionalV( clear_inherited_attached_objects );

			for( const auto& obj : attached_objects )
			{
				if( obj.origin != parent_type.current_origin )
					continue;

				WriteCustom(
				{
					if( obj.slave_animations )
						stream << L"attached_slaved_animated_object = \"";
					else
						stream << L"attached_object = \"";

					if( !obj.effects_propagate && obj.bone_name != "-2" && obj.bone_name != "<lock>" && obj.bone_name != "*" )
						stream << L"lock:";
						
					stream << StringToWstring( obj.bone_name ) << " " << obj.filename;

					if( obj.alias != L"" )
						stream << " alias " << obj.alias;

					stream << "\"";
				} );

				PushIndent();
				if( obj.offset != simd::matrix::identity() )
				{
					simd::vector3 translation, rotation, scale;
					simd::matrix rotation_mat;
					obj.offset.decompose( translation, rotation_mat, scale );
					rotation = rotation_mat.yawpitchroll();

					WriteCustomConditional( stream << L"attached_object_translation = \"" << translation.x << L" " << translation.y << L" " << translation.z << "\"", translation != 0.0f );

					const auto old_precision = stream.precision();
					stream << std::setprecision( 4 );
					WriteCustomConditional( stream << L"attached_object_rotation = \"" << rotation.x << L" " << rotation.y << L" " << rotation.z << "\"", rotation != 0.0f );
					WriteCustomConditional( stream << L"attached_object_scale = \"" << scale.x << L" " << scale.y << L" " << scale.z << "\"", ( scale - simd::vector3( 1.0f ) ).len() > 1e-4f );
					stream << std::setprecision( old_precision );
				}
			}

			if( animation_controller_index != -1 )
			{
				auto on_exit = Finally( [this, old_value = save_all_values]() { save_all_values = old_value; } );
				save_all_values = true;

				const auto* animation_controller = parent_type.GetStaticComponent< AnimationControllerStatic >( animation_controller_index );

				unsigned current_animation = -1;
				for( const auto& attach_event : attached_object_events )
				{
					if( attach_event->copy_of || attach_event->origin != parent_type.current_origin )
						continue;

					const auto* child_controller = attach_event->type->GetStaticComponent< AnimationControllerStatic >();
					if( !child_controller )
					{
						if( attach_event->length == 0.0f )
						{
							LOG_CRIT( L"Failed to save attached animated object event for fixed mesh with no end time" );
							continue;
						}
					}
					if( current_animation != attach_event->animation_index )
					{
						current_animation = attach_event->animation_index;
						WriteValueNamed( L"animation", animation_controller->GetMetadataByIndex( current_animation ).name.c_str() );
					}

					PushIndent();
					WriteCustom( stream << L"attach_event = \"" << attach_event->time << L" " << attach_event->length << L" " <<
								 L"\"" << attach_event->type.GetFilename() << L"\" \"" << ( ( !child_controller || attach_event->animation == -1 ) ? "" : child_controller->GetAnimationNameByIndex( attach_event->animation ) ) << "\"\"" );
					{
						PushIndent();

						WriteValueNamedConditional( L"bone", attach_event->from.bone_name, !attach_event->from.bone_name.empty() && attach_event->from.bone_name != "<root>" );
						WriteValueNamedConditional( L"to_bone", attach_event->to.bone_name, !attach_event->to.bone_name.empty() );

						if( attach_event->from.index_within_child_bone_group && !attach_event->from.child_bone_name.empty() )
						{
							WriteValueNamed( L"child_bone_group", attach_event->from.child_bone_name );
							if( *attach_event->from.index_within_child_bone_group == -1 )
								WriteValueNamed( L"index_within_bone_group", -1 );
							else if( *attach_event->from.index_within_child_bone_group > 0 )
								WriteValueNamed( L"index_within_bone_group", *attach_event->from.index_within_child_bone_group );
						}
						else
							WriteValueNamedConditional( L"child_bone", attach_event->from.child_bone_name, !attach_event->from.child_bone_name.empty() );

						if( attach_event->to.index_within_child_bone_group && !attach_event->to.child_bone_name.empty() )
						{
							WriteValueNamed( L"to_child_bone_group", attach_event->to.child_bone_name );
							if( *attach_event->to.index_within_child_bone_group == -1 )
								WriteValueNamed( L"to_index_within_bone_group", -1 );
							else if( *attach_event->to.index_within_child_bone_group > 0 )
								WriteValueNamed( L"to_index_within_bone_group", *attach_event->to.index_within_child_bone_group );
						}
						else
							WriteValueNamedConditional( L"to_child_bone", attach_event->to.child_bone_name, !attach_event->to.child_bone_name.empty() );

						if( !child_controller )
							( void )0;
						else if( attach_event->speed_type == SpeedType::Fixed )
							WriteValueNamed( L"attach_speed", attach_event->speed );
						else if( attach_event->speed_type == SpeedType::Inherit )
							WriteValueNamed( L"attach_speed", std::string( "inherit" ) );
						else if( attach_event->speed_type == SpeedType::Stretch && attach_event->length > 0.0f )
							WriteValueNamed( L"attach_speed", std::string( "stretch" ) );
						else if( attach_event->speed_type == SpeedType::Curve )
						{
							std::wstringstream ss;
							ss << attach_event->playback_curve.points.size();
							for( const auto& p : attach_event->playback_curve.points )
							{
								ss << L" " << p.time << L" " << p.value << L" " << StringToWstring( std::string( magic_enum::enum_name( p.type ) ) );
								if( p.type == simd::CurveControlPoint::Interpolation::Custom )
									ss << L" " << p.left_dt << L" " << p.right_dt;
								else if( p.type == simd::CurveControlPoint::Interpolation::CustomSmooth )
									ss << L" " << p.left_dt;
							}
							WriteValueNamed( L"attach_speed_curve", ss.str() );
						}

						if( attach_event->to.bone_name.empty() && attach_event->offset != simd::matrix::identity() )
						{
							simd::vector3 translation, rotation, scale;
							simd::matrix rotation_mat;
							attach_event->offset.decompose( translation, rotation_mat, scale );
							rotation = rotation_mat.yawpitchroll();

							WriteCustomConditional( stream << L"attach_translation = \"" << translation.x << L" " << translation.y << L" " << translation.z << "\"", translation != 0.0f );

							const auto old_precision = stream.precision();
							stream << std::setprecision( 4 );
							WriteCustomConditional( stream << L"attach_rotation = \"" << rotation.x << L" " << rotation.y << L" " << rotation.z << "\"", rotation != 0.0f );
							WriteCustomConditional( stream << L"attach_scale = \"" << scale.x << L" " << scale.y << L" " << scale.z << "\"", ( scale - simd::vector3( 1.0f ) ).len() > 1e-4f );
							stream << std::setprecision( old_precision );
						}
					}
				}

				for( const auto& ao_event : animated_object_events )
				{
					if( ao_event->copy_of || ao_event->origin != parent_type.current_origin )
						continue;

					if( current_animation != ao_event->animation_index )
					{
						current_animation = ao_event->animation_index;
						WriteValueNamed( L"animation", animation_controller->GetMetadataByIndex( current_animation ).name.c_str() );
					}

					const auto* child_controller = ao_event->ao_type->GetStaticComponent< AnimationControllerStatic >();
					PushIndent();
					WriteCustom( stream << L"event = \"" << ao_event->time << " \"" << ao_event->ao_type.GetFilename() << L"\" \"" <<
								 ( ao_event->animation == -1 ? "" : child_controller->GetAnimationNameByIndex( ao_event->animation ) ) << "\"\"" );
					{
						PushIndent();

						WriteValueNamedConditional( L"object_type", ao_event->ot_type.GetFilename(), !ao_event->ot_type.IsNull() && ao_event->ot_type.GetFilename() != L"Metadata/Effects/Effect" );

						if( ao_event->flags.test( TargetLocation ) )
							WriteValueNamed( L"target_location", true );
						else if( ao_event->min_offset == ao_event->max_offset )
							WriteCustomConditional( stream << L"offset = \"" << ao_event->min_offset.x << L" " << ao_event->min_offset.y << "\"", ao_event->min_offset.sqrlen() > 0.0f );
						else
							WriteCustom( stream << L"random_offset = \"" << ao_event->min_offset.x << L" " << ao_event->min_offset.y << L" " << ao_event->max_offset.x << L" " << ao_event->max_offset.y << L"\"" );

						if( ao_event->flags.test( InheritHeight ) )
							WriteValueNamed( L"inherit_height", true );
						if( ao_event->min_height == ao_event->max_height )
							WriteValueNamedConditional( L"height", ao_event->min_height, ao_event->min_height != 0.0f );
						else
							WriteCustom( stream << L"random_height = \"" << ao_event->min_height << L" " << ao_event->max_height << "\"" );

						if( ao_event->flags.test( InheritSpeed ) )
							WriteValueNamed( L"inherit_speed", true );
						else if( ao_event->min_speed == ao_event->max_speed )
							WriteValueNamedConditional( L"speed", ao_event->min_speed, ao_event->min_speed != 1.0f );
						else
							WriteCustom( stream << L"random_speed = \"" << ao_event->min_speed << L" " << ao_event->max_speed << L"\"" );

						if( ao_event->flags.test( InheritScale ) )
							WriteValueNamed( L"inherit_scale", true );
						if( ao_event->min_scale == ao_event->max_scale && ao_event->min_scale.x == ao_event->min_scale.y && ao_event->min_scale.y == ao_event->min_scale.z )
							WriteValueNamedConditional( L"scale", ao_event->min_scale.x, ao_event->min_scale.x != 1.0f );
						else if( ao_event->min_scale == ao_event->max_scale )
							WriteCustom( stream << L"scale = \"" << ao_event->min_scale.x << L" " << ao_event->min_scale.y << L" " << ao_event->min_scale.z << L"\"" );
						else
							WriteCustom( stream << L"random_scale = \"" << ao_event->min_scale.x << L" " << ao_event->min_scale.y << L" " << ao_event->min_scale.z << L" "
																		<< ao_event->max_scale.x << L" " << ao_event->max_scale.y << L" " << ao_event->max_scale.z << L"\"" );

						if( ao_event->flags.test( InheritOrientation ) )
							WriteValueNamed( L"inherit_orientation", true );
						if( abs( ao_event->max_orientation - ao_event->min_orientation ) < 1e-4f )
							WriteValueNamedConditional( L"orientation", ao_event->min_orientation, ao_event->min_orientation != 0.0f );
						else
							WriteCustom( stream << L"random_orientation = \"" << ao_event->min_orientation << L" " << ao_event->max_orientation << L"\"" );

						if( ao_event->min_angle_offset == ao_event->max_angle_offset )
							WriteValueNamedConditional( L"angle_offset", ao_event->min_angle_offset, ao_event->min_angle_offset != 0.0f );
						else
							WriteCustom( stream << L"random_angle_offset = \"" << ao_event->min_angle_offset << L" " << ao_event->max_angle_offset << L"\"" );
					}
				}
			}
#endif
		}

		bool ClientAttachedAnimatedObjectStatic::SetValue( const std::string& name, const std::wstring& value )
		{
			if( name == "attach_event" )
			{
				if( current_animation == -1 )
					throw Resource::Exception() << L"No current animation set";

				auto attach_event = std::make_unique< AttachedAnimatedObjectEvent >();
				attach_event->animation_index = current_animation;

				std::wstringstream stream( value );
				stream >> attach_event->time >> attach_event->length;

				std::wstring filename, animation;
				if( !ExtractString( stream, filename ) || !ExtractString( stream, animation ) )
					throw Resource::Exception() << L"Failed to read attach event filename or animation";
				attach_event->type = filename;

				if( !animation.empty() )
				{
					const auto* child_controller = attach_event->type->GetStaticComponent< AnimationControllerStatic >();
					attach_event->animation = child_controller->GetAnimationIndexByName( WstringToString( animation ) );
				}

#ifndef PRODUCTION_CONFIGURATION
				attach_event->origin = parent_type.current_origin;
#endif
				attached_object_events.emplace_back( std::move( attach_event ) );

				assert( animation_controller_index != -1 );
				auto* animation_controller = parent_type.GetStaticComponent< AnimationControllerStatic >( animation_controller_index );
				animation_controller->AddEvent( current_animation, *attached_object_events.back() );

				return true;
			}
			if( name == "bone" && !attached_object_events.empty() )
			{
				attached_object_events.back()->from.bone_name = WstringToString( value );
				return true;
			}
			if( name == "child_bone" && !attached_object_events.empty() )
			{
				attached_object_events.back()->from.child_bone_name = WstringToString( value );
				return true;
			}
			if( name == "child_bone_group" && !attached_object_events.empty() )
			{
				attached_object_events.back()->from.child_bone_name = WstringToString( value );
				attached_object_events.back()->from.index_within_child_bone_group = 0;
				return true;
			}
			if( name == "to_bone" && !attached_object_events.empty() )
			{
				attached_object_events.back()->to.bone_name = WstringToString( value );
				return true;
			}
			if( name == "to_child_bone" && !attached_object_events.empty() )
			{
				attached_object_events.back()->to.child_bone_name = WstringToString( value );
				return true;
			}
			if( name == "to_child_bone_group" && !attached_object_events.empty() )
			{
				attached_object_events.back()->to.child_bone_name = WstringToString( value );
				attached_object_events.back()->to.index_within_child_bone_group = true;
				return true;
			}
			if( name == "attach_speed" && !attached_object_events.empty() )
			{
				assert( attached_object_events.back()->type->HasComponent< AnimationControllerStatic >() );
				if( value == L"inherit" )
					attached_object_events.back()->speed_type = SpeedType::Inherit;
				else if( value == L"stretch" )
				{
					if( attached_object_events.back()->length <= 0.0f )
						throw Resource::Exception() << L"Cannot stretch attached object event with no end time";
					attached_object_events.back()->speed_type = SpeedType::Stretch;
					attached_object_events.back()->playback_curve.points.emplace_back( simd::CurveControlPoint::Interpolation::Linear, attached_object_events.back()->time, 0.0f );
					attached_object_events.back()->playback_curve.points.emplace_back( simd::CurveControlPoint::Interpolation::Linear, attached_object_events.back()->time + attached_object_events.back()->length, 1.0f );
					attached_object_events.back()->playback_track = attached_object_events.back()->playback_curve.MakeTrack( 0.0f );
				}
				return true;
			}
			if( name == "attach_speed_curve" && !attached_object_events.empty() )
			{
				if( attached_object_events.back()->length <= 0.0f )
					throw Resource::Exception() << L"Cannot set attached object event speed curve with no end time";

				assert( attached_object_events.back()->type->HasComponent< AnimationControllerStatic >() );

				attached_object_events.back()->speed_type = SpeedType::Curve;
				unsigned num_points = 0;
				std::wstringstream stream( value );
				stream >> num_points;
				if( num_points < 2 || num_points > attached_object_events.back()->playback_curve.NumControlPoints )
					throw Resource::Exception() << L"Unable to parse attached object event speed curve";

				for( unsigned i = 0; i < num_points; ++i )
				{
					simd::CurveControlPoint p;
					std::wstring type;
					stream >> p.time >> p.value >> type;

					auto mode = magic_enum::enum_cast< simd::CurveControlPoint::Interpolation >( WstringToString( type ) );
					if( stream.fail() || !mode )
						throw Resource::Exception() << L"Unable to parse attached object event speed curve";
					p.type = *mode;

					if( p.type == simd::CurveControlPoint::Interpolation::Custom )
						stream >> p.left_dt >> p.right_dt;
					else if( p.type == simd::CurveControlPoint::Interpolation::CustomSmooth )
						stream >> p.left_dt;
					attached_object_events.back()->playback_curve.points.push_back( std::move( p ) );
				}
				if( stream.fail() )
					throw Resource::Exception() << L"Unable to parse attached object event speed curve";

				attached_object_events.back()->playback_curve.RescaleTime( attached_object_events.back()->time, attached_object_events.back()->length );
				attached_object_events.back()->playback_track = attached_object_events.back()->playback_curve.MakeTrack( 0.0f );
				return true;
			}
			if( name == "attach_translation" && !attached_object_events.empty() )
			{
				std::wstringstream stream( value );
				simd::vector3 translation;
				stream >> translation.x >> translation.y >> translation.z;
				attached_object_events.back()->offset = simd::matrix::translation( translation ) * attached_object_events.back()->offset;
				return true;
			}
			if( name == "attach_rotation" && !attached_object_events.empty() )
			{
				std::wstringstream stream( value );
				simd::vector3 rotation;
				stream >> rotation.x >> rotation.y >> rotation.z;
				attached_object_events.back()->offset = simd::matrix::yawpitchroll( rotation ) * attached_object_events.back()->offset;
				return true;
			}
			if( name == "attach_scale" && !attached_object_events.empty() )
			{
				std::wstringstream stream( value );
				simd::vector3 scale;
				stream >> scale.x >> scale.y >> scale.z;
				attached_object_events.back()->offset = simd::matrix::scale( scale ) * attached_object_events.back()->offset;
				return true;
			}
			return AttachedAnimatedObjectStatic::SetValue( name, value );
		}

		bool ClientAttachedAnimatedObjectStatic::SetValue( const std::string& name, const float value )
		{
			if( name == "attach_speed" && !attached_object_events.empty() )
			{
				assert( attached_object_events.back()->type->HasComponent< AnimationControllerStatic >() );
				attached_object_events.back()->speed = value;
				attached_object_events.back()->speed_type = SpeedType::Fixed;
				return true;
			}
			if( name == "attach_scale" && !attached_object_events.empty() )
			{
				attached_object_events.back()->offset = simd::matrix::scale( value ) * attached_object_events.back()->offset;
				return true;
			}

			return AttachedAnimatedObjectStatic::SetValue( name, value );
		}

		bool ClientAttachedAnimatedObjectStatic::SetValue( const std::string& name, const int value )
		{
			if( name == "index_within_bone_group" && !attached_object_events.empty() )
			{
				attached_object_events.back()->from.index_within_child_bone_group = value;
				return true;
			}
			if( name == "to_index_within_bone_group" && !attached_object_events.empty() )
			{
				attached_object_events.back()->to.index_within_child_bone_group = value;
				return true;
			}

			return AttachedAnimatedObjectStatic::SetValue( name, value );
		}

		void ClientAttachedAnimatedObjectStatic::CopyAnimationData( const int source_animation, const int destination_animation, std::function<bool( const Animation::Event& )> predicate, std::function<void( const Animation::Event&, Animation::Event& )> transform )
		{
			auto* animation_controller = parent_type.GetStaticComponent< AnimationControllerStatic >( animation_controller_index );

			std::vector< AttachedAnimatedObjectEvent > copied;
			for( auto& e : attached_object_events )
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
				attached_object_events.emplace_back( std::make_unique< AttachedAnimatedObjectEvent >( std::move( e ) ) );
				animation_controller->AddEvent( destination_animation, *attached_object_events.back() );
			}
			return AttachedAnimatedObjectStatic::CopyAnimationData( source_animation, destination_animation, predicate, transform );
		}

		void ClientAttachedAnimatedObjectStatic::FixupBoneName( std::string& name )
		{
			// TODO: Fix deprecated bone syntax.
			if( name.find_first_not_of( "0123456789" ) == -1 )
			{
				const unsigned bone_index = std::stoul( name );
				if( bone_index == -1 )
					name = "<root>";
				else if( bone_index == -2 )
					name = "<lock>";
				else
				{
					auto* animation_controller_static = this->parent_type.GetStaticComponent< ClientAnimationControllerStatic >();
					name = animation_controller_static->skeleton->GetBoneName( bone_index );
				}
			}
			else if( name == "*" )
				name = "<lock>";
		}

		void ClientAttachedAnimatedObjectStatic::RemoveAttachedAnimatedObjectEvent( const AttachedAnimatedObjectEvent& event )
		{
			auto* animation_controller = parent_type.GetStaticComponent< AnimationControllerStatic >();
			animation_controller->RemoveEvent( event );

			const auto found = std::find_if( attached_object_events.begin(), attached_object_events.end(), [&]( const std::unique_ptr< AttachedAnimatedObjectEvent >& iter ) { return iter.get() == &event; } );
			attached_object_events.erase( found );
		}

		void ClientAttachedAnimatedObjectStatic::AddAttachedAnimatedObjectEvent( std::unique_ptr<AttachedAnimatedObjectEvent> event )
		{
			const unsigned animation_index = event->animation_index;
			attached_object_events.emplace_back( std::move( event ) );
			//Add the event
			auto* animation_controller = parent_type.GetStaticComponent< AnimationControllerStatic >();
			animation_controller->AddEvent( animation_index, *attached_object_events.back() );
			//Needs sorting now
			animation_controller->SortEvents();
		}

		ClientAttachedAnimatedObject::ClientAttachedAnimatedObject( const ClientAttachedAnimatedObjectStatic& static_ref, Objects::Object& object )
			: AttachedAnimatedObject( static_ref, object ), static_ref( static_ref )
		{

		}

		void ClientAttachedAnimatedObject::RenderFrameMove( const float elapsed_time )
		{
			RenderFrameMoveInternal( elapsed_time );
		}

		void ClientAttachedAnimatedObject::RenderFrameMoveNoVirtual( AnimationObjectComponent* component, const float elapsed_time )
		{
			static_cast< ClientAttachedAnimatedObject* >( component )->RenderFrameMoveInternal( elapsed_time );
		}

		void Animation::Components::ClientAttachedAnimatedObject::DecoupleAttachedObject( const std::shared_ptr<AnimatedObject>& object )
		{
			auto attached_object = FindAttached( object );
			if( attached_object == AttachedObjectsEnd() )
				return;

			auto current_transform = GetObject().GetComponent< AnimatedRender >()->AttachedObjectTransform( *attached_object );

			attached_object->offset = current_transform;
			attached_object->ignore_transform_flags = IgnoreTranslation | IgnoreRotation | IgnoreScale;
		}

		void ClientAttachedAnimatedObject::RenderFrameMoveInternal( const float elapsed_time )
		{
			for( auto it = playback_controlled_effects.begin(); it != playback_controlled_effects.end(); )
			{
				const auto ao = it->effect.lock();
				if( !ao )
				{
					it = playback_controlled_effects.erase( it );
					continue;
				}

				const auto* this_controller = GetObject().GetComponent< ClientAnimationController >();
				const float this_position = this_controller->GetCurrentAnimationPosition();

				if( this_position >= it->attach_event->time + it->attach_event->length )
				{
					if( IsWeakPtrNull( it->parent ) )
						RemoveAttachedObjectNextFrame( ao );
					else if( const auto parent = it->parent.lock() )
						parent->GetComponent< AttachedAnimatedObject >()->RemoveAttachedObject( ao );

					it = playback_controlled_effects.erase( it );
					continue;
				}

				if( it->attach_event->speed_type == SpeedType::Curve || it->attach_event->speed_type == SpeedType::Stretch )
				{
					auto* child_controller = ao->GetComponent< AnimationController >();
					auto* client_controller = ao->GetComponent< ClientAnimationController >();
					const float animation_length = client_controller->GetCurrentAnimationLength();
					const float start_position = client_controller->GetCurrentAnimationPosition();
					const float end_position = Clamp( it->attach_event->playback_track.Interpolate( this_position ), 0.0f, 1.0f ) * animation_length;
					if( end_position >= start_position )
					{
						const float target_speed = elapsed_time <= 0.0f ? 0.0f : std::max( 0.0f, ( end_position - start_position ) / elapsed_time );
						child_controller->SetPlayingAnimationSpeedMultiplier( target_speed );
					}
					else
					{
						child_controller->SetPlayingAnimationSpeedMultiplier( 0.0f );
						child_controller->SetCurrentAnimationPosition( end_position );
					}
				}

				++it;
			}
		}

		void ClientAttachedAnimatedObject::UpdateBeam( const AttachedObject& object )
		{
			auto it = playing_beams.find( object.object.get() );
			if( it != playing_beams.end() )
			{
				const bool from_this = IsWeakPtrNull( it->second.from );
				const bool to_this = IsWeakPtrNull( it->second.to );
				const auto& from_shared_ptr = from_this ? nullptr : it->second.from.lock();
				const auto& to_shared_ptr = to_this ? nullptr : it->second.to.lock();
				const auto* from = from_this ? &this->GetObject() : from_shared_ptr.get();
				const auto* to = to_this ? &this->GetObject() : to_shared_ptr.get();
				if( !from || !to )
				{
					playing_beams.erase( it );
					if( !Contains( finished_effects, object.object ) )
						finished_effects.push_back( object.object );
					return;
				}

				const simd::vector3 from_world_position = [&]()
				{
					if( it->second.from_index_within_bone_group )
					{
						const auto& bone_positions = from->GetComponent< BoneGroups >()->GetBoneWorldPositions( it->second.from_bone_index );
						return *it->second.from_index_within_bone_group < bone_positions.size() ? bone_positions[ *it->second.from_index_within_bone_group ] : bone_positions.back();
					}
					else return from->GetComponent< ClientAnimationController >()->GetBoneWorldPosition( it->second.from_bone_index );
				}( );
				const simd::vector3 to_world_position = [&]()
				{
					if( it->second.to_index_within_bone_group )
					{
						const auto& bone_positions = to->GetComponent< BoneGroups >()->GetBoneWorldPositions( it->second.to_bone_index );
						return *it->second.to_index_within_bone_group < bone_positions.size() ? bone_positions[ *it->second.to_index_within_bone_group ] : bone_positions.back();
					}
					else return to->GetComponent< ClientAnimationController >()->GetBoneWorldPosition( it->second.to_bone_index );
				}( );

				const auto inverse_transform = GetObject().GetComponent< AnimatedRender >()->GetTransform().inverse();
				const simd::vector3 from_position = inverse_transform.mulpos( from_world_position );
				const simd::vector3 to_position = inverse_transform.mulpos( to_world_position );
				const simd::vector3 beam_dist = to_position - from_position;
				const simd::vector3 scale( it->second.scale.x, it->second.scale.y * beam_dist.len(), it->second.scale.z );
				const float orientation = Geometric::OrientationFromVector( to_position.x - from_position.x, to_position.y - from_position.y );
				const float tilt = atan2f( -beam_dist.z, beam_dist.xy().len() );

				const simd::matrix offset = simd::matrix::scale( scale ) * simd::matrix::rotationX( tilt ) * simd::matrix::rotationZ( orientation ) * simd::matrix::translation( from_position );
				SetAttachedObjectMatrix( object.object, offset );
			}
		}

		void ClientAttachedAnimatedObject::SetAttachedObjectVisibilityString( const Memory::Vector<std::wstring, Memory::Tag::Mesh>& names, const bool show )
		{
			AttachedAnimatedObject::SetAttachedObjectVisibilityString( names, show );
			UpdateAttachedRenderEnabled();
		}

		void ClientAttachedAnimatedObject::SetAttachedObjectVisibilityString( const AttachedObject& object, const Memory::Vector<std::wstring, Memory::Tag::Mesh>& names, const bool show )
		{
			AttachedAnimatedObject::SetAttachedObjectVisibilityString( object, names, show );
			UpdateAttachedRenderEnabled();
		}

		void ClientAttachedAnimatedObject::AddChildAttachedObjects( const AnimatedObjectTypeHandle& type, const simd::matrix& transform, const StaticType::AttachmentEndpoint& from_endpoint, const StaticType::AttachmentEndpoint& to_endpoint, ClientAttachedAnimatedObject::ChildAttachedCallback callback )
		{
			const bool is_beam = !to_endpoint.bone_name.empty();

			struct Endpoint
			{
				std::shared_ptr< AnimatedObject > parent;
				unsigned bone_index = 0;
				std::optional< unsigned > index_within_bone_group;
			};

			const auto FindChildEndpoints = [is_beam]( std::vector< Endpoint >& results,
				const BoneGroups* bone_groups,
				const ClientAnimationController* client_animation_controller,
				const std::shared_ptr< AnimatedObject >& ao,
				const std::string& bone_name,
				const std::optional< unsigned >& index_within_bone_group )
			{
				if( index_within_bone_group )  // Attach to a bone group
				{
					if( !bone_groups )
						return;

					if( !is_beam && bone_groups->static_ref.fixed_mesh_index != -1 )
						return;

					if( Contains( bone_name, '*' ) )
					{
						for( unsigned bone_group_index = 0; bone_group_index < bone_groups->static_ref.NumBoneGroups(); ++bone_group_index )
						{
							const auto& bone_group = bone_groups->static_ref.BoneGroupName( bone_group_index );
							if( WildcardSearch( bone_group, bone_name ) )
							{
								if( is_beam )
									results.push_back( { ao, bone_group_index, index_within_bone_group } );
								else
								{
									const auto& bone_indices = bone_groups->static_ref.GetBoneGroup( bone_group_index ).bone_indices;
									if( bone_indices.empty() )
										continue;
									const auto bone_index = *index_within_bone_group < bone_indices.size() ? bone_indices[ *index_within_bone_group ] : bone_indices.back();
									results.push_back( { ao, bone_index } );
								}
							}
						}
					}
					else
					{
						const unsigned bone_group_index = bone_groups->static_ref.BoneGroupIndexByName( bone_name );
						if( bone_group_index != -1 )
						{
							if( is_beam )
								results.push_back( { ao, bone_group_index, index_within_bone_group } );
							else
							{
								const auto& bone_indices = bone_groups->static_ref.GetBoneGroup( bone_group_index ).bone_indices;
								if( bone_indices.empty() )
									return;
								const auto bone_index = *index_within_bone_group < bone_indices.size() ? bone_indices[ *index_within_bone_group ] : bone_indices.back();
								results.push_back( { ao, bone_index } );
							}
						}
					}
				}
				else  // Attach to a bone
				{
					if( !client_animation_controller )
						return;

					if( Contains( bone_name, '*' ) )
					{
						for( unsigned bone_index = 0; bone_index < client_animation_controller->GetNumBonesAndSockets(); ++bone_index )
						{
							const auto& bone_name_to_test = client_animation_controller->GetBoneNameByIndex( bone_index );
							if( WildcardSearch( bone_name_to_test, bone_name ) )
								results.push_back( { ao, bone_index } );
						}
					}
					else
					{
						const auto bone_index = client_animation_controller->GetBoneIndexByName( bone_name.c_str() );
						if( bone_index < client_animation_controller->GetNumBonesAndSockets() || ( !is_beam && bone_index != Bones::Invalid ) )
							results.push_back( { ao, bone_index } );
					}
				}
			};

			const auto FindEndpoints = [&]( std::vector< Endpoint >& results, const StaticType::AttachmentEndpoint& endpoint )
			{
				if( endpoint.child_bone_name.empty() )  // Attach to the current object
				{
					FindChildEndpoints( results, GetObject().GetComponent< BoneGroups >(), GetObject().GetComponent< ClientAnimationController >(), nullptr, endpoint.bone_name, endpoint.index_within_bone_group );
				}
				else  // Attach to a child of the current object
				{
					const auto* bone_groups = endpoint.index_within_bone_group ? GetObject().GetComponent< BoneGroups >() : nullptr;
					const auto* client_controller = endpoint.index_within_bone_group ? GetObject().GetComponent< ClientAnimationController >() : nullptr;

					for( const auto& ao : attached_objects )
					{
						if( endpoint.index_within_bone_group )
						{
							if( !client_controller || !bone_groups || bone_groups->static_ref.fixed_mesh_index != -1 )
								break;

							bool found = false;
							for( unsigned idx = 0; idx < bone_groups->static_ref.NumBoneGroups(); ++idx )
							{
								const auto& bone_group = bone_groups->static_ref.BoneGroupName( idx );
								if( WildcardSearch( bone_group, endpoint.bone_name ) )
								{
									const auto& bone_indices = bone_groups->static_ref.GetBoneGroup( idx ).bone_indices;
									if( bone_indices.empty() )
										return;

									const auto bone_index = *endpoint.index_within_bone_group < bone_indices.size() ? bone_indices[ *endpoint.index_within_bone_group ] : bone_indices.back();
									if( ao.bone_name == client_controller->GetBoneNameByIndex( bone_index ) )
									{
										found = true;
										break;
									}
								}
							}

							if( !found )
								continue;
						}
						else if( !WildcardSearch( ao.bone_name, endpoint.bone_name ) )
							continue;

						FindChildEndpoints( results, ao.object->GetComponent< BoneGroups >(), ao.object->GetComponent< ClientAnimationController >(), ao.object, endpoint.child_bone_name, endpoint.index_within_child_bone_group );
					}
				}
			};

			std::vector< Endpoint > from_endpoints;
			FindEndpoints( from_endpoints, from_endpoint );
			if( !is_beam )
			{
				for( const Endpoint& endpoint : from_endpoints )
				{
					ClientAttachedAnimatedObject* attached_component = !endpoint.parent ? this : endpoint.parent->GetComponent< ClientAttachedAnimatedObject >();
					if( !attached_component )
						continue;

					ClientAnimationController* client_animation_controller = attached_component->GetObject().GetComponent< ClientAnimationController >();
					if( !client_animation_controller )
						continue;

					auto ao = attached_component->AddAttachedObjectFromType( GetObject().global_variables, type, client_animation_controller->GetBoneNameByIndex( endpoint.bone_index ).c_str(), transform );
					callback( endpoint.parent, ao );
				}
				return;
			}

			if( from_endpoints.empty() )
				return;

			std::vector< Endpoint > to_endpoints;
			FindEndpoints( to_endpoints, to_endpoint );
			const auto num_beams = std::min( from_endpoints.size(), to_endpoints.size() );
			for( unsigned i = 0; i < num_beams; ++i )
			{
				const auto& from = from_endpoints[ i ];
				const auto& to = to_endpoints[ i ];

				// Beams are always attached to this object
				auto ao = AddAttachedObjectFromType( GetObject().global_variables, type, nullptr, transform );

				PlayingBeam beam = { from.parent, to.parent, from.bone_index, to.bone_index, 1.0f, from.index_within_bone_group, to.index_within_bone_group };

				const float beam_length = [&]() -> float
				{
					if( const auto* base_animation_events = type->GetStaticComponent< BaseAnimationEventsStatic >() )
					{
						if( base_animation_events->beam_length > 0 )
							return ( float )base_animation_events->beam_length;
					}
					const auto& beam_size = ao->GetComponent< AnimatedRender >()->GetObjectSize();
					return beam_size.maximum_point.y - beam_size.minimum_point.y;
				}();

				if( beam_length > 0.0f )
					beam.scale.y /= beam_length;

				playing_beams[ ao.get() ] = std::move( beam );
				callback( nullptr, ao );
			}
		}

		void ClientAttachedAnimatedObject::UpdateAttachedRenderEnabled()
		{
			for( auto& attached : attached_objects )
			{
				auto* attached_render = attached.object->GetComponent< AnimatedRender >();
				if( attached.hidden )
					attached_render->DisableRendering();
				else
					attached_render->EnableRendering();
			}
		}

		void ClientAttachedAnimatedObject::OnAnimationStart( const AnimationController& controller, const CurrentAnimation& animation, const bool blend )
		{
			AttachedAnimatedObject::OnAnimationStart( controller, animation, blend );

			if( &controller == GetAnimationController() )
				RemoveInterruptibleEffects();
		}

		void ClientAttachedAnimatedObject::OnAnimationEnd( const AnimationController& controller, const unsigned animation_index, const unsigned animation_unique_id )
		{
			AttachedAnimatedObject::OnAnimationEnd( controller, animation_index, animation_unique_id );

			if( &controller == GetAnimationController() )
				RemoveInterruptibleEffects();
		}

		void ClientAttachedAnimatedObject::OnAnimationLooped( const AnimationController& controller, const unsigned animation_index )
		{
			AttachedAnimatedObject::OnAnimationLooped( controller, animation_index );

			if( &controller == GetAnimationController() )
				RemoveInterruptibleEffects();
		}

		void ClientAttachedAnimatedObject::OnAnimationEvent( const AnimationController& controller, const Event& e )
		{
			AttachedAnimatedObject::OnAnimationEvent( controller, e );

			if( &controller != GetAnimationController() )
				return;

			if( e.event_type == AttachedAnimatedObjectEventType )
			{
				const auto& attach_event = static_cast< const ClientAttachedAnimatedObjectStatic::AttachedAnimatedObjectEvent& >( e );
				const simd::matrix transform = attach_event.to.bone_name.empty() ? attach_event.offset : simd::matrix::identity();

#if defined( DEVELOPMENT_CONFIGURATION ) || defined( TESTING_CONFIGURATION )
				// In Asset Viewer, skip events that have no end time if attaching a fixed mesh, or if their animation loops (this is also checked in Asset Tester)
				if( attach_event.length <= 0.0f )
				{
					const auto* controller = attach_event.type->GetStaticComponent< AnimationControllerStatic >();
					if( !controller )
						return;
					else if( attach_event.animation == -1 )
					{
						const unsigned default_animation = controller->GetDefaultAnimationIndex();
						if( default_animation == -1 || controller->IsLoopingAnimation( default_animation ) )
							return;
					}
					else if( controller->IsLoopingAnimation( attach_event.animation ) )
						return;
				}
#endif

				AddChildAttachedObjects( attach_event.type, transform, attach_event.from, attach_event.to, [&]( const std::shared_ptr< AnimatedObject >& parent, const std::shared_ptr< AnimatedObject >& ao )
				{
					const float speed = [&]() -> float
					{
						if( attach_event.speed_type == SpeedType::Fixed )
							return attach_event.speed;
						if( attach_event.speed_type == SpeedType::Inherit )
							return controller.GetCurrentAnimationSpeed();
						else
							return 0.0f;  // Speed is controlled by playback curve
					}( );

					if( attach_event.animation != -1 )
					{
						if( auto* child_controller = ao->GetComponent< AnimationController >() )
							child_controller->PlayAnimation( attach_event.animation, speed );
					}
					else if( speed != 1.0f )
					{
						if( auto* child_controller = ao->GetComponent< AnimationController >() )
							child_controller->SetPlayingAnimationSpeedMultiplier( speed );
					}

					if( attach_event.length <= 0.0f )
						( parent ? parent->GetComponent< ClientAttachedAnimatedObject >() : this )->ConvertAttachedObjectToEffect( ao );  // Remove when the attached object's animation ends
					else
						playback_controlled_effects.push_back( { parent, ao, &attach_event } );  // Remove at the end of the event, or earlier if this object's animation is interrupted

#if defined( DEVELOPMENT_CONFIGURATION ) || defined( TESTING_CONFIGURATION )
					if( Game::IsTool() )
					{
						attach_event.active_event_parent = parent;
						attach_event.active_event_ao = ao;

						ao->GetComponent< AnimatedRender >()->EnableOrphanedEffects( true );
					}
#endif
				} );
			}
		}

		void ClientAttachedAnimatedObject::OnAnimationEventTriggered( AnimatedObject& object, const AttachedAnimatedObjectStatic::AnimatedObjectEvent& e )
		{
			object.GetComponent< AnimatedRender >()->SetScale( e.min_scale.lerp( e.max_scale, RandomFloat() ) );
		}

		void ClientAttachedAnimatedObject::OnObjectRemoved( const AttachedObject& object )
		{
			playing_beams.erase( object.object.get() );
		}

		void ClientAttachedAnimatedObject::RemoveInterruptibleEffects()
		{
			for( const auto& effect : playback_controlled_effects )
			{
				if( const auto child = effect.effect.lock() )
				{
					if( IsWeakPtrNull( effect.parent ) )
						RemoveAttachedObject( child );
					else if( const auto parent = effect.parent.lock() )
						parent->GetComponent< AttachedAnimatedObject >()->RemoveAttachedObject( child );
				}
			}
			playback_controlled_effects.clear();
		}
	}
}

