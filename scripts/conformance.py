"""How closely the library's readers follow each format's own standard,
measured with the standard's own test suite rather than claimed.

    python3 scripts/conformance.py <conform> <suites-dir> [--floor F] [--list]
    python3 scripts/conformance.py --self <conform> <repository>

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


# The project's own configuration, read as a user's would be: each file
# through its reader, and every key and value compared with what the
# reference reader for its format makes of it.
OWN = (
    ("CMakePresets.json", "json"),
    ("pyproject.toml", "toml"),
    (".clang-format", "yaml"),
    (".clang-tidy", "yaml"),
    ("mkdocs.yml", "yaml"),
    (".pre-commit-hooks.yaml", "yaml"),
)


def reference(fmt, path):
    """The reference reader's document, or None where there is none."""
    if fmt == "json":
        import json
        with open(path, encoding="utf-8") as f:
            return json.load(f)
    if fmt == "toml":
        try:
            import tomllib
        except ImportError:
            return None
        with open(path, "rb") as f:
            return tomllib.load(f)
    if fmt == "yaml":
        try:
            import yaml
        except ImportError:
            return None
        with open(path, encoding="utf-8") as f:
            return yaml.safe_load(f)
    return None


def flat(doc, prefix, out):
    """A document as the library's flat keys: a.b, a.0, a.count."""
    if isinstance(doc, dict):
        for k, v in doc.items():
            flat(v, f"{prefix}.{k}" if prefix else str(k), out)
    elif isinstance(doc, list):
        for i, v in enumerate(doc):
            flat(v, f"{prefix}.{i}", out)
        out[f"{prefix}.count"] = ("int", str(len(doc)))
    elif isinstance(doc, bool):
        out[prefix] = ("bool", "true" if doc else "false")
    elif isinstance(doc, int):
        out[prefix] = ("int", str(doc))
    elif isinstance(doc, float):
        out[prefix] = ("real", doc)
    elif doc is None:
        out[prefix] = ("null", "")
    else:
        out[prefix] = ("text", str(doc))


def same(want, got):
    wtype, wtext = want
    gtype, gtext = got
    if wtype == "real":
        try:
            return gtype == "real" and float(gtext) == wtext
        except ValueError:
            return False
    return wtype == gtype and wtext == gtext


def differential(conform, repo):
    """0 when every file the library and the reference both read agrees."""
    failed = False
    for name, fmt in OWN:
        path = os.path.join(repo, name)
        if not os.path.isfile(path):
            continue
        doc = reference(fmt, path)
        if doc is None:
            print(f"{name}: no reference reader for {fmt} here, skipped")
            continue
        p = subprocess.run([conform, fmt, path, "--keys"], capture_output=True, timeout=20)
        if p.returncode == 2:
            print(f"{name}: this build has no {fmt} reader, skipped")
            continue
        if p.returncode != 0:
            print(f"{name}: REFUSED - {p.stdout.decode('utf-8', 'replace').strip()}")
            failed = True
            continue
        got = {}
        for line in p.stdout.decode("utf-8", "replace").splitlines():
            parts = line.split("\t", 2)
            if len(parts) == 3:
                got[parts[0]] = (parts[1], parts[2])
        want = {}
        flat(doc, "", want)
        wrong = [k for k in sorted(set(want) | set(got))
                 if k not in want or k not in got or not same(want[k], got[k])]
        if wrong:
            failed = True
            print(f"{name}: {len(wrong)} of {len(want)} keys differ")
            for k in wrong[:10]:
                print(f"  {k}: reference {want.get(k)}, library {got.get(k)}")
        else:
            print(f"{name}: {len(want)} keys, the same as the reference")
    return 1 if failed else 0


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
    if args and args[0] == "--self":
        return differential(args[1], args[2])
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
