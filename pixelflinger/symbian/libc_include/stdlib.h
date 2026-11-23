#ifndef PF_SYMBIAN_STDLIB_H
#define PF_SYMBIAN_STDLIB_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void *malloc(size_t size);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);
void  free(void *ptr);
void  abort(void) __attribute__((noreturn));
void  qsort(void *base, size_t nmemb, size_t size,
            int (*compar)(const void *, const void *));

static inline int abs(int x)
{
    return (x < 0) ? -x : x;
}

static inline int atoi(const char *s)
{
    int sign = 1, val = 0;
    if (!s) return 0;
    while (*s == ' ' || *s == '\t' || *s == '\n') ++s;
    if (*s == '-') { sign = -1; ++s; }
    else if (*s == '+') { ++s; }
    while (*s >= '0' && *s <= '9') {
        val = val * 10 + (*s - '0');
        ++s;
    }
    return sign * val;
}

#ifdef __cplusplus
}
#endif

#endif
