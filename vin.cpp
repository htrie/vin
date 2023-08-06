#pragma comment(lib, "dwmapi.lib")

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define _HAS_EXCEPTIONS 0

#include <stddef.h>
#include <stdint.h>
#include <assert.h>
#include <array>
#include <unordered_map>
#include <iostream>
#include <filesystem>
#include <sstream>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <psapi.h>
#include <dwmapi.h>
#include <shlobj.h>

#include "resource.h"
#include "text.h"
#include "state.h"
#include "buffer.h"
#include "file.h"
#include "font.h"

const unsigned version_major = 1;
const unsigned version_minor = 4;

class System {
public:
	static size_t get_memory_usage() {
		PROCESS_MEMORY_COUNTERS_EX counters;
		GetProcessMemoryInfo(GetCurrentProcess(), (PPROCESS_MEMORY_COUNTERS)&counters, sizeof(PROCESS_MEMORY_COUNTERS_EX));
		return (size_t)counters.PrivateUsage;
	}
};

class Window {
	HWND hwnd = nullptr;
	HDC hdc = nullptr;
	HBITMAP bitmap = nullptr;
	COLORREF* bits = nullptr;

	unsigned width = 0;
	unsigned height = 0;

	static HWND create(HINSTANCE hinstance, WNDPROC proc, void* data) {
		const char* name = "vin";
		const auto hicon = LoadIcon(hinstance, MAKEINTRESOURCE(IDI_ICON1));
		WNDCLASSEX win_class;
		win_class.cbSize = sizeof(WNDCLASSEX);
		win_class.style = CS_HREDRAW | CS_VREDRAW;
		win_class.lpfnWndProc = proc;
		win_class.cbClsExtra = 0;
		win_class.cbWndExtra = 0;
		win_class.hInstance = hinstance;
		win_class.hIcon = hicon;
		win_class.hCursor = LoadCursor(nullptr, IDC_ARROW);
		win_class.hbrBackground = CreateSolidBrush(0);
		win_class.lpszMenuName = nullptr;
		win_class.lpszClassName = name;
		win_class.hIconSm = hicon;
		RegisterClassEx(&win_class);
		SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE);
		return CreateWindowEx(0, name, name, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1, 1, nullptr, nullptr, hinstance, data);
	}

	static void destroy(HWND hwnd) {
		DestroyWindow(hwnd);
	}

	static void set_title(HWND hwnd) {
		SetWindowTextA(hwnd, "");
	}

	static void set_dark(HWND hwnd) {
		BOOL value = TRUE;
		DwmSetWindowAttribute(hwnd, 20, &value, sizeof(value)); // Dark mode.
	}

	void reset() {
		if (bitmap)
			DeleteObject(bitmap);
		if (hdc)
			DeleteDC(hdc);
	}

	void recreate() {
		BITMAPINFO info = {};
		info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		info.bmiHeader.biWidth = width;
		info.bmiHeader.biHeight = -(LONG)height; // Negative so (0,0) is at top left.
		info.bmiHeader.biPlanes = 1;
		info.bmiHeader.biBitCount = 32;
		bitmap = CreateDIBSection(GetDC(hwnd), &info, DIB_RGB_COLORS, (void**)&bits, NULL, 0);
		if (bitmap) {
			hdc = CreateCompatibleDC(GetDC(hwnd));
			if (hdc)
				SelectObject(hdc, bitmap);
		}
	}

public:
	Window(HINSTANCE hinstance, WNDPROC proc, void* data)
		: hwnd(create(hinstance, proc, data)) {
		set_dark(hwnd);
		set_title(hwnd);
	}

	~Window() {
		reset();
		destroy(hwnd);
	}

	void show(int nshow) {
		ShowWindow(hwnd, nshow);
	}

	void maximize(bool b) {
		ShowWindow(hwnd, b ? SW_SHOWMAXIMIZED : SW_SHOWDEFAULT);
	}

	void set_size(int w, int h) {
		SetWindowPos(hwnd, 0, 0, 0, w, h, SWP_NOMOVE | SWP_NOOWNERZORDER);
	}

	void resize(unsigned width, unsigned height) {
		this->width = width;
		this->height = height;
		reset();
		recreate();
	}

	void clear(uint32_t color) {
		static_assert(sizeof(COLORREF) == sizeof(uint32_t));
		if (bits)
			memset(bits, color, width * height * sizeof(COLORREF));
	}

	void blit() {
		if (hdc)
			BitBlt(GetDC(hwnd), 0, 0, width, height, hdc, 0, 0, SRCCOPY);
	}

	Color* get_pixels() { return (Color*)bits; }
	unsigned get_width() const { return width; }
	unsigned get_height() const { return height; }

	unsigned get_dpi() const { return GetDpiForWindow(hwnd); }
};

class Switcher {
	std::vector<Buffer> buffers;
	size_t active = 0;

	std::string url;
	std::string clipboard;

	Buffer& current() { return buffers[active]; }
	const Buffer& current() const { return buffers[active]; }

	void select_index(unsigned index) { active = index >= 0 && index < buffers.size() ? index : active; }
	void select_previous() { active = active > 0 ? active - 1 : buffers.size() - 1; }
	void select_next() { active = (active + 1) % buffers.size(); }

	size_t find_buffer(const std::string_view filename) {
		for (size_t i = 0; i < buffers.size(); ++i) {
			if (buffers[i].get_filename() == filename)
				return i;
		}
		return (size_t)-1;
	}

	void open(const std::string_view filename) {
		if (auto index = find_buffer(filename); index != (size_t)-1) {
			active = index;
		}
		else if (buffers.size() < 10) { // No more than 10 tabs so we can use numbered fast switch.
			buffers.emplace_back(filename);
			active = buffers.size() - 1;
			current().init(load(filename));
		}
	}

	void reload() {
		current().init(load(current().get_filename()));
	}

	void save() {
		if (write(current().get_filename(), current().get_text())) {
			current().set_dirty(false);
		}
	}

	void close() {
		if (active > 0) { // Don't close buffer 0.
			const auto filename = std::string(current().get_filename());
			buffers.erase(buffers.begin() + active);
			active = (active >= buffers.size() ? active - 1 : active) % buffers.size();
		}
	}

	void process_space_e() {
		open("list");
		current().init(list());
		current().set_dirty(true);
	}

	void process_space_f() {
		const auto seed = current().get_word();
		open(std::string("find ") + seed);
		current().init(find(seed));
		current().set_highlight(seed);
	}

	void process_space(bool& quit, bool& maximize, double& font_size, unsigned key) {
		if (key == 'q') { quit = true; }
		else if (key == 'm') { maximize = true; }
		else if (key == '+') { font_size = std::min(font_size + 1.0, 80.0); }
		else if (key == '-') { font_size = std::max(font_size - 1.0, 8.0); }
		else if (key == 'w') { close(); }
		else if (key == 'r') { reload(); }
		else if (key == 's') { save(); }
		else if (key == 'e') { process_space_e(); }
		else if (key == 'f') { process_space_f(); }
		else if (key == 'j') { current().window_down(); }
		else if (key == 'k') { current().window_up(); }
		else if (key == 'h') { select_previous(); }
		else if (key == 'l') { select_next(); }
		else if (key == 'n') { current().clear_highlight(); }
		else if (key >= '0' && key <= '9') { select_index(key - '0'); }
	}

	void process_normal(unsigned key) {
		current().process(clipboard, url, key);
		if (!url.empty()) {
			open(extract_filename(url));
			current().jump(extract_location(url));
			url.clear();
		}
	}

	void push_status(Characters& characters, unsigned col_count, const std::string_view text, const std::string_view status) const {
		const unsigned row = 0;
		unsigned col = 0;
		push_line(characters, colors().status, row, col, col_count);
		push_string(characters, colors().status_text, row, col, text);
		push_string(characters, colors().status_text, row, col, "  ");
		push_string(characters, colors().status_text, row, col, status);
	}

	void push_tabs(Characters& characters) const {
		const unsigned row = 1;
		unsigned col = 0;
		for (unsigned i = 0; i < buffers.size(); ++i) {
			const auto back_color = i == active ? colors().bar_text : colors().bar;
			const auto fore_color = i == active ? colors().bar : colors().bar_text;
			const auto text = std::string(buffers[i].get_filename()) + (buffers[i].is_dirty() ? "*" : "");
			push_line(characters, back_color, row, col, col + (int)(1 + text.size()));
			characters.emplace_back(superscript_codepoint(i), fore_color, row, col++);
			push_string(characters, fore_color, row, col, text);
			col += 1;
		}
	}

public:
	Switcher() {
		open("scratch");
	}

	void process(bool space_down, bool& quit, bool& maximize, double& font_size, unsigned key) {
		if (space_down && current().is_normal()) { process_space(quit, maximize, font_size, key); }
		else { process_normal(key); }
	}

	Characters cull(unsigned col_count, unsigned row_count, const std::string_view text) {
		Characters characters;
		push_status(characters, col_count, text, current().status());
		push_tabs(characters);
		current().set_line_count(current().cull(characters, col_count, row_count));
		return characters;
	}
};

class Application {
	Switcher switcher;
	Window window;
	Book book;

	bool minimized = false;
	bool maximized = false;
	bool dirty = true;
	bool space_down = false;
	bool quit = false;

	double font_size = 1.0;

	int64_t render_time_ms = 0;
	int64_t process_time_ms = 0;

	void set_minimized(bool b) { minimized = b; }
	void set_maximized(bool b) { maximized = b; }
	void set_dirty(bool b) { dirty = b; }
	void set_space_down(bool b) { space_down = b; }

	double get_default_font_size() const { return window.get_dpi() / 6.4; } 

	unsigned get_col_count() const { return (unsigned)((float)window.get_width() / (float)book.get_character_width()); }
	unsigned get_row_count() const { return (unsigned)((float)window.get_height() / (float)book.get_line_height()); }

	std::string get_status_text() const {
		return "v" + std::to_string(version_major) + "." + std::to_string(version_minor) +
			" " + readable_size(System::get_memory_usage()) + 
			" " + std::to_string(unsigned(font_size)) + "pt" + 
			" " + std::to_string(window.get_width()) + "x" + std::to_string(window.get_height()) + 
			" " + std::to_string(process_time_ms) + "ms:" + std::to_string(render_time_ms) + "ms";
	}

	void resize(unsigned width, unsigned height) {
		if (!minimized) {
			window.resize(width, height);
		}
	}

	void render(const Characters& characters) {
		if (auto* pixels = window.get_pixels()) {
			window.clear(colors().clear.as_uint());
			const auto col_count = get_col_count();
			const auto row_count = get_row_count();
			for (auto& character : characters) {
				if (character.col < col_count && character.row < row_count) {
					const auto& glyph = book.find_glyph(character.index);
					unsigned in = 0;
					unsigned out = (book.get_line_baseline() + character.row * book.get_line_height() + glyph.mtx.yOffset) * window.get_width() +
						(character.col * book.get_character_width() + (int)glyph.mtx.leftSideBearing);
					auto color = character.color;
					for (unsigned j = 0; j < (unsigned)glyph.mtx.minHeight; ++j) {
						for (unsigned i = 0; i < (unsigned)glyph.mtx.minWidth; ++i) {
							if (out + i < window.get_width() * window.get_height()) {
								pixels[out + i].blend(color.set_alpha(glyph.pixels[in + i]));
							}
						}
						in += glyph.mtx.minWidth;
						out += window.get_width();
					}
				}
			}
			window.blit();
		}
	}

	void redraw() {
		if (!minimized && dirty) {
			Timer timer;
			render(switcher.cull(get_col_count(), get_row_count(), get_status_text()));
			render_time_ms = timer.get_elapsed_time_ms();
		}
		else {
			Sleep(1); // Avoid busy loop when minimized.
		}
		dirty = false;
	}

	void process(unsigned key) {
		Timer timer;
		bool maximize = false;
		switcher.process(space_down, quit, maximize, font_size, key);
		if (maximize)
			window.maximize(!maximized);
		book.set_font_size(font_size); 
		process_time_ms = timer.get_elapsed_time_ms();
	}

	static LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
		switch (msg) {
		case WM_CREATE: {
			LPCREATESTRUCT create_struct = reinterpret_cast<LPCREATESTRUCT>(lparam);
			SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create_struct->lpCreateParams));
			break;
		}
		case WM_DESTROY:
		case WM_CLOSE: {
			PostQuitMessage(0);
			break;
		}
		case WM_GETMINMAXINFO: {
			((MINMAXINFO*)lparam)->ptMinTrackSize = {
				GetSystemMetrics(SM_CXMINTRACK),
				GetSystemMetrics(SM_CYMINTRACK) + 1 // Must be at least 1 pixel high.
			};
			break;
		}
		case WM_ERASEBKGND: {
			if (auto* app = reinterpret_cast<Application*>(GetWindowLongPtr(hwnd, GWLP_USERDATA))) {
				app->set_dirty(true);
			}
			break;
		}
		case WM_SETREDRAW:
		case WM_PAINT: {
			if (auto* app = reinterpret_cast<Application*>(GetWindowLongPtr(hwnd, GWLP_USERDATA))) {
				app->redraw();
			}
			break;
		}
		case WM_SIZE: {
			if (auto* app = reinterpret_cast<Application*>(GetWindowLongPtr(hwnd, GWLP_USERDATA))) {
				const unsigned width = lparam & 0xffff;
				const unsigned height = (lparam & 0xffff0000) >> 16;
				app->set_minimized(wparam == SIZE_MINIMIZED);
				app->set_maximized(wparam == SIZE_MAXIMIZED);
				app->set_dirty(true);
				app->resize(width, height);
			}
			break;
		}
		case WM_KEYDOWN: {
			if (auto* app = reinterpret_cast<Application*>(GetWindowLongPtr(hwnd, GWLP_USERDATA))) {
				if (wparam == VK_SPACE) {
					app->set_dirty(true);
					app->set_space_down(true);
				}
			}
			break;
		}
		case WM_KEYUP: {
			if (auto* app = reinterpret_cast<Application*>(GetWindowLongPtr(hwnd, GWLP_USERDATA))) {
				if (wparam == VK_SPACE) {
					app->set_dirty(true);
					app->set_space_down(false);
				}
			}
			break;
		}
		case WM_CHAR: {
			if (auto* app = reinterpret_cast<Application*>(GetWindowLongPtr(hwnd, GWLP_USERDATA))) {
				app->set_dirty(true);
				app->process((unsigned)wparam);
			}
			break;
		}
		default: {
			return DefWindowProc(hwnd, msg, wparam, lparam);
		}
		}
		return 0;
	}

public:
	Application(HINSTANCE hinstance, int nshow)
		: window(hinstance, proc, this)
		, font_size(get_default_font_size()) {
		book.set_font_size(font_size);
		window.set_size(8 * window.get_dpi(), 26 * window.get_dpi() / 5);
		window.show(nshow);
	}

	void run() {
		MSG msg = {};
		while (!quit && GetMessage(&msg, nullptr, 0, 0)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
};

int WINAPI WinMain(HINSTANCE hinstance, HINSTANCE hprev, LPSTR cmd, int nshow) {
	Application application(hinstance, nshow);
	application.run();
	return 0;
}

