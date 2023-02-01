#pragma once
//--------------------------------------------------------------------------------------
// File: Camera.h
//
// Helper functions for Direct3D programming.
//
// Copyright (c) Microsoft Corporation. All rights reserved
//--------------------------------------------------------------------------------------
#pragma once

#include "Visual/Device/Constants.h"
#include "Visual/Utility/Gizmo.h"
#include "Common/Utility/Numeric.h"

#define GIZMO_ENABLED GGG_CHEATS

//--------------------------------------------------------------------------------------
class ArcBall
{
public:
	ArcBall();

	// Functions to change behavior
	void                            Reset();
	void                            SetTranslationRadius(float fRadiusTranslation)
	{
		m_fRadiusTranslation = fRadiusTranslation;
	}
	void                            SetWindow(INT nWidth, INT nHeight, float fRadius = 0.9f)
	{
		m_nWidth = nWidth; m_nHeight = nHeight; m_fRadius = fRadius;
		m_vCenter = simd::vector2(m_nWidth / 2.0f, m_nHeight / 2.0f);
	}
	void                            SetOffset(INT nX, INT nY)
	{
		m_Offset.x = nX; m_Offset.y = nY;
	}

	// Call these from client and use GetRotationMatrix() to read new rotation matrix
	void                            OnBegin(int nX, int nY);  // start the rotation (pass current mouse position)
	void                            OnMove(int nX, int nY);   // continue the rotation (pass current mouse position)
	void                            OnEnd();                    // end the rotation 

																// Or call this to automatically handle left, middle, right buttons
	LRESULT                         HandleMessages(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	// Functions to get/set state
	simd::matrix GetRotationMatrix()
	{
		return simd::matrix::rotationquaternion(m_qNow);
	};
	const simd::matrix* GetTranslationMatrix() const
	{
		return &m_mTranslation;
	}
	const simd::matrix* GetTranslationDeltaMatrix() const
	{
		return &m_mTranslationDelta;
	}
	bool IsBeingDragged() const
	{
		return m_bDrag;
	}

	const simd::quaternion& GetQuatNow() const
	{
		return m_qNow;
	}

	void SetQuatNow(const simd::quaternion& q)
	{
		m_qNow = q;
	}

	static simd::quaternion WINAPI QuatFromBallPoints(const simd::vector3& vFrom, const simd::vector3& vTo);


protected:
	simd::matrix m_mRotation;         // Matrix for arc ball's orientation
	simd::matrix m_mTranslation;      // Matrix for arc ball's position
	simd::matrix m_mTranslationDelta; // Matrix for arc ball's position

	POINT m_Offset;   // window offset, or upper-left corner of window
	INT m_nWidth;   // arc ball's window width
	INT m_nHeight;  // arc ball's window height
	simd::vector2 m_vCenter;  // center of arc ball 
	float m_fRadius;  // arc ball's radius in screen coords
	float m_fRadiusTranslation; // arc ball's radius for translating the target

	simd::quaternion m_qDown;             // Quaternion before button down
	simd::quaternion m_qNow;              // Composite quaternion for current drag

	bool m_bDrag;             // Whether user is dragging arc ball

	POINT m_ptLastMouse;      // position of last mouse point
	simd::vector3 m_vDownPt;           // starting point of rotation arc
	simd::vector3 m_vCurrentPt;        // current point of rotation arc

	simd::vector3                     ScreenToVector(float fScreenPtX, float fScreenPtY);
};


//--------------------------------------------------------------------------------------
// used by CCamera to map WM_KEYDOWN keys
//--------------------------------------------------------------------------------------
enum CamKeys
{
	CAM_KEYS_STRAFE_LEFT = 0,
	CAM_KEYS_STRAFE_RIGHT,
	CAM_KEYS_MOVE_FORWARD,
	CAM_KEYS_MOVE_BACKWARD,
	CAM_KEYS_MOVE_UP,
	CAM_KEYS_MOVE_DOWN,
	CAM_KEYS_RESET,
	CAM_KEYS_CONTROLDOWN,
	CAM_KEYS_MAX_KEYS,
	CAM_KEYS_UNKNOWN = 0xFF
};


#define MOUSE_LEFT_BUTTON   0x01
#define MOUSE_MIDDLE_BUTTON 0x02
#define MOUSE_RIGHT_BUTTON  0x04
#define MOUSE_WHEEL         0x08


//--------------------------------------------------------------------------------------
// Simple base camera class that moves and rotates.  The base class
//       records mouse and keyboard input for use by a derived class, and 
//       keeps common state.
//--------------------------------------------------------------------------------------
class BaseCamera
#if GIZMO_ENABLED
	: public Utility::GizmoListener
#endif
{
public:
	BaseCamera();
	virtual ~BaseCamera();

	virtual bool ForceUseCameraControls() const { return false; }

	// Call these from client and use Get*Matrix() to read new matrices
	virtual LRESULT             HandleMessages(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	virtual void                FrameMove(float fElapsedTime);

	// Functions to change camera matrices
	virtual void                Reset();
	virtual void                SetViewParams( simd::vector3* pvEyePt, simd::vector3* pvLookatPt, simd::vector3* pvUpPt );
	virtual void                SetProjParams(float fFOV, float fAspect, float fNearPlane, float fFarPlane);

	bool IsOrthographicCamera() const { return is_orthographic_camera; }
	void SetOrthographicCamera( const bool enable );
	virtual float GetOrthographicScale() const { return orthographic_scale; }
	void SetOrthographicScale( const float scale ) { orthographic_scale = scale; }
	virtual std::pair<float, float> GetOrthographicWidthHeight() const;

	// Functions to change behavior
	virtual void                SetDragRect(Device::Rect& rc)
	{
		m_rcDrag = rc;
	}
	void                        SetInvertPitch(bool bInvertPitch)
	{
		m_bInvertPitch = bInvertPitch;
	}
	void                        SetDrag(bool bMovementDrag, float fTotalDragTimeToZero = 0.25f)
	{
		m_bMovementDrag = bMovementDrag; m_fTotalDragTimeToZero = fTotalDragTimeToZero;
	}
	void                        SetEnableYAxisMovement(bool bEnableYAxisMovement)
	{
		m_bEnableYAxisMovement = bEnableYAxisMovement;
	}
	void                        SetEnablePositionMovement(bool bEnablePositionMovement)
	{
		m_bEnablePositionMovement = bEnablePositionMovement;
	}
	void                        SetEnablePrecisionMovement( bool bEnablePrecisionMovement )
	{
		m_bEnablePrecisionMovement = bEnablePrecisionMovement;
	}
	void                        SetClipToBoundary(bool bClipToBoundary, simd::vector3* pvMinBoundary, simd::vector3* pvMaxBoundary)
	{
		m_bClipToBoundary = bClipToBoundary; if (pvMinBoundary) m_vMinBoundary = *pvMinBoundary;
		if (pvMaxBoundary) m_vMaxBoundary = *pvMaxBoundary;
	}
	void                        SetScalers(float fRotationScaler = 0.01f, float fMoveScaler = 5.0f)
	{
		m_fRotationScaler = fRotationScaler; m_fMoveScaler = fMoveScaler;
	}
	void                        SetNumberOfFramesToSmoothMouseData(int nFrames)
	{
		if (nFrames > 0) m_fFramesToSmoothMouseData = (float)nFrames;
	}
	void                        SetResetCursorAfterMove(bool bResetCursorAfterMove)
	{
		m_bResetCursorAfterMove = bResetCursorAfterMove;
	}

	// Functions to get state
	const simd::matrix* GetViewMatrix() const
	{
		return &m_mView;
	}
	const simd::matrix* GetProjMatrix() const
	{
		return &m_mProj;
	}
	const simd::vector3* GetEyePt() const
	{
		return &m_vEye;
	}
	const simd::vector3* GetLookAtPt() const
	{
		return &m_vLookAt;
	}
	const simd::vector3* GetUpPt() const
	{
		return &m_vUp;
	}
	float GetNearClip() const
	{
		return m_fNearPlane;
	}
	float GetFarClip() const
	{
		return m_fFarPlane;
	}
	bool IsBeingDragged() const
	{
		return (m_bMouseLButtonDown || m_bMouseMButtonDown || m_bMouseRButtonDown);
	}
	bool IsMouseLButtonDown() const
	{
		return m_bMouseLButtonDown;
	}
	bool IsMouseMButtonDown() const
	{
		return m_bMouseMButtonDown;
	}
	bool IsMouseRButtonDown() const
	{
		return m_bMouseRButtonDown;
	}

#if defined(MOVIE_MODE) && defined(ENABLE_DEBUG_CAMERA)
	virtual void SetFOV( const float new_fov );
	virtual float GetFOV() const { return m_fFOV; }
#endif

	void Translate( const simd::vector3& delta );

	virtual void Dirty() {}

	inline static bool IsGizmoEnabled = false;
	inline static bool ImmediateMode = true;
	
	enum OrthoViewMode : unsigned
	{
		None,
		Side,
		Top,
		NumOrthoViewMode
	};
	static OrthoViewMode ortho_viewmode; // = Side;
	static std::string ortho_viewmodes[NumOrthoViewMode];

	// For rendering gizmos
	virtual void OnRender();

protected:
	bool IsCamKeyDown(CamKeys key) const
	{
		return cam_key_down[ key ];
	}

	void                        ConstrainToBoundary(simd::vector3* pV);
	void                        UpdateMouseDelta();
	void                        UpdateVelocity(float fElapsedTime);
	void                        GetInput(bool bGetKeyboardInput, bool bGetMouseInput, bool bGetGamepadInput, bool bResetCursorAfterMove);

#if GIZMO_ENABLED
	// Gizmo functions
	virtual simd::vector3 GetGizmoPosition( const Gizmo& gizmo ) const override;
	virtual unsigned            GetPattern( const Gizmo& gizmo ) const override;
	virtual bool            FlipYDirection( const Gizmo& gizmo ) const override;
	virtual bool            IsGizmoVisible( const Gizmo& gizmo ) const override;
	virtual bool              CanGizmoDrag( const Gizmo& gizmo ) const override;
	virtual int               GetGizmoAxes( const Gizmo& gizmo ) const override;
	virtual float            GetLineLength( const Gizmo& gizmo ) const override;
	virtual simd::color          GetColour( const Gizmo& gizmo ) const override;

	struct Gizmos { enum { Ground, LookAt, Max }; };
	Gizmo camera_gizmos[Gizmos::Max];
#endif

	simd::matrix m_mView;              // View matrix 
	simd::matrix m_mProj;              // Projection matrix

	int m_cKeysDown;            // Number of camera keys that are down.
	unsigned char cam_key_down[ CAM_KEYS_MAX_KEYS ] = { 0 };
	bool key_down[ 256 ] = { false };
	std::map<int, CamKeys> code_to_cam_key;

	simd::vector3 m_vKeyboardDirection;   // Direction vector of keyboard input
	Device::Point m_ptLastMousePosition;  // Last absolute position of mouse cursor
	Device::Point m_ptCurMouseDelta;
	Device::Point m_ptCurMousePos;
	bool m_bMouseLButtonDown;    // True if left button is down 
	bool m_bMouseMButtonDown;    // True if middle button is down 
	bool m_bMouseRButtonDown;    // True if right button is down 
	int m_nCurrentButtonMask;   // mask of which buttons are down
	int m_nMouseWheelDelta;     // Amount of middle wheel scroll (+/-) 
	simd::vector2 m_vMouseDelta;          // Mouse relative delta smoothed over a few frames
	float m_fFramesToSmoothMouseData; // Number of frames to smooth mouse data over

	simd::vector3 m_vDefaultEye;          // Default camera eye position
	simd::vector3 m_vDefaultLookAt;       // Default LookAt position
	simd::vector3 m_vDefaultUp;           // Default Up vector
	simd::vector3 m_vEye;                 // Camera eye position
	simd::vector3 m_vLookAt;              // LookAt position
	simd::vector3 m_vUp;                  // Up vector
	float m_fCameraYawAngle;      // Yaw angle of camera
	float m_fCameraPitchAngle;    // Pitch angle of camera

	bool m_bEnablePrecisionMovement;

	Device::Rect m_rcDrag;               // Rectangle within which a drag can be initiated.
	simd::vector3 m_vVelocity;            // Velocity of camera
	bool m_bMovementDrag;        // If true, then camera movement will slow to a stop otherwise movement is instant
	simd::vector3 m_vVelocityDrag;        // Velocity drag force
	float m_fDragTimer;           // Countdown timer to apply drag
	float m_fTotalDragTimeToZero; // Time it takes for velocity to go from full to 0
	simd::vector2 m_vRotVelocity;         // Velocity of camera

	float m_fFOV;                 // Field of view
	float m_fAspect;              // Aspect ratio
	float m_fNearPlane;           // Near plane
	float m_fFarPlane;            // Far plane

	float m_fRotationScaler;      // Scaler for rotation
	float m_fMoveScaler;          // Scaler for movement

	bool m_bInvertPitch;         // Invert the pitch axis
	bool m_bEnablePositionMovement; // If true, then the user can translate the camera/model 
	bool m_bEnableYAxisMovement; // If true, then camera can move in the y-axis

	bool m_bClipToBoundary;      // If true, then the camera will be clipped to the boundary
	simd::vector3 m_vMinBoundary;         // Min point in clip boundary
	simd::vector3 m_vMaxBoundary;         // Max point in clip boundary

	bool m_bResetCursorAfterMove;// If true, the class will reset the cursor position so that the cursor always has space to move 

	bool is_orthographic_camera = false;
	float orthographic_scale = 1.0f;
};


//--------------------------------------------------------------------------------------
// Simple first person camera class that moves and rotates.
//       It allows yaw and pitch but not roll.  It uses WM_KEYDOWN and 
//       GetCursorPos() to respond to keyboard and mouse input and updates the 
//       view matrix based on input.  
//--------------------------------------------------------------------------------------
class FPSCamera : public BaseCamera
{
public:
	FPSCamera();

	virtual void Dirty() override;
	virtual void FrameMove(float fElapsedTime) override;
	virtual void SetViewParams( simd::vector3* pvEyePt, simd::vector3* pvLookatPt, simd::vector3* pvUpPt ) override;
	virtual LRESULT HandleMessages(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override;

	virtual float GetOrthographicScale() const override { return orthographic_scale; };

protected:
	int m_nActiveButtonMask;  // Mask to determine which button to enable for rotation
	bool dirty = true;
};


//--------------------------------------------------------------------------------------
// Simple model viewing camera class that rotates around the object.
//--------------------------------------------------------------------------------------
class RotationCamera : public BaseCamera
{
public:
	RotationCamera();

	virtual void Dirty() override;

	// Call these from client and use Get*Matrix() to read new matrices
	virtual LRESULT HandleMessages(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override;
	virtual void    FrameMove(float fElapsedTime) override;


	// Functions to change behavior
	virtual void    SetDragRect(Device::Rect& rc) override;
	virtual void    Reset() override;
	virtual void    SetViewParams(simd::vector3* pvEyePt, simd::vector3* pvLookatPt, simd::vector3* pvUpVec) override;
	void            SetButtonMasks(int nRotateModelButtonMask = MOUSE_LEFT_BUTTON, int nZoomButtonMask = MOUSE_WHEEL,
		int nRotateCameraButtonMask = MOUSE_RIGHT_BUTTON)
	{
		m_nRotateModelButtonMask = nRotateModelButtonMask;
		m_nZoomButtonMask = nZoomButtonMask;
		m_nRotateCameraButtonMask = nRotateCameraButtonMask;
	}
	void SetAttachCameraToModel(bool bEnable = false)
	{
		m_bAttachCameraToModel = bEnable;
	}
	virtual void SetWindow(int nWidth, int nHeight, float fArcballRadius = 0.9f)
	{
		m_WorldArcBall.SetWindow(nWidth, nHeight, fArcballRadius);
		m_ViewArcBall.SetWindow(nWidth, nHeight, fArcballRadius);
	}
	void SetRadiusOnly( float radius )
	{
		m_fRadius = Clamp( radius, m_fMinRadius, m_fMaxRadius );
		m_bDragSinceLastUpdate = true;
	}
	void SetRadius(float fDefaultRadius = 5.0f, float fMinRadius = 1.0f, float fMaxRadius = FLT_MAX)
	{
		m_fDefaultRadius = m_fRadius = fDefaultRadius; m_fMinRadius = fMinRadius; m_fMaxRadius = fMaxRadius;
		m_bDragSinceLastUpdate = true;
	}

	float GetRadius() const { return m_fRadius; }
	float GetMinRadius() const { return m_fMinRadius; }
	float GetMaxRadius() const { return m_fMaxRadius; }

	void SetModelCenter(const simd::vector3& vModelCenter)
	{
		m_vModelCenter = vModelCenter;
	}
	void SetLimitPitch(bool bLimitPitch)
	{
		m_bLimitPitch = bLimitPitch;
	}
	void SetViewQuat(const simd::quaternion&  q)
	{
		m_ViewArcBall.SetQuatNow((simd::vector4&)q); m_bDragSinceLastUpdate = true;
	}
	void SetWorldQuat(const simd::vector4& q)
	{
		m_WorldArcBall.SetQuatNow(q); m_bDragSinceLastUpdate = true;
	}

	// Functions to get state
	const simd::matrix* GetWorldMatrix() const
	{
		return &m_mWorld;
	}
	void            SetWorldMatrix(simd::matrix& mWorld)
	{
		m_mWorld = mWorld; m_bDragSinceLastUpdate = true;
	}

	const simd::quaternion& GetViewQuat() const { return m_ViewArcBall.GetQuatNow(); }
	const simd::vector3& GetModelCenter() const { return m_vModelCenter; }

protected:
	ArcBall m_WorldArcBall;
	ArcBall m_ViewArcBall;
	simd::vector3 m_vModelCenter;
	simd::matrix m_mModelLastRot;        // Last arcball rotation matrix for model 
	simd::matrix m_mModelRot;            // Rotation matrix of model
	simd::matrix m_mWorld;               // World matrix of model

	int m_nRotateModelButtonMask;
	int m_nZoomButtonMask;
	int m_nRotateCameraButtonMask;

	bool m_bAttachCameraToModel;
	bool m_bLimitPitch;
	float m_fRadius;              // Distance from the camera to model 
	float m_fDefaultRadius;       // Distance from the camera to model 
	float m_fMinRadius;           // Min radius
	float m_fMaxRadius;           // Max radius
	bool m_bDragSinceLastUpdate; // True if mouse drag has happened since last time FrameMove is called.
	bool dirty;

	simd::matrix m_mCameraRotLast;
};

class MayaCamera : public RotationCamera
{
public:
	// WIP
	enum Operations
	{
		OP_NONE,
		OP_LMB_ROTATE,
		OP_MMB_PAN,
		OP_RMB_ZOOM,
	};

	virtual bool ForceUseCameraControls() const override { return alt_down; }

	virtual LRESULT HandleMessages( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam ) override;

	void UpdateView();

	virtual void FrameMove( const float elapsed ) override;
	virtual void OnRender() override;

	virtual void SetViewParams( simd::vector3* pvEyePt, simd::vector3* pvLookatPt, simd::vector3* pvUpVec ) override;

	std::pair< simd::vector3, simd::vector3 > ScreenPositionToRay( const int sx, const int sy ) const;

	virtual void SetWindow( int nWidth, int nHeight, float fArcballRadius = 0.9f ) override;

	virtual float GetOrthographicScale() const override;

	Operations current_operation = OP_NONE;
	bool alt_down = false;
	int iLastMouseX = 0;
	int iLastMouseY = 0;
	int screen_width = 0;
	int screen_height = 0;

	simd::vector3 start_pan = { 0,0,0 };
};
