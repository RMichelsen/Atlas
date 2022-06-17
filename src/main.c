#include "common_types.h"
#include "shared_types.h"

#include <windows.h>
#include <vulkan/vulkan.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H

#include "editor.c"
#include "renderer.c"

typedef struct WindowProcContext {
    Editor *editor;
    Renderer *renderer;
} WindowProcContext;

LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    WindowProcContext *context = (WindowProcContext *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    if (!context) {
        return DefWindowProc(hwnd, msg, wparam, lparam);
    }

    switch(msg) {
    case WM_DESTROY: {
        PostQuitMessage(0);
    } return 0;
    case WM_PAINT:
    {
        PAINTSTRUCT paint_struct = { 0 };
        BeginPaint(hwnd, &paint_struct);

        DrawList draw_lists[] = {
            text_document_get_text_draw_list(
                &context->editor->active_document,
                renderer_get_number_of_lines_on_screen(context->renderer)
            ),
            text_document_get_line_number_draw_list(
                &context->editor->active_document,
                renderer_get_number_of_lines_on_screen(context->renderer)
            )
        };

        renderer_update_draw_lists(context->renderer, draw_lists, _countof(draw_lists));
        renderer_present(context->renderer);

        EndPaint(hwnd, &paint_struct);
    } return 0;
    case WM_MOUSEWHEEL: {
        i32 line_delta = (GET_WHEEL_DELTA_WPARAM(wparam) / WHEEL_DELTA) * LINES_PER_SCROLL;
        editor_scroll_down(context->editor, -line_delta);

        InvalidateRect(hwnd, NULL, FALSE);
    }
    }
    return DefWindowProc(hwnd, msg, wparam, lparam);
}

int WINAPI wWinMain(HINSTANCE hinstance, HINSTANCE prev_hinstance, PWSTR cmd_line, int cmd_show) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE);

    AllocConsole();
    FILE *dummy;
    freopen_s(&dummy, "CONIN$", "r", stdin);
    freopen_s(&dummy, "CONOUT$", "w", stdout);
    freopen_s(&dummy, "CONOUT$", "w", stderr);

    const char *window_class_name = "Atlas_Class";
    const char *window_title = "Atlas";
    WNDCLASSEX window_class = {
        .cbSize = sizeof(WNDCLASSEX),
        .style = CS_HREDRAW | CS_VREDRAW,
        .lpfnWndProc = window_proc,
        .hInstance = hinstance,
        .hCursor = LoadCursor(NULL, IDC_ARROW),
        .hbrBackground = NULL,
        .lpszClassName = window_class_name,
    };

    if(!RegisterClassEx(&window_class)) {
        return 1;
    }

    HWND hwnd = CreateWindow(
        window_class_name,
        window_title,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        NULL,
        NULL,
        hinstance,
        NULL
    );
    if(hwnd == NULL) return 1;
    ShowWindow(hwnd, cmd_show);

    Editor editor = editor_initialize();
    Renderer renderer = renderer_initialize(hinstance, hwnd);
    WindowProcContext window_proc_context = {
        .editor = &editor,
        .renderer = &renderer
    };
    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)&window_proc_context);

    editor_open_file(&editor, "C:/Users/RasmusMichelsen/Desktop/Atlas/src/main.c");
    
    MSG msg;
    uint32_t previous_width = 0, previous_height = 0;
    while (GetMessage(&msg, 0, 0, 0)) {
        // TODO: Virtual-key to character translation
        // TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    renderer_destroy(&renderer);
    UnregisterClass(window_class_name, hinstance);
    DestroyWindow(hwnd);
    return 0;
}
