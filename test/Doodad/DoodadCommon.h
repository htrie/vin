#pragma once
#include <array>

namespace Doodad
{
	///The width/height in tiles of a doodad layer group.
	enum { DoodadLayerGroupSize = 3 };

	using DoodadCornersEnabled = std::array< bool, ( DoodadLayerGroupSize + 1 ) * ( DoodadLayerGroupSize + 1 ) >;
}
