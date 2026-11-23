#ifndef PF_SYMBIAN_STDIO_H
#define PF_SYMBIAN_STDIO_H

#include <stddef.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct PF_FILE PF_FILE;
#define FILE PF_FILE

static inline int printf(const char *fmt, ...)
{
    (void)fmt;
    return 0;
}

static inline int fprintf(FILE *stream, const char *fmt, ...)
{
    (void)stream;
    (void)fmt;
    return 0;
}

int sprintf(char *buf, const char *fmt, ...);
int snprintf(char *buf, size_t size, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif
