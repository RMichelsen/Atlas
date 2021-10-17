#pragma once

#define LINES_PER_SCROLL 3

typedef struct TextLine {
	char *content;
	u32 length;
} TextLine;

typedef struct TextView {
	u32 start_line;
} TextView;

typedef struct TextDocument {
	TextLine *lines;
	u32 num_lines;

	TextView view;
} TextDocument;

typedef struct Editor {
	TextDocument active_document;
} Editor;

static Editor editor_initialize();
static void editor_open_file(Editor *editor, const char *path);
static void editor_destroy(Editor *editor);

static void editor_scroll_down(Editor* editor, i32 line_delta);

static DrawList text_document_get_text_draw_list(TextDocument *document, u32 num_lines_on_screen);
static DrawList text_document_get_line_number_draw_list(TextDocument *document, u32 num_lines_on_screen);

