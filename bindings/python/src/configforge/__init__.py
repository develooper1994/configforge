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
    ANY_WRITER,
    OPTIONAL,
    TRUSTED_DIR,
    UNKNOWN_IGNORE,
    UNKNOWN_REJECT,
    UNKNOWN_WARN,
    Builder,
    Config,
    Error,
    flag,
    rank,
)
from ._edit import Edit, edit
from ._live import Live, Mode, Reload, Status
from ._native import lib as _lib
from ._settings import define, read, section, setting

__all__ = [
    "ANY_WRITER",
    "OPTIONAL",
    "TRUSTED_DIR",
    "UNKNOWN_IGNORE",
    "UNKNOWN_REJECT",
    "UNKNOWN_WARN",
    "Builder",
    "Config",
    "Edit",
    "Error",
    "Live",
    "Mode",
    "Reload",
    "Status",
    "define",
    "edit",
    "flag",
    "has_feature",
    "rank",
    "read",
    "section",
    "setting",
    "version",
]

__version__ = "1.9.0"


def version() -> str:
    """The library's version, and the commit it was built from."""
    return f"{_lib.cf_version().decode()} ({_lib.cf_build_id().decode()})"


def has_feature(name: str) -> bool:
    """Whether the library was built with a part: "toml", "yaml", "real"..."""
    return _lib.cf_has_feature(name.encode()) != 0
