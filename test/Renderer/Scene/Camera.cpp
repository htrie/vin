
#include "Visual/Device/State.h"
#include "Visual/Renderer/Scene/Camera.h"
#include "Visual/Renderer/DrawCalls/Uniform.h"
#include "Visual/Renderer/DrawCalls/DrawDataNames.h"
#include "BaseCamera.h"

namespace Renderer::Scene
{
		Camera::Camera() noexcept
			: View(View::Type::Frustum)
		{
		}

		BoundingVolume Camera::BuildBoundingVolume(const simd::matrix& sub_scene_transform) const
		{
			BoundingVolume res(BoundingVolume::Type::Frustum);
			res.SetFrustum(sub_scene_transform * view_projection_matrix);
			return res;
		}

		void Camera::UpdateMatrices( const simd::matrix&_view_matrix, const simd::matrix&_projection_matrix, const simd::matrix& post_projection, const simd::vector3& position, const simd::vector3& focus_point, float near_plane )
		{
			view_matrix = _view_matrix;
			projection_matrix = _projection_matrix;
			post_projection_matrix = post_projection;
			final_projection_matrix = _projection_matrix * post_projection;

			view_projection_matrix_nopost = view_matrix * projection_matrix;
			view_projection_matrix = view_projection_matrix_nopost * post_projection_matrix;

			camera_position = simd::vector4(position, 0.0f);
			camera_focus_point = simd::vector4(focus_point, 0.0f);
			depth_scale = ( near_plane > 0.f ) ? ( near_plane / 75.f ) : 1.f;

			const Frustum frustum(view_projection_matrix );
			SetFrustum(frustum);
			SetBoundingBox(frustum.ToBoundingBox());
		}

		std::pair< simd::vector3, simd::vector3 > Camera::ScreenPositionToRay( const int sx, const int sy ) const
		{
			const auto inverse_view = GetViewMatrix().inverse();
			const auto position = inverse_view.translation();

			if( IsOrthographicCamera() )
			{
				const float orthographic_scale = GetOrthographicScale();
				const float ray_x = ( sx - width / 2.0f ) * orthographic_scale;
				const float ray_y = -( sy - height / 2.0f ) * orthographic_scale;
				return std::make_pair( position + GetRightVector() * ray_x + GetUpVector() * ray_y, GetForwardVector() );
			}

			simd::vector3 v;
			v.x = ( ( sx * 2 ) / float( width ) - 1.0f ) / projection_matrix[0][0];
			v.y = -( ( sy * 2 ) / float( height ) - 1.0f ) / projection_matrix[1][1]; //Screen space is upside down from projection space
			v.z = 1.0f;

			const simd::vector3 direction( 
				v.x * inverse_view[0][0] + v.y * inverse_view[1][0] + v.z * inverse_view[2][0],
				v.x * inverse_view[0][1] + v.y * inverse_view[1][1] + v.z * inverse_view[2][1],
				v.x * inverse_view[0][2] + v.y * inverse_view[1][2] + v.z * inverse_view[2][2]
			);

			return std::make_pair( position, direction );
		}

		simd::vector2 Camera::WorldToScreen( const simd::vector3& world_position ) const
		{
			auto screen_position = view_projection_matrix * simd::vector4( world_position.x, world_position.y, world_position.z, 1.0f );

			screen_position[0] /= screen_position[3];
			screen_position[1] /= screen_position[3];

			// Change from post projection space to screen space
			const float half_width  = (width * 0.5f);
			const float half_height = (height * 0.5f);
			screen_position[0] = (screen_position[0] * half_width + half_width);
			screen_position[1] = (-screen_position[1] * half_height + half_height);

			return simd::vector2( screen_position[0], screen_position[1] );
		}

		simd::vector3 Camera::Project(const simd::vector3& world_position) const
		{
			simd::vector4 world_point4 = simd::vector4(world_position, 1.0f);
			simd::vector4 normalized_pos = view_projection_matrix * world_point4;
			normalized_pos = normalized_pos * (1.0f / normalized_pos.w);
			return simd::vector3(normalized_pos.x * 0.5f + 0.5f, 1.0f - (normalized_pos.y * 0.5f + 0.5f), normalized_pos.z);
		}


		bool Camera::IsPointBehindCamera( const simd::vector3& world_position ) const
		{
			return ( world_position - GetPosition() ).dot( GetForwardVector() ) < 1e-3f;
		}

		void Camera::WindowChanged( const unsigned width, const unsigned height )
		{
			this->width = width;
			this->height = height;
		}

		void Camera::DoScreenShake(const float frequency, const float amplitude, const float duration, const simd::vector3& axis, const simd::vector2& pos, const ShakeDampMode damp, const bool global, const bool always_on)
		{
			if (!(always_on || screen_shake_enabled) || (duration == 0.0f))
				return;

			float amp = amplitude;

			if (!global)
			{
				const auto dist_vec = simd::vector2(camera_focus_point.x, camera_focus_point.y) - pos;
				const auto dist = dist_vec.len();

				const float max_dist = 1000.0f;
				const float slope = 2.0f;

				amp *= Clamp((1.0f - (dist / max_dist)) * slope, 0.0f, 1.0f);

				if (amp <= 0.0f)
					return;
			}

			Memory::Lock lock(screen_shake_mutex);
			shakes.resize(shakes.size() + 1);
			auto& shake = shakes.back();
			shake.frequency = frequency;
			const auto total_amp = Accumulate( shakes, 0.01f, []( const float value, const ScreenShake& shake ) { return value + shake.amplitude; } );
			shake.amplitude = amp * std::min( 1.0f, amp / total_amp );
			shake.duration = duration;
			shake.time = 0.0;
			shake.axis = axis;
			shake.damp = damp;
			shake.always_on = always_on;
		}

		void Camera::SetScreenShakeEnabled(const bool enabled)
		{
			screen_shake_enabled = enabled;
			if (!screen_shake_enabled)
			{
				Memory::Lock lock(screen_shake_mutex);
				shakes.erase(std::remove_if(shakes.begin(), shakes.end(), [](const ScreenShake& shake)
				{
					return !shake.always_on;
				}), shakes.end());
			}
		}

		bool Camera::ApplyScreenShakes(simd::vector3& inout_pos, const float frametime)
		{
			Memory::Lock lock(screen_shake_mutex);

			simd::vector3 total_shake( 0.0f, 0.0f, 0.0f );

			bool changed = false;
			//float max_amp = 0.0f;
			for (auto shake = shakes.begin(); shake != shakes.end(); ++shake)
			{
				float shake_amount = sin(shake->time * shake->frequency) * shake->amplitude;
				if( shake->damp == ShakeDampMode::LinearDown )
					shake_amount *= 1.0f - ( std::min( shake->time, shake->duration ) / shake->duration );
				else if( shake->damp == ShakeDampMode::LinearUp )
					shake_amount *= std::min( shake->time, shake->duration ) / shake->duration;
				shake->time += frametime;
				total_shake += shake->axis * shake_amount;
				//max_amp = std::max( max_amp, shake->amplitude );
				changed = true;
			}

			//const auto magnitude = total_shake.len();
			//if( magnitude > 0.0f )
			//{
				//total_shake *= std::min( 1.0f, log( magnitude + max_amp ) / log( 1.75f ) / magnitude );
				inout_pos += total_shake;
			//}

			shakes.erase(std::remove_if(shakes.begin(), shakes.end(), [](const ScreenShake& shake)
			{
				return shake.time >= shake.duration;
			}), shakes.end());

			return changed;
		}
}
