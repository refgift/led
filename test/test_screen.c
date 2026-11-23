/* What reaches the screen, read back from the memory front end. */
#include "test_controller.h"
#include "editor.h"
#include "view.h"
#include "frontend/frontend.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

int *compute_line_colors (const char *full_line, int line_len,
                          int highlight_pair, EditorConfig *config,
                          int *brace_level, int *brace_top, int *brace_stack,
                          int *kw_level, int *kw_top, int *kw_stack,
                          int *in_comment);
void run_color_tests (void);

/* Colour every byte of one line with a fresh nesting state. */
static int *
colors_of (EditorConfig *cfg, const char *line, int *kw_level_out)
{
  int bl = 1, bt = 0, kl = 1, kt = 0;
  static int bs[256], ks[100];
  int ic = 0;
  int *c = compute_line_colors (line, (int) strlen (line), 4, cfg,
                                &bl, &bt, bs, &kl, &kt, ks, &ic);
  if (kw_level_out)
    *kw_level_out = kl;
  return c;
}

void
run_color_tests (void)
{
  fprintf (stderr, "\n=== Colour tests ===\n");
  static EditorConfig cfg;
  set_default_config (&cfg);
  test_assert (cfg.syntax.paired_keywords[0] == '\0',
               "no word pairs by default: C nests with brackets");
  /* Word pairs are for languages that nest with words; give one here. */
  strcpy (cfg.syntax.paired_keywords, "begin-end");
  config_parse_syntax (&cfg);
  int *c;

  c = colors_of (&cfg, "  break;", NULL);
  test_assert (c && c[2] == LED_Q_RESERVED && c[6] == LED_Q_RESERVED,
               "a reserved word keeps its colour when ';' follows it");
  test_assert (c && c[7] == LED_Q_SEMICOLON, "and the ';' has its own colour");
  free (c);

  c = colors_of (&cfg, "  break; x", NULL);
  test_assert (c && c[2] == LED_Q_RESERVED && c[7] == LED_Q_SEMICOLON
               && c[8] == LED_Q_NORMAL && c[9] == LED_Q_NORMAL,
               "the same with a space after the ';'");
  free (c);

  c = colors_of (&cfg, "return(x);", NULL);
  test_assert (c && c[0] == LED_Q_RESERVED && c[5] == LED_Q_RESERVED
               && c[6] == LED_Q_LEVEL1 && c[7] == LED_Q_NORMAL
               && c[8] == LED_Q_LEVEL1 && c[9] == LED_Q_SEMICOLON,
               "a word ends at a bracket too");
  free (c);

  c = colors_of (&cfg, "int a,b;", NULL);
  test_assert (c && c[0] == LED_Q_RESERVED && c[4] == LED_Q_NORMAL
               && c[5] == LED_Q_LEVEL1 && c[6] == LED_Q_NORMAL
               && c[7] == LED_Q_SEMICOLON,
               "a comma separates words and does not join them");
  free (c);

  c = colors_of (&cfg, "while", NULL);
  test_assert (c && c[0] == LED_Q_RESERVED && c[4] == LED_Q_RESERVED,
               "a word at the end of the line is coloured");
  free (c);

  /* A paired keyword against a ';' must still count for nesting, or
   * every later line is coloured one level off. */
  int kl = 0;
  c = colors_of (&cfg, "begin;", &kl);
  test_assert (c && c[0] == LED_Q_LEVEL1 && c[5] == LED_Q_SEMICOLON && kl == 2,
               "a paired keyword against ';' is coloured and opens a level");
  free (c);

  /* In C, "if" has no "then": it is a reserved word like any other,
   * the same colour every time, and opens no level. */
  c = colors_of (&cfg, "if (a) x; if (b) y; if (c) z;", &kl);
  test_assert (c && c[0] == LED_Q_RESERVED && c[10] == LED_Q_RESERVED
               && c[20] == LED_Q_RESERVED && kl == 1,
               "every 'if' has the reserved-word colour and opens no level");
  free (c);

  /* Strings, character literals and comments are not code. */
  c = colors_of (&cfg, "x = \"if (\"; y(1);", NULL);
  test_assert (c && c[5] == LED_Q_NORMAL && c[8] == LED_Q_NORMAL
               && c[13] == LED_Q_LEVEL1 && c[15] == LED_Q_LEVEL1,
               "a bracket or keyword inside a string is plain and does not nest");
  free (c);
  c = colors_of (&cfg, "c = '('; n = 1'000; f(2);", NULL);
  test_assert (c && c[5] == LED_Q_NORMAL && c[21] == LED_Q_LEVEL1
               && c[23] == LED_Q_LEVEL1,
               "a character literal is skipped; a digit separator is not a quote");
  free (c);
  c = colors_of (&cfg, "a(1); // if (", &kl);
  test_assert (c && c[1] == LED_Q_LEVEL1 && c[9] == LED_Q_NORMAL
               && c[12] == LED_Q_NORMAL && kl == 1,
               "a line comment is plain");
  free (c);
  c = colors_of (&cfg, "a /* ( if */ (b)", NULL);
  test_assert (c && c[5] == LED_Q_NORMAL && c[7] == LED_Q_NORMAL
               && c[13] == LED_Q_LEVEL1 && c[15] == LED_Q_LEVEL1,
               "a block comment is plain, and code after it is coloured");
  free (c);

  /* A block comment left open carries to the next line. */
  {
    int bl = 1, bt = 0, k = 1, kt = 0, ic = 0;
    static int bs[256], ks[100];
    int *c1 = compute_line_colors ("x /* open (", 11, 4, &cfg,
                                   &bl, &bt, bs, &k, &kt, ks, &ic);
    test_assert (ic == 1 && bl == 1, "an open block comment is carried on");
    int *c2 = compute_line_colors ("( still ) */ f(1)", 17, 4, &cfg,
                                   &bl, &bt, bs, &k, &kt, ks, &ic);
    test_assert (c2 && ic == 0 && c2[0] == LED_Q_NORMAL && c2[8] == LED_Q_NORMAL
                 && c2[14] == LED_Q_LEVEL1 && bl == 1,
                 "the next line stays plain until the comment closes");
    free (c1);
    free (c2);
  }

  /* Colour names in colorization.conf. */
  test_assert (string_to_color ("BLUE") == LED_COLOR_BLUE
               && string_to_color ("blue") == LED_COLOR_BLUE
               && string_to_color (" Blue ") == LED_COLOR_BLUE,
               "a colour name is read in any letter case");
  test_assert (string_to_color ("BRIGHT_BLUE") == LED_COLOR_BRIGHT + LED_COLOR_BLUE
               && string_to_color ("LIGHT_BLUE") == LED_COLOR_BRIGHT + LED_COLOR_BLUE
               && string_to_color ("lightblue") == LED_COLOR_BRIGHT + LED_COLOR_BLUE
               && string_to_color ("BRIGHT_BLUE") != string_to_color ("BLUE"),
               "each colour has a bright variant, distinct from the plain one");
  test_assert (string_to_color ("BRIGHT_WHITE") == 15
               && string_to_color ("GRAY") == 8 && string_to_color ("grey") == 8,
               "bright white is colour 15 and gray is bright black");
  test_assert (string_to_color ("208") == 208 && string_to_color ("0") == 0
               && string_to_color ("256") == LED_COLOR_WHITE,
               "a colour can be given by number, 0 to 255");
  test_assert (string_to_color ("DEFAULT") == LED_COLOR_DEFAULT,
               "DEFAULT is the terminal's own colour");
  test_assert (string_to_color ("PURPLE") == LED_COLOR_WHITE
               && string_to_color ("BRIGHT_") == LED_COLOR_WHITE
               && string_to_color ("") == LED_COLOR_WHITE,
               "an unknown name falls back to white");

  /* Deep nesting must not read past the level stack. */
  char deep[700];
  memset (deep, '(', 300);
  memset (deep + 300, ')', 300);
  deep[600] = '\0';
  c = colors_of (&cfg, deep, NULL);
  test_assert (c && c[0] == LED_Q_LEVEL1 && c[599] == LED_Q_LEVEL1,
               "300 nested brackets colour without reading out of bounds");
  free (c);

  /* The nesting-only pass must agree with the colouring pass. */
  Editor ed;
  memset (&ed, 0, sizeof ed);
  buffer_init (&ed.model);
  buffer_insert_line (&ed.model, 0, "begin;");
  buffer_insert_line (&ed.model, 1, "begin x");
  int bl = 0, bt = 0, kl2 = 0, kt = 0;
  static int bs[256], ks[100];
  int ic = 0;
  get_starting_levels (&ed.model, 1, &bl, &bt, bs, &kl2, &kt, ks, &ic, &cfg);
  test_assert (kl2 == 2, "nesting carried to the next line counts 'begin;'");
  buffer_insert_line (&ed.model, 2, "/* ( ( (");
  buffer_insert_line (&ed.model, 3, "*/ x");
  get_starting_levels (&ed.model, 3, &bl, &bt, bs, &kl2, &kt, ks, &ic, &cfg);
  test_assert (ic == 1 && bl == 1,
               "the nesting pass also knows a comment is open");
  get_starting_levels (&ed.model, 4, &bl, &bt, bs, &kl2, &kt, ks, &ic, &cfg);
  test_assert (ic == 0 && bl == 1, "and that it closed");
  editor_cleanup (&ed);
}

void run_large_file_tests (void);

/* Per-line cost is what decides how big a file can be opened. */
void
run_large_file_tests (void)
{
  fprintf (stderr, "\n=== Large file tests ===\n");
  static EditorConfig cfg;
  set_default_config (&cfg);
  config_parse_syntax (&cfg);

  test_assert (sizeof (NestingCache) <= 32,
               "the per-line nesting cache holds depths, not stacks");

  /* A line starts small and still grows as it is typed into. */
  Buffer b;
  buffer_init (&b);
  buffer_insert_line (&b, 0, "ab");
  test_assert (b.lines[0]->buffer_size < 64, "a short line takes a small buffer");
  for (int i = 0; i < 5000; i++)
    buffer_insert_char (&b, 0, 1, 'x');
  test_assert (buffer_get_line_length (&b, 0) == 5002
               && buffer_get_char (&b, 0, 0) == 'a'
               && buffer_get_char (&b, 0, 5001) == 'b'
               && buffer_get_char (&b, 0, 2500) == 'x',
               "and grows to hold 5000 typed characters");
  buffer_insert_char (&b, 0, 2, '\n');
  test_assert (buffer_num_lines (&b) == 2 && buffer_get_line_length (&b, 0) == 2
               && buffer_get_line_length (&b, 1) == 5000
               && buffer_get_char (&b, 1, 4999) == 'b',
               "splitting a long line keeps both halves");
  buffer_free (&b);

  /* 100,000 lines open under the default 50 MB ceiling. */
  char path[] = "/tmp/led_big_XXXXXX";
  int fd = mkstemp (path);
  FILE *fp = fd >= 0 ? fdopen (fd, "w") : NULL;
  test_assert (fp != NULL, "temp file for the large load");
  if (fp)
    {
      for (int i = 0; i < 100000; i++)
        fprintf (fp, "  if (a[%d]) { f (b[%d]); } /* %d ( */\n", i, i, i);
      fclose (fp);
      buffer_init (&b);
      int rc = buffer_load_from_file (&b, path, 0, 0);
      test_assert (rc == BUFFER_LOAD_OK && buffer_num_lines (&b) == 100001,
                   "a 100,000-line file loads under the default limits");

      /* Nesting state deep in the file, reached three ways, agrees. */
      int bl, bt, kl, kt, ic;
      static int bs[256], ks[100];
      buffer_insert_line (&b, 50000, "{ ( [ { (");
      buffer_insert_line (&b, 50001, "/* open");
      get_starting_levels (&b, 50002, &bl, &bt, bs, &kl, &kt, ks, &ic, &cfg);
      test_assert (bt == 5 && bl == 6 && ic == 1 && bs[0] == 1 && bs[4] == 5,
                   "state at line 50,002 computed from the top");
      /* Cache one line just above, as a draw would, and ask again. */
      b.nesting_cache[50000].valid = 1;
      b.nesting_cache[50000].brace_level = 6;
      b.nesting_cache[50000].brace_top = 5;
      b.nesting_cache[50000].kw_level = 1;
      b.nesting_cache[50000].kw_top = 0;
      b.nesting_cache[50000].in_comment = 0;
      int bl2, bt2, kl2, kt2, ic2;
      static int bs2[256], ks2[100];
      get_starting_levels (&b, 50002, &bl2, &bt2, bs2, &kl2, &kt2, ks2, &ic2, &cfg);
      test_assert (bl2 == bl && bt2 == bt && kl2 == kl && kt2 == kt && ic2 == ic
                   && memcmp (bs, bs2, sizeof (int) * 5) == 0,
                   "the same state from the nearest cached line, stacks rebuilt");
      buffer_free (&b);
      unlink (path);
    }

  /* Deep nesting drawn with a warm cache has the colours of a cold one. */
  Editor ed;
  memset (&ed, 0, sizeof ed);
  buffer_init (&ed.model);
  ed.config = cfg;
  ed.config.display.tab_width = 8;
  ed.syntax_highlight = 1;
  buffer_insert_line (&ed.model, 0, "a(b[c{d(e[f");
  buffer_insert_line (&ed.model, 1, "]) } ] ) ) ) z(");
  const Frontend *before = frontend_current ();
  frontend_use (frontend_memory ());
  memory_reset (30, 4);
  screen_resize (30, 4);
  view_invalidate ();
  Pane *text = NULL;
  view_create_text_window_ex (screen_pane (), &text, 0);
  editor_draw (screen_pane (), text, &ed);          /* fills the cache */
  int warm[16], cold[16];
  for (int x = 0; x < 16; x++)
    warm[x] = memory_quality (x, 1);
  ed.model.nesting_cache[0].valid = 0;              /* forget, redraw */
  ed.model.edit_generation++;
  view_invalidate ();
  editor_draw (screen_pane (), text, &ed);
  for (int x = 0; x < 16; x++)
    cold[x] = memory_quality (x, 1);
  test_assert (memcmp (warm, cold, sizeof warm) == 0
               && warm[0] == LED_Q_LEVEL4 && warm[1] == LED_Q_LEVEL4
               && warm[3] == LED_Q_LEVEL3 && warm[7] == LED_Q_LEVEL1
               && warm[9] == LED_Q_LEVEL1 && warm[14] == LED_Q_LEVEL1,
               "closers take their opener's colour, cache warm or cold");
  pane_free (text);
  editor_cleanup (&ed);
  screen_resize (0, 0);
  view_invalidate ();
  frontend_use (before);
}
