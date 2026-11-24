"""EGL object queries and deferred destruction with complete normalized state."""
import ctypes as C
import struct


NAMES = ("eglQuerySurface", "eglSurfaceAttrib", "eglQueryContext", "eglDestroySurface",
         "eglDestroyContext", "eglGetCurrentContext", "eglGetCurrentSurface", "eglGetCurrentDisplay",
         "eglWaitGL", "eglWaitNative", "eglSwapInterval", "eglSwapBuffers", "eglCopyBuffers", "eglGetError")
RANGES = {
    "arm": ((0xA3C8, 0xA52C), (0xA30C, 0xA3A8), (0xB3E8, 0xB47C), (0xA288, 0xA30C),
            (0xA6A4, 0xA79C), (0xB310, 0xB34C), (0xB34C, 0xB3AC), (0xB3AC, 0xB3E8),
            (0xAA50, 0xAA80), (0xAA80, 0xAAC0), (0xAE40, 0xAEAC), (0xADB0, 0xAE40),
            (0xB228, 0xB310), (0xB53C, 0xB590)),
    "x86": ((0x4049B0, 0x404B8C), (0x404890, 0x40495A), (0x405F90, 0x406062),
            (0x4047E0, 0x40488B), (0x404F20, 0x405174), (0x405E50, 0x405EA4),
            (0x405EB0, 0x405F3F), (0x405F40, 0x405F90), (0x405830, 0x405865),
            (0x405870, 0x4058C7), (0x405C60, 0x405D09), (0x405BA0, 0x405C54),
            (0x405D10, 0x405E45), (0x4081A0, 0x4082B3)),
}
EGL_CONFIG_ID, EGL_HEIGHT, EGL_WIDTH = 0x3028, 0x3056, 0x3057
EGL_LARGEST_PBUFFER, EGL_NO_TEXTURE = 0x3058, 0x305C
EGL_TEXTURE_RGB, EGL_TEXTURE_RGBA, EGL_TEXTURE_2D = 0x305D, 0x305E, 0x305F
EGL_TEXTURE_FORMAT, EGL_TEXTURE_TARGET, EGL_MIPMAP_TEXTURE, EGL_MIPMAP_LEVEL = range(0x3080, 0x3084)
ATTRIBUTES = (EGL_CONFIG_ID, EGL_HEIGHT, EGL_WIDTH, EGL_LARGEST_PBUFFER,
              EGL_TEXTURE_FORMAT, EGL_TEXTURE_TARGET, EGL_MIPMAP_TEXTURE, EGL_MIPMAP_LEVEL, 0xDEAD)
STATE_WORDS = 99


def words(values):
    return struct.pack("<" + "I" * len(values), *(x & 0xFFFFFFFF for x in values))


def run(suite):
    lib, oracle = suite.lib, suite.oracle
    ranges = RANGES[suite.arch]
    for name, (start, end) in zip(NAMES, ranges):
        suite.register(name, start, end)
    lib.differential_egl_objects_create.argtypes = [C.c_void_p]
    lib.differential_egl_objects_create.restype = C.c_void_p
    lib.differential_egl_objects_step.argtypes = [C.c_void_p, C.c_int, C.c_void_p, C.c_void_p]
    lib.differential_egl_objects_step.restype = C.c_int
    lib.differential_egl_objects_snapshot.argtypes = [C.c_void_p, C.c_void_p]
    lib.differential_egl_objects_snapshot.restype = None
    lib.differential_egl_objects_destroy.argtypes = [C.c_void_p]
    lib.differential_egl_objects_destroy.restype = None
    tls = 0
    pixmap_kind = 0

    def set_global(machine):
        nonlocal tls
        tls, = machine.arguments(1)
        machine.return_from_call()

    def pixmap_size(machine):
        pointer, width, height = machine.arguments(3)
        machine.write(width, machine.read(pointer, 4))
        machine.write(height, machine.read(pointer + 4, 4))
        machine.return_from_call()

    def blit(machine):
        # The API fixture has no pixel storage. OS bitmap copying is covered
        # separately; preserve its format-mismatch result and error here.
        if pixmap_kind == 6:
            machine.write(tls + 24, words((0x3009,)))
        machine.return_from_call(int(pixmap_kind != 6))

    hooks = [oracle.add_hook(0x8084 if suite.arch == "arm" else 0x406DF0,
                            lambda machine: machine.return_from_call(tls)),
             oracle.add_hook(0x90C4 if suite.arch == "arm" else 0x406DE0, set_global),
             oracle.add_hook(0x8AB4 if suite.arch == "arm" else 0x4076D0, pixmap_size),
             oracle.add_hook(0xB120 if suite.arch == "arm" else 0x407890, blit)]

    def case(settings, commands):
        nonlocal tls, pixmap_kind
        oracle.reset()
        state = oracle.alloc(40)
        display = oracle.alloc(20)
        surfaces = [oracle.alloc(80) for _ in range(3)]
        contexts = [oracle.alloc(32) for _ in range(3)]
        width, height, kind, fmt, target, level, largest, mipmap, dead_s, dead_c, mask, config = settings
        tls = 0 if mask & 16 else state
        oracle.write(state, words((0, contexts[1] if mask & 1 else 0,
                                  surfaces[0] if mask & 2 else 0, surfaces[2] if mask & 4 else 0,
                                  display if mask & 8 else 0, display, 0x3002, 7, 0, 0)))
        oracle.write(display, words((0, surfaces[0], contexts[0], 3, 3)))
        for i, pointer in enumerate(surfaces):
            oracle.write(pointer, words((2 | ((dead_s if i == 1 else 0) << 16),
                surfaces[i - 1] if i else 0, surfaces[i + 1] if i < 2 else 0, display, 0, 0, 0,
                config + i * 3, 2 * width, width, height, 11 + i, kind, largest, mipmap,
                fmt, target, level, 0, 0)))
        for i, pointer in enumerate(contexts):
            oracle.write(pointer, words((1 | ((dead_c if i == 1 else 0) << 16), display,
                contexts[i - 1] if i else 0, contexts[i + 1] if i < 2 else 0, 0,
                1 + i * 3, 21 + i, i & 1)))
        allocations = [(state, 40, (0, 1, 2, 3, 4, 5, 8)), (display, 20, (1, 2))]
        allocations += [(p, 80, (1, 2, 3, 4, 5, 6, 18)) for p in surfaces]
        allocations += [(p, 32, (1, 2, 3, 4)) for p in contexts]
        for pointer, length, _ in allocations:
            oracle.watch(pointer, length, "EGL object")
        identities = {0: 0, display: 1, **{p: 100 + i for i, p in enumerate(surfaces)},
                      **{p: 200 + i for i, p in enumerate(contexts)}}
        native_settings = (C.c_int * len(settings))(*settings)
        fixture = lib.differential_egl_objects_create(native_settings)
        if not fixture:
            raise RuntimeError("EGL object fixture allocation failed")
        output_address = oracle.alloc(12, b"\xA5" * 12)
        oracle.watch(output_address, 12, "query output and canaries")
        pixmap = oracle.alloc(8)
        try:
            for op, arguments in commands:
                args = list(arguments) + [0] * (4 - len(arguments))
                original_args = args.copy()
                if op in (0, 2):
                    original_args[3] = output_address + 4 if args[3] else 0
                if op == 12:
                    pixmap_kind = args[2]
                    original_args[2] = pixmap if pixmap_kind else 0
                    oracle.write(pixmap, words((width + (pixmap_kind == 3) - (pixmap_kind == 2),
                                               height + (pixmap_kind == 5) - (pixmap_kind == 4))))
                oracle.write(output_address, b"\xA5" * 12)
                original_result = oracle.call(ranges[op][0], original_args)
                expected = []
                for pointer, length, pointers in allocations:
                    data = list(struct.unpack("<" + "I" * (length // 4), oracle.readback(pointer, length)))
                    for index in pointers:
                        data[index] = identities.get(data[index], 0xFFFFFFFF)
                    expected += data
                native_output = C.create_string_buffer(b"\xA5" * 12, 12)
                native_result = lib.differential_egl_objects_step(
                    fixture, op, (C.c_int * 4)(*args), C.byref(native_output, 4))
                snapshot = (C.c_uint * STATE_WORDS)()
                lib.differential_egl_objects_snapshot(fixture, snapshot)
                suite.compare_bytes(NAMES[op], (settings, args),
                    words((native_result,)) + bytes(snapshot) + native_output.raw,
                    words((original_result,)) + words(expected) + oracle.readback(output_address, 12))
        finally:
            lib.differential_egl_objects_destroy(fixture)

    baseline = [37, 19, 1, EGL_TEXTURE_RGBA, EGL_TEXTURE_2D, 3, 0, 1, 0, 0, 15, 1]
    try:
        for kind in range(3):
            for fmt, target in ((EGL_NO_TEXTURE, EGL_NO_TEXTURE), (EGL_TEXTURE_RGB, EGL_TEXTURE_2D),
                                (EGL_TEXTURE_RGBA, EGL_NO_TEXTURE), (EGL_NO_TEXTURE, EGL_TEXTURE_2D)):
                settings = baseline.copy()
                settings[2:5] = kind, fmt, target
                if kind != 1:
                    settings[6:8] = 0, 0
                case(settings, [(0, (1, 12, attribute, 1)) for attribute in ATTRIBUTES])
                case(settings, [(1, (1, 12, EGL_MIPMAP_LEVEL, value)) for value in (-1, 0, 5, 11, 12, 99)]
                     + [(0, (1, 12, EGL_MIPMAP_LEVEL, 1))])
        for display in (0, 1, 2):
            for handle in (0, 11, 12, 13, 99):
                for dead in (0, 1):
                    settings = baseline.copy()
                    settings[8] = settings[9] = dead
                    commands = [(0, (display, handle, attribute, output))
                                for attribute in (EGL_WIDTH, 0xDEAD) for output in (0, 1)]
                    commands += [(2, (display, handle + 10, attribute, output))
                                 for attribute in (EGL_CONFIG_ID, 0xDEAD) for output in (0, 1)]
                    # Unknown attributes are defined even with an invalid handle.
                    # The original dereferences a null surface for MIPMAP_LEVEL;
                    # that undefined path is deliberately outside the oracle run.
                    commands += [(1, (display, handle, 0xDEAD, 1))]
                    case(settings, commands)
        for mask in range(17):
            settings = baseline.copy()
            settings[10] = mask
            commands = [(5, ()), (7, ()), (8, ())]
            commands += [(6, (token,)) for token in (0x3059, 0x305A, 0xDEAD)]
            commands += [(9, (engine,)) for engine in (0x305B, -1, 0)]
            commands += [(10, (display, interval)) for display in (0, 1, 2)
                         for interval in (-1, 0, 1, 2, 0x7FFFFFFF)]
            commands += [(11, (display, handle)) for display in (0, 1, 2)
                         for handle in (0, 11, 12, 13, 99)]
            # Read every error twice, including preinitialization TLS tags.
            case(settings, [command for call in commands for command in (call, (13, ()), (13, ()))])
            settings[11] = 19  # RGB565, matching the fixture's native pixmap.
            commands = [(12, (display, handle, pixmap)) for display in (0, 1, 2)
                        for handle in (0, 11, 12, 13, 99) for pixmap in range(7)]
            case(settings, [command for call in commands for command in (call, (13, ()))])
        settings = baseline.copy()
        settings[11] = 19
        # Referenced destroyed surfaces remain usable through their handles.
        case(settings, [(3, (1, 11)), (12, (1, 11, 1)), (13, ()),
                        (3, (1, 13)), (11, (1, 13)), (12, (1, 13, 1)), (13, ())])
        case(baseline, [(3, (1, handle)) for handle in (12, 12, 11, 13)]
             + [(0, (1, handle, EGL_WIDTH, 1)) for handle in (11, 12, 13)]
             + [(4, (1, handle)) for handle in (22, 22, 21, 23)]
             + [(2, (1, handle, EGL_CONFIG_ID, 1)) for handle in (21, 22, 23)])

        lifecycle = ((0x9370, 0x9404), (0x917C, 0x9224)) if suite.arch == "arm" else (
                     (0x4032D0, 0x403399), (0x405180, 0x405254))
        for name, limits in zip(("eglInitialize", "eglTerminate"), lifecycle):
            suite.register(name, *limits)
        lib.differential_egl_lifecycle.argtypes = [C.c_int, C.c_int, C.c_void_p, C.c_void_p]
        lib.differential_egl_lifecycle.restype = None

        def initialize_state(machine):
            nonlocal tls
            display = machine.alloc(20)
            tls = machine.alloc(40, words((0, 0, 0, 0, 0, display, 0, 7, 0, 0)))
            machine.return_from_call(tls)

        def destroy_state(machine):
            nonlocal tls
            tls = 0
            machine.return_from_call()

        # State creation/destruction crosses the OS allocator and environment
        # boundary. Public lifecycle code and error handling execute unchanged.
        init_hook = oracle.add_hook(0x9224 if suite.arch == "arm" else 0x406A80, initialize_state)
        destroy_hook = oracle.add_hook(0x911C if suite.arch == "arm" else 0x406E00, destroy_state)
        try:
            for tag in (0, 1, 2):
                for display_id in (0, 1, 2, -1):
                    commands = [(1, display_id), (0, display_id), (2, 0), (0, 1),
                                (1, display_id), (2, 0), (1, 1), (1, display_id), (2, 0)]
                    oracle.reset()
                    tls = tag
                    values = oracle.alloc(16)
                    oracle.watch(values, 16, "version outputs and canaries")
                    expected = []
                    for operation, display in commands:
                        oracle.write(values, words((-777,) * 4))
                        entry = lifecycle[operation][0] if operation < 2 else ranges[5][0]
                        result = oracle.call(entry, [display, values + 4, values + 8])
                        error = oracle.call(ranges[13][0], [])
                        versions = struct.unpack("<4I", oracle.readback(values, 16))
                        if versions[0] != (-777 & 0xFFFFFFFF) or versions[3] != versions[0]:
                            raise RuntimeError("original EGL lifecycle wrote a version canary")
                        expected += [result, error, int(tls > 3), versions[1], versions[2]]
                    native = (C.c_uint * len(expected))()
                    flat_commands = [value for command in commands for value in command]
                    lib.differential_egl_lifecycle(tag, len(commands),
                        (C.c_int * len(flat_commands))(*flat_commands), native)
                    for index, (operation, display) in enumerate(commands):
                        name = ("eglInitialize", "eglTerminate", "eglGetCurrentContext")[operation]
                        start = index * 5
                        suite.compare(name, (tag, commands[:index + 1]),
                                      tuple(native[start:start + 5]), tuple(expected[start:start + 5]))
        finally:
            oracle.remove_hook(init_hook)
            oracle.remove_hook(destroy_hook)
    finally:
        for hook in hooks:
            oracle.remove_hook(hook)
    from differential_egl_pbuffer import run as pbuffer_cases
    pbuffer_cases(suite)
