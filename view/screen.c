#include "screen.h"
#include "frontend/frontend.h"
#include "utils/utils.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

typedef struct {
  int code;                     /* 0 = right half of a wide character */
  int quality;
} Cell;

#define UNKNOWN (-1)

static Cell *want;              /* what the view has drawn */
static Cell *sent;              /* what the front end was last told */
static int g_rows, g_cols;
static Pane whole;
static int caret_y, caret_x;
static int cur_quality = LED_Q_NORMAL;

int
screen_rows (void)
{
  return g_rows;
}

int
screen_cols (void)
{
  return g_cols;
}

void
screen_forget (void)
{
  for (int i = 0; sent && i < g_rows * g_cols; i++)
    sent[i].code = UNKNOWN;
}

void
screen_resize (int cols, int rows)
{
  free (want);
  free (sent);
  want = sent = NULL;
  g_cols = cols > 0 ? cols : 0;
  g_rows = rows > 0 ? rows : 0;
  whole.y0 = whole.x0 = 0;
  whole.rows = g_rows;
  whole.cols = g_cols;
  whole.cur_y = whole.cur_x = 0;
  caret_y = caret_x = 0;
  if (g_cols == 0 || g_rows == 0)
    return;
  size_t n = (size_t) g_cols * (size_t) g_rows;
  want = xmalloc (n * sizeof (Cell));
  sent = xmalloc (n * sizeof (Cell));
  for (size_t i = 0; i < n; i++)
    {
      want[i].code = ' ';
      want[i].quality = LED_Q_NORMAL;
      sent[i].code = UNKNOWN;
      sent[i].quality = 0;
    }
}

Pane *
screen_pane (void)
{
  return (g_rows > 0 && g_cols > 0) ? &whole : NULL;
}

Pane *
pane_new (int rows, int cols, int y0, int x0)
{
  Pane *p = xmalloc (sizeof (Pane));
  pane_set (p, rows, cols, y0, x0);
  return p;
}

void
pane_set (Pane *p, int rows, int cols, int y0, int x0)
{
  if (!p)
    return;
  p->rows = rows > 0 ? rows : 0;
  p->cols = cols > 0 ? cols : 0;
  p->y0 = y0;
  p->x0 = x0;
  p->cur_y = p->cur_x = 0;
}

void
pane_free (Pane *p)
{
  free (p);
}

static Pane *
real (Pane *p)
{
  return p ? p : &whole;
}

int
pane_rows (const Pane *p)
{
  return p ? p->rows : g_rows;
}

int
pane_cols (const Pane *p)
{
  return p ? p->cols : g_cols;
}

void
pane_quality (int quality)
{
  cur_quality = (quality > 0 && quality < LED_Q_COUNT) ? quality : LED_Q_NORMAL;
}

/* Set one cell by pane coordinates. Outside the pane or the screen is
 * silently dropped: the grid is the clipping. */
static void
put (Pane *p, int y, int x, int code, int quality)
{
  if (y < 0 || x < 0 || y >= p->rows || x >= p->cols)
    return;
  int sy = p->y0 + y;
  int sx = p->x0 + x;
  if (!want || sy < 0 || sx < 0 || sy >= g_rows || sx >= g_cols)
    return;
  Cell *c = &want[sy * g_cols + sx];
  c->code = code;
  c->quality = quality;
}

void
pane_addnstr (Pane *pp, int y, int x, const char *s, int n)
{
  Pane *p = real (pp);
  if (!s)
    return;
  int i = 0;
  while (i < n && s[i])
    {
      int clen = utf8_char_len (s + i, n - i);
      wchar_t wc = 0;
      mbstate_t st;
      memset (&st, 0, sizeof st);
      size_t got = mbrtowc (&wc, s + i, (size_t) clen, &st);
      int code;
      int width;
      if (got == (size_t) -1 || got == (size_t) -2 || got == 0)
        {
          /* Not valid in this locale: decode by hand, else show '?'. */
          unsigned char b = (unsigned char) s[i];
          code = '?';
          if (clen == 1 && b < 0x80)
            code = b;
          else if (clen == 2)
            code = ((b & 0x1F) << 6) | (s[i + 1] & 0x3F);
          else if (clen == 3)
            code = ((b & 0x0F) << 12) | ((s[i + 1] & 0x3F) << 6)
              | (s[i + 2] & 0x3F);
          else if (clen == 4)
            code = ((b & 0x07) << 18) | ((s[i + 1] & 0x3F) << 12)
              | ((s[i + 2] & 0x3F) << 6) | (s[i + 3] & 0x3F);
          width = 1;
        }
      else
        {
          code = (int) wc;
          width = wcwidth (wc);
          if (width < 0)
            width = 1;
        }
      i += clen;
      if (code < 32 || code == 127)
        code = ' ';             /* never send a control code to a cell */
      if (width == 0)
        continue;               /* combining mark: no cell of its own */
      if (x + width > p->cols)
        break;
      put (p, y, x, code, cur_quality);
      if (width == 2)
        put (p, y, x + 1, 0, cur_quality);
      x += width;
    }
  p->cur_y = y;
  p->cur_x = x;
}

void
pane_printf (Pane *p, int y, int x, const char *fmt, ...)
{
  char text[2048];
  va_list ap;
  va_start (ap, fmt);
  int n = vsnprintf (text, sizeof text, fmt, ap);
  va_end (ap);
  if (n < 0)
    return;
  if ((size_t) n >= sizeof text)
    n = (int) sizeof text - 1;
  pane_addnstr (p, y, x, text, n);
}

void
pane_move (Pane *pp, int y, int x)
{
  Pane *p = real (pp);
  p->cur_y = y;
  p->cur_x = x;
}

void
pane_clrtoeol (Pane *pp)
{
  Pane *p = real (pp);
  for (int x = p->cur_x; x < p->cols; x++)
    put (p, p->cur_y, x, ' ', LED_Q_NORMAL);
}

void
pane_erase (Pane *pp)
{
  Pane *p = real (pp);
  for (int y = 0; y < p->rows; y++)
    for (int x = 0; x < p->cols; x++)
      put (p, y, x, ' ', LED_Q_NORMAL);
  p->cur_y = p->cur_x = 0;
}

void
pane_box (Pane *pp)
{
  Pane *p = real (pp);
  if (p->rows < 2 || p->cols < 2)
    return;
  int b = p->rows - 1;
  int r = p->cols - 1;
  for (int x = 1; x < r; x++)
    {
      put (p, 0, x, 0x2500, LED_Q_NORMAL);      /* ─ */
      put (p, b, x, 0x2500, LED_Q_NORMAL);
    }
  for (int y = 1; y < b; y++)
    {
      put (p, y, 0, 0x2502, LED_Q_NORMAL);      /* │ */
      put (p, y, r, 0x2502, LED_Q_NORMAL);
    }
  put (p, 0, 0, 0x250C, LED_Q_NORMAL);  /* ┌ */
  put (p, 0, r, 0x2510, LED_Q_NORMAL);  /* ┐ */
  put (p, b, 0, 0x2514, LED_Q_NORMAL);  /* └ */
  put (p, b, r, 0x2518, LED_Q_NORMAL);  /* ┘ */
}

void
pane_noutrefresh (Pane *pp)
{
  Pane *p = real (pp);
  caret_y = p->y0 + p->cur_y;
  caret_x = p->x0 + p->cur_x;
}

void
screen_commit (void)
{
  const Frontend *fe = frontend_current ();
  if (!fe || !want)
    return;
  for (int y = 0; y < g_rows; y++)
    for (int x = 0; x < g_cols; x++)
      {
        Cell *w = &want[y * g_cols + x];
        Cell *s = &sent[y * g_cols + x];
        if (w->code == s->code && w->quality == s->quality)
          continue;
        *s = *w;
        if (w->code == 0)
          continue;             /* covered by the wide character before it */
        if (w->code == ' ' && w->quality == LED_Q_NORMAL)
          fe->hide (x, y, ' ');
        else
          {
            fe->mark (x, y, w->code);
            fe->show (x, y, w->quality);
          }
      }
  /* Finish with a show where the caret belongs. */
  if (caret_y >= 0 && caret_y < g_rows && caret_x >= 0 && caret_x < g_cols)
    fe->show (caret_x, caret_y, want[caret_y * g_cols + caret_x].quality);
}

void
pane_refresh (Pane *p)
{
  pane_noutrefresh (p);
  screen_commit ();
}
