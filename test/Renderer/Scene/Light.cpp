
#include <cmath>

#include "Common/Utility/Logger/Logger.h"
#include "Common/Utility/Profiler/ScopedProfiler.h"
#include "Common/Geometry/Bounding.h"
#include "Common/Utility/Geometric.h"

#include "Visual/Device/State.h"
#include "Visual/Renderer/DrawCalls/DrawDataNames.h" 
#include "Visual/Renderer/DrawCalls/Uniform.h"
#include "Visual/Renderer/ShadowManager.h"

#include "Camera.h"
#include "Light.h"

namespace Renderer
{
	namespace Scene
	{

		Light::Light(Type type, View::Type view_type, const bool _shadow_map_enabled, UsageFrequency usage_frequency)
			: View(view_type)
			, type(type)
			, shadow_map_enabled(_shadow_map_enabled)
			, usage_frequency(usage_frequency)
		{
		}

		LightInfo Light::BuildLightInfo(const simd::matrix& sub_scene_transform) const
		{
			LightInfo info;
			info.light_matrix = sub_scene_transform * light_tex_matrix;
		#if defined(PS4)
			info.light_matrix = info.light_matrix.transpose();
		#endif
			info.light_position = simd::vector4(sub_scene_transform.mulpos((simd::vector3&)position), position.w);
			info.light_direction = simd::vector4(sub_scene_transform.muldir((simd::vector3&)direction), direction.w);
			info.light_color = colour;
			info.light_type = simd::vector4(GetFloatType());
			info.shadow_scale = shadow_scale;
			info.shadow_atlas_offset_scale = shadow_atlas_offset_scale;
			info.shadow_enabled = simd::vector4((shadow_map_enabled && shadow_atlas_offset_scale.z > 1e-3f) ? 1.0f : 0.0f);
			return info;
		}

		simd::vector4 Light::GetFloatType() const
		{
			switch (type)
			{
			case Type::Directional: return 0.0f;
			case Type::DirectionalLimited: return 0.0f;
			case Type::Spot: return 1.0f;
			case Type::Point: return 2.0f;
			case Type::Null: return 3.0f;
			default:
			case Type::Undefined: return 4.0f;
			}
		}

		void Light::SetColour(const simd::vector3& _colour)
		{
			simd::vector4 colour;
			colour.x = _colour.x;
			colour.y = _colour.y;
			colour.z = _colour.z;
			colour.w = this->colour.w; // preserve w component
			this->colour = colour;
		}

		void DirectionalLight::SetDirection(const simd::vector3& _direction)
		{
			auto direction = ((simd::vector3&)_direction).normalize();
			this->direction = simd::vector4(direction.x, direction.y, direction.z, 0.0f);
		}

		DirectionalLight::DirectionalLight(const simd::vector3& _direction, const simd::vector3& _colour, const float cam_near_percent, const float cam_far_percent, const float cam_extra_z, const bool _shadow_map_enabled, const float _min_offset, const float _max_offset, const bool cinematic_mode /*= false*/, float penumbra_radius, float penumbra_dist)
			: Light(Type::Directional, View::Type::Frustum, _shadow_map_enabled, UsageFrequency::Low)
			, cam_near_percent(cam_near_percent)
			, cam_far_percent(cam_far_percent)
			, cam_extra_z(cam_extra_z)
			, min_offset(_min_offset)
			, max_offset(_max_offset)
			, cinematic_mode(cinematic_mode)

		{
			SetDirection(_direction);

			simd::vector4 colour;
			colour.x = _colour.x;
			colour.y = _colour.y;
			colour.z = _colour.z;
			this->colour = colour;
			this->penumbra_radius = penumbra_radius;
			this->penumbra_dist = penumbra_dist;

			SetBoundingBox(BoundingBox(simd::vector3(-FLT_MAX, -FLT_MAX, -FLT_MAX), simd::vector3(FLT_MAX, FLT_MAX, FLT_MAX)));
		}

		BoundingVolume DirectionalLight::BuildBoundingVolume(const simd::matrix& sub_scene_transform, bool for_shadows) const
		{
			if (for_shadows)
			{
				BoundingVolume res(BoundingVolume::Type::Frustum);
				res.SetFrustum(sub_scene_transform * view_projection_matrix);
				return res;
			}

			BoundingVolume res(BoundingVolume::Type::Sphere);
			res.SetSphere(simd::vector3(0.0f), std::numeric_limits<float>::max()); // No need to transform with sub-scene since infinite anyway.
			return res;
		}

		struct LightView
		{
			simd::matrix view_transform;
			simd::matrix projection_transform;
			simd::matrix view_projection_transform;
			simd::vector4 shadowmap_scale;
		};

		LightView BuildCinematicLightView(const Camera& camera, simd::vector4 direction, simd::vector2_int shadow_map_size, BoundingBox light_bounding_box, float cam_extra_z, float cam_far_percent, float cam_near_percent)
		{
			simd::vector3 z_axis(0.0f, 0.0f, 1.0f);
			simd::vector3 light_direction(direction.x, direction.y, direction.z);

			auto rotation_axis = light_direction.cross(z_axis);

			float rotation_angle = acosf(light_direction.dot(z_axis));

			auto to_directional_light_space = simd::matrix::rotationaxis((simd::vector3&)rotation_axis, rotation_angle);

			const auto* frustum_points = (simd::vector3*)camera.GetFrustum().GetPoints();

			// get frustum box in lightspace
			simd::vector3 adjusted_frustum_points[8];
			for (unsigned i = 0; i < 4; ++i)
			{
				*(adjusted_frustum_points + (i * 2)) = (*(frustum_points + (i * 2 + 1))).lerp(*(frustum_points + (i * 2)), cam_far_percent);
				*(adjusted_frustum_points + (i * 2 + 1)) = (*(frustum_points + (i * 2 + 1))).lerp(*(frustum_points + (i * 2)), cam_near_percent);
			}
			simd::vector3 transformed_frustrum_points[8];
			for (unsigned i = 0; i < 8; ++i)
				transformed_frustrum_points[i] = to_directional_light_space.mulpos((simd::vector3&)adjusted_frustum_points[i]);
			BoundingBox frustum_lightspace_box;
			frustum_lightspace_box.fromPoints(transformed_frustrum_points, 8);

			// get light box in lightspace
			simd::vector3 light_worldspace_box_points[8];
			light_bounding_box.toCorners(light_worldspace_box_points);
			simd::vector3 light_lightspace_box_points[8];
			for (unsigned i = 0; i < 8; ++i)
				light_lightspace_box_points[i] = to_directional_light_space.mulpos(light_worldspace_box_points[i]);
			BoundingBox light_lightspace_box;
			light_lightspace_box.fromPoints(light_lightspace_box_points, 8);

			BoundingBox bounding_box = frustum_lightspace_box;
			bounding_box &= light_lightspace_box;

			bounding_box.minimum_point.z -= cam_extra_z;

			simd::vector3 camera_position(bounding_box.minimum_point.x * 0.5f + bounding_box.maximum_point.x * 0.5f, bounding_box.minimum_point.y * 0.5f + bounding_box.maximum_point.y * 0.5f, bounding_box.minimum_point.z);

			// align the light camera position to shadow texel size to reduce shadow flickering in camera movement
			float light_box_size_x = bounding_box.maximum_point.x - bounding_box.minimum_point.x,
				light_box_size_y = bounding_box.maximum_point.y - bounding_box.minimum_point.y;
			float shadow_texel_size_x = light_box_size_x / shadow_map_size.x,
				shadow_texel_size_y = light_box_size_y / shadow_map_size.y;
			camera_position.x = floorf(camera_position.x / shadow_texel_size_x) * shadow_texel_size_x;
			camera_position.y = floorf(camera_position.y / shadow_texel_size_y) * shadow_texel_size_y;


			auto translate_to_camera = simd::matrix::translation(-camera_position);

			const float far_plane = bounding_box.maximum_point.z - bounding_box.minimum_point.z;
			auto orthographic_projection = simd::matrix::orthographic(bounding_box.maximum_point.x - bounding_box.minimum_point.x, bounding_box.maximum_point.y - bounding_box.minimum_point.y, ShadowManager::near_plane, far_plane);

			auto scale = simd::matrix::scale(simd::vector3(far_plane));
			scale[3][3] = far_plane;

			LightView res;
			res.view_transform = to_directional_light_space * translate_to_camera;
			res.projection_transform = orthographic_projection * scale;
			res.view_projection_transform = to_directional_light_space * translate_to_camera * orthographic_projection * scale;

			float world_penumbra_size = 1.0f;
			res.shadowmap_scale = simd::vector4(world_penumbra_size / light_box_size_x, world_penumbra_size / light_box_size_y, ShadowManager::depth_bias, 0);

			return res;
		}

		struct IntersectionPoint
		{
			bool exists;
			simd::vector3 point;
		};
		IntersectionPoint EdgePlaneIntersect(simd::vector3 edge_points[2], simd::vector3 plane_point, simd::vector3 plane_norm)
		{
			float param = (plane_point - edge_points[0]).dot(plane_norm) / (edge_points[1] - edge_points[0]).dot(plane_norm);
			return { param >= 0.0f && param < 1.0f, edge_points[0] + (edge_points[1] - edge_points[0]) * param };
		}

		struct Basis
		{
			simd::vector3 dir;
			simd::vector3 tangents[2];
		};

		simd::matrix CreateAxisBasis(simd::vector3 dir, float basis_ang)
		{
			simd::vector3 z_axis(0.0f, 0.0f, 1.0f);

			simd::vector3 basis[2];
			basis[0] = dir.cross(z_axis).normalize();
			basis[1] = dir.cross(basis[0]).normalize();

			Basis res;
			res.dir = dir;
			simd::vector2 local_coord = simd::vector2(cos(basis_ang), sin(basis_ang));
			simd::vector3 tangents[2];
			tangents[0] = basis[0] * local_coord.x + basis[1] * local_coord.y;
			tangents[1] = basis[0] * -local_coord.y + basis[1] * local_coord.x;

			return simd::matrix(
				simd::vector4(tangents[0], 0.0f),
				simd::vector4(tangents[1], 0.0f),
				simd::vector4(dir, 0.0f),
				simd::vector4(0.0f, 0.0f, 0.0f, 1.0f)).inverse();
		}
		struct EdgeIndices
		{
			size_t indices[2];
		};
		const size_t edges_count = 12;
		EdgeIndices edges[edges_count] =
		{
			//forward-facing edges
			{0, 1},
			{2, 3},
			{4, 5},
			{6, 7},

			//updward-facing edges
			{0, 2},
			{1, 3},
			{6, 4},
			{7, 5},

			//right-facing edges
			{0, 6},
			{2, 4},
			{3, 5},
			{1, 7}
		};

		std::vector<simd::vector3> BuildFrustumPlaneIntersection(const simd::vector3* frustum_points, simd::vector3 plane_point, simd::vector3 plane_norm, float plane_min_offset, float plane_max_offset)
		{
			std::vector<simd::vector3> intersection_points;
			for (size_t edge_index = 0; edge_index < edges_count; edge_index++)
			{
				simd::vector3 edge_points[] = {
					frustum_points[edges[edge_index].indices[0]],
					frustum_points[edges[edge_index].indices[1]] };

				auto int_min = EdgePlaneIntersect(edge_points, plane_point + plane_norm * plane_min_offset, plane_norm);
				if (int_min.exists)
					intersection_points.push_back(int_min.point);

				auto int_max = EdgePlaneIntersect(edge_points, plane_point + plane_norm * plane_max_offset, plane_norm);
				if (int_max.exists)
					intersection_points.push_back(int_max.point);
			}

			for (size_t point_index = 0; point_index < 8; point_index++)
			{
				float proj = (frustum_points[point_index] - plane_point).dot(plane_norm);
				if (proj > plane_min_offset && proj < plane_max_offset)
					intersection_points.push_back(frustum_points[point_index]);
			}

			return intersection_points;
		}

		simd::vector2 SnapGridCoord(simd::vector2 coord, simd::vector2 grid_step)
		{
			return simd::vector2(
				floorf(coord.x / grid_step.x) * grid_step.x,
				floorf(coord.y / grid_step.y) * grid_step.y);
		}

		LightView BuildDirectionalLightView(const Camera& camera, simd::vector4 direction, float basis_ang, simd::vector3 player_position, simd::vector2_int shadow_map_size, float cam_extra_z, float min_offset, float max_offset)
		{
			simd::matrix world_to_light_view = CreateAxisBasis(simd::vector3(direction.x, direction.y, direction.z), basis_ang);

			const auto* frustum_points = (simd::vector3*)camera.GetFrustum().GetPoints();
			std::vector<simd::vector3> intersection_world_points = BuildFrustumPlaneIntersection(frustum_points, player_position, simd::vector3(0.0f, 0.0f, 1.0f), min_offset, max_offset);

			/*for (size_t point_index = 0; point_index < 8; point_index++)
			{
				light_view_points.push_back(world_to_light_view.mulpos(frustum_points[point_index]));
			}*/
			simd::vector3 light_view_min = 1e7f;
			simd::vector3 light_view_max = -1e7f;
			for (auto& world_point : intersection_world_points)
			{
				auto view_point = world_to_light_view.mulpos(world_point);

				light_view_min.x = std::min(light_view_min.x, view_point.x);
				light_view_min.y = std::min(light_view_min.y, view_point.y);
				light_view_min.z = std::min(light_view_min.z, view_point.z);

				light_view_max.x = std::max(light_view_max.x, view_point.x);
				light_view_max.y = std::max(light_view_max.y, view_point.y);
				light_view_max.z = std::max(light_view_max.z, view_point.z);
			}


			simd::vector3 box_center = (light_view_min + light_view_max) * 0.5f;
			simd::vector2 translation_vec = { box_center.x, box_center.y };

			simd::vector2 shadowmap_world_size = {
				light_view_max.x - light_view_min.x,
				light_view_max.y - light_view_min.y };
			simd::vector2 shadowmap_world_texel_size = shadowmap_world_size * simd::vector2(1.0f / float(shadow_map_size.x), 1.0f / float(shadow_map_size.y));

			simd::vector2 snapped_translation_vec = SnapGridCoord(translation_vec, shadowmap_world_texel_size);
			simd::matrix light_view_translation = simd::matrix::translation(simd::vector3(-snapped_translation_vec.x, -snapped_translation_vec.y, 0.0f));

			auto view_to_proj = simd::matrix::orthographic(shadowmap_world_size.x, shadowmap_world_size.y, light_view_min.z - cam_extra_z, light_view_max.z);

			LightView res;
			res.view_transform = world_to_light_view * light_view_translation;
			res.projection_transform = view_to_proj;
			res.view_projection_transform = world_to_light_view * light_view_translation * view_to_proj;

			float world_penumbra_size = 1.0f;
			res.shadowmap_scale = simd::vector4(world_penumbra_size / shadowmap_world_size.x, world_penumbra_size / shadowmap_world_size.y, ShadowManager::depth_bias, 0);
			return res;
		}

		float GetDim(simd::vector4 vec)
		{
			simd::vector3 vec3 = { vec.x, vec.y, vec.z };
			simd::vector2 vec2 = { vec.x, vec.y };
			return vec2.len();
		}

		float GetLightMatrixArea(simd::matrix light_matrix)
		{
			simd::matrix light_to_world = light_matrix.inverse();

			float width = GetDim(
				light_to_world * simd::vector4(1.0f, 0.0f, 0.0f, 1.0f) -
				light_to_world * simd::vector4(-1.0f, 0.0f, 0.0f, 1.0f));
			float height = GetDim(
				light_to_world * simd::vector4(0.0f, 1.0f, 0.0f, 1.0f) -
				light_to_world * simd::vector4(0.0f, -1.0f, 0.0f, 1.0f));
			return width * height;
		}

		void DirectionalLight::UpdateLightMatrix(const Camera& camera, simd::vector2_int shadow_map_size)
		{
			LightView light_view;
			if (cinematic_mode)
				light_view = BuildCinematicLightView(camera, GetDirection(), shadow_map_size, GetBoundingBox(), cam_extra_z, cam_far_percent, cam_near_percent);
			else
			{
				float best_area = -1.0f;
				for (float ang = 0.0f; ang < PI * 0.5f; ang += 0.1f)
				{
					auto test_view = BuildDirectionalLightView(camera, GetDirection(), ang, camera.GetFocusPoint(), shadow_map_size, cam_extra_z, min_offset, max_offset);
					float area = GetLightMatrixArea(test_view.view_projection_transform);
					if (best_area < 0.0f || area < best_area)
					{
						light_view = test_view;
						best_area = area;
					}
				}
			}

			view_matrix = light_view.view_transform;
			projection_matrix = light_view.projection_transform;
			view_projection_matrix = light_view.view_projection_transform; //draw data matrices are transposed
			shadow_scale = light_view.shadowmap_scale * penumbra_radius;
			shadow_scale.w = 1.0f / std::max(1e-5f, penumbra_dist);

			auto tex_matrix = simd::matrix::identity();
			tex_matrix[0][0] = 0.5f; // x scale
			tex_matrix[1][1] = -0.5f; // y scale
			tex_matrix[3][0] = 0.5f; // x offset
			tex_matrix[3][1] = 0.5f; // y offset
			light_tex_matrix = view_projection_matrix * tex_matrix;

			SetFrustum(Frustum(view_projection_matrix));
		}

		DirectionalLightLimited::DirectionalLightLimited(const simd::vector3& _position, const simd::vector3& _direction, const simd::vector3& _colour, const float cam_near_percent, const float cam_far_percent, const float cam_extra_z, const bool _shadow_map_enabled, const float _min_offset, const float _max_offset, const BoundingBox& box)
			: DirectionalLight(_direction, _colour, cam_near_percent, cam_far_percent, cam_extra_z, _shadow_map_enabled, _min_offset, _max_offset)
		{
			SetBoundingBox(box);
		}

		BoundingVolume DirectionalLightLimited::BuildBoundingVolume(const simd::matrix& sub_scene_transform, bool for_shadows) const
		{
			BoundingVolume res(BoundingVolume::Type::Frustum);
			res.SetFrustum(sub_scene_transform * view_projection_matrix);
			return res;
		}

		void DirectionalLightLimited::UpdateLightMatrix(const Camera& camera, simd::vector2_int shadow_map_size)
		{
			const simd::vector3 z_axis(0.0f, 0.0f, 1.0f);
			const simd::vector3 light_direction(direction.x, direction.y, direction.z);
			const auto rotation_axis = light_direction.cross(z_axis);
			const float rotation_angle = acosf(light_direction.dot(z_axis));
			const auto to_directional_light_space = simd::matrix::rotationaxis((simd::vector3&)rotation_axis, rotation_angle);

			const auto* frustum_points = (simd::vector3*)camera.GetFrustum().GetPoints();

			// get frustum box in lightspace
			simd::vector3 adjusted_frustum_points[8];
			for (unsigned i = 0; i < 4; ++i)
			{
				*(adjusted_frustum_points + (i * 2)) = (*(frustum_points + (i * 2 + 1))).lerp(*(frustum_points + (i * 2)), cam_far_percent);
				*(adjusted_frustum_points + (i * 2 + 1)) = (*(frustum_points + (i * 2 + 1))).lerp(*(frustum_points + (i * 2)), cam_near_percent);
			}
			simd::vector3 transformed_frustrum_points[8];
			for (unsigned i = 0; i < 8; ++i)
				transformed_frustrum_points[i] = to_directional_light_space.mulpos((simd::vector3&)adjusted_frustum_points[i]);
			BoundingBox frustum_lightspace_box;
			frustum_lightspace_box.fromPoints(transformed_frustrum_points, 8);

			// get light box in lightspace
			simd::vector3 light_worldspace_box_points[8];
			GetBoundingBox().toCorners(light_worldspace_box_points);
			simd::vector3 light_lightspace_box_points[8];
			for (unsigned i = 0; i < 8; ++i)
				light_lightspace_box_points[i] = to_directional_light_space.mulpos(light_worldspace_box_points[i]);
			BoundingBox light_lightspace_box;
			light_lightspace_box.fromPoints(light_lightspace_box_points, 8);

			BoundingBox bounding_box = frustum_lightspace_box; bounding_box &= light_lightspace_box;

			bounding_box.minimum_point.z -= cam_extra_z;

			simd::vector3 camera_position(bounding_box.minimum_point.x * 0.5f + bounding_box.maximum_point.x * 0.5f, bounding_box.minimum_point.y * 0.5f + bounding_box.maximum_point.y * 0.5f, bounding_box.minimum_point.z);

			// align the light camera position to shadow texel size to reduce shadow flickering in camera movement
			float light_box_size_x = bounding_box.maximum_point.x - bounding_box.minimum_point.x,
				light_box_size_y = bounding_box.maximum_point.y - bounding_box.minimum_point.y;
			float shadow_texel_size_x = light_box_size_x / shadow_map_size.x,
				shadow_texel_size_y = light_box_size_y / shadow_map_size.y;
			camera_position.x = floorf(camera_position.x / shadow_texel_size_x) * shadow_texel_size_x;
			camera_position.y = floorf(camera_position.y / shadow_texel_size_y) * shadow_texel_size_y;

			simd::vector2 shadow_map_size_factor = simd::vector2(1024.0f / shadow_map_size.x, 1024.0f / shadow_map_size.y);
			shadow_scale = simd::vector4(light_box_size_x * shadow_map_size_factor.x, light_box_size_y * shadow_map_size_factor.y, ShadowManager::depth_bias, 1e5f);

			auto translate_to_camera = simd::matrix::translation(-camera_position);

			const float far_plane = bounding_box.maximum_point.z - bounding_box.minimum_point.z;
			auto orthographic_projection = simd::matrix::orthographic(bounding_box.maximum_point.x - bounding_box.minimum_point.x, bounding_box.maximum_point.y - bounding_box.minimum_point.y, ShadowManager::near_plane, far_plane);

			auto scale = simd::matrix::scale(simd::vector3(far_plane));
			scale[3][3] = far_plane;

			view_matrix = to_directional_light_space * translate_to_camera;
			projection_matrix = orthographic_projection * scale;
			view_projection_matrix = to_directional_light_space * translate_to_camera * orthographic_projection * scale;

			auto tex_matrix = simd::matrix::identity();
			tex_matrix[0][0] = 0.5f; // x scale
			tex_matrix[1][1] = -0.5f; // y scale
			tex_matrix[3][0] = 0.5f; // x offset
			tex_matrix[3][1] = 0.5f; // y offset
			light_tex_matrix = view_projection_matrix * tex_matrix;

			SetFrustum(Frustum(view_projection_matrix));
		}

		void DirectionalLight::SetCamProperties(const float cam_near_percent, const float cam_far_percent, const float cam_extra_z, const float min_offset, const float max_offset, const bool cinematic_mode /*= false*/, float penumbra_radius, float penumbra_dist)
		{
			this->cam_near_percent = cam_near_percent;
			this->cam_far_percent = cam_far_percent;
			this->cam_extra_z = cam_extra_z;
			this->min_offset = min_offset;
			this->max_offset = max_offset;
			this->cinematic_mode = cinematic_mode;
			this->penumbra_radius = penumbra_radius;
			this->penumbra_dist = penumbra_dist;
		}

		float DirectionalLightLimited::Importance(const BoundingBox& bounding_box) const
		{
			return (GetBoundingBox().Intersect(bounding_box) != BoundingBox::None) ? std::numeric_limits<float>::infinity() : -std::numeric_limits<float>::infinity(); // if boxes intersect then set highest priority, otherwise set lowest
		}

		SpotLight::SpotLight(const simd::vector3& _position, const simd::vector3& _direction, const simd::vector3& _colour, const float distance, const float penumbra, const float umbra, const bool _shadow_map_enabled, bool player_light)
			: Light(Type::Spot, View::Type::Frustum, _shadow_map_enabled, UsageFrequency::Low)
			, player_light(player_light)
			, cone_radius(0.f)
		{
			SetLightParams(_position, _direction, _colour, distance, penumbra, umbra);
		}

		BoundingVolume SpotLight::BuildBoundingVolume(const simd::matrix& sub_scene_transform, bool for_shadows) const
		{
			BoundingVolume res(BoundingVolume::Type::Frustum);
			res.SetFrustum(sub_scene_transform * view_projection_matrix);
			return res;
		}

		bool FuzzyCompare(const simd::vector3& a, const simd::vector3& b, const float tolerance)
		{
			return fabs(a.x - b.x) + fabs(a.y - b.y) + fabs(a.z - b.z) < tolerance;
		}

		void SpotLight::SetLightParams(const simd::vector3& _position, const simd::vector3& _direction, const simd::vector3& _colour, const float distance, const float penumbra, const float umbra)
		{
			simd::vector4 position;
			position.x = _position.x;
			position.y = _position.y;
			position.z = _position.z;
			position.w = cos(penumbra);
			this->position = position;

			auto direction = ((simd::vector3&)_direction).normalize();
			this->direction = simd::vector4(direction.x, direction.y, direction.z, cos(umbra));

			simd::vector4 colour;
			colour.x = _colour.x;
			colour.y = _colour.y;
			colour.z = _colour.z;
			colour.w = distance;
			this->colour = colour;

			// Make the up vector as axis-aligned as possible for the most efficient bounding box size
			simd::vector3 up;
			float abs_dir_x = std::fabs(_direction.x);
			float abs_dir_y = std::fabs(_direction.y);
			float abs_dir_z = std::fabs(_direction.z);
			if (_direction.x == 0.f && _direction.y == 0.f && _direction.z == 0.f)
			{
				up = simd::vector3(1.0f, 0.0f, 0.0f);
			}
			else if (abs_dir_x <= abs_dir_y && abs_dir_x <= abs_dir_z)
			{
				up.x = 0.f;
				up.y = -_direction.z;
				up.z = _direction.y;
			}
			else if (abs_dir_y <= abs_dir_x && abs_dir_y <= abs_dir_z)
			{
				up.x = -_direction.z;
				up.y = 0.f;
				up.z = _direction.x;
			}
			else
			{
				up.x = -_direction.y;
				up.y = _direction.x;
				up.z = 0.f;
			}

			up = up.normalize();

			float spot_blur_radius = 0.003f;
			// set depth bias value for spotlights
			shadow_scale = simd::vector4(spot_blur_radius, spot_blur_radius, ShadowManager::depth_bias, 1e5f);

			view_matrix = simd::matrix::lookat(_position, _position + _direction, up);
			projection_matrix = simd::matrix::perspectivefov(umbra * 2, 1.0f, ShadowManager::near_plane, distance < FLT_EPSILON ? FLT_EPSILON : distance);
			view_projection_matrix = view_matrix * projection_matrix;

			simd::matrix tex_matrix = simd::matrix::identity();
			tex_matrix[0][0] = 0.5f; // x scale
			tex_matrix[1][1] = -0.5f; // y scale
			tex_matrix[3][0] = 0.5f; // x offset
			tex_matrix[3][1] = 0.5f; // y offset
			light_tex_matrix = view_projection_matrix * tex_matrix;

			Frustum frustum(view_projection_matrix);
			SetBoundingBox(frustum.ToBoundingBox());

			auto extents = frustum.GetPoint(1) - frustum.GetPoint(7);
			cone_radius = extents.len() * 0.5f;

			SetFrustum(frustum);
		}

		simd::vector3 GetClosestPointOnLineSegment(const simd::vector3& line_begin, const simd::vector3& line_end, const simd::vector3& point)
		{
			if ((line_end - line_begin).sqrlen() < FLT_EPSILON)
				return line_begin;

			const auto l = line_end - line_begin;
			const auto lnorm = l.normalize();
			const auto pl = point - line_begin;
			const float distance = lnorm.dot(pl);
			const float r = distance / l.len();
			if (r <= 0)
			{
				return line_begin;
			}
			else if (r >= 1)
			{
				return line_end;
			}
			else
			{
				return line_begin + lnorm * distance;
			}
		}

		float SpotLight::Importance(const BoundingBox& bounding_box) const
		{
			// Make the player light be the highest priority always ( along with the directional light )
			if (IsPlayerLight())
				return std::numeric_limits< float >::infinity();

			// get the shortest distance to the cone's line
			const auto obj_pos = bounding_box.ToAABB().center;
			const auto cone_start = simd::vector3(position.x, position.y, position.z);
			const auto cone_end = cone_start + simd::vector3(direction.x, direction.y, direction.z) * colour.w;
			const simd::vector3 closest_point = GetClosestPointOnLineSegment(cone_start, cone_end, obj_pos);
			float nearest_distance = (closest_point - obj_pos).len();

			// get the radius based on the nearest point on the cone
			const float factor = (closest_point - cone_start).sqrlen() / (cone_end - cone_start).sqrlen();
			const float nearest_radius = factor * cone_radius;

			// If inside the spotlight cone, then calculate importance based from the light's origin ( same as point light ). Else, base from the nearest distance and radius from the cone's line
			float distance, radius;
			if (nearest_distance < nearest_radius)
			{
				distance = (obj_pos - cone_start).len();
				radius = colour.w;
			}
			else
			{
				distance = nearest_distance;
				radius = nearest_radius;
			}

			// Importance is ( Radius - Distance )
			return radius - distance;
		}

		static float GetPointCutoffRadius(simd::vector3 color, float median_radius)
		{
			float color_threshold = 0.1f;
			float zero_intensity_mult = 0.2f;
			float color_intensity = color.dot(simd::vector3(1.0f, 1.0f, 1.0f));
			float cutoff_radius = median_radius * sqrt(color_intensity * zero_intensity_mult / color_threshold);
			return cutoff_radius;
		}

		std::atomic<int> curr_point_light_index = 0;
		int point_light_cannels_count = 1;
		uint32_t parabolic_light_profile = 42;

		PointLight::PointLight(const simd::vector3& _position, const simd::vector3& _colour, const float median_radius, const bool _shadow_map_enabled, UsageFrequency _usage_frequency /*= UsageFrequency::Low*/, char _shadow_map_channel)
			: Light(Type::Point, View::Type::Sphere, _shadow_map_enabled, _usage_frequency)
			, dist_threshold(150.0f)
		{
			simd::vector4 position;
			position.x = _position.x;
			position.y = _position.y;
			position.z = _position.z;
			position.w = float(_shadow_map_channel);

			this->position = position;

			simd::vector4 direction;
			direction.x = 0.0f;
			direction.y = 0.0f;
			direction.z = 1.0f;
			direction.w = -1.f;
			this->direction = direction;

			simd::vector4 colour;
			colour.x = _colour.x;
			colour.y = _colour.y;
			colour.z = _colour.z;
			colour.w = median_radius;
			this->colour = colour;

			this->dist_threshold = median_radius * 0.1f;
			this->profile = 0;
			use_parabolic_projection = _shadow_map_enabled;

			UpdateState();
		}

		BoundingVolume PointLight::BuildBoundingVolume(const simd::matrix& sub_scene_transform, bool for_shadows) const
		{
			BoundingVolume res(BoundingVolume::Type::Sphere);
			res.SetSphere(sub_scene_transform.mulpos((simd::vector3&)position), sub_scene_transform.muldir(simd::vector3(std::sqrt(GetCutoffRadius()), 0.0f, 0.0f)).len());
			return res;
		}

		void PointLight::UpdateState()
		{
			light_tex_matrix = simd::matrix::translation(-simd::vector3(position.x, position.y, position.z));

			view_matrix = light_tex_matrix;
			projection_matrix = simd::matrix::identity();
			view_projection_matrix = light_tex_matrix;

			float shadow_blur_radius = this->penumbra_radius;
			float shadow_blur_inv_dist = 1.0f / std::max(1e-5f, this->penumbra_dist);
			shadow_scale = simd::vector4(shadow_blur_radius, shadow_blur_radius, 0.0f, shadow_blur_inv_dist);

			float cutoff_radius = GetCutoffRadius();
			simd::vector3 pos = simd::vector3(position.x, position.y, position.z);
			simd::vector3 offset(cutoff_radius, cutoff_radius, cutoff_radius);
			SetBoundingBox(BoundingBox(pos - offset, pos + offset));

			SetSphere(pos, cutoff_radius);
		}

		float PointLight::GetDistThreshold() const
		{
			return dist_threshold;
		}

		float PointLight::GetCutoffRadius() const
		{
			simd::vector4 c = colour;
			return GetPointCutoffRadius(simd::vector3(c.x, c.y, c.z), c.w);
		}

		void PointLight::SetDistThreshold(float value)
		{
			dist_threshold = value;
		}

		void PointLight::SetPosition(const simd::vector3& new_position, const simd::vector3& new_direction, const float median_radius)
		{
			const auto& colour_vec = colour;
			SetPosition(new_position, new_direction, median_radius, simd::vector3(colour_vec.x, colour_vec.y, colour_vec.z));
		}

		void PointLight::SetPosition(const simd::vector3& new_position, const simd::vector3& new_direction, const float median_radius, const simd::vector3& colour)
		{
			float shadow_channel_index = this->position.w;
			const simd::vector4 position(new_position.x, new_position.y, new_position.z, shadow_channel_index);
			this->position = position;
			this->direction = simd::vector4(new_direction, 0.0f);
			this->colour = simd::vector4(colour.x, colour.y, colour.z, median_radius);

			UpdateState();
		}

		uint32_t PointLight::GetProfile() const
		{
			return profile;
		}

		void PointLight::SetProfile(uint32_t value)
		{
			profile = value;
		}

		void PointLight::SetPenumbra(float penumbra_radius, float penumbra_dist)
		{
			this->penumbra_radius = penumbra_radius;
			this->penumbra_dist = penumbra_dist;
		}

		float PointLight::GetMedianRadius() const
		{
			return colour.w;
		}

		void PointLight::SetMedianRadius( float median_radius )
		{
			colour.w = median_radius;

			float cutoff_radius = GetCutoffRadius();
			// warning: written by Rian!
			const auto pos4 = position;
			simd::vector3 pos3( pos4.x, pos4.y, pos4.z );
			simd::vector3 offset(cutoff_radius, cutoff_radius, cutoff_radius);
			SetBoundingBox( BoundingBox( pos3 - offset, pos3 + offset ) );

			SetSphere(pos3, cutoff_radius);
		}

		float PointLight::Importance( const BoundingBox& bounding_box ) const
		{
			//Radius - Distance from ceteroid.
			const auto& pos = position;
			const auto dist = bounding_box.ToAABB().center - simd::vector3( pos.x, pos.y, pos.z );

			return shadow_map_enabled ? std::numeric_limits<float>::infinity() : colour.w - dist.len();
		}

		PointLightInfo PointLight::BuildPointLightInfo(const simd::matrix& sub_scene_transform) const
		{
			PointLightInfo info;
			info.position = sub_scene_transform.mulpos((simd::vector3&)position);
			info.channel_index = position.w;
			info.direction = sub_scene_transform.muldir((simd::vector3&)direction);
			info.dist_threshold = sub_scene_transform.muldir(simd::vector3(GetDistThreshold(), 0.0f, 0.0f)).len();
			info.color = (simd::vector3&)colour;
			info.median_radius = sub_scene_transform.muldir(simd::vector3(GetMedianRadius(), 0.0f, 0.0f)).len();
			info.cutoff_radius = sub_scene_transform.muldir(simd::vector3(GetCutoffRadius(), 0.0f, 0.0f)).len();
			return info;
		}


		NullLight::NullLight()
			: Light(Type::Null, View::Type::Sphere, false, UsageFrequency::Low)
		{
			simd::vector4 zero(0.f, 0.f, 0.f, 0.f);
			position = zero;
			direction = zero;
			colour = zero;
		}

		BoundingVolume NullLight::BuildBoundingVolume(const simd::matrix& sub_scene_transform, bool for_shadows) const
		{
			return BoundingVolume(BoundingVolume::Type::Sphere); // Empty sphere at origin.
		}

		float NullLight::Importance(const BoundingBox & bounding_box) const
		{
			return 0.0f;
		}
	}
}
