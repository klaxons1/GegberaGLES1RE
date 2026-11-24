"""Use the differential library's native fault handler in standalone probes."""
import ctypes
import os
from pathlib import Path


def install(seconds=30):
    root = Path(__file__).resolve().parents[1]
    path = os.environ.get("GERBERA_DIFFERENTIAL_LIBRARY", root / "build/differential/libgerbera_diff.so")
    library = ctypes.CDLL(str(Path(path).resolve()))
    library.differential_install_signal_handlers.argtypes = []
    library.differential_install_signal_handlers.restype = ctypes.c_int
    if library.differential_install_signal_handlers() != 0:
        raise RuntimeError("could not install native fault handlers; refusing to run")
    library.differential_set_timeout.argtypes = [ctypes.c_uint]
    library.differential_set_timeout.restype = None
    library.differential_set_timeout(seconds)
    return library
