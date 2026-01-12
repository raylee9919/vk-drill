/* ========================================================================

   (C) Copyright 2025 by Sung Woo Lee, All Rights Reserved.

   This software is provided 'as-is', without any express or implied
   warranty. In no event will the authors be held liable for any damages
   arising from the use of this software.

   ======================================================================== */




    
#include <windows.h>

#include "core.h"
#include "intrinsics.h"
#include "math.h"

#include "platform.h"

Os os;

#include "dst.h"

#include "renderer.h"
#include "win32_renderer.h"

#define STB_IMAGE_IMPLEMENTATION
#include "vendor/stb_image.h"

global b32 g_running = true;
global b32 g_show_cursor = true;

#if 1
#  define RENDERER_DLL "vulkan.dll"
#else
#  define RENDERER_DLL "d3d11.dll"
#endif

#include <stdlib.h>
#include <time.h>

function LRESULT 
win32_window_callback(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    LRESULT result{};

    switch(msg) {
        case WM_CLOSE: {
            g_running = false;
        } break;

        case WM_DESTROY: {
            // @TODO: Handle this as an error - recreate window?
            g_running = false;
        } break;

        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP: {
            ASSERT(!"Keyboard input came in through a non-dispatch message!");
        } break;

        case WM_PAINT: {
            PAINTSTRUCT paint;
            HDC hdc = BeginPaint(hwnd, &paint);
            ReleaseDC(hwnd, hdc);
            EndPaint(hwnd, &paint);
        } break;

        case WM_SETCURSOR: {
            if (g_show_cursor) {
                result = DefWindowProcA(hwnd, msg, wparam, lparam);
            } else {
                SetCursor(0);
            }
        } break;

        default: {
            result = DefWindowProcA(hwnd, msg, wparam, lparam);
        } break;
    }

    return result;
}

// @SPEC: ZII
function
OS_ALLOC(win32_alloc) {
    void *result = VirtualAlloc(0, size, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
    return result;
}

function
OS_FREE(win32_free) {
    if (memory) {
        VirtualFree(memory, 0, MEM_RELEASE);
    }
}

function f32
rand01(void) {
    return ((f32)rand() / RAND_MAX);
}

#if __DEBUG
int main(void) {
    HINSTANCE hinst = GetModuleHandle(0);
#else
int WINAPI
WinMain(HINSTANCE hinst, HINSTANCE deprecated, LPSTR cmd, int show_cmd) {
#endif
    WNDCLASSEX window_class{};
    window_class.cbSize         = sizeof(WNDCLASSEX);
    window_class.style          = CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc    = win32_window_callback;
    window_class.cbClsExtra     = 0;
    window_class.cbWndExtra     = 0;
    window_class.hInstance      = hinst;
    window_class.hIcon          = LoadIcon(hinst, MAKEINTRESOURCE(IDI_APPLICATION));
    window_class.hCursor        = LoadCursor(NULL, IDC_ARROW);
    window_class.hbrBackground  = (HBRUSH)GetStockObject(BLACK_BRUSH);
    window_class.lpszMenuName   = NULL;
    window_class.lpszClassName  = "Win32 Renderer Testbed";
    window_class.hIconSm        = LoadIcon(window_class.hInstance, MAKEINTRESOURCE(IDI_APPLICATION));
    if (!RegisterClassExA(&window_class)) {
        ASSERT(0);
    }

    HWND hwnd = CreateWindowExA(0, window_class.lpszClassName, "Win32 Renderer Testbed",
                             WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
                             CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                             0, 0, hinst, 0);

    if (!hwnd) {
        ASSERT(0);
    }

    Win32_Renderer_Function_Table renderer_function_table{};

    const char *renderer_dll_filepath = RENDERER_DLL;
    HMODULE renderer_dll = LoadLibraryA(renderer_dll_filepath);
    for (int i = 0; i < arraycount(win32_renderer_function_table_names); ++i) {
        ((void **)&renderer_function_table)[i] = GetProcAddress(renderer_dll, win32_renderer_function_table_names[i]);
    }

    g_renderer = renderer_function_table.load_renderer(hwnd);

    ShowWindow(hwnd, SW_SHOW);
    SetForegroundWindow(hwnd);
    SetFocus(hwnd);

    os.alloc = win32_alloc;
    os.free  = win32_free;

    Image images[3] = {};
    images[0].id = 2;
    images[0].data = stbi_load("texture.jpg", (int *)&images[0].width, (int *)&images[0].height, 0, 4);
    ASSERT(images[0].data);
    images[1].id = 3;
    images[1].data = stbi_load("doggo.png", (int *)&images[1].width, (int *)&images[1].height, 0, 4);
    ASSERT(images[1].data);
    images[2].id = 4;
    images[2].data = stbi_load("doggo2.png", (int *)&images[2].width, (int *)&images[2].height, 0, 4);
    ASSERT(images[2].data);


    while (g_running) 
    {
        MSG msg;
        while (PeekMessageA(&msg, hwnd, 0, 0, PM_REMOVE)) 
        {
            switch(msg.message) {
                case WM_QUIT: {
                    g_running = false;
                } break;

                case WM_SYSKEYDOWN:
                case WM_SYSKEYUP:
                case WM_KEYDOWN:
                case WM_KEYUP: {
                } break;

                case WM_MOUSEWHEEL: {
                } break;

                default: {
                    TranslateMessage(&msg);
                    DispatchMessageA(&msg);
                } break;
            }
        }

        RECT rect{};
        GetClientRect(hwnd, &rect);
        f32 w  = (f32)rect.right - (f32)rect.left;
        f32 h = (f32)rect.bottom - (f32)rect.top;

        for (u32 i = 0; i < 256; ++i) {
            u32 max_image_index = arraycount(images) - 1;
            u32 image_index = (u32)(rand01()*max_image_index + 0.5f);
            v2 center = {rand01()*w, rand01()*h};
            v2 dim = {50, 50};
            draw_textured_quad(center, dim, images[image_index]);
        }

        renderer_bubblesort();
        renderer_fill_drawcall_vertex_count();

        renderer_function_table.end_frame();
    }

    ExitProcess(0);
}
