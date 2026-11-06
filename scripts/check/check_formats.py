#!/usr/bin/env python3
"""Check every format string in the library against what cf_fmt supports.

cf_fmt has no width, no precision and no flags.  That is deliberate, and
the failure mode when somebody forgets is why this exists: an
unsupported conversion is copied out literally, so "%08x" produces four
characters rather than a number.  Every provenance report carried the
literal format string that way for a whole session, and the test that
should have caught it only checked the prefix.

The library uses no printf, so every format string under src/ belongs to
the cf_fmt family.  Tools and tests print with the C library and are not
checked.

Comments are stripped first.  They discuss the unsupported conversions
by name - that is where the reasons live - and a checker that could not
tell a comment from code would make the reasons unwritable.
"""

import re
import sys

# %s %c %d %i %u %x %% plus %zu, and the ConfigForge extensions
# %I (cf_i64), %U (cf_u64), %S (pointer and length).
SUPPORTED = {"%s", "%c", "%d", "%i", "%u", "%x", "%%", "%zu", "%I", "%U", "%S"}

CONVERSION = re.compile(r"%[-+ #0-9.*]*(?:zu|hh?[diouxX]|ll?[diouxX]|[a-zA-Z%])")


def strip_comments_and_find_literals(text):
    """Yield (line, literal) for every string literal outside a comment."""
    out = []
    i = 0
    line = 1
    n = len(text)
    while i < n:
        c = text[i]
        if c == "\n":
            line += 1
            i += 1
        elif text.startswith("/*", i):
            end = text.find("*/", i + 2)
            if end < 0:
                break
            line += text.count("\n", i, end)
            i = end + 2
        elif text.startswith("//", i):
            i = text.find("\n", i)
            if i < 0:
                break
        elif c == '"':
            j = i + 1
            buf = []
            while j < n:
                if text[j] == "\\":
                    buf.append(text[j:j + 2])
                    j += 2
                    continue
                if text[j] == '"':
                    break
                buf.append(text[j])
                j += 1
            out.append((line, "".join(buf)))
            line += text.count("\n", i, j)
            i = j + 1
        elif c == "'":
            j = i + 1
            while j < n and text[j] != "'":
                j += 2 if text[j] == "\\" else 1
            i = j + 1
        else:
            i += 1
    return out


def main(paths):
    bad = 0
    for path in paths:
        with open(path, encoding="utf-8") as f:
            text = f.read()
        for line, literal in strip_comments_and_find_literals(text):
            for conv in CONVERSION.findall(literal):
                if conv in SUPPORTED:
                    continue
                print("VIOLATION: %s:%d: '%s' is not a cf_fmt conversion"
                      % (path, line, conv))
                print("  it would be copied out literally, not formatted")
                bad = 1
    if bad:
        return 1
    print("format string gate: clean")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
