#!/usr/bin/env python3
"""Check lighting's freestanding double helpers against host IEEE-754."""
import ctypes
import math
import pathlib
import random
import struct
import subprocess
import tempfile

source = pathlib.Path(__file__).resolve().parents[1] / "symbian/aeabi_softfp.cpp"

def bits(value):
    return struct.unpack("<Q", struct.pack("<d", value))[0]

def number(value):
    return struct.unpack("<d", struct.pack("<Q", value))[0]

with tempfile.TemporaryDirectory() as tmp:
    library = pathlib.Path(tmp) / "softfp.so"
    subprocess.run(["g++", "-O2", "-shared", "-fPIC", "-DPF_SOFTFP_PREFIX=pf_test_",
                    str(source), "-o", str(library)], check=True)
    lib = ctypes.CDLL(str(library))
    lib.pf_test_ddiv.argtypes = [ctypes.c_uint64, ctypes.c_uint64]
    lib.pf_test_ddiv.restype = ctypes.c_uint64
    lib.pf_test_dcmpgt.argtypes = [ctypes.c_uint64, ctypes.c_uint64]
    lib.pf_test_dcmpgt.restype = ctypes.c_int
    special = [0.0, -0.0, 1.0, -1.0, math.inf, -math.inf, math.nan,
               5e-324, -5e-324, 2.2250738585072014e-308, 1.7976931348623157e308]
    pairs = [(bits(a), bits(b)) for a in special for b in special]
    rng = random.Random(0xAEB1)
    pairs += [(rng.getrandbits(64), rng.getrandbits(64)) for _ in range(50000)]
    for a, b in pairs:
        fa, fb = number(a), number(b)
        assert lib.pf_test_dcmpgt(a, b) == int(fa > fb)
        if fb == 0:
            expected = math.nan if fa == 0 or math.isnan(fa) else math.copysign(math.inf, math.copysign(1, fa)*math.copysign(1, fb))
        else:
            expected = fa / fb
        actual = lib.pf_test_ddiv(a, b)
        assert (math.isnan(number(actual)) if math.isnan(expected) else actual == bits(expected)), (hex(a), hex(b), hex(actual), expected)
    print(f"Double division/comparison: {len(pairs)} IEEE-754 pairs passed")
