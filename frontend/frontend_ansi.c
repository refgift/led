/* ANSI front end: escape codes straight to the terminal, no curses.
 *
 * NOT WRITTEN YET. This file exists so `make UI=ansi` links without
 * ncurses, which proves nothing above frontend.h depends on it.
 *
 * To finish it:
 *   start     termios raw mode, alternate screen (ESC [ ? 1049 h),
 *             remember the styles, install a SIGWINCH handler.
 *   mark      remember c for the cell (a small pending buffer is enough,
 *             since show for the same cell follows).
 *   show      append "ESC [ y+1 ; x+1 H", the SGR colours for quality c,
 *             and the character as UTF-8 to an output buffer.
 *   hide      append the cursor move and c with the normal colours.
 *   read_key  write the output buffer in one write(), then read bytes
 *             and turn sequences such as ESC [ A into LED_KEY_UP.
 *   size      ioctl (TIOCGWINSZ).
 *   stop      leave the alternate screen, restore termios.
 */
#include "frontend.h"
#include <stdio.h>

static void
ansi_cell (int x, int y, int c)
{
  (void) x;
  (void) y;
  (void) c;
}

static int
ansi_start (const FrontendStyle styles[LED_Q_COUNT])
{
  (void) styles;
  fprintf (stderr, "led: the ANSI front end is not written yet. "
           "Build with `make UI=curses`.\n");
  return -1;
}

static void
ansi_stop (void)
{
}

static void
ansi_size (int *cols, int *rows)
{
  if (cols)
    *cols = 80;
  if (rows)
    *rows = 24;
}

static int
ansi_read_key (void)
{
  return LED_KEY_NONE;
}

const Frontend *
frontend_default (void)
{
  static const Frontend fe = {
    ansi_cell, ansi_cell, ansi_cell,
    ansi_start, ansi_stop, ansi_size, ansi_read_key
  };
  return &fe;
}
