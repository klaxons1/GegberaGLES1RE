"""Stateful public matrix API comparisons, including every inactive stack slot."""
import ctypes as C
import struct


# Operation order is shared with diff_gl_matrix_step. Scalar float arguments
# travel as IEEE words in the x86 ABI; the four matrix-array calls use pointers.
OPERATIONS = (
    ("glMatrixMode", 0x4162D0, 0x416359, 1),
    ("glActiveTexture", 0x416230, 0x4162C6, 1),
    ("glCurrentPaletteMatrixOES", 0x41B260, 0x41B2D0, 1),
    ("glLoadPaletteFromModelViewMatrixOES", 0x41B2D0, 0x41B340, 0),
    ("glPushMatrix", 0x416360, 0x416471, 0),
    ("glPopMatrix", 0x416480, 0x416564, 0),
    ("glLoadIdentity", 0x416570, 0x4165A3, 0),
    ("glLoadMatrixf", 0x416600, 0x416637, 16),
    ("glLoadMatrixx", 0x416640, 0x416684, 16),
    ("glMultMatrixf", 0x416690, 0x4166EB, 16),
    ("glMultMatrixx", 0x4166F0, 0x416734, 16),
    ("glRotatef", 0x416740, 0x4167CF, 4),
    ("glRotatex", 0x4167D0, 0x416819, 4),
    ("glTranslatef", 0x416820, 0x41687D, 3),
    ("glTranslatex", 0x416880, 0x4168BA, 3),
    ("glScalef", 0x4168C0, 0x41692F, 3),
    ("glScalex", 0x416930, 0x41696A, 3),
    ("glFrustumf", 0x416970, 0x416A88, 6),
    ("glFrustumx", 0x416A90, 0x416AF7, 6),
    ("glOrthof", 0x416B00, 0x416BFC, 6),
    ("glOrthox", 0x416C00, 0x416C67, 6),
    ("glGetError", 0x414650, 0x414720, 0),
)
INDEX = {name: i for i, (name, *_) in enumerate(OPERATIONS)}
ARM_RANGES = (
    (0x10228, 0x102CC), (0x101C4, 0x10228), (0x144E0, 0x1452C), (0x1452C, 0x14580),
    (0x102CC, 0x10330), (0x10330, 0x1037C), (0x1037C, 0x1039C),
    (0x103CC, 0x103F4), (0x103F4, 0x1042C), (0x1042C, 0x10478), (0x10478, 0x104B0),
    (0x104B0, 0x105E4), (0x10618, 0x10668), (0x10668, 0x106AC), (0x106AC, 0x106E8),
    (0x106E8, 0x1078C), (0x1078C, 0x107C8), (0x107C8, 0x108BC), (0x108BC, 0x10938),
    (0x10938, 0x10A24), (0x10A24, 0x10AA0), (0xEC04, 0xEC3C),
)
STACK_COUNT = 13
STACK_DEPTH = 4
STACK_BYTES = 4 + 80 * STACK_DEPTH
CONTEXT_BYTES = 1068
SNAPSHOT_BYTES = 980 + STACK_COUNT * STACK_BYTES
GETTER_OFFSET = 0x148
GETTER_BYTES = 676
GL_MODELVIEW, GL_PROJECTION, GL_TEXTURE, GL_MATRIX_PALETTE_OES = 0x1700, 0x1701, 0x1702, 0x8840
GL_TEXTURE0, GL_TEXTURE1 = 0x84C0, 0x84C1


def words(values):
    return struct.pack("<" + "I" * len(values), *(v & 0xFFFFFFFF for v in values))


def command(name, *values):
    if name.endswith("f"):
        values = struct.unpack("<" + "I" * len(values), struct.pack("<" + "f" * len(values), *values))
    elif name.endswith("x"):
        values = tuple(int(v * 65536) & 0xFFFFFFFF for v in values)
    return INDEX[name], tuple(values)


def seed_sequences():
    matrix = [1.0 if c == r else 0.0 for c in range(4) for r in range(4)]
    matrix[1], matrix[4], matrix[12] = 0.25, -0.5, 2.0
    arithmetic = []
    for suffix in ("f", "x"):
        arithmetic += [command("glLoadMatrix" + suffix, *matrix),
                       command("glPushMatrix"), command("glTranslate" + suffix, -2, 0.5, 3),
                       command("glRotate" + suffix, 45, 1, -2, 0.5),
                       command("glScale" + suffix, -1, 2, 0.25),
                       command("glMultMatrix" + suffix, *matrix), command("glPopMatrix"),
                       command("glFrustum" + suffix, -1, 2, -0.5, 1, 0.25, 20),
                       command("glOrtho" + suffix, -1, 2, -0.5, 1, -1, 20)]
    stacks = []
    for mode in (GL_MODELVIEW, GL_PROJECTION, GL_TEXTURE, GL_MATRIX_PALETTE_OES):
        stacks += [command("glMatrixMode", mode)] + [command("glPushMatrix")] * 5
        stacks += [command("glMatrixMode", 0), command("glGetError")]
        stacks += [command("glPopMatrix")] * 5 + [command("glGetError")]
    selection = [command("glMatrixMode", GL_TEXTURE)]
    for unit in (GL_TEXTURE1, GL_TEXTURE0, GL_TEXTURE1, GL_TEXTURE1 + 1):
        selection += [command("glActiveTexture", unit), command("glTranslatef", 1, 2, 3)]
    selection += [command("glMatrixMode", GL_MODELVIEW), command("glTranslatex", 4, -2, 1),
                  command("glMatrixMode", GL_MATRIX_PALETTE_OES)]
    for palette in (0, 8, 3, 9, 0xFFFFFFFF):
        selection += [command("glCurrentPaletteMatrixOES", palette),
                      command("glLoadPaletteFromModelViewMatrixOES"), command("glGetError")]
    invalid = []
    for name in ("glFrustumf", "glFrustumx", "glOrthof", "glOrthox"):
        for planes in ((1, 1, -1, 1, 1, 2), (-1, 1, 1, 1, 1, 2),
                       (-1, 1, -1, 1, 2, 2), (-1, 1, -1, 1, -1, 2)):
            invalid += [command("glLoadIdentity"), command(name, *planes), command("glGetError")]
    return [arithmetic, stacks, selection, invalid]


def run(suite):
    oracle, lib = suite.oracle, suite.lib
    operations = (tuple((entry[0], *address, entry[3]) for entry, address in zip(OPERATIONS, ARM_RANGES))
                  if suite.arch == "arm" else OPERATIONS)
    get_context = 0xB59C if suite.arch == "arm" else 0x4085B0
    set_matrix = 0x2D87C if suite.arch == "arm" else 0x4616D0
    getter_defaults = 0x350F4 if suite.arch == "arm" else 0x471A74
    for name, start, end, _ in operations:
        suite.register(name, start, end)
    lib.diff_gl_matrix_create.argtypes = [C.c_uint, C.c_void_p]
    lib.diff_gl_matrix_create.restype = C.c_void_p
    lib.diff_gl_matrix_destroy.argtypes = [C.c_void_p]
    lib.diff_gl_matrix_destroy.restype = None
    lib.diff_gl_matrix_step.argtypes = [C.c_void_p, C.c_int, C.c_void_p]
    lib.diff_gl_matrix_step.restype = C.c_uint32
    lib.diff_gl_matrix_snapshot.argtypes = [C.c_void_p, C.c_void_p]
    lib.diff_gl_matrix_snapshot.restype = None
    # Establish identity from the original; setfv itself has independent checks.
    oracle.reset()
    floats = words([0x3F800000 if i % 5 == 0 else 0 for i in range(16)])
    source, destination = oracle.alloc(64, floats), oracle.alloc(80)
    oracle.call(set_matrix, [destination, source])
    identity = oracle.read(destination, 80)
    defaults = oracle.read(getter_defaults, GETTER_BYTES)
    native_identity = C.create_string_buffer(identity, len(identity))

    def execute(sequence, fill, case):
        oracle.reset()
        core = oracle.alloc(0x3000)
        context = bytearray(CONTEXT_BYTES)
        struct.pack_into("<I", context, 0, core)
        context[GETTER_OFFSET:GETTER_OFFSET + GETTER_BYTES] = defaults
        stacks = []
        for i in range(STACK_COUNT):
            data = bytes((0, STACK_DEPTH, 0, fill)) + identity + bytes([fill]) * (80 * (STACK_DEPTH - 1))
            stack = oracle.alloc(STACK_BYTES, data)
            stacks.append(stack)
            struct.pack_into("<I", context, 0x18 + 4 * i, stack)
            oracle.watch(stack, STACK_BYTES, f"matrixStack[{i}]")
        ctx = oracle.alloc(CONTEXT_BYTES, bytes(context))
        arguments = oracle.alloc(64)
        oracle.watch(ctx, CONTEXT_BYTES, "context")
        oracle.watch(core, 0x3000, "core")
        hook = oracle.add_hook(get_context, lambda machine: machine.return_from_call(ctx))
        session = lib.diff_gl_matrix_create(fill, native_identity)
        if not session:
            raise ValueError("matrix fixture allocation failed")
        native_snapshot = C.create_string_buffer(SNAPSHOT_BYTES)
        try:
            for step, (operation, values) in enumerate(sequence):
                name, address, _, argc = operations[operation]
                data = words(values) + bytes(64 - 4 * len(values))
                oracle.write(arguments, data)
                result = oracle.call(address, [arguments] if argc == 16 else values)
                native_result = lib.diff_gl_matrix_step(session, operation, C.create_string_buffer(data, 64))
                # Void API return registers are unspecified. GetError is not.
                result = result if name == "glGetError" else 0
                raw = oracle.read(ctx, CONTEXT_BYTES)
                pointer_changes = int(struct.unpack_from("<I", raw, 0)[0] != core)
                for i, stack in enumerate(stacks):
                    pointer_changes |= int(struct.unpack_from("<I", raw, 0x18 + 4 * i)[0] != stack) << (i + 1)
                pointer_changes |= int(any(struct.unpack_from("<I", raw, offset)[0]
                                           for offset in (8, 12, 16, 20, 76, 80, 84, 116, 120))) << 14
                original = words([pointer_changes]) + oracle.readback(ctx + 4, 4)
                original += oracle.readback(ctx + 88, 28) + oracle.readback(ctx + 124, 944)
                original += b"".join(oracle.readback(p, STACK_BYTES) for p in stacks)
                lib.diff_gl_matrix_snapshot(session, native_snapshot)
                suite.compare_bytes(name, {"matrix_sequence": case, "step": step, "fill": fill,
                                           "prefix": sequence[:step + 1]},
                                    words([native_result]) + native_snapshot.raw,
                                    words([result]) + original)
        finally:
            lib.diff_gl_matrix_destroy(session)
            oracle.remove_hook(hook)

    corpus = seed_sequences()
    # Each seed runs with two unused-slot patterns. Both sides receive the same
    # pattern; differences in live outputs must not come from hidden zero inputs.
    for i, sequence in enumerate(corpus):
        for fill in (0, 0xA5):
            execute(sequence, fill, f"seed {i}")
    mutation_pool = [entry for sequence in corpus for entry in sequence]
    retained = 0
    for case in range(min(32, suite.samples // 4)):
        sequence = list(suite.rng.choice(corpus))[:48]
        for _ in range(1 + case % 8):
            where = suite.rng.randrange(len(sequence) + 1)
            if case % 3 == 0 and where < len(sequence):
                del sequence[where]
            else:
                sequence.insert(where, suite.rng.choice(mutation_pool))
        before = suite.coverage_progress()
        execute(sequence, 0xA5 if case & 1 else 0, f"mutation {case}")
        if suite.coverage_progress() > before:
            corpus.append(sequence)
            retained += 1
    if suite.coverage:
        print(f"  Matrix command corpus: {retained} mutations retained for new original branch outcomes")
