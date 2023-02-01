
#include "Visual/Renderer/DrawCalls/Uniform.h"
#include "Visual/Renderer/Scene/View.h"

namespace Renderer
{
	namespace Scene
	{

		BoundingVolume::BoundingVolume(Type type)
			: type(type)
		{
			switch (type)
			{
			case Type::Frustum: new(&frustum) Frustum(); break;
			case Type::Sphere: new(&sphere) Sphere(); break;
			default: throw std::runtime_error("unknow view type");
			}
		}

		int BoundingVolume::Compare(const BoundingBox& bounding_box) const
		{
			switch (type)
			{
			case Type::Frustum: return CompareFrustum(bounding_box);
			case Type::Sphere: return CompareSphere(bounding_box);
			default: throw std::runtime_error("unknow view type");
			}
		}

		int BoundingVolume::CompareFrustum( const BoundingBox& bounding_box) const
		{
			return frustum.TestBoundingBox( bounding_box);
		}

		int BoundingVolume::CompareSphere( const BoundingBox& bounding_box) const
		{
			//this is currently broken for large spheres relative to box size
			float near_distance_squared = 0.0f;
			float far_distance_squared = 0.0f;
			float tn, tx;
			tn = sphere.center.x - bounding_box.minimum_point.x;
			tx = bounding_box.maximum_point.x - sphere.center.x;
			near_distance_squared += ( tn >= 0.0f ) ? 0.0f : tn * tn;
			near_distance_squared += ( tx >= 0.0f ) ? 0.0f : tx * tx;
			far_distance_squared += ( tn >= 0.0f ) ? tn * tn : 0.0f;
			far_distance_squared += ( tx >= 0.0f ) ? tx * tx : 0.0f;

			tn = sphere.center.y - bounding_box.minimum_point.y;
			tx = bounding_box.maximum_point.y - sphere.center.y;
			near_distance_squared += ( tn >= 0.0f ) ? 0.0f : tn * tn;
			near_distance_squared += ( tx >= 0.0f ) ? 0.0f : tx * tx;
			far_distance_squared += ( tn >= 0.0f ) ? tn * tn : 0.0f;
			far_distance_squared += ( tx >= 0.0f ) ? tx * tx : 0.0f;

			tn = sphere.center.z - bounding_box.minimum_point.z;
			tx = bounding_box.maximum_point.z - sphere.center.z;
			near_distance_squared += ( tn >= 0.0f ) ? 0.0f : tn * tn;
			near_distance_squared += ( tx >= 0.0f ) ? 0.0f : tx * tx;
			far_distance_squared += ( tn >= 0.0f ) ? tn * tn : 0.0f;
			far_distance_squared += ( tx >= 0.0f ) ? tx * tx : 0.0f;

			if( near_distance_squared >= sphere.squared_radius )
				return 0; //outside

			if( far_distance_squared > sphere.squared_radius )
				return 2; //intersects

			return 1; //contains
		}


		View::View(Type type)
			: BoundingVolume(type)
		{}

		ViewInfo View::BuildViewInfo(const simd::matrix& sub_scene_transform) const
		{
			ViewInfo info;
			info.view_matrix = sub_scene_transform * view_matrix;
			info.view_matrix_inv = info.view_matrix.inverse();
			info.projection_matrix = projection_matrix * post_projection_matrix;
			info.projection_matrix_inv = info.projection_matrix.inverse();
			info.view_projection_matrix_nopost = info.view_matrix * projection_matrix;
			info.view_projection_matrix = info.view_projection_matrix_nopost * post_projection_matrix;
			info.view_projection_matrix_inv = view_projection_matrix.inverse();
		#if defined(PS4)
			info.view_matrix = info.view_matrix.transpose();
			info.view_matrix_inv = info.view_matrix_inv.transpose();
			info.projection_matrix = info.projection_matrix.transpose();
			info.projection_matrix_inv = info.projection_matrix_inv.transpose();
			info.view_projection_matrix_nopost = info.view_projection_matrix_nopost.transpose();
			info.view_projection_matrix = info.view_projection_matrix.transpose();
			info.view_projection_matrix_inv = info.view_projection_matrix_inv.transpose();
		#endif
			info.camera_position = simd::vector4(sub_scene_transform.mulpos((simd::vector3&)camera_position), 0.0f);
			info.camera_right_vector = simd::vector4(info.view_matrix[0][0], info.view_matrix[1][0], info.view_matrix[2][0], 0.0f);
			info.camera_up_vector = simd::vector4(info.view_matrix[0][1], info.view_matrix[1][1], info.view_matrix[2][1], 0.0f);
			info.camera_forward_vector = simd::vector4(info.view_matrix[0][2], info.view_matrix[1][2], info.view_matrix[2][2], 0.0f);
			info.depth_scale = depth_scale;
			info.use_parabolic_projection = use_parabolic_projection ? 1.0f : 0.0f;
			return info;
		}

		ViewBounds View::BuildViewBounds(const simd::matrix& sub_scene_transform) const
		{
			ViewBounds bounds;
			bounds.frustum = Frustum(sub_scene_transform * view_projection_matrix);
			bounds.position = sub_scene_transform.mulpos((simd::vector3&)camera_position);
			bounds.tan_fov2 = projection_matrix[1][1];
			return bounds;
		}

	}
}

