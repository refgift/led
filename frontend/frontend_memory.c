/* Memory front end: the three view functions write into a grid that a
 * test can read back. No terminal, no input. */
#include "frontend.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
  int code;
  int quality;
} MemCell;

static MemCell *grid;
static int g_cols, g_rows;
static int caret_x, caret_y;
static int calls;
static char *clip;

static MemCell *
cell (int x, int y)
{
  if (!grid || x < 0 || y < 0 || x >= g_cols || y >= g_rows)
    return NULL;
  return &grid[y * g_cols + x];
}

void
memory_reset (int cols, int rows)
{
  free (grid);
  grid = NULL;
  g_cols = cols > 0 ? cols : 0;
  g_rows = rows > 0 ? rows : 0;
  caret_x = caret_y = 0;
  calls = 0;
  free (clip);
  clip = NULL;
  if (g_cols && g_rows)
    {
      grid = malloc (sizeof (MemCell) * (size_t) g_cols * (size_t) g_rows);
      for (int i = 0; grid && i < g_cols * g_rows; i++)
        {
          grid[i].code = ' ';
          grid[i].quality = 0;
        }
    }
}

static void
mem_mark (int x, int y, int c)
{
  MemCell *m = cell (x, y);
  calls++;
  if (m)
    m->code = c;
}

static void
mem_show (int x, int y, int c)
{
  MemCell *m = cell (x, y);
  calls++;
  if (m)
    m->quality = c;
  caret_x = x;
  caret_y = y;
}

static void
mem_hide (int x, int y, int c)
{
  MemCell *m = cell (x, y);
  calls++;
  if (m)
    {
      m->code = c;
      m->quality = 0;
    }
}

static int
mem_start (const FrontendStyle styles[LED_Q_COUNT])
{
  (void) styles;
  return 0;
}

static void
mem_stop (void)
{
}

static void
mem_size (int *cols, int *rows)
{
  if (cols)
    *cols = g_cols;
  if (rows)
    *rows = g_rows;
}

static int
mem_read_key (void)
{
  return LED_KEY_NONE;
}

int
memory_char (int x, int y)
{
  MemCell *m = cell (x, y);
  return m ? m->code : ' ';
}

int
memory_quality (int x, int y)
{
  MemCell *m = cell (x, y);
  return m ? m->quality : 0;
}

void
memory_caret (int *x, int *y)
{
  if (x)
    *x = caret_x;
  if (y)
    *y = caret_y;
}

static void
mem_clipboard (const char *text, int len)
{
  free (clip);
  clip = malloc ((size_t) len + 1);
  if (clip)
    {
      memcpy (clip, text, (size_t) len);
      clip[len] = '\0';
    }
}

const char *
memory_clipboard (void)
{
  return clip;
}

int
memory_calls (void)
{
  return calls;
}

static int
put_utf8 (char *out, int code)
{
  if (code < 0x80)
    {
      out[0] = (char) code;
      return 1;
    }
  if (code < 0x800)
    {
      out[0] = (char) (0xC0 | (code >> 6));
      out[1] = (char) (0x80 | (code & 0x3F));
      return 2;
    }
  if (code < 0x10000)
    {
      out[0] = (char) (0xE0 | (code >> 12));
      out[1] = (char) (0x80 | ((code >> 6) & 0x3F));
      out[2] = (char) (0x80 | (code & 0x3F));
      return 3;
    }
  out[0] = (char) (0xF0 | (code >> 18));
  out[1] = (char) (0x80 | ((code >> 12) & 0x3F));
  out[2] = (char) (0x80 | ((code >> 6) & 0x3F));
  out[3] = (char) (0x80 | (code & 0x3F));
  return 4;
}

char *
memory_row (int y, char *out, int out_size)
{
  int n = 0;
  int last = 0;
  if (out_size < 1)
    return out;
  for (int x = 0; x < g_cols && n + 5 < out_size; x++)
    {
      int code = memory_char (x, y);
      if (code == 0)
        continue;               /* right half of a wide character */
      n += put_utf8 (out + n, code);
      if (code != ' ')
        last = n;
    }
  out[last] = '\0';
  return out;
}

const Frontend *
frontend_memory (void)
{
  static const Frontend fe = {
    mem_show, mem_hide, mem_mark,
    mem_start, mem_stop, mem_size, mem_read_key, mem_clipboard
  };
  return &fe;
}
