#include "editor.h"
#include "view.h"
#include "controller.h"
#include <stdlib.h>
#include <string.h>

void editor_init(Editor* ed, int argc, char* argv[]) {
    buffer_init(&ed->model);
    ed->filename = argc > 1 ? argv[1] : NULL;
    if (ed->filename) {
        buffer_load_from_file(&ed->model, ed->filename);
    }
    if (buffer_num_lines(&ed->model) == 0) {
        buffer_insert_line(&ed->model, 0, "");
    }
    ed->scroll_row = 0;
    ed->scroll_col = 0;
    ed->show_line_numbers = 0;
    ed->cursor_line = 0;
    ed->cursor_col = buffer_get_line_length(&ed->model, 0);
    ed->search_buffer[0] = 0;
    ed->search_mode = 0;
    ed->clipboard = NULL;
}

void editor_draw(WINDOW* win, Editor* ed) {
    size_t dummy_y, dummy_x;
    draw_update(win, &ed->model, ed->scroll_row, ed->scroll_col, ed->cursor_line, ed->cursor_col, ed->show_line_numbers, ed->search_mode, ed->search_buffer, &dummy_y, &dummy_x);
}

void editor_handle_input(Editor* ed, int ch) {
    handle_input(ch, &ed->model, &ed->scroll_row, &ed->scroll_col, &ed->cursor_line, &ed->cursor_col, &ed->show_line_numbers, ed->search_buffer, &ed->search_mode, &ed->clipboard, ed->filename);
}

void editor_cleanup(Editor* ed) {
    buffer_free(&ed->model);
    if (ed->clipboard) free(ed->clipboard);
}