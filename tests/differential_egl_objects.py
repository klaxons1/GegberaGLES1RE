"""EGL object queries and deferred destruction with complete normalized state."""
import ctypes as C
import struct


NAMES = ("eglQuerySurface", "eglSurfaceAttrib", "eglQueryContext", "eglDestroySurface",
         "eglDestroyContext", "eglGetCurrentContext", "eglGetCurrentSurface", "eglGetCurrentDisplay")
RANGES = {
    "arm": ((0xA3C8, 0xA52C), (0xA30C, 0xA3A8), (0xB3E8, 0xB47C), (0xA288, 0xA30C),
            (0xA6A4, 0xA79C), (0xB310, 0xB34C), (0xB34C, 0xB3AC), (0xB3AC, 0xB3E8)),
    "x86": ((0x4049B0, 0x404B8C), (0x404890, 0x40495A), (0x405F90, 0x406062),
            (0x4047E0, 0x40488B), (0x404F20, 0x405174), (0x405E50, 0x405EA4),
            (0x405EB0, 0x405F3F), (0x405F40, 0x405F90)),
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
    state = 0
    hook = oracle.add_hook(0x8088 if suite.arch == "arm" else 0x406A50,
                           lambda machine: machine.return_from_call(state))

    def case(settings, commands):
        nonlocal state
        oracle.reset()
        state = oracle.alloc(40)
        display = oracle.alloc(20)
        surfaces = [oracle.alloc(80) for _ in range(3)]
        contexts = [oracle.alloc(32) for _ in range(3)]
        width, height, kind, fmt, target, level, largest, mipmap, dead_s, dead_c, mask = settings
        oracle.write(state, words((0, contexts[1] if mask & 1 else 0,
                                  surfaces[0] if mask & 2 else 0, surfaces[2] if mask & 4 else 0,
                                  display if mask & 8 else 0, display, 0x3002, 7, 0, 0)))
        oracle.write(display, words((0, surfaces[0], contexts[0], 3, 3)))
        for i, pointer in enumerate(surfaces):
            oracle.write(pointer, words((2 | ((dead_s if i == 1 else 0) << 16),
                surfaces[i - 1] if i else 0, surfaces[i + 1] if i < 2 else 0, display, 0, 0, 0,
                1 + i * 3, 2 * width, width, height, 11 + i, kind, largest, mipmap,
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
        try:
            for op, arguments in commands:
                args = list(arguments) + [0] * (4 - len(arguments))
                original_args = args.copy()
                if op in (0, 2):
                    original_args[3] = output_address + 4 if args[3] else 0
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

    baseline = [37, 19, 1, EGL_TEXTURE_RGBA, EGL_TEXTURE_2D, 3, 0, 1, 0, 0, 15]
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
        for mask in range(16):
            settings = baseline.copy()
            settings[10] = mask
            case(settings, [(5, ()), (7, ())] + [(6, (token,)) for token in (0x3059, 0x305A, 0xDEAD)])
        case(baseline, [(3, (1, handle)) for handle in (12, 12, 11, 13)]
             + [(0, (1, handle, EGL_WIDTH, 1)) for handle in (11, 12, 13)]
             + [(4, (1, handle)) for handle in (22, 22, 21, 23)]
             + [(2, (1, handle, EGL_CONFIG_ID, 1)) for handle in (21, 22, 23)])
    finally:
        oracle.remove_hook(hook)
    from differential_egl_pbuffer import run as pbuffer_cases
    pbuffer_cases(suite)
