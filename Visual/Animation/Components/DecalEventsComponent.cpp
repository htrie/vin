#include "DecalEventsComponent.h"
#include "ClientAnimationControllerComponent.h"

#include "Common/Utility/Format.h"
#include "ClientInstanceCommon/Animation/AnimatedObject.h"
#include "ClientInstanceCommon/Animation/Components/AnimationControllerComponent.h"
#include "AnimatedRenderComponent.h"
#include "Visual/Decals/Decal.h"
#include "Visual/Decals/DecalManager.h"
#include "Visual/Mesh/BoneConstants.h"

namespace Animation
{
	namespace Components
	{

		DecalEventsStatic::DecalEventsStatic( const Objects::Type& parent ) 
			: AnimationObjectStaticComponent( parent )
			, animation_controller( parent.GetStaticComponentIndex< AnimationControllerStatic >() )
			, animated_render( parent.GetStaticComponentIndex< AnimatedRenderStatic >() )
			, client_animation_controller( parent.GetStaticComponentIndex< ClientAnimationControllerStatic >() )
		{

		}
		
		COMPONENT_POOL_IMPL(DecalEventsStatic, DecalEvents, 64)

		std::unique_ptr< DecalEventsStatic::DecalEvent > DecalEventsStatic::GetDecalEvent( const std::wstring& str, const float time ) const
		{
			//Parse the value
			std::wstringstream stream( str );
			std::wstring filename;
			if( !( stream >> filename ) )
				throw Resource::Exception() << L"Unable to read decal atlas filename";

			std::wstring group_name;
			if( !( stream >> group_name ) )
				throw Resource::Exception() << L"Unable to read decal group name.";

			std::wstring bone_name;
			if( !( stream >> bone_name ) )
				throw Resource::Exception() << L"Unable to read bone name";

			float size;
			if( !( stream >> size ) )
				throw Resource::Exception() << L"Unable to read decal size";

			float fade_in_time;
			if( !( stream >> fade_in_time ) )
				throw Resource::Exception() << L"Unable to read decal fade in time";

			// Optional flags/values after this point
			bool actor_scale = true;

			std::wstring value;
			while( stream >> value )
			{
				if( value == L"no_actor_scale" )
					actor_scale = false;
			}

			const Materials::AtlasGroupHandle atlas_group = GetAtlasGroup( Materials::MaterialAtlasHandle( filename ), group_name );
			if( atlas_group.IsNull() )
				throw Resource::Exception() << L"No decal group named " << group_name;

			unsigned bone_index = Bones::Invalid;
			if( bone_name == L"-1" || bone_name == L"<root>" )
				bone_index = Bones::Root;
			else if( bone_name == L"-2" || bone_name == L"<lock>" )
				bone_index = Bones::Lock;
			else if( const auto* controller = GetController() )
				bone_index = controller->GetBoneIndexByName( WstringToString( bone_name ) );
			if( bone_index == Bones::Invalid )
				throw Resource::Exception() << L"Bone name doesn't exist. " << bone_name.c_str();

			auto new_event = std::make_unique< DecalEvent >( time, current_animation, atlas_group, size, bone_index, fade_in_time, filename, actor_scale );

#ifndef PRODUCTION_CONFIGURATION
			new_event->origin = parent_type.current_origin;
#endif
			return new_event;
		}

		bool DecalEventsStatic::SetValue( const std::string& name, const std::wstring& value )
		{
			if( name == "on_create" )
			{
				create_event = GetDecalEvent( value, 0.0f );
				return true;
			}
			else if( name == "animation" )
			{
				const auto* animation_controller = parent_type.GetStaticComponent< AnimationControllerStatic >();
				if( !animation_controller )
					throw Resource::Exception() << L"Tried to set an animation event without an AnimationController";

				const unsigned animation_index = animation_controller->GetAnimationIndexByName( WstringToString( value ) );
				if( animation_index == -1 )
					throw Resource::Exception() << value << L" animation not found when setting decal.";

				current_animation = animation_index;
				return true;
			}  
			else
			{
				//Assume that the name is actually a float value indicating the time into an animation that the event is
				float time = 0.0f;
				if( sscanf_s( name.c_str(), "%f", &time ) == 0 )
					return false; //not a float value

				decal_events.push_back( GetDecalEvent( value, time ) );

				auto* animation_controller = parent_type.GetStaticComponent< AnimationControllerStatic >();
				if( !animation_controller )
					throw Resource::Exception() << L"Tried to set an animation event without an AnimationController";

				animation_controller->AddEvent( current_animation, *decal_events.back() );
				return true;
			}

			return false;
		}

		bool DecalEventsStatic::SetValue( const std::string &name, const float value )
		{
			if( name == "size_variation_maximum_increase" )
			{
				//size of decals is scaled up by somewhere between 0% to this value (treated as a percentage)
				//so a value of 0.2 means it's randomly scaled up by 0% - 20% of the base size in the decal event.
				size_variation_maximum_increase = value;
				return true;
			}

			return false;
		}

		bool DecalEventsStatic::SetValue( const std::string &name, const bool value )
		{
			if( name == "random_rotation" )
			{
				random_rotation = value;
				return true;
			}
			else if( name == "jitter" )
			{
				jitter = value;
				return true;
			}

			return false;
		}

		ClientAnimationControllerStatic* DecalEventsStatic::GetController() const
		{
			return parent_type.GetStaticComponent< ClientAnimationControllerStatic >();
		}

		void DecalEventsStatic::SaveComponent( std::wostream& stream ) const
		{
#ifndef PRODUCTION_CONFIGURATION
			const auto* parent = parent_type.parent_type->GetStaticComponent< DecalEventsStatic >();
			assert( parent );

			const auto* animation_controller = parent_type.GetStaticComponent< AnimationControllerStatic >( this->animation_controller );
			const auto* client_animation_controller = parent_type.GetStaticComponent< ClientAnimationControllerStatic >();

			WriteValueIfSet( random_rotation );
			WriteValueIfSet( jitter );
			WriteValueIfSet( size_variation_maximum_increase );

			const auto FormatDecalEvent = [&]( const DecalEvent& decal_event )
			{
				std::wstring bone_name = L"invalid";
				if( decal_event.bone_index == -1 )
					bone_name = L"<root>";
				else if( decal_event.bone_index == -2 )
					bone_name = L"<lock";
				else if( client_animation_controller )
					bone_name = StringToWstring( client_animation_controller->GetBoneNameByIndex( decal_event.bone_index ) );

				return Utility::format( L"{} {} {} {} {}{}", decal_event.texture.GetParent().GetFilename(), decal_event.texture->name, bone_name, decal_event.size, decal_event.fade_in_time, decal_event.scale_with_actor ? L"" : L" no_actor_scale" );
			};

			WriteValueNamedConditional( L"on_create", FormatDecalEvent( *create_event ), !!create_event );
			
			if( animation_controller ) //write animation events
			{
				unsigned current_animation = -1;
				for( const std::unique_ptr< DecalEvent >& decal_event : decal_events )
				{
					if( decal_event->copy_of )
						continue;

					if( !decal_event->texture )
						continue;

					if( decal_event->origin != parent_type.current_origin )
						continue;

					if( current_animation != decal_event->animation_index )
					{
						current_animation = decal_event->animation_index;
						WriteValueNamed( L"animation", animation_controller->GetMetadataByIndex( current_animation ).name.c_str() );
					}

					PushIndent();
					WriteCustom(
						{
							stream << decal_event->time << L" = \"" << FormatDecalEvent( *decal_event ) << L"\"";
						} );
				}
			}
#endif
		}

		DecalEvents::DecalEvents( const DecalEventsStatic& static_ref, Objects::Object& object ) 
			: AnimationObjectComponent( object ), static_ref( static_ref )
		{ 
			if( static_ref.animation_controller != -1 )
				anim_cont_list_ptr = GetObject().GetComponent< AnimationController >( static_ref.animation_controller )->GetListener( *this );
		}

		void DecalEvents::CreateDecal( const Event& e ) const
		{
			PROFILE;
			if( !decal_manager )
				return;

			if( e.event_type != DecalEventType )
				return;
			
			const auto& decal_event = static_cast< const DecalEventsStatic::DecalEvent& >( e );

			const auto* animated_render = GetObject().GetComponent< AnimatedRender >( static_ref.animated_render );
			assert( animated_render );
			const auto* client_animation_controller = ( static_ref.client_animation_controller != -1 ) ? GetObject().GetComponent< ClientAnimationController >( static_ref.client_animation_controller ) : nullptr;
			const bool check_bone = (int)decal_event.bone_index >= 0 && client_animation_controller != nullptr;

			const simd::vector2 forwards(0, -1);
			const simd::matrix bone_transform = check_bone ? client_animation_controller->GetBoneTransform(decal_event.bone_index) : simd::matrix::identity();
			const simd::matrix render_transform = animated_render->GetTransform();

			const simd::vector2 out = bone_transform.muldir(forwards);
			const simd::vector2 out2 = render_transform.muldir(out);

			const float orientation = Geometric::OrientationFromVector(out2[0], out2[1]);
			const float scale = out2.len();

			Decals::Decal decal;
			decal.angle = static_ref.random_rotation ? ( generate_random_number( 256 ) / 255.0f ) * PI2 : orientation;
			decal.size = decal_event.size;

			if( decal_event.scale_with_actor )
				decal.size *= scale;

			//size of decals is scaled up by somewhere between 0% to this value (treated as a percentage)
			//so a value of 0.2 means it's randomly scaled up by 0% - 20% of the base size.
			if( static_ref.size_variation_maximum_increase > 0.0f )
			{
				decal.size = decal.size * (1.0f + static_ref.size_variation_maximum_increase * generate_random_number( 1024 ) / 1024.0f);
			}

			decal.texture = decal_event.texture;
			decal.fade_in_time = decal_event.fade_in_time;

			const simd::vector3 position = check_bone ? client_animation_controller->GetBonePosition( decal_event.bone_index ) : simd::vector3( 0.0f );
			const simd::vector3 world_pos = render_transform.mulpos( position );

			decal.position = Vector2d( world_pos[0], world_pos[1] );

			if( static_ref.jitter )
			{
				const Vector2d jitter( ( generate_random_number( 256 ) / 255.0f ) * 10.0f, ( generate_random_number( 256 ) / 255.0f ) * 10.0f );
				decal.position += jitter;
			}

			decal.flipped = false;

			decal_manager->AddDecal( decal );
		}

		void DecalEvents::SetDecalManager( Decals::DecalManager& decal_manager )
		{
			this->decal_manager = &decal_manager;

			if( !created )
			{
				created = true;
				if( static_ref.create_event )
					CreateDecal( *static_ref.create_event );
			}
		}

		void DecalEventsStatic::RemoveEvent( const DecalEvent& decal_event )
		{
			const auto found = std::find_if( decal_events.begin(), decal_events.end(), [&]( const std::unique_ptr< DecalEvent >& val ) { return val.get() == &decal_event; } );
			if( found == decal_events.end() )
				return;

			parent_type.GetStaticComponent< AnimationControllerStatic >()->RemoveEvent( **found );
			decal_events.erase( found );
		}

		void DecalEventsStatic::AddEvent( const float time, const unsigned animation_index, const Materials::AtlasGroupHandle& texture, const float size, const unsigned bone_index, const float fade_in_time, const std::wstring& filename, const bool scale_with_actor /*= true*/ )
		{
			AddEvent( std::unique_ptr< DecalEvent >( new DecalEvent( time, animation_index, texture, size, bone_index, fade_in_time, filename, scale_with_actor ) ) );
		}

		void DecalEventsStatic::AddEvent( std::unique_ptr< DecalEventsStatic::DecalEvent > decal_event )
		{
			if( auto* animation_controller = parent_type.GetStaticComponent< AnimationControllerStatic >() )
			{
				decal_events.emplace_back( std::move( decal_event ) );

				animation_controller->AddEvent( decal_events.back()->animation_index, *decal_events.back() );
				animation_controller->SortEvents();
			}
		}

		const char* DecalEventsStatic::GetComponentName()
		{
			return "DecalEvents";
		}


		void DecalEventsStatic::CopyAnimationData( const int source_animation, const int destination_animation, std::function<bool( const Animation::Event& )> predicate, std::function<void( const Animation::Event&, Animation::Event& )> transform )
		{
			std::vector<DecalEvent> copied;
			for( auto& e : decal_events )
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
				decal_events.emplace_back( std::make_unique<DecalEvent>( std::move( e ) ) );
				animation_controller->AddEvent( destination_animation, *decal_events.back() );
			}
		}

		DecalEventsStatic::DecalEvent::DecalEvent( const float time, const unsigned animation_index, const Materials::AtlasGroupHandle& texture, const float size, const unsigned bone_index, const float fade_in_time, const std::wstring& filename, const bool scale_with_actor /*= true*/ ) 
			: Event( DecalEventType, animation_index, time )
			, texture( texture )
			, size( size )
			, bone_index( bone_index )
			, fade_in_time( fade_in_time )
			, filename( filename )
			, scale_with_actor( scale_with_actor )
		{

		}

	}

}