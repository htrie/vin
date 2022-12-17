#ifndef PRECOMPILED_VISUAL_RENDERER_H
#define PRECOMPILED_VISUAL_RENDERER_H

#include <fstream>
#include <functional>
#include <ostream>
#include <memory>
#include <cmath>
#include <array>
#include <iostream>
#include <vector>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <sstream>
#include <limits>
#include <algorithm>
#include <tuple>
#include <memory>

#ifdef WIN32
#	define NOMINMAX
#	ifndef _XBOX_ONE
#		define WIN32_LEAN_AND_MEAN
#		define _WINSOCK_DEPRECATED_NO_WARNINGS
#		define _CRT_SECURE_NO_WARNINGS
#		include <shlobj.h>
#		include <CommDlg.h>
#	endif // _XBOX_ONE
#	include <Windows.h>
	// due to line 1146 in Microsoft SDKs\Windows\v7.1A\Include\shlobj.h
#	pragma warning (disable: 4091)
#endif

#include "Common/Utility/Memory/Memory.h"
#include "Common/Simd/Simd.h"

#endif //PRECOMPILED_VISUAL_RENDERER_H