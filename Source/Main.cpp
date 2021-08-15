#include "PCH.h"

#include "Graphics/Renderer.h"

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
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
	WNDCLASSEX window_class{
		.cbSize = sizeof(WNDCLASSEX),
		.style = CS_HREDRAW | CS_VREDRAW,
		.lpfnWndProc = WndProc,
		.hInstance = hinstance,
		.hCursor = LoadCursor(nullptr, IDC_ARROW),
		.hbrBackground = nullptr,
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
		nullptr,
		nullptr,
		hinstance,
		nullptr
	);
	if(hwnd == nullptr) return 1;
	ShowWindow(hwnd, cmd_show);

	Renderer renderer = RendererInitialize(hinstance, hwnd);

	MSG msg;
	while(true) {
		while(PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
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
			RendererUpdate(hwnd, &renderer);
		}
	}

Done:
	RendererDestroy(&renderer);
	UnregisterClass(window_class_name, hinstance);
	DestroyWindow(hwnd);
	return 0;
}
