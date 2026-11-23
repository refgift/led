#include "editor.h"
#include "model.h"
#include "controller.h"
#include "view.h"
#include "config.h"
#include "utils/utils.h"
#include "undo_store.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>

static int
all_digits (const char *s)
{
  if (!s || !*s)
    return 0;
  for (const char *p = s; *p; p++)
    if (!isdigit ((unsigned char) *p))
      return 0;
  return 1;
}

static int
all_digits_range (const char *s, const char *end)
{
  if (!s || s >= end)
    return 0;
  for (const char *p = s; p < end; p++)
    if (!isdigit ((unsigned char) *p))
      return 0;
  return 1;
}

int
led_parse_plus_line (const char *arg, int *line)
{
  if (!arg || arg[0] != '+' || !arg[1] || !line)
    return 0;
  if (!all_digits (arg + 1))
    return 0;
  long v = strtol (arg + 1, NULL, 10);
  if (v < 1 || v > 1000000000L)
    return 0;
  *line = (int) v;
  return 1;
}

int
led_parse_file_location (const char *arg, char *path, size_t path_sz,
                         int *line, int *col)
{
  if (line)
    *line = 0;
  if (col)
    *col = 0;
  if (!arg || !path || path_sz < 2 || !line || !col)
    return 0;
  const char *last = strrchr (arg, ':');
  if (!last || last == arg || !all_digits (last + 1))
    return 0;
  long tail = strtol (last + 1, NULL, 10);
  if (tail < 1 || tail > 1000000000L)
    return 0;

  const char *prev = NULL;
  for (const char *p = arg; p < last; p++)
    if (*p == ':')
      prev = p;
  if (prev && prev != arg && all_digits_range (prev + 1, last))
    {
      long head = strtol (prev + 1, NULL, 10);
      if (head < 1 || head > 1000000000L)
        return 0;
      size_t n = (size_t) (prev - arg);
      if (n == 0 || n >= path_sz)
        return 0;
      memcpy (path, arg, n);
      path[n] = '\0';
      *line = (int) head;
      *col = (int) tail;
      return 1;
    }

  size_t n = (size_t) (last - arg);
  if (n == 0 || n >= path_sz)
    return 0;
  memcpy (path, arg, n);
  path[n] = '\0';
  *line = (int) tail;
  *col = 0;
  return 1;
}

int
editor_apply_goto (Editor *ed, const char *spec)
{
  if (!ed || !spec || !isdigit ((unsigned char) spec[0]))
    return -1;
  char *end = NULL;
  long line = strtol (spec, &end, 10);
  if (end == spec || line < 1 || line > 1000000000L)
    return -1;
  long col = 1;
  if (*end == ':')
    {
      char *end2 = NULL;
      col = strtol (end + 1, &end2, 10);
      if (end2 == end + 1 || col < 1 || col > 1000000000L || *end2 != '\0')
        return -1;
    }
  else if (*end != '\0')
    return -1;

  int n = buffer_num_lines (&ed->model);
  if (n < 1)
    return -1;
  if (line > n)
    line = n;
  ed->cursor_line = (int) line - 1;
  int len = buffer_get_line_length (&ed->model, ed->cursor_line);
  int c = (int) col - 1;
  if (c > len)
    c = len;
  if (c < 0)
    c = 0;
  ed->cursor_col = c;
  ed->scroll_col = 0;
  ed->scroll_row = ed->cursor_line > 3 ? ed->cursor_line - 3 : 0;
  return 0;
}

void
editor_bind_terminal_keys (Editor *ed)
{
  if (!ed)
    return;
  ed->key_word_left = LED_KEY_WORD_LEFT;
  ed->key_word_right = LED_KEY_WORD_RIGHT;
}

void
editor_init (Editor *ed, int argc, char *argv[])
{
  (void) load_editor_config (&ed->config);
  buffer_init (&ed->model);
  ed->filename_storage[0] = '\0';
  ed->filename = NULL;
  int start_line = 0;
  int start_col = 0;
  const char *raw = NULL;
  if (argc > 1 && led_parse_plus_line (argv[1], &start_line))
    raw = (argc > 2) ? argv[2] : NULL;
  else if (argc > 1)
    raw = argv[1];
  if (raw)
    {
      char peeled[PATH_MAX];
      int pline = 0;
      int pcol = 0;
      /* "file.c:142" is a compiler location unless that exact name exists. */
      if (led_parse_file_location (raw, peeled, sizeof peeled, &pline, &pcol)
          && access (raw, F_OK) != 0)
        {
          snprintf (ed->filename_storage, sizeof ed->filename_storage, "%s",
                    peeled);
          ed->filename = ed->filename_storage;
          start_line = pline;
          start_col = pcol;
        }
      else
        ed->filename = raw;
    }
  int name_refused = 0;
  if (ed->filename && !is_filename_safe (ed->filename))
    {
      name_refused = 1;
      ed->filename = NULL;      /* reject unsafe names early */
      start_line = 0;
      start_col = 0;
    }
  int open_note = BUFFER_LOAD_OK;
  int recovered = 0;
  ed->save_blocked = 0;
  if (ed->filename)
    {
      long maxb = (long) ed->config.performance.max_file_size_mb * 1024 * 1024;
      if (maxb <= 0)
        maxb = 10L * 1024 * 1024;
      long memb = (long) ed->config.performance.memory_limit_mb * 1024 * 1024;
      if (memb <= 0)
        memb = 50L * 1024 * 1024;
      char recovery_path[PATH_MAX];
      snprintf (recovery_path, sizeof (recovery_path), "%s.recovery",
                ed->filename);
      if (access (recovery_path, F_OK) == 0)
        {
          open_note = buffer_load_from_file (&ed->model, recovery_path, maxb,
                                              memb);
          /* Only a recovery file that loaded whole is used up. A bad
           * one stays on disk for the user to look at. */
          if (open_note == BUFFER_LOAD_OK)
            {
              recovered = 1;
              unlink (recovery_path);
            }
        }
      if (!recovered)
        open_note = buffer_load_from_file (&ed->model, ed->filename, maxb,
                                           memb);
      /* A name that does not exist yet is a new file. Anything else
       * that did not load whole must never be written back. */
      if (open_note == BUFFER_LOAD_NOT_FOUND)
        open_note = BUFFER_LOAD_OK;
      else if (open_note != BUFFER_LOAD_OK)
        ed->save_blocked = 1;
    }
  if (buffer_num_lines (&ed->model) == 0)
    {
      buffer_insert_line (&ed->model, 0, "");
    }
  ed->scroll_row = 0;
  ed->scroll_col = 0;
  /* The file's extension chooses its language. A file no language
   * claims, and plain text, are shown without colours. */
  const Language *lang = config_select_language (&ed->config, ed->filename);
  ed->syntax_highlight = (lang && lang->highlight) ? 1 : 0;
  /* Source files show line numbers unless the config file says otherwise.
   * Compiler diagnostics are line numbers; the screen should match. */
  if (ed->config.display.line_numbers_configured)
    ed->show_line_numbers = ed->config.display.show_line_numbers ? 1 : 0;
  else
    ed->show_line_numbers = ed->syntax_highlight ? 1 : 0;
  ed->cursor_line = 0;
  ed->cursor_col = 0;
  if (start_line > 0)
    {
      int n = buffer_num_lines (&ed->model);
      int line = start_line;
      if (line > n)
        line = n;
      if (line < 1)
        line = 1;
      ed->cursor_line = line - 1;
      int len = buffer_get_line_length (&ed->model, ed->cursor_line);
      int col = start_col > 0 ? start_col - 1 : 0;
      if (col > len)
        col = len;
      if (col < 0)
        col = 0;
      ed->cursor_col = col;
      ed->scroll_row = ed->cursor_line > 3 ? ed->cursor_line - 3 : 0;
    }
  ed->selection_start_line = 0;
  ed->selection_start_col = 0;
  ed->selection_end_line = 0;
  ed->selection_end_col = 0;
  ed->selection_active = 0;
  ed->search_buffer[0] = 0;
  ed->search_mode = 0;
  ed->replace_buffer[0] = 0;
  ed->replace_step = 0;
  ed->clipboard = NULL;
  ed->paste_mode = 0;
  ed->paste_buf = NULL;
  ed->paste_len = 0;
  ed->paste_cap = 0;
  // Initialize per-Editor undo/redo stacks (moved from globals)
  ed->undo_stack.changes = NULL;
  ed->undo_stack.count = 0;
  ed->undo_stack.capacity = 0;
  ed->redo_stack.changes = NULL;
  ed->redo_stack.count = 0;
  ed->redo_stack.capacity = 0;
  /* Undo history from the last session, if the file is still the text
   * that history was saved with. A recovered buffer is not: it holds
   * changes the file on disk never got. */
  int history = 0;
  if (ed->filename && !recovered && !ed->save_blocked
      && ed->config.display.persistent_undo)
    history = undo_store_load (ed->filename, &ed->undo_stack,
                               &ed->redo_stack) == 0;
  // Initialize auto-save
  ed->unsaved_keystrokes = 0;
  ed->auto_save_threshold = ed->config.autosave.keystrokes;     // Configurable keystrokes
  ed->auto_save_timeout = ed->config.autosave.timeout;  // Configurable timeout
  ed->last_save_time = time (NULL);
  ed->backup_count = 0;
  // Initialize status
  ed->status_message[0] = '\0';
  ed->status_message_time = 0;
  /* Recovered text is not on disk under the real name yet. */
  ed->file_modified = recovered ? 1 : 0;
  ed->last_key_us = 0;
  ed->prev_key = 0;
  ed->goto_mode = 0;
  ed->goto_buffer[0] = '\0';
  ed->quit_armed = 0;
  ed->meta_pending = 0;
  ed->key_word_left = 0;
  ed->key_word_right = 0;
  ed->page_rows = 0;
  if (name_refused)
    set_status_message (ed, "File name not usable — nothing opened");
  else if (recovered)
    set_status_message (ed, "Recovered unsaved changes — Ctrl+S to keep");
  else if (open_note == BUFFER_LOAD_NOT_REGULAR)
    set_status_message (ed, "Not a regular file");
  else if (open_note == BUFFER_LOAD_UNREADABLE)
    set_status_message (ed, "Cannot read file");
  else if (open_note == BUFFER_LOAD_TOO_BIG)
    set_status_message (ed, "File too large");
  else if (open_note == BUFFER_LOAD_BINARY)
    set_status_message (ed, "Binary file");
  else if (open_note == BUFFER_LOAD_OVER_MEMORY)
    set_status_message (ed, "Too many lines");
  else if (open_note == BUFFER_LOAD_NOMEM)
    set_status_message (ed, "Out of memory");
  else if (start_line > 0)
    {
      char msg[64];
      snprintf (msg, sizeof msg, "Line %d", ed->cursor_line + 1);
      set_status_message (ed, msg);
    }
  else if (history && ed->undo_stack.count > 0)
    set_status_message (ed, "Undo history restored");
}
void
set_status_message (Editor *ed, const char *message)
{
  strncpy (ed->status_message, message, sizeof (ed->status_message) - 1);
  ed->status_message[sizeof (ed->status_message) - 1] = '\0';
  ed->status_message_time = time (NULL);
}
int
editor_write_recovery (Editor *ed)
{
  if (!ed || !ed->filename || !ed->file_modified || ed->save_blocked)
    return -1;
  char recovery_path[PATH_MAX];
  int n = snprintf (recovery_path, sizeof recovery_path, "%s.recovery",
                    ed->filename);
  if (n < 0 || (size_t) n >= sizeof recovery_path)
    return -1;
  return buffer_save_to_file (&ed->model, recovery_path);
}

void
editor_keep_history (Editor *ed)
{
  if (ed && ed->filename && !ed->save_blocked
      && ed->config.display.persistent_undo)
    (void) undo_store_save (ed->filename, &ed->undo_stack, &ed->redo_stack);
}

void
auto_save (Editor *ed)
{
  if (ed->filename && ed->save_blocked)
    {
      ed->unsaved_keystrokes = 0;
      ed->last_save_time = time (NULL);
      return;
    }
  if (ed->filename)
    {
      // Create backup before auto-saving
      char backup_path[PATH_MAX];
      int n = snprintf (backup_path, sizeof (backup_path), "%s.bak.%d",
                        ed->filename, ed->backup_count % 10 + 1);
      /* A cut-off name would be some other file. Skip the backup then. */
      if (n > 0 && (size_t) n < sizeof (backup_path))
        (void) buffer_save_to_file (&ed->model, backup_path);
      if (buffer_save_to_file (&ed->model, ed->filename) != 0)
        {
          set_status_message (ed, "Auto-save failed");
          return;
        }
      editor_keep_history (ed);
      ed->backup_count++;
      ed->unsaved_keystrokes = 0;
      ed->last_save_time = time (NULL);
      ed->file_modified = 0;
      set_status_message (ed, "Auto-saved");
    }
}
void
editor_draw (Pane *frame, Pane *text, Editor *ed)
{
  int dummy_y, dummy_x;
  // If only frame given (text==NULL), try to use frame as both for legacy,
  // but prefer subwindow path when text is provided.
  if (frame && !text)
    {
      // Legacy single-window call — treat as frame, no text subwindow
      draw_update (frame, NULL, &ed->model, &ed->scroll_row, &ed->scroll_col,
                   ed->cursor_line, ed->cursor_col, ed->show_line_numbers,
                   ed->syntax_highlight, ed->search_mode, ed->search_buffer,
                   ed->selection_start_line, ed->selection_start_col,
                   ed->selection_end_line, ed->selection_end_col,
                   ed->selection_active, &dummy_y, &dummy_x, ed->replace_step,
                   ed->replace_buffer, &ed->config, ed);
      return;
    }
  draw_update (frame, text, &ed->model, &ed->scroll_row, &ed->scroll_col,
               ed->cursor_line, ed->cursor_col, ed->show_line_numbers,
               ed->syntax_highlight, ed->search_mode, ed->search_buffer,
               ed->selection_start_line, ed->selection_start_col,
               ed->selection_end_line, ed->selection_end_col,
               ed->selection_active, &dummy_y, &dummy_x, ed->replace_step,
               ed->replace_buffer, &ed->config, ed);
}

void
editor_draw_compat (Pane *win, Editor *ed)
{
  editor_draw (win, NULL, ed);
}
int
editor_handle_input (Editor *ed, int ch)
{
  struct timeval start_time;
  gettimeofday(&start_time, NULL);

  /* Ctrl+Q. A modified buffer asks for a second press so a slip does not
   * discard the edit. Any other key disarms it. */
  if (ch == 17)
    {
      if (!ed->file_modified || ed->quit_armed)
        return 1;
      ed->quit_armed = 1;
      set_status_message (ed, "Modified — Ctrl+Q again to quit");
      return 0;
    }
  ed->quit_armed = 0;

  /* The controller turns the key into a symbol, a macro, or a token. */
  unsigned long gen_before = ed->model.edit_generation;
  (void) handle_input (ch, &ed->model, &ed->scroll_row, &ed->scroll_col,
                       &ed->cursor_line, &ed->cursor_col,
                       &ed->show_line_numbers, ed->search_buffer,
                       &ed->search_mode, &ed->clipboard, ed->filename, ed);
  if (controller_key_was_plain ())
    {
      ed->prev_key = ch;
      /* Modified means the text changed, whatever key did it. That
       * covers undo and redo, and leaves out copy and cursor keys. */
      if (ed->model.edit_generation != gen_before)
        {
          ed->unsaved_keystrokes++;
          ed->file_modified = 1;
        }
      // Auto-save if threshold reached (keystrokes or time)
      time_t now = time (NULL);
      if ((ed->unsaved_keystrokes >= ed->auto_save_threshold ||
           (ed->auto_save_timeout > 0 && ed->file_modified
            && now - ed->last_save_time >= ed->auto_save_timeout))
          && ed->filename)
        {
          auto_save (ed);
        }
      /* Save reports its own success or failure from the controller. */
      if (ch == 26)
        {                       // Ctrl+Z undo
          set_status_message (ed, "Undid operation");
        }
      else if (ch == 25)
        {                       // Ctrl+Y redo
          set_status_message (ed, "Redid operation");
        }
      else if (ch == 3)
        {                       // Ctrl+C Copy
          set_status_message (ed, "Copied");
        }
      else if (ch == 22)
        {                       // Ctrl+V Paste
          set_status_message (ed, "Pasted");
        }
      else if (ch == 24)
        {                       // Ctrl+X Cut
          set_status_message (ed, "Cut");
        }
    }

  struct timeval end_time;
  gettimeofday(&end_time, NULL);
  ed->last_key_us = (end_time.tv_sec - start_time.tv_sec) * 1000000 +
                    (end_time.tv_usec - start_time.tv_usec);
  return 0;
}
void
editor_cleanup (Editor *ed)
{
  buffer_free (&ed->model);
  if (ed->clipboard)
    free (ed->clipboard);
  free (ed->paste_buf);
  ed->paste_buf = NULL;
  free_undo_stacks (&ed->undo_stack, &ed->redo_stack);
}
