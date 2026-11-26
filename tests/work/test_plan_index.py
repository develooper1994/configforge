"""plan_index marks a card done in docs/Plan.md's index, in its Status column."""

import importlib.util
import sys
from pathlib import Path
from tempfile import TemporaryDirectory

INDEX = """| Order | Card | Version | Size | Needs | Status |
| ----: | ---- | ------- | ----- | ----- | ------ |
| 1 | A-1 First | 1.9.0 | small | nothing | open |
| 2 | A-2 Second | 1.9.0 | small | A-1 | waits: A-1 |
| 3 | A-3 Third | 1.9.0 | small | B-1 | waits: B-1 |
"""

WANT = """| Order | Card | Version | Size | Needs | Status |
| ----: | ---- | ------- | ----- | ----- | ------ |
| 1 | A-1 First | 1.9.0 | small | nothing | done (abc1234) |
| 2 | A-2 Second | 1.9.0 | small | A-1 | open |
| 3 | A-3 Third | 1.9.0 | small | B-1 | waits: B-1 |
"""


def main():
    script, build = map(Path, sys.argv[1:])
    spec = importlib.util.spec_from_file_location("work", script)
    work = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(work)
    with TemporaryDirectory(dir=build) as directory:
        plan = Path(directory) / "Plan.md"
        plan.write_text(INDEX, encoding="utf-8")
        work.plan_index(str(plan), "A-1", "abc1234")
        got = plan.read_text(encoding="utf-8")
    assert got == WANT, got


if __name__ == "__main__":
    main()
