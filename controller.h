#pragma once

#include <stdbool.h>
#include "model.h"
#include "editor.h"

typedef struct {
    bool is_insert;
    int line;
    int col;
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
void push_undo(bool is_insert, int line, int col, char ch);
void push_redo(bool is_insert, int line, int col, char ch);
void undo_operation(Buffer* buf, int* cursor_line, int* cursor_col);
void redo_operation(Buffer* buf, int* cursor_line, int* cursor_col);
void clear_redo(void);
void free_undo(void);
void search_next(Buffer* buf, int* cursor_line, int* cursor_col, const char* pattern);
int handle_input(int ch, Buffer* buf, int* scroll_row, int* scroll_col, int* cursor_line, int* cursor_col, int* show_line_numbers, char* search_buffer, int* search_mode, char* clipboard, const char* filename, Editor *ed);
