#pragma once

#include "Common/Geometry/Bounding.h"
#include "Common/Utility/NonCopyable.h"

namespace Device
{
	struct UniformInputs;
}

namespace Renderer
{
	namespace Scene
	{
		struct ViewInfo // ORDER MATTERS (same as in Common.ffx).
		{
			simd::matrix view_projection_matrix = simd::matrix::identity();
			simd::matrix view_projection_matrix_inv = simd::matrix::identity();
			simd::matrix view_projection_matrix_nopost = simd::matrix::identity();
			simd::matrix view_matrix = simd::matrix::identity();
			simd::matrix view_matrix_inv = simd::matrix::identity();
			simd::matrix projection_matrix = simd::matrix::identity();
			simd::matrix projection_matrix_inv = simd::matrix::identity();
			simd::vector4 camera_position = 0.0f;
			simd::vector4 camera_right_vector = 0.0f;
			simd::vector4 camera_up_vector = 0.0f;
			simd::vector4 camera_forward_vector = 0.0f;
			simd::vector4 depth_scale = 1.0f;
			simd::vector4 use_parabolic_projection = 0.0f;
		};

		struct ViewBounds
		{
			Frustum frustum;
			simd::vector3 position = 0.0f;
			float tan_fov2 = 0.0f;
		};

		class BoundingVolume
		{
		public:
			enum class Type
			{
				Frustum,
				Sphere
			};

		protected:
			Type type;
			Frustum frustum;
			Sphere sphere;

			int CompareFrustum(const BoundingBox& bounding_box) const;
			int CompareSphere(const BoundingBox& bounding_box) const;

		public:
			BoundingVolume(Type type = Type::Frustum);

			void SetFrustum(const Frustum& frustum) { assert(type == Type::Frustum); this->frustum = frustum; }
			void SetSphere(const simd::vector3 center, float radius) { assert(type == Type::Sphere); sphere = Sphere(center, radius); }

			const Frustum& GetFrustum() const { assert(type == Type::Frustum); return frustum; }
			
			///Checks for collision between the view and a bounding box
			///@return 0 if no intersection
			///1 if bounding box is fully contained in the view
			///2 if the bounding box intersects the view.
			int Compare(const BoundingBox& bounding_box) const;
		};

		class View : public BoundingVolume
		{
		private:
			unsigned start_index = 0;

			BoundingBox bounding_box;

		protected:
			simd::matrix view_projection_matrix = simd::matrix::identity();
			simd::matrix view_projection_matrix_nopost = simd::matrix::identity();
			simd::matrix projection_matrix = simd::matrix::identity();
			simd::matrix post_projection_matrix = simd::matrix::identity();
			simd::matrix view_matrix = simd::matrix::identity();

			simd::vector4 camera_position = 0.0f;

			bool use_parabolic_projection = false;
			float depth_scale = 1.0f;

		public:
			View(Type type);

			void SetStartIndex(unsigned index) { start_index = index; }
			const unsigned& GetStartIndex() const { return start_index; } // Return reference here because we need the actual pointer for temp UniformInput.

			ViewInfo BuildViewInfo(const simd::matrix& sub_scene_transform) const;
			ViewBounds BuildViewBounds(const simd::matrix& sub_scene_transform) const;

			const simd::matrix& GetViewMatrix( ) const { return view_matrix; }
			const simd::matrix& GetProjectionMatrix( ) const { return projection_matrix; }
			const simd::matrix& GetViewProjectionMatrix() const { return view_projection_matrix; }

			const simd::vector3& GetPosition( ) const { return *(const simd::vector3*)&camera_position; }

			simd::vector3 GetUpVector( ) const { return simd::vector3(view_matrix[0][1], view_matrix[1][1], view_matrix[2][1]); }
			simd::vector3 GetForwardVector( ) const { return simd::vector3(view_matrix[0][2], view_matrix[1][2], view_matrix[2][2]); }
			simd::vector3 GetRightVector() const { return simd::vector3(view_matrix[0][0], view_matrix[1][0], view_matrix[2][0]); }

			void SetBoundingBox( const BoundingBox& _bounding_box ) { assert(_bounding_box.maximum_point.z >= _bounding_box.minimum_point.z); bounding_box = _bounding_box; }
			const BoundingBox& GetBoundingBox( ) const { return bounding_box; }
		};

	}

}