#pragma once

#include <ncurses.h>
#include "model.h"

void draw_initial(WINDOW* win, Buffer* buf, size_t scroll_row, size_t scroll_col, size_t cursor_line, size_t cursor_col, int show_line_numbers);
void draw_update(WINDOW* win, Buffer* buf, size_t scroll_row, size_t scroll_col, size_t cursor_line, size_t cursor_col, int show_line_numbers, int search_mode, char* search_buffer, size_t* cursor_screen_y, size_t* cursor_screen_x);