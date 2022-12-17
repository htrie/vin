#pragma once
#include "Common/Resource/DeviceUploader.h"
#include "Common/Resource/Handle.h"

class Material;
typedef Resource::DeferredHandle<Material> MaterialDeferredHandle;

namespace Resource
{
	class DeviceUploader;
}
namespace Mesh
{
	class FixedMesh;
	typedef Resource::DeferredHandle< FixedMesh, Resource::DeviceUploader > FixedMeshDeferredHandle;
}

namespace GpuParticles
{
	enum class FaceLock
	{
		None,
		Camera,
		Camera_Z,
		Camera_Fixed,
		Camera_Velocity,
		Velocity,
		XY,
		XZ,
		YZ,
		XYZ,
	};

	enum class LockMode
	{
		Disabled,
		EmitOnly,
		Enabled,
	};

	enum class CullPriority
	{
		Gameplay,
		Important,
		Cosmetic,
	};

	struct EmitterInterval
	{
		float min_start = 0.0f;
		float max_start = 0.0f;
		float min_active = 0.0f;
		float max_active = 0.0f;
		float min_pause = 0.0f;
		float max_pause = 0.0f;
	};

	struct EmitterTemplate
	{
		MaterialDeferredHandle update_material;
		MaterialDeferredHandle render_material;
		Mesh::FixedMeshDeferredHandle particle_mesh;
		FaceLock face_lock = FaceLock::None;
		size_t particles_count = 100;
		float emitter_duration_max = 10.0f;
		float emitter_duration_min = 10.0f;
		float particle_duration_max = 0.5f;
		float particle_duration_min = 0.5f;
		float min_animation_speed = 0.2f;
		float bounding_size = 500.0f; // in cm
		float emit_burst = 0.0f;
		float custom_seed = -1.0f;
		EmitterInterval interval;
		simd::Curve7 particles_per_second = simd::Curve7().FromConstant(10.0f);
		LockMode lock_rotation = LockMode::Disabled;
		LockMode lock_scale = LockMode::Disabled;
		CullPriority culling_prio = CullPriority::Cosmetic;
		bool use_pps = false;
		bool is_continuous = false;
		bool scale_emitter_duration = false;
		bool scale_particle_duration = false;
		bool lock_translation = false;
		bool lock_to_bone = false;
		bool lock_to_screen = false;
		bool lock_movement = false;
		bool ignore_bounding = false;
		bool reverse_bones = false;

		//Tools only stuff
		Memory::DebugStringA<128> ui_name;
		bool enabled = true;

		bool IsLocked() const
		{
			return lock_translation || lock_to_bone || lock_movement || lock_rotation != LockMode::Disabled || lock_scale != LockMode::Disabled;
		}

		size_t ComputeMinSimultaneousParticles(bool animation_event = false) const
		{
			if(!use_pps)
				return particles_count;

			float particle_duration = particle_duration_min;
			float emitter_duration = emitter_duration_min;

			if (scale_emitter_duration || (animation_event && !is_continuous))
				emitter_duration /= std::max(1e-1f, min_animation_speed);

			if (scale_particle_duration)
				particle_duration /= std::max(1e-1f, min_animation_speed);

			const auto min_pps = std::max(0.0f, particles_per_second.CalculateMinMax().first - particles_per_second.variance);
			const auto ring_buffer_time = is_continuous || animation_event || particle_duration < emitter_duration ? particle_duration : emitter_duration;
			return size_t(std::ceil(ring_buffer_time * min_pps));
		}

		size_t ComputeParticleCount(bool animation_event = false) const
		{
			if (!use_pps)
				return particles_count;

			float particle_duration = particle_duration_max;
			float emitter_duration = emitter_duration_max;

			if (scale_emitter_duration || (animation_event && !is_continuous))
				emitter_duration /= std::max(1e-1f, min_animation_speed);

			if (scale_particle_duration)
				particle_duration /= std::max(1e-1f, min_animation_speed);

			const auto max_pps = particles_per_second.CalculateMinMax().second + particles_per_second.variance;
			const auto ring_buffer_time = is_continuous || animation_event || particle_duration < emitter_duration ? particle_duration : emitter_duration;
			return size_t(std::ceil(ring_buffer_time * max_pps)) + 1;
		}
	};
}
