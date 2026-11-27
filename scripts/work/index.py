"""Write docs/Plan.md's index from the card files and docs/plan/Layout.md."""

import argparse
import re
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from brief import sections

ROOT = Path(__file__).resolve().parents[2]
BEGIN = "<!-- index begin (scripts/work/index.py) -->"
END = "<!-- index end -->"
HEADER = [
    "| Order | Card | Version | Size | Needs | Status |",
    "| ----: | ---- | ------- | ----- | ----- | ------ |",
]
ID = r"[A-Z]-\d+[a-z]?(?:-m)?"
ID_IN_TEXT = re.compile(rf"(?<![\w-])({ID})(?![\w])")
CARD = re.compile(rf"^## ({ID}) - (.+?)\s*$")
FIELD = re.compile(r"^\* \*\*([a-z]+)\*\*:[ \t]*(.*?)[ \t]*$")
VERSION = re.compile(r"^(\d+\.\d+\.\d+|not now)$")
STATUS = re.compile(r"^(open|parked|parked: \S.*|done \([0-9A-Za-z]+\))$")
DONE = re.compile(r"^done \(")
STALE = "docs/Plan.md's index is not what scripts/work/index.py writes: run it"


def read_lines(path):
    return path.read_text(encoding="utf-8").splitlines()


def write_lines(path, lines):
    path.write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")


def read_cards(root, errors):
    """Every card of docs/plan/*.md: id, title, size, needs, version, status."""
    cards = {}
    for path in sorted((root / "docs/plan").glob("*.md")):
        lines = read_lines(path)
        for _, start, end in sections(lines):
            match = CARD.match(lines[start])
            if not match or match[1].startswith("D-"):
                continue  # not a card; D-numbers are decisions
            card_id, title = match.groups()
            fields = {}
            for line in lines[start + 1 : end]:
                field = FIELD.match(line)
                if field and field[1] not in fields:
                    fields[field[1]] = field[2]
            if card_id in cards:
                errors.append(f"{card_id}: duplicate card id")
                continue
            cards[card_id] = {
                "title": title,
                "size": (fields.get("size", "").split() or [""])[0].strip("(),.;:"),
                # D-numbers are decisions, not cards: never a need.
                "needs": [
                    n
                    for n in ID_IN_TEXT.findall(fields.get("needs", ""))
                    if not n.startswith("D-")
                ],
                "version": fields.get("version"),
                "status": fields.get("status"),
                "file": path,
            }
    return cards


DONE_FILE = "docs/plan/Done.md"
DONE_INTRO = [
    "# Done",
    "",
    ("Cards that landed, one line each; the card's full text is in git at the"
    " commit before it was archived: `git show <archive commit>^:docs/plan/<file>`."
    " `python scripts/work/index.py --archive` adds the cards that are done."),
    "",
    "| id | goal | landed | file |",
    "| -- | ---- | ------ | ---- |",
]
DONE_ROW = re.compile(rf"^\| ({ID}) \|")


def archived(root):
    """The ids listed in docs/plan/Done.md: landed, and no longer cards."""
    path = root / DONE_FILE
    if not path.exists():
        return set()
    return {m[1] for line in read_lines(path) if (m := DONE_ROW.match(line))}


def archive(root):
    """Move every done card into Done.md; return the ids moved."""
    done_path = root / DONE_FILE
    rows = read_lines(done_path) if done_path.exists() else list(DONE_INTRO)
    moved = []
    for path in sorted((root / "docs/plan").glob("*.md")):
        if path.name in ("Layout.md", "A-architecture.md", "worker-notes.md", "Done.md"):
            continue
        lines = read_lines(path)
        drop = []
        for _, start, end in sections(lines):
            match = CARD.match(lines[start])
            if not match or match[1].startswith("D-"):
                continue
            status = next(
                (
                    f[2]
                    for line in lines[start + 1 : end]
                    if (f := FIELD.match(line)) and f[1] == "status"
                ),
                "",
            )
            hash_ = re.match(r"^done \(([0-9A-Za-z]+)\)$", status)
            if not hash_:
                continue
            goal = re.sub(r"(?<!\\)\|", r"\\|", match[2])
            rows.append(f"| {match[1]} | {goal} | {hash_[1]} | {path.name} |")
            moved.append(match[1])
            drop.append((start, end))
        for start, end in reversed(drop):
            del lines[start:end]
        if drop:
            while lines and not lines[-1].strip():
                lines.pop()
            write_lines(path, lines)
    if moved:
        write_lines(done_path, rows)
    return moved


def layout_order(root, cards):
    order = []
    text = (root / "docs/plan/Layout.md").read_text(encoding="utf-8")
    for card_id in ID_IN_TEXT.findall(text):
        if card_id in cards and card_id not in order:
            order.append(card_id)
    return order


def git_lines(root, *args):
    done = subprocess.run(
        ["git", "-C", str(root), *args],
        capture_output=True,
        text=True,
        encoding="utf-8",
        check=False,
    )
    return done.stdout.splitlines() if done.returncode == 0 else None


def landed(root):
    """The subjects of main's commits; None when the history is not here.

    A staged copy has no repository and a CI checkout may be shallow or
    have no local main: a need without a card cannot be judged there.
    """
    if not (root / ".git").exists():
        return None
    if git_lines(root, "rev-parse", "--is-shallow-repository") != ["false"]:
        return None
    for ref in ("main", "origin/main"):
        subjects = git_lines(root, "log", "--format=%s", ref)
        if subjects is not None:
            return subjects
    return None


def build(root):
    """Return the table's lines and the errors."""
    errors = []
    cards = read_cards(root, errors)
    order = layout_order(root, cards)
    subjects = landed(root)
    gone = archived(root)
    for card_id, card in cards.items():
        if card["version"] is None or not VERSION.match(card["version"]):
            what = (
                "no version field"
                if card["version"] is None
                else (f"version '{card['version']}' is not X.Y.Z or 'not now'")
            )
            errors.append(f"{card_id}: {what}")
        if card["status"] is None:
            errors.append(f"{card_id}: no status field")
        elif not STATUS.match(card["status"]):
            errors.append(
                f"{card_id}: status '{card['status']}' is none of open, parked,"
                " parked: <why>, done (<hash>)"
            )
        if card_id not in order:
            errors.append(f"{card_id}: not in docs/plan/Layout.md")
        for need in card["needs"]:
            if (
                need not in cards
                and need not in gone
                and subjects is not None
                and not any(s.startswith(f"{need}: ") for s in subjects)
            ):
                errors.append(f"{card_id}: need {need} has no card and never landed")
    if errors:
        return None, errors

    def done(need):
        if need in cards:
            return DONE.match(cards[need]["status"]) is not None
        return True

    rows = []
    for number, card_id in enumerate(order, 1):
        card = cards[card_id]
        status = card["status"]
        if status == "open":
            wait = next((n for n in card["needs"] if not done(n)), None)
            if wait:
                status = f"waits: {wait}"
        title = re.sub(r"(?<!\\)\|", r"\\|", card["title"])
        needs = ", ".join(card["needs"]) or "nothing"
        rows.append(
            f"| {number} | {card_id} {title} | {card['version']} | {card['size']}"
            f" | {needs} | {status} |"
        )
    return HEADER + rows, []


def markers(lines):
    try:
        return lines.index(BEGIN), lines.index(END)
    except ValueError:
        return None


def set_status(cards, card_id, value):
    path = cards[card_id]["file"]
    lines = read_lines(path)
    for _, start, end in sections(lines):
        match = CARD.match(lines[start])
        if match and match[1] == card_id:
            for row in range(start + 1, end):
                if re.match(r"^\* \*\*status\*\*:", lines[row]):
                    lines[row] = f"* **status**: {value}"
                    write_lines(path, lines)
                    return
    raise ValueError(f"{card_id}: no status field")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--check", action="store_true")
    mode.add_argument("--archive", action="store_true")
    mode.add_argument("--done", nargs=2, metavar=("ID", "HASH"))
    args = parser.parse_args()
    root = args.root.resolve()

    plan = root / "docs/Plan.md"
    lines = read_lines(plan)
    found = markers(lines)
    if found is None:
        print(f"docs/Plan.md: lines '{BEGIN}' and '{END}' not found")
        return 1

    if args.done:
        card_id, hash_ = args.done
        errors = []
        cards = read_cards(root, errors)
        if card_id not in cards:
            print(f"{card_id}: no such card")
            return 1
        try:
            set_status(cards, card_id, f"done ({hash_})")
        except ValueError as error:
            print(error)
            return 1

    if args.archive:
        moved = archive(root)
        print(f"archived {len(moved)} card(s): " + ", ".join(moved))

    table, errors = build(root)
    if errors:
        print("\n".join(errors))
        return 1
    first, last = found
    if args.check:
        have = lines[first + 1 : last]
        if have == table:
            return 0
        row = next(
            (
                (a, b)
                for a, b in zip(have + [None] * len(table), table + [None] * len(have))
                if a != b
            ),
        )
        print(STALE)
        print(f"first differing row: have {row[0]!r}, would write {row[1]!r}")
        return 1
    write_lines(plan, lines[: first + 1] + table + lines[last:])
    return 0


if __name__ == "__main__":
    sys.exit(main())
