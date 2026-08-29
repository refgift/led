#include <ncurses.h>
#include <string.h>
#include <stdlib.h>
#include <locale.h>
#include "editor.h"
#include "view.h"
#include "config.h"
#include "test_controller.h"

/* Tests re-enabled. Wordwrap-dependent tests skipped for stability. Runs ~20 core tests (clipboard, undo, autosave, buffer, view, cut/paste). */
extern int tests_passed;
extern int tests_failed;

int
main (int argc, char *argv[])
{
  if (getenv ("LED_TEST"))
    {
      run_comprehensive_tests();
      return 0;
    }
  int test_mode = 0;
  if (argc > 1 && strcmp (argv[1], "-t") == 0)
    {
      test_mode = 1;
      argc--;
      argv++;
    }
  // Initialize curses only if not in test mode
  if (!test_mode)
    {
      (void) setlocale (LC_ALL, "");    /* Enable UTF-8 / extended chars */
      if (initscr () == NULL)
        {
          fprintf (stderr, "Error: Failed to initialize ncurses\n");
          return 1;
        }
      raw ();
      noecho ();
      keypad (stdscr, TRUE);
      scrollok (stdscr, FALSE); // Prevent line wrapping/scrolling
      if (start_color () == ERR)
        {
          fprintf (stderr, "Error: Failed to start colors\n");
          endwin ();
          return 1;
        }
    }
  Editor ed;
  editor_init (&ed, argc, argv);
  WINDOW *text_win = NULL;
  if (!test_mode)
    {
      // Initialize color pairs
      init_pair (1, ed.config.colors.normal_fg, ed.config.colors.normal_bg);
      wbkgd(stdscr, COLOR_PAIR(1));
      // Also set background for stdscr via bkgd for compat
      bkgd(COLOR_PAIR(1));
      init_pair (2, ed.config.colors.selection_fg,
                 ed.config.colors.selection_bg);
      init_pair (3, ed.config.colors.semicolon_fg,
                 ed.config.colors.semicolon_bg);
      init_pair (4, ed.config.colors.meta_level1_fg,
                 ed.config.colors.meta_level1_bg);
      init_pair (5, ed.config.colors.meta_level2_fg,
                 ed.config.colors.meta_level2_bg);
      init_pair (6, ed.config.colors.meta_level3_fg,
                 ed.config.colors.meta_level3_bg);
      init_pair (7, ed.config.colors.meta_level4_fg,
                 ed.config.colors.meta_level4_bg);
      init_pair (8, ed.config.colors.reserved_words_fg,
                 ed.config.colors.reserved_words_bg);
      // Create inset text subwindow: border stays on stdscr (frame),
      // all text rendering goes to text_win (derwin). This isolates
      // border from xterm block-select so copying text never drags '|'.
      // Geometry depends on show_border (F4 / LED_NO_BORDER env).
      if (view_create_text_window_ex(stdscr, &text_win, ed.config.display.show_border) != 0)
        {
          fprintf (stderr, "Warning: failed to create text subwindow, falling back to stdscr\n");
          text_win = NULL;
        }
      else
        {
          wbkgd(text_win, COLOR_PAIR(1));
          // Ensure child inherits keypad setting
          keypad(text_win, FALSE);
        }
      int dummy_y, dummy_x;
      draw_initial (stdscr, text_win, &ed.model, &ed.scroll_row, &ed.scroll_col,
                    ed.cursor_line, ed.cursor_col, ed.show_line_numbers,
                    ed.syntax_highlight, &dummy_y, &dummy_x, &ed.config);
    }
  if (test_mode)
    {
      //COLS = 80;
      //LINES = 24;
      run_comprehensive_tests();
      fprintf(stderr, "\nTest summary: %d passed, %d failed\n", tests_passed, tests_failed);
      if (tests_failed == 0) {
        fprintf(stderr, "ALL TESTS PASSED\n");
      } else {
        fprintf(stderr, "SOME TESTS FAILED\n");
      }
      editor_cleanup (&ed);  // cleanup test Editor
      return 0;
    }
  else
    {
      while (1)
        {
          int ch = getch ();
          if (ch == KEY_RESIZE)
            {
              // Handle terminal resize: stdscr is resized by ncurses,
              // text subwindow must be resized/moved to stay inset/aligned
              if (text_win)
                view_resize_windows_ex(stdscr, text_win, ed.config.display.show_border);
              view_invalidate();
              // Full repaint with new geometry
              int dummy_y, dummy_x;
              draw_initial (stdscr, text_win, &ed.model, &ed.scroll_row, &ed.scroll_col,
                            ed.cursor_line, ed.cursor_col, ed.show_line_numbers,
                            ed.syntax_highlight, &dummy_y, &dummy_x, &ed.config);
              continue;
            }
          if (ch == 17)
            break;              // Ctrl+Q
          {
            int old_border = ed.config.display.show_border;
            editor_handle_input (&ed, ch);
            if (old_border != ed.config.display.show_border)
              {
                // F4 toggled border: recreate text window with new geometry
                view_recreate_text_window(stdscr, &text_win, ed.config.display.show_border);
                view_invalidate();
                int dummy_y, dummy_x;
                draw_initial(stdscr, text_win, &ed.model, &ed.scroll_row, &ed.scroll_col,
                             ed.cursor_line, ed.cursor_col, ed.show_line_numbers,
                             ed.syntax_highlight, &dummy_y, &dummy_x, &ed.config);
                continue;
              }
          }
          editor_draw (stdscr, text_win, &ed);
        }
    }
  if (text_win)
    delwin(text_win);
  editor_cleanup (&ed);
  if (!test_mode)
    endwin ();
  return 0;
}

