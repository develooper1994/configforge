"""Exercise the brief command against an isolated repository layout."""

import shutil
import subprocess
import sys
from pathlib import Path
from tempfile import TemporaryDirectory


def run_brief(script, *options, card="T-1"):
    return subprocess.run(
        [sys.executable, str(script), card, *options],
        capture_output=True,
        check=False,
    )


def main():
    script, fixture, build = map(Path, sys.argv[1:])
    with TemporaryDirectory(dir=build) as directory:
        root = Path(directory)
        shutil.copytree(fixture, root, dirs_exist_ok=True)
        fixture_script = root / "scripts" / "work" / "brief.py"
        fixture_script.parent.mkdir(parents=True)
        (root / "scripts" / "check").mkdir()
        shutil.copyfile(script, fixture_script)
        shutil.copyfile(
            script.with_name("index.py"), fixture_script.with_name("index.py")
        )
        shutil.copyfile(
            script.parents[1] / "check" / "ready.py", root / "scripts/check/ready.py"
        )
        done = run_brief(fixture_script)
        assert done.returncode == 0, done
        output = done.stdout
        over = run_brief(fixture_script, "--max-bytes", "100").stdout
        refused = run_brief(fixture_script, card="T-2")
        forced = run_brief(fixture_script, "--force", card="T-2")

    assert refused.returncode == 1 and refused.stdout == b"", refused
    assert b"T-2: fields: no 'id' field" in refused.stderr, refused
    assert b"T-2: status: 'None' is not open" in refused.stderr, refused
    assert forced.returncode == 0 and b"Outside the card." in forced.stdout, forced

    brief = output.decode("utf-8").replace("\r\n", "\n")
    parts = [
        "Fixture worker instructions.",
        "## T-1 - Fixture card",
        "--- docs/Decisions.md, D-70",
        "Decision content.",
        "Nested decision content.",
        "--- docs/Guide.md, A heading",
        "Heading content.",
        "Heading detail.",
        "--- docs/Guide.md, lines 9-10",
        "Range first line.",
        "Range second line.",
        '--- unresolved: `docs/Missing.md`, "Missing heading"',
    ]
    for part in parts:
        assert brief.count(part) == 1, brief
    assert [brief.index(part) for part in parts] == sorted(map(brief.index, parts)), (
        brief
    )
    assert "Outside" not in brief, brief
    assert "--- over budget:" not in brief, brief
    warning = f"--- over budget: {len(output)} bytes; split the card or cite less\n"
    assert over == output + warning.encode("utf-8"), over


if __name__ == "__main__":
    main()
