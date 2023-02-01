#pragma once

#include "Common/Utility/NonCopyable.h"

namespace Renderer
{
	struct PointLine
	{
		std::vector<simd::vector3> points;
		struct Segment
		{
			int offset;
			int count;
			bool is_cycle;
		};
		std::vector<Segment> segments;

		void Smooth(float smooth_ratio, int smooth_iterations, float min_curvature_radius);
	};
}
