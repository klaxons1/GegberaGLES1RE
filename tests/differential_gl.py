"""Compare GL getter metadata, state mutations, and conversions with the PE."""
import ctypes as C
import struct


FUNCTIONS = (
    ("oglIntToOutput", 0x40CB90, 0x40CDB5),
    ("oglUIntToOutput", 0x40CDC0, 0x40D029),
    ("oglBoolToOutput", 0x40D030, 0x40D264),
    ("oglIScalarToOutput", 0x40D270, 0x40D559),
    ("oglFloatToOutput", 0x40D560, 0x40D854),
    ("oglUnitValueToOutput", 0x40D860, 0x40DAE8),
    ("oglFindGetterInfo", 0x40DB30, 0x40DBE7),
    ("oglSetDefaultGetterValues", 0x40DBF0, 0x40DC6C),
    ("oglSetGetterValueBool", 0x40DC70, 0x40DEC3),
    ("oglSetGetterValueInt", 0x40DED0, 0x40E122),
    ("oglSetGetterValueEnum", 0x40E130, 0x40E149),
    ("oglSetGetterValueFloat", 0x40E150, 0x40E3AF),
    ("oglGetInternal", 0x40E3B0, 0x40E901),
)
ADDRESSES = {name: start for name, start, _ in FUNCTIONS}
CONVERTERS = FUNCTIONS[:6]
SETTERS = FUNCTIONS[8:12]
VALUE_SIZE = 676
VALUE_OFFSET = 0x148
CONTEXT_SIZE = VALUE_OFFSET + VALUE_SIZE
INDEXED_TOKENS = {0x0DE1, 0x0B03, 0x8069, 0x8078, 0x8088, 0x8089, 0x808A, 0x889A}
PUBLIC_SETTERS = (
    ("glClearColor", 0x414910, 0x4149B0),
    ("glClearColorx", 0x4149B0, 0x414A51),
    ("glClearDepthf", 0x414A60, 0x414AD3),
    ("glClearDepthx", 0x414AE0, 0x414AFA),
    ("glClearStencil", 0x414B00, 0x414B2F),
    ("glColorMask", 0x4160A0, 0x416172),
    ("glDepthMask", 0x416180, 0x4161DC),
    ("glStencilMask", 0x4161E0, 0x416225),
    ("glActiveTexture", 0x416230, 0x4162C6),
    ("glClientActiveTexture", 0x4171A0, 0x417211),
    ("glPixelStorei", 0x41AE10, 0x41AEB3),
    ("glMatrixMode", 0x4162D0, 0x416359),
    ("glHint", 0x414720, 0x41484A),
)


def _buffer(data):
    return C.create_string_buffer(data, len(data))


def _words(values):
    return struct.pack("<" + "I" * len(values), *values)


def _context(values, active=0, client=0, error=0):
    result = bytearray(CONTEXT_SIZE)
    struct.pack_into("<I", result, 4, error)
    result[0x63] = active
    result[0x64] = client
    result[VALUE_OFFSET:] = values
    return bytes(result)


def _configure(lib):
    for name, _, _ in CONVERTERS:
        function = getattr(lib, name)
        function.argtypes = [C.c_void_p, C.c_void_p, C.c_int, C.c_int]
        function.restype = None
    lib.oglSetDefaultGetterValues.argtypes = [C.c_void_p]
    lib.oglSetDefaultGetterValues.restype = None
    lib.diff_gl_getter_info.argtypes = [C.c_uint32, C.POINTER(C.c_uint32)]
    lib.diff_gl_getter_info.restype = C.c_int
    lib.diff_gl_getter_set.argtypes = [C.c_int, C.c_int, C.c_uint32, C.c_void_p, C.c_void_p]
    lib.diff_gl_getter_set.restype = C.c_int
    lib.diff_gl_getter_get.argtypes = [C.c_uint32, C.c_int, C.c_int, C.c_int,
                                      C.c_void_p, C.c_void_p, C.c_int]
    lib.diff_gl_getter_get.restype = C.c_int
    lib.diff_gl_public_set.argtypes = [C.c_int, C.POINTER(C.c_uint32),
                                       C.POINTER(C.c_uint32), C.c_void_p,
                                       C.POINTER(C.c_uint32)]
    lib.diff_gl_public_set.restype = None


def _metadata(suite):
    oracle = suite.oracle
    oracle.reset()
    tokens = struct.unpack("<154H", oracle.read(0x471D18, 308))
    converter_ids = {start: i for i, (_, start, _) in enumerate(CONVERTERS)}
    metadata = {}
    probes = list(tokens) + [0, 1, 0xFFFF, 0xFFFFFFFF]
    probes += [token + 0x10000 for token in tokens[:8]]
    probes += [token + 1 for token in tokens if token + 1 not in tokens][:24]
    for token in probes:
        oracle.reset()
        pointer = oracle.call(ADDRESSES["oglFindGetterInfo"], [token])
        expected = None
        if pointer:
            offset, n, stride, converter = struct.unpack("<4I", oracle.read(pointer, 16))
            expected = (offset, n, stride, converter_ids[converter])
            metadata[token] = expected
        output = (C.c_uint32 * 4)()
        found = suite.lib.diff_gl_getter_info(token, output)
        actual = tuple(output) if found == 1 else None if found == 0 else ("unknown converter",)
        suite.compare("oglFindGetterInfo", f"token={token:#x}", actual, expected)
    return tokens, metadata


def _conversions(suite):
    integer_edges = [0, 1, 0xFFFFFFFF, 0x7FFFFFFF, 0x80000000,
                     0xFFFF, 0x10000, 0xFFFF0000, 0xFFFFFF, 0x1000001]
    float_edges = [0, 0x80000000, 1, 0x80000001, 0x007FFFFF, 0x00800000,
                   0x3EAAAAAB, 0x3F000000, 0x3F7FFFFF, 0x3F800000,
                   0x3F800001, 0xBF800000, 0x4EFFFFFF, 0x4F000000,
                   0xCF000000, 0x7F7FFFFF, 0xFF7FFFFF, 0x7F800000,
                   0xFF800000, 0x7FC00001, 0xFFC00001]
    for kind, (name, address, _) in enumerate(CONVERTERS):
        if kind == 2:
            values = list(range(256))
        else:
            values = (integer_edges if kind < 2 else float_edges).copy()
            values += [suite.rng.getrandbits(32) for _ in range(suite.samples)]
        for batch in range(0, len(values), 64):
            chunk = values[batch:batch + 64]
            data = bytes(chunk) if kind == 2 else _words(chunk)
            for output_type in range(4):
                oracle = suite.oracle
                oracle.reset()
                input_pointer = oracle.alloc(len(data), data)
                sentinel = b"\xA5" * (16 + 4 * len(chunk) + 16)
                output_pointer = oracle.alloc(len(sentinel), sentinel)
                oracle.call(address, [output_pointer + 16, input_pointer, len(chunk), output_type])
                expected = oracle.read(output_pointer, len(sentinel))
                native_input = _buffer(data)
                native_output = _buffer(sentinel)
                getattr(suite.lib, name)(C.byref(native_output, 16), native_input,
                                         len(chunk), output_type)
                suite.compare(name, f"type={output_type}, input={data.hex()}",
                              native_output.raw, expected)


def _get(suite, token, values, active, client, initial_error=0, null=False):
    oracle = suite.oracle
    oracle.reset()
    context_pointer = oracle.alloc(CONTEXT_SIZE, _context(values, active, client, initial_error))
    native_values = _buffer(values)
    for output_type in range(4):
        sentinel = b"\xA5" * 80
        output_pointer = oracle.alloc(len(sentinel), sentinel)
        native_output = _buffer(sentinel)
        oracle.call(ADDRESSES["oglGetInternal"],
                    [context_pointer, token, 0 if null else output_pointer + 16, output_type])
        expected = (struct.unpack("<I", oracle.read(context_pointer + 4, 4))[0],
                    oracle.read(output_pointer, len(sentinel)))
        error = suite.lib.diff_gl_getter_get(token, output_type, active, client, native_values,
                                             None if null else C.byref(native_output, 16), initial_error)
        suite.compare("oglGetInternal",
                      f"token={token:#x}, type={output_type}, active={active}, client={client}, "
                      f"initial_error={initial_error}, null={null}",
                      (error, native_output.raw), expected)


def _states(suite, tokens, metadata):
    oracle = suite.oracle
    oracle.reset()
    pointer = oracle.alloc(VALUE_SIZE, b"\xA5" * VALUE_SIZE)
    oracle.call(ADDRESSES["oglSetDefaultGetterValues"], [pointer])
    defaults = oracle.read(pointer, VALUE_SIZE)
    native_defaults = _buffer(b"\xA5" * VALUE_SIZE)
    suite.lib.oglSetDefaultGetterValues(native_defaults)
    suite.compare("oglSetDefaultGetterValues", "complete state", native_defaults.raw, defaults)

    for token in tokens:
        _, count, stride, converter = metadata[token]
        offsets = (0, 1) if token in INDEXED_TOKENS else (0,)
        for offset in offsets:
            if stride == 1:
                kind = 0
                values = [suite.rng.getrandbits(32) for _ in range(count)]
            elif converter in (4, 5):
                kind = 3
                values = [struct.unpack("<I", struct.pack("<f", suite.rng.uniform(-2, 2)))[0]
                          for _ in range(count)]
            else:
                kind = 1 + (token & 1)
                values = [suite.rng.getrandbits(32) for _ in range(count)]
            data = _words(values)
            oracle.reset()
            context_pointer = oracle.alloc(CONTEXT_SIZE, _context(defaults))
            input_pointer = oracle.alloc(len(data), data)
            name, address, _ = SETTERS[kind]
            oracle.call(address, [context_pointer, offset, token, input_pointer])
            expected = oracle.read(context_pointer + VALUE_OFFSET, VALUE_SIZE)
            native_values = _buffer(defaults)
            native_input = _buffer(data)
            result = suite.lib.diff_gl_getter_set(kind, offset, token, native_input, native_values)
            suite.compare(name, f"token={token:#x}, offset={offset}, input={data.hex()}",
                          (result, native_values.raw), (0, expected))
            _get(suite, token, expected, offset, offset)
    for token in (0, 0xFFFFFFFF, 0x0B00):
        for error in (0, 3):
            _get(suite, token, defaults, 0, 0, error, null=token == 0x0B00)


def _public_setters(suite):
    defaults = suite.oracle.read(0x471A74, VALUE_SIZE)
    float_bits = [0, 0x80000000, 0x3EAAAAAB, 0x3F000000, 0x3F7FFFFF,
                  0x3F800000, 0x3F800001, 0xBF800000, 0x40000000,
                  0x7F7FFFFF, 0xFF7FFFFF]
    integer_bits = [0, 1, 255, 256, 0xFFFF, 0x10000, 0xFFFFFFFF,
                    0x7FFFFFFF, 0x80000000, 0xFFFF0000]
    for operation, (name, address, _) in enumerate(PUBLIC_SETTERS):
        if operation in (0, 2):
            cases = [[value] * 4 for value in float_bits]
        elif operation in (8, 9):
            cases = [[value, 0, 0, 0] for value in (0, 0x84BF, 0x84C0, 0x84C1, 0x84C2, 0xFFFFFFFF)]
            # Also select a valid unit while the current matrix mode is not texture.
            cases.append([0x84C0, 0, 0, 0])
        elif operation == 10:
            cases = [[token, value, 0, 0] for token in (0x0D05, 0x0CF5, 0)
                     for value in (0, 1, 2, 3, 4, 8, 9, 0xFFFFFFFF)]
        elif operation == 11:
            cases = [[value, 0, 0, 0] for value in (0x1700, 0x1701, 0x1702, 0x8840, 0, 0xFFFFFFFF)]
        elif operation == 12:
            cases = [[token, mode, 0, 0]
                     for token in (0x0C50, 0x0C51, 0x0C52, 0x0C54, 0x8192, 0)
                     for mode in (0x1100, 0x1101, 0x1102, 0)]
        else:
            cases = [[value] * 4 for value in integer_bits]
        for _ in range(min(suite.samples, 32)):
            cases.append([suite.rng.getrandbits(32) for _ in range(4)])
        for index, inputs in enumerate(cases):
            control = [3 if index % 7 == 6 else 0, index % 13, index % 9,
                       index % 2, (index // 2) % 2, 4, 4, int(index % 13 != 12)]
            context = bytearray(_context(defaults, control[3], control[4], control[0]))
            context[0x61] = control[1]
            context[0x62] = control[2]
            context[0x65] = control[5]
            context[0x66] = control[6]
            oracle = suite.oracle
            oracle.reset()
            core_pointer = oracle.alloc(0x3000)
            struct.pack_into("<I", context, 0, core_pointer)
            context_pointer = oracle.alloc(CONTEXT_SIZE, bytes(context))
            oracle.watch(core_pointer, 0x3000, "core")
            oracle.watch(context_pointer, CONTEXT_SIZE, "context")
            hook = oracle.add_hook(0x4085B0,
                                   lambda machine: machine.return_from_call(context_pointer if control[7] else 0))
            try:
                oracle.call(address, inputs)
            finally:
                oracle.remove_hook(hook)
            core = oracle.read(core_pointer, 0xB0)
            result_context = oracle.read(context_pointer, CONTEXT_SIZE)
            expected = [struct.unpack_from("<I", core, offset)[0] for offset in (0xA4, 0xA0, 0xA8)]
            expected += [core[offset] for offset in (0x1F, 0x0A, 0x2D, 0x27)]
            expected += [struct.unpack_from("<I", result_context, 4)[0]]
            expected += list(result_context[0x61:0x67])
            expected += list(struct.unpack("<169I", result_context[VALUE_OFFSET:]))
            expected += [core[0x3D + 8], core[0x3D + 17]]
            for offset, size in ((0xA4, 4), (0xA0, 4), (0xA8, 4), (0x1F, 1),
                                 (0x0A, 1), (0x2D, 1), (0x27, 1), (0x45, 1), (0x4E, 1)):
                oracle.readback(core_pointer + offset, size)
            oracle.readback(context_pointer + 4, 4)
            oracle.readback(context_pointer + 0x61, 6)
            oracle.readback(context_pointer + VALUE_OFFSET, VALUE_SIZE)
            native_output = (C.c_uint32 * 185)()
            suite.lib.diff_gl_public_set(operation, (C.c_uint32 * 4)(*inputs),
                                         (C.c_uint32 * 8)(*control), _buffer(defaults), native_output)
            suite.compare(name, f"args={inputs}, context={control}", tuple(native_output), tuple(expected))


def run(suite):
    if suite.arch == "arm":
        from differential_gl_matrix import run as matrix_sequences
        matrix_sequences(suite)
        from differential_gl_state import run as state_sequences
        state_sequences(suite)
        return
    for name, start, end in FUNCTIONS + PUBLIC_SETTERS:
        suite.register(name, start, end)
    _configure(suite.lib)
    tokens, metadata = _metadata(suite)
    _conversions(suite)
    _states(suite, tokens, metadata)
    _public_setters(suite)
    from differential_gl_matrix import run as matrix_sequences
    matrix_sequences(suite)
    from differential_gl_state import run as state_sequences
    state_sequences(suite)
