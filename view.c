#include "view.h"
#include "config.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <limits.h>

static const char* meta_symbols = ";,{}()[]";

typedef struct {
    char open[32];
    char close[32];
} KeywordPair;

static int is_reserved_word(const char* word, const char* list) {
    char* dup = malloc(strlen(list) + 1);
    if (!dup) return 0;
    strcpy(dup, list);
    char* token = strtok(dup, ",");
    while (token) {
        if (strcmp(word, token) == 0) {
            free(dup);
            return 1;
        }
        token = strtok(NULL, ",");
    }
    free(dup);
    return 0;
}

static int calculate_digits(size_t n) {
    if (n == 0) return 1;
    int d = 0;
    do {
        d++;
        n /= 10;
    } while (n > 0);
    return d;
}

int* compute_line_colors(const char* full_line, size_t line_len, int highlight_pair, EditorConfig* config) {
    if (highlight_pair == 1 || line_len > 10000) {
        return NULL; // No colors
    }
    int* colors = malloc(line_len * sizeof(int));
    if (!colors) return NULL;
    int stack[256]; // Simple stack for nesting levels
    int top = 0;
    int current_level = 1;
    for (size_t i = 0; i < line_len; i++) {
        char c = full_line[i];
        if (strchr(meta_symbols, c)) {
            if (c == ';') {
                colors[i] = 3; // Red for semicolons
            } else if (c == '{' || c == '(' || c == '[') {
                // Opening
                if (top < 256) stack[top++] = current_level;
                int lvl = current_level;
                if (lvl == 1) colors[i] = 4;
                else if (lvl == 2) colors[i] = 5;
                else if (lvl == 3) colors[i] = 6;
                else if (lvl >= 4) colors[i] = 7;
                else colors[i] = 4;
                current_level++;
            } else if (c == '}' || c == ')' || c == ']') {
                // Closing
                if (top > 0) {
                    int lvl = stack[--top];
                    if (lvl == 1) colors[i] = 4;
                    else if (lvl == 2) colors[i] = 5;
                    else if (lvl == 3) colors[i] = 6;
                    else if (lvl >= 4) colors[i] = 7;
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
    // Highlight reserved words and paired keywords
    if (highlight_pair != 1) { // Only if highlighting enabled
        // Parse paired keywords
        KeywordPair pairs[10];
        int num_pairs = 0;
        char* dup_pk = malloc(strlen(config->syntax.paired_keywords) + 1);
        if (dup_pk) {
            strcpy(dup_pk, config->syntax.paired_keywords);
            char* token = strtok(dup_pk, ",");
            while (token && num_pairs < 10) {
                char* dash = strchr(token, '-');
                if (dash) {
                    *dash = '\0';
                    strncpy(pairs[num_pairs].open, token, sizeof(pairs[num_pairs].open) - 1);
                    strncpy(pairs[num_pairs].close, dash + 1, sizeof(pairs[num_pairs].close) - 1);
                    num_pairs++;
                }
                token = strtok(NULL, ",");
            }
            free(dup_pk);
        }
        // Stacks for keyword pairing
        int kw_stack[100];
        int kw_top = 0;
        int kw_current_level = 1;
        size_t word_start = 0;
        int in_word = 0;
        int word_count = 0;
        for (size_t i = 0; i < line_len; i++) {
            char c = full_line[i];
            if (isalnum(c) || c == '_') {
                if (!in_word) {
                    word_start = i;
                    in_word = 1;
                }
            } else {
                if (in_word) {
                    if (word_count++ > 100) {
                        in_word = 0;
                        continue;
                    }
                    size_t wlen = i - word_start;
                    char* word = malloc(wlen + 1);
                    if (word) {
                        memcpy(word, &full_line[word_start], wlen);
                        word[wlen] = '\0';
                        int colored = 0;
                        // Check paired keywords
                        for (int p = 0; p < num_pairs; p++) {
                            if (strcmp(word, pairs[p].open) == 0) {
                                if (kw_top < 100) kw_stack[kw_top++] = kw_current_level;
                                int lvl = kw_current_level;
                                for (size_t j = word_start; j < i && j < line_len; j++) {
                                    colors[j] = 3 + (lvl > 4 ? 4 : lvl);
                                }
                                kw_current_level++;
                                colored = 1;
                                break;
                            } else if (strcmp(word, pairs[p].close) == 0) {
                                if (kw_top > 0) {
                                    int lvl = kw_stack[--kw_top];
                                    for (size_t j = word_start; j < i && j < line_len; j++) {
                                        colors[j] = 3 + (lvl > 4 ? 4 : lvl);
                                    }
                                }
                                kw_current_level = kw_current_level > 1 ? kw_current_level - 1 : 1;
                                colored = 1;
                                break;
                            }
                        }
                        // If not paired and reserved, color 8
                         if (!colored && is_reserved_word(word, config->syntax.reserved_words)) {
                            for (size_t j = word_start; j < i && j < line_len; j++) {
                                colors[j] = 8;
                            }
                        }
                        free(word);
                    }

                    // Check for symbol pairs
                    int symbol_colored = 0;
                    for (int p = 0; p < num_pairs && !symbol_colored; p++) {
                        size_t len_open = strlen(pairs[p].open);
                        if (i + len_open <= line_len && strncmp(&full_line[i], pairs[p].open, len_open) == 0) {
                            if (kw_top < 100) kw_stack[kw_top++] = kw_current_level;
                            int lvl = kw_current_level;
                            for (size_t j = i; j < i + len_open && j < line_len; j++) {
                                colors[j] = 3 + (lvl > 4 ? 4 : lvl);
                            }
                            kw_current_level++;
                            i += len_open - 1; // skip ahead
                            symbol_colored = 1;
                        } else {
                            size_t len_close = strlen(pairs[p].close);
                            if (i + len_close <= line_len && strncmp(&full_line[i], pairs[p].close, len_close) == 0) {
                                if (kw_top > 0) {
                                    int lvl = kw_stack[--kw_top];
                                    for (size_t j = i; j < i + len_close && j < line_len; j++) {
                                        colors[j] = 3 + (lvl > 4 ? 4 : lvl);
                                    }
                                }
                                kw_current_level = kw_current_level > 1 ? kw_current_level - 1 : 1;
                                i += len_close - 1;
                                symbol_colored = 1;
                            }
                        }
                    }

                    in_word = 0;
                }
            }
        }
        if (in_word) { // End of line
            if (word_count++ > 100) {
                // Skip
            } else {
                size_t wlen = line_len - word_start;
                char* word = malloc(wlen + 1);
                if (word) {
                memcpy(word, &full_line[word_start], wlen);
                word[wlen] = '\0';
                int colored = 0;
                // Check paired keywords
                for (int p = 0; p < num_pairs; p++) {
                    if (strcmp(word, pairs[p].open) == 0) {
                        kw_stack[kw_top++] = kw_current_level;
                        int lvl = kw_current_level;
                        for (size_t j = word_start; j < line_len; j++) {
                            colors[j] = 3 + (lvl > 4 ? 4 : lvl);
                        }
                        kw_current_level++;
                        colored = 1;
                        break;
                    } else if (strcmp(word, pairs[p].close) == 0) {
                        if (kw_top > 0) {
                            int lvl = kw_stack[--kw_top];
                            for (size_t j = word_start; j < line_len; j++) {
                                colors[j] = 3 + (lvl > 4 ? 4 : lvl);
                            }
                        }
                        kw_current_level = kw_current_level > 1 ? kw_current_level - 1 : 1;
                        colored = 1;
                        break;
                    }
                }
                // If not paired and reserved, color 8
                if (!colored && is_reserved_word(word, config->syntax.reserved_words)) {
                    for (size_t j = word_start; j < line_len; j++) {
                        colors[j] = 8;
                    }
                }
                free(word);
                }
            }
        }
    }
    return colors;
}

static void print_highlighted(int y, int x, const char* full_line, size_t line_len, size_t start, size_t len, int highlight_pair, EditorConfig* config) {
    int* colors = compute_line_colors(full_line, line_len, highlight_pair, config);
    if (!colors) {
        mvprintw(y, x, "%.*s", (int)(len <= INT_MAX ? len : INT_MAX), &full_line[start]);
        return;
    }
    int current_x = x;
    for (size_t i = 0; i < len; i++) {
        if (current_x > INT_MAX - 1 || current_x >= COLS) break;
        size_t pos = start + i;
        attron(COLOR_PAIR(colors[pos]));
        mvprintw(y, current_x++, "%c", full_line[pos]);
        attroff(COLOR_PAIR(colors[pos]));
    }
    if (colors) free(colors);
}

void draw_initial(WINDOW* win, Buffer* buf, size_t* scroll_row, size_t* scroll_col, size_t cursor_line, size_t cursor_col, int show_line_numbers, int syntax_highlight, size_t* cursor_screen_y, size_t* cursor_screen_x, EditorConfig* config) {
    clear();
    box(win, 0, 0);
    int num_digits = calculate_digits(buffer_num_lines(buf));
    int num_width = show_line_numbers ? num_digits + 1 : 0;
    int max_lines = LINES > 2 ? LINES - 2 : 0;
    for (int i = 0; i < max_lines; i++) {
        size_t line_idx = *scroll_row + (size_t)i;
        if (line_idx >= buffer_num_lines(buf)) break;
        if (show_line_numbers) {
            mvprintw(1 + (int)i, 1, "%*zu ", num_digits, line_idx + 1);
        }
        const char* line = buffer_get_line(buf, line_idx);
        size_t start_col = *scroll_col;
        size_t line_len = strlen(line);
        if (start_col < line_len) {
            size_t print_len = line_len - start_col;
            size_t max_print = COLS > 2 + num_width ? (size_t)(COLS - 2 - num_width) : 0;
            if (print_len > max_print) print_len = max_print;
            print_highlighted(1 + (int)i, 1 + num_width, line, line_len, start_col, print_len, syntax_highlight ? 4 : 1, config);
        }
    }
    // Cursor position
    size_t y_diff = (cursor_line >= *scroll_row) ? cursor_line - *scroll_row : 0;
    int screen_y = 1 + (y_diff <= INT_MAX ? (int)y_diff : INT_MAX);
    size_t x_diff = (cursor_col >= *scroll_col) ? cursor_col - *scroll_col : 0;
    int screen_x = 1 + num_width + (x_diff <= INT_MAX ? (int)x_diff : INT_MAX);
    if (screen_x < 1 + num_width) screen_x = 1 + num_width;
    if (screen_x > COLS - 1) screen_x = COLS - 1;
    *cursor_screen_y = (size_t)screen_y;
    *cursor_screen_x = (size_t)screen_x;
    move(screen_y, screen_x);
    refresh();
}

void draw_update(WINDOW* win, Buffer* buf, size_t* scroll_row, size_t* scroll_col, size_t cursor_line, size_t cursor_col, int show_line_numbers, int syntax_highlight, int search_mode, char* search_buffer, size_t selection_start_line, size_t selection_start_col, size_t selection_end_line, size_t selection_end_col, int selection_active, size_t* cursor_screen_y, size_t* cursor_screen_x, int replace_step, char* replace_buffer, EditorConfig* config, Editor* ed) {
    // Adjust scroll to keep cursor visible
    size_t max_lines = LINES > 2 ? LINES - 2 : 0;
    if (cursor_line < *scroll_row) {
        *scroll_row = cursor_line;
    } else if (cursor_line >= *scroll_row + max_lines) {
        *scroll_row = cursor_line - max_lines + 1;
    }
    if (*scroll_row >= buffer_num_lines(buf) && buffer_num_lines(buf) > max_lines) *scroll_row = buffer_num_lines(buf) - max_lines;
    if (*scroll_row >= buffer_num_lines(buf)) *scroll_row = 0;

    // Clear and redraw border
    clear();
    box(win, 0, 0);

    int num_digits = calculate_digits(buffer_num_lines(buf));
    int num_width = show_line_numbers ? num_digits + 1 : 0;
    for (int i = 0; i < max_lines; i++) {
        size_t line_idx = *scroll_row + (size_t)i;
        if (line_idx >= buffer_num_lines(buf)) break;
        if (show_line_numbers) {
            mvprintw(1 + (int)i, 1, "%*zu ", num_digits, line_idx + 1);
        }
        const char* line = buffer_get_line(buf, line_idx);
        size_t len = strlen(line);
        size_t pos = *scroll_col;
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
                int remaining_width = COLS - 2 - num_width - (x - 1 - num_width);
                if (remaining_width > 0 && print_len > (size_t)remaining_width) print_len = (size_t)remaining_width;
            print_highlighted(1 + (int)i, x, line, len, pos, print_len, syntax_highlight ? 4 : 1, config);
                x += print_len;
                pos += print_len;
            }
        }
        // Selection
        if (pos < sel_end) {
            size_t end = sel_end;
            size_t print_len = end - pos;
            if (print_len > 0) {
                int remaining_width = COLS - 2 - num_width - (x - 1 - num_width);
                if (remaining_width > 0 && print_len > (size_t)remaining_width) print_len = (size_t)remaining_width;
                if (syntax_highlight) attron(COLOR_PAIR(2));
                mvprintw(1 + (int)i, x, "%.*s", (int)(print_len <= INT_MAX ? print_len : INT_MAX), &line[pos]);
                if (syntax_highlight) attroff(COLOR_PAIR(2));
                x += print_len;
                pos += print_len;
            }
        }
        // Clamp x to prevent overflow
        if (x > COLS - 1) x = COLS - 1;
        // After selection
        if (pos < len) {
            size_t print_len = len - pos;
            if (print_len > (size_t)COLS - 2 - num_width - (x - 1 - num_width)) print_len = COLS - 2 - num_width - (x - 1 - num_width);
            print_highlighted(1 + (int)i, x, line, len, pos, print_len, syntax_highlight ? 4 : 1, config);
        }
    }
    // Status bar
    char status_line[COLS + 1];
    // Check for temporary status messages
    char message_prefix[256] = "";
    if (ed->status_message[0] && time(NULL) - ed->status_message_time < 5) {
        snprintf(message_prefix, sizeof(message_prefix), "%s | ", ed->status_message);
    }

    if (replace_step == 1) {
        snprintf(status_line, sizeof(status_line), "Replace search: %s", search_buffer);
    } else if (replace_step == 2) {
        snprintf(status_line, sizeof(status_line), "Replace with: %s", replace_buffer);
    } else if (search_mode) {
        snprintf(status_line, sizeof(status_line), "Search: %s", search_buffer);
    } else {
        // Build status with version, position, time
        char time_str[16] = "";
        if (config->statusbar.show_time) {
            time_t now = time(NULL);
            struct tm* tm_info = localtime(&now);
            if (config->statusbar.time_format == 24) {
                (void)strftime(time_str, sizeof(time_str), "%H:%M", tm_info);
            } else {
                (void)strftime(time_str, sizeof(time_str), "%I:%M%p", tm_info);
            }
        }

        char version_str[32] = "";
        if (config->statusbar.show_version) {
            snprintf(version_str, sizeof(version_str), "%s ", VERSION);
        }

        char pos_str[64];
        snprintf(pos_str, sizeof(pos_str), "Line %zu/%zu Col %zu", cursor_line + 1, buffer_num_lines(buf), cursor_col + 1);

        // Get filename without path
        char filename_display[256] = "";
        if (ed->filename) {
            const char* base = strrchr(ed->filename, '/');
            if (base) base++; else base = ed->filename;
            snprintf(filename_display, sizeof(filename_display), "%s%s", base, ed->file_modified ? "*" : "");
        }

        // Assemble based on style
        if (config->statusbar.style == 1) { // centered
            int total_len = strlen(version_str) + strlen(filename_display) + strlen(pos_str) + strlen(time_str) + 6; // spaces
            int start_pos = (COLS - total_len) / 2;
            if (start_pos < 1) start_pos = 1;
            snprintf(status_line, sizeof(status_line), "%*s%s %s %s %s", start_pos - 1, "", version_str, filename_display, pos_str, time_str);
        } else { // balanced (default)
            int remaining = COLS - 2 - strlen(pos_str) - strlen(filename_display) - 1; // space
            int left_space = remaining / 2;
            int right_space = remaining - left_space;
            char left[COLS/2 + 1];
            char right[COLS/2 + 1];
            if (left_space > 0) {
                snprintf(left, sizeof(left), "%*s%s", left_space - (int)strlen(version_str), "", version_str);
            } else {
                left[0] = '\0';
            }
            if (right_space > 0) {
                snprintf(right, sizeof(right), "%s%*s", time_str, right_space - (int)strlen(time_str), "");
            } else {
                right[0] = '\0';
            }
            snprintf(status_line, sizeof(status_line), "%s%s %s%s", left, filename_display, pos_str, right);
        }
    }

    // Prepend message if any
    if (message_prefix[0]) {
        char temp[COLS + 1];
        snprintf(temp, sizeof(temp), "%s%s", message_prefix, status_line);
        strncpy(status_line, temp, sizeof(status_line) - 1);
        status_line[sizeof(status_line) - 1] = '\0';
    }

    // Ensure null termination and fit in screen
    status_line[COLS - 1] = '\0';
    mvprintw(LINES - 1, 1, "%s", status_line);
    size_t y_diff = (cursor_line >= *scroll_row) ? cursor_line - *scroll_row : 0;
    int screen_y = 1 + (y_diff <= INT_MAX ? (int)y_diff : INT_MAX);
    size_t x_diff = (cursor_col >= *scroll_col) ? cursor_col - *scroll_col : 0;
    int screen_x = 1 + num_width + (x_diff <= INT_MAX ? (int)x_diff : INT_MAX);
    if (screen_x < 1 + num_width) screen_x = 1 + num_width;
    if (screen_x > COLS - 1) screen_x = COLS - 1;
    // Clamp cursor position to screen bounds
    if (screen_y < 1) screen_y = 1;
    if (screen_y > LINES - 1) screen_y = LINES - 1;
    if (screen_x < 1 + num_width) screen_x = 1 + num_width;
    if (screen_x > COLS - 1) screen_x = COLS - 1;

    *cursor_screen_y = (size_t)screen_y;
    *cursor_screen_x = (size_t)screen_x;
    move(screen_y, screen_x);
    refresh();
}
