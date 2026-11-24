"""Persistent public fixed-function state, getters, and error-retention checks."""
import ctypes as C
import struct

from differential_gl_matrix import (CONTEXT_BYTES, GETTER_BYTES, GETTER_OFFSET,
                                   SNAPSHOT_BYTES, STACK_BYTES, STACK_COUNT,
                                   STACK_DEPTH, words)

# Operation order is shared with diff_gl_state_step. Ranges are the exported
# original functions, including each scalar wrapper's parameter validation.
OPERATIONS = (
    ("glFogf", 0xF24C, 0xF2A4, 0x414EB0, 0x414F00),
    ("glFogfv", 0xF2A4, 0xF2D0, 0x414F00, 0x414F33),
    ("glFogx", 0xF2D0, 0xF328, 0x414F40, 0x414F90),
    ("glFogxv", 0xF328, 0xF354, 0x414F90, 0x414FC3),
    ("glMaterialf", 0x11798, 0x117FC, 0x417B20, 0x417B6E),
    ("glMaterialfv", 0x117FC, 0x11840, 0x417B70, 0x417BA6),
    ("glMaterialx", 0x11840, 0x118A4, 0x417BB0, 0x417BFE),
    ("glMaterialxv", 0x118A4, 0x118E8, 0x417C00, 0x417C36),
    ("glLightf", 0x11D18, 0x11D80, 0x418110, 0x418163),
    ("glLightfv", 0x11D80, 0x11DC4, 0x418170, 0x4181A6),
    ("glLightx", 0x11DD4, 0x11E3C, 0x4181B0, 0x418203),
    ("glLightxv", 0x11E3C, 0x11E80, 0x418210, 0x418246),
    ("glLightModelf", 0x11FA4, 0x11FF8, 0x418440, 0x418490),
    ("glLightModelfv", 0x11FF8, 0x12024, 0x418490, 0x4184C3),
    ("glLightModelx", 0x12024, 0x12078, 0x4184D0, 0x418520),
    ("glLightModelxv", 0x12078, 0x120A4, 0x418520, 0x418553),
    ("glTexEnvf", 0x12648, 0x126AC, 0x418AC0, 0x418B13),
    ("glTexEnvfv", 0x126C4, 0x12708, 0x418B20, 0x418B56),
    ("glTexEnvx", 0x12708, 0x1276C, 0x418B60, 0x418BB3),
    ("glTexEnvxv", 0x1276C, 0x127B0, 0x418BC0, 0x418BF6),
    ("glTexEnvi", 0x127B0, 0x12814, 0x418C00, 0x418C53),
    ("glTexEnviv", 0x12814, 0x12858, 0x418C60, 0x418C96),
    ("glGetLightfv", 0xE810, 0xE854, 0x4141E0, 0x414216),
    ("glGetLightxv", 0xE854, 0xE898, 0x414220, 0x414256),
    ("glGetMaterialfv", 0xE4B0, 0xE4F4, 0x413DB0, 0x413DE6),
    ("glGetMaterialxv", 0xE4F4, 0xE538, 0x413DF0, 0x413E26),
    ("glGetTexEnvfv", 0xE2D4, 0xE318, 0x413B10, 0x413B46),
    ("glGetTexEnviv", 0xE318, 0xE35C, 0x413B50, 0x413B86),
    ("glGetTexEnvxv", 0xE35C, 0xE3A0, 0x413B90, 0x413BC6),
    ("glGetFloatv", 0xDA54, 0xDA80, 0x413170, 0x4131A3),
    ("glGetFixedv", 0xDA80, 0xDAAC, 0x4131B0, 0x4131E3),
    ("glGetIntegerv", 0xDA00, 0xDA54, 0x4130F0, 0x413162),
    ("glGetBooleanv", 0xDAAC, 0xDAD8, 0x4131F0, 0x413223),
    ("glActiveTexture", 0x101C4, 0x10228, 0x416230, 0x4162C6),
    ("glGetError", 0xEC04, 0xEC3C, 0x414650, 0x414720),
    ("glClientActiveTexture", 0x11118, 0x11168, 0x4171A0, 0x417211),
    ("glEnableClientState", 0x110D0, 0x110F4, 0x417140, 0x417170),
    ("glDisableClientState", 0x110F4, 0x11118, 0x417170, 0x4171A0),
    ("glVertexPointer", 0x1146C, 0x114B8, 0x417860, 0x417899),
    ("glColorPointer", 0x113D8, 0x11424, 0x4177E0, 0x417819),
    ("glNormalPointer", 0x11424, 0x1146C, 0x417820, 0x417858),
    ("glTexCoordPointer", 0x11384, 0x113D8, 0x417790, 0x4177D2),
    ("glWeightPointerOES", 0x114B8, 0x11504, 0x4178A0, 0x4178D9),
    ("glMatrixIndexPointerOES", 0x11504, 0x11550, 0x4178E0, 0x417919),
    ("glPointSizePointerOES", 0x11550, 0x11598, 0x417920, 0x417958),
    ("glGetPointerv", 0xEB28, 0xEC04, 0x4144F0, 0x414642),
    ("glEnable", 0xD96C, 0xD990, 0x413030, 0x413060),
    ("glDisable", 0xD990, 0xD9B4, 0x413060, 0x413090),
    ("glIsEnabled", 0xDAD8, 0xDB30, 0x413230, 0x413311),
    ("glFrontFace", 0xF5EC, 0xF65C, 0x4152E0, 0x41536B),
    ("glCullFace", 0xF65C, 0xF6F0, 0x415370, 0x415419),
    ("glViewport", 0xF430, 0xF4FC, 0x4150A0, 0x4151A4),
    ("glScissor", 0xF4FC, 0xF5EC, 0x4151B0, 0x4152D4),
)
INDEX = {entry[0]: i for i, entry in enumerate(OPERATIONS)}
# Canonical offsets follow ARM. The x86 core has an eight-byte prefix.
CORE_RANGES = ((3, 4), (8, 9), (20, 1), (24, 5), (30, 2), (33, 1), (50, 1), (52, 55),
               (220, 88), (308, 160),
               (472, 28), (504, 28), (532, 20), (594, 2), (5520, 56),
               (5576, 12), (5712, 208 * 8))


def command(name, first=0, second=0, values=(), null=False):
    if name.endswith(("f", "fv")) and not name.startswith("glGet"):
        values = struct.unpack("<" + "I" * len(values), struct.pack("<" + "f" * len(values), *values))
    elif name.endswith(("x", "xv")) and not name.startswith("glGet"):
        # Enum parameters are encoded separately by enum_command below.
        values = tuple(int(v * 65536) & 0xFFFFFFFF for v in values)
    return INDEX[name], (first, second, *values, *([0] * (5 - len(values))), int(null))


def enum_command(name, first, second, value):
    # Fixed APIs pass enums as raw enum values, whereas float APIs convert.
    if name.endswith(("x", "xv")):
        return INDEX[name], (first, second, value, 0, 0, 0, 0, 0)
    return command(name, first, second, (value,))


def seeds(arm=False, reference_arm=False):
    fog, materials, lights, environment = [], [], [], []
    for suffix in ("f", "fv", "x", "xv"):
        for pname, values in ((0xB62, (0.5,)), (0xB63, (-2,)), (0xB64, (8,)),
                              (0xB64, (-2,)), (0xB62, (-1,)), (0xB66, (-0.5, 0.25, 1.5, 0.75))):
            fog.append(command("glFog" + suffix, pname, values=values))
            for getter in ("glGetFloatv", "glGetFixedv", "glGetIntegerv", "glGetBooleanv"):
                fog.append(command(getter, pname))
        for mode in (0x2601, 0x800, 0x801, 0):
            fog.append(enum_command("glFog" + suffix, 0xB65, 0, mode))
        fog += [command("glFog" + suffix, 0, values=(1,)), command("glGetError")]
        for pname in (0x1200, 0x1201, 0x1202, 0x1600, 0x1602):
            materials.append(command("glMaterial" + suffix, 0x408, pname, (0.25, 0.5, 0.75, 0.375)))
            for actual in ((0x1200, 0x1201) if pname == 0x1602 else (pname,)):
                for getter in ("glGetMaterialfv", "glGetMaterialxv"):
                    materials.append(command(getter, 0x404, actual))
        for value in (0, 17.5, 128, -1, 129):
            materials += [command("glMaterial" + suffix, 0x408, 0x1601, (value,)),
                          command("glGetMaterialfv", 0x405, 0x1601), command("glGetError")]
        materials += [command("glMaterial" + suffix, 0x404, 0x1601, (1,)), command("glGetError")]
        for pname, values in ((0xB53, (0.1, 0.2, 0.3, 0.4)), (0xB52, (1,)), (0xB52, (0,)), (0, (1,))):
            materials += [command("glLightModel" + suffix, pname, values=values),
                          command("glGetFloatv", pname), command("glGetError")]
        for light in (0x4000, 0x4003, 0x4007):
            for pname, values in ((0x1200, (0.1, 0.2, 0.3, 0.4)), (0x1201, (0.8, 0.6, 0.4, 0.2)),
                                  (0x1202, (0.25, 0.5, 0.75, 1)), (0x1203, (1, -2, 3, 0)),
                                  (0x1203, (2, -4, 6, 2)), (0x1204, (1, -2, 3)),
                                  (0x1205, (17.5,)), (0x1206, (45,)), (0x1206, (180,)),
                                  (0x1207, (0.5,)), (0x1208, (1.25,)), (0x1209, (2.5,))):
                lights += [command("glLight" + suffix, light, pname, values),
                           command("glGetLightfv", light, pname), command("glGetLightxv", light, pname)]
            for pname, value in ((0x1205, 129), (0x1205, -1), (0x1206, 91), (0x1207, -1)):
                lights += [command("glLight" + suffix, light, pname, (value,)), command("glGetError")]
    for unit in (0x84C0, 0x84C1):
        environment.append(command("glActiveTexture", unit))
        for suffix in ("f", "fv", "x", "xv", "i", "iv"):
            name = "glTexEnv" + suffix
            for pname, choices in ((0x2200, (0x2100, 0x2101, 0xBE2, 0x1E01, 0x104, 0x8570, 0)),
                                   (0x8571, (0x2100, 0x1E01, 0x104, 0x8574, 0x8575, 0x84E7, 0x86AE, 0x86AF, 0)),
                                   (0x8572, (0x2100, 0x8575, 0x86AE, 0)),
                                   *[(p, (0x1702, 0x8576, 0x8577, 0x8578, 0)) for p in (*range(0x8580, 0x8583), *range(0x8588, 0x858B))],
                                   *[(p, (0x300, 0x301, 0x302, 0x303, 0)) for p in (*range(0x8590, 0x8593), *range(0x8598, 0x859B))]):
                for value in choices:
                    environment += [enum_command(name, 0x2300, pname, value),
                                    command("glGetTexEnviv", 0x2300, pname)]
            for pname in (0x8573, 0xD1C):
                for value in (1, 2, 4, 3, 0):
                    environment += [command(name, 0x2300, pname, (value,)),
                                    command("glGetTexEnvfv", 0x2300, pname),
                                    command("glGetTexEnvxv", 0x2300, pname)]
            environment += [command(name, 0x2300, 0x2201, (0.25, 0.5, 0.75, 1) if suffix[0] != "i" else (1, 0, 1, 0)),
                            command("glGetTexEnvfv", 0x2300, 0x2201),
                            enum_command(name, 0x8861, 0x8862, 1),
                            command("glGetTexEnviv", 0x8861, 0x8862), command("glGetError")]
    invalid = []
    for name in ("glGetLightfv", "glGetLightxv", "glGetMaterialfv", "glGetMaterialxv",
                 "glGetTexEnvfv", "glGetTexEnviv", "glGetTexEnvxv"):
        valid = 0x4000 if "Light" in name else 0x404 if "Material" in name else 0x2300
        for first, second, null in ((0, 0, False), (valid, 0, False), (valid, 0x1200, True)):
            invalid += [command(name, first, second, null=null), command("glGetError")]
    arrays = []
    for unit in (0x84C0, 0x84C1):
        arrays.append(command("glClientActiveTexture", unit))
        for name, cap, pointer_token, default_size in (
                ("glVertexPointer", 0x8074, 0x808E, 3), ("glColorPointer", 0x8076, 0x8090, 4),
                ("glNormalPointer", 0x8075, 0x808F, 3), ("glTexCoordPointer", 0x8078, 0x8092, 2),
                ("glWeightPointerOES", 0x86AD, 0x86AC, 2),
                ("glMatrixIndexPointerOES", 0x8844, 0x8849, 2),
                ("glPointSizePointerOES", 0x8B9C, 0x898C, 1)):
            for datatype in (0x1400, 0x1401, 0x1402, 0x1403, 0x1404, 0x1406, 0x140C, 0):
                for stride in (0, 20, -1):
                    args = ((datatype, stride, 0x12340) if name in ("glNormalPointer", "glPointSizePointerOES")
                            else (default_size, datatype, stride, 0x12340))
                    arrays += [(INDEX[name], tuple(v & 0xFFFFFFFF for v in args) + (0,) * (8 - len(args))),
                               command("glGetPointerv", pointer_token),
                               command("glEnableClientState", cap), command("glIsEnabled", cap),
                               command("glDisableClientState", cap), command("glGetBooleanv", cap),
                               command("glGetError")]
        arrays += [command("glEnableClientState", 0), command("glGetPointerv", 0),
                   command("glGetPointerv", 0x808E, null=True), command("glGetError")]
    capabilities = []
    # Texture enable needs live texture ownership; scissor/dither need a surface.
    # These capabilities operate only on this fixture's core and getter state.
    for cap in (0xB10, 0xB20, 0xB44, 0xB50, 0xB57, 0xB60, 0xB71, 0xB90,
                0xBA1, 0xBC0, 0xBE2, 0xBF2, *range(0x3000, 0x3006),
                *range(0x4000, 0x4008), 0x8037, 0x803A, 0x809D, 0x809E,
                0x809F, 0x80A0, 0x8840, 0x8861, 0):
        # The supplied ARM image has an import ordinal at getter-table
        # 0x35444 in place of LIGHT7/POLYGON_OFFSET_FILL keys. Enabling either
        # dereferences a missing getter record; keep those cases on x86 only.
        if arm and cap in (0x4007, 0x8037):
            continue
        for name in ("glEnable", "glEnable", "glDisable", "glDisable"):
            capabilities += [command(name, cap), command("glIsEnabled", cap),
                             command("glGetBooleanv", cap), command("glGetError")]
    raster = []
    front_modes = [0x900, 0x901, 0x902, 0xFFFFFFFF]
    # ARM accepts byte aliases that the x86 binary rejects. Exercise those
    # inputs only against the reference selected when compiling the source.
    if arm == reference_arm:
        front_modes += [0, 1, 0x1900, 0x1901, 0xFFFFF900, 0xFFFFF901]
    for front in front_modes:
        raster += [command("glFrontFace", front), command("glGetIntegerv", 0xB46), command("glGetError")]
        for face in (0x404, 0x405, 0x408, 0):
            raster += [command("glCullFace", face), command("glGetIntegerv", 0xB45), command("glGetError")]
    for name, token in (("glViewport", 0xBA2), ("glScissor", 0xC10)):
        for x, y in ((0, 0), (-10, 25), (31, -19),
                     (0x7FFFFFFF, -0x80000000), (-0x80000000, 0x7FFFFFFF)):
            for width, height in ((0, 0), (96, 64), (2048, 2048), (4096, 8192),
                                  (0x7FFFFFFF, 0x7FFFFFFF), (-1, 32), (32, -1)):
                inputs = tuple(v & 0xFFFFFFFF for v in (x, y, width, height)) + (0,) * 4
                raster += [(INDEX[name], inputs), command("glGetIntegerv", token), command("glGetError")]
    return [fog, materials, lights, environment, invalid, arrays, capabilities, raster]


def run(suite):
    oracle, lib = suite.oracle, suite.lib
    arm = suite.arch == "arm"
    ranges = [(n, a, b) if arm else (n, c, d) for n, a, b, c, d in OPERATIONS]
    for entry in ranges:
        suite.register(*entry)
    helpers = (
        ("oglFogInternal", 0xEFE8, 0xF220, 0x414B30, 0x414E87),
        ("oglMaterialInternal", 0x11598, 0x11798, 0x417960, 0x417B1B),
        ("oglLightInternal", 0x11950, 0x11D18, 0x417CC0, 0x4180EE),
        ("oglLightModelInternal", 0x11E80, 0x11FA4, 0x418250, 0x418413),
        ("oglTexEnvInternal", 0x120A4, 0x12648, 0x418560, 0x418A91),
        ("oglGetLightInternal", 0xE538, 0xE810, 0x413E30, 0x4141DC),
        ("oglGetMaterialInternal", 0xE3A0, 0xE4B0, 0x413BD0, 0x413DB0),
        ("oglGetTexEnvInternal", 0xDE40, 0xE274, 0x413740, 0x413B0B),
        ("oglPointerInternal", 0x11168, 0x11358, 0x417220, 0x417788),
    )
    for n, a, b, c, d in helpers:
        suite.register(n, a if arm else c, b if arm else d)
    lib.diff_gl_state_step.argtypes = [C.c_void_p, C.c_int, C.c_void_p, C.c_void_p]
    lib.diff_gl_state_step.restype = C.c_uint32
    lib.diff_gl_state_snapshot.argtypes = [C.c_void_p, C.c_void_p]
    lib.diff_gl_state_snapshot.restype = None
    lib.diff_gl_state_surface.argtypes = [C.c_void_p, C.c_int, C.c_int]
    lib.diff_gl_state_surface.restype = None
    # Matrix run configures the common session functions; keep this module also
    # usable independently by configuring them here.
    lib.diff_gl_matrix_create.argtypes = [C.c_uint, C.c_void_p]
    lib.diff_gl_matrix_create.restype = C.c_void_p
    lib.diff_gl_matrix_destroy.argtypes = [C.c_void_p]
    lib.diff_gl_matrix_snapshot.argtypes = [C.c_void_p, C.c_void_p]
    oracle.reset()
    source = oracle.alloc(64, words([0x3F800000 if i % 5 == 0 else 0 for i in range(16)]))
    destination = oracle.alloc(80)
    oracle.call(0x2D87C if arm else 0x4616D0, [destination, source])
    identity = oracle.read(destination, 80)
    defaults = oracle.read(0x350F4 if arm else 0x471A74, GETTER_BYTES)
    for case, sequence in enumerate(seeds(arm, suite.arm_reference)):
        oracle.reset()
        core = oracle.alloc(0x3000)
        oracle.write(core + (0x1CE0 if arm else 0x1CE8), words([96, 64]))
        context = bytearray(CONTEXT_BYTES)
        struct.pack_into("<I", context, 0, core)
        struct.pack_into("<ii", context, 108, 96, 64)
        context[GETTER_OFFSET:GETTER_OFFSET + GETTER_BYTES] = defaults
        stacks = []
        for i in range(STACK_COUNT):
            stack = oracle.alloc(STACK_BYTES, bytes((0, STACK_DEPTH, 0, 0xA5)) + identity + bytes([0xA5]) * (80 * (STACK_DEPTH - 1)))
            stacks.append(stack)
            struct.pack_into("<I", context, 24 + 4 * i, stack)
            oracle.watch(stack, STACK_BYTES, f"state.stack[{i}]")
        ctx, arguments, output = oracle.alloc(CONTEXT_BYTES, bytes(context)), oracle.alloc(32), oracle.alloc(64)
        oracle.watch(ctx, CONTEXT_BYTES, "state.context")
        oracle.watch(core, 0x3000, "state.core")
        oracle.watch(output, 64, "state.output-with-canaries")
        hook = oracle.add_hook(0xB59C if arm else 0x4085B0, lambda m: m.return_from_call(ctx))
        surface_hooks = [oracle.add_hook(address, lambda m: m.return_from_call(0))
                         for address in ((0xB074, 0xAEF8) if arm else (0x406890, 0x4069B0))]
        session = lib.diff_gl_matrix_create(0xA5, C.create_string_buffer(identity, 80))
        if not session:
            raise ValueError("state fixture allocation failed")
        lib.diff_gl_state_surface(session, 96, 64)
        native_snapshot, native_core = C.create_string_buffer(SNAPSHOT_BYTES), C.create_string_buffer(0x3000)
        try:
            for step, (operation, inputs) in enumerate(sequence):
                name, address, _ = ranges[operation]
                data = words(inputs)
                oracle.write(arguments, data)
                oracle.write(output, bytes([0xA5]) * 64)
                native_output = C.create_string_buffer(bytes([0xA5]) * 64, 64)
                if operation in (38, 39, 41, 42, 43, 51, 52):
                    args = inputs[:4]
                elif operation in (40, 44):
                    args = inputs[:3]
                elif operation == 45:
                    args = [inputs[0], 0 if inputs[7] else output + 16]
                elif operation >= 35:
                    args = inputs[:1]
                elif operation == 34:
                    args = []
                elif operation == 33:
                    args = inputs[:1]
                elif operation >= 22:
                    args = list(inputs[:1 if operation >= 29 else 2]) + [0 if inputs[7] else output + 16]
                else:
                    prefix = inputs[:1 if operation < 4 or 12 <= operation < 16 else 2]
                    value = (0 if inputs[7] else arguments + 8) if name.endswith("v") else inputs[2]
                    args = [*prefix, value]
                result = oracle.call(address, args)
                if operation == 48:
                    result &= 0xFF  # GLboolean return: x86 defines AL, not upper EAX.
                native_result = lib.diff_gl_state_step(session, operation, C.create_string_buffer(data, 32), C.byref(native_output, 16))
                raw = oracle.read(ctx, CONTEXT_BYTES)
                pointer_changes = int(struct.unpack_from("<I", raw, 0)[0] != core)
                for i, stack in enumerate(stacks):
                    pointer_changes |= int(struct.unpack_from("<I", raw, 24 + 4 * i)[0] != stack) << (i + 1)
                pointer_changes |= int(any(struct.unpack_from("<I", raw, o)[0] for o in (8, 12, 16, 20, 76, 80, 84, 116, 120))) << 14
                original = words([pointer_changes]) + oracle.readback(ctx + 4, 4)
                original += oracle.readback(ctx + 88, 28) + oracle.readback(ctx + 124, 944)
                original += b"".join(oracle.readback(p, STACK_BYTES) for p in stacks)
                lib.diff_gl_matrix_snapshot(session, native_snapshot)
                lib.diff_gl_state_snapshot(session, native_core)
                core_parts = []
                for offset, count in CORE_RANGES:
                    part = oracle.readback(core + offset + (0 if arm else 8), count)
                    core_parts.append(part)
                expected_core = b"".join(core_parts)
                actual_core = b"".join(native_core.raw[o:o + n] for o, n in CORE_RANGES)
                suite.compare_bytes(name, {"state_sequence": case, "step": step, "inputs": inputs},
                                    words([native_result]) + native_snapshot.raw + actual_core + native_output.raw,
                                    words([result if operation in (34, 48) else 0]) + original + expected_core + oracle.readback(output, 64))
        finally:
            lib.diff_gl_matrix_destroy(session)
            oracle.remove_hook(hook)
            for surface_hook in surface_hooks:
                oracle.remove_hook(surface_hook)
