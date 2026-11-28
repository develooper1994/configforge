"""Exercise scripts/check/guarantees.py on small trees: the Thread:, Signal:
and Alloc: lines beside a guarantee, their table columns, and the call-graph
check behind Signal: safe and Alloc: none (K-9h).

The call data is a fixture in the format of GCC's -fcallgraph-info, so no
compiler is needed. Usage: test_guarantees.py <guarantees.py> <build dir>
"""

import subprocess
import sys
from pathlib import Path
from tempfile import TemporaryDirectory

HEADER = "lib/demo/include/configforge/demo.h"
CI_DIR = "ci"

DOC = """# Failure guarantees

<!-- process:begin -->

| Across | What holds | Test |
| ------ | ---------- | ---- |
| `a` | x | `unit.test_p1` |
| `b` | x | `unit.test_p2` |
| `c` | x | `unit.test_p3` |
| `d` | x | `unit.test_p4` |

<!-- process:end -->

<!-- guarantees:begin -->

<!-- guarantees:end -->
"""

WORDS = {
    "Thread": "safe, distinct objects or unsafe",
    "Signal": "safe or unsafe",
    "Alloc": "none or may",
}


def header(decls):
    """A header with one declaration under each comment; returns its text
    and the line of each declaration."""
    out = ["#define CF_API", ""]
    at = {}
    for name, comment in decls:
        out.append("/* " + comment[0])
        out += ["   " + c for c in comment[1:]]
        out[-1] += " */"
        out.append(f"CF_API int {name}(int x);")
        at[name] = len(out)
        out.append("")
    return "\n".join(out), at


def vcg(unit, calls):
    """GCC's call data for one unit: calls maps a function to its callees;
    a callee that is not a key is an external node, '?' an indirect call."""
    out = [f'graph: {{ title: "{unit}"']
    for name in calls:
        out.append(f'node: {{ title: "{name}" label: "{name}\\n{unit}:1:1" }}')
    seen = set(calls)
    for name in calls:
        for callee in calls[name]:
            target = "__indirect_call" if callee == "?" else callee
            if target not in seen:
                seen.add(target)
                label = "Indirect Call Placeholder" if callee == "?" else target
                out.append(f'node: {{ title: "{target}" label: "{label}" shape : ellipse }}')
            out.append(
                f'edge: {{ sourcename: "{name}" targetname: "{target}" label: "{unit}:2:3" }}'
            )
    out.append("}")
    return "\n".join(out) + "\n"


def put(root, rel, text):
    path = root / rel
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8", newline="\n")


def make_tree(root, decls, calls, notsignal="", with_ci=True):
    text, at = header(decls)
    put(root, "cmake/configforge.symbols", "\n".join(n for n, _ in decls) + "\n")
    put(root, "cmake/configforge.experimental.symbols", "# none\n")
    put(root, HEADER, text)
    put(root, "docs/Guarantees.md", DOC)
    put(root, "scripts/check/notsignal.txt", notsignal)
    put(root, "tests/CMakeLists.txt", "test_p1 test_p2 test_p3 test_p4\n")
    for n in "1234":
        put(root, f"tests/test_p{n}.c", "")
    if with_ci:
        put(root, CI_DIR + "/demo.c.ci", vcg("demo.c", calls))
    return at


def run(script, root, *args):
    done = subprocess.run(
        [sys.executable, str(script), *args, str(root)],
        capture_output=True,
        text=True,
        encoding="utf-8",
        check=False,
    )
    return done.returncode, done.stdout.splitlines()


def case(name, script, base, decls, calls, faults=(), notes=(), notsignal="", with_ci=True):
    """Check the tree: `faults` is every fault line, in order, as
    (name, message); `notes` are texts some output line must hold."""
    root = base / name
    at = make_tree(root, decls, calls, notsignal, with_ci)
    code, lines = run(script, root, "--write")
    assert code == 0, (name, lines)
    code, lines = run(script, root, "--callgraph", str(root / CI_DIR))
    got = [x for x in lines if x.startswith(("lib/", "scripts/"))]
    want = [f"{HEADER}:{at[n]}: {n}: {msg}" for n, msg in faults]
    assert got == want, (name, got, want, lines)
    assert (code == 0) == (not want), (name, code, lines)
    for note in notes:
        assert any(note in x for x in lines), (name, note, lines)
    return root


def main():
    script, build = map(Path, sys.argv[1:])
    with TemporaryDirectory(dir=build) as directory:
        base = Path(directory)

        # 1. A word nobody knows is refused, and so is a line with no full stop.
        for tag, word in (("Thread", "brisk"), ("Signal", "maybe"), ("Alloc", "lots")):
            case(
                "word-" + tag,
                script,
                base,
                [("cf_one", ["Guarantee: nothrow.", f"{tag}: {word}."])],
                {"cf_one": []},
                [("cf_one", f"{tag}: '{word}' is not {WORDS[tag]}")],
            )
        case(
            "nostop",
            script,
            base,
            [("cf_one", ["Guarantee: nothrow.", "Thread: safe"])],
            {"cf_one": []},
            [("cf_one", f"Thread: '' is not {WORDS['Thread']}")],
        )

        # 2. An allocating function marked Alloc: none, refused with the path.
        case(
            "alloc",
            script,
            base,
            [
                ("cf_one", ["Guarantee: strong.", "Alloc: none."]),
                ("cf_two", ["Guarantee: strong."]),
            ],
            {"cf_one": ["helper"], "helper": ["malloc"], "cf_two": ["malloc"]},
            [("cf_one", "Alloc: none, but cf_one -> helper -> malloc")],
        )

        # 3. Signal: safe is refused for stdio, a lock, a listed name, an
        #    indirect call and an allocator reached through a callee; a callee
        #    that only copies is fine.
        case(
            "signal",
            script,
            base,
            [(n, ["Guarantee: nothrow.", "Signal: safe."]) for n in ["cf_a", "cf_b", "cf_c", "cf_d", "cf_e", "cf_f"]],
            {
                "cf_a": ["fopen"],
                "cf_b": ["pthread_mutex_lock"],
                "cf_c": ["zap"],
                "cf_d": ["?"],
                "cf_e": ["step"],
                "step": ["free"],
                "cf_f": ["step_ok"],
                "step_ok": ["memcpy"],
            },
            [
                ("cf_a", "Signal: safe, but cf_a -> fopen"),
                ("cf_b", "Signal: safe, but cf_b -> pthread_mutex_lock"),
                ("cf_c", "Signal: safe, but cf_c -> zap"),
                ("cf_d", "Signal: safe, but cf_d -> an indirect call"),
                ("cf_e", "Signal: safe, but cf_e -> step -> free"),
            ],
            notsignal="# comment\nzap|a name the project lists\n",
        )

        # 4. A clean tree passes, its table has the three columns, and a
        #    Thread: unsafe is printed.
        root = case(
            "clean",
            script,
            base,
            [
                ("cf_one", ["Guarantee: nothrow.", "Thread: safe.", "Signal: safe.", "Alloc: none."]),
                ("cf_two", ["Guarantee: strong.", "Thread: unsafe."]),
                ("cf_three", ["Guarantee: basic."]),
            ],
            {"cf_one": ["helper"], "helper": ["memcpy"], "cf_two": ["malloc"], "cf_three": ["malloc"]},
            notes=["note: ", "cf_two: Thread: unsafe"],
        )
        text = (root / "docs/Guarantees.md").read_text(encoding="utf-8")
        for row in (
            "| Function | Header | Guarantee | Thread | Signal | Alloc |",
            "| `cf_one` | `demo.h` | nothrow | safe | safe | none |",
            "| `cf_two` | `demo.h` | strong | unsafe | unsafe | may |",
            "| `cf_three` | `demo.h` | basic | distinct objects | unsafe | may |",
        ):
            assert row in text, (row, text)
        assert "thread safe 1, signal safe 1, alloc none 1" in text, text

        # 5. No call data: the check says it is skipped, and passes.
        case(
            "nodata",
            script,
            base,
            [("cf_one", ["Guarantee: strong.", "Alloc: none."])],
            {"cf_one": ["malloc"]},
            notes=["skipped"],
            with_ci=False,
        )

        # 6. A claim on a name the call data does not hold is said, not refused.
        case(
            "absent",
            script,
            base,
            [("cf_one", ["Guarantee: strong.", "Alloc: none."])],
            {"cf_other": []},
            notes=["cf_one: not in the call data"],
        )

        # 7. A static helper of the same name in two files is told apart.
        root = case(
            "twins",
            script,
            base,
            [("cf_one", ["Guarantee: strong.", "Alloc: none."])],
            {"cf_one": ["pick"], "pick": ["memcpy"]},
        )
        put(root, CI_DIR + "/other.c.ci", vcg("other.c", {"pick": ["malloc"]}))
        code, lines = run(script, root, "--callgraph", str(root / CI_DIR))
        assert code == 0, lines


if __name__ == "__main__":
    main()
