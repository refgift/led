#pragma once

#include <stdbool.h>
#include "model.h"

typedef struct {
    bool is_insert;
    size_t line;
    size_t col;
    char ch;
} Change;

typedef struct {
    Change* changes;
    int count;
    int capacity;
} UndoStack;

extern UndoStack undo_stack;
extern UndoStack redo_stack;

void init_undo(void);
void push_undo(bool is_insert, size_t line, size_t col, char ch);
void push_redo(bool is_insert, size_t line, size_t col, char ch);
void undo_operation(Buffer* buf, size_t* cursor_line, size_t* cursor_col);
void redo_operation(Buffer* buf, size_t* cursor_line, size_t* cursor_col);
void clear_redo(void);
void free_undo(void);
void search_next(Buffer* buf, size_t* cursor_line, size_t* cursor_col, const char* pattern);
int handle_input(int ch, Buffer* buf, size_t* scroll_row, size_t* scroll_col, size_t* cursor_line, size_t* cursor_col, int* show_line_numbers, char* search_buffer, int* search_mode, char** clipboard, const char* filename, size_t* selection_start_line, size_t* selection_start_col, size_t* selection_end_line, size_t* selection_end_col, int* selection_active);