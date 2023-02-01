#pragma once

namespace Trails
{
	struct SegmentVertex
	{
		///Relative offset from the position of the segment
		simd::vector3 offset;
		///< V coord of texture
		float v;
		float width;
	};

	//Do not allocate this structure in the usual way as it has follow on data of variable size
	struct Segment
	{
		simd::vector3 position;
		simd::vector4 emit_colour;
		simd::vector4 colour;
		simd::vector4 variance;
		float emitter_variance;
		float emit_scale;
		float scale;
		float phase;
		float gradient;
		float elapsed_time;
		float distance_from_tip;
		float time_since_start;
		float distance_since_start;

		bool restart;

		//Positions are stored after the end of the structure
		SegmentVertex* GetVertices( ) { return reinterpret_cast<SegmentVertex*>(this + 1); }
		const SegmentVertex* GetVertices( ) const { return reinterpret_cast<const SegmentVertex*>(this + 1); }
	};
}
