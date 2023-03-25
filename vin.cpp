#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "vulkan-1.lib")
#pragma comment(lib, "winhttp.lib")

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
#include <winhttp.h>

#include "resource.h"
#include "math.h"
#include "vulkan.h"
#include "text.h"
#include "state.h"
#include "buffer.h"
#include "font.h"
#include "application.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) {
	Application application(hInstance, nCmdShow);
	application.run();
	return 0;
}

