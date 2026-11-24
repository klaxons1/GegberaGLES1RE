"""Whole clipper comparisons, including its inlined ARM polygon arithmetic."""
import ctypes as C
import struct


TARGETS = {"arm": (0x1AA2C, 0x1B624), "x86": (0x42F730, 0x4302D2)}
VERTEX_OFFSET = 1332
VERTEX_BYTES = 128
STACK_BYTES = VERTEX_OFFSET + 104 * VERTEX_BYTES
STATE_OFFSETS = {"arm": (116, 4728), "x86": (124, 4736)}


def _position(values):
    largest = max(abs(value) for value in values)
    if largest == 0:
        return bytes(20)
    shift = 30 - largest.bit_length()
    return struct.pack("<I4i", 156 - shift, *(value << shift for value in values))


def _mask(position, planes):
    x, y, z, w = position
    distances = (z + w, w - z, x + 4 * w, 4 * w - x, y + 4 * w, 4 * w - y)
    result = sum(int(distance < 0) << plane for plane, distance in enumerate(distances))
    for plane, coefficients in enumerate(planes):
        result |= int(sum(a * b for a, b in zip(position, coefficients)) < 0) << (10 + plane)
    return result


def _planes(equations):
    result = bytearray(120)
    for index, coefficients in enumerate(equations):
        exponents = bytes(127 if coefficient else 0 for coefficient in coefficients)
        result[20 * index:20 * index + 20] = exponents + struct.pack(
            "<4i", *(coefficient * 0x20000000 for coefficient in coefficients))
    return bytes(result)


def _cases(suite):
    triangle = ((-1, -1, 0, 1), (1, -1, 0, 1), (0, 1, 0, 1))
    # Isolate every guardband plane, then exercise repeated/zero-distance edges.
    for plane in range(6):
        axis = (2, 2, 0, 0, 1, 1)[plane]
        sign = -1 if plane % 2 == 0 else 1
        boundary = 1 if axis == 2 else 4
        for outside in (1, 2, 3):
            positions = [list(v) for v in triangle]
            for index in range(outside):
                positions[index][axis] = sign * (boundary + 1)
            yield positions, (), (0, 1, 2)
        for touch in (0, 1):
            positions = [list(v) for v in triangle]
            positions[0][axis] = sign * (boundary + 1)
            positions[1][axis] = sign * boundary
            if touch:
                positions[2] = positions[1].copy()
            yield positions, (), (3, 7, 18)
    for axis in range(3):
        for sign in (-1, 1):
            equation = tuple(sign if i == axis else 0 for i in range(4))
            positions = [list(v) for v in triangle]
            positions[0][axis], positions[1][axis], positions[2][axis] = -2, 0, 2
            slot = axis * 2 + int(sign > 0)
            yield positions, ((0, 0, 0, 0),) * slot + (equation,), (5, 6, 8)
    yield ((-2, -1, -2, 1), (1, -2, 0, 1), (2, 2, 2, 1)), (
        (1, 0, 0, 0), (0, 1, 0, 0), (0, 0, 1, 0)), (2, 16, 42)
    yield ((0, 0, 0, 0), triangle[1], triangle[2]), (), (0, 1, 2)
    for _ in range(min(suite.samples, 128)):
        positions = [tuple(suite.rng.randrange(-6, 7) for _ in range(3)) + (1,) for _ in range(3)]
        if not any(_mask(position, ()) for position in positions):
            continue
        yield positions, (), tuple(suite.rng.sample(range(92), 3))


def run(suite):
    oracle = suite.oracle
    start, end = TARGETS[suite.arch]
    stack_offset, planes_offset = STATE_OFFSETS[suite.arch]
    suite.register("grClipper", start, end)
    adapter = suite.lib.differential_clipper
    adapter.argtypes = [C.c_void_p, C.c_void_p, C.c_uint32, C.c_void_p, C.c_void_p]
    adapter.restype = C.c_int
    for case, (positions, equations, indices) in enumerate(_cases(suite)):
        oracle.reset()
        plane_data = _planes(equations)
        stack_data = bytearray(b"\xA5" * STACK_BYTES)
        for index, position in zip(indices, positions):
            offset = VERTEX_OFFSET + VERTEX_BYTES * index
            struct.pack_into("<iHH", stack_data, offset, index, 0, _mask(position, equations))
            stack_data[offset + 8:offset + 28] = _position(position)
        stack = oracle.alloc(STACK_BYTES, bytes(stack_data))
        core_data = bytearray(0x3000)
        struct.pack_into("<I", core_data, stack_offset, stack)
        core_data[planes_offset:planes_offset + 120] = plane_data
        core = oracle.alloc(len(core_data), bytes(core_data))
        sentinel = b"\xA5" * 47
        fan = oracle.alloc(len(sentinel), sentinel)
        oracle.watch(stack, STACK_BYTES, "render stack and all vertex records")
        oracle.watch(core, len(core_data), "core state")
        oracle.watch(fan, len(sentinel), "fan output and canaries")
        packed_indices = sum(index << (8 * i) for i, index in enumerate(indices))
        count = oracle.call(start, [core, fan + 16, packed_indices])
        native_input = C.create_string_buffer(bytes(stack_data), STACK_BYTES)
        native_planes = C.create_string_buffer(plane_data, 120)
        native_output = C.create_string_buffer(STACK_BYTES)
        native_fan = C.create_string_buffer(sentinel, len(sentinel))
        native_count = adapter(native_input, native_planes, packed_indices,
                               native_output, C.byref(native_fan, 16))
        suite.compare_bytes("grClipper", {"case": case, "positions": positions,
                            "equations": equations, "indices": indices},
                            struct.pack("<i", native_count) + native_output.raw + native_fan.raw,
                            struct.pack("<I", count) + oracle.readback(stack, STACK_BYTES)
                            + oracle.readback(fan, len(sentinel)))
