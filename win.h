#pragma once

HWND create_window(WNDPROC proc, HINSTANCE hInstance, int nCmdShow, void* data, unsigned width, unsigned height) {
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

	if (!RegisterClassEx(&win_class)) {
		error("Unexpected error trying to start the application!\n", "RegisterClass Failure");
	}

	RECT wr = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
	AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);

	auto hWnd = CreateWindowEx(0, name, name, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, wr.right - wr.left, wr.bottom - wr.top,
		nullptr, nullptr, hInstance, data);
	if (!hWnd) {
		error("Cannot create a window in which to draw!\n", "CreateWindow Failure");
	}

	// [TODO] Dark mode.

	ShowWindow(hWnd, nCmdShow);

	return hWnd;
}

void destroy_window(HWND hWnd) {
	DestroyWindow(hWnd);
}


class Timer {
	LARGE_INTEGER frequency;

public:
	Timer() {
		QueryPerformanceFrequency(&frequency);
	}

	uint64_t now() const {
		LARGE_INTEGER counter;
		QueryPerformanceCounter(&counter);
		return 1000000 * counter.QuadPart / frequency.QuadPart;
	}

	float duration(uint64_t start) {
		return (float)((double)(now() - start) * 0.001);
	}
};

