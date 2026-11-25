/* start.S - bare-metal entry for the emulator harness (ARM state, AAPCS). */
    .syntax unified
    .arm
    .section .text.start, "ax", %progbits
    .global _start
    .type _start, %function
_start:
    ldr     sp, =__stack_top
    /* zero .bss */
    ldr     r0, =__bss_start
    ldr     r1, =__bss_end
    mov     r2, #0
1:  cmp     r0, r1
    strlo   r2, [r0], #4
    blo     1b
    bl      main
    svc     #2
2:  b       2b
    .size _start, . - _start

/* ARM AAPCS (SoftVFP) setjmp / longjmp, identical to symbian_entry.S. */
    .section .text.setjmp, "ax", %progbits
    .align 2
    .global setjmp
    .type setjmp, %function
setjmp:
    stmia   r0, {r4, r5, r6, r7, r8, r9, r10, r11}
    str     sp, [r0, #32]
    str     lr, [r0, #36]
    mov     r0, #0
    bx      lr
    .size setjmp, . - setjmp

    .section .text.longjmp, "ax", %progbits
    .align 2
    .global longjmp
    .type longjmp, %function
longjmp:
    ldmia   r0, {r4, r5, r6, r7, r8, r9, r10, r11}
    ldr     sp, [r0, #32]
    ldr     lr, [r0, #36]
    movs    r0, r1
    moveq   r0, #1
    bx      lr
    .size longjmp, . - longjmp
