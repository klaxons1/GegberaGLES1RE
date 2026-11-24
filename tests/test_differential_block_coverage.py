"""Compare native and Python coverage on bounded fabricated ARM/x86 code."""

import ctypes as C
import struct
import time

import capstone
import unicorn
from unicorn import arm_const as ARM

from differential import OracleError, Original, OriginalARM
from differential_block_coverage import NativeBlockCoverage
from differential_fault_guard import install


def fixture(architecture):
    cls = OriginalARM if architecture == "arm" else Original
    oracle = cls.__new__(cls)
    oracle.base, oracle.code_end = 0x1000, 0x2000
    oracle.uc = unicorn.Uc(unicorn.UC_ARCH_ARM if architecture == "arm" else unicorn.UC_ARCH_X86,
                           unicorn.UC_MODE_ARM if architecture == "arm" else unicorn.UC_MODE_32)
    oracle.uc.mem_map(oracle.base, 4096, unicorn.UC_PROT_READ | unicorn.UC_PROT_EXEC)
    oracle.uc.mem_map(oracle.STACK, oracle.STACK_SIZE)
    oracle.uc.mem_map(oracle.RETURN, 4096, unicorn.UC_PROT_READ | unicorn.UC_PROT_EXEC)
    oracle.decoder = capstone.Cs(capstone.CS_ARCH_ARM if architecture == "arm" else capstone.CS_ARCH_X86,
                                 capstone.CS_MODE_ARM if architecture == "arm" else capstone.CS_MODE_32)
    oracle.decoder.detail = True
    if architecture == "arm":
        oracle.thumb_decoder = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB)
        oracle.thumb_decoder.detail = True
        oracle.block_cpsr = C.c_uint32()
        oracle.block_cpsr_pointer = C.byref(oracle.block_cpsr)
        oracle.block_read_register = unicorn.Uc.hook_add.__globals__["uclib"].uc_reg_read
        # mov r2,#0; add r2,#1; subs r0,#1; bne loop; bx r1.
        oracle.uc.mem_write(0x1000, struct.pack("<5I", 0xE3A02000, 0xE2822001, 0xE2500001,
                                               0x1AFFFFFC, 0xE12FFF11))
        oracle.uc.mem_write(0x1040, struct.pack("<2I", 0xE1A00002, 0xE12FFF1E))
        oracle.uc.mem_write(oracle.RETURN + 0x100, struct.pack("<2I", 0xE1A00002, 0xE12FFF1E))
        # Thumb adds r0,r2,#1; bx lr.
        oracle.uc.mem_write(0x1080, struct.pack("<2H", 0x1C50, 0x4770))
        # A separate conditional path is first translated with tracing suspended.
        oracle.uc.mem_write(0x10C0, struct.pack("<6I", 0xE3520002, 0x0A000001,
                                               0xE1A00002, 0xE12FFF1E,
                                               0xE2820002, 0xE12FFF1E))
    else:
        # mov ecx,[esp+4]; xor eax,eax; inc eax; dec ecx; jnz loop;
        # mov edx,[esp+8]; jmp edx. Targets return n or n+1.
        oracle.uc.mem_write(0x1000, bytes.fromhex("8b4c240431c0404975fc8b542408ffe2"))
        oracle.uc.mem_write(0x1040, b"\xC3")
        oracle.uc.mem_write(oracle.RETURN + 0x100, b"\xC3")
        oracle.uc.mem_write(0x1080, bytes.fromhex("83c001c3"))
        oracle.uc.mem_write(0x10C0, bytes.fromhex("83f8027401c383c002c3"))
    oracle.blocks, oracle.edges, oracle.indirect_edges = set(), set(), set()
    oracle.branch_cache, oracle.direct_calls = {}, {}
    oracle.previous_branch, oracle.previous_indirect = None, False
    oracle.deadline = time.monotonic() + 20
    oracle.block_hook = oracle.add_block_hook()
    return oracle


def enable(oracle, library, **limits):
    native = NativeBlockCoverage(oracle, library, error_type=OracleError, **limits)
    oracle.remove_hook(oracle.block_hook)
    oracle.native_blocks = native
    oracle.block_hook = native.install_hook()
    return native


def parity(library, architecture):
    python = fixture(architecture)
    native_oracle = fixture(architecture)
    native = enable(native_oracle, library)
    try:
        target = 0x1081 if architecture == "arm" else 0x1080
        for count in (1, 2, 7, 1, 7):
            for destination in (0x1040, target, python.RETURN + 0x100):
                args = [count, destination]
                expected = count + (destination == target)
                assert python.call(0x1000, args) == expected
                assert native_oracle.call(0x1000, args) == expected
                native.materialize()
                assert native.edge_count() == len(python.edges) + len(python.indirect_edges)
                assert (native_oracle.blocks, native_oracle.edges, native_oracle.indirect_edges) == (
                    python.blocks, python.edges, python.indirect_edges)
                assert all(python.base <= address < python.code_end for address in native_oracle.blocks)
        # Suspended tracing neither adds coverage nor links the previous call
        # to the next call. Resumption must use a live native callback.
        before = native.snapshot()
        for oracle in (python, native_oracle):
            assert oracle.call(0x1000, [5, target], trace_blocks=False) == 6
            assert oracle.call(0x1000, [1, 0x1040]) == 1
        native.materialize()
        assert native.snapshot() == before
        assert native_oracle.branch_cache == python.branch_cache
        assert (native_oracle.blocks, native_oracle.edges, native_oracle.indirect_edges) == (
            python.blocks, python.edges, python.indirect_edges)
        # Warming an unobserved path while suspended must not leave a cached
        # translation that bypasses the reinstalled coverage callback.
        for oracle in (python, native_oracle):
            assert oracle.call(0x1000, [5, 0x10C0], trace_blocks=False) == 5
        assert native.snapshot() == before
        for oracle in (python, native_oracle):
            assert oracle.call(0x1000, [5, 0x10C0]) == 5
        native.materialize()
        branch = 0x10C4 if architecture == "arm" else 0x10C3
        fallthrough = 0x10C8 if architecture == "arm" else 0x10C5
        assert (branch, fallthrough) in native_oracle.edges
        assert (native_oracle.blocks, native_oracle.edges, native_oracle.indirect_edges) == (
            python.blocks, python.edges, python.indirect_edges)
    finally:
        native.close()
        python.remove_hook(python.block_hook)
    assert native_oracle.block_hook is None


def deliver(oracle, address, size):
    callback = oracle.native_blocks.callback
    callback.argtypes = [C.c_void_p, C.c_uint64, C.c_uint32, C.c_void_p]
    callback.restype = None
    callback(oracle.uc._uch, address, size, oracle.native_blocks.state)


def modes_and_limits(library):
    oracle = fixture("arm")
    native = enable(oracle, library)
    try:
        for size, thumb in ((4, False), (4, True), (8, False), (4, False)):
            oracle.uc.reg_write(ARM.UC_ARM_REG_CPSR, 0x10 | (0x20 if thumb else 0))
            native.reset_trace()
            deliver(oracle, 0x1000, size)
            native.check()
        assert set(oracle.branch_cache) == {(0x1000, 4, False), (0x1000, 4, True), (0x1000, 8, False)}
        assert native.snapshot()[0] == {0x1000}
    finally:
        native.close()

    for resource in ("block", "edge", "classifier"):
        oracle = fixture("x86")
        native = enable(oracle, library, block_capacity=4 if resource == "block" else 64,
                        edge_capacity=4)
        try:
            if resource == "classifier":
                def invalid_read(address, size):
                    raise ValueError("classification probe")
                oracle.read = invalid_read
                deliver(oracle, 0x1000, 4)
            else:
                # jmp eax supplies one indirect branch with four destinations.
                oracle.uc.mem_write(0x1100, b"\xFF\xE0")
                for index in range(4):
                    address = 0x1200 + 16 * index
                    oracle.uc.mem_write(address, b"\xC3")
                    native.reset_trace()
                    if resource == "edge":
                        deliver(oracle, 0x1100, 2)
                    deliver(oracle, address, 1)
            try:
                native.check()
            except OracleError as error:
                assert resource in str(error), str(error)
            else:
                raise AssertionError(resource + " failure was silently ignored")
        finally:
            native.close()


def run(library):
    for architecture in ("arm", "x86"):
        parity(library, architecture)
    modes_and_limits(library)
    print("Native block coverage: ARM/Thumb/x86 edge parity, reset, suspension and explicit errors pass")


if __name__ == "__main__":
    run(install())
