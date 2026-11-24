"""Verify conservative guest instruction limits, including loops and REP."""

from pathlib import Path
import struct
import time

import unicorn
from unicorn import arm_const as ARM, x86_const as X86

from differential import Original, OriginalARM, OracleError
from differential_budget import NativeInstructionBudget
from differential_fault_guard import install


def check_budgets(library):
    root = Path(__file__).resolve().parents[1]
    comparisons = 0
    for architecture, oracle_type, original in (
            ("arm", OriginalARM, "libgles_cm.dll.epoc"),
            ("x86", Original, "libGLES_CM.dll")):
        machine = oracle_type(root / "docs" / original, time.monotonic() + 20,
                              coverage=False, audit_memory=True)
        budget = NativeInstructionBudget(machine, library, error_type=OracleError)

        def prepare(code, thumb=False):
            machine.reset()
            entry = machine.install_code(code)
            stack = machine.STACK + machine.STACK_SIZE - 4096
            if architecture == "arm":
                machine.uc.reg_write(ARM.UC_ARM_REG_CPSR, 0x30 if thumb else 0x10)
                machine.uc.reg_write(ARM.UC_ARM_REG_SP, stack)
                machine.uc.reg_write(ARM.UC_ARM_REG_LR, machine.RETURN)
            else:
                machine.write(stack, struct.pack("<I", machine.RETURN))
                machine.uc.reg_write(X86.UC_X86_REG_ESP, stack)
                machine.uc.reg_write(X86.UC_X86_REG_EFLAGS, 2)
            return entry | int(thumb)

        def exhaust(entry, limit):
            nonlocal comparisons
            try:
                budget.run(entry, machine.RETURN, limit)
            except OracleError as error:
                assert "instruction budget exhausted" in str(error), error
            else:
                raise AssertionError("guest execution exceeded its instruction limit")
            assert budget.used <= limit
            comparisons += 1

        try:
            if architecture == "arm":
                for code, thumb in ((struct.pack("<I", 0xEAFFFFFE), False),
                                    (struct.pack("<H", 0xE7FE), True)):
                    exhaust(prepare(code, thumb), 11)
                # mov r2,#42; str r2,[r0]; bx lr: three ARM instructions.
                code = struct.pack("<III", 0xE3A0202A, 0xE5802000, 0xE12FFF1E)
                entry = prepare(code)
                target = machine.alloc(4, b"\xA5" * 4)
                machine.uc.reg_write(ARM.UC_ARM_REG_R0, target)
                exhaust(entry, 2)
                assert machine.read(target, 4) == b"\xA5" * 4
                budget.run(entry, machine.RETURN, 3)
                assert budget.used == 3 and machine.read(target, 4) == struct.pack("<I", 42)
                comparisons += 1
                # A stop inside the block consumes an upper bound, never less
                # than the instructions that actually executed.
                machine.write(target, b"\xA5" * 4)
                hook = machine.uc.hook_add(unicorn.UC_HOOK_CODE,
                    lambda uc, address, size, data: uc.emu_stop(),
                    begin=entry + 4, end=entry + 4)
                machine.uc.ctl_remove_cache(entry, entry + len(code))
                try:
                    budget.run(entry, machine.RETURN, 3)
                finally:
                    machine.remove_hook(hook)
                    machine.uc.ctl_remove_cache(entry, entry + len(code))
                assert 1 <= budget.used <= 3 and machine.read(target, 4) == b"\xA5" * 4
                comparisons += 1
            else:
                exhaust(prepare(b"\xEB\xFE"), 11)
                # mov [edi],42; ret: all seven bytes must fit before the store.
                entry = prepare(bytes.fromhex("c7 07 2a 00 00 00 c3"))
                target = machine.alloc(4, b"\xA5" * 4)
                machine.uc.reg_write(X86.UC_X86_REG_EDI, target)
                exhaust(entry, 6)
                assert machine.read(target, 4) == b"\xA5" * 4
                budget.run(entry, machine.RETURN, 7)
                assert budget.used == 7 and machine.read(target, 4) == struct.pack("<I", 42)
                comparisons += 1
                # Measure actual Unicorn behavior: every REP iteration must
                # reenter the charged block even though ECX is much larger.
                for opcode, width, fill in (("f3 a4 c3", 1, False),
                                            ("f3 aa c3", 1, True),
                                            ("f3 a5 c3", 4, False)):
                    entry = prepare(bytes.fromhex(opcode))
                    size = 4096 * width
                    source = machine.alloc(size, bytes((i * 73 + 19) & 255 for i in range(size)))
                    destination = machine.alloc(size, b"\xA5" * size)
                    machine.watch(destination, size, "bounded REP destination")
                    machine.uc.reg_write(X86.UC_X86_REG_ESI, source)
                    machine.uc.reg_write(X86.UC_X86_REG_EDI, destination)
                    machine.uc.reg_write(X86.UC_X86_REG_ECX, 4096)
                    machine.uc.reg_write(X86.UC_X86_REG_EAX, 0x5A)
                    exhaust(entry, 23)
                    remaining = machine.uc.reg_read(X86.UC_X86_REG_ECX)
                    iterations = 4096 - remaining
                    assert 0 < iterations <= 23
                    changed = iterations * width
                    expected = ((b"\x5A" * changed) if fill else machine.read(source, changed))
                    expected += b"\xA5" * (size - changed)
                    assert machine.readback(destination, size) == expected
                    assert machine.watched[0]["writes"] == set(range(changed))
                    machine.finish_memory_audit("bounded REP")
                    assert not machine.memory_gaps()
            for limit in (0, -1, 1 << 64):
                try:
                    budget.run(entry, machine.RETURN, limit)
                except ValueError:
                    pass
                else:
                    raise AssertionError("invalid instruction limit accepted")
            # A budget installed after a cached translation must still charge
            # every loop iteration; the warming run also has an explicit cap.
            budget.close()
            entry = prepare(struct.pack("<I", 0xEAFFFFFE) if architecture == "arm" else b"\xEB\xFE")
            machine.uc.emu_start(entry, machine.RETURN, count=3)
            budget = NativeInstructionBudget(machine, library, error_type=OracleError)
            exhaust(entry, 11)
        finally:
            budget.close()
    print(f"Instruction budgets: {comparisons} loop, REP, Thumb and pre-execution bounds checks pass")


if __name__ == "__main__":
    guard = install()
    check_budgets(guard)
