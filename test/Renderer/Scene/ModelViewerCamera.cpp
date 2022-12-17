#include "ModelViewerCamera.h"

#ifdef ENABLE_DEBUG_CAMERA

#include "Common/Utility/Profiler/ScopedProfiler.h"
#include "Common/Utility/Geometric.h"

namespace Renderer
{
namespace Scene
{

ModelViewerCamera::ModelViewerCamera( const float max_view_dist ) : width( 0 ), height( 0 ), max_view_dist( max_view_dist )
#ifdef ENABLE_DEBUG_CAMERA
	, doing_pan( false )
#ifdef MOVIE_MODE
	, fov( PIDIV4 )
#endif
#endif
	, prev_shake_delta( 0, 0, 0 )
{
	cameras[ArcballCamera] = new RotationCamera;
	cameras[FirstPersonCamera] = new FPSCamera;
	cameras[MayaOrbitCamera] = new MayaCamera;

	//Set up the camera
	simd::vector3 vecEye( -400.0f, -400.0f, -400.0f ), vecAt( 0.0f, 0.0f, 0.0f ), vecUp( 0.0f, 0.0f, -1.0f );

	// DEFAULT CAMERA
	auto& arcball_camera = GetArcballCamera();
	arcball_camera.SetModelCenter(simd::vector3( 0.0f, 0.0f, 0.0f ) );
	arcball_camera.SetRadius( 500, 10.0f, 10000.0f );
	arcball_camera.SetEnablePositionMovement( true );
	arcball_camera.SetScalers( 0.01f, 2000.0f );
	arcball_camera.SetViewParams( &vecEye, &vecAt, &vecUp );

	// MAYA CAMERA
	auto& maya_camera = GetMayaCamera();
	maya_camera.SetModelCenter(simd::vector3( 0.0f, 0.0f, 0.0f ) );
	maya_camera.SetRadius( 500, 10.0f, 10000.0f );
	maya_camera.SetEnablePositionMovement( true );
	maya_camera.SetViewParams( &vecEye, &vecAt, &vecUp );
	maya_camera.SetScalers( 0.01f, 2000.0f );

	// FPS CAMERA
	auto& fps_camera = GetFirstPersonCamera();
	fps_camera.SetEnablePositionMovement( true );
	fps_camera.SetViewParams(&vecEye, &vecAt, &vecUp );
	fps_camera.SetScalers( 0.01f, 2000.0f );

	Device::Rect r;
	r.left   = LONG_MIN;
	r.top    = LONG_MIN;
	r.right  = LONG_MAX;
	r.bottom = LONG_MAX;
	fps_camera.SetDragRect( r );
}

ModelViewerCamera::~ModelViewerCamera()
{
	delete cameras[ArcballCamera];
	delete cameras[FirstPersonCamera];
	delete cameras[MayaOrbitCamera];
}

void ModelViewerCamera::SetMaxViewDist(const float max_view_dist)
{
	this->max_view_dist = max_view_dist;
	ProjectionChanged();
}

void ModelViewerCamera::WindowChanged( const unsigned int width, const unsigned height )
{
	this->width = width;
	this->height = height;

	Camera::WindowChanged( width, height );

	ProjectionChanged();
	GetArcballCamera().SetWindow( width, height );
	GetMayaCamera().SetWindow( width, height );
}

void ModelViewerCamera::ProjectionChanged()
{
	float aspect_ratio = width / float(height);
#if defined(ENABLE_DEBUG_CAMERA) && defined(MOVIE_MODE)
	for( auto* camera : cameras )
		camera->SetProjParams( fov, aspect_ratio, 75.0f, max_view_dist );
#else
	for( auto* camera : cameras )
		camera->SetProjParams( PI / 4, aspect_ratio, 75.0f, max_view_dist );
#endif
}

void ModelViewerCamera::FrameMove( float elapsed_time )
{
#ifdef ENABLE_DEBUG_CAMERA
		if( doing_pan )
		{
			pan_time_remaining -= elapsed_time;

			auto& camera = GetArcballCamera();
			const simd::quaternion current_q = camera.GetViewQuat();
			const simd::quaternion q = simd::matrix::yawpitchroll( 0.0f, 0.03f * elapsed_time, 0.0f ).rotation();
			const simd::quaternion new_q = current_q * q;
			camera.SetViewQuat( new_q );
			
			if( pan_time_remaining <= 0.0f )
				doing_pan = false;
		}
#endif

	auto& current = GetCurrentCamera();
	current.FrameMove(elapsed_time);
	auto position = *current.GetEyePt();
	auto at = *current.GetLookAtPt();
	auto up = *current.GetUpPt();
	position -= prev_shake_delta;
	prev_shake_delta = position;
	if (camera_mode != FirstPersonCamera && ApplyScreenShakes(position, elapsed_time))
		current.SetViewParams(&position, &at, &up);
	prev_shake_delta -= position;
	UpdateMatrices( *current.GetViewMatrix(), *current.GetProjMatrix(), simd::matrix::identity(), position, *current.GetLookAtPt(), 75.f );
}

LRESULT ModelViewerCamera::HandleMessages( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	LRESULT lresult = 0;

#	ifdef ENABLE_DEBUG_CAMERA
	if( uMsg == WM_KEYDOWN )
	{
		if( wParam == VK_INSERT )
		{
			doing_pan = true;
			pan_time_remaining = 10.0f;
		}
	}

	if( !doing_pan )
#	endif
	{
		lresult = GetCurrentCamera().HandleMessages( hWnd, uMsg, wParam, lParam );
	}

	return lresult;
}

void ModelViewerCamera::SetPositionAndOrientation( simd::vector3 position, simd::vector3 look_at )
{
	simd::vector3 up( 0, 0, -1 );
	GetCurrentCamera().SetViewParams( &position, &look_at, &up );
}

void ModelViewerCamera::SetPositionAndOrientation( simd::vector3 position, simd::vector3 look_at, simd::vector3 up_vec )
{
	GetCurrentCamera().SetViewParams( &position, &look_at, &up_vec );
}

void ModelViewerCamera::SetCameraScalers( const float rotation, const float translation )
{
	GetCurrentCamera().SetScalers( rotation, translation );
}

#if defined(MOVIE_MODE) && defined(ENABLE_DEBUG_CAMERA)
void ModelViewerCamera::SetFOV( const float new_fov )
{
	fov = new_fov;
	const float fAspectRatio = width / float( height );

	for( auto* camera : cameras )
		camera->SetProjParams( fov, fAspectRatio, 1.0f, max_view_dist );

	GetArcballCamera().SetWindow( width, height );
	GetMayaCamera().SetWindow( width, height );
}

void ModelViewerCamera::SetCameraMode( int mode )
{
	auto& old_camera = GetCurrentCamera();
	if( mode >= 0 && mode <= MaxCameras )
		camera_mode = mode;

	auto& new_camera = GetCurrentCamera(); 
	auto eye = simd::vector3( *old_camera.GetEyePt() );
	auto at = simd::vector3( *old_camera.GetLookAtPt() );
	auto up = old_camera.GetUpPt()->normalize();

	new_camera.Reset();
	new_camera.SetViewParams( &eye, &at, &up );
}

void ModelViewerCamera::CycleCameraMode()
{
	auto& old_camera = GetCurrentCamera();

	camera_mode++;
	if( camera_mode == MaxCameras )
		camera_mode = 0;

	auto& new_camera = GetCurrentCamera(); 
	auto eye = *old_camera.GetEyePt();
	auto at = *old_camera.GetLookAtPt();
	auto up = *old_camera.GetUpPt();

	new_camera.SetViewParams( &eye, &at, &up );
}

void ModelViewerCamera::SetEnablePrecisionMovement( const bool b )
{
	for( auto* camera : cameras )
		camera->SetEnablePrecisionMovement( b );
}

bool ModelViewerCamera::IsOrthographicCamera() const
{
	return GetCurrentCamera().IsOrthographicCamera();
}

void ModelViewerCamera::SetOrthographicCamera( const bool enable )
{
	GetCurrentCamera().SetOrthographicCamera( enable );
}

float ModelViewerCamera::GetOrthographicScale() const
{
	return GetCurrentCamera().GetOrthographicScale();
}

#endif

} // namespace Scene
} // namespace Renderer

#endif