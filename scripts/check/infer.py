"""Fail on an Infer finding in lib/*/src that .infer-ignore.txt does not list.

usage: infer.py <build-dir> <repo>

Reads <build-dir>/infer-out/report.json (Infer's own exit code is not used),
keeps the findings in a file under lib/*/src, and exits 1 for each finding
whose bug_type, file and procedure are not an entry of <repo>/.infer-ignore.txt,
and for each entry that no finding matches: a stale entry is an error, so
that a fix removes its line.

An entry is a '#' line with the reason, then '<bug_type> <file> <procedure>'
on one line; never a whole file, never a bare bug type.
"""

import fnmatch
import json
import os
import sys


def read_ignore(path):
    """Return the (bug_type, file, procedure) entries and the errors found."""
    entries = []
    errors = []
    if not os.path.exists(path):
        return entries, errors
    reason = False
    with open(path, encoding="utf-8") as handle:
        for number, raw in enumerate(handle, 1):
            line = raw.strip()
            if not line:
                reason = False
                continue
            if line.startswith("#"):
                reason = True
                continue
            parts = line.split(None, 2)
            if len(parts) != 3:
                errors.append(f"{path}:{number}: want '<bug_type> <file> <procedure>'")
                continue
            if not reason:
                errors.append(f"{path}:{number}: an entry needs a '#' line with its reason before it")
            entries.append((tuple(parts), number))
    return entries, errors


def in_library(name):
    """Whether a finding's file is a library source under lib/*/src."""
    return fnmatch.fnmatch(name.replace("\\", "/"), "lib/*/src/*")


def main(argv):
    if len(argv) != 3:
        print(__doc__, file=sys.stderr)
        return 2
    build, repo = argv[1], argv[2]
    report = os.path.join(build, "infer-out", "report.json")
    try:
        with open(report, encoding="utf-8") as handle:
            findings = json.load(handle)
    except (OSError, ValueError) as error:
        print(f"infer.py: cannot read {report}: {error}", file=sys.stderr)
        return 2

    ignore_path = os.path.join(repo, ".infer-ignore.txt")
    entries, errors = read_ignore(ignore_path)
    keys = {key for key, _ in entries}
    used = set()
    failures = list(errors)
    seen = set()
    for item in findings:
        name = item.get("file", "")
        if not in_library(name):
            continue
        key = (item.get("bug_type", ""), name, item.get("procedure", ""))
        if key in keys:
            used.add(key)
            continue
        text = (f"{name}:{item.get('line', 0)}: {key[0]}: "
                f"{item.get('qualifier', '').replace(chr(10), ' ')} [in {key[2]}]")
        if text not in seen:
            seen.add(text)
            failures.append(text)
    for key, number in entries:
        if key not in used:
            failures.append(f"{ignore_path}:{number}: stale entry, no finding matches: {' '.join(key)}")

    for text in failures:
        print(text)
    if failures:
        print(f"infer.py: {len(failures)} problem(s)", file=sys.stderr)
        return 1
    print("infer.py: no finding outside .infer-ignore.txt")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
