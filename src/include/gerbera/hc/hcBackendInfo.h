/* hcBackendInfo.h - per-compile backend analysis info (hcBackend.h of the
 * original: HC_getFreeHardRegs / HC_getDUChain / HC_getUDChain / HCChain_*).
 *
 * The original's cv_anon_19E5 ("compInfo" / "beInfo") is filled by
 * computeBackEndInfo() (basic blocks + liveness + free-hard-reg masks +
 * DU chains; lives in hcCompiler.c, ported with the compiler core) and is
 * passed to the backend fixers and codegen.
 *
 * Layout pinned from the decompiled accessors (docs/libGLES_CM.dll.c):
 *   +0  lst                HCInstr_s*   (HC_getUDChain asserts non-NULL)
 *   +4  freeHardRegMasks   int*         (per instruction; HC_getFreeHardRegs)
 *   +8  liveRegs           unsigned*
 *   +12 chains.duChains    unsigned short*  (relative-offset index; asserted
 *                           non-NULL by HC_getDUChain, entries 0 = no chain)
 *   +16 chains.udChains    void**       (per instruction: per-use chain array;
 *                           HC_getUDChain dereferences it unconditionally)
 *
 * DUChain_s layout (HCChain_* accessors): { unsigned short length;
 * unsigned short values[length]; } - chain = &duChains[-offset].
 */
#ifndef GERBERA_HC_BACKEND_INFO_H
#define GERBERA_HC_BACKEND_INFO_H

#include "gerbera/hc/hcOpcode.h"

#ifdef __cplusplus
extern "C" {
#endif

/* cv_anon_19DF - DU/UD chain containers (du first: HC_getDUChain passes
 * &info->chains to DUChains_getDUChain, which reads ->duChains at +0). */
typedef struct hcChains_s
{
    unsigned short *duChains;             /* +0 relative index (per instr)   */
    void **udChains;                      /* +4 per instr: per-use chains    */
} hcChains_s;

/* cv_anon_19E5 - computeBackEndInfo result (see header comment). */
typedef struct HCBackendInfo_s
{
    HCInstr_s *lst;                       /* +0                               */
    int *freeHardRegMasks;                /* +4                               */
    unsigned int *liveRegs;               /* +8                               */
    hcChains_s chains;                    /* +12                              */
} HCBackendInfo_s;

/* Accessors (hcCompiler.c / hcBackend.h originals). */
unsigned int HC_getFreeHardRegs(void *beInfo, int ndx);
void *HC_getDUChain(void *beInfo, int instrNdx);
void *HC_getUDChain(void *beInfo, int instrNdx, int use);

/* hcDUChain.c: DUChains_getDUChain - chains->duChains[which] is a ushort
 * offset backwards into the same array (0 = no chain). */
void *DUChains_getDUChain(hcChains_s *chains, int which);

/* HCChain_* (hcBackend.h inline originals). */
int HCChain_getLength(void *chain);
int HCChain_getValue(void *chain, int ndx);

int HC_max(int a, int b);

/* computeBackEndInfo (hcCompiler.c) - ported with the hcCompiler analysis
 * core (HC_buildBasicBlocks / liveness / getFreeHardRegMasks /
 * computeDUChains). Declared here so backends can link against it later. */

#ifdef __cplusplus
}
#endif

#endif /* GERBERA_HC_BACKEND_INFO_H */
