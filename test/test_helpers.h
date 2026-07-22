#pragma once

#include "model.h"
#include "editor.h"

/**
 * test_helpers.h - Common utilities for the led test suite.
 *
 * Goal: Reduce massive duplication across the large test_*.c files.
 * This is infrastructure to make the cold test code less cold.
 */

typedef struct {
    Buffer buf;
    int scroll_row;
    int scroll_col;
    int cursor_line;
    int cursor_col;
    int show_line_numbers;
    char search_buffer[256];
    int search_mode;
    char *clipboard;
    const char *filename;
    Editor ed;
} TestContext;

/** Initialize a fresh test context with an empty buffer and clean undo state. */
void test_init(TestContext *ctx);

/** Clean up a test context (frees buffer + clipboard + undo stacks). */
void test_cleanup(TestContext *ctx);

/** Reset only the undo/redo state for the given Editor (useful between sub-tests). */
void test_reset_undo(Editor *ed);

/**
 * Convenience wrapper for handle_input using the TestContext.
 * Many tests repeat the long argument list; this reduces boilerplate.
 */
int test_handle_input(TestContext *ctx, int ch);
