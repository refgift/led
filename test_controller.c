#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "model.h"
#include "controller.h"
#include "view.h"
#include "config.h"

void simulate_input (Buffer * buf, int *scroll_row, int *scroll_col,
                     int *cursor_line, int *cursor_col,
                     int *show_line_numbers, char *search_buffer,
                     int *search_mode, char **clipboard, const char *filename,
                     int *selection_start_line,
                     int *selection_start_col, int *selection_end_line,
                     int *selection_end_col, int *selection_active,
                     const char *inputs);

void test_buffer_manipulation ();
void test_buffer_manipulation ();
void test_buffer_advanced ();
void test_controller_simulation ();
void test_selection_clipboard ();
void test_undo_operations ();
void test_colorization ();
void test_nested_structures ();
void test_paired_keywords ();
void test_edge_cases ();
void test_config_themes ();
void test_performance_stress ();
void test_search_functionality ();
void test_buffer_replace_all ();

#define SEARCH_BUFFER_SIZE 256

static int tests_passed = 0;
static int tests_failed = 0;
static int test_number = 0;

void
test_assert (int condition, const char *msg)
{
  if (condition)
    {
      tests_passed++;
      fprintf (stderr, "PASS: %s\n", msg);
    }
  else
    {
      tests_failed++;
      fprintf (stderr, "FAIL: %s\n", msg);
    }
}

void
test_buffer_init_free ()
{
  Buffer buf;
  buffer_init (&buf);
  test_assert (buf.lines == NULL, "buffer_init sets lines to NULL");
  test_assert (buf.num_lines == 0, "buffer_init sets num_lines to 0");
  test_assert (buf.capacity == 0, "buffer_init sets capacity to 0");
  buffer_free (&buf);
  test_assert (buf.lines == NULL, "buffer_free sets lines to NULL");
}

void
test_buffer_load_save ()
{
  Buffer buf;
  buffer_init (&buf);
  // Load non-existent file (should fail gracefully)
  int load_result = buffer_load_from_file (&buf, "/nonexistent");
  test_assert (load_result == -1,
               "buffer_load_from_file returns -1 for non-existent file");
  test_assert (buffer_num_lines (&buf) == 0,
               "buffer remains empty after failed load");

  // Create a temp file
  char temp_file[] = "/tmp/led_test.txt";
  FILE *wfp = fopen (temp_file, "w");
  if (wfp)
    {
      fputs ("line1\nline2\n", wfp);
      fclose (wfp);
      // Load the temp file
      load_result = buffer_load_from_file (&buf, temp_file);
      test_assert (load_result == 0,
                   "buffer_load_from_file succeeds for valid file");
      test_assert (buffer_num_lines (&buf) == 2,
                   "buffer has correct number of lines after load");
      test_assert (strcmp (buffer_get_line (&buf, 0), "line1") == 0,
                   "first line loaded correctly");
      test_assert (strcmp (buffer_get_line (&buf, 1), "line2") == 0,
                   "second line loaded correctly");

      // Save to another temp file
      char save_file[] = "/tmp/led_save.txt";
      int save_result = buffer_save_to_file (&buf, save_file);
      test_assert (save_result == 0, "buffer_save_to_file succeeds");

      // Cleanup
      unlink (temp_file);
      unlink (save_file);
    }
  buffer_free (&buf);
}

void
test_buffer_manipulation ()
{
  Buffer buf;
  buffer_init (&buf);

  // Test insert_line
  buffer_insert_line (&buf, 0, "first");
  test_assert (buffer_num_lines (&buf) == 1, "insert_line adds line");
  test_assert (strcmp (buffer_get_line (&buf, 0), "first") == 0,
               "inserted line correct");

  // Test insert_char
  buffer_insert_char (&buf, 0, 5, '!');
  test_assert (strcmp (buffer_get_line (&buf, 0), "first!") == 0,
               "insert_char adds char");

  // Test delete_char
  buffer_delete_char (&buf, 0, 5);
  test_assert (strcmp (buffer_get_line (&buf, 0), "first") == 0,
               "delete_char removes char");

  // Test delete_line
  buffer_insert_line (&buf, 1, "second");
  test_assert (buffer_num_lines (&buf) == 2, "insert second line");
  buffer_delete_line (&buf, 0);
  test_assert (buffer_num_lines (&buf) == 1, "delete_line removes line");
  test_assert (strcmp (buffer_get_line (&buf, 0), "second") == 0,
               "remaining line correct");

  buffer_free (&buf);
}

void
test_search_functionality ()
{
  fprintf (stderr, "Running search functionality test\n");
  Buffer buf;
  buffer_init (&buf);
  buffer_insert_line (&buf, 0, "hello world");
  buffer_insert_line (&buf, 1, "goodbye world");

  int cursor_line = 0, cursor_col = 0;

  // Test search
  search_next (&buf, &cursor_line, &cursor_col, "world");
  test_assert (cursor_line == 0
               && cursor_col == 6, "search finds first match");

  // Search next
  search_next (&buf, &cursor_line, &cursor_col, "world");
  test_assert (cursor_line == 1
               && cursor_col == 8, "search finds second match");

  // Search not found
  cursor_line = 0;
  cursor_col = 0;
  search_next (&buf, &cursor_line, &cursor_col, "notfound");
  test_assert (cursor_line == 0
               && cursor_col == 0, "search no match stays put");

  // Test cursor beyond line end
  cursor_line = 0;
  cursor_col = 20;              // beyond len=11
  search_next (&buf, &cursor_line, &cursor_col, "world");
  // Should clamp cursor_col and search from end
  test_assert (cursor_line == 0
               && cursor_col == 11, "cursor clamped when beyond line end");

  buffer_free (&buf);
  fprintf (stderr, "Search functionality test completed\n");
}

void
test_buffer_replace_all ()
{
  fprintf (stderr, "Running buffer replace test\n");
  Buffer buf;
  buffer_init (&buf);
  buffer_insert_line (&buf, 0, "hello world");
  buffer_insert_line (&buf, 1, "hello again");

  // Test basic literal string replace
  buffer_replace_all (&buf, "hello", "hi");
  test_assert (strcmp (buffer_get_line (&buf, 0), "hi world") == 0,
               "replace_all changes first occurrence");
  test_assert (strcmp (buffer_get_line (&buf, 1), "hi again") == 0,
               "replace_all changes second occurrence");

  // Test multiple replacements in one line
  buffer_free (&buf);
  buffer_init (&buf);
  buffer_insert_line (&buf, 0, "foo foo foo");
  buffer_replace_all (&buf, "foo", "bar");
  test_assert (strcmp (buffer_get_line (&buf, 0), "bar bar bar") == 0,
               "replace_all handles multiple matches on same line");

  // Test empty replacement
  buffer_free (&buf);
  buffer_init (&buf);
  buffer_insert_line (&buf, 0, "abcdef");
  buffer_replace_all (&buf, "cd", "");
  test_assert (strcmp (buffer_get_line (&buf, 0), "abef") == 0,
               "empty replacement deletes matched text");

  // Test invalid regex (should not crash)
  buffer_replace_all (&buf, "[invalid(", "test");
  test_assert (1, "invalid regex does not crash");

  buffer_free (&buf);
  fprintf (stderr, "Buffer replace test completed\n");
}

void
run_all_tests ()
{
  fprintf (stderr, "Running led test suite...\n");
  test_number = 0;
  fprintf (stderr, "Test %d: buffer_init_free - Basic buffer lifecycle\n",
           ++test_number);
  test_buffer_init_free ();
  fprintf (stderr, "Test %d: buffer_load_save - File I/O operations\n",
           ++test_number);
  test_buffer_load_save ();
  fprintf (stderr,
           "Test %d: buffer_manipulation - Line and character operations\n",
           ++test_number);
  test_buffer_manipulation ();
  // fprintf(stderr, "Test %d: buffer_advanced - Text operations and ranges\n", ++test_number);
  // test_buffer_advanced();
  // fprintf(stderr, "Test %d: controller_simulation - Input handling\n", ++test_number);
  // test_controller_simulation();
  // fprintf(stderr, "Test %d: selection_clipboard - Selection and clipboard\n", ++test_number);
  // test_selection_clipboard();
  // fprintf(stderr, "Test %d: undo_operations - Undo/redo functionality\n", ++test_number);
  // test_undo_operations();
  // fprintf(stderr, "Test %d: colorization - Basic syntax highlighting\n", ++test_number);
  // test_colorization();
  // fprintf(stderr, "Test %d: nested_structures - Bracket nesting\n", ++test_number);
  // test_nested_structures();
  // fprintf(stderr, "Test %d: paired_keywords - Keyword pairing\n", ++test_number);
  // test_paired_keywords();
  // fprintf(stderr, "Test %d: edge_cases - Boundary conditions\n", ++test_number);
  // test_edge_cases();
  // fprintf(stderr, "Test %d: config_themes - Configuration handling\n", ++test_number);
  // test_config_themes();
  // fprintf(stderr, "Test %d: performance_stress - Stress testing\n", ++test_number);
  // test_performance_stress();
  fprintf(stderr, "Test %d: search_functionality - Search operations\n", ++test_number);
  test_search_functionality();
  fprintf (stderr, "Test %d: buffer_replace_all - Buffer replace operations\n",
           ++test_number);
  test_buffer_replace_all ();
  fprintf (stderr, "Tests completed: %d passed, %d failed\n", tests_passed,
           tests_failed);
}
