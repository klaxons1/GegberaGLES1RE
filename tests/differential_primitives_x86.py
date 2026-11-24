"""Bounded cdecl memory primitives executed inside the x86 oracle."""

import struct

import unicorn


class _Code:
    def __init__(self, address):
        self.address = address
        self.data = bytearray()
        self.labels = {}
        self.fixups = []

    def emit(self, code):
        self.data.extend(bytes.fromhex(code))

    def immediate(self, value):
        self.data.extend(struct.pack("<I", value))

    def label(self, name):
        self.labels[name] = self.address + len(self.data)

    def branch(self, opcode, target):
        self.emit(opcode)
        self.fixups.append((len(self.data), target))
        self.data.extend(bytes(4))

    def finish(self):
        for offset, target in self.fixups:
            address = self.labels[target] if isinstance(target, str) else target
            struct.pack_into("<i", self.data, offset, address - (self.address + offset + 4))
        return bytes(self.data)


def _validate(code, regions, register, prefix):
    # EDX = range end; ESI or EDI = range start. Inclusive endpoints match
    # the host check for a zero-length destination at the end of a region.
    code.emit("89 fa 01 ca" if register == "edi" else "89 f2 01 ca")
    code.branch("0f 82", "invalid") # jc: address + count wrapped
    for index, (start, length) in enumerate(regions):
        code.emit("81 ff" if register == "edi" else "81 fe")
        code.immediate(start)
        code.branch("0f 82", f"{prefix}{index}")
        code.emit("81 fa")
        code.immediate(start + length)
        code.branch("0f 86", f"{prefix}valid")
        code.label(f"{prefix}{index}")
    code.branch("e9", "invalid")
    code.label(f"{prefix}valid")


def _save(code, scratch):
    for instruction, offset in (("89 1d", 0), ("89 35", 4), ("89 3d", 8)):
        code.emit(instruction)         # save ebx, esi, edi without using the stack
        code.immediate(scratch + offset)
    code.emit("8b 7c 24 04 89 3d")    # edi = destination; save return value
    code.immediate(scratch + 12)
    code.emit("8b 4c 24 0c")          # ecx = byte count


def _restore(code, scratch):
    for instruction, offset in (("8b 1d", 0), ("8b 35", 4), ("8b 3d", 8),
                                ("a1", 12)):
        code.emit(instruction)         # restore ebx, esi, edi; eax = destination
        code.immediate(scratch + offset)
    code.emit("c3")
    code.label("invalid")
    code.emit("cc")                  # rejected by the oracle interrupt hook


def install_x86_primitives(oracle):
    """Replace Mem::Copy, memset and wordmove without host memory callbacks."""
    move_address = oracle.RETURN + 0x800
    set_address = oracle.RETURN + 0xC00
    scratch = oracle.RETURN + 0x10000
    writable = [(oracle.HEAP, oracle.HEAP_SIZE), (oracle.STACK, oracle.STACK_SIZE)]
    writable.extend((address, len(data)) for address, data in oracle.initial)
    # Scratch lies outside every accepted source/destination region. Neither
    # primitive calls back into the guest, so these saved registers cannot nest.
    # Avoid guest stack saves: legal inputs can cover the bytes below ESP.
    oracle.uc.mem_map(scratch, 4096, unicorn.UC_PROT_READ | unicorn.UC_PROT_WRITE)

    move = _Code(move_address)
    _save(move, scratch)
    move.emit("8b 74 24 08 81 f9")    # esi = source; cmp ecx, HEAP_SIZE
    move.immediate(oracle.HEAP_SIZE)
    move.branch("0f 87", "invalid")
    _validate(move, writable, "edi", "dst")
    move.emit("fc 85 c9")             # cld; test ecx, ecx
    move.branch("0f 84", "done")
    # Source reads may cross adjacent mappings, but must not wrap or expose
    # the primitive's private scratch storage.
    move.emit("89 f2 01 ca")          # edx = source + count
    move.branch("0f 82", "invalid")
    move.emit("81 fe")
    move.immediate(scratch + 4096)
    move.branch("0f 83", "source")
    move.emit("81 fa")
    move.immediate(scratch)
    move.branch("0f 86", "source")
    move.branch("e9", "invalid")
    move.label("source")
    move.emit("39 f7")                # cmp edi, esi
    move.branch("0f 86", "forward")
    move.emit("89 fa 29 f2 39 ca")    # edx = edi - esi; cmp edx, ecx
    move.branch("0f 83", "forward")
    # Copy overlapping ranges backward, byte tail first then complete dwords.
    move.emit("8d 74 0e ff 8d 7c 0f ff fd")
    move.emit("89 cb 83 e1 03 f3 a4") # ebx = count; rep movsb (tail)
    move.emit("83 ee 03 83 ef 03")    # point to the last complete dword
    move.emit("89 d9 c1 e9 02 f3 a5") # rep movsd
    move.branch("e9", "done")
    move.label("forward")
    move.emit("89 cb c1 e9 02 f3 a5") # ebx = count; rep movsd
    move.emit("89 d9 83 e1 03 f3 a4") # rep movsb (tail)
    move.label("done")
    move.emit("fc")
    _restore(move, scratch)

    fill = _Code(set_address)
    _save(fill, scratch)
    fill.emit("81 f9")               # cmp ecx, HEAP_SIZE
    fill.immediate(oracle.HEAP_SIZE)
    fill.branch("0f 87", "invalid")
    _validate(fill, writable, "edi", "dst")
    fill.emit("8b 44 24 08 25 ff 00 00 00") # eax = value & 255
    fill.emit("69 c0 01 01 01 01")    # repeat byte throughout eax
    fill.emit("fc 89 cb c1 e9 02 f3 ab") # ebx = count; rep stosd
    fill.emit("89 d9 83 e1 03 f3 aa") # rep stosb (tail)
    _restore(fill, scratch)

    for code, limit in ((move, set_address), (fill, oracle.RETURN + 4096)):
        data = code.finish()
        if code.address + len(data) > limit:
            raise ValueError("x86 memory primitive exceeds its reserved code range")
        oracle.uc.mem_write(code.address, data)
    oracle.MEMMOVE, oracle.MEMSET = move_address, set_address
    for address, target in ((0x4902F0, move_address), (0x490354, set_address),
                            (0x490358, move_address)):
        oracle.uc.mem_write(address, struct.pack("<I", target))
