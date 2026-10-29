"""ConfigForge for Python: configuration whose every value can say why it
is what it is - which source won, which lost, and where each was set.

    import configforge as cf

    cfg = (cf.Builder.app("myapp")
           .file("app.ini", cf.rank.USER_FILE, "app.ini", flags=cf.OPTIONAL)
           .define_int("server.port", 8080, 1, 65535)
           .create())
    print(cfg.server.port)
    print(cfg.explain("server.port"))

The C library does the work, through ctypes over its shared build, so
a Python program and a C one built from the same contract resolve,
refuse and explain identically. Standard library only.
"""

from ._config import (
    OPTIONAL,
    UNKNOWN_IGNORE,
    UNKNOWN_REJECT,
    UNKNOWN_WARN,
    Builder,
    Config,
    Error,
    flag,
    rank,
)
from ._live import Live, Mode, Reload
from ._native import lib as _lib
from ._settings import define, read, section, setting

__all__ = [
    "Builder", "Config", "Error", "Live", "Mode", "Reload",
    "rank", "flag", "OPTIONAL", "UNKNOWN_IGNORE", "UNKNOWN_WARN", "UNKNOWN_REJECT",
    "setting", "section", "define", "read", "version", "has_feature",
]

__version__ = "0.4.0"


def version() -> str:
    """The library's version, and the commit it was built from."""
    return "%s (%s)" % (_lib.cf_version().decode(), _lib.cf_build_id().decode())


def has_feature(name: str) -> bool:
    """Whether the library was built with a part: "toml", "yaml", "real"..."""
    return _lib.cf_has_feature(name.encode()) != 0
