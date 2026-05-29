#include "utils.h"

#include <stdio.h>
#include <stdlib.h>

/**
 * Centralized allocation helpers.
 *
 * These are deliberately simple and brutal: on allocation failure we
 * print a message and terminate. This is intentional for an interactive
 * editor — we would rather crash loudly than silently corrupt state or
 * lose the user's work.
 */

void* xmalloc(size_t n)
{
    void *p = malloc(n);
    if (!p) {
        perror("xmalloc");
        exit(EXIT_FAILURE);
    }
    return p;
}

void* xrealloc(void *p, size_t n)
{
    void *np = realloc(p, n);
    if (!np) {
        perror("xrealloc");
        exit(EXIT_FAILURE);
    }
    return np;
}

void* xcalloc(size_t nmemb, size_t size)
{
    void *p = calloc(nmemb, size);
    if (!p) {
        perror("xcalloc");
        exit(EXIT_FAILURE);
    }
    return p;
}
