#include "editor.h"

Editor editor_initialize() {
	return (Editor) {
		.main_document = {
			.lines = NULL,
			.num_lines = 0,
			.view = {
				.start_row = 0
			}
		}
	};
}

void editor_open_file(Editor *editor, const char *path) {
	FILE *file = fopen(path, "r");

	editor->main_document.lines = malloc(sizeof(TextLine) * 1024);

	u32 offset = 0;
	u32 line = 0;
	while(1) {
		editor->main_document.lines[line].content = malloc(sizeof(char) * 1024);
		char *result = fgets(editor->main_document.lines[line].content, 1024, file);

		u32 new_offset = ftell(file);
		editor->main_document.lines[line].length = new_offset - offset;
		offset = new_offset;

		if(!result) {
			break;
		}

		++line;
	}


	editor->main_document.num_lines = line;
	fclose(file);
}

void editor_destroy(Editor *editor) {
}
