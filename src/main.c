#include <core/common_types.h>
#include <windows.h>

#include "editor/editor.h"
#include "rendering/renderer.h"

LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	switch(msg) {
	case WM_DESTROY:
	{
		PostQuitMessage(0);
	} return 0;
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

	const wchar_t *window_class_name = L"Atlas_Class";
	const wchar_t *window_title = L"Atlas";
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

	Renderer renderer = renderer_initialize(hinstance, hwnd);

	Editor editor = editor_initialize();
	editor_open_file(&editor, "C:/Users/RM/Desktop/Atlas/src/main.c");
	
	MSG msg;
	while(1) {
		while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
			if(msg.message == WM_QUIT) {
				goto Done;
			}
		}

		if(IsIconic(hwnd)) {
			continue;
		}
		else {
			DrawCommands draw_commands;
			draw_commands.commands = malloc(editor.main_document.num_lines * sizeof(DrawCommand));
			draw_commands.num_commands = editor.main_document.num_lines;
			for(u32 i = 0; i < editor.main_document.num_lines; ++i) {
				draw_commands.commands[i] = (DrawCommand) {
					.type = DRAW_COMMAND_TEXT,
					.text = {
						.content = editor.main_document.lines[i].content,
						.length = editor.main_document.lines[i].length,
						.column = 3,
						.row = i
					}
				};

			}
			renderer_update_draw_commands(&renderer, &draw_commands, 1);

			renderer_present(&renderer);
		}
	}

Done:
	renderer_destroy(&renderer);
	UnregisterClass(window_class_name, hinstance);
	DestroyWindow(hwnd);
	return 0;
}
