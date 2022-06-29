#include "common_types.h"
#include "shared_types.h"

#include "editor.c"
#include "renderer.c"

typedef struct WindowProcContext {
    Editor *editor;
    Renderer *renderer;
} WindowProcContext;

#ifdef _WIN32
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
    Renderer renderer = renderer_initialize((Window) { .handle = hwnd, .instance = hinstance });
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
#else
int main(int argc, char **argv) {
    xcb_connection_t *connection = xcb_connect(NULL, NULL);
    xcb_screen_t *screen = xcb_setup_roots_iterator(xcb_get_setup(connection)).data;

    uint32_t values[2] = { screen->black_pixel, 0 };

    xcb_window_t window = xcb_generate_id(connection);

    xcb_create_window(
        connection,
        XCB_COPY_FROM_PARENT,
        window,
        screen->root,
        0, 0,
        1920, 1080,
        0,
        XCB_WINDOW_CLASS_INPUT_OUTPUT,
        screen->root_visual,
        XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK,
        values
    );

    xcb_map_window(connection, window);
    xcb_flush(connection);

    Window w;

    Renderer renderer = renderer_initialize((Window) { .handle = window, .connection = connection });

    Editor editor = editor_initialize();
    editor_open_file(&editor, "/home/rm/Atlas/src/main.c");

    for (;;) {
        DrawList draw_lists[] = {
            text_document_get_text_draw_list(
                &editor.active_document,
                renderer_get_number_of_lines_on_screen(&renderer)
            ),
            text_document_get_line_number_draw_list(
                &editor.active_document,
                renderer_get_number_of_lines_on_screen(&renderer)
            )
        };

        renderer_update_draw_lists(&renderer, draw_lists, ARRAY_LENGTH(draw_lists));
        renderer_present(&renderer);
    }

    xcb_destroy_window(connection, window);
    return 0;
}
#endif
