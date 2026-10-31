"""A configuration that reloads itself, in a mode the program chooses
(cf_live, D-48).

    live = Live(Mode.AUTO, lambda: Builder().file("app.ini", rank.USER_FILE, "app.ini"))
    cfg = live.get()            # any thread; one request, one snapshot
    live.poll()                 # the loop that drives reloads

A build that raises, or a contract that refuses, leaves the running
snapshot serving; the call raises :class:`Error` saying why, with the
build's own exception as its cause.
"""

from __future__ import annotations

import ctypes
import dataclasses
import enum
import logging
from typing import Any, Callable, List, Optional

from . import _native as n
from ._config import Builder, Config, Error, log_sink


class Mode(enum.IntEnum):
    MANUAL = 0  #: only reload() rebuilds
    APPROVE = 1  #: a change is built and waits for approve()
    AUTO = 2  #: a change is built and swapped in


class Reload(enum.IntEnum):
    UNCHANGED = 0
    SWAPPED = 1
    PENDING = 2


@dataclasses.dataclass(frozen=True)
class Status:
    """What a health page says about the reloads."""

    generation: int
    reloads: int  #: rebuilds tried, from poll, reload and wait
    failures: int  #: of those, refused or failed
    last: Optional[Reload]  #: what the last one did; None when it failed
    pending: bool  #: a change waits for approve()
    last_error: Optional[Error]  #: why the last failure failed


class Live:
    """The reloads. :meth:`get` may be called from any thread; the rest
    belong to the one thread that drives them."""

    def __init__(self, mode: Mode, build: Callable[[], Builder],
                 logger: Optional[logging.Logger] = None, level: int = logging.INFO):
        """``logger``, when given, receives each reload's result and the
        events of every configuration built - see :meth:`Builder.log`."""
        self._build = build
        self._kept: List[Any] = []  # what every builder handed over borrows
        self._failure: Optional[BaseException] = None
        # Held for the life of the Live: the C side keeps the pointers.
        self._fn = n.BUILD_FN(self._trampoline)
        self._log = log_sink(logger) if logger is not None else n.LOG_FN()
        o = n.cf_live_options()
        o.size = ctypes.sizeof(n.cf_live_options)
        o.mode = int(mode)
        o.build = self._fn
        o.log = self._log
        o.log_min = level
        e = n.new_error()
        self._l = n.lib.cf_live_open_with(ctypes.byref(o), ctypes.byref(e))
        if not self._l:
            raise self._error(e)

    def _trampoline(self, _user: Any) -> Optional[int]:
        try:
            b = self._build()
            raw, keep = b._release()
        except BaseException as exc:  # never across the C frame
            self._failure = exc
            return None
        # The library copies every name and path, and reads the rest only
        # while it creates: an earlier build's strings can go. Its log
        # functions cannot - a snapshot built with one may still call it.
        for old in self._kept:
            old.items[:] = [x for x in old.items if isinstance(x, n.LOG_FN)]
        self._kept[:] = [k for k in self._kept if k.items]
        self._kept.append(keep)
        return raw

    def _error(self, e: n.cf_error) -> Error:
        err = Error._from(e)
        err.__cause__, self._failure = self._failure, None
        return err

    def _result(self, r: int, e: n.cf_error) -> Reload:
        if r < 0:
            raise self._error(e)
        return Reload(r)

    def get(self) -> Config:
        """The current snapshot. A reader keeps the one it got."""
        return Config(n.lib.cf_live_get(self._l), self._kept)

    def poll(self) -> Reload:
        """Rebuild if a file changed (APPROVE and AUTO)."""
        e = n.new_error()
        return self._result(n.lib.cf_live_poll(self._l, ctypes.byref(e)), e)

    def reload(self) -> Reload:
        """Rebuild now, in any mode."""
        e = n.new_error()
        return self._result(n.lib.cf_live_reload(self._l, ctypes.byref(e)), e)

    def wait(self, timeout: float) -> Reload:
        """Wait up to ``timeout`` seconds for a file to change, then poll."""
        e = n.new_error()
        r = n.lib.cf_live_wait(self._l, int(timeout * 1000), ctypes.byref(e))
        return self._result(r, e)

    @property
    def pending(self) -> Optional[Config]:
        """The snapshot waiting for approval."""
        p = n.lib.cf_live_pending(self._l)
        if not p:
            return None
        n.lib.cf_config_ref(p)
        return Config(p, self._kept)

    def approve(self) -> Reload:
        e = n.new_error()
        return self._result(n.lib.cf_live_approve(self._l, ctypes.byref(e)), e)

    def reject(self) -> None:
        n.lib.cf_live_reject(self._l)

    def commit(self, edit, note: Optional[str] = None) -> Reload:
        """Commit ``edit`` (an :class:`Edit`) and reload, in one step; raise
        :class:`Error`, with nothing written, when the commit is refused."""
        raw, edit._e = edit._live(), None
        e = n.new_error()
        r = n.lib.cf_live_commit(self._l, raw, n.encode(note if note is not None else edit._note),
                                 ctypes.byref(e))
        return self._result(r, e)

    @property
    def status(self) -> Status:
        """The reloads' status; safe from any thread."""
        s = n.cf_live_status()
        s.size = ctypes.sizeof(n.cf_live_status)
        n.lib.cf_live_status_get(self._l, ctypes.byref(s))
        e = n.new_error()
        failed = n.lib.cf_live_last_error(self._l, ctypes.byref(e)) != 0
        return Status(
            s.generation, s.reloads, s.failures,
            Reload(s.last) if s.last >= 0 else None,
            bool(s.pending),
            Error._from(e) if failed else None,
        )

    @property
    def generation(self) -> int:
        """How many snapshots readers have been given."""
        return n.lib.cf_live_generation(self._l)

    @property
    def restarts(self) -> List[str]:
        """The keys defined RESTART whose new value waits for a restart."""
        return n.render(lambda buf, cap: n.lib.cf_live_restarts(self._l, buf, cap)).split()

    def close(self) -> None:
        if getattr(self, "_l", None):
            n.lib.cf_live_close(self._l)
            self._l = None

    def __enter__(self) -> "Live":
        return self

    def __exit__(self, *exc: Any) -> None:
        self.close()

    def __del__(self) -> None:
        self.close()
