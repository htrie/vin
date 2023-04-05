#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "winhttp.lib")

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define _HAS_EXCEPTIONS 0

#include <array>
#include <unordered_map>
#include <iostream>
#include <filesystem>
#include <sstream>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <dwmapi.h>
#include <winhttp.h>
#include <shlobj.h>

#include "schrift.h"
#include "resource.h"
#include "text.h"
#include "state.h"
#include "buffer.h"
#include "file.h"

const unsigned version_major = 1;
const unsigned version_minor = 3;

class Font {
public:
	struct Glyph {
		SFT_Glyph gid;
		SFT_GMetrics mtx;
		std::vector<uint8_t> pixels;
	};

private:
	SFT_Font* font = nullptr;
	SFT sft;
	SFT_LMetrics lmtx;

	std::unordered_map<uint32_t, Glyph> glyphs;

	const Glyph& add_glyph(uint32_t codepoint) {
		auto& glyph = glyphs[codepoint];
		if (sft_lookup(&sft, codepoint, &glyph.gid) == 0) {
			if (sft_gmetrics(&sft, glyph.gid, &glyph.mtx) == 0) {
				SFT_Image image;
				image.width  = glyph.mtx.minWidth;
				image.height = glyph.mtx.minHeight;
				glyph.pixels.resize(image.width * image.height);
				image.pixels = glyph.pixels.data();
				sft_render(&sft, glyph.gid, image);
				return glyph;
			}
		}
		return add_glyph(0);
	}

	SFT_Font* try_font(const std::string_view filename) {
		if (auto* font = sft_loadfile(filename.data()))
			return font;
		return nullptr;
	}

public:
	Font() {
		font = try_font(get_user_font_path() + "PragmataPro_Mono_R_liga.ttf");
		if (font == nullptr)
			font = try_font(get_system_font_path() + get_system_font_name("Consolas"));
		if (font) {
			memset(&sft, 0, sizeof sft);
			sft.font = font;
			sft.flags = SFT_DOWNWARD_Y;
		}
	}

	~Font() {
		if (font)
			sft_freefont(font);
	}

	void set_size(double size) {
		if (size != sft.xScale) {
			sft.xScale = size;
			sft.yScale = size;
			sft_lmetrics(&sft, &lmtx);
			glyphs.clear();
			add_glyph(0);
		}
	}

	const Glyph& find_glyph(uint32_t codepoint) {
		if (auto found = glyphs.find(codepoint); found != glyphs.end())
			return found->second;
		return add_glyph(codepoint);
	}

	unsigned get_character_width() const { return (unsigned)glyphs.find(0)->second.mtx.advanceWidth; }
	unsigned get_line_height() const { return (unsigned)(sft.yScale - lmtx.descender); }
	unsigned get_line_baseline() const { return (unsigned)lmtx.ascender; }
};

class Window {
	HWND hwnd = nullptr;

	std::vector<COLORREF> pixels;
	static_assert(sizeof(COLORREF) == sizeof(uint32_t));

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
		const auto title  = std::string("Vin ") + std::to_string(version_major) + "." + std::to_string(version_minor);
		SetWindowTextA(hwnd, title.data());
	}

	static void set_dark(HWND hwnd) {
		BOOL value = TRUE;
		DwmSetWindowAttribute(hwnd, 20, &value, sizeof(value)); // Dark mode.
	}

public:
	Window(HINSTANCE hinstance, WNDPROC proc, void* data)
		: hwnd(create(hinstance, proc, data)) {
		set_dark(hwnd);
		set_title(hwnd);
	}

	~Window() {
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
		pixels.resize(width * height);
	}

	void clear(uint32_t color) {
		memset(pixels.data(), color, width * height * sizeof(COLORREF));
	}

	void blit() {
		const HBITMAP map = CreateBitmap(width, height, 1, 32, (void*)pixels.data());
		const HDC hdc = GetDC(hwnd);
		const HDC src = CreateCompatibleDC(GetDC(hwnd));
		SelectObject(src, map);
		BitBlt(hdc, 0, 0, width, height, src, 0, 0, SRCCOPY);
		DeleteDC(src);
		DeleteObject(map);
	}

	Color* get_pixels() { return (Color*)pixels.data(); }
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
		else {
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
	}

	void process_space_f() {
		const auto seed = current().get_word();
		open(std::string("find ") + seed);
		current().init(find(seed));
		current().set_highlight(seed);
	}

	void process_space(bool& quit, bool& maximize, double& zoom, unsigned key) {
		if (key == 'q') { quit = true; }
		else if (key == 'm') { maximize = true; }
		else if (key == '+') { zoom *= 1.05; }
		else if (key == '-') { zoom *= 0.95; }
		else if (key == 'w') { close(); }
		else if (key == 'r') { reload(); }
		else if (key == 's') { save(); }
		else if (key == 'e') { process_space_e(); }
		else if (key == 'f') { process_space_f(); }
		else if (key == 'i') { current().window_down(); }
		else if (key == 'o') { current().window_up(); }
		else if (key >= '0' && key <= '9') { select_index(key - '0'); }
		else if (key == 'h') { select_previous(); }
		else if (key == 'l') { select_next(); }
		else if (key == 'n') { current().clear_highlight(); }
	}

	void process_normal(unsigned key) {
		current().process(clipboard, url, key);
		if (!url.empty()) {
			open(extract_filename(url));
			current().jump(extract_location(url));
			url.clear();
		}
	}

	void push_tabs(Characters& characters) const {
		std::vector<std::string> tabs;
		for (size_t i = 0; i < buffers.size(); ++i) {
			tabs.push_back(std::to_string(i) + ":" + std::string(buffers[i].get_filename()) + (buffers[i].is_dirty() ? "*" : ""));
		}
		const unsigned row = 0;
		unsigned col = 0;
		for (size_t i = 0; i < buffers.size(); ++i) {
			push_line(characters, i == active ? colors().bar_text : colors().bar, row, col, col + (int)tabs[i].size());
			push_string(characters, i == active ? colors().bar : colors().bar_text, row, col, tabs[i]);
			col += 1;
		}
	}

public:
	Switcher() {
		open("");
	}

	void process(bool space_down, bool& quit, bool& maximize, double& zoom, unsigned key) {
		if (space_down && current().is_normal()) { process_space(quit, maximize, zoom, key); }
		else { process_normal(key); }
	}

	Characters cull(unsigned col_count, unsigned row_count) {
		Characters characters;
		push_tabs(characters);
		current().set_line_count(current().cull(characters, col_count, row_count - 1)); // Remove 1 for tabs bar.
		return characters;
	}
};

class Application {
	Switcher switcher;
	Window window;
	Font font;

	bool minimized = false;
	bool maximized = false;
	bool dirty = true;
	bool space_down = false;
	bool quit = false;

	double zoom = 1.0;

	void set_minimized(bool b) { minimized = b; }
	void set_maximized(bool b) { maximized = b; }
	void set_dirty(bool b) { dirty = b; }
	void set_space_down(bool b) { space_down = b; }

	double get_default_font_size() const { return double(window.get_dpi() / 6); } 

	unsigned get_col_count() const { return (unsigned)((float)window.get_width() / (float)font.get_character_width()); }
	unsigned get_row_count() const { return (unsigned)((float)window.get_height() / (float)font.get_line_height() - 0.5f); }

	void resize(unsigned width, unsigned height) {
		if (!minimized) {
			window.resize(width, height);
		}
	}

	void render(const Characters& characters) {
		auto* pixels = window.get_pixels();
		for (auto& character : characters) {
			const auto& glyph = font.find_glyph(character.index);
			unsigned in = 0;
			unsigned out = (font.get_line_baseline() + character.row * font.get_line_height() + glyph.mtx.yOffset) * window.get_width() +
				(character.col * font.get_character_width() + (int)glyph.mtx.leftSideBearing);
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

	void redraw() {
		if (!minimized && dirty) {
			const auto characters = switcher.cull(get_col_count(), get_row_count());
			window.clear(colors().clear.as_uint());
			render(characters);
			window.blit();
		}
		else {
			Sleep(1); // Avoid busy loop when minimized.
		}
		dirty = false;
	}

	void process(unsigned key) {
		bool maximize = false;
		switcher.process(space_down, quit, maximize, zoom, key);
		if (maximize)
			window.maximize(!maximized);
		font.set_size(zoom * get_default_font_size()); 
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
		case WM_SIZING:
		case WM_MOUSEMOVE:
		case WM_MOUSELEAVE:
		case WM_MOVE:
		case WM_MOVING:
		case WM_KILLFOCUS:
		case WM_SETFOCUS: {
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
		: window(hinstance, proc, this) {
		font.set_size(get_default_font_size());
		window.set_size(7 * window.get_dpi(), 5 * window.get_dpi());
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

