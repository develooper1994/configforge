#!/usr/bin/env python3
"""Every public function states one failure guarantee, and the library never
ends the program (K-9a, docs/Guarantees.md).

A public function is a name on a non-comment line of cmake/configforge.symbols
or cmake/configforge.experimental.symbols. Its declaration is the CF_API
line in a header of lib/*/include/configforge/. In the comment directly above
the declaration (or above a run of declarations with no blank line between
them) one line starts "Guarantee: " and a word (nothrow, strong or basic)
and a full stop.

Usage:
  guarantees.py <root>             check; exit 1 and one line a fault
  guarantees.py --write <root>     rewrite the table of docs/Guarantees.md
  guarantees.py --todo <fragment> <root>
                                   names with no guarantee yet, in the
                                   headers whose path holds the fragment
"""

import glob
import os
import re
import sys

WORDS = ("nothrow", "strong", "basic")
API = r"(?:CF_API|CF_EXPERIMENTAL\(\d+,\s*\d+\))"
START = re.compile(r"^\s*(CF_NODISCARD\s+)?" + API + r"(?!\w)")
DECL = re.compile(API + r"\s+(.*?)\b([A-Za-z_]\w*)\s*\(", re.DOTALL)
LINE = re.compile(r"Guarantee:\s*([^.\s]*)")
BEGIN = "<!-- guarantees:begin -->"
END = "<!-- guarantees:end -->"
PBEGIN = "<!-- process:begin -->"
PEND = "<!-- process:end -->"
NONLOCAL = [
    "abort",
    "exit",
    "_Exit",
    "quick_exit",
    "longjmp",
    "siglongjmp",
    "setjmp",
    "sigsetjmp",
    "assert",
    "raise",
    "__builtin_trap",
]
CALL = re.compile(r"(?<![\w.>])(" + "|".join(NONLOCAL) + r")\s*\(")
TODO = "scripts/check/guarantees_todo.txt"  # absent since K-9g: an empty list
NONLOCAL_LIST = "scripts/check/nonlocal.txt"
DOC = "docs/Guarantees.md"


def read_lines(root, rel):
    path = os.path.join(root, *rel.split("/"))
    if not os.path.exists(path):
        return []
    with open(path, encoding="utf-8") as f:
        return [ln.strip() for ln in f.read().split("\n")]


def public_names(root):
    names = []
    for rel in ("cmake/configforge.symbols", "cmake/configforge.experimental.symbols"):
        for line in read_lines(root, rel):
            if not line or line.startswith("#"):
                continue
            names.append(line.split()[0])
    return sorted(set(names))


def comment_map(lines):
    """For each line: 'blank', 'comment' (all of it is comment) or 'code'."""
    kinds = []
    inside = False
    for line in lines:
        s = line.strip()
        if not s and not inside:
            kinds.append("blank")
            continue
        pos = 0
        code = False
        while pos < len(s):
            if inside:
                end = s.find("*/", pos)
                if end < 0:
                    pos = len(s)
                else:
                    inside = False
                    pos = end + 2
            else:
                start = s.find("/*", pos)
                if start < 0:
                    code = code or bool(s[pos:].strip())
                    pos = len(s)
                else:
                    code = code or bool(s[pos:start].strip())
                    inside = True
                    pos = start + 2
        kinds.append("code" if code else "comment")
    return kinds


def comment_above(lines, kinds, i):
    """The comment block directly above declaration line i, as text lines."""
    j = i - 1
    while j >= 0 and kinds[j] == "code":
        j -= 1
    if j < 0 or kinds[j] != "comment":
        return []
    end = j
    while j >= 0 and kinds[j] == "comment" and "/*" not in lines[j]:
        j -= 1
    if j < 0 or kinds[j] != "comment":
        return []
    return lines[j : end + 1]


def guarantee_of(block):
    """The word of the first Guarantee line ('' when malformed), or None."""
    for line in block:
        text = line.strip()
        text = re.sub(r"^/\*+", "", text)
        text = re.sub(r"\*+/$", "", text)
        text = re.sub(r"^\*+", "", text).strip()
        if text.startswith("Guarantee:"):
            m = LINE.match(text)
            return m.group(1) if m else ""
    return None


def declarations(root):
    """name -> (relative header, line number, word or None, nodiscard)."""
    found = {}
    pattern = os.path.join(root, "lib", "*", "include", "configforge", "*.h")
    for path in sorted(glob.glob(pattern)):
        with open(path, encoding="utf-8") as f:
            lines = f.read().split("\n")
        kinds = comment_map(lines)
        rel = os.path.relpath(path, root).replace(os.sep, "/")
        for i, line in enumerate(lines):
            if kinds[i] != "code":
                continue
            m = START.match(line)
            if not m:
                continue
            text = line[re.search(API, line).start() :]
            j = i
            while "(" not in text and j + 1 < len(lines):
                j += 1
                text += " " + lines[j].strip()
            d = DECL.match(text)
            if not d:
                continue
            nodiscard = bool(m.group(1)) or (i > 0 and lines[i - 1].strip() == "CF_NODISCARD")
            word = guarantee_of(comment_above(lines, kinds, i))
            found.setdefault(d.group(2), (rel, i + 1, word, nodiscard))
    return found


def todo_names(root):
    return [ln for ln in read_lines(root, TODO) if ln and not ln.startswith("#")]


def table(names, found, todo):
    rows = []
    counts = {"nothrow": 0, "strong": 0, "basic": 0, "not yet": 0}
    for name in names:
        rel, _, word, _ = found[name]
        shown = word if word in WORDS and name not in todo else "not yet"
        counts[shown] += 1
        rows.append(f"| `{name}` | `{os.path.basename(rel)}` | {shown} |")
    out = ["| Function | Header | Guarantee |", "| -------- | ------ | --------- |"]
    out += rows
    out.append("")
    c = counts
    out.append(
        f"nothrow {c['nothrow']}, strong {c['strong']}, "
        f"basic {c['basic']}, not yet {c['not yet']}"
    )
    return "\n".join(out)


def split_doc(text):
    a = text.find(BEGIN)
    b = text.find(END)
    if a < 0 or b < a:
        return None
    return text[: a + len(BEGIN)], text[b:]


def strip_code(text):
    """Comments, string and character literals removed; newlines kept."""
    out = []
    i = 0
    n = len(text)
    while i < n:
        c = text[i]
        two = text[i : i + 2]
        if two == "/*":
            end = text.find("*/", i + 2)
            end = n if end < 0 else end + 2
            out.append(re.sub(r"[^\n]", " ", text[i:end]))
            i = end
        elif two == "//":
            end = text.find("\n", i)
            i = n if end < 0 else end
        elif c in "\"'":
            j = i + 1
            while j < n and text[j] != c and text[j] != "\n":
                j += 2 if text[j] == "\\" else 1
            out.append(c + c)
            i = j + 1
        else:
            out.append(c)
            i += 1
    return "".join(out)


def nonlocal_faults(root):
    allowed = set()
    faults = []
    for line in read_lines(root, NONLOCAL_LIST):
        if not line or line.startswith("#"):
            continue
        parts = [p.strip() for p in line.split("|")]
        if len(parts) < 3 or not parts[2]:
            faults.append(f"{NONLOCAL_LIST}: '{line}': path|token|reason needed")
            continue
        allowed.add((parts[0], parts[1]))
    files = []
    for sub in ("src", "include"):
        for ext in ("c", "h", "cpp", "hpp"):
            files += glob.glob(os.path.join(root, "lib", "*", sub, "**", "*." + ext), recursive=True)
    for path in sorted(set(files)):
        rel = os.path.relpath(path, root).replace(os.sep, "/")
        with open(path, encoding="utf-8", errors="replace") as f:
            text = strip_code(f.read())
        for no, line in enumerate(text.split("\n"), 1):
            for m in CALL.finditer(line):
                if (rel, m.group(1)) in allowed:
                    continue
                faults.append(f"{rel}:{no}: {m.group(1)}: the library never ends the program")
    return faults


def ctest_names(root):
    """The unit tests the tree registers: a name of a CMakeLists.txt or .cmake
    word that is also a source file of a tests directory (test_x.c, .cpp)."""
    words = set()
    for pat in ("CMakeLists.txt", "*.cmake"):
        for path in glob.glob(os.path.join(root, "**", pat), recursive=True):
            with open(path, encoding="utf-8", errors="replace") as f:
                words.update(re.findall(r"test_\w+", f.read()))
    names = set()
    for ext in ("c", "cpp"):
        for path in glob.glob(os.path.join(root, "**", "tests", "**", "test_*." + ext), recursive=True):
            stem = os.path.splitext(os.path.basename(path))[0]
            if stem in words:
                names.add("unit." + stem)
    return names


def process_faults(text, root):
    """Each row of the table "What holds across a process" ends in a cell of
    ctest names in backticks; each must be a test the tree registers."""
    a = text.find(PBEGIN)
    b = text.find(PEND)
    if a < 0 or b < a:
        return [f"{DOC}: markers {PBEGIN} and {PEND} not found"]
    known = ctest_names(root)
    faults = []
    rows = 0
    for line in text[a:b].split("\n"):
        cells = [c.strip() for c in line.strip().strip("|").split("|")]
        if not line.startswith("|") or len(cells) != 3 or set(cells[0]) <= set("- "):
            continue
        if cells[0] == "Across":
            continue
        rows += 1
        names = re.findall(r"`([^`]+)`", cells[2])
        if not names:
            faults.append(f"{DOC}: {cells[0]}: no test named")
        for n in names:
            if n not in known:
                faults.append(f"{DOC}: {cells[0]}: {n} is not a test")
    if rows < 4:
        faults.append(f"{DOC}: the table across a process has {rows} rows, not 4")
    return faults


def main(argv):
    mode = "check"
    fragment = ""
    args = []
    it = iter(argv)
    for a in it:
        if a == "--write":
            mode = "write"
        elif a == "--todo":
            mode = "todo"
            fragment = next(it, "")
        else:
            args.append(a)
    root = args[0] if args else "."
    names = public_names(root)
    found = declarations(root)
    missing = [n for n in names if n not in found]
    if missing:
        for n in missing:
            print(f"cmake/configforge.symbols: {n}: no CF_API declaration found")
        return 1
    todo = set(todo_names(root))

    if mode == "todo":
        for n in names:
            rel, _, word, _ = found[n]
            if fragment in rel and word is None:
                print(n)
        return 0
    new_table = table(names, found, todo)
    doc_path = os.path.join(root, *DOC.split("/"))
    current = None
    if os.path.exists(doc_path):
        with open(doc_path, encoding="utf-8", newline="") as f:
            current = f.read().replace("\r\n", "\n")
    parts = split_doc(current) if current is not None else None
    if mode == "write":
        if not parts:
            print(f"{DOC}: markers {BEGIN} and {END} not found")
            return 1
        with open(doc_path, "w", encoding="utf-8", newline="\n") as f:
            f.write(parts[0] + "\n\n" + new_table + "\n\n" + parts[1])
        return 0

    faults = []
    for n in names:
        rel, line, word, nodiscard = found[n]
        if n in todo:
            if word is not None:
                faults.append(f"{TODO}: {n}: has a guarantee, remove it from the list")
            continue
        if word is None:
            faults.append(f"{rel}:{line}: {n}: no guarantee")
        elif word not in WORDS:
            faults.append(f"{rel}:{line}: {n}: '{word}' is not nothrow, strong or basic")
        elif word == "nothrow" and nodiscard:
            faults.append(f"{rel}:{line}: {n}: CF_NODISCARD and nothrow")
    for n in sorted(todo):
        if n not in found:
            faults.append(f"{TODO}: {n}: not a public name")
    faults += nonlocal_faults(root)
    if not parts or parts[0] + "\n\n" + new_table + "\n\n" + parts[1] != current:
        faults.append(f"{DOC}: the table is not what the headers say: run scripts/check/guarantees.py --write")
    if current is not None:
        faults += process_faults(current, root)
    for f in faults:
        print(f)
    if faults:
        print(f"{len(faults)} fault(s)")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
