/* hcQueue.c - arena-backed circular FIFO queue (hcQueue.c of the original).
 *
 * Ported from docs/libGLES_CM.dll.c:46478-46482, 52968-53030.
 */
#include "gerbera/hc/hcQueue.h"
#include "gerbera/hc/hcOpcode.h"

#include <string.h>

HCQueue_s *HCQueue_create(HCArena_s *mgr)
{
    HCQueue_s *self = (HCQueue_s *)HCArena_allocBottom(mgr, (unsigned int)sizeof(HCQueue_s));
    memset(self, 0, sizeof(*self));
    self->elements = (unsigned short *)HCArena_current(mgr);
    return self;
}

int HCQueue_isEmpty(HCQueue_s *self)
{
    return self->nElems == 0;
}

void HCQueue_enqueue(HCArena_s *mgr, HCQueue_s *self, int value)
{
    int tail;

    if ((unsigned int)value > 0xFFFFu)
        HC_assertFail("HC_IN_RANGE(value, 0, 65535)", "hcQueue.c", 78, NULL);
    if (self->nElems > self->allocSize)
        HC_assertFail("self->nElems <= self->allocSize", "hcQueue.c", 79, NULL);

    if (self->nElems >= self->allocSize)
    {
        int oldAllocSize = self->allocSize;
        unsigned short *src;
        unsigned short *dst;
        int i;

        if (self->head > self->tail)
            HC_assertFail("self->head <= self->tail", "hcQueue.c", 85, NULL);
        self->allocSize += 8;
        HCArena_growUnaligned(mgr, 16u);
        src = &self->elements[oldAllocSize - 1];
        dst = src + 8;
        for (i = 0; i < oldAllocSize - self->head; ++i)
            *dst-- = *src--;
        self->head += 8;
        if (self->head >= self->allocSize)
            self->head -= self->allocSize;
    }

    tail = self->tail;
    self->tail = tail + 1;
    self->elements[tail] = (unsigned short)value;
    if (self->tail >= self->allocSize)
        self->tail -= self->allocSize;
    ++self->nElems;
}

int HCQueue_dequeue(HCQueue_s *self)
{
    int idx;
    unsigned short v;

    if (self->nElems <= 0)
        HC_assertFail("self->nElems > 0", "hcQueue.c", 118, NULL);
    idx = self->head++;
    v = self->elements[idx];
    if (self->head >= self->allocSize)
        self->head -= self->allocSize;
    --self->nElems;
    return v;
}
