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
void
editor_init (Editor *ed, int argc, char *argv[])
{
  (void) load_editor_config (&ed->config);
  buffer_init (&ed->model);
  ed->filename = argc > 1 ? argv[1] : NULL;
  if (ed->filename && !is_filename_safe (ed->filename))
    {
      ed->filename = NULL;      /* reject unsafe names early */
    }
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
                {
                  // Skip loading large files
                }
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
  ed->show_line_numbers = 0;
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
  ed->cursor_line = 0;
  ed->cursor_col = buffer_get_line_length (&ed->model, 0);
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
      buffer_save_to_file (&ed->model, backup_path);
      ed->backup_count++;
      // Auto-save to main file
      buffer_save_to_file (&ed->model, ed->filename);
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
void
editor_handle_input (Editor *ed, int ch)
{
  struct timeval start_time;
  gettimeofday(&start_time, NULL);

  // Handle config toggles
  if (ch == KEY_F (2))
    {
      ed->show_line_numbers = !ed->show_line_numbers;
      return;
    }
  if (ch == KEY_F (3))
    {
      ed->config.display.word_wrap = !ed->config.display.word_wrap;
      set_status_message (ed, ed->config.display.word_wrap ? "Word wrap: ON" : "Word wrap: OFF");
      return;
    }
  if (ch == KEY_F (4))
    {
      ed->config.display.show_border = !ed->config.display.show_border;
      // Invalidate view so next draw does a full repaint (border appears/disappears)
      view_invalidate();
      set_status_message (ed, ed->config.display.show_border ? "Border: ON" : "Border: OFF (xterm copy clean)");
      return;
    }
  if ((ch == 31) && ed->config.search.enabled) /* Ctrl+/ or / */
    {
      ed->search_mode = 1;
      ed->replace_step = 0;
      memset (ed->search_buffer, 0, sizeof (ed->search_buffer));
      set_status_message (ed, "Search (Esc to cancel):");
      return;
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
      if (handle_input
          (ch, &ed->model, &ed->scroll_row, &ed->scroll_col, &ed->cursor_line,
           &ed->cursor_col, &ed->show_line_numbers, ed->search_buffer,
            &ed->search_mode, &ed->clipboard, ed->filename, ed) != 0)
        {
          set_status_message (ed,
                              "Error: Operation failed (insufficient memory?)");
        }
      ed->prev_key = ch;
      // Increment unsaved keystrokes for editing operations (exclude search/replace)
      if (!ed->search_mode && !ed->replace_step &&
          ((ch >= 32 && ch <= 126) || (ch >= 128 && ch <= 255)
           || ch == 9 || ch == 10 || ch == 13 || ch == 127
           || ch == KEY_BACKSPACE || ch == KEY_DC || ch == KEY_F(3)
           || ch == 3 || ch == 22 || ch == 24))
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
      // Set status messages for operations
      if (ch == 19)
        {                       // Ctrl+S save
          set_status_message (ed, "File saved");
          ed->file_modified = 0;
        }
      else if (ch == 26)
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
      else if (ed->search_mode)
        {
          set_status_message (ed, "Search (type regex, Enter to find, Esc cancel)");
        }
    }

  struct timeval end_time;
  gettimeofday(&end_time, NULL);
  ed->last_key_us = (end_time.tv_sec - start_time.tv_sec) * 1000000 +
                    (end_time.tv_usec - start_time.tv_usec);
}
void
editor_cleanup (Editor *ed)
{
  buffer_free (&ed->model);
  if (ed->clipboard)
    free (ed->clipboard);
  free_undo_stacks (&ed->undo_stack, &ed->redo_stack);
}
