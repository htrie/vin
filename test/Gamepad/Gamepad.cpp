#include "Gamepad.h"

#if defined( PS4 )
#	include "Visual/Gamepad/Gamepad_PS4.cpp"
#elif defined(_XBOX_ONE)
#	include "Visual/Gamepad/Gamepad_XBox.cpp"
#elif defined(__APPLE__)
#	include "Visual/Gamepad/Gamepad_Apple.cpp"
#elif defined(ANDROID)
#	include "Visual/Gamepad/Gamepad_Android.cpp"
#elif defined(WIN32)
#	include "Visual/Gamepad/Gamepad_Win32.cpp"
#else
#	include "Visual/Gamepad/Gamepad_None.cpp"
#endif