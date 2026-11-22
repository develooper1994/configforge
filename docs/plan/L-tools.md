# L - the tools, by group

Every kind of tool that finds a fault the others miss, in groups, each with
what the project runs today and what it adds. The rule: a tool joins when it
is free and finds what nothing else here finds; it never replaces one that
works (formatting churn is the cost of a swap, not of an addition). Paid
tools are listed as wished for, not planned: they may never be used.

Where a tool runs: `CI` (a job or step in `.github/workflows`), `verify`
(`scripts/verify.cmake`, a preset or an `EXTRA` step), `planned` (a card
or milestone names it), `new` (a card below).

## The groups

| Group | What it finds | Today | Added |
| ----- | ------------- | ----- | ----- |
| 1. Formatting | layout that differs from the rule | clang-format 22.1.8 (CI, verify, `scripts/work/format.sh`), cmake-format (verify) | - |
| 2. Linters, per language | mistakes a language's own checker knows | C: cppcheck, clang-tidy (CI, verify); Rust: clippy (CI); CMake: cmake-lint (verify); shell: shellcheck, Python: ruff, workflows: actionlint (verify, CI `lint`; L-1) | - |
| 3. Static analysis, path-sensitive | a fault on one path through the code | clang-tidy's `clang-analyzer-*`, GCC `-fanalyzer` (analyze preset), MSVC `/analyze` (CI), CodeQL (CI) | Infer (planned in cmake, made a CI job: L-3) |
| 4. Static analysis, sound | the proof that a class of fault cannot happen | CBMC (verify `EXTRA`, planned for 1.9.0) | IKOS, Frama-C EVA (L-3, 1.9.0) |
| 5. Rules and standards | a rule a safety standard writes | - | cppcheck's MISRA addon (1.9.0), CERT and CWE tables (1.9.0), semgrep for the project's own rules (L-3) |
| 6. Compilers | what one compiler accepts and another refuses | GCC, clang, MSVC, tcc, pcc, musl, FreeBSD clang (CI `compilers`, matrix) | - |
| 7. Run-time checks | memory and thread faults while tests run | ASan, LSan and UBSan's default checks (sanitize preset, GCC and MSVC), TSan (CI), Valgrind (verify `EXTRA`; CI `valgrind`), MemorySanitizer (the `msan` preset, `EXTRA=msan`, CI `msan`), clang's extra UBSan groups (the `ubsan-strict` preset, `EXTRA=ubsan-strict`, CI `ubsan-strict`), fatal and wider sanitizer options in the test presets (L-2) | HWASan with the Arm CI (1.9.0) |
| 8. Fuzzing | input no test thought of | libFuzzer (CI), ClusterFuzzLite (cflite), AFL++ on the same targets (`EXTRA=afl`, CI `afl`; L-2) | OSS-Fuzz (1.9.0) |
| 9. Coverage and mutation | tests that run code without checking it | gcov (CI `coverage`) | mutation testing with Mull (planned for 1.9.0) |
| 10. Interface | a release that breaks callers, a header that includes what it does not use | abidiff (verify `EXTRA`); include-what-you-use on `lib/*/src` (CI `iwyu`; L-4) | - |
| 11. Supply chain and security | a leaked secret, a weak dependency, an untraceable build | Dependabot, an SBOM with the release, reproducible-build check (CI), REUSE (verify), gitleaks (CI `secrets`), osv-scanner (CI `osv`), OpenSSF Scorecard (`scorecard.yml`, weekly; the badge in the README) (L-2) | - |
| 12. Documentation | a dead link, a misspelling | mkdocs build (CI `docs`); codespell (verify, CI `lint`; L-1); lychee: the links inside the repository (`gate.links`, verify, CI `lint`) and, weekly, those to other sites (`docs.yml` job `links`; L-4) | - |
| 13. Profiling and measurement | where the time and the memory go, so that a speed-up starts from data | `cf_bench` (P-1, CI `bench` runs it); `scripts/work/profile.py` (L-5): perf, callgrind, cachegrind, heaptrack or massif, GCC's vectorization notes on Linux, Visual Studio's collector and xperf on Windows, each where it is present | hyperfine for whole-program timings, uftrace for call traces, `setup.sh` installing the profilers (L-5); Intel VTune and AMD uProf (free, vendor-bound) where the maintainer has them |

Wished for, paid (open-source projects may get them free; to be asked for
only by the maintainer): Coverity Scan, PVS-Studio, Polyspace, LDRA,
Parasoft C/C++test - each with a MISRA checker of its own, which would be a
second opinion beside cppcheck's.

## L-1 - the other languages' linters

* **id**: L-1
* **goal**: shellcheck, ruff, actionlint and codespell run in `verify` and CI, and the tree passes them.
* **why**: the scripts that land every card, the Python binding and the workflows had no checker; two faults in `scripts/work` on 2026-11-19 were of shellcheck's kind.
* **needs**: 1.7.0 released
* **size**: medium
* **decided**: each tool a gate test (`gate.shellcheck`, `gate.ruff`, `gate.actionlint`, `gate.codespell`) beside `gate.format.cmake`, skipped with a message when the tool is absent, as the format gates are; `setup.sh` and `dev-setup` install them; the first pass fixes what they find, or records a reasoned exception in the tool's own config file (`.shellcheckrc`, `pyproject.toml [tool.ruff]`, `.codespellrc`).
* **accept**: ACCEPT-LINUX, ACCEPT-CI.

## L-2 - run-time, fuzzing and supply chain in CI

* **id**: L-2
* **goal**: Valgrind's memcheck, AFL++ on the libFuzzer targets, gitleaks, osv-scanner and OpenSSF Scorecard run in CI; and every sanitizer a compiler offers for C on the CI's targets runs: MemorySanitizer (a clang preset `msan`), clang's extra UBSan groups (the preset `ubsan-strict`), and run-time options that make a finding fatal and look further in the presets' test environment. HWASan comes with the Arm CI (1.9.0).
* **why**: the sanitize preset reads what ASan, LSan and UBSan's default checks see; a read of memory nobody wrote (MSan, Valgrind), a narrowing that loses a value and an input only a second fuzzer's mutations reach (AFL++) are seen by none of them. A leaked secret, a vulnerable pinned dependency and a repository whose practices nobody outside has measured are faults no test can show.
* **needs**: L-1 (done, 7bec7d8)
* **size**: medium
* **decided**: `EXTRA` steps `msan`, `ubsan-strict` and `afl` (new, tier 2) and `valgrind` (existing, now a CI job), each skipped with a message where its tool is absent; CI jobs `valgrind`, `msan`, `ubsan-strict`, `afl`, `secrets`, `osv`, and `scorecard.yml` of its own (weekly; no `repo_token`; the badge in the README). The preset `msan` builds with no optional library (`CF_WITH_XML`, `CF_WITH_SQLITE`, `CF_WITH_CXX` OFF; the built-in YAML parser stays): a library not built with MSan makes every value it returns read as uninitialised, and `docs/Dependencies.md` says what MSan does not cover; its tests run under `setarch -R`, as TSan's do, and a test of glibc's fault is labelled `no-msan`. The preset `ubsan-strict` adds `integer`, `implicit-conversion`, `nullability` and `local-bounds` to `CF_SANITIZE` (`CF_SANITIZE_STRICT`, clang only); a wrap or narrowing the code means is listed by function in `.ubsan-ignorelist.txt` with its reason, never a bare `src:` for a library; more than 40 distinct sites was the card's stop (the first pass found 31, none a narrowing, all wraps and shifts the code means). The `environment` of the `sanitize`, `ubsan-strict` and `msan` test presets: `halt_on_error=1:print_stacktrace=1` and, for ASan, `detect_stack_use_after_return`, `strict_string_checks`, `detect_invalid_pointer_pairs=2`, `check_initialization_order`. AFL++ (`afl-clang-fast`, preset `afl`, driver `scripts/check/afl.cmake`) runs each `fuzz_<reader>` of `tests/fuzz` unchanged for `FUZZ_SECONDS` (60 in CI, as `libfuzzer`'s); a file `id:*` under `crashes/` fails it and the CI job keeps `afl-out` as `afl-findings`. gitleaks (`.gitleaks.toml`: the defaults and two narrow allowlist entries) and osv-scanner (the Rust binding's and the documentation's lockfiles) are the release binaries pinned and checked against the SHA-256 their release publishes, in `setup.sh` and the jobs. The first passes fixed a struct's padding copied unwritten into the arena (MSan, `validate.c`) and made the live-reload thread test take smaller counts under Valgrind (a spin lock and one thread at a time).
* **accept**: ACCEPT-LINUX, ACCEPT-CI with the new jobs green and `gate.actionlint` on `scorecard.yml`; `cmake -DEXTRA="valgrind;msan;ubsan-strict;afl" -DMODE=quick -DJOBS=3 -DFUZZ_SECONDS=10 -P scripts/verify.cmake` ending `all green`. `scorecard.yml`'s first run and the badge are the maintainer's to check after the card lands.
* **notes**: (empty)

## L-3 - more analysers

* **id**: L-3
* **goal**: Infer as a CI job; semgrep with the project's own rules (the ones gates now do by grep); IKOS or Frama-C EVA on the library's core, beside CBMC.
* **needs**: L-2; the sound analysers belong to 1.9.0
* **size**: large

## L-4 - interface and documentation

* **id**: L-4
* **goal**: include-what-you-use on the library's sources and lychee on the docs site, in CI.
* **why**: a header that includes what it does not use binds two components the layering table keeps apart (D-100); a dead link in the docs is a reader sent nowhere.
* **needs**: L-1
* **size**: small
* **decided**:
  * **include-what-you-use**: a CI job `iwyu` (ubuntu-26.04; `apt-get install iwyu`) builds the library with `-DCMAKE_C_INCLUDE_WHAT_YOU_USE="include-what-you-use;-Xiwyu;--error;-Xiwyu;--mapping_file=${{ github.workspace }}/.iwyu.imp"` and fails on a finding. Only `lib/*/src` is held to it (the tests and tools are not, in this card). `.iwyu.imp` (new, at the root) maps what iwyu would get wrong on this project: `cf_build_options.h` and `cf_port.h` as the providers of the fixed-width types and the `CF_` macros. The first pass fixes each finding in the sources, or keeps an include iwyu calls unused with `/* IWYU pragma: keep */` and the reason after it, on the same line. `setup.sh` installs iwyu; no gate in verify (iwyu's output differs by version: CI's version is the judge, as clang-format's is).
  * **lychee**: `gate.links` in verify and CI (beside `gate.codespell`, skipped when `lychee` is absent), `lychee --offline --no-progress docs README.md CONTRIBUTING.md CHANGELOG.md scripts/README.md`: every link inside the repository and every anchor. Outside links are checked weekly by a CI job `links` (a `schedule:` of its own in `docs.yml`, `lychee --no-progress` with `--max-retries 3`, failing the job, not the push). `setup.sh` pins lychee's release binary as it pins clang-format; `.lychee.toml` (new) holds the exceptions, each with its reason.
* **files**: `.github/workflows/ci.yml` (job `iwyu`), `.github/workflows/docs.yml` (job `links`), `.iwyu.imp`, `.lychee.toml`, `cmake/ConfigForgeFormat.cmake` (`gate.links`), `scripts/buildenv/setup.sh`, the sources iwyu's first pass names; last: `docs/plan/L-tools.md` (group 10 and 12 "Today"), `CHANGELOG.md`.
* **steps**: 1. `gate.links`, see it fail on the tree, fix the links. 2. The `iwyu` job, its first run on a branch, fix what it names. 3. The weekly `links` job. 4. ACCEPT.
* **accept**: ACCEPT-LINUX, ACCEPT-CI with the `iwyu` job green.
* **stop if**: iwyu's fix for a header would include a header across the layering table's order (gate.layering refuses it): keep the include with the pragma and note it, do not reorder components.

## L-5 - profilers, so that every speed-up starts from data

* **id**: L-5
* **goal**: one command, `scripts/work/profile.sh <format>...`, builds the
  benchmark with symbols and writes, for each format, the top functions
  by time (perf where the kernel has it, callgrind otherwise), the cache
  misses (cachegrind), the peak and the count of allocations (heaptrack,
  else massif) and the compilers' vectorization report for the functions
  it names - and on Windows the same table from Visual Studio's collector
  and `xperf`, which need no administrator (`docs/Performance.md`);
  `setup.sh` installs the tools; `docs/Performance.md` explains
  each table and is refreshed by the command.
* **why**: P-1's profile was taken by hand, and `perf` is missing in WSL;
  the P cards and the SIMD backend (2.4.0) are to change only what a
  profile names, and to show the gain under the same command.
* **needs**: P-1 (done)
* **size**: medium
* **before**: P-2 - no word-at-a-time code is written without this
  command's numbers before and after.

L-3 is an outline; it is written out in full when it opens.
