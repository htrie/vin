#define NOMINMAX
#define VC_EXTRALEAN
#define WIN32_LEAN_AND_MEAN
#define _HAS_EXCEPTIONS 0
#define VULKAN_HPP_NO_EXCEPTIONS

#include <math.h>
#include <stdlib.h>
#include <string>
#include <fstream>
#include <thread>
#include <vulkan/vulkan.hpp>
#include <Windows.h>

#include "resource.h"
#include "util.h"
#include "matrix.h"
#include "font.h"
#include "text.h"
#include "win.h"
#include "vk.h"

class App {
    Device device;
    Buffer buffer;
    Timer timer;

    Color clear_color = Color::rgba(1, 22, 39, 255);

    float cull_time = 0.0f;
    float redraw_time = 0.0f;
    float process_time = 0.0f;

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
            const auto viewport = device.viewport();
            const auto start = timer.now();
            const auto text = buffer.cull(process_time, cull_time, redraw_time, viewport.w, viewport.h);
            cull_time = timer.duration(start);
            {
                const auto start = timer.now();
                device.redraw(clear_color, text);
                redraw_time = timer.duration(start);
            }
        }
        else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    void process(WPARAM key) {
        const auto start = timer.now();
        if (buffer.process(key))
            PostQuitMessage(0);
        process_time = timer.duration(start);
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
        : device(proc, hInstance, nCmdShow, 600, 400) {
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

