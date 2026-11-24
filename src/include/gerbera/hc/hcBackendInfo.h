/* Per-instruction liveness and def-use information passed to backend fixers
 * and code generation. Offsets below describe the 32-bit reference ABI.
 */
#ifndef GERBERA_HC_BACKEND_INFO_H
#define GERBERA_HC_BACKEND_INFO_H

#include "gerbera/hc/hcOpcode.h"
#include "gerbera/hc/hcDUChain.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Anonymous DU/UD chain container: Nokia TI=0x19DF, UIQ 1.4.8 TI=0x19EA. */
typedef struct hcChains_s
{
    unsigned short *duChains;             /* +0 relative index (per instr)   */
    UDChainStub_s *const *udChains;        /* +4 per instr: per-use chains    */
} hcChains_s;

/* Anonymous computeBackEndInfo result: Nokia TI=0x19E5, UIQ TI=0x1ABE.
 * Member names and pointer types agree in both CodeView records. */
typedef struct HCBackendInfo_s
{
    HCInstr_s *instructions;              /* +0                               */
    unsigned int *freeRegMasks;           /* +4                               */
    unsigned int *liveRegs;               /* +8                               */
    hcChains_s chains;                    /* +12                              */
} HCBackendInfo_s;

unsigned int HC_getFreeHardRegs(void *beInfo, int ndx);
void *HC_getDUChain(void *beInfo, int instrNdx);
void *HC_getUDChain(void *beInfo, int instrNdx, int use);

/* hcDUChain.c: DUChains_getDUChain - chains->duChains[which] is a ushort
 * offset backwards into the same array (0 = no chain). */
void *DUChains_getDUChain(hcChains_s *chains, int which);

int HCChain_getLength(void *chain);
int HCChain_getValue(void *chain, int ndx);

int HC_max(int a, int b);

#ifdef __cplusplus
}
#endif

#endif /* GERBERA_HC_BACKEND_INFO_H */
