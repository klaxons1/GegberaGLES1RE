/* grrStubs.c - rasterizer point/line wrappers and hosted EGL/OS hooks
 * (eglSymbian.inl / eglBase.inl) until the full egl module lands.
 */
#include <setjmp.h>
#include <stdlib.h>
#include "gerbera/grState.h"
#include "gerbera/hc/hcBackend.h"
#include "gerbera/api_opengl/oglContext.h"

void grrDrawClippedPoint(Gerbera_s *state, _grClipTriangle *tri, unsigned int origIndices)
{
    grDrawClippedPoint(state, tri, origIndices);
}

void grrDrawClippedLine(Gerbera_s *state, _grClipTriangle *tri, unsigned int origIndices)
{
    grDrawClippedLine(state, tri, origIndices);
}

void grrDrawClippedTriangle(Gerbera_s *state, _grClipTriangle *tri, unsigned int origIndices)
{
    if (grTestCapability(state, GR_CAPABILITY_PERSPECTIVE_CORRECTION) != 0)
        grTriangleTessellator(state, tri, origIndices);
    else
        grDrawClippedTriangle(state, tri, origIndices, 8);
}

/* --- grSymbian.cpp: active JIT backend ------------------------------------- */

void *grGetBackend(void)
{
    return HCX86Backend_create();
}

/* --- eglSymbian.inl / eglBase.inl hosted allocator, executable & setjmp ---- */

struct grExecutable_s
{
    void *data;
    int   size;
};

void *eglAlloc(unsigned int size)
{
    if (size == 0 && grAssert("size > 0", "eglSymbian.inl", 252) != 0)
        grDie(0);
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

    if (size == 0 && grAssert("size > 0", "eglSymbian.inl", 281) != 0)
        grDie(0);
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
    if (executable == NULL && grAssert("executable", "eglSymbian.inl", 306) != 0)
        grDie(0);
    free(executable->data);
    free(executable);
}

int eglGetExecutableSize(grExecutable_s *executable)
{
    if (executable == NULL && grAssert("executable", "eglSymbian.inl", 321) != 0)
        grDie(0);
    return executable->size;
}

void *eglGetExecutableData(grExecutable_s *executable)
{
    if (executable == NULL && grAssert("executable", "eglSymbian.inl", 334) != 0)
        grDie(0);
    return executable->data;
}

int eglGetJmpbufSize(void)
{
    return (hgMax32(64, (int)sizeof(jmp_buf)) + 7) & ~7;
}

int eglHCWrapSetjmp(void *jmpbuf, int (*func)(void *), void *args)
{
    if (setjmp(*(jmp_buf *)jmpbuf) != 0)
        return -1;
    return func(args);
}

void eglHCWrapLongjmp(void *jmpbuf, int val)
{
    longjmp(*(jmp_buf *)jmpbuf, val);
}

/* --- Hosted current-context / EGL error state (until egl module lands) ----- */

static eglState_s s_hostedEglState = { NULL, 0x3000 };
static int        s_hostedEglInitialized = 0;

eglState_s *egliGetState(void)
{
    return s_hostedEglInitialized ? &s_hostedEglState : NULL;
}

void eglSetError(int error)
{
    s_hostedEglState.errorCode = error;
}

void egliSetCurrentContext(oglContext_s *context)
{
    s_hostedEglState.currentESContext = context;
    s_hostedEglInitialized = (context != NULL) ? 1 : 0;
    s_hostedEglState.errorCode = 0x3000;
}

int eglGetLastErrorHosted(void)
{
    return s_hostedEglState.errorCode;
}
