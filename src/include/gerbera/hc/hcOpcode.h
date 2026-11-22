/* hcOpcode.h - HC instruction encoding (hcOpcode.h / hcOpcode.c).
 *
 * An HCInstr is two 32-bit words:
 *   instr0: bits 0-7 = opcode, bit 8 = hasImm, bits 9 = (unused),
 *           bits 10-20 = dst register, bits 21-31 = src0 register
 *   instr1: src1 register (bits 0-10) or immediate value (full word)
 *
 * The opcode table HC_opcodeDecls[55] encodes per-opcode metadata:
 *   bits 0-2: argument count
 *   bits 3-4: type (0=ALUOP, 1=MEMOP, 2=BRANCH)
 *   bit 5: commutative
 *   bits 6-7: dst flags
 */
#ifndef GERBERA_HC_OPCODE_H
#define GERBERA_HC_OPCODE_H

/* --- instruction ---------------------------------------------------------- */
typedef struct _HCInstr
{
    unsigned int instr0;
    unsigned int instr1;
} HCInstr_s;

/* --- opcode enum (order pinned by HC_opcodeDecls + range checks) ---------- */
enum HCOP_e
{
    HCOP_NOP = 0,
    HCOP_MOV = 1,
    HCOP_MOVN = 2,
    HCOP_ADD = 3,
    HCOP_SUB = 4,
    HCOP_SUBR = 5,
    HCOP_AND = 6,
    HCOP_OR = 7,
    HCOP_XOR = 8,
    HCOP_BIC = 9,
    HCOP_MULS = 10,
    HCOP_MULU = 11,
    HCOP_LSL = 12,
    HCOP_LSR = 13,
    HCOP_ASR = 14,
    HCOP_ROL = 15,
    HCOP_ROR = 16,
    HCOP_EOP = 17,
    HCOP_ASD = 18,
    HCOP_MAXS = 19,
    HCOP_MINS = 20,
    HCOP_MAXU = 21,
    HCOP_MINU = 22,
    HCOP_LBS = 23,
    HCOP_LSD = 24,
    HCOP_SB = 25,
    HCOP_SW = 26,
    HCOP_SW2 = 27,
    HCOP_LD = 28,
    HCOP_SD = 29,
    HCOP_LD4 = 30,
    HCOP_SD4 = 31,
    HCOP_BRA = 39,
    HCOP_LABEL = 40,
    HCOP_CBEQ = 41,
    HCOP_CBNEQ = 42,
    HCOP_CBGTS = 43,
    HCOP_CBGTU = 44,
    HCOP_CBGES = 45,
    HCOP_CBGEU = 46,
    HCOP_CBLTS = 47,
    HCOP_CBLTU = 48,
    HCOP_CBLEU = 49,
    HCOP_CBLEU2 = 50,
    HCOP_CBNA = 51,
    HCOP_CBA = 52,
    HCOP_MAX = 55
};

/* opcode types (HC_opcodeDecls bits 3-4) */
enum HCOP_TYPE_e
{
    HCOP_TYPE_ALUOP = 0,
    HCOP_TYPE_MEMOP = 1,
    HCOP_TYPE_BRANCH = 2
};

/* dst flags (HC_opcodeDecls bits 6-7) */
enum HCOP_DST_e
{
    HCOP_DST_NORMAL = 0,
    HCOP_DST_AS_SRC = 1,
    HCOP_DST_SIZE = 2
};

#define HCOP_OPER_SIZE 8
#define HCOP_DST_SIZE 11
#define HCREG_SIZE 11
#define HC_MAX_REG_INDEX 2048

/* --- opcode table --------------------------------------------------------- */
extern const unsigned char HC_opcodeDecls[55];

/* --- instruction generation (hcOpcode.c) ---------------------------------- */
HCInstr_s HC_genOp0(int op);
HCInstr_s HC_genOp1(int op, int dst);
HCInstr_s HC_genOp1Imm(int op, int imm);
HCInstr_s HC_genOp2(int op, int dst, int src0);
HCInstr_s HC_genOp2Imm(int op, int dst, int imm);
HCInstr_s HC_genOp3(int op, int dst, int src0, int src1);
HCInstr_s HC_genOp3Imm(int op, int dst, int src0, int imm);
HCInstr_s HC_genOp3Raw(int op, int dst, int src0, int src1);
HCInstr_s HC_genOp3ImmRaw(int op, int dst, int src0, int imm);

/* --- opcode accessors (hcOpcode.h) ---------------------------------------- */
int HC_getOpcodeOper(HCInstr_s *instr);
int HC_getOpcodeArgCountOp(int op);
int HC_getOpcodeArgCount(HCInstr_s *instr);
int HC_getOpcodeDst(HCInstr_s *instr);
int HC_getOpcodeSrc0Raw(HCInstr_s *instr);
int HC_getOpcodeSrc0(HCInstr_s *instr);
int HC_getOpcodeSrc1Raw(HCInstr_s *instr);
int HC_getOpcodeSrc1(HCInstr_s *instr);
int HC_isStoreOp(int op);
int HC_getOpcodeTypeOp(int op);
int HC_getOpcodeFlags(HCInstr_s *instr);
int HC_opcodeHasImm(HCInstr_s *instr);
int HC_opcodeIsCommutative(HCInstr_s *instr);
int HC_getOpcodeImm(HCInstr_s *instr);
int HC_getOpcodeLabel(HCInstr_s *instr);
void HC_setOpcodeSrc1(HCInstr_s *instr, int v);
int HC_getOpcodeType(HCInstr_s *instr);
int HC_isLoadOp(int op);
int HC_isCondBranchOp(int op);
int HC_isBranch(HCInstr_s *instr);
void HC_setOpcodeImm(HCInstr_s *instr, int v);
int HC_opcodeDstFlagsOp(int op);
void HC_setOpcodeDst(HCInstr_s *instr, int v);
void HC_setOpcodeNoImm(HCInstr_s *instr);
void HC_setOpcodeSrc0(HCInstr_s *instr, int v);
int HC_isBranchOp(int op);

/* --- assert --------------------------------------------------------------- */
void HC_assertFail(const char *expr, const char *file, int line, const char *msg);

#endif /* GERBERA_HC_OPCODE_H */
