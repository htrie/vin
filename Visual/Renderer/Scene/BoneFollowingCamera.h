#pragma once

#include "Visual/Renderer/Scene/Camera.h"

namespace Animation
{
	class AnimatedObject;
	namespace Components
	{
		class ClientAnimationController;
		class AnimatedRender;
	}
};

namespace Renderer
{
	namespace Scene
	{
		class BoneFollowingCamera : public Camera
		{
		public:
			BoneFollowingCamera( const ::Animation::AnimatedObject& animated_object, const unsigned bone_index, const float camera_fov_height, const float camera_fov_width, const float z_near, const float z_far, const unsigned window_width, const unsigned window_height );

			void WindowChanged( const unsigned width, const unsigned height );

			void FrameMove( const float elapsed_time );

			virtual void SetNearFarPlanes(const float near_plane, const float far_plane) override;

		private:
			void CalculateViewAndPosition( simd::matrix& out, simd::vector3& position );

			const ::Animation::Components::ClientAnimationController *const client_animation_controller;
			const ::Animation::Components::AnimatedRender *const animated_render;

			const unsigned bone_index;
			float z_near, z_far;
			float fov_height, fov_width;
		};
	}
}
