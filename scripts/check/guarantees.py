#!/usr/bin/env python3
"""Every public function states one failure guarantee, and the library never
ends the program (K-9a, docs/Guarantees.md).

A public function is a name on a non-comment line of cmake/configforge.symbols
or cmake/configforge.experimental.symbols. Its declaration is the CF_API
line in a header of lib/*/include/configforge/. In the comment directly above
the declaration (or above a run of declarations with no blank line between
them) one line starts "Guarantee: " and a word (nothrow, strong or basic)
and a full stop.

Three more lines may stand beside it (K-9h), each a word and a full stop:
"Thread: " safe, distinct objects or unsafe; "Signal: " safe or unsafe;
"Alloc: " none or may. A line left out is the weakest word: distinct
objects, unsafe, may. "Signal: safe" and "Alloc: none" are checked against
the compiler's call data (GCC 10+, -fcallgraph-info, the *.ci files under
--callgraph): no allocator and no indirect call is reached, and for
"Signal: safe" no stdio, no lock and no name of scripts/check/notsignal.txt.

Usage:
  guarantees.py [--callgraph <dir>] <root>
                                   check; exit 1 and one line a fault
  guarantees.py --write <root>     rewrite the table of docs/Guarantees.md
  guarantees.py --todo <fragment> <root>
                                   names with no guarantee yet, in the
                                   headers whose path holds the fragment
"""

import collections
import fnmatch
import glob
import os
import re
import sys

WORDS = ("nothrow", "strong", "basic")
# The claims beside the guarantee: tag -> (the words, the default).
CLAIMS = {
    "Thread": (("safe", "distinct objects", "unsafe"), "distinct objects"),
    "Signal": (("safe", "unsafe"), "unsafe"),
    "Alloc": (("none", "may"), "may"),
}
API = r"(?:CF_API|CF_EXPERIMENTAL\(\d+,\s*\d+\))"
START = re.compile(r"^\s*(CF_NODISCARD\s+)?" + API + r"(?!\w)")
DECL = re.compile(API + r"\s+(.*?)\b([A-Za-z_]\w*)\s*\(", re.DOTALL)
LINE = re.compile(r"Guarantee:\s*([^.\s]*)")
CLAIM = re.compile(r"^(Thread|Signal|Alloc):\s*([^.]*)\.")
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
NOTSIGNAL_LIST = "scripts/check/notsignal.txt"
DOC = "docs/Guarantees.md"

Decl = collections.namedtuple("Decl", "rel line word nodiscard claims")

# What neither "Alloc: none" nor "Signal: safe" may reach: an allocator, and
# a call through a pointer (the library's allocator is one, and nothing says
# what else a pointer holds).
ALLOCATORS = (
    "malloc",
    "calloc",
    "realloc",
    "reallocarray",
    "free",
    "aligned_alloc",
    "posix_memalign",
    "memalign",
    "valloc",
    "strdup",
    "strndup",
    "cf_alloc",
    "cf_free",
    "cf_arena_new",
    "cf_arena_alloc",
    "cf_arena_calloc",
    "cf_arena_dup",
)
INDIRECT = "__indirect_call"
# What "Signal: safe" may not reach besides: stdio and locks (name patterns,
# * allowed; GCC turns printf into putchar and, fortified, into __printf_chk).
# scripts/check/notsignal.txt adds the project's own.
NOTSIGNAL = (
    "fopen",
    "freopen",
    "fclose",
    "fread",
    "fwrite",
    "fgets",
    "fputs",
    "fgetc",
    "fputc",
    "getc",
    "putc",
    "getchar",
    "putchar",
    "puts",
    "fflush",
    "fseek",
    "ftell",
    "rewind",
    "tmpfile",
    "perror",
    "printf",
    "fprintf",
    "vprintf",
    "vfprintf",
    "scanf",
    "sprintf",
    "snprintf",
    "vsprintf",
    "vsnprintf",
    "__*printf_chk",
    "pthread_mutex_*",
    "pthread_rwlock_*",
    "pthread_cond_*",
    "pthread_once",
    "mtx_*",
    "cnd_*",
    "call_once",
)


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


def comment_texts(block):
    """The lines of a comment block without the comment's own marks."""
    for line in block:
        text = line.strip()
        text = re.sub(r"^/\*+", "", text)
        text = re.sub(r"\*+/$", "", text)
        yield re.sub(r"^\*+", "", text).strip()


def guarantee_of(block):
    """The word of the first Guarantee line ('' when malformed), or None."""
    for text in comment_texts(block):
        if text.startswith("Guarantee:"):
            m = LINE.match(text)
            return m.group(1) if m else ""
    return None


def claims_of(block):
    """tag -> the word of the first Thread, Signal or Alloc line ('' when it
    has no full stop), for the tags written."""
    found = {}
    for text in comment_texts(block):
        tag = text.split(":", 1)[0]
        if tag in CLAIMS and tag not in found:
            m = CLAIM.match(text)
            found[tag] = " ".join(m.group(2).split()) if m else ""
    return found


def declarations(root):
    """name -> Decl(relative header, line number, word or None, nodiscard,
    {tag: word} of the claims written)."""
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
            block = comment_above(lines, kinds, i)
            found.setdefault(
                d.group(2),
                Decl(rel, i + 1, guarantee_of(block), nodiscard, claims_of(block)),
            )
    return found


def todo_names(root):
    return [ln for ln in read_lines(root, TODO) if ln and not ln.startswith("#")]


def claim_word(decl, tag):
    """The word a declaration states for `tag`, the default when it says none
    and '?' when what it says is not a word."""
    words, default = CLAIMS[tag]
    word = decl.claims.get(tag)
    if word is None:
        return default
    return word if word in words else "?"


def table(names, found, todo):
    rows = []
    counts = {"nothrow": 0, "strong": 0, "basic": 0, "not yet": 0}
    safe = {"Thread": 0, "Signal": 0, "Alloc": 0}
    for name in names:
        d = found[name]
        shown = d.word if d.word in WORDS and name not in todo else "not yet"
        counts[shown] += 1
        cells = []
        for tag in CLAIMS:
            word = claim_word(d, tag)
            if word in ("safe", "none"):
                safe[tag] += 1
            cells.append(word)
        rows.append(f"| `{name}` | `{os.path.basename(d.rel)}` | {shown} | " + " | ".join(cells) + " |")
    out = [
        "| Function | Header | Guarantee | Thread | Signal | Alloc |",
        "| -------- | ------ | --------- | ------ | ------ | ----- |",
    ]
    out += rows
    out.append("")
    c = counts
    out.append(
        f"nothrow {c['nothrow']}, strong {c['strong']}, basic {c['basic']}, not yet {c['not yet']}"
    )
    out.append("")
    out.append(
        f"thread safe {safe['Thread']}, signal safe {safe['Signal']}, alloc none {safe['Alloc']}"
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


def load_callgraph(directory):
    """The call data of GCC's -fcallgraph-info, every *.ci under `directory`:
    (graph, defined), or None when there is none. `graph` maps a node to the
    nodes it calls, `defined` a function's name to the nodes of that name
    that a file defines. A node is (file, name) for a function the file
    defines, so that two static functions of one name stay two; a call to a
    name this file does not define goes to every file that does, and to
    (None, name) when none does (libc, the compiler's helpers)."""
    files = sorted(glob.glob(os.path.join(directory, "**", "*.ci"), recursive=True))
    if not files:
        return None
    units = []
    defined = {}
    for path in files:
        with open(path, encoding="utf-8", errors="replace") as f:
            text = f.read()
        own = set()
        for m in re.finditer(r'node:\s*\{\s*title:\s*"([^"]*)"([^}]*)\}', text):
            if "ellipse" not in m.group(2):
                own.add(m.group(1))
                defined.setdefault(m.group(1), []).append((path, m.group(1)))
        calls = re.findall(r'edge:\s*\{\s*sourcename:\s*"([^"]*)"\s*targetname:\s*"([^"]*)"', text)
        units.append((path, own, calls))
    graph = {}
    for path, own, calls in units:
        for name in own:
            graph.setdefault((path, name), set())
        for src, dst in calls:
            targets = [(path, dst)] if dst in own else defined.get(dst) or [(None, dst)]
            graph.setdefault((path, src), set()).update(targets)
    return graph, defined


def notsignal_patterns(root):
    """The name patterns of scripts/check/notsignal.txt (name|reason a line)
    and the faults of its own lines."""
    names = []
    faults = []
    for line in read_lines(root, NOTSIGNAL_LIST):
        if not line or line.startswith("#"):
            continue
        parts = [p.strip() for p in line.split("|")]
        if len(parts) < 2 or not parts[0] or not parts[1]:
            faults.append(f"{NOTSIGNAL_LIST}: '{line}': name|reason needed")
            continue
        names.append(parts[0])
    return names, faults


def shown(name):
    """A node's name as a person reads it: GCC titles a static function
    'dir/file.c:name'; the directory is left out."""
    return "an indirect call" if name == INDIRECT else re.sub(r"^.*[/\\]", "", name)


def path_to(graph, start, bad):
    """The shortest chain of names from `start` to a name `bad` refuses, as a
    list, or None."""
    seen = {start}
    queue = collections.deque([(start, [shown(start[1])])])
    while queue:
        node, path = queue.popleft()
        for nxt in sorted(graph.get(node, ()), key=lambda k: (k[0] or "", k[1])):
            if bad(nxt[1]):
                return path + [shown(nxt[1])]
            if nxt not in seen:
                seen.add(nxt)
                queue.append((nxt, path + [shown(nxt[1])]))
    return None


def claim_faults(root, names, found, data):
    """(faults, notes) of "Signal: safe" and "Alloc: none" against the call
    data (None: there is none)."""
    faults = []
    notes = []
    wanted = [
        n
        for n in names
        if found[n].claims.get("Signal") == "safe" or found[n].claims.get("Alloc") == "none"
    ]
    if not wanted:
        return faults, notes
    if data is None:
        notes.append(
            "guarantees: call data skipped: no *.ci files (GCC 10+ writes them with "
            "-fcallgraph-info); Signal: safe and Alloc: none are not checked here"
        )
        return faults, notes
    graph, defined = data
    extra, more = notsignal_patterns(root)
    faults += more
    patterns = NOTSIGNAL + tuple(extra)

    def no_alloc(name):
        return name == INDIRECT or name in ALLOCATORS

    def no_signal(name):
        return no_alloc(name) or any(fnmatch.fnmatchcase(name, p) for p in patterns)

    for n in wanted:
        d = found[n]
        place = f"{d.rel}:{d.line}: {n}"
        starts = defined.get(n)
        if not starts:
            notes.append(f"note: {place}: not in the call data: its claim is not checked here")
            continue
        checks = []
        if d.claims.get("Signal") == "safe":
            checks.append(("Signal: safe", no_signal))
        if d.claims.get("Alloc") == "none":
            checks.append(("Alloc: none", no_alloc))
        for tag, test in checks:
            chain = None
            for start in starts:
                chain = path_to(graph, start, test)
                if chain:
                    break
            if chain:
                faults.append(f"{place}: {tag}, but " + " -> ".join(chain))
                break
    return faults, notes


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


def word_list(words):
    return ", ".join(words[:-1]) + " or " + words[-1] if len(words) > 2 else " or ".join(words)


def main(argv):
    mode = "check"
    fragment = ""
    callgraph = None
    args = []
    it = iter(argv)
    for a in it:
        if a == "--write":
            mode = "write"
        elif a == "--todo":
            mode = "todo"
            fragment = next(it, "")
        elif a == "--callgraph":
            callgraph = next(it, "")
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
            if fragment in found[n].rel and found[n].word is None:
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
    notes = []
    for n in names:
        rel, line, word, nodiscard, claims = found[n]
        for tag, (words, _) in CLAIMS.items():
            if tag in claims and claims[tag] not in words:
                faults.append(f"{rel}:{line}: {n}: {tag}: '{claims[tag]}' is not {word_list(words)}")
        if claims.get("Thread") == "unsafe":
            notes.append(f"note: {rel}:{line}: {n}: Thread: unsafe")
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
    more, said = claim_faults(root, names, found, load_callgraph(callgraph) if callgraph else None)
    faults += more
    notes += said
    if not parts or parts[0] + "\n\n" + new_table + "\n\n" + parts[1] != current:
        faults.append(f"{DOC}: the table is not what the headers say: run scripts/check/guarantees.py --write")
    if current is not None:
        faults += process_faults(current, root)
    for f in notes + faults:
        print(f)
    if faults:
        print(f"{len(faults)} fault(s)")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
