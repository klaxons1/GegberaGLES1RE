/* pf_host_bitmap.h - host-only stand-in for Symbian CFbsBitmap.
 *
 * Headless builds (PF_HEADLESS_UNIX) use this descriptor as the
 * EGLNativePixmapType so that eglCreatePixmapSurface, eglCopyBuffers,
 * eglWaitNative and glFinish run the same pixmap conversion code as the
 * Symbian DLL. It is not part of the Symbian SDK.
 */
#ifndef PF_HOST_BITMAP_H
#define PF_HOST_BITMAP_H

#include <stdint.h>

#define PF_HOST_BITMAP_MAGIC 0x4D424650u /* "PFBM" */

/* Real Symbian TDisplayMode values. */
enum {
    PF_HOST_ECOLOR64K   = 7,
    PF_HOST_ECOLOR16M   = 8,
    PF_HOST_ECOLOR4K    = 10,
    PF_HOST_ECOLOR16MU  = 11,
    PF_HOST_ECOLOR16MA  = 12,
    PF_HOST_ECOLOR16MAP = 13
};

typedef struct PfHostBitmap {
    uint32_t magic;       /* PF_HOST_BITMAP_MAGIC */
    int32_t  width;
    int32_t  height;
    int32_t  displayMode; /* PF_HOST_ECOLOR* */
    int32_t  stride;      /* bytes; must equal the Symbian word-aligned stride */
    uint8_t* data;        /* top-down rows, Symbian channel order */
    int32_t  lockCount;   /* incremented while EGL is inside LockHeap */
    int32_t  maxLockCount;
} PfHostBitmap;

/* Symbian CFbsBitmap::ScanLineLength rule: rows are padded to 32 bits. */
static inline int32_t pf_host_bitmap_stride(int32_t width, int32_t mode)
{
    int32_t bpp = (mode == PF_HOST_ECOLOR16M) ? 3 :
                  (mode == PF_HOST_ECOLOR64K || mode == PF_HOST_ECOLOR4K) ? 2 : 4;
    return (width * bpp + 3) & ~3;
}

#endif
