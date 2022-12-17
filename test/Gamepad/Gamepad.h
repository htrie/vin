#pragma once

#include "Common/Console/ConsoleCommon.h"
#include "Common/Utility/Geometric.h"
#include "Common/Loaders/CEGamepadButtonEnum.h"

#if defined( PS4 )
#	include "Visual/Gamepad/Gamepad_PS4.h"
#elif defined(_XBOX_ONE)
#	include "Visual/Gamepad/Gamepad_XBox.h"
#elif defined(__APPLE__)
#	include "Visual/Gamepad/Gamepad_Apple.h"
#elif defined(ANDROID)
#	include "Visual/Gamepad/Gamepad_Android.h"
#elif defined(WIN32)
#	include "Visual/Gamepad/Gamepad_Win32.h"
#else
#	include "Visual/Gamepad/Gamepad_None.h"
#endif