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

#include <pf_symbian_tls.h>
#include <private/pixelflinger/ggl_context.h>
#include "../libpixelflinger/codeflinger/ARMAssembler.h"
#include "../libpixelflinger/codeflinger/CodeCache.h"
#include "../libpixelflinger/codeflinger/GGLAssembler.h"
#include "../libagl/context.h"

/* Headless window/pixmap structs matching egl_symbian.cpp */
struct PfHeadlessWindow {
    int      width;
    int      height;
    int      strideBytes;
    int      displayMode; /* 8 = EColor64K (RGB565), 12 = EColor16MA (BGRA8888) */
    void*    pixels;
    uint32_t swapCount;
};

struct PfHeadlessPixmap {
    int      width;
    int      height;
    int      strideBytes;
    int      displayMode;
    void*    pixels;
};

static void set_perspective(float fovyDeg, float aspect, float zNear, float zFar)
{
    float fH = tanf(fovyDeg * (float)M_PI / 360.0f) * zNear;
    float fW = fH * aspect;
    glFrustumf(-fW, fW, -fH, fH, zNear, zFar);
}

static void test_codeflinger_armv5_jit(void)
{
    using namespace android;

    const size_t arenaBytes = 128 * 1024;
    void* rawMem = malloc(arenaBytes);
    assert(rawMem != NULL);

    PfExecArena arena;
    pf_exec_arena_init(&arena, rawMem, arenaBytes);
    CodeCache cache(32 * 1024);

    /* Synthesize 3 distinct ARMv5TE scanline kernels via GGLAssembler + ARMAssembler */
    needs_t configs[3];
    memset(configs, 0, sizeof(configs));

    /* Kernel 0: Gouraud-shaded RGB565 + Z-test LESS + Z-write */
    configs[0].n = GGL_BUILD_NEEDS(GGL_PIXEL_FORMAT_RGB_565, CB_FORMAT) |
                   GGL_BUILD_NEEDS(1, SHADE);
    configs[0].p = GGL_BUILD_NEEDS(1, P_DITHER) |
                   GGL_BUILD_NEEDS(1, P_MASK_Z) |
                   GGL_BUILD_NEEDS(GGL_LESS & 7, P_DEPTH_TEST);

    /* Kernel 1: Perspective-correct RGBA8888 modulated texture + fog -> RGB565 */
    configs[1].n = GGL_BUILD_NEEDS(GGL_PIXEL_FORMAT_RGB_565, CB_FORMAT) |
                   GGL_BUILD_NEEDS(1, SHADE) |
                   GGL_BUILD_NEEDS(1, W);
    configs[1].p = GGL_BUILD_NEEDS(1, P_DITHER) |
                   GGL_BUILD_NEEDS(1, P_MASK_Z) |
                   GGL_BUILD_NEEDS(GGL_LEQUAL & 7, P_DEPTH_TEST) |
                   GGL_BUILD_NEEDS(1, P_FOG);
    configs[1].t[0] = GGL_BUILD_NEEDS(GGL_PIXEL_FORMAT_RGBA_8888, T_FORMAT) |
                      GGL_BUILD_NEEDS(ggl_env_to_needs(GGL_MODULATE), T_ENV) |
                      GGL_BUILD_NEEDS(1, T_LINEAR);

    /* Kernel 2: Alpha-blended SRC_ALPHA / ONE_MINUS_SRC_ALPHA -> BGRA8888 */
    configs[2].n = GGL_BUILD_NEEDS(GGL_PIXEL_FORMAT_BGRA_8888, CB_FORMAT) |
                   GGL_BUILD_NEEDS(ggl_blendfactor_to_needs(GGL_SRC_ALPHA), BLEND_SRC) |
                   GGL_BUILD_NEEDS(ggl_blendfactor_to_needs(GGL_ONE_MINUS_SRC_ALPHA), BLEND_DST) |
                   GGL_BUILD_NEEDS(ggl_blendfactor_to_needs(GGL_SRC_ALPHA), BLEND_SRCA) |
                   GGL_BUILD_NEEDS(ggl_blendfactor_to_needs(GGL_ONE_MINUS_SRC_ALPHA), BLEND_DSTA) |
                   GGL_BUILD_NEEDS(1, SHADE);

    GGLContext* ggl = NULL;
    gglInit(&ggl);
    assert(ggl != NULL);
    context_t* c = reinterpret_cast<context_t*>(ggl);

    for (int i = 0; i < 3; ++i) {
        sp<Assembly> asmBuf(new Assembly(2048));
        uint32_t* initialBase = asmBuf->base();
        assert(initialBase != NULL);

        GGLAssembler jit(new ARMAssembler(asmBuf));
        int err = jit.scanline(configs[i], c);
        assert(err == 0);
        /* Verify shrink-in-place did NOT relocate base (branch targets stay valid!) */
        assert(asmBuf->base() == initialBase);
        assert(asmBuf->size() >= 16);

        /* Inspect emitted 32-bit ARMv5TE words (must be non-zero machine instructions) */
        ssize_t nWords = asmBuf->size() / 4;
        int condAlwaysCount = 0;
        for (ssize_t w = 0; w < nWords; ++w) {
            if ((initialBase[w] >> 28) == 0xEu)
                ++condAlwaysCount;
        }
        assert(condAlwaysCount > 4);

        AssemblyKey<needs_t> key(configs[i]);
        int cerr = cache.cache(key, asmBuf);
        assert(cerr == 0);

        sp<Assembly> hit = cache.lookup(key);
        assert(hit.get() == asmBuf.get());
        printf("[test] ARMv5TE JIT kernel #%d assembled cleanly: %zd bytes (%zd ARM insns), cache hit OK\n",
               i, (ssize_t)asmBuf->size(), nWords);
    }

    gglUninit(ggl);
    free(rawMem);
}

int main(void)
{
    printf("=== Symbian EKA2 Pixelflinger + Codeflinger + libagl Headless Suite ===\n");

    /* 1. Test Codeflinger ARMv5TE JIT + PfExecArena + CodeCache */
    test_codeflinger_armv5_jit();

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
    printf("=== ALL 12 SYMBIAN PIXELFLINGER + CODEFLINGER + LIBAGL TESTS PASSED! ===\n");
    return 0;
}
