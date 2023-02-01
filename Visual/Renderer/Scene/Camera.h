#pragma once

#include "Visual/Renderer/Scene/View.h"

class Frustum;

namespace Device
{
	struct UniformInputs;
}

namespace Renderer::Scene
{
	class Light;

	///Base class for a camera that views objects in the scene
	class Camera : public View
	{
	public:
		BoundingVolume BuildBoundingVolume(const simd::matrix& sub_scene_transform) const;

		struct DoFSettings
		{
			float focus_distance = 0.0f;
			float focus_range = 0.0f;
			float max_focus_distance = 0.0f;
		};

		void WindowChanged( const unsigned width, const unsigned height );
	
		const simd::matrix& GetFinalProjectionMatrix( ) const { return final_projection_matrix; }

		simd::vector2_int GetViewportSize() const { return simd::vector2_int(width, height); }

		///Generate a ray from the given screen space co-ordinates
		///@return A tuple of ( position, direction ) representing a ray
		virtual std::pair< simd::vector3, simd::vector3 > ScreenPositionToRay( const int sx, const int sy ) const;

		///For changing near/far planes at runtime
		virtual void SetNearFarPlanes( const float near_plane, const float far_plane ) {}
		virtual void SetZRotationOffset( const float z_rot ) {}
		void SetZRotationEnabled( const bool enabled ) { zrotation_enabled = enabled; }

		//Tools only
		virtual bool IsOrthographicCamera() const { return false; }
		virtual void SetOrthographicCamera( const bool enable ) {}
		virtual float GetOrthographicScale() const { return 1.0f; }

		/// Transforms the given world position into screen space.
		/// @return A 2D vector of the calculated screen coordinates.
		simd::vector2 WorldToScreen( const simd::vector3& world_position ) const;
		simd::vector3 Project(const simd::vector3& world_position) const;

		bool IsPointBehindCamera( const simd::vector3& world_position ) const;

		const simd::vector3& GetFocusPoint() const { return *(const simd::vector3*)&camera_focus_point; }

		enum class ShakeDampMode : unsigned char
		{
			NoDampening,
			LinearDown,
			LinearUp
		};
		///@param lateral If true, then the stake is applied laterally in the direction of the supplied angle
		void DoScreenShake(const float frequency, const float amplitude, const float duration, const simd::vector3& axis, const simd::vector2& pos, const ShakeDampMode damp, const bool global, const bool always_on);
		bool GetScreenShakeEnabled() const { return screen_shake_enabled; }
		void SetScreenShakeEnabled(const bool enabled);
		bool ApplyScreenShakes(simd::vector3& inout_pos, const float frametime);

		void SetDoFSettings(const DoFSettings& settings) { dof_settings = settings; }
		const DoFSettings& GetDoFSettings() const { return dof_settings; }

	protected:
		//Must be derived from
		Camera() noexcept;

		void UpdateMatrices( const simd::matrix &view_matrix, const simd::matrix &projection_matrix, const simd::matrix& post_projection, const simd::vector3& position, const simd::vector3& focus_point, const float near_plane );

		unsigned width = 0, height = 0;

		struct ScreenShake
		{
			simd::vector3 axis = 0.0f;
			float frequency = 0.0f, amplitude = 0.0f, duration = 0.0f, time = 0.0f;
			bool always_on = false;
			ShakeDampMode damp = ShakeDampMode::NoDampening;
		};
		std::vector< ScreenShake > shakes;
		bool screen_shake_enabled = true;
		bool zrotation_enabled = true;
		Memory::Mutex screen_shake_mutex;

	private:
		simd::matrix final_projection_matrix = simd::matrix::identity();
		simd::vector4 camera_focus_point = 0.0f;

		DoFSettings dof_settings;

		//Remove copy and assign
		const Camera& operator=( const Camera& camera ) = delete;
		Camera( const Camera& ) = delete;
	};

	class DummyCamera : public Camera
	{
	public:
		DummyCamera() noexcept {};
	};

}
