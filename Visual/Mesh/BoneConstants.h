#pragma once

#include <limits>

namespace Bones
{
	enum BoneConstants
	{
		Root = -1,
		Lock = -2,
		Invalid = -3,
		Unset = -4,

		NumSpecialBones = 4 // MUST BE MAINTAINED MANUALLY
	};
}