#!/usr/bin/env python3
"""Bounded, deterministic comparisons with the original ARM and x86 drivers.

Requires unicorn, capstone and pefile. The DLL is never loaded by the host OS.
"""
import argparse
import ctypes
import importlib
import math
from pathlib import Path
import random
import struct
import sys
import time

import capstone
import pefile
import unicorn
from unicorn import x86_const as x86
from unicorn import arm_const as arm


class OracleError(RuntimeError):
    pass


class NativeFunction:
    def __init__(self, function, record):
        self.function = function
        self.record = record

    @property
    def argtypes(self):
        return self.function.argtypes

    @argtypes.setter
    def argtypes(self, value):
        self.function.argtypes = value

    @property
    def restype(self):
        return self.function.restype

    @restype.setter
    def restype(self, value):
        self.function.restype = value

    def __call__(self, *args):
        self.record(f"{self.function.__name__}{args!r}".encode()[:255])
        return self.function(*args)


class NativeLibrary:
    def __init__(self, library):
        self.library = library
        self.functions = {}
        self.record = library.differential_record_call
        self.record.argtypes = [ctypes.c_char_p]
        self.record.restype = None

    def __getattr__(self, name):
        if name not in self.functions:
            self.functions[name] = NativeFunction(getattr(self.library, name), self.record)
        return self.functions[name]


class Original:
    arch = "x86"
    HEAP = 0x10000000
    HEAP_SIZE = 4 * 1024 * 1024
    STACK = 0x20000000
    STACK_SIZE = 65536
    RETURN = 0x30000000
    MEMMOVE = RETURN + 16
    MEMSET = RETURN + 32
    GENERATED_CODE = 0x31000000
    GENERATED_CODE_SIZE = 65536

    def __init__(self, path, deadline, coverage=True, audit_memory=False):
        pe = pefile.PE(str(path))
        if pe.FILE_HEADER.Machine != 0x14C or pe.OPTIONAL_HEADER.ImageBase != 0x400000:
            raise OracleError("expected the original x86 PE32 image at 0x400000")
        self.base = pe.OPTIONAL_HEADER.ImageBase
        self.uc = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)
        size = (pe.OPTIONAL_HEADER.SizeOfImage + 4095) & ~4095
        self.uc.mem_map(self.base, size, unicorn.UC_PROT_READ)
        self.uc.mem_write(self.base, pe.get_memory_mapped_image())
        self.initial = []
        for section in pe.sections:
            address = self.base + section.VirtualAddress
            length = (max(section.Misc_VirtualSize, section.SizeOfRawData) + 4095) & ~4095
            flags = section.Characteristics
            permissions = ((unicorn.UC_PROT_READ if flags & 0x40000000 else 0)
                           | (unicorn.UC_PROT_WRITE if flags & 0x80000000 else 0)
                           | (unicorn.UC_PROT_EXEC if flags & 0x20000000 else 0))
            if length:
                self.uc.mem_protect(address, length, permissions)
                if permissions & unicorn.UC_PROT_WRITE:
                    self.initial.append((address, bytes(self.uc.mem_read(address, length))))
        # Imports remain unresolved except the bounded byte-copy/fill primitives.
        self.imports = []
        for descriptor in getattr(pe, "DIRECTORY_ENTRY_IMPORT", []):
            for entry in descriptor.imports:
                self.imports.append(entry.address)
        self.uc.mem_map(self.HEAP, self.HEAP_SIZE, unicorn.UC_PROT_READ | unicorn.UC_PROT_WRITE)
        self.uc.mem_map(self.STACK, self.STACK_SIZE, unicorn.UC_PROT_READ | unicorn.UC_PROT_WRITE)
        self.uc.mem_map(self.RETURN, 4096, unicorn.UC_PROT_READ | unicorn.UC_PROT_EXEC)
        self.uc.mem_map(self.GENERATED_CODE, self.GENERATED_CODE_SIZE,
                        unicorn.UC_PROT_READ | unicorn.UC_PROT_EXEC)
        # Original IAT: euser ordinal 4 supplies Mem::Copy's memmove; memset is
        # reached through Mem::Fill. These shims preserve cdecl return values.
        self.uc.mem_write(self.MEMMOVE, b"\xc3")
        self.uc.mem_write(self.MEMSET, b"\xc3")
        self.uc.hook_add(unicorn.UC_HOOK_CODE, self._memmove,
                         begin=self.MEMMOVE, end=self.MEMMOVE)
        self.uc.hook_add(unicorn.UC_HOOK_CODE, self._memset,
                         begin=self.MEMSET, end=self.MEMSET)
        self.add_hook(0x46E434, lambda machine: machine._compare_primitive())
        self.decoder = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
        self.decoder.detail = True
        self.blocks = set()
        self.edges = set()
        self.indirect_edges = set()
        self.direct_calls = {}
        self.branch_cache = {}
        self.previous_branch = None
        self.previous_indirect = False
        self.watched = []
        self.memory_audit = {}
        self.audit_memory = audit_memory
        self.deadline = deadline
        self.block_hook = self.uc.hook_add(unicorn.UC_HOOK_BLOCK, self._block) if coverage else None
        self.uc.hook_add(unicorn.UC_HOOK_INTR, self._interrupt)
        primitives = {0x4902F0: self.MEMMOVE, 0x490354: self.MEMSET}
        for address in self.imports:
            self.uc.mem_write(address, struct.pack("<I", primitives.get(address, 0xDEAD0000)))
        self.initial = [(address, self.read(address, len(data))) for address, data in self.initial]
        self.reset()

    @staticmethod
    def conditional(instruction):
        return ((instruction.mnemonic.startswith("j") and instruction.mnemonic != "jmp")
                or instruction.mnemonic.startswith("loop"))

    def _compare_primitive(self):
        left, left_length, right, right_length = self.arguments(4)
        if max(left_length, right_length) > self.HEAP_SIZE:
            raise OracleError("original memory comparison exceeds fixture limit")
        length = min(left_length, right_length)
        first, second = self.read(left, length), self.read(right, length)
        result = next((a - b for a, b in zip(first, second) if a != b), left_length - right_length)
        if self.audit_memory:
            self._memory(self.uc, unicorn.UC_MEM_READ, left, length, 0, None)
            self._memory(self.uc, unicorn.UC_MEM_READ, right, length, 0, None)
        self.return_from_call(result)

    def _block(self, uc, address, size, _):
        self.blocks.add(address)
        if self.previous_branch is not None:
            outcomes = self.indirect_edges if self.previous_indirect else self.edges
            outcomes.add((self.previous_branch, address))
        key = (address, size)
        if key not in self.branch_cache:
            instructions = list(self.decoder.disasm(bytes(uc.mem_read(address, size)), address))
            last = instructions[-1] if instructions else None
            indirect = bool(last and last.mnemonic == "jmp"
                            and last.operands[0].type != capstone.x86.X86_OP_IMM)
            self.branch_cache[key] = (last.address if last and
                                      (self.conditional(last) or indirect) else None, indirect)
        self.previous_branch, self.previous_indirect = self.branch_cache[key]

    def _memory(self, uc, access, address, size, value, _):
        for region in self.watched:
            self._memory_region(region, access, address, size)

    @staticmethod
    def _memory_region(region, access, address, size):
        start = max(address, region["address"])
        end = min(address + size, region["address"] + region["size"])
        if start < end:
            offsets = range(start - region["address"], end - region["address"])
            region["writes" if access == unicorn.UC_MEM_WRITE else "reads"].update(offsets)
            if access == unicorn.UC_MEM_WRITE:
                region["compared"].difference_update(offsets)

    def watch(self, address, size, label):
        """Audit a fixture allocation; pointers still need explicit normalization."""
        if self.audit_memory:
            if size <= 0 or address < self.HEAP or address + size > self.cursor:
                raise OracleError("memory audit range is outside allocated fixture memory")
            region = dict(address=address, size=size, label=label,
                          reads=set(), writes=set(), compared=set())
            # Restrict callbacks to observed buffers, not compiler arena traffic.
            # Include a prefix for accesses that straddle the start boundary.
            region["hook"] = self.uc.hook_add(
                unicorn.UC_HOOK_MEM_READ | unicorn.UC_HOOK_MEM_WRITE,
                lambda uc, access, start, count, value, data:
                    self._memory_region(region, access, start, count),
                begin=max(self.HEAP, address - 15), end=address + size - 1)
            self.watched.append(region)

    def unwatch(self, address):
        """End observation after comparison, before an allocation is released."""
        for region in self.watched:
            if region["address"] == address:
                if region["writes"]:
                    raise OracleError("watched allocation must be compared before ending observation")
                self.remove_hook(region["hook"])
                self.watched.remove(region)
                return

    def readback(self, address, size):
        """Read bytes that the caller includes completely in a comparison."""
        for region in self.watched:
            start = max(address, region["address"])
            end = min(address + size, region["address"] + region["size"])
            region["compared"].update(range(start - region["address"], end - region["address"]))
        return self.read(address, size)

    def finish_memory_audit(self, name):
        for region in self.watched:
            if region["reads"] or region["writes"]:
                totals = self.memory_audit.setdefault((name, region["label"]),
                                                      (set(), set(), set()))
                totals[0].update(region["reads"])
                totals[1].update(region["writes"])
                totals[2].update(region["writes"] - region["compared"])
            for key in ("reads", "writes", "compared"):
                region[key].clear()

    def memory_gaps(self):
        return sum(bool(missing) for _, _, missing in self.memory_audit.values())

    def verify_memory_audit(self):
        """Exercise real store hooks, including a readback invalidated by a write."""
        code = (struct.pack("<2I", 0xE5801000, 0xE12FFF1E) if self.arch == "arm" else
                b"\x8B\x44\x24\x04\x8B\x54\x24\x08\x89\x10\xC3")
        entry = self.install_code(code)
        self.reset()
        address = self.alloc(4)
        self.watch(address, 4, "store probe")
        self.call(entry, [address, 1])
        self.readback(address, 4)
        self.call(entry, [address, 2])
        self.finish_memory_audit("stale readback")
        if self.memory_audit.get(("stale readback", "store probe"), ((), (), ()))[2] != set(range(4)):
            raise OracleError("memory audit failed to detect a stale output comparison")
        self.memory_audit.clear()
        self.call(entry, [address, 3])
        self.readback(address, 4)
        self.finish_memory_audit("complete readback")
        if self.memory_gaps():
            raise OracleError("memory audit rejected a complete output comparison")
        self.memory_audit.clear()
        self.reset()

    def _interrupt(self, uc, number, _):
        raise OracleError(f"original attempted interrupt {number}")

    def _check_writable(self, destination, length):
        writable = [(self.HEAP, self.HEAP_SIZE), (self.STACK, self.STACK_SIZE)]
        writable += [(start, len(data)) for start, data in self.initial]
        if length > self.HEAP_SIZE or not any(
                start <= destination and destination + length <= start + count
                for start, count in writable):
            raise OracleError("byte primitive destination is outside emulated writable memory")

    def _memmove(self, uc, address, size, _):
        destination, source, length = self.arguments(3)
        self._check_writable(destination, length)
        self.write(destination, self.read(source, length))
        if self.audit_memory:
            self._memory(uc, unicorn.UC_MEM_READ, source, length, 0, None)
            self._memory(uc, unicorn.UC_MEM_WRITE, destination, length, 0, None)
        uc.reg_write(x86.UC_X86_REG_EAX, destination)

    def _memset(self, uc, address, size, _):
        destination, value, length = self.arguments(3)
        self._check_writable(destination, length)
        self.write(destination, bytes([value & 255]) * length)
        if self.audit_memory:
            self._memory(uc, unicorn.UC_MEM_WRITE, destination, length, 0, None)
        uc.reg_write(x86.UC_X86_REG_EAX, destination)

    def reset(self):
        self.finish_memory_audit("unchecked before reset")
        for region in self.watched:
            self.remove_hook(region["hook"])
        self.watched.clear()
        self.cursor = self.HEAP
        for address, data in self.initial:
            self.uc.mem_write(address, data)

    def alloc(self, size, data=b""):
        if size < len(data) or size < 0:
            raise OracleError("invalid allocation")
        address = (self.cursor + 15) & ~15
        self.cursor = address + max(size, 1)
        if self.cursor > self.HEAP + self.HEAP_SIZE:
            raise OracleError("oracle heap exhausted")
        self.uc.mem_write(address, data + bytes(size - len(data)))
        return address

    def read(self, address, size):
        return bytes(self.uc.mem_read(address, size))

    def install_code(self, code):
        """Install one bounded emitted pipeline, invalidating previous translations."""
        if not code or len(code) > self.GENERATED_CODE_SIZE:
            raise OracleError("generated pipeline exceeds its code buffer")
        self.uc.mem_write(self.GENERATED_CODE,
                          bytes(code) + b"\xCC" * (self.GENERATED_CODE_SIZE - len(code)))
        self.uc.ctl_remove_cache(self.GENERATED_CODE, self.GENERATED_CODE + self.GENERATED_CODE_SIZE)
        for key in list(self.branch_cache):
            if self.GENERATED_CODE <= key[0] < self.GENERATED_CODE + self.GENERATED_CODE_SIZE:
                del self.branch_cache[key]
        return self.GENERATED_CODE

    def write(self, address, data):
        self.uc.mem_write(address, bytes(data))

    def add_hook(self, address, callback):
        return self.uc.hook_add(unicorn.UC_HOOK_CODE,
                                lambda uc, pc, size, data: callback(self),
                                begin=address, end=address)

    def remove_hook(self, handle):
        self.uc.hook_del(handle)

    def arguments(self, count):
        stack = self.uc.reg_read(x86.UC_X86_REG_ESP)
        return struct.unpack("<" + "I" * count, self.read(stack + 4, 4 * count))

    def return_from_call(self, value=0):
        stack = self.uc.reg_read(x86.UC_X86_REG_ESP)
        address, = struct.unpack("<I", self.read(stack, 4))
        self.uc.reg_write(x86.UC_X86_REG_EAX, value & 0xFFFFFFFF)
        self.uc.reg_write(x86.UC_X86_REG_ESP, stack + 4)
        self.uc.reg_write(x86.UC_X86_REG_EIP, address)

    def call(self, address, args, return64=False, instruction_budget=200000):
        if time.monotonic() > self.deadline:
            raise OracleError("time budget exhausted; reduce --samples or select --module")
        for register in (x86.UC_X86_REG_EAX, x86.UC_X86_REG_EBX, x86.UC_X86_REG_ECX,
                         x86.UC_X86_REG_EDX, x86.UC_X86_REG_ESI, x86.UC_X86_REG_EDI,
                         x86.UC_X86_REG_EBP):
            self.uc.reg_write(register, 0)
        self.uc.reg_write(x86.UC_X86_REG_EFLAGS, 2)
        self.uc.reg_write(x86.UC_X86_REG_FPCW, 0x37F)
        self.uc.reg_write(x86.UC_X86_REG_FPSW, 0)
        words = [self.RETURN] + [int(value) & 0xFFFFFFFF for value in args]
        stack = self.STACK + self.STACK_SIZE - 4096
        self.uc.mem_write(stack, bytes(4096))
        self.uc.mem_write(stack, struct.pack("<" + "I" * len(words), *words))
        self.uc.reg_write(x86.UC_X86_REG_ESP, stack)
        self.previous_branch = None
        self.previous_indirect = False
        self.direct_calls[address] = self.direct_calls.get(address, 0) + 1
        try:
            self.uc.emu_start(address, self.RETURN, count=instruction_budget)
        except unicorn.UcError as error:
            pc = self.uc.reg_read(x86.UC_X86_REG_EIP)
            raise OracleError(f"original at 0x{pc:x}, entry 0x{address:x}: {error}") from error
        if self.uc.reg_read(x86.UC_X86_REG_EIP) != self.RETURN:
            raise OracleError(f"instruction budget exhausted at entry 0x{address:x}")
        value = self.uc.reg_read(x86.UC_X86_REG_EAX)
        if return64:
            value |= self.uc.reg_read(x86.UC_X86_REG_EDX) << 32
        return value


class OriginalARM(Original):
    """The supplied uncompressed E32 ARM/Thumb image, with AAPCS calls."""
    arch = "arm"

    def add_hook(self, address, callback):
        return super().add_hook(address & ~1, callback)

    def enable_predicate_coverage(self, library):
        """Install a native callback; instruction execution stays out of Python."""
        self.predicate_library = library
        self.predicate_sites = set()
        self.unicorn_library = unicorn.Uc.hook_add.__globals__["uclib"]
        library.differential_arm_coverage_create.argtypes = [ctypes.c_uint64, ctypes.c_size_t,
                                                            ctypes.c_void_p, ctypes.c_int]
        library.differential_arm_coverage_create.restype = ctypes.c_void_p
        library.differential_arm_coverage_add.argtypes = [ctypes.c_void_p, ctypes.c_uint64, ctypes.c_uint]
        library.differential_arm_coverage_add.restype = ctypes.c_int
        library.differential_arm_coverage_get.argtypes = [ctypes.c_void_p, ctypes.c_uint64]
        library.differential_arm_coverage_get.restype = ctypes.c_uint
        for name in ("error", "count", "destroy"):
            function = getattr(library, "differential_arm_coverage_" + name)
            function.argtypes = [ctypes.c_void_p]
            function.restype = None if name == "destroy" else ctypes.c_uint

        def install(base, size):
            state = library.differential_arm_coverage_create(
                base, size, ctypes.cast(self.unicorn_library.uc_reg_read, ctypes.c_void_p), arm.UC_ARM_REG_CPSR)
            if not state:
                raise OracleError("could not allocate ARM predicate coverage")
            handle = ctypes.c_size_t()
            callback = library.differential_arm_coverage_callback
            status = self.unicorn_library.uc_hook_add(
                self.uc._uch, ctypes.byref(handle), unicorn.UC_HOOK_CODE,
                ctypes.cast(callback, ctypes.c_void_p), ctypes.c_void_p(state),
                ctypes.c_uint64(base), ctypes.c_uint64(base + size - 1))
            if status:
                library.differential_arm_coverage_destroy(state)
                raise OracleError(f"could not install native ARM coverage: {status}")
            self.uc._callbacks[handle.value] = callback
            return state, handle.value

        # Confirm this Unicorn version hooks skipped predicates too. The probe
        # has no target DLL dependencies: CMP r0,#0; MOVEQ/MOVNE r1; BX lr.
        entry = self.install_code(struct.pack("<4I", 0xE3500000, 0x03A01001, 0x13A01002, 0xE12FFF1E))
        probe, hook = install(entry, 16)
        try:
            for offset, condition in ((4, 0), (8, 1)):
                library.differential_arm_coverage_add(probe, entry + offset, condition)
            for value in (0, 1):
                self.call(entry, [value])
            if any(library.differential_arm_coverage_get(probe, entry + offset) != 3 for offset in (4, 8)):
                raise OracleError("Unicorn did not expose both ARM predicate outcomes")
        finally:
            self.remove_hook(hook)
            library.differential_arm_coverage_destroy(probe)
        self.predicate_state, self.predicate_hook = install(self.base, self.code_size)

    def close_predicate_coverage(self):
        if getattr(self, "predicate_state", None):
            self.remove_hook(self.predicate_hook)
            self.predicate_library.differential_arm_coverage_destroy(self.predicate_state)
            self.predicate_state = None

    def register_predicate(self, instruction):
        if len(instruction.bytes) == 4:
            condition = instruction.bytes[3] >> 4
            if condition < 14:
                if self.predicate_library.differential_arm_coverage_add(
                        self.predicate_state, instruction.address, condition):
                    raise OracleError("invalid ARM predicate registration")
                self.predicate_sites.add(instruction.address)

    def __init__(self, path, deadline, coverage=True, audit_memory=False):
        data = Path(path).read_bytes()
        if len(data) < 156 or data[16:20] != b"EPOC":
            raise OracleError("expected the original ARM E32 image")
        word = lambda offset: struct.unpack_from("<I", data, offset)[0]
        size, self.base, offset = word(0x30), word(0x4C), word(0x64)
        self.code_size = size
        if word(0x1C) or word(0x34) or word(0x44) or self.base != 0x8000 or offset + size > len(data):
            raise OracleError("expected uncompressed ARM code at 0x8000 without data/BSS")
        self.uc = unicorn.Uc(unicorn.UC_ARCH_ARM, unicorn.UC_MODE_ARM)
        self.uc.mem_map(self.base, (size + 4095) & ~4095,
                        unicorn.UC_PROT_READ | unicorn.UC_PROT_EXEC)
        self.uc.mem_write(self.base, data[offset:offset + size])
        self.initial = []
        self.uc.mem_map(self.HEAP, self.HEAP_SIZE, unicorn.UC_PROT_READ | unicorn.UC_PROT_WRITE)
        self.uc.mem_map(self.STACK, self.STACK_SIZE, unicorn.UC_PROT_READ | unicorn.UC_PROT_WRITE)
        self.uc.mem_map(self.RETURN, 4096, unicorn.UC_PROT_READ | unicorn.UC_PROT_EXEC)
        self.uc.mem_map(self.GENERATED_CODE, self.GENERATED_CODE_SIZE,
                        unicorn.UC_PROT_READ | unicorn.UC_PROT_EXEC)
        self.decoder = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_ARM)
        self.thumb_decoder = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB)
        self.decoder.detail = self.thumb_decoder.detail = True
        self.blocks, self.edges, self.indirect_edges = set(), set(), set()
        self.direct_calls, self.branch_cache = {}, {}
        self.previous_branch, self.previous_indirect = None, False
        self.watched, self.memory_audit = [], {}
        self.audit_memory, self.deadline = audit_memory, deadline
        self.block_hook = self.uc.hook_add(unicorn.UC_HOOK_BLOCK, self._block) if coverage else None
        self.uc.hook_add(unicorn.UC_HOOK_INTR, self._interrupt)
        # Hook the actual import stubs; E32 stores ordinals in their literals.
        # EABI memset uses (destination, length, value), unlike C memset.
        self.add_hook(0x34CBC, lambda machine: machine._copy_primitive())
        self.add_hook(0x34CB4, lambda machine: machine._compare_primitive())
        self.add_hook(0x34E44, lambda machine: machine._copy_primitive())
        self.add_hook(0x34E4C, lambda machine: machine._fill_primitive(False))
        self.add_hook(0x34CAC, lambda machine: machine._fill_primitive(True))
        self.add_hook(0x34E3C, lambda machine: machine._signed_divmod())
        self.reset()

    def _signed_divmod(self):
        numerator, denominator = (ctypes.c_int32(value).value for value in self.arguments(2))
        if denominator == 0:
            raise OracleError("original ARM requested division by zero")
        quotient = abs(numerator) // abs(denominator)
        if (numerator < 0) != (denominator < 0):
            quotient = -quotient
        remainder = numerator - quotient * denominator
        self.uc.reg_write(arm.UC_ARM_REG_R1, remainder & 0xFFFFFFFF)
        self.return_from_call(quotient)

    def _copy_primitive(self):
        destination, source, length = self.arguments(3)
        self._check_writable(destination, length)
        self.write(destination, self.read(source, length))
        if self.audit_memory:
            self._memory(self.uc, unicorn.UC_MEM_READ, source, length, 0, None)
            self._memory(self.uc, unicorn.UC_MEM_WRITE, destination, length, 0, None)
        self.return_from_call(destination)

    def _fill_primitive(self, clear):
        destination, length, value = self.arguments(3)
        self._check_writable(destination, length)
        self.write(destination, bytes([0 if clear else value & 255]) * length)
        if self.audit_memory:
            self._memory(self.uc, unicorn.UC_MEM_WRITE, destination, length, 0, None)
        self.return_from_call(destination)

    @staticmethod
    def conditional(instruction):
        return (instruction.id in (capstone.arm.ARM_INS_B, capstone.arm.ARM_INS_BX,
                                   capstone.arm.ARM_INS_BL, capstone.arm.ARM_INS_BLX)
                and instruction.cc not in (capstone.arm.ARM_CC_AL, capstone.arm.ARM_CC_INVALID))

    def _block(self, uc, address, size, _):
        self.blocks.add(address)
        if self.previous_branch is not None:
            outcomes = self.indirect_edges if self.previous_indirect else self.edges
            outcomes.add((self.previous_branch, address))
        thumb = bool(uc.reg_read(arm.UC_ARM_REG_CPSR) & 0x20)
        key = (address, size, thumb)
        if key not in self.branch_cache:
            decoder = self.thumb_decoder if thumb else self.decoder
            instructions = list(decoder.disasm(self.read(address, size), address))
            last = instructions[-1] if instructions else None
            conditional = bool(last and self.conditional(last))
            indirect = bool(last and last.id == capstone.arm.ARM_INS_BX and not conditional)
            self.branch_cache[key] = (last.address if conditional or indirect else None, indirect)
        self.previous_branch, self.previous_indirect = self.branch_cache[key]

    def arguments(self, count):
        registers = (arm.UC_ARM_REG_R0, arm.UC_ARM_REG_R1, arm.UC_ARM_REG_R2, arm.UC_ARM_REG_R3)
        values = tuple(self.uc.reg_read(reg) for reg in registers[:count])
        if count > 4:
            values += struct.unpack("<" + "I" * (count - 4),
                                    self.read(self.uc.reg_read(arm.UC_ARM_REG_SP), (count - 4) * 4))
        return values

    def return_from_call(self, value=0):
        destination = self.uc.reg_read(arm.UC_ARM_REG_LR)
        self.uc.reg_write(arm.UC_ARM_REG_R0, value & 0xFFFFFFFF)
        self.uc.reg_write(arm.UC_ARM_REG_PC, destination)

    def call(self, address, args, return64=False, instruction_budget=200000,
             initial_registers=None, trace_blocks=True):
        if time.monotonic() > self.deadline:
            raise OracleError("ARM time budget exhausted; reduce --samples or select --module")
        for register in range(arm.UC_ARM_REG_R0, arm.UC_ARM_REG_R12 + 1):
            self.uc.reg_write(register, 0)
        self.uc.reg_write(arm.UC_ARM_REG_CPSR, 0x10 | (0x20 if address & 1 else 0))
        stack = self.STACK + self.STACK_SIZE - 4096
        self.uc.mem_write(stack, bytes(4096))
        self.uc.reg_write(arm.UC_ARM_REG_SP, stack)
        self.uc.reg_write(arm.UC_ARM_REG_LR, self.RETURN)
        for register, value in zip((arm.UC_ARM_REG_R0, arm.UC_ARM_REG_R1,
                                    arm.UC_ARM_REG_R2, arm.UC_ARM_REG_R3), args[:4]):
            self.uc.reg_write(register, int(value) & 0xFFFFFFFF)
        if len(args) > 4:
            self.write(stack, struct.pack("<" + "I" * (len(args) - 4),
                                          *(int(value) & 0xFFFFFFFF for value in args[4:])))
        for register, value in (initial_registers or {}).items():
            if not arm.UC_ARM_REG_R4 <= register <= arm.UC_ARM_REG_R11:
                raise ValueError("only callee-saved ARM registers may be seeded")
            self.uc.reg_write(register, value & 0xFFFFFFFF)
        self.previous_branch, self.previous_indirect = None, False
        self.direct_calls[address & ~1] = self.direct_calls.get(address & ~1, 0) + 1
        suspended = self.block_hook is not None and not trace_blocks
        if suspended:
            self.remove_hook(self.block_hook)
            self.block_hook = None
        try:
            self.uc.emu_start(address, self.RETURN, count=instruction_budget)
        except unicorn.UcError as error:
            pc = self.uc.reg_read(arm.UC_ARM_REG_PC)
            raise OracleError(f"original ARM at 0x{pc:x}, entry 0x{address:x}: {error}") from error
        finally:
            if suspended:
                self.block_hook = self.uc.hook_add(unicorn.UC_HOOK_BLOCK, self._block)
                self.previous_branch = None
        if self.uc.reg_read(arm.UC_ARM_REG_PC) != self.RETURN:
            raise OracleError(f"ARM instruction budget exhausted at entry 0x{address:x}")
        value = self.uc.reg_read(arm.UC_ARM_REG_R0)
        if return64:
            value |= self.uc.reg_read(arm.UC_ARM_REG_R1) << 32
        return value


class Suite:
    def __init__(self, args):
        self.started = time.monotonic()
        library = ctypes.CDLL(str(Path(args.library).resolve()))
        library.differential_install_signal_handlers.argtypes = []
        library.differential_install_signal_handlers.restype = ctypes.c_int
        if library.differential_install_signal_handlers() != 0:
            raise OracleError("could not install native fault handlers; refusing to run")
        library.differential_set_timeout.argtypes = [ctypes.c_uint]
        library.differential_set_timeout.restype = None
        library.differential_set_timeout(math.ceil(args.seconds))
        self.lib = NativeLibrary(library)
        self.arch = args.arch
        oracle_type = OriginalARM if self.arch == "arm" else Original
        self.oracle = oracle_type(args.original_arm if self.arch == "arm" else args.original,
                                  self.started + args.seconds, not args.no_coverage, args.audit_memory)
        if self.arch == "arm" and not args.no_coverage:
            self.oracle.enable_predicate_coverage(library)
        if args.audit_memory:
            self.oracle.verify_memory_audit()
        self.rng = random.Random(args.seed)
        self.samples = args.samples
        self.max_failures = args.max_failures
        self.failures = 0
        self.counts = {}
        self.targets = {}
        self.arm_branches = {}
        self.code_differences = 0
        self.unverified_code = set()
        self.coverage_details = args.coverage_details
        self.coverage = not args.no_coverage

    def register(self, name, start, end):
        thumb = self.arch == "arm" and start & 1
        if self.arch == "arm":
            start &= ~1
        if end <= start:
            raise ValueError(f"invalid function range for {name}")
        decoder = self.oracle.thumb_decoder if thumb else self.oracle.decoder
        instructions = decoder.disasm(self.oracle.read(start, end - start), start)
        edges = set()
        for instruction in instructions:
            if self.arch == "arm" and not thumb and self.coverage:
                self.oracle.register_predicate(instruction)
            if self.oracle.conditional(instruction):
                edges.add((instruction.address, instruction.address + instruction.size))
                immediate = capstone.arm.ARM_OP_IMM if self.arch == "arm" else capstone.x86.X86_OP_IMM
                if instruction.operands[0].type == immediate:
                    edges.add((instruction.address, instruction.operands[0].imm))
                    if self.arch == "arm" and not thumb:
                        self.arm_branches[instruction.address] = (
                            instruction.address + instruction.size, instruction.operands[0].imm)
        self.targets[name] = (start, end, edges)

    def coverage_progress(self):
        result = len(self.oracle.edges) + len(self.oracle.indirect_edges)
        if self.arch == "arm" and self.coverage:
            result += self.oracle.predicate_library.differential_arm_coverage_count(self.oracle.predicate_state)
        return result

    def compare(self, name, inputs, expected, actual):
        self.counts[name] = self.counts.get(name, 0) + 1
        self.oracle.finish_memory_audit(name)
        if expected != actual:
            self.failures += 1
            print(f"MISMATCH {name} input={inputs!r}\n  recovered={expected!r}\n  original ={actual!r}", flush=True)
            if self.failures >= self.max_failures:
                raise OracleError("mismatch limit reached")

    def check(self, name, address, args, expected, return64=False):
        self.oracle.reset()
        actual = self.oracle.call(address, args, return64=return64)
        self.compare(name, args, expected, actual)

    def compare_bytes(self, name, inputs, expected, actual):
        """Compare every byte, limiting only the displayed mismatch detail."""
        if expected == actual:
            self.compare(name, inputs, (), ())
            return
        offsets = [i for i in range(max(len(expected), len(actual)))
                   if expected[i:i + 1] != actual[i:i + 1]][:16]
        self.compare(name, inputs,
                     (len(expected), [(hex(i), expected[i:i + 1].hex()) for i in offsets]),
                     (len(actual), [(hex(i), actual[i:i + 1].hex()) for i in offsets]))

    def compare_code(self, name, inputs, recovered, original):
        """Different encodings require a subsequent semantic execution check."""
        if recovered != original:
            self.code_differences += 1
            self.unverified_code.add((name, repr(inputs)))

    def confirm_code_semantics(self, name, inputs):
        self.unverified_code.discard((name, repr(inputs)))

    def report(self):
        if self.arch == "arm" and self.coverage:
            # Native predicates also measure Bcc outcomes when the expensive
            # Python block tracer is suspended during compiler execution.
            for address, (fallthrough, taken) in self.arm_branches.items():
                mask = self.oracle.predicate_library.differential_arm_coverage_get(
                    self.oracle.predicate_state, address)
                if mask & 1:
                    self.oracle.edges.add((address, fallthrough))
                if mask & 2:
                    self.oracle.edges.add((address, taken))
        covered = total = 0
        direct = sum(bool(self.oracle.direct_calls.get(start)) for start, _, _ in self.targets.values())
        for name, (start, end, edges) in sorted(self.targets.items()) if self.coverage else ():
            missing = edges - self.oracle.edges
            hit = len(edges) - len(missing)
            covered += hit
            total += len(edges)
            calls = self.oracle.direct_calls.get(start, 0)
            checks = self.counts.get(name, 0)
            dispatch = {(a, b) for a, b in self.oracle.indirect_edges if start <= a < end}
            executed = any(start <= block < end for block in self.oracle.blocks)
            label = (f"{calls} direct calls, {checks} comparisons" if calls else
                     "callee execution only" if executed else "not executed")
            print(f"  {name}: {hit}/{len(edges)} conditional outcomes executed; {label}"
                  + (f"; {len(dispatch)} indirect jump outcomes observed" if dispatch else ""))
            if missing and self.coverage_details:
                print("    uncovered: " + ", ".join(f"0x{a:x}->0x{b:x}" for a, b in sorted(missing)))
        coverage = f"{covered}/{total} conditional branch outcomes" if self.coverage else "coverage hooks disabled"
        print(f"{self.arch}: {sum(self.counts.values())} comparisons; {self.failures} mismatches; "
              f"{len(self.targets)} registered functions ({direct} invoked directly); {coverage}; "
              f"{time.monotonic() - self.started:.2f}s")
        print("Branch counts describe original execution, including assertions. Comparisons check each "
              "fixture's declared outputs; callee execution does not imply a separate output check.")
        if self.arch == "arm" and self.coverage:
            predicates = [(address, self.oracle.predicate_library.differential_arm_coverage_get(
                self.oracle.predicate_state, address)) for address in sorted(self.oracle.predicate_sites)]
            reached = [(address, mask) for address, mask in predicates if mask]
            hits = sum(mask.bit_count() for _, mask in reached)
            print(f"ARM predication: {hits}/{2 * len(reached)} true/false outcomes at "
                  f"{len(reached)} reached instruction sites (native hook); unexecuted sites excluded.")
            if self.coverage_details:
                print("  one-sided predicates: " + ", ".join(
                    f"0x{address:x} missing {'true' if mask == 1 else 'false'}"
                    for address, mask in reached if mask != 3))
            if self.oracle.predicate_library.differential_arm_coverage_error(self.oracle.predicate_state):
                self.failures += 1
                print("INCOMPLETE: native ARM predicate register read failed")
        if self.code_differences:
            print(f"Emitted code: {self.code_differences} encoding differences; "
                  f"{len(self.unverified_code)} without corresponding semantic execution checks.")
        if self.oracle.audit_memory:
            self.oracle.finish_memory_audit("unchecked at end")
            for (name, label), (reads, writes, missing) in sorted(self.oracle.memory_audit.items()):
                if missing:
                    print(f"  MEMORY GAP {name}: original wrote {label}" + self._ranges(missing)
                          + " without a complete readback comparison")
            print(f"Memory audit: {len(self.oracle.memory_audit)} target/allocation pairs; "
                  f"{self.oracle.memory_gaps()} with unchecked writes. "
                  "Only explicitly watched allocations are audited.")

    @staticmethod
    def _ranges(offsets):
        runs = []
        for offset in sorted(offsets):
            if runs and offset == runs[-1][1]:
                runs[-1][1] += 1
            else:
                runs.append([offset, offset + 1])
        return ",".join(f"+0x{start:x}..+0x{end:x}" for start, end in runs)


def main():
    root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--original", type=Path, default=root / "docs/libGLES_CM.dll")
    parser.add_argument("--original-arm", type=Path, default=root / "docs/libgles_cm.dll.epoc")
    parser.add_argument("--arch", choices=("arm", "x86", "both"), default="both",
                        help="original target; both runs ARM first (default)")
    parser.add_argument("--library", type=Path, default=root / "build/differential/libgerbera_diff.so")
    parser.add_argument("--module", action="append", choices=("numeric", "vertex", "raster", "texture", "clipper", "gl", "egl"))
    parser.add_argument("--samples", type=int, default=128)
    parser.add_argument("--seed", type=int, default=0x474C4553)
    parser.add_argument("--seconds", type=float, default=60)
    parser.add_argument("--max-failures", type=int, default=10)
    parser.add_argument("--coverage-details", action="store_true",
                        help="list the addresses of every uncovered branch outcome")
    parser.add_argument("--no-coverage", action="store_true",
                        help="disable block hooks for faster randomized comparisons")
    parser.add_argument("--audit-memory", action="store_true",
                        help="fail on unchecked original writes to watched fixture allocations")
    args = parser.parse_args()
    if args.samples < 0 or not math.isfinite(args.seconds) or args.seconds <= 0 or args.max_failures <= 0:
        parser.error("samples must be nonnegative; seconds and max-failures must be positive")
    architectures = ("arm", "x86") if args.arch == "both" else (args.arch,)
    failed = 0
    for architecture in architectures:
        args.arch = architecture
        suite = Suite(args)
        print(f"Original/source differential: arch={architecture}, seed={args.seed}, "
              f"samples={args.samples}, budget={args.seconds:g}s", flush=True)
        try:
            for module in args.module or ("numeric", "vertex", "raster", "texture", "clipper", "gl", "egl"):
                importlib.import_module(f"differential_{module}").run(suite)
        except (OracleError, ValueError) as error:
            print(f"INCOMPLETE ({architecture}): {error}", file=sys.stderr)
            return 2
        finally:
            suite.report()
            if architecture == "arm":
                suite.oracle.close_predicate_coverage()
            suite.lib.library.differential_set_timeout(0)
        failed |= int(suite.failures != 0 or suite.oracle.memory_gaps() != 0 or bool(suite.unverified_code))
    return failed


if __name__ == "__main__":
    sys.exit(main())
