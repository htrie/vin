#pragma once

#include "Common/Utility/Numeric.h"
#include "Common/FileReader/TRLReader.h"
#include "Visual/Material/Material.h"
#include "Visual/Trails/TrailSystem.h"

namespace Trails
{
	namespace UvMode
	{
		enum Value
		{
			Fixed,
			Distance,
			Time,
			None,
			NumUvModes
		};
	}

	extern const wchar_t* UVModeNames[UvMode::NumUvModes];

	///How long between each emit of a segment
	const float SegmentPeriod = 0.001f;

	struct TrailsTemplate
	{
	private:
		class Reader;
	public:
		enum 
		{ 
			NumPointsInSegmentLifetime = 8,
			NumPointsInEmitterLifetime = 7,
		};

		MaterialHandle material;

		bool has_normals = false;
		bool legacy_light = false;
		bool double_trail = false;
		bool spline_u = true;
		bool spline_v = true;
		bool move_standing_uvs = false;

		unsigned u_tessellation = 0;
		unsigned v_tessellation = 0;

		///Duration of a single segment of the trail
		float segment_duration;
		float segment_duration_variance;
		float trail_length;
		float trail_length_variance;

		float spline_strength = 1.0f;

		// Value between zero and one to lerp between duration driven (zero) and length driven (one)
		float duration_length_ratio;

		float texture_stretch;

		//Distance threshold to start emitting
		float start_distance_threshold;
		float stop_distance_threshold;

		//Distance between two segments
		float max_segment_length;

		//Scales the update frequency of the trail according to this value when the trail is stopped.
		float update_speed_multiplier_on_death;

		float emit_min_speed = -1.0f;
		float emit_max_speed = -1.0f;
		float emit_start_speed = -1.0f;

		typedef simd::GenericCurve<NumPointsInSegmentLifetime> SegmentCurve;
		typedef simd::GenericCurve<NumPointsInEmitterLifetime> EmitterCurve;

		SegmentCurve color_r;
		SegmentCurve color_g;
		SegmentCurve color_b;
		SegmentCurve color_a;

		SegmentCurve scale;
		SegmentCurve velocity_model_x;
		SegmentCurve velocity_model_y;
		SegmentCurve velocity_model_z;
		SegmentCurve velocity_world_x;
		SegmentCurve velocity_world_y;
		SegmentCurve velocity_world_z;

		float randomness_frequency;

		//These properties change over the lifetime of an emitter.
		//They are all disabled when the trail lasts forever
		EmitterCurve emit_color_r;
		EmitterCurve emit_color_g;
		EmitterCurve emit_color_b;
		EmitterCurve emit_color_a;
		EmitterCurve emit_scale;

		UvMode::Value uv_mode;

		float emit_width = 20.0f;

		bool emit_at_tip;
		bool reverse_bone_group;
		bool locked_to_parent;
		bool enabled; ///< Not loaded from file. Turned on and off by UI temporarily
		bool scale_with_animation;
		bool lock_scale;

		// Scales the segment duration according to the parent's anim speed
		bool scale_segment_duration;

		float lifetime;
		bool scale_lifetime;

		// Forces the trail to face to the camera (behave as if there are no bone groups)
		bool camera_facing;

		Memory::DebugStringA<128> ui_name;
		
		///@throws Resource::Exception on failure
		Resource::UniquePtr<FileReader::TRLReader::Trail> GetReader(Resource::Allocator& allocator );
		void Write(std::wostream& stream ) const;

		void SetToDefaults();

		///Gets the maximum number of segments that this emitter could generate
		unsigned GetMaxSegments() const;
		VertexType GetVertexType(bool force_camera_facing = false) const;

		void Warmup() const;

		unsigned GetTessellationU() const;
		unsigned GetTessellationV() const;

	private:


	};
}
