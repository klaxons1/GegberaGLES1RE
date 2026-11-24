/* test_egl.c - unit & end-to-end integration tests for Gerbera EGL 1.1 + GLES 1.1. */
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <EGL/egl.h>
#include <GLES/egl.h>
#include <GLES/gl.h>
#include "gerbera/egl/eglInternal.h"

static int g_checks = 0;
static int g_fails  = 0;

#define CHECK(cond)                                                     \
    do                                                                  \
    {                                                                   \
        ++g_checks;                                                     \
        if (!(cond))                                                    \
        {                                                               \
            ++g_fails;                                                  \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        }                                                               \
    } while (0)

/* -------------------------------------------------------------------------- */
/* 1. Pre-initialization & Display Lifecycle                                  */
/* -------------------------------------------------------------------------- */
static void test_preinit_and_lifecycle(void)
{
    EGLDisplay dpy;
    EGLint major = 0, minor = 0;
    EGLint numCfg = 0;
    EGLint val = 0;

    /* Ensure clean uninitialized starting state */
    dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    CHECK(dpy == (EGLDisplay)1);
    CHECK(eglGetError() == EGL_SUCCESS);
    CHECK(eglGetDisplay((NativeDisplayType)42) == EGL_NO_DISPLAY);
    CHECK(eglGetError() == EGL_SUCCESS);

    eglTerminate(dpy);
    CHECK(eglGetError() == EGL_SUCCESS);

    /* Uninitialized queries & error sentinel tags in TLS */
    CHECK(eglGetCurrentContext() == EGL_NO_CONTEXT);
    CHECK(eglGetCurrentSurface(EGL_DRAW) == EGL_NO_SURFACE);
    CHECK(eglGetCurrentSurface(EGL_READ) == EGL_NO_SURFACE);
    CHECK(eglGetCurrentDisplay() == EGL_NO_DISPLAY);

    CHECK(eglQueryString(dpy, EGL_VENDOR) == NULL);
    CHECK(eglGetError() == EGL_NOT_INITIALIZED);
    /* Reading eglGetError resets TLS sentinel to EGL_SUCCESS */
    CHECK(eglGetError() == EGL_SUCCESS);

    CHECK(eglInitialize(EGL_NO_DISPLAY, &major, &minor) == EGL_FALSE);
    CHECK(eglGetError() == EGL_BAD_DISPLAY);
    CHECK(eglGetError() == EGL_SUCCESS);

    CHECK(eglGetConfigs(dpy, NULL, 0, &numCfg) == EGL_FALSE);
    CHECK(eglGetError() == EGL_NOT_INITIALIZED);
    CHECK(eglChooseConfig(dpy, NULL, NULL, 0, &numCfg) == EGL_FALSE);
    CHECK(eglGetError() == EGL_NOT_INITIALIZED);
    CHECK(eglCreatePbufferSurface(dpy, 1, NULL) == EGL_NO_SURFACE);
    CHECK(eglGetError() == EGL_NOT_INITIALIZED);
    CHECK(eglCreateWindowSurface(dpy, 1, NULL, NULL) == EGL_NO_SURFACE);
    CHECK(eglGetError() == EGL_NOT_INITIALIZED);
    CHECK(eglCreatePixmapSurface(dpy, 1, NULL, NULL) == EGL_NO_SURFACE);
    CHECK(eglGetError() == EGL_NOT_INITIALIZED);
    CHECK(eglCreateContext(dpy, 1, EGL_NO_CONTEXT, NULL) == EGL_NO_CONTEXT);
    CHECK(eglGetError() == EGL_NOT_INITIALIZED);
    CHECK(eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT) == EGL_FALSE);
    CHECK(eglGetError() == EGL_NOT_INITIALIZED);
    CHECK(eglSwapBuffers(dpy, 1) == EGL_FALSE);
    CHECK(eglGetError() == EGL_NOT_INITIALIZED);
    CHECK(eglCopyBuffers(dpy, 1, NULL) == EGL_FALSE);
    CHECK(eglGetError() == EGL_NOT_INITIALIZED);
    CHECK(eglWaitGL() == EGL_FALSE);
    CHECK(eglGetError() == EGL_NOT_INITIALIZED);
    CHECK(eglWaitNative(EGL_CORE_NATIVE_ENGINE) == EGL_FALSE);
    CHECK(eglGetError() == EGL_NOT_INITIALIZED);
    CHECK(eglBindTexImage(dpy, 1, EGL_BACK_BUFFER) == EGL_FALSE);
    CHECK(eglGetError() == EGL_NOT_INITIALIZED);
    CHECK(eglReleaseTexImage(dpy, 1, EGL_BACK_BUFFER) == EGL_FALSE);
    CHECK(eglGetError() == EGL_NOT_INITIALIZED);
    CHECK(eglSwapInterval(dpy, 1) == EGL_FALSE);
    CHECK(eglGetError() == EGL_NOT_INITIALIZED);
    CHECK(eglSurfaceAttrib(dpy, 1, EGL_MIPMAP_LEVEL, 0) == EGL_FALSE);
    CHECK(eglGetError() == EGL_NOT_INITIALIZED);
    CHECK(eglQuerySurface(dpy, 1, EGL_WIDTH, &val) == EGL_FALSE);
    CHECK(eglGetError() == EGL_NOT_INITIALIZED);
    CHECK(eglQueryContext(dpy, 1, EGL_CONFIG_ID, &val) == EGL_FALSE);
    CHECK(eglGetError() == EGL_NOT_INITIALIZED);

    /* Initialize EGL 1.1 */
    CHECK(eglInitialize(dpy, &major, &minor) == EGL_TRUE);
    CHECK(eglGetError() == EGL_SUCCESS);
    CHECK(major == 1 && minor == 1);

    /* Idempotent second eglInitialize */
    major = 0;
    minor = 0;
    CHECK(eglInitialize(dpy, &major, &minor) == EGL_TRUE);
    CHECK(major == 1 && minor == 1);

    /* Strings */
    CHECK(strcmp(eglQueryString(dpy, EGL_VENDOR), "Hybrid Graphics, Ltd.") == 0);
    CHECK(strcmp(eglQueryString(dpy, EGL_VERSION), "1.1") == 0);
    CHECK(strcmp(eglQueryString(dpy, EGL_EXTENSIONS), "") == 0);
    CHECK(eglQueryString(EGL_NO_DISPLAY, EGL_VENDOR) == NULL);
    CHECK(eglGetError() == EGL_BAD_DISPLAY);
    CHECK(eglQueryString(dpy, 0x9999) == NULL);
    CHECK(eglGetError() == EGL_BAD_PARAMETER);

    /* eglGetProcAddress: all 18 exported OES / ES 1.1 procedures */
    CHECK(eglGetProcAddress(NULL) == NULL);
    CHECK(eglGetProcAddress("glNonExistentFunctionOES") == NULL);
    CHECK(eglGetProcAddress("glWeightPointerOES") == (void (*)(void))glWeightPointerOES);
    CHECK(eglGetProcAddress("glMatrixIndexPointerOES") == (void (*)(void))glMatrixIndexPointerOES);
    CHECK(eglGetProcAddress("glPointSizePointerOES") == (void (*)(void))glPointSizePointerOES);
    CHECK(eglGetProcAddress("glCurrentPaletteMatrixOES") == (void (*)(void))glCurrentPaletteMatrixOES);
    CHECK(eglGetProcAddress("glLoadPaletteFromModelViewMatrixOES") == (void (*)(void))glLoadPaletteFromModelViewMatrixOES);
    CHECK(eglGetProcAddress("glDrawTexfOES") == (void (*)(void))glDrawTexfOES);
    CHECK(eglGetProcAddress("glDrawTexxOES") == (void (*)(void))glDrawTexxOES);
    CHECK(eglGetProcAddress("glDrawTexiOES") == (void (*)(void))glDrawTexiOES);
    CHECK(eglGetProcAddress("glDrawTexsOES") == (void (*)(void))glDrawTexsOES);
    CHECK(eglGetProcAddress("glDrawTexfvOES") == (void (*)(void))glDrawTexfvOES);
    CHECK(eglGetProcAddress("glDrawTexxvOES") == (void (*)(void))glDrawTexxvOES);
    CHECK(eglGetProcAddress("glDrawTexivOES") == (void (*)(void))glDrawTexivOES);
    CHECK(eglGetProcAddress("glDrawTexsvOES") == (void (*)(void))glDrawTexsvOES);
    CHECK(eglGetProcAddress("glTexParameteri") == (void (*)(void))glTexParameteri);
    CHECK(eglGetProcAddress("glTexParameterfv") == (void (*)(void))glTexParameterfv);
    CHECK(eglGetProcAddress("glTexParameterxv") == (void (*)(void))glTexParameterxv);
    CHECK(eglGetProcAddress("glTexParameteriv") == (void (*)(void))glTexParameteriv);
    CHECK(eglGetProcAddress("glQueryMatrixxOES") == (void (*)(void))glQueryMatrixxOES);

    CHECK(eglTerminate(dpy) == EGL_TRUE);
}

/* -------------------------------------------------------------------------- */
/* 2. Config Enumeration, Caveats, Filtering, & Sorting                       */
/* -------------------------------------------------------------------------- */
static void test_configs(void)
{
    EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EGLConfig all[80];
    EGLConfig sub[5];
    EGLint count = 0;
    EGLint val = 0;
    int i;

    egliSetDefaultDisplayMode(EGL_SYMBIAN_ECOLOR64K);
    CHECK(eglInitialize(dpy, NULL, NULL) == EGL_TRUE);

    CHECK(eglGetConfigs(dpy, NULL, 0, NULL) == EGL_FALSE);
    CHECK(eglGetError() == EGL_BAD_PARAMETER);

    CHECK(eglGetConfigs(dpy, NULL, 0, &count) == EGL_TRUE);
    CHECK(count == 48);

    CHECK(eglGetConfigs(dpy, sub, 5, &count) == EGL_TRUE);
    CHECK(count == 5);
    CHECK(sub[0] == 1 && sub[4] == 13);

    CHECK(eglGetConfigs(dpy, all, 80, &count) == EGL_TRUE);
    CHECK(count == 48);

    for (i = 0; i < count; ++i)
    {
        EGLConfig c = all[i];
        EGLint cid = 0, stype = 0, mw = 0, mh = 0, mp = 0, minSw = -1, maxSw = -1, alpha = -1;

        CHECK(eglGetConfigAttrib(dpy, c, EGL_CONFIG_ID, &cid) == EGL_TRUE);
        CHECK(cid == c);
        CHECK(eglGetConfigAttrib(dpy, c, EGL_SURFACE_TYPE, &stype) == EGL_TRUE);
        CHECK(eglGetConfigAttrib(dpy, c, EGL_MAX_PBUFFER_WIDTH, &mw) == EGL_TRUE);
        CHECK(eglGetConfigAttrib(dpy, c, EGL_MAX_PBUFFER_HEIGHT, &mh) == EGL_TRUE);
        CHECK(eglGetConfigAttrib(dpy, c, EGL_MAX_PBUFFER_PIXELS, &mp) == EGL_TRUE);
        CHECK(eglGetConfigAttrib(dpy, c, EGL_MIN_SWAP_INTERVAL, &minSw) == EGL_TRUE);
        CHECK(eglGetConfigAttrib(dpy, c, EGL_MAX_SWAP_INTERVAL, &maxSw) == EGL_TRUE);
        CHECK(eglGetConfigAttrib(dpy, c, EGL_ALPHA_SIZE, &alpha) == EGL_TRUE);
        CHECK(minSw == 1 && maxSw == 1);
        CHECK(alpha == 0 || alpha == 4 || alpha == 8);
        CHECK(mw == EGL_MAX_PBUFFER_DIM && mh == EGL_MAX_PBUFFER_DIM && mp == EGL_MAX_PBUFFER_PIX);
    }

    CHECK(eglGetConfigAttrib(dpy, 0, EGL_CONFIG_ID, &val) == EGL_FALSE);
    CHECK(eglGetError() == EGL_BAD_CONFIG);
    CHECK(eglGetConfigAttrib(dpy, 73, EGL_CONFIG_ID, &val) == EGL_FALSE);
    CHECK(eglGetError() == EGL_BAD_CONFIG);
    CHECK(eglGetConfigAttrib(dpy, 1, 0x9999, &val) == EGL_FALSE);
    CHECK(eglGetError() == EGL_BAD_ATTRIBUTE);

    /* Window configs must match the original native display format. */
    CHECK(eglChooseConfig(dpy, NULL, all, 80, &count) == EGL_TRUE);
    CHECK(count == 6);
    {
        EGLint caveat0 = 0, bits0 = 0, caveatLast = 0;
        CHECK(eglGetConfigAttrib(dpy, all[0], EGL_CONFIG_CAVEAT, &caveat0) == EGL_TRUE);
        CHECK(eglGetConfigAttrib(dpy, all[0], EGL_BUFFER_SIZE, &bits0) == EGL_TRUE);
        CHECK(eglGetConfigAttrib(dpy, all[5], EGL_CONFIG_CAVEAT, &caveatLast) == EGL_TRUE);
        CHECK(caveat0 == EGL_NONE && bits0 == 16);
        CHECK(caveatLast == EGL_NONE);
    }

    /* Filter for RGBA8888 + Depth 16 + Stencil 8 + Pbuffer + BindToTextureRGBA */
    {
        const EGLint attrs[] =
        {
            EGL_SURFACE_TYPE,         EGL_PBUFFER_BIT,
            EGL_RED_SIZE,             8,
            EGL_GREEN_SIZE,           8,
            EGL_BLUE_SIZE,            8,
            EGL_ALPHA_SIZE,           8,
            EGL_DEPTH_SIZE,           16,
            EGL_STENCIL_SIZE,         8,
            EGL_BIND_TO_TEXTURE_RGBA, EGL_TRUE,
            EGL_NONE
        };
        EGLint dbits = 0, sbits = 0;

        CHECK(eglChooseConfig(dpy, attrs, all, 80, &count) == EGL_TRUE);
        /* Depth 16 and Depth 32 both satisfy >= 16; sorted ascending by depthBits (16 first) */
        CHECK(count == 2);
        CHECK(eglGetConfigAttrib(dpy, all[0], EGL_DEPTH_SIZE, &dbits) == EGL_TRUE);
        CHECK(eglGetConfigAttrib(dpy, all[0], EGL_STENCIL_SIZE, &sbits) == EGL_TRUE);
        CHECK(dbits == 16 && sbits == 8);
    }

    /* Filter by exact EGL_CONFIG_ID overrides all other criteria */
    {
        const EGLint byId[] = { EGL_CONFIG_ID, 13, EGL_RED_SIZE, 99, EGL_NONE };
        CHECK(eglChooseConfig(dpy, byId, all, 80, &count) == EGL_TRUE);
        CHECK(count == 1 && all[0] == 13);
    }

    /* Bad filter attributes */
    {
        const EGLint bad1[] = { EGL_LEVEL, EGL_DONT_CARE, EGL_NONE };
        const EGLint bad2[] = { EGL_RED_SIZE, -5, EGL_NONE };
        const EGLint bad3[] = { 0x3999, 0, EGL_NONE };
        const EGLint bad4[] = { EGL_LEVEL, -2, EGL_NONE };
        const EGLint bad5[] = { EGL_NATIVE_VISUAL_TYPE, -2, EGL_NONE };
        const EGLint bad6[] = { EGL_RED_SIZE, -2, EGL_RED_SIZE, 0, EGL_NONE };
        CHECK(eglChooseConfig(dpy, bad1, all, 80, &count) == EGL_FALSE);
        CHECK(eglGetError() == EGL_BAD_ATTRIBUTE);
        CHECK(eglChooseConfig(dpy, bad2, all, 80, &count) == EGL_FALSE);
        CHECK(eglGetError() == EGL_BAD_ATTRIBUTE);
        CHECK(eglChooseConfig(dpy, bad3, all, 80, &count) == EGL_FALSE);
        CHECK(eglGetError() == EGL_BAD_ATTRIBUTE);
        CHECK(eglChooseConfig(dpy, bad4, all, 80, &count) == EGL_FALSE);
        CHECK(eglGetError() == EGL_BAD_ATTRIBUTE);
        CHECK(eglChooseConfig(dpy, bad5, all, 80, &count) == EGL_FALSE);
        CHECK(eglGetError() == EGL_BAD_ATTRIBUTE);
        CHECK(eglChooseConfig(dpy, bad6, all, 80, &count) == EGL_FALSE);
        CHECK(eglGetError() == EGL_BAD_ATTRIBUTE);
    }

    {
        const EGLint pbuffer[] = { EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_NONE };
        const EGLint invalid[] = { 0x3999, 0, EGL_NONE };

        /* Original 0x403540 preserves errors and sorts only the selected prefix. */
        CHECK(eglChooseConfig(dpy, invalid, all, 80, &count) == EGL_FALSE);
        CHECK(eglChooseConfig(dpy, pbuffer, all, 3, &count) == EGL_TRUE);
        CHECK(eglGetError() == EGL_BAD_ATTRIBUTE);
        CHECK(count == 3 && all[0] == 1 && all[1] == 4 && all[2] == 7);

        /* Its equality limit accepts a negative size; provide a full array. */
        CHECK(eglChooseConfig(dpy, pbuffer, all, -1, &count) == EGL_TRUE);
        CHECK(count == 24 && all[0] == 1 && all[1] == 19);
        all[0] = -99;
        CHECK(eglChooseConfig(dpy, pbuffer, all, 0, &count) == EGL_TRUE);
        CHECK(count == 0 && all[0] == -99);
    }

    CHECK(eglTerminate(dpy) == EGL_TRUE);
}

/* -------------------------------------------------------------------------- */
/* 3. Context Sharing, Deferred Destruction, & Pbuffer Render-to-Texture      */
/* -------------------------------------------------------------------------- */
static void test_pbuffer_and_render_to_texture(void)
{
    EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EGLConfig rttCfg = 0;
    EGLint count = 0;
    EGLSurface rttSurf, dstSurf, largeSurf;
    EGLContext ctx1, ctx2;
    EGLint val = 0;
    GLuint sharedTex = 0;
    unsigned char pixel[4] = { 0, 0, 0, 0 };

    const EGLint cfgAttrs[] =
    {
        EGL_SURFACE_TYPE,         EGL_PBUFFER_BIT,
        EGL_RED_SIZE,             8,
        EGL_GREEN_SIZE,           8,
        EGL_BLUE_SIZE,            8,
        EGL_ALPHA_SIZE,           8,
        EGL_DEPTH_SIZE,           16,
        EGL_BIND_TO_TEXTURE_RGBA, EGL_TRUE,
        EGL_NONE
    };
    const EGLint rttSurfAttrs[] =
    {
        EGL_WIDTH,          64,
        EGL_HEIGHT,         64,
        EGL_TEXTURE_FORMAT, EGL_TEXTURE_RGBA,
        EGL_TEXTURE_TARGET, EGL_TEXTURE_2D,
        EGL_MIPMAP_TEXTURE, EGL_TRUE,
        EGL_NONE
    };
    const EGLint dstSurfAttrs[] =
    {
        EGL_WIDTH,  64,
        EGL_HEIGHT, 64,
        EGL_NONE
    };
    const EGLint largeSurfAttrs[] =
    {
        EGL_WIDTH,           4096,
        EGL_HEIGHT,          3000,
        EGL_LARGEST_PBUFFER, EGL_TRUE,
        EGL_NONE
    };
    const EGLint npotTexAttrs[] =
    {
        EGL_WIDTH,          60,
        EGL_HEIGHT,         60,
        EGL_TEXTURE_FORMAT, EGL_TEXTURE_RGBA,
        EGL_TEXTURE_TARGET, EGL_TEXTURE_2D,
        EGL_NONE
    };

    CHECK(eglInitialize(dpy, NULL, NULL) == EGL_TRUE);
    CHECK(eglChooseConfig(dpy, cfgAttrs, &rttCfg, 1, &count) == EGL_TRUE && count == 1);

    /* Both original drivers report BAD_ATTRIBUTE for a non-power-of-two texture. */
    CHECK(eglCreatePbufferSurface(dpy, rttCfg, npotTexAttrs) == EGL_NO_SURFACE);
    CHECK(eglGetError() == EGL_BAD_ATTRIBUTE);

    /* EGL_LARGEST_PBUFFER clamps to 2048x2048 */
    largeSurf = eglCreatePbufferSurface(dpy, rttCfg, largeSurfAttrs);
    CHECK(largeSurf != EGL_NO_SURFACE);
    CHECK(eglQuerySurface(dpy, largeSurf, EGL_WIDTH, &val) == EGL_TRUE && val == 2048);
    CHECK(eglQuerySurface(dpy, largeSurf, EGL_HEIGHT, &val) == EGL_TRUE && val == 2048);
    CHECK(eglQuerySurface(dpy, largeSurf, EGL_LARGEST_PBUFFER, &val) == EGL_TRUE && val == EGL_TRUE);
    CHECK(eglDestroySurface(dpy, largeSurf) == EGL_TRUE);

    {
        const EGLint rounded[] = { EGL_WIDTH, 63, EGL_HEIGHT, 31,
            EGL_LARGEST_PBUFFER, EGL_TRUE, EGL_TEXTURE_FORMAT, EGL_TEXTURE_RGBA,
            EGL_TEXTURE_TARGET, EGL_TEXTURE_2D, 0xDEAD, 7, EGL_NONE };
        const EGLint empty[] = { EGL_WIDTH, 0, EGL_HEIGHT, 4,
            EGL_LARGEST_PBUFFER, EGL_TRUE, EGL_TEXTURE_FORMAT, EGL_TEXTURE_RGBA,
            EGL_TEXTURE_TARGET, EGL_TEXTURE_2D, EGL_NONE };
        eglSurface_s *surface;
        largeSurf = eglCreatePbufferSurface(dpy, rttCfg, rounded);
        CHECK(largeSurf != EGL_NO_SURFACE);
        CHECK(eglQuerySurface(dpy, largeSurf, EGL_WIDTH, &val) && val == 32);
        CHECK(eglQuerySurface(dpy, largeSurf, EGL_HEIGHT, &val) && val == 16);
        surface = eglMapSurface(eglMapDisplay(dpy), largeSurf);
        CHECK(surface != NULL && surface->depthBuffer == NULL && surface->stencilBuffer == NULL);
        CHECK(eglDestroySurface(dpy, largeSurf));
        /* The originals shift by -1 for this dimension; the recovered C keeps
         * the empty surface without an undefined shift or oversized allocation. */
        largeSurf = eglCreatePbufferSurface(dpy, rttCfg, empty);
        CHECK(largeSurf != EGL_NO_SURFACE);
        CHECK(eglQuerySurface(dpy, largeSurf, EGL_WIDTH, &val) && val == 0);
        CHECK(eglDestroySurface(dpy, largeSurf));
    }

    rttSurf = eglCreatePbufferSurface(dpy, rttCfg, rttSurfAttrs);
    dstSurf = eglCreatePbufferSurface(dpy, rttCfg, dstSurfAttrs);
    CHECK(rttSurf != EGL_NO_SURFACE && dstSurf != EGL_NO_SURFACE);

    /* Surface attribute queries & EGL_MIPMAP_LEVEL clamping */
    CHECK(eglQuerySurface(dpy, rttSurf, EGL_TEXTURE_FORMAT, &val) == EGL_TRUE && val == EGL_TEXTURE_RGBA);
    CHECK(eglQuerySurface(dpy, rttSurf, EGL_TEXTURE_TARGET, &val) == EGL_TRUE && val == EGL_TEXTURE_2D);
    CHECK(eglQuerySurface(dpy, rttSurf, EGL_MIPMAP_TEXTURE, &val) == EGL_TRUE && val == EGL_TRUE);
    CHECK(eglSurfaceAttrib(dpy, rttSurf, EGL_MIPMAP_LEVEL, 99) == EGL_TRUE);
    CHECK(eglQuerySurface(dpy, rttSurf, EGL_MIPMAP_LEVEL, &val) == EGL_TRUE && val == 11);
    CHECK(eglSurfaceAttrib(dpy, rttSurf, EGL_MIPMAP_LEVEL, 0) == EGL_TRUE);
    CHECK(eglQuerySurface(dpy, rttSurf, EGL_MIPMAP_LEVEL, &val) == EGL_TRUE && val == 0);

    /* Create primary context & shared context */
    ctx1 = eglCreateContext(dpy, rttCfg, EGL_NO_CONTEXT, NULL);
    ctx2 = eglCreateContext(dpy, rttCfg, ctx1, NULL);
    CHECK(ctx1 != EGL_NO_CONTEXT && ctx2 != EGL_NO_CONTEXT);
    CHECK(eglQueryContext(dpy, ctx1, EGL_CONFIG_ID, &val) == EGL_TRUE && val == rttCfg);

    /* Step 1: Make (rttSurf, ctx1) current, render blue clear + green centered quad,
     * and also allocate a shared texture in ctx1 */
    CHECK(eglMakeCurrent(dpy, rttSurf, rttSurf, ctx1) == EGL_TRUE);
    CHECK(eglGetCurrentContext() == ctx1);
    CHECK(eglGetCurrentSurface(EGL_DRAW) == rttSurf);
    CHECK(eglGetCurrentSurface(EGL_READ) == rttSurf);
    CHECK(eglGetCurrentDisplay() == dpy);

    glGenTextures(1, &sharedTex);
    CHECK(sharedTex != 0);
    glBindTexture(GL_TEXTURE_2D, sharedTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    {
        const GLfloat quadVerts[] =
        {
            -0.5f, -0.5f,
             0.5f, -0.5f,
            -0.5f,  0.5f,
             0.5f,  0.5f
        };
        glColor4f(0.0f, 1.0f, 0.0f, 1.0f);
        glEnableClientState(GL_VERTEX_ARRAY);
        glVertexPointer(2, GL_FLOAT, 0, quadVerts);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glDisableClientState(GL_VERTEX_ARRAY);
    }

    /* Verify rendered pixels directly in rttSurf */
    glReadPixels(32, 32, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    CHECK(pixel[0] == 0 && pixel[1] == 255 && pixel[2] == 0 && pixel[3] == 255);
    glReadPixels(4, 4, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    CHECK(pixel[0] == 0 && pixel[1] == 0 && pixel[2] == 255 && pixel[3] == 255);

    /* Test deferred context destruction: destroy ctx1 WHILE current! */
    CHECK(eglDestroyContext(dpy, ctx1) == EGL_TRUE);
    CHECK(eglQueryContext(dpy, ctx1, EGL_CONFIG_ID, &val) == EGL_TRUE && val == rttCfg);
    CHECK(eglGetError() == EGL_SUCCESS);
    CHECK(eglWaitGL() == EGL_TRUE);

    /* Step 2: Switch to (dstSurf, ctx2) -> frees ctx1 cleanly, binds sharedTexture via
     * eglBindTexImage(dpy, rttSurf, EGL_BACK_BUFFER), and renders textured quad! */
    CHECK(eglMakeCurrent(dpy, dstSurf, dstSurf, ctx2) == EGL_TRUE);
    CHECK(glIsTexture(sharedTex) == GL_TRUE);
    glBindTexture(GL_TEXTURE_2D, sharedTex);

    CHECK(eglBindTexImage(dpy, rttSurf, EGL_BACK_BUFFER) == EGL_TRUE);
    CHECK(eglGetError() == EGL_SUCCESS);
    /* Second bind while already bound must fail with EGL_BAD_ACCESS */
    CHECK(eglBindTexImage(dpy, rttSurf, EGL_BACK_BUFFER) == EGL_FALSE);
    CHECK(eglGetError() == EGL_BAD_ACCESS);

    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    {
        const GLfloat fsVerts[] =
        {
            -1.0f, -1.0f,
             1.0f, -1.0f,
            -1.0f,  1.0f,
             1.0f,  1.0f
        };
        const GLfloat fsUVs[] =
        {
            0.0f, 0.0f,
            1.0f, 0.0f,
            0.0f, 1.0f,
            1.0f, 1.0f
        };
        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glVertexPointer(2, GL_FLOAT, 0, fsVerts);
        glTexCoordPointer(2, GL_FLOAT, 0, fsUVs);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        glDisableClientState(GL_VERTEX_ARRAY);
    }
    glDisable(GL_TEXTURE_2D);

    /* Center of dstSurf sampled the green quad from rttSurf; corner sampled blue! */
    memset(pixel, 0, sizeof(pixel));
    glReadPixels(32, 32, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    CHECK(pixel[0] == 0 && pixel[1] == 255 && pixel[2] == 0 && pixel[3] == 255);
    memset(pixel, 0, sizeof(pixel));
    glReadPixels(4, 4, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    CHECK(pixel[0] == 0 && pixel[1] == 0 && pixel[2] == 255 && pixel[3] == 255);

    /* Release texture image and verify re-bind succeeds */
    CHECK(eglReleaseTexImage(dpy, rttSurf, EGL_BACK_BUFFER) == EGL_TRUE);
    CHECK(eglBindTexImage(dpy, rttSurf, EGL_BACK_BUFFER) == EGL_TRUE);
    CHECK(eglReleaseTexImage(dpy, rttSurf, EGL_BACK_BUFFER) == EGL_TRUE);

    CHECK(eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT) == EGL_TRUE);
    CHECK(eglDestroySurface(dpy, rttSurf) == EGL_TRUE);
    CHECK(eglDestroySurface(dpy, dstSurf) == EGL_TRUE);
    CHECK(eglDestroyContext(dpy, ctx2) == EGL_TRUE);
    CHECK(eglTerminate(dpy) == EGL_TRUE);
}

/* -------------------------------------------------------------------------- */
/* 4. Window Surface, SwapBuffers, Dynamic Resize, Pixmap, & CopyBuffers      */
/* -------------------------------------------------------------------------- */
static void test_window_and_pixmap_surfaces(void)
{
    EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EGLConfig winCfg = 0, pixCfg = 0;
    EGLint count = 0;
    EGLSurface ownedWinSurf, winSurf, pixSurf;
    EGLContext winCtx, pixCtx;
    EGLint w = 0, h = 0;
    uint32_t *frontPixels;
    uint32_t *pixmapPixels;
    uint32_t *copyTargetPixels;
    eglNativeWindow_s nativeWin;
    eglNativePixmap_s nativePix;
    eglNativePixmap_s copyTarget;

    const EGLint winAttrs[] =
    {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE,     8,
        EGL_GREEN_SIZE,   8,
        EGL_BLUE_SIZE,    8,
        EGL_ALPHA_SIZE,   8,
        EGL_DEPTH_SIZE,   16,
        EGL_NONE
    };
    const EGLint pixAttrs[] =
    {
        EGL_SURFACE_TYPE, EGL_PIXMAP_BIT,
        EGL_RED_SIZE,     8,
        EGL_GREEN_SIZE,   8,
        EGL_BLUE_SIZE,    8,
        EGL_ALPHA_SIZE,   8,
        EGL_DEPTH_SIZE,   16,
        EGL_NONE
    };

    egliSetDefaultDisplayMode(EGL_SYMBIAN_ECOLOR16MA);
    CHECK(eglInitialize(dpy, NULL, NULL) == EGL_TRUE);
    CHECK(eglChooseConfig(dpy, winAttrs, &winCfg, 1, &count) == EGL_TRUE && count == 1);
    CHECK(eglChooseConfig(dpy, pixAttrs, &pixCfg, 1, &count) == EGL_TRUE && count == 1);

    /* Owned default window (win == NULL -> 320x240) */
    ownedWinSurf = eglCreateWindowSurface(dpy, winCfg, NULL, NULL);
    CHECK(ownedWinSurf != EGL_NO_SURFACE);
    CHECK(eglQuerySurface(dpy, ownedWinSurf, EGL_WIDTH, &w) == EGL_TRUE && w == 320);
    CHECK(eglQuerySurface(dpy, ownedWinSurf, EGL_HEIGHT, &h) == EGL_TRUE && h == 240);
    CHECK(eglDestroySurface(dpy, ownedWinSurf) == EGL_TRUE);

    /* Caller-supplied window (64x48) with frontBuffer */
    frontPixels = (uint32_t *)calloc(96 * 80, sizeof(uint32_t));
    CHECK(frontPixels != NULL);
    memset(&nativeWin, 0, sizeof(nativeWin));
    nativeWin.width = 64;
    nativeWin.height = 48;
    nativeWin.stride = 64 * 4;
    nativeWin.displayMode = EGL_SYMBIAN_ECOLOR16MA;
    nativeWin.frontBuffer = frontPixels;

    winSurf = eglCreateWindowSurface(dpy, winCfg, &nativeWin, NULL);
    CHECK(winSurf != EGL_NO_SURFACE);
    /* Duplicate surface on same nativeWindow must fail with EGL_BAD_ALLOC */
    CHECK(eglCreateWindowSurface(dpy, winCfg, &nativeWin, NULL) == EGL_NO_SURFACE);
    CHECK(eglGetError() == EGL_BAD_ALLOC);

    winCtx = eglCreateContext(dpy, winCfg, EGL_NO_CONTEXT, NULL);
    CHECK(winCtx != EGL_NO_CONTEXT);
    CHECK(eglMakeCurrent(dpy, winSurf, winSurf, winCtx) == EGL_TRUE);
    CHECK(eglSwapInterval(dpy, 1) == EGL_TRUE);
    CHECK(eglWaitNative(EGL_CORE_NATIVE_ENGINE) == EGL_TRUE);

    /* Clear window to magenta (R=255, G=0, B=255, A=255 -> BGRA 0xFFFF00FF) and swap */
    glClearColor(1.0f, 0.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    CHECK(eglSwapBuffers(dpy, winSurf) == EGL_TRUE);
    CHECK(nativeWin.swapCount == 1);
    CHECK(frontPixels[0] == 0xFFFF00FFu);
    CHECK(frontPixels[64 * 48 - 1] == 0xFFFF00FFu);

    /* Dynamic native window resize (64x48 -> 96x80) */
    nativeWin.width = 96;
    nativeWin.height = 80;
    nativeWin.stride = 96 * 4;
    memset(frontPixels, 0, 96 * 80 * sizeof(uint32_t));
    glViewport(0, 0, 96, 80);
    glScissor(0, 0, 96, 80);
    glClearColor(0.0f, 1.0f, 1.0f, 1.0f); /* Cyan: R=0, G=255, B=255, A=255 -> BGRA 0xFF00FFFF */
    glClear(GL_COLOR_BUFFER_BIT);
    CHECK(eglSwapBuffers(dpy, winSurf) == EGL_TRUE);
    CHECK(nativeWin.swapCount == 2);
    CHECK(frontPixels[0] == 0xFF00FFFFu);
    CHECK(frontPixels[96 * 80 - 1] == 0xFF00FFFFu);

    /* Copy window surface to matching native pixmap via eglCopyBuffers */
    copyTargetPixels = (uint32_t *)calloc(96 * 80, sizeof(uint32_t));
    CHECK(copyTargetPixels != NULL);
    memset(&copyTarget, 0, sizeof(copyTarget));
    copyTarget.width = 96;
    copyTarget.height = 80;
    copyTarget.stride = 96 * 4;
    copyTarget.displayMode = EGL_SYMBIAN_ECOLOR16MA;
    copyTarget.data = copyTargetPixels;

    CHECK(eglCopyBuffers(dpy, winSurf, &copyTarget) == EGL_TRUE);
    CHECK(copyTargetPixels[0] == 0xFF00FFFFu);
    CHECK(copyTargetPixels[96 * 80 - 1] == 0xFF00FFFFu);

    /* Mismatched pixmap displayMode fails eglCopyBuffers with EGL_BAD_MATCH */
    copyTarget.displayMode = EGL_SYMBIAN_ECOLOR64K;
    CHECK(eglCopyBuffers(dpy, winSurf, &copyTarget) == EGL_FALSE);
    CHECK(eglGetError() == EGL_BAD_MATCH);

    /* Direct Pixmap Surface rendering */
    pixmapPixels = (uint32_t *)calloc(32 * 32, sizeof(uint32_t));
    CHECK(pixmapPixels != NULL);
    memset(&nativePix, 0, sizeof(nativePix));
    nativePix.width = 32;
    nativePix.height = 32;
    nativePix.stride = 32 * 4;
    nativePix.displayMode = EGL_SYMBIAN_ECOLOR16MA;
    nativePix.data = pixmapPixels;

    /* Creating pixmap surface with window config fails with EGL_BAD_MATCH */
    CHECK(eglCreatePixmapSurface(dpy, winCfg, &nativePix, NULL) == EGL_NO_SURFACE);
    CHECK(eglGetError() == EGL_BAD_MATCH);

    pixSurf = eglCreatePixmapSurface(dpy, pixCfg, &nativePix, NULL);
    CHECK(pixSurf != EGL_NO_SURFACE);
    /* Duplicate pixmap surface on same nativePixmap fails with EGL_BAD_ALLOC */
    CHECK(eglCreatePixmapSurface(dpy, pixCfg, &nativePix, NULL) == EGL_NO_SURFACE);
    CHECK(eglGetError() == EGL_BAD_ALLOC);

    pixCtx = eglCreateContext(dpy, pixCfg, EGL_NO_CONTEXT, NULL);
    CHECK(pixCtx != EGL_NO_CONTEXT);
    CHECK(eglMakeCurrent(dpy, pixSurf, pixSurf, pixCtx) == EGL_TRUE);

    /* Clear pixmap to yellow (R=255, G=255, B=0, A=255 -> BGRA 0xFFFFFF00) */
    glClearColor(1.0f, 1.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    CHECK(eglWaitGL() == EGL_TRUE);
    CHECK(pixmapPixels[0] == 0xFFFFFF00u);
    CHECK(pixmapPixels[32 * 32 - 1] == 0xFFFFFF00u);
    CHECK(nativePix.locked == 0);

    /* Leave pixCtx & pixSurf bound and test eglTerminate full teardown! */
    CHECK(eglTerminate(dpy) == EGL_TRUE);
    CHECK(eglGetCurrentContext() == EGL_NO_CONTEXT);
    CHECK(eglQueryString(dpy, EGL_VERSION) == NULL);
    CHECK(eglGetError() == EGL_NOT_INITIALIZED);

    free(frontPixels);
    free(copyTargetPixels);
    free(pixmapPixels);
    egliSetDefaultDisplayMode(EGL_SYMBIAN_ECOLOR64K);
}

static void test_bound_pbuffer_destruction(void)
{
    const EGLint cfgAttrs[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_BIND_TO_TEXTURE_RGBA, EGL_TRUE, EGL_NONE
    };
    const EGLint textureAttrs[] = {
        EGL_WIDTH, 4, EGL_HEIGHT, 4,
        EGL_TEXTURE_FORMAT, EGL_TEXTURE_RGBA,
        EGL_TEXTURE_TARGET, EGL_TEXTURE_2D, EGL_NONE
    };
    const EGLint drawAttrs[] = { EGL_WIDTH, 4, EGL_HEIGHT, 4, EGL_NONE };
    EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EGLConfig cfg;
    EGLint count;
    EGLSurface draw, image, replaced;
    EGLContext first, second, unrelated;
    GLuint name;
    eglPbufferSurface *pbuffer;

    CHECK(eglInitialize(dpy, NULL, NULL) == EGL_TRUE);
    CHECK(eglChooseConfig(dpy, cfgAttrs, &cfg, 1, &count) == EGL_TRUE && count == 1);
    first = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, NULL);
    second = eglCreateContext(dpy, cfg, first, NULL);
    draw = eglCreatePbufferSurface(dpy, cfg, drawAttrs);
    image = eglCreatePbufferSurface(dpy, cfg, textureAttrs);
    CHECK(first != EGL_NO_CONTEXT && second != EGL_NO_CONTEXT);
    CHECK(draw != EGL_NO_SURFACE && image != EGL_NO_SURFACE);
    CHECK(eglMakeCurrent(dpy, draw, draw, first) == EGL_TRUE);
    glGenTextures(1, &name);
    glBindTexture(GL_TEXTURE_2D, name);
    CHECK(eglReleaseTexImage(dpy, image, EGL_BACK_BUFFER) == EGL_FALSE);
    CHECK(eglGetError() == EGL_BAD_SURFACE);
    CHECK(eglReleaseTexImage(dpy, draw, EGL_BACK_BUFFER) == EGL_FALSE);
    CHECK(eglGetError() == EGL_BAD_SURFACE);
    CHECK(eglBindTexImage(dpy, image, EGL_BACK_BUFFER) == EGL_TRUE);
    CHECK(oglIsSurfaceTextureReleased(egliGetState()->currentESContext, name) == 0);

    /* Dropping a different context must not erase the only reference used to
     * detach this storage before it is freed. The original can leave a UAF. */
    unrelated = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, NULL);
    CHECK(unrelated != EGL_NO_CONTEXT);
    CHECK(eglDestroyContext(dpy, unrelated) == EGL_TRUE);
    pbuffer = (eglPbufferSurface *)eglMapSurface(eglMapDisplay(dpy), image);
    CHECK(pbuffer->boundESContext == egliGetState()->currentESContext);

    /* Destroying the image must detach its storage from the live texture. */
    CHECK(eglDestroySurface(dpy, image) == EGL_TRUE);
    CHECK(oglIsSurfaceTextureReleased(egliGetState()->currentESContext, name) == 1);

    image = eglCreatePbufferSurface(dpy, cfg, textureAttrs);
    CHECK(image != EGL_NO_SURFACE);
    CHECK(eglBindTexImage(dpy, image, EGL_BACK_BUFFER) == EGL_TRUE);
    replaced = image;
    image = eglCreatePbufferSurface(dpy, cfg, textureAttrs);
    CHECK(image != EGL_NO_SURFACE);
    CHECK(eglBindTexImage(dpy, image, EGL_BACK_BUFFER) == EGL_TRUE);
    pbuffer = (eglPbufferSurface *)eglMapSurface(eglMapDisplay(dpy), image);

    /* The original tracks the texture name, even after a new image replaces it. */
    CHECK(eglMakeCurrent(dpy, replaced, replaced, first) == EGL_FALSE);
    CHECK(eglGetError() == EGL_BAD_SURFACE);
    CHECK(eglDestroySurface(dpy, replaced) == EGL_TRUE);
    CHECK(oglIsSurfaceTextureReleased(egliGetState()->currentESContext, name) == 1);
    CHECK(eglBindTexImage(dpy, image, EGL_BACK_BUFFER) == EGL_FALSE);
    CHECK(eglGetError() == EGL_BAD_ACCESS);
    CHECK(eglReleaseTexImage(dpy, image, EGL_BACK_BUFFER) == EGL_TRUE);
    CHECK(eglReleaseTexImage(dpy, image, EGL_BACK_BUFFER) == EGL_FALSE);
    CHECK(eglGetError() == EGL_BAD_SURFACE);
    CHECK(eglBindTexImage(dpy, image, EGL_BACK_BUFFER) == EGL_TRUE);

    /* A current context dies on the later switch, not in DestroyContext. */
    CHECK(eglDestroyContext(dpy, first) == EGL_TRUE);
    CHECK(pbuffer->boundESContext == egliGetState()->currentESContext);
    CHECK(eglMakeCurrent(dpy, draw, draw, second) == EGL_TRUE);
    CHECK(pbuffer->boundESContext == NULL && pbuffer->textureName == 0);
    CHECK(oglIsSurfaceTextureReleased(egliGetState()->currentESContext, name) == 1);
    if (pbuffer->boundESContext == NULL)
    {
        CHECK(eglMakeCurrent(dpy, image, image, second) == EGL_TRUE);
        glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        CHECK(glGetError() == GL_NO_ERROR);
        CHECK(eglMakeCurrent(dpy, draw, draw, second) == EGL_TRUE);
        glBindTexture(GL_TEXTURE_2D, name);
        CHECK(eglBindTexImage(dpy, image, EGL_BACK_BUFFER) == EGL_TRUE);
    }
    CHECK(eglTerminate(dpy) == EGL_TRUE);
}

int main(void)
{
    test_preinit_and_lifecycle();
    test_configs();
    test_pbuffer_and_render_to_texture();
    test_window_and_pixmap_surfaces();
    test_bound_pbuffer_destruction();

    printf("test_egl: %d/%d checks passed\n", g_checks - g_fails, g_checks);
    return (g_fails == 0) ? 0 : 1;
}
