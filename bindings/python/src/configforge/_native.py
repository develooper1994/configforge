"""The C library, loaded and declared: the one module that knows ctypes.

Found as $CONFIGFORGE_LIB, then beside this package, then by the
platform's own search (ctypes.util.find_library). It must be a shared
build (-DBUILD_SHARED_LIBS=ON). The struct layouts follow the build's
limits, which are checked against the library's own (D-45) before the
first call that fills an error.
"""

from __future__ import annotations

import ctypes
import ctypes.util
import os
import sys
from ctypes import (
    CFUNCTYPE,
    POINTER,
    Structure,
    c_char,
    c_char_p,
    c_int,
    c_int64,
    c_long,
    c_size_t,
    c_uint,
    c_uint64,
    c_void_p,
)
from typing import Optional

KEY_MAX = 256
SOURCE_MAX = 64
MESSAGE_MAX = 256


class cf_error(Structure):
    _fields_ = [
        ("size", c_size_t),
        ("code", c_int),
        ("key", c_char * KEY_MAX),
        ("source", c_char * SOURCE_MAX),
        ("message", c_char * MESSAGE_MAX),
    ]


class cf_pair(Structure):
    _fields_ = [("key", c_char_p), ("value", c_char_p)]


class cf_iter(Structure):
    _fields_ = [
        ("cfg", c_void_p),
        ("prefix", c_char_p),
        ("plen", c_size_t),
        ("pos", c_size_t),
        ("children", c_int),
        ("key", POINTER(c_char)),
        ("keylen", c_size_t),
        ("name", POINTER(c_char)),
        ("namelen", c_size_t),
        ("value", c_void_p),
    ]


class cf_live_status(Structure):
    _fields_ = [
        ("size", c_size_t),
        ("generation", c_uint64),
        ("reloads", c_uint64),
        ("failures", c_uint64),
        ("last", c_int),
        ("pending", c_int),
        ("error", cf_error),
    ]


BUILD_FN = CFUNCTYPE(c_void_p, c_void_p)

TYPE_NULL, TYPE_BOOL, TYPE_INT, TYPE_REAL, TYPE_TEXT = range(5)
OK = 0


def _candidates() -> list:
    names = {
        "win32": ["configforge.dll"],
        "darwin": ["libconfigforge.dylib"],
    }.get(sys.platform, ["libconfigforge.so"])
    here = os.path.dirname(os.path.abspath(__file__))
    found = [os.environ.get("CONFIGFORGE_LIB")]
    found += [os.path.join(here, n) for n in names]
    found.append(ctypes.util.find_library("configforge"))
    return [p for p in found if p]


def _load() -> ctypes.CDLL:
    tried = []
    for path in _candidates():
        try:
            return ctypes.CDLL(path)
        except OSError as e:
            tried.append("%s (%s)" % (path, e))
    raise ImportError(
        "configforge: no shared libconfigforge; build it with -DBUILD_SHARED_LIBS=ON "
        "and set CONFIGFORGE_LIB. Tried: " + "; ".join(tried or ["nothing"])
    )


lib = _load()


def _declare(name: str, restype, *argtypes, optional: bool = False) -> None:
    fn = getattr(lib, name, None)
    if fn is None:
        if optional:
            return
        raise ImportError("configforge: the library has no %s" % name)
    fn.restype = restype
    fn.argtypes = list(argtypes)


B, C, L = c_void_p, c_void_p, c_void_p  # builder, config, live: opaque
E = POINTER(cf_error)
ARGV = POINTER(c_char_p)

for _name, _res, _args in [
    ("cf_version", c_char_p, ()),
    ("cf_build_id", c_char_p, ()),
    ("cf_build_limit", c_size_t, (c_char_p,)),
    ("cf_has_feature", c_int, (c_char_p,)),
    ("cf_code_name", c_char_p, (c_int,)),
    ("cf_build", B, (c_void_p,)),
    ("cf_app_builder", B, (c_char_p, c_int, ARGV)),
    ("cf_builder_abort", None, (B,)),
    ("cf_builder_create", C, (B, E)),
    ("cf_builder_cli", c_int, (B, c_char_p, c_int, c_int, ARGV)),
    ("cf_builder_env", c_int, (B, c_char_p, c_int, c_char_p, ARGV)),
    ("cf_builder_file", c_int, (B, c_char_p, c_int, c_char_p, c_char_p, c_uint)),
    ("cf_builder_dir", c_int, (B, c_char_p, c_int, c_char_p, c_char_p, c_uint)),
    ("cf_builder_dotenv", c_int, (B, c_char_p, c_int, c_char_p, c_char_p, c_uint)),
    ("cf_builder_secrets", c_int, (B, c_char_p, c_int, c_char_p, c_char_p, c_uint)),
    ("cf_builder_memory", c_int, (B, c_char_p, c_int, POINTER(cf_pair), c_size_t)),
    ("cf_builder_bool", c_int, (B, c_char_p, c_int, c_uint)),
    ("cf_builder_int", c_int, (B, c_char_p, c_int64, c_int64, c_int64, c_uint)),
    ("cf_builder_text", c_int, (B, c_char_p, c_char_p, c_uint)),
    ("cf_builder_open", c_int, (B, c_char_p)),
    ("cf_builder_unknown", None, (B, c_int)),
    ("cf_config_free", None, (C,)),
    ("cf_config_ref", None, (C,)),
    ("cf_has", c_int, (C, c_char_p)),
    ("cf_type_of", c_int, (C, c_char_p)),
    ("cf_try_bool", c_int, (C, c_char_p, POINTER(c_int))),
    ("cf_try_int", c_int, (C, c_char_p, POINTER(c_int64))),
    ("cf_try_text", c_int, (C, c_char_p, POINTER(POINTER(c_char)), POINTER(c_size_t))),
    ("cf_explain", c_size_t, (C, c_char_p, c_char_p, c_size_t)),
    ("cf_inspect", c_size_t, (C, c_char_p, c_size_t)),
    ("cf_diff_text", c_size_t, (C, C, c_char_p, c_size_t)),
    ("cf_schema", c_size_t, (C, c_char_p, c_char_p, c_size_t)),
    ("cf_derive", C, (C, c_char_p, POINTER(cf_pair), c_size_t, E)),
    ("cf_digest", c_uint64, (C,)),
    ("cf_count", c_size_t, (C,)),
    ("cf_changed", c_int, (C,)),
    ("cf_iter_keys", c_int, (POINTER(cf_iter), C, c_char_p)),
    ("cf_iter_children", c_int, (POINTER(cf_iter), C, c_char_p)),
    ("cf_iter_next", c_int, (POINTER(cf_iter),)),
    ("cf_live_open", L, (c_int, BUILD_FN, c_void_p, c_void_p, c_void_p, E)),
    ("cf_live_close", None, (L,)),
    ("cf_live_get", C, (L,)),
    ("cf_live_poll", c_int, (L, E)),
    ("cf_live_reload", c_int, (L, E)),
    ("cf_live_wait", c_int, (L, c_long, E)),
    ("cf_live_pending", C, (L,)),
    ("cf_live_approve", c_int, (L, E)),
    ("cf_live_reject", None, (L,)),
    ("cf_live_generation", c_uint64, (L,)),
    ("cf_live_restarts", c_size_t, (L, c_char_p, c_size_t)),
    ("cf_live_status_get", c_int, (L, POINTER(cf_live_status))),
]:
    _declare(_name, _res, *_args)

# Present only where the library was built with reals (CF_WITH_REAL).
_declare("cf_try_real", c_int, C, c_char_p, POINTER(ctypes.c_double), optional=True)
_declare("cf_builder_real", c_int, B, c_char_p, ctypes.c_double, ctypes.c_double,
         ctypes.c_double, c_uint, optional=True)
HAVE_REAL = hasattr(lib, "cf_try_real") and lib.cf_has_feature(b"real") != 0

for _limit, _ours in (("key", KEY_MAX), ("source", SOURCE_MAX), ("message", MESSAGE_MAX)):
    _theirs = lib.cf_build_limit(_limit.encode())
    if _theirs != _ours:
        raise ImportError(
            "configforge: libconfigforge was built with CF_%s_MAX %d; this binding expects %d"
            % (_limit.upper(), _theirs, _ours)
        )


def new_error() -> cf_error:
    e = cf_error()
    e.size = ctypes.sizeof(cf_error)
    return e


def encode(s: Optional[str]) -> Optional[bytes]:
    return None if s is None else s.encode("utf-8")


def decode(b: Optional[bytes]) -> str:
    return "" if not b else b.decode("utf-8", "replace")


def render(call) -> str:
    """A text the C side writes into a buffer it measures first."""
    n = call(None, 0)
    if n == 0:
        return ""
    buf = ctypes.create_string_buffer(n + 1)
    call(buf, n + 1)
    return buf.raw[:n].decode("utf-8", "replace")
