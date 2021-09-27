#pragma once
#include <editor/shared_editor_types.h>

Editor editor_initialize();
void editor_open_file(Editor *editor, const char *path);
void editor_destroy(Editor *editor);

