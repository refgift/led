/* Curses front end. The only file that includes ncurses. */
#define _XOPEN_SOURCE_EXTENDED 1
#include "frontend.h"
#include <ncurses.h>
#include <locale.h>
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
static int word_left_code, word_right_code;

static void
curses_mark (int x, int y, int c)
{
  wchar_t w[2] = { (wchar_t) c, L'\0' };
  cchar_t cc;
  if (setcchar (&cc, w, A_NORMAL, LED_Q_NORMAL, NULL) == OK)
    mvadd_wch (y, x, &cc);
}

static void
curses_show (int x, int y, int c)
{
  if (c > 0 && c < LED_Q_COUNT)
    mvchgat (y, x, 1, A_NORMAL, (short) c, NULL);
  move (y, x);
}

static void
curses_hide (int x, int y, int c)
{
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
  for (int q = 1; q < LED_Q_COUNT; q++)
    init_pair ((short) q, (short) styles[q].fg, (short) styles[q].bg);
  bkgd (COLOR_PAIR (LED_Q_NORMAL));
  word_left_code = terminal_key ("kLFT5");
  word_right_code = terminal_key ("kRIT5");
  /* '/' is not a C0 control character, so foot and xterm
   * modifyOtherKeys send Ctrl+/ as CSI 27 ; 5 ; 47 ~ rather than
   * byte 31. kitty sends CSI 47 ; 5 u. Bind both so one code comes
   * back instead of ESC plus the rest of the sequence. */
  define_key ("\033[27;5;47~", LED_KEY_SEARCH);
  define_key ("\033[47;5u", LED_KEY_SEARCH_ALT);
  started = 1;
  return 0;
}

static void
curses_stop (void)
{
  if (started)
    endwin ();
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

static int
curses_read_key (void)
{
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
    curses_start, curses_stop, curses_size, curses_read_key
  };
  return &fe;
}
