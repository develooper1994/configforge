#!/usr/bin/env python3
"""The performance page, kept by CI (D-111): docs/Performance.md.

    page.py --check docs/Performance.md      the static block is what this writes
    page.py --write docs/Performance.md      rewrite the static block
    page.py --validate <results.json>        a results file has schema 1
    page.py --history <checkout> --out docs/performance [--page docs/Performance.md]
                                             the history block and the charts

The page has two generated blocks. The static one, between
`<!-- bench:static:begin -->` and `<!-- bench:static:end -->`, is made from
files in the repository (bench/compare/libraries.cmake, settings.json,
documents.json, probes.expected) and committed; `gate.perfpage` fails when the
committed block differs from what --write makes. The history one, between
`<!-- bench:history:begin -->` and `<!-- bench:history:end -->`, is made when
the site is built, from the branch bench-history, and never committed: each
format's line over time as SVG, the commit of each point its hover and link,
and the places where ConfigForge is behind first.

Python 3 standard library only: the docs environment has no plotting library.
"""

import glob
import html
import json
import os
import re
import statistics
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
COMPARE = os.path.join(ROOT, "bench", "compare")

STATIC = ("<!-- bench:static:begin -->", "<!-- bench:static:end -->")
HISTORY = ("<!-- bench:history:begin -->", "<!-- bench:history:end -->")
NO_HISTORY = "No history yet: the page is filled by CI from the branch bench-history."
FORMATS = ["json", "yaml", "toml", "cbor", "ini", "xml"]
FORMAT_NAMES = {"json": "JSON", "yaml": "YAML", "toml": "TOML", "cbor": "CBOR",
                "ini": "INI", "xml": "XML"}
BEHIND, AHEAD = 0.97, 1.03
CHECK_MESSAGE = ("docs/Performance.md is not what scripts/bench/page.py writes: "
                 "run python3 scripts/bench/page.py --write docs/Performance.md")


def load_json(name):
    with open(os.path.join(COMPARE, name), encoding="utf-8") as f:
        return json.load(f)


def read(path):
    with open(path, encoding="utf-8", newline="") as f:
        return f.read().replace("\r\n", "\n")


def libraries_pins():
    text = read(os.path.join(COMPARE, "libraries.cmake"))
    pins = {}
    for m in re.finditer(r'set\(LIB_(\w+?)_(URL|TAG|COMMIT|DATE|LICENSE)\s+"([^"]*)"\)', text):
        pins.setdefault(m.group(1), {})[m.group(2)] = m.group(3)
    return pins


def cell_text(s):
    return s.replace("|", "\\|").replace("\n", " ")


# ---- the static block -------------------------------------------------------

def static_block():
    settings = load_json("settings.json")["libraries"]
    docs = load_json("documents.json")
    pins = libraries_pins()
    out = []
    out.append("### The libraries\n")
    out.append("Each library's sources are compiled by this project's compiler with "
               "one set of flags, and read the same files. One row a library: its "
               "pinned version, the settings it is given, and what each gets.\n")
    out.append("| Library | Version | Commit | Licence | Reads | Settings |")
    out.append("| ------- | ------- | ------ | ------- | ----- | -------- |")
    for key, lib in settings.items():
        pin = pins.get(key, {})
        if key == "configforge":
            version, commit, licence = "this tree", "-", "MIT"
            title = lib["title"]
        elif key == "expat":
            version, commit, licence = "the system's (libexpat1-dev)", "-", "MIT"
            title = lib["title"]
        else:
            version = pin.get("TAG") or ("commit of " + pin.get("DATE", "?"))
            commit = "`{}`".format(pin.get("COMMIT", "?")[:12])
            licence = pin.get("LICENSE", "?")
            title = "[{}]({})".format(lib["title"], pin.get("URL", ""))
        reads = ", ".join(FORMAT_NAMES[f] for f in lib["formats"])
        out.append("| {} | {} | {} | {} | {} | {} |".format(
            title, cell_text(version), commit, licence, reads, cell_text(lib["settings"])))
    out.append("")
    notes = [(lib["title"], lib) for lib in settings.values() if lib["notes"] or lib["refuses"]]
    if notes:
        out.append("Notes on a library:\n")
        for title, lib in notes:
            text = lib["notes"]
            if lib["refuses"]:
                text += " Refused in this round: " + ", ".join(
                    "`{}` ({})".format(r["doc"], r["why"]) for r in lib["refuses"]) + "."
            out.append(f"* **{title}**: {text.strip()}")
        out.append("")
    out.append("Not compared, and said so: JSON5, JSONC, `.properties`, dotenv and TLV "
               "(no library found whose job is the same), and pugixml, a DOM for XML "
               "(second round, with libfyaml and libcbor).\n")

    out.append("### The documents\n")
    out.append("Each is chosen to hurt a reader. A letter says the document exists in "
               "that format (J JSON, Y YAML, T TOML, C CBOR, I INI, X XML), a blank that "
               "the format cannot say it. Sizes 1, 16 and 256 MiB.\n")
    out.append("| Name | What it is | J | Y | T | C | I | X |")
    out.append("| ---- | ---------- | - | - | - | - | - | - |")
    for d in docs["documents"]:
        marks = " | ".join(c if c in d["formats"] else " " for c in "JYTCIX")
        out.append("| `{}` | {} | {} |".format(d["name"], cell_text(d["what"]), marks))
    out.append("")

    out.append("### What each library checks\n")
    out.append("`cfb_compare --mode probe` gives every reader the same eight probe "
               "documents per format (invalid UTF-8 in a string, a duplicate key, an "
               "integer past int64, a truncated document, text after the root value, "
               "nesting 100 deep, a lone surrogate escape, a NUL inside a string) in "
               "strict mode, which turns on every check the library offers that ConfigForge "
               "makes. A line is a probe ConfigForge refuses and the library reads: the "
               "gap is shown, not hidden. The table is `bench/compare/probes.expected`.\n")
    probes = {}
    path = os.path.join(COMPARE, "probes.expected")
    if os.path.exists(path):
        for ln in read(path).splitlines():
            parts = ln.split()
            probes[(parts[0], parts[1], parts[2])] = dict(p.split("=") for p in parts[3:])
    out.append("| Library | Format | Does not check (strict) |")
    out.append("| ------- | ------ | ----------------------- |")
    for (adapter, fmt, mode), res in probes.items():
        if adapter == "configforge" or mode != "strict":
            continue
        ref = probes.get(("configforge", fmt, "strict"), {})
        gap = [p for p, v in res.items() if ref.get(p) == "refuse" and v == "read"]
        out.append("| {} | {} | {} |".format(adapter, FORMAT_NAMES[fmt],
                                          ", ".join(gap) if gap else "nothing ConfigForge checks"))
    out.append("")
    return "\n".join(out)


def split_page(text, marks):
    a, b = marks
    i, j = text.find(a), text.find(b)
    if i < 0 or j < 0 or j < i:
        sys.exit("page: {} and {} are not both in the page".format(*marks))
    return text[: i + len(a)], text[j:]


def render_page(text):
    head, tail = split_page(text, STATIC)
    return head + "\n\n" + static_block() + "\n" + tail


def check(page):
    have = read(page)
    if render_page(have) != have:
        print(CHECK_MESSAGE)
        return 1
    print(f"page: {page} is what page.py writes")
    return 0


def write(page):
    text = render_page(read(page))
    with open(page, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)
    return 0


# ---- a results file ---------------------------------------------------------

def validate(path):
    try:
        with open(path, encoding="utf-8") as f:
            r = json.load(f)
    except (OSError, ValueError) as e:
        print(f"page: {path}: {e}")
        return 1
    bad = []
    if r.get("schema") != 1:
        bad.append("schema is not 1")
    if r.get("kind") not in ("quick", "full"):
        bad.append("kind is not quick or full")
    if not re.fullmatch(r"[0-9a-f]{40}", str(r.get("commit", ""))):
        bad.append("commit is not 40 hex digits")
    for k in ("committed_at", "run_id", "machine", "compiler", "libraries", "cells"):
        if k not in r:
            bad.append("no " + k)
    # The file may come from a run of a pull request (CI's artifact bench-quick,
    # read by .github/workflows/bench.yml with a token that writes), and
    # publish.sh makes a path and a commit message of these fields: each must
    # be exactly what run.py writes, and nothing a path or a shell could read.
    if not re.fullmatch(r"[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}Z", str(r.get("committed_at", ""))):
        bad.append("committed_at is not YYYY-MM-DDThh:mm:ssZ")
    if not re.fullmatch(r"[0-9]{1,20}", str(r.get("run_id", ""))):
        bad.append("run_id is not a number")
    compiler = r.get("compiler")
    if not isinstance(compiler, dict):
        bad.append("compiler is not an object")
    else:
        if compiler.get("id") not in ("gcc", "clang"):
            bad.append("compiler id is not gcc or clang")
        if compiler.get("flags", "shipped") not in ("shipped", "bare", "native"):
            bad.append("compiler flags is not shipped, bare or native")
    if not isinstance(r.get("cells", []), list):
        bad.append("cells is not a list")
    for c in r.get("cells", []) if isinstance(r.get("cells", []), list) else []:
        for k in ("lib", "format", "doc", "size_mib", "mode"):
            if k not in c:
                bad.append("a cell has no " + k)
                break
    if bad:
        print("page: {}: {}".format(path, "; ".join(bad)))
        return 1
    print("page: %s: schema 1, %s, %d cells" % (path, r["kind"], len(r["cells"])))
    return 0


# ---- the history ------------------------------------------------------------

def commit_base():
    repo = os.environ.get("GITHUB_REPOSITORY")
    if repo:
        return f"https://github.com/{repo}/commit/"
    try:
        url = subprocess.run(["git", "-C", ROOT, "remote", "get-url", "origin"],
                             capture_output=True, text=True, check=False).stdout.strip()
    except OSError:
        return ""
    m = re.search(r"github\.com[:/](.+?)(\.git)?$", url)
    return f"https://github.com/{m.group(1)}/commit/" if m else ""


def load_runs(checkout):
    runs = []
    for p in sorted(glob.glob(os.path.join(checkout, "runs", "**", "*.json"), recursive=True)):
        try:
            with open(p, encoding="utf-8") as f:
                r = json.load(f)
        except (OSError, ValueError):
            continue
        if r.get("schema") == 1 and "cells" in r:
            runs.append(r)
    runs.sort(key=lambda r: r.get("committed_at", ""))
    return runs


def cells_of(run, **want):
    return [c for c in run["cells"] if all(c.get(k) == v for k, v in want.items())]


def headline(run):
    return run.get("compiler", {}).get("id") == "gcc" and run.get("compiler", {}).get("flags", "shipped") == "shipped"


COLORS = ["#1b6ec2", "#c2410c", "#15803d", "#7e22ce", "#b45309", "#0e7490", "#be123c", "#4d7c0f"]


def svg_chart(title, ylabel, series, base, note=""):
    """series: [(name, dashed, [(x, y, sha, label)])] -> an SVG document."""
    w, h, ml, mr, mt, mb = 760, 340, 64, 16, 44, 40
    pts = [p for _, _, ps in series for p in ps]
    parts = ['<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 %d %d" role="img" '
             'aria-label="%s" font-family="sans-serif" font-size="11">' % (w, h, html.escape(title))]
    parts.append('<rect width="%d" height="%d" fill="#ffffff"/>' % (w, h))
    parts.append('<text x="%d" y="18" font-size="13" font-weight="bold">%s</text>' % (ml, html.escape(title)))
    if not pts:
        parts.append('<text x="%d" y="%d" fill="#555">no data yet</text>' % (ml, h // 2))
        parts.append("</svg>")
        return "\n".join(parts) + "\n"
    xs = [p[0] for p in pts]
    x0, x1 = min(xs), max(xs)
    if x1 == x0:
        x1 = x0 + 1
    ymax = max(p[1] for p in pts) * 1.1 or 1.0

    def X(x):
        return ml + (x - x0) / (x1 - x0) * (w - ml - mr) if len(set(xs)) > 1 else ml + (w - ml - mr) / 2

    def Y(y):
        return h - mb - y / ymax * (h - mb - mt)

    for i in range(6):
        v = ymax * i / 5
        parts.append('<line x1="%d" y1="%.1f" x2="%d" y2="%.1f" stroke="#ddd"/>' % (ml, Y(v), w - mr, Y(v)))
        parts.append('<text x="%d" y="%.1f" text-anchor="end" fill="#555">%s</text>' % (ml - 6, Y(v) + 4, (f"{v:.0f}") if ymax >= 20 else (f"{v:.2f}")))
    parts.append('<text x="14" y="%d" fill="#555" transform="rotate(-90 14 %d)">%s</text>' % (
        h // 2, h // 2, html.escape(ylabel)))
    parts.append('<text x="%d" y="%d" fill="#555">older</text>' % (ml, h - 22))
    parts.append('<text x="%d" y="%d" fill="#555" text-anchor="end">newer: the x axis is the commit date</text>' % (w - mr, h - 22))
    lx = ml
    for i, (name, dashed, ps) in enumerate(series):
        color = COLORS[i % len(COLORS)]
        parts.append(f'<g class="series" data-name="{html.escape(name)}">')
        line = " ".join(f"{X(p[0]):.1f},{Y(p[1]):.1f}" for p in ps)
        if len(ps) > 1:
            parts.append('<polyline fill="none" stroke="{}" stroke-width="{}"{} points="{}"/>'.format(
                color, "1.5" if dashed else "2.5", ' stroke-dasharray="5 3"' if dashed else "", line))
        for x, y, sha, label in ps:
            circle = '<circle cx="{:.1f}" cy="{:.1f}" r="{}" fill="{}"><title>{}</title></circle>'.format(
                X(x), Y(y), "3.5" if dashed else "2.5", color, html.escape(label))
            parts.append(f'<a href="{base}{sha}" target="_top">{circle}</a>' if base else circle)
        parts.append("</g>")
        parts.append('<text x="%d" y="34" fill="%s">%s</text>' % (lx, color, html.escape(name)))
        lx += 9 * len(name) + 24
    parts.append("</svg>")
    return "\n".join(parts) + "\n"


def timestamp(run):
    s = run.get("committed_at", "")
    m = re.match(r"(\d+)-(\d+)-(\d+)T(\d+):(\d+):(\d+)", s)
    if not m:
        return 0
    y, mo, d, hh, mi, ss = map(int, m.groups())
    return ((y * 12 + mo) * 31 + d) * 86400 + hh * 3600 + mi * 60 + ss


def charts(runs, out, base):
    os.makedirs(out, exist_ok=True)
    adapters = {}
    for r in runs:
        for c in r["cells"]:
            adapters.setdefault(c["format"], set()).add(c["lib"])
    for fmt in FORMATS:
        speed, instr = {}, {}
        for r in runs:
            full = r["kind"] == "full"
            if full and not headline(r):
                continue
            sha = r["commit"][:7]
            x = timestamp(r)
            for c in r["cells"]:
                if c["format"] != fmt or c["doc"] != "mixed":
                    continue
                lib = c["lib"]
                if lib != "configforge" and not full:
                    continue
                if c["mode"] == "speed" and c["size_mib"] == (16 if full else 4) and "mib_s" in c:
                    speed.setdefault(lib, []).append(
                        (x, c["mib_s"], r["commit"], "{} {}: {:.1f} MiB/s".format(sha, r.get("committed_at", ""), c["mib_s"])))
                if c["mode"] == "count" and c.get("instructions") and c.get("bytes"):
                    v = c["instructions"] / c["bytes"]
                    instr.setdefault("%s %d MiB" % (lib, c["size_mib"]), []).append(
                        (x, v, r["commit"], "{} {}: {:.2f} instructions per byte".format(sha, r.get("committed_at", ""), v)))
        for name, data, ylabel, title, dashed_other in (
                ("speed", speed, "MiB/s", f"{FORMAT_NAMES[fmt]}: speed, mixed, 16 MiB", True),
                ("instr", instr, "instructions per byte", f"{FORMAT_NAMES[fmt]}: instructions per byte, mixed", True)):
            keys = sorted(data, key=lambda k: (not k.startswith("configforge"), k))
            series = [(k, not k.startswith("configforge"), list(data[k]))
                      for k in keys]
            with open(os.path.join(out, f"{fmt}-{name}.svg"), "w", encoding="utf-8", newline="\n") as f:
                f.write(svg_chart(title, ylabel, series, base))


def ratios(runs):
    """Rows of the comparison, from the latest three headline full runs."""
    full = [r for r in runs if r["kind"] == "full" and headline(r)][-3:]
    table = {}
    for r in full:
        best = {}
        cf = {}
        for c in r["cells"]:
            if c["mode"] != "speed" or "mib_s" not in c:
                continue
            key = (c["format"], c["doc"], c["size_mib"])
            if c["lib"] == "configforge":
                cf[key] = c
            else:
                best.setdefault(key, []).append(c)
        for key, c in cf.items():
            others = [o for o in best.get(key, []) if o.get("accepted") == c.get("accepted")]
            if not others or c["mib_s"] <= 0:
                continue
            top = max(others, key=lambda o: o["mib_s"])
            table.setdefault(key, []).append((c["mib_s"] / top["mib_s"], c["mib_s"], top["lib"], top["mib_s"], r["commit"]))
    rows = []
    for key, v in table.items():
        ratio = statistics.median(x[0] for x in v)
        last = v[-1]
        rows.append((ratio, key, statistics.median(x[1] for x in v), last[2], last[3], last[4]))
    return rows


def history_block(runs, base, outdir):
    if not runs:
        return NO_HISTORY + "\n"
    out = []
    rows = ratios(runs)

    def table(title, sel, heading, expat=False):
        out.append(f"#### {title}\n")
        if not sel:
            out.append("None in the latest runs.\n")
            return
        out.append("| Format | Document | MiB | ConfigForge MiB/s | {} | {} MiB/s | ConfigForge against it | Commit |".format(
            "expat" if expat else "Best other", "expat" if expat else "its"))
        out.append("| ------ | -------- | --: | ----------------: | ---------- | -------: | ---------------------: | ------ |")
        for ratio, key, cf, lib, other, commit in sel:
            link = f"[`{commit[:7]}`]({base}{commit})" if base else f"`{commit[:7]}`"
            out.append("| %s | `%s` | %d | %.1f | %s | %.1f | %.0f%% | %s |" % (
                FORMAT_NAMES[key[0]], key[1], key[2], cf, lib, other, ratio * 100, link))
        out.append("")

    non_xml = [r for r in rows if r[1][0] != "xml"]
    xml = [r for r in rows if r[1][0] == "xml"]
    out.append("The losses come first: every document, in every format, where ConfigForge is under "
               "97% of the best other library's speed (the median of the latest three weekly "
               "runs), worst first; then the level ones (within 3%) and the wins. The table is "
               "above the charts.\n")
    table("Where ConfigForge is behind", sorted([r for r in non_xml if r[0] < BEHIND], key=lambda r: r[0]), "")
    table("Level", sorted([r for r in non_xml if BEHIND <= r[0] <= AHEAD], key=lambda r: r[0]), "")
    table("Ahead", sorted([r for r in non_xml if r[0] > AHEAD], key=lambda r: -r[0]), "")
    out.append("The XML reader is expat with the layer's own handlers and flattening on top, so "
               "its line is expat's plus what the layer costs: the rows below are an overhead "
               "of the layer, not a place where it is behind.\n")
    table("Overhead of the layer over expat", sorted(xml, key=lambda r: r[0]), "", expat=True)

    out.append("#### Each format over time\n")
    out.append("ConfigForge's line is from every push (the quick run); each other library's is "
               "from the weekly runs, dashed, a marker a run. Each point is a commit: hover for "
               "its number, click for the commit. The instruction chart is the trend; the "
               "speed chart is noisy by the runner.\n")
    for fmt in FORMATS:
        out.append(f"##### {FORMAT_NAMES[fmt]}\n")
        out.append(f"![{FORMAT_NAMES[fmt]} speed over commits](performance/{fmt}-speed.svg)\n")
        out.append(f"![{FORMAT_NAMES[fmt]} instructions per byte over commits](performance/{fmt}-instr.svg)\n")

    fulls = [r for r in runs if r["kind"] == "full" and headline(r)]
    if fulls:
        last = fulls[-1]
        out.append("#### Size and memory\n")
        sizes = {s["lib"]: s["bytes_added"] for s in last.get("sizes", [])}
        mem = {}
        for c in last["cells"]:
            if c["mode"] == "memory" and c["doc"] == "mixed" and "peak_growth_kib" in c:
                mem.setdefault(c["lib"], []).append((c["format"], c["peak_growth_kib"]))
        out.append("Code a library adds to a program, and the peak growth of one parse of `mixed` at "
                   "16 MiB, from the run of `{}`. ConfigForge's rows are marked.\n".format(last["commit"][:7]))
        out.append("| Library | Code added (KiB) | Peak growth, mixed 16 MiB (MiB) |")
        out.append("| ------- | ---------------: | ------------------------------: |")
        names = sorted(set(sizes) | set(mem), key=lambda n: sizes.get(n, 0))
        for n in names:
            mark = "**%s**" if n.startswith("configforge") else "%s"
            peaks = ", ".join(f"{f} {kib / 1024.0:.1f}" for f, kib in sorted(mem.get(n, [])))
            out.append("| {} | {:.1f} | {} |".format(mark % n, sizes.get(n, 0) / 1024.0, peaks or "-"))
        out.append("")
        out.append("#### What each library refuses\n")
        out.append("The probe table of the latest run (strict), and what each reader did with the "
                   "alias bomb `yaml-aliases` (nine levels of aliases of aliases):\n")
        out.append("```text")
        out.extend(last.get("probes", []))
        out.append("```\n")
        out.append("| Library | `yaml-aliases` | Time |")
        out.append("| ------- | -------------- | ---: |")
        for c in last["cells"]:
            if c["doc"] == "yaml-aliases" and c["mode"] == "speed" and "mib_s" in c:
                ms = c.get("bytes", 0) / 1048576.0 / c["mib_s"] * 1000.0 if c["mib_s"] else 0
                out.append("| {} | {} | {:.1f} ms |".format(c["lib"], "read" if c.get("accepted") else "refused", ms))
        out.append("")
    charts(runs, outdir, base)
    return "\n".join(out)


def history(checkout, outdir, page):
    runs = load_runs(checkout) if checkout and os.path.isdir(checkout) else []
    base = commit_base()
    block = history_block(runs, base, outdir)
    if not runs:
        charts([], outdir, base)
    if page and os.path.exists(page):
        text = read(page)
        head, tail = split_page(text, HISTORY)
        with open(page, "w", encoding="utf-8", newline="\n") as f:
            f.write(head + "\n\n" + block + "\n" + tail)
    return 0


def main(argv):
    args = list(argv)
    if len(args) == 2 and args[0] == "--check":
        return check(args[1])
    if len(args) == 2 and args[0] == "--write":
        return write(args[1])
    if len(args) == 2 and args[0] == "--validate":
        return validate(args[1])
    if args and args[0] == "--history":
        checkout = args[1] if len(args) > 1 else ""
        outdir, page = "docs/performance", "docs/Performance.md"
        i = 2
        while i < len(args):
            if args[i] == "--out" and i + 1 < len(args):
                outdir = args[i + 1]
                i += 2
            elif args[i] == "--page" and i + 1 < len(args):
                page = args[i + 1]
                i += 2
            else:
                break
        return history(checkout, outdir, page)
    print(__doc__.strip().split("\n")[0])
    print("usage: page.py --check|--write <page> | --validate <results> | --history <checkout> --out <dir> [--page <page>]")
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
