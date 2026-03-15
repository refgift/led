#include <ncurses.h>
#include <string.h>
#include <stdlib.h>
#include <locale.h>
#include "editor.h"
#include "view.h"
#include "config.h"

extern void run_tests (Editor * ed);
extern void run_all_tests ();

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
      (void) setlocale (LC_ALL, "");    // Enable extended character support
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
          int ch = getch ();
          if (ch == 17)
            break;              // Ctrl+Q
          editor_handle_input (&ed, ch);
          editor_draw (stdscr, &ed);
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
  // Basic test output
  fprintf (stderr, "Hello World\n");
  // Model tests
  int total_size = 0;
  for (int i = 0; i < ed->model.num_lines; i++)
    {
      total_size += buffer_get_line_length (&ed->model, i) + 1; // +1 for newline
    }
  fprintf (stderr, "Model Test: Buffer size: %u bytes\n", total_size);
  fprintf (stderr, "Model Test: Number of lines: %u\n", ed->model.num_lines);
  // Config test
  fprintf (stderr, "Config Test: Syntax extensions: %s\n",
           ed->config.syntax.extensions);
  fprintf (stderr, "Config Test: Paired keywords: %s\n",
           ed->config.syntax.paired_keywords);
  // Test controller: simulate inputs
  int fake_inputs[] = { 'H', 'e', 'l', 'l', 'o', 19, 0 };       // 'H','e','l','l','o', Ctrl+S (19), terminator
  for (int i = 0; fake_inputs[i] != 0; i++)
    {
      editor_handle_input (ed, fake_inputs[i]);
      int new_size = 0;
      for (int j = 0; j < ed->model.num_lines; j++)
        {
          new_size += buffer_get_line_length (&ed->model, j) + 1;
        }
      fprintf (stderr,
               "Test Controller %d: Input %c, Buffer size: %u, Cursor: (%u,%u)\n",
               i, fake_inputs[i], new_size, ed->cursor_line, ed->cursor_col);
    }
  fprintf (stderr, "Tests completed\n");
}
