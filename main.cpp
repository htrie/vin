#define NOMINMAX
#define VC_EXTRALEAN
#define WIN32_LEAN_AND_MEAN
#define _HAS_EXCEPTIONS 0
#define VULKAN_HPP_NO_EXCEPTIONS

#ifndef NDEBUG
#define VERIFY(expr) \
    if (!(expr)) { \
        MessageBox(nullptr, #expr, "Error", MB_OK); \
        abort(); \
    }
#else
#define VERIFY(x) ((void)(x))
#endif

#define ERR_EXIT(err_msg, err_class) \
    do { \
        MessageBox(nullptr, err_msg, err_class, MB_OK); \
        exit(1); \
    } while (0)

#include <Windows.h>
#include <math.h>
#include <stdlib.h>
#include <vulkan/vulkan.hpp>

#include "linmath.h"
#include "win.h"
#include "vk.h"

class App {
    Device device;

    bool minimized = false;

    void set_minimized(bool b) {
        minimized = b;
    }

    void resize(unsigned w, unsigned h) {
        if (!minimized)
            device.resize(w, h);
    }

    void redraw() {
        if (!minimized)
            device.redraw();
    }

    static LRESULT CALLBACK proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        switch (uMsg) {
        case WM_CREATE: {
            LPCREATESTRUCT create_struct = reinterpret_cast<LPCREATESTRUCT>(lParam);
            SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create_struct->lpCreateParams));
            return 0;
        }
        case WM_DESTROY:
        case WM_CLOSE: {
            PostQuitMessage(0);
            return 0;
        }
        case WM_GETMINMAXINFO: {
            // Window client area size must be at least 1 pixel high, to prevent crash.
            const POINT minsize = { GetSystemMetrics(SM_CXMINTRACK), GetSystemMetrics(SM_CYMINTRACK) + 1 };
            ((MINMAXINFO*)lParam)->ptMinTrackSize = minsize;
            return 0;
        }
        case WM_PAINT: {
            if (auto* app = reinterpret_cast<App*>(GetWindowLongPtr(hWnd, GWLP_USERDATA)))
                app->redraw();
            return 0;
        }
        case WM_SIZE: {
            if (auto* app = reinterpret_cast<App*>(GetWindowLongPtr(hWnd, GWLP_USERDATA))) {
                app->set_minimized(wParam == SIZE_MINIMIZED);
                const unsigned width = lParam & 0xffff;
                const unsigned height = (lParam & 0xffff0000) >> 16;
                app->resize(width, height);
            }
            return 0;
        }
        }
        return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }

public:
    App(HINSTANCE hInstance, int nCmdShow)
        : device(proc, hInstance, nCmdShow, 800, 600) {
    }

    void run() {
        MSG msg = {};
        while (GetMessage(&msg, nullptr, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) {
    App app(hInstance, nCmdShow);
    app.run();
    return 0;
}

