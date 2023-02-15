#pragma once

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
	#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

std::string get_error_string() {
	const DWORD error = GetLastError();
	std::string res = std::string("(error ") + std::to_string((unsigned)error);
	char* message = NULL;
	if (FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&message, 0, NULL)) {
		res += std::string(": ") + message;
		LocalFree(message);
	}
	res += ")";
	return res;
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
	
	void resize(int dw, int dh) {
		RECT rect;
		if (GetWindowRect(hWnd, &rect)) {
			const auto w = rect.right - rect.left;
			const auto h = rect.bottom - rect.top;
			SetWindowPos(hWnd, 0, 0, 0, w + dw, h + dh, SWP_NOMOVE | SWP_NOOWNERZORDER);
		}
	}

	unsigned get_dpi() {
		const auto dpi = GetDpiForWindow(hWnd);
		return dpi == 0 ? 96 : dpi;
	}
	
	HWND get() const { return hWnd; }
};

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

	std::string us() const {
		return std::to_string((unsigned)(duration() * 1000.0f)) + "us";
	}
};

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
