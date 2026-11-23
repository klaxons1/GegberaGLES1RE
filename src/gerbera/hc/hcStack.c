/* hcStack.c - arena-backed LIFO stack of uint16 indices (hcStack.c/.h).
 *
 * Ported from docs/libGLES_CM.dll.c:46485-46506, 53091-53130.
 */
#include "gerbera/hc/hcStack.h"
#include "gerbera/hc/hcOpcode.h"

#include <stddef.h>

HCStack_s *HCStack_create(HCArena_s *mgr)
{
    HCStack_s *result = (HCStack_s *)HCArena_allocBottom(mgr, (unsigned int)sizeof(HCStack_s));
    result->numElems = 0;
    result->allocSize = 2;
    return result;
}

void HCStack_push(HCArena_s *mgr, HCStack_s *self, int value)
{
    int numElems;
    unsigned short *elems;

    if (!(mgr != NULL && self != NULL))
        HC_assertFail("mgr && self", "hcStack.c", 64, NULL);
    if ((unsigned int)value > 0xFFFFu)
        HC_assertFail("HC_IN_RANGE(value, 0, 0xFFFF)", "hcStack.c", 65, NULL);

    numElems = self->numElems;
    if (self->allocSize < numElems)
    {
        int ok = (self->allocSize == numElems - 1) || (self->allocSize == 2);
        if (!ok)
        {
            HC_assertFail("self->allocSize == numElems-1 || self->allocSize == 2",
                          "hcStack.c", 70, NULL);
        }
        self->allocSize = self->numElems + 1;
        HCArena_growUnaligned(mgr, 2u);
    }

    elems = (unsigned short *)((unsigned char *)self + offsetof(HCStack_s, elements));
    elems[numElems] = (unsigned short)value;
    self->numElems = numElems + 1;
}

int HCStack_pop(HCStack_s *self)
{
    int n;
    const unsigned short *elems;

    if (!(self != NULL && self->allocSize > 0))
        HC_assertFail("self && self->allocSize > 0", "hcStack.h", 54, NULL);
    n = self->numElems - 1;
    self->numElems = n;
    if (n < 0)
        HC_assertFail("n >= 0", "hcStack.h", 58, NULL);

    elems = (const unsigned short *)((const unsigned char *)self + offsetof(HCStack_s, elements));
    return elems[n];
}

int HCStack_isEmpty(HCStack_s *self)
{
    return self->numElems == 0;
}
