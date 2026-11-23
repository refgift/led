#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include "editor.h"
#include "view.h"
#include "config.h"
#include "frontend/frontend.h"
#include "undo_store.h"
#include "test_controller.h"

/* main.c ties the parts together and owns nothing else: it starts the
 * front end, hands keys to the editor, and asks the view to draw. It
 * does not know which front end is linked in. */
extern int tests_passed;
extern int tests_failed;

/* Set by SIGHUP / SIGTERM: the terminal closed or the session ended. */
static volatile sig_atomic_t stop_requested = 0;

/* What the out-of-memory handler needs: the work to save and the
 * display to put back. Set once the editor is running. */
static Editor *g_running;
static const Frontend *g_display;

static void
on_out_of_memory (void)
{
  if (g_running)
    (void) editor_write_recovery (g_running);
  if (g_display)
    g_display->stop ();
  if (g_running && g_running->filename && g_running->file_modified)
    fprintf (stderr, "led: unsaved changes were written to %s.recovery\n",
             g_running->filename);
}

static void
on_stop_signal (int sig)
{
  (void) sig;
  stop_requested = 1;
}

/* Each quality's colours, from the user's config. */
static void
styles_from_config (const EditorConfig *c, FrontendStyle s[LED_Q_COUNT])
{
  memset (s, 0, sizeof (FrontendStyle) * LED_Q_COUNT);
  s[LED_Q_NORMAL] = (FrontendStyle) { c->colors.normal_fg, c->colors.normal_bg };
  s[LED_Q_SELECTION] = (FrontendStyle) { c->colors.selection_fg, c->colors.selection_bg };
  s[LED_Q_SEMICOLON] = (FrontendStyle) { c->colors.semicolon_fg, c->colors.semicolon_bg };
  s[LED_Q_LEVEL1] = (FrontendStyle) { c->colors.meta_level1_fg, c->colors.meta_level1_bg };
  s[LED_Q_LEVEL2] = (FrontendStyle) { c->colors.meta_level2_fg, c->colors.meta_level2_bg };
  s[LED_Q_LEVEL3] = (FrontendStyle) { c->colors.meta_level3_fg, c->colors.meta_level3_bg };
  s[LED_Q_LEVEL4] = (FrontendStyle) { c->colors.meta_level4_fg, c->colors.meta_level4_bg };
  s[LED_Q_RESERVED] = (FrontendStyle) { c->colors.reserved_words_fg, c->colors.reserved_words_bg };
}

/* Read the device size into the screen grid and the editor. */
static void
adopt_size (const Frontend *fe, Editor *ed)
{
  int cols = 0, rows = 0;
  fe->size (&cols, &rows);
  screen_resize (cols, rows);
  ed->page_rows = rows;
}

/* A full draw is the ordinary draw with nothing remembered, so the
 * first frame, a resize and F4 paint the same picture as any key
 * press: status line, word wrap, and lines cut at a character. */
static void
full_draw (Pane *text_win, Editor *ed)
{
  view_invalidate ();
  editor_draw (screen_pane (), text_win, ed);
}

/* Tests keep their undo histories in a directory of their own, so a
 * run neither reads what an earlier run left nor litters the real one. */
static char test_store[] = "/tmp/led_test_undo_XXXXXX";

static void
test_store_open (void)
{
  if (mkdtemp (test_store))
    undo_store_set_dir (test_store);
}

static void
test_store_close (void)
{
  char cmd[sizeof test_store + 16];
  snprintf (cmd, sizeof cmd, "rm -rf '%s'", test_store);
  if (strncmp (test_store, "/tmp/led_test_undo_", 19) == 0
      && system (cmd) != 0)
    fprintf (stderr, "could not remove %s\n", test_store);
  undo_store_set_dir (NULL);
}

static int
run_tests (int argc, char *argv[])
{
  test_store_open ();
  /* Tests draw into the memory front end; no terminal is touched. */
  frontend_use (frontend_memory ());
  Editor ed;
  editor_init (&ed, argc, argv);
  run_comprehensive_tests ();
  fprintf (stderr, "\nTest summary: %d passed, %d failed\n", tests_passed,
           tests_failed);
  fprintf (stderr, tests_failed == 0 ? "ALL TESTS PASSED\n"
           : "SOME TESTS FAILED\n");
  editor_cleanup (&ed);
  test_store_close ();
  return 0;
}

int
main (int argc, char *argv[])
{
  /* "--prefix DIR": keep the config files under DIR, not $HOME/.config.
   * Taken out of argv here so the rest sees only the file arguments. */
  int kept = 1;
  for (int i = 1; i < argc; i++)
    {
      if (strcmp (argv[i], "--prefix") == 0)
        {
          if (i + 1 >= argc)
            {
              fprintf (stderr, "led: --prefix needs a directory\n");
              return 2;
            }
          config_set_prefix (argv[++i]);
        }
      else if (strncmp (argv[i], "--prefix=", 9) == 0)
        config_set_prefix (argv[i] + 9);
      else
        argv[kept++] = argv[i];
    }
  argc = kept;
  argv[argc] = NULL;

  if (getenv ("LED_TEST"))
    {
      frontend_use (frontend_memory ());
      test_store_open ();
      run_comprehensive_tests ();
      test_store_close ();
      return 0;
    }
  if (argc > 1 && strcmp (argv[1], "-t") == 0)
    return run_tests (argc - 1, argv + 1);

  /* First run: leave both config files behind, defaults commented. */
  (void) config_write_defaults ();
  Editor ed;
  editor_init (&ed, argc, argv);

  const Frontend *fe = frontend_default ();
  FrontendStyle styles[LED_Q_COUNT];
  styles_from_config (&ed.config, styles);
  if (fe->start (styles) != 0)
    {
      fprintf (stderr, "Error: failed to start the display\n");
      editor_cleanup (&ed);
      return 1;
    }
  frontend_use (fe);
  g_running = &ed;
  g_display = fe;
  xalloc_reserve (1 << 20);
  xalloc_on_failure (on_out_of_memory);
  editor_bind_terminal_keys (&ed);
  adopt_size (fe, &ed);

  /* The whole screen owns the border and the status line; the text
   * pane owns the text. Its geometry depends on show_border (F4). */
  Pane *text_win = NULL;
  (void) view_create_text_window_ex (screen_pane (), &text_win,
                                     ed.config.display.show_border);
  full_draw (text_win, &ed);

  /* No SA_RESTART, so a signal makes read_key return and the loop
   * can save the work before leaving. */
  struct sigaction sa;
  memset (&sa, 0, sizeof sa);
  sa.sa_handler = on_stop_signal;
  sigemptyset (&sa.sa_mask);
  sigaction (SIGHUP, &sa, NULL);
  sigaction (SIGTERM, &sa, NULL);

  int read_errors = 0;
  while (1)
    {
      int ch = fe->read_key ();
      if (stop_requested)
        {
          (void) editor_write_recovery (&ed);
          break;
        }
      if (ch == LED_KEY_NONE)
        {
          /* A blocking read that keeps failing means input is gone
           * (terminal closed without a signal). Do not spin. */
          if (!isatty (STDIN_FILENO) || ++read_errors > 50)
            {
              (void) editor_write_recovery (&ed);
              break;
            }
          continue;
        }
      read_errors = 0;
      if (ch == LED_KEY_RESIZE)
        {
          adopt_size (fe, &ed);
          if (text_win)
            view_resize_windows_ex (screen_pane (), text_win,
                                    ed.config.display.show_border);
          else
            (void) view_create_text_window_ex (screen_pane (), &text_win,
                                               ed.config.display.show_border);
          full_draw (text_win, &ed);
          continue;
        }
      int old_border = ed.config.display.show_border;
      if (editor_handle_input (&ed, ch))
        break;                  /* Ctrl+Q */
      if (old_border != ed.config.display.show_border)
        {
          /* F4 toggled the border: the text pane changes shape. */
          view_recreate_text_window (screen_pane (), &text_win,
                                     ed.config.display.show_border);
          full_draw (text_win, &ed);
          continue;
        }
      editor_draw (screen_pane (), text_win, &ed);
    }
  pane_free (text_win);
  editor_cleanup (&ed);
  fe->stop ();
  return 0;
}
