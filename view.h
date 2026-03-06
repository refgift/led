#pragma once

#include <ncurses.h>
#include "model.h"
#include "config.h"

void draw_initial(WINDOW* win, Buffer* buf, size_t scroll_row, size_t scroll_col, size_t cursor_line, size_t cursor_col, int show_line_numbers, int syntax_highlight, ColorConfig* config);
void draw_update(WINDOW* win, Buffer* buf, size_t scroll_row, size_t scroll_col, size_t cursor_line, size_t cursor_col, int show_line_numbers, int syntax_highlight, int search_mode, char* search_buffer, size_t selection_start_line, size_t selection_start_col, size_t selection_end_line, size_t selection_end_col, int selection_active, size_t* cursor_screen_y, size_t* cursor_screen_x, int replace_step, char* replace_buffer, ColorConfig* config);