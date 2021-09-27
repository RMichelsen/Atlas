#pragma once
#include <core/common_types.h>

typedef struct TextLine {
	char *content;
	u32 length;
} TextLine;

typedef struct TextView {
	u32 start_row;
} TextView;

typedef struct TextDocument {
	TextLine *lines;
	u32 num_lines;

	TextView view;
} TextDocument;

typedef struct Editor {
	TextDocument main_document;
} Editor;
