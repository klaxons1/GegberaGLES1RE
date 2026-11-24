/* Host replacement for m3gcore11/src/m3g_symbian_gl.cpp + m3g_symbian.cpp.
 *
 * The Symbian glue maps CFbsBitmap handles to M3G formats; on the host the
 * native bitmap is PixelFlinger's PfHostBitmap (pixelflinger/symbian/
 * pf_host_bitmap.h), the same object egl_symbian.cpp accepts as a pixmap in
 * PF_HEADLESS_UNIX builds. Test infrastructure only.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "M3G/m3g_core.h"
#include "../../symbian/pf_host_bitmap.h"

int g_m3gGlueLocks;          /* m3gglLockNativeBitmap calls (buffered path) */
int g_m3gGlueParamQueries;

static PfHostBitmap* toBitmap(M3GNativeBitmap h)
{
    PfHostBitmap* b = (PfHostBitmap*)(uintptr_t)h;
    return (b && b->magic == PF_HOST_BITMAP_MAGIC) ? b : NULL;
}

/* Same table as m3gSymbianPixelFormat() in m3g_symbian_gl.cpp. */
static M3GPixelFormat formatOf(int mode)
{
    switch (mode) {
    case PF_HOST_ECOLOR4K:    return M3G_RGB4;
    case PF_HOST_ECOLOR64K:   return M3G_RGB565;
    case PF_HOST_ECOLOR16M:   return M3G_RGB8;
    case PF_HOST_ECOLOR16MU:  return M3G_BGR8_32;
    case PF_HOST_ECOLOR16MA:
    case PF_HOST_ECOLOR16MAP: return M3G_BGRA8;
    default:                  return M3G_NO_FORMAT;
    }
}

M3Gbool m3gglLockNativeBitmap(M3GNativeBitmap bitmap, M3Gubyte **ptr, M3Gsizei *stride)
{
    PfHostBitmap* b = toBitmap(bitmap);
    if (!b) return M3G_FALSE;
    ++g_m3gGlueLocks;
    ++b->lockCount;
    *ptr = b->data;
    *stride = b->stride;
    return M3G_TRUE;
}

void m3gglReleaseNativeBitmap(M3GNativeBitmap bitmap)
{
    PfHostBitmap* b = toBitmap(bitmap);
    if (b) --b->lockCount;
}

M3Gbool m3gglGetNativeBitmapParams(M3GNativeBitmap bitmap, M3GPixelFormat *format,
                                   M3Gint *width, M3Gint *height, M3Gint *pixels)
{
    PfHostBitmap* b = toBitmap(bitmap);
    if (!b) return M3G_FALSE;
    ++g_m3gGlueParamQueries;
    *format = formatOf(b->displayMode);
    *width = b->width;
    *height = b->height;
    *pixels = (M3Gint)(intptr_t)b->data;
    return M3G_TRUE;
}

M3Gbool m3gglGetNativeWindowParams(M3GNativeWindow wnd, M3GPixelFormat *format,
                                   M3Gint *width, M3Gint *height)
{
    (void)wnd; (void)format; (void)width; (void)height;
    return M3G_FALSE; /* no window system on host */
}

/* m3g_loader.c only includes the zlib-based inflater for non-generic
 * targets; the host has no zlib, so compressed .m3g sections are rejected. */
M3Gsizei m3gInflateBlock(M3Gsizei srcLength, const M3Gubyte *src,
                         M3Gsizei dstLength, M3Gubyte *dst)
{
    (void)srcLength; (void)src; (void)dstLength; (void)dst;
    return 0;
}

void m3gAssertFailed(const char *filename, int line)
{
    fprintf(stderr, "M3G_ASSERT failed: %s:%d\n", filename, line);
    fflush(stderr);
    abort();
}
