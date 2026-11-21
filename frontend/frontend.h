#pragma once

/**
 * frontend.h - the contract between led and whatever shows it.
 *
 * A front end is the device: a curses terminal, a raw ANSI terminal,
 * a memory grid for tests, later perhaps a window. led draws through
 * three functions with one signature, and reads keys through one more.
 * Nothing above this header knows which front end is linked in.
 *
 * The three view functions, each (x, y, c), x = column, y = row, 0-based:
 *
 *   mark (x, y, c)   the cell holds character c. c is a quantity: the
 *                    character's number in the Unicode ordering.
 *   show (x, y, c)   the cell is seen with quality c (LED_Q_*), and the
 *                    cursor rests there.
 *   hide (x, y, c)   the cell is blanked with c, normally a space.
 *
 * The rule for a redraw: for each cell that changed, mark it and show
 * it, or hide it; finish with a show where the caret belongs. Nothing
 * has to reach the device until read_key is called.
 */

typedef void (*view_fn) (int x, int y, int c);

/* Qualities: what a cell means, not what colour it is. The front end
 * decides how each one looks. */
enum {
  LED_Q_NORMAL = 1,
  LED_Q_SELECTION = 2,
  LED_Q_SEMICOLON = 3,
  LED_Q_LEVEL1 = 4,             /* nesting depth 1 (outermost) .. 4 */
  LED_Q_LEVEL2 = 5,
  LED_Q_LEVEL3 = 6,
  LED_Q_LEVEL4 = 7,
  LED_Q_RESERVED = 8,
  LED_Q_COUNT = 9
};

/* Colours a quality can be given in colorization.conf. The numbers are
 * the usual terminal order, so a front end can use them directly. */
enum {
  LED_COLOR_BLACK = 0,
  LED_COLOR_RED = 1,
  LED_COLOR_GREEN = 2,
  LED_COLOR_YELLOW = 3,
  LED_COLOR_BLUE = 4,
  LED_COLOR_MAGENTA = 5,
  LED_COLOR_CYAN = 6,
  LED_COLOR_WHITE = 7,
  /* Colours 8 to 15, given by number: a terminal with 16 or more
   * colours shows each as the bright form of the colour eight below
   * it. On one with only eight, a front end shows that colour in bold
   * instead. */
  LED_COLOR_BRIGHT = 8,
  /* The terminal's own foreground or background, whatever it is. */
  LED_COLOR_DEFAULT = -1
  /* 16 to 255 are the terminal's extended colours, by number. */
};

typedef struct {
  int fg;
  int bg;
} FrontendStyle;

/* Keys. Plain bytes (0-255) are themselves: control codes, ASCII, and
 * the bytes of UTF-8 text. Everything else is one of these. */
#define LED_KEY_NONE        (-1)   /* nothing read: interrupted or closed */
#define LED_KEY_DOWN        0402
#define LED_KEY_UP          0403
#define LED_KEY_LEFT        0404
#define LED_KEY_RIGHT       0405
#define LED_KEY_HOME        0406
#define LED_KEY_BACKSPACE   0407
#define LED_KEY_F0          0410
#define LED_KEY_F(n)        (LED_KEY_F0 + (n))
#define LED_KEY_DC          0512   /* Delete */
#define LED_KEY_NPAGE       0522
#define LED_KEY_PPAGE       0523
#define LED_KEY_ENTER       0527
#define LED_KEY_BTAB        0541   /* Shift+Tab */
#define LED_KEY_END         0550
#define LED_KEY_RESIZE      0632   /* the device changed size */
#define LED_KEY_SEARCH      01000  /* Ctrl+/ sent as a modified-key sequence */
#define LED_KEY_SEARCH_ALT  01001
#define LED_KEY_WORD_LEFT   01002  /* Ctrl+Left */
#define LED_KEY_WORD_RIGHT  01003  /* Ctrl+Right */

typedef struct {
  /* The view. */
  view_fn show;
  view_fn hide;
  view_fn mark;

  /* The device around it. */
  int (*start) (const FrontendStyle styles[LED_Q_COUNT]);  /* 0 on success */
  void (*stop) (void);
  void (*size) (int *cols, int *rows);
  int (*read_key) (void);       /* sends pending output, then waits */
} Frontend;

/** The front end chosen at build time (make UI=curses | UI=ansi). */
const Frontend *frontend_default (void);

/** The memory front end: a grid tests can read. Always built. */
const Frontend *frontend_memory (void);

/** Select the front end the view draws through. */
void frontend_use (const Frontend *fe);

/** The selected front end, or NULL before frontend_use. */
const Frontend *frontend_current (void);

/* --- Reading the memory front end (tests) --- */

/** Empty the grid and set its size. */
void memory_reset (int cols, int rows);
/** Character number in a cell; ' ' if hidden or never marked. */
int memory_char (int x, int y);
/** Quality a cell was last shown with; 0 if hidden or never shown. */
int memory_quality (int x, int y);
/** Where the last show left the cursor. */
void memory_caret (int *x, int *y);
/** Row y as UTF-8 text, trailing blanks removed. Returns out. */
char *memory_row (int y, char *out, int out_size);
/** Calls made since memory_reset. */
int memory_calls (void);
