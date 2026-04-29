#pragma once
#include <ncurses.h>
#include <sched.h>
#include "model.h"
#include "config.h"
#include "editor.h"
//int* compute_line_colors(const char* full_line, int line_len, int highlight_pair, EditorConfig* config);
void draw_initial(WINDOW* win, Buffer* buf, int* scroll_row, int* scroll_col, int cursor_line, int cursor_col, int show_line_numbers, int syntax_highlight, int* cursor_screen_y, int* cursor_screen_x, EditorConfig* config);
void draw_update(WINDOW* win, Buffer* buf, int* scroll_row, int* scroll_col, int cursor_line, int cursor_col, int show_line_numbers, int syntax_highlight, int search_mode, char* search_buffer, int selection_start_line, int selection_start_col, int selection_end_line, int selection_end_col, int selection_active, int* cursor_screen_y, int* cursor_screen_x, int replace_step, char* replace_buffer, EditorConfig* config, Editor* ed);

int calculate_digits(int n);

void get_starting_levels(Buffer *buf, int start_line, int *brace_level, int *brace_top, int brace_stack[], int *kw_level, int *kw_top, int kw_stack[], EditorConfig *config);

int visual_column (const char *line, int len, int logical_pos, int tab_width);
