"""Check guest primitive boundaries, overlap, ABI, and memory-audit hooks."""

from pathlib import Path
import struct
import time

import unicorn
from unicorn import arm_const as ARM
from unicorn import x86_const as X86

from differential import OracleError, Original, OriginalARM
from differential_fault_guard import install
from differential_primitives import ARM_CLEAR_IMPORT, ARM_COPY_IMPORTS, ARM_FILL_IMPORT


def check_primitive(machine, preserved, entry, destination, length, source, value, size):
    machine.reset()
    before = bytes((i * 73 + 19) & 255 for i in range(size))
    address = machine.alloc(size, before)
    machine.watch(address, size, "primitive buffer and guards")
    expected = bytearray(before)
    if source is None:
        byte = 0 if machine.arch == "arm" and entry == ARM_CLEAR_IMPORT else value & 255
        expected[destination:destination + length] = bytes([byte]) * length
        arguments = ([address + destination, length, value] if machine.arch == "arm" else
                     [address + destination, value, length])
    else:
        expected[destination:destination + length] = before[source:source + length]
        arguments = [address + destination, address + source, length]
    assert machine.call(entry, arguments, initial_registers=preserved) == address + destination
    assert machine.readback(address, size) == expected
    region = machine.watched[0]
    assert region["writes"] == set(range(destination, destination + length))
    assert region["reads"] == (set(range(source, source + length)) if source is not None else set())
    assert all(machine.uc.reg_read(register) == value for register, value in preserved.items())
    if machine.arch == "x86":
        assert not machine.uc.reg_read(X86.UC_X86_REG_EFLAGS) & 0x400
    machine.finish_memory_audit("guest primitive")
    assert machine.memory_gaps() == 0


def check_arm():
    root = Path(__file__).resolve().parents[1]
    machine = OriginalARM(root / "docs/libgles_cm.dll.epoc", time.monotonic() + 25,
                          coverage=False, audit_memory=True)
    entries = []
    machine.uc.hook_add(unicorn.UC_HOOK_CODE,
                        lambda uc, address, size, data: entries.append(address),
                        begin=machine.RETURN + 0x100, end=machine.RETURN + 0x100)
    preserved = {register: 0x12340000 + register
                 for register in range(ARM.UC_ARM_REG_R4, ARM.UC_ARM_REG_R11 + 1)}
    comparisons = 0

    def exercise(entry, destination, length, source=None, value=0x123456AB):
        nonlocal comparisons
        check_primitive(machine, preserved, entry, destination, length, source, value, 256)
        comparisons += 1

    lengths = (*range(18), 31, 32, 33, 63)
    for source_alignment in range(4):
        for destination_alignment in range(4):
            for length in lengths:
                for source, destination in ((16 + source_alignment, 112 + destination_alignment),
                                            (112 + source_alignment, 16 + destination_alignment)):
                    exercise(ARM_COPY_IMPORTS[0], destination, length, source)
        for displacement in range(-9, 10):
            for length in lengths:
                source = 64 + source_alignment
                exercise(ARM_COPY_IMPORTS[0], source + displacement, length, source)
    for entry in ARM_COPY_IMPORTS[1:]:
        exercise(entry, 17, 63, 16)
    for entry in (ARM_FILL_IMPORT, ARM_CLEAR_IMPORT):
        for alignment in range(4):
            for length in lengths:
                for value in (0, 0xAB, 0x123456AB, 0xFFFFFFFF):
                    exercise(entry, 16 + alignment, length, value=value)
    assert entries, "copy import did not execute the guest implementation"

    # Empty ranges at either writable region's end are valid; empty copies
    # must not dereference their source. Both ends and integer wrap are checked.
    machine.reset()
    for start, size in ((machine.HEAP, machine.HEAP_SIZE), (machine.STACK, machine.STACK_SIZE)):
        for destination, length in ((start, 0), (start + size, 0), (start + size - 3, 3)):
            assert machine.call(ARM_FILL_IMPORT, [destination, length, 0x1AB]) == destination
        assert machine.call(ARM_COPY_IMPORTS[0], [start + size, 0xFFFFFFFF, 0]) == start + size
        for destination, length in ((start - 1, 0), (start + size + 1, 0),
                                    (start + size - 1, 2), (start, machine.HEAP_SIZE + 1)):
            for entry, arguments in ((ARM_FILL_IMPORT, [destination, length, 7]),
                                     (ARM_COPY_IMPORTS[0], [destination, start, length])):
                try:
                    machine.call(entry, arguments)
                except OracleError:
                    pass
                else:
                    raise AssertionError(("accepted invalid writable range", arguments))
    for destination, length in ((0xFFFFFFFE, 4), (machine.HEAP, 0xFFFFFFFF)):
        try:
            machine.call(ARM_FILL_IMPORT, [destination, length, 7])
        except OracleError:
            pass
        else:
            raise AssertionError("accepted wrapped destination range")

    # A Thumb caller must return through the ARM import and guest implementation.
    # push {lr}; ldr r3,[pc,#4]; blx r3; pop {pc}; .word fill_import.
    thumb = machine.install_code(struct.pack("<4HI", 0xB500, 0x4B01, 0x4798, 0xBD00,
                                              ARM_FILL_IMPORT))
    address = machine.alloc(8)
    assert machine.call(thumb | 1, [address, 8, 0xAB]) == address
    assert machine.read(address, 8) == b"\xAB" * 8
    try:
        machine.call(ARM_FILL_IMPORT, [address, 8, 0], instruction_budget=4)
    except OracleError:
        pass
    else:
        raise AssertionError("guest primitive bypassed the instruction budget")
    print(f"ARM primitives: {comparisons} whole-buffer/audit comparisons; bounds, ABI and budgets pass")


def check_x86():
    root = Path(__file__).resolve().parents[1]
    machine = Original(root / "docs/libGLES_CM.dll", time.monotonic() + 25,
                       coverage=False, audit_memory=True)
    entries = []
    machine.uc.hook_add(unicorn.UC_HOOK_CODE,
                        lambda uc, address, size, data: entries.append(address),
                        begin=machine.RETURN + 0x800, end=machine.RETURN + 0x800)
    assert machine.MEMMOVE == machine.RETURN + 0x800
    for slot, target in ((0x4902F0, machine.MEMMOVE), (0x490354, machine.MEMSET),
                          (0x490358, machine.MEMMOVE)):
        assert struct.unpack("<I", machine.read(slot, 4))[0] == target
    preserved = {register: 0x12340000 + register for register in
                 (X86.UC_X86_REG_EBX, X86.UC_X86_REG_ESI,
                  X86.UC_X86_REG_EDI, X86.UC_X86_REG_EBP)}
    comparisons = 0

    def exercise(destination, length, source=None, value=0x123456AB):
        nonlocal comparisons
        entry = machine.MEMSET if source is None else machine.MEMMOVE
        check_primitive(machine, preserved, entry, destination, length, source, value, 384)
        comparisons += 1

    lengths = (*range(33), 63, 64, 65, 127, 128, 129, 255)
    for length in lengths:
        for source, destination in ((32, 32), (32, 33), (33, 32), (32, 35),
                                    (35, 32), (32, 64), (64, 32)):
            exercise(destination, length, source)
        for value in (0, 1, 0x1234, 0xFFFFFFFF):
            exercise(17, length, value=value)
    assert entries, "copy import did not execute the guest implementation"

    # Guest scratch must not use bytes below ESP or reread overwritten arguments.
    stack = machine.STACK + machine.STACK_SIZE - 4096
    for destination, length in ((stack - 16, 16), (stack + 4, 12)):
        machine.reset()
        source = machine.alloc(32, bytes(range(32)))
        assert machine.call(machine.MEMMOVE, [destination, source, length],
                            initial_registers=preserved) == destination
        assert machine.read(destination, length) == bytes(range(length))
        assert all(machine.uc.reg_read(register) == value for register, value in preserved.items())
        assert machine.call(machine.MEMSET, [destination, 0x1234, length],
                            initial_registers=preserved) == destination
        assert machine.read(destination, length) == b"4" * length
        assert all(machine.uc.reg_read(register) == value for register, value in preserved.items())
    for source in (stack - 16, stack + 4):
        machine.reset()
        destination = machine.alloc(32)
        machine.write(stack - 16, b"0123456789abcdef")
        expected = (b"0123456789abcdef" if source == stack - 16 else
                    struct.pack("<III", destination, source, 12))
        assert machine.call(machine.MEMMOVE, [destination, source, len(expected)]) == destination
        assert machine.read(destination, len(expected)) == expected

    regions = [(machine.HEAP, machine.HEAP_SIZE), (machine.STACK, machine.STACK_SIZE)]
    regions += [(start, len(data)) for start, data in machine.initial]
    for start, size in regions:
        machine.reset()
        for destination, length in ((start, 0), (start + size, 0), (start + size - 3, 3)):
            assert machine.call(machine.MEMSET, [destination, 0x1AB, length]) == destination
        assert machine.call(machine.MEMMOVE, [start + size, 0xFFFFFFFF, 0]) == start + size
    for destination, source, length in (
            (machine.HEAP - 1, machine.HEAP, 1),
            (machine.HEAP + machine.HEAP_SIZE, machine.HEAP, 1),
            (machine.HEAP, machine.HEAP, machine.HEAP_SIZE + 1),
            (0xFFFFFFF0, machine.HEAP, 32),
            (machine.RETURN, machine.HEAP, 1),
            (machine.HEAP, machine.RETURN + 0x10000, 1),
            (machine.HEAP, 0xFFFFFFF0, 32)):
        machine.reset()
        try:
            machine.call(machine.MEMMOVE, [destination, source, length])
        except OracleError:
            pass
        else:
            raise AssertionError(("accepted invalid memory range", destination, source, length))
    try:
        machine.call(machine.MEMSET, [machine.HEAP, 0, 8], instruction_budget=4)
    except OracleError:
        pass
    else:
        raise AssertionError("guest primitive bypassed the instruction budget")
    print(f"x86 primitives: {comparisons} whole-buffer/audit comparisons; stack, bounds, ABI and budgets pass")


if __name__ == "__main__":
    guard = install()
    check_arm()
    check_x86()
