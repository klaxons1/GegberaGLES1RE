/* hcBackendInfo.c - backend analysis info accessors (hcCompiler.c /
 * hcBackend.h / hcDUChain.c of the original).
 *
 * Ported from docs/libGLES_CM.dll.c: HC_getFreeHardRegs (0044C630),
 * HC_getDUChain (42846), HCChain_getLength/getValue (42854/42862),
 * DUChains_getDUChain (42828), HC_getUDChain (0045A850), HC_max (42819).
 */
#include "gerbera/hc/hcBackendInfo.h"

#include <stddef.h>

int HC_max(int a, int b)
{
    if (a < b)
        return b;
    else
        return a;
}

unsigned int HC_getFreeHardRegs(void *beInfo, int ndx)
{
    HCBackendInfo_s *info = (HCBackendInfo_s *)beInfo;
    return info->freeRegMasks[ndx];
}

void *DUChains_getDUChain(hcChains_s *chains, int which)
{
    unsigned short p;

    p = chains->duChains[which];
    if (p != 0)
        return (void *)((unsigned short *)chains->duChains - p);
    else
        return NULL;
}

void *HC_getDUChain(void *beInfo, int instrNdx)
{
    HCBackendInfo_s *info = (HCBackendInfo_s *)beInfo;

    if (info->chains.duChains == NULL)
        HC_assertFail("info->chains.duChains", "hcBackend.h", 159, NULL);
    return DUChains_getDUChain(&info->chains, instrNdx);
}

int HCChain_getLength(void *chain)
{
    if (chain == NULL)
        HC_assertFail("c", "hcBackend.h", 172, NULL);
    return *(unsigned short *)chain;
}

int HCChain_getValue(void *chain, int ndx)
{
    if (chain == NULL)
        HC_assertFail("c", "hcBackend.h", 186, NULL);
    if (!(ndx >= 0 && ndx < *(unsigned short *)chain))
        HC_assertFail("ndx >= 0 && ndx < c->length", "hcBackend.h", 187, NULL);
    return *((unsigned short *)chain + ndx + 1);
}

void *HC_getUDChain(void *beInfo, int instrNdx, int use)
{
    HCBackendInfo_s *info = (HCBackendInfo_s *)beInfo;
    HCInstr_s *chainByInstr;
    UDChainStub_s *chain;
    int nUses;
    int uses[4];
    int i;

    chainByInstr = info->instructions;
    chain = info->chains.udChains[instrNdx];
    if (chainByInstr == NULL)
        HC_assertFail("lst", "hcCompiler.c", 4777, NULL);
    if (chain != NULL)
    {
        nUses = HC_getUses(&chainByInstr[instrNdx], uses);
        for (i = 0; i < nUses; ++i)
        {
            DUChain_s *link = chain->chains[i];
            if (link != NULL && use == uses[i])
                return link;
        }
    }
    return NULL;
}
