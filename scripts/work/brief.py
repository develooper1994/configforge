"""Print a worker's instructions, card and cited excerpts."""

import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
INSTRUCTIONS = "The brief every card's subagent gets"
HEADING = re.compile(r"^(#{1,6})[ \t]+(.+?)[ \t]*$")
PATH = r"[\w./-]+\.[\w-]+"
REFERENCE = re.compile(
    rf"`(?P<range_path>{PATH}):(?P<first>\d+)-(?P<last>\d+)`"
    rf"|(?<![\w/`.-])(?P<quote>`?)(?P<path>{PATH})(?P=quote)"
    r'\s*,?\s*\(?\s*"(?P<heading>[^"\n]+(?:\n[^"\n]+)*)"'
    r"|\b(?P<decision>D-\d+)\b"
)


def sections(lines):
    """Yield each Markdown heading with its start and exclusive end index."""
    headings = []
    fence = ""
    for index, line in enumerate(lines):
        marker = re.match(r"^ {0,3}(`{3,}|~{3,})(.*)$", line)
        if marker:
            run, rest = marker.groups()
            if not fence:
                fence = run
            elif run.startswith(fence) and not rest.strip():
                fence = ""
            continue
        if not fence and (match := HEADING.match(line)):
            headings.append((index, len(match[1]), " ".join(match[2].split())))
    for index, (start, level, title) in enumerate(headings):
        end = next(
            (row for row, depth, _ in headings[index + 1 :] if depth <= level),
            len(lines),
        )
        yield title, start, end


def read_lines(path):
    return path.read_text(encoding="utf-8").splitlines()


def find_card(card_id):
    for path in sorted((ROOT / "docs/plan").glob("*.md")):
        lines = read_lines(path)
        for title, start, end in sections(lines):
            if lines[start].startswith("## ") and title.startswith(f"{card_id} - "):
                return "\n".join(lines[start:end]).rstrip()
    raise ValueError(f"card {card_id} not found")


def standard_brief():
    lines = read_lines(ROOT / "CLAUDE.md")
    for title, start, end in sections(lines):
        if title == INSTRUCTIONS:
            section = "\n".join(lines[start:end])
            block = re.search(
                r"^```[^\n]*\n(.*?)^```[ \t]*$", section, re.MULTILINE | re.DOTALL
            )
            if block:
                return block[1].rstrip()
    raise ValueError(f"CLAUDE.md: missing fenced brief under {INSTRUCTIONS}")


def resolve(match):
    """Return the source, bounds, label and excerpt, or None if unresolved."""
    decision = match["decision"]
    path = "docs/Decisions.md" if decision else match["range_path"] or match["path"]
    source = (ROOT / path).resolve()
    if ROOT not in source.parents:
        return None
    try:
        lines = read_lines(source)
    except (OSError, UnicodeError):
        return None

    if match["range_path"]:
        first, last = int(match["first"]), int(match["last"])
        if 1 <= first <= last <= len(lines):
            return source, first - 1, last, f"{path}, lines {first}-{last}", lines
        return None

    heading = " ".join(match["heading"].split()) if not decision else decision
    for title, start, end in sections(lines):
        if decision:
            if not lines[start].startswith("## ") or not re.match(
                rf"^{decision}\b", title
            ):
                continue
        elif heading != title:
            continue
        return source, start, end, f"{path}, {heading}", lines
    return None


def render(card):
    parts = [standard_brief(), card]
    seen = set()
    for match in REFERENCE.finditer(card):
        excerpt = resolve(match)
        if excerpt is None:
            label = " ".join(match[0].split())
            if len(label) > 100:
                # A quote that runs on is a mis-read, not a citation:
                # name its start, not the paragraph it swallowed.
                label = label[:97] + "..."
            if label not in seen:
                parts.append(f"--- unresolved: {label}")
                seen.add(label)
            continue
        source, start, end, label, lines = excerpt
        key = (source, start, end)
        if key not in seen:
            parts.append(f"--- {label}\n" + "\n".join(lines[start:end]).rstrip())
            seen.add(key)
    return ("\n\n".join(parts) + "\n").encode("utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("card_id")
    parser.add_argument("--max-bytes", type=int, default=15000)
    args = parser.parse_args()
    try:
        output = render(find_card(args.card_id))
    except (OSError, ValueError) as error:
        parser.error(str(error))
    sys.stdout.buffer.write(output)
    if len(output) > args.max_bytes:
        warning = f"--- over budget: {len(output)} bytes; split the card or cite less\n"
        sys.stdout.buffer.write(warning.encode("utf-8"))


if __name__ == "__main__":
    main()
