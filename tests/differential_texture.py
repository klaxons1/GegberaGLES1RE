"""Whole-buffer comparisons for image conversion, fills, clears, and blits."""

import ctypes as C
import struct

from differential_raster import buffer, words


TARGETS = {
    "arm": {
        "glCopyTexImage2D": (0x1395C, 0x13AA8),
        "glCopyTexSubImage2D": (0x13768, 0x1395C),
        "grReadDataTypeColor": (0x216E0, 0x21868),
        "grWriteDataTypeColor": (0x21868, 0x219E0),
        "grFill8Masked": (0x174E4, 0x17528),
        "grFill16Masked": (0x17528, 0x17580),
        "grFill32Masked": (0x17580, 0x175B8),
        "grFill4x8": (0x175B8, 0x176C8),
        "grFill4x16": (0x176C8, 0x177C8),
        "grFill4x32": (0x177C8, 0x17890),
        "grrClearBuffers": (0x20D90, 0x20DE0),
        "grrClearColorBuffer": (0x20C7C, 0x20D08),
        "grrClearDitheredColorBuffer": (0x20A98, 0x20BAC),
        "grrClearDepthBuffer": (0x20D08, 0x20D58),
        "grrClearStencilBuffer": (0x20D58, 0x20D90),
        "grrCopyColorSpan": (0x219E0, 0x21B34),
        "grrBlit": (0x21B34, 0x21D40),
        "grrConvertBlitImage": (0x21274, 0x21368),
        "grInitTexture": (0x229D8, 0x22A74),
        "grrSetupTextureLevel": (0x20E38, 0x20ECC),
        "grrReplaceTextureLevel": (0x20DEC, 0x20E38),
        "grrSetPalettedTexture": (0x21540, 0x216E0),
        "grrCalcTextureMipmaps": (0x213C8, 0x214AC),
        "grrBlitMipmap": (0x21368, 0x213C8),
        "grrDownsample": (0x20ECC, 0x21254),
        "grrFreeTextureLevel": (0x214AC, 0x214FC),
        "grrFreeTextureData": (0x214FC, 0x21540),
    },
    "x86": {
        "glCopyTexImage2D": (0x41A220, 0x41A407),
        "glCopyTexSubImage2D": (0x419ED0, 0x41A218),
        "grReadDataTypeColor": (0x440360, 0x44071A),
        "grWriteDataTypeColor": (0x440720, 0x440B2B),
        "grFill8Masked": (0x427DF0, 0x427F0F),
        "grFill16Masked": (0x427F10, 0x42808E),
        "grFill32Masked": (0x428090, 0x4281F9),
        "grFill4x8": (0x428200, 0x428481),
        "grFill4x16": (0x428490, 0x42881E),
        "grFill4x32": (0x428820, 0x428A06),
        "grrClearBuffers": (0x43E980, 0x43EA4D),
        "grrCopyColorSpan": (0x4410E0, 0x441777),
        "grrBlit": (0x441780, 0x441EF9),
        "grrConvertBlitImage": (0x440B30, 0x440F7C),
        "grInitTexture": (0x446D10, 0x446ED1),
        "grrSetupTextureLevel": (0x43F3E0, 0x43F73E),
        "grrReplaceTextureLevel": (0x43F740, 0x43FA00),
        "grrSetPalettedTexture": (0x43FEA0, 0x440358),
        "grrCalcTextureMipmaps": (0x43FA00, 0x43FC99),
        "grrBlitMipmap": (0x4429C0, 0x442E3F),
        "grrDownsample": (0x442500, 0x4429B9),
        "grrFreeTextureLevel": (0x43FCA0, 0x43FDDE),
        "grrFreeTextureData": (0x43FDE0, 0x43FE97),
    },
}

COMPONENTS = (1, 3, 4, 1, 2, 3, 4)
TYPE_BYTES = (1, 1, 2, 2, 4, 4, 4)
GUARD_BYTES = 16
IMAGE_WIDTH = 8
IMAGE_HEIGHT = 5
BLIT_IMAGE_HEADER_BYTES = 12


def descriptor(offsets, sizes, bpp):
    mask = sum(1 << i for i, value in enumerate(sizes) if value)
    return bytes((*offsets, *sizes, mask, bpp, 2, 1, 0, 0, 0, 0))


DESCRIPTORS = (
    descriptor((0, 0, 0, 0), (0, 0, 0, 8), 1),
    descriptor((0, 3, 6, 0), (3, 3, 2, 0), 1),
    descriptor((0, 5, 11, 0), (5, 6, 5, 0), 2),
    descriptor((0, 5, 10, 15), (5, 5, 5, 1), 2),
    descriptor((0, 4, 8, 12), (4, 4, 4, 4), 2),
    descriptor((0, 8, 16, 24), (8, 8, 8, 8), 4),
    descriptor((16, 8, 0, 24), (8, 8, 8, 8), 4),
)


def run(suite):
    targets = TARGETS[suite.arch]
    for name, bounds in targets.items():
        suite.register(name, *bounds)
    run_colors(suite, targets)
    run_fills(suite, targets)
    run_clears(suite, targets)
    run_copies(suite, targets)
    run_conversions(suite, targets)
    run_allocations(suite, targets)
    run_empty_copies(suite, targets)
    run_uploads(suite, targets)


def run_colors(suite, targets):
    oracle, rng = suite.oracle, suite.rng
    read = suite.lib.grReadDataTypeColor
    read.restype = C.c_uint32
    write = suite.lib.grWriteDataTypeColor
    for dtype in range(len(TYPE_BYTES)):
        for fmt in range(len(COMPONENTS)):
            for variant in range(4):
                data = rng.randbytes(48)
                if dtype == 6:
                    values = ((-1.0, 0.0, 0.5, 1.0), (2.0, 0.1, 0.9, 1.5),
                              (1.0 / 255, 127.0 / 255, 128.0 / 255, 254.0 / 255),
                              (-0.0, 1.0e-30, -1.0e30, 1.0e30))[variant]
                    data = data[:16] + struct.pack("<4f", *values) + data[32:]
                native = buffer(data)
                oracle.reset()
                original = oracle.alloc(len(data), data)
                oracle.watch(original, len(data), "typed-color")
                inputs = (dtype, fmt, variant)
                # The x86 debug component-count helper asserts RGBA for BGRA;
                # the ARM release routine accepts all seven formats.
                if suite.arch == "arm" or fmt != 6:
                    result = read(C.byref(native, GUARD_BYTES), dtype, fmt)
                    expected = oracle.call(targets["grReadDataTypeColor"][0],
                                           [original + GUARD_BYTES, dtype, fmt])
                    suite.compare("grReadDataTypeColor", inputs,
                                  (result, bytes(native)),
                                  (expected, oracle.readback(original, len(data))))
                color = (0, 0xFFFFFFFF, 0x10203040, rng.getrandbits(32))[variant]
                write(C.byref(native, GUARD_BYTES), dtype, fmt, C.c_uint32(color))
                oracle.call(targets["grWriteDataTypeColor"][0],
                            [original + GUARD_BYTES, dtype, fmt, color])
                suite.compare("grWriteDataTypeColor", (*inputs, color), bytes(native),
                              oracle.readback(original, len(data)))


def run_fills(suite, targets):
    oracle, rng = suite.oracle, suite.rng
    for bpp in (1, 2, 4):
        for patterned in (False, True):
            name = f"grFill4x{8 * bpp}" if patterned else f"grFill{8 * bpp}Masked"
            sizes = (1, 2, 3, 4, 5, 7, 8, 15, 16, 17, 32, 65)
            for size in sizes if patterned else (0, *sizes):
                for offset in range(0, 4, bpp):
                    for mask in (0, 0xFFFFFFFF, 0xA55A3CC3):
                        oracle.reset()
                        data = rng.randbytes(GUARD_BYTES * 2 + 4 + size * bpp)
                        native = buffer(data)
                        original = oracle.alloc(len(data), data)
                        oracle.watch(original, len(data), "fill-buffer")
                        values = [rng.getrandbits(bpp * 8) for _ in range(4 if patterned else 1)]
                        args = [*values, mask, size]
                        getattr(suite.lib, name)(C.byref(native, GUARD_BYTES + offset),
                                                *(C.c_uint32(v) for v in args))
                        oracle.call(targets[name][0], [original + GUARD_BYTES + offset, *args])
                        suite.compare(name, (size, offset, *args[:-1]),
                                      bytes(native)[:len(data)], oracle.readback(original, len(data)))


def run_clears(suite, targets):
    oracle, rng = suite.oracle, suite.rng
    for index in range(max(64, suite.samples)):
        oracle.reset()
        desc = bytearray(DESCRIPTORS[index % len(DESCRIPTORS)])
        desc[10] = (1, 2, 4)[index % 3]
        desc[11] = (1, 2, 4)[(index // 3) % 3]
        padding = (index % 2) * 8
        strides = [IMAGE_WIDTH * bpp + padding for bpp in desc[9:12]]
        config = (IMAGE_WIDTH, IMAGE_HEIGHT, *strides)
        rect = ((0, IMAGE_WIDTH, 0, IMAGE_HEIGHT), (1, 6, 1, 4),
                (3, 3, 2, 3), (2, 5, 2, 2))[index % 4]
        info = words((*rect, rng.getrandbits(32) if index % 5 else 0,
                      rng.randrange(1 << 30), rng.getrandbits(32)))
        info += bytes((index % 8, index % 16, index % 3 != 0,
                       (0, 255, 0x5A)[index % 3], (index // 4) % 2, 0, 0, 0))
        data = [rng.randbytes(GUARD_BYTES * 2 + stride * IMAGE_HEIGHT) for stride in strides]
        native = [buffer(item) for item in data]
        original = [oracle.alloc(len(item), item) for item in data]
        active = [(index + i) % 13 != 0 for i in range(3)]
        for pointer, item in zip(original, data):
            oracle.watch(pointer, len(item), "clear-buffer")
        surface = bytes(desc) + words((IMAGE_WIDTH, IMAGE_HEIGHT))
        surface += b"".join(words((ptr + GUARD_BYTES if enabled else 0, stride))
                            for ptr, stride, enabled in zip(original, strides, active))
        suite.lib.differential_texture_clear(buffer(bytes(desc)), buffer(words(config)), buffer(info),
                                             *(C.byref(item, GUARD_BYTES) if enabled else None
                                               for item, enabled in zip(native, active)))
        oracle.call(targets["grrClearBuffers"][0],
                    [oracle.alloc(len(surface), surface), oracle.alloc(len(info), info)])
        suite.compare("grrClearBuffers", (index, tuple(desc), rect, tuple(info[28:33])),
                      tuple(bytes(item) for item in native),
                      tuple(oracle.readback(ptr, len(item)) for ptr, item in zip(original, data)))


def memory_image(config, desc, pixels, arch):
    width, height, stride, bpp, use_descriptor, dtype, fmt = config
    types = bytes((dtype, fmt, 0, 0)) if arch == "arm" else words((dtype, fmt))
    return words((width, height, stride, bpp, pixels, use_descriptor)) + desc + types


def run_copies(suite, targets):
    oracle, rng = suite.oracle, suite.rng
    for index in range(max(98, suite.samples)):
        oracle.reset()
        configs, descriptors, data, native, original, offsets = [], [], [], [], [], []
        for side in range(2):
            desc = DESCRIPTORS[(index + side) % len(DESCRIPTORS)]
            dtype, fmt = (index + side) % 7, (index // 7 + side) % (7 if suite.arch == "arm" else 6)
            use_descriptor = (index >> side) & 1
            bpp = desc[9] if use_descriptor else TYPE_BYTES[dtype] * COMPONENTS[fmt]
            stride = IMAGE_WIDTH * bpp + 8
            item = rng.randbytes(GUARD_BYTES * 2 + stride * IMAGE_HEIGHT)
            if dtype == 6 and not use_descriptor:
                item = struct.pack("<f", 0.375) * (len(item) // 4)
            negative = (index // 4 + side) % 2
            offset = GUARD_BYTES + (IMAGE_HEIGHT - 1) * stride * negative
            configs.extend((IMAGE_WIDTH, IMAGE_HEIGHT, -stride if negative else stride,
                            bpp, use_descriptor, dtype, fmt))
            descriptors.append(desc)
            data.append(item)
            native.append(buffer(item))
            original.append(oracle.alloc(len(item), item))
            offsets.append(offset)
            oracle.watch(original[-1], len(item), "blit-buffer")
        if index % 9 == 0:
            configs[11:14] = configs[4:7]
            configs[10] = configs[3]
            configs[9] = configs[2]
            descriptors[1] = descriptors[0]
            data[1] = rng.randbytes(len(data[0]))
            native[1] = buffer(data[1])
            original[1] = oracle.alloc(len(data[1]), data[1])
            offsets[1] = offsets[0]
            oracle.watch(original[1], len(data[1]), "blit-buffer")
        rect = (1, 6, 1, 2, 2, 7, 2, 3)
        blit = index % 2
        if blit:
            rect = ((-2, 7, -1, 4, 0, 9, 1, 6), (1, 6, 1, 4, -1, 4, 0, 3),
                    (9, 12, 0, 3, 1, 4, 1, 4), (0, 8, 0, 5, 0, 8, 0, 5))[(index // 2) % 4]
        active = [not blit or (index + side) % 11 != 0 for side in range(2)]
        native_rect = buffer(words(rect))
        suite.lib.differential_texture_copy(buffer(words(configs)), buffer(b"".join(descriptors)),
                                            *(C.byref(native[side], offsets[side]) if active[side] else None
                                              for side in range(2)),
                                            native_rect, blit)
        images = [oracle.alloc(BLIT_IMAGE_HEADER_BYTES + (44 if suite.arch == "arm" else 48),
                              words((0, 0, 0)) + memory_image(configs[side * 7:side * 7 + 7],
                                                              descriptors[side], original[side] + offsets[side]
                                                              if active[side] else 0, suite.arch))
                  for side in range(2)]
        name = "grrBlit" if blit else "grrCopyColorSpan"
        if blit:
            state = bytearray(12000)
            scratch = oracle.alloc(8192)
            delta = 0 if suite.arch == "arm" else 8
            struct.pack_into("<III", state, 176 + delta, scratch, 8192, 0)
            struct.pack_into("<II", state, 600 + delta, scratch, 8192)
            info = oracle.alloc(40, words((*rect, *images)))
            oracle.watch(info, 32, "blit-rectangles")
            oracle.call(targets[name][0], [oracle.alloc(len(state), bytes(state)), info])
            original_rect = oracle.readback(info, 32)
        else:
            oracle.call(targets[name][0], [images[1] + BLIT_IMAGE_HEADER_BYTES,
                                          images[0] + BLIT_IMAGE_HEADER_BYTES,
                                          rect[4], rect[6], rect[0], rect[2], rect[1] - rect[0], 0])
            original_rect = words(rect)
        suite.compare(name, (index, tuple(configs), rect),
                      (bytes(native_rect), *(bytes(item) for item in native)),
                      (original_rect, *(oracle.readback(ptr, len(item))
                                        for ptr, item in zip(original, data))))


TEXTURE_BYTES = 208
TEXTURE_PAYLOAD_BYTES = 4096
TEXTURE_OUTPUT_BYTES = 4 + TEXTURE_BYTES * 2 + TEXTURE_PAYLOAD_BYTES
MIP_LEVELS = 11
BITS_PER_PIXEL = (8, 32, 32, 8, 16, 4, 4, 4, 4, 4, 8, 8, 8, 8, 8, 16, 16, 16)


def normalized_texture(data):
    result = bytearray(data)
    for offset in (*range(0, 176, 16), 176):
        struct.pack_into("<I", result, offset, bool(struct.unpack_from("<I", result, offset)[0]))
    return bytes(result)


def run_conversions(suite, targets):
    oracle, rng = suite.oracle, suite.rng
    for index in range(48):
        oracle.reset()
        fmt = (0, 1, 2, 3, 4, 15, 16, 17)[index % 8]
        dtype = buffer(bytes(16))
        suite.lib.grrGetTextureFormatDescriptor(dtype, fmt)
        desc = bytes(dtype)
        image_type, present = index % 3, (index // 3) % 2
        width, height, level = (8 if index % 2 else 2), (4 if index % 2 else 1), index % 11
        config = (image_type, width, height, fmt, present, level)
        pixels = rng.randbytes(256)
        native_pixels, output = buffer(pixels), buffer(bytes(60))
        suite.lib.differential_texture_convert(buffer(words(config)), dtype, native_pixels, output)
        pixel_pointer = oracle.alloc(len(pixels), pixels)
        texture_data = bytearray(TEXTURE_BYTES)
        struct.pack_into("<IhhHHI", texture_data, level * 16,
                         pixel_pointer if present else 0, 3 if width == 8 else 1,
                         2 if height == 4 else 0, fmt, 0, 0)
        texture = oracle.alloc(TEXTURE_BYTES, bytes(texture_data))
        state = bytearray(12000)
        surface = desc + words((width, height, pixel_pointer if present else 0,
                               width * desc[9] + 8, 0, 0, 0, 0))
        state_offset = 7424 + (8 if suite.arch == "x86" else 0)
        state[state_offset:state_offset + len(surface)] = surface
        original_image = words((image_type, texture, level))
        original_image += memory_image((3, 2, 16, 4, 0, 5, 6), desc, pixel_pointer, suite.arch)
        image = oracle.alloc(len(original_image), original_image)
        oracle.watch(image, len(original_image), "converted-image")
        oracle.watch(pixel_pointer, len(pixels), "conversion-pixels")
        oracle.call(targets["grrConvertBlitImage"][0],
                    [oracle.alloc(len(state), bytes(state)), image])
        result = bytearray(oracle.readback(image, len(original_image)))
        texture_pointer, = struct.unpack_from("<I", result, 4)
        struct.pack_into("<I", result, 4, 1 if texture_pointer == texture else (2 if texture_pointer else 0))
        pointer, = struct.unpack_from("<I", result, 28)
        struct.pack_into("<I", result, 28, pointer - pixel_pointer + 1 if pointer else 0)
        normalized = bytes(result[:52]) + (words((result[52], result[53]))
                                           if suite.arch == "arm" else bytes(result[52:]))
        suite.compare("grrConvertBlitImage", config,
                      (bytes(output), bytes(native_pixels)),
                      (normalized, oracle.readback(pixel_pointer, len(pixels))))


def run_allocations(suite, targets):
    oracle = suite.oracle
    allocation_entry, free_entry = ((0x8A90, 0x823C) if suite.arch == "arm"
                                    else (0x4060D0, 0x4061B0))
    allocations = {}

    def allocate(machine):
        size, = machine.arguments(1)
        if not 0 < size <= 128:
            raise ValueError(f"unexpected texture allocation size {size}")
        pointer = machine.alloc(size)
        allocations[pointer] = size
        machine.return_from_call(pointer)

    def release(machine):
        pointer, = machine.arguments(1)
        if pointer:
            if pointer not in allocations:
                raise ValueError("texture freed borrowed or already-freed storage")
            del allocations[pointer]
        machine.return_from_call()

    hooks = [oracle.add_hook(allocation_entry, allocate), oracle.add_hook(free_entry, release)]
    try:
        # Same-size replacements still allocate in the Nokia builds. Empty
        # levels remain initialized, and replacement preserves pbuffer ownership.
        for fmt in range(len(BITS_PER_PIXEL)):
            for width, height in ((0, 0), (0, 4), (4, 0), (1, 1), (4, 4), (8, 4)):
                for old_kind in range(3):
                    oracle.reset()
                    allocations.clear()
                    level = (0, 4, 10)[old_kind]
                    config = (fmt, width, height, level, old_kind)
                    native = buffer(bytes(8 + 2 * TEXTURE_BYTES + 64))
                    suite.lib.differential_texture_allocate(buffer(words(config)), native)
                    texture = oracle.alloc(TEXTURE_BYTES)
                    oracle.watch(texture, TEXTURE_BYTES, "texture-allocation-record")
                    oracle.call(targets["grInitTexture"][0], [texture])
                    oracle.write(texture + 182, b"\0\2")
                    borrowed_data = bytes([0x5A]) * 64
                    borrowed = oracle.alloc(64, borrowed_data)
                    oracle.watch(borrowed, 64, "borrowed-texture-storage")
                    old_pointer = 0
                    if old_kind:
                        old_pointer = borrowed if old_kind == 2 else oracle.alloc(64)
                        if old_kind == 1:
                            allocations[old_pointer] = 64
                        oracle.call(targets["grrReplaceTextureLevel"][0],
                                    [texture, level, old_pointer, 4, 4, fmt])
                        oracle.write(texture + level * 16 + 12, words((old_kind == 2,)))
                        oracle.write(texture + 183, b"\2")
                    status = oracle.call(targets["grrSetupTextureLevel"][0],
                                         [texture, level, width, height, fmt])
                    record = oracle.readback(texture, TEXTURE_BYTES)
                    pointer, = struct.unpack_from("<I", record, level * 16)
                    prefix = words((status, bool(pointer and pointer == old_pointer)))
                    suite.compare("grrSetupTextureLevel", config,
                                  bytes(native)[:8 + TEXTURE_BYTES],
                                  prefix + normalized_texture(record))
                    oracle.call(targets["grrFreeTextureData"][0], [texture])
                    freed = normalized_texture(oracle.readback(texture, TEXTURE_BYTES))
                    preserved = oracle.readback(borrowed, 64)
                    expected_allocations = {pointer: allocations[pointer]} if pointer and old_kind == 2 else {}
                    suite.compare("grrFreeTextureData", config,
                                  (bytes(native)[8 + TEXTURE_BYTES:], expected_allocations),
                                  (freed + preserved, allocations))
    finally:
        for hook in hooks:
            oracle.remove_hook(hook)


def run_empty_copies(suite, targets):
    oracle = suite.oracle
    get_context, get_texture, get_descriptor, free = (
        (0xB59C, 0xD7AC, 0x227FC, 0x823C) if suite.arch == "arm"
        else (0x4085B0, 0x412720, 0x445D00, 0x4061B0))
    descriptor_bytes = buffer(bytes(16))
    suite.lib.grrGetTextureFormatDescriptor(descriptor_bytes, 2)

    def descriptor(machine):
        _, destination = machine.arguments(2)
        machine.write(destination, bytes(descriptor_bytes))
        machine.return_from_call()

    def release(machine):
        if machine.arguments(1) != (0,):
            raise ValueError("empty texture copy unexpectedly freed storage")
        machine.return_from_call()

    for width, height in ((0, 0), (0, 1), (1, 0)):
        for level in (0, 10):
            for fmt in range(0x1906, 0x190B):
                for initial_error in (0, 3):
                    oracle.reset()
                    config = (width, height, level, fmt, initial_error)
                    native = buffer(bytes(4 + TEXTURE_BYTES))
                    suite.lib.differential_texture_copy_empty(buffer(words(config)), native)
                    context = oracle.alloc(8, words((0, initial_error)))
                    texture = oracle.alloc(TEXTURE_BYTES)
                    oracle.watch(context, 8, "empty-copy-context")
                    oracle.watch(texture, TEXTURE_BYTES, "empty-copy-texture")
                    oracle.call(targets["grInitTexture"][0], [texture])
                    oracle.write(texture + 182, b"\0")
                    hooks = [oracle.add_hook(get_context, lambda m: m.return_from_call(context)),
                             oracle.add_hook(get_texture, lambda m: m.return_from_call(texture)),
                             oracle.add_hook(get_descriptor, descriptor),
                             oracle.add_hook(free, release)]
                    try:
                        oracle.call(targets["glCopyTexImage2D"][0],
                                    [0xDE1, level, fmt, 0, 0, width, height, 0])
                    finally:
                        for hook in hooks:
                            oracle.remove_hook(hook)
                    error = oracle.readback(context, 8)[4:]
                    record = normalized_texture(oracle.readback(texture, TEXTURE_BYTES))
                    suite.compare("glCopyTexImage2D", config, bytes(native), error + record)


def run_uploads(suite, targets):
    oracle, rng = suite.oracle, suite.rng
    allocation_entry, free_entry = ((0x8A90, 0x823C) if suite.arch == "arm"
                                    else (0x4060D0, 0x4061B0))
    allocations = {}

    def allocate(machine):
        size, = machine.arguments(1)
        if not 0 < size <= TEXTURE_PAYLOAD_BYTES:
            raise ValueError(f"unexpected texture allocation size {size}")
        pointer = machine.alloc(size, bytes([0xA5]) * size)
        allocations[pointer] = size
        machine.watch(pointer, size, "texture-pixels")
        machine.return_from_call(pointer)

    def release(machine):
        pointer, = machine.arguments(1)
        if pointer:
            if pointer not in allocations:
                raise ValueError("texture freed a foreign or already-freed allocation")
            del allocations[pointer]
        machine.return_from_call()

    hooks = [oracle.add_hook(allocation_entry, allocate), oracle.add_hook(free_entry, release)]
    try:
        for fmt in range(len(BITS_PER_PIXEL)):
            for width, height in ((1, 1), (1, 8), (8, 1), (8, 4), (4, 8)):
                oracle.reset()
                allocations.clear()
                descriptor_bytes = buffer(bytes(16))
                suite.lib.grrGetTextureFormatDescriptor(descriptor_bytes, fmt)
                desc = bytes(descriptor_bytes)
                paletted = 5 <= fmt <= 14
                levels = max(width, height).bit_length()
                if paletted:
                    palette_entries = 16 if fmt < 10 else 256
                    entry_bytes = 3 if fmt in (5, 10) else desc[9]
                    count = palette_entries * entry_bytes
                    count += sum(max(1, max(1, width >> level) * max(1, height >> level)
                                     * BITS_PER_PIXEL[fmt] // 8) for level in range(levels))
                else:
                    count = width * height * BITS_PER_PIXEL[fmt] // 8
                source = rng.randbytes(count)
                native = buffer(bytes(TEXTURE_OUTPUT_BYTES))
                suite.lib.differential_texture_upload(buffer(words((fmt, width, height, levels))),
                                                      descriptor_bytes, buffer(source), native)
                texture = oracle.alloc(TEXTURE_BYTES)
                oracle.watch(texture, TEXTURE_BYTES, "texture-record")
                oracle.call(targets["grInitTexture"][0], [texture])
                oracle.write(texture + 182, b"\0")
                source_pointer = oracle.alloc(len(source), source)
                if paletted:
                    name = "grrSetPalettedTexture"
                    status = oracle.call(targets[name][0],
                                         [texture, oracle.alloc(16, desc), fmt,
                                          width, height, levels, source_pointer],
                                         instruction_budget=1000000)
                else:
                    status = oracle.call(targets["grrSetupTextureLevel"][0],
                                         [texture, 0, width, height, fmt])
                    if status != 1:
                        raise ValueError("texture base allocation failed")
                    pixel_pointer, = struct.unpack("<I", oracle.read(texture, 4))
                    oracle.write(pixel_pointer, source)
                    name = "grrCalcTextureMipmaps"
                    status = oracle.call(targets[name][0], [oracle.alloc(12000), texture])
                record = oracle.readback(texture, TEXTURE_BYTES)
                payload = b""
                palette, = struct.unpack_from("<I", record, 176)
                if palette:
                    payload += oracle.readback(palette, allocations[palette])
                for level in range(MIP_LEVELS):
                    pointer, = struct.unpack_from("<I", record, level * 16)
                    if pointer:
                        payload += oracle.readback(pointer, allocations[pointer])
                payload += bytes([0xA5]) * (TEXTURE_PAYLOAD_BYTES - len(payload))
                # Compare before freeing so a subsequent write invalidates the
                # record readback instead of being accidentally counted as checked.
                suite.compare(name, (fmt, width, height, levels),
                              bytes(native)[:4 + TEXTURE_BYTES + TEXTURE_PAYLOAD_BYTES],
                              words((status,)) + normalized_texture(record) + payload)
                oracle.call(targets["grrFreeTextureData"][0], [texture])
                freed = normalized_texture(oracle.readback(texture, TEXTURE_BYTES))
                suite.compare("grrFreeTextureData", (fmt, width, height),
                              (bytes(native)[-TEXTURE_BYTES:], 0), (freed, len(allocations)))
    finally:
        for hook in hooks:
            oracle.remove_hook(hook)
