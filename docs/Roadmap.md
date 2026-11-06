# Roadmap

The state of the work, kept true in the same commit as the work - so that
whoever picks this up next, on whichever machine, knows what is next,
what is known to be wrong, and what has been thought of and not built.

Each item says **why** it matters and **what done looks like**, and
carries a mark for how far it has come. "At a glance" is the whole list
in one table; the sections after it are the detail.

## Status

### Marks

Every item carries one mark, which moves one way as the work does:

| Mark | Stage | Means |
| :--: | ----- | ----- |
| ⬜ | idea | thought of; not chosen yet |
| 🟦 | chosen | accepted as work, or decided (a D-number), nothing built |
| 🟨 | under way | partly in the tree; the item says what is left |
| 🟩 | done | on `main`, verified at tier 1, CI green |
| ✅ | released | in a published version (the CHANGELOG names it) |

An item marked ✅ moves to the CHANGELOG at the next release and leaves
this page; until then it stays here, marked, so what is finished is seen
beside what is not.

### At a glance

| Mark | Item | Where |
| :--: | ---- | ----- |
| ✅ | Ubuntu 26.04 for CI and the development machine | D-65, 1.2.0 |
| ✅ | 1.2.0 released | CHANGELOG |
| 🟩 | JSON5 and JSONC | D-66 |
| 🟩 | `doctor check --schema`, `doctor gen --schema` | D-67 |
| 🟩 | A format of one's own, on the builder | D-68 |
| 🟩 | The built-in YAML reader, on by default | D-69 |
| 🟩 | TOML 1.1.0 | Formats.md |
| 🟨 | Sizing: transient file bytes done; stream and dynamic modes for 2.0 | D-61 |
| 🟩 | Every C++ standard from C++98, cumulatively | D-72 |
| 🟩 | The Rust crate built by cargo and the `cc` crate | D-70 |
| 🟩 | `cf_diff_json`, the settle interval, limits in the report | CHANGELOG 1.3.0 |
| 🟩 | Signed release artifacts (attestations) | release.yml |
| 🟩 | Python examples; a table of format versions | examples/python, Formats.md |
| 🟦 | The format told from the content when the name says nothing | D-73 |
| 🟦 | One `define`, the type given or inferred | D-74 |
| 🟦 | 1.3.0 released | CHANGELOG |
| ⬜ | Doctor checks a report's fingerprint against its contents | plan item 9 |
| ⬜ | Continuous fuzzing in CI (ClusterFuzzLite) | plan item 9 |
| ⬜ | INI and `.properties` compared with `configparser` and `Properties` | plan item 9 |
| ⬜ | Native OS sources | plan item 9 |
| ⬜ | Go and .NET bindings, when asked | plan item 9 |
| ⬜ | Binary configuration: a layout-driven TLV codec, BER-TLV, CBOR | assessment |
| ⬜ | Every operation on a file's data, written back in its format | assessment |
| ⬜ | Converting between formats | assessment |
| ⬜ | Code for each binding from a contract | assessment |
| ⬜ | 2.0: what the sizing modes break, once | plan item 10 |

## Release follow-up

1.0.0 was published with three known round-trip defects, before the
agreed work was complete. Their corrections are in the unreleased
CHANGELOG section: long JSON/TOML values, literal references on emit,
and refusing a value/container key collision (D-57). The existing
1.0.0 archives still have those defects.

The report follow-up also found that sensitive defaults appeared in
blind reports' definition metadata, and an unsupported report mode
could expose values under a `blind` label. The unreleased fixes cover
both whole reports and single-key JSON. Reports made with an affected
build must not be assumed value-blind merely because of their label.
The existing 0.2.0, 0.3.0, 0.4.1 and 1.0.0 release notes now warn
about this; their tags and archives have not been replaced.

A deferred item remains open. Passing the release matrix does not
complete an unimplemented feature, and publishing a version does not
complete this roadmap. No further release is planned until the agreed
work and its validation are complete.

## Next: 1.x

1.0.0 is released: from here the C interface follows semantic versioning
(D-56), a patch fixes, a minor version adds, and `verify -DEXTRA=abi`
fails an incompatible change within 1.x.

### Resume here (2026-11-05)

1.1.0 is released: the release workflow published it, with the macOS
archive, when the tag was pushed - GitHub Actions runs again, so a
pushed `v*` tag *is* a release; tier 2 and `scripts/release/release_local.ps1` come
before it. The work from here is "The plan from 1.1.0" below, item 1
first.

Before that, on 2026-11-02: Doctor
history/blame (`a183b3e`), the MSVC sanitizer runtime fix (`7d2530d`),
default drift (CFD017, `5917bd5`), and read coverage in C (`fb383c4`)
and the bindings plus Doctor CFD018 (`ec24578`) are built. Their full
Linux and Windows verification passed. The Python binding tests also
passed against an older shared library without the read-tracking
symbols: existing calls remain usable, while requesting read tracking
fails explicitly (`ac26e6c`).

The organization pass is complete. Project CMake modules live in
`cmake/`, gates in `cmake/gates/`, and toolchains in `cmake/toolchains/`;
per-directory `CMakeLists.txt` files own their subtrees. Each generated
artifact has a consumer: build options, commit identity, and the SBOM
are build-specific; the API reference links declarations to source; and
the amalgamation is the tested no-build-system distribution. No
unnecessary generation step was found. The core stays C89: the public
header and compiler gates enforce it, and no C99 feature was needed to
complete the agreed work.

Before anything: read `CONTRIBUTING.md`, then run
`sh scripts/buildenv/dev-setup.sh --check` (Windows:
`pwsh scripts/buildenv/dev-setup.ps1 -Check`) and, if a tool is missing,
the same without the check.

The three items under "Accepted follow-up" are done. Four more were
accepted on 2026-11-05, after 1.1.0 (below them). The ideas further
down are not approved scope; if selected, each addition is a minor
version.

### Accepted follow-up

The agreed additions and organization review are complete; no release
is a substitute for the final review and verification.

Chosen on 2026-11-05, in this order. Each is a minor
version, so main's unreleased "1.0.1" - which already adds features - is
1.1.0 when it is released. Each item is done when its tests fail without
it, `verify` passes at tier 1 on Linux and Windows, and the CHANGELOG,
Guide and API reference say what it does.

1. **A reload on probation, rolled back by itself** - done. Probation is
   counted in polls, not time: the library has no clock (C89), and the
   program's poll interval is the unit. `cf_live_rollback` (the name
   `cf_live_reject` was taken by APPROVE) and `cf_live_confirm`, a
   `health` function and `probation` in `cf_live_options`,
   `CF_LIVE_ROLLED_BACK`, `rollbacks` and `probation` in the status;
   a poll does not swap back in the values a rollback came from. Guide
   section 10, scene 4 of the live examples, `test_live`.
2. **Doctor on a pull request** - done. The action takes `base` and
   diffs, writes Markdown (`doctor --format markdown`) to the job
   summary and, with `comment: true`, to one comment updated per push;
   `.pre-commit-hooks.yaml` and `actions/doctor/pre-commit.sh` lint
   staged reports. Non-secret values do show in a diff, as `doctor diff`
   shows them; secrets only by digest. `doctor.action` tests the
   scripts, the comment against a stub `gh`; the CI job runs the action
   when GitHub Actions does.
3. **The configuration's fingerprint where operators look** - done.
   `cf_fingerprint` is separate from `cf_digest`, which hashes secrets
   raw and stays internal to quiet swaps; a secret enters the
   fingerprint as its blind-report digest (`cf_ks_value_digest`, which
   the report now uses too). In the report, `cf_live`'s log events
   (`previous`, `fingerprint`), Doctor's `dump` and Markdown, the
   bindings, and the Guide's log-field and OpenTelemetry recipe.
   Found on the way: Rust's `Debug` printed `cf_digest`.

## The plan from 2026-11-06 (accepted; the order to follow)

Items 1 to 7, 7d and 6 of the plan below are done. What is left, in this
order - each stage one or more verified commits, the CI run read after
each push:

1. ✅ **CI and the development environment on Ubuntu 26.04 LTS, together.**
   First a survey: every package `setup.sh` installs, looked for in an
   `ubuntu:26.04` container, before anything changes. Then `setup.sh`,
   the Containerfile and `wsl-distro.ps1` move to 26.04 (libfyaml comes
   from apt there, 0.9.4, and the version floor stays); a new WSL
   distribution is made beside the old one and passes tier 2; CI's jobs
   name `ubuntu-26.04` (`ubuntu-latest` still means 24.04), and one
   compatibility job stays on 24.04. The old distribution is removed once
   the new one is green. Done: 1.2.0 was built and tested there.
2. ✅ **1.2.0**, after tier 2 on the new environment: the version, the
   CHANGELOG's date, the tag (which publishes it).
3. 🟩 **7c JSON5 and JSONC** - done (D-66).
4. 🟩 Done (D-67): **`doctor check --schema app.schema.json app.toml`** - a file checked
   against a JSON Schema contract without writing C, in CI and in the
   pre-commit hook - and **`doctor gen --schema`**, the `cf_builder_*`
   calls generated from a schema, so a build without the JSON reader
   (embedded) has the same contract from the one source.
5. 🟩 Done (D-68), registered on the builder rather than in a table for
   the whole process: **A format of one's own, all the way**: `cf_codec_register`, and
   `conform` loading a codec from a shared library to score it.
6. 🟩 **7b, the built-in YAML reader** - done (D-69).
7. 🟨 **8, sizing modes, with dump and stream** - D-61 decided, the
   transient dump done; the stream and the dynamic modes are 2.0's. (below): D-61 written and
   discussed first; then the transient dump; then the stream.
8. 🟩 **9, modern C++** - done (D-72, replacing D-71): every standard from
   C++98, cumulatively, with `CF_CXX_LEVEL`.
9. 🟨 **10, the ideas left**, in this order: Doctor checks a report's
   fingerprint against its contents; limits and the build id in the
   report and the image header; CFD012 against the contract; `cf_diff_json`;
   a settle interval in `cf_live`; continuous fuzzing in CI
   (ClusterFuzzLite); INI and `.properties` compared value by value with
   Python's `configparser` and Java's `Properties`; signed release
   artifacts (GitHub attestations); native OS sources where the platform
   is at hand. Go and .NET bindings last, and only when someone asks.
   Done: `cf_diff_json`, the settle interval, the limits and the build id
   in the report (the image header keeps its layout until 2.0), signed
   release artifacts.
10. ⬜ **11, 2.0**: what 8 and 9 break, together, once.
11. 🟨 **Then** new ideas while any is useful, and the clean-up of the
    development machine's scratch.

**After the plan, an assessment** (2026-11-06), once item 11 is reached:

* The project and how it is worked on, held to the standards each part
  has (the languages', the build's, the packaging's, the forge's).
* Every format read to its latest standard where that is possible, and
  a table of which version each reader follows, what it does not support
  and how it says so.
* The Rust crate and cargo: the open problem with them settled.
* The examples brought up to date, and new ones where a feature has
  none; each binding with an example of its own, Python's included.
* Optimisations recorded, the ones that paid and the ones that did not,
  with their numbers, in Maintaining.md's "Measured", so the next one
  starts from them.
* Binary configuration (2026-11-06). TLV has no one standard: BER-TLV
  (ASN.1 BER, ISO/IEC 7816-4, EMV) has one, with its tag classes and
  long lengths; the rest are layouts, as a device's `.cfg` seen here -
  a two-byte little-endian tag and length, values that are text, numbers
  or fixed records, and an `FF FF` end. So: a `tlv` codec driven by a
  layout (tag and length widths, byte order, BER, an end tag, which tags
  nest) and a map from tag to key and type, every value placed by its
  byte offset for explain(); and CBOR (RFC 8949, with its test vectors),
  the JSON model in binary. MessagePack only if asked; BSON and protobuf
  not (a database's, and schema-bound).
* Every operation on a file's data, through its codec: read, add,
  change and remove keys in a document and write it back in its own
  format (cf_edit and cf_write do it for a configuration and its stores
  today), keeping what can be kept of the document's layout.
* Converting between formats: read a file through its codec, write it
  through another's (`cf_emit`), saying what the target cannot hold
  (comments, nulls, arrays in INI) rather than dropping it quietly.
* Code for each binding from a contract: `doctor gen --schema` writes C
  now; C++ (`Key`s and a `Binding`), Rust (`Def`s and a struct) and
  Python (a dataclass) from the same schema, and a schema drafted from a
  sample file for whoever has no schema yet.

**Dump and stream** (2026-11-06). Today a file is read whole
into the configuration's own arena, and the key space copies every value
it keeps, so the file's bytes stay as long as the configuration though
nothing reads them again. Three modes: *dump* (today: file and values,
at the peak and after); *transient dump* (the file in an arena of its
own, freed when the reader is done: the steady footprint loses the file;
the default wherever there is a heap); *stream* (the reader fed in
chunks of a few kilobytes through an input callback, with a look-back
window: the peak no longer grows with the file - what a device without
room for the whole file needs). INI, `.properties`, `.env` and JSON read
in one pass already; TOML measures a string and reads it again, and
changes most. Places in `explain()` stay exact in every mode. Chosen per
builder at run time, with a default per profile, as D-61 decides for
the sizing modes; `bench/` measures the peak and the steady footprint
before and after.

## The plan from 1.1.0 (2026-11-05)

1.1.0 is released (the release workflow published it, macOS included).
Everything below is accepted, in this order; each is one or more
verified commits, and a release is cut where the order says so. The
1.x items are additive: 1.2.0. What breaks source or ABI waits for 2.0.

1. **Small fixes.** `verify -DEXTRA=abi` takes the last release before
   HEAD, not a tag on HEAD itself (it compared 1.1.0 with itself before
   the tag was pushed). `setup.sh` installs Rust with rustup, current
   stable, since Ubuntu's 1.75 cannot read the tree's `Cargo.lock`
   (version 4). `configforge.hpp` joins `gate.format`, written so both
   clang-formats agree. A pre-stage script: format, the API reference
   and the analyze build of what changed, in minutes, before a verify.
2. **The compiler's help where it is missing.** MSVC's analyzer
   (`/analyze`) - done, on every C target, tests included, in the
   Windows quick run; `CF_GC_SECTIONS` - done, on in `minimal`, -33%;
   `CF_LTO` - done, off by default, measured in Maintaining.md. Was: `minimal` with
   `-ffunction-sections -fdata-sections` and `--gc-sections` (`/Gy`,
   `/OPT:REF`), the size before and after written down; `CF_LTO`, off by
   default, measured on the release build, the stack budget measured on
   its result.
3. **A benchmark, not a gate** - done (`bench/`). It showed Doctor's
   lookup, as expected, and one nobody expected: defining keys was
   quadratic. Both are fixed and held by tests; the numbers are in
   Maintaining.md, "Measured".
4. **Arrays in INI** - done (D-60). C-style arrays were wanted first,
   then each format's own standard; INI's arrays, where it has any, are
   PHP's `name[] = v` and `name[i] = v`, and the second is C's element
   assignment, so both hold. A brace list was built first and dropped:
   it was a syntax invented here.
   4b. **A declared list splits an INI value too** - done (2026-11-06:
   should `ports=[80,443, 8443 ,22]` work?). Not as syntax: no INI reader
   knows brackets - it is TOML's, which this library reads - and old
   files with such text would change meaning. But INI carries no types,
   so the definition decides: a list declared with `list_separator`
   splits `ports = 80,443,8443,22` from an INI file as it does from the
   environment and `.env` today.
5. **Doctor `--only-changed`** - done: `lint --base B --only-changed`,
   in the library, the action, the hook and Python; findings about a
   source are kept when a touched key came from it.
6. **Done (2026-11-06): each reader held to its format's own
   standard**, starting from the best-known methods and improving on
   them. The method, for every
   reader: the specification's grammar, its official test suite as the
   bar (JSONTestSuite, toml-test, yaml-test-suite, pinned by commit),
   a differential test against the reference implementation, fuzzing.
   Measured first: JSON valid 95/95 and invalid 188/188 after its number
   grammar was made RFC 8259's (it accepted 01, 2. and .5); TOML 1.0 valid
   161/208, invalid 394/509 - no multi-line strings, datetimes, inf or
   nan, and lenient numbers, keys and table redefinitions. Now TOML is
   valid 208/208, invalid 509/509, values 208/208, and JSON's values
   95/95 against Python's json. YAML through libfyaml 0.9.1 or later
   (`CF_YAML_BACKEND`, D-62): valid 122/122, invalid 94/94, values
   110/110; through libyaml, which stays for MSVC and C89-only builds,
   100/133, 87/94 and 90/93. libfyaml 0.8 hangs on malformed input and is
   refused by the build; two defects in libfyaml's pre-C11 header were
   reported upstream (libfyaml issue 506). Done. tomlc17
   conforms fully but builds a tree without positions, and explain()
   needs file:line:col, so the built-in TOML reader is completed rather
   than replaced. Then YAML, and `verify -DEXTRA=conformance`. Record:
   D-62. The differential test starts from the project's own
   configuration - the project configures itself, hence the name:
   `.clang-format`, `.clang-tidy`, `mkdocs.yml` and
   `.pre-commit-hooks.yaml` (YAML), `CMakePresets.json` (JSON) and
   `pyproject.toml` (TOML), each read by its reader and compared, key by
   key and value by value, with the reference reader's (Python's json and
   tomllib, PyYAML where installed).
   Below, the item as first written:
6. **Dependencies behind one contract**: every
   reader a library supplies - JSON, TOML, YAML, XML, SQLite - sits
   behind the `cf_codec` / `cf_doc_sink` contract, and a conformance
   suite every implementation must pass (the same documents, the same
   events, the same refusals, the same limits), so a dependency can be
   replaced without a caller noticing. Then the best maintained library
   is chosen for each, by a written comparison (licence, maintenance,
   conformance, size, speed, fuzzing history): yyjson and tomlc17 are
   the candidates for JSON and TOML; libyaml, expat and SQLite are
   re-examined. The built-in readers stay, behind the same contract.
7. **Done: JSON Schema import** (D-64): definitions from a schema, the reverse of
   `cf_schema`; unsupported keywords refused by name. After 6, on the
   chosen JSON reader.
7b. **Done (D-69): a built-in YAML reader** (2026-11-06: why should YAML alone need
   a dependency?). INI, JSON and TOML are read here, streaming,
   allocation-free, every value with its place; YAML is read through
   libfyaml or libyaml, so it is off by default, cannot run in a fixed
   arena, and its positions come from someone else's events. The reader
   for the YAML configuration is written in: block and flow mappings and
   sequences, plain, quoted and block scalars with their chomping and
   indentation indicators, anchors and aliases, the core schema's tags,
   comments, directives. Held to yaml-test-suite with the same floors,
   libfyaml the differential reference rather than the parser, fuzzed;
   then on by default as TOML is, with libfyaml and libyaml kept behind
   `CF_YAML_BACKEND` for whoever wants a dependency's reading.
7c. **Done (D-66): JSON5, and with it JSONC** (2026-11-06: which other formats are
   worth reading for configuration, without overdoing it). JSON with
   comments and trailing commas is what `tsconfig.json`, VS Code's
   settings and Deno's configuration are written in, and a JSONC file is
   JSON5. JSON5 (json5.org) has a specification and an official suite
   (`json5-tests`), so it is held to it like the others; it is a mode of
   the JSON reader, the grammar shared, not a second reader. `.json5`
   and `.jsonc` choose it. Weighed and not taken: HCL (a language, with
   expressions), HOCON (the JVM's), plist (under native OS sources),
   KDL, RON, CUE, Dhall, Pkl (young or narrow). After 7c the format set
   is enough.
7d. **Done: the tree in order** (2026-11-06: the files looked
   scattered). `scripts/` keeps its entry points (`verify.cmake`,
   `prestage.cmake`, `format.cmake`) and puts the rest by purpose under
   `check/`, `generate/`, `release/` and `docs/`, one naming style;
   `cmake/` puts its templates under `templates/`; an in-source build,
   which left a `CMakeFiles/` at the root, is refused by the build.
8. **Sizing modes** (2026-11-05: full-dynamic, semi-dynamic, manual): today every limit is a fixed buffer (`CF_KEY_MAX` and its
   siblings, D-37) - manual. Semi-dynamic: buffers start on the stack
   and grow from the allocator up to a cap. Full-dynamic: no length or
   depth limit but memory, decoders iterating rather than recursing,
   with run-time caps still there for untrusted input. A decision record
   (D-61) first: build option or run time, the default per profile, and
   what `cf_error`'s fixed `key` and `message` do - truncate in 1.x,
   grow in 2.0. Then the buffers, one subsystem at a time, each under
   all three modes in the suite. The largest item here.
   With it, dump and stream (2026-11-06): today a file is
   read whole (`cf_blob_file`) into the configuration's own arena, the
   readers stream events from those bytes, and the key space copies
   every value it keeps - so the file's bytes stay, unused, as long as
   the configuration does. First, a transient dump: the bytes in an
   arena of their own, freed when the reader is done (a fixed arena
   keeps today's way), which takes the file off the steady footprint.
   Then a stream: readers fed in chunks through an input callback rather
   than a whole blob, so the peak does not grow with the file; the
   readers that look back (TOML measures a string and reads it again,
   INI looks back at an earlier array run) are reworked one at a time.
9. **Modern C++ only**: the header deprecates
   C++98 to C++14 in 1.x with a compile-time warning, and requires
   C++17 from 2.0, where `std::optional`, `std::string_view` and
   structured bindings simplify it. C++20 modules (`import configforge;`)
   come with 2.0 where the compilers in the matrix build them.
10. **The ideas, by use** - each its own commit, in this order:
    * A format of one's own, all the way (2026-11-06): today
      a caller's `cf_codec` is read through `cf_builder_file_codec` and
      nothing else knows it. `cf_codec_register` puts it in the table,
      so its extensions choose it, `cf_codec_by_name` finds it and
      `cf_emit` writes it; and `conform` loads a codec from a shared
      library, so its author scores it against its standard's suite
      with the same floors.
    * Doctor checks a report's fingerprint against its contents.
    * `cf_diff_json` for log pipelines.
    * a settle interval in `cf_live` for a burst of writes.
    * limits and the build id in the report and the image header -
      with item 8, since a sizing mode is a limit a reader must know.
    * CFD012 against the contract rather than the key list.
    * native OS sources (macOS defaults, gsettings) where the platform
      is at hand to test.
    * Go and .NET bindings over the C ABI.
11. **2.0**: what 8 and 9 break, together, once.

## Ideas

Every idea written here before 2026-11-05 is now in the plan above, in
the order of its use. New ones go here until they are
accepted.

### Considered, and decided against for now: TOON

TOON, Token-Oriented Object Notation (github.com/toon-format/toon, spec
v4.4, MIT), was weighed on 2026-11-06. Its own description answers it:
a compact encoding of the JSON data model, produced by programs from
JSON, suited to uniform arrays of objects and advised against for
deeply nested data, which is what configuration is. The reference is
TypeScript, there is no C implementation, and the spec calls itself an
idea in progress. As a configuration source it would be a reader to
keep for files nobody writes by hand; anyone who wants a report in it
can convert the report's JSON with the TOON CLI already.

Revisit if configuration files in TOON appear in the wild.

### Considered, and decided against for now: a jq-like query language

jq's lesson is one data model and small tools that compose over it. The
provenance report is already that model - JSON, with a published schema
- so jq itself answers questions about a configuration today, with no
code here; Doctor.md, section 7a, has the recipes, each run against a
real report. Writing a second query language inside the library would
be a parser to maintain for a question jq already answers, and a reason
to read *two* documents to use one tool. Keys are flat dotted paths, so
`upstream.1.host` already is the path syntax a selector would offer.

Revisit if there is a question jq cannot answer from a report - one
that needs the live configuration rather than its record.

## Open decisions

None. The last one, arrays in INI, was decided on 2026-11-05: C style;
the plan above, item 4.
