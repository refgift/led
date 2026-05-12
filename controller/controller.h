#pragma once

#include <stdbool.h>
#include "model.h"
#include "editor.h"

extern UndoStack undo_stack;
extern UndoStack redo_stack;

void push_undo (bool is_insert, int line, int col, char ch);
void push_redo (bool is_insert, int line, int col, char ch);
void undo_operation (Buffer * buf, int *cursor_line, int *cursor_col);
void redo_operation (Buffer * buf, int *cursor_line, int *cursor_col);
void clear_redo (void);
int handle_input (int ch, Buffer * buf, int *scroll_row, int *scroll_col,
                  int *cursor_line, int *cursor_col, int *show_line_numbers,
                  char *search_buffer, int *search_mode, char **clipboard,
                  const char *filename, Editor * ed);
void search_next (Buffer * buf, int *cursor_line, int *cursor_col,
                  const char *pattern);
void free_undo (void);
