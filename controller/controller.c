#include "controller.h"
#include "editor.h"
#include "view.h"
#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include <ctype.h>
#include <stdio.h>

static void
push_change (UndoStack *stack, bool is_insert, int line, int col, char ch)
{
  if (!stack)
    return;
  if (stack->count >= stack->capacity)
    {
      if (stack->capacity >= 10000)
        return;
      int new_capacity =
        stack->capacity == 0 ? 16 : stack->capacity * 2;
      Change *temp =
        xrealloc (stack->changes, new_capacity * sizeof (Change));
      if (!temp)
        return;
      stack->changes = temp;
      stack->capacity = new_capacity;
    }
  stack->changes[stack->count].is_insert = is_insert;
  stack->changes[stack->count].line = line;
  stack->changes[stack->count].col = col;
  stack->changes[stack->count].ch = ch;
  stack->count++;
}

void
push_undo (UndoStack *stack, bool is_insert, int line, int col, char ch)
{
  push_change (stack, is_insert, line, col, ch);
}

void
push_redo (UndoStack *stack, bool is_insert, int line, int col, char ch)
{
  push_change (stack, is_insert, line, col, ch);
}

void
undo_operation (Buffer *buf, UndoStack *undo, UndoStack *redo, int *cursor_line, int *cursor_col)
{
  if (!undo || undo->count <= 0)
    return;
  undo->count--;
  Change c = undo->changes[undo->count];
  push_redo (redo, c.is_insert, c.line, c.col, c.ch);
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

void
redo_operation (Buffer *buf, UndoStack *undo, UndoStack *redo, int *cursor_line, int *cursor_col)
{
  if (!redo || redo->count <= 0)
    return;
  redo->count--;
  Change c = redo->changes[redo->count];
  push_undo (undo, !c.is_insert, c.line, c.col, c.ch);
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

void
clear_redo (UndoStack *redo)
{
  if (!redo)
    return;
  free (redo->changes);
  redo->changes = NULL;
  redo->count = 0;
  redo->capacity = 0;
}

void
free_undo_stacks (UndoStack *undo, UndoStack *redo)
{
  if (undo)
    {
      free (undo->changes);
      undo->changes = NULL;
      undo->count = 0;
      undo->capacity = 0;
    }
  if (redo)
    {
      free (redo->changes);
      redo->changes = NULL;
      redo->count = 0;
      redo->capacity = 0;
    }
}

/* Legacy shim to keep older test code compiling during migration.
   New code should use free_undo_stacks or test_reset_undo(&ed). */
void
free_undo (void)
{
  /* no-op */
}

/* === Simple dispatch table (KISS) === */
typedef struct {
    Buffer   *buf;
    int      *scroll_row, *scroll_col;
    int      *cursor_line, *cursor_col;
    int      *show_line_numbers;
    char     *search_buffer;
    int      *search_mode;
    char    **clipboard;
    const char *filename;
    Editor   *ed;
} InputContext;

typedef struct {
    int  key;
    void (*handler)(int ch, InputContext *ctx);
} KeyHandler;

/* Forward declarations for handlers */
static void handle_left      (int ch, InputContext *ctx);
static void handle_right     (int ch, InputContext *ctx);
static void handle_up        (int ch, InputContext *ctx);
static void handle_down      (int ch, InputContext *ctx);
static void handle_home      (int ch, InputContext *ctx);
static void handle_end       (int ch, InputContext *ctx);
static void handle_ppage     (int ch, InputContext *ctx);
static void handle_npage     (int ch, InputContext *ctx);
static void handle_undo      (int ch, InputContext *ctx);
static void handle_redo      (int ch, InputContext *ctx);
static void handle_select_all(int ch, InputContext *ctx);
static void handle_copy      (int ch, InputContext *ctx);
static void handle_enter     (int ch, InputContext *ctx);
static void handle_cut       (int ch, InputContext *ctx);
static void handle_paste     (int ch, InputContext *ctx);
static void handle_save      (int ch, InputContext *ctx);
static void handle_backspace (int ch, InputContext *ctx);
static void handle_delete    (int ch, InputContext *ctx);
static void handle_tab       (int ch, InputContext *ctx);
static void handle_printable (int ch, InputContext *ctx);

static const KeyHandler key_table[] = {
    { KEY_LEFT,   handle_left },
    { KEY_RIGHT,  handle_right },
    { KEY_UP,     handle_up },
    { KEY_DOWN,   handle_down },
    { KEY_HOME,   handle_home },
    { KEY_END,    handle_end },
    { KEY_PPAGE,  handle_ppage },
    { KEY_NPAGE,  handle_npage },
    { 26,         handle_undo },        /* Ctrl+Z */
    { 25,         handle_redo },        /* Ctrl+Y */
    { 1,          handle_select_all },  /* Ctrl+A */
    { 3,          handle_copy },        /* Ctrl+C */
    { KEY_ENTER,  handle_enter },
    { 10,         handle_enter },
    { 13,         handle_enter },
    { 24,         handle_cut },         /* Ctrl+X */
    { 22,         handle_paste },       /* Ctrl+V */
    { 19,         handle_save },        /* Ctrl+S */
    { KEY_BACKSPACE, handle_backspace },
    { 127,        handle_backspace },  /* DEL / ^? — common terminal Backspace */
    { 8,          handle_backspace },  /* BS / Ctrl+H */
    { KEY_DC,     handle_delete },      /* Delete key */
    { 9,          handle_tab },         /* TAB */
    { 0,          NULL }                /* sentinel */
};

static void
dispatch_key (int ch, InputContext *ctx)
{
  for (int i = 0; key_table[i].handler != NULL; i++)
    {
      if (key_table[i].key == ch)
        {
          key_table[i].handler (ch, ctx);
          return;
        }
    }
  /* ASCII printables and UTF-8 continuation / lead bytes (128-255) */
  if ((ch >= 32 && ch <= 126) || (ch >= 128 && ch <= 255))
    handle_printable (ch, ctx);
}

/* === Handler implementations === */
static void handle_left (int ch, InputContext *ctx)
{
  (void)ch;
  if (*ctx->cursor_col > 0)
    (*ctx->cursor_col)--;
  else if (*ctx->cursor_line > 0)
    {
      (*ctx->cursor_line)--;
      *ctx->cursor_col = buffer_get_line_length (ctx->buf, *ctx->cursor_line);
    }
}

static void handle_right (int ch, InputContext *ctx)
{
  (void)ch;
  int len = buffer_get_line_length (ctx->buf, *ctx->cursor_line);
  if (*ctx->cursor_col < len)
    (*ctx->cursor_col)++;
  else if (*ctx->cursor_line < buffer_num_lines (ctx->buf) - 1)
    {
      (*ctx->cursor_line)++;
      *ctx->cursor_col = 0;
    }
}

static void handle_up (int ch, InputContext *ctx)
{
  (void)ch;
  if (*ctx->cursor_line > 0)
    {
      (*ctx->cursor_line)--;
      if (*ctx->cursor_col > buffer_get_line_length (ctx->buf, *ctx->cursor_line))
        *ctx->cursor_col = buffer_get_line_length (ctx->buf, *ctx->cursor_line);
    }
}

static void handle_down (int ch, InputContext *ctx)
{
  (void)ch;
  if (*ctx->cursor_line < buffer_num_lines (ctx->buf) - 1)
    {
      (*ctx->cursor_line)++;
      if (*ctx->cursor_col > buffer_get_line_length (ctx->buf, *ctx->cursor_line))
        *ctx->cursor_col = buffer_get_line_length (ctx->buf, *ctx->cursor_line);
    }
}

static void handle_home (int ch, InputContext *ctx)
{
  (void)ch;
  if (ctx->ed && ctx->ed->prev_key == KEY_HOME)
    {
      *ctx->cursor_line = 0;
      *ctx->cursor_col = 0;
      *ctx->scroll_row = 0;
      *ctx->scroll_col = 0;
    }
  else
    *ctx->cursor_col = 0;
}

static void handle_end (int ch, InputContext *ctx)
{
  (void)ch;
  if (ctx->ed && ctx->ed->prev_key == KEY_END)
    {
      *ctx->cursor_line = buffer_num_lines (ctx->buf) - 1;
      *ctx->cursor_col = buffer_get_line_length (ctx->buf, *ctx->cursor_line);
      *ctx->scroll_row = *ctx->cursor_line > 5 ? *ctx->cursor_line - 5 : 0;
    }
  else
    *ctx->cursor_col = buffer_get_line_length (ctx->buf, *ctx->cursor_line);
}

static void handle_ppage (int ch, InputContext *ctx)
{
  (void)ch;
  *ctx->cursor_line -= (LINES > 5 ? LINES - 3 : 5);
  if (*ctx->cursor_line < 0) *ctx->cursor_line = 0;
  *ctx->cursor_col = 0;
  if (*ctx->scroll_row > *ctx->cursor_line) *ctx->scroll_row = *ctx->cursor_line;
}

static void handle_npage (int ch, InputContext *ctx)
{
  (void)ch;
  *ctx->cursor_line += (LINES > 5 ? LINES - 3 : 5);
  if (*ctx->cursor_line >= buffer_num_lines (ctx->buf))
    *ctx->cursor_line = buffer_num_lines (ctx->buf) - 1;
  *ctx->cursor_col = 0;
}

static void handle_undo (int ch, InputContext *ctx)
{
  (void)ch;
  if (ctx->ed)
    undo_operation (ctx->buf, &ctx->ed->undo_stack, &ctx->ed->redo_stack, ctx->cursor_line, ctx->cursor_col);
}

static void handle_redo (int ch, InputContext *ctx)
{
  (void)ch;
  if (ctx->ed)
    redo_operation (ctx->buf, &ctx->ed->undo_stack, &ctx->ed->redo_stack, ctx->cursor_line, ctx->cursor_col);
}

static void handle_select_all (int ch, InputContext *ctx)
{
  (void)ch;
  if (ctx->ed)
    {
      ctx->ed->selection_active = 1;
      ctx->ed->selection_start_line = 0;
      ctx->ed->selection_start_col = 0;
      ctx->ed->selection_end_line = buffer_num_lines (ctx->buf) - 1;
      ctx->ed->selection_end_col = buffer_get_line_length (ctx->buf, ctx->ed->selection_end_line);
      *ctx->cursor_line = ctx->ed->selection_end_line;
      *ctx->cursor_col = ctx->ed->selection_end_col;
    }
}

static void handle_enter (int ch, InputContext *ctx)
{
  (void)ch;
  if (ctx->ed)
    {
      push_undo (&ctx->ed->undo_stack, true, *ctx->cursor_line, *ctx->cursor_col, '\n');
      clear_redo (&ctx->ed->redo_stack);
    }
  buffer_insert_char (ctx->buf, *ctx->cursor_line, *ctx->cursor_col, '\n');
  (*ctx->cursor_line)++;
  *ctx->cursor_col = 0;
}

static void handle_cut (int ch, InputContext *ctx)
{
  (void)ch;
  int row = *ctx->cursor_line;
  int col = *ctx->cursor_col;
  int linelen = buffer_get_line_length (ctx->buf, row);
  if (col < linelen)
    {
      char *linecontent = buffer_get_line (ctx->buf, row);
      if (linecontent)
        {
          size_t textlen = strlen (linecontent + col);
          if (*ctx->clipboard) free (*ctx->clipboard);
          *ctx->clipboard = xmalloc (textlen + 1);
          if (*ctx->clipboard) strcpy (*ctx->clipboard, linecontent + col);
          free (linecontent);
          if (ctx->ed)
            {
              push_undo (&ctx->ed->undo_stack, false, row, col, 0);
              clear_redo (&ctx->ed->redo_stack);
            }
          buffer_delete_range (ctx->buf, row, col, row, linelen);
        }
    }
}

static void handle_copy (int ch, InputContext *ctx)
{
  (void)ch;
  int row = *ctx->cursor_line;
  int col = *ctx->cursor_col;
  int linelen = buffer_get_line_length (ctx->buf, row);
  if (col < linelen)
    {
      char *linecontent = buffer_get_line (ctx->buf, row);
      if (linecontent)
        {
          size_t textlen = strlen (linecontent + col);
          if (*ctx->clipboard) free (*ctx->clipboard);
          *ctx->clipboard = xmalloc (textlen + 1);
          if (*ctx->clipboard) strcpy (*ctx->clipboard, linecontent + col);
          free (linecontent);
        }
    }
}

static void handle_paste (int ch, InputContext *ctx)
{
  (void)ch;
  if (*ctx->clipboard && **ctx->clipboard)
    {
      buffer_insert_text (ctx->buf, *ctx->cursor_line, *ctx->cursor_col, *ctx->clipboard);
      *ctx->cursor_col += strlen (*ctx->clipboard);
    }
}

static void handle_save (int ch, InputContext *ctx)
{
  (void)ch;
  if (ctx->filename) buffer_save_to_file (ctx->buf, ctx->filename);
}

static void handle_backspace (int ch, InputContext *ctx)
{
  (void)ch;
  if (*ctx->cursor_col > 0)
    {
      char deleted = buffer_get_char (ctx->buf, *ctx->cursor_line, *ctx->cursor_col - 1);
      if (ctx->ed)
        {
          push_undo (&ctx->ed->undo_stack, false, *ctx->cursor_line, *ctx->cursor_col - 1, deleted);
          clear_redo (&ctx->ed->redo_stack);
        }
      buffer_delete_char (ctx->buf, *ctx->cursor_line, *ctx->cursor_col - 1);
      (*ctx->cursor_col)--;
    }
  else if (*ctx->cursor_line > 0)
    {
      int prev = *ctx->cursor_line - 1;
      int prevlen = buffer_get_line_length (ctx->buf, prev);
      *ctx->cursor_line = prev;
      *ctx->cursor_col = prevlen;
      if (ctx->ed)
        {
          push_undo (&ctx->ed->undo_stack, false, prev, prevlen, '\n');
          clear_redo (&ctx->ed->redo_stack);
        }
      buffer_delete_char (ctx->buf, prev, prevlen);
    }
}

static void handle_delete (int ch, InputContext *ctx)
{
  (void)ch;
  int len = buffer_get_line_length (ctx->buf, *ctx->cursor_line);
  if (*ctx->cursor_col < len)
    {
      // Delete character at cursor (forward)
      char deleted = buffer_get_char (ctx->buf, *ctx->cursor_line, *ctx->cursor_col);
      if (ctx->ed)
        {
          push_undo (&ctx->ed->undo_stack, false, *ctx->cursor_line, *ctx->cursor_col, deleted);
          clear_redo (&ctx->ed->redo_stack);
        }
      buffer_delete_char (ctx->buf, *ctx->cursor_line, *ctx->cursor_col);
      // cursor position does not move
    }
  else if (*ctx->cursor_line < buffer_num_lines (ctx->buf) - 1)
    {
      // At end of line: delete the newline (merge next line into this one)
      // buffer_delete_char at col == len already handles the merge
      int curr_line = *ctx->cursor_line;
      if (ctx->ed)
        {
          push_undo (&ctx->ed->undo_stack, false, curr_line, len, '\n');
          clear_redo (&ctx->ed->redo_stack);
        }
      buffer_delete_char (ctx->buf, curr_line, len);
      // cursor stays at the join point (old end of line)
    }
}

static void handle_tab (int ch, InputContext *ctx)
{
  (void)ch;
  if (ctx->ed && ctx->ed->config.display.tab_width == 0) return;
  if (ctx->ed && ctx->ed->config.display.spaces_for_tab)
    {
      char *line = buffer_get_line (ctx->buf, *ctx->cursor_line);
      if (line)
        {
          int line_len = strlen (line);
          int current_vis = visual_column (line, line_len, *ctx->cursor_col,
                                           ctx->ed->config.display.tab_width);
          int tabw = ctx->ed->config.display.tab_width;
          int spaces = tabw - (current_vis % tabw);
          if (spaces == 0) spaces = tabw;
          for (int i = 0; i < spaces; i++)
            {
              if (ctx->ed)
                push_undo (&ctx->ed->undo_stack, true, *ctx->cursor_line, *ctx->cursor_col, ' ');
              buffer_insert_char (ctx->buf, *ctx->cursor_line, *ctx->cursor_col, ' ');
              (*ctx->cursor_col)++;
            }
          free (line);
        }
    }
    else
    {
      if (ctx->ed)
        {
          push_undo (&ctx->ed->undo_stack, true, *ctx->cursor_line, *ctx->cursor_col, '\t');
          clear_redo (&ctx->ed->redo_stack);
        }
      buffer_insert_char (ctx->buf, *ctx->cursor_line, *ctx->cursor_col, '\t');
      (*ctx->cursor_col)++;
    }
}

static void handle_printable (int ch, InputContext *ctx)
{
  buffer_insert_char (ctx->buf, *ctx->cursor_line, *ctx->cursor_col, (char) ch);
  if (ctx->ed)
    {
      push_undo (&ctx->ed->undo_stack, true, *ctx->cursor_line, *ctx->cursor_col, (char) ch);
      clear_redo (&ctx->ed->redo_stack);
    }
  (*ctx->cursor_col)++;
}
/* === End of dispatch table === */

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
      else if ((ch >= 32 && ch <= 126) || (ch >= 128 && ch <= 255))
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
      InputContext ctx = {
        .buf = buf,
        .scroll_row = scroll_row,
        .scroll_col = scroll_col,
        .cursor_line = cursor_line,
        .cursor_col = cursor_col,
        .show_line_numbers = show_line_numbers,
        .search_buffer = search_buffer,
        .search_mode = search_mode,
        .clipboard = clipboard,
        .filename = filename,
        .ed = ed
      };
      dispatch_key (ch, &ctx);
    }
  // Clamp cursor after input
  {
    int clen = buffer_get_line_length (buf, *cursor_line);
    if (*cursor_col > clen)
      {
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
      while (regexec (&regex, line + pos, 1, &match, line_flags) == 0
             && iters++ < 1000)
        {
          if (match.rm_eo == 0)
            {
              pos++;
              if (pos >= len)
                break;
              continue;
            }
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
