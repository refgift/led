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

#include <stddef.h>
#include <stdbool.h>

/* Allocation helpers - abort on failure with perror + exit */
void* xmalloc(size_t n);
void* xrealloc(void *p, size_t n);
void* xcalloc(size_t nmemb, size_t size);

/** Basic filename safety check to mitigate trivial directory traversal.
 *  Rejects: NULL/empty, length > 255, any ".." sequence, control characters.
 */
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
