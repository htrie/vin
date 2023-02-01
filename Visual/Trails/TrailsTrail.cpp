#include "Visual/Renderer/EffectGraphSystem.h"

#include "Common/Utility/StackAlloc.h"
#include "Common/Utility/Logger/Logger.h"
#include "Common/Utility/StringManipulation.h"

#include "TrailSegment.h"
#include "TrailsTrail.h"

//TODO: Local Velocity (relative to segment basis)
//TODO: Trail needs to be paused when monsters teleport due to network packages
//TODO: Trail should (maybe?) be paused on knock-back and similar gameplay effects?

namespace Trails
{
	namespace {
		// Minimal delta-time for speed computations (avoids floating point instability)
		constexpr float MinDT = 1e-3f;

		bool PassSpeedThreshold(float speed, float min_speed, float max_speed, bool is_paused)
		{
			if (is_paused)
			{
				if (min_speed > 0.0f && speed < min_speed * 1.1f)
					return false;

				if (max_speed > 0.0f && speed > max_speed * 0.9f)
					return false;
			}
			else
			{
				if (min_speed > 0.0f && speed < min_speed * 0.9f)
					return false;

				if (max_speed > 0.0f && speed > max_speed * 1.1f)
					return false;
			}

			return true;
		}

		bool TransformsEqual(const simd::matrix& a, const simd::matrix& b)
		{
			const auto diff = a * b.transpose();
			for (unsigned i = 0; i < 4; i++)
				for (unsigned j = 0; j < 4; j++)
					if (std::abs(diff[i][j] - (i == j ? 1.0f : 0.0f)) > 1e-2f)
						return false;
			
			return true;
		}

		float FlattenScale(const simd::vector3& scale)
		{
			return std::max({ std::abs(scale.x), std::abs(scale.y), std::abs(scale.z) });
		}

		struct DeltaTransform
		{
			simd::vector3 position;
			simd::vector3 scale;
			simd::quaternion rotation;
			simd::matrix transform;

			DeltaTransform(float elapsed_time, float time_offset, const simd::vector3& delta_pos, const simd::vector3& delta_scale, const simd::quaternion& delta_rot, const simd::matrix& last_transform)
			{
				const float t = elapsed_time < 1e-5f ? 0.0f : std::clamp((elapsed_time - time_offset) / elapsed_time, 0.0f, 1.0f);

				position = simd::vector3(0.0f, 0.0f, 0.0f).lerp(delta_pos, t);
				scale = simd::vector3(1.0f, 1.0f, 1.0f).lerp(delta_scale, t);
				rotation = simd::quaternion::identity().slerp(delta_rot, t);

				transform = simd::matrix::scalerotationtranslation(scale, rotation, position) * last_transform;
			}
		};
	}

	Trail::Trail( TrailsTemplateHandle trail_template, const simd::vector3& position, const std::vector< SegmentVertex >& offsets, const simd::matrix& transform, float delay, float emitter_duration)
		: trail_template( std::move( trail_template ) )
		, instance_position( position )
		, instance_offsets( offsets )
		, transform( transform )
		, emitter_duration(emitter_duration)
	{
		Reinitialise(delay);
	}

	Trail::~Trail()
	{
	}

	void Trail::SetAnimationStorage(const Animation::DetailedStorageID& id, const Memory::Span<const unsigned>& bone_indices)
	{
		animation_id = id;
		animation_bones.clear();
		animation_bones.reserve(bone_indices.size());
		for (const auto& a : bone_indices)
			animation_bones.push_back(a);
	}

	void Trail::SetAnimationSpeedMultiplier(float animspeed_multiplier)
	{
		if (is_stopped && animation_speed > 0.0f)
			return;

		animation_speed = std::max(animspeed_multiplier, 0.0f);
	}

	BoundingBox Trail::MergeDynamicBoundingBox()
	{
		simd::vector3 min(FLT_MAX, FLT_MAX, FLT_MAX);
		simd::vector3 max(-FLT_MAX, -FLT_MAX, -FLT_MAX);

		ForEachPiece([&](const Piece& piece, bool first, bool last) 
		{
			piece.ForEachSegment([&](const Segment& segment, unsigned index, unsigned ordered_index) 
			{
				const auto seg_pos = segment.position;
				min.x = std::min(min.x, seg_pos.x);
				min.y = std::min(min.y, seg_pos.y);
				min.z = std::min(min.z, seg_pos.z);

				max.x = std::max(max.x, seg_pos.x);
				max.y = std::max(max.y, seg_pos.y);
				max.z = std::max(max.z, seg_pos.z);

				for (unsigned i = 0; i < num_vertexes_per_segment; ++i)
				{
					const auto pos = seg_pos + segment.GetVertices()[i].offset;

					min.x = std::min(min.x, pos.x);
					min.y = std::min(min.y, pos.y);
					min.z = std::min(min.z, pos.z);

					max.x = std::max(max.x, pos.x);
					max.y = std::max(max.y, pos.y);
					max.z = std::max(max.z, pos.z);
				}
			});
		});

		return BoundingBox(min, max);
	}

	size_t Trail::GetSegmentSize( ) const
	{
		return sizeof(Segment)+sizeof(SegmentVertex)*num_vertexes_per_segment;
	}

	void Trail::ReinitialiseTrail()
	{
		reinitialise = true;
	}

	unsigned Trail::NumVerticesPerSegment() const
	{
		return num_vertexes_per_segment;
	}

	VertexType Trail::GetVertexType() const
	{
		return trail_template->GetVertexType(camera_facing);
	}

	unsigned Trail::GetMergeId() const
	{
		const auto vertex_type = GetVertexType();
		const auto num_vertices = NumVerticesPerSegment();

		uint64_t v[] = {
			uint64_t(vertex_type),
			uint64_t(num_vertices),
		};

		return MurmurHash2(v, int(sizeof(v)), 0xc58f1a7b);
	}

	void Trail::Reinitialise(float delay)
	{
		//Reinitialise to starting state		
		num_segments = trail_template->GetMaxSegments();
		current_segment = 0;
		first_dead = last_dead = current_segment;
		
		emit_delay = delay;
		remaining_lifetime = trail_template->lifetime;
		remaining_start_distance = trail_template->start_distance_threshold;
		last_transform = transform;

		emit_color_r = trail_template->emit_color_r.MakeTrack(1.0f);
		emit_color_g = trail_template->emit_color_g.MakeTrack(1.0f);
		emit_color_b = trail_template->emit_color_b.MakeTrack(1.0f);
		emit_color_a = trail_template->emit_color_a.MakeTrack(1.0f);
		emit_scale = trail_template->emit_scale.MakeTrack(1.0f);
		color_r = trail_template->color_r.MakeNormalizedTrack();
		color_g = trail_template->color_g.MakeNormalizedTrack();
		color_b = trail_template->color_b.MakeNormalizedTrack();
		color_a = trail_template->color_a.MakeNormalizedTrack();
		velocity_model_x = trail_template->velocity_model_x.MakeNormalizedTrack();
		velocity_model_y = trail_template->velocity_model_y.MakeNormalizedTrack();
		velocity_model_z = trail_template->velocity_model_z.MakeNormalizedTrack();
		velocity_world_x = trail_template->velocity_world_x.MakeNormalizedTrack();
		velocity_world_y = trail_template->velocity_world_y.MakeNormalizedTrack();
		velocity_world_z = trail_template->velocity_world_z.MakeNormalizedTrack();
		segment_scale = trail_template->scale.MakeNormalizedTrack(1.0f);

		if(UseLocalSpace())
			old_position = instance_position;
		else
			old_position = transform.mulpos(instance_position);

		camera_facing = instance_offsets.size() < 2 || trail_template->camera_facing;
		num_vertexes_per_segment = unsigned(trail_template->emit_at_tip ? 2 : camera_facing ? 0 : instance_offsets.size());

		is_stopped = false;
		last_stop_time = -1.0f;
		is_paused = false;
		is_manually_paused = false;
		was_paused = false;

		distance_since_start = 0.0f;
		time_since_start = 0.0f;
		time_since_random_value = 0.0f;

		random_emitter_seed = simd::RandomFloatLocal();
		RollNewVariance();
		RollNewVariance(); // Call twice to initialise last_random_value

		InitSegments();
	}

	void Trail::RollNewVariance()
	{
		last_random_value = random_value;
		random_value = simd::vector4(simd::RandomFloatLocal(), simd::RandomFloatLocal(), simd::RandomFloatLocal(), simd::RandomFloatLocal());
	}

	void Trail::StopEmitting(float stop_time)
	{
		if (is_stopped)
			return;

		last_stop_time = stop_time;
		is_stopped = true;
	}

	void Trail::OnOrphaned()
	{
		// Workaround to prevent the trail from 'freezing' in time if it got orphaned with zero animation speed (p[rimarily happens in tools)
		if (trail_template->scale_with_animation && animation_speed < 1e-4f)
			last_dead = first_dead = current_segment;
	}

	void Trail::Pause()
	{
		is_paused = true;
		is_manually_paused = true;
	}

	void Trail::Resume()
	{
		is_manually_paused = false;
	}

	unsigned Trail::GetLastSegment() const
	{
		return (current_segment + num_segments - 1) % num_segments;
	}

	Segment& Trail::GetSegment(unsigned index)
	{
		return *reinterpret_cast<Segment*>(&segments[index * GetSegmentSize()]);
	}

	bool Trail::UpdateVariance(float time_offset, float elapsed_time)
	{
		if (trail_template->randomness_frequency > 0.0f && time_since_random_value >= elapsed_time && time_since_random_value - time_offset > std::max(trail_template->randomness_frequency, MinDT))
		{
			RollNewVariance();
			time_since_random_value = time_offset;
			return true;
		}

		return false;
	}

	void Trail::NewSegment(float elapsed_time, float time_offset, float distance_offset, const simd::vector3& local_position, const simd::vector3& global_position, const Memory::Span<const SegmentVertex>& bone_data, const simd::matrix& current_transform, float total_distance, float current_scale)
	{
		if (was_paused && current_segment != last_dead)
			has_new_segment = true;

		if (WriteSegmentData(elapsed_time, time_offset, distance_offset, local_position, global_position, bone_data, current_transform, total_distance, current_scale))
		{
			was_paused = false;
			has_new_segment = true;
			last_emit_transform = current_transform;

			UpdateVariance(time_offset, elapsed_time);
			if (current_segment == last_dead)
				WriteSegmentData(elapsed_time, time_offset, distance_offset, local_position, global_position, bone_data, current_transform, total_distance, current_scale);
		}
	}

	bool Trail::WriteSegmentData(float elapsed_time, float time_offset, float distance_offset, const simd::vector3& local_position, const simd::vector3& global_position, const Memory::Span<const SegmentVertex>& bone_data, const simd::matrix& current_transform, float total_distance, float current_scale)
	{
		if (current_segment != first_dead && (global_position - GetSegment(GetLastSegment()).position).sqrlen() < 1e-1f && TransformsEqual(last_emit_transform, current_transform))
			return false;

		if (has_new_segment)
		{
			has_new_segment = false;

			current_segment++;
			if (current_segment == num_segments)
				current_segment = 0;

			const auto next_segment = current_segment + 1 == num_segments ? 0 : current_segment + 1;
			if (current_segment == last_dead)
				last_dead = next_segment;

			if (current_segment == first_dead)
				first_dead = next_segment;
		}

		auto* segment = reinterpret_cast<Segment*>(&segments[current_segment * GetSegmentSize()]);

		float local_emitter_time = 0.0f;
		if (trail_template->lifetime > 0.0f)
			local_emitter_time = (trail_template->lifetime - (remaining_lifetime + time_offset)) / trail_template->lifetime;
		else if (emitter_duration > 0.0f)
		{
			if (!trail_template->scale_with_animation)
				local_emitter_time = (emitter_time - (time_offset * animation_speed)) / emitter_duration;
			else
				local_emitter_time = (emitter_time - time_offset) / emitter_duration;
		}

		float base_scale = emit_scale.Interpolate(local_emitter_time);
		if (num_vertexes_per_segment < 2 && trail_template->lock_scale)
			base_scale *= current_scale;

		local_emitter_time = std::clamp(local_emitter_time, 0.0f, 1.0f);
		segment->emit_colour = simd::vector4(emit_color_r.Interpolate(local_emitter_time), emit_color_g.Interpolate(local_emitter_time), emit_color_b.Interpolate(local_emitter_time), emit_color_a.Interpolate(local_emitter_time));
		segment->emit_scale = base_scale;
		segment->phase = ComputeSegmentPhase(0.0f, time_offset, distance_offset, 0, random_emitter_seed);
		segment->emitter_variance = random_emitter_seed;
		segment->restart = was_paused;
		segment->gradient = total_distance > 1e-5f ? distance_offset / total_distance : 1.0f;
		segment->distance_from_tip = distance_offset;
		segment->time_since_start = time_since_start - time_offset;
		segment->distance_since_start = distance_since_start - distance_offset;
		segment->elapsed_time = time_offset;

		if (trail_template->randomness_frequency > 0.0f)
			segment->variance = last_random_value.lerp(random_value, Clamp((time_since_random_value - time_offset) / std::max(trail_template->randomness_frequency, MinDT), 0.0f, 1.0f));
		else
			segment->variance = last_random_value;

		auto ApplyTransform = [local_space = UseLocalSpace(), &current_transform](const simd::vector3& pos)
		{
			//IF we are locked to parent then all the segments are in the parent space
			if (local_space)
				return pos;
			else
				return current_transform.muldir(pos);
		};

		const auto offset_count = instance_offsets.size();
		assert(offset_count <= bone_data.size());

		if (offset_count < num_vertexes_per_segment)
			return true; // Special case for tools when deleting bones from an bone group

		segment->position = global_position;

		if (camera_facing)
		{
			if (offset_count > 1 && trail_template->emit_at_tip)
			{
				for (unsigned i = 0; i < num_vertexes_per_segment; ++i)
				{
					auto& vertex_out = segment->GetVertices()[i];
					vertex_out.offset = simd::vector3(0.0f, 0.0f, 0.0f);
					vertex_out.v = 0.0f;
				}

				if (offset_count > 1)
				{
					const auto& vertex_0 = bone_data[trail_template->reverse_bone_group ? offset_count - 1 : 0].offset;
					const auto& vertex_1 = bone_data[trail_template->reverse_bone_group ? offset_count - 2 : 1].offset;

					const auto offset = ApplyTransform(vertex_0);
					const auto direction = ApplyTransform((vertex_1 - vertex_0).normalize() * trail_template->emit_width);
					segment->position += offset + direction;
				}
				else
				{
					const auto offset = trail_template->reverse_bone_group ? bone_data[offset_count - 1].offset : bone_data[0].offset;
					segment->position += ApplyTransform(offset * (1.0f - (trail_template->emit_width / offset.len())));
				}
			}
		}
		else
		{
			if (trail_template->emit_at_tip)
			{
				assert(num_vertexes_per_segment >= 2);
				const auto& vertex_0 = bone_data[trail_template->reverse_bone_group ? offset_count - 1 : 0].offset;
				const auto& vertex_1 = bone_data[trail_template->reverse_bone_group ? offset_count - 2 : 1].offset;

				const auto width = trail_template->emit_width * (trail_template->lock_scale ? current_scale : 1.0f);
				const auto offset = ApplyTransform(vertex_0);
				const auto direction = ApplyTransform((vertex_1 - vertex_0).normalize() * width);

				for (unsigned a = 0; a < num_vertexes_per_segment; a++)
				{
					auto& vertex_out = segment->GetVertices()[a];
					const auto v = float(a) / float(num_vertexes_per_segment - 1);

					vertex_out.offset = offset + (direction * v);
					vertex_out.v = a < num_vertexes_per_segment ? v : (1.0f - v);
				}
			}
			else
			{
				for (unsigned a = 0; a < num_vertexes_per_segment; a++)
				{
					const auto v_i = trail_template->reverse_bone_group ? offset_count - (a + 1) : a;
					const auto& v = bone_data[v_i];

					auto& vertex_out = segment->GetVertices()[a];
					vertex_out.offset = ApplyTransform(v.offset);
					vertex_out.v = v.v;
				}
			}
		}

		FillSegmentVertices(*segment);
		return true;
	}

	void Trail::WriteSegment(float elapsed_time, float time_offset, float distance_offset, const simd::vector3& local_position, const simd::vector3& global_position, const Memory::Span<const SegmentVertex>& bone_data, const simd::matrix& current_transform, float total_distance, float current_scale)
	{
		WriteSegmentData(elapsed_time, time_offset, distance_offset, local_position, global_position, bone_data, current_transform, total_distance, current_scale);
	}

	void Trail::FillSegmentVertices(Segment& segment) const
	{
		if (num_vertexes_per_segment > 0)
		{
			auto vertex = segment.GetVertices();
			vertex[0].width = 0.0f;

			const auto num_bones = num_vertexes_per_segment;
			for (unsigned a = 1; a < num_vertexes_per_segment; a++)
				vertex[a].width = vertex[a - 1].width + ((vertex[a].offset - vertex[a - 1].offset).len() * std::max(0.0f, segment.scale));

			const auto mid_width = vertex[num_vertexes_per_segment - 1].width * 0.5f;
			for (unsigned a = 0; a < num_vertexes_per_segment; a++)
				vertex[a].width -= mid_width;
		}
	}

	void Trail::InitSegments()
	{
		const auto segment_size = GetSegmentSize();
		segments.resize(segment_size * num_segments, uint8_t(0));
		segments.shrink_to_fit();
	}

	bool Trail::UseLocalSpace() const
	{
		return trail_template->locked_to_parent;
	}

	bool Trail::StartTrail(float elapsed_time, float time_offset, float distance_offset, float speed, float distance)
	{
		if (remaining_start_distance > 0.0f)
		{
			remaining_start_distance -= distance;
			if (remaining_start_distance > 0.0f)
				return false;
		}

		if (speed < trail_template->emit_start_speed)
			return false;

		if (elapsed_time + (MinDT * 0.5f) < emit_delay)
			return false;

		remaining_lifetime = trail_template->lifetime > 0.0f ? trail_template->lifetime - time_offset : -1.0f;
		remaining_stop_distance = trail_template->stop_distance_threshold;
		is_paused = is_manually_paused;
		time_since_start = time_offset;
		distance_since_start = distance_offset; 
		random_emitter_seed = simd::RandomFloatLocal();
		RollNewVariance();
		RollNewVariance();
		first_dead = last_dead = current_segment;

		if (trail_template->lifetime > 0.0f && remaining_lifetime <= 0.0f)
		{
			is_stopped = true;
			remaining_lifetime = -1.0f;
			return false;
		}

		return true;
	}

	bool Trail::UpdatePause(float speed, float time_offset, float distance_offset)
	{
		if (is_paused)
		{
			if (is_manually_paused)
				return false;

			if (PassSpeedThreshold(speed, trail_template->emit_min_speed, trail_template->emit_max_speed, true))
			{
				is_paused = false;
				was_paused = true;

				time_since_start = time_offset;
				distance_since_start = distance_offset;
				random_emitter_seed = simd::RandomFloatLocal();
				RollNewVariance();
				RollNewVariance();
				return true;
			}

			return false;
		}

		if (!PassSpeedThreshold(speed, trail_template->emit_min_speed, trail_template->emit_max_speed, false))
		{
			is_paused = true;
			return false;
		}

		return true;
	}

	float Trail::ComputeSegmentPhase(float last_phase, float elapsed_time, float distance, size_t index, float variance) const
	{
		const float v = (variance - 0.5f) * 2.0f;
		const float segment_duration = trail_template->segment_duration + (v * trail_template->segment_duration_variance);
		const float trail_length = trail_template->trail_length + (v * trail_template->trail_length_variance);

		const float segment_phase_multiplier = trail_template->scale_segment_duration && !trail_template->scale_with_animation ? animation_speed : 1.0f;
		const float time_phase_delta = (segment_duration > 0.0f ? elapsed_time * segment_phase_multiplier / segment_duration : FLT_MAX) * std::max(is_stopped ? trail_template->update_speed_multiplier_on_death : 1.0f, 1.0f);
		const float length_phase_delta = trail_length > 0.0f ? (distance / trail_length) - last_phase : 0.0f;

		// The max function causes a discontinuity in the second derivative, which is fine. It also guarantees that we always respect time_phase_delta to 100% if the trail is standing still
		const float phase_delta = std::max(Lerp(time_phase_delta, length_phase_delta, trail_template->duration_length_ratio), time_phase_delta);

		const auto max_index = num_segments - (NumDeadSegments + 1);
		const auto min_index = max_index > 10 ? max_index - 10 : 0;
		return std::max(last_phase + phase_delta, index < min_index ? 0.0f : index > max_index ? FLT_MAX : static_cast<float>(index - min_index) / (max_index - min_index));
	}

	unsigned Trail::FindLastDead(unsigned index, unsigned first_dead, unsigned last_dead, unsigned num_segments)
	{
		const auto ld = (index + num_segments - (NumDeadSegments - 1)) % num_segments;

		const bool t_a = ld < index;
		const bool t_b = ld > last_dead;

		return (index > last_dead && t_a && t_b) || (index < last_dead && t_a != t_b) ? ld : last_dead;
	}

	float Trail::UpdateOldSegments(float elapsed_time, float new_distance)
	{
		// Update old segments
		const auto segment_size = GetSegmentSize();
		float final_total_distance = 0.0f;

		ForEachPiece([&](const Piece& piece, bool first, bool last)
		{
			const auto num_segs = piece.NumTotalSegments();
			piece.ForEachSegmentWithDistance([&](const Segment&, unsigned index, unsigned ordered_index, float distance, float total_distance, float segment_total_distance)
			{
				assert(ordered_index < num_segs);
				Segment& segment = *reinterpret_cast<Segment*>(&segments[index * segment_size]);

				segment.phase = ComputeSegmentPhase(segment.phase, elapsed_time, distance, num_segs - ordered_index, segment.emitter_variance);
				segment.elapsed_time = elapsed_time;

				if (first && segment.phase > 1.0f)
				{
					const bool in_range_a = index > first_dead && index <= current_segment;
					const bool in_range_b = index > first_dead != index <= current_segment;
					const bool in_range = current_segment > first_dead ? in_range_a : in_range_b;

					if (in_range)
					{
						last_dead = FindLastDead(index, first_dead, last_dead, num_segments);
						first_dead = index;

						for (unsigned a = last_dead; true; a = (a + 1) % num_segments)
						{
							const Segment& s = *reinterpret_cast<Segment*>(&segments[a * segment_size]);
							if (s.restart)
								last_dead = a;

							if (a == first_dead)
								break;
						}
					}
				}

				if (last)
					final_total_distance = total_distance;
			});
		});

		return final_total_distance;
	}

	void Trail::UpdateAllSegments()
	{
		// Transform basis vectors for X and Y

		const auto model_basis_x = UseLocalSpace() ? simd::vector3(1.0f, 0.0f, 0.0f) : transform.muldir(simd::vector3(1.0f, 0.0f, 0.0f));
		const auto model_basis_y = UseLocalSpace() ? simd::vector3(0.0f, 1.0f, 0.0f) : transform.muldir(simd::vector3(0.0f, 1.0f, 0.0f));
		const auto model_basis_z = model_basis_x.cross(model_basis_y);

		const auto segment_size = GetSegmentSize();
		const bool standing_moving = trail_template->move_standing_uvs;

		ForEachPiece([&](const Piece& piece, bool first, bool last)
		{
			float base_distance = 0.0f;

			piece.ForEachSegment([&](const Segment&, unsigned index, unsigned ordered_index)
			{
				Segment& segment = *reinterpret_cast<Segment*>(&segments[index * segment_size]);

				const auto x_model_velocity = model_basis_x * velocity_model_x.Interpolate(segment.phase, segment.variance.x);
				const auto y_model_velocity = model_basis_y * velocity_model_y.Interpolate(segment.phase, segment.variance.y);
				const auto z_model_velocity = model_basis_z * velocity_model_z.Interpolate(segment.phase, segment.variance.z);
				const auto x_velocity = simd::vector3(velocity_world_x.Interpolate(segment.phase, segment.variance.x), 0.0f, 0.0f);
				const auto y_velocity = simd::vector3(0.0f, velocity_world_y.Interpolate(segment.phase, segment.variance.y), 0.0f);
				const auto z_velocity = simd::vector3(0.0f, 0.0f, velocity_world_z.Interpolate(segment.phase, segment.variance.z));

				const auto color = simd::vector4(color_r.Interpolate(segment.phase), color_g.Interpolate(segment.phase), color_b.Interpolate(segment.phase), color_a.Interpolate(segment.phase));
				const auto velocity = x_model_velocity + y_model_velocity + z_model_velocity + x_velocity + y_velocity + z_velocity;

				segment.position += velocity * segment.elapsed_time;
				segment.scale = segment.emit_scale * segment_scale.Interpolate(segment.phase, segment.variance.x);
				segment.colour = segment.emit_colour * color;

				if (index == GetLastSegment())
					last_emit_transform = last_emit_transform * simd::matrix::translation(velocity * segment.elapsed_time);

				if (ordered_index == 0)
					base_distance = segment.distance_since_start;
			});

			piece.ForEachSegmentWithDistance([&](const Segment&, unsigned index, unsigned ordered_index, float distance, float total_distance, float segment_total_distance)
			{
				Segment& segment = *reinterpret_cast<Segment*>(&segments[index * segment_size]);

				segment.gradient = total_distance > 1e-5f ? distance / total_distance : 1.0f;
				segment.distance_from_tip = distance;
				if(standing_moving)
					segment.distance_since_start = base_distance + segment_total_distance - distance;

				FillSegmentVertices(segment);
			});
		});
	}

	float Trail::UpdateLifetime(float elapsed_time, float speed)
	{
		if (is_stopped)
			elapsed_time = last_stop_time;

		const float lifetime_scale = trail_template->scale_lifetime && !trail_template->scale_with_animation ? animation_speed : 1.0f;
		if (remaining_lifetime >= 0.0f && lifetime_scale > 0.0f && !is_paused)
		{
			const float delta = elapsed_time * lifetime_scale;
			if (remaining_lifetime > delta)
				remaining_lifetime -= delta;
			else
			{
				const auto r = remaining_lifetime / lifetime_scale;
				remaining_lifetime = -1.0f;
				last_stop_time = -1.0f;
				is_stopped = true;

				return r;
			}
		}

		return elapsed_time;
	}

	float Trail::ComputePath(TempPath* temp_data, size_t frame_count, float elapsed_time, const simd::vector3& delta_pos, const simd::quaternion& delta_rot, const simd::vector3& delta_scale, const simd::vector3& current_position, float time_factor)
	{
		const auto last_scale = FlattenScale(last_transform.scale());

		float distance = 0.0f;
		simd::vector3 last_position = old_position;
		if (!animation_bones.empty())
		{
			// Offset by one because last 'frame' is the current frame data, instead of comming from the detailed storage
			for (size_t a = 1; a < frame_count; a++)
			{
				const auto& frame = Animation::System::Get().GetDetailedStorage(animation_id, a - 1);
				if (frame.bones.empty())
					continue;

				auto sum = simd::vector3(0.0f, 0.0f, 0.0f);
				for (const auto& bone_index : animation_bones)
					sum += bone_index < frame.bones.size() ? frame.bones[bone_index] : simd::vector3();

				sum /= float(animation_bones.size());

				const float time_offset = elapsed_time - (frame.elapsed_time * time_factor);
				const auto current_transform = DeltaTransform(elapsed_time, time_offset, delta_pos, delta_scale, delta_rot, last_transform);
				const auto current_pos = UseLocalSpace() ? sum : current_transform.transform.mulpos(sum);

				distance += (current_pos - last_position).len();
				temp_data[a - 1].current_transform = current_transform.transform;
				temp_data[a - 1].pos = sum;
				temp_data[a - 1].world_pos = current_pos;
				temp_data[a - 1].distance = distance;
				temp_data[a - 1].scale = last_scale * FlattenScale(current_transform.scale);
				last_position = current_pos;
			}
		}

		distance += (current_position - last_position).len();
		temp_data[frame_count - 1].current_transform = transform;
		temp_data[frame_count - 1].pos = instance_position;
		temp_data[frame_count - 1].world_pos = current_position;
		temp_data[frame_count - 1].distance = distance;
		temp_data[frame_count - 1].scale = last_scale * FlattenScale(delta_scale);
		old_position = current_position;

		return distance;
	}

	std::tuple<float, float> Trail::EmitAnimationSegments(TempPath* temp_data, size_t frame_count, float elapsed_time, bool first_frame, float time_factor, float frame_speed, float total_distance)
	{
		Utility::StackArray<SegmentVertex> bone_positions(STACK_ALLOCATOR(SegmentVertex, animation_bones.size()));

		const float total_frame_distance = temp_data[frame_count - 1].distance;
		float last_distance = 0.0f;
		float last_time = 0.0f;
		for (size_t a = 1; a < frame_count; a++)
		{
			if (is_stopped)
				break;

			const auto& frame = Animation::System::Get().GetDetailedStorage(animation_id, a - 1);
			if (frame.bones.empty())
				continue;

			auto frame_elapsed = frame.elapsed_time * time_factor;
			if (frame_elapsed > elapsed_time)
			{
				if (frame_elapsed < elapsed_time + (MinDT * 0.5f))
					frame_elapsed = elapsed_time;
				else
					break;
			}

			const float distance = temp_data[a - 1].distance - last_distance;
			const float time_offset = elapsed_time - frame_elapsed;
			const float dt = frame_elapsed - last_time;

			const bool is_first_segment = current_segment == last_dead;

			if (!first_frame || trail_template->emit_start_speed < 0.0f)
			{
				float total_length = 0.0f;
				const auto& local_position = temp_data[a - 1].pos;
				const auto& world_position = temp_data[a - 1].world_pos;

				for (size_t b = 0; b < animation_bones.size(); b++)
				{
					const auto p = animation_bones[b] < frame.bones.size() ? frame.bones[animation_bones[b]] : simd::vector3();
					const auto d = b > 0 ? (p - bone_positions[b - 1].offset).len() : 0.0f;
					total_length += d;

					bone_positions[b].offset = p;
					bone_positions[b].v = total_length;
				}

				if (total_length > 1e-5f)
				{
					for (size_t b = 0; b < animation_bones.size(); b++)
					{
						bone_positions[b].offset -= local_position;
						bone_positions[b].v /= total_length;
					}
				}
				else
				{
					for (size_t b = 0; b < animation_bones.size(); b++)
					{
						bone_positions[b].offset = simd::vector3(0, 0, 0);
						bone_positions[b].v = 0.0f;
					}
				}

				const float distance_offset = total_frame_distance - temp_data[a - 1].distance;

				if (!is_first_segment)
				{
					remaining_stop_distance -= distance;
					if (UpdatePause(frame_speed, time_offset, distance_offset))
					{
						NewSegment(frame_elapsed, time_offset, distance_offset, local_position, world_position, bone_positions, temp_data[a - 1].current_transform, total_distance, temp_data[a - 1].scale); // Output keyframe

						if (remaining_stop_distance < 0.0f && trail_template->stop_distance_threshold >= 0.0f)
						{
							is_stopped = true;
							is_paused = true; // Pause to prevent final segment to be written
						}
					}
				}
				else if (StartTrail(frame_elapsed, time_offset, distance_offset, frame_speed, temp_data[a - 1].distance - last_distance))
					NewSegment(frame_elapsed, time_offset, distance_offset, local_position, world_position, bone_positions, temp_data[a - 1].current_transform, total_distance, temp_data[a - 1].scale); // Start trail
			}

			last_distance = temp_data[a - 1].distance;
			last_time = frame_elapsed;
		}

		return std::make_tuple(last_distance, last_time);
	}

	void Trail::UpdateLastSegment(float elapsed_time, float last_time, bool first_frame, float frame_speed, float trail_distance, const simd::vector3& current_position, float total_distance)
	{
		const bool is_first_segment = current_segment == last_dead;
		const auto current_scale = FlattenScale(transform.scale());

		if (!is_first_segment)
		{
			if (UpdatePause(frame_speed, 0.0f, 0.0f))
			{
				if (was_paused)
					NewSegment(elapsed_time, 0.0f, 0.0f, instance_position, current_position, instance_offsets, transform, total_distance, current_scale);
				else
				{
					const float rigid_body_distance = UseLocalSpace() ? 0.0f : (transform.mulpos(instance_position) - last_emit_transform.mulpos(instance_position)).len();

					const auto last_time = GetSegment(has_new_segment ? current_segment : GetLastSegment()).time_since_start;
					float max_length = trail_template->max_segment_length;
					float max_time = trail_template->segment_duration * 0.25f;
					
					if (rigid_body_distance > max_length || time_since_start - last_time > max_time)
						NewSegment(elapsed_time, 0.0f, 0.0f, instance_position, current_position, instance_offsets, transform, total_distance, current_scale);
					else
						WriteSegment(elapsed_time, 0.0f, 0.0f, instance_position, current_position, instance_offsets, transform, total_distance, current_scale); // Update current trail end
				}
			}
		}
		else if (!is_stopped && (!first_frame || trail_template->emit_start_speed < 0.0f) && StartTrail(elapsed_time, 0.0f, 0.0f, frame_speed, trail_distance))
			NewSegment(elapsed_time, 0.0f, 0.0f, instance_position, current_position, instance_offsets, transform, total_distance, current_scale); // Start trail
	}

	BoundingBox Trail::FrameMove(float elapsed_time)
	{
		const bool first_frame = reinitialise;
		float time_factor = trail_template->scale_with_animation ? animation_speed : multiplier;
		elapsed_time *= time_factor;

		if (reinitialise)
		{
			reinitialise = false;
			Reinitialise(0.0f);
		}

		const auto delta_transform = transform * last_transform.inverse();

		simd::vector3 delta_pos = simd::vector3(0.0f, 0.0f, 0.0f);
		simd::vector3 delta_scale = simd::vector3(1.0f, 1.0f, 1.0f);
		simd::quaternion delta_rot = simd::quaternion::identity();

		if (!UseLocalSpace())
		{
			if (delta_transform.determinant()[0] < 0.0f)
				Reinitialise(0.0f);
			else
			{
				simd::matrix delta_rot_matrix;
				delta_transform.decompose(delta_pos, delta_rot_matrix, delta_scale);
				delta_rot = delta_rot_matrix.rotation();
			}
		}

		size_t frame_count = 1;
		if (!animation_bones.empty())
		{
			const auto& info = Animation::System::Get().GetDetailedStorageStatic(animation_id);

			if (info.frame_count > 0)
			{
				frame_count += info.frame_count;
				elapsed_time = info.elapsed_time_total * time_factor;
			}
		}

		// IMPORTANT: DO NOT RETURN WITHOUT FREEING THIS MEMORY
		std::pair<TempPath*, size_t> path;
		if (frame_count <= MaxLocalTempData)
			path = { STACK_ALLOCATOR(TempPath, frame_count) };
		else
			path = { reinterpret_cast<TempPath*>(Memory::Allocate(Memory::Tag::Trail, sizeof(TempPath) * frame_count, alignof(TempPath))), frame_count };

		const auto current_position = UseLocalSpace() ? instance_position : transform.mulpos(instance_position);
		const float distance = ComputePath(path.first, path.second, elapsed_time, delta_pos, delta_rot, delta_scale, current_position, time_factor);

		const float frame_speed = elapsed_time > MinDT * 0.5f ? delta_pos.len() / elapsed_time : 0.0f;

		const float new_elapsed = UpdateLifetime(elapsed_time, frame_speed);
		const float total_distance = UpdateOldSegments(elapsed_time, distance);
		distance_since_start += distance;
		time_since_start += elapsed_time;
		time_since_random_value += elapsed_time;

		if (new_elapsed >= 0.0f)
		{
			float last_time = 0.0f;
			float last_distance = 0.0f;
			if (!animation_bones.empty() && animation_bones.size() >= instance_offsets.size())
				std::tie(last_distance, last_time) = EmitAnimationSegments(path.first, path.second, new_elapsed, first_frame, time_factor, frame_speed, total_distance);

			if (new_elapsed >= last_time)
			{
				remaining_stop_distance -= distance - last_distance;
				if (remaining_stop_distance < 0.0f && trail_template->stop_distance_threshold >= 0.0f)
					is_stopped = true;

				if (is_stopped)
					WriteFinalSegment(new_elapsed, elapsed_time, last_time, distance, last_distance, delta_pos, delta_rot, delta_scale, total_distance);
				else
					UpdateLastSegment(new_elapsed, last_time, first_frame, frame_speed, distance - last_distance, current_position, total_distance);
			}
		}

		UpdateAllSegments(); // After updating/emitting segments

		if (frame_count > MaxLocalTempData)
			Memory::Free(path.first);

		if (emit_delay > 0.0f)
			emit_delay -= elapsed_time;

		last_stop_time = -1.0f;
		last_transform = transform;
		return MergeDynamicBoundingBox();
	}

	void Trail::WriteFinalSegment(float new_elapsed, float elapsed_time, float last_time, float distance, float last_distance, const simd::vector3& delta_pos, const simd::quaternion& delta_rot, const simd::vector3& delta_scale, float total_distance)
	{
		if (current_segment == last_dead || is_paused)
			return;

		const auto last_scale = FlattenScale(last_transform.scale());

		const float scaled_distance = distance * new_elapsed / elapsed_time;
		const auto current_transform = DeltaTransform(elapsed_time, elapsed_time - new_elapsed, delta_pos, delta_scale, delta_rot, last_transform);
		const auto local_position = instance_position;
		const auto world_position = UseLocalSpace() ? local_position : current_transform.transform.mulpos(local_position);
		const auto current_scale = last_scale * FlattenScale(current_transform.scale);

		const float rigid_body_distance = UseLocalSpace() ? 0.0f : (current_transform.transform.mulpos(local_position) - last_emit_transform.mulpos(local_position)).len();
		const float max_length = trail_template->max_segment_length;

		if(rigid_body_distance > max_length)
			NewSegment(elapsed_time, 0.0f, 0.0f, local_position, world_position, instance_offsets, current_transform.transform, total_distance, current_scale);
		else
			WriteSegment(elapsed_time, 0.0f, 0.0f, local_position, world_position, instance_offsets, current_transform.transform, total_distance, current_scale);
	}

	bool Trail::IsComplete( ) const
	{
		return is_stopped && first_dead == current_segment;
	}

}
