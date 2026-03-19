#include "view.h"
#include "config.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <limits.h>
#include <time.h>               // For time functions in status bar
#include <string.h>             // For strdup
// Meta symbols for basic syntax highlighting (braces, semicolons, etc.)
static const char *meta_symbols = ";,{}()[]";
// Structure for keyword pairs (e.g., if-else, for-while)
typedef struct
{
  char open[32];
  char close[32];
} KeywordPair;
// Check if a word is in a comma-separated list of reserved words
static int
is_reserved_word (const char *word, const char *list)
{
  if (!list || !word)
    return 0;
  char *dup = malloc (strlen (list) + 1);
  if (!dup)
    return 0;
  strcpy (dup, list);
  char *token = strtok (dup, ",");
  while (token)
    {
      if (strcmp (word, token) == 0)
        {
          free (dup);
          return 1;
        }
      token = strtok (NULL, ",");
    }
  free (dup);
  return 0;
}
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
// Calculate how many visual rows a logical line uses when word wrapping
int
visual_rows_for_line (const char *line, int available_width)
{
  if (!line || available_width <= 0)
    return 1;

  int len = strlen (line);
  if (len == 0)
    return 1;

  int rows = 0;
  int pos = 0;

  while (pos < len)
    {
      int segment_len = available_width;

      // Break at space if possible
      if (pos + segment_len < len)
        {
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
            segment_len = available_width;
        }
      else
        {
          segment_len = len - pos;
        }

      pos += segment_len;
      rows++;
    }

  return rows;
}
// Calculate total visual lines in buffer (accounting for word wrap)
static int
calculate_total_visual_lines (Buffer *buf, EditorConfig *config, int num_width)
{
  if (!config->display.word_wrap)
    return buffer_num_lines (buf);

  int available_width = COLS - 2 - num_width;
  if (available_width <= 0)
    available_width = 40;  // Fallback

  int total = 0;
  for (int i = 0; i < buffer_num_lines (buf); i++)
    {
      const char *line = buffer_get_line (buf, i);
      total += visual_rows_for_line (line, available_width);
    }
  return total;
}

// Get the 0-based visual row for a given logical column
int get_visual_row_for_column(const char* line, int col, int available_width, int tab_width) {
  if (!line || available_width <= 0) return 0;

  int len = strlen(line);
  int pos = 0;
  int vis_row = 0;
  int current_vis_col = 0;

  while (pos < len && pos < col) {
    int segment_len = available_width - current_vis_col;

    if (segment_len <= 0) {
      vis_row++;
      current_vis_col = 0;
      segment_len = available_width;
    }

    int break_at = segment_len;
    for (int i = segment_len; i > 0; i--) {
      if (pos + i >= len) break;
      if (line[pos + i] == ' ') {
        break_at = i;
        break;
      }
    }
    if (break_at == 0) break_at = segment_len;

    int actual_len = (pos + break_at > col) ? (col - pos) : break_at;

    for (int i = 0; i < actual_len; i++) {
      if (line[pos + i] == '\t') {
        int spaces = tab_width - (current_vis_col % tab_width);
        current_vis_col += spaces;
      } else {
        current_vis_col++;
      }
    }

    pos += actual_len;

    if (current_vis_col >= available_width) {
      vis_row++;
      current_vis_col = 0;
    }
  }

  return vis_row;
}

// Get the starting logical column for a given visual row
int get_start_column_for_visual_row(const char* line, int target_vis_row, int available_width, int tab_width) {
  if (!line || available_width <= 0 || target_vis_row < 0) return 0;

  int len = strlen(line);
  int pos = 0;
  int vis_row = 0;
  int current_vis_col = 0;

  while (pos < len && vis_row < target_vis_row) {
    int segment_len = available_width - current_vis_col;

    if (segment_len <= 0) {
      vis_row++;
      current_vis_col = 0;
      segment_len = available_width;
      continue;
    }

    int break_at = segment_len;
    for (int i = segment_len; i > 0; i--) {
      if (pos + i >= len) break;
      if (line[pos + i] == ' ') {
        break_at = i;
        break;
      }
    }
    if (break_at == 0) break_at = segment_len;

    int actual_move = break_at;

    for (int i = 0; i < actual_move; i++) {
      if (pos + i >= len) break;
      if (line[pos + i] == '\t') {
        int spaces = tab_width - (current_vis_col % tab_width);
        current_vis_col += spaces;
      } else {
        current_vis_col++;
      }

      if (current_vis_col >= available_width) {
        pos += i + 1;
        vis_row++;
        current_vis_col = 0;
        break;
      }
    }

    if (current_vis_col < available_width) {
      pos += actual_move;
    }
  }

  return pos;
}
// Compute the visual column for a given logical position in a line
static int
visual_column (const char *line, int len, int logical_pos,
               int tab_width)
{
  int vis = 0;
  for (int i = 0; i < logical_pos && i < len; i++)
    {
      if (line[i] == '\t')
        {
          vis += tab_width - (vis % tab_width);
        }
      else
        {
          vis++;
        }
    }
  return vis;
}
// Compute color array for a line based on syntax highlighting
// Returns an array of color indices (1-based, as per ncurses COLOR_PAIR)
// Caller must free the returned array.
// highlight_pair: 1 to disable, else enable
int *
compute_line_colors (const char *full_line, int line_len,
                     int highlight_pair, EditorConfig *config, int starting_brace_level, int starting_brace_top, int *starting_brace_stack, int starting_kw_level, int starting_kw_top, int *starting_kw_stack)
{
  if (highlight_pair == 1 || line_len > 10000)
    {
      return NULL;              // No colors if disabled or line too long
    }
  int *colors = malloc (line_len * sizeof (int));
  if (!colors)
    return NULL;
  // Initialize all to normal color (1)
  for (int i = 0; i < line_len; i++)
    {
      colors[i] = 1;
    }
  if (!config)
    return colors;              // Safety check
  // Parse paired keywords into array
  KeywordPair pairs[10];
  memset(pairs, 0, sizeof(pairs));
  int num_pairs = 0;
  if (strlen(config->syntax.paired_keywords) >0 )
    {
      char *dup_pk = malloc (strlen (config->syntax.paired_keywords) + 1);
      if (dup_pk)
        {
          strcpy (dup_pk, config->syntax.paired_keywords);
          char *token = strtok (dup_pk, ",");
          while (token && num_pairs < 10)
            {
              char *dash = strchr (token, '-');
              if (dash)
                {
        *dash = '\0';
                   strncpy (pairs[num_pairs].open, token,
                            sizeof(pairs[num_pairs].open) - 1);
                   pairs[num_pairs].open[sizeof(pairs[num_pairs].open) - 1] = '\0';
                   strncpy (pairs[num_pairs].close, dash + 1,
                            sizeof(pairs[num_pairs].close) - 1);
                   pairs[num_pairs].close[sizeof(pairs[num_pairs].close) - 1] = '\0';
                  num_pairs++;
                }
              token = strtok (NULL, ",");
            }
          free (dup_pk);
        }
    }
  // Copy starting stacks and levels
  int brace_level = starting_brace_level;
  int brace_top = starting_brace_top;
  int brace_stack[256];
  memcpy(brace_stack, starting_brace_stack, starting_brace_top * sizeof(int));

  int kw_level = starting_kw_level;
  int kw_top = starting_kw_top;
  int kw_stack[100];
  memcpy(kw_stack, starting_kw_stack, starting_kw_top * sizeof(int));
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
              if (brace_top < 256)
                brace_stack[brace_top++] = brace_level;
              int lvl = brace_level;
              colors[i] = 4 + (lvl > 4 ? 3 : lvl - 1);  // Levels 4-7
              brace_level++;
            }
          else if (c == '}' || c == ')' || c == ']')
            {
              // Closing brace
              if (brace_top > 0)
                {
                  int lvl = brace_stack[--brace_top];
                  colors[i] = 4 + (lvl > 4 ? 3 : lvl - 1);
                }
              else
                {
                  colors[i] = 4;        // Default
                }
              brace_level = (brace_level > 1) ? brace_level - 1 : 1;
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
              char *word = malloc (wlen + 1);
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
                          if (kw_top < 100)
                            kw_stack[kw_top++] = kw_level;
                          int lvl = kw_level;
                          int color = 3 + (lvl > 4 ? 4 : lvl);
                          for (int j = word_start; j < i; j++)
                            colors[j] = color;
                          kw_level++;
                          colored = 1;
                        }
                      else if (strcmp (word, pairs[p].close) == 0)
                        {
                          if (kw_top > 0)
                            {
                              int lvl = kw_stack[--kw_top];
                              int color = 3 + (lvl > 4 ? 4 : lvl);
                              for (int j = word_start; j < i; j++)
                                colors[j] = color;
                            }
                          kw_level = (kw_level > 1) ? kw_level - 1 : 1;
                          colored = 1;
                        }
                    }
                  // Check for reserved words (if not paired)
                  if (!colored
                      && is_reserved_word (word,
                                           config->syntax.reserved_words))
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
          char *word = malloc (wlen + 1);
          if (word)
            {
              memcpy (word, &full_line[word_start], wlen);
              word[wlen] = '\0';
              int colored = 0;
              for (int p = 0; p < num_pairs && !colored; p++)
                {
                  if (strcmp (word, pairs[p].open) == 0)
                    {
                      if (kw_top < 100)
                        kw_stack[kw_top++] = kw_level;
                      int lvl = kw_level;
                      int color = 3 + (lvl > 4 ? 4 : lvl);
                      for (int j = word_start; j < line_len; j++)
                        colors[j] = color;
                      kw_level++;
                      colored = 1;
                    }
                  else if (strcmp (word, pairs[p].close) == 0)
                    {
                      if (kw_top > 0)
                        {
                          int lvl = kw_stack[--kw_top];
                          int color = 3 + (lvl > 4 ? 4 : lvl);
                          for (int j = word_start; j < line_len; j++)
                            colors[j] = color;
                        }
                      kw_level = (kw_level > 1) ? kw_level - 1 : 1;
                      colored = 1;
                    }
                }
              if (!colored
                  && is_reserved_word (word, config->syntax.reserved_words))
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

extern char* strdup(char*);

// Helper to update nesting state for a line without coloring
static void update_nesting(const char *full_line, int line_len, int *brace_level, int *brace_top, int brace_stack[256], int *kw_level, int *kw_top, int kw_stack[100], KeywordPair *pairs, int num_pairs) {
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
        char word[32] = {0};
        if (wlen < 31) memcpy(word, full_line + word_start, wlen);

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
        in_word = 0;
      }
    }
  }

  // Word at end
  if (in_word) {
    int wlen = line_len - word_start;
    char word[32] = {0};
    if (wlen < 31) memcpy(word, full_line + word_start, wlen);

    for (int p = 0; p < num_pairs; p++) {
      if (strcmp(word, pairs[p].open) == 0) {
        if (*kw_top < 100) kw_stack[(*kw_top)++] = *kw_level;
        (*kw_level)++;
      } else if (strcmp(word, pairs[p].close) == 0) {
        if (*kw_top > 0) --(*kw_top);
        *kw_level = (*kw_level > 1) ? *kw_level - 1 : 1;
      }
    }
  }
}

// Compute starting nesting state for a line
void get_starting_levels(Buffer *buf, int start_line, int *brace_level, int *brace_top, int brace_stack[256], int *kw_level, int *kw_top, int kw_stack[100], EditorConfig *config) {
  *brace_level = 1;
  *brace_top = 0;
  *kw_level = 1;
  *kw_top = 0;

  // Parse paired keywords
  KeywordPair pairs[10];
  memset(pairs, 0, sizeof(pairs));
  int num_pairs = 0;
  if (strlen(config->syntax.paired_keywords) > 0) {
    char *dup_pk = strdup(config->syntax.paired_keywords);
    if (dup_pk) {
      char *token = strtok(dup_pk, ",");
      while (token && num_pairs < 10) {
        char *dash = strchr(token, '-');
        if (dash) {
          *dash = '\0';
          strncpy(pairs[num_pairs].open, token, sizeof(pairs[num_pairs].open) - 1);
          pairs[num_pairs].open[sizeof(pairs[num_pairs].open) - 1] = '\0';
          strncpy(pairs[num_pairs].close, dash + 1, sizeof(pairs[num_pairs].close) - 1);
          pairs[num_pairs].close[sizeof(pairs[num_pairs].close) - 1] = '\0';
          num_pairs++;
        }
        token = strtok(NULL, ",");
      }
      free(dup_pk);
    }
  }

  // Update state for each previous line
  for (int l = 0; l < start_line; l++) {
    const char *line = buffer_get_line(buf, l);
    int len = strlen(line);
    update_nesting(line, len, brace_level, brace_top, brace_stack, kw_level, kw_top, kw_stack, pairs, num_pairs);
  }
}

// Print a highlighted substring of a line
static void
print_highlighted (int y, int x, const char *full_line, int line_len,
                   int start, int len, int highlight_pair,
                   EditorConfig *config, Buffer *buf, int logical_line)
{
  int brace_level = 1;
int brace_top = 0;
int brace_stack[256] = {0};
int kw_level = 1;
int kw_top = 0;
int kw_stack[100] = {0};
get_starting_levels(buf, logical_line, &brace_level, &brace_top, brace_stack, &kw_level, &kw_top, kw_stack, config);
int *colors = compute_line_colors(full_line, line_len, highlight_pair, config, brace_level, brace_top, brace_stack, kw_level, kw_top, kw_stack);
  // Compute expanded length
  int expanded_len = 0;
  int current_vis = 0;
  for (int i = start; i < start + len && i < line_len; i++)
    {
      if (full_line[i] == '\t')
        {
          int spaces =
            config->display.tab_width -
            (current_vis % config->display.tab_width);
          expanded_len += spaces;
          current_vis += spaces;
        }
      else
        {
          expanded_len++;
          current_vis++;
        }
    }
  // Build expanded string
  char *expanded = malloc (expanded_len + 1);
  if (!expanded)
    {
      if (colors)
        free (colors);
      mvprintw (y, x, "%.*s", (int) (len <= INT_MAX ? len : INT_MAX),
                &full_line[start]);
      return;
    }
  int exp_idx = 0;
  current_vis = 0;
  for (int i = start; i < start + len && i < line_len; i++)
    {
      if (full_line[i] == '\t')
        {
          int spaces =
            config->display.tab_width -
            (current_vis % config->display.tab_width);
          for (int s = 0; s < spaces; s++)
            {
              expanded[exp_idx++] = ' ';
            }
          current_vis += spaces;
        }
      else
        {
          expanded[exp_idx++] = full_line[i];
          current_vis++;
        }
    }
  expanded[exp_idx] = '\0';
  // If no colors, print expanded
  if (!colors)
    {
      mvprintw (y, x, "%.*s", (int) expanded_len, expanded);
      free (expanded);
      return;
    }
  // Build expanded colors
  int *expanded_colors = malloc (expanded_len * sizeof (int));
  if (!expanded_colors)
    {
      free (colors);
      mvprintw (y, x, "%.*s", (int) expanded_len, expanded);
      free (expanded);
      return;
    }
  int log_idx = start;
  exp_idx = 0;
  current_vis = 0;
  while (log_idx < start + len && log_idx < line_len)
    {
      int color = colors[log_idx];
      if (full_line[log_idx] == '\t')
        {
          int spaces =
            config->display.tab_width -
            (current_vis % config->display.tab_width);
          for (int s = 0; s < spaces; s++)
            {
              expanded_colors[exp_idx++] = color;
            }
          current_vis += spaces;
        }
      else
        {
          expanded_colors[exp_idx++] = color;
          current_vis++;
        }
      log_idx++;
    }
  // Print
  int current_x = x;
  for (int i = 0; i < expanded_len; i++)
    {
      if (current_x >= COLS)
        break;
      attron (COLOR_PAIR (expanded_colors[i]));
      mvprintw (y, current_x++, "%c", expanded[i]);
      attroff (COLOR_PAIR (expanded_colors[i]));
    }
  free (colors);
  free (expanded);
  free (expanded_colors);
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
      const char *line = buffer_get_line (buf, cursor_line);
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
    }
  else
    {
      // Insert a tab character
      // Assuming buffer_insert_char exists
      buffer_insert_char (buf, cursor_line, cursor_col, '\t');
      // Update cursor_col += 1;
    }
}
// Draw the initial editor view
void
draw_initial (WINDOW *win, Buffer *buf, int *scroll_row,
              int *scroll_col, int cursor_line, int cursor_col,
              int show_line_numbers, int syntax_highlight,
              int *cursor_screen_y, int *cursor_screen_x,
              EditorConfig *config)
{
  clear ();
  box (win, 0, 0);
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
      const char *line = buffer_get_line (buf, line_idx);
      int line_len = strlen (line);
      int start_col = *scroll_col;
      if (start_col < line_len)
        {
          int print_len = line_len - start_col;
          int max_print = (int) (COLS - 2 - num_width);
          if (print_len > max_print)
            print_len = max_print;
          print_highlighted (1 + i, 1 + num_width, line, line_len, start_col,
                             print_len, syntax_highlight ? 4 : 1, config, buf, line_idx);
        }
    }
  // Calculate cursor screen position
  int y_diff =
    (cursor_line >= *scroll_row) ? cursor_line - *scroll_row : 0;
  int screen_y = 1 + (int) y_diff;
  const char *line = buffer_get_line (buf, cursor_line);
  int line_len = strlen (line);
  int vis_scroll =
    visual_column (line, line_len, *scroll_col, config->display.tab_width);
  int vis_cursor =
    visual_column (line, line_len, cursor_col, config->display.tab_width);
  int x_diff =
    (vis_cursor >= vis_scroll) ? (int) (vis_cursor - vis_scroll) : 0;
  int screen_x = 1 + num_width + (int) x_diff;
  // Clamp to screen bounds
  if (screen_x < 1 + num_width)
    screen_x = 1 + num_width;
  if (screen_x > COLS - 1)
    screen_x = COLS - 1;
  *cursor_screen_y = (int) screen_y;
  *cursor_screen_x = (int) screen_x;
  move (screen_y, screen_x);
  refresh ();
}
// Draw an updated editor view (with scrolling, selection, status bar)
void
draw_update (WINDOW *win, Buffer *buf, int *scroll_row, int *scroll_col,
             int cursor_line, int cursor_col, int show_line_numbers,
             int syntax_highlight, int search_mode, char *search_buffer,
             int selection_start_line, int selection_start_col,
             int selection_end_line, int selection_end_col,
             int selection_active, int *cursor_screen_y,
             int *cursor_screen_x, int replace_step, char *replace_buffer,
             EditorConfig *config, Editor *ed)
{
  // Adjust scroll to keep cursor visible
  int max_lines = (LINES > 2) ? LINES - 2 : 0;
  if (cursor_line < *scroll_row)
    {
      *scroll_row = cursor_line;
    }
  else if (cursor_line >= *scroll_row + max_lines)
    {
      *scroll_row = cursor_line - max_lines + 1;
    }
  if (*scroll_row >= buffer_num_lines (buf)
      && buffer_num_lines (buf) > max_lines)
    {
      *scroll_row = buffer_num_lines (buf) - max_lines;
    }
  if (*scroll_row >= buffer_num_lines (buf))
    *scroll_row = 0;
  clear ();
  box (win, 0, 0);
  int num_digits = calculate_digits (buffer_num_lines (buf));
  int num_width = show_line_numbers ? num_digits + 1 : 0;
  int available_width = COLS - 2 - num_width;
  
  int visual_row = 0;  // Current row on screen
  int logical_line = *scroll_row;  // Current logical line in buffer
  
  while (visual_row < max_lines && logical_line < buffer_num_lines (buf))
    {
      const char *line = buffer_get_line (buf, logical_line);
      int len = strlen (line);
      int pos = (*scroll_col && !config->display.word_wrap) ? *scroll_col : 0;
      
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
      
      // Render line with word wrap handling
      if (config->display.word_wrap)
        {
          // Word wrap enabled: split across multiple visual rows
          while (pos < len && visual_row < max_lines)
            {
              // Show line number only on first visual row of this logical line
              if (show_line_numbers && pos == ((*scroll_col && !config->display.word_wrap) ? *scroll_col : 0))
                {
                  mvprintw (1 + visual_row, 1, "%*u ", num_digits, logical_line + 1);
                }
              
              int segment_len = available_width;
              
              // Don't split in middle of word: break at space if possible
              if (pos + segment_len < len)
                {
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
              
              int x = 1 + num_width;
              
              // Print segment (respecting selection)
              int seg_end = pos + segment_len;
              
              // Before selection
              if (pos < sel_start && seg_end > pos)
                {
                  int end = (sel_start < seg_end) ? sel_start : seg_end;
                  int print_len = end - pos;
                  print_highlighted (1 + visual_row, x, line, len, pos, print_len,
                                     syntax_highlight ? 4 : 1, config, buf, logical_line);
                  x += print_len;
                  pos += print_len;
                }
              
              // Selection
              if (pos < sel_end && seg_end > pos)
                {
                  int end = (sel_end < seg_end) ? sel_end : seg_end;
                  int print_len = end - pos;
                  if (syntax_highlight)
                    attron (COLOR_PAIR (2));
                  mvprintw (1 + visual_row, x, "%.*s", (int) print_len, &line[pos]);
                  if (syntax_highlight)
                    attroff (COLOR_PAIR (2));
                  x += print_len;
                  pos += print_len;
                }
              
              // After selection
              if (pos < seg_end)
                {
                  int print_len = seg_end - pos;
                  print_highlighted (1 + visual_row, x, line, len, pos, print_len,
                                     syntax_highlight ? 4 : 1, config, buf, logical_line);
                  pos += print_len;
                }
              
              visual_row++;
            }
        }
      else
        {
          // Word wrap disabled: truncate (original behavior)
          if (show_line_numbers)
            {
              mvprintw (1 + visual_row, 1, "%*u ", num_digits, logical_line + 1);
            }
          
          int x = 1 + num_width;
          
          // Print before selection
          if (pos < sel_start)
            {
              int end = (sel_start < len) ? sel_start : len;
              int print_len = end - pos;
              int max_print = available_width;
              if (print_len > max_print)
                print_len = max_print;
              print_highlighted (1 + visual_row, x, line, len, pos, print_len,
                                 syntax_highlight ? 4 : 1, config, buf, logical_line);
              x += print_len;
              pos += print_len;
            }
          // Print selection
          if (pos < sel_end)
            {
              int end = sel_end;
              int print_len = end - pos;
              int max_print = available_width - (x - 1 - num_width);
              if (print_len > max_print)
                print_len = max_print;
              if (syntax_highlight)
                attron (COLOR_PAIR (2));
              mvprintw (1 + visual_row, x, "%.*s", (int) print_len, &line[pos]);
              if (syntax_highlight)
                attroff (COLOR_PAIR (2));
              x += print_len;
              pos += print_len;
            }
          // Print after selection
          if (pos < len)
            {
              int print_len = len - pos;
              int max_print = available_width - (x - 1 - num_width);
              if (print_len > max_print)
                print_len = max_print;
              print_highlighted (1 + visual_row, x, line, len, pos, print_len,
                                 syntax_highlight ? 4 : 1, config, buf, logical_line);
            }
          
          visual_row++;
        }
      
      logical_line++;
    }
  // Status bar
  char status_line[COLS + 1];
  // Temporary message prefix
  char message_prefix[COLS];
  message_prefix[0] = 0;
  if (ed && ed->status_message[0]
      && (time (NULL) - ed->status_message_time) < 5)
    {
      snprintf (message_prefix, COLS, "%s | ",
                ed->status_message);
    }
  if (replace_step == 1)
    {
      snprintf (status_line, COLS, "Replace search: %s",
                search_buffer ? search_buffer : "");
    }
  else if (replace_step == 2)
    {
      snprintf (status_line, COLS, "Replace with: %s",
                replace_buffer ? replace_buffer : "");
    }
  else if (search_mode)
    {
      snprintf (status_line, COLS, "Search: %s",
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
       int total_lines = calculate_total_visual_lines (buf, config, num_width);
       snprintf (pos_str, 64, "Line %d/%d Col %d",
                 cursor_line + 1, total_lines, cursor_col + 1);
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
          int start_pos = (COLS - total_len) / 2;
          if (start_pos < 1)
            start_pos = 1;
          snprintf (status_line, COLS, "%*s%s %s %s %s",
                    start_pos - 1, "", version_str, filename_display, pos_str,
                    time_str);
        }
      else
        {                       // Balanced
          int remaining =
            COLS - 2 - (int) (strlen (pos_str) + strlen (filename_display) +
                              1);
          int left_space = remaining / 2;
          int right_space = remaining - left_space;
          char left[COLS / 2 + 1];
          char right[COLS / 2 + 1];
          if (left_space > 0)
            {
              snprintf (left, COLS / 2 + 1, "%*s%s",
                        left_space - (int) strlen (version_str), "",
                        version_str);
            }
          else
            {
              left[0] = '\0';
            }
          if (right_space > 0)
            {
              snprintf (right, COLS / 2 + 1, "%s%*s", time_str,
                        right_space - (int) strlen (time_str), "");
            }
          else
            {
              right[0] = '\0';
            }
          snprintf (status_line, COLS, "%s%s %s%s", left,
                    filename_display, pos_str, right);
        }
    }
  // Prepend message
  if (message_prefix[0])
    {
      char temp[COLS + 1];
      snprintf (temp, COLS, "%s%s", message_prefix, status_line);
      strncpy (status_line, temp, COLS - 1);
      status_line[COLS - 1] = '\0';
    }
  status_line[COLS - 1] = '\0';
  mvprintw (LINES - 1, 1, "%s", status_line);
  // Cursor position
  int y_diff =
    (cursor_line >= *scroll_row) ? cursor_line - *scroll_row : 0;
  int screen_y = 1 + (int) y_diff;
  const char *line = buffer_get_line (buf, cursor_line);
  int line_len = strlen (line);
  int vis_scroll =
    visual_column (line, line_len, *scroll_col, config->display.tab_width);
  int vis_cursor =
    visual_column (line, line_len, cursor_col, config->display.tab_width);
  int x_diff =
    (vis_cursor >= vis_scroll) ? (int) (vis_cursor - vis_scroll) : 0;
  int screen_x = 1 + num_width + (int) x_diff;
  // Clamp cursor
  if (screen_y < 1)
    screen_y = 1;
  if (screen_y > LINES - 1)
    screen_y = LINES - 1;
  if (screen_x < 1 + num_width)
    screen_x = 1 + num_width;
  if (screen_x > COLS - 1)
    screen_x = COLS - 1;
  *cursor_screen_y = (int) screen_y;
  *cursor_screen_x = (int) screen_x;
  move (screen_y, screen_x);
  refresh ();
}
