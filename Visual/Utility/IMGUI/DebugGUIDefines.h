#pragma once

#include "Deployment/configs/Configuration.h"

#if !defined(ANDROID) && !defined(PRODUCTION_CONFIGURATION) && (GGG_CHEATS)
#	define DEBUG_GUI_ENABLED 1
#else
#	define DEBUG_GUI_ENABLED 0
#endif
