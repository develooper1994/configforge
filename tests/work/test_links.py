"""links.py anchors the cards and links the Roadmap's card ids to them."""

import subprocess
import sys
from pathlib import Path
from tempfile import TemporaryDirectory

CARDS = """# A - fixture cards

## D-1 - A decision, not a card

* **size**: nothing

## A-1 - First

* **id**: A-1
* **status**: open

```
## A-9 - Not a card, inside a fence
```

## K-9k-m - Second, with a long id

* **id**: K-9k-m
* **status**: open

## Notes

Not a card.
"""
ROADMAP = """# Roadmap

## A-1 in a heading stays

A-1 is linked; so is K-9k-m, but `A-1` in a code span is not, nor
[A-1](plan/A-first.md#a-1) again, nor D-1 (a decision), nor Q-7 (no card),
nor UTF-8, nor SHA-256, nor a path plan/A-1-x.md.

```
A-1 in a fence stays
```

| a | b |
| - | - |
| A-1 | see [the card](plan/A-first.md#old) and [A-1](plan/Old.md#a-1) |
"""
LINKED = """# Roadmap

## A-1 in a heading stays

[A-1](plan/A-first.md#a-1) is linked; so is [K-9k-m](plan/A-first.md#k-9k-m), but `A-1` in a code span is not, nor
[A-1](plan/A-first.md#a-1) again, nor D-1 (a decision), nor Q-7 (no card),
nor UTF-8, nor SHA-256, nor a path plan/A-1-x.md.

```
A-1 in a fence stays
```

| a | b |
| - | - |
| [A-1](plan/A-first.md#a-1) | see [the card](plan/A-first.md#old) and [A-1](plan/A-first.md#a-1) |
"""


def run(script, root, *options):
    done = subprocess.run(
        [sys.executable, str(script), "--root", str(root), *options],
        capture_output=True,
        text=True,
        encoding="utf-8",
        check=False,
    )
    return done.returncode, done.stdout + done.stderr


def read(root, name):
    return (root / name).read_text(encoding="utf-8")


def main():
    script = Path(sys.argv[1])
    with TemporaryDirectory(dir=sys.argv[2]) as directory:
        root = Path(directory)
        (root / "docs/plan").mkdir(parents=True)
        (root / "docs/plan/A-first.md").write_text(CARDS, encoding="utf-8", newline="\n")
        (root / "docs/Roadmap.md").write_text(ROADMAP, encoding="utf-8", newline="\n")

        # red: nothing is anchored or linked yet, and --check writes nothing
        rc, out = run(script, root, "--check")
        assert rc == 1 and "A-first.md" in out and "Roadmap.md" in out, (rc, out)
        assert read(root, "docs/Roadmap.md") == ROADMAP

        # write: an anchor right before each card's heading, nowhere else
        rc, out = run(script, root)
        assert rc == 0, out
        cards = read(root, "docs/plan/A-first.md")
        assert '\n<a id="a-1"></a>\n## A-1 - First\n' in cards, cards
        assert '\n<a id="k-9k-m"></a>\n## K-9k-m - Second' in cards, cards
        assert cards.count("<a id=") == 2, cards
        assert read(root, "docs/Roadmap.md") == LINKED, read(root, "docs/Roadmap.md")
        assert b"\r" not in (root / "docs/Roadmap.md").read_bytes()

        # idempotent: a second run, and --check, change nothing
        before = (cards, read(root, "docs/Roadmap.md"))
        rc, out = run(script, root)
        assert rc == 0, out
        assert before == (read(root, "docs/plan/A-first.md"), read(root, "docs/Roadmap.md"))
        rc, out = run(script, root, "--check")
        assert rc == 0, out

        # a card renamed: its old anchor goes, the new one comes, the link follows
        text = read(root, "docs/plan/A-first.md").replace("A-1 - First", "A-2 - First")
        (root / "docs/plan/A-first.md").write_text(text, encoding="utf-8", newline="\n")
        rc, out = run(script, root, "--check")
        assert rc == 1 and "A-first.md" in out, (rc, out)
        rc, out = run(script, root)
        assert rc == 0, out
        cards = read(root, "docs/plan/A-first.md")
        assert '<a id="a-2"></a>\n## A-2 - First' in cards and 'id="a-1"' not in cards
        roadmap = read(root, "docs/Roadmap.md")
        # the card A-1 is gone: its links turn back into plain ids, and the
        # bare A-1 stays plain
        assert "[A-1](" not in roadmap and "A-1 is linked" in roadmap, roadmap
        assert "[K-9k-m](plan/A-first.md#k-9k-m)" in roadmap

        # a stale link (the card moved to another file) is rewritten
        (root / "docs/plan/B-other.md").write_text(
            '<a id="k-9k-m"></a>\n## K-9k-m - Moved\n\n* **id**: K-9k-m\n',
            encoding="utf-8",
            newline="\n",
        )
        text = read(root, "docs/plan/A-first.md")
        start = text.index('<a id="k-9k-m"></a>')
        end = text.index("## Notes")
        (root / "docs/plan/A-first.md").write_text(
            text[:start] + text[end:], encoding="utf-8", newline="\n"
        )
        rc, out = run(script, root, "--check")
        assert rc == 1 and "Roadmap.md" in out, (rc, out)
        rc, out = run(script, root)
        assert rc == 0, out
        assert "[K-9k-m](plan/B-other.md#k-9k-m)" in read(root, "docs/Roadmap.md")
        rc, out = run(script, root, "--check")
        assert rc == 0, out


if __name__ == "__main__":
    main()
