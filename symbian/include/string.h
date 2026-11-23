/* Freestanding C99 <string.h> for Symbian EKA2 ARM EABI. */
#ifndef SYMBIAN_STRING_H
#define SYMBIAN_STRING_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void  *memcpy(void *dst, const void *src, size_t n);
void  *memmove(void *dst, const void *src, size_t n);
void  *memset(void *s, int c, size_t n);
int    memcmp(const void *s1, const void *s2, size_t n);
int    strcmp(const char *s1, const char *s2);
size_t strlen(const char *s);

#ifdef __cplusplus
}
#endif

#endif /* SYMBIAN_STRING_H */
