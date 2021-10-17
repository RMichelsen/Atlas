#include "editor.h"

static i32 clamp(i32 x, i32 low, i32 high) {
	if(x < low) return low;
	if(x > high) return high;
	return x;
}

static Editor editor_initialize() {
	return (Editor) {
		.active_document = {
			.lines = NULL,
			.num_lines = 0,
			.view = {
				.start_line = 0
			}
		}
	};
}

static void editor_open_file(Editor *editor, const char *path) {
	FILE *file = fopen(path, "r");

	editor->active_document.lines = malloc(sizeof(TextLine) * 1024);

	u32 offset = 0;
	u32 line = 0;
	while(1) {
		// TODO: Make string arena to decrease number of allocations
		editor->active_document.lines[line].content = malloc(sizeof(char) * 1024);
		char *result = fgets(editor->active_document.lines[line].content, 1024, file);

		u32 new_offset = ftell(file);
		editor->active_document.lines[line].length = new_offset - offset;
		offset = new_offset;

		if(!result) {
			break;
		}

		++line;
	}

	editor->active_document.num_lines = line;
	fclose(file);
}

static void editor_destroy(Editor *editor) {
	for(u32 i = 0; i < editor->active_document.num_lines; ++i) {
		free(editor->active_document.lines[i].content);
	}
	free(editor->active_document.lines);
}

static void editor_scroll_down(Editor* editor, i32 line_delta) {
	editor->active_document.view.start_line = clamp(
		editor->active_document.view.start_line + line_delta,
		0,
		editor->active_document.num_lines
	);
}

static DrawList text_document_get_text_draw_list(TextDocument *document, u32 num_lines_on_screen) {
	u32 start_line = document->view.start_line;
	u32 end_line = min(start_line + num_lines_on_screen, document->num_lines);
	u32 line_number_digit_count = (u32)log10(end_line) + 1;

	DrawList draw_list;
	draw_list.num_commands = end_line - start_line;
	draw_list.commands = malloc(draw_list.num_commands * sizeof(DrawCommand));

	for(u32 line = start_line, i = 0; line < end_line; ++line, ++i) {
		draw_list.commands[i] = (DrawCommand) {
			.type = DRAW_COMMAND_TEXT,
			.text = {
				.content = document->lines[line].content,
				.length = document->lines[line].length,
				.column = line_number_digit_count + 1,
				.row = i
			}
		};
	}
	return draw_list;
}

static DrawList text_document_get_line_number_draw_list(TextDocument *document, u32 num_lines_on_screen) {
	u32 start_line = document->view.start_line;
	u32 end_line = min(start_line + num_lines_on_screen, document->num_lines);
	u32 line_number_digit_count = (u32)log10(end_line) + 1;

	DrawList draw_list;
	draw_list.num_commands = end_line - start_line;
	draw_list.commands = malloc(draw_list.num_commands * sizeof(DrawCommand));

	for(u32 line = start_line, i = 0; line < end_line; ++line, ++i) {
		u32 digits_in_number = (u32)log10(line) + 1;
		draw_list.commands[i] = (DrawCommand) {
			.type = DRAW_COMMAND_NUMBER,
			.number = {
				.num = line,
				.column = line_number_digit_count - digits_in_number,
				.row = i
			}
		};
	}
	return draw_list;
}

