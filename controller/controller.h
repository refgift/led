#pragma once

#include <stdbool.h>
#include "model.h"
#include "editor.h"

/**
 * controller.h - Input handling, undo/redo, and editing operations.
 *
 * This is a "hot" header. It defines the controller layer that sits
 * between raw key input and mutations to the Buffer model.
 */

/* === Undo / Redo primitives (operate on per-Editor stacks) === */

void push_undo (UndoStack *stack, bool is_insert, int line, int col, char ch);
void push_redo (UndoStack *stack, bool is_insert, int line, int col, char ch);
void undo_operation (Buffer * buf, UndoStack *undo, UndoStack *redo, int *cursor_line, int *cursor_col);
void redo_operation (Buffer * buf, UndoStack *undo, UndoStack *redo, int *cursor_line, int *cursor_col);
void clear_redo (UndoStack *redo);
void free_undo_stacks (UndoStack *undo, UndoStack *redo);

/* === symbol / macro / token ===
 *
 * Every edit goes through one of three functions with one signature.
 * They act on the Editor given to controller_attach, and return 0, or
 * -1 when nothing is attached.
 *
 *   symbol (action, value)   one action: a key press.
 *   macro  (action, value)   an action made of several symbols. Whatever
 *                            it records is one undo step.
 *   token  (action, value)   keys collected into a unit before anything
 *                            happens: a prompt. Open it, feed it one
 *                            character at a time, finish or cancel it.
 */

/** symbol actions. value is the byte for S_INSERT, a repeat count for
 *  motions and deletions (0 means 1), unused otherwise. */
enum {
  S_INSERT = 1,       /* insert byte `value` at the cursor */
  S_NEWLINE,          /* split the line at the cursor */
  S_LEFT, S_RIGHT, S_UP, S_DOWN,
  S_HOME, S_END,      /* start / end of the line */
  S_TOP, S_BOTTOM,    /* start / end of the buffer */
  S_PAGE_UP, S_PAGE_DOWN,
  S_WORD_LEFT, S_WORD_RIGHT,
  S_MATCH_BRACE,
  S_BACKSPACE, S_DELETE,
  S_UNDO, S_REDO,
  S_SELECT_ALL, S_COPY,
  S_SAVE,
  S_TOGGLE_NUMBERS, S_TOGGLE_WRAP, S_TOGGLE_BORDER
};

/** macro actions. value is a repeat count (0 means 1). */
enum {
  M_ENTER = 1,        /* newline, then the indent of the line above */
  M_TAB,              /* a tab, or spaces to the next tab stop */
  M_OUTDENT,
  M_COMMENT,          /* toggle a // comment on this line */
  M_DELETE_WORD,      /* backspace to the start of the word */
  M_CUT,
  M_PASTE
};

/** token actions. */
enum {
  T_BEGIN = 1,        /* open a prompt; value is its kind, K_* */
  T_CHAR,             /* add character `value` to the open prompt */
  T_ERASE,            /* remove its last character */
  T_DONE,             /* act on what was collected */
  T_CANCEL            /* close the prompt without acting */
};

/** token kinds: the value of T_BEGIN. */
enum {
  K_SEARCH = 1,       /* regex; T_DONE finds the next match, stays open */
  K_REPLACE,          /* pattern, T_DONE, replacement, T_DONE */
  K_GOTO,             /* "line" or "line:col" */
  K_META,             /* Esc, then one key: b, f, or Backspace */
  K_PASTE             /* text the terminal pastes: T_CHAR per byte, then
                         T_DONE inserts it as it is, as one undo step */
};

int symbol (int action, int value);
int macro (int action, int value);
int token (int action, int value);

/** The Editor that symbol, macro and token act on. NULL detaches. */
void controller_attach (Editor *ed);

/** After handle_input: 1 if the key went to the text (a symbol, a macro,
 *  the search prompt, or Esc), 0 if a prompt or a display toggle took it. */
int controller_key_was_plain (void);

/* === Keys === */

/**
 * handle_input - Process a single key and perform the appropriate edit.
 *
 * Turns the key into one call of symbol, macro, or token, acting on the
 * pieces given here instead of an attached Editor.
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
