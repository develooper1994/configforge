# Roadmap

The state of the work, kept true in the same commit as the work - so that
whoever picks this up next, on whichever machine, knows what is next,
what is known to be wrong, and what has been thought of and not built.

Each item says **why** it matters and **what done looks like**. When an
item lands, move it to the CHANGELOG and delete it here; a roadmap that
lists finished work stops being read.

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
pushed `v*` tag *is* a release; tier 2 and `release-local.ps1` come
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

Chosen by the owner on 2026-11-05, in this order. Each is a minor
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

## The plan from 1.1.0 (the owner, 2026-11-05)

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
   Windows quick run; then `minimal` with
   `-ffunction-sections -fdata-sections` and `--gc-sections` (`/Gy`,
   `/OPT:REF`), the size before and after written down; `CF_LTO`, off by
   default, measured on the release build, the stack budget measured on
   its result.
3. **A benchmark, not a gate**: building N keys, `cf_get`, a report,
   `doctor diff` of two N-key reports. Then what it shows; the first
   expected is Doctor's linear key lookup, quadratic in `diff` and
   `history`.
4. **Arrays in INI, C style** (the owner's decision): `ports = {80, 443}`
   reads and writes an array; `ports[0] = 80` sets one element; both are
   the flat `ports.0` / `ports.count` underneath, so explain(), reports
   and every other format see the same thing. A decision record first.
5. **Doctor `--only-changed`**: rules restricted to the keys a change
   touches - the keys whose candidates differ between `--base` and the
   report - so a large existing configuration can adopt Doctor one
   change at a time. The action and the hook pass it through.
6. **Dependencies behind one contract** (the owner's decision): every
   reader a library supplies - JSON, TOML, YAML, XML, SQLite - sits
   behind the `cf_codec` / `cf_doc_sink` contract, and a conformance
   suite every implementation must pass (the same documents, the same
   events, the same refusals, the same limits), so a dependency can be
   replaced without a caller noticing. Then the best maintained library
   is chosen for each, by a written comparison (licence, maintenance,
   conformance, size, speed, fuzzing history): yyjson and tomlc17 are
   the candidates for JSON and TOML; libyaml, expat and SQLite are
   re-examined. The built-in readers stay, behind the same contract.
7. **JSON Schema import**: definitions from a schema, the reverse of
   `cf_schema`; unsupported keywords refused by name. After 6, on the
   chosen JSON reader.
8. **Sizing modes** (the owner, 2026-11-05: full-dynamic, semi-dynamic,
   manual): today every limit is a fixed buffer (`CF_KEY_MAX` and its
   siblings, D-37) - manual. Semi-dynamic: buffers start on the stack
   and grow from the allocator up to a cap. Full-dynamic: no length or
   depth limit but memory, decoders iterating rather than recursing,
   with run-time caps still there for untrusted input. A decision record
   (D-57) first: build option or run time, the default per profile, and
   what `cf_error`'s fixed `key` and `message` do - truncate in 1.x,
   grow in 2.0. Then the buffers, one subsystem at a time, each under
   all three modes in the suite. The largest item here.
9. **Modern C++ only** (the owner's decision): the header deprecates
   C++98 to C++14 in 1.x with a compile-time warning, and requires
   C++17 from 2.0, where `std::optional`, `std::string_view` and
   structured bindings simplify it. C++20 modules (`import configforge;`)
   come with 2.0 where the compilers in the matrix build them.
10. **The ideas, by use** - each its own commit, in this order:
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
the order the owner and its use put it. New ones go here until they are
accepted.

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

## Decisions waiting for the owner

* **Arrays in INI** - decided by the owner (2026-11-05): C style; the
  plan above, item 4.
