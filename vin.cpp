#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "vulkan-1.lib")

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define _HAS_EXCEPTIONS 0
#define VULKAN_HPP_NO_EXCEPTIONS

#include <iostream>
#include <filesystem>
#include <sstream>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <dwmapi.h>
#include <vulkan/vulkan.hpp>

#include "resource.h"
#include "memory.h"
#include "windows.h"
#include "math.h"
#include "font.h"
#include "config.h"
#include "text.h"
#include "state.h"
#include "buffer.h"
#include "switcher.h"
#include "vulkan.h"
#include "device.h"
#include "application.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) {
	Application application(hInstance, nCmdShow);
	application.run();
	return 0;
}

