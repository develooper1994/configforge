# L - the tools, by group

L-1 to L-10 and L-3b are done and removed (docs/Plan.md, "Done, their
cards removed"). No card here is open.

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
| 3. Static analysis, path-sensitive | a fault on one path through the code | clang-tidy's `clang-analyzer-*`, GCC `-fanalyzer` (analyze preset), MSVC `/analyze` (CI), CodeQL (CI), Infer (`EXTRA=infer`, CI `infer`; L-3) | - |
| 4. Static analysis, sound | the proof that a class of fault cannot happen | CBMC (verify `EXTRA`, planned for 1.9.0) | Frama-C EVA on `port`, the integer half of `numeric` and `base` (`EXTRA=eva`, CI `eva`; L-3b, 4c3ce8c); IKOS was weighed and not chosen |
| 5. Rules and standards | a rule a safety standard writes | semgrep for the project's own rules (`gate.semgrep`, `.semgrep/`; verify, CI `lint`; L-3) | cppcheck's MISRA addon (1.9.0), CERT and CWE tables (1.9.0) |
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
