"""Every platform function and capability query has a row and a definition on each platform.

    python3 platforms.py [--write] <repository>

D-220: the documents carry one table of each feature against each platform.
docs/platforms.txt is that table, one row a line, five fields separated by
'|': `feature|probe|posix|win32|none`. `probe` is the cf_platform_has_*
query that answers for the feature, or '-'. Lines starting with '#' and
empty lines are ignored. This prints one line a problem and exits 1 when

  (1) a `CF_API ... cf_platform_<name>(` declared in
      lib/platform/include/configforge/*.h has no definition at column 0
      in src/none/*.c, src/posix/*.c and src/win32/*.c (a definition in
      lib/platform/src/*.c serves all three): `<header>:<line>: <name>:
      no definition for <platform>`;
  (2) a declared cf_platform_has_* is named by no row's probe (`<name>: no
      row`), or a probe names none declared (`row '<feature>': no such
      query`);
  (3) the block of docs/Architecture.md between `<!-- platforms:begin -->`
      and `<!-- platforms:end -->` is not the table rendered from the list.

--write rewrites that block.
"""

import glob
import os
import re
import sys

DECL = re.compile(r"\b(cf_platform_[a-z0-9_]+)\s*\(")
BEGIN = "<!-- platforms:begin -->"
END = "<!-- platforms:end -->"
LIST = "docs/platforms.txt"
DOC = "docs/Architecture.md"
PLATFORMS = ("posix", "win32", "none")


def read(path):
    with open(path, encoding="utf-8", errors="replace", newline="") as f:
        return f.read().replace("\r\n", "\n")


def declared(root):
    found = []
    pattern = os.path.join(root, "lib", "platform", "include", "configforge", "*.h")
    for path in sorted(glob.glob(pattern)):
        rel = os.path.relpath(path, root).replace(os.sep, "/")
        for no, line in enumerate(read(path).split("\n"), 1):
            if "CF_API" in line:
                for name in DECL.findall(line):
                    found.append((rel, no, name))
    return found


def defined(root, platform):
    text = ""
    for sub in (platform, ""):
        pattern = os.path.join(root, "lib", "platform", "src", sub, "*.c")
        for path in sorted(glob.glob(pattern)):
            text += read(path) + "\n"
    names = set()
    for m in re.finditer(
        r"^(?![\s#/*])[^\n]*?\b(cf_platform_[a-z0-9_]+)\s*\(", text, re.MULTILINE
    ):
        names.add(m.group(1))
    return names


def rows(root):
    out = []
    path = os.path.join(root, LIST)
    if not os.path.exists(path):
        return out
    for line in read(path).split("\n"):
        if line.strip() == "" or line.startswith("#"):
            continue
        out.append([f.strip() for f in line.split("|")])
    return out


def render(table):
    lines = [
        "| Feature | Query | posix | win32 | none |",
        "| --- | --- | --- | --- | --- |",
    ]
    for r in table:
        r = r + [""] * (5 - len(r))
        query = "none" if r[1] == "-" else f"`{r[1]}`"
        lines.append(f"| {r[0]} | {query} | {r[2]} | {r[3]} | {r[4]} |")
    return "\n".join(lines)


def main():
    args = [a for a in sys.argv[1:] if a != "--write"]
    write = "--write" in sys.argv[1:]
    if len(args) != 1:
        print(__doc__)
        return 2
    root = args[0]
    problems = []
    decls = declared(root)
    for platform in PLATFORMS:
        have = defined(root, platform)
        for rel, no, name in decls:
            if name not in have:
                problems.append(f"{rel}:{no}: {name}: no definition for {platform}")
    table = rows(root)
    probes = {r[1] for r in table if len(r) > 1 and r[1] != "-"}
    queries = {n for _, _, n in decls if n.startswith("cf_platform_has_")}
    for name in sorted(queries - probes):
        problems.append(f"{name}: no row")
    for r in table:
        if len(r) != 5:
            problems.append(f"{LIST}: row '{r[0]}': not five fields")
        elif r[1] != "-" and r[1] not in queries:
            problems.append(f"{LIST}: row '{r[0]}': no such query {r[1]}")
    doc_path = os.path.join(root, DOC)
    doc = read(doc_path)
    b, e = doc.find(BEGIN), doc.find(END)
    if b < 0 or e < b:
        problems.append(f"{DOC}: the {BEGIN} and {END} markers are missing")
    else:
        want = BEGIN + "\n" + render(table) + "\n" + END
        if doc[b : e + len(END)] != want:
            if write:
                new = doc[:b] + want + doc[e + len(END) :]
                with open(doc_path, "w", encoding="utf-8", newline="") as f:
                    f.write(new)
            else:
                problems.append(
                    f"{DOC}: the table is not docs/platforms.txt's: run scripts/check/platforms.py --write ."
                )
    for p in problems:
        print(p)
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
