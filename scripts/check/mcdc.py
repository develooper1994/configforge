#!/usr/bin/env python3
"""MC/DC as GCC measures it: the share of condition outcomes the suite covers
(L-25, D-233, docs/Compliance.md "What the library gives a user's safety
process").

Usage: mcdc.py <build dir> --root <source root> --filter <regex> --min <n>
                [--report <file>] [--gcov <exe>]

The build dir holds the .gcda files of a build made with
-fcondition-coverage --coverage that was then run. For each one the script
runs `<gcov> --json-format --stdout --conditions` from the .gcda's directory
and reads lines[].conditions[]: `count` outcomes, `covered` of them covered.
A source file compiled into two targets is merged per condition, taking the
larger `covered`, as gcovr merges. Files whose path (relative to the root,
with / separators) matches the filter are summed.

Prints `mcdc: <covered>/<count> outcomes (<p>%) in <files> files`; exit 1 when
p is under --min, when a relative path does not resolve to a file under the
root, or when no condition data was found at all. --report writes one line a
file, worst first: `<p>% <covered>/<count> <path>`. An absolute path outside
the root (a system header) is not the project's and is left out.
Python 3, standard library only.
"""

import argparse
import json
import os
import re
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor


def find_gcda(build):
    found = []
    for dirpath, _dirs, names in os.walk(build):
        for name in names:
            if name.endswith(".gcda"):
                found.append(os.path.join(dirpath, name))
    return sorted(found)


def run_gcov(gcov, gcda):
    """The JSON documents gcov prints for one .gcda (one per line)."""
    proc = subprocess.run(
        [gcov, "--json-format", "--stdout", "--conditions", os.path.basename(gcda)],
        cwd=os.path.dirname(gcda),
        capture_output=True,
        text=True,
        check=False,
    )
    if proc.returncode != 0:
        why = proc.stderr.strip()[:200]
        return gcda, [], f"{gcda}: gcov exited {proc.returncode}: {why}"
    docs = []
    decoder = json.JSONDecoder()
    text = proc.stdout
    pos = 0
    while True:
        while pos < len(text) and text[pos].isspace():
            pos += 1
        if pos >= len(text):
            break
        doc, pos = decoder.raw_decode(text, pos)
        docs.append(doc)
    return gcda, docs, None


def resolve(path, cwd, root):
    """(relative path under root or None, failure text or None)."""
    absolute = os.path.isabs(path)
    full = os.path.normpath(path if absolute else os.path.join(cwd, path))
    rel = os.path.relpath(full, root).replace(os.sep, "/")
    if rel == ".." or rel.startswith("../") or os.path.isabs(rel):
        if absolute:
            return None, None
        return None, f"{path}: not under the root {root} (compiled in {cwd})"
    if not os.path.isfile(full):
        return None, f"{path}: no such file (resolved to {full})"
    return rel, None


def collect(gcdas, gcov, root, pattern):
    """(merged condition counts, failures) over every .gcda."""
    failures = []
    # (path, line, function, condition index) -> [count, covered]
    merged = {}
    with ThreadPoolExecutor(max_workers=os.cpu_count() or 2) as pool:
        results = list(pool.map(lambda g: run_gcov(gcov, g), gcdas))
    for gcda, docs, err in results:
        if err:
            failures.append(err)
            continue
        for doc in docs:
            cwd = doc.get("current_working_directory", os.path.dirname(gcda))
            for entry in doc.get("files", []):
                rel, bad = resolve(entry.get("file", ""), cwd, root)
                if bad:
                    if bad not in failures:
                        failures.append(bad)
                    continue
                if rel is None or not pattern.search(rel):
                    continue
                for line in entry.get("lines", []):
                    name = line.get("function_name", "")
                    for i, cond in enumerate(line.get("conditions", [])):
                        key = (rel, line.get("line_number"), name, i)
                        have = merged.setdefault(key, [0, 0])
                        have[0] = max(have[0], cond.get("count", 0))
                        have[1] = max(have[1], cond.get("covered", 0))
    return merged, failures


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("build")
    ap.add_argument("--root", required=True)
    ap.add_argument("--filter", required=True)
    ap.add_argument("--min", type=float, required=True)
    ap.add_argument("--report")
    ap.add_argument("--gcov", default="gcov")
    args = ap.parse_args(argv)

    gcdas = find_gcda(args.build)
    merged, failures = collect(
        gcdas, args.gcov, os.path.realpath(args.root), re.compile(args.filter)
    )

    per_file = {}
    for (rel, _line, _fn, _i), (count, covered) in merged.items():
        tot = per_file.setdefault(rel, [0, 0])
        tot[0] += count
        tot[1] += covered
    count = sum(v[0] for v in per_file.values())
    covered = sum(v[1] for v in per_file.values())

    if args.report:
        rows = sorted(
            ((rel, v) for rel, v in per_file.items() if v[0]),
            key=lambda kv: (kv[1][1] / kv[1][0], kv[0]),
        )
        with open(args.report, "w", newline="\n") as out:
            for rel, (c, cv) in rows:
                out.write(f"{100.0 * cv / c:.1f}% {cv}/{c} {rel}\n")

    for text in failures:
        print("mcdc: " + text)
    if count == 0:
        print(
            f"mcdc: no condition data under {args.build} for {args.filter} "
            f"({len(gcdas)} .gcda files)"
        )
        return 1
    pct = 100.0 * covered / count
    files = len([1 for v in per_file.values() if v[0]])
    print(f"mcdc: {covered}/{count} outcomes ({pct:.1f}%) in {files} files")
    if failures:
        return 1
    if covered * 100.0 < args.min * count:
        print(f"mcdc: {pct:.1f}% is under the floor of {args.min:g}%")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
