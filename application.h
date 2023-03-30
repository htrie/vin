#pragma once

const unsigned version_major = 1;
const unsigned version_minor = 1;

static bool ignore_file(const std::string_view path) {
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
	const auto len = mbstowcs(nullptr, url.data(), 0);
	verify(len == url.length());
	std::wstring res;
    res.resize(len);
	const auto converted = mbstowcs(res.data(), url.data(), len + 1);
	verify(converted == len);
	return res;
}

std::string download(const HINTERNET request) {
	std::string contents;
	DWORD size = 0;
	do {
		if (WinHttpQueryDataAvailable(request, &size)) {
			std::vector<char> partial(size);
			DWORD downloaded = 0;
			if (WinHttpReadData(request, (LPVOID)partial.data(), (DWORD)partial.size(), &downloaded)) {
				verify(downloaded == size);
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
static void process_files(const std::string& path, F func) {
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
static void map(const std::string_view filename, F func) {
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

static bool write(const std::string_view filename, const std::string_view text) {
	bool res = false;
	if (const auto file = CreateFileA(filename.data(), GENERIC_WRITE, 0, nullptr,
		CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr); file != INVALID_HANDLE_VALUE) {
		res = WriteFile(file, text.data(), (DWORD)text.size(), nullptr, nullptr);
		CloseHandle(file);
	}
	return res;
}

static std::string list() {
	std::string list;
	process_files(".", [&](const auto& path) {
		if (!ignore_file(path))
			list += path + "\n";
		});
	return list;
}

static std::string_view cut_line(const std::string_view text, size_t pos) {
	Line line(text, pos);
	return line.to_string(text);
}

static std::string make_line(const std::string_view text) {
	if (text.size() > 0)
		if (text[text.size() - 1] == '\n')
			return std::string(text);
	return std::string(text) + "\n";
}

static std::string make_entry(size_t pos, const char* mem, size_t size) {
	const auto start = pos > (size_t)100 ? pos - (size_t)100 : (size_t)0;
	const auto count = std::min((size_t)200, size - start);
	const auto context = std::string(&mem[start], count);
	return "(" + std::to_string(pos) + ") " + make_line(cut_line(context, pos - start));
}

static std::string scan(const std::string_view path, const std::string_view pattern) {
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

static std::string find(const std::string_view pattern) {
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

static std::string prettify_html(const std::string_view contents) {
	std::string out = std::string(contents);
	// TODO
	return out;
}

static std::string load(const std::string_view filename) {
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

class Window {
	HWND hWnd = nullptr;

public:
	Window(HINSTANCE hInstance, WNDPROC proc, void* data) {
		const char* name = "vin";
		const auto hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));

		WNDCLASSEX win_class;
		win_class.cbSize = sizeof(WNDCLASSEX);
		win_class.style = CS_HREDRAW | CS_VREDRAW;
		win_class.lpfnWndProc = proc;
		win_class.cbClsExtra = 0;
		win_class.cbWndExtra = 0;
		win_class.hInstance = hInstance;
		win_class.hIcon = hIcon;
		win_class.hCursor = LoadCursor(nullptr, IDC_ARROW);
		win_class.hbrBackground = CreateSolidBrush(0);
		win_class.lpszMenuName = nullptr;
		win_class.lpszClassName = name;
		win_class.hIconSm = hIcon;

		RegisterClassEx(&win_class);

		SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE);

		RECT wr = { 0, 0, 1, 1 };
		AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);

		hWnd = CreateWindowEx(0, name, name, WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT, CW_USEDEFAULT, wr.right - wr.left, wr.bottom - wr.top,
			nullptr, nullptr, hInstance, data);

	#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
		#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
	#endif
		BOOL value = TRUE;
		DwmSetWindowAttribute(hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &value, sizeof(value));
	}

	~Window() {
		DestroyWindow(hWnd);
	}

	void show(int nCmdShow) {
		ShowWindow(hWnd, nCmdShow);
	}

	void set_title(const std::string_view text) {
		SetWindowTextA(hWnd, text.data());
	}

	void set_size(int w, int h) {
		SetWindowPos(hWnd, 0, 0, 0, w, h, SWP_NOMOVE | SWP_NOOWNERZORDER);
	}

	unsigned get_dpi() {
		const auto dpi = GetDpiForWindow(hWnd);
		return dpi == 0 ? 96 : dpi;
	}

	HWND get() const { return hWnd; }
};

struct Uniforms {
	static const unsigned CharacterMaxCount = 8192;

	Matrix view_proj;
	Vec4 scale_and_pos[CharacterMaxCount];
	Vec4 color[CharacterMaxCount];
	Vec4 uv_origin_and_size[CharacterMaxCount];
};

struct Viewport {
	unsigned w = 0;
	unsigned h = 0;
};

class Device {
	vk::UniqueInstance instance;
	vk::PhysicalDevice gpu;
	vk::UniqueSurfaceKHR surface;
	vk::SurfaceFormatKHR surface_format;
	vk::UniqueDevice device;
	vk::Queue queue;

	vk::UniqueCommandPool cmd_pool;
	vk::UniqueDescriptorPool desc_pool;
	vk::UniqueRenderPass render_pass;
	vk::UniqueDescriptorSetLayout desc_layout;
	vk::UniqueDescriptorSet descriptor_set;
	vk::UniquePipelineLayout pipeline_layout;
	vk::UniquePipeline pipeline;

	vk::UniqueBuffer uniform_buffer;
	vk::UniqueDeviceMemory uniform_memory;
	void* uniform_ptr = nullptr;

	vk::UniqueFence fence;
	vk::UniqueSemaphore image_acquired_semaphore;
	vk::UniqueSemaphore draw_complete_semaphore;

	vk::UniqueSwapchainKHR swapchain;
	std::vector<vk::UniqueImageView> image_views;
	std::vector<vk::UniqueFramebuffer> framebuffers;
	std::vector<vk::UniqueCommandBuffer> cmds;

	vk::UniqueSampler sampler;
	vk::UniqueImage image;
	vk::UniqueDeviceMemory image_memory;
	vk::UniqueImageView image_view;

	float spacing_character = 9.0f;
	float spacing_line = 20.0f;

	unsigned width = 0;
	unsigned height = 0;

	void upload(const vk::CommandBuffer& cmd_buf) {
		sampler = create_sampler(device.get());
		image = create_image(gpu, device.get(), font_width, font_height);
		image_memory = create_image_memory(gpu, device.get(), image.get());
		copy_image_data(device.get(), image.get(), image_memory.get(), font_pixels, sizeof(font_pixels), font_width);
		image_view = create_image_view(device.get(), image.get(), vk::Format::eR8Unorm);
		add_image_barrier(cmd_buf, image.get());
		descriptor_set = create_descriptor_set(device.get(), desc_pool.get(), desc_layout.get());
		update_descriptor_set(device.get(), descriptor_set.get(), uniform_buffer.get(), sizeof(Uniforms), sampler.get(), image_view.get());
	}

	const FontGlyph* find_glyph(uint16_t id) {
		for (auto& glyph : font_glyphs) {
			if (glyph.id == id)
				return &glyph;
		}
		return nullptr;
	}

	Matrix compute_viewproj() {
		const auto view = Matrix::look_at({ 0.0f, 0.0f, 10.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f });
		const auto proj = Matrix::ortho(0.0f, (float)width, 0.0f, (float)height, -100.0f, 100.0f);
		return view * proj;
	}

	unsigned accumulate_uniforms(const Characters& characters) {
		auto& uniforms = *(Uniforms*)uniform_ptr;
		uniforms.view_proj = compute_viewproj();

		unsigned index = 0;

		const auto add = [&](const auto& character, const auto& glyph) {
			uniforms.scale_and_pos[index] = {
				glyph.w * font_width * character.scale_x,
				glyph.h * font_height * character.scale_y, 
				character.col * spacing_character + glyph.x_off * font_width + character.offset_x,
				character.row * spacing_line + glyph.y_off * font_height + character.offset_y };
			uniforms.color[index] = character.color.rgba();
			uniforms.uv_origin_and_size[index] = { glyph.x, glyph.y, glyph.w, glyph.h };
			index++;
		};

		for (auto& character : characters) {
			const auto* glyph = find_glyph(character.index);
			if (glyph == nullptr)
				glyph = find_glyph(Glyph::UNKNOWN);
			if (glyph && index < Uniforms::CharacterMaxCount)
				add(character, *glyph);
		}

		return index;
	}

public:
	Device(HINSTANCE hInstance, HWND hWnd) {
		instance = create_instance();
		gpu = pick_gpu(instance.get());
		surface = create_surface(instance.get(), hInstance, hWnd);
		surface_format = select_format(gpu, surface.get());
		auto family_index = find_queue_family(gpu, surface.get());
		device = create_device(gpu, family_index);
		queue = fetch_queue(device.get(), family_index);

		cmd_pool = create_command_pool(device.get(), family_index);
		desc_pool = create_descriptor_pool(device.get());
		desc_layout = create_descriptor_layout(device.get());
		pipeline_layout = create_pipeline_layout(device.get(), desc_layout.get());
		render_pass = create_render_pass(device.get(), surface_format);
		pipeline = create_pipeline(device.get(), pipeline_layout.get(), render_pass.get());

		uniform_buffer = create_uniform_buffer(device.get(), sizeof(Uniforms));
		uniform_memory = create_uniform_memory(gpu, device.get(), uniform_buffer.get());
		bind_memory(device.get(), uniform_buffer.get(), uniform_memory.get());
		uniform_ptr = map_memory(device.get(), uniform_memory.get());

		fence = create_fence(device.get());
		image_acquired_semaphore = create_semaphore(device.get());
		draw_complete_semaphore = create_semaphore(device.get());
	}

	~Device() {
		wait_idle(device.get());
	}

	Viewport resize(unsigned w, unsigned h) {
		width = w;
		height = h;

		if (device) {
			wait_idle(device.get());

			image_views.clear();
			framebuffers.clear();
			cmds.clear();

			swapchain = create_swapchain(gpu, device.get(), surface.get(), surface_format, swapchain.get(), width, height, 3);
			for (auto& swapchain_image : get_swapchain_images(device.get(), swapchain.get())) {
				image_views.emplace_back(create_image_view(device.get(), swapchain_image, surface_format.format));
				framebuffers.emplace_back(create_framebuffer(device.get(), render_pass.get(), image_views.back().get(), width, height));
				cmds.emplace_back(create_command_buffer(device.get(), cmd_pool.get()));
			}
		}

		return {
			(unsigned)((float)width / spacing_character - 0.5f),
			(unsigned)((float)height / spacing_line - 0.5f)
		};
	}

	void redraw(const Characters& characters) {
		wait(device.get(), fence.get());
		const auto frame_index = acquire(device.get(), swapchain.get(), image_acquired_semaphore.get());
		const auto& cmd = cmds[frame_index].get();

		begin(cmd);
		if (!image) upload(cmd);
		begin_pass(cmd, render_pass.get(), framebuffers[frame_index].get(), colors().clear, width, height);

		set_viewport(cmd, (float)width, (float)height);
		set_scissor(cmd, width, height);

		bind_pipeline(cmd, pipeline.get());
		bind_descriptor_set(cmd, pipeline_layout.get(), descriptor_set.get());

		draw(cmd, 6, accumulate_uniforms(characters));

		end_pass(cmd);
		end(cmd);

		submit(queue, image_acquired_semaphore.get(), draw_complete_semaphore.get(), cmd, fence.get());
		present(swapchain.get(), queue, draw_complete_semaphore.get(), frame_index);
	}
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

	void process_space(bool& quit, bool& toggle, unsigned key) {
		if (key == 'q') { quit = true; }
		else if (key == 'm') { toggle = true; }
		else if (key == 'w') { close(); }
		else if (key == 'r') { reload(); }
		else if (key == 's') { save(); }
		else if (key == 'e') { process_space_e(); }
		else if (key == 'f') { process_space_f(); }
		else if (key == 'i') { current().window_down(row_count); }
		else if (key == 'o') { current().window_up(row_count); }
		else if (key >= '0' && key <= '9') { select_index(key - '0'); }
		else if (key == 'h') { select_previous(); }
		else if (key == 'l') { select_next(); }
		else if (key == 'n') { current().clear_highlight(); }
	}

	void process_normal(unsigned key) {
		current().process(clipboard, url, key, col_count, row_count);
		if (!url.empty()) {
			open(extract_filename(url));
			current().jump(extract_location(url), row_count);
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
			push_line(characters, i == active ? colors().bar_text : colors().bar, (float)row, col, col + (int)tabs[i].size());
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

	void process(bool space_down, bool& quit, bool& toggle, unsigned key) {
		if (space_down && current().is_normal()) { process_space(quit, toggle, key); }
		else { process_normal(key); }
	}

	Characters cull() const {
		Characters characters;
		push_tabs(characters);
		current().cull(characters, col_count, row_count);
		return characters;
	}
};

class Application {
	Window window;
	Device device;
	Switcher switcher;

	bool maximized = false;
	bool minimized = false;
	bool dirty = false;
	bool space_down = false;
	bool quit = false;

	void set_maximized(bool b) { maximized = b; }
	void set_minimized(bool b) { minimized = b; }
	void set_dirty(bool b) { dirty = b; }
	void set_space_down(bool b) { space_down = b; }

	void resize(unsigned w, unsigned h) {
		if (!minimized) {
			const auto viewport = device.resize(w, h);
			switcher.resize(viewport.w, viewport.h);
		}
	}

	void redraw() {
		if (!minimized && dirty) {
			device.redraw(switcher.cull());
		}
		else {
			Sleep(1); // Avoid busy loop when minimized.
		}
		dirty = false;
	}

	void process(unsigned key) {
		bool toggle = false;
		switcher.process(space_down, quit, toggle, key);
		if (toggle) {
			window.show(maximized ? SW_SHOWDEFAULT : SW_SHOWMAXIMIZED);
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
					app->set_space_down(true);
				}
			}
			break;
		}
		case WM_KEYUP: {
			if (auto* app = reinterpret_cast<Application*>(GetWindowLongPtr(hWnd, GWLP_USERDATA))) {
				if (wParam == VK_SPACE) {
					app->set_dirty(true);
					app->set_space_down(false);
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
		, device(hInstance, window.get()) {
		window.set_title(std::string("Vin ") + std::to_string(version_major) + "." + std::to_string(version_minor));
		window.set_size(7 * window.get_dpi(), 5 * window.get_dpi());
		window.show(nCmdShow);
	}

	void run() {
		MSG msg = {};
		while (!quit && GetMessage(&msg, nullptr, 0, 0)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
};

