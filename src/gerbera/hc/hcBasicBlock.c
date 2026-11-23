/* hcBasicBlock.c - CFG basic block helpers (hcBasicBlock.h of the original).
 *
 * Ported from docs/libGLES_CM.dll.c:46308-46368.
 */
#include "gerbera/hc/hcBasicBlock.h"
#include "gerbera/hc/hcOpcode.h"

#include <stddef.h>
#include <string.h>

void BasicBlock_init(BasicBlock_s *self)
{
    memset(self, 0, sizeof(*self));
}

void BasicBlock_setStart(BasicBlock_s *self, int start)
{
    if (!(start >= 0 && start < 65536))
        HC_assertFail("start >= 0 && start < 65536", "hcBasicBlock.h", 75, NULL);
    self->start = (unsigned short)start;
}

void BasicBlock_setEnd(BasicBlock_s *self, int end)
{
    if (!(end >= 0 && end < 65536))
        HC_assertFail("end >= 0 && end < 65536", "hcBasicBlock.h", 81, NULL);
    self->end = (unsigned short)end;
}

int BasicBlock_getStart(const BasicBlock_s *self)
{
    if (self == NULL)
        HC_assertFail("self", "hcBasicBlock.h", 87, NULL);
    return self->start;
}

int BasicBlock_getEnd(const BasicBlock_s *self)
{
    if (self == NULL)
        HC_assertFail("self", "hcBasicBlock.h", 93, NULL);
    return self->end;
}

void BasicBlock_setDominators(BasicBlock_s *self, HCBitSet_s *dom)
{
    if (!(self != NULL && dom != NULL))
        HC_assertFail("self && dom", "hcBasicBlock.h", 106, NULL);
    self->dominators = dom;
}
