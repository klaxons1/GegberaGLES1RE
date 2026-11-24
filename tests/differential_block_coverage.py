"""Native block bookkeeping with Capstone branch classification."""

import ctypes as C

import capstone
import unicorn
from unicorn import arm_const as ARM


_Classifier = C.CFUNCTYPE(C.c_int, C.c_uint64, C.c_uint32, C.c_uint, C.POINTER(C.c_uint64))


class NativeBlockCoverage:
    def __init__(self, oracle, library, error_type=RuntimeError,
                 block_capacity=65536, edge_capacity=131072):
        self.oracle = oracle
        self.library = library
        self.error_type = error_type
        self.failure = None
        self.state = None
        self.hooks = set()
        self.classifier = _Classifier(self._classify)
        self.unicorn_library = unicorn.Uc.hook_add.__globals__["uclib"]
        self.callback = library.differential_block_coverage_callback
        self.create = library.differential_block_coverage_create
        self.create.argtypes = [C.c_uint64, C.c_uint64, C.c_void_p, C.c_int,
                               C.c_void_p, _Classifier, C.c_size_t, C.c_size_t]
        self.create.restype = C.c_void_p
        self.destroy = library.differential_block_coverage_destroy
        self.destroy.argtypes, self.destroy.restype = [C.c_void_p], None
        self.reset_native = library.differential_block_coverage_reset
        self.reset_native.argtypes, self.reset_native.restype = [C.c_void_p], None
        self.error_native = library.differential_block_coverage_error
        self.error_native.argtypes = [C.c_void_p, C.POINTER(C.c_int)]
        self.error_native.restype = C.c_int
        self.count_native = library.differential_block_coverage_count
        self.count_native.argtypes = [C.c_void_p, C.c_uint]
        self.count_native.restype = C.c_size_t
        self.export_native = library.differential_block_coverage_export
        self.export_native.argtypes = [C.c_void_p, C.c_uint, C.POINTER(C.c_uint64), C.c_size_t]
        self.export_native.restype = C.c_size_t
        self.state = self.create(
            oracle.base, oracle.code_end,
            C.cast(self.unicorn_library.uc_reg_read, C.c_void_p),
            ARM.UC_ARM_REG_CPSR if oracle.arch == "arm" else -1,
            C.cast(self.unicorn_library.uc_emu_stop, C.c_void_p), self.classifier,
            block_capacity, edge_capacity)
        if not self.state:
            raise error_type("could not allocate native block coverage")

    def _classify(self, address, size, thumb, output):
        try:
            oracle = self.oracle
            key = (address, size, bool(thumb)) if oracle.arch == "arm" else (address, size)
            branch = oracle.branch_cache.get(key)
            if branch is None:
                decoder = oracle.thumb_decoder if thumb else oracle.decoder
                instructions = list(decoder.disasm(oracle.read(address, size), address))
                last = instructions[-1] if instructions else None
                conditional = bool(last and oracle.conditional(last))
                if oracle.arch == "arm":
                    indirect = bool(last and last.id == capstone.arm.ARM_INS_BX and not conditional)
                else:
                    indirect = bool(last and last.mnemonic == "jmp" and
                                    last.operands[0].type != capstone.x86.X86_OP_IMM)
                branch = (last.address if conditional or indirect else None, indirect)
                oracle.branch_cache[key] = branch
            address, indirect = branch
            if address is None:
                return 0
            output[0] = address
            return 2 if indirect else 1
        except BaseException as error:
            # ctypes callbacks cannot propagate exceptions into emu_start.
            # Tell C to stop immediately and rethrow through check().
            self.failure = error
            return -1

    def install_hook(self):
        self.check()
        handle = self.oracle.add_native_hook(unicorn.UC_HOOK_BLOCK, self.callback, self.state,
            self.oracle.base, self.oracle.code_end - 1, error_type=self.error_type)
        self.hooks.add(handle)
        # A path translated while tracing was suspended otherwise keeps its
        # old hook-free translation after this callback is reinstalled.
        self.oracle.uc.ctl_remove_cache(self.oracle.base, self.oracle.code_end)
        return handle

    def reset_trace(self):
        self.check()
        self.reset_native(self.state)

    def check(self):
        if self.failure is not None:
            raise self.error_type("native block classifier failed") from self.failure
        detail = C.c_int()
        error = self.error_native(self.state, C.byref(detail))
        if error:
            reasons = {1: "block outside original image", 2: "register read failed",
                       3: "invalid block classification", 4: "block capacity exceeded",
                       5: "edge capacity exceeded"}
            raise self.error_type(f"native block coverage: {reasons.get(error, 'invalid state')} "
                                  f"(detail {detail.value})")

    def edge_count(self):
        self.check()
        return self.count_native(self.state, 1) + self.count_native(self.state, 2)

    def snapshot(self):
        self.check()
        result = []
        for kind in range(3):
            count = self.count_native(self.state, kind)
            output = (C.c_uint64 * (count * (2 if kind else 1)))()
            if self.export_native(self.state, kind, output, count) != count:
                raise self.error_type("native block coverage export failed")
            result.append(set(zip(output[::2], output[1::2])) if kind else set(output))
        return tuple(result)

    def materialize(self):
        for destination, values in zip((self.oracle.blocks, self.oracle.edges, self.oracle.indirect_edges),
                                       self.snapshot()):
            destination.update(values)

    def close(self):
        if self.state:
            # Suspended hooks may already have been removed by the driver.
            # Keep the classifier and C state alive until every live hook is gone.
            for handle in self.hooks:
                if self.oracle.uc._callbacks.get(handle) is self.callback:
                    self.oracle.remove_hook(handle)
                if getattr(self.oracle, "block_hook", None) == handle:
                    self.oracle.block_hook = None
            self.hooks.clear()
            self.destroy(self.state)
            self.state = None
