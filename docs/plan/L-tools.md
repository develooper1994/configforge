# L - the tools, by group

L-1 to L-10 are done and removed (docs/Plan.md, "Done, their cards removed");
L-3b, below, is open.

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
| 4. Static analysis, sound | the proof that a class of fault cannot happen | CBMC (verify `EXTRA`, planned for 1.9.0) | Frama-C EVA on `port`, `numeric` and `base` (`EXTRA=eva`, CI `eva`; L-3b, 1.9.0); IKOS was weighed and not chosen |
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

## L-3b - a sound analyser: Frama-C EVA on the bottom components

* **id**: L-3b
* **goal**: Frama-C's EVA plugin proves, for `lib/port`, `lib/numeric` and `lib/base`, that no execution has an out-of-bounds access, a read of uninitialised memory, a signed overflow, a division by zero or an invalid shift, as `EXTRA=eva` (tier 2) and a CI job `eva`; every alarm is fixed or recorded with its reason.
* **why**: CBMC proves the same classes for inputs up to a bound (`tests/proofs`); EVA proves them for every value of an integer argument by abstract interpretation, and its alarms are the places it could not prove, not a sample. The two disagree usefully: a proof one has and the other lacks is a hole in a harness or in a bound.
* **needs**: L-3; 1.9.0 under way (Maintaining.md: "trusted, on every target" lists the proofs; this is one of them)
* **size**: large
* **decided**:
  * **Frama-C, not IKOS.** IKOS 3.5 (December 2024, the last release) ships no binary (its release has no assets), builds only against LLVM 14, which Ubuntu 26.04 does not package, and speaks LLVM bitcode, so the C89 sources go through clang first. Frama-C 33.0 reads C directly, is on opam, has EVA as a plugin of the one package, and a released version can be pinned. Ubuntu's own `frama-c-base` is the 2022 release and is not in 26.04. Frama-C's licence is LGPL-2.1; it is run, never linked.
  * **Install**: `opam` from apt, `opam init --bare --disable-sandboxing -y`, a switch `cf-eva` with `ocaml-base-compiler.5.3.0` (the version `opam show frama-c.33.0` lists as supported; the worker uses the newest it lists if 5.3.0 is not), `opam install -y frama-c.33.0`. opam checks the checksums its package files carry for each source; the repository itself is pinned: `opam repository set-url default git+https://github.com/ocaml/opam-repository#<the commit the worker records on the day, 40 characters>`. The job caches `~/.opam` with `actions/cache` (pinned by commit like the other actions), key `eva-<frama-c version>-<ocaml version>-<repository commit>`; the first run builds from source and takes 20 to 40 minutes, hence `timeout-minutes: 90`. `setup.sh` installs the same into `/opt/cf-tools/opam`, with `/opt/cf-tools/bin/frama-c` linking to it, only when `FRAMAC=1` is in its environment (it is slow and optional).
  * **Scope**: `lib/port/src/port.c`, `lib/numeric/src/numeric.c`, `lib/base/src/alloc.c`, `error.c`, `key.c`, `value.c`: 2135 lines, the three components at the bottom of D-100's table (`port` uses nothing; `numeric` uses `port` and `lanes`; `base` uses those and `numeric`), whose callers are everyone else. `lanes` is not in it (its portable and vector files are a different analysis). The next card extends it upward.
  * **How it runs**: one harness per component, `tests/eva/eva_<component>.c` (new), each calling every function the component's headers export under `CF_INTERNAL_API=1` with arguments from Frama-C's builtins (`Frama_C_interval`, from `__fc_builtin.h`): integers in their whole range, a pointer to a stack buffer of a fixed size (64 bytes) with a length in `0..64`, the string arguments NUL-terminated at an unknown place. `scripts/check/eva.cmake -DSRC= -DSTAGED=` (new) runs, for each harness, `frama-c -cpp-extra-args="-I<staged> -DCF_INTERNAL_API=1" -machdep gcc_x86_64 -eva -eva-precision 3 -eva-slevel-function <fn>:<n> where a loop needs it -warn-signed-overflow -warn-invalid-shift -eva-warn-copy-indeterminate -then -report -report-csv <out>.csv -no-unicode` and reads the CSV. Unsigned wrap is not an alarm (C defines it, and the code uses it on purpose); a signed overflow is. Flag spellings are confirmed against `frama-c -help` of 33.0; a renamed flag is a step-order question, not a design one.
  * **What it must prove**: no property of status `Unknown` or `Invalid` among the kinds `mem_access`, `index_bound`, `initialization`, `signed_overflow`, `division_by_zero`, `shift`, `pointer_arithmetic`, `valid_pointer` for any line of the six files.
  * **Triage, in this order, for each alarm**: (1) it is a fault: fix the code, with a test in `lib/<component>/tests` that fails before (the regression, as the fuzzers' findings get); (2) the analysis is too coarse: raise `-eva-slevel-function` for that function (never above 500, never a global `-eva-slevel`), or split the harness's range; (3) it holds for a reason Frama-C cannot see: a line in `.eva-accepted.txt` (new, at the root): a `#` line with the reason, then `<file> <function> <kind>`; never a whole file. `scripts/check/eva.cmake` fails on an alarm not listed and on a listed entry no alarm matches.
  * **EXTRA step and CI job**: `EXTRA=eva` (tier 2; in the `TIER=2` list, the usage comment and the unknown-step message), skipped with `eva                   skipped: no frama-c` when `find_program(FRAMAC_EXE frama-c HINTS /opt/cf-tools/bin)` finds none; steps `eva configure` (the default preset's configure, for the staged headers), `eva run`. CI job `eva` as above, on every push to `main` and on pull requests that touch `lib/port`, `lib/numeric`, `lib/base`, `tests/eva`, `.eva-accepted.txt` or the job (its first step is `git diff --name-only` against the push's base; when none of those paths changed the job ends green at once, with no action from outside the repository), so the cached switch's 20 minutes are not paid on a documentation commit.
  * **Stop numbers**: more than 30 distinct alarms on the first pass after step 4's precision work; the install not reproducible twice from the pinned repository commit; one function whose alarm needs `-eva-slevel-function` above 500.
* **files**: `tests/eva/eva_port.c`, `eva_numeric.c`, `eva_base.c`, `scripts/check/eva.cmake`, `.eva-accepted.txt`, `scripts/verify.cmake` (`eva`), `scripts/buildenv/setup.sh`, `.github/workflows/ci.yml` (job `eva`), the sources and tests the alarms lead to; last: `docs/Safety.md` (a paragraph beside the CBMC one: what each proves, and that `.eva-accepted.txt` is the list of what is trusted), `docs/Dependencies.md` (a row: `Frama-C 33.0 (opam; LGPL-2.1-only) | `EXTRA=eva` (tier 2) and the CI job `eva`: EVA over `port`, `numeric` and `base` (`tests/eva`, `.eva-accepted.txt`); not linked` and one for `opam`), `docs/plan/L-tools.md` (group 4 "Today"), `CHANGELOG.md`.
* **steps**:
  1. `git switch -c work/L-3b`. Install Frama-C locally as above (WSL, off `/mnt`); record the opam repository commit.
  2. Write `tests/eva/eva_port.c` first, with one function of `port.c`; run `frama-c` by hand; see the CSV list its alarms. Then every exported function of the component; then `eva_numeric.c`, `eva_base.c`.
  3. `scripts/check/eva.cmake` and `EXTRA=eva`. See it fail: the first pass lists every alarm and exits 1. More than 30 distinct after precision work: stop.
  4. Triage each alarm as decided. See the fix of a real one demonstrated: revert the fix, see the alarm return and the new test fail, restore.
  5. The CI job `eva` and `setup.sh`'s `FRAMAC=1` branch; push the branch; the first run is the slow one, the second (from the cache) must take under 10 minutes.
  6. `docs/Safety.md`, `docs/Dependencies.md`, `docs/plan/L-tools.md`, `CHANGELOG.md`.
  7. ACCEPT.
* **accept**: ACCEPT-LINUX, ACCEPT-CI with `eva` green twice running (the second from the cache); `cmake -DEXTRA="eva;proofs" -DMODE=quick -DJOBS=3 -P scripts/verify.cmake` ending `all green`.
* **stop if**: a stop number above; an alarm that is a real fault needing a change to a public signature or limit (record it as accepted with the reason, write it under "notes", tell the maintainer); Frama-C 33.0 not installable from the pinned repository commit on the runner.
* **notes**: (empty)
