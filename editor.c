#include "editor.h"
#include "model.h"
#include "controller.h"
#include "view.h"
#include "config.h"
#include "utils/utils.h"
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
  if (!ed || !stdscr)
    return;
  ed->key_word_left = 0;
  ed->key_word_right = 0;
  char *seq = tigetstr ("kLFT5");
  if (seq && seq != (char *) -1)
    {
      int code = key_defined (seq);
      if (code > 0)
        ed->key_word_left = code;
    }
  seq = tigetstr ("kRIT5");
  if (seq && seq != (char *) -1)
    {
      int code = key_defined (seq);
      if (code > 0)
        ed->key_word_right = code;
    }
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
      char peeled[512];
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
  if (ed->filename && !is_filename_safe (ed->filename))
    {
      ed->filename = NULL;      /* reject unsafe names early */
      start_line = 0;
      start_col = 0;
    }
  int open_note = 0;            /* 1 = file larger than the configured limit */
  if (ed->filename)
    {
      // Check for recovery file first
      char recovery_path[512];
      snprintf (recovery_path, sizeof (recovery_path), "%s.recovery",
                ed->filename);
      FILE *recovery_fp = fopen (recovery_path, "rb");
      if (recovery_fp)
        {
          fclose (recovery_fp);
          // Recovery file exists, load from it
          buffer_load_from_file (&ed->model, recovery_path,
            (long)ed->config.performance.max_file_size_mb * 1024 * 1024,
            ed->config.performance.max_line_length);
          // Remove recovery file after loading
          unlink (recovery_path);
        }
      else
        {
          // Check file size to prevent loading huge files
          FILE *fp = fopen (ed->filename, "rb");
          if (fp)
            {
              fseek (fp, 0, SEEK_END);
              long size = ftell (fp);
              fclose (fp);
              long maxb = (long)ed->config.performance.max_file_size_mb * 1024 * 1024;
              if (maxb <= 0) maxb = 10L * 1024 * 1024;
              if (size > maxb)
                open_note = 1;  /* leave the buffer empty; say so once status exists */
              else
                {
                  buffer_load_from_file (&ed->model, ed->filename, maxb, ed->config.performance.max_line_length);
                }
            }
        }
    }
  if (buffer_num_lines (&ed->model) == 0)
    {
      buffer_insert_line (&ed->model, 0, "");
    }
  ed->scroll_row = 0;
  ed->scroll_col = 0;
  const char *ext = ed->filename ? strrchr (ed->filename, '.') : NULL;
  ed->syntax_highlight = 0;
  if (ext)
    {
      int len = strlen (ed->config.syntax.extensions);
      char *list = xmalloc (len + 1);
      if (list)
        {
          strcpy (list, ed->config.syntax.extensions);
          char *token = strtok (list, ",");
          while (token)
            {
		



              if (strcmp (ext, token) == 0)
                {
                  ed->syntax_highlight = 1;
                  break;
                }
              token = strtok (NULL, ",");
            }
          free (list);
        }
    }
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
  // Initialize per-Editor undo/redo stacks (moved from globals)
  ed->undo_stack.changes = NULL;
  ed->undo_stack.count = 0;
  ed->undo_stack.capacity = 0;
  ed->redo_stack.changes = NULL;
  ed->redo_stack.count = 0;
  ed->redo_stack.capacity = 0;
  // Initialize auto-save
  ed->unsaved_keystrokes = 0;
  ed->auto_save_threshold = ed->config.autosave.keystrokes;     // Configurable keystrokes
  ed->auto_save_timeout = ed->config.autosave.timeout;  // Configurable timeout
  ed->last_save_time = time (NULL);
  ed->backup_count = 0;
  // Initialize status
  ed->status_message[0] = '\0';
  ed->status_message_time = 0;
  ed->file_modified = 0;
  ed->last_key_us = 0;
  ed->prev_key = 0;
  ed->goto_mode = 0;
  ed->goto_buffer[0] = '\0';
  ed->quit_armed = 0;
  ed->meta_pending = 0;
  ed->key_word_left = 0;
  ed->key_word_right = 0;
  if (open_note)
    set_status_message (ed, "File too large");
  else if (start_line > 0)
    {
      char msg[64];
      snprintf (msg, sizeof msg, "Line %d", ed->cursor_line + 1);
      set_status_message (ed, msg);
    }
}
void
set_status_message (Editor *ed, const char *message)
{
  strncpy (ed->status_message, message, sizeof (ed->status_message) - 1);
  ed->status_message[sizeof (ed->status_message) - 1] = '\0';
  ed->status_message_time = time (NULL);
}
void
auto_save (Editor *ed)
{
  if (ed->filename)
    {
      // Create backup before auto-saving
      char backup_path[512];
      snprintf (backup_path, sizeof (backup_path), "%s.bak.%d", ed->filename,
                ed->backup_count % 10 + 1);
      (void) buffer_save_to_file (&ed->model, backup_path);
      if (buffer_save_to_file (&ed->model, ed->filename) != 0)
        {
          set_status_message (ed, "Auto-save failed");
          return;
        }
      ed->backup_count++;
      ed->unsaved_keystrokes = 0;
      ed->last_save_time = time (NULL);
      ed->file_modified = 0;
      set_status_message (ed, "Auto-saved");
    }
}
void
editor_draw (WINDOW *frame, WINDOW *text, Editor *ed)
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
editor_draw_compat (WINDOW *win, Editor *ed)
{
  editor_draw (win, NULL, ed);
}
static int
goto_edit (Editor *ed, int ch)
{
  if (ch == 27)
    {
      ed->goto_mode = 0;
      ed->goto_buffer[0] = '\0';
      set_status_message (ed, "Cancelled");
      return 0;
    }
  if (ch == 10 || ch == 13 || ch == KEY_ENTER)
    {
      if (editor_apply_goto (ed, ed->goto_buffer) == 0)
        {
          char msg[64];
          snprintf (msg, sizeof msg, "Line %d Col %d",
                    ed->cursor_line + 1, ed->cursor_col + 1);
          set_status_message (ed, msg);
        }
      else
        set_status_message (ed, "Bad line number");
      ed->goto_mode = 0;
      return 0;
    }
  if (ch == 127 || ch == 8 || ch == KEY_BACKSPACE)
    {
      int n = (int) strlen (ed->goto_buffer);
      if (n > 0)
        ed->goto_buffer[n - 1] = '\0';
      return 0;
    }
  if ((ch >= '0' && ch <= '9') || ch == ':')
    {
      int n = (int) strlen (ed->goto_buffer);
      if (n < (int) sizeof (ed->goto_buffer) - 1)
        {
          ed->goto_buffer[n] = (char) ch;
          ed->goto_buffer[n + 1] = '\0';
        }
      return 0;
    }
  return 0;
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

  if (ed->goto_mode)
    return goto_edit (ed, ch);
  if (ch == 7 && !ed->search_mode && ed->replace_step == 0)
    {
      ed->goto_mode = 1;
      ed->goto_buffer[0] = '\0';
      ed->meta_pending = 0;
      set_status_message (ed, "Go to line[:col]");
      return 0;
    }

  // Handle config toggles
  if (ch == KEY_F (2))
    {
      ed->show_line_numbers = !ed->show_line_numbers;
      return 0;
    }
  if (ch == KEY_F (3))
    {
      ed->config.display.word_wrap = !ed->config.display.word_wrap;
      set_status_message (ed, ed->config.display.word_wrap ? "Word wrap: ON" : "Word wrap: OFF");
      return 0;
    }
  if (ch == KEY_F (4))
    {
      ed->config.display.show_border = !ed->config.display.show_border;
      // Invalidate view so next draw does a full repaint (border appears/disappears)
      view_invalidate();
      set_status_message (ed, ed->config.display.show_border ? "Border: ON" : "Border: OFF (xterm copy clean)");
      return 0;
    }
  if ((ch == 31) && ed->config.search.enabled) /* Ctrl+/ or / */
    {
      ed->search_mode = 1;
      ed->replace_step = 0;
      memset (ed->search_buffer, 0, sizeof (ed->search_buffer));
      set_status_message (ed, "Search (Esc to cancel):");
      return 0;
    }
  if (ch == 18 && ed->config.search.enabled)
    {
      if (ed->replace_step == 0)
        {
          ed->replace_step = 1;
          ed->search_mode = 0;
          memset (ed->search_buffer, 0, sizeof (ed->search_buffer));
          memset (ed->replace_buffer, 0, sizeof (ed->replace_buffer));
        }
    }
  else if (ed->replace_step == 1)
    {
      if (ch == 27)
        ed->replace_step = 0;
      else if (ch == 10 || ch == 13 || ch == KEY_ENTER)
        {
          if (strlen (ed->search_buffer) > 0)
            ed->replace_step = 2;
        }
      else if (((ch >= 32 && ch <= 126) || (ch >= 128 && ch <= 255))
               && strlen (ed->search_buffer) < sizeof (ed->search_buffer) - 1)
        {
          strncat (ed->search_buffer, (char *) &ch, 1);
        }
    }
  else if (ed->replace_step == 2)
    {
      if (ch == 27)
        ed->replace_step = 0;
      else if (ch == 10 || ch == 13 || ch == KEY_ENTER)
        {
          buffer_replace_all (&ed->model, ed->search_buffer,
                              ed->replace_buffer);
          ed->replace_step = 0;
        }
      else if (((ch >= 32 && ch <= 126) || (ch >= 128 && ch <= 255))
               && strlen (ed->replace_buffer) <
               sizeof (ed->replace_buffer) - 1)
        {
          strncat (ed->replace_buffer, (char *) &ch, 1);
        }
    }
  else
    {
      int was_meta = ed->meta_pending;
      if (handle_input
          (ch, &ed->model, &ed->scroll_row, &ed->scroll_col, &ed->cursor_line,
           &ed->cursor_col, &ed->show_line_numbers, ed->search_buffer,
            &ed->search_mode, &ed->clipboard, ed->filename, ed) != 0)
        {
          set_status_message (ed,
                              "Error: Operation failed (insufficient memory?)");
        }
      ed->prev_key = ch;
      /* Esc-b / Esc-f only move. Esc-Backspace deletes a word. */
      if (was_meta)
        {
          if (!ed->search_mode && !ed->replace_step
              && (ch == 127 || ch == 8 || ch == KEY_BACKSPACE))
            {
              ed->unsaved_keystrokes++;
              ed->file_modified = 1;
            }
        }
      else if (!ed->search_mode && !ed->replace_step &&
          ((ch >= 32 && ch <= 126) || (ch >= 128 && ch <= 255)
           || ch == 9 || ch == 10 || ch == 13 || ch == KEY_ENTER || ch == 127
           || ch == KEY_BACKSPACE || ch == KEY_DC || ch == KEY_F(3)
           || ch == KEY_BTAB
           || ch == 3 || ch == 11 || ch == 22 || ch == 23 || ch == 24))
        {
          ed->unsaved_keystrokes++;
          ed->file_modified = 1;
        }
      // Auto-save if threshold reached (keystrokes or time)
      time_t now = time (NULL);
      if ((ed->unsaved_keystrokes >= ed->auto_save_threshold ||
           (ed->auto_save_timeout > 0
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
  free_undo_stacks (&ed->undo_stack, &ed->redo_stack);
}
