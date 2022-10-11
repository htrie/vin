#define NOMINMAX
#define VC_EXTRALEAN
#define WIN32_LEAN_AND_MEAN
#define _HAS_EXCEPTIONS 0
#define VULKAN_HPP_NO_EXCEPTIONS

#include <math.h>
#include <stdlib.h>
#include <string>
#include <chrono>
#include <fstream>
#include <thread>
#include <vulkan/vulkan.hpp>
#include <dwmapi.h>
#include <Windows.h>

#include "resource.h"
#include "util.h"
#include "win.h"
#include "matrix.h"
#include "font.h"
#include "config.h"
#include "text.h"
#include "vk.h"

class App {
	Device device;
	Buffer buffer;
	Timer timer;

	float cull_time = 0.0f;
	float redraw_time = 0.0f;
	float process_time = 0.0f;

	bool minimized = false;
	bool dirty = true;

	void set_minimized(bool b) { minimized = b; }
	void set_dirty(bool b) { dirty = b; }

	std::string status() {
		const auto process_duration = std::string("proc ") + std::to_string((unsigned)(process_time * 1000.0f)) + "us";
		const auto cull_duration = std::string("cull ") + std::to_string((unsigned)(cull_time * 1000.0f)) + "us";
		const auto redraw_duration = std::string("draw ") + std::to_string((unsigned)(redraw_time * 1000.0f)) + "us";
		return buffer.status() + " (" + process_duration + ", " + cull_duration + ", " + redraw_duration + ")";
	}

	void resize(unsigned w, unsigned h) {
		if (!minimized) {
			device.resize(w, h);
		}
	}

	void redraw() {
		if (!minimized && dirty) {
			dirty = false;
			const auto viewport = device.viewport();
			const auto start = timer.now();
			Layout layout(viewport.w, viewport.h);
			const auto text = layout.cull(buffer, status());
			cull_time = timer.duration(start);
			{
				const auto start = timer.now();
				device.redraw(text);
				redraw_time = timer.duration(start);
			}
		}
		else {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}

	void process(unsigned key, bool released) {
		const auto start = timer.now();
		const auto viewport = device.viewport();
		if (buffer.process(key, released, viewport.w, viewport.h))
			PostQuitMessage(0);
		process_time = timer.duration(start);
	}

	static LRESULT CALLBACK proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
		switch (uMsg) {
		case WM_CREATE: {
			LPCREATESTRUCT create_struct = reinterpret_cast<LPCREATESTRUCT>(lParam);
			SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create_struct->lpCreateParams));
			break;
		}
		case WM_MOVE:
		case WM_SETFOCUS: {
			if (auto* app = reinterpret_cast<App*>(GetWindowLongPtr(hWnd, GWLP_USERDATA)))
				app->set_dirty(true);
			break;
		}
		case WM_DESTROY:
		case WM_CLOSE: {
			PostQuitMessage(0);
			break;
		}
		case WM_GETMINMAXINFO: {
			// Window client area size must be at least 1 pixel high, to prevent crash.
			((MINMAXINFO*)lParam)->ptMinTrackSize = { GetSystemMetrics(SM_CXMINTRACK), GetSystemMetrics(SM_CYMINTRACK) + 1 };
			break;
		}
		case WM_PAINT: {
			if (auto* app = reinterpret_cast<App*>(GetWindowLongPtr(hWnd, GWLP_USERDATA)))
				app->redraw();
			break;
		}
		case WM_SIZE: {
			if (auto* app = reinterpret_cast<App*>(GetWindowLongPtr(hWnd, GWLP_USERDATA))) {
				app->set_minimized(wParam == SIZE_MINIMIZED);
				const unsigned width = lParam & 0xffff;
				const unsigned height = (lParam & 0xffff0000) >> 16;
				app->resize(width, height);
				app->set_dirty(true);
			}
			break;
		}
		case WM_KEYUP: {
			if (auto* app = reinterpret_cast<App*>(GetWindowLongPtr(hWnd, GWLP_USERDATA))) {
				if (wParam == VK_SPACE) {
					app->process(' ', true);
					app->set_dirty(true);
				}
			}
			break;
		}
		case WM_CHAR: {
			if (auto* app = reinterpret_cast<App*>(GetWindowLongPtr(hWnd, GWLP_USERDATA))) {
				app->process((unsigned)wParam, false);
				app->set_dirty(true);
			}
			break;
		}
		default: {
			return DefWindowProc(hWnd, uMsg, wParam, lParam);
		}
		}
		return 0;
	}

public:
	App(HINSTANCE hInstance, int nCmdShow)
		: device(proc, hInstance, nCmdShow, 600, 400)
		, buffer("todo.diff") // [TODO] File picker. // [TODO] Start with empty buffer and welcome screen.
	{}

	void run(float init_time) {
		buffer.notify(std::string("init in ") + std::to_string((unsigned)(init_time * 1000.0f)) + "us");
		MSG msg = {};
		while (GetMessage(&msg, nullptr, 0, 0)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) {
	Timer timer;
	const auto start = timer.now();
	App app(hInstance, nCmdShow);
	const auto init_time = timer.duration(start);
	app.run(init_time);
	return 0;
}

