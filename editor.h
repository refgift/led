#pragma once

#include <ncurses.h>
#include "model.h"

typedef struct {
    Buffer model;
    size_t scroll_row;
    size_t scroll_col;
    int show_line_numbers;
    int syntax_highlight;
    size_t cursor_line;
    size_t cursor_col;
    size_t selection_start_line;
    size_t selection_start_col;
    size_t selection_end_line;
    size_t selection_end_col;
    int selection_active;
    char search_buffer[256];
    int search_mode;
    char replace_buffer[256];
    int replace_step;
    char* clipboard;
    const char* filename;
} Editor;

void editor_init(Editor* ed, int argc, char* argv[]);
void editor_draw(WINDOW* win, Editor* ed);
void editor_handle_input(Editor* ed, int ch);
void editor_cleanup(Editor* ed);