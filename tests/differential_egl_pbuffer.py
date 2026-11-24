"""Pbuffer creation, configuration parsing, list insertion and immediate release."""
import ctypes as C
import struct

from differential_egl_objects import (words, EGL_WIDTH, EGL_HEIGHT, EGL_LARGEST_PBUFFER,
    EGL_TEXTURE_FORMAT, EGL_TEXTURE_TARGET, EGL_MIPMAP_TEXTURE, EGL_NO_TEXTURE,
    EGL_TEXTURE_RGB, EGL_TEXTURE_RGBA, EGL_TEXTURE_2D)


def run(suite):
    oracle, lib = suite.oracle, suite.lib
    arm = suite.arch == "arm"
    create, end = (0x9D58, 0xA07C) if arm else (0x403F50, 0x404603)
    destroy = 0xA288 if arm else 0x4047E0
    suite.register("eglCreatePbufferSurface", create, end)
    lib.differential_egl_pbuffer.argtypes = [C.c_int, C.c_int, C.c_void_p, C.c_void_p]
    lib.differential_egl_pbuffer.restype = None
    state = 0
    allocated, freed = {}, set()

    def allocate(machine):
        size, = machine.arguments(1)
        if not 0 < size <= 65536:
            raise ValueError(f"pbuffer allocation outside fixture limit: {size}")
        pointer = machine.alloc(size)
        if not allocated:
            machine.watch(pointer, size, "complete pbuffer record during creation")
        allocated[pointer] = size
        machine.return_from_call(pointer)

    def release(machine):
        pointer, = machine.arguments(1)
        if pointer:
            if pointer not in allocated or pointer in freed:
                raise ValueError("pbuffer released an unknown allocation")
            freed.add(pointer)
        machine.return_from_call()

    hooks = [oracle.add_hook(0x8088 if arm else 0x406A50, lambda m: m.return_from_call(state)),
             oracle.add_hook(0x8A90 if arm else 0x4060D0, allocate),
             oracle.add_hook(0x823C if arm else 0x4061B0, release)]
    attributes = [None, (), (0xDEAD, 9), (EGL_WIDTH, -1), (EGL_HEIGHT, -1),
                  (EGL_TEXTURE_FORMAT, 0), (EGL_TEXTURE_TARGET, 0),
                  (EGL_WIDTH, 3, EGL_HEIGHT, 5)]
    for width, height in ((0, 0), (0, 4), (1, 1), (4, 8), (3, 5), (63, 1), (2049, 1)):
        for largest in (0, 1):
            for fmt, target in ((EGL_NO_TEXTURE, EGL_NO_TEXTURE), (EGL_TEXTURE_RGB, EGL_TEXTURE_2D),
                                (EGL_TEXTURE_RGBA, EGL_TEXTURE_2D), (EGL_TEXTURE_RGBA, EGL_NO_TEXTURE)):
                # largest + zero textured dimensions enters an original signed
                # shift UB path; the native safe handling is tested separately.
                if largest and target == EGL_TEXTURE_2D and not width * height:
                    continue
                attributes.append((EGL_WIDTH, width, EGL_HEIGHT, height, EGL_LARGEST_PBUFFER, largest,
                                   EGL_TEXTURE_FORMAT, fmt, EGL_TEXTURE_TARGET, target,
                                   EGL_MIPMAP_TEXTURE, -1))
    attributes += [(EGL_WIDTH, 4, EGL_WIDTH, 8, EGL_HEIGHT, 2),
                   (EGL_WIDTH, -1, EGL_WIDTH, 4), (EGL_TEXTURE_TARGET, EGL_TEXTURE_2D)]
    cases = [(1, config, attrs) for config in (1, 4, 7, 19, 22, 37, 55, 70) for attrs in attributes]
    cases += [(display, config, attrs) for display in (0, 1, 2) for config in (0, 2, 73)
              for attrs in (None, (0xDEAD, 1), (EGL_WIDTH, -1))]
    try:
        for display_id, config_id, attrs in cases:
            oracle.reset()
            allocated.clear()
            freed.clear()
            state = oracle.alloc(40)
            display = oracle.alloc(20)
            existing = oracle.alloc(80)
            oracle.write(state + 20, words((display, 0x3002, 7)))
            oracle.write(display + 4, words((existing, 0, 100, 0)))
            oracle.write(existing, words((2, 0, 0, display, 0, 0, 0, 0, 0, 0, 0, 77, 1)))
            data = words((*attrs, 0x3038)) if attrs is not None else None
            pointer = oracle.alloc(len(data), data) if data else 0
            if pointer:
                oracle.watch(pointer, len(data), "pbuffer attributes")
            result = oracle.call(create, [display_id, config_id, pointer])
            native = (C.c_uint * 30)()
            attribute_buffer = C.create_string_buffer(data, len(data)) if data else None
            lib.differential_egl_pbuffer(display_id, config_id, attribute_buffer, native)
            read_word = lambda p: struct.unpack("<I", oracle.read(p, 4))[0]
            new = read_word(display + 4) if result else 0
            identities = {0: 0, display: 1, existing: 10}
            if new:
                identities[new] = 11
            expected = [result, read_word(state + 24), read_word(display + 12),
                        identities.get(read_word(display + 4), 0xFFFFFFFF),
                        identities.get(read_word(existing + 4), 0xFFFFFFFF)]
            if new:
                body = list(struct.unpack("<20I", oracle.readback(new, 80)))
                for i in (1, 2, 3, 18):
                    body[i] = identities.get(body[i], 0xFFFFFFFF)
                for i in (4, 5, 6):
                    body[i] = int(bool(body[i]))
                expected += body
                suite.compare_bytes("eglCreatePbufferSurface", (display_id, config_id, attrs),
                                    bytes(native)[:100] + (attribute_buffer.raw if data else b""),
                                    words(expected) + (oracle.readback(pointer, len(data)) if data else b""))
                oracle.unwatch(new)
                destroyed = oracle.call(destroy, [1, result])
            else:
                expected += [0] * 20
                destroyed = 0
            expected += [destroyed, read_word(state + 24), read_word(display + 12),
                         identities.get(read_word(display + 4), 0xFFFFFFFF),
                         identities.get(read_word(existing + 4), 0xFFFFFFFF)]
            if set(allocated) != freed:
                raise ValueError("original pbuffer fixture left an allocation live")
            if new:
                suite.compare_bytes("eglDestroySurface", (config_id, attrs),
                                    bytes(native)[100:], words(expected[25:]))
            else:
                suite.compare_bytes("eglCreatePbufferSurface", (display_id, config_id, attrs),
                                    bytes(native) + (attribute_buffer.raw if data else b""),
                                    words(expected) + (oracle.readback(pointer, len(data)) if data else b""))
    finally:
        for hook in hooks:
            oracle.remove_hook(hook)
