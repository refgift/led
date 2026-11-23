#include "undo_store.h"
#include "utils/utils.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAGIC "led-undo 1\n"
/* Most changes written per stack. Older ones are left out: the file is
 * rewritten at every save, and a megabyte each time is enough. */
#define PERSIST_MAX 100000
#define RECORD_BYTES 10         /* flags, line, col, character */
#define STACK_LIMIT 1000000     /* never read more than a stack can hold */

static char *dir_override;

void
undo_store_set_dir (const char *dir)
{
  free (dir_override);
  dir_override = NULL;
  if (dir)
    {
      dir_override = xmalloc (strlen (dir) + 1);
      strcpy (dir_override, dir);
    }
}

static uint64_t
fnv1a (uint64_t h, const unsigned char *p, size_t n)
{
  for (size_t i = 0; i < n; i++)
    {
      h ^= p[i];
      h *= 1099511628211ULL;
    }
  return h;
}

#define FNV_START 1469598103934665603ULL

/* Size and hash of the file as it is on disk. */
static int
file_fingerprint (const char *path, uint64_t *hash, uint64_t *size)
{
  FILE *fp = fopen (path, "rb");
  if (!fp)
    return -1;
  unsigned char block[65536];
  uint64_t h = FNV_START;
  uint64_t total = 0;
  size_t n;
  while ((n = fread (block, 1, sizeof block, fp)) > 0)
    {
      h = fnv1a (h, block, n);
      total += n;
    }
  int bad = ferror (fp);
  fclose (fp);
  if (bad)
    return -1;
  *hash = h;
  *size = total;
  return 0;
}

/* The user's own directory for histories. Created if missing. Refused
 * unless it is a real directory, ours, and closed to everyone else. */
static int
store_dir (char *out, size_t out_size, int create)
{
  int n;
  if (dir_override)
    n = snprintf (out, out_size, "%s", dir_override);
  else
    {
      const char *tmp = getenv ("TMPDIR");
      if (!tmp || tmp[0] != '/')
        tmp = "/tmp";
      n = snprintf (out, out_size, "%s/led-%lu", tmp, (unsigned long) getuid ());
    }
  if (n < 0 || (size_t) n >= out_size)
    return -1;
  struct stat st;
  if (lstat (out, &st) != 0)
    {
      if (!create || errno != ENOENT)
        return -1;
      if (mkdir (out, 0700) != 0 && errno != EEXIST)
        return -1;
      if (lstat (out, &st) != 0)
        return -1;
    }
  if (!S_ISDIR (st.st_mode) || st.st_uid != getuid ()
      || (st.st_mode & 077) != 0)
    return -1;
  return 0;
}

int
undo_store_directory (char *out, size_t out_size)
{
  return store_dir (out, out_size, 1);
}

/* The history file for filename: named by a hash of its full path. */
static int
store_path (const char *filename, char *real, char *out, size_t out_size,
            int create)
{
  char dir[PATH_MAX];
  if (!filename || !realpath (filename, real))
    return -1;
  if (store_dir (dir, sizeof dir, create) != 0)
    return -1;
  uint64_t h = fnv1a (FNV_START, (const unsigned char *) real, strlen (real));
  int n = snprintf (out, out_size, "%s/%016llx.undo", dir,
                    (unsigned long long) h);
  return (n < 0 || (size_t) n >= out_size) ? -1 : 0;
}

static void
put32 (unsigned char *p, uint32_t v)
{
  p[0] = (unsigned char) v;
  p[1] = (unsigned char) (v >> 8);
  p[2] = (unsigned char) (v >> 16);
  p[3] = (unsigned char) (v >> 24);
}

static uint32_t
get32 (const unsigned char *p)
{
  return (uint32_t) p[0] | ((uint32_t) p[1] << 8) | ((uint32_t) p[2] << 16)
    | ((uint32_t) p[3] << 24);
}

/* First entry to write: the newest PERSIST_MAX, starting at the head of
 * an undo step so no step is written in part. */
static int
first_kept (const UndoStack *s)
{
  int from = s->count > PERSIST_MAX ? s->count - PERSIST_MAX : 0;
  while (from < s->count && from > 0 && s->changes[from].chained)
    from++;
  return from;
}

static int
write_stack (FILE *fp, const UndoStack *s, int from)
{
  for (int i = from; i < s->count; i++)
    {
      const Change *c = &s->changes[i];
      unsigned char rec[RECORD_BYTES];
      rec[0] = (unsigned char) ((c->is_insert ? 1 : 0) | (c->chained ? 2 : 0));
      put32 (rec + 1, (uint32_t) c->line);
      put32 (rec + 5, (uint32_t) c->col);
      rec[9] = (unsigned char) c->ch;
      if (fwrite (rec, 1, sizeof rec, fp) != sizeof rec)
        return -1;
    }
  return 0;
}

int
undo_store_save (const char *filename, const UndoStack *undo,
                 const UndoStack *redo)
{
  char real[PATH_MAX], path[PATH_MAX], tmp[PATH_MAX];
  uint64_t hash, size;
  if (!undo || !redo || store_path (filename, real, path, sizeof path, 1) != 0)
    return -1;
  if (file_fingerprint (real, &hash, &size) != 0)
    return -1;
  int n = snprintf (tmp, sizeof tmp, "%s.%d", path, (int) getpid ());
  if (n < 0 || (size_t) n >= sizeof tmp)
    return -1;
  /* A new file of our own, never one that is already there or a link. */
  unlink (tmp);
  int fd = open (tmp, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
  if (fd < 0)
    return -1;
  FILE *fp = fdopen (fd, "wb");
  if (!fp)
    {
      close (fd);
      unlink (tmp);
      return -1;
    }
  int ufrom = first_kept (undo);
  int rfrom = first_kept (redo);
  int ok = fprintf (fp, MAGIC "%s\n%llu %llu %d %d\n", real,
                    (unsigned long long) size, (unsigned long long) hash,
                    undo->count - ufrom, redo->count - rfrom) > 0
    && write_stack (fp, undo, ufrom) == 0 && write_stack (fp, redo, rfrom) == 0;
  if (fclose (fp) != 0)
    ok = 0;
  if (!ok || rename (tmp, path) != 0)
    {
      unlink (tmp);
      return -1;
    }
  return 0;
}

static int
read_stack (FILE *fp, UndoStack *s, int count)
{
  if (count == 0)
    return 0;
  s->changes = xmalloc ((size_t) count * sizeof (Change));
  s->capacity = count;
  for (int i = 0; i < count; i++)
    {
      unsigned char rec[RECORD_BYTES];
      if (fread (rec, 1, sizeof rec, fp) != sizeof rec)
        return -1;
      Change *c = &s->changes[i];
      c->is_insert = (rec[0] & 1) != 0;
      c->chained = (rec[0] & 2) != 0;
      c->line = (int) get32 (rec + 1);
      c->col = (int) get32 (rec + 5);
      c->ch = (char) rec[9];
      if ((rec[0] & ~3) != 0 || c->line < 0 || c->col < 0 || c->ch == '\0')
        return -1;
    }
  s->count = count;
  return 0;
}

static void
drop_stack (UndoStack *s)
{
  free (s->changes);
  s->changes = NULL;
  s->count = 0;
  s->capacity = 0;
}

int
undo_store_load (const char *filename, UndoStack *undo, UndoStack *redo)
{
  char real[PATH_MAX], path[PATH_MAX], named[PATH_MAX + 2];
  uint64_t hash, size;
  if (!undo || !redo || undo->count != 0 || redo->count != 0)
    return -1;
  if (store_path (filename, real, path, sizeof path, 0) != 0)
    return -1;
  int fd = open (path, O_RDONLY | O_NOFOLLOW);
  if (fd < 0)
    return -1;
  struct stat st;
  FILE *fp = NULL;
  if (fstat (fd, &st) != 0 || !S_ISREG (st.st_mode) || st.st_uid != getuid ()
      || !(fp = fdopen (fd, "rb")))
    {
      close (fd);
      return -1;
    }
  char magic[sizeof MAGIC];
  unsigned long long want_size = 0, want_hash = 0;
  int ucount = -1, rcount = -1;
  int ok = fread (magic, 1, sizeof MAGIC - 1, fp) == sizeof MAGIC - 1
    && memcmp (magic, MAGIC, sizeof MAGIC - 1) == 0
    && fgets (named, sizeof named, fp) != NULL;
  if (ok)
    {
      named[strcspn (named, "\n")] = '\0';
      char counts[128];
      ok = strcmp (named, real) == 0
        && fgets (counts, sizeof counts, fp) != NULL
        && sscanf (counts, "%llu %llu %d %d", &want_size, &want_hash,
                   &ucount, &rcount) == 4
        && ucount >= 0 && rcount >= 0
        && ucount <= STACK_LIMIT && rcount <= STACK_LIMIT;
    }
  /* The history describes one exact text. Anything else on disk and
   * its positions would point at the wrong characters. */
  ok = ok && file_fingerprint (real, &hash, &size) == 0
    && size == want_size && hash == want_hash;
  /* Exactly the promised records, and nothing after them. */
  ok = ok && st.st_size == ftell (fp)
    + ((off_t) ucount + (off_t) rcount) * RECORD_BYTES;
  ok = ok && read_stack (fp, undo, ucount) == 0
    && read_stack (fp, redo, rcount) == 0;
  fclose (fp);
  if (!ok)
    {
      drop_stack (undo);
      drop_stack (redo);
      return -1;
    }
  return 0;
}
