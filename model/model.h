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
} Buffer;

/* === GapBuffer operations (low-level) === */

GapBuffer* gap_buffer_create(void);
void gap_buffer_free(GapBuffer* gb);
void gap_buffer_insert(GapBuffer* gb, int pos, char c);
void gap_buffer_delete(GapBuffer* gb, int pos);
char gap_buffer_get_char(const GapBuffer* gb, int pos);
const char* gap_buffer_get_text(const GapBuffer* gb);
int gap_buffer_length(const GapBuffer* gb);
void gap_buffer_move_gap(GapBuffer* gb, int pos);
void gap_buffer_insert_many(GapBuffer* gb, int pos, const char* s, int n);

/* === High-level Buffer operations === */

void buffer_init(Buffer* buf);
void buffer_free(Buffer* buf);

int buffer_load_from_file(Buffer* buf, const char* filename, long max_bytes, int max_line_len);
int buffer_save_to_file(const Buffer* buf, const char* filename);

/** Returns a newly allocated copy of the line. Caller must free(). */
char* buffer_get_line(const Buffer* buf, int line);

int buffer_get_line_length(const Buffer* buf, int line);
int buffer_num_lines(const Buffer* buf);
char buffer_get_char(const Buffer* buf, int line, int col);

int buffer_insert_line(Buffer* buf, int line, const char* content);
int buffer_delete_line(Buffer* buf, int line);
int buffer_insert_char(Buffer* buf, int line, int col, char c);
int buffer_delete_char(Buffer* buf, int line, int col);
int buffer_delete_range(Buffer* buf, int start_line, int start_col, int end_line, int end_col);
int buffer_insert_text(Buffer* buf, int line, int col, const char* text);
void buffer_replace_all(Buffer* buf, const char* search_regex, const char* replace_str);

#endif