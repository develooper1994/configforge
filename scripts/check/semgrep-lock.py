"""Make scripts/check/semgrep-requirements.txt from scripts/check/semgrep.in.

usage: python3 scripts/check/semgrep-lock.py

pip-compile cannot make this file: semgrep 1.186.0 pins mcp, the
opentelemetry packages and wcmatch to versions with known vulnerabilities
(osv-scanner, CI job `osv`), and the resolver refuses newer ones. So the
input names every package: `name==version` as the last compile chose it, or
`name>=version` for what is taken newer (the latest release, mcp the latest
1.x), and this script writes each with the SHA-256 of every file PyPI lists
for it. The file is installed with `pip install --require-hashes --no-deps`;
scan it with `pip check` and the project's gate.semgrep after a change.
"""

import json
import os
import re
import sys
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
BS = chr(92)


def info(name, version=None):
    """PyPI's JSON for a package, or for one of its versions."""
    tail = f"{version}/" if version else ""
    with urllib.request.urlopen(f"https://pypi.org/pypi/{name}/{tail}json", timeout=60) as r:
        return json.load(r)


def newest(name, data):
    """The release taken for a `>=` line: the latest, mcp the latest 1.x."""
    if name != "mcp":
        return data["info"]["version"]
    found = [k for k in data["releases"] if re.fullmatch(r"1[.]\d+[.]\d+", k) and data["releases"][k]]
    return max(found, key=lambda k: tuple(int(p) for p in k.split(".")))


def main():
    out = [
        "# Made by scripts/check/semgrep-lock.py from scripts/check/semgrep.in:",
        "# every package of semgrep 1.186.0 with the SHA-256 of each of its files,",
        "# installed with `pip install --require-hashes --no-deps -r`. mcp, the",
        "# opentelemetry packages and wcmatch are newer than semgrep pins, whose",
        "# versions have known vulnerabilities.",
        "",
    ]
    with open(os.path.join(HERE, "semgrep.in"), encoding="utf-8") as handle:
        wanted = [x.strip() for x in handle if x.strip() and not x.startswith("#")]
    for line in wanted:
        name, op, version = re.match(r"^([A-Za-z0-9_.-]+)(==|>=)([^,]+)", line).groups()
        if op == ">=":
            version = newest(name, info(name))
        hashes = sorted({u["digests"]["sha256"] for u in info(name, version)["urls"]})
        out.append(f"{name}=={version} {BS}")
        out.append(f" {BS}\n".join(f"    --hash=sha256:{h}" for h in hashes))
    with open(os.path.join(HERE, "semgrep-requirements.txt"), "w", encoding="utf-8", newline="\n") as handle:
        handle.write("\n".join(out) + "\n")


if __name__ == "__main__":
    sys.exit(main())
