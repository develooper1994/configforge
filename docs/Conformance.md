# Conformance

How closely each reader follows its format's own standard, measured with
that standard's own test suite rather than claimed. A configuration
library that reads a file its format calls invalid teaches the file's
author a dialect; one that refuses a valid file sends them elsewhere.
Both are measured here.

## The method

For every format, in this order:

1. **The specification's grammar** is what the reader implements: RFC
   8259 for JSON, the TOML 1.0.0 ABNF, the INI conventions of D-60.
2. **The standard's official suite** is the bar. Each document the suite
   calls valid must be accepted, and each it calls invalid refused.
3. **A ratchet**, `tests/conformance/floor.txt`: the scores below are
   floors, a change that lowers one fails `EXTRA=conformance`, and the
   change that raises one raises its floor.
4. **A differential test** against a reference reader where one exists,
   and **fuzzing** (`tests/fuzz`), for what the suites do not cover.

A refusal the library makes on purpose, a construct its configuration
model has no place for, counts against it here like any other: the
score is the standard's, and the reasons are listed under each format.

## Running it

```
cmake -DEXTRA=conformance -P scripts/verify.cmake
python3 scripts/check/conformance.py <conform> <suites> --list
```

The verify step fetches the suites at the commits below, builds
`tests/conformance/conform` (one document through one reader: exit 0
accepted, 1 refused, 2 not tried) and runs `scripts/check/conformance.py`
against the floor. `--list` names every document that went the wrong
way. It is a tier 2 step, since it needs a network the first time.

Each valid document the reader accepts is also asked which format its
content is, with no name to go by - as if renamed without an extension
(`conform --detect`, D-73) - and counted when the answer is its own format:
`detect-<format> <n> <total>` in the floor file. Lone values and lists, which
are no configuration, are not detected, and a JSON5 file that is also JSON
is taken as JSON, so JSON and JSON5 score low by design; TOML and YAML are
high, and the documents they miss are named by `--list`.

| Suite | Standard | Commit |
| ----- | -------- | ------ |
| [JSONTestSuite](https://github.com/nst/JSONTestSuite) | RFC 8259 | `1ef36fa01286573e846ac449e8683f8833c5b26a` |
| [toml-test](https://github.com/toml-lang/toml-test) | TOML 1.1.0 (`tests/files-toml-1.1.0`), which takes in 1.0.0 | `96fd088448ec0f4d9f061c730ed7854399c46faf` |
| [yaml-test-suite](https://github.com/yaml/yaml-test-suite), its `data` branch | YAML 1.2 | `6ad3d2c62885d82fc349026c136ef560838fdf3d` |
| [json5-tests](https://github.com/json5/json5-tests) | JSON5 1.0.0 | `ceb24d4080137d70833f86c25659c1331b80a387` |
| [cbor-test-vectors](https://github.com/cbor/test-vectors), its `appendix_a.json` | RFC 8949 Appendix A (and F.1's malformed items, typed into the script) | `aba89b653e484bc8573c22f3ff35641d79dfd8c1` |

## The scores

| Format | Valid accepted | Invalid refused | Valid read to the right values |
| ------ | -------------- | --------------- | ------------------------------ |
| JSON | 95 / 95 | 188 / 188 | 95 / 95, against Python's `json` |
| JSON5 and JSONC | 83 / 83 | 31 / 31 | 83 / 83, against the `json5` package 0.17.3 |
| TOML 1.1.0 | 218 / 218 | 502 / 502 | 218 / 218, against toml-test's stated values |
| CBOR (D-91) | 68 / 68 | 94 / 94 | 55 / 55, against the vectors' stated values |
| YAML, the built-in parser (D-69) | 122 / 122 | 94 / 94 | 110 / 110, against the suite's `in.json` |
| YAML, through libfyaml 0.9.6 | 122 / 122 | 94 / 94 | 110 / 110, against the suite's `in.json` |
| YAML, through libyaml 0.2.5 | 100 / 133 | 87 / 94 | 90 / 93, against the suite's `in.json` |

INI has no standard and no suite. Its reader is compared with Python's
`configparser` on 36 small documents, 36 of 36 read the same, and 12
more differ on purpose, each in a table in `scripts/check/conformance.py`
with its reason (docs/Formats.md names them).

YAML's valid count leaves out the documents the configuration model
refuses by name - a top level that is not a mapping, a second document,
a key that is not a scalar, an empty key, a tag of the writing
application's (D-39, D-63) - 186 of them through the built-in parser
and libfyaml, 175 through libyaml, which refuses some of those for its
parser's reasons first. The
run reports them beside the score. A plain scalar is untyped here, typed
by its definition, and is compared as the text of the suite's value.

Three parsers sit behind one codec (`CF_YAML_BACKEND`, D-62, D-69). The
built-in one, the default, is complete, and was held to the suite's own
event streams (`test.event`) before the library's values: all 308 valid
cases event for event, all 94 invalid ones refused. Through libfyaml,
from 0.9.1, the reader is complete too; every miss through libyaml is
its parser's, which follows YAML 1.1. libfyaml before 0.9.1 hangs on
malformed input and is refused by the build.

The last column is the stronger claim: a valid document read is compared
key by key and value by value, every string's escapes and line breaks
included, with what the suite says it holds (TOML) or with a reference
reader (JSON, whose suite states no values).

JSON's root may be any value, and a configuration is a table: `conform`
reads a document whose root is not an object as the member of one,
`{"v": ...}`, which keeps an invalid document invalid. The `i_` cases,
which the standard leaves to the implementation, are not counted.
JSON5's root is wrapped the same way, always, with the closing brace on
a line of its own, since a comment may come before the root or end the
last line. json5-tests calls its `.json` and `.json5` files valid, its
`.js` files (ECMAScript, not JSON5) and `.txt` files invalid; it states
no values, so the `json5` package is the reference, where it is
installed (`setup.sh` pins it). JSON5's Infinity and NaN are their text,
as TOML's `inf` and `nan` are (D-66).

CBOR's vectors are mostly a lone value and a configuration is a map, so
`conform` reads each as the value of the member `v` - the three bytes
`A1 61 76` before it - which keeps a malformed item malformed: the map's
value is cut short, or holds a stray break. Appendix A's vectors are the
repository's `appendix_a.json`, each file's bytes taken from its `cbor`
field; the malformed items are Appendix F.1's, 94 of them, typed into
`scripts/check/conformance.py` as `INVALID_CBOR` with F.1's headings.
The valid count leaves out the 13 vectors the reader refuses by name for
the configuration model's sake, listed by the run: a byte string (three
vectors), the bignum tags 2 and 3, tags 23 and 24, an integer past 64 bits
(two), `undefined`, `simple(16)` and `simple(255)`, and a map whose key is
an integer. The repository follows RFC 7049's Appendix A, and one of
its vectors, `simple(24)` as `f8 18`, is malformed in RFC 8949 (F.1; G.1
records the example's removal): it is scored among the malformed items,
not the valid ones. The values column compares the 55 vectors that have
a `decoded` value and were read with that value. Detection by content
(`detect-cbor`) takes 4 of the 68, every vector that is a map with a
member: the rest are lone values and lists, which are no configuration.

TOML's dates and times, and `inf` and `nan`, are read as their text,
checked against their grammar (D-62): the library has no date type and
no value that is not finite, so a definition decides what they are. The
value columns compare them as text, with toml-test's normalisation (`T`
for the delimiter, `Z` for UTC, milliseconds). The define-once rules
need the workspace a sink lends (`cf_doc_sink.reserve_work`); the loader
always lends it.

## A codec of one's own

A codec written outside the library is measured the same way: built into
a shared library that exports a pointer to it, it is loaded by
`conform <format> <file> --codec <library> <symbol>` and scored by
`conformance.py <conform> <suites> --codec <library> <symbol> <format>`
on that format's suite alone, against that format's floors (D-68). In a
shared build, `tests/conformance/codec_module.c` is the example the tests
load: the unit tests' "key: value" codec, and one that reads through
this library's JSON reader.

## The project's own configuration

`scripts/check/conformance.py --self <conform> <repository>`, the last part of
the verify step, reads the files that configure this project - the
build's presets, the Python package, the formatter and linter settings,
the documentation site, the pre-commit hook - each with its own reader,
and compares every key and value with the reference reader's: Python's
`json` and `tomllib`, and PyYAML where it is installed. A file whose
reader is not in the build, or whose reference is not installed, is
named and skipped.
