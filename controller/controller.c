#include "controller.h"
#include "editor.h"
#include "view.h"
#include "utils.h"
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
static void handle_word_left (int ch, InputContext *ctx);
static void handle_word_right(int ch, InputContext *ctx);
static void handle_delete_word(int ch, InputContext *ctx);
static void handle_match_brace(int ch, InputContext *ctx);
static void handle_comment   (int ch, InputContext *ctx);
static void handle_outdent   (int ch, InputContext *ctx);

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
    { 11,         handle_comment },     /* Ctrl+K toggle // comment */
    { 23,         handle_delete_word }, /* Ctrl+W delete word backward */
    { 29,         handle_match_brace }, /* Ctrl+] matching brace */
    { KEY_BTAB,   handle_outdent },     /* Shift-Tab outdent */
    { 0,          NULL }                /* sentinel */
};

static void
dispatch_key (int ch, InputContext *ctx)
{
  if (ctx->ed && ctx->ed->key_word_left > 0 && ch == ctx->ed->key_word_left)
    {
      handle_word_left (ch, ctx);
      return;
    }
  if (ctx->ed && ctx->ed->key_word_right > 0 && ch == ctx->ed->key_word_right)
    {
      handle_word_right (ch, ctx);
      return;
    }
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

static void
insert_with_undo (InputContext *ctx, char c)
{
  if (ctx->ed)
    push_undo (&ctx->ed->undo_stack, true, *ctx->cursor_line, *ctx->cursor_col, c);
  buffer_insert_char (ctx->buf, *ctx->cursor_line, *ctx->cursor_col, c);
  (*ctx->cursor_col)++;
}

static void
insert_indent_unit (InputContext *ctx)
{
  if (!ctx->ed)
    return;
  int tabw = ctx->ed->config.display.tab_width;
  if (ctx->ed->config.display.spaces_for_tab)
    {
      if (tabw < 1)
        return;
      for (int i = 0; i < tabw; i++)
        insert_with_undo (ctx, ' ');
    }
  else if (tabw != 0)
    insert_with_undo (ctx, '\t');
}

static void handle_enter (int ch, InputContext *ctx)
{
  (void)ch;
  int row = *ctx->cursor_line;
  int col = *ctx->cursor_col;
  int len = buffer_get_line_length (ctx->buf, row);
  /* Auto-indent only when breaking at the end of a line. A mid-line split
   * stays a plain split so existing text is not shoved sideways. */
  int do_indent = (ctx->ed && col == len);
  char indent[1024];
  int indent_n = 0;
  int extra = 0;
  if (do_indent)
    {
      char *line = buffer_get_line (ctx->buf, row);
      if (line)
        {
          while (indent_n < len && indent_n < (int) sizeof indent
                 && (line[indent_n] == ' ' || line[indent_n] == '\t'))
            {
              indent[indent_n] = line[indent_n];
              indent_n++;
            }
          int end = len;
          while (end > 0 && (line[end - 1] == ' ' || line[end - 1] == '\t'))
            end--;
          if (end > 0 && (line[end - 1] == '{' || line[end - 1] == '('))
            extra = 1;
          free (line);
        }
    }
  if (ctx->ed)
    {
      push_undo (&ctx->ed->undo_stack, true, row, col, '\n');
      clear_redo (&ctx->ed->redo_stack);
    }
  buffer_insert_char (ctx->buf, row, col, '\n');
  (*ctx->cursor_line)++;
  *ctx->cursor_col = 0;
  if (do_indent)
    {
      for (int i = 0; i < indent_n; i++)
        insert_with_undo (ctx, indent[i]);
      if (extra)
        insert_indent_unit (ctx);
    }
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
      char *filtered = border_filter_dup (*ctx->clipboard);
      const char *to_insert = filtered ? filtered : *ctx->clipboard;
      buffer_insert_text (ctx->buf, *ctx->cursor_line, *ctx->cursor_col, to_insert);
      *ctx->cursor_col += strlen (to_insert);
      if (filtered) free (filtered);
    }
}

static void handle_save (int ch, InputContext *ctx)
{
  (void)ch;
  if (!ctx->filename)
    {
      if (ctx->ed)
        set_status_message (ctx->ed, "No file name");
      return;
    }
  if (buffer_save_to_file (ctx->buf, ctx->filename) != 0)
    {
      if (ctx->ed)
        set_status_message (ctx->ed, "Save failed");
      return;
    }
  if (ctx->ed)
    {
      ctx->ed->file_modified = 0;
      ctx->ed->unsaved_keystrokes = 0;
      set_status_message (ctx->ed, "File saved");
    }
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
      clear_redo (&ctx->ed->redo_stack);
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

static int
is_word_char (unsigned char c)
{
  return isalnum (c) || c == '_';
}

static int
step_forward (Buffer *buf, int *line, int *col)
{
  int len = buffer_get_line_length (buf, *line);
  if (*col < len)
    {
      (*col)++;
      return 1;
    }
  if (*line + 1 < buffer_num_lines (buf))
    {
      (*line)++;
      *col = 0;
      return 1;
    }
  return 0;
}

static int
step_back (Buffer *buf, int *line, int *col)
{
  if (*col > 0)
    {
      (*col)--;
      return 1;
    }
  if (*line > 0)
    {
      (*line)--;
      *col = buffer_get_line_length (buf, *line);
      return 1;
    }
  return 0;
}

static int
word_at (Buffer *buf, int line, int col)
{
  int len = buffer_get_line_length (buf, line);
  if (col < 0 || col >= len)
    return 0;
  return is_word_char ((unsigned char) buffer_get_char (buf, line, col));
}

static void
word_forward (Buffer *buf, int *cursor_line, int *cursor_col)
{
  int line = *cursor_line;
  int col = *cursor_col;
  if (line < 0 || line >= buffer_num_lines (buf))
    return;
  if (word_at (buf, line, col))
    {
      while (word_at (buf, line, col))
        col++;
    }
  for (;;)
    {
      if (word_at (buf, line, col))
        break;
      if (!step_forward (buf, &line, &col))
        break;
    }
  *cursor_line = line;
  *cursor_col = col;
}

static void
word_backward (Buffer *buf, int *cursor_line, int *cursor_col)
{
  int line = *cursor_line;
  int col = *cursor_col;
  if (line < 0 || line >= buffer_num_lines (buf))
    return;
  if (!step_back (buf, &line, &col))
    return;
  while (!word_at (buf, line, col))
    {
      if (!step_back (buf, &line, &col))
        {
          *cursor_line = line;
          *cursor_col = col;
          return;
        }
    }
  while (word_at (buf, line, col))
    {
      if (!step_back (buf, &line, &col))
        {
          *cursor_line = line;
          *cursor_col = col;
          return;
        }
    }
  step_forward (buf, &line, &col);
  *cursor_line = line;
  *cursor_col = col;
}

static void
handle_word_left (int ch, InputContext *ctx)
{
  (void) ch;
  word_backward (ctx->buf, ctx->cursor_line, ctx->cursor_col);
}

static void
handle_word_right (int ch, InputContext *ctx)
{
  (void) ch;
  word_forward (ctx->buf, ctx->cursor_line, ctx->cursor_col);
}

static void
handle_delete_word (int ch, InputContext *ctx)
{
  (void) ch;
  int tline = *ctx->cursor_line;
  int tcol = *ctx->cursor_col;
  word_backward (ctx->buf, &tline, &tcol);
  if (tline == *ctx->cursor_line && tcol == *ctx->cursor_col)
    return;
  if (ctx->ed)
    clear_redo (&ctx->ed->redo_stack);
  int guard = 0;
  while ((*ctx->cursor_line != tline || *ctx->cursor_col != tcol)
         && guard++ < 100000)
    handle_backspace (0, ctx);
}

static int
brace_partner (char c)
{
  switch (c)
    {
    case '(': return ')';
    case ')': return '(';
    case '[': return ']';
    case ']': return '[';
    case '{': return '}';
    case '}': return '{';
    default: return 0;
    }
}

static int
is_opener (char c)
{
  return c == '(' || c == '[' || c == '{';
}

/* col points at a double quote. True for R, u8R, uR, UR, LR. */
static int
raw_prefix_at (const char *s, int col)
{
  if (col < 1 || s[col - 1] != 'R')
    return 0;
  int start = col - 1;
  while (start > 0
         && (isalnum ((unsigned char) s[start - 1]) || s[start - 1] == '_'))
    start--;
  int n = col - start;
  const char *p = s + start;
  if (n == 1 && p[0] == 'R')
    return 1;
  if (n == 2 && p[1] == 'R' && (p[0] == 'u' || p[0] == 'U' || p[0] == 'L'))
    return 1;
  if (n == 3 && p[0] == 'u' && p[1] == '8' && p[2] == 'R')
    return 1;
  return 0;
}

enum {
  SC_NORM = 0,
  SC_LINE,
  SC_BLOCK,
  SC_SQ,
  SC_DQ,
  SC_RAW
};

typedef struct {
  int state;
  int escape;
  char raw_close[20];
  int raw_close_len;
  int raw_pos;
} ScanSt;

/* On success, *resume is the index just after the opening '('. */
static int
try_enter_raw (ScanSt *st, const char *s, int len, int col, int *resume)
{
  if (!raw_prefix_at (s, col))
    return 0;
  int i = col + 1;
  char delim[16];
  int dlen = 0;
  while (i < len && s[i] != '(')
    {
      unsigned char c = (unsigned char) s[i];
      if (dlen >= 16 || c == ')' || c == '\\' || c == '"' || isspace (c))
        return 0;
      delim[dlen++] = (char) c;
      i++;
    }
  if (i >= len || s[i] != '(')
    return 0;
  st->raw_close[0] = ')';
  memcpy (st->raw_close + 1, delim, (size_t) dlen);
  st->raw_close[1 + dlen] = '"';
  st->raw_close_len = dlen + 2;
  st->raw_pos = 0;
  st->state = SC_RAW;
  st->escape = 0;
  *resume = i + 1;
  return 1;
}

typedef struct {
  int line;
  int col;
  char ch;
} BraceFrame;

/* Jump between () [] {}. Strings, comments, and C++ raw strings are skipped.
 * Returns 1 if the cursor moved. */
static int
jump_brace (Buffer *buf, int *cursor_line, int *cursor_col)
{
  int nlines = buffer_num_lines (buf);
  if (nlines <= 0)
    return 0;
  int tline = *cursor_line;
  int tcol = *cursor_col;
  if (tline < 0 || tline >= nlines)
    return 0;

  char *ttext = buffer_get_line (buf, tline);
  if (!ttext)
    return 0;
  int tlen = (int) strlen (ttext);
  char target = 0;
  if (tcol >= 0 && tcol < tlen && brace_partner (ttext[tcol]))
    target = ttext[tcol];
  else if (tcol > 0 && brace_partner (ttext[tcol - 1]))
    {
      tcol--;
      target = ttext[tcol];
    }
  free (ttext);
  if (!target)
    return 0;

  BraceFrame stack[4096];
  int top = 0;
  int watch = -1;
  int stop = 0;
  ScanSt st;
  memset (&st, 0, sizeof st);
  int found = 0;
  int mline = 0;
  int mcol = 0;

  for (int line = 0; line < nlines && !found; line++)
    {
      char *s = buffer_get_line (buf, line);
      if (!s)
        continue;
      int len = (int) strlen (s);
      int i = 0;
      int cont = 0;
      while (i < len && !found)
        {
          char c = s[i];
          if (st.state == SC_LINE)
            break;
          if (st.state == SC_BLOCK)
            {
              if (c == '*' && i + 1 < len && s[i + 1] == '/')
                {
                  st.state = SC_NORM;
                  i += 2;
                  continue;
                }
              i++;
              continue;
            }
          if (st.state == SC_RAW)
            {
              if (st.raw_pos < st.raw_close_len && c == st.raw_close[st.raw_pos])
                {
                  st.raw_pos++;
                  if (st.raw_pos >= st.raw_close_len)
                    {
                      st.state = SC_NORM;
                      st.raw_pos = 0;
                    }
                }
              else
                {
                  st.raw_pos = 0;
                  if (st.raw_close_len > 0 && c == st.raw_close[0])
                    st.raw_pos = 1;
                }
              i++;
              continue;
            }
          if (st.state == SC_SQ || st.state == SC_DQ)
            {
              char q = (st.state == SC_SQ) ? '\'' : '"';
              if (st.escape)
                {
                  st.escape = 0;
                  i++;
                  continue;
                }
              if (c == '\\')
                {
                  if (i + 1 >= len)
                    {
                      cont = 1;
                      break;
                    }
                  st.escape = 1;
                  i++;
                  continue;
                }
              if (c == q)
                st.state = SC_NORM;
              i++;
              continue;
            }

          if (c == '/' && i + 1 < len && s[i + 1] == '/')
            {
              st.state = SC_LINE;
              break;
            }
          if (c == '/' && i + 1 < len && s[i + 1] == '*')
            {
              st.state = SC_BLOCK;
              i += 2;
              continue;
            }
          if (c == '\'')
            {
              st.state = SC_SQ;
              st.escape = 0;
              i++;
              continue;
            }
          if (c == '"')
            {
              int resume = 0;
              if (try_enter_raw (&st, s, len, i, &resume))
                {
                  i = resume;
                  continue;
                }
              st.state = SC_DQ;
              st.escape = 0;
              i++;
              continue;
            }
          if (brace_partner (c))
            {
              int here = (line == tline && i == tcol);
              if (is_opener (c))
                {
                  if (top >= 4096)
                    {
                      free (s);
                      return 0;
                    }
                  stack[top].line = line;
                  stack[top].col = i;
                  stack[top].ch = c;
                  if (here)
                    watch = top;
                  top++;
                }
              else if (here)
                {
                  if (top > 0 && brace_partner (stack[top - 1].ch) == c)
                    {
                      mline = stack[top - 1].line;
                      mcol = stack[top - 1].col;
                      found = 1;
                    }
                  stop = 1;
                  break;
                }
              else if (top > 0 && brace_partner (stack[top - 1].ch) == c)
                {
                  top--;
                  if (watch >= 0 && top == watch)
                    {
                      mline = line;
                      mcol = i;
                      found = 1;
                    }
                }
            }
          i++;
        }
      free (s);
      if (stop)
        break;
      if (st.state == SC_LINE)
        st.state = SC_NORM;
      else if ((st.state == SC_SQ || st.state == SC_DQ) && !cont)
        st.state = SC_NORM;
      st.escape = 0;
    }

  if (!found)
    return 0;
  *cursor_line = mline;
  *cursor_col = mcol;
  return 1;
}

static void
handle_match_brace (int ch, InputContext *ctx)
{
  (void) ch;
  if (!jump_brace (ctx->buf, ctx->cursor_line, ctx->cursor_col) && ctx->ed)
    set_status_message (ctx->ed, "No match");
}

static void
handle_comment (int ch, InputContext *ctx)
{
  (void) ch;
  int row = *ctx->cursor_line;
  char *line = buffer_get_line (ctx->buf, row);
  if (!line)
    return;
  int len = (int) strlen (line);
  int ind = 0;
  while (ind < len && (line[ind] == ' ' || line[ind] == '\t'))
    ind++;
  int uncomment = (ind + 1 < len && line[ind] == '/' && line[ind + 1] == '/');
  if (ctx->ed)
    clear_redo (&ctx->ed->redo_stack);
  if (uncomment)
    {
      int n = 2;
      if (ind + 2 < len && line[ind + 2] == ' ')
        n = 3;
      for (int c = ind + n - 1; c >= ind; c--)
        {
          char deleted = buffer_get_char (ctx->buf, row, c);
          if (ctx->ed)
            push_undo (&ctx->ed->undo_stack, false, row, c, deleted);
          buffer_delete_char (ctx->buf, row, c);
        }
      if (*ctx->cursor_col >= ind + n)
        *ctx->cursor_col -= n;
      else if (*ctx->cursor_col > ind)
        *ctx->cursor_col = ind;
    }
  else
    {
      const char *ins = "// ";
      for (int k = 0; k < 3; k++)
        {
          if (ctx->ed)
            push_undo (&ctx->ed->undo_stack, true, row, ind + k, ins[k]);
          buffer_insert_char (ctx->buf, row, ind + k, ins[k]);
        }
      if (*ctx->cursor_col >= ind)
        *ctx->cursor_col += 3;
    }
  free (line);
}

static void
handle_outdent (int ch, InputContext *ctx)
{
  (void) ch;
  int row = *ctx->cursor_line;
  char *line = buffer_get_line (ctx->buf, row);
  if (!line || !line[0])
    {
      free (line);
      return;
    }
  int tabw = 8;
  if (ctx->ed && ctx->ed->config.display.tab_width > 0)
    tabw = ctx->ed->config.display.tab_width;
  if (ctx->ed)
    clear_redo (&ctx->ed->redo_stack);
  if (line[0] == '\t')
    {
      if (ctx->ed)
        push_undo (&ctx->ed->undo_stack, false, row, 0, '\t');
      buffer_delete_char (ctx->buf, row, 0);
      if (*ctx->cursor_col > 0)
        (*ctx->cursor_col)--;
    }
  else if (line[0] == ' ')
    {
      int n = 0;
      while (line[n] == ' ' && n < tabw)
        n++;
      for (int c = n - 1; c >= 0; c--)
        {
          if (ctx->ed)
            push_undo (&ctx->ed->undo_stack, false, row, c, ' ');
          buffer_delete_char (ctx->buf, row, c);
        }
      if (*ctx->cursor_col >= n)
        *ctx->cursor_col -= n;
      else
        *ctx->cursor_col = 0;
    }
  free (line);
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
              int found = search_next (buf, cursor_line, cursor_col, search_buffer);
              if (ed)
                {
                  if (found < 0)
                    set_status_message (ed, "Bad pattern");
                  else if (found == 0)
                    set_status_message (ed, "Not found");
                  else if (found == 2)
                    set_status_message (ed, "Search wrapped");
                  else
                    set_status_message (ed, "Found");
                }
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
      else if (ch == 127 || ch == 8 || ch == KEY_BACKSPACE)
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
      if (ed && ed->meta_pending)
        {
          ed->meta_pending = 0;
          if (ch == 'b' || ch == KEY_LEFT)
            word_backward (buf, cursor_line, cursor_col);
          else if (ch == 'f' || ch == KEY_RIGHT)
            word_forward (buf, cursor_line, cursor_col);
          else if (ch == KEY_BACKSPACE || ch == 127 || ch == 8)
            handle_delete_word (0, &ctx);
          else
            set_status_message (ed, "Cancelled");
        }
      else if (ch == 27 && ed)
        {
          ed->meta_pending = 1;
          set_status_message (ed, "Esc-b back word  Esc-f forward word");
        }
      else
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

/* Search [from_line, from_col) .. exclusive end (to_line, to_col).
 * to_line < 0 means through EOF. A match that starts exactly at
 * (skip_line, skip_col) is ignored when skip_line >= 0.
 * Returns 1 and writes the match start. */
static int
find_in_range (Buffer *buf, regex_t *re, int from_line, int from_col,
               int to_line, int to_col, int skip_line, int skip_col,
               int *out_line, int *out_col)
{
  int lines = buffer_num_lines (buf);
  if (from_line < 0)
    from_line = 0;
  for (int line = from_line; line < lines; line++)
    {
      if (to_line >= 0 && line > to_line)
        break;
      char *text = buffer_get_line (buf, line);
      if (!text)
        continue;
      int len = (int) strlen (text);
      int begin = (line == from_line) ? from_col : 0;
      if (begin < 0)
        begin = 0;
      if (begin > len)
        begin = len;
      int limit = len;
      if (to_line >= 0 && line == to_line)
        {
          limit = to_col;
          if (limit < 0)
            limit = 0;
          if (limit > len)
            limit = len;
        }
      int pos = begin;
      int flags = pos > 0 ? REG_NOTBOL : 0;
      int iters = 0;
      while (pos < limit && iters++ < 1000)
        {
          regmatch_t match;
          if (regexec (re, text + pos, 1, &match, flags) != 0)
            break;
          if (match.rm_so < 0)
            break;
          if (match.rm_eo == 0)
            {
              pos++;
              flags = REG_NOTBOL;
              continue;
            }
          int at = pos + (int) match.rm_so;
          if (at >= limit)
            break;
          if (!(line == skip_line && at == skip_col))
            {
              *out_line = line;
              *out_col = at;
              free (text);
              return 1;
            }
          int step = (int) match.rm_eo;
          if (step < 1)
            step = 1;
          pos += step;
          flags = REG_NOTBOL;
        }
      free (text);
    }
  return 0;
}

int
search_next (Buffer *buf, int *cursor_line, int *cursor_col,
             const char *pattern)
{
  if (!buf || !cursor_line || !cursor_col || !pattern)
    return -1;
  if (pattern[0] == '\0' || strlen (pattern) > 100)
    return -1;
  regex_t regex;
  if (regcomp (&regex, pattern, REG_EXTENDED) != 0)
    return -1;

  int orig_line = *cursor_line;
  int orig_col = *cursor_col;
  int line = 0;
  int col = 0;
  if (find_in_range (buf, &regex, orig_line, orig_col, -1, 0,
                     orig_line, orig_col, &line, &col))
    {
      *cursor_line = line;
      *cursor_col = col;
      regfree (&regex);
      return 1;
    }
  /* Wrap. Include a match sitting on the original cursor so a lone hit
   * is reported instead of "not found". */
  int end_col = orig_col + 1;
  if (find_in_range (buf, &regex, 0, 0, orig_line, end_col, -1, -1, &line, &col))
    {
      *cursor_line = line;
      *cursor_col = col;
      regfree (&regex);
      return 2;
    }
  regfree (&regex);
  return 0;
}
