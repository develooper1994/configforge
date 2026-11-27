#!/usr/bin/env python3
"""Every public load or write function has an allocation sweep, or says why
not (K-9b, D-237, docs/Safety.md "Memory that runs out").

A public name (cmake/configforge.symbols and the experimental list; the same
names scripts/check/guarantees.py reads) whose guarantee is strong or basic
and whose name holds _create, _load, _decode, _write, _commit, _replace,
_derive, _reload, _emit, _open or _save needs a line in
lib/config/tests/oom_paths.txt:

  <name>|<file>:<function>   the case: <function> is defined in a file of
                             lib/*/tests/ called <file> and calls cf_oom_sweep
  <name>|-|<reason>          it allocates nothing, or is covered through
                             another name: the reason says which
  <name>|known: <why>        the sweep found a defect not yet fixed; 2.0.0 does
                             not release while one remains

Usage: oom.py <root>   exit 1 and one line a fault
"""

import glob
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import guarantees

LIST = "lib/config/tests/oom_paths.txt"
HOLDS = (
    "_create",
    "_load",
    "_decode",
    "_write",
    "_commit",
    "_replace",
    "_derive",
    "_reload",
    "_emit",
    "_open",
    "_save",
)


def read_list(root):
    path = os.path.join(root, *LIST.split("/"))
    rows = []
    if not os.path.exists(path):
        return rows, [f"{LIST}: missing"]
    faults = []
    with open(path, encoding="utf-8") as f:
        for no, line in enumerate(f.read().split("\n"), 1):
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            if "|" not in line:
                faults.append(f"{LIST}:{no}: '{line}': name|where needed")
                continue
            name, where = line.split("|", 1)
            rows.append((no, name.strip(), where.strip()))
    return rows, faults


def function_body(text, function):
    """The text of `function`'s definition, or None."""
    m = re.search(r"^[A-Za-z_][\w \t\*]*\b" + re.escape(function) + r"\s*\([^;{]*\)\s*\{", text, re.MULTILINE)
    if not m:
        return None
    end = text.find("\n}", m.end())
    return text[m.start() : end if end >= 0 else len(text)]


def find_file(root, name):
    return sorted(glob.glob(os.path.join(root, "lib", "*", "tests", name)))


def main(argv):
    root = argv[0] if argv else "."
    names = guarantees.public_names(root)
    found = guarantees.declarations(root)
    rows, faults = read_list(root)
    have = {}
    for no, name, where in rows:
        if name in have:
            faults.append(f"{LIST}:{no}: {name}: listed twice")
        have[name] = (no, where)
        if name not in found:
            faults.append(f"{LIST}:{no}: {name}: not a public name")

    needed = []
    for name in names:
        if name not in found:
            continue
        word = found[name][2]
        if word in ("strong", "basic") and any(h in name for h in HOLDS):
            needed.append(name)
    for name in needed:
        if name not in have:
            faults.append(f"{name}: no sweep case or excuse")

    for name, (no, where) in sorted(have.items(), key=lambda kv: kv[1][0]):
        if where.startswith("known:"):
            if not where[6:].strip():
                faults.append(f"{LIST}:{no}: {name}: known: needs its why")
            continue
        if where.startswith("-|"):
            if not where[2:].strip():
                faults.append(f"{LIST}:{no}: {name}: an excuse needs its reason")
            continue
        if ":" not in where:
            faults.append(f"{LIST}:{no}: {name}: '{where}' is neither file:function, -|reason nor known: why")
            continue
        fname, function = where.split(":", 1)
        paths = find_file(root, fname)
        if not paths:
            faults.append(f"{name}: the case {function} is not in {fname}: no such file in lib/*/tests")
            continue
        body = None
        for p in paths:
            with open(p, encoding="utf-8", errors="replace") as f:
                body = function_body(f.read(), function)
            if body is not None:
                break
        if body is None:
            faults.append(f"{name}: the case {function} is not in {fname}")
        elif "cf_oom_sweep(" not in body:
            faults.append(f"{name}: {where} does not call cf_oom_sweep")

    for f in faults:
        print(f)
    if faults:
        print(f"{len(faults)} fault(s)")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
