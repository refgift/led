#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "model.h"
#include "config.h"
#include "editor.h"

/**
 * test_view.c - Shadow View (text-only, no ncurses)
 * 
 * Tests rendering logic without terminal dependencies.
 * Shadow renderer outputs text representation of what screen SHOULD show.
 */

typedef struct {
  char **rows;        // Array of screen rows (strings)
  int num_rows;       // Number of rows displayed
  int max_cols;       // Screen width
  int cursor_y;       // Cursor screen position
  int cursor_x;
} ScreenBuffer;

ScreenBuffer* screen_create(int rows, int cols) {
  ScreenBuffer *sb = malloc(sizeof(ScreenBuffer));
  sb->rows = malloc(rows * sizeof(char*));
  for (int i = 0; i < rows; i++) {
    sb->rows[i] = malloc(cols + 1);
    memset(sb->rows[i], ' ', cols);
    sb->rows[i][cols] = '\0';
  }
  sb->num_rows = rows;
  sb->max_cols = cols;
  sb->cursor_y = 0;
  sb->cursor_x = 0;
  return sb;
}

void screen_free(ScreenBuffer *sb) {
  if (!sb) return;
  for (int i = 0; i < sb->num_rows; i++) {
    free(sb->rows[i]);
  }
  free(sb->rows);
  free(sb);
}

void screen_put_char(ScreenBuffer *sb, int row, int col, char c) {
  if (row < 0 || row >= sb->num_rows || col < 0 || col >= sb->max_cols) {
    return;
  }
  sb->rows[row][col] = c;
}

void screen_put_string(ScreenBuffer *sb, int row, int col, const char *str, int len) {
  if (row < 0 || row >= sb->num_rows) return;
  for (int i = 0; i < len && col + i < sb->max_cols; i++) {
    sb->rows[row][col + i] = str[i];
  }
}

void screen_print(ScreenBuffer *sb) {
  for (int i = 0; i < sb->num_rows; i++) {
    fprintf(stderr, "|%s|\n", sb->rows[i]);
  }
}

/**
 * Shadow render function - mimics draw_update() but outputs to text buffer
 * 
 * Returns number of visual rows used.
 */
int shadow_render(ScreenBuffer *sb, Buffer *buf, int scroll_row, int scroll_col,
                  int cursor_line, int cursor_col, int show_line_numbers,
                  EditorConfig *config) {
  if (!sb || !buf || !config) return 0;
  
  int max_lines = sb->num_rows;
  int num_width = 0;
  
  // Calculate line number width
  if (show_line_numbers) {
    int num_digits = 0;
    int n = buffer_num_lines(buf);
    if (n == 0) num_digits = 1;
    else {
      while (n > 0) {
        num_digits++;
        n /= 10;
      }
    }
    num_width = num_digits + 1;  // +1 for space after number
  }
  
  int available_width = sb->max_cols - 2 - num_width;  // -2 for borders
  int visual_row = 0;
  int logical_line = scroll_row;
  
  // Render each logical line
  while (visual_row < max_lines && logical_line < buffer_num_lines(buf)) {
    const char *line = buffer_get_line(buf, logical_line);
    int line_len = strlen(line);
    
    // Show line number only on first visual row of this logical line
    int col = 1 + num_width;
    if (show_line_numbers) {
      int num_digits = 0;
      int n = buffer_num_lines(buf);
      if (n == 0) num_digits = 1;
      else {
        while (n > 0) {
          num_digits++;
          n /= 10;
        }
      }
      char num_str[32];
      snprintf(num_str, 32, "%*u ", num_digits, logical_line + 1);
      screen_put_string(sb, visual_row, 1, num_str, strlen(num_str));
    }
    
    // Render line with word wrap consideration
    int pos = 0;  // Position in logical line
    
    if (config->display.word_wrap) {
      // Word wrap enabled: split line across visual rows
      while (pos < line_len && visual_row < max_lines) {
        int segment_len = available_width;
        
        // Don't split in middle of word (simple: break at space if possible)
        if (pos + segment_len < line_len) {
          int break_at = segment_len;
          for (int i = segment_len; i > 0; i--) {
            if (line[pos + i] == ' ') {
              break_at = i;
              break;
            }
          }
          // If no space found, just break at segment_len
          if (break_at == segment_len) {
            break_at = segment_len;
          }
          segment_len = break_at;
        } else {
          segment_len = line_len - pos;
        }
        
        // Print segment
        screen_put_string(sb, visual_row, col, &line[pos], segment_len);
        
        pos += segment_len;
        visual_row++;
        
        // Line numbers only on first row of wrapped line
        col = 1 + num_width;
      }
    } else {
      // Word wrap disabled: truncate at screen width
      int print_len = line_len - pos;
      if (print_len > available_width) {
        print_len = available_width;
        // Could add indicator: screen_put_char(sb, visual_row, col + available_width - 1, '>');
      }
      screen_put_string(sb, visual_row, col, &line[pos], print_len);
      visual_row++;
    }
    
    logical_line++;
  }
  
  return visual_row;
}

// Test assertions (external, defined in test_controller.c)
extern void test_assert(int condition, const char *msg);

/**
 * Test: Long line truncates when word_wrap OFF
 */
void test_truncate_no_wrap(void) {
  fprintf(stderr, "\n=== Test: Truncate (word_wrap OFF) ===\n");
  
  Buffer buf;
  buffer_init(&buf);
  EditorConfig config;
  load_editor_config(&config);
  config.display.word_wrap = 0;  // OFF
  
  // Long line (100 chars)
  const char *long_line = "This is a very long line that extends well beyond the normal terminal width of eighty characters and should be truncated.";
  buffer_insert_line(&buf, 0, long_line);
  
  ScreenBuffer *sb = screen_create(10, 80);
  int rows_used = shadow_render(sb, &buf, 0, 0, 0, 0, 0, &config);
  
  fprintf(stderr, "Rendered output (80 cols, word_wrap OFF):\n");
  screen_print(sb);
  
  // Should use exactly 1 row
  test_assert(rows_used == 1, "truncate: uses 1 visual row");
  
  // Content should be truncated (not full 100 chars visible)
  // Row 0 starts at col 1 (border at 0), content area is 1-78
  char *row = sb->rows[0];
  int content_len = 0;
  for (int i = 1; i < 79; i++) {
    if (row[i] != ' ') content_len = i + 1;
  }
  test_assert(content_len > 0, "truncate: has content");
  test_assert(strncmp(&row[1], "This is a very long", 19) == 0, "truncate: starts correctly");
  
  screen_free(sb);
  buffer_free(&buf);
}

/**
 * Test: Long line wraps across multiple rows when word_wrap ON
 */
void test_wrap_enabled(void) {
  fprintf(stderr, "\n=== Test: Word Wrap (word_wrap ON) ===\n");
  
  Buffer buf;
  buffer_init(&buf);
  EditorConfig config;
  load_editor_config(&config);
  config.display.word_wrap = 1;  // ON
  
  // Long line (100 chars)
  const char *long_line = "This is a very long line that extends well beyond the normal terminal width and should be wrapped across multiple visual rows.";
  buffer_insert_line(&buf, 0, long_line);
  
  ScreenBuffer *sb = screen_create(10, 80);
  int rows_used = shadow_render(sb, &buf, 0, 0, 0, 0, 0, &config);
  
  fprintf(stderr, "Rendered output (80 cols, word_wrap ON):\n");
  screen_print(sb);
  
  // Should use multiple rows (>1)
  test_assert(rows_used > 1, "wrap: uses multiple visual rows");
  test_assert(rows_used < 10, "wrap: doesn't use all rows");
  
  // First row should have content (starting at col 1, not 0, due to border)
  test_assert(strncmp(&sb->rows[0][1], "This is a very", 14) == 0, "wrap: first row starts correctly");
  
  // Later rows should continue the text
  char combined[256] = "";
  for (int i = 0; i < rows_used; i++) {
    // Extract non-space content from each row
    char *row = sb->rows[i];
    for (int j = 0; j < 80; j++) {
      if (row[j] != ' ') {
        strncat(combined, &row[j], 1);
      }
    }
  }
  
  // Combined should roughly match original (minus spaces at breaks)
  test_assert(strlen(combined) > 50, "wrap: significant content rendered across rows");
  
  screen_free(sb);
  buffer_free(&buf);
}

/**
 * Test: Model unchanged during rendering
 */
void test_render_no_model_change(void) {
  fprintf(stderr, "\n=== Test: Render doesn't modify model ===\n");
  
  Buffer buf;
  buffer_init(&buf);
  EditorConfig config;
  load_editor_config(&config);
  
  const char *line1 = "First line";
  const char *line2 = "Second line that is quite long and should wrap if word wrapping is enabled for testing";
  buffer_insert_line(&buf, 0, line1);
  buffer_insert_line(&buf, 1, line2);
  
  int orig_lines = buffer_num_lines(&buf);
  int orig_len = strlen(buffer_get_line(&buf, 1));
  
  // Render with word_wrap OFF
  config.display.word_wrap = 0;
  ScreenBuffer *sb1 = screen_create(10, 80);
  shadow_render(sb1, &buf, 0, 0, 0, 0, 0, &config);
  
  test_assert(buffer_num_lines(&buf) == orig_lines, "render (wrap OFF): model unchanged");
  test_assert(strlen(buffer_get_line(&buf, 1)) == orig_len, "render (wrap OFF): line length unchanged");
  
  // Render with word_wrap ON
  config.display.word_wrap = 1;
  ScreenBuffer *sb2 = screen_create(10, 80);
  shadow_render(sb2, &buf, 0, 0, 0, 0, 0, &config);
  
  test_assert(buffer_num_lines(&buf) == orig_lines, "render (wrap ON): model unchanged");
  test_assert(strlen(buffer_get_line(&buf, 1)) == orig_len, "render (wrap ON): line length unchanged");
  
  screen_free(sb1);
  screen_free(sb2);
  buffer_free(&buf);
}

void run_view_tests(void) {
  fprintf(stderr, "\n====== TEST VIEW ======\n");
  
  test_truncate_no_wrap();
  test_wrap_enabled();
  test_render_no_model_change();
  
  fprintf(stderr, "====== VIEW TESTS COMPLETE ======\n");
}
