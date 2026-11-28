"""Exercise ready.py against fixture cards, one failing each check."""

import shutil
import subprocess
import sys
from pathlib import Path
from tempfile import TemporaryDirectory

EXPECTED = {
    "R-1": (0, []),
    "R-2": (1, ["R-2: fields: no 'docs' field"]),
    "R-3": (1, ["R-3: status: 'parked' is not open"]),
    "R-4": (
        1,
        [
            "R-4: decide: line 80: 'TBD' is not decided",
            "R-4: decide: line 81: 'to be rewritten' is not decided",
        ],
    ),
    "R-5": (
        1,
        [
            "R-5: settled: line 98: a question mark in decided",
            "R-5: settled: line 99: 'either' in decided",
        ],
    ),
    "R-6": (
        1,
        ["R-6: defaults: line 116: cf_two_options has no default on its bullet"],
    ),
    "R-7": (1, ["R-7: cites: line 133: src/code.c:30-60: the file has 40 lines"]),
    "R-8": (
        1,
        [
            (
                "R-8: cites: line 150: src/code.c:20-22: "
                "'widget_open' is not within 10 lines of it"
            )
        ],
    ),
    "R-9": (1, ["R-9: cites: line 167: src/none.c:1-2: no such file"]),
    "R-10": (1, ["R-10: needs: R-11 is not done"]),
    "R-12": (1, ["R-12: needs: R-13 is in 2.0.0, after 1.9.0"]),
    "R-14": (1, ["R-14: decide: line 242: 'TO DECIDE' is not decided"]),
}


def run(script, root, *args):
    done = subprocess.run(
        [sys.executable, str(script), "--root", str(root), *args],
        capture_output=True,
        text=True,
        encoding="utf-8",
        check=False,
    )
    return done.returncode, done.stdout.splitlines(), done.stderr


def main():
    script, fixture, build = map(Path, sys.argv[1:])
    with TemporaryDirectory(dir=build) as directory:
        root = Path(directory) / "tree"
        shutil.copytree(fixture, root)
        scripts = Path(directory) / "scripts"
        for name in ("check/ready.py", "work/brief.py", "work/index.py"):
            (scripts / name).parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(script, scripts / "check/ready.py")
        source = script.parents[1]
        for name in ("brief.py", "index.py", "links.py"):
            shutil.copyfile(source / "work" / name, scripts / "work" / name)
        ready = scripts / "check/ready.py"

        for card, (code, lines) in EXPECTED.items():
            got = run(ready, root, card)
            assert got[:2] == (code, lines), (card, got)
        code, lines, _ = run(ready, root, "R-99")
        assert code == 2, (code, lines)

        code, lines, _ = run(ready, root, "--all")
        assert code == 0, lines
        assert lines[-1] == "ready: 2", lines
        assert "R-1: ready" in lines and "R-11: ready" in lines, lines
        assert not any(line.startswith("R-3") for line in lines), lines
        assert "R-2: fields: no 'docs' field" in lines, lines


if __name__ == "__main__":
    main()
