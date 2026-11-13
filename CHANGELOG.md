# Changelog

Versions follow semantic versioning from 1.0. Before that, `0.x` promises
nothing: the C interface is not frozen, and a minor bump may change it.

## 1.4.1 — unreleased

### Security

- **What a document may make is bounded**, for every reader at once
  (D-82): at most 64 times its size and 1 MiB of keys and values, or it
  is refused by name. A YAML document's aliases were bounded; a JSON,
  TOML, INI, XML or `.properties` document, and a program's own codec,
  were not. The fuzzers fail any input that costs more than four times
  that.

- **A new SQLite store file is its owner's alone**: mode 0600 on POSIX
  whatever the umask, an access control list for its owner only on
  Windows, where it was created with the default permissions though it
  may hold sensitive values. An existing store keeps the mode it has.
  `cf_platform_create_private` is the capability, and
  `cf_platform_has_private_create` says whether a platform has it.
- **Doctor printed a sensitive key's value from a full report.** With
  full (not blind) reports, `doctor diff` wrote a sensitive key's two
  values in text, in SARIF and in the Markdown the GitHub Action posts
  as a pull-request comment; CFD017, CFD006, `dump` and `explain` did
  too, and a CFD006 refusal in the live-reload gate, which always lints
  a full report, put the value into the error `cf_live` logs. The
  documents promised digests only, and that held only for blind reports.
  Every finding and listing now goes by the key's sensitive flag.
  **Published versions up to 1.4.0 are affected**: a full report's diff
  posted to a pull request by one of them should be treated as having
  shown the secret.
- **A `CF_SENSITIVE` value is no longer quoted in a reason.** A value
  with a stray `${`, or one naming a key with no value, was refused with
  up to 40 bytes of it in the reason, which reached `cf_error`,
  `explain()` and a blind report's `reason`; the reason now quotes
  nothing. A sensitive list's length was printed in a shape error, and
  no longer is.
- **A deprecated name takes `CF_SENSITIVE` from the key that replaced
  it**, through chains of renames. Before, a secret written under the old
  name showed in clear in `explain()`, in a blind report and in a diff,
  and `${old.name}` from a plain key was accepted.
- **Doctor's text output escapes control characters**, so a value in a
  report cannot start a GitHub workflow command in the Actions log, and
  its Markdown no longer forms @mentions or links from values. `dump`
  and `explain` escape every field they print.
- **A secret's refusal says its kind, and nothing of the value**: wrong
  type, out of range, not one of the permitted values, too short, too
  long, a malformed reference, a reference to a key with no value - a
  fixed wording, without the value, its length or its bounds, in
  `cf_error`, `explain()`, the log, a report's `reason` and Doctor's
  output. A report names the kind as `reason_kind` (an added field);
  Doctor shows a secret's reason only where it is there, and "invalid"
  for a report from 1.4.0 or before, which can quote the value in it.
  `cf_refusal`, `cf_refusal_name` and `cf_refusal_text` are new.
- **The action takes for Doctor's only a comment written by its own
  account** (`github-actions[bot]`, or the new `comment-author` input).
  The marker alone let anyone's comment be taken for it, so Doctor's
  findings were never posted. The CI workflow's token is read-only,
  which closes the 20 CodeQL "missing workflow permissions" alerts, and
  the release workflow's build jobs no longer hold `contents: write`.
  Every action the workflows use is pinned to a commit, with its tag in
  a comment for Dependabot to move: a tag can be pointed elsewhere, and
  two of them run with a token that writes.
- A `${}` expansion is bounded by `CF_EXPAND_MAX` (1 MiB, `cf_define.h`,
  D-77): two dozen doubling lines could make gigabytes, and on a 32-bit
  `size_t` the measured size could wrap and the copy overrun the heap.
  All the expansions of one configuration together are bounded by
  `CF_EXPAND_TOTAL_MAX`, 16 MiB: one value just under the per-key limit,
  named by ten thousand short lines, made gigabytes with no key over it.
- A YAML alias is bounded in bytes as well as events, 64 times the
  document plus 1 MiB (D-63): one large scalar aliased thousands of
  times made a megabyte of input copy gigabytes.
- A drop-in or secrets directory no longer loses files without a word
  (D-78), below.
- A SENSITIVE `setting()` in the Python binding is left out of its
  dataclass's `repr`.
- **A reader's name tables are seeded a decode** (D-81). The TOML, YAML,
  INI and XML readers hashed names from a fixed start, so a document
  could choose names that all land in one bucket and make reading
  quadratic. Each decode starts from a seed made of the addresses of
  its table and of a local, which a document's author cannot know where
  the system randomises addresses; where it does not, nothing is worse
  than it was. Nothing a caller sees changes.
- **A test holds the library to it**: `test_marker` drives a marker
  value through every way a sensitive key is accepted or refused - each
  refusal reason, strict and with fallback, from memory, files, the
  environment, the command line and a loser's place - and fails if the
  marker, or its first six characters, is in any text the library
  writes: errors, `explain()`, the blind report, diffs, the fingerprint,
  the schema, log events, `cf_live`'s reloads and a store's edits. Both
  refusal leaks found by hand fail it when put back.

### Added

- **CBOR** (RFC 8949), read and written (D-91): `.cbor` files and
  documents named `cbor`, chosen from content when a name gives no
  format, and `cf_emit(c, "cbor", ...)` with numbers in
  their preferred serialization; maps of text keys, arrays, integers, floats of every width,
  text, booleans and null, each value placed in `explain()` by the byte
  it starts at. Byte strings, tags other than 0, 1, 32 and 55799,
  undefined and a key that is not text are refused by name. Held to
  RFC 8949's Appendix A vectors and its malformed examples: 68 of 68
  read, 94 of 94 refused, 55 of 55 values; fuzzed with the other
  readers. `CF_WITH_CBOR`, on by default.
- **`CF_CODEC_BINARY` and `cf_encoder_head`**: a codec of one's own that
  writes bytes can keep `${` literal as the built-in ones do.

- **Keys checked at compile time in C11** (D-75): `CF_DEFINE_INT` and
  `CF_DEFINE_TEXT` refuse, when compiled, a definition the library would
  refuse at `create` - a default outside its range, a range upside down,
  a text default outside its length bounds, an empty or overlong key, a
  key required and defaulted. A C89 program sees nothing new.

- **systemd's credentials are a source**: `cf_builder_credentials` reads
  the directory `$CREDENTIALS_DIRECTORY` names, as the secrets directory
  reads one, and every value says "systemd credential <id> (<file>)" in
  `explain()` and in a report, so it is told from a plain secret file.
  Outside systemd `CF_OPTIONAL` reads nothing; without it the error
  names the variable (D-80). C++ has it as `Builder::credentials()`,
  Rust as `Builder::credentials`, Python as `Builder.credentials`; a
  Python binding on an older library refuses the call by name.
- **Keys checked at compile time in C++** (D-75): from C++11 a
  `constexpr` `cf::Key` with a name the library would refuse, a range
  upside down or a default outside its range does not compile, and the
  error names which. C++17 adds constant field lists and C++20
  `constinit`; C++98 is as it was. `cf::key_is_valid` is the library's
  rule in every standard, and the check at `create` stays in all of
  them. Tests that must not compile are a kind of test now
  (`cmake/ConfigForgeMustNotCompile.cmake`).
- **Doctor is held to the marker too**: `doctor.marker` puts a marker
  secret into every state one configuration can hold - accepted,
  refused five ways, shadowed, under an old name, in a list, a number -
  and runs every Doctor command in every format on the reports. None
  prints the marker, its first six characters or its digits; with the
  redaction switched off, `dump` is the first to fail it.
- `docs/Plan.md` and `docs/plan/`: the work left, a card a unit, each
  with its design decided, its test and what done is - written so that
  a small model can carry a card out without designing anything.
- `docs/Agents.md`: how the work is divided between an organising
  session and the subagents it briefs, and how a session stops without
  losing it.

### Fixed

- **A reference to a key whose own expansion was refused**, with nothing
  to fall back to, is refused instead of read through NULL, which was a
  crash.
- **A name anchored again in YAML takes over its entry** instead of
  lengthening the lookup chain.
- **A real past the largest double is refused**: `1e309` in JSON or
  TOML, or an integer of 310 digits, entered the key space as infinity
  and was written back as `"a": ` with nothing after it.
- **A literal of hundreds of digits no longer becomes a wrong value**
  when its digits overflow the parser's 2048-bit integer: digits past the
  290th move the exponent and set the rounding. Known limit: a literal
  of more than 290 digits lying exactly on a halfway point may be one
  unit in the last place off. `0e999` now reads as 0.
- **A level of distinct sibling names no longer makes the XML reader
  quadratic**: each child's name asks a hash in the sink's work space,
  not every earlier name, and 40,000 siblings read in under a second
  where they took forty.
- **A multi-line string no longer reserves a key-space record a line**:
  the guess from a document's lines is capped at one record for four
  bytes of input, so two megabytes of one string reserve nothing like
  the ninety they did, on each reload.
- **`cf_codec_detect` with no arena detects TOML, and JSON with long
  strings**: it makes its own work space for the call, where TOML took
  the lack of one for running out of memory.
- **The Rust binding no longer quotes a string with a NUL in its
  error**, which could be a secret, and `Edit::set` refuses one at
  commit instead of writing a different value.
- **A section of many distinct `name[]` arrays in INI no longer takes
  quadratic time**: 60,000 lines took a minute and take seconds. The
  reader remembers the section's names in the sink's work space, and
  rescans as before only when none is lent.
- **A drop-in or secrets directory no longer loses files without a
  word** (D-78). Past 256 files the listing stopped at whatever the disk
  handed over first, so a `99-override.ini` could vanish; it is now an
  error naming the directory and the limit, which `CF_OPTIONAL` does not
  excuse. A name too long to carry, and an entry that is not a file (a
  FIFO, a socket, a link to nothing), are errors that name it; a FIFO no
  longer hangs `cf_create`, a reload or the kqueue watcher. Links are
  still followed, so Kubernetes mounts work as before; subdirectories are
  skipped on every platform. On Windows a watch on a drive-relative path
  to a missing drive (`Q:app.ini`) returns an error instead of looping
  for ever.
- **A document that starts with a UTF-8 byte order mark** - as Windows
  editors write one - is read by the TOML, INI, JSON and JSON5 readers,
  which refused it, as the `.properties` and YAML readers already did.
  The mark is skipped at the start only, and positions in `explain()`
  are as without it; `cf_blob_skip_bom` is the one check all of them use.

## 1.4.0 — 2026-11-07

### Changed

- **Response files are opt-in** (D-79). This changes a default inside
  1.x, on purpose. `cf_app` and `cf_app_builder` no longer expand an
  argument that starts with `@`: it is an ordinary positional argument,
  which the command line skips, so an untrusted one cannot inject
  options or tell which files exist. A program that relied on `@file`
  starts from `cf_app_builder`, calls the new
  `cf_builder_response_files` (C++ `responseFiles()`, Python and Rust
  `response_files()`), then `cf_builder_create`; `cf_app` itself never
  expands. A response file that cannot be read still fails the builder.

- A file whose extension no format claims was an error and is now read
  by its content, when one format reads it (D-73).
- **The key space is sized before it is filled** (D-76): from the lines
  of a document that start like a key, and on a reload from the
  generation before, in a heap arena; a wrong guess leaves the doubling
  as before. Loading 1,000 keys is 14% faster and asks the allocator for
  28% less; a reload of 10,000 is 14% faster. The definitions' index,
  the emitter's output and branch hints were measured and left as they
  are, as they did not pay (Maintaining.md, "Measured").

### Added

- **The format of a file is chosen from its name, then its content**
  (D-73). A file with no extension, or one no format claims, is read as
  the format its content turns out to be, decided by reading it with
  each codec - the builder's own first, then the strict formats, then
  YAML and INI, which must not read it differently; `.properties` is
  never chosen from content. An empty file, text no format reads and an
  ambiguous file are refused, naming what was tried. A name's format is
  still taken as it stands, and when it cannot read the file the error
  adds the format the content looks like. `cf_codec_detect` does the same
  for bytes of a program's own; `cf_source_file_format` says which format
  a file was read as, and how.
- **`cf_builder_document`**: a document the program holds in memory -
  fetched, embedded, read from a database - as a source, its format
  named, from its name, or from its content; its bytes are copied, and
  its places in `explain()` are its name and line.
- **The rest of D-73**: a drop-in directory reads a file with no
  extension by its content (one with an extension no format claims, such
  as a backup's `x.toml~`, is still refused); `cf_builder_env_document`
  reads a whole document from one environment variable, opt-in per
  variable; the report gives each document source its format and how it
  was chosen (`format`, `format_how`, optional), and Doctor's `explain`
  prints it; the conformance run asks each suite's valid documents, with
  no name, which format they are (`detect-<format>` floors).
- **Doctor checks a report's fingerprint against its own keys**, CFD019
  (error): a report whose `fingerprint` is not the one its keys give was
  edited, cut short or written by a broken build. A report without the
  field is not an error. Doctor computes it with the library's own steps,
  now public as `cf_fingerprint_seed` and `cf_fingerprint_step`, for a
  reader that holds keys as text.
- **INI is compared with Python's `configparser`**, value by value, in
  `-DEXTRA=conformance`: 36 of 36 documents read the same, with the floor
  `ini-configparser`; twelve more differ by design, each with its reason
  in the script and in Formats.md - quotes around a value, keys before a
  section, `:` as a separator, continuation lines, `name[]` arrays,
  trimmed section names, text after a header, `[DEFAULT]`.
- **Rust keys checked at compile time** (D-75): a `const` `Key` whose name
  the library would refuse, or whose range runs backwards, does not
  compile; `key_is_valid` is the rule, held by a test to the library's.
- **Continuous fuzzing with ClusterFuzzLite**: five minutes against each
  pull request, half an hour every night on `main`, findings in the
  Security tab.

## 1.3.1 — 2026-11-07

What 1.3.0 was to carry - it was tagged, and not published - and one
change since.

### Added

- **JSON5, and with it JSONC** (D-66): `.json5` and `.jsonc` files are
  read by the JSON reader in its JSON5 mode - comments, trailing commas,
  unquoted and single-quoted names, single-quoted strings, `\v`, `\0`,
  `\xHH` and line continuations, hexadecimal and signed numbers, `.5`
  and `5.`, Infinity and NaN, and Unicode white space - with every value's
  place exact for `explain()`. The codec is `json5`, the feature name
  `json5`; written, a configuration is JSON. Infinity and NaN are their
  text, as TOML's inf and nan are, so a key defined as real refuses one.
  Held to json5-tests: 83/83 valid, 31/31 invalid, 83/83 values against
  the `json5` package; fuzzed, a libFuzzer target of its own. A `.json`
  file is read exactly as before.
- **`doctor check --schema app.schema.json app.toml`** (D-67): files read
  against a JSON Schema contract as the program would, without it -
  unknown keys, types, ranges, spellings, required keys - the first
  refusal named, exit 1; and the `configforge-check` pre-commit hook,
  which checks each staged file.
- **`doctor gen --schema app.schema.json`**: the contract as one C89
  function of `cf_def` and `cf_builder_define` calls, so a build without
  the JSON reader has the same contract from the same schema. It reads
  back through `cf_schema` as the schema it came from.
- **A format of one's own, registered on the builder** (D-68):
  `cf_builder_codec(b, codec)` - its extensions choose it for the files
  and directories added after it, a named format finds it, ahead of the
  built-in codecs; `cf_emit` writes it and `cf_config_codec` returns it,
  and a derived configuration keeps it. A second codec of one name is
  refused. `conform --codec <library> <symbol>` and `conformance.py
  --codec` score a codec from a shared library on its format's suite
  and floors. New: `cf_builder_codec`, `cf_config_codec`,
  `cf_codec_among`, `cf_source_dir_codecs`.
- **`cf_diff_json`**: the changes between two configurations as JSON
  Lines, one object a key - its change, the values before and after,
  where the new value is from and the old one was - for a log pipeline;
  a sensitive key has `"sensitive": true` and no values, as in the text.
  `cf_sink_json_string` and `cf_sink_json_value` are the JSON writers the
  report, the encoder and the diff now share.
- **Signed provenance for every released file**: the release workflow
  attests each archive it publishes (GitHub's build provenance), and
  `gh attestation verify <file> -R develooper1994/configforge` checks it.
- **The build's limits in the report**: `"limits"` beside `"features"`
  in the library block - key, message and path lengths and the nesting
  depth - so a report from a build with other limits says so.
- **A settle interval for reloads**: `cf_live_options.settle`, the polls
  a change waits after it is seen, so a burst of writes is read once, at
  its end. 0, the default, reads it at once, as before.

### Changed

- **A file is read into a buffer of the size the system reports**
  (D-76): one allocation and no copy, where the buffer used to start at
  4 KB and double, copying each time. The size is taken as a
  prediction: a pipe, a device or a file that grows is still read whole,
  in chunks, as before. Reading a 1 MB file is 3.4 times faster and its
  peak in the transient arena a third (Maintaining.md, "Measured").

- **TOML 1.1.0**: times without seconds (`13:37`), `\e` and `\xHH`
  escapes, and inline tables over several lines, with comments and a
  comma after the last key. Held to toml-test's 1.1.0 files: 218/218
  valid, 502/502 invalid, 218/218 values. A TOML 1.0 file reads as it
  did; a few that 1.0 refused, such as a trailing comma in an inline
  table, now read, as 1.1 says.
- **One `define`, the type given or understood from the default**
  (D-74), beside the typed calls, which stay: `cf_builder_key(b, key,
  type, "default", min, max, flags)` in C, with `CF_TYPE_NULL` for "the
  default's text decides", the one call Python and Rust pass through, so
  the rules and errors are the library's everywhere; `CF_DEFINE(b, key, dflt)` in C11, by the default's C
  type; `Builder::define(key, dflt)` in C++98; `Builder.define(key,
  default, type=None)` in Python; `Builder::def(key, dflt)` in Rust. Each
  is tested to make the same contract as the typed calls.
- **A file's bytes are kept only while it is read** (D-61): a file, a
  `.env` file and a drop-in directory are read into an arena of their
  own, freed - zeroed first - once the reader is done, so a
  configuration holds its values and no longer the text of every file it
  came from; a 200 KB file of one key leaves the key. A fixed arena
  keeps the bytes in it, as before. `cf_blob_arena` and
  `cf_blob_arena_done` give a source of one's own the same, and
  `cf_source_read_blob` reads a document's bytes through a codec as the
  file sources do.
- **Every C++ standard from C++98, cumulatively** (D-72): C++98 is the
  floor and stays, and each later standard adds to the one before
  without changing it. `CF_CXX_LEVEL` says which layer a program has (98,
  11, 17, 20 or 23), and the header gate checks it at every standard.
- **YAML is read by a parser of the library's own, and is on by
  default** (D-69): YAML 1.2 by recursive descent, allocating nothing,
  every value with its place; all of yaml-test-suite - 308 of 308 valid
  cases event for event against the suite's own streams, 94 of 94
  invalid refused, and through the library 122/122, 94/94 and 110/110
  values, as libfyaml. `CF_WITH_YAML` defaults to ON and
  `CF_YAML_BACKEND=auto` takes the built-in parser; `libfyaml` and
  `libyaml` remain by name, and the vcpkg and Conan packages no longer
  need libyaml for YAML. A build that named neither and has libfyaml
  installed reads through the built-in parser now.
- **The Rust crate builds with cargo alone** (D-70): `build.rs` compiles
  the library's C sources with the `cc` crate instead of running CMake,
  so a C compiler is all a Rust build needs. `cc` is the one build
  dependency, pinned with its checksum in `Cargo.lock`; cargo runs with
  `--locked`. The packaged crate carries only the headers and sources.
- `setup.sh`'s Python environment (`/opt/cf-tools`) sees the system's
  packages and adds json5 0.17.3, the reference for JSON5's values; verify
  runs the conformance step with it.

## 1.3.0 — 2026-11-06

Tagged, not published: the release's Windows build left out the binary
of a Doctor test added in this version, and the release stopped before
publishing. What it would have carried is listed under 1.3.1.

## 1.2.0 — 2026-11-06

### Changed

- The formatting gate takes one clang-format major version, 22
  (`cmake/ConfigForgeClangFormat.cmake`): 18 and 22 lay out some code
  differently, so a file clean under one failed under the other. Another
  version is reported and not used. `setup.sh` and CI install 22.1.8
  from PyPI; the C++ header, tests and examples are now formatted and
  checked too.
- `setup.sh` installs Rust 1.99.1 with rustup, checked against its
  published checksum; Ubuntu's 1.75 could not read the tree's lock file.
- CI and the development environment (`setup.sh`, the Containerfile,
  `wsl-distro.ps1`) are on Ubuntu 26.04 LTS: GCC 15, clang 21, CMake 4.2,
  Python 3.14, libfyaml 0.9.4. CI's jobs name `ubuntu-26.04`, since
  `ubuntu-latest` still means 24.04; the kqueue job stays on 24.04, where
  libkqueue still is, and keeps the previous LTS in the matrix.

### Added

- MSVC's analyzer (`/analyze`) under `CF_ANALYZER`, on every C target,
  and in verify's Windows quick run (the analyze preset). Of its findings
  in the library and Doctor, five were fixed in the code and five are
  silenced at their line with the reason - `CF_ANALYZER_COUNTED` and
  `CF_ANALYZER_FILLED` in `cf_port.h`, one warning number each, nothing
  outside an analyzed build. The tests needed none: nine of them read
  through a pointer a failed check had found NULL, and crashed rather
  than reporting; each now reports and stops.
- `CF_GC_SECTIONS`: each function and object in its own section, the
  unused ones dropped at link (`--gc-sections`, `-dead_strip`, MSVC
  `/OPT:REF`), on in the `minimal` preset: a third of a small program.
  `CF_LTO`: link-time optimisation where the toolchain has it, off by
  default; the stack budget says it is not measured under it.
- A declared list with a separator splits a value from a file that
  carries no types - INI, `.properties`, a `conf.d` directory of them -
  as it splits the environment's: `ports = 80,443,8443,22` is four
  elements, typed by the definition. A key that is not a declared list
  keeps its text. Not a syntax: no INI reader has bracket lists, and
  `[80,443]` is TOML's, which this library reads (`cf_flatten_lists`,
  `cf_source_file_defs`).
- `doctor lint --base B --only-changed`: only what a change touched -
  the keys whose candidates differ from B's, or that one side lacks, and
  the sources those came from - with the rest counted, so a large
  configuration adopts Doctor one change at a time. The action's
  `only-changed`, the hook's `--only-changed`, Python's `lint(base=)` and
  `cf_doctor_only_changed_file` take it too.
- Arrays in INI, as PHP's `parse_ini_file` has them (D-60): `ports[] = 80`
  appends an element, `ports[1] = 8443` sets one, and both are the flat
  `ports.0` and `ports.count` every other format's array is. An array of
  values is written that way; an array of tables, of arrays, or an empty
  one is refused by name (`CF_CODEC_SCALAR_ARRAYS`). A second run of one
  array's `name[]` lines is refused with both lines named. Additive: a
  key with `[` was refused before.
- `bench/` (`-DCF_BUILD_BENCH=ON`): what building, reading, reporting
  and Doctor's diff cost at 100 to 20,000 keys - not a test, the numbers
  an optimisation starts from. It found both of the quadratic costs
  under Fixed.
- `scripts/prestage.cmake`: format, the API reference, the text check
  and the analyze build of a change, before it is staged.
- Conformance, measured (D-62, `docs/Conformance.md`): each reader
  against its standard's own suite - JSONTestSuite for JSON, toml-test
  for TOML 1.0.0 - fetched at a pinned commit by `verify
  -DEXTRA=conformance` (tier 2) and scored by `scripts/check/conformance.py`
  against a floor that only rises, `tests/conformance/floor.txt`. The
  same step reads the project's own configuration files with the
  library's readers and compares each key with Python's reference
  readers (`conformance.py --self`).
- All of TOML 1.0.0: multi-line strings, basic and literal; dates, times
  and date-times with or without an offset, checked against the calendar
  and read as their text; `inf` and `nan`, read as their text, since the
  library keeps every real finite - a key defined as real refuses them by
  name. toml-test: 208/208 valid documents read to the values it states,
  509/509 invalid ones refused. JSON's 95 valid documents read to the
  values Python's `json` reads.
- YAML anchors and aliases (D-63): an alias is its anchored node again,
  and an aliased value is explained at the line it was written. An alias
  inside its own anchor is refused, and so is expansion past 64 events
  per event read, so a "billion laughs" fails fast, by name. The core
  schema's tags - `!!str`, `!!int`, `!!float`, `!!bool`, `!!null`,
  `!!map`, `!!seq` - type a value as YAML 1.2 says; another tag is still
  refused by name. The conformance run scores YAML on yaml-test-suite,
  counting what the configuration model refuses apart from the grammar.
- YAML through libfyaml (D-62): `CF_YAML_BACKEND` is `auto`, `libfyaml`
  or `libyaml`, and `auto` takes libfyaml 0.9.1 or later where it is
  installed. Through it the reader reads all of yaml-test-suite as YAML
  1.2 says - 122/122 valid, 94/94 invalid, 110/110 values - where
  libyaml, which follows YAML 1.1, gives 100/133, 87/94 and 90/93.
  libyaml stays the backend for MSVC and C89-only compilers. libfyaml
  before 0.9.1, which Ubuntu 24.04 ships, hangs on malformed input
  (libfyaml issue 101) and is refused by name, at every point it could
  enter: by the build, from the `libfyaml.pc` beside the library; at run
  time, by asking the loaded library its version, since 0.8 and 0.9
  share a soname; and by `configforge.pc`'s `Requires.private`. CI has
  a libfyaml job, on Ubuntu 26.04's 0.9.4.
- Definitions from a JSON Schema (D-64): `cf_builder_schema`,
  `cf_builder_schema_file`, `cf_defset_schema` and
  `cf_defset_schema_file`, and C++'s `Builder::schema`/`schemaFile`. The
  reverse of `cf_schema`, and what it writes reads back as the same
  contract; a keyword the library cannot enforce (`$ref`, `allOf`,
  `pattern`, ...) is refused by name and place, never dropped. In a
  closed schema, `cf_schema` now writes `"additionalProperties": true`
  on an opened prefix, which is JSON Schema's default made explicit.
- `cf_doc_sink.reserve_work`: optional workspace a reader keeps
  bookkeeping in until decode returns (D-62); the loader lends it from
  the configuration's arena.

### Changed - look at this before upgrading

- TOML defines a key, and a table, once, as its standard says (D-62): a
  key set twice, a table with two `[headers]`, a key or a `[header]`
  added to an inline table or to a value, a dotted key adding to a table
  a `[header]` defined, and `[[name]]` on what is not an array of tables
  are refused with the name and the place. 1.1 read such a document,
  the later key winning, as a stated departure from TOML; a file that
  loaded under 1.1 can stop loading. A sink of its own without
  `reserve_work` still reads as 1.1 did.
- TOML refuses control characters in comments and DEL in strings, a
  carriage return that does not end a line, and malformed UTF-8 in
  comments and strings, and refuses a trailing comma in an inline
  table. With these, toml-test's 509 invalid documents are all refused.

### Fixed

- Numbers, as their standards write them. JSON accepted what RFC 8259
  forbids - a leading zero, `1.` with no digit after the point, a sign
  with no digits - and is 95/95 and 188/188 on JSONTestSuite now. TOML
  accepted underscores not between two digits, a sign on a hexadecimal,
  octal or binary integer, a leading zero, a point or an exponent with
  no digits after it, and letters after a number; refused now, by the
  TOML 1.0.0 grammar, with what was wrong named.
- TOML refused two valid documents as reopening an array of tables:
  `[[a.b]]` in the second element of `[[a]]`, which is an array of its
  own, and a second `[['']]`.
- Defining keys was quadratic: each definition was checked against the
  others by a scan, and sealing the set was an insertion sort, on the
  stated ground that "definition counts are in the tens". Ten thousand
  definitions took 1.15 s and now take 16 ms; twenty thousand, 3.96 s
  and 38 ms (a hash of the unsealed set, `qsort` to seal it).
  `test_complexity` holds it.
- Doctor's `diff` and `history` were quadratic in report size, looking
  each key up by a scan; two 20,000-key reports took 6.8 s and take
  1.3 s. `doctor_api` holds it, at a step wide enough that the scan
  cannot hide behind the cost of reading a report.
- `example.policy` and `example.policy_cpp` write one Registry key, and
  under a parallel ctest one read what the other rewrote; they hold a
  resource lock now.
- `verify -DEXTRA=abi` compares with the newest release that is not
  this version; with the release commit tagged first, it had compared
  1.1.0 with itself.

## 1.1.0 — 2026-11-05

### Added

- Probation for a reload: with `probation` set in `cf_live_options`, a
  swap keeps the snapshot it replaced for that many polls.
  `cf_live_rollback` returns readers to it, as does a `health` function
  that answers false at one of those polls; `cf_live_confirm`, or the
  last poll, ends it. A rollback is a new generation, a
  `"reload-rolled-back"` event with its reason, and a count in
  `cf_live_status` (`rollbacks`, `probation`), and the file it came from
  is not swapped in again until it changes. `CF_LIVE_ROLLED_BACK` is a
  new result. Off by default; an options struct of 1.0's size is read
  as 1.0's. C++, Rust (`LiveOptions`) and Python have the same, and
  the live example a fourth scene.
- The configuration's fingerprint where operators look:
  `cf_fingerprint` and `cf_fingerprint_format` ("fnv1a:" and sixteen hex
  digits), in the provenance report as `"fingerprint"`, in each
  `cf_live` log event as `previous` and `fingerprint` (new members after
  1.0's), in the reload's log line, and in Doctor's `dump` and Markdown.
  A secret enters it only as its blind-report digest, so it may be
  published wherever a blind report could. C++, Rust and Python have it;
  the Guide has a recipe for a log field and an OpenTelemetry attribute.

- Doctor on a pull request: the action takes the base branch's report
  as `base` and diffs the change against it, writes the findings to the
  job summary as Markdown and, with `comment: true`, to one comment
  that each push updates; a fork's read-only token is a note, not a
  failure. A pre-commit hook (`.pre-commit-hooks.yaml`) lints each
  staged report the same way. `doctor --format markdown` is new for
  every command that finds. The scripts behind both are tested by
  `doctor.action`.

- API reference entries link directly to their implementation lines,
  including platform variants. The reference gate checks the targets
  against source, keeping the Markdown usable without a site generator.
- Declared scalar lists with copied typed defaults, per-element
  constraints, separator-delimited environment/dotenv inputs and
  repeated command-line options. REPLACE keeps per-index resolution;
  APPEND combines source-local contributions in precedence or policy
  order while preserving original keys and assignment history.
  Reports and JSON Schema describe the list contract. Resolved
  snapshots do not append defaults twice.
- Python `Builder.define_list` and Rust `Def::list` with typed list
  defaults and aliases. The `lists` runtime capability lets bindings
  refuse unsupported libraries rather than silently define scalars.
- Whole-list restart protection during live reload, and source-local
  index translation for writes to existing appended elements.
  Ambiguous structural writes are refused before the source callback.
- `doctor history` and `doctor blame` follow one service through a
  sequence of reports, oldest first: each key added, removed, changed
  (by digest for a secret) or moved to another source, and the report a
  key's present state dates from. A step that cannot be compared is
  reported as such, not as unchanged; no message carries a value. Every
  report is read and room for every finding reserved before anything is
  recorded, so failure adds nothing. `cf_doctor_history_files`,
  `cf_doctor_blame_files` and Python `history`/`blame` expose the same.
- Default drift (CFD017): `doctor diff` warns where a key takes its
  declared default on either side and the defaults differ, or one side
  has none, so an upgrade's changed default is found by diffing the
  deployed report against the upgrade's. Report candidates that are a
  declared default carry `declared_default: true`, an additive schema-1
  field; Doctor reads an older report's defaults by source kind.
- Read coverage: `cf_builder_track_reads` records each key the program
  asks for; `cf_was_read`, `cf_read_count` and `cf_tracks_reads` answer,
  and a report from a tracking configuration gives every key a
  `read` boolean, blind reports included. Off by default, with no
  change to a read; on, each key's flag is atomic and set once, so
  concurrent reads stay safe (D-14). The Python, Rust and C++ bindings
  have the same calls, and Doctor's CFD018 names each key a source set
  and the program never read.

- A searchable documentation site built directly from the existing
  Markdown and generated API reference, retaining repository-relative
  links. Strict builds, anchor validation and search-index checks run
  before GitHub Pages deployment; pull requests cannot deploy.
- Python wheels carry the shared library alongside the binding. The
  root `pyproject.toml` uses scikit-build-core, and a source distribution
  includes the native sources needed to build without the checkout.
  `CONFIGFORGE_LIB` still selects a custom library; Doctor's Python
  driver still needs a separate Doctor executable.
- `scripts/package-rust.cmake` stages a standalone crate with its C
  sources, licence and examples, then verifies and tests the unpacked
  package with Cargo offline. Native sources are copied when packaging,
  not maintained twice. Nothing is published by either packaging path.

### Changed

- Releases use GitHub's source zip and tar downloads rather than
  uploading a duplicate source archive. The release workflow and local
  release script still build the amalgamation and platform archives;
  `SHA256SUMS` covers those uploaded artifacts.

### Fixed

- Rust's `Debug` for `Config` printed `cf_digest`, which hashes secret
  values themselves; it prints the fingerprint now.
- `CF_SANITIZE` under MSVC copies AddressSanitizer's runtime DLL
  beside every program it builds. Without it on PATH, Windows stopped
  each test with a dialog that waited for someone to press OK.
- Blind reports hash all sensitive text bytes and retain their actual
  length, including embedded NUL. Text longer than the numeric
  formatting buffer previously had length zero and the empty digest,
  so changes to long secrets were invisible to report comparisons.
- The list provenance table uses explicit byte-copy conversions for
  clang-tidy. Response-file argument growth uses an equivalent branch:
  PCC's preprocessor misplaced its ternary token at an input-buffer
  boundary after the flat list sink enlarged the source file.
- Reload preserves typed running values without reinterpreting literal
  `${...}` text. Replaying a snapshot retains original insertion order,
  including same-source duplicate precedence.
- Natural environment and dotenv spellings no longer also create an
  unknown underscore key. Exact canonical definitions win ambiguous
  spellings, and escaped names win independently of enumeration order.
- Blind reports and single-key JSON omit sensitive defaults from
  definition metadata as well as redacting candidates. Previously a
  selected or shadowed secret default was still visible there.
  Unsupported report modes now refuse output instead of producing
  unredacted values under a `blind` label; explicit full mode is
  unchanged.
- Configuration emission escapes literal `${` as `$${`, so values and
  defaults survive loading the written file without being expanded
  again. Measurement and every truncated buffer keep the same escaped
  length and prefix. Raw document encoding remains unchanged.
- JSON and TOML configuration strings are no longer limited by the
  error-message buffer. Decoders measure with the same grammar they
  use to copy, then borrow reusable workspace from the flattener's
  arena. The optional, size-gated `cf_doc_sink.reserve_text` callback
  gives custom readers the same facility; older sinks still work for
  strings up to 255 bytes. Encoders write only the original sink
  prefix, so a 1.0 caller's output object is not overrun.
- A selected key that is both a value and a container prefix is refused
  on emission as `CF_E_KEY`, naming the key, instead of producing
  duplicate members that other parsers reject. A selection containing
  only one side remains writable.
- `gate.abi` failed every Windows shared build: it listed the DLL's
  symbols with nm, which finds none in a PE export table, and reported
  all 292 public functions missing. It now reads the import library,
  which names them (on Linux and macOS the linker file is the shared
  library itself). CI's `shared (windows-latest)` job had failed on it
  since 2026-10-31; the library was never at fault.
- The Python binding remains importable when a selected library does
  not export read-tracking functions; requesting tracking reports the
  missing capability instead of failing during import.

## 1.0.0 — 2026-11-01

The C interface is frozen: from here it follows semantic versioning,
additive within 1.x (D-56), and `verify -DEXTRA=abi` fails an
incompatible change. Three things an emitted file holds do not come back
the same, and are written down rather than silent: a JSON or TOML string
longer than `CF_MESSAGE_MAX - 2` bytes, a text value holding `${`, and a
key that is both a value and a table (`docs/Guide.md`, section 10;
`docs/Roadmap.md` has the plan for each).

### Changed

- `cf_report_info` and `cf_doctor_gate` begin with `size`, as every struct a
  program fills does (D-51), and start from `CF_REPORT_INFO_INIT` and
  `CF_DOCTOR_GATE_INIT`. Code from 0.4 that cleared one with `memset` or
  listed its members now passes a size of 0: `cf_report` writes nothing for
  it, and `cf_doctor_live_check` refuses every snapshot, naming the macro.
  `NULL` is still the defaults. Done before 1.0 because a member added to
  either afterwards would have broken the ABI.
- `cf_derive_in` and `cf_watcher_open` are `CF_NODISCARD`, as every other
  call that creates something is (D-53): a result dropped is a leak.
- `cf_live_commit` without a `cf_live` is `CF_E_CAPABILITY`, as
  `cf_live_poll`, `cf_live_reload` and `cf_live_approve` are; it said
  `CF_E_SOURCE`.
- The longest key is `CF_KEY_MAX - 1` bytes, 255 by default, as
  `cf_build_options.h` has always said: the limit counts the terminator.
  `cf_key_valid` took a key one byte longer, which a builder's pairs, the
  command line and the environment then accepted, though no buffer that
  holds a key had room for it, and no reader took it back from a file.
  Every reader now takes a key of 255 bytes, which each refused, as one
  name or as many; and an array whose length key, `<array>.count`, would
  be longer than that is refused as a key this build cannot hold, where it
  was read without a length, which no source could then shorten.

### Fixed

- Doctor did not see a key no source won: an optional key with no default
  whose only value came from a source its policy refuses, which a report
  writes with `"selected": null`. CFD016 never fired for it, so a secret
  passed on the command line to such a key went unreported; `doctor explain`
  said the key was not in the report; and a report with no other key was
  refused as describing none. Doctor now finds a key by either entry.
  CFD004, CFD012 and CFD015 had no test at all, and now have one.
- `cf_emit` wrote some configurations as a different one. An array of
  eleven elements or more came out in the order of its keys, `0, 1, 10,
  11, 2`, so the third of twelve was the twelfth; an array that held both
  scalars and tables wrote the scalars first; a table in an array was
  given a name, which made the JSON `["a", "1": {...}]`; and a table with
  a member called `count` - a pool size, a retry count - was taken for an
  array and written as `[]` with everything else in it lost, or refused
  outright by INI. Members now come out in index order whatever they are,
  with no name inside an array, and a group is an array only when it holds
  index keys and the `count` the readers record and nothing else. INI also
  refused a configuration for an array that `CF_EMIT_CONFIGURED` or
  `CF_EMIT_FROM_RANK` would not have written; it refuses only what it
  would have written.
- A format's writer and its own reader disagreed about how deep a document
  may nest, so a configuration `cf_emit` wrote could be one its reader
  refused. TOML wrote a table nested nine levels or more, and its reader
  refused a header of more than eight parts, a limit nothing documented;
  it counted the levels of an inline array without the tables around
  them, and so wrote one a level too deep; and JSON wrote a level more
  than its reader takes. Every format now reads and writes the document
  and `CF_MAX_DEPTH - 1` levels inside it, and `cf_emit` refuses the next
  one in every format alike, as `CF_E_KEY`: "the document nests deeper
  than 32 levels". It said "sink refused 'a'", as `CF_E_SOURCE`, for the
  formats that stopped it, and wrote a document no reader would take for
  the rest. A refused `cf_emit` also leaves an empty string in the buffer,
  where it left the front of a document that stopped in the middle.
- An array's length did not end the array. A command line that said
  `hosts.count=1` over a file that listed three hosts left `hosts.1` and
  `hosts.2` in the key space: `cf_get_text` read them, `cf_iter_keys` listed
  them, `cf_emit` wrote them and the digest held them, a list no source had
  asked for. D-02 decided the elements past the length are discarded, and
  the report schema has a `truncated` outcome for them, but nothing ever
  assigned it. The source that wins `<array>.count` now fixes the length,
  and an element at or past it has no value, whichever source supplied it
  - the default count included. It is not read, listed, written or in the
  digest; `cf_explain` and the report still show it, as `truncated`, with
  "past the end: hosts.count is 1, from cli" (a sensitive length is not
  quoted), and CFD015 says the same. A count that is not an integer, is
  negative, or is a `${...}` template fixes no length, and a group with a
  member besides indexes and `count` is a table and is left alone. Code
  that read an element past a shortened array no longer finds it;
  `cf_count` still counts it as a key, as it counts one whose candidates
  were all refused, and `cf_has` says which have a value. D-02 and the
  Design also describe an append mode and separator and repeated-flag forms
  for lists; none was ever built, and they now say so.
- `cf_emit` wrote some configurations that its own readers read back as
  different ones. A group of index keys with one missing, or whose `count`
  did not match its elements, was written as an array, so the gap closed
  or the length changed; an index that was both a value and a table became
  two elements. Only a group that is an array exactly - the indexes 0 to
  n - 1 and a `count` of n, which may still be text in a key space no
  definition has typed - is written as one now, and any other is a table
  of the keys it holds. YAML wrote C1 control characters, U+FFFE and
  U+FFFF as they were, which a YAML reader refuses, and text that is not
  UTF-8; it escapes the first and refuses the second. INI wrote a value
  with a line break in it, which its reader cannot read, and wrote bare a
  value its reader takes differently: one with a carriage return at an end
  or wrapped in single quotes. It refuses the first and quotes the second.
  A new test writes some ten thousand values and shapes in every format
  and reads each back. What still does not come back the same - a JSON or
  TOML string longer than `CF_MESSAGE_MAX - 2` bytes, a value holding
  `${`, a key that is both a value and a table - is in the Guide, §10.
- `doctor diff` reported origin drift (CFD002) for every key whose source
  differed, a key whose value differed too among them, so one difference
  came out as CFD002, CFD003 and a value diff. CFD002 is now what Doctor.md
  says it is: the same value - the same digest, for a secret - from a
  different *kind* of source. Two sources of one kind under different names
  are not drift, and a digest set against a value (a blind report against a
  full one) is not compared at all.
- The reference says what the code does: `cf_live_open`'s example takes
  six arguments; `cf_app` reads no file, and `cf_app_builder` is the way
  to add one; `"edit-refused"` is among the stable log events; a refused
  `cf_live_commit` touches no status; `CF_LIVE_OPTIONS_INIT` sets
  `log_min`; `cf_edit_begin` also returns NULL for a NULL argument;
  a builder borrows the pairs, the environment and the command line it is
  given until `cf_builder_create`, and copies names and prefixes at once;
  `cf_error_clear`, `cf_app_builder` and `cf_builder_abort` have their own
  descriptions, and `cf_builder_file_codec` and `cf_live_commit` keep
  theirs.

## 0.4.1 — 2026-10-31

### Fixed

- The test suite builds under MSVC's Release configuration again: one test
  divided by a constant zero, which MSVC folds and refuses (C4723). The
  0.4.0 tag has this one difference and was not released; the library is
  the same.

## 0.4.0 — 2026-10-31

### Added

- **The development environment in one command**:
  `scripts/buildenv/dev-setup.sh` (apt, Homebrew, or a container) and
  `dev-setup.ps1` (winget for MSVC and the tools, and the WSL
  distribution), each run again to bring a machine up to date, and each
  with a check that names every missing tool and the step it serves.
- **What 1.x promises, checked** (D-56): semantic versioning over the C
  interface, with what counts as breaking written down, and
  `verify -DEXTRA=abi` (tier 2) comparing the shared library with the
  last release's by `abidiff` - an incompatible change within one major
  version from 1.0 fails the run.
- **WebAssembly** (D-55): the library and its unit tests for wasm32-wasi,
  through clang and wasi-libc (`cmake/toolchains/wasm32-wasi.cmake`), run
  under Node's WASI (`scripts/wasi-run.js`) - single-threaded, no change
  handle, every unit test passing. `verify -DEXTRA=wasm`, tier 2.
- **Proofs, not only tests, for the code untrusted input goes through**
  (`tests/proofs`, `verify -DEXTRA=proofs`, tier 2): CBMC shows for every
  input up to a bound that the integer parsers and printers,
  `cf_infer_scalar` and `cf_fmt` read and write nothing outside their
  buffers and overflow nothing - and names the line when an off-by-one
  is put in. `docs/Safety.md` lists what is proven and what is not yet.
- **What Rust guarantees, stated and held for the C** (`docs/Safety.md`):
  each design rule that rules a class of memory or thread error out, the
  tool that holds it, and what is not yet proven. With it, hardened
  shipped targets - stack protector, stack-clash probes, CET, fortified
  calls, full RELRO, `/guard:cf` (`CF_HARDEN`, on); Valgrind's memcheck
  over every unit test (`verify -DEXTRA=valgrind`, tier 2); and
  `gate.symbols` refusing unbounded copies and `strtok` as it refused
  the locale-dependent calls.
  Valgrind's first run found a key space whose init failed left with
  its fields as the stack held them - a later refusal read one
  uninitialised; `cf_ks_init` now defines every member before anything
  can fail.
- **No string borrowed past the call that took it**: a builder copies
  every source name, path, prefix and format it is given, and a
  definition every string and list in it - description, new name,
  allowed spellings, aliases, policy - so a caller's buffer freed after
  the call is never read, by create, by explain(), or by a watcher. A
  memory source's pairs and a command line's arguments are read by
  create and not after. `cf_define` also copies a caller's `cf_def` only
  as far as its `size` reaches (D-51). `test_lifetime` frees every one
  of them, scribbled over, before create, under AddressSanitizer.
  The bindings' `Live` no longer keeps every reload's strings for as
  long as it lives - only the last build's, and any log callbacks a
  snapshot may still call.
- **Create, update and delete as one edit** (`cf_edit.h`, D-54): set
  keys, typed or as text, and delete others so the next source down
  supplies them again; commit holds the whole change to the contract and
  each key's policy and writes it in one transaction, or writes nothing
  and names the key. `cf_live_commit` commits and reloads in one step.
  `cf_write_item` gained `op` (`CF_WRITE_SET`, `CF_WRITE_DELETE`), and
  any other value is refused, so an item left uninitialised fails
  loudly rather than deleting. C++ `cf::Edit`, Python `cf.edit()` (a
  `with` block commits, an exception drops it) and `Live.commit`, Rust
  `Edit` and `Live::commit`.
- **The program's own C standard at the interface** (D-53): under C11
  one `cf_get(cfg, key, &out)` typed by `out` - `int`, `long`,
  `long long`, `bool`, `double`, `const char*` - refusing an integer the
  target cannot hold; C11 atomics for a compiler that is neither
  GCC-like nor MSVC; under C23 and C++17, `[[nodiscard]]` on the calls
  whose result is the only sign of failure. Compile-time checks hold the
  library to what D-51 rests on, and the header gate compiles every
  header as C89, C11 and C23.
- **Rust reads text without a copy**: `Config::str` returns a `&str`
  borrowed from the snapshot, so the compiler holds the caller to its
  lifetime; and `Config` is `Debug`, showing its keys and digest and
  never a value, which may be a secret.
- **`gate.text`**: no tracked text file holds a NUL byte or a carriage
  return, the two ways a file written through a tool stops being the
  same file - named by file and line. It found `docs/API.md` and the
  symbol list written with CRLF on Windows; both generators write LF now.
- **A log for what is not an error** (`cf_log.h`, D-52): a function the
  program gives (`cf_builder_log`, `cf_live_options.log`) receives each
  unknown key under `CF_UNKNOWN_WARN`, deprecated key in use, value
  refused and fallen back from, reload, failed reload and key waiting
  for a restart - with a level, a stable event name, the key, the
  source and the code. Nothing is written anywhere by the library
  itself, and a secret's value never reaches a message. Python sends
  events to `logging` (the levels are the same numbers), Rust to a
  closure (`Builder::log`, `Live::open_logged`), C++ through
  `Builder::log`.
- **Verification in tiers, and fast enough to run every time** (D-50).
  Tier 1 - GCC through the five presets, Clang, MSVC - for every commit
  (`verify -DTIER=1`); tier 2 - the thread sanitizer, the shared build,
  musl, TinyCC and pcc, YAML, kqueue, clang-tidy, fuzzing, coverage -
  before a release (`-DTIER=2`). Ninja and ccache where installed, so a
  rerun compiles what changed; a private copy of the index, so the Linux
  and Windows runs start together; a per-test timeout.
- **Releases without CI**: `scripts/release-local.sh` (source,
  amalgamation, static musl Linux in podman) and `release-local.ps1`
  (Windows) run the release workflow's jobs from a tag, for while
  GitHub Actions cannot; `scripts/README.md` has the steps to publish.
  `version.cmake` and `gate.version` now cover the Rust crate's and the
  Python package's versions as well.
- **One guide for every contributor**: `CONTRIBUTING.md` is the way in -
  the map, the loop, the traps, the recipes - and `docs/Maintaining.md`
  what the code does not say about how the work is done and on which
  machines.
- **A reload's status, for a health page** (`cf_live_status_get`, any
  thread): the generation serving, rebuilds tried and failed, what the
  last one did, whether a change waits for approval, and the last
  failure's error. `status()` in C++ and Rust, `live.status` in Python.
- **Doctor as the gate a reload passes through**:
  `cf_doctor_live_check` and `cf_doctor_gate` (a profile, a level to
  fail on, a baseline and a severity file), handed to `cf_live`, lint
  every snapshot before readers can have it; the first finding at the
  gate's level is the refusal's reason, and the running configuration
  keeps serving. A test also shows `cf_live` noticing a Kubernetes
  ConfigMap updated the kubelet's way, by renaming the `..data` link.
- **Python: the library itself, not only Doctor** (D-49). The
  `configforge` package, over the shared library through ctypes, with
  the standard library only: `cfg.server.port` by attribute,
  `cfg["server.port"]`, `cfg.get(key, int)`, `explain`, `derive`,
  `to_dict`, a refusal as `cf.Error` with the key; a dataclass whose
  fields (`cf.setting`, `cf.section`) are the contract; `cf.Live`. Typed,
  packaged by `pyproject.toml` with `configforge_doctor` beside it, and
  tested against a shared build by verify's new `EXTRA=shared` step -
  CI's shared job, run locally.
- **Rust: typed reads, keys, a struct, and reloading**:
  `cfg.get::<T>()` (nothing for a value `T` cannot hold), `Key<T>`
  constants that define and read, `settings!` for the program's struct
  with ranges and flags, and `Live` with a `LiveReader` for the workers;
  a panic in a build function is a failed build. `examples/rust/live.rs`,
  and `cargo test` in the suite under `CF_RUST`.
- **A configuration that reloads itself, in a mode the program chooses**
  (`cf_live`, D-48): manual, approved (a change is built and waits for
  `cf_live_approve`, and an edit after it replaces it) or automatic. A
  check sees every snapshot before readers do; a build or a check that
  fails leaves the running one serving and says why. `cf_live_get` is
  safe on any thread, and a reader keeps the snapshot it took. A key
  defined `CF_RESTART` keeps its running value, shown by explain() as
  the source `running` over the file's new value, and listed by
  `cf_live_restarts`. No thread of the library's: the loop polls, or
  `cf_live_wait` waits on a watcher. `cf::Live` in C++, with lambdas
  under C++11; `examples/c/live.c` and `live.cpp`.
- **One C++ header for every standard** (D-47): `configforge.hpp` offers
  what the standard it is compiled as can say. Under C++11 the classes
  move, have an explicit bool and derive from pairs in braces. C++17
  adds `cf::find<T>` as `std::optional` (nothing, never a truncated
  integer), `cf::value` as a `std::variant`, `cf::items` and
  `cf::children` for structured bindings, and `cf::fields` - the
  program's struct by member-pointer paths known at compile time. C++20
  adds the `cf::readable` concept, `cf::Items` as a range view,
  `std::format` of a `cf::Setting` and `std::span` pairs; C++23
  `cf::lookup<T>` as `std::expected`. Each part turns on by what the
  standard library has, so a C++20 compiler without `<format>` still
  compiles it. The header gate builds it under every standard from 98
  to 23.
- **Reading a value without repeating its default, in C++** (D-46): a
  declared `cf::Key<T>` defines the contract and reads it
  (`cfg[port]`); `cfg["server"]["port"]` is a `cf::Setting`, typed where
  it is converted, with the contract's default; and `cf::Binding<S>`
  fills the program's own struct from a field list that is also the
  contract, so the program reads `settings.server.port`. C++03, so
  every later standard has it.
- **XML, through expat, behind `CF_WITH_XML`** (D-23), which until now
  was an option that compiled nothing while `cf_has_feature("xml")`
  said yes. Read only; the root's name is in no key, attributes and
  children share one namespace, adjacent same-named siblings are a
  list, a leaf's text is trimmed and typed by the contract, and a
  DOCTYPE, mixed content, namespaces and a scattered list are refused
  by name. An XML document and the JSON written for it resolve to the
  same digest. Conan `with_xml`, vcpkg feature `xml`, fuzzed with the
  other readers.
- **A SQLite store, behind `CF_WITH_SQLITE`** - the other option that
  compiled nothing. `cf_builder_store` reads the table `configforge(key,
  type, value, meta)` as a source: each value typed by its row, and
  located as `app.db: <meta>`, so explain() says who changed a value,
  when and why, after a restart. `cf_store_set` writes typed values in
  one transaction, every item or none, and creates the store; reading
  never does. The contract applies to it as to any source. This is
  where a running program's changes go, as Design.md section 39 has
  it: hand-written files stay read only. Conan `with_sqlite`, vcpkg
  feature `sqlite`, `Builder::store` in C++.
- **A directory of secrets as a source** (`cf_builder_secrets`), one
  file a key, as Docker and Kubernetes mount secrets and systemd passes
  credentials: a secret never goes through the environment, where
  every child process can read it. A file named as its key is that key,
  one named as its environment variable would be maps the same way, a
  name that is neither is an error, the trailing newline is dropped and
  explain() names the file. Under `CF_WITH_DIR`, whose listing it uses.
- **Java `.properties`, read** (`CF_WITH_PROPERTIES`, on by default, no
  dependency): `=`, `:` or whitespace between key and value, `#` and
  `!` comments, continuation lines, the escapes and `\uXXXX` with
  surrogate pairs, as java.util.Properties reads them, in UTF-8. Keys
  are the library's keys: Spring's camelCase is Spring's to relax and
  is refused here rather than guessed at. Values are typed by the
  contract. Read only, because the format has no quoting and text
  written as 8080 would come back a number.

- **`cf_explain_json`: explain() for a machine** - one key's chain as
  JSON, for an administration endpoint, the way Spring's
  `/actuator/env` serves it. It is exactly the object the provenance
  report holds under `keys.<key>` (`cf_report_key_write` for a key
  space), redacted as the report is, so a sensitive key shows where it
  came from and its digest and never its value.
- **The report names the build that wrote it**: `library.build`, beside
  `library.version`, is `cf_build_id()`. An added field, which a reader
  of schema 1 does not object to, so the schema number stays.
- **Doctor's rules as a library** (D-41): `cf_doctor.h`,
  `libconfigforge_doctor`, of which `doctor` is now the command line. A
  program lints its own configuration in process - `cf_doctor_lint(d,
  cfg, "production", &err)` writes the full report in memory, reads it
  as `doctor lint` would and wipes it - or a report as text or a file,
  diffs two, and keeps a baseline and severities; the findings come
  back with rule, level, key, place and message. What the program kept
  in globals is a context now, and nothing in it prints. A static
  library of its own, installed as `ConfigForge::configforge_doctor`
  and left out of the amalgamation, so a device that reads
  configuration carries none of it. Every Doctor test passes against
  the program unchanged; `doctor.api` tests the library.
- **`cf_write`: a change kept, if it may be** (D-42). Before writing,
  it asks whether the source can be written (a store; nothing else),
  whether the program granted it (`cf_builder_writable`, checked at
  create, so a grant to the environment or to a misspelt source fails
  at start), and whether the contract would accept each value from it -
  the read side's own rules, so a value that would be rejected on every
  restart is never stored, and a key whose policy says "environment
  only" stays that way. Every item or none, and the error says which
  question said no. `Builder::writable` in C++. `cf_store_set` stays,
  for provisioning a store, and asks nothing.
- **Doctor from Python** (`bindings/python/src/configforge_doctor.py`):
  `lint`, `diff` and `version`, each finding with its rule, level, key,
  message, file and line. It runs the `doctor` program and reads its
  SARIF output, so a script sees what CI sees; the standard library
  only, and no ABI, since Doctor's library is static.
- **The feature-flag scenario, in C, C++ and Rust**: every feature
  shipped off, a canary drop-in turning two on, an environment variable
  killing one at three in the morning, and the flags discovered by
  iterating an open prefix rather than listed in the program;
  `explain()` says which layer turned it off. The Rust binding gained
  `children()` and `keys()` for it.
- **A Rust binding** (`bindings/rust`, D-44): a crate with no
  dependencies whose `build.rs` builds the library from the checkout with
  CMake, so `cargo build --offline` downloads nothing. A consuming
  `Builder`, `Result<Config, Error>`, reads that cannot fail,
  `explain()`, `derive()`, `Def` for a full definition; `Config` is
  `Send + Sync`. Five examples in `examples/rust`, the C ones saying the
  same lines, run by the suite under `-DCF_RUST=ON`.
- **Scenario examples, each in C and C++**: `preflight`, a service that
  lints its own configuration with Doctor's library and refuses to serve
  with a password typed as a flag; `policy`, a Windows editor whose
  administrator's Registry policy overrides the user's settings and
  alone may set the update channel. Checked by the suite for the same
  lines in both languages; `policy` where there is a Registry.
- **A renamed key keeps working** (D-43): a definition that is
  `CF_DEPRECATED` with `replaced_by` - a field nothing read until now -
  offers each value written under the old name to the new key, at the
  same rank and below the new spelling in the same source, and
  `explain()` places it `app.ini:2:1 (as db.host)`. Doctor's CFD009 says
  what to rename it to. `examples/c/migration.c` and its C++ twin.
- **`cf_build_limit`: the build's limits, by name** (D-45). The three
  that size `cf_error` - key, source, message - are part of a build's
  ABI, and a consumer that cannot see the headers the library was built
  with can now ask; the Rust binding checks all three before its first
  call and refuses a library built with others.
- **The symbol list**: `cmake/configforge.symbols` names every `CF_API`
  function, and `gate.abi` holds the headers and the built library to
  it - every declaration listed and every listed name declared, each
  defined in the library, and a shared library exporting nothing else.
  The interface is a file now, so a change to it is a line in a diff.
- **References between values, `${key}`** (D-10, decided): a value
  somebody wrote can use another key's - `url = https://${server.host}`.
  Precedence chooses every key first and references are replaced after,
  so a reference reads what a program reading that key would; the result
  is typed by the contract; `explain()` shows the template it was made
  from, and the report records it as `written`. A cycle, a reference to
  a key with no value, and a plain key showing a sensitive one are
  refused naming the key. `${HOME}`, naming no key, is still refused, and
  `$${` is still the literal.
- **The Windows Registry as a source** (`cf_builder_registry`, D-30):
  every value under a key such as `HKCU\Software\MyApp`, a subkey a
  section, names lowered. DWORD and QWORD are integers, REG_SZ is text
  the contract types, REG_EXPAND_SZ is kept unexpanded, REG_MULTI_SZ is
  a list; any other type and a set unnamed value are refused naming the
  value, and `explain()` gives each value's full path. A platform
  capability rather than an option - there is no dependency - so on any
  other platform the source fails by name, optional or not.
  `Builder::registry` in C++.
- **`doctor import-systemd`**: `systemd-analyze cat-config` or
  `systemctl cat` in, a report out. Each file is a source in the order
  systemd applies them, the later above the earlier, and each value is
  placed at its file's line - so CFD001 finds a key set twice in one
  file and CFD010 a drop-in that wins nothing. Blind unless `--full`.
- **`doctor import-spring` and `doctor import-dotnet`**, the second and
  third foreign adapters. Spring Boot's `/actuator/env` gives the
  sources in Spring's order and an origin for most properties, which
  become the report's precedence and places
  (`application.properties:1:13`); what Spring's sanitiser masked is
  sensitive, so a masked password on the command line is CFD007, and a
  variable not yet relaxed into a property is counted, not re-spelled.
  .NET's `GetDebugView()` names only the provider that won each key, so
  that report has winners and no chain, blind unless `--full`, for
  comparing environments with `doctor diff`. Neither adapter knows its
  ecosystem's precedence rules; each writes down what the ecosystem
  published (Doctor.md, section 12).

### Fixed

- **The C++ wrapper compiled only as C++98 on LP64 systems.** Under
  C++11 and later `cf_i64` is `int64_t`, which is `long` there, so
  `get<long>` and `get<cf_i64>` were one specialization twice. Every
  example and test built the header as C++98, so nothing noticed. The
  reads are an overload set now, and a test builds it as C++17 too.
- **The INI reader could refuse without saying why.** When its consumer
  refused to close a section, it returned failure and an empty error -
  breaking the one promise every reader makes, that a refusal names its
  reason. It says where now, as the TOML reader does; and it no longer
  ignores the consumer's answer at the end of a document. Found by
  feeding each codec a sink that refuses at each event in turn.

- **`explain()` ran a wide source name into its value.** A name or a
  value as wide as its column left no space before the next one -
  `features.inifalse` - in every chain with a source called
  `features.ini`. A column is always followed by a space now.

- **`cf_derive` kept the caller's source name, not a copy.** Every
  `explain()` of the derived configuration names the change's source, so
  a name passed from a buffer the caller then reused - a request
  handler's, or a Rust `String` - came back as whatever the buffer held
  next. It is copied into the derived configuration's arena, as its
  values always were. The Rust binding's first run found it.

- **A per-key policy refused the program's own default.** A key locked
  to one source (D-33) with nothing in that source had no value at all,
  so the program read the getter's fallback instead of the default it
  declared. A policy says who may set a key, and nobody sets a default:
  it now wins when no permitted source has a value, and only then, in a
  derived configuration too. Inherited candidates also keep what they
  were, so one the original refused keeps its reason.

- **CI was red, and nobody here could see it.** Two jobs had failed on
  every commit since the change-diff commit: clang-tidy refused
  `cf_diff` and `cf_diff_text` declaring `before`/`after` and defining
  `a`/`b`, and a branch chain with two identical bodies; and the shared
  build's gates had only ever been written for an archive -
  `gate.amalgamation` and `gate.install` linked a shared library
  without telling the program where to find it, `gate.install` looked
  for `libconfigforge.a`, and `gate.symbols` read crtbegin's
  `completed.0` and `__dso_handle` as this library's mutable globals and
  could not tell the allocator's file from the others in a library
  with no members. All four are fixed, and the shared build passes on
  Linux here; with `gh` now signed in, CI is watched rather than
  assumed.

### Changed

- **The interface made ready to freeze** (D-51), after an independent
  review of every public header and both bindings. Breaking, as 0.x
  may be, and the last time it can be:
  * a struct the caller allocates is filled only as far as its `size`
    reaches: `cf_error_copy` instead of whole-struct copies,
    `cf_def_init` a macro over `cf_def_init_sized` with the caller's
    size, `cf_live_status_get` accepting any size from 1.0's up, and
    the last error moved out of `cf_live_status` to
    `cf_live_last_error`;
  * `cf_source_vtable`, `cf_codec` and `cf_doc_sink` begin with a
    `size`; `cf_iter` and `cf_image` end with reserved room;
  * source flags moved to bit 16 and up (`CF_OPTIONAL` was bit 0, the
    same as `CF_REQUIRED`), and a flag a function does not know is
    refused by name; `cf_builder_replay` takes flags, and
    `CF_OPTIONAL` there means a missing report supplies nothing;
  * `cf_live_open_with` and `cf_live_options`, with a `user` each for
    build and check;
  * the internal layers are marked outside the 1.x promise;
  * each Doctor finding is its own struct, so two can be held at once;
  * C++: a build or check that throws inside `cf::Live` no longer
    unwinds through C; `Live::lastError()`;
  * Rust: `sys` hidden, `#[non_exhaustive]` on what can grow, a NUL in
    a string an error rather than a panic, a single-threaded library
    refused;
  * Python: `Config` is a `collections.abc.Mapping`, and
    `get(key, default, *, type=...)` takes its arguments as
    `Mapping.get` does; a single-threaded library is refused.
- **Branch coverage is past 80%**, and the floors follow it: 89% of
  lines and 80% of branches (`EXTRA=coverage`, CI's coverage job), up
  from 84 and 74. The tests that did it go where the refusals are: every
  facade call handed NULL or the wrong type; the core's names, values
  and contradictory definitions; each codec's malformed and edge-case
  documents, from data files, and a consumer that refuses at every
  event; the number conversions and the lexers at their limits; damaged
  reports for Replay; and every source and feature together in a fixed
  arena swept eight bytes at a time, with `cf_derive_in` the same way.
- **`CF_OPTIONAL` is the spelling of "may be absent"**: the flag now
  also means a Registry key, a store, a directory or a `.env`, so
  `CF_FILE_OPTIONAL` said less than it did. The old spelling is the same
  flag, kept, so nothing written against it changes; the documents and
  examples use the new one.
- **The examples are in a directory a language**: `examples/c` and
  `examples/cpp`, so the C++ wrapper in use is not one file among the C
  programs. Targets and tests keep their names.
- **C++ examples beside the C ones**: `tier0`, `layers`, `mistakes`,
  `runtime` and `reload` join `tier1` in `examples/cpp`, each the C
  program through the wrapper and checked by the suite for the same
  lines, so the two cannot drift. The wrapper gained what they needed:
  `Config::derive()` and `Config::write()`, `Builder::memory()` and
  `Builder::define()` for a full `cf_def`.
- **`cf_source_vtable` has a `write` member**, the source's capability,
  after `path`. A custom source sets it to NULL; one that does not
  compile until it does, by the same `-Wmissing-field-initializers`
  that made `path` a decision. `cf_write_item` moved above it.
- **Every package says who made it.** The Conan recipe's `author`, the
  vcpkg manifest's `maintainers` and the SBOM's `supplier` and
  `originator` name Mustafa Selçuk Çağlar, as the licence and the
  SBOM's copyright already did.

### Fixed, and it mattered

- **An image read out of bounds wherever size_t is 32 bits.** An image
  header's record count was multiplied by the record size before it was
  checked; on i386, armv7 or wasm32 a count of 2^28 made the product wrap
  to 0, the regions "fit", and every record past the real ones was read
  outside the image - an untrusted file, read past its end. Invisible on
  64-bit, found by the fuzz test the first time it ran on wasm32. The
  count is checked against the image's size before it is multiplied.
  The FNV prime the digests use, and two of the fuzz test's constants,
  were 64-bit literals a 32-bit long makes `long long`, which C89 has
  not; they are written as expressions.
- **A TinyCC build claimed to be safe on any thread, and was not.**
  tcc defines no `__GNUC__`, so it got no atomic operations: reference
  counts were plain additions, while the build did not say
  "single-threaded". `cf_port.h` now refuses a build without atomics
  unless `CF_SINGLE_THREADED` is set, as the support matrix always said
  it would, and `CF_FORCE_PORTABLE` - which turns the atomics off -
  requires it at configure time. The installed `cf_build_options.h`
  says when a build is single-threaded, so a program including its
  headers is too, without a `-D` to remember; cppcheck is told the
  compiler's version, and analyses the atomic path the build compiles
  rather than the one with none. Found by `cf_live`'s thread test,
  which spun for two hours under tcc.
- **`cf_live`'s lock could livelock** (never released): a lock of one
  count, taken from 0 to 1 or given back and retried, can have every
  contender step up and back forever. It is a ticket lock now, served in
  order. `cf_ref_get` is an atomic read on every path, so a loop waiting
  on it sees another thread's write.

## 0.3.0 — 2026-10-24

### Fixed, and it mattered

- **A derived configuration lost the names of its sources.** `cf_derive`
  carried the original's candidates in one snapshot source called
  "inherited", so the derived configuration knew no other source:
  `explain()` said `?` for the file and the command line and listed the
  default twice, and a configuration with a per-key policy - `{ "env",
  NULL }` for a password - could not be derived at all, because no
  source called "env" was left ("the policy names only env, and this
  build has none of them"). A run-time change to any configuration
  with a policy was impossible. The original's sources now travel by
  name, rank, kind and file, their candidates are loaded ahead of the
  new source rather than as one, defaults are not seeded twice, and the
  original's files keep the stamps they had. Found by writing
  `examples/runtime.c`; `test_derive` covers it and fails without it.

### Added

- **The version is written in one place** (D-40). `cf_version.h` is the
  source; CMake reads it before `project()`, the Conan recipe reads it,
  and `cmake -DTO=x.y.z -P scripts/version.cmake` rewrites the header,
  the vcpkg manifest and the CHANGELOG heading together. `gate.version`
  fails when any of them disagree - before, it compared two of four
  copies. Tags are `vX.Y.Z`, and the release workflow refuses one that
  is not the header's version.
- **Four more examples, each a situation rather than a function**
  (`docs/Guide.md` has a table of all of them): `mistakes` (what the
  person running the program reads for each kind of mistake, and the one
  a contract may forgive), `secrets` (a password only the environment may
  set, refused from a file, redacted everywhere, wiped), `plugins` (a
  strict contract with one prefix handed over, discovered by iteration)
  and `runtime` (an admin's change in a running service, explained the
  next morning). Each is checked for its output by the suite.
- **`cf_build_id()` and `doctor --version`**: the commit a build comes
  from, `20d0f4b` or `20d0f4b-dirty`, refreshed on every build and
  carried by a `git archive` source tarball; empty when the build
  cannot know.

## 0.2.0 — 2026-10-24

### Added

- **Releases from a tag** (`.github/workflows/release.yml`): the source,
  the amalgamation, and the library with Doctor for Linux, Windows and
  macOS, each built and tested on its platform before it is packaged,
  with `SHA256SUMS` and notes taken from this file's section for the
  version. The Linux build is musl in Alpine, linked statically: one
  Doctor binary for any Linux. The Windows Doctor is linked against the
  static C runtime, so it needs no Visual C++ redistributable, while
  the library keeps the DLL runtime that programs linking it expect. A
  tag is refused before anything builds if it disagrees with
  `project(VERSION)` or if its section here still says "unreleased".
  Running the Linux and Windows jobs on a workstation before the first
  tag found Doctor over the stack budget at `-O3` and the suite not
  building in MSVC's Release, both fixed below.
- **The development environment as a file** (`scripts/buildenv`): one
  list of every tool the build, the gates and each `EXTRA` step use,
  `setup.sh`, and two ways to have it - a container (`Containerfile`)
  and, for Windows, a WSL distribution whose disk is on the drive you
  name (`wsl-distro.ps1`), so builds do not grow C:. `freebsd.sh` boots
  a throwaway FreeBSD machine inside it - FreeBSD's CI image, emulated
  where there is no KVM - and runs the unit tests against the real
  kqueue, which until now only macOS CI saw: all 41 pass on FreeBSD
  14.4, after the fix below that its first build found. Downloads come
  from the projects that publish them and are checked against their
  published checksums or signatures.
- **A usage guide and the examples it walks through** (`docs/Guide.md`):
  getting the library into a build, a contract, sources and precedence,
  asking why, errors, formats, secrets, reloading, the editor and
  Doctor, small targets, C++ and threads. Four new examples, each run
  by the suite and checked for the lines that show it does what its
  comment says: `layers` (six sources at once, and why each value won),
  `reload` (watch, rebuild, log what changed), `embedded` (a fixed
  buffer and an image, no heap) and `schema`.
- **Any C89 compiler, any C library**, checked rather than claimed:
  `EXTRA=compilers` in `verify.cmake` and a CI job run the whole suite
  against musl (`musl-gcc`) and with the Tiny C Compiler and the Portable
  C Compiler, and an Alpine job runs it on musl as a distribution ships
  it. All three pass. pcc found that `cf_port.h` trusted `__GNUC__`,
  which pcc defines without having GCC's builtins (fixed by `CF_GNUC`),
  and that its lexer reads `"\\u"` as a universal character name (two
  messages split after the backslash). tcc showed that it puts `const`
  data in `.data`, which the symbol gate now says and skips for it, and
  that it accepts `-fstack-usage` without measuring anything, so the
  stack budget is checked with GCC and Clang only. `CF_WERROR` (on by
  default) lets a compiler that warns at the C library's own headers -
  pcc at glibc's `__cold__` - build with its warnings shown.
- **Conan and vcpkg packages**, built from the checkout and consumed by a
  project that knows only the package, in CI and here: `conanfile.py`
  with every `CF_WITH_*` as an option (`with_yaml` brings libyaml from
  ConanCenter), static and shared; and an overlay port in
  `packaging/vcpkg` with a `yaml` feature. The installed headers state
  the build's options, so neither recipe passes a define a consumer
  could disagree with.
- **YAML** (D-39, `CF_WITH_YAML`, off by default: the one codec with a
  dependency). Read through libyaml's event parser, which maps onto the
  document events one to one, and written by hand in block style. A
  plain scalar is text until a definition types it, a quoted one is text
  and says so, and with the library's one spelling of a boolean (D-24)
  `country: no` is the text "no" under YAML 1.1 and 1.2 alike - the
  problem every YAML configuration has met, answered by the contract
  instead of by whichever schema a reader picked. Anchors, aliases,
  tags, several documents and non-scalar keys are refused by name. A
  `.yaml` file in a build without it names the option, as every disabled
  format does. `test_yaml` reads, refuses and round-trips.
- **An API reference that cannot fall behind** (`docs/API.md`): every one
  of the 241 public functions with the comment written above it, under
  its header's section banners, generated from the headers by
  `scripts/apiref.cmake`. `gate.apiref` regenerates it and fails when it
  differs from the committed file, so documenting a function means
  writing its header comment, and nothing else can disagree with it. No
  Doxygen: the comments are prose and stay prose.
- **The fuzz corpus is kept**: each reader's inputs that reached new code
  are written to `corpus_<reader>`, which the next run starts from; CI
  caches the directories and minimises them with `-merge=1` before
  saving, so they move forward without growing with every run.
- **Coverage-guided fuzzing** (`fuzz` preset, `CF_FUZZ`, `EXTRA=fuzz`, a
  CI job a minute per reader): libFuzzer harnesses for JSON, INI, TOML,
  `.env`, the image, Report Replay and the command line, under the
  address and undefined-behaviour sanitisers. They are `test_fuzz.c`'s
  own readers fed by libFuzzer instead of a fixed generator, and they
  start from its corpora, so there is one corpus and not two; a finding
  leaves its input as `crash-<hash>`, which CI keeps.
- **Coverage, measured and held** (`coverage` preset, `EXTRA=coverage`
  in `verify.cmake`, a CI job): 85.4% of the library's lines, 93.3% of
  its functions and 74.8% of its branches are reached by the suite.
  The step fails below `COVERAGE_MIN`, 84% of lines, or
  `COVERAGE_BRANCH_MIN`, 74% of branches - ratchets, to be raised as
  tests are added, not targets to reach once.
- **pkg-config**: an installed, relocatable `configforge.pc` - the prefix
  is found from where the file is, so an install moved with `--prefix`
  still describes itself - for builds that are not CMake. The install
  gate now also builds a consumer from what `pkg-config` says, and from
  nothing else.
- **A thread-sanitiser preset** (`tsan`, `CF_SANITIZE_THREAD`; `EXTRA=tsan`
  in `verify.cmake`, a CI job): the threading contract (D-14) under a race
  detector rather than reasoned about. The whole suite passes, `test_threads`
  included. On Linux 6.x the runtime cannot place its shadow memory when
  mmap is randomised with 32 bits, and every test dies before its first
  line; the step runs with randomisation off, as the CI job does.
- **Doctor as a GitHub Action** (`actions/doctor`): builds Doctor from the
  checkout, lints a report, prints the findings and writes SARIF for
  `upload-sarif`. Inputs reach the shell through the environment, never
  pasted into the script, because a path from a pull request is text
  somebody else wrote. A CI job uses it on a fixture report.
- **The contract as a JSON Schema** (`cf_schema`, `cf_schema_write`;
  C++: `Config::schema`), Design.md section 41: a configuration file
  that names it gets completion, inline documentation and validation in
  an editor, from the definitions the program already declared. Types,
  ranges, lengths, enumerations, descriptions, deprecation; a sensitive
  key is `writeOnly` and its default is left out. Objects are closed
  (`additionalProperties: false`) when the configuration rejects unknown
  keys, except under a prefix opened with `cf_defset_open`, so an editor
  flags a misspelt key as the program would. `required` is not written:
  a required key may come from the environment, so a file without it is
  not wrong, and a schema that said so would flag correct files.
  `test_schema` walks the result with the library's own JSON reader.
- **What a reload changed** (`cf_diff`, `cf_diff_text`; C++:
  `Config::changesSince`). One pass over two sorted key spaces, a line
  per changed key with where the new value came from - the line a
  program that watches its files should log, instead of two dumps side
  by side. A value that is the same but now comes from somewhere else
  is a change of its own kind: the configuration means the same thing,
  and the file an edit would have to touch is a different one. Secrets
  read `changed, <sensitive>` - not even their lengths, since a changed
  length is a fact about the secret. Proved by disabling the origin
  check: four of `test_diff`'s checks failed.
- **Parts of the library can be left out** (D-38): `CF_WITH_DIR`,
  `CF_WITH_DOTENV`, `CF_WITH_EMIT` (which takes every encoder with it),
  `CF_WITH_IMAGE` and `CF_WITH_REPORT`, all on by default. Off, the
  functions stay - a program links against any build - and each refuses
  by name, the rule a disabled format already followed. Everything
  optional off takes the library from 92 KB to 58 KB at `-Os`.
  `test_features.c` checks every part both ways, and the `minimal`
  preset turns them all off. Every `CF_WITH_*` is now listed once, in
  `CF_FEATURES`; the amalgamation gate had been given only the formats
  and none of the limits, so under `minimal` it compared two different
  libraries as if they were one.
- **Limits and the stack budget are the builder's** (D-37):
  `CF_KEY_MAX`, `CF_MESSAGE_MAX`, `CF_PATH_MAX`, `CF_MAX_DEPTH` and
  `CF_STACK_BUDGET` are build options, the limits written into the
  generated `cf_build_options.h` because they shape the public structs.
  Every library frame is measured with the compiler's own figures after
  the link (`scripts/stack.cmake`), checked against the budget, and
  reported by feature - "what fits in a kilobyte" is answered with
  options, and the same script answers "what if" for another budget
  without a rebuild. One nesting limit replaces three that disagreed:
  JSON accepted 64 levels that the flattener then refused at 32. The
  `minimal` preset runs the suite at 64-byte keys, 128-byte messages and
  256-byte paths.
- **Binary snapshot image** (`docs/ImageFormat.md`, decision D-28). Loads
  without parsing and without allocating, which is what a target with no
  filesystem and no heap needs and what the freestanding profile was
  missing. Optionally carries the provenance chain, so a device with no
  filesystem can still answer why a value is what it is.
- **`src/bridge`**: the flattener, the file source and the image, which
  are the only components that legitimately use both the configuration
  model and the serialization layer.
- **Flat lexers separated from recording.** `cf_lex_env` and
  `cf_lex_cli` produce entries into a sink and need no arena, no key
  space and no source object, so they are tested against an array.
- **Key mappings** (`cf_keymap`), which made the reverse direction
  possible and closed D-06: with a definition set the environment
  accepts `MYAPP_SERVER_PORT` as well as `MYAPP_SERVER__PORT`.
- **The command line's remaining forms** (D-29): `--key value`,
  `--no-key`, and aliases such as `-p`. All three need to know whether
  an option takes a value, which is the one thing a command line cannot
  be parsed correctly without, so all three arrive with a definition set
  and none exist at Tier 0. The lexer asks through a small spec
  interface rather than taking the definitions, which is what lets a
  test answer the questions with no definition set existing.
- **`.env` file source.** An environment written down, so it takes the
  same key mapping and `export NAME=value` is accepted and ignored.
  Unlike the environment it belongs to this program, so a name that
  derives no key is an error with a line number rather than somebody
  else's variable.
- **`@file` response files**, because the Windows command-line length
  limit is real and this is the universal way around it. Expansion is a
  pass of its own, so the lexer keeps needing nothing but its arguments.
  Nesting is refused: a response file that includes another is a command
  line nobody can follow from the one they typed.
- **`cf_cli_args()`**, which returns the arguments in UTF-8. On Windows
  `main`'s argv arrives in the active code page, so a value containing
  any non-ASCII character is wrong before any parser sees it — and what
  the code page dropped cannot be converted back, which is why it reads
  `GetCommandLineW` rather than converting argv. Identity everywhere
  else.
- **Drop-in directory source** (`conf.d`), which is how systemd, sudo
  and apt let a package add configuration without editing a file it does
  not own. Files are read in byte order at one shared rank, so
  `10-base` losing to `90-site` is precedence rather than a merge rule
  and `explain()` names the file that won — which is the ordering that
  fell out of last-occurrence-wins rather than needing a rule of its
  own. Dotfiles are skipped, because an editor's `.swp` and a package
  manager's `.dpkg-new` both land in such a directory.
- **Prefix iteration** (`cf_iter_keys`, `cf_iter_children`) and array
  helpers. The questions a program actually asks are rarely "what is
  server.port" — they are "what are the elements of this array" and
  "which plugins are configured", and both needed the caller to build
  keys by hand and guess where to stop. Both are now a scan from one
  binary search, and nothing is allocated.
  A prefix matches at a segment boundary only, so `server` does not
  match `servername` — the bug every hand-written prefix check has.
- **Per-parameter policy** (D-33). A definition can carry the source
  names permitted to supply that key, in decreasing precedence —
  `{ "env", "user-file", NULL }`, which reads as the specification
  writes it. One list restricts and reorders at once, which is how an
  operator-controlled setting survives a command line trying to override
  it.
  A violation rejects the candidate rather than failing construction:
  refusing to start would let any user stop the program by passing a
  locked option, and for a secret on the command line the damage is done
  the moment it is typed. It is reported instead — in `explain()`, in
  the report, and at error severity in Doctor.
- **`cf_emit`**, which writes a configuration out — restricted, as the
  specification says, to a file ConfigForge owns and rewrites in full.
  `CF_EMIT_CONFIGURED` is the one that matters: it writes what a source
  configured and leaves out what nobody touched, so an overrides file
  stays small and a later release can still move a default.
  "Configured" means "did not come from the defaults source" rather than
  "differs from the default value". The two part company when someone
  sets a value that happens to equal the default: comparing values would
  drop it and let a later release move it, while provenance keeps the
  intent that was expressed.
  An array is all or nothing — emitting only the members that pass a
  filter would produce a shorter array, which is a different
  configuration rather than a subset of one. And a format with no array
  syntax refuses before writing anything, keeping the promise the INI
  encoder made.
- **Open prefixes** (`cf_builder_open`). Unknown-key rejection and a
  dynamic key space were mutually exclusive: a plugin's settings cannot
  be in the program's contract because the plugin is not in the program
  either. An exempt prefix is a hole of a stated size — everything
  outside it is still checked, and the prefix matches at a segment
  boundary so opening `plugins` does not open `pluginsomething`.
- **`CF_TRANSIENT`**, for a key that is a command rather than a setting.
  `--cf.explain` asks a question; a file that recorded it would ask the
  question again on every run that read the file.
- **The C++03 wrapper now covers iteration, emitting and reporting.**
  A wrapper that covers half its library is worse than none: it lets
  someone believe they can stay in C++ and then drops them into C.
  Iteration is `begin`/`end` over a real iterator type, which is valid
  C++03 and gives a C++11 consumer range-for over the same interface.
- **The threading contract is settled and written into the public
  header** (D-14): reads are safe concurrently, building and releasing
  are the caller's to synchronise. An unstated threading contract is
  assumed to be full internal locking by users and nothing at all by
  implementers.
- **Snapshots are reference counted** (`cf_config_ref`), which is
  stronger than the contract requires and gives the reload story a
  foundation. Retrofitting ownership later is the same class of mistake
  as retrofitting the provenance chain.
- **`cf_derive`** (D-34), which is what setting a value means when a
  snapshot is immutable: it returns another one. The original stays
  valid and unchanged, so a reader holding it does not have it move
  underneath them and a thread reading while another sets needs no lock.
  Provenance grows rather than collapsing. The change arrives from a
  source of its own above the command line, and the displaced value
  stays in the chain — so `explain()` afterwards says what it is now,
  what it was, and where that came from, which is the question somebody
  asks right after changing a setting.
  The contract travels with it: definitions, open prefixes and the
  unknown-key policy are inherited, because a derived configuration that
  accepted what the original refused would be a different contract
  wearing the same name.
- **Change notification** (D-35): `cf_changed`, `cf_watch_count`,
  `cf_watch_path`. Pollable, never a callback — a library that owns a
  thread and calls you back has made a threading decision that belongs
  to the caller, and calling back while that caller reads is the race
  the threading contract exists to avoid.
  A conflict the design expected did not appear. It predicted that
  enabling watch would change `cf_get_text`'s lifetime contract; because
  snapshots became immutable and reference counted for other reasons, a
  reload produces a *new* snapshot and the old one stays valid while
  anything holds it. Watching changes no contract at all.
  What is compared is modification time and size, which misses a file
  rewritten inside one clock tick at the same length. That is documented
  as a hint worth acting on rather than a guarantee; the exact answer is
  to rebuild and compare digests.
- **A codec from outside the library**: `cf_builder_file_codec` (C++:
  `Builder::fileWith`) reads a file through a `cf_codec` the caller
  defines - over libyaml's events, expat's callbacks, any streaming
  parser - since the document events are already format-neutral. It is
  handed over rather than registered, because a run-time registry would
  be global mutable state. `test_codec.c` is a complete one written
  against the public headers alone, and the values it produces are
  typed, validated and explained like any other.
- **A way in for whoever continues the work.** A guide for any
  contributor, on any machine, maps the repository, gives the
  build-verify-commit loop, the house rules and every trap this work
  fell into, and the recipes for extending it (now `CONTRIBUTING.md`).
  `docs/Roadmap.md` holds what is next, what is known to be wrong, and
  the ideas not yet built, and is kept in the same commit as the work.
  `CONTRIBUTING.md`, `SECURITY.md` (private reporting, and what is in
  scope: every reader of untrusted bytes), `CODE_OF_CONDUCT.md`, issue
  and pull request templates, and Dependabot for the CI actions.
- **Usage as a test.** `tests/unit/test_usage.c` is the smallest real
  program for each usage tier, compiled and run, so the tiers cannot
  name an interface that does not exist - which Design.md's did: its
  Tier 1 and Tier 2 examples called functions from an early draft that
  the library never had. They show the real API now.
- **jq recipes for reports** (Doctor.md, section 7a), each run against
  a real report, and the reasoning for not writing a query language of
  our own (Roadmap.md).
- **TOML**, as the subset D-23 described, built in and on by default:
  tables and arrays of tables, inline tables, dotted keys, basic and
  literal strings with every escape, integers in four bases, floats,
  booleans and arrays, each value with its line and column. Dates,
  multi-line strings and inf/nan are refused with a message saying what
  to write instead. It reads, it writes for `cf_emit`, and what it
  writes reads back as the same configuration; it is in the fuzzer and
  the amalgamation like the other readers.
  The round trip found that the emitter wrote arrays after tables,
  which TOML cannot express - an array after a sub-table's header lands
  in the sub-table. Arrays come before tables at every level now, which
  every format with arrays writes inline.
- **A handle an event loop can wait on** (D-35, option 3): `cf_watcher`,
  an inotify descriptor on Linux and one event HANDLE on Windows, plus
  `cf_watcher_wait` for a program with no loop. Still no callback and no
  thread. It watches each file's *directory*, because editors save by
  renaming a new file over the old one, so a wake-up means "ask now" and
  `cf_changed` remains the answer. A directory that does not exist yet
  is watched through its nearest ancestor and the watch moves down as
  the levels appear, so an optional file nobody has created can still
  wake the caller when it arrives. On macOS and the BSDs it is a kqueue,
  which watches what it has open rather than a name, so it watches each
  file as well as its directory and reopens everything on every drain.
  `CF_WAKE_KQUEUE` builds that path on Linux against libkqueue, and CI
  runs it there on every commit as well as on macOS.
- **Doctor proves CFD001, CFD007 and CFD013.** CFD001, an unreachable
  value, fires where it can be proved from one report: the same source
  sets a key twice in one file or command line, the later occurrence
  wins wherever it is read, and editing the earlier line has no effect.
  The broader reading - every shadowed candidate - cannot be proved from
  one environment and would bury the finding under precedence working
  as designed. CFD007 is a secret on the command line that no policy
  refused; CFD013 a policy that ranks a less specific source above a
  more specific one, which is what explains an `explain()` where the
  command line loses to a variable.
- **Doctor can be adopted on a project's own terms.** `--severity`
  reads a file of `CFD010 warning` lines, with an optional profile -
  `CFD008 error production` - and `off`; `--fail-on` sets the threshold;
  `--format sarif` writes SARIF 2.1.0 so a forge shows each finding on
  the line that caused it. Doctor.md had asked for all three from the
  first release, because a lint tool that cannot be tuned is turned off.
  Profiles arrive the way D-12 decided: as a label a rule's severity can
  follow - CFD008, "default-only in production", is a warning there and
  information elsewhere - not as a concept in the library.
  CFD010 is a warning by default now. A system file wholly overridden by
  a user's is often what somebody meant, and an error made that a
  failed build.
- **`doctor import-git`**, the first foreign adapter. It turns
  `git config --list --show-origin --show-scope` into a provenance
  report through the library's own writer, so every Doctor command runs
  on git's configuration unchanged - a scope that wins nothing is CFD010
  there too. Blind unless `--full`, because git configuration carries
  credentials more often than it looks; a key a report cannot carry is
  left out and counted rather than mangled into one that collides.
- **A licence: zlib.** A binary built with the library, firmware
  included, owes nobody a notice; every source distribution keeps one,
  so the single-file amalgamation carries it at its top. Installed with
  the headers. The options weighed are D-36.
- **A bill of materials.** `docs/Dependencies.md` lists every
  dependency with its licence and when it is needed - the default build
  needs none - and each build generates an SPDX 2.3 SBOM from the
  options it was configured with, installed beside the licence.
  `gate.dependencies` fails when the build looks for something the page
  does not name, and `gate.sbom` when the SBOM does not parse or
  disagrees with the project's version and licence.
- **CFD007 sees files.** A report records, for a sensitive value that
  came from a file, whether every user of the machine can read that
  file - the library records it because Doctor judges other machines'
  reports and must never look at its own disk. Windows cannot say with
  a single bit, so it says nothing rather than guessing.
- **The C++03 wrapper covers change notification**: `Config::changed()`
  and `Watcher`, which owns the handle and is not copyable - two owners
  of one descriptor is a double close - plus the `dir()` and `dotenv()`
  sources it had been missing.
- **Three more Doctor rules** that a single report can prove from its
  definitions alone: `CFD005` a key with no definition, `CFD009` a
  deprecated key something is still supplying, `CFD011` a key set to an
  empty value — which is not the same as being unset, and is usually a
  variable that expanded to nothing rather than a decision.
- **ConfigForge Doctor**: `dump`, `explain`, `lint`, `diff`, and
  `lint --baseline`, which records existing findings as accepted so that
  only new ones fail. A tool that cannot be adopted gradually is a tool
  nobody adopts.
- **Provenance report** (schema v1) and **Report Replay**.
- **C++03 wrapper**, compiled as C++98 by a gate.
- **Amalgamation**, compiled and run by a gate.
- `cf_version()`, `cf_feature_at()`, and a gate that fails when the
  header version and the project version disagree.
- **A deterministic mutation fuzzer** over the codecs, the `.env` lexer
  and the binary image. The seed is the bug report; the generator is
  written out rather than taken from `rand()`, which is reproducible on
  no two machines.
- **`gate.formats`**, which checks every format string in the library
  against what `cf_fmt` actually supports. `cf_fmt` has no width,
  precision or flags, and an unsupported conversion is copied out
  literally — which is how every provenance report carried `%08x` for
  many commits. Proved by putting that bug back.
- **`test_complexity`**, which measures the claims that are shapes:
  resolution at one size and four times it, where `O(n log n)` and
  `O(n squared)` are 4.3x and 16x apart. A regression from a binary
  search to a scan would have passed every other test here.
- clang-format and clang-tidy as build targets and gates.
- cmake-format and cmake-lint, so the build files get the same treatment
  as the C sources.
- **A compiler-verified warning standard** (D-32). The flags are grouped
  by what they protect rather than by which compiler has them, and each
  one is put to the compiler at configure time with `-Werror` set, so a
  compiler that merely warns about an unknown `-W` flag still fails the
  check. No table to keep true.

### Changed

- **A value containing `${` is refused** (D-10). References to other
  values are not supported yet, and taken literally,
  `cache.dir = ${HOME}/cache` created a directory called `${HOME}` -
  and the day references arrive, every such configuration would have
  changed meaning without a word. It is an invalid value like any other:
  construction fails and says where, unless the parameter asked for
  fallback, in which case `explain()` shows it refused. `$${` is a
  literal `${`. A program's own default is code rather than
  configuration and is left alone, and a replayed report is not refused
  the second time round. A `.env` written for a tool that does expand
  `${VAR}` now fails loudly rather than silently meaning something else.

- **Within one source, the later occurrence of a key now wins.** It was
  the first, which is the convention nowhere: a command line where
  `--port=1 --port=2` gives 2, an INI file where the second assignment
  wins, a drop-in directory where `90-site` overrides `10-base`. Found
  by an end-to-end run where a response file beat the arguments typed
  after it.
  The environment is a set and has no order of its own, so where both
  the escaped and the natural spelling are present the escaped one wins
  by construction rather than by which pass happened to run second.

### Fixed, and it mattered

- **The library did not build on FreeBSD.** The stack check reads the
  `.su` files `-fstack-usage` writes, and expected GCC's
  `file:line:column:function`; FreeBSD's clang 19 writes
  `file:line:function`, so nothing was read and the check failed the
  build on an empty table. Both forms are read now, and a file in
  neither form fails with its first line quoted rather than an internal
  error. Found by the first build in a real FreeBSD machine
  (`scripts/buildenv/freebsd.sh`).

- **Doctor did not build at CMake's own Release settings on Alpine.**
  At `-O3` Alpine's gcc inlined every lint rule into `main` - each is
  called exactly once - without sharing their buffers, and `main` came to
  4416 bytes against a budget of 2048; `-Werror` made that a failed
  build. Ubuntu's gcc at the same level, and every preset (none of which
  is `-O3`), stayed under, which is why only the release job found it.
  The rules now run from a table through a pointer, which no compiler
  inlines into one frame, and `main` is 1328 bytes there. The CI Alpine
  job now builds Release, so it is checked before a tag rather than at
  one.
- **The suite did not build in MSVC's Release.** `test_numeric` makes
  infinity and NaN by dividing by a zero in a variable, since `INFINITY`
  and `NAN` are C99; the optimiser saw the constant, warned "potential
  divide by 0" (C4723), and `/WX` made it an error. Every Windows preset
  is Debug, so only the release job would have built it. The zero is
  `volatile` now.
- **`test_complexity` failed on correct code under load.** It compared
  one key space of 4000 keys against one of 16000, and on a 12 MiB cache
  the first fit and the second did not, so the ratio measured the cache:
  8 to 12 times for fourfold data, against a bound of 8. It now resolves
  the same total number of keys as many small spaces or a few large
  ones, both in cache, so the ratio is the cost per key: 0.8 to 1.7 for
  correct code under any load tried, about 4 for a quadratic one, and
  the bound is 2.5.

- **An image's provenance travelled and nothing read it.** With
  `CF_IMAGE_PROVENANCE` the writer stored every candidate - source,
  outcome, value, location - and the reader loaded the winners and said
  "image" for all of them, so the reason to carry the chain, a device
  with no filesystem still able to say why a value is what it is, was a
  claim. Loading now takes each value's location from its winning
  candidate: on the device, `explain()` says `release.ini:2:1`, the line
  on the build machine. Found by writing `examples/embedded.c`, whose
  comment said so before the code did. With the lookup disabled,
  `test_image` fails.

- **Frames that were larger than they needed to be**, found by the stack
  report on its first run. The flattener kept 24 bytes for each of its
  32 nesting levels where 8 hold the same (a 32-bit index, a 16-bit
  length, a flag): 512 bytes off every frame that reads a document,
  `dir_load` 1344 to 832 at short limits. The inotify drain read into a
  kilobyte where two of the largest records are enough, the loop
  draining until empty either way. `cf_emit` held a fixed kilobyte for
  encoder state; `CF_ENCODER_STATE_MAX` follows the limits now, and each
  encoder asserts at compile time that it fits. The INI encoder had its
  nesting fixed at 32 regardless of the limit. Without floating point
  and with 64-byte keys, every library function now fits in a kilobyte.

- **The TOML decoder broke the stack budget**, by four times: thirty-two
  open tables and two dotted keys each held a key-sized buffer per name,
  8.7 KB in one frame (10.9 KB once `-Os` inlined the callers). Linux
  CI had been red since TOML landed, because MSVC, where it was tested,
  has no stack check. The names now share one key-sized buffer, which
  costs nothing: the whole path has to fit in one key anyway. The
  `portable` and `minimal` presets now build optimised (`-O2`, `-Os`),
  so the inlined case is checked on every verify; that also found a
  `system()` result that a `(void)` cast does not silence under glibc's
  fortified headers.
- **TOML did not compile without floating point** (`CF_WITH_REAL=OFF`,
  the `minimal` preset): its number buffer was sized by the float
  digits. It is sized by `CF_VALUE_TEXT_MAX` now, and a float in such a
  build is refused by name, which `test_toml` checks.
- **`cf_ks_resolve` could fail silently.** It allocated a scratch array
  for the merge sort, set the key count to zero when there was no room,
  and returned `void`. Between roughly 11.7 and 12 kilobytes of fixed
  arena, `cf_builder_create` therefore *succeeded* and handed back a
  configuration with no keys in it — a device in that window would have
  booted with every value at its default and no indication that anything
  had gone wrong, which is worse than not booting.
  It returns `cf_bool` now and the builder reports it, naming the number
  of candidates it could not sort.
- **`cf_build_in`**, which was missing. `cf_arena_fixed` existed and
  nothing in the builder accepted an arena, so the claim that a target
  with no heap can hand over a buffer and have the library work inside
  it had no way to be true. The arena is borrowed: `cf_config_free`
  never frees a buffer this library did not allocate.

### Interface safety

- **An installed library now ships facts about itself, not defaults.**
  The `CF_WITH_*` options were defaulted in `cf_port.h`, so a consumer
  compiling with a plain `-I` against an installed library saw
  `CF_WITH_REAL` as 1 even when the library had been built with 0. That
  is not a link error in general: `CF_WITH_REAL` decides whether
  `cf_value`'s union holds a double, so the two sides disagree about
  `sizeof` and the corruption surfaces somewhere else entirely. The link
  error that revealed it was the lucky outcome.
  The defaults moved to `cf_build_options.h`, which exists twice on
  purpose: the source tree's copy holds `#ifndef`-guarded defaults for
  someone compiling the sources, and the copy generated at configure
  time states this build's values unconditionally and is the one that
  gets installed. `gate.install` compiles a consumer with a plain `-I`
  and checks both the headers and the compiled library agree — proved by
  making the generated header lie.

### Security

- **explain() and inspect() printed sensitive values in the clear**, and
  so did the reason a sensitive value was refused - which explain()
  shows and a strict definition raises as an error. Design.md said
  secrets must not appear in diagnostics; nothing checked it. The chain
  is still shown in full - which source won, which lost, where each was
  set - and each value of a `CF_SENSITIVE` key reads
  `<sensitive, N bytes>`, what a blind report gives. A refusal names
  the range or the type and not the value. `test_secrets` checks all of
  it; with the redaction disabled it fails five times, printing exactly
  the leak it is there to prevent. `cf_ks_explain` and `cf_ks_inspect`
  stay the raw renderers for a key space with no definitions;
  `cf_ks_explain_redacted` and `cf_ks_inspect_redacted` take them.
- **Freed memory no longer holds the configuration's secrets.** An
  arena now zeroes every byte it handed out before giving it back -
  through a volatile pointer, because a store to memory about to be
  freed is the textbook dead store an optimiser removes. That covers
  every value and the raw text of every file read, on a successful
  build, a failed one and an abandoned one; `cf_arena_wipe` does the
  same for a caller-owned arena (`cf_build_in`). `test_wipe` watches
  every block as it is released and searches it for the secret; with
  the wipe disabled, all three of its cases fail. A first version also
  tested an abandoned builder, which never holds the secret because
  nothing is read before create - a test that passed either way, so it
  was replaced by a build that fails after reading.
- **The binary image reader dereferenced a file-supplied index without
  bounding it.** Every region the header describes was validated against
  the image's length; the value index inside each entry was not, so a
  corrupt image could read anywhere. Found by a mutation fuzzer on its
  first run — the image reader is the one reader here that dereferences
  offsets taken from its input, which is exactly why it was the most
  valuable thing to fuzz.
  Bounds are now written as subtraction rather than addition, because
  two 32-bit values and a one can wrap where `size_t` is 32 bits and a
  wrapped bound passes every check it was meant to fail. The chain
  region is validated too, so a reader added later does not inherit an
  unchecked offset.

### Fixed

- **A path outside the Windows code page could not be opened.** The
  platform layer called the code-page ("A") functions and the file
  reader called `fopen`, so a configuration under a name the active code
  page cannot spell - a Greek letter on a Turkish or Western machine -
  was unreadable, unstampable and unlistable. Paths are UTF-8 at the
  interface (D-15) and now UTF-16 to Windows: every path goes through
  one conversion into a "W" function, and files open through
  `cf_platform_fopen`, which is `_wfopen` there and `fopen` everywhere
  else. `test_unicode` builds such a tree and reads it as a file, a
  drop-in directory and a change check; against the old calls it fails.
  Its first version used only Turkish letters and passed against the
  old code too, because this machine's code page spells them - the
  alpha in its directory name is the part that proves anything.
- **A shared library was unusable on Windows and leaked internals
  elsewhere.** Nothing was marked for export, so a DLL exported nothing.
  Every public function is `CF_API` now - dllexport building the DLL,
  dllimport using it, `visibility("default")` under
  `-fvisibility=hidden` on ELF and Mach-O - and whether the build is
  shared is stated in the generated `cf_build_options.h`, so a consumer
  of an installed DLL is told rather than having to know. A CI job
  builds and runs the whole suite as a shared library on all three
  systems.
- **The Windows build had never been built.** The first MSVC run failed
  to compile `win32.c` - `_environ` declared by hand collided with the
  CRT's macro - and two `/W4` warnings stopped the tests and Doctor. The
  suite then failed one test for the right reason: `cf_cli_args` reads
  the real wide command line on Windows, and the test handed it an
  invented argv. It now uses the process's own arguments, and
  `unit.test_lex.utf8` asserts the non-ASCII round trip the wide path
  exists for. The four gates that drive the compiler with a GCC-style
  command line are registered only where one exists.
- **A drop-in directory edited in place was not a change.** `cf_changed`
  stamped a directory by its own modification time, which moves when a
  file is added or removed and not when one is rewritten. A directory is
  now stamped by what it holds.
- **The `analyze` preset had never analysed anything.** cppcheck defines
  none of the compiler's macros, so `cf_port.h` stopped it at its
  `#error` for want of a 64-bit type in every translation unit. Told
  which compiler it is looking at, it analyses the library clean; the
  tests get its warnings and not its style class, which calls a test
  asserting a constant on purpose "a condition that is always true".
- **The clang job could not have passed.** Clang 18 rejected the fixed
  arena's casts under `-Wcast-align`, and clang-tidy 18 failed the
  library on nineteen findings, two of them analyzer paths through an
  image that had not opened and an explain buffer that was NULL with a
  capacity. Both are closed - the image is zeroed before anything can
  fail, a NULL buffer always measures - and the rest were one-line
  fixes. clang-tidy now checks the library and Doctor, not the tests
  and examples, whose forty findings were unchecked `printf` results.
- **The C format gate depended on the formatter's version.**
  clang-format 18, which CI installs, and 22 disagreed on three lines;
  they are written so that both leave them alone.
- **One leaked secret was three Doctor errors.** A candidate rejected by
  policy was also counted as a contract violation (CFD006) and as a
  source that lost (CFD010). A rejection is reported once, by the rule
  that owns it.
- **Rounding carry did not travel past the last digit.** The formatter
  handled a carry out of the leading digit and silently produced `"1:"`
  when the last one overflowed. Found by clang-tidy noticing the dead
  store that was the symptom.
- **Digest was the literal format string.** The report wrote `%08x%08x`
  through a formatter that has no width specifiers, so every report
  carried `fnv1a:%08x%08x`. The test only checked the prefix.
- **Emission put root keys after the first section**, so an INI file
  written by the library read back as a different configuration. Fixed by
  ordering leaves before containers at every level, which fixes it for
  every format rather than in one codec.
- **Key space insertion was quadratic.** Two-phase construction with one
  merge sort at resolve.
- **GCC-only warning flags were handed to every compiler**, which would
  have broken a real clang build. Found by clang-tidy's front end.
- Arena leak in Doctor, unaligned fixed arenas, a key space that kept
  working after a failed init, `-Wundef` on a feature macro visible in
  one translation unit and not another.

### Notes

- `cmake-format` with `keyword_case: upper` rewrote the *variable* in
  `get_filename_component(name "${f}" NAME)` to `NAME`, because the token
  matches a keyword of that command, and silently broke two gates. The
  setting is now `unchanged`: a formatter that can change meaning by
  spelling gets one job, whitespace. The gates caught it, which is the
  argument for having them.

### Removed

- **`CF_WITH_WATCH`**, which switched nothing. It was off by default, so
  `cf_has_feature("watch")` said no on every build while `cf_changed`
  and `cf_watcher` worked on all of them. The feature list now reports
  "watch" where the platform has a handle to wait on, which is the only
  thing the word could honestly mean.

- The hand-written `Makefile` (D-31). The no-build-system path is the
  amalgamation, which is generated from the real sources and tested on
  every commit rather than merely present.

## 0.1.0

Milestone M0: portability layer, exact shortest-round-trip numeric
conversion, arena, value model, flat key space with the provenance
chain, definitions and validation, sources, `explain()` and `inspect()`,
and the `cf_config` facade.
