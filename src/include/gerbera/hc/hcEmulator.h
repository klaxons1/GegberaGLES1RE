/* hcEmulator.h - HC IR constant-evaluation & operand-substitution helpers
 * (hcEmulator.c of the original).
 */
#ifndef GERBERA_HC_EMULATOR_H
#define GERBERA_HC_EMULATOR_H

#include "gerbera/hc/hcOpcode.h"

#ifdef __cplusplus
extern "C" {
#endif

int emulator_evalConditional(HCOP_e op, unsigned int s0, unsigned int s1);
int emulator_evalALUOp(HCOP_e op, unsigned int s0, unsigned int s1);
HCInstr_s emulator_substitute(HCInstr_s *instr, int cReg, int cVal);

#ifdef __cplusplus
}
#endif

#endif /* GERBERA_HC_EMULATOR_H */
