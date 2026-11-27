#!/usr/bin/env python3
"""Every public function whose result is the only way to know it failed is
CF_NODISCARD (K-9c, D-223).

A function declared CF_API in lib/*/include/configforge/*.h needs the mark
when it returns cf_bool or cf_code, or a pointer and its name contains
_new, _create, _open, _load, _build, _dup or _from. Names starting cf_get_,
cf_has_ or cf_is_ (queries that answer, with a fallback) are left out, and
so are the names in scripts/check/nodiscard.txt, one `name|reason` a line.

Usage: nodiscard.py <root>. Prints each unmarked declaration as
<header>:<line>: <name>: not CF_NODISCARD and exits 1 if there is any.
"""

import glob
import os
import re
import sys

POINTER_WORDS = ("_new", "_create", "_open", "_load", "_build", "_dup", "_from")
QUERY_PREFIXES = ("cf_get_", "cf_has_", "cf_is_")
START = re.compile(r"^\s*(CF_NODISCARD\s+)?CF_API\b")
DECL = re.compile(r"CF_API\s+(.*?)\b([A-Za-z_]\w*)\s*\(", re.DOTALL)


def excused(root):
    names = {}
    path = os.path.join(root, "scripts", "check", "nodiscard.txt")
    if not os.path.exists(path):
        return names
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            name, _, reason = line.partition("|")
            if not reason.strip():
                print(f"{path}: {name}: no reason", file=sys.stderr)
                sys.exit(1)
            names[name.strip()] = reason.strip()
    return names


def needs_mark(rtype, name):
    if name.startswith(QUERY_PREFIXES):
        return False
    words = rtype.replace("*", " * ").split()
    if "*" in words:
        return any(w in name for w in POINTER_WORDS)
    return bool(words) and words[-1] in ("cf_bool", "cf_code")


def main():
    root = sys.argv[1] if len(sys.argv) > 1 else "."
    skip = excused(root)
    bad = 0
    pattern = os.path.join(root, "lib", "*", "include", "configforge", "*.h")
    for path in sorted(glob.glob(pattern)):
        with open(path, encoding="utf-8") as f:
            lines = f.read().split("\n")
        for i, line in enumerate(lines):
            m = START.match(line)
            if not m:
                continue
            text = line[line.index("CF_API") :]
            j = i
            while "(" not in text and j + 1 < len(lines):
                j += 1
                text += " " + lines[j].strip()
            d = DECL.match(text)
            if not d:
                continue
            rtype, name = d.group(1), d.group(2)
            if m.group(1) or name in skip or not needs_mark(rtype, name):
                continue
            rel = os.path.relpath(path, root).replace(os.sep, "/")
            print(f"{rel}:{i + 1}: {name}: not CF_NODISCARD")
            bad += 1
    if bad:
        print(f"{bad} declaration(s) not CF_NODISCARD")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
