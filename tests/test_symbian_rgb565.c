/* RGB565 framebuffer build (-DGERBERA_RGB565, GERBERA_FRAMEBUFFER_BPP=16):
 * window surfaces always render into EColor64K back buffers, window configs
 * are RGB565 only, and R8G8B8(A8) window requests get the RGB565 config.
 * Runs the real Symbian EGL platform layer and eglChooseConfig on the host. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "symbian_os.h"
#include "gerbera/egl/eglInternal.h"
#define __SYMBIAN32__ 1
#ifndef GERBERA_RGB565
#define GERBERA_RGB565 1
#endif
#include "../src/gerbera/egl/eglSymbian.c"
#include "../src/gerbera/egl/eglUtil.c"
#include "../src/gerbera/egl/eglBase.c"

#include "test_support.h"

static void *tls;
static uintptr_t environment[32];
static void *coe;
static int heapToken;
static int screenMode = EGL_SYMBIAN_ECOLOR16MU;

void sym_RArrayBase_Ctor(SymRArrayBase *array, int size)
{
    memset(array, 0, sizeof(*array));
    array->entrySize = size;
    array->entries = malloc(EGL_NUM_BUFFER_FORMATS * (size_t)size);
}
int sym_RArrayBase_Append(SymRArrayBase *array, const void *entry)
{
    memcpy((char *)array->entries + array->count * array->entrySize,
           entry, (size_t)array->entrySize);
    ++array->count;
    return 0;
}
void sym_RArrayBase_HeapSort(SymRArrayBase *array, int (*compare)(const void *, const void *))
{
    qsort(array->entries, (size_t)array->count, (size_t)array->entrySize, compare);
}
void *sym_RArrayBase_At(const SymRArrayBase *array, int index)
{
    return (char *)array->entries + index * array->entrySize;
}
void sym_RArrayBase_Close(SymRArrayBase *array) { free(array->entries); }

void *sym_DllTls(void) { return tls; }
int sym_DllSetTls(void *p) { tls = p; return 0; }
void *sym_CCoeEnv_Static(void) { return coe; }
void *sym_User_Alloc(int size) { return malloc((size_t)size); }
void sym_User_Free(void *p) { free(p); }
void *sym_User_AllocZL(int size)
{
    (void)size;
    memset(environment, 0, sizeof(environment));
    return environment;
}
void *sym_CCoeEnv_Ctor(void *p) { coe = p; return p; }
void sym_CCoeEnv_ConstructTrapped(void *p) { SYM_CCOEENV_SCREENDEV(p) = &heapToken; }
int sym_HAL_Get(int attr, int *value) { (void)attr; *value = 32 * 1024 * 1024; return 0; }
void *sym_UserHeap_ChunkHeap(const void *name, int minLen, int maxLen,
                            int growBy, int align, int singleThread)
{
    (void)name; (void)minLen; (void)maxLen; (void)growBy; (void)align; (void)singleThread;
    return &heapToken;
}
void sym_RAllocator_Close(void *heap) { (void)heap; }
int sym_RWsSession_GetDefModeMaxNumColors(const void *session, int *c, int *g)
{
    (void)session;
    *c = 1 << 24; *g = 0;
    return screenMode;
}

/* Surface/context teardown is never reached: no surfaces or contexts exist. */
void oglDestroyContext(oglContext_s *context) { (void)context; abort(); }
void oglReleaseSurfaceTexture(oglContext_s *context, unsigned int name, int level)
{ (void)context; (void)name; (void)level; abort(); }
int oglIsSurfaceTextureReleased(oglContext_s *context, unsigned int name)
{ (void)context; (void)name; abort(); }
void sym_RWindowTreeNode_Close(void *win) { (void)win; abort(); }
void sym_CFbsBitmap_Reset(void *bmp) { (void)bmp; abort(); }

static int attrib(EGLConfig config, EGLint name)
{
    EGLint value = -12345;
    CHECK(eglGetConfigAttrib((EGLDisplay)1, config, name, &value) == EGL_TRUE);
    return value;
}

static int choose(const EGLint *attribs, EGLConfig *configs)
{
    EGLint count = -1;
    CHECK(eglChooseConfig((EGLDisplay)1, attribs, configs,
                          EGL_NUM_BUFFER_FORMATS, &count) == EGL_TRUE);
    return count;
}

static void check_all_rgb565(const EGLConfig *configs, int count, int minDepth)
{
    int i;
    for (i = 0; i < count; ++i)
    {
        CHECK(attrib(configs[i], EGL_RED_SIZE) == 5);
        CHECK(attrib(configs[i], EGL_GREEN_SIZE) == 6);
        CHECK(attrib(configs[i], EGL_BLUE_SIZE) == 5);
        CHECK(attrib(configs[i], EGL_ALPHA_SIZE) == 0);
        CHECK(attrib(configs[i], EGL_BUFFER_SIZE) == 16);
        CHECK((attrib(configs[i], EGL_SURFACE_TYPE) & EGL_WINDOW_BIT) != 0);
        CHECK(attrib(configs[i], EGL_DEPTH_SIZE) >= minDepth);
    }
}

int main(void)
{
    static const int screens[] = {
        EGL_SYMBIAN_ECOLOR16MU, EGL_SYMBIAN_ECOLOR16MA,
        EGL_SYMBIAN_ECOLOR64K, EGL_SYMBIAN_ECOLOR4K
    };
    EGLConfig configs[EGL_NUM_BUFFER_FORMATS];
    eglConfig filter;
    int i, n, s;

    for (s = 0; s < (int)(sizeof(screens) / sizeof(screens[0])); ++s)
    {
        int windows = 0, pixmaps32 = 0;
        screenMode = screens[s];
        tls = coe = NULL;
        CHECK(eglInitialize((EGLDisplay)1, NULL, NULL) == EGL_TRUE);
        /* Back buffers are EColor64K whatever the screen mode is. */
        CHECK(egliGetState()->displayMode == EGL_SYMBIAN_ECOLOR64K);

        for (i = 1; i <= EGL_NUM_BUFFER_FORMATS; ++i)
        {
            eglConfig cfg;
            eglFillConfig(&cfg, i);
            if ((cfg.surfaceType & EGL_WINDOW_BIT) != 0)
            {
                ++windows;
                CHECK(cfg.redBits == 5 && cfg.greenBits == 6 &&
                      cfg.blueBits == 5 && cfg.alphaBits == 0);
            }
            /* Native 32-bit pixmaps (EColor16MU/MA bitmaps) still work. */
            if ((cfg.surfaceType & EGL_PIXMAP_BIT) != 0 && cfg.redBits == 8)
                ++pixmaps32;
        }
        CHECK(windows == EGL_NUM_DEPTH_FORMATS * EGL_NUM_STENCIL_FORMATS);
        CHECK(pixmaps32 == 2 * EGL_NUM_DEPTH_FORMATS * EGL_NUM_STENCIL_FORMATS);

        {   /* Typical game request: RGB888 + 16-bit depth window. */
            static const EGLint a[] = { EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
                                        EGL_DEPTH_SIZE, 16, EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
                                        EGL_NONE };
            n = choose(a, configs);
            CHECK(n == 4);
            check_all_rgb565(configs, n, 16);
        }
        {   /* RGBA8888 + 24-bit depth with the default (window) surface type. */
            static const EGLint a[] = { EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
                                        EGL_ALPHA_SIZE, 8, EGL_BUFFER_SIZE, 32,
                                        EGL_DEPTH_SIZE, 24, EGL_NONE };
            n = choose(a, configs);
            CHECK(n == 2);
            check_all_rgb565(configs, n, 24);
        }
        {   /* Explicit RGB565 requests behave as in the default build. */
            static const EGLint a[] = { EGL_RED_SIZE, 5, EGL_GREEN_SIZE, 6, EGL_BLUE_SIZE, 5,
                                        EGL_NONE };
            n = choose(a, configs);
            CHECK(n == 6);
            check_all_rgb565(configs, n, 0);
        }
        {   /* Off-screen 32-bit surfaces are unaffected. */
            static const EGLint a[] = { EGL_RED_SIZE, 8, EGL_ALPHA_SIZE, 8,
                                        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_NONE };
            n = choose(a, configs);
            CHECK(n == 6);
            for (i = 0; i < n; ++i)
                CHECK(attrib(configs[i], EGL_RED_SIZE) == 8 && attrib(configs[i], EGL_ALPHA_SIZE) == 8);
        }
        {
            static const EGLint a[] = { EGL_RED_SIZE, 8, EGL_SURFACE_TYPE, EGL_PIXMAP_BIT, EGL_NONE };
            n = choose(a, configs);
            CHECK(n == 12);
            CHECK(n > 0 && attrib(configs[0], EGL_RED_SIZE) == 8);
        }
        {   /* Requests that cannot be met by relaxing color still fail. */
            static const EGLint a[] = { EGL_RED_SIZE, 8, EGL_STENCIL_SIZE, 16, EGL_NONE };
            CHECK(choose(a, configs) == 0);
        }
        CHECK(eglTerminate((EGLDisplay)1) == EGL_TRUE);
        egliDestroyState();
    }

    /* The relaxation applies only to window requests without a config ID. */
    filter = s_defaultFilter;
    filter.redBits = 8; filter.alphaBits = 8; filter.colorBits = 32;
    CHECK(egliRelaxConfigFilter(&filter) == 1);
    CHECK(filter.redBits == 5 && filter.alphaBits == 0 && filter.colorBits == 16);
    CHECK(egliRelaxConfigFilter(&filter) == 0);
    filter = s_defaultFilter;
    filter.redBits = 8; filter.surfaceType = EGL_PBUFFER_BIT;
    CHECK(egliRelaxConfigFilter(&filter) == 0 && filter.redBits == 8);
    filter = s_defaultFilter;
    filter.redBits = 8; filter.configID = 3;
    CHECK(egliRelaxConfigFilter(&filter) == 0 && filter.redBits == 8);
    filter = s_defaultFilter;
    filter.redBits = EGL_DONT_CARE; filter.surfaceType = EGL_DONT_CARE;
    CHECK(egliRelaxConfigFilter(&filter) == 0);

    printf("Symbian RGB565: %d checks, %d failures\n", checks, failures);
    return failures != 0;
}
