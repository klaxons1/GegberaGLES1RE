/* hcDUChain.c - def-use and use-def chain allocation (hcDUChain.c).
 *
 * Ported from docs/libGLES_CM.dll.c:51840-51862 (DUChains_getDUChain lives in
 * hcBackendInfo.c so backends can link standalone).
 */
#include "gerbera/hc/hcDUChain.h"
#include "gerbera/hc/hcOpcode.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

DUChain_s *DUChain_create(HCMemory_s *mgr, int length)
{
    DUChain_s *self;
    int count = (length > 1) ? length : 1;

    self = (DUChain_s *)HCMemory_allocAlign2(
        mgr, 2u * (unsigned int)(count - 1) + 4u);
    if (((uintptr_t)self & 1u) != 0)
        HC_assertFail("(((HCuintptr)self) & 1) == 0", "hcDUChain.c", 30, NULL);
    memset(self, 0, 4u);
    return self;
}

UDChainStub_s *UDChainStub_create(HCMemory_s *mgr, int nChains)
{
    UDChainStub_s *self;
    unsigned int bytes;

    if (nChains == 0)
        HC_assertFail("nChains", "hcDUChain.c", 37, NULL);
    bytes = (unsigned int)nChains * (unsigned int)sizeof(DUChain_s *);
    self = (UDChainStub_s *)HCMemory_alloc(mgr, bytes);
    memset(self, 0, bytes);
    return self;
}
