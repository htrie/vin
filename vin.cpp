#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "winhttp.lib")

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define _HAS_EXCEPTIONS 0

#include <array>
#include <iostream>
#include <filesystem>
#include <sstream>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <dwmapi.h>
#include <winhttp.h>

#include "resource.h"
#include "text.h"
#include "state.h"
#include "buffer.h"
#include "font.h"

const unsigned version_major = 1;
const unsigned version_minor = 2;

bool ignore_file(const std::string_view path) {
	if (path.ends_with(".db")) return true;
	if (path.ends_with(".aps")) return true;
	if (path.ends_with(".bin")) return true;
	if (path.ends_with(".dat")) return true;
	if (path.ends_with(".exe")) return true;
	if (path.ends_with(".idb")) return true;
	if (path.ends_with(".ilk")) return true;
	if (path.ends_with(".iobj")) return true;
	if (path.ends_with(".ipdb")) return true;
	if (path.ends_with(".jpg")) return true;
	if (path.ends_with(".lib")) return true;
	if (path.ends_with(".obj")) return true;
	if (path.ends_with(".pch")) return true;
	if (path.ends_with(".pdb")) return true;
	if (path.ends_with(".png")) return true;
	if (path.ends_with(".tlog")) return true;
	return false;
}

std::string get_error_string() {
	const DWORD error = GetLastError();
	std::string res = "(error " + std::to_string((unsigned)error);
	char* message = NULL;
	if (FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&message, 0, NULL)) {
		res += ": " + std::string(message);
		LocalFree(message);
	}
	res += ")";
	return res;
}

std::wstring convert(const std::string_view url) {
	if (const auto len = mbstowcs(nullptr, url.data(), 0); len == url.length()) {
		std::wstring res;
		res.resize(len);
		if (const auto converted = mbstowcs(res.data(), url.data(), len + 1); converted == len)
			return res;
	}
	return {};
}

std::string download(const HINTERNET request) {
	std::string contents;
	DWORD size = 0;
	do {
		if (WinHttpQueryDataAvailable(request, &size)) {
			std::vector<char> partial(size);
			DWORD downloaded = 0;
			if (WinHttpReadData(request, (LPVOID)partial.data(), (DWORD)partial.size(), &downloaded) && downloaded == size) {
				contents += std::string(partial.begin(), partial.end());
			}
		}
	} while (size > 0);
	return contents;
}

std::string request(const std::string_view url) {
	std::string contents;
	const auto server = convert(extract_server(url));
	const auto payload = convert(extract_payload(url));
	if (const auto session = WinHttpOpen(L"Vin", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0)) {
		if (const auto connection = WinHttpConnect(session, server.data(), INTERNET_DEFAULT_HTTPS_PORT, 0)) {
			if (const auto request = WinHttpOpenRequest(connection, L"GET", payload.data(), NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE)) {
				if (WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
					if (WinHttpReceiveResponse(request, NULL))
						contents = download(request);
				}
				else { contents = std::string(url) + " Failed to send request " + get_error_string(); }
				WinHttpCloseHandle(request);
			}
			else { contents = std::string(url) + " Failed to open request " + get_error_string(); }
			WinHttpCloseHandle(connection);
		}
		else { contents = std::string(url) + " Failed to establish connection " + get_error_string(); }
		WinHttpCloseHandle(session);
	}
	else { contents = std::string(url) + " Failed to open session " + get_error_string(); }
	return contents;
}

template<typename F>
void process_files(const std::string& path, F func) {
	const auto filter = path + "/*";
	WIN32_FIND_DATA find_data;
	HANDLE handle = FindFirstFile(filter.c_str(), &find_data);
	do {
		if (handle != INVALID_HANDLE_VALUE) {
			if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) > 0) {
				if (find_data.cFileName[0] != '.') {
					const auto dir = path + "/" + find_data.cFileName;
					process_files(dir, func);
				}
			}
			else if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
				const auto file = path + "/" + find_data.cFileName;
				func(file);
			}
		}
	} while (FindNextFile(handle, &find_data) != 0);
	FindClose(handle);
}

template <typename F>
void map(const std::string_view filename, F func) {
	if (const auto file = CreateFileA(filename.data(), GENERIC_READ, FILE_SHARE_READ, nullptr,
		OPEN_EXISTING, FILE_ATTRIBUTE_READONLY | FILE_FLAG_SEQUENTIAL_SCAN, nullptr); file != INVALID_HANDLE_VALUE) {
		if (size_t size = 0; GetFileSizeEx(file, (PLARGE_INTEGER)&size)) {
			if (const auto mapping = CreateFileMapping(file, nullptr, PAGE_READONLY, 0, 0, nullptr); mapping != INVALID_HANDLE_VALUE) {
				if (const auto mem = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0); mem != nullptr) {
					func((char*)mem, size);
					UnmapViewOfFile(mem);
				}
				CloseHandle(mapping);
			}
		}
		CloseHandle(file);
	}
}

bool write(const std::string_view filename, const std::string_view text) {
	bool res = false;
	if (const auto file = CreateFileA(filename.data(), GENERIC_WRITE, 0, nullptr,
		CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr); file != INVALID_HANDLE_VALUE) {
		res = WriteFile(file, text.data(), (DWORD)text.size(), nullptr, nullptr);
		CloseHandle(file);
	}
	return res;
}

std::string list() {
	std::string list;
	process_files(".", [&](const auto& path) {
		if (!ignore_file(path))
			list += path + "\n";
		});
	return list;
}

std::string_view cut_line(const std::string_view text, size_t pos) {
	Line line(text, pos);
	return line.to_string(text);
}

std::string make_line(const std::string_view text) {
	if (text.size() > 0)
		if (text[text.size() - 1] == '\n')
			return std::string(text);
	return std::string(text) + "\n";
}

std::string make_entry(size_t pos, const char* mem, size_t size) {
	const auto start = pos > (size_t)100 ? pos - (size_t)100 : (size_t)0;
	const auto count = std::min((size_t)200, size - start);
	const auto context = std::string(&mem[start], count);
	return "(" + std::to_string(pos) + ") " + make_line(cut_line(context, pos - start));
}

std::string scan(const std::string_view path, const std::string_view pattern) {
	std::string list;
	map(path, [&](const char* mem, size_t size) {
		for (size_t i = 0; i < size; ++i) {
			if (mem[i] == pattern[0]) {
				if (strncmp(&mem[i], pattern.data(), std::min(pattern.size(), size - i)) == 0) {
					list += std::string(path) + make_entry(i, mem, size);
				}
			}
		}
		});
	return list;
}

std::string find(const std::string_view pattern) {
	std::string list;
	if (!pattern.empty() && pattern.size() > 2) {
		process_files(".", [&](const auto& path) {
			if (!ignore_file(path)) {
				list += scan(path, pattern);
			}
			});
	}
	return list;
}

std::string prettify_html(const std::string_view contents) {
	std::string out = std::string(contents);
	// TODO parse html <p> and <a>
	return out;
}

std::string load(const std::string_view filename) {
	std::string text = "\n"; // EOF
	if (filename.starts_with("www")) {
		text = prettify_html(request(filename)); }
	else if (filename.starts_with("https")) {
		text = prettify_html(request(filename.substr(sizeof("https://") - 1))); }
	else if (filename.starts_with("http")) {
		text = prettify_html(request(filename.substr(sizeof("http://") - 1))); }
	else if (std::filesystem::exists(filename)) {
		map(filename, [&](const char* mem, size_t size) {
			text = std::string(mem, size);
			});
	}
	return text;
}

const FontGlyph* find_glyph(uint16_t id) {
	for (auto& glyph : font_glyphs) {
		if (glyph.id == id)
			return &glyph;
	}
	return nullptr;
}

class Window {
	HWND hwnd = nullptr;

	COLORREF* pixels = nullptr;
	static_assert(sizeof(COLORREF) == sizeof(uint32_t));

	unsigned width = 0;
	unsigned height = 0;

	unsigned spacing_character = 9;
	unsigned spacing_line = 20;

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
		return CreateWindowEx(0, name, name, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1024, 768, nullptr, nullptr, hinstance, data);
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

	static void show(HWND hwnd, int nshow) {
		ShowWindow(hwnd, nshow);
	}

	void reset() {
		if (pixels) {
			free(pixels);
			pixels = nullptr;
		}
	}

	void alloc() {
		pixels = (COLORREF*)malloc(width * height * sizeof(COLORREF));
	}

	void clear() {
		memset(pixels, colors().clear.as_uint(), width * height * sizeof(COLORREF));
	}

	void render_glyph(const Character& character, const FontGlyph& glyph) {
		int in = (glyph.y) * font_width + (glyph.x);
		int out = (character.row * spacing_line + glyph.y_off) * width + (character.col * spacing_character + glyph.x_off);
		auto color = character.color;
		const bool force_opaque = glyph.id == Glyph::BLOCK;
		for (unsigned j = 0; j < glyph.h; ++j) {
			for (unsigned i = 0; i < glyph.w; ++i) {
				if (out + i < width * height) {
					((Color&)pixels[out + i]).blend(color.set_alpha(force_opaque ? 255 : font_pixels[in + i]));
				}
			}
			in += font_width;
			out += width;
		}
	}

	void render(const Characters& characters) {
		for (auto& character : characters) {
			const auto* glyph = find_glyph(character.index);
			if (glyph == nullptr)
				glyph = find_glyph(Glyph::UNKNOWN);
			if (glyph)
				render_glyph(character, *glyph);
		}
	}

	void blit() {
		const HBITMAP map = CreateBitmap(width, height, 1, 32, (void*)pixels);
		const HDC hdc = GetDC(hwnd);
		const HDC src = CreateCompatibleDC(GetDC(hwnd));
		SelectObject(src, map);
		BitBlt(hdc, 0, 0, width, height, src, 0, 0, SRCCOPY);
		DeleteDC(src);
		DeleteObject(map);
	}

public:
	Window(HINSTANCE hinstance, WNDPROC proc, void* data, int nshow)
		: hwnd(create(hinstance, proc, data)) {
		set_dark(hwnd);
		set_title(hwnd);
		show(hwnd, nshow);
	}

	~Window() {
		destroy(hwnd);
		reset();
	}

	void resize(unsigned w, unsigned h) {
		width = w;
		height = h;
		reset();
		alloc();
	}

	void redraw(const Characters& characters) {
		clear();
		render(characters);
		blit();
	}

	unsigned get_col_count() const { return (unsigned)((float)width / (float)spacing_character - 0.5f); }
	unsigned get_row_count() const { return (unsigned)((float)height / (float)spacing_line - 0.5f); }
};

class Switcher {
	std::vector<Buffer> buffers;
	size_t active = 0;

	std::string url;
	std::string clipboard;

	unsigned col_count = 0;
	unsigned row_count = 0;

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
		if (!filename.empty()) {
			if (auto index = find_buffer(filename); index != (size_t)-1) {
				active = index;
			}
			else {
				buffers.emplace_back(filename);
				active = buffers.size() - 1;
				current().init(load(filename));
			}
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
		const auto filename = std::string(current().get_filename());
		if (active != 0) { // Don't close scratch.
			buffers.erase(buffers.begin() + active);
			active = (active >= buffers.size() ? active - 1 : active) % buffers.size();
		}
	}

	void process_space_e() {
		open("open");
		current().init(list());
	}

	void process_space_f() {
		const auto seed = current().get_word();
		open(std::string("find ") + seed);
		current().init(find(seed));
		current().set_highlight(seed);
	}

	void process_space(bool& quit, unsigned key) {
		if (key == 'q') { quit = true; }
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
		open("scratch");
	}

	void resize(unsigned w, unsigned h) {
		col_count = w;
		row_count = h - 1; // Remove 1 for tabs bar.
	}

	void process(bool space_down, bool& quit, unsigned key) {
		if (space_down && current().is_normal()) { process_space(quit, key); }
		else { process_normal(key); }
	}

	Characters cull() {
		Characters characters;
		push_tabs(characters);
		current().set_line_count(current().cull(characters, col_count, row_count));
		return characters;
	}
};

class Application {
	Switcher switcher;
	Window window;

	bool minimized = false;
	bool dirty = true;
	bool space_down = false;
	bool quit = false;

	void set_minimized(bool b) { minimized = b; }
	void set_dirty(bool b) { dirty = b; }
	void set_space_down(bool b) { space_down = b; }

	void resize(unsigned w, unsigned h) {
		if (!minimized) {
			window.resize(w, h);
			switcher.resize(window.get_col_count(), window.get_row_count());
		}
	}

	void redraw() {
		if (!minimized && dirty) {
			window.redraw(switcher.cull());
		}
		else {
			Sleep(1); // Avoid busy loop when minimized.
		}
		dirty = false;
	}

	void process(unsigned key) {
		switcher.process(space_down, quit, key);
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
		: window(hinstance, proc, this, nshow) {
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

