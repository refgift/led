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

/* === Undo / Redo primitives (operate on per-Editor stacks) === */

void push_undo (UndoStack *stack, bool is_insert, int line, int col, char ch);
void push_redo (UndoStack *stack, bool is_insert, int line, int col, char ch);
void undo_operation (Buffer * buf, UndoStack *undo, UndoStack *redo, int *cursor_line, int *cursor_col);
void redo_operation (Buffer * buf, UndoStack *undo, UndoStack *redo, int *cursor_line, int *cursor_col);
void clear_redo (UndoStack *redo);
void free_undo_stacks (UndoStack *undo, UndoStack *redo);

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

/** Find the next regex match at or after the cursor, skipping a match that
 *  starts exactly on the cursor. Wraps to the top of the buffer once.
 *  Returns 1 if found, 2 if found after wrapping, 0 if absent, -1 if the
 *  pattern is empty, too long, or not a valid extended regex. */
int search_next (Buffer * buf, int *cursor_line, int *cursor_col,
                 const char *pattern);
