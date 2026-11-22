#!/usr/bin/env python3
"""The driver of the comparison (D-111): runs the cells, writes one JSON file.

    run.py --scope quick|full --build <dir> --cfbench <cf_bench>
           [--docs <dir>] [--sizes 1,16,256] [--reps N] [--out <file>]
           [--flags shipped|bare|native] [--mixed-only] [--cold]
    run.py --check touch|probes --compare <cfb_compare> --cfbench <cf_bench>
           --work <dir>

`--build` is the bench/compare build directory (cfb_compare, size/size_*), and
`--cfbench` the benchmark that writes the documents (cf_bench --emit-docs). The
documents are made once, here, and every library reads the same files.

`quick` is ConfigForge alone: the `mixed`, `strings-plain`, `strings-escape`,
`whitespace` and `numbers-edge` documents of each format that has them (the
table in bench/compare/documents.json) at 4 MiB in speed (9 reps) and count,
and `mixed` at 1 MiB in speed. `full` is every library and every document:
speed and count at 1 MiB, speed at 16 and 256 MiB, memory at 16 MiB, the size
rows once, the probes once.

The result, schema version 1:

    {"schema": 1, "kind": "quick"|"full", "commit": "<40 hex>",
     "committed_at": "<UTC>", "run_id": "<n>", "machine": {...},
     "compiler": {"id", "version", "flags"}, "libraries": [{name, version,
     commit}], "cells": [{lib, format, doc, size_mib, mode, ...}],
     "sizes": [{lib, bytes_added}], "probes": [...]}

Python 3 standard library only.
"""

import argparse
import datetime
import json
import os
import platform
import re
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
COMPARE = os.path.join(ROOT, "bench", "compare")

QUICK_DOCS = ["mixed", "strings-plain", "strings-escape", "whitespace", "numbers-edge"]
SKIP_TOUCH = ("invalid-late", "yaml-aliases")  # refused by design, or expanded
COUNTERS = ["containers", "strings", "string_bytes", "numbers", "bools", "nulls"]


def load_json(name):
    with open(os.path.join(COMPARE, name), encoding="utf-8") as f:
        return json.load(f)


def sh(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, check=False, **kw)


def first_line(cmd):
    try:
        r = sh(cmd)
        out = (r.stdout or r.stderr).strip().splitlines()
        return out[0] if out else "unknown"
    except OSError:
        return "unknown"


def utc(ts=None):
    return datetime.datetime.fromtimestamp(ts if ts is not None else datetime.datetime.now().timestamp(),
                                           datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


# ---- the documents --------------------------------------------------------

def emit_docs(cfbench, docs, sizes):
    """Writes the documents of `sizes` into `docs`; {file: (bytes, fnv)}."""
    os.makedirs(docs, exist_ok=True)
    lines = {}
    for s in sizes:
        r = sh([cfbench, "--emit-docs", docs, "--size", str(s)])
        if r.returncode != 0:
            sys.exit(f"run: cf_bench --emit-docs failed: {r.stderr.strip()}")
        for ln in r.stdout.splitlines():
            name, n, fnv = ln.split()
            lines[name] = (int(n), fnv)
    return lines


def doc_info(file):
    """('json', 'mixed', 16) from 'json-mixed-16.json' or 'json-many-tiny-1.bin'."""
    base = os.path.basename(file)
    stem, ext = base.rsplit(".", 1)
    if stem.startswith("yaml-aliases-"):  # the one document named for its format
        return "yaml", "yaml-aliases", int(stem.split("-")[-1])
    parts = stem.split("-")
    size = int(parts[-1])
    fmt = parts[0] if ext == "bin" else ext
    name = "-".join(parts[1:-1]) if parts[0] == fmt else "-".join(parts[:-1])
    return fmt, name, size


def adapters_of(compare):
    r = sh([compare, "--list"])
    if r.returncode != 0:
        sys.exit("run: cfb_compare --list failed")
    return [json.loads(ln) for ln in r.stdout.splitlines() if ln.strip()]


# ---- cells ----------------------------------------------------------------

def taskset():
    return ["taskset", "-c", "1"] if shutil.which("taskset") else []


def run_json(cmd, timeout=3600):
    r = sh(cmd, timeout=timeout)
    if r.returncode != 0 or not r.stdout.strip():
        return {"failed": (r.stderr or "no output").strip()[-300:]}
    return json.loads(r.stdout.strip().splitlines()[-1])


def instructions(compare, adapter, doc, workdir):
    """Instructions of one parse, from callgrind's summary line, or None."""
    if not shutil.which("valgrind"):
        return None
    out = os.path.join(workdir, "callgrind.out")
    if os.path.exists(out):
        os.remove(out)
    sh(["valgrind", "--tool=callgrind", "--collect-atstart=no",
            "--toggle-collect=cfb_cell", "--callgrind-out-file=" + out,
            compare, "--lib", adapter, "--doc", doc, "--mode", "count"], timeout=7200)
    try:
        with open(out, encoding="utf-8", errors="replace") as f:
            for ln in f:
                if ln.startswith("summary:"):
                    return int(ln.split()[1])
    except OSError:
        pass
    return None


def cell(adapter, fmt, doc, size, mode):
    return {"lib": adapter, "format": fmt, "doc": doc, "size_mib": size, "mode": mode}


def run_cells(args, compare, docs, adapters, kind):
    cells = []
    work = tempfile.mkdtemp(prefix="cfb-run-")
    files = sorted(f for f in os.listdir(docs) if not f.endswith(".idx"))
    sizes = [int(s) for s in args.sizes.split(",")]
    for f in files:
        fmt, name, size = doc_info(f)
        if size not in sizes and not (size == 1 and name == "yaml-aliases"):
            continue
        if kind == "quick":
            ok = (size == 4 and name in QUICK_DOCS) or (size == 1 and name == "mixed")
            if not ok:
                continue
        elif args.mixed_only and name != "mixed":
            continue
        path = os.path.join(docs, f)
        for ad in adapters:
            if ad["format"] != fmt:
                continue
            if kind == "quick" and ad["adapter"] != "configforge":
                continue
            if args.only and ad["lib"] not in args.only.split(","):
                continue
            modes = []
            if kind == "quick":
                if size == 4:
                    modes = ["speed", "count"]
                else:
                    modes = ["speed"]
            else:
                if size == 1:
                    modes = ["speed", "count"]
                else:
                    modes = ["speed"]
                if size == 16:
                    modes.append("memory")
                if args.cold and size == 256 and name == "mixed":
                    modes.append("cold")
            if args.mixed_only:
                modes = [m for m in modes if m in ("speed", "cold")]
            for mode in modes:
                c = cell(ad["adapter"], fmt, name, size, mode)
                if mode == "speed" or mode == "cold":
                    reps = args.reps or (3 if size >= 256 else 9)
                    cmd = taskset() + [compare, "--lib", ad["adapter"], "--doc", path,
                                       "--mode", "speed", "--reps", str(reps)]
                    if mode == "cold":
                        cmd.append("--cold")
                    c.update(run_json(cmd))
                elif mode == "count":
                    j = run_json([compare, "--lib", ad["adapter"], "--doc", path, "--mode", "count"])
                    c.update(j)
                    c["instructions"] = instructions(compare, ad["adapter"], path, work)
                elif mode == "memory":
                    c.update(run_json([compare, "--lib", ad["adapter"], "--doc", path, "--mode", "memory"]))
                c.pop("strict", None)
                c.update({"lib": ad["adapter"], "format": fmt, "doc": name, "file": f,
                          "size_mib": size, "mode": mode})
                cells.append(c)
                sys.stderr.write("run: %s %s %s %d %s\n" % (ad["adapter"], fmt, name, size, mode))
    shutil.rmtree(work, ignore_errors=True)
    return cells


def size_rows(build):
    """Bytes each program adds over the one with an empty adapter."""
    d = os.path.join(build, "size")
    empty = os.path.join(d, "size_empty")
    if not os.path.exists(empty):
        return []
    base = os.path.getsize(empty)
    rows = []
    for f in sorted(os.listdir(d)):
        p = os.path.join(d, f)
        if f.startswith("size_") and f != "size_empty" and os.path.isfile(p) and os.access(p, os.X_OK):
            rows.append({"lib": f[len("size_"):], "bytes_added": os.path.getsize(p) - base})
    return rows


def probes(compare):
    r = sh([compare, "--mode", "probe"])
    return [ln for ln in r.stdout.splitlines() if ln.strip()]


# ---- the machine, the compiler, the libraries ----------------------------

def machine():
    cpu = "unknown"
    try:
        with open("/proc/cpuinfo", encoding="utf-8") as f:
            for ln in f:
                if ln.startswith("model name"):
                    cpu = ln.split(":", 1)[1].strip()
                    break
    except OSError:
        pass
    gov = "unknown"
    try:
        with open("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor", encoding="utf-8") as f:
            gov = f.read().strip()
    except OSError:
        pass
    return {
        "uname": f"{platform.system()} {platform.release()}",
        "cpu": cpu,
        "cpus": os.cpu_count(),
        "libc": first_line(["ldd", "--version"]),
        "image_os": os.environ.get("ImageOS", "unknown"),
        "image_version": os.environ.get("ImageVersion", "unknown"),
        "governor": gov,
    }


def compiler_info(build, flags):
    cc = "c++"
    try:
        with open(os.path.join(build, "CMakeCache.txt"), encoding="utf-8") as f:
            for ln in f:
                if ln.startswith("CMAKE_CXX_COMPILER:"):
                    cc = ln.split("=", 1)[1].strip()
    except OSError:
        pass
    line = first_line([cc, "--version"])
    ident = "clang" if "clang" in line.lower() else "gcc"
    return {"id": ident, "version": line, "flags": flags}


def libraries(adapters):
    """The pinned versions, from bench/compare/libraries.cmake."""
    with open(os.path.join(COMPARE, "libraries.cmake"), encoding="utf-8") as f:
        text = f.read()
    pins = {}
    for m in re.finditer(r'set\(LIB_(\w+?)_(TAG|COMMIT)\s+"([^"]*)"\)', text):
        pins.setdefault(m.group(1), {})[m.group(2)] = m.group(3)
    seen = {}
    for a in adapters:
        lib = a["lib"]
        if lib in seen:
            continue
        p = pins.get(lib, {})
        seen[lib] = {"name": lib, "version": a["version"], "commit": p.get("COMMIT", "")}
    return list(seen.values())


def git(*a):
    r = sh(["git", "-C", ROOT] + list(a))
    return r.stdout.strip()


# ---- the checks ctest runs ------------------------------------------------

def check_touch(compare, cfbench, work):
    """Every adapter of a format gives ConfigForge's counters on every 1 MiB
    document it can read (bench.touch)."""
    docs = os.path.join(work, "docs1")
    emit_docs(cfbench, docs, [1])
    adapters = adapters_of(compare)
    settings = load_json("settings.json")["libraries"]
    bad = 0
    for f in sorted(os.listdir(docs)):
        if f.endswith(".idx"):
            continue
        fmt, name, _size = doc_info(f)
        if name in SKIP_TOUCH:
            continue
        path = os.path.join(docs, f)
        got = {}
        for ad in adapters:
            if ad["format"] != fmt:
                continue
            j = run_json([compare, "--lib", ad["adapter"], "--doc", path, "--mode", "count"])
            if "touch" not in j:
                print("bench.touch: {} on {}: {}".format(ad["adapter"], f, j.get("failed")))
                bad += 1
                continue
            got[ad["adapter"]] = (ad, j)
        ref = got.get("configforge")
        if ref is None:
            continue
        if not ref[1]["accepted"]:
            print(f"bench.touch: ConfigForge refuses {f}")
            bad += 1
            continue
        for ad, j in got.values():
            if ad["adapter"] == "configforge":
                continue
            st = settings.get(ad["lib"], {})
            refused_ok = any(r["doc"] == name for r in st.get("refuses", []))
            if not j["accepted"]:
                if not refused_ok:
                    print("bench.touch: {} refuses {} and settings.json does not say it may".format(ad["adapter"], f))
                    bad += 1
                continue
            exempt = set()
            for e in st.get("touch_exempt", []):
                if e["doc"] == name:
                    exempt.update(e["counters"])
            for k in COUNTERS:
                if k not in exempt and j["touch"][k] != ref[1]["touch"][k]:
                    print("bench.touch: %s on %s: %s is %d, ConfigForge's is %d"
                          % (ad["adapter"], f, k, j["touch"][k], ref[1]["touch"][k]))
                    bad += 1
                    break
    print("bench.touch: %s" % ("failed" if bad else "ok"))
    return 1 if bad else 0


def check_probes(compare):
    """The probe table equals bench/compare/probes.expected (bench.probes)."""
    have = probes(compare)
    path = os.path.join(COMPARE, "probes.expected")
    with open(path, encoding="utf-8") as f:
        want = [ln.rstrip("\n") for ln in f if ln.strip()]
    if have == want:
        print("bench.probes: ok")
        return 0
    for a, b in zip(have, want):
        if a != b:
            print(f"bench.probes: differs\n  have: {a}\n  want: {b}")
            break
    else:
        print("bench.probes: %d lines, expected %d" % (len(have), len(want)))
    print("If the change is meant: run.py --write-probes --compare <cfb_compare>, read it, commit it.")
    return 1


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--scope", choices=["quick", "full"])
    ap.add_argument("--check", choices=["touch", "probes"])
    ap.add_argument("--write-probes", action="store_true")
    ap.add_argument("--build")
    ap.add_argument("--compare")
    ap.add_argument("--cfbench")
    ap.add_argument("--docs")
    ap.add_argument("--work")
    ap.add_argument("--sizes", default=None)
    ap.add_argument("--reps", type=int, default=0)
    ap.add_argument("--out")
    ap.add_argument("--flags", default="shipped")
    ap.add_argument("--only", default="")
    ap.add_argument("--mixed-only", action="store_true")
    ap.add_argument("--cold", action="store_true")
    ap.add_argument("--keep-docs", action="store_true")
    args = ap.parse_args(argv)

    compare = args.compare or (os.path.join(args.build, "cfb_compare") if args.build else None)
    if args.write_probes:
        with open(os.path.join(COMPARE, "probes.expected"), "w", encoding="utf-8", newline="\n") as f:
            f.writelines(ln + "\n" for ln in probes(compare))
        return 0
    if args.check == "probes":
        return check_probes(compare)
    if args.check == "touch":
        work = args.work or tempfile.mkdtemp(prefix="cfb-check-")
        return check_touch(compare, args.cfbench, work)
    if not args.scope or not compare or not args.cfbench:
        ap.print_usage()
        return 2

    adapters = adapters_of(compare)
    build = args.build or os.path.dirname(compare)
    docs = args.docs or tempfile.mkdtemp(prefix="cfb-docs-")
    if args.sizes is None:
        # quick reads 1 and 4 MiB, full 1, 16 and 256 (the module comment).
        args.sizes = "1,4" if args.scope == "quick" else "1,16,256"
    sizes = [int(s) for s in args.sizes.split(",")]
    need = sorted(set(([1, 4] if args.scope == "quick" else sizes) + ([1] if args.scope == "full" else [])))
    emit_docs(args.cfbench, docs, need)

    result = {
        "schema": 1,
        "kind": args.scope,
        "commit": git("rev-parse", "HEAD") or "0" * 40,
        "committed_at": utc(float(git("show", "-s", "--format=%ct", "HEAD") or 0) or None),
        "run_id": os.environ.get("GITHUB_RUN_ID", "0"),
        "machine": machine(),
        "compiler": compiler_info(build, args.flags),
        "libraries": libraries(adapters),
        "cells": run_cells(args, compare, docs, adapters, args.scope),
    }
    if args.scope == "full":
        result["sizes"] = size_rows(build)
        result["probes"] = probes(compare)
    if not args.keep_docs and not args.docs:
        shutil.rmtree(docs, ignore_errors=True)
    out = args.out or f"results-{args.scope}.json"
    with open(out, "w", encoding="utf-8", newline="\n") as f:
        json.dump(result, f, indent=1)
        f.write("\n")
    sys.stderr.write("run: wrote %s (%d cells)\n" % (out, len(result["cells"])))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
