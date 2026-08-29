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

/* === Border filter (replaces external `tr -d '|+'`) === */
static const char *border_glyphs[] = {
  "|", "+", "-",
  "\xE2\x94\x82", /* │ U+2502 */
  "\xE2\x94\x80", /* ─ U+2500 */
  "\xE2\x94\x8C", /* ┌ U+250C */
  "\xE2\x94\x90", /* ┐ U+2510 */
  "\xE2\x94\x94", /* └ U+2514 */
  "\xE2\x94\x98", /* ┘ U+2518 */
  "\xE2\x94\x9C", /* ├ U+251C */
  "\xE2\x94\xA4", /* ┤ U+2524 */
  "\xE2\x94\xAC", /* ┬ U+252C */
  "\xE2\x94\xB4", /* ┴ U+2534 */
  "\xE2\x94\xBC", /* ┼ U+253C */
  "\xE2\x94\x83", /* ┃ */
  "\xE2\x94\x81", /* ━ */
  NULL
};

static size_t
border_prefix_len (const char *s, size_t len)
{
  for (int i = 0; border_glyphs[i]; i++)
    {
      size_t gl = strlen (border_glyphs[i]);
      if (len >= gl && memcmp (s, border_glyphs[i], gl) == 0)
        return gl;
    }
  return 0;
}

static size_t
border_suffix_len (const char *s, size_t len)
{
  for (int i = 0; border_glyphs[i]; i++)
    {
      size_t gl = strlen (border_glyphs[i]);
      if (len >= gl && memcmp (s + len - gl, border_glyphs[i], gl) == 0)
        return gl;
    }
  return 0;
}

int
border_filter_is_pure_border_line (const char *line, size_t len)
{
  if (len == 0) return 0;
  size_t i = 0;
  int has_border = 0;
  while (i < len)
    {
      if (line[i] == ' ' || line[i] == '\t')
        { i++; continue; }
      size_t gl = border_prefix_len (line + i, len - i);
      if (gl)
        { has_border = 1; i += gl; continue; }
      return 0; /* non-border content found */
    }
  return has_border;
}

char *
border_filter_dup (const char *text)
{
  if (!text) return NULL;
  size_t n = strlen (text);
  char *out = xmalloc (n + 1);
  size_t oi = 0;
  size_t i = 0;
  while (i < n)
    {
      size_t line_start = i;
      size_t line_end = i;
      while (line_end < n && text[line_end] != '\n')
        line_end++;
      size_t line_len = line_end - line_start;
      int has_nl = (line_end < n && text[line_end] == '\n') ? 1 : 0;

      /* Drop pure border lines (e.g. "┌──────────┐") */
      if (border_filter_is_pure_border_line (text + line_start, line_len))
        {
          /* preserve blank line? Drop entirely but keep newline if it was the only line? */
          /* For block copy, horizontal borders are not wanted, so drop line including newline */
          i = line_end + has_nl;
          /* Avoid collapsing: if this was the only content, keep one newline */
          if (oi == 0 && i >= n)
            {
              /* empty file -> keep empty */
            }
          else if (has_nl && oi > 0)
            {
              /* keep line break structure: if we drop a line, don't add extra newline */
            }
          continue;
        }

      size_t lo = 0, hi = line_len;
      size_t pre = border_prefix_len (text + line_start, line_len);
      if (pre)
        {
          lo = pre;
          if (lo < hi && text[line_start + lo] == ' ')
            lo++; /* strip one padding space after border */
        }
      size_t suf = 0;
      if (hi > lo)
        suf = border_suffix_len (text + line_start + lo, hi - lo);
      if (suf)
        {
          size_t new_hi = hi - suf;
          if (new_hi > lo && text[line_start + new_hi - 1] == ' ')
            new_hi--; /* strip one padding space before border */
          hi = new_hi;
        }

      size_t copy_len = (hi > lo) ? hi - lo : 0;
      if (copy_len)
        {
          memcpy (out + oi, text + line_start + lo, copy_len);
          oi += copy_len;
        }
      if (has_nl)
        out[oi++] = '\n';
      i = line_end + has_nl;
    }
  out[oi] = '\0';
  /* Shrink if we dropped a lot (optional) */
  return out;
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
