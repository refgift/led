/* Fuzzing: random key sequences typed into the editor through the
 * memory front end. Each run checks two things that must always hold:
 *
 *   screen  after the keys, the screen built up one key at a time is
 *           the same as a fresh draw of the same text;
 *   undo    undoing every step gives back the starting text, and
 *           redoing as many steps as were undone gives back the edited
 *           text (the keys may have ended with an undo, so there can be
 *           redo history beyond it).
 *
 * The runs are seeded, so a failure prints the seed and the keys, and
 * the same seed repeats it. LED_FUZZ_RUNS sets how many runs each
 * language gets (default 150); LED_FUZZ_SEED sets the first seed. */
#include "test_helpers.h"
#include "editor.h"
#include "view.h"
#include "screen.h"
#include "config.h"
#include "controller.h"
#include "frontend/frontend.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void test_assert (int condition, const char *msg);
void run_fuzz_tests (void);

enum { COLS = 40, ROWS = 8, MAX_KEYS = 48 };

/* Keys a person types while editing. Prompts (search, replace, go to
 * line), save, quit and F4 (which main.c handles) are left out. */
static const int g_keys[] = {
  'a', 'x', 'Z', '_', '0', '7', ' ', ' ', ' ', '\t',
  '(', ')', '[', ']', '{', '}', ';', ',', '"', '\'',
  '/', '*', '#', '<', '\\', '$', '-',
  '\r', '\r', 0xC3, 0xA9,                     /* Enter, and é as UTF-8 */
  127, LED_KEY_BACKSPACE, LED_KEY_DC,
  LED_KEY_LEFT, LED_KEY_RIGHT, LED_KEY_UP, LED_KEY_DOWN,
  LED_KEY_HOME, LED_KEY_END, LED_KEY_NPAGE, LED_KEY_PPAGE,
  LED_KEY_WORD_LEFT, LED_KEY_WORD_RIGHT, LED_KEY_BTAB,
  11,                                         /* Ctrl+K comment */
  23,                                         /* Ctrl+W delete word */
  26, 25,                                     /* Ctrl+Z, Ctrl+Y */
  1, 3, 24, 22,                               /* Ctrl+A, C, X, V */
  29,                                         /* Ctrl+] */
  LED_KEY_F (2), LED_KEY_F (3)                /* line numbers, wrap */
};
#define NKEYS ((int) (sizeof g_keys / sizeof g_keys[0]))

/* A file per language, with the things that carry state from line to
 * line: comments, strings, here-documents, word pairs. */
typedef struct {
  const char *name;
  const char *lines[10];
} Sample;

static const Sample g_samples[] = {
  { "f.c", { "int f(int a) {", "  /* open (", "  still ) */ s = \"x {\\",
             "  y\"; // }", "  if (a) { return [a]; }", "}", NULL } },
  { "f.sh", { "f() {", "  cat <<EOF", "  } ( \"", "EOF", "  echo \"a (",
              "  b )\"; if [ $# ]; then x; fi", "}", NULL } },
  { "f.fst", { "BEGIN", "  IF X THEN", "    OUTPUT \"END (\";", "  BEGIN Y END",
               "END", NULL } },
  { "f.txt", { "Plain text, don't colour (this).", "",
               "\tA tab, and a long line that runs past the edge of a forty column screen.",
               NULL } },
};
#define NSAMPLES ((int) (sizeof g_samples / sizeof g_samples[0]))

static unsigned long g_rand;

static unsigned
next_rand (void)
{
  g_rand = g_rand * 6364136223846793005UL + 1442695040888963407UL;
  return (unsigned) (g_rand >> 33);
}

/* The whole buffer as one string, lines joined by '\n'. */
static char *
buffer_text (Buffer *b)
{
  size_t cap = 256, n = 0;
  char *out = malloc (cap);
  for (int i = 0; i < buffer_num_lines (b); i++)
    {
      char *l = buffer_get_line (b, i);
      size_t ln = l ? strlen (l) : 0;
      while (n + ln + 2 > cap)
        out = realloc (out, cap *= 2);
      if (ln)
        memcpy (out + n, l, ln);
      n += ln;
      out[n++] = '\n';
      free (l);
    }
  out[n] = '\0';
  return out;
}

/* Every cell of rows 0..ROWS-2: character and quality. A blank looks
 * the same whatever quality it was last shown with. */
static void
snapshot (int *out)
{
  for (int y = 0; y < ROWS - 1; y++)
    for (int x = 0; x < COLS; x++)
      out[y * COLS + x] = memory_char (x, y) == ' ' ? 0
        : memory_char (x, y) * 16 + memory_quality (x, y);
}

/* Print the rows that differ: characters, then each cell's quality. */
static void
print_rows (const int *typed, const int *fresh)
{
  for (int y = 0; y < ROWS - 1; y++)
    {
      if (memcmp (typed + y * COLS, fresh + y * COLS, sizeof (int) * COLS) == 0)
        continue;
      for (int pass = 0; pass < 2; pass++)
        {
          const int *g = pass ? fresh : typed;
          fprintf (stderr, "  row %d %s |", y, pass ? "fresh" : "typed");
          for (int x = 0; x < COLS; x++)
            {
              int ch = g[y * COLS + x] / 16;
              fputc (ch >= 32 && ch < 127 ? ch : ch ? '?' : ' ', stderr);
            }
          fprintf (stderr, "|\n  row %d %s |", y, pass ? "fresh" : "typed");
          for (int x = 0; x < COLS; x++)
            {
              int q = g[y * COLS + x] % 16;
              fputc (g[y * COLS + x] ? '0' + q : ' ', stderr);
            }
          fprintf (stderr, "|\n");
        }
    }
}

static void
print_keys (const char *what, const char *file, unsigned long seed,
            const int *keys, int n)
{
  fprintf (stderr, "FUZZ %s: %s seed %lu keys", what, file, seed);
  for (int k = 0; k < n; k++)
    fprintf (stderr, " %d", keys[k]);
  fprintf (stderr, "\n");
}

/* One run. Returns a bit mask of the properties that failed. */
static int
fuzz_once (const Sample *sm, unsigned long seed)
{
  enum { BAD_SCREEN = 1, BAD_UNDO = 2, BAD_REDO = 4 };
  static int typed[COLS * ROWS], fresh[COLS * ROWS];
  int keys[MAX_KEYS];
  g_rand = seed;
  int nkeys = 1 + (int) (next_rand () % MAX_KEYS);
  for (int k = 0; k < nkeys; k++)
    keys[k] = g_keys[next_rand () % NKEYS];

  Editor ed;
  memset (&ed, 0, sizeof ed);
  buffer_init (&ed.model);
  set_default_config (&ed.config);
  config_parse_syntax (&ed.config);
  ed.config.display.persistent_undo = 0;
  const Language *l = config_select_language (&ed.config, sm->name);
  ed.syntax_highlight = l && l->highlight;
  ed.show_line_numbers = ed.syntax_highlight;
  ed.auto_save_threshold = 1000000;
  for (int i = 0; sm->lines[i]; i++)
    buffer_insert_line (&ed.model, i, sm->lines[i]);
  char *start = buffer_text (&ed.model);

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
  int bad = 0;
  snapshot (typed);
  memory_reset (COLS, ROWS);
  screen_resize (COLS, ROWS);
  view_invalidate ();
  ed.model.edit_generation++;
  editor_draw (screen_pane (), text, &ed);
  snapshot (fresh);
  if (memcmp (typed, fresh, sizeof typed) != 0)
    {
      bad |= BAD_SCREEN;
      print_keys ("screen differs from a fresh draw", sm->name, seed, keys, nkeys);
      print_rows (typed, fresh);
    }

  /* Leave any selection, then undo everything and redo everything. */
  ed.selection_active = 0;
  char *edited = buffer_text (&ed.model);
  int undone_steps = 0;
  while (ed.undo_stack.count > 0 && undone_steps < 100000)
    {
      editor_handle_input (&ed, 26);
      undone_steps++;
    }
  char *undone = buffer_text (&ed.model);
  if (strcmp (undone, start) != 0)
    {
      bad |= BAD_UNDO;
      print_keys ("undo all did not restore the start", sm->name, seed, keys, nkeys);
    }
  for (int k = 0; k < undone_steps; k++)
    editor_handle_input (&ed, 25);
  char *redone = buffer_text (&ed.model);
  if (strcmp (redone, edited) != 0)
    {
      bad |= BAD_REDO;
      print_keys ("redo did not restore the edit", sm->name, seed, keys, nkeys);
    }

  free (start);
  free (edited);
  free (undone);
  free (redone);
  pane_free (text);
  editor_cleanup (&ed);
  screen_resize (0, 0);
  view_invalidate ();
  return bad;
}

void
run_fuzz_tests (void)
{
  fprintf (stderr, "\n=== Fuzz tests ===\n");
  const Frontend *before = frontend_current ();
  frontend_use (frontend_memory ());
  const char *env = getenv ("LED_FUZZ_RUNS");
  int runs = env ? atoi (env) : 150;
  if (runs < 1)
    runs = 1;
  env = getenv ("LED_FUZZ_SEED");
  unsigned long seed0 = env ? strtoul (env, NULL, 10) : 1;
  for (int s = 0; s < NSAMPLES; s++)
    {
      int screen_bad = 0, undo_bad = 0, redo_bad = 0;
      for (int r = 0; r < runs; r++)
        {
          int bad = fuzz_once (&g_samples[s], seed0 + (unsigned long) r);
          screen_bad += (bad & 1) != 0;
          undo_bad += (bad & 2) != 0;
          redo_bad += (bad & 4) != 0;
        }
      char msg[160];
      snprintf (msg, sizeof msg,
                "%s: %d random runs, the screen matches a fresh draw", g_samples[s].name, runs);
      test_assert (screen_bad == 0, msg);
      snprintf (msg, sizeof msg,
                "%s: %d random runs, undo all restores the start", g_samples[s].name, runs);
      test_assert (undo_bad == 0, msg);
      snprintf (msg, sizeof msg,
                "%s: %d random runs, redo restores the edit", g_samples[s].name, runs);
      test_assert (redo_bad == 0, msg);
    }
  frontend_use (before);
}
