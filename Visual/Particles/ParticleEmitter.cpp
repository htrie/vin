
#include "Common/Geometry/Bounding.h"

#include "Visual/Device/VertexDeclaration.h"
#include "Visual/Renderer/EffectGraphSystem.h"
#include "Visual/Renderer/DrawCalls/DrawCall.h"
#include "Visual/Renderer/DrawCalls/EffectGraphInstance.h"
#include "Visual/Renderer/Scene/View.h"
#include "Visual/Renderer/Scene/Camera.h"
#include "Visual/Animation/AnimationSystem.h"

#include "Particle.h"
#include "ParticleEmitter.h"

namespace Particles
{
	inline float ParticleEmitter::RandomFloatLocal()
	{
		return simd::RandomFloatLocal(random_next);
	}

	inline float ParticleEmitter::RandomFloatLocal(float min, float max)
	{
		return simd::RandomFloatLocal(min, max, random_next);
	}

	inline bool ParticleEmitter::RandomBoolLocal()
	{
		const auto res = RandomFloatLocal();
		return res > 0.5f;
	}

	inline float ParticleEmitter::RandomAngleLocal() ///Random number in [0.0f,2*PI]
	{
		return RandomFloatLocal(0.0f, 6.283185308f);
	}
	
	void ParticleEmitter::SetAnimationSpeedMultiplier( float animspeed_multiplier )
	{
		if ((emitter_template->scale_emitter_duration || event_duration > 0.0f) && !emitter_template->is_infinite)
			emitter_duration_multiplier = std::max(animspeed_multiplier, 0.0f);
		else
			emitter_duration_multiplier = 1.f;

		if (emitter_template->scale_particle_duration)
			particle_duration_multiplier = std::max(animspeed_multiplier, 0.0f);
		else
			particle_duration_multiplier = 1.f;

		if (emitter_template->scale_interval_durations)
			interval_duration_multiplier = std::max(animspeed_multiplier, 0.0f);
		else
			interval_duration_multiplier = 1.f;
	}

	void CalculateLockModeWorldTransform(const EmitterTemplateHandle& emitter_template, const simd::matrix &emitter_transform, simd::matrix& out_world_transform, simd::matrix& out_emit_transform, simd::matrix& out_lock_transform)
	{
		simd::vector3 lock_translation(0.f); simd::vector3 lock_scale(1.f);
		simd::vector3 emit_translation(0.f); simd::vector3 emit_scale(1.f);
		if (emitter_template->lock_mode.test(LockMode::Translation))
		{
			lock_translation = emitter_transform.translation();
			lock_scale = emitter_transform.scale();
		}
		else
		{
			emit_translation = emitter_transform.translation();
			emit_scale = emitter_transform.scale();
		}

		auto transform_no_scale = simd::matrix::scale(simd::vector3(1.f) / emitter_transform.scale()) * emitter_transform;
		if (emitter_template->lock_mode.test(LockMode::Rotation))
		{
			out_world_transform = transform_no_scale;
			out_emit_transform = simd::matrix::identity();

			if (emitter_template->lock_mode.test(LockMode::Translation))
				out_lock_transform = transform_no_scale;
			else
				out_lock_transform = simd::matrix::identity();
		}
		else
		{
			out_emit_transform = transform_no_scale;
			out_world_transform = simd::matrix::identity();
			out_lock_transform = simd::matrix::identity();
		}

		out_world_transform = simd::matrix::scale(lock_scale) * out_world_transform;
		out_world_transform[3] = simd::vector4(lock_translation, 1.f);
		out_lock_transform = simd::matrix::scale(lock_scale) * out_lock_transform;
		out_lock_transform[3] = simd::vector4(lock_translation, 1.f);
		out_emit_transform = simd::matrix::scale(emit_scale) * out_emit_transform;
		out_emit_transform[3] = simd::vector4(emit_translation, 1.f);
	}

	simd::vector3 CalculateLockModeScale(const EmitterTemplateHandle& emitter_template, const simd::matrix& emitter_transform)
	{
		//We abs the scale here because we don't want particle effects to flip backwards when attached to negatively scaled objects
		//In addition, some animations flip between negative scales on different axis at random points which was looking ugly
		simd::vector3 scale(1.f);
		const auto parent_scale = emitter_transform.scale();
		if (emitter_template->lock_mode.test(LockMode::ScaleX))
			scale.x = std::abs(parent_scale.x);
		if (emitter_template->lock_mode.test(LockMode::ScaleY))
			scale.y = std::abs(parent_scale.y);
		if (emitter_template->lock_mode.test(LockMode::ScaleZ))
			scale.z = std::abs(parent_scale.z);
		return scale;
	}

	float CalculateScaleFromBonesLength(const float reference_length, const Memory::Span<const simd::vector3>& positions)
	{
		if (positions.size() < 2 || reference_length == 0.f)
			return 1.f;
		else
		{
			float bone_length = (positions.front() - positions.back()).len();
			return bone_length / reference_length;
		}
	}

	void CalculateTransforms(const EmitterTemplateHandle& emitter_template, const Memory::Span<const simd::vector3>& positions, const simd::vector3& bone_parent, const simd::matrix &emitter_transform, simd::matrix& out_world_transform, simd::matrix& out_emit_transform, simd::matrix& out_lock_transform)
	{
		const bool is_locked_to_bone = emitter_template->IsLockedToBone();
		const bool is_locked_movement_to_bone = emitter_template->IsLockedMovementToBone();
		if (is_locked_to_bone || is_locked_movement_to_bone)
		{
			if (is_locked_to_bone && !positions.empty())
			{
				simd::matrix bone_transform = simd::matrix::translation(bone_parent);
				out_world_transform = bone_transform * emitter_transform;
			}
			else
				out_world_transform = emitter_transform;

			out_lock_transform = out_world_transform;
			out_emit_transform = simd::matrix::identity();
		}
		else
			CalculateLockModeWorldTransform(emitter_template, emitter_transform, out_world_transform, out_emit_transform, out_lock_transform);
	}

	simd::vector3 CalculateScale(const EmitterTemplateHandle& emitter_template, const Memory::Span<const simd::vector3>& positions, const simd::matrix& emitter_transform)
	{
		auto result = CalculateLockModeScale(emitter_template, emitter_transform);
		if (emitter_template->scale_to_bone_length > 0.f)
		{
			float scale = CalculateScaleFromBonesLength(emitter_template->scale_to_bone_length, positions);
			result.y *= scale;
		}
		return result;
	}

	bool PassSpeedThreshold(const EmitterTemplateHandle& emitter_template, float speed)
	{
		if (emitter_template->emit_min_speed >= 0.0f && speed < emitter_template->emit_min_speed)
			return false;

		if (emitter_template->emit_max_speed >= 0.0f && speed > emitter_template->emit_max_speed)
			return false;

		return true;
	}

	float CalculateSpeedMultipliers(const float elapsed_time, const EmitterTemplateHandle& emitter_template, const float distance, float& out_emit_speed_multiplier, float& out_update_speed_multiplier)
	{
		out_emit_speed_multiplier = 1.f;
		out_update_speed_multiplier = 1.f;
		const auto speed = (elapsed_time == 0.f) ? 0.f : distance / elapsed_time;
		if (emitter_template->emit_speed_threshold > 0.f)
			out_emit_speed_multiplier = std::min(speed / emitter_template->emit_speed_threshold, 1.f);
		if (emitter_template->update_speed_threshold > 0.f)
			out_update_speed_multiplier = std::max(speed / emitter_template->update_speed_threshold, 1.f);

		return speed;
	}

	void ParticleEmitter::UpdateTransformInternal(const simd::matrix &_emitter_transform, const Memory::Span<const simd::vector3>& positions, const simd::vector3& local_bone_parent)
	{
		CalculateTransforms(emitter_template, positions, local_bone_parent, _emitter_transform, emitter_world_transform, emitter_transform, emitter_lock_transform);
		global_scale = CalculateScale(emitter_template, positions, _emitter_transform);
		emitter_world_transform_noscale = simd::matrix::scale(simd::vector3(1.f) / emitter_world_transform.scale()) * emitter_world_transform;
		emitter_lock_transform_noscale = simd::matrix::scale(simd::vector3(1.f) / emitter_lock_transform.scale()) * emitter_lock_transform;
		center_pos = simd::vector4(current_transform.translation(), global_scale.x);
	}

	void ParticleEmitter::SetTransform(const simd::matrix &_emitter_transform)
	{
		current_transform = _emitter_transform;

		const auto local_bone_parent = current_positions.size() <= bone_parent ? simd::vector3() : current_positions[bone_parent];
		UpdateTransformInternal(_emitter_transform, current_positions, local_bone_parent);
	}

	float ParticleEmitter::CalculateDeltaTime(const float elapsed_time) const
	{
		float delta_time = elapsed_time * update_speed_multiplier;
		if (quashed)
			delta_time *= emitter_template->anim_speed_scale_on_death;
		else
			delta_time *= particle_duration_multiplier;
		return delta_time;
	}

	void ParticleEmitter::StartEmitting()
	{ 
		if (!emitter_template->is_infinite)
		{
			float emitter_duration = emit_duration;
			if (emitter_duration_multiplier > 0.f)
				emitter_duration /= emitter_duration_multiplier;

			if(time_alive >= emitter_duration) // Reset particle
				time_alive = 0;
		}

		quashed = false;
	}

	bool ParticleEmitter::IsEmitting() const
	{ 
		float emitter_duration = emit_duration;
		if (emitter_duration_multiplier > 0.f)
			emitter_duration /= emitter_duration_multiplier;

		return !quashed && (emitter_template->is_infinite || time_alive < emitter_duration); 
	}

	void ParticleEmitter::SetAnimationStorage(const Animation::DetailedStorageID& id, const Memory::Span<const unsigned>& bone_indices)
	{
		animation_id = id;
		animation_bones.clear();
		animation_bones.reserve(bone_indices.size());
		for (const auto& a : bone_indices)
			animation_bones.push_back(a);
	}

	void ParticleEmitter::EmitSingleParticle(const simd::vector3& local_bone_parent, float emit_scale, const simd::vector4& emit_color, const Memory::Span<const simd::vector3>& positions)
	{
		const size_t count = ParticlesGetCount(particles);
		for (unsigned i = 0; i < count; i++) 
		{
			const auto index = next_particle; 
			if (++next_particle >= count)
				next_particle = 0;

			if (ParticlesGetDead(particles, index))
			{
				emit_started = true;
				EmitParticle(index, emit_scale, emit_color, local_bone_parent, positions);
				ParticlesMoveSlow(particles, index, time_since_last_emit);
				break; 
			}
		}
	}

	std::pair<bool, float> ParticleEmitter::UpdateIntervals(float elapsed_time)
	{
		bool pause_after_frame = false;
		float interval_delta = elapsed_time;
		if (emitter_template->scale_interval_durations && interval_duration_multiplier > 0.f)
			interval_delta *= interval_duration_multiplier;

		interval_duration -= interval_delta;
		if (interval_duration < 0)
		{
			if (is_paused)
			{
				if (emitter_template->emit_interval_max > 0) // Check whether there can be a emit interval
				{
					elapsed_time = std::min(elapsed_time, -interval_duration); // Adjust elapsed time for time alive calculation 
					is_paused = false;
					interval_duration += RandomFloatLocal(emitter_template->emit_interval_min, emitter_template->emit_interval_max);
				}
			}
			else
			{
				if (emitter_template->pause_interval_max > 0) // Check whether there can be a paused interval
				{
					elapsed_time = std::max(0.0f, elapsed_time + interval_duration); // Adjust elapsed time for time alive calculation 
					pause_after_frame = true; // delay state change to include the remaining elapsed_time in the emitter's lifetime
					interval_duration += RandomFloatLocal(emitter_template->pause_interval_min, emitter_template->pause_interval_max);
				}
			}

			if (interval_duration < 0) // We don't switch the state more than once per frame for performance reasons
				interval_duration = 0;
		}

		return std::make_pair(pause_after_frame, elapsed_time);
	}

	void ParticleEmitter::EmitParticles(const simd::vector3& local_bone_parent, float sample_time, float emitter_duration, const Memory::Span<const simd::vector3>& positions)
	{
		float current_particles_per_second = (emitter_template->particles_per_second.GetInterpolatedValue(sample_time, emitter_duration) + pps_variance) * emitter_duration_multiplier * emit_speed_multiplier;
		if (current_particles_per_second > 0)
		{
			float current_emit_scale = emitter_template->emit_scale.GetInterpolatedValue(sample_time, emitter_duration);
			const auto emit_color = emitter_template->emit_colour.GetInterpolatedValue(sample_time, emitter_duration);
			float current_time_per_particle = 1.0f / current_particles_per_second;
			bool particle_emitted = false;

			while (true)
			{
				if (emit_started && time_since_last_emit < current_time_per_particle)
					break;

				time_since_last_emit = std::max(time_since_last_emit - current_time_per_particle, 0.f);
				particle_emitted = true;

				EmitSingleParticle(local_bone_parent, current_emit_scale, emit_color, positions);
			}

			if (particle_emitted)
				pps_variance = RandomFloatLocal(-emitter_template->emit_speed_variance, emitter_template->emit_speed_variance);
		}
		else
			time_since_last_emit = 0.0f;
	}

	void ParticleEmitter::UpdateTransform(simd::matrix transform_matrix, const Memory::Span<const simd::vector3>& positions, const simd::vector3& local_bone_parent)
	{
		const auto rot_offset = emitter_template->rotation_offset_min + ((emitter_template->rotation_offset_max - emitter_template->rotation_offset_min) * random_rotation);
		auto pos_offset = emitter_template->position_offset_min + ((emitter_template->position_offset_max - emitter_template->position_offset_min) * random_position);
		simd::matrix offset_matrix = simd::matrix::yawpitchroll(rot_offset.z, rot_offset.x, rot_offset.y);
		if (emitter_template->mesh_handle.IsNull())
		{
			if (emitter_template->rotation_lock_type == RotationLockType::Fixed)
				transform_matrix = transform_matrix * simd::matrix::translation(pos_offset);
			else if (emitter_template->rotation_lock_type == RotationLockType::FixedLocal || emitter_template->rotation_lock_type == RotationLockType::Velocity)
				transform_matrix = simd::matrix::translation(pos_offset) * transform_matrix;
		}

		UpdateTransformInternal(transform_matrix, positions, local_bone_parent);

		if (emitter_template->rotation_lock_type == RotationLockType::FixedLocal)
		{
			if (emitter_template->mesh_handle.IsNull())
			{
				const auto emitter_rot = emitter_world_transform_noscale.rotation() * emitter_transform.rotation();
				if (emitter_template->face_type == FaceType::NoLock)
					offset_matrix = offset_matrix * simd::matrix::rotationquaternion(emitter_rot);
				else if (emitter_template->face_type == FaceType::XZLock || emitter_template->face_type == FaceType::YZLock || emitter_template->face_type == FaceType::ZLock)
					offset_matrix = offset_matrix * simd::matrix::yawpitchroll(0.0f, PIDIV2, 0.0f) * simd::matrix::rotationquaternion(emitter_rot.inverse()) * simd::matrix::yawpitchroll(0.0f, -PIDIV2, 0.0f);
				else if (emitter_template->face_type != FaceType::FaceCamera)
					offset_matrix = offset_matrix * simd::matrix::rotationquaternion(emitter_rot.inverse());
			}
			else
			{
				if (emitter_template->face_type == FaceType::FaceCamera)
					pos_offset = emitter_world_transform_noscale.rotation().transform(pos_offset);

				offset_matrix = simd::matrix::translation(pos_offset) * offset_matrix;
			}
		}
		else if (!emitter_template->mesh_handle.IsNull())
			offset_matrix = simd::matrix::translation(pos_offset) * offset_matrix;

		rotation_offset = offset_matrix;
		first_frame = false;
	}

	void ParticleEmitter::SetPosition(const Positions& _position)
	{
		current_positions = _position;
	}

	simd::vector3 ParticleEmitter::GetPosition(size_t i, size_t count, const Memory::Span<const simd::vector3>& positions) const
	{
		if (i >= positions.size() || i >= count)
			return simd::vector3();

		if (emitter_template->reverse_bone_group)
			return positions[std::min(count, positions.size()) - (1 + i)];

		return positions[i];
	}

	void ParticleEmitter::FrameMoveInternal(float elapsed_time, const Memory::Span<const simd::vector3>& positions, float emitter_duration, const simd::matrix& transform, float speed)
	{
		const auto local_bone_parent = positions.size() <= bone_parent ? simd::vector3() : positions[bone_parent];
		UpdateTransform(transform, positions, local_bone_parent);
		UpdatePosition(positions);

		float delta_time = CalculateDeltaTime(elapsed_time);

		const auto path_points = emitter_template->IsLockedMovementToBone() ? positions : Memory::Span<const simd::vector3>();
		ParticlesSetGlobals(particles, delta_time, global_scale, &*emitter_template, path_points, emitter_world_transform_noscale);
		ParticlesMove(particles, emitter_lock_transform, emitter_lock_transform_noscale, emitter_template->UseAnimFromLife());
		
		if (IsEmitting())
		{
			bool pause_after_frame = false;
			std::tie(pause_after_frame, elapsed_time) = UpdateIntervals(elapsed_time);

			if (!is_paused)
			{
				time_alive += elapsed_time;
				if (PassSpeedThreshold(emitter_template, speed))
				{
					time_since_last_emit += elapsed_time;
					if (time_alive >= emitter_duration)
					{
						if (emitter_template->is_infinite)
							time_alive -= emitter_duration;
						else
							time_since_last_emit -= time_alive - emitter_duration;
					}

					float sample_time = time_alive - time_since_last_emit;
					if (sample_time < 0.0f)
						sample_time += emitter_duration;

					EmitParticles(local_bone_parent, sample_time, emitter_duration, positions);
				}

				if (pause_after_frame)
					is_paused = true;
			}
		}
	}

	float ParticleEmitter::UpdateSpeed(float elapsed_time)
	{
		const auto local_bone_parent = current_positions.size() <= bone_parent ? simd::vector3() : current_positions[bone_parent];
		float distance_traveled = 0.0f;

		if (!first_frame && (emitter_template->emit_speed_threshold > 0.f || emitter_template->update_speed_threshold > 0.f || emitter_template->emit_min_speed >= 0.0f || emitter_template->emit_max_speed >= 0.0f))
		{
			auto ComputePos = [this](const simd::vector3& bone)
			{
				if (emitter_template->use_world_speed)
					return (emitter_world_transform * emitter_transform).mulpos(bone);
				else
					return emitter_transform.mulpos(bone);
			};

			const auto prev_position = ComputePos(prev_bone_parent);
			CalculateTransforms(emitter_template, current_positions, local_bone_parent, current_transform, emitter_world_transform, emitter_transform, emitter_lock_transform);
			const auto new_position = ComputePos(local_bone_parent);
			distance_traveled = (new_position - prev_position).len();
		}

		prev_bone_parent = local_bone_parent;
		return CalculateSpeedMultipliers(elapsed_time, emitter_template, distance_traveled, emit_speed_multiplier, update_speed_multiplier);
	}

    BoundingBox ParticleEmitter::FrameMove(float elapsed_time)
    {
		if (reinitialise)
		{
			reinitialise = false;
			Reinitialise();
		}

		elapsed_time *= multiplier;

		const auto delta_transform = current_transform * last_transform.inverse();

		simd::vector3 delta_pos = simd::vector3(0.0f, 0.0f, 0.0f);
		simd::vector3 delta_scale = simd::vector3(1.0f, 1.0f, 1.0f);
		simd::quaternion delta_rot = simd::quaternion::identity();

		if (delta_transform.determinant()[0] < 0.0f)
			Reinitialise();
		else
		{
			simd::matrix delta_rot_matrix;
			delta_transform.decompose(delta_pos, delta_rot_matrix, delta_scale);
			delta_rot = delta_rot_matrix.rotation();
		}

		size_t frame_count = 1;
		if (!animation_bones.empty() && animation_bones.size() >= current_positions.size())
		{
			const auto& info = Animation::System::Get().GetDetailedStorageStatic(animation_id);

			if (info.frame_count > 0)
			{
				frame_count += info.frame_count;
				elapsed_time = info.elapsed_time_total * multiplier;
			}
		}

		float emitter_duration = emit_duration;
		if (emitter_duration_multiplier > 0.f)
			emitter_duration /= emitter_duration_multiplier;

		const bool emitter_alive = !quashed && (emitter_template->is_infinite || time_alive < emitter_duration);
		float last_time = 0.0f;

		const float speed = UpdateSpeed(elapsed_time);

		if (frame_count > 1)
		{
			assert(animation_bones.size() >= current_positions.size());
			simd::vector3* bone_positions = (simd::vector3*)alloca(sizeof(simd::vector3) * current_positions.size());

			for (size_t a = 1; a < frame_count; a++)
			{
				const auto& frame = Animation::System::Get().GetDetailedStorage(animation_id, a - 1);
				if (frame.bones.empty())
					continue;

				const auto frame_elapsed = frame.elapsed_time * multiplier;
				if (frame_elapsed > last_time)
				{
					const float t = std::clamp(frame_elapsed / elapsed_time, 0.0f, 1.0f);

					// We compute the delta matrix, so we interpolate between identity and given delta values
					const auto p = simd::vector3(0.0f, 0.0f, 0.0f).lerp(delta_pos, t);
					const auto s = simd::vector3(1.0f, 1.0f, 1.0f).lerp(delta_scale, t);
					const auto r = simd::quaternion::identity().slerp(delta_rot, t);

					for (size_t b = 0; b < current_positions.size(); b++)
						bone_positions[b] = animation_bones[b] < frame.bones.size() ? frame.bones[animation_bones[b]] : simd::vector3();

					//TODO: Do proper spline interpolation between bone positions, so particles get more spread out

					// Compute the final (interpoalted) transformation matrix from last & delta
					const auto transform = simd::matrix::scalerotationtranslation(s, r, p) * last_transform;
					FrameMoveInternal(frame_elapsed - last_time, Memory::Span<simd::vector3>(bone_positions, current_positions.size()), emitter_duration, transform, speed);
					last_time = frame_elapsed;
				}
			}
		}

		if (elapsed_time >= last_time)
			FrameMoveInternal(elapsed_time - last_time, current_positions, emitter_duration, current_transform, speed);

		last_transform = current_transform;
		is_alive = ParticlesGetAlive(particles) || emitter_alive;
		return ParticlesMergeDynamicBoundingBoxSlow(particles);
	}

    ParticleEmitter::ParticleEmitter( const EmitterTemplateHandle& emitter_template, const Positions& _position, const simd::matrix& transform, size_t bone_parent, float delay, float event_duration)
        : emitter_template( emitter_template )
		, current_positions( _position )
		, particles(ParticlesCreate())
		, next_particle( 0 )
		, bone_parent(bone_parent)
		, current_transform(transform)
		, event_duration(event_duration)
    {
        Reinitialise(delay);
    }

    ParticleEmitter::~ParticleEmitter()
    {
        ParticlesDestroy(particles);
        particles = nullptr;
    }

	unsigned ParticleEmitter::GetMergeId() const
	{
		const auto vertex_type = GetVertexType();
		const auto mesh_handle = GetMesh();

		void* mesh_ptr = mesh_handle.IsNull() ? nullptr : (void*)(&*mesh_handle);

		uint64_t v[] = {
			uint64_t(vertex_type),
			uint64_t(reinterpret_cast<uintptr_t>(mesh_ptr)),
		};

		return MurmurHash2(v, int(sizeof(v)), 0xc58f1a7b);
	}

	VertexType ParticleEmitter::GetVertexType() const
	{
		if (!emitter_template->mesh_handle.IsNull())
			return VERTEX_TYPE_MESH;

		if (emitter_template->face_type == FaceType::NoLock)
			return VERTEX_TYPE_NORMAL;

		if (emitter_template->rotation_lock_type == RotationLockType::Velocity)
			return VERTEX_TYPE_VELOCITY;

		return VERTEX_TYPE_DEFAULT;
	}

	bool ParticleEmitter::NeedsSorting() const
	{
		return emitter_template->NeedsSorting();
	}

	bool ParticleEmitter::UseAnimFromLife() const
	{
		return emitter_template->UseAnimFromLife();
	}

	Mesh::FixedMeshHandle ParticleEmitter::GetMesh() const
	{
		return emitter_template->mesh_handle;
	}

	//Returns a tuple of (position,direction) on the line segment by a factor [0,1]
	std::tuple< simd::vector3, simd::vector3, float > ParticleEmitter::PickPointOnLineByFactor(const Memory::Span<const simd::vector3>& positions, const Percentages& line_percentages, const float t)
	{
		//Find out which segment the chosen position is on
		const auto upper_bound = std::upper_bound(line_percentages.begin(), line_percentages.end(), t);
		const size_t upper_line_segment_index = upper_bound - line_percentages.begin();
		assert(upper_line_segment_index > 0 && upper_line_segment_index < positions.size());

		//Work out how far into the segment we need to be
		const float t_dash = (t - line_percentages[upper_line_segment_index - 1]) / (line_percentages[upper_line_segment_index] - line_percentages[upper_line_segment_index - 1]);

		//Get vector for chosen segment
		const simd::vector3 segment = positions[upper_line_segment_index] - positions[upper_line_segment_index - 1];
		const simd::vector3 position = positions[upper_line_segment_index - 1] + segment * t_dash;
		const simd::vector3 direction = segment.normalize();
		return std::make_tuple(position, direction, t);
	}

	std::tuple< simd::vector3, simd::vector3, float > ParticleEmitter::PickPointOnLineRandom(const Memory::Span<const simd::vector3>& positions, const Percentages& line_percentages )
	{
		const float t = RandomFloatLocal();
		return PickPointOnLineByFactor(positions, line_percentages, t);
	}

	std::pair< simd::vector3, simd::vector3 > ParticleEmitter::PickPointOnCircleByAngle(const simd::vector3& center, const simd::vector3& x_basis, const simd::vector3& y_basis, bool use_axis_as_direction, float angle)
	{
		const float s = sinf(angle), c = cosf(angle);
		simd::vector3 direction, offset;
		if (use_axis_as_direction)
		{
			direction = y_basis.normalize();
			offset = x_basis * s + direction.cross(x_basis) * c;
		}
		else
		{
			direction = x_basis * s + y_basis * c;
			offset = direction;
			direction = direction.normalize();
		}
		const simd::vector3 position = center + offset;
		return std::make_pair(position, direction);
	}

	std::pair< simd::vector3, simd::vector3 > ParticleEmitter::PickPointOnCircleRandom( const simd::vector3& center, const simd::vector3& x_basis, const simd::vector3& y_basis, bool use_axis_as_direction )
	{
		const float angle = RandomAngleLocal();
		return PickPointOnCircleByAngle(center, x_basis, y_basis, use_axis_as_direction, angle);
	}

	std::pair< simd::vector3, simd::vector3 > ParticleEmitter::PickPointOnSphere(const simd::vector3 sphere_position, const float radius)
	{
		const float angle = RandomAngleLocal();
		const float u = RandomFloatLocal() * 2.0f - 1.0f;
		const float val = sqrtf(1.0f - u * u);

		const simd::vector3 direction(val * cosf(angle), val * sinf(angle), u);
		const simd::vector3 position = sphere_position + direction * radius;
		return std::make_pair(position, direction);
	}

    void ParticleEmitter::ReinitialiseParticleEffect( )
    {
		reinitialise = true;
	}

	void ParticleEmitter::Reinitialise(float delay)
	{
		last_transform = current_transform;
        emitter_transform = simd::matrix::identity();
        emitter_world_transform = emitter_transform;
		emitter_world_transform_noscale = emitter_transform;
		emitter_lock_transform = emitter_transform;
		rotation_offset = simd::matrix::identity();
		first_frame = true;
		
		global_scale = 1.f;
        center_pos = simd::vector4(emitter_transform.mulpos(simd::vector3(0.0f, 0.0f, 0.0f)), 0.0f);
		
		// Always select a random seed for calculating emit duration, then overwrite seed if seed is fixed
			do {
				random_next = std::rand();
			} while (random_next == 0); // Our RNG doesn't produce random values with a seed of 0

		random_rotation = simd::vector3(RandomFloatLocal(), RandomFloatLocal(), RandomFloatLocal());
		random_position = simd::vector3(RandomFloatLocal(), RandomFloatLocal(), RandomFloatLocal());

		emit_duration = RandomFloatLocal(emitter_template->min_emitter_duration, emitter_template->max_emitter_duration);
		if (event_duration > 0.0f && !emitter_template->is_infinite)
			emit_duration = event_duration;

		if (emitter_template->enable_random_seed)
			random_next = emitter_template->random_seed;

		pps_variance = RandomFloatLocal(-emitter_template->emit_speed_variance, emitter_template->emit_speed_variance);
		is_paused = emitter_template->start_interval_max > 0;
		interval_duration = is_paused 
			? RandomFloatLocal(emitter_template->start_interval_min, emitter_template->start_interval_max) 
			: RandomFloatLocal(emitter_template->emit_interval_min, emitter_template->emit_interval_max);

		if (delay > 0.0f)
		{
			if (is_paused)
				interval_duration += delay;
			else
			{
				is_paused = true;
				interval_duration = delay;
			}
		}

		const size_t max_particles = emitter_template->MaxParticles();
        const size_t rounded_count = simd::align(max_particles, (size_t)4);

        ParticlesResize(particles, (unsigned)rounded_count / 4);

        time_since_last_emit = 0.0f;
        next_particle = 0;
        time_alive = 0.0f;
        quashed = false;
		emit_started = false;
    }

	simd::vector3 ParticleEmitter::RandomDirectionInCone(const simd::vector3& emit_direction, const float cone_size)
	{
		//Construct the particle direction
		float sigma = RandomFloatLocal() * (2.0f * simd::pi); //between 0 and 2pi
		float alpha = RandomFloatLocal() * (2.0f * cone_size) - cone_size; //between +-scatter_cone_size

		const auto normalized_direction = emit_direction.normalize();
		const simd::vector3 y_axis(0.0f, 1.0f, 0.0f);

		//Work out the rotation axis
		simd::vector3 alpha_rotation_axis(1.0f, 0.0f, 0.0f);
		if (abs(y_axis.dot(normalized_direction)) + FLT_EPSILON < 1.f)
		{
			alpha_rotation_axis = y_axis.cross(normalized_direction);
			alpha_rotation_axis = alpha_rotation_axis.normalize();
		}

		//Construct rotations for random vector
		simd::matrix alpha_rotation = simd::matrix::rotationaxis(alpha_rotation_axis, alpha);
		simd::matrix sigma_rotation = simd::matrix::rotationaxis(normalized_direction, sigma);

		//Apply rotations to create the final random vector
		const simd::matrix final_rotation = alpha_rotation * sigma_rotation;
		simd::vector3 output_direction = final_rotation.mulpos(emit_direction);

		return output_direction;
	}
    
    void ParticleEmitter::AddParticle( size_t i, const simd::vector3& position, const simd::vector3& local_bone_parent, const simd::vector3& direction, float emit_scale, const simd::vector4& emit_color, float distance_percent /*= 0.f*/, const simd::vector4& offset_direction /*= simd::vector4(0.f)*/ )
    {
		const float scale = emitter_template->scale[0].Value;
		const float stretch = emitter_template->stretch[0].Value;
		simd::vector3 pos_offset = simd::vector3(0.0f);
		simd::quaternion rot_offset = simd::quaternion::identity();

		if (!emitter_template->mesh_handle.IsNull())
		{
			auto _rot_offset = emitter_template->instance_rotation_offset_min + ((emitter_template->instance_rotation_offset_max - emitter_template->instance_rotation_offset_min) * simd::vector3(RandomFloatLocal(), RandomFloatLocal(), RandomFloatLocal()));
			pos_offset = emitter_template->instance_position_offset_min + ((emitter_template->instance_position_offset_max - emitter_template->instance_position_offset_min) * simd::vector3(RandomFloatLocal(), RandomFloatLocal(), RandomFloatLocal()));
			rot_offset = simd::matrix::yawpitchroll(-_rot_offset.y, _rot_offset.x, -_rot_offset.z).rotation().normalize();
			pos_offset *= emitter_world_transform.scale();
		}

        ParticlesAdd(particles,
            (unsigned)i, 
            simd::vector3(0.0f, 0.0f, 0.0f),
			emitter_template->IsLockedToBone() ? position - local_bone_parent : emitter_template->IsLockedMovementToBone() ? local_bone_parent : position,
			emitter_template->rotation_lock_type == RotationLockType::Random ? RandomAngleLocal() : 0.f,
            direction,
            0.0f,
            ( RandomFloatLocal() * 2.0f - 1.0f ),
            0,
			emit_color,
            scale,
			emit_scale,
            stretch,
            (emitter_template->random_flip_x ? RandomBoolLocal() : false),
            (emitter_template->random_flip_y ? RandomBoolLocal() : false),
            0.0f, 
            emitter_template->particleLife(),
            false,
			distance_percent,
			offset_direction,
			rot_offset,
			pos_offset
		);
    }

	Entity::Desc ParticleEmitter3d::GenerateCreateInfo(Device::CullMode cull_mode) const // IMPORTANT: Keep in sync with EmitterTemplate::Warmup. // [TODO] Share.
	{
		Entity::Desc desc;

		const auto rotation_locked = emitter_template->rotation_lock_type == RotationLockType::Velocity;

	#ifdef ENABLE_PARTICLE_FMT_SEQUENCE
		const bool is_mesh_particle = !emitter_template->mesh_handles.empty();
	#else
		const bool is_mesh_particle = !emitter_template->mesh_handle.IsNull();
	#endif

		if (!is_mesh_particle || (emitter_template->flip_mode == FlipMode::FlipMirrorNoCulling && (emitter_template->random_flip_x || emitter_template->random_flip_y)))
		{
			cull_mode = Device::CullMode::NONE;
		}
		else if (emitter_template->face_type == FaceType::XYLock)
		{
			if (cull_mode == Device::CullMode::CCW)
				cull_mode = Device::CullMode::CW;
			else if (cull_mode == Device::CullMode::CW)
				cull_mode = Device::CullMode::CCW;
		}

		const auto particle_offset = simd::vector4( emitter_template->particle_offset[ 0 ], emitter_template->particle_offset[ 1 ], (float)emitter_template->anim_speed, 0.f );
		const auto particle_numframes = simd::vector4((float)emitter_template->number_of_frames_x, (float)emitter_template->number_of_frames_y, 0.f, 0.f);

		desc.SetType(Renderer::DrawCalls::Type::Particle)
			.SetAsync(!high_priority)
			.SetCullMode(cull_mode)
			.AddObjectUniforms(Renderer::DrawCalls::Uniforms()
				.AddStatic(Renderer::DrawDataNames::WorldTransform, simd::matrix::identity()) // Note: world transform is always identity since the particle merging change
				.AddStatic(Renderer::DrawDataNames::FlipTangent, cull_mode == Device::CullMode::CW ? -1.0f : 1.0f)
				.AddStatic(Renderer::DrawDataNames::StartTime, -100.0f)
				.AddStatic(Renderer::DrawDataNames::ParticleOffset, particle_offset)
				.AddDynamic(Renderer::DrawDataNames::ParticleRotationOffset, rotation_offset)
				.AddStatic(Renderer::DrawDataNames::ParticleNumFrames, particle_numframes)
				.AddDynamic(Renderer::DrawDataNames::ParticleEmitterPos, center_pos)
				.AddDynamic(Renderer::DrawDataNames::ParticleEmitterTransform, emitter_world_transform))
			.AddEffectGraphs({ L"Metadata/EngineGraphs/base.fxgraph" }, 0)
			.SetDebugName(emitter_template.GetParent().GetFilename());

		if (is_mesh_particle)
		{
			auto merged_vertex_elements = emitter_template->mesh_handle->GetVertexElements();
			for (const auto& a : mesh_vertex_elements)
				merged_vertex_elements.push_back(a);

			desc.SetVertexElements(merged_vertex_elements).AddEffectGraphs({ L"Metadata/EngineGraphs/particle_mesh.fxgraph" }, 0);

			const Mesh::VertexLayout layout(emitter_template->mesh_handle->GetFlags());
			if (layout.HasUV2())
				desc.AddEffectGraphs({ L"Metadata/EngineGraphs/attrib_uv2.fxgraph" }, 0);

			if (layout.HasColor())
				desc.AddEffectGraphs({ L"Metadata/EngineGraphs/attrib_color.fxgraph" }, 0);
		}
		else
		{
			switch (emitter_template->face_type)
			{
			case FaceType::FaceCamera:
			{
				if (rotation_locked)
					desc.SetVertexElements(rotation_lock_vertex_elements).AddEffectGraphs({ L"Metadata/EngineGraphs/particle_camerafacing_rotationlocked.fxgraph" }, 0);
				else
					desc.SetVertexElements(vertex_elements).AddEffectGraphs({ L"Metadata/EngineGraphs/particle_camerafacing.fxgraph" }, 0);
				break;
			}
			case FaceType::XYLock:
			{
				if (rotation_locked)
					desc.SetVertexElements(rotation_lock_vertex_elements).AddEffectGraphs({ L"Metadata/EngineGraphs/particle_xylocked_rotationlocked.fxgraph" }, 0);
				else
					desc.SetVertexElements(vertex_elements).AddEffectGraphs({ L"Metadata/EngineGraphs/particle_xylocked.fxgraph" }, 0);
				break;
			}
			case FaceType::ZLock:
			{
				if (rotation_locked)
					desc.SetVertexElements(rotation_lock_vertex_elements).AddEffectGraphs({ L"Metadata/EngineGraphs/particle_zlocked_rotationlocked.fxgraph" }, 0);
				else
					desc.SetVertexElements(vertex_elements).AddEffectGraphs({ L"Metadata/EngineGraphs/particle_zlocked.fxgraph" }, 0);
				break;
			}
			case FaceType::NoLock:
			{
				desc.SetVertexElements(no_lock_vertex_elements).AddEffectGraphs({ L"Metadata/EngineGraphs/particle_nolock.fxgraph" }, 0);
				break;
			}
			case FaceType::XZLock:
			{
				if (rotation_locked)
					desc.SetVertexElements(rotation_lock_vertex_elements).AddEffectGraphs({ L"Metadata/EngineGraphs/particle_xzlocked_rotationlocked.fxgraph" }, 0);
				else
					desc.SetVertexElements(vertex_elements).AddEffectGraphs({ L"Metadata/EngineGraphs/particle_xzlocked.fxgraph" }, 0);
				break;
			}
			case FaceType::YZLock:
			{
				if (rotation_locked)
					desc.SetVertexElements(rotation_lock_vertex_elements).AddEffectGraphs({ L"Metadata/EngineGraphs/particle_yzlocked_rotationlocked.fxgraph" }, 0);
				else
					desc.SetVertexElements(vertex_elements).AddEffectGraphs({ L"Metadata/EngineGraphs/particle_yzlocked.fxgraph" }, 0);
				break;
			}
			}
		}
		
		desc.AddEffectGraphs(emitter_template->material->GetGraphDescs(), 0)
			.AddObjectUniforms(emitter_template->material->GetMaterialOverrideUniforms())
			.AddObjectBindings(emitter_template->material->GetMaterialOverrideBindings());

		if (emitter_template->number_of_frames_x > 1 || emitter_template->number_of_frames_y > 1) // animated particles
			desc.AddEffectGraphs({ emitter_template->anim_play_once ? L"Metadata/EngineGraphs/particle_animate_once.fxgraph" : L"Metadata/EngineGraphs/particle_animate.fxgraph" }, 0);

		auto blend_mode = emitter_template->blend_mode;
		if (emitter_template->material->GetBlendMode(true) != Renderer::DrawCalls::BlendMode::NumBlendModes)
			blend_mode = emitter_template->material->GetBlendMode(true);
		desc.SetBlendMode(blend_mode);

		return desc;
	}

	bool ApproxEqual( const float a, const float b )
	{
		return abs( a - b ) < 0.0001f;
	}

	bool ApproxEqual( const simd::vector3& a, const simd::vector3& b )
	{
		return ApproxEqual( a[0], b[0] ) && ApproxEqual( a[1], b[1] ) && ApproxEqual( a[2], b[2] );
	}

	void PointParticleEmitter::UpdatePosition(const Memory::Span<const simd::vector3>& positions)
	{
		if (positions.empty())
			return;

		emit_position = GetPosition(0, 2, positions);
		if (positions.size() >= 2)
			emit_direction = (GetPosition(1, 2, positions) - GetPosition(0, 2, positions)).normalize();
		else
			emit_direction = simd::vector3(1.f, 0.f, 0.f);
	}

	void PointParticleEmitter3d::EmitParticle(size_t i, float emit_scale, const simd::vector4& emit_color, const simd::vector3& local_bone_parent, const Memory::Span<const simd::vector3>& positions)
	{
		if( positions.empty() )
			return;

        simd::vector3 emit_position = emitter_transform.mulpos(this->emit_position);
        simd::vector3 emit_direction = emitter_transform.muldir(this->emit_direction);

		float emitter_duration = emit_duration;
		if (emitter_duration_multiplier > 0.f)
			emitter_duration /= emitter_duration_multiplier;

		emit_direction = RandomDirectionInCone(emit_direction, positions.size() >= 2 ? emitter_template->scatter_cone_size.GetInterpolatedValue(time_alive, emitter_duration) : PI);
        
		AddParticle(i, emit_position, local_bone_parent, emit_direction, emit_scale, emit_color);
	}

	void DynamicPointParticleEmitter::UpdatePosition(const Memory::Span<const simd::vector3>& positions)
	{
		if(positions.empty() )
			return;

		emit_position = GetPosition(0, 2, positions);
		if( positions.size() >= 2 )
			emit_direction = (GetPosition(1, 2, positions) - GetPosition(0, 2, positions)).normalize();
		else
			emit_direction = simd::vector3( 1.f, 0.f, 0.f );

		if( positions.size() >= 3 )
			dynamic_scale = positions[2].x;
		else
			dynamic_scale = 1.0f;
	}

	void DynamicPointParticleEmitter::EmitParticle( size_t i, float emit_scale, const simd::vector4& emit_color, const simd::vector3& local_bone_parent, const Memory::Span<const simd::vector3>& positions)
	{
		if(positions.empty() )
			return;

		simd::vector3 emit_position = emitter_transform.mulpos( this->emit_position );
		simd::vector3 emit_direction = emitter_transform.muldir( this->emit_direction );

		float emitter_duration = emit_duration;
		if( emitter_duration_multiplier > 0.f )
			emitter_duration /= emitter_duration_multiplier;

		emit_direction = RandomDirectionInCone( emit_direction, positions.size() >= 2 ? emitter_template->scatter_cone_size.GetInterpolatedValue( time_alive, emitter_duration ) : PI );

		AddParticle( i, emit_position, local_bone_parent, emit_direction, emit_scale * dynamic_scale, emit_color );
	}

	void LineParticleEmitter3d::UpdatePosition(const Memory::Span<const simd::vector3>& positions)
	{
		if( positions.size() < 2 )
			return;

		line_percentages.resize( positions.size() );

		//Sum the lengths of line segments
		float length = 0.f;
		Percentages lengths( positions.size() ) ;
		for (size_t i = 0; i < positions.size() - 1; ++i)
		{
			lengths[ i ] = (GetPosition(i, positions.size(), positions) - GetPosition(i + 1, positions.size(), positions)).len();
			length += lengths[ i ];
		}

		line_percentages[ 0 ] = 0.0f;
		float running_total = 0.0f;
		for( size_t i = 1; i < line_percentages.size(); ++i )
		{
			line_percentages[i] = running_total + lengths[i-1] / length;
			running_total = line_percentages[i];
		}
		line_percentages[ line_percentages.size() - 1 ] = 1.0f;

		if(emitter_template->distribute_distance > 0.f && length > FLT_EPSILON)
			uniform_distance_percentage = emitter_template->distribute_distance / length;
	}

	void LineParticleEmitter3d::EmitParticle(size_t i, float emit_scale, const simd::vector4& emit_color, const simd::vector3& local_bone_parent, const Memory::Span<const simd::vector3>& positions)
	{
		if(positions.size() < 2 )
			return;

		//Generate emit position
		simd::vector3 emit_position, emit_direction;
		float distance_percent = 0.f;
		std::tie(emit_position, emit_direction, distance_percent ) = (uniform_distance_percentage > 0.f) ?
			PickPointOnLineByFactor(positions, line_percentages, fmodf(i * uniform_distance_percentage, 1.f)) :
			PickPointOnLineRandom(positions, line_percentages);

		//Transform to world space.
        emit_position = emitter_transform.mulpos(emit_position);
        emit_direction = emitter_transform.muldir(emit_direction);

		simd::vector3 original_emit_direction = emit_direction;
		float emitter_duration = emit_duration;
		if (emitter_duration_multiplier > 0.f)
			emitter_duration /= emitter_duration_multiplier;

		const float scatter_cone_size = emitter_template->scatter_cone_size.GetInterpolatedValue(time_alive, emitter_duration);
		
		emit_direction = RandomDirectionInCone(emit_direction, scatter_cone_size);

		// if locked-movement-to-bone cone size is enabled, calculate the offset rotation per particle
		simd::vector4 offset_direction(0.f);
		if(emitter_template->IsLockedMovementToBone() && emitter_template->enable_lmb_cone_size && scatter_cone_size > 0.f)
		{
			original_emit_direction = original_emit_direction.normalize();
			emit_direction = emit_direction.normalize();
			simd::vector3 quat_xyz = original_emit_direction.cross(emit_direction);
			if(quat_xyz.len() > 0.000001f)
			{
				float len_a = original_emit_direction.sqrlen();
				float len_b = emit_direction.sqrlen();
				float quat_w = std::sqrt(len_a * len_b) + original_emit_direction.dot(emit_direction);
				offset_direction = simd::quaternion(quat_xyz, quat_w).normalize();
			}
		}

        AddParticle(i, emit_position, local_bone_parent, emit_direction, emit_scale, emit_color, distance_percent, offset_direction);
	}

	void SphereParticleEmitter3d::EmitParticle( size_t i, float emit_scale, const simd::vector4& emit_color, const simd::vector3& local_bone_parent, const Memory::Span<const simd::vector3>& positions)
	{
		if( positions.size() < 2 )
			return;

		//Generate emit position
        simd::vector3 emit_position, emit_direction;
		std::tie( emit_position, emit_direction ) = PickPointOnSphere(GetPosition(0, 2, positions), radius );
		
		//Transform to world space.
        emit_position = emitter_transform.mulpos(emit_position);
        emit_direction = emitter_transform.muldir(emit_direction);

		float emitter_duration = emit_duration;
		if (emitter_duration_multiplier > 0.f)
			emitter_duration /= emitter_duration_multiplier;

		emit_direction = RandomDirectionInCone(emit_direction, emitter_template->scatter_cone_size.GetInterpolatedValue(time_alive, emitter_duration));

        AddParticle(i, emit_position, local_bone_parent, emit_direction, emit_scale, emit_color);
	}

	void SphereParticleEmitter3d::UpdatePosition(const Memory::Span<const simd::vector3>& positions)
	{
		if( positions.size() < 2 )
			return;

		radius = (GetPosition(1, 2, positions) - GetPosition(0, 2, positions)).len();
	}

	void CircleParticleEmitter3d::UpdatePosition(const Memory::Span<const simd::vector3>& positions)
	{
		if( positions.size() < 3 )
			return;

		//Need to establish a basis with 3 points
		x_basis = GetPosition(0, 3, positions) - GetPosition(1, 3, positions);
		y_basis = GetPosition(2, 3, positions) - GetPosition(1, 3, positions);
	}

	void CircleParticleEmitter3d::CircleEmitParticle(size_t i, float emit_scale, bool use_axis_as_direction, const simd::vector4& emit_color, const simd::vector3& local_bone_parent, const Memory::Span<const simd::vector3>& positions)
	{
		if (positions.size() < 3)
			return;

		//Generate emit position
		simd::vector3 emit_position, emit_direction;
		if (emitter_template->distribute_distance > 0.f)
		{
			const float angle = i * emitter_template->distribute_distance * PI / 180.f;
			std::tie(emit_position, emit_direction) = PickPointOnCircleByAngle(GetPosition(1, 3, positions), x_basis, y_basis, use_axis_as_direction, angle);
		}
		else
			std::tie(emit_position, emit_direction) = PickPointOnCircleRandom(GetPosition(1, 3, positions), x_basis, y_basis, use_axis_as_direction);

		//Transform to world space.
		emit_position = emitter_transform.mulpos(emit_position);
		emit_direction = emitter_transform.muldir(emit_direction);

		float emitter_duration = emit_duration;
		if (emitter_duration_multiplier > 0.f)
			emitter_duration /= emitter_duration_multiplier;

		emit_direction = RandomDirectionInCone(emit_direction, emitter_template->scatter_cone_size.GetInterpolatedValue(time_alive, emitter_duration));

		AddParticle(i, emit_position, local_bone_parent, emit_direction, emit_scale, emit_color);
	}

	void CircleParticleEmitter3d::EmitParticle(size_t i, float emit_scale, const simd::vector4& emit_color, const simd::vector3& local_bone_parent, const Memory::Span<const simd::vector3>& positions)
	{
		CircleEmitParticle(i, emit_scale, false, emit_color, local_bone_parent, positions);
	}

	void CircleAxisParticleEmitter3d::EmitParticle(size_t i, float emit_scale, const simd::vector4& emit_color, const simd::vector3& local_bone_parent, const Memory::Span<const simd::vector3>& positions)
	{
		CircleEmitParticle(i, emit_scale, true, emit_color, local_bone_parent, positions);
	}

	void BoxParticleEmitter3d::UpdatePosition(const Memory::Span<const simd::vector3>& positions)
	{
		if( positions.size() < 3 )
			direction = simd::vector3( 1.0f, 0.0f, 0.0f );
		else
			direction = (GetPosition(2, 3, positions) - GetPosition(1, 3, positions)).normalize();
	}

	void BoxParticleEmitter3d::EmitParticle( size_t i, float emit_scale, const simd::vector4& emit_color, const simd::vector3& local_bone_parent, const Memory::Span<const simd::vector3>& positions)
	{
		if( positions.size() < 2 )
			return;

		const simd::vector3 box_size = GetPosition(1, 3, positions) - GetPosition(0, 3, positions);

        simd::vector3 emit_position = simd::vector3(RandomFloatLocal() * box_size[0], RandomFloatLocal() * box_size[1], RandomFloatLocal() * box_size[2] ) + GetPosition(0, 3, positions);
		float emitter_duration = emit_duration;
		if (emitter_duration_multiplier > 0.f)
			emitter_duration /= emitter_duration_multiplier;

		simd::vector3 emit_direction = RandomDirectionInCone(direction, emitter_template->scatter_cone_size.GetInterpolatedValue(time_alive, emitter_duration));

        emit_position = emitter_transform.mulpos(emit_position);
        emit_direction = emitter_transform.muldir(emit_direction);

        AddParticle(i, emit_position, local_bone_parent, emit_direction, emit_scale, emit_color);
	}

	void CylinderEmitter3d::UpdatePosition(const Memory::Span<const simd::vector3>& positions)
	{
		if( positions.size() < 2 )
			return;

		const simd::vector3 direction = GetPosition(1, 2, positions) - GetPosition(0, 2, positions);

		height = direction[2];
		radius = simd::vector2( direction[0], direction[1] ).len();
	}

	void CylinderEmitter3d::EmitParticle( size_t i, float emit_scale, const simd::vector4& emit_color, const simd::vector3& local_bone_parent, const Memory::Span<const simd::vector3>& positions)
	{
		if(positions.size() < 2)
			return;

		const float theta = RandomAngleLocal( );
		const float r = radius * sqrtf(RandomFloatLocal() );

		const float s = sin( theta );
		const float c = cos( theta );
        simd::vector3 emit_position = simd::vector3( s * r, c * r, RandomFloatLocal() * height ) + GetPosition(0, 2, positions);
        simd::vector3 emit_direction = simd::vector3( s, c, 0.0f );

        emit_position = emitter_transform.mulpos(emit_position);
        emit_direction = emitter_transform.muldir(emit_direction);

        AddParticle(i, emit_position, local_bone_parent, emit_direction, emit_scale, emit_color);
	}

	void CylinderAxisEmitter3d::UpdatePosition(const Memory::Span<const simd::vector3>& positions)
	{
		if(positions.size() < 3)
			return;

		radius = (GetPosition(0, 3, positions) - GetPosition(1, 3, positions)).len();
		simd::vector3 z_axis = GetPosition(2, 3, positions) - GetPosition(1, 3, positions);
		height = z_axis.len();
		z_axis = z_axis.normalize();

		simd::vector3 x_axis, y_axis;
		float abs_dir_x = std::fabs(z_axis.x);
		float abs_dir_y = std::fabs(z_axis.y);
		float abs_dir_z = std::fabs(z_axis.z);
		float min = std::min(std::min(abs_dir_x, abs_dir_y), abs_dir_z);
		if(min == abs_dir_x)
			x_axis = simd::vector3(0.f, -z_axis.z, z_axis.y);
		else if(min == abs_dir_y)
			x_axis = simd::vector3(-z_axis.z, 0.f, z_axis.x);
		else
			x_axis = simd::vector3(-z_axis.y, z_axis.x, 0.f);
		y_axis = x_axis.cross(z_axis);
		axis_rotation_transform = simd::matrix(simd::vector4(x_axis, 0.f), simd::vector4(y_axis, 0.f), simd::vector4(z_axis, 0.f), simd::vector4(0.f, 0.f, 0.f, 1.f));
	}

	void CylinderAxisEmitter3d::EmitParticle(size_t i, float emit_scale, const simd::vector4& emit_color, const simd::vector3& local_bone_parent, const Memory::Span<const simd::vector3>& positions)
	{
		if(positions.size() < 3)
			return;

		const float theta = RandomAngleLocal();
		const float r = radius * sqrtf(RandomFloatLocal());

		const float s = sin(theta);
		const float c = cos(theta);
		simd::vector3 emit_position = simd::vector3(s * r, c * r, RandomFloatLocal() * height);
		emit_position = axis_rotation_transform.mulpos(emit_position) + GetPosition(1, 3, positions);
		simd::vector3 emit_direction = simd::vector3(axis_rotation_transform[2].x, axis_rotation_transform[2].y, axis_rotation_transform[2].z);

		emit_position = emitter_transform.mulpos(emit_position);
		emit_direction = emitter_transform.muldir(emit_direction);

		AddParticle(i, emit_position, local_bone_parent, emit_direction, emit_scale, emit_color);
	}

	void SphereSurfaceEmitter3d::UpdatePosition(const Memory::Span<const simd::vector3>& positions)
	{
		if( positions.size() < 2 )
			return;

		radius = (GetPosition(1, 2, positions) - GetPosition(0, 2, positions)).len();
		pos = GetPosition(0, 2, positions);
	}

	void SphereSurfaceEmitter3d::EmitParticle( size_t i, float emit_scale, const simd::vector4& emit_color, const simd::vector3& local_bone_parent, const Memory::Span<const simd::vector3>& positions)
	{
		if(positions.size() < 2)
			return;

		const float theta = RandomAngleLocal();
		const float phi = acosf( RandomFloatLocal( -1.0f, 1.0f ) );
		
		const float sphi = sinf( phi );
		const simd::vector3 random_sphere_point( cosf( theta ) * sphi, sinf( theta ) * sphi, cosf( phi ) );

        simd::vector3 emit_position = random_sphere_point * radius + pos;
        simd::vector3 emit_direction = -random_sphere_point;

        emit_position = emitter_transform.mulpos(emit_position);
        emit_direction = emitter_transform.muldir(emit_direction);

        AddParticle(i, emit_position, local_bone_parent, emit_direction, emit_scale, emit_color);
	}

	void CameraFrustumPlaneEmitter3d::EmitParticle( size_t i, float emit_scale, const simd::vector4& emit_color, const simd::vector3& local_bone_parent, const Memory::Span<const simd::vector3>& positions)
	{
		if (!camera)
			return;

		const auto* frustum_points = camera->GetFrustum().GetPoints();

		const float zdist = RandomFloatLocal() * 0.2f;
		const float xdist = RandomFloatLocal() * 1.5f - 0.25f;
                
        simd::vector3 line_begin = ((simd::vector3&)frustum_points[ 0 ]).lerp((simd::vector3&)frustum_points[ 1 ], zdist);
        simd::vector3 line_end = ((simd::vector3&)frustum_points[ 4 ]).lerp((simd::vector3&)frustum_points[ 5 ], zdist);

        simd::vector3 emit_position = line_begin.lerp(line_end, xdist);
        emit_position[2] -= 20.0f;

        simd::vector3 emit_direction = ( line_begin - line_end ).normalize();

        AddParticle(i, emit_position, local_bone_parent, emit_direction, emit_scale, emit_color);
	}

	void CameraFrustumPlaneEmitter3d::UpdatePosition(const Memory::Span<const simd::vector3>& positions)
	{
		
	}
}