"""How closely the library's readers follow each format's own standard,
measured with the standard's own test suite rather than claimed.

    python3 scripts/check/conformance.py <conform> <suites-dir> [--floor F] [--list]
                                   [--yaml-name NAME]
                                   [--codec <library> <symbol> <format>]
    python3 scripts/check/conformance.py --self <conform> <repository>

With --codec, a codec from a shared library is scored instead of this
library's reader, on <format>'s suite alone and against its floors: a
format of one's own measured as the built-in ones are (D-68).

<conform> is the program tests/conformance/conform.c builds into;
<suites-dir> holds the suites, each at the commit recorded in
docs/Conformance.md:

    JSONTestSuite    github.com/nst/JSONTestSuite         (RFC 8259)
    toml-test        github.com/toml-lang/toml-test       (TOML 1.1.0)
    yaml-test-suite  github.com/yaml/yaml-test-suite      (YAML 1.2)
    json5-tests      github.com/json5/json5-tests         (JSON5 1.0.0)
    cbor-test-vectors github.com/cbor/test-vectors        (RFC 8949 Appendix A)

For each: how many documents the standard calls valid were accepted,
and how many it calls invalid were refused; with --list, the ones that
went the other way by name. With --floor, the exit status is 1 when a
score falls below the floor file's (tests/conformance/floor.txt). A
suite's documents are data: they are read by the reader under test and
never run.
"""

import os
import shutil
import subprocess
import sys
import tempfile


# conform's arguments after the document: ["--codec", library, symbol]
# when a codec from a shared library is scored (--codec, D-68).
EXTRA = []


def run(conform, fmt, path):
    """The exit status, and what conform printed."""
    try:
        p = subprocess.run([conform, fmt, path] + EXTRA, capture_output=True, timeout=20)
    except subprocess.TimeoutExpired:
        return 3, ""  # a hang is a failure of its own
    return p.returncode, p.stdout.decode("utf-8", "replace")


# What a configuration has no place for, which the YAML reader refuses by
# name though the document is YAML (D-39, D-63): counted apart, so the
# grammar's score is the grammar's. JSON's lone values are measured
# wrapped instead (conform.c), and TOML has none.
MODEL = (
    "a configuration is a mapping at the top",
    "a second document",
    "a key has to be a scalar",
    "an empty key",
    "not one this library resolves",
    # CBOR's items a configuration has no place for (D-91): a byte string,
    # undefined, a simple value, an integer past 64 bits, a key that is no
    # text, and a tag the reader does not read.
    "which a configuration does not hold",
    "which this reader does not read",
)


def report(name, fmt, conform, valid, invalid, out, accepted=None):
    good, model = [], []
    for f in valid:
        rc, text = run(conform, fmt, f)
        if rc == 0:
            good.append(f)
        elif rc == 1 and any(m in text for m in MODEL):
            model.append(f)
    bad = [f for f in invalid if run(conform, fmt, f)[0] == 1]
    if accepted is not None:
        accepted[name] = (fmt, good)
    wrong_valid = sorted(set(valid) - set(good) - set(model))
    wrong_invalid = sorted(set(invalid) - set(bad))
    out.append((name, len(good), len(valid) - len(model), len(bad), len(invalid),
                wrong_valid, wrong_invalid, len(model)))


def detected(conform, path):
    """The format conform's content detection (D-73) says a file is, from
    its bytes alone - no name, so as if renamed without an extension - or
    None when it chooses none."""
    try:
        p = subprocess.run([conform, "--detect", path], capture_output=True, timeout=20)
    except subprocess.TimeoutExpired:
        return None
    return p.stdout.decode("utf-8", "replace").strip() if p.returncode == 0 else None


def detect_score(conform, fmt, good):
    """(detected as its own format, tried, the ones that were not): of the
    valid documents the reader accepts, how many content detection says are
    `fmt`'s."""
    missed = [f for f in good if detected(conform, f) != fmt]
    return len(good) - len(missed), len(good), missed


ESCAPES = {"\\": "\\", "t": "\t", "n": "\n", "r": "\r", "0": "\0"}


def unescape(s):
    """conform's escaping undone: \\\\, \\t, \\n, \\r and \\0."""
    out, i = [], 0
    while i < len(s):
        if s[i] == "\\" and i + 1 < len(s) and s[i + 1] in ESCAPES:
            out.append(ESCAPES[s[i + 1]])
            i += 2
        else:
            out.append(s[i])
            i += 1
    return "".join(out)


def keys_of(conform, fmt, path):
    """What the reader made of a document, as {flat key: (type, text)};
    None when it refused it."""
    p = subprocess.run([conform, fmt, path, "--keys"] + EXTRA, capture_output=True,
                       timeout=20)
    if p.returncode != 0:
        return None
    got = {}
    # Split on the line feed alone: an escaped value holds none, and a
    # carriage return in one is data.
    for line in p.stdout.decode("utf-8", "replace").split("\n"):
        parts = line.split("\t", 2)
        if len(parts) == 3:
            got[unescape(parts[0])] = (parts[1], unescape(parts[2]))
    return got


def join(prefix, name):
    """A flat key as conform builds one: segments joined by dots, the
    first without one - so an empty name still makes its own segment."""
    return name if prefix is None else f"{prefix}.{name}"


# toml-test's expected values: {"type": ..., "value": ...} at each leaf.
# Dates and times, and inf and nan, are read here as their text (D-62),
# so they are compared as text, normalised the way toml-test writes them.
TOML_TYPES = {"string": "text", "integer": "int", "float": "real", "bool": "bool"}


def toml_expected(doc, prefix, out):
    if isinstance(doc, dict) and set(doc) == {"type", "value"} and \
            isinstance(doc["type"], str):
        t, v = doc["type"], doc["value"]
        if t.startswith("date") or t.startswith("time"):
            out[prefix] = ("datetime", v)
        elif t == "float" and v.lstrip("+-") in ("inf", "nan"):
            out[prefix] = ("nonfinite", v.lstrip("+") if "inf" in v else "nan")
        else:
            out[prefix] = (TOML_TYPES.get(t, t), v)
    elif isinstance(doc, dict):
        for k, v in doc.items():
            toml_expected(v, join(prefix, k), out)
    elif isinstance(doc, list):
        for i, v in enumerate(doc):
            toml_expected(v, join(prefix, str(i)), out)
        out[f"{prefix}.count"] = ("int", str(len(doc)))


def millis(t):
    """A date-time's fraction of a second with its trailing zeros gone."""
    import re
    return re.sub(r"\.(\d*?)0*(?=[^\d]|$)", lambda m: "." + m.group(1) if m.group(1) else "", t)


def toml_same(want, got):
    wtype, wtext = want
    gtype, gtext = got
    if wtype == "datetime":
        # toml-test writes the delimiter as T and the zone as Z.
        g = gtext.replace("t", "T").replace("z", "Z")
        if len(g) > 10 and g[10] == " ":
            g = g[:10] + "T" + g[11:]
        # TOML 1.1 lets the seconds go; toml-test writes them as :00.
        import re
        g = re.sub(r"(^|T)(\d\d:\d\d)(?=$|[Z+-])",
                   lambda m: m.group(1) + m.group(2) + ":00", g)
        # and fractions of a second to milliseconds: .6 as .600.
        return gtype == "untyped" and millis(g) == millis(wtext)
    if wtype == "nonfinite":
        g = gtext.lstrip("+")
        return gtype == "untyped" and (g == wtext or (wtext == "nan" and g == "-nan"))
    if wtype == "real":
        try:
            return gtype == "real" and float(gtext) == float(wtext)
        except ValueError:
            return False
    if wtype == "int":
        return gtype == "int" and int(gtext) == int(wtext)
    return wtype == gtype and wtext == gtext


def toml_values(conform, valid, wrong_out):
    """How many valid documents were read to exactly toml-test's values."""
    import json
    good = 0
    for path in valid:
        expected = os.path.splitext(path)[0] + ".json"
        got = keys_of(conform, "toml", path)
        if got is None or not os.path.isfile(expected):
            wrong_out.append(path)
            continue
        with open(expected, encoding="utf-8") as f:
            want = {}
            toml_expected(json.load(f), None, want)
        if set(want) == set(got) and all(toml_same(want[k], got[k]) for k in want):
            good += 1
        else:
            wrong_out.append(path)
    return good


def json_values(conform, valid, wrong_out):
    """How many valid JSON documents were read to the values Python's json
    reads, the non-object root measured as {"v": ...} as conform reads it."""
    import json
    good = 0
    for path in valid:
        got = keys_of(conform, "json", path)
        unread = False  # a lone null is a document, so None cannot say this
        try:
            with open(path, encoding="utf-8") as f:
                doc = json.load(f)
        except (ValueError, UnicodeDecodeError):
            unread = True
        if got is None or unread:
            wrong_out.append(path)
            continue
        want = {}
        flat(doc if isinstance(doc, dict) else {"v": doc}, None, want)
        if set(want) == set(got) and all(same(want[k], got[k]) for k in want):
            good += 1
        else:
            wrong_out.append(path)
    return good


def yaml_values(conform, valid, wrong_out):
    """Of the valid YAML documents read whose in.json the suite gives, how
    many were read to its values: (good, compared). A plain scalar is
    untyped here and compared as the text of the suite's value."""
    import json
    good = compared = 0
    for path in valid:
        expected = os.path.join(os.path.dirname(path), "in.json")
        if not os.path.isfile(expected):
            continue
        got = keys_of(conform, "yaml", path)
        if got is None:
            continue  # refused: counted by report()
        try:
            with open(expected, encoding="utf-8") as f:
                doc = json.load(f)
        except ValueError:
            continue  # several documents: a model refusal
        if not isinstance(doc, dict):
            continue
        compared += 1
        want = {}
        flat(doc, None, want)
        if set(want) == set(got) and all(same(want[k], got[k]) for k in want):
            good += 1
        else:
            wrong_out.append(path)
    return good, compared


def json5_values(conform, valid, wrong_out):
    """How many valid JSON5 documents were read to the values the json5
    package reads, the root measured as {"v": ...} as conform reads it;
    (good, compared), compared 0 where the package is not installed."""
    try:
        import json5
    except ImportError:
        return 0, 0
    good = 0
    for path in valid:
        got = keys_of(conform, "json5", path)
        try:
            with open(path, encoding="utf-8") as f:
                doc = json5.load(f)
        except (ValueError, UnicodeDecodeError):
            wrong_out.append(path)
            continue
        if got is None:
            wrong_out.append(path)
            continue
        want = {}
        flat({"v": doc}, None, want)
        if set(want) == set(got) and all(same(want[k], got[k]) for k in want):
            good += 1
        else:
            wrong_out.append(path)
    return good, len(valid)


def json5_suite(root):
    """json5-tests: .json and .json5 files are valid JSON5; .js files are
    valid ECMAScript and not JSON5, and .txt files neither."""
    base = os.path.join(root, "json5-tests")
    valid, invalid = [], []
    if not os.path.isdir(base):
        return valid, invalid
    for top, dirs, files in os.walk(base):
        dirs.sort()
        if ".git" in dirs:
            dirs.remove(".git")
        for f in sorted(files):
            ext = os.path.splitext(f)[1]
            if ext in (".json", ".json5"):
                valid.append(os.path.join(top, f))
            elif ext in (".js", ".txt"):
                invalid.append(os.path.join(top, f))
    return valid, invalid


def json_suite(root):
    d = os.path.join(root, "JSONTestSuite", "test_parsing")
    files = sorted(os.listdir(d))
    valid = [os.path.join(d, f) for f in files if f.startswith("y_")]
    invalid = [os.path.join(d, f) for f in files if f.startswith("n_")]
    return valid, invalid


def toml_suite(root):
    base = os.path.join(root, "toml-test", "tests")
    listing = os.path.join(base, "files-toml-1.1.0")  # the latest, a superset of 1.0
    valid, invalid = [], []
    with open(listing, encoding="utf-8") as f:
        for line in f:
            rel = line.strip()
            if not rel.endswith(".toml"):
                continue
            (valid if rel.startswith("valid/") else invalid).append(os.path.join(base, rel))
    return valid, invalid


def yaml_suite(root):
    """The suite's data branch, the cases as files: a directory a case
    (or one a sub-case) holding in.yaml, and `error` where it is invalid."""
    data = os.path.join(root, "yaml-test-suite-data")
    valid, invalid = [], []
    if not os.path.isdir(data):
        return valid, invalid
    for top, dirs, files in os.walk(data):
        dirs.sort()
        if ".git" in dirs:
            dirs.remove(".git")
        if "in.yaml" in files:
            doc = os.path.join(top, "in.yaml")
            (invalid if "error" in files else valid).append(doc)
    return valid, invalid


# RFC 8949 Appendix F.1's malformed items, as (group, hex): the group is
# F.1's heading for it. Hex strings are facts, not the RFC's text. The
# valid vectors are fetched (cbor-test-vectors); these are in the RFC only.
INVALID_CBOR = tuple(
    (group, h)
    for group, items in (
        ("end of input in a head",
         "18 19 1a 1b 1901 1a0102 1b01020304050607 38 58 78 98 9a01ff00 b8 d8 f8 "
         "f900 fa0000 fb000000"),
        ("definite-length strings with short data",
         "41 61 5affffffff00 5bffffffffffffffff010203 7affffffff00 "
         "7b7fffffffffffffff010203"),
        ("definite-length maps and arrays not closed with enough items",
         "81 818181818181818181 8200 a1 a20102 a100 a2000000"),
        ("tag number not followed by tag content", "c0"),
        ("indefinite-length strings not closed by a break", "5f4100 7f6100"),
        ("indefinite-length maps and arrays not closed by a break",
         "9f 9f0102 bf bf01020102 819f 9f8000 9f9f9f9f9fffffffff 9f819f819f9fffffff"),
        ("reserved additional information values",
         "1c 1d 1e 3c 3d 3e 5c 5d 5e 7c 7d 7e 9c 9d 9e bc bd be dc dd de fc fd fe"),
        ("reserved two-byte encodings of simple values", "f800 f801 f818 f81f"),
        ("indefinite-length string chunks not of the correct type",
         "5f00ff 5f21ff 5f6100ff 5f80ff 5fa0ff 5fc000ff 5fe0ff 7f4100ff"),
        ("indefinite-length string chunks not definite length",
         "5f5f4100ffff 7f7f6100ffff"),
        ("a break on its own", "ff"),
        ("a break in a definite-length array, map or tag",
         "81ff 8200ff a1ff a1ff00 a100ff a20000ff 9f81ff 9f829f819f9fffffffff"),
        ("a break in a map's value position", "bf00ff bf000000ff"),
        ("major type 0, 1 or 6 with additional information 31", "1f 3f df"),
    )
    for h in items.split()
)


def cbor_suite(root, tmp):
    """cbor-test-vectors' Appendix A as files of bytes, and F.1's malformed
    items likewise, in `tmp`: (valid, invalid, {path: vector})."""
    import base64
    import json
    valid, invalid, vectors = [], [], {}
    path = os.path.join(root, "cbor-test-vectors", "appendix_a.json")
    if not os.path.isfile(path):
        return valid, invalid, vectors
    malformed = {h for _group, h in INVALID_CBOR}
    with open(path, encoding="utf-8") as f:
        for n, v in enumerate(json.load(f)):
            # The repository follows RFC 7049's Appendix A, which lists
            # simple(24), f818; RFC 8949 calls that encoding malformed
            # (F.1), and the later RFC wins: the item is scored there.
            if v["hex"].lower() in malformed:
                continue
            p = os.path.join(tmp, f"a{n:03d}.cbor")
            with open(p, "wb") as out:
                out.write(base64.b64decode(v["cbor"]))
            valid.append(p)
            vectors[p] = v
    for n, (_group, h) in enumerate(INVALID_CBOR):
        p = os.path.join(tmp, f"f{n:03d}.cbor")
        with open(p, "wb") as out:
            out.write(bytes.fromhex(h))
        invalid.append(p)
    return valid, invalid, vectors


def cbor_values(conform, valid, vectors, wrong_out):
    """Of the vectors with a `decoded` value that were read, how many were
    read to it: (good, compared). A vector with only a diagnostic notation
    has no JSON form to compare with."""
    good = compared = 0
    for path in valid:
        v = vectors[path]
        if "decoded" not in v:
            continue
        got = keys_of(conform, "cbor", path)
        if got is None:
            continue  # refused: counted by report()
        compared += 1
        want = {}
        flat({"v": v["decoded"]}, None, want)
        if set(want) == set(got) and all(same(want[k], got[k]) for k in want):
            good += 1
        else:
            wrong_out.append(path)
    return good, compared


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
            flat(v, join(prefix, str(k)), out)
    elif isinstance(doc, list):
        for i, v in enumerate(doc):
            flat(v, join(prefix, str(i)), out)
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


NULLS = ("", "~", "null", "Null", "NULL")
BOOLS = {"true": "true", "True": "true", "TRUE": "true",
         "false": "false", "False": "false", "FALSE": "false"}


def untyped_same(wtype, wtext, gtext):
    """Untyped text - a YAML plain scalar, typed by its definition - against
    a reference value: the same when the text says that value."""
    try:
        if wtype == "int":
            return int(gtext.replace("_", ""), 0) == int(wtext)
        if wtype == "real":
            g = float(gtext)  # JSON5's Infinity and NaN are their text
            return g == wtext or (g != g and wtext != wtext)
    except ValueError:
        return False
    if wtype == "bool":
        return BOOLS.get(gtext) == wtext
    if wtype == "null":
        return gtext in NULLS
    return gtext == wtext


def same(want, got):
    wtype, wtext = want
    gtype, gtext = got
    if gtype == "untyped":
        return untyped_same(wtype, wtext, gtext)
    if wtype == "null":
        return gtype == "null"  # its text is the writer's spelling
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
        if not isinstance(doc, dict):
            # A hook list, say: a file, not a configuration (D-39).
            print(f"{name}: its root is a {type(doc).__name__}, and a configuration "
                  "is a mapping; skipped")
            continue
        p = subprocess.run([conform, fmt, path], capture_output=True, timeout=20)
        if p.returncode == 2:
            print(f"{name}: this build has no {fmt} reader, skipped")
            continue
        if p.returncode != 0:
            print(f"{name}: REFUSED - {p.stdout.decode('utf-8', 'replace').strip()}")
            failed = True
            continue
        got = keys_of(conform, fmt, path)
        want = {}
        flat(doc, None, want)
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


# INI has no standard, so its reader is compared with the most used one,
# Python's configparser (RawConfigParser: interpolation off; strict off, so
# that a repeated key or section is read, last one winning, as the library
# reads it; keys keep their case, the library's reader does not fold it).
# Each case is a small document, read by both and compared as text: the
# same keys with the same text, or both refusing. A case the two differ on
# by design says why, and is checked to still differ; any other
# difference is a finding and counts against the score (docs/Formats.md).
INI_CASES = (
    # name, document, why they differ (None: they should agree)
    ("plain", "[s]\na = 1\nb = two\n", None),
    ("sections", "[a]\nx = 1\n[b]\nx = 2\n", None),
    ("dotted-section", "[s.t.u]\ndeep = 1\n", None),
    ("spaces-around", "[s]\n  spaced   =   value with spaces  \n", None),
    ("no-spaces", "[s]\nk=v\n", None),
    ("equals-in-value", "[s]\nk = a = b\n", None),
    ("empty-value", "[s]\nk =\nj = \n", None),
    ("full-line-hash", "# c\n[s]\n# c\nk = 1\n", None),
    ("full-line-semicolon", "; c\n[s]\n; c\nk = 1\n", None),
    ("indented-comment", "[s]\n   # c\nk = 1\n", None),
    ("blank-lines", "\n\n[s]\n\nk = 1\n\n\nj = 2\n\n", None),
    ("inline-hash", "[s]\nk = v # not a comment\n", None),
    ("inline-semicolon", "[s]\nk = v ; not a comment\n", None),
    ("hash-in-value", "[s]\nk = a#b\n", None),
    ("case-of-keys", "[s]\nMixed = 1\nmixed2 = 2\n", None),
    ("case-of-sections", "[Server]\nk = 1\n", None),
    ("duplicate-key", "[s]\nk = 1\nk = 2\n", None),
    ("duplicate-section", "[a]\nx = 1\n[a]\ny = 2\n", None),
    ("crlf", "[s]\r\nk = 1\r\nj = 2\r\n", None),
    ("last-line-no-newline", "[s]\nk = 1", None),
    ("many-sections", "[a]\n[b]\nk = 1\n[c]\n", None),
    ("percent", "[s]\nk = 100%\n", None),
    ("interpolation-text", "[s]\na = 1\nk = %(a)s ${a}\n", None),
    ("unicode", "[s]\nk = é中\n", None),
    ("tab-around", "[s]\nk\t=\tv\t\n", None),
    ("empty-document", "", None),
    ("only-comments", "# a\n; b\n", None),
    ("unterminated-section", "[unterminated\nk = 1\n", None),
    ("empty-section-name", "[]\nk = 1\n", None),
    ("line-without-equals", "[s]\nno equals here\n", None),
    ("empty-key", "[s]\n= no key\n", None),
    ("double-quote-stripped", '[s]\nk = "a quoted value"\n',
     "quotes around a whole value are removed; configparser keeps them"),
    ("single-quote-stripped", "[s]\nk = 'single'\n",
     "quotes around a whole value are removed; configparser keeps them"),
    ("quote-inside-kept", '[s]\nk = say "hi"\n', None),
    ("quote-one-sided", '[s]\nk = "unterminated quote\n', None),
    ("quoted-with-comment", '[s]\nk = "a # b"\n',
     "quotes around a whole value are removed; configparser keeps them"),
    ("key-before-section", "root = 1\n[s]\nk = 2\n",
     "keys before any section are allowed (the root table); configparser refuses them"),
    ("colon-delimiter", "[s]\nk: v\n",
     "only '=' separates a key from its value; configparser also takes ':'"),
    ("colon-in-key-value", "[s]\nurl = http://x:80\n", None),
    ("continuation", "[s]\nk = a\n  b\n  c\n",
     "no continuation lines: an indented line is a line of its own, and "
     "this one is refused for having no '='; configparser joins them"),
    ("continuation-blank", "[s]\nk = a\n\n  b\n",
     "no continuation lines; configparser joins them"),
    ("array-append", "[s]\nports[] = 80\nports[] = 443\n",
     "name[] appends to an array (D-60); configparser takes it as a key"),
    ("array-index", "[s]\nports[1] = 8443\n",
     "name[i] is an element of an array (D-60); configparser takes it as a key"),
    ("section-with-spaces", "[ a ]\nk = 1\n",
     "a section's name is trimmed; configparser keeps the spaces"),
    ("section-trailing-text", "[a] x\nk = 1\n",
     "text after a section header is refused; configparser ignores it"),
    ("section-comment", "[a] # c\nk = 1\n", None),
    ("space-in-key", "[s]\nBad Key = 1\n", None),
    ("default-section", "[DEFAULT]\nk = 1\n[s]\nj = 2\n",
     "[DEFAULT] is an ordinary section here; configparser's values for every section"),
)


def ini_read_reference(text):
    """configparser's document as {flat key: text}, or None where it refuses."""
    import configparser
    p = configparser.RawConfigParser(strict=False)
    p.optionxform = str
    try:
        p.read_string(text)
    except configparser.Error:
        return None
    out = {}
    for sec in p.sections():
        for k, v in p.items(sec, raw=True):
            out[f"{sec}.{k}"] = v if v is not None else ""
    return out


def ini_read_library(conform, text, tmp):
    """The library's reader's document as {flat key: text}, or None."""
    path = os.path.join(tmp, "doc.ini")
    with open(path, "w", encoding="utf-8", newline="") as f:
        f.write(text)
    return keys_of(conform, "ini", path)


def ini_configparser(conform, wrong_out, design_out):
    """(agreed, compared): the cases with no reason to differ on which the
    two read the same; the by-design ones that no longer differ, and the
    ones that differ, go to design_out and wrong_out by name."""
    import tempfile
    good = total = 0
    with tempfile.TemporaryDirectory() as tmp:
        for name, text, why in INI_CASES:
            want = ini_read_reference(text)
            got = ini_read_library(conform, text, tmp)
            if got is not None:
                got = {k: v[1] for k, v in got.items()}
            agree = want == got
            if why is not None:
                design_out.append((name, why, agree))
                continue
            total += 1
            if agree:
                good += 1
            else:
                wrong_out.append((name, want, got))
    return good, total


def floors(path):
    """The floor file: "<format> <valid> <invalid>" a line."""
    out = {}
    with open(path, encoding="utf-8") as f:
        for line in f:
            words = line.split("#", 1)[0].split()
            if len(words) == 3:
                out[words[0]] = (int(words[1]), int(words[2]))
            elif len(words) == 2:
                out[words[0]] = (int(words[1]),)
    return out


def main():
    # The CBOR vectors are written as files of bytes; the detection score
    # reads them after the report, so they go when the run is over.
    tmp = tempfile.mkdtemp()
    try:
        return score(tmp)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def score(tmp):
    args = sys.argv[1:]
    floor = {}
    verbose = "--list" in args
    if "--floor" in args:
        k = args.index("--floor")
        floor = floors(args[k + 1])
        del args[k:k + 2]
    args = [a for a in args if a != "--list"]
    # The name YAML's scores go by, for a second parser behind the codec
    # (D-62): verify scores libfyaml as yaml-libfyaml, against its floor.
    yaml_name = "yaml"
    if "--yaml-name" in args:
        k = args.index("--yaml-name")
        yaml_name = args[k + 1]
        del args[k:k + 2]
    # A codec from a shared library, scored on its format's suite alone and
    # against that format's floors: --codec <library> <symbol> <format>.
    only = None
    if "--codec" in args:
        k = args.index("--codec")
        EXTRA[:] = ["--codec", args[k + 1], args[k + 2]]
        only = args[k + 3]
        del args[k:k + 4]
    if args and args[0] == "--self":
        return differential(args[1], args[2])
    conform, root = args[0], args[1]
    out = []
    json_wrong, json_good, json_all = [], 0, 0
    json5_wrong, json5_good, json5_all = [], 0, 0
    values_wrong, values_good, values_all = [], 0, 0
    yaml_wrong, yaml_good, yaml_all = [], 0, 0
    cbor_wrong, cbor_good, cbor_all = [], 0, 0
    accepted = {}  # by report name: the format, and the valid documents it read
    v, i = json_suite(root) if only in (None, "json") else ([], [])
    if v or i:
        report("json", "json", conform, v, i, out, accepted)
        json_good, json_all = json_values(conform, v, json_wrong), len(v)
    v, i = json5_suite(root) if only in (None, "json5") else ([], [])
    if v or i:
        report("json5", "json5", conform, v, i, out, accepted)
        json5_good, json5_all = json5_values(conform, v, json5_wrong)
    v, i = toml_suite(root) if only in (None, "toml") else ([], [])
    if v or i:
        report("toml", "toml", conform, v, i, out, accepted)
        values_good, values_all = toml_values(conform, v, values_wrong), len(v)
    v, i = yaml_suite(root) if only in (None, "yaml") else ([], [])
    if v and run(conform, "yaml", v[0])[0] == 2:
        print("yaml: this build has no YAML reader (CF_WITH_YAML), skipped")
        v, i = [], []
    if v or i:
        report(yaml_name, "yaml", conform, v, i, out, accepted)
        yaml_good, yaml_all = yaml_values(conform, v, yaml_wrong)
    v, i, vectors = cbor_suite(root, tmp) if only in (None, "cbor") else ([], [], {})
    if v and run(conform, "cbor", v[0])[0] == 2:
        print("cbor: this build has no CBOR reader (CF_WITH_CBOR), skipped")
        v, i = [], []
    if v or i:
        report("cbor", "cbor", conform, v, i, out)  # detect-cbor is W4-C4's
        cbor_good, cbor_all = cbor_values(conform, v, vectors, cbor_wrong)
    failed = False
    # Content detection: each valid document, read with no name, is asked
    # which format it is, and counted when that is its own. A document the
    # strict formats share - valid JSON is valid JSON5 - is taken by the
    # first of them, and a lone value or a list is no configuration, so
    # these are not all 100%: the score is the floor's to keep (D-73).
    if not EXTRA:
        for name, (fmt, good) in accepted.items():
            n, total, missed = detect_score(conform, fmt, good)
            print(f"detect-{name}: read as its own format from content alone {n}/{total}")
            if verbose:
                for f in missed:
                    print(f"  not detected: {os.path.relpath(f, root)}")
            fl = floor.get("detect-" + name)
            if fl is not None and len(fl) == 2:
                if n < fl[0] or total < fl[1]:
                    print(f"  BELOW THE FLOOR {fl[0]}/{fl[1]}")
                    failed = True
                elif n > fl[0] or total > fl[1]:
                    print(f"  above the floor {fl[0]}/{fl[1]}: raise it in "
                          "tests/conformance/floor.txt")
    if only is None:
        ini_wrong, ini_design = [], []
        ini_good, ini_all = ini_configparser(conform, ini_wrong, ini_design)
        print(f"ini-configparser: read as Python's configparser reads {ini_good}/{ini_all}"
              f" ({len(ini_design)} more differ by design, listed in INI_CASES)")
        for name, want, got in ini_wrong:
            print(f"  read differently: {name}: configparser {want}, library {got}")
        for name, why, agree in ini_design:
            if agree:
                print(f"  BY-DESIGN CASE NOW AGREES: {name} ({why}); take it off the table")
                failed = True
            elif verbose:
                print(f"  by design: {name}: {why}")
        if "ini-configparser" in floor:
            fv = floor["ini-configparser"][0]
            if ini_good < fv or ini_all < floor["ini-configparser"][1]:
                print(f"  BELOW THE FLOOR {fv}/{floor['ini-configparser'][1]}")
                failed = True
            elif ini_good > fv:
                print(f"  above the floor {fv}: raise it in tests/conformance/floor.txt")
    for name, g, nv, b, ni, wv, wi, nm in out:
        extra = f" ({nm} more refused by the configuration model, by name)" if nm else ""
        print(f"{name}: valid accepted {g}/{nv}, invalid refused {b}/{ni}{extra}")
        if verbose:
            for f in wv:
                print(f"  refused a valid document: {os.path.basename(f)}")
            for f in wi:
                print(f"  accepted an invalid one:  {os.path.basename(f)}")
        if name in floor and len(floor[name]) == 2:
            fv, fi = floor[name]
            if g < fv or b < fi:
                print(f"  BELOW THE FLOOR {fv}/{fi}")
                failed = True
            elif g > fv or b > fi:
                print(f"  above the floor {fv}/{fi}: raise it in tests/conformance/floor.txt")
    # The values: not only that a valid document was accepted, but that it
    # was read as what it says - as the suite states it for TOML, and as
    # Python's json reads it for JSON.
    for name, good, total, wrong, how in (
            ("json-values", json_good, json_all, json_wrong, "Python's json reads"),
            ("json5-values", json5_good, json5_all, json5_wrong, "the json5 package reads"),
            ("toml-values", values_good, values_all, values_wrong, "toml-test states"),
            (yaml_name + "-values", yaml_good, yaml_all, yaml_wrong, "yaml-test-suite states"),
            ("cbor-values", cbor_good, cbor_all, cbor_wrong, "RFC 8949 Appendix A states")):
        if total == 0:
            continue
        print(f"{name}: read as {how} {good}/{total}")
        if verbose:
            for f in wrong:
                print(f"  read differently: {os.path.relpath(f, root)}")
        if name in floor:
            fv = floor[name][0]
            if good < fv:
                print(f"  BELOW THE FLOOR {fv}")
                failed = True
            elif good > fv:
                print(f"  above the floor {fv}: raise it in tests/conformance/floor.txt")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
