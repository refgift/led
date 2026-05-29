#pragma once

#include <stdbool.h>
#include "model.h"
#include "editor.h"

/**
 * controller.h - Input handling, undo/redo, and editing operations.
 *
 * This is a "hot" header. It defines the controller layer that sits
 * between raw key input and mutations to the Buffer model.
 *
 * Note: The global undo/redo stacks are currently a legacy artifact.
 * They should eventually move into the Editor struct.
 */

/* Global undo/redo stacks (legacy — see note above) */
extern UndoStack undo_stack;
extern UndoStack redo_stack;

/* === Undo / Redo primitives === */

void push_undo (bool is_insert, int line, int col, char ch);
void push_redo (bool is_insert, int line, int col, char ch);
void undo_operation (Buffer * buf, int *cursor_line, int *cursor_col);
void redo_operation (Buffer * buf, int *cursor_line, int *cursor_col);
void clear_redo (void);
void free_undo (void);

/* === Main input dispatcher === */

/**
 * handle_input - Process a single key and perform the appropriate edit.
 *
 * This is the heart of the controller. It dispatches to specialized
 * handlers for arrows, editing keys, clipboard, search, etc.
 */
int handle_input (int ch, Buffer * buf, int *scroll_row, int *scroll_col,
                  int *cursor_line, int *cursor_col, int *show_line_numbers,
                  char *search_buffer, int *search_mode, char **clipboard,
                  const char *filename, Editor * ed);

/* === Search === */

void search_next (Buffer * buf, int *cursor_line, int *cursor_col,
                  const char *pattern);
