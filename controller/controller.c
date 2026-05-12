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
        return;
      int new_capacity =
        undo_stack.capacity == 0 ? 16 : undo_stack.capacity * 2;
      Change *temp =
        xrealloc (undo_stack.changes, new_capacity * sizeof (Change));
      if (!temp)
        return;
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
        return;
      int new_capacity =
        redo_stack.capacity == 0 ? 16 : redo_stack.capacity * 2;
      Change *temp =
        xrealloc (redo_stack.changes, new_capacity * sizeof (Change));
      if (!temp)
        return;
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
      push_redo (c.is_insert, c.line, c.col, c.ch);
      if (c.is_insert)
        {
          buffer_delete_char (buf, c.line, c.col);
          if (c.ch == '\n')
            {
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
              if (*cursor_line == c.line && *cursor_col >= c.col)
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
      push_undo (!c.is_insert, c.line, c.col, c.ch);
      if (c.is_insert)
        {
          buffer_insert_char (buf, c.line, c.col, c.ch);
          if (c.ch == '\n')
            {
              if (*cursor_line == c.line && *cursor_col >= c.col)
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

int
handle_input (int ch, Buffer *buf, int *scroll_row, int *scroll_col,
              int *cursor_line, int *cursor_col, int *show_line_numbers,
              char *search_buffer, int *search_mode, char **clipboard,
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
        }
      else if (ch == 27)
        {
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
          if (ed && ed->prev_key == KEY_HOME) {
            /* double home: top of file */
            *cursor_line = 0;
            *cursor_col = 0;
            *scroll_row = 0;
            *scroll_col = 0;
          } else {
            *cursor_col = 0;
          }
          break;
        case KEY_END:
          if (ed && ed->prev_key == KEY_END) {
            /* double end: bottom of file */
            *cursor_line = buffer_num_lines (buf) - 1;
            *cursor_col = buffer_get_line_length (buf, *cursor_line);
            *scroll_row = *cursor_line > 5 ? *cursor_line - 5 : 0;
          } else {
            *cursor_col = buffer_get_line_length (buf, *cursor_line);
          }
          break;
        case KEY_PPAGE:
          *cursor_line -= (LINES > 5 ? LINES - 3 : 5);
          if (*cursor_line < 0)
            *cursor_line = 0;
          *cursor_col = 0;
          if (*scroll_row > *cursor_line)
            *scroll_row = *cursor_line;
          break;
        case KEY_NPAGE:
          *cursor_line += (LINES > 5 ? LINES - 3 : 5);
          if (*cursor_line >= buffer_num_lines (buf))
            *cursor_line = buffer_num_lines (buf) - 1;
          *cursor_col = 0;
          break;
        case 26: /* Ctrl+Z */
          undo_operation (buf, cursor_line, cursor_col);
          break;
        case 25: /* Ctrl+Y */
          redo_operation (buf, cursor_line, cursor_col);
          break;
        case 1: /* Ctrl+A select all */
          if (ed) {
            ed->selection_active = 1;
            ed->selection_start_line = 0;
            ed->selection_start_col = 0;
            ed->selection_end_line = buffer_num_lines(buf) - 1;
            ed->selection_end_col = buffer_get_line_length(buf, ed->selection_end_line);
            *cursor_line = ed->selection_end_line;
            *cursor_col = ed->selection_end_col;
          }
          break;

        case KEY_ENTER:
        case 10:
        case 13:
          push_undo(true, *cursor_line, *cursor_col, '\n');
          buffer_insert_char(buf, *cursor_line, *cursor_col, '\n');
          *cursor_line += 1;
          *cursor_col = 0;
          clear_redo();
          break;
        case 24: /* Ctrl-X cut to EOL */
          {
            int row = *cursor_line;
            int col = *cursor_col;
            int linelen = buffer_get_line_length(buf, row);
            if (col < linelen) {
              char *linecontent = buffer_get_line(buf, row);
              if (linecontent) {
                size_t textlen = strlen(linecontent + col);
                if (*clipboard) free(*clipboard);
                *clipboard = xmalloc(textlen + 1);
                if (*clipboard) {
                  strcpy(*clipboard, linecontent + col);
                }
                free(linecontent);
                /* record for undo */
                push_undo(false, row, col, 0); /* simplified */
                buffer_delete_range(buf, row, col, row, linelen);
                clear_redo();
              }
            }
          }
          break;
        case 22: /* Ctrl-V paste */
if (*clipboard && **clipboard) {
            buffer_insert_text (buf, *cursor_line, *cursor_col, *clipboard);
            *cursor_col += strlen (*clipboard);
          }
          break;
        case KEY_BACKSPACE:
        case 127:
        case 8:
          if (*cursor_col > 0) {
            char deleted = buffer_get_char(buf, *cursor_line, *cursor_col - 1);
            push_undo(false, *cursor_line, *cursor_col - 1, deleted);
            buffer_delete_char(buf, *cursor_line, *cursor_col - 1);
            (*cursor_col)--;
            clear_redo();
          } else if (*cursor_line > 0) {
            int prev = *cursor_line - 1;
            int prevlen = buffer_get_line_length(buf, prev);
            *cursor_line = prev;
            *cursor_col = prevlen;
            push_undo(false, prev, prevlen, '\n');
            buffer_delete_char(buf, prev, prevlen);
            clear_redo();
          }
          break;
        default:
          if (ch >= 32 && ch <= 126) {
            buffer_insert_char(buf, *cursor_line, *cursor_col, (char)ch);
            push_undo(true, *cursor_line, *cursor_col, (char)ch);
            (*cursor_col)++;
            clear_redo();
          }
          break;
        }
    }
  // Clamp cursor after input
  {
    int clen = buffer_get_line_length(buf, *cursor_line);
    if (*cursor_col > clen) {
      *cursor_col = clen;
    }
  }
  return error_occurred ? -1 : 0;
}

void
search_next (Buffer *buf, int *cursor_line, int *cursor_col,
             const char *pattern)
{
  if (strlen (pattern) > 100)
    return;
  regex_t regex;
  if (regcomp (&regex, pattern, REG_EXTENDED) != 0)
    return;

  int found = 0;
  int clamped = 0;
  int iters = 0;
  for (int l = *cursor_line;
       l < buffer_num_lines (buf) && !found && (!clamped || l == *cursor_line);
       l++) {
    const char *line = buffer_get_line (buf, l);
    int len = strlen (line);
    if (l == *cursor_line && *cursor_col > len) {
      *cursor_col = len;
      clamped = 1;
    }

    regmatch_t match;
    int pos = (l == *cursor_line) ? *cursor_col : 0;
    int line_flags = (pos > 0) ? REG_NOTBOL : 0;
    while (regexec (&regex, line + pos, 1, &match, line_flags) == 0 && iters++ < 1000) {
      if (match.rm_eo == 0) {
        pos++;
        if (pos >= len) break;
        continue;
      }
      if (match.rm_so == 0 && pos == *cursor_col && l == *cursor_line) {
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
