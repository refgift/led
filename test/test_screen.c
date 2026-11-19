/* What reaches the screen, read back from the memory front end. */
#include "test_controller.h"
#include "editor.h"
#include "view.h"
#include "frontend/frontend.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void test_assert (int condition, const char *msg);
void run_screen_tests (void);

static void
editor_blank (Editor *ed)
{
  memset (ed, 0, sizeof *ed);
  buffer_init (&ed->model);
  ed->config.display.tab_width = 8;
  ed->auto_save_threshold = 100000;
}

static void
draw (Editor *ed, Pane *text)
{
  editor_draw (screen_pane (), text, ed);
}

void
run_screen_tests (void)
{
  fprintf (stderr, "\n=== Screen tests (memory front end) ===\n");
  const Frontend *before = frontend_current ();
  frontend_use (frontend_memory ());
  memory_reset (20, 6);
  screen_resize (20, 6);
  view_invalidate ();

  Editor ed;
  editor_blank (&ed);
  buffer_insert_line (&ed.model, 0, "hello");
  buffer_insert_line (&ed.model, 1, "\xD0\xB6\xD1\x83\xD0\xBA");   /* жук */
  buffer_insert_line (&ed.model, 2, "a\tb");
  Pane *text = NULL;
  view_create_text_window_ex (screen_pane (), &text, 0);
  test_assert (text && pane_rows (text) == 5 && pane_cols (text) == 20,
               "borderless text pane leaves one row for the status line");

  char row[128];
  int cx, cy;
  draw (&ed, text);
  test_assert (strcmp (memory_row (0, row, sizeof row), "hello") == 0,
               "row 0 shows the first line");
  test_assert (memory_char (0, 1) == 0x436 && memory_char (2, 1) == 0x43A,
               "a Cyrillic letter is one cell holding its character number");
  test_assert (strcmp (memory_row (2, row, sizeof row), "a       b") == 0,
               "a tab is drawn as blanks up to the tab stop");
  memory_caret (&cx, &cy);
  test_assert (cx == 0 && cy == 0, "the last show leaves the caret at the cursor");
  test_assert (memory_quality (0, 0) == LED_Q_NORMAL,
               "plain text is shown with the normal quality");

  /* One typed character: only a few cells are sent, not the screen. */
  int calls0 = memory_calls ();
  ed.cursor_col = 5;
  editor_handle_input (&ed, '!');
  draw (&ed, text);
  memory_caret (&cx, &cy);
  test_assert (strcmp (memory_row (0, row, sizeof row), "hello!") == 0
               && cx == 6 && cy == 0,
               "typing shows the new character and moves the caret");
  test_assert (memory_calls () - calls0 < 60,
               "a one-character edit sends only the cells that changed");

  /* Deleting text hides the cells it leaves behind. */
  editor_handle_input (&ed, LED_KEY_BACKSPACE);
  editor_handle_input (&ed, LED_KEY_BACKSPACE);
  draw (&ed, text);
  test_assert (strcmp (memory_row (0, row, sizeof row), "hell") == 0
               && memory_char (4, 0) == ' ' && memory_quality (5, 0) == 0,
               "deleted cells are hidden");
  memory_caret (&cx, &cy);
  test_assert (cx == 4 && cy == 0 && memory_quality (4, 0) == LED_Q_NORMAL,
               "the caret cell is shown even when it is blank");

  /* Selection is a quality, whatever colour the front end gives it. */
  ed.syntax_highlight = 1;
  editor_handle_input (&ed, 1);         /* Ctrl+A */
  draw (&ed, text);
  test_assert (memory_quality (0, 0) == LED_Q_SELECTION
               && memory_quality (2, 1) == LED_Q_SELECTION,
               "selected cells are shown with the selection quality");
  editor_handle_input (&ed, LED_KEY_UP);
  draw (&ed, text);
  test_assert (memory_quality (0, 0) != LED_Q_SELECTION,
               "the selection quality goes when the selection does");

  /* Border on: the frame is drawn and the text moves in by one cell. */
  ed.syntax_highlight = 0;
  ed.config.display.show_border = 1;
  view_recreate_text_window (screen_pane (), &text, 1);
  view_invalidate ();
  draw (&ed, text);
  test_assert (memory_char (0, 0) == 0x250C && memory_char (19, 0) == 0x2510
               && memory_char (0, 2) == 0x2502,
               "the border is drawn with line characters");
  test_assert (memory_char (1, 1) == 'h' && memory_char (4, 1) == 'l',
               "with a border the text is inset by one cell");

  /* Scrolling: Page Down by the screen height. */
  ed.config.display.show_border = 0;
  view_recreate_text_window (screen_pane (), &text, 0);
  for (int i = 3; i < 40; i++)
    {
      char line[16];
      snprintf (line, sizeof line, "line %d", i);
      buffer_insert_line (&ed.model, i, line);
    }
  ed.page_rows = 10;
  ed.cursor_line = 0;
  editor_handle_input (&ed, LED_KEY_NPAGE);
  test_assert (ed.cursor_line == 7, "Page Down moves by the front end's rows");
  view_invalidate ();
  draw (&ed, text);
  memory_caret (&cx, &cy);
  int found = 0;
  for (int y = 0; y < 5; y++)
    if (strcmp (memory_row (y, row, sizeof row), "line 7") == 0 && y == cy)
      found = 1;
  test_assert (found, "after Page Down the caret row shows the cursor line");

  /* A resize forgets what was sent, so everything is sent again. */
  memory_reset (30, 8);
  screen_resize (30, 8);
  view_resize_windows_ex (screen_pane (), text, 0);
  view_invalidate ();
  draw (&ed, text);
  test_assert (memory_calls () > 0
               && strncmp (memory_row (cy, row, sizeof row), "line", 4) == 0,
               "after a resize the screen is sent again");

  pane_free (text);
  editor_cleanup (&ed);
  screen_resize (0, 0);
  view_invalidate ();
  frontend_use (before);
}
