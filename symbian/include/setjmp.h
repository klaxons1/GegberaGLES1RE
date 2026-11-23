#ifndef _GERBERA_SYMBIAN_SETJMP_H
#define _GERBERA_SYMBIAN_SETJMP_H

#ifdef __cplusplus
extern "C" {
#endif

/* ARM EABI SoftVFP jmp_buf: r4-r11, sp, lr (10 words, padded to 16 words) */
typedef int jmp_buf[16];

int  setjmp(jmp_buf env) __attribute__((returns_twice));
void longjmp(jmp_buf env, int val) __attribute__((noreturn));

#ifdef __cplusplus
}
#endif

#endif /* _GERBERA_SYMBIAN_SETJMP_H */
