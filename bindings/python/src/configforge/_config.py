"""Builder, Config and Error: the configuration itself."""

from __future__ import annotations

import collections.abc
import ctypes
import logging
import os
import sys
from typing import Any, Dict, Iterator, List, Mapping, Optional, Sequence, Tuple, Union

from . import _native as n

Scalar = Union[bool, int, float, str]


class Error(Exception):
    """What went wrong, with the key and the source: the C library's one
    error. ``str(error)`` is the message a person reads."""

    def __init__(self, code: int, name: str, key: str, source: str, message: str):
        super().__init__(message)
        self.code = code
        self.name = name
        self.key = key
        self.source = source
        self.message = message

    @classmethod
    def _from(cls, e: n.cf_error) -> "Error":
        return cls(
            e.code,
            n.decode(n.lib.cf_code_name(e.code)),
            n.decode(e.key),
            n.decode(e.source),
            n.decode(e.message),
        )


class rank:
    """Source ranks: lower wins."""

    OVERRIDE = -10
    CLI = 0
    ENV = 10
    USER_FILE = 20
    SYS_FILE = 30
    DEFAULT = 40


class flag:
    """Definition flags."""

    REQUIRED = 1 << 0
    SENSITIVE = 1 << 1
    DEPRECATED = 1 << 2
    TRANSIENT = 1 << 3
    RESTART = 1 << 4


OPTIONAL = 1 << 16
"""A file or directory that may be absent."""

UNKNOWN_IGNORE, UNKNOWN_WARN, UNKNOWN_REJECT = 0, 1, 2


class _Keep:
    """What the C side borrows - strings, pair arrays, argv - kept alive
    with every configuration that may point into it."""

    __slots__ = ("items",)

    def __init__(self) -> None:
        self.items: List[Any] = []

    def s(self, v: Optional[str]) -> Optional[bytes]:
        b = n.encode(v)
        if b is not None:
            self.items.append(b)
        return b

    def argv(self, items: Sequence[str]):
        arr = (ctypes.c_char_p * (len(items) + 1))(*[self.s(i) for i in items], None)
        self.items.append(arr)
        return arr

    def pairs(self, items: Mapping[str, Any]):
        arr = (n.cf_pair * max(len(items), 1))()
        for i, (k, v) in enumerate(items.items()):
            arr[i].key = self.s(k)
            arr[i].value = self.s(_text_of(v))
        self.items.append(arr)
        return arr, len(items)


def _text_of(v: Any) -> str:
    """A Python value as the text a source holds: the contract types it."""
    if isinstance(v, bool):
        return "true" if v else "false"
    return str(v)


def log_sink(logger: Optional[logging.Logger] = None):
    """A cf_log function that hands each event to ``logger`` (by default
    the ``configforge`` logger): its level is the event's - the library's
    levels are Python's numbers - and the event's name, key, source and
    code travel as the record's ``cf_event``, ``cf_key``, ``cf_source``
    and ``cf_code`` attributes."""
    target = logger or logging.getLogger("configforge")

    def emit(_user, ev):
        e = ev.contents
        try:
            target.log(e.level, "%s: %s", n.decode(e.event), n.decode(e.message),
                       extra={"cf_event": n.decode(e.event), "cf_key": n.decode(e.key) or None,
                              "cf_source": n.decode(e.source) or None, "cf_code": e.code})
        except Exception:  # never across the C frame
            pass

    return n.LOG_FN(emit)


class Builder:
    """A configuration being assembled. Each call adds a source or a
    definition and returns the builder; :meth:`create` resolves it all,
    once, and is where every error is raised."""

    def __init__(self, _raw: Optional[int] = None, _keep: Optional[_Keep] = None):
        self._keep = _keep or _Keep()
        self._b = _raw if _raw is not None else n.lib.cf_build(None)

    @classmethod
    def app(cls, name: str, argv: Optional[Sequence[str]] = None) -> "Builder":
        """The usual sources for a program: its command line, the
        environment (``NAME_`` prefix) and the defaults."""
        keep = _Keep()
        args = list(sys.argv if argv is None else argv)
        raw = n.lib.cf_app_builder(keep.s(name), len(args), keep.argv(args))
        return cls(raw, keep)

    def _live(self) -> int:
        if not self._b:
            raise RuntimeError("this builder was already consumed")
        return self._b

    def cli(self, name: str, rank: int, argv: Sequence[str]) -> "Builder":
        args = list(argv)
        n.lib.cf_builder_cli(self._live(), self._keep.s(name), rank, len(args),
                             self._keep.argv(args))
        return self

    def env(self, name: str, rank: int, prefix: str,
            environ: Optional[Mapping[str, str]] = None) -> "Builder":
        """Variables with ``prefix``: the process's own, or ``environ``."""
        env = os.environ if environ is None else environ
        items = ["%s=%s" % (k, v) for k, v in env.items()]
        n.lib.cf_builder_env(self._live(), self._keep.s(name), rank, self._keep.s(prefix),
                             self._keep.argv(items))
        return self

    def file(self, name: str, rank: int, path: str, format: Optional[str] = None,
             flags: int = 0) -> "Builder":
        n.lib.cf_builder_file(self._live(), self._keep.s(name), rank, self._keep.s(path),
                              self._keep.s(format), flags)
        return self

    def dir(self, name: str, rank: int, path: str, format: Optional[str] = None,
            flags: int = 0) -> "Builder":
        """A drop-in directory, conf.d style."""
        n.lib.cf_builder_dir(self._live(), self._keep.s(name), rank, self._keep.s(path),
                             self._keep.s(format), flags)
        return self

    def dotenv(self, name: str, rank: int, path: str, prefix: Optional[str] = None,
               flags: int = 0) -> "Builder":
        n.lib.cf_builder_dotenv(self._live(), self._keep.s(name), rank, self._keep.s(path),
                                self._keep.s(prefix), flags)
        return self

    def secrets(self, name: str, rank: int, path: str, prefix: Optional[str] = None,
                flags: int = 0) -> "Builder":
        """A directory of secrets, one file a key."""
        n.lib.cf_builder_secrets(self._live(), self._keep.s(name), rank, self._keep.s(path),
                                 self._keep.s(prefix), flags)
        return self

    def memory(self, name: str, rank: int, values: Mapping[str, Any]) -> "Builder":
        """Values given by the program, as text the contract types."""
        arr, count = self._keep.pairs(values)
        n.lib.cf_builder_memory(self._live(), self._keep.s(name), rank, arr, count)
        return self

    def define_bool(self, key: str, default: bool = False, flags: int = 0) -> "Builder":
        n.lib.cf_builder_bool(self._live(), self._keep.s(key), int(default), flags)
        return self

    def define_int(self, key: str, default: int = 0, min: int = 0, max: int = 0,
                   flags: int = 0) -> "Builder":
        """An integer; ``min == max`` means no range."""
        n.lib.cf_builder_int(self._live(), self._keep.s(key), default, min, max, flags)
        return self

    def define_text(self, key: str, default: Optional[str] = None,
                    flags: int = 0) -> "Builder":
        n.lib.cf_builder_text(self._live(), self._keep.s(key), self._keep.s(default), flags)
        return self

    def define_real(self, key: str, default: float = 0.0, min: float = 0.0,
                    max: float = 0.0, flags: int = 0) -> "Builder":
        if not n.HAVE_REAL:
            raise RuntimeError("this libconfigforge was built without reals")
        n.lib.cf_builder_real(self._live(), self._keep.s(key), default, min, max, flags)
        return self

    def open(self, prefix: str) -> "Builder":
        """Exempt a prefix from unknown-key rejection: a plugin's settings."""
        n.lib.cf_builder_open(self._live(), self._keep.s(prefix))
        return self

    def unknown(self, policy: int) -> "Builder":
        n.lib.cf_builder_unknown(self._live(), policy)
        return self

    def log(self, logger: Optional[logging.Logger] = None,
            level: int = logging.INFO) -> "Builder":
        """Send what is not an error - an unknown key under
        ``UNKNOWN_WARN``, a deprecated key in use, a value fallen back
        from - to ``logger`` (the ``configforge`` logger by default),
        from ``level`` up. Configurations derived from this one log
        there too."""
        fn = log_sink(logger)
        self._keep.items.append(fn)  # the C side keeps the pointer
        n.lib.cf_builder_log(self._live(), fn, None, level)
        return self

    def create(self) -> "Config":
        """Resolve everything, once. Raises :class:`Error`, which names the
        key and the place."""
        e = n.new_error()
        raw = n.lib.cf_builder_create(self._live(), ctypes.byref(e))
        self._b = None  # consumed either way
        if not raw:
            raise Error._from(e)
        return Config(raw, self._keep)

    def _release(self) -> Tuple[int, _Keep]:
        """The C builder, uncreated, and what it borrows: for cf_live."""
        raw, self._b = self._live(), None
        return raw, self._keep

    def __del__(self) -> None:
        if getattr(self, "_b", None):
            n.lib.cf_builder_abort(self._b)


class _Node:
    """``cfg.server`` where ``server`` is a table: a prefix, read on."""

    __slots__ = ("_cfg", "_prefix")

    def __init__(self, cfg: "Config", prefix: str):
        self._cfg = cfg
        self._prefix = prefix

    def __getattr__(self, name: str) -> Any:
        return self._cfg._at(self._prefix + "." + name)

    def __getitem__(self, name: str) -> Any:
        return self._cfg[self._prefix + "." + name]

    def __iter__(self) -> Iterator[str]:
        return iter(self._cfg.children(self._prefix))

    def __repr__(self) -> str:
        return "<configforge table %r>" % self._prefix


class Config(collections.abc.Mapping):
    """A resolved configuration: an immutable snapshot whose reads cannot
    fail, and which can say why each value is what it is.

    A read-only Mapping of dotted keys to values, so everything that takes
    a mapping takes it - ``dict(cfg)``, ``**cfg``, ``json.dumps(dict(cfg))``.
    Three ways to read, by taste::

        cfg.server.port                   # attributes; a table is a node
        cfg["server.port"]                # KeyError when absent
        cfg.get("server.port", type=int)  # None when absent or another type

    An attribute that is also a method's name (``get``, ``keys``) is read
    with ``cfg["get"]``; methods win, as they do on any Python object. So
    that no key a program reads this way is hidden by a later release, the
    methods and properties below are the whole set for 1.x: what 1.x adds
    comes as a function of the ``configforge`` module that takes a Config
    (D-51).
    """

    def __init__(self, raw: int, keep: Any):
        self._c = raw
        self._keep = keep

    def __del__(self) -> None:
        if getattr(self, "_c", None):
            n.lib.cf_config_free(self._c)

    # Reading -----------------------------------------------------------

    def _read(self, key: str) -> Tuple[bool, Any]:
        k = key.encode("utf-8")
        t = n.lib.cf_type_of(self._c, k)
        if t == n.TYPE_BOOL:
            b = ctypes.c_int()
            n.lib.cf_try_bool(self._c, k, ctypes.byref(b))
            return True, b.value != 0
        if t == n.TYPE_INT:
            i = ctypes.c_int64()
            n.lib.cf_try_int(self._c, k, ctypes.byref(i))
            return True, i.value
        if t == n.TYPE_REAL and n.HAVE_REAL:
            r = ctypes.c_double()
            n.lib.cf_try_real(self._c, k, ctypes.byref(r))
            return True, r.value
        if t == n.TYPE_TEXT:
            p = ctypes.POINTER(ctypes.c_char)()
            ln = ctypes.c_size_t()
            n.lib.cf_try_text(self._c, k, ctypes.byref(p), ctypes.byref(ln))
            return True, ctypes.string_at(p, ln.value).decode("utf-8", "replace")
        return False, None

    def _is_table(self, prefix: str) -> bool:
        it = n.cf_iter()
        p = prefix.encode("utf-8")  # the iterator points into it
        n.lib.cf_iter_keys(ctypes.byref(it), self._c, p)
        return n.lib.cf_iter_next(ctypes.byref(it)) != 0

    def _at(self, key: str) -> Any:
        found, v = self._read(key)
        if found:
            return v
        if self._is_table(key):
            return _Node(self, key)
        raise AttributeError("no key or table %r in this configuration" % key)

    def __getattr__(self, name: str) -> Any:
        if name.startswith("_"):
            raise AttributeError(name)
        return self._at(name)

    def __getitem__(self, key: str) -> Scalar:
        found, v = self._read(key)
        if not found:
            raise KeyError(key)
        return v

    def __contains__(self, key: object) -> bool:
        return isinstance(key, str) and self._read(key)[0]

    def __iter__(self) -> Iterator[str]:
        return iter(self.keys())

    def get(self, key: str, default: Any = None, *, type: Optional[type] = None) -> Any:
        """The value of ``key``, as ``Mapping.get`` gives it; ``default``
        when it is absent or, with ``type``, of another type. An ``int``
        asked for as ``float`` is another type: nothing is converted behind
        the contract's back."""
        found, v = self._read(key)
        if not found:
            return default
        if type is not None and not (isinstance(v, type) and
                                     (type is not int or not isinstance(v, bool))):
            return default
        return v

    def _walk(self, prefix: Optional[str], children: bool) -> List[str]:
        it = n.cf_iter()
        p = n.encode(prefix)
        (n.lib.cf_iter_children if children else n.lib.cf_iter_keys)(
            ctypes.byref(it), self._c, p)
        out = []
        while n.lib.cf_iter_next(ctypes.byref(it)):
            s, ln = (it.name, it.namelen) if children else (it.key, it.keylen)
            out.append(ctypes.string_at(s, ln).decode("utf-8", "replace"))
        return out

    def keys(self, prefix: Optional[str] = None) -> List[str]:  # type: ignore[override]
        """Every key under a prefix (a segment boundary: "server" is not
        "servername")."""
        return self._walk(prefix, False)

    def children(self, prefix: Optional[str] = None) -> List[str]:
        """Each immediate child of a prefix, once."""
        return self._walk(prefix, True)

    def items(self, prefix: Optional[str] = None) -> List[Tuple[str, Scalar]]:  # type: ignore[override]
        """(key, value) for every key under a prefix."""
        return [(k, self[k]) for k in self.keys(prefix)]

    def to_dict(self, prefix: Optional[str] = None) -> Dict[str, Any]:
        """The keys under ``prefix`` as nested dictionaries."""
        out: Dict[str, Any] = {}
        cut = len(prefix) + 1 if prefix else 0
        for k, v in self.items(prefix):
            node = out
            parts = k[cut:].split(".")
            for part in parts[:-1]:
                node = node.setdefault(part, {})
            node[parts[-1]] = v
        return out

    def __len__(self) -> int:
        return len(self.keys())

    # Asking why -------------------------------------------------------

    def explain(self, key: str) -> str:
        """Every candidate for one key, the winner, and why."""
        k = key.encode("utf-8")
        return n.render(lambda buf, cap: n.lib.cf_explain(self._c, k, buf, cap))

    def inspect(self) -> str:
        return n.render(lambda buf, cap: n.lib.cf_inspect(self._c, buf, cap))

    def changes_since(self, before: "Config") -> str:
        """What changed since ``before``, a line a key, secrets redacted."""
        return n.render(lambda buf, cap: n.lib.cf_diff_text(before._c, self._c, buf, cap))

    def schema(self, title: str) -> str:
        """The contract as a JSON Schema, for editors."""
        t = title.encode("utf-8")
        return n.render(lambda buf, cap: n.lib.cf_schema(self._c, t, buf, cap))

    @property
    def digest(self) -> int:
        return n.lib.cf_digest(self._c)

    @property
    def changed(self) -> bool:
        """Whether a file it was read from has moved on: a hint."""
        return n.lib.cf_changed(self._c) != 0

    # Changing ------------------------------------------------------------

    def derive(self, source: str, values: Mapping[str, Any]) -> "Config":
        """A new snapshot with ``values`` applied as a source named
        ``source``; this one is unchanged. Raises :class:`Error` when the
        contract refuses a value."""
        keep = _Keep()
        arr, count = keep.pairs(values)
        e = n.new_error()
        raw = n.lib.cf_derive(self._c, keep.s(source), arr, count, ctypes.byref(e))
        if not raw:
            raise Error._from(e)
        return Config(raw, (self._keep, keep))

    def __repr__(self) -> str:
        return "<configforge.Config %d keys, digest %016x>" % (len(self), self.digest)
