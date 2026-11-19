"""The program's own dataclass, whose fields are the contract (D-46).

    @dataclass
    class Server:
        host: str = setting("server.host", "0.0.0.0")
        port: int = setting("server.port", 8080, range=(1, 65535), flags=flag.RESTART)

    @dataclass
    class Settings:
        server: Server = section(Server)
        debug: bool = setting("app.debug", False)

    cfg = define(Builder.app("myapp"), Settings).create()
    settings = read(cfg, Settings)          # settings.server.port

Each field's key and default are written once, and the same list
defines the contract; a field the configuration leaves unset has its
default, and one the contract refuses is refused at create(), with the
key and the place, before any of it is read.
"""

from __future__ import annotations

import dataclasses
import typing
from typing import Any, TypeVar

from ._config import Builder, Config, flag

T = TypeVar("T")

_META = "configforge"


@dataclasses.dataclass(frozen=True)
class _Spec:
    key: str
    range: tuple[float, float] | None
    flags: int
    doc: str | None


def setting(key: str, default: Any, *, range: tuple[float, float] | None = None,
            flags: int = 0, doc: str | None = None) -> Any:
    """A dataclass field read from ``key``, with its default and, for a
    number, its permitted ``range`` (inclusive). A ``flag.SENSITIVE``
    field is left out of the dataclass's repr, so printing or logging a
    settings object does not show the secret."""
    return dataclasses.field(default=default, repr=not (flags & flag.SENSITIVE),
                             metadata={_META: _Spec(key, range, flags, doc)})


def section(cls: type[Any]) -> Any:
    """A field that is itself a settings dataclass."""
    return dataclasses.field(default_factory=cls, metadata={_META: None})


def _fields(cls: type[Any]):
    if not dataclasses.is_dataclass(cls):
        raise TypeError(f"{cls!r} is not a dataclass")
    hints = typing.get_type_hints(cls)
    for f in dataclasses.fields(cls):
        if _META not in f.metadata:
            raise TypeError(f"{cls.__name__}.{f.name}: use setting() or section()")
        yield f, hints[f.name], f.metadata[_META]


def define(builder: Builder, cls: type[Any]) -> Builder:
    """Every field of ``cls``, nested sections included, defined in
    ``builder``."""
    for f, tp, spec in _fields(cls):
        if spec is None:
            define(builder, tp)
            continue
        lo, hi = spec.range if spec.range else (0, 0)
        d = f.default
        if tp is bool:
            builder.define_bool(spec.key, bool(d), spec.flags)
        elif tp is int:
            builder.define_int(spec.key, int(d), int(lo), int(hi), spec.flags)
        elif tp is float:
            builder.define_real(spec.key, float(d), float(lo), float(hi), spec.flags)
        elif tp is str:
            builder.define_text(spec.key, None if d is None else str(d), spec.flags)
        else:
            raise TypeError(f"{cls.__name__}.{f.name}: a setting is bool, int, float or str, not {tp!r}")
    return builder


def read(cfg: Config, cls: type[T]) -> T:
    """An instance of ``cls`` filled from ``cfg``; an unset field has its
    default."""
    values = {}
    for f, tp, spec in _fields(cls):
        if spec is None:
            values[f.name] = read(cfg, tp)
        else:
            values[f.name] = cfg.get(spec.key, f.default, type=tp)
    return cls(**values)
