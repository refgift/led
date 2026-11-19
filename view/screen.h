#pragma once

/**
 * screen.h - the view's own picture of the screen.
 *
 * A grid of cells, each a character and a quality. The view draws into
 * the grid through panes (rectangles of it). screen_commit compares the
 * grid with what the front end was last told and sends only the cells
 * that differ, through the front end's mark / show / hide.
 */

typedef struct Pane {
  int y0, x0;                   /* top-left corner on the screen */
  int rows, cols;
  int cur_y, cur_x;             /* pane-relative write position */
} Pane;

/** Set the grid size. Forgets what the front end has, so the next
 *  commit sends every cell. */
void screen_resize (int cols, int rows);
int screen_rows (void);
int screen_cols (void);

/** Forget what the front end has without changing size. */
void screen_forget (void);

/** The whole screen as a pane, or NULL while the size is 0 x 0. */
Pane *screen_pane (void);

/** A pane of the screen. Caller frees with pane_free. */
Pane *pane_new (int rows, int cols, int y0, int x0);
void pane_set (Pane *p, int rows, int cols, int y0, int x0);
void pane_free (Pane *p);

/* In every call below a NULL pane means the whole screen. */
int pane_rows (const Pane *p);
int pane_cols (const Pane *p);

/** Quality given to cells written from now on (LED_Q_*). */
void pane_quality (int quality);

/** Write n bytes of UTF-8 at (y, x), cut at the pane's right edge. */
void pane_addnstr (Pane *p, int y, int x, const char *s, int n);
void pane_printf (Pane *p, int y, int x, const char *fmt, ...)
  __attribute__ ((format (printf, 4, 5)));

void pane_move (Pane *p, int y, int x);
/** Blank from the write position to the pane's right edge. */
void pane_clrtoeol (Pane *p);
void pane_erase (Pane *p);
/** Draw a line border around the pane's edge. */
void pane_box (Pane *p);

/** The caret will rest at this pane's write position. */
void pane_noutrefresh (Pane *p);
/** Send every changed cell to the front end, then show the caret. */
void screen_commit (void);
/** pane_noutrefresh then screen_commit. */
void pane_refresh (Pane *p);
