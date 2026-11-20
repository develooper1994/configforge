#!/usr/bin/env python3
"""One command for the profile of cf_bench: time, cache, memory, vectorization.

    python scripts/work/profile.py [--host <ssh host>]... [--wsl] [--windows] <format>...

It archives HEAD, builds cf_bench with symbols and profiles each format on
every place it can reach, with every tool that is there and works:

  Linux (this machine, WSL, or an ssh host)
      perf (cpu-clock; hardware events when the kernel gives them),
      callgrind (instructions), cachegrind (cache misses), heaptrack else
      massif (peak memory, allocations), GCC's -fopt-info-vec-missed for the
      source files of the top functions.
  Windows
      MSVC RelWithDebInfo, Visual Studio's collector (VSDiagnostics) and
      xperf; no administrator rights are needed.

With no place named it uses all it can reach: this machine, and on Windows
the WSL distribution CF_DISTRO (default configforge-dev) if `wsl.exe -l -q`
lists it. --host names an ssh alias from ~/.ssh/config (BatchMode, 5 s
connect timeout); --wsl and --windows name those places. Whatever is
missing - a host, WSL, Visual Studio, perf, valgrind, heaptrack, xperf -
is one line "skipped: <what>, <why>" and the run goes on.

Output: one Markdown file in CF_WORK (default D:/cf-work when D: exists on
Windows, else $TMPDIR/cf-work), a table per tool and format with the top
six functions, and one summary line per format on stdout. Exit 0 when at
least one profile was taken, 1 when none could be.

CF_PROFILE_TIMEOUT (seconds, default 1800) limits each tool run.
"""

import argparse
import contextlib
import os
import re
import shlex
import shutil
import subprocess
import sys
import tarfile
import tempfile
import time
import zipfile
from datetime import datetime, timezone
from pathlib import Path

FLAGS = "-O2 -g -fno-omit-frame-pointer"
LIMIT = int(os.environ.get("CF_PROFILE_TIMEOUT", "1800"))
ROOT = Path(__file__).resolve().parents[2]
WINDOWS = sys.platform == "win32"
REASON = re.compile(r"\s+")


def skipped(what, why):
    print(f"skipped: {what}, {why}", flush=True)


def grab(pattern, text):
    m = re.search(pattern, text)
    return m.group(1) if m else "?"


def first_line(text):
    for line in text.splitlines():
        if line.strip():
            return REASON.sub(" ", line.strip())[:160]
    return "no output"


def work_dir():
    env = os.environ.get("CF_WORK")
    if env:
        path = Path(env)
    elif WINDOWS and Path("D:/").exists():
        path = Path("D:/cf-work")
    else:
        path = Path(os.environ.get("TMPDIR", tempfile.gettempdir())) / "cf-work"
    path.mkdir(parents=True, exist_ok=True)
    return path


def run(cmd, timeout=None, env=None, **kw):
    """A command's exit status and its output (both streams); never raises."""
    try:
        p = subprocess.run(
            cmd, capture_output=True, timeout=timeout, env=env, check=False, **kw
        )
    except (OSError, subprocess.TimeoutExpired) as e:
        return 255, f"{type(e).__name__}: {e}"
    return p.returncode, (p.stdout + b"\n" + p.stderr).decode("utf-8", "replace")


def archive(work, stamp):
    tar = work / f"profile-src-{stamp}.tar"
    cmd = [
        "git",
        "-c",
        "core.autocrlf=false",
        "-C",
        str(ROOT),
        "archive",
        "--format=tar",
        "-o",
        str(tar),
        "HEAD",
    ]
    rc, out = run(cmd)
    if rc != 0:
        raise SystemExit(f"git archive failed: {first_line(out)}")
    return tar


# ---- results ----------------------------------------------------------


class Report:
    """The Markdown file and the stdout summary."""

    def __init__(self, path):
        self.path = path
        self.lines = ["# Profile of cf_bench", ""]
        self.taken = 0

    def add(self, *lines):
        self.lines.extend(lines)

    def table(self, title, rows, columns=("Function", "Share")):
        """rows: (name, share...) tuples, the largest first column of share on top."""
        if not rows:
            return
        rows = sorted(rows, key=lambda r: -r[1])[:6]
        self.add("", f"**{title}**", "")
        self.add("| " + " | ".join(columns) + " |")
        self.add(
            "| " + " | ".join(["--------"] + ["----:"] * (len(columns) - 1)) + " |"
        )
        for r in rows:
            cells = [f"`{r[0]}`"] + [f"{v:.1f} %" for v in r[1:]]
            self.add("| " + " | ".join(cells) + " |")

    def save(self):
        self.path.write_text(
            "\n".join(self.lines) + "\n", encoding="utf-8", newline="\n"
        )


def brief(rows, n=3):
    rows = sorted(rows, key=lambda r: -r[1])[:n]
    return ", ".join(f"{r[0]} {r[1]:.0f}%" for r in rows)


# ---- parsers ----------------------------------------------------------


def parse_perf(text):
    rows = []
    for line in text.splitlines():
        m = re.match(r"^\s*([0-9.]+)%\s+\[.\]\s+(\S+)", line)
        if m:
            rows.append((m.group(2), float(m.group(1))))
    return rows


def parse_callgrind(text):
    """(file, function, share) from callgrind_annotate --inclusive=no."""
    rows = []
    listing = (
        False  # the table under the "file:function" header, not the annotated sources
    )
    for line in text.splitlines():
        if re.match(r"^\S.*\bfile:function\b", line):
            listing = True
            continue
        if listing and line.startswith("-----") and rows:
            break
        m = re.match(r"^\s*[0-9,]+\s+\(\s*([0-9.]+)%\)\s+(\S+)", line)
        if not listing or not m or m.group(2) == "PROGRAM":
            continue
        name = m.group(2)
        path, _, func = name.rpartition(":")
        func = re.sub(r"'\d+$", "", func or name)  # callgrind marks recursion depth
        rows.append((os.path.basename(path), func, float(m.group(1))))
    return rows


def parse_cachegrind(text):
    """(function, last-level miss share, L1 data miss share)."""
    col = {}
    fn = None
    ll, d1 = {}, {}
    for line in text.splitlines():
        if line.startswith("events:"):
            col = {n: i for i, n in enumerate(line.split()[1:])}
        elif line.startswith("fn="):
            fn = line[3:]
        elif fn is not None and line[:1].isdigit() and col:
            v = line.split()[1:]
            try:
                n = [int(x) for x in v]
                ll[fn] = (
                    ll.get(fn, 0) + n[col["ILmr"]] + n[col["DLmr"]] + n[col["DLmw"]]
                )
                d1[fn] = d1.get(fn, 0) + n[col["D1mr"]] + n[col["D1mw"]]
            except (KeyError, ValueError, IndexError):
                continue
    tl, td = sum(ll.values()), sum(d1.values())
    return [
        (k, 100.0 * ll[k] / tl if tl else 0.0, 100.0 * d1[k] / td if td else 0.0)
        for k in ll
    ]


def parse_xperf(text):
    """Self-time share, within cf_bench, per function from `xperf -a profile -detail`.

    Rows: `process (pid), weight, usage %, module!function`.
    """
    counts = {}
    total = 0
    for line in text.splitlines():
        m = re.match(
            r"^\s*cf_bench\.exe \(\s*\d+\),\s*(\d+),\s*[0-9.]+,\s*(\S+?)!(.*?)\s*$",
            line,
        )
        if m:
            name = m.group(3).strip('"')
            if m.group(2) != "cf_bench.exe":
                name = m.group(2).strip('"') + "!" + name
            counts[name] = counts.get(name, 0) + int(m.group(1))
            total += int(m.group(1))
    return [(k, 100.0 * v / total) for k, v in counts.items()] if total else []


# ---- Linux ------------------------------------------------------------


class Linux:
    """Commands on Linux: here, in a WSL distribution, or over ssh."""

    def __init__(self, label, kind, arg=None):
        self.label, self.kind, self.arg = label, kind, arg
        self.t = "$HOME/cf-prof/t"

    def sh(self, script, timeout=None):
        if self.kind == "local":
            cmd = ["sh", "-c", script]
        elif self.kind == "wsl":
            cmd = ["wsl.exe", "-d", self.arg, "-e", "sh", "-c", script]
        else:
            cmd = [
                "ssh",
                "-o",
                "BatchMode=yes",
                "-o",
                "ConnectTimeout=5",
                self.arg,
                "sh -c " + shlex.quote(script),
            ]
        return run(cmd, timeout=timeout)

    def reachable(self):
        rc, out = self.sh("echo cf-ok", timeout=30)
        if rc != 0 or "cf-ok" not in out:
            return first_line(out)
        return None

    def upload(self, tar, work):
        if self.kind == "local":
            return str(tar)
        if self.kind == "wsl":
            p = tar.resolve().as_posix()
            return re.sub(r"^([A-Za-z]):", lambda m: "/mnt/" + m.group(1).lower(), p)
        rc, out = run(
            [
                "scp",
                "-o",
                "BatchMode=yes",
                "-o",
                "ConnectTimeout=5",
                "-q",
                str(tar),
                f"{self.arg}:cf-prof-src.tar",
            ]
        )
        if rc != 0:
            raise RuntimeError("scp: " + first_line(out))
        return "$HOME/cf-prof-src.tar"

    def tool(self, script):
        """A tool's run under the limit: (ok, output or the one-line reason)."""
        wrapped = f"cd {self.t} && ( command -v timeout >/dev/null && T='timeout {LIMIT}' || T=; {script} )"
        rc, out = self.sh(wrapped, timeout=LIMIT + 120)
        return rc == 0, out

    def profile(self, formats, tar, report, work):
        rc, out = self.sh(
            "uname -srm; grep -m1 'model name' /proc/cpuinfo | cut -d: -f2", timeout=60
        )
        report.add(
            "",
            f"## {self.label}",
            "",
            "* machine: "
            + " / ".join(x.strip() for x in out.splitlines()[:2] if x.strip()),
        )
        src = self.upload(tar, work)
        gen = "-G Ninja" if self.sh("command -v ninja")[0] == 0 else ""
        setup = (
            f"rm -rf $HOME/cf-prof && mkdir -p {self.t} $HOME/cf-prof/src && tar -xf {src} -C $HOME/cf-prof/src && "
            f"cd $HOME/cf-prof && cmake -S src -B build {gen} -DCMAKE_BUILD_TYPE=RelWithDebInfo "
            f"'-DCMAKE_C_FLAGS_RELWITHDEBINFO={FLAGS}' -DCF_BUILD_BENCH=ON >t/cfg.log 2>&1 && "
            "cmake --build build --target cf_bench >t/build.log 2>&1 || { tail -n 12 t/cfg.log t/build.log; exit 1; }"
        )
        rc, out = self.sh(setup, timeout=LIMIT)
        if rc != 0:
            skipped(self.label, "the build failed: " + first_line(out))
            return
        rc, cc = self.sh("${CC:-cc} --version | head -n 1")
        cc = first_line(cc)
        report.add(
            f"* compiler: {cc}, `{FLAGS}`, RelWithDebInfo",
            "* benchmark: `cf_bench docs <format>` (1 and 16 MiB documents)",
        )
        bench = "$HOME/cf-prof/build/bench/cf_bench"

        have = {}
        probes = {
            "perf": ("perf record -e cpu-clock -F 4000 -g -o p.data true", "perf"),
            "valgrind": ("valgrind -q true", "valgrind"),
            "heaptrack": (
                "heaptrack -o hp-probe $(command -v ls) && ls hp-probe*",
                "heaptrack",
            ),
            "annotate": ("command -v callgrind_annotate", "callgrind_annotate"),
            "heaptrack_print": ("command -v heaptrack_print", "heaptrack_print"),
            "hw": (
                "perf stat -e cycles true 2>&1 | grep -qv 'not supported\\|<not'",
                "perf hardware events",
            ),
            "vec": (
                "${CC:-cc} -fopt-info-vec-missed -x c -c /dev/null -o /dev/null",
                "GCC -fopt-info",
            ),
        }
        for key, (cmd, what) in probes.items():
            ok, out = self.tool(cmd + " 2>&1")
            have[key] = ok
            if not ok:
                installed = self.sh(f"command -v {what.split()[0]} >/dev/null")[0] == 0
                skipped(
                    f"{self.label} {what}",
                    first_line(out) if installed else "not installed",
                )
        massif = (
            not (have["heaptrack"] and have["heaptrack_print"]) and have["valgrind"]
        )
        vec_log = None
        for f in formats:
            run_ = f"{bench} docs {f}"
            summary = []
            srcs = []
            report.add("", f"### {self.label}: {f}")
            if have["perf"]:
                ok, out = self.tool(
                    f"$T perf record -e cpu-clock -F 4000 -g -o p.data {run_} >/dev/null 2>log && perf report -i p.data --no-children --stdio --sort symbol -g none 2>/dev/null"
                )
                if ok:
                    rows = parse_perf(out)
                    report.table("perf, time (cpu-clock, 4000 Hz), self", rows)
                    summary.append(f"perf [{brief(rows)}]")
                    srcs = [r[0] for r in sorted(rows, key=lambda r: -r[1])[:3]]
                else:
                    skipped(f"{self.label} perf {f}", first_line(out))
                if have["hw"]:
                    ok, out = self.tool(
                        f"$T perf record -e cycles -F 4000 -g -o c.data {run_} >/dev/null 2>log && perf report -i c.data --no-children --stdio --sort symbol -g none 2>/dev/null"
                    )
                    if ok:
                        report.table("perf, cycles, self", parse_perf(out))
            if have["valgrind"] and have["annotate"]:
                ok, out = self.tool(
                    f"$T valgrind --tool=callgrind --callgrind-out-file=cg.out {run_} >log 2>&1 && callgrind_annotate --inclusive=no cg.out"
                )
                if ok:
                    rows = parse_callgrind(out)
                    report.table(
                        "callgrind, instructions, self", [(fn, s) for _, fn, s in rows]
                    )
                    summary.append(
                        "callgrind [" + brief([(fn, s) for _, fn, s in rows]) + "]"
                    )
                    files = []
                    for fl, _, _ in sorted(rows, key=lambda r: -r[2])[:3]:
                        if fl not in files:
                            files.append(fl)
                    srcs = files
                else:
                    skipped(f"{self.label} callgrind {f}", first_line(out))
            if have["valgrind"]:
                ok, out = self.tool(
                    f"$T valgrind --tool=cachegrind --cache-sim=yes --cachegrind-out-file=cache.out {run_} >log 2>&1 && cat cache.out"
                )
                if ok:
                    rows = parse_cachegrind(out)
                    report.table(
                        "cachegrind, cache misses (simulated), self",
                        [r for r in rows if r[1] or r[2]],
                        ("Function", "Last-level misses", "L1 data misses"),
                    )
                    summary.append(
                        "cache [" + brief([(r[0], r[1]) for r in rows], 2) + "]"
                    )
                else:
                    skipped(f"{self.label} cachegrind {f}", first_line(out))
            if have["heaptrack"] and have["heaptrack_print"]:
                ok, out = self.tool(
                    f"$T heaptrack -o hp {run_} >log 2>&1 && heaptrack_print -n 6 hp.*"
                )
                if ok:
                    peak = grab(r"peak heap memory consumption: *(\S+)", out)
                    calls = grab(r"calls to allocation functions: *(\d+)", out)
                    total = grab(r"total memory allocated: *(\S+)", out)
                    report.add(
                        "",
                        f"**heaptrack**: peak {peak}, {calls} allocations, {total} allocated in total",
                    )
                    summary.append(f"heaptrack [peak {peak}, {calls} allocations]")
                else:
                    skipped(f"{self.label} heaptrack {f}", first_line(out))
            elif massif:
                ok, out = self.tool(
                    f"$T valgrind --tool=massif --massif-out-file=ms.out {run_} >log 2>&1 && cat ms.out"
                )
                if ok:
                    peaks = [
                        int(x)
                        for x in re.findall(r"^mem_heap_B=(\d+)", out, re.MULTILINE)
                    ]
                    mib = max(peaks or [0]) // 1048576
                    report.add(
                        "",
                        f"**massif**: peak heap {max(peaks or [0])} bytes ({mib} MiB)",
                    )
                    summary.append(f"massif [peak {mib} MiB]")
                else:
                    skipped(f"{self.label} massif {f}", first_line(out))
            if have["vec"] and srcs:
                if vec_log is None:
                    cfg = (
                        f"cd $HOME/cf-prof && cmake -S src -B build-vec {gen} -DCMAKE_BUILD_TYPE=RelWithDebInfo "
                        f"'-DCMAKE_C_FLAGS_RELWITHDEBINFO={FLAGS} -fopt-info-vec-missed' >t/cfgv.log 2>&1 && "
                        "cmake --build build-vec --target configforge 2>&1"
                    )
                    rc, text = self.sh(cfg, timeout=LIMIT)
                    vec_log = text if rc == 0 else ""
                    if not vec_log:
                        skipped(
                            f"{self.label} vectorization notes",
                            "the extra build failed",
                        )
                if vec_log:
                    self.vectorization(report, summary, f, srcs, vec_log)
            print(f"[{self.label}] {f}: " + " | ".join(summary), flush=True)
            if summary:
                report.taken += 1
        self.sh("rm -rf $HOME/cf-prof $HOME/cf-prof-src.tar")

    def vectorization(self, report, summary, f, srcs, log):
        rows = []
        total = 0
        for s in srcs:
            if not s.endswith(".c"):
                # perf names functions; find the file that defines one
                rc, out = self.sh(
                    f"grep -rlE '(^|[ *]){re.escape(s)}\\(' $HOME/cf-prof/src/lib --include='*.c' | head -n 1"
                )
                s = (
                    os.path.basename(out.strip())
                    if rc == 0 and out.strip().endswith(".c")
                    else None
                )
            if not s:
                continue
            lines = {
                ln
                for ln in log.splitlines()
                if re.search(rf"/{re.escape(s)}:\d+:\d+: missed:", ln)
            }
            loops = sum("couldn't vectorize loop" in ln for ln in lines)
            why = {}
            for ln in lines:
                if "missed: not vectorized" in ln:
                    k = ln.split("missed: ", 1)[1]
                    why[k] = why.get(k, 0) + 1
            top = max(why.items(), key=lambda kv: kv[1]) if why else None
            rows.append(
                f"| `{s}` | {loops} | {top[0] + ' (' + str(top[1]) + ')' if top else '-'} |"
            )
            total += loops
        if rows:
            report.add(
                "",
                "**vectorization, loops GCC did not vectorize (`-fopt-info-vec-missed`), in the files of the top functions**",
                "",
            )
            report.add(
                "| File | Missed loops | Most common reason |",
                "| ---- | ----: | ------------------ |",
                *rows,
            )
            summary.append(f"vec [{total} loops missed]")


# ---- Windows ----------------------------------------------------------


class Win:
    label = "windows"

    def __init__(self, work, stamp):
        self.work, self.stamp = work, stamp

    def find(self):
        """The tools, or the first thing that is missing."""
        pf = os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")
        vswhere = Path(pf) / "Microsoft Visual Studio/Installer/vswhere.exe"
        if not vswhere.exists():
            return "Visual Studio is not installed (no vswhere)"
        rc, out = run(
            [str(vswhere), "-latest", "-products", "*", "-property", "installationPath"]
        )
        vs = Path(out.strip().splitlines()[0]) if rc == 0 and out.strip() else None
        if not vs or not (vs / "VC/Auxiliary/Build/vcvars64.bat").exists():
            return "Visual Studio with the C++ tools is not installed"
        self.vcvars = vs / "VC/Auxiliary/Build/vcvars64.bat"
        coll = vs / "Team Tools/DiagnosticsHub/Collector"
        self.diag = coll / "VSDiagnostics.exe"
        self.config = coll / "AgentConfigs/CpuUsageHigh.json"
        if not self.diag.exists() or not self.config.exists():
            return "Visual Studio's collector (VSDiagnostics) is missing"
        self.xperf = shutil.which("xperf")
        if not self.xperf:
            for base in (pf, os.environ.get("ProgramFiles", r"C:\Program Files")):
                c = Path(base) / "Windows Kits/10/Windows Performance Toolkit/xperf.exe"
                if c.exists():
                    self.xperf = str(c)
        if not self.xperf:
            return "xperf (Windows Performance Toolkit) is missing"
        return None

    def cmd(self, lines, timeout=LIMIT, env=None):
        bat = self.work / f"profile-{self.stamp}.cmd"
        body = "\r\n".join(lines)
        bat.write_bytes(
            f'@echo off\r\ncall "{self.vcvars}" >nul || exit /b 1\r\n{body}\r\n'.encode(
                "ascii", "replace"
            )
        )
        try:
            return run(["cmd", "/c", str(bat)], timeout=timeout, env=env)
        finally:
            bat.unlink(missing_ok=True)

    def profile(self, formats, tar, report):
        report.add("", "## windows", "")
        src = self.work / "prof-win-src"
        build = self.work / "prof-win-build"
        shutil.rmtree(src, ignore_errors=True)
        with tarfile.open(tar) as t:
            t.extractall(src, filter="data") if hasattr(
                tarfile, "data_filter"
            ) else t.extractall(src)
        gen = "-G Ninja"
        rc, out = self.cmd(
            [
                f'cmake -S "{src}" -B "{build}" {gen} -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCF_BUILD_BENCH=ON >"{build}-cfg.log" 2>&1 || (type "{build}-cfg.log" & exit /b 1)',
                f'cmake --build "{build}" --target cf_bench 2>&1',
            ]
        )
        if rc != 0:
            skipped("windows", "the MSVC build failed: " + first_line(out[-600:]))
            return
        exe = next(build.rglob("cf_bench.exe"), None)
        if not exe:
            skipped("windows", "no cf_bench.exe after the build")
            return
        report.add(
            "* compiler: MSVC, RelWithDebInfo",
            "* tools: VSDiagnostics (CpuUsageHigh), xperf",
        )
        for f in formats:
            report.add("", f"### windows: {f}")
            rows, why = self.one(exe, f)
            if rows:
                report.table("Visual Studio collector + xperf, time, self", rows)
                report.taken += 1
                print(f"[windows] {f}: xperf [{brief(rows)}]", flush=True)
            else:
                skipped(f"windows {f}", why)
        shutil.rmtree(src, ignore_errors=True)

    def one(self, exe, f):
        sid = str(os.getpid() % 200 + 20)  # the collector takes 0 to 255
        d = self.work / f"prof-win-{f}"
        shutil.rmtree(d, ignore_errors=True)
        d.mkdir(parents=True)
        session = d / f"{f}.diagsession"
        rc, out = run(
            [
                str(self.diag),
                "start",
                sid,
                f"/launch:{exe}",
                f"/launchArgs:docs {f}",
                f"/loadConfig:{self.config}",
            ],
            timeout=120,
            cwd=str(exe.parent),
        )
        if rc != 0 or "Running" not in out:
            return None, "VSDiagnostics start: " + first_line(out)
        deadline = time.time() + LIMIT
        while time.time() < deadline:
            _, tl = run(["tasklist", "/fi", "imagename eq cf_bench.exe", "/nh"])
            if "cf_bench.exe" not in tl:
                break
            time.sleep(1)
        rc, out = run([str(self.diag), "stop", sid, f"/output:{session}"], timeout=300)
        if rc != 0 or not session.exists():
            return None, "VSDiagnostics stop: " + first_line(out)
        with zipfile.ZipFile(session) as z:
            names = [n for n in z.namelist() if n.endswith(".etl")]
            etl = next(
                (n for n in names if n.endswith("sc.user_aux.etl")),
                names[0] if names else None,
            )
            if not etl:
                return None, "no trace in the diagsession"
            z.extract(etl, d)
        env = dict(os.environ, _NT_SYMBOL_PATH=str(exe.parent))
        prof = d / "prof.txt"
        rc, out = run(
            [
                self.xperf,
                "-i",
                str(d / etl),
                "-symbols",
                "-o",
                str(prof),
                "-a",
                "profile",
                "-detail",
            ],
            timeout=900,
            env=env,
        )
        if rc != 0 or not prof.exists():
            return None, "xperf: " + first_line(out)
        rows = parse_xperf(prof.read_text(encoding="utf-8", errors="replace"))
        return rows, "no samples in the trace"


# ---- main -------------------------------------------------------------


def main():
    ap = argparse.ArgumentParser(
        description="profile cf_bench: time, cache, memory, vectorization"
    )
    ap.add_argument("formats", nargs="+", metavar="format")
    ap.add_argument(
        "--host", action="append", default=[], help="an ssh host alias (repeatable)"
    )
    ap.add_argument("--wsl", action="store_true", help="the WSL distribution CF_DISTRO")
    ap.add_argument(
        "--windows",
        action="store_true",
        help="MSVC with Visual Studio's collector and xperf",
    )
    ap.add_argument(
        "--local", action="store_true", help="this machine's own Linux tools"
    )
    a = ap.parse_args()
    for f in a.formats:
        if not re.fullmatch(r"[a-z0-9]+", f):
            ap.error(f"bad format name: {f}")

    chosen = a.host or a.wsl or a.windows or a.local
    work = work_dir()
    stamp = datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S")
    report = Report(work / f"profile-{stamp}.md")
    places = []  # (what, a function that profiles)
    distro = os.environ.get("CF_DISTRO", "configforge-dev")

    if WINDOWS:
        if a.windows or not chosen:
            places.append("windows")
        if a.wsl or not chosen:
            rc, out = run(["wsl.exe", "-l", "-q"], timeout=30)
            names = out.replace("\x00", "").split()
            if rc == 0 and distro in names:
                places.append("wsl")
            else:
                skipped(
                    "wsl",
                    f"distribution {distro} is not listed by wsl.exe -l -q"
                    if rc == 0
                    else "wsl.exe: " + first_line(out),
                )
    elif a.local or not chosen:
        places.append("local")
    elif a.windows:
        skipped("windows", "this machine is not Windows")
    if a.wsl and not WINDOWS:
        skipped("wsl", "this machine is not Windows")
    for h in a.host:
        places.append("host:" + h)

    if not places:
        skipped("everything", "no place to profile on")
        return 1
    tar = archive(work, stamp)
    try:
        for place in places:
            try:
                if place == "windows":
                    w = Win(work, stamp)
                    why = w.find()
                    if why:
                        skipped("windows", why)
                        continue
                    w.profile(a.formats, tar, report)
                    continue
                if place == "wsl":
                    lx = Linux(f"wsl {distro}", "wsl", distro)
                elif place == "local":
                    lx = Linux("linux", "local")
                else:
                    lx = Linux("host " + place[5:], "ssh", place[5:])
                why = lx.reachable()
                if why:
                    skipped(lx.label, f"not reachable: {why}")
                    continue
                lx.profile(a.formats, tar, report, work)
            except (RuntimeError, OSError) as e:
                skipped(place, str(e))
    finally:
        with contextlib.suppress(OSError):
            tar.unlink()
    if report.taken:
        report.save()
        print(f"wrote {report.path}")
        return 0
    print("no profile could be taken")
    return 1


if __name__ == "__main__":
    sys.exit(main())
