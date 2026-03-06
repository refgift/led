#pragma once

#include <stdbool.h>
#include "model.h"

typedef struct {
    bool is_insert;
    size_t line;
    size_t col;
    char ch;
} Change;

#define MAX_UNDO 100

extern Change undo_stack[MAX_UNDO];
extern int undo_top;

void push_undo(bool is_insert, size_t line, size_t col, char ch);
void undo_operation(Buffer* buf, size_t* cursor_line, size_t* cursor_col);
void search_next(Buffer* buf, size_t* cursor_line, size_t* cursor_col, const char* pattern);
void handle_input(int ch, Buffer* buf, size_t* scroll_row, size_t* scroll_col, size_t* cursor_line, size_t* cursor_col, int* show_line_numbers, char* search_buffer, int* search_mode, char** clipboard, const char* filename);