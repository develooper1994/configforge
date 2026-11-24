"""Every workflow's top-level token is read-only.

    python3 permissions.py <repository>

A workflow with no top-level `permissions:` gets the repository's default
token, which may write; one that says `write-all` or `contents: write`
there gives every job and every step that power. A job asks for what it
needs in its own block, so job blocks are not checked. A top-level block
must be `permissions: {}` or hold only `<scope>: read` or `<scope>: none`
lines. The check reads the text of .github/workflows/*.yml: the top-level
key is the one at column 0.
"""

import glob
import os
import re
import sys

OK = ("read", "none")


def check(path, rel):
    with open(path, encoding="utf-8") as f:
        lines = f.read().split("\n")
    start = None
    for n, line in enumerate(lines, 1):
        if re.match(r"permissions:", line):
            start = n
            break
    if start is None:
        print(f"permissions: {rel}: no top-level permissions:")
        return 1
    head = lines[start - 1].split("#")[0].strip()
    value = head[len("permissions:") :].strip()
    if value:
        if value == "{}":
            return 0
        print(f"permissions: {rel}:{start}: top-level permissions: {value}")
        return 1
    bad = 0
    for n in range(start + 1, len(lines) + 1):
        line = lines[n - 1]
        body = line.split("#")[0].rstrip()
        if not body.strip():
            continue
        if not line.startswith((" ", "\t")):
            break
        m = re.match(r"\s+([A-Za-z-]+):\s*(\S+)$", body)
        if not m or m.group(2) not in OK:
            print(f"permissions: {rel}:{n}: {body.strip()}")
            bad += 1
    return bad


def main(root):
    paths = sorted(glob.glob(os.path.join(root, ".github", "workflows", "*.yml")))
    bad = 0
    for path in paths:
        bad += check(path, os.path.relpath(path, root).replace(os.sep, "/"))
    if bad:
        return 1
    print(f"permissions: {len(paths)} workflows read-only at the top")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1] if len(sys.argv) > 1 else "."))
