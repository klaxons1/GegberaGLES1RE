/* Host integration test: Nokia M3G (JSR-184) 1.1 core engine from
 * SymbianSource/oss.FCL.sf.os.graphics (m3g/m3gcore11) running on top of the
 * PixelFlinger GLES 1.1 + EGL implementation (pixelflinger/symbian/
 * egl_symbian.cpp, PF_HEADLESS_UNIX build).
 *
 * Exercises the EGL/GL paths m3gcore uses on Symbian:
 *   - EGL_MATCH_NATIVE_PIXMAP + EGL_SAMPLE_BUFFERS/EGL_SAMPLES config queries
 *     (m3gcore asserts eglGetError()==EGL_SUCCESS after eglChooseConfig)
 *   - eglBindAPI, eglQueryContext(EGL_CONFIG_ID) as EGLConfig
 *   - direct rendering to CFbsBitmap pixmap surfaces (EColor16MU/64K/16MA),
 *     preserving and re-importing native 2D content via eglWaitNative and
 *     publishing results via glFinish
 *   - buffered rendering through an RGBA8 pbuffer (Image2D targets)
 *   - eglBindEGLSurfaceTarget on an application pbuffer
 *   - textures (RGB/RGBA/paletted), lighting, fog, blending, depth
 * Run with tests/m3g/run_m3gcore_host.sh.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <EGL/egl.h>
#include <GLES/gl.h>

#include "M3G/m3g_core.h"
#include "../../symbian/pf_host_bitmap.h"

extern int g_m3gGlueLocks;
uintptr_t low4g_heap_high_water(void); /* highest heap address */

static int g_fail;
static int g_m3gErrors;
static M3Genum g_lastM3gError;

#define CHECK(cond, ...) do { if (!(cond)) { ++g_fail; \
    fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
    fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } while (0)

#define H(p) ((M3Guint)(uintptr_t)(p))

/* Like the JSR-184 Java peer layer, the test owns one reference to every
 * object it creates; m3gcore frees objects whose reference count drops to 0. */
static void* refObj(void* o) { if (o) m3gAddRef((M3GObject)o); return o; }
#define REF(x) ((__typeof__(x))refObj((void*)(x)))

static void* m3gMalloc(M3Guint bytes) { return malloc(bytes); }
static void  m3gFree(void* p) { free(p); }
static void  m3gError(M3Genum code, M3GInterface m3g)
{
    (void)m3g;
    ++g_m3gErrors;
    g_lastM3gError = code;
    fprintf(stderr, "  m3g error 0x%x\n", (unsigned)code);
}

static void checkPtr32(const void* p, const char* what)
{
    if ((uintptr_t)p > 0x7FFFFFFFu) {
        fprintf(stderr, "%s pointer %p does not fit an M3G 32-bit handle\n", what, p);
        exit(2);
    }
}

/* ---- PfHostBitmap helpers (Symbian channel order) ---- */

static PfHostBitmap* newBitmap(int w, int h, int mode)
{
    PfHostBitmap* b = (PfHostBitmap*)calloc(1, sizeof(PfHostBitmap));
    b->magic = PF_HOST_BITMAP_MAGIC;
    b->width = w; b->height = h; b->displayMode = mode;
    b->stride = pf_host_bitmap_stride(w, mode);
    b->data = (uint8_t*)calloc(1, (size_t)b->stride * h);
    checkPtr32(b, "bitmap");
    return b;
}

static void freeBitmap(PfHostBitmap* b) { free(b->data); free(b); }

/* Store/load as 0xAARRGGBB. */
static void bmpPut(PfHostBitmap* b, int x, int y, uint32_t argb)
{
    uint8_t* row = b->data + y * b->stride;
    uint32_t r = (argb >> 16) & 255, g = (argb >> 8) & 255, bl = argb & 255;
    switch (b->displayMode) {
    case PF_HOST_ECOLOR64K:
        ((uint16_t*)row)[x] = (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (bl >> 3));
        break;
    case PF_HOST_ECOLOR16MU:
        ((uint32_t*)row)[x] = 0xFF000000u | (argb & 0xFFFFFFu);
        break;
    default:
        ((uint32_t*)row)[x] = argb;
        break;
    }
}

static uint32_t bmpGet(const PfHostBitmap* b, int x, int y)
{
    const uint8_t* row = b->data + y * b->stride;
    if (b->displayMode == PF_HOST_ECOLOR64K) {
        uint32_t v = ((const uint16_t*)row)[x];
        uint32_t r = (v >> 11) & 31, g = (v >> 5) & 63, bl = v & 31;
        r = (r << 3) | (r >> 2); g = (g << 2) | (g >> 4); bl = (bl << 3) | (bl >> 2);
        return 0xFF000000u | (r << 16) | (g << 8) | bl;
    }
    return ((const uint32_t*)row)[x];
}

static void bmpFill(PfHostBitmap* b, int x0, int y0, int x1, int y1, uint32_t argb)
{
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x)
            bmpPut(b, x, y, argb);
}

static int near(uint32_t a, uint32_t b, int tol)
{
    for (int s = 0; s < 24; s += 8) {
        int d = (int)((a >> s) & 255) - (int)((b >> s) & 255);
        if (d < -tol || d > tol) return 0;
    }
    return 1;
}

/* ---- scene helpers ---- */

typedef struct {
    M3GInterface m3g;
    M3GRenderContext ctx;
    M3GCamera cam;
    M3GVertexBuffer quad;
    M3GIndexBuffer strip;
    M3GAppearance flatApp;
    M3GBackground bgClearAll;
    M3GBackground bgDepthOnly;
    M3GMatrix identity;
} Scene;

static M3GVertexBuffer makeQuad(M3GInterface m3g, short half, int withTex, int withNormals)
{
    const short pos[] = { -half, -half, 0,  half, -half, 0,  -half, half, 0,  half, half, 0 };
    const short tc[]  = { 0, 1,  1, 1,  0, 0,  1, 0 };
    const signed char nrm[] = { 0, 0, 127,  0, 0, 127,  0, 0, 127,  0, 0, 127 };
    M3GVertexBuffer vb = REF(m3gCreateVertexBuffer(m3g));
    M3GVertexArray va = REF(m3gCreateVertexArray(m3g, 4, 3, M3G_SHORT));
    m3gSetVertexArrayElements(va, 0, 4, 12, M3G_SHORT, pos);
    m3gSetVertexArray(vb, va, 1.0f / 100.0f, NULL, 0);
    if (withTex) {
        M3GVertexArray ta = REF(m3gCreateVertexArray(m3g, 4, 2, M3G_SHORT));
        m3gSetVertexArrayElements(ta, 0, 4, 8, M3G_SHORT, tc);
        m3gSetTexCoordArray(vb, 0, ta, 1.0f, NULL, 0);
    }
    if (withNormals) {
        M3GVertexArray na = REF(m3gCreateVertexArray(m3g, 4, 3, M3G_BYTE));
        m3gSetVertexArrayElements(na, 0, 4, 12, M3G_BYTE, nrm);
        m3gSetNormalArray(vb, na);
    }
    return vb;
}

static void sceneInit(Scene* s)
{
    M3Gparams p;
    memset(&p, 0, sizeof(p));
    p.mallocFunc = m3gMalloc;
    p.freeFunc = m3gFree;
    p.errorFunc = m3gError;
    s->m3g = m3gCreateInterface(&p);
    CHECK(s->m3g != NULL, "m3gCreateInterface");
    checkPtr32(s->m3g, "interface");
    s->ctx = m3gCreateContext(s->m3g);
    CHECK(s->ctx != NULL, "m3gCreateContext");

    m3gIdentityMatrix(&s->identity);
    s->cam = REF(m3gCreateCamera(s->m3g));
    m3gSetParallel(s->cam, 2.0f, 1.0f, -10.0f, 10.0f);

    s->quad = makeQuad(s->m3g, 50, 1, 1);   /* covers [-0.5,0.5]^2 */
    M3Gsizei len = 4;
    s->strip = REF(m3gCreateImplicitStripBuffer(s->m3g, 1, &len, 0));

    s->flatApp = REF(m3gCreateAppearance(s->m3g));
    M3GPolygonMode pm = REF(m3gCreatePolygonMode(s->m3g));
    m3gSetCulling(pm, M3G_CULL_NONE);
    m3gSetPolygonMode(s->flatApp, pm);

    s->bgClearAll = REF(m3gCreateBackground(s->m3g));
    m3gSetBgColor(s->bgClearAll, 0xFF0000FFu);          /* blue */
    s->bgDepthOnly = REF(m3gCreateBackground(s->m3g));
    m3gSetBgEnable(s->bgDepthOnly, M3G_SETGET_COLORCLEAR, M3G_FALSE);
}

static void beginFrame(Scene* s, int w, int h)
{
    m3gSetViewport(s->ctx, 0, 0, w, h);
    m3gSetParallel(s->cam, 2.0f, (float)w / (float)h, -10.0f, 10.0f);
    m3gSetCamera(s->ctx, s->cam, &s->identity);
}

static void drawQuad(Scene* s, M3GVertexBuffer vb, M3GAppearance app, uint32_t argb)
{
    m3gSetVertexDefaultColor(vb, argb);
    m3gRender(s->ctx, vb, s->strip, app, &s->identity, 1.0f, -1);
}

/* ---- tests ---- */

static void testBitmapTarget(Scene* s, int mode, const char* name)
{
    const int W = 64, H = 48;
    PfHostBitmap* bmp = newBitmap(W, H, mode);
    bmpFill(bmp, 0, 0, W, H, 0xFF00FF00u);            /* native 2D UI: green */
    int errs = g_m3gErrors, locks = g_m3gGlueLocks;

    /* frame 1: draw a red quad over native content (no color clear) */
    m3gBindBitmapTarget(s->ctx, H(bmp));
    beginFrame(s, W, H);
    m3gClear(s->ctx, s->bgDepthOnly);
    drawQuad(s, s->quad, s->flatApp, 0xFFFF0000u);
    m3gReleaseTarget(s->ctx);

    CHECK(near(bmpGet(bmp, W / 2, H / 2), 0xFFFF0000u, 8), "%s: center red, got %08x", name, bmpGet(bmp, W / 2, H / 2));
    CHECK(near(bmpGet(bmp, 2, 2), 0xFF00FF00u, 8), "%s: native green kept, got %08x", name, bmpGet(bmp, 2, 2));
    CHECK(near(bmpGet(bmp, W - 3, H - 3), 0xFF00FF00u, 8), "%s: native green kept (BR)", name);

    /* native 2D drawing between frames (MIDP Graphics on the same bitmap) */
    bmpFill(bmp, 0, 0, 8, 8, 0xFFFFFF00u);             /* yellow, top-left */
    bmpFill(bmp, W / 2 - 2, H / 2 - 2, W / 2 + 2, H / 2 + 2, 0xFF00FFFFu); /* cyan over quad */

    /* frame 2: draw a smaller white quad only (no color clear) */
    M3GVertexBuffer small = makeQuad(s->m3g, 10, 0, 0);
    m3gBindBitmapTarget(s->ctx, H(bmp));
    beginFrame(s, W, H);
    m3gClear(s->ctx, s->bgDepthOnly);
    drawQuad(s, small, s->flatApp, 0xFFFFFFFFu);
    /* a second render call in the same frame (m3gcore calls eglWaitNative
     * again) must not re-import stale native pixels over the white quad */
    {
        M3GMatrix right;
        m3gIdentityMatrix(&right);
        m3gTranslationMatrix(&right, -0.6f, 0.0f, 0.0f);
        m3gSetVertexDefaultColor(small, 0xFFFF00FFu);
        m3gRender(s->ctx, small, s->strip, s->flatApp, &right, 1.0f, -1);
    }
    m3gReleaseTarget(s->ctx);
    CHECK(near(bmpGet(bmp, W / 2 - 14, H / 2), 0xFFFF00FFu, 8), "%s: second render call, got %08x", name, bmpGet(bmp, W / 2 - 14, H / 2));

    CHECK(near(bmpGet(bmp, 3, 3), 0xFFFFFF00u, 8), "%s: native yellow (drawn between frames) kept, got %08x", name, bmpGet(bmp, 3, 3));
    CHECK(near(bmpGet(bmp, W / 2, H / 2), 0xFFFFFFFFu, 8), "%s: white quad, got %08x", name, bmpGet(bmp, W / 2, H / 2));
    CHECK(near(bmpGet(bmp, W / 2 + 8, H / 2), 0xFFFF0000u, 8), "%s: frame-1 red kept, got %08x", name, bmpGet(bmp, W / 2 + 8, H / 2));

    /* frame 3: full clear through the bitmap target */
    m3gBindBitmapTarget(s->ctx, H(bmp));
    beginFrame(s, W, H);
    m3gClear(s->ctx, s->bgClearAll);
    m3gReleaseTarget(s->ctx);
    CHECK(near(bmpGet(bmp, 3, 3), 0xFF0000FFu, 8), "%s: cleared to blue, got %08x", name, bmpGet(bmp, 3, 3));

    CHECK(g_m3gGlueLocks == locks, "%s: expected direct EGL pixmap rendering, but m3gcore used the buffered LockNativeBitmap path (%d locks)", name, g_m3gGlueLocks - locks);
    CHECK(g_m3gErrors == errs, "%s: m3g errors raised", name);
    CHECK(bmp->lockCount == 0, "%s: bitmap left locked", name);
    freeBitmap(bmp);
    printf("[m3g] bitmap target %-10s OK (direct pixmap surface)\n", name);
}

static uint32_t imgPixel(M3GImage img, int w, int x, int y, uint32_t* buf)
{
    m3gGetImageARGB(img, buf);
    return buf[y * w + x];
}

static void testImageTarget(Scene* s)
{
    const int W = 32, H = 32;
    int errs = g_m3gErrors;
    M3GImage img = REF(m3gCreateImage(s->m3g, M3G_RGB, W, H, M3G_RENDERING_TARGET));
    CHECK(img != NULL, "image target create");
    uint32_t* buf = (uint32_t*)malloc(W * H * 4);

    m3gBindImageTarget(s->ctx, img);
    beginFrame(s, W, H);
    m3gClear(s->ctx, s->bgClearAll);
    drawQuad(s, s->quad, s->flatApp, 0xFFFF8000u);
    m3gReleaseTarget(s->ctx);

    uint32_t c = imgPixel(img, W, W / 2, H / 2, buf), e = imgPixel(img, W, 1, 1, buf);
    CHECK(near(c, 0xFFFF8000u, 8), "image target center orange, got %08x", c);
    CHECK(near(e, 0xFF0000FFu, 8), "image target edge blue, got %08x", e);
    /* orientation: top row is image row 0 -> quad is symmetric, check a corner quad */
    CHECK(g_m3gErrors == errs, "image target errors");

    /* render-to-texture: use the rendered image as texture on the bitmap */
    M3GTexture tex = REF(m3gCreateTexture(s->m3g, img));
    m3gTextureSetBlending(tex, M3G_FUNC_REPLACE);
    m3gSetFiltering(tex, M3G_FILTER_BASE_LEVEL, M3G_FILTER_NEAREST);
    M3GAppearance app = REF(m3gCreateAppearance(s->m3g));
    m3gSetPolygonMode(app, m3gGetPolygonMode(s->flatApp));
    m3gSetTexture(app, 0, tex);
    PfHostBitmap* bmp = newBitmap(32, 32, PF_HOST_ECOLOR16MU);
    M3GVertexBuffer full = makeQuad(s->m3g, 100, 1, 0);
    m3gBindBitmapTarget(s->ctx, H(bmp));
    beginFrame(s, 32, 32);
    m3gClear(s->ctx, s->bgClearAll);
    drawQuad(s, full, app, 0xFFFFFFFFu);
    m3gReleaseTarget(s->ctx);
    CHECK(near(bmpGet(bmp, 16, 16), 0xFFFF8000u, 10), "RTT center, got %08x", bmpGet(bmp, 16, 16));
    CHECK(near(bmpGet(bmp, 1, 1), 0xFF0000FFu, 10), "RTT edge, got %08x", bmpGet(bmp, 1, 1));
    CHECK(g_m3gErrors == errs, "RTT errors");
    freeBitmap(bmp);
    free(buf);
    printf("[m3g] Image2D target (RGBA8 pbuffer + readback) + render-to-texture OK\n");
}

static void testEGLSurfaceTarget(Scene* s)
{
    const int W = 40, H = 40;
    EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EGLint attr[] = { EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
                      EGL_DEPTH_SIZE, 16, EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_NONE };
    EGLConfig cfg; EGLint n = 0;
    eglChooseConfig(dpy, attr, &cfg, 1, &n);
    CHECK(n == 1, "pbuffer config");
    EGLint pb[] = { EGL_WIDTH, W, EGL_HEIGHT, H, EGL_NONE };
    EGLSurface surf = eglCreatePbufferSurface(dpy, cfg, pb);
    CHECK(surf != EGL_NO_SURFACE, "pbuffer");
    checkPtr32(surf, "EGLSurface");
    int errs = g_m3gErrors;

    m3gBindEGLSurfaceTarget(s->ctx, H(surf));
    beginFrame(s, W, H);
    m3gClear(s->ctx, s->bgClearAll);
    drawQuad(s, s->quad, s->flatApp, 0xFF00FF00u);
    /* read back while m3gcore's context is still current on our surface */
    uint8_t px[4] = {0}, edge[4] = {0};
    glReadPixels(W / 2, H / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
    glReadPixels(1, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, edge);
    m3gReleaseTarget(s->ctx);
    CHECK(px[0] < 8 && px[1] > 247 && px[2] < 8, "EGL target center green, got %d,%d,%d", px[0], px[1], px[2]);
    CHECK(edge[0] < 8 && edge[1] < 8 && edge[2] > 247, "EGL target edge blue, got %d,%d,%d", edge[0], edge[1], edge[2]);
    CHECK(g_m3gErrors == errs, "EGL target errors");
    eglDestroySurface(dpy, surf);
    printf("[m3g] EGL surface target (application pbuffer) OK\n");
}

static void testTexturedLitFog(Scene* s)
{
    const int W = 64, H = 64;
    int errs = g_m3gErrors;
    /* 8x8 RGB checker: red / white */
    uint8_t rgb[8 * 8 * 3];
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x) {
            uint8_t* p = rgb + (y * 8 + x) * 3;
            int red = ((x >> 2) ^ (y >> 2)) & 1;
            p[0] = 255; p[1] = red ? 0 : 255; p[2] = red ? 0 : 255;
        }
    M3GImage img = REF(m3gCreateImage(s->m3g, M3G_RGB, 8, 8, 0));
    m3gSetImage(img, rgb);
    M3GTexture tex = REF(m3gCreateTexture(s->m3g, img));
    m3gTextureSetBlending(tex, M3G_FUNC_MODULATE);
    m3gSetFiltering(tex, M3G_FILTER_NEAREST, M3G_FILTER_NEAREST);
    m3gSetWrapping(tex, M3G_WRAP_REPEAT, M3G_WRAP_REPEAT);

    M3GMaterial mat = REF(m3gCreateMaterial(s->m3g));
    m3gSetColor(mat, M3G_DIFFUSE_BIT, 0xFFFFFFFFu);
    m3gSetColor(mat, M3G_AMBIENT_BIT, 0xFF000000u);

    M3GAppearance app = REF(m3gCreateAppearance(s->m3g));
    m3gSetPolygonMode(app, m3gGetPolygonMode(s->flatApp));
    m3gSetTexture(app, 0, tex);
    m3gSetMaterial(app, mat);

    M3GLight light = REF(m3gCreateLight(s->m3g));
    m3gSetLightMode(light, M3G_DIRECTIONAL);   /* points down -Z (towards the quad) */
    m3gSetIntensity(light, 1.0f);

    PfHostBitmap* bmp = newBitmap(W, H, PF_HOST_ECOLOR16MU);
    M3GVertexBuffer full = makeQuad(s->m3g, 100, 1, 1);
    m3gBindBitmapTarget(s->ctx, H(bmp));
    beginFrame(s, W, H);
    m3gClearLights(s->ctx);
    m3gAddLight(s->ctx, light, &s->identity);
    m3gClear(s->ctx, s->bgClearAll);
    drawQuad(s, full, app, 0xFFFFFFFFu);
    m3gReleaseTarget(s->ctx);

    uint32_t a = bmpGet(bmp, 4, 4), b = bmpGet(bmp, 4 + W / 2, 4);
    /* lit (N.L = 1) + modulate: one quadrant red, the neighbouring one white */
    CHECK((near(a, 0xFFFF0000u, 12) && near(b, 0xFFFFFFFFu, 12)) ||
          (near(a, 0xFFFFFFFFu, 12) && near(b, 0xFFFF0000u, 12)),
          "lit checker texture, got %08x %08x", a, b);

    /* light off -> ambient black only */
    m3gBindBitmapTarget(s->ctx, H(bmp));
    beginFrame(s, W, H);
    m3gClearLights(s->ctx);
    m3gClear(s->ctx, s->bgClearAll);
    drawQuad(s, full, app, 0xFFFFFFFFu);
    m3gReleaseTarget(s->ctx);
    CHECK(near(bmpGet(bmp, 4, 4), 0xFF000000u, 12), "unlit material black, got %08x", bmpGet(bmp, 4, 4));

    /* linear fog fully saturating to green (no lighting, no texture) */
    M3GFog fog = REF(m3gCreateFog(s->m3g));
    m3gSetFogMode(fog, M3G_LINEAR_FOG);
    m3gSetFogLinear(fog, -100.0f, -50.0f);   /* everything beyond far -> fog color */
    m3gSetFogColor(fog, 0x00FF00u);
    M3GAppearance fogApp = REF(m3gCreateAppearance(s->m3g));
    m3gSetPolygonMode(fogApp, m3gGetPolygonMode(s->flatApp));
    m3gSetFog(fogApp, fog);
    m3gBindBitmapTarget(s->ctx, H(bmp));
    beginFrame(s, W, H);
    m3gClear(s->ctx, s->bgClearAll);
    drawQuad(s, full, fogApp, 0xFFFF0000u);
    m3gReleaseTarget(s->ctx);
    CHECK(near(bmpGet(bmp, 32, 32), 0xFF00FF00u, 16), "linear fog -> green, got %08x", bmpGet(bmp, 32, 32));

    /* alpha blending 50% white over blue */
    M3GCompositingMode cm = REF(m3gCreateCompositingMode(s->m3g));
    m3gSetBlending(cm, M3G_ALPHA_BLEND);
    M3GAppearance blendApp = REF(m3gCreateAppearance(s->m3g));
    m3gSetPolygonMode(blendApp, m3gGetPolygonMode(s->flatApp));
    m3gSetCompositingMode(blendApp, cm);
    m3gBindBitmapTarget(s->ctx, H(bmp));
    beginFrame(s, W, H);
    m3gClear(s->ctx, s->bgClearAll);
    drawQuad(s, full, blendApp, 0x80FFFFFFu);
    m3gReleaseTarget(s->ctx);
    CHECK(near(bmpGet(bmp, 32, 32), 0xFF8080FFu, 10), "alpha blend, got %08x", bmpGet(bmp, 32, 32));

    /* depth test: far red quad drawn after near green quad must be hidden */
    M3GMatrix nearM, farM;
    m3gIdentityMatrix(&nearM); m3gIdentityMatrix(&farM);
    m3gTranslationMatrix(&nearM, 0, 0, 1.0f);
    m3gTranslationMatrix(&farM, 0, 0, -1.0f);
    m3gBindBitmapTarget(s->ctx, H(bmp));
    beginFrame(s, W, H);
    m3gClear(s->ctx, s->bgClearAll);
    m3gSetVertexDefaultColor(s->quad, 0xFF00FF00u);
    m3gRender(s->ctx, s->quad, s->strip, s->flatApp, &nearM, 1.0f, -1);
    m3gSetVertexDefaultColor(full, 0xFFFF0000u);
    m3gRender(s->ctx, full, s->strip, s->flatApp, &farM, 1.0f, -1);
    m3gReleaseTarget(s->ctx);
    CHECK(near(bmpGet(bmp, 32, 32), 0xFF00FF00u, 8), "depth test near quad, got %08x", bmpGet(bmp, 32, 32));
    CHECK(near(bmpGet(bmp, 2, 2), 0xFFFF0000u, 8), "depth test far quad, got %08x", bmpGet(bmp, 2, 2));

    CHECK(g_m3gErrors == errs, "textured/lit/fog errors");
    freeBitmap(bmp);
    printf("[m3g] texture modulate + directional light + linear fog + blend + depth OK\n");
}

static M3GImage solidImage(Scene* s, int fmt, int w, int h, uint32_t rgb)
{
    uint8_t* px = (uint8_t*)malloc(w * h * 4);
    int bpp = fmt == M3G_RGBA ? 4 : 3;
    for (int i = 0; i < w * h; ++i) {
        px[i * bpp] = (uint8_t)(rgb >> 16); px[i * bpp + 1] = (uint8_t)(rgb >> 8); px[i * bpp + 2] = (uint8_t)rgb;
        if (bpp == 4) px[i * 4 + 3] = 255;
    }
    M3GImage img = REF(m3gCreateImage(s->m3g, (M3GImageFormat)fmt, w, h, 0));
    m3gSetImage(img, px);
    free(px);
    return img;
}

static void testSpritesPaletteMultitex(Scene* s)
{
    const int W = 64, H = 64;
    int errs = g_m3gErrors;
    PfHostBitmap* bmp = newBitmap(W, H, PF_HOST_ECOLOR16MU);

    /* Background image (Background.setImage) */
    M3GBackground bg = REF(m3gCreateBackground(s->m3g));
    m3gSetBgImage(bg, solidImage(s, M3G_RGB, 16, 16, 0xFF00FFu));
    m3gBindBitmapTarget(s->ctx, H(bmp));
    beginFrame(s, W, H);
    m3gClear(s->ctx, bg);
    m3gReleaseTarget(s->ctx);
    CHECK(near(bmpGet(bmp, 32, 32), 0xFFFF00FFu, 8), "background image, got %08x", bmpGet(bmp, 32, 32));

    /* Paletted RGB image -> glCompressedTexImage2D(PALETTE8) or expansion,
     * trilinear (mipmapped) filtering with GL_GENERATE_MIPMAP */
    uint8_t idx[16 * 16];
    for (int i = 0; i < 16 * 16; ++i) idx[i] = 1;
    const uint8_t pal[] = { 0, 0, 0,  0, 200, 255 };
    M3GImage pimg = REF(m3gCreateImage(s->m3g, M3G_RGB, 16, 16, M3G_PALETTED));
    m3gSetImage(pimg, idx);
    m3gSetImagePalette(pimg, 2, pal);
    M3GTexture ptex = REF(m3gCreateTexture(s->m3g, pimg));
    m3gTextureSetBlending(ptex, M3G_FUNC_REPLACE);
    m3gSetFiltering(ptex, M3G_FILTER_LINEAR, M3G_FILTER_LINEAR);

    /* second unit: white RGBA texture modulated with a blend color */
    M3GTexture t1 = REF(m3gCreateTexture(s->m3g, solidImage(s, M3G_RGBA, 8, 8, 0x808080u)));
    m3gTextureSetBlending(t1, M3G_FUNC_MODULATE);

    M3GAppearance app = REF(m3gCreateAppearance(s->m3g));
    m3gSetPolygonMode(app, m3gGetPolygonMode(s->flatApp));
    m3gSetTexture(app, 0, ptex);
    M3GVertexBuffer vb = makeQuad(s->m3g, 100, 1, 0);
    m3gBindBitmapTarget(s->ctx, H(bmp));
    beginFrame(s, W, H);
    m3gClear(s->ctx, s->bgClearAll);
    drawQuad(s, vb, app, 0xFFFFFFFFu);
    m3gReleaseTarget(s->ctx);
    CHECK(near(bmpGet(bmp, 32, 32), 0xFF00C8FFu, 8), "paletted mipmapped texture, got %08x", bmpGet(bmp, 32, 32));

    /* multitexture: unit1 = 0x808080 modulate -> half intensity */
    M3GVertexArray ta = REF(m3gCreateVertexArray(s->m3g, 4, 2, M3G_SHORT));
    const short tc[] = { 0, 1, 1, 1, 0, 0, 1, 0 };
    m3gSetVertexArrayElements(ta, 0, 4, 8, M3G_SHORT, tc);
    m3gSetTexCoordArray(vb, 1, ta, 1.0f, NULL, 0);
    m3gSetTexture(app, 1, t1);
    m3gBindBitmapTarget(s->ctx, H(bmp));
    beginFrame(s, W, H);
    m3gClear(s->ctx, s->bgClearAll);
    drawQuad(s, vb, app, 0xFFFFFFFFu);
    m3gReleaseTarget(s->ctx);
    CHECK(near(bmpGet(bmp, 32, 32), 0xFF006480u, 10), "2-unit multitexture, got %08x", bmpGet(bmp, 32, 32));

    /* unscaled Sprite3D (glDrawTex-free path: textured quad, ortho) */
    M3GAppearance sapp = REF(m3gCreateAppearance(s->m3g));
    M3GSprite spr = REF(m3gCreateSprite(s->m3g, M3G_FALSE, solidImage(s, M3G_RGBA, 16, 16, 0xFFFF00u), sapp));
    m3gBindBitmapTarget(s->ctx, H(bmp));
    beginFrame(s, W, H);
    m3gClear(s->ctx, s->bgClearAll);
    m3gRenderNode(s->ctx, (M3GNode)spr, &s->identity);
    m3gReleaseTarget(s->ctx);
    CHECK(near(bmpGet(bmp, 32, 32), 0xFFFFFF00u, 8), "sprite center, got %08x", bmpGet(bmp, 32, 32));
    CHECK(near(bmpGet(bmp, 32 + 12, 32), 0xFF0000FFu, 8), "sprite is 16x16 px, got %08x", bmpGet(bmp, 44, 32));

    CHECK(g_m3gErrors == errs, "sprite/palette/multitexture errors");
    freeBitmap(bmp);
    printf("[m3g] background image + paletted mipmapped texture + 2 texture units + Sprite3D OK\n");
}

static void testWorld(Scene* s)
{
    /* scene graph path: World + Mesh + Background + active camera */
    const int W = 48, H = 48;
    int errs = g_m3gErrors;
    M3GWorld world = REF(m3gCreateWorld(s->m3g));
    M3GCamera cam = REF(m3gCreateCamera(s->m3g));
    m3gSetPerspective(cam, 60.0f, 1.0f, 0.1f, 100.0f);
    m3gSetTranslation((M3GTransformable)cam, 0, 0, 3.0f);
    M3GBackground bg = REF(m3gCreateBackground(s->m3g));
    m3gSetBgColor(bg, 0xFF202020u);
    M3GVertexBuffer vb = makeQuad(s->m3g, 100, 0, 0);
    m3gSetVertexDefaultColor(vb, 0xFFFFFF00u);
    M3GIndexBuffer ib = s->strip;
    M3GAppearance app = s->flatApp;
    M3GMesh mesh = REF(m3gCreateMesh(s->m3g, vb, &ib, &app, 1));
    m3gAddChild((M3GGroup)world, (M3GNode)cam);
    m3gAddChild((M3GGroup)world, (M3GNode)mesh);
    m3gSetActiveCamera(world, cam);
    m3gSetBackground(world, bg);

    PfHostBitmap* bmp = newBitmap(W, H, PF_HOST_ECOLOR64K);
    m3gBindBitmapTarget(s->ctx, H(bmp));
    m3gSetViewport(s->ctx, 0, 0, W, H);
    m3gRenderWorld(s->ctx, world);
    m3gReleaseTarget(s->ctx);
    CHECK(near(bmpGet(bmp, W / 2, H / 2), 0xFFFFFF00u, 8), "world mesh yellow, got %08x", bmpGet(bmp, W / 2, H / 2));
    CHECK(near(bmpGet(bmp, 1, 1), 0xFF202020u, 8), "world background, got %08x", bmpGet(bmp, 1, 1));
    CHECK(g_m3gErrors == errs, "world errors");
    freeBitmap(bmp);
    printf("[m3g] World/Mesh/Camera(perspective)/Background render OK\n");
}

static void testEglBehaviour(void)
{
    EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(dpy, NULL, NULL);   /* m3gcore initializes EGL the same way */
    /* m3gcore's exact config query for an EColor16MU bitmap target */
    PfHostBitmap* bmp = newBitmap(16, 16, PF_HOST_ECOLOR16MU);
    EGLint attr[] = { EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 0,
                      EGL_DEPTH_SIZE, 16, EGL_SURFACE_TYPE, EGL_PIXMAP_BIT,
                      EGL_MATCH_NATIVE_PIXMAP, (EGLint)H(bmp),
                      EGL_SAMPLE_BUFFERS, 1, EGL_SAMPLES, 4, EGL_NONE };
    EGLConfig cfg; EGLint n = -1;
    EGLBoolean ok = eglChooseConfig(dpy, attr, &cfg, 1, &n);
    CHECK(ok && n == 0 && eglGetError() == EGL_SUCCESS, "4x MSAA request: 0 configs without error (n=%d)", n);
    attr[15] = EGL_FALSE; attr[17] = 0;
    ok = eglChooseConfig(dpy, attr, &cfg, 1, &n);
    CHECK(ok && n == 1 && eglGetError() == EGL_SUCCESS, "MATCH_NATIVE_PIXMAP config (n=%d)", n);
    EGLint bs = 0;
    eglGetConfigAttrib(dpy, cfg, EGL_BUFFER_SIZE, &bs);
#ifndef PF_FRAMEBUFFER_16BPP
    CHECK(bs >= 24, "32-bit bitmap matched %d-bit config", bs);
#endif
    PfHostBitmap* notBmp = newBitmap(16, 16, 3 /* EGray256: unsupported mode */);
    attr[13] = (EGLint)H(notBmp);
    ok = eglChooseConfig(dpy, attr, &cfg, 1, &n);
    CHECK(!ok && eglGetError() == EGL_BAD_NATIVE_PIXMAP, "bad native pixmap rejected");
    CHECK(eglBindAPI(EGL_OPENGL_ES_API) && eglQueryAPI() == EGL_OPENGL_ES_API, "eglBindAPI(ES)");
    CHECK(!eglBindAPI(0x30A2 /* EGL_OPENVG_API */) && eglGetError() == EGL_BAD_PARAMETER, "eglBindAPI(VG) rejected");
    CHECK(eglGetProcAddress("eglBindAPI") != NULL, "eglGetProcAddress(eglBindAPI)");
    freeBitmap(notBmp);
    freeBitmap(bmp);
    printf("[m3g] EGL config/API queries used by m3gcore OK\n");
}

int main(void)
{
    Scene s;
    setvbuf(stdout, NULL, _IONBF, 0);
    memset(&s, 0, sizeof(s));
    sceneInit(&s);
    testEglBehaviour();
    testBitmapTarget(&s, PF_HOST_ECOLOR16MU, "EColor16MU");
    testBitmapTarget(&s, PF_HOST_ECOLOR64K, "EColor64K");
    testBitmapTarget(&s, PF_HOST_ECOLOR16MA, "EColor16MA");
    testImageTarget(&s);
    testEGLSurfaceTarget(&s);
    testTexturedLitFog(&s);
    testWorld(&s);
    testSpritesPaletteMultitex(&s);

    m3gDeleteObject((M3GObject)s.ctx);
    m3gDeleteInterface(s.m3g);
    CHECK(low4g_heap_high_water() <= 0x80000000u, "heap escaped the 31-bit handle range");
    printf("[m3g] heap top 0x%08lx (all M3G handles fit 31 bits)\n",
           (unsigned long)low4g_heap_high_water());
    if (g_fail) {
        printf("=== %d M3G CHECK(S) FAILED ===\n", g_fail);
        return 1;
    }
    printf("=== ALL M3G (m3gcore11) ON PIXELFLINGER TESTS PASSED ===\n");
    return 0;
}
