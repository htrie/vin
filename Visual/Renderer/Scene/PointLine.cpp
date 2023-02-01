
#include "Common/Utility/NonCopyable.h"
#include "Common/Utility/Geometric.h"
#include "PointLine.h"

namespace Renderer
{
	Vector2d MakeVector2d(simd::vector3 point); //in ScreenspaceTracer.cpp
	/*{
		return Vector2d(point.x, point.y);
	}*/

	float Sgn(float val);
	/*{
		return val > 0.0f ? 1.0f : -1.0f;
	}*/

	void PointLine::Smooth(float smooth_ratio, int smooth_iterations, float min_curvature_radius)
	{
		for (int i = 0; i < smooth_iterations; i++)
		{
			std::vector<Vector2d> smoothed_points;
			smoothed_points.resize(points.size());
			for (size_t segment_index = 0; segment_index < segments.size(); segment_index++)
			{
				auto &curr_segment = segments[segment_index];
				for (int point_number = 0; point_number < curr_segment.count; point_number++)
				{
					size_t point_index = curr_segment.offset + point_number;

					Vector2d curr_point = MakeVector2d(points[point_index]);
					smoothed_points[point_index] = curr_point;

					if (!curr_segment.is_cycle && (point_number == 0 || point_number >= curr_segment.count - 1))
						continue;

					size_t prev_index = curr_segment.offset + (point_number + curr_segment.count - 1) % curr_segment.count;
					size_t next_index = curr_segment.offset + (point_number + curr_segment.count + 1) % curr_segment.count;

					Vector2d prev_point = MakeVector2d(points[prev_index]);
					Vector2d next_point = MakeVector2d(points[next_index]);

					Vector2d delta = next_point - prev_point;
					Vector2d norm = Vector2d(-delta.y, delta.x).normalize();

					float dist = (curr_point - prev_point).dot(norm);

					float chord_len = (next_point - prev_point).length();
					float half_len = chord_len / 2.0f;

					float local_radius = std::max(min_curvature_radius, half_len);

					float max_dist = local_radius - std::sqrtf(std::max(0.0f, local_radius * local_radius - half_len * half_len));
					float total_displ = std::min(abs(dist), abs(max_dist)) * Sgn(dist) - dist;
					//float totalDispl = maxDist * sgn(dist) - dist; 
					total_displ *= smooth_ratio;

					curr_point = (prev_point + next_point) * 0.5f + norm * dist;


					//currPoint += norm * totalDispl;

					curr_point += norm * total_displ * 0.5f;
					prev_point += norm * total_displ * -0.25f;
					next_point += norm * total_displ * -0.25f;

					points[prev_index].x = prev_point.x;
					points[prev_index].y = prev_point.y;
					points[point_index].x = curr_point.x;
					points[point_index].y = curr_point.y;
					points[next_index].x = next_point.x;
					points[next_index].y = next_point.y;
					/*prevPoint -= (massCenter - newMassCenter) * 3.0f / 2.0f;
					nextPoint -= (massCenter - newMassCenter) * 3.0f / 2.0f;*/

					/*

					float curr_angle = (curr_point - prev_point).normalize().dot((next_point - curr_point).normalize());
					float new_angle = std::min(abs(curr_angle), min_angle);

					Vector2d delta = (next_point - prev_point).normalize();
					Vector2d norm = Vector2d(-delta.y, delta.x);

					Vector2d smoothed_point = (prev_point + next_point) * 0.5f + norm * (next_point - prev_point).length() * 0.5f * new_angle * 0.5f;

					smoothed_points[point_index] = curr_point * (1.0f - smooth_ratio) + smoothed_point * smooth_ratio;*/


				}
			}
			/*for(size_t point_index = 0; point_index < points.size(); point_index++)
			{
			this->points[point_index].x = smoothed_points[point_index].x;
			this->points[point_index].y = smoothed_points[point_index].y;
			}*/
		}
	}

}
