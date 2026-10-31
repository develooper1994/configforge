# Roadmap

The state of the work, kept true in the same commit as the work - so that
whoever picks this up next, on whichever machine, knows what is next,
what is known to be wrong, and what has been thought of and not built.

Each item says **why** it matters and **what done looks like**. When an
item lands, move it to the CHANGELOG and delete it here; a roadmap that
lists finished work stops being read.

## Known issues

None open. The last, "Darwin's kqueue has not been seen to pass", was
answered by the v0.3.0 release run: its macOS (arm64) job ran the whole
suite, 88 of 88, `test_watch` among them, with kqueue the change handle
on Darwin; `src/platform/posix/posix.c`'s kqueue code is unchanged since.
What Darwin has not run is what came after - `cf_live`'s waits and the
ConfigMap test - and FreeBSD's real kqueue is the check for those
(item 3 below) until a macOS job runs again.

## Next: 1.0.0

The plan, in order (2026-10-30). GitHub Actions is blocked by billing,
so each CI job is run locally, as the maintainer decided (`docs/Maintaining.md`).
Each unit passes tier 1 (D-50); tier 2 runs before each release.

Done since the plan was written, each in its own commit: the interface
review's findings (D-51); a log for what is not an error (D-52); the
C standard the program uses (D-53); create, update and delete as one
edit (D-54); zero-copy text in Rust and a `Mapping` in Python; no
string borrowed past its call; `docs/Safety.md`, hardened builds and
Valgrind; `gate.text`; bounded proofs of the integer parsers and
printers, type inference and `cf_fmt`; WebAssembly (D-55), which found
an image read past its end on every 32-bit platform. Left, in order:
v0.4.0, real kqueue, the documents, 1.0.0.

### Resume here (2026-10-31)

Whoever continues: read `CONTRIBUTING.md`, then do these in order.

1. **Tier 2** passed on 92cd643 (all green, 18 minutes, without the
   ABI step, which came after). After a rebuilt environment, run
   `scripts/buildenv/setup.sh` first (it now installs `abigail-tools`).
2. **The ABI step** (D-56) ran first on 2026-10-31 against v0.3.0 and
   reported the 0.4.0 changes, as it should before 1.0. After v0.4.0 is
   tagged, run it again: it should then report only what changes after.
3. **v0.4.1** (item 1): tagged and pushed; 0.4.0 was tagged and not
   released (its MSVC Release build refused a test, CHANGELOG 0.4.1).
   The four archives and `SHA256SUMS` are built in `D:\cf-dist\v0.4.1`
   on the maintainer's machine, the notes in `D:\cf-dist\notes-0.4.1.md`;
   publishing waits for the maintainer's go-ahead:
   `gh release create v0.4.1 <the files> --title "ConfigForge 0.4.1"
   --notes-file <notes>`. Then `cmake -DTO=1.0.0 -P scripts/version.cmake`.
4. **Real kqueue** (item 6).
5. **The documents** (item 7). An audit (2026-10-31) found, each
   checked against the code:
   * README: lines 139-140 and SECURITY.md:15 say 0.x promises nothing -
     rewrite as the 1.x promise (D-56); 459 "Two are still open" is one
     (D-12); 218-220 `minimal` keeps the codecs on, and the optimised
     presets are `portable` and `minimal` (also CMakePresets.json:71);
     304-305 and 372: version.cmake and gate.version cover the Cargo and
     Python manifests too, and the examples should say `x.y.z`, not
     0.3.0 (also scripts/README.md:10 and scripts/version.cmake's
     comments); 223-225 options miss `CF_WITH_YAML`, `CF_PLATFORM`,
     `CF_RUST`, `CF_HARDEN`; the gates table misses `gate.abi`,
     `gate.apiref`, `gate.text`, `gate.dependencies`, `gate.sbom`.
   * Guide: 646 twelve C++ examples, not six; 46 add `live.rs`; 86-88
     add pcc; 22 the contents entry for section 13 (C++, Rust, Python);
     337 "INI, JSON, TOML and YAML can also be written"; 30 and 642 the
     C++ header is C++98 to C++23, not a "C++03 wrapper" (also
     CONTRIBUTING.md:11).
   * CONTRIBUTING: 24-28 the source table is behind `src/`; 36 the
     scripts list misses version, buildid, release-local, check_text,
     proofs; 244-251 `release-local.sh` downloads too (alpine, musl's
     licence).
   * Maintaining: 90 and 113-114 Rust is in `setup.sh` now; 19-22 date
     the Actions-billing note; 23-25 keep only "commits and files carry
     their maintainers' names"; 105 rename `D:\cf-handoff` to
     `D:\cf-tools`. scripts/README.md:10 misses the newer EXTRA steps.
   * Doctor.md:40 "runtime agent" -> "runtime daemon".
   * docs/API.md is generated (`scripts/apiref.cmake`) and matches every
     signature; the fixes go in the header comments or the generator,
     then regenerate. Wrong: cf_live.h:17's example passes five
     arguments to `cf_live_open`, which takes six; configforge.h:56-58
     says file sources have not landed (say cf_app reads no file, use
     cf_app_builder + cf_builder_file); cf_error_clear's text is
     "clang-format on" (comment it; make the generator skip
     clang-format lines); `"edit-refused"` missing from the event lists
     in cf_log.h:129 and cf_live.h:111; a refused `cf_live_commit` is
     not recorded in status or last_error - say so or record it;
     CF_LIVE_OPTIONS_INIT sets log_min, not only zeros; cf_edit_begin
     also returns NULL on a NULL argument and always uses the default
     allocator, cf_edit_set_text refuses NULL (CF_E_CONVERT); the D-51
     INTERNAL notice for cf_defset is lost above a typedef;
     cf_app_builder has no comment of its own; `CF_NODISCARD` missing on
     `cf_derive_in` and `cf_watcher_open` (add before the freeze).
     Missing: the C11 `cf_get`; comments lost behind forward
     declarations (cf_builder_file_codec, cf_live_commit,
     cf_source_defaults, cf_source_file_codec - move the `struct x;`
     lines up); the flag-bit rule (definition bits 0-15, source 16 and
     up); the error codes of cf_code.def as a table; the members of the
     sized structs (cf_error, cf_live_options, cf_live_status,
     cf_log_event); an intro paragraph naming the internal tier
     (cf_flatten, cf_keyspace, cf_numeric, cf_platform, cf_define's
     set, cf_port's ref/fmt/cmp). Before the freeze, decide:
     `cf_report_info` and `cf_doctor_gate` are caller-allocated with no
     `size`, so they cannot grow in 1.x (D-51); NULL handles return
     CF_E_SOURCE except `cf_live_approve`, CF_E_CAPABILITY.
6. **1.0.0**: tier 2, CHANGELOG dated, tag, release as in step 3.

1. **v0.4.0, released locally** (`scripts/release-local.sh`, `.ps1`):
   source, amalgamation, static musl Linux, Windows; the macOS archive
   needs a Mac, and the release notes say so.
2. **The interface fixed before it freezes.** An independent review of
   every public header and both bindings (2026-10-30) found what 1.x
   would be stuck with; each is fixed or decided:
   * `cf_error.size` is never read, and `cf_def`'s size check runs the
     wrong way: a struct that grows in 1.x would overrun a 1.0 caller's.
     Every sized struct copied by the smaller size; `cf_def_init` by
     the caller's.
   * `cf_live_status_get` accepts only its exact size; its error moves
     to `cf_live_last_error`.
   * the interfaces callers implement - `cf_source_vtable`, `cf_codec`
     - carry a size, so 1.x can add a member.
   * `CF_OPTIONAL` and `CF_REQUIRED` share a bit and reach the same
     `flags` parameters; source flags move up, and unknown bits are
     refused.
   * `cf_live_open` grows through a sized options struct.
   * internal layers (`cf_platform_*`, `cf_ks_*`, `cf_flatten*`) are
     exported: an internal tier outside the 1.x promise, hidden where
     the platform allows.
   * structs callers allocate that expose internals (`cf_iter`,
     `cf_image`, `cf_keyspace`, `cf_defset`): reserved room, or opaque.
   * the C++ `Live` lets an exception unwind through C; the Doctor
     finding view is one reused struct; `cf_builder_replay` has no
     flags; header and implementation disagree in three places in
     `cf_live`.
   * Rust: `sys` hidden, `#[non_exhaustive]` on what can grow, no panic
     on a NUL in a key. Python: one convention for properties, and a
     frozen method set before attribute reads.
3. **What 1.0 adds** (the owner, 2026-10-30), each assessed:
   * **The C standard the user compiles with**, as the C++ header does:
     the core stays C89, so it builds everywhere; the headers add, where
     the standard has them, C11 `<stdatomic.h>` atomics (a portable path
     beside the GCC builtins), `_Static_assert` on the sizes the ABI
     promises, a `_Generic` `cf_get(cfg, key, &out)` typed by `out`, and
     C23 `[[nodiscard]]` on every call whose result is an error.
   * **The standard library of each language**: Python's `Config` a
     `collections.abc.Mapping`, Rust's the standard traits (`Index`,
     `IntoIterator`, `Debug`, `TryFrom`), C++'s already ranges and
     `std::format`. Not the locale-dependent parts of C's (`<ctype.h>`,
     `strtod`): they read one file two ways on a Turkish system, which
     is why they are on the forbidden list.
   * **One CRUD accessor**: create, update and delete are spread over
     `cf_derive` and `cf_write` today. `cf_edit` - begin from a
     snapshot, set and unset keys, commit - checks the whole change
     against the contract, writes it to the store with its note in one
     transaction, and hands the new snapshot to a `cf_live`.
   * **Copies avoided where the lifetime is enforced**: text is already
     a view into its snapshot; Rust's reads return `&str` borrowed from
     the `Config`, so the compiler holds the lifetime. Not memory-mapped
     files: one changed under the mapping kills the process.
   * **Errors and logging at a professional level**: a structured log
     sink (`cf_log`: level, code, key, source, message) for what is a
     warning rather than an error - an unknown key under
     `CF_UNKNOWN_WARN`, a deprecated key, each reload's result -
     delivered to Python's `logging`, to a Rust and a C++ callback; and
     every error code stable, named and documented.
4. **Rust-level safety, for the C.** The owner asked for it: what Rust
   proves at compile time, proven here by tools and kept proven - a
   written safety model (`docs/Safety.md`: ownership, bounds, the
   forbidden list, what each tool covers); hardened release builds
   (`_FORTIFY_SOURCE=3`, stack protector and clash protection, CET,
   full RELRO); bounds checking in the sanitize preset
   (`-fsanitize=bounds`); **no string borrowed past the call that took
   it** - a builder copies every name, path and prefix it is given into
   its own arena, so a caller's buffer freed after `cf_builder_file`
   cannot be read later, and the bindings stop keeping every reload's
   strings for the life of a `Live`; Valgrind's memcheck as a tier-2 step for what
   ASan cannot see; and bounded proofs (CBMC) of the parsers' core
   functions - the UTF-8 decoder, `cf_fmt`, the number parsers - that no
   input reads or writes out of bounds, and of `cf_live`'s lock that
   it excludes and cannot livelock. Memory and thread errors are the
   kind found late and fixed at great cost, so the claim is what can be
   proven, written in `docs/Safety.md` with the tool that proves each
   part, and kept by the tier-2 run.
5. **WebAssembly**: `wasm32-wasi` through clang and wasi-libc, the
   suite run under wasmtime, single-threaded and with no change handle -
   a tier-2 target, and the library in a browser or an edge runtime.
6. **Real kqueue on the new code**: `scripts/buildenv/freebsd.sh` with
   `test_live` (its waits and the ConfigMap test) and `test_watch`.
7. **The documents audited** against the code (README, Guide, API,
   Maintaining), then version 1.0.0, tier 2, and the release.

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

None. When to call it 1.0 was decided on 2026-10-30: when the plan
above is done, with authority given to finish it.
