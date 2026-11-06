"""Validate the rendered pages and searchable API before deployment."""

import json
from pathlib import Path
import sys


def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit("usage: check_docs.py <site-directory>")
    site = Path(sys.argv[1])
    for name in (
        "index.html",
        "docs/Guide/index.html",
        "docs/API/index.html",
        "docs/Doctor/index.html",
        "CONTRIBUTING/index.html",
        "search.html",
        "docs/stylesheets/site.css",
        "LICENSE",
    ):
        if not (site / name).is_file():
            raise SystemExit("docs: missing rendered file " + name)

    data = json.loads((site / "search/search_index.json").read_text(encoding="utf-8"))
    entries = data["docs"]
    locations = {entry["location"].split("#", 1)[0] for entry in entries}
    for location in ("", "docs/Guide/", "docs/API/", "docs/Doctor/"):
        if location not in locations:
            raise SystemExit("docs: page is not searchable: " + (location or "home"))
    api_text = " ".join(
        entry["title"] + " " + entry["text"]
        for entry in entries
        if entry["location"].startswith("docs/API/")
    )
    for symbol in ("cf_emit", "cf_report", "cf_live"):
        if symbol not in api_text:
            raise SystemExit("docs: API search is missing " + symbol)
    print("docs: rendered pages and %d search entries verified" % len(entries))


if __name__ == "__main__":
    main()
