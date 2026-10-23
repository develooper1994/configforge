# Roadmap

The state of the work, kept true in the same commit as the work - so that
whoever picks this up next, on whichever machine, knows what is next,
what is known to be wrong, and what has been thought of and not built.

Each item says **why** it matters and **what done looks like**. When an
item lands, move it to the CHANGELOG and delete it here; a roadmap that
lists finished work stops being read.

## Known issues

Things that are wrong or unproven today, most serious first.

1. **The kqueue path has run only against libkqueue**, which reports
   more than a real kqueue for in-place edits and less for new
   directories. *Done:* the macOS CI job green.
2. **Nothing measures test coverage**, so nobody knows which lines no
   test reaches. *Done:* a coverage build, and a gate on the core.
3. **The fuzzer is deterministic mutation only.** Good for regression,
   weak at finding new paths. *Done:* libFuzzer harnesses for every
   reader of untrusted bytes - JSON, INI, TOML, `.env`, image, report -
   run for a bounded time in CI.

## Next

4. **A codec from outside the library.** The document-event interface
   (`cf_doc_sink`) is already format-neutral: begin a table, begin an
   array, a scalar with its location, end. A streaming parser in
   another library - libyaml's events, expat's callbacks - maps onto it
   one to one, which is the whole of what an integration needs. What is
   missing is a way to hand such a codec to a builder: codecs are a list
   fixed at compile time, and a registry that code could add to at run
   time would be global mutable state, which the library forbids.
   *Done:* `cf_builder_file_codec(b, name, rank, path, codec, flags)`,
   taking the codec explicitly, and a test that defines a codec outside
   `src/` and reads a file through it. Architecture.md has the recipe.
5. **Packaging**: a pkg-config file, a vcpkg port and a Conan recipe,
   each installed and consumed by a test project in CI.
6. **API reference** generated from the header comments.
7. **1.0**, once 1-4 are done: a symbol list, an ABI check in CI
   (abidiff), and semantic versioning from then on. Before 1.0 is when
   an interface mistake is cheap.

## Ideas

Worth building, in the order they serve the library's purpose.

* **`cf_diff(old, new)`** - what changed between two snapshots, *with
  provenance*: `server.port: 8080 (default) -> 9090 (env
  MYAPP_SERVER__PORT)`. The watcher and `cf_derive` already make
  reloads; this makes them explain themselves.
* **JSON Schema from the definitions** (Design.md, section 41). Editors
  then validate and complete `settings.json` as it is written, so the
  contract reaches the person before the program runs.
* **Machine-readable `explain()`** for an administration endpoint, the
  way Spring's `/actuator/env` works.
* **Wiping sensitive values** when a configuration is released.
* **A ThreadSanitizer preset**, so the threading contract (D-14) is
  checked under real concurrency rather than reasoned about.
* **Doctor as a GitHub Action**, with SARIF upload, so adopting it is
  four lines of YAML.
* **More Doctor adapters**: .NET's `GetDebugView()`, Spring's
  `/actuator/env` (Doctor.md, section 12).
* **Doctor `--only-changed`** and **CFD012 against the contract** rather
  than the key list.
* **Value interpolation** (D-10), now that its syntax is reserved and
  refused - which makes adding it later an addition, not a break.
* **Native OS sources** (D-30): the Windows Registry, macOS defaults.
* **Bindings**: Rust, and Python for scripting Doctor.

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

* When to call it 1.0 (item 7).
