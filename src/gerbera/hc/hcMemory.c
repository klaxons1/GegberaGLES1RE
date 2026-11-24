/* Double-ended scratch arena and register-allocation PRNG. */
#include "gerbera/hc/hcMemory.h"
#include "gerbera/hc/hcOpcode.h"

#include <stddef.h>
#include <stdint.h>

/* --- hcRandom (hcRandom.c) ------------------------------------------------ */

static unsigned int rotateLeft(unsigned int x, int r)
{
    r &= 31;
    if (r == 0)
        return x;
    return (unsigned int)((x << r) | (x >> (32 - r)));
}

void HCRandom_init(HCRandom_s *self)
{
    HCRandom_setSeed(self, 0xFFFFFFFFu);
}

void HCRandom_setSeed(HCRandom_s *self, unsigned int seed)
{
    int i;
    int ia;
    unsigned int seeda;

    if (seed == 0)
        seed = 0xFFFFFFFFu;
    for (i = 0; i < HC_RANDOM_KK; ++i)
    {
        seeda = (((seed << 13) ^ seed) >> 17) ^ (seed << 13) ^ seed;
        seed = (32u * seeda) ^ seeda;
        self->randBuffer[i] = seed;
    }
    self->p1 = 0;
    self->p2 = 7;
    for (ia = 0; ia < 9; ++ia)
        HCRandom_get(self);
}

unsigned int HCRandom_get(HCRandom_s *self)
{
    unsigned int result;
    int p2;
    int p1;

    p1 = self->p1;
    p2 = self->p2;
    if (!(p1 >= 0 && p1 < HC_RANDOM_KK))
        HC_assertFail("p1 >= 0 && p1 < HC_RANDOM_KK", "hcRandom.c", 39, NULL);
    if (!(p2 >= 0 && p2 < HC_RANDOM_KK))
        HC_assertFail("p2 >= 0 && p2 < HC_RANDOM_KK", "hcRandom.c", 40, NULL);
    result = rotateLeft(self->randBuffer[p2] + self->randBuffer[p1], 13);
    self->randBuffer[p1] = result;
    self->p1 = (((p1 - 1) >> 31) & 0xB) + p1 - 1;
    self->p2 = (((p2 - 1) >> 31) & 0xB) + p2 - 1;
    return result;
}

/* --- arena ---------------------------------------------------------------- */

void HCArena_init(HCArena_s *self, void *memory, int size)
{
    if (!(memory != NULL && size != 0))
        HC_assertFail("memory && size != 0", "hcMemory.c", 36, NULL);
    self->arena = (unsigned char *)memory;
    self->bottom = 0;
    self->top = size;
    self->arenaSize = size;
    self->peakMemUsage[0] = 0;
    self->peakMemUsage[1] = 0;
    self->releaseCount[0] = 0;
    self->releaseCount[1] = 0;
    self->simultaneousPeak = 0;
    self->longjmp = NULL;
    self->jmpbuf = NULL;
    HCRandom_init(&self->random);
}

int HCArena_markBottom(HCArena_s *self)
{
    if (!(self->bottom >= 0 && self->top >= 0))
        HC_assertFail("self->bottom >= 0 && self->top >= 0", "hcMemory.h", 118, NULL);
    if (++self->releaseCount[HC_ARENA_BOTTOM] < 0)
        HC_assertFail("self->releaseCount[HC_ARENA_BOTTOM] >= 0", "hcMemory.h", 121, NULL);
    return self->bottom;
}

void HCArena_releaseBottom(HCArena_s *self, int mark)
{
    if (mark < 0)
        HC_assertFail("mark >= 0", "hcMemory.h", 128, NULL);
    if (--self->releaseCount[HC_ARENA_BOTTOM] < 0)
        HC_assertFail("self->releaseCount[HC_ARENA_BOTTOM] >= 0", "hcMemory.h", 131, NULL);
    self->bottom = mark;
}

int HCArena_markTop(HCArena_s *self)
{
    if (!(self->bottom >= 0 && self->top >= 0))
        HC_assertFail("self->bottom >= 0 && self->top >= 0", "hcMemory.h", 138, NULL);
    if (++self->releaseCount[HC_ARENA_TOP] < 0)
        HC_assertFail("self->releaseCount[HC_ARENA_TOP] >= 0", "hcMemory.h", 141, NULL);
    return self->top;
}

void HCArena_releaseTop(HCArena_s *self, int mark)
{
    if (mark < 0)
        HC_assertFail("mark >= 0", "hcMemory.h", 148, NULL);
    if (--self->releaseCount[HC_ARENA_TOP] < 0)
        HC_assertFail("self->releaseCount[HC_ARENA_TOP] >= 0", "hcMemory.h", 151, NULL);
    self->top = mark;
}

void *HCArena_current(HCArena_s *self)
{
#if UINTPTR_MAX > 0xFFFFFFFFu
    /* The original allocator aligns four-byte pointers (0045E168). Host
     * compiler records contain wider pointers; align a new block's start
     * without adding padding between its packed, appended elements. */
    uintptr_t address = (uintptr_t)self->arena + (unsigned int)self->bottom;
    self->bottom = (int)((unsigned int)self->bottom
        + (unsigned int)((0u - address) & (sizeof(void *) - 1u)));
    HCArena_updateStats(self);
    if (self->bottom < 0 || self->bottom >= self->top)
        self->longjmp(self->jmpbuf, 1);
#endif
    return &self->arena[self->bottom];
}

void HCArena_growCurrent(HCArena_s *self, unsigned int nBytes)
{
    self->bottom = (int)((nBytes + (unsigned int)self->bottom + 3u) & ~3u);
    HCArena_updateStats(self);
    if (self->bottom < 0 || self->bottom >= self->top)
        self->longjmp(self->jmpbuf, 1);
}

int HCArena_inLowMem(HCArena_s *self, void *ptr)
{
    return ptr < (void *)&self->arena[self->bottom];
}

void HCArena_updateStats(HCArena_s *self)
{
    int usedTotal;
    int usedHi;
    int usedLow;

    usedLow = self->bottom;
    usedHi = self->arenaSize - self->top;
    usedTotal = usedHi + usedLow;
    if (usedLow > self->peakMemUsage[0])
        self->peakMemUsage[0] = usedLow;
    if (usedHi > self->peakMemUsage[1])
        self->peakMemUsage[1] = usedHi;
    if (self->simultaneousPeak < usedTotal)
        self->simultaneousPeak = usedTotal;
}

void *HCArena_allocBottom(HCArena_s *self, unsigned int nBytes)
{
    int oldIndex;

#if UINTPTR_MAX > 0xFFFFFFFFu
    HCArena_current(self);
#endif
    oldIndex = self->bottom;
    HCArena_growCurrent(self, nBytes);
    return &self->arena[oldIndex];
}

void HCArena_setBottom(HCArena_s *self, void *ptr)
{
    int newBtm;

    if ((char *)ptr < (char *)self->arena)
        HC_assertFail("(HCuintptr)ptr >= (HCuintptr)self->arena", "hcMemory.c", 94, NULL);
    newBtm = (int)((char *)ptr - (char *)self->arena);
    if (newBtm >= self->top || newBtm < 0)
        self->longjmp(self->jmpbuf, 1);
    self->bottom = newBtm;
}

void *HCArena_allocTop(HCArena_s *self, unsigned int nBytes)
{
#if UINTPTR_MAX > 0xFFFFFFFFu
    uintptr_t address = (uintptr_t)self->arena + (unsigned int)self->top - nBytes;
    self->top = (int)((address & ~(uintptr_t)(sizeof(void *) - 1u))
                     - (uintptr_t)self->arena);
#else
    self->top = (int)(((unsigned int)self->top - nBytes) & ~3u);
#endif
    HCArena_updateStats(self);
    if (self->top < 0 || self->top < self->bottom)
        self->longjmp(self->jmpbuf, 1);
    return &self->arena[self->top];
}

void *HCArena_allocBottomAlign2(HCArena_s *self, unsigned int nBytes)
{
    int oldIndex;

    oldIndex = self->bottom;
    self->bottom = (int)((nBytes + (unsigned int)oldIndex + 1u) & ~1u);
    HCArena_updateStats(self);
    if (self->bottom < 0 || self->bottom >= self->top)
        self->longjmp(self->jmpbuf, 1);
    return &self->arena[oldIndex];
}

void *HCArena_allocTopAlign2(HCArena_s *self, unsigned int nBytes)
{
    self->top = (int)((self->top - (int)nBytes) & ~1);
    HCArena_updateStats(self);
    if (self->top < 0 || self->top < self->bottom)
        self->longjmp(self->jmpbuf, 1);
    return &self->arena[self->top];
}

HCMemory_s HCArena_bottomInterface(HCArena_s *self)
{
    HCMemory_s m;
    m.arena = self;
    m.whichPool = 0;
    return m;
}

HCMemory_s HCArena_topInterface(HCArena_s *self)
{
    HCMemory_s m;
    m.arena = self;
    m.whichPool = 1;
    return m;
}

HCMark_s HCArena_mark(HCArena_s *self)
{
    HCMark_s mrk;

    mrk = *(HCMark_s *)&self->bottom;
    ++self->releaseCount[HC_ARENA_BOTTOM];
    ++self->releaseCount[HC_ARENA_TOP];
    if (self->releaseCount[HC_ARENA_BOTTOM] < 0)
        HC_assertFail("self->releaseCount[HC_ARENA_BOTTOM] >= 0", "hcMemory.c", 168, NULL);
    if (self->releaseCount[HC_ARENA_TOP] < 0)
        HC_assertFail("self->releaseCount[HC_ARENA_TOP] >= 0", "hcMemory.c", 169, NULL);
    return mrk;
}

void HCArena_release(HCArena_s *self, HCMark_s *mark)
{
    int top;

    top = mark->top;
    self->bottom = mark->bottom;
    self->top = top;
    --self->releaseCount[HC_ARENA_BOTTOM];
    --self->releaseCount[HC_ARENA_TOP];
    if (self->releaseCount[HC_ARENA_BOTTOM] < 0)
        HC_assertFail("self->releaseCount[HC_ARENA_BOTTOM] >= 0", "hcMemory.c", 183, NULL);
    if (self->releaseCount[HC_ARENA_TOP] < 0)
        HC_assertFail("self->releaseCount[HC_ARENA_TOP] >= 0", "hcMemory.c", 184, NULL);
}

int HCMemory_mark(HCMemory_s *self)
{
    HCArena_s *a;

    a = self->arena;
    if (self->whichPool != 0)
        return HCArena_markTop(a);
    else
        return HCArena_markBottom(a);
}

void HCMemory_release(HCMemory_s *self, int mark)
{
    HCArena_s *a;

    a = self->arena;
    if (self->whichPool != 0)
        HCArena_releaseTop(a, mark);
    else
        HCArena_releaseBottom(a, mark);
}

void *HCMemory_alloc(HCMemory_s *self, unsigned int nBytes)
{
    HCArena_s *a;

    a = self->arena;
    if (self->whichPool != 0)
        return HCArena_allocTop(a, nBytes);
    else
        return HCArena_allocBottom(a, nBytes);
}

void *HCMemory_allocAlign2(HCMemory_s *self, unsigned int nBytes)
{
    HCArena_s *a;

    a = self->arena;
    if (self->whichPool != 0)
        return HCArena_allocTopAlign2(a, nBytes);
    else
        return HCArena_allocBottomAlign2(a, nBytes);
}

void HCArena_growUnaligned(HCArena_s *self, unsigned int nBytes)
{
    self->bottom += (int)nBytes;
    HCArena_updateStats(self);
    if (self->bottom < 0 || self->bottom >= self->top)
        self->longjmp(self->jmpbuf, 1);
}
