/* libs/pixelflinger/codeflinger/ARMAssembler.cpp
**
** Copyright 2006, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License"); 
** you may not use this file except in compliance with the License. 
** You may obtain a copy of the License at 
**
**     http://www.apache.org/licenses/LICENSE-2.0 
**
** Unless required by applicable law or agreed to in writing, software 
** distributed under the License is distributed on an "AS IS" BASIS, 
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
** See the License for the specific language governing permissions and 
** limitations under the License.
*/

#define LOG_TAG "ARMAssembler"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cutils/log.h>
#include <cutils/properties.h>

#if defined(WITH_LIB_HARDWARE)
#include <hardware_legacy/qemu_tracing.h>
#endif

#include <private/pixelflinger/ggl_context.h>

#include "ARMAssembler.h"
#include "CodeCache.h"
#ifdef PF_ENABLE_DISASM
#include "disassem.h"
#endif

// ----------------------------------------------------------------------------

namespace android {

// ----------------------------------------------------------------------------
#if 0
#pragma mark -
#pragma mark ARMAssembler...
#endif

ARMAssembler::ARMAssembler(const sp<Assembly>& assembly)
    :   ARMAssemblerInterface(),
        mAssembly(assembly)
{
    mBase = mPC = (uint32_t *)assembly->base();
    ssize_t bytes = assembly->size();
    mLimit = (mBase && bytes > 0) ? (mBase + (bytes / 4)) : mBase;
    mPrologPC = mBase;
    mOverflow = (mBase == 0 || bytes <= 0);
    mDuration = ggl_system_time();
#if defined(WITH_LIB_HARDWARE)
    mQemuTracing = true;
#endif
}

ARMAssembler::~ARMAssembler()
{
}

uint32_t* ARMAssembler::pc() const
{
    return mPC;
}

uint32_t* ARMAssembler::base() const
{
    return mBase;
}

void ARMAssembler::reset()
{
    mBase = mPC = (uint32_t *)mAssembly->base();
    ssize_t bytes = mAssembly->size();
    mLimit = (mBase && bytes > 0) ? (mBase + (bytes / 4)) : mBase;
    mPrologPC = mBase;
    mOverflow = (mBase == 0 || bytes <= 0);
    mBranchTargets.clear();
    mLabels.clear();
    mLabelsInverseMapping.clear();
    mComments.clear();
}

int ARMAssembler::getCodegenArch()
{
    return CODEGEN_ARCH_ARM;
}

// ----------------------------------------------------------------------------

void ARMAssembler::disassemble(const char* name)
{
#ifdef PF_ENABLE_DISASM
    if (name) {
        printf("%s:\n", name);
    }
    size_t count = pc()-base();
    uint32_t* i = base();
    while (count--) {
        ssize_t label = mLabelsInverseMapping.indexOfKey(i);
        if (label >= 0) {
            printf("%s:\n", mLabelsInverseMapping.valueAt(label));
        }
        ssize_t comment = mComments.indexOfKey(i);
        if (comment >= 0) {
            printf("; %s\n", mComments.valueAt(comment));
        }
        printf("%08x:    %08x    ", uintptr_t(i), int(i[0]));
        ::disassemble((uintptr_t)i);
        i++;
    }
#else
    (void)name;
#endif
}

void ARMAssembler::comment(const char* string)
{
#ifdef PF_ENABLE_DISASM
    mComments.add(mPC, string);
#else
    (void)string;
#endif
}

void ARMAssembler::label(const char* theLabel)
{
    mLabels.add(theLabel, mPC);
    mLabelsInverseMapping.add(mPC, theLabel);
}

void ARMAssembler::B(int cc, const char* label)
{
    mBranchTargets.add(branch_target_t(label, mPC));
    emit((cc<<28) | (0xA<<24) | 0);
}

void ARMAssembler::BL(int cc, const char* label)
{
    mBranchTargets.add(branch_target_t(label, mPC));
    emit((cc<<28) | (0xB<<24) | 0);
}

#if 0
#pragma mark -
#pragma mark Prolog/Epilog & Generate...
#endif


void ARMAssembler::prolog()
{
    // write dummy prolog code
    mPrologPC = mPC;
    STM(AL, FD, SP, 1, LSAVED);
}

void ARMAssembler::epilog(uint32_t touched)
{
    touched &= LSAVED;
    if (touched) {
        // write prolog code
        uint32_t* pc = mPC;
        mPC = mPrologPC;
        STM(AL, FD, SP, 1, touched | LLR);
        mPC = pc;
        // write epilog code
        LDM(AL, FD, SP, 1, touched | LLR);
        BX(AL, LR);
    } else {   // heh, no registers to save!
        // write prolog code
        uint32_t* pc = mPC;
        mPC = mPrologPC;
        MOV(AL, 0, R0, R0); // NOP
        mPC = pc;
        // write epilog code
        BX(AL, LR);
    }
}

int ARMAssembler::generate(const char* name)
{
    if (mOverflow || mPC >= mLimit || mPC <= mBase) {
        return NO_MEMORY;
    }

    // fixup all the branches (pointer lookup first, strcmp fallback for safety)
    size_t count = mBranchTargets.size();
    while (count--) {
        const branch_target_t& bt = mBranchTargets[count];
        ssize_t idx = mLabels.indexOfKey(bt.label);
        uint32_t* target_pc = (idx >= 0) ? mLabels.valueAt(idx) : 0;
        if (!target_pc && bt.label) {
            for (size_t k = 0; k < mLabels.size(); ++k) {
                if (mLabels.keyAt(k) && strcmp(mLabels.keyAt(k), bt.label) == 0) {
                    target_pc = mLabels.valueAt(k);
                    break;
                }
            }
        }
        if (!target_pc || bt.pc < mBase || bt.pc >= mPC) {
            return -EINVAL;
        }
        int32_t offset = int32_t(target_pc - (bt.pc+2));
        *bt.pc |= offset & 0xFFFFFF;
    }

    mAssembly->resize( int(pc()-base())*4 );
    
    // the instruction cache is flushed by CodeCache
    const int64_t duration = ggl_system_time() - mDuration;
    (void)duration;
    (void)name;

#ifdef PF_ENABLE_DISASM
    const char * const format = "generated %s (%d ins) at [%p:%p] in %lld ns\n";
    ALOGI(format, name, int(pc()-base()), base(), pc(), duration);
    char value[PROPERTY_VALUE_MAX];
    property_get("debug.pf.disasm", value, "0");
    if (atoi(value) != 0) {
        printf(format, name, int(pc()-base()), base(), pc(), duration);
        disassemble(name);
    }
#endif
    
    return NO_ERROR;
}

uint32_t* ARMAssembler::pcForLabel(const char* label)
{
    return mLabels.valueFor(label);
}

// ----------------------------------------------------------------------------

#if 0
#pragma mark -
#pragma mark Data Processing...
#endif

void ARMAssembler::dataProcessing(int opcode, int cc,
        int s, int Rd, int Rn, uint32_t Op2)
{
    emit((cc<<28) | (opcode<<21) | (s<<20) | (Rn<<16) | (Rd<<12) | Op2);
}

#if 0
#pragma mark -
#pragma mark Multiply...
#endif

// multiply...
void ARMAssembler::MLA(int cc, int s,
        int Rd, int Rm, int Rs, int Rn) {
    if (Rd == Rm) { int t = Rm; Rm=Rs; Rs=t; } 
    LOG_FATAL_IF(Rd==Rm, "MLA(r%u,r%u,r%u,r%u)", Rd,Rm,Rs,Rn);
    emit((cc<<28) | (1<<21) | (s<<20) |
                (Rd<<16) | (Rn<<12) | (Rs<<8) | 0x90 | Rm);
}
void ARMAssembler::MUL(int cc, int s,
        int Rd, int Rm, int Rs) {
    if (Rd == Rm) { int t = Rm; Rm=Rs; Rs=t; } 
    LOG_FATAL_IF(Rd==Rm, "MUL(r%u,r%u,r%u)", Rd,Rm,Rs);
    emit((cc<<28) | (s<<20) | (Rd<<16) | (Rs<<8) | 0x90 | Rm);
}
void ARMAssembler::UMULL(int cc, int s,
        int RdLo, int RdHi, int Rm, int Rs) {
    LOG_FATAL_IF(RdLo==Rm || RdHi==Rm || RdLo==RdHi,
                        "UMULL(r%u,r%u,r%u,r%u)", RdLo,RdHi,Rm,Rs);
    emit((cc<<28) | (1<<23) | (s<<20) |
                (RdHi<<16) | (RdLo<<12) | (Rs<<8) | 0x90 | Rm);
}
void ARMAssembler::UMUAL(int cc, int s,
        int RdLo, int RdHi, int Rm, int Rs) {
    LOG_FATAL_IF(RdLo==Rm || RdHi==Rm || RdLo==RdHi,
                        "UMUAL(r%u,r%u,r%u,r%u)", RdLo,RdHi,Rm,Rs);
    emit((cc<<28) | (1<<23) | (1<<21) | (s<<20) |
                (RdHi<<16) | (RdLo<<12) | (Rs<<8) | 0x90 | Rm);
}
void ARMAssembler::SMULL(int cc, int s,
        int RdLo, int RdHi, int Rm, int Rs) {
    LOG_FATAL_IF(RdLo==Rm || RdHi==Rm || RdLo==RdHi,
                        "SMULL(r%u,r%u,r%u,r%u)", RdLo,RdHi,Rm,Rs);
    emit((cc<<28) | (1<<23) | (1<<22) | (s<<20) |
                (RdHi<<16) | (RdLo<<12) | (Rs<<8) | 0x90 | Rm);
}
void ARMAssembler::SMUAL(int cc, int s,
        int RdLo, int RdHi, int Rm, int Rs) {
    LOG_FATAL_IF(RdLo==Rm || RdHi==Rm || RdLo==RdHi,
                        "SMUAL(r%u,r%u,r%u,r%u)", RdLo,RdHi,Rm,Rs);
    emit((cc<<28) | (1<<23) | (1<<22) | (1<<21) | (s<<20) |
                (RdHi<<16) | (RdLo<<12) | (Rs<<8) | 0x90 | Rm);
}

#if 0
#pragma mark -
#pragma mark Branches...
#endif

// branches...
void ARMAssembler::B(int cc, uint32_t* pc)
{
    int32_t offset = int32_t(pc - (mPC+2));
    emit((cc<<28) | (0xA<<24) | (offset & 0xFFFFFF));
}

void ARMAssembler::BL(int cc, uint32_t* pc)
{
    int32_t offset = int32_t(pc - (mPC+2));
    emit((cc<<28) | (0xB<<24) | (offset & 0xFFFFFF));
}

void ARMAssembler::BX(int cc, int Rn)
{
    emit((cc<<28) | 0x12FFF10 | Rn);
}

#if 0
#pragma mark -
#pragma mark Data Transfer...
#endif

// data transfert...
void ARMAssembler::LDR(int cc, int Rd, int Rn, uint32_t offset) {
    emit((cc<<28) | (1<<26) | (1<<20) | (Rn<<16) | (Rd<<12) | offset);
}
void ARMAssembler::LDRB(int cc, int Rd, int Rn, uint32_t offset) {
    emit((cc<<28) | (1<<26) | (1<<22) | (1<<20) | (Rn<<16) | (Rd<<12) | offset);
}
void ARMAssembler::STR(int cc, int Rd, int Rn, uint32_t offset) {
    emit((cc<<28) | (1<<26) | (Rn<<16) | (Rd<<12) | offset);
}
void ARMAssembler::STRB(int cc, int Rd, int Rn, uint32_t offset) {
    emit((cc<<28) | (1<<26) | (1<<22) | (Rn<<16) | (Rd<<12) | offset);
}

void ARMAssembler::LDRH(int cc, int Rd, int Rn, uint32_t offset) {
    emit((cc<<28) | (1<<20) | (Rn<<16) | (Rd<<12) | 0xB0 | offset);
}
void ARMAssembler::LDRSB(int cc, int Rd, int Rn, uint32_t offset) {
    emit((cc<<28) | (1<<20) | (Rn<<16) | (Rd<<12) | 0xD0 | offset);
}
void ARMAssembler::LDRSH(int cc, int Rd, int Rn, uint32_t offset) {
    emit((cc<<28) | (1<<20) | (Rn<<16) | (Rd<<12) | 0xF0 | offset);
}
void ARMAssembler::STRH(int cc, int Rd, int Rn, uint32_t offset) {
    emit((cc<<28) | (Rn<<16) | (Rd<<12) | 0xB0 | offset);
}

#if 0
#pragma mark -
#pragma mark Block Data Transfer...
#endif

// block data transfer...
void ARMAssembler::LDM(int cc, int dir,
        int Rn, int W, uint32_t reg_list)
{   //                    ED FD EA FA      IB IA DB DA
    const uint8_t P[8] = { 1, 0, 1, 0,      1, 0, 1, 0 };
    const uint8_t U[8] = { 1, 1, 0, 0,      1, 1, 0, 0 };
    emit((cc<<28) | (4<<25) | (uint32_t(P[dir])<<24) |
            (uint32_t(U[dir])<<23) | (1<<20) | (W<<21) | (Rn<<16) | reg_list);
}

void ARMAssembler::STM(int cc, int dir,
        int Rn, int W, uint32_t reg_list)
{   //                    ED FD EA FA      IB IA DB DA
    const uint8_t P[8] = { 0, 1, 0, 1,      1, 0, 1, 0 };
    const uint8_t U[8] = { 0, 0, 1, 1,      1, 1, 0, 0 };
    emit((cc<<28) | (4<<25) | (uint32_t(P[dir])<<24) |
            (uint32_t(U[dir])<<23) | (0<<20) | (W<<21) | (Rn<<16) | reg_list);
}

#if 0
#pragma mark -
#pragma mark Special...
#endif

// special...
void ARMAssembler::SWP(int cc, int Rn, int Rd, int Rm) {
    emit((cc<<28) | (2<<23) | (Rn<<16) | (Rd << 12) | 0x90 | Rm);
}
void ARMAssembler::SWPB(int cc, int Rn, int Rd, int Rm) {
    emit((cc<<28) | (2<<23) | (1<<22) | (Rn<<16) | (Rd << 12) | 0x90 | Rm);
}
void ARMAssembler::SWI(int cc, uint32_t comment) {
    emit((cc<<28) | (0xF<<24) | comment);
}

#if 0
#pragma mark -
#pragma mark DSP instructions...
#endif

// DSP instructions...
void ARMAssembler::PLD(int Rn, uint32_t offset) {
    if (!((offset&(1<<24)) && !(offset&(1<<21)))) {
        mOverflow = true;
        return;
    }
    emit(0xF550F000 | (Rn<<16) | offset);
}

void ARMAssembler::CLZ(int cc, int Rd, int Rm)
{
    emit((cc<<28) | 0x16F0F10| (Rd<<12) | Rm);
}

void ARMAssembler::QADD(int cc,  int Rd, int Rm, int Rn)
{
    emit((cc<<28) | 0x1000050 | (Rn<<16) | (Rd<<12) | Rm);
}

void ARMAssembler::QDADD(int cc,  int Rd, int Rm, int Rn)
{
    emit((cc<<28) | 0x1400050 | (Rn<<16) | (Rd<<12) | Rm);
}

void ARMAssembler::QSUB(int cc,  int Rd, int Rm, int Rn)
{
    emit((cc<<28) | 0x1200050 | (Rn<<16) | (Rd<<12) | Rm);
}

void ARMAssembler::QDSUB(int cc,  int Rd, int Rm, int Rn)
{
    emit((cc<<28) | 0x1600050 | (Rn<<16) | (Rd<<12) | Rm);
}

void ARMAssembler::SMUL(int cc, int xy,
                int Rd, int Rm, int Rs)
{
    emit((cc<<28) | 0x1600080 | (Rd<<16) | (Rs<<8) | (xy<<4) | Rm);
}

void ARMAssembler::SMULW(int cc, int y,
                int Rd, int Rm, int Rs)
{
    emit((cc<<28) | 0x12000A0 | (Rd<<16) | (Rs<<8) | (y<<4) | Rm);
}

void ARMAssembler::SMLA(int cc, int xy,
                int Rd, int Rm, int Rs, int Rn)
{
    emit((cc<<28) | 0x1000080 | (Rd<<16) | (Rn<<12) | (Rs<<8) | (xy<<4) | Rm);
}

void ARMAssembler::SMLAL(int cc, int xy,
                int RdHi, int RdLo, int Rs, int Rm)
{
    emit((cc<<28) | 0x1400080 | (RdHi<<16) | (RdLo<<12) | (Rs<<8) | (xy<<4) | Rm);
}

void ARMAssembler::SMLAW(int cc, int y,
                int Rd, int Rm, int Rs, int Rn)
{
    emit((cc<<28) | 0x1200080 | (Rd<<16) | (Rn<<12) | (Rs<<8) | (y<<4) | Rm);
}

#if 0
#pragma mark -
#pragma mark Byte/half word extract and extend (ARMv6+ only)...
#endif

void ARMAssembler::UXTB16(int cc, int Rd, int Rm, int rotate)
{
    emit((cc<<28) | 0x6CF0070 | (Rd<<12) | ((rotate >> 3) << 10) | Rm);
}
#if 0
#pragma mark -
#pragma mark Bit manipulation (ARMv7+ only)...
#endif

// Bit manipulation (ARMv7+ only)...
void ARMAssembler::UBFX(int cc, int Rd, int Rn, int lsb, int width)
{
    emit((cc<<28) | 0x7E00000 | ((width-1)<<16) | (Rd<<12) | (lsb<<7) | 0x50 | Rn);
}

#if 0
#pragma mark -
#pragma mark Addressing modes...
#endif

int ARMAssembler::buildImmediate(
        uint32_t immediate, uint32_t& rot, uint32_t& imm)
{
    rot = 0;
    imm = immediate;
    if (imm > 0x7F) { // skip the easy cases
        while (!(imm&3)  || (imm&0xFC000000)) {
            uint32_t newval;
            newval = imm >> 2;
            newval |= (imm&3) << 30;
            imm = newval;
            rot += 2;
            if (rot == 32) {
                rot = 0;
                break;
            }
        }
    }
    rot = (16 - (rot>>1)) & 0xF;

    if (imm>=0x100)
        return -EINVAL;

    if (((imm>>(rot<<1)) | (imm<<(32-(rot<<1)))) != immediate)
        return -1;

    return 0;
}

// shifters...

bool ARMAssembler::isValidImmediate(uint32_t immediate)
{
    uint32_t rot, imm;
    return buildImmediate(immediate, rot, imm) == 0;
}

uint32_t ARMAssembler::imm(uint32_t immediate)
{
    uint32_t rot, imm;
    int err = buildImmediate(immediate, rot, imm);
    if (err) {
        mOverflow = true;
        return 0;
    }
    return (1<<25) | (rot<<8) | imm;
}

uint32_t ARMAssembler::reg_imm(int Rm, int type, uint32_t shift)
{
    return ((shift&0x1F)<<7) | ((type&0x3)<<5) | (Rm&0xF);
}

uint32_t ARMAssembler::reg_rrx(int Rm)
{
    return (ROR<<5) | (Rm&0xF);
}

uint32_t ARMAssembler::reg_reg(int Rm, int type, int Rs)
{
    return ((Rs&0xF)<<8) | ((type&0x3)<<5) | (1<<4) | (Rm&0xF);
}

// addressing modes...
// LDR(B)/STR(B)/PLD (immediate and Rm can be negative, which indicate U=0)
uint32_t ARMAssembler::immed12_pre(int32_t immed12, int W)
{
    if (abs(immed12) >= 0x800) {
        mOverflow = true;
        return 0;
    }
    return (1<<24) | (((uint32_t(immed12)>>31)^1)<<23) |
            ((W&1)<<21) | (abs(immed12)&0x7FF);
}

uint32_t ARMAssembler::immed12_post(int32_t immed12)
{
    if (abs(immed12) >= 0x800) {
        mOverflow = true;
        return 0;
    }
    return (((uint32_t(immed12)>>31)^1)<<23) | (abs(immed12)&0x7FF);
}

uint32_t ARMAssembler::reg_scale_pre(int Rm, int type,
        uint32_t shift, int W)
{
    return  (1<<25) | (1<<24) |
            (((uint32_t(Rm)>>31)^1)<<23) | ((W&1)<<21) |
            reg_imm(abs(Rm), type, shift);
}

uint32_t ARMAssembler::reg_scale_post(int Rm, int type, uint32_t shift)
{
    return (1<<25) | (((uint32_t(Rm)>>31)^1)<<23) | reg_imm(abs(Rm), type, shift);
}

// LDRH/LDRSB/LDRSH/STRH (immediate and Rm can be negative, which indicate U=0)
uint32_t ARMAssembler::immed8_pre(int32_t immed8, int W)
{
    uint32_t offset = abs(immed8);
    if (offset >= 0x100) {
        mOverflow = true;
        return 0;
    }
    return  (1<<24) | (1<<22) | (((uint32_t(immed8)>>31)^1)<<23) |
            ((W&1)<<21) | (((offset&0xF0)<<4)|(offset&0xF));
}

uint32_t ARMAssembler::immed8_post(int32_t immed8)
{
    uint32_t offset = abs(immed8);
    if (offset >= 0x100) {
        mOverflow = true;
        return 0;
    }
    return (1<<22) | (((uint32_t(immed8)>>31)^1)<<23) |
            (((offset&0xF0)<<4) | (offset&0xF));
}

uint32_t ARMAssembler::reg_pre(int Rm, int W)
{
    return (1<<24) | (((uint32_t(Rm)>>31)^1)<<23) | ((W&1)<<21) | (abs(Rm)&0xF);
}

uint32_t ARMAssembler::reg_post(int Rm)
{
    return (((uint32_t(Rm)>>31)^1)<<23) | (abs(Rm)&0xF);
}

}; // namespace android

