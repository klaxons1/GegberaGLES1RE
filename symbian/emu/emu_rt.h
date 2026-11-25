#ifndef EMU_RT_H
#define EMU_RT_H

#include <stdarg.h>

void emu_exit(int status) __attribute__((noreturn));
void emu_marker(int id, int a, int b);
void emu_frame(const void *pixels, int width, int height, int tag);
void emu_print(const char *text);
void emu_printf(const char *fmt, ...);
int emu_vformat(char *buf, int cap, const char *fmt, va_list ap);

/* marker ids */
enum
{
    EMU_MARK_SCENE_BEGIN = 1, /* a = scene */
    EMU_MARK_FRAME_BEGIN = 2, /* a = scene, b = frame */
    EMU_MARK_FRAME_END = 3,   /* a = scene, b = frame */
    EMU_MARK_CHECKSUM = 4,    /* a = scene*256+frame, b = crc */
    EMU_MARK_PIPE_BEGIN = 5,  /* a = 1 fast span, 0 JIT */
    EMU_MARK_PIPE_END = 6
};

#endif
