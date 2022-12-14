#pragma once

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
	#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

SmallString get_error_string() {
	const DWORD error = GetLastError();
	SmallString res = SmallString("(error ") + SmallString((unsigned)error);
	char* message = NULL;
	if (FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&message, 0, NULL)) {
		res += SmallString(": ") + message;
		LocalFree(message);
	}
	res += ")";
	return res;
}

HWND create_window(WNDPROC proc, HINSTANCE hInstance, void* data, unsigned width, unsigned height) {
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

	RECT wr = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
	AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);

	const auto hWnd = CreateWindowEx(0, name, name, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, wr.right - wr.left, wr.bottom - wr.top,
		nullptr, nullptr, hInstance, data);

	BOOL value = TRUE;
	DwmSetWindowAttribute(hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &value, sizeof(value));

	return hWnd;
}

void destroy_window(HWND hWnd) {
	DestroyWindow(hWnd);
}

void resize_window(HWND hWnd, int dw, int dh) {
	RECT rect;
	if (GetWindowRect(hWnd, &rect)) {
		const auto w = rect.right - rect.left;
		const auto h = rect.bottom - rect.top;
		SetWindowPos(hWnd, 0, 0, 0, w + dw, h + dh, SWP_NOMOVE | SWP_NOOWNERZORDER);
	}
}


class Timer {
	LARGE_INTEGER frequency;
	uint64_t start = 0;

	uint64_t now() const {
		LARGE_INTEGER counter;
		QueryPerformanceCounter(&counter);
		return 1000000 * counter.QuadPart / frequency.QuadPart;
	}

	float duration() const {
		return (float)((double)(now() - start) * 0.001);
	}

public:
	Timer() {
		QueryPerformanceFrequency(&frequency);
		start = now();
	}

	SmallString us() const {
		return SmallString((unsigned)(duration() * 1000.0f)) + "us";
	}
};

template <typename F>
void map(const PathString& filename, F func) {
	if (const auto file = CreateFileA(filename.data(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, nullptr); file != INVALID_HANDLE_VALUE) {
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

bool write(const PathString& filename, const HugeString& text) {
	bool res = false;
	if (const auto file = CreateFileA(filename.data(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr); file != INVALID_HANDLE_VALUE) {
		res = WriteFile(file, text.data(), (DWORD)text.size(), nullptr, nullptr);
		CloseHandle(file);
	}
	return res;
}

template<typename F>
void process_files(const char* path, F func) {
	const auto filter = PathString(path) + "/*";
	WIN32_FIND_DATA find_data;
	HANDLE handle = FindFirstFile(filter.c_str(), &find_data);
	do {
		if (handle != INVALID_HANDLE_VALUE) {
			if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) > 0) {
				if (find_data.cFileName[0] != '.') {
					const auto dir = PathString(path) + "/" + find_data.cFileName;
					process_files(dir.c_str(), func);
				}
			}
			else if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
				const auto file = PathString(path) + "/" + find_data.cFileName;
				func(file);
			}
		}
	} while (FindNextFile(handle, &find_data) != 0);
	FindClose(handle);
}

bool is_secure(const PathString& url) {
	if (url.starts_with("https://"))
		return true;
	return false;
}

PathString extract_domain(const PathString& url) {
	const auto prefix_pos = url.find("//");
	const auto begin = prefix_pos != PathString::npos ? prefix_pos + 2 : 0;
	const auto end = url.find("/", begin);
	return url.substr(begin, end - begin);
}

PathString extract_object(const PathString& url) {
	const auto prefix_pos = url.find("//");
	const auto begin = prefix_pos != PathString::npos ? prefix_pos + 2 : 0;
	const auto end = url.find("/", begin);
	return url.substr(end);
}

HugeString request(const PathString& url) {
	HugeString contents;
	const auto agent = SmallString("Vin/") + SmallString(version_major) + "." + SmallString(version_minor);
	const bool secure = is_secure(url);
	const auto domain = extract_domain(url);
	const auto object = extract_object(url);
	if (HINTERNET session = InternetOpenA(agent.data(), INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0)) {
		if (HINTERNET connection = InternetConnectA(session, domain.data(), secure ? INTERNET_DEFAULT_HTTPS_PORT : 0, "", "", INTERNET_SERVICE_HTTP, 0, 0)) {
			if (HINTERNET request = HttpOpenRequestA(connection, "GET", object.data(), NULL, NULL, NULL, secure ? INTERNET_FLAG_SECURE : 0, 0)) {
				if (HttpSendRequestA(request, NULL, 0, 0, 0)) {
					DWORD read = 0;
					do {
						HugeString partial;
						partial.reserve(HugeString::max_size());
						if (InternetReadFile(request, partial.data(), (DWORD)partial.size(), &read)) {
							partial.resize(read);
							contents += partial;
						}
						else { contents = url + " Failed to read file " + get_error_string(); break; }
					} while (read > 0);
				}
				else { contents = url + " Failed to send request " + get_error_string(); }
				InternetCloseHandle(request);
			}
			else { contents = url + " Failed to open request " + get_error_string(); }
			InternetCloseHandle(connection);
		}
		else { contents = url + " Failed to establish connection " + get_error_string(); }
		InternetCloseHandle(session);
	}
	else { contents = url + " Failed to open session " + get_error_string(); }
	return contents;
}
