"""Execute original and recovered IA-32 pixel programs with their native ABI."""

import ctypes as C
import struct
from unicorn import x86_const as X86

from differential_raster import buffer, words, unpack


TARGETS = {
    "compileInternal": (0x45CB20, 0x45CFA4),
    "HCArena_init": (0x45E020, 0x45E0BE),
    "HCX86Backend_create": (0x450E70, 0x450E7A),
    "HCX86Backend_codegen": (0x44F1A0, 0x45044E),
    "HCX86Backend_preRegallocFixer": (0x450500, 0x450790),
    "HCX86Backend_postRegallocFixer": (0x4507B0, 0x450E70),
    "grExecutePixelPipeline": (0x421D60, 0x421E60),
}


def compile_pipeline(suite, program, native_program, case):
    oracle, lib = suite.oracle, suite.lib
    arena_bytes, capacity = 1024 * 1024, 65536
    arena = oracle.alloc(arena_bytes)
    manager, output, mask = oracle.alloc(128), oracle.alloc(4), oracle.alloc(16)
    mutable = oracle.alloc(12, bytes.fromhex("fefffeff00fcffffffff0000"))
    oracle.watch(mask, 16, "x86 compiled parameter mask")
    oracle.call(TARGETS["HCArena_init"][0], [manager, arena, arena_bytes])
    backend = oracle.call(TARGETS["HCX86Backend_create"][0], [])
    # Trace one representative compilation; debug-build compiler loops otherwise
    # overwhelm Python callbacks. Every emitted program is still executed below.
    length = oracle.call(TARGETS["compileInternal"][0],
                         [manager, output, 0, program, backend, 0x3FF, mask, mutable, 78],
                         instruction_budget=200000000, trace_blocks=(case == 0))
    pointer, = unpack(oracle.read(output, 4))
    if length > capacity or not arena <= pointer <= arena + arena_bytes - length:
        raise ValueError("original x86 compiler exceeded its bounded arena")
    native_output, native_mask = buffer(bytes(capacity)), buffer(bytes(16))
    lib.differential_raster_compile_x86.argtypes = [C.c_void_p, C.c_uint32, C.c_void_p,
                                                  C.c_int, C.c_void_p]
    lib.differential_raster_compile_x86.restype = C.c_int
    native_length = lib.differential_raster_compile_x86(
        buffer(native_program), 0x3FF, native_output, capacity, native_mask)
    if native_length < 0 or native_length > capacity:
        raise ValueError("recovered x86 compiler exceeded its bounded arena")
    original, recovered = oracle.read(pointer, length), bytes(native_output)[:native_length]
    suite.compare("compileInternal", ("x86 span", case), bytes(native_mask), oracle.readback(mask, 16))
    suite.compare_code("compileInternal", ("x86 span", case), recovered, original)
    return original, recovered


def compare_span(suite, signature, codes, case):
    oracle, lib = suite.oracle, suite.lib
    gradients = ([0x10000000, 0, 80 << 16, 40 << 16, 180 << 16, 64 << 16, 0, 0, 0, 0]
                 + [0x01000000, 0, 7 << 16, 9 << 16, -(11 << 16), 31 << 16, 0, 0, 0, 0]
                 + [0x02000000, 0, 4 << 16, 3 << 16, 2 << 16, 5 << 16, 0, 0, 0, 0])
    initial = (words([0x8F3A716C + i for i in range(8)]) + b"\xA5" * 8,
               struct.pack("<8H", *([0x2000, 0xFFFF] * 4)) + b"\xA5" * 8,
               bytes((1, 2, 3, 4, 4, 3, 2, 1)) + b"\xA5" * 8)
    buffers = [oracle.alloc(len(data), data) for data in initial]
    for address, data, label in zip(buffers, initial, ("x86 color", "x86 depth", "x86 stencil")):
        oracle.watch(address, len(data), label)
    render_stack = oracle.alloc(23860)
    stack = render_stack + 14644
    pipe = stack + 44  # HC_extraStackSpace: 40 backend bytes plus the first word.
    spans = oracle.alloc(8, words([4 << 16, 1 | (2 << 16)]))
    # Call the original dispatch too: its argument is the scratch base rather
    # than the parameter pointer stored separately in Gerbera_s.
    state = oracle.alloc(8192)
    oracle.write(state + 0x7C, words([render_stack]))
    saved = (X86.UC_X86_REG_EBX, X86.UC_X86_REG_ESI,
             X86.UC_X86_REG_EDI, X86.UC_X86_REG_EBP)
    registers = {register: 0x12345670 + index * 0x11111111 for index, register in enumerate(saved)}
    expected_abi = (*registers.values(), oracle.STACK + oracle.STACK_SIZE - 4096 + 4)
    results = []
    for code_index, code in enumerate(codes):
        entry = oracle.install_code(code)
        # State offsets are pinned by grExecutePixelPipeline's x86 disassembly.
        oracle.write(state + 0x8C, words([entry]))
        executions = []
        # A direct call checks all emitted callee-saved registers. The dispatcher
        # restores EBX itself, which would otherwise conceal an emitter defect.
        for address, arguments in ((entry, [stack]), (TARGETS["grExecutePixelPipeline"][0], [state])):
            for pointer, data in zip(buffers, initial):
                oracle.write(pointer, data)
            oracle.write(pipe + 132, words([16, 8, 4, 0, 64, 3, 3, 255, 255]))
            oracle.write(pipe + 168, words([*buffers, 2, spans, 0]))
            oracle.write(pipe + 192, words(gradients))
            oracle.call(address, arguments, initial_registers=registers)
            executions.append((tuple(oracle.readback(pointer, len(data))
                                     for pointer, data in zip(buffers, initial)),
                               tuple(oracle.uc.reg_read(reg) for reg in (*saved, X86.UC_X86_REG_ESP))))
        suite.compare("grExecutePixelPipeline", (case, code_index), executions[1], executions[0])
        results.append(executions[0])
    suite.compare("compiledPixelSpan ABI", ("x86", case),
                  (results[0][1], results[1][1]), (expected_abi, expected_abi))
    suite.compare("compiledPixelSpan", ("x86 emitted", case), results[1], results[0])
    if results[1] == results[0]:
        suite.confirm_code_semantics("compileInternal", ("x86 span", case))
    actual = [buffer(data) for data in initial]
    lib.differential_raster_span.argtypes = [C.c_void_p] * 5
    lib.differential_raster_span.restype = None
    lib.differential_raster_span(buffer(signature), buffer(words(gradients)), *actual)
    suite.compare("compiledPixelSpan", ("x86 hosted", case), tuple(bytes(data) for data in actual), results[0][0])


def run(suite, descriptors):
    if suite.arch != "x86":
        return
    for name, (start, end) in TARGETS.items():
        suite.register(name, start, end)
    oracle, lib = suite.oracle, suite.lib
    for case in range(min(suite.samples, 12)):
        signature = bytearray(144)
        signature[:16] = descriptors[2]
        signature[10:12] = bytes((2, 1))
        for index, value in ((16, 1), (17, 15), (42, 1), (47, 1)):
            signature[index] = value
        if case == 1:
            signature[18], signature[21], signature[22] = 1, 1, 3
        elif case == 2:
            signature[27:30] = bytes((1, 4, 5))
        elif case == 3:
            signature[31:38] = bytes((1, 1, 1, 3, 2, 3, 1))
        elif case == 4:
            signature[19], signature[20] = 1, 4
        elif case >= 5:
            source, destination = ((1, 0), (0, 1), (2, 3), (6, 7), (8, 9), (10, 1), (1, 10))[case - 5]
            signature[27:30] = bytes((1, source, destination))
        signature = bytes(signature)
        oracle.reset()
        sig, program = oracle.alloc(144, signature), oracle.alloc(32768)
        oracle.watch(program, 32768, "x86 HC instructions")
        count = oracle.call(0x43A8F0, [program, sig, 4096], instruction_budget=2000000)
        native_program = buffer(bytes(32768))
        lib.grExecuteProgram.argtypes = [C.c_void_p, C.c_void_p, C.c_int]
        lib.grExecuteProgram.restype = C.c_int
        native_count = lib.grExecuteProgram(native_program, buffer(signature), 4096)
        suite.compare("grExecuteProgram", ("x86 span", case),
                      (native_count, bytes(native_program)), (count, oracle.readback(program, 32768)))
        codes = compile_pipeline(suite, program, bytes(native_program), case)
        compare_span(suite, signature, codes, case)
