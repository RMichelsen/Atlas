#pragma once

typedef enum DrawCommandType {
	DRAW_COMMAND_TEXT,
	DRAW_COMMAND_NUMBER,
	DRAW_COMMAND_RECT
} DrawCommandType;

typedef struct DrawCommandText {
	const char *content;
	u32 length;
	u32 row;
	u32 column;
} DrawCommandText;

typedef struct DrawCommandNumber {
	u32 num;
	u32 row;
	u32 column;
} DrawCommandNumber;

typedef struct DrawCommandRect {
	u32 dummy;
} DrawCommandRect;

typedef struct DrawCommand {
	DrawCommandType type;
	union {
		DrawCommandText text;
		DrawCommandNumber number;
		DrawCommandRect rect;
	};
} DrawCommand;

typedef struct DrawList {
	DrawCommand *commands;
	u32 num_commands;
} DrawList;
