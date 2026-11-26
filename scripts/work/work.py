#!/usr/bin/env python3
"""The loop's helper scripts as one program (docs/Plan.md, "The loop").

    python scripts/work/work.py verify <id> [full|quick]
    python scripts/work/work.py format
    python scripts/work/work.py ship <id>
    python scripts/work/work.py land <id> "<subject>" [--dry-run]
    python scripts/work/work.py win <worktree> <id> [ctest regex]

It does what verify.sh, format.sh, ship.sh, land.sh and win.cmd do, on any
machine: on Linux directly, on Windows with the Linux half run in the WSL
distribution CF_DISTRO (configforge-dev) through wsl.exe.

  CF_WORK     a directory with room for copies and Windows builds (default
              D:/cf-work when D: exists on Windows, else $TMPDIR/cf-work)
  CF_DISTRO   the WSL distribution with buildenv/setup.sh's tools
  STYLE=1     verify: a milestone's end - nothing formatted, clang-tidy first
  CF_EXTRA    verify: more verify.cmake EXTRA steps in a full run
  SKIP_SHIP=1 land: the branch's last commit already passed ship
  CF_CI_MINUTES (60)   ship: a CI run still going after this is cancelled
  CF_STEP_MINUTES (120) one build or test step of verify
  CF_LOCK_MINUTES (240) how long verify waits for the machine's one slot
  CF_VERIFY_MINUTES (720) the whole Linux half of verify, as seen from Windows

Every wait has a limit; every failure is one line and a non-zero exit.
"""

import argparse
import json
import os
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import time
from pathlib import Path

WINDOWS = sys.platform == "win32"
HERE = Path(__file__).resolve()
TOOLS = "/opt/cf-tools/bin"
TRAILER = "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
C_SUFFIXES = (".c", ".h", ".cpp", ".hpp")
BAD = ("failure", "cancelled", "timed_out")


class Fail(Exception):
    def __init__(self, msg, rc=1):
        Exception.__init__(self, msg)
        self.rc = rc


def say(*a):
    print(*a, flush=True)


def minutes(name, default):
    try:
        return float(os.environ.get(name) or default) * 60
    except ValueError:
        raise Fail(f"{name} is not a number")


# ---- shared helpers (profile.py uses them) --------------------------------


def work_dir():
    """CF_WORK, made: D:/cf-work on Windows when D: exists, else tmp/cf-work."""
    env = os.environ.get("CF_WORK")
    if env:
        path = Path(env)
    elif WINDOWS and Path("D:/").exists():
        path = Path("D:/cf-work")
    else:
        path = Path(os.environ.get("TMPDIR", tempfile.gettempdir())) / "cf-work"
    path.mkdir(parents=True, exist_ok=True)
    return path


def distro():
    return os.environ.get("CF_DISTRO") or "configforge-dev"


def to_mnt(path):
    """A Windows path as WSL sees it (C:\\a\\b -> /mnt/c/a/b); others as they are."""
    s = str(Path(path).resolve()) if WINDOWS else str(path)
    m = re.match(r"^([A-Za-z]):[\\/]*(.*)$", s)
    if not m:
        return s.replace("\\", "/")
    return "/mnt/" + m.group(1).lower() + "/" + m.group(2).replace("\\", "/")


def kill_tree(p):
    if WINDOWS:
        subprocess.run(
            ["taskkill", "/T", "/F", "/PID", str(p.pid)],
            capture_output=True,
            check=False,
        )
    else:
        try:
            os.killpg(p.pid, signal.SIGKILL)
        except OSError:
            p.kill()


def run(cmd, timeout, cwd=None, env=None, out=None, stdin=None):
    """A command's exit status, 124 after the timeout (its tree is killed).

    Output goes to `out` (a file, with stderr) or to ours."""
    kw = {} if WINDOWS else {"start_new_session": True}
    try:
        p = subprocess.Popen(
            cmd,
            cwd=cwd,
            env=env,
            stdin=stdin,
            stdout=out,
            stderr=None if out is None else subprocess.STDOUT,
            **kw,
        )
    except OSError as e:
        raise Fail(f"cannot run {cmd[0]}: {e}", 127)
    try:
        return p.wait(timeout)
    except subprocess.TimeoutExpired:
        kill_tree(p)
        p.wait()
        return 124


def capture(cmd, timeout=300, cwd=None, text=None):
    """Exit status and the output (both streams) of a short command."""
    try:
        p = subprocess.run(
            cmd,
            cwd=cwd,
            input=text,
            capture_output=True,
            timeout=timeout,
            check=False,
            encoding="utf-8",
            errors="replace",
        )
    except subprocess.TimeoutExpired:
        raise Fail(f"{cmd[0]} {cmd[1] if len(cmd) > 1 else ''} timed out", 124)
    except OSError as e:
        raise Fail(f"cannot run {cmd[0]}: {e}", 127)
    return p.returncode, p.stdout + p.stderr


def first_line(text):
    for line in text.splitlines():
        if line.strip():
            return line.strip()[:160]
    return "no output"


def git(*args, cwd=None, timeout=300, ok=False):
    """git's output, stripped; a failure is a Fail, or with ok=True its rc."""
    rc, out = capture(["git", "-c", "core.quotepath=off", *args], timeout, cwd)
    if ok:
        return rc
    if rc:
        raise Fail(f"git {args[0]} failed: {first_line(out)}", rc)
    return out.strip()


def linux_cmd(argv):
    """A command for Linux: here, or in the distribution on Windows."""
    return ["wsl.exe", "-d", distro(), "-e", *argv] if WINDOWS else list(argv)


def linux_env():
    env = dict(os.environ)
    env["MSYS_NO_PATHCONV"] = "1"
    return env


def self_cmd(*args):
    """work.py's own command line, as Linux sees it."""
    return linux_cmd(["python3", to_mnt(HERE) if WINDOWS else str(HERE), *args])


def tools_path():
    return TOOLS + os.pathsep + os.environ.get("PATH", "")


# ---- format ---------------------------------------------------------------


def changed(*patterns):
    a = git("diff", "--name-only", "--diff-filter=AMR", "origin/main", "--", *patterns)
    b = git("ls-files", "-o", "--exclude-standard", "--", *patterns)
    return sorted({x for x in (a + "\n" + b).splitlines() if x})


def format_files(files):
    """Format files (cwd's) with the pinned clang-format or cmake-format."""
    os.environ["PATH"] = tools_path()
    for tool, group in (
        ("clang-format", [f for f in files if f.endswith(C_SUFFIXES)]),
        ("cmake-format", [f for f in files if not f.endswith(C_SUFFIXES)]),
    ):
        if not group:
            continue
        if not shutil.which(tool):
            raise Fail(f"{tool} not found in {TOOLS} or PATH")
        for i in range(0, len(group), 40):
            rc = run([tool, "-i", *group[i : i + 40]], minutes("CF_STEP_MINUTES", 120))
            if rc:
                raise Fail(f"{tool} exit {rc}", rc)


def cmd_format(_a):
    c = changed("*.c", "*.h", "*.cpp", "*.hpp")
    m = changed("*.cmake", "*CMakeLists.txt")
    files = c + m
    root = Path.cwd()
    if WINDOWS:
        for i in range(0, len(files), 40):
            cmd = self_cmd("_format-in", to_mnt(root), *files[i : i + 40])
            rc = run(cmd, minutes("CF_STEP_MINUTES", 120), env=linux_env())
            if rc:
                raise Fail(f"format in {distro()} exit {rc}", rc)
    else:
        format_files(files)
    say(f"formatted: {len(files)} files")
    return 0


def cmd_format_in(a):
    os.chdir(a.root)
    format_files(a.files)
    return 0


# ---- verify ---------------------------------------------------------------


def cmd_verify(a):
    work = work_dir()
    tree = git("write-tree")
    tar = work / f"src-{a.id}.tar"
    git("-c", "core.autocrlf=false", "archive", "--format=tar", "-o", str(tar), tree)
    # The last release, so that verify's abi step has a tag to compare with.
    base = ""
    rc, out = capture(
        ["git", "describe", "--tags", "--abbrev=0", "--match", "v*", "HEAD"]
    )
    if rc == 0:
        base = out.strip()
        btar = work / f"src-{a.id}-base.tar"
        git(
            "-c",
            "core.autocrlf=false",
            "archive",
            "--format=tar",
            "-o",
            str(btar),
            base,
        )
    style = "1" if os.environ.get("STYLE") == "1" else "0"
    extra = os.environ.get("CF_EXTRA", "")
    if WINDOWS:
        args = ["_verify-in", a.id, a.mode, to_mnt(tar), "--style", style]
        if extra:
            args += ["--extra", extra]
        if base:
            args += ["--base", base]
        rc = run(self_cmd(*args), minutes("CF_VERIFY_MINUTES", 720), env=linux_env())
    else:
        rc = verify_in(a.id, a.mode, str(tar), style == "1", extra, base)
    if rc == 124:
        raise Fail(f"verify {a.id} timed out", 124)
    say(f"verify {a.id} {a.mode}: {'ok' if rc == 0 else 'FAILED'} (exit {rc})")
    return rc


def cmd_verify_in(a):
    return verify_in(a.id, a.mode, a.tar, a.style == "1", a.extra, a.base)


def lock(path):
    import fcntl

    f = open(path, "a+")  # noqa: SIM115 - held until the process ends
    limit = time.monotonic() + minutes("CF_LOCK_MINUTES", 240)
    waited = False
    while True:
        try:
            fcntl.flock(f, fcntl.LOCK_EX | fcntl.LOCK_NB)
            return f
        except OSError:
            if time.monotonic() > limit:
                raise Fail(
                    f"verify.lock still held after {minutes('CF_LOCK_MINUTES', 240) // 60:g} minutes"
                )
            if not waited:
                say("waiting for another verify run (verify.lock)")
                waited = True
            time.sleep(5)


def find_sources(*dirs):
    out = []
    for d in dirs:
        for suffix in C_SUFFIXES:
            out += [str(p) for p in Path(d).rglob("*" + suffix) if p.is_file()]
    return sorted(out)


def have_yaml_libs():
    """What the yaml job's libraries lack here, or an empty string."""
    missing = []
    for pc, header in (
        ("yaml-0.1", "yaml.h"),
        ("expat", "expat.h"),
        ("sqlite3", "sqlite3.h"),
    ):
        if shutil.which("pkg-config"):
            found = (
                subprocess.run(["pkg-config", "--exists", pc], check=False).returncode
                == 0
            )
        else:
            found = any(
                list(Path(d).glob(header)) or list(Path(d).glob("*/" + header))
                for d in ("/usr/include", "/usr/local/include")
            )
        if not found:
            missing.append(pc)
    return ", ".join(missing)


def verify_in(id_, mode, tar, style, extra, base):
    """The Linux half of verify: a copy of the staged tree, one run at a time."""
    home = Path(os.environ.get("CF_LINUX_WORK") or Path.home())
    src = home / f"src-{id_}"
    step = minutes("CF_STEP_MINUTES", 120)
    os.environ["PATH"] = tools_path()
    # Out of the caller's directory: a Windows worktree's .git names a path
    # Linux git cannot read.
    os.chdir("/")
    safe = subprocess.run(
        ["git", "config", "--global", "--get-all", "safe.directory"],
        capture_output=True,
        encoding="utf-8",
        check=False,
    ).stdout.split()
    if "*" not in safe:
        git("config", "--global", "--add", "safe.directory", "*")
    tarp = Path(tar)
    btar = tarp.with_name(tarp.stem + "-base.tar")
    held = lock(home / "verify.lock")  # noqa: F841 - held until the process ends
    # Unpacked inside the lock: a queued run never deletes a running one's files.
    for d in (src, Path(str(src) + ".base")):
        shutil.rmtree(d, ignore_errors=True)
    src.mkdir(parents=True)
    rc = run(["tar", "-xf", str(tarp), "-C", str(src)], step)
    if rc:
        raise Fail(f"tar of the staged tree exit {rc}", rc)
    os.chdir(src)
    if not style:
        files = find_sources(
            "src", "include", "lib", "tools", "tests", "bench", "examples"
        )
        cm = [
            str(p)
            for p in list(Path(".").rglob("CMakeLists.txt"))
            + list(Path(".").rglob("*.cmake"))
            if p.parts[0] != "build"
        ]
        format_files(files)
        if cm and shutil.which("cmake-format"):
            for i in range(0, len(cm), 40):
                run(
                    ["cmake-format", "-i", *cm[i : i + 40]],
                    step,
                    out=subprocess.DEVNULL,
                )
    git("init", "-q")
    # The last release as the copy's first commit, tagged, so verify's abi
    # step finds it; the tree under test is then what is staged.
    if base and btar.is_file():
        bdir = Path(str(src) + ".base")
        bdir.mkdir()
        rc = run(["tar", "-xf", str(btar), "-C", str(bdir)], step)
        if rc:
            raise Fail(f"tar of the release {base} exit {rc}", rc)
        git("--work-tree=" + str(bdir), "add", "-A")
        git(
            "-c",
            "user.name=verify",
            "-c",
            "user.email=verify@localhost",
            "commit",
            "-q",
            "-m",
            base,
        )
        git("tag", base)
        shutil.rmtree(bdir, ignore_errors=True)
    git("add", "-A")

    def logged(name, steps, fail_re, fail_msg):
        """Run steps into one log; on failure its error lines and a message."""
        log = home / f"{name}-{id_}.log"
        with open(log, "wb") as f:
            rc = 0
            for cmd in steps:
                rc = run(cmd, step, out=f)
                if rc:
                    break
        if rc:
            text = log.read_text(encoding="utf-8", errors="replace").splitlines()
            for line in [x for x in text if re.search(fail_re, x)][:20]:
                say(line)
            say(
                f"{fail_msg} FAILED"
                + (" (timed out)" if rc == 124 else f" (exit {rc})")
            )
        return rc

    if style:
        say("--- clang-tidy (CI's job)")
        tidy = str(home / f"tidy-{id_}")
        steps = [
            ["cmake", "-S", ".", "-B", tidy, "-DCF_CLANG_TIDY=ON"],
            ["cmake", "--build", tidy, "-j3"],
        ]
        if logged("tidy", steps, r"error:", "clang-tidy"):
            return 1
        say("clang-tidy ok")
    say("--- clang build and test (CI's clang job)")
    clang = str(home / f"clang-{id_}")
    steps = [
        [
            "cmake",
            "-S",
            ".",
            "-B",
            clang,
            "-G",
            "Ninja",
            "-DCMAKE_C_COMPILER=clang",
            "-DCMAKE_CXX_COMPILER=clang++",
        ],
        ["cmake", "--build", clang],
        ["ctest", "--test-dir", clang, "-j3", "-E", "^gate\\.format"],
    ]
    if logged("clang", steps, r"error:|\(Failed\)", "clang"):
        return 1
    say("clang ok")
    if mode == "full":
        say("--- yaml, xml and sqlite build and test (CI's yaml job)")
        lacking = have_yaml_libs()
        if lacking:
            say(f"yaml skipped: {lacking} not installed here")
        else:
            yb = str(home / f"yaml-{id_}")
            steps = [
                [
                    "cmake",
                    "-S",
                    ".",
                    "-B",
                    yb,
                    "-G",
                    "Ninja",
                    "-DCF_WITH_YAML=ON",
                    "-DCF_YAML_BACKEND=libyaml",
                    "-DCF_WITH_XML=ON",
                    "-DCF_WITH_SQLITE=ON",
                ],
                ["cmake", "--build", yb],
                ["ctest", "--test-dir", yb, "-j3", "-E", "^gate\\.format"],
            ]
            if logged("yaml", steps, r"error:|\(Failed\)", "yaml"):
                return 1
            say("yaml ok")
        # CI's iwyu job, here when include-what-you-use is installed.
        if shutil.which("include-what-you-use"):
            say("--- include-what-you-use (CI's iwyu job)")
            ib = str(home / f"iwyu-{id_}")
            iwyu = (
                "include-what-you-use;-Xiwyu;--error;-Xiwyu;--mapping_file="
                + str(Path.cwd() / ".iwyu.imp")
                + ";-Wno-unknown-warning-option"
            )
            steps = [
                [
                    "cmake",
                    "-S",
                    ".",
                    "-B",
                    ib,
                    "-DCMAKE_BUILD_TYPE=Debug",
                    "-DCMAKE_C_INCLUDE_WHAT_YOU_USE=" + iwyu,
                ],
                [
                    "cmake",
                    "--build",
                    ib,
                    "--target",
                    "configforge",
                    "configforge_serialize",
                    "--",
                    "-k",
                ],
            ]
            if logged("iwyu", steps, r"should add these lines|error:", "iwyu"):
                return 1
            say("iwyu ok")
        else:
            say("iwyu skipped: include-what-you-use not installed here")
    # CF_EXTRA names more verify EXTRA steps (a card's ACCEPT may ask).
    ex = "-DEXTRA="
    if mode == "full":
        ex = "-DEXTRA=compilers" + (";" + extra if extra else "")
    log = home / f"verify-{id_}.log"
    with open(log, "wb") as f:
        rc = run(
            [
                "cmake",
                "-DSOURCE=index",
                f"-DMODE={mode}",
                ex,
                "-DJOBS=3",
                f"-DWORK={home / ('cfv-' + id_)}",
                "-P",
                "scripts/verify.cmake",
            ],
            step * 2,
            out=f,
        )
    for line in log.read_text(encoding="utf-8", errors="replace").splitlines()[-25:]:
        say(line)
    if rc == 124:
        say("verify.cmake timed out")
    return rc


# ---- ship -----------------------------------------------------------------


def gh(*args, timeout=120):
    rc, out = capture(["gh", *args], timeout)
    if rc:
        raise Fail(f"gh {args[0]} {args[1]} failed: {first_line(out)}", rc)
    return out


def cmd_ship(a):
    id_ = a.id
    br = "work/" + id_
    if git("status", "--porcelain"):
        raise Fail("tree not clean")
    work = work_dir()
    log = work / f"ship-{id_}.log"
    with open(log, "wb") as f:
        rc = run(
            [sys.executable, str(HERE), "verify", id_],
            minutes("CF_VERIFY_MINUTES", 720) + 600,
            out=f,
        )
    text = log.read_text(encoding="utf-8", errors="replace").splitlines()
    for line in [x for x in text if re.search("clang-tidy|all green|FAILED|failed", x)][
        :8
    ]:
        say(line)
    if rc:
        say(f"local verify exit {rc}: not pushed")
        return rc
    rc, out = capture(["git", "push", "-q", "-u", "origin", br], 600)
    for line in out.splitlines():
        if not line.startswith("remote"):
            say(line)
    if rc:
        raise Fail(f"git push of {br} failed", rc)
    sha = git("rev-parse", "HEAD")
    run_id = ""
    for _ in range(12):
        runs = json.loads(
            gh(
                "run",
                "list",
                "--branch",
                br,
                "--workflow",
                "ci",
                "--limit",
                "1",
                "--json",
                "databaseId,headSha",
            )
        )
        ids = [r["databaseId"] for r in runs if r["headSha"] == sha]
        if ids:
            run_id = str(ids[0])
            break
        time.sleep(10)
    if not run_id:
        raise Fail(f"no CI run for {sha[:10]} on {br} after 2 minutes")
    # A job that hangs would hold the chain for GitHub's six hours: after
    # CF_CI_MINUTES (60) the run is cancelled, and a cancelled job fails below.
    limit = minutes("CF_CI_MINUTES", 60)
    rc = run(["gh", "run", "watch", run_id], limit, out=subprocess.DEVNULL)
    if rc == 124:
        capture(["gh", "run", "cancel", run_id], 120)
        run(["gh", "run", "watch", run_id], 600, out=subprocess.DEVNULL)
    jobs = json.loads(gh("run", "view", run_id, "--json", "jobs"))["jobs"]
    # Code first: a failed style step (clang-format, clang-tidy, format-patch)
    # does not fail a card; any other failed step does (docs/Plan.md).
    bad = []
    for j in jobs:
        if j["name"] == "format-patch":
            continue
        for s in j.get("steps") or []:
            n = s["name"]
            if (
                s.get("conclusion") in BAD
                and n not in ("clang-tidy", "clang-format")
                and not re.search("format", n, re.IGNORECASE)
            ):
                bad.append(f"{j['name']}: {n}")
    style = ", ".join(j["name"] for j in jobs if j.get("conclusion") in BAD)
    say(f"{id_} CI run {run_id}; failed jobs: {style or 'none'}")
    if not bad:
        say("code CI green")
        return 0
    say("code failures:")
    say("\n".join(bad))
    return 1


# ---- land -----------------------------------------------------------------


def worktree_of(branch):
    wt = ""
    for line in git("worktree", "list", "--porcelain").splitlines():
        if line.startswith("worktree "):
            wt = line[9:]
        elif line == "branch refs/heads/" + branch:
            return wt
    return ""


def cmd_land(a):
    id_, subj, dry = a.id, a.subject, a.dry_run
    br = "work/" + id_
    skip = os.environ.get("SKIP_SHIP") == "1"

    def do(desc, fn=None):
        if dry:
            say("would: " + desc)
            return None
        return fn() if fn else None

    root = Path(git("rev-parse", "--show-toplevel")) if not dry else Path.cwd()
    os.chdir(root)
    wt = ""
    if dry:
        wt = f"<worktree of {br}>"
        say(f"would: find the worktree on {br} (none: stop)")
    else:
        wt = worktree_of(br)
        if not wt:
            raise Fail(f"no worktree on {br}")
        if git("rev-parse", "--abbrev-ref", "HEAD") != "main":
            raise Fail("land runs from the main checkout, on main")
    do("git fetch -q", lambda: git("fetch", "-q"))

    def main_synced():
        if git("rev-parse", "main") != git("rev-parse", "origin/main"):
            raise Fail("main differs from origin/main")

    do("stop unless main == origin/main", main_synced)

    def merge_main():
        nonlocal skip
        if git("merge-base", "--is-ancestor", "origin/main", br, ok=True):
            git("-C", wt, "merge", "-q", "--no-edit", "origin/main")
            skip = False

    do(
        f"if origin/main is not in {br}: merge it into the branch in the worktree, and ship again",
        merge_main,
    )
    before = [None]

    def fmt():
        before[0] = git("rev-parse", br)
        rc = run(
            [sys.executable, str(HERE), "format"], 1800, cwd=wt, out=subprocess.DEVNULL
        )
        if rc:
            raise Fail(f"format exit {rc}", rc)
        # docs/API.md links to source lines: made again, as gate.apiref wants.
        rc = run(
            ["cmake", "-P", "scripts/generate/apiref.cmake"],
            600,
            cwd=wt,
            out=subprocess.DEVNULL,
        )
        if rc:
            raise Fail(f"apiref exit {rc}", rc)
        git("add", "-A", cwd=wt)
        if git("diff", "--cached", "--quiet", cwd=wt, ok=True):
            git(
                "commit",
                "-q",
                "-m",
                f"{id_} as clang-format and cmake-format leave it\n\n{TRAILER}",
                cwd=wt,
            )

    do(
        f"format the worktree; commit '{id_} as clang-format and cmake-format leave it' when anything changed",
        fmt,
    )
    if not dry and git("rev-parse", br) != before[0]:
        skip = False

    def ship():
        if skip:
            return
        wait = (minutes("CF_VERIFY_MINUTES", 720) + minutes("CF_CI_MINUTES", 60)) + 3600
        rc = run([sys.executable, str(HERE), "ship", id_], wait, cwd=wt)
        if rc:
            raise Fail(f"ship of {id_} exit {rc}", rc)

    do(
        "ship the branch in the worktree (verify, push, CI) unless SKIP_SHIP=1 and nothing changed",
        ship,
    )
    body = [""]

    def squash():
        body[0] = git("log", "--reverse", "--no-merges", "main.." + br, "--format=- %s")
        git("merge", "--squash", "-q", br)
        msg = f"{id_}: {subj}\n\n{body[0]}\n\n{TRAILER}\n"
        rc, out = capture(["git", "commit", "-q", "-F", "-"], 300, text=msg)
        if rc:
            raise Fail(f"git commit failed: {first_line(out)}", rc)
        if git("rev-parse", "HEAD^{tree}") != git("rev-parse", br + "^{tree}"):
            git("reset", "-q", "--hard", "origin/main")
            raise Fail("tree differs: not pushed")

    do(
        f"squash {br} onto main: '{id_}: {subj}', the branch's subjects in the body; stop if the tree differs",
        squash,
    )

    def index():
        h = git("rev-parse", "--short", "HEAD")
        rc, out = capture(
            [sys.executable, str(HERE.with_name("index.py")), "--done", id_, h]
        )
        if rc:
            raise Fail(f"index.py --done {id_} {h} failed: {first_line(out)}", rc)
        git("add", "docs/Plan.md", "docs/plan", "CLAUDE.md")
        git("commit", "-q", "-m", f"Plan: {id_} done ({h})\n\n{TRAILER}")
        rc, out = capture(["git", "push", "-q", "origin", "main"], 600)
        if rc:
            raise Fail(f"git push of main failed: {first_line(out)}", rc)

    do(
        f"index.py --done {id_} (hash): the card's status and docs/Plan.md's index; commit; push main",
        index,
    )

    def cleanup():
        if git("worktree", "remove", "-f", "-f", wt, ok=True):
            git("worktree", "prune")
        names = git("branch", "--format=%(refname:short)").splitlines()
        stale = [n for n in names if n.startswith("worktree-agent-")]
        capture(["git", "branch", "-D", br, *stale], 300)
        capture(["git", "push", "-q", "origin", "--delete", br], 300)
        say(git("log", "--oneline", "-2"))

    do(
        f"remove the worktree, the branch {br}, the worktree-agent-* branches and {br} on origin",
        cleanup,
    )
    return 0


# ---- win ------------------------------------------------------------------


def vs_environment():
    """The environment vcvars64.bat makes, captured once."""
    pf = os.environ.get("ProgramFiles(x86)") or r"C:\Program Files (x86)"
    vswhere = Path(pf) / "Microsoft Visual Studio" / "Installer" / "vswhere.exe"
    if not vswhere.is_file():
        raise Fail(f"vswhere not found at {vswhere}")
    rc, out = capture(
        [str(vswhere), "-latest", "-products", "*", "-property", "installationPath"], 60
    )
    vs = out.strip().splitlines()[0] if rc == 0 and out.strip() else ""
    if not vs:
        raise Fail("vswhere found no Visual Studio")
    vcvars = Path(vs) / "VC" / "Auxiliary" / "Build" / "vcvars64.bat"
    if not vcvars.is_file():
        raise Fail(f"{vcvars} not found")
    try:
        p = subprocess.run(
            f'cmd /c ""{vcvars}" >nul && set"',
            capture_output=True,
            timeout=120,
            check=False,
            encoding="utf-8",
            errors="replace",
        )
    except subprocess.TimeoutExpired:
        raise Fail("vcvars64 timed out", 124)
    if p.returncode:
        raise Fail(f"vcvars64 exit {p.returncode}", p.returncode)
    env = {}
    for line in p.stdout.splitlines():
        k, eq, v = line.partition("=")
        if eq and k:
            env[k] = v
    return env


def cmd_win(a):
    if not WINDOWS:
        raise Fail("win is for Windows: MSVC")
    env = vs_environment()
    work = work_dir()
    src = str(Path(a.worktree).resolve())
    b = work / f"win-{a.id}"
    step = minutes("CF_STEP_MINUTES", 120)

    def stage(cmd, log=None):
        if log is None:
            return run(cmd, step, env=env)
        with open(log, "wb") as f:
            rc = run(cmd, step, env=env, out=f)
        if rc:
            say(Path(log).read_text(encoding="utf-8", errors="replace"))
        return rc

    cfg = work / f"win-{a.id}-cfg.log"
    if not b.exists() and stage(
        ["cmake", "-S", src, "-B", str(b), "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release"],
        cfg,
    ):
        raise Fail(f"cmake configure of {src} failed (log above)")
    if stage(["cmake", "-S", src, "-B", str(b)], work / f"win-{a.id}-recfg.log"):
        raise Fail("cmake reconfigure failed (log above)")
    rc = stage(["cmake", "--build", str(b)])
    if rc:
        raise Fail(f"MSVC build failed (exit {rc})", rc)
    # Formatting is judged on Linux with the pinned clang-format.
    ctest = ["ctest", "--test-dir", str(b), "-j8"]
    if a.regex:
        ctest += ["-R", a.regex]
    rc = stage([*ctest, "-E", "^gate\\.format", "--output-on-failure"])
    if rc:
        say(f"win {a.id}: tests FAILED (exit {rc})")
    return rc


# ---- main -----------------------------------------------------------------


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("verify")
    p.add_argument("id")
    p.add_argument("mode", nargs="?", default="full", choices=["full", "quick"])
    p.set_defaults(fn=cmd_verify)
    p = sub.add_parser("_verify-in")
    p.add_argument("id")
    p.add_argument("mode")
    p.add_argument("tar")
    p.add_argument("--style", default="0")
    p.add_argument("--extra", default="")
    p.add_argument("--base", default="")
    p.set_defaults(fn=cmd_verify_in)
    sub.add_parser("format").set_defaults(fn=cmd_format)
    p = sub.add_parser("_format-in")
    p.add_argument("root")
    p.add_argument("files", nargs="*")
    p.set_defaults(fn=cmd_format_in)
    p = sub.add_parser("ship")
    p.add_argument("id")
    p.set_defaults(fn=cmd_ship)
    p = sub.add_parser("land")
    p.add_argument("id")
    p.add_argument("subject")
    p.add_argument("--dry-run", action="store_true")
    p.set_defaults(fn=cmd_land)
    p = sub.add_parser("win")
    p.add_argument("worktree")
    p.add_argument("id")
    p.add_argument("regex", nargs="?", default="")
    p.set_defaults(fn=cmd_win)
    a = ap.parse_args()
    try:
        return a.fn(a)
    except Fail as e:
        print(f"work {a.cmd}: {e}", file=sys.stderr, flush=True)
        return e.rc or 1
    except KeyboardInterrupt:
        print(f"work {a.cmd}: interrupted", file=sys.stderr, flush=True)
        return 130


if __name__ == "__main__":
    sys.exit(main())
