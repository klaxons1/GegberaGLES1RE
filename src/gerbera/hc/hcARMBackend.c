/* hcARMBackend.c - 32-bit ARM (A32) JIT backend (hcARMBackend.c of original).
 *
 * Complete port of the ARM machine-code generator, pre-regalloc fixer,
 * immediate recognizer (rotated imm8 + multi-chunk MOV/ORR & MVN/BIC
 * synthesis), barrel-shifter operand fusion, CPSR flag-setting CMP/TST
 * elision, MUL Rd!=Rm register allocator/spiller, and backendDecl vtable
 * from docs/libgles_cm.dll.epoc.c (sub_23B4C..sub_25484) and image bytes in
 * docs/libgles_cm.dll.epoc (VA 0x36EB8..0x36F9B, file = VA - 0x7F64).
 */
#include "gerbera/hc/hcBackend.h"
#include "gerbera/hc/hcBackendInfo.h"
#include "gerbera/hc/hcMemory.h"
#include "gerbera/hc/hcOpcode.h"

#include <stddef.h>
#include <string.h>

/* --- static tables (verified directly from docs/libgles_cm.dll.epoc) ------ */

/* VA 0x36EB8: ARM condition codes (LT=11, CC=3, GT=12, HI=8) for
 * HCOP_MAXS, HCOP_MAXU, HCOP_MINS, HCOP_MINU. */
static const unsigned char armMinMaxCond[4] = { 11, 3, 12, 8 };

/* VA 0x36EBC: per-opcode metadata word (55 entries):
 *   bits 0..4   (w & 0x1F)        : ARM data-processing opcode
 *   bits 5..7   ((w >> 5) & 7)    : immediate class for canCreateImmediate
 *   bits 8..9   ((w >> 8) & 3)    : CPSR flag class (0=none, 1=NZ, 2=NZCV)
 *   bit  10     ((w >> 10) & 1)   : barrel-shifter / 0xFFFF-mask ALU op
 *   bits 11..14 ((w >> 11) & 0xF) : codegen switch dispatch class (1..12) */
static const unsigned short armOpcodeInfo[55] =
{
    0x1624u, 0x1520u, 0x195Fu, 0x195Fu, 0x152Eu, 0x195Fu, 0x195Fu, 0x195Fu,
    0x283Fu, 0x283Fu, 0x283Fu, 0x283Fu, 0x207Fu, 0x207Fu, 0x090Du, 0x090Fu,
    0x48DFu, 0x152Cu, 0x195Fu, 0x195Fu, 0x1622u, 0x1623u, 0x1521u, 0x409Fu,
    0x40BFu, 0x409Fu, 0x409Fu, 0x00FFu, 0x00FFu, 0x40BFu, 0x40FFu, 0x40BFu,
    0x409Fu, 0x00FFu, 0x40BFu, 0x40FFu, 0x40FFu, 0x40FFu, 0x40FFu, 0x50DFu,
    0x30DFu, 0x383Fu, 0x383Fu, 0x383Fu, 0x383Fu, 0x383Fu, 0x383Fu, 0x383Fu,
    0x383Fu, 0x383Fu, 0x383Fu, 0x383Fu, 0x383Fu, 0x58DFu, 0x60DFu
};

/* VA 0x36F2A: maps HC hard register 0..13 -> ARM register r0..r12, lr(r14). */
static const unsigned char armRegRemap[14] =
{
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 14
};

/* VA 0x36F38: S-bit auto-enable table indexed by ARM DP opcode (TST..CMN=1). */
static const unsigned char armAutoSetFlags[16] =
{
    0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 0, 2, 0, 2
};

/* VA 0x36F48: preRegallocFixer scaled-memop lowering for op 27..35
 * (bits 6..7 = index shift 0/1/2, bits 0..5 = unscaled HCOP). */
static const unsigned char armMemScaleFix[9] =
{
    0x59u, 0x5Au, 0x1Du, 0x9Du, 0x1Fu, 0x20u, 0x60u, 0x22u, 0xA2u
};

/* VA 0x36F51: load/store descriptor for op 23..35 (HCOP_LBS..HCOP_SD4). */
static const unsigned char armMemDesc[13] =
{
    0x03u, 0x01u, 0x07u, 0x05u, 0x17u, 0x15u, 0x09u,
    0x29u, 0x00u, 0x04u, 0x14u, 0x08u, 0x28u
};

/* VA 0x36F5E: ARM condition codes for HCOP_CBA..HCOP_CBNEQ (op 41..52). */
static const unsigned char armBranchCond[12] =
{
    1, 0, 10, 2, 12, 8, 13, 9, 11, 3, 0, 1
};

/* --- internal state structs ----------------------------------------------- */

typedef struct ArmEmitCtx_s
{
    unsigned int *code;
    int count;
    unsigned int regMask;
} ArmEmitCtx_s;

/* Scratch register state for MUL Rd==Rm==Rs temporary allocation (sub_23B4C). */
typedef struct ArmScratchState_s
{
    unsigned int freeSpillSlots;          /* +0: 1-bit mask (slot 0 -> [r0+60]) */
    unsigned int freeHardRegs;            /* +4: per-instr free hard reg mask   */
    unsigned char regStatus[16];          /* +8: status per ARM register 0..14  */
} ArmScratchState_s;

typedef struct ArmLabelNode_s
{
    struct ArmLabelNode_s *next;
    int label;
    int offset;
} ArmLabelNode_s;

/* --- scratch register state & barrel-shift helpers ------------------------ */

/* sub_23B4C */
static void armScratchInit(ArmScratchState_s *st)
{
    int i;

    memset(st, 0, sizeof(*st));
    st->regStatus[0] = 0xBFu;
    for (i = 1; i < 14; ++i)
        st->regStatus[i] = 0x3Fu;
    st->freeHardRegs = 0;
    st->freeSpillSlots = 1u;
}

/* sub_23B84 */
static int armAllocSpillSlot(ArmScratchState_s *st)
{
    unsigned int m = st->freeSpillSlots;
    int slot;

    for (slot = 0; slot < 1; ++slot)
    {
        if ((m & (1u << slot)) != 0)
        {
            st->freeSpillSlots = m & ~(1u << slot);
            return slot;
        }
    }
    return -1;
}

/* sub_23BB8: encode immediate shift field (bits 5..11) for shiftType 1..4. */
static unsigned int armEncodeImmShift(int shiftType, int amount)
{
    if (shiftType != 0)
    {
        unsigned int imm5 = ((unsigned int)amount & 0x1Fu) << 7;
        unsigned int st = 0;
        switch (shiftType)
        {
        case 2: st = 1u; break; /* LSR */
        case 3: st = 2u; break; /* ASR */
        case 4: st = 3u; break; /* ROR */
        default: break;         /* LSL */
        }
        return imm5 + 32u * st;
    }
    return 0;
}

/* --- ARM instruction emitters --------------------------------------------- */

/* sub_23BF8: emit single-data-transfer (LDR/STR/LDRB/STRB/LDRH/STRH/LDRSB/LDRSH).
 * Verified against machine code at VA 0x23BF8..0x23D78 (including the
 * +0x02000000 I-bit at VA 0x23D14 for register-indexed word/byte transfers). */
static unsigned int armEmitMemOp(ArmEmitCtx_s *ctx, int cond, int opKind,
                                 int regsPacked, int offsetOrShift, int flags)
{
    unsigned int sizeField = ((unsigned int)flags >> 1) & 3u;
    int signedFlag = (flags & 8) != 0;
    unsigned int rd = ((unsigned int)(unsigned short)regsPacked) >> 12;
    unsigned int rn = (((unsigned int)regsPacked) >> 8) & 0xFu;
    unsigned int rm = (((unsigned int)regsPacked) >> 4) & 0xFu;
    unsigned int word = ((unsigned int)cond << 28) | (rn << 16) | (rd << 12);
    int isExtraLoadStore = 0;
    unsigned int result;

    ctx->regMask |= (1u << rd) | (1u << rn);
    if (opKind == 19)
        word += 0x00100000u; /* L = 1 (load) */

    if (signedFlag || sizeField == 1u)
    {
        /* ARMv4 halfword / signed-byte transfer: 0x00000090 | S<<6 | H<<5 */
        word += 0x90u;
        if (sizeField == 1u)
            word += 0x20u;   /* H = 1 (halfword) */
        if (signedFlag)
            word += 0x40u;   /* S = 1 (sign-extend) */
        isExtraLoadStore = 1;
    }
    else
    {
        /* Word / unsigned-byte transfer: 0x04000000 | (B<<22) */
        word += 0x04000000u;
        if (sizeField == 0u)
            word += 0x00400000u; /* B = 1 (byte) */
    }
    word += 0x01000000u; /* P = 1 (pre-indexed) */

    if ((flags & 1) == 1)
    {
        /* Register offset */
        ctx->regMask |= (1u << rm);
        result = word + rm;
        if ((flags & 0x40) != 0)
            result += 0x00800000u; /* U = 1 */
        if (isExtraLoadStore == 0)
        {
            unsigned int shiftKind = ((unsigned int)flags >> 7) & 7u;
            result = (result + 0x02000000u) + armEncodeImmShift((int)shiftKind, offsetOrShift);
        }
    }
    else
    {
        /* Immediate offset */
        int off = offsetOrShift;
        if ((flags & 0x40) == 0)
            off = -off;
        if (off < 0)
            off = -off;
        else
            word += 0x00800000u; /* U = 1 */

        if (isExtraLoadStore != 0)
            result = word + 0x00400000u + (unsigned int)(off & 0xF) + 16u * (unsigned int)(off & 0xF0);
        else
            result = word + (unsigned int)off;
    }

    ctx->code[ctx->count++] = result;
    return result;
}

/* sub_23D7C */
static unsigned char *HCARMBackend_targetRegisterMapping(void)
{
    return (unsigned char *)armRegRemap;
}

/* sub_23D84 (3rd arg `excludeMask` in r2 verified from VA 0x23DA8 & 0x251E4). */
static int armAllocScratchReg(ArmScratchState_s *st, ArmEmitCtx_s *ctx,
                              unsigned int excludeMask)
{
    const unsigned char *remap = HCARMBackend_targetRegisterMapping();
    int i;

    for (i = 1; i < 14; ++i)
    {
        int r = remap[i];
        unsigned int bit = 1u << r;
        if ((excludeMask & bit) == 0
            && (st->regStatus[r] & 0x80u) == 0
            && (st->freeHardRegs & bit) != 0)
        {
            st->freeHardRegs &= ~bit;
            st->regStatus[r] = 0xFFu;
            return r;
        }
    }
    for (i = 1; i < 14; ++i)
    {
        int r = remap[i];
        unsigned int bit = 1u << r;
        if ((excludeMask & bit) == 0 && (st->regStatus[r] & 0x80u) == 0)
        {
            int slot = armAllocSpillSlot(st);
            st->regStatus[r] = (unsigned char)((st->regStatus[r] & 0x40u) | (unsigned int)slot | 0x80u);
            /* Spill `r` to [r0 + 60 + 4*slot] (STR r, [r0, #60]). */
            armEmitMemOp(ctx, 14, 21, (unsigned short)(r << 12), 4 * slot + 60, 68);
            return r;
        }
    }
    return -1;
}

/* sub_23E8C */
static void armFreeScratchReg(ArmScratchState_s *st, ArmEmitCtx_s *ctx, int r)
{
    unsigned char status = st->regStatus[r];

    if ((status & 0x40u) != 0)
    {
        st->regStatus[r] = status & 0xBFu;
    }
    else
    {
        int slot = status & 0x3F;
        /* Reload `r` from [r0 + 60 + 4*slot] (LDR r, [r0, #60]). */
        armEmitMemOp(ctx, 14, 19, (unsigned short)(r << 12), 4 * slot + 60, 68);
        st->freeSpillSlots |= (1u << slot);
    }
    st->regStatus[r] &= 0x7Fu;
}

/* sub_23F04: emit branch `B<cond>` placeholder with labelId in bits 0..23. */
static void armEmitBranch(ArmEmitCtx_s *ctx, int cond, int labelId)
{
    unsigned int word = ((unsigned int)cond << 28) | 0x0A000000u | ((unsigned int)labelId & 0x00FFFFFFu);
    ctx->code[ctx->count++] = word;
}

/* sub_23F2C: emit ARM data-processing instruction.
 * Verified against machine code at VA 0x23F2C..0x2408C (including `+ rm` at
 * VA 0x2406C after sub_23BB8 in the immediate-shifted-register path). */
static void armEmitDataProc(ArmEmitCtx_s *ctx, int cond, int armOp,
                            int regsPacked, unsigned int immOrAmt, int modeFlags)
{
    unsigned int word = ((unsigned int)cond << 28) + ((unsigned int)armOp << 21);
    int opndMode = modeFlags & 3;
    int shiftType = ((unsigned int)modeFlags >> 2) & 7u;
    unsigned int rd = (((unsigned int)regsPacked) >> 12) & 0xFu;
    unsigned int rn = (((unsigned int)regsPacked) >> 8) & 0xFu;
    unsigned int rm = (((unsigned int)regsPacked) >> 4) & 0xFu;
    unsigned int rs = ((unsigned int)regsPacked) & 0xFu;
    unsigned int shifterOperand;

    if (armAutoSetFlags[armOp] == 1u || (modeFlags & 0x20) != 0)
        word += 0x00100000u; /* S = 1 */

    word += (rn << 16) + (rd << 12);
    ctx->regMask |= (1u << rd) | (1u << rn);

    if (opndMode != 0)
    {
        ctx->regMask |= (1u << rm);
        if (opndMode == 1)
        {
            /* Register-shifted register: Rm, <shift> Rs */
            unsigned int st = 0;
            ctx->regMask |= (1u << rs);
            switch (shiftType)
            {
            case 2: st = 1u; break;
            case 3: st = 2u; break;
            case 4: st = 3u; break;
            default: break;
            }
            shifterOperand = rm + (rs << 8) + 16u + 32u * st;
        }
        else
        {
            /* Immediate-shifted (or unshifted when shiftType==0) register Rm */
            shifterOperand = armEncodeImmShift(shiftType, (int)immOrAmt) + rm;
        }
    }
    else
    {
        /* 8-bit rotated immediate (I = 1 -> 0x02000000) */
        unsigned int val = immOrAmt;
        unsigned int rot = 0;
        if (immOrAmt != 0 && (immOrAmt & 3u) == 0)
        {
            do
            {
                val >>= 2;
                rot += 2u;
            }
            while ((val & 3u) == 0);
            if (rot != 0)
                rot = 32u - rot;
        }
        shifterOperand = (val + ((rot >> 1) << 8)) | 0x02000000u;
    }

    ctx->code[ctx->count++] = word + shifterOperand;
}

/* sub_24090: emit block data transfer (STMDB / LDMIA for prologue/epilogue). */
static void armEmitBlockMem(ArmEmitCtx_s *ctx, int cond, int opKind, int baseReg,
                            unsigned int regList, int postIndex, int writeBack, int upBit)
{
    unsigned int word = ((unsigned int)cond << 28) + 0x08000000u + ((unsigned int)baseReg << 16);

    if (postIndex == 0)
        word += 0x01000000u; /* P = 1 */
    if (writeBack == 1)
        word += 0x00200000u; /* W = 1 */
    if (upBit == 1)
        word += 0x00800000u; /* U = 1 */
    if (opKind == 20)
        word += 0x00100000u; /* L = 1 (LDM) */

    ctx->regMask |= regList | (1u << baseReg);
    ctx->code[ctx->count++] = word + regList;
}

/* --- simple vtable methods ------------------------------------------------ */

/* sub_24100 */
static int HCARMBackend_internalStackSpace(void)
{
    return 60;
}

/* sub_24108 */
static int HCARMBackend_numHardRegs(void)
{
    return 14;
}

/* sub_24110 */
static int HCARMBackend_isNative(void)
{
    return 1;
}

/* sub_24118 */
static void HCARMBackend_emulate(void (*program)(void *), void *stackPtr)
{
    program(stackPtr);
}

/* sub_25480: bare `bx lr` in slot 9 (+36). */
static void HCARMBackend_reserved9(void)
{
}

/* --- immediate recognition & preRegallocFixer ----------------------------- */

/* sub_24124: count how many even-aligned 8-bit chunks are needed to cover `val`. */
static int armCountImmChunks(unsigned int val)
{
    int chunks;
    int bit = 0;

    if (val == 0)
        return 1;
    chunks = 0;
    do
    {
        int nonZero = (val & (3u << bit)) != 0;
        if (nonZero)
        {
            bit += 6;
            ++chunks;
        }
        bit += 2;
    }
    while (bit < 32);

    return chunks;
}

/* sub_24158: map HCOP shift/rotate opcode -> internal shiftType 1..4. */
static int armShiftTypeForOp(int op)
{
    switch (op)
    {
    case HCOP_ASR: return 3;
    case HCOP_LSL: return 1;
    case HCOP_ROR: return 4;
    default:       return 2; /* HCOP_LSR */
    }
}

/* sub_245C0 */
static int HCARMBackend_canCreateImmediate(HCInstr_s *instr)
{
    int op = HC_getOpcodeOper(instr);
    int imm = (int)instr->instr1;
    unsigned int immCls = ((unsigned int)armOpcodeInfo[op] >> 5) & 7u;

    switch (immCls)
    {
    case 0u:
        if (armCountImmChunks((unsigned int)imm) > 1
            && armCountImmChunks(~(unsigned int)imm) > 1)
        {
            return 0;
        }
        return 1;
    case 1u:
        return armCountImmChunks((unsigned int)imm) <= 1;
    case 2u:
    case 6u:
        return 1;
    case 4u:
        return (imm < 256 && imm > -256);
    case 5u:
        return (imm < 4096 && imm > -4096);
    default:
        return 0;
    }
}

/* sub_2417C */
static int HCARMBackend_preRegallocFixer(void *mgrVoid, HCInstr_s **dest,
                                         HCInstr_s *src, void *compHandle)
{
    HCArena_s *mgr = (HCArena_s *)mgrVoid;
    int maxR = 0;
    int scanIdx = 0;
    int inIdx = 0;
    int nextReg;
    int mark;
    HCInstr_s *out;
    int outIdx = 0;
    int prevOp;

    (void)compHandle;

    do
    {
        int uses[3];
        int nUses = HC_getUses(&src[scanIdx], uses);
        int k;
        int def;

        for (k = 0; k < nUses; ++k)
        {
            if (uses[k] > maxR)
                maxR = uses[k];
        }
        def = HC_getDef(&src[scanIdx]);
        if (def > maxR)
            maxR = def;
        prevOp = HC_getOpcodeOper(&src[scanIdx++]);
    }
    while (prevOp != HCOP_EOP);

    nextReg = maxR + 1;
    mark = HCArena_markBottom(mgr);
    out = (HCInstr_s *)HCArena_current(mgr);
    *dest = out;

    do
    {
        HCInstr_s *instr = &src[inIdx];
        int op = HC_getOpcodeOper(instr);

        HCArena_growCurrent(mgr, 8u);

        /* Scaled loads/stores (LW2S, LW2U, LD4, SW2, SD4 -> op 27..35):
         * scale immediate offsets by 2 or 4, and expand halfword scaled-index
         * (shift == 1) into LSL tmp, s1, #1 + unscaled LWS/LWU/SW. */
        if ((unsigned int)(op - HCOP_LW2S) < 9u)
        {
            unsigned int fix = armMemScaleFix[op - HCOP_LW2S];
            unsigned int shift = fix >> 6;
            if (shift != 0)
            {
                int baseOp = (int)(fix & 0x3Fu);
                int s0 = (int)(instr->instr0 >> 21);
                int dst = (int)((instr->instr0 >> 10) & 0x7FFu);
                if ((instr->instr0 & 0x100u) != 0)
                {
                    out[outIdx++] = HC_genOp3Imm(baseOp, dst, s0,
                                                 (int)(instr->instr1 << shift));
                    goto next_instr;
                }
                if (shift == 1u)
                {
                    int tmp = nextReg++;
                    out[outIdx++] = HC_genOp3Imm(HCOP_LSL, tmp,
                                                 (int)(instr->instr1 & 0x7FFu), 1);
                    HCArena_growCurrent(mgr, 8u);
                    out[outIdx++] = HC_genOp3(baseOp, dst, s0, tmp);
                    goto next_instr;
                }
            }
        }

        if (op == HCOP_ROR)
        {
            if ((instr->instr0 & 0x100u) != 0 && instr->instr1 == 0)
            {
                int dst = (int)((instr->instr0 >> 10) & 0x7FFu);
                int s0 = (int)(instr->instr0 >> 21);
                out[outIdx++] = HC_genOp2(HCOP_MOV, dst, s0);
                goto next_instr;
            }
        }
        else if (op == HCOP_ROL)
        {
            /* ARM has ROR but no ROL: lower ROL(s0, k) into LSR + LSL + OR. */
            int s0 = (int)(instr->instr0 >> 21);
            int hasImm = (instr->instr0 & 0x100u) != 0;
            int dst = (int)((instr->instr0 >> 10) & 0x7FFu);

            if (hasImm != 0)
            {
                int tmp = nextReg++;
                int amt = (int)instr->instr1;
                HCArena_growCurrent(mgr, 16u);
                if (amt != 0)
                {
                    out[outIdx++] = HC_genOp3Imm(HCOP_LSR, tmp, s0, 32 - amt);
                    out[outIdx++] = HC_genOp3Imm(HCOP_LSL, dst, s0, amt);
                    out[outIdx++] = HC_genOp3(HCOP_OR, dst, dst, tmp);
                }
                else
                {
                    out[outIdx++] = HC_genOp2(HCOP_MOV, dst, s0);
                }
            }
            else
            {
                int s1 = (int)(instr->instr1 & 0x7FFu);
                int tmpShifted = nextReg;
                int tmpInvAmt = nextReg + 1;
                nextReg += 2;
                HCArena_growCurrent(mgr, 24u);
                out[outIdx++] = HC_genOp3Imm(HCOP_SUBR, tmpInvAmt, s1, 32);
                out[outIdx++] = HC_genOp3Imm(HCOP_AND, tmpInvAmt, tmpInvAmt, 31);
                out[outIdx++] = HC_genOp3(HCOP_LSR, tmpShifted, s0, tmpInvAmt);
                out[outIdx++] = HC_genOp3(HCOP_LSL, dst, s0, s1);
                out[outIdx++] = HC_genOp3(HCOP_OR, dst, dst, tmpShifted);
            }
            goto next_instr;
        }

        /* Synthesize common 12-bit / 16-bit masks (0xFFF / 0xFFFF) via
         * MOVN tmp, #0 (= 0xFFFFFFFF) + LSR tmp, tmp, #20 / #16. */
        if ((instr->instr0 & 0x100u) != 0
            && ((armOpcodeInfo[op] >> 10) & 1u) == 1u
            && (instr->instr1 == 0xFFFFu || instr->instr1 == 0xFFFu))
        {
            int tmp = nextReg++;
            int lsrAmt = (instr->instr1 == 0xFFFu) ? 20 : 16;
            int dst = (int)((instr->instr0 >> 10) & 0x7FFu);
            int s0 = (int)(instr->instr0 >> 21);

            HCArena_growCurrent(mgr, 16u);
            out[outIdx++] = HC_genOp2Imm(HCOP_MOVN, tmp, 0);
            out[outIdx++] = HC_genOp3Imm(HCOP_LSR, tmp, tmp, lsrAmt);
            out[outIdx++] = HC_genOp3(op, dst, s0, tmp);
        }
        else
        {
            out[outIdx++] = *instr;
        }

    next_instr:
        prevOp = HC_getOpcodeOper(&src[inIdx++]);
    }
    while (prevOp != HCOP_EOP);

    return mark;
}

/* --- codegen helpers ------------------------------------------------------ */

/* sub_24674 */
static void armGenLoadStore(ArmEmitCtx_s *ctx, int cond, HCInstr_s *instr)
{
    int op = HC_getOpcodeOper(instr);
    unsigned int desc = armMemDesc[op - HCOP_LBS];
    unsigned int rdField = ((instr->instr0 >> 10) & 0x7FFu) << 12;
    unsigned int rn = instr->instr0 >> 21;
    int opKind = ((desc & 1u) != 0) ? 19 : 21;
    int flagsBase = (int)(2u * ((desc >> 2) & 3u) + 8u * ((desc >> 1) & 1u));
    int offsetOrShift = -1;
    int regsPacked = (int)(rdField + (rn << 8));
    int flags;

    if ((instr->instr0 & 0x100u) != 0)
    {
        int imm = (int)instr->instr1;
        offsetOrShift = (imm < 0) ? -imm : imm;
        flags = flagsBase + ((imm >= 0) ? 64 : 0);
    }
    else
    {
        int isScaledWord = (desc & 0x20u) != 0;
        flags = flagsBase + (isScaledWord ? 128 : 0) + 65;
        regsPacked += 16 * (int)(instr->instr1 & 0x7FFu);
        if (isScaledWord)
            offsetOrShift = 2;
    }

    armEmitMemOp(ctx, cond, opKind, regsPacked, offsetOrShift, flags);
}

/* sub_24734: MAXS, MAXU, MINS, MINU. */
static void armGenMinMax(ArmEmitCtx_s *ctx, HCInstr_s *instr)
{
    int dst = (int)((instr->instr0 >> 10) & 0x7FFu);
    int op = HC_getOpcodeOper(instr);
    int s0 = (int)(instr->instr0 >> 21);
    int hasImm = (instr->instr0 & 0x100u) != 0;
    unsigned int immVal = instr->instr1;
    int kind = op - HCOP_MAXS;
    unsigned int opndImm;
    unsigned short cmpRm;
    int modeFlags;

    if (hasImm != 0)
    {
        /* Fast path: MAXS dst, s0, #0 -> BIC dst, s0, s0, ASR #31 */
        if (op == HCOP_MAXS && immVal == 0)
        {
            armEmitDataProc(ctx, 14, 14,
                            (unsigned short)((dst << 12) + (s0 << 8) + 16 * s0),
                            31u, 14);
            return;
        }
        opndImm = immVal;
        cmpRm = 0;
        modeFlags = 0;
        if (dst != s0)
        {
            armEmitDataProc(ctx, 14, 13,
                            (unsigned short)((dst << 12) + 16 * s0),
                            0xFFFFFFFFu, 2);
        }
    }
    else
    {
        int s1 = (int)(immVal & 0x7FFu);
        opndImm = 0xFFFFFFFFu;
        cmpRm = (unsigned short)s0;
        modeFlags = 2;
        if (dst != s1)
        {
            cmpRm = (unsigned short)s1;
            if (dst != s0)
            {
                armEmitDataProc(ctx, 14, 13,
                                (unsigned short)((dst << 12) + 16 * s0),
                                0xFFFFFFFFu, 2);
            }
        }
        else if (dst == s0)
        {
            return;
        }
    }

    /* CMP dst, opnd; MOV<cond> dst, opnd */
    armEmitDataProc(ctx, 14, 10,
                    (unsigned short)((dst << 8) + 16 * cmpRm),
                    opndImm, modeFlags);
    armEmitDataProc(ctx, armMinMaxCond[kind], 13,
                    (unsigned short)((dst << 12) + 16 * cmpRm),
                    opndImm, modeFlags);
}

/* sub_2488C: standard 3-operand / compare ARM DP instruction. */
static void armGenALU(ArmEmitCtx_s *ctx, HCInstr_s *instr, int armOp, int dst)
{
    int hasImm = (instr->instr0 & 0x100u) != 0;
    int s0 = (int)(instr->instr0 >> 21);
    int rm = hasImm ? 0 : (int)(instr->instr1 & 0x7FFu);
    unsigned int immVal = hasImm ? instr->instr1 : 0xFFFFFFFFu;
    int modeFlags = hasImm ? 0 : 2;
    unsigned short regsPacked = (unsigned short)((dst << 12) + (s0 << 8) + 16 * rm);

    armEmitDataProc(ctx, 14, armOp, regsPacked, immVal, modeFlags);
}

/* sub_248F4: shifted-operand ARM DP instruction (standalone shift or fused ALU). */
static void armGenShiftedALU(ArmEmitCtx_s *ctx, HCInstr_s *shiftInstr,
                             int armOp, int dst, int rn, int rm, int shiftType)
{
    int hasImm = (shiftInstr->instr0 & 0x100u) != 0;
    unsigned int immOrAmt = hasImm ? shiftInstr->instr1 : 0xFFFFFFFFu;
    int opndMode = hasImm ? 2 : 1;
    int rs = hasImm ? 0 : (int)(shiftInstr->instr1 & 0x7FFu);
    unsigned short regsPacked = (unsigned short)((dst << 12) + (rn << 8) + 16 * rm + rs);

    armEmitDataProc(ctx, 14, armOp, regsPacked, immOrAmt, opndMode + 4 * shiftType);
}

/* sub_2496C: conditional branch with CPSR S-bit fusion onto preceding ALU op. */
static void armGenCondBranch(ArmEmitCtx_s *ctx, HCInstr_s *instrs, int pc, void *info)
{
    HCInstr_s *instr = &instrs[pc];
    int op = HC_getOpcodeOper(instr);
    int hasImm = (instr->instr0 & 0x100u) != 0;
    int s0 = (int)(instr->instr0 >> 21);
    int dstLabel = (int)((instr->instr0 >> 10) & 0x7FFu);
    int tryFuseFlags = 0;

    if (hasImm != 0 && instr->instr1 == 0)
    {
        if (op != HCOP_CBA && op != HCOP_CBNA)
            tryFuseFlags = 1;
    }
    if (!tryFuseFlags
        && (op == HCOP_CBA || op == HCOP_CBNA)
        && hasImm == 0
        && s0 == (int)(instr->instr1 & 0x7FFu))
    {
        tryFuseFlags = 1;
    }

    if (tryFuseFlags && pc != 0 && ctx->count > 0)
    {
        int prevOp = HC_getOpcodeOper(&instrs[pc - 1]);
        unsigned int flagCls = ((unsigned int)armOpcodeInfo[prevOp] >> 8) & 3u;
        if (flagCls != 0)
        {
            if (flagCls != 1u
                || op == HCOP_CBEQ || op == HCOP_CBNEQ
                || op == HCOP_CBA  || op == HCOP_CBNA)
            {
                void *ud = HC_getUDChain(info, pc, s0);
                if (ud != NULL
                    && HCChain_getLength(ud) == 1
                    && HCChain_getValue(ud, 0) == pc - 1)
                {
                    /* Set S-bit on preceding ARM instruction to elide CMP/TST! */
                    ctx->code[ctx->count - 1] |= 0x00100000u;
                    armEmitBranch(ctx, armBranchCond[op - HCOP_CBA], dstLabel);
                    return;
                }
            }
        }
    }

    armGenALU(ctx, instr, (op == HCOP_CBA || op == HCOP_CBNA) ? 8 : 10, 0);
    armEmitBranch(ctx, armBranchCond[op - HCOP_CBA], dstLabel);
}

/* sub_24ABC */
static void armGrowCodeBuf(HCArena_s *mgr, int *allocated, int needed)
{
    if (needed - *allocated > 64)
    {
        HCArena_growUnaligned(mgr, 128u);
        *allocated += 128;
    }
}

/* --- HCARMBackend_codegen (sub_24AEC) ------------------------------------- */

static int HCARMBackend_codegen(void *mgrVoid, void **destination,
                                HCInstr_s *instrs, void *infoVoid, int spillStart)
{
    HCArena_s *mgr = (HCArena_s *)mgrVoid;
    HCBackendInfo_s *info = (HCBackendInfo_s *)infoVoid;
    int pc = 0;
    int deferredCount = 0;
    int deferredIdx = 0;
    unsigned int deferredWords[8];
    int deferredPCs[8];
    int running = 1;
    int allocated = 0;
    int topMark = HCArena_markTop(mgr);
    ArmLabelNode_s *labelHead = NULL;
    ArmLabelNode_s *labelTail = NULL;
    ArmEmitCtx_s ctx;
    ArmScratchState_s scratch;
    int bytesEmitted;
    int j;

    (void)spillStart;

    *destination = HCArena_current(mgr);
    ctx.code = (unsigned int *)(*destination);
    ctx.count = 0;
    ctx.regMask = 0;

    armScratchInit(&scratch);
    bytesEmitted = 8;
    armGrowCodeBuf(mgr, &allocated, 8);
    /* Prologue: STMFD sp!, {r0-r12, lr} (0xE92D5FFF). */
    armEmitBlockMem(&ctx, 14, 22, 13, 0x5FFFu, 0, 1, 0);

    while (running != 0)
    {
        HCInstr_s *instr = &instrs[pc];
        int op = HC_getOpcodeOper(instr);
        int prevCount = ctx.count;
        unsigned int cgClass;

        armGrowCodeBuf(mgr, &allocated, bytesEmitted + 16);

        if (deferredIdx < deferredCount && deferredPCs[deferredIdx] == pc)
        {
            ctx.code[ctx.count++] = deferredWords[deferredIdx];
            ++pc;
            ++deferredIdx;
            continue;
        }

        scratch.freeHardRegs = HC_getFreeHardRegs(info, pc) & 0xFFFFFFFEu;
        cgClass = ((unsigned int)armOpcodeInfo[op] >> 11) & 0xFu;

        switch (cgClass)
        {
        case 1u: /* HCOP_MOV, HCOP_MOVN */
            if ((instr->instr0 & 0x100u) != 0)
            {
                int imm = (int)instr->instr1;
                int dst = (int)((instr->instr0 >> 10) & 0x7FFu);
                if (op == HCOP_MOVN)
                    imm = ~imm;
                if ((unsigned int)(imm + 256) >= 0x200u)
                {
                    int posChunks = armCountImmChunks((unsigned int)imm);
                    int negImm = ~imm;
                    int negChunks = armCountImmChunks((unsigned int)negImm);
                    int useNeg = (posChunks > negChunks);
                    int armOp = useNeg ? 15 : 13; /* MVN : MOV */
                    int bit = 0;
                    int first = 1;
                    int rn = 0;

                    if (useNeg)
                        imm = negImm;
                    do
                    {
                        if (((unsigned int)imm & (3u << bit)) != 0)
                        {
                            if (!first)
                            {
                                armOp = useNeg ? 14 : 12; /* BIC : ORR */
                                rn = dst;
                            }
                            first = 0;
                            armEmitDataProc(&ctx, 14, armOp,
                                            (unsigned short)((dst << 12) + (rn << 8)),
                                            (unsigned int)imm & (255u << bit), 0);
                            bit += 6;
                        }
                        bit += 2;
                    }
                    while (bit < 32);
                }
                else
                {
                    int armOp = 13;
                    if (imm < 0)
                    {
                        armOp = 15;
                        imm = ~imm;
                    }
                    armEmitDataProc(&ctx, 14, armOp,
                                    (unsigned short)(dst << 12),
                                    (unsigned int)imm, 0);
                }
            }
            else
            {
                int dst = (int)((instr->instr0 >> 10) & 0x7FFu);
                int s0 = (int)(instr->instr0 >> 21);
                armEmitDataProc(&ctx, 14, armOpcodeInfo[op] & 0x1F,
                                (unsigned short)((dst << 12) + 16 * s0),
                                0xFFFFFFFFu, 2);
            }
            break;

        case 2u: /* ADD, AND, BIC, OR, SUB, SUBR, XOR */
            armGenALU(&ctx, instr, armOpcodeInfo[op] & 0x1F,
                      (int)((instr->instr0 >> 10) & 0x7FFu));
            break;

        case 3u: /* ASR, LSL, LSR, ROR (+ barrel-shifter consumer fusion) */
            if (deferredIdx == deferredCount)
            {
                void *du = HC_getDUChain(info, pc);
                if (du != NULL)
                {
                    int nUses = HCChain_getLength(du);
                    int hasImm = (instr->instr0 & 0x100u) != 0;
                    if ((unsigned int)nUses <= 8u
                        && !(hasImm != 0 && instr->instr1 == 0))
                    {
                        int shiftDst = (int)((instr->instr0 >> 10) & 0x7FFu);
                        int shiftS0 = (int)(instr->instr0 >> 21);
                        int shiftS1 = hasImm ? -1 : (int)(instr->instr1 & 0x7FFu);
                        int scan = pc + 1;
                        int ok = 1;
                        int u;

                        for (u = 0; u < nUses; ++u)
                        {
                            int usePC = HCChain_getValue(du, u);
                            HCInstr_s *useInstr = &instrs[usePC];
                            int useOp = HC_getOpcodeOper(useInstr);
                            void *ud;

                            if ((armOpcodeInfo[useOp] & 0x400u) == 0)
                            {
                                ok = 0;
                                break;
                            }
                            while (scan < usePC)
                            {
                                HCInstr_s *mid = &instrs[scan];
                                int midOp = HC_getOpcodeOper(mid);
                                int midDef = HC_getDef(mid);
                                if (midDef == shiftS0
                                    || (!hasImm && midDef == shiftS1)
                                    || midOp == HCOP_LABEL
                                    || HC_getOpcodeTypeOp(midOp) == HCOP_TYPE_BRANCH)
                                {
                                    ok = 0;
                                    break;
                                }
                                ++scan;
                            }
                            if (!ok)
                                break;
                            ud = HC_getUDChain(info, usePC, shiftDst);
                            if ((useInstr->instr0 & 0x100u) != 0
                                || ud == NULL
                                || HCChain_getLength(ud) != 1)
                            {
                                ok = 0;
                                break;
                            }
                        }

                        if (ok)
                        {
                            int shiftType = armShiftTypeForOp(op);
                            deferredIdx = 0;
                            for (u = 0; u < nUses; ++u)
                            {
                                int usePC = HCChain_getValue(du, u);
                                HCInstr_s *useInstr = &instrs[usePC];
                                int rn = (int)(useInstr->instr0 >> 21);
                                int useOp = HC_getOpcodeOper(useInstr);
                                int useDst = (int)((useInstr->instr0 >> 10) & 0x7FFu);
                                ArmEmitCtx_s tmpCtx;

                                if (rn == shiftDst)
                                {
                                    rn = (int)(useInstr->instr1 & 0x7FFu);
                                    if (useOp == HCOP_SUB)
                                        useOp = HCOP_SUBR;
                                    else if (useOp == HCOP_SUBR)
                                        useOp = HCOP_SUB;
                                }
                                tmpCtx.code = &deferredWords[u];
                                tmpCtx.count = 0;
                                tmpCtx.regMask = 0;
                                armGenShiftedALU(&tmpCtx, instr,
                                                 armOpcodeInfo[useOp] & 0x1F,
                                                 useDst, rn, shiftS0, shiftType);
                                ctx.regMask |= tmpCtx.regMask;
                                deferredPCs[u] = usePC;
                            }
                            deferredCount = nUses;
                            break;
                        }
                    }
                }
            }

            if ((instr->instr0 & 0x100u) == 0 || instr->instr1 != 0)
            {
                int shiftType = armShiftTypeForOp(op);
                armGenShiftedALU(&ctx, instr, 13,
                                 (int)((instr->instr0 >> 10) & 0x7FFu),
                                 0, (int)(instr->instr0 >> 21), shiftType);
            }
            else
            {
                int dst = (int)((instr->instr0 >> 10) & 0x7FFu);
                int s0 = (int)(instr->instr0 >> 21);
                if (dst != s0)
                {
                    armEmitDataProc(&ctx, 14, 13,
                                    (unsigned short)((dst << 12) + 16 * s0),
                                    0xFFFFFFFFu, 2);
                }
            }
            break;

        case 4u: /* HCOP_MULS, HCOP_MULU (ARM `MUL Rd, Rm, Rs`, Rd != Rm) */
        {
            int rd = (int)((instr->instr0 >> 10) & 0x7FFu);
            int rm = (int)(instr->instr0 >> 21);
            int rs = (int)(instr->instr1 & 0x7FFu);
            int needScratch = 0;

            if (rm == rd)
            {
                if (rs == rd)
                {
                    rm = armAllocScratchReg(&scratch, &ctx, 1u << rd);
                    armEmitDataProc(&ctx, 14, 13,
                                    (unsigned short)((rm << 12) + 16 * rd),
                                    0xFFFFFFFFu, 2);
                    needScratch = 1;
                }
                else
                {
                    rs = (int)(instr->instr0 >> 21);
                    rm = (int)(instr->instr1 & 0x7FFu);
                }
            }

            /* 0xE0000090 | (rd << 16) | (rs << 8) | rm (-536870768 == 0xE0000090) */
            ctx.regMask |= (1u << rd) | (1u << rm) | (1u << (rs & 0xF));
            ctx.code[ctx.count++] = 0xE0000090u
                | ((unsigned int)rd << 16)
                | ((unsigned int)(rs & 0xF) << 8)
                | (unsigned int)(rm & 0xF);
            if (needScratch != 0)
                armFreeScratchReg(&scratch, &ctx, rm);
            break;
        }

        case 5u: /* HCOP_MAXS..HCOP_MINU */
            armGenMinMax(&ctx, instr);
            break;

        case 6u: /* HCOP_BRA */
            armEmitBranch(&ctx, 14, (int)((instr->instr0 >> 10) & 0x7FFu));
            break;

        case 7u: /* HCOP_CBA..HCOP_CBNEQ */
            armGenCondBranch(&ctx, instrs, pc, info);
            break;

        case 8u: /* Loads & stores */
            armGenLoadStore(&ctx, 14, instr);
            break;

        case 11u: /* HCOP_EOP */
            running = 0;
            break;

        case 12u: /* HCOP_LABEL */
        {
            ArmLabelNode_s *node = (ArmLabelNode_s *)HCArena_allocTop(mgr, (unsigned int)sizeof(ArmLabelNode_s));
            node->next = NULL;
            node->label = (int)instr->instr1;
            node->offset = ctx.count;
            if (labelHead != NULL)
                labelTail->next = node;
            else
                labelHead = node;
            labelTail = node;
            break;
        }

        default:
            break;
        }

        bytesEmitted += 4 * (ctx.count - prevCount);
        ++pc;
    }

    armGrowCodeBuf(mgr, &allocated, bytesEmitted + 8);
    /* Epilogue: LDMFD sp!, {r0-r12, pc} (0xE8BD9FFF). */
    armEmitBlockMem(&ctx, 14, 20, 13, 0x9FFFu, 1, 1, 1);

    /* Patch 24-bit signed word offsets into all emitted `B<cond>` words. */
    for (j = 0; j < ctx.count; ++j)
    {
        unsigned int w = ctx.code[j];
        if (((w >> 24) & 0xEu) == 0xAu)
        {
            int labelId = (int)(w & 0x00FFFFFFu);
            unsigned int hi = w & 0xFF000000u;
            ArmLabelNode_s *cur = labelHead;
            int byteDelta = -1;

            while (cur != NULL)
            {
                if (cur->label == labelId)
                {
                    byteDelta = 4 * (cur->offset - j);
                    break;
                }
                cur = cur->next;
            }
            /* ARM 0002542C..00025434 shifts the wrapped 32-bit displacement. */
            ctx.code[j] = hi | ((((unsigned int)byteDelta - 8u) << 6) >> 8);
        }
    }

    HCArena_releaseTop(mgr, topMark);
    return 4 * ctx.count;
}

/* --- vtable & factory (VA 0x36F6C, sub_25484) ----------------------------- */

static const HCBackend_s armBackendDecl =
{
    &HCARMBackend_canCreateImmediate,
    &HCARMBackend_internalStackSpace,
    NULL,
    &HCARMBackend_targetRegisterMapping,
    &HCARMBackend_numHardRegs,
    &HCARMBackend_preRegallocFixer,
    &HCARMBackend_codegen,
    NULL,                               /* no ARM postRegallocFixer */
    &HCARMBackend_emulate,
    &HCARMBackend_reserved9,            /* VA 0x25480 (`bx lr`)     */
    &HCARMBackend_isNative,
    NULL
};

HCBackend_s *HCARMBackend_create(void)
{
    return (HCBackend_s *)&armBackendDecl;
}
