/* hcDUChain.h - def-use and use-def chain records (hcDUChain.h/.c).
 *
 * 32-bit layouts from CodeView LF_STRUCTURE TI=0x19D7 / TI=0x19DB:
 *   DUChain_s:     { unsigned short length; unsigned short values[1]; }
 *   UDChainStub_s: { DUChain_s *chains[1]; }
 */
#ifndef GERBERA_HC_DUCHAIN_H
#define GERBERA_HC_DUCHAIN_H

#include "gerbera/hc/hcMemory.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct DUChain_s
{
    unsigned short length;
    unsigned short values[1];
} DUChain_s;

typedef struct UDChainStub_s
{
    DUChain_s *chains[1];
} UDChainStub_s;

DUChain_s *DUChain_create(HCMemory_s *mgr, int length);
UDChainStub_s *UDChainStub_create(HCMemory_s *mgr, int nChains);

#ifdef __cplusplus
}
#endif

#endif /* GERBERA_HC_DUCHAIN_H */
