/* eglSymbian.c - portable hosted platform backend for Gerbera EGL 1.1
 * (ported from eglSymbian.inl & PE .rdata s_validColors at 0x4702B4).
 *
 * Replaces Symbian OS Dll::SetTls / UserHeap / CFbsBitmap / RWindow / CWindowGc
 * calls with portable C99 + POSIX mmap equivalents while preserving every
 * original config fixup, surface lock/resize/swap/copy behavior, and
 * uninitialized TLS error sentinel encoding.
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if defined(__unix__) || defined(__APPLE__)
#include <sys/mman.h>
#ifndef MAP_ANONYMOUS
#ifdef MAP_ANON
#define MAP_ANONYMOUS MAP_ANON
#else
#define MAP_ANONYMOUS 0x20
#endif
#endif
#endif

#include "gerbera/egl/eglInternal.h"
#include "gerbera/grState.h"
#include "gerbera/hc/hcBackend.h"
#include "hg/hgInt32.h"
#include "hg/hgMemory.h"

/* --- PE .rdata s_validColors (0x4702B4..0x4702E3) -------------------------- */
static const eglValidColorInfo_s s_validColors[6] =
{
    { 8, 8, 8, 8, EGL_SYMBIAN_ECOLOR16MU },
    { 8, 8, 8, 8, EGL_SYMBIAN_ECOLOR16MA },
    { 5, 6, 5, 0, EGL_SYMBIAN_ECOLOR64K  },
    { 8, 8, 8, 0, EGL_SYMBIAN_ECOLOR16MU },
    { 8, 8, 8, 0, EGL_SYMBIAN_ECOLOR16MA },
    { 4, 4, 4, 0, EGL_SYMBIAN_ECOLOR4K   }
};

/* --- Hosted TLS & default display mode ------------------------------------- */
static void      *s_tlsSlot = NULL;
static eglState_s s_directState;
static int        s_defaultDisplayMode = EGL_SYMBIAN_ECOLOR64K;

void egliSetDefaultDisplayMode(int displayMode)
{
    s_defaultDisplayMode = displayMode;
}

void egliSetGlobal(void *data)
{
    s_tlsSlot = data;
}

void *egliGetGlobal(void)
{
    return s_tlsSlot;
}

eglState_s *egliGetState(void)
{
    void *state = egliGetGlobal();

    if ((uintptr_t)state <= 3u)
        return NULL;
    return (eglState_s *)state;
}

/* Compatibility helper for standalone api_opengl tests without eglInitialize */
void egliSetCurrentContext(oglContext_s *context)
{
    if (context != NULL)
    {
        hgMemSet(&s_directState, 0, sizeof(s_directState));
        s_directState.currentESContext = context;
        s_directState.errorCode = EGL_SUCCESS;
        s_directState.displayMode = s_defaultDisplayMode;
        s_tlsSlot = &s_directState;
    }
    else if (s_tlsSlot == &s_directState)
    {
        s_directState.currentESContext = NULL;
        s_tlsSlot = NULL;
    }
}

/* --- Hosted heap & executable code allocator ------------------------------- */

struct grExecutable_s
{
    void *data;
    int   size;
};

void *eglAlloc(unsigned int size)
{
    EGL_ASSERT_AT("eglSymbian.inl", 252, "size > 0", size > 0);
    return malloc((size_t)size);
}

void eglFree(void *ptr)
{
    if (ptr != NULL)
        free(ptr);
}

grExecutable_s *eglAllocExecutable(unsigned int size)
{
    grExecutable_s *exec;

    EGL_ASSERT_AT("eglSymbian.inl", 281, "size > 0", size > 0);
    exec = (grExecutable_s *)malloc(sizeof(grExecutable_s));
    if (exec == NULL)
        return NULL;
    exec->data = malloc((size_t)size);
    if (exec->data == NULL)
    {
        free(exec);
        return NULL;
    }
    exec->size = (int)size;
    return exec;
}

void eglFreeExecutable(grExecutable_s *executable)
{
    EGL_ASSERT_AT("eglSymbian.inl", 306, "executable", executable != NULL);
    free(executable->data);
    free(executable);
}

int eglGetExecutableSize(grExecutable_s *executable)
{
    EGL_ASSERT_AT("eglSymbian.inl", 321, "executable", executable != NULL);
    return executable->size;
}

void *eglGetExecutableData(grExecutable_s *executable)
{
    EGL_ASSERT_AT("eglSymbian.inl", 334, "executable", executable != NULL);
    return executable->data;
}

void *grGetBackend(void)
{
    return HCX86Backend_create();
}

/* --- Numeric & Config Helpers ---------------------------------------------- */

int egliNextPowerOfTwo(int v)
{
    int u = v | (v >> 1);
    u |= u >> 2;
    u |= u >> 4;
    u |= u >> 8;
    return (u | (u >> 16)) + 1;
}

int egliGetTotalConfigs(void)
{
    return EGL_NUM_BUFFER_FORMATS;
}

eglState_s *egliInitState(void)
{
    eglState_s *state = (eglState_s *)malloc(sizeof(eglState_s));

    if (state == NULL)
        return NULL;

    hgMemSet(state, 0, sizeof(*state));
    state->errorCode = EGL_SUCCESS;
    state->displayMode = s_defaultDisplayMode;
    state->heap = state;
    state->ownEnvironment = 1;
    egliSetGlobal(state);
    return state;
}

void egliDestroyState(void)
{
    eglState_s *state = egliGetState();

    EGL_ASSERT_AT("eglSymbian.inl", 622, "state", state != NULL);
    free(state);
    egliSetGlobal(NULL);
}

/* --- Native Window & Pixmap Backend ---------------------------------------- */

static eglNativePixmap_s *egliCreateBackBufferPixmap(int width, int height, int displayMode, int bytesPerPixel)
{
    eglNativePixmap_s *bmp;
    size_t byteCount;

    bmp = (eglNativePixmap_s *)eglAlloc(sizeof(eglNativePixmap_s));
    if (bmp == NULL)
        return NULL;

    hgMemSet(bmp, 0, sizeof(*bmp));
    bmp->width = width;
    bmp->height = height;
    bmp->stride = width * bytesPerPixel;
    bmp->displayMode = displayMode;
    bmp->locked = 0;

    byteCount = (size_t)bmp->stride * (size_t)height;
    if (byteCount > 0)
    {
        bmp->data = eglAlloc((unsigned int)byteCount);
        if (bmp->data == NULL)
        {
            eglFree(bmp);
            return NULL;
        }
        hgMemSet(bmp->data, 0, (int)byteCount);
    }
    return bmp;
}

static void egliDestroyBackBufferPixmap(eglNativePixmap_s *bmp)
{
    if (bmp == NULL)
        return;
    if (bmp->data != NULL)
        eglFree(bmp->data);
    eglFree(bmp);
}

int egliIsValidNativeWindow(eglNativeWindow_s *window)
{
    if (window == NULL)
        return 1; /* Symbian / Gerbera allows NULL window -> owned default window */
    if (window->width < 0 || window->height < 0)
        return 0;
    return 1;
}

int egliIsValidNativePixmap(eglNativePixmap_s *pixmap)
{
    if (pixmap == NULL)
        return 0;
    if (pixmap->width < 0 || pixmap->height < 0)
        return 0;
    if (pixmap->width > 0 && pixmap->height > 0 && pixmap->data == NULL)
        return 0;
    return 1;
}

void egliGetNativeWindowSize(eglWindowSurface *winSurface, int *width, int *height)
{
    EGL_ASSERT_AT("eglSymbian.inl", 1111, "winSurface && width && height",
                  winSurface != NULL && width != NULL && height != NULL);
    if (winSurface->nativeWindow != NULL)
    {
        *width = winSurface->nativeWindow->width;
        *height = winSurface->nativeWindow->height;
    }
    else
    {
        *width = 0;
        *height = 0;
    }
}

void egliGetNativePixmapSize(eglNativePixmap_s *pixmap, int *width, int *height)
{
    EGL_ASSERT_AT("eglSymbian.inl", 1134, "egliIsValidNativePixmap(pixmap) && width && height",
                  egliIsValidNativePixmap(pixmap) != 0 && width != NULL && height != NULL);
    *width = pixmap->width;
    *height = pixmap->height;
}

int egliInitWindowSurface(eglWindowSurface *winSurface)
{
    eglState_s *state = egliGetState();

    EGL_ASSERT_AT("eglSymbian.inl", 734, "winSurface && state",
                  winSurface != NULL && state != NULL);

    if (winSurface->nativeWindow == NULL)
    {
        eglNativeWindow_s *owned = (eglNativeWindow_s *)eglAlloc(sizeof(eglNativeWindow_s));
        if (owned == NULL)
            return 0;
        hgMemSet(owned, 0, sizeof(*owned));
        owned->width = 320;
        owned->height = 240;
        owned->displayMode = state->displayMode;
        owned->stride = 0;
        winSurface->nativeWindow = owned;
        winSurface->ownWindow = 1;
    }
    else
    {
        winSurface->ownWindow = 0;
        if (winSurface->nativeWindow->displayMode == 0)
            winSurface->nativeWindow->displayMode = state->displayMode;
    }

    winSurface->windowGc = winSurface->nativeWindow;
    return 1;
}

void egliWindowSurfaceDestroy(eglWindowSurface *winSurface)
{
    if (winSurface == NULL)
        return;

    if (winSurface->backBufferBitmap != NULL)
    {
        egliDestroyBackBufferPixmap(winSurface->backBufferBitmap);
        winSurface->backBufferBitmap = NULL;
    }

    if (winSurface->ownWindow != 0 && winSurface->nativeWindow != NULL)
    {
        eglFree(winSurface->nativeWindow);
        winSurface->nativeWindow = NULL;
        winSurface->ownWindow = 0;
    }
    winSurface->windowGc = NULL;
    winSurface->surface.colorBuffer = NULL;
}

int egliConfigPixmapMatch(int configID, eglNativePixmap_s *pixmap)
{
    eglConfig cfg;
    int i;

    if (egliIsValidNativePixmap(pixmap) == 0)
        return 0;

    eglFillConfig(&cfg, configID);
    for (i = 0; i < 6; ++i)
    {
        if (cfg.redBits == s_validColors[i].r &&
            cfg.greenBits == s_validColors[i].g &&
            cfg.blueBits == s_validColors[i].b &&
            cfg.alphaBits == s_validColors[i].a &&
            pixmap->displayMode == s_validColors[i].displayMode)
        {
            return 1;
        }
    }
    return 0;
}

int egliInitPixmapSurface(eglPixmapSurface *pixSurface)
{
    EGL_ASSERT_AT("eglSymbian.inl", 1042, "pixSurface && egliIsValidNativePixmap(pixSurface->nativePixmap)",
                  pixSurface != NULL && egliIsValidNativePixmap(pixSurface->nativePixmap) != 0);

    pixSurface->surface.colorBuffer = pixSurface->nativePixmap->data;
    pixSurface->surface.colorBufferStride = pixSurface->nativePixmap->stride;
    return 1;
}

void egliPixmapSurfaceDestroy(eglPixmapSurface *pixSurface)
{
    if (pixSurface != NULL)
    {
        pixSurface->nativePixmap = NULL;
        pixSurface->surface.colorBuffer = NULL;
    }
}

int egliSurfaceResized(eglSurface_s *surface)
{
    EGL_ASSERT_AT("eglSymbian.inl", 1495, "surface", surface != NULL);

    if (surface->type == EGL_SURFACE_WINDOW)
    {
        eglWindowSurface *winSurf = (eglWindowSurface *)surface;
        eglConfig cfg;
        int bytesPerPixel;
        int displayMode;

        if (winSurf->backBufferBitmap != NULL)
        {
            egliDestroyBackBufferPixmap(winSurf->backBufferBitmap);
            winSurf->backBufferBitmap = NULL;
        }
        surface->colorBuffer = NULL;
        surface->colorBufferStride = 0;

        if (surface->width == 0 || surface->height == 0)
            return 1;

        eglFillConfig(&cfg, surface->configID);
        bytesPerPixel = egliNextPowerOfTwo((cfg.colorBits - 1) >> 3);
        displayMode = (bytesPerPixel == 4)
                    ? ((cfg.alphaBits > 0) ? EGL_SYMBIAN_ECOLOR16MA : EGL_SYMBIAN_ECOLOR16MU)
                    : ((cfg.redBits == 4) ? EGL_SYMBIAN_ECOLOR4K : EGL_SYMBIAN_ECOLOR64K);

        winSurf->backBufferBitmap = egliCreateBackBufferPixmap(surface->width, surface->height,
                                                               displayMode, bytesPerPixel);
        if (winSurf->backBufferBitmap == NULL)
            return 0;

        surface->colorBuffer = winSurf->backBufferBitmap->data;
        surface->colorBufferStride = winSurf->backBufferBitmap->stride;
    }
    else if (surface->type == EGL_SURFACE_PIXMAP)
    {
        eglPixmapSurface *pixSurf = (eglPixmapSurface *)surface;
        if (pixSurf->nativePixmap != NULL)
        {
            surface->colorBuffer = pixSurf->nativePixmap->data;
            surface->colorBufferStride = pixSurf->nativePixmap->stride;
        }
    }

    return 1;
}

void egliSwapWindowSurface(eglWindowSurface *winSurface)
{
    eglNativeWindow_s *win;
    eglNativePixmap_s *back;

    if (winSurface == NULL || winSurface->nativeWindow == NULL)
        return;

    win = winSurface->nativeWindow;
    back = winSurface->backBufferBitmap;
    ++win->swapCount;

    if (win->frontBuffer != NULL && back != NULL && back->data != NULL &&
        winSurface->surface.width > 0 && winSurface->surface.height > 0)
    {
        eglConfig cfg;
        int bpp;
        int dstStride;

        eglFillConfig(&cfg, winSurface->surface.configID);
        bpp = egliNextPowerOfTwo((cfg.colorBits - 1) >> 3);
        dstStride = (win->stride > 0) ? win->stride : (winSurface->surface.width * bpp);
        eglBlit(win->frontBuffer, dstStride,
                back->data, back->stride,
                winSurface->surface.width, winSurface->surface.height,
                bpp, bpp);
    }
}

int egliBlitToPixmap(eglSurface_s *surface, eglNativePixmap_s *target)
{
    eglConfig cfg;
    int bpp;
    int dstWidth = 0;
    int dstHeight = 0;
    int w, h;
    int lockedDraw;

    EGL_ASSERT_AT("eglSymbian.inl", 1438, "surface && egliIsValidNativePixmap(target)",
                  surface != NULL && egliIsValidNativePixmap(target) != 0);

    if (egliConfigPixmapMatch(surface->configID, target) == 0)
        return 0;

    lockedDraw = grLockSurface(GR_SURFACE_DRAW);
    if (lockedDraw == 0)
        ++target->locked;

    eglFillConfig(&cfg, surface->configID);
    bpp = egliNextPowerOfTwo((cfg.colorBits - 1) >> 3);

    if (surface->colorBuffer != NULL && target->data != NULL)
    {
        egliGetNativePixmapSize(target, &dstWidth, &dstHeight);
        w = hgMin32(surface->width, dstWidth);
        h = hgMin32(surface->height, dstHeight);
        if (w > 0 && h > 0)
        {
            eglBlit(target->data, target->stride,
                    surface->colorBuffer, surface->colorBufferStride,
                    w, h, bpp, bpp);
        }
    }

    if (lockedDraw == 0 && target->locked > 0)
        --target->locked;
    grReleaseSurface(GR_SURFACE_DRAW);
    return 1;
}

/* --- Surface Locking & Render Target Refresh ------------------------------- */

eglNativePixmap_s *eglGetActiveBitmap(int op)
{
    eglState_s *state = egliGetState();
    eglSurface_s *surf;

    if (state == NULL)
        return NULL;

    surf = (op == GR_SURFACE_DRAW) ? state->currentDrawSurface : state->currentReadSurface;
    if (surf == NULL)
        return NULL;

    if (surf->type == EGL_SURFACE_WINDOW)
        return ((eglWindowSurface *)surf)->backBufferBitmap;
    if (surf->type == EGL_SURFACE_PIXMAP)
        return ((eglPixmapSurface *)surf)->nativePixmap;
    return NULL;
}

int egliCheckSurfaceSize(eglSurface_s *surface)
{
    int width = 0;
    int height = 0;

    EGL_ASSERT_AT("eglSymbian.inl", 442, "surface", surface != NULL);
    switch (surface->type)
    {
    case EGL_SURFACE_WINDOW:
        egliGetNativeWindowSize((eglWindowSurface *)surface, &width, &height);
        break;
    case EGL_SURFACE_PBUFFER:
        width = surface->width;
        height = surface->height;
        break;
    case EGL_SURFACE_PIXMAP:
        egliGetNativePixmapSize(((eglPixmapSurface *)surface)->nativePixmap, &width, &height);
        break;
    default:
        break;
    }

    if (width != surface->width || height != surface->height)
    {
        eglState_s *state = egliGetState();
        if (state != NULL && state->currentESContext != NULL)
        {
            eglUpdateSurfaces(state->currentESContext,
                              state->currentDrawSurface,
                              state->currentReadSurface);
            return 1;
        }
    }
    return 0;
}

int egliUpdateSurfaceRenderTarget(eglSurface_s *surface)
{
    void *newBuf = surface->colorBuffer;
    int newStride = surface->colorBufferStride;

    if (surface->type == EGL_SURFACE_WINDOW)
    {
        eglWindowSurface *winSurf = (eglWindowSurface *)surface;
        if (winSurf->backBufferBitmap != NULL)
        {
            newBuf = winSurf->backBufferBitmap->data;
            newStride = winSurf->backBufferBitmap->stride;
        }
    }
    else if (surface->type == EGL_SURFACE_PIXMAP)
    {
        eglPixmapSurface *pixSurf = (eglPixmapSurface *)surface;
        if (pixSurf->nativePixmap != NULL)
        {
            newBuf = pixSurf->nativePixmap->data;
            newStride = pixSurf->nativePixmap->stride;
        }
    }

    if (newBuf != surface->colorBuffer || newStride != surface->colorBufferStride)
    {
        eglState_s *state = egliGetState();
        surface->colorBuffer = newBuf;
        surface->colorBufferStride = newStride;
        if (state != NULL && state->currentESContext != NULL)
        {
            eglUpdateSurfaces(state->currentESContext,
                              state->currentDrawSurface,
                              state->currentReadSurface);
            return 1;
        }
    }
    return 0;
}

int grLockSurface(int op)
{
    eglState_s *state;
    eglSurface_s *surf;
    eglNativePixmap_s *bmp;

    EGL_ASSERT_AT("eglSymbian.inl", 482, "op == GR_SURFACE_DRAW || op == GR_SURFACE_READ",
                  op == GR_SURFACE_DRAW || op == GR_SURFACE_READ);

    state = egliGetState();
    if (state == NULL || state->currentDrawSurface == NULL || state->currentReadSurface == NULL)
        return 0;

    surf = (op == GR_SURFACE_DRAW) ? state->currentDrawSurface : state->currentReadSurface;
    egliCheckSurfaceSize(surf);
    bmp = eglGetActiveBitmap(op);
    if (bmp != NULL)
    {
        ++bmp->locked;
        egliUpdateSurfaceRenderTarget(surf);
        return 1;
    }
    return 0;
}

int grReleaseSurface(int op)
{
    eglNativePixmap_s *bmp;

    EGL_ASSERT_AT("eglSymbian.inl", 518, "op == GR_SURFACE_DRAW || op == GR_SURFACE_READ",
                  op == GR_SURFACE_DRAW || op == GR_SURFACE_READ);

    bmp = eglGetActiveBitmap(op);
    if (bmp != NULL)
    {
        if (bmp->locked > 0)
            --bmp->locked;
        return 1;
    }
    return 0;
}

/* --- Symbian Config & Pixel Offset Fixups ---------------------------------- */

void egliFixConfig(eglConfig *config)
{
    eglState_s *state = egliGetState();

    EGL_ASSERT_AT("eglSymbian.inl", 1554, "config", config != NULL);

    if (config->alphaBits == 4)
    {
        config->colorBits -= 4;
        config->alphaBits = 0;
    }

    if ((config->surfaceType & EGL_WINDOW_BIT) != 0 && state != NULL)
    {
        int mode = state->displayMode;
        if (mode == EGL_SYMBIAN_ECOLOR16MA || mode == EGL_SYMBIAN_ECOLOR16MU)
        {
            if (config->colorBits < 24)
                config->configCaveat = EGL_SLOW_CONFIG;
        }
        else if (mode == EGL_SYMBIAN_ECOLOR64K)
        {
            if (config->colorBits != 16)
                config->configCaveat = EGL_SLOW_CONFIG;
        }
        else if (mode == EGL_SYMBIAN_ECOLOR4K)
        {
            if (config->colorBits != 12)
                config->configCaveat = EGL_SLOW_CONFIG;
        }
    }
}

void egliFixPixelOffsets(oglSurfaceDescriptor_s *desc, const eglSurface_s *surface)
{
    /* On Symbian EColor4K / EColor64K / EColor16MU / EColor16MA, B is at bit 0,
     * G at blueBits, R at blueBits+greenBits, A at blueBits+greenBits+redBits -
     * identical to eglSetSurfaceDescriptor's default layout. */
    (void)desc;
    (void)surface;
}

/* --- EGL Error State (including pre-init TLS sentinel tags) ---------------- */

void eglSetError(int error)
{
    eglState_s *state = egliGetState();

    EGL_ASSERT_AT("eglSymbian.inl", 1609, "error >= EGL_SUCCESS && error <= EGL_CONTEXT_LOST",
                  error >= EGL_SUCCESS && error <= EGL_CONTEXT_LOST);

    if (state != NULL)
    {
        state->errorCode = error;
    }
    else
    {
        uintptr_t tag = 0u;
        if (error == EGL_BAD_DISPLAY)
            tag = 1u;
        else if (error == EGL_NOT_INITIALIZED)
            tag = 2u;
        else
            EGL_ASSERT_AT("eglSymbian.inl", 1626, "error == EGL_SUCCESS", error == EGL_SUCCESS);
        egliSetGlobal((void *)tag);
    }
}

EGLAPI EGLint EGLAPIENTRY eglGetError(void)
{
    eglState_s *state = egliGetState();

    if (state != NULL)
    {
        int err = state->errorCode;
        state->errorCode = EGL_SUCCESS;
        return err;
    }
    else
    {
        uintptr_t tag = (uintptr_t)egliGetGlobal();
        int err = EGL_SUCCESS;

        if (tag == 1u)
            err = EGL_BAD_DISPLAY;
        else if (tag == 2u)
            err = EGL_NOT_INITIALIZED;
        egliSetGlobal(NULL);
        return err;
    }
}

/* --- Config Sorting (TEGLCachedConfig insertion sort, matching RArray) ----- */

int egliCompareConfigs(const TEGLCachedConfig *config1, const TEGLCachedConfig *config2)
{
    return eglCompareConfigs(&config1->iConfig, &config2->iConfig, config1->iFilter);
}

void egliSortConfigs(EGLConfig *configs, int configCount, const eglConfig *filter)
{
    TEGLCachedConfig cached[EGL_NUM_BUFFER_FORMATS];
    int i, j;

    if (configs == NULL || configCount <= 1)
        return;

    if (configCount > EGL_NUM_BUFFER_FORMATS)
        configCount = EGL_NUM_BUFFER_FORMATS;

    for (i = 0; i < configCount; ++i)
    {
        TEGLCachedConfig item;
        item.iFilter = filter;
        eglFillConfig(&item.iConfig, configs[i]);

        j = i;
        while (j > 0 && egliCompareConfigs(&cached[j - 1], &item) > 0)
        {
            cached[j] = cached[j - 1];
            --j;
        }
        cached[j] = item;
    }

    for (i = 0; i < configCount; ++i)
        configs[i] = cached[i].iConfig.configID;
}
