# Roadmap

The state of the work, kept true in the same commit as the work - so that
whoever picks this up next, on whichever machine, knows what is next,
what is known to be wrong, and what has been thought of and not built.

Each item says **why** it matters and **what done looks like**. When an
item lands, move it to the CHANGELOG and delete it here; a roadmap that
lists finished work stops being read.

## Known issues

Things that are wrong or unproven today, most serious first.

1. **Darwin's kqueue has not been seen to pass.** The kqueue path passes
   against FreeBSD 14.4's own (`scripts/buildenv/freebsd.sh`: all 41
   unit tests, `test_watch` with the scenario libkqueue cannot run), so
   the code is right for a real kqueue; macOS's is a different kernel's,
   and the macOS CI job is its only check, whose result this machine
   cannot see. *Done:* the macOS CI job seen green.

## Next

2. **1.0**, once 1 is done: an ABI check in CI (abidiff against the
   last release), and semantic versioning from then on. The symbol list
   is built (`cmake/configforge.symbols`, `gate.abi`), so a change to the
   interface is already a line in a diff. Before 1.0 is when an
   interface mistake is cheap, and a review of the 0.4.0 interface left
   three to decide, all decided: `CF_OPTIONAL`, `cf_write_item`, and the
   limits as part of the ABI (D-45).

## Ideas

Worth building, in the order they serve the library's purpose.

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
* **Native OS sources** (D-30): macOS defaults and gsettings, each when
  there is the platform to test on. The Windows Registry is built.
* **A Python binding for the configuration library**, as Rust's is
  (`bindings/rust`). Doctor already has one (`bindings/python`), over
  the program; the library would need ctypes over a shared build.

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

* When to call it 1.0 (item 3).
