#pragma once

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
	#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

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

	//SetWindowPos(hWnd, HWND_TOP, 0, 0, width, height, SWP_SHOWWINDOW);

	return hWnd;
}

void destroy_window(HWND hWnd) {
	DestroyWindow(hWnd);
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

HugeString request() { // TODO: Use URL parameter.
	DWORD dwSize = 0; // TODO: Local variables.
	BOOL  bResults = FALSE; // TODO: Rename variables.
	HINTERNET hConnect = NULL;
	HINTERNET hRequest = NULL;

	const auto hSession = WinHttpOpen(L"Vin", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	if (hSession) hConnect = WinHttpConnect(hSession, L"www.microsoft.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
	if (hConnect) hRequest = WinHttpOpenRequest(hConnect, L"GET", NULL, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE); 
	if (hRequest) bResults = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
	if (bResults) bResults = WinHttpReceiveResponse(hRequest, NULL);

	HugeString contents;

	if (bResults)
	{
		do
		{
			dwSize = 0;
			if (!WinHttpQueryDataAvailable(hRequest, &dwSize))
				return {}; // TODO: Don't return early (handle leaks).

			HugeString partial;
			partial.reserve(dwSize);

			DWORD dwDownloaded = 0;
			if (!WinHttpReadData(hRequest, (LPVOID)partial.data(), dwSize, &dwDownloaded))
				return {};

			contents += partial;
		}
		while (dwSize > 0);
	}

	if (hRequest) WinHttpCloseHandle(hRequest);
	if (hConnect) WinHttpCloseHandle(hConnect);
	if (hSession) WinHttpCloseHandle(hSession);

	return contents;
}
