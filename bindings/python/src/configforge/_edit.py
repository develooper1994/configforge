"""Create, update and delete as one change (cf_edit.h).

    with cf.edit(cfg, "store", note="admin, 2026-10-30: incident 42") as e:
        e["pool.size"] = 32
        e["log.level"] = "debug"
        del e["feature.beta"]          # the next source down supplies it

Leaving the block commits the whole change, or raises :class:`Error`
and writes nothing; an exception inside the block drops it. Or by hand:
``e = cf.edit(cfg, "store"); e.set(...); e.commit(note)``. Under a
:class:`Live`, ``live.commit(e, note)`` commits and reloads.
"""

from __future__ import annotations

import ctypes
from typing import Any, Optional

from . import _native as n
from ._config import Config, Error


class Edit:
    """A change gathered before anything is written. Commit once."""

    def __init__(self, cfg: Config, target: str, note: Optional[str] = None):
        e = n.new_error()
        self._e = n.lib.cf_edit_begin(cfg._c, target.encode("utf-8"), ctypes.byref(e))
        if not self._e:
            raise Error._from(e)
        self._note = note

    def _live(self) -> int:
        if not self._e:
            raise RuntimeError("this edit was already committed or dropped")
        return self._e

    def set(self, key: str, value: Any) -> "Edit":
        """Set ``key``: a bool, int, float or str - text is typed by the
        contract as a file's is."""
        k = key.encode("utf-8")
        if isinstance(value, bool):
            n.lib.cf_edit_set_bool(self._live(), k, int(value))
        elif isinstance(value, int):
            n.lib.cf_edit_set_int(self._live(), k, value)
        elif isinstance(value, float):
            if not n.HAVE_REAL:
                raise RuntimeError("this libconfigforge was built without reals")
            n.lib.cf_edit_set_real(self._live(), k, value)
        elif isinstance(value, str):
            n.lib.cf_edit_set_text(self._live(), k, value.encode("utf-8"))
        else:
            raise TypeError("a setting is bool, int, float or str, not %r" % type(value))
        return self

    def unset(self, key: str) -> "Edit":
        """Delete ``key`` from the target, so the next source down supplies it."""
        n.lib.cf_edit_unset(self._live(), key.encode("utf-8"))
        return self

    __setitem__ = set

    def __delitem__(self, key: str) -> None:
        self.unset(key)

    def __len__(self) -> int:
        return n.lib.cf_edit_count(self._e) if self._e else 0

    def commit(self, note: Optional[str] = None) -> None:
        """Check and write the whole change, or raise :class:`Error`,
        writing nothing. Spends the edit."""
        e = n.new_error()
        raw, self._e = self._live(), None
        if not n.lib.cf_edit_commit(raw, n.encode(note if note is not None else self._note),
                                    ctypes.byref(e)):
            raise Error._from(e)

    def abort(self) -> None:
        """Drop the edit; nothing is written."""
        if self._e:
            n.lib.cf_edit_abort(self._e)
            self._e = None

    def __enter__(self) -> "Edit":
        return self

    def __exit__(self, kind, value, tb) -> None:
        if kind is None:
            self.commit()
        else:
            self.abort()

    def __del__(self) -> None:
        self.abort()


def edit(cfg: Config, target: str, note: Optional[str] = None) -> Edit:
    """An :class:`Edit` of ``target``, a writable source of ``cfg``."""
    return Edit(cfg, target, note)
