/* symbian_entry.S - Symbian OS v9.x (EKA2) ARM EABI DLL startup header,
 * C++ exception descriptor, kernel TLS thunks, and AAPCS setjmp/longjmp.
 */
    .syntax unified
    .arch armv5t
    .arm

    .section .init, "ax", %progbits
    .align 2
    .global _E32Dll
    .hidden _E32Dll
    .type _E32Dll, %function
_E32Dll:
    tst     pc, #0                  /* +0x00: 0xe31f0000 -> KImageEpt_Eka2   */
    ldr     ip, [pc, #4]            /* +0x04: load _E32Dll_Body from +0x10   */
    bx      ip                      /* +0x08: jump to _E32Dll_Body           */
_E32Dll_ModuleHandle:
    .word   0                       /* +0x0C: KCodeSegIdOffset patched by OS */
    .word   _E32Dll_Body            /* +0x10: entry body                     */

    .global "Symbian$$CPP$$Exception$$Descriptor"
    .hidden "Symbian$$CPP$$Exception$$Descriptor"
"Symbian$$CPP$$Exception$$Descriptor":
    .word   .ARM.exidx$$Base        /* +0x14 */
    .word   .ARM.exidx$$Limit       /* +0x18 */
    .word   Image$$ER_RO$$Base + 1  /* +0x1C */
    .word   Image$$ER_RO$$Limit     /* +0x20 */
    .word   0                       /* +0x24 */

    .section .text._E32Dll_Body, "ax", %progbits
    .align 2
    .hidden _E32Dll_Body
    .type _E32Dll_Body, %function
_E32Dll_Body:
    /* Reasons 2/3 are process attach/detach. The original DLL has empty
     * constructor/destructor lists and does not release EGL thread state. */
    mov     r0, #0                  /* KErrNone                              */
    bx      lr
    .size _E32Dll_Body, . - _E32Dll_Body

/* TRAP_IGNORE(CCoeEnv::ConstructL), with the native ARM EHABI personality.
 * AllocZL and the constructor remain outside this trap, as in ARM 0x9250.
 * Only this frame needs exception tables: all trapped calls go straight to
 * the OS. A leave is consumed by GetReason; an unrelated exception panics. */
    .section .text.sym_CCoeEnv_ConstructTrapped, "ax", %progbits
    .align 2
    .global sym_CCoeEnv_ConstructTrapped
    .hidden sym_CCoeEnv_ConstructTrapped
    .type sym_CCoeEnv_ConstructTrapped, %function
sym_CCoeEnv_ConstructTrapped:
    .fnstart
    .save {r4, r5, r6, lr}
    push    {r4, r5, r6, lr}
    mov     r4, r0
    bl      sym_User_MarkCleanupStack
.Ltrap_begin:
    mov     r5, r0
    mov     r0, r4
    bl      sym_CCoeEnv_ConstructL
    mov     r0, r5
    bl      sym_User_UnMarkCleanupStack
    b       .Ltrap_return
.Lcatch_leave:
    bl      __cxa_begin_catch
    bl      sym_XLeaveException_GetReason
.Lcatch_body:
    b       .Lcatch_end
.Lcatch_other:
    bl      __cxa_begin_catch
    bl      sym_User_Invariant
.Lcatch_end:
    bl      __cxa_end_catch
.Ltrap_return:
    pop     {r4, r5, r6, pc}
.Lcatch_cleanup:
    bl      __cxa_end_catch
    bl      __cxa_end_cleanup
    .size sym_CCoeEnv_ConstructTrapped, . - sym_CCoeEnv_ConstructTrapped
    .personalityindex 0
    .handlerdata
    /* EHABI16 catch descriptors: length|1, offset, PREL31 landing pad,
     * absolute typeinfo. The reference catch sets landing-pad bit 31. */
    .short .Lcatch_body - .Ltrap_begin + 1
    .short .Ltrap_begin - sym_CCoeEnv_ConstructTrapped
    .reloc ., R_ARM_PREL31, .Lcatch_leave
    .word 0x80000000
    .word _ZTI15XLeaveException
    .short .Lcatch_body - .Ltrap_begin + 1
    .short .Ltrap_begin - sym_CCoeEnv_ConstructTrapped
    .reloc ., R_ARM_PREL31, .Lcatch_other
    .word 0
    .word 0xffffffff
    /* End the active catch if GetReason unexpectedly propagates. */
    .short .Ltrap_return - .Lcatch_body
    .short .Lcatch_body - sym_CCoeEnv_ConstructTrapped
    .reloc ., R_ARM_PREL31, .Lcatch_cleanup
    .word 0
    .word 0
    .fnend

/* --- Dll::Tls / Dll::SetTls / Dll::FreeTls ------------------------------- */

    .section .text.sym_DllTls, "ax", %progbits
    .align 2
    .global sym_DllTls
    .hidden sym_DllTls
    .type sym_DllTls, %function
sym_DllTls:
    ldr     r0, .L_e32dll_tls
    ldr     r1, [r0, #12]
    b       sym_UserSvr_DllTls
.L_e32dll_tls:
    .word   _E32Dll
    .size sym_DllTls, . - sym_DllTls

    .section .text.sym_DllSetTls, "ax", %progbits
    .align 2
    .global sym_DllSetTls
    .hidden sym_DllSetTls
    .type sym_DllSetTls, %function
sym_DllSetTls:
    mov     r2, r0
    ldr     r0, .L_e32dll_settls
    ldr     r1, [r0, #12]
    b       sym_UserSvr_DllSetTls
.L_e32dll_settls:
    .word   _E32Dll
    .size sym_DllSetTls, . - sym_DllSetTls

    .section .text.sym_DllFreeTls, "ax", %progbits
    .align 2
    .global sym_DllFreeTls
    .hidden sym_DllFreeTls
    .type sym_DllFreeTls, %function
sym_DllFreeTls:
    ldr     r0, .L_e32dll_freetls
    b       sym_UserSvr_DllFreeTls
.L_e32dll_freetls:
    .word   _E32Dll
    .size sym_DllFreeTls, . - sym_DllFreeTls

/* --- ARM AAPCS (SoftVFP) setjmp / longjmp -------------------------------- */

    .section .text.sym_setjmp, "ax", %progbits
    .align 2
    .global sym_setjmp
    .hidden sym_setjmp
    .global setjmp
    .hidden setjmp
    .type sym_setjmp, %function
    .type setjmp, %function
sym_setjmp:
setjmp:
    stmia   r0, {r4, r5, r6, r7, r8, r9, r10, r11}
    str     sp, [r0, #32]
    str     lr, [r0, #36]
    mov     r0, #0
    bx      lr
    .size sym_setjmp, . - sym_setjmp

    .section .text.sym_longjmp, "ax", %progbits
    .align 2
    .global sym_longjmp
    .hidden sym_longjmp
    .global longjmp
    .hidden longjmp
    .type sym_longjmp, %function
    .type longjmp, %function
sym_longjmp:
longjmp:
    ldmia   r0, {r4, r5, r6, r7, r8, r9, r10, r11}
    ldr     sp, [r0, #32]
    ldr     lr, [r0, #36]
    movs    r0, r1
    moveq   r0, #1
    bx      lr
    .size sym_longjmp, . - sym_longjmp
