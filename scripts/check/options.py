"""Every public options struct has one shape, and every fixed limit says where it went.

    python3 options.py <repository>

D-220: a limit is an option with a default value, never a fixed number; and
every options struct is built the same way (docs/Architecture.md, "Options
with defaults"). For every `typedef struct cf_<x>_options {` in
lib/*/include/configforge/*.h this prints
`<header>:<line>: cf_<x>_options: <problem>` and exits 1 when

  (a) no `CF_API void cf_<x>_options_init(` is declared in any
      lib/*/include header, or
  (b) the first member is not `size_t size;` (a comment is fine).

For every `#define CF_MAX_<NAME> <number>` in lib/*/include it requires a
line `CF_MAX_<NAME>|<its option, or why it is none>` in
scripts/check/options.txt: one line a macro, a missing or an extra line is
printed. Lines starting with '#' and empty lines in the list are ignored.
"""

import glob
import os
import re
import sys

STRUCT = re.compile(r"^\s*typedef\s+struct\s+(cf_[a-z0-9_]*options)\s*\{", re.MULTILINE)
MACRO = re.compile(r"^\s*#\s*define\s+(CF_MAX_[A-Z0-9_]+)\s+\(?\d+", re.MULTILINE)
COMMENTS = re.compile(r"/\*.*?\*/|//[^\n]*", re.DOTALL)
LIST = "scripts/check/options.txt"


def headers(root):
    return sorted(glob.glob(os.path.join(root, "lib", "*", "include", "**", "*.h"), recursive=True))


def read(path):
    with open(path, encoding="utf-8", errors="replace") as f:
        return f.read()


def line_of(text, pos):
    return text.count("\n", 0, pos) + 1


def first_member(text, pos):
    body = COMMENTS.sub(" ", text[pos:])
    return " ".join(body.split(";", 1)[0].split()) + ";"


def listed(root):
    found = {}
    with open(os.path.join(root, LIST), encoding="utf-8") as f:
        for n, line in enumerate(f, 1):
            line = line.rstrip("\n")
            if not line.strip() or line.startswith("#"):
                continue
            name, bar, why = line.partition("|")
            if not bar or not why.strip():
                print(f"{LIST}:{n}: needs `CF_MAX_<NAME>|<its option, or why none>`")
                found[None] = True
                continue
            found[name.strip()] = n
    return found


def main(root):
    bad = 0
    files = headers(root)
    texts = {p: read(p) for p in files}
    alltext = "\n".join(texts.values())
    for path in files:
        text = texts[path]
        rel = os.path.relpath(path, root).replace(os.sep, "/")
        for m in STRUCT.finditer(text):
            name = m.group(1)
            line = line_of(text, m.start())
            init = re.compile(rf"CF_API\s+void\s+{re.escape(name)}_init\s*\(")
            if not init.search(alltext):
                print(f"{rel}:{line}: {name}: no {name}_init")
                bad += 1
            if first_member(text, m.end()) != "size_t size;":
                print(f"{rel}:{line}: {name}: the first member is not `size_t size;`")
                bad += 1
    macros = {}
    for path in files:
        text = texts[path]
        rel = os.path.relpath(path, root).replace(os.sep, "/")
        for m in MACRO.finditer(text):
            macros[m.group(1)] = f"{rel}:{line_of(text, m.start())}"
    entries = listed(root)
    if None in entries:
        bad += 1
    for name in sorted(macros):
        if name not in entries:
            print(f"{macros[name]}: {name}: no line in {LIST}")
            bad += 1
    for name in sorted(k for k in entries if k):
        if name not in macros:
            print(f"{LIST}:{entries[name]}: {name}: no such macro in lib/*/include")
            bad += 1
    if bad:
        print(f"options gate: {bad} problem(s)")
        return 1
    print("options gate: ok")
    return 0


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(__doc__)
        sys.exit(2)
    sys.exit(main(sys.argv[1]))
