#include "controller.h"
#include "editor.h"
#include "view.h"
#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include <ctype.h>
#include <stdio.h>
UndoStack undo_stack = { NULL, 0, 0 };
UndoStack redo_stack = { NULL, 0, 0 };

static char lineclip[400];

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
      if (undo_stack.capacity >= 10000)
        return;                 // Cap at 10,000 operations
      int new_capacity =
        undo_stack.capacity == 0 ? 16 : undo_stack.capacity * 2;
      Change *temp =
        realloc (undo_stack.changes, new_capacity * sizeof (Change));
      if (!temp)
        return;                 // Handle error
      undo_stack.changes = temp;
      undo_stack.capacity = new_capacity;
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
      if (redo_stack.capacity >= 10000)
        return;                 // Cap at 10,000 operations
      int new_capacity =
        redo_stack.capacity == 0 ? 16 : redo_stack.capacity * 2;
      Change *temp =
        realloc (redo_stack.changes, new_capacity * sizeof (Change));
      if (!temp)
        return;                 // Handle error
      redo_stack.changes = temp;
      redo_stack.capacity = new_capacity;
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
          if (c.ch == '\n')
            {
              // Merging lines, adjust cursor if it was on the deleted line
              if (*cursor_line > c.line)
                {
                  (*cursor_line)--;
                  *cursor_col += c.col;
                }
              else if (*cursor_line == c.line && *cursor_col > c.col)
                (*cursor_col)--;
            }
          else
            {
              if (*cursor_line == c.line && *cursor_col > c.col)
                (*cursor_col)--;
            }
        }
      else
        {
          buffer_insert_char (buf, c.line, c.col, c.ch);
          if (c.ch == '\n')
            {
              // Splitting lines, move cursor to new line if after insert
              if (*cursor_line == c.line && *cursor_col > c.col)
                {
                  (*cursor_line)++;
                  *cursor_col -= c.col;
                }
            }
          else
            {
              if (*cursor_line == c.line && *cursor_col >= c.col)
                (*cursor_col)++;
            }
        }
      int len = buffer_get_line_length (buf, *cursor_line);
      if (*cursor_col > len)
        *cursor_col = len;
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
          if (c.ch == '\n')
            {
              // Splitting lines, move cursor to new line if after insert
              if (*cursor_line == c.line && *cursor_col > c.col)
                {
                  (*cursor_line)++;
                  *cursor_col -= c.col;
                }
            }
          else
            {
              if (*cursor_line == c.line && *cursor_col >= c.col)
                (*cursor_col)++;
            }
        }
      else
        {
          buffer_delete_char (buf, c.line, c.col);
          if (c.ch == '\n')
            {
              // Merging lines, adjust cursor if it was on the deleted line
              if (*cursor_line > c.line)
                {
                  (*cursor_line)--;
                  *cursor_col += c.col;
                }
              else if (*cursor_line == c.line && *cursor_col > c.col)
                (*cursor_col)--;
            }
          else
            {
              if (*cursor_line == c.line && *cursor_col > c.col)
                (*cursor_col)--;
            }
        }
      int len = buffer_get_line_length (buf, *cursor_line);
      if (*cursor_col > len)
        *cursor_col = len;
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
  if (strlen (pattern) > 100)
    {
      return;                   // Pattern too long
    }
  regex_t regex;
  if (regcomp (&regex, pattern, REG_EXTENDED) != 0)
    return;
  int found = 0;
  int clamped = 0;
  for (int l = *cursor_line;
       l < buffer_num_lines (buf) && !found && (!clamped
                                                || l == *cursor_line); l++)
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
		sched_yield();




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
               char *search_buffer, int *search_mode, char *clipboard,
               const char *filename, Editor *ed)
{
  int error_occurred = 0;
  if (*search_mode)
    {
      if (ch == '\n' || ch == 13 || ch == KEY_ENTER)
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
          int len = buffer_get_line_length (buf, *cursor_line);
          if (*cursor_col < len)
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
          if (*cursor_line > 0)
            {
              (*cursor_line)--;
              if (*cursor_col > buffer_get_line_length (buf, *cursor_line))
                {
                  *cursor_col = buffer_get_line_length (buf, *cursor_line);
                }
            }
          break;
        case KEY_DOWN:
          if (*cursor_line < buffer_num_lines (buf) - 1)
            {
              (*cursor_line)++;
              if (*cursor_col > buffer_get_line_length (buf, *cursor_line))
                {
                  *cursor_col = buffer_get_line_length (buf, *cursor_line);
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
          if (*cursor_col > 0)
            {
              (*cursor_col)--;
            }
          else if (*cursor_line > 0)
            {
              (*cursor_line)--;
              *cursor_col = buffer_get_line_length (buf, *cursor_line);
            }
	    char backspaced = buffer_delete_char(buf,*cursor_line,*cursor_col);
	    push_undo(false, *cursor_line, *cursor_col, backspaced);
	    clear_redo();
	    break;
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
                  int new_len = buffer_get_line_length (buf, *cursor_line);
                  if (*cursor_col > new_len)
                    *cursor_col = new_len;
                }
              else
                {
                  error_occurred = 1;
                }
            }
          else if (*cursor_line > 0)
            {
              int prev_len = buffer_get_line_length (buf, *cursor_line - 1);
              if (buffer_delete_char (buf, *cursor_line - 1, prev_len) == 0)
                {
                  push_undo (false, *cursor_line - 1, prev_len, '\n');
                  clear_redo ();
                  (*cursor_line)--;
                  *cursor_col = prev_len;
                  int new_len = buffer_get_line_length (buf, *cursor_line);
                  if (*cursor_col > new_len)
                    *cursor_col = new_len;
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
                  int new_len = buffer_get_line_length (buf, *cursor_line);
                  if (*cursor_col > new_len)
                    *cursor_col = new_len;
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
                  int new_len = buffer_get_line_length (buf, *cursor_line);
                  if (*cursor_col > new_len)
                    *cursor_col = new_len;
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
        case 26:
          undo_operation (buf, cursor_line, cursor_col);
          break;
         case 3:
           strcpy (lineclip, buffer_get_line (buf, *cursor_line));
           if (clipboard) strcpy (clipboard, lineclip);
           break;
         case 24:
           if (buffer_num_lines (buf) == 0) {
             buffer_insert_line (buf, 0, "");
             *cursor_line = 0;
             *cursor_col = 0;
             strcpy(lineclip,""); 
             if (clipboard) strcpy(clipboard, "");
           } else {
             if (*cursor_line >= buffer_num_lines (buf)) {
               *cursor_line = buffer_num_lines (buf) - 1;
             }
             strcpy (lineclip,buffer_get_line (buf, *cursor_line));
             if (clipboard) strcpy (clipboard, lineclip);
             int len = buffer_get_line_length (buf, *cursor_line);
             if (len > 0) {
               buffer_delete_range (buf, *cursor_line, 0, *cursor_line, len);
             }
             *cursor_col = 0;
           }
           clear_redo ();
           break;
         case 22:
           {
             char *clip = (clipboard && strlen(clipboard) > 0) ? clipboard : lineclip;
             if (strlen(clip) > 0) {
               if (buffer_insert_text (buf, *cursor_line, *cursor_col, clip) == 0) {
                 clear_redo ();
                 const char *p = clip;
                 while (*p) {
                   if (*p == '\n') {
                     (*cursor_line)++;
                     *cursor_col = 0;
                   } else {
                     (*cursor_col)++;
                   }
                   p++;
                 }
               } else {
                 error_occurred = 1;
               }
             }
           }
           break;
         case 31:
           if (ed && ed->config.search.enabled)
             {
               *search_mode = 1;
               search_buffer[0] = 0;
             }
           break;
        default:
          if (ch == '\n' || ch == KEY_ENTER || ch == '\t'
              || (ch >= 32 && ch <= 126))
            {
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
    return true;
}
