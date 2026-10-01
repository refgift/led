#ifndef BUFFER_H
#define BUFFER_H

/**
 * model.h - Core text buffer and gap buffer implementation for led.
 *
 * This is one of the hottest headers in the project.
 * The Buffer and GapBuffer abstractions are the foundation of all editing.
 *
 * All public functions here must be clearly documented.
 */

#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include "utils/utils.h"

#define INITIAL_LINES_CAPACITY 10

/**
 * GapBuffer - Efficient text buffer using the gap buffer technique.
 *
 * Inserts and deletes near the cursor are O(1) because the "gap"
 * (empty space) moves with the cursor. This is the fundamental
 * data structure behind all editing in led.
 */
typedef struct {
    char* buffer;
    int buffer_size;   /* total allocated size including gap */
    int gap_start;
    int gap_end;
    int text_len;      /* logical length without the gap */
} GapBuffer;

/**
 * NestingCache - Cached brace/keyword nesting state for one line.
 * Used by the syntax highlighter to avoid re-scanning from the top
 * of the file on every redraw.
 */
typedef struct {
    int valid;          /* Is this cache entry valid? */
    int brace_level;
    int brace_top;
    int brace_stack[256];
    int kw_level;
    int kw_top;
    int kw_stack[100];
} NestingCache;

/**
 * Buffer - The main line-oriented text container.
 *
 * Owns an array of GapBuffers (one per logical line) plus
 * metadata and the syntax highlighting cache.
 */
typedef struct {
    GapBuffer** lines;
    int num_lines;
    int capacity;
    NestingCache* nesting_cache;  /* one entry per logical line */
    bool dirty;                   /* true if any line has been modified since last cache invalidation */

    /* Incremental rendering support (consumed by view, see draw_update) */
    unsigned long edit_generation; /* bumped on every mutation */
    int changed_first;             /* lowest logical line touched since last draw; INT_MAX = none */
    int changed_last;              /* highest logical line touched since last draw; -1 = none */
    int structure_changed;         /* 1 if lines were inserted/deleted/shifted or bulk-replaced */
} Buffer;

/* === GapBuffer operations (low-level) === */

/** Creates an empty gap buffer. Caller must free with gap_buffer_free(). */
GapBuffer* gap_buffer_create(void);
/** Frees the gap buffer and its storage. Passing NULL is safe. */
void gap_buffer_free(GapBuffer* gb);
/** Inserts c at logical position pos. pos must be within [0, text_len]. */
void gap_buffer_insert(GapBuffer* gb, int pos, char c);
/** Deletes the character at logical position pos. */
void gap_buffer_delete(GapBuffer* gb, int pos);
/** Returns the character at logical position pos. */
char gap_buffer_get_char(const GapBuffer* gb, int pos);
/** Returns the logical text (gap excluded). Pointer is owned by gb. */
const char* gap_buffer_get_text(const GapBuffer* gb);
/** Returns the logical length (text_len, gap excluded). */
int gap_buffer_length(const GapBuffer* gb);
/** Moves the gap so it starts at logical position pos. */
void gap_buffer_move_gap(GapBuffer* gb, int pos);
/** Inserts n chars from s at logical position pos. s is borrowed. */
void gap_buffer_insert_many(GapBuffer* gb, int pos, const char* s, int n);

/* === High-level Buffer operations === */

/** Initializes an empty buffer. buf must point to valid storage. */
void buffer_init(Buffer* buf);
/** Frees all lines and metadata. buf itself is borrowed, not freed. */
void buffer_free(Buffer* buf);

/** Loads filename into buf, rejecting files over max_bytes (or the default
 *  when max_bytes <= 0) and truncating lines past max_line_len.
 *  Returns 0 on success, -1 on error. */
int buffer_load_from_file(Buffer* buf, const char* filename, long max_bytes, int max_line_len);

/** Write the buffer to filename.
 *  When the directory is writable, the bytes go to a temp file in that
 *  directory and replace filename via rename, so a crash mid-write leaves
 *  the previous file intact. Existing permission bits are kept.
 *  Returns 0 on success, -1 on error (the previous file is then unchanged
 *  unless the temp file could not be created and the direct write was used). */
int buffer_save_to_file(const Buffer* buf, const char* filename);

/** Returns a newly allocated copy of the line. Caller must free(). */
char* buffer_get_line(const Buffer* buf, int line);

/** Returns the logical length of line, or 0 if line is out of range. */
int buffer_get_line_length(const Buffer* buf, int line);
/** Returns the number of logical lines in buf. */
int buffer_num_lines(const Buffer* buf);
/** Returns the character at (line, col). */
char buffer_get_char(const Buffer* buf, int line, int col);

/** Inserts content as a new line at index line. Returns 0 on success, -1 on error. */
int buffer_insert_line(Buffer* buf, int line, const char* content);
/** Deletes the line at index line. Returns 0 on success, -1 on error. */
int buffer_delete_line(Buffer* buf, int line);
/** Inserts c at (line, col). Returns 0 on success, -1 on error. */
int buffer_insert_char(Buffer* buf, int line, int col, char c);
/** Deletes the character at (line, col). Returns 0 on success, -1 on error. */
int buffer_delete_char(Buffer* buf, int line, int col);
/** Deletes the range between the two points; reversed points are swapped. Returns 0 on success, -1 on error. */
int buffer_delete_range(Buffer* buf, int start_line, int start_col, int end_line, int end_col);
/** Inserts text at (line, col), splitting lines on '\n'. Returns 0 on success, -1 on error. */
int buffer_insert_text(Buffer* buf, int line, int col, const char* text);
/** Replaces every regex match of search_regex with replace_str, all lines. */
void buffer_replace_all(Buffer* buf, const char* search_regex, const char* replace_str);

/** Clears the per-draw change range (changed_first/last/structure_changed).
 *  Does NOT reset edit_generation. The view calls this after consuming
 *  change info at the end of a draw pass. */
void buffer_reset_change_tracking(Buffer* buf);

#endif