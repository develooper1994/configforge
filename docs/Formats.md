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
| INI | none: PHP's `parse_ini_file` for arrays (D-60) | built in | yes | the unit tests | interpolation, multi-line values, escapes; a value with a line break is not written, by name |
| Java `.properties` | what `java.util.Properties.load` reads, as UTF-8 | built in | no | the unit tests | writing (no quoting, so text would read back as a number) |
| XML | XML 1.0, through expat | `CF_WITH_XML` | no | the unit tests | DOCTYPE and entities, namespaces, mixed content, an attribute and a child of one name - refused by name (D-23) |
| `.env` | none: the common dotenv convention | built in | no | the unit tests | variable expansion: `${...}` is the library's own references (D-10) |
| SQLite store | SQLite 3.35+ | `CF_WITH_SQLITE` | yes (`cf_write`) | the unit tests | - |
| the image | the library's own (`cf_image.h`) | built in | yes | the unit tests, fuzzed | - |
| the provenance report | the library's own, its JSON Schema published | yes (Report Replay) | yes | the report's schema | - |

A format of one's own is a `cf_codec` (D-68), measured the same way:
`conformance.py --codec`.
