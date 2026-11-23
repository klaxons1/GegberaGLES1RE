/* hcX86Backend.c - IA-32 x86 JIT backend (hcX86Backend.c of the original).
 *
 * Complete port of the 32-bit x86 code generator, pre/post-regalloc fixers,
 * immediate pool (Imm32Set), stack-load fusion (checkLoad), EFLAGS tracker
 * (EncodeContext), and backendDecl vtable from docs/libGLES_CM.dll.c
 * (0044CC80..00450E70, .data 0047D814..0047D8DF) and embedded CodeView enums.
 */
#include "gerbera/hc/hcBackend.h"
#include "gerbera/hc/hcBackendInfo.h"
#include "gerbera/hc/hcMemory.h"
#include "gerbera/hc/hcOpcode.h"

#include <stddef.h>
#include <string.h>

/* --- enums (CodeView LF_ENUMERATE in docs/libGLES_CM.dll) ----------------- */

typedef enum X86Reg_e
{
    REG_EAX  = 0,
    REG_ECX  = 1,
    REG_EDX  = 2,
    REG_EBX  = 3,
    REG_ESP  = 4,
    REG_EBP  = 5,
    REG_ESI  = 6,
    REG_EDI  = 7,
    REG_LAST = 8
} X86Reg_e;

typedef enum X86Extend_e
{
    EXTEND_UNSIGNED = 0,
    EXTEND_SIGNED   = 1
} X86Extend_e;

typedef enum X86ALU_e
{
    X86_AND  = 0,
    X86_CMP  = 1,
    X86_OR   = 2,
    X86_SUB  = 3,
    X86_TEST = 4,
    X86_XOR  = 5
} X86ALU_e;

typedef enum X86UOP_e
{
    X86UOP_NEG  = 0,
    X86UOP_NOT  = 1,
    X86UOP_LAST = 2
} X86UOP_e;

typedef enum X86CMP_e
{
    X86CMP_EQ   = 0,
    X86CMP_GES  = 1,
    X86CMP_GEU  = 2,
    X86CMP_GTS  = 3,
    X86CMP_GTU  = 4,
    X86CMP_LES  = 5,
    X86CMP_LEU  = 6,
    X86CMP_LTS  = 7,
    X86CMP_LTU  = 8,
    X86CMP_NE   = 9,
    X86CMP_LAST = 10
} X86CMP_e;

/* --- internal types ------------------------------------------------------- */

typedef struct ALUOpEncode_s
{
    unsigned char normOp;
    unsigned char r32Imm32;
    unsigned char r32Imm8;
    unsigned char eaxImm32;
    unsigned char opField;
    unsigned char swapOperands;
} ALUOpEncode_s;

typedef struct EncodeContext_s
{
    unsigned char *out;
    unsigned int flags;
    int reg;
} EncodeContext_s;

#define X86_MAX_CONSTANTS 8

typedef struct Imm32Set_s
{
    int nValues;
    int values[2 * X86_MAX_CONSTANTS];
} Imm32Set_s;

/* --- static tables (.data 0047D814..0047D8AD, verified against binary) ---- */

static const ALUOpEncode_s ALUOpEncode[6] =
{
    { 35u,  129u, 131u, 37u,  4u, 1u },   /* X86_AND  */
    { 57u,  129u, 131u, 61u,  7u, 0u },   /* X86_CMP  */
    { 11u,  129u, 131u, 13u,  1u, 1u },   /* X86_OR   */
    { 43u,  129u, 131u, 45u,  5u, 1u },   /* X86_SUB  */
    { 133u, 247u, 246u, 169u, 0u, 0u },   /* X86_TEST */
    { 49u,  129u, 131u, 53u,  6u, 0u }    /* X86_XOR  */
};

/* Maps HC hard register 0..7 -> IA-32 register encoding (0 -> ESP frame). */
static const unsigned char regRemap[8] = { 4, 0, 3, 1, 2, 6, 7, 5 };

/* Unary ops: NEG = F7 /3, NOT = F7 /2. */
static const unsigned char unaryOps[4] = { 0xF7u, 0x03u, 0xF7u, 0x02u };

/* Pairs of (rel8 opcode, 0F <rel32> second byte) indexed by (op - HCOP_BRA). */
static const unsigned char branchInstrs[26] =
{
    0xEBu, 0xE9u,   /* 40 HCOP_BRA   */
    0x75u, 0x85u,   /* 41 HCOP_CBA   (JNZ) */
    0x74u, 0x84u,   /* 42 HCOP_CBEQ  (JZ)  */
    0x7Du, 0x8Du,   /* 43 HCOP_CBGES (JGE) */
    0x73u, 0x83u,   /* 44 HCOP_CBGEU (JAE) */
    0x7Fu, 0x8Fu,   /* 45 HCOP_CBGTS (JG)  */
    0x77u, 0x87u,   /* 46 HCOP_CBGTU (JA)  */
    0x7Eu, 0x8Eu,   /* 47 HCOP_CBLES (JLE) */
    0x76u, 0x86u,   /* 48 HCOP_CBLEU (JBE) */
    0x7Cu, 0x8Cu,   /* 49 HCOP_CBLTS (JL)  */
    0x72u, 0x82u,   /* 50 HCOP_CBLTU (JB)  */
    0x74u, 0x84u,   /* 51 HCOP_CBNA  (JZ)  */
    0x75u, 0x85u    /* 52 HCOP_CBNEQ (JNZ) */
};

/* Second opcode byte after 0x0F for CMOVcc, indexed by X86CMP_e. */
static const unsigned char cmovOps[10] =
{
    'D', 'M', 'C', 'O', 'G', 'N', 'F', 'L', 'B', 'E'
};

static const unsigned char cmovMemOps[10] =
{
    'D', 'M', 'C', 'O', 'G', 'N', 'F', 'L', 'B', 'E'
};

/* X86CMP condition for HCOP_MAXS, HCOP_MAXU, HCOP_MINS, HCOP_MINU. */
static const int minMaxCops[4] = { 7, 8, 3, 4 };

/* ModRM reg field for ASR(7), LSL(4), LSR(5). */
static const unsigned char shiftModOps[3] = { 7, 4, 5 };

/* Load size (0=byte, 1=word, 2=dword) and sign-extend for HCOP_LBS..HCOP_LD4. */
static const unsigned char loadSize[8]    = { 0, 0, 1, 1, 1, 1, 2, 2 };
static const unsigned char loadSExtend[8] = { 1, 0, 1, 0, 1, 0, 0, 0 };

/* Store size (0=byte, 1=word, 2=dword) for HCOP_SB..HCOP_SD4. */
static const unsigned char storeSize[5] = { 0, 1, 1, 2, 2 };

/* EFLAGS knowledge mask required to elide CMP/TEST before branch (op - 40). */
static const unsigned char branchFlags[16] =
{
    0, 1, 1, 0, 0, 0, 0, 0,
    0, 0, 0, 1, 1, 0, 0, 0
};

/* --- unaligned little-endian 32-bit emit helper --------------------------- */

static void emitI32(unsigned char *p, int v)
{
    unsigned int u = (unsigned int)v;
    p[0] = (unsigned char)(u & 0xFFu);
    p[1] = (unsigned char)((u >> 8) & 0xFFu);
    p[2] = (unsigned char)((u >> 16) & 0xFFu);
    p[3] = (unsigned char)((u >> 24) & 0xFFu);
}

/* --- EncodeContext -------------------------------------------------------- */

static void EncodeContext_init(EncodeContext_s *self, unsigned char *out)
{
    if (!(self != NULL && out != NULL))
        HC_assertFail("self && out", "hcX86Backend.c", 186, NULL);
    self->out = out;
    self->flags = 0;
    self->reg = -1;
}

static void EncodeContext_clearFlags(EncodeContext_s *self)
{
    self->flags = 0;
}

static void EncodeContext_clearUnsetFlags(EncodeContext_s *self, int def)
{
    unsigned int f;
    int i;

    if (self == NULL)
        HC_assertFail("self", "hcX86Backend.c", 212, NULL);
    f = self->flags;
    for (i = 0; i < 2; ++i)
    {
        int p = 2 * i;
        if (((1u << p) & f) == 0)
            f &= ~(3u << p);
        f &= ~(1u << p);
    }
    self->flags = f;
    if (def != -1 && def == self->reg)
        self->flags = 0;
}

static void EncodeContext_setFlags(EncodeContext_s *self, int r, unsigned int flags)
{
    unsigned int m = 0;
    int i;

    if (self == NULL)
        HC_assertFail("self", "hcX86Backend.c", 237, NULL);
    for (i = 0; i < 16; ++i)
    {
        if (((1u << i) & flags) != 0)
            m |= (3u << (2 * i));
    }
    self->flags = m;
    self->reg = r;
}

static void EncodeContext_retainFlags(EncodeContext_s *self, unsigned int flags)
{
    unsigned int m = 0;
    int i;

    if (self == NULL)
        HC_assertFail("self", "hcX86Backend.c", 259, NULL);
    for (i = 0; i < 16; ++i)
    {
        if (((1u << i) & flags) != 0)
            m |= (self->flags & (3u << (2 * i))) | (1u << (2 * i));
    }
    self->flags = m;
}

static int EncodeContext_flagsKnown(EncodeContext_s *self, unsigned int msk)
{
    int i;

    if (self == NULL)
        HC_assertFail("self", "hcX86Backend.c", 277, NULL);
    if (msk == 0)
        return 0;
    for (i = 0; i < 2; ++i)
    {
        if (((1u << i) & msk) != 0 && ((2u << (2 * i)) & self->flags) == 0)
            return 0;
    }
    return 1;
}

static X86Reg_e EncodeContext_flagsReg(EncodeContext_s *self)
{
    if (self == NULL)
        HC_assertFail("self", "hcX86Backend.c", 296, NULL);
    return (X86Reg_e)self->reg;
}

/* --- ModRM / SIB ---------------------------------------------------------- */

static unsigned char ModRM(int mod, int rm, int reg)
{
    if ((unsigned int)mod >= 256u)
        HC_assertFail("(HCuint32)mod < 256", "hcX86Backend.c", 347, NULL);
    if ((unsigned int)rm >= 256u)
        HC_assertFail("(HCuint32)rm < 256", "hcX86Backend.c", 348, NULL);
    if ((unsigned int)reg >= 256u)
        HC_assertFail("(HCuint32)reg < 256", "hcX86Backend.c", 349, NULL);
    return (unsigned char)(rm + ((unsigned char)mod << 6) + 8 * reg);
}

static unsigned char SIB(int ss, int ndx, int base)
{
    if ((unsigned int)ss >= 256u)
        HC_assertFail("(unsigned int)ss < 256", "hcX86Backend.c", 402, NULL);
    if ((unsigned int)ndx >= 256u)
        HC_assertFail("(unsigned int)ndx < 256", "hcX86Backend.c", 403, NULL);
    if ((unsigned int)base >= 256u)
        HC_assertFail("(unsigned int)base < 256", "hcX86Backend.c", 404, NULL);
    return (unsigned char)(base + (ss << 6) + 8 * ndx);
}

/* --- simple backend queries ----------------------------------------------- */

static int HCX86Backend_canCreateImmediate(HCInstr_s *instr)
{
    (void)instr;
    return 1;
}

static int HCX86Backend_internalStackSpace(void)
{
    return 40;
}

static unsigned char *HCX86Backend_targetRegisterMapping(void)
{
    return (unsigned char *)regRemap;
}

static int HCX86Backend_numHardRegs(void)
{
    return 8;
}

static void HCX86Backend_emulate(void (*program)(void *), void *stackPtr)
{
    program(stackPtr);
}

static int HCX86Backend_isNative(void)
{
    return 1;
}

/* --- x86 instruction emitters --------------------------------------------- */

static int isGeneralReg(X86Reg_e r)
{
    return r <= REG_EBX;
}

static int isSignedByte(int v)
{
    return (signed char)v == v;
}

static int isNegativeByte(int v)
{
    if (v >= -128)
        return v < 0;
    return 0;
}

static void mov(EncodeContext_s *ctx, X86Reg_e d, X86Reg_e s)
{
    unsigned char *t;

    if (d >= REG_LAST)
        HC_assertFail("d < REG_LAST", "hcX86Backend.c", 497, NULL);
    if (s >= REG_LAST)
        HC_assertFail("s < REG_LAST", "hcX86Backend.c", 498, NULL);
    if (d != s)
    {
        t = ctx->out;
        t[0] = 0x89u;
        t[1] = ModRM(3, (int)d, (int)s);
        ctx->out += 2;
    }
}

static void movi(EncodeContext_s *ctx, X86Reg_e d, int imm)
{
    unsigned char *t;

    if ((unsigned int)d > (unsigned int)REG_EDI)
        HC_assertFail("HC_IN_RANGE(d, 0, 7)", "hcX86Backend.c", 506, NULL);
    if (imm != 0)
    {
        t = ctx->out;
        t[0] = 0xC7u;
        t[1] = ModRM(3, (int)d, 0);
        ctx->out += 2;
        emitI32(ctx->out, imm);
        ctx->out += 4;
    }
    else
    {
        t = ctx->out;
        t[0] = 0x31u;
        t[1] = ModRM(3, (int)d, (int)d);
        ctx->out += 2;
        EncodeContext_setFlags(ctx, (int)d, 3u);
    }
}

static void movFixSize(EncodeContext_s *ctx, X86Reg_e d, X86Reg_e s)
{
    unsigned char *t;

    if (d >= REG_LAST)
        HC_assertFail("d < REG_LAST", "hcX86Backend.c", 524, NULL);
    if (s >= REG_LAST)
        HC_assertFail("s < REG_LAST", "hcX86Backend.c", 525, NULL);
    t = ctx->out;
    t[0] = 0x89u;
    t[1] = ModRM(3, (int)d, (int)s);
    ctx->out += 2;
}

static void moviFixSize(EncodeContext_s *ctx, X86Reg_e d, int imm)
{
    unsigned char *t;

    if ((unsigned int)d > (unsigned int)REG_EDI)
        HC_assertFail("HC_IN_RANGE(d, 0, 7)", "hcX86Backend.c", 533, NULL);
    t = ctx->out;
    t[0] = 0xC7u;
    t[1] = ModRM(3, (int)d, 0);
    ctx->out += 2;
    emitI32(ctx->out, imm);
    ctx->out += 4;
}

static void add(EncodeContext_s *ctx, X86Reg_e d, X86Reg_e s0, X86Reg_e s1)
{
    unsigned char *t;

    if (d != s0)
    {
        X86Reg_e tmp = s0;
        s0 = s1;
        s1 = tmp;
    }
    if (d == s0)
    {
        t = ctx->out;
        t[0] = 0x03u;
        t[1] = ModRM(3, (unsigned char)s1, (unsigned char)d);
        ctx->out += 2;
        EncodeContext_setFlags(ctx, (int)d, 1u);
    }
    else
    {
        if (s1 == REG_ESP)
            HC_assertFail("s1 != REG_ESP", "hcX86Backend.c", 560, NULL);
        if (d == REG_ESP)
            HC_assertFail("d != REG_ESP", "hcX86Backend.c", 561, NULL);
        t = ctx->out;
        t[0] = 0x8Du;
        t[1] = ModRM(1, 4, (int)d);
        t[2] = SIB(0, (int)s1, (int)s0);
        t[3] = 0;
        ctx->out += 4;
    }
}

static void unaryOp(EncodeContext_s *ctx, X86UOP_e op, X86Reg_e d)
{
    unsigned char *t;

    if ((unsigned int)op >= (unsigned int)X86UOP_LAST)
        HC_assertFail("(HCuint32)op < (HCuint32)X86UOP_LAST", "hcX86Backend.c", 573, NULL);
    t = ctx->out;
    t[0] = unaryOps[2 * (int)op];
    t[1] = ModRM(3, (unsigned char)d, unaryOps[2 * (int)op + 1]);
    ctx->out += 2;
}

static void gen2AOpImm(EncodeContext_s *ctx, X86Reg_e d, int imm,
                       unsigned int imm32Op, unsigned int imm8Op,
                       unsigned int eaxOp, unsigned int opMod)
{
    unsigned char *t;

    if (imm32Op >= 256u)
        HC_assertFail("imm32Op < 256", "hcX86Backend.c", 595, NULL);
    if (imm8Op >= 256u)
        HC_assertFail("imm8Op < 256", "hcX86Backend.c", 596, NULL);
    if (eaxOp >= 256u)
        HC_assertFail("eaxOp < 256", "hcX86Backend.c", 597, NULL);
    if (opMod >= 256u)
        HC_assertFail("opMod < 256", "hcX86Backend.c", 598, NULL);
    if (d != REG_EAX || isSignedByte(imm) != 0)
    {
        if (isSignedByte(imm) != 0)
        {
            t = ctx->out;
            t[0] = (unsigned char)imm8Op;
            t[1] = ModRM(3, (unsigned char)d, (int)opMod);
            t[2] = (unsigned char)imm;
            ctx->out += 3;
        }
        else
        {
            t = ctx->out;
            t[0] = (unsigned char)imm32Op;
            t[1] = ModRM(3, (unsigned char)d, (int)opMod);
            ctx->out += 2;
            emitI32(ctx->out, imm);
            ctx->out += 4;
        }
    }
    else
    {
        *ctx->out++ = (unsigned char)eaxOp;
        emitI32(ctx->out, imm);
        ctx->out += 4;
    }
}

static void addi(EncodeContext_s *ctx, X86Reg_e d, X86Reg_e s0, int imm)
{
    unsigned char *t;
    int mod;

    if (d == s0)
    {
        gen2AOpImm(ctx, d, imm, 0x81u, 0x83u, 0x05u, 0);
        EncodeContext_setFlags(ctx, (int)d, 1u);
    }
    else
    {
        if (s0 == REG_ESP)
            HC_assertFail("s0 != REG_ESP", "hcX86Backend.c", 635, NULL);
        if (d == REG_ESP)
            HC_assertFail("d != REG_ESP", "hcX86Backend.c", 636, NULL);
        t = ctx->out;
        t[0] = 0x8Du;
        mod = (isSignedByte(imm) != 0) ? 1 : 2;
        t[1] = ModRM(mod, (unsigned char)s0, (unsigned char)d);
        ctx->out += 2;
        if (isSignedByte(imm) != 0)
        {
            *ctx->out++ = (unsigned char)imm;
        }
        else
        {
            emitI32(ctx->out, imm);
            ctx->out += 4;
        }
    }
}

static void x86ALUOp(EncodeContext_s *ctx, X86ALU_e op, X86Reg_e d, X86Reg_e s0)
{
    unsigned char *t;
    X86Reg_e rmReg = d;
    X86Reg_e regField = s0;

    if (ALUOpEncode[op].swapOperands != 0)
    {
        rmReg = s0;
        regField = d;
    }
    t = ctx->out;
    t[0] = ALUOpEncode[op].normOp;
    t[1] = ModRM(3, (unsigned char)rmReg, (unsigned char)regField);
    ctx->out += 2;
    if (op == X86_SUB)
        EncodeContext_setFlags(ctx, (int)rmReg, 1u);
}

static void x86ALUOpImm(EncodeContext_s *ctx, X86ALU_e op, X86Reg_e d, int imm)
{
    const ALUOpEncode_s *enc = &ALUOpEncode[op];
    unsigned char *t;

    if (op != X86_TEST || isGeneralReg(d) != 0)
    {
        gen2AOpImm(ctx, d, imm, enc->r32Imm32, enc->r32Imm8, enc->eaxImm32, enc->opField);
        if (op == X86_SUB)
            EncodeContext_setFlags(ctx, (int)d, 1u);
    }
    else
    {
        t = ctx->out;
        t[0] = enc->r32Imm32;
        t[1] = ModRM(3, (unsigned char)d, 0);
        ctx->out += 2;
        emitI32(ctx->out, imm);
        ctx->out += 4;
    }
}

static void x86ShlImm(EncodeContext_s *ctx, X86Reg_e d, X86Reg_e s, int shift)
{
    unsigned char *t;

    if (s == REG_ESP)
        HC_assertFail("s != REG_ESP", "hcX86Backend.c", 686, NULL);
    if (!(shift > 0 && shift <= 3))
        HC_assertFail("shift > 0 && shift <= 3", "hcX86Backend.c", 687, NULL);
    if (shift == 1)
    {
        t = ctx->out;
        t[0] = 0x8Du;
        t[1] = ModRM(1, 4, (int)d);
        t[2] = SIB(0, (int)s, (int)s);
        t[3] = 0;
    }
    else
    {
        t = ctx->out;
        t[0] = 0x8Du;
        t[1] = ModRM(0, 4, (int)d);
        t[2] = SIB(shift, (int)s, 5);
        ctx->out += 3;
        emitI32(ctx->out, 0);
    }
    ctx->out += 4;
}

static void x86RotateImm(EncodeContext_s *ctx, int op, int d, int shift)
{
    unsigned char *t;

    *ctx->out++ = 0xC1u;
    t = ctx->out;
    if (op == HCOP_ROL)
        t[0] = ModRM(3, d, 0);
    else
        t[0] = ModRM(3, d, 1);
    t[1] = (unsigned char)shift;
    ctx->out += 2;
}

static void x86Rotate(EncodeContext_s *ctx, int op, int d)
{
    *ctx->out++ = 0xD3u;
    if (op == HCOP_ROL)
        *ctx->out = ModRM(3, d, 0);
    else
        *ctx->out = ModRM(3, d, 1);
    ++ctx->out;
}

static void genSmallBranch(EncodeContext_s *ctx, int op, int target)
{
    unsigned char *t = ctx->out;

    t[0] = branchInstrs[2 * op];
    t[1] = (unsigned char)target;
    ctx->out += 2;
}

static void genCmovJump(EncodeContext_s *ctx, X86CMP_e cop, int target)
{
    int jumpOp = -1;

    switch (cop)
    {
    case X86CMP_GTS:
        jumpOp = 9;
        break;
    case X86CMP_GTU:
        jumpOp = 10;
        break;
    case X86CMP_LTS:
        jumpOp = 3;
        break;
    case X86CMP_LTU:
        jumpOp = 4;
        break;
    default:
        HC_assertFail("HG_FALSE", "hcX86Backend.c", 756, NULL);
        break;
    }
    if (jumpOp == -1)
    {
        HC_assertFail("jumpOp != -1", "hcX86Backend.c", 758, NULL);
        return;
    }
    genSmallBranch(ctx, jumpOp, target);
}

static void cmov(EncodeContext_s *ctx, X86Reg_e d, X86CMP_e cop, X86Reg_e s)
{
    unsigned char *t = ctx->out;

    t[0] = 0x0Fu;
    t[1] = cmovOps[cop];
    t[2] = ModRM(3, (unsigned char)s, (unsigned char)d);
    ctx->out += 3;
}

static void emitMemOperand(EncodeContext_s *ctx, X86Reg_e d, X86Reg_e s, int ofs)
{
    unsigned char *t;
    int mod;

    if (s == REG_ESP)
    {
        if (isSignedByte(ofs) != 0)
        {
            t = ctx->out;
            t[0] = ModRM(1, 4, (int)d);
            t[1] = SIB(0, 4, 4);
            t[2] = (unsigned char)ofs;
            ctx->out += 3;
        }
        else
        {
            t = ctx->out;
            t[0] = ModRM(2, 4, (int)d);
            t[1] = SIB(0, 4, 4);
            ctx->out += 2;
            emitI32(ctx->out, ofs);
            ctx->out += 4;
        }
    }
    else
    {
        mod = (isSignedByte(ofs) != 0) ? 1 : 2;
        *ctx->out++ = ModRM(mod, (int)s, (int)d);
        if (isSignedByte(ofs) != 0)
        {
            *ctx->out++ = (unsigned char)ofs;
        }
        else
        {
            emitI32(ctx->out, ofs);
            ctx->out += 4;
        }
    }
}

static void cmovMem(EncodeContext_s *ctx, X86Reg_e d, X86CMP_e cop, X86Reg_e s, int ofs)
{
    unsigned char *t = ctx->out;

    t[0] = 0x0Fu;
    t[1] = cmovMemOps[cop];
    ctx->out += 2;
    emitMemOperand(ctx, d, s, ofs);
}

static void cmpMem(EncodeContext_s *ctx, X86Reg_e d, X86Reg_e s, int ofs)
{
    *ctx->out++ = 0x3Bu;
    emitMemOperand(ctx, d, s, ofs);
}

static void aluMemOp(EncodeContext_s *ctx, X86Reg_e d, X86Reg_e s, int ofs)
{
    emitMemOperand(ctx, d, s, ofs);
}

static void addMem(EncodeContext_s *ctx, X86Reg_e d, X86Reg_e s, int ofs)
{
    *ctx->out++ = 0x03u;
    aluMemOp(ctx, d, s, ofs);
    EncodeContext_setFlags(ctx, (int)d, 1u);
}

static void andMem(EncodeContext_s *ctx, X86Reg_e d, X86Reg_e s, int ofs)
{
    *ctx->out++ = 0x23u;
    aluMemOp(ctx, d, s, ofs);
    EncodeContext_setFlags(ctx, (int)d, 1u);
}

static void imulMem(EncodeContext_s *ctx, X86Reg_e d, X86Reg_e s, int ofs)
{
    unsigned char *t = ctx->out;

    t[0] = 0x0Fu;
    t[1] = 0xAFu;
    ctx->out += 2;
    emitMemOperand(ctx, d, s, ofs);
}

static void storeBase(EncodeContext_s *ctx, int sz, X86Reg_e base, X86Reg_e w)
{
    unsigned char *t;

    if (sz == 0 && isGeneralReg(w) == 0)
        HC_assertFail("isGeneralReg(w)", "hcX86Backend.c", 879, NULL);
    if (sz == 1)
        *ctx->out++ = 0x66u;
    *ctx->out++ = (sz != 0) ? 0x89u : 0x88u;
    if (base == REG_EBP)
    {
        t = ctx->out;
        t[0] = ModRM(1, 5, (unsigned char)w);
        t[1] = 0;
        ctx->out += 2;
    }
    else if (base == REG_ESP)
    {
        t = ctx->out;
        t[0] = ModRM(1, 4, (unsigned char)w);
        t[1] = SIB(0, 4, 4);
        t[2] = 0;
        ctx->out += 3;
    }
    else
    {
        *ctx->out++ = ModRM(0, (unsigned char)base, (unsigned char)w);
    }
}

static void storeBaseOffset(EncodeContext_s *ctx, int sz, X86Reg_e base, int ofs, X86Reg_e w)
{
    if (sz == 0 && isGeneralReg(w) == 0)
        HC_assertFail("isGeneralReg(w)", "hcX86Backend.c", 895, NULL);
    if (ofs != 0)
    {
        if (sz == 1)
            *ctx->out++ = 0x66u;
        *ctx->out++ = (sz != 0) ? 0x89u : 0x88u;
        emitMemOperand(ctx, w, base, ofs);
    }
    else
    {
        storeBase(ctx, sz, base, w);
    }
}

static void storeBaseIndex(EncodeContext_s *ctx, int sz, int shift,
                           X86Reg_e base, X86Reg_e ndx, X86Reg_e w)
{
    unsigned char *t;

    if (base == REG_ESP)
        HC_assertFail("base != REG_ESP", "hcX86Backend.c", 929, NULL);
    if (ndx == REG_ESP)
        HC_assertFail("ndx != REG_ESP", "hcX86Backend.c", 930, NULL);
    if (!(shift >= 0 && shift <= 3))
        HC_assertFail("shift >= 0 && shift <= 3", "hcX86Backend.c", 931, NULL);
    if (sz == 0 && isGeneralReg(w) == 0)
        HC_assertFail("isGeneralReg(w)", "hcX86Backend.c", 932, NULL);
    if (sz == 1)
        *ctx->out++ = 0x66u;
    *ctx->out++ = (sz != 0) ? 0x89u : 0x88u;
    if (base == REG_EBP)
    {
        t = ctx->out;
        t[0] = ModRM(1, 4, (int)w);
        t[1] = SIB(shift, (int)ndx, 5);
        t[2] = 0;
        ctx->out += 3;
    }
    else
    {
        t = ctx->out;
        t[0] = ModRM(0, 4, (int)w);
        t[1] = SIB(shift, (int)ndx, (int)base);
        ctx->out += 2;
    }
}

static void loadBase(EncodeContext_s *ctx, int sz, X86Extend_e e, X86Reg_e base, X86Reg_e w)
{
    unsigned char *t;

    if ((unsigned int)sz > 1u)
    {
        *ctx->out++ = 0x8Bu;
    }
    else
    {
        int wordOrByte = (sz == 1);
        t = ctx->out;
        t[0] = 0x0Fu;
        t[1] = (e == EXTEND_SIGNED)
            ? (unsigned char)(wordOrByte + 0xBE)
            : (unsigned char)(wordOrByte + 0xB6);
        ctx->out += 2;
    }
    if (base == REG_EBP)
    {
        t = ctx->out;
        t[0] = ModRM(1, 5, (int)w);
        t[1] = 0;
        ctx->out += 2;
    }
    else if (base == REG_ESP)
    {
        t = ctx->out;
        t[0] = ModRM(1, 4, (int)w);
        t[1] = SIB(0, 4, 4);
        t[2] = 0;
        ctx->out += 3;
    }
    else
    {
        *ctx->out++ = ModRM(0, (int)base, (int)w);
    }
}

static void loadBaseOffset(EncodeContext_s *ctx, int sz, X86Extend_e e,
                           X86Reg_e base, int ofs, X86Reg_e w)
{
    if (ofs != 0)
    {
        if ((unsigned int)sz > 1u)
        {
            *ctx->out++ = 0x8Bu;
        }
        else
        {
            int wordOrByte = (sz == 1);
            unsigned char *t = ctx->out;
            t[0] = 0x0Fu;
            t[1] = (e == EXTEND_SIGNED)
                ? (unsigned char)(wordOrByte + 0xBE)
                : (unsigned char)(wordOrByte + 0xB6);
            ctx->out += 2;
        }
        emitMemOperand(ctx, w, base, ofs);
    }
    else
    {
        loadBase(ctx, sz, e, base, w);
    }
}

static void loadBaseIndex(EncodeContext_s *ctx, int sz, int shift, X86Extend_e e,
                          X86Reg_e base, X86Reg_e ndx, X86Reg_e w)
{
    unsigned char *t;

    if (base == REG_ESP)
        HC_assertFail("base != REG_ESP", "hcX86Backend.c", 997, NULL);
    if (ndx == REG_ESP)
        HC_assertFail("ndx != REG_ESP", "hcX86Backend.c", 998, NULL);
    if (!(shift >= 0 && shift <= 3))
        HC_assertFail("shift >= 0 && shift <= 3", "hcX86Backend.c", 999, NULL);
    if (sz == 2 && e != EXTEND_UNSIGNED)
        HC_assertFail("e == EXTEND_UNSIGNED", "hcX86Backend.c", 1000, NULL);
    if ((unsigned int)sz > 1u)
    {
        *ctx->out++ = 0x8Bu;
    }
    else
    {
        int wordOrByte = (sz == 1);
        t = ctx->out;
        t[0] = 0x0Fu;
        t[1] = (e == EXTEND_SIGNED)
            ? (unsigned char)(wordOrByte + 0xBE)
            : (unsigned char)(wordOrByte + 0xB6);
        ctx->out += 2;
    }
    if (base == REG_EBP)
    {
        t = ctx->out;
        t[0] = ModRM(1, 4, (int)w);
        t[1] = SIB(shift, (int)ndx, 5);
        t[2] = 0;
        ctx->out += 3;
    }
    else
    {
        t = ctx->out;
        t[0] = ModRM(0, 4, (int)w);
        t[1] = SIB(shift, (int)ndx, (int)base);
        ctx->out += 2;
    }
}

/* --- stack-load fusion (checkLoad) ---------------------------------------- */

static int isStackLoad(HCInstr_s *instr)
{
    if (HC_getOpcodeOper(instr) == HCOP_LD
        && HC_getOpcodeSrc0(instr) == (int)REG_ESP
        && HC_opcodeHasImm(instr) != 0)
    {
        return 1;
    }
    return 0;
}

static int isStackStore(HCInstr_s *instr)
{
    if (HC_getOpcodeOper(instr) == HCOP_SD
        && HC_getOpcodeSrc0(instr) == (int)REG_ESP
        && HC_opcodeHasImm(instr) != 0)
    {
        return 1;
    }
    return 0;
}

static int isBasicBlockBoundary(HCInstr_s *instr)
{
    int op = HC_getOpcodeOper(instr);

    if (op != HCOP_LABEL)
        return HC_isBranchOp(op) != 0;
    return 1;
}

static int checkLoad(void *info, HCInstr_s *lst, int ndx, int loadNdx)
{
    HCInstr_s *instr = &lst[ndx];
    int op = HC_getOpcodeOper(instr);
    int dst;
    int s0;
    int s1;
    void *ud;
    int defSite;
    int defReg;
    void *du;
    HCInstr_s *loadInstr;
    int spillOfs;
    int i;

    if ((op != HCOP_ADD && op != HCOP_MULS && op != HCOP_MULU && op != HCOP_AND)
        || HC_opcodeHasImm(instr) != 0)
    {
        return -1;
    }
    dst = HC_getOpcodeDst(instr);
    s0 = HC_getOpcodeSrc0(instr);
    s1 = HC_getOpcodeSrc1(instr);
    if (dst != s0 || s0 == s1)
        return -1;
    ud = HC_getUDChain(info, ndx, s1);
    if (ud == NULL || HCChain_getLength(ud) != 1)
        return -1;
    defSite = HCChain_getValue(ud, 0);
    defReg = HC_getDef(&lst[defSite]);
    du = HC_getDUChain(info, defSite);
    loadInstr = &lst[defSite];
    if (du == NULL || HCChain_getLength(du) != 1 || defReg != s1)
        return -1;
    if (loadNdx != -1 && loadNdx != defSite)
        return -1;
    if (defSite >= ndx || isStackLoad(loadInstr) == 0)
        return -1;
    spillOfs = HC_getOpcodeImm(loadInstr);
    for (i = defSite + 1; i < ndx; ++i)
    {
        if (isBasicBlockBoundary(&lst[i]) != 0)
            return -1;
        if (isStackStore(&lst[i]) != 0)
        {
            if (spillOfs == HC_getOpcodeImm(&lst[i]))
                return -1;
        }
    }
    if (ndx != HCChain_getValue(du, 0))
        HC_assertFail("HCChain_getValue(du, 0) == ndx", "hcX86Backend.c", 1118, NULL);
    return spillOfs;
}

/* --- Imm32Set (min/max immediate constant slots at [esp + 12 + 4*i]) ------ */

static void Imm32Set_reset(Imm32Set_s *constantSet)
{
    int i;

    constantSet->nValues = 0;
    for (i = 0; i < X86_MAX_CONSTANTS; ++i)
        constantSet->values[2 * i] = -1;
}

static int Imm32Set_allocConst(Imm32Set_s *constantSet, int value)
{
    int i;

    for (i = 0; i < constantSet->nValues; ++i)
    {
        if (value == constantSet->values[2 * i + 1])
            return constantSet->values[2 * i];
    }
    if (i >= X86_MAX_CONSTANTS)
        HC_assertFail("i < MAX_CONSTANTS", "hcX86Backend.c", 1144, NULL);
    constantSet->values[2 * i] = 4 * i + 12;
    constantSet->values[2 * i + 1] = value;
    ++constantSet->nValues;
    return 4 * i + 12;
}

static unsigned int Imm32Set_getConstLoc(Imm32Set_s *constantSet, int which)
{
    if (!(which >= 0 && which < constantSet->nValues))
        HC_assertFail("which >= 0 && which < constantSet->nValues", "hcX86Backend.c", 1157, NULL);
    return (unsigned int)constantSet->values[2 * which];
}

static unsigned int Imm32Set_getConstVal(Imm32Set_s *constantSet, int which)
{
    if (!(which >= 0 && which < constantSet->nValues))
        HC_assertFail("which >= 0 && which < constantSet->nValues", "hcX86Backend.c", 1164, NULL);
    return (unsigned int)constantSet->values[2 * which + 1];
}

/* --- arena code-buffer growth & CMOV query -------------------------------- */

static void growMemory(HCArena_s *mgr, int current, int *allocated)
{
    if (*allocated - current < 64)
    {
        HCArena_growUnaligned(mgr, 0x80u);
        *allocated += 128;
    }
}

static int supportCMOV(void)
{
    return 0;
}

/* --- HCX86Backend_codegen ------------------------------------------------- */

static int HCX86Backend_codegen(void *mgrVoid, void **destination,
                                HCInstr_s *instrs, void *info, int spillStart)
{
    HCArena_s *mgr = (HCArena_s *)mgrVoid;
    int hasCMOV;
    int topMark;
    short *labelOffsets;
    int btmMark;
    Imm32Set_s constantSet;
    EncodeContext_s ctx;
    unsigned char *codeBase;
    unsigned char *codeEnd = NULL;
    int pass;
    int i;

    (void)spillStart;

    hasCMOV = supportCMOV();
    topMark = HCArena_markTop(mgr);
    labelOffsets = (short *)HCArena_allocTop(mgr, 0x1000u);
    btmMark = HCArena_markBottom(mgr);
    for (i = 0; i < 2048; ++i)
        labelOffsets[i] = -1;
    Imm32Set_reset(&constantSet);
    codeBase = (unsigned char *)HCArena_current(mgr);
    *destination = codeBase;

    for (pass = 0; pass < 2; ++pass)
    {
        int allocated = 0;
        int pc = 0;
        unsigned char *bodyStart;

        EncodeContext_init(&ctx, codeBase);
        if (pass == 1)
            HCArena_releaseBottom(mgr, btmMark);

        growMemory(mgr, (int)(ctx.out - codeBase), &allocated);

        /* Prologue: load stackPtr arg from [esp+4], save caller's esp at
         * [eax+4] and callee/scratch regs below caller's esp, then pivot
         * esp = stackPtr. */
        loadBaseOffset(&ctx, 2, EXTEND_UNSIGNED, REG_ESP, 4, REG_EAX);
        storeBaseOffset(&ctx, 2, REG_EAX, 4, REG_ESP);
        storeBaseOffset(&ctx, 2, REG_ESP, -4, REG_EBX);
        storeBaseOffset(&ctx, 2, REG_ESP, -8, REG_ECX);
        storeBaseOffset(&ctx, 2, REG_ESP, -12, REG_EDX);
        storeBaseOffset(&ctx, 2, REG_ESP, -16, REG_ESI);
        storeBaseOffset(&ctx, 2, REG_ESP, -20, REG_EDI);
        storeBaseOffset(&ctx, 2, REG_ESP, -24, REG_EBP);
        mov(&ctx, REG_ESP, REG_EAX);

        /* Pass 1 materializes Imm32Set constants into [esp + loc] via
         * C7 84 24 <disp32> <imm32>. */
        if (pass == 1)
        {
            int cIdx;
            for (cIdx = 0; cIdx < constantSet.nValues; ++cIdx)
            {
                unsigned char *p;
                growMemory(mgr, (int)(ctx.out - codeBase), &allocated);
                p = ctx.out;
                p[0] = 0xC7u;
                p[1] = 0x84u;
                p[2] = 0x24u;
                ctx.out += 3;
                emitI32(ctx.out, (int)Imm32Set_getConstLoc(&constantSet, cIdx));
                ctx.out += 4;
                emitI32(ctx.out, (int)Imm32Set_getConstVal(&constantSet, cIdx));
                ctx.out += 4;
            }
        }

        bodyStart = ctx.out;
        while (HC_getOpcodeOper(&instrs[pc]) != HCOP_EOP)
        {
            HCInstr_s *instr = &instrs[pc];
            int hasImm = HC_opcodeHasImm(instr);
            int op = HC_getOpcodeOper(instr);
            int def = HC_getDef(instr);

            EncodeContext_clearUnsetFlags(&ctx, def);
            switch (op)
            {
            case HCOP_ADD:
            {
                X86Reg_e dst = (X86Reg_e)HC_getOpcodeDst(instr);
                X86Reg_e s0 = (X86Reg_e)HC_getOpcodeSrc0(instr);
                int spillOfs = checkLoad(info, instrs, pc, -1);
                if (spillOfs == -1)
                {
                    if (HC_opcodeHasImm(instr) != 0)
                        addi(&ctx, dst, s0, HC_getOpcodeImm(instr));
                    else
                        add(&ctx, dst, s0, (X86Reg_e)HC_getOpcodeSrc1(instr));
                }
                else
                {
                    addMem(&ctx, dst, REG_ESP, spillOfs);
                }
                break;
            }

            case HCOP_AND:
            case HCOP_OR:
            case HCOP_SUB:
            case HCOP_XOR:
            {
                X86Reg_e dst = (X86Reg_e)HC_getOpcodeDst(instr);
                X86ALU_e aluOp = X86_AND;

                if (dst != (X86Reg_e)HC_getOpcodeSrc0(instr))
                    HC_assertFail("(Reg)HC_getOpcodeSrc0(instr) == dst", "hcX86Backend.c", 1352, NULL);
                switch (op)
                {
                case HCOP_AND: aluOp = X86_AND; break;
                case HCOP_OR:  aluOp = X86_OR;  break;
                case HCOP_SUB: aluOp = X86_SUB; break;
                case HCOP_XOR: aluOp = X86_XOR; break;
                default:
                    HC_assertFail("0", "hcX86Backend.c", 1359, NULL);
                    break;
                }
                if (HC_opcodeHasImm(instr) != 0)
                {
                    x86ALUOpImm(&ctx, aluOp, dst, HC_getOpcodeImm(instr));
                }
                else
                {
                    int spillOfs = checkLoad(info, instrs, pc, -1);
                    if (op != HCOP_AND || spillOfs == -1)
                        x86ALUOp(&ctx, aluOp, dst, (X86Reg_e)HC_getOpcodeSrc1(instr));
                    else
                        andMem(&ctx, dst, REG_ESP, spillOfs);
                }
                break;
            }

            case HCOP_ASD:
            case HCOP_BIC:
            case HCOP_LSD:
            case HCOP_NOP:
            case HCOP_FILLB:
            case HCOP_FILLW:
            case HCOP_FILLD:
            case HCOP_EOP:
            case HCOP_MAX:
                HC_assertFail("0", "hcX86Backend.c", 1753, NULL);
                break;

            case HCOP_ASR:
            case HCOP_LSL:
            case HCOP_LSR:
            {
                int kind = (op == HCOP_ASR) ? 0 : ((op == HCOP_LSL) ? 1 : 2);
                X86Reg_e d = (X86Reg_e)HC_getOpcodeDst(instr);

                if (HC_opcodeHasImm(instr) != 0)
                {
                    int imm = HC_getOpcodeImm(instr);
                    X86Reg_e s0 = (X86Reg_e)HC_getOpcodeSrc0(instr);

                    if (!(imm >= 0 && imm <= 31))
                        HC_assertFail("imm >= 0 && imm <= 31", "hcX86Backend.c", 1526, NULL);
                    if (op != HCOP_LSL || d == s0 || (unsigned int)(imm - 1) > 2u)
                    {
                        unsigned char *p;
                        if (d != (X86Reg_e)HC_getOpcodeSrc0(instr))
                            HC_assertFail("d == HC_getOpcodeSrc0(instr)", "hcX86Backend.c", 1531, NULL);
                        p = ctx.out;
                        p[0] = 0xC1u;
                        p[1] = ModRM(3, (unsigned char)d, shiftModOps[kind]);
                        p[2] = (unsigned char)imm;
                        ctx.out += 3;
                    }
                    else
                    {
                        x86ShlImm(&ctx, d, s0, imm);
                    }
                }
                else
                {
                    unsigned char *p;
                    if (d != (X86Reg_e)HC_getOpcodeSrc0(instr))
                        HC_assertFail("d == HC_getOpcodeSrc0(instr)", "hcX86Backend.c", 1536, NULL);
                    p = ctx.out;
                    p[0] = 0xD3u;
                    p[1] = ModRM(3, (unsigned char)d, shiftModOps[kind]);
                    ctx.out += 2;
                }
                break;
            }

            case HCOP_MAXS:
            case HCOP_MAXU:
            case HCOP_MINS:
            case HCOP_MINU:
            {
                X86Reg_e d = (X86Reg_e)HC_getOpcodeDst(instr);
                X86CMP_e cop;

                if ((int)d != HC_getOpcodeSrc0(instr))
                    HC_assertFail("HC_getOpcodeSrc0(instr) == d", "hcX86Backend.c", 1426, NULL);
                if ((unsigned int)(op - HCOP_MAXS) > 3u)
                    HC_assertFail("HC_IN_RANGE(op, (int)HCOP_MAXS, (int)HCOP_MINU)", "hcX86Backend.c", 1427, NULL);
                cop = (X86CMP_e)minMaxCops[op - HCOP_MAXS];
                if (HC_opcodeHasImm(instr) != 0)
                {
                    int imm = HC_getOpcodeImm(instr);
                    int loc = Imm32Set_allocConst(&constantSet, imm);
                    cmpMem(&ctx, d, REG_ESP, loc);
                    if (hasCMOV != 0)
                    {
                        cmovMem(&ctx, d, cop, REG_ESP, loc);
                    }
                    else
                    {
                        genCmovJump(&ctx, cop, 6);
                        moviFixSize(&ctx, d, HC_getOpcodeImm(instr));
                    }
                }
                else
                {
                    X86Reg_e s1 = (X86Reg_e)HC_getOpcodeSrc1(instr);
                    x86ALUOp(&ctx, X86_CMP, d, s1);
                    if (hasCMOV != 0)
                    {
                        cmov(&ctx, d, cop, s1);
                    }
                    else
                    {
                        genCmovJump(&ctx, cop, 2);
                        movFixSize(&ctx, d, s1);
                    }
                }
                break;
            }

            case HCOP_MULS:
            case HCOP_MULU:
            {
                X86Reg_e d = (X86Reg_e)HC_getOpcodeDst(instr);
                int spillOfs = checkLoad(info, instrs, pc, -1);

                if (spillOfs == -1)
                {
                    if (HC_opcodeHasImm(instr) != 0)
                    {
                        int s0 = HC_getOpcodeSrc0(instr);
                        unsigned char *p = ctx.out;
                        p[0] = 0x69u;
                        p[1] = ModRM(3, (int)d, s0);
                        ctx.out += 2;
                        emitI32(ctx.out, HC_getOpcodeImm(instr));
                        ctx.out += 4;
                    }
                    else
                    {
                        int s1 = HC_getOpcodeSrc1(instr);
                        unsigned char *p = ctx.out;
                        p[0] = 0x0Fu;
                        p[1] = 0xAFu;
                        p[2] = ModRM(3, s1, (int)d);
                        ctx.out += 3;
                    }
                }
                else
                {
                    imulMem(&ctx, d, REG_ESP, spillOfs);
                }
                break;
            }

            case HCOP_MOV:
                if (HC_opcodeHasImm(instr) != 0)
                    movi(&ctx, (X86Reg_e)HC_getOpcodeDst(instr), HC_getOpcodeImm(instr));
                else
                    mov(&ctx, (X86Reg_e)HC_getOpcodeDst(instr), (X86Reg_e)HC_getOpcodeSrc0(instr));
                EncodeContext_retainFlags(&ctx, 3u);
                break;

            case HCOP_MOVN:
            {
                X86Reg_e d = (X86Reg_e)HC_getOpcodeDst(instr);
                if (HC_opcodeHasImm(instr) != 0)
                {
                    movi(&ctx, d, ~HC_getOpcodeImm(instr));
                }
                else
                {
                    mov(&ctx, d, (X86Reg_e)HC_getOpcodeSrc0(instr));
                    unaryOp(&ctx, X86UOP_NOT, d);
                }
                break;
            }

            case HCOP_ROL:
            case HCOP_ROR:
            {
                int d = HC_getOpcodeDst(instr);
                if (HC_opcodeHasImm(instr) != 0)
                    x86RotateImm(&ctx, op, d, HC_getOpcodeImm(instr));
                else
                    x86Rotate(&ctx, op, d);
                break;
            }

            case HCOP_SUBR:
            {
                X86Reg_e d = (X86Reg_e)HC_getOpcodeDst(instr);
                if (HC_opcodeHasImm(instr) != 0)
                {
                    int imm = HC_getOpcodeImm(instr);
                    unaryOp(&ctx, X86UOP_NEG, d);
                    if (HC_getOpcodeImm(instr) != 0)
                        addi(&ctx, d, d, imm);
                }
                else
                {
                    X86Reg_e s1 = (X86Reg_e)HC_getOpcodeSrc1(instr);
                    if (d == s1)
                    {
                        x86ALUOp(&ctx, X86_XOR, d, d);
                    }
                    else
                    {
                        unaryOp(&ctx, X86UOP_NEG, d);
                        add(&ctx, d, d, s1);
                    }
                }
                break;
            }

            case HCOP_LBS:
            case HCOP_LBU:
            case HCOP_LWS:
            case HCOP_LWU:
            case HCOP_LW2S:
            case HCOP_LW2U:
            case HCOP_LD:
            case HCOP_LD4:
            {
                int loadKind = op - HCOP_LBS;
                int idxScale = HC_getMemOpIndexSizeOp(op);
                int fused = 0;
                void *du = HC_getDUChain(info, pc);

                if (isStackLoad(instr) != 0 && du != NULL && HCChain_getLength(du) == 1)
                {
                    int useSite = HCChain_getValue(du, 0);
                    fused = (checkLoad(info, instrs, useSite, pc) != -1);
                }
                if (fused == 0)
                {
                    X86Reg_e w = (X86Reg_e)HC_getOpcodeDst(instr);
                    X86Reg_e base = (X86Reg_e)HC_getOpcodeSrc0(instr);
                    if (hasImm != 0)
                    {
                        int ofs = idxScale * HC_getOpcodeImm(instr);
                        loadBaseOffset(&ctx, loadSize[loadKind],
                                       (X86Extend_e)loadSExtend[loadKind],
                                       base, ofs, w);
                    }
                    else
                    {
                        X86Reg_e ndx = (X86Reg_e)HC_getOpcodeSrc1(instr);
                        loadBaseIndex(&ctx, loadSize[loadKind],
                                      (int)((unsigned int)idxScale >> 1),
                                      (X86Extend_e)loadSExtend[loadKind],
                                      base, ndx, w);
                    }
                }
                EncodeContext_retainFlags(&ctx, 3u);
                break;
            }

            case HCOP_SB:
            case HCOP_SW:
            case HCOP_SW2:
            case HCOP_SD:
            case HCOP_SD4:
            {
                int storeKind = op - HCOP_SB;
                int idxScale = HC_getMemOpIndexSizeOp(op);
                X86Reg_e w = (X86Reg_e)HC_getOpcodeDst(instr);
                X86Reg_e base = (X86Reg_e)HC_getOpcodeSrc0(instr);

                if (hasImm != 0)
                {
                    int ofs = idxScale * HC_getOpcodeImm(instr);
                    storeBaseOffset(&ctx, storeSize[storeKind], base, ofs, w);
                }
                else
                {
                    X86Reg_e ndx = (X86Reg_e)HC_getOpcodeSrc1(instr);
                    storeBaseIndex(&ctx, storeSize[storeKind],
                                   (int)((unsigned int)idxScale >> 1),
                                   base, ndx, w);
                }
                EncodeContext_retainFlags(&ctx, 3u);
                break;
            }

            case HCOP_ANSA:
            {
                unsigned char *p = ctx.out;
                p[0] = 0xCDu;
                p[1] = 0x03u;
                ctx.out += 2;
                break;
            }

            case HCOP_BRA:
            case HCOP_CBA:
            case HCOP_CBEQ:
            case HCOP_CBGES:
            case HCOP_CBGEU:
            case HCOP_CBGTS:
            case HCOP_CBGTU:
            case HCOP_CBLES:
            case HCOP_CBLEU:
            case HCOP_CBLTS:
            case HCOP_CBLTU:
            case HCOP_CBNA:
            case HCOP_CBNEQ:
            {
                X86ALU_e cmpOp = (op == HCOP_CBA || op == HCOP_CBNA) ? X86_TEST : X86_CMP;
                int brIdx;
                int curOfs;
                int dstLabel;
                int targetOfs;

                if (op != HCOP_BRA)
                {
                    X86Reg_e s0 = (X86Reg_e)HC_getOpcodeSrc0(instr);
                    int knownOnS0 = 0;

                    if (EncodeContext_flagsKnown(&ctx, branchFlags[op - HCOP_BRA]) != 0)
                        knownOnS0 = (s0 == EncodeContext_flagsReg(&ctx));
                    if (hasImm != 0)
                    {
                        int elided = 0;
                        if (HC_getOpcodeImm(instr) == 0 && knownOnS0 != 0)
                        {
                            elided = 1;
                            EncodeContext_retainFlags(&ctx, 3u);
                        }
                        if (elided == 0)
                            x86ALUOpImm(&ctx, cmpOp, s0, HC_getOpcodeImm(instr));
                    }
                    else
                    {
                        int elided = 0;
                        if (op == HCOP_CBNA || op == HCOP_CBA)
                        {
                            int s1 = HC_getOpcodeSrc1(instr);
                            if (HC_getOpcodeSrc0(instr) == s1 && knownOnS0 != 0)
                            {
                                elided = 1;
                                EncodeContext_retainFlags(&ctx, 3u);
                            }
                        }
                        if (elided == 0)
                            x86ALUOp(&ctx, cmpOp, s0, (X86Reg_e)HC_getOpcodeSrc1(instr));
                    }
                }

                brIdx = op - HCOP_BRA;
                curOfs = (int)(ctx.out - bodyStart);
                dstLabel = HC_getOpcodeDst(instr);
                targetOfs = labelOffsets[dstLabel];
                if (targetOfs == -1 || isNegativeByte(targetOfs - (curOfs + 2)) == 0)
                {
                    if (op == HCOP_BRA)
                    {
                        *ctx.out++ = 0xE9u;
                        emitI32(ctx.out, targetOfs - (curOfs + 5));
                    }
                    else
                    {
                        unsigned char *p = ctx.out;
                        p[0] = 0x0Fu;
                        p[1] = branchInstrs[2 * brIdx + 1];
                        ctx.out += 2;
                        emitI32(ctx.out, targetOfs - (curOfs + 6));
                    }
                    ctx.out += 4;
                }
                else
                {
                    genSmallBranch(&ctx, brIdx,
                                   (unsigned char)((unsigned char)targetOfs - ((unsigned char)curOfs + 2u)));
                }
                break;
            }

            case HCOP_LABEL:
            {
                int lbl;
                EncodeContext_clearFlags(&ctx);
                lbl = HC_getOpcodeLabel(instr);
                labelOffsets[lbl] = (short)(ctx.out - bodyStart);
                break;
            }

            default:
                break;
            }

            growMemory(mgr, (int)(ctx.out - codeBase), &allocated);
            ++pc;
        }

        /* Epilogue: restore caller's esp from [esp+4], restore callee/scratch
         * regs from below caller's esp, and ret. */
        loadBaseOffset(&ctx, 2, EXTEND_UNSIGNED, REG_ESP, 4, REG_ESP);
        loadBaseOffset(&ctx, 2, EXTEND_UNSIGNED, REG_ESP, -4, REG_EBX);
        loadBaseOffset(&ctx, 2, EXTEND_UNSIGNED, REG_ESP, -8, REG_ECX);
        loadBaseOffset(&ctx, 2, EXTEND_UNSIGNED, REG_ESP, -12, REG_EDX);
        loadBaseOffset(&ctx, 2, EXTEND_UNSIGNED, REG_ESP, -16, REG_ESI);
        loadBaseOffset(&ctx, 2, EXTEND_UNSIGNED, REG_ESP, -20, REG_EDI);
        loadBaseOffset(&ctx, 2, EXTEND_UNSIGNED, REG_ESP, -24, REG_EBP);
        *ctx.out++ = 0xC3u;
        codeEnd = ctx.out;
    }

    HCArena_releaseTop(mgr, topMark);
    return (int)(codeEnd - codeBase);
}

/* --- pre/post-regalloc fixers --------------------------------------------- */

static int scanMaxReg(HCInstr_s *lst)
{
    int maxR = 0;
    int i = 0;
    int prev;

    do
    {
        HCInstr_s *instr = &lst[i];
        int uses[3];
        int nUses = HC_getUses(instr, uses);
        int j;

        for (j = 0; j < nUses; ++j)
            maxR = HC_max(maxR, uses[j]);
        maxR = HC_max(maxR, HC_getDef(instr));
        prev = i++;
    }
    while (HC_getOpcodeOper(&lst[prev]) != HCOP_EOP);

    return maxR + 1;
}

static int HCX86Backend_preRegallocFixer(void *mgrVoid, HCInstr_s **dest,
                                         HCInstr_s *src, void *compHandle)
{
    HCArena_s *mgr = (HCArena_s *)mgrVoid;
    int inIdx = 0;
    int tmpReg = scanMaxReg(src);
    int outIdx = 0;
    int mark = HCArena_markBottom(mgr);
    HCInstr_s *out = (HCInstr_s *)HCArena_current(mgr);
    int prev;

    (void)compHandle;

    do
    {
        HCInstr_s *instr = &src[inIdx];
        int op = HC_getOpcodeOper(instr);

        if ((op == HCOP_LSL || op == HCOP_LSR || op == HCOP_ASR
             || op == HCOP_ROL || op == HCOP_ROR)
            && HC_opcodeHasImm(instr) == 0
            && HC_getOpcodeSrc1(instr) != 3)
        {
            HCArena_growCurrent(mgr, 0x20u);
            out[outIdx++] = HC_genOp2(HCOP_MOV, tmpReg, 3);
            out[outIdx++] = HC_genOp2(HCOP_MOV, 3, HC_getOpcodeSrc1(instr));
            out[outIdx++] = HC_genOp3(op, HC_getOpcodeDst(instr), HC_getOpcodeSrc0(instr), 3);
            out[outIdx++] = HC_genOp2(HCOP_MOV, 3, tmpReg);
        }
        else if (op == HCOP_BIC)
        {
            int dst = HC_getOpcodeDst(instr);
            int s0 = HC_getOpcodeSrc0(instr);
            if (HC_opcodeHasImm(instr) != 0)
            {
                HCArena_growCurrent(mgr, 8u);
                out[outIdx++] = HC_genOp3Imm(HCOP_AND, dst, s0, ~HC_getOpcodeImm(instr));
            }
            else
            {
                HCArena_growCurrent(mgr, 0x10u);
                out[outIdx++] = HC_genOp2(HCOP_MOVN, tmpReg, HC_getOpcodeSrc1(instr));
                out[outIdx++] = HC_genOp3(HCOP_AND, dst, s0, tmpReg);
            }
        }
        else
        {
            HCArena_growCurrent(mgr, 8u);
            out[outIdx++] = *instr;
        }
        prev = inIdx++;
    }
    while (HC_getOpcodeOper(&src[prev]) != HCOP_EOP);

    *dest = out;
    return mark;
}

static unsigned int freeRegMask(void *compInfo, int i)
{
    return HC_getFreeHardRegs(compInfo, i) & 0xFFFFFFFEu;
}

static int HCX86Backend_postRegallocFixer(void *mgrVoid, HCInstr_s **dest,
                                          HCInstr_s *src, void *compInfo)
{
    HCArena_s *mgr = (HCArena_s *)mgrVoid;
    int inIdx = 0;
    int outIdx = 0;
    int mark = HCArena_markBottom(mgr);
    HCInstr_s *out = (HCInstr_s *)HCArena_current(mgr);
    int prev;

    do
    {
        HCInstr_s *instr = &src[inIdx];
        int op = HC_getOpcodeOper(instr);
        int opType = HC_getOpcodeTypeOp(op);

        if (opType != HCOP_TYPE_ALUOP || op == HCOP_ADD)
        {
            if (opType == HCOP_TYPE_MEMOP && HC_isStoreOp(op) != 0)
            {
                int valReg = HC_getOpcodeDst(instr);
                int baseReg = HC_getOpcodeSrc0(instr);

                /* HC hard regs 1..4 (mask 0x1E) map to EAX/EBX/ECX/EDX, the
                 * only IA-32 registers with low-byte (AL/BL/CL/DL) encodings. */
                if (op == HCOP_SB && (((1u << valReg) & 0x1Eu) == 0))
                {
                    unsigned int usedMsk = (1u << valReg) | 1u | (1u << baseReg);
                    unsigned int freeMsk = freeRegMask(compInfo, inIdx);
                    int hasImm = HC_opcodeHasImm(instr);
                    int needSpill = 0;
                    int tmpReg;

                    if (hasImm == 0)
                        usedMsk |= (1u << HC_getOpcodeSrc1(instr));
                    freeMsk = (freeMsk & ~usedMsk) & 0x1Eu;
                    if (freeMsk != 0)
                    {
                        tmpReg = HC_bsf(freeMsk);
                    }
                    else
                    {
                        needSpill = 1;
                        tmpReg = HC_bsf((~usedMsk) & 0x1Eu);
                    }
                    if (needSpill != 0)
                    {
                        HCArena_growCurrent(mgr, 8u);
                        out[outIdx++] = HC_genOp3Imm(HCOP_SD, tmpReg, 0, 8);
                    }
                    HCArena_growCurrent(mgr, 0x10u);
                    out[outIdx++] = HC_genOp2(HCOP_MOV, tmpReg, valReg);
                    if (hasImm != 0)
                        out[outIdx++] = HC_genOp3Imm(HCOP_SB, tmpReg, baseReg, HC_getOpcodeImm(instr));
                    else
                        out[outIdx++] = HC_genOp3(HCOP_SB, tmpReg, baseReg, HC_getOpcodeSrc1(instr));
                    if (needSpill != 0)
                    {
                        HCArena_growCurrent(mgr, 8u);
                        out[outIdx++] = HC_genOp3Imm(HCOP_LD, tmpReg, 0, 8);
                    }
                    goto next_instr;
                }
            }
            HCArena_growCurrent(mgr, 8u);
            out[outIdx++] = *instr;
        }
        else
        {
            if (op == HCOP_SUB && HC_opcodeHasImm(instr) != 0)
            {
                HCArena_growCurrent(mgr, 8u);
                out[outIdx++] = HC_genOp3Imm(HCOP_ADD,
                                             HC_getOpcodeDst(instr),
                                             HC_getOpcodeSrc0(instr),
                                             -HC_getOpcodeImm(instr));
                goto next_instr;
            }
            if (HC_getOpcodeArgCount(instr) != 3)
            {
                HCArena_growCurrent(mgr, 8u);
                out[outIdx++] = *instr;
                goto next_instr;
            }

            {
                int dst = HC_getOpcodeDst(instr);
                int s0 = HC_getOpcodeSrc0(instr);

                if (HC_opcodeHasImm(instr) != 0)
                {
                    int imm = HC_getOpcodeImm(instr);
                    if (op == HCOP_LSL && (unsigned int)(imm - 1) <= 2u)
                    {
                        HCArena_growCurrent(mgr, 8u);
                        out[outIdx++] = HC_genOp3Imm(op, dst, s0, imm);
                    }
                    else
                    {
                        if (dst == s0)
                        {
                            HCArena_growCurrent(mgr, 8u);
                        }
                        else
                        {
                            HCArena_growCurrent(mgr, 0x10u);
                            out[outIdx++] = HC_genOp2(HCOP_MOV, dst, s0);
                        }
                        out[outIdx++] = HC_genOp3Imm(op, dst, dst, imm);
                    }
                }
                else
                {
                    int s1 = HC_getOpcodeSrc1(instr);
                    if (dst == s0)
                    {
                        HCArena_growCurrent(mgr, 8u);
                        out[outIdx++] = HC_genOp3(op, dst, dst, s1);
                    }
                    else if (dst == s1)
                    {
                        if (HC_opcodeIsCommutative(instr) != 0)
                        {
                            HCArena_growCurrent(mgr, 8u);
                            out[outIdx++] = HC_genOp3(op, dst, dst, s0);
                        }
                        else
                        {
                            unsigned int usedMsk = (1u << dst) | 1u | (1u << s0);
                            unsigned int freeMsk = freeRegMask(compInfo, inIdx);
                            int tmpReg;

                            if ((freeMsk & ~255u) != 0)
                                HC_assertFail("(freeMsk & ~255u) == 0", "hcX86Backend.c", 1940, NULL);
                            freeMsk &= ~usedMsk;
                            tmpReg = (freeMsk != 0) ? HC_bsf(freeMsk) : HC_bsf(~usedMsk);
                            if (freeMsk == 0)
                            {
                                HCArena_growCurrent(mgr, 8u);
                                out[outIdx++] = HC_genOp3Imm(HCOP_SD, tmpReg, 0, 8);
                            }
                            HCArena_growCurrent(mgr, 0x18u);
                            out[outIdx++] = HC_genOp2(HCOP_MOV, tmpReg, s0);
                            out[outIdx++] = HC_genOp3(op, tmpReg, tmpReg, s1);
                            out[outIdx++] = HC_genOp2(HCOP_MOV, dst, tmpReg);
                            if (freeMsk == 0)
                            {
                                HCArena_growCurrent(mgr, 8u);
                                out[outIdx++] = HC_genOp3Imm(HCOP_LD, tmpReg, 0, 8);
                            }
                        }
                    }
                    else
                    {
                        HCArena_growCurrent(mgr, 0x10u);
                        out[outIdx++] = HC_genOp2(HCOP_MOV, dst, s0);
                        out[outIdx++] = HC_genOp3(op, dst, dst, s1);
                    }
                }
            }
        }

    next_instr:
        prev = inIdx++;
    }
    while (HC_getOpcodeOper(&src[prev]) != HCOP_EOP);

    *dest = out;
    return mark;
}

/* --- vtable & factory ----------------------------------------------------- */

static const HCBackend_s backendDecl =
{
    &HCX86Backend_canCreateImmediate,
    &HCX86Backend_internalStackSpace,
    NULL,
    &HCX86Backend_targetRegisterMapping,
    &HCX86Backend_numHardRegs,
    &HCX86Backend_preRegallocFixer,
    &HCX86Backend_codegen,
    &HCX86Backend_postRegallocFixer,
    &HCX86Backend_emulate,
    NULL,
    &HCX86Backend_isNative,
    NULL
};

HCBackend_s *HCX86Backend_create(void)
{
    return (HCBackend_s *)&backendDecl;
}
