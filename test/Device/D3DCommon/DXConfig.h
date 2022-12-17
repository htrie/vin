#pragma once

#include "Deployment/configs/Configuration.h"

#if defined(_XBOX_ONE)
	#define GGG_XBOX_USE_DX12
#else
#if defined(XBOX_REALM) || (defined(_WIN64) && !defined(MOBILE))
	#define GGG_PC_ENABLE_DX12
#endif
#endif
