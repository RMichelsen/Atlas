#pragma once
#include <rendering/shared_rendering_types.h>

Renderer renderer_initialize(HINSTANCE hinstance, HWND hwnd);
void renderer_resize(Renderer *renderer);
void renderer_update(HWND hwnd, Renderer *renderer);
void renderer_destroy(Renderer *renderer);

