#!/usr/bin/env python3
"""Generate the single-file distribution.

SQLite's reach owes as much to sqlite3.c as to its portability: a
developer evaluating a library copies two files and compiles them with
whatever they already have, and meets the build system only after
deciding to keep it.  For a library whose premise is reaching old and
unusual toolchains, the distribution form that needs no build system is
the important one.

Generated in CI from the real sources and never maintained by hand.
"""

import os
import re
import sys

# Headers in dependency order.  Explicit rather than computed: the order
# is part of the architecture, and a topological sort would hide a cycle
# instead of failing on it.
HEADERS = [
    "cf_build_options.h",
    "cf_port.h",
    "cf_version.h",
    "cf_error.h",
    "cf_numeric.h",
    "cf_value.h",
    "cf_alloc.h",
    "cf_platform.h",
    "cf_keyspace.h",
    "cf_entry.h",
    "cf_serialize.h",
    "cf_source.h",
    "cf_define.h",
    "cf_explain.h",
    "cf_iterate.h",
    "cf_flatten.h",
    "cf_emit.h",
    "cf_image.h",
    "cf_report.h",
    "configforge.h",
]

# Sources.  The amalgamation carries the built-in codecs only, which is
# what keeps it dependency-free.
SOURCES = [
    # Every platform, each guarded internally: the amalgamation is one
    # translation unit and has no build to choose for it.
    "platform/posix/posix.c", "platform/win32/win32.c",
    "platform/none/none.c",
    "core/version.c", "core/port.c", "core/error.c", "core/numeric.c", "core/alloc.c",
    "core/value.c", "core/keyspace.c", "core/define.c", "core/validate.c",
    "core/config.c", "core/iterate.c",
    "serialize/blob.c", "serialize/codec.c", "serialize/ini.c",
    "serialize/json.c", "serialize/toml.c",
    "sources/entry.c", "sources/memory.c", "sources/defaults.c", "sources/env.c",
    "sources/cli.c", "sources/compat.c", "sources/snapshot.c",
    "bridge/flatten.c", "bridge/file.c", "bridge/dotenv.c", "bridge/dir.c", "bridge/emit.c", "bridge/image.c",
    "diag/explain.c", "diag/report.c",
]

LOCAL_INCLUDE = re.compile(r'^\s*#\s*include\s*"configforge/([^"]+)"\s*$')
GUARD_OPEN = re.compile(r'^\s*#\s*ifndef\s+CONFIGFORGE\w*\s*$')
GUARD_DEF = re.compile(r'^\s*#\s*define\s+CONFIGFORGE\w*\s*$')


def resolve(line, incdir):
    """Return the replacement lines for a local include.

    A .h include is dropped: the amalgamation already contains it.  A
    .def include is inlined *at every occurrence*, because an X-macro
    list is included once per macro definition and each expansion is a
    different piece of code.  Dropping them instead produced an
    amalgamation that referred to enumerators nothing had declared,
    which the amalgamation gate caught on the first run.
    """
    m = LOCAL_INCLUDE.match(line)
    if m is None:
        return [line]
    name = m.group(1)
    if not name.endswith(".def"):
        return []
    with open(os.path.join(incdir, name), encoding="utf-8") as f:
        body = f.read().splitlines()
    return (["/* inlined: configforge/%s */" % name] + body)


def strip_header(text, incdir):
    """Drop the include guard and local includes; keep everything else."""
    out = []
    lines = text.splitlines()
    i = 0
    depth = 0
    # Remove the outermost guard: the opening #ifndef, its #define, and
    # the matching #endif at the very end.
    while i < len(lines) and not GUARD_OPEN.match(lines[i]):
        out.extend(resolve(lines[i], incdir))
        i += 1
    if i < len(lines):
        i += 1                      # the #ifndef
        if i < len(lines) and GUARD_DEF.match(lines[i]):
            i += 1                  # the #define
        depth = 1
        body = []
        while i < len(lines):
            line = lines[i]
            if re.match(r'^\s*#\s*if', line):
                depth += 1
            elif re.match(r'^\s*#\s*endif', line):
                depth -= 1
                if depth == 0:
                    i += 1
                    continue
            body.extend(resolve(line, incdir))
            i += 1
        out.extend(body)
    return "\n".join(out).rstrip() + "\n"


def strip_source(text, incdir):
    out = []
    for line in text.splitlines():
        out.extend(resolve(line, incdir))
    return "\n".join(out).rstrip() + "\n"


# Transliteration for the notice the amalgamation carries. ASCII,
# because the generated files are compiled by whatever compiler
# receives them, in whatever code page it runs: MSVC reading a Turkish
# letter outside its code page warns, and at /WX that is a failed build
# caused by a comment.
ASCII = str.maketrans("çÇğĞıİöÖşŞüÜ", "cCgGiIoOsSuU")


def notice(src):
    """The licence as a comment. zlib asks that it stay with every source
    distribution, and the amalgamation is the one most likely to be
    copied into somebody else's tree on its own."""
    with open(os.path.join(src, "LICENSE"), encoding="utf-8") as f:
        text = f.read().translate(ASCII)
    if any(ord(c) > 127 for c in text):
        raise SystemExit("amalgamate: LICENSE has characters the notice cannot carry")
    return "/*\n" + "".join(" * " + l + "\n" if l else " *\n"
                              for l in text.rstrip().splitlines()) + " */\n\n"


def banner(title):
    return ("\n/* " + "=" * 68 + "\n"
            " * " + title + "\n"
            " * " + "=" * 68 + " */\n\n")


def check_complete(src):
    """Every .c under src/ has to be in SOURCES, and every .h in HEADERS.

    Relying on a test program to notice a missing file only works when
    that program happens to use a symbol from it - a negative test here
    dropped core/iterate.c and nothing failed, because the differential
    program did not iterate. The list is checked against the directory
    instead, which does not depend on what anyone remembered to call.
    """
    on_disk = set()
    for root, _dirs, files in os.walk(os.path.join(src, "src")):
        for name in files:
            if name.endswith(".c"):
                rel = os.path.relpath(os.path.join(root, name),
                                      os.path.join(src, "src"))
                on_disk.add(rel.replace(os.sep, "/"))

    listed = set(SOURCES)
    missing = sorted(on_disk - listed)
    extra = sorted(listed - on_disk)

    hdr_disk = set()
    for name in os.listdir(os.path.join(src, "include", "configforge")):
        if name.endswith(".h"):
            hdr_disk.add(name)
    hdr_missing = sorted(hdr_disk - set(HEADERS))

    problems = []
    if missing:
        problems.append("not in SOURCES: " + ", ".join(missing))
    if extra:
        problems.append("in SOURCES but not on disk: " + ", ".join(extra))
    if hdr_missing:
        problems.append("not in HEADERS: " + ", ".join(hdr_missing))
    return problems


def main():
    if len(sys.argv) != 3:
        sys.stderr.write("usage: amalgamate.py <source-dir> <out-dir>\n")
        return 2
    src, out = sys.argv[1], sys.argv[2]

    problems = check_complete(src)
    if problems:
        for p in problems:
            sys.stderr.write("amalgamate: %s\n" % p)
        sys.stderr.write("amalgamate: the file list and the tree disagree\n")
        return 1
    incdir = os.path.join(src, "include", "configforge")
    os.makedirs(out, exist_ok=True)

    head = [
        notice(src) +
        "/*\n"
        " * configforge/configforge.h - single-file distribution.\n"
        " *\n"
        " * Generated by scripts/amalgamate.py from the real sources. Do not\n"
        " * edit: every change belongs upstream, where the tests are.\n"
        " */\n\n"
        "#ifndef CONFIGFORGE_AMALGAMATED_H\n"
        "#define CONFIGFORGE_AMALGAMATED_H\n"
    ]
    for h in HEADERS:
        p = os.path.join(src, "include", "configforge", h)
        with open(p, encoding="utf-8") as f:
            head.append(banner(h))
            head.append(strip_header(f.read(), incdir))
    head.append("\n#endif /* CONFIGFORGE_AMALGAMATED_H */\n")

    body = [
        notice(src) +
        "/*\n"
        " * configforge.c - single-file distribution.\n"
        " *\n"
        " * Compile this one file and include <configforge/configforge.h>.\n" " * No build\n"
        " * system, no options, no dependencies beyond the C library.\n"
        " */\n\n"
        '#include <configforge/configforge.h>\n'
    ]
    for s in SOURCES:
        p = os.path.join(src, "src", s)
        with open(p, encoding="utf-8") as f:
            body.append(banner(s))
            body.append(strip_source(f.read(), incdir))

    # Into a configforge/ subdirectory, so that <configforge/configforge.h>
    # is the only spelling anywhere: the same source compiles against the
    # amalgamation and against the installed library with nothing but a
    # different -I. A flat configforge.h next to the .c would have been
    # one file fewer and a second spelling to remember.
    os.makedirs(os.path.join(out, "configforge"), exist_ok=True)
    hpath = os.path.join(out, "configforge", "configforge.h")
    with open(hpath, "w", encoding="utf-8") as f:
        f.write("".join(head))
    with open(os.path.join(out, "configforge.c"), "w", encoding="utf-8") as f:
        f.write("".join(body))

    hn = sum(1 for _ in open(hpath, encoding="utf-8"))
    cn = sum(1 for _ in open(os.path.join(out, "configforge.c"), encoding="utf-8"))
    print("amalgamation: configforge.h %d lines, configforge.c %d lines"
          % (hn, cn))
    return 0


if __name__ == "__main__":
    sys.exit(main())
