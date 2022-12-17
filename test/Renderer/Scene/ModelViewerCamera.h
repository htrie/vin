#pragma once

#include "Deployment/configs/Configuration.h"

#ifdef ENABLE_DEBUG_CAMERA

#include "Visual/Renderer/Scene/Camera.h"
#include "Visual/Renderer/Scene/BaseCamera.h"

namespace Renderer
{
	namespace Scene
	{
		/// Converts a DXUT model viewer camera to a game camera
		class ModelViewerCamera : public Camera
		{
		public:
			ModelViewerCamera( const float max_view_dist = 6000.0f );
			~ModelViewerCamera();

			enum CameraModes
			{
				ArcballCamera,
				FirstPersonCamera,
				MayaOrbitCamera,
				MaxCameras
			};

			bool ForceUseCameraControls() const { return GetCurrentCamera().ForceUseCameraControls(); }

			void WindowChanged( const unsigned int width, const unsigned height );
			void FrameMove( const float elapsed_time );
			void SetCameraScalers( const float rotation, const float translation );

			float GetMaxViewDist() const { return max_view_dist; }
			void SetMaxViewDist(const float max_view_dist);

			void SetPositionAndOrientation( simd::vector3 position, simd::vector3 look_at );
			void SetPositionAndOrientation( simd::vector3 position, simd::vector3 look_at, simd::vector3 up_vec );

			LRESULT HandleMessages(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

#if defined(MOVIE_MODE) && defined(ENABLE_DEBUG_CAMERA)
			float GetFOV() const { return fov; }
			void SetFOV( const float new_fov );
#endif

			void SetCameraMode( int mode );
			void CycleCameraMode();

			CameraModes GetCameraMode() const { return (CameraModes)camera_mode; }
			BaseCamera& GetCurrentCamera() const { return *cameras[camera_mode]; }

			RotationCamera& GetArcballCamera() const { return GetCamera<RotationCamera>( ArcballCamera ); }
			FPSCamera&  GetFirstPersonCamera() const { return GetCamera<FPSCamera>( FirstPersonCamera );  }
			MayaCamera&        GetMayaCamera() const { return GetCamera<MayaCamera>( MayaOrbitCamera );   }

			void SetEnablePrecisionMovement( const bool b );

			using Cameras_t = BaseCamera*[MaxCameras];
			Cameras_t& GetCameras() { return cameras; }

			bool IsOrthographicCamera() const override;
			void SetOrthographicCamera( const bool enable ) override;
			float GetOrthographicScale() const override;

		private:
			void ProjectionChanged();

			template<typename T> T& GetCamera( const int idx ) const { return *static_cast<T*>( cameras[idx] ); }

			Cameras_t cameras;
			int camera_mode = ArcballCamera;

			unsigned width, height;
			float max_view_dist;
			simd::vector3 prev_shake_delta;
#ifdef ENABLE_DEBUG_CAMERA
			bool doing_pan;
			float pan_time_remaining;
#ifdef MOVIE_MODE
			float fov;
#endif
#endif
		};

	} // namespace Scene

} // namespace Renderer

#endif
//EOF
