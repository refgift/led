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

#endif /* UTILS_H */
