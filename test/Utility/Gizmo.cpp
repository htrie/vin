#include "Gizmo.h"

#include "Common/Utility/Numeric.h"
#include "Common/Utility/Geometric.h"
#include "Visual/Device/Constants.h"
#include "Visual/Device/WindowDX.h"
#include "Visual/Device/Font.h"
#include "Visual/Device/Line.h"
#include "Visual/GUI2/imgui/imgui.h"
#include "Visual/Renderer/Scene/Camera.h"
#include "Visual/Renderer/Scene/BaseCamera.h"

#include "Common/Geometry/RayBox.h"

using simd::vector3;
using simd::matrix;
using simd::color;
using simd::quaternion;

namespace Utility
{
	namespace
	{
		int AXES[] = { Axes::X, Axes::Y, Axes::Z };
	}

	Gizmo::Gizmo()
		: drag_axis( -1 )
		, root( nullptr )
		, dragging( false )
		, transform( matrix::identity() )
		, last_trace( 0 )
		, drag_start( 0 )
	{
	}

	bool Gizmo::GetVisible() const
	{
		const auto& listeners = GetListeners();
		return !listeners.empty() && AnyOfIf( listeners, [ this ]( const GizmoListener* listener ) { return listener && listener->IsGizmoVisible( *this ); } );
	}

	const matrix& Gizmo::GetLocalTransform() const
	{
		return transform;
	}

	const simd::vector3 Gizmo::GetPosition() const
	{
		return { transform[3][0], transform[3][1], transform[3][2] };
	}

	void Gizmo::OnRender(const std::function<ImVec2(const simd::vector3&)>& worldToScreen)
	{
		if (!GetVisible() || !worldToScreen)
			return;

		Refresh();

		const auto rotation = root ? root->rotation() : quaternion::identity();
		const auto pos = root ? root->mulpos(transform.translation()) : transform.translation();

		const auto& listeners = GetListeners();

		auto found_listener = FindIf(listeners, [this](const GizmoListener* listener)
		{
			return listener && listener->IsGizmoVisible(*this);
		});

		if (found_listener == listeners.end())
			return;

		GizmoListener* listener = *found_listener;

		const float line_length = listener->GetLineLength(*this);
		const float line_width = listener->GetLineWidth(*this);
		const bool y_flip = listener->FlipYDirection(*this);

		std::array<vector3, 8> world_basis_vectors =
		{
			pos, pos + rotation.transform(vector3(+line_length, 0.0f, 0.0f)),
			pos, pos + rotation.transform(vector3(0.0f, +line_length, 0.0f)),
			pos, pos + rotation.transform(vector3(0.0f, 0.0f, y_flip ? -line_length : +line_length)),
			pos, vector3(pos.x, pos.y, 0),
		};

		color colours[] =
		{
			color::argb(255,255,0,0),
			color::argb(255,0,255,0),
			color::argb(255,0,0,255),
			color::argb(255,255,255,255),
			color::argb(255,255,200,0),
			color::argb(64, 64, 64, 64)
		};

		enum colour_indices
		{
			c_X, c_Y, c_Z, c_Normal, c_Locked, c_Dim
		};

		auto colour = listener->GetColour(*this);
		if (listener->IsGizmoLocked(*this))
			colour = colours[c_Locked];

		auto draw_list = ImGui::GetBackgroundDrawList(ImGui::GetMainViewport());
		const auto offset = ImGui::GetMainViewport()->Pos;

		const auto screen_vectors = Transform<ImVec2>(world_basis_vectors, [&](const auto& pos) {return worldToScreen(pos) + offset; });

		draw_list->AddLine(screen_vectors[6], screen_vectors[7], ImColor(colours[c_Dim].rgba()), line_width);

		auto axes = listener->GetGizmoAxes(*this);
		for (int i = 0; i < 3; ++i)
		{
			if (!(AXES[i] & axes))
				continue;

			auto axis_colour = drag_axis == AXES[i] ? colours[i] : colour;
			axis_colour.a = colour.a;

			draw_list->AddLine(screen_vectors[(i * 2) + 0], screen_vectors[(i * 2) + 1], ImColor(axis_colour.rgba()), line_width);
		}
	}

	void Gizmo::OnRender(const BaseCamera& camera)
	{
		OnRender([&](const simd::vector3& pos) 
		{
			float w, h;
			std::tie(w, h) = camera.GetOrthographicWidthHeight();

			auto screen_position = (*camera.GetViewMatrix() * *camera.GetProjMatrix()) * simd::vector4(pos, 1.0f);

			screen_position[0] /= screen_position[3];
			screen_position[1] /= screen_position[3];

			const float half_width = (w * 0.5f);
			const float half_height = (h * 0.5f);
			screen_position[0] = (screen_position[0] * half_width + half_width);
			screen_position[1] = (-screen_position[1] * half_height + half_height);

			return ImVec2(screen_position[0], screen_position[1]);
		});
	}

	void Gizmo::OnRender(const Renderer::Scene::Camera& camera)
	{
		OnRender([&](const simd::vector3& pos) { return ImVec2(camera.WorldToScreen(pos)); });
	}

	bool Gizmo::RayMsgProc( UINT uMsg, WPARAM wParam, LPARAM lParam, const vector3& ray_dir, const vector3& ray_pos )
	{
	#ifndef CONSOLE
		if( !GetVisible() || !CanGizmoDrag() )
			return false;

		const auto base_centre = transform.translation();
		const auto centre = root ? root->mulpos( base_centre ) : base_centre;

		auto ray_intersect_plane = [ & ]()
		{
			vector3 normal;
			switch( drag_axis )
			{
			default:
			case Axes::X: normal = vector3( 0, 0, 1 );
				break;
			case Axes::Y: normal = vector3( 0, 0, 1 );
				break;
			case Axes::Z: normal = vector3( 0, 1, 0 );
				break;
			}

			if( root )
				normal = root->muldir( normal );

			float denom = normal.dot( ray_dir );
			if( abs( denom ) > 0.0001f ) // your favorite epsilon
			{
				float t = ( centre - ray_pos ).dot( normal ) / denom;
				if( t >= 0 )
					return ray_pos + ray_dir * t;
			}

			assert( false );
			return vector3( 0, 0, 0 );
		};

		if( uMsg == WM_LBUTTONDOWN )
		{
			if( drag_axis == -1 )
				return false;
			else
			{
				dragging = true;
				last_trace = ray_intersect_plane();
				drag_start = transform.translation();
				NotifyListeners( &GizmoListener::OnDragBegin, *this );
				return true;
			}

			return false;
		}
		else if( uMsg == WM_LBUTTONUP )
		{
			if( dragging )
			{
				dragging = false;
				const auto distance = transform.translation() - drag_start;
				NotifyListeners( &GizmoListener::OnDragEnd, *this, distance );
				return true;
			}

			return false;
		}
		else if( uMsg != WM_MOUSEMOVE || GetLocked() )
			return false;

		if( !dragging )
		{
			drag_axis = -1;

			const float len = 12.5;
			AABB test;

			const auto inverse_root = root ? root->inverse() : matrix::identity();
			const auto ray_pos_transformed = inverse_root.mulpos( ray_pos );
			const auto ray_dir_transformed = inverse_root.muldir( ray_dir ).normalize();

			test.center = base_centre + vector3( len, 0, 0 );
			test.extents = vector3( len, 2, 2 );

			const auto& listeners = GetListeners();

			auto found_listener = FindIf( listeners, [this]( const GizmoListener* listener )
				{
					return listener && listener->IsGizmoVisible( *this );
				} );

			if( found_listener == listeners.end() )
				return false;

			auto axes = (*found_listener)->GetGizmoAxes( *this );

			if( axes & Axes::X && IntersectRayBox( ray_dir_transformed, ray_pos_transformed, test ) )
			{
				drag_axis = Axes::X;
				return false;
			}

			test.center = base_centre + vector3( 0, len, 0 );
			test.extents = vector3( 2, len, 2 );
			if( axes & Axes::Y && IntersectRayBox( ray_dir_transformed, ray_pos_transformed, test ) )
			{
				drag_axis = Axes::Y;
				return false;
			}

			test.center = base_centre + vector3( 0, 0, len );
			test.extents = vector3( 2, 2, len );
			if( axes & Axes::Z && IntersectRayBox( ray_dir_transformed, ray_pos_transformed, test ) )
			{
				drag_axis = Axes::Z;
				return false;
			}

			return false; // Consume no input either way.
		}

		// We are dragging, don't change the axis handle now.
		// Set new location.

		const float cursor_x = float( LOWORD( lParam ) );
		const float cursor_y = float( HIWORD( lParam ) );

		const auto projection = Vector2d( ray_pos.x, ray_pos.y ) + Vector2d( ray_dir.x, ray_dir.y ) * -( ray_pos.z / ray_dir.z );

		const auto new_trace = ray_intersect_plane();

		const auto rotation = root ? root->rotation() : quaternion::identity();
		const auto diff = rotation.inverse().transform( new_trace - last_trace );
		last_trace = new_trace;

		switch( drag_axis )
		{
		default:break;
		case Axes::X: Translate( vector3( diff.x, 0.0f, 0.0f ) );
			break;
		case Axes::Y: Translate( vector3( 0.0f, diff.y, 0.0f ) );
			break;
		case Axes::Z: Translate( vector3( 0.0f, 0.0f, diff.z ) );
			break;
		}

		return true;
	#else
		return false;
	#endif
	}

	void Gizmo::Refresh()
	{
		if( dragging )
			return;

		vector3 new_pos( 0.0f );

		const auto& listeners = GetListeners();
		if( listeners.size() == 1 && listeners[0] )
			new_pos = listeners.front()->GetGizmoPosition( *this );
		else if( listeners.size() > 1 )
		{
			simd::vector3 A( 0.0f );
			simd::vector3 B( 0.0f );
			simd::vector3 C( 0.0f );
			float furthest = 0;

			for( auto* l1 : listeners )
			{
				if( !l1 )
					continue;
				C = l1->GetGizmoPosition( *this );
				for( auto* l2 : listeners )
				{
					if( !l2 || l1 == l2 )
						continue;

					const auto l1_p = l1->GetGizmoPosition( *this );
					const auto l2_p = l2->GetGizmoPosition( *this );
					const auto dist_sq = ( l2_p - l1_p ).sqrlen();

					if( dist_sq > furthest )
					{
						furthest = dist_sq;
						A = l1_p;
						B = l2_p;
					}
				}
			}
			if( furthest > 0 )
				new_pos = A.lerp( B, 0.5f );
			else
				new_pos = C;
		}

		SetPosition( new_pos, true );
	}

	bool Gizmo::CanGizmoDrag() const
	{
		if( !GetVisible() )
			return false;

		const auto& listeners = GetListeners();
		return AllOfIf( listeners, [ this ]( const GizmoListener* listener ) { return !listener || listener->CanGizmoDrag( *this ); } );
	}

	void Gizmo::Translate( const vector3& offset, const bool no_callback )
	{
		SetPosition( transform.translation() + offset, no_callback );
	}

	void Gizmo::SetPosition( const simd::vector3& position, const bool no_callback /*= false */ )
	{
		const auto distance = position - transform.translation();

		transform[3][0] = position.x;
		transform[3][1] = position.y;
		transform[3][2] = position.z;

		if( no_callback )
			return;

		NotifyListeners( &GizmoListener::OnPositionChanged, *this, distance );
	}
}