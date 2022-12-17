#pragma once
#include "Common/Loaders/CEBuffVisuals.h"

namespace Buffs
{
	// Any buff visual in the BuffVisuals table that does not have a Preload Group and does not Always Preload 
	// should ideally call one of these functions to preload itself before it is used.
	namespace Internal
	{
		void InitialiseBuffPreloading();
	}
}