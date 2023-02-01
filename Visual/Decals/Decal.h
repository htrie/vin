#pragma once

#include "Common/Resource/ChildHandle.h"
#include "Visual/Material/MaterialAtlas.h"
#include "Common/Utility/Geometric.h"

namespace Decals
{
	struct Decal
	{
		Materials::AtlasGroupHandle texture;
		Vector2d position;
		float size = 0.0f;
		float angle = 0.0f;
		float fade_in_time = 0.0;
		bool flipped = false;
	};
}
