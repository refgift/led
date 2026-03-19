#include "controller.h"
#include "editor.h"
#include "view.h"
#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include <ctype.h>
UndoStack undo_stack = { NULL, 0, 0 };
UndoStack redo_stack = { NULL, 0, 0 };
static char *
my_strdup (const char *s)
{
  if (!s)
    return NULL;
  int len = strlen (s);
  char *d = malloc (len + 1);
  if (d)
    memcpy (d, s, len + 1);
  return d;
}
void
init_undo (void)
{
  // Already initialized statically
}
void
push_undo (bool is_insert, int line, int col, char ch)
{
  if (undo_stack.count >= undo_stack.capacity)
    {
      undo_stack.capacity =
        undo_stack.capacity == 0 ? 16 : undo_stack.capacity * 2;
      undo_stack.changes =
        realloc (undo_stack.changes, undo_stack.capacity * sizeof (Change));
      if (!undo_stack.changes)
        return;                 // Handle error
    }
  undo_stack.changes[undo_stack.count].is_insert = is_insert;
  undo_stack.changes[undo_stack.count].line = line;
  undo_stack.changes[undo_stack.count].col = col;
  undo_stack.changes[undo_stack.count].ch = ch;
  undo_stack.count++;
}
void
push_redo (bool is_insert, int line, int col, char ch)
{
  if (redo_stack.count >= redo_stack.capacity)
    {
      redo_stack.capacity =
        redo_stack.capacity == 0 ? 16 : redo_stack.capacity * 2;
      redo_stack.changes =
        realloc (redo_stack.changes, redo_stack.capacity * sizeof (Change));
      if (!redo_stack.changes)
        return;                 // Handle error
    }
  redo_stack.changes[redo_stack.count].is_insert = is_insert;
  redo_stack.changes[redo_stack.count].line = line;
  redo_stack.changes[redo_stack.count].col = col;
  redo_stack.changes[redo_stack.count].ch = ch;
  redo_stack.count++;
}
void
undo_operation (Buffer *buf, int *cursor_line, int *cursor_col)
{
  if (undo_stack.count > 0)
    {
      undo_stack.count--;
      Change c = undo_stack.changes[undo_stack.count];
      // Push to redo for redo functionality
      push_redo (c.is_insert, c.line, c.col, c.ch);
      // Apply reverse
      if (c.is_insert)
        {
          buffer_delete_char (buf, c.line, c.col);
          if (*cursor_line == c.line && *cursor_col > c.col)
            (*cursor_col)--;
        }
      else
        {
          buffer_insert_char (buf, c.line, c.col, c.ch);
          if (*cursor_line == c.line && *cursor_col >= c.col)
            (*cursor_col)++;
        }
    }
}
void
redo_operation (Buffer *buf, int *cursor_line, int *cursor_col)
{
  if (redo_stack.count > 0)
    {
      redo_stack.count--;
      Change c = redo_stack.changes[redo_stack.count];
      // Push inverse to undo stack for future undos
      push_undo (!c.is_insert, c.line, c.col, c.ch);
      // Apply the operation
      if (c.is_insert)
        {
          buffer_insert_char (buf, c.line, c.col, c.ch);
          if (*cursor_line == c.line && *cursor_col >= c.col)
            (*cursor_col)++;
        }
      else
        {
          buffer_delete_char (buf, c.line, c.col);
          if (*cursor_line == c.line && *cursor_col > c.col)
            (*cursor_col)--;
        }
    }
}
void
clear_redo (void)
{
  redo_stack.count = 0;
}
void
free_undo (void)
{
  free (undo_stack.changes);
  undo_stack.changes = NULL;
  undo_stack.count = 0;
  undo_stack.capacity = 0;
  free (redo_stack.changes);
  redo_stack.changes = NULL;
  redo_stack.count = 0;
  redo_stack.capacity = 0;
}
void
search_next (Buffer *buf, int *cursor_line, int *cursor_col,
             const char *pattern)
{
  regex_t regex;
  if (regcomp (&regex, pattern, 0) != 0)
    return;
  int found = 0;
  int clamped = 0;
  for (int l = *cursor_line; l < buffer_num_lines (buf) && !found && (!clamped || l == *cursor_line); l++)
    {
       const char *line = buffer_get_line (buf, l);
       int len = strlen (line);
       if (l == *cursor_line && *cursor_col > len)
         {
           *cursor_col = len;
           clamped = 1;
         }
       regmatch_t match;
       int pos = (l == *cursor_line) ? *cursor_col : 0;
      int line_flags = (pos > 0) ? REG_NOTBOL : 0;
      while (regexec (&regex, line + pos, 1, &match, line_flags) == 0)
        {
          if (match.rm_so == 0 && pos == *cursor_col && l == *cursor_line)
            {
              pos += match.rm_eo;
              line_flags = REG_NOTBOL;
              continue;
            }
          *cursor_line = l;
          *cursor_col = pos + match.rm_so;
          found = 1;
          break;
        }
    }
  regfree (&regex);
}
int
handle_input (int ch, Buffer *buf, int *scroll_row, int *scroll_col,
              int *cursor_line, int *cursor_col, int *show_line_numbers,
              char *search_buffer, int *search_mode, char **clipboard,
              const char *filename, int *selection_start_line,
              int *selection_start_col, int *selection_end_line,
              int *selection_end_col, int *selection_active, Editor *ed)
{
  int error_occurred = 0;
  if (show_line_numbers==NULL) {}
  if (ed==NULL) {}
  if (*search_mode)
    {
      if (ch == '\n' || ch == 13)
        {
          if (strlen (search_buffer) > 0)
            {
              search_next (buf, cursor_line, cursor_col, search_buffer);
            }
          // Stay in search mode, allow repeated searches with Enter
        }
      else if (ch == 27)
        {                       // ESC exits search mode
          *search_mode = 0;
          search_buffer[0] = 0;
        }
      else if (ch >= 32 && ch <= 126)
        {
          int len = strlen (search_buffer);
          if (len < 255)
            {
              search_buffer[len] = (char) ch;
              search_buffer[len + 1] = 0;
            }
        }
      else if (ch == 127 || ch == 8)
        {
          int len = strlen (search_buffer);
          if (len > 0)
            search_buffer[len - 1] = 0;
        }
    }
  else
    {
      switch (ch)
        {
        case KEY_LEFT:
          if (*cursor_col > 0)
            {
              (*cursor_col)--;
            }
          else if (*cursor_line > 0)
            {
              (*cursor_line)--;
              *cursor_col = buffer_get_line_length (buf, *cursor_line);
            }
          break;
        case KEY_RIGHT:
          if (*cursor_col < buffer_get_line_length (buf, *cursor_line))
            {
              (*cursor_col)++;
            }
          else if (*cursor_line < buffer_num_lines (buf) - 1)
            {
              (*cursor_line)++;
              *cursor_col = 0;
            }
          break;
case KEY_UP:
  if (ed->config.display.word_wrap) {
    int num_digits = calculate_digits(buffer_num_lines(buf));
    int num_width = *show_line_numbers ? num_digits + 1 : 0;
    int available_width = COLS - 2 - num_width;
    const char* current_line = buffer_get_line(buf, *cursor_line);
    int current_vis_row = get_visual_row_for_column(current_line, *cursor_col, available_width, ed->config.display.tab_width);
    if (current_vis_row > 0) {
      // Move to previous visual row in same logical line
      int prev_start_col = get_start_column_for_visual_row(current_line, current_vis_row - 1, available_width, ed->config.display.tab_width);
      *cursor_col = prev_start_col;
    } else {
      // Move to previous logical line, to its last visual row
      if (*cursor_line > 0) {
        (*cursor_line)--;
        const char* prev_line = buffer_get_line(buf, *cursor_line);
        int prev_vis_rows = visual_rows_for_line(prev_line, available_width);
        int last_start_col = get_start_column_for_visual_row(prev_line, prev_vis_rows - 1, available_width, ed->config.display.tab_width);
        *cursor_col = last_start_col;
      }
    }
  } else {
    if (*cursor_line > 0)
      {
        (*cursor_line)--;
        if (*cursor_col > buffer_get_line_length (buf, *cursor_line))
          {
            *cursor_col = buffer_get_line_length (buf, *cursor_line);
          }
      }
  }
  break;
case KEY_DOWN:
  if (ed->config.display.word_wrap) {
    const char* current_line = buffer_get_line(buf, *cursor_line);
    int num_digits = calculate_digits(buffer_num_lines(buf));
    int num_width = *show_line_numbers ? num_digits + 1 : 0;
    int available_width = COLS - 2 - num_width;
    int vis_rows = visual_rows_for_line(current_line, available_width);
    int current_vis_row = get_visual_row_for_column(current_line, *cursor_col, available_width, ed->config.display.tab_width);
    if (current_vis_row < vis_rows - 1) {
      // Move to next visual row in same logical line
      int next_start_col = get_start_column_for_visual_row(current_line, current_vis_row + 1, available_width, ed->config.display.tab_width);
      *cursor_col = next_start_col;
    } else {
      // Move to next logical line
      if (*cursor_line < buffer_num_lines(buf) - 1) {
        (*cursor_line)++;
        *cursor_col = 0;
      }
    }
  } else {
    if (*cursor_line < buffer_num_lines (buf) - 1)
      {
        (*cursor_line)++;
        if (*cursor_col > buffer_get_line_length (buf, *cursor_line))
          {
            *cursor_col = buffer_get_line_length (buf, *cursor_line);
          }
      }
  }
  break;
        case KEY_HOME:
          if (*cursor_col == 0)
            {
              *cursor_line = 0;
              *cursor_col = 0;
            }
          else
            {
              *cursor_col = 0;
            }
          break;
        case KEY_END:
          if (*cursor_col == buffer_get_line_length (buf, *cursor_line))
            {
              *cursor_line = buffer_num_lines (buf) - 1;
              *cursor_col = buffer_get_line_length (buf, *cursor_line);
            }
          else
            {
              *cursor_col = buffer_get_line_length (buf, *cursor_line);
            }
          break;
        case KEY_PPAGE:
          if (*scroll_row >= (int) LINES - 2)
            *scroll_row -= LINES - 2;
          else
            *scroll_row = 0;
          if (*cursor_line > *scroll_row + (int) LINES - 3)
            *cursor_line = *scroll_row + LINES - 3;
          break;
        case KEY_NPAGE:
          *scroll_row += LINES - 2;
          if (*scroll_row > buffer_num_lines (buf) - ((int) LINES - 2))
            *scroll_row =
              buffer_num_lines (buf) >
              (int) LINES - 2 ? buffer_num_lines (buf) - (LINES - 2) : 0;
          if (*cursor_line < *scroll_row)
            *cursor_line = *scroll_row;
          break;
        case KEY_BACKSPACE:
        case 127:              // Delete key
          if (*cursor_col > 0)
            {
              char deleted =
                buffer_get_char (buf, *cursor_line, *cursor_col - 1);
              if (buffer_delete_char (buf, *cursor_line, *cursor_col - 1) ==
                  0)
                {
                  push_undo (false, *cursor_line, *cursor_col - 1, deleted);
                  clear_redo ();
                  (*cursor_col)--;
                }
              else
                {
                  error_occurred = 1;
                }
            }
          else if (*cursor_line > 0)
            {
              int prev_len =
                buffer_get_line_length (buf, *cursor_line - 1);
              if (buffer_delete_char (buf, *cursor_line - 1, prev_len) == 0)
                {
                  push_undo (false, *cursor_line - 1, prev_len, '\n');
                  clear_redo ();
                  (*cursor_line)--;
                  *cursor_col = prev_len;
                }
              else
                {
                  error_occurred = 1;
                }
            }
          break;
        case KEY_DC:           // Delete forward
          if (*cursor_col < buffer_get_line_length (buf, *cursor_line))
            {
              char deleted = buffer_get_char (buf, *cursor_line, *cursor_col);
              if (buffer_delete_char (buf, *cursor_line, *cursor_col) == 0)
                {
                  push_undo (false, *cursor_line, *cursor_col, deleted);
                  clear_redo ();
                }
              else
                {
                  error_occurred = 1;
                }
            }
          else if (*cursor_line < buffer_num_lines (buf) - 1)
            {
              if (buffer_delete_char (buf, *cursor_line, *cursor_col) == 0)
                {
                  push_undo (false, *cursor_line,
                             buffer_get_line_length (buf, *cursor_line),
                             '\n');
                  clear_redo ();
                }
              else
                {
                  error_occurred = 1;
                }
            }
          break;
        case 19:               // Ctrl+S to save
          if (filename)
            buffer_save_to_file (buf, filename);
          break;
        case 25:               // Ctrl+Y to redo
          redo_operation (buf, cursor_line, cursor_col);
          break;
        case 26:               // Ctrl+Z to undo
          undo_operation (buf, cursor_line, cursor_col);
          break;
        case 3:                // Ctrl+C to copy
          if (*clipboard)
            free (*clipboard);
          if (*selection_active)
            {
              // normalize
              int sl = *selection_start_line, sc =
                *selection_start_col, el = *selection_end_line, ec =
                *selection_end_col;
              if (sl > el || (sl == el && sc > ec))
                {
                  int t = sl;
                  sl = el;
                  el = t;
                  t = sc;
                  sc = ec;
                  ec = t;
                }
              // build string
              int total = 0;
              for (int l = sl; l <= el; l++)
                {
                  const char *line = buffer_get_line (buf, l);
                  int len = strlen (line);
                  int s = (l == sl) ? sc : 0;
                  int e = (l == el) ? ec : len;
                  total += e - s + (l < el ? 1 : 0);
                }
              *clipboard = malloc (total + 1);
              if (*clipboard)
                {
                  char *p = *clipboard;
                  for (int l = sl; l <= el; l++)
                    {
                      const char *line = buffer_get_line (buf, l);
                      int len = strlen (line);
                      int s = (l == sl) ? sc : 0;
                      int e = (l == el) ? ec : len;
                      memcpy (p, &line[s], e - s);
                      p += e - s;
                      if (l < el)
                        *p++ = '\n';
                    }
                  *p = 0;
                }
            }
          else
            {
              *clipboard = my_strdup (buffer_get_line (buf, *cursor_line));
            }
          break;
        case 24:               // Ctrl+X to cut
          if (*clipboard)
            free (*clipboard);
          if (*selection_active)
            {
              // normalize
              int sl = *selection_start_line, sc =
                *selection_start_col, el = *selection_end_line, ec =
                *selection_end_col;
              if (sl > el || (sl == el && sc > ec))
                {
                  int t = sl;
                  sl = el;
                  el = t;
                  t = sc;
                  sc = ec;
                  ec = t;
                }
              // build string
              int total = 0;
              for (int l = sl; l <= el; l++)
                {
                  const char *line = buffer_get_line (buf, l);
                  int len = strlen (line);
                  int s = (l == sl) ? sc : 0;
                  int e = (l == el) ? ec : len;
                  total += e - s + (l < el ? 1 : 0);
                }
              *clipboard = malloc (total + 1);
              if (*clipboard)
                {
                  char *p = *clipboard;
                  for (int l = sl; l <= el; l++)
                    {
                      const char *line = buffer_get_line (buf, l);
                      int len = strlen (line);
                      int s = (l == sl) ? sc : 0;
                      int e = (l == el) ? ec : len;
                      memcpy (p, &line[s], e - s);
                      p += e - s;
                      if (l < el)
                        *p++ = '\n';
                    }
                  *p = 0;
                  // delete range
                  if (buffer_delete_range (buf, sl, sc, el, ec) == 0)
                    {
                      // adjust cursor
                      *cursor_line = sl;
                      *cursor_col = sc;
                      *selection_active = 0;
                      clear_redo ();
                      // Note: Undo for multi-char delete not fully implemented in this simple system
                    }
                  else
                    {
                      error_occurred = 1;
                    }
                }
            }
          else
            {
              *clipboard = my_strdup (buffer_get_line (buf, *cursor_line));
              if (buffer_delete_line (buf, *cursor_line) != 0)
                {
                  error_occurred = 1;
                }
            }
          break;
        case 22:               // Ctrl+V to paste
          if (*clipboard)
            {
              if (buffer_insert_text
                  (buf, *cursor_line, *cursor_col, *clipboard) == 0)
                {
                  clear_redo ();
                  // move cursor to end
                  const char *p = *clipboard;
                  while (*p)
                    {
                      if (*p == '\n')
                        {
                          (*cursor_line)++;
                          *cursor_col = 0;
                        }
                      else
                        {
                          (*cursor_col)++;
                        }
                      p++;
                    }
                  // Note: Undo for multi-char insert not fully implemented in this simple system
                }
              else
                {
                  error_occurred = 1;
                }
            }
          break;
        case 31:               // Ctrl+/
          *search_mode = 1;
          search_buffer[0] = 0;
          break;
        case 1:                // Ctrl+A to select all
          *selection_start_line = 0;
          *selection_start_col = 0;
          *selection_end_line = buffer_num_lines (buf) - 1;
          *selection_end_col =
            buffer_get_line_length (buf, *selection_end_line);
          *selection_active = 1;
          break;
        default:
          if (ch == '\n' ||  ch == '\t' || (ch >= 32 && ch <= 126))
            {                   // Printable chars or newline
              if (ch == '\n')
                {
                  if (buffer_insert_char
                      (buf, *cursor_line, *cursor_col, '\n') == 0)
                    {
                      push_undo (true, *cursor_line, *cursor_col, '\n');
                      clear_redo ();
                      (*cursor_line)++;
                      *cursor_col = 0;
                    }
                  else
                    {
                      error_occurred = 1;
                    }
                }
              else if (ch == '\t')      // TAB
                {
                  if (buffer_insert_char
                      (buf, *cursor_line, *cursor_col, '\t') == 0)
                    {
                      push_undo (true, *cursor_line, *cursor_col, '\t');
                      clear_redo ();
                      (*cursor_col)++;
                    }
                  else
                    {
                      error_occurred = 1;
                    }
                }
              else
                {
                  if (buffer_insert_char
                      (buf, *cursor_line, *cursor_col, (char) ch) == 0)
                    {
                      push_undo (true, *cursor_line, *cursor_col, (char) ch);
                      clear_redo ();
                      (*cursor_col)++;
                    }
                  else
                    {
                      error_occurred = 1;
                    }
                }
            }
          break;
        }
    }
  if (*selection_active)
    {
      *selection_end_line = *cursor_line;
      *selection_end_col = *cursor_col;
    }
  // Adjust scroll
  if (*cursor_line < *scroll_row)
    *scroll_row = *cursor_line;
  else if (*cursor_line >= *scroll_row + (int) LINES - 2)
    *scroll_row = *cursor_line - (LINES - 3);
  if (*cursor_col < *scroll_col)
    *scroll_col = *cursor_col;
  else if (*cursor_col >= *scroll_col + (int) COLS - 2)
    *scroll_col = *cursor_col - (COLS - 3);
  if (*scroll_row > buffer_num_lines (buf) - ((int) LINES - 2))
    *scroll_row =
      buffer_num_lines (buf) >
      (int) LINES - 2 ? buffer_num_lines (buf) - (LINES - 2) : 0;
  return error_occurred ? -1 : 0;
}