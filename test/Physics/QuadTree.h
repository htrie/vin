#pragma once

#include "Common/Utility/Numeric.h"

#include "Visual/Physics/Maths/VectorMaths.h"
#include "Visual/Renderer/Scene/PointLine.h"

namespace Physics
{
	//using Vector2f = ::Vector2f;
	using Vector2i = Physics::Vector2<int>;
	using AABB2f = Physics::AABB2f;

	namespace DistanceTools
	{
		struct Projection
		{
			float min_scale;
			float max_scale;
		};
		Vector2f GetAABBPoint(AABB2f aabb, size_t point_index)
		{
			Vector2f aabb_points[4];
			aabb_points[0] = Vector2f(aabb.box_point1.x, aabb.box_point1.y);
			aabb_points[1] = Vector2f(aabb.box_point2.x, aabb.box_point1.y);
			aabb_points[2] = Vector2f(aabb.box_point2.x, aabb.box_point2.y);
			aabb_points[3] = Vector2f(aabb.box_point1.x, aabb.box_point2.y);
			return aabb_points[point_index];
		}
		Projection GetEdgeProjection(Vector2f edge_points[2], Vector2f axis)
		{
			Projection res;
			res.min_scale = std::min(edge_points[0] * axis, edge_points[1] * axis);
			res.max_scale = std::max(edge_points[0] * axis, edge_points[1] * axis);
			return res;
		}
		Projection GetAABBProjection(AABB2f aabb, Vector2f axis)
		{
			Projection res;
			res.min_scale = std::numeric_limits<float>::max();
			res.max_scale = -std::numeric_limits<float>::max();
			for(size_t point_index = 0; point_index < 4; point_index++)
			{
				float scale = GetAABBPoint(aabb, point_index) * axis;
				res.min_scale = std::min(res.min_scale, scale);
				res.max_scale = std::max(res.max_scale, scale);
			}
			return res;
		}
		AABB2f GetChildAABB(AABB2f aabb, size_t child_number)
		{
			Vector2f mid_point = (aabb.box_point1 + aabb.box_point2) * 0.5f;
			AABB2f child_aabbs[4];
			child_aabbs[0] = AABB2f(Vector2f(aabb.box_point1.x, aabb.box_point1.y), Vector2f(mid_point.x, mid_point.y));
			child_aabbs[1] = AABB2f(Vector2f(mid_point.x, aabb.box_point1.y), Vector2f(aabb.box_point2.x, mid_point.y));
			child_aabbs[2] = AABB2f(Vector2f(mid_point.x, mid_point.y), Vector2f(aabb.box_point2.x, aabb.box_point2.y));
			child_aabbs[3] = AABB2f(Vector2f(aabb.box_point1.x, mid_point.y), Vector2f(mid_point.x, aabb.box_point2.y));
			return child_aabbs[child_number];
		}
		void GetEdgeAABBDist(Vector2f edge_points[2], AABB2f aabb, float &min_dist, float &max_dist)
		{

			Vector2f axes[11];
			size_t axes_count = 0;

			axes[axes_count++] = Vector2f(1.0f, 0.0f);
			axes[axes_count++] = Vector2f(0.0f, 1.0f);
			Vector2f dir = (edge_points[1] - edge_points[0]).GetNorm();
			axes[axes_count++] = Vector2f(-dir.y, dir.x);

			for (size_t aabb_point = 0; aabb_point < 4; aabb_point++)
			{
				for (size_t edge_point = 0; edge_point < 2; edge_point++)
				{
					axes[axes_count++] = (GetAABBPoint(aabb, aabb_point) - edge_points[edge_point]).GetNorm();
				}
			}

			max_dist = -std::numeric_limits<float>::max();
			min_dist = -std::numeric_limits<float>::max();

			for(int axis = 0; axis < axes_count; axis++)
			{
				Projection edge_proj = GetEdgeProjection(edge_points, axes[axis]);
				Projection aabb_proj = GetAABBProjection(aabb, axes[axis]);

				//float axis_min_dist = std::max(edge_proj.min_scale, aabb_proj.min_scale) - std::min(edge_proj.max_scale, aabb_proj.max_scale);
				float axis_min_dist = std::max(edge_proj.min_scale - aabb_proj.max_scale, aabb_proj.min_scale - edge_proj.max_scale);
				axis_min_dist = std::max(axis_min_dist, 0.0f);

				float axis_max_dist = std::max(
					std::max(edge_proj.min_scale, aabb_proj.min_scale) - std::min(edge_proj.max_scale, aabb_proj.min_scale),
					std::max(edge_proj.min_scale, aabb_proj.max_scale) - std::min(edge_proj.max_scale, aabb_proj.max_scale));

				/*float axis_max_dist = std::max(
					std::min(edge_proj.min_scale, edge_proj.max_scale) - aabb_proj.min_scale,
					aabb_proj.min_scale - std::max(edge_proj.min_scale, edge_proj.max_scale));*/
				axis_max_dist = std::max(axis_max_dist, 0.0f);

				min_dist = std::max(min_dist, axis_min_dist);
				max_dist = std::max(max_dist, axis_max_dist);
			}
		}
	}
	struct QuadTree
	{
		struct Node
		{
			bool is_leaf;
			union
			{
				struct
				{
					size_t edges_start;
					size_t edges_count;
				};
				struct
				{
					size_t children_indices[4];
				};
			};
		};
		struct Point
		{
			size_t next_index;
			size_t prev_index;
			float longitude;
			Vector2f pos;
		};
		struct Edge
		{
			size_t point_index;
		};
		std::vector<Node> nodes;
		std::vector<Edge> node_edges;
		std::vector<Point> points;
		AABB2f aabb;

		void ProcessNode(Point *points, size_t node_index, Edge *active_edges, size_t active_edges_count, AABB2f curr_aabb, size_t depth)
		{
			std::vector<float> edge_min_dists;
			std::vector<float> edge_max_dists;
			edge_min_dists.resize(active_edges_count);
			edge_max_dists.resize(active_edges_count);

			std::vector<Edge> curr_child_edges;
			for(size_t child_number = 0; child_number < 4; child_number++)
			{
				AABB2f child_aabb = DistanceTools::GetChildAABB(curr_aabb, child_number);
				float threshold_dist = 1e7f;
				for(size_t edge_index = 0; edge_index < active_edges_count; edge_index++)
				{
					Vector2f edge_points[2];
					size_t point_index0 = active_edges[edge_index].point_index;
					size_t point_index1 = points[point_index0].next_index;
					if (point_index1 == size_t(-1))
						continue;
					edge_points[0] = points[point_index0].pos;
					edge_points[1] = points[point_index1].pos;

					DistanceTools::GetEdgeAABBDist(edge_points, child_aabb, edge_min_dists[edge_index], edge_max_dists[edge_index]);
					threshold_dist = std::min(threshold_dist, edge_max_dists[edge_index]);
				}
				curr_child_edges.clear();
				for(size_t edge_index = 0; edge_index < active_edges_count; edge_index++)
				{
					if(edge_min_dists[edge_index] <= threshold_dist)
						curr_child_edges.push_back(active_edges[edge_index]);
				}
				if(curr_child_edges.size() > 0)
				{
					if(depth > 5 || curr_child_edges.size() < 10)
					{
						Node newbie;
						newbie.is_leaf = true;
						newbie.edges_start = node_edges.size();
						newbie.edges_count = curr_child_edges.size();
						for(auto child_edge : curr_child_edges)
							node_edges.push_back(child_edge);

						nodes[node_index].children_indices[child_number] = nodes.size();
						nodes.push_back(newbie);
					}else
					{
						Node newbie;
						newbie.is_leaf = false;

						size_t child_node_index = nodes.size();
						nodes[node_index].children_indices[child_number] = child_node_index;
						nodes.push_back(newbie);
						ProcessNode(points, child_node_index, curr_child_edges.data(), curr_child_edges.size(), child_aabb, depth + 1);
					}
				}else
				{
					nodes[node_index].children_indices[child_number] = size_t(-1);
				}
			}
		}

		void LoadPointLine(const Renderer::PointLine &point_line, const AABB2f scene_aabb)
		{
			nodes.clear();
			this->aabb = scene_aabb;
			/*this->aabb.box_point1 = Vector2f(1e7f, 1e7f);
			this->aabb.box_point2 = Vector2f(-1e7f, -1e7f);*/

			this->points.clear();
			for (size_t point_index = 0; point_index < point_line.points.size(); point_index++)
			{
				Point new_point;
				new_point.pos = Vector2f(
					point_line.points[point_index].x,
					point_line.points[point_index].y);
				new_point.next_index = size_t(-1);
				new_point.prev_index = size_t(-1);
				this->points.push_back(new_point);
			}

			std::vector<Edge> active_edges;
			for(size_t segment_index = 0; segment_index < point_line.segments.size(); segment_index++)
			{
				auto &curr_segment = point_line.segments[segment_index];
				size_t segment_offset = curr_segment.offset;
				size_t count = curr_segment.is_cycle ? curr_segment.count : curr_segment.count - 1;
				for(size_t point_index = 0; point_index < count; point_index++)
				{
					Vector2f point = Vector2f(
						point_line.points[segment_offset + point_index].x,
						point_line.points[segment_offset + point_index].y);
					/*this->aabb.box_point1.x = std::min(this->aabb.box_point1.x, point.x);
					this->aabb.box_point1.y = std::min(this->aabb.box_point1.y, point.y);
					this->aabb.box_point2.x = std::max(this->aabb.box_point2.x, point.x);
					this->aabb.box_point2.y = std::max(this->aabb.box_point2.y, point.y);*/

					size_t curr_index = segment_offset + point_index;
					size_t next_index = segment_offset + (point_index + 1) % curr_segment.count;

					points[curr_index].next_index = next_index;
					points[next_index].prev_index = curr_index;

					Edge newbie;
					newbie.point_index = curr_index;
					active_edges.push_back(newbie);
				}
			}

			Node newbie;
			newbie.is_leaf = false;
			nodes.push_back(newbie);
			ProcessNode(points.data(), 0, active_edges.data(), active_edges.size(), this->aabb, 0);
		}

		struct PointPhase
		{
			float dist;
			float longitude;
		};
		void UpdateEdgeMinDist(Vector2f point, const Edge &edge, PointPhase &curr_phase)
		{
			size_t index0 = edge.point_index;
			size_t index1 = points[index0].next_index;
			assert(index0 != size_t(-1) && index1 != size_t(-1));
			Vector2f edge_points[2];
			edge_points[0] = points[index0].pos;
			edge_points[1] = points[index1].pos;

			auto UpdatePhase = [](PointPhase &curr_phase, const PointPhase &new_phase)
			{
				curr_phase = abs(curr_phase.dist) < abs(new_phase.dist) ? curr_phase : new_phase;
			};

			float ratio = ((point - edge_points[0]) * (edge_points[1] - edge_points[0])) / ((edge_points[1] - edge_points[0]).SquareLen() + 1e-5f);
			if (ratio > 0.0f && ratio < 1.0f)
			{
				Vector2f dir = (edge_points[1] - edge_points[0]).GetNorm();
				Vector2f perp = Vector2f(-dir.y, dir.x);

				PointPhase new_phase;
				new_phase.dist = (point - edge_points[0]) * perp;
				new_phase.longitude = Lerp(points[index0].longitude, points[index1].longitude, ratio);
				UpdatePhase(curr_phase, new_phase);
			}
			else
			{
				size_t curr_index = (ratio <= 0.0f) ? index0 : index1;
				size_t prev_index = points[curr_index].prev_index;
				size_t next_index = points[curr_index].next_index;
				
				if (prev_index == size_t(-1) || next_index == size_t(-1))
					return;


				PointPhase new_phase;
				new_phase.dist = (point - points[curr_index].pos).Len();
				new_phase.longitude = points[curr_index].longitude;

				Vector2f delta0 = points[curr_index].pos - points[prev_index].pos;
				Vector2f delta1 = points[next_index].pos - points[curr_index].pos;

				Vector2f delta_center = point - points[curr_index].pos;

				float sign = 0.0f;
				if ((delta0 ^ delta1) > 0.0f)
				{
					sign = ((delta0 ^ delta_center) > 0.0f && (delta1 ^ delta_center) > 0.0f) ? 1.0f : -1.0f;
				}
				else
				{
					sign = ((delta0 ^ delta_center) > 0.0f || (delta1 ^ delta_center) > 0.0f) ? 1.0f : -1.0f;
				}
				new_phase.dist *= sign;
				UpdatePhase(curr_phase, new_phase);
			}
		}
		void GetPhaseInternal(Vector2f point, size_t node_index, AABB2f curr_aabb, PointPhase &curr_phase)
		{
			auto &curr_node = nodes[node_index];
			if (!curr_node.is_leaf)
			{
				for (size_t child_number = 0; child_number < 4; child_number++)
				{
					AABB2f child_aabb = DistanceTools::GetChildAABB(curr_aabb, child_number);
					size_t child_index = curr_node.children_indices[child_number];
					if (child_index == size_t(-1) || !child_aabb.Includes(point))
						continue;
					GetPhaseInternal(point, child_index, child_aabb, curr_phase);
				}
			}
			else
			{
				for (size_t edge_number = 0; edge_number < curr_node.edges_count; edge_number++)
				{
					size_t edge_index = curr_node.edges_start + edge_number;
					auto &curr_edge = node_edges[edge_index];
					UpdateEdgeMinDist(point, curr_edge, curr_phase);
				}
			}
		}

		PointPhase GetPointPhase(Vector2f point)
		{
			PointPhase phase;
			phase.dist = -1e7f;
			phase.longitude = 0.0f;
			GetPhaseInternal(point, 0, this->aabb, phase);
			return phase;
		}
	};
}