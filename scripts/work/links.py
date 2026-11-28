#!/usr/bin/env python3
"""Anchor every card of docs/plan/*.md and link the card ids of docs/Roadmap.md.

Each card's `## <id> - ` heading gets `<a id="<id in lower case>"></a>` on
the line before it (the slugs of GitHub and of mkdocs differ for a heading,
an explicit id is the same on both). In docs/Roadmap.md a bare card id
becomes `[<id>](plan/<file>.md#<id in lower case>)`, except in a code span,
a fenced block, a heading or an existing link; a link of that shape whose
card moved is rewritten, and one whose card is gone is made a plain id again.

    python scripts/work/links.py           write
    python scripts/work/links.py --check   list the files it would change; exit 1
"""

import argparse
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from brief import ANCHOR, sections

ROOT = Path(__file__).resolve().parents[2]
ID = r"[A-Z]-\d+[a-z]?(?:-m)?"
CARD = re.compile(rf"^## ({ID}) - ")
# One match is one thing to leave alone, to repair, or an id to link.
TOKEN = re.compile(
    r"(?P<code>`+[^`\n]*`+)"
    r"|(?P<link>\[(?P<text>[^\]\n]*)\]\((?P<target>[^)\n]*)\))"
    r"|(?P<url>https?://\S+|<[^>\n]*>)"
    rf"|(?<![\w/.-])(?P<id>{ID})(?!\w|-\w|/)"
)
MANAGED = re.compile(r"^plan/[^#)]*\.md#[\w-]+$")


def read_lines(path):
    return path.read_text(encoding="utf-8").splitlines()


def write_lines(path, lines):
    path.write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")


def fenced(lines):
    """The indices of the lines inside a fenced block, fences included."""
    inside = set()
    fence = ""
    for index, line in enumerate(lines):
        marker = re.match(r"^ {0,3}(`{3,}|~{3,})(.*)$", line)
        if marker:
            run, rest = marker.groups()
            inside.add(index)
            if not fence:
                fence = run
            elif run.startswith(fence) and not rest.strip():
                fence = ""
        elif fence:
            inside.add(index)
    return inside


def card_files(root):
    return sorted((root / "docs/plan").glob("*.md"))


def anchored(lines):
    """The lines with exactly the anchors the cards need."""
    inside = fenced(lines)
    kept = [
        line for index, line in enumerate(lines) if index in inside or not ANCHOR.match(line)
    ]
    starts = []
    for _, start, _ in sections(kept):
        match = CARD.match(kept[start])
        if match and not match[1].startswith("D-"):
            starts.append((start, match[1]))
    for start, card_id in reversed(starts):
        kept.insert(start, f'<a id="{card_id.lower()}"></a>')
    return kept


def locate(root):
    """card id -> file name, for every card (D-numbers are decisions)."""
    found = {}
    for path in card_files(root):
        for line in read_lines(path):
            match = CARD.match(line)
            if match and not match[1].startswith("D-") and match[1] not in found:
                found[match[1]] = path.name
    return found


def target(cards, card_id):
    return f"plan/{cards[card_id]}#{card_id.lower()}"


def link_card(card_id, cards):
    """A card id as a Markdown link to its card; plain when it has none."""
    return f"[{card_id}]({target(cards, card_id)})" if card_id in cards else card_id


def link_line(line, cards):
    def replace(match):
        if match["id"]:
            return link_card(match["id"], cards)
        if match["link"] and re.fullmatch(ID, match["text"]) and MANAGED.match(match["target"]):
            return link_card(match["text"], cards)
        return match[0]

    return TOKEN.sub(replace, line)


def linked(lines, cards):
    inside = fenced(lines)
    return [
        line
        if index in inside or re.match(r"^ {0,3}#{1,6}[ \t]", line)
        else link_line(line, cards)
        for index, line in enumerate(lines)
    ]


def run(root, write):
    """Return the files (relative to root) that differ from what this writes."""
    changed = []
    cards = locate(root)
    for path in card_files(root):
        lines = read_lines(path)
        want = anchored(lines)
        if want != lines:
            changed.append(path)
            if write:
                write_lines(path, want)
    roadmap = root / "docs/Roadmap.md"
    if roadmap.exists():
        lines = read_lines(roadmap)
        want = linked(lines, cards)
        if want != lines:
            changed.append(roadmap)
            if write:
                write_lines(roadmap, want)
    return [path.relative_to(root).as_posix() for path in changed]


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    changed = run(args.root.resolve(), write=not args.check)
    if args.check and changed:
        print("scripts/work/links.py would change: " + ", ".join(changed) + ": run it")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
