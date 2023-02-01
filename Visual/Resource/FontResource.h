#pragma once

#ifdef USE_FREETYPE
#	include "FontResourceFreeType.h"
typedef FontResourceFreeType FontResource;
#else
#	include "FontResourceWindows.h"
typedef FontResourceWindows FontResource;
#endif
