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

### Resume here (2026-11-01)

Where it stands: the three release corrections passed the Linux and
Windows verification, including format round trips and the 1.x ABI
comparison. Standalone Python and Rust packages passed their installed
tests on Linux, Windows and macOS. Searchable documentation reads the
same Markdown as the repository (D-59). Scalar lists cover flat inputs,
append merging, reloads, writes and the language bindings (D-02).
Their verification includes allocation failures, wasm32 and frozen
1.0.0 clients on 32-bit and 64-bit Windows. These changes are on `main`,
which is 1.0.1, unreleased; the existing 1.0.0 archives are unchanged.
Doctor history/blame is built (`docs/Doctor.md`, section 7c).

Before anything: read `CONTRIBUTING.md`, then run
`sh scripts/buildenv/dev-setup.sh --check` (Windows:
`pwsh scripts/buildenv/dev-setup.ps1 -Check`) and, if a tool is missing,
the same without the check.

Left, in order - each a verified commit (`CONTRIBUTING.md`, "How work is
done here"):

1. **Default-drift preflight**: a report records which default a key
   was given, so Doctor can tell a changed application default from an
   unchanged one, using observed reports rather than another resolver.
2. **Optional configuration read/use coverage**, with its secrecy,
   concurrency and binding checks.
3. **The remaining decisions and ideas**, only when selected, each
   addition a minor version.

### Accepted follow-up

These additions are agreed, not completed: preflight detection of
changed application defaults, and optional configuration read/use
coverage. Each needs implementation and tests; no release is a
substitute for them. Doctor history/blame, the first of them, is built.

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
