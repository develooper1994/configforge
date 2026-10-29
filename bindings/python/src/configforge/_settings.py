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
from typing import Any, Optional, Tuple, Type, TypeVar

from ._config import Builder, Config

T = TypeVar("T")

_META = "configforge"


@dataclasses.dataclass(frozen=True)
class _Spec:
    key: str
    range: Optional[Tuple[float, float]]
    flags: int
    doc: Optional[str]


def setting(key: str, default: Any, *, range: Optional[Tuple[float, float]] = None,
            flags: int = 0, doc: Optional[str] = None) -> Any:
    """A dataclass field read from ``key``, with its default and, for a
    number, its permitted ``range`` (inclusive)."""
    return dataclasses.field(default=default,
                             metadata={_META: _Spec(key, range, flags, doc)})


def section(cls: Type[Any]) -> Any:
    """A field that is itself a settings dataclass."""
    return dataclasses.field(default_factory=cls, metadata={_META: None})


def _fields(cls: Type[Any]):
    if not dataclasses.is_dataclass(cls):
        raise TypeError("%r is not a dataclass" % (cls,))
    hints = typing.get_type_hints(cls)
    for f in dataclasses.fields(cls):
        if _META not in f.metadata:
            raise TypeError("%s.%s: use setting() or section()" % (cls.__name__, f.name))
        yield f, hints[f.name], f.metadata[_META]


def define(builder: Builder, cls: Type[Any]) -> Builder:
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
            raise TypeError("%s.%s: a setting is bool, int, float or str, not %r"
                            % (cls.__name__, f.name, tp))
    return builder


def read(cfg: Config, cls: Type[T]) -> T:
    """An instance of ``cls`` filled from ``cfg``; an unset field has its
    default."""
    values = {}
    for f, tp, spec in _fields(cls):
        if spec is None:
            values[f.name] = read(cfg, tp)
        else:
            values[f.name] = cfg.get(spec.key, tp, f.default)
    return cls(**values)
