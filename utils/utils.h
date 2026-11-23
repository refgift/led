#ifndef UTILS_H
#define UTILS_H

/**
 * utils.h - Shared low-level utilities for the led editor.
 *
 * This is a "hot" module. Keep it extremely clean, well-documented,
 * and low-entropy.
 *
 * Currently contains:
 *   - xmalloc / xrealloc / xcalloc : Allocation helpers that abort on OOM
 *     with a clear error message. Centralized here so the policy is defined
 *     in exactly one place.
 *
 * Philosophy: Fail fast and loudly on memory exhaustion rather than
 * returning NULL and hoping callers check. This matches the editor's
 * overall data-safety priority.
 */

#include <limits.h>
#include <stddef.h>
#include <stdbool.h>

/* Allocation helpers. On failure they free the reserve, call the
 * failure handler once (to save work and restore the terminal), print
 * a message and exit. */
void* xmalloc(size_t n);
void* xrealloc(void *p, size_t n);
void* xcalloc(size_t nmemb, size_t size);
/** Keep n bytes allocated for the failure handler to use (0 frees it). */
void xalloc_reserve(size_t n);
/** Run fn once when an allocation fails, before led exits. */
void xalloc_on_failure(void (*fn)(void));

/** Longest path led will open: the system limit, less room for the
 *  suffixes led adds beside the file (.recovery, .bak.N, .ledtmp.PID). */
#define LED_PATH_LIMIT (PATH_MAX - 32)

/** True if fn can be opened and shown: not NULL or empty, no longer than
 *  LED_PATH_LIMIT, and free of control characters (which would corrupt
 *  the status line). Any directory is allowed, ".." included: an editor
 *  opens the path it is given, with the user's own permissions. */
bool is_filename_safe(const char *fn);

/* UTF-8 helpers (byte-oriented buffer; display width via wcwidth) */
int utf8_char_len(const char *s, int maxlen);
int utf8_char_width(const char *s, int maxlen);
int utf8_visual_width(const char *s, int byte_len, int tab_width, int start_vis);
/* Max bytes of s[0..byte_len) that fit in max_vis display columns (tabs expanded). */
int utf8_fit_bytes(const char *s, int byte_len, int max_vis, int tab_width, int start_vis);

/* Border filter — removes curses box() artifacts from pasted text.
 * Xterm block-select in copy mode captures the outer border (|, ─, ┌ etc.)
 * along with text. This filter strips those per-line so `tr -d '|+'`
 * is no longer needed. Returns heap-allocated copy (caller free) or NULL.
 * Pure horizontal border lines (e.g. "┌──────┐") are dropped.
 */
char* border_filter_dup(const char *text);
int   border_filter_is_pure_border_line(const char *line, size_t len);

#endif /* UTILS_H */
