/* Curses front end. The only file that includes ncurses. */
#define _XOPEN_SOURCE_EXTENDED 1
#include "frontend.h"
#include <ncurses.h>
#include <locale.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>

/* led's key numbers are the curses ones, so keys pass straight through.
 * If a curses build ever disagrees, stop here instead of mis-typing. */
_Static_assert (LED_KEY_DOWN == KEY_DOWN && LED_KEY_UP == KEY_UP
                && LED_KEY_LEFT == KEY_LEFT && LED_KEY_RIGHT == KEY_RIGHT
                && LED_KEY_HOME == KEY_HOME && LED_KEY_END == KEY_END
                && LED_KEY_BACKSPACE == KEY_BACKSPACE && LED_KEY_DC == KEY_DC
                && LED_KEY_NPAGE == KEY_NPAGE && LED_KEY_PPAGE == KEY_PPAGE
                && LED_KEY_ENTER == KEY_ENTER && LED_KEY_BTAB == KEY_BTAB
                && LED_KEY_RESIZE == KEY_RESIZE && LED_KEY_F0 == KEY_F0,
                "led key codes must match curses");
_Static_assert (LED_KEY_SEARCH > KEY_MAX, "led's own keys sit above curses");
_Static_assert (LED_COLOR_BLACK == COLOR_BLACK && LED_COLOR_WHITE == COLOR_WHITE
                && LED_COLOR_RED == COLOR_RED && LED_COLOR_BLUE == COLOR_BLUE,
                "led colour numbers must match curses");

static int started;
static attr_t quality_attr[LED_Q_COUNT];        /* A_BOLD where bold stands in for bright */
static int word_left_code, word_right_code;

/* Rows written since the last key was read. led sends every changed
 * cell itself and never asks for a scroll, but curses still notices
 * when rows moved (a line inserted or joined in a file drawn without
 * line numbers) and moves them with the terminal's scroll region. A
 * terminal that gets a scroll region wrong then shows stale rows that
 * curses believes are right, and only repainting every row clears
 * them. So when several rows changed at once, curses is told to write
 * those rows out whole (wredrawln), which leaves it nothing to scroll.
 * One edited row plus the status line stays an ordinary small update. */
#define TOUCH_ROWS 1024
static unsigned char row_touched[TOUCH_ROWS];
static int touched_count, touched_first, touched_last;

static void
touch_row (int y)
{
  if (y < 0 || y >= TOUCH_ROWS || row_touched[y])
    return;
  row_touched[y] = 1;
  if (touched_count++ == 0)
    touched_first = touched_last = y;
  if (y < touched_first)
    touched_first = y;
  if (y > touched_last)
    touched_last = y;
}

static void
write_moved_rows_whole (void)
{
  if (touched_count > 2)
    wredrawln (stdscr, touched_first, touched_last - touched_first + 1);
  for (int y = touched_first; touched_count > 0 && y <= touched_last; y++)
    row_touched[y] = 0;
  touched_count = 0;
}

static void
curses_mark (int x, int y, int c)
{
  touch_row (y);
  wchar_t w[2] = { (wchar_t) c, L'\0' };
  cchar_t cc;
  if (setcchar (&cc, w, A_NORMAL, LED_Q_NORMAL, NULL) == OK)
    mvadd_wch (y, x, &cc);
}

static void
curses_show (int x, int y, int c)
{
  if (c > 0 && c < LED_Q_COUNT)
    mvchgat (y, x, 1, quality_attr[c], (short) c, NULL);
  move (y, x);
}

static void
curses_hide (int x, int y, int c)
{
  touch_row (y);
  mvaddch (y, x, (chtype) (unsigned char) c | COLOR_PAIR (LED_Q_NORMAL));
}

static int
terminal_key (const char *capname)
{
  char *seq = tigetstr ((char *) capname);
  if (seq && seq != (char *) -1)
    {
      int code = key_defined (seq);
      if (code > 0)
        return code;
    }
  return 0;
}

/* Turn one of led's colours into one this terminal has.
 * curs_color(3x): colour values run from 0 to COLORS-1, the first eight
 * are the standard ones, and where a terminal has more, the next eight
 * are the same hues at full strength. On a terminal that stops at
 * eight, bold is how a foreground is made bright; a background cannot
 * be, so it falls back to the plain colour. */
static short
terminal_color (int c, int have_default, int is_fg, attr_t *attr)
{
  if (c == LED_COLOR_DEFAULT)
    return have_default ? -1 : (short) (is_fg ? COLOR_WHITE : COLOR_BLACK);
  if (c < 0)
    return (short) (is_fg ? COLOR_WHITE : COLOR_BLACK);
  if (c < COLORS)
    return (short) c;
  if (c >= LED_COLOR_BRIGHT && c < LED_COLOR_BRIGHT + 8)
    {
      if (is_fg && attr)
        *attr |= A_BOLD;
      return (short) (c - LED_COLOR_BRIGHT);
    }
  return (short) (is_fg ? COLOR_WHITE : COLOR_BLACK);
}

static int
curses_start (const FrontendStyle styles[LED_Q_COUNT])
{
  (void) setlocale (LC_ALL, "");        /* UTF-8 / wide characters */
  if (initscr () == NULL)
    return -1;
  raw ();
  noecho ();
  keypad (stdscr, TRUE);
  scrollok (stdscr, FALSE);
  if (start_color () == ERR)
    {
      endwin ();
      return -1;
    }
  int have_default = (use_default_colors () == OK);
  for (int q = 1; q < LED_Q_COUNT; q++)
    {
      quality_attr[q] = A_NORMAL;
      short fg = terminal_color (styles[q].fg, have_default, 1, &quality_attr[q]);
      short bg = terminal_color (styles[q].bg, have_default, 0, NULL);
      init_pair ((short) q, fg, bg);
    }
  bkgd (COLOR_PAIR (LED_Q_NORMAL));
  word_left_code = terminal_key ("kLFT5");
  word_right_code = terminal_key ("kRIT5");
  /* '/' is not a C0 control character, so foot and xterm
   * modifyOtherKeys send Ctrl+/ as CSI 27 ; 5 ; 47 ~ rather than
   * byte 31. kitty sends CSI 47 ; 5 u. Bind both so one code comes
   * back instead of ESC plus the rest of the sequence. */
  define_key ("\033[27;5;47~", LED_KEY_SEARCH);
  define_key ("\033[47;5u", LED_KEY_SEARCH_ALT);
  /* Bracketed paste: the terminal marks pasted text with these two
   * sequences, so it can be inserted as text and not as typed keys. */
  define_key ("\033[200~", LED_KEY_PASTE_BEGIN);
  define_key ("\033[201~", LED_KEY_PASTE_END);
  putp ("\033[?2004h");
  fflush (stdout);
  started = 1;
  return 0;
}

static void
curses_stop (void)
{
  if (started)
    {
      putp ("\033[?2004l");
      fflush (stdout);
      endwin ();
    }
  started = 0;
}

static void
curses_size (int *cols, int *rows)
{
  int h = 0, w = 0;
  getmaxyx (stdscr, h, w);
  if (cols)
    *cols = w;
  if (rows)
    *rows = h;
}

/* OSC 52: "ESC ] 52 ; c ; <base64> BEL" asks the terminal to put the
 * text on the system clipboard. It works through ssh, since it is the
 * terminal that acts. A terminal that does not know it ignores it. */
static void
curses_clipboard (const char *text, int len)
{
  static const char b64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  if (!started || !text || len <= 0)
    return;
  size_t out_len = 4 * (((size_t) len + 2) / 3);
  char *seq = malloc (out_len + 16);
  if (!seq)
    return;
  size_t n = 0;
  memcpy (seq, "\033]52;c;", 7);
  n = 7;
  for (int i = 0; i < len; i += 3)
    {
      unsigned v = (unsigned char) text[i] << 16;
      if (i + 1 < len)
        v |= (unsigned char) text[i + 1] << 8;
      if (i + 2 < len)
        v |= (unsigned char) text[i + 2];
      seq[n++] = b64[(v >> 18) & 63];
      seq[n++] = b64[(v >> 12) & 63];
      seq[n++] = (i + 1 < len) ? b64[(v >> 6) & 63] : '=';
      seq[n++] = (i + 2 < len) ? b64[v & 63] : '=';
    }
  seq[n++] = '\007';
  /* Straight to the terminal, after what curses has pending, so the
   * sequence is not split by a screen update. */
  refresh ();
  fflush (stdout);
  if (write (STDOUT_FILENO, seq, n) < 0)
    {
      /* nothing to do: the copy inside led has already happened */
    }
  free (seq);
}

static int
curses_read_key (void)
{
  write_moved_rows_whole ();
  int ch = getch ();            /* refreshes the screen first */
  if (ch == ERR)
    return LED_KEY_NONE;
  if (word_left_code > 0 && ch == word_left_code)
    return LED_KEY_WORD_LEFT;
  if (word_right_code > 0 && ch == word_right_code)
    return LED_KEY_WORD_RIGHT;
  return ch;
}

const Frontend *
frontend_default (void)
{
  static const Frontend fe = {
    curses_show, curses_hide, curses_mark,
    curses_start, curses_stop, curses_size, curses_read_key, curses_clipboard
  };
  return &fe;
}
