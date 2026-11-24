"""Native conservative block budgets for long oracle calls."""

import ctypes as C

import unicorn
from unicorn import arm_const as ARM


class NativeInstructionBudget:
    def __init__(self, oracle, library, error_type=RuntimeError):
        self.oracle = oracle
        self.error_type = error_type
        self.running = False
        self.used = 0
        self.handle = None
        self.unicorn_library = unicorn.Uc.hook_add.__globals__["uclib"]
        self.callback = library.differential_budget_callback
        self.create = library.differential_budget_create
        self.create.argtypes = [C.c_void_p, C.c_uint, C.c_void_p, C.c_int]
        self.create.restype = C.c_void_p
        self.begin = library.differential_budget_begin
        self.begin.argtypes, self.begin.restype = [C.c_void_p, C.c_uint64], None
        self.end = library.differential_budget_end
        self.end.argtypes, self.end.restype = [C.c_void_p], C.c_int
        self.read_used = library.differential_budget_used
        self.read_used.argtypes, self.read_used.restype = [C.c_void_p], C.c_uint64
        self.destroy = library.differential_budget_destroy
        self.destroy.argtypes, self.destroy.restype = [C.c_void_p], None
        self.state = self.create(C.cast(self.unicorn_library.uc_emu_stop, C.c_void_p),
                                 2 if oracle.arch == "arm" else 1,
                                 C.cast(self.unicorn_library.uc_reg_read, C.c_void_p),
                                 ARM.UC_ARM_REG_CPSR)
        if not self.state:
            raise error_type("could not allocate the native instruction budget")
        try:
            self.handle = oracle.add_native_hook(unicorn.UC_HOOK_BLOCK, self.callback,
                                                  self.state, error_type=error_type)
        except BaseException:
            self.destroy(self.state)
            self.state = None
            raise
        # Lazy installation can follow earlier translations, including guest
        # primitives outside the original image's coverage-hook range.
        try:
            oracle.uc.ctl_flush_tb()
        except BaseException:
            self.close()
            raise

    def run(self, address, end, limit):
        if self.running or self.state is None:
            raise self.error_type("native instruction budget is unavailable or already running")
        if not isinstance(limit, int) or not 0 < limit <= 0xFFFFFFFFFFFFFFFF:
            raise ValueError("instruction budget must be a positive 64-bit integer")
        self.running = True
        self.begin(self.state, limit)
        try:
            # The native block hook is active before disabling Unicorn's slower
            # per-instruction counter. The suite's wall-clock alarm stays armed.
            self.oracle.uc.emu_start(address, end, count=0)
        finally:
            status = self.end(self.state)
            self.used = self.read_used(self.state)
            self.running = False
        if status < 0:
            raise self.error_type(f"could not stop emulation at the instruction budget: {-status}")
        if status:
            raise self.error_type(f"instruction budget exhausted at entry 0x{address:x} "
                                  f"(conservative block limit {limit})")

    def close(self):
        if self.running:
            raise self.error_type("cannot close a running instruction budget")
        if self.handle is not None:
            self.oracle.remove_hook(self.handle)
            self.handle = None
        if self.state is not None:
            self.destroy(self.state)
            self.state = None
