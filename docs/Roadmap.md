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

### Resume here (2026-11-02)

The agreed additions are implemented on `main`, which is 1.0.1,
unreleased; the existing 1.0.0 archives are unchanged. Doctor
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

Accepted work: the three items under "Accepted follow-up" (2026-11-05).
The ideas below them are not approved scope; if selected, each addition
is a minor version.

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
2. **Doctor on a pull request.** A composite GitHub Action and a
   pre-commit hook, both in this repository, that build or download
   Doctor, run `doctor diff` (and `doctor lint` against the
   definitions) between the base and the head, and write the findings
   as a pull-request comment or a hook failure: which key changes, which
   source would win in production, what a default drift (CFD017) or an
   unread key (CFD018) means. No new library code; values stay out of
   every message, as Doctor's already do. Tested by a workflow in this
   repository against fixture reports, and documented in Doctor.md.
3. **The configuration's fingerprint where operators look.**
   `cf_digest` already identifies a resolved configuration - quiet
   swaps use it - but nothing outside the process sees it. Put it in
   each reload's `cf_log_event` (a new member after `size`, the old and
   new digest), in the provenance report (an additive schema-1 field),
   and in Doctor's output, so an incident can be matched to the
   configuration change before it with one field. A sensitive value
   enters the fingerprint only as its blind-report digest, so the
   fingerprint never confirms a guessed secret. The Guide gets a recipe
   for carrying it as a log field and an OpenTelemetry resource
   attribute, with no dependency here.

## Ideas

Worth building, in the order they serve the library's purpose.

* **A reload diff as JSON** (`cf_diff_json`), for log pipelines that
  index fields rather than lines: the key, old and new value (redacted
  as the text diff is), and the source each came from.
* **Settling a burst of writes**: an editor saving twice, or a
  deployment writing three files, is three wake-ups. Quiet swaps
  already make the repeats cheap; a settle interval in `cf_live` would
  make them one reload, and one log line.
* **More languages over the same C ABI**: Go (cgo) and .NET (P/Invoke)
  are the next most asked-for, each a thin layer like Python's, checked
  for the same example lines.
* **C++20 modules** (`import configforge;`), once the minimum CMake and
  the compilers the matrix covers all build them; until then the one
  header is the interface.
* **The build id in the image header**, as the report now carries it:
  a change to the image format, so it waits for a reason to change it.
* **JSON Schema import** - definitions *from* a schema - the other
  direction of Design.md section 41, with unsupported keywords refused
  by name.
* **Limits in the report and the image header**, so a reader built with
  shorter keys than the writer says so by name rather than as a key it
  cannot add.
* **Doctor `--only-changed`** and **CFD012 against the contract** rather
  than the key list.
* **Established readers behind options**: the owner (2026-11-01) would
  rather a maintained library, behind a `CF_WITH_*` option, than one
  written here - as libyaml, expat and SQLite already are. A JSON reader
  (yyjson) and a TOML one (tomlc17), both MIT-licensed C, are the
  candidates; each passes the conformance, fuzz and round-trip tests
  the built-in reader passes, and the built-in readers stay the default,
  so a build with no dependency keeps every format it has today.
* **Native OS sources** (D-30): macOS defaults and gsettings, each when
  there is the platform to test on. The Windows Registry is built.

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

* **Arrays in INI**: INI has none, so an array is refused there. Its
  index keys and count, as plain keys, would round-trip through this
  library alone.
