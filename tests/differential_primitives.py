"""Bounded ARM guest implementations of the oracle's byte primitives.

These ARMv5 instructions execute under the caller's instruction budget. Guest
loads/stores retain normal watched-memory auditing without a Python callback
for each imported copy or fill. No assembler is needed when running tests.
"""

import struct


# r0=destination, r1=source, r2=length for copy; r0=destination,
# r1=length, r2=value for EABI fill. r12 saves the return value. Only AAPCS
# scratch registers and flags change; no guest stack space is used.
#
# Each entry first checks length <= HEAP_SIZE and containment in one complete
# writable region. Unsigned offset/remaining comparisons avoid address wrap.
# The fixed bounds match OriginalARM; installation rejects a different map.
_ARM_WORDS = (
    # copy: validate r2 against heap [0x10000000,0x10400000] or
    # stack [0x20000000,0x20010000], including empty ranges at either end.
    0xE3520501, 0x8A000078, 0xE3500201, 0x3A000005,
    0xE2403201, 0xE3530501, 0x8A000002, 0xE2633501,
    0xE1520003, 0x9A000007, 0xE3500202, 0x3A00006E,
    0xE2403202, 0xE3530801, 0x8A00006B, 0xE2633801,
    0xE1520003, 0x8A000068,
    # mov r12,r0; cmp r2,#0; bxeq lr; cmp r0,r1; blo forward.
    0xE1A0C000, 0xE3520000, 0x012FFF1E, 0xE1500001, 0x3A000018,
    # Backward memmove: end pointers, then byte alignment when both pointers
    # have the same alignment. Differently aligned pointers use byte copies.
    0xE0800002, 0xE0811002, 0xE0203001, 0xE3130003, 0x1A00000C,
    0xE3100003, 0x0A000004,
    0xE5713001, 0xE5603001, 0xE2522001, 0x0A000023, 0xEAFFFFF8,
    # cmp r2,#4; blo bytes; ldr/str r3,[r1/r0,#-4]!; sub r2,#4; repeat.
    0xE3520004, 0x3A000003, 0xE5313004, 0xE5203004, 0xE2422004, 0xEAFFFFF9,
    0xE3520000, 0x0A00001A,
    0xE5713001, 0xE5603001, 0xE2522001, 0x1AFFFFFB, 0xEA000015,
    # Forward memmove uses the same alignment, word, and byte-tail paths.
    0xE0203001, 0xE3130003, 0x1A00000C,
    0xE3100003, 0x0A000004,
    0xE4D13001, 0xE4C03001, 0xE2522001, 0x0A00000C, 0xEAFFFFF8,
    0xE3520004, 0x3A000003, 0xE4913004, 0xE4803004, 0xE2422004, 0xEAFFFFF9,
    0xE3520000, 0x0A000003,
    0xE4D13001, 0xE4C03001, 0xE2522001, 0x1AFFFFFB,
    # done: mov r0,r12; bx lr.
    0xE1A0000C, 0xE12FFF1E,
    # fill (+0x120): the same writable-range validator, with length in r1.
    0xE3510501, 0x8A000030, 0xE3500201, 0x3A000005,
    0xE2403201, 0xE3530501, 0x8A000002, 0xE2633501,
    0xE1510003, 0x9A000007, 0xE3500202, 0x3A000026,
    0xE2403202, 0xE3530801, 0x8A000023, 0xE2633801,
    0xE1510003, 0x8A000020,
    # mov r12,r0; cmp r1,#0; bxeq lr; replicate value's low byte in r2.
    0xE1A0C000, 0xE3510000, 0x012FFF1E,
    0xE20220FF, 0xE1822402, 0xE1822802,
    # Align destination with strb, then emit four word stores per iteration.
    0xE3100003, 0x0A000003, 0xE4C02001, 0xE2511001, 0x0AFFFFE0, 0xEAFFFFF9,
    0xE3510010, 0x3A000005,
    0xE4802004, 0xE4802004, 0xE4802004, 0xE4802004, 0xE2411010, 0xEAFFFFF7,
    # Remaining complete words, followed by bytes, then return destination.
    0xE3510004, 0x3A000002, 0xE4802004, 0xE2411004, 0xEAFFFFFA,
    0xE3510000, 0x0AFFFFD0,
    0xE4C02001, 0xE2511001, 0x1AFFFFFC, 0xEAFFFFCC,
    # clear (+0x1e4): mov r2,#0; b fill. invalid: udf #0.
    0xE3A02000, 0xEAFFFFCC, 0xE7F000F0,
)

ARM_COPY_IMPORTS = (0x34CBC, 0x34CC4, 0x34E44)
ARM_FILL_IMPORT = 0x34E4C
ARM_CLEAR_IMPORT = 0x34CAC


def install_arm_primitives(oracle):
    """Replace five unresolved E32 import veneers with bounded guest code."""
    if (oracle.HEAP, oracle.HEAP_SIZE, oracle.STACK, oracle.STACK_SIZE) != (
            0x10000000, 0x400000, 0x20000000, 0x10000) or oracle.initial:
        raise ValueError("ARM primitive code requires the oracle's heap/stack-only memory map")
    code = struct.pack("<" + "I" * len(_ARM_WORDS), *_ARM_WORDS)
    entry = oracle.RETURN + 0x100
    oracle.uc.mem_write(entry, code)
    targets = {address: entry for address in ARM_COPY_IMPORTS}
    targets.update({ARM_FILL_IMPORT: entry + 0x120, ARM_CLEAR_IMPORT: entry + 0x1E4})
    for address, target in targets.items():
        # ldr pc,[pc,#-4]; .word target. This retains the E32 veneer shape.
        oracle.uc.mem_write(address, struct.pack("<II", 0xE51FF004, target))
        oracle.uc.ctl_remove_cache(address, address + 8)
    oracle.uc.ctl_remove_cache(entry, entry + len(code))
