/* egl_symbian.cpp - Native Symbian OS v9.x (EKA2) EGL 1.1 implementation for
 * Android libpixelflinger + codeflinger + libagl (zero WSD, 175-ordinal ABI).
 *
 * Supports:
 *   - Symbian RWindow + CWsBitmap + CWindowGc::BitBlt window surfaces (with
 *     dynamic S60 portrait/landscape resize & FBS heap lock/unlock safety)
 *   - Symbian CFbsBitmap pixmap surfaces + eglCopyBuffers color conversion
 *   - Pbuffer offscreen surfaces + zero-copy eglBindTexImage/eglReleaseTexImage
 *   - Portable PF_HEADLESS_UNIX mode for automated Linux render verification
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES/gl.h>
#include <GLES/glext.h>

#ifdef __SYMBIAN32__
#include "symbian_os.h"
void  pf_symbian_set_egl_state(void* state, void (*destroyFn)(void*));
void* pf_symbian_get_egl_state(void);
#endif

#include "../libagl/context.h"
#include "../libagl/state.h"
#include "../libagl/matrix.h"
#include "../libagl/texture.h"
#include "../libagl/TextureObjectManager.h"

using namespace android;
using namespace android::gl;

/* Symbian TDisplayMode constants */
#define SYM_DISP_ECOLOR4K     7
#define SYM_DISP_ECOLOR64K    8
#define SYM_DISP_ECOLOR16M    9
#define SYM_DISP_ECOLOR16MU  11
#define SYM_DISP_ECOLOR16MA  12
#define SYM_DISP_ECOLOR16MAP 13

#define PF_DEFAULT_DISPLAY ((EGLDisplay)1)

struct PfConfigDesc {
    EGLint      configId;
    EGLint      bufferSize;
    EGLint      redSize;
    EGLint      greenSize;
    EGLint      blueSize;
    EGLint      alphaSize;
    EGLint      depthSize;
    uint8_t     gglFormat;
    uint8_t     gglDepthFormat;
    uint8_t     bytesPerPixel;
    uint8_t     symDisplayMode;
};

static const PfConfigDesc kConfigs[] = {
    /* Config 1: RGB565 (EColor64K) + 16-bit Z-buffer */
    { 1, 16, 5, 6, 5, 0, 16, GGL_PIXEL_FORMAT_RGB_565,   GGL_PIXEL_FORMAT_Z_16, 2, SYM_DISP_ECOLOR64K  },
    /* Config 2: RGB565 (EColor64K), no Z-buffer */
    { 2, 16, 5, 6, 5, 0,  0, GGL_PIXEL_FORMAT_RGB_565,   GGL_PIXEL_FORMAT_NONE, 2, SYM_DISP_ECOLOR64K  },
    /* Config 3: BGRA8888 (EColor16MA/16MU) + 16-bit Z-buffer */
    { 3, 32, 8, 8, 8, 8, 16, GGL_PIXEL_FORMAT_BGRA_8888, GGL_PIXEL_FORMAT_Z_16, 4, SYM_DISP_ECOLOR16MA },
    /* Config 4: BGRX8888 (EColor16MU) + 16-bit Z-buffer */
    { 4, 32, 8, 8, 8, 0, 16, GGL_PIXEL_FORMAT_BGRA_8888, GGL_PIXEL_FORMAT_Z_16, 4, SYM_DISP_ECOLOR16MU },
    /* Config 5: BGRA8888 (EColor16MA), no Z-buffer */
    { 5, 32, 8, 8, 8, 8,  0, GGL_PIXEL_FORMAT_BGRA_8888, GGL_PIXEL_FORMAT_NONE, 4, SYM_DISP_ECOLOR16MA },
    /* Config 6: RGBA4444 (EColor4K) + 16-bit Z-buffer */
    { 6, 16, 4, 4, 4, 4, 16, GGL_PIXEL_FORMAT_RGBA_4444, GGL_PIXEL_FORMAT_Z_16, 2, SYM_DISP_ECOLOR4K   },
};

static const int kNumConfigs = (int)(sizeof(kConfigs) / sizeof(kConfigs[0]));

static const PfConfigDesc* findConfig(EGLConfig cfg)
{
    intptr_t id = (intptr_t)cfg;
    if (id < 1 || id > kNumConfigs)
        return 0;
    return &kConfigs[id - 1];
}

enum PfSurfaceKind {
    PF_SURF_WINDOW  = 1,
    PF_SURF_PBUFFER = 2,
    PF_SURF_PIXMAP  = 3
};

struct PfEglContextHdr {
    uint32_t        magic;
    uint32_t        neverCurrent;
    uint32_t        isCurrent;
    uint32_t        deletePending;
    EGLDisplay      dpy;
    EGLConfig       config;
    struct PfSurface* draw;
    struct PfSurface* read;
    PfEglContextHdr*  next;
};

#define PF_CTX_MAGIC  0x50464358u
#define PF_SURF_MAGIC 0x50465346u

struct PfSurface {
    uint32_t            magic;
    PfSurfaceKind       kind;
    EGLDisplay          dpy;
    const PfConfigDesc* cfg;
    EGLContext          boundCtx;
    int                 deletePending;

    int32_t             width;
    int32_t             height;
    int32_t             stridePixels;

    GGLSurface          color;
    GGLSurface          depth;
    int                 ownsColorMem;

    /* EGL 1.1 Pbuffer render-to-texture state */
    EGLint              texFormat;
    EGLint              texTarget;
    EGLBoolean          mipmapTex;
    EGLint              mipmapLevel;
    EGLTextureObject*   boundTexObj;

#ifdef __SYMBIAN32__
    void*               nativeWindow;
    int                 ownWindow;
    void*               windowGc;
    void*               wsBitmap;
    void*               nativePixmap;
    int                 fbsLocked;
#else
    void*               hostWindowBuf;
#endif

    PfSurface*          next;
};

struct PfEglState {
    EGLint              lastError;
    int                 initialized;
    int                 ownCoeEnv;
    void*               createdCoeEnv;
    EGLContext          currentCtx;
    PfSurface*          currentDraw;
    PfSurface*          currentRead;
    PfSurface*          surfaces;
    PfEglContextHdr*    contexts;
};

#ifndef __SYMBIAN32__
static __thread ogles_context_t* gHostGlContext = 0;
static __thread PfEglState*      gHostEglState  = 0;

namespace android {
namespace gl {
void setGlThreadSpecific(ogles_context_t* value) { gHostGlContext = value; }
ogles_context_t* getGlThreadSpecific() { return gHostGlContext; }
}
}
#endif

static void pfDestroyEglState(void* rawState);

static PfEglState* getEglState(bool createIfMissing = true)
{
#ifdef __SYMBIAN32__
    PfEglState* st = (PfEglState*)pf_symbian_get_egl_state();
    if (!st && createIfMissing) {
        st = (PfEglState*)calloc(1, sizeof(PfEglState));
        if (st) {
            st->lastError = EGL_SUCCESS;
            pf_symbian_set_egl_state(st, pfDestroyEglState);
        }
    }
    return st;
#else
    if (!gHostEglState && createIfMissing) {
        gHostEglState = (PfEglState*)calloc(1, sizeof(PfEglState));
        if (gHostEglState)
            gHostEglState->lastError = EGL_SUCCESS;
    }
    return gHostEglState;
#endif
}

template <typename T>
static inline T pfSetError(EGLint err, T ret)
{
    PfEglState* st = getEglState(true);
    if (st)
        st->lastError = err;
    return ret;
}

static inline PfEglContextHdr* getCtxHdr(EGLContext ctx)
{
    if (!ctx) return 0;
    ogles_context_t* gl = (ogles_context_t*)ctx;
    PfEglContextHdr* hdr = (PfEglContextHdr*)gl->rasterizer.base;
    if (!hdr || hdr->magic != PF_CTX_MAGIC)
        return 0;
    return hdr;
}

static inline bool isValidSurface(EGLSurface surf)
{
    if (!surf) return false;
    PfSurface* s = (PfSurface*)surf;
    return s->magic == PF_SURF_MAGIC;
}

static bool allocDepthIfNeeded(PfSurface* s)
{
    if (s->cfg->depthSize <= 0) {
        memset(&s->depth, 0, sizeof(GGLSurface));
        s->depth.version = sizeof(GGLSurface);
        return true;
    }
    int32_t w = s->width > 0 ? s->width : 1;
    int32_t h = s->height > 0 ? s->height : 1;
    int32_t stride = (w + 1) & ~1;
    if (s->depth.data && (int32_t)s->depth.width == w && (int32_t)s->depth.height == h)
        return true;

    free(s->depth.data);
    s->depth.version = sizeof(GGLSurface);
    s->depth.width   = (GGLuint)w;
    s->depth.height  = (GGLuint)h;
    s->depth.stride  = stride;
    s->depth.format  = GGL_PIXEL_FORMAT_Z_16;
    s->depth.compressedFormat = 0;
    s->depth.data    = (GGLubyte*)malloc((size_t)stride * (size_t)h * 2u);
    return s->depth.data != 0;
}

static bool connectSurface(PfSurface* s)
{
    if (!s) return false;
#ifdef __SYMBIAN32__
    void* bmp = (s->kind == PF_SURF_WINDOW) ? s->wsBitmap :
                (s->kind == PF_SURF_PIXMAP) ? s->nativePixmap : 0;
    if (bmp) {
        SymTSize sz = { 0, 0 };
        sym_CFbsBitmap_SizeInPixels(&sz, bmp);
        if (sz.iWidth <= 0)  sz.iWidth = 1;
        if (sz.iHeight <= 0) sz.iHeight = 1;
        int dispMode = sym_CFbsBitmap_DisplayMode(bmp);
        int byteStride = sym_CFbsBitmap_ScanLineLength(sz.iWidth, dispMode);
        int bpp = s->cfg->bytesPerPixel;
        if (!s->fbsLocked) {
            sym_CFbsBitmap_LockHeap(bmp, 0);
            s->fbsLocked = 1;
        }
        void* addr = sym_CFbsBitmap_DataAddress(bmp);
        s->width        = sz.iWidth;
        s->height       = sz.iHeight;
        s->stridePixels = (byteStride > 0 && bpp > 0) ? (byteStride / bpp) : sz.iWidth;
        s->color.version = sizeof(GGLSurface);
        s->color.width   = (GGLuint)s->width;
        s->color.height  = (GGLuint)s->height;
        s->color.stride  = s->stridePixels;
        s->color.format  = s->cfg->gglFormat;
        s->color.compressedFormat = 0;
        s->color.data    = (GGLubyte*)addr;
    }
#endif
    return allocDepthIfNeeded(s);
}

static void disconnectSurface(PfSurface* s)
{
    if (!s) return;
#ifdef __SYMBIAN32__
    if (s->fbsLocked) {
        void* bmp = (s->kind == PF_SURF_WINDOW) ? s->wsBitmap :
                    (s->kind == PF_SURF_PIXMAP) ? s->nativePixmap : 0;
        if (bmp)
            sym_CFbsBitmap_UnlockHeap(bmp, 0);
        s->fbsLocked = 0;
    }
#endif
}

static void bindDrawToGl(PfSurface* s, ogles_context_t* gl)
{
    if (!s || !gl) return;
    gl->rasterizer.procs.colorBuffer(gl, &s->color);
    gl->rasterizer.procs.depthBuffer(gl, s->depth.data ? &s->depth : 0);
}

static void bindReadToGl(PfSurface* s, ogles_context_t* gl)
{
    if (!s || !gl) return;
    gl->rasterizer.procs.readBuffer(gl, &s->color);
}

static void freeSurfaceImmediate(PfSurface* s)
{
    if (!s) return;
    disconnectSurface(s);
    if (s->boundTexObj) {
        s->boundTexObj->decStrong(s);
        s->boundTexObj = 0;
    }
#ifdef __SYMBIAN32__
    if (s->ownWindow && s->nativeWindow) {
        sym_RWindowTreeNode_Close(s->nativeWindow);
        sym_User_Free(s->nativeWindow);
        s->nativeWindow = 0;
    }
    if (s->wsBitmap) {
        sym_CFbsBitmap_Reset(s->wsBitmap);
        sym_CBase_Delete(s->wsBitmap);
        s->wsBitmap = 0;
    }
    if (s->windowGc) {
        sym_CBase_Delete(s->windowGc);
        s->windowGc = 0;
    }
#endif
    if (s->ownsColorMem && s->color.data) {
        free(s->color.data);
        s->color.data = 0;
    }
    if (s->depth.data) {
        free(s->depth.data);
        s->depth.data = 0;
    }
    s->magic = 0;
    free(s);
}

static void unlinkAndFreeSurface(PfEglState* st, PfSurface* s)
{
    if (st) {
        PfSurface** pp = &st->surfaces;
        while (*pp) {
            if (*pp == s) {
                *pp = s->next;
                break;
            }
            pp = &(*pp)->next;
        }
    }
    freeSurfaceImmediate(s);
}

static void freeContextImmediate(EGLContext ctx)
{
    PfEglContextHdr* hdr = getCtxHdr(ctx);
    if (!hdr) return;
    hdr->magic = 0;
    ogles_uninit((ogles_context_t*)ctx);
}

static void unlinkAndFreeContext(PfEglState* st, EGLContext ctx)
{
    PfEglContextHdr* hdr = getCtxHdr(ctx);
    if (!hdr) return;
    if (st) {
        PfEglContextHdr** pp = &st->contexts;
        while (*pp) {
            if (*pp == hdr) {
                *pp = hdr->next;
                break;
            }
            pp = &(*pp)->next;
        }
    }
    freeContextImmediate(ctx);
}

static void pfDestroyEglState(void* rawState)
{
    PfEglState* st = (PfEglState*)rawState;
    if (!st) return;

    setGlThreadSpecific(0);
    st->currentCtx = EGL_NO_CONTEXT;
    st->currentDraw = 0;
    st->currentRead = 0;

    while (st->contexts) {
        PfEglContextHdr* hdr = st->contexts;
        st->contexts = hdr->next;
        ogles_context_t* gl = (ogles_context_t*)(((uintptr_t)hdr + sizeof(PfEglContextHdr) + 31u) & ~(uintptr_t)31u);
        hdr->magic = 0;
        ogles_uninit(gl);
    }

    while (st->surfaces) {
        PfSurface* s = st->surfaces;
        st->surfaces = s->next;
        freeSurfaceImmediate(s);
    }

#ifdef __SYMBIAN32__
    if (st->ownCoeEnv && st->createdCoeEnv) {
        sym_CCoeEnv_DestroyEnvironment(st->createdCoeEnv);
        st->createdCoeEnv = 0;
        st->ownCoeEnv = 0;
    }
#endif
    free(st);
}

/* ==========================================================================
 * 29 Exported EGL 1.1 Functions (matching symbian/libgles_cm.def)
 * ========================================================================== */

extern "C" {

EGLAPI EGLint EGLAPIENTRY eglGetError(void)
{
    PfEglState* st = getEglState(false);
    if (!st)
        return EGL_SUCCESS;
    EGLint err = st->lastError;
    st->lastError = EGL_SUCCESS;
    return err;
}

EGLAPI EGLDisplay EGLAPIENTRY eglGetDisplay(EGLNativeDisplayType display_id)
{
    if (display_id != EGL_DEFAULT_DISPLAY)
        return EGL_NO_DISPLAY;
    (void)getEglState(true);
    return PF_DEFAULT_DISPLAY;
}

EGLAPI EGLBoolean EGLAPIENTRY eglInitialize(EGLDisplay dpy, EGLint *major, EGLint *minor)
{
    if (dpy != PF_DEFAULT_DISPLAY)
        return pfSetError(EGL_BAD_DISPLAY, EGL_FALSE);

    PfEglState* st = getEglState(true);
    if (!st)
        return pfSetError(EGL_BAD_ALLOC, EGL_FALSE);

#ifdef __SYMBIAN32__
    if (!st->initialized) {
        if (sym_CCoeEnv_Static() == 0) {
            void* env = sym_User_AllocZL(SYM_CCOEENV_SIZE);
            if (env) {
                env = sym_CCoeEnv_Ctor(env);
                void* mark = sym_User_MarkCleanupStack();
                sym_CCoeEnv_ConstructL(env);
                sym_User_UnMarkCleanupStack(mark);
                st->ownCoeEnv = 1;
                st->createdCoeEnv = env;
            }
        }
    }
#endif
    st->initialized = 1;
    if (major) *major = 1;
    if (minor) *minor = 1;
    return pfSetError(EGL_SUCCESS, EGL_TRUE);
}

EGLAPI EGLBoolean EGLAPIENTRY eglTerminate(EGLDisplay dpy)
{
    if (dpy != PF_DEFAULT_DISPLAY)
        return pfSetError(EGL_BAD_DISPLAY, EGL_FALSE);

    PfEglState* st = getEglState(false);
    if (!st)
        return EGL_TRUE;

#ifdef __SYMBIAN32__
    pf_symbian_set_egl_state(0, 0);
#else
    gHostEglState = 0;
#endif
    pfDestroyEglState(st);
    return EGL_TRUE;
}

EGLAPI const char* EGLAPIENTRY eglQueryString(EGLDisplay dpy, EGLint name)
{
    if (dpy != PF_DEFAULT_DISPLAY)
        return pfSetError<const char*>(EGL_BAD_DISPLAY, 0);
    PfEglState* st = getEglState(false);
    if (!st || !st->initialized)
        return pfSetError<const char*>(EGL_NOT_INITIALIZED, 0);

    switch (name) {
    case EGL_VENDOR:      return pfSetError<const char*>(EGL_SUCCESS, "klaxons");
    case EGL_VERSION:     return pfSetError<const char*>(EGL_SUCCESS, "1.1");
    case EGL_EXTENSIONS:  return pfSetError<const char*>(EGL_SUCCESS, "");
    default:              return pfSetError<const char*>(EGL_BAD_PARAMETER, 0);
    }
}

EGLAPI EGLBoolean EGLAPIENTRY eglGetConfigs(EGLDisplay dpy, EGLConfig *configs,
                                            EGLint config_size, EGLint *num_config)
{
    if (dpy != PF_DEFAULT_DISPLAY)
        return pfSetError(EGL_BAD_DISPLAY, EGL_FALSE);
    PfEglState* st = getEglState(false);
    if (!st || !st->initialized)
        return pfSetError(EGL_NOT_INITIALIZED, EGL_FALSE);
    if (!num_config)
        return pfSetError(EGL_BAD_PARAMETER, EGL_FALSE);

    if (!configs) {
        *num_config = kNumConfigs;
        return pfSetError(EGL_SUCCESS, EGL_TRUE);
    }
    int n = (config_size < kNumConfigs) ? config_size : kNumConfigs;
    if (n < 0) n = 0;
    for (int i = 0; i < n; i++)
        configs[i] = (EGLConfig)(intptr_t)kConfigs[i].configId;
    *num_config = n;
    return pfSetError(EGL_SUCCESS, EGL_TRUE);
}

static bool getConfigAttribInternal(const PfConfigDesc* c, EGLint attribute, EGLint* value)
{
    switch (attribute) {
    case EGL_CONFIG_ID:             *value = c->configId; return true;
    case EGL_BUFFER_SIZE:           *value = c->bufferSize; return true;
    case EGL_RED_SIZE:              *value = c->redSize; return true;
    case EGL_GREEN_SIZE:            *value = c->greenSize; return true;
    case EGL_BLUE_SIZE:             *value = c->blueSize; return true;
    case EGL_ALPHA_SIZE:            *value = c->alphaSize; return true;
    case EGL_DEPTH_SIZE:            *value = c->depthSize; return true;
    case EGL_STENCIL_SIZE:          *value = 0; return true;
    case EGL_CONFIG_CAVEAT:         *value = EGL_NONE; return true;
    case EGL_LEVEL:                 *value = 0; return true;
    case EGL_MAX_PBUFFER_WIDTH:     *value = 2048; return true;
    case EGL_MAX_PBUFFER_HEIGHT:    *value = 2048; return true;
    case EGL_MAX_PBUFFER_PIXELS:    *value = 2048 * 2048; return true;
    case EGL_NATIVE_RENDERABLE:     *value = EGL_TRUE; return true;
    case EGL_NATIVE_VISUAL_ID:      *value = c->symDisplayMode; return true;
    case EGL_NATIVE_VISUAL_TYPE:    *value = c->symDisplayMode; return true;
    case EGL_SAMPLE_BUFFERS:        *value = 0; return true;
    case EGL_SAMPLES:               *value = 0; return true;
    case EGL_SURFACE_TYPE:          *value = EGL_WINDOW_BIT | EGL_PIXMAP_BIT | EGL_PBUFFER_BIT; return true;
    case EGL_TRANSPARENT_TYPE:      *value = EGL_NONE; return true;
    case EGL_TRANSPARENT_RED_VALUE:
    case EGL_TRANSPARENT_GREEN_VALUE:
    case EGL_TRANSPARENT_BLUE_VALUE:*value = 0; return true;
    case EGL_BIND_TO_TEXTURE_RGB:   *value = (c->alphaSize == 0) ? EGL_TRUE : EGL_FALSE; return true;
    case EGL_BIND_TO_TEXTURE_RGBA:  *value = (c->alphaSize > 0) ? EGL_TRUE : EGL_FALSE; return true;
    case EGL_MIN_SWAP_INTERVAL:     *value = 0; return true;
    case EGL_MAX_SWAP_INTERVAL:     *value = 1; return true;
    default:                        return false;
    }
}

EGLAPI EGLBoolean EGLAPIENTRY eglGetConfigAttrib(EGLDisplay dpy, EGLConfig config,
                                                 EGLint attribute, EGLint *value)
{
    if (dpy != PF_DEFAULT_DISPLAY)
        return pfSetError(EGL_BAD_DISPLAY, EGL_FALSE);
    PfEglState* st = getEglState(false);
    if (!st || !st->initialized)
        return pfSetError(EGL_NOT_INITIALIZED, EGL_FALSE);
    if (!value)
        return pfSetError(EGL_BAD_PARAMETER, EGL_FALSE);
    const PfConfigDesc* c = findConfig(config);
    if (!c)
        return pfSetError(EGL_BAD_CONFIG, EGL_FALSE);
    if (!getConfigAttribInternal(c, attribute, value))
        return pfSetError(EGL_BAD_ATTRIBUTE, EGL_FALSE);
    return pfSetError(EGL_SUCCESS, EGL_TRUE);
}

EGLAPI EGLBoolean EGLAPIENTRY eglChooseConfig(EGLDisplay dpy, const EGLint *attrib_list,
                                              EGLConfig *configs, EGLint config_size,
                                              EGLint *num_config)
{
    if (dpy != PF_DEFAULT_DISPLAY)
        return pfSetError(EGL_BAD_DISPLAY, EGL_FALSE);
    PfEglState* st = getEglState(false);
    if (!st || !st->initialized)
        return pfSetError(EGL_NOT_INITIALIZED, EGL_FALSE);
    if (!num_config)
        return pfSetError(EGL_BAD_PARAMETER, EGL_FALSE);

    EGLint reqId = EGL_DONT_CARE;
    EGLint reqBuf = 0, reqR = 0, reqG = 0, reqB = 0, reqA = 0, reqD = 0, reqS = 0;
    EGLint reqSurf = EGL_WINDOW_BIT;
    EGLint reqBindRgb = EGL_DONT_CARE, reqBindRgba = EGL_DONT_CARE;

    if (attrib_list) {
        for (int i = 0; attrib_list[i] != EGL_NONE; i += 2) {
            EGLint k = attrib_list[i];
            EGLint v = attrib_list[i + 1];
            switch (k) {
            case EGL_CONFIG_ID:           reqId = v; break;
            case EGL_BUFFER_SIZE:         if (v > 0) reqBuf = v; break;
            case EGL_RED_SIZE:            if (v > 0) reqR = v; break;
            case EGL_GREEN_SIZE:          if (v > 0) reqG = v; break;
            case EGL_BLUE_SIZE:           if (v > 0) reqB = v; break;
            case EGL_ALPHA_SIZE:          if (v > 0) reqA = v; break;
            case EGL_DEPTH_SIZE:          if (v > 0) reqD = v; break;
            case EGL_STENCIL_SIZE:        if (v > 0) reqS = v; break;
            case EGL_SURFACE_TYPE:        if (v != EGL_DONT_CARE) reqSurf = v; break;
            case EGL_BIND_TO_TEXTURE_RGB: reqBindRgb = v; break;
            case EGL_BIND_TO_TEXTURE_RGBA:reqBindRgba = v; break;
            case EGL_CONFIG_CAVEAT:
            case EGL_NATIVE_RENDERABLE:
            case EGL_LEVEL:
            case EGL_SAMPLE_BUFFERS:
            case EGL_SAMPLES:
            case EGL_TRANSPARENT_TYPE:
            case EGL_MAX_SWAP_INTERVAL:
            case EGL_MIN_SWAP_INTERVAL:
                break;
            default:
                return pfSetError(EGL_BAD_ATTRIBUTE, EGL_FALSE);
            }
        }
    }

    int matched = 0;
    for (int i = 0; i < kNumConfigs; i++) {
        const PfConfigDesc& c = kConfigs[i];
        if (reqId != EGL_DONT_CARE) {
            if (c.configId != reqId) continue;
        } else {
            if (c.bufferSize < reqBuf) continue;
            if (c.redSize    < reqR)   continue;
            if (c.greenSize  < reqG)   continue;
            if (c.blueSize   < reqB)   continue;
            if (c.alphaSize  < reqA)   continue;
            if (c.depthSize  < reqD)   continue;
            if (0            < reqS)   continue;
            EGLint stMask = EGL_WINDOW_BIT | EGL_PIXMAP_BIT | EGL_PBUFFER_BIT;
            if ((stMask & reqSurf) != reqSurf) continue;
            if (reqBindRgb == EGL_TRUE && c.alphaSize != 0) continue;
            if (reqBindRgba == EGL_TRUE && c.alphaSize == 0) continue;
        }
        if (configs && matched < config_size)
            configs[matched] = (EGLConfig)(intptr_t)c.configId;
        if (!configs || matched < config_size)
            ++matched;
    }

    *num_config = matched;
    return pfSetError(EGL_SUCCESS, EGL_TRUE);
}

EGLAPI EGLSurface EGLAPIENTRY eglCreateWindowSurface(EGLDisplay dpy, EGLConfig config,
                                                     EGLNativeWindowType win,
                                                     const EGLint *attrib_list)
{
    (void)attrib_list;
    if (dpy != PF_DEFAULT_DISPLAY)
        return pfSetError(EGL_BAD_DISPLAY, EGL_NO_SURFACE);
    PfEglState* st = getEglState(false);
    if (!st || !st->initialized)
        return pfSetError(EGL_NOT_INITIALIZED, EGL_NO_SURFACE);
    const PfConfigDesc* cfg = findConfig(config);
    if (!cfg)
        return pfSetError(EGL_BAD_CONFIG, EGL_NO_SURFACE);

    PfSurface* s = (PfSurface*)calloc(1, sizeof(PfSurface));
    if (!s)
        return pfSetError(EGL_BAD_ALLOC, EGL_NO_SURFACE);

    s->magic     = PF_SURF_MAGIC;
    s->kind      = PF_SURF_WINDOW;
    s->dpy       = dpy;
    s->cfg       = cfg;
    s->texFormat = EGL_NO_TEXTURE;
    s->texTarget = EGL_NO_TEXTURE;

#ifdef __SYMBIAN32__
    void* coe = sym_CCoeEnv_Static();
    if (!coe) {
        free(s);
        return pfSetError(EGL_BAD_NATIVE_WINDOW, EGL_NO_SURFACE);
    }

    if (win != 0) {
        s->nativeWindow = win;
        s->ownWindow = 0;
    } else {
        void* newWin = sym_User_Alloc(SYM_RWINDOW_SIZE);
        if (!newWin) {
            free(s);
            return pfSetError(EGL_BAD_ALLOC, EGL_NO_SURFACE);
        }
        newWin = sym_RWindow_Ctor(newWin, SYM_CCOEENV_WSSESSION(coe));
        s->nativeWindow = newWin;
        s->ownWindow = 1;
        if (sym_RWindow_Construct(newWin, SYM_CCOEENV_ROOTWIN(coe),
                                  (unsigned int)(uintptr_t)s) != 0) {
            sym_User_Free(newWin);
            free(s);
            return pfSetError(EGL_BAD_NATIVE_WINDOW, EGL_NO_SURFACE);
        }
    }

    void* gc = sym_User_AllocZ(SYM_CWINDOWGC_SIZE);
    if (gc) gc = sym_CWindowGc_Ctor(gc, SYM_CCOEENV_SCREENDEV(coe));

    void* bmp = sym_User_AllocZ(SYM_CWSBITMAP_SIZE);
    if (bmp) bmp = sym_CWsBitmap_Ctor(bmp, SYM_CCOEENV_WSSESSION(coe));

    SymTSize sz = { 0, 0 };
    sym_RWindowBase_Size(&sz, s->nativeWindow);
    if (sz.iWidth <= 0)  sz.iWidth = 240;
    if (sz.iHeight <= 0) sz.iHeight = 320;

    if (!gc || sym_CWindowGc_Construct(gc) != 0 ||
        !bmp || sym_CWsBitmap_Create(bmp, &sz, cfg->symDisplayMode) != 0) {
        if (bmp) { sym_CWsBitmap_Reset(bmp); sym_CBase_Delete(bmp); }
        if (gc)  { sym_CBase_Delete(gc); }
        if (s->ownWindow && s->nativeWindow) {
            sym_RWindowTreeNode_Close(s->nativeWindow);
            sym_User_Free(s->nativeWindow);
        }
        free(s);
        return pfSetError(EGL_BAD_ALLOC, EGL_NO_SURFACE);
    }

    s->windowGc = gc;
    s->wsBitmap = bmp;
    s->width    = sz.iWidth;
    s->height   = sz.iHeight;
#else
    /* Host headless window simulation: win may optionally point to int[2] {w,h} */
    int w = 240, h = 320;
    if (win) {
        const int* dims = (const int*)win;
        if (dims[0] > 0 && dims[0] <= 4096 && dims[1] > 0 && dims[1] <= 4096) {
            w = dims[0];
            h = dims[1];
        }
    }
    s->width        = w;
    s->height       = h;
    s->stridePixels = (w + 3) & ~3;
    s->ownsColorMem = 1;
    s->color.version = sizeof(GGLSurface);
    s->color.width   = (GGLuint)w;
    s->color.height  = (GGLuint)h;
    s->color.stride  = s->stridePixels;
    s->color.format  = cfg->gglFormat;
    s->color.data    = (GGLubyte*)calloc((size_t)s->stridePixels * (size_t)h, cfg->bytesPerPixel);
    if (!s->color.data) {
        free(s);
        return pfSetError(EGL_BAD_ALLOC, EGL_NO_SURFACE);
    }
#endif

    if (!allocDepthIfNeeded(s)) {
        freeSurfaceImmediate(s);
        return pfSetError(EGL_BAD_ALLOC, EGL_NO_SURFACE);
    }

    s->next = st->surfaces;
    st->surfaces = s;
    return pfSetError(EGL_SUCCESS, (EGLSurface)s);
}

EGLAPI EGLSurface EGLAPIENTRY eglCreatePbufferSurface(EGLDisplay dpy, EGLConfig config,
                                                      const EGLint *attrib_list)
{
    if (dpy != PF_DEFAULT_DISPLAY)
        return pfSetError(EGL_BAD_DISPLAY, EGL_NO_SURFACE);
    PfEglState* st = getEglState(false);
    if (!st || !st->initialized)
        return pfSetError(EGL_NOT_INITIALIZED, EGL_NO_SURFACE);
    const PfConfigDesc* cfg = findConfig(config);
    if (!cfg)
        return pfSetError(EGL_BAD_CONFIG, EGL_NO_SURFACE);

    EGLint w = 0, h = 0;
    EGLint texFmt = EGL_NO_TEXTURE, texTgt = EGL_NO_TEXTURE;
    EGLBoolean mipTex = EGL_FALSE, largest = EGL_FALSE;

    if (attrib_list) {
        for (int i = 0; attrib_list[i] != EGL_NONE; i += 2) {
            EGLint k = attrib_list[i];
            EGLint v = attrib_list[i + 1];
            switch (k) {
            case EGL_WIDTH:           w = v; break;
            case EGL_HEIGHT:          h = v; break;
            case EGL_TEXTURE_FORMAT:  texFmt = v; break;
            case EGL_TEXTURE_TARGET:  texTgt = v; break;
            case EGL_MIPMAP_TEXTURE:  mipTex = v ? EGL_TRUE : EGL_FALSE; break;
            case EGL_LARGEST_PBUFFER: largest = v ? EGL_TRUE : EGL_FALSE; break;
            default:
                return pfSetError(EGL_BAD_ATTRIBUTE, EGL_NO_SURFACE);
            }
        }
    }

    if (w < 0 || h < 0)
        return pfSetError(EGL_BAD_PARAMETER, EGL_NO_SURFACE);
    if ((texFmt == EGL_NO_TEXTURE && texTgt != EGL_NO_TEXTURE) ||
        (texFmt != EGL_NO_TEXTURE && texTgt == EGL_NO_TEXTURE))
        return pfSetError(EGL_BAD_MATCH, EGL_NO_SURFACE);

    if (largest) {
        if (w > 2048) w = 2048;
        if (h > 2048) h = 2048;
    }
    int32_t allocW = w > 0 ? w : 1;
    int32_t allocH = h > 0 ? h : 1;
    int32_t stride = (allocW + 3) & ~3;

    PfSurface* s = (PfSurface*)calloc(1, sizeof(PfSurface));
    if (!s)
        return pfSetError(EGL_BAD_ALLOC, EGL_NO_SURFACE);

    s->magic        = PF_SURF_MAGIC;
    s->kind         = PF_SURF_PBUFFER;
    s->dpy          = dpy;
    s->cfg          = cfg;
    s->width        = w;
    s->height       = h;
    s->stridePixels = stride;
    s->texFormat    = texFmt;
    s->texTarget    = texTgt;
    s->mipmapTex    = mipTex;
    s->ownsColorMem = 1;

    s->color.version = sizeof(GGLSurface);
    s->color.width   = (GGLuint)allocW;
    s->color.height  = (GGLuint)allocH;
    s->color.stride  = stride;
    s->color.format  = cfg->gglFormat;
    s->color.data    = (GGLubyte*)calloc((size_t)stride * (size_t)allocH, cfg->bytesPerPixel);

    if (!s->color.data || !allocDepthIfNeeded(s)) {
        freeSurfaceImmediate(s);
        return pfSetError(EGL_BAD_ALLOC, EGL_NO_SURFACE);
    }

    s->next = st->surfaces;
    st->surfaces = s;
    return pfSetError(EGL_SUCCESS, (EGLSurface)s);
}

EGLAPI EGLSurface EGLAPIENTRY eglCreatePixmapSurface(EGLDisplay dpy, EGLConfig config,
                                                     EGLNativePixmapType pixmap,
                                                     const EGLint *attrib_list)
{
    (void)attrib_list;
    if (dpy != PF_DEFAULT_DISPLAY)
        return pfSetError(EGL_BAD_DISPLAY, EGL_NO_SURFACE);
    PfEglState* st = getEglState(false);
    if (!st || !st->initialized)
        return pfSetError(EGL_NOT_INITIALIZED, EGL_NO_SURFACE);
    const PfConfigDesc* cfg = findConfig(config);
    if (!cfg)
        return pfSetError(EGL_BAD_CONFIG, EGL_NO_SURFACE);
    if (!pixmap)
        return pfSetError(EGL_BAD_NATIVE_PIXMAP, EGL_NO_SURFACE);

#ifdef __SYMBIAN32__
    SymTSize sz = { 0, 0 };
    sym_CFbsBitmap_SizeInPixels(&sz, pixmap);
    if (sz.iWidth <= 0 || sz.iHeight <= 0)
        return pfSetError(EGL_BAD_NATIVE_PIXMAP, EGL_NO_SURFACE);

    PfSurface* s = (PfSurface*)calloc(1, sizeof(PfSurface));
    if (!s)
        return pfSetError(EGL_BAD_ALLOC, EGL_NO_SURFACE);

    s->magic        = PF_SURF_MAGIC;
    s->kind         = PF_SURF_PIXMAP;
    s->dpy          = dpy;
    s->cfg          = cfg;
    s->nativePixmap = pixmap;
    s->width        = sz.iWidth;
    s->height       = sz.iHeight;
    s->texFormat    = EGL_NO_TEXTURE;
    s->texTarget    = EGL_NO_TEXTURE;

    if (!allocDepthIfNeeded(s)) {
        freeSurfaceImmediate(s);
        return pfSetError(EGL_BAD_ALLOC, EGL_NO_SURFACE);
    }

    s->next = st->surfaces;
    st->surfaces = s;
    return pfSetError(EGL_SUCCESS, (EGLSurface)s);
#else
    return pfSetError(EGL_BAD_NATIVE_PIXMAP, EGL_NO_SURFACE);
#endif
}

EGLAPI EGLBoolean EGLAPIENTRY eglDestroySurface(EGLDisplay dpy, EGLSurface surface)
{
    if (dpy != PF_DEFAULT_DISPLAY)
        return pfSetError(EGL_BAD_DISPLAY, EGL_FALSE);
    PfEglState* st = getEglState(false);
    if (!st || !st->initialized)
        return pfSetError(EGL_NOT_INITIALIZED, EGL_FALSE);
    if (!isValidSurface(surface))
        return pfSetError(EGL_BAD_SURFACE, EGL_FALSE);

    PfSurface* s = (PfSurface*)surface;
    if (s->boundCtx != EGL_NO_CONTEXT) {
        s->deletePending = 1;
        return pfSetError(EGL_SUCCESS, EGL_TRUE);
    }
    unlinkAndFreeSurface(st, s);
    return pfSetError(EGL_SUCCESS, EGL_TRUE);
}

EGLAPI EGLBoolean EGLAPIENTRY eglQuerySurface(EGLDisplay dpy, EGLSurface surface,
                                              EGLint attribute, EGLint *value)
{
    if (dpy != PF_DEFAULT_DISPLAY)
        return pfSetError(EGL_BAD_DISPLAY, EGL_FALSE);
    PfEglState* st = getEglState(false);
    if (!st || !st->initialized)
        return pfSetError(EGL_NOT_INITIALIZED, EGL_FALSE);
    if (!isValidSurface(surface))
        return pfSetError(EGL_BAD_SURFACE, EGL_FALSE);
    if (!value)
        return pfSetError(EGL_BAD_PARAMETER, EGL_FALSE);

    PfSurface* s = (PfSurface*)surface;
    switch (attribute) {
    case EGL_CONFIG_ID:         *value = s->cfg->configId; break;
    case EGL_WIDTH:             *value = s->width; break;
    case EGL_HEIGHT:            *value = s->height; break;
    case EGL_LARGEST_PBUFFER:   if (s->kind == PF_SURF_PBUFFER) *value = EGL_FALSE; break;
    case EGL_TEXTURE_FORMAT:    *value = (s->kind == PF_SURF_PBUFFER) ? s->texFormat : EGL_NO_TEXTURE; break;
    case EGL_TEXTURE_TARGET:    *value = (s->kind == PF_SURF_PBUFFER) ? s->texTarget : EGL_NO_TEXTURE; break;
    case EGL_MIPMAP_TEXTURE:    *value = (s->kind == PF_SURF_PBUFFER) ? s->mipmapTex : EGL_FALSE; break;
    case EGL_MIPMAP_LEVEL:      *value = (s->kind == PF_SURF_PBUFFER) ? s->mipmapLevel : 0; break;
    default:                    return pfSetError(EGL_BAD_ATTRIBUTE, EGL_FALSE);
    }
    return pfSetError(EGL_SUCCESS, EGL_TRUE);
}

EGLAPI EGLBoolean EGLAPIENTRY eglSurfaceAttrib(EGLDisplay dpy, EGLSurface surface,
                                               EGLint attribute, EGLint value)
{
    if (dpy != PF_DEFAULT_DISPLAY)
        return pfSetError(EGL_BAD_DISPLAY, EGL_FALSE);
    PfEglState* st = getEglState(false);
    if (!st || !st->initialized)
        return pfSetError(EGL_NOT_INITIALIZED, EGL_FALSE);
    if (!isValidSurface(surface))
        return pfSetError(EGL_BAD_SURFACE, EGL_FALSE);

    PfSurface* s = (PfSurface*)surface;
    if (attribute == EGL_MIPMAP_LEVEL) {
        if (s->kind == PF_SURF_PBUFFER && s->mipmapTex)
            s->mipmapLevel = (value >= 0) ? value : 0;
        return pfSetError(EGL_SUCCESS, EGL_TRUE);
    }
    return pfSetError(EGL_BAD_PARAMETER, EGL_FALSE);
}

EGLAPI EGLContext EGLAPIENTRY eglCreateContext(EGLDisplay dpy, EGLConfig config,
                                               EGLContext share_context,
                                               const EGLint *attrib_list)
{
    (void)share_context;
    if (dpy != PF_DEFAULT_DISPLAY)
        return pfSetError(EGL_BAD_DISPLAY, EGL_NO_CONTEXT);
    PfEglState* st = getEglState(false);
    if (!st || !st->initialized)
        return pfSetError(EGL_NOT_INITIALIZED, EGL_NO_CONTEXT);
    if (!findConfig(config))
        return pfSetError(EGL_BAD_CONFIG, EGL_NO_CONTEXT);
    if (attrib_list && attrib_list[0] != EGL_NONE)
        return pfSetError(EGL_BAD_ATTRIBUTE, EGL_NO_CONTEXT);

    ogles_context_t* gl = ogles_init(sizeof(PfEglContextHdr));
    if (!gl)
        return pfSetError(EGL_BAD_ALLOC, EGL_NO_CONTEXT);

    PfEglContextHdr* hdr = (PfEglContextHdr*)gl->rasterizer.base;
    memset(hdr, 0, sizeof(PfEglContextHdr));
    hdr->magic        = PF_CTX_MAGIC;
    hdr->neverCurrent = 1;
    hdr->dpy          = dpy;
    hdr->config       = config;
    hdr->next         = st->contexts;
    st->contexts      = hdr;

    return pfSetError(EGL_SUCCESS, (EGLContext)gl);
}

EGLAPI EGLBoolean EGLAPIENTRY eglDestroyContext(EGLDisplay dpy, EGLContext ctx)
{
    if (dpy != PF_DEFAULT_DISPLAY)
        return pfSetError(EGL_BAD_DISPLAY, EGL_FALSE);
    PfEglState* st = getEglState(false);
    if (!st || !st->initialized)
        return pfSetError(EGL_NOT_INITIALIZED, EGL_FALSE);
    PfEglContextHdr* hdr = getCtxHdr(ctx);
    if (!hdr)
        return pfSetError(EGL_BAD_CONTEXT, EGL_FALSE);

    if (hdr->isCurrent) {
        hdr->deletePending = 1;
        return pfSetError(EGL_SUCCESS, EGL_TRUE);
    }
    unlinkAndFreeContext(st, ctx);
    return pfSetError(EGL_SUCCESS, EGL_TRUE);
}

EGLAPI EGLBoolean EGLAPIENTRY eglMakeCurrent(EGLDisplay dpy, EGLSurface draw,
                                             EGLSurface read, EGLContext ctx)
{
    if (dpy != PF_DEFAULT_DISPLAY)
        return pfSetError(EGL_BAD_DISPLAY, EGL_FALSE);
    PfEglState* st = getEglState(false);
    if (!st || !st->initialized)
        return pfSetError(EGL_NOT_INITIALIZED, EGL_FALSE);

    if (ctx == EGL_NO_CONTEXT && (draw != EGL_NO_SURFACE || read != EGL_NO_SURFACE))
        return pfSetError(EGL_BAD_MATCH, EGL_FALSE);
    if (ctx != EGL_NO_CONTEXT && (draw == EGL_NO_SURFACE || read == EGL_NO_SURFACE))
        return pfSetError(EGL_BAD_MATCH, EGL_FALSE);

    PfSurface* d = (PfSurface*)draw;
    PfSurface* r = (PfSurface*)read;
    PfEglContextHdr* newHdr = 0;

    if (ctx != EGL_NO_CONTEXT) {
        newHdr = getCtxHdr(ctx);
        if (!newHdr)
            return pfSetError(EGL_BAD_CONTEXT, EGL_FALSE);
        if (!isValidSurface(draw) || !isValidSurface(read))
            return pfSetError(EGL_BAD_SURFACE, EGL_FALSE);
        if ((d->boundCtx && d->boundCtx != ctx) || (r->boundCtx && r->boundCtx != ctx))
            return pfSetError(EGL_BAD_ACCESS, EGL_FALSE);
    }

    /* Detach previous context & surfaces */
    if (st->currentCtx != EGL_NO_CONTEXT && st->currentCtx != ctx) {
        PfEglContextHdr* oldHdr = getCtxHdr(st->currentCtx);
        if (oldHdr) {
            if (oldHdr->draw && oldHdr->draw != d && oldHdr->draw != r) {
                disconnectSurface(oldHdr->draw);
                oldHdr->draw->boundCtx = EGL_NO_CONTEXT;
                if (oldHdr->draw->deletePending)
                    unlinkAndFreeSurface(st, oldHdr->draw);
            }
            if (oldHdr->read && oldHdr->read != oldHdr->draw &&
                oldHdr->read != d && oldHdr->read != r) {
                disconnectSurface(oldHdr->read);
                oldHdr->read->boundCtx = EGL_NO_CONTEXT;
                if (oldHdr->read->deletePending)
                    unlinkAndFreeSurface(st, oldHdr->read);
            }
            oldHdr->draw = 0;
            oldHdr->read = 0;
            oldHdr->isCurrent = 0;
            if (oldHdr->deletePending)
                unlinkAndFreeContext(st, st->currentCtx);
        }
    } else if (ctx == EGL_NO_CONTEXT && st->currentCtx != EGL_NO_CONTEXT) {
        PfEglContextHdr* oldHdr = getCtxHdr(st->currentCtx);
        if (oldHdr) {
            if (oldHdr->draw) {
                PfSurface* oldD = oldHdr->draw;
                disconnectSurface(oldD);
                oldD->boundCtx = EGL_NO_CONTEXT;
                if (oldHdr->read == oldD) oldHdr->read = 0;
                if (oldD->deletePending) unlinkAndFreeSurface(st, oldD);
            }
            if (oldHdr->read) {
                PfSurface* oldR = oldHdr->read;
                disconnectSurface(oldR);
                oldR->boundCtx = EGL_NO_CONTEXT;
                if (oldR->deletePending) unlinkAndFreeSurface(st, oldR);
            }
            oldHdr->draw = 0;
            oldHdr->read = 0;
            oldHdr->isCurrent = 0;
            if (oldHdr->deletePending)
                unlinkAndFreeContext(st, st->currentCtx);
        }
        st->currentCtx = EGL_NO_CONTEXT;
        st->currentDraw = 0;
        st->currentRead = 0;
        setGlThreadSpecific(0);
        return pfSetError(EGL_SUCCESS, EGL_TRUE);
    }

    if (ctx == EGL_NO_CONTEXT) {
        st->currentCtx = EGL_NO_CONTEXT;
        st->currentDraw = 0;
        st->currentRead = 0;
        setGlThreadSpecific(0);
        return pfSetError(EGL_SUCCESS, EGL_TRUE);
    }

    /* Detach old surfaces if switching surfaces on the SAME context */
    if (newHdr->draw && newHdr->draw != d && newHdr->draw != r) {
        disconnectSurface(newHdr->draw);
        newHdr->draw->boundCtx = EGL_NO_CONTEXT;
        if (newHdr->draw->deletePending)
            unlinkAndFreeSurface(st, newHdr->draw);
    }
    if (newHdr->read && newHdr->read != newHdr->draw &&
        newHdr->read != d && newHdr->read != r) {
        disconnectSurface(newHdr->read);
        newHdr->read->boundCtx = EGL_NO_CONTEXT;
        if (newHdr->read->deletePending)
            unlinkAndFreeSurface(st, newHdr->read);
    }

    if (!connectSurface(d))
        return pfSetError(EGL_BAD_ALLOC, EGL_FALSE);
    if (r != d && !connectSurface(r))
        return pfSetError(EGL_BAD_ALLOC, EGL_FALSE);

    ogles_context_t* gl = (ogles_context_t*)ctx;
    setGlThreadSpecific(gl);

    d->boundCtx = ctx;
    r->boundCtx = ctx;
    newHdr->draw = d;
    newHdr->read = r;
    newHdr->isCurrent = 1;
    st->currentCtx = ctx;
    st->currentDraw = d;
    st->currentRead = r;

    /* CRITICAL: bindDrawToGl BEFORE initial ogles_viewport/ogles_scissor so
     * gl->rasterizer.state.buffers.color.height is populated for Y-flip! */
    bindDrawToGl(d, gl);
    bindReadToGl(r, gl);

    if (newHdr->neverCurrent) {
        newHdr->neverCurrent = 0;
        ogles_surfaceport(gl, 0, 0);
        ogles_viewport(gl, 0, 0, d->width, d->height);
        ogles_scissor(gl, 0, 0, d->width, d->height);
    }

    return pfSetError(EGL_SUCCESS, EGL_TRUE);
}

EGLAPI EGLContext EGLAPIENTRY eglGetCurrentContext(void)
{
    PfEglState* st = getEglState(false);
    return st ? st->currentCtx : EGL_NO_CONTEXT;
}

EGLAPI EGLSurface EGLAPIENTRY eglGetCurrentSurface(EGLint readdraw)
{
    PfEglState* st = getEglState(false);
    if (!st) return EGL_NO_SURFACE;
    if (readdraw == EGL_DRAW) return (EGLSurface)st->currentDraw;
    if (readdraw == EGL_READ) return (EGLSurface)st->currentRead;
    pfSetError(EGL_BAD_PARAMETER, 0);
    return EGL_NO_SURFACE;
}

EGLAPI EGLDisplay EGLAPIENTRY eglGetCurrentDisplay(void)
{
    PfEglState* st = getEglState(false);
    return (st && st->currentCtx != EGL_NO_CONTEXT) ? PF_DEFAULT_DISPLAY : EGL_NO_DISPLAY;
}

EGLAPI EGLBoolean EGLAPIENTRY eglQueryContext(EGLDisplay dpy, EGLContext ctx,
                                              EGLint attribute, EGLint *value)
{
    if (dpy != PF_DEFAULT_DISPLAY)
        return pfSetError(EGL_BAD_DISPLAY, EGL_FALSE);
    PfEglState* st = getEglState(false);
    if (!st || !st->initialized)
        return pfSetError(EGL_NOT_INITIALIZED, EGL_FALSE);
    PfEglContextHdr* hdr = getCtxHdr(ctx);
    if (!hdr)
        return pfSetError(EGL_BAD_CONTEXT, EGL_FALSE);
    if (!value)
        return pfSetError(EGL_BAD_PARAMETER, EGL_FALSE);

    if (attribute == EGL_CONFIG_ID) {
        const PfConfigDesc* cfg = findConfig(hdr->config);
        *value = cfg ? cfg->configId : 1;
        return pfSetError(EGL_SUCCESS, EGL_TRUE);
    }
    return pfSetError(EGL_BAD_ATTRIBUTE, EGL_FALSE);
}

EGLAPI EGLBoolean EGLAPIENTRY eglWaitGL(void)
{
    glFinish();
    return EGL_TRUE;
}

EGLAPI EGLBoolean EGLAPIENTRY eglWaitNative(EGLint engine)
{
    if (engine != EGL_CORE_NATIVE_ENGINE)
        return pfSetError(EGL_BAD_PARAMETER, EGL_FALSE);
    return EGL_TRUE;
}

EGLAPI EGLBoolean EGLAPIENTRY eglSwapBuffers(EGLDisplay dpy, EGLSurface surface)
{
    if (dpy != PF_DEFAULT_DISPLAY)
        return pfSetError(EGL_BAD_DISPLAY, EGL_FALSE);
    PfEglState* st = getEglState(false);
    if (!st || !st->initialized)
        return pfSetError(EGL_NOT_INITIALIZED, EGL_FALSE);
    if (!isValidSurface(surface))
        return pfSetError(EGL_BAD_SURFACE, EGL_FALSE);

    PfSurface* s = (PfSurface*)surface;
    if (s->kind != PF_SURF_WINDOW)
        return pfSetError(EGL_SUCCESS, EGL_TRUE);

#ifdef __SYMBIAN32__
    if (s->nativeWindow && s->windowGc && s->wsBitmap) {
        /* Unlock FBS heap before Window Server BitBlt */
        if (s->fbsLocked) {
            sym_CFbsBitmap_UnlockHeap(s->wsBitmap, 0);
            s->fbsLocked = 0;
        }

        SymTRect drawRect;
        sym_RDrawableWindow_GetDrawRect(&drawRect, s->nativeWindow);
        int isEmpty = sym_TRect_IsEmpty(&drawRect);
        if (isEmpty != 0)
            sym_RWindow_Invalidate(s->nativeWindow);

        sym_CWindowGc_Activate(s->windowGc, s->nativeWindow);
        if (isEmpty != 0)
            sym_RWindow_BeginRedraw(s->nativeWindow);

        SymTPoint origin = { 0, 0 };
        sym_CWindowGc_BitBlt(s->windowGc, &origin, s->wsBitmap);

        if (isEmpty != 0)
            sym_RWindow_EndRedraw(s->nativeWindow);
        sym_CWindowGc_Deactivate(s->windowGc);

        /* Handle dynamic S60 screen rotation / window resize */
        SymTSize winSz = { 0, 0 };
        sym_RWindowBase_Size(&winSz, s->nativeWindow);
        if (winSz.iWidth > 0 && winSz.iHeight > 0 &&
            (winSz.iWidth != s->width || winSz.iHeight != s->height)) {
            (void)sym_CFbsBitmap_Resize(s->wsBitmap, &winSz);
        }

        /* Re-lock FBS heap & refresh data pointer if surface is current */
        if (s->boundCtx != EGL_NO_CONTEXT) {
            (void)connectSurface(s);
            if (s->boundCtx == st->currentCtx) {
                ogles_context_t* gl = (ogles_context_t*)st->currentCtx;
                if (st->currentDraw == s) bindDrawToGl(s, gl);
                if (st->currentRead == s) bindReadToGl(s, gl);
            }
        }
    }
#endif
    return pfSetError(EGL_SUCCESS, EGL_TRUE);
}

EGLAPI EGLBoolean EGLAPIENTRY eglCopyBuffers(EGLDisplay dpy, EGLSurface surface,
                                             EGLNativePixmapType target)
{
    if (dpy != PF_DEFAULT_DISPLAY)
        return pfSetError(EGL_BAD_DISPLAY, EGL_FALSE);
    PfEglState* st = getEglState(false);
    if (!st || !st->initialized)
        return pfSetError(EGL_NOT_INITIALIZED, EGL_FALSE);
    if (!isValidSurface(surface))
        return pfSetError(EGL_BAD_SURFACE, EGL_FALSE);
    if (!target)
        return pfSetError(EGL_BAD_NATIVE_PIXMAP, EGL_FALSE);

#ifdef __SYMBIAN32__
    PfSurface* s = (PfSurface*)surface;
    bool tempLocked = false;
    if (!s->fbsLocked && (s->kind == PF_SURF_WINDOW || s->kind == PF_SURF_PIXMAP)) {
        (void)connectSurface(s);
        tempLocked = true;
    }

    SymTSize dstSz = { 0, 0 };
    sym_CFbsBitmap_SizeInPixels(&dstSz, target);
    int dstMode = sym_CFbsBitmap_DisplayMode(target);
    int dstByteStride = sym_CFbsBitmap_ScanLineLength(dstSz.iWidth, dstMode);

    if (dstSz.iWidth <= 0 || dstSz.iHeight <= 0 || !s->color.data) {
        if (tempLocked) disconnectSurface(s);
        return pfSetError(EGL_BAD_NATIVE_PIXMAP, EGL_FALSE);
    }

    sym_CFbsBitmap_LockHeap(target, 0);
    uint8_t* dstBase = (uint8_t*)sym_CFbsBitmap_DataAddress(target);
    int copyW = (s->width < dstSz.iWidth) ? s->width : dstSz.iWidth;
    int copyH = (s->height < dstSz.iHeight) ? s->height : dstSz.iHeight;

    if (dstBase && copyW > 0 && copyH > 0) {
        int srcBpp = s->cfg->bytesPerPixel;
        int srcByteStride = s->stridePixels * srcBpp;
        const uint8_t* srcBase = (const uint8_t*)s->color.data;

        if (dstMode == s->cfg->symDisplayMode ||
            (srcBpp == 4 && (dstMode == SYM_DISP_ECOLOR16MU || dstMode == SYM_DISP_ECOLOR16MA))) {
            size_t rowBytes = (size_t)copyW * (size_t)srcBpp;
            for (int y = 0; y < copyH; y++)
                memcpy(dstBase + y * dstByteStride, srcBase + y * srcByteStride, rowBytes);
        } else if (s->cfg->gglFormat == GGL_PIXEL_FORMAT_RGB_565 &&
                   (dstMode == SYM_DISP_ECOLOR16MU || dstMode == SYM_DISP_ECOLOR16MA)) {
            for (int y = 0; y < copyH; y++) {
                const uint16_t* sp = (const uint16_t*)(srcBase + y * srcByteStride);
                uint32_t* dp = (uint32_t*)(dstBase + y * dstByteStride);
                for (int x = 0; x < copyW; x++) {
                    uint32_t v = sp[x];
                    uint32_t r = ((v >> 11) & 0x1Fu); r = (r << 3) | (r >> 2);
                    uint32_t g = ((v >>  5) & 0x3Fu); g = (g << 2) | (g >> 4);
                    uint32_t b = ( v        & 0x1Fu); b = (b << 3) | (b >> 2);
                    dp[x] = 0xFF000000u | (r << 16) | (g << 8) | b;
                }
            }
        } else if (s->cfg->gglFormat == GGL_PIXEL_FORMAT_BGRA_8888 &&
                   dstMode == SYM_DISP_ECOLOR64K) {
            for (int y = 0; y < copyH; y++) {
                const uint32_t* sp = (const uint32_t*)(srcBase + y * srcByteStride);
                uint16_t* dp = (uint16_t*)(dstBase + y * dstByteStride);
                for (int x = 0; x < copyW; x++) {
                    uint32_t v = sp[x];
                    uint16_t r = (uint16_t)((v >> 19) & 0x1Fu);
                    uint16_t g = (uint16_t)((v >> 10) & 0x3Fu);
                    uint16_t b = (uint16_t)((v >>  3) & 0x1Fu);
                    dp[x] = (uint16_t)((r << 11) | (g << 5) | b);
                }
            }
        }
    }

    sym_CFbsBitmap_UnlockHeap(target, 0);
    if (tempLocked) disconnectSurface(s);
    return pfSetError(EGL_SUCCESS, EGL_TRUE);
#else
    PfSurface* s = (PfSurface*)surface;
    if (!s->color.data)
        return pfSetError(EGL_BAD_SURFACE, EGL_FALSE);
    uint32_t* dst32 = (uint32_t*)target;
    int srcBpp = s->cfg->bytesPerPixel;
    int srcByteStride = s->stridePixels * srcBpp;
    const uint8_t* srcBase = (const uint8_t*)s->color.data;
    for (int y = 0; y < s->height; y++) {
        if (s->cfg->gglFormat == GGL_PIXEL_FORMAT_RGB_565) {
            const uint16_t* sp = (const uint16_t*)(srcBase + y * srcByteStride);
            uint32_t* dp = dst32 + y * s->width;
            for (int x = 0; x < s->width; x++) {
                uint32_t v = sp[x];
                uint32_t r = ((v >> 11) & 0x1Fu); r = (r << 3) | (r >> 2);
                uint32_t g = ((v >>  5) & 0x3Fu); g = (g << 2) | (g >> 4);
                uint32_t b = ( v        & 0x1Fu); b = (b << 3) | (b >> 2);
                dp[x] = 0xFF000000u | (r << 16) | (g << 8) | b;
            }
        } else {
            memcpy(dst32 + y * s->width, srcBase + y * srcByteStride, (size_t)s->width * 4u);
        }
    }
    return pfSetError(EGL_SUCCESS, EGL_TRUE);
#endif
}

EGLAPI EGLBoolean EGLAPIENTRY eglBindTexImage(EGLDisplay dpy, EGLSurface surface, EGLint buffer)
{
    if (dpy != PF_DEFAULT_DISPLAY)
        return pfSetError(EGL_BAD_DISPLAY, EGL_FALSE);
    PfEglState* st = getEglState(false);
    if (!st || !st->initialized)
        return pfSetError(EGL_NOT_INITIALIZED, EGL_FALSE);
    if (!isValidSurface(surface))
        return pfSetError(EGL_BAD_SURFACE, EGL_FALSE);
    if (buffer != EGL_BACK_BUFFER)
        return pfSetError(EGL_BAD_PARAMETER, EGL_FALSE);

    PfSurface* s = (PfSurface*)surface;
    if (s->kind != PF_SURF_PBUFFER || s->texFormat == EGL_NO_TEXTURE)
        return pfSetError(EGL_BAD_MATCH, EGL_FALSE);
    if (s->boundTexObj)
        return pfSetError(EGL_BAD_ACCESS, EGL_FALSE);

    ogles_context_t* c = ogles_context_t::get();
    if (!c)
        return pfSetError(EGL_SUCCESS, EGL_TRUE);

    EGLTextureObject* tex = c->textures.tmu[c->textures.active].texture;
    if (tex) {
        tex->setSurface(&s->color);
        tex->incStrong(s);
        s->boundTexObj = tex;
        c->textures.tmu[c->textures.active].dirty = 0xFF;
    }
    return pfSetError(EGL_SUCCESS, EGL_TRUE);
}

EGLAPI EGLBoolean EGLAPIENTRY eglReleaseTexImage(EGLDisplay dpy, EGLSurface surface, EGLint buffer)
{
    if (dpy != PF_DEFAULT_DISPLAY)
        return pfSetError(EGL_BAD_DISPLAY, EGL_FALSE);
    PfEglState* st = getEglState(false);
    if (!st || !st->initialized)
        return pfSetError(EGL_NOT_INITIALIZED, EGL_FALSE);
    if (!isValidSurface(surface))
        return pfSetError(EGL_BAD_SURFACE, EGL_FALSE);
    if (buffer != EGL_BACK_BUFFER)
        return pfSetError(EGL_BAD_PARAMETER, EGL_FALSE);

    PfSurface* s = (PfSurface*)surface;
    if (s->kind != PF_SURF_PBUFFER || s->texFormat == EGL_NO_TEXTURE)
        return pfSetError(EGL_BAD_MATCH, EGL_FALSE);

    if (s->boundTexObj) {
        GGLSurface emptySurf;
        memset(&emptySurf, 0, sizeof(emptySurf));
        emptySurf.version = sizeof(GGLSurface);
        s->boundTexObj->setSurface(&emptySurf);
        s->boundTexObj->decStrong(s);
        s->boundTexObj = 0;

        ogles_context_t* c = ogles_context_t::get();
        if (c)
            c->textures.tmu[c->textures.active].dirty = 0xFF;
    }
    return pfSetError(EGL_SUCCESS, EGL_TRUE);
}

EGLAPI EGLBoolean EGLAPIENTRY eglSwapInterval(EGLDisplay dpy, EGLint interval)
{
    (void)interval;
    if (dpy != PF_DEFAULT_DISPLAY)
        return pfSetError(EGL_BAD_DISPLAY, EGL_FALSE);
    PfEglState* st = getEglState(false);
    if (!st || !st->initialized)
        return pfSetError(EGL_NOT_INITIALIZED, EGL_FALSE);
    return pfSetError(EGL_SUCCESS, EGL_TRUE);
}

/* --- eglGetProcAddress (.rodata table of OES & core entry points) --------- */

struct PfProcEntry {
    const char* name;
    __eglMustCastToProperFunctionPointerType proc;
};

#define PF_PROC(fn) { #fn, (__eglMustCastToProperFunctionPointerType)&fn }

static const PfProcEntry kProcTable[] = {
    PF_PROC(glDrawTexsOES),
    PF_PROC(glDrawTexiOES),
    PF_PROC(glDrawTexxOES),
    PF_PROC(glDrawTexsvOES),
    PF_PROC(glDrawTexivOES),
    PF_PROC(glDrawTexxvOES),
    PF_PROC(glDrawTexfOES),
    PF_PROC(glDrawTexfvOES),
    PF_PROC(glQueryMatrixxOES),
    PF_PROC(glPointSizePointerOES),
    PF_PROC(glBindBuffer),
    PF_PROC(glBufferData),
    PF_PROC(glBufferSubData),
    PF_PROC(glDeleteBuffers),
    PF_PROC(glGenBuffers),
    PF_PROC(glIsBuffer),
    PF_PROC(glIsEnabled),
    PF_PROC(glIsTexture),
    PF_PROC(eglBindTexImage),
    PF_PROC(eglReleaseTexImage),
    PF_PROC(eglSurfaceAttrib),
    PF_PROC(eglSwapInterval)
};

EGLAPI __eglMustCastToProperFunctionPointerType EGLAPIENTRY eglGetProcAddress(const char *procname)
{
    if (!procname)
        return 0;
    const size_t n = sizeof(kProcTable) / sizeof(kProcTable[0]);
    for (size_t i = 0; i < n; i++) {
        if (strcmp(procname, kProcTable[i].name) == 0)
            return kProcTable[i].proc;
    }
    return 0;
}

} /* extern "C" */
