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
  test_assert (c && c[4] == LED_Q_STRING && c[5] == LED_Q_STRING
               && c[8] == LED_Q_STRING && c[9] == LED_Q_STRING
               && c[13] == LED_Q_LEVEL1 && c[14] == LED_Q_NUMBER
               && c[15] == LED_Q_LEVEL1,
               "a string, quotes included, has the string colour and does not nest");
  free (c);
  c = colors_of (&cfg, "c = '('; n = 1'000; f(2);", NULL);
  test_assert (c && c[5] == LED_Q_STRING && c[13] == LED_Q_NUMBER
               && c[14] == LED_Q_NORMAL && c[21] == LED_Q_LEVEL1
               && c[23] == LED_Q_LEVEL1,
               "a character literal is skipped; a digit separator is not a quote");
  free (c);
  c = colors_of (&cfg, "a(1); // if (", &kl);
  test_assert (c && c[1] == LED_Q_LEVEL1 && c[6] == LED_Q_COMMENT
               && c[9] == LED_Q_COMMENT && c[12] == LED_Q_COMMENT && kl == 1,
               "a line comment has the comment colour and does not nest");
  free (c);
  c = colors_of (&cfg, "a /* ( if */ (b)", NULL);
  test_assert (c && c[2] == LED_Q_COMMENT && c[5] == LED_Q_COMMENT
               && c[7] == LED_Q_COMMENT && c[11] == LED_Q_COMMENT
               && c[12] == LED_Q_NORMAL
               && c[13] == LED_Q_LEVEL1 && c[15] == LED_Q_LEVEL1,
               "a block comment has the comment colour, and code after it is coloured");
  free (c);

  /* A C string goes on to the next line only after a backslash. */
  {
    int bl = 1, bt = 0, k = 1, kt = 0, ic = 0;
    static int bs[256], ks[100];
    int *s1 = compute_line_colors ("s = \"ab (\\", 10, 4, &cfg,
                                   &bl, &bt, bs, &k, &kt, ks, &ic);
    test_assert (s1 && ic == LED_OPEN_STRING + '"' && s1[4] == LED_Q_STRING
                 && s1[8] == LED_Q_STRING && bl == 1,
                 "a C string ending in a backslash goes on to the next line");
    int *s2 = compute_line_colors ("cd ) \"; f(1)", 13, 4, &cfg,
                                   &bl, &bt, bs, &k, &kt, ks, &ic);
    test_assert (s2 && ic == 0 && s2[0] == LED_Q_STRING && s2[3] == LED_Q_STRING
                 && s2[5] == LED_Q_STRING && s2[9] == LED_Q_LEVEL1 && bl == 1,
                 "the continued string closes, and code after it nests");
    free (s1);
    free (s2);
    ic = 0;
    int *s3 = compute_line_colors ("s = \"ab", 7, 4, &cfg,
                                   &bl, &bt, bs, &k, &kt, ks, &ic);
    int *s4 = compute_line_colors ("f(1)", 4, 4, &cfg,
                                   &bl, &bt, bs, &k, &kt, ks, &ic);
    test_assert (s3 && s4 && ic == 0 && s4[1] == LED_Q_LEVEL1,
                 "an unclosed C string without a backslash ends with its line");
    free (s3);
    free (s4);
  }

  /* A block comment left open carries to the next line. */
  {
    int bl = 1, bt = 0, k = 1, kt = 0, ic = 0;
    static int bs[256], ks[100];
    int *c1 = compute_line_colors ("x /* open (", 11, 4, &cfg,
                                   &bl, &bt, bs, &k, &kt, ks, &ic);
    test_assert (ic == 1 && bl == 1, "an open block comment is carried on");
    int *c2 = compute_line_colors ("( still ) */ f(1)", 17, 4, &cfg,
                                   &bl, &bt, bs, &k, &kt, ks, &ic);
    test_assert (c2 && ic == 0 && c2[0] == LED_Q_COMMENT && c2[8] == LED_Q_COMMENT
                 && c2[11] == LED_Q_COMMENT && c2[14] == LED_Q_LEVEL1 && bl == 1,
                 "the next line is comment until the comment closes");
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

#include <sys/stat.h>

void run_language_tests (void);

/* Colours of one line in the language now chosen, fresh nesting state. */
static int *
lang_colors (EditorConfig *cfg, const char *line, int *kw_level_out)
{
  return colors_of (cfg, line, kw_level_out);
}

/* A file's extension chooses its language: its words, its comments,
 * its strings. */
void
run_language_tests (void)
{
  fprintf (stderr, "\n=== Language tests ===\n");
  static EditorConfig cfg;
  set_default_config (&cfg);
  config_parse_syntax (&cfg);
  const Language *l;
  int *c;
  int kl = 0;

  l = config_select_language (&cfg, "prog.c");
  test_assert (l && strcmp (l->name, "c") == 0, ".c is C");
  l = config_select_language (&cfg, "dir.fst/Makefile");
  test_assert (l == NULL, "a file with no extension has no language");
  l = config_select_language (&cfg, "notes.txt");
  test_assert (l && strcmp (l->name, "text") == 0 && l->highlight == 0,
               ".txt is plain text, shown without colours");
  l = config_select_language (&cfg, "/some/dir/zda.fst");
  test_assert (l && strcmp (l->name, "first") == 0 && l->highlight == 1,
               ".fst is FIRST");

  /* FIRST: words nest, upper case, no comments. */
  c = lang_colors (&cfg, "\tIF ACCUMULATOR = 100 THEN JUMP S2;", &kl);
  test_assert (c && c[1] == LED_Q_LEVEL1 && c[2] == LED_Q_LEVEL1
               && c[4] == LED_Q_NORMAL && c[22] == LED_Q_LEVEL1
               && c[27] == LED_Q_RESERVED && c[32] == LED_Q_NORMAL
               && c[34] == LED_Q_SEMICOLON && kl == 1,
               "IF and THEN pair on one line; JUMP is reserved; names are plain");
  free (c);
  c = lang_colors (&cfg, "BEGIN BEGIN END", &kl);
  test_assert (c && c[0] == LED_Q_LEVEL1 && c[6] == LED_Q_LEVEL2
               && c[12] == LED_Q_LEVEL2 && kl == 2,
               "BEGIN opens a level and END takes the colour of its BEGIN");
  free (c);
  c = lang_colors (&cfg, "OUTPUT \"IF BEGIN (\"; OUTPUT NEWLINE", &kl);
  test_assert (c && c[0] == LED_Q_RESERVED && c[8] == LED_Q_STRING
               && c[11] == LED_Q_STRING && c[17] == LED_Q_STRING
               && c[19] == LED_Q_SEMICOLON && c[28] == LED_Q_RESERVED && kl == 1,
               "words inside a denotation are text, not code");
  free (c);
  c = lang_colors (&cfg, "OUTPUT \"a\\\"; IF", NULL);
  test_assert (c && c[11] == LED_Q_SEMICOLON && c[13] == LED_Q_LEVEL1,
               "FIRST has no escapes: a backslash does not hide the closing quote");
  free (c);
  c = lang_colors (&cfg, "A := B // C; IF /* D */ THEN", NULL);
  test_assert (c && c[13] == LED_Q_LEVEL1 && c[19] == LED_Q_NORMAL
               && c[24] == LED_Q_LEVEL1,
               "FIRST has no comments: // and /* are not special");
  free (c);
  c = lang_colors (&cfg, "if (x) return;", NULL);
  test_assert (c && c[0] == LED_Q_NORMAL && c[7] == LED_Q_NORMAL,
               "C's lower-case words mean nothing in FIRST");
  free (c);

  /* Back to C: its own rules again. */
  config_select_language (&cfg, "prog.c");
  c = lang_colors (&cfg, "if (x) return; // BEGIN", &kl);
  test_assert (c && c[0] == LED_Q_RESERVED && c[7] == LED_Q_RESERVED
               && c[18] == LED_Q_COMMENT && kl == 1,
               "choosing C again restores C's words and comments");
  free (c);

  /* In the editor: FIRST. */
  char dir[] = "/tmp/led_lang_XXXXXX";
  if (!mkdtemp (dir))
    {
      test_assert (0, "mkdtemp");
      return;
    }
  char sub[300], path[300], conf[320];
  snprintf (sub, sizeof sub, "%s/led", dir);
  mkdir (sub, 0700);
  config_set_prefix (dir);
  snprintf (path, sizeof path, "%s/p.fst", dir);
  FILE *fp = fopen (path, "w");
  fputs ("BEGIN\n\tOUTPUT \"hi\";\nEND\n", fp);
  fclose (fp);
  char *argv1[] = { "led", path };
  Editor ed;
  editor_init (&ed, 2, argv1);
  ed.auto_save_threshold = 100000;
  test_assert (ed.syntax_highlight == 1 && ed.show_line_numbers == 1,
               "a .fst file is highlighted and numbered like other source");
  editor_handle_input (&ed, LED_KEY_END);
  editor_handle_input (&ed, '\n');
  char *got = buffer_get_line (&ed.model, 1);
  test_assert (got && strcmp (got, "\t") == 0,
               "Enter after BEGIN indents one level");
  free (got);
  editor_handle_input (&ed, 26);
  editor_handle_input (&ed, 11);                /* Ctrl+K */
  got = buffer_get_line (&ed.model, 0);
  test_assert (got && strcmp (got, "BEGIN") == 0
               && strstr (ed.status_message, "no line comment") != NULL,
               "Ctrl+K in FIRST says there is no comment and changes nothing");
  free (got);
  editor_cleanup (&ed);

  /* Plain text: an apostrophe is not a quote. */
  snprintf (path, sizeof path, "%s/n.txt", dir);
  fp = fopen (path, "w");
  fputs ("don't (stop) now\n", fp);
  fclose (fp);
  char *argv2[] = { "led", path };
  editor_init (&ed, 2, argv2);
  test_assert (ed.syntax_highlight == 0 && ed.show_line_numbers == 0,
               "a .txt file is plain, without line numbers");
  ed.cursor_col = 6;
  editor_handle_input (&ed, 29);                /* Ctrl+] */
  test_assert (ed.cursor_col == 11,
               "in text, bracket matching is not thrown by an apostrophe");
  editor_cleanup (&ed);

  /* A language of the user's own, and a change to a built-in one. */
  snprintf (conf, sizeof conf, "%s/led/colorization.conf", dir);
  fp = fopen (conf, "w");
  fputs ("py.extensions=.py\npy.reserved_words=def,return\n"
         "py.line_comment=#\npy.block_comment=\npy.quotes=\"'\n"
         "first.reserved_words=BEGIN,END,OUTPUT,SHOUT\n", fp);
  fclose (fp);
  snprintf (path, sizeof path, "%s/s.py", dir);
  fp = fopen (path, "w");
  fputs ("def f(): # return (\n", fp);
  fclose (fp);
  char *argv3[] = { "led", path };
  editor_init (&ed, 2, argv3);
  ed.auto_save_threshold = 100000;
  c = lang_colors (&ed.config, "def f(): # return (", &kl);
  test_assert (ed.syntax_highlight == 1 && c && c[0] == LED_Q_RESERVED
               && c[5] == LED_Q_LEVEL1 && c[11] == LED_Q_COMMENT
               && c[18] == LED_Q_COMMENT,
               "a language defined in colorization.conf is used");
  free (c);
  editor_handle_input (&ed, 11);
  got = buffer_get_line (&ed.model, 0);
  test_assert (got && strncmp (got, "# def", 5) == 0,
               "Ctrl+K uses that language's comment mark");
  free (got);
  editor_handle_input (&ed, 11);
  got = buffer_get_line (&ed.model, 0);
  test_assert (got && strncmp (got, "def", 3) == 0, "and removes it again");
  free (got);
  config_select_language (&ed.config, "x.fst");
  c = lang_colors (&ed.config, "SHOUT JUMP", NULL);
  test_assert (c && c[0] == LED_Q_RESERVED && c[6] == LED_Q_NORMAL,
               "a built-in language's words can be replaced");
  free (c);
  editor_cleanup (&ed);

  config_set_prefix (NULL);
  char cmd[400];
  snprintf (cmd, sizeof cmd, "rm -rf '%s'", dir);
  if (system (cmd) != 0)
    fprintf (stderr, "cleanup failed: %s\n", dir);
}

void run_repaint_tests (void);
void run_shell_lex_tests (void);

/* The quality of every cell in rows 0..rows-1, to compare two draws. */
static void
snapshot (int *out, int cols, int rows)
{
  for (int y = 0; y < rows; y++)
    for (int x = 0; x < cols; x++)
      /* A blank cell looks the same whatever it was last shown with
         (the caret leaves its mark on blanks it has rested on). */
      out[y * cols + x] = memory_char (x, y) == ' ' ? 0
        : memory_char (x, y) * 16 + memory_quality (x, y);
}

/* Type keys into a file, drawing after each as the editor does, then
 * compare the screen with a fresh full draw of the same text. */
static int
typed_matches_fresh (const char *name, const char *lines[], int nlines,
                     const int *keys, int nkeys)
{
  enum { COLS = 40, ROWS = 8 };
  static int typed[COLS * ROWS], fresh[COLS * ROWS];
  Editor ed;
  memset (&ed, 0, sizeof ed);
  buffer_init (&ed.model);
  set_default_config (&ed.config);
  config_parse_syntax (&ed.config);
  const Language *l = config_select_language (&ed.config, name);
  ed.syntax_highlight = l && l->highlight;
  ed.auto_save_threshold = 100000;
  for (int i = 0; i < nlines; i++)
    buffer_insert_line (&ed.model, i, lines[i]);
  memory_reset (COLS, ROWS);
  screen_resize (COLS, ROWS);
  view_invalidate ();
  Pane *text = NULL;
  view_create_text_window_ex (screen_pane (), &text, 0);
  editor_draw (screen_pane (), text, &ed);
  for (int k = 0; k < nkeys; k++)
    {
      editor_handle_input (&ed, keys[k]);
      editor_draw (screen_pane (), text, &ed);
    }
  snapshot (typed, COLS, ROWS - 1);
  /* The same text, drawn from nothing. */
  memory_reset (COLS, ROWS);
  screen_resize (COLS, ROWS);
  view_invalidate ();
  ed.model.edit_generation++;
  editor_draw (screen_pane (), text, &ed);
  snapshot (fresh, COLS, ROWS - 1);
  pane_free (text);
  editor_cleanup (&ed);
  screen_resize (0, 0);
  view_invalidate ();
  return memcmp (typed, fresh, sizeof (int) * COLS * (ROWS - 1)) == 0;
}

/* An edit that changes nesting must recolour the lines below it. */
void
run_repaint_tests (void)
{
  fprintf (stderr, "\n=== Repaint tests ===\n");
  const Frontend *before = frontend_current ();
  frontend_use (frontend_memory ());
  const char *c_lines[] = { "", "  a(b);", "  c[d];", "  {e;}" };
  const char *f_lines[] = { "", "\tIF A THEN OUTPUT B;", "\tBEGIN", "\tEND" };
  const char *d_lines[] = { "if(x){;", "  a(b);", "  {e;}" };
  int k1[] = { 'i', 'f', '(', 'x', ')', '{' };
  int k2[] = { '/', '*' };
  int k3[] = { 'B', 'E', 'G', 'I', 'N' };
  int k4[] = { LED_KEY_END, LED_KEY_BACKSPACE, LED_KEY_BACKSPACE };
  int k5[] = { 'r', 'e', 't', 'u', 'r', 'n', ';' };
  int k6[] = { 'x', '=', '"', '(' };
  int k7[] = { 'f', '(', ')' };
  int k8[] = { '{', 26 };       /* type, then undo */
  test_assert (typed_matches_fresh ("a.c", c_lines, 4, k1, 6),
               "typing { recolours the brackets on the lines below");
  test_assert (typed_matches_fresh ("a.c", c_lines, 4, k2, 2),
               "opening a comment turns the lines below plain");
  test_assert (typed_matches_fresh ("a.fst", f_lines, 4, k3, 5),
               "completing the word BEGIN recolours the pairs below");
  test_assert (typed_matches_fresh ("a.c", d_lines, 3, k4, 3),
               "deleting a { recolours the lines below");
  test_assert (typed_matches_fresh ("a.c", c_lines, 4, k5, 7),
               "an edit that nests nothing leaves the lines below as they were");
  test_assert (typed_matches_fresh ("a.c", c_lines, 4, k6, 4),
               "a bracket typed inside a string changes nothing below");
  test_assert (typed_matches_fresh ("a.c", c_lines, 4, k7, 3),
               "an opened and closed bracket ends where it began");
  test_assert (typed_matches_fresh ("a.c", c_lines, 4, k8, 2),
               "undoing a { puts the colours below back");
  frontend_use (before);
}

void
run_shell_lex_tests (void)
{
  fprintf (stderr, "\n=== Shell and lex tests ===\n");
  static EditorConfig cfg;
  set_default_config (&cfg);
  config_parse_syntax (&cfg);
  const Language *l;
  int *c;
  int kl = 0;

  l = config_select_language (&cfg, "run.sh");
  test_assert (l && strcmp (l->name, "shell") == 0, ".sh is shell");
  l = config_select_language (&cfg, "run.bash");
  test_assert (l && strcmp (l->name, "shell") == 0, ".bash is shell");
  {
    int bl = 1, bt = 0, k = 1, kt = 0, ic = 0;
    static int bs[256], ks[100];
    int *m1 = compute_line_colors ("echo \"a (", 9, 4, &cfg,
                                   &bl, &bt, bs, &k, &kt, ks, &ic);
    int *m2 = compute_line_colors ("b ) if\" (x)", 11, 4, &cfg,
                                   &bl, &bt, bs, &k, &kt, ks, &ic);
    test_assert (m1 && m2 && m1[8] == LED_Q_STRING && m2[2] == LED_Q_STRING
                 && m2[4] == LED_Q_STRING && m2[6] == LED_Q_STRING
                 && m2[8] == LED_Q_LEVEL1 && ic == 0 && bl == 1,
                 "a shell string spans lines; a bracket inside it does not nest");
    free (m1);
    free (m2);
  }
  /* Here-documents: the body is text, the delimiter line ends it. */
  {
    const char *doc[] = { "cat <<'EOF' > out", "if ( x", "EOF", "f (1)" };
    int bl = 1, bt = 0, k = 1, kt = 0, ic = 0;
    static int bs[256], ks[100];
    int *h[4];
    int state[4];
    for (int n = 0; n < 4; n++)
      {
        h[n] = compute_line_colors (doc[n], (int) strlen (doc[n]), 4, &cfg,
                                    &bl, &bt, bs, &k, &kt, ks, &ic);
        state[n] = ic;
      }
    test_assert (h[0] && (state[0] & LED_OPEN_HEREDOC),
                 "<<'EOF' opens a here-document on the next line");
    test_assert (h[1] && h[1][0] == LED_Q_STRING && h[1][3] == LED_Q_STRING
                 && (state[1] & LED_OPEN_HEREDOC) && bl == 1,
                 "a here-document line is text: no keyword, no nesting");
    test_assert (state[2] == 0, "the delimiter line ends the here-document");
    test_assert (h[3] && h[3][2] == LED_Q_LEVEL1 && state[3] == 0,
                 "code after the here-document nests again");
    for (int n = 0; n < 4; n++)
      free (h[n]);

    ic = 0;
    int *t1 = compute_line_colors ("\tcat <<-END", 11, 4, &cfg,
                                   &bl, &bt, bs, &k, &kt, ks, &ic);
    int *t2 = compute_line_colors ("\t\tEND", 5, 4, &cfg,
                                   &bl, &bt, bs, &k, &kt, ks, &ic);
    test_assert (t1 && t2 && ic == 0,
                 "<<- ends at a delimiter indented with tabs");
    free (t1);
    free (t2);

    ic = 0;
    int *a1 = compute_line_colors ("x=$((1<<2)); y=$(cat <<< hi)", 28, 4, &cfg,
                                   &bl, &bt, bs, &k, &kt, ks, &ic);
    test_assert (a1 && ic == 0,
                 "a shift in $(( )) and a <<< here-string are not here-documents");
    free (a1);
  }
  c = colors_of (&cfg, "if [ $# -gt 0 ];then echo \"x\";fi", &kl);
  test_assert (c && c[0] == LED_Q_LEVEL1 && c[5] == LED_Q_NORMAL
               && c[6] == LED_Q_NORMAL && c[9] == LED_Q_NORMAL
               && c[16] == LED_Q_RESERVED && c[21] == LED_Q_RESERVED
               && c[27] == LED_Q_STRING && c[30] == LED_Q_LEVEL1 && kl == 1,
               "if ... fi pair; $# is not a comment; then and echo are reserved");
  free (c);
  c = colors_of (&cfg, "for f in *;do echo $f;done # if (", &kl);
  test_assert (c && c[0] == LED_Q_RESERVED && c[11] == LED_Q_LEVEL1
               && c[22] == LED_Q_LEVEL1 && c[29] == LED_Q_COMMENT
               && c[32] == LED_Q_COMMENT && kl == 1,
               "do ... done pair; # after a space starts a comment");
  free (c);
  c = colors_of (&cfg, "echo ${x#y} \\\" \\( done", &kl);
  test_assert (c && c[8] == LED_Q_NORMAL && c[9] == LED_Q_NORMAL
               && c[13] == LED_Q_NORMAL && c[16] == LED_Q_NORMAL
               && c[18] == LED_Q_LEVEL1,
               "${x#y} is not a comment; an escaped quote or bracket is not code");
  free (c);

  /* A script with no extension says what it is on its first line. */
  l = config_select_by_first_line (&cfg, "#!/bin/sh");
  test_assert (l && strcmp (l->name, "shell") == 0, "#!/bin/sh is shell");
  l = config_select_by_first_line (&cfg, "#!/usr/bin/env bash");
  test_assert (l && strcmp (l->name, "shell") == 0, "#!/usr/bin/env bash is shell");
  l = config_select_by_first_line (&cfg, "#! /bin/bash -e");
  test_assert (l && strcmp (l->name, "shell") == 0, "options after the name are ignored");
  test_assert (config_select_by_first_line (&cfg, "#!/usr/bin/perl") == NULL
               && config_select_by_first_line (&cfg, "# not a shebang") == NULL
               && config_select_by_first_line (&cfg, "") == NULL,
               "an unknown interpreter, or no shebang, chooses nothing");

  /* lex: C with flex's names; a backslash hides the next character. */
  l = config_select_language (&cfg, "first.l");
  test_assert (l && strcmp (l->name, "lex") == 0, ".l is lex");
  c = colors_of (&cfg, "{BG}*OUTPUT{BG}+\\\".+\\\"\t\t{", &kl);
  int n = (int) strlen ("{BG}*OUTPUT{BG}+\\\".+\\\"\t\t{");
  test_assert (c && c[0] == LED_Q_LEVEL1 && c[3] == LED_Q_LEVEL1
               && c[n - 1] == LED_Q_LEVEL1,
               "an escaped quote in a pattern does not swallow the action's brace");
  free (c);
  c = colors_of (&cfg, "\tprintf(\"%s\", yytext); return 0; ECHO;", &kl);
  test_assert (c && c[14] == LED_Q_RESERVED && c[19] == LED_Q_RESERVED
               && c[23] == LED_Q_RESERVED && c[33] == LED_Q_RESERVED
               && c[10] == LED_Q_STRING,
               "yytext, return and ECHO are reserved; the string has the string colour");
  free (c);

  /* In the editor: a shell script with no extension, and Enter after do. */
  char dir[] = "/tmp/led_sh_XXXXXX";
  if (!mkdtemp (dir))
    {
      test_assert (0, "mkdtemp");
      return;
    }
  char sub[300], path[300];
  snprintf (sub, sizeof sub, "%s/led", dir);
  mkdir (sub, 0700);
  config_set_prefix (dir);
  snprintf (path, sizeof path, "%s/deploy", dir);
  FILE *fp = fopen (path, "w");
  fputs ("#!/bin/sh\nfor f in *; do\n", fp);
  fclose (fp);
  char *argv[] = { "led", path };
  Editor ed;
  editor_init (&ed, 2, argv);
  ed.auto_save_threshold = 100000;
  test_assert (ed.syntax_highlight == 1
               && strcmp (config_language (&ed.config)->name, "shell") == 0,
               "a script without an extension is recognised by its first line");
  ed.cursor_line = 1;
  editor_handle_input (&ed, LED_KEY_END);
  editor_handle_input (&ed, '\n');
  char *got = buffer_get_line (&ed.model, 2);
  test_assert (got && strcmp (got, "\t") == 0, "Enter after do indents one level");
  free (got);
  editor_handle_input (&ed, 11);                /* Ctrl+K */
  got = buffer_get_line (&ed.model, 2);
  test_assert (got && strcmp (got, "\t# ") == 0, "Ctrl+K in shell inserts #");
  free (got);
  editor_cleanup (&ed);
  config_set_prefix (NULL);
  char cmd[400];
  snprintf (cmd, sizeof cmd, "rm -rf '%s'", dir);
  if (system (cmd) != 0)
    fprintf (stderr, "cleanup failed: %s\n", dir);
}
