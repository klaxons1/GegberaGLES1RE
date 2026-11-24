"""Exhaust EGL configurations with a live native display-mode fixture."""
import ctypes
from enum import IntEnum
from functools import cmp_to_key
import struct


class DisplayMode(IntEnum):
    UNSUPPORTED = 0
    ECOLOR64K = 7
    ECOLOR4K = 10
    ECOLOR16MU = 11
    ECOLOR16MA = 12


CONFIG_COUNT = 72
CONFIG_WORDS = 24
CONFIG_BYTES = CONFIG_WORDS * 4
ORIGINAL_STATE_BYTES = 40
TARGETS = {
    "x86": {
        "eglFillConfig": (0x401980, 0x401BB2),
        "egliFixConfig": (0x407E90, 0x40803C),
        "eglGetConfigs": (0x403440, 0x403525),
        "eglGetConfigAttrib": (0x404B90, 0x404CBB),
        "eglChooseConfig": (0x403540, 0x403DB2),
        "eglCompareConfigs": (0x401F50, 0x402268),
    },
    "arm": {
        "eglFillConfig": (0x84B0, 0x85A8),
        "egliFixConfig": (0x83AC, 0x84B0),
        "eglGetConfigs": (0x9484, 0x9558),
        "eglGetConfigAttrib": (0x9AEC, 0x9C28),
        "eglChooseConfig": (0x9604, 0x9938),
        # Shared tail of the cached comparator at 0x9558: r0/r1=configs, r2=filter.
        "eglCompareConfigs": (0x85A8, 0x8754),
    },
}
GET_STATE = {"x86": 0x406A50, "arm": 0x8088}
SORT_CONFIGS = {"x86": 0x408320, "arm": 0x9564}
CONFIG_ATTRIBUTES = tuple(attribute for attribute in range(0x3020, 0x303D)
                          if attribute not in (0x3030, 0x3038))


def _fix_configs(suite, reset, targets):
    oracle = suite.oracle
    adapter = suite.lib.differential_egl_fix_config
    adapter.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_void_p]
    adapter.restype = None
    colors = ((8, 8, 8, 8), (5, 6, 5, 0), (8, 8, 8, 0),
              (4, 4, 4, 0), (4, 4, 4, 4), (5, 5, 5, 1))
    cases = [(mode, rgba, surface, True) for mode in DisplayMode
             for rgba in colors for surface in range(8)]
    cases += [(DisplayMode.ECOLOR64K, colors[0], surface, False) for surface in range(8)]
    for mode, rgba, surface, initialized in cases:
        reset(mode, initialized)
        # Sentinel fields reveal any writes beyond nativeRenderable/surfaceType.
        values = [0x12345678] * CONFIG_WORDS
        values[1:5] = reversed(rgba)
        values[9], values[15] = surface & 1, surface
        data = struct.pack("<24i", *values)
        pointer = oracle.alloc(CONFIG_BYTES, data)
        oracle.watch(pointer, CONFIG_BYTES, "config output")
        oracle.call(targets["egliFixConfig"][0], [pointer])
        native = ctypes.create_string_buffer(data, CONFIG_BYTES)
        adapter(mode, initialized, native)
        suite.compare_bytes("egliFixConfig", {"mode": mode.name, "rgba": rgba,
                            "surface": surface, "initialized": initialized},
                            native.raw, oracle.readback(pointer, CONFIG_BYTES))


def _get_configs(suite, reset, targets):
    oracle = suite.oracle
    adapter = suite.lib.differential_egl_get_configs
    adapter.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int,
                        ctypes.c_void_p, ctypes.c_void_p, ctypes.POINTER(ctypes.c_int)]
    adapter.restype = ctypes.c_int
    cases = [(1, limit, configs, count)
             for limit in (-0x80000000, -1, 0, 1, 2, 5, 35, 71, 72, 73, 0x7FFFFFFF)
             for configs in (False, True) for count in (False, True)]
    cases += [(display, 72, configs, count) for display in (0, 2, -1)
              for configs in (False, True) for count in (False, True)]
    config_sentinel = b"\xA5" * (16 + CONFIG_COUNT * 4 + 16)
    count_sentinel = b"\xA5" * 12
    for mode in DisplayMode:
        for display, limit, has_configs, has_count in cases:
            state_pointer = reset(mode)
            configs_pointer = oracle.alloc(len(config_sentinel), config_sentinel)
            count_pointer = oracle.alloc(len(count_sentinel), count_sentinel)
            oracle.watch(configs_pointer, len(config_sentinel), "config output and canaries")
            oracle.watch(count_pointer, len(count_sentinel), "count output and canaries")
            result = oracle.call(targets["eglGetConfigs"][0], [display,
                                 configs_pointer + 16 if has_configs else 0, limit,
                                 count_pointer + 4 if has_count else 0])
            expected = (result, struct.unpack("<i", oracle.readback(state_pointer + 24, 4))[0],
                        oracle.readback(count_pointer, len(count_sentinel)),
                        oracle.readback(configs_pointer, len(config_sentinel)))
            native_configs = ctypes.create_string_buffer(config_sentinel, len(config_sentinel))
            native_count = ctypes.create_string_buffer(count_sentinel, len(count_sentinel))
            error = ctypes.c_int()
            result = adapter(mode, display, limit,
                             ctypes.byref(native_configs, 16) if has_configs else None,
                             ctypes.byref(native_count, 4) if has_count else None,
                             ctypes.byref(error))
            suite.compare("eglGetConfigs", {"display_mode": mode.name, "display": display,
                          "limit": limit, "configs": has_configs, "count": has_count},
                          (result, error.value, native_count.raw, native_configs.raw), expected)


def _get_config_attrib(suite, reset, targets):
    oracle = suite.oracle
    adapter = suite.lib.differential_egl_get_config_attrib
    adapter.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int,
                        ctypes.c_void_p, ctypes.POINTER(ctypes.c_int)]
    adapter.restype = ctypes.c_int
    cases = []
    for config_id in range(1, CONFIG_COUNT + 1):
        # Check surface-independent pbuffer limits on every config and rotate
        # the ordinary attributes; the full attribute set also runs on ID 1.
        attributes = (0x302A, 0x302B, 0x302C, 0x303B + (config_id & 1),
                      CONFIG_ATTRIBUTES[(config_id - 1) % len(CONFIG_ATTRIBUTES)])
        cases += [(1, config_id, attribute, True) for attribute in attributes]
    cases += [(1, 1, attribute, True) for attribute in CONFIG_ATTRIBUTES]
    cases += [(display, config_id, attribute, output)
              for display in (0, 2, -1) for config_id in (0, 1)
              for attribute in (0x3028, 0xDEAD) for output in (False, True)]
    cases += [(1, config_id, attribute, output) for config_id in (0, 73, -1)
              for attribute in (0x3028, 0xDEAD) for output in (False, True)]
    cases += [(1, 1, attribute, output) for attribute in (0x3028, 0x3030, 0x3038, 0xDEAD)
              for output in (False, True)]
    sentinel = b"\xA5" * 12
    for mode in DisplayMode:
        for display, config_id, attribute, has_output in cases:
            state_pointer = reset(mode)
            pointer = oracle.alloc(len(sentinel), sentinel)
            oracle.watch(pointer, len(sentinel), "attribute output and canaries")
            result = oracle.call(targets["eglGetConfigAttrib"][0], [display, config_id, attribute,
                                                     pointer + 4 if has_output else 0])
            expected = (result, struct.unpack("<i", oracle.readback(state_pointer + 24, 4))[0],
                        oracle.readback(pointer, len(sentinel)))
            output = ctypes.create_string_buffer(sentinel, len(sentinel))
            error = ctypes.c_int()
            result = adapter(mode, display, config_id, attribute,
                             ctypes.byref(output, 4) if has_output else None,
                             ctypes.byref(error))
            suite.compare("eglGetConfigAttrib", {"display_mode": mode.name, "display": display,
                          "config_id": config_id, "attribute": hex(attribute), "output": has_output},
                          (result, error.value, output.raw), expected)


def _choose_configs(suite, reset, targets):
    oracle = suite.oracle
    adapter = suite.lib.differential_egl_choose_configs
    adapter.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_void_p, ctypes.c_void_p,
                        ctypes.c_int, ctypes.c_void_p, ctypes.c_int, ctypes.POINTER(ctypes.c_int)]
    adapter.restype = ctypes.c_int
    filters = [None, (), (0x3033, 1), (0x3033, -1), (0x3024, 1),
               (0x3024, 8), (0x3028, 1), (0x3028, 72), (0x3028, 0),
               (0x3021, 8, 0x3033, 1)]
    cases = [(mode, 1, attributes, limit, True, True)
             for mode in DisplayMode for attributes in filters for limit in (-1, 0, 1, 3, 72)]
    cases += [(mode, display, attributes, limit, configs, count)
              for mode in DisplayMode for display in (0, 1, 2)
              for attributes in (None, (0xDEAD, 0)) for limit in (-1, 0, 72)
              for configs, count in ((False, True), (True, False))]
    cases += [(DisplayMode.ECOLOR64K, 1, (attribute, value), 72, True, True)
              for attribute in CONFIG_ATTRIBUTES + (0, 0x3030, 0xDEAD)
              for value in (-2, -1, 0, 1, 2, 8, 0x3038, 0x3050, 0x3051, 0x3052)]
    cases += [(DisplayMode.ECOLOR64K, 1, attributes, 72, True, True)
              for attributes in ((0x3024, 8, 0x3024, 0), (0x3024, -2, 0x3024, 0),
                                 (0x3028, 1, 0x3024, 99), (0x3028, 1, 0x3029, -2))]
    for _ in range(min(suite.samples, 64)):
        attributes = []
        for _ in range(suite.rng.randrange(1, 6)):
            attributes += [suite.rng.choice(CONFIG_ATTRIBUTES),
                           suite.rng.choice((-2, -1, 0, 1, 2, 4, 8, 16, 32, 0x3038))]
        cases.append((suite.rng.choice(tuple(DisplayMode)), 1, tuple(attributes),
                      suite.rng.choice((-1, 0, 1, 3, 72)), True, True))

    captured_sort = None

    def capture_sort(machine):
        nonlocal captured_sort
        pointer, count, filter_pointer = machine.arguments(3)
        captured_sort = (pointer, count, machine.read(filter_pointer, CONFIG_BYTES))
        machine.return_from_call()

    # The platform RArray allocator/sort is outside the DLL. Capture that
    # boundary, then order its selected IDs with the original DLL comparator.
    # Calls happen after ChooseConfig returns, so emulator execution is not nested.
    sort_hook = oracle.add_hook(SORT_CONFIGS[suite.arch], capture_sort)
    configs_sentinel = b"\xA5" * (16 + CONFIG_COUNT * 4 + 16)
    count_sentinel = b"\xA5" * 12
    try:
        for case_index, (mode, display, attributes, limit, has_configs, has_count) in enumerate(cases):
            captured_sort = None
            state_pointer = reset(mode)
            initial_error = (0x3000, 0x3002, 0x3004)[case_index % 3]
            oracle.write(state_pointer + 24, struct.pack("<i", initial_error))
            attribute_data = None if attributes is None else struct.pack(
                "<" + "i" * (len(attributes) + 1), *attributes, 0x3038)
            attribute_pointer = oracle.alloc(len(attribute_data), attribute_data) if attribute_data else 0
            configs_pointer = oracle.alloc(len(configs_sentinel), configs_sentinel)
            count_pointer = oracle.alloc(len(count_sentinel), count_sentinel)
            if attribute_pointer:
                oracle.watch(attribute_pointer, len(attribute_data), "attribute input")
            oracle.watch(configs_pointer, len(configs_sentinel), "config output and canaries")
            oracle.watch(count_pointer, len(count_sentinel), "count output and canaries")
            result = oracle.call(targets["eglChooseConfig"][0], [display, attribute_pointer,
                                 configs_pointer + 16 if has_configs else 0, limit,
                                 count_pointer + 4 if has_count else 0])
            error = struct.unpack("<i", oracle.readback(state_pointer + 24, 4))[0]
            if captured_sort:
                pointer, count, filter_data = captured_sort
                selected = struct.unpack("<" + "i" * count, oracle.read(pointer, 4 * count))
                filter_pointer = oracle.alloc(CONFIG_BYTES, filter_data)
                cached = {}
                for config_id in selected:
                    cached[config_id] = oracle.alloc(CONFIG_BYTES)
                    oracle.call(targets["eglFillConfig"][0], [cached[config_id], config_id])

                def compare(first, second):
                    value = oracle.call(targets["eglCompareConfigs"][0],
                                        [cached[first], cached[second], filter_pointer])
                    return ctypes.c_int32(value).value

                ordered = sorted(selected, key=cmp_to_key(compare))
                oracle.write(pointer, struct.pack("<" + "i" * count, *ordered))
            expected = (result, error, oracle.readback(count_pointer, len(count_sentinel)),
                        oracle.readback(configs_pointer, len(configs_sentinel)))
            native_attributes = ctypes.create_string_buffer(attribute_data, len(attribute_data)) if attribute_data else None
            native_configs = ctypes.create_string_buffer(configs_sentinel, len(configs_sentinel))
            native_count = ctypes.create_string_buffer(count_sentinel, len(count_sentinel))
            native_error = ctypes.c_int()
            result = adapter(mode, display, native_attributes,
                             ctypes.byref(native_configs, 16) if has_configs else None, limit,
                             ctypes.byref(native_count, 4) if has_count else None,
                             initial_error, ctypes.byref(native_error))
            suite.compare("eglChooseConfig", {"mode": mode.name, "display": display,
                          "attributes": attributes, "limit": limit, "configs": has_configs,
                          "count": has_count, "initial_error": initial_error},
                          (result, native_error.value, native_count.raw, native_configs.raw), expected)
    finally:
        oracle.remove_hook(sort_hook)


def run(suite):
    oracle = suite.oracle
    targets = TARGETS[suite.arch]
    adapter = suite.lib.differential_egl_platform_config
    adapter.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_void_p]
    adapter.restype = ctypes.c_int
    for name in ("eglFillConfig", "egliFixConfig", "eglGetConfigs", "eglGetConfigAttrib", "eglChooseConfig"):
        suite.register(name, *targets[name])
    state_pointer = 0
    hook = oracle.add_hook(GET_STATE[suite.arch], lambda emu: emu.return_from_call(state_pointer))

    def reset(mode, initialized=True):
        nonlocal state_pointer
        oracle.reset()
        display_pointer = oracle.alloc(24)
        allocated_state = oracle.alloc(ORIGINAL_STATE_BYTES)
        state_pointer = allocated_state if initialized else 0
        oracle.write(allocated_state + 20, struct.pack("<Iii", display_pointer, 0x3002, mode))
        # Original TLS and display fixtures contain no contexts or surfaces.
        oracle.watch(allocated_state, ORIGINAL_STATE_BYTES, "EGL state")
        oracle.watch(display_pointer, 24, "EGL display")
        return allocated_state

    try:
        for mode in DisplayMode:
            for config_id in range(1, CONFIG_COUNT + 1):
                reset(mode)
                config_pointer = oracle.alloc(CONFIG_BYTES, b"\xA5" * CONFIG_BYTES)
                oracle.watch(config_pointer, CONFIG_BYTES, "config output")
                oracle.call(targets["eglFillConfig"][0], [config_pointer, config_id])
                original_config = struct.unpack("<24i", oracle.readback(config_pointer, CONFIG_BYTES))
                # ARM public APIs inline this validity test after FillConfig.
                original_valid = int(original_config[15] != 0)
                native_config = (ctypes.c_int32 * CONFIG_WORDS)()
                ctypes.memset(native_config, 0xA5, CONFIG_BYTES)
                native_valid = adapter(config_id, mode, native_config)
                suite.compare("eglFillConfig", {"display_mode": mode.name, "config_id": config_id},
                              (native_valid, tuple(native_config)),
                              (original_valid, original_config))
        _fix_configs(suite, reset, targets)
        _get_configs(suite, reset, targets)
        _get_config_attrib(suite, reset, targets)
        _choose_configs(suite, reset, targets)
    finally:
        oracle.remove_hook(hook)
