#include "BoneFollowingCamera.h"

#include "ClientInstanceCommon/Animation/AnimatedObject.h"
#include "Visual/Animation/Components/ClientAnimationControllerComponent.h"
#include "Visual/Animation/Components/AnimatedRenderComponent.h"

namespace Renderer
{
	namespace Scene
	{
		BoneFollowingCamera::BoneFollowingCamera( const Animation::AnimatedObject& animated_object, const unsigned bone_index, const float camera_fov_height, const float camera_fov_width, const float z_near, const float z_far, const unsigned window_width, const unsigned window_height )
			: client_animation_controller( animated_object.GetComponent< Animation::Components::ClientAnimationController >() )
			, animated_render( animated_object.GetComponent< Animation::Components::AnimatedRender >() )
			, bone_index( bone_index ), fov_height( camera_fov_height ), fov_width( camera_fov_width ), z_near( z_near ), z_far( z_far )
		{
			assert(z_near != z_far);

			if( !client_animation_controller || !animated_render )
				throw std::runtime_error( "BoneFollowingCamera: animated object doesn't have the required components." );
		}

		void BoneFollowingCamera::WindowChanged( const unsigned width, const unsigned height )
		{
			Camera::WindowChanged( width, height );

			const auto aspect = width / float( height );

			const auto vertical_view_angle = 2 * atan(fov_height / (2 * fov_width));
			const auto projection = simd::matrix::perspectivefov(vertical_view_angle, aspect, z_near, z_far);

			simd::matrix view;
			simd::vector3 position;
			CalculateViewAndPosition(view, position);

			UpdateMatrices(view, projection, simd::matrix::identity(), position, position, z_near);
		}

		void BoneFollowingCamera::FrameMove(const float elapsed_time)
		{
			simd::matrix view;
			simd::vector3 position;
			CalculateViewAndPosition(view, position);

			simd::matrix projection;

			bool projection_set = false;
			bool dof_set = false;
			float focus_distance = 0.0f;
			float focus_range = 0.0f;

			if (client_animation_controller->HasUVAlphaData())
			{
				const auto uv_alpha = client_animation_controller->GetUVAlphaPalette()[bone_index];

				if (uv_alpha.z > 0.001f)
				{
					// Get the focal length on the alpha value stored on the animation. Can only store [0,1] range so need to multiply by 100 here to get the actual value.
					const auto aspect = width / float(height);
					fov_width = uv_alpha.z * 100.f;
					const auto vertical_view_angle = 2 * atan(fov_height / (2 * fov_width));
					projection = simd::matrix::perspectivefov(vertical_view_angle, aspect, z_near, z_far);
					projection_set = true;
				}

				if (uv_alpha.x > 0.001f)
					focus_distance = uv_alpha.x;

				if (uv_alpha.y > 0.001f)
					focus_range = uv_alpha.y;
			}
			
			if (!projection_set)
				projection = GetProjectionMatrix();

			SetDoFSettings({ focus_distance, focus_range });

			UpdateMatrices(view, projection, simd::matrix::identity(), position, position, z_near);
		}

		void BoneFollowingCamera::SetNearFarPlanes(const float near_plane, const float far_plane)
		{
			z_near = near_plane;
			z_far = far_plane;
			WindowChanged(width, height);
		}

		void BoneFollowingCamera::CalculateViewAndPosition(simd::matrix& out, simd::vector3& position)
		{
			//const D3DXVECTOR3 at( -1000.0f, 300.0f, -400.0f );
			//const D3DXVECTOR3 pos = at + D3DXVECTOR3( 840, -400, 0.0f );
			const simd::matrix cam_pos_mat = client_animation_controller->GetBoneTransform(bone_index) * animated_render->GetTransform();
			const simd::vector3 identity(0.0f, 0.0f, 0.0f), unit_along(0.0, 1.0, 0.0f), unit_up(0.0f, 0.0f, -1.0f);

			//D3DXMATRIX camera_rotation;
			//D3DXMatrixRotationYawPitchRoll( &camera_rotation, 
			//D3DXVec3TransformNormal

			const auto pos = cam_pos_mat.mulpos(identity);
			const auto dir = cam_pos_mat.muldir(unit_along);
			const auto up = cam_pos_mat.muldir(unit_up);

			out = simd::matrix::lookat(pos, pos + dir, up);
			position = pos;
		}
	}
}
