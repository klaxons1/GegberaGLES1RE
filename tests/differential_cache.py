"""Stateful JIT-cache comparisons, including the complete arena and entry lists."""

import ctypes as C
import struct

from differential_raster import buffer, words


class Entry(C.Structure):
    pass


Entry._fields_ = [("signature", C.c_ubyte * 40), ("gradientMask", C.c_uint32),
                 ("timeStamp", C.c_uint32), ("dataOffset", C.c_int32),
                 ("dataSize", C.c_int32), ("prev", C.POINTER(Entry)),
                 ("next", C.POINTER(Entry))]


class Cache(C.Structure):
    _fields_ = [("firstUsed", C.POINTER(Entry)), ("firstFree", C.POINTER(Entry)),
                ("firstUnused", C.POINTER(Entry)), ("entries", C.POINTER(Entry)),
                ("arena", C.c_void_p), ("arenaSize", C.c_int32),
                ("entryCount", C.c_int32), ("timeStamp", C.c_uint32)]


TARGETS = {
    "arm": {
        "grCacheChangeList": (0x1F3CC, 0x1F410),
        "grCacheMergeFreeBlock": (0x1F410, 0x1F4C4),
        "grCacheFindFreeEntry": (0x1F4C4, 0x1F4F0),
        "grCacheInit": (0x1F4F0, 0x1F590),
        "grCacheSetArena": (0x1F590, 0x1F5B0),
        "grCacheFind": (0x1F5B0, 0x1F620),
        "grCacheInsert": (0x1F620, 0x1F87C),
    },
    "x86": {
        "grCacheMatchSignatures": (0x439A80, 0x439B1A),
        "grCacheGetOldestUsed": (0x439B20, 0x439BE3),
        "grCacheGetFreeMemory": (0x439BF0, 0x439C31),
        "grCacheSelfCheck": (0x439C40, 0x439C45),
        "grCacheChangeList": (0x439C50, 0x439D87),
        "grCacheMergeFreeBlock": (0x439D90, 0x439ED9),
        "grCacheRemove": (0x439EE0, 0x439FC1),
        "grCacheDefragment": (0x439FD0, 0x43A1E8),
        "grCacheFindFreeEntry": (0x43A1F0, 0x43A231),
        "grCacheInit": (0x43A240, 0x43A469),
        "grCacheFind": (0x43A470, 0x43A556),
        "grCacheInsert": (0x43A560, 0x43A8E7),
    },
}

ARENA_BYTES = 256
GUARD_BYTES = 16
ORIGINAL_ENTRY_BYTES = 64


def pointer(value):
    return C.cast(value, C.c_void_p).value or 0


def relative(value, base, stride=1):
    if not value:
        return -1
    offset = value - base
    if offset % stride:
        raise ValueError("unaligned cache pointer")
    return offset // stride


class Fixture:
    def __init__(self, suite, count):
        self.suite, self.count = suite, count
        self.oracle, self.lib = suite.oracle, suite.lib
        self.targets = TARGETS[suite.arch]
        self.cache = Cache()
        self.entries = (Entry * max(1, count))()
        self.arena_data = suite.rng.randbytes(ARENA_BYTES + 2 * GUARD_BYTES)
        self.arena = buffer(self.arena_data)
        self.native_arena = C.addressof(self.arena) + GUARD_BYTES
        self.oracle.reset()
        self.original_cache = self.oracle.alloc(32)
        self.original_entries = self.oracle.alloc(max(1, count) * ORIGINAL_ENTRY_BYTES)
        self.original_arena = self.oracle.alloc(len(self.arena_data), self.arena_data)
        self.oracle.watch(self.original_cache, 32, "complete pipeline cache")
        if count:
            self.oracle.watch(self.original_entries, count * ORIGINAL_ENTRY_BYTES,
                              "complete cache entries")
        self.oracle.watch(self.original_arena, len(self.arena_data), "complete cache arena")
        self.original_arena += GUARD_BYTES
        self.signature = self.oracle.alloc(40)
        self.data = self.oracle.alloc(ARENA_BYTES + 1)
        self.mask = self.oracle.alloc(4)
        self.lib.grCacheInit(C.byref(self.cache), self.native_arena, self.entries,
                             ARENA_BYTES, count)
        self.oracle.call(self.targets["grCacheInit"][0],
                         [self.original_cache, self.original_arena, self.original_entries,
                          ARENA_BYTES, count])
        self.compare("grCacheInit", count)

    def canonical_native(self):
        cache = self.cache
        index = lambda p: relative(pointer(p), C.addressof(self.entries), C.sizeof(Entry))
        state = (relative(cache.arena, self.native_arena), index(cache.entries),
                 index(cache.firstUsed), index(cache.firstFree), index(cache.firstUnused),
                 cache.timeStamp, cache.arenaSize, cache.entryCount)
        entries = tuple((bytes(e.signature), e.gradientMask, e.dataOffset, e.dataSize,
                         e.timeStamp, index(e.prev), index(e.next))
                        for e in self.entries[:self.count])
        return state, entries, bytes(self.arena)

    def canonical_original(self):
        read = self.oracle.readback
        state = list(struct.unpack("<8I", read(self.original_cache, 32)))
        state[0] = relative(state[0], self.original_arena)
        for i in range(1, 5):
            state[i] = relative(state[i], self.original_entries, ORIGINAL_ENTRY_BYTES)
        data = read(self.original_entries, self.count * ORIGINAL_ENTRY_BYTES)
        entries = []
        for i in range(self.count):
            entry = data[i * ORIGINAL_ENTRY_BYTES:(i + 1) * ORIGINAL_ENTRY_BYTES]
            fields = list(struct.unpack("<6I", entry[40:]))
            fields[-2:] = [relative(p, self.original_entries, ORIGINAL_ENTRY_BYTES)
                           for p in fields[-2:]]
            entries.append((entry[:40], *fields))
        return (tuple(state), tuple(entries),
                read(self.original_arena - GUARD_BYTES, len(self.arena_data)))

    def compare(self, name, inputs, native=None, original=None):
        self.suite.compare(name, inputs, (native, self.canonical_native()),
                           (original, self.canonical_original()))

    def stamp(self, value):
        self.cache.timeStamp = value
        self.oracle.uc.mem_write(self.original_cache + 20, words((value,)))

    def operation(self, insert, key, size=0):
        signature = bytes((key,)) * 40
        native_signature = buffer(signature)
        self.oracle.uc.mem_write(self.signature, signature)
        if insert:
            name = "grCacheInsert"
            data = self.suite.rng.randbytes(size)
            self.oracle.uc.mem_write(self.data, data)
            mask = self.suite.rng.getrandbits(32)
            result = self.lib.grCacheInsert(C.byref(self.cache), native_signature, mask,
                                            buffer(data), size)
            expected = self.oracle.call(self.targets[name][0],
                                        [self.original_cache, self.signature, mask, self.data, size])
            extra = ()
        else:
            name = "grCacheFind"
            mask = C.c_uint32(0xA5A5A5A5)
            self.oracle.uc.mem_write(self.mask, words((mask.value,)))
            result = self.lib.grCacheFind(C.byref(self.cache), native_signature, C.byref(mask))
            expected = self.oracle.call(self.targets[name][0],
                                        [self.original_cache, self.signature, self.mask])
            extra = (mask.value, struct.unpack("<I", self.oracle.readback(self.mask, 4))[0])
        self.compare(name, (self.count, insert, key, size),
                     (relative(result, self.native_arena), extra[:1]),
                     (relative(expected, self.original_arena), extra[1:]))

    def replace_arena(self):
        old_arena, old_original = self.arena, self.original_arena
        data = self.suite.rng.randbytes(len(self.arena_data))
        self.arena = buffer(data)
        self.native_arena = C.addressof(self.arena) + GUARD_BYTES
        self.original_arena = self.oracle.alloc(len(data), data) + GUARD_BYTES
        self.oracle.watch(self.original_arena - GUARD_BYTES, len(data), "replacement cache arena")
        self.lib.grCacheSetArena(C.byref(self.cache), self.native_arena, ARENA_BYTES)
        self.oracle.call(self.targets["grCacheSetArena"][0],
                         [self.original_cache, self.original_arena, ARENA_BYTES])
        self.compare("grCacheSetArena", self.count)
        self.suite.compare_bytes("grCacheSetArena previous arena", self.count,
                                 bytes(old_arena), self.oracle.readback(
                                     old_original - GUARD_BYTES, len(self.arena_data)))


def run(suite):
    targets = TARGETS[suite.arch]
    for name, bounds in targets.items():
        suite.register(name, *bounds)
    for name, args, result in (
            ("grCacheInit", [C.c_void_p] * 3 + [C.c_int] * 2, None),
            ("grCacheSetArena", [C.c_void_p] * 2 + [C.c_int], None),
            ("grCacheInsert", [C.c_void_p, C.c_void_p, C.c_uint32, C.c_void_p, C.c_int], C.c_void_p),
            ("grCacheFind", [C.c_void_p] * 3, C.c_void_p)):
        function = getattr(suite.lib, name)
        function.argtypes, function.restype = args, result
    # Invalidation has no effect on the byte-addressed emulated arena.
    hook = suite.oracle.add_hook(0x233DC if suite.arch == "arm" else 0x44B8A0,
                                 lambda oracle: oracle.return_from_call(0))
    try:
        for stamp in (0x7FFFFFFC, 0xFFFFFFFC):
            fixture = Fixture(suite, 4)
            fixture.stamp(stamp)
            for key in range(3):
                fixture.operation(True, key, 32)
            fixture.operation(False, 0)
            fixture.operation(True, 3, 32)
            for key in range(4):
                fixture.operation(False, key)
        for count in (0, 1, 2, 4, 8):
            fixture = Fixture(suite, count)
            fixture.operation(False, 0)
            fixture.operation(True, 0, ARENA_BYTES + 1)
            # Several live entries straddle both timestamp boundaries. Repeated
            # keys, zero-length entries, full arenas and undersized entry arrays
            # exercise the original eviction and free-list policies.
            for stamp in (0, 0x7FFFFFFD, 0xFFFFFFFD):
                fixture.stamp(stamp)
                for key, size in enumerate((32, 64, 0, 48, 128, 16, 64, 256)):
                    fixture.operation(True, key, size)
                    fixture.operation(False, key // 2)
            for _ in range(min(suite.samples, 128)):
                fixture.operation(suite.rng.randrange(3) != 0, suite.rng.randrange(8),
                                  suite.rng.choice((0, 4, 16, 32, 64, 96, 128, 192, 256)))
            if suite.arch == "arm" and count:
                fixture.replace_arena()
                fixture.operation(False, 0)
                fixture.operation(True, 7, 64)
    finally:
        suite.oracle.remove_hook(hook)
