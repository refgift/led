#include "editor.h"
#include "model.h"
#include "controller.h"
#include "view.h"
#include "config.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

void editor_init(Editor* ed, int argc, char* argv[]) {
    load_color_config(&ed->config);
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
    const char* ext = ed->filename ? strrchr(ed->filename, '.') : NULL;
    ed->syntax_highlight = 0;
    if (ext) {
        size_t len = strlen(ed->config.syntax_extensions);
        char* list = malloc(len + 1);
        if (list) {
            strcpy(list, ed->config.syntax_extensions);
            char* token = strtok(list, ",");
            while (token) {
                if (strcmp(ext, token) == 0) {
                    ed->syntax_highlight = 1;
                    break;
                }
                token = strtok(NULL, ",");
            }
            free(list);
        }
    }
    ed->cursor_line = 0;
    ed->cursor_col = buffer_get_line_length(&ed->model, 0);
    ed->selection_start_line = 0;
    ed->selection_start_col = 0;
    ed->selection_end_line = 0;
    ed->selection_end_col = 0;
    ed->selection_active = 0;
    ed->search_buffer[0] = 0;
    ed->search_mode = 0;
    ed->replace_buffer[0] = 0;
    ed->replace_step = 0;
    ed->clipboard = NULL;
}

void editor_draw(WINDOW* win, Editor* ed) {
    size_t dummy_y, dummy_x;
    draw_update(win, &ed->model, ed->scroll_row, ed->scroll_col, ed->cursor_line, ed->cursor_col, ed->show_line_numbers, ed->syntax_highlight, ed->search_mode, ed->search_buffer, ed->selection_start_line, ed->selection_start_col, ed->selection_end_line, ed->selection_end_col, ed->selection_active, &dummy_y, &dummy_x, ed->replace_step, ed->replace_buffer, &ed->config);
}

void editor_handle_input(Editor* ed, int ch) {
    if (ch == 18) { // Ctrl+R replace
        if (ed->replace_step == 0) {
            ed->replace_step = 1;
            ed->search_mode = 0;
            memset(ed->search_buffer, 0, sizeof(ed->search_buffer));
            memset(ed->replace_buffer, 0, sizeof(ed->replace_buffer));
        }
    } else if (ed->replace_step == 1) {
        if (ch == 27) ed->replace_step = 0;
        else if (ch == 10 || ch == 13) ed->replace_step = 2;
        else if (isprint(ch) && strlen(ed->search_buffer) < sizeof(ed->search_buffer) - 1) {
            strncat(ed->search_buffer, (char*)&ch, 1);
        }
    } else if (ed->replace_step == 2) {
        if (ch == 27) ed->replace_step = 0;
        else if (ch == 10 || ch == 13) {
            buffer_replace_all(&ed->model, ed->search_buffer, ed->replace_buffer);
            ed->replace_step = 0;
        } else if (isprint(ch) && strlen(ed->replace_buffer) < sizeof(ed->replace_buffer) - 1) {
            strncat(ed->replace_buffer, (char*)&ch, 1);
        }
    } else {
        handle_input(ch, &ed->model, &ed->scroll_row, &ed->scroll_col, &ed->cursor_line, &ed->cursor_col, &ed->show_line_numbers, ed->search_buffer, &ed->search_mode, &ed->clipboard, ed->filename, &ed->selection_start_line, &ed->selection_start_col, &ed->selection_end_line, &ed->selection_end_col, &ed->selection_active);
    }
}

void editor_cleanup(Editor* ed) {
    buffer_free(&ed->model);
    if (ed->clipboard) free(ed->clipboard);
}