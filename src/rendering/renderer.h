#pragma once
#include "rendering/shared_rendering_types.h"

Renderer renderer_initialize(HINSTANCE hinstance, HWND hwnd);
void renderer_resize(Renderer *renderer);
void renderer_update(Renderer *renderer, DrawCommands draw_commands);
void renderer_draw(Renderer *renderer);
void renderer_destroy(Renderer *renderer);

