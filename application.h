#pragma once

const unsigned version_major = 1;
const unsigned version_minor = 0;

enum class Menu {
	space,
	normal,
};

bool ignore_file(const std::string_view path) {
	if (path.ends_with(".db")) return true;
	if (path.ends_with(".exe")) return true;
	if (path.ends_with(".ilk")) return true;
	if (path.ends_with(".iobj")) return true;
	if (path.ends_with(".ipdb")) return true;
	if (path.ends_with(".obj")) return true;
	if (path.ends_with(".pch")) return true;
	if (path.ends_with(".pdb")) return true;
	return false;
}

class Index {
	std::vector<std::string> paths;

public:
	std::string populate() {
		const Timer timer;
		paths.clear();
		process_files(".", [&](const auto& path) {
			if (!ignore_file(path))
				paths.push_back(path);
		});
		return std::string("populate (") + 
			std::to_string(paths.size()) + " paths) in " + timer.us();
	}

	std::string generate() {
		std::string list;
		for (const auto& path : paths) {
			list += path + "\n";
		}
		return list;
	}
};

class Database {
	struct File {
		std::string name;
		std::vector<char> contents;
	};

	struct Location {
		size_t file_index = 0;
		size_t position = 0;
	};

	std::vector<File> files;
	std::vector<Location> locations;

	void add(const std::string& filename) {
		map(filename, [&](const char* mem, size_t size) {
			if (!ignore_file(filename)) {
				std::vector<char> contents(size);
				memcpy(contents.data(), mem, contents.size());
				files.emplace_back(filename, std::move(contents));
			}
		});
	}

	void scan(unsigned file_index, const std::vector<char>& contents, const std::string_view pattern) {
		for (size_t i = 0; i < contents.size(); ++i) {
			if (contents[i] == pattern[0]) {
				if (strncmp(&contents[i], pattern.data(), pattern.size()) == 0) {
					locations.emplace_back(file_index, i);
				}
			}
		}
	}

	static std::string_view cut_line(const std::string_view text, size_t pos) {
		Line line(text, pos);
		return line.to_string(text);
	}

	std::string make_line(const std::string_view text) {
		if (text.size() > 0)
			if (text[text.size() - 1] == '\n')
				return std::string(text);
		return std::string(text) + "\n";
	}

public:
	std::string populate() {
		const Timer timer;
		files.clear();
		process_files(".", [&](const auto& path) {
			add(path);
		});
		return std::string("populate (") + 
			std::to_string(files.size()) + " files) in " + timer.us();
	}

	std::string generate(const std::string_view pattern) {
		const Timer timer;
		if (!pattern.empty() && pattern.size() > 2) {
			for (unsigned i = 0; i < files.size(); i++) {
				scan(i, files[i].contents, pattern);
			}
		}
		std::string list;
		for (const auto& location : locations) {
			const auto& file = files[location.file_index];
			const auto pos = location.position;
			const auto start = pos > (size_t)100 ? pos - (size_t)100 : (size_t)0;
			const auto count = std::min((size_t)200, file.contents.size() - start);
			const auto context = std::string(&file.contents[start], count);
			list += file.name + "(" + std::to_string(pos) + ") " + make_line(cut_line(context, pos - start));
		}
		return list;
	}
};

class Application {
	Window window;
	Device device;
	Switcher switcher;

	Menu menu = Menu::normal;

	std::string url;
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

	Characters cull() {
		const auto vp = device.viewport();
		Characters characters;
		switch (menu) {
		case Menu::space: // pass-through.
		case Menu::normal: switcher.cull(characters, vp.w, vp.h); break;
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
			device.redraw(cull());
			window.set_title(status().data());
		}
		else {
			Sleep(1); // Avoid busy loop when minimized.
		}
		dirty = false;
	}

	void process_space_e() {
		Index index;
		notify(index.populate());
		switcher.load("open");
		switcher.current().init(index.generate());
	}

	void process_space_f() {
		Database database;
		notify(database.populate());
		const auto seed = switcher.current().get_word();
		switcher.load(std::string("find ") + seed);
		switcher.current().init(database.generate(seed));
		switcher.current().set_highlight(seed);
	}

	void process_space(unsigned key, unsigned row_count) {
		if (key == 'q') { quit = true; }
		else if (key == 'w') { notify(switcher.close()); }
		else if (key == 'r') { notify(switcher.reload()); }
		else if (key == 's') { notify(switcher.save()); }
		else if (key == 'e') { process_space_e(); }
		else if (key == 'f') { process_space_f(); }
		else if (key == 'i') { switcher.current().state().window_down(row_count - 1); }
		else if (key == 'o') { switcher.current().state().window_up(row_count - 1); }
		else if (key >= '0' && key <= '9') { switcher.select_index(key - '0'); }
		else if (key == 'h') { switcher.select_previous(); }
		else if (key == 'l') { switcher.select_next(); }
		else if (key == 'n') { switcher.current().clear_highlight(); }
		else if (key == 'm') { window.show(maximized ? SW_SHOWDEFAULT : SW_SHOWMAXIMIZED); }
	}

	void process_normal(unsigned key, unsigned col_count, unsigned row_count) {
		switcher.current().process(clipboard, url, key, col_count, row_count - 1);
		if (!url.empty()) {
			notify(switcher.load(extract_filename(url)));
			switcher.current().jump(extract_location(url), row_count);
			url.clear();
		}
	}

	void process(unsigned key) {
		const auto vp = device.viewport();
		switch (menu) {
		case Menu::space: process_space(key, vp.h); break;
		case Menu::normal: process_normal(key, vp.w, vp.h); break;
		}
		switcher.current().state().cursor_clamp(vp.h - 1);
	}

	void update(bool space_down) {
		switch (menu) {
		case Menu::space: if (!space_down) { menu = Menu::normal; } break;
		case Menu::normal: if (space_down && switcher.current().is_normal()) { menu = Menu::space; } break;
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
		: window(hInstance, proc, this)
		, device(hInstance, window.get(), window.get_dpi()) {
		window.set_size(7 * window.get_dpi(), 5 * window.get_dpi());
		window.show(nCmdShow);
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

