#include "view.h"
#include "config.h"
#include "utils.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <limits.h>
#include <time.h>               // For time functions in status bar
#include <string.h>             // For strdup

/* === Pane helpers (border isolation) === */
int
view_create_text_window (Pane *frame, Pane **text_out)
{
  return view_create_text_window_ex (frame, text_out, 1);
}

void
view_resize_windows (Pane *frame, Pane *text)
{
  view_resize_windows_ex (frame, text, 1);
}

/* Text pane geometry: inset by one cell when the border is on,
 * else full width with the bottom row left for the status line. */
static int
text_geometry (Pane *frame, int show_border, int *rows, int *cols, int *y0, int *x0)
{
  int fh = pane_rows (frame);
  int fw = pane_cols (frame);
  if (show_border)
    {
      if (fh < 3 || fw < 3)
        return -1;
      *rows = fh - 2; *cols = fw - 2; *y0 = 1; *x0 = 1;
    }
  else
    {
      if (fh < 2 || fw < 1)
        return -1;
      *rows = fh - 1; *cols = fw; *y0 = 0; *x0 = 0;
    }
  return 0;
}

int
view_create_text_window_ex (Pane *frame, Pane **text_out, int show_border)
{
  if (!frame || !text_out)
    return -1;
  int rows, cols, y0, x0;
  if (text_geometry (frame, show_border, &rows, &cols, &y0, &x0) != 0)
    {
      *text_out = NULL;
      return -1;
    }
  *text_out = pane_new (rows, cols, y0, x0);
  return *text_out ? 0 : -1;
}

void
view_resize_windows_ex (Pane *frame, Pane *text, int show_border)
{
  if (!frame || !text)
    return;
  int rows, cols, y0, x0;
  if (text_geometry (frame, show_border, &rows, &cols, &y0, &x0) != 0)
    return;
  pane_set (text, rows, cols, y0, x0);
}

void
view_recreate_text_window (Pane *frame, Pane **text, int show_border)
{
  if (!frame || !text)
    return;
  if (*text)
    {
      pane_free (*text);
      *text = NULL;
    }
  view_create_text_window_ex (frame, text, show_border);
}

/* Helpers to get text pane dimensions safely (fallback to the screen) */
static int
text_win_rows (Pane *text)
{
  if (text)
    return pane_rows (text);
  return (screen_rows () > 2) ? screen_rows () - 2 : 0;
}

static int
text_win_cols (Pane *text)
{
  if (text)
    return pane_cols (text);
  return (screen_cols () > 2) ? screen_cols () - 2 : 0;
}

static int
frame_win_cols (Pane *frame)
{
  if (frame)
    return pane_cols (frame);
  return screen_cols ();
}

static int
frame_win_rows (Pane *frame)
{
  if (frame)
    return pane_rows (frame);
  return screen_rows ();
}
// Alias for the pre-parsed config pair type
typedef SyntaxPair KeywordPair;
// Calculate the number of digits in a int number (for line numbers)
int
calculate_digits (int n)
{
  if (n == 0)
    return 1;
  int digits = 0;
  do
    {
		



      digits++;
      n /= 10;
    }
  while (n > 0);
  return digits;
}
// Compute the visual column for a given logical byte position in a line
int
visual_column (const char *line, int len, int logical_pos,
               int tab_width)
{
  if (!line || logical_pos <= 0)
    return 0;
  if (logical_pos > len)
    logical_pos = len;
  if (tab_width <= 0)
    tab_width = 8;
  int vis = 0;
  int i = 0;
  while (i < logical_pos)
    {
      if (line[i] == '\t')
        {
          vis += tab_width - (vis % tab_width);
          i++;
        }
      else
        {
          int clen = utf8_char_len (line + i, logical_pos - i);
          /* If multi-byte sequence would cross logical_pos, count remaining bytes as width 1 */
          if (i + clen > logical_pos)
            {
              vis += logical_pos - i;
              break;
            }
          vis += utf8_char_width (line + i, logical_pos - i);
          i += clen;
        }
    }
  return vis;
}

/*
 * Find a good break point for word wrapping.
 * Returns the number of characters (in logical line) that can be printed
 * starting at 'start' without exceeding max_width visual columns.
 * Prefers breaking at a space; falls back to hard break.
 * Correctly accounts for tabs via visual_column.
 */
static int
get_wrap_break (const char *line, int start, int line_len, int max_width, int tab_width)
{
  if (start >= line_len)
    return 0;

  // First, find the maximum we could take if we ignored word boundaries
  int vis = 0;
  int max_chars = 0;
  for (int i = start; i < line_len; )
    {
      int clen;
      int char_vis;
      if (line[i] == '\t')
        {
          clen = 1;
          char_vis = tab_width - (vis % tab_width);
        }
      else
        {
          clen = utf8_char_len (line + i, line_len - i);
          char_vis = utf8_char_width (line + i, line_len - i);
        }
      if (vis + char_vis > max_width)
        break;
      vis += char_vis;
      max_chars += clen;
      i += clen;
    }
  if (start + max_chars >= line_len)
    return line_len - start;   // fits entirely

  // Try to find a space to break at, working backwards from max_chars
  for (int i = max_chars; i > 0; i--)
    {
      if (line[start + i] == ' ')
        {
          // Include the space on this row (or not? convention: break after space)
          // We include it so the next row starts clean.
          return i + 1;
        }
    }

  // No nice break — hard break at max_chars
  if (max_chars == 0)
    max_chars = 1;             // always make progress on pathological input
  return max_chars;
}
/* A word has ended: full_line[start, end). Update the keyword nesting,
 * and colour the word when colors is given. Both the colouring pass and
 * the nesting-only pass end words here, so they cannot disagree. */
static void
finish_word (const char *full_line, int start, int end, int *colors,
             EditorConfig *config, KeywordPair *pairs, int num_pairs,
             int *kw_level, int *kw_top, int *kw_stack)
{
  char word[64];
  int wlen = end - start;
  if (wlen <= 0 || wlen >= (int) sizeof word)
    return;                     /* too long to be a keyword */
  memcpy (word, full_line + start, (size_t) wlen);
  word[wlen] = '\0';
  int color = 0;
  int matched = 0;
  for (int p = 0; p < num_pairs && !matched; p++)
    {
      if (strcmp (word, pairs[p].open) == 0)
        {
          /* Past what the stack holds the depth is still counted, so
           * a level is always one more than the depth it opened at. */
          if (*kw_top < 100)
            kw_stack[*kw_top] = *kw_level;
          (*kw_top)++;
          int lvl = *kw_level;
          color = 3 + (lvl > 4 ? 4 : lvl);
          (*kw_level)++;
          matched = 1;
        }
      else if (strcmp (word, pairs[p].close) == 0)
        {
          if (*kw_top > 0)
            {
              --(*kw_top);
              int lvl = (*kw_top < 100) ? kw_stack[*kw_top] : 5;
              color = 3 + (lvl > 4 ? 4 : lvl);
            }
          *kw_level = (*kw_level > 1) ? *kw_level - 1 : 1;
          matched = 1;
        }
    }
  if (!colors)
    return;
  if (!matched && config && config_is_reserved_word (config, word))
    color = 8;
  if (color)
    for (int j = start; j < end; j++)
      colors[j] = color;
}

/* Read one line of source: colour it when colors is given, and carry
 * the nesting state across it either way. One scanner serves both the
 * colouring pass and the nesting-only pass, so they cannot disagree.
 *
 * Text inside a string, a character literal or a comment is not code:
 * it is left plain, and a bracket or keyword in it does not nest.
 * *in_comment carries an open block comment from line to line. */
static void
scan_line (const char *s, int len, int *colors, EditorConfig *config,
           KeywordPair *pairs, int num_pairs,
           int *brace_level, int *brace_top, int *brace_stack,
           int *kw_level, int *kw_top, int *kw_stack, int *in_comment)
{
  int word_start = 0;
  int in_word = 0;
  int word_count = 0;
  int i = 0;
  /* i == len is one step past the text, so a word that runs to the end
   * of the line is ended by the same code as any other. */
  while (i <= len)
    {
      if (*in_comment)
        {
          while (i < len && !(s[i] == '*' && i + 1 < len && s[i + 1] == '/'))
            i++;
          if (i >= len)
            break;              /* still open at the end of the line */
          i += 2;
          *in_comment = 0;
          continue;
        }
      char c = (i < len) ? s[i] : '\0';
      if (i < len && (isalnum ((unsigned char) c) || c == '_'))
        {
          if (!in_word)
            {
              word_start = i;
              in_word = 1;
            }
          i++;
          continue;
        }
      /* Anything that is not a word character ends the word, a meta
       * symbol included. "break;" is the word "break", then ';'. */
      if (in_word)
        {
          /* Past 100 words the line is not coloured further, but the
           * nesting is still counted so later lines stay right. */
          finish_word (s, word_start, i,
                       word_count++ > 100 ? NULL : colors, config, pairs,
                       num_pairs, kw_level, kw_top, kw_stack);
          in_word = 0;
        }
      if (i >= len)
        break;
      if (c == '/' && i + 1 < len && s[i + 1] == '/')
        break;                  /* line comment: the rest is not code */
      if (c == '/' && i + 1 < len && s[i + 1] == '*')
        {
          *in_comment = 1;
          i += 2;
          continue;
        }
      /* A quote opens a literal. An apostrophe straight after a letter
       * or digit does not: that is 1'000'000 or a word like don't. */
      if (c == '"' || (c == '\'' && !(i > 0 && (isalnum ((unsigned char) s[i - 1])
                                                || s[i - 1] == '_'))))
        {
          i++;
          while (i < len && s[i] != c)
            {
              if (s[i] == '\\' && i + 1 < len)
                i++;
              i++;
            }
          if (i < len)
            i++;                /* the closing quote */
          continue;
        }
      if (c == '{' || c == '(' || c == '[')
        {
          if (*brace_top < 256)
            brace_stack[*brace_top] = *brace_level;
          (*brace_top)++;
          int lvl = *brace_level;
          if (colors)
            colors[i] = 4 + (lvl > 4 ? 3 : lvl - 1);    // Levels 4-7
          (*brace_level)++;
        }
      else if (c == '}' || c == ')' || c == ']')
        {
          int lvl = 1;          /* a closer with no opener: outermost */
          if (*brace_top > 0)
            {
              /* Deeper than the stack holds: the depth is still counted,
               * but there is no saved level to read. */
              --(*brace_top);
              lvl = (*brace_top < 256) ? brace_stack[*brace_top] : 4;
            }
          if (colors)
            colors[i] = 4 + (lvl > 4 ? 3 : lvl - 1);
          *brace_level = (*brace_level > 1) ? *brace_level - 1 : 1;
        }
      else if (colors && c == ';')
        colors[i] = 3;          // Red for semicolons
      else if (colors && c == ',')
        colors[i] = 4;
      i++;
    }
}

// Compute color array for a line based on syntax highlighting
// Returns an array of color indices (1-based: the LED_Q_* qualities)
// Caller must free the returned array.
// highlight_pair: 1 to disable, else enable
int *
compute_line_colors (const char *full_line, int line_len,
                     int highlight_pair, EditorConfig *config, int *brace_level, int *brace_top, int *brace_stack, int *kw_level, int *kw_top, int *kw_stack, int *in_comment)
{
  int maxll = (config && config->performance.max_line_length > 0) ? config->performance.max_line_length : 10000;
  if (highlight_pair == 1)
    return NULL;                // highlighting is off
  if (line_len > maxll)
    {
      /* Too long to colour, but its brackets and comments still count:
       * the nesting-only pass reads this line too, and the two must
       * leave the same state for the lines below. */
      if (config)
        scan_line (full_line, line_len, NULL, config,
                   (KeywordPair *) config->syntax_pairs,
                   config->num_syntax_pairs, brace_level, brace_top,
                   brace_stack, kw_level, kw_top, kw_stack, in_comment);
      return NULL;
    }
  int *colors = xmalloc ((size_t) (line_len > 0 ? line_len : 1) * sizeof (int));
  for (int i = 0; i < line_len; i++)
    colors[i] = 1;              // normal
  if (!config)
    return colors;              // Safety check
  scan_line (full_line, line_len, colors, config,
             (KeywordPair *) config->syntax_pairs, config->num_syntax_pairs,
             brace_level, brace_top, brace_stack, kw_level, kw_top, kw_stack,
             in_comment);
  return colors;
}
/* The level stacks are not kept per line (see NestingCache). A level
 * is always one more than the depth it was opened at, so entry i of a
 * stack is i + 1. */
static void
restore_stacks (int brace_top, int brace_stack[], int kw_top, int kw_stack[])
{
  for (int i = 0; i < brace_top && i < 256; i++)
    brace_stack[i] = i + 1;
  for (int i = 0; i < kw_top && i < 100; i++)
    kw_stack[i] = i + 1;
}

// Compute starting nesting state for a line (with caching)
void get_starting_levels(Buffer *buf, int start_line, int *brace_level, int *brace_top, int brace_stack[], int *kw_level, int *kw_top, int kw_stack[], int *in_comment, EditorConfig *config) {
  *brace_level = 1;
  *brace_top = 0;
  *kw_level = 1;
  *kw_top = 0;
  *in_comment = 0;
  /* Start from the nearest earlier line whose state is cached, not from
   * the top of the file: in a long file that is the difference between
   * reading a screenful and reading everything above it. */
  int from = 0;
  if (buf->nesting_cache)
    for (int l = start_line - 1; l >= 0; l--)
      if (buf->nesting_cache[l].valid)
        {
          const NestingCache *c = &buf->nesting_cache[l];
          *brace_level = c->brace_level;
          *brace_top = c->brace_top;
          *kw_level = c->kw_level;
          *kw_top = c->kw_top;
          *in_comment = c->in_comment;
          restore_stacks (*brace_top, brace_stack, *kw_top, kw_stack);
          from = l + 1;
          break;
        }
  // Use pre-parsed pairs from config
  KeywordPair *pairs = (KeywordPair *) config->syntax_pairs;
  int num_pairs = config->num_syntax_pairs;
  // Fold in each line between there and start_line
  for (int l = from; l < start_line; l++) {
    char *line = buffer_get_line(buf, l);
    if (!line)
      continue;
    int len = strlen(line);
    scan_line (line, len, NULL, config, pairs, num_pairs, brace_level, brace_top, brace_stack, kw_level, kw_top, kw_stack, in_comment);
    free(line);
  }
}

/* === Per-line color cache (Tier 4) === */
#define CCACHE_SLOTS 256
#define CCACHE_MAX_LINE 4096

typedef struct {
  int used;
  int line_no;
  unsigned long generation;
  unsigned long start_hash;
  int has_colors;
  int text_len;
  int *colors;
  char *text;
  /* nesting state AFTER this line, so hits restore identical state */
  int brace_level, brace_top, brace_stack[256];
  int kw_level, kw_top, kw_stack[100];
  int in_comment;
} LineColorEntry;

static LineColorEntry g_ccache[CCACHE_SLOTS];

static unsigned long
hash_start_state (int bl, int bt, const int *bs, int kl, int kt, const int *ks,
                  int in_comment)
{
  unsigned long h = 1469598103934665603UL ^ (in_comment ? 0x9E3779B9UL : 0);
  unsigned long vals[8];
  int n = 0;
  vals[n++] = (unsigned long) bl;
  vals[n++] = (unsigned long) bt;
  for (int i = 0; i < bt && i < 256; i++)
    vals[(n++) & 7] = (unsigned long) bs[i];
  vals[n++ & 7] = (unsigned long) kl;
  vals[n++ & 7] = (unsigned long) kt;
  for (int i = 0; i < kt && i < 100; i++)
    vals[(n++) & 7] = (unsigned long) ks[i];
  for (int i = 0; i < 8; i++)
    {
      h ^= vals[i];
      h *= 1099511628211UL;
    }
  return h;
}

/*
 * Compute (or fetch cached) colors for one logical line.
 * Always returns a pointer owned by this module (or NULL when
 * highlighting is disabled/line too long). Callers must NOT free it.
 * The in/out nesting state behaves exactly like compute_line_colors.
 */
static int *
colors_for_line (Buffer *buf, int logical_line, const char *line, int len,
                 int highlight_pair, EditorConfig *config,
                 int *brace_level, int *brace_top, int brace_stack[],
                 int *kw_level, int *kw_top, int kw_stack[], int *in_comment)
{
  static int *overflow_colors;
  static int overflow_cap;
  int maxll = (config && config->performance.max_line_length > 0)
    ? config->performance.max_line_length : 10000;
  if (highlight_pair == 1 || len > maxll || len > CCACHE_MAX_LINE)
    {
      int *computed = compute_line_colors (line, len, highlight_pair, config,
                                           brace_level, brace_top, brace_stack,
                                           kw_level, kw_top, kw_stack,
                                           in_comment);
      if (!computed)
        return NULL;
      if (overflow_cap < len)
        {
          int *nc = realloc (overflow_colors, (size_t) len * sizeof (int));
          if (!nc)
            {
              free (computed);
              return NULL;
            }
          overflow_colors = nc;
          overflow_cap = len;
        }
      memcpy (overflow_colors, computed, (size_t) len * sizeof (int));
      free (computed);
      return overflow_colors;
    }
  LineColorEntry *slot = &g_ccache[logical_line & (CCACHE_SLOTS - 1)];
  unsigned long h = hash_start_state (*brace_level, *brace_top, brace_stack,
                                      *kw_level, *kw_top, kw_stack,
                                      *in_comment);
  if (slot->used && slot->line_no == logical_line
      && slot->generation == buf->edit_generation
      && slot->start_hash == h
      && slot->text_len == len
      && memcmp (slot->text, line, (size_t) len) == 0)
    {
      *brace_level = slot->brace_level;
      *brace_top = slot->brace_top;
      memcpy (brace_stack, slot->brace_stack, sizeof (int) * 256);
      *kw_level = slot->kw_level;
      *kw_top = slot->kw_top;
      memcpy (kw_stack, slot->kw_stack, sizeof (int) * 100);
      *in_comment = slot->in_comment;
      return slot->has_colors ? slot->colors : NULL;
    }
  int *computed = compute_line_colors (line, len, highlight_pair, config,
                                       brace_level, brace_top, brace_stack,
                                       kw_level, kw_top, kw_stack, in_comment);
  if (!slot->colors)
    slot->colors = xmalloc ((size_t) CCACHE_MAX_LINE * sizeof (int));
  if (!slot->text)
    slot->text = xmalloc ((size_t) CCACHE_MAX_LINE + 1);
  slot->used = 1;
  slot->line_no = logical_line;
  slot->generation = buf->edit_generation;
  slot->start_hash = h;         /* the state on entry, which a hit compares */
  slot->has_colors = (computed != NULL);
  slot->text_len = len;
  memcpy (slot->text, line, (size_t) len);
  slot->text[len] = '\0';
  if (computed)
    memcpy (slot->colors, computed, (size_t) len * sizeof (int));
  slot->brace_level = *brace_level;
  slot->brace_top = *brace_top;
  memcpy (slot->brace_stack, brace_stack, sizeof (int) * 256);
  slot->kw_level = *kw_level;
  slot->kw_top = *kw_top;
  memcpy (slot->kw_stack, kw_stack, sizeof (int) * 100);
  slot->in_comment = *in_comment;
  /* The slot owns a copy; the computed array was only a carrier. */
  free (computed);
  return slot->has_colors ? slot->colors : NULL;
}

/* === Grow-only scratch buffers for tab expansion === */
static char *g_exp_buf;
static int *g_exp_src_buf;
static size_t g_exp_cap;

static int
ensure_exp_capacity (size_t need)
{
  if (g_exp_cap >= need)
    return 1;
  size_t ncap = g_exp_cap ? g_exp_cap : 1024;
  while (ncap < need)
    ncap *= 2;
  char *nb = realloc (g_exp_buf, ncap);
  if (!nb)
    return 0;
  g_exp_buf = nb;
  int *ns = realloc (g_exp_src_buf, ncap * sizeof (int));
  if (!ns)
    return 0;
  g_exp_src_buf = ns;
  g_exp_cap = ncap;
  return 1;
}

// Print a highlighted substring of a line into text subwindow tw
// y,x are tw-relative (0,0 origin). If tw==NULL (headless test), no-op fallback.
static void
print_highlighted (Pane *tw, int y, int x, const char *full_line, int line_len,
                   int start, int len, int highlight_pair,
                   EditorConfig *config, Buffer *buf, int logical_line)
{
  int brace_level = 1;
  int brace_top = 0;
  int brace_stack[256];
  memset(brace_stack, 0, sizeof(brace_stack));
  int kw_level = 1;
  int kw_top = 0;
  int kw_stack[100];
  memset(kw_stack, 0, sizeof(kw_stack));
  int in_comment = 0;
  get_starting_levels(buf, logical_line, &brace_level, &brace_top, brace_stack, &kw_level, &kw_top, kw_stack, &in_comment, config);
  int *colors = colors_for_line (buf, logical_line, full_line, line_len,
                                 highlight_pair, config,
                                 &brace_level, &brace_top, brace_stack,
                                 &kw_level, &kw_top, kw_stack, &in_comment);

  // Cache the nesting state after processing this line (for next line's use)
  if (buf->nesting_cache && logical_line < buf->capacity) {
    buf->nesting_cache[logical_line].valid = 1;
    buf->nesting_cache[logical_line].brace_level = brace_level;
    buf->nesting_cache[logical_line].brace_top = brace_top;
    buf->nesting_cache[logical_line].kw_level = kw_level;
    buf->nesting_cache[logical_line].kw_top = kw_top;
    buf->nesting_cache[logical_line].in_comment = in_comment;
  }

  int end = start + len;
  if (end > line_len)
    end = line_len;
  if (start < 0)
    start = 0;
  if (start >= end)
    {
      return;
    }

  /* Expand tabs to spaces; keep UTF-8 multi-byte sequences intact */
  int tab_width = (config && config->display.tab_width > 0)
                    ? config->display.tab_width : 8;
  int base_vis = visual_column (full_line, line_len, start, tab_width);
  int max_exp = 0;
  int current_vis = base_vis;
  for (int i = start; i < end; )
    {
      if (full_line[i] == '\t')
        {
          int spaces = tab_width - (current_vis % tab_width);
          max_exp += spaces;
          current_vis += spaces;
          i++;
        }
      else
        {
          int clen = utf8_char_len (full_line + i, end - i);
          max_exp += clen;
          current_vis += utf8_char_width (full_line + i, end - i);
          i += clen;
        }
    }

  if (!ensure_exp_capacity ((size_t) max_exp + 1))
    {
      if (tw)
        pane_addnstr (tw, y, x, &full_line[start], end - start);
      else
        pane_addnstr (NULL, y, x, &full_line[start], end - start);
      return;
    }
  char *expanded = g_exp_buf;
  int *exp_src = g_exp_src_buf;

  int exp_idx = 0;
  current_vis = base_vis;
  for (int i = start; i < end; )
    {
      if (full_line[i] == '\t')
        {
          int spaces = tab_width - (current_vis % tab_width);
          for (int s = 0; s < spaces; s++)
            {
              expanded[exp_idx] = ' ';
              exp_src[exp_idx] = i;
              exp_idx++;
            }
          current_vis += spaces;
          i++;
        }
      else
        {
          int clen = utf8_char_len (full_line + i, end - i);
          for (int b = 0; b < clen; b++)
            {
              expanded[exp_idx] = full_line[i + b];
              exp_src[exp_idx] = i;
              exp_idx++;
            }
          current_vis += utf8_char_width (full_line + i, end - i);
          i += clen;
        }
    }
  expanded[exp_idx] = '\0';
  int expanded_len = exp_idx;

  if (!colors)
    {
      if (tw)
        pane_addnstr (tw, y, x, expanded, expanded_len);
      else
        pane_addnstr (NULL, y, x, expanded, expanded_len);
      return;
    }

  /* Print runs of same color as complete UTF-8 sequences */
  int win_cols = tw ? pane_cols (tw) : screen_cols ();
  int current_x = x;
  int i = 0;
  while (i < expanded_len && current_x < win_cols)
    {
      int color = colors[exp_src[i]];
      int run_start = i;
      while (i < expanded_len && colors[exp_src[i]] == color)
        i++;
      int run_len = i - run_start;
      if (tw)
        pane_quality (color);
      else
        pane_quality (color);
      /* Advance by display width of each char in the run */
      int j = run_start;
      int run_x = current_x;
      while (j < run_start + run_len && run_x < win_cols)
        {
          int clen = utf8_char_len (expanded + j, run_start + run_len - j);
          int cw = utf8_char_width (expanded + j, run_start + run_len - j);
          if (run_x + cw > win_cols)
            break;
          if (tw)
            pane_addnstr (tw, y, run_x, expanded + j, clen);
          else
            pane_addnstr (NULL, y, run_x, expanded + j, clen);
          run_x += cw;
          j += clen;
        }
      if (tw)
        pane_quality (LED_Q_NORMAL);
      else
        pane_quality (LED_Q_NORMAL);
      current_x = run_x;
    }
}

// Function to handle tab insertion
// Inserts spaces or a tab character at the cursor position based on config
void
handle_tab_key (Buffer *buf, int cursor_line, int cursor_col,
                EditorConfig *config)
{
  if (!config || !buf)
    return;
  if (config->display.spaces_for_tab)
    {
      // Insert spaces to reach the next tab stop
      char *line = buffer_get_line (buf, cursor_line);
      int line_len = strlen (line);
      int current_vis =
        visual_column (line, line_len, cursor_col, config->display.tab_width);
      int spaces_to_insert =
        config->display.tab_width - (current_vis % config->display.tab_width);
      for (int i = 0; i < spaces_to_insert; i++)
        {
		



          // Assuming buffer_insert_char exists; if not, replace with appropriate buffer function
          buffer_insert_char (buf, cursor_line, cursor_col + i, ' ');
        }
      // Update cursor_col += spaces_to_insert;
      free(line);
    }
  else
    {
      // Insert a tab character
      // Assuming buffer_insert_char exists
      buffer_insert_char (buf, cursor_line, cursor_col, '\t');
      // Update cursor_col += 1;
    }
}
// Draw the initial editor view — subwindow-aware
// frame = border window (box), text = inset derwin(frame) for content
// Either may be NULL in headless tests; falls back to screen_pane ()/screen_rows ()/screen_cols ()
void
draw_initial (Pane *frame, Pane *text, Buffer *buf, int *scroll_row,
               int *scroll_col, int cursor_line, int cursor_col,
               int show_line_numbers, int syntax_highlight,
               int *cursor_screen_y, int *cursor_screen_x,
               EditorConfig *config)
{
  Pane *fw = frame ? frame : screen_pane ();
  Pane *tw = text;
  // If only one window passed historically (win==screen_pane ()), treat as frame and create fallback
  if (!tw && fw)
    {
      // Legacy: no text subwindow — use fw dimensions with border inset math
      // Keep old clear/box behaviour for compat
      if (fw == screen_pane ())
        pane_erase (NULL);
      else
        {
          pane_erase (fw);
        }
      if (!config || config->display.show_border)
        pane_box (fw);
      int num_digits = calculate_digits (buffer_num_lines (buf));
      int num_width = show_line_numbers ? num_digits + 1 : 0;
      int max_lines = (screen_rows () > 2) ? screen_rows () - 2 : 0;
      for (int i = 0; i < max_lines; i++)
        {
          int line_idx = *scroll_row + (int) i;
          if (line_idx >= buffer_num_lines (buf))
            break;
          if (show_line_numbers)
            {
              pane_printf (NULL, 1 + i, 1, "%*u ", num_digits, line_idx + 1);
            }
          char *line = buffer_get_line (buf, line_idx);
          int line_len = strlen (line);
          int start_col = *scroll_col;
          if (start_col < line_len)
            {
              int print_len = line_len - start_col;
              int max_print = (int) (screen_cols () - 2 - num_width);
              if (print_len > max_print)
                print_len = max_print;
              // fallback tw NULL => uses mv* path
              print_highlighted (NULL, 1 + i, 1 + num_width, line, line_len, start_col,
                                 print_len, syntax_highlight ? 4 : 1, config, buf, line_idx);
            }
          free(line);
        }
      int y_diff = (cursor_line >= *scroll_row) ? cursor_line - *scroll_row : 0;
      int screen_y = 1 + (int) y_diff;
      char *line = buffer_get_line (buf, cursor_line);
      int line_len = strlen (line);
      int vis_scroll = visual_column (line, line_len, *scroll_col, config->display.tab_width);
      int vis_cursor = visual_column (line, line_len, cursor_col, config->display.tab_width);
      free(line);
      int x_diff = (vis_cursor >= vis_scroll) ? (int) (vis_cursor - vis_scroll) : 0;
      int screen_x = 1 + num_width + (int) x_diff;
      if (screen_x < 1 + num_width)
        screen_x = 1 + num_width;
      if (screen_x > screen_cols () - 1)
        screen_x = screen_cols () - 1;
      if (cursor_screen_y) *cursor_screen_y = (int) screen_y;
      if (cursor_screen_x) *cursor_screen_x = (int) screen_x;
      pane_move (NULL, screen_y, screen_x);
      if (fw == screen_pane ())
        pane_refresh (NULL);
      else
        pane_refresh (fw);
      view_invalidate ();
      return;
    }

  // Subwindow path: frame owns border, text owns content
  // Clear/erase only text; box only frame (never cleared in STATUS_ONLY/screen_rows () paths)
  int do_border = !config || config->display.show_border;
  if (fw)
    {
      pane_erase (fw);
      if (do_border)
        pane_box (fw);
      // Ensure text subwindow background matches
      if (tw)
        pane_erase (tw);
    }
  else if (tw)
    {
      pane_erase (tw);
    }

  int num_digits = calculate_digits (buffer_num_lines (buf));
  int num_width = show_line_numbers ? num_digits + 1 : 0;
  int max_lines = text_win_rows (tw);
  int text_cols = text_win_cols (tw);
  for (int i = 0; i < max_lines; i++)
    {
      int line_idx = *scroll_row + (int) i;
      if (line_idx >= buffer_num_lines (buf))
        break;
      if (show_line_numbers)
        {
          if (tw)
            pane_printf (tw, i, 0, "%*u ", num_digits, line_idx + 1);
          else
            pane_printf (NULL, 1 + i, 1, "%*u ", num_digits, line_idx + 1);
        }
      char *line = buffer_get_line (buf, line_idx);
      int line_len = strlen (line);
      int start_col = *scroll_col;
      if (start_col < line_len)
        {
          int print_len = line_len - start_col;
          int max_print = text_cols - num_width;
          if (print_len > max_print)
            print_len = max_print;
          if (tw)
            print_highlighted (tw, i, num_width, line, line_len, start_col,
                               print_len, syntax_highlight ? 4 : 1, config, buf, line_idx);
          else
            print_highlighted (NULL, 1 + i, 1 + num_width, line, line_len, start_col,
                               print_len, syntax_highlight ? 4 : 1, config, buf, line_idx);
        }
      free(line);
    }
  // Calculate cursor screen position (screen_pane () coords for caller, tw-relative for wmove)
  int y_diff = (cursor_line >= *scroll_row) ? cursor_line - *scroll_row : 0;
  int screen_y = 1 + (int) y_diff;
  char *line = buffer_get_line (buf, cursor_line);
  int line_len2 = strlen (line);
  int vis_scroll = visual_column (line, line_len2, *scroll_col, config->display.tab_width);
  int vis_cursor = visual_column (line, line_len2, cursor_col, config->display.tab_width);
  free(line);
  int x_diff = (vis_cursor >= vis_scroll) ? (int) (vis_cursor - vis_scroll) : 0;
  int screen_x = 1 + num_width + (int) x_diff;
  // Clamp to text window bounds (text_cols is tw width, last valid screen_pane () x is 1+text_cols)
  // Clamp screen_x which is screen_pane () coords (1-based)
  if (screen_x < 1 + num_width)
    screen_x = 1 + num_width;
  if (tw)
    {
      // tw cols is text area; max screen_pane () x is 1+tw_cols (since border at 0)
      int max_sx = 1 + text_cols;
      if (screen_x > max_sx)
        screen_x = max_sx;
    }
  else
    {
      if (screen_x > screen_cols () - 1)
        screen_x = screen_cols () - 1;
    }
  if (cursor_screen_y) *cursor_screen_y = (int) screen_y;
  if (cursor_screen_x) *cursor_screen_x = (int) screen_x;
  // Move cursor inside text window
  if (tw)
    {
      int wy = screen_y - 1;
      int wx = screen_x - 1;
      // clamp to tw bounds
      int tw_rows = text_win_rows (tw);
      int tw_cols = text_win_cols (tw);
      if (wy < 0) wy = 0;
      if (wy >= tw_rows) wy = tw_rows - 1;
      if (wx < 0) wx = 0;
      if (wx >= tw_cols) wx = tw_cols - 1;
      pane_move (tw, wy, wx);
      if (fw)
        {
          pane_noutrefresh (fw);
          pane_noutrefresh (tw);
          screen_commit ();
        }
      else
        pane_refresh (tw);
    }
  else
    {
      pane_move (NULL, screen_y, screen_x);
      if (fw == screen_pane ())
        pane_refresh (NULL);
      else if (fw)
        pane_refresh (fw);
    }
  view_invalidate ();
}
// Repaint one content row in truncate (no-wrap) mode, erasing stale cells
// tw is the inset text subwindow (NULL => fallback to screen_pane () for headless tests)
static void
render_content_row (Pane *tw, int visual_row, int logical_line, Buffer *buf,
                     int *scroll_col, int available_width, int num_digits,
                     int num_width, int show_line_numbers,
                     int syntax_highlight,
                     int sel_start_line, int sel_start_col,
                     int sel_end_line, int sel_end_col, int selection_active,
                     EditorConfig *config)
{
  if (tw)
    {
      pane_move (tw, visual_row, 0);
      pane_clrtoeol (tw);
    }
  else
    {
      pane_move (NULL, 1 + visual_row, 1);
      pane_clrtoeol (NULL);
    }
  char *line = NULL;
  int len = 0;

  if (show_line_numbers)
    {
      if (tw)
        pane_printf (tw, visual_row, 0, "%*u ", num_digits, logical_line + 1);
      else
        pane_printf (NULL, 1 + visual_row, 1, "%*u ", num_digits, logical_line + 1);
    }

  line = buffer_get_line (buf, logical_line);
  len = strlen (line);
  int pos = *scroll_col ? *scroll_col : 0;

  // Handle selection
  int sel_start = len;
  int sel_end = len;
  if (selection_active && logical_line >= sel_start_line
      && logical_line <= sel_end_line)
    {
      sel_start =
        (logical_line == sel_start_line) ? sel_start_col : 0;
      sel_end =
        (logical_line == sel_end_line) ? sel_end_col : len;
    }

  int x = tw ? num_width : 1 + num_width;
  int tab_w = config ? config->display.tab_width : 8;

  // Print before selection
  if (pos < sel_start)
    {
      int end = (sel_start < len) ? sel_start : len;
      int print_len = end - pos;
      int max_print = available_width;
      int fit = utf8_fit_bytes (&line[pos], print_len, max_print, tab_w, 0);
      print_len = fit;
      int px = tw ? x : x;
      int py = tw ? visual_row : 1 + visual_row;
      print_highlighted (tw, py, px, line, len, pos, print_len,
                         syntax_highlight ? 4 : 1, config, buf, logical_line);
      x += utf8_visual_width (&line[pos], print_len, tab_w, 0);
      pos += print_len;
    }
  // Print selection
  if (pos < sel_end && (x - (tw ? num_width : 1 + num_width)) < available_width)
    {
      int end = sel_end;
      int print_len = end - pos;
      int max_print = available_width - (x - (tw ? num_width : 1 + num_width));
      print_len = utf8_fit_bytes (&line[pos], print_len, max_print, tab_w, 0);
      if (tw)
        {
          if (syntax_highlight)
            pane_quality (2);
          pane_addnstr (tw, visual_row, x, &line[pos], print_len);
          if (syntax_highlight)
            pane_quality (LED_Q_NORMAL);
        }
      else
        {
          if (syntax_highlight)
            pane_quality (2);
          pane_addnstr (NULL, 1 + visual_row, x, &line[pos], print_len);
          if (syntax_highlight)
            pane_quality (LED_Q_NORMAL);
        }
      x += utf8_visual_width (&line[pos], print_len, tab_w, 0);
      pos += print_len;
    }
  // Print after selection
  if (pos < len && (x - (tw ? num_width : 1 + num_width)) < available_width)
    {
      int print_len = len - pos;
      int max_print = available_width - (x - (tw ? num_width : 1 + num_width));
      print_len = utf8_fit_bytes (&line[pos], print_len, max_print, tab_w, 0);
      int px = tw ? x : x;
      int py = tw ? visual_row : 1 + visual_row;
      print_highlighted (tw, py, px, line, len, pos, print_len,
                         syntax_highlight ? 4 : 1, config, buf, logical_line);
    }

  free (line);
}

// Rebuild and repaint the bottom status line onto frame's bottom row
// frame is border window (screen_pane ()), bottom row is frame's screen_rows ()-1
static void
render_status_bar (Pane *frame, Editor *ed, Buffer *buf, int cursor_line, int cursor_col,
                   int search_mode, char *search_buffer, int replace_step,
                   char *replace_buffer, EditorConfig *config)
{
  int fcols = frame_win_cols (frame);
  int frows = frame_win_rows (frame);
  // Clear bottom row of frame (status line)
  if (frame)
    {
      pane_move (frame, frows - 1, 1);
      pane_clrtoeol (frame);
    }
  else
    {
      pane_move (NULL, screen_rows () - 1, 1);
      pane_clrtoeol (NULL);
    }
  char status_line[1024];
  if ((int) sizeof(status_line) > fcols + 1) { /* ensure fits */ }
  // Temporary message prefix
  char message_prefix[1024];
  message_prefix[0] = 0;
  if (ed && ed->status_message[0]
      && (time (NULL) - ed->status_message_time) < 5)
    {
      snprintf (message_prefix, sizeof(message_prefix), "%s | ",
                ed->status_message);
    }
  if (ed && ed->goto_mode)
    {
      snprintf (status_line, sizeof (status_line), "Go to line[:col]: %s",
                ed->goto_buffer);
    }
  else if (replace_step == 1)
    {
      snprintf (status_line, sizeof(status_line), "Replace search: %s",
                search_buffer ? search_buffer : "");
    }
  else if (replace_step == 2)
    {
      snprintf (status_line, sizeof(status_line), "Replace with: %s",
                replace_buffer ? replace_buffer : "");
    }
  else if (search_mode)
    {
      snprintf (status_line, sizeof(status_line), "Search: %s",
                search_buffer ? search_buffer : "");
    }
  else
    {
      // Build standard status
      char time_str[16] = "";
      if (config && config->statusbar.show_time)
        {
          time_t now = time (NULL);
           struct tm *tm_info = localtime (&now);
           if (config->statusbar.time_format == 24)
             {
               (void) strftime (time_str, 16, "%H:%M", tm_info);
             }
           else
             {
               (void) strftime (time_str, 16, "%I:%M%p", tm_info);
             }
        }
      char version_str[32] = "";
      if (config && config->statusbar.show_version)
        {
          snprintf (version_str, 32, "%s ", VERSION);
        }
         char pos_str[64];
         int total_lines = buffer_num_lines (buf);
         snprintf (pos_str, 64, "Line %d/%d Col %d",
                   cursor_line + 1, total_lines, cursor_col + 1);
          char meter_str[16] = "";
          if (ed && config && config->statusbar.show_key_meter) {
            snprintf (meter_str, 16, " | %dus", ed->last_key_us);
          }
      char filename_display[256] = "";
      if (ed && ed->filename)
        {
          const char *base = strrchr (ed->filename, '/');
          base = base ? base + 1 : ed->filename;
          snprintf (filename_display, 256, "%s%s", base,
                    ed->file_modified ? "*" : "");
        }
      // Assemble status line
      if (config && config->statusbar.style == 1)
        {                       // Centered
          int total_len =
            (int) (strlen (version_str) + strlen (filename_display) +
                   strlen (pos_str) + strlen (time_str) + 6);
          int start_pos = (fcols - total_len) / 2;
          if (start_pos < 1)
            start_pos = 1;
           snprintf (status_line, sizeof(status_line), "%*s%s %s %s %s%s",
                     start_pos - 1, "", version_str, filename_display, pos_str,
                     time_str, meter_str);
        }
      else
        {                       // Balanced
          int remaining =
            fcols - 2 - (int) (strlen (pos_str) + strlen (filename_display) +
                              strlen (meter_str) + 1);
          int left_space = remaining / 2;
          int right_space = remaining - left_space;
          char left[1024];
          char right[1024];
          int half = fcols / 2 + 16;
          if (half > 1023) half = 1023;
          if (left_space > 0)
            {
              snprintf (left, sizeof(left), "%*s%s",
                        left_space - (int) strlen (version_str), "",
                        version_str);
            }
          else
            {
              left[0] = '\0';
            }
          if (right_space > 0)
            {
              snprintf (right, sizeof(right), "%s%*s", time_str,
                        right_space - (int) strlen (time_str), "");
            }
          else
            {
              right[0] = '\0';
            }
           snprintf (status_line, sizeof(status_line), "%s%s %s%s%s", left,
                     filename_display, pos_str, right, meter_str);
        }
    }
  // Prepend message
  if (message_prefix[0])
    {
      char temp[1024];
      snprintf (temp, sizeof(temp), "%s%s", message_prefix, status_line);
      strncpy (status_line, temp, sizeof(status_line) - 1);
      status_line[sizeof(status_line) - 1] = '\0';
    }
  // Draw status onto frame bottom row
  if (frame)
    pane_printf (frame, frows - 1, 1, "%s", status_line);
  else
    pane_printf (NULL, screen_rows () - 1, 1, "%s", status_line);
}

RenderPlan
view_decide (const ViewFrameState *last, const ViewFrameState *cur)
{
  RenderPlan plan;
  plan.kind = LED_PLAN_FULL;
  plan.first_line = 0;
  plan.last_line = 0;
  if (!last || !cur || !last->valid)
    return plan;
  if (cur->term_lines != last->term_lines || cur->term_cols != last->term_cols)
    return plan;
  if (cur->word_wrap || last->word_wrap)
    return plan;
  if (cur->show_line_numbers != last->show_line_numbers)
    return plan;
  if (cur->syntax_highlight != last->syntax_highlight)
    return plan;
  if (cur->selection_active != last->selection_active)
    return plan;
  if (cur->sel_sl != last->sel_sl || cur->sel_sc != last->sel_sc
      || cur->sel_el != last->sel_el || cur->sel_ec != last->sel_ec)
    return plan;
  if (cur->scroll_row != last->scroll_row
      || cur->scroll_col != last->scroll_col)
    return plan;
  if (cur->generation == last->generation)
    {
      plan.kind = LED_PLAN_STATUS_ONLY;
      return plan;
    }
  if (cur->structure_changed || last->structure_changed)
    return plan;
  if (cur->num_lines != last->num_lines)
    return plan;
  if (cur->changed_first > cur->changed_last)
    return plan;
  plan.kind = LED_PLAN_LINES;
  plan.first_line = cur->changed_first < 0 ? 0 : cur->changed_first;
  plan.last_line = cur->changed_last;
  return plan;
}

static ViewFrameState g_last_frame;

void
view_invalidate (void)
{
  g_last_frame.valid = 0;
}

// Draw an updated editor view (with scrolling, selection, status bar)
// frame = border window (box), text = inset derwin(frame) for content
void
draw_update (Pane *frame, Pane *text, Buffer *buf, int *scroll_row, int *scroll_col,
              int cursor_line, int cursor_col, int show_line_numbers,
              int syntax_highlight, int search_mode, char *search_buffer,
              int selection_start_line, int selection_start_col,
              int selection_end_line, int selection_end_col,
              int selection_active, int *cursor_screen_y,
              int *cursor_screen_x, int replace_step, char *replace_buffer,
              EditorConfig *config, Editor *ed)
{
  Pane *fw = frame ? frame : screen_pane ();
  Pane *tw = text;
  int eff_rows = tw ? pane_rows (tw) : ((screen_rows () > 2) ? screen_rows () - 2 : 0);
  int eff_cols = tw ? pane_cols (tw) : ((screen_cols () > 2) ? screen_cols () - 2 : 0);
  int frame_rows = fw ? pane_rows (fw) : screen_rows ();
  int frame_cols = fw ? pane_cols (fw) : screen_cols ();
  // Adjust scroll to keep cursor visible (no word wrap)
  int max_lines = tw ? pane_rows (tw) : ((screen_rows () > 2) ? screen_rows () - 2 : 0);
  // For legacy fallback when tw==NULL, max_lines is as above
  (void) eff_rows; (void) eff_cols; (void) frame_rows; (void) frame_cols;
  int num_digits = calculate_digits (buffer_num_lines (buf));
  int num_width = show_line_numbers ? num_digits + 1 : 0;
  int available_width = (tw ? pane_cols (tw) : screen_cols () - 2) - num_width;
  if (available_width < 1) available_width = 1;

  // Simple scroll adjustment (no word wrap)
  if (cursor_line < *scroll_row) {
    *scroll_row = cursor_line;
  } else if (cursor_line >= *scroll_row + max_lines) {
    *scroll_row = cursor_line - max_lines + 1;
  }
  if (*scroll_row < 0) *scroll_row = 0;
  if (*scroll_row >= buffer_num_lines(buf)) *scroll_row = buffer_num_lines(buf) > 0 ? buffer_num_lines(buf) - 1 : 0;

  // Snapshot the frame state and decide the minimal repaint
  ViewFrameState cur;
  memset (&cur, 0, sizeof (cur));
  cur.valid = 1;
  cur.term_lines = fw ? pane_rows (fw) : screen_rows ();
  cur.term_cols = fw ? pane_cols (fw) : screen_cols ();
  cur.generation = buf->edit_generation;
  cur.num_lines = buffer_num_lines (buf);
  cur.changed_first = buf->changed_first;
  cur.changed_last = buf->changed_last;
  cur.structure_changed = buf->structure_changed;
  cur.scroll_row = *scroll_row;
  cur.scroll_col = *scroll_col;
  cur.cursor_line = cursor_line;
  cur.cursor_col = cursor_col;
  cur.show_line_numbers = show_line_numbers;
  cur.syntax_highlight = syntax_highlight;
  cur.word_wrap = config ? config->display.word_wrap : 0;
  cur.selection_active = selection_active;
  cur.sel_sl = selection_start_line;
  cur.sel_sc = selection_start_col;
  cur.sel_el = selection_end_line;
  cur.sel_ec = selection_end_col;

  RenderPlan plan = view_decide (&g_last_frame, &cur);

  if (plan.kind == LED_PLAN_STATUS_ONLY)
    {
      render_status_bar (fw, ed, buf, cursor_line, cursor_col, search_mode,
                         search_buffer, replace_step, replace_buffer, config);
      // Defer refresh to final cursor handling to avoid double refresh
    }
  else if (plan.kind == LED_PLAN_LINES)
    {
      int first = plan.first_line;
      int last = plan.last_line;
      if (first < *scroll_row)
        first = *scroll_row;
      int last_visible = *scroll_row + max_lines - 1;
      if (last > last_visible)
        last = last_visible;
      for (int l = first; l <= last && l < buffer_num_lines (buf); l++)
        render_content_row (tw, l - *scroll_row, l, buf, scroll_col,
                            available_width, num_digits, num_width,
                            show_line_numbers, syntax_highlight,
                            selection_start_line, selection_start_col,
                            selection_end_line, selection_end_col,
                            selection_active, config);
      render_status_bar (fw, ed, buf, cursor_line, cursor_col, search_mode,
                         search_buffer, replace_step, replace_buffer, config);
    }
  else
    {
      // FULL repaint: box frame, clear text subwindow only
      // Frame owns border; text window owns content. Clearing frame first
      // then text avoids overwriting child's content with parent's erase.
      if (fw)
        {
          pane_erase (fw);
          if (!config || config->display.show_border)
            pane_box (fw);
        }
      if (tw)
        pane_erase (tw);
      else
        {
          for (int r = 1; r <= screen_rows () - 2; r++)
            {
              pane_move (NULL, r, 1);
              pane_clrtoeol (NULL);
            }
        }
      
  int visual_row = 0;  // Current row on screen
  int logical_line = *scroll_row;  // Current logical line in buffer
  
  while (visual_row < max_lines && logical_line < buffer_num_lines (buf))
    {
		



      /* Wrap paints segments here. Truncate uses the same row painter
         as an incremental update, so the two paths cannot drift. */
      if (config && config->display.word_wrap)
        {
          char *line = buffer_get_line (buf, logical_line);
          int len = strlen (line);
          int pos = *scroll_col ? *scroll_col : 0;
          int sel_start = len;
          int sel_end = len;
          if (selection_active && logical_line >= selection_start_line
              && logical_line <= selection_end_line)
            {
              sel_start =
                (logical_line == selection_start_line) ? selection_start_col : 0;
              sel_end =
                (logical_line == selection_end_line) ? selection_end_col : len;
            }
          // Word wrap ON: break long logical lines across multiple visual rows.
          while (pos < len && visual_row < max_lines)
            {
		



              // Show line number only on first visual row of this logical line
              if (show_line_numbers && pos == 0)
                {
                  if (tw)
                    pane_printf (tw, visual_row, 0, "%*u ", num_digits, logical_line + 1);
                  else
                    pane_printf (NULL, 1 + visual_row, 1, "%*u ", num_digits, logical_line + 1);
                }
              
              // Use helper for word-aware breaking (correctly handles tabs)
              int segment_len = get_wrap_break (line, pos, len, available_width,
                                                config ? config->display.tab_width : 8);

              int x = tw ? num_width : 1 + num_width;
              int tab_w = config ? config->display.tab_width : 8;
              
              // Print segment (respecting selection)
              int seg_end = pos + segment_len;
              
              // Before selection
              if (pos < sel_start && seg_end > pos)
                {
                  int end = (sel_start < seg_end) ? sel_start : seg_end;
                  int print_len = end - pos;
                  if (tw)
                    print_highlighted (tw, visual_row, x, line, len, pos, print_len,
                                       syntax_highlight ? 4 : 1, config, buf, logical_line);
                  else
                    print_highlighted (NULL, 1 + visual_row, x, line, len, pos, print_len,
                                       syntax_highlight ? 4 : 1, config, buf, logical_line);
                  x += utf8_visual_width (&line[pos], print_len, tab_w, 0);
                  pos += print_len;
                }
              
              // Selection
              if (pos < sel_end && seg_end > pos)
                {
                  int end = (sel_end < seg_end) ? sel_end : seg_end;
                  int print_len = end - pos;
                  if (tw)
                    {
                      if (syntax_highlight) pane_quality (2);
                      pane_addnstr (tw, visual_row, x, &line[pos], print_len);
                      if (syntax_highlight) pane_quality (LED_Q_NORMAL);
                    }
                  else
                    {
                      if (syntax_highlight) pane_quality (2);
                      pane_addnstr (NULL, 1 + visual_row, x, &line[pos], print_len);
                      if (syntax_highlight) pane_quality (LED_Q_NORMAL);
                    }
                  x += utf8_visual_width (&line[pos], print_len, tab_w, 0);
                  pos += print_len;
                }
              
              // After selection
              if (pos < seg_end)
                {
                  int print_len = seg_end - pos;
                  if (tw)
                    print_highlighted (tw, visual_row, x, line, len, pos, print_len,
                                       syntax_highlight ? 4 : 1, config, buf, logical_line);
                  else
                    print_highlighted (NULL, 1 + visual_row, x, line, len, pos, print_len,
                                       syntax_highlight ? 4 : 1, config, buf, logical_line);
                  pos += print_len;
                }
              
              visual_row++;
            }
          free (line);
        }
      else
        {
          render_content_row (tw, visual_row, logical_line, buf, scroll_col,
                              available_width, num_digits, num_width,
                              show_line_numbers, syntax_highlight,
                              selection_start_line, selection_start_col,
                              selection_end_line, selection_end_col,
                              selection_active, config);
          visual_row++;
        }

      logical_line++;
    }

      render_status_bar (fw, ed, buf, cursor_line, cursor_col, search_mode,
                         search_buffer, replace_step, replace_buffer, config);
      // For FULL, border and text already staged; refresh both
      if (fw && tw)
        {
          pane_noutrefresh (fw);
          pane_noutrefresh (tw);
          screen_commit ();
        }
      else if (tw)
        pane_refresh (tw);
      else if (fw)
        pane_refresh (fw);
      else
        pane_refresh (NULL);
    }

  g_last_frame = cur;
  buffer_reset_change_tracking ((Buffer *) buf);

  int screen_y = 1 + (cursor_line - *scroll_row);
  char *line = buffer_get_line (buf, cursor_line);
  int line_len = strlen (line);
  int vis_scroll = visual_column (line, line_len, *scroll_col, config->display.tab_width);
  int vis_cursor = visual_column (line, line_len, cursor_col, config->display.tab_width);
  int x_diff = (vis_cursor >= vis_scroll) ? (int) (vis_cursor - vis_scroll) : 0;
  free(line);
  int screen_x = 1 + num_width + (int) x_diff;
  // Clamp cursor to visible area (use frame/text sizes if available)
  int frows = fw ? pane_rows (fw) : screen_rows ();
  int fcols = fw ? pane_cols (fw) : screen_cols ();
  int tcols = tw ? pane_cols (tw) : screen_cols () - 2;
  (void) fcols; (void) tcols;
  if (screen_y < 1)
    screen_y = 1;
  if (screen_y > frows - 1)
    screen_y = frows - 1;
  if (screen_x < 1 + num_width)
    screen_x = 1 + num_width;
  int max_sx = tw ? (1 + tcols) : (screen_cols () - 2);
  if (screen_x > max_sx)
    screen_x = max_sx;
  if (cursor_screen_y) *cursor_screen_y = (int) screen_y;
  if (cursor_screen_x) *cursor_screen_x = (int) screen_x;
  // Move cursor inside text subwindow (if present) else screen_pane ()
  if (tw)
    {
      int wy = screen_y - 1;
      int wx = screen_x - 1;
      int tw_rows = pane_rows (tw);
      int tw_cols = pane_cols (tw);
      if (wy < 0) wy = 0;
      if (wy >= tw_rows) wy = tw_rows - 1;
      if (wx < 0) wx = 0;
      if (wx >= tw_cols) wx = tw_cols - 1;
      pane_move (tw, wy, wx);
      if (fw)
        {
          pane_noutrefresh (tw);
          pane_noutrefresh (fw);
          // Ensure cursor is on tw
          pane_noutrefresh (tw);
          screen_commit ();
          // Move hardware cursor via wmove already; set after doupdate
          pane_move (tw, wy, wx);
          pane_refresh (tw);
        }
      else
        pane_refresh (tw);
    }
  else
    {
      pane_move (NULL, screen_y, screen_x);
      if (fw == screen_pane ())
        pane_refresh (NULL);
      else if (fw)
        pane_refresh (fw);
      else
        pane_refresh (NULL);
    }
}

/* === Compat wrappers for headless tests (single window == screen_pane ()) === */
void
draw_initial_compat (Pane *win, Buffer *buf, int *scroll_row,
               int *scroll_col, int cursor_line, int cursor_col,
               int show_line_numbers, int syntax_highlight,
               int *cursor_screen_y, int *cursor_screen_x,
               EditorConfig *config)
{
  draw_initial (win, NULL, buf, scroll_row, scroll_col, cursor_line, cursor_col,
                show_line_numbers, syntax_highlight, cursor_screen_y, cursor_screen_x, config);
}

void
draw_update_compat (Pane *win, Buffer *buf, int *scroll_row, int *scroll_col,
              int cursor_line, int cursor_col, int show_line_numbers,
              int syntax_highlight, int search_mode, char *search_buffer,
              int selection_start_line, int selection_start_col,
              int selection_end_line, int selection_end_col,
              int selection_active, int *cursor_screen_y,
              int *cursor_screen_x, int replace_step, char *replace_buffer,
              EditorConfig *config, Editor *ed)
{
  draw_update (win, NULL, buf, scroll_row, scroll_col, cursor_line, cursor_col,
               show_line_numbers, syntax_highlight, search_mode, search_buffer,
               selection_start_line, selection_start_col, selection_end_line, selection_end_col,
               selection_active, cursor_screen_y, cursor_screen_x, replace_step, replace_buffer,
               config, ed);
}