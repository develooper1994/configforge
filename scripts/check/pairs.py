#!/usr/bin/env python3
"""Every valid document of the conformance suites through every ordered
pair of the six writing formats (D-109).

    pairs.py <codec_pairs> <suites-dir>

<suites-dir> is where conformance.py keeps the suites (verify's
EXTRA=conformance fetches them). The valid documents are the ones
conformance.py lists - JSONTestSuite's y_ cases, json5-tests, toml-test,
the YAML test suite and the CBOR vectors - written as a manifest in a
temporary directory, one line `<format> TAB <path> TAB <mode>`, and
`codec_pairs --docs` runs them: a lone JSON, JSON5 or CBOR value is a
configuration only as the member `v` of a table, which mode `wrap` does
as conform does. Run on the build that has the built-in YAML parser
only; the libyaml and libfyaml builds run `conform` as before. The exit
code is codec_pairs's: 0 when every hop is what the table predicts.
"""

import os
import shutil
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import conformance


def manifest_lines(root, tmp):
    """(format, path, mode) for each valid document of each suite found."""
    lines = []
    json_dir = os.path.join(root, "JSONTestSuite", "test_parsing")
    if os.path.isdir(json_dir):
        for path in conformance.json_suite(root)[0]:
            lines.append(("json", path, "wrap"))
    if os.path.isdir(os.path.join(root, "json5-tests")):
        for path in conformance.json5_suite(root)[0]:
            lines.append(("json5", path, "wrap"))
    if os.path.isfile(os.path.join(root, "toml-test", "tests", "files-toml-1.1.0")):
        for path in conformance.toml_suite(root)[0]:
            lines.append(("toml", path, "-"))
    for path in conformance.yaml_suite(root)[0]:
        lines.append(("yaml", path, "-"))
    for path in conformance.cbor_suite(root, tmp)[0]:
        lines.append(("cbor", path, "wrap"))
    return lines


def main():
    if len(sys.argv) != 3:
        print(__doc__, file=sys.stderr)
        return 2
    program, root = sys.argv[1], sys.argv[2]
    tmp = tempfile.mkdtemp()
    try:
        lines = manifest_lines(root, tmp)
        if not lines:
            print(f"pairs: no suite in {root}", file=sys.stderr)
            return 2
        manifest = os.path.join(tmp, "pairs.manifest")
        with open(manifest, "w", encoding="utf-8", newline="\n") as out:
            out.writelines(f"{fmt}\t{path}\t{mode}\n" for fmt, path, mode in lines)
        return subprocess.call([program, "--docs", manifest])
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
