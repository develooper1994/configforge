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

A deferred item remains open. Passing the release matrix does not
complete an unimplemented feature, and publishing a version does not
complete this roadmap. No further release is planned until the agreed
work and its validation are complete.

## Next: 1.x

1.0.0 is released: from here the C interface follows semantic versioning
(D-56), a patch fixes, a minor version adds, and `verify -DEXTRA=abi`
fails an incompatible change within 1.x.

### Resume here (2026-11-01)

Where it stands: v1.0.0 is tagged from the commit that dated its
CHANGELOG heading, after tier 2 and the Windows Release build passed on
its tree, and the release workflow built and published its archives,
macOS's among them. CI's `shared (windows-latest)` job had failed on
every push since 2026-10-31 in `gate.abi`, which read a DLL with nm; it
reads the import library now. Everything is on `main`, which is 1.0.1,
unreleased.

Before anything: read `CONTRIBUTING.md`, then run
`sh scripts/buildenv/dev-setup.sh --check` (Windows:
`pwsh scripts/buildenv/dev-setup.ps1 -Check`) and, if a tool is missing,
the same without the check.

Left, in order - each a verified commit (`CONTRIBUTING.md`, "How work is
done here"):

1. **Complete the release follow-up**, including compatibility and
   format-conformance checks for the corrected readers and writers.
2. **The decisions below**, once the owner has made them.
3. **The ideas**, in their order, each a minor version.

### Accepted follow-up

These additions are agreed, not completed: installable Python and Rust
packages, searchable documentation, Doctor history/blame across a
sequence of reports, preflight detection of changed application
defaults, and optional configuration read/use coverage. The original
flat-source list forms and append merge below also remain open. Each
needs implementation and tests; no release is a substitute for them.

## Ideas

Worth building, in the order they serve the library's purpose.

* **A documentation site** as the best-kept projects on GitHub have one:
  the Markdown in `docs/` rendered to GitHub Pages - navigation, search,
  the API reference from the headers, the examples with their checked
  output - built from the same files, so the site cannot drift from the
  repository. The Markdown is the source until then.
* **A reload diff as JSON** (`cf_diff_json`), for log pipelines that
  index fields rather than lines: the key, old and new value (redacted
  as the text diff is), and the source each came from.
* **Settling a burst of writes**: an editor saving twice, or a
  deployment writing three files, is three wake-ups. Quiet swaps
  already make the repeats cheap; a settle interval in `cf_live` would
  make them one reload, and one log line.
* **Packages that carry the library**: a Python wheel with the shared
  library inside (scikit-build-core), and a crate that builds from C
  sources inside its own package so it can be published. Both are
  packaging steps over code that already finds the library either way.
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
* **Lists from a flat source**: D-02's append mode, and Design.md's
  separator form (`MYAPP_TAGS="a,b,c"`) and repeated flag (`--tag a
  --tag b`), were designed and never built; the documents say so. A list
  from the environment or the command line is index keys and a count.
  Done looks like a definition that declares a separator, the two
  sources expanding to index keys and a count as they are read, and a
  merge mode that extends a lower source's list, each with a test.
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
