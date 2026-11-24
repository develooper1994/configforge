"""Every download in a workflow or a script is listed, with how it is held.

    python3 fetches.py <repository>

Code that reaches a build from the network must be held by a commit or by
a SHA-256, or be data nothing runs. This gate cannot prove that, but it
makes sure no fetch is added unseen: every line of the tracked workflows,
scripts, CMake files, benchmarks and packaging that downloads or clones
must be covered by one entry of scripts/check/fetches.txt, and every entry
must match a line.

An entry is three tab-separated fields: the file; a URL prefix as written
in that file, up to the first '$', '"' or space ('-' for a fetch line that
has no URL on it); and how it is held: 'sha256 <where it is checked>',
'commit <40-hex, or where it is>' or 'data <why no code runs from it>'.
Lines starting with '#' and empty lines are ignored. Documents (*.md),
C sources and this script and its list are not searched.
"""

import os
import re
import subprocess
import sys

FETCH = re.compile(
    r"FetchContent_Declare|ExternalProject_Add|git clone|git fetch|curl -|wget "
    r"|file\(DOWNLOAD|Invoke-WebRequest|iwr "
)
ROOTS = (".github/", "scripts/", "cmake/", "bench/", "packaging/")
SKIP_SUFFIX = (".md", ".c", ".h")
SKIP_FILES = ("scripts/check/fetches.txt", "scripts/check/fetches.py")
HOLDS = ("sha256", "commit", "data")
LIST = "scripts/check/fetches.txt"


def tracked(root):
    out = subprocess.run(
        ["git", "-C", root, "ls-files", "-z"], check=False, capture_output=True
    )
    if out.returncode != 0:
        return None
    return [p for p in out.stdout.decode("utf-8", "replace").split("\0") if p]


def searched(rel):
    if rel in SKIP_FILES or rel.endswith(SKIP_SUFFIX):
        return False
    return rel.startswith(ROOTS) or os.path.basename(rel) == "CMakeLists.txt"


def entries(root):
    found = []
    with open(os.path.join(root, LIST), encoding="utf-8") as f:
        for n, line in enumerate(f, 1):
            line = line.rstrip("\n")
            if not line.strip() or line.startswith("#"):
                continue
            parts = line.split("\t")
            if len(parts) != 3 or not parts[2].split(" ", 1)[0] in HOLDS:
                print(
                    f"fetches: {LIST}:{n}: want file<TAB>prefix<TAB>sha256|commit|data why"
                )
                return None
            found.append((n, parts[0], parts[1], parts[2]))
    return found


def covers(entry, rel, line):
    _, file, prefix, _ = entry
    if file != rel:
        return False
    if prefix == "-":
        return "://" not in line
    return prefix in line


def main(root):
    files = tracked(root)
    if files is None:
        print("fetches: not a git checkout, skipped")
        return 0
    held = entries(root)
    if held is None:
        return 1
    used = set()
    bad = 0
    count = 0
    for rel in files:
        if not searched(rel):
            continue
        try:
            with open(os.path.join(root, rel), encoding="utf-8", errors="replace") as f:
                lines = f.read().split("\n")
        except OSError:
            continue
        for n, line in enumerate(lines, 1):
            if not FETCH.search(line):
                continue
            hits = [e for e in held if covers(e, rel, line)]
            if len(hits) != 1:
                what = "not in" if not hits else "matched twice in"
                print(f"fetches: {rel}:{n}: {what} {LIST}")
                bad += 1
                continue
            used.add(hits[0][0])
            count += 1
    for n, file, prefix, how in held:
        if n not in used:
            print(f"fetches: {LIST}:{n}: {file} {prefix}: no such fetch")
            bad += 1
    if bad:
        return 1
    print(f"fetches: {count} held")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1] if len(sys.argv) > 1 else "."))
