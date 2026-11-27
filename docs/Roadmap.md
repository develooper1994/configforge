# Roadmap

The state of the work, kept true in the same commit as the work - so that
whoever picks this up next, on whichever machine, knows what is next,
what is known to be wrong, and what has been thought of and not built.

Each item says **why** it matters and **what done looks like**, and
carries a mark for how far it has come. "At a glance" is the whole list
in one table; the sections after it are the detail.

The Roadmap says what is wanted and why; the order of the work and its
cards are in `docs/Plan.md` (its index is generated) and
`docs/plan/Layout.md` (stage 0, then each module). A card that lands
marks its Roadmap item here.

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
| 🟩 | Continuous fuzzing in CI (ClusterFuzzLite): every pull request and a daily batch on main, green with no finding open (read 2026-11-27) | `.clusterfuzzlite/`, cflite.yml |
| 🟩 | INI compared with `configparser` (36 of 36; `.properties` needs a JVM, left to its tests) | plan item 9 |
| 🟩 | Native OS sources: systemd's credentials (D-80) and macOS's user defaults (D-87), in C, C++, Rust and Python | plan item 9 |
| ⬜ | Go and .NET bindings, when asked | plan item 9 |
| 🟩 | Binary configuration: a layout-driven TLV codec, BER-TLV, CBOR | D-91, D-93 |
| ⬜ | Every operation on a file's data, written back in its format | assessment |
| 🟩 | Converting between formats | D-94 |
| 🟩 | Code for each binding from a contract | D-95, D-96 |
| ⬜ | 2.0: what the sizing modes break, once | plan item 10 |
| 🟩 | Sizes and work predicted, then checked: the file buffer and the key space kept, three more measured and left | D-76, plan of the evening |
| 🟩 | A UTF-8 byte order mark read past by every text reader (`cf_blob_skip_bom`) | 1.5.0 |
| 🟩 | Checks at compile time where they can be, at run time always: Rust, C++, C11 and `doctor gen`'s output | D-75 |
| 🟩 | GitHub used for all it offers that fits | plan of the evening |
| 🟩 | A security pass: secrets in reasons, renames and Doctor's diff; bounds on references and YAML aliases; directories read whole or refused | 1.5.0 |
| 🟩 | One refusal model: a place and a reason on every refusal | A-10, D-107 |
| 🟩 | Build profiles, the C version per file, and the feature matrix made from the build | A-11, A-12, D-105, D-106 |
| 🟩 | A page of the project's limits; every pair of formats and what each can hold | A-13, W5-V2, D-109 |
| 🟩 | The other libraries measured beside ours; allocation measured | P-1b, P-4 |
| 🟩 | Static analysis, run-time tools and supply-chain checks in CI; cppcheck on both libraries | L-2, L-3, N-9, N-10 |
| ⬜ | EVA over `numeric`'s float half (bignum limbs, `cf_gen_digits`, `cf_f64_to_text`, `cf_text_to_f64`, `cf_f64_from_*bits`): a harness and precision work of its own, after L-3b | L-3b |
| 🟩 | The Linux musl release archive built twice, the bytes compared | N-2 |
| 🟩 | A reload names the keys it changed; `cf_usage` writes `--help` | N-7, N-6 |
| 🟩 | `configforge diff` by value: flat, nested and scalar-array; the Git difftool for JSON | N-1, N-5, N-8, N-3 |
| 🟩 | The 1.8.0 group's integration findings | I-1 |

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
| A | D-73, the format from the name and the content; `cf_builder_document` | `lib/serialize/src/codec.c`, `lib/config/src/bridge/file.c`, a new source, `cf_serialize.h` | large |
| C | D-76's rest: the key space and the index sized from the bytes or the last generation; the emitter's output sized in dump mode | `lib/config/src/core/keyspace.c`, `lib/config/src/core/define.c`, `lib/config/src/bridge/emit.c`, `bench/` | medium |
| D | Doctor checks a report's fingerprint against its contents | `tools/doctor/` | small |
| E | INI compared value by value with Python's `configparser` (Java's `Properties` needs a JVM the machines here do not have, so `.properties` is left to its own tests) | `scripts/check/conformance.py`, `tests/conformance/` | small |
| F | Native OS sources | `lib/platform/src/`, a source each | large, and each system tested on itself |

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
3. 🟨 **Wave 3**: B, D-75: done (a `constexpr` Key's name, range
   and default are checked by the compiler from C++11, C11's
   `_Static_assert` in the define macros and the code `doctor gen`
   writes); F,
   the native sources: systemd's credentials (`$CREDENTIALS_DIRECTORY`)
   are done, `cf_builder_credentials` (D-80), tested on Linux and
   Windows, in C, C++, Rust and Python; the user defaults (`CFPreferences`) on macOS
   are done too, `cf_builder_preferences` (D-87), read on macOS in CI and
   offered in every language - Windows has its registry source already.
4. 🟩 **Wave 4**: CBOR (D-91) and TLV by a layout, BER-TLV included
   (D-92, D-93). **1.5.0.**
5. 🟩 **Wave 5**: converting between formats (`configforge codec convert`,
   D-94); code for each binding from a contract (`configforge gen --lang
   c|cpp|rust|python`, D-95) and a contract drafted from a sample
   (`configforge gen schema --from`, D-96).
6. ⬜ **Wave 6**: every operation on a file's data through its codec -
   read, add, change, remove, written back in its own format, keeping
   the layout where the format can. **2.2.0.**
7. ⬜ **Wave 7**: Go (cgo) and .NET (P/Invoke) bindings, each with its
   tests, an example and a CI job. **2.3.0.**
8. ⬜ **Wave 8, 2.0**: the stream reader and the dynamic sizing modes
   (D-61), and what else waits for a major version (the image header's
   layout), together, once. **2.0.0.**

Which version each item makes is in `docs/Maintaining.md`, "Versions";
the numbers above are those of the table there.

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

The serialization layer becomes a library of its own in two steps
(revised 2026-11-14; until then it was to be split last of all):

1. 🟩 **Before the first new format: a library target in this
   repository** (cards S-3, S-4, S-5): the target, its symbol list, its
   install, pkg-config file and Conan component (D-90). `src/serialize` is the largest part of the code (about
   9,900 lines of 29,000 on 2026-11-14) and has value without the rest:
   a C89 reader of YAML 1.2 that passes all of yaml-test-suite, JSON5,
   TOML, CBOR, TLV, with no dependency. `gate.layering` already holds it
   apart from the core. It gets its own CMake target and installed
   headers, its own symbol list in `gate.abi`, and its own version
   macros; `libconfigforge` keeps carrying it inside (linked
   statically), so no 1.x program loses a symbol. Every format added
   after this lands in that library and its promise, not ConfigForge's.
   With "What the 1.x promise covers, made smaller", which asks the same
   question: which functions stay public. Done when a consumer test
   builds a program against the serialization library alone, and one
   against ConfigForge alone, and `gate.abi` holds both lists.
2. **A repository of its own** once that library has users of its own,
   or at 2.0, whichever is first. A second amalgamation of the
   serialization library alone comes with it (S-4 left it out: a second
   distribution form to gate, for a library nobody asked for alone).

Doctor stays here as a package of its own (it already ships as its own
binary and depends only on the report's schema), and so do the Rust and
Python bindings, tested with the C they wrap. The platform layer, the
sources and diag are not split: without the core they mean nothing.

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
    * native OS sources: gsettings, where the platform is at hand to
      test (macOS's user defaults are done, D-87).
    * the user defaults' other scopes (this host, any user, a managed
      profile): a scope prefix in `domain` (`"currentHost:..."`,
      `"anyUser:..."`, `"managed:..."`), each its own source at its own
      rank (D-87, option c).
    * Go and .NET bindings over the C ABI.
11. **2.0**: what 8 and 9 break, together, once.

## Ideas

Every idea written here before 2026-11-05 is now in the plan above, in
the order of its use. New ones go here until they are
accepted.

### 🟩 Sizes predicted, then checked (2026-11-06; 068e065, dea83f0)

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

* 🟩 **A marker-secret gate.** One test drives a marker secret through
  every refusal path and every output and fails if the marker appears.
  Built for the library: `test_marker`, 46 scenarios, strict and with
  fallback, from eight kinds of source, over the error, `explain()`,
  the blind report, the schema, diffs, the fingerprint, log events,
  `cf_live` and store edits; both refusal leaks of the pass fail it
  when put back. What it taught: the marker must sit inside the
  reference (`${MARK`), not before it, or the wording that leaks is
  never reached. Doctor is held to it by `doctor.marker`: every
  command in every format over a pair of full reports that hold the
  marker in each state. An undefined key named like a secret is kept
  as one (D-84).
* 🟩 **A per-decode output budget** (D-82, `test_budget`), in bytes and keys, enforced in the
  flattener so that every codec inherits one bound instead of each
  reader keeping its own. Done when a hostile document of each format
  hits the same named limit.
* 🟩 **A fuzz check comparing arena bytes with input size** (in every libFuzzer harness and `test_fuzz`: four times D-82's bound), so that a
  document which costs a multiple of its size fails the fuzzer and does
  not wait to be found by hand.
* 🟩 **Keying the secret digest.** The library keys it (D-85); Doctor tells
  digests of different keys apart (CFD020). The digest was an unsalted FNV-1a, and
  over a PIN it can be brute-forced offline from a blind report or a
  fingerprint. Done when a digest cannot be tested against guesses
  without a key, and the report schema and `doctor diff` still compare.
* 🟩 **A trusted-directory check**, like sudo's, for drop-in and secrets
  directories: refuse one a user other than its owner or root can write
  to. Done when it is on by default for secrets and opt-in for drop-ins,
  with the decision in Decisions.md: on for secrets and credentials,
  `CF_TRUSTED_DIR` for drop-ins (D-83).
* 🟩 **Doctor `--redact-all`**, on by default in the action, so that a
  report's values are shown to no pull request unless asked for. Done
  when the action's default shows no value and a flag restores it: on in
  the action; `show-values` restores the values (D-86).
* 🟨 **The readers proved, not only tested** (card N-11: the BOM skip, 765de40; the BOM skip, `cf_fmt` and the integer conversions are proved, the blob and an INI reader are not yet). The aim is the safety a
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
* 🟩 **A reload names the keys it changed** (card N-7, done). A swap's `"reload"` event
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
  This is the other comparison, of whole configuration libraries: the
  one of the readers is "The aim: beat the others at everything" below
  (P-1b), and this stays open.
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
* 🟩 **What the 1.x promise covers, made smaller** (cards S-1, S-2): internals fenced and an experimental tier (D-88, D-89). Functions marked
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
* 🟨 **Help, completions and a sample file from the contract** (card N-6: `cf_usage` only). A
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
* 🟨 **Trust that can be checked from outside.** The OpenSSF Scorecard
  workflow is done (L-2; its badge shows after its first run on `main`),
  and so is the job that builds the Linux musl archive twice and
  compares the bytes (N-2). The OpenSSF Best Practices badge is not.
  Done when the badges are in the README.

### Tools, distribution and digests, 2026-11-14

* 🟩 **Doctor does one job; the other two get tools of their own** (three programs behind `configforge`; Doctor's aliases go in 1.8.0 (T-5))
  (cards T-1, T-2, T-3; T-5 drops the aliases in 1.8.0).
  Doctor gathers three kinds of command, and wave 5 would add more:
  reading reports (explain, dump, diff, history, blame, lint, parity,
  emit), which is what it is for; the contract and generated code
  (`check --schema`, `gen`, and the planned `schema --from`,
  `gen --lang`); and formats (`formats`, the planned `convert`). Three
  programs, each beside the library it serves, behind one front door
  as git has it: `configforge <command>` runs `configforge-<command>`,
  so `configforge doctor explain` (as Doctor.md already writes it),
  `configforge gen`, `configforge convert`. Done when Doctor holds only
  the report commands, the others answer under their own names, and the
  GitHub Action and the docs use the new names; the old Doctor commands
  stay one minor release as aliases that say where they went.
* 🟩 **The serialization library's own tool** (formats, detect, check, dump and convert, D-94; cards T-3, T-4,
  W5-V1). Once that library is a
  target of its own: convert a document from one format to another as
  a stream of the codecs' events - nesting and arrays kept, no key space
  and no contract on the way - and say what the target cannot hold;
  pretty-print, check that a file is well formed, tell the format from
  the bytes, and dump a binary one (CBOR, TLV, the image) with each
  value's byte offset. Distinct from exporting a configuration (its
  layers merged, typed by its contract, written by `cf_emit`), which
  stays with ConfigForge. Done when every built-in codec converts to
  every other that can hold the document, held by the conformance
  suites' documents round-tripped through each.
* ⬜ **Distribution, as infrastructure.** Publishing from the release
  workflow by trusted publishing (OIDC: PyPI and crates.io accept it,
  so no token is kept in the repository), wheels for each platform;
  recipes submitted to vcpkg, ConanCenter, Meson's WrapDB, the Bazel
  Central Registry and conda-forge, a Homebrew tap; for embedded
  targets the ESP-IDF Component Registry, PlatformIO's registry and a
  Zephyr module; the GitHub Action in a repository of its own with
  `action.yml` at its root, as the Marketplace requires. Discussions
  open; downloads, traffic and issues counted once a month into
  Maintaining.md. Done when each channel installs a release on a clean
  machine in CI.
* ⬜ **Being found.** A playground on the documentation site: the
  library built to WebAssembly, a few files and a contract pasted in,
  `explain()` and the provenance shown, nothing installed. An honest
  comparison page (libconfig, inih, toml++, figment, .NET's
  `GetDebugView`); a short terminal recording; two articles - "why is
  the production value what it is" and a C89 YAML reader that passes
  all of yaml-test-suite; then the C, C++, Rust and embedded
  communities and the curated lists. Written in the project's voice, as
  every document here. Done when the playground is live and the
  articles are published.
* ⬜ **One digest a purpose, and a door for the program's own.** A
  digest does one of four jobs, and each wants its own kind: telling a
  change (the fingerprint: FNV-1a, fast and stable), hiding a secret
  that is compared (keyed SipHash-2-4, H-8a), deciding something - a
  fingerprint used to approve a configuration or allow it through a
  gate, where FNV-1a's collisions can be made on purpose (SHA-256, as an
  option of the fingerprint, `sha256:`), and a rule or a device that
  names its own (FIPS asks for HMAC-SHA-256; many microcontrollers hash
  in hardware): a function the program gives, with the name the report
  writes before its digests. Not a menu of algorithms: one default for
  each job, and the prefix every digest already carries (`fnv1a:`,
  `sip24:`) keeps old reports readable. One algorithm for the
  fingerprint within a report, so that a diff compares; H-8b's "cannot
  be compared" covers two reports that differ. Done when a gate built on
  a SHA-256 fingerprint is in the Guide, and a test gives a digest
  function of its own and reads its name back from the report.
* ⬜ **A running program asked for its report.** Spring Boot's
  `/actuator/env`, kept within the library's rule that it owns no
  thread: a function that answers one request on a descriptor the
  program's own loop accepted, with the value-blind report, and
  `configforge doctor pull unix:/run/myapp.sock`. Done when an example
  serves it from `cf_watcher`'s kind of loop and Doctor reads it.
* 🟨 **Recipes, not code** (the fingerprint as an OpenTelemetry attribute is in the Guide; the reload events as log records and the OPA and CUE recipe remain), in the Guide: the fingerprint as an
  OpenTelemetry resource attribute and the reload events as its log
  records; a report as input to OPA (Rego) or CUE for rules about
  values per profile.
* ⬜ **"Start here", one page a language.** Design.md, Decisions.md and
  API.md are 11,000 lines between them; a newcomer needs one page that
  goes from the build to `explain()` in C, C++, Rust and Python, and
  links on from there.
* Later, and large: a language server driven by the contract - a key's
  description, default and range on hover, and, from a report, what
  overrides it in production.

### Critical-system standards, and large files (asked 2026-11-16; not yet cards)

* 🟨 **Critical-system standards, checked by machine** (cards N-9 and N-10 for
  the first step, cppcheck on the libraries with every class, none left: done;
  N-19: `gate.misra` runs the MISRA addon over the `misra` profile's sources
  and every one of the 6,337 reports is recorded, in
  `docs/compliance/deviations.txt`, as a deviation, a tool error or a backlog
  line a later card lowers; what waits is the matrix of every rule, the
  public headers' zero, and the compliance report). MISRA C first,
  then SEI CERT C's rules no tool sees, a CWE table, and MISRA C++:2023
  for the wrapper; each rule's status in a matrix, each deviation in a
  record, and a gate (`gate.misra`, cppcheck's MISRA addon on a `misra`
  profile build) that fails a violation with no record. Standards
  disagree (MISRA's no-heap and single-exit rules against CERT's
  practice; MISRA's no-stdio against reading files), so each conflict is
  decided once, and the standards left out are named with why
  (`docs/Compliance.md`). Nothing is called "certified" until a
  qualified tool has run. Done when the gate is in CI and the
  compliance report is attached to a release. Belongs with 1.9.0's
  "outside trust checks". Found on 2026-11-16: the library's own
  sources have never run through cppcheck (the `configforge` target is
  made before `configforge_setup_cppcheck()`; only tests and tools are
  checked), and a first run on the serialization sources reports
  `uninitvar` and `variableScope`; turning it on for both libraries, and
  answering what it finds, is this item's first step.
* 🟨 **Large files, and SIMD as an option** (cards P-1, P-2, P-3 for the
  measurement and SWAR; SIMD a card after them). Configuration is small, but
  a format library is also handed documents of megabytes (data in CBOR,
  MessagePack, JSON exports). First measure: 1.8.0's benchmark gains
  documents of 1, 16 and 256 MiB and says where time goes. Then, if the
  scanning (UTF-8 validation, whitespace and escape search) is where it
  goes: a portable word-at-a-time (SWAR) path in C89, for every target;
  and after it, SIMD behind a build option (`CF_WITH_SIMD`, off by
  default), for the targets that have it (SSE2/AVX2, NEON) and not
  required of the rest. The scalar path stays the reference: every
  vector path is tested against it on the same inputs, and the fuzzers
  run both. Not before the measurement says so. Measured (P-1): the
  benchmark reads documents of 1, 16 and 256 MiB and `docs/Performance.md`
  has the baseline. P-4 measured allocation on Linux and Windows. Faster
  arena wiping cut a 16 MiB JSON builder load from 1.16 s to 1.04 s on
  Linux. All three sizing trials exceeded the memory limit for acceptance.
  [Allocation measurements](Performance.md#allocation)

### Suggestions of 2026-11-17 (not yet cards)

* ⬜ **verify sees the version CI sees.** The staged copy verify builds
  has no git directory, so a version string carries no commit id there
  and a test of it passes locally while failing in CI (`gen.version`,
  2026-11-17). The copy is given the commit id of what was staged, the
  way the build reads it in a checkout.
* ⬜ **Tests that are known to catch.** Coverage says a line ran, not
  that a wrong line would fail a test. Mutation testing (mull, for C
  through LLVM) on the core and the readers, its score reported beside
  coverage; evidence `docs/Compliance.md` can cite.
* 🟩 **The release built twice, compared byte for byte** (card N-2, the Linux musl archive). Provenance
  attestations say where an archive came from; a reproducible build
  says it is what the source makes. The release workflow builds the
  Linux archive twice in clean trees and fails if they differ.
* ⬜ **Documents kept short.** `docs/Decisions.md` is over 4,300 lines:
  its index stays where it is, and each decision's text moves to
  `docs/decisions/D-NN.md`, linked from the index, so a reader opens
  one decision rather than scrolls past ninety. The Roadmap's finished
  items move to a page of their own, the Roadmap keeping what is left.
* ⬜ **The serialization sources, laid out by what they share.** Today
  `lib/serialize/src/` is one file per format (about 10,900 lines; `toml.c`,
  `yamlparse.c`, `yaml.c`, `json.c` and `tlv.c` over 1,100 each). To
  weigh, each split or merge kept only if it removes duplicated code or
  lets a build leave a part out:
  * **Split by stage**: each large format as its lexer, its parser and
    its printer (`toml_lex.c`, `toml_parse.c`, `toml_emit.c`), so a
    read-only build links no printer, a review reads one stage, and the
    MISRA matrix and coverage count per stage.
  * **Merge what repeats**: UTF-8 validation, escape decoding, number
    text to value (integer and real), whitespace and comment skips, and
    the line/column count are likely written once per format; one
    internal `scan.c`/`text.c` holds each once, tested once, and is the
    one place P-3's word-at-a-time scans go.
  * **Group by family**: the text formats (JSON, YAML, TOML, INI,
    properties, XML) and the binary ones (CBOR, TLV, blob) as two
    groups a build can take apart (`CF_WITH_TEXT_FORMATS`,
    `CF_WITH_BINARY_FORMATS`, beside the per-format options).
  * **A single-file build**: a generated amalgamation
    (`configforge_serialize.c`, as SQLite ships), for embedded users who
    drop one file into their build; produced by a script and checked to
    compile alone, never edited.
  Before MISRA (`docs/Compliance.md`) and before the P cards: moving the
  code after they have touched it is moving it twice. First step: a
  table of the duplicated helpers across the readers, with line counts.
* 🟨 **Infrastructure behind abstractions.** Decided on 2026-11-19,
  every layer at once, in `docs/plan/A-architecture.md` (D-100 to
  D-108: the components under `lib/`, the vector library, one copy of
  each text helper, composed codecs, the build-time layers, profiles,
  the C version per file, one refusal model, every text UTF-8 inside);
  built from 1.8.0 (A-1b, A-7, A-8, A-9 and A-10 done). As first
  written: Every capability the
  readers lean on - the byte scans, number parsing, memory, the
  processor's features - is reached through an internal interface with
  a scalar reference behind it, never called directly from a format.
  A format asks "find the next of these bytes" or "parse this number";
  which implementation answers is the layer's business. This is what
  lets one optimization serve every format, a backend be swapped or
  turned off at build time, and the MISRA profile keep only the scalar
  forms. It is the shape the items below build on, and the sources'
  new layout (above) is where it starts.
* ⬜ **Where optimization learns from: the fastest libraries, all of
  their good parts.** Read before the P cards' forms are chosen, and
  cited in each card's notes. What each is known for, to confirm by
  reading its code:
  * **yyjson** (ANSI C89, MIT): an immutable document held in two
    contiguous areas, one for values and one for strings, with a
    mutable document beside it; exact reading and writing of int64,
    uint64 and double; a caller's allocator. The closest in language
    and aim to ConfigForge, so the first to read. Not confirmed from
    its pages (2026-11-19): that it reads in a single pass with no
    separate tokenizer, and how its numbers and its pool are done -
    read in its source before citing.
  * **simdjson** (C++, Apache-2.0 or MIT): a two-stage design
    (Langdale and Lemire, "Parsing Gigabytes of JSON per Second", VLDB
    Journal 28(6), 2019) - stage 1 finds every structural character
    with SIMD into an index, stage 2 walks the index; On-Demand parsing
    that reads only what is asked; a parser for the processor chosen at
    run time; number parsing
    (`include/simdjson/generic/numberparsing.h`: eight digits checked
    and read at a time with SWAR, `is_made_of_eight_digits_fast` and
    `parse_eight_digits_unrolled`; reals by a 128-bit multiply against
    a table of powers of five, after Mushtak and Lemire, "Fast Number
    Parsing Without Fallback", the Eisel-Lemire line).
  * **RapidJSON** (C++, MIT): SAX and DOM over one reader, SSE2 and
    SSE4.2 whitespace skips; in-situ parsing (strings decoded in the
    input's own buffer), NEON and Grisu2 for printing reals are
    remembered but not confirmed from its pages (2026-11-19).
  * *Hacker's Delight* (Warren) for the word tricks.
  Their ideas are taken, not their code (C++, and licences differ).
  **Not only JSON**: each idea is restated as an operation of the
  shared layer (a structural index, in-situ strings, number parsing,
  a byte-class scan), so YAML, TOML, INI, XML, CBOR and the rest gain
  from it too; a JSON-only trick is the exception, and named as one.
* 🟨 **The aim: beat the others at everything, and say where it does.**
  The benchmark (P-1, and 1.8.0's comparison) runs the same documents
  through yyjson, simdjson and RapidJSON for JSON, and through each
  format's best-known C or C++ reader for the others (to be listed:
  libyaml and rapidyaml, toml++ and tomlc17 (tomlc99's successor;
  tomlc99 calls itself obsolete), tinycbor and libcbor,
  inih, expat and pugixml). Each measure is its own line - parse speed,
  print speed, memory, code size - per format and size. Winning every
  line may not be possible; the README leads with the lines it wins,
  and the page shows all of them, losses included, with the machine and
  compiler. Built (P-1b), first round: 11 libraries in 6 formats; the
  README's lead lines wait for the first weekly run.
* 🟨 **A SWAR and SIMD backend, in C89/C99, possibly a library of its
  own** (the `lanes` library, A-2, da0dd49, with a scalar form and a SWAR backend, P-2, e0c0904; the SIMD tiers and run-time dispatch remain). The gap, as checked on 2026-11-19 against the libraries'
  own pages: every piece exists somewhere, the whole does not.
  Highway (C++17), xsimd (C++17), EVE (C++20), libsimdpp (C++14) and
  qlibs/swar (C++20, SWAR only) are C++. SIMDe (C99) maps one
  instruction set's intrinsics onto another and chooses at build time.
  nsimd (MIT) comes closest: C89 where it can, C99 otherwise, one
  portable vocabulary over SSE2 to AVX-512, NEON and SVE, and a scalar
  target - but its extension is chosen when it is built, and it has no
  SWAR tier. VOLK (LGPL-3.0) and Simd Library (MIT, a C API over C++)
  dispatch at run time over scalar bases, for radio and image kernels.
  simdutf has a C11 API for text only; cpu_features (C99) only
  detects. Among those checked, none combines a C89 SWAR tier, a
  scalar reference per operation, C99 SIMD tiers and automatic
  run-time dispatch behind one C API for byte-oriented operations
  (find any of a set, run length, eight digits to a number). "Among
  those checked": the search was not exhaustive, and nsimd's activity
  is unconfirmed. Its shape:
  * **Operations, not instructions.** Each operation (find a byte of a
    set, a run below a value, a byte-class mask, eight digits to a
    number, compare, count) has a scalar form (the reference), a SWAR
    form in C89 (32- and 64-bit words) and SIMD forms in C99.
  * **Lanes.** A register is a vector of lanes whose width the caller
    picks: a 512-bit register as 64 bytes, 32 16-bit, 16 32-bit or 8
    64-bit integers; SWAR does the same within a word. The operations
    are written per lane width, so a number parser and a byte scanner
    share one vocabulary.
  * **Detection, at build time and at run time.** The build knows the
    architecture (x86, x86-64, Arm, AArch64), the endianness and what
    the compiler can emit; the run finds what the processor has (CPUID
    and XGETBV on x86, the auxiliary vector or the system's query on
    Arm). Endianness matters only where a mask is turned back into a
    byte's index; that step has a form per byte order, tested on a
    big-endian target (s390x or PowerPC under QEMU in CI).
  * **Choice in two stages.** First manual: the program names the
    backend (the build option, or a call at start-up), on the
    assumption that the programmer knows the target. Then automatic:
    the best the processor has, falling back in order - AVX-512, AVX2,
    SSE4.2, SSE2 on x86; SVE, NEON on Arm - then SWAR, then scalar.
  * **Every part on or off at build time.** `CF_WITH_SIMD`,
    `CF_WITH_SWAR`, and one option per instruction set and per lane
    width, so an embedded build carries only what its target runs, and
    the MISRA profile carries the scalar forms alone.
  * **Tested against the scalar form**, every operation, every backend
    the CI machine has, with the fuzzers comparing them.
  * **Inspiration**: SIMDe (portable intrinsics in C), xsimd and
    Highway (one source for many instruction sets; Highway's run-time
    dispatch), qlibs/swar (the SWAR vocabulary).
  It grows out of P-2's `swar.h` rather than ahead of it: built inside
  ConfigForge first, split into its own repository only if a second
  project wants it (the shared-tools rule: upstream only what is
  repeated).
* 🟩 **`configforge gen --lang <lang>`, open to more languages** (D-95, D-110). Today
  `gen` writes one contract, in C (`tools/gen/contract.c`). Each
  language becomes a generator behind one interface (its name, its file
  extension, and a function from the contract's model to text), held in
  a table. The languages shipped with the project (C first; C++, Rust
  and Python next, as the bindings have them) are entries of that same
  table, not a special path, so adding one is adding an entry and its
  test. A generator outside the tree is found the same way: a program
  named `configforge-gen-<lang>` on the PATH, handed the model as JSON
  on its input, the way `git` and `cargo` find their subcommands. A
  `--lang` with no built-in entry and no program fails with the list of
  what is there; `--lang` absent keeps today's C. Placed (2026-11-19):
  the table is W5-G1's, the outside generators W5-G4 after W5-G3.
* 🟨 **A live optimization and benchmark page.** `docs/Performance.md`
  (P-1 opens it) becomes a page CI keeps current: every push to main
  runs the benchmark, appends its numbers to a history kept on a branch
  of its own or in the Pages site, and the page draws each format's
  line over time against the other libraries, with the commit of each
  point and the machine. A change that moves a line shows on it the day
  it lands; the instruction-count gate (P cards, "What keeps a gain")
  stays what fails a build, the page is what shows the trend. Each
  optimization tried, kept or reverted, is listed on it with its
  numbers, so the page is also the log of what was learned. Built
  (P-1b), first round: 11 libraries in 6 formats; the README's lead lines
  wait for the first weekly run.
* 🟩 **Tests that convert between every pair of formats** (D-109). For each
  document of the conformance suites and the fuzz corpora: read it in
  format A, write it in B, read it back, write it in A, and require the
  model to be the same, or the refusal to name what B cannot hold (a
  NaN in JSON, a mixed-type array in TOML, a key order XML keeps and
  INI loses). Every ordered pair, not only a few; a fuzz target that
  takes a random document through a random chain of formats; the
  values at their edges (the largest integers, reals that do not
  round-trip in fewer digits, empty keys, deep nesting, every Unicode
  plane, a NUL inside a string). What one format loses is written down
  per pair, in a table the test checks.
* 🟨 **Benchmarks fair, and without mercy.** Fair: every library is
  built with the same compiler and flags, at its own recommended
  settings, from a named version; each is asked for the same work (a
  library that validates is not set against one that does not, or both
  are run both ways); the documents, the scripts and the raw numbers
  are published so anyone can run them; the machine and its state
  (frequency scaling off, pinned cores) are recorded. Without mercy:
  the documents are chosen to hurt - all-escape strings, numbers at
  every edge, nesting to the limit, one huge array, many tiny files,
  invalid input that fails late, 256 MiB and beyond, a cold cache and
  a warm one, one thread and every thread; memory and code size are
  counted, not only speed; and a case ConfigForge loses is kept on the
  page, not dropped. Built (P-1b), first round: 11 libraries in 6
  formats; the README's lead lines wait for the first weekly run.
* ⬜ **The other libraries' CVEs and bug reports, as tests here.** The
  readers this project is compared with have public histories: CVEs and
  issue trackers of libyaml, PyYAML, SnakeYAML, yaml-cpp, rapidyaml,
  RapidJSON, cJSON, json-c, Jansson, yyjson, simdjson, expat, libxml2,
  tinyxml2, inih, toml++, tinycbor, libcbor and others. Each known
  weakness becomes a regression test or a fuzz seed here, named after
  its CVE or issue, with the rule that holds it: billion-laughs and
  other alias or entity expansion, stack exhaustion from deep nesting,
  integer overflow in a length or a count, quadratic time on crafted
  input (hash collisions, repeated keys, long escapes), a read past the
  end on a truncated document, NUL inside a string, invalid UTF-8 and
  overlong forms, numbers that overflow or lose precision silently,
  duplicate keys resolved differently than another reader would (a
  confusion attack), and the type tags that let a YAML reader build
  objects (not applicable here, said so in the table). The table,
  `docs/compliance/known-weaknesses.md`, goes beside the CWE table
  (`docs/Compliance.md`), each row naming the source, the test and its
  state, and `gate.cwe` checks that each named test exists. First
  step: the NVD and each project's tracker searched, and the list
  counted.
* 🟩 **A page of the project's limits.** `docs/Limits.md`; `gate.limits`, `unit.test_limits`. What the
  library does not do and what it will not take, in one place, so a
  user learns it before they depend on it. The build limits
  (`CF_KEY_MAX`, `CF_PATH_MAX` and the rest, today in the README's
  "Limits and the stack budget"), the decode budget (D-82) and depth
  limit, the largest document each format reads and why, what each
  format cannot hold and what a conversion loses (the pair table
  above), the platforms tested and those only built, the C versions,
  thread rules, what is not proven (Safety.md), what is not claimed
  (Compliance.md), and the features not offered (a schema language, a
  network source, writing comments back) with the D-number that
  decided each. Each number on the page is read from the build options
  or a test, and a test checks the page against them, so it cannot
  drift.
* ⬜ **Ways to read: DOM, SAX, lazy, in-situ.** Corrected on
  2026-11-19 against the code: the readers today give **events**, and
  that interface is already public (`cf_doc_sink` in `cf_serialize.h`,
  a callback per table, array and value); the configuration library
  flattens them into its key space, which is deliberately not a tree
  (`docs/Architecture.md`, "What is deliberately not here"). So the
  modes are, all on the same lexers:
  * **Events (SAX)**: done; the base the others are built on.
  * **A document tree (DOM)** for the serialization library alone:
    W5-V1 builds one inside `configforge codec convert`; made a public
    type of the serialization library, read once and walked many
    times, for programs that want a document rather than a
    configuration.
  * **Lazy / on-demand** (simdjson's): a structural index first, values
    parsed only when asked for; for reading three keys of a large
    document.
  * **In-situ** (RapidJSON's): strings unescaped in the caller's own
    buffer, which the caller gives up for the document's life; no copy,
    no allocation for strings.
  Each mode is measured on the benchmark page, and each is a
  `CF_WITH_*` option, so a small build keeps only the one it uses.
  Which formats support which mode is a table; a binary format (CBOR,
  TLV) is in-situ by nature.
* ⬜ **Arm supported well, added as devices can be reached.** Today CI
  runs arm64 only as macOS (`macos-latest`), no Linux arm64 job and the entry above adds armv7 and a
  Cortex-M build under qemu. The aim: AArch64 and 32-bit Arm (Cortex-A
  and Cortex-M) as first-class targets - NEON in the SIMD backend, SVE
  where present, unaligned access and alignment faults handled (the
  `memcpy` rule), big-endian Arm considered, and the `minimal` build's
  size per Cortex-M class in the README. Because not every device or a
  working qemu is always at hand, the support is listed per target with
  how it was shown: **tested on hardware** (named board, date),
  **tested under qemu**, **built only**, **not tried**. A target moves
  up the list as a device or an emulator run becomes available, and a
  test run on hardware is recorded with the commit it ran. Built-only
  targets are still built in CI so they do not rot. Its per-target list is
  `docs/targets.txt`.
* 🟨 **Small embedded systems: everything optional, a minimal build
  that stays minimal** (card A-11, b837c23: the `minimal`, `embedded-json` and `embedded-cbor` profiles exist; the Cortex-M stack gate is a CI job, L-18a, D-157; the `.text` gate remains). The library will gain many features (the modes
  above, SWAR and SIMD, gen's languages, every format); each one is a
  `CF_WITH_*` option, and none is required by the core. The `minimal`
  preset (today: codecs only, short limits, a 1 KiB stack budget,
  unreached code dropped) stays the floor, and CI holds it to numbers:
  its `.text` and largest stack frame on a Cortex-M build are gated,
  so a new feature that leaks into it fails. Beyond `minimal`, presets
  by need (`embedded-json`, `embedded-cbor`: one format, DOM or SAX,
  fixed arena, no heap, no stdio), and the README's table gives each
  part's size, so a user adds up their build before making it.
* 🟩 **Profiles: features turned on and off by name** (card A-11, D-105). The options
  grow (`CF_WITH_*` for each format, mode, backend and source), and a
  user should not have to learn forty switches to get a sensible build.
  A profile is a named set of options, chosen with one setting
  (`-DCF_PROFILE=embedded-json`, or a CMake preset of the same name):
  `full` (everything that builds on the host), `default`, `minimal`,
  `embedded-json`, `embedded-cbor`, `misra` (Compliance.md),
  `server` (every format, SIMD with run-time dispatch, threads). A
  profile is a file in `cmake/profiles/`, one option a line, so a user
  can copy one and keep their own beside their project
  (`-DCF_PROFILE_FILE=...`). Any option set by hand after the profile
  wins over it, and the configure step prints the profile, every option
  that differs from it, and the resulting size estimate from the
  README's table. The chosen profile is written into
  `cf_build_options.h` and the build id, so a report says which build
  produced it. Each shipped profile is built in CI.
* 🟩 **The C version raised only where it must be, and said so** (card A-11, D-106). C89
  stays the floor for the core: code already written in C89 is not
  rewritten for a newer standard unless it has to be. Where a part
  needs more - a dependency's header (libfyaml needs C11 today, the
  macOS user defaults C99), SIMD intrinsics and run-time dispatch (C99
  at least), `_Atomic` or threads - that part alone is compiled at the
  higher version, through one CMake helper
  (`cf_source_needs_c(<file> 99 REASON "...")`), never by raising the
  whole target. The helper prints a configure-time warning naming the
  file, the version and the reason (dependency, optimization,
  platform), records it in the build id, and adds it to a list a gate
  checks: a source compiled above C89 that is not on the list, or a
  list entry whose reason is empty, fails. A profile that must stay C89
  (`minimal`, `misra`) refuses any part on the list, by name, at
  configure time. `docs/Limits.md` shows the list.
* 🟨 **Formats registered, with what each can do.** What each writer can hold is done, as one table the converter, the tests and the page read (D-109); the ways to read it are left. `docs/Limits.md` links the pair table and does not copy it (A-13). The codecs already
  are a table (`cf_codecs[]` and `cf_formats[]` in
  `lib/serialize/src/codec.c`, read through `cf_codec_by_name` and
  `cf_codec_at`). What is left is the capabilities beside each entry:
  the ways it can be read (DOM, events, in-situ, lazy) and what it
  cannot hold. `detect.c`, Doctor, `codec convert` and the pair test
  (W5-V2) then read one table, and the pair table is generated from it
  rather than kept by hand (`docs/Limits.md` links it). To
  check first how much of this `cf_format_info` holds today.
* 🟨 **The YAML reader: no second document table** (card N-4: one root table delivered to a sink; the TOML and INI limits left). At the end of every
  file the reader says an empty document table again (`yaml_stream_end`
  takes the depth of -1 after the top-level mapping for a stream with
  nothing in it), so a sink that takes one root, as `codec dump` shows and
  as a converter's tree would, refuses it. `configforge codec convert`
  and the pair test tolerate it (D-109); done when the reader says the
  empty document only for a file with nothing in it, with a test, and the
  two tolerances come out. The same pass: TOML's reader limits the dotted
  key of a line and INI's reader limits no depth, which
  `cf_format_limits` records and which could be made equal.
* 🟨 **One diagnostic model across formats** (card A-10: place and reason built; severity and weakness ids left). Each reader reports a
  refusal its own way today, as far as the formats differ; a single
  diagnostic record (code, severity, byte offset, line and column, the
  format's own detail, and the CWE or known-weakness id when the
  refusal is a defence) used by every reader, the report and Doctor,
  so a test of "the same bad input refused the same way in every
  format" can be written, and a user filters by code rather than by
  message text. To check first how far the readers already share one.
* 🟩 **A feature matrix generated from the build** (card A-12; `gate.matrix`). The README's
  tables of options, sizes, profiles and per-target status (Arm entry
  above) are produced by a script from the CMake options, the profile
  files and CI's results, and a gate fails when the committed tables
  differ from what the script makes, so the documentation of what can
  be turned off never falls behind the options themselves.
* 🟨 **Differential fuzzing against the other readers** (card N-12, d8a0cfa: YAML against libyaml, a nightly exploratory target; JSON, TOML, XML and CBOR remain). The same input
  goes to ConfigForge and to another library's reader for the format
  (yyjson or RapidJSON for JSON, libyaml for YAML, toml++ for TOML,
  expat for XML, tinycbor for CBOR). Where one accepts and the other
  refuses, or both accept with different values, there is either a bug
  or a confusion an attacker can use (two programs reading one file
  differently). Each such case found is a test here, with the decision
  of which reading is right and why. Built on the fuzz targets that
  exist, in a CI job of its own, since it links the other libraries.
* ⬜ **OSS-Fuzz.** ClusterFuzzLite runs on every change (`cflite`);
  OSS-Fuzz would fuzz the project continuously on Google's machines,
  report crashes privately, and give a public, checkable "fuzzed by
  OSS-Fuzz", a claim the README can make. An application to the
  project, with the existing fuzz targets and their build script.
* ⬜ **An evidence pack with each release.** One archive attached to a
  release: the MISRA compliance summary and deviation records, the CERT,
  CWE and known-weakness tables, statement, branch and MC/DC coverage,
  the mutation score, fuzzing hours and corpora sizes, the benchmark
  against the other libraries, the safety manual, and the provenance
  and reproducible-build results. Everything an integrator's safety
  case asks the library for, from one place and for one commit.
  Built by the release workflow from what the gates already produce
  (`docs/Compliance.md`).

### For the people who change configuration (side discussion, 2026-11-19; not yet cards)

Most of the people a configuration library touches do not write code:
they change configuration, and the incidents they live with start
there. ConfigForge is to be a **lens over the stores they already use**
(git, files, Consul, etcd), not a store of its own: a configuration
server would turn a C library into a service to run, against the
critical-system aim, and the field of stores is crowded (Consul, etcd,
AWS AppConfig, Spring Cloud Config; the name "Vault" is HashiCorp's).
The lens is meaning: a diff of values rather than of text, provenance,
the contract's checks, secrets kept out. Four first steps, each mostly
from parts that exist:

* 🟨 **`configforge diff a.yaml b.json`: two files, any formats, compared
  by value.** What exists: `configforge doctor diff` compares two
  *reports* (a program's resolved configuration, `cf_report`).
  **Flat slice built (N-1):** `configforge diff` compares two plain
  scalar mappings across readable formats, ignoring key order and
  scalar spelling. It lists changed keys without values or digests.
  Nested tables, named by their whole path, were built in N-5. The first
  array slice compares named arrays of scalars (card N-8), and N-14
  compares arrays of tables element by element. What is left: arrays of
  arrays, contract-aware secret digests.
  With a contract, units compared as values (`30s` is `30000ms`, with 1.10.0's durations
  and sizes), which plain semantic diffs (dyff, graphtage) do not do.
* 🟩 **Git shows configuration changes by value** (card N-3: the difftool recipe). N-3 adds a
  [repository-local Git difftool recipe](Tools.md#git-difftool-for-json)
  for modified flat JSON mappings. It lists changed keys without values.
  The `textconv` driver for `git diff` and `git log -p` is built (card N-16:
  `configforge codec textconv`; values shown, unsorted, a refused file shown
  as it is). What is left: sorted output, and contract-aware redaction.
* 🟨 **A pull request that changes configuration, reviewed without a
  report** (card N-15, dfc1fed). What exists: `actions/doctor` with `base` and
  `comment: true` diffs the base branch's report against the head's and
  keeps one comment on the pull request up to date; with `config:` it
  also diffs each matching file's base and head versions by the command
  above and keeps a second comment that lists the changed keys per file
  (N-15, 1.9.0; keys only, never a value). What is left: checked against
  the contract when one is in the repository, and the comment saying
  which keys are out of range and which touch a secret.
* 🟨 **Drift across many hosts.** What exists: `doctor diff` compares
  two environments, origin drift is CFD002, and `doctor drift` (N-17,
  1.9.0) reads two to 1024 reports, one a host, and says for each key the
  hosts do not all agree on which reports hold which value, the odd ones
  first; a secret is never shown, and the exit status says whether the set
  is the same. What is left: the source of a value is not compared, a list
  file for more reports than a shell passes, configuration files rather
  than reports (`configforge diff` over N files), and the Python binding.
* 🟨 **Market research before the next step.** Done on 2026-11-19 in
  [Market.md](Market.md): fields 1 (a diff that knows the contract) with
  3 (pull requests) and 5 (embedded configuration checked at build time)
  look open; structural diff and Kubernetes policy are crowded, so the
  project adds the contract and provenance to dyff, conftest and gitleaks
  rather than competing with them. Left: confdiff's real features and
  licence, and the rows marked unconfirmed. The question, as asked:
  who already sells what to these people, and where nobody does: configuration languages (Cue,
  KCL, Pkl), policy (OPA and conftest), diff tools (dyff, graphtage,
  difftastic), stores (Consul, AppConfig), and GitOps (Argo, Flux).
  Candidates for a field with no rival, to test against that list:
  a diff that knows the contract (units, ranges, secrets) and says why
  a value is what it is; and configuration for embedded firmware,
  checked at build time with its provenance.

### Considered, and decided against for now: a GitHub wiki

The documents live in `docs/`, are reviewed with the code, checked by
the gates (`gate.apiref`, the wording check) and published by the docs
workflow to GitHub Pages. A wiki is a second repository outside all of
that, and would drift from the code it describes. Questions go to
Discussions (Q&A).

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

### Next after 1.8.0 (2026-11-24)

The eight items that would help users most, from the sections marked
"not yet cards", in this order. None needs a decision from the
maintainer; each is a card to write.

1. **Differential fuzzing against the other readers** (card N-12;
   Suggestions of 2026-11-17). Begun: YAML against libyaml (N-12,
   d8a0cfa); JSON, TOML, XML and CBOR remain. The libraries of P-1b are already built beside ours, so
   the same input can go to all of them; a disagreement is a bug in one
   of them, found before a user finds it.
2. **One diagnostic model: severity and weakness ids** (card N-13)
   (Suggestions of 2026-11-17). A-10 gave every refusal a place and a reason; the rest
   makes "the same bad input refused the same way in every format" a
   test that can be written.
3. **`configforge diff` with the contract** (card N-14: arrays of
   tables) (People who change configuration). Arrays of tables are done
   (N-14); arrays of arrays, then units compared as values (`30s` is
   `30000ms`) remain; N-1, N-5 and N-8 did the scalar cases.
4. **A pull request that changes configuration, reviewed without a
   report** (People who change configuration). Done in 1.9.0 (card N-15):
   the comment lists the changed keys. What waits is the contract read by
   `configforge diff`: which keys are out of range and which touch a
   secret.
5. **Mutation testing on the core and the readers** (Suggestions of
   2026-11-17). Coverage says a line ran, not that a wrong line fails a
   test; the score beside coverage shows where the tests are weak.
6. **`gate.misra` on the `misra` profile** (Critical-system standards).
   Done in 1.9.0 (card N-19): the addon runs on every commit in the
   `profiles (misra)` job, and a report the record lacks, or a record line
   that matches no report, fails it. What waits is the matrix of every
   rule, the public headers with no `backlog` line, and the compliance
   report.
7. **Drift across many hosts** (People who change configuration).
   Done in 1.9.0 (card N-17): `doctor drift` groups N reports by value, per
   key. What waits is the source of a value, a list file, N configuration
   files and the Python binding.
8. **Git's `textconv` driver for configuration** (card N-16) (People who change
   configuration). N-3 gave the difftool for flat JSON; a driver makes
   `git diff` and `git log -p` show changes by value in every format.

* **Three more differences between the YAML readers, "to decide"** (found by
  N-12's differential fuzzer against libyaml, not examined): a literal block
  scalar whose last line is only spaces, with no line break after it (the
  built-in parser ends its text with a line feed, libyaml does not; base64 in
  N-12's notes), and two inputs libyaml accepts and the built-in parser refuses
  (a block scalar header at the mapping's own column; an anchor followed by
  tabs; both in N-12's notes, items 4 and 5, removed with the card in 70ad1c3: `git show 70ad1c3^:docs/plan/N-next.md`, lines 302-314). A lone CR inside a
  comment, once listed here, is closed by N-20: the comment skip ends it at LF
  or CR, and rows hold that.

After the features above (the maintainer, 2026-11-24):

* **The vector library carried further** (the maintainer, 2026-11-24: it
  can pay). P-2 (on its branch, waiting for 1.9.0) gives lanes a
  word-at-a-time backend; four of six operations won by 1.4 to 7 times on
  their own. Next, in order: P-3 puts those operations under the readers'
  own scans (JSON strings, YAML plain scalars, the UTF-8 check) and keeps
  each only where the readers' benchmark shows it; then a measured case
  for SSE2 and NEON backends behind the same `lanes_ops` table (both are
  the baseline of their targets, so no detection is needed there), which
  is the `CF_WITH_SIMD` decision listed below as the maintainer's. The
  numbers decide each step, as for P-4.
* **Profiling after every benchmark, and the optimisations it points to.**
  A benchmark says how fast; a profile says where the time goes. Each
  benchmark run is followed by `scripts/work/profile.py` (perf, callgrind,
  heaptrack) on the same documents, and a change is made only where the
  profile shows the cost and kept only if the benchmark shows the win (as
  P-4 found the wipe, not the allocator, was 70% of a small load). The
  profile is also read for the library's own faults: a cost that should
  not be there is a bug before it is an optimisation.
* **The OpenSSF Scorecard score raised** (low priority). `scorecard.yml`
  runs weekly (L-2); its findings (branch protection, signed releases,
  pinned dependencies, a security policy, code review) are read one by
  one, the ones the project can meet are met, and the rest are written
  down with why.

Left for the maintainer, because each needs a decision: OSS-Fuzz (an
application to Google), the evidence pack with each release (a release
step), ways to read (DOM, SAX, lazy, in-situ: a new public interface),
SIMD behind `CF_WITH_SIMD` (a new build option and its targets), and a
Go or .NET binding (a new binding).

### From the maintainer, 2026-11-26 (not yet cards)

* ⬜ **Reads and writes at the same time, guarded by choice.** Today reads
  of a built configuration are safe at once from many threads; a mutation
  or a reload needs the program's own lock (Decisions.md, "Reads are safe
  concurrently"), and nothing guards a file two processes write at once.
  To weigh, as an option the program asks for: an advisory lock on the
  file written (`flock`/`fcntl`, `LockFileEx`) around F-1a's whole-or-
  nothing write, a lock taken by a reload, and what the library does when
  the lock is held (wait with a bound, or refuse).
* ⬜ **Compression.** A document or an image read from, and written to,
  a compressed file (gzip, zstd), chosen by name or by magic bytes, as a
  composed codec (D-103) so no reader changes; a new optional dependency
  each, behind its own build option.
* ⬜ **An analysis tool for configuration files.** Doctor already lints a
  report, diffs two and finds drift; this asks for more: static analysis
  of configuration files (unused keys, keys the contract lacks, values a
  rule forbids, secrets in plain text, what one file overrides in
  another) and dynamic (what a running program read, misread or never
  read). Research first: what exists (conftest/OPA, cue vet, kubeconform,
  yamllint, checkov, Spring Boot's configuration processor, systemd's
  `analyze verify`), what each finds, then the same first and what only
  the contract can add after.
* ⬜ **A configuration compiler.** Not only merge, parse and convert,
  but compile: sources, contract and rules in, one checked artifact out,
  with what a compiler gives (a front end, an intermediate form, passes
  that check and fold, diagnostics with places, a target chosen). Not yet
  defined, and its competitors not yet known. What exists here to start
  from: the image format (docs/ImageFormat.md), a baked configuration
  read with no parser linked (D-104, layer 1), `configforge gen`. Research
  first: CUE, Jsonnet, Dhall, Nickel, Pkl, KCL, HCL with Terraform's
  planner, Nix, and what each compiles to.
* ⬜ **Threads and processes made easier.** Programs that read
  configuration will use threads and processes; to weigh what the library
  can give them: a configuration shared by many threads and swapped on
  reload without a lock on the read path, a child process handed the
  same configuration (an image in shared memory or a file descriptor),
  workers told of a reload. Research first: what other libraries and
  runtimes do (RCU-style swaps, Go's atomic.Value, Java's
  volatile-reference reload, systemd's socket and file-descriptor
  passing, nginx's master and workers).

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
