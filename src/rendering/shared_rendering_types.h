#pragma once
#include <core/common_types.h>

#define NUM_PRINTABLE_CHARS 95

typedef struct GlyphPoint {
	float x;
	float y;
} GlyphPoint;

typedef struct GlyphLine {
	GlyphPoint a;
	GlyphPoint b;
} GlyphLine;

typedef struct GlyphOffset {
	u32 offset;
	u32 num_lines;
	float min_y;
	u32 padding;
} GlyphOffset;

typedef struct GlyphMetrics {
	float glyph_width;
	float glyph_height;
	u32 cell_width;
	u32 cell_height;
} GlyphMetrics;

typedef struct TesselatedGlyphs {
	GlyphLine *lines;
	u64 num_lines;
	GlyphOffset *glyph_offsets;
	u64 num_glyphs;
	GlyphMetrics metrics;
} TesselatedGlyphs;

typedef enum DrawCommandType {
	DRAW_COMMAND_TEXT,
	DRAW_COMMAND_RECT
} DrawCommandType;

typedef struct DrawCommandText {
	const char *content;
	u32 length;
	u32 row;
	u32 column;
} DrawCommandText;

typedef struct DrawCommandRect {
	u32 dummy;
} DrawCommandRect;

typedef struct DrawCommand {
	DrawCommandType type;
	union {
		DrawCommandText text;
		DrawCommandRect rect;
	};
} DrawCommand;

typedef struct DrawCommands {
	DrawCommand *commands;
	u32 num_commands;
} DrawCommands;
