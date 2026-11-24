/* Supply platform state while comparing native EGL configuration filtering. */
#include <string.h>
#include <stdlib.h>

#include "gerbera/egl/eglInternal.h"

typedef char config_layout_must_match_original[(sizeof(eglConfig) == 96) ? 1 : -1];

void differential_egl_fix_config(int displayMode, int initialized, eglConfig *config)
{
    eglState_s state;
    void *previous = egliGetGlobal();

    memset(&state, 0, sizeof(state));
    state.displayMode = displayMode;
    egliSetGlobal(initialized ? &state : NULL);
    egliFixConfig(config);
    egliSetGlobal(previous);
}

int differential_egl_platform_config(int configID, int displayMode, eglConfig *config)
{
    eglState_s state;
    void *previous = egliGetGlobal();
    int valid;

    memset(&state, 0, sizeof(state));
    state.displayMode = displayMode;
    egliSetGlobal(&state);
    eglFillConfig(config, configID);
    valid = eglIsValidConfigID(configID);
    egliSetGlobal(previous);
    return valid;
}

static void differential_egl_state(eglState_s *state, eglDisplay_s *display, int mode)
{
    memset(state, 0, sizeof(*state));
    memset(display, 0, sizeof(*display));
    state->windowDisplay = display;
    state->displayMode = mode;
    state->errorCode = EGL_BAD_ACCESS;
    egliSetGlobal(state);
}

int differential_egl_get_configs(int mode, int displayID, int limit,
                                  EGLConfig *configs, EGLint *count, int *error)
{
    eglState_s state;
    eglDisplay_s display;
    void *previous = egliGetGlobal();
    int result;

    differential_egl_state(&state, &display, mode);
    result = eglGetConfigs(displayID, configs, limit, count);
    *error = state.errorCode;
    egliSetGlobal(previous);
    return result;
}

int differential_egl_get_config_attrib(int mode, int displayID, int configID,
                                       int attribute, EGLint *value, int *error)
{
    eglState_s state;
    eglDisplay_s display;
    void *previous = egliGetGlobal();
    int result;

    differential_egl_state(&state, &display, mode);
    result = eglGetConfigAttrib(displayID, configID, attribute, value);
    *error = state.errorCode;
    egliSetGlobal(previous);
    return result;
}

int differential_egl_choose_configs(int mode, int displayID, const EGLint *attributes,
                                     EGLConfig *configs, int limit, EGLint *count,
                                     int initialError, int *error)
{
    eglState_s state;
    eglDisplay_s display;
    void *previous = egliGetGlobal();
    int result;

    differential_egl_state(&state, &display, mode);
    state.errorCode = initialError;
    result = eglChooseConfig(displayID, attributes, configs, limit, count);
    *error = state.errorCode;
    egliSetGlobal(previous);
    return result;
}

typedef struct DifferentialEGLObjects
{
    eglState_s state;
    eglDisplay_s display;
    eglPbufferSurface surfaces[3];
    eglContext_s contexts[3];
    void *previous;
} DifferentialEGLObjects;

void *differential_egl_objects_create(const int *input)
{
    DifferentialEGLObjects *f = calloc(1, sizeof(*f));
    int i;
    if (f == NULL) return NULL;
    f->previous = egliGetGlobal();
    f->state.windowDisplay = &f->display;
    f->state.errorCode = EGL_BAD_ACCESS;
    f->state.displayMode = EGL_SYMBIAN_ECOLOR64K;
    f->display.surfaceCount = f->display.contextCount = 3;
    f->display.surfaceList = &f->surfaces[0].surface;
    f->display.contextList = &f->contexts[0];
    for (i = 0; i < 3; ++i)
    {
        eglPbufferSurface *p = &f->surfaces[i];
        eglContext_s *c = &f->contexts[i];
        p->surface.refCount.referenceCount = 2;
        p->surface.refCount.destroyed = i == 1 ? input[8] : 0;
        p->surface.prev = i ? &f->surfaces[i - 1].surface : NULL;
        p->surface.next = i < 2 ? &f->surfaces[i + 1].surface : NULL;
        p->surface.display = &f->display;
        p->surface.configID = input[11] + i * 3;
        p->surface.colorBufferStride = 2 * input[0];
        p->surface.width = input[0];
        p->surface.height = input[1];
        p->surface.id = 11 + i;
        p->surface.type = input[2];
        p->isLargestPbuffer = input[6];
        p->mipmapTexture = input[7];
        p->textureFormat = input[3];
        p->textureTarget = input[4];
        p->mipmapLevel = input[5];
        c->refCount.referenceCount = 1;
        c->refCount.destroyed = i == 1 ? input[9] : 0;
        c->display = &f->display;
        c->prev = i ? &f->contexts[i - 1] : NULL;
        c->next = i < 2 ? &f->contexts[i + 1] : NULL;
        c->configID = 1 + i * 3;
        c->id = 21 + i;
        c->hasBeenBound = i & 1;
    }
    if (input[10] & 1) f->state.currentContext = &f->contexts[1];
    if (input[10] & 2) f->state.currentReadSurface = &f->surfaces[0].surface;
    if (input[10] & 4) f->state.currentDrawSurface = &f->surfaces[2].surface;
    if (input[10] & 8) f->state.currentDisplay = &f->display;
    egliSetGlobal((input[10] & 16) ? NULL : &f->state);
    return f;
}

static unsigned int object_pointer(const DifferentialEGLObjects *f, const void *pointer)
{
    int i;
    if (pointer == NULL) return 0;
    if (pointer == &f->display) return 1;
    for (i = 0; i < 3; ++i)
    {
        if (pointer == &f->surfaces[i]) return 100 + i;
        if (pointer == &f->contexts[i]) return 200 + i;
    }
    return 0xffffffffu;
}

void differential_egl_objects_snapshot(void *fixture, unsigned int *out)
{
    DifferentialEGLObjects *f = fixture;
    int i;
#define PTR(p) (*out++ = object_pointer(f, (p)))
#define WORD(v) (*out++ = (unsigned int)(v))
#define REF(r) do { memcpy(out, &(r), 4); ++out; } while (0)
    PTR(f->state.currentESContext); PTR(f->state.currentContext);
    PTR(f->state.currentReadSurface); PTR(f->state.currentDrawSurface);
    PTR(f->state.currentDisplay); PTR(f->state.windowDisplay);
    WORD(f->state.errorCode); WORD(f->state.displayMode);
    PTR(f->state.heap); WORD(f->state.ownEnvironment);
    REF(f->display.refCount); PTR(f->display.surfaceList); PTR(f->display.contextList);
    WORD(f->display.surfaceCount); WORD(f->display.contextCount);
    for (i = 0; i < 3; ++i)
    {
        eglPbufferSurface *p = &f->surfaces[i];
        REF(p->surface.refCount); PTR(p->surface.prev); PTR(p->surface.next);
        PTR(p->surface.display); PTR(p->surface.colorBuffer);
        PTR(p->surface.depthBuffer); PTR(p->surface.stencilBuffer);
        WORD(p->surface.configID); WORD(p->surface.colorBufferStride);
        WORD(p->surface.width); WORD(p->surface.height); WORD(p->surface.id);
        WORD(p->surface.type); WORD(p->isLargestPbuffer); WORD(p->mipmapTexture);
        WORD(p->textureFormat); WORD(p->textureTarget); WORD(p->mipmapLevel);
        PTR(p->boundESContext); WORD(p->textureName);
    }
    for (i = 0; i < 3; ++i)
    {
        eglContext_s *c = &f->contexts[i];
        REF(c->refCount); PTR(c->display); PTR(c->prev); PTR(c->next); PTR(c->esContext);
        WORD(c->configID); WORD(c->id); WORD(c->hasBeenBound);
    }
#undef PTR
#undef WORD
#undef REF
}

int differential_egl_objects_step(void *fixture, int operation, const int *args, int *output)
{
    DifferentialEGLObjects *f = fixture;
    switch (operation)
    {
    case 0: return eglQuerySurface(args[0], args[1], args[2], args[3] ? output : NULL);
    case 1: return eglSurfaceAttrib(args[0], args[1], args[2], args[3]);
    case 2: return eglQueryContext(args[0], args[1], args[2], args[3] ? output : NULL);
    case 3: return eglDestroySurface(args[0], args[1]);
    case 4: return eglDestroyContext(args[0], args[1]);
    case 5: return eglGetCurrentContext();
    case 6: return eglGetCurrentSurface(args[0]);
    case 7: return eglGetCurrentDisplay();
    case 8: return eglWaitGL();
    case 9: return eglWaitNative(args[0]);
    case 10: return eglSwapInterval(args[0], args[1]);
    case 11: return eglSwapBuffers(args[0], args[1]);
    case 12:
    {
        eglNativePixmap_s target;
        unsigned int pixel = 0;
        memset(&target, 0, sizeof(target));
        target.width = f->surfaces[0].surface.width;
        target.height = f->surfaces[0].surface.height;
        target.width += (args[2] == 2) ? -1 : (args[2] == 3);
        target.height += (args[2] == 4) ? -1 : (args[2] == 5);
        target.stride = target.width * 2;
        target.displayMode = args[2] == 6 ? EGL_SYMBIAN_ECOLOR16MA : EGL_SYMBIAN_ECOLOR64K;
        target.data = &pixel;
        return eglCopyBuffers(args[0], args[1], args[2] ? &target : NULL);
    }
    case 13: return eglGetError();
    default: abort();
    }
}

void differential_egl_objects_destroy(void *fixture)
{
    DifferentialEGLObjects *f = fixture;
    egliSetGlobal(f->previous);
    free(f);
}

void differential_egl_lifecycle(int tag, int count, const int *commands, unsigned int *output)
{
    void *previous = egliGetGlobal();
    int i;
    egliSetGlobal((void *)(uintptr_t)tag);
    for (i = 0; i < count; ++i)
    {
        int major = -777, minor = -777;
        int operation = commands[2 * i], display = commands[2 * i + 1];
        switch (operation)
        {
        case 0: *output++ = eglInitialize(display, &major, &minor); break;
        case 1: *output++ = eglTerminate(display); break;
        case 2: *output++ = eglGetCurrentContext(); break;
        default: abort();
        }
        *output++ = eglGetError();
        *output++ = egliGetState() != NULL;
        *output++ = (unsigned int)major;
        *output++ = (unsigned int)minor;
    }
    eglTerminate(1);
    egliSetGlobal(previous);
}

void differential_egl_pbuffer(int displayID, int configID, const int *attributes,
                              unsigned int *output)
{
    eglState_s state;
    eglDisplay_s display;
    eglPbufferSurface existing;
    eglPbufferSurface *p;
    EGLSurface id;
    void *previous = egliGetGlobal();
    unsigned int *out = output;
    memset(output, 0, 30 * sizeof(*output));
    differential_egl_state(&state, &display, EGL_SYMBIAN_ECOLOR64K);
    memset(&existing, 0, sizeof(existing));
    existing.surface.display = &display;
    existing.surface.id = 77;
    existing.surface.type = EGL_SURFACE_PBUFFER;
    existing.surface.refCount.referenceCount = 2;
    display.surfaceList = &existing.surface;
    display.surfaceCount = 100;
    id = eglCreatePbufferSurface(displayID, configID, attributes);
    p = id ? (eglPbufferSurface *)eglMapSurface(&display, id) : NULL;
#define IDENTITY(v) ((v) == NULL ? 0u : (const void *)(v) == &display ? 1u : \
                    (const void *)(v) == &existing ? 10u : (const void *)(v) == p ? 11u : 0xffffffffu)
    *out++ = id; *out++ = state.errorCode; *out++ = display.surfaceCount;
    *out++ = IDENTITY(display.surfaceList); *out++ = IDENTITY(existing.surface.prev);
    if (p != NULL)
    {
        memcpy(out++, &p->surface.refCount, 4);
        *out++ = IDENTITY(p->surface.prev); *out++ = IDENTITY(p->surface.next);
        *out++ = IDENTITY(p->surface.display);
        *out++ = p->surface.colorBuffer != NULL;
        *out++ = p->surface.depthBuffer != NULL;
        *out++ = p->surface.stencilBuffer != NULL;
        *out++ = p->surface.configID; *out++ = p->surface.colorBufferStride;
        *out++ = p->surface.width; *out++ = p->surface.height;
        *out++ = p->surface.id; *out++ = p->surface.type;
        *out++ = p->isLargestPbuffer; *out++ = p->mipmapTexture;
        *out++ = p->textureFormat; *out++ = p->textureTarget; *out++ = p->mipmapLevel;
        *out++ = IDENTITY(p->boundESContext); *out++ = p->textureName;
        output[25] = eglDestroySurface(1, id);
    }
    output[26] = state.errorCode;
    output[27] = display.surfaceCount;
    output[28] = IDENTITY(display.surfaceList);
    output[29] = IDENTITY(existing.surface.prev);
#undef IDENTITY
    egliSetGlobal(previous);
}
