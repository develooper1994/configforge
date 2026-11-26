"""Say whether a card of docs/plan/*.md is ready to be carried out.

A card is ready when nothing is left to decide and every reference is
true. Each failure is one line `<id>: <check>: <what>`; the exit code is 1
when there is any. `--all` reports every open card and exits 0.
"""

import argparse
import os
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts" / "work"))
from brief import sections
from index import CARD, DONE, read_cards

PATH = r"[\w./-]+\.[\w-]+"
HEADER = re.compile(r"^\* \*\*([^*]+)\*\*:")
REQUIRED = [
    "id",
    "goal",
    "why",
    "needs",
    "size",
    "version",
    "status",
    "decided",
    "files",
    "steps",
    "accept",
    "stop if",
    "docs",
]
UNDECIDED = re.compile(
    r"\bTO DECIDE\b|\bTBD\b|\bTODO\b|(?i:to be rewritten|rewritten to)"
)
EITHER = re.compile(r"\beither\b", re.IGNORECASE)
CODE = re.compile(r"`[^`\n]*`")
FENCE = re.compile(r"^ {0,3}(`{3,}|~{3,})")
CITE = re.compile(
    rf"(?<![\w/.-])(?P<path>{PATH}):(?P<first>\d+)(?:-(?P<last>\d+))?(?!\d)"
)
IDENT = re.compile(r"`([A-Za-z_]\w*)(?:\(\))?`")
OPTIONS = re.compile(r"cf_\w+_options")
WINDOW = 60
SLACK = 10


def read_lines(path):
    return path.read_text(encoding="utf-8").splitlines()


def prose(lines):
    """Each line with fenced blocks and backticked spans removed."""
    out, fence = [], ""
    for line in lines:
        marker = FENCE.match(line)
        if marker:
            run = marker[1]
            if not fence:
                fence = run
            elif run.startswith(fence):
                fence = ""
            out.append("")
        else:
            out.append("" if fence else CODE.sub("", line))
    return out


def find_cards(root):
    """Yield (id, first line number, the card's lines)."""
    for path in sorted((root / "docs/plan").glob("*.md")):
        lines = read_lines(path)
        for _, start, end in sections(lines):
            match = CARD.match(lines[start])
            if match and not match[1].startswith("D-"):
                yield match[1], start + 1, lines[start:end]


def split_fields(lines):
    """Map a field name to its (offset, line) pairs; list the names found."""
    fields, names, current = {}, [], None
    for offset, line in enumerate(lines):
        header = HEADER.match(line)
        if header:
            current = header[1].strip().lower()
            names.append(current)
            fields.setdefault(current, [])
        if current:
            fields[current].append((offset, line))
    return fields, names


def bullets(pairs):
    """Group a field's lines into bullets: a marker line and what continues it."""
    groups = []
    for offset, line in pairs:
        if re.match(r"^\s*[*-] ", line) or not groups:
            groups.append([])
        groups[-1].append((offset, line))
    return groups


def version_key(text):
    if text == "not now":
        return (float("inf"),)
    return tuple(int(part) for part in text.split("."))


def check_fields(names):
    failures = []
    for name in REQUIRED:
        count = names.count(name)
        if count == 0:
            failures.append(("fields", f"no '{name}' field"))
        elif count > 1:
            failures.append(("fields", f"'{name}' field appears {count} times"))
    return failures


def check_decide(lines, fields, first):
    skip = {offset for offset, _ in fields.get("notes", [])}
    failures = []
    for offset, line in enumerate(prose(lines)):
        if offset in skip:
            continue
        for found in UNDECIDED.finditer(line):
            failures.append(
                ("decide", f"line {first + offset}: '{found[0]}' is not decided")
            )
    return failures


def check_settled(fields, first):
    pairs = fields.get("decided", [])
    text = prose([line for _, line in pairs])
    failures = []
    for (offset, _), line in zip(pairs, text):
        where = f"line {first + offset}"
        if "?" in line:
            failures.append(("settled", f"{where}: a question mark in decided"))
        if EITHER.search(line):
            failures.append(("settled", f"{where}: 'either' in decided"))
    return failures


def check_defaults(fields, first):
    failures = []
    for group in bullets(fields.get("decided", [])):
        text = " ".join(line for _, line in group)
        if "default" in text.lower():
            continue
        # Only a bullet that defines the struct must give its defaults; a
        # bullet that merely names it is not checked (the organiser,
        # 2026-11-27: the first run flagged every mention).
        defined = re.findall(r"typedef struct (cf_\w+_options)\b", text)
        for name in dict.fromkeys(defined):
            failures.append(
                (
                    "defaults",
                    f"line {first + group[0][0]}: {name} has no default on its bullet",
                )
            )
    return failures


_BASENAMES = {}


def basenames(root):
    """Every file of the tree by its name, for a citation that names a
    file without its directory (`main.c:22`); the build trees are not the
    tree."""
    key = str(root)
    if key not in _BASENAMES:
        found = {}
        for top, dirs, files in os.walk(root):
            dirs[:] = [d for d in dirs if d != ".git" and not d.startswith("build")]
            for name in files:
                found.setdefault(name, []).append(Path(top) / name)
        _BASENAMES[key] = found
    return _BASENAMES[key]


def read_cited(root, path):
    """The lines of a cited file, or the reason there are none. A bare
    file name is found by its name when exactly one file has it (the
    organiser, 2026-11-27, under the maintainer's standing approval)."""
    target = root / path
    if not target.is_file() and "/" not in path:
        hits = basenames(root).get(path, [])
        if len(hits) > 1:
            return f"{len(hits)} files are named {path}: cite its directory"
        if hits:
            target = hits[0]
    try:
        return read_lines(target)
    except (OSError, UnicodeError):
        return "no such file"


def check_cites(root, lines, first):
    failures = []
    cache = {}
    for offset, line in enumerate(lines):
        for cite in CITE.finditer(line):
            path = cite["path"]
            low = int(cite["first"])
            high = int(cite["last"] or low)
            span_text = f"{low}-{high}" if cite["last"] else f"{low}"
            where = f"line {first + offset}: {path}:{span_text}"
            if path not in cache:
                cache[path] = read_cited(root, path)
            target = cache[path]
            if isinstance(target, str):
                failures.append(("cites", f"{where}: {target}"))
                continue
            if high < low or low < 1 or high > len(target):
                failures.append(("cites", f"{where}: the file has {len(target)} lines"))
                continue
            window = line[max(0, cite.start() - WINDOW) : cite.start()]
            named = IDENT.findall(window)
            if not named:
                continue
            word = named[-1]
            near = target[max(0, low - 1 - SLACK) : high + SLACK]
            if not any(re.search(rf"\b{re.escape(word)}\b", row) for row in near):
                failures.append(
                    ("cites", f"{where}: '{word}' is not within {SLACK} lines of it")
                )
    return failures


def check_needs(cards, card_id):
    card = cards.get(card_id)
    if card is None:
        return []
    failures = []
    for need in card["needs"]:
        other = cards.get(need)
        if other is None:
            continue  # landed before the cards were written (index.py's rule)
        if not DONE.match(other["status"] or ""):
            failures.append(("needs", f"{need} is not done"))
        try:
            later = version_key(other["version"]) > version_key(card["version"])
        except (TypeError, ValueError, AttributeError):
            later = False
        if later:
            failures.append(
                ("needs", f"{need} is in {other['version']}, after {card['version']}")
            )
    return failures


def check_card(root, cards, card_id, first, lines):
    fields, names = split_fields(lines)
    failures = check_fields(names)
    status = cards.get(card_id, {}).get("status")
    if status != "open":
        failures.append(("status", f"'{status}' is not open"))
    failures += check_decide(lines, fields, first)
    failures += check_settled(fields, first)
    failures += check_defaults(fields, first)
    failures += check_cites(root, lines, first)
    failures += check_needs(cards, card_id)
    return [f"{card_id}: {check}: {what}" for check, what in failures]


def run(root, only=None):
    """Return (card id, status, failure lines) for each card asked for."""
    cards = read_cards(root, [])
    results = []
    for card_id, first, lines in find_cards(root):
        if only is None or card_id == only:
            status = cards.get(card_id, {}).get("status")
            results.append(
                (card_id, status, check_card(root, cards, card_id, first, lines))
            )
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("card_id", nargs="?")
    parser.add_argument("--all", action="store_true")
    parser.add_argument("--root", type=Path, default=ROOT)
    args = parser.parse_args()
    if bool(args.card_id) == args.all:
        parser.error("give a card id, or --all")
    results = run(args.root.resolve(), None if args.all else args.card_id)
    if args.all:
        ready = 0
        for card_id, status, failures in results:
            if status != "open":
                continue
            if failures:
                print("\n".join(failures))
            else:
                ready += 1
                print(f"{card_id}: ready")
        print(f"ready: {ready}")
        return 0
    if not results:
        parser.error(f"card {args.card_id} not found")
    failures = results[0][2]
    if failures:
        print("\n".join(failures))
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
