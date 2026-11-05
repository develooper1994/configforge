"""How closely the library's readers follow each format's own standard,
measured with the standard's own test suite rather than claimed.

    python3 scripts/conformance.py <conform> <suites-dir> [--floor F] [--list]

<conform> is the program tests/conformance/conform.c builds into;
<suites-dir> holds the suites, each at the commit recorded in
docs/Conformance.md:

    JSONTestSuite    github.com/nst/JSONTestSuite         (RFC 8259)
    toml-test        github.com/toml-lang/toml-test       (TOML 1.0.0)
    yaml-test-suite  github.com/yaml/yaml-test-suite      (YAML 1.2)

For each: how many documents the standard calls valid were accepted,
and how many it calls invalid were refused; with --list, the ones that
went the other way by name. With --floor, the exit status is 1 when a
score falls below the floor file's (tests/conformance/floor.txt). A
suite's documents are data: they are read by the reader under test and
never run.
"""

import os
import subprocess
import sys


def run(conform, fmt, path):
    try:
        p = subprocess.run([conform, fmt, path], capture_output=True, timeout=20)
    except subprocess.TimeoutExpired:
        return 3  # a hang is a failure of its own
    return p.returncode


def report(name, fmt, conform, valid, invalid, out):
    good = [f for f in valid if run(conform, fmt, f) == 0]
    bad = [f for f in invalid if run(conform, fmt, f) == 1]
    wrong_valid = sorted(set(valid) - set(good))
    wrong_invalid = sorted(set(invalid) - set(bad))
    out.append((name, len(good), len(valid), len(bad), len(invalid),
                wrong_valid, wrong_invalid))


def json_suite(root):
    d = os.path.join(root, "JSONTestSuite", "test_parsing")
    files = sorted(os.listdir(d))
    valid = [os.path.join(d, f) for f in files if f.startswith("y_")]
    invalid = [os.path.join(d, f) for f in files if f.startswith("n_")]
    return valid, invalid


def toml_suite(root):
    base = os.path.join(root, "toml-test", "tests")
    listing = os.path.join(base, "files-toml-1.0.0")
    valid, invalid = [], []
    with open(listing, encoding="utf-8") as f:
        for line in f:
            rel = line.strip()
            if not rel.endswith(".toml"):
                continue
            (valid if rel.startswith("valid/") else invalid).append(os.path.join(base, rel))
    return valid, invalid


def yaml_suite(root):
    d = os.path.join(root, "yaml-test-suite", "src")
    valid, invalid = [], []
    # The suite's source is one YAML file a case; its in.yaml and its
    # error flag are extracted by the suite's own tooling, which is not
    # run here. Cases are counted only where that extraction exists.
    data = os.path.join(root, "yaml-test-suite", "data")
    if not os.path.isdir(data):
        return valid, invalid
    for case in sorted(os.listdir(data)):
        p = os.path.join(data, case)
        doc = os.path.join(p, "in.yaml")
        if os.path.isfile(doc):
            (invalid if os.path.exists(os.path.join(p, "error")) else valid).append(doc)
    return valid, invalid


def floors(path):
    """The floor file: "<format> <valid> <invalid>" a line."""
    out = {}
    with open(path, encoding="utf-8") as f:
        for line in f:
            words = line.split("#", 1)[0].split()
            if len(words) == 3:
                out[words[0]] = (int(words[1]), int(words[2]))
    return out


def main():
    args = sys.argv[1:]
    floor = {}
    verbose = "--list" in args
    if "--floor" in args:
        k = args.index("--floor")
        floor = floors(args[k + 1])
        del args[k:k + 2]
    args = [a for a in args if a != "--list"]
    conform, root = args[0], args[1]
    out = []
    v, i = json_suite(root)
    report("json", "json", conform, v, i, out)
    v, i = toml_suite(root)
    report("toml", "toml", conform, v, i, out)
    v, i = yaml_suite(root)
    if v or i:
        report("yaml", "yaml", conform, v, i, out)
    failed = False
    for name, g, nv, b, ni, wv, wi in out:
        print(f"{name}: valid accepted {g}/{nv}, invalid refused {b}/{ni}")
        if verbose:
            for f in wv:
                print(f"  refused a valid document: {os.path.basename(f)}")
            for f in wi:
                print(f"  accepted an invalid one:  {os.path.basename(f)}")
        if name in floor:
            fv, fi = floor[name]
            if g < fv or b < fi:
                print(f"  BELOW THE FLOOR {fv}/{fi}")
                failed = True
            elif g > fv or b > fi:
                print(f"  above the floor {fv}/{fi}: raise it in tests/conformance/floor.txt")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
