#pragma once

#include "Camera.h"

namespace Renderer::Scene
{
		///Camera used in the game. Views the scene at an offset from some position.
		class InGameCamera : public Camera
		{
		public:
			InGameCamera( const simd::vector3 offset, const float fovy, const float near_plane, const float far_plane );

			virtual void SetNearFarPlanes( const float near_plane, const float far_plane ) override;
			void WindowChanged( const unsigned width, const unsigned height );
			void FrameMove( simd::vector3 look_at_pos, simd::vector3 offset, const float frame_time, const simd::matrix* camera_transform = nullptr );

			const float GetZRotation() const { return z_rot; }
			const simd::vector3& GetOffset() const { return offset; }
			void SetZRotationOffset( const float z_rot ) override;

			void Dirty();

			void AdjustDoFSettings( simd::vector3 player_position );

#ifdef MOVIE_MODE
			void SetFOV( const float new_fov );
			float GetFOV() const { return fovy; }
#endif

#if GGG_CHEATS
			simd::vector3 custom_offset = { 0,0,0 };
			float far_plane_override = 0;
#endif

			///Changes the horizontal portion of the screen that is viewed by the camera
			///@param position and width are in pixels
			void SetViewableArea( const unsigned position, const unsigned width );
			void SetViewableArea( const unsigned x, const unsigned y, const unsigned width, const unsigned height );

			virtual std::pair< simd::vector3, simd::vector3 > ScreenPositionToRay( const int sx, const int sy ) const override;

			bool IsOrthographicCamera() const override { return orthographic_camera; }
			void SetOrthographicCamera( const bool enable ) override;
			float GetOrthographicScale() const override { return orthographic_scale; }
			void SetOrthographicScale( const float orthographic_scale );

		private:

			const simd::vector3 default_offset;
			simd::vector3 offset; //offset of camera eye from look-at point
			simd::matrix post_projection;
			float fovy = 1.0f, z_rot = 0.0f;
			float near_plane = 0.0f, far_plane = 0.0f;

			///Used so that the viewable area can be smaller then the total screen size
			float percentage_width = 1.0f, percentage_height = 1.0f;
			unsigned viewable_position = 0;

			bool orthographic_camera = false;
			float orthographic_scale = 1.0f;
		};

}
