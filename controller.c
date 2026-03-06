#include "controller.h"
#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include <ctype.h>

Change undo_stack[MAX_UNDO];
int undo_top = -1;

static char* my_strdup(const char* s) {
    size_t len = strlen(s);
    char* d = malloc(len + 1);
    if (d) memcpy(d, s, len + 1);
    return d;
}

void push_undo(bool is_insert, size_t line, size_t col, char ch) {
    if (undo_top < MAX_UNDO - 1) {
        undo_top++;
        undo_stack[undo_top].is_insert = is_insert;
        undo_stack[undo_top].line = line;
        undo_stack[undo_top].col = col;
        undo_stack[undo_top].ch = ch;
    }
}

void undo_operation(Buffer* buf, size_t* cursor_line, size_t* cursor_col) {
    if (undo_top >= 0) {
        Change c = undo_stack[undo_top--];
        if (c.is_insert) {
            buffer_delete_char(buf, c.line, c.col);
            if (*cursor_line == c.line && *cursor_col > c.col) (*cursor_col)--;
        } else {
            buffer_insert_char(buf, c.line, c.col, c.ch);
            if (*cursor_line == c.line && *cursor_col >= c.col) (*cursor_col)++;
        }
    }
}

void search_next(Buffer* buf, size_t* cursor_line, size_t* cursor_col, const char* pattern) {
    regex_t regex;
    if (regcomp(&regex, pattern, REG_EXTENDED) != 0) return;
    int found = 0;
    for (size_t l = *cursor_line; l < buffer_num_lines(buf) && !found; l++) {
        const char* line = buffer_get_line(buf, l);
        regmatch_t match;
        const char* search_line = line;
        int flags = 0;
        if (l == *cursor_line) {
            search_line = line + *cursor_col;
            flags = REG_NOTBOL;
        }
        if (regexec(&regex, search_line, 1, &match, flags) == 0) {
            *cursor_line = l;
            *cursor_col = (l == *cursor_line ? *cursor_col : 0) + match.rm_so;
            found = 1;
        }
    }
    regfree(&regex);
}

void handle_input(int ch, Buffer* buf, size_t* scroll_row, size_t* scroll_col, size_t* cursor_line, size_t* cursor_col, int* show_line_numbers, char* search_buffer, int* search_mode, char** clipboard, const char* filename, size_t* selection_start_line, size_t* selection_start_col, size_t* selection_end_line, size_t* selection_end_col, int* selection_active) {
    if (*search_mode) {
        if (ch == '\n' || ch == 13) {
            if (strlen(search_buffer) > 0) {
                search_next(buf, cursor_line, cursor_col, search_buffer);
            }
            *search_mode = 0;
            search_buffer[0] = 0;
        } else if (ch == 27) { // ESC
            *search_mode = 0;
            search_buffer[0] = 0;
        } else if (ch >= 32 && ch <= 126) {
            size_t len = strlen(search_buffer);
            if (len < 255) {
                search_buffer[len] = (char)ch;
                search_buffer[len + 1] = 0;
            }
        } else if (ch == 127 || ch == 8) {
            size_t len = strlen(search_buffer);
            if (len > 0) search_buffer[len - 1] = 0;
        }
    } else {
        switch (ch) {
            case KEY_LEFT:
                if (*cursor_col > 0) {
                    (*cursor_col)--;
                } else if (*cursor_line > 0) {
                    (*cursor_line)--;
                    *cursor_col = buffer_get_line_length(buf, *cursor_line);
                }
                break;
            case KEY_RIGHT:
                if (*cursor_col < buffer_get_line_length(buf, *cursor_line)) {
                    (*cursor_col)++;
                } else if (*cursor_line < buffer_num_lines(buf) - 1) {
                    (*cursor_line)++;
                    *cursor_col = 0;
                }
                break;
            case KEY_UP:
                if (*cursor_line > 0) {
                    (*cursor_line)--;
                    if (*cursor_col > buffer_get_line_length(buf, *cursor_line)) {
                        *cursor_col = buffer_get_line_length(buf, *cursor_line);
                    }
                }
                break;
            case KEY_DOWN:
                if (*cursor_line < buffer_num_lines(buf) - 1) {
                    (*cursor_line)++;
                    if (*cursor_col > buffer_get_line_length(buf, *cursor_line)) {
                        *cursor_col = buffer_get_line_length(buf, *cursor_line);
                    }
                }
                break;
            case KEY_HOME:
                *cursor_col = 0;
                break;
            case KEY_END:
                *cursor_col = buffer_get_line_length(buf, *cursor_line);
                break;
            case KEY_PPAGE:
                if (*scroll_row >= (size_t)LINES - 2) *scroll_row -= LINES - 2;
                else *scroll_row = 0;
                if (*cursor_line > *scroll_row + (size_t)LINES - 3) *cursor_line = *scroll_row + LINES - 3;
                break;
            case KEY_NPAGE:
                *scroll_row += LINES - 2;
                if (*scroll_row > buffer_num_lines(buf) - ((size_t)LINES - 2)) *scroll_row = buffer_num_lines(buf) > (size_t)LINES - 2 ? buffer_num_lines(buf) - (LINES - 2) : 0;
                if (*cursor_line < *scroll_row) *cursor_line = *scroll_row;
                break;
            case KEY_F(2):
                *show_line_numbers = !*show_line_numbers;
                break;
            case KEY_BACKSPACE:
            case 127: // Delete key
                if (*cursor_col > 0) {
                    char deleted = buffer_get_char(buf, *cursor_line, *cursor_col - 1);
                    push_undo(false, *cursor_line, *cursor_col - 1, deleted);
                    buffer_delete_char(buf, *cursor_line, --*cursor_col);
                } else if (*cursor_line > 0) {
                    size_t prev_len = buffer_get_line_length(buf, *cursor_line - 1);
                    push_undo(false, *cursor_line - 1, prev_len, '\n');
                    (*cursor_line)--;
                    *cursor_col = prev_len;
                    buffer_delete_char(buf, *cursor_line, prev_len);
                }
                break;
            case 19: // Ctrl+S to save
                if (filename) buffer_save_to_file(buf, filename);
                break;
            case 26: // Ctrl+Z to undo
                undo_operation(buf, cursor_line, cursor_col);
                break;
            case 3: // Ctrl+C to copy
                if (*clipboard) free(*clipboard);
                if (*selection_active) {
                    // normalize
                    size_t sl = *selection_start_line, sc = *selection_start_col, el = *selection_end_line, ec = *selection_end_col;
                    if (sl > el || (sl == el && sc > ec)) {
                        size_t t = sl; sl = el; el = t;
                        t = sc; sc = ec; ec = t;
                    }
                    // build string
                    size_t total = 0;
                    for (size_t l = sl; l <= el; l++) {
                        const char* line = buffer_get_line(buf, l);
                        size_t len = strlen(line);
                        size_t s = (l == sl) ? sc : 0;
                        size_t e = (l == el) ? ec : len;
                        total += e - s + (l < el ? 1 : 0);
                    }
                    *clipboard = malloc(total + 1);
                    char* p = *clipboard;
                    for (size_t l = sl; l <= el; l++) {
                        const char* line = buffer_get_line(buf, l);
                        size_t len = strlen(line);
                        size_t s = (l == sl) ? sc : 0;
                        size_t e = (l == el) ? ec : len;
                        memcpy(p, &line[s], e - s);
                        p += e - s;
                        if (l < el) *p++ = '\n';
                    }
                    *p = 0;
                } else {
                    *clipboard = my_strdup(buffer_get_line(buf, *cursor_line));
                }
                break;
            case 24: // Ctrl+X to cut
                if (*clipboard) free(*clipboard);
                if (*selection_active) {
                    // normalize
                    size_t sl = *selection_start_line, sc = *selection_start_col, el = *selection_end_line, ec = *selection_end_col;
                    if (sl > el || (sl == el && sc > ec)) {
                        size_t t = sl; sl = el; el = t;
                        t = sc; sc = ec; ec = t;
                    }
                    // build string
                    size_t total = 0;
                    for (size_t l = sl; l <= el; l++) {
                        const char* line = buffer_get_line(buf, l);
                        size_t len = strlen(line);
                        size_t s = (l == sl) ? sc : 0;
                        size_t e = (l == el) ? ec : len;
                        total += e - s + (l < el ? 1 : 0);
                    }
                    *clipboard = malloc(total + 1);
                    char* p = *clipboard;
                    for (size_t l = sl; l <= el; l++) {
                        const char* line = buffer_get_line(buf, l);
                        size_t len = strlen(line);
                        size_t s = (l == sl) ? sc : 0;
                        size_t e = (l == el) ? ec : len;
                        memcpy(p, &line[s], e - s);
                        p += e - s;
                        if (l < el) *p++ = '\n';
                    }
                    *p = 0;
                    // delete range
                    buffer_delete_range(buf, sl, sc, el, ec);
                    // adjust cursor
                    *cursor_line = sl;
                    *cursor_col = sc;
                    *selection_active = 0;
                } else {
                    *clipboard = my_strdup(buffer_get_line(buf, *cursor_line));
                    buffer_delete_line(buf, *cursor_line);
                }
                break;
            case 22: // Ctrl+V to paste
                if (*clipboard) {
                    buffer_insert_text(buf, *cursor_line, *cursor_col, *clipboard);
                    // move cursor to end
                    const char* p = *clipboard;
                    while (*p) {
                        if (*p == '\n') {
                            (*cursor_line)++;
                            *cursor_col = 0;
                        } else {
                            (*cursor_col)++;
                        }
                        p++;
                    }
                }
                break;
            case 31: // Ctrl+/
                *search_mode = 1;
                search_buffer[0] = 0;
                break;
            case 1: // Ctrl+A to toggle selection
                if (*selection_active) {
                    *selection_active = 0;
                } else {
                    *selection_start_line = *cursor_line;
                    *selection_start_col = *cursor_col;
                    *selection_end_line = *cursor_line;
                    *selection_end_col = *cursor_col;
                    *selection_active = 1;
                }
                break;
            default:
                if (ch == '\n' || (ch >= 32 && ch <= 126)) { // Printable chars or newline
                    if (ch == '\n') {
                        buffer_insert_char(buf, *cursor_line, *cursor_col, '\n');
                        push_undo(true, *cursor_line, *cursor_col, '\n');
                        (*cursor_line)++;
                        *cursor_col = 0;
                    } else {
                        buffer_insert_char(buf, *cursor_line, *cursor_col, (char)ch);
                        push_undo(true, *cursor_line, *cursor_col, (char)ch);
                        (*cursor_col)++;
                    }
                }
                break;
        }
    }
    if (*selection_active) {
        *selection_end_line = *cursor_line;
        *selection_end_col = *cursor_col;
    }
    // Adjust scroll
    if (*cursor_line < *scroll_row) *scroll_row = *cursor_line;
    else if (*cursor_line >= *scroll_row + (size_t)LINES - 2) *scroll_row = *cursor_line - (LINES - 3);
    if (*cursor_col < *scroll_col) *scroll_col = *cursor_col;
    else if (*cursor_col >= *scroll_col + (size_t)COLS - 2) *scroll_col = *cursor_col - (COLS - 3);
    if (*scroll_row > buffer_num_lines(buf) - ((size_t)LINES - 2)) *scroll_row = buffer_num_lines(buf) > (size_t)LINES - 2 ? buffer_num_lines(buf) - (LINES - 2) : 0;
}