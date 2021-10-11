#pragma once
#include "core/shared_types.h"
#include "editor/editor_types.h"

Editor editor_initialize();
void editor_open_file(Editor *editor, const char *path);
void editor_destroy(Editor *editor);

void editor_scroll_down(Editor* editor, i32 line_delta);

DrawList text_document_get_text_draw_list(TextDocument *document, u32 num_lines_on_screen);
DrawList text_document_get_line_number_draw_list(TextDocument *document, u32 num_lines_on_screen);
