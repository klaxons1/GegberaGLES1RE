/* hcBasicBlock.h - CFG basic block record & inline accessors (hcBasicBlock.h).
 *
 * 32-bit layout from CodeView LF_STRUCTURE TI=0x1A8D in docs/libGLES_CM.dll
 * (64 bytes on 32-bit targets):
 *   +0  unsigned char *pred
 *   +4  unsigned char *succ
 *   +8  HCBitSet_s *liveIn
 *   +12 HCBitSet_s *liveOut
 *   +16 HCBitSet_s *reachIn
 *   +20 HCBitSet_s *reachOut
 *   +24 HCBitSet_s *prsv
 *   +28 HCBitSet_s *gen
 *   +32 HCBitSet_s *dominators
 *   +36 unsigned int loopGroup
 *   +40 unsigned short numSuccs
 *   +42 unsigned short numPreds
 *   +44 unsigned short start
 *   +46 unsigned short end
 *   +48 unsigned short nesting
 *   +50 unsigned char padding[12]
 */
#ifndef GERBERA_HC_BASICBLOCK_H
#define GERBERA_HC_BASICBLOCK_H

#include "gerbera/hc/hcBitSet.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct BasicBlock_s
{
    unsigned char *pred;
    unsigned char *succ;
    HCBitSet_s *liveIn;
    HCBitSet_s *liveOut;
    HCBitSet_s *reachIn;
    HCBitSet_s *reachOut;
    HCBitSet_s *prsv;
    HCBitSet_s *gen;
    HCBitSet_s *dominators;
    unsigned int loopGroup;
    unsigned short numSuccs;
    unsigned short numPreds;
    unsigned short start;
    unsigned short end;
    unsigned short nesting;
    unsigned char padding[12];
} BasicBlock_s;

void BasicBlock_init(BasicBlock_s *self);
void BasicBlock_setStart(BasicBlock_s *self, int start);
void BasicBlock_setEnd(BasicBlock_s *self, int end);
int BasicBlock_getStart(const BasicBlock_s *self);
int BasicBlock_getEnd(const BasicBlock_s *self);
void BasicBlock_setDominators(BasicBlock_s *self, HCBitSet_s *dom);

#ifdef __cplusplus
}
#endif

#endif /* GERBERA_HC_BASICBLOCK_H */
