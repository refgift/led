#include "controller.h"
#include "editor.h"
#include "view.h"
#include "utils.h"
#include "frontend/frontend.h"
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include <ctype.h>
#include <stdio.h>

/* Most changes kept per stack. One change is one byte of text. */
#define UNDO_CAP 1000000

static int
is_utf8_cont (char c)
{
  return ((unsigned char) c & 0xC0) == 0x80;
}

static void
push_change (UndoStack *stack, bool is_insert, int line, int col, char ch,
             bool chained)
{
  if (!stack)
    return;
  if (stack->count >= stack->capacity)
    {
      if (stack->capacity >= UNDO_CAP)
        {
          /* Full. Forget the oldest half. Dropping the new change instead
           * would leave the stack describing a buffer that no longer
           * exists, and the next undo would edit the wrong place. */
          int keep = stack->count / 2;
          memmove (stack->changes, stack->changes + (stack->count - keep),
                   (size_t) keep * sizeof (Change));
          stack->count = keep;
        }
      else
        {
          int new_capacity =
            stack->capacity == 0 ? 16 : stack->capacity * 2;
          if (new_capacity > UNDO_CAP)
            new_capacity = UNDO_CAP;
          Change *temp =
            xrealloc (stack->changes, new_capacity * sizeof (Change));
          if (!temp)
            return;
          stack->changes = temp;
          stack->capacity = new_capacity;
        }
    }
  stack->changes[stack->count].is_insert = is_insert;
  stack->changes[stack->count].line = line;
  stack->changes[stack->count].col = col;
  stack->changes[stack->count].ch = ch;
  stack->changes[stack->count].chained = chained;
  stack->count++;
}

void
push_undo (UndoStack *stack, bool is_insert, int line, int col, char ch)
{
  push_change (stack, is_insert, line, col, ch, is_utf8_cont (ch));
}

/* Make everything pushed since the stack held `start` entries one undo
 * step: every entry but the first is chained to the one below it. */
static void
chain_since (UndoStack *stack, int start)
{
  if (!stack || start < 0)
    return;
  for (int i = start + 1; i < stack->count; i++)
    stack->changes[i].chained = true;
}

void
push_redo (UndoStack *stack, bool is_insert, int line, int col, char ch)
{
  push_change (stack, is_insert, line, col, ch, is_utf8_cont (ch));
}

void
undo_operation (Buffer *buf, UndoStack *undo, UndoStack *redo, int *cursor_line, int *cursor_col)
{
  if (!undo || undo->count <= 0)
    return;
  undo->count--;
  Change c = undo->changes[undo->count];
  push_change (redo, c.is_insert, c.line, c.col, c.ch, c.chained);
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
  push_change (undo, c.is_insert, c.line, c.col, c.ch, c.chained);
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

/* === The state every action works on === */
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
static void handle_newline   (int ch, InputContext *ctx);
static void do_symbol (InputContext *ctx, int action, int value);
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

/* === Handler implementations === */

/* Byte column where the character ending just before col starts. */
static int
char_start_before (const Buffer *buf, int line, int col)
{
  int c = col - 1;
  int steps = 0;
  while (c > 0 && steps < 3 && is_utf8_cont (buffer_get_char (buf, line, c)))
    {
      c--;
      steps++;
    }
  return c < 0 ? 0 : c;
}

/* Byte column just past the character that starts at col. */
static int
char_end_after (const Buffer *buf, int line, int col)
{
  int len = buffer_get_line_length (buf, line);
  int c = col + 1;
  int steps = 0;
  while (c < len && steps < 3 && is_utf8_cont (buffer_get_char (buf, line, c)))
    {
      c++;
      steps++;
    }
  return c > len ? len : c;
}

/* Order two points so (*l1,*c1) comes first. */
static void
order_points (int *l1, int *c1, int *l2, int *c2)
{
  if (*l1 > *l2 || (*l1 == *l2 && *c1 > *c2))
    {
      int t = *l1; *l1 = *l2; *l2 = t;
      t = *c1; *c1 = *c2; *c2 = t;
    }
}

/* Text between two ordered points, lines joined by '\n'. Caller frees. */
static char *
range_text (const Buffer *buf, int l1, int c1, int l2, int c2)
{
  size_t total = 0;
  for (int l = l1; l <= l2; l++)
    total += (size_t) buffer_get_line_length (buf, l) + 1;
  char *out = xmalloc (total + 1);
  size_t n = 0;
  for (int l = l1; l <= l2; l++)
    {
      int len = buffer_get_line_length (buf, l);
      int from = (l == l1) ? c1 : 0;
      int to = (l == l2) ? c2 : len;
      if (from > len) from = len;
      if (to > len) to = len;
      for (int c = from; c < to; c++)
        out[n++] = buffer_get_char (buf, l, c);
      if (l < l2)
        out[n++] = '\n';
    }
  out[n] = '\0';
  return out;
}

/* Delete an ordered range and record every byte, so undo puts the text
 * back. A range too large for the undo stack empties both stacks: no
 * history is better than history that points at the wrong text.
 * Returns 0 when the range is empty or invalid. */
static int
delete_range_with_undo (InputContext *ctx, int l1, int c1, int l2, int c2)
{
  int n = buffer_num_lines (ctx->buf);
  if (l1 < 0 || l2 >= n || l1 > l2 || (l1 == l2 && c1 >= c2) || c1 < 0)
    return 0;
  char *text = range_text (ctx->buf, l1, c1, l2, c2);
  size_t len = strlen (text);
  if (len == 0)
    {
      free (text);
      return 0;
    }
  if (buffer_delete_range (ctx->buf, l1, c1, l2, c2) != 0)
    {
      free (text);
      return 0;
    }
  if (ctx->ed)
    {
      clear_redo (&ctx->ed->redo_stack);
      if (len > UNDO_CAP / 2)
        {
          free_undo_stacks (&ctx->ed->undo_stack, &ctx->ed->redo_stack);
          set_status_message (ctx->ed, "Too large to undo");
        }
      else
        /* Each byte is "deleted at (l1,c1)". Undo runs them backwards
         * and re-inserts at that one point, which rebuilds the text. */
        for (size_t i = 0; i < len; i++)
          push_change (&ctx->ed->undo_stack, false, l1, c1, text[i], i > 0);
    }
  free (text);
  return 1;
}

/* The active selection as an ordered range. Returns 0 if there is none. */
static int
selection_range (InputContext *ctx, int *l1, int *c1, int *l2, int *c2)
{
  if (!ctx->ed || !ctx->ed->selection_active)
    return 0;
  *l1 = ctx->ed->selection_start_line;
  *c1 = ctx->ed->selection_start_col;
  *l2 = ctx->ed->selection_end_line;
  *c2 = ctx->ed->selection_end_col;
  order_points (l1, c1, l2, c2);
  int n = buffer_num_lines (ctx->buf);
  if (*l1 < 0 || *l2 >= n)
    return 0;
  return (*l1 < *l2 || *c1 < *c2);
}

/* Delete the selection, if any, and put the cursor where it began. */
static int
delete_selection (InputContext *ctx)
{
  int l1, c1, l2, c2;
  if (!selection_range (ctx, &l1, &c1, &l2, &c2))
    return 0;
  if (!delete_range_with_undo (ctx, l1, c1, l2, c2))
    return 0;
  ctx->ed->selection_active = 0;
  *ctx->cursor_line = l1;
  *ctx->cursor_col = c1;
  return 1;
}
static void handle_left (int ch, InputContext *ctx)
{
  (void)ch;
  if (*ctx->cursor_col > 0)
    *ctx->cursor_col = char_start_before (ctx->buf, *ctx->cursor_line, *ctx->cursor_col);
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
    *ctx->cursor_col = char_end_after (ctx->buf, *ctx->cursor_line, *ctx->cursor_col);
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
  *ctx->cursor_col = 0;
}

static void handle_end (int ch, InputContext *ctx)
{
  (void)ch;
  *ctx->cursor_col = buffer_get_line_length (ctx->buf, *ctx->cursor_line);
}

static void handle_top (InputContext *ctx)
{
  *ctx->cursor_line = 0;
  *ctx->cursor_col = 0;
  *ctx->scroll_row = 0;
  *ctx->scroll_col = 0;
}

static void handle_bottom (InputContext *ctx)
{
  *ctx->cursor_line = buffer_num_lines (ctx->buf) - 1;
  *ctx->cursor_col = buffer_get_line_length (ctx->buf, *ctx->cursor_line);
  *ctx->scroll_row = *ctx->cursor_line > 5 ? *ctx->cursor_line - 5 : 0;
}

/* Lines a page key moves: the screen height less a little overlap. */
static int
page_step (InputContext *ctx)
{
  int rows = ctx->ed ? ctx->ed->page_rows : 0;
  return rows > 5 ? rows - 3 : 5;
}

static void handle_ppage (int ch, InputContext *ctx)
{
  (void)ch;
  *ctx->cursor_line -= page_step (ctx);
  if (*ctx->cursor_line < 0) *ctx->cursor_line = 0;
  *ctx->cursor_col = 0;
  if (*ctx->scroll_row > *ctx->cursor_line) *ctx->scroll_row = *ctx->cursor_line;
}

static void handle_npage (int ch, InputContext *ctx)
{
  (void)ch;
  *ctx->cursor_line += page_step (ctx);
  if (*ctx->cursor_line >= buffer_num_lines (ctx->buf))
    *ctx->cursor_line = buffer_num_lines (ctx->buf) - 1;
  *ctx->cursor_col = 0;
}

static void handle_undo (int ch, InputContext *ctx)
{
  (void)ch;
  if (!ctx->ed)
    return;
  UndoStack *u = &ctx->ed->undo_stack;
  /* One step is one whole operation: keep going while the entry just
   * undone was chained to the one below it. */
  while (u->count > 0)
    {
      bool chained = u->changes[u->count - 1].chained;
      undo_operation (ctx->buf, u, &ctx->ed->redo_stack, ctx->cursor_line, ctx->cursor_col);
      if (!chained)
        break;
    }
}

static void handle_redo (int ch, InputContext *ctx)
{
  (void)ch;
  if (!ctx->ed)
    return;
  UndoStack *r = &ctx->ed->redo_stack;
  if (r->count <= 0)
    return;
  redo_operation (ctx->buf, &ctx->ed->undo_stack, r, ctx->cursor_line, ctx->cursor_col);
  while (r->count > 0 && r->changes[r->count - 1].chained)
    redo_operation (ctx->buf, &ctx->ed->undo_stack, r, ctx->cursor_line, ctx->cursor_col);
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

/* One level of indent, typed as symbols. */
static void
insert_indent_unit (InputContext *ctx)
{
  if (!ctx->ed)
    return;
  int tabw = ctx->ed->config.display.tab_width;
  if (ctx->ed->config.display.spaces_for_tab)
    {
      for (int i = 0; i < tabw; i++)
        do_symbol (ctx, S_INSERT, ' ');
    }
  else if (tabw != 0)
    do_symbol (ctx, S_INSERT, '\t');
}

/* Split the line at the cursor. Nothing else. */
static void handle_newline (int ch, InputContext *ctx)
{
  (void)ch;
  int row = *ctx->cursor_line;
  int col = *ctx->cursor_col;
  if (ctx->ed)
    {
      push_undo (&ctx->ed->undo_stack, true, row, col, '\n');
      clear_redo (&ctx->ed->redo_stack);
    }
  buffer_insert_char (ctx->buf, row, col, '\n');
  (*ctx->cursor_line)++;
  *ctx->cursor_col = 0;
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
  /* The macro itself: a newline, then the indent, typed as symbols. */
  do_symbol (ctx, S_NEWLINE, 0);
  if (do_indent)
    {
      for (int i = 0; i < indent_n; i++)
        do_symbol (ctx, S_INSERT, (unsigned char) indent[i]);
      if (extra)
        insert_indent_unit (ctx);
    }
}

static void
set_clipboard (InputContext *ctx, char *text)
{
  if (*ctx->clipboard)
    free (*ctx->clipboard);
  *ctx->clipboard = text;
}

/* With a selection: cut it. Without: cut from the cursor to end of line. */
static void handle_cut (int ch, InputContext *ctx)
{
  (void)ch;
  int l1, c1, l2, c2;
  if (selection_range (ctx, &l1, &c1, &l2, &c2))
    {
      set_clipboard (ctx, range_text (ctx->buf, l1, c1, l2, c2));
      delete_selection (ctx);
      return;
    }
  int row = *ctx->cursor_line;
  int col = *ctx->cursor_col;
  int linelen = buffer_get_line_length (ctx->buf, row);
  if (col < linelen)
    {
      set_clipboard (ctx, range_text (ctx->buf, row, col, row, linelen));
      delete_range_with_undo (ctx, row, col, row, linelen);
    }
}

/* With a selection: copy it. Without: copy from the cursor to end of line. */
static void handle_copy (int ch, InputContext *ctx)
{
  (void)ch;
  int l1, c1, l2, c2;
  if (selection_range (ctx, &l1, &c1, &l2, &c2))
    {
      set_clipboard (ctx, range_text (ctx->buf, l1, c1, l2, c2));
      return;
    }
  int row = *ctx->cursor_line;
  int col = *ctx->cursor_col;
  int linelen = buffer_get_line_length (ctx->buf, row);
  if (col < linelen)
    set_clipboard (ctx, range_text (ctx->buf, row, col, row, linelen));
}

static void handle_paste (int ch, InputContext *ctx)
{
  (void)ch;
  if (!*ctx->clipboard || !**ctx->clipboard)
    return;
  char *filtered = border_filter_dup (*ctx->clipboard);
  const char *to_insert = filtered ? filtered : *ctx->clipboard;
  size_t n = strlen (to_insert);
  int line = *ctx->cursor_line;
  int col = *ctx->cursor_col;
  int len = buffer_get_line_length (ctx->buf, line);
  if (col > len)
    col = len;
  if (n > 0 && buffer_insert_text (ctx->buf, line, col, to_insert) == 0)
    {
      if (ctx->ed)
        {
          clear_redo (&ctx->ed->redo_stack);
          if (n > UNDO_CAP / 2)
            {
              free_undo_stacks (&ctx->ed->undo_stack, &ctx->ed->redo_stack);
              set_status_message (ctx->ed, "Too large to undo");
            }
        }
      /* Record each byte where it landed, and leave the cursor after
       * the pasted text, on the last pasted line. */
      int record = (ctx->ed && n <= UNDO_CAP / 2);
      for (size_t i = 0; i < n; i++)
        {
          if (record)
            push_change (&ctx->ed->undo_stack, true, line, col, to_insert[i],
                         i > 0);
          if (to_insert[i] == '\n')
            {
              line++;
              col = 0;
            }
          else
            col++;
        }
      *ctx->cursor_line = line;
      *ctx->cursor_col = col;
    }
  if (filtered)
    free (filtered);
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
  if (ctx->ed && ctx->ed->save_blocked)
    {
      /* The buffer is not the whole file. Writing it would destroy
       * the part that was never loaded. */
      set_status_message (ctx->ed, "Not saved: file was not fully loaded");
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
  if (delete_selection (ctx))
    return;
  if (*ctx->cursor_col > 0)
    {
      /* Remove the whole character, lead byte first, so the undo
       * stack holds its bytes in the same order as a typed one. */
      int row = *ctx->cursor_line;
      int start = char_start_before (ctx->buf, row, *ctx->cursor_col);
      int nbytes = *ctx->cursor_col - start;
      if (ctx->ed)
        clear_redo (&ctx->ed->redo_stack);
      for (int i = 0; i < nbytes; i++)
        {
          char deleted = buffer_get_char (ctx->buf, row, start);
          if (ctx->ed)
            push_undo (&ctx->ed->undo_stack, false, row, start, deleted);
          buffer_delete_char (ctx->buf, row, start);
        }
      *ctx->cursor_col = start;
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
  if (delete_selection (ctx))
    return;
  int len = buffer_get_line_length (ctx->buf, *ctx->cursor_line);
  if (*ctx->cursor_col < len)
    {
      // Delete the whole character at the cursor (forward)
      int row = *ctx->cursor_line;
      int col = *ctx->cursor_col;
      int nbytes = char_end_after (ctx->buf, row, col) - col;
      if (ctx->ed)
        clear_redo (&ctx->ed->redo_stack);
      for (int i = 0; i < nbytes; i++)
        {
          char deleted = buffer_get_char (ctx->buf, row, col);
          if (ctx->ed)
            push_undo (&ctx->ed->undo_stack, false, row, col, deleted);
          buffer_delete_char (ctx->buf, row, col);
        }
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
      /* Spaces up to the next tab stop, typed as symbols. */
      char *line = buffer_get_line (ctx->buf, *ctx->cursor_line);
      if (line)
        {
          int tabw = ctx->ed->config.display.tab_width;
          int current_vis = visual_column (line, (int) strlen (line),
                                           *ctx->cursor_col, tabw);
          int spaces = tabw - (current_vis % tabw);
          if (spaces == 0) spaces = tabw;
          free (line);
          for (int i = 0; i < spaces; i++)
            do_symbol (ctx, S_INSERT, ' ');
        }
    }
  else
    do_symbol (ctx, S_INSERT, '\t');
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
    do_symbol (ctx, S_BACKSPACE, 1);
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
/* === symbol / macro / token ===
 *
 * Every edit goes through one of three functions with one signature:
 *
 *   symbol (action, value)   one action            a key press
 *   macro  (action, value)   several symbols       one undo step
 *   token  (action, value)   keys collected into   a prompt
 *                            a unit, then acted on
 *
 * They act on the attached context: an Editor (controller_attach), or
 * the pieces handle_input was given.
 */

static InputContext *cx;        /* what the three functions act on */
static InputContext attached;   /* storage for controller_attach */
static int last_plain;          /* did the last key reach a symbol/macro? */

void
controller_attach (Editor *ed)
{
  if (!ed)
    {
      cx = NULL;
      return;
    }
  attached = (InputContext) {
    .buf = &ed->model,
    .scroll_row = &ed->scroll_row,
    .scroll_col = &ed->scroll_col,
    .cursor_line = &ed->cursor_line,
    .cursor_col = &ed->cursor_col,
    .show_line_numbers = &ed->show_line_numbers,
    .search_buffer = ed->search_buffer,
    .search_mode = &ed->search_mode,
    .clipboard = &ed->clipboard,
    .filename = ed->filename,
    .ed = ed
  };
  cx = &attached;
}

int
controller_key_was_plain (void)
{
  return last_plain;
}

/* Keep the cursor on a real character of a real line. */
static void
settle (InputContext *ctx)
{
  int nl = buffer_num_lines (ctx->buf);
  if (*ctx->cursor_line >= nl)
    *ctx->cursor_line = nl > 0 ? nl - 1 : 0;
  if (*ctx->cursor_line < 0)
    *ctx->cursor_line = 0;
  int clen = buffer_get_line_length (ctx->buf, *ctx->cursor_line);
  if (*ctx->cursor_col > clen)
    *ctx->cursor_col = clen;
  if (*ctx->cursor_col < 0)
    *ctx->cursor_col = 0;
  /* Up/Down can land inside a multi-byte character. Step back to
   * its first byte so the next edit cannot split it. */
  int steps = 0;
  while (*ctx->cursor_col > 0 && *ctx->cursor_col < clen && steps++ < 3
         && is_utf8_cont (buffer_get_char (ctx->buf, *ctx->cursor_line,
                                           *ctx->cursor_col)))
    (*ctx->cursor_col)--;
}

/* --- symbol --- */

static void
do_symbol (InputContext *ctx, int action, int value)
{
  int times = value > 0 ? value : 1;
  switch (action)
    {
    case S_INSERT:
      handle_printable (value, ctx);
      break;
    case S_NEWLINE:
      handle_newline (0, ctx);
      break;
    case S_LEFT:
      while (times--) handle_left (0, ctx);
      break;
    case S_RIGHT:
      while (times--) handle_right (0, ctx);
      break;
    case S_UP:
      while (times--) handle_up (0, ctx);
      break;
    case S_DOWN:
      while (times--) handle_down (0, ctx);
      break;
    case S_HOME:
      handle_home (0, ctx);
      break;
    case S_END:
      handle_end (0, ctx);
      break;
    case S_TOP:
      handle_top (ctx);
      break;
    case S_BOTTOM:
      handle_bottom (ctx);
      break;
    case S_PAGE_UP:
      while (times--) handle_ppage (0, ctx);
      break;
    case S_PAGE_DOWN:
      while (times--) handle_npage (0, ctx);
      break;
    case S_WORD_LEFT:
      while (times--) handle_word_left (0, ctx);
      break;
    case S_WORD_RIGHT:
      while (times--) handle_word_right (0, ctx);
      break;
    case S_MATCH_BRACE:
      handle_match_brace (0, ctx);
      break;
    case S_BACKSPACE:
      while (times--) handle_backspace (0, ctx);
      break;
    case S_DELETE:
      while (times--) handle_delete (0, ctx);
      break;
    case S_UNDO:
      while (times--) handle_undo (0, ctx);
      break;
    case S_REDO:
      while (times--) handle_redo (0, ctx);
      break;
    case S_SELECT_ALL:
      handle_select_all (0, ctx);
      break;
    case S_COPY:
      handle_copy (0, ctx);
      break;
    case S_SAVE:
      handle_save (0, ctx);
      break;
    case S_TOGGLE_NUMBERS:
      *ctx->show_line_numbers = !*ctx->show_line_numbers;
      break;
    case S_TOGGLE_WRAP:
      if (ctx->ed)
        {
          ctx->ed->config.display.word_wrap = !ctx->ed->config.display.word_wrap;
          set_status_message (ctx->ed, ctx->ed->config.display.word_wrap
                              ? "Word wrap: ON" : "Word wrap: OFF");
        }
      break;
    case S_TOGGLE_BORDER:
      if (ctx->ed)
        {
          ctx->ed->config.display.show_border = !ctx->ed->config.display.show_border;
          /* The border appears or disappears: repaint everything. */
          view_invalidate ();
          set_status_message (ctx->ed, ctx->ed->config.display.show_border
                              ? "Border: ON" : "Border: OFF (xterm copy clean)");
        }
      break;
    default:
      break;
    }
}

/* A selection lasts until the next action that is not Select All or
 * Copy. Otherwise the highlight would never go away. */
static void
end_selection (InputContext *ctx)
{
  if (ctx->ed)
    ctx->ed->selection_active = 0;
}

int
symbol (int action, int value)
{
  if (!cx)
    return -1;
  do_symbol (cx, action, value);
  if (action != S_SELECT_ALL && action != S_COPY
      && action != S_TOGGLE_NUMBERS && action != S_TOGGLE_WRAP
      && action != S_TOGGLE_BORDER)
    end_selection (cx);
  settle (cx);
  return 0;
}

/* --- macro --- */

static void
do_macro (InputContext *ctx, int action, int value)
{
  int times = value > 0 ? value : 1;
  /* Whatever a macro records is one undo step. */
  int start = ctx->ed ? ctx->ed->undo_stack.count : -1;
  while (times--)
    switch (action)
      {
      case M_ENTER:
        handle_enter (0, ctx);
        break;
      case M_TAB:
        handle_tab (0, ctx);
        break;
      case M_OUTDENT:
        handle_outdent (0, ctx);
        break;
      case M_COMMENT:
        handle_comment (0, ctx);
        break;
      case M_DELETE_WORD:
        handle_delete_word (0, ctx);
        break;
      case M_CUT:
        handle_cut (0, ctx);
        break;
      case M_PASTE:
        handle_paste (0, ctx);
        break;
      default:
        break;
      }
  if (ctx->ed && start >= 0 && ctx->ed->undo_stack.count > start)
    chain_since (&ctx->ed->undo_stack, start);
}

int
macro (int action, int value)
{
  if (!cx)
    return -1;
  do_macro (cx, action, value);
  end_selection (cx);
  settle (cx);
  return 0;
}

/* --- token --- */

/* Which prompt is collecting keys, if any. */
static int
token_open (InputContext *ctx)
{
  Editor *ed = ctx->ed;
  if (ed && ed->goto_mode)
    return K_GOTO;
  if (ed && ed->replace_step)
    return K_REPLACE;
  if (*ctx->search_mode)
    return K_SEARCH;
  if (ed && ed->meta_pending)
    return K_META;
  return 0;
}

static void
text_append (char *text, size_t size, int c)
{
  size_t n = strlen (text);
  if (n + 1 < size)
    {
      text[n] = (char) c;
      text[n + 1] = '\0';
    }
}

static void
text_erase (char *text)
{
  /* Remove one whole character, not one byte of it. */
  size_t n = strlen (text);
  while (n > 0 && is_utf8_cont (text[n - 1]))
    n--;
  if (n > 0)
    n--;
  text[n] = '\0';
}

static void
token_begin (InputContext *ctx, int kind)
{
  Editor *ed = ctx->ed;
  switch (kind)
    {
    case K_SEARCH:
      *ctx->search_mode = 1;
      memset (ctx->search_buffer, 0, 256);
      if (ed)
        {
          ed->replace_step = 0;
          set_status_message (ed, "Search (Esc to cancel):");
        }
      break;
    case K_REPLACE:
      if (ed && ed->replace_step == 0)
        {
          ed->replace_step = 1;
          *ctx->search_mode = 0;
          memset (ed->search_buffer, 0, sizeof ed->search_buffer);
          memset (ed->replace_buffer, 0, sizeof ed->replace_buffer);
        }
      break;
    case K_GOTO:
      if (ed)
        {
          ed->goto_mode = 1;
          ed->goto_buffer[0] = '\0';
          ed->meta_pending = 0;
          set_status_message (ed, "Go to line[:col]");
        }
      break;
    case K_META:
      if (ed)
        {
          ed->meta_pending = 1;
          set_status_message (ed, "Esc-b back word  Esc-f forward word");
        }
      break;
    default:
      break;
    }
}

static void
token_char (InputContext *ctx, int kind, int c)
{
  Editor *ed = ctx->ed;
  switch (kind)
    {
    case K_SEARCH:
      text_append (ctx->search_buffer, 256, c);
      break;
    case K_REPLACE:
      if (ed->replace_step == 1)
        text_append (ed->search_buffer, sizeof ed->search_buffer, c);
      else
        text_append (ed->replace_buffer, sizeof ed->replace_buffer, c);
      break;
    case K_GOTO:
      if ((c >= '0' && c <= '9') || c == ':')
        text_append (ed->goto_buffer, sizeof ed->goto_buffer, c);
      break;
    case K_META:
      /* Esc then one key: the pair is the token. */
      ed->meta_pending = 0;
      if (c == 'b' || c == LED_KEY_LEFT)
        do_symbol (ctx, S_WORD_LEFT, 1);
      else if (c == 'f' || c == LED_KEY_RIGHT)
        do_symbol (ctx, S_WORD_RIGHT, 1);
      else if (c == LED_KEY_BACKSPACE || c == 127 || c == 8)
        do_macro (ctx, M_DELETE_WORD, 1);
      else
        set_status_message (ed, "Cancelled");
      end_selection (ctx);
      break;
    default:
      break;
    }
}

static void
token_erase (InputContext *ctx, int kind)
{
  Editor *ed = ctx->ed;
  if (kind == K_SEARCH)
    text_erase (ctx->search_buffer);
  else if (kind == K_REPLACE)
    text_erase (ed->replace_step == 1 ? ed->search_buffer : ed->replace_buffer);
  else if (kind == K_GOTO)
    text_erase (ed->goto_buffer);
}

static void
token_done (InputContext *ctx, int kind)
{
  Editor *ed = ctx->ed;
  if (kind == K_SEARCH)
    {
      /* Find next. The prompt stays open so Enter repeats the search. */
      if (ctx->search_buffer[0] == '\0')
        return;
      int found = search_next (ctx->buf, ctx->cursor_line, ctx->cursor_col,
                               ctx->search_buffer);
      if (ed)
        set_status_message (ed, found < 0 ? "Bad pattern"
                            : found == 0 ? "Not found"
                            : found == 2 ? "Search wrapped" : "Found");
    }
  else if (kind == K_REPLACE && ed->replace_step == 1)
    {
      if (ed->search_buffer[0] != '\0')
        ed->replace_step = 2;
    }
  else if (kind == K_REPLACE)
    {
      int n = buffer_replace_all (ctx->buf, ed->search_buffer,
                                  ed->replace_buffer);
      ed->replace_step = 0;
      if (n > 0)
        {
          /* Replace is not recorded for undo, and every position
           * already on the stacks is now wrong. Drop them. */
          free_undo_stacks (&ed->undo_stack, &ed->redo_stack);
          ed->file_modified = 1;
          ed->unsaved_keystrokes++;
          char msg[64];
          snprintf (msg, sizeof msg, "Replaced %d (cannot be undone)", n);
          set_status_message (ed, msg);
        }
      else
        set_status_message (ed, n == 0 ? "Not found" : "Bad pattern");
    }
  else if (kind == K_GOTO)
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
    }
}

static void
token_cancel (InputContext *ctx, int kind)
{
  Editor *ed = ctx->ed;
  if (kind == K_SEARCH)
    {
      *ctx->search_mode = 0;
      ctx->search_buffer[0] = '\0';
    }
  else if (kind == K_REPLACE)
    ed->replace_step = 0;
  else if (kind == K_GOTO)
    {
      ed->goto_mode = 0;
      ed->goto_buffer[0] = '\0';
      set_status_message (ed, "Cancelled");
    }
  else if (kind == K_META)
    ed->meta_pending = 0;
}

static void
do_token (InputContext *ctx, int action, int value)
{
  if (action == T_BEGIN)
    {
      token_begin (ctx, value);
      return;
    }
  int kind = token_open (ctx);
  if (!kind)
    return;
  if (action == T_CHAR)
    token_char (ctx, kind, value);
  else if (action == T_ERASE)
    token_erase (ctx, kind);
  else if (action == T_DONE)
    token_done (ctx, kind);
  else if (action == T_CANCEL)
    token_cancel (ctx, kind);
}

int
token (int action, int value)
{
  if (!cx)
    return -1;
  do_token (cx, action, value);
  settle (cx);
  return 0;
}

/* === Keys ===
 * What each key means, as data: which of the three functions, which
 * action, which value. */

typedef int (*control_fn) (int action, int value);

typedef struct {
  int key;
  control_fn level;
  int action;
  int value;
} KeyBinding;

static const KeyBinding bindings[] = {
  { LED_KEY_LEFT,      symbol, S_LEFT, 1 },
  { LED_KEY_RIGHT,     symbol, S_RIGHT, 1 },
  { LED_KEY_UP,        symbol, S_UP, 1 },
  { LED_KEY_DOWN,      symbol, S_DOWN, 1 },
  { LED_KEY_HOME,      symbol, S_HOME, 0 },
  { LED_KEY_END,       symbol, S_END, 0 },
  { LED_KEY_PPAGE,     symbol, S_PAGE_UP, 1 },
  { LED_KEY_NPAGE,     symbol, S_PAGE_DOWN, 1 },
  { 26,                symbol, S_UNDO, 1 },          /* Ctrl+Z */
  { 25,                symbol, S_REDO, 1 },          /* Ctrl+Y */
  { 1,                 symbol, S_SELECT_ALL, 0 },    /* Ctrl+A */
  { 3,                 symbol, S_COPY, 0 },          /* Ctrl+C */
  { 19,                symbol, S_SAVE, 0 },          /* Ctrl+S */
  { LED_KEY_BACKSPACE, symbol, S_BACKSPACE, 1 },
  { 127,               symbol, S_BACKSPACE, 1 },     /* DEL / ^? */
  { 8,                 symbol, S_BACKSPACE, 1 },     /* BS / Ctrl+H */
  { LED_KEY_DC,        symbol, S_DELETE, 1 },
  { 29,                symbol, S_MATCH_BRACE, 0 },   /* Ctrl+] */
  { LED_KEY_ENTER,     macro,  M_ENTER, 1 },
  { 10,                macro,  M_ENTER, 1 },
  { 13,                macro,  M_ENTER, 1 },
  { 9,                 macro,  M_TAB, 1 },
  { LED_KEY_BTAB,      macro,  M_OUTDENT, 1 },       /* Shift+Tab */
  { 11,                macro,  M_COMMENT, 1 },       /* Ctrl+K */
  { 23,                macro,  M_DELETE_WORD, 1 },   /* Ctrl+W */
  { 24,                macro,  M_CUT, 1 },           /* Ctrl+X */
  { 22,                macro,  M_PASTE, 1 },         /* Ctrl+V */
  { 0, NULL, 0, 0 }
};

static int
is_enter (int ch)
{
  return ch == '\n' || ch == 13 || ch == LED_KEY_ENTER;
}

static int
is_erase (int ch)
{
  return ch == 127 || ch == 8 || ch == LED_KEY_BACKSPACE;
}

/* ASCII printables and the bytes of UTF-8 text. */
static int
is_text (int ch)
{
  return (ch >= 32 && ch <= 126) || (ch >= 128 && ch <= 255);
}

/* A key typed into an open prompt. */
static void
key_to_token (int ch)
{
  if (ch == 27)
    token (T_CANCEL, 0);
  else if (is_enter (ch))
    token (T_DONE, 0);
  else if (is_erase (ch))
    token (T_ERASE, 0);
  else if (is_text (ch))
    token (T_CHAR, ch);
}

/* Turn one key into one call of symbol, macro, or token. */
static void
route_key (int ch)
{
  Editor *ed = cx->ed;
  last_plain = 0;
  if (ed)
    {
      if (ed->goto_mode)
        {
          key_to_token (ch);
          return;
        }
      if (ch == 7 && !*cx->search_mode && ed->replace_step == 0)
        {
          token (T_BEGIN, K_GOTO);      /* Ctrl+G */
          return;
        }
      if (ch == LED_KEY_F (2) || ch == LED_KEY_F (3) || ch == LED_KEY_F (4))
        {
          symbol (ch == LED_KEY_F (2) ? S_TOGGLE_NUMBERS
                  : ch == LED_KEY_F (3) ? S_TOGGLE_WRAP : S_TOGGLE_BORDER, 0);
          return;
        }
      /* Byte 31 is traditional Ctrl+/ (and Ctrl+_). The other two codes
       * are the same chord from a terminal that sends a modified-key
       * sequence. Consume the chord either way so it is never inserted. */
      if (ch == 31 || ch == LED_KEY_SEARCH || ch == LED_KEY_SEARCH_ALT)
        {
          if (ed->config.search.enabled)
            token (T_BEGIN, K_SEARCH);
          return;
        }
      if (ch == 18 && ed->config.search.enabled)
        {
          token (T_BEGIN, K_REPLACE);   /* Ctrl+R */
          return;
        }
      if (ed->replace_step)
        {
          key_to_token (ch);
          return;
        }
    }
  last_plain = 1;
  if (*cx->search_mode)
    {
      key_to_token (ch);
      return;
    }
  if (ed && ed->meta_pending)
    {
      token (T_CHAR, ch);       /* the key after Esc */
      return;
    }
  if (ch == 27 && ed)
    {
      token (T_BEGIN, K_META);
      end_selection (cx);
      return;
    }
  if (ed && ed->key_word_left > 0 && ch == ed->key_word_left)
    {
      symbol (S_WORD_LEFT, 1);
      return;
    }
  if (ed && ed->key_word_right > 0 && ch == ed->key_word_right)
    {
      symbol (S_WORD_RIGHT, 1);
      return;
    }
  /* Home or End pressed twice in a row goes to the top or the bottom. */
  if (ed && ch == LED_KEY_HOME && ed->prev_key == LED_KEY_HOME)
    {
      symbol (S_TOP, 0);
      return;
    }
  if (ed && ch == LED_KEY_END && ed->prev_key == LED_KEY_END)
    {
      symbol (S_BOTTOM, 0);
      return;
    }
  for (int i = 0; bindings[i].level != NULL; i++)
    if (bindings[i].key == ch)
      {
        bindings[i].level (bindings[i].action, bindings[i].value);
        return;
      }
  if (is_text (ch))
    symbol (S_INSERT, ch);
  else
    end_selection (cx);
}

int
handle_input (int ch, Buffer *buf, int *scroll_row, int *scroll_col,
              int *cursor_line, int *cursor_col, int *show_line_numbers,
              char *search_buffer, int *search_mode, char **clipboard,
              const char *filename, Editor *ed)
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
  InputContext *before = cx;
  cx = &ctx;
  route_key (ch);
  settle (&ctx);
  cx = before;
  return 0;
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
