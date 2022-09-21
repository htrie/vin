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
#include <thread>
#include <string>
#include <vulkan/vulkan.hpp>

#include "linmath.h"
#include "font.h"
#include "win.h"
#include "vk.h"

class App {
    Device device;

    std::array<float, 4> clear_color = { 0.2f, 0.2f, 0.2f, 1.0f };

    std::vector<std::string> text = {
        "abcdefghijklmnopqrstuvwxyz",
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ",
        "`1234567890-=[]\\;',./",
        "~!@#$%^&*()_+{}|:\"<>?",
        ""};

    bool minimized = false;
    bool dirty = true;

    void set_minimized(bool b) { minimized = b; } 
    void set_dirty(bool b) { dirty = b; } 

    void resize(unsigned w, unsigned h) {
        if (!minimized) {
            device.resize(w, h);
        }
    }

    void redraw() {
        if (!minimized && dirty) {
            dirty = false;
            device.redraw(clear_color, text);
        }
        else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    void process(WPARAM key) {
        // [TODO] Handle backspace.
        // [TODO] Use only one line of text.
        // [TODO] Handle enter.
        // [TODO] Add text.h.
        // [TODO] Handle space+Q to quit.
        // [TODO] Display block cursor.
        text.back() += (char)key;
    }

    static LRESULT CALLBACK proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        switch (uMsg) {
            case WM_CREATE: {
                LPCREATESTRUCT create_struct = reinterpret_cast<LPCREATESTRUCT>(lParam);
                SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create_struct->lpCreateParams));
                break;
            }
            case WM_MOVE:
            case WM_SETFOCUS: {
                if (auto* app = reinterpret_cast<App*>(GetWindowLongPtr(hWnd, GWLP_USERDATA)))
                    app->set_dirty(true);
                break;
            }
            case WM_DESTROY:
            case WM_CLOSE: {
                PostQuitMessage(0);
                break;
            }
            case WM_GETMINMAXINFO: {
                // Window client area size must be at least 1 pixel high, to prevent crash.
                ((MINMAXINFO*)lParam)->ptMinTrackSize = { GetSystemMetrics(SM_CXMINTRACK), GetSystemMetrics(SM_CYMINTRACK) + 1 };
                break;
            }
            case WM_PAINT: {
                if (auto* app = reinterpret_cast<App*>(GetWindowLongPtr(hWnd, GWLP_USERDATA)))
                    app->redraw();
                break;
            }
            case WM_SIZE: {
                if (auto* app = reinterpret_cast<App*>(GetWindowLongPtr(hWnd, GWLP_USERDATA))) {
                    app->set_minimized(wParam == SIZE_MINIMIZED);
                    const unsigned width = lParam & 0xffff;
                    const unsigned height = (lParam & 0xffff0000) >> 16;
                    app->resize(width, height);
                    app->set_dirty(true);
                }
                break;
            }
            case WM_CHAR: {
                if (auto* app = reinterpret_cast<App*>(GetWindowLongPtr(hWnd, GWLP_USERDATA))) {
                    app->process(wParam);
                    app->set_dirty(true);
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
    App(HINSTANCE hInstance, int nCmdShow)
        : device(proc, hInstance, nCmdShow, 640, 480) {
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

