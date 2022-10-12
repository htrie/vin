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
#include "vk.h"

enum class Menu {
	space,
	normal,
	picker,
};

class Picker {
	std::vector<std::string> paths;
	std::string filename;

	void populate_directory(const std::filesystem::path& dir) {
		for (const auto& path : std::filesystem::directory_iterator{ dir }) {
			if (path.is_directory()) {
				auto dirname = path.path().generic_string();
				if (dirname.size() > 0 && (dirname.substr(0, 3) != "./.")) { // Skip hidden directories.
					//populate_directory(path); // [TODO] Recursion seems to crash on large folders.
				}
			} else if (path.is_regular_file()) {
				auto filename = path.path().generic_string();
				if (filename.size() > 0 && (filename.substr(0, 3) != "./.")) { // Skip hidden files.
					paths.push_back(path.path().generic_string());
				}
			}
		}
	}

	void push_char(Characters& characters, unsigned row, unsigned col, char character) const {
		characters.emplace_back((uint8_t)character, colors().text, row, col);
	};

	void push_string(Characters& characters, unsigned row, unsigned& col, const std::string_view s) const {
		for (auto& character : s) {
			push_char(characters, row, col++, character);
		}
	}

	void push_cursor(Characters& characters, unsigned row, unsigned col) const {
		characters.emplace_back(Glyph::LINE, colors().cursor, row, col);
	};

public:
	void reset() {
		paths.clear();
		filename.clear();
	}

	void populate() {
		populate_directory(".");
	}

	std::string selection() {
		return filename;
	}

	void process(unsigned key, unsigned col_count, unsigned row_count) { // [TODO] Select with arrows.
		if (key == Glyph::CR) { return; }
		else if (key == Glyph::ESC) { return; }
		else if (key == Glyph::TAB) { return; } // [TODO] Auto completion.
		else if (key == Glyph::BS) { if (filename.size() > 0) { filename.pop_back(); } }
		else { filename += (char)key; }
	}

	void cull(Characters& characters, unsigned col_count, unsigned row_count) const { // [TODO] Display cursor line.
		unsigned col = 0;
		unsigned row = 2;
		push_string(characters, row, col, "open: ");
		push_string(characters, row, col, filename);
		push_cursor(characters, row++, col);
		for (auto& path : paths) {
			if (row == row_count + 3) { break; }
			col = 0;
			if( path.find(filename) != std::string::npos) {
				push_string(characters, row++, col, path);
			}
		}
	}
};

class App {
	Device device;
	Bar bar;
	Picker picker;

	Menu menu = Menu::normal;

	std::unique_ptr<Buffer> buffer; // [TODO] Buffer switcher.

	std::string cull_duration;
	std::string redraw_duration;
	std::string process_duration;

	bool minimized = false;
	bool dirty = true;
	bool quit = false;

	void set_minimized(bool b) { minimized = b; }
	void set_dirty(bool b) { dirty = b; }

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

	std::string status() const {
		return std::string(buffer ? buffer->get_filename() : "<empty>") + 
			" (proc: " + process_duration + 
			", cull: " + cull_duration + 
			", draw: " + redraw_duration + ")";
	}

	Characters cull() const {
		const auto viewport = device.viewport();
		Characters characters;
		characters.reserve(1024);
		bar.cull(characters, status(), viewport.w, viewport.h); // [TODO] Move status to window title bar.
		switch (menu) {
		case Menu::space: // pass-through.
		case Menu::normal: if (buffer) { buffer->cull(characters, viewport.w, viewport.h); } break;
		case Menu::picker: picker.cull(characters, viewport.w, viewport.h); break;
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
		else if (key == 'e') { picker.populate(); menu = Menu::picker; }
		else if (key == 'r') { reload(); }
		else if (key == 's') { save(); }
		else if (key == 'o') { if (buffer) { buffer->state().window_up(row_count); } }
		else if (key == 'i') { if (buffer) { buffer->state().window_down(row_count); } }
	}

	void process_normal(unsigned key, unsigned col_count, unsigned row_count) {
		if (buffer) {
			buffer->process(key, col_count, row_count);
		}
	}

	void process_picker(unsigned key, unsigned col_count, unsigned row_count) {
		if (key == Glyph::CR) { load(picker.selection()); picker.reset(); menu = Menu::normal; }
		else if (key == Glyph::ESC) { picker.reset(); menu = Menu::normal; }
		else { picker.process(key, col_count, row_count); }
	}

	void process(unsigned key) {
		const Timer timer;
		const auto viewport = device.viewport();
		switch (menu) {
		case Menu::space: process_space(key, viewport.h); break;
		case Menu::normal: process_normal(key, viewport.w, viewport.h); break;
		case Menu::picker: process_picker(key, viewport.w, viewport.h); break;
		}
		if (buffer) { buffer->state().cursor_clamp(viewport.h); }
		process_duration = timer.us();
	}

	void update(bool space_down) {
		switch (menu) {
		case Menu::space: if (!space_down) { menu = Menu::normal; } break;
		case Menu::normal: if (space_down && (!buffer || buffer->is_normal())) { menu = Menu::space; } break;
		case Menu::picker: break;
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

