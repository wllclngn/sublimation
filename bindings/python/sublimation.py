"""
sublimation Python binding via ctypes.

Loads libsublimation.so and exposes sort functions directly.
"""
import ctypes
import os
from pathlib import Path

# find the shared library
_lib_paths = [
    Path(__file__).parent.parent.parent / "build" / "libsublimation.so",
    Path("/usr/local/lib/libsublimation.so"),
    Path("/usr/lib/libsublimation.so"),
]

_lib = None
for p in _lib_paths:
    if p.exists():
        _lib = ctypes.CDLL(str(p))
        break

if _lib is None:
    raise ImportError("libsublimation.so not found. Run ./install.py build first.")

# bind sublimation_i64(int64_t *arr, size_t n)
_lib.sublimation_i64.argtypes = [ctypes.POINTER(ctypes.c_int64), ctypes.c_size_t]
_lib.sublimation_i64.restype = None

# bind sublimation_api_version()
_lib.sublimation_api_version.argtypes = []
_lib.sublimation_api_version.restype = ctypes.c_int


def sort_i64(arr):
    """Sort a list of integers in-place using sublimation."""
    n = len(arr)
    c_arr = (ctypes.c_int64 * n)(*arr)
    _lib.sublimation_i64(c_arr, n)
    arr[:] = list(c_arr)
    return arr


def sort_i64_array(c_arr, n):
    """Sort a ctypes int64 array in-place."""
    _lib.sublimation_i64(c_arr, n)


def api_version():
    return _lib.sublimation_api_version()
