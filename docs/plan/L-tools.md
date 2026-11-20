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
| 7. Run-time checks | memory and thread faults while tests run | ASan and UBSan (sanitize preset), TSan (CI), Valgrind (verify `EXTRA`) | Valgrind as a CI job (L-2) |
| 8. Fuzzing | input no test thought of | libFuzzer (CI), ClusterFuzzLite (cflite) | AFL++ on the same targets (L-2); OSS-Fuzz (1.9.0) |
| 9. Coverage and mutation | tests that run code without checking it | gcov (CI `coverage`) | mutation testing with Mull (planned for 1.9.0) |
| 10. Interface | a release that breaks callers | abidiff (verify `EXTRA`) | include-what-you-use (L-4) |
| 11. Supply chain and security | a leaked secret, a weak dependency, an untraceable build | Dependabot, an SBOM with the release, reproducible-build check (CI), REUSE (verify) | gitleaks, osv-scanner, OpenSSF Scorecard (L-2) |
| 12. Documentation | a dead link, a misspelling | mkdocs build (CI `docs`); codespell (verify, CI `lint`; L-1) | lychee link check (L-4) |
| 13. Profiling and measurement | where the time and the memory go, so that a speed-up starts from data | `cf_bench` (P-1, CI `bench` runs it), Valgrind callgrind (the profile in `docs/Performance.md`) | perf where the kernel has it, cachegrind and the compilers' vectorization reports beside each profile, heaptrack or massif for memory, hyperfine for whole-program timings, uftrace for call traces (L-5); Intel VTune and AMD uProf (free, vendor-bound) where the maintainer has them |

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
* **goal**: Valgrind's memcheck, AFL++ on the libFuzzer targets, gitleaks, osv-scanner and OpenSSF Scorecard run in CI.
* **needs**: L-1
* **size**: medium
* **decided**: Valgrind and AFL++ as `EXTRA` steps of verify, run by CI jobs; gitleaks and osv-scanner on every push; Scorecard weekly, its result on the docs site.

## L-3 - more analysers

* **id**: L-3
* **goal**: Infer as a CI job; semgrep with the project's own rules (the ones gates now do by grep); IKOS or Frama-C EVA on the library's core, beside CBMC.
* **needs**: L-2; the sound analysers belong to 1.9.0
* **size**: large

## L-4 - interface and documentation

* **id**: L-4
* **goal**: include-what-you-use on the library's sources and lychee on the docs site, in CI.
* **needs**: L-1
* **size**: small

## L-5 - profilers, so that every speed-up starts from data

* **id**: L-5
* **goal**: one command, `scripts/work/profile.sh <format>...`, builds the
  benchmark with symbols and writes, for each format, the top functions
  by time (perf where the kernel has it, callgrind otherwise), the cache
  misses (cachegrind), the peak and the count of allocations (heaptrack,
  else massif) and the compilers' vectorization report for the functions
  it names; `setup.sh` installs the tools; `docs/Performance.md` explains
  each table and is refreshed by the command.
* **why**: P-1's profile was taken by hand, and `perf` is missing in WSL;
  the P cards and the SIMD backend (2.4.0) are to change only what a
  profile names, and to show the gain under the same command.
* **needs**: P-1 (done)
* **size**: medium
* **before**: P-2 - no word-at-a-time code is written without this
  command's numbers before and after.

L-2 to L-5 are outlines; each is written out in full when it opens.
