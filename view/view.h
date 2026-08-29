#pragma once

#include <ncurses.h>

#include "model.h"
#include "config.h"
#include "editor.h"

/**
 * view.h - ncurses rendering and visual helpers.
 *
 * This is a hot header. It owns all screen output, syntax highlighting,
 * word-wrap logic (when enabled), and cursor positioning math.
 */

/* === Incremental rendering === */

/**
 * ViewFrameState - snapshot of everything that determines the rendered
 * output of one draw pass (minus transient status text). Two consecutive
 * passes with equal states render identical pixels, so the view can skip
 * work. Pure data: safe to construct and compare in unit tests.
 */
typedef struct {
    int valid;
    int term_lines, term_cols;
    unsigned long generation;        /* Buffer.edit_generation at draw time */
    int num_lines;
    int changed_first, changed_last; /* Buffer change range (INT_MAX/-1 if none) */
    int structure_changed;           /* Buffer.structure_changed */
    int scroll_row, scroll_col;
    int cursor_line, cursor_col;
    int show_line_numbers, syntax_highlight;
    int word_wrap;
    int selection_active;
    int sel_sl, sel_sc, sel_el, sel_ec;
} ViewFrameState;

typedef enum {
    LED_PLAN_STATUS_ONLY = 0,  /* rewrite status bar + cursor only */
    LED_PLAN_LINES,            /* repaint first_line..last_line (+ status) */
    LED_PLAN_FULL              /* erase + repaint everything */
} RenderPlanKind;

typedef struct {
    RenderPlanKind kind;
    int first_line, last_line;   /* meaningful when kind == LED_PLAN_LINES */
} RenderPlan;

/**
 * Decide how much of the screen a new frame must paint, given the
 * previously committed frame state and the current one. Pure function.
 */
RenderPlan view_decide(const ViewFrameState* last, const ViewFrameState* cur);

/** Forces the next draw_update to perform a full repaint. */
void view_invalidate(void);

/* === Window management (subwindow isolation) === */

/**
 * view_create_text_window - create the inset text subwindow for frame.
 * frame is typically stdscr; text is derwin(frame, LINES-2, COLS-2, 1,1).
 * Caller owns *text and must delwin() it before endwin().
 * Returns 0 on success, -1 if frame is NULL or allocation fails.
 */
int view_create_text_window(WINDOW* frame, WINDOW** text_out);

/**
 * view_resize_windows - handle terminal resize (KEY_RESIZE).
 * Resizes frame (stdscr) is handled by ncurses; this resizes/moves the
 * text subwindow to remain inset 1,1 and LINES-2 x COLS-2.
 */
void view_resize_windows(WINDOW* frame, WINDOW* text);

/* Border-aware variant: text window geometry depends on show_border */
int view_create_text_window_ex(WINDOW* frame, WINDOW** text_out, int show_border);
void view_resize_windows_ex(WINDOW* frame, WINDOW* text, int show_border);
void view_recreate_text_window(WINDOW* frame, WINDOW** text, int show_border);

/* === Main rendering entry points ===
 * frame = border window (typically stdscr, owns box())
 * text  = inset subwindow (derwin(frame, LINES-2, COLS-2, 1,1), owns all text)
 * Either may be NULL in headless tests (LED_TEST).
 */

void draw_initial(WINDOW* frame, WINDOW* text, Buffer* buf, int* scroll_row, int* scroll_col,
                   int cursor_line, int cursor_col, int show_line_numbers,
                   int syntax_highlight, int* cursor_screen_y, int* cursor_screen_x,
                   EditorConfig* config);

void draw_update(WINDOW* frame, WINDOW* text, Buffer* buf, int* scroll_row, int* scroll_col,
                  int cursor_line, int cursor_col, int show_line_numbers,
                  int syntax_highlight, int search_mode, char* search_buffer,
                  int selection_start_line, int selection_start_col,
                  int selection_end_line, int selection_end_col,
                  int selection_active, int* cursor_screen_y, int* cursor_screen_x,
                  int replace_step, char* replace_buffer,
                  EditorConfig* config, Editor* ed);

/* Back-compat wrappers for headless tests that pass a single win==stdscr */
void draw_initial_compat(WINDOW* win, Buffer* buf, int* scroll_row, int* scroll_col,
                         int cursor_line, int cursor_col, int show_line_numbers,
                         int syntax_highlight, int* cursor_screen_y, int* cursor_screen_x,
                         EditorConfig* config);
void draw_update_compat(WINDOW* win, Buffer* buf, int* scroll_row, int* scroll_col,
                  int cursor_line, int cursor_col, int show_line_numbers,
                  int syntax_highlight, int search_mode, char* search_buffer,
                  int selection_start_line, int selection_start_col,
                  int selection_end_line, int selection_end_col,
                  int selection_active, int* cursor_screen_y, int* cursor_screen_x,
                  int replace_step, char* replace_buffer,
                  EditorConfig* config, Editor* ed);

/* === Utility functions === */

int calculate_digits(int n);

void get_starting_levels(Buffer *buf, int start_line,
                         int *brace_level, int *brace_top, int brace_stack[],
                         int *kw_level, int *kw_top, int kw_stack[],
                         EditorConfig *config);

int visual_column (const char *line, int len, int logical_pos, int tab_width);
