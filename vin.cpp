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

class Picker {
public:
	std::string select() {
		return "todo.diff"; // [TODO] File picker.
	}
};

class App {
	Device device;
	Bar bar;
	Picker picker;

	std::unique_ptr<Buffer> buffer;

	std::string cull_duration;
	std::string redraw_duration;
	std::string process_duration;

	bool space_mode = false; // [TODO] Replace with Menu (similar to Mode) for Space and FilePick.
	bool minimized = false;
	bool dirty = true;
	bool quit = false;

	void set_space_mode(bool b) { space_mode = b; }
	void set_minimized(bool b) { minimized = b; }
	void set_dirty(bool b) { dirty = b; }

	void pick() {
		load(picker.select());
	}

	void load(const std::string_view filename) {
		const Timer timer;
		buffer = std::make_unique<Buffer>(filename);
		notify(std::string("load ") + std::string(filename) + " in " + timer.us());
	}

	void reload() {
		if (buffer) {
			const Timer timer;
			buffer->reload();
			notify(std::string("reload ") + std::string(buffer->get_filename()) + " in " + timer.us());
		}
	}

	void save() {
		if (buffer) {
			const Timer timer;
			buffer->save();
			notify(std::string("save ") + std::string(buffer->get_filename()) + " in " + timer.us());
		}
	}

	void close() {
		if (buffer) {
			const Timer timer;
			const auto filename = std::string(buffer->get_filename());
			buffer.reset();
			notify(std::string("close ") + filename + " in " + timer.us());
		}
	}

	std::string status() {
		return std::string(buffer ? buffer->get_filename() : "") + 
			" (proc: " + process_duration + 
			", cull: " + cull_duration + 
			", draw: " + redraw_duration + ")";
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
			const Timer timer;
			Characters characters;
			characters.reserve(1024);
			bar.cull(characters, status(), viewport.w, viewport.h);
			if (buffer) {
				buffer->cull(characters, viewport.w, viewport.h);
			}
			cull_duration = timer.us();
			{
				const Timer timer;
				device.redraw(characters);
				redraw_duration = timer.us();
			}
		}
		else {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}

	void process_space(unsigned key, unsigned row_count) {
		if (key == 'q') { quit = true; }
		else if (key == 'w') { close(); }
		else if (key == 'e') { pick(); }
		else if (key == 'r') { reload(); }
		else if (key == 's') { save(); }
		else if (key == 'o') { if (buffer) { buffer->state().window_up(row_count); } }
		else if (key == 'i') { if (buffer) { buffer->state().window_down(row_count); } }
	}

	void process(unsigned key) {
		const Timer timer;
		const auto viewport = device.viewport();
		if (space_mode && (!buffer || buffer->is_normal())) {
			process_space(key, viewport.h);
		}
		else {
			if (buffer) { buffer->process(key, viewport.w, viewport.h); }
		}
		if (buffer) { buffer->state().cursor_clamp(viewport.h); }
		process_duration = timer.us();
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
					app->set_space_mode(true);
					app->set_dirty(true);
				}
			}
			break;
		}
		case WM_KEYUP: {
			if (auto* app = reinterpret_cast<App*>(GetWindowLongPtr(hWnd, GWLP_USERDATA))) {
				if (wParam == VK_SPACE) {
					app->set_space_mode(false);
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
		: device(proc, hInstance, nCmdShow, 600, 400)
	{}

	void notify(const std::string& s) {
		bar.notify(s);
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

