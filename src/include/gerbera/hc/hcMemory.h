/* hcMemory.h - HC arena allocator (hcMemory.h of the original).
 *
 * Double-ended arena: allocations grow up from `bottom` (low pool) and down
 * from `top` (high pool). HCMemory_s is a tagged view selecting which pool a
 * client allocates from (bottomInterface / topInterface).
 *
 * Ported from generated/fragments/hc/hcMemory.h.fragments.c
 * (decompiled: docs/libGLES_CM.dll.c HCArena_* / HCMemory_* / HCRandom_*).
 */
#ifndef GERBERA_HC_MEMORY_H
#define GERBERA_HC_MEMORY_H

#ifdef __cplusplus
extern "C" {
#endif

#define HC_RANDOM_KK 11

/* --- hcRandom (hcRandom.c) ----------------------------------------------- */

typedef struct HCRandom_s
{
    unsigned int randBuffer[HC_RANDOM_KK];
    int p1;
    int p2;
} HCRandom_s;

void HCRandom_init(HCRandom_s *self);
void HCRandom_setSeed(HCRandom_s *self, unsigned int seed);
unsigned int HCRandom_get(HCRandom_s *self);

/* --- arena ---------------------------------------------------------------- */

#define HC_ARENA_BOTTOM 0
#define HC_ARENA_TOP    1

typedef struct HCArena_s
{
    unsigned char *arena;                 /* backing buffer                  */
    int bottom;                           /* low-pool bump index             */
    int top;                              /* high-pool bump index            */
    int arenaSize;
    int peakMemUsage[2];
    int releaseCount[2];
    int simultaneousPeak;
    HCRandom_s random;
    void (*longjmp)(void *jmpbuf, int value);  /* overflow escape (setjmp)  */
    void *jmpbuf;
} HCArena_s;

/* HCMark_s overlays { bottom, top } at &self->bottom (hcMemory.c mark). */
typedef struct HCMark_s
{
    int bottom;
    int top;
} HCMark_s;

void HCArena_init(HCArena_s *self, void *memory, int size);
int HCArena_markBottom(HCArena_s *self);
void HCArena_releaseBottom(HCArena_s *self, int mark);
int HCArena_markTop(HCArena_s *self);
void HCArena_releaseTop(HCArena_s *self, int mark);
void *HCArena_current(HCArena_s *self);
void HCArena_growCurrent(HCArena_s *self, unsigned int nBytes);
int HCArena_inLowMem(HCArena_s *self, void *ptr);
void HCArena_updateStats(HCArena_s *self);
void *HCArena_allocBottom(HCArena_s *self, unsigned int nBytes);
void HCArena_setBottom(HCArena_s *self, void *ptr);
void *HCArena_allocTop(HCArena_s *self, unsigned int nBytes);
void *HCArena_allocBottomAlign2(HCArena_s *self, unsigned int nBytes);
void *HCArena_allocTopAlign2(HCArena_s *self, unsigned int nBytes);
HCMark_s HCArena_mark(HCArena_s *self);
void HCArena_release(HCArena_s *self, HCMark_s *mark);
void HCArena_growUnaligned(HCArena_s *self, unsigned int nBytes);

/* --- tagged pool interface ------------------------------------------------ */

typedef struct HCMemory_s
{
    HCArena_s *arena;
    int whichPool;                        /* 0 = bottom, 1 = top            */
} HCMemory_s;

HCMemory_s HCArena_bottomInterface(HCArena_s *self);
HCMemory_s HCArena_topInterface(HCArena_s *self);

int HCMemory_mark(HCMemory_s *self);
void HCMemory_release(HCMemory_s *self, int mark);
void *HCMemory_alloc(HCMemory_s *self, unsigned int nBytes);
void *HCMemory_allocAlign2(HCMemory_s *self, unsigned int nBytes);

#ifdef __cplusplus
}
#endif

#endif /* GERBERA_HC_MEMORY_H */
