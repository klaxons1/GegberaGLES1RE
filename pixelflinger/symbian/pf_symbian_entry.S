/* pf_symbian_entry.S - Symbian OS v9.x (EKA2) ARM EABI DLL startup header,
 * C++ exception descriptor, and kernel TLS thunks for Pixelflinger GLES 1.1.
 */
    .syntax unified
    .arch armv5te
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
    cmp     r0, #2                  /* KModuleEntryReasonThreadDetach == 2   */
    bne     1f
    push    {r4, lr}
    bl      pf_symbian_on_thread_detach
    mov     r0, #0
    pop     {r4, pc}
1:
    mov     r0, #0                  /* KErrNone                              */
    bx      lr
    .size _E32Dll_Body, . - _E32Dll_Body

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
