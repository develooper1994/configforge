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
    "cf_log.h",
    "cf_numeric.h",
    "cf_text.h",
    "cf_encoding.h",
    "cf_value.h",
    "cf_alloc.h",
    "cf_platform_file.h",
    "cf_platform.h",
    "cf_keyspace.h",
    "cf_entry.h",
    "cf_serialize_version.h",
    "cf_serialize.h",
    "cf_yamlparse.h",
    "cf_source.h",
    "cf_define.h",
    "cf_explain.h",
    "cf_iterate.h",
    "cf_flatten.h",
    "cf_emit.h",
    "cf_image.h",
    "cf_report.h",
    "cf_live.h",
    "cf_edit.h",
    "configforge.h",
]

# Sources.  The amalgamation carries the built-in codecs only, which is
# what keeps it dependency-free.
# Public headers that are not the configuration library's, and so not
# in its single file: Doctor's rules are a library of their own
# (cf_doctor.h), which a device reading configuration does not carry.
NOT_AMALGAMATED = {"cf_doctor.h"}

SOURCES = [
    # Every platform, each guarded internally: the amalgamation is one
    # translation unit and has no build to choose for it.
    "platform/src/posix/posix.c", "platform/src/posix/file.c",
    "platform/src/win32/win32.c", "platform/src/win32/file.c",
    "platform/src/none/none.c", "platform/src/none/file.c",
    "config/src/core/version.c", "port/src/port.c", "base/src/error.c",
    "base/src/key.c",
    "numeric/src/numeric.c", "base/src/alloc.c",
    "text/src/utf8.c", "text/src/hex.c", "text/src/hash.c", "text/src/encoding.c", "text/src/place.c", "text/src/escape.c", "text/src/refs.c",
    "text/src/space.c", "text/src/track.c", "text/src/fail.c",
    "lanes/src/lanes.c", "lanes/src/scalar.c",
    "base/src/value.c", "config/src/core/keyspace.c", "config/src/core/define.c",
    "config/src/core/validate.c",
    "config/src/core/config.c", "config/src/core/iterate.c", "config/src/core/live.c",
    "config/src/core/edit.c",
    "serialize/src/blob.c", "serialize/src/codec.c", "serialize/src/detect.c", "serialize/src/ini.c",
    "serialize/src/json.c", "serialize/src/toml.c", "serialize/src/yamlparse.c", "serialize/src/yaml.c", "serialize/src/xml.c", "serialize/src/properties.c", "serialize/src/cbor.c", "serialize/src/tlv.c", "serialize/src/version.c",
    "config/src/sources/entry.c", "config/src/sources/memory.c", "config/src/sources/defaults.c", "config/src/sources/env.c",
    "config/src/sources/cli.c", "config/src/sources/compat.c", "config/src/sources/snapshot.c",
    "config/src/bridge/flatten.c", "config/src/bridge/file.c", "config/src/bridge/dotenv.c", "config/src/bridge/dir.c", "config/src/bridge/emit.c", "config/src/bridge/image.c", "config/src/bridge/store.c", "config/src/bridge/secrets.c", "config/src/bridge/registry.c", "config/src/bridge/preferences.c","config/src/bridge/schema.c",
    "config/src/diag/explain.c", "config/src/diag/report.c", "config/src/diag/diff.c",
]

# The components, in D-100's order: the public headers of each are in
# lib/<component>/include/configforge/, and the sources in
# lib/<component>/src/.  A header is looked for in each in turn.
COMPONENTS = ["port", "numeric", "base", "text", "platform", "serialize", "config", "cxx"]

# lanes is not ConfigForge's (D-101): "lanes/..." and <lanes/...> are local
# includes too. Its header is put into configforge.c under its prefix, and
# its sources are among SOURCES.
LOCAL_INCLUDE = re.compile(
    r'^\s*#\s*include\s*(?:"configforge/([^"]+)"|["<]lanes/([^">]+)[">])\s*$')
GUARD_OPEN = re.compile(r'^\s*#\s*ifndef\s+CONFIGFORGE\w*\s*$')
GUARD_DEF = re.compile(r'^\s*#\s*define\s+CONFIGFORGE\w*\s*$')


def find_header(incdirs, name):
    """The path of a public header, in whichever component holds it."""
    for d in incdirs:
        p = os.path.join(d, name)
        if os.path.exists(p):
            return p
    raise SystemExit(f"amalgamate: no component holds configforge/{name}")


def resolve(line, incdirs):
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
    name = m.group(1) or m.group(2)
    if not name.endswith(".def"):
        return []
    with open(find_header(incdirs, name), encoding="utf-8") as f:
        body = f.read().splitlines()
    return ([f"/* inlined: configforge/{name} */"] + body)


def strip_header(text, incdirs):
    """Drop the include guard and local includes; keep everything else."""
    out = []
    lines = text.splitlines()
    i = 0
    depth = 0
    # Remove the outermost guard: the opening #ifndef, its #define, and
    # the matching #endif at the very end.
    while i < len(lines) and not GUARD_OPEN.match(lines[i]):
        out.extend(resolve(lines[i], incdirs))
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
            body.extend(resolve(line, incdirs))
            i += 1
        out.extend(body)
    return "\n".join(out).rstrip() + "\n"


SYSTEM_INCLUDE = re.compile(r'^\s*#\s*include\s*<([^>]+)>\s*$')


def strip_source(text, incdirs, seen):
    """Drop local includes, and a system header a source before this one
    already included outside any #if.

    One file a source, each included <string.h>; here that was thirty
    includes of it in one file. They cost nothing where a guard works,
    but pcc's preprocessor, re-reading a guarded header at some byte
    offsets, writes a stray character into its output, and whether it
    does moved with every edit to the library. An include under an #if
    is kept wherever it stands: what it depends on is not known here.
    """
    out = []
    depth = 0
    for line in text.splitlines():
        if re.match(r'^\s*#\s*if', line):
            depth += 1
        elif re.match(r'^\s*#\s*endif', line):
            depth -= 1
        m = SYSTEM_INCLUDE.match(line)
        if m is not None and depth == 0:
            if m.group(1) in seen:
                continue
            seen.add(m.group(1))
        out.extend(resolve(line, incdirs))
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


def include_dirs(src):
    return [os.path.join(src, "lib", c, "include", "configforge")
            for c in COMPONENTS]


def check_complete(src):
    """Every .c under lib/*/src/ has to be in SOURCES, and every .h of
    lib/*/include/configforge/ in HEADERS.

    Relying on a test program to notice a missing file only works when
    that program happens to use a symbol from it - a negative test here
    dropped core/iterate.c and nothing failed, because the differential
    program did not iterate. The list is checked against the directory
    instead, which does not depend on what anyone remembered to call.
    """
    on_disk = set()
    lib = os.path.join(src, "lib")
    for c in COMPONENTS + ["lanes"]:
        for root, _dirs, files in os.walk(os.path.join(lib, c, "src")):
            for name in files:
                if name.endswith(".c"):
                    rel = os.path.relpath(os.path.join(root, name), lib)
                    on_disk.add(rel.replace(os.sep, "/"))

    listed = set(SOURCES)
    missing = sorted(on_disk - listed)
    extra = sorted(listed - on_disk)

    hdr_disk = set()
    for d in include_dirs(src):
        for name in os.listdir(d):
            if name.endswith(".h"):
                hdr_disk.add(name)
    hdr_missing = sorted(hdr_disk - set(HEADERS) - NOT_AMALGAMATED)

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
            sys.stderr.write(f"amalgamate: {p}\n")
        sys.stderr.write("amalgamate: the file list and the tree disagree\n")
        return 1
    incdirs = include_dirs(src)
    os.makedirs(out, exist_ok=True)

    head = [
        notice(src) +
        "/*\n"
        " * configforge/configforge.h - single-file distribution.\n"
        " *\n"
        " * Generated by scripts/generate/amalgamate.py from the real sources. Do not\n"
        " * edit: every change belongs upstream, where the tests are.\n"
        " */\n\n"
        "#ifndef CONFIGFORGE_AMALGAMATED_H\n"
        "#define CONFIGFORGE_AMALGAMATED_H\n"
    ]
    for h in HEADERS:
        with open(find_header(incdirs, h), encoding="utf-8") as f:
            head.append(banner(h))
            head.append(strip_header(f.read(), incdirs))
    head.append("\n#endif /* CONFIGFORGE_AMALGAMATED_H */\n")

    body = [
        notice(src) +
        "/*\n"
        " * configforge.c - single-file distribution.\n"
        " *\n"
        " * Compile this one file and include <configforge/configforge.h>.\n" " * No build\n"
        " * system, no options, no dependencies beyond the C library.\n"
        " */\n\n"
        # Before any header, where lib/platform/src/posix/posix.c has it for
        # the library built file by file: here that file comes after
        # every header has been read, which is too late (D-83).
        "/* What glibc hides in strict C89 and the POSIX platform asks\n"
        "   for: lstat, getpwuid_r, struct timespec. */\n"
        "#if defined(__linux__) && !defined(_POSIX_C_SOURCE)\n"
        "#define _POSIX_C_SOURCE 200112L\n"
        "#endif\n\n"
        "/* The library's own code sees its internal tier (D-51). */\n"
        "#define CF_INTERNAL_API 1\n\n"
        '#include <configforge/configforge.h>\n'
    ]
    # lanes under ConfigForge's prefix (cf_lanes_*), its header read from
    # its own directory, before the sources that include it.
    with open(os.path.join(src, "lib", "lanes", "include", "lanes", "lanes.h"),
              encoding="utf-8") as f:
        body.append(banner("lanes/lanes.h"))
        body.append("#define LANES_PREFIX_CF 1\n\n")
        body.append(strip_header(f.read(), incdirs))
    seen = set()
    for s in SOURCES:
        p = os.path.join(src, "lib", s)
        with open(p, encoding="utf-8") as f:
            body.append(banner(s))
            body.append(strip_source(f.read(), incdirs, seen))

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

    with open(hpath, encoding="utf-8") as f:
        hn = sum(1 for _ in f)
    with open(os.path.join(out, "configforge.c"), encoding="utf-8") as f:
        cn = sum(1 for _ in f)
    print(f"amalgamation: configforge.h {hn} lines, configforge.c {cn} lines")
    return 0


if __name__ == "__main__":
    sys.exit(main())
