/* Data-safety regressions: undo after cut / paste / replace, the undo
 * cap, selection clipboard, UTF-8 keys, and loads that must not be
 * written back. */
#include "test_helpers.h"
#include "test_controller.h"
#include "controller.h"
#include "editor.h"
#include "utils/utils.h"
#include "frontend/frontend.h"
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdint.h>
#include "utils.h"

void test_assert (int condition, const char *msg);
void run_safety_tests (void);

static int
line_is (Buffer *buf, int line, const char *want)
{
  char *s = buffer_get_line (buf, line);
  int ok = s && strcmp (s, want) == 0;
  free (s);
  return ok;
}

static void
write_file (const char *path, const char *text)
{
  FILE *fp = fopen (path, "wb");
  if (fp)
    {
      fputs (text, fp);
      fclose (fp);
    }
}

static int
file_is (const char *path, const char *want)
{
  char buf[256];
  FILE *fp = fopen (path, "rb");
  if (!fp)
    return 0;
  size_t n = fread (buf, 1, sizeof buf - 1, fp);
  fclose (fp);
  buf[n] = '\0';
  return strcmp (buf, want) == 0;
}

static void
test_cut_paste_undo (void)
{
  TestContext t;
  test_init (&t);
  buffer_insert_line (&t.buf, 0, "hello world");
  t.cursor_col = 5;
  test_handle_input (&t, 24);   /* Ctrl+X */
  test_assert (line_is (&t.buf, 0, "hello") && t.clipboard
               && strcmp (t.clipboard, " world") == 0,
               "cut takes the rest of the line");
  test_handle_input (&t, 26);   /* Ctrl+Z */
  test_assert (line_is (&t.buf, 0, "hello world")
               && buffer_get_line_length (&t.buf, 0) == 11,
               "undo after cut restores the text, no NUL byte");
  test_handle_input (&t, 25);   /* Ctrl+Y */
  test_assert (line_is (&t.buf, 0, "hello"), "redo repeats the cut");

  t.cursor_col = 0;
  test_handle_input (&t, 22);   /* Ctrl+V */
  test_assert (line_is (&t.buf, 0, " worldhello") && t.cursor_col == 6,
               "paste inserts at the cursor");
  test_handle_input (&t, 26);
  test_assert (line_is (&t.buf, 0, "hello"), "undo removes the paste");
  test_handle_input (&t, 26);
  test_assert (line_is (&t.buf, 0, "hello world"),
               "undo before the paste still lands in the right place");
  test_assert (buffer_insert_char (&t.buf, 0, 0, '\0') == -1
               && buffer_get_line_length (&t.buf, 0) == 11,
               "the buffer refuses a NUL byte");
  test_cleanup (&t);
}

static void
test_selection_ops (void)
{
  TestContext t;
  test_init (&t);
  buffer_insert_line (&t.buf, 0, "one");
  buffer_insert_line (&t.buf, 1, "two");
  buffer_insert_line (&t.buf, 2, "three");
  test_handle_input (&t, 1);    /* Ctrl+A */
  test_handle_input (&t, 3);    /* Ctrl+C */
  test_assert (t.clipboard && strcmp (t.clipboard, "one\ntwo\nthree") == 0
               && t.ed.selection_active == 1,
               "copy after select-all copies every line");
  test_handle_input (&t, 24);   /* Ctrl+X */
  test_assert (buffer_num_lines (&t.buf) == 1 && line_is (&t.buf, 0, "")
               && t.cursor_line == 0 && t.cursor_col == 0
               && t.ed.selection_active == 0,
               "cut after select-all empties the buffer");
  test_handle_input (&t, 26);
  test_assert (buffer_num_lines (&t.buf) == 3 && line_is (&t.buf, 0, "one")
               && line_is (&t.buf, 1, "two") && line_is (&t.buf, 2, "three"),
               "undo restores a multi-line cut");
  t.cursor_line = 2;
  t.cursor_col = 5;
  test_handle_input (&t, 22);
  test_assert (buffer_num_lines (&t.buf) == 5 && line_is (&t.buf, 2, "threeone")
               && line_is (&t.buf, 4, "three")
               && t.cursor_line == 4 && t.cursor_col == 5,
               "multi-line paste leaves the cursor after the text");
  test_handle_input (&t, 26);
  test_assert (buffer_num_lines (&t.buf) == 3 && line_is (&t.buf, 2, "three"),
               "undo removes a multi-line paste");

  test_handle_input (&t, 1);
  test_handle_input (&t, LED_KEY_LEFT);
  test_assert (t.ed.selection_active == 0, "a cursor key drops the selection");
  test_handle_input (&t, 1);
  test_handle_input (&t, LED_KEY_BACKSPACE);
  test_assert (buffer_num_lines (&t.buf) == 1 && line_is (&t.buf, 0, ""),
               "Backspace deletes the selection");
  test_handle_input (&t, 26);
  test_assert (buffer_num_lines (&t.buf) == 3 && line_is (&t.buf, 1, "two"),
               "and undo brings it back");
  test_cleanup (&t);
}

static void
test_undo_cap (void)
{
  TestContext t;
  test_init (&t);
  buffer_insert_line (&t.buf, 0, "");
  UndoStack *u = &t.ed.undo_stack;
  /* Fill the stack with real edits, each on its own short line so the
   * work stays cheap. */
  int pushed = 0;
  while (u->capacity < 1000000 || u->count < u->capacity)
    {
      push_undo (u, true, 0, 0, 'x');
      pushed++;
    }
  int before = u->count;
  free_undo_stacks (u, &t.ed.redo_stack);
  /* Rebuild a full stack whose last entries are real. */
  for (int i = 0; i < before - 3; i++)
    push_undo (u, true, 0, 0, 'x');
  test_handle_input (&t, 'a');
  test_handle_input (&t, 'b');
  test_handle_input (&t, 'c');
  test_assert (u->count == before, "undo stack is full");
  test_handle_input (&t, 'd');
  test_assert (u->count < before && u->count > 0
               && u->changes[u->count - 1].ch == 'd',
               "a full undo stack drops old changes, not the new one");
  test_handle_input (&t, 26);
  test_assert (line_is (&t.buf, 0, "abc"),
               "undo on a full stack undoes the latest edit");
  test_cleanup (&t);
}

static void
test_utf8_keys (void)
{
  TestContext t;
  test_init (&t);
  /* "a" + U+0436 (2 bytes) + U+20AC (3 bytes) + "b" */
  buffer_insert_line (&t.buf, 0, "a\xD0\xB6\xE2\x82\xAC" "b");
  buffer_insert_line (&t.buf, 1, "abc");
  test_handle_input (&t, LED_KEY_RIGHT);
  test_handle_input (&t, LED_KEY_RIGHT);
  test_assert (t.cursor_col == 3, "Right steps over a 2-byte character");
  test_handle_input (&t, LED_KEY_RIGHT);
  test_assert (t.cursor_col == 6, "Right steps over a 3-byte character");
  test_handle_input (&t, LED_KEY_LEFT);
  test_assert (t.cursor_col == 3, "Left steps back over it");
  test_handle_input (&t, LED_KEY_DC);
  test_assert (line_is (&t.buf, 0, "a\xD0\xB6" "b"),
               "Delete removes the whole character");
  test_handle_input (&t, 26);
  test_assert (line_is (&t.buf, 0, "a\xD0\xB6\xE2\x82\xAC" "b"),
               "one undo restores the whole character");
  t.cursor_col = 3;
  test_handle_input (&t, LED_KEY_BACKSPACE);
  test_assert (line_is (&t.buf, 0, "a\xE2\x82\xAC" "b") && t.cursor_col == 1,
               "Backspace removes the whole character");
  test_handle_input (&t, 26);
  test_assert (line_is (&t.buf, 0, "a\xD0\xB6\xE2\x82\xAC" "b"),
               "one undo restores it");
  test_handle_input (&t, 25);
  test_assert (line_is (&t.buf, 0, "a\xE2\x82\xAC" "b"),
               "one redo removes it again");
  test_handle_input (&t, 26);
  /* Type a 2-byte character as the terminal sends it: two bytes. */
  t.cursor_col = 0;
  test_handle_input (&t, 0xD0);
  test_handle_input (&t, 0xB6);
  test_handle_input (&t, 26);
  test_assert (line_is (&t.buf, 0, "a\xD0\xB6\xE2\x82\xAC" "b"),
               "one undo removes a typed multi-byte character");
  t.cursor_line = 1;
  t.cursor_col = 2;
  test_handle_input (&t, LED_KEY_UP);
  test_assert (t.cursor_col == 1, "Up does not land inside a character");
  test_cleanup (&t);
}

static void
test_replace_all_fixes (void)
{
  Buffer b;
  buffer_init (&b);
  buffer_insert_line (&b, 0, "aaa");
  buffer_insert_line (&b, 1, "bbb");
  test_assert (buffer_replace_all (&b, "^a", "X") == 1 && line_is (&b, 0, "Xaa"),
               "^ matches once per line in replace");
  test_assert (buffer_replace_all (&b, "zzz", "y") == 0,
               "replace with no match reports zero");
  test_assert (buffer_replace_all (&b, "(", "y") == -1,
               "a bad pattern is reported");
  buffer_free (&b);

  Editor ed;
  memset (&ed, 0, sizeof ed);
  buffer_init (&ed.model);
  buffer_insert_line (&ed.model, 0, "cat cat");
  ed.config.search.enabled = 1;
  ed.auto_save_threshold = 1000;
  editor_handle_input (&ed, 'x');
  ed.file_modified = 0;
  editor_handle_input (&ed, 18);        /* Ctrl+R */
  editor_handle_input (&ed, 'c');
  editor_handle_input (&ed, '\n');
  editor_handle_input (&ed, 'd');
  editor_handle_input (&ed, '\n');
  test_assert (line_is (&ed.model, 0, "xdat dat") && ed.file_modified == 1,
               "replace-all marks the buffer modified");
  test_assert (ed.undo_stack.count > 0,
               "replace-all is recorded for undo");
  editor_handle_input (&ed, 26);        /* Ctrl+Z: one step takes it all back */
  test_assert (line_is (&ed.model, 0, "xcat cat"),
               "one undo restores every replaced line");
  editor_handle_input (&ed, 25);        /* Ctrl+Y */
  test_assert (line_is (&ed.model, 0, "xdat dat"),
               "redo repeats replace-all");
  /* A replacement with multi-byte text undoes to the exact old bytes. */
  Editor ue;
  memset (&ue, 0, sizeof ue);
  buffer_init (&ue.model);
  buffer_insert_line (&ue.model, 0, "a b a");
  ue.config.search.enabled = 1;
  ue.auto_save_threshold = 1000;
  editor_handle_input (&ue, 18);        /* Ctrl+R */
  editor_handle_input (&ue, 'a');
  editor_handle_input (&ue, '\n');
  for (const char *p = "\xc3\xa9"; *p; p++)
    editor_handle_input (&ue, (unsigned char) *p);
  editor_handle_input (&ue, '\n');
  test_assert (line_is (&ue.model, 0, "\xc3\xa9 b \xc3\xa9"),
               "replace-all with a multi-byte replacement");
  editor_handle_input (&ue, 26);
  test_assert (line_is (&ue.model, 0, "a b a"),
               "undo restores the exact bytes of a multi-byte replace");

  ed.file_modified = 0;
  editor_handle_input (&ed, 3);         /* Ctrl+C */
  editor_handle_input (&ed, LED_KEY_RIGHT);
  test_assert (ed.file_modified == 0, "copy and cursor keys do not mark modified");
  editor_handle_input (&ed, 'q');
  ed.file_modified = 0;
  editor_handle_input (&ed, 26);
  test_assert (ed.file_modified == 1, "undo marks the buffer modified");
  editor_cleanup (&ed);
}

static void
test_blocked_saves (void)
{
  char dir[] = "/tmp/led_safety_XXXXXX";
  if (!mkdtemp (dir))
    {
      test_assert (0, "mkdtemp");
      return;
    }
  char conf_home[300], file[300], link_path[300], rec[320];
  snprintf (conf_home, sizeof conf_home, "%s/home", dir);
  mkdir (conf_home, 0700);
  char *old_home = getenv ("HOME") ? strdup (getenv ("HOME")) : NULL;
  setenv ("HOME", conf_home, 1);

  /* A file with a NUL byte is refused; nothing may overwrite it. */
  snprintf (file, sizeof file, "%s/bin.dat", dir);
  FILE *fp = fopen (file, "wb");
  fwrite ("ab\0cd", 1, 5, fp);
  fclose (fp);
  char *argv1[] = { "led", file };
  Editor ed;
  editor_init (&ed, 2, argv1);
  test_assert (ed.save_blocked == 1, "a refused load blocks saving");
  ed.auto_save_threshold = 2;
  editor_handle_input (&ed, 'x');
  editor_handle_input (&ed, 'y');
  editor_handle_input (&ed, 'z');
  editor_handle_input (&ed, 19);        /* Ctrl+S */
  struct stat st;
  test_assert (stat (file, &st) == 0 && st.st_size == 5,
               "autosave and Ctrl+S leave a refused file untouched");
  editor_cleanup (&ed);

  /* A directory is not a text file. */
  char *argv2[] = { "led", dir };
  editor_init (&ed, 2, argv2);
  test_assert (ed.save_blocked == 1, "a directory is not opened for writing");
  editor_cleanup (&ed);

  /* A name that does not exist is a new file and saves normally. */
  snprintf (file, sizeof file, "%s/new.txt", dir);
  char *argv3[] = { "led", file };
  editor_init (&ed, 2, argv3);
  ed.auto_save_threshold = 1000;
  editor_handle_input (&ed, 'h');
  editor_handle_input (&ed, 'i');
  editor_handle_input (&ed, 19);
  test_assert (ed.save_blocked == 0 && file_is (file, "hi"),
               "a new file saves");

  /* Saving through a symlink keeps the link and updates its target. */
  snprintf (link_path, sizeof link_path, "%s/link.txt", dir);
  test_assert (symlink (file, link_path) == 0, "symlink created");
  editor_cleanup (&ed);
  char *argv4[] = { "led", link_path };
  editor_init (&ed, 2, argv4);
  ed.auto_save_threshold = 1000;
  editor_handle_input (&ed, '!');
  editor_handle_input (&ed, 19);
  test_assert (lstat (link_path, &st) == 0 && S_ISLNK (st.st_mode)
               && file_is (file, "!hi"),
               "save through a symlink keeps the link");

  /* Recovery file: written on demand, loaded as modified, then removed. */
  editor_handle_input (&ed, '?');
  snprintf (rec, sizeof rec, "%s.recovery", link_path);
  test_assert (editor_write_recovery (&ed) == 0 && file_is (rec, "!?hi"),
               "unsaved work goes to a recovery file");
  editor_cleanup (&ed);
  editor_init (&ed, 2, argv4);
  test_assert (line_is (&ed.model, 0, "!?hi") && ed.file_modified == 1
               && access (rec, F_OK) != 0 && file_is (file, "!hi"),
               "recovered text loads as modified and the file is untouched");
  ed.auto_save_timeout = 1;
  ed.auto_save_threshold = 1000;
  ed.file_modified = 0;
  ed.last_save_time = time (NULL) - 100;
  editor_handle_input (&ed, LED_KEY_RIGHT);
  test_assert (file_is (file, "!hi"),
               "the autosave timer does not write an unmodified buffer");
  editor_cleanup (&ed);

  /* A line far past max_line_length is kept whole, edited, and saved. */
  snprintf (file, sizeof file, "%s/long.txt", dir);
  {
    FILE *lf = fopen (file, "wb");
    for (int i = 0; i < 30000; i++)
      fputc ('a' + i % 26, lf);
    fputs ("\nsecond\n", lf);
    fclose (lf);
  }
  char *argv5[] = { "led", file };
  editor_init (&ed, 2, argv5);
  ed.auto_save_threshold = 1000;
  test_assert (ed.save_blocked == 0
               && buffer_get_line_length (&ed.model, 0) == 30000
               && buffer_get_char (&ed.model, 0, 29999) == 'a' + 29999 % 26,
               "a 30,000-character line loads whole and can be saved");
  editor_handle_input (&ed, LED_KEY_END);
  editor_handle_input (&ed, '!');
  editor_handle_input (&ed, 19);
  {
    struct stat lst;
    FILE *lf = fopen (file, "rb");
    int last = 0;
    if (lf)
      {
        fseek (lf, 30000, SEEK_SET);
        last = fgetc (lf);
        fclose (lf);
      }
    test_assert (stat (file, &lst) == 0 && lst.st_size == 30000 + 1 + 8
                 && last == '!',
                 "an edit at the end of the long line is saved, nothing lost");
  }
  editor_cleanup (&ed);

  /* A path longer than 255 bytes is an ordinary path. */
  {
    char deep[1200];
    int n = snprintf (deep, sizeof deep, "%s", dir);
    for (int level = 0; level < 4; level++)
      {
        n += snprintf (deep + n, sizeof deep - (size_t) n, "/%s",
                       "a_directory_name_that_is_eighty_characters_long_to_make_the_whole_path_long_0123");
        mkdir (deep, 0700);
      }
    snprintf (deep + n, sizeof deep - (size_t) n, "/file.txt");
    write_file (deep, "deep");
    test_assert (strlen (deep) > 255 && is_filename_safe (deep),
                 "a path over 255 bytes is accepted");
    char *argv6[] = { "led", deep };
    editor_init (&ed, 2, argv6);
    ed.auto_save_threshold = 2;
    test_assert (ed.filename != NULL && line_is (&ed.model, 0, "deep"),
                 "and opens");
    editor_handle_input (&ed, 'x');
    editor_handle_input (&ed, 'y');     /* autosave fires */
    char bak[1300];
    snprintf (bak, sizeof bak, "%s.bak.1", deep);
    test_assert (file_is (deep, "xydeep") && file_is (bak, "xydeep"),
                 "it autosaves, and the backup has the whole name");
    editor_handle_input (&ed, 'z');
    snprintf (bak, sizeof bak, "%s.recovery", deep);
    test_assert (editor_write_recovery (&ed) == 0 && file_is (bak, "xyzdeep"),
                 "its recovery file has the whole name too");
    unlink (bak);
    editor_cleanup (&ed);
  }

  /* Names that are refused say so instead of opening nothing in silence. */
  {
    char *argv7[] = { "led", "bad\033[2Jname.txt" };
    editor_init (&ed, 2, argv7);
    test_assert (ed.filename == NULL
                 && strstr (ed.status_message, "not usable") != NULL,
                 "a name with a control character is refused, with a message");
    editor_cleanup (&ed);
    static char huge[PATH_MAX + 100];
    memset (huge, 'a', sizeof huge - 1);
    huge[sizeof huge - 1] = '\0';
    test_assert (!is_filename_safe (huge), "a path past the system limit is refused");
    huge[LED_PATH_LIMIT] = '\0';
    test_assert (is_filename_safe (huge), "a path at led's limit is accepted");
  }

  char cmd[400];
  snprintf (cmd, sizeof cmd, "rm -rf '%s'", dir);
  if (system (cmd) != 0)
    fprintf (stderr, "cleanup failed: %s\n", dir);
  if (old_home)
    {
      setenv ("HOME", old_home, 1);
      free (old_home);
    }
  else
    unsetenv ("HOME");
}

/* led.conf and colorization.conf: written once with every default as
 * a comment, read from the --prefix directory, led.conf read last. */
static void
test_config_files (void)
{
  char dir[] = "/tmp/led_conf_XXXXXX";
  if (!mkdtemp (dir))
    {
      test_assert (0, "mkdtemp");
      return;
    }
  char general[300], colors[300], cmd[800];
  snprintf (general, sizeof general, "%s/led/led.conf", dir);
  snprintf (colors, sizeof colors, "%s/led/colorization.conf", dir);
  config_set_prefix (dir);

  test_assert (config_write_defaults () == 2, "first run writes both config files");
  test_assert (access (general, R_OK) == 0 && access (colors, R_OK) == 0,
               "led.conf and colorization.conf are under the prefix");
  test_assert (config_write_defaults () == 0, "existing config files are left alone");

  EditorConfig *def = malloc (sizeof *def);
  EditorConfig *cfg = malloc (sizeof *cfg);
  set_default_config (def);
  load_editor_config (cfg);
  test_assert (cfg->display.tab_width == def->display.tab_width
               && cfg->colors.reserved_words_fg == def->colors.reserved_words_fg
               && cfg->display.line_numbers_configured == 0
               && strcmp (cfg->syntax.reserved_words, def->syntax.reserved_words) == 0,
               "the written files change nothing: every line is a comment");

  /* Take the '#' off every setting: the files must say what the
   * defaults are, the long reserved_words line included. */
  snprintf (cmd, sizeof cmd, "sed -i 's/^#\\([a-z0-9_]*=\\)/\\1/' '%s' '%s'",
            general, colors);
  test_assert (system (cmd) == 0, "uncomment the written defaults");
  load_editor_config (cfg);
  test_assert (strcmp (cfg->syntax.reserved_words, def->syntax.reserved_words) == 0
               && strcmp (cfg->syntax.extensions, def->syntax.extensions) == 0
               && strcmp (cfg->syntax.paired_keywords, def->syntax.paired_keywords) == 0,
               "uncommented syntax lines read back as the defaults");
  test_assert (memcmp (&cfg->colors, &def->colors, sizeof def->colors) == 0,
               "uncommented colours read back as the defaults");
  test_assert (cfg->display.tab_width == def->display.tab_width
               && cfg->performance.max_line_length == def->performance.max_line_length
               && cfg->autosave.keystrokes == def->autosave.keystrokes
               && cfg->search.max_pattern_length == def->search.max_pattern_length
               && cfg->display.line_numbers_configured == 1,
               "uncommented general lines read back as the defaults");

  /* An old single file still sets general keys; led.conf wins a tie. */
  write_file (colors, "tab_width=3\nmax_line_length=500\nnormal_fg=CYAN\n");
  write_file (general, "tab_width=5\n");
  load_editor_config (cfg);
  test_assert (cfg->performance.max_line_length == 500
               && cfg->colors.normal_fg == LED_COLOR_CYAN,
               "colorization.conf may still hold any key");
  test_assert (cfg->display.tab_width == 5, "led.conf is read last and wins");

  free (def);
  free (cfg);
  config_set_prefix (NULL);
  snprintf (cmd, sizeof cmd, "rm -rf '%s'", dir);
  if (system (cmd) != 0)
    fprintf (stderr, "cleanup failed: %s\n", dir);
}

/* An allocation that fails runs the failure handler once, then exits
 * with a failure status. Run in a child so the test suite survives. */
static char g_oom_marker[64];

static void
oom_handler (void)
{
  FILE *fp = fopen (g_oom_marker, "a");
  if (fp)
    {
      fputs ("saved\n", fp);
      fclose (fp);
    }
}

static void
test_out_of_memory (void)
{
  snprintf (g_oom_marker, sizeof g_oom_marker, "/tmp/led_oom_%d", (int) getpid ());
  unlink (g_oom_marker);
  fflush (stderr);
  pid_t pid = fork ();
  if (pid == 0)
    {
      if (!freopen ("/dev/null", "w", stderr))
        _exit (2);
      xalloc_reserve (4096);
      xalloc_on_failure (oom_handler);
      volatile void *p = xmalloc (SIZE_MAX / 2);
      (void) p;
      _exit (0);                /* not reached: the allocation fails */
    }
  int status = 0;
  waitpid (pid, &status, 0);
  test_assert (WIFEXITED (status) && WEXITSTATUS (status) == EXIT_FAILURE,
               "a failed allocation exits with a failure status");
  char got[32] = "";
  FILE *fp = fopen (g_oom_marker, "r");
  if (fp)
    {
      if (!fgets (got, sizeof got, fp))
        got[0] = '\0';
      fclose (fp);
    }
  test_assert (strcmp (got, "saved\n") == 0,
               "a failed allocation runs the failure handler once first");
  unlink (g_oom_marker);
}

void
run_safety_tests (void)
{
  fprintf (stderr, "\n=== Data-safety tests ===\n");
  test_cut_paste_undo ();
  test_selection_ops ();
  test_undo_cap ();
  test_utf8_keys ();
  test_replace_all_fixes ();
  test_blocked_saves ();
  test_config_files ();
  test_out_of_memory ();
}

#include "undo_store.h"
#include <dirent.h>

void run_undo_store_tests (void);

/* The one history file in the store, or "" if there is not exactly one. */
static void
only_history (char *out, size_t out_size)
{
  char dir[1024];
  out[0] = '\0';
  if (out_size < sizeof dir + 300
      || undo_store_directory (dir, sizeof dir) != 0)
    return;
  DIR *d = opendir (dir);
  if (!d)
    return;
  int found = 0;
  struct dirent *e;
  while ((e = readdir (d)) != NULL)
    if (strstr (e->d_name, ".undo"))
      {
        snprintf (out, out_size, "%s/%s", dir, e->d_name);
        found++;
      }
  closedir (d);
  if (found != 1)
    out[0] = '\0';
}

/* Undo history kept between sessions. */
void
run_undo_store_tests (void)
{
  fprintf (stderr, "\n=== Undo history tests ===\n");
  char dir[] = "/tmp/led_undo_XXXXXX";
  if (!mkdtemp (dir))
    {
      test_assert (0, "mkdtemp");
      return;
    }
  char home[300], store[300], file[300], hist[PATH_MAX];
  snprintf (home, sizeof home, "%s/home", dir);
  snprintf (store, sizeof store, "%s/store", dir);
  snprintf (file, sizeof file, "%s/note.txt", dir);
  mkdir (home, 0700);
  char *old_home = getenv ("HOME") ? strdup (getenv ("HOME")) : NULL;
  setenv ("HOME", home, 1);
  char main_store[PATH_MAX];
  int had_store = undo_store_directory (main_store, sizeof main_store) == 0;
  undo_store_set_dir (store);

  write_file (file, "one two");
  char *argv[] = { "led", file };
  Editor ed;
  editor_init (&ed, 2, argv);
  ed.auto_save_threshold = 100000;
  test_assert (ed.undo_stack.count == 0, "a file never edited has no history");
  editor_handle_input (&ed, LED_KEY_END);
  editor_handle_input (&ed, '!');
  editor_handle_input (&ed, LED_KEY_HOME);
  editor_handle_input (&ed, 24);                /* Ctrl+X: cut the line */
  editor_handle_input (&ed, 26);                /* undo the cut */
  editor_handle_input (&ed, 19);                /* save: "one two!" */
  struct stat st;
  only_history (hist, sizeof hist);
  test_assert (hist[0] && stat (hist, &st) == 0 && (st.st_mode & 077) == 0
               && stat (store, &st) == 0 && (st.st_mode & 0777) == 0700,
               "saving writes the history, private to the user");
  editor_handle_input (&ed, 'Z');               /* typed, never saved */
  editor_cleanup (&ed);

  editor_init (&ed, 2, argv);
  ed.auto_save_threshold = 100000;
  test_assert (line_is (&ed.model, 0, "one two!") && ed.undo_stack.count == 1
               && ed.redo_stack.count == 8
               && strstr (ed.status_message, "history restored") != NULL,
               "reopening the file brings the history back as it was at the save");
  editor_handle_input (&ed, 25);                /* redo the cut */
  test_assert (line_is (&ed.model, 0, ""), "redo from the last session works, as one step");
  editor_handle_input (&ed, 26);
  editor_handle_input (&ed, 26);
  test_assert (line_is (&ed.model, 0, "one two") && ed.undo_stack.count == 0,
               "undo from the last session works");
  editor_cleanup (&ed);

  /* The file changed behind led's back: the history is not used. */
  write_file (file, "one tw0!");                /* same size, other text */
  editor_init (&ed, 2, argv);
  test_assert (ed.undo_stack.count == 0 && ed.redo_stack.count == 0
               && line_is (&ed.model, 0, "one tw0!"),
               "a file changed by something else gets no history");
  /* persistent_undo=0 writes nothing. */
  ed.auto_save_threshold = 100000;
  ed.config.display.persistent_undo = 0;
  editor_handle_input (&ed, 'q');
  editor_handle_input (&ed, 19);
  editor_cleanup (&ed);
  editor_init (&ed, 2, argv);
  test_assert (ed.undo_stack.count == 0 && line_is (&ed.model, 0, "qone tw0!"),
               "with persistent_undo off a save keeps no history");
  ed.auto_save_threshold = 100000;
  editor_handle_input (&ed, 'r');
  editor_handle_input (&ed, 19);                /* history written again */
  editor_cleanup (&ed);

  /* A damaged history file is ignored, not half read. */
  only_history (hist, sizeof hist);
  test_assert (hist[0] && truncate (hist, 40) == 0, "history file cut short");
  editor_init (&ed, 2, argv);
  test_assert (ed.undo_stack.count == 0 && ed.redo_stack.count == 0
               && line_is (&ed.model, 0, "rqone tw0!"),
               "a damaged history is ignored");
  editor_cleanup (&ed);

  /* A directory open to others is not used. */
  char loose[300];
  snprintf (loose, sizeof loose, "%s/loose", dir);
  mkdir (loose, 0755);
  chmod (loose, 0755);
  undo_store_set_dir (loose);
  UndoStack u = { NULL, 0, 0 }, r = { NULL, 0, 0 };
  test_assert (undo_store_save (file, &u, &r) == -1,
               "a directory others can read is refused");
  char link_dir[300];
  snprintf (link_dir, sizeof link_dir, "%s/link", dir);
  test_assert (symlink (store, link_dir) == 0, "symlink made");
  undo_store_set_dir (link_dir);
  test_assert (undo_store_save (file, &u, &r) == -1,
               "a symbolic link in place of the directory is refused");

  undo_store_set_dir (had_store ? main_store : NULL);
  char cmd[400];
  snprintf (cmd, sizeof cmd, "rm -rf '%s'", dir);
  if (system (cmd) != 0)
    fprintf (stderr, "cleanup failed: %s\n", dir);
  if (old_home)
    {
      setenv ("HOME", old_home, 1);
      free (old_home);
    }
  else
    unsetenv ("HOME");
}
