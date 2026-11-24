"""Exhaustive config identities/orderings and bounded EGL helper comparisons."""
import ctypes as C
import struct


FUNCTIONS = (
    ("eglInitRefCount", 0x4012D0, 0x40133A),
    ("eglAddRef", 0x405710, 0x40582B),
    ("eglDestroyRef", 0x401350, 0x401413),
    ("eglReleaseRef", 0x401420, 0x4014EE),
    ("eglGetConfigIndices", 0x401BC0, 0x401E97),
    ("eglFillConfig", 0x401980, 0x401BB2),
    ("eglCompareConfigs", 0x401F50, 0x402268),
    ("eglFilterConfig", 0x4023A0, 0x4026D2),
    ("eglGetConfigAttributeOffset", 0x402270, 0x402394),
    ("eglIsValidConfigID", 0x402720, 0x402770),
)
ADDRESSES = {name: start for name, start, _ in FUNCTIONS}


def packed(values):
    return struct.pack("<" + "i" * len(values), *values)


def run(suite):
    if suite.arch == "arm":
        from differential_egl_platform import run as platform_cases
        platform_cases(suite)
        from differential_egl_objects import run as object_cases
        object_cases(suite)
        return
    # Supply only the TLS lookup; all config and native-format code executes.
    hook = suite.oracle.add_hook(0x406A50, lambda emu: emu.return_from_call(0))
    try:
        _run(suite)
    finally:
        suite.oracle.remove_hook(hook)
    from differential_egl_platform import run as platform_cases
    platform_cases(suite)
    from differential_egl_objects import run as object_cases
    object_cases(suite)


def _run(suite):
    lib, oracle = suite.lib, suite.oracle
    for name, start, end in FUNCTIONS:
        suite.register(name, start, end)
    for name in ("eglGetConfigIndices", "eglFillConfig"):
        getattr(lib, name).argtypes = [C.c_void_p, C.c_int]
        getattr(lib, name).restype = None
    for name in ("eglGetConfigAttributeOffset", "eglIsValidConfigID"):
        getattr(lib, name).argtypes = [C.c_int]
        getattr(lib, name).restype = C.c_int
    for name in ("eglInitRefCount", "eglAddRef", "eglDestroyRef", "eglReleaseRef"):
        getattr(lib, name).argtypes = [C.c_void_p]
        getattr(lib, name).restype = None if name in ("eglInitRefCount", "eglAddRef") else C.c_int
    lib.eglCompareConfigs.argtypes = [C.c_void_p] * 3
    lib.eglCompareConfigs.restype = C.c_int
    lib.eglFilterConfig.argtypes = [C.c_void_p] * 2
    lib.eglFilterConfig.restype = C.c_int

    configs = []
    for config_id in range(1, 73):
        for name, length in (("eglGetConfigIndices", 16), ("eglFillConfig", 96)):
            oracle.reset()
            pointer = oracle.alloc(length, b"\xA5" * length)
            oracle.call(ADDRESSES[name], [pointer, config_id])
            original = oracle.read(pointer, length)
            host = C.create_string_buffer(b"\xA5" * length, length)
            getattr(lib, name)(host, config_id)
            suite.compare(name, config_id, host.raw, original)
            if name == "eglFillConfig":
                configs.append(list(struct.unpack("<24i", original)))

    for token in list(range(0x3010, 0x3041)) + [0, -1, 0x10000]:
        # The recovered helper returns byte offsets; its callers use byte
        # pointers. The original returns indices into the same int array.
        offset = lib.eglGetConfigAttributeOffset(token)
        suite.check("eglGetConfigAttributeOffset", ADDRESSES["eglGetConfigAttributeOffset"],
                    [token], (offset // 4 if offset >= 0 else offset) & 0xFFFFFFFF)
    for value in (-0x80000000, -1, 0, 1, 24, 72, 73, 0x7FFFFFFF):
        suite.check("eglIsValidConfigID", ADDRESSES["eglIsValidConfigID"],
                    [value], lib.eglIsValidConfigID(value))

    for name in ("eglInitRefCount", "eglAddRef", "eglDestroyRef", "eglReleaseRef"):
        for count in ((0, 1, 100, 32766) if name != "eglReleaseRef" else (1, 2, 100)):
            for destroyed in (0, 1):
                if name == "eglAddRef" and count == 0 and destroyed:
                    continue  # The original rejects resurrecting a dead object.
                data = struct.pack("<hBB", count, destroyed, 0xA5)
                oracle.reset()
                pointer = oracle.alloc(4, data)
                result = oracle.call(ADDRESSES[name], [pointer])
                host = C.create_string_buffer(data, 4)
                native_result = getattr(lib, name)(host)
                if name in ("eglInitRefCount", "eglAddRef"):
                    native_result = result = 0  # These recovered APIs return void.
                suite.compare(name, (count, destroyed), (native_result, host.raw),
                              (result, oracle.read(pointer, 4)))

    default_filter = list(struct.unpack("<24i", oracle.read(0x4701C0, 96)))

    def compare(first, second, filter_values):
        oracle.reset()
        data = [packed(first), packed(second)]
        pointers = [oracle.alloc(96, item) for item in data]
        host = [C.create_string_buffer(item, 96) for item in data]
        if filter_values is None:
            pointers.append(0)
            host.append(None)
        else:
            data.append(packed(filter_values))
            pointers.append(oracle.alloc(96, data[2]))
            host.append(C.create_string_buffer(data[2], 96))
        original = oracle.call(ADDRESSES["eglCompareConfigs"], pointers)
        native = lib.eglCompareConfigs(*host) & 0xFFFFFFFF
        suite.compare("eglCompareConfigs", (first, second, filter_values), native, original)

    for first in configs:
        for second in configs:
            compare(first, second, default_filter)
    for caveat1 in (0x3038, 0x3050, 0x3051):
        for caveat2 in (0x3038, 0x3050, 0x3051):
            first, second = configs[0].copy(), configs[0].copy()
            first[5], second[5] = caveat1, caveat2
            compare(first, second, None)
    # Each tie-break field and every channel-selection mask, including a null
    # filter, are tested independently of the finite built-in config table.
    for field in (0, 1, 2, 3, 4, 6, 7, 10, 12, 13, 14):
        first, second = configs[0].copy(), configs[0].copy()
        second[field] += 1
        for mask in range(16):
            filter_values = default_filter.copy()
            for channel in range(1, 5):
                filter_values[channel] = 1 if mask & (1 << (channel - 1)) else 0
            compare(first, second, filter_values)
            compare(second, first, filter_values)
        compare(first, second, None)

    def filter_case(config, filter_values):
        oracle.reset()
        data = [packed(config), packed(filter_values)]
        pointers = [oracle.alloc(96, item) for item in data]
        host = [C.create_string_buffer(item, 96) for item in data]
        original = oracle.call(ADDRESSES["eglFilterConfig"], pointers)
        suite.compare("eglFilterConfig", (config, filter_values),
                      lib.eglFilterConfig(*host), original)

    for config in configs:
        filter_case(config, default_filter)
    for field in range(24):
        for value in (-1, 0, 1, 2, 0x3038, 0x3052):
            config = configs[0].copy()
            filter_values = config.copy()
            filter_values[6] = -1
            filter_values[field] = value
            filter_case(config, filter_values)
    for config_id in (0, -1, 73):
        config = configs[0].copy()
        config[6] = config_id
        filter_case(config, config)
    config = configs[0].copy()
    config[16] = 0x3052
    for channel in (17, 18, 19):
        filter_values = config.copy()
        filter_values[6] = -1
        filter_values[channel] = 1
        filter_case(config, filter_values)
