/* hcBackend.h - HCBackend_s JIT backend vtable (hcBackend.h of the original).
 *
 * Layout pinned from BOTH reference binaries (session 4c):
 *   - x86 WINSCW build: backendDecl initializer, docs/libGLES_CM.dll.c:5793
 *     (slots verified against compileInternal's member accesses at
 *      docs/libGLES_CM.dll.c:50302/50449/50493/51067/51452/51566/51628/51653/51677)
 *   - ARM GCCE build: backendDecl in the image at file offset 0x2F008
 *     (VA 0x36F6C), recovered from docs/libgles_cm.dll.epoc because the
 *     decompiler typed the object as only [2] entries; the vtable's targets
 *     were resolved in docs/libgles_cm.dll.epoc.c and verified against the
 *     image bytes (VA->file: off = VA - 0x7F64).
 *
 * Both builds use the SAME 12-slot struct (12 * 4 bytes), followed in the
 * data segment by swap[] = "3412/0-.+,)*".
 *
 * Slot values per build:
 *   member                        x86 (hcX86Backend.c)   ARM (hcARMBackend.c)
 *   canCreateImmediate            returns 1              sub_245C0 (rotated-imm8 check)
 *   internalStackSpace            40                     60  (sub_24100)
 *   internalStackSpaceAfterInvars NULL                   NULL
 *   targetRegisterMapping         &regRemap              &word_36F2A = {0..12,14} (sub_23D7C)
 *   numHardRegs                   8                      14  (sub_24108)
 *   preRegallocFixer              HCX86Backend_...       sub_2417C
 *   codegen                       HCX86Backend_codegen   sub_24AEC (emits ARM words)
 *   postRegallocFixer             present                NULL (compileWrap falls back)
 *   emulate                       program(stackPtr)      sub_24118 (same)
 *   reserved slot 9 (+36)         NULL                   sub_25480 (4-byte bx-lr no-op)
 *   isNative                      1                      1  (sub_24110)
 *   optimizationMask              NULL                   NULL
 *                                 (compileInternal/compileWrap NULL-check +44
 *                                  before calling; never non-NULL in either build)
 *
 * Slot 9's member name is unknown: it is NULL on x86 and never invoked via
 * the vtable in either decompilation; the ARM target is a bare `bx lr`.
 */
#ifndef GERBERA_HC_BACKEND_H
#define GERBERA_HC_BACKEND_H

#include "gerbera/hc/hcOpcode.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct HCBackend_s
{
    int (*canCreateImmediate)(HCInstr_s *instr);                    /* +0  */
    int (*internalStackSpace)(void);                                /* +4  */
    int (*internalStackSpaceAfterInvars)(void);                     /* +8  */
    unsigned char *(*targetRegisterMapping)(void);                  /* +12 */
    int (*numHardRegs)(void);                                       /* +16 */
    int (*preRegallocFixer)(void *mgr, HCInstr_s **dest,
                            HCInstr_s *src, void *compHandle);      /* +20 */
    int (*codegen)(void *mgr, void **destination, HCInstr_s *instrs,
                   void *info, int spillStart);                     /* +24 */
    int (*postRegallocFixer)(void *mgr, HCInstr_s **dest,
                             HCInstr_s *src, void *compInfo);       /* +28 */
    void (*emulate)(void (*program)(void *), void *stackPtr);       /* +32 */
    void (*reserved9)(void);                                        /* +36, name unknown */
    int (*isNative)(void);                                          /* +40 */
    int (*optimizationMask)(void);                                  /* +44 */
} HCBackend_s;

HCBackend_s *HCX86Backend_create(void);
HCBackend_s *HCARMBackend_create(void);

#ifdef __cplusplus
}
#endif

#endif /* GERBERA_HC_BACKEND_H */
