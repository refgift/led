#pragma once

#include <ncurses.h>
#include <time.h>
#include "model.h"
#include "config.h"

/**
 * editor.h - Main editor state and public API.
 *
 * This is a core "hot" header. The Editor struct is the central
 * coordinator that ties together the Buffer (model), undo/redo,
 * configuration, selection, search/replace state, and autosave.
 *
 * Keep this file extremely well documented and low-entropy.
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
};

/**
 * UndoStack - Dynamic array of Changes.
 * Owned by the Editor. Capped at 10,000 entries for safety.
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
};

/* === Public API === */

/** Initialize a new Editor, optionally loading a file from argv. */
void editor_init(Editor* ed, int argc, char* argv[]);

/** Render the current editor state to the given ncurses windows.
 *  frame = border window (stdscr), text = inset derwin(frame) for text */
void editor_draw(WINDOW* frame, WINDOW* text, Editor* ed);
/* Compat: single-window wrapper (frame==text==stdscr legacy) */
void editor_draw_compat(WINDOW* win, Editor* ed);

/** Process one keypress. This is the main input dispatcher. */
void editor_handle_input(Editor* ed, int ch);

/** Release all resources owned by the Editor. */
void editor_cleanup(Editor* ed);

/** Perform an autosave + backup if conditions are met. */
void auto_save(Editor* ed);

/** Set a temporary message that appears in the status bar. */
void set_status_message(Editor* ed, const char* message);