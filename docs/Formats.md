# Formats

Which standard each reader and writer follows, which version, how it is
held to it, and what it does not do - each refusal by name, never a file
read as something else. The scores are `docs/Conformance.md`'s, measured
by `verify -DEXTRA=conformance` against floors that only rise.

| Format | Standard, version | Read | Write | Held to | Not supported, and what happens |
| ------ | ----------------- | ---- | ----- | ------- | ------------------------------- |
| JSON | RFC 8259 (ECMA-404, 2nd edition) | built in | yes | JSONTestSuite: 95/95 valid, 188/188 invalid, 95/95 values | a root that is not an object, refused by name (a configuration is a table) |
| JSON5, JSONC | JSON5 1.0.0; JSONC as its subset | built in (D-66) | as JSON | json5-tests: 83/83, 31/31, 83/83 values | identifier characters past ASCII are any non-space character, not Unicode's letter classes; Infinity and NaN are text |
| TOML | TOML 1.1.0, which takes in 1.0.0 | built in | yes | toml-test 1.1.0: 218/218, 502/502, 218/218 values | dates and times, inf and nan are their text, typed by the contract (D-62) |
| YAML | YAML 1.2.2 | built in (D-69); libfyaml 0.9.1+ or libyaml by choice | yes, block style | yaml-test-suite: 308/308 events, 122/122, 94/94, 110/110 values | several documents, a top level that is not a mapping, a key that is not a scalar, an empty key, an application's tag - each refused by name (D-39, D-63); YAML 1.1's booleans (yes/no) are text |
| INI | none: PHP's `parse_ini_file` for arrays (D-60) | built in | yes | the unit tests, and Python's `configparser`: 36/36 documents read the same, 12 more differing by design (below) | interpolation, multi-line values, escapes; a value with a line break is not written, by name |
| Java `.properties` | what `java.util.Properties.load` reads, as UTF-8 | built in | no | the unit tests | writing (no quoting, so text would read back as a number) |
| XML | XML 1.0, through expat | `CF_WITH_XML` | no | the unit tests | DOCTYPE and entities, namespaces, mixed content, an attribute and a child of one name - refused by name (D-23) |
| `.env` | none: the common dotenv convention | built in | no | the unit tests | variable expansion: `${...}` is the library's own references (D-10) |
| SQLite store | SQLite 3.35+ | `CF_WITH_SQLITE` | yes (`cf_write`) | the unit tests | - |
| the image | the library's own (`cf_image.h`) | built in | yes | the unit tests, fuzzed | - |
| the provenance report | the library's own, its JSON Schema published | yes (Report Replay) | yes | the report's schema | - |

INI has no standard, so its reader is compared with the one most INI is
written for, Python's `configparser` (`RawConfigParser`: interpolation
off, keys keeping their case, a repeated key or section read with the
last one winning). `scripts/check/conformance.py` reads a set of small
documents - sections, dotted sections, keys, quoting, comments, blank
lines, empty values, duplicates, line endings, Unicode, refusals - with
both and compares the keys and their text, or that both refuse; the
score (`ini-configparser`) is in `tests/conformance/floor.txt` and
only rises. Where the two differ on purpose, the case is in the script's
`INI_CASES` with its reason, and the run fails if one of them starts to
agree, so the list stays true:

- quotes around a whole value are removed here, and kept by configparser;
- keys before any section are the root table's here, and refused by
  configparser (`MissingSectionHeaderError`);
- only `=` separates a key from its value here; configparser takes `:`
  too;
- no continuation lines here: an indented line is a line of its own and,
  with no `=`, is refused; configparser joins it to the value above;
- `name[]` and `name[i]` are an array's elements (D-60); configparser
  takes them as keys named so;
- a section's name is trimmed here, and text after its header's `]` is
  refused here, where configparser keeps the spaces and drops the text;
- `[DEFAULT]` is an ordinary section here, and configparser's values for
  every section;
- values are text, typed by the definition or by inference; configparser
  has `getint` and the like, and the comparison is of text.

A comment after a value is not one in either (configparser's
`inline_comment_prefixes` is off by default), and neither reads
interpolation: `%(a)s` and `${a}` are text in both reads of the file
(the library's `${...}` references belong to the configuration model,
D-10, not to the reader).

A format of one's own is a `cf_codec` (D-68), measured the same way:
`conformance.py --codec`.

## How the format is chosen (D-73)

Strategies in turn, the first that answers deciding; each says why
(`cf_source_file_format` reports it: named, extension or content).

1. The format the program names.
2. The name's extension, the builder's own codecs ahead of the built-in
   ones. It is taken as it stands. If it cannot read the file, the file is
   refused as before, and the error adds the format the content looks
   like: `(the content looks like yaml)` for a renamed file.
3. The content, when the name gives no format - no extension, or one no
   format claims. The first bytes only order the candidates (`{` or `[`
   JSON first, `<` XML, a `[section]` or `key =` line TOML and INI, a
   `key:` line YAML); reading decides. Each codec reads the bytes into a
   sink that keeps a hash of the events, and one that accepts them and
   yields at least one keyed value is a candidate. A strict format - the
   builder's own, JSON, JSON5, TOML, XML - is taken at once. A loose one -
   YAML, INI - is taken only when no other loose format reads the bytes
   into different events; otherwise the file is refused as ambiguous,
   naming both. `.properties` is never chosen from content.

An empty file, and text no format reads, is refused. `cf_codec_detect`
does the same for bytes of a program's own, and `cf_builder_document`
for a document held in memory.
