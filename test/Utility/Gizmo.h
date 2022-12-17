#pragma once

#include "Common/Utility/ContainerOperations.h"
#include "../Device/Constants.h"
#include "Common/Utility/Listenable.h"

struct ImVec2;
class BaseCamera;

namespace Device
{
	class Font;
	class Line;
}

namespace Renderer::Scene
{
	class Camera;
}

namespace Utility
{
	// Enumerations
	struct Axes
	{
		enum 
		{
			X = 1,
			Y = 2,
			Z = 4,
			XY = X | Y,
			XZ = X | Z,
			YZ = Y | Z,
			XYZ = X | Y | Z,
		};
	};

	class Gizmo;

	// Callbacks
	class GizmoListener
	{
	private:
		friend class Gizmo;

		// This is used to determine the location of the gizmo in 3d space.
		virtual simd::vector3 GetGizmoPosition( const Gizmo& gizmo ) const = 0;

		// This is used to determine if the gizmo's location should be locked.
		virtual bool FlipYDirection( const Gizmo& gizmo ) const { return false; }
		virtual bool IsGizmoVisible( const Gizmo& gizmo ) const { return false; }
		virtual bool IsGizmoLocked( const Gizmo& gizmo ) const { return false; }
		virtual bool CanGizmoDrag( const Gizmo& gizmo ) const { return true; }
		virtual float GetLineLength( const Gizmo& gizmo ) const { return 15.0f; }
		virtual int GetGizmoAxes( const Gizmo& gizmo ) const { return Axes::XYZ; }
		virtual float GetLineWidth( const Gizmo& gizmo ) const { return 1.0f; }
		virtual unsigned GetPattern( const Gizmo& gizmo ) const { return 0xffffffff; }
		virtual simd::color GetColour( const Gizmo& gizmo ) const { return simd::color::argb( 255, 255, 255, 255 ); }

		virtual void OnConnected( Gizmo& gizmo ) {}
		virtual void OnDetached( Gizmo& gizmo ) {}
		virtual void OnPositionChanged( Gizmo& gizmo, const simd::vector3& distance ) {}
		virtual void OnDragBegin( Gizmo& gizmo ) {}
		virtual void OnDragEnd( Gizmo& gizmo, const simd::vector3& distance ) {}
	};

	class Gizmo : public Listenable<GizmoListener>
	{
	public:

		Gizmo();

		// Get methods
		bool GetVisible() const;

		const simd::matrix& GetLocalTransform() const;
		const simd::vector3 GetPosition() const;

		void SetTransformRoot( const simd::matrix* new_root ) { root = new_root; }

		bool GetDragging() const { return dragging; }
		bool GetLocked() const { return AnyOf( GetListeners(), [ this ]( const auto l ) { return l && l->IsGizmoLocked( *this ); } ); }

		// Update Methods
		void OnRender( const Renderer::Scene::Camera& camera );
		void OnRender( const BaseCamera& camera );
		void OnRender(const std::function<ImVec2(const simd::vector3&)>& worldToScreen);
		bool RayMsgProc( UINT uMsg, WPARAM wParam, LPARAM lParam, const simd::vector3& ray_dir, const simd::vector3& ray_pos );

		void Refresh();

	private:
		bool CanGizmoDrag() const;

		// Transform methods
		void Translate( const simd::vector3& offset, const bool no_callback = false );
		void SetPosition( const simd::vector3& position, const bool no_callback = false );

		// Front-end information
		const simd::matrix* root;
		simd::matrix transform;
		bool dragging;

		// Back-end information
		int drag_axis;
		simd::vector3 last_trace;
		simd::vector3 drag_start;
	};
}

using Gizmo = Utility::Gizmo;