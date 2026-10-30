"""Every tracked text file is text: no NUL byte, no carriage return.

    python3 check_text.py <repository>

A tool that writes a source file through a shell can lose a backslash on
the way, and '\\0' then reaches the file as a NUL byte or '\\n' as a line
break inside a literal - it happened, more than once. A NUL compiles
into a different program or none; nothing in the build said which file
was at fault. This names it. A carriage return is the other way a text
file stops being the same file on every system: .gitattributes keeps
them out, and this checks that it did.

Binary files - images, the image format's fixtures - are told apart by
extension and left alone.
"""

import os
import subprocess
import sys

TEXT = (
    ".c", ".h", ".hpp", ".cpp", ".def", ".in", ".rs", ".py", ".toml",
    ".cmake", ".txt", ".md", ".json", ".ini", ".yml", ".yaml", ".sh",
    ".ps1", ".cfg", ".env", ".properties", ".xml",
)


def tracked(root):
    out = subprocess.run(["git", "-C", root, "ls-files", "-z"], capture_output=True)
    if out.returncode != 0:
        return None
    return [p for p in out.stdout.decode("utf-8", "replace").split("\0") if p]


def main(root):
    files = tracked(root)
    if files is None:
        print("text gate: not a git checkout, skipped")
        return 0
    bad = 0
    for rel in files:
        if not rel.endswith(TEXT) and os.path.basename(rel) not in ("CMakeLists.txt", "LICENSE"):
            continue
        path = os.path.join(root, rel)
        try:
            data = open(path, "rb").read()
        except OSError:
            continue  # deleted in the working tree, not yet in the index
        if b"\0" in data:
            line = data[: data.index(b"\0")].count(b"\n") + 1
            print("VIOLATION: %s:%d holds a NUL byte" % (rel, line))
            bad += 1
        if b"\r" in data:
            line = data[: data.index(b"\r")].count(b"\n") + 1
            print("VIOLATION: %s:%d holds a carriage return" % (rel, line))
            bad += 1
    if bad == 0:
        print("text gate: %d files, each one text" % len(files))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1] if len(sys.argv) > 1 else "."))
