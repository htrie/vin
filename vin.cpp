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
#include <ranges>
#include <unordered_set>
#include <fstream>
#include <filesystem>
#include <future>
#include <thread>
#include <vulkan/vulkan.hpp>
#include <dwmapi.h>
#include <Windows.h>

#include "resource.h"
#include "util.h"
#include "win.h"
#include "matrix.h"
#include "font_common.h"
#include "font_regular.h"
#include "font_bold.h"
#include "font_italic.h"
#include "config.h"
#include "index.h"
#include "text.h"
#include "gpu.h"

enum class Menu {
	space,
	normal,
	picker,
	switcher,
	finder,
};

class App {
	Device device;
	Index index;
	Database database;
	Picker picker;
	Switcher switcher;
	Finder finder;

	Menu menu = Menu::normal;

	Characters characters;

	std::string clipboard;
	std::string notification;

	bool maximized = false;
	bool minimized = false;
	bool dirty = false;
	bool quit = false;

	void set_maximized(bool b) { maximized = b; }
	void set_minimized(bool b) { minimized = b; }
	void set_dirty(bool b) { dirty = b; }

	std::string status() {
		return std::string("Vin v0.5  ") +
			std::string(switcher.current().get_filename()) + 
			(switcher.current().is_dirty() ? "*" : "") + "  " +
			std::to_string(switcher.current().location_percentage()) + "%  " +
			notification;
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
		case Menu::finder: finder.cull(characters, viewport.w, viewport.h); break;
		}
		return characters;
	}

	void resize(unsigned w, unsigned h) {
		if (!minimized) {
			device.resize(w, h);
			dirty = true;
		}
	}

	void redraw() {
		if (dirty) {
			dirty = false;
			characters = cull();
			if (!minimized) {
				device.redraw(characters);
				SetWindowTextA(device.get_hwnd(), status().data());
			}
		}
		else {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}

	void process_space(unsigned key, unsigned row_count) {
		if (key == 'q') { quit = true; }
		else if (key == 'w') { notify(switcher.close()); }
		else if (key == 'e') { index.populate(); picker.filter(index, row_count); menu = Menu::picker; }
		else if (key == 'f') { database.populate(); finder.seed(switcher.current().get_word()); finder.filter(database, row_count); menu = Menu::finder; }
		else if (key == 'r') { notify(switcher.reload()); }
		else if (key == 's') { notify(switcher.save()); }
		else if (key == 'a') { std::system("run.bat"); }
		else if (key == 'c') { std::system("build.bat"); }
		else if (key == 'x') { std::system("commit.bat"); }
		else if (key == 'o') { switcher.current().state().window_up(row_count); }
		else if (key == 'i') { switcher.current().state().window_down(row_count); }
		else if (key == 'j') { switcher.select_next(); menu = Menu::switcher; }
		else if (key == 'k') { switcher.select_previous(); menu = Menu::switcher; }
		else if (key == 'n') { switcher.current().clear_highlight(); }
		else if (key == 'm') { ShowWindow(device.get_hwnd(), maximized ? SW_SHOWDEFAULT : SW_SHOWMAXIMIZED); }
		else if (key == 'b') { colors().use_rgb(); hsb().use_rgb(); }
		else if (key == 'g') { colors().use_gray(); hsb().use_gray(); }
		else if (key == '1') { notify(hsb().decrease_hue_start()); }
		else if (key == '2') { notify(hsb().increase_hue_start()); }
		else if (key == '3') { notify(hsb().decrease_hue_range()); }
		else if (key == '4') { notify(hsb().increase_hue_range()); }
		else if (key == '5') { notify(hsb().decrease_hue_adjust()); }
		else if (key == '6') { notify(hsb().increase_hue_adjust()); }
		else if (key == '7') { notify(hsb().decrease_saturation()); }
		else if (key == '8') { notify(hsb().increase_saturation()); }
		else if (key == '9') { notify(hsb().decrease_brightness()); }
		else if (key == '0') { notify(hsb().increase_brightness()); }
		else if (key == '[') { notify(spacing().decrease_char_width()); }
		else if (key == ']') { notify(spacing().increase_char_width()); }
		else if (key == '{') { notify(spacing().decrease_char_height()); }
		else if (key == '}') { notify(spacing().increase_char_height()); }
	}

	void process_normal(unsigned key, unsigned col_count, unsigned row_count) {
		switcher.current().process(clipboard, key, col_count, row_count);
	}

	void process_picker(unsigned key, unsigned col_count, unsigned row_count) {
		if (key == Glyph::CR) { notify(switcher.load(picker.selection())); picker.reset(); menu = Menu::normal; }
		else if (key == Glyph::ESC) { picker.reset(); menu = Menu::normal; }
		else { picker.process(key, col_count, row_count); picker.filter(index, row_count); }
	}

	void process_switcher(unsigned key, unsigned col_count, unsigned row_count) {
		 switcher.process(key, col_count, row_count);
	}

	void process_finder(unsigned key, unsigned col_count, unsigned row_count) {
		if (key == Glyph::CR) { notify(switcher.load(finder.selection())); switcher.current().jump(finder.position()); finder.reset(); menu = Menu::normal; }
		else if (key == Glyph::ESC) { menu = Menu::normal; }
		else { finder.process(key, col_count, row_count); finder.filter(database, row_count); }
	}

	void process(unsigned key) {
		const auto viewport = device.viewport();
		switch (menu) {
		case Menu::space: process_space(key, viewport.h); break;
		case Menu::normal: process_normal(key, viewport.w, viewport.h); break;
		case Menu::picker: process_picker(key, viewport.w, viewport.h); break;
		case Menu::switcher: process_switcher(key, viewport.w, viewport.h); break;
		case Menu::finder: process_finder(key, viewport.w, viewport.h); break;
		}
		switcher.current().state().cursor_clamp(viewport.h);
		dirty = true;
	}

	void update(bool space_down) {
		switch (menu) {
		case Menu::space: if (!space_down) { menu = Menu::normal; } break;
		case Menu::normal: if (space_down && switcher.current().is_normal()) { menu = Menu::space; } break;
		case Menu::picker: break;
		case Menu::switcher: if (!space_down) { menu = Menu::normal; } break;
		case Menu::finder: break;
		}
		dirty = true;
	}

	static LRESULT CALLBACK proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
		switch (uMsg) {
		case WM_CREATE: {
			LPCREATESTRUCT create_struct = reinterpret_cast<LPCREATESTRUCT>(lParam);
			SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create_struct->lpCreateParams));
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
		case WM_MOVE:
		case WM_SETREDRAW:
		case WM_SETFOCUS: {
			if (auto* app = reinterpret_cast<App*>(GetWindowLongPtr(hWnd, GWLP_USERDATA))) {
				app->set_dirty(true);
			}
			break;
		}
		case WM_PAINT: {
			if (auto* app = reinterpret_cast<App*>(GetWindowLongPtr(hWnd, GWLP_USERDATA))) {
				app->redraw();
			}
			break;
		}
		case WM_SIZE: {
			if (auto* app = reinterpret_cast<App*>(GetWindowLongPtr(hWnd, GWLP_USERDATA))) {
				app->set_maximized(wParam == SIZE_MAXIMIZED);
				app->set_minimized(wParam == SIZE_MINIMIZED);
				const unsigned width = lParam & 0xffff;
				const unsigned height = (lParam & 0xffff0000) >> 16;
				app->resize(width, height);
			}
			break;
		}
		case WM_KEYDOWN: {
			if (auto* app = reinterpret_cast<App*>(GetWindowLongPtr(hWnd, GWLP_USERDATA))) {
				if (wParam == VK_SPACE) {
					app->update(true);
				}
			}
			break;
		}
		case WM_KEYUP: {
			if (auto* app = reinterpret_cast<App*>(GetWindowLongPtr(hWnd, GWLP_USERDATA))) {
				if (wParam == VK_SPACE) {
					app->update(false);
				}
			}
			break;
		}
		case WM_CHAR: {
			if (auto* app = reinterpret_cast<App*>(GetWindowLongPtr(hWnd, GWLP_USERDATA))) {
				app->process((unsigned)wParam);
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
		: device(proc, hInstance, 700, 500) {
		ShowWindow(device.get_hwnd(), nCmdShow);
		colors().use_rgb();
	}

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

