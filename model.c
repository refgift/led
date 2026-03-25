#include "model.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <regex.h>
#include <sys/stat.h>
#define MAX_FILE_SIZE (10 * 1024 * 1024)       // 10MB
#define MAX_LINE_LENGTH 10000
static void *
safe_malloc (int size)
{
  void *p = malloc (size);
  if (!p)
    {
      fprintf (stderr, "Error: memory allocation failed for %d bytes\n",
                size);
      return NULL;
    }
  return p;
}
static char *
safe_strdup (const char *s)
{
  int len = strlen (s);
  char *d = safe_malloc (len + 1);
  if (!d)
    {
      return NULL;
    }
  memcpy (d, s, len + 1);
  return d;
}
void
buffer_init (Buffer *buf)
{
  buf->lines = NULL;
  buf->num_lines = 0;
  buf->capacity = 0;
}
void
buffer_free (Buffer *buf)
{
  for (int i = 0; i < buf->num_lines; i++)
    {
      free (buf->lines[i]);
    }
  free (buf->lines);
  buf->lines = NULL;
  buf->num_lines = 0;
  buf->capacity = 0;
}
int
buffer_load_from_file (Buffer *buf, const char *filename)
{
  struct stat st;
  if (stat (filename, &st) != 0)
    {
      return -1;
    }
  if (st.st_size > MAX_FILE_SIZE)
    {
      return -1;                  // File too large
    }
  FILE *fp = fopen (filename, "rb");
  if (!fp)
    {
      return -1;
    }
  // Read entire file into temp buffer
  int temp_cap = 1024;
  char *temp = malloc (temp_cap);
  if (!temp)
    {
      fclose (fp);
      return -1;
    }
  int pos = 0;
  int c;
  while ((c = fgetc (fp)) != EOF)
    {
      if (pos >= temp_cap)
        {
          temp_cap *= 2;
          char *new_temp = realloc (temp, temp_cap);
          if (!new_temp)
            {
              free (temp);
              fclose (fp);
              return -1;
            }
          temp = new_temp;
        }
      temp[pos++] = c;
    }
  fclose (fp);
  temp[pos] = '\0';
  // Safety checks
  if (pos > MAX_FILE_SIZE)
    {
      free (temp);
      return -1;                // File too large
    }
  // Check for null bytes (indicates binary file)
  if (memchr (temp, '\0', pos) != NULL)
    {
      free (temp);
      return -1;                // Binary file or null bytes
    }
  // Split into lines
  int start = 0;
  for (int i = 0; i <= pos; i++)
    {
      if (temp[i] == '\n' || temp[i] == '\0')
        {
          int len = i - start;
          if (len > MAX_LINE_LENGTH)
            {
              len = MAX_LINE_LENGTH; // Truncate long lines
            }
          char *line = malloc (len + 1);
          if (!line)
            {
              free (temp);
              return -1;
            }
          memcpy (line, &temp[start], len);
          line[len] = '\0';
          if (buffer_insert_line (buf, buf->num_lines, line) != 0)
            {
              free (line);
              free (temp);
              return -1;
            }
          free (line);
          start = i + 1;
        }
    }
  free (temp);
  return 0;
}
int
buffer_delete_char (Buffer *buf, int line, int col)
{
  if (line >= buf->num_lines)
    {
      return -1;
    }
  int len = strlen (buf->lines[line]);
  if (col < len)
    {
      // Delete char in line
      char *current = buf->lines[line];
      char *new_str = malloc (len);
      if (!new_str)
        {
          fprintf (stderr,
                   "Error: memory allocation failed in buffer_delete_char\n");
          return -1;
        }
      memcpy (new_str, current, col);
      memcpy (&new_str[col], &current[col + 1], len - col);
      new_str[len - 1] = '\0';
      free (buf->lines[line]);
      buf->lines[line] = new_str;
    }
  else if (line < buf->num_lines - 1)
    {
      // Merge with next line
      char *current = buf->lines[line];
      char *next = buf->lines[line + 1];
      int len1 = strlen (current);
      int len2 = strlen (next);
      char *merged = malloc (len1 + len2 + 1);
      if (!merged)
        {
          fprintf (stderr,
                   "Error: memory allocation failed in buffer_delete_char\n");
          return -1;
        }
      memcpy (merged, current, len1);
      memcpy (&merged[len1], next, len2 + 1);
      free (buf->lines[line]);
      free (buf->lines[line + 1]);
      buf->lines[line] = merged;
      // Shift remaining lines
      for (int i = line + 1; i < buf->num_lines - 1; i++)
        {
          buf->lines[i] = buf->lines[i + 1];
        }
      buf->num_lines--;
    }
  return 0;
}
int
buffer_delete_range (Buffer *buf, int start_line, int start_col,
                     int end_line, int end_col)
{
  if (start_line < 0 || end_line < 0 || start_line >= buf->num_lines
      || end_line >= buf->num_lines)
    {
      return -1;
    }
  if (start_line > end_line
      || (start_line == end_line && start_col > end_col))
    {
      int temp = start_line;
      start_line = end_line;
      end_line = temp;
      temp = start_col;
      start_col = end_col;
      end_col = temp;
    }
  if (start_line == end_line)
    {
      // Delete chars in line
      char *current = buf->lines[start_line];
      int len = strlen (current);
      if (start_col >= len)
        {
          return -1;
        }
      if (end_col > len)
        {
          end_col = len;
        }
      int new_len = len - (end_col - start_col);
      char *new_str = malloc (new_len + 1);
      if (!new_str)
        {
          fprintf (stderr,
                   "Error: memory allocation failed in buffer_delete_range\n");
          return -1;
        }
      memcpy (new_str, current, start_col);
      memcpy (&new_str[start_col], &current[end_col], len - end_col + 1);
      free (buf->lines[start_line]);
      buf->lines[start_line] = new_str;
    }
  else
    {
      // Delete from start_col to end of start_line
      char *start_line_str = buf->lines[start_line];
      char *new_start = malloc (start_col + 1);
      if (!new_start)
        {
          fprintf (stderr,
                   "Error: memory allocation failed in buffer_delete_range\n");
          return -1;
        }
      memcpy (new_start, start_line_str, start_col);
      new_start[start_col] = '\0';
      free (buf->lines[start_line]);
      buf->lines[start_line] = new_start;
      // Delete from start of end_line to end_col
      char *end_line_str = buf->lines[end_line];
      int end_len = strlen (end_line_str);
      char *new_end = malloc (end_len - end_col + 1);
      if (!new_end)
        {
          fprintf (stderr,
                   "Error: memory allocation failed in buffer_delete_range\n");
          return -1;
        }
      memcpy (new_end, &end_line_str[end_col], end_len - end_col + 1);
      free (buf->lines[end_line]);
      buf->lines[end_line] = new_end;
      // Delete lines in between
      for (int i = 0; i < end_line - start_line - 1; i++)
        {
          if (buffer_delete_line (buf, start_line + 1) != 0)
            {
              fprintf (stderr,
                       "Error: failed to delete line in buffer_delete_range\n");
              return -1;
            }
        }
      // Merge start and end
      char *s = buf->lines[start_line];
      char *e = buf->lines[start_line + 1];
      int sl = strlen (s);
      int el = strlen (e);
      char *merged = malloc (sl + el + 1);
      if (!merged)
        {
          fprintf (stderr,
                   "Error: memory allocation failed in buffer_delete_range\n");
          return -1;
        }
      memcpy (merged, s, sl);
      memcpy (&merged[sl], e, el + 1);
      free (buf->lines[start_line]);
      free (buf->lines[start_line + 1]);
      buf->lines[start_line] = merged;
      // Shift remaining lines
      for (int i = start_line + 1; i < buf->num_lines - 1; i++)
        {
          buf->lines[i] = buf->lines[i + 1];
        }
      buf->num_lines--;
    }
  return 0;
}
int
buffer_num_lines (const Buffer *buf)
{
  return buf->num_lines;
}
const char *
buffer_get_line (const Buffer *buf, int line)
{
  if (line >= buf->num_lines)
    {
      return NULL;
    }
  return buf->lines[line];
}
int
buffer_get_line_length (const Buffer *buf, int line)
{
  if (line >= buf->num_lines)
    {
      return 0;
    }
  return strlen (buf->lines[line]);
}
char
buffer_get_char (const Buffer *buf, int line, int col)
{
  if (line >= buf->num_lines)
    {
      return '\0';
    }
  const char *ln = buf->lines[line];
  int len = strlen (ln);
  if (col >= len)
    {
      return '\0';
    }
  return ln[col];
}
int
buffer_insert_line (Buffer *buf, int line, const char *content)
{
  if (line > buf->num_lines)
    {
      return -1;
    }
  if (buf->num_lines >= buf->capacity)
    {
      buf->capacity =
        (buf->capacity == 0) ? INITIAL_LINES_CAPACITY : buf->capacity * 2;
      char **new_lines =
        realloc (buf->lines, buf->capacity * sizeof (char *));
      if (!new_lines)
        {
          fprintf (stderr,
                   "Error: memory allocation failed in buffer_insert_line\n");
          return -1;
        }
      buf->lines = new_lines;
    }
  // Shift lines down
  for (int i = buf->num_lines; i > line; i--)
    {
      buf->lines[i] = buf->lines[i - 1];
    }
  buf->lines[line] = safe_strdup (content);
  if (!buf->lines[line])
    {
      // Shift back on failure
      for (int i = line; i < buf->num_lines; i++)
        {
          buf->lines[i] = buf->lines[i + 1];
        }
      return -1;
    }
  buf->num_lines++;
  return 0;
}
int
buffer_delete_line (Buffer *buf, int line)
{
  if (line >= buf->num_lines)
    {
      return -1;
    }
  free (buf->lines[line]);
  // Shift lines up
  for (int i = line; i < buf->num_lines - 1; i++)
    {
      buf->lines[i] = buf->lines[i + 1];
    }
  buf->num_lines--;
  return 0;
}
int
buffer_insert_char (Buffer *buf, int line, int col, char c)
{
  if (line >= buf->num_lines)
    {
      return -1;
    }
  if (c == '\n')
    {
      // Split line at col
      char *ln = buf->lines[line];
      char *before = malloc (col + 1);
      if (!before)
        {
          fprintf (stderr,
                   "Error: memory allocation failed in buffer_insert_char\n");
          return -1;
        }
      memcpy (before, ln, col);
      before[col] = '\0';
      char *after = safe_strdup (ln + col);
      if (!after)
        {
          free (before);
          return -1;
        }
      free (buf->lines[line]);
      buf->lines[line] = before;
      if (buffer_insert_line (buf, line + 1, after) != 0)
        {
          free (after);
          return -1;
        }
      free (after);
    }
  else
    {
      // Insert char
      char *ln = buf->lines[line];
      int len = strlen (ln);
      if (col > len)
        {
          col = len;
        }
      char *new_ln = malloc (len + 2);
      if (!new_ln)
        {
          fprintf (stderr,
                   "Error: memory allocation failed in buffer_insert_char\n");
          return -1;
        }
      memcpy (new_ln, ln, col);
      new_ln[col] = c;
      memcpy (&new_ln[col + 1], ln + col, len - col + 1);
      free (buf->lines[line]);
      buf->lines[line] = new_ln;
    }
  return 0;
}
int
buffer_insert_text (Buffer *buf, int line, int col, const char *text)
{
  if (line >= buf->num_lines)
    {
      return -1;
    }
  const char *p = text;
  int current_line = line;
  int current_col = col;
  while (*p)
    {
      if (*p == '\n')
        {
          if (buffer_insert_char (buf, current_line, current_col, '\n') != 0)
            {
              fprintf (stderr,
                       "Error: failed to insert char in buffer_insert_text\n");
              return -1;
            }
          current_line++;
          current_col = 0;
          p++;
        }
      else
        {
          const char *end = p;
          while (*end && *end != '\n')
            {
              end++;
            }
          int len = end - p;
          char *ln = buf->lines[current_line];
          int ln_len = strlen (ln);
          if (current_col > ln_len)
            {
              current_col = ln_len;
            }
          char *new_ln = malloc (ln_len + len + 1);
          if (!new_ln)
            {
              fprintf (stderr,
                       "Error: memory allocation failed in buffer_insert_text\n");
              return -1;
            }
          memcpy (new_ln, ln, current_col);
          memcpy (new_ln + current_col, p, len);
          memcpy (new_ln + current_col + len, ln + current_col,
                  ln_len - current_col + 1);
          free (buf->lines[current_line]);
          buf->lines[current_line] = new_ln;
          current_col += len;
          p = end;
        }
    }
  return 0;
}
int
buffer_save_to_file (const Buffer *buf, const char *filename)
{
  FILE *fp = fopen (filename, "wb");
  if (!fp)
    {
      return -1;
    }
  for (int i = 0; i < buf->num_lines; i++)
    {
      fputs (buf->lines[i], fp);
      if (i < buf->num_lines - 1)
        {
          fputc ('\n', fp);
        }
    }
  fclose (fp);
  return 0;
}
void
buffer_replace_all (Buffer *buf, const char *search_regex,
                    const char *replace_str)
{
  if (strlen (search_regex) > 100)
    {
      return;                     // Pattern too long
    }
  regex_t reg;
  if (regcomp (&reg, search_regex, REG_EXTENDED) != 0)
    {
      return;
    }
  int replace_len = strlen (replace_str);
  for (int i = 0; i < buf->num_lines; i++)
    {
      char *line = buf->lines[i];
      int len = strlen (line);
      // Build new line
      char *new_line = NULL;
      int new_cap = 0;
      int used = 0;
       int pos = 0;
       regmatch_t match;
       while (regexec (&reg, line + pos, 1, &match, 0) == 0)
         {
            // Append before match
            int before_len = (int) match.rm_so;
            if (used + before_len >= new_cap)
              {
                new_cap = (new_cap == 0) ? 128 : new_cap * 2;
                while (used + before_len >= new_cap)
                  {
                    new_cap *= 2;
                  }
                char *temp = realloc (new_line, new_cap);
                if (!temp)
                  {
                    free (new_line);
                    regfree (&reg);
                    return;
                  }
                new_line = temp;
              }
            memcpy (new_line + used, line + pos, before_len);
            used += before_len;
            // Append replacement
            if (used + replace_len >= new_cap)
              {
                new_cap = (new_cap == 0) ? 128 : new_cap * 2;
                while (used + replace_len >= new_cap)
                  {
                    new_cap *= 2;
                  }
                char *temp = realloc (new_line, new_cap);
                if (!temp)
                  {
                    free (new_line);
                    regfree (&reg);
                    return;
                  }
                new_line = temp;
              }
            memcpy (new_line + used, replace_str, replace_len);
            used += replace_len;
            pos += match.rm_eo;
        }
      // Append rest
      int rest_len = len - pos;
      if (used + rest_len >= new_cap)
        {
          new_cap = used + rest_len + 1;
          char *temp = realloc (new_line, new_cap);
          if (!temp)
            {
              free (new_line);
              regfree (&reg);
              return;
            }
          new_line = temp;
        }
      memcpy (new_line + used, line + pos, rest_len);
      used += rest_len;
      new_line[used] = '\0';
      // Replace line
      free (buf->lines[i]);
      buf->lines[i] = new_line;
    }
  regfree (&reg);
}