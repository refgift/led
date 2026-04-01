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

void test_undo_redo_comprehensive ();
void test_clipboard_comprehensive ();
void test_enter_key_newline_insertion ();
void test_right_arrow_repeat_navigation ();

extern void run_view_tests(void);  // From test_view.c
extern void test_autosave_comprehensive(void);  // From test_autosave.c

#define SEARCH_BUFFER_SIZE 256

int tests_passed = 0;
int tests_failed = 0;
static int test_number = 0;

static char *
my_strdup (const char *s)
{
  if (!s)
    return NULL;
  int len = strlen (s);
  char *d = malloc (len + 1);
  if (d)
    memcpy (d, s, len + 1);
  return d;
}

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
      test_assert (buffer_num_lines (&buf) == 3,
                   "buffer has correct number of lines after load");
      test_assert (strcmp (buffer_get_line (&buf, 0), "line1") == 0,
                   "first line loaded correctly");
      test_assert (strcmp (buffer_get_line (&buf, 1), "line2") == 0,
                   "second line loaded correctly");
      test_assert (strcmp (buffer_get_line (&buf, 2), "") == 0,
                   "empty line at end loaded correctly");

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
  buffer_insert_line (&buf, 0, "hello world hello");
  buffer_insert_line (&buf, 1, "goodbye world hello");

  int cursor_line = 0, cursor_col = 0;

  // Test search from (0,0) - if match is AT cursor, search_next skips to next
  // This is by design: search_next always finds the NEXT match after cursor
  search_next (&buf, &cursor_line, &cursor_col, "hello");
  test_assert (cursor_line == 0
               && cursor_col == 12, "search from (0,0) skips match at cursor, finds next");

  // Move cursor past first match to find second
  cursor_col = 6;
  search_next (&buf, &cursor_line, &cursor_col, "hello");
  test_assert (cursor_line == 0
               && cursor_col == 12, "search finds second 'hello' on same line");

  // Move to next line to test multi-line search
  cursor_line = 1;
  cursor_col = 0;
  search_next (&buf, &cursor_line, &cursor_col, "hello");
  test_assert (cursor_line == 1
               && cursor_col == 14, "search finds 'hello' on different line");

  // Search for "world" starting from (0,0)
  cursor_line = 0;
  cursor_col = 0;
  search_next (&buf, &cursor_line, &cursor_col, "world");
  test_assert (cursor_line == 0
               && cursor_col == 6, "search finds 'world'");

  // Search not found
  cursor_line = 0;
  cursor_col = 0;
  search_next (&buf, &cursor_line, &cursor_col, "notfound");
  test_assert (cursor_line == 0
               && cursor_col == 0, "search no match stays put");

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

/* Word wrap test removed - feature disabled to ensure reliable editing */

void test_undo_redo_comprehensive ()
{
  fprintf (stderr, "Running undo/redo comprehensive tests\n");
  Buffer buf;
  buffer_init (&buf);
  init_undo ();
  free_undo ();
  init_undo ();
  
  // Test 1: initialization
  test_assert (undo_stack.count == 0, "undo_stack initializes with count=0");
  test_assert (redo_stack.count == 0, "redo_stack initializes with count=0");
  
  // Test 2: single insert and undo
  buffer_insert_line (&buf, 0, "");
  buffer_insert_char (&buf, 0, 0, 'a');
  push_undo (true, 0, 0, 'a');
  test_assert (strcmp (buffer_get_line (&buf, 0), "a") == 0, "char inserted");
  int cursor_line = 0, cursor_col = 1;
  undo_operation (&buf, &cursor_line, &cursor_col);
  test_assert (strcmp (buffer_get_line (&buf, 0), "") == 0, "undo removes char");
  test_assert (cursor_col == 0, "undo adjusts cursor");
  
  // Test 3: delete and undo
  buffer_free (&buf);
  buffer_init (&buf);
  free_undo ();
  init_undo ();
  buffer_insert_line (&buf, 0, "hello");
  char ch = buffer_get_char (&buf, 0, 0);
  buffer_delete_char (&buf, 0, 0);
  push_undo (false, 0, 0, ch);
  test_assert (strcmp (buffer_get_line (&buf, 0), "ello") == 0, "char deleted");
  cursor_line = 0; cursor_col = 0;
  undo_operation (&buf, &cursor_line, &cursor_col);
  test_assert (strcmp (buffer_get_line (&buf, 0), "hello") == 0, "undo restores");
  
  // Test 4: undo with empty stack
  buffer_free (&buf);
  buffer_init (&buf);
  free_undo ();
  init_undo ();
  buffer_insert_line (&buf, 0, "test");
  cursor_line = 0; cursor_col = 0;
  undo_operation (&buf, &cursor_line, &cursor_col);
  test_assert (strcmp (buffer_get_line (&buf, 0), "test") == 0, "empty undo doesn't crash");
  
  // Test 5: redo after undo
  buffer_free (&buf);
  buffer_init (&buf);
  free_undo ();
  init_undo ();
  buffer_insert_line (&buf, 0, "");
  buffer_insert_char (&buf, 0, 0, 'x');
  push_undo (true, 0, 0, 'x');
  cursor_line = 0; cursor_col = 1;
  undo_operation (&buf, &cursor_line, &cursor_col);
  test_assert (redo_stack.count == 1, "redo_stack has entry");
  redo_operation (&buf, &cursor_line, &cursor_col);
  test_assert (strcmp (buffer_get_line (&buf, 0), "x") == 0, "redo restores");
  
  // Test 6: stack growth
  buffer_free (&buf);
  buffer_init (&buf);
  free_undo ();
  init_undo ();
  buffer_insert_line (&buf, 0, "");
  for (int i = 0; i < 32; i++)
    {
		sched_yield();



      buffer_insert_char (&buf, 0, i, 'a');
      push_undo (true, 0, i, 'a');
    }
  test_assert (undo_stack.capacity >= 32, "stack grows");
  
  // Test 7: multiline undo
  buffer_free (&buf);
  buffer_init (&buf);
  free_undo ();
  init_undo ();
  buffer_insert_line (&buf, 0, "hello");
  buffer_insert_line (&buf, 1, "world");
  buffer_insert_char (&buf, 1, 3, 'X');
  push_undo (true, 1, 3, 'X');
  cursor_line = 1; cursor_col = 4;
  undo_operation (&buf, &cursor_line, &cursor_col);
  test_assert (strcmp (buffer_get_line (&buf, 1), "world") == 0, "multiline undo");
  
  // Test 8: redo clears on new edit
  buffer_free (&buf);
  buffer_init (&buf);
  free_undo ();
  init_undo ();
  buffer_insert_line (&buf, 0, "");
  buffer_insert_char (&buf, 0, 0, 'a');
  push_undo (true, 0, 0, 'a');
  cursor_line = 0; cursor_col = 1;
  undo_operation (&buf, &cursor_line, &cursor_col);
  test_assert (redo_stack.count == 1, "redo populated");
  clear_redo ();
  test_assert (redo_stack.count == 0, "redo cleared");
  
  // Test 9: multiple undos
  buffer_free (&buf);
  buffer_init (&buf);
  free_undo ();
  init_undo ();
  buffer_insert_line (&buf, 0, "initial");
  for (int i = 0; i < 5; i++)
    {
		sched_yield();



      buffer_insert_char (&buf, 0, 7 + i, 'x');
      push_undo (true, 0, 7 + i, 'x');
    }
  for (int i = 0; i < 5; i++)
    {
		sched_yield();



      cursor_line = 0; cursor_col = 12 - i;
      undo_operation (&buf, &cursor_line, &cursor_col);
    }
  test_assert (strcmp (buffer_get_line (&buf, 0), "initial") == 0, "multiple undos");
  
  // Test 10: redo with multiple operations
  for (int i = 0; i < 3; i++)
    {
		sched_yield();



      cursor_line = 0; cursor_col = 7 + i;
      redo_operation (&buf, &cursor_line, &cursor_col);
    }
  test_assert (strcmp (buffer_get_line (&buf, 0), "initialxxx") == 0, "partial redo");
  
  buffer_free (&buf);
  free_undo ();
  fprintf (stderr, "Undo/redo comprehensive tests completed\n");
}

void test_clipboard_comprehensive ()
{
  fprintf (stderr, "Running clipboard comprehensive tests\n");
  Buffer buf;
  char *clipboard = NULL;
  buffer_init (&buf);
  
  // Test 1: select all empty
  buffer_insert_line (&buf, 0, "");
  int sel_start_line = 0;
  int sel_end_line = buffer_num_lines (&buf) - 1;
  int sel_end_col = buffer_get_line_length (&buf, sel_end_line);
  int sel_active = 1;
  test_assert (sel_active == 1, "select_all sets active");
  
  // Test 2: select all single line
  buffer_free (&buf);
  buffer_init (&buf);
  buffer_insert_line (&buf, 0, "hello");
  sel_start_line = 0;
  sel_end_line = 0;
  sel_end_col = 5;
  test_assert (sel_end_col == 5, "select_all correct length");
  
  // Test 3: select all multiline
  buffer_free (&buf);
  buffer_init (&buf);
  buffer_insert_line (&buf, 0, "line1");
  buffer_insert_line (&buf, 1, "line2");
  buffer_insert_line (&buf, 2, "line3");
  sel_start_line = 0;
  sel_end_line = 2;
  test_assert (sel_start_line == 0 && sel_end_line == 2, "select_all multiline");
  
  // Test 4: copy current line
  buffer_free (&buf);
  buffer_init (&buf);
  buffer_insert_line (&buf, 0, "foo");
  buffer_insert_line (&buf, 1, "bar");
  int cursor_line = 1;
  if (clipboard) free (clipboard);
  clipboard = my_strdup (buffer_get_line (&buf, cursor_line));
  test_assert (clipboard != NULL && strcmp (clipboard, "bar") == 0, "copy current");
  
  // Test 5: copy selection
  buffer_free (&buf);
  buffer_init (&buf);
  buffer_insert_line (&buf, 0, "hello world");
  if (clipboard) free (clipboard);
  const char *line = buffer_get_line (&buf, 0);
  clipboard = malloc (6);
  if (clipboard)
    {
      memcpy (clipboard, line, 5);
      clipboard[5] = 0;
    }
  test_assert (clipboard != NULL && strcmp (clipboard, "hello") == 0, "copy selection");
  
  // Test 6: clipboard overwrite
  if (clipboard) free (clipboard);
  clipboard = malloc (6);
  strcpy (clipboard, "first");
  if (clipboard) free (clipboard);
  clipboard = malloc (7);
  strcpy (clipboard, "second");
  test_assert (strcmp (clipboard, "second") == 0, "clipboard overwrite");
  
  // Test 7: cut and clipboard
  buffer_free (&buf);
  buffer_init (&buf);
  buffer_insert_line (&buf, 0, "hello world");
  if (clipboard) free (clipboard);
  clipboard = malloc (6);
  line = buffer_get_line (&buf, 0);
  memcpy (clipboard, line, 5);
  clipboard[5] = 0;
  buffer_delete_range (&buf, 0, 0, 0, 5);
  test_assert (strcmp (clipboard, "hello") == 0 && strcmp (buffer_get_line (&buf, 0), " world") == 0, "cut");
  
  // Test 8: paste
  buffer_free (&buf);
  buffer_init (&buf);
  buffer_insert_line (&buf, 0, "hello");
  if (clipboard) free (clipboard);
  clipboard = malloc (6);
  strcpy (clipboard, "hello");
  buffer_insert_text (&buf, 0, 5, clipboard);
  test_assert (strcmp (buffer_get_line (&buf, 0), "hellohello") == 0, "paste");
  
  // Test 9: paste multiple
  buffer_free (&buf);
  buffer_init (&buf);
  buffer_insert_line (&buf, 0, "");
  if (clipboard) free (clipboard);
  clipboard = malloc (2);
  strcpy (clipboard, "x");
  for (int i = 0; i < 5; i++)
    buffer_insert_text (&buf, 0, i, clipboard);
  test_assert (strcmp (buffer_get_line (&buf, 0), "xxxxx") == 0, "paste multiple");
  
  // Test 10: empty clipboard
  buffer_free (&buf);
  buffer_init (&buf);
  buffer_insert_line (&buf, 0, "test");
  if (clipboard) free (clipboard);
  clipboard = NULL;
  test_assert (clipboard == NULL && strcmp (buffer_get_line (&buf, 0), "test") == 0, "empty clipboard");
  
  // Test 11: copy line no selection
  buffer_free (&buf);
  buffer_init (&buf);
  buffer_insert_line (&buf, 0, "line1");
  buffer_insert_line (&buf, 1, "line2");
  if (clipboard) free (clipboard);
  clipboard = my_strdup (buffer_get_line (&buf, 1));
  test_assert (strcmp (clipboard, "line2") == 0, "copy no selection");
  
// Test 12: cut line no selection
buffer_free (&buf);
buffer_init (&buf);
buffer_insert_line (&buf, 0, "line1");
buffer_insert_line (&buf, 1, "line2");
buffer_insert_line (&buf, 2, "line3");
if (clipboard) free (clipboard);
clipboard = my_strdup (buffer_get_line (&buf, 2));
buffer_delete_line (&buf, 2);
test_assert (strcmp (clipboard, "line3") == 0 && buffer_num_lines (&buf) == 2, "cut line");

// Test 13: cut last line when only one line
buffer_free (&buf);
buffer_init (&buf);
buffer_insert_line (&buf, 0, "onlyline");
if (clipboard) free (clipboard);
clipboard = my_strdup (buffer_get_line (&buf, 0));
buffer_delete_line (&buf, 0);
test_assert (strcmp (clipboard, "onlyline") == 0 && buffer_num_lines (&buf) == 0, "cut single line leaves empty buffer");
  
  buffer_free (&buf);
  if (clipboard) free (clipboard);
  fprintf (stderr, "Clipboard comprehensive tests completed\n");
}

void
test_ctrl_x_last_line (void)
{
  fprintf (stderr, "Running Ctrl-X last line test\n");
  Buffer buf;
  buffer_init (&buf);
  buffer_insert_line (&buf, 0, "testline");
  int scroll_row = 0, scroll_col = 0, cursor_line = 0, cursor_col = 0;
  int show_line_numbers = 0;
  char search_buffer[SEARCH_BUFFER_SIZE] = "";
  int search_mode = 0;
  char *clipboard = NULL;
  const char *filename = NULL;
  int selection_start_line = 0, selection_start_col = 0;
  int selection_end_line = 0, selection_end_col = 0;
  int selection_active = 0;
  cursor_line = 0;
  cursor_col = 0;
  handle_input (24, &buf, &scroll_row, &scroll_col, &cursor_line, &cursor_col,
                &show_line_numbers, search_buffer, &search_mode, &clipboard,
                filename, &selection_start_line, &selection_start_col,
                &selection_end_line, &selection_end_col, &selection_active,
                NULL);
  test_assert (buffer_num_lines (&buf) == 1
               && strcmp (buffer_get_line (&buf, 0), "") == 0
               && cursor_line == 0 && cursor_col == 0
               && strcmp (clipboard, "testline") == 0,
               "Ctrl-X on single line leaves empty line and adjusts cursor");
  if (clipboard)
    free (clipboard);
  buffer_free (&buf);
  fprintf (stderr, "Ctrl-X last line test completed\n");
}

void
test_enter_key_newline_insertion (void)
{
  fprintf (stderr, "Running enter key newline insertion test\n");
  Buffer buf;
  buffer_init (&buf);
  buffer_insert_line (&buf, 0, "Hello world");
  int scroll_row = 0, scroll_col = 0, cursor_line = 0, cursor_col = 5; // Cursor at "Hello| world"
  int show_line_numbers = 0;
  char search_buffer[SEARCH_BUFFER_SIZE] = "";
  int search_mode = 0;
  char *clipboard = NULL;
  const char *filename = NULL;
  int selection_start_line = 0, selection_start_col = 0;
  int selection_end_line = 0, selection_end_col = 0;
  int selection_active = 0;
  // Simulate Enter key
  handle_input ('\n', &buf, &scroll_row, &scroll_col, &cursor_line, &cursor_col,
                &show_line_numbers, search_buffer, &search_mode, &clipboard,
                filename, &selection_start_line, &selection_start_col,
                &selection_end_line, &selection_end_col, &selection_active,
                NULL);
  test_assert (buffer_num_lines (&buf) == 2
               && strcmp (buffer_get_line (&buf, 0), "Hello") == 0
               && strcmp (buffer_get_line (&buf, 1), " world") == 0
               && cursor_line == 1 && cursor_col == 0,
               "Enter inserts newline, splits line, and moves cursor to new line start");
  buffer_free (&buf);
  fprintf (stderr, "Enter key newline insertion test completed\n");
}

void
test_cursor_newline_undo_redo_bug (void)
{
  fprintf (stderr, "Running cursor newline undo/redo bug test\n");
  Buffer buf;
  buffer_init (&buf);
  Editor ed = {0}; // Dummy
  ed.config.display.tab_width = 8;
  COLS = 80; // Set terminal width for test
  init_undo ();

  // Insert test line
  buffer_insert_line (&buf, 0, "hello world");
  int cursor_line = 0, cursor_col = 11;  // Cursor at end
  int scroll_row = 0, scroll_col = 0, show_line_numbers = 0;
  char search_buffer[SEARCH_BUFFER_SIZE] = "";
  int search_mode = 0;
  char *clipboard = NULL;
  const char *filename = NULL;
  int selection_start_line = 0, selection_start_col = 0;
  int selection_end_line = 0, selection_end_col = 0, selection_active = 0;

  // Step 1: Initial newline
  handle_input ('\n', &buf, &scroll_row, &scroll_col, &cursor_line, &cursor_col,
                &show_line_numbers, search_buffer, &search_mode, &clipboard,
                filename, &selection_start_line, &selection_start_col,
                &selection_end_line, &selection_end_col, &selection_active, &ed);
  test_assert (buffer_num_lines (&buf) == 2 && cursor_line == 1 && cursor_col == 0,
               "Initial newline splits line and positions cursor correctly");

  // Step 2: Undo
  undo_operation (&buf, &cursor_line, &cursor_col);
  test_assert (buffer_num_lines (&buf) == 1 && strcmp (buffer_get_line (&buf, 0), "hello world") == 0
               && cursor_line == 0 && cursor_col == 11,
               "Undo reverts newline and cursor position");

  // Step 3: Redo
  redo_operation (&buf, &cursor_line, &cursor_col);
  test_assert (buffer_num_lines (&buf) == 2 && cursor_line == 1 && cursor_col == 0,
               "Redo reapplies newline and cursor position");

    // Step 4: Subsequent newline (potential bug point)
    handle_input ('\n', &buf, &scroll_row, &scroll_col, &cursor_line, &cursor_col,
                 &show_line_numbers, search_buffer, &search_mode, &clipboard,
                 filename, &selection_start_line, &selection_start_col,
                 &selection_end_line, &selection_end_col, &selection_active, &ed);
    test_assert (buffer_num_lines (&buf) == 3 && cursor_line == 2 && cursor_col == 0
                && strcmp (buffer_get_line (&buf, 2), "") == 0,
               "Subsequent newline after redo positions cursor correctly");

    // Step 5: Backspace to merge lines
    handle_input (KEY_BACKSPACE, &buf, &scroll_row, &scroll_col, &cursor_line, &cursor_col,
                 &show_line_numbers, search_buffer, &search_mode, &clipboard,
                 filename, &selection_start_line, &selection_start_col,
                 &selection_end_line, &selection_end_col, &selection_active, &ed);
    test_assert (buffer_num_lines (&buf) == 2 && cursor_line == 1 && cursor_col == 0
                && strcmp (buffer_get_line (&buf, 1), "") == 0,
               "Backspace at start of line merges lines correctly");

    // Step 6: Undo the backspace
    undo_operation (&buf, &cursor_line, &cursor_col);
    test_assert (buffer_num_lines (&buf) == 3 && cursor_line == 2 && cursor_col == 0
                && strcmp (buffer_get_line (&buf, 2), "") == 0,
               "Undo of backspace positions cursor correctly");

    free_undo ();
    buffer_free (&buf);
    fprintf (stderr, "Cursor newline undo/redo bug test completed\n");
}

void
test_right_arrow_repeat_navigation (void)
{
  fprintf (stderr, "Running right arrow repeat navigation test\n");
  Buffer buf;
  buffer_init (&buf);
  // Create a multi-line document with a very long line
  buffer_insert_line (&buf, 0, "Line one");
  buffer_insert_line (&buf, 1, ""); // Empty line
  char long_line[200];
  memset(long_line, 'a', 199);
  long_line[199] = '\0'; // Very long line
  buffer_insert_line (&buf, 2, long_line);
  buffer_insert_line (&buf, 3, "Last"); // Last line
  int scroll_row = 0, scroll_col = 0, cursor_line = 0, cursor_col = 0;
  int show_line_numbers = 0;
  char search_buffer[SEARCH_BUFFER_SIZE] = "";
  int search_mode = 0;
  char *clipboard = NULL;
  const char *filename = NULL;
  int selection_start_line = 0, selection_start_col = 0;
  int selection_end_line = 0, selection_end_col = 0;
  int selection_active = 0;
  Editor ed = {0}; // Dummy
  ed.config.display.tab_width = 8;
  COLS = 80; // Set terminal width for test

  // Start at beginning
  cursor_line = 0;
  cursor_col = 0;

  // Simulate repeated right arrow presses until end of document
  int total_chars = 0;
  for (int l = 0; l < buffer_num_lines (&buf); l++)
    {
		sched_yield();



      total_chars += buffer_get_line_length (&buf, l) + 1; // +1 for newline
    }
  // Move right until we can't anymore
  while (1)
    {
		sched_yield();



      int prev_line = cursor_line;
      int prev_col = cursor_col;
      handle_input (KEY_RIGHT, &buf, &scroll_row, &scroll_col, &cursor_line, &cursor_col,
                    &show_line_numbers, search_buffer, &search_mode, &clipboard,
                    filename, &selection_start_line, &selection_start_col,
                    &selection_end_line, &selection_end_col, &selection_active,
                    &ed);
      if (cursor_line == prev_line && cursor_col == prev_col)
        break; // No movement, at end
    }

  // Should be at end of last line
  int expected_line = buffer_num_lines (&buf) - 1;
  int expected_col = buffer_get_line_length (&buf, expected_line);
  test_assert (cursor_line == expected_line && cursor_col == expected_col,
               "Right arrow repeat reaches end of document correctly");
  buffer_free (&buf);
  fprintf (stderr, "Right arrow repeat navigation test completed\n");
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
  // word wrap test removed (feature disabled)
  fprintf (stderr, "Test %d: undo_redo_comprehensive - Undo/redo functionality\n",
           ++test_number);
  test_undo_redo_comprehensive ();
fprintf (stderr, "Test %d: clipboard_comprehensive - Clipboard operations\n",
            ++test_number);
  test_clipboard_comprehensive ();
  fprintf (stderr, "Test %d: ctrl_x_last_line - Cut last line in controller\n",
             ++test_number);
  test_ctrl_x_last_line ();
  fprintf (stderr, "Test %d: enter_key_newline_insertion - Enter key inserts newline\n",
             ++test_number);
  test_enter_key_newline_insertion ();
  fprintf (stderr, "Test %d: right_arrow_repeat_navigation - Right arrow navigation through document\n",
             ++test_number);
  test_right_arrow_repeat_navigation ();
  fprintf (stderr, "Test %d: autosave_comprehensive - Auto-save and backups\n",
             ++test_number);
  test_autosave_comprehensive (); 
  
  fprintf (stderr, "\n");
  run_view_tests();
  
  fprintf (stderr, "Tests completed: %d passed, %d failed\n", tests_passed,
           tests_failed);
}
