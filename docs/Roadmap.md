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
| 🟩 | The format from the name, then the content, by several strategies | D-73 |
| 🟩 | One `define`, the type given or inferred: C, C11, C++, Python, Rust | D-74 |
| 🟨 | 1.3.1 released (1.3.0 tagged, not published) | CHANGELOG |
| 🟩 | Doctor checks a report's fingerprint against its contents (CFD019) | plan item 9 |
| 🟨 | Continuous fuzzing in CI (ClusterFuzzLite): written, its first run to read | `.clusterfuzzlite/`, cflite.yml |
| 🟩 | INI compared with `configparser` (36 of 36; `.properties` needs a JVM, left to its tests) | plan item 9 |
| 🟨 | Native OS sources: systemd's credentials done (D-80); macOS's user defaults left | plan item 9 |
| ⬜ | Go and .NET bindings, when asked | plan item 9 |
| 🟩 | Binary configuration: a layout-driven TLV codec, BER-TLV, CBOR | D-91, D-93 |
| ⬜ | Every operation on a file's data, written back in its format | assessment |
| ⬜ | Converting between formats | assessment |
| ⬜ | Code for each binding from a contract | assessment |
| ⬜ | 2.0: what the sizing modes break, once | plan item 10 |
| 🟩 | Sizes and work predicted, then checked: the file buffer and the key space kept, three more measured and left | D-76, plan of the evening |
| 🟩 | A UTF-8 byte order mark read past by every text reader (`cf_blob_skip_bom`) | 1.5.0 |
| 🟨 | Checks at compile time where they can be, at run time always: Rust's `const` keys, C++'s `constexpr` Keys and C11's define macros done; `doctor gen` left | D-75 |
| 🟩 | GitHub used for all it offers that fits | plan of the evening |
| 🟩 | A security pass: secrets in reasons, renames and Doctor's diff; bounds on references and YAML aliases; directories read whole or refused | 1.5.0 |

### Left from the security pass

Nothing. The last of it went on 2026-11-12: the readers' name tables
are seeded a decode (D-81), and Dependabot's pull request 5 was merged
against the pin.

The marker-secret gate, the first of the ideas "From the security pass
of 2026-11-11", is built for the library's outputs; Doctor's formats
are its second half, below. Wave 3's Linux source is built; what is
left of wave 3 is under "Every item to done".

What the pass taught about the machines: a change is pushed as a branch
and its CI read before `main` moves, since CI has clang-format 22,
clang-tidy 21 and MSVC's analyzer where a machine may have none of
them; and CI's jobs name `ubuntu-26.04`, as `ubuntu-latest` is 24.04,
whose libfyaml (0.8) is under the floor and whose clang-tidy (18) is
not the one the code is held to.

### What can run at once

The open items grouped by the files they change: items in different
groups can be worked on side by side, each in its own branch, and
merged in any order; items in one group go one after another.

| Group | Items | Changes | Size |
| ----- | ----- | ------- | ---- |
| A | D-73, the format from the name and the content; `cf_builder_document` | `src/serialize/codec.c`, `src/bridge/file.c`, a new source, `cf_serialize.h` | large |
| B | D-75 in C++ (`constexpr` keys and ranges), C11 and `doctor gen`'s output; Rust's part is done | `configforge.hpp`, `configforge.h`'s macros, `tools/doctor/contract.c` | medium |
| C | D-76's rest: the key space and the index sized from the bytes or the last generation; the emitter's output sized in dump mode | `src/core/keyspace.c`, `src/core/define.c`, `src/bridge/emit.c`, `bench/` | medium |
| D | Doctor checks a report's fingerprint against its contents | `tools/doctor/` | small |
| E | INI compared value by value with Python's `configparser` (Java's `Properties` needs a JVM the machines here do not have, so `.properties` is left to its own tests) | `scripts/check/conformance.py`, `tests/conformance/` | small |
| F | Native OS sources | `src/platform/`, a source each | large, and each system tested on itself |

Only A and B touch one file in common (a declaration each in
`configforge.h`), which merges by hand in a minute. Not in any group:
the release (it waits on a tag), 2.0 (it comes after all of these), and
the later ideas - TLV and CBOR, every operation on a file's data,
conversion, code for each binding - which wait until these are done. D
and E are the next pair, as they are small and share nothing.

### Every item to done (2026-11-07): the order, in waves

Every open item above, in waves; the items of one wave share no files
and run side by side, each in a branch of its own, written against a
specification and its tests; the wave is reviewed, merged one branch at
a time, verified (tier 1, the extra steps the change touches, Windows
quick) and pushed before the next starts. A unit is merged whole or not
at all, so wherever the work stops nothing is left half-done: a unit cut
short keeps its branch, unmerged, and says so here.

1. 🟩 **Wave 1**: D, Doctor's fingerprint check (CFD019); E, INI
   against `configparser` (36 of 36, 12 differences by design listed);
   ClusterFuzzLite; Rust's `const` keys (D-75).
2. 🟩 **Wave 2**: A, D-73 and `cf_builder_document`; C, D-76's rest,
   each prediction measured and kept only where it pays. **1.4.0** (ready; its tag is left).
3. 🟨 **Wave 3**: B, D-75: C++ is done (a `constexpr` Key's name, range
   and default are checked by the compiler from C++11), C11's
   `_Static_assert` in the define macros and the code `doctor gen`
   writes are left; F,
   the native sources: systemd's credentials (`$CREDENTIALS_DIRECTORY`)
   are done, `cf_builder_credentials` (D-80), tested on Linux and
   Windows, in C, C++, Rust and Python; the user defaults (`CFPreferences`) on macOS are
   left, to be tested on its own system in CI - Windows has its registry
   source already.
4. 🟩 **Wave 4**: CBOR (D-91) and TLV by a layout, BER-TLV included
   (D-92, D-93). **1.5.0.**
5. ⬜ **Wave 5**: converting between formats (`doctor convert`, through
   `cf_emit`, naming what the target cannot hold); code for each binding
   from a contract (`doctor gen --lang c|cpp|rust|python`).
6. ⬜ **Wave 6**: every operation on a file's data through its codec -
   read, add, change, remove, written back in its own format, keeping
   the layout where the format can. **1.6.0.**
7. ⬜ **Wave 7**: Go (cgo) and .NET (P/Invoke) bindings, each with its
   tests, an example and a CI job. **1.7.0.**
8. ⬜ **Wave 8, 2.0**: the stream reader and the dynamic sizing modes
   (D-61), and what else waits for a major version (the image header's
   layout), together, once. **2.0.0.**

### More formats, and what a format is (asked 2026-11-13; not yet cards)

After wave 4's CBOR and TLV. To be written as cards in
the plan, the library's own code first:

* 🟩 **What a format is, told to the program** (D-97; the bindings
  left). Each codec says, as
  flags a program can read and ask by ("the binary ones that need no
  schema"): text or binary, self-describing or bound to a schema, typed
  or not, readable as a stream or only whole, written or only read. The
  table in Formats.md is generated from the same flags, so the page and
  the code cannot differ.
* ⬜ **Binary formats that describe themselves**, each a reader, a
  writer, its standard's vectors and a fuzz target, as CBOR has:
  MessagePack first, then BSON, UBJSON, Smile, Amazon Ion, Bencode.
* ⬜ **Formats a configuration is really kept in**: Apple property
  lists (binary and XML), HCL, HOCON, KDL.
* ⬜ **Formats bound to a schema**: Avro (a container file carries its
  schema as JSON), Protobuf through a descriptor set rather than a
  `.proto` parser, Thrift (binary and compact), then FlatBuffers by its
  binary schema and Cap'n Proto.
* ⬜ **TLV as it is met, not only as the standards have it.** The
  standards as presets of the layout - BER and DER, ISO 7816's
  SIMPLE-TLV, EMV, SMPTE's KLV - and the layout itself free to say what
  embedded devices do to save a byte: the order of the three fields
  (TLV, LTV, TVL), each field's width down to one byte and its byte
  order, a tag whose length is fixed and so not written, a tag and a
  length packed into one byte.
* ⬜ **Fast, and without copying where it can be.** A benchmark first,
  megabytes a second for each codec; a reader hands the sink text where
  it lies in the document when the format stores it whole. Kept where
  measured, as D-76 was.
* ⬜ **Whole or as a stream, in every format that can.** One interface
  for reading in pieces, designed once (D-61 left it for 2.0), the
  length-prefixed binary formats first.

Rules for these: a format may start on a dependency where one is truly
needed and lose it later; one that cannot be written in C89 may leave
C89, noted where it does - every format is an option of the build. The
reference libraries are used in tests: one document, written in each
format by that format's own library, must read to the same keys.

Last of all, when everything else here is done: the serialization
layer becomes a project of its own.

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

## The plan from the evening of 2026-11-06 (the order to follow)

Asked for: CI fixed, 1.3.0 out, GitHub used for all it offers, and
three principles over everything that follows - predict where a guess
can be checked, decide at compile time what can be decided there, and
use several strategies where each is simple enough that a failure still
names its cause. Each is a trade-off, measured before it is kept.

1. 🟩 **CI**: the one red run was GitHub Pages refusing a deployment
   ("try again later"); run again.
2. 🟨 **1.3.1** (1.3.0 was tagged and its release failed on Windows; the tag cannot move): the CHANGELOG's date, tier 2 on Linux and Windows, then
   the tag, which publishes it (the tag is pushed by the maintainer).
3. 🟩 **GitHub, all of it that fits**: CodeQL code scanning (C and C++,
   Python, Rust, the workflows); Dependabot for the crate as well as the
   actions; `CODEOWNERS`; issue forms instead of free-text templates (done);
   private vulnerability reporting, Discussions, and a ruleset on
   `main` (all done, approved 2026-11-07): no deletion, no force push,
   and CI's gates, the three-system matrix, the sanitisers and clang
   required, with a bypass for the repository's administrators; release
   notes come from the CHANGELOG (done).
4. 🟦 **D-73, the format from the name and the content, by several
   strategies in turn**: the format the name gives is tried first,
   whatever it is; a cheap look at the first bytes orders the rest
   (`{` or `[` the JSON family, `<` XML, a `[section]` line INI or TOML,
   `key:` YAML, `key =` TOML or INI); each is then read in that order and
   the first that reads it, without a loose rival reading it otherwise,
   is taken. A file whose name names a format that cannot read it is
   still refused - with the format its content looks like, as a hint.
   Not only for files: for any source of bytes - standard input, a
   document the program holds (`cf_builder_document`), a whole document
   in one environment variable - by the same function over bytes.
5. 🟦 **D-75, checked at compile time where it can be, at run time
   otherwise**: a key's name, its range and its default against the
   range, checked by the compiler - `constexpr` in C++14 and later,
   `_Static_assert` in C11 for constants, `const` evaluation in Rust,
   and the code `doctor gen` writes - and by the library at run time
   everywhere, as now, so the same mistake is caught in every build and
   earlier in the ones that can.
6. 🟨 **D-76, sizes and work predicted, then checked** (the file buffer done): a file's buffer
   from the size the system reports; the key space and the definitions'
   index from the bytes (keys a line, about) or, on a reload, from the
   generation before; an emitter's output (dump mode) sized from the key
   space it walks, so one pass writes it; branch hints on the readers'
   hot paths; sequential read advice to the system for a large file.
   Each kept only where `bench/` shows it pays, and the numbers - the
   ones that did not pay too - go in Maintaining.md's "Measured".
7. Then the ideas in the table above, in its order.

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

### ⬜ Sizes predicted, then checked (2026-11-06)

Reading a file starts with a 4 KB buffer and doubles it, copying each
time: a 1 MB file is copied about nine times and peaks near twice its
size, in the transient arena (D-61). The operating system knows the
size already (`fstat`, `GetFileSizeEx`), so the first buffer can be the
size it says. It is a prediction, not a fact - a pipe, a file growing as
it is read, a `/proc` file that says 0 - so the reading loop stays as it
is and only its first capacity changes: a wrong guess costs what today
costs, a right one a single allocation and no copy. The same holds one
step on: the number of keys guessed from the bytes (a line a key, about)
to size the key space and the definitions' index once rather than as
they grow. Done when `bench/` shows the peak and the time for 1 KB to
10 MB files before and after, on Linux and Windows, the numbers go in
Maintaining.md's "Measured" whichever way they come out, and a file
whose size changes while it is read is still read whole.

### From the security pass of 2026-11-11

* 🟨 **A marker-secret gate.** One test drives a marker secret through
  every refusal path and every output and fails if the marker appears.
  Built for the library: `test_marker`, 46 scenarios, strict and with
  fallback, from eight kinds of source, over the error, `explain()`,
  the blind report, the schema, diffs, the fingerprint, log events,
  `cf_live` and store edits; both refusal leaks of the pass fail it
  when put back. What it taught: the marker must sit inside the
  reference (`${MARK`), not before it, or the wording that leaks is
  never reached. Doctor is held to it by `doctor.marker`: every
  command in every format over a pair of full reports that hold the
  marker in each state. Left: an undefined key that holds a secret (a
  mistyped name), which nothing can know is one - decide what a report
  shows for it (the plan, the hardening cards).
* 🟩 **A per-decode output budget** (D-82, `test_budget`), in bytes and keys, enforced in the
  flattener so that every codec inherits one bound instead of each
  reader keeping its own. Done when a hostile document of each format
  hits the same named limit.
* 🟩 **A fuzz check comparing arena bytes with input size** (in every libFuzzer harness and `test_fuzz`: four times D-82's bound), so that a
  document which costs a multiple of its size fails the fuzzer and does
  not wait to be found by hand.
* ⬜ **Keying the secret digest.** The digest is an unsalted FNV-1a, and
  over a PIN it can be brute-forced offline from a blind report or a
  fingerprint. Done when a digest cannot be tested against guesses
  without a key, and the report schema and `doctor diff` still compare.
* ⬜ **A trusted-directory check**, like sudo's, for drop-in and secrets
  directories: refuse one a user other than its owner or root can write
  to. Done when it is on by default for secrets and opt-in for drop-ins,
  with the decision in Decisions.md.
* ⬜ **Doctor `--redact-all`**, on by default in the action, so that a
  report's values are shown to no pull request unless asked for. Done
  when the action's default shows no value and a flag restores it.
* ⬜ **The readers proved, not only tested.** The aim is the safety a
  language such as Rust or Go gives by construction, shown here by
  evidence: sanitizers, fuzzing and three analyzers find what an input
  happens to reach, and a bounded model checker (CBMC) shows that no
  input of a bounded size can read or write out of bounds or overflow a
  signed integer. Done when the blob, the BOM skip and one text reader
  (INI first, the smallest) each have a harness that CI proves, and
  Safety.md says what is proved and to what bound.
* ⬜ **`.properties` held to Java's `Properties`**, value by value, as
  INI is to `configparser`. It was left for want of a JVM; the
  development machine's WSL has OpenJDK 21 now. Done when
  `scripts/check/conformance.py` runs both and lists the differences
  kept by design.

### From a review of the whole, 2026-11-14

Read against the code: the secret digest, `--redact-all`, a mistyped
secret and a writable secrets directory are cards already (H-8a, H-6,
H-7, H-5); generated code from one contract is wave 5; editing a file
in its own format is wave 6; the reload events already carry the
generation and both fingerprints (`cf_log_event`, 1.1). SipHash-2-4
under a key, as H-8a has it, is a keyed pseudorandom function, which is
what a digest that cannot be tested against guesses needs; an HMAC over
SHA-256 would add a hash to the library for no gain here. What was new:

* ⬜ **A file written whole or not at all.** The library writes no file
  today, but wave 6 will, and `examples/c/settings.c` already writes its
  settings with `fopen(..., "wb")` over the old ones: a crash or a full
  disk halfway leaves a file that is neither, and the next start reads
  it. One function in the platform layer: a temporary file in the same
  directory, written, flushed to the device (`fsync`,
  `FlushFileBuffers`), put in place by one rename (`rename`,
  `MoveFileExW` with `MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH`),
  the directory synced where POSIX needs it, the temporary removed on
  every failure, the original's mode kept. Done when wave 6 and every
  example that saves use it, and a test that fails the write at each
  step finds the old file intact.
* ⬜ **An override file the library owns.** Between the store (SQLite,
  transactional, D-42) and wave 6 (a user's file changed in place, its
  layout kept) there is the common case `settings.c` shows by hand: a
  program keeps what its user changed in a file of its own, read last,
  written whole by `cf_emit` with `CF_EMIT_CONFIGURED`. As a writable
  source, `cf_write` would commit to it through the function above, and
  the provenance would name it like any other file. Done when
  `settings.c` is that source's example and no longer writes a file
  itself.
* ⬜ **A reload names the keys it changed.** A swap's `"reload"` event
  says the generation and the fingerprints, not which keys moved; a
  program gets those today only by keeping the old snapshot and calling
  `cf_diff`. One `"key-changed"` event a key at debug level, with the
  key and the winning source and never the value, from the diff the swap
  can make, so that a log pipeline can tell what a reload did. Done when
  `test_marker` holds a secret through it and the events are in
  `cf_log.h`'s list.
* ⬜ **Measured against the libraries a program would otherwise use.**
  `bench/` measures this library against itself. The same documents read
  by inih, libconfig, cJSON, toml++ (or tomlc99), libyaml, and this
  library - time to parse, time to look up, peak memory, the stripped
  size each adds to a program - with the numbers in Maintaining.md's
  "Measured" whichever way they come out. The "Fast, and without
  copying" benchmark of the formats above uses the same harness. Done
  when CI runs it on Linux and the table is generated, not written.
* ⬜ **Used by a program not written for it.** Every example here was
  written for the library. One small open-source daemon whose
  configuration code is replaced by this library, in a fork, shows what
  the guide does not: what is awkward, what is missing, how much code
  goes. Done when the change is published with what it found, and each
  finding is a fix or an idea here.
* ⬜ **The references drawn.** A key's value may come from others
  (`${...}`), and a renamed key from its old name; `doctor graph` would
  write those edges, and each key's sources, as Graphviz or Mermaid from
  a report, so that a cycle or a long chain is seen rather than read.
  After wave 5, and only if a report's JSON through jq proves not to be
  enough (as with the query language below).
* A format is added for what it brings: each card in "More formats"
  says which of the categories (D-97) it fills that no codec here fills
  yet, or which programs keep their configuration in it, and what it
  costs - its lines, its fuzz target, its suite. The order inside each
  group follows from that.

Weighed and left: automatic migration of old files beyond what
`replaced_by` already does on read is wave 6's work once a file can be
written back in its format, and a value policy per profile ("no debug
log in production") is a jq expression over a full report today
(Doctor.md, 7a).

### From reading the project as a newcomer, 2026-11-14

Measured on that day: no package registry carries the library
(crates.io, PyPI, vcpkg and ConanCenter all answer "not found", though
`packaging/` holds the recipes), each 1.5.0 archive was downloaded
once, and 345 functions are exported under the 1.x promise, five minor
releases in nine days. Nothing below is a format; most of it is what a
first user meets. In the order proposed:

* ⬜ **Published where programs get their libraries.** The crate on
  crates.io, the wheel (with its shared library) on PyPI, the vcpkg and
  ConanCenter recipes submitted upstream, a Homebrew tap; each from the
  release workflow, after the GitHub release. Needs the registries'
  tokens as repository secrets. Done when `cargo add configforge`,
  `pip install configforge` and `vcpkg install configforge` work on a
  clean machine, and CI installs each in a job of its own. Go and .NET
  (wave 7) wait for this.
* ⬜ **What the 1.x promise covers, made smaller.** Functions marked
  INTERNAL sit in eight installed headers (`cf_ks_*` among them),
  exported for Doctor and the tests; a program can call them and will
  break in a minor release. Move them behind a macro a program does not
  define (`CF_INTERNAL_API`, set by Doctor's and the tests' builds) or
  into headers that are not installed; and add an experimental tier,
  `CF_EXPERIMENTAL`, for a new function's first release, outside the
  promise and marked so in `docs/API.md` and `gate.abi`. Done when
  `gate.abi` holds only the promised symbols and a consumer test that
  calls an internal one fails to compile.
* ⬜ **A program's files where its platform keeps them.** Design.md
  promised that `cf_app` would read the user's and the system's files
  from the platform's places; D-09 closed `cf_app` without them, so every
  program writes the XDG, `%APPDATA%` and `~/Library/Application
  Support` code itself - the repeated code this library exists to
  remove. `cf_builder_app_files(b, "myapp", "toml")`: the system file
  and its `conf.d` at `CF_RANK_SYS_FILE` (`$XDG_CONFIG_DIRS` or
  `/etc`, `%PROGRAMDATA%`, `/Library/Application Support`), the user's
  at `CF_RANK_USER_FILE` (`$XDG_CONFIG_HOME`, `%APPDATA%`,
  `~/Library/Application Support`); a missing file is no error, and
  `explain()` names each path it looked at. Opt-in: `cf_app` stays as
  D-09 has it. Done when the three platforms' paths are tested with a
  fixed environment and `layers.c` uses it.
* ⬜ **Profiles.** `--profile`, or `NAME_PROFILE`, adds
  `config.<profile>.<ext>` above the base file at each of the places
  above, as .NET's `appsettings.Production.json` and Spring's profiles
  do; the report's `profile` is then the library's, not only Doctor's.
  With the item above.
* ⬜ **A read of a key the contract does not have, noticed.**
  `cf_get_int(cfg, "server.prot", 8080)` returns 8080 without a word,
  and so does reading a text key as an int, contract or not; C++ and
  Rust catch both with typed keys, C does not. Opt-in, beside
  `cf_builder_track_reads`: a read that finds no definition, or the
  wrong type, is recorded - not logged, since a read takes no lock and
  may be on any thread - and `cf_misreads()` and the report list them,
  and Doctor's CFD021 reports them (CFD020 is H-8b's). Done when both mistakes in a test
  program reach `doctor lint`.
* ⬜ **Help, completions and a sample file from the contract.** A
  definition has its description, aliases, permitted spellings, range,
  default and environment name: enough for `--help`, completions for
  bash, zsh, fish and PowerShell, a man page, and a sample file in a
  chosen format with each key's description as a comment. In the
  library for `--help` (`cf_usage`), in `doctor gen` for the rest. Done
  when tier1's `--help` is generated and a test holds it.
* ⬜ **Durations and sizes.** `"30s"`, `"1h30m"`, `"512MiB"` are the
  commonest values a configuration holds and here are text or a bare
  int. A unit on an int definition, not a new `cf_type` (the enum is in
  the ABI): the value is kept as milliseconds or bytes, the range is
  given in the unit, the provenance keeps the text as written, and
  `explain()` shows both. TOML's dates and YAML's timestamps after it,
  as text with a checked form, if asked for. Done when a duration and a
  size go through every reader, `cf_emit` and the report.
* ⬜ **Byte order, word size and a bare-metal target in CI.** CI runs
  x86-64 and arm64, both little endian: the swaps in CBOR, TLV and the
  image (always little endian, ImageFormat.md) never run on a host that
  needs them, and nothing 32-bit runs. Jobs under qemu-user for s390x
  (big endian) and i386 or armv7 (32 bit), running the suite; and an
  `arm-none-eabi` build of the `minimal` preset for a Cortex-M, with
  the size of each `CF_WITH_*` part from that build in the README's
  table. Done when the three jobs are green and required.
* ⬜ **Values migrated, not only renamed.** `replaced_by` moves a value
  to a new name; nothing converts it (`timeout_s` to `timeout_ms`) or
  splits one key into two. A function on the definition, run on the old
  key's winning value, its result offered at the same rank, the
  provenance saying "from timeout_s, converted". Done when
  `migration.c` shows a unit change and `explain()` names the old key.
* ⬜ **Trust that can be checked from outside.** The OpenSSF Scorecard
  workflow, the OpenSSF Best Practices badge, and a job that builds a
  release twice and compares the bytes. Done when the badges are in the
  README and the comparison is a release step.

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

None. Two were decided on 2026-11-11:

* **Response files become opt-in.** `@file` arguments are expanded by
  default in `cf_app`, so an untrusted positional argument can inject
  configuration and probe for files. Decided: a program asks for them.
  Built in 1.5.0 (D-79): `cf_builder_response_files`.
* **Repository settings.** Dependabot alerts, secret scanning and its
  push protection are on, since 2026-11-11.

The last decision before these, arrays in INI, was made on 2026-11-05:
C style; the plan above, item 4.
