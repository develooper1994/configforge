# The serialization library, the tools, and a smaller promise

The 1.7.0 milestone's own work (docs/Maintaining.md, "Versions") as
cards (docs/Plan.md says what a card is and how one is carried out).
Three pieces:

* **S-1, S-2**: what 1.x promises, made smaller (Roadmap, "From reading
  the project as a newcomer"): the internal tier fenced behind
  `CF_INTERNAL_API`, which a program does not define, and an
  experimental tier, `CF_EXPERIMENTAL`, for a function's first minor
  release; `gate.abi` holds the promised list apart from the others.
* **S-3, S-4, S-5**: the serialization layer as a library target of its
  own (Roadmap, "More formats", step 1): its last ties to the rest cut
  (S-3), the target with its own symbol list and version macros (S-4),
  its install, package component, pkg-config file and the packages
  (S-5). `libconfigforge` keeps carrying every one of its functions.
* **T-1 to T-5**: Doctor kept to its report commands, behind one front
  door, `configforge <command>` running `configforge-<command>`
  (Roadmap, "Tools, distribution and digests"): the front door and
  Doctor's new name (T-1), `configforge-gen` (T-2), the serialization
  library's own tool and its first commands (T-3), `dump` (T-4), and in
  1.8.0 the aliases removed (T-5). Converting a document is that tool's
  too: W5-V1, rewritten for it in docs/plan/W5-convert-and-gen.md, with
  the rest of wave 5 rewritten for `configforge-gen`.

Order: S-1, S-2, S-3, S-4, S-5, T-1, T-2, T-3, T-4; then wave 5
(W5-V1, W5-S1, W5-G1, W5-G2, W5-G3); then T-5, after 1.7.0 is
released. S-1 to S-5 go one after another (each changes `gate.abi` or
the CMake of the library). T-1 needs S-1 only and could run beside
S-2 to S-5, but shares `tools/CMakeLists.txt` with S-1 and
`CMakeLists.txt` with S-4: kept in order. T-3 needs S-5, since its
program links the serialization library alone.

Every card here belongs to 1.7.0, and 1.7.0 is opened after 1.6.0 is
released: the first card of this file carried out after that release
opens it, `cmake -DTO=1.7.0 -P scripts/release/version.cmake` (it
changes `cf_version.h`, the vcpkg, Cargo and Python manifests,
`Cargo.lock` and the CHANGELOG's newest heading; those files belong to
that card's commit, as H-5's notes say). Every CHANGELOG entry below
goes under `## 1.7.0 — unreleased`. Before 1.6.0 is released no card of
this file is started.

For the maintainer: three questions - in S-1 (whether the functions
the examples call out of the internal tier join the promise), in S-4
(the serialization library's name) and in T-3 (its tool's name). Each
card is written for its recommendation. Nothing to install.

Each card that adds a decision takes the next free D-number at the
time it is carried out (`git grep -n "^## D-" docs/Decisions.md`),
skipping D-94 to D-96, which wave 5's cards hold. The cards below call
theirs "this card's D-number" and say which later card adds to it.

Lessons from the hardening cards, which every card below is held to:

* The `minimal` preset fails the build on any library function whose
  frame is over 1024 bytes (`scripts/check/stack.cmake`); the tools are
  held to 2048 (`-Wstack-usage=2048`, `configforge_apply_warnings`).
  A buffer larger than a few hundred bytes is allocated, not declared.
* The amalgamation (`scripts/generate/amalgamate.py`) is every source
  in one translation unit: a new `.c` is listed in `SOURCES` and a new
  header in `HEADERS`; a `static` name must be unique across all of
  them; a feature-test macro must come before the first header of the
  unit, which in the amalgamation is the preamble `amalgamate.py`
  writes.
* CI's newest gcc makes an implicit declaration an error: a function a
  file calls must be declared where that file is compiled, which the
  fence of S-1 makes easy to get wrong.
* wasm32-wasi has no processes, no users and no `pwd.h`; every card
  says what its code does there (`verify -DEXTRA=wasm`).
* A changed header comment needs `cmake -P scripts/generate/apiref.cmake`
  and `docs/API.md` staged, last, after the final edit to any header or
  source.
* New C is built with `cmake --preset analyze` before it is staged.

---

## S-1 - The internal tier fenced: a program cannot call what 1.x does not promise

* **id**: S-1
* **goal**: the functions D-51 calls internal are declared only where
  `CF_INTERNAL_API` is 1 - the library's own sources, Doctor, the tests,
  the benchmark, the proofs - so a program that calls one does not
  compile, and `gate.abi` keeps them in a list of their own apart from
  the promised one.
* **why**: a program that calls `cf_ks_resolve` today compiles, links
  and breaks in the next minor release, and nothing told it so but a
  sentence in a header. What the library promises is what it can keep;
  a promise nobody can see the edge of is the one a reader cannot trust.
* **needs**: 1.6.0 released (this card opens 1.7.0, see the preamble)
* **size**: large
* **decided**:
  * **TO DECIDE - what the examples call out of the internal tier.**
    D-51 and the preamble of `docs/API.md` (written by
    `scripts/generate/apiref.cmake`) put `cf_port.h`'s `cf_fmt*` and
    `cf_platform.h` whole in the internal tier, and yet
    `examples/c/flags.c`, `examples/c/policy.c`,
    `examples/cpp/policy.cpp`, `tests/consumer/consumer.c` and
    `tests/consumer/digest.c` - what a program's author copies - call
    `cf_fmt`, `cf_platform_name` and `cf_platform_has_registry`.
    Options:
    1. **They join the promise**: `cf_fmt`, `cf_vfmt`, `cf_fmt_append`,
       `cf_platform_name` and every `cf_platform_has_*` query (today
       `_environ`, `_dir_list`, `_wide_args`, `_trusted_dir`,
       `_registry`, `_private_create`, `_wake`; any added by W3-F2a
       counts too). Eleven functions promised for the rest of 1.x;
       the examples and the consumer tests are unchanged. A query of
       what the build can do is what Architecture.md, "Adding a
       platform capability", says a caller must be able to ask.
    2. **They are fenced like the rest**, and the five files are
       rewritten without them (`cf_fmt` replaced by C89 `sprintf` into
       sized buffers, the platform name dropped from two lines of
       output). The promise stays as small as D-51 drew it.
    Recommended: **1**. The rest of the card is written for 1. If the
    answer is 2: the eleven go into the fenced list below, the five
    files are rewritten in step 6, and nothing else changes.
  * The fence, in `include/configforge/cf_port.h`, right after the
    `CF_API` block ("Export control"), exactly:

    ```c
    /* The internal tier (D-51, and this card's D-number): functions the
       library's own layers, ConfigForge Doctor and the tests call,
       exported from the library and outside the 1.x promise. They are
       declared only where CF_INTERNAL_API is 1 - the library's build,
       Doctor's and the tests' - so a program that calls one does not
       compile rather than break in a later minor release. */
    #ifndef CF_INTERNAL_API
    #define CF_INTERNAL_API 0
    #endif
    ```

    Defined to 0 there so that `#if CF_INTERNAL_API` passes `-Wundef`
    (D-32's set has it). Each fenced run of declarations in a header is
    wrapped in `#if CF_INTERNAL_API` and `#endif /* CF_INTERNAL_API */`,
    the comment above a declaration inside the block with it; types,
    macros and enumerators are never fenced, only functions, so every
    header still compiles alone and every struct a promised function
    takes is still there.
  * What is fenced - D-51's internal tier, function by function, with
    the answer above taken out. 84 functions:
    * `cf_flatten.h`, all ten: `cf_flatten_init`, `cf_flatten_sink`,
      `cf_flatten_lists`, `cf_decode_budget`, `cf_flatten_budget`,
      `cf_flatten_decode`, `cf_source_read_blob`,
      `cf_source_by_content`, `cf_doc_emit`, `cf_doc_emit_where`.
    * `cf_numeric.h`, all eleven: `cf_i64_to_text`, `cf_u64_to_text`,
      `cf_text_to_i64`, `cf_text_to_u64`, `cf_f64_to_text`,
      `cf_text_to_f64`, `cf_f64_bits`, `cf_f64_from_bits`,
      `cf_f64_from_f16_bits`, `cf_f64_from_f32_bits`,
      `cf_infer_scalar`.
    * `cf_platform.h`, all but `cf_platform_name` and the `_has_`
      queries: `cf_platform_environ`, `cf_platform_args`,
      `cf_platform_dir_list`, `cf_platform_dir_list_trusted`,
      `cf_platform_registry_walk`, `cf_platform_stamp`,
      `cf_platform_fopen`, `cf_platform_world_readable`,
      `cf_platform_create_private`, `cf_platform_wake_open`,
      `cf_platform_wake_fd`, `cf_platform_wake_handle`,
      `cf_platform_wake_drain`, `cf_platform_wake_wait`,
      `cf_platform_wake_close` (15).
    * `cf_keyspace.h`, every `cf_ks_` function but `cf_ks_arena` and
      the `cf_ks_add*` ones, which D-51 promises to a source's load:
      `cf_ks_init`, `cf_ks_rename`, `cf_ks_rename_list`, `cf_ks_replay`,
      `cf_ks_append_list`, `cf_ks_original_key`, `cf_ks_reject`,
      `cf_ks_resolve`, `cf_ks_candidate_count`, `cf_ks_infer_untyped`,
      `cf_ks_at_mut`, `cf_ks_reselect`, `cf_ks_find`,
      `cf_ks_lower_bound`, `cf_ks_get`, `cf_ks_count`, `cf_ks_at`,
      `cf_ks_is_sorted`, `cf_ks_digest`, `cf_ks_value_digest`,
      `cf_ks_digest_text` (21). `cf_key_valid`, `cf_key_segments`,
      `cf_key_cmp`, `cf_outcome_*` and `cf_refusal_*` stay promised.
    * `cf_explain.h`, the four `cf_ks_` ones: `cf_ks_explain`,
      `cf_ks_inspect`, `cf_ks_explain_redacted`,
      `cf_ks_inspect_redacted`.
    * `cf_define.h`, the definition set's: `cf_defset_init`,
      `cf_define`, `cf_def_bool`, `cf_def_int`, `cf_def_text`,
      `cf_def_real` (inside its `#if CF_WITH_REAL`, which stays,
      nested), `cf_defset_seal`, `cf_defset_open`, `cf_defset_schema`,
      `cf_defset_schema_file`, `cf_defset_is_open`, `cf_defset_find`,
      `cf_defset_list`, `cf_defset_fingerprint`, `cf_defset_count`,
      `cf_defset_at`, `cf_defset_nearest` (17). `cf_def_init_sized`
      (the `cf_def_init` macro's), `cf_fingerprint_seed`,
      `cf_fingerprint_step`, `cf_policy_text`, `cf_defs_apply` and the
      `cf_source_*_defs` stay promised: D-51 names none of them.
    * `cf_port.h`: `cf_ref_init`, `cf_ref_inc`, `cf_ref_dec`,
      `cf_ref_get`, `cf_cmp_bytes`, `cf_cmp_ascii_ci` (6).
    A header comment that says "INTERNAL" for a whole header
    (`cf_flatten.h`, `cf_numeric.h`, `cf_platform.h`, `cf_keyspace.h`,
    `cf_define.h`'s defset) is rewritten to say: "its functions are
    declared only under CF_INTERNAL_API (D-51, D-<this card's>)", and
    for `cf_platform.h` "but cf_platform_name and the cf_platform_has_
    queries, which are promised". Any function a 1.6.0 card added to
    these headers since this card was written follows the same rule
    (a `cf_ks_` one fenced, a `cf_platform_has_` one promised, and so
    on); step 1 finds them.
  * Who defines it, each as `CF_INTERNAL_API=1`:
    * the library: `target_compile_definitions(configforge PRIVATE CF_INTERNAL_API=1)`
      beside `CF_BUILDING_LIBRARY=1` in the top-level `CMakeLists.txt`;
    * Doctor, the tests, the benchmark:
      `add_definitions(-DCF_INTERNAL_API=1)` as the first command of
      `tools/doctor/CMakeLists.txt`, `tests/CMakeLists.txt` and
      `bench/CMakeLists.txt` (directory-wide and inherited by the
      directories below; `add_compile_definitions` needs CMake 3.12,
      the floor is 3.10). Not `tools/CMakeLists.txt`: T-1's front door
      and T-3's program must compile without it, and a target cannot
      take back a directory's `-D` without the compiler warning that
      the macro is defined twice. A test that must not see the tier
      (`fenced.c`, S-4's `test_serialize_alone.c`) begins
      `#undef CF_INTERNAL_API` and `#define CF_INTERNAL_API 0`. `tests/consumer` is not under it: it is a
      project of its own, configured by `gate.install` against the
      installed package, which is the point.
    * the amalgamation: `amalgamate.py`'s preamble of `configforge.c`
      gains, after the `_POSIX_C_SOURCE` block and before
      `#include <configforge/configforge.h>`:
      `"/* The library's own code sees its internal tier (D-51). */\n#define CF_INTERNAL_API 1\n\n"`.
      A program compiled beside it includes the same header and does
      not.
    * the Rust crate's `build.rs`: `.define("CF_INTERNAL_API", Some("1"))`
      on the `cc::Build`, after `.include(&include)`.
    * the proofs: `scripts/check/proofs.cmake`'s `cbmc` command line
      gains `-DCF_INTERNAL_API=1` after `-I "${SRC}/include"` (the
      harnesses in `tests/proofs/` call `cf_text_to_i64` and its kin).
    * not the examples: they are a program's code, and must compile
      without it. `examples/c/explain_demo.c` is the one that does not
      (it builds a key space by hand): it is rewritten onto the builder
      - `cf_build(NULL)`, `cf_builder_cli(b, "cli", CF_RANK_CLI, argc, argv)`,
      `cf_builder_env(b, "env", CF_RANK_ENV, "MYAPP_", cf_environ())`,
      `cf_builder_memory(b, "default", CF_RANK_DEFAULT, defaults, 4)`,
      `cf_builder_create(b, &err)` (NULL: print `err.message` to stderr,
      exit 1), then `cf_inspect(cfg, buf, sizeof buf)`, the digest line
      from `cf_digest(cfg)` as it prints it now, `cf_config_free(cfg)`.
      Its header comment, its three usage lines and its `static char buf[4096]`
      stay. `example.explain_demo` checks only the exit code.
  * `gate.abi`, three lists where it had one, each in
    `cmake/gates/GateAbi.cmake`:
    * `cmake/configforge.symbols`: the promised functions, as now.
    * `cmake/configforge.internal.symbols` (new): the fenced ones, the
      same format ("name", or "name real"), its first lines
      `# The internal tier of libconfigforge: exported, outside the 1.x promise (D-51).`
      and the same three lines the promised list's header has after
      its first. A line added or removed here is no version at all.
    * How a declaration is told apart: the gate reads, besides the
      `CF_API` lines it reads now, every `#if`, `#ifdef`, `#ifndef` and
      `#endif` line (`REGEX "^(#[ \t]*(if|ifdef|ifndef|endif)|(CF_NODISCARD[ \t]+)?CF_API[ \t])"`)
      and keeps a stack: `#if CF_WITH_REAL` pushes "real",
      `#if CF_INTERNAL_API` pushes "internal", any other `#if*` pushes
      "other", `#endif` pops. A declaration is "real" when "real" is on
      the stack and internal when "internal" is. This replaces the
      `real` flag, which an `#endif` nested inside would have reset.
      `#else` and `#elif` are not read: a fenced block holds neither.
    * The checks: the promised declarations equal
      `configforge.symbols`; the internal ones equal
      `configforge.internal.symbols`; a name in both is a violation
      (`'<name>' is in both lists`); every name of both lists is
      defined in the built library; a shared library exports nothing
      that is in neither. `WRITE` writes both files.
    * The promised list's header: its last line becomes
      `# A line added is a minor version, a line removed a major one; a name moved to configforge.internal.symbols is neither (D-51).`
      and the commit message says that the 84 lines leave this file and
      are in the other, still exported.
  * `gate.headers` (`cmake/gates/GateHeaders.cmake`) compiles every
    header a second time with `-DCF_INTERNAL_API=1`, for each standard
    it already compiles (the probe's name gains `_internal`), so a
    fenced declaration is compiled somewhere outside the library too.
  * The test that a program cannot call one:
    * `tests/unit/fenced.c` (new):

      ```c
      /* A program does not see the internal tier (D-51): this must not
         compile, and the compiler's message names cf_ks_count. */
      #undef CF_INTERNAL_API
      #define CF_INTERNAL_API 0
      #include "configforge/configforge.h"

      size_t (*fenced_count)(const cf_keyspace*) = cf_ks_count;
      ```

      An undeclared identifier, not a call, so it fails as an error in
      every compiler and every standard, implicit declarations or not.
      `tests/unit/CMakeLists.txt`, after the C11 must-not-compile cases:
      `cf_must_not_compile(unit.internal_fenced SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/fenced.c LANG C STD 90 DEFINE CF_FENCED=1 EXPECT "cf_ks_count")`
      (`include(ConfigForgeMustNotCompile)` first if the file does not
      include it yet).
    * The installed package, `cmake/gates/GateInstall.cmake`, after the
      plain `-I` compile of `options.c`: the same file compiled against
      `${prefix}/include` with
      `${CC} -std=c89 -c -o "${work}/fenced.o" "${SRC}/tests/unit/fenced.c" -I "${prefix}/include"`
      must fail and its output match `cf_ks_count`; else
      `message(FATAL_ERROR "a program sees the internal tier of the installed headers")`.
  * `docs/API.md`: `scripts/generate/apiref.cmake` follows the same
    stack as the gate (it reads every line already: an `#if CF_INTERNAL_API`
    line sets a flag kept until its `#endif`, counted through nested
    `#if`s), and a group declared inside the fence is rendered with one
    line after its code block:
    `Internal (\`CF_INTERNAL_API\`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.`
    The introduction's paragraph "Semantic versioning covers all of it
    (D-56) except the internal tier ..." becomes: "Semantic versioning
    covers every function not marked Internal (D-56). The internal tier
    (D-51, D-<this card's>) is declared only where `CF_INTERNAL_API` is
    1 - the library's own build, Doctor's and the tests' - so a program
    does not compile against it. The flags a definition takes are bits
    0-15 and a source's bits 16 and up; an unknown bit is refused by
    name." S-2 extends it.
  * Rejected: headers that are not installed (`include/configforge/internal/`).
    `cf_doctor.h` and Doctor's programs are installed against the
    package and include `configforge.h`; the internal functions are
    spread through headers whose promised parts a program needs; and
    the amalgamation is one header. Rejected: no longer exporting the
    internal tier - Doctor links a shared library in a shared build
    (D-51 says so). Rejected: renaming them `cfi_` - every one is an
    exported name, and renaming is removing.
* **files**: `include/configforge/cf_port.h`, `cf_flatten.h`,
  `cf_numeric.h`, `cf_platform.h`, `cf_keyspace.h`, `cf_explain.h`,
  `cf_define.h`, `CMakeLists.txt`, `tools/doctor/CMakeLists.txt`,
  `tests/CMakeLists.txt`, `bench/CMakeLists.txt`,
  `scripts/generate/amalgamate.py`, `bindings/rust/build.rs`,
  `scripts/check/proofs.cmake`, `examples/c/explain_demo.c`,
  `cmake/gates/GateAbi.cmake`, `cmake/gates/GateHeaders.cmake`,
  `cmake/gates/GateInstall.cmake`, `tests/unit/fenced.c` (new),
  `tests/unit/CMakeLists.txt`, `scripts/generate/apiref.cmake`; last:
  `cmake/configforge.symbols`, `cmake/configforge.internal.symbols`
  (new), `docs/API.md` (regenerated), `docs/Conventions.md`,
  `docs/Decisions.md`, `docs/Guide.md`, `CHANGELOG.md`,
  `docs/Roadmap.md`, and the files `version.cmake` changes.
* **steps**:
  1. The maintainer's answer is written in this card (loop step 4).
     `cmake -DTO=1.7.0 -P scripts/release/version.cmake` if 1.7.0 is
     not open yet. Then the list as it is today:
     `git grep -n -E "^(CF_NODISCARD )?CF_API .*(cf_(ks|flatten|defset|platform|ref|cmp)_|cf_(i64|u64|f64)_|cf_text_to_|cf_infer_scalar|cf_decode_budget|cf_doc_emit|cf_source_(read_blob|by_content)|cf_define\\(|cf_def_(bool|int|text|real))" include/configforge`;
     every match not in the decided list is a function added since -
     classify it by the rule and write it into this card's list
     before going on.
  2. `tests/unit/fenced.c` and its `cf_must_not_compile`. See it fail:
     `cmake --preset default && cmake --build --preset default && ctest --preset default -R unit.internal_fenced`
     fails (it compiles: nothing is fenced).
  3. The macro in `cf_port.h` and every `CF_INTERNAL_API=1` above
     (CMake, `amalgamate.py`, `build.rs`, `proofs.cmake`), nothing
     fenced yet. Build: nothing changes.
  4. The fences, header by header. After each header,
     `cmake --build --preset default`: a file that now calls an
     undeclared function names a place that lacks the definition -
     add it there (it is one of the places above), never in the
     header. Rerun step 2's test: passes.
  5. `GateAbi.cmake`, then
     `cmake -DSRC=. -DWRITE=ON -P cmake/gates/GateAbi.cmake` from a
     POSIX shell (Maintaining.md: `-D` from PowerShell lost its
     value); `git diff --stat cmake/` shows 84 lines (or step 1's
     count) leave `configforge.symbols` and the same number in the new
     file. `ctest --preset default -R gate\\.abi` passes.
  6. `explain_demo.c` onto the builder; `GateHeaders.cmake`;
     `GateInstall.cmake`. `ctest --preset default -R "gate\\.|example\\."`.
  7. `apiref.cmake`, then `cmake -P scripts/generate/apiref.cmake`.
  8. Docs. `cmake -P scripts/format.cmake`. ACCEPT-LINUX (the
     amalgamation, the `minimal` and `portable` presets), with
     `-DEXTRA=wasm` and `-DEXTRA=abi` where their tools are; with
     `CF_RUST=ON` the cargo test; ACCEPT-WINDOWS.
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI (cargo, clang,
  macOS, MSVC `/analyze`). `unit.internal_fenced`, `gate.abi`,
  `gate.headers`, `gate.install`, `gate.amalgamation` pass;
  `EXTRA=abi` reports no removed function (the binary is unchanged:
  fencing changes declarations, not exports).
* **stop if**: a binding or `configforge.hpp` turns out to call a
  fenced function (`git grep` of the list in `bindings/` and
  `include/configforge/configforge.hpp` found none on 2026-11-16 -
  name it under notes); a promised function's declaration needs a
  fenced one (an inline function or macro in a header calling it);
  `EXTRA=abi` reports a removed symbol; or a fenced block needs an
  `#else` to compile.
* **docs**: add this card's D-number, "What 1.x promises, made
  smaller: the internal tier fenced": the question (D-51's internal
  tier was a sentence; 345 exported functions, 84 of them not
  promised and callable); options (as it was; headers not installed;
  a macro a program does not define; hiding the symbols); the decision
  (the macro, where it is defined and why there, the list and its
  rule, the maintainer's answer on the eleven, the three lists of
  `gate.abi`, the two tests); compatibility (a program that called an
  internal function no longer compiles; it links and runs as before,
  since nothing exported changed; it may define `CF_INTERNAL_API` to
  1 and keep the 1.x-unstable calls, at its own risk - say so in those
  words). `docs/Conventions.md`, "Naming": a row
  `CF_INTERNAL_API   the fence: a declaration only the library's own build, Doctor and the tests see`.
  `docs/Guide.md`: where it first shows `cf_fmt` or the platform layer
  (`git grep -n "cf_fmt\|cf_platform_" docs/Guide.md`), nothing; one
  sentence at the end of its introduction: "Every function in
  `docs/API.md` not marked Internal is promised for 1.x." CHANGELOG,
  1.7.0, "### Changed": "**The internal tier is fenced** (D-<n>): the
  functions D-51 left outside the 1.x promise - the key space's, the
  flattener's, the definition set's, numeric conversion, most of the
  platform layer - are declared only where `CF_INTERNAL_API` is 1, so
  a program that calls one no longer compiles; nothing exported
  changed. `cf_fmt`, `cf_vfmt`, `cf_fmt_append`, `cf_platform_name`
  and the `cf_platform_has_` queries are promised." Roadmap, "From
  reading the project as a newcomer": "What the 1.x promise covers,
  made smaller" goes 🟨, "internals fenced (D-<n>); the experimental
  tier left (S-2)".
* **notes**:

---

## S-2 - An experimental tier, and the promise written in one place

* **id**: S-2
* **goal**: a new function may be declared `CF_EXPERIMENTAL(major, minor)`
  for the minor release it comes in - exported, listed apart by
  `gate.abi`, marked in `docs/API.md`, outside the promise - and the
  next minor release either promotes it to `CF_API` or removes it,
  which `gate.abi` enforces; `docs/API.md`'s introduction states the
  three tiers.
* **why**: a function is designed best after somebody has called it;
  frozen on its first day, a mistake in it is kept until 2.0. A tier
  that lasts one release and then must be decided is room to correct
  it without a promise broken.
* **needs**: S-1
* **size**: medium
* **decided**:
  * The macro, in `cf_port.h` after S-1's `CF_INTERNAL_API` block:

    ```c
    /* A function in its first minor release (this card's D-number):
       exported as CF_API is, and outside the 1.x promise until the next
       minor release, which makes it CF_API or removes it. `major` and
       `minor` are the release it came in; gate.abi fails once the
       version has moved past it. */
    #define CF_EXPERIMENTAL(major, minor) CF_API
    ```

    Declared as `CF_EXPERIMENTAL(1, 8) cf_bool cf_example(...);`, at the
    start of the line, never inside the internal fence. The bindings
    expose no experimental function (decided: a binding's own semver
    would have to say the same, and the bindings follow the C once it
    is settled - the order of 2026-11-12).
  * `gate.abi`: the declaration regex accepts
    `^(CF_NODISCARD[ \t]+)?CF_EXPERIMENTAL\(([0-9]+), *([0-9]+)\)[ \t]`
    beside `CF_API`; such a name goes to a third list,
    `cmake/configforge.experimental.symbols` (new, header
    `# Experimental: exported, outside the 1.x promise until promoted or removed.`
    and the rewrite line), one line `name major.minor`, plus " real"
    under `CF_WITH_REAL` as the others. Checks: the list equals the
    declarations; every name is defined in the library and allowed in a
    shared library's exports; and, reading the version with
    `configforge_read_version` (include `cmake/ConfigForgeVersion.cmake`),
    a name whose `major.minor` is below the current major and minor is
    a violation:
    `'<name>' has been experimental since <m>.<n>: make it CF_API or remove it (D-<this card's>)`.
    So the card that opens 1.9.0 meets every function 1.8.0 left
    experimental, and decides it.
  * `apiref.cmake`: such a declaration starts a group like a `CF_API`
    one (its regex gains the same alternative), and the group gets,
    after its code block, `Experimental since <m>.<n>: outside the 1.x promise until the next minor release makes it promised or removes it.`
  * The promise, written once: `apiref.cmake`'s introduction, replacing
    S-1's paragraph, under a heading `## What 1.x promises`:
    "Three tiers. **Promised**: every function not marked otherwise
    below; semantic versioning holds it (D-56) - within 1.x a program
    built against an earlier release links and runs against a later
    one. **Experimental**: marked so, with the release it came in;
    exported, and outside the promise until the next minor release,
    which makes it promised or removes it. **Internal**: marked so;
    declared only where `CF_INTERNAL_API` is 1, the library's own
    build, Doctor's and the tests'; free to change in any release. The
    flags a definition takes are bits 0-15 and a source's bits 16 and
    up; an unknown bit is refused by name." S-4 adds the serialization
    library's paragraph. `docs/Maintaining.md`, "Versions", "What moves
    which number", gains a bullet: "**Experimental** (`CF_EXPERIMENTAL`):
    a function may come in outside the promise for its first minor; the
    card that opens the next minor makes it `CF_API` or removes it -
    `gate.abi` fails until one is done."
  * No test in the tree for the gate itself: a gate is proved by
    breaking it once (Architecture.md, "The gates"); step 2 does that
    by hand and the commit message says what it printed.
  * Rejected: an experimental tier in a header of its own (a function
    would move between headers when promoted, a source change for every
    caller); one without a version in it (nothing could say when it was
    due).
* **files**: `include/configforge/cf_port.h`,
  `cmake/gates/GateAbi.cmake`, `scripts/generate/apiref.cmake`; last:
  `cmake/configforge.experimental.symbols` (new), `docs/API.md`
  (regenerated), `docs/Maintaining.md`, `docs/Decisions.md`,
  `CHANGELOG.md`, `docs/Roadmap.md`.
* **steps**:
  1. The macro; the gate's regex and list; the empty list file written
     by `cmake -DSRC=. -DWRITE=ON -P cmake/gates/GateAbi.cmake` (POSIX
     shell). `ctest --preset default -R gate\\.abi` passes.
  2. See it fail, by hand and undone after: add
     `CF_EXPERIMENTAL(1, 6) cf_bool cf_experimental_probe(void);` to
     `cf_port.h` and a definition returning `CF_TRUE` to `src/core/port.c`;
     `WRITE`, build, `ctest -R gate\\.abi` fails with
     "has been experimental since 1.6"; change it to `(1, 7)`: passes,
     and `docs/API.md` regenerated into a scratch file shows the
     "Experimental since 1.7" line. `git checkout -- include src cmake`.
  3. `apiref.cmake`'s heading and paragraph;
     `cmake -P scripts/generate/apiref.cmake`.
  4. Docs. ACCEPT-LINUX, ACCEPT-WINDOWS.
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI; `gate.abi`,
  `gate.apiref`.
* **stop if**: `CF_EXPERIMENTAL(1, 7) ` at the start of a declaration
  is moved by clang-format 22 so that the gate's regex no longer finds
  it (then the macro's form is wrong: hand back with the formatted
  line); or `-pedantic` warns on a function-like macro there.
* **docs**: S-1's D-number gains "The experimental tier": the macro,
  one minor release, the gate that makes the next minor decide, the
  bindings left out and why; and "the promise is stated in
  `docs/API.md`'s introduction". CHANGELOG, 1.7.0, "### Added": "An
  experimental tier, `CF_EXPERIMENTAL(major, minor)`: a function
  outside the 1.x promise for the minor release it comes in, promoted
  or removed by the next (D-<n>); `docs/API.md` begins with what 1.x
  promises." Roadmap: "What the 1.x promise covers, made smaller" goes
  🟩, "internals fenced and an experimental tier (D-<n>)".
* **notes**:

---

## S-3 - The serialization layer's last ties cut, and held by the gate

* **id**: S-3
* **goal**: the serialization layer and the leaf code it uses - eight
  named files - call nothing outside themselves and include only the
  serialization library's headers, and `gate.layering` checks it as a
  list of what is allowed rather than of what is not.
* **why**: D-27 drew the boundary so that the layer could leave as "a
  directory move and a build file"; read on 2026-11-16 it could not:
  `blob.c` opened files through the whole platform layer, TLV checked
  its keys through the key space, and `error.c` defined a function of
  the log. A library cut along a boundary that leaks is a library that
  does not link.
* **needs**: S-1
* **size**: medium
* **decided**:
  * What the serialization library is, read from the tree on
    2026-11-16 (`src/serialize/*.c` and what they call):
    * its sources: every `.c` in `src/serialize/` - `blob.c`,
      `codec.c`, `detect.c`, `ini.c`, `json.c`, `toml.c`,
      `properties.c`, `cbor.c`, `xml.c`, `yaml.c`, `yamlparse.c`, and
      after this card `tlv.c`; and the leaves they call:
      `src/core/alloc.c` (the arena), `src/core/port.c` (`cf_fmt`,
      `cf_vfmt`, `cf_cmp_ascii_ci`, which call `numeric.c`),
      `src/core/error.c` (`cf_error_set`), `src/core/numeric.c`,
      `src/core/value.c`, and the platform's one file opener, after
      this card `src/platform/posix/file.c`, `src/platform/win32/file.c`
      and `src/platform/none/file.c`.
    * its headers: `cf_build_options.h` (generated), `cf_port.h`,
      `cf_error.h` with `cf_code.def`, `cf_numeric.h`, `cf_value.h`
      with `cf_type.def`, `cf_alloc.h`, `cf_platform_file.h` (new),
      `cf_serialize.h`; `cf_yamlparse.h` is used by `yaml.c` and
      `yamlparse.c` and declares nothing exported. S-4 adds
      `cf_serialize_version.h`.
  * The three ties, cut:
    1. **The file opener.** `cf_platform_fopen`'s declaration moves
       from `cf_platform.h` to a new `include/configforge/cf_platform_file.h`
       (its comment with it; include guard `CONFIGFORGE_CF_PLATFORM_FILE_H`;
       it includes `configforge/cf_port.h` only; the S-1 fence around
       the declaration goes with it), and `cf_platform.h` includes the
       new header where the declaration was, so every file that saw it
       still does. Its three definitions move out of `posix.c`,
       `win32.c` and `none.c`, unchanged, into
       `src/platform/posix/file.c`, `src/platform/win32/file.c`,
       `src/platform/none/file.c`, each guarded whole by its platform's
       macro (`#if CF_PLATFORM_POSIX` and so on, after
       `#include "configforge/cf_platform.h"`, as its sibling is).
       `win32/file.c` needs `win32.c`'s `static cf_bool win32_wide`: it
       gets its own copy, `static cf_bool win32f_wide(...)`, the same
       body, its comment saying it is `win32_wide`'s and why it is a
       copy (the serialization library carries this file without
       `win32.c`; a shared helper would be an exported name of
       `win32.c`'s). `posix/file.c` includes `<stdio.h>`; `none/file.c`
       keeps the `CF_NO_STDIO` branch. `src/platform/CMakeLists.txt`
       adds `file.c` beside each platform's file. `blob.c` includes
       `configforge/cf_platform_file.h` in place of
       `configforge/cf_platform.h`.
    2. **TLV.** `src/bridge/tlv.c` moves to `src/serialize/tlv.c`
       (`git mv`): its functions are declared in `cf_serialize.h`
       already. Its two calls of `cf_key_valid` (the layout's keys,
       lines 132 and 232 on 2026-11-16) become calls of a new
       `static cf_bool tlv_key_valid(const char* key, size_t len)`,
       `cf_key_valid`'s body as `src/core/keyspace.c` has it, its
       comment: "The key space's rule (cf_key_valid), which this layer
       may not reach: a canonical key is frozen for 1.x, and
       test_tlv holds the two to each other." Its include of
       `configforge/cf_keyspace.h` goes; its header comment's last
       paragraph ("In the bridge rather than with the other readers
       ...") becomes "With the other readers since the serialization
       library was made a target of its own (D-<S-4's>): the layout's
       keys are checked by the key space's rule, copied." Its CMake
       line moves from `src/bridge/CMakeLists.txt` to
       `src/serialize/CMakeLists.txt`'s first `target_sources`
       (`tlv.c` guards itself by `CF_WITH_TLV`; verify with
       `git grep -n "CF_WITH_TLV" src/bridge/tlv.c` before moving -
       stop if it does not).
    3. **The log.** `cf_log_level_name` moves, unchanged, from
       `src/core/error.c` to the end of `src/core/config.c`, and
       `error.c` loses `#include "configforge/cf_log.h"`.
  * The test that the copied key rule is the key space's:
    `tests/unit/test_tlv.c` gains `test_layout_key_rule`: for each of
    `"a"`, `"a.b"`, `"a-b_c.9"`, `"A"`, `"a..b"`, `".a"`, `"a."`,
    `"a b"`, and a key of `CF_KEY_MAX - 1` and of `CF_KEY_MAX` bytes
    of `'k'`, a one-tag layout naming it is accepted by `cf_tlv_codec`
    exactly when `cf_key_valid` accepts it; registered in its `main`.
  * `gate.layering`, rule 2 rewritten as a list of what is allowed, in
    `cmake/gates/GateLayering.cmake`, the lists in a new
    `cmake/ConfigForgeSerialize.cmake` that the gate `include()`s by
    path (`${SRC}/../cmake/ConfigForgeSerialize.cmake`: the gate's
    `SRC` is the `src` directory) and S-4's CMake includes too:

    ```cmake
    # The serialization library: what it is made of and what it may
    # include. One list, read by gate.layering and by the build.
    set(CF_SERIALIZE_LEAVES core/alloc.c core/port.c core/error.c core/numeric.c
        core/value.c platform/posix/file.c platform/win32/file.c platform/none/file.c)
    set(CF_SERIALIZE_HEADERS cf_build_options.h cf_port.h cf_error.h cf_numeric.h
        cf_value.h cf_alloc.h cf_platform_file.h cf_serialize.h cf_yamlparse.h)
    ```

    (S-4 appends `cf_serialize_version.h` and `serialize/version.c`).
    The rule: every file of `src/serialize/` and every leaf may include,
    of `configforge/`, only a header of `CF_SERIALIZE_HEADERS` and the
    two `.def` files; so may each of those headers. A violation:
    `VIOLATION: <file> (the serialization library) includes configforge/<h>`.
    Rule 1 and rule 3 stay as they are; rule 2's two old checks are
    covered by the new one and go.
  * The amalgamation: `amalgamate.py`'s `SOURCES` gains the three
    `file.c` after their siblings and has `serialize/tlv.c` in place of
    `bridge/tlv.c`; `HEADERS` gains `cf_platform_file.h` before
    `cf_platform.h`. The Rust crate compiles every `.c` under `src/`
    (`build.rs`), so it needs nothing; the files guard themselves.
  * wasm32-wasi: `posix/file.c` is `fopen`, which wasi-libc has.
  * Rejected: the whole platform layer in the serialization library
    (environment, directories, the Registry, the change handle and
    libkqueue, for one `fopen`); `cf_key_valid` moved into the
    serialization layer (the key rule is the configuration model's, and
    the core would then call into the layer above it); TLV left in the
    bridge with its declarations moved out of `cf_serialize.h` (a
    program that includes `cf_serialize.h` alone would stop compiling).
* **files**: `include/configforge/cf_platform_file.h` (new),
  `include/configforge/cf_platform.h`, `src/platform/posix/posix.c`,
  `src/platform/posix/file.c` (new), `src/platform/win32/win32.c`,
  `src/platform/win32/file.c` (new), `src/platform/none/none.c`,
  `src/platform/none/file.c` (new), `src/platform/CMakeLists.txt`,
  `src/serialize/blob.c`, `src/serialize/tlv.c` (moved),
  `src/bridge/CMakeLists.txt`, `src/serialize/CMakeLists.txt`,
  `src/core/error.c`, `src/core/config.c`, `tests/unit/test_tlv.c`,
  `cmake/ConfigForgeSerialize.cmake` (new),
  `cmake/gates/GateLayering.cmake`, `scripts/generate/amalgamate.py`;
  last: `docs/API.md` (regenerated), `docs/Architecture.md`,
  `CHANGELOG.md`, `docs/Roadmap.md`.
* **steps**:
  1. `cmake/ConfigForgeSerialize.cmake` and the new rule 2. See it
     fail: `ctest --preset default -R gate\\.layering` names `blob.c`
     (`cf_platform.h`) and `bridge/tlv.c` is not yet in `serialize/`;
     `error.c` (`cf_log.h`) is named too.
  2. `test_layout_key_rule`; it passes already (the rule is the same
     function today) - the test is for after the copy.
  3. The opener's header and files; the build; `ctest --preset default`.
  4. TLV moved with its copy of the rule; `ctest --preset default -R "unit\\.test_tlv|example\\.tlv"`.
  5. The log function moved. `gate.layering` passes.
  6. `amalgamate.py`; `ctest --preset default -R gate\\.amalgamation`.
  7. `cmake -P scripts/generate/apiref.cmake` (a declaration moved
     header). Docs. ACCEPT-LINUX (with `-DEXTRA=wasm`), ACCEPT-WINDOWS
     (the win32 file), ACCEPT-CI.
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI; `gate.layering`,
  `gate.amalgamation` (two statics with one name would fail it here),
  `gate.detection` (the new files are under `src/platform/`),
  `gate.abi` (nothing added or removed).
* **stop if**: anything in `src/serialize/` or a leaf calls a function
  defined outside the serialization library's sources other than the
  three above (on 2026-11-16 there was none: read with a script over
  the definitions, the calls of `src/serialize/*.c` reached only
  `alloc.c`, `port.c`, `error.c`, `numeric.c`, `value.c` and
  `cf_platform_fopen`, and `tlv.c`'s only `cf_key_valid` besides those
  - re-check with the build of S-4's target, which would not link);
  `tlv.c` does not guard itself by `CF_WITH_TLV`; or `cf_log_level_name`
  is not declared where `config.c` is compiled.
* **docs**: no D-number of its own: S-4's records the cut. Architecture.md,
  "Layers": the `src/serialize` row's "May use" becomes "portability,
  `cf_value`, `cf_error`, `cf_alloc`, `cf_numeric`, the platform's file
  opener", and the paragraph after the table gains: "`gate.layering`
  holds the serialization library to a list of what it may include
  (`cmake/ConfigForgeSerialize.cmake`)." CHANGELOG, 1.7.0, "### Changed":
  "`cf_platform_fopen` is declared in `cf_platform_file.h`, which
  `cf_platform.h` includes; the TLV codec is with the other readers.
  Nothing a program sees changes." Roadmap: nothing until S-4.
* **notes**:

---

## S-4 - The serialization library, a target of its own

* **id**: S-4
* **goal**: `configforge_serialize` is built beside `configforge` from
  the serialization library's sources, with its own symbol list held by
  `gate.abi` and its own version macros, and a program built against it
  alone in the tree reads and writes a document; `libconfigforge` still
  carries every one of its functions.
* **why**: the readers are most of the code (about 9,900 lines of
  29,000) and have value without a configuration: a C89 YAML 1.2 reader
  that passes yaml-test-suite, JSON5, TOML, CBOR, TLV. Made a library of
  their own before the first new format, every format after it joins
  that library's promise and not ConfigForge's (Roadmap, "More
  formats").
* **needs**: S-2, S-3
* **size**: large
* **decided**:
  * **TO DECIDE - the serialization library's name.** It is public from
    its first release and cannot change in 1.x: the CMake target and
    its exported name, the library's file, the pkg-config name, the
    package component. Its functions stay `cf_` and its headers
    `configforge/`, whatever it is called (D-18: the prefix is fixed;
    renaming an exported function is removing it). Options:
    1. **`configforge_serialize`**: target `configforge_serialize`,
       exported `ConfigForge::configforge_serialize` (as
       `ConfigForge::configforge_doctor` is), `libconfigforge_serialize.a`/`.so`/`.dylib`,
       `configforge_serialize.dll`/`.lib`, `configforge_serialize.pc`,
       component `serialize` of the package `ConfigForge`. One family
       with everything inside it; the layer's directory and header say
       "serialize" already.
    2. **A name of its own now**, for the day it has a repository of
       its own (Roadmap, step 2): the package would carry it while every
       symbol and header inside says configforge - two names for one
       thing until 2.0 at least.
    3. **`configforge_codec`**, matching the tool if T-3's answer is
       `configforge-codec`: shorter, and "codec" is what the library
       calls each format, not the library.
    Recommended: **1**. The rest of this card, S-5 and T-3 are written
    for 1. Another answer is a name replaced in those three cards before
    any is started; nothing else changes.
  * **Compiled twice, not shared as objects.** The serialization
    library's sources are added to both targets, `configforge` as now
    and `configforge_serialize`. Rejected: an `OBJECT` library feeding
    both (under CMake 3.10 it cannot take `target_link_libraries`, so
    every helper's options, definitions and include directories would
    be copied to it by hand, and `yaml.c`'s per-source C11 flag is set
    in the top directory); `configforge` linking `configforge_serialize`
    statically (a shared `libconfigforge` would then hold only the
    archive members something in it calls, and an uncalled exported
    function such as `cf_codec_kind` would be lost - a 1.x break).
    The cost is the readers compiled twice, a few seconds.
  * The target, in the top-level `CMakeLists.txt` right after
    `configforge`'s block (before `add_subdirectory(src)`), with
    `include(ConfigForgeSerialize)` (S-3's file, on `CMAKE_MODULE_PATH`)
    first:

    ```cmake
    # The serialization library (D-27, D-<this card's>): the readers and
    # writers and what they need, a library of its own. libconfigforge
    # compiles the same sources, so it carries every function of it.
    add_library(configforge_serialize)
    add_library(ConfigForge::configforge_serialize ALIAS configforge_serialize)
    target_include_directories(
      configforge_serialize PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                                   $<INSTALL_INTERFACE:include>)
    configforge_apply_warnings(configforge_serialize)
    configforge_apply_hardening(configforge_serialize)
    configforge_generate_options(configforge_serialize)
    configforge_apply_options(configforge_serialize)
    configforge_apply_analysis(configforge_serialize)
    target_compile_definitions(configforge_serialize PRIVATE CF_BUILDING_LIBRARY=1
                                                     CF_INTERNAL_API=1)
    ```

    and the same `set_target_properties` as `configforge` (VERSION,
    SOVERSION, hidden visibility, C 90). Its sources:
    `src/serialize/CMakeLists.txt` adds each of its files to both
    targets - every `target_sources(configforge PRIVATE ...)` there
    becomes a `foreach(t configforge configforge_serialize)` over the
    same line, and likewise its `target_include_directories` and
    `target_compile_definitions` for the YAML and XML parsers; the
    leaves are added to `configforge_serialize` by
    `src/CMakeLists.txt`, after the subdirectories, from
    `CF_SERIALIZE_LEAVES`, each `${CMAKE_CURRENT_SOURCE_DIR}/<leaf>`,
    the three `file.c` chosen by platform as `src/platform/CMakeLists.txt`
    chooses (`none` when `CF_PLATFORM STREQUAL "none"`, `win32` on
    `WIN32`, else `posix`). The top-level links of the YAML, XML
    libraries (`if(CF_WITH_YAML) target_link_libraries(configforge PRIVATE ...)`)
    and `yaml.c`'s `-std=c11` property are repeated for
    `configforge_serialize`; SQLite is not (the store is the bridge's).
    `CF_PLATFORM_NONE` is a PUBLIC definition of `configforge` under
    `none`: give it to `configforge_serialize` too.
  * The stack budget: `configforge_apply_warnings`
    (`cmake/ConfigForgeCompiler.cmake`) measures `configforge` only
    (`if(tgt STREQUAL "configforge")`): it becomes
    `if(tgt STREQUAL "configforge" OR tgt STREQUAL "configforge_serialize")`,
    and `configforge_stack_budget` writes `-DOUT=${CMAKE_BINARY_DIR}/${tgt}-stack.txt`
    (for `configforge` the name it has now). The frames are the same
    code's, so the `minimal` preset passes for both or neither.
  * The version macros, `include/configforge/cf_serialize_version.h`
    (new), including `configforge/cf_port.h` only:

    ```c
    /* The serialization library's version: ConfigForge's, for as long as
       the two are in one repository (D-<this card's>). A copy of
       cf_version.h's numbers, which scripts/release/version.cmake
       rewrites with them and gate.version holds to them. */
    #define CF_SERIALIZE_VERSION_MAJOR 1
    #define CF_SERIALIZE_VERSION_MINOR 7
    #define CF_SERIALIZE_VERSION_PATCH 0
    #define CF_SERIALIZE_VERSION_NUMBER (CF_SERIALIZE_VERSION_MAJOR * 10000 + \
        CF_SERIALIZE_VERSION_MINOR * 100 + CF_SERIALIZE_VERSION_PATCH)

    /* The version the serialization library was compiled as: "1.7.0",
       and the same as one number. (1.7) */
    CF_API const char* cf_serialize_version(void);
    CF_API int         cf_serialize_version_number(void);
    ```

    (written as clang-format lays it out; the `STRINGIFY` of
    `cf_version.h` is not repeated: the function returns a literal built
    from the three numbers by `src/serialize/version.c`'s own two-level
    stringify macros, named `SV_STR_` and `SV_STR`). `src/serialize/version.c`
    (new) defines the two; it is in `src/serialize/`, so both libraries
    compile it, and `CF_SERIALIZE_LEAVES`' sibling list - S-3's
    `CF_SERIALIZE_HEADERS` - gains `cf_serialize_version.h`.
    `cf_serialize.h` includes `cf_serialize_version.h`. Rejected: its
    own version number from 1.0.0 (a second release process, D-27's own
    objection; its functions are under ConfigForge's 1.x promise already
    and cannot be under a younger one); including `cf_version.h` (it
    declares `cf_version`, `cf_build_id` and the feature list, which
    this library does not define: a declaration that cannot link is a
    lie in its headers).
    `scripts/release/version.cmake` rewrites the three
    `CF_SERIALIZE_VERSION_*` lines as it does `cf_version.h`'s;
    `cmake/gates/GateVersion.cmake` reads them and adds
    `include/configforge/cf_serialize_version.h says <x.y.z>` to its
    failures when they differ from the project version.
  * `gate.abi` for the second library, in `GateAbi.cmake` with a new
    input `-DPART=serialize` (default: the whole library, as now) and
    `-DLIB=` its file:
    * `cmake/configforge_serialize.symbols` (new): the promised
      functions declared in `CF_SERIALIZE_HEADERS` (read from
      `cmake/ConfigForgeSerialize.cmake`), header lines as the other
      lists, first line
      `# The public interface of libconfigforge_serialize: every CF_API function of its headers.`
      Its internal tier is not listed again: the names of
      `configforge.internal.symbols` that its headers declare are
      allowed in its exports.
    * Checks: the list equals the promised declarations of its headers;
      every listed name is also in `configforge.symbols` (libconfigforge
      carries all of it: `'<name>' is in the serialization library and
      not in libconfigforge`); every listed name, and every internal one
      its headers declare, is defined in its library; a shared one
      exports nothing else. Experimental names of its headers likewise,
      from `configforge.experimental.symbols`.
    * `cmake/ConfigForgeGates.cmake` registers
      `gate.abi.serialize` beside `gate.abi`, the same command with
      `-DPART=serialize -DLIB=$<TARGET_LINKER_FILE:configforge_serialize>`.
    * `cf_serialize_version` and `cf_serialize_version_number` are new
      in both lists (a minor's addition).
  * The test in the tree: `tests/unit/test_serialize_alone.c` (new),
    built as its own target linked to `configforge_serialize` and
    nothing else, including only `configforge/cf_serialize.h`, after
    `#undef CF_INTERNAL_API` and `#define CF_INTERNAL_API 0` (the
    directory defines it to 1; S-1). It: reads
    `{"server": {"port": 8080}, "tags": ["a", "b"]}` through
    `cf_codec_by_name("json")`'s decode into a sink of its own that
    counts the events (2 tables, 1 array, 3 scalars, 3 ends) and keeps
    the `port` scalar's int value (8080); checks
    `cf_serialize_version_number() == CF_SERIALIZE_VERSION_NUMBER` and
    that `cf_serialize_version()` is `"1.7.0"` built from the macros;
    and, where `cf_codec_by_name("json")->encoder` is not NULL, encodes
    the same events into a `cf_sink` and finds `"port"` in it. Test
    name `unit.serialize_alone`, under `if(CF_WITH_JSON)`. A program
    that links: that is the test; an unresolved symbol fails its build,
    naming the function that the library lacks.
  * `docs/API.md`: `apiref.cmake`'s "What 1.x promises" (S-2) gains a
    paragraph: "**The serialization library**, `configforge_serialize`,
    is made of the headers `cf_port.h`, `cf_error.h`, `cf_numeric.h`,
    `cf_value.h`, `cf_alloc.h`, `cf_platform_file.h`, `cf_serialize.h`
    and `cf_serialize_version.h`, and holds their functions to the same
    three tiers; `libconfigforge` carries every one of them, so a
    program links one library or the other, never both." The headers'
    sections are not moved.
  * The amalgamation stays one file, the whole of ConfigForge, as now,
    with `serialize/version.c` added to `SOURCES` and
    `cf_serialize_version.h` to `HEADERS` before `cf_serialize.h`.
    Rejected for now: a second amalgamation of the serialization
    library alone - a second distribution form to gate, for a library
    nobody has asked for alone yet; it comes with the repository of its
    own (Roadmap, step 2).
  * Not changed: `gate.symbols` and `gate.interface` read
    `configforge` (the same code); the SBOM describes the package, which
    holds both; `scripts/verify.cmake`'s `EXTRA=abi` globs
    `libconfigforge.so*`, which `libconfigforge_serialize.so` does not
    match - read it to be sure.
* **files**: `CMakeLists.txt`, `src/CMakeLists.txt`,
  `src/serialize/CMakeLists.txt`, `cmake/ConfigForgeSerialize.cmake`,
  `cmake/ConfigForgeCompiler.cmake`, `cmake/ConfigForgeGates.cmake`,
  `cmake/gates/GateAbi.cmake`, `cmake/gates/GateVersion.cmake`,
  `include/configforge/cf_serialize_version.h` (new),
  `include/configforge/cf_serialize.h`, `src/serialize/version.c`
  (new), `scripts/release/version.cmake`,
  `scripts/generate/amalgamate.py`, `scripts/generate/apiref.cmake`,
  `tests/unit/test_serialize_alone.c` (new), `tests/unit/CMakeLists.txt`;
  last: `cmake/configforge.symbols`, `cmake/configforge_serialize.symbols`
  (new), `docs/API.md` (regenerated), `docs/Architecture.md`,
  `docs/Decisions.md`, `CHANGELOG.md`, `docs/Roadmap.md`.
* **steps**:
  1. The maintainer's answer is written in this card (loop step 4).
  2. `test_serialize_alone.c` and its target linked to a
     `configforge_serialize` that has no sources yet. See it fail:
     `cmake --preset default && cmake --build --preset default`
     fails to configure or to link the test.
  3. The target and its sources. The build links the test; if it does
     not, the linker names what the serialization library lacks: S-3
     missed a tie - stop.
  4. The version header, `version.c`, `version.cmake`, `GateVersion.cmake`.
     `ctest --preset default -R "unit\\.serialize_alone|gate\\.version"`.
  5. `gate.abi.serialize`; write its list (`-DPART=serialize -DWRITE=ON`,
     POSIX shell) and `configforge.symbols` again for the two new names.
     `ctest --preset default -R gate\\.abi`.
  6. The stack budget for the second target; the amalgamation;
     `apiref.cmake` and `cmake -P scripts/generate/apiref.cmake`.
  7. Docs. ACCEPT-LINUX (the `minimal` preset measures both libraries;
     `-DEXTRA=shared`, where `gate.abi.serialize` reads a shared one;
     `-DEXTRA=wasm`), ACCEPT-WINDOWS (two DLLs exporting the same
     functions, each through its own import library).
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI;
  `unit.serialize_alone`, `gate.abi`, `gate.abi.serialize`,
  `gate.version`, `gate.layering`, `gate.amalgamation`.
* **stop if**: the test does not link against `configforge_serialize`
  alone (a tie S-3 did not cut - name the symbol); MSVC refuses two
  libraries in one build exporting one name (it should not: each DLL
  has its own export table); or the second target's stack table
  differs from the first's for the same function (the flags differ:
  find which helper was missed).
* **docs**: add this card's D-number, "The serialization library, a
  target of its own": the question (D-27 kept it internal "until
  needed"; the Roadmap needs it before the first new format); the
  options (as it was; a target in this repository; a repository now);
  the decision (a target; its sources and headers and how
  `gate.layering` holds them; compiled twice and why; libconfigforge
  carries all of it, so a program links one or the other; the version
  is ConfigForge's, as a checked copy; the maintainer's answer on the
  name; S-3's three cuts and TLV's copy of the key rule; one
  amalgamation); compatibility (nothing a program of 1.6 sees changes;
  two functions added). Architecture.md, "Layers": a paragraph after
  the table: "`src/serialize` and the leaves it uses are also a library
  of their own, `configforge_serialize` (D-<n>); every function of it
  is in `libconfigforge` too." CHANGELOG, 1.7.0, "### Added": "**The
  serialization library, `configforge_serialize`**: the readers and
  writers - JSON, JSON5, TOML, YAML, INI, `.properties`, XML, CBOR, TLV
  - as a library of their own, which a program that wants documents and
  not a configuration links alone; `libconfigforge` still carries all
  of it. `cf_serialize_version()` (D-<n>)." Roadmap, "More formats":
  step 1 goes 🟨, "the target and its symbol list done (D-<n>); its
  install and packages left (S-5)".
* **notes**:

---

## S-5 - The serialization library installed, found and packaged

* **id**: S-5
* **goal**: an installed ConfigForge holds `configforge_serialize` as
  the component `serialize` of its CMake package and as its own
  pkg-config file, the Conan recipe describes both libraries as
  components, and `gate.install` builds one program against the
  serialization library alone and one against ConfigForge alone, from
  the installed package and from pkg-config.
* **why**: a library that only the source tree can link is not one a
  program can take. The Roadmap's "done when" is these two consumers.
* **needs**: S-4
* **size**: medium
* **decided**:
  * Install, `cmake/ConfigForgeInstall.cmake`: `configforge_serialize`
    joins `install(TARGETS configforge EXPORT ConfigForgeTargets ...)`
    (the same three destinations); the headers are one directory and
    installed once, as now. The Python wheel (`SKBUILD`) installs only
    `libconfigforge` and does not change.
  * The component, `cmake/templates/ConfigForgeConfig.cmake.in`, before
    `check_required_components(ConfigForge)`:

    ```cmake
    # Components: serialize (the serialization library alone) and doctor
    # (Doctor's rules), each found when its target was installed.
    foreach(_cf_comp serialize doctor)
      if(TARGET ConfigForge::configforge_${_cf_comp})
        set(ConfigForge_${_cf_comp}_FOUND TRUE)
      else()
        set(ConfigForge_${_cf_comp}_FOUND FALSE)
      endif()
    endforeach()
    ```

    `find_package(ConfigForge REQUIRED COMPONENTS serialize)` then
    fails cleanly on an install without it.
  * pkg-config: `cmake/templates/configforge_serialize.pc.in` (new),
    the shape of `configforge.pc.in` with `Name: ConfigForge serialization`,
    `Description: The readers and writers of ConfigForge as a library of their own: JSON, JSON5, TOML, YAML, INI, CBOR, TLV`,
    `Libs: -L${libdir} -lconfigforge_serialize`, and the same
    `Libs.private` and `Requires.private` as `configforge.pc` less
    `-lsqlite3` (the store is not in it). `ConfigForgeInstall.cmake`
    builds that second private list beside the first and configures and
    installs the file beside `configforge.pc`.
  * The consumers, `tests/consumer/` (configured by `gate.install`
    against the installed prefix):
    * `tests/consumer/serialize.c` (new): includes only
      `configforge/cf_serialize.h`; decodes
      `{"a": {"b": 1}}` through `cf_codec_by_name("json")` (when it is
      NULL - a build without JSON - prints `serialize: no json in this build`
      and returns 0) into a sink that counts scalars, and prints
      `serialize ok: <version> <n> value(s)` with
      `cf_serialize_version()`; returns 0 when n is 1.
    * `tests/consumer/CMakeLists.txt`: `find_package(ConfigForge REQUIRED COMPONENTS serialize)`,
      `add_executable(consumer_serialize serialize.c)` linked to
      `ConfigForge::configforge_serialize` only, C 90.
    * `GateInstall.cmake`: after `consumer` runs, `consumer_serialize`
      runs and its output must match `^serialize ok: `, else
      `message(FATAL_ERROR "a program cannot use the serialization library alone")`;
      and in the pkg-config part, the same `serialize.c` built from
      `pkg-config --cflags --libs --static configforge_serialize` and
      run, as `consumer_pc` is. The existing `consumer` is the
      ConfigForge-alone one and is unchanged.
  * Conan, `conanfile.py`'s `package_info`: components in place of the
    one `libs` -

    ```python
    self.cpp_info.set_property("cmake_file_name", "ConfigForge")
    self.cpp_info.set_property("pkg_config_name", "configforge")
    lib = self.cpp_info.components["configforge"]
    lib.libs = ["configforge"]
    lib.set_property("cmake_target_name", "ConfigForge::configforge")
    lib.set_property("pkg_config_name", "configforge")
    ser = self.cpp_info.components["serialize"]
    ser.libs = ["configforge_serialize"]
    ser.set_property("cmake_target_name", "ConfigForge::configforge_serialize")
    ser.set_property("pkg_config_name", "configforge_serialize")
    ```

    and `requires` of expat/sqlite3, when the options bring them, on
    the components that link them (`expat::expat` on both,
    `sqlite3::sqlite3` on `configforge` only). The test package is
    unchanged (it links `ConfigForge::configforge`).
  * vcpkg: the port installs what CMake installs, the component
    included; `portfile.cmake` is unchanged. Its `usage` file, if the
    port has none, is not added here.
  * Rust and Python: unchanged. The crate compiles every source under
    `src/` into one static library, as before (`build.rs`); the wheel
    carries `libconfigforge`, which holds every serialization function.
* **files**: `cmake/ConfigForgeInstall.cmake`,
  `cmake/templates/ConfigForgeConfig.cmake.in`,
  `cmake/templates/configforge_serialize.pc.in` (new),
  `tests/consumer/serialize.c` (new), `tests/consumer/CMakeLists.txt`,
  `cmake/gates/GateInstall.cmake`, `conanfile.py`; last:
  `docs/Guide.md`, `README.md`, `docs/Decisions.md`, `CHANGELOG.md`,
  `docs/Roadmap.md`.
* **steps**:
  1. `serialize.c`, its target and the gate's run. See it fail:
     `ctest --preset default -R gate\\.install` fails at
     `find_package(ConfigForge REQUIRED COMPONENTS serialize)`.
  2. The install of the target and the component. Rerun: the CMake
     consumer passes and the pkg-config one fails (no `.pc`).
  3. The `.pc` file. Rerun: passes.
  4. `conanfile.py`; where Conan is,
     `conan create . --build=missing -tf packaging/conan/test_package`.
  5. Docs. ACCEPT-LINUX (with `-DEXTRA=shared`: the consumers against a
     shared install), ACCEPT-WINDOWS (no `gate.install` there: build
     and install by hand once, `cmake --install build/default --prefix <tmp>`,
     and see `configforge_serialize.dll` and `.lib` installed).
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI (the Conan and
  vcpkg jobs where CI has them); `gate.install` with both consumers.
* **stop if**: a static install links `consumer_serialize` only with
  `libconfigforge.a` too (the export set made one depend on the other
  - it must not); or pkg-config's `--static` list for the serialization
  library needs SQLite.
* **docs**: S-4's D-number gains "Installed as the `serialize`
  component and `configforge_serialize.pc`; Conan's components". Guide,
  where it shows `find_package(ConfigForge ...)`
  (`git grep -n "find_package(ConfigForge" docs/Guide.md README.md`):
  one paragraph after it, "A program that reads and writes documents
  and has no configuration links the serialization library alone:
  `find_package(ConfigForge REQUIRED COMPONENTS serialize)` and
  `ConfigForge::configforge_serialize`, or `pkg-config configforge_serialize`.
  Never both libraries: `ConfigForge::configforge` holds every function
  of the other." README: the same in one sentence where it lists the
  libraries. CHANGELOG, 1.7.0, "### Added", S-4's entry gains: "Installed
  as the CMake component `serialize` and `configforge_serialize.pc`."
  Roadmap, "More formats": step 1 goes 🟩, its "Done when" met.
* **notes**:

---

## T-1 - `configforge <command>`, and Doctor's own name

* **id**: T-1
* **goal**: a program `configforge` runs `configforge-<command>` with the
  rest of its arguments - found beside itself first, then on the PATH -
  and returns its exit code; Doctor's program is `configforge-doctor`,
  with a copy named `doctor` for 1.7.x that says its new name; the tools
  are installed by `cmake --install`.
* **why**: three tools are coming (Doctor, the contract's, the
  documents'), each beside the library it serves; one name to remember,
  as git has it, keeps them one family, and keeps Doctor to the one job
  that makes a value explain itself across environments.
* **needs**: S-1, H-6, H-8b (both change `tools/doctor/doctor.c` and the
  action)
* **size**: large
* **decided**:
  * The program: `tools/configforge/main.c` (the command line),
    `tools/configforge/front.h` (one declaration),
    `tools/configforge/run_posix.c` and `tools/configforge/run_win32.c`
    (finding and running the program), `tools/configforge/CMakeLists.txt`.
    C89. Target `configforge_front`, `OUTPUT_NAME configforge` (the
    library is the target `configforge`). It links no library: it
    includes `configforge/cf_platform.h` for `CF_PLATFORM_POSIX` and
    `CF_PLATFORM_WIN32` - the platform decided once, where the
    library decides it - and `configforge/cf_version.h` for
    `CF_VERSION_STRING`, through
    `target_include_directories(configforge_front PRIVATE $<TARGET_PROPERTY:configforge,INTERFACE_INCLUDE_DIRECTORIES>)`
    and `target_compile_definitions(configforge_front PRIVATE $<TARGET_PROPERTY:configforge,INTERFACE_COMPILE_DEFINITIONS>)`,
    as `cmake/ConfigForgeMustNotCompile.cmake` does. So it needs no DLL
    beside it on Windows. On Windows it links `shell32`
    (`CommandLineToArgvW`). CMake builds one of the two run files by
    platform, as `src/platform/CMakeLists.txt` chooses; each is also
    guarded whole by its macro.
  * Not built where there are no processes: `tools/configforge/CMakeLists.txt`
    begins
    `if(CF_PLATFORM STREQUAL "none" OR CMAKE_SYSTEM_NAME STREQUAL "WASI")`,
    `message(STATUS "configforge: a front door runs other programs, which this platform cannot; not built")`,
    `return()`. wasm32-wasi has no `exec` and no process to wait for;
    its toolchain file turns `CF_BUILD_TOOLS` off already, and this
    keeps it so if somebody turns them on.
  * `front.h`:

    ```c
    /* Runs `program` (configforge-<command>) with argv[2..argc-1] after
       its name, found beside this program first and then on the PATH,
       and returns its exit code; or prints why on stderr and returns 2
       when it is not found or cannot be started. On POSIX the program
       replaces this one and this returns only on failure. */
    int front_run(const char* program, int argc, char** argv);
    ```

  * The command line, `main.c`, in this order:
    * no argument: the help on stderr, exit 2;
    * `help`, `--help` or `-h` alone: the help on stdout, exit 0;
    * `--version` alone: `ConfigForge <CF_VERSION_STRING>` and a newline
      on stdout, exit 0;
    * otherwise `argv[1]` is the command. A name is 1 to 64 bytes of
      `a-z`, `0-9` and `-`, not starting with `-`; any other is
      `configforge: '<name>' is not a command name` on stderr, exit 2 -
      so `../x` or `/bin/sh` is never run. The program is
      `configforge-<name>`, on Windows `configforge-<name>.exe`, built in
      a buffer of `8 + 1 + 64 + 4 + 1` bytes; then `front_run`.
    * The help, exactly (in two `fputs` calls, each literal under 509
      bytes):

      ```text
      ConfigForge

        configforge doctor <command> ...   reports: explain, dump, lint, diff,
                                           history, blame, import-*
        configforge gen ...                the contract: check files, write code
        configforge codec <command> ...    documents: formats, detect, check,
                                           dump, convert, format
        configforge help
        configforge --version

      `configforge <name> ...` runs configforge-<name>, found beside this
      program or on the PATH, and exits with its exit code.
      ```

      T-2 and T-3 bring `gen` and `codec`; their lines are there from
      this card, as the front door's whole design, and `configforge gen`
      before T-2 is "not a configforge command" like any other missing.
  * Not found: `configforge: '<name>' is not a configforge command (no configforge-<name> beside this program or on the PATH); configforge help lists them`
    on stderr, exit 2. Found and not started:
    `configforge: cannot run <path>` on stderr, exit 2. Out of memory:
    `configforge: out of memory`, exit 2. Rejected: 127 and 126, the
    shells' codes - Doctor's and the other tools' 2 is "unusable input",
    and a CI step that runs `configforge doctor lint` needs one meaning
    for "it did not run". No error text from `strerror` (it is
    locale-sensitive, and `gate.symbols` keeps the library away from
    such functions; the tools follow it).
  * POSIX, `run_posix.c`: `#define _POSIX_C_SOURCE 200112L` as its first
    line, before any header (glibc hides `execv` and `access` in strict
    C89, and CI's newest gcc makes the implicit declaration an error);
    then `#include "configforge/cf_platform.h"`, `#if CF_PLATFORM_POSIX`,
    `<errno.h>`, `<stdio.h>`, `<stdlib.h>`, `<string.h>`, `<unistd.h>`.
    `front_run` builds a new `argv` with `malloc` - `program`, then
    `argv[2]` to `argv[argc - 1]`, then NULL. When `argv[0]` holds a
    `/`, the program's directory is its text up to the last `/`; the
    path `<dir>/<program>` (malloc'd) is tried with `access(path, X_OK)`,
    and when it may be executed, `execv(path, args)`. Otherwise, or
    when that `execv` returns, `execvp(program, args)` - the PATH; it
    returns only on failure: `errno == ENOENT` is "not a configforge
    command", any other "cannot run <program>". When `argv[0]` holds no
    `/` the shell found this program on the PATH, and the same search
    finds its siblings installed beside it.
  * Windows, `run_win32.c`: `#include "configforge/cf_platform.h"`,
    `#if CF_PLATFORM_WIN32`, `<windows.h>`, `<shellapi.h>`, `<stdio.h>`,
    `<stdlib.h>`, `<wchar.h>`. Everything is UTF-16, so an argument
    outside the code page reaches the program whole:
    1. This program's path: `GetModuleFileNameW(NULL, buf, n)` into a
       `malloc`'d buffer, doubled from 260 `wchar_t` until the result
       is shorter than the buffer, at most 32768; its directory is up to
       the last `\`.
    2. The program, `<dir>\<program>` with `program` converted by
       `MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, ...)`: used
       when `GetFileAttributesW` says it exists and is not a directory.
       Else each entry of `PATH` (`GetEnvironmentVariableW`, `malloc`'d),
       split at `;`, an empty entry skipped, a pair of `"` around an
       entry removed, `<entry>\<program>` tried the same way. Not the
       current directory: an entry has to name it.
    3. The arguments: `CommandLineToArgvW(GetCommandLineW(), &n)`; its
       first two (this program and the command) are skipped. The command
       line is the program's path quoted, then each argument quoted by
       the rule `CommandLineToArgvW` reads back: an argument that is not
       empty and has no space, tab, newline, vertical tab or `"` is
       written as it is; any other is written inside `"`, where a run of
       n backslashes followed by `"` is written as 2n+1 backslashes and
       the `"`, a run at the end as 2n backslashes, and any other run as
       it is. In a `malloc`'d buffer, sized by a first pass over the
       same rule.
    4. `SetConsoleCtrlHandler(NULL, TRUE)` - Ctrl+C is the program's to
       answer, not the front door's - then `CreateProcessW(path, cmdline, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)`
       with a zeroed `STARTUPINFOW` whose `cb` is set; failure is
       "cannot run <program>". `WaitForSingleObject(pi.hProcess, INFINITE)`,
       `GetExitCodeProcess`, both handles closed, `LocalFree` of the
       argument array, and the exit code returned as an `int`.
    Each failure returns early rather than testing inside an expression
    (MSVC's `/analyze`, Maintaining.md).
  * Every tool in one directory: `tools/CMakeLists.txt` sets
    `set(CF_TOOLS_DIR ${CMAKE_BINARY_DIR}/tools/bin)` and every tool
    executable gets `RUNTIME_OUTPUT_DIRECTORY ${CF_TOOLS_DIR}` (a
    multi-config generator adds its `Debug/` or `Release/` below it), so
    the front door finds Doctor beside itself in the build tree as in an
    install. In a Windows shared build, where the tests' programs stay in
    `bin/` beside the DLL, a tool that links the library gets it copied
    beside it:
    `if(WIN32 AND BUILD_SHARED_LIBS) add_custom_command(TARGET doctor POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:configforge> $<TARGET_FILE_DIR:doctor> VERBATIM)`.
    The front door links nothing and needs none; its name differs from
    the DLL's directory, so `configforge.exe`'s `.pdb` and `.ilk` and
    `configforge.dll`'s never share a directory.
  * Doctor's program: the target stays `doctor` (every test names
    `$<TARGET_FILE:doctor>`), with `OUTPUT_NAME configforge-doctor`. A
    copy named `doctor` (with the executable suffix) is made beside it
    by `POST_BUILD`:
    `${CMAKE_COMMAND} -E copy $<TARGET_FILE:doctor> $<TARGET_FILE_DIR:doctor>/doctor${CMAKE_EXECUTABLE_SUFFIX}`.
    `doctor.c`'s `main`, first: when the base name of `argv[0]` (after
    the last `/` or `\`) is `doctor` or `doctor.exe`, compared ASCII
    case-insensitively by a static `old_name` of its own, it prints
    `doctor: this program is configforge-doctor now (configforge doctor); the name doctor goes in 1.8.0`
    and a newline on stderr, then goes on as it would. Its usage lines
    say `configforge doctor` where they say `doctor`; its messages keep
    their `doctor: ` prefix; `--version` keeps `ConfigForge Doctor <v>`.
  * Install: `tools/CMakeLists.txt` does `include(GNUInstallDirs)` (the
    install module includes it later, after `tools/`), and at its end:

    ```cmake
    # The programs, for `cmake --install`: the front door and every tool
    # it runs, in one directory (D-<this card's>).
    foreach(t configforge_front doctor)
      if(TARGET ${t})
        install(TARGETS ${t} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT tools)
      endif()
    endforeach()
    if(TARGET doctor)
      install(PROGRAMS $<TARGET_FILE_DIR:doctor>/doctor${CMAKE_EXECUTABLE_SUFFIX}
              DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT tools)
    endif()
    ```

    T-2 and T-3 add theirs to the list. In a shared build the tools find
    the installed library through `INSTALL_RPATH` `$ORIGIN/../${CMAKE_INSTALL_LIBDIR}`
    (`@loader_path/../${CMAKE_INSTALL_LIBDIR}` on Apple), set on each
    tool when `BUILD_SHARED_LIBS AND NOT WIN32`; on Windows the DLL is
    installed to `bin/` already (`RUNTIME DESTINATION`). vcpkg, Conan and
    the wheel build with `CF_BUILD_TOOLS=OFF` and install none.
  * The release, `.github/workflows/release.yml`: Linux and macOS
    `install -s -m 755` each of `build/tools/bin/configforge`,
    `configforge-doctor` and `doctor` into `stage/configforge-$v/bin/`
    in place of the one `doctor` line (the `cmake --install` before
    them has put them there unstripped); Windows copies
    `build-doctor/tools/bin/Release/*.exe` into the stage's `bin/` in
    place of `doctor.exe` alone (the /MT build: the /MD tools that
    `cmake --install build` put there are overwritten). The header
    comment's three archive lines say "the tools" for "a static Doctor".
  * The action and the hook: `actions/doctor/action.yml`'s find and
    `actions/doctor/doctor-bin.sh`'s two finds look for
    `-name configforge-doctor -o -name configforge-doctor.exe`; the build
    target is still `doctor`. The Python wrapper,
    `bindings/python/src/configforge_doctor.py`'s `find_doctor`:
    `doctor or os.environ.get("CONFIGFORGE_DOCTOR") or shutil.which("configforge-doctor") or shutil.which("doctor")`,
    its message
    `no doctor program: set CONFIGFORGE_DOCTOR or put configforge-doctor on PATH`,
    and its docstring's "`doctor` on PATH" as "`configforge-doctor`
    (or an older `doctor`) on PATH".
  * The tests, `tests/front/` (new), added by `tests/CMakeLists.txt`
    under `if(CF_BUILD_TOOLS AND TARGET configforge_front)`:
    `tests/front/CMakeLists.txt` registers `front.cmake` as
    `front.run` with `-DFRONT=$<TARGET_FILE:configforge_front>`,
    `-DHAVE_DOCTOR=ON` when `if(TARGET doctor)` (CMake 3.10 has no
    `$<TARGET_EXISTS>`), else `OFF`,
    `-DUNIX_HOST=${CMAKE_HOST_UNIX}`, `-DWORK=${CMAKE_CURRENT_BINARY_DIR}/work`,
    `WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/tests/doctor` and, with
    Doctor, `FIXTURES_REQUIRED DOCTOR_REPORTS`. A helper
    `front_case(<expected-exit> <stdout-regex or -> <stderr-regex or -> <args>...)`
    runs the front door and fails with both outputs on a mismatch.
    Cases:
    1. `--version`: 0, stdout `^ConfigForge [0-9]+\.[0-9]+\.[0-9]+\n$`.
    2. `help`: 0, stdout `configforge doctor`. No argument: 2, stderr
       `configforge doctor`.
    3. `nosuch`: 2, stderr `'nosuch' is not a configforge command`.
    4. `../doctor` and `Doctor`: 2, stderr `is not a command name`.
    5. With Doctor: `doctor --version`: 0, stdout `^ConfigForge Doctor`.
    6. `doctor lint cf_fx_leak.json`: 1 (the exit code passes through).
    7. `doctor explain cf_fx_prod.json "no such key"`: 2, stderr
       `'no such key' is not in` (an argument with spaces arrives whole).
    8. `doctor explain cf_fx_prod.json` and the argument `a"b\` (in
       CMake `"a\"b\\"`): 2, stderr contains `'a"b\' is not in` (a
       quote and a trailing backslash arrive as written).
    9. On a Unix host: `${WORK}/path/configforge-hello`, written as
       `#!/bin/sh\necho "hello $*"\nexit 3\n` and made executable by
       `file(COPY ... FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)`;
       `configforge hello a b` with `PATH=${WORK}/path:$ENV{PATH}`
       (`cmake -E env`): 3, stdout `hello a b` - found on the PATH, not
       beside the front door.
    And `tests/doctor/CMakeLists.txt` gains `doctor.old_name`: the copy
    `$<TARGET_FILE_DIR:doctor>/doctor${CMAKE_EXECUTABLE_SUFFIX}` run
    with `--version`, its stderr to match `this program is configforge-doctor now`
    (through `run.cmake` with a new optional `-DEXPECT_ERR=<regex>`:
    when given, the stderr must match it, else `FATAL_ERROR` - so the
    exit code is still checked, which `PASS_REGULAR_EXPRESSION` would
    not do).
  * clang-tidy: `tools/configforge/CMakeLists.txt` gives
    `configforge_front` the same `C_CLANG_TIDY` as Doctor's (less
    `cert-err33-c`), copied from `tools/doctor/CMakeLists.txt`.
  * Rejected: running the program through the library's platform layer
    (a new `cf_platform_exec`): every program that reads a configuration
    would carry process spawning for the sake of one tool. Rejected:
    `_wspawnv` on Windows (it joins the arguments with spaces and quotes
    nothing). Rejected: a table of known commands (an extension a user
    writes, `configforge-<name>` on the PATH, works as git's does).
    Rejected: dropping the name `doctor` at once (every script and CI
    job that calls it would break on an upgrade of a minor version).
* **files**: `tools/configforge/main.c`, `front.h`, `run_posix.c`,
  `run_win32.c`, `CMakeLists.txt` (all new), `tools/CMakeLists.txt`,
  `tools/doctor/CMakeLists.txt`, `tools/doctor/doctor.c`,
  `tests/front/CMakeLists.txt` (new), `tests/front/front.cmake` (new),
  `tests/CMakeLists.txt`, `tests/doctor/CMakeLists.txt`,
  `tests/doctor/run.cmake`, `actions/doctor/action.yml`,
  `actions/doctor/doctor-bin.sh`, `bindings/python/src/configforge_doctor.py`,
  `.github/workflows/release.yml`; last: `docs/Tools.md` (new),
  `mkdocs.yml`, `docs/Doctor.md`, `README.md`, `docs/Guide.md`,
  `docs/Decisions.md`, `CHANGELOG.md`, `docs/Roadmap.md`.
* **steps**:
  1. `tests/front/` and its registration, with a `configforge_front`
     whose `main` returns 2. See it fail:
     `cmake --preset default && cmake --build --preset default && ctest --preset default -R front\\.`
     fails on case 1.
  2. `main.c` without running anything: cases 1 to 4 pass.
  3. `CF_TOOLS_DIR`, Doctor's name and its copy, `old_name`;
     `run_posix.c`. Cases 5 to 9 pass; `ctest --preset default -R "doctor\\."`
     all pass.
  4. `run_win32.c`; ACCEPT-WINDOWS: cases 5 to 8 there (9 is Unix
     only), and `cmake --build --preset default` with the analyzer job's
     flags if the machine has them (CI's MSVC `/analyze` otherwise).
  5. Install rules, the action, the hook, the Python wrapper, the
     release workflow. `ctest --preset default -R "doctor\\.(action|python)|gate\\.install"`.
  6. Docs. ACCEPT-LINUX (with `-DEXTRA=shared`: the installed tools
     find `libconfigforge.so` by their run path; `-DEXTRA=wasm`: no
     front door is built).
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI - and the release
  workflow's dry run, which a change to `tools/doctor/**` and
  `CMakeLists.txt` starts on the push to main: read it green before the
  next card.
* **stop if**: `CommandLineToArgvW` gives fewer than two arguments for
  the front door's own command line (then step 3 of the Windows design
  cannot skip them: hand back with the command line); a shared Windows
  build cannot run Doctor from `tools/bin` after the copy (a second DLL
  - name it); or `install(PROGRAMS $<TARGET_FILE_DIR:doctor>/...)` is
  refused by CMake 3.10's generator expressions in `install` (then
  install the copy by a `install(CODE ...)` file copy - hand back if
  that does not work either).
* **docs**: add this card's D-number, "Three programs behind one front
  door": the question (Doctor gathered three kinds of command); the
  options (Doctor keeps all; three programs alone; three behind one
  door); the decision (git's rule; found beside itself then on the
  PATH, never the current directory; the exit codes; the name rule;
  not built where there are no processes; Doctor's program renamed and
  its old name kept one minor release; every tool in one directory;
  what was rejected); compatibility (the program `doctor` is
  `configforge-doctor`; `doctor` still runs in 1.7.x and says so on
  stderr; the archives ship both). `docs/Tools.md` (new; nav entry
  `- Tools: docs/Tools.md` after Doctor in `mkdocs.yml`): "# The
  tools", the front door's section - the rule, the help, the exit
  codes, where the programs are found, Windows' and WASI's answers - and
  a line for each tool: Doctor (`docs/Doctor.md`), and the two T-2 and
  T-3 add. Doctor.md: every command line in section 7, 11 and 12 is
  `configforge doctor ...` (as section 7 writes most of them already);
  `parity` and `emit` in section 7 are said for what they are -
  `parity` is part of `diff` (CFD012) and `emit` is not a command (the
  library's `cf_report` writes a report) - since neither is in
  `doctor.c`; section 7 opens with "The program is `configforge-doctor`;
  `configforge doctor` runs it (docs/Tools.md). Until 1.8.0 it is also
  installed as `doctor`." README: the release paragraph's `bin/doctor`
  and `doctor.exe` and every `doctor ...` example line become the new
  names. Guide, section 11: likewise. CHANGELOG, 1.7.0: "### Added":
  "**`configforge <command>`**: one front door for the tools, as git
  has it - `configforge doctor lint` runs `configforge-doctor` (D-<n>).
  `cmake --install` installs the tools." "### Changed": "Doctor's
  program is `configforge-doctor`; the name `doctor` still runs it in
  1.7.x, says so on stderr, and goes in 1.8.0 (D-<n>)." Roadmap,
  "Tools, distribution and digests": "Doctor does one job" goes 🟨,
  "the front door and Doctor's name done (D-<n>); `configforge gen` and
  the codec tool left (T-2, T-3)".
* **notes**:

---

## T-2 - `configforge gen`: the contract's own program

* **id**: T-2
* **goal**: `configforge-gen --schema S [--name N] [--out F]` writes the
  contract as C calls and `configforge-gen check --schema S <file>...`
  checks files against it, exactly as `doctor gen` and `doctor check`
  do today; Doctor keeps both for 1.7.x as aliases that say where they
  went.
* **why**: the contract is what lets a value be refused with its key
  and its reason; checking files against it and writing it as code is
  the contract's work, not a report's, and wave 5 adds three more
  commands of that kind.
* **needs**: T-1, H-2 (it changes `tools/doctor/contract.c` and adds
  the generated contract's compile tests that move here)
* **size**: medium
* **decided**:
  * The code moves, `git mv tools/doctor/contract.c tools/gen/contract.c`.
    New: `tools/gen/gen.c` (the command line), `tools/gen/gen.h`,
    `tools/gen/CMakeLists.txt`; `tools/CMakeLists.txt` adds
    `add_subdirectory(gen)` inside its `if(CF_WITH_JSON AND CF_WITH_REPORT)`
    beside Doctor (the contract is read by the JSON reader and written
    through `cf_schema_write`). `tools/gen/CMakeLists.txt` begins
    `add_definitions(-DCF_INTERNAL_API=1)`, as `tools/doctor/`'s does
    (S-1): the contract code uses the definition set's functions.
    Target `configforge_gen`, `OUTPUT_NAME
    configforge-gen`, linked to `configforge`, C 90, the helpers
    Doctor's executable gets (`configforge_apply_warnings`,
    `_hardening`, `_analysis`), `RUNTIME_OUTPUT_DIRECTORY ${CF_TOOLS_DIR}`,
    the DLL copy and the run path of T-1, clang-tidy as Doctor's, and
    `configforge_gen` added to T-1's install list.
  * `gen.h` declares, in place of `adapters.h`'s two:

    ```c
    /* Files read against a JSON Schema contract, as a program built from
       it would read them (contract.c). argv is what follows `check`. */
    int gen_check(int argc, char** argv);

    /* The contract as C calls (contract.c). argv is what follows the
       program's name: --schema, --name, --out. */
    int gen_code(int argc, char** argv);
    ```

    `doctor_check_schema` becomes `gen_check` and `doctor_gen_schema`
    `gen_code`, bodies unchanged; `contract.c` includes `gen.h` for
    `adapters.h`. Its messages' prefix `doctor: ` becomes `gen: `; its
    `contract_usage()` lines become T-2's usage lines below; its header
    comment's two command lines likewise. The generated file's comment,
    "written by `doctor gen --schema`", becomes "written by
    `configforge gen --schema`" (a generated file says where it came
    from; the old name would send a reader to an alias).
  * `gen.c`'s `main`: `argv[1]` is `check` - `gen_check(argc - 2, argv + 2)`;
    `--version` alone - `ConfigForge gen <cf_version()>` and the build
    id as Doctor prints them; any other argument starting with `--` -
    `gen_code(argc - 1, argv + 1)`; nothing or anything else - the
    usage on stderr, exit 2. The usage, exactly:

    ```text
    ConfigForge gen: the contract, and code from it

      configforge gen --schema <schema> [--name <function>] [--out <file>]
                                     the contract as C calls
      configforge gen check --schema <schema> <file>...
                                     files against the contract
      configforge gen --version

    Exit codes: 0 done, and for check every file holds to the contract;
    1 a file does not; 2 unusable input.
    ```

  * Doctor's aliases, `tools/doctor/doctor.c`: Doctor's executable
    compiles `../gen/contract.c` too (its `add_executable` list) and
    includes `../gen/gen.h`; `adapters.h` loses the two declarations. In
    `main`, `check` and `gen` run
    `doctor_moved("check", "configforge gen check")` /
    `doctor_moved("gen", "configforge gen")` and then `gen_check` /
    `gen_code` with the arguments as before. `static void doctor_moved(const char* was, const char* now)`
    prints on stderr exactly
    `doctor: '<was>' is '<now>' since 1.7.0; this name goes in 1.8.0`
    and a newline. The exit code is the command's. Doctor's usage loses
    the two lines; its header comment lists them under "moved".
  * The pre-commit hook: `actions/doctor/doctor-bin.sh` takes one
    optional argument, the program, `configforge-doctor` (the default)
    or `configforge-gen`; the variable that names one ready-made is
    `CONFIGFORGE_DOCTOR` for the first and `CONFIGFORGE_GEN` for the
    second; the build target is `doctor` or `configforge_gen`; the
    find looks for that name and its `.exe`. Its header comment says
    so. `actions/doctor/check.sh` calls `doctor-bin.sh configforge-gen`,
    runs `"$gen" check --schema ...`, and says `configforge gen check`
    in its comment and `gen:` in its three messages.
    `.pre-commit-hooks.yaml` is unchanged (its ids are the hooks'
    public names).
  * The tests, `tests/gen/` (new), added in `tests/CMakeLists.txt`
    inside the `if(CF_BUILD_TOOLS AND CF_WITH_JSON AND CF_WITH_REPORT)`
    block after `add_subdirectory(doctor)`; every test there runs in
    `${CMAKE_BINARY_DIR}/tests/doctor`, where `doctor_fixtures` writes
    the contract fixtures, with `FIXTURES_REQUIRED DOCTOR_REPORTS`:
    * `gen_case(<name> <exit> <args>)`, `doctor_case`'s twin, running
      `tests/doctor/run.cmake` with `-DDOCTOR=$<TARGET_FILE:configforge_gen>`
      (run.cmake's `check` assertions read `argv[0]`, which is `check`
      here as it was for Doctor). The six `check_*` cases of
      `tests/doctor/CMakeLists.txt` move here as `gen.check_ok`,
      `gen.check_typo`, `gen.check_range`, `gen.check_layers`,
      `gen.check_no_schema`, `gen.check_alone`, same arguments, the
      schema `${CMAKE_SOURCE_DIR}/tests/doctor/contract.schema.json`
      (it stays where it is: the action's test reads it too).
    * The generated contract: `tests/doctor/test_gen.c` moves to
      `tests/gen/test_gen.c` (`git mv`; its `#include` of `../unit/cf_test.h`
      is at the same depth); the `add_custom_command` that runs
      `doctor gen` runs `configforge_gen --schema ... --name gen_contract --out ...`
      with `DEPENDS configforge_gen`; the target `doctor_gen` is `gen_c`
      and its test `doctor.gen` is `gen.c`. H-2's objects and refusals
      move with it and are renamed so: `doctor_gen_c11` `gen_c11`,
      `doctor_gen_cxx11` `gen_cxx11`, `doctor.refused.gen.c11`
      `gen.refused.c11`, `doctor.refused.gen.cxx11` `gen.refused.cxx11`,
      `tests/doctor/contract_refused.schema.json` to
      `tests/gen/contract_refused.schema.json` - the names H-2's card
      gives them; if H-2's notes renamed any, the notes' names.
    * The aliases, in `tests/doctor/CMakeLists.txt`:
      `doctor.check_alias` - `doctor check --schema <schema> cf_fx_contract_ok.json`
      with `-DEXPECT_ERR='check' is 'configforge gen check' since 1.7.0`,
      exit 0 - and `doctor.gen_alias` - `doctor gen --schema <schema> --out ${CMAKE_CURRENT_BINARY_DIR}/alias.c`,
      exit 0, `-DEXPECT_ERR='gen' is 'configforge gen' since 1.7.0`;
      both through T-1's `EXPECT_ERR` of `run.cmake`, by a
      `doctor_case_err(<name> <exit> <regex> <args>)`.
    * `gen.version`: `configforge_gen --version` matching
      `^ConfigForge gen ${PROJECT_VERSION}( \\([0-9a-f]+(-dirty)?\\))?\n$`.
    * `gen_case(usage 2 --nonsense)`: an option it does not know is the
      usage, exit 2 (`contract_usage` returns 2 today; read it).
    * The hook's test, `tests/doctor/action.cmake`: its `check.sh` runs
      set `CONFIGFORGE_GEN=${GEN}` in place of `CONFIGFORGE_DOCTOR`, and
      `doctor.action` is registered with `-DGEN=$<TARGET_FILE:configforge_gen>`.
    * The front door: `tests/front/front.cmake` gains, with gen built,
      `gen --version`: 0, stdout `^ConfigForge gen`.
  * wasm32-wasi and `none`: as Doctor - a program that reads files; it
    is not built where tools are off.
  * Rejected: `configforge gen` with `gen` as a command of its own
    (`configforge gen gen --schema`); the program's name says what its
    first job is. Rejected: Doctor running `configforge-gen` for its
    aliases (a second program to find for a release's worth of
    convenience; the code is linked twice for one minor release).
* **files**: `tools/gen/contract.c` (moved), `tools/gen/gen.c`,
  `tools/gen/gen.h`, `tools/gen/CMakeLists.txt` (new),
  `tools/CMakeLists.txt`, `tools/doctor/CMakeLists.txt`,
  `tools/doctor/doctor.c`, `tools/doctor/adapters.h`,
  `actions/doctor/doctor-bin.sh`, `actions/doctor/check.sh`,
  `tests/gen/CMakeLists.txt` (new), `tests/gen/test_gen.c` (moved),
  `tests/gen/contract_refused.schema.json` (moved, if H-2 made it),
  `tests/CMakeLists.txt`, `tests/doctor/CMakeLists.txt`,
  `tests/doctor/action.cmake`, `tests/front/front.cmake`; last:
  `docs/Tools.md`, `docs/Doctor.md`, `README.md`, `docs/Guide.md`,
  `docs/Decisions.md`, `CHANGELOG.md`, `docs/Roadmap.md`.
* **steps**:
  1. `tests/gen/` with every case above, and `tests/doctor/`'s alias
     cases. See them fail: `ctest --preset default -R "gen\\.|doctor\\.(check|gen)_alias"`
     fails (no `configforge_gen`; the build of the test directory
     fails first - that is the failure).
  2. The move and `gen.c`. `ctest --preset default -R "gen\\."` passes.
  3. Doctor's aliases. `ctest --preset default -R "doctor\\."` passes.
  4. The hook. `ctest --preset default -R doctor\\.action`.
  5. Docs. ACCEPT-LINUX, ACCEPT-WINDOWS (the C++ refusal under MSVC
     moved with H-2's tests).
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI; every `gen.*`,
  `doctor.*` and `front.*` test.
* **stop if**: H-2 is not done (`git grep -n "CF_GEN_KEY" tools/doctor/contract.c`
  finds nothing - then it is not, and this card waits); a test of
  `tests/doctor` other than the six moved cases runs `check` or `gen`
  (`git grep -n "check|\|gen|" tests/doctor/CMakeLists.txt` - name it
  and move it too); or `run.cmake`'s `check` assertions do not hold for
  the new program's output (its `ok: ` line must be the same).
* **docs**: T-1's D-number gains "`configforge gen`: check and the C
  writer moved; the aliases". `docs/Tools.md`: a section "configforge
  gen" with Doctor.md's section 7d moved into it whole and rewritten
  with the new command lines (`configforge gen check --schema
  app.schema.json app.toml`, `configforge gen --schema app.schema.json
  --name app_contract --out app_contract.c`), and H-2's paragraph on
  the checks the file makes. Doctor.md: 7d leaves (a line in its place:
  "Moved to docs/Tools.md, `configforge gen`."), section 7's `check`
  and `gen` lines go, and the aliases are said once in section 7.
  README and Guide: every `doctor check` and `doctor gen` becomes
  `configforge gen check` and `configforge gen`. CHANGELOG, 1.7.0,
  "### Added": "**`configforge gen`**: the contract's program -
  `configforge gen --schema` writes it as C, `configforge gen check`
  holds files to it." "### Changed": "`doctor check` and `doctor gen`
  are `configforge gen check` and `configforge gen`; Doctor still runs
  them in 1.7.x, says so on stderr, and drops them in 1.8.0." Roadmap:
  "Doctor does one job" keeps 🟨, "front door, Doctor's name and
  `configforge gen` done; the codec tool left (T-3)".
* **notes**:

---

## T-3 - `configforge codec`: the serialization library's own program

* **id**: T-3
* **goal**: `configforge-codec`, linked to the serialization library
  alone, answers `formats` (moved from Doctor, which keeps it as an
  alias for 1.7.x), `detect <file>` (the format and how it was told)
  and `check <file>... [--as <format>]` (whether each file is well
  formed in its format).
* **why**: a document read wrong is a value explained wrong. Which
  format a file is, and whether it is one at all, are questions about
  bytes that need no contract and no configuration; asked of a program
  that is the serialization library and nothing more, the answer is the
  reader's own.
* **needs**: T-2, S-5
* **size**: medium
* **decided**:
  * **TO DECIDE - the tool's name.** Public from its first release, and
    on every user's PATH. Options:
    1. **`configforge-codec`**, run as `configforge codec <command>`:
       one program with commands; "codec" is what the library calls a
       format's reader and writer.
    2. **`configforge-serialize`**, matching S-4's library name: longer
       to type on every command line.
    3. **One program per command** (`configforge-convert`,
       `configforge-detect`, ...), as the Roadmap's example
       `configforge convert` reads: five programs on the PATH, and
       `configforge check` would be this tool's while
       `configforge gen check` is the contract's.
    Recommended: **1**. The rest of this card, T-4 and W5-V1 are
    written for 1. If 2: the name is replaced in those three cards and
    T-1's help line before any is started. If 3: stop; T-4 and W5-V1
    need redesign.
  * The program: `tools/codec/codec.c` (the command line),
    `tools/codec/codec.h`, `tools/codec/formats.c` (`git mv` from
    `tools/doctor/formats.c`), `tools/codec/detect.c` (new: `detect`
    and `check`), `tools/codec/CMakeLists.txt`; `tools/CMakeLists.txt`
    adds `add_subdirectory(codec)` outside the JSON and REPORT
    condition (it needs neither). Target `configforge_codec`,
    `OUTPUT_NAME configforge-codec`, linked to `configforge_serialize`
    only; its sources include `configforge/cf_serialize.h` and nothing
    else of the library, and are compiled without the internal tier
    (S-1 defines it in `tools/doctor/` and `tools/gen/` only): this
    program is the proof that the serialization library is enough. Otherwise as T-2's target (helpers,
    `CF_TOOLS_DIR`, the DLL copy of `configforge_serialize` on Windows
    shared builds, the run path, clang-tidy, the install list).
  * `codec.h`:

    ```c
    /* Every built-in codec, its extensions and its kind (formats.c). */
    int codec_formats(int argc, char** argv);
    /* The format of a file and how it was told (detect.c). */
    int codec_detect(int argc, char** argv);
    /* Whether each file is well formed in its format (detect.c). */
    int codec_check(int argc, char** argv);
    ```

    each taking what follows its command's name and returning the exit
    code. `formats.c`: `doctor_formats` becomes `codec_formats`, its
    include of `configforge/configforge.h` becomes `configforge/cf_serialize.h`
    and of `adapters.h` `codec.h`, its messages' prefix `doctor: `
    becomes `codec: `; nothing else.
  * `codec.c`'s `main`: `formats`, `detect`, `check` dispatch;
    `--version` alone prints `ConfigForge codec <cf_serialize_version()>`;
    anything else the usage on stderr, exit 2. The usage, exactly:

    ```text
    ConfigForge codec: documents, in every format the library reads

      configforge codec formats [--format text|markdown]
                                     the built-in codecs and their kind
      configforge codec detect <file>
                                     the format of a file, and how it was told
      configforge codec check <file>... [--as <format>]
                                     whether each file is well formed
      configforge codec --version

    Exit codes: 0 done (check: every file is well formed), 1 a file is
    not, or its format cannot be told; 2 unusable input.
    ```

    T-4 and W5-V1 add their lines to it.
  * Reading a file, one helper in `detect.c` used by both commands and
    later by T-4 and W5-V1 (declared in `codec.h`):

    ```c
    /* `path` read whole ("-" is standard input, binary) into `ar`, and
       its codec: `as` by name when not NULL, else from its name and then
       its content (cf_codec_detect, D-73). 0 when read, with *out and
       *how set; else the exit code (1 or 2) with the reason on stderr. */
    int codec_read(const char* path, const char* as, cf_arena* ar, cf_blob* out,
                   const cf_codec** codec, cf_detect* how);
    ```

    `cf_blob_file(out, ar, path, &err)` for a file - failure
    `codec: cannot read <path>` exit 2; standard input read in 64 KiB
    chunks into a growing `cf_arena_alloc`'d buffer (on Windows
    `_setmode(_fileno(stdin), _O_BINARY)` first, under `#ifdef _WIN32`
    as `tests/conformance/conform.c` has it - `gate.detection` reads
    `src/` and `include/` only, and this file includes no header of the
    platform layer), named `stdin`. Then `as`:
    `cf_codec_by_name(as)`; NULL with `cf_format_option(as)` not NULL
    is `codec: format '<as>' is not compiled into this build; turn on <option>`,
    with it NULL `codec: no format called '<as>'`, both exit 2.
    Without `as`: `cf_codec_detect(NULL, 0, out, path or NULL for "-", NULL, how)`;
    NULL with `how->first` set is
    `codec: <path> reads as <first> and as <second>, differently; name its format with --as`,
    else `codec: cannot tell the format of <path>; name it with --as`,
    both exit 1 (the bytes are the answer, not the command line). A
    codec that cannot read (`!cf_codec_can_read`) is
    `codec: format '<name>' can be written but not read`, exit 2.
  * `detect <file>`: exactly one file, else the usage. Prints
    `<name> (<how>)` and a newline, `how` being `extension` or
    `content`, exit 0; the failures above otherwise.
  * `check <file>... [--as <format>]`: every file is read with
    `codec_read` and decoded by `cf_codec_decode` into a sink that keeps
    nothing - its four callbacks return `CF_TRUE`, and `reserve_text`
    and `reserve_work` hand out `cf_arena_alloc` from one arena per file,
    so no reader refuses a long string for want of room (`cf_doc_sink`'s
    comment: without `reserve_text` JSON and TOML refuse strings over
    255 bytes). Each file prints `<path>: ok` or `<path>: <the reader's message>`;
    the exit code is the worst: 0, 1 for a file not well formed or of
    a format that cannot be told, 2 for one that cannot be read or an
    unknown `--as`. A TLV codec is not reachable (it needs a layout).
    YAML's own limits bound what an alias costs; nothing is kept, so
    D-82's bound has nothing to count here.
  * Doctor's alias: `formats` in Doctor runs
    `doctor_moved("formats", "configforge codec formats")` (T-2's) and
    then `codec_formats`; Doctor's executable compiles
    `../codec/formats.c` and includes `../codec/codec.h`; `adapters.h`
    loses `doctor_formats`.
  * The tests, `tests/codec/` (new), added by `tests/CMakeLists.txt`
    under `if(CF_BUILD_TOOLS AND TARGET configforge_codec)`:
    * `codec.formats`: `tests/doctor/formats.cmake` moves here
      (`git mv`) and runs `${CODEC} formats` where it ran
      `${DOCTOR} formats`; its regeneration text says
      `configforge codec formats --format markdown`. Registered as
      Doctor's was, with `-DCODEC=$<TARGET_FILE:configforge_codec>`.
    * `codec.detect` and `codec.check`, `tests/codec/detect.cmake` and
      `tests/codec/check.cmake` (new), each writing its inputs with
      `file(WRITE)` into `${WORK}` and checking exit code and output:
      detect - `a.json` `{"a": 1}`: 0, `json (extension)`; `noext`
      holding `{"a": 1}`: 0, `json (content)`; an empty file
      `empty` (no extension): 1, `cannot tell the format`;
      `missing.json`: 2. (Two loose formats reading one text
      differently is `cf_codec_detect`'s own test's to show; this
      checks only that the message passes it on, by reading the code.)
      check - `good.json` `{"a": [1, 2]}` and `good.toml` `a = 1\n`: 0,
      two `: ok` lines; `bad.json` `{"a": }`: 1, `bad.json: ` and the
      reader's message; `bad.json --as toml`: 1; `x --as nosuch`: 2,
      `no format called 'nosuch'`; a JSON string of 300 bytes: 0 (the
      sink lends room).
    * `doctor.formats_alias` in `tests/doctor/CMakeLists.txt`:
      `formats`, exit 0, `-DEXPECT_ERR='formats' is 'configforge codec formats' since 1.7.0`.
    * `tests/front/front.cmake`: `codec formats`: 0, stdout `json`.
  * Where it runs: every platform the tools build on; the `minimal`
    preset builds it (it needs no option), `-DEXTRA=wasm` does not
    (tools are off there).
* **files**: `tools/codec/codec.c`, `codec.h`, `detect.c`,
  `CMakeLists.txt` (new), `tools/codec/formats.c` (moved),
  `tools/CMakeLists.txt`, `tools/doctor/CMakeLists.txt`,
  `tools/doctor/doctor.c`, `tools/doctor/adapters.h`,
  `tests/codec/CMakeLists.txt`, `detect.cmake`, `check.cmake` (new),
  `tests/codec/formats.cmake` (moved), `tests/CMakeLists.txt`,
  `tests/doctor/CMakeLists.txt`, `tests/front/front.cmake`; last:
  `docs/Tools.md`, `docs/Doctor.md`, `docs/Formats.md`, `docs/Guide.md`,
  `docs/Decisions.md`, `CHANGELOG.md`, `docs/Roadmap.md`.
* **steps**:
  1. The maintainer's answer is written in this card (loop step 4).
  2. `tests/codec/` and the alias test. See them fail:
     `cmake --build --preset default` stops at `tests/codec` (no
     `configforge_codec`).
  3. The program with `formats` moved; `codec.formats` and
     `doctor.formats_alias` pass.
  4. `codec_read`, `detect`, `check`; their tests pass.
  5. Docs. ACCEPT-LINUX (`minimal` and `portable` build it with every
     other option off), ACCEPT-WINDOWS (`-` read as binary is W5-V1's
     to test; here the build).
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI; `codec.*`,
  `doctor.formats_alias`, `front.run`.
* **stop if**: `configforge_codec` does not link against
  `configforge_serialize` alone (a function S-3 and S-4 did not bring -
  name it); or `cf_codec_detect` with a NULL arena reads differently
  from the builder's (it makes and frees its own: read `detect.c`'s
  handling of NULL first).
* **docs**: T-1's D-number gains "`configforge codec`, the
  serialization library's program, linked to it alone; `formats`
  moved; `detect` and `check`; the maintainer's answer on the name".
  `docs/Tools.md`: a section "configforge codec" with the three
  commands, their outputs and exit codes, and that it reads documents
  and never a configuration (no contract, no key space, no layers).
  Doctor.md: section 7's `formats` lines go, the alias said with the
  others. Formats.md line 28 and Guide (line 511 on 2026-11-16):
  `doctor formats` becomes `configforge codec formats`; D-97 is not
  edited (a decision is kept as it was taken; this card's D says what
  moved). CHANGELOG, 1.7.0, "### Added": "**`configforge codec`**: the
  serialization library's own program - `formats`, `detect` (a file's
  format and how it was told) and `check` (whether each file is well
  formed), linked to that library alone." "### Changed": "`doctor
  formats` is `configforge codec formats`; Doctor keeps it in 1.7.x."
  Roadmap: "Doctor does one job" goes 🟩, "three programs behind
  `configforge`; Doctor's aliases go in 1.8.0 (T-5)"; "The
  serialization library's own tool" goes 🟨, "formats, detect and
  check done; dump (T-4) and convert (W5-V1) left".
* **notes**:

---

## T-4 - `configforge codec dump`: every value with its place

* **id**: T-4
* **goal**: `configforge codec dump <file> [--as <format>]` prints one
  line for each event of the document - each table and array opened,
  each value with its type - with the place the reader gives it: a
  line and column for text, a byte offset for CBOR.
* **why**: a binary configuration cannot be read by eye, and a value
  that cannot be read cannot be checked. The place is the one
  `explain()` would name, so what the dump shows is what a report
  will say.
* **needs**: T-3
* **size**: medium
* **decided**:
  * `tools/codec/dump.c` (new), `int codec_dump(int argc, char** argv);`
    in `codec.h`, dispatched by `codec.c` as `dump`; the usage gains
    `"  configforge codec dump <file> [--as <format>]\n"`
    `"                                 every value and table, with its place\n"`.
    Arguments: exactly one file, `--as` once; else the usage.
  * Read with `codec_read` (T-3); decoded by `cf_codec_decode` into a
    sink of `dump.c`'s (statics `dp_`) that prints as the events come -
    nothing is kept, so a large document costs its own bytes and the
    path. The path is the names joined by `.`, an array's members by
    their index from 0, kept in a `char` buffer of `CF_KEY_MAX * 2`
    bytes (static to the sink's context, not the frame); a path that
    does not fit is printed ending in `...`. `reserve_text` and
    `reserve_work` lend from the arena, as `check`'s sink does.
  * One line an event, fields separated by a tab:
    * a table or array opened: `-`, the path (`.` for the document
      itself), `table` or `array`;
    * a value: its place by `cf_loc_text(at, buf, sizeof buf)` - "app.toml:3:5",
      or "app.cbor, byte 12" for CBOR, which names its places so
      (`cbor.c`, `cbor_place`) - the path, its type by
      `cf_type_name(v->type)`, then its text by `cf_value_to_text`
      (`"text"` quoted, with `\`, `"` and every byte below 0x20 or 0x7f
      written as `\\`, `\"`, `\n`, `\r`, `\t` or `\xNN`, so nothing a
      file holds starts a line of its own in a CI log), and ` (textual)`
      when the reader said the value was untyped text;
    * an end: nothing.
    A place buffer of 160 bytes; one that the reader leaves empty is `-`.
  * Exit codes: 0 dumped; 1 the format cannot be told, or the reader
    refused the bytes (its message on stderr, after what was printed);
    2 unusable input. A TLV codec is not reachable (no layout); the
    image (`cf_image`) is ConfigForge's format, read by `libconfigforge`,
    not by this program - not here, and said so in Tools.md.
  * The test, `tests/codec/dump.cmake` (new), registered `codec.dump`:
    * `a.json` `{"s": {"p": 8080}, "t": ["x"]}`: 0; lines
      `-\t.\ttable`, a line matching `^a.json:1:[0-9]+\ts.p\tint\t8080$`
      (the column is the JSON reader's to give), `-\tt\tarray`,
      and `t.0\ttext\t"x"`.
    * `c.ini` `[s]\nk = v\n`: the `k` line ends `text\t"v" (textual)`.
    * CBOR, under `-DCBOR=ON` (`CF_WITH_CBOR`): the bytes `A1 61 61 01`
      (a map, "a" to 1), made by `string(ASCII 161 97 97 1 out)` and
      written by `file(WRITE)` as `m.cbor`: a line
      `m.cbor, byte 3\ta\tint\t1`.
    * a text value `"a\nb"` in JSON: its line holds `"a\nb"`, escaped,
      and the output has as many lines as events.
    * `bad.json` `{"a": }`: 1, the reader's message on stderr.
* **files**: `tools/codec/dump.c` (new), `tools/codec/codec.h`,
  `tools/codec/codec.c`, `tools/codec/CMakeLists.txt`,
  `tests/codec/dump.cmake` (new), `tests/codec/CMakeLists.txt`; last:
  `docs/Tools.md`, `CHANGELOG.md`, `docs/Roadmap.md`.
* **steps**:
  1. `dump.cmake` and its registration; a `codec_dump` that returns 2.
     See it fail: `ctest --preset default -R codec\\.dump`.
  2. The sink and its printing. Rerun: passes.
  3. Docs. ACCEPT-LINUX, ACCEPT-WINDOWS.
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI; `codec.dump`.
* **stop if**: `string(ASCII 1 ...)` cannot make a byte below 0x20 in
  CMake 3.10 (then the CBOR input is a fixture file checked in, binary,
  under `tests/codec/data/` with a `.gitattributes` entry - hand back
  for that to be decided); or a reader gives a place that
  `cf_loc_text` prints empty for a value (then the dump's promise of
  "each value with its place" does not hold for that format: name it).
* **docs**: no D-number. `docs/Tools.md`, the codec section: `dump`,
  its line format, an example of six lines, and the image's and TLV's
  absence with the reason. CHANGELOG, 1.7.0, "### Added": "`configforge
  codec dump`: every value of a document, with its type and its place
  - a line and column, or a byte offset for CBOR." Roadmap: "The
  serialization library's own tool" keeps 🟨, "dump done; convert left
  (W5-V1)".
* **notes**:

---

## T-5 - Doctor's aliases, and the name `doctor`, removed

* **id**: T-5
* **goal**: in 1.8.0 Doctor no longer answers `check`, `gen` or
  `formats`, the program is not built or shipped as `doctor`, and the
  Python wrapper no longer looks for `doctor`.
* **why**: T-1, T-2 and T-3 said "goes in 1.8.0" on every run; a
  promise to remove that is not kept becomes the second name kept
  forever, and two names for one program is how a reader is unsure
  which is meant.
* **needs**: 1.7.0 released, W5-G3 (the last 1.7.0 card)
* **size**: small
* **decided**:
  * `tools/doctor/doctor.c`: `doctor_moved`, the three alias branches
    and `old_name` go; `check`, `gen` and `formats` fall to the usage,
    exit 2, as any unknown command. `tools/doctor/CMakeLists.txt`:
    `../gen/contract.c` and `../codec/formats.c` leave the executable's
    list, the `POST_BUILD` copy and its `install(PROGRAMS ...)` go.
  * Tests: `doctor.check_alias`, `doctor.gen_alias`,
    `doctor.formats_alias`, `doctor.old_name` go; one case,
    `doctor_case(moved_check 2 check|--schema|${contract}|cf_fx_contract_ok.json)`,
    stays to show the name is gone.
  * `release.yml`: no `doctor` in `bin/`. `configforge_doctor.py`:
    `shutil.which("doctor")` goes, and the docstring's "(or an older
    `doctor`)".
  * The opening of 1.8.0, if this is its first card,
    `cmake -DTO=1.8.0 -P scripts/release/version.cmake`, as the
    preamble's rule for 1.7.0.
* **files**: `tools/doctor/doctor.c`, `tools/doctor/CMakeLists.txt`,
  `tools/CMakeLists.txt`, `tests/doctor/CMakeLists.txt`,
  `.github/workflows/release.yml`,
  `bindings/python/src/configforge_doctor.py`; last: `docs/Doctor.md`,
  `docs/Tools.md`, `CHANGELOG.md`, `docs/Roadmap.md`.
* **steps**:
  1. The tests changed. See `doctor.moved_check` fail (it exits 0).
  2. The code and CMake. `ctest --preset default -R "doctor\\.|front\\."`.
  3. Docs. ACCEPT-LINUX, ACCEPT-WINDOWS; the release dry run green.
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI.
* **stop if**: anything in the tree still runs `doctor check`, `doctor
  gen`, `doctor formats` or a program named `doctor`
  (`git grep -n -E "doctor (check|gen|formats)|/doctor\\b|doctor\\.exe"`
  outside CHANGELOG and Decisions) - change it first, in this card, and
  name it in the commit message.
* **docs**: T-1's D-number gains "the aliases and the old name went in
  1.8.0". Doctor.md: the alias sentences go. CHANGELOG, 1.8.0,
  "### Changed": "Doctor no longer answers `check`, `gen` or `formats`,
  and the program `doctor` is gone: `configforge gen check`,
  `configforge gen`, `configforge codec formats` and `configforge-doctor`
  (D-<T-1's>)." Roadmap: nothing (the item was 🟩 at T-3).
* **notes**:
