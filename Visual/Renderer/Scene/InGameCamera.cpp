#include "InGameCamera.h"

namespace Renderer::Scene
{
	void InGameCamera::WindowChanged( const unsigned _width, const unsigned _height )
	{
		Camera::WindowChanged( _width, _height );

		const float aspect_ratio = static_cast<float>( width ) * percentage_width / float( height );

		const simd::matrix projection = IsOrthographicCamera() ?
			simd::matrix::orthographic( static_cast<float>( width ) * orthographic_scale, static_cast<float>( height ) * orthographic_scale, near_plane, far_plane ) :
			simd::matrix::perspectivefov( fovy, aspect_ratio, near_plane, far_plane );

		UpdateMatrices( GetViewMatrix(), projection, post_projection, GetPosition(), GetFocusPoint(), near_plane );
	}

	void InGameCamera::FrameMove( simd::vector3 look_at_pos, simd::vector3 offset, const float frame_time, const simd::matrix* camera_transform )
	{
		const simd::vector3 focus_point = look_at_pos;
		simd::vector3 world_up( 0.0f, 0.0f, -1.0f );
		if( camera_transform )
		{
			simd::vector3 translation, scale;
			simd::matrix rotation;
			camera_transform->decompose( translation, rotation, scale );

			world_up = rotation * world_up;
			offset = rotation * offset;
		}

		if( frame_time != 0.0f )
			ApplyScreenShakes( look_at_pos, frame_time );

		const auto perpen = offset.cross( world_up );
		const auto cam_up = perpen.cross( offset );

		simd::vector3 position = look_at_pos + offset;
#if GGG_CHEATS
		position += custom_offset;
#endif

		const simd::matrix look_at = simd::matrix::lookat( position, look_at_pos, cam_up );
		UpdateMatrices( look_at, GetProjectionMatrix(), post_projection, position, focus_point, near_plane );
	}

	void InGameCamera::Dirty()
	{
		WindowChanged( width, height );
	}

	void InGameCamera::AdjustDoFSettings( simd::vector3 player_position )
	{
		Camera::DoFSettings settings;
		const float distance_camera_to_player = (GetPosition() - player_position).len();
		settings.max_focus_distance = distance_camera_to_player - 100;
		SetDoFSettings(settings);
	}

	void InGameCamera::SetViewableArea( const unsigned _position, const unsigned _width )
	{
		viewable_position = _position;

		percentage_width = _width / float( width );
		const float percentage_position = _position / float( width );

		const simd::matrix scale = simd::matrix::scale(simd::vector3( percentage_width, 1.0f, 1.0f ));
		const simd::matrix translation = simd::matrix::translation(simd::vector3( percentage_position * 2.0f - ( 1.0f - percentage_width ), 0.0f, 0.0f ));
		post_projection = scale * translation;

		const float aspect_ratio = width * percentage_width / float( height );

		const simd::matrix projection = IsOrthographicCamera() ?
			simd::matrix::orthographic( static_cast< float >( width ) * orthographic_scale, static_cast< float >( height ) * orthographic_scale, near_plane, far_plane ) :
			simd::matrix::perspectivefov( fovy, aspect_ratio, near_plane, far_plane );

		UpdateMatrices( GetViewMatrix(), projection, post_projection, GetPosition(), GetFocusPoint(), near_plane );
	}

	void InGameCamera::SetViewableArea( const unsigned x, const unsigned y, const unsigned _width, const unsigned _height )
	{
		viewable_position = x;

		percentage_width = _width / float( width );
		percentage_height = _height / float( height );
		const float percentage_x = x / float( width );
		const float percentage_y = y / float( height );

		const simd::matrix scale = simd::matrix::scale(simd::vector3( percentage_width, 1.0f, 1.0f ));
		const simd::matrix translation = simd::matrix::translation(simd::vector3( percentage_x * 2.0f - ( 1.0f - percentage_width ), percentage_y * 2.0f - ( 1.0f - percentage_height ), 0.0f ));
		post_projection = scale * translation;

		const float aspect_ratio = width * percentage_width / float( height );

		const simd::matrix projection = IsOrthographicCamera() ?
			simd::matrix::orthographic( static_cast< float >( width ) * orthographic_scale, static_cast< float >( height ) * orthographic_scale, near_plane, far_plane ) :
			simd::matrix::perspectivefov( fovy, aspect_ratio, near_plane, far_plane );

		UpdateMatrices( GetViewMatrix(), projection, post_projection, GetPosition(), GetFocusPoint(), near_plane );
	}

	InGameCamera::InGameCamera( const simd::vector3 offset, const float fovy, const float near_plane, const float far_plane ) 
		: fovy( fovy )
		, near_plane( near_plane ), far_plane( far_plane )
		, offset( offset )
		, default_offset( offset )
		, post_projection( simd::matrix::identity() )
	{
		assert(near_plane != far_plane);
		width = height = 1;
		FrameMove( simd::vector3( 0.f, 0.f, 0.f ), offset, 0.0f );
	}

	void InGameCamera::SetNearFarPlanes( const float near_plane, const float far_plane )
	{
		this->near_plane = near_plane;
		this->far_plane = (far_plane == near_plane) ? near_plane + 0.001f : far_plane;

#if GGG_CHEATS
		if( far_plane_override )
			this->far_plane = far_plane_override;
#endif

		Dirty();
	}

	void InGameCamera::SetZRotationOffset( const float z_rot )
	{
		const float new_rot = zrotation_enabled ? z_rot : 0.0f;
		//if( new_rot != this->z_rot ) //this doesn't work!
		{
			this->z_rot = new_rot; //Storing for use with input
			
			const simd::matrix rotation = simd::matrix::rotationZ( new_rot );
			offset = rotation * default_offset;

			Dirty();
		}
	}

	std::pair< simd::vector3, simd::vector3 > InGameCamera::ScreenPositionToRay( const int sx, const int sy ) const
	{	
		return Camera::ScreenPositionToRay( static_cast< int >( ( static_cast< float >( sx ) - viewable_position ) / percentage_width ), sy );
	}

	void InGameCamera::SetOrthographicCamera( const bool enable )
	{
		this->orthographic_camera = enable;
		Dirty();
	}

	void InGameCamera::SetOrthographicScale( const float orthographic_scale )
	{
		this->orthographic_scale = orthographic_scale;
		Dirty();
	}

#ifdef MOVIE_MODE
	void InGameCamera::SetFOV( const float new_fov )
	{
		fovy = new_fov;
		Dirty();
	}
#endif

}
