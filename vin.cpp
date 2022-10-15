#define NOMINMAX
#define VC_EXTRALEAN
#define WIN32_LEAN_AND_MEAN
#define _HAS_EXCEPTIONS 0
#define VULKAN_HPP_NO_EXCEPTIONS

#include <math.h>
#include <stdlib.h>
#include <string>
#include <chrono>
#include <map>
#include <fstream>
#include <filesystem>
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
#include "gpu.h"

enum class Menu {
	space,
	normal,
	picker,
	switcher,
};

class App {
	Device device;
	Picker picker;
	Switcher switcher;

	Menu menu = Menu::normal;

	std::string cull_duration;
	std::string redraw_duration;
	std::string process_duration;

	std::string notification;

	bool minimized = false;
	bool dirty = true;
	bool quit = false;

	void set_minimized(bool b) { minimized = b; }
	void set_dirty(bool b) { dirty = b; }

	std::string status() {
		return std::string("Vin v0.1 - ") +
			std::string(switcher.current().get_filename()) + 
			(switcher.current().is_dirty() ? "*" : "") +
			" - " +
			"proc " + process_duration +
			", cull " + cull_duration +
			", draw " + redraw_duration + 
			" - " + notification;
	}

	Characters cull() {
		const auto viewport = device.viewport();
		Characters characters;
		characters.reserve(1024);
		switch (menu) {
		case Menu::space: // pass-through.
		case Menu::normal: switcher.current().cull(characters, viewport.w, viewport.h); break;
		case Menu::picker: picker.cull(characters, viewport.w, viewport.h); break;
		case Menu::switcher: switcher.current().cull(characters, viewport.w, viewport.h); switcher.cull(characters, viewport.w, viewport.h); break;
		}
		return characters;
	}

	void resize(unsigned w, unsigned h) {
		if (!minimized) {
			device.resize(w, h);
		}
	}

	void redraw() {
		if (!minimized && dirty) {
			dirty = false;
			const Timer timer;
			const auto characters = cull();
			cull_duration = timer.us();
			{
				const Timer timer;
				device.redraw(characters, status());
				redraw_duration = timer.us();
			}
		}
		else {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}

	void process_space(unsigned key, unsigned row_count) {
		if (key == 'q') { quit = true; }
		else if (key == 'w') { notify(switcher.close()); }
		else if (key == 'e') { picker.filter(row_count); menu = Menu::picker; }
		else if (key == 'r') { notify(switcher.reload()); }
		else if (key == 's') { notify(switcher.save()); }
		else if (key == 'o') { switcher.current().state().window_up(row_count); }
		else if (key == 'i') { switcher.current().state().window_down(row_count); }
		else if (key == 'p') { notify(picker.populate()); }
		else if (key == 'j') { menu = Menu::switcher; }
		else if (key == 'k') { menu = Menu::switcher; }
		else if (key == 'c') { } // [TODO] Copy to clipboard.
		else if (key == 'v') { } // [TODO] Paste from clipboard.
	}

	void process_normal(unsigned key, unsigned col_count, unsigned row_count) {
		switcher.current().process(key, col_count, row_count);
	}

	void process_picker(unsigned key, unsigned col_count, unsigned row_count) {
		if (key == Glyph::CR) { notify(switcher.load(picker.selection())); picker.reset(); menu = Menu::normal; }
		else if (key == Glyph::ESC) { picker.reset(); menu = Menu::normal; }
		else { picker.process(key, col_count, row_count); picker.filter(row_count); }
	}

	void process_switcher(unsigned key, unsigned col_count, unsigned row_count) {
		 switcher.process(key, col_count, row_count);
	}

	void process(unsigned key) {
		const Timer timer;
		const auto viewport = device.viewport();
		switch (menu) {
		case Menu::space: process_space(key, viewport.h); break;
		case Menu::normal: process_normal(key, viewport.w, viewport.h); break;
		case Menu::picker: process_picker(key, viewport.w, viewport.h); break;
		case Menu::switcher: process_switcher(key, viewport.w, viewport.h); break;
		}
		switcher.current().state().cursor_clamp(viewport.h);
		process_duration = timer.us();
	}

	void update(bool space_down) {
		switch (menu) {
		case Menu::space: if (!space_down) { menu = Menu::normal; } break;
		case Menu::normal: if (space_down && switcher.current().is_normal()) { menu = Menu::space; } break;
		case Menu::picker: break;
		case Menu::switcher: if (!space_down) { menu = Menu::normal; } break;
		}
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
		case WM_KEYDOWN: {
			if (auto* app = reinterpret_cast<App*>(GetWindowLongPtr(hWnd, GWLP_USERDATA))) {
				if (wParam == VK_SPACE) {
					app->update(true);
					app->set_dirty(true);
				}
			}
			break;
		}
		case WM_KEYUP: {
			if (auto* app = reinterpret_cast<App*>(GetWindowLongPtr(hWnd, GWLP_USERDATA))) {
				if (wParam == VK_SPACE) {
					app->update(false);
					app->set_dirty(true);
				}
			}
			break;
		}
		case WM_CHAR: {
			if (auto* app = reinterpret_cast<App*>(GetWindowLongPtr(hWnd, GWLP_USERDATA))) {
				app->process((unsigned)wParam);
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
		: device(proc, hInstance, nCmdShow, 800, 600)
	{}

	void notify(const std::string& s) {
		notification = timestamp() + "  " + s;
	}

	void run() {
		MSG msg = {};
		while (!quit && GetMessage(&msg, nullptr, 0, 0)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) {
	const Timer timer;
	App app(hInstance, nCmdShow);
	app.notify(std::string("init in ") + timer.us());
	app.run();
	return 0;
}

