/* Native initialization and OS heap ownership, without a Symbian device. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "symbian_os.h"
#include "gerbera/egl/eglInternal.h"
#define __SYMBIAN32__ 1
#include "../src/gerbera/egl/eglSymbian.c"
#include "../src/gerbera/egl/eglUtil.c"

static int checks, failures;
#define CHECK(c) do { ++checks; if (!(c)) { ++failures; \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static void *tls;
static uintptr_t environment[32];
static void *coe;
static int allocations, frees, failAllocation, failHeap, heapClosed;
static int constructions, leaveWithoutScreen, leaveWithoutEnv, modeQueries, halFails;
static int heapToken;
static int sortAppends, sortCalls, sortReads, sortCloses;

void sym_RArrayBase_Ctor(SymRArrayBase *array, int size)
{
    CHECK(size == (int)sizeof(TEGLCachedConfig));
    memset(array, 0, sizeof(*array));
    array->entrySize = size;
    array->entries = malloc(EGL_NUM_BUFFER_FORMATS * (size_t)size);
    CHECK(array->entries != NULL);
}
int sym_RArrayBase_Append(SymRArrayBase *array, const void *entry)
{
    memcpy((char *)array->entries + array->count * array->entrySize,
           entry, (size_t)array->entrySize);
    ++array->count;
    ++sortAppends;
    return 0;
}
void sym_RArrayBase_HeapSort(SymRArrayBase *array, int (*compare)(const void *, const void *))
{
    ++sortCalls;
    qsort(array->entries, (size_t)array->count, (size_t)array->entrySize, compare);
}
void *sym_RArrayBase_At(const SymRArrayBase *array, int index)
{
    CHECK(index >= 0 && index < array->count);
    ++sortReads;
    return (char *)array->entries + index * array->entrySize;
}
void sym_RArrayBase_Close(SymRArrayBase *array)
{
    ++sortCloses;
    free(array->entries);
}

void *sym_DllTls(void) { return tls; }
int sym_DllSetTls(void *p) { tls = p; return 0; }
void *sym_CCoeEnv_Static(void) { return coe; }
void *sym_User_Alloc(int size)
{
    ++allocations;
    return allocations == failAllocation ? NULL : malloc((size_t)size);
}
void sym_User_Free(void *p) { if (p) ++frees; free(p); }
void *sym_User_AllocZL(int size)
{
    CHECK(size == SYM_CCOEENV_SIZE);
    memset(environment, 0, sizeof(environment));
    return environment;
}
void *sym_CCoeEnv_Ctor(void *p) { coe = p; return p; }
void sym_CCoeEnv_ConstructTrapped(void *p)
{
    CHECK(p == coe);
    ++constructions;
    if (!leaveWithoutScreen)
        SYM_CCOEENV_SCREENDEV(p) = &heapToken;
    if (leaveWithoutEnv)
        coe = NULL;
}
int sym_HAL_Get(int attr, int *value)
{
    CHECK(attr == SYM_HAL_MEMORY_RAM);
    if (!halFails) *value = 32 * 1024 * 1024;
    return halFails ? -1 : 0;
}
void *sym_UserHeap_ChunkHeap(const void *name, int minLen, int maxLen,
                            int growBy, int align, int singleThread)
{
    CHECK(!name && !minLen && growBy == 1 && !align && !singleThread);
    CHECK(maxLen == (halFails ? 16 : 32) * 1024 * 1024);
    return failHeap ? NULL : &heapToken;
}
void sym_RAllocator_Close(void *heap)
{
    CHECK(heap == &heapToken);
    ++heapClosed;
}
int sym_RWsSession_GetDefModeMaxNumColors(const void *session, int *c, int *g)
{
    CHECK(session == SYM_CCOEENV_WSSESSION(coe));
    *c = 65536; *g = 0;
    ++modeQueries;
    return EGL_SYMBIAN_ECOLOR64K;
}

static void reset(void)
{
    tls = coe = NULL;
    allocations = frees = failAllocation = failHeap = heapClosed = 0;
    constructions = leaveWithoutScreen = leaveWithoutEnv = modeQueries = halFails = 0;
    memset(environment, 0, sizeof(environment));
}

static void check_init(int existing, int noScreen, int failedHal)
{
    eglState_s *s;
    reset();
    leaveWithoutScreen = noScreen;
    halFails = failedHal;
    if (existing)
    {
        coe = environment;
        if (!noScreen) SYM_CCOEENV_SCREENDEV(coe) = &heapToken;
    }
    s = egliInitState();
    CHECK(s != NULL && s == egliGetState());
    CHECK(allocations == 2 && frees == 0);
    CHECK(constructions == !existing);
    CHECK(s->windowDisplay != NULL && s->windowDisplay->surfaceList == NULL);
    CHECK(s->windowDisplay->refCount.referenceCount == 0);
    CHECK(s->ownEnvironment == 0 && s->errorCode == 0);
    CHECK(s->displayMode == (noScreen ? 0 : EGL_SYMBIAN_ECOLOR64K));
    CHECK(modeQueries == !noScreen);
    egliDestroyState();
    CHECK(tls == NULL && frees == 2 && heapClosed == 1);
    CHECK(coe == environment);
}

int main(void)
{
    int i;
    EGLConfig configs[EGL_NUM_BUFFER_FORMATS];
    eglConfig filter, previous, current;
    for (i = 0; i < 8; ++i) check_init(i & 1, (i >> 1) & 1, (i >> 2) & 1);
    reset();
    leaveWithoutEnv = 1;
    CHECK(egliInitState() != NULL);
    CHECK(egliGetState()->displayMode == 0 && modeQueries == 0);
    egliDestroyState();
    CHECK(frees == 2 && heapClosed == 1);
    for (i = 1; i <= 2; ++i)
    {
        reset();
        failAllocation = i;
        CHECK(egliInitState() == NULL);
        CHECK(allocations == i && frees == i - 1 && tls == NULL);
    }
    reset();
    failHeap = 1;
    CHECK(egliInitState() == NULL);
    CHECK(allocations == 2 && frees == 2 && heapClosed == 0 && tls == NULL);
    CHECK(coe == environment);
    reset();
    CHECK(egliInitState() != NULL);
    memset(&filter, 0, sizeof(filter));
    for (i = 0; i < EGL_NUM_BUFFER_FORMATS; ++i)
        configs[i] = EGL_NUM_BUFFER_FORMATS - i;
    egliSortConfigs(configs, EGL_NUM_BUFFER_FORMATS, &filter);
    CHECK(sortAppends == EGL_NUM_BUFFER_FORMATS && sortReads == EGL_NUM_BUFFER_FORMATS);
    CHECK(sortCalls == 1 && sortCloses == 1);
    eglFillConfig(&previous, configs[0]);
    for (i = 1; i < EGL_NUM_BUFFER_FORMATS; ++i)
    {
        eglFillConfig(&current, configs[i]);
        CHECK(eglCompareConfigs(&previous, &current, &filter) <= 0);
        previous = current;
    }
    /* Original invokes the OS array path even with zero or one element. */
    egliSortConfigs(configs, 0, &filter);
    egliSortConfigs(configs, 1, &filter);
    CHECK(sortCalls == 3 && sortCloses == 3);
    CHECK(sortAppends == EGL_NUM_BUFFER_FORMATS + 1);
    egliDestroyState();
    printf("Symbian state: %d checks, %d failures\n", checks, failures);
    return failures != 0;
}
