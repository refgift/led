#pragma once

#include <ncurses.h>
#include "model.h"
#include "config.h"
#include "editor.h"

int* compute_line_colors(const char* full_line, size_t line_len, int highlight_pair, EditorConfig* config);
void draw_initial(WINDOW* win, Buffer* buf, size_t* scroll_row, size_t* scroll_col, size_t cursor_line, size_t cursor_col, int show_line_numbers, int syntax_highlight, size_t* cursor_screen_y, size_t* cursor_screen_x, EditorConfig* config);
void draw_update(WINDOW* win, Buffer* buf, size_t* scroll_row, size_t* scroll_col, size_t cursor_line, size_t cursor_col, int show_line_numbers, int syntax_highlight, int search_mode, char* search_buffer, size_t selection_start_line, size_t selection_start_col, size_t selection_end_line, size_t selection_end_col, int selection_active, size_t* cursor_screen_y, size_t* cursor_screen_x, int replace_step, char* replace_buffer, EditorConfig* config, Editor* ed);