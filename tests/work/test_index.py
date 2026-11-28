"""index.py writes docs/Plan.md's index from the cards and the Layout."""

import shutil
import subprocess
import sys
from pathlib import Path
from tempfile import TemporaryDirectory

HEADER = (
    "| Order | Card | Version | Size | Needs | Status |\n"
    "| ----: | ---- | ------- | ----- | ----- | ------ |\n"
)
A1, A2 = "[A-1](plan/A-first.md#a-1)", "[A-2](plan/A-first.md#a-2)"
B1, C1 = "[B-1](plan/B-second.md#b-1)", "[C-1](plan/B-second.md#c-1)"
A3 = "[A-3](plan/B-second.md#a-3)"
ROWS = [
    f"| 1 | {B1} Big thing | 2.0.0 | large | {A2}, K-9 | parked: not now |",
    f"| 2 | {A2} Second | 1.9.0 | small | {A1} | waits: {A1} |",
    f"| 3 | {A1} First | 1.9.0 | medium | nothing | open |",
    f"| 4 | {C1} Third | 1.9.0 | small | K-9 | open |",
    f"| 5 | {A3} Fourth | 1.9.0 | small | {A2}, {B1} | waits: {A2} |",
]
BEGIN = "<!-- index begin (scripts/work/index.py) -->\n"
END = "<!-- index end -->\n"
STALE = "is not what scripts/work/index.py writes: run it"


def git(root, *args):
    subprocess.run(
        ["git", "-c", "user.name=t", "-c", "user.email=t@t", *args],
        cwd=root,
        check=True,
        capture_output=True,
    )


def tree(fixture, directory, name):
    """A fresh git repository holding the fixture, K-9 landed on main."""
    root = Path(directory) / name
    shutil.copytree(fixture, root)
    git(root, "init", "-q")
    git(root, "checkout", "-q", "-b", "main")
    git(root, "commit", "-q", "--allow-empty", "-m", "K-9: a card that landed")
    return root


def run(script, root, *options):
    done = subprocess.run(
        [sys.executable, str(script), "--root", str(root), *options],
        capture_output=True,
        text=True,
        encoding="utf-8",
        check=False,
    )
    return done.returncode, done.stdout + done.stderr


def table(root):
    text = (root / "docs/Plan.md").read_text(encoding="utf-8")
    return text.split(BEGIN)[1].split(END)[0]


def edit(root, name, old, new):
    path = root / "docs/plan" / name
    text = path.read_text(encoding="utf-8")
    assert old in text, old
    path.write_text(text.replace(old, new, 1), encoding="utf-8", newline="\n")


def expect_error(script, fixture, directory, name, file, old, new, want):
    root = tree(fixture, directory, name)
    edit(root, file, old, new)
    rc, out = run(script, root, "--check")
    assert rc == 1 and want in out, (name, rc, out)


def main():
    script, fixture, build = map(Path, sys.argv[1:])
    with TemporaryDirectory(dir=build) as directory:
        root = tree(fixture, directory, "main")
        plan = (root / "docs/Plan.md").read_text(encoding="utf-8")

        # red: the stale index, and nothing written by --check
        rc, out = run(script, root, "--check")
        assert rc == 1 and STALE in out and "A-1 Stale" in out, (rc, out)
        assert (root / "docs/Plan.md").read_text(encoding="utf-8") == plan

        # write: Layout's order, "waits" computed, the rest of the page kept
        rc, out = run(script, root)
        assert rc == 0, out
        assert table(root) == HEADER + "\n".join(ROWS) + "\n", table(root)
        got = (root / "docs/Plan.md").read_text(encoding="utf-8")
        assert got.startswith("# Fixture plan\n\nText before the index.\n\n" + BEGIN)
        assert got.endswith(END + "\nText after the index.\n"), got
        assert b"\r" not in (root / "docs/Plan.md").read_bytes()

        # green; the cards are anchored where the index links to them
        rc, out = run(script, root, "--check")
        assert rc == 0, out
        text = (root / "docs/plan/B-second.md").read_text(encoding="utf-8")
        assert '\n<a id="a-3"></a>\n## A-3 - Fourth\n' in text, text

        # an anchor lost: --check names links.py's files, the write restores it
        (root / "docs/plan/B-second.md").write_text(
            text.replace('<a id="a-3"></a>\n', ""), encoding="utf-8", newline="\n"
        )
        rc, out = run(script, root, "--check")
        assert rc == 1 and "links.py would change docs/plan/B-second.md" in out, (rc, out)
        rc, out = run(script, root)
        assert rc == 0, out
        assert (root / "docs/plan/B-second.md").read_text(encoding="utf-8") == text

        # --done: the field, the row, and the card that waited only on it
        rc, out = run(script, root, "--done", "A-1", "abc1234")
        assert rc == 0, out
        text = (root / "docs/plan/A-first.md").read_text(encoding="utf-8")
        assert "* **status**: done (abc1234)\n* **decided**: text" in text, text
        assert text.count("* **status**:") == 2
        rows = table(root).splitlines()
        assert f"| 2 | {A2} Second | 1.9.0 | small | {A1} | open |" in rows, rows
        assert f"| 3 | {A1} First | 1.9.0 | medium | nothing | done (abc1234) |" in rows
        rc, out = run(script, root, "--check")
        assert rc == 0, out

        # --archive: the done card leaves its file for Done.md, once
        rc, out = run(script, root, "--archive")
        assert rc == 0 and "A-1" in out, (rc, out)
        text = (root / "docs/plan/A-first.md").read_text(encoding="utf-8")
        assert "## A-1 - " not in text and "## A-2 - " in text, text
        assert 'id="a-1"' not in text and '<a id="a-2"></a>\n## A-2' in text, text
        done = (root / "docs/plan/Done.md").read_text(encoding="utf-8")
        assert "| A-1 | First | abc1234 | A-first.md |" in done, done
        rows = table(root).splitlines()
        assert not any("A-1 First" in row for row in rows), rows
        assert f"| {A2} Second | 1.9.0 | small | A-1 | open |" in " ".join(rows).replace("| 2 ", "| ")
        rc, out = run(script, root, "--check")
        assert rc == 0, out
        before = {p.name: p.read_bytes() for p in (root / "docs").rglob("*.md")}
        rc, out = run(script, root, "--archive")
        assert rc == 0 and "archived 0" in out, (rc, out)
        assert before == {p.name: p.read_bytes() for p in (root / "docs").rglob("*.md")}

        # a card that cannot be found
        rc, out = run(script, root, "--done", "Q-1", "abc1234")
        assert rc == 1 and "Q-1" in out, (rc, out)

        # no history to ask (a staged copy, a shallow clone): a need without
        # a card is not judged
        bare = Path(directory) / "bare"
        shutil.copytree(fixture, bare)
        edit(bare, "B-second.md", "needs**: K-9\n", "needs**: K-9a\n")
        rc, out = run(script, bare)
        assert rc == 0, out
        rc, out = run(script, bare, "--check")
        assert rc == 0, out

        # the errors
        for case in [
            (
                "nostatus",
                "A-first.md",
                "* **status**: open\n* **decided**: text\n\n## A-2",
                "* **decided**: text\n\n## A-2",
                "A-1: no status field",
            ),
            (
                "noversion",
                "A-first.md",
                "* **version**: 1.9.0\n* **status**: open\n* **decided**: text\n\n## Notes",
                "* **status**: open\n* **decided**: text\n\n## Notes",
                "A-2: no version field",
            ),
            (
                "badstatus",
                "B-second.md",
                "parked: not now",
                "waiting",
                "B-1: status 'waiting' is none of",
            ),
            (
                "notinlayout",
                "B-second.md",
                "## C-1 - Third",
                "## C-9 - Third",
                "C-9: not in docs/plan/Layout.md",
            ),
            (
                "noneed",
                "B-second.md",
                "needs**: K-9\n",
                "needs**: K-9a\n",
                "C-1: need K-9a has no card and never landed",
            ),
            (
                "duplicate",
                "B-second.md",
                "## C-1 - Third",
                "## A-1 - Third",
                "A-1: duplicate card id",
            ),
        ]:
            expect_error(script, fixture, directory, *case)


if __name__ == "__main__":
    main()
