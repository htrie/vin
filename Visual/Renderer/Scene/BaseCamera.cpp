
#include "Common/Utility/ContainerOperations.h"
#include "Common/Utility/Geometric.h"

#include "Visual/Device/WindowDX.h"
#include "Visual/Device/Device.h"
#include "Visual/Device/Texture.h"
#include "Visual/Device/DeviceInfo.h"
#include "Visual/UI/UISystem.h"
#include "Visual/GUI2/imgui/imgui.h"
#include "Visual/Renderer/RendererSubsystem.h"

#include "BaseCamera.h"


constexpr float max(float a, float b) { return a > b ? a : b; }
constexpr float min(float a, float b) { return a < b ? a : b; }

BaseCamera::OrthoViewMode BaseCamera::ortho_viewmode = BaseCamera::OrthoViewMode::None;
std::string BaseCamera::ortho_viewmodes[BaseCamera::OrthoViewMode::NumOrthoViewMode] =
{
	"None",
	"Side",
	"Top"
};

namespace
{
	bool LinePlaneIntersection( simd::vector3& contact, simd::vector3 ray, simd::vector3 rayOrigin, simd::vector3 normal, simd::vector3 coord )
	{
		// get d value
		float d = normal.dot( coord );

		if( normal.dot( ray ) == 0 )
			return false; // No intersection, the line is parallel to the plane

		// Compute the X value for the directed line ray intersecting the plane
		float x = ( d - normal.dot( rayOrigin ) ) / normal.dot( ray );

		// output contact point
		contact = rayOrigin + ray.normalize() * x; //Make sure your ray vector is normalized
		return true;
	}
}

//--------------------------------------------------------------------------------------
ArcBall::ArcBall()
{
	Reset();
	m_vDownPt = simd::vector3(0, 0, 0);
	m_vCurrentPt = simd::vector3(0, 0, 0);
	m_Offset.x = m_Offset.y = 0;

	Device::Rect rc;
	Device::WindowDX::GetClientRect(Device::WindowDX::GetForegroundWindow(), &rc);
	SetWindow(rc.right, rc.bottom);
}





//--------------------------------------------------------------------------------------
void ArcBall::Reset()
{
	m_qDown = simd::quaternion::identity();
	m_qNow = simd::quaternion::identity();
	m_mRotation = simd::matrix::identity();
	m_mTranslation = simd::matrix::identity();
	m_mTranslationDelta = simd::matrix::identity();
	m_bDrag = FALSE;
	m_fRadiusTranslation = 1.0f;
	m_fRadius = 1.0f;
}




//--------------------------------------------------------------------------------------
simd::vector3 ArcBall::ScreenToVector(float fScreenPtX, float fScreenPtY)
{
	// Scale to screen
	FLOAT x = -(fScreenPtX - m_Offset.x - m_nWidth / 2) / (m_fRadius * m_nWidth / 2);
	FLOAT y = (fScreenPtY - m_Offset.y - m_nHeight / 2) / (m_fRadius * m_nHeight / 2);

	FLOAT z = 0.0f;
	FLOAT mag = x * x + y * y;

	if (mag > 1.0f)
	{
		FLOAT scale = 1.0f / sqrtf(mag);
		x *= scale;
		y *= scale;
	}
	else
		z = sqrtf(1.0f - mag);

	// Return vector
	return simd::vector3(x, y, z);
}




//--------------------------------------------------------------------------------------
simd::quaternion ArcBall::QuatFromBallPoints(const simd::vector3& vFrom, const simd::vector3& vTo)
{
	return simd::quaternion(vFrom.cross(vTo), vFrom.dot(vTo));
}




//--------------------------------------------------------------------------------------
void ArcBall::OnBegin(int nX, int nY)
{
	// Only enter the drag state if the click falls
	// inside the click rectangle.
	if (nX >= m_Offset.x &&
		nX < m_Offset.x + m_nWidth &&
		nY >= m_Offset.y &&
		nY < m_Offset.y + m_nHeight)
	{
		m_bDrag = true;
		m_qDown = m_qNow;
		m_vDownPt = ScreenToVector((float)nX, (float)nY);
	}
}




//--------------------------------------------------------------------------------------
void ArcBall::OnMove(int nX, int nY)
{
	if (m_bDrag)
	{
		m_vCurrentPt = ScreenToVector((float)nX, (float)nY);
		m_qNow = m_qDown * QuatFromBallPoints(m_vDownPt, m_vCurrentPt);
	}
}




//--------------------------------------------------------------------------------------
void ArcBall::OnEnd()
{
	m_bDrag = false;
}




//--------------------------------------------------------------------------------------
// Desc:
//--------------------------------------------------------------------------------------
LRESULT ArcBall::HandleMessages(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	// Current mouse position
	int iMouseX = (short)LOWORD(lParam);
	int iMouseY = (short)HIWORD(lParam);

	Device::WindowDX* window = Device::WindowDX::GetWindow();

	switch (uMsg)
	{
	case WM_LBUTTONDOWN:
	case WM_LBUTTONDBLCLK:
		window->SetCapture();
		OnBegin(iMouseX, iMouseY);
		return TRUE;

	case WM_LBUTTONUP:
		window->ReleaseCapture();
		OnEnd();
		return TRUE;
	case WM_CAPTURECHANGED:
		if ((HWND)lParam != hWnd)
		{
			window->ReleaseCapture();
			OnEnd();
		}
		return TRUE;

	case WM_RBUTTONDOWN:
	case WM_RBUTTONDBLCLK:
	case WM_MBUTTONDOWN:
	case WM_MBUTTONDBLCLK:
		window->SetCapture();
		// Store off the position of the cursor when the button is pressed
		m_ptLastMouse.x = iMouseX;
		m_ptLastMouse.y = iMouseY;
		return TRUE;

	case WM_RBUTTONUP:
	case WM_MBUTTONUP:
		window->ReleaseCapture();
		return TRUE;

	case WM_MOUSEMOVE:
		if (MK_LBUTTON & wParam)
		{
			OnMove(iMouseX, iMouseY);
		}
		else if ((MK_RBUTTON & wParam) || (MK_MBUTTON & wParam))
		{
			// Normalize based on size of window and bounding sphere radius
			const FLOAT fDeltaX = (m_ptLastMouse.x - iMouseX) * m_fRadiusTranslation / m_nWidth;
			const FLOAT fDeltaY = (m_ptLastMouse.y - iMouseY) * m_fRadiusTranslation / m_nHeight;

			if (wParam & MK_RBUTTON)
			{
				m_mTranslationDelta = simd::matrix::translation(simd::vector3(-2.0f * fDeltaX, 2.0f * fDeltaY, 0.0f));
			}
			else  // wParam & MK_MBUTTON
			{
				m_mTranslationDelta = simd::matrix::translation(simd::vector3(0.0f, 0.0f, 5 * fDeltaY));
			}

			m_mTranslation = m_mTranslation * m_mTranslationDelta;

			// Store mouse coordinate
			m_ptLastMouse.x = iMouseX;
			m_ptLastMouse.y = iMouseY;
		}
		return TRUE;
	}

	return FALSE;
}


//--------------------------------------------------------------------------------------
// Constructor
//--------------------------------------------------------------------------------------
BaseCamera::BaseCamera()
{
#if GIZMO_ENABLED
	for( auto& g : camera_gizmos )
		g.AddListener( this );
#endif

	code_to_cam_key = {
		{ VK_CONTROL,	CAM_KEYS_CONTROLDOWN },
		{ VK_HOME,		CAM_KEYS_RESET },

		{ VK_A,			CAM_KEYS_STRAFE_LEFT },
		{ VK_D,			CAM_KEYS_STRAFE_RIGHT },
		{ VK_W,			CAM_KEYS_MOVE_FORWARD },
		{ VK_S,			CAM_KEYS_MOVE_BACKWARD },
		{ VK_E,			CAM_KEYS_MOVE_UP },
		{ VK_Q,			CAM_KEYS_MOVE_DOWN },

		{ VK_LEFT,		CAM_KEYS_STRAFE_LEFT },
		{ VK_RIGHT,		CAM_KEYS_STRAFE_RIGHT },
		{ VK_UP,		CAM_KEYS_MOVE_FORWARD },
		{ VK_DOWN,		CAM_KEYS_MOVE_BACKWARD },
		{ VK_PRIOR,		CAM_KEYS_MOVE_UP },
		{ VK_NEXT,		CAM_KEYS_MOVE_DOWN },
	};

	m_cKeysDown = 0;

	// Set attributes for the view matrix
	auto vEyePt = simd::vector3( 0.0f, 0.0f, 0.0f );
	auto vLookatPt = simd::vector3( 0.0f, 0.0f, 1.0f );
	auto vUpPt = simd::vector3( 0.0f, 1.0f, 0.0f );

	// Setup the view matrix
	SetViewParams( &vEyePt, &vLookatPt, &vUpPt );

	// Setup the projection matrix
	SetProjParams(PIDIV4, 1.0f, 1.0f, 1000.0f);

	Device::WindowDX* window = Device::WindowDX::GetWindow();
	if (window)
	{
		window->GetCursorPos(&m_ptLastMousePosition);
	}

	m_bMouseLButtonDown = false;
	m_bMouseMButtonDown = false;
	m_bMouseRButtonDown = false;
	m_nCurrentButtonMask = 0;
	m_nMouseWheelDelta = 0;

	m_bEnablePrecisionMovement = false;

	m_fCameraYawAngle = 0.0f;
	m_fCameraPitchAngle = 0.0f;

	if (window)
	{
		window->SetRect(&m_rcDrag, LONG_MIN, LONG_MIN, LONG_MAX, LONG_MAX);
	}

	m_vVelocity = simd::vector3(0, 0, 0);
	m_bMovementDrag = false;
	m_vVelocityDrag = simd::vector3(0, 0, 0);
	m_fDragTimer = 0.0f;
	m_fTotalDragTimeToZero = 0.25;
	m_vRotVelocity = simd::vector2(0, 0);

	m_fRotationScaler = 0.01f;
	m_fMoveScaler = 5.0f;

	m_bInvertPitch = false;
	m_bEnableYAxisMovement = true;
	m_bEnablePositionMovement = true;

	m_vMouseDelta = simd::vector2(0, 0);
	m_fFramesToSmoothMouseData = 2.0f;

	m_bClipToBoundary = false;
	m_vMinBoundary = simd::vector3(-1, -1, -1);
	m_vMaxBoundary = simd::vector3(1, 1, 1);

	m_bResetCursorAfterMove = false;
}


BaseCamera::~BaseCamera()
{
#if GIZMO_ENABLED
	for( auto& g : camera_gizmos )
		g.RemoveListener( this );
#endif
}


//--------------------------------------------------------------------------------------
// Client can call this to change the position and direction of camera
//--------------------------------------------------------------------------------------
VOID BaseCamera::SetViewParams(simd::vector3* pvEyePt, simd::vector3* pvLookatPt, simd::vector3* pvUpPt )
{
	if( NULL == pvEyePt || NULL == pvLookatPt || NULL == pvUpPt )
		return;

	m_vDefaultEye = m_vEye = *pvEyePt;
	m_vDefaultLookAt = m_vLookAt = *pvLookatPt;
	m_vDefaultUp = m_vUp = *pvUpPt;

	// Calc the view matrix
	m_mView = simd::matrix::lookat( *pvEyePt, *pvLookatPt, *pvUpPt );

	simd::matrix mInvView = m_mView.inverse();

	// The axis basis vectors and camera position are stored inside the 
	// position matrix in the 4 rows of the camera's world matrix.
	// To figure out the yaw/pitch of the camera, we just need the Z basis vector
	const simd::vector3* pZBasis = (simd::vector3*)&mInvView[2][0];

	m_fCameraYawAngle = atan2f(pZBasis->x, pZBasis->z);
	float fLen = sqrtf(pZBasis->z * pZBasis->z + pZBasis->x * pZBasis->x);
	m_fCameraPitchAngle = -atan2f(pZBasis->y, fLen);
	Dirty();
}




//--------------------------------------------------------------------------------------
// Calculates the projection matrix based on input params
//--------------------------------------------------------------------------------------
VOID BaseCamera::SetProjParams(FLOAT fFOV, FLOAT fAspect, FLOAT fNearPlane,
	FLOAT fFarPlane)
{
	// Set attributes for the projection matrix
	m_fFOV = fFOV;
	m_fAspect = fAspect;
	m_fNearPlane = fNearPlane;
	m_fFarPlane = fFarPlane;

	if( is_orthographic_camera )
	{
		float w, h;
		std::tie( w, h ) = GetOrthographicWidthHeight();
		m_mProj = simd::matrix::orthographic( w, h, m_fNearPlane, m_fFarPlane );
	}
	else
	{
		m_mProj = simd::matrix::perspectivefov( fFOV, fAspect, fNearPlane, fFarPlane );
	}
}

//--------------------------------------------------------------------------------------
// Call this from your message proc so this class can handle window messages
//--------------------------------------------------------------------------------------
LRESULT BaseCamera::HandleMessages(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	(void)hWnd;
	(void)lParam;

	switch (uMsg)
	{
	case WM_KEYDOWN:
	{
		if( wParam >= std::size( key_down ) || key_down[ wParam ] )
			break;
		
		key_down[ wParam ] = true;

		auto it = code_to_cam_key.find((int)wParam);
		if (it != code_to_cam_key.end())
		{
			if( cam_key_down[ it->second ]++ == 0 )
				++m_cKeysDown;
		}
		break;
	}

	case WM_KEYUP:
	{
		if( wParam >= std::size( key_down ) || !key_down[ wParam ] )
			break;

		key_down[ wParam ] = false;

		auto it = code_to_cam_key.find((int)wParam);
		if (it != code_to_cam_key.end())
		{
			if( --cam_key_down[ it->second ] == 0 )
				--m_cKeysDown;
		}
		break;
	}

	case WM_RBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDBLCLK:
	case WM_MBUTTONDBLCLK:
	case WM_LBUTTONDBLCLK:
	{
		// Compute the drag rectangle in screen coord.
		Device::Point ptCursor =
		{
			(short)LOWORD(lParam), (short)HIWORD(lParam)
		};

		// Update member var state
		Device::WindowDX* window = Device::WindowDX::GetWindow();
		bool is_point_in_rect = window->IsPointInRect(&m_rcDrag, ptCursor);
		if ((uMsg == WM_LBUTTONDOWN || uMsg == WM_LBUTTONDBLCLK) && is_point_in_rect)
		{
			m_bMouseLButtonDown = true; m_nCurrentButtonMask |= MOUSE_LEFT_BUTTON;
		}
		if ((uMsg == WM_MBUTTONDOWN || uMsg == WM_MBUTTONDBLCLK) && is_point_in_rect)
		{
			m_bMouseMButtonDown = true; m_nCurrentButtonMask |= MOUSE_MIDDLE_BUTTON;
		}
		if ((uMsg == WM_RBUTTONDOWN || uMsg == WM_RBUTTONDBLCLK) && is_point_in_rect)
		{
			m_bMouseRButtonDown = true; m_nCurrentButtonMask |= MOUSE_RIGHT_BUTTON;
		}

		// Capture the mouse, so if the mouse button is 
		// released outside the window, we'll get the WM_LBUTTONUP message
		window->SetCapture();
		window->GetCursorPos(&m_ptLastMousePosition);

		return TRUE;
	}

	case WM_RBUTTONUP:
	case WM_MBUTTONUP:
	case WM_LBUTTONUP:
	{
		// Update member var state
		if (uMsg == WM_LBUTTONUP)
		{
			m_bMouseLButtonDown = false; m_nCurrentButtonMask &= ~MOUSE_LEFT_BUTTON;
		}
		if (uMsg == WM_MBUTTONUP)
		{
			m_bMouseMButtonDown = false; m_nCurrentButtonMask &= ~MOUSE_MIDDLE_BUTTON;
		}
		if (uMsg == WM_RBUTTONUP)
		{
			m_bMouseRButtonDown = false; m_nCurrentButtonMask &= ~MOUSE_RIGHT_BUTTON;
		}

		// Release the capture if no mouse buttons down
		if (!m_bMouseLButtonDown &&
			!m_bMouseRButtonDown &&
			!m_bMouseMButtonDown)
		{
			Device::WindowDX* window = Device::WindowDX::GetWindow();
			window->ReleaseCapture();
		}
		break;
	}

	case WM_CAPTURECHANGED:
	{
		if ((HWND)lParam != hWnd)
		{
			if ((m_nCurrentButtonMask & MOUSE_LEFT_BUTTON) ||
				(m_nCurrentButtonMask & MOUSE_MIDDLE_BUTTON) ||
				(m_nCurrentButtonMask & MOUSE_RIGHT_BUTTON))
			{
				m_bMouseLButtonDown = false;
				m_bMouseMButtonDown = false;
				m_bMouseRButtonDown = false;
				m_nCurrentButtonMask &= ~MOUSE_LEFT_BUTTON;
				m_nCurrentButtonMask &= ~MOUSE_MIDDLE_BUTTON;
				m_nCurrentButtonMask &= ~MOUSE_RIGHT_BUTTON;
				Device::WindowDX* window = Device::WindowDX::GetWindow();
				window->ReleaseCapture();
			}
		}
		break;
	}

	case WM_MOUSEWHEEL:
		// Update member var state
		m_nMouseWheelDelta += (short)HIWORD(wParam);
		break;
	}

	return FALSE;
}

//--------------------------------------------------------------------------------------
// Figure out the velocity based on keyboard input & drag if any
//--------------------------------------------------------------------------------------
void BaseCamera::GetInput(bool bGetKeyboardInput, bool bGetMouseInput, bool bGetGamepadInput,
	bool bResetCursorAfterMove)
{
	m_vKeyboardDirection = simd::vector3(0, 0, 0);
	if (bGetKeyboardInput)
	{
		// Update acceleration vector based on keyboard state
		if (IsCamKeyDown(CAM_KEYS_MOVE_FORWARD))
			m_vKeyboardDirection.z += 1.0f;
		if (IsCamKeyDown(CAM_KEYS_MOVE_BACKWARD))
			m_vKeyboardDirection.z -= 1.0f;
		if (m_bEnableYAxisMovement)
		{
			if (IsCamKeyDown(CAM_KEYS_MOVE_UP))
				m_vKeyboardDirection.y += 1.0f;
			if (IsCamKeyDown(CAM_KEYS_MOVE_DOWN))
				m_vKeyboardDirection.y -= 1.0f;
		}
		if (IsCamKeyDown(CAM_KEYS_STRAFE_RIGHT))
			m_vKeyboardDirection.x += 1.0f;
		if (IsCamKeyDown(CAM_KEYS_STRAFE_LEFT))
			m_vKeyboardDirection.x -= 1.0f;
	}

	if (bGetMouseInput)
	{
		UpdateMouseDelta();
	}
}


//--------------------------------------------------------------------------------------
// Figure out the mouse delta based on mouse movement
//--------------------------------------------------------------------------------------
void BaseCamera::UpdateMouseDelta()
{

	// Get current position of mouse
	auto* window = Device::WindowDX::GetWindow();
	window->GetCursorPos(&m_ptCurMousePos);
	// Calc how far it's moved since last frame
	m_ptCurMouseDelta.x = m_ptCurMousePos.x - m_ptLastMousePosition.x;
	m_ptCurMouseDelta.y = m_ptCurMousePos.y - m_ptLastMousePosition.y;

	// Record current position for next time
	m_ptLastMousePosition = m_ptCurMousePos;

	if (m_bResetCursorAfterMove)
	{
		// Set position of camera to center of desktop, 
		// so it always has room to move.  This is very useful
		// if the cursor is hidden.  If this isn't done and cursor is hidden, 
		// then invisible cursor will hit the edge of the screen 
		// and the user can't tell what happened
		Device::Point ptCenter;

		// Get the center of the current monitor
#if !defined( CONSOLE ) && !defined( __APPLE__ ) && !defined( ANDROID )
		Device::MonitorInfo mi;
		mi.cbSize = sizeof(Device::MonitorInfo);

		HWND hwnd = window->GetWnd();
		window->FetchMonitorInfo(MONITOR_DEFAULTTONEAREST, &mi);

		ptCenter.x = (mi.rcMonitor.left + mi.rcMonitor.right) / 2;
		ptCenter.y = (mi.rcMonitor.top + mi.rcMonitor.bottom) / 2;
#else
		if (auto* device = Renderer::System::Get().GetDevice())
		{
			ptCenter.x = device->GetBackBufferWidth() / 2.0f;
			ptCenter.y = device->GetBackBufferHeight() / 2.0f;
		}
#endif
		window->SetCursorPos(ptCenter.x, ptCenter.y);
		m_ptLastMousePosition = ptCenter;
	}

	// Smooth the relative mouse data over a few frames so it isn't 
	// jerky when moving slowly at low frame rates.
	float fPercentOfNew = 1.0f / m_fFramesToSmoothMouseData;
	float fPercentOfOld = 1.0f - fPercentOfNew;
	m_vMouseDelta.x = m_vMouseDelta.x * fPercentOfOld + m_ptCurMouseDelta.x * fPercentOfNew;
	m_vMouseDelta.y = m_vMouseDelta.y * fPercentOfOld + m_ptCurMouseDelta.y * fPercentOfNew;

	m_vRotVelocity = m_vMouseDelta * m_fRotationScaler;
}




//--------------------------------------------------------------------------------------
// Figure out the velocity based on keyboard input & drag if any
//--------------------------------------------------------------------------------------
void BaseCamera::UpdateVelocity(float fElapsedTime)
{
	simd::matrix mRotDelta;
	m_vRotVelocity = m_vMouseDelta * m_fRotationScaler;
	simd::vector3 vAccel = m_vKeyboardDirection;

	// Normalize vector so if moving 2 dirs (left & forward), 
	// the camera doesn't move faster than if moving in 1 dir
	vAccel = vAccel.normalize();

	float precision_mult = 1.0f;

#if !defined( CONSOLE ) && !defined( __APPLE__ ) && !defined( ANDROID )
	if ( m_bEnablePrecisionMovement )
	{
		precision_mult = (GetAsyncKeyState( VK_SHIFT ) & 0x8000) ? 0.1f : 1.0f;// IsCamKeyDown(VK_SHIFT) ? 0.1f : 1.0f;
	}
#endif

	// Scale the acceleration vector
	vAccel = vAccel * m_fMoveScaler * precision_mult;

	if (m_bMovementDrag)
	{
		// Is there any acceleration this frame?
		if (vAccel.sqrlen() > 0) // can be optimised, maybe a _mm_sub with a ble 0 ?
		{
			// If so, then this means the user has pressed a movement key
			// so change the velocity immediately to acceleration 
			// upon keyboard input.  This isn't normal physics
			// but it will give a quick response to keyboard input
			m_vVelocity = vAccel;
			m_vVelocityDrag = vAccel / m_fDragTimer;
			m_fDragTimer = m_fTotalDragTimeToZero;
		}
		else
		{
			// If no key being pressed, then slowly decrease velocity to 0
			if (m_fDragTimer > 0)
			{
				// Drag until timer is <= 0
				m_vVelocity -= m_vVelocityDrag * fElapsedTime;

				m_fDragTimer -= fElapsedTime;
			}
			else
			{
				// Zero velocity
				m_vVelocity = simd::vector3(0, 0, 0);
			}
		}
	}
	else
	{
		// No drag, so immediately change the velocity
		m_vVelocity = vAccel;
	}
}




//--------------------------------------------------------------------------------------
// Clamps pV to lie inside m_vMinBoundary & m_vMaxBoundary
//--------------------------------------------------------------------------------------
void BaseCamera::ConstrainToBoundary(simd::vector3* pV)
{
	// Constrain vector to a bounding box 
	pV->x = max(pV->x, m_vMinBoundary.x);
	pV->y = max(pV->y, m_vMinBoundary.y);
	pV->z = max(pV->z, m_vMinBoundary.z);

	pV->x = min(pV->x, m_vMaxBoundary.x);
	pV->y = min(pV->y, m_vMaxBoundary.y);
	pV->z = min(pV->z, m_vMaxBoundary.z);
}




//--------------------------------------------------------------------------------------
// Reset the camera's position back to the default
//--------------------------------------------------------------------------------------
VOID BaseCamera::Reset()
{
	SetViewParams( &m_vDefaultEye, &m_vDefaultLookAt, &m_vDefaultUp );
	orthographic_scale = 1.0f;
}



void BaseCamera::Translate( const simd::vector3& delta )
{
	m_vEye += delta;
	m_vLookAt += delta;
	Dirty();
}

void BaseCamera::FrameMove( float fElapsedTime )
{
#if GIZMO_ENABLED
	for( auto& g : camera_gizmos )
		g.Refresh();
#endif

#if !defined( CONSOLE ) && !defined( __APPLE__ ) && !defined( ANDROID )
	if( m_cKeysDown )
	{
		if( ImGui::GetCurrentContext() && ( ImGui::GetIO().WantTextInput || !ImmediateMode && ImGui::GetIO().KeyAlt ) || !Device::WindowDX::GetWindow()->IsActive() )
		{
			Fill( key_down, false );
			Fill( cam_key_down, 0 );
			m_cKeysDown = 0;
		}
	}

	if( ImmediateMode )
	{
		for( const auto& [code, cam_key] : code_to_cam_key )
		{
			if( ( GetAsyncKeyState( code ) & 0x8000 ) == 0 )
			{
				if( !key_down[ code ] )
					continue;

				key_down[ code ] = false;
				if( --cam_key_down[ cam_key ] == 0 )
					--m_cKeysDown;
			}
		}
	}
#endif
}

void BaseCamera::OnRender()
{
#if GIZMO_ENABLED
	for( auto& g : camera_gizmos )
		g.OnRender( *this );
#endif
}

#if GIZMO_ENABLED
simd::vector3 BaseCamera::GetGizmoPosition( const Gizmo& gizmo ) const
{
	auto look_at = *GetLookAtPt();
	if( &gizmo == &camera_gizmos[Gizmos::LookAt] )
		return look_at;
	return { look_at.x, look_at.y, 0.0f };
}

unsigned BaseCamera::GetPattern( const Gizmo& gizmo ) const
{
	return &gizmo == &camera_gizmos[Gizmos::LookAt] ? 0xffffffff : 0xff0ff0ff;
}

bool BaseCamera::FlipYDirection( const Gizmo& gizmo ) const
{
	return gizmo.GetPosition().z > 0.0f;
}

bool BaseCamera::IsGizmoVisible( const Gizmo& gizmo ) const
{
	return IsGizmoEnabled;
}

bool BaseCamera::CanGizmoDrag( const Gizmo& gizmo ) const
{
	return false;
}

int BaseCamera::GetGizmoAxes( const Gizmo& gizmo ) const
{
	if( &gizmo == &camera_gizmos[Gizmos::Ground] )
		return Utility::Axes::XY;
	return Utility::Axes::XYZ;
}

float BaseCamera::GetLineLength( const Gizmo& gizmo ) const
{
	return 5.0f;
}

simd::color BaseCamera::GetColour( const Gizmo& gizmo ) const
{
	if( &gizmo == &camera_gizmos[Gizmos::Ground] )
		return simd::color::argb( 200, 200, 200, 200 );
	return simd::color::argb( 255, 255, 255, 255 );
}

#endif

void BaseCamera::SetOrthographicCamera( const bool enable )
{
	is_orthographic_camera = enable;
	Dirty();
}

std::pair<float, float> BaseCamera::GetOrthographicWidthHeight() const
{
	if (auto* device = Renderer::System::Get().GetDevice())
	{
		const float orthographic_scale = GetOrthographicScale();
		return { (float)device->GetBackBufferWidth() * orthographic_scale, (float)device->GetBackBufferHeight() * orthographic_scale };
	}
	return { 0.0f, 0.0f };
}

#if defined(MOVIE_MODE) && defined(ENABLE_DEBUG_CAMERA)
void BaseCamera::SetFOV( const float new_fov )
{
	SetProjParams( new_fov, m_fAspect, m_fNearPlane, m_fFarPlane );
}
#endif

//--------------------------------------------------------------------------------------
// Constructor
//--------------------------------------------------------------------------------------
FPSCamera::FPSCamera()
	: m_nActiveButtonMask( MOUSE_RIGHT_BUTTON )
{
}




void FPSCamera::Dirty()
{
	dirty = true;
}

//--------------------------------------------------------------------------------------
// Update the view matrix based on user input & elapsed time
//--------------------------------------------------------------------------------------
VOID FPSCamera::FrameMove(FLOAT fElapsedTime)
{
	auto* window = Device::WindowDX::GetWindow();
	if (!window)
		return;

	auto* device = Renderer::System::Get().GetDevice();
	if (!device)
		return;

	if (device->TimePaused())
	{
		float fps = device->GetFPS();
		if (fps == 0.0f) fElapsedTime = 0;
		else fElapsedTime = 1.0f / fps;
	}

	BaseCamera::FrameMove( fElapsedTime );

	if (IsCamKeyDown(CAM_KEYS_RESET))
		Reset();

	// Get keyboard/mouse/gamepad input
	GetInput( m_bEnablePositionMovement, ( m_nActiveButtonMask & m_nCurrentButtonMask ), true, m_bResetCursorAfterMove );

	//// Get the mouse movement (if any) if the mouse button are down
	//if( (m_nActiveButtonMask & m_nCurrentButtonMask) || m_bRotateWithoutButtonDown )
	//    UpdateMouseDelta( fElapsedTime );

	// Get amount of velocity based on the keyboard input and drag (if any)
	UpdateVelocity(fElapsedTime);

	// Simple euler method to calculate position delta
	simd::vector3 vPosDelta(m_vVelocity * fElapsedTime);
	if( IsOrthographicCamera() )
	{
		orthographic_scale = Clamp( orthographic_scale - vPosDelta.z * 0.0005f, 0.01f, 100.0f );
		vPosDelta.z = 0.0f;
	}

	// If rotating the camera 
	if( ( m_nActiveButtonMask & m_nCurrentButtonMask ) || dirty )
	{
		// Update the pitch & yaw angle based on mouse movement
		float fYawDelta = m_vRotVelocity.x;
		float fPitchDelta = m_vRotVelocity.y;

		// Invert pitch if requested
		if (m_bInvertPitch)
			fPitchDelta = -fPitchDelta;

		m_fCameraPitchAngle += fPitchDelta;
		m_fCameraYawAngle += fYawDelta;

		// Limit pitch to straight up or straight down
		m_fCameraPitchAngle = max(-PI, m_fCameraPitchAngle);
		m_fCameraPitchAngle = min(0, m_fCameraPitchAngle);
	}

	simd::matrix mCameraRot;
	const auto x_rot = simd::matrix::rotationX(m_fCameraPitchAngle);
	const auto y_rot = simd::matrix::rotationY(0.0f);
	const auto z_rot = simd::matrix::rotationZ(-m_fCameraYawAngle);

	const auto combining = x_rot * z_rot;
	mCameraRot = combining * y_rot;

	const float look_dist = ( m_vEye - m_vLookAt ).len();
	const simd::vector3 forward( 0, 0, look_dist );
	const auto forward_rotated( mCameraRot.mulpos( forward ) );
	m_vLookAt = m_vEye + forward_rotated;

	// Transform the position delta by the camera's rotation 
	const auto vPosDeltaWorld(mCameraRot.mulpos(vPosDelta));

	// Move the eye position 
	m_vEye = m_vEye + vPosDeltaWorld;
	m_vLookAt = m_vLookAt + vPosDeltaWorld;

	if (m_bClipToBoundary)
		ConstrainToBoundary(&m_vEye);

	const auto translation = simd::matrix::translation(m_vEye);
	const auto transformed = mCameraRot * translation;
	m_mView = transformed.inverse();

	SetProjParams( m_fFOV, m_fAspect, m_fNearPlane, m_fFarPlane );
	dirty = false;
}

void FPSCamera::SetViewParams( simd::vector3* pvEyePt, simd::vector3* pvLookatPt, simd::vector3* pvUpPt )
{
	BaseCamera::SetViewParams( pvEyePt, pvLookatPt, pvUpPt );
	Dirty();
}

LRESULT FPSCamera::HandleMessages( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	BaseCamera::HandleMessages( hWnd, uMsg, wParam, lParam );
	return 0;
}

//--------------------------------------------------------------------------------------
// Constructor 
//--------------------------------------------------------------------------------------
RotationCamera::RotationCamera()
{
	m_mWorld = simd::matrix::identity();
	m_mModelRot = simd::matrix::identity();
	m_mModelLastRot = simd::matrix::identity();
	m_mCameraRotLast = simd::matrix::identity();
	m_vModelCenter = simd::vector3(0, 0, 0);
	m_fRadius = 5.0f;
	m_fDefaultRadius = 5.0f;
	m_fMinRadius = 10.0f;
	m_fMaxRadius = 20000.0f;
	m_bLimitPitch = false;
	m_bEnablePositionMovement = false;
	m_bAttachCameraToModel = false;

	m_nRotateModelButtonMask = MOUSE_LEFT_BUTTON;
	m_nZoomButtonMask = MOUSE_WHEEL;
	m_nRotateCameraButtonMask = MOUSE_RIGHT_BUTTON;
	m_bDragSinceLastUpdate = true;
	dirty = true;
}




void RotationCamera::Dirty()
{
	m_bDragSinceLastUpdate = true; dirty = true;
}

//--------------------------------------------------------------------------------------
// Update the view matrix & the model's world matrix based 
//       on user input & elapsed time
//--------------------------------------------------------------------------------------
VOID RotationCamera::FrameMove(FLOAT fElapsedTime)
{
	BaseCamera::FrameMove( fElapsedTime );

	if (IsCamKeyDown(CAM_KEYS_RESET))
		Reset();

	if( dirty || m_bDragSinceLastUpdate )
	{
		SetProjParams( m_fFOV, m_fAspect, m_fNearPlane, m_fFarPlane );
		dirty = false;
	}

	// If no dragged has happend since last time FrameMove is called,
	// and no camera key is held down, then no need to handle again.
	if (!m_bDragSinceLastUpdate && 0 == m_cKeysDown)
		return;
	m_bDragSinceLastUpdate = false;

	//// If no mouse button is held down, 
	//// Get the mouse movement (if any) if the mouse button are down
	//if( m_nCurrentButtonMask != 0 ) 
	//    UpdateMouseDelta( fElapsedTime );

	GetInput(m_bEnablePositionMovement, m_nCurrentButtonMask != 0, true, false);

	// Get amount of velocity based on the keyboard input and drag (if any)
	UpdateVelocity(fElapsedTime);

	// Simple euler method to calculate position delta
	auto vPosDelta(m_vVelocity * fElapsedTime);
	if( IsOrthographicCamera() && vPosDelta.z != 0.0f )
	{
		orthographic_scale = Clamp( orthographic_scale - vPosDelta.z * 0.0005f, 0.01f, 100.0f );
		vPosDelta.z = 0.0f;
		dirty = true;
	}

	// Change the radius from the camera to the model based on wheel scrolling
	if (m_nMouseWheelDelta && m_nZoomButtonMask == MOUSE_WHEEL)
		m_fRadius -= m_nMouseWheelDelta * m_fRadius * 0.1f / 120.0f;
	m_fRadius = min(m_fMaxRadius, m_fRadius);
	m_fRadius = max(m_fMinRadius, m_fRadius);
	m_nMouseWheelDelta = 0;

	// Get the inverse of the arcball's rotation matrix
	const auto xmCameraRot(m_ViewArcBall.GetRotationMatrix().inverse());

	// Transform vectors based on camera's rotation matrix
	const simd::vector3 vLocalUp(0, 1, 0);
	const simd::vector3 vLocalAhead(0, 0, 1);
	const auto vWorldAhead(xmCameraRot.mulpos(vLocalAhead));

	const auto vWorldUp(xmCameraRot.mulpos(vLocalUp));
	m_vUp = vWorldUp;

	// Transform the position delta by the camera's rotation 
	const auto vPosDeltaWorld(xmCameraRot.mulpos(vPosDelta));

	// Move the lookAt position 
	const auto vLookAt(m_vLookAt + vPosDeltaWorld);
	m_vLookAt = vLookAt;

	if (m_bClipToBoundary)
		ConstrainToBoundary(&m_vLookAt);

	// Update the eye point based on a radius away from the lookAt position
	const auto vEye(vLookAt - vWorldAhead * m_fRadius);
	m_vEye = vEye;

	// Update the view matrix
	const auto xmView = simd::matrix::lookat(vEye, vLookAt, vWorldUp);
	m_mView = xmView;

	simd::matrix mInvView = xmView.inverse();
	mInvView[3][0] = mInvView[3][1] = mInvView[3][2] = 0;
	simd::matrix xmInvView = mInvView; // this is a bit ugly, but above zeros a part of the matrix, not so easy with

	const simd::matrix xmModelLastRotInv(m_mModelLastRot.inverse());

	// Accumulate the delta of the arcball's rotation in view space.
	// Note that per-frame delta rotations could be problematic over long periods of time.

	auto xmModelRot = m_WorldArcBall.GetRotationMatrix();
	m_mModelRot = m_mModelRot * xmView * xmModelLastRotInv * xmModelRot * xmInvView;

	if (m_ViewArcBall.IsBeingDragged() && m_bAttachCameraToModel && !IsCamKeyDown(CAM_KEYS_CONTROLDOWN))
	{
		// Attach camera to model by inverse of the model rotation
		simd::matrix xmCameraLastRotInv(m_mCameraRotLast.inverse());
		simd::matrix xmCameraRotDelta(xmCameraLastRotInv * xmCameraRot); // local to world matrix
		xmModelRot = xmModelRot * xmCameraRotDelta;
	}
	m_mCameraRotLast = xmCameraRot;
	m_mModelRot = xmModelRot;
	m_mModelLastRot = xmModelRot;

	// Since we're accumulating delta rotations, we need to orthonormalize 
	// the matrix to prevent eventual matrix skew
	simd::vector3* pXBasis = reinterpret_cast<simd::vector3*>(&m_mModelRot[0][0]);
	simd::vector3* pYBasis = reinterpret_cast<simd::vector3*>(&m_mModelRot[1][0]);
	simd::vector3* pZBasis = reinterpret_cast<simd::vector3*>(&m_mModelRot[2][0]);
	*pXBasis = pXBasis->normalize();
	*pYBasis = pZBasis->cross(*pXBasis);
	*pYBasis = pYBasis->normalize();
	*pZBasis = pXBasis->cross(*pYBasis);

	// Translate the rotation matrix to the same position as the lookAt position
	m_mModelRot[3][0] = m_vLookAt.x;
	m_mModelRot[3][1] = m_vLookAt.y;
	m_mModelRot[3][2] = m_vLookAt.z;

	// Translate world matrix so its at the center of the model
	const auto xmTrans = simd::matrix::translation(-m_vModelCenter);
	m_mWorld = xmTrans * xmModelRot;
}


void RotationCamera::SetDragRect(Device::Rect &rc)
{
	BaseCamera::SetDragRect(rc);

	m_WorldArcBall.SetOffset(rc.left, rc.top);
	m_ViewArcBall.SetOffset(rc.left, rc.top);
	SetWindow(rc.right - rc.left, rc.bottom - rc.top);
}


//--------------------------------------------------------------------------------------
// Reset the camera's position back to the default
//--------------------------------------------------------------------------------------
VOID RotationCamera::Reset()
{
	BaseCamera::Reset();

	m_mWorld = simd::matrix::identity();
	m_mModelRot = simd::matrix::identity();
	m_mModelLastRot = simd::matrix::identity();
	m_mCameraRotLast = simd::matrix::identity();

	m_fRadius = m_fDefaultRadius;
	m_WorldArcBall.Reset();
	m_ViewArcBall.Reset();
}


//--------------------------------------------------------------------------------------
// Override for setting the view parameters
//--------------------------------------------------------------------------------------
void RotationCamera::SetViewParams( simd::vector3* pvEyePt, simd::vector3* pvLookatPt, simd::vector3* pvUpPt )
{
	BaseCamera::SetViewParams( pvEyePt, pvLookatPt, pvUpPt );

	// Propogate changes to the member arcball
	const auto mRotation = simd::matrix::lookat( *pvEyePt, *pvLookatPt, *pvUpPt );

	simd::quaternion quat = mRotation.rotation();
	m_ViewArcBall.SetQuatNow(quat);

	// Set the radius according to the distance
	const auto vEyeToPoint(*pvLookatPt - *pvEyePt);
	SetRadius( vEyeToPoint.len(), m_fMinRadius, m_fMaxRadius );
	
	// View information changed. FrameMove should be called.
	m_bDragSinceLastUpdate = true;

	SetModelCenter( *pvLookatPt );
}

//--------------------------------------------------------------------------------------
// Call this from your message proc so this class can handle window messages
//--------------------------------------------------------------------------------------
LRESULT RotationCamera::HandleMessages(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	BaseCamera::HandleMessages(hWnd, uMsg, wParam, lParam);

	if (((uMsg == WM_LBUTTONDOWN || uMsg == WM_LBUTTONDBLCLK) && m_nRotateModelButtonMask & MOUSE_LEFT_BUTTON) ||
		((uMsg == WM_MBUTTONDOWN || uMsg == WM_MBUTTONDBLCLK) && m_nRotateModelButtonMask & MOUSE_MIDDLE_BUTTON) ||
		((uMsg == WM_RBUTTONDOWN || uMsg == WM_RBUTTONDBLCLK) && m_nRotateModelButtonMask & MOUSE_RIGHT_BUTTON))
	{
		int iMouseX = (short)LOWORD(lParam);
		int iMouseY = (short)HIWORD(lParam);
		m_WorldArcBall.OnBegin(iMouseX, iMouseY);
	}

	if (((uMsg == WM_LBUTTONDOWN || uMsg == WM_LBUTTONDBLCLK) && m_nRotateCameraButtonMask & MOUSE_LEFT_BUTTON) ||
		((uMsg == WM_MBUTTONDOWN || uMsg == WM_MBUTTONDBLCLK) &&
			m_nRotateCameraButtonMask & MOUSE_MIDDLE_BUTTON) ||
			((uMsg == WM_RBUTTONDOWN || uMsg == WM_RBUTTONDBLCLK) && m_nRotateCameraButtonMask & MOUSE_RIGHT_BUTTON))
	{
		int iMouseX = (short)LOWORD(lParam);
		int iMouseY = (short)HIWORD(lParam);
		m_ViewArcBall.OnBegin(iMouseX, iMouseY);
	}

	if (uMsg == WM_MOUSEMOVE)
	{
		int iMouseX = (short)LOWORD(lParam);
		int iMouseY = (short)HIWORD(lParam);
		m_WorldArcBall.OnMove(iMouseX, iMouseY);
		m_ViewArcBall.OnMove(iMouseX, iMouseY);
	}

	if ((uMsg == WM_LBUTTONUP && m_nRotateModelButtonMask & MOUSE_LEFT_BUTTON) ||
		(uMsg == WM_MBUTTONUP && m_nRotateModelButtonMask & MOUSE_MIDDLE_BUTTON) ||
		(uMsg == WM_RBUTTONUP && m_nRotateModelButtonMask & MOUSE_RIGHT_BUTTON))
	{
		m_WorldArcBall.OnEnd();
	}

	if ((uMsg == WM_LBUTTONUP && m_nRotateCameraButtonMask & MOUSE_LEFT_BUTTON) ||
		(uMsg == WM_MBUTTONUP && m_nRotateCameraButtonMask & MOUSE_MIDDLE_BUTTON) ||
		(uMsg == WM_RBUTTONUP && m_nRotateCameraButtonMask & MOUSE_RIGHT_BUTTON))
	{
		m_ViewArcBall.OnEnd();
	}

	if (uMsg == WM_CAPTURECHANGED)
	{
		if ((HWND)lParam != hWnd)
		{
			if ((m_nRotateModelButtonMask & MOUSE_LEFT_BUTTON) ||
				(m_nRotateModelButtonMask & MOUSE_MIDDLE_BUTTON) ||
				(m_nRotateModelButtonMask & MOUSE_RIGHT_BUTTON))
			{
				m_WorldArcBall.OnEnd();
			}

			if ((m_nRotateCameraButtonMask & MOUSE_LEFT_BUTTON) ||
				(m_nRotateCameraButtonMask & MOUSE_MIDDLE_BUTTON) ||
				(m_nRotateCameraButtonMask & MOUSE_RIGHT_BUTTON))
			{
				m_ViewArcBall.OnEnd();
			}
		}
	}

	if (uMsg == WM_LBUTTONDOWN ||
		uMsg == WM_LBUTTONDBLCLK ||
		uMsg == WM_MBUTTONDOWN ||
		uMsg == WM_MBUTTONDBLCLK ||
		uMsg == WM_RBUTTONDOWN ||
		uMsg == WM_RBUTTONDBLCLK ||
		uMsg == WM_LBUTTONUP ||
		uMsg == WM_MBUTTONUP ||
		uMsg == WM_RBUTTONUP ||
		uMsg == WM_MOUSEWHEEL ||
		uMsg == WM_MOUSEMOVE)
	{
		m_bDragSinceLastUpdate = true;
	}

	return FALSE;
}

LRESULT MayaCamera::HandleMessages( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	if( uMsg == WM_MOUSEWHEEL )
		m_nMouseWheelDelta += (short)HIWORD( wParam );

#if !defined( CONSOLE ) && !defined( __APPLE__ ) && !defined( ANDROID )
	if( !( GetKeyState( VK_MENU ) & 0x8000 ) )
	{
		current_operation = OP_NONE;
		alt_down = false;
		return 0;
	}
#endif

	alt_down = true;

	if( uMsg == WM_LBUTTONDOWN || uMsg == WM_LBUTTONDBLCLK )
	{
		iLastMouseX = (short)LOWORD( lParam );
		iLastMouseY = (short)HIWORD( lParam );

		current_operation = OP_LMB_ROTATE;
	}
	else if( uMsg == WM_MBUTTONDOWN || uMsg == WM_MBUTTONDBLCLK )
	{
		int iMouseX = iLastMouseX = (short)LOWORD( lParam );
		int iMouseY = iLastMouseY = (short)HIWORD( lParam );

		simd::vector3 curr_dir, curr_pos;
		std::tie( curr_pos, curr_dir ) = ScreenPositionToRay( iMouseX, iMouseY );
		const simd::vector3 plane_normal = ( *GetEyePt() - *GetLookAtPt() ).normalize();
		if( LinePlaneIntersection( start_pan, curr_dir, curr_pos, plane_normal, *GetLookAtPt() ) )
			current_operation = OP_MMB_PAN;
	}
	else if( uMsg == WM_RBUTTONDOWN || uMsg == WM_RBUTTONDBLCLK )
	{
		iLastMouseX = (short)LOWORD( lParam );
		iLastMouseY = (short)HIWORD( lParam );
		current_operation = OP_RMB_ZOOM;
	}
	else if( uMsg == WM_MOUSEMOVE )
	{
		int iMouseX = (short)LOWORD( lParam );
		int iMouseY = (short)HIWORD( lParam );

		if( current_operation == OP_LMB_ROTATE )
		{
			auto yaw = -( iMouseX - iLastMouseX ) / 120.0f;
			auto pit = (is_orthographic_camera && BaseCamera::ortho_viewmode != BaseCamera::None) ? 0.f : -(iMouseY - iLastMouseY) / 120.0f;

			auto yaw_transform = simd::matrix::rotationaxis( simd::vector3( 0, 0, -1 ), yaw );
			auto pit_transform = simd::matrix::rotationaxis( simd::vector3( 1, 0, 0 ), pit );
			m_ViewArcBall.SetQuatNow( yaw_transform.rotation() * m_ViewArcBall.GetQuatNow() * pit_transform.rotation() );

			UpdateView();
		}
		else if( current_operation == OP_RMB_ZOOM )
		{
			const float distance = static_cast<float>( iMouseY - iLastMouseY )
								 + static_cast<float>( iMouseX - iLastMouseX );
			if( distance )
			{
				m_fRadius -= distance * m_fRadius * 0.2f / 120.0f;
				m_fRadius = min( m_fMaxRadius, m_fRadius );
				m_fRadius = max( m_fMinRadius, m_fRadius );
				m_bDragSinceLastUpdate = true;
			}
		}
		else if( current_operation == OP_MMB_PAN )
		{
			simd::vector3 curr_dir, curr_pos;
			simd::vector3 last_dir, last_pos;

			std::tie( curr_pos, curr_dir ) = ScreenPositionToRay( iMouseX,  iMouseY );
			std::tie( last_pos, last_dir ) = ScreenPositionToRay( iLastMouseX, iLastMouseY );

			simd::vector3 last_intersect, curr_intersect;

			const simd::vector3 plane_normal = ( *GetEyePt() - *GetLookAtPt() ).normalize();

			if( LinePlaneIntersection( last_intersect, last_dir, last_pos, plane_normal, start_pan )
				&& LinePlaneIntersection( curr_intersect, curr_dir, curr_pos, plane_normal, start_pan ) )
			{
				Translate( last_intersect - curr_intersect );
			}
		}

		iLastMouseX = iMouseX;
		iLastMouseY = iMouseY;
	}
	else if( uMsg == WM_LBUTTONUP || uMsg == WM_MBUTTONUP || uMsg == WM_RBUTTONUP )
	{
		current_operation = OP_NONE;
	}
	else if( uMsg == WM_CAPTURECHANGED )
	{
		if( (HWND)lParam != hWnd )
		{
			current_operation = OP_NONE;
		}
	}
	else if( uMsg == WM_LBUTTONDOWN ||
		uMsg == WM_LBUTTONDBLCLK ||
		uMsg == WM_MBUTTONDOWN ||
		uMsg == WM_MBUTTONDBLCLK ||
		uMsg == WM_RBUTTONDOWN ||
		uMsg == WM_RBUTTONDBLCLK ||
		uMsg == WM_LBUTTONUP ||
		uMsg == WM_MBUTTONUP ||
		uMsg == WM_RBUTTONUP ||
		uMsg == WM_MOUSEWHEEL ||
		uMsg == WM_MOUSEMOVE )
	{
		m_bDragSinceLastUpdate = true;
	}
	else
	{
		return 0;
	}

	return 1;
}

void MayaCamera::UpdateView()
{
	Dirty();
}

void MayaCamera::FrameMove( const float elapsed )
{
	if( m_nMouseWheelDelta )
	{
		m_fRadius -= m_nMouseWheelDelta * m_fRadius * 0.1f / 120.0f;
		m_fRadius = min( m_fMaxRadius, m_fRadius );
		m_fRadius = max( m_fMinRadius, m_fRadius );
		Dirty();
	}

	m_nMouseWheelDelta = 0;

	RotationCamera::FrameMove( elapsed );
}

void MayaCamera::OnRender()
{
	RotationCamera::OnRender();

	if( current_operation == OP_MMB_PAN )
	{
		const auto vp = *GetViewMatrix() * *GetProjMatrix();

		auto world_to_screen = [&]( simd::vector3 position )
		{
			//Get the screen space position of the connection point
			simd::vector4 v( position.x, position.y, position.z, 1.0f );
			simd::vector4 screen_position = vp * simd::vector4( position, 1.0f );
			screen_position.x /= screen_position.w;
			screen_position.y /= screen_position.w;

			//Change from post projection space to screen space
			screen_position.x = ( screen_position.x + 1.0f ) * 0.5f;
			screen_position.y = 1.0f - ( screen_position.y + 1.0f ) * 0.5f;

			return simd::vector2( screen_position.x * screen_width, screen_position.y * screen_height );
		};

		simd::vector2 points[] =
		{
			world_to_screen( start_pan + simd::vector3(  0, 0,+5 ) ),
			world_to_screen( start_pan + simd::vector3(  0, 0,-5 ) ),
			world_to_screen( start_pan + simd::vector3(  0,+5, 0 ) ),
			world_to_screen( start_pan + simd::vector3(  0,-5, 0 ) ),
			world_to_screen( start_pan + simd::vector3( +5, 0, 0 ) ),
			world_to_screen( start_pan + simd::vector3( -5, 0, 0 ) ),
		};

		UI::System::Get().Begin();

		// Pan point
		const auto white = simd::color::argb( 255, 255, 255, 255 );
		for( int i = 0; i < 3; ++i )
			UI::System::Get().Draw( points + i * 2, 2, white );

		UI::System::Get().End();
	}

	RotationCamera::OnRender();
}

void MayaCamera::SetViewParams( simd::vector3* pvEyePt, simd::vector3* pvLookatPt, simd::vector3* pvUpVec )
{
	RotationCamera::SetViewParams( pvEyePt, pvLookatPt, pvUpVec );

	UpdateView();
}

std::pair< simd::vector3, simd::vector3 > MayaCamera::ScreenPositionToRay( const int sx, const int sy ) const
{
	auto inverse_view = GetViewMatrix()->inverse();
	const simd::vector3 position( inverse_view[3][0], inverse_view[3][1], inverse_view[3][2]  );

	if( is_orthographic_camera )
	{
		const float scale = GetOrthographicScale();
		const float ray_x = ( sx - screen_width / 2.0f ) * scale;
		const float ray_y = -( sy - screen_height / 2.0f ) * scale;

		auto forward = (*GetLookAtPt() - *GetEyePt()).normalize();
		auto up = *GetUpPt();
		auto right = up.cross( forward );

		return std::make_pair( position + right * ray_x + up * ray_y, forward );
	}

	auto& projection_matrix = *GetProjMatrix();

	simd::vector3 v;
	v.x = ( ( sx * 2 ) / float( screen_width ) - 1.0f ) / projection_matrix[0][0];
	v.y = -( ( sy * 2 ) / float( screen_height ) - 1.0f ) / projection_matrix[1][1]; //Screen space is upside down from projection space
	v.z = 1.0f;

	const simd::vector3 direction( 
		v.x * inverse_view[0][0] + v.y * inverse_view[1][0] + v.z * inverse_view[2][0],
		v.x * inverse_view[0][1] + v.y * inverse_view[1][1] + v.z * inverse_view[2][1],
		v.x * inverse_view[0][2] + v.y * inverse_view[1][2] + v.z * inverse_view[2][2]
	);

	return std::make_pair( position, direction );
}

void MayaCamera::SetWindow( int nWidth, int nHeight, float fArcballRadius /*= 0.9f */ )
{
	RotationCamera::SetWindow( nWidth, nHeight, fArcballRadius );
	screen_width = nWidth;
	screen_height = nHeight;
}

float MayaCamera::GetOrthographicScale() const
{
	return 10.0f * ( m_fRadius - m_fMinRadius ) / ( m_fMaxRadius - m_fMinRadius );
}
