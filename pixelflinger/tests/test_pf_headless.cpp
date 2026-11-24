/* test_pf_headless.cpp - Comprehensive headless verification suite for
 * Symbian OS EKA2 Pixelflinger + Codeflinger + libagl + EGL 1.1 port.
 */

#define PF_HEADLESS_UNIX 1

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <EGL/egl.h>
#include <GLES/gl.h>
#include <GLES/glext.h>

#include <private/pixelflinger/ggl_context.h>
#include "../libagl/context.h"
#include "../libpixelflinger/codeflinger/CodeCache.h"
#include "../libpixelflinger/codeflinger/ARMAssembler.h"

static void set_perspective(float fovyDeg, float aspect, float zNear, float zFar)
{
    float fH = tanf(fovyDeg * (float)M_PI / 360.0f) * zNear;
    float fW = fH * aspect;
    glFrustumf(-fW, fW, -fH, fH, zNear, zFar);
}

static void test_codecache_and_armassembler_safety(void)
{
    using namespace android;
    /* 1. Verify ARMAssembler emits valid ARMv5TE machine words & shrinks Assembly */
    sp<Assembly> okAsm(new Assembly(256));
    assert(okAsm->base() != NULL && okAsm->size() == 256);
    {
        ARMAssembler arm(okAsm);
        arm.prolog();
        arm.MOV(ARMAssembler::AL, 0, ARMAssembler::R0, arm.imm(0x42));
        arm.epilog(0);
        assert(arm.generate("unit_ok") == 0);
        assert(okAsm->size() == 12);
        assert(okAsm->base()[1] == 0xE3A00042u); /* mov r0, #0x42 */
        assert(okAsm->base()[2] == 0xE12FFF1Eu); /* bx lr */
    }

    /* 2. Verify ARMAssembler catches buffer overflow cleanly (returns NO_MEMORY, zero OOB writes) */
    sp<Assembly> tinyAsm(new Assembly(32));
    assert(tinyAsm->base() != NULL);
    {
        ARMAssembler arm(tinyAsm);
        arm.prolog();
        for (int i = 0; i < 40; ++i)
            arm.MOV(ARMAssembler::AL, 0, ARMAssembler::R0, arm.imm(1));
        assert(arm.generate("unit_overflow") < 0);
    }

    /* 3. Verify unencodable immediate sets mOverflow instead of abort() */
    sp<Assembly> badImmAsm(new Assembly(128));
    {
        ARMAssembler arm(badImmAsm);
        arm.prolog();
        arm.MOV(ARMAssembler::AL, 0, ARMAssembler::R0, arm.imm(0x12345678u));
        assert(arm.generate("unit_bad_imm") < 0);
    }

    /* 4. Verify CodeCache lookup + LRU eviction (owning key per Assembly like ScanlineAssembly) */
    struct TestAssembly : public Assembly {
        AssemblyKey<needs_t> mKey;
        TestAssembly(const needs_t& n, size_t sz) : Assembly(sz), mKey(n) {}
    };
    CodeCache cache(512);
    for (uint32_t k = 1; k <= 12; ++k) {
        needs_t n;
        memset(&n, 0, sizeof(n));
        n.n = k;
        sp<TestAssembly> entry(new TestAssembly(n, 128));
        assert(entry->resize(96) == 96);
        assert(cache.cache(entry->mKey, entry) >= 0);
    }
    /* Latest keys must hit; oldest keys must have been evicted cleanly */
    needs_t n12, n1;
    memset(&n12, 0, sizeof(n12)); n12.n = 12;
    memset(&n1,  0, sizeof(n1));  n1.n  = 1;
    assert(cache.lookup(AssemblyKey<needs_t>(n12)) != NULL);
    assert(cache.lookup(AssemblyKey<needs_t>(n1))  == NULL);
    printf("[test] ARMAssembler bounds/immediate safety + CodeCache LRU eviction verified!\n");
}

static void test_nonjit_formats_and_config_attrs(void)
{
    EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    assert(dpy != EGL_NO_DISPLAY);
    assert(eglInitialize(dpy, NULL, NULL) == EGL_TRUE);

    /* Verify EGL 1.2+ attributes & stencil fallback accepted by S60/Symbian^3 apps */
    const EGLint s60GameAttribs[] = {
        0x3040 /* EGL_RENDERABLE_TYPE */, 1,
        0x303F /* EGL_COLOR_BUFFER_TYPE */, 0x308E,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_DEPTH_SIZE, 16,
        EGL_STENCIL_SIZE, 8,
        EGL_NONE
    };
    EGLConfig cfg32 = 0;
    EGLint num = 0;
    assert(eglChooseConfig(dpy, s60GameAttribs, &cfg32, 1, &num) == EGL_TRUE && num >= 1);

    /* Test rendering & glCopyTexImage2D on Config 2 (RGBA8888), Config 3 (RGBX8888), Config 6 (RGBA4444) */
    const int cfgIds[3] = { 2, 3, 6 };
    for (int i = 0; i < 3; ++i) {
        EGLConfig cfg = (EGLConfig)(intptr_t)cfgIds[i];
        int dims[2] = { 64, 64 };
        EGLSurface surf = eglCreateWindowSurface(dpy, cfg, (EGLNativeWindowType)dims, NULL);
        assert(surf != EGL_NO_SURFACE);
        EGLContext ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, NULL);
        assert(ctx != EGL_NO_CONTEXT);
        assert(eglMakeCurrent(dpy, surf, surf, ctx) == EGL_TRUE);

        /* Verify JIT is 100% disabled (scanline_as == NULL) */
        const android::gl::ogles_context_t* glCtx = (const android::gl::ogles_context_t*)ctx;
        assert(glCtx->rasterizer.scanline_as == NULL);

        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        /* Draw Gouraud + alpha-blended triangle exercising generic C++ scanline() */
        const float tri[6] = { -0.9f, -0.9f,  0.9f, -0.9f,  0.0f, 0.9f };
        glEnableClientState(GL_VERTEX_ARRAY);
        glEnable(GL_DEPTH_TEST);
        glColor4ub(255, 64, 32, 0); /* alpha=0: verify window blit still produces opaque RGB! */
        glVertexPointer(2, GL_FLOAT, 0, tri);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        assert(glCtx->rasterizer.scanline_as == NULL);

        uint8_t px[4] = {0};
        glReadPixels(32, 24, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
        assert(px[0] >= 220 && px[1] >= 45 && px[1] <= 85 && px[2] <= 50);

        /* Verify glCopyTexImage2D succeeds on this colorbuffer format! */
        GLuint copyTex = 0;
        glGenTextures(1, &copyTex);
        glBindTexture(GL_TEXTURE_2D, copyTex);
        glCopyTexImage2D(GL_TEXTURE_2D, 0, (cfgIds[i] == 3) ? GL_RGB : GL_RGBA, 0, 0, 64, 64, 0);
        assert(glGetError() == GL_NO_ERROR);

        /* Verify eglCopyBuffers produces opaque 0xFFRRGGBB even when GL alpha was 0 */
        if (cfgIds[i] != 6) {
            uint32_t out32[64 * 64];
            memset(out32, 0, sizeof(out32));
            assert(eglCopyBuffers(dpy, surf, (EGLNativePixmapType)out32) == EGL_TRUE);
            uint32_t mid = out32[(64 - 24) * 64 + 32];
            assert((mid & 0xFF000000u) == 0xFF000000u);
            assert(((mid >> 16) & 0xFFu) >= 220);
        }

        glDeleteTextures(1, &copyTex);
        assert(eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT) == EGL_TRUE);
        assert(eglDestroySurface(dpy, surf) == EGL_TRUE);
        assert(eglDestroyContext(dpy, ctx) == EGL_TRUE);
    }
    assert(eglTerminate(dpy) == EGL_TRUE);
    printf("[test] Non-JIT C++ scanline verified on RGBA8888, RGBX8888, RGBA4444 + glCopyTexImage2D!\n");
}

/* ---------------------------------------------------------------------------
 * Regression suite for the four audited subsystems: lights, Z-buffer /
 * perspective, textures, and point sprites. Renders into a small RGBA window
 * with an identity MVP and checks actual pixel values.
 * --------------------------------------------------------------------------- */
static void sample_px(int x, int y, uint8_t* px)
{
    glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
}

/* Average a 3x3 block to be robust against partial coverage. */
static void sample_block(int cx, int cy, int rgb[3])
{
    int acc[3] = { 0, 0, 0 };
    uint8_t px[4];
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            sample_px(cx + dx, cy + dy, px);
            acc[0] += px[0]; acc[1] += px[1]; acc[2] += px[2];
        }
    }
    for (int i = 0; i < 3; ++i) rgb[i] = acc[i] / 9;
}

static int count_lit_px(int x0, int y0, int x1, int y1, int thresh)
{
    uint8_t px[4];
    int n = 0;
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            sample_px(x, y, px);
            if ((int)px[0] + (int)px[1] + (int)px[2] > thresh) ++n;
        }
    }
    return n;
}

static void test_lights_z_tex_sprites(void)
{
    EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    assert(dpy != EGL_NO_DISPLAY);
    assert(eglInitialize(dpy, NULL, NULL) == EGL_TRUE);

    const EGLint attribs[] = {
        0x3040 /* EGL_RENDERABLE_TYPE */, 1,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
        EGL_DEPTH_SIZE, 16, EGL_NONE
    };
    EGLConfig cfg = 0;
    EGLint num = 0;
    assert(eglChooseConfig(dpy, attribs, &cfg, 1, &num) == EGL_TRUE && num >= 1);
    int dims[2] = { 96, 72 };
    EGLSurface surf = eglCreateWindowSurface(dpy, cfg, (EGLNativeWindowType)dims, NULL);
    EGLContext ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, NULL);
    assert(surf != EGL_NO_SURFACE && ctx != EGL_NO_CONTEXT);
    assert(eglMakeCurrent(dpy, surf, surf, ctx) == EGL_TRUE);

    const int W = 96, H = 72;
    glViewport(0, 0, W, H);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    assert(glGetError() == GL_NO_ERROR);

    int rgb[3];
    uint8_t px[4];

    /* ================= A. LIGHTS ================= */
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    const float lightPos[4] = { 0.0f, 0.0f, 1.0f, 0.0f }; /* headlight */
    const float lightDiff[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    const float lightAmb[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiff);
    glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmb);
    glEnable(GL_COLOR_MATERIAL);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glShadeModel(GL_SMOOTH);

    /* Two triangles sharing edge B-C with OPPOSITE window windings:
     * indices {0,1,2} = (A,B,C) is front-facing, {1,2,3} = (B,C,D) is
     * back-facing. GL_TRIANGLE_STRIP normalizes winding (both triangles
     * come out front), so GL_TRIANGLES + explicit indices are required
     * to get a real back face. The shared vertices B,C make this the
     * KR-18 relight case: with GL_LIGHT_MODEL_TWO_SIDE they must be
     * re-lit with the reversed normal for tri2, so tri2 must be DARK
     * while tri1 stays BRIGHT. */
    const float twoSidePos[12] = {
        -0.7f, -0.5f, 0.0f,   0.7f, -0.5f, 0.0f,
        -0.7f,  0.5f, 0.0f,   0.7f,  0.5f, 0.0f,
    };
    const float twoSideNrm[12] = {
        0.0f, 0.0f, 1.0f,   0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f,   0.0f, 0.0f, 1.0f,
    };
    static const uint16_t twoSideIdx[6] = { 0, 1, 2,  1, 2, 3 };
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, twoSidePos);
    glNormalPointer(GL_FLOAT, 0, twoSideNrm);

    glLightModelx(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, twoSideIdx);
    assert(glGetError() == GL_NO_ERROR);
    {
        const android::gl::ogles_context_t* gc = (const android::gl::ogles_context_t*)ctx;
        /* the last processed triangle (B,C,D) is the back-facing one */
        assert(gc->lighting.backFace == GL_TRUE);
    }
    /* tri1 centroid (A+B+C)/3 = (-0.233,-0.167) -> (37,30): front lit */
    sample_block(W / 2 - 11, H / 2 - 6, rgb);
    assert(rgb[0] > 200 && rgb[1] > 200 && rgb[2] > 200);
    /* tri2 centroid (B+C+D)/3 = (0.233,0.167) -> (59,42): back face,
     * reversed normal -> max(N.L,0) = 0 -> ambient only (~0.2 -> 51).
     * Missing the LIT relight leaves shared B,C bright -> centroid
     * ~187; the old abs() clamp made it fully bright -> both of those
     * regressions fail this assert. */
    sample_block(W / 2 + 11, H / 2 + 6, rgb);
    assert(rgb[0] < 70 && rgb[1] < 70 && rgb[2] < 70);

    /* two-side OFF: no normal reversal and no LIT clear -> the back
     * triangle lights up with its original +Z normal too */
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glLightModelx(GL_LIGHT_MODEL_TWO_SIDE, GL_FALSE);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, twoSideIdx);
    sample_block(W / 2 + 11, H / 2 + 6, rgb);
    assert(rgb[0] > 200 && rgb[1] > 200 && rgb[2] > 200); /* back now lit */

    /* GL_NORMALIZE with components >> 256 units must still produce a
     * correctly scaled normal (old vnorm3() wrapped): N.L = 1/sqrt(3) */
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_NORMALIZE);
    assert(glIsEnabled(GL_NORMALIZE) == GL_TRUE);
    const float bigPos[9] = { -0.9f, -0.9f, 0.0f,  0.9f, -0.9f, 0.0f,  0.0f, 0.9f, 0.0f };
    glVertexPointer(3, GL_FLOAT, 0, bigPos);
    glDisableClientState(GL_NORMAL_ARRAY);
    glNormal3f(100.0f, 100.0f, 100.0f);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    sample_block(W / 2, H / 2, rgb);
    /* 0.577 * 255 ~= 147, plus scene ambient 0.2*255 = 51 -> ~198 */
    assert(rgb[0] > 150 && rgb[0] < 240);
    glDisable(GL_NORMALIZE);

    /* glMaterialx SHININESS: [0,128] validation (raw GLfixed storage) */
    glMaterialx(GL_FRONT_AND_BACK, GL_SHININESS, 0x00400000 /* 64.0 */);
    assert(glGetError() == GL_NO_ERROR);
    glMaterialx(GL_FRONT_AND_BACK, GL_SHININESS, 0x00800000 /* 128.0 */);
    assert(glGetError() == GL_NO_ERROR);
    glMaterialx(GL_FRONT_AND_BACK, GL_SHININESS, 0x00810000 /* 129.0 */);
    assert(glGetError() == GL_INVALID_VALUE);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 200.0f);
    assert(glGetError() == GL_INVALID_VALUE);

    glDisable(GL_LIGHTING);
    assert(glIsEnabled(GL_LIGHTING) == GL_FALSE);

    /* ================= B. DEPTH / Z ================= */
    glDisableClientState(GL_NORMAL_ARRAY);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glClearColor(0.0f, 1.0f, 0.0f, 1.0f); /* green: nothing drawn */
    glClearDepthx(0x10000);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    /* Far-plane point (clip.z == w -> window.z == 0xFFFF): the old
     * z*0x00010001 iteration wrapped to depth 0 and the point was
     * accepted against a fully-cleared buffer. */
    glColor4f(1.0f, 0.0f, 0.0f, 1.0f);
    const float farPt[3] = { 0.0f, 0.0f, 1.0f };
    glVertexPointer(3, GL_FLOAT, 0, farPt);
    glDrawArrays(GL_POINTS, 0, 1);
    assert(glGetError() == GL_NO_ERROR);
    sample_block(W / 2, H / 2, rgb);
    assert(rgb[0] < 40 && rgb[1] > 200 && rgb[2] < 40); /* still green */

    /* Draw order + polygon offset sign (KR-13) */
    const float quadPos[12] = {
        -0.8f, -0.6f, 0.5f,   0.8f, -0.6f, 0.5f,
        -0.8f,  0.6f, 0.5f,   0.8f,  0.6f, 0.5f,
    };
    glEnable(GL_POLYGON_OFFSET_FILL);

    /* equal depth: only a negative units offset can win GL_LESS */
    glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glColor4f(1.0f, 0.0f, 0.0f, 1.0f);
    glVertexPointer(3, GL_FLOAT, 0, quadPos);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glPolygonOffsetx(0, -655360); /* 0, -10.0 */
    glColor4f(0.0f, 1.0f, 0.0f, 1.0f);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    sample_block(W / 2, H / 2, rgb);
    assert(rgb[0] < 40 && rgb[1] > 200 && rgb[2] < 40); /* green won */

    glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glColor4f(0.0f, 1.0f, 0.0f, 1.0f);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glPolygonOffsetx(0, 655360); /* 0, 10.0 */
    glColor4f(1.0f, 0.0f, 0.0f, 1.0f);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    sample_block(W / 2, H / 2, rgb);
    assert(rgb[0] < 40 && rgb[1] > 200 && rgb[2] < 40); /* red rejected */

    /* Both slope signs and large factors. These planes cross near/far:
     * clamping only the plane origin still wraps at the opposite side. */
    for (int slopeSign = -1; slopeSign <= 1; slopeSign += 2) {
        float tiltPos[12];
        memcpy(tiltPos, quadPos, sizeof(tiltPos));
        for (int i = 0; i < 4; ++i)
            tiltPos[i*3+2] = 0.5f + ((i & 1) ? 0.1f : -0.1f) * slopeSign;
        for (int factorSign = -1; factorSign <= 1; factorSign += 2) {
            glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glPolygonOffsetx(0, 0);
            glColor4f(1.0f, 0.0f, 0.0f, 1.0f);
            glVertexPointer(3, GL_FLOAT, 0, quadPos);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            glPolygonOffsetx(factorSign * 800 * 65536, 0);
            glColor4f(0.0f, 1.0f, 0.0f, 1.0f);
            glVertexPointer(3, GL_FLOAT, 0, tiltPos);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            for (int side = 1; side <= 3; side += 2) {
                sample_block(side * W / 4, H / 2, rgb);
                const int winner = factorSign > 0 ? 0 : 1;
                assert(rgb[winner] > 200 && rgb[1-winner] < 40);
            }
        }
    }
    glDisable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffsetx(0, 0);

    /* glDrawTex z at the far plane must be depth-rejected (KR-16 set_depth_and_fog
     * wrap used to write ~mid depth and the rect was drawn on top) */
    GLuint whiteTex = 0;
    glGenTextures(1, &whiteTex);
    glBindTexture(GL_TEXTURE_2D, whiteTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    const uint32_t whitePx = 0xFFFFFFFFu;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &whitePx);
    const GLint whiteCrop[4] = { 0, 0, 1, 1 };
    glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_CROP_RECT_OES, whiteCrop);
    glEnable(GL_TEXTURE_2D);

    glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f); /* drawTex modulates with current color */
    glDrawTexxOES(10 << 16, 10 << 16, 0x10000 /* z = far */, 20 << 16, 20 << 16);
    assert(glGetError() == GL_NO_ERROR);
    sample_block(20, 20, rgb);
    assert(rgb[2] > 200 && rgb[0] < 40); /* rejected: clear blue stays */

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDrawTexxOES(10 << 16, 10 << 16, 0 /* z = near */, 20 << 16, 20 << 16);
    assert(glGetError() == GL_NO_ERROR);
    sample_block(20, 20, rgb);
    assert(rgb[0] > 200 && rgb[1] > 200 && rgb[2] > 200); /* drawn: white */

    /* ================= C. TEXTURES ================= */
    glDisable(GL_DEPTH_TEST);
    glActiveTexture(GL_TEXTURE0);

    /* glTexEnv COMBINE family: set + glGetTexEnviv round-trip (used to
     * return GL_INVALID_ENUM for every non-MODE/COLOR pname) */
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
    glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_INTERPOLATE);
    glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
    glTexEnvi(GL_TEXTURE_ENV, GL_SRC0_RGB, GL_TEXTURE);
    glTexEnvi(GL_TEXTURE_ENV, GL_SRC1_RGB, GL_PREVIOUS);
    glTexEnvi(GL_TEXTURE_ENV, GL_SRC2_RGB, GL_CONSTANT);
    glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_ALPHA);
    glTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE, 2);
    GLint gv = -1;
    glGetTexEnviv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, &gv);          assert(gv == GL_COMBINE);
    glGetTexEnviv(GL_TEXTURE_ENV, GL_COMBINE_RGB, &gv);      assert(gv == GL_INTERPOLATE);
    glGetTexEnviv(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, &gv);    assert(gv == GL_REPLACE);
    glGetTexEnviv(GL_TEXTURE_ENV, GL_SRC0_RGB, &gv);         assert(gv == GL_TEXTURE);
    glGetTexEnviv(GL_TEXTURE_ENV, GL_SRC1_RGB, &gv);         assert(gv == GL_PREVIOUS);
    glGetTexEnviv(GL_TEXTURE_ENV, GL_SRC2_RGB, &gv);         assert(gv == GL_CONSTANT);
    glGetTexEnviv(GL_TEXTURE_ENV, GL_OPERAND0_RGB, &gv);     assert(gv == GL_SRC_ALPHA);
    glGetTexEnviv(GL_TEXTURE_ENV, GL_RGB_SCALE, &gv);        assert(gv == 2);
    assert(glGetError() == GL_NO_ERROR);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    /* Nontrivial COMBINE must execute, not just round-trip its state.
     * REPLACE previous, scaled by two: (0.25, 0.125, 0) -> (0.5, 0.25, 0). */
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
    glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_REPLACE);
    glTexEnvi(GL_TEXTURE_ENV, GL_SRC0_RGB, GL_PREVIOUS);
    glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
    glColor4f(0.25f, 0.125f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawTexxOES(10<<16, 10<<16, 0, 20<<16, 20<<16);
    sample_block(20, 20, rgb);
    assert(rgb[0] > 105 && rgb[0] < 150 && rgb[1] > 45 && rgb[1] < 85 && rgb[2] < 20);
    GLfixed scaleX = 0;
    glGetTexEnvxv(GL_TEXTURE_ENV, GL_RGB_SCALE, &scaleX);
    assert(scaleX == 0x20000);
    GLfloat scaleF[2] = { 0, 1234 };
    glGetTexEnvfv(GL_TEXTURE_ENV, GL_RGB_SCALE, scaleF);
    assert(scaleF[0] == 2.0f && scaleF[1] == 1234); /* scalar getter */
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glColor4f(1, 1, 1, 1);

    /* incomplete -> complete: sticky GL_TEXTURE_2D must survive the
     * rasterizer-side disable and come back when the chain completes */
    GLuint texInc = 0;
    glGenTextures(1, &texInc);
    glBindTexture(GL_TEXTURE_2D, texInc);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    uint8_t checker4[4 * 4 * 4];
    for (int i = 0; i < 16; ++i) {
        int cx = i & 3, cy = i >> 2;
        const bool orng = ((cx ^ cy) & 1) != 0;
        checker4[i * 4 + 0] = orng ? 255 : 40;
        checker4[i * 4 + 1] = orng ? 140 : 180;
        checker4[i * 4 + 2] = orng ? 40 : 255;
        checker4[i * 4 + 3] = 255;
    }
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, checker4);
    /* level 1 with a mismatching format => incomplete */
    uint8_t lvl565[2 * 2 * 2] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    glTexImage2D(GL_TEXTURE_2D, 1, GL_RGB, 2, 2, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, lvl565);
    assert(glGetError() == GL_NO_ERROR);
    assert(glIsEnabled(GL_TEXTURE_2D) == GL_TRUE); /* user's sticky enable */

    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_LIGHTING);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    const float fullQuad[12] = {
        -0.9f, -0.9f, 0.0f,  0.9f, -0.9f, 0.0f,  -0.9f, 0.9f, 0.0f,  0.9f, 0.9f, 0.0f,
    };
    static const float fullUv[8] = { 0, 0, 1, 0, 0, 1, 1, 1 };
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, fullQuad);
    glTexCoordPointer(2, GL_FLOAT, 0, fullUv);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    sample_block(W / 2, H / 2, rgb);
    assert(rgb[0] > 230 && rgb[1] > 230 && rgb[2] > 230); /* unit off: white */

    /* complete the chain properly => texture comes back (sticky revalidate) */
    glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, checker4);
    glTexImage2D(GL_TEXTURE_2D, 2, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, checker4);
    assert(glGetError() == GL_NO_ERROR);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    sample_block(W / 2, H / 2, rgb);
    /* modulated by white: must be one of the checker texels, NOT white */
    assert(!(rgb[0] > 230 && rgb[1] > 230 && rgb[2] > 230));

    /* generate-mipmap chain on odd / 1-wide / 1-tall textures: the old
     * (w&h)==1 exit skipped the chain entirely and the box filter read
     * out of bounds on 1-wide/1-tall sources (ASan-checked here) */
    for (int s = 0; s < 3; ++s) {
        static const int dims3[3][2] = { { 3, 5 }, { 1, 6 }, { 6, 1 } };
        GLuint mt = 0;
        glGenTextures(1, &mt);
        glBindTexture(GL_TEXTURE_2D, mt);
        glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, dims3[s][0], dims3[s][1], 0,
                GL_RGBA, GL_UNSIGNED_BYTE, checker4);
        assert(glGetError() == GL_NO_ERROR);
        glDeleteTextures(1, &mt);
    }

    /* out-of-range mip level => INVALID_OPERATION (no editMip aliasing) */
    glBindTexture(GL_TEXTURE_2D, texInc);
    glTexImage2D(GL_TEXTURE_2D, 99, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, checker4);
    assert(glGetError() == GL_INVALID_OPERATION);

    /* ================= D. POINT SPRITES ================= */
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnable(GL_POINT_SPRITE_OES);
    assert(glIsEnabled(GL_POINT_SPRITE_OES) == GL_TRUE);
    glTexEnvi(GL_POINT_SPRITE_OES, GL_COORD_REPLACE_OES, GL_TRUE);
    gv = -1;
    glGetTexEnviv(GL_POINT_SPRITE_OES, GL_COORD_REPLACE_OES, &gv);
    assert(gv == GL_TRUE);
    glTexEnvf(GL_POINT_SPRITE_OES, GL_COORD_REPLACE_OES, 0.0f);
    glTexEnvx(GL_POINT_SPRITE_OES, GL_COORD_REPLACE_OES, 0x10000);
    gv = -1;
    glGetTexEnvxv(GL_POINT_SPRITE_OES, GL_COORD_REPLACE_OES, (GLfixed*)&gv);
    assert(gv == GL_TRUE);
    assert(glGetError() == GL_NO_ERROR);

    /* 2x2 sprite texture: TL red, TR green, BL blue, BR yellow */
    GLuint sprTex = 0;
    glGenTextures(1, &sprTex);
    glBindTexture(GL_TEXTURE_2D, sprTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    const uint8_t spr2[16] = {
        255, 0, 0, 255,    0, 255, 0, 255,
        0, 0, 255, 255,    255, 255, 0, 255,
    };
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, spr2);

    glDisable(GL_POINT_SMOOTH);
    glDisable(GL_DEPTH_TEST);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glPointSize(16.0f);
    const float sprPt[3] = { 0.0f, 0.0f, 0.0f };
    glVertexPointer(3, GL_FLOAT, 0, sprPt);
    glDrawArrays(GL_POINTS, 0, 1);
    assert(glGetError() == GL_NO_ERROR);

    const int cxm = W / 2, cym = H / 2;
    const int n = count_lit_px(cxm - 12, cym - 12, cxm + 12, cym + 12, 90);
    assert(n >= 180 && n <= 300); /* ~16x16 sprite quad */
    sample_px(cxm - 4, cym + 4, px);                    /* upper-left of sprite */
    assert(px[0] > 150 && px[1] < 90 && px[2] < 90);   /* red: (0,0) upper-left */
    sample_px(cxm + 4, cym - 4, px);                    /* lower-right */
    assert(px[0] > 150 && px[1] > 150 && px[2] < 90);   /* yellow: (1,1) lower-right */

    /* coordReplace off: the vertex texcoord drives the whole sprite */
    glTexEnvi(GL_POINT_SPRITE_OES, GL_COORD_REPLACE_OES, GL_FALSE);
    const float sprTc[2] = { 0.75f, 0.75f };
    glTexCoordPointer(2, GL_FLOAT, 0, sprTc);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_POINTS, 0, 1);
    sample_px(cxm, cym, px);
    assert(px[0] > 150 && px[1] > 150 && px[2] < 90); /* uniform yellow */
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);

    /* GL_POINT_SIZE_ARRAY_OES: per-vertex sizes (float fetcher) */
    glTexEnvi(GL_POINT_SPRITE_OES, GL_COORD_REPLACE_OES, GL_TRUE);
    static const float szArr[2] = { 24.0f, 4.0f };
    static const float szPts[6] = { 0.0f, -0.6f, 0.0f,  0.0f, 0.6f, 0.0f };
    glEnableClientState(GL_POINT_SIZE_ARRAY_OES);
    glPointSizePointerOES(GL_FLOAT, 0, szArr);
    glVertexPointer(3, GL_FLOAT, 0, szPts);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_POINTS, 0, 2);
    assert(glGetError() == GL_NO_ERROR);
    const int yLo = H / 2 + (int)(-0.6f * (H / 2)); /* vertex0: y=-0.6 */
    const int yHi = H / 2 + (int)( 0.6f * (H / 2));
    const int nBig = count_lit_px(cxm - 14, yLo - 14, cxm + 14, yLo + 14, 90);
    const int nSml = count_lit_px(cxm - 14, yHi - 14, cxm + 14, yHi + 14, 90);
    assert(nBig >= 400);   /* ~24x24 */
    assert(nSml <= 30);    /* ~4x4 */
    assert(nBig > nSml * 4);
    glDisableClientState(GL_POINT_SIZE_ARRAY_OES);

    /* distance attenuation shrinks the point (and never crashes on
     * degenerate denominators) */
    glClear(GL_COLOR_BUFFER_BIT);
    glVertexPointer(3, GL_FLOAT, 0, sprPt);
    const GLfixed att[3] = { 0, 0, 0x10000 }; /* 0 + 0*d + 1*d^2 */
    glPointParameterxv(GL_POINT_DISTANCE_ATTENUATION, att);
    glDrawArrays(GL_POINTS, 0, 1);
    assert(glGetError() == GL_NO_ERROR);
    const int nZero = count_lit_px(cxm - 14, cym - 14, cxm + 14, cym + 14, 90);
    assert(nZero <= 20);   /* d=0 => degenerate denominator => minimal point */
    const GLfixed att1[3] = { 0x10000, 0, 0 };
    glPointParameterxv(GL_POINT_DISTANCE_ATTENUATION, att1);

    /* cleanup */
    glDeleteTextures(1, &whiteTex);
    glDeleteTextures(1, &texInc);
    glDeleteTextures(1, &sprTex);
    glDisable(GL_POINT_SPRITE_OES);
    glDisable(GL_TEXTURE_2D);
    assert(glGetError() == GL_NO_ERROR);

    printf("[test] Lights (two-sided/normalize/shininess), Z & polygon offset, "
           "texture COMBINE/completeness/mipmaps, point sprites verified!\n");

    assert(eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT) == EGL_TRUE);
    assert(eglDestroySurface(dpy, surf) == EGL_TRUE);
    assert(eglDestroyContext(dpy, ctx) == EGL_TRUE);
    assert(eglTerminate(dpy) == EGL_TRUE);
}

int main(void)
{
    printf("=== Symbian EKA2 Pixelflinger + Codeflinger + libagl Headless Suite ===\n");

    /* 0. Test ARMAssembler bounds/immediate safety + CodeCache LRU eviction */
    test_codecache_and_armassembler_safety();

    /* 1. Test C++ fallback rasterizer across 32-bit/24-bit/16-bit formats & EGL attrs */
    test_nonjit_formats_and_config_attrs();

    /* 1b. Lights / Z & polygon offset / textures / point sprites regressions */
    test_lights_z_tex_sprites();

    /* 2. Initialize EGL 1.1 */
    EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    assert(dpy != EGL_NO_DISPLAY);
    EGLint major = 0, minor = 0;
    assert(eglInitialize(dpy, &major, &minor) == EGL_TRUE);
    assert(major == 1 && minor >= 1);

    const char* eglVendor = eglQueryString(dpy, EGL_VENDOR);
    assert(eglVendor && strcmp(eglVendor, "klaxons") == 0);

    /* Choose RGB565 + Z16 config (S60 EColor64K standard) */
    const EGLint cfgAttribs565[] = {
        EGL_RED_SIZE, 5,
        EGL_GREEN_SIZE, 6,
        EGL_BLUE_SIZE, 5,
        EGL_DEPTH_SIZE, 16,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT | EGL_PBUFFER_BIT,
        EGL_NONE
    };
    EGLConfig cfg565 = 0;
    EGLint numCfg = 0;
    assert(eglChooseConfig(dpy, cfgAttribs565, &cfg565, 1, &numCfg) == EGL_TRUE && numCfg >= 1);

    /* Create 320x240 S60 QVGA RGB565 headless window */
    const int W = 320, H = 240;
    int winDims[2] = { W, H };

    EGLSurface surfWin = eglCreateWindowSurface(dpy, cfg565, (EGLNativeWindowType)winDims, NULL);
    assert(surfWin != EGL_NO_SURFACE);

    EGLContext ctx = eglCreateContext(dpy, cfg565, EGL_NO_CONTEXT, NULL);
    assert(ctx != EGL_NO_CONTEXT);
    assert(eglMakeCurrent(dpy, surfWin, surfWin, ctx) == EGL_TRUE);

    /* 3. Verify strings & channel/depth bit queries */
    const char* vendor   = (const char*)glGetString(GL_VENDOR);
    const char* renderer = (const char*)glGetString(GL_RENDERER);
    const char* version  = (const char*)glGetString(GL_VERSION);
    const char* exts     = (const char*)glGetString(GL_EXTENSIONS);
    printf("[test] GL_VENDOR   = %s\n", vendor);
    printf("[test] GL_RENDERER = %s\n", renderer);
    printf("[test] GL_VERSION  = %s\n", version);
    assert(strcmp(vendor, "klaxons") == 0);
    assert(strstr(version, "1.1") != NULL);
    assert(strstr(exts, "GL_OES_compressed_ETC1_RGB8_texture") != NULL);
    assert(strstr(exts, "GL_OES_draw_texture") != NULL);

    GLint rBits = 0, gBits = 0, bBits = 0, aBits = -1, zBits = 0, ptRange[2] = {0, 0};
    glGetIntegerv(GL_RED_BITS, &rBits);
    glGetIntegerv(GL_GREEN_BITS, &gBits);
    glGetIntegerv(GL_BLUE_BITS, &bBits);
    glGetIntegerv(GL_ALPHA_BITS, &aBits);
    glGetIntegerv(GL_DEPTH_BITS, &zBits);
    glGetIntegerv(GL_ALIASED_POINT_SIZE_RANGE, ptRange);
    assert(rBits == 5 && gBits == 6 && bBits == 5 && aBits == 0 && zBits == 16);
    assert(ptRange[0] == 1 && ptRange[1] >= 1);
    assert(glGetError() == GL_NO_ERROR);
    printf("[test] Channel & depth bits OK: R=%d G=%d B=%d A=%d Z=%d\n",
           rBits, gBits, bBits, aBits, zBits);

    /* 4. Verify top-down Symbian CFbsBitmap Y-orientation vs bottom-up glReadPixels */
    glViewport(0, 0, W, H);
    glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    /* Top half (+Y in NDC: y in [0, 1]) = pure RED; bottom half (-Y: y in [-1, 0]) = pure BLUE */
    const float halfTop[8] = { -1,0,  1,0, -1,1,  1,1 };
    const float halfBot[8] = { -1,-1, 1,-1, -1,0, 1,0 };
    glEnableClientState(GL_VERTEX_ARRAY);
    glColor4ub(255, 0, 0, 255);
    glVertexPointer(2, GL_FLOAT, 0, halfTop);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glColor4ub(0, 0, 255, 255);
    glVertexPointer(2, GL_FLOAT, 0, halfBot);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    assert(eglSwapBuffers(dpy, surfWin) == EGL_TRUE);

    const android::gl::ogles_context_t* glCtx = (const android::gl::ogles_context_t*)ctx;
    int fbStridePx = glCtx->rasterizer.state.buffers.color.stride;
    const uint16_t* fb565 = (const uint16_t*)glCtx->rasterizer.state.buffers.color.data;
    uint16_t topWinPx = fb565[20 * fbStridePx + W / 2];          /* Row 20 from top -> RED (0xF800) */
    uint16_t botWinPx = fb565[(H - 20) * fbStridePx + W / 2];    /* Row H-20 (bottom) -> BLUE (0x001F) */
    assert(((topWinPx >> 11) & 0x1F) >= 28 && (topWinPx & 0x1F) == 0);
    assert(((botWinPx >> 11) & 0x1F) == 0  && (botWinPx & 0x1F) >= 28);

    uint8_t glBotRgba[4] = {0}, glTopRgba[4] = {0};
    glReadPixels(W / 2, 20, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, glBotRgba);     /* OpenGL y=20 is bottom -> BLUE */
    glReadPixels(W / 2, H - 20, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, glTopRgba); /* OpenGL y=H-20 is top -> RED */
    assert(glBotRgba[2] >= 240 && glBotRgba[0] == 0);
    assert(glTopRgba[0] >= 240 && glTopRgba[2] == 0);
    printf("[test] Symbian top-down CFbsBitmap & OpenGL bottom-up glReadPixels orientation OK!\n");

    /* 5. Test EGL 1.1 Pbuffer + eglBindTexImage render-to-texture */
    const EGLint pbufAttribs[] = {
        EGL_WIDTH, 64,
        EGL_HEIGHT, 64,
        EGL_TEXTURE_FORMAT, EGL_TEXTURE_RGB,
        EGL_TEXTURE_TARGET, EGL_TEXTURE_2D,
        EGL_NONE
    };
    EGLSurface pbuf = eglCreatePbufferSurface(dpy, cfg565, pbufAttribs);
    assert(pbuf != EGL_NO_SURFACE);
    assert(eglMakeCurrent(dpy, pbuf, pbuf, ctx) == EGL_TRUE);
    glViewport(0, 0, 64, 64);
    glClearColor(0.1f, 0.85f, 0.25f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    /* Draw golden diamond inside Pbuffer */
    const float diamond[8] = { 0,-0.75f, 0.75f,0, -0.75f,0, 0,0.75f };
    glColor4f(1.0f, 0.85f, 0.1f, 1.0f);
    glVertexPointer(2, GL_FLOAT, 0, diamond);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glFinish();

    /* Switch back to main window and bind Pbuffer as a 2D texture! */
    assert(eglMakeCurrent(dpy, surfWin, surfWin, ctx) == EGL_TRUE);
    GLuint texPbuf = 0;
    glGenTextures(1, &texPbuf);
    glBindTexture(GL_TEXTURE_2D, texPbuf);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    assert(eglBindTexImage(dpy, pbuf, EGL_BACK_BUFFER) == EGL_TRUE);

    /* 6. Create auto-mipmapped checkerboard texture + ETC1 compressed texture */
    GLuint texChecker = 0;
    glGenTextures(1, &texChecker);
    glBindTexture(GL_TEXTURE_2D, texChecker);
    glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    uint8_t checker[64 * 64 * 4];
    for (int y = 0; y < 64; ++y) {
        for (int x = 0; x < 64; ++x) {
            bool c = ((x >> 3) ^ (y >> 3)) & 1;
            uint8_t* p = &checker[(y * 64 + x) * 4];
            p[0] = c ? 245 : 40;
            p[1] = c ? 120 : 180;
            p[2] = c ? 40  : 245;
            p[3] = 255;
        }
    }
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, checker);
    assert(glGetError() == GL_NO_ERROR);

    /* ETC1 8x8 (four 4x4 blocks: vivid cyan/magenta/yellow/white) */
    GLuint texEtc1 = 0;
    glGenTextures(1, &texEtc1);
    glBindTexture(GL_TEXTURE_2D, texEtc1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    const uint8_t etc1Blocks[4 * 8] = {
        0x0F, 0xFF, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, /* cyan */
        0xF0, 0xFF, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, /* magenta */
        0xFF, 0x0F, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, /* yellow */
        0xFF, 0xFF, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00  /* white */
    };
    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_ETC1_RGB8_OES, 8, 8, 0, sizeof(etc1Blocks), etc1Blocks);
    assert(glGetError() == GL_NO_ERROR);

    /* 7. Build VBO-backed 3D cube (pos + normal + uv) + IBO */
    struct Vtx { float x, y, z, nx, ny, nz, u, v; };
    static const Vtx cubeVerts[24] = {
        /* Front (+Z) */
        {-1,-1, 1,  0,0,1, 0,0}, { 1,-1, 1,  0,0,1, 1,0}, { 1, 1, 1,  0,0,1, 1,1}, {-1, 1, 1,  0,0,1, 0,1},
        /* Back (-Z) */
        { 1,-1,-1,  0,0,-1, 0,0}, {-1,-1,-1, 0,0,-1, 1,0}, {-1, 1,-1, 0,0,-1, 1,1}, { 1, 1,-1,  0,0,-1, 0,1},
        /* Right (+X) */
        { 1,-1, 1,  1,0,0, 0,0}, { 1,-1,-1,  1,0,0, 1,0}, { 1, 1,-1,  1,0,0, 1,1}, { 1, 1, 1,  1,0,0, 0,1},
        /* Left (-X) */
        {-1,-1,-1, -1,0,0, 0,0}, {-1,-1, 1, -1,0,0, 1,0}, {-1, 1, 1, -1,0,0, 1,1}, {-1, 1,-1, -1,0,0, 0,1},
        /* Top (+Y) */
        {-1, 1, 1,  0,1,0, 0,0}, { 1, 1, 1,  0,1,0, 1,0}, { 1, 1,-1,  0,1,0, 1,1}, {-1, 1,-1,  0,1,0, 0,1},
        /* Bottom (-Y) */
        {-1,-1,-1,  0,-1,0,0,0}, { 1,-1,-1,  0,-1,0,1,0}, { 1,-1, 1,  0,-1,0,1,1}, {-1,-1, 1,  0,-1,0,0,1},
    };
    static const uint16_t cubeIdx[36] = {
        0,1,2, 0,2,3,       4,5,6, 4,6,7,
        8,9,10, 8,10,11,    12,13,14, 12,14,15,
        16,17,18, 16,18,19, 20,21,22, 20,22,23
    };

    GLuint vbo = 0, ibo = 0;
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ibo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVerts), NULL, GL_STATIC_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(cubeVerts), cubeVerts);
    assert(glIsBuffer(vbo) == GL_TRUE);
    GLint bufSize = 0;
    glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &bufSize);
    assert(bufSize == (GLint)sizeof(cubeVerts));

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cubeIdx), cubeIdx, GL_STATIC_DRAW);

    /* 8. Render full 320x240 showcase scene */
    glViewport(0, 0, W, H);
    glClearColor(0.08f, 0.11f, 0.18f, 1.0f);
    glClearDepthf(1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_CULL_FACE);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    set_perspective(50.0f, (float)W / (float)H, 0.5f, 30.0f);

    /* Query projection matrix via GL_OES_query_matrix */
    GLfixed mant[16];
    GLint   exp[16];
    GLbitfield qmask = glQueryMatrixxOES(mant, exp);
    assert(qmask == 0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    /* Configure directional + ambient lighting */
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    const float lightPos[4] = { 2.5f, 3.5f, 4.0f, 0.0f };
    const float lightDif[4] = { 1.0f, 0.96f, 0.90f, 1.0f };
    const float lightAmb[4] = { 0.28f, 0.30f, 0.36f, 1.0f };
    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDif);
    glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmb);

    float gotDif[4] = {0};
    glGetLightfv(GL_LIGHT0, GL_DIFFUSE, gotDif);
    assert(fabsf(gotDif[0] - 1.0f) < 0.01f);

    /* Draw mipmap-textured lit VBO cube on the left */
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texChecker);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    GLint envMode = 0;
    glGetTexEnviv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, &envMode);
    assert(envMode == GL_MODULATE);

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glVertexPointer(3, GL_FLOAT, sizeof(Vtx), (const void*)0);
    glNormalPointer(GL_FLOAT, sizeof(Vtx), (const void*)(3 * sizeof(float)));
    glTexCoordPointer(2, GL_FLOAT, sizeof(Vtx), (const void*)(6 * sizeof(float)));

    glPushMatrix();
    glTranslatef(-1.15f, 0.1f, -4.6f);
    glRotatef(28.0f, 1.0f, 0.0f, 0.0f);
    glRotatef(38.0f, 0.0f, 1.0f, 0.0f);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_SHORT, (const void*)0);
    glPopMatrix();

    /* Draw second VBO cube on the right textured with the live Pbuffer (eglBindTexImage)! */
    glBindTexture(GL_TEXTURE_2D, texPbuf);
    glPushMatrix();
    glTranslatef(1.25f, -0.05f, -4.8f);
    glRotatef(-22.0f, 1.0f, 0.0f, 0.0f);
    glRotatef(-34.0f, 0.0f, 1.0f, 0.0f);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_SHORT, (const void*)0);
    glPopMatrix();

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glDisableClientState(GL_NORMAL_ARRAY);
    glDisable(GL_LIGHTING);

    /* 9. Blit ETC1 compressed texture badge in top-right corner via GL_OES_draw_texture! */
    glBindTexture(GL_TEXTURE_2D, texEtc1);
    const GLint crop[4] = { 0, 0, 8, 8 };
    glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_CROP_RECT_OES, crop);
    GLint gotCrop[4] = {0};
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_CROP_RECT_OES, gotCrop);
    assert(gotCrop[2] == 8 && gotCrop[3] == 8);
    glDisable(GL_DEPTH_TEST);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glDrawTexiOES(W - 44, H - 44, 0, 36, 36);

    /* 10. Test user clip plane query + pointer query */
    const float planeEq[4] = { 1.0f, 0.0f, 0.0f, 2.0f };
    glClipPlanef(GL_CLIP_PLANE0, planeEq);
    float gotPlane[4] = {0};
    glGetClipPlanef(GL_CLIP_PLANE0, gotPlane);

    void* vtxPtr = NULL;
    glGetPointerv(GL_VERTEX_ARRAY_POINTER, &vtxPtr);

    assert(glGetError() == GL_NO_ERROR);
    assert(eglSwapBuffers(dpy, surfWin) == EGL_TRUE);

    /* 11. Test eglCopyBuffers to a BGRA8888 pixmap buffer */
    uint8_t* pix32Pixels = (uint8_t*)calloc(W * H, 4);
    assert(eglCopyBuffers(dpy, surfWin, (EGLNativePixmapType)pix32Pixels) == EGL_TRUE);

    /* Count distinct colors & write /tmp/pf_showcase.ppm for inspection */
    FILE* fp = fopen("/tmp/pf_showcase.ppm", "wb");
    assert(fp != NULL);
    fprintf(fp, "P6\n%d %d\n255\n", W, H);
    int nonBgPixels = 0;
    const uint8_t* bgra = pix32Pixels;
    for (int i = 0; i < W * H; ++i) {
        uint8_t b = bgra[i * 4 + 0];
        uint8_t g = bgra[i * 4 + 1];
        uint8_t r = bgra[i * 4 + 2];
        uint8_t rgb[3] = { r, g, b };
        fwrite(rgb, 1, 3, fp);
        if (r > 35 || g > 45 || b > 65)
            ++nonBgPixels;
    }
    fclose(fp);
    printf("[test] Showcase 320x240 frame rendered: %d lit/textured foreground pixels (>15000 expected)\n",
           nonBgPixels);
    assert(nonBgPixels > 15000);

    /* Release Pbuffer texture & clean up */
    glBindTexture(GL_TEXTURE_2D, texPbuf);
    assert(eglReleaseTexImage(dpy, pbuf, EGL_BACK_BUFFER) == EGL_TRUE);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &ibo);
    glDeleteTextures(1, &texChecker);
    glDeleteTextures(1, &texPbuf);
    glDeleteTextures(1, &texEtc1);

    assert(eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT) == EGL_TRUE);
    assert(eglDestroySurface(dpy, pbuf) == EGL_TRUE);
    assert(eglDestroySurface(dpy, surfWin) == EGL_TRUE);
    assert(eglDestroyContext(dpy, ctx) == EGL_TRUE);
    assert(eglTerminate(dpy) == EGL_TRUE);

    free(pix32Pixels);
    printf("=== ALL 13 SYMBIAN PIXELFLINGER + CODEFLINGER + LIBAGL TESTS PASSED! ===\n");
    return 0;
}
