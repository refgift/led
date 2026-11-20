/* The controller's three functions, driven as a script of
 * (level, action, value) with no keys involved. */
#include "test_controller.h"
#include "controller.h"
#include "editor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void test_assert (int condition, const char *msg);
void run_action_tests (void);

typedef struct {
  int (*level) (int action, int value);
  int action;
  int value;
} Step;

static void
run (const Step *steps, int n)
{
  for (int i = 0; i < n; i++)
    steps[i].level (steps[i].action, steps[i].value);
}

#define RUN(steps) run (steps, (int) (sizeof (steps) / sizeof (steps[0])))

static void
type (const char *text)
{
  for (const char *p = text; *p; p++)
    symbol (S_INSERT, (unsigned char) *p);
}

static void
feed (const char *text)
{
  for (const char *p = text; *p; p++)
    token (T_CHAR, (unsigned char) *p);
}

static int
line_is (Editor *ed, int line, const char *want)
{
  char *s = buffer_get_line (&ed->model, line);
  int ok = s && strcmp (s, want) == 0;
  free (s);
  return ok;
}

static void
editor_blank (Editor *ed)
{
  memset (ed, 0, sizeof *ed);
  buffer_init (&ed->model);
  buffer_insert_line (&ed->model, 0, "");
  ed->config.display.tab_width = 4;
  ed->config.search.enabled = 1;
}

void
run_action_tests (void)
{
  fprintf (stderr, "\n=== Action tests (symbol / macro / token) ===\n");
  controller_attach (NULL);
  test_assert (symbol (S_INSERT, 'x') == -1 && macro (M_ENTER, 1) == -1
               && token (T_BEGIN, K_GOTO) == -1,
               "with nothing attached the three functions refuse");

  Editor ed;
  editor_blank (&ed);
  controller_attach (&ed);

  /* symbol: one action each. */
  type ("abc");
  test_assert (line_is (&ed, 0, "abc") && ed.cursor_col == 3,
               "S_INSERT types at the cursor");
  symbol (S_LEFT, 2);
  test_assert (ed.cursor_col == 1, "a motion's value is a repeat count");
  symbol (S_DELETE, 1);
  test_assert (line_is (&ed, 0, "ac"), "S_DELETE removes the character ahead");
  symbol (S_UNDO, 1);
  test_assert (line_is (&ed, 0, "abc"), "S_UNDO reverses one symbol");
  symbol (S_REDO, 1);
  symbol (S_END, 0);
  symbol (S_BACKSPACE, 2);
  test_assert (line_is (&ed, 0, "") && ed.cursor_col == 0,
               "S_BACKSPACE with a count removes that many");
  symbol (S_NEWLINE, 0);
  test_assert (buffer_num_lines (&ed.model) == 2 && ed.cursor_line == 1,
               "S_NEWLINE splits the line and nothing more");
  symbol (S_UNDO, 4);
  test_assert (buffer_num_lines (&ed.model) == 1 && line_is (&ed, 0, "abc"),
               "S_UNDO with a count goes back that many steps");

  /* macro: several symbols, one undo step. */
  symbol (S_HOME, 0);
  symbol (S_DELETE, 3);
  type ("    if (x) {");
  int lines_before = buffer_num_lines (&ed.model);
  macro (M_ENTER, 1);
  test_assert (buffer_num_lines (&ed.model) == lines_before + 1
               && line_is (&ed, 1, "    \t") && ed.cursor_col == 5,
               "M_ENTER is a newline plus the indent, one level deeper after {");
  symbol (S_UNDO, 1);
  test_assert (buffer_num_lines (&ed.model) == lines_before
               && line_is (&ed, 0, "    if (x) {"),
               "one undo reverses the whole macro");
  symbol (S_REDO, 1);
  test_assert (line_is (&ed, 1, "    \t"), "one redo repeats the whole macro");
  symbol (S_UNDO, 1);

  ed.config.display.spaces_for_tab = 1;
  symbol (S_HOME, 0);
  macro (M_TAB, 1);
  test_assert (line_is (&ed, 0, "        if (x) {") && ed.cursor_col == 4,
               "M_TAB types spaces up to the tab stop");
  symbol (S_UNDO, 1);
  test_assert (line_is (&ed, 0, "    if (x) {"), "and undoes as one step");

  macro (M_COMMENT, 1);
  test_assert (line_is (&ed, 0, "    // if (x) {"), "M_COMMENT adds the comment");
  macro (M_COMMENT, 1);
  test_assert (line_is (&ed, 0, "    if (x) {"), "and takes it away again");

  symbol (S_END, 0);
  macro (M_DELETE_WORD, 2);
  test_assert (line_is (&ed, 0, "    "), "a macro's value is a repeat count");
  symbol (S_UNDO, 1);
  test_assert (line_is (&ed, 0, "    if (x) {"),
               "a repeated macro is still one undo step");

  symbol (S_HOME, 0);
  macro (M_CUT, 1);
  symbol (S_SELECT_ALL, 0);
  test_assert (ed.selection_active == 1, "S_SELECT_ALL selects");
  symbol (S_RIGHT, 1);
  test_assert (ed.selection_active == 0, "the next symbol ends the selection");
  macro (M_PASTE, 2);
  test_assert (line_is (&ed, 0, "    if (x) {    if (x) {"),
               "M_CUT then M_PASTE twice");
  symbol (S_UNDO, 1);
  test_assert (line_is (&ed, 0, ""), "the two pastes are one undo step");

  /* token: keys collected into a unit, then acted on. */
  editor_cleanup (&ed);
  editor_blank (&ed);
  controller_attach (&ed);
  buffer_insert_line (&ed.model, 1, "alpha beta");
  buffer_insert_line (&ed.model, 2, "gamma beta");

  token (T_BEGIN, K_GOTO);
  test_assert (ed.goto_mode == 1, "T_BEGIN opens the prompt");
  feed ("3:7x");
  test_assert (strcmp (ed.goto_buffer, "3:7") == 0,
               "the go-to token takes digits and ':' only");
  test_assert (ed.cursor_line == 0, "nothing happens while a token is collected");
  token (T_ERASE, 0);
  feed ("1");
  token (T_DONE, 0);
  test_assert (ed.goto_mode == 0 && ed.cursor_line == 2 && ed.cursor_col == 0,
               "T_DONE acts on the whole token and closes the prompt");

  token (T_BEGIN, K_GOTO);
  feed ("1");
  token (T_CANCEL, 0);
  test_assert (ed.goto_mode == 0 && ed.cursor_line == 2,
               "T_CANCEL closes the prompt without acting");

  symbol (S_TOP, 0);
  test_assert (ed.cursor_line == 0 && ed.cursor_col == 0, "S_TOP goes to the start");
  token (T_BEGIN, K_SEARCH);
  feed ("beta");
  token (T_DONE, 0);
  test_assert (ed.cursor_line == 1 && ed.cursor_col == 6 && ed.search_mode == 1,
               "the search token finds the pattern and stays open");
  token (T_DONE, 0);
  test_assert (ed.cursor_line == 2 && ed.cursor_col == 6,
               "T_DONE again finds the next match");
  token (T_CANCEL, 0);
  test_assert (ed.search_mode == 0, "T_CANCEL closes the search");

  token (T_BEGIN, K_REPLACE);
  feed ("beta");
  token (T_DONE, 0);
  test_assert (ed.replace_step == 2, "the replace token has two parts");
  feed ("delta");
  token (T_DONE, 0);
  test_assert (line_is (&ed, 1, "alpha delta") && line_is (&ed, 2, "gamma delta")
               && ed.replace_step == 0 && ed.file_modified == 1,
               "the second T_DONE replaces everywhere");

  symbol (S_END, 0);
  token (T_BEGIN, K_META);
  token (T_CHAR, 'b');
  test_assert (ed.cursor_col == 6 && ed.meta_pending == 0,
               "Esc then b is one token: back a word");

  /* A recorded macro is a list of steps, replayed by the same calls. */
  static const Step add_item[] = {
    { symbol, S_END, 0 },
    { macro, M_ENTER, 1 },
    { symbol, S_INSERT, '-' },
    { symbol, S_INSERT, ' ' },
    { symbol, S_INSERT, 'x' },
  };
  editor_cleanup (&ed);
  editor_blank (&ed);
  controller_attach (&ed);
  type ("list");
  RUN (add_item);
  RUN (add_item);
  RUN (add_item);
  test_assert (buffer_num_lines (&ed.model) == 4 && line_is (&ed, 0, "list")
               && line_is (&ed, 1, "- x") && line_is (&ed, 3, "- x"),
               "a recorded list of steps replays");

  /* Keys still arrive the same way and mean the same thing. */
  editor_handle_input (&ed, LED_KEY_HOME);
  editor_handle_input (&ed, 'A');
  test_assert (line_is (&ed, 3, "A- x"), "a key becomes a symbol");
  editor_handle_input (&ed, 7);
  editor_handle_input (&ed, '1');
  editor_handle_input (&ed, '\n');
  test_assert (ed.cursor_line == 0, "keys in a prompt become a token");

  controller_attach (NULL);
  editor_cleanup (&ed);
}
