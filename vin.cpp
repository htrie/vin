#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "vulkan-1.lib")

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define _HAS_EXCEPTIONS 0
#define VULKAN_HPP_NO_EXCEPTIONS

#include <iostream>
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
#include "font_regular.h"
#include "font_bold.h"
#include "font_italic.h"
#include "config.h"
#include "index.h"
#include "text.h"
#include "state.h"
#include "buffer.h"
#include "picker.h"
#include "switcher.h"
#include "finder.h"
#include "vulkan.h"
#include "device.h"
#include "app.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) {
	const Timer timer;
	App app(hInstance, nCmdShow);
	app.notify(std::string("init in ") + timer.us());
	app.run();
	return 0;
}

