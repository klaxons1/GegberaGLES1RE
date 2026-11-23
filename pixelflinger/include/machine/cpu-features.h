#ifndef PIXELFLINGER_SHIM_MACHINE_CPU_FEATURES_H
#define PIXELFLINGER_SHIM_MACHINE_CPU_FEATURES_H

/* Symbian OS v9.x (EKA2 / S60 3rd Edition+) guarantees ARMv5TEJ+ (ARM926EJ-S,
 * ARM1136JF-S, Cortex-A8), providing CLZ, PLD, and 16x16 DSP multiplies
 * (SMULBB/SMULWB/SMLABB/QADD) across every EKA2 handset. */
#ifndef __ARM_ARCH__
#if defined(__ARM_ARCH)
#define __ARM_ARCH__ __ARM_ARCH
#else
#define __ARM_ARCH__ 5
#endif
#endif

#define __ARM_HAVE_PLD               1
#define __ARM_HAVE_HALFWORD_MULTIPLY 1

#endif /* PIXELFLINGER_SHIM_MACHINE_CPU_FEATURES_H */
