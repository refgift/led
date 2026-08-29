#include "view.h"
#include "config.h"
#include "utils.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <limits.h>
#include <time.h>               // For time functions in status bar
#include <string.h>             // For strdup
// Meta symbols for basic syntax highlighting (braces, semicolons, etc.)
static const char *meta_symbols = ";,{}()[]";

/* === Subwindow helpers (border isolation) === */
int
view_create_text_window (WINDOW *frame, WINDOW **text_out)
{
  return view_create_text_window_ex (frame, text_out, 1);
}

void
view_resize_windows (WINDOW *frame, WINDOW *text)
{
  view_resize_windows_ex (frame, text, 1);
}

int
view_create_text_window_ex (WINDOW *frame, WINDOW **text_out, int show_border)
{
  if (!frame || !text_out)
    return -1;
  int fh, fw;
  getmaxyx (frame, fh, fw);
  if (show_border)
    {
      if (fh < 3 || fw < 3)
        {
          *text_out = NULL;
          return -1;
        }
      *text_out = derwin (frame, fh - 2, fw - 2, 1, 1);
    }
  else
    {
      /* Border off: text uses full width, leaves 1 row for status bar on frame */
      if (fh < 2 || fw < 1)
        {
          *text_out = NULL;
          return -1;
        }
      *text_out = derwin (frame, fh - 1, fw, 0, 0);
    }
  return *text_out ? 0 : -1;
}

void
view_resize_windows_ex (WINDOW *frame, WINDOW *text, int show_border)
{
  if (!frame || !text)
    return;
  int fh, fw;
  getmaxyx (frame, fh, fw);
  if (show_border)
    {
      if (fh < 3 || fw < 3)
        return;
      wresize (text, fh - 2, fw - 2);
      mvderwin (text, 1, 1);
    }
  else
    {
      if (fh < 2 || fw < 1)
        return;
      wresize (text, fh - 1, fw);
      mvderwin (text, 0, 0);
    }
}

void
view_recreate_text_window (WINDOW *frame, WINDOW **text, int show_border)
{
  if (!frame || !text)
    return;
  if (*text)
    {
      delwin (*text);
      *text = NULL;
    }
  view_create_text_window_ex (frame, text, show_border);
  if (*text)
    wbkgd (*text, COLOR_PAIR(1));
}

/* Helpers to get text window dimensions safely (fallback to LINES/COLS) */
static int
text_win_rows (WINDOW *text)
{
  if (text)
    return getmaxy (text);
  return (LINES > 2) ? LINES - 2 : 0;
}

static int
text_win_cols (WINDOW *text)
{
  if (text)
    return getmaxx (text);
  return (COLS > 2) ? COLS - 2 : 0;
}

static int
frame_win_cols (WINDOW *frame)
{
  if (frame)
    return getmaxx (frame);
  return COLS;
}

static int
frame_win_rows (WINDOW *frame)
{
  if (frame)
    return getmaxy (frame);
  return LINES;
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
          vis += utf8_char_width (line + i, len - i);
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
// Compute color array for a line based on syntax highlighting
// Returns an array of color indices (1-based, as per ncurses COLOR_PAIR)
// Caller must free the returned array.
// highlight_pair: 1 to disable, else enable
int *
compute_line_colors (const char *full_line, int line_len,
                     int highlight_pair, EditorConfig *config, int *brace_level, int *brace_top, int *brace_stack, int *kw_level, int *kw_top, int *kw_stack)
{
  int maxll = (config && config->performance.max_line_length > 0) ? config->performance.max_line_length : 10000;
  if (highlight_pair == 1 || line_len > maxll)
    {
      return NULL;              // No colors if disabled or line too long
    }
  int *colors = xmalloc (line_len * sizeof (int));
  if (!colors)
    return NULL;
  // Initialize all to normal color (1)
  for (int i = 0; i < line_len; i++)
    {
		



      colors[i] = 1;
    }
  if (!config)
    return colors;              // Safety check
  // Use pre-paired keyword pairs parsed once by load_editor_config
  KeywordPair *pairs = (KeywordPair *) config->syntax_pairs;
  int num_pairs = config->num_syntax_pairs;
  // Copy starting stacks and levels
  // State is updated in place via pointers
  int word_start = 0;
  int in_word = 0;
  int word_count = 0;
  for (int i = 0; i < line_len; i++)
    {
		



      char c = full_line[i];
      // Handle meta symbols (braces, etc.)
      if (strchr (meta_symbols, c))
        {
          if (c == ';')
            {
              colors[i] = 3;    // Red for semicolons
            }
          else if (c == '{' || c == '(' || c == '[')
            {
              // Opening brace
               if (*brace_top < 256)
                 brace_stack[*brace_top] = *brace_level;
               (*brace_top)++;
               int lvl = *brace_level;
               colors[i] = 4 + (lvl > 4 ? 3 : lvl - 1);  // Levels 4-7
               (*brace_level)++;
            }
          else if (c == '}' || c == ')' || c == ']')
            {
              // Closing brace
              if (*brace_top > 0)
                {
                  int lvl = brace_stack[--(*brace_top)];
                  colors[i] = 4 + (lvl > 4 ? 3 : lvl - 1);
                }
              else
                {
                  colors[i] = 4;        // Default
                }
              *brace_level = (*brace_level > 1) ? *brace_level - 1 : 1;
            }
          else
            {
              colors[i] = 4;    // Other meta like ','
            }
          continue;             // Skip word processing for meta chars
        }
      // Handle words (alphanumeric or underscore)
      if (isalnum (c) || c == '_')
        {
          if (!in_word)
            {
              word_start = i;
              in_word = 1;
            }
        }
      else
        {
          if (in_word)
            {
              // Process the word
              if (word_count++ > 100)
                {
                  in_word = 0;
                  continue;     // Skip if too many words
                }
              int wlen = i - word_start;
              char *word = xmalloc (wlen + 1);
              if (word)
                {
                  memcpy (word, &full_line[word_start], wlen);
                  word[wlen] = '\0';
                  int colored = 0;
                  // Check for paired keywords
                  for (int p = 0; p < num_pairs && !colored; p++)
                    {
		



                      if (strcmp (word, pairs[p].open) == 0)
                        {
                          if (*kw_top < 100)
                            kw_stack[(*kw_top)++] = *kw_level;
                          int lvl = *kw_level;
                          int color = 3 + (lvl > 4 ? 4 : lvl);
                          for (int j = word_start; j < i; j++)
                            colors[j] = color;
                          (*kw_level)++;
                          colored = 1;
                        }
                      else if (strcmp (word, pairs[p].close) == 0)
                        {
                          if (*kw_top > 0)
                            {
                              int lvl = kw_stack[--(*kw_top)];
                              int color = 3 + (lvl > 4 ? 4 : lvl);
                              for (int j = word_start; j < i; j++)
                                colors[j] = color;
                            }
                          *kw_level = (*kw_level > 1) ? *kw_level - 1 : 1;
                          colored = 1;
                        }
                    }
                  // Check for reserved words (if not paired)
                  if (!colored
                      && config_is_reserved_word (config, word))
                    {
                      for (int j = word_start; j < i; j++)
                        colors[j] = 8;
                    }
                  free (word);
                }
              in_word = 0;
            }
        }
    }
  // Handle word at end of line
  if (in_word)
    {
      if (word_count++ <= 100)
        {
          int wlen = line_len - word_start;
          char *word = xmalloc (wlen + 1);
          if (word)
            {
              memcpy (word, &full_line[word_start], wlen);
              word[wlen] = '\0';
              int colored = 0;
              for (int p = 0; p < num_pairs && !colored; p++)
                {
		



                  if (strcmp (word, pairs[p].open) == 0)
                    {
                      if (*kw_top < 100)
                        kw_stack[(*kw_top)++] = *kw_level;
                      int lvl = *kw_level;
                      int color = 3 + (lvl > 4 ? 4 : lvl);
                      for (int j = word_start; j < line_len; j++)
                        colors[j] = color;
                      (*kw_level)++;
                      colored = 1;
                    }
                  else if (strcmp (word, pairs[p].close) == 0)
                    {
                      if (*kw_top > 0)
                        {
                          int lvl = kw_stack[--(*kw_top)];
                          int color = 3 + (lvl > 4 ? 4 : lvl);
                          for (int j = word_start; j < line_len; j++)
                            colors[j] = color;
                        }
                      *kw_level = (*kw_level > 1) ? *kw_level - 1 : 1;
                      colored = 1;
                    }
                }
              if (!colored
                  && config_is_reserved_word (config, word))
                {
                  for (int j = word_start; j < line_len; j++)
                    colors[j] = 8;
                }
              free (word);
            }
        }
    }
  return colors;
}
// Helper to update nesting state for a line without coloring
static void update_nesting(const char *full_line, int line_len, int *brace_level, int *brace_top, int *brace_stack, int *kw_level, int *kw_top, int *kw_stack, KeywordPair *pairs, int num_pairs) {
  int word_start = 0;
  int in_word = 0;
  for (int i = 0; i < line_len; i++) {
		



    char c = full_line[i];
    // Handle braces
    if (strchr(";,{}()[]", c)) {
      if (c == '{' || c == '(' || c == '[') {
        if (*brace_top < 256) brace_stack[(*brace_top)++] = *brace_level;
        (*brace_level)++;
      } else if (c == '}' || c == ')' || c == ']') {
        if (*brace_top > 0) --(*brace_top);
        *brace_level = (*brace_level > 1) ? *brace_level - 1 : 1;
      }
      continue;
    }
    // Handle words
    if (isalnum(c) || c == '_') {
      if (!in_word) {
        word_start = i;
        in_word = 1;
      }
    } else {
      if (in_word) {
        int wlen = i - word_start;
        char *word = malloc(wlen + 1);
        if (!word) continue;
        memcpy(word, full_line + word_start, wlen);
        word[wlen] = '\0';
        for (int p = 0; p < num_pairs; p++) {
		



          if (strcmp(word, pairs[p].open) == 0) {
            if (*kw_top < 100) kw_stack[(*kw_top)++] = *kw_level;
            (*kw_level)++;
            break;
          } else if (strcmp(word, pairs[p].close) == 0) {
            if (*kw_top > 0) --(*kw_top);
            *kw_level = (*kw_level > 1) ? *kw_level - 1 : 1;
            break;
          }
        }
        free(word);
        in_word = 0;
      }
    }
  }
  // Word at end
  if (in_word) {
    int wlen = line_len - word_start;
    char *word = malloc(wlen + 1);
    if (!word) return;
    memcpy(word, full_line + word_start, wlen);
    word[wlen] = '\0';
    for (int p = 0; p < num_pairs; p++) {
		



      if (strcmp(word, pairs[p].open) == 0) {
        if (*kw_top < 100) kw_stack[(*kw_top)++] = *kw_level;
        (*kw_level)++;
      } else if (strcmp(word, pairs[p].close) == 0) {
        if (*kw_top > 0) --(*kw_top);
        *kw_level = (*kw_level > 1) ? *kw_level - 1 : 1;
      }
    }
    free(word);
  }
}
// Compute starting nesting state for a line (with caching)
void get_starting_levels(Buffer *buf, int start_line, int *brace_level, int *brace_top, int brace_stack[], int *kw_level, int *kw_top, int kw_stack[], EditorConfig *config) {
  // Check if cache is available and valid for previous line
  if (buf->nesting_cache && start_line > 0 && buf->nesting_cache[start_line - 1].valid) {
    // Use cached state from previous line
    *brace_level = buf->nesting_cache[start_line - 1].brace_level;
    *brace_top = buf->nesting_cache[start_line - 1].brace_top;
    memcpy(brace_stack, buf->nesting_cache[start_line - 1].brace_stack, sizeof(int) * 256);
    *kw_level = buf->nesting_cache[start_line - 1].kw_level;
    *kw_top = buf->nesting_cache[start_line - 1].kw_top;
    memcpy(kw_stack, buf->nesting_cache[start_line - 1].kw_stack, sizeof(int) * 100);
    return;
  }

  *brace_level = 1;
  *brace_top = 0;
  *kw_level = 1;
  *kw_top = 0;
  // Use pre-parsed pairs from config
  KeywordPair *pairs = (KeywordPair *) config->syntax_pairs;
  int num_pairs = config->num_syntax_pairs;
  // Update state for each previous line
  for (int l = 0; l < start_line; l++) {
		



    char *line = buffer_get_line(buf, l);
    int len = strlen(line);
    update_nesting(line, len, brace_level, brace_top, brace_stack, kw_level, kw_top, kw_stack, pairs, num_pairs);
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
} LineColorEntry;

static LineColorEntry g_ccache[CCACHE_SLOTS];

static unsigned long
hash_start_state (int bl, int bt, const int *bs, int kl, int kt, const int *ks)
{
  unsigned long h = 1469598103934665603UL;
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
                 int *kw_level, int *kw_top, int kw_stack[])
{
  static int *overflow_colors;
  static int overflow_cap;
  int maxll = (config && config->performance.max_line_length > 0)
    ? config->performance.max_line_length : 10000;
  if (highlight_pair == 1 || len > maxll || len > CCACHE_MAX_LINE)
    {
      int *computed = compute_line_colors (line, len, highlight_pair, config,
                                           brace_level, brace_top, brace_stack,
                                           kw_level, kw_top, kw_stack);
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
                                      *kw_level, *kw_top, kw_stack);
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
      return slot->has_colors ? slot->colors : NULL;
    }
  int *computed = compute_line_colors (line, len, highlight_pair, config,
                                       brace_level, brace_top, brace_stack,
                                       kw_level, kw_top, kw_stack);
  if (!slot->colors)
    slot->colors = xmalloc ((size_t) CCACHE_MAX_LINE * sizeof (int));
  if (!slot->text)
    slot->text = xmalloc ((size_t) CCACHE_MAX_LINE + 1);
  if (slot->colors && slot->text)
    {
      slot->used = 1;
      slot->line_no = logical_line;
      slot->generation = buf->edit_generation;
      slot->start_hash = hash_start_state (*brace_level, *brace_top,
                                           brace_stack, *kw_level, *kw_top,
                                           kw_stack);
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
    }
  return computed;
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
print_highlighted (WINDOW *tw, int y, int x, const char *full_line, int line_len,
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
  get_starting_levels(buf, logical_line, &brace_level, &brace_top, brace_stack, &kw_level, &kw_top, kw_stack, config);
  int *colors = colors_for_line (buf, logical_line, full_line, line_len,
                                 highlight_pair, config,
                                 &brace_level, &brace_top, brace_stack,
                                 &kw_level, &kw_top, kw_stack);

  // Cache the nesting state after processing this line (for next line's use)
  if (buf->nesting_cache && logical_line < buf->capacity) {
    buf->nesting_cache[logical_line].valid = 1;
    buf->nesting_cache[logical_line].brace_level = brace_level;
    buf->nesting_cache[logical_line].brace_top = brace_top;
    memcpy(buf->nesting_cache[logical_line].brace_stack, brace_stack, sizeof(int) * 256);
    buf->nesting_cache[logical_line].kw_level = kw_level;
    buf->nesting_cache[logical_line].kw_top = kw_top;
    memcpy(buf->nesting_cache[logical_line].kw_stack, kw_stack, sizeof(int) * 100);
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
        mvwaddnstr (tw, y, x, &full_line[start], end - start);
      else
        mvaddnstr (y, x, &full_line[start], end - start);
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
        mvwaddnstr (tw, y, x, expanded, expanded_len);
      else
        mvaddnstr (y, x, expanded, expanded_len);
      return;
    }

  /* Print runs of same color as complete UTF-8 sequences */
  int win_cols = tw ? getmaxx (tw) : COLS;
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
        wattron (tw, COLOR_PAIR (color));
      else
        attron (COLOR_PAIR (color));
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
            mvwaddnstr (tw, y, run_x, expanded + j, clen);
          else
            mvaddnstr (y, run_x, expanded + j, clen);
          run_x += cw;
          j += clen;
        }
      if (tw)
        wattroff (tw, COLOR_PAIR (color));
      else
        attroff (COLOR_PAIR (color));
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
// Either may be NULL in headless tests; falls back to stdscr/LINES/COLS
void
draw_initial (WINDOW *frame, WINDOW *text, Buffer *buf, int *scroll_row,
               int *scroll_col, int cursor_line, int cursor_col,
               int show_line_numbers, int syntax_highlight,
               int *cursor_screen_y, int *cursor_screen_x,
               EditorConfig *config)
{
  WINDOW *fw = frame ? frame : stdscr;
  WINDOW *tw = text;
  // If only one window passed historically (win==stdscr), treat as frame and create fallback
  if (!tw && fw)
    {
      // Legacy: no text subwindow — use fw dimensions with border inset math
      // Keep old clear/box behaviour for compat
      if (fw == stdscr)
        clear ();
      else
        {
          wclear (fw);
        }
      if (!config || config->display.show_border)
        box (fw, 0, 0);
      int num_digits = calculate_digits (buffer_num_lines (buf));
      int num_width = show_line_numbers ? num_digits + 1 : 0;
      int max_lines = (LINES > 2) ? LINES - 2 : 0;
      for (int i = 0; i < max_lines; i++)
        {
          int line_idx = *scroll_row + (int) i;
          if (line_idx >= buffer_num_lines (buf))
            break;
          if (show_line_numbers)
            {
              mvprintw (1 + i, 1, "%*u ", num_digits, line_idx + 1);
            }
          char *line = buffer_get_line (buf, line_idx);
          int line_len = strlen (line);
          int start_col = *scroll_col;
          if (start_col < line_len)
            {
              int print_len = line_len - start_col;
              int max_print = (int) (COLS - 2 - num_width);
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
      if (screen_x > COLS - 1)
        screen_x = COLS - 1;
      if (cursor_screen_y) *cursor_screen_y = (int) screen_y;
      if (cursor_screen_x) *cursor_screen_x = (int) screen_x;
      move (screen_y, screen_x);
      if (fw == stdscr)
        refresh ();
      else
        wrefresh (fw);
      view_invalidate ();
      return;
    }

  // Subwindow path: frame owns border, text owns content
  // Clear/erase only text; box only frame (never cleared in STATUS_ONLY/LINES paths)
  int do_border = !config || config->display.show_border;
  if (fw)
    {
      werase (fw);
      if (do_border)
        box (fw, 0, 0);
      // Ensure text subwindow background matches
      if (tw)
        werase (tw);
    }
  else if (tw)
    {
      werase (tw);
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
            mvwprintw (tw, i, 0, "%*u ", num_digits, line_idx + 1);
          else
            mvprintw (1 + i, 1, "%*u ", num_digits, line_idx + 1);
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
  // Calculate cursor screen position (stdscr coords for caller, tw-relative for wmove)
  int y_diff = (cursor_line >= *scroll_row) ? cursor_line - *scroll_row : 0;
  int screen_y = 1 + (int) y_diff;
  char *line = buffer_get_line (buf, cursor_line);
  int line_len2 = strlen (line);
  int vis_scroll = visual_column (line, line_len2, *scroll_col, config->display.tab_width);
  int vis_cursor = visual_column (line, line_len2, cursor_col, config->display.tab_width);
  free(line);
  int x_diff = (vis_cursor >= vis_scroll) ? (int) (vis_cursor - vis_scroll) : 0;
  int screen_x = 1 + num_width + (int) x_diff;
  // Clamp to text window bounds (text_cols is tw width, last valid stdscr x is 1+text_cols)
  // Clamp screen_x which is stdscr coords (1-based)
  if (screen_x < 1 + num_width)
    screen_x = 1 + num_width;
  if (tw)
    {
      // tw cols is text area; max stdscr x is 1+tw_cols (since border at 0)
      int max_sx = 1 + text_cols;
      if (screen_x > max_sx)
        screen_x = max_sx;
    }
  else
    {
      if (screen_x > COLS - 1)
        screen_x = COLS - 1;
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
      wmove (tw, wy, wx);
      if (fw)
        {
          wnoutrefresh (fw);
          wnoutrefresh (tw);
          doupdate ();
        }
      else
        wrefresh (tw);
    }
  else
    {
      move (screen_y, screen_x);
      if (fw == stdscr)
        refresh ();
      else if (fw)
        wrefresh (fw);
    }
  view_invalidate ();
}
// Repaint one content row in truncate (no-wrap) mode, erasing stale cells
// tw is the inset text subwindow (NULL => fallback to stdscr for headless tests)
static void
render_content_row (WINDOW *tw, int visual_row, int logical_line, Buffer *buf,
                     int *scroll_col, int available_width, int num_digits,
                     int num_width, int show_line_numbers,
                     int syntax_highlight,
                     int sel_start_line, int sel_start_col,
                     int sel_end_line, int sel_end_col, int selection_active,
                     EditorConfig *config)
{
  if (tw)
    {
      wmove (tw, visual_row, 0);
      wclrtoeol (tw);
    }
  else
    {
      move (1 + visual_row, 1);
      clrtoeol ();
    }
  char *line = NULL;
  int len = 0;

  if (show_line_numbers)
    {
      if (tw)
        mvwprintw (tw, visual_row, 0, "%*u ", num_digits, logical_line + 1);
      else
        mvprintw (1 + visual_row, 1, "%*u ", num_digits, logical_line + 1);
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
            wattron (tw, COLOR_PAIR (2));
          mvwaddnstr (tw, visual_row, x, &line[pos], print_len);
          if (syntax_highlight)
            wattroff (tw, COLOR_PAIR (2));
        }
      else
        {
          if (syntax_highlight)
            attron (COLOR_PAIR (2));
          mvaddnstr (1 + visual_row, x, &line[pos], print_len);
          if (syntax_highlight)
            attroff (COLOR_PAIR (2));
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
// frame is border window (stdscr), bottom row is frame's LINES-1
static void
render_status_bar (WINDOW *frame, Editor *ed, Buffer *buf, int cursor_line, int cursor_col,
                   int search_mode, char *search_buffer, int replace_step,
                   char *replace_buffer, EditorConfig *config)
{
  int fcols = frame_win_cols (frame);
  int frows = frame_win_rows (frame);
  // Clear bottom row of frame (status line)
  if (frame)
    {
      wmove (frame, frows - 1, 1);
      wclrtoeol (frame);
    }
  else
    {
      move (LINES - 1, 1);
      clrtoeol ();
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
  if (replace_step == 1)
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
    mvwprintw (frame, frows - 1, 1, "%s", status_line);
  else
    mvprintw (LINES - 1, 1, "%s", status_line);
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
draw_update (WINDOW *frame, WINDOW *text, Buffer *buf, int *scroll_row, int *scroll_col,
              int cursor_line, int cursor_col, int show_line_numbers,
              int syntax_highlight, int search_mode, char *search_buffer,
              int selection_start_line, int selection_start_col,
              int selection_end_line, int selection_end_col,
              int selection_active, int *cursor_screen_y,
              int *cursor_screen_x, int replace_step, char *replace_buffer,
              EditorConfig *config, Editor *ed)
{
  WINDOW *fw = frame ? frame : stdscr;
  WINDOW *tw = text;
  int eff_rows = tw ? getmaxy (tw) : ((LINES > 2) ? LINES - 2 : 0);
  int eff_cols = tw ? getmaxx (tw) : ((COLS > 2) ? COLS - 2 : 0);
  int frame_rows = fw ? getmaxy (fw) : LINES;
  int frame_cols = fw ? getmaxx (fw) : COLS;
  // Adjust scroll to keep cursor visible (no word wrap)
  int max_lines = tw ? getmaxy (tw) : ((LINES > 2) ? LINES - 2 : 0);
  // For legacy fallback when tw==NULL, max_lines is as above
  (void) eff_rows; (void) eff_cols; (void) frame_rows; (void) frame_cols;
  int num_digits = calculate_digits (buffer_num_lines (buf));
  int num_width = show_line_numbers ? num_digits + 1 : 0;
  int available_width = (tw ? getmaxx (tw) : COLS - 2) - num_width;
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
  cur.term_lines = fw ? getmaxy (fw) : LINES;
  cur.term_cols = fw ? getmaxx (fw) : COLS;
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
          werase (fw);
          if (!config || config->display.show_border)
            box (fw, 0, 0);
        }
      if (tw)
        werase (tw);
      else
        {
          for (int r = 1; r <= LINES - 2; r++)
            {
              move (r, 1);
              clrtoeol ();
            }
        }
      
  int visual_row = 0;  // Current row on screen
  int logical_line = *scroll_row;  // Current logical line in buffer
  
  while (visual_row < max_lines && logical_line < buffer_num_lines (buf))
    {
		



      char *line = buffer_get_line (buf, logical_line);
      int len = strlen (line);
      int pos = *scroll_col ? *scroll_col : 0;
      
      // Handle selection
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
      
      // Render line — choose wrap vs truncate based on config
      // IMPORTANT: The truncate path below (the else) must remain untouched.
      if (config && config->display.word_wrap)
        {
          // Word wrap ON: break long logical lines across multiple visual rows.
          while (pos < len && visual_row < max_lines)
            {
		



              // Show line number only on first visual row of this logical line
              if (show_line_numbers && pos == 0)
                {
                  if (tw)
                    mvwprintw (tw, visual_row, 0, "%*u ", num_digits, logical_line + 1);
                  else
                    mvprintw (1 + visual_row, 1, "%*u ", num_digits, logical_line + 1);
                }
              
              // Use helper for word-aware breaking (correctly handles tabs)
              int segment_len = get_wrap_break (line, pos, len, available_width,
                                                config ? config->display.tab_width : 8);
              // (old naive word-break logic removed — using get_wrap_break above)
              if (0) { /* old block disabled - safe to delete */
                  int break_at = segment_len;
                  for (int i = segment_len; i > 0; i--)
                    {
		



                      if (line[pos + i] == ' ')
                        {
                          break_at = i;
                          break;
                        }
                    }
                  segment_len = break_at;
                  if (segment_len == 0)
                    segment_len = available_width;  // No space found, break hard
                }
              else
                {
                  segment_len = len - pos;
                }
              
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
                      if (syntax_highlight) wattron (tw, COLOR_PAIR (2));
                      mvwaddnstr (tw, visual_row, x, &line[pos], print_len);
                      if (syntax_highlight) wattroff (tw, COLOR_PAIR (2));
                    }
                  else
                    {
                      if (syntax_highlight) attron (COLOR_PAIR (2));
                      mvaddnstr (1 + visual_row, x, &line[pos], print_len);
                      if (syntax_highlight) attroff (COLOR_PAIR (2));
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
        }
      else
        {
          // Word wrap disabled: truncate (original behavior) — subwindow aware
          if (show_line_numbers)
            {
              if (tw)
                mvwprintw (tw, visual_row, 0, "%*u ", num_digits, logical_line + 1);
              else
                mvprintw (1 + visual_row, 1, "%*u ", num_digits, logical_line + 1);
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
              if (tw)
                print_highlighted (tw, visual_row, x, line, len, pos, print_len,
                                   syntax_highlight ? 4 : 1, config, buf, logical_line);
              else
                print_highlighted (NULL, 1 + visual_row, x, line, len, pos, print_len,
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
                  if (syntax_highlight) wattron (tw, COLOR_PAIR (2));
                  mvwaddnstr (tw, visual_row, x, &line[pos], print_len);
                  if (syntax_highlight) wattroff (tw, COLOR_PAIR (2));
                }
              else
                {
                  if (syntax_highlight) attron (COLOR_PAIR (2));
                  mvaddnstr (1 + visual_row, x, &line[pos], print_len);
                  if (syntax_highlight) attroff (COLOR_PAIR (2));
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
              if (tw)
                print_highlighted (tw, visual_row, x, line, len, pos, print_len,
                                   syntax_highlight ? 4 : 1, config, buf, logical_line);
              else
                print_highlighted (NULL, 1 + visual_row, x, line, len, pos, print_len,
                                   syntax_highlight ? 4 : 1, config, buf, logical_line);
            }
          
          visual_row++;
        }

      logical_line++;
      free(line);
    }

      render_status_bar (fw, ed, buf, cursor_line, cursor_col, search_mode,
                         search_buffer, replace_step, replace_buffer, config);
      // For FULL, border and text already staged; refresh both
      if (fw && tw)
        {
          wnoutrefresh (fw);
          wnoutrefresh (tw);
          doupdate ();
        }
      else if (tw)
        wrefresh (tw);
      else if (fw)
        wrefresh (fw);
      else
        refresh ();
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
  int frows = fw ? getmaxy (fw) : LINES;
  int fcols = fw ? getmaxx (fw) : COLS;
  int tcols = tw ? getmaxx (tw) : COLS - 2;
  (void) fcols; (void) tcols;
  if (screen_y < 1)
    screen_y = 1;
  if (screen_y > frows - 1)
    screen_y = frows - 1;
  if (screen_x < 1 + num_width)
    screen_x = 1 + num_width;
  int max_sx = tw ? (1 + tcols) : (COLS - 2);
  if (screen_x > max_sx)
    screen_x = max_sx;
  if (cursor_screen_y) *cursor_screen_y = (int) screen_y;
  if (cursor_screen_x) *cursor_screen_x = (int) screen_x;
  // Move cursor inside text subwindow (if present) else stdscr
  if (tw)
    {
      int wy = screen_y - 1;
      int wx = screen_x - 1;
      int tw_rows = getmaxy (tw);
      int tw_cols = getmaxx (tw);
      if (wy < 0) wy = 0;
      if (wy >= tw_rows) wy = tw_rows - 1;
      if (wx < 0) wx = 0;
      if (wx >= tw_cols) wx = tw_cols - 1;
      wmove (tw, wy, wx);
      if (fw)
        {
          wnoutrefresh (tw);
          wnoutrefresh (fw);
          // Ensure cursor is on tw
          wnoutrefresh (tw);
          doupdate ();
          // Move hardware cursor via wmove already; set after doupdate
          wmove (tw, wy, wx);
          wrefresh (tw);
        }
      else
        wrefresh (tw);
    }
  else
    {
      move (screen_y, screen_x);
      if (fw == stdscr)
        refresh ();
      else if (fw)
        wrefresh (fw);
      else
        refresh ();
    }
}

/* === Compat wrappers for headless tests (single window == stdscr) === */
void
draw_initial_compat (WINDOW *win, Buffer *buf, int *scroll_row,
               int *scroll_col, int cursor_line, int cursor_col,
               int show_line_numbers, int syntax_highlight,
               int *cursor_screen_y, int *cursor_screen_x,
               EditorConfig *config)
{
  draw_initial (win, NULL, buf, scroll_row, scroll_col, cursor_line, cursor_col,
                show_line_numbers, syntax_highlight, cursor_screen_y, cursor_screen_x, config);
}

void
draw_update_compat (WINDOW *win, Buffer *buf, int *scroll_row, int *scroll_col,
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