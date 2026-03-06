#include "view.h"
#include <string.h>

static int calculate_digits(size_t n) {
    if (n == 0) return 1;
    int d = 0;
    do {
        d++;
        n /= 10;
    } while (n > 0);
    return d;
}

void draw_initial(WINDOW* win, Buffer* buf, size_t scroll_row, size_t scroll_col, size_t cursor_line, size_t cursor_col, int show_line_numbers) {
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
        size_t len = strlen(line);
        if (start_col < len) {
            size_t print_len = len - start_col;
            if (print_len > (size_t)COLS - 2 - num_width) print_len = COLS - 2 - num_width;
            mvprintw(1 + (int)i, 1 + num_width, "%.*s", (int)print_len, &line[start_col]);
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

void draw_update(WINDOW* win, Buffer* buf, size_t scroll_row, size_t scroll_col, size_t cursor_line, size_t cursor_col, int show_line_numbers, int search_mode, char* search_buffer, size_t* cursor_screen_y, size_t* cursor_screen_x) {
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
        size_t len = strlen(line);
        if (start_col < len) {
            size_t print_len = len - start_col;
            if (print_len > (size_t)COLS - 2 - num_width) print_len = COLS - 2 - num_width;
            mvprintw(1 + (int)i, 1 + num_width, "%.*s", (int)print_len, &line[start_col]);
        }
    }
    // Status bar
    if (search_mode) {
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