#include <ncurses.h>
#include <string.h>
#include <stdlib.h>
#include <locale.h>
#include <sched.h>
#include "editor.h"
#include "view.h"
#include "config.h"

extern void run_tests (Editor * ed);
extern void run_all_tests ();


#include <sched.h>
int
main (int argc, char *argv[])
{
  if (getenv ("LED_TEST"))
    {
      run_all_tests ();
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
      (void) setlocale (LC_ALL, "C");    // Use C locale for consistent behavior
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
  if (!test_mode)
    {
      // Initialize color pairs
      init_pair (1, ed.config.colors.normal_fg, ed.config.colors.normal_bg);
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
      int dummy_y, dummy_x;
      draw_initial (stdscr, &ed.model, &ed.scroll_row, &ed.scroll_col,
                    ed.cursor_line, ed.cursor_col, ed.show_line_numbers,
                    ed.syntax_highlight, &dummy_y, &dummy_x, &ed.config);
    }
  if (test_mode)
    {
      run_tests (&ed);
    }
  else
    {
      while (1)
        {
		sched_yield();

          int ch = getch ();
          if (ch == 17)
            break;              // Ctrl+Q
          editor_handle_input (&ed, ch);
          editor_draw (stdscr, &ed);
          sched_yield();
        }
    }
  editor_cleanup (&ed);
  if (!test_mode)
    endwin ();
  return 0;
}


void
run_tests (Editor *ed)
{
  (void) ed;
  // Run the comprehensive test suite instead of simple simulation
  run_all_tests ();
}
