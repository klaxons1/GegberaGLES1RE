/* grInterpreter.c - grExecuteProgram (grInterpreter.inl).
 *
 * A bytecode VM that interprets g_pixelPipeBytecode (3122 bytes) to generate
 * HCInstr sequences for the pixel pipeline. The VM has:
 *   - an execute stack (grows up from &executeStack[62])
 *   - a repeat stack (grows up from &executeStack[52])
 *   - a return stack (grows up from &executeStack[36])
 *   - current texture/gradient/color indices
 *
 * Bytecode format:
 *   - byte & 0x80: control instruction (low 5 bits = opcode, bits 5-6 = pop);
 *     HC instructions loop back without popping, control instructions pop
 *     then dispatch on the low 5 bits (case 9 ends the program)
 *   - else: HC instruction (low 6 bits = opcode, bit 6 = hasImm, then
 *     dst/src0/src1 bytes with register remapping); 16/32-bit immediates
 *     are big-endian
 */
#include <string.h>
#include "gerbera/hc/hcOpcode.h"
#include "gerbera/grState.h"
#include "gerbera/grDefs.h"
#include "hg/hgInt32.h"

extern const unsigned char g_pixelPipeBytecode[3122];

#define EXEC_STACK_SIZE 190
#define EXEC_START_OFFSET 62
#define REPEAT_STACK_OFFSET 52
#define RETURN_STACK_OFFSET 36

int grExecuteProgram(HCInstr_s *result, _grSignature *signature, int resultLength)
{
    int executeStack[EXEC_STACK_SIZE];
    unsigned char *program = (unsigned char *)g_pixelPipeBytecode;
    unsigned char *p = program;
    int *stackPtr = &executeStack[EXEC_START_OFFSET];
    int *repeatPtr = &executeStack[REPEAT_STACK_OFFSET];
    int *returnPtr = &executeStack[RETURN_STACK_OFFSET];
    HCInstr_s *dst = result;
    int dstIndex = 0;
    int curTexture = 0;
    int curGradient = 0;
    int curColor = 0;

    memset(executeStack, 0xCC, sizeof(executeStack));

    if (result == NULL && grAssert("result && dst", "grInterpreter.inl", 59) != 0)
        grDie(0);
    if (signature->constantWriteColor != 0)
    {
        if (signature->flatShading == 0 &&
            grAssert("signature->flatShading", "grInterpreter.inl", 66) != 0)
            grDie(0);
    }

    for (;;) /* outer loop: control instructions */
    {
        unsigned char opcode;
        int popCount;
        int stackTop;      /* byteImm[23]: stackPtr[-1] before pop */
        int stackSecond;   /* byteImm[22]: stackPtr[-2] before pop */

        for (;;) /* inner loop: HC instructions loop back without popping */
        {
    LABEL_10:
            opcode = *p++;
            stackTop = stackPtr[-1];
            stackSecond = stackPtr[-2];
            if (stackPtr < &executeStack[EXEC_START_OFFSET] &&
                grAssert("stackPtr >= &executeStack[START_OFFSET]", "grInterpreter.inl", 80) != 0)
                grDie(0);
            if (!hgInRange32((int)(p - 1 - program), 0, 3122) &&
                grAssert("hgInRange32(p-1-program, 0, sizeof(g_pixelPipeBytecode))",
                         "grInterpreter.inl", 81) != 0)
                grDie(0);
            if ((opcode & 0x80) != 0)
                break; /* control instruction: pop + switch below */

            {
            /* HC instruction */
            int op = opcode & 0x3F;
            int dstReg, src0Reg, src1Reg;
            int hasImm;
            int imm = 0;
            int i;

            if (op == 55)
                op = stackTop;
            if (op == 54)
            {
                int label = *p++;

                if (hgInRange32(label, 11, 13) != 0)
                    label += 3 * curTexture;
                if (dstIndex++ >= resultLength &&
                    grAssert("dstIndex++ < resultLength", "grInterpreter.inl", 270) != 0)
                    grDie(0);
                *dst++ = HC_genOp1Imm(HCOP_LABEL, label);
            }
            else
            {
                dstReg = *p++;
                src0Reg = *p++;
                src1Reg = *p++;
                hasImm = (opcode & 0x40) != 0;

                /* register remapping (3 rounds: dst, src0, src1) */
                for (i = 0; i < 3; ++i)
                {
                    if (dstReg == 254)
                        dstReg = stackTop;
                    if (dstReg == 253)
                        dstReg = stackSecond;
                    if (dstReg == (int)0xCCCCCCCC)
                    {
                        if (grAssert("rDst != 0xcccccccc", "grInterpreter.inl", 292) != 0)
                            grDie(0);
                    }
                    if (hgInRange32(dstReg, 128, 141) != 0)
                        dstReg += 14 * curTexture;
                    if (dstReg == 160 || dstReg == 171 || dstReg == 182 || dstReg == 193)
                        dstReg += curGradient + 1;
                    if (dstReg == 70 || dstReg == 75 || dstReg == 80 || dstReg == 90 ||
                        dstReg == 95 || dstReg == 100 || dstReg == 105 || dstReg == 110 ||
                        dstReg == 115 || dstReg == 120)
                        dstReg += curColor + 1;
                    {
                        int tmp = dstReg;
                        dstReg = src0Reg;
                        src0Reg = src1Reg;
                        src1Reg = tmp;
                    }
                }
                if (src1Reg == (int)0xCCCCCCCC)
                {
                    if (grAssert("rSrc1 != 0xcccccccc", "grInterpreter.inl", 317) != 0)
                        grDie(0);
                }
                if (hasImm != 0)
                {
                    imm = *p++;
                    if (src1Reg == 0)
                    {
                        /* 4-byte immediate, big-endian */
                        imm = (int)(((unsigned)imm << 24) | ((unsigned)(*p & 0xFF) << 16) |
                                    ((unsigned)(p[1] & 0xFF) << 8) | (unsigned)(p[2] & 0xFF));
                        p += 3;
                    }
                }
                if (src1Reg == 255)
                {
                    if (hasImm != 0 &&
                        grAssert("!hasImm", "grInterpreter.inl", 340) != 0)
                        grDie(0);
                    hasImm = 1;
                    imm = stackTop;
                }
                if (HC_isBranchOp(op) != 0 && hgInRange32(dstReg, 11, 13) != 0)
                    dstReg += 3 * curTexture;
                if (src1Reg == (int)0xCCCCCCCC)
                {
                    if (grAssert("rSrc1 != 0xcccccccc && \"\"", "grInterpreter.inl", 351) != 0)
                        grDie(0);
                }
                if (dstIndex++ >= resultLength &&
                    grAssert("dstIndex++ < resultLength", "grInterpreter.inl", 352) != 0)
                    grDie(0);
                if (hasImm != 0)
                    *dst++ = HC_genOp3ImmRaw(op, dstReg, src0Reg, imm);
                else
                    *dst++ = HC_genOp3Raw(op, dstReg, src0Reg, src1Reg);
            }
            } /* end HC instruction */
            /* inner loop: HC instructions fall back to LABEL_10 (no pop) */
        } /* end inner loop */

        /* control instruction: pop operands, then dispatch */
        popCount = (opcode >> 5) & 3;
        stackPtr -= popCount;
        if ((opcode & 0x1F) == 9)
            break;
        switch (opcode & 0x1F)
        {
        case 0: /* push return, jump */
            *returnPtr++ = (int)(p + 2 - program);
            { int off = (p[1] | (p[0] << 8)); p = program + off; }
            break;
        case 1: /* conditional jump if top == byte */
            p += 3;
            if (stackPtr[-1] == *(unsigned char *)(p - 3))
            {
                *returnPtr++ = (int)(p - program);
                p = program + (*(unsigned char *)(p - 1) | (*(unsigned char *)(p - 2) << 8));
            }
            break;
        case 2: /* return */
            p = program + *(--returnPtr);
            break;
        case 3: /* push signature byte */
            {
                int ndx = *p++;

                if (hgInRange32(ndx, 56, 99) != 0)
                {
                    if (!hgInRange32(curTexture, 0, 1) &&
                        grAssert("hgInRange32(curTexture, 0, GR_MAX_TEXTURE_UNITS-1)",
                                 "grInterpreter.inl", 108) != 0)
                        grDie(0);
                    ndx += 44 * curTexture;
                }
                if (!hgInRange32(ndx, 0, 144) &&
                    grAssert("hgInRange32(ndx, 0, sizeof(grSignature))",
                             "grInterpreter.inl", 111) != 0)
                    grDie(0);
                *stackPtr++ = ((unsigned char *)signature)[ndx];
            }
            break;
        case 4: /* push immediate byte */
            *stackPtr++ = *p++;
            break;
        case 5: /* dup stack top at offset */
            { int n = *p++; int v = stackPtr[-1 - n]; *stackPtr++ = v; }
            break;
        case 6: /* push bytecode byte at (top + top-1) */
            {
                int offset = (p[1] | (p[0] << 8));
                p += 2;
                *stackPtr++ = program[stackTop + offset];
            }
            break;
        case 7: /* push repeat stack top */
            if ((repeatPtr - &executeStack[REPEAT_STACK_OFFSET]) <= 0 &&
                grAssert("repeatPtr - repeatStack > 0", "grInterpreter.inl", 135) != 0)
                grDie(0);
            *stackPtr++ = repeatPtr[-1];
            break;
        case 8: /* swap stack top with stack top-n */
            {
                int n = *p++;
                int tmp;

                if ((stackPtr - &executeStack[EXEC_START_OFFSET]) < n + 1 &&
                    grAssert("(stackPtr - &executeStack[START_OFFSET]) >= p[0]+1",
                             "grInterpreter.inl", 140) != 0)
                    grDie(0);
                tmp = stackPtr[-1 - n];
                stackPtr[-1 - n] = stackPtr[-1];
                stackPtr[-1] = tmp;
            }
            break;
        case 9: /* invalid */
            if (grAssert("!\"disasm output: invalid exec\"", "grInterpreter.inl", 257) != 0)
                grDie(0);
            break;
        case 0xA: /* jump */
            p = program + (p[1] | (p[0] << 8));
            break;
        case 0xB: /* conditional jump if top == 0 */
            {
                int offset = (p[1] | (p[0] << 8));
                p += 2;
                if (stackTop == 0)
                    p = program + offset;
            }
            break;
        case 0xC: /* push repeat frame */
            *repeatPtr++ = stackTop;
            *repeatPtr++ = 0;
            break;
        case 0xD: /* increment repeat, loop */
            {
                int offset = (p[1] | (p[0] << 8));
                p += 2;
                if (++repeatPtr[-1] == repeatPtr[-2])
                    repeatPtr -= 2;
                else
                    p = program + offset;
            }
            break;
        case 0xE: /* set curGradient */
            if (repeatPtr <= &executeStack[REPEAT_STACK_OFFSET] &&
                grAssert("repeatPtr > &repeatStack[0]", "grInterpreter.inl", 204) != 0)
                grDie(0);
            curGradient = repeatPtr[-1];
            if (!hgInRange32(curGradient, 0, 9) &&
                grAssert("hgInRange32(curGradient, 0, GR_GRADIENT_MAX-1)",
                         "grInterpreter.inl", 206) != 0)
                grDie(0);
            break;
        case 0xF: /* set curTexture */
            if (repeatPtr <= &executeStack[REPEAT_STACK_OFFSET] &&
                grAssert("repeatPtr > &repeatStack[0]", "grInterpreter.inl", 210) != 0)
                grDie(0);
            curTexture = repeatPtr[-1];
            if (!hgInRange32(curTexture, 0, 1) &&
                grAssert("hgInRange32(curTexture, 0, GR_MAX_TEXTURE_UNITS-1)",
                         "grInterpreter.inl", 212) != 0)
                grDie(0);
            break;
        case 0x10: /* set curColor */
            if (repeatPtr <= &executeStack[REPEAT_STACK_OFFSET] &&
                grAssert("repeatPtr > &repeatStack[0]", "grInterpreter.inl", 216) != 0)
                grDie(0);
            curColor = repeatPtr[-1];
            if (!hgInRange32(curColor, 0, 3) &&
                grAssert("hgInRange32(curColor, 0, 3)", "grInterpreter.inl", 218) != 0)
                grDie(0);
            break;
        case 0x11: /* push descriptor bytes */
            {
                int texDesc = *p++;
                unsigned char *desc;

                if (texDesc != 0)
                    desc = (unsigned char *)&signature->texture[curTexture];
                else
                    desc = (unsigned char *)signature;
                if (texDesc != 0 && !hgInRange32(curTexture, 0, 1) &&
                    grAssert("!texDesc || hgInRange32(curTexture, 0, GR_MAX_TEXTURE_UNITS-1)",
                             "grInterpreter.inl", 226) != 0)
                    grDie(0);
                *stackPtr++ = desc[3];
                *stackPtr++ = desc[7];
                *stackPtr++ = desc[2];
                *stackPtr++ = desc[6];
                *stackPtr++ = desc[1];
                *stackPtr++ = desc[5];
                *stackPtr++ = desc[0];
                *stackPtr++ = desc[4];
            }
            break;
        case 0x12: /* continue */
            goto LABEL_10;
        case 0x13: /* push (top + top-1) */
            *stackPtr++ = stackSecond + stackTop;
            break;
        case 0x14: /* push (top - second) */
            *stackPtr++ = stackTop - stackSecond;
            break;
        case 0x15: /* push (~top) */
            *stackPtr++ = ~stackTop;
            break;
        case 0x16: /* push (top & top-1) */
            *stackPtr++ = stackSecond & stackTop;
            break;
        case 0x17: /* push (top | top-1) */
            *stackPtr++ = stackSecond | stackTop;
            break;
        case 0x18: /* push (top ^ top-1) */
            *stackPtr++ = stackSecond ^ stackTop;
            break;
        case 0x19: /* push (top == 0) */
            *stackPtr++ = stackTop == 0;
            break;
        case 0x1A: /* push (top && top-1) */
            *stackPtr++ = stackSecond != 0 && stackTop != 0;
            break;
        case 0x1B: /* push (top || top-1) */
            *stackPtr++ = stackSecond != 0 || stackTop != 0;
            break;
        case 0x1C: /* push (second << top) */
            *stackPtr++ = (int)((unsigned)stackSecond << (stackTop & 31));
            break;
        case 0x1D: /* push (top * top-1) */
            *stackPtr++ = stackSecond * stackTop;
            break;
        case 0x1E: /* push (top == top-1) */
            *stackPtr++ = stackSecond == stackTop;
            break;
        case 0x1F: /* push (top != top-1) */
            *stackPtr++ = stackSecond != stackTop;
            break;
        }
    }

    if (dstIndex++ >= resultLength &&
        grAssert("dstIndex++ < resultLength", "grInterpreter.inl", 93) != 0)
        grDie(0);
    *dst++ = HC_genOp0(HCOP_EOP);
    if (stackPtr != &executeStack[EXEC_START_OFFSET] &&
        grAssert("stackPtr == &executeStack[START_OFFSET]", "grInterpreter.inl", 95) != 0)
        grDie(0);
    if (dstIndex != (int)(dst - result) &&
        grAssert("dstIndex == (dst - result)", "grInterpreter.inl", 360) != 0)
        grDie(0);
    return (int)(dst - result);
}
