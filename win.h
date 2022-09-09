#pragma once

#include <Windows.h>

HWND create_window(WNDPROC proc, HINSTANCE hInstance, int nCmdShow, void* data, unsigned width, unsigned height) {
    const char* name = "vin";

    WNDCLASSEX win_class;
    win_class.cbSize = sizeof(WNDCLASSEX);
    win_class.style = CS_HREDRAW | CS_VREDRAW;
    win_class.lpfnWndProc = proc;
    win_class.cbClsExtra = 0;
    win_class.cbWndExtra = 0;
    win_class.hInstance = hInstance;
    win_class.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    win_class.hCursor = LoadCursor(nullptr, IDC_ARROW);
    win_class.hbrBackground = CreateSolidBrush(0);
    win_class.lpszMenuName = nullptr;
    win_class.lpszClassName = name;
    win_class.hIconSm = LoadIcon(nullptr, IDI_WINLOGO);

    if (!RegisterClassEx(&win_class)) {
        ERR_EXIT("Unexpected error trying to start the application!\n", "RegisterClass Failure");
    }

    RECT wr = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);

    auto hWnd = CreateWindowEx(0, name, name, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, wr.right - wr.left, wr.bottom - wr.top,
        nullptr, nullptr, hInstance, data);
    if (!hWnd) {
        ERR_EXIT("Cannot create a window in which to draw!\n", "CreateWindow Failure");
    }

    ShowWindow(hWnd, nCmdShow);

    return hWnd;
}

