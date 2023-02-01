#pragma once
#include "Common/Resource/DeviceUploader.h"
#include "Common/Resource/Handle.h"
#include "Common/Utility/Numeric.h"

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
		Velocity_Camera,
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

		EmitterInterval& SetStart(float min_v, float max_v) { min_start = min_v; max_start = max_v; return *this; }
		EmitterInterval& SetActive(float min_v, float max_v) { min_active = min_v; max_active = max_v; return *this; }
		EmitterInterval& SetPause(float min_v, float max_v) { min_pause = min_v; max_pause = max_v; return *this; }
	};

	struct RenderPassDesc
	{
		MaterialDeferredHandle render_material;
		Mesh::FixedMeshDeferredHandle particle_mesh;
		bool overwrite_mesh = false;
	};

	struct EmitterTemplate
	{
		MaterialDeferredHandle update_material;
		Memory::SmallVector<RenderPassDesc, 1, Memory::Tag::Storage> render_passes;
		Mesh::FixedMeshDeferredHandle default_particle_mesh;
		FaceLock face_lock = FaceLock::None;
		size_t particles_count_min = 100;
		size_t particles_count_max = 100;
		float emitter_duration_max = 10.0f;
		float emitter_duration_min = 10.0f;
		float particle_duration_max = 0.5f;
		float particle_duration_min = 0.5f;
		float min_animation_speed = 0.2f;
		float bounding_size = 500.0f; // in cm
		float emit_burst = 0.0f;
		float emit_chance = 1.0f;
		unsigned group_size_shift = 0;
		EmitterInterval interval;
		simd::Curve7 particles_per_second = simd::Curve7().FromConstant(10.0f);
		LockMode lock_rotation = LockMode::Disabled;
		LockMode lock_rotation_to_bone = LockMode::Disabled;
		LockMode lock_scale_x = LockMode::Disabled;
		LockMode lock_scale_x_bone = LockMode::Disabled;
		LockMode lock_scale_y = LockMode::Disabled;
		LockMode lock_scale_y_bone = LockMode::Disabled;
		LockMode lock_scale_z = LockMode::Disabled;
		LockMode lock_scale_z_bone = LockMode::Disabled;
		CullPriority culling_prio = CullPriority::Cosmetic;
		bool use_pps = false;
		bool is_continuous = false;
		bool scale_emitter_duration = false;
		bool scale_particle_duration = false;
		bool lock_translation = false;
		bool lock_to_screen = false;
		bool lock_movement = false;
		bool lock_movement_to_bone = false;
		bool lock_translation_to_bone = false;
		bool ignore_bounding = false;
		bool reverse_bones = false;
		bool apply_weapon_mesh = false;
		bool apply_weapon_material = false;
		bool emit_once = false;

		//Tools only stuff
		Memory::DebugStringA<128> ui_name;
		bool enabled = true;

		bool IsLocked() const
		{
			return lock_translation 
				|| lock_translation_to_bone
				|| lock_movement 
				|| lock_movement_to_bone
				|| lock_rotation != LockMode::Disabled
				|| lock_rotation_to_bone != LockMode::Disabled;
		}

		bool IsLockedScale() const
		{
			return lock_scale_x != LockMode::Disabled
				|| lock_scale_y != LockMode::Disabled
				|| lock_scale_z != LockMode::Disabled
				|| lock_scale_x_bone != LockMode::Disabled
				|| lock_scale_y_bone != LockMode::Disabled
				|| lock_scale_z_bone != LockMode::Disabled;
		}

		size_t ComputeGroupCount(float seed, bool animation_event = false) const
		{
			if (!use_pps)
				return std::min(particles_count_max, Lerp(particles_count_min, 1 + particles_count_max, Clamp(seed, 0.0f, 1.0f)));

			float particle_duration = particle_duration_max;
			float emitter_duration = emitter_duration_max;

			if (scale_emitter_duration || (animation_event && !is_continuous))
				emitter_duration /= std::max(1e-1f, min_animation_speed);

			if (scale_particle_duration)
				particle_duration /= std::max(1e-1f, min_animation_speed);

			const auto max_pps = particles_per_second.InlineVariance(seed).CalculateMinMax().second;
			const auto ring_buffer_time = is_continuous || animation_event || particle_duration < emitter_duration ? particle_duration : emitter_duration;
			return size_t(std::ceil(ring_buffer_time * max_pps)) + 1;
		}

		size_t ComputeParticleCount(float seed, bool animation_event = false) const
		{
			return ComputeGroupCount(seed, animation_event) << group_size_shift;
		}
	};
}
