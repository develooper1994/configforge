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
python3 scripts/conformance.py <conform> <suites> --list
```

The verify step fetches the suites at the commits below, builds
`tests/conformance/conform` (one document through one reader: exit 0
accepted, 1 refused, 2 not tried) and runs `scripts/conformance.py`
against the floor. `--list` names every document that went the wrong
way. It is a tier 2 step, since it needs a network the first time.

| Suite | Standard | Commit |
| ----- | -------- | ------ |
| [JSONTestSuite](https://github.com/nst/JSONTestSuite) | RFC 8259 | `1ef36fa01286573e846ac449e8683f8833c5b26a` |
| [toml-test](https://github.com/toml-lang/toml-test) | TOML 1.0.0 (`tests/files-toml-1.0.0`) | `96fd088448ec0f4d9f061c730ed7854399c46faf` |

## The scores

| Format | Valid accepted | Invalid refused |
| ------ | -------------- | --------------- |
| JSON | 95 / 95 | 188 / 188 |
| TOML | 164 / 208 | 509 / 509 |

JSON's root may be any value, and a configuration is a table: `conform`
reads a document whose root is not an object as the member of one,
`{"v": ...}`, which keeps an invalid document invalid. The `i_` cases,
which the standard leaves to the implementation, are not counted.

Every invalid TOML document is refused: the number grammar, the
define-once rules (D-62), control characters, bare carriage returns and
malformed UTF-8. What is left is valid documents refused - multi-line
strings, dates and times, `inf` and `nan` - which Roadmap item 6 closes
next. The define-once rules need the workspace a sink lends
(`cf_doc_sink.reserve_work`); the loader always lends it.

## The project's own configuration

`scripts/conformance.py --self <conform> <repository>`, the last part of
the verify step, reads the files that configure this project - the
build's presets, the Python package, the formatter and linter settings,
the documentation site, the pre-commit hook - each with its own reader,
and compares every key and value with the reference reader's: Python's
`json` and `tomllib`, and PyYAML where it is installed. A file whose
reader is not in the build, or whose reference is not installed, is
named and skipped.
