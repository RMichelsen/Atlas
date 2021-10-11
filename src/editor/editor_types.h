#pragma once
#include "core/common_types.h"

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
