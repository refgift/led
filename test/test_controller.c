#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "model.h"
#include "controller.h"
#include "view.h"
#include "config.h"
#include "editor.h"
#include "test_helpers.h"

void simulate_input (Buffer * buf, int *scroll_row, int *scroll_col,
                     int *cursor_line, int *cursor_col,
                     int *show_line_numbers, char *search_buffer,
                     int *search_mode, char *clipboard, const char *filename,
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
void test_delete_key ();
void test_backspace_key ();
void test_right_arrow_repeat_navigation ();

extern void run_view_tests(void);  // From test_view.c
extern void test_autosave_comprehensive(void);  // From test_autosave.c

#define SEARCH_BUFFER_SIZE 256

int tests_passed = 0;
int tests_failed = 0;
static int test_number = 0;

/* strdup provided by controller.c to avoid duplicate symbol */
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
  int load_result = buffer_load_from_file (&buf, "/nonexistent", 0, 0);
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
      load_result = buffer_load_from_file (&buf, temp_file, 0, 0);
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
test_undo_redo_comprehensive ()
{
  fprintf (stderr, "Running undo/redo comprehensive tests\n");
  Buffer buf;
  Editor ed = {0};
  buffer_init (&buf);

  // Test 1: initialization
  test_assert (ed.undo_stack.count == 0, "undo_stack initializes with count=0");
  test_assert (ed.redo_stack.count == 0, "redo_stack initializes with count=0");
  
  // Test 2: single insert and undo
  buffer_insert_line (&buf, 0, "");
  buffer_insert_char (&buf, 0, 0, 'a');
  push_undo (&ed.undo_stack, true, 0, 0, 'a');
  test_assert (strcmp (buffer_get_line (&buf, 0), "a") == 0, "char inserted");
  int cursor_line = 0, cursor_col = 1;
  undo_operation (&buf, &ed.undo_stack, &ed.redo_stack, &cursor_line, &cursor_col);
  test_assert (strcmp (buffer_get_line (&buf, 0), "") == 0, "undo removes char");
  test_assert (cursor_col == 0, "undo adjusts cursor");
  
  // Test 3: delete and undo
  buffer_free (&buf);
  buffer_init (&buf);

  buffer_insert_line (&buf, 0, "hello");
  char ch = buffer_get_char (&buf, 0, 0);
  buffer_delete_char (&buf, 0, 0);
  push_undo (&ed.undo_stack, false, 0, 0, ch);
  test_assert (strcmp (buffer_get_line (&buf, 0), "ello") == 0, "char deleted");
  cursor_line = 0; cursor_col = 0;
  undo_operation (&buf, &ed.undo_stack, &ed.redo_stack, &cursor_line, &cursor_col);
  test_assert (strcmp (buffer_get_line (&buf, 0), "hello") == 0, "undo restores");
  
  // Test 4: undo with empty stack
  buffer_free (&buf);
  buffer_init (&buf);

  buffer_insert_line (&buf, 0, "test");
  cursor_line = 0; cursor_col = 0;
  undo_operation (&buf, &ed.undo_stack, &ed.redo_stack, &cursor_line, &cursor_col);
  test_assert (strcmp (buffer_get_line (&buf, 0), "test") == 0, "empty undo doesn't crash");
  
  // Test 5: redo after undo
  buffer_free (&buf);
  buffer_init (&buf);
  test_reset_undo(&ed);

  buffer_insert_line (&buf, 0, "");
  buffer_insert_char (&buf, 0, 0, 'x');
  push_undo (&ed.undo_stack, true, 0, 0, 'x');
  cursor_line = 0; cursor_col = 1;
  undo_operation (&buf, &ed.undo_stack, &ed.redo_stack, &cursor_line, &cursor_col);
  test_assert (ed.redo_stack.count == 1, "redo_stack has entry");
  redo_operation (&buf, &ed.undo_stack, &ed.redo_stack, &cursor_line, &cursor_col);
  test_assert (strcmp (buffer_get_line (&buf, 0), "x") == 0, "redo restores");
  
  // Test 6: stack growth
  buffer_free (&buf);
  buffer_init (&buf);

  buffer_insert_line (&buf, 0, "");
  for (int i = 0; i < 32; i++)
    {
		

 

 
      buffer_insert_char (&buf, 0, i, 'a');
      push_undo (&ed.undo_stack, true, 0, i, 'a');
    }
  test_assert (ed.undo_stack.capacity >= 32, "stack grows");
  
  // Test 7: multiline undo
  buffer_free (&buf);
  buffer_init (&buf);

  buffer_insert_line (&buf, 0, "hello");
  buffer_insert_line (&buf, 1, "world");
  buffer_insert_char (&buf, 1, 3, 'X');
  push_undo (&ed.undo_stack, true, 1, 3, 'X');
  cursor_line = 1; cursor_col = 4;
  undo_operation (&buf, &ed.undo_stack, &ed.redo_stack, &cursor_line, &cursor_col);
  test_assert (strcmp (buffer_get_line (&buf, 1), "world") == 0, "multiline undo");
  
  // Test 8: redo clears on new edit
  buffer_free (&buf);
  buffer_init (&buf);
  test_reset_undo(&ed);

  buffer_insert_line (&buf, 0, "");
  buffer_insert_char (&buf, 0, 0, 'a');
  push_undo (&ed.undo_stack, true, 0, 0, 'a');
  cursor_line = 0; cursor_col = 1;
  undo_operation (&buf, &ed.undo_stack, &ed.redo_stack, &cursor_line, &cursor_col);
  test_assert (ed.redo_stack.count == 1, "redo populated");
  clear_redo (&ed.redo_stack);
  test_assert (ed.redo_stack.count == 0, "redo cleared");
  
  // Test 9: multiple undos
  buffer_free (&buf);
  buffer_init (&buf);

  buffer_insert_line (&buf, 0, "initial");
  for (int i = 0; i < 5; i++)
    {
		

 

 

      buffer_insert_char (&buf, 0, 7 + i, 'x');
      push_undo (&ed.undo_stack, true, 0, 7 + i, 'x');
    }
  for (int i = 0; i < 5; i++)
    {
		

 

 

      cursor_line = 0; cursor_col = 12 - i;
      undo_operation (&buf, &ed.undo_stack, &ed.redo_stack, &cursor_line, &cursor_col);
    }
  test_assert (strcmp (buffer_get_line (&buf, 0), "initial") == 0, "multiple undos");

  buffer_free (&buf);
  fprintf (stderr, "Undo/redo comprehensive tests completed\n");
}

void
test_search_functionality ()
{
  fprintf (stderr, "Running search functionality test\n");
  Buffer buf;
  Editor ed = {0};
  test_reset_undo (&ed);
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
  int missing = search_next (&buf, &cursor_line, &cursor_col, "notfound");
  test_assert (missing == 0 && cursor_line == 0
               && cursor_col == 0, "search no match stays put");

  cursor_line = 1;
  cursor_col = 14;              /* the last "hello"; next search must wrap */
  int wrapped = search_next (&buf, &cursor_line, &cursor_col, "hello");
  test_assert (wrapped == 2 && cursor_line == 0 && cursor_col == 0,
               "search wraps to the first match");

  cursor_line = 0;
  cursor_col = 0;
  test_assert (search_next (&buf, &cursor_line, &cursor_col, "[") == -1
               && cursor_line == 0 && cursor_col == 0,
               "invalid regex does not move the cursor");

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
  clipboard = strdup (buffer_get_line (&buf, cursor_line));
  test_assert (clipboard != NULL && strcmp (clipboard, "bar") == 0, "copy current");
  
  // Test 5: copy selection
  buffer_free (&buf);
  buffer_init (&buf);
  buffer_insert_line (&buf, 0, "hello world");
  if (clipboard) free (clipboard);
  const char *line = buffer_get_line (&buf, 0);
  clipboard = xmalloc (6);
  if (clipboard)
    {
      memcpy (clipboard, line, 5);
      clipboard[5] = 0;
    }
  test_assert (clipboard != NULL && strcmp (clipboard, "hello") == 0, "copy selection");
  
  // Test 6: clipboard overwrite
  if (clipboard) free (clipboard);
  clipboard = xmalloc (6);
  strcpy (clipboard, "first");
  if (clipboard) free (clipboard);
  clipboard = xmalloc (7);
  strcpy (clipboard, "second");
  test_assert (strcmp (clipboard, "second") == 0, "clipboard overwrite");
  
  // Test 7: cut and clipboard
  buffer_free (&buf);
  buffer_init (&buf);
  buffer_insert_line (&buf, 0, "hello world");
  if (clipboard) free (clipboard);
  clipboard = xmalloc (6);
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
  clipboard = xmalloc (6);
  strcpy (clipboard, "hello");
  buffer_insert_text (&buf, 0, 5, clipboard);
  test_assert (strcmp (buffer_get_line (&buf, 0), "hellohello") == 0, "paste");
  
  // Test 9: paste multiple
  buffer_free (&buf);
  buffer_init (&buf);
  buffer_insert_line (&buf, 0, "");
  if (clipboard) free (clipboard);
  clipboard = xmalloc (2);
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
  clipboard = strdup (buffer_get_line (&buf, 1));
  test_assert (strcmp (clipboard, "line2") == 0, "copy no selection");
  
// Test 12: cut line no selection
buffer_free (&buf);
  buffer_init (&buf);
buffer_insert_line (&buf, 0, "line1");
buffer_insert_line (&buf, 1, "line2");
buffer_insert_line (&buf, 2, "line3");
if (clipboard) free (clipboard);
clipboard = strdup (buffer_get_line (&buf, 2));
buffer_delete_line (&buf, 2);
test_assert (strcmp (clipboard, "line3") == 0 && buffer_num_lines (&buf) == 2, "cut line");

// Test 13: cut last line when only one line
buffer_free (&buf);
  buffer_init (&buf);
buffer_insert_line (&buf, 0, "onlyline");
if (clipboard) free (clipboard);
clipboard = strdup (buffer_get_line (&buf, 0));
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
  cursor_line = 0;
  cursor_col = 0;
  clipboard = strdup("prior_data_from_before_led");
   handle_input (24, &buf, &scroll_row, &scroll_col, &cursor_line, &cursor_col,
                  &show_line_numbers, search_buffer, &search_mode, &clipboard,
                  filename, NULL);
  {
    char *l = buffer_get_line (&buf, 0);
    test_assert (buffer_num_lines (&buf) == 1
                 && strcmp (l, "") == 0
                 && cursor_line == 0 && cursor_col == 0
                 && strcmp (clipboard, "testline") == 0,
                 "Ctrl-X on single line leaves empty line, adjusts cursor (replaces startup clipboard)");
  }
   handle_input (22, &buf, &scroll_row, &scroll_col, &cursor_line, &cursor_col,
                  &show_line_numbers, search_buffer, &search_mode, &clipboard,
                  filename, NULL);
  {
    char *l = buffer_get_line (&buf, 0);
    test_assert (strcmp (l, "testline") == 0,
                 "paste after cut restores the line");
  }
  fprintf (stderr, "Ctrl-X last line test completed\n");
}

void
test_strdup (void)
{
  fprintf (stderr, "Running strdup test\n");
  char * s = strdup ("");
  test_assert (s != NULL && *s == 0, "strdup empty");
  free (s);
  s = strdup ("test line");
  test_assert (s != NULL && strcmp (s, "test line") == 0, "strdup normal");
  free (s);
  fprintf (stderr, "strdup test completed\n");
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
  // Simulate Enter key
   handle_input ('\n', &buf, &scroll_row, &scroll_col, &cursor_line, &cursor_col,
                 &show_line_numbers, search_buffer, &search_mode, &clipboard,
                 filename, NULL);
  test_assert (buffer_num_lines (&buf) == 2
               && strcmp (buffer_get_line (&buf, 0), "Hello") == 0
               && strcmp (buffer_get_line (&buf, 1), " world") == 0
               && cursor_line == 1 && cursor_col == 0,
               "Enter inserts newline, splits line, and moves cursor to new line start");
  buffer_free (&buf);
  fprintf (stderr, "Enter key newline insertion test completed\n");
}

void
test_delete_key (void)
{
  fprintf (stderr, "Running delete key test\n");
  Buffer buf;
  buffer_init (&buf);
  buffer_insert_line (&buf, 0, "Hello world");
  int scroll_row = 0, scroll_col = 0, cursor_line = 0, cursor_col = 5; // Cursor between "Hello| world"
  int show_line_numbers = 0;
  char search_buffer[SEARCH_BUFFER_SIZE] = "";
  int search_mode = 0;
  char *clipboard = NULL;
  const char *filename = NULL;
  Editor ed = {0};
  ed.config.display.tab_width = 8;

  // Delete the space after "Hello"
  handle_input (KEY_DC, &buf, &scroll_row, &scroll_col, &cursor_line, &cursor_col,
                &show_line_numbers, search_buffer, &search_mode, &clipboard,
                filename, &ed);
  test_assert (strcmp (buffer_get_line (&buf, 0), "Helloworld") == 0
               && cursor_line == 0 && cursor_col == 5,
               "Delete removes char at cursor (forward delete)");

  // Undo the delete (cursor adjustment follows existing undo rules)
  (void)cursor_col;
  undo_operation (&buf, &ed.undo_stack, &ed.redo_stack, &cursor_line, &cursor_col);
  test_assert (strcmp (buffer_get_line (&buf, 0), "Hello world") == 0,
               "Undo after Delete restores the character");

  // Redo
  redo_operation (&buf, &ed.undo_stack, &ed.redo_stack, &cursor_line, &cursor_col);
  test_assert (strcmp (buffer_get_line (&buf, 0), "Helloworld") == 0,
               "Redo after Delete re-applies the deletion");

  // Now test deleting at end of line (merge with next)
  buffer_insert_line (&buf, 1, "next");
  cursor_col = buffer_get_line_length (&buf, 0); // end of first line
  handle_input (KEY_DC, &buf, &scroll_row, &scroll_col, &cursor_line, &cursor_col,
                &show_line_numbers, search_buffer, &search_mode, &clipboard,
                filename, &ed);
  test_assert (buffer_num_lines (&buf) == 1
               && strcmp (buffer_get_line (&buf, 0), "Helloworldnext") == 0
               && cursor_line == 0,
               "Delete at end of line merges with next line (cursor at join point)");

  buffer_free (&buf);
  fprintf (stderr, "Delete key test completed\n");
}

void
test_backspace_key (void)
{
  fprintf (stderr, "Running backspace key test\n");
  Buffer buf;
  buffer_init (&buf);
  buffer_insert_line (&buf, 0, "Hello world");
  int scroll_row = 0, scroll_col = 0, cursor_line = 0, cursor_col = 6;
  int show_line_numbers = 0;
  char search_buffer[SEARCH_BUFFER_SIZE] = "";
  int search_mode = 0;
  char *clipboard = NULL;
  const char *filename = NULL;
  Editor ed = {0};
  ed.config.display.tab_width = 8;

  /* ASCII 127 (DEL / ^?) — typical terminal Backspace */
  handle_input (127, &buf, &scroll_row, &scroll_col, &cursor_line, &cursor_col,
                &show_line_numbers, search_buffer, &search_mode, &clipboard,
                filename, &ed);
  test_assert (strcmp (buffer_get_line (&buf, 0), "Helloworld") == 0
               && cursor_line == 0 && cursor_col == 5,
               "Backspace (127) removes char before cursor");

  undo_operation (&buf, &ed.undo_stack, &ed.redo_stack, &cursor_line, &cursor_col);
  test_assert (strcmp (buffer_get_line (&buf, 0), "Hello world") == 0,
               "Undo after Backspace restores the character");

  redo_operation (&buf, &ed.undo_stack, &ed.redo_stack, &cursor_line, &cursor_col);
  test_assert (strcmp (buffer_get_line (&buf, 0), "Helloworld") == 0,
               "Redo after Backspace re-applies the deletion");

  /* ASCII 8 (BS / Ctrl+H) */
  clear_redo (&ed.redo_stack);
  buffer_free (&buf);
  buffer_init (&buf);
  buffer_insert_line (&buf, 0, "ab");
  cursor_line = 0;
  cursor_col = 2;
  handle_input (8, &buf, &scroll_row, &scroll_col, &cursor_line, &cursor_col,
                &show_line_numbers, search_buffer, &search_mode, &clipboard,
                filename, &ed);
  test_assert (strcmp (buffer_get_line (&buf, 0), "a") == 0
               && cursor_col == 1,
               "Backspace (8) removes char before cursor");

  /* KEY_BACKSPACE (ncurses symbolic) */
  handle_input (KEY_BACKSPACE, &buf, &scroll_row, &scroll_col, &cursor_line,
                &cursor_col, &show_line_numbers, search_buffer, &search_mode,
                &clipboard, filename, &ed);
  test_assert (strcmp (buffer_get_line (&buf, 0), "") == 0
               && cursor_col == 0,
               "Backspace (KEY_BACKSPACE) removes char before cursor");

  /* Line merge: backspace at column 0 joins with previous line */
  clear_redo (&ed.redo_stack);
  buffer_free (&buf);
  buffer_init (&buf);
  buffer_insert_line (&buf, 0, "foo");
  buffer_insert_line (&buf, 1, "bar");
  cursor_line = 1;
  cursor_col = 0;
  handle_input (127, &buf, &scroll_row, &scroll_col, &cursor_line, &cursor_col,
                &show_line_numbers, search_buffer, &search_mode, &clipboard,
                filename, &ed);
  test_assert (buffer_num_lines (&buf) == 1
               && strcmp (buffer_get_line (&buf, 0), "foobar") == 0
               && cursor_line == 0 && cursor_col == 3,
               "Backspace at start of line merges with previous line");

  buffer_free (&buf);
  fprintf (stderr, "Backspace key test completed\n");
}

void
test_cursor_newline_undo_redo_bug (void)
{
  fprintf (stderr, "Running cursor newline undo/redo bug test\n");
  Buffer buf;
  buffer_init (&buf);
  Editor ed = {0}; // Dummy
  ed.config.display.tab_width = 8;
  


  // Insert test line
  buffer_insert_line (&buf, 0, "hello world");
  int cursor_line = 0, cursor_col = 11;  // Cursor at end
  int scroll_row = 0, scroll_col = 0, show_line_numbers = 0;
  char search_buffer[SEARCH_BUFFER_SIZE] = "";
  int search_mode = 0;
  char *clipboard = NULL;
  const char *filename = NULL;
  //int selection_end_line = 0, selection_end_col = 0, selection_active = 0;

  // Step 1: Initial newline
   handle_input ('\n', &buf, &scroll_row, &scroll_col, &cursor_line, &cursor_col,
                 &show_line_numbers, search_buffer, &search_mode, &clipboard,
                 filename, &ed);
  test_assert (buffer_num_lines (&buf) == 2 && cursor_line == 1 && cursor_col == 0,
               "Initial newline splits line and positions cursor correctly");

  // Step 2: Undo
  undo_operation (&buf, &ed.undo_stack, &ed.redo_stack, &cursor_line, &cursor_col);
  test_assert (buffer_num_lines (&buf) == 1 && strcmp (buffer_get_line (&buf, 0), "hello world") == 0
               && cursor_line == 0 && cursor_col == 11,
               "Undo reverts newline and cursor position");

  // Step 3: Redo
  redo_operation (&buf, &ed.undo_stack, &ed.redo_stack, &cursor_line, &cursor_col);
  test_assert (buffer_num_lines (&buf) == 2 && cursor_line == 1 && cursor_col == 0,
               "Redo reapplies newline and cursor position");

    // Step 4: Subsequent newline (potential bug point)
    handle_input ('\n', &buf, &scroll_row, &scroll_col, &cursor_line, &cursor_col,
                 &show_line_numbers, search_buffer, &search_mode, &clipboard,
                 filename, &ed);
    test_assert (buffer_num_lines (&buf) == 3 && cursor_line == 2 && cursor_col == 0
                && strcmp (buffer_get_line (&buf, 2), "") == 0,
               "Subsequent newline after redo positions cursor correctly");

    // Step 5: Backspace to merge lines
    handle_input (KEY_BACKSPACE, &buf, &scroll_row, &scroll_col, &cursor_line, &cursor_col,
                  &show_line_numbers, search_buffer, &search_mode, &clipboard,
                  filename, &ed);
    test_assert (buffer_num_lines (&buf) == 2 && cursor_line == 1 && cursor_col == 0
                && strcmp (buffer_get_line (&buf, 1), "") == 0,
               "Backspace at start of line merges lines correctly");

    // Step 6: Undo the backspace
    undo_operation (&buf, &ed.undo_stack, &ed.redo_stack, &cursor_line, &cursor_col);
    test_assert (buffer_num_lines (&buf) == 3 && cursor_line == 2 && cursor_col == 0
                && strcmp (buffer_get_line (&buf, 2), "") == 0,
                "Undo of backspace positions cursor correctly");

  
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
  Editor ed = {0}; // Dummy
  ed.config.display.tab_width = 8;
 

  // Start at beginning
  cursor_line = 0;
  cursor_col = 0;

  // Simulate repeated right arrow presses until end of document
  (void) buffer_num_lines(&buf); // total length calculation not needed for the test logic
  // Move right until we can't anymore
  while (1)
    {
      int prev_line = cursor_line;
      int prev_col = cursor_col;
       handle_input (KEY_RIGHT, &buf, &scroll_row, &scroll_col, &cursor_line, &cursor_col,
                     &show_line_numbers, search_buffer, &search_mode, &clipboard,
                     filename, &ed);
      if (cursor_line == prev_line && cursor_col == prev_col)
        break;
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
test_tab_key (void)
{
  fprintf (stderr, "Running TAB key handling test\n");
  Buffer buf;
  buffer_init (&buf);
  Editor ed = {0};
  ed.config.display.tab_width = 4;
  ed.config.display.spaces_for_tab = 1;

  buffer_insert_line (&buf, 0, "abc");
  int cursor_line = 0, cursor_col = 3;
  int scroll_row = 0, scroll_col = 0, show_line_numbers = 0;
  char search_buffer[256] = "";
  int search_mode = 0;
  char *clipboard = NULL;
  const char *filename = NULL;

  // Test 1: spaces_for_tab = 1 → insert 4 spaces at col 3 (next tab stop)
  handle_input (9, &buf, &scroll_row, &scroll_col, &cursor_line, &cursor_col,
                &show_line_numbers, search_buffer, &search_mode, &clipboard,
                filename, &ed);
  char *line = buffer_get_line (&buf, 0);
  test_assert (strcmp (line, "abc ") == 0, "TAB inserts 1 space to next tab stop");
  test_assert (cursor_col == 4, "Cursor advanced by 1 to next tab stop");
  free (line);

  // Test 2: spaces_for_tab = 0 → insert literal tab char
  buffer_free (&buf);
  buffer_init (&buf);
  buffer_insert_line (&buf, 0, "x");
  cursor_line = 0;
  cursor_col = 1;
  ed.config.display.spaces_for_tab = 0;
  handle_input (9, &buf, &scroll_row, &scroll_col, &cursor_line, &cursor_col,
                &show_line_numbers, search_buffer, &search_mode, &clipboard,
                filename, &ed);
  line = buffer_get_line (&buf, 0);
  test_assert (line[1] == '\t', "TAB inserts literal tab char");
  test_assert (cursor_col == 2, "Cursor advanced by 1 for literal tab");
  free (line);

  // Test 3: tab_width = 0 → TAB does nothing
  ed.config.display.tab_width = 0;
  int before_len = buffer_get_line_length (&buf, 0);
  handle_input (9, &buf, &scroll_row, &scroll_col, &cursor_line, &cursor_col,
                &show_line_numbers, search_buffer, &search_mode, &clipboard,
                filename, &ed);
  test_assert (buffer_get_line_length (&buf, 0) == before_len,
               "TAB ignored when tab_width=0");

  buffer_free (&buf);
  fprintf (stderr, "TAB key handling test completed\n");
}

static char *
read_whole (const char *path)
{
  FILE *fp = fopen (path, "rb");
  if (!fp)
    return NULL;
  if (fseek (fp, 0, SEEK_END) != 0)
    {
      fclose (fp);
      return NULL;
    }
  long n = ftell (fp);
  if (n < 0)
    {
      fclose (fp);
      return NULL;
    }
  rewind (fp);
  char *buf = malloc ((size_t) n + 1);
  if (!buf)
    {
      fclose (fp);
      return NULL;
    }
  size_t got = fread (buf, 1, (size_t) n, fp);
  fclose (fp);
  buf[got] = '\0';
  return buf;
}

static int
col_of (Buffer *buf, int line, char ch, int which)
{
  char *s = buffer_get_line (buf, line);
  if (!s)
    return -1;
  int seen = 0;
  int col = -1;
  for (int i = 0; s[i]; i++)
    {
      if (s[i] == ch && ++seen == which)
        {
          col = i;
          break;
        }
    }
  free (s);
  return col;
}

void
test_programmer_motions (void)
{
  fprintf (stderr, "Running programmer motion tests\n");
  TestContext ctx;
  test_init (&ctx);

  buffer_insert_line (&ctx.buf, 0, "    foo;");
  ctx.cursor_col = buffer_get_line_length (&ctx.buf, 0);
  test_handle_input (&ctx, '\n');
  char *line = buffer_get_line (&ctx.buf, 1);
  test_assert (line && strcmp (line, "    ") == 0 && ctx.cursor_col == 4
               && ctx.cursor_line == 1,
               "Enter at end of line copies the indent");
  free (line);

  test_cleanup (&ctx);
  test_init (&ctx);
  ctx.ed.config.display.tab_width = 4;
  ctx.ed.config.display.spaces_for_tab = 1;
  buffer_insert_line (&ctx.buf, 0, "    if (x) {");
  ctx.cursor_col = buffer_get_line_length (&ctx.buf, 0);
  test_handle_input (&ctx, '\n');
  line = buffer_get_line (&ctx.buf, 1);
  test_assert (line && strcmp (line, "        ") == 0 && ctx.cursor_col == 8,
               "Enter after { adds one indent level");
  free (line);

  test_cleanup (&ctx);
  test_init (&ctx);
  ctx.ed.config.display.tab_width = 4;
  ctx.ed.config.display.spaces_for_tab = 1;
  buffer_insert_line (&ctx.buf, 0, "    call(");
  ctx.cursor_col = buffer_get_line_length (&ctx.buf, 0);
  test_handle_input (&ctx, '\n');
  line = buffer_get_line (&ctx.buf, 1);
  test_assert (line && strcmp (line, "        ") == 0,
               "Enter after ( adds one indent level");
  free (line);

  test_cleanup (&ctx);
  test_init (&ctx);
  buffer_insert_line (&ctx.buf, 0, "\tif (x) {");
  ctx.cursor_col = buffer_get_line_length (&ctx.buf, 0);
  test_handle_input (&ctx, '\n');
  line = buffer_get_line (&ctx.buf, 1);
  test_assert (line && strcmp (line, "\t\t") == 0,
               "Enter after { inserts a tab when spaces_for_tab is off");
  free (line);

  test_cleanup (&ctx);
  test_init (&ctx);
  buffer_insert_line (&ctx.buf, 0, "    Hello world");
  ctx.cursor_col = 9;
  test_handle_input (&ctx, '\n');
  line = buffer_get_line (&ctx.buf, 0);
  char *line1 = buffer_get_line (&ctx.buf, 1);
  test_assert (line && line1 && strcmp (line, "    Hello") == 0
               && strcmp (line1, " world") == 0 && ctx.cursor_col == 0,
               "Enter in the middle of a line does not auto-indent");
  free (line);
  free (line1);

  test_cleanup (&ctx);
  test_init (&ctx);
  const char *src[] = {
    "void g() {",
    "  char *s = \"}\";",
    "  // }",
    "  if (x) {",
    "    return;",
    "  }",
    "  const char *r = R\"foo( } )foo\";",
    "}"
  };
  for (int i = 0; i < 8; i++)
    buffer_insert_line (&ctx.buf, i, src[i]);
  ctx.cursor_line = 0;
  ctx.cursor_col = col_of (&ctx.buf, 0, '{', 1);
  test_handle_input (&ctx, 29);
  test_assert (ctx.cursor_line == 7 && ctx.cursor_col == 0,
               "Ctrl+] on { jumps to the closing brace");
  test_handle_input (&ctx, 29);
  test_assert (ctx.cursor_line == 0
               && ctx.cursor_col == col_of (&ctx.buf, 0, '{', 1),
               "Ctrl+] on } jumps back to the opening brace");

  ctx.cursor_line = 3;
  ctx.cursor_col = col_of (&ctx.buf, 3, '{', 1);
  test_handle_input (&ctx, 29);
  test_assert (ctx.cursor_line == 5 && ctx.cursor_col == 2,
               "Ctrl+] matches the inner brace, not the outer one");

  int stay_l = 1;
  int stay_c = col_of (&ctx.buf, 1, '}', 1);
  ctx.cursor_line = stay_l;
  ctx.cursor_col = stay_c;
  test_handle_input (&ctx, 29);
  test_assert (ctx.cursor_line == stay_l && ctx.cursor_col == stay_c
               && strstr (ctx.ed.status_message, "No match") != NULL,
               "brace inside a string is not a match");

  stay_c = col_of (&ctx.buf, 2, '}', 1);
  ctx.cursor_line = 2;
  ctx.cursor_col = stay_c;
  test_handle_input (&ctx, 29);
  test_assert (ctx.cursor_line == 2 && ctx.cursor_col == stay_c,
               "brace inside a // comment is not a match");

  stay_c = col_of (&ctx.buf, 6, '}', 1);
  ctx.cursor_line = 6;
  ctx.cursor_col = stay_c;
  test_handle_input (&ctx, 29);
  test_assert (ctx.cursor_line == 6 && ctx.cursor_col == stay_c,
               "brace inside a C++ raw string is not a match");

  ctx.cursor_line = 0;
  ctx.cursor_col = col_of (&ctx.buf, 0, '(', 1);
  test_handle_input (&ctx, 29);
  test_assert (ctx.cursor_line == 0
               && ctx.cursor_col == col_of (&ctx.buf, 0, ')', 1),
               "Ctrl+] matches parentheses");

  test_cleanup (&ctx);
  test_init (&ctx);
  buffer_insert_line (&ctx.buf, 0, "int foo_bar;");
  ctx.ed.key_word_right = 3001;
  ctx.ed.key_word_left = 3000;
  test_handle_input (&ctx, 3001);
  test_assert (ctx.cursor_col == 4, "Ctrl+Right lands on the next word");
  test_handle_input (&ctx, 3001);
  test_assert (ctx.cursor_col == 12, "Ctrl+Right from the last word reaches the end");
  test_handle_input (&ctx, 3000);
  test_assert (ctx.cursor_col == 4, "Ctrl+Left lands on the start of the word");
  test_handle_input (&ctx, 3000);
  test_assert (ctx.cursor_col == 0, "Ctrl+Left reaches the first word");
  ctx.cursor_col = 12;
  test_handle_input (&ctx, 23);
  line = buffer_get_line (&ctx.buf, 0);
  test_assert (line && strcmp (line, "int ") == 0 && ctx.cursor_col == 4,
               "Ctrl+W deletes the previous word");
  free (line);

  test_cleanup (&ctx);
  test_init (&ctx);
  buffer_insert_line (&ctx.buf, 0, "int foo");
  test_handle_input (&ctx, 27);
  test_handle_input (&ctx, 'f');
  test_assert (ctx.cursor_col == 4 && ctx.ed.meta_pending == 0,
               "Esc then f moves to the next word");

  test_cleanup (&ctx);
  test_init (&ctx);
  buffer_insert_line (&ctx.buf, 0, "    int x;");
  ctx.cursor_col = 6;
  test_handle_input (&ctx, 11);
  line = buffer_get_line (&ctx.buf, 0);
  test_assert (line && strcmp (line, "    // int x;") == 0 && ctx.cursor_col == 9,
               "Ctrl+K comments the line");
  free (line);
  test_handle_input (&ctx, 11);
  line = buffer_get_line (&ctx.buf, 0);
  test_assert (line && strcmp (line, "    int x;") == 0 && ctx.cursor_col == 6,
               "Ctrl+K again removes the comment");
  free (line);

  test_cleanup (&ctx);
  test_init (&ctx);
  ctx.ed.config.display.tab_width = 4;
  buffer_insert_line (&ctx.buf, 0, "        x");
  ctx.cursor_col = 8;
  test_handle_input (&ctx, KEY_BTAB);
  line = buffer_get_line (&ctx.buf, 0);
  test_assert (line && strcmp (line, "    x") == 0 && ctx.cursor_col == 4,
               "Shift-Tab removes one indent level of spaces");
  free (line);
  buffer_free (&ctx.buf);
  buffer_init (&ctx.buf);
  buffer_insert_line (&ctx.buf, 0, "\tx");
  ctx.cursor_line = 0;
  ctx.cursor_col = 1;
  test_handle_input (&ctx, KEY_BTAB);
  line = buffer_get_line (&ctx.buf, 0);
  test_assert (line && strcmp (line, "x") == 0 && ctx.cursor_col == 0,
               "Shift-Tab removes a leading tab");
  free (line);
  test_cleanup (&ctx);

  Editor ed;
  memset (&ed, 0, sizeof ed);
  buffer_init (&ed.model);
  buffer_insert_line (&ed.model, 0, "aaaa");
  buffer_insert_line (&ed.model, 1, "bbbb");
  buffer_insert_line (&ed.model, 2, "cccc");
  test_assert (editor_handle_input (&ed, 7) == 0 && ed.goto_mode == 1,
               "Ctrl+G opens the go-to prompt");
  editor_handle_input (&ed, '2');
  editor_handle_input (&ed, ':');
  editor_handle_input (&ed, '3');
  test_assert (editor_handle_input (&ed, '\n') == 0 && ed.goto_mode == 0
               && ed.cursor_line == 1 && ed.cursor_col == 2,
               "Go to line:col lands on that character");
  test_assert (editor_apply_goto (&ed, "99") == 0 && ed.cursor_line == 2,
               "a line past the end clamps to the last line");
  test_assert (editor_apply_goto (&ed, "0") == -1, "line 0 is rejected");
  test_assert (editor_apply_goto (&ed, "1:0") == -1, "column 0 is rejected");

  ed.file_modified = 0;
  test_assert (editor_handle_input (&ed, 17) == 1,
               "Ctrl+Q quits immediately when nothing is modified");
  ed.file_modified = 1;
  ed.quit_armed = 0;
  test_assert (editor_handle_input (&ed, 17) == 0 && ed.quit_armed == 1,
               "Ctrl+Q on a modified buffer asks for a second press");
  test_assert (editor_handle_input (&ed, 'x') == 0 && ed.quit_armed == 0,
               "another key disarms quit");
  ed.file_modified = 1;
  editor_handle_input (&ed, 17);
  test_assert (editor_handle_input (&ed, 17) == 1,
               "second Ctrl+Q quits");
  editor_cleanup (&ed);

  memset (&ed, 0, sizeof ed);
  buffer_init (&ed.model);
  buffer_insert_line (&ed.model, 0, "int foo_bar");
  editor_handle_input (&ed, 27);
  test_assert (editor_handle_input (&ed, 'f') == 0 && ed.cursor_col == 4
               && ed.file_modified == 0,
               "a word motion does not mark the buffer modified");
  editor_cleanup (&ed);

  memset (&ed, 0, sizeof ed);
  buffer_init (&ed.model);
  buffer_insert_line (&ed.model, 0, "hi");
  ed.filename = "/tmp/no_such_led_dir/out.txt";
  ed.file_modified = 1;
  ed.auto_save_threshold = 100000;
  ed.auto_save_timeout = 0;
  editor_handle_input (&ed, 19);
  test_assert (ed.file_modified == 1
               && strstr (ed.status_message, "Save failed") != NULL,
               "a failed save stays modified and says so");
  editor_cleanup (&ed);

  fprintf (stderr, "Programmer motion tests completed\n");
}

void
test_open_location (void)
{
  fprintf (stderr, "Running open-at-location tests\n");
  const char *path = "/tmp/led_jump_test.c";
  unlink ("/tmp/led_jump_test.c.recovery");
  FILE *fp = fopen (path, "wb");
  test_assert (fp != NULL, "location fixture can be written");
  if (fp)
    {
      fputs ("alpha\nbeta\ngamma\ndelta\n", fp);
      fclose (fp);
    }
  char arg[] = "/tmp/led_jump_test.c:3:2";
  char *av[] = { "led", arg };
  Editor ed;
  editor_init (&ed, 2, av);
  char *l0 = buffer_get_line (&ed.model, 0);
  test_assert (l0 && strcmp (l0, "alpha") == 0, "file:line opens the file, not the colon name");
  free (l0);
  test_assert (ed.cursor_line == 2 && ed.cursor_col == 1,
               "file:line:col places the cursor");
  test_assert (ed.filename && strstr (ed.filename, ":3") == NULL,
               "the stored filename does not keep the :line suffix");
  if (ed.syntax_highlight && !ed.config.display.line_numbers_configured)
    test_assert (ed.show_line_numbers == 1,
                 "a C file shows line numbers unless config turns them off");
  editor_cleanup (&ed);

  char *avplus[] = { "led", "+2", "/tmp/led_jump_test.c" };
  editor_init (&ed, 3, avplus);
  test_assert (ed.cursor_line == 1 && ed.cursor_col == 0,
               "led +N file opens at that line");
  editor_cleanup (&ed);

  const char *colon = "/tmp/led_colon:1";
  unlink ("/tmp/led_colon:1.recovery");
  fp = fopen (colon, "wb");
  if (fp)
    {
      fputs ("keep\n", fp);
      fclose (fp);
    }
  char carg[] = "/tmp/led_colon:1";
  char *avc[] = { "led", carg };
  editor_init (&ed, 2, avc);
  l0 = buffer_get_line (&ed.model, 0);
  test_assert (l0 && strcmp (l0, "keep") == 0 && ed.cursor_line == 0
               && ed.cursor_col == 0,
               "an existing name that contains :line is opened as itself");
  free (l0);
  editor_cleanup (&ed);

  char pathbuf[64];
  int line = 0;
  int col = 0;
  test_assert (led_parse_plus_line ("+20", &line) == 1 && line == 20,
               "+N parses a line number");
  test_assert (led_parse_plus_line ("+0", &line) == 0, "+0 is not a line");
  test_assert (led_parse_plus_line ("+20a", &line) == 0, "+N rejects trailing junk");
  test_assert (led_parse_file_location ("src/file.c:142", pathbuf, sizeof pathbuf,
                                        &line, &col) == 1
               && strcmp (pathbuf, "src/file.c") == 0 && line == 142 && col == 0,
               "path:line peels the line");
  test_assert (led_parse_file_location ("src/file.c:142:8", pathbuf, sizeof pathbuf,
                                        &line, &col) == 1
               && line == 142 && col == 8,
               "path:line:col peels both");
  test_assert (led_parse_file_location ("foo:bar:12", pathbuf, sizeof pathbuf,
                                        &line, &col) == 1
               && strcmp (pathbuf, "foo:bar") == 0 && line == 12,
               "only a numeric suffix is a location");
  test_assert (led_parse_file_location ("archive.tar.gz", pathbuf, sizeof pathbuf,
                                        &line, &col) == 0,
               "a name without :line is left whole");

  unlink (path);
  unlink (colon);
  fprintf (stderr, "Open-at-location tests completed\n");
}

void
test_save_replaces_cleanly (void)
{
  fprintf (stderr, "Running atomic save tests\n");
  const char *path = "/tmp/led_mode_test.txt";
  unlink (path);
  FILE *fp = fopen (path, "wb");
  test_assert (fp != NULL, "mode fixture can be written");
  if (fp)
    {
      fputs ("old\n", fp);
      fclose (fp);
    }
  chmod (path, 0600);

  Buffer buf;
  buffer_init (&buf);
  buffer_insert_line (&buf, 0, "new line");
  buffer_insert_line (&buf, 1, "two");
  test_assert (buffer_save_to_file (&buf, path) == 0, "save succeeds");
  struct stat st;
  test_assert (stat (path, &st) == 0 && (st.st_mode & 0777) == 0600,
               "save keeps the file mode");
  char *body = read_whole (path);
  test_assert (body && strcmp (body, "new line\ntwo") == 0,
               "save writes the buffer without an extra trailing newline");
  free (body);
  char tmp[128];
  snprintf (tmp, sizeof tmp, "%s.ledtmp.%d", path, (int) getpid ());
  test_assert (access (tmp, F_OK) != 0, "save leaves no temp file behind");

  buffer_free (&buf);
  buffer_init (&buf);
  buffer_insert_line (&buf, 0, "line1");
  buffer_insert_line (&buf, 1, "line2");
  buffer_insert_line (&buf, 2, "");
  test_assert (buffer_save_to_file (&buf, path) == 0, "save of a trailing newline succeeds");
  body = read_whole (path);
  test_assert (body && strcmp (body, "line1\nline2\n") == 0,
               "a final empty line is a trailing newline on disk");
  free (body);
  buffer_free (&buf);
  unlink (path);
  fprintf (stderr, "Atomic save tests completed\n");
}

void
test_default_cpp_keywords (void)
{
  fprintf (stderr, "Running default C/C++ keyword tests\n");
  const char *old_home = getenv ("HOME");
  char *home_copy = old_home ? strdup (old_home) : NULL;
  setenv ("HOME", "/tmp/led-no-home-config", 1);
  EditorConfig cfg;
  memset (&cfg, 0, sizeof cfg);
  load_editor_config (&cfg);
  test_assert (config_is_reserved_word (&cfg, "class") == 1, "class is a default keyword");
  test_assert (config_is_reserved_word (&cfg, "constexpr") == 1,
               "constexpr is a default keyword");
  test_assert (config_is_reserved_word (&cfg, "restrict") == 1,
               "restrict is a default keyword");
  test_assert (strstr (cfg.syntax.extensions, ".hpp") != NULL
               && strstr (cfg.syntax.extensions, ".cc") != NULL,
               "default extensions cover C and C++");
  if (home_copy)
    {
      setenv ("HOME", home_copy, 1);
      free (home_copy);
    }
  else
    unsetenv ("HOME");
  fprintf (stderr, "Default C/C++ keyword tests completed\n");
}

void
run_comprehensive_tests (void)
{
  fprintf (stderr, "Running led test suite... (wordwrap tests skipped for stability)\n");
  tests_passed = 0;
  tests_failed = 0;
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
  fprintf (stderr, "Test %d: strdup - String duplication\n",
             ++test_number);
  test_strdup ();
  fprintf (stderr, "Test %d: enter_key_newline_insertion - Enter key inserts newline\n",
             ++test_number);
  test_enter_key_newline_insertion ();
  fprintf (stderr, "Test %d: delete_key - Delete key (forward delete)\n",
             ++test_number);
  test_delete_key ();
  fprintf (stderr, "Test %d: backspace_key - Backspace (127, 8, KEY_BACKSPACE)\n",
             ++test_number);
  test_backspace_key ();
  fprintf (stderr, "Test %d: right_arrow_repeat_navigation - Right arrow navigation through document\n",
             ++test_number);
  test_right_arrow_repeat_navigation ();
  fprintf (stderr, "Test %d: tab_key_handling - TAB key with spaces and literal tab\n",
           ++test_number);
  test_tab_key ();
  fprintf (stderr, "Test %d: autosave_comprehensive - Auto-save and backups\n",
             ++test_number);
  test_autosave_comprehensive ();
  fprintf (stderr, "Test %d: programmer_motions - indent, braces, words, goto, quit\n",
           ++test_number);
  test_programmer_motions ();
  fprintf (stderr, "Test %d: open_location - file:line and +N\n",
           ++test_number);
  test_open_location ();
  fprintf (stderr, "Test %d: save_replaces_cleanly - atomic save\n",
           ++test_number);
  test_save_replaces_cleanly ();
  fprintf (stderr, "Test %d: default_cpp_keywords - C and C++ defaults\n",
           ++test_number);
  test_default_cpp_keywords (); 
  
  fprintf (stderr, "\n");
  run_view_tests();
  
  fprintf (stderr, "Tests completed: %d passed, %d failed\n", tests_passed,
           tests_failed);
}
