#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <wchar.h>

/**
 * Centralized allocation helpers.
 *
 * These are deliberately simple and brutal: on allocation failure we
 * print a message and terminate. This is intentional for an interactive
 * editor — we would rather crash loudly than silently corrupt state or
 * lose the user's work.
 */

void* xmalloc(size_t n)
{
    void *p = malloc(n);
    if (!p) {
        perror("xmalloc");
        exit(EXIT_FAILURE);
    }
    return p;
}

void* xrealloc(void *p, size_t n)
{
    void *np = realloc(p, n);
    if (!np) {
        perror("xrealloc");
        exit(EXIT_FAILURE);
    }
    return np;
}

void* xcalloc(size_t nmemb, size_t size)
{
    void *p = calloc(nmemb, size);
    if (!p) {
        perror("xcalloc");
        exit(EXIT_FAILURE);
    }
    return p;
}

bool
is_filename_safe (const char *fn)
{
  if (!fn || !*fn)
    return false;
  size_t n = strlen (fn);
  if (n > 255)
    return false;
  if (strstr (fn, ".."))
    return false;
  for (size_t i = 0; i < n; i++)
    {
      unsigned char c = (unsigned char) fn[i];
      if (c < 32 || c == 127)
        return false;
    }
  return true;
}

int
utf8_char_len (const char *s, int maxlen)
{
  if (!s || maxlen <= 0)
    return 0;
  unsigned char c = (unsigned char) s[0];
  int need;
  if (c < 0x80)
    need = 1;
  else if ((c & 0xE0) == 0xC0)
    need = 2;
  else if ((c & 0xF0) == 0xE0)
    need = 3;
  else if ((c & 0xF8) == 0xF0)
    need = 4;
  else
    return 1;
  if (need > maxlen)
    return 1;
  for (int i = 1; i < need; i++)
    {
      if (((unsigned char) s[i] & 0xC0) != 0x80)
        return 1;
    }
  return need;
}

int
utf8_char_width (const char *s, int maxlen)
{
  if (!s || maxlen <= 0)
    return 0;
  if ((unsigned char) s[0] == '\t')
    return 1;
  int clen = utf8_char_len (s, maxlen);
  wchar_t wc;
  mbstate_t st;
  memset (&st, 0, sizeof st);
  size_t n = mbrtowc (&wc, s, (size_t) clen, &st);
  if (n == (size_t) -1 || n == (size_t) -2 || n == 0)
    return 1;
  int w = wcwidth (wc);
  return (w < 0) ? 1 : w;
}

int
utf8_visual_width (const char *s, int byte_len, int tab_width, int start_vis)
{
  int vis = start_vis;
  int i = 0;
  if (tab_width <= 0)
    tab_width = 8;
  while (i < byte_len)
    {
      if (s[i] == '\t')
        {
          vis += tab_width - (vis % tab_width);
          i++;
        }
      else
        {
          int clen = utf8_char_len (s + i, byte_len - i);
          vis += utf8_char_width (s + i, byte_len - i);
          i += clen;
        }
    }
  return vis - start_vis;
}

int
utf8_fit_bytes (const char *s, int byte_len, int max_vis, int tab_width,
                int start_vis)
{
  if (!s || byte_len <= 0 || max_vis <= 0)
    return 0;
  if (tab_width <= 0)
    tab_width = 8;
  int vis = 0;
  int i = 0;
  int col = start_vis;
  while (i < byte_len)
    {
      int clen;
      int cw;
      if (s[i] == '\t')
        {
          clen = 1;
          cw = tab_width - (col % tab_width);
        }
      else
        {
          clen = utf8_char_len (s + i, byte_len - i);
          cw = utf8_char_width (s + i, byte_len - i);
        }
      if (vis + cw > max_vis)
        break;
      vis += cw;
      col += cw;
      i += clen;
    }
  return i;
}
