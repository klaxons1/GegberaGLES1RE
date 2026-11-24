/* No-output stdio functions for the freestanding Symbian runtime. */
#ifndef SYMBIAN_STDIO_H
#define SYMBIAN_STDIO_H

#include <stdarg.h>
#include <stddef.h>

typedef struct _sym_FILE FILE;
#define stdout ((FILE *)0)
#define stderr ((FILE *)0)

static inline int fflush(FILE *f) { (void)f; return 0; }
static inline int fprintf(FILE *f, const char *fmt, ...) { (void)f; (void)fmt; return 0; }
static inline int vfprintf(FILE *f, const char *fmt, va_list ap) { (void)f; (void)fmt; (void)ap; return 0; }

#endif /* SYMBIAN_STDIO_H */
