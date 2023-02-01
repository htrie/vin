#pragma once

#include <tuple>

#include "Common/Geometry/Bounding.h"

#include "TrailsEffect.h"

#include "Visual/Trails/TrailSystem.h"
#include "Visual/Trails/TrailSegment.h"
#include "Visual/Animation/AnimationSystem.h"

namespace Trails
{
	struct SegmentVertex;

	struct Trail
	{
	private:
		static constexpr unsigned NumDeadSegments = 3;

	public:
		typedef TrailsTemplate::EmitterCurve::Track EmitterTrack;
		typedef TrailsTemplate::SegmentCurve::NormalizedTrack SegmentTrack;

		Trail( TrailsTemplateHandle trail_template, const simd::vector3& position, const std::vector< SegmentVertex >& offsets, const simd::matrix& transform, float delay, float emitter_duration);
		~Trail();

		///Must be called if you change the number of segments per second, the duration of the segments, or the number of elements in the position
		void ReinitialiseTrail();
		void StopEmitting(float stop_time = -1.0f);
		void OnOrphaned();
		bool IsStopped() const { return is_stopped; }
		
		void SetEmitterTime(float emitter_time) { this->emitter_time = emitter_time; }
		void SetMultiplier(float _multiplier) { multiplier = _multiplier; }
		void SetAnimationStorage(const Animation::DetailedStorageID& id, const Memory::Span<const unsigned>& bone_indices);

		void SetAnimationSpeedMultiplier(float animspeed_multiplier);

		void SetTransform(const simd::matrix& _transform) { transform = _transform; }

		BoundingBox FrameMove(float elapsed_time);

		bool IsComplete( ) const;

		void Pause();
		void Resume();

		unsigned NumVerticesPerSegment() const;
		unsigned GetMergeId() const;
		VertexType GetVertexType() const;

		const simd::matrix& GetTransform() const { return transform; }

		class Piece
		{
			friend struct Trail;

		private:
			const unsigned first_dead;
			const unsigned last_dead;
			const unsigned current_segment;
			const Trail* const trail;

			Piece(unsigned last_segment, unsigned first_dead, unsigned last_dead, const Trail* trail)
				: current_segment(last_segment)
				, trail(trail)
				, first_dead(first_dead)
				, last_dead(last_dead)
			{ }

		public:
			const Trail* GetTrail() const { return trail; }

			unsigned NumSplineSegments() const
			{
				if (first_dead == current_segment)
					return 0;

				return unsigned(NumLiveSegments() + NumDeadSegments);
			}

			unsigned NumLiveSegments() const
			{
				return (current_segment + trail->num_segments - first_dead) % trail->num_segments;
			}

			unsigned NumTotalSegments() const
			{
				return ((current_segment + trail->num_segments - last_dead) % trail->num_segments) + 1;
			}

			template<typename F>
			void ForEachSegment(F func) const
			{
				const auto segment_size = trail->GetSegmentSize();
				const auto num_segs = NumTotalSegments();
				const auto first = last_dead;
				for (unsigned a = 0; a < num_segs; a++)
				{
					const auto i = (a + first) % trail->num_segments;
					func(*reinterpret_cast<const Segment*>(&trail->segments[i * segment_size]), i, a);
				}
			}

			template< typename F >
			void ForEachSplineSegment(F func) const
			{
				const auto segment_size = trail->GetSegmentSize();
				const auto num_segs = NumLiveSegments();
				const auto first = first_dead + 1;

				if (num_segs == 0)
					return;

				const auto num_dead = ((first_dead + trail->num_segments - last_dead) % trail->num_segments) + 1;
				assert(num_dead <= NumDeadSegments);
				const auto rep = NumDeadSegments - num_dead;
				for (unsigned a = 0; a < rep; a++)
					func(*reinterpret_cast<const Segment*>(&trail->segments[last_dead * segment_size]), false, a);
				
				for (unsigned a = 0; a < num_dead; a++)
				{
					const auto i = (last_dead + a) % trail->num_segments;
					func(*reinterpret_cast<const Segment*>(&trail->segments[i * segment_size]), false, a + rep);
				}

				for (unsigned a = 0; a < num_segs; a++)
				{
					const auto i = (first + a) % trail->num_segments;
					func(*reinterpret_cast<const Segment*>(&trail->segments[i * segment_size]), i == current_segment, NumDeadSegments + a);
				}
			}

			template<typename F>
			void ForEachSegmentWithDistance(F func) const
			{
				const auto segment_size = trail->GetSegmentSize();
				const auto num_segs = NumTotalSegments();

				float* segment_distance = (float*)alloca(sizeof(float) * num_segs);

				simd::vector3 last_segment_pos;
				float last_phase = FLT_MAX;
				double total_distance = 0.0;
				double segment_total_distance = 0.0;

				for (unsigned a = 0; a < num_segs; a++)
					segment_distance[a] = 0.0f;

				ForEachSegment([&](const Segment& segment, unsigned index, unsigned ordered_index)
				{
					assert(ordered_index < num_segs);

					if (ordered_index > 0)
					{
						const auto l = (segment.position - last_segment_pos).len();
						const auto f = segment.phase > 1.0f ? 0.0f : last_phase > 1.0f ? 1.0f - std::clamp((1.0f - last_phase) / (segment.phase - last_phase), 0.0f, 1.0f) : 1.0f;

						segment_distance[ordered_index] = l;
						segment_total_distance += l;
						total_distance += l * f;
					}

					last_segment_pos = segment.position;
					last_phase = segment.phase;
				});

				for (unsigned a = num_segs; a > 1; a--)
					segment_distance[a - 2] += segment_distance[a - 1];

				const auto first = last_dead;
				for (unsigned a = 0; a < num_segs; a++)
				{
					const auto i = (a + first) % trail->num_segments;
					const auto distance = a + 1 == num_segs ? 0.0f : segment_distance[a + 1];
					func(*reinterpret_cast<const Segment*>(&trail->segments[i * segment_size]), i, a, distance, float(total_distance), float(segment_total_distance));
				}
			}
		};

		template< typename F >
		void ForEachPiece(F func) const
		{
			const auto segment_size = GetSegmentSize();
			const auto num_segs = (current_segment + num_segments - first_dead) % num_segments;
			if (num_segs == 0)
				return;

			unsigned cur_first_dead = first_dead;
			unsigned cur_last_dead = last_dead;
			unsigned next_first_dead = first_dead;

			bool is_first = true;

			for (unsigned a = 1; a < num_segs; a++)
			{
				const auto i = (first_dead + a) % num_segments;
				const auto& segment = *reinterpret_cast<const Segment*>(&segments[i * segment_size]);
				
				if (segment.restart)
				{
					const auto prev = i == 0 ? num_segments - 1 : i - 1;
					if (prev != next_first_dead)
					{
						cur_last_dead = FindLastDead(next_first_dead, cur_first_dead, cur_last_dead, num_segments);
						cur_first_dead = next_first_dead;

						func(Piece(prev, cur_first_dead, cur_last_dead, this), is_first, i == current_segment);
						is_first = false;
					}

					cur_first_dead = cur_last_dead = next_first_dead = i;
				}
				else if (segment.phase > 1.0f)
					next_first_dead = i;
			}

			cur_last_dead = FindLastDead(next_first_dead, cur_first_dead, cur_last_dead, num_segments);
			cur_first_dead = next_first_dead;

			if (current_segment != cur_last_dead)
				func(Piece(current_segment, cur_first_dead, cur_last_dead, this), is_first, true);
		}

		const TrailsTemplateHandle trail_template;

	private:
		void Reinitialise(float delay);

		struct TempPath
		{
			simd::matrix current_transform; // transform matrix to transform into parent space
			simd::vector3 pos; // center position of the trail in local space
			simd::vector3 world_pos; // center position of the trail in world space
			float distance; // distance in parent space
			float scale; // scale of the parent
		};

		static constexpr auto MaxLocalTempData = 1024 / sizeof(TempPath);

		static unsigned FindLastDead(unsigned index, unsigned first_dead, unsigned last_dead, unsigned num_segments);

		unsigned GetLastSegment() const;
		Segment& GetSegment(unsigned index);

		void NewSegment(float elapsed_time, float time_offset, float distance_offset, const simd::vector3& local_position, const simd::vector3& global_position, const Memory::Span<const SegmentVertex>& bone_data, const simd::matrix& current_transform, float total_distance, float current_scale);
		bool WriteSegmentData(float elapsed_time, float time_offset, float distance_offset, const simd::vector3& local_position, const simd::vector3& global_position, const Memory::Span<const SegmentVertex>& bone_data, const simd::matrix& current_transform, float total_distance, float current_scale);
		void WriteSegment(float elapsed_time, float time_offset, float distance_offset, const simd::vector3& local_position, const simd::vector3& global_position, const Memory::Span<const SegmentVertex>& bone_data, const simd::matrix& current_transform, float total_distance, float current_scale);
		void FillSegmentVertices(Segment& segment) const;
		void RollNewVariance();
		void InitSegments();
		bool UseLocalSpace() const;
		bool StartTrail(float elapsed_time, float time_offset, float distance_offset, float speed, float distance);
		bool UpdatePause(float speed, float time_offset, float distance_offset);

		void WriteFinalSegment(float new_elapsed, float elapsed_time, float last_time, float distance, float last_distance, const simd::vector3& delta_pos, const simd::quaternion& delta_rot, const simd::vector3& delta_scale, float total_distance);
		bool UpdateVariance(float time_offset, float elapsed_time);
		float UpdateOldSegments(float elapsed_time, float new_distance);
		void UpdateAllSegments();
		void UpdateLastSegment(float elapsed_time, float last_time, bool first_frame, float speed, float trail_distance, const simd::vector3& current_position, float total_distance);
		float UpdateLifetime(float elapsed_time, float speed);
		float ComputePath(TempPath* temp_data, size_t frame_count, float elapsed_time, const simd::vector3& delta_pos, const simd::quaternion& delta_rot, const simd::vector3& delta_scale, const simd::vector3& current_position, float time_factor);
		float ComputeSegmentPhase(float last_phase, float elapsed_time, float distance, size_t index, float variance) const;
		std::tuple<float, float> EmitAnimationSegments(TempPath* temp_data, size_t frame_count, float elapsed_time, bool first_frame, float time_factor, float frame_speed, float total_distance);

		BoundingBox MergeDynamicBoundingBox();

		///The size of each segment element in the segments array
		size_t GetSegmentSize( ) const;

		unsigned num_segments;
		unsigned current_segment;
		unsigned first_dead;
		unsigned last_dead;

		unsigned num_vertexes_per_segment;

		///This is a ring buffer of trail segments
		Memory::Vector<uint8_t, Memory::Tag::Trail> segments;

		Animation::DetailedStorageID animation_id;
		Memory::SmallVector<unsigned, 16, Memory::Tag::Trail> animation_bones;

		simd::vector3 old_position;

		simd::matrix transform;
		simd::matrix last_transform;
		simd::matrix last_emit_transform;

		///This data is a reference directly in to the parent TrailsEffectInstance beacuse it is the same for all trails
		const simd::vector3& instance_position;
		const std::vector< SegmentVertex >& instance_offsets;

		BoundingBox bounding_box;

		EmitterTrack emit_color_r;
		EmitterTrack emit_color_g;
		EmitterTrack emit_color_b;
		EmitterTrack emit_color_a;
		EmitterTrack emit_scale;

		SegmentTrack color_r;
		SegmentTrack color_g;
		SegmentTrack color_b;
		SegmentTrack color_a;
		SegmentTrack velocity_model_x;
		SegmentTrack velocity_model_y;
		SegmentTrack velocity_model_z;
		SegmentTrack velocity_world_x;
		SegmentTrack velocity_world_y;
		SegmentTrack velocity_world_z;
		SegmentTrack segment_scale;

		float multiplier = 1.0f;
		float animation_speed = 1.0f;
		float remaining_lifetime = -1.0f;
		float remaining_start_distance = 0.0f;
		float remaining_stop_distance = 0.0f;
		float time_since_random_value = 0.0f;
		float last_stop_time = -1.0f;
		float random_emitter_seed = 0.0f;

		simd::vector4 random_value;
		simd::vector4 last_random_value;

		float time_since_start;
		float distance_since_start;

		const float emitter_duration = 0.0f;
		float emitter_time = 0.0f;
		float emit_delay = 0.0f;

		bool camera_facing = false;
		bool is_stopped;
		bool is_paused = false;
		bool is_manually_paused = false;
		bool was_paused = false;
		bool has_new_segment = false;

		bool reinitialise = false;
	};
}
