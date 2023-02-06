#pragma once

const unsigned version_major = 1;
const unsigned version_minor = 0;

enum class Menu {
	space,
	normal,
	picker,
	switcher,
	finder,
};

class Application {
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
		return std::string("Vin ") +
			std::to_string(version_major) + "." + std::to_string(version_minor)
			+ " | " + notification.c_str();
	}

	unsigned adjust_search(unsigned row_count) {
		return std::max(unsigned(float(row_count) * 0.45f), 1u);
	}

	void cull_normal(Characters& characters, unsigned col_count, unsigned row_count) {
		switcher.current().cull(characters, col_count, row_count);
	}

	void cull_picker(Characters& characters, unsigned col_count, unsigned row_count) {
		const auto search_height = adjust_search(row_count);
		switcher.current().cull(characters, col_count, row_count - search_height);
		picker.cull(characters, col_count, search_height, row_count - search_height + 1);
	}

	void cull_switcher(Characters& characters, unsigned col_count, unsigned row_count) {
		switcher.current().cull(characters, col_count, row_count);
		switcher.cull(characters, col_count, row_count);
	}

	void cull_finder(Characters& characters, unsigned col_count, unsigned row_count) {
		const auto search_height = adjust_search(row_count);
		switcher.current().cull(characters, col_count, row_count - search_height);
		finder.cull(characters, col_count, search_height, row_count - search_height + 1);
	}

	Characters cull() {
		const auto vp = device.viewport();
		Characters characters;
		switch (menu) {
		case Menu::space: // pass-through.
		case Menu::normal: cull_normal(characters, vp.w, vp.h); break;
		case Menu::picker: cull_picker(characters, vp.w, vp.h); break;
		case Menu::switcher: cull_switcher(characters, vp.w, vp.h); break;
		case Menu::finder: cull_finder(characters, vp.w, vp.h); break;
		}
		return characters;
	}

	void resize(unsigned w, unsigned h) {
		if (!minimized) {
			device.resize(w, h);
		}
	}

	void redraw() {
		if (dirty) {
			characters = cull();
		}
		if (!minimized && dirty) {
			device.redraw(characters);
			SetWindowTextA(device.get_hwnd(), status().data());
		}
		else {
			Sleep(1); // Avoid busy loop when minimized.
		}
		dirty = false;
	}

	void process_space_e() {
		notify(index.populate());
		picker.filter(index);
		menu = Menu::picker;
	}

	void process_space_f() {
		const auto note = database.populate();
		finder.seed(switcher.current().get_word());
		notify(note + ", " + database.search(finder.get_pattern()));
		finder.filter(database);
	}

	void process_space_less(unsigned row_count) {
		finder.select_next();
		switcher.load(finder.selection());
		switcher.current().jump(finder.position(), row_count);
	}

	void process_space_greater(unsigned row_count) {
		finder.select_previous();
		switcher.load(finder.selection());
		switcher.current().jump(finder.position(), row_count);
	}

	void process_space(unsigned key, unsigned row_count) {
		if (key == 'q') { quit = true; }
		else if (key == 'w') { notify(switcher.close()); }
		else if (key == 'r') { notify(switcher.reload()); }
		else if (key == 's') { notify(switcher.save()); }
		else if (key == 'e') { process_space_e(); }
		else if (key == 'f') { process_space_f(); menu = Menu::finder; }
		else if (key == '<') { process_space_less(row_count); }
		else if (key == '>') { process_space_greater(row_count); }
		else if (key == 'l') { menu = Menu::finder; }
		else if (key == 'o') { switcher.current().state().window_up(row_count - 1); }
		else if (key == 'i') { switcher.current().state().window_down(row_count - 1); }
		else if (key == 'j') { switcher.select_next(); menu = Menu::switcher; }
		else if (key == 'k') { switcher.select_previous(); menu = Menu::switcher; }
		else if (key == 'n') { switcher.current().clear_highlight(); }
		else if (key == 'm') { show_window(device.get_hwnd(), maximized ? SW_SHOWDEFAULT : SW_SHOWMAXIMIZED); }
		else if (key == '-') { resize_window(device.get_hwnd(), 0, 40); }
		else if (key == '_') { resize_window(device.get_hwnd(), 0, -40); }
		else if (key == '=') { resize_window(device.get_hwnd(), 40, 0); }
		else if (key == '+') { resize_window(device.get_hwnd(), -40, 0); }
		else if (key == '[') { notify(spacing().decrease_char_width()); }
		else if (key == ']') { notify(spacing().increase_char_width()); }
		else if (key == '{') { notify(spacing().decrease_char_height()); }
		else if (key == '}') { notify(spacing().increase_char_height()); }
		else if (key == '(') { notify(spacing().decrease_char_zoom()); }
		else if (key == ')') { notify(spacing().increase_char_zoom()); }
		else if (key == ',') { notify(spacing().decrease_tab_size()); }
		else if (key == '.') { notify(spacing().increase_tab_size()); }
	}

	void process_normal(unsigned key, unsigned col_count, unsigned row_count) {
		switcher.current().process(clipboard, key, col_count, row_count - 1);
	}

	void process_picker(unsigned key, unsigned col_count, unsigned row_count) {
		if (key == '\r') { notify(switcher.load(picker.selection())); menu = Menu::normal; }
		else if (key == Glyph::ESCAPE) { menu = Menu::normal; }
		else { picker.process(key); picker.filter(index); }
	}

	void process_switcher(unsigned key, unsigned col_count, unsigned row_count) {
		if (key == 'j') { switcher.select_next(); }
		else if (key == 'k') { switcher.select_previous(); }
		else if (key == 'w') { notify(switcher.close()); }
		else if (key == 'r') { notify(switcher.reload()); }
		else if (key == 's') { notify(switcher.save()); }
		else if (key == 'o') { switcher.current().state().window_up(row_count - 1); }
		else if (key == 'i') { switcher.current().state().window_down(row_count - 1); }
		else { switcher.process(key, col_count, row_count); }
	}

	void process_finder_return(unsigned row_count) {
		notify(switcher.load(finder.selection()));
		switcher.current().jump(finder.position(), row_count);
		menu = Menu::normal;
	}

	void process_finder_key(unsigned key) {
		if (finder.process(key)) {
			notify(database.search(finder.get_pattern()));
		}
		finder.filter(database);
	}

	void process_finder(unsigned key, unsigned col_count, unsigned row_count) {
		if (key == '\r') { process_finder_return(row_count); }
		else if (key == Glyph::ESCAPE) { menu = Menu::normal; }
		else { process_finder_key(key); }
	}

	void process(unsigned key) {
		const auto vp = device.viewport();
		switch (menu) {
		case Menu::space: process_space(key, vp.h); break;
		case Menu::normal: process_normal(key, vp.w, vp.h); break;
		case Menu::picker: process_picker(key, vp.w, vp.h); break;
		case Menu::switcher: process_switcher(key, vp.w, vp.h); break;
		case Menu::finder: process_finder(key, vp.w, vp.h); break;
		}
		switcher.current().state().cursor_clamp(vp.h - 1);
	}

	void update(bool space_down) {
		switch (menu) {
		case Menu::space: if (!space_down) { menu = Menu::normal; } break;
		case Menu::normal: if (space_down && switcher.current().is_normal()) { menu = Menu::space; } break;
		case Menu::picker: break;
		case Menu::switcher: if (!space_down) { menu = Menu::normal; } break;
		case Menu::finder: break;
		}
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
			((MINMAXINFO*)lParam)->ptMinTrackSize = {
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
			if (auto* app = reinterpret_cast<Application*>(GetWindowLongPtr(hWnd, GWLP_USERDATA))) {
				app->set_dirty(true);
			}
			break;
		}
		case WM_SETREDRAW:
		case WM_PAINT: {
			if (auto* app = reinterpret_cast<Application*>(GetWindowLongPtr(hWnd, GWLP_USERDATA))) {
				app->redraw();
			}
			break;
		}
		case WM_SIZE: {
			if (auto* app = reinterpret_cast<Application*>(GetWindowLongPtr(hWnd, GWLP_USERDATA))) {
				const unsigned width = lParam & 0xffff;
				const unsigned height = (lParam & 0xffff0000) >> 16;
				app->set_maximized(wParam == SIZE_MAXIMIZED);
				app->set_minimized(wParam == SIZE_MINIMIZED);
				app->set_dirty(true);
				app->resize(width, height);
			}
			break;
		}
		case WM_KEYDOWN: {
			if (auto* app = reinterpret_cast<Application*>(GetWindowLongPtr(hWnd, GWLP_USERDATA))) {
				if (wParam == VK_SPACE) {
					app->set_dirty(true);
					app->update(true);
				}
			}
			break;
		}
		case WM_KEYUP: {
			if (auto* app = reinterpret_cast<Application*>(GetWindowLongPtr(hWnd, GWLP_USERDATA))) {
				if (wParam == VK_SPACE) {
					app->set_dirty(true);
					app->update(false);
				}
			}
			break;
		}
		case WM_CHAR: {
			if (auto* app = reinterpret_cast<Application*>(GetWindowLongPtr(hWnd, GWLP_USERDATA))) {
				app->set_dirty(true);
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
	Application(HINSTANCE hInstance, int nCmdShow)
		: device(proc, hInstance, 800, 600) {
		const auto dpi = get_window_dpi(device.get_hwnd());
		size_window(device.get_hwnd(), 7 * dpi, 6 * dpi);
		show_window(device.get_hwnd(), nCmdShow);
	}

	void notify(const std::string_view s) {
		notification = s;
	}

	void run() {
		MSG msg = {};
		while (!quit && GetMessage(&msg, nullptr, 0, 0)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
};

