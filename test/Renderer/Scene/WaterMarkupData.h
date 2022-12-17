#pragma once

#include "Common/Utility/NonCopyable.h"
#include "PointLine.h"

namespace Renderer
{
	namespace Scene
	{
		struct OceanDataStorage
		{
			PointLine shallow_shoreline;
			PointLine rocky_shoreline;
			PointLine deep_shoreline;
		};

		struct OceanData
		{
			const PointLine *shallow_shoreline = nullptr;
			const PointLine *rocky_shoreline = nullptr;
			const PointLine *deep_shoreline = nullptr;
			float shoreline_offset = 0.0f;
			size_t texels_per_tile_count = 1;
		};

		struct WaterFlowConstraint
		{
			simd::vector2 world_pos;
			float radius = 1.0f;
			simd::vector2 flow_intensity;
		};

		struct WaterSourceConstraint
		{
			simd::vector2 world_pos;
			float radius = 0.0f;
			float source = 0.0f;
		};

		struct WaterBlockingConstraint
		{
			simd::vector2 world_pos;
			float radius = 0.0f;
			float blocking = 0.0f;
		};

		struct RiverDataStorage
		{
			PointLine river_bank_line;
			std::vector<WaterFlowConstraint> flow_constraints;
			std::vector<WaterSourceConstraint> source_constraints;
			std::vector<WaterBlockingConstraint> blocking_constraints;
		};

		struct RiverData
		{
			const PointLine *river_bank_line = nullptr;

			const WaterFlowConstraint *flow_constraints = nullptr;
			size_t flow_constraints_count = 0;

			const WaterSourceConstraint *source_constraints = nullptr;
			size_t source_constraints_count = 0;

			const WaterBlockingConstraint *blocking_constraints = nullptr;
			size_t blocking_constraints_count = 0;
			bool wait_for_initialization = false;

			size_t texels_per_tile_count = 4;

			float initial_turbulence = 0.0f;
		};
	}
}
