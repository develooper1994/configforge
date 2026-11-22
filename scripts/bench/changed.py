#!/usr/bin/env python3
"""Whether the comparison's own files changed (P-1b, D-111).

    changed.py [--before <sha>]

Prints `run=true` or `run=false`, and appends the same line to the file named
by $GITHUB_OUTPUT. CI's job `bench-smoke` runs only when it says true: a file
under bench/, under scripts/bench/ or .github/workflows/bench.yml changed
against the merge base with origin/main, or against --before (a push's
github.event.before) when that is a real commit. A checkout it cannot compare
(a shallow one, no origin/main) says true: the smoke test is the safe side.
"""

import os
import subprocess
import sys

PREFIXES = ("bench/", "scripts/bench/", ".github/workflows/bench.yml")


def git(*args):
    r = subprocess.run(["git"] + list(args), capture_output=True, text=True, check=False)
    return r.returncode, r.stdout.strip()


def base_of(before):
    if before and set(before) != {"0"}:
        rc, _ = git("cat-file", "-e", before + "^{commit}")
        if rc == 0:
            return before
    rc, out = git("merge-base", "origin/main", "HEAD")
    return out if rc == 0 and out else None


def changed(before):
    base = base_of(before)
    if base is None:
        return True
    rc, out = git("diff", "--name-only", base, "HEAD")
    if rc != 0:
        return True
    return any(p.startswith(PREFIXES) for p in out.splitlines())


def main(argv):
    before = ""
    if len(argv) == 2 and argv[0] == "--before":
        before = argv[1]
    line = "run=%s" % ("true" if changed(before) else "false")
    print(line)
    out = os.environ.get("GITHUB_OUTPUT")
    if out:
        with open(out, "a", encoding="utf-8") as f:
            f.write(line + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
