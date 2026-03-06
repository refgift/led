#include "view.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

static const char* meta_symbols = ";,{}()[]";

static int calculate_digits(size_t n) {
    if (n == 0) return 1;
    int d = 0;
    do {
        d++;
        n /= 10;
    } while (n > 0);
    return d;
}

static void print_highlighted(int y, int x, const char* full_line, size_t line_len, size_t start, size_t len, int highlight_pair) {
    if (highlight_pair == 1) {
        mvprintw(y, x, "%.*s", (int)len, &full_line[start]);
        return;
    }
    int* colors = malloc(line_len * sizeof(int));
    if (!colors) return; // Handle error
    int stack[100]; // Simple stack for nesting levels
    int top = 0;
    int current_level = 1;
    for (size_t i = 0; i < line_len; i++) {
        char c = full_line[i];
        if (strchr(meta_symbols, c)) {
            if (c == ';') {
                colors[i] = 3; // Red for semicolons
            } else if (c == '{' || c == '(' || c == '[') {
                // Opening
                stack[top++] = current_level;
                int lvl = current_level;
                if (lvl == 1) colors[i] = 4; // Blue
                else if (lvl == 2) colors[i] = 5; // Green
                else if (lvl >= 3) colors[i] = 6; // Yellow
                else colors[i] = 4;
                current_level++;
            } else if (c == '}' || c == ')' || c == ']') {
                // Closing
                if (top > 0) {
                    int lvl = stack[--top];
                    if (lvl == 1) colors[i] = 4; // Blue
                    else if (lvl == 2) colors[i] = 5; // Green
                    else if (lvl >= 3) colors[i] = 6; // Yellow
                    else colors[i] = 4;
                } else {
                    colors[i] = 4; // Default
                }
                current_level = current_level > 1 ? current_level - 1 : 1;
            } else {
                colors[i] = 4; // Other meta like ,
            }
        } else {
            colors[i] = 1; // Normal
        }
    }
    for (size_t i = 0; i < len; i++) {
        size_t pos = start + i;
        attron(COLOR_PAIR(colors[pos]));
        mvprintw(y, x + i, "%c", full_line[pos]);
        attroff(COLOR_PAIR(colors[pos]));
    }
    if (colors) free(colors);
}

void draw_initial(WINDOW* win, Buffer* buf, size_t scroll_row, size_t scroll_col, size_t cursor_line, size_t cursor_col, int show_line_numbers, int syntax_highlight) {
    clear();
    box(win, 0, 0);
    int num_digits = calculate_digits(buffer_num_lines(buf));
    int num_width = show_line_numbers ? num_digits + 1 : 0;
    for (size_t i = 0; i < (size_t)LINES - 2; i++) {
        size_t line_idx = scroll_row + i;
        if (line_idx >= buffer_num_lines(buf)) break;
        if (show_line_numbers) {
            mvprintw(1 + (int)i, 1, "%*zu ", num_digits, line_idx + 1);
        }
        const char* line = buffer_get_line(buf, line_idx);
        size_t start_col = scroll_col;
        size_t line_len = strlen(line);
        if (start_col < line_len) {
            size_t print_len = line_len - start_col;
            if (print_len > (size_t)COLS - 2 - num_width) print_len = COLS - 2 - num_width;
            print_highlighted(1 + (int)i, 1 + num_width, line, line_len, start_col, print_len, syntax_highlight ? 4 : 1);
        }
    }
    // Status bar
    mvprintw(LINES - 1, 1, "Line %zu/%zu Col %zu", cursor_line + 1, buffer_num_lines(buf), cursor_col + 1);
    int screen_y = 1 + (int)(cursor_line - scroll_row);
    int screen_x = 1 + num_width + (int)(cursor_col - scroll_col);
    if (screen_x < 1 + num_width) screen_x = 1 + num_width;
    if (screen_x > COLS - 1) screen_x = COLS - 1;
    move(screen_y, screen_x);
    refresh();
}

void draw_update(WINDOW* win, Buffer* buf, size_t scroll_row, size_t scroll_col, size_t cursor_line, size_t cursor_col, int show_line_numbers, int syntax_highlight, int search_mode, char* search_buffer, size_t selection_start_line, size_t selection_start_col, size_t selection_end_line, size_t selection_end_col, int selection_active, size_t* cursor_screen_y, size_t* cursor_screen_x, int replace_step, char* replace_buffer) {
    clear();
    box(win, 0, 0);
    int num_digits = calculate_digits(buffer_num_lines(buf));
    int num_width = show_line_numbers ? num_digits + 1 : 0;
    for (size_t i = 0; i < (size_t)LINES - 2; i++) {
        size_t line_idx = scroll_row + i;
        if (line_idx >= buffer_num_lines(buf)) break;
        if (show_line_numbers) {
            mvprintw(1 + (int)i, 1, "%*zu ", num_digits, line_idx + 1);
        }
        const char* line = buffer_get_line(buf, line_idx);
        size_t len = strlen(line);
        size_t pos = scroll_col;
        int x = 1 + num_width;
        size_t sel_start = len;
        size_t sel_end = len;
        if (selection_active && line_idx >= selection_start_line && line_idx <= selection_end_line) {
            sel_start = (line_idx == selection_start_line) ? selection_start_col : 0;
            sel_end = (line_idx == selection_end_line) ? selection_end_col : len;
        }
        // Print line in parts: before selection, selection, after
        // Before selection
        if (pos < sel_start) {
            size_t end = sel_start < len ? sel_start : len;
            size_t print_len = end - pos;
            if (print_len > 0) {
                if (print_len > (size_t)COLS - 2 - num_width - (x - 1 - num_width)) print_len = COLS - 2 - num_width - (x - 1 - num_width);
                print_highlighted(1 + (int)i, x, line, len, pos, print_len, syntax_highlight ? 4 : 1);
                x += print_len;
                pos += print_len;
            }
        }
        // Selection
        if (pos < sel_end) {
            size_t end = sel_end;
            size_t print_len = end - pos;
            if (print_len > 0) {
                if (print_len > (size_t)COLS - 2 - num_width - (x - 1 - num_width)) print_len = COLS - 2 - num_width - (x - 1 - num_width);
                attron(COLOR_PAIR(2));
                mvprintw(1 + (int)i, x, "%.*s", (int)print_len, &line[pos]);
                attroff(COLOR_PAIR(2));
                x += print_len;
                pos += print_len;
            }
        }
        // After selection
        if (pos < len) {
            size_t print_len = len - pos;
            if (print_len > (size_t)COLS - 2 - num_width - (x - 1 - num_width)) print_len = COLS - 2 - num_width - (x - 1 - num_width);
            print_highlighted(1 + (int)i, x, line, len, pos, print_len, 4);
        }
    }
    // Status bar
    if (replace_step == 1) {
        mvprintw(LINES - 1, 1, "Replace search: %s", search_buffer);
    } else if (replace_step == 2) {
        mvprintw(LINES - 1, 1, "Replace with: %s", replace_buffer);
    } else if (search_mode) {
        mvprintw(LINES - 1, 1, "Search: %s", search_buffer);
    } else {
        mvprintw(LINES - 1, 1, "Line %zu/%zu Col %zu", cursor_line + 1, buffer_num_lines(buf), cursor_col + 1);
    }
    *cursor_screen_y = 1 + (cursor_line - scroll_row);
    *cursor_screen_x = 1 + num_width + (cursor_col - scroll_col);
    if (*cursor_screen_x < 1 + num_width) *cursor_screen_x = 1 + num_width;
    if (*cursor_screen_x > COLS - 1) *cursor_screen_x = COLS - 1;
    move(*cursor_screen_y, *cursor_screen_x);
    refresh();
}
