#pragma once

#include "frontend/frontend.h"
#include <time.h>
#include <limits.h>
#include "model.h"
#include "config.h"
#include "view/screen.h"

/**
 * editor.h - The editor system.
 *
 * This is a core header. The Editor struct is the central
 * coordinator that ties together the model subsystem's undo/redo,
 * configuration, selection, search/replace state, and autosave.
 * with the view and controller subsystems.
 *
 */

/* Opaque forward declarations */
typedef struct UndoStack UndoStack;
typedef struct Change Change;

/**
 * Change - A single entry in the undo/redo stack.
 * Records enough information to reverse or re-apply an edit.
 */
struct Change {
    bool is_insert;   /* true = this was an insertion, false = deletion */
    int line;
    int col;
    char ch;          /* the character involved (or '\n' for line breaks) */
    bool chained;     /* undo/redo together with the entry below it:
                         the later bytes of one character, or of one
                         cut, paste, or other multi-byte operation */
};

/**
 * UndoStack - Dynamic array of Changes.
 * Owned by the Editor. Capped at 1,000,000 entries; when full, the
 * oldest half is forgotten.
 */
struct UndoStack {
    Change* changes;
    int count;
    int capacity;
};

/* Forward declaration of the main Editor type */
typedef struct Editor Editor;

/**
 * Editor - The central state of one editing session.
 *
 * This struct is deliberately large because it owns almost everything
 * needed for an interactive editing experience in one place.
 */
struct Editor {
    /* Core data model */
    Buffer model;

    /* Undo / Redo */
    UndoStack undo_stack;
    UndoStack redo_stack;

    /* Viewport / scrolling state (logical, not visual) */
    int scroll_row;
    int scroll_col;

    /* Display toggles (F2/F3 etc.) */
    int show_line_numbers;
    int syntax_highlight;

    /* Cursor position in the model (logical) */
    int cursor_line;
    int cursor_col;

    /* Selection (also logical coordinates) */
    int selection_start_line;
    int selection_start_col;
    int selection_end_line;
    int selection_end_col;
    int selection_active;

    /* Search / Replace mode state */
    char search_buffer[256];
    int search_mode;
    char replace_buffer[256];
    int replace_step;

    /* Clipboard for cut/copy/paste */
    char* clipboard;

    /* Text the terminal is pasting, collected until it is complete. */
    int paste_mode;
    char* paste_buf;
    int paste_len;
    int paste_cap;

    /* Current file */
    const char* filename;

    /* Full configuration (colors, syntax rules, autosave, etc.) */
    EditorConfig config;

    /* Auto-save subsystem */
    int unsaved_keystrokes;
    int auto_save_threshold;   /* edits before triggering autosave */
    int auto_save_timeout;     /* seconds of inactivity */
    time_t last_save_time;
    int backup_count;

    /* Transient UI feedback */
    char status_message[256];
    time_t status_message_time;

    /* Dirty flag for the status bar */
    int file_modified;

    /* Word-wrap related (currently mostly vestigial) */
    int total_visual_lines;

    /* Timing / double-key detection */
    int last_key_us;
    int prev_key;   /* used for double-Home / double-End */

    /* Go-to-line prompt (Ctrl+G). goto_buffer holds "line" or "line:col". */
    int goto_mode;
    char goto_buffer[64];

    /* Set after Ctrl+Q when the buffer is modified. A second Ctrl+Q quits. */
    int quit_armed;

    /* Esc was pressed; the next key is a word motion (b / f / Backspace). */
    int meta_pending;

    /* Rows the front end shows; Page Up / Page Down move by this. */
    int page_rows;

    /* Key codes for Ctrl+Left / Ctrl+Right. 0 if unknown. */
    int key_word_left;
    int key_word_right;

    /* Set when the buffer is not the whole file (load refused, or long
     * lines were cut). Ctrl+S and autosave refuse to write while set. */
    int save_blocked;

    /* Owns the filename when it was peeled from "path:line". */
    char filename_storage[PATH_MAX];
};

/* === Public API === */

/** Initialize a new Editor, optionally loading a file from argv. */
void editor_init(Editor* ed, int argc, char* argv[]);

/** Render the current editor state to the given ncurses windows.
 *  frame = the whole screen (owns the border), text = the pane for text */
void editor_draw(Pane* frame, Pane* text, Editor* ed);
/* Compat: single-window wrapper (frame==text==stdscr legacy) */
void editor_draw_compat(Pane* win, Editor* ed);

/** Process one keypress. Returns 1 when the session should end (Ctrl+Q). */
int editor_handle_input(Editor* ed, int ch);

/** Point the word-motion keys at the front end's Ctrl+Left / Ctrl+Right
 *  codes. The front end does the terminal-specific binding itself. */
void editor_bind_terminal_keys(Editor* ed);

/** "+142" -> 142. Returns 1 on success. */
int led_parse_plus_line(const char* arg, int* line);

/** Peel a trailing ":line" or ":line:col" (both 1-based) off arg.
 *  Does not look at the filesystem. Returns 1 when a suffix was peeled. */
int led_parse_file_location(const char* arg, char* path, size_t path_sz,
                            int* line, int* col);

/** Move the cursor to a 1-based "line" or "line:col". Returns 0 on success. */
int editor_apply_goto(Editor* ed, const char* spec);

/** Release all resources owned by the Editor. */
void editor_cleanup(Editor* ed);

/** Write "<filename>.recovery" if the buffer has unsaved changes.
 *  Called when the terminal goes away. Returns 0 if a file was written. */
int editor_write_recovery(Editor* ed);

/** Write the undo history beside the system's temporary files, so it is
 *  there when the file is next opened. Call after a successful save. */
void editor_keep_history(Editor* ed);

/** Perform an autosave + backup if conditions are met. */
void auto_save(Editor* ed);

/** Set a temporary message that appears in the status bar. */
void set_status_message(Editor* ed, const char* message);