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

## L-3 - Infer and semgrep

* **id**: L-3
* **goal**: Infer runs as a CI job (`infer`) and as `EXTRA=infer`, and semgrep runs the project's own rules in `.semgrep/` as `gate.semgrep`, in verify and in CI's `lint`; the two source-pattern checks that were regexes in CMake (`gate.detection`, the include check of `gate.interface`) are semgrep rules now, and the tree passes all of it.
* **why**: Infer's Pulse follows a null, an uninitialised value and a use after free across function calls, and its buffer-overrun checker follows an index across them; clang-tidy, cppcheck and `-fanalyzer` stop at the function or the file. The project's own rules (no locale-sensitive call, no raw allocation outside `alloc.c`, no compiler or platform macro outside the portability layer) are held today by a regex in a CMake gate or by `nm` on a built library; a rule in semgrep has a test file that must match and must not match, names the line, and is read in source, so it covers the Windows and `none` platform files a Linux build never links.
* **needs**: L-2 (done, 6442c8f)
* **size**: large
* **decided**:
  * **Infer**: version 1.3.0, the release tarball `infer-linux-x86_64-v1.3.0.tar.xz` from `https://github.com/facebook/infer/releases/download/v1.3.0/`, SHA-256 `02be62ba931cb43e2d42bbf7abf4bd9a410cf8464cce5453934e25c90a825850` (the digest GitHub publishes for the asset; the worker checks it against the release page before using it). Installed as `setup.sh` installs gitleaks: `/opt/cf-tools/infer-1.3.0/`, `/opt/cf-tools/bin/infer` a link to its `bin/infer`, `/usr/local/bin/infer` a link to that, skipped when `infer --version` already says 1.3.0; CI's job `infer` repeats the same `curl`, `sha256sum -c`, `tar -xJf` into `$RUNNER_TEMP`. The runtime libraries the tarball needs are found with `ldd $RUNNER_TEMP/infer/lib/infer/bin/infer*` on the job's first run and added, by exact apt package name, to the job and to `setup.sh`; Infer's own clang front end is inside the tarball, and the build is made with the system's `clang`.
  * **How Infer runs**: the job configures a build of the libraries only, `cmake -S . -B binfer -DCMAKE_C_COMPILER=clang -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCF_BUILD_TESTING=OFF -DCF_BUILD_EXAMPLES=OFF -DCF_BUILD_TOOLS=OFF -DCF_WITH_CXX=OFF -DCF_WITH_XML=OFF -DCF_WITH_SQLITE=OFF` (clang, so that the recorded flags are ones Infer's front end knows; the optional libraries off, as the `msan` preset has them: Infer would see their headers and not their code), builds `configforge` and `configforge_serialize` (the generated headers are made by the build), then `infer run --compilation-database binfer/compile_commands.json --project-root . --results-dir binfer/infer-out --no-default-checkers --pulse --bufferoverrun --liveness --disable-issue-type INTEGER_OVERFLOW_L2 --disable-issue-type BUFFER_OVERRUN_L3 --disable-issue-type BUFFER_OVERRUN_L4 --disable-issue-type BUFFER_OVERRUN_L5 --report-force-relative-path` (unsigned wrap is defined in C and the code uses it on purpose, signed overflow is held by `ubsan-strict` and, with 1.9.0, EVA; L3 and above are Infer's least certain overrun reports; `INTEGER_OVERFLOW_L1`, `BUFFER_OVERRUN_L1`/`L2` and every Pulse type stay; the stop number counts distinct causes after this filter, findings sharing one cause in one callee counting as one; organiser, 2026-11-23). Checkers: `pulse` (null dereference, uninitialised value, use after free, leak), `bufferoverrun` and `liveness` (a dead store). Not `cost`, `loop-hoisting` or `biabduction`: they report speed or are superseded by Pulse. The names are confirmed against `infer help --list-checkers` of 1.3.0; one that is not there is a step-order question (Organiser's answers), not a design one.
  * **How a finding fails the job**: Infer's exit code is not used. `scripts/check/infer.py <build-dir> <repo>` (new; standard library only; ruff checks it) reads `<build-dir>/infer-out/report.json`, keeps what is in a file under `lib/*/src`, and exits 1 for each finding whose `bug_type`, `file` and `procedure` are not an entry of `.infer-ignore.txt`, and for each entry that no finding matches (a stale entry is an error, so that a fix removes its line). It prints each finding as `file:line: bug_type: qualifier`.
  * **A false positive**: one entry in `.infer-ignore.txt` (new, at the root): a `#` line with the reason, then `<bug_type> <file> <procedure>` on one line; never a whole file, never a bare bug type. Same form as `.ubsan-ignorelist.txt`. A real finding is fixed in the code; the entry is for what the code means and Infer cannot see.
  * **`EXTRA=infer`** (new, tier 2, in the `TIER=2` list, the usage comment and the unknown-step message of `scripts/verify.cmake`): `find_program(INFER_EXE infer HINTS /opt/cf-tools/bin)` and `clang`; absent either, `infer                 skipped: no infer` as the other steps say; otherwise the configure, the build and `scripts/check/infer.py` above, in `build-infer/`, as steps named `infer configure`, `infer build`, `infer run`. No `gate.*` test: Infer takes minutes and needs a build of its own.
  * **semgrep**: version 1.186.0 (PyPI, 2026-11-18). In CI it is installed from `scripts/check/semgrep-requirements.txt` (new): every package, semgrep's dependencies included, with its SHA-256, made by `pip-compile --generate-hashes --python-version <the runner's> scripts/check/semgrep.in` (`semgrep.in` holds the one line `semgrep==1.186.0`; the command is the file's first comment), installed with `pip install --require-hashes --no-deps -r`. Chosen over its docker image because the lint job already has a venv, and because osv-scanner reads a requirements file and cannot read an image: the card adds the file to the `osv` job's lockfiles. `setup.sh` has no repository beside it (the Containerfile copies it alone), so it pins `semgrep==1.186.0` by version as it pins ruff. Every run: `--metrics=off --disable-version-check`, never a registry rule (`--config` is only `.semgrep`).
  * **`gate.semgrep`** (in `cmake/ConfigForgeFormat.cmake`, beside `gate.links`; `find_program(SEMGREP_EXE NAMES semgrep)`; skipped with `semgrep not found: gate.semgrep skipped`): runs `cmake -DSEMGREP=<exe> -DSRC=<root> -P scripts/check/semgrep.cmake` (new), which does three things and fails on the first that fails: `semgrep --test --config .semgrep .semgrep` (every fixture's `ruleid:` lines matched, every `ok:` line not); `semgrep scan --config .semgrep --error --quiet --metrics=off --disable-version-check lib tools/doctor/include`; and a search of `lib` and `tools` for `nosemgrep` comments without ` -- <reason>` after the rule id (an exception without its reason fails). CI: the `lint` job installs semgrep as above and its `ctest -R` pattern gains `semgrep`. `docs/Dependencies.md` names `semgrep` (the `find_program` makes `gate.dependencies` require it).
  * **The rules**, one `.semgrep/<name>.yaml` each, with its fixture `.semgrep/<name>.c` beside it (`// ruleid: <id>` or `// ok: <id>` on the line before each case; the fixtures are not compiled and not formatted: `.clang-format`'s file list is `lib`, `tools` and `tests`). A rule has no `paths.include` except where said (a fixture sits in `.semgrep/` and must be reached); each exclusion is a `paths.exclude` entry. Each rule's `message` says what to use instead.
    * Moved from CMake (the regex becomes a `pattern-regex` rule, because a `#if` line is not in the syntax tree semgrep builds): `cf-detect-compiler` (`__STDC_VERSION__`, `_MSC_VER`, `__GNUC__`, `__clang__`; excluded: `**/cf_port.h`, `**/port.c`, `**/cf_platform.h`, `**/cf_platform_file.h`, `lib/*/tests/`, `lib/lanes/`; the sets are `GateDetection.cmake`'s, line for line); `cf-detect-platform` (`_WIN32`, `_WIN64`, `__unix__`, `__unix`, `__linux__`, `__APPLE__`, `__FreeBSD__`, `__MINGW32__`, as whole words; the same exclusions and `lib/platform/`); `cf-public-header-include` (a `#include <...>` of anything but `stddef.h`, `stdarg.h`, `stdint.h`; `paths.include`: `**/include/configforge/*.h` and `.semgrep/cf-public-header-include.c`; `GateInterface.cmake`'s first loop).
    * New, on the syntax tree: `cf-no-locale-call` (a call of `strtod`, `atof`, `atoi`, `atol`, `strtol`, `strtoul`, `tolower`, `toupper`, `isalpha`, `isdigit`, `isspace`, `isalnum`, `isupper`, `islower`, `strcoll`, `strxfrm`, `localeconv`, `setlocale`, `sprintf`, `snprintf`, `vsprintf`, `vsnprintf`, `printf`, `fprintf`; the list of `GateSymbols.cmake`'s `FORBIDDEN`, minus its unbounded copies; excluded `lib/*/tests/`); `cf-no-unbounded-copy` (`strcpy`, `strcat`, `strncpy`, `strncat`, `strtok`, `gets`); `cf-no-raw-alloc` (`malloc`, `calloc`, `realloc`, `free`; excluded `lib/base/src/alloc.c` and `lib/*/tests/`; `GateSymbols.cmake`'s last check, in source). The two `nm` checks stay: they read what is linked, a macro or an inline function included, and these rules read what is written; neither replaces the other.
  * **What stays as it is, and why**: `gate.layering` (it reads the component table the build writes, `layers.cmake`; a regex over data semgrep does not see); `gate.version`, `gate.text` (bytes, not code); `gate.dependencies` (it reads CMake files); `gate.symbols`, `gate.install`, `gate.abi` (a built library); `gate.headers` (a compiler). The second half of `gate.interface` (the preprocessed length) stays.
  * **Stop numbers**: more than 25 distinct Infer findings on the first pass, or more than 15 distinct semgrep matches on the tree by the new rules (a moved rule matches nothing: it was a gate), or Infer 1.3.0 or semgrep 1.186.0 not obtainable at the pinned checksum.
* **files**: `.github/workflows/ci.yml` (job `infer`; `lint`'s installation and its `ctest -R`; `osv`'s lockfiles), `.semgrep/*.yaml` and `.semgrep/*.c` (the six rules and their fixtures), `scripts/check/semgrep.cmake`, `scripts/check/semgrep.in`, `scripts/check/semgrep-requirements.txt`, `scripts/check/infer.py`, `.infer-ignore.txt`, `cmake/ConfigForgeFormat.cmake` (`gate.semgrep`), `cmake/ConfigForgeGates.cmake` and `cmake/gates/GateDetection.cmake` (removed), `cmake/gates/GateInterface.cmake` (its first loop removed), `scripts/verify.cmake` (`infer`), `scripts/buildenv/setup.sh` (infer, semgrep), the library sources the first passes name; last: `docs/Conventions.md` (`[gate: gate.detection]` becomes `[gate: gate.semgrep]`; and the table's gate line), every other mention of `gate.detection` that `grep -rn gate.detection docs cmake scripts` finds, `docs/Dependencies.md` (rows below), `docs/plan/L-tools.md` (group 3 and 5 "Today", this card), `CHANGELOG.md`.
* **steps**:
  1. `git switch -c work/L-3`. Confirm the two digests (Infer's release page; semgrep's PyPI file list) and write them nowhere until step 2 uses them.
  2. `scripts/check/infer.py` and an empty `.infer-ignore.txt`; the `EXTRA=infer` step; `setup.sh`'s installation. Run `cmake -DEXTRA=infer -DMODE=quick -DJOBS=3 -P scripts/verify.cmake` in WSL (`machine-disk-and-wsl`: build off `/mnt`): see it list the findings. More than 25 distinct: stop (stop if). Otherwise fix each in the code, or give it an entry with its reason; the run ends `all green`.
  3. See `infer.py` fail on a made-up case: remove one line of the first fix, run, see the finding return; restore it. A stale entry is seen too: add an entry for a function that has no finding, run, see the error, remove it.
  4. The CI job `infer` (ubuntu-26.04, `timeout-minutes: 60`): apt `clang` and the libraries step 2 found, the pinned download, the configure and build of step 2's, `python3 scripts/check/infer.py binfer .`; `gate.actionlint` on `ci.yml`. Push the branch and fix what the runner finds that WSL did not.
  5. `scripts/check/semgrep.in`, `semgrep-requirements.txt`, `semgrep.cmake`, `gate.semgrep`; the `lint` job's installation. With no rule in `.semgrep/`, the gate must fail on a fixture: write `cf-no-raw-alloc.yaml` and its fixture with one `// ruleid:` line first and see `semgrep --test` fail (no rule matches it), then fill the rule and see it pass; the same order for each of the other five. Each fixture has at least two `ruleid:` cases and two `ok:` cases (a name that only contains the word, a call in a comment).
  6. Run `semgrep scan` on the tree: the three new rules list what they find; fix each in the code or give it `// nosemgrep: <id> -- <reason>` on its line. The three moved rules list nothing: if one lists a file, the regex of the gate and the rule differ, and the rule is wrong (stop if). `cmake --build` and `ctest -R gate.semgrep` pass.
  7. Remove `gate.detection` and `GateDetection.cmake`, and the first loop of `GateInterface.cmake`; for each, first make the violation it caught (add `#ifdef __GNUC__` to a file under `lib/numeric/src`; add `#include <stdio.h>` to `lib/base/include/configforge/cf_alloc.h`) and see `gate.semgrep` refuse it, then undo. Update `docs/Conventions.md` and the other mentions.
  8. `osv`'s lockfiles gain `--lockfile=scripts/check/semgrep-requirements.txt`; run the job on the branch.
  9. `docs/Dependencies.md` rows (below), `docs/plan/L-tools.md` group rows 3 and 5 ("Today": Infer, `gate.semgrep`; "Added": what is left), `CHANGELOG.md` (an item under `## 1.8.0`, `### Added`, after the include-what-you-use and lychee one: Infer and the project's semgrep rules, and that `gate.detection` is `gate.semgrep` now).
  10. ACCEPT.
* **dependencies rows** (`docs/Dependencies.md`, section 2b, after the `iwyu` row): `| Infer 1.3.0 (the release tarball, checked against its checksum by `setup.sh` and CI) | MIT | `EXTRA=infer` (tier 2) and the CI job `infer`: Pulse, buffer overrun and liveness over the libraries' sources (`scripts/check/infer.py`, `.infer-ignore.txt`); not linked |` and `| semgrep 1.186.0 (PyPI; CI installs it with every package's hash, `scripts/check/semgrep-requirements.txt`) | LGPL-2.1-only | `gate.semgrep`: the project's rules in `.semgrep/`, each with a fixture; no rule from the registry, no metrics |`.
* **accept**: ACCEPT-LINUX, ACCEPT-CI with `infer`, `lint`, `osv` green on the branch's last commit and `gate.actionlint` on `ci.yml`; `cmake -DEXTRA="infer" -DMODE=quick -DJOBS=3 -P scripts/verify.cmake` ending `all green`; `ctest --test-dir build -R gate.semgrep` passing with semgrep present, and the same command on a machine without it printing the skip line.
* **stop if**: any of the stop numbers above; Infer's `--compilation-database` refuses the clang-built database in a way no flag of 1.3.0 mends (write the message under "notes"); a `cf-detect-*` rule cannot be made to agree with the regex it replaces on every file of the tree (then `gate.detection` stays, the rule is not added, and the card says so); a fix for a finding would change a public signature or a limit (D-decisions are not reopened: record it as an `.infer-ignore.txt` entry with the reason and write the finding under "notes").
* **notes**: (the stop below was answered by the organiser's answer on 2026-11-23, see "Organiser's answers") STOPPED at step 2 (stop number: more than 25 distinct Infer findings), 2026-11-23. Infer 1.3.0 downloaded and its SHA-256 matched the card's digest; semgrep 1.186.0 installs from PyPI (venv, WSL). Done and pushed: `scripts/check/infer.py`, an empty `.infer-ignore.txt`, the `EXTRA=infer` step in `scripts/verify.cmake` (steps `infer configure`, `infer build`, `infer run`, `infer report`; also in the `TIER=2` list). Not done: `setup.sh`, everything from step 3 on, all of semgrep. First pass on a clang Debug build of `configforge` and `configforge_serialize` (the card's configure, checkers `--pulse --bufferoverrun --liveness`): 102 findings, all under `lib/*/src`; 61 distinct by (bug_type, file, procedure), 71 by (bug_type, file, line). By type: INTEGER_OVERFLOW_L2 63, BUFFER_OVERRUN_L2 31, BUFFER_OVERRUN_L3 4, NULLPTR_DEREFERENCE 3, INTEGER_OVERFLOW_L1 1; Pulse reported only the three nulls (`lib/config/src/bridge/flatten.c` 686 and 696 in `emit_group`, `e` from `cf_ks_at()`; `lib/config/src/core/define.c` 422 in `cf_define`, `d->key`), liveness nothing. The bulk is `--bufferoverrun`'s: (1) INTEGER_OVERFLOW_L2/L1 on unsigned subtraction and addition, which C defines (wrap), so a finding of the kind the code means; (2) BUFFER_OVERRUN_L2 `Offset: [-30, 30] Size: 21 by call to cf_value_to_text` (and `cf_u64_to_text`) at every caller: one cause (an offset of up to 30 against a size of 21), repeated in about 12 callers; (3) `toml_fail_named` offsets against a 256-byte buffer in `toml.c`. The card's own number is exceeded whichever way they are counted, so no entry or code fix was made. Options for the organiser (a design choice the card does not make): `--disable-issue-type INTEGER_OVERFLOW_L2 --disable-issue-type INTEGER_OVERFLOW_L1` (the flag exists in 1.3.0; it drops 64 of 102 and leaves 38, 35 of them one or two causes); or raise the stop number; or entries per procedure. The three Pulse nulls need reading before any decision (maybe real). The tree: branch `work/L-3`.
* **Organiser's answers**: 2026-11-23: the filter above (flag spelling `--disable-issue-type` confirmed in 1.3.0); the stop number counts causes after it. After it: 8 causes, no real fault. Fixed in the code, by giving Infer a bound it can see and changing no behaviour: `cf_u64_to_text`'s digit loop stops at the buffer's size (`lib/port/src/port.c`; it cleared every caller of `cf_value_to_text`, `cf_i64_to_text` and `ks_value_bytes`) and `toml_fail_named` clamps the name and the text up front (`lib/serialize/src/toml.c`). Five false positives have an entry with its reason in `.infer-ignore.txt`. 2026-11-23, osv: option (a) taken. `scripts/check/semgrep.in` names every package and takes `mcp` (latest 1.x, 1.30.0), the opentelemetry packages (1.45.2 and 0.66b2, with the two new packages `opentelemetry-exporter-http-transport` and `opentelemetry-exporter-otlp-common` and `pyjwt` that those need) and `wcmatch` (12.0.4) newer than semgrep pins; `pip-compile` cannot do this (its resolver refuses what semgrep's metadata forbids, even with `--pip-args=--no-deps`), so `scripts/check/semgrep-lock.py` (standard library; ruff checks it) writes `semgrep-requirements.txt` with each file's SHA-256 from PyPI. semgrep 1.186.0 runs with them: every fixture and the tree pass `gate.semgrep` in a venv installed from the file with `--require-hashes --no-deps`. `setup.sh` still pins `semgrep==1.186.0` and takes its dependencies from pip, as it does for the other tools. Step 3 seen: with the `port.c` fix removed 27 findings returned and `infer.py` exited 1; a made-up entry gave `stale entry` and exit 1. `gate.cmake.lint` is red on main itself (`scripts/generate/apiref.cmake:219`, 82 columns, from A-10): the local `verify` ends `all green` only with that gate and `gate.format.cmake` left out of the staged copy's run; CI's cmake-format decides. Semgrep, decided by the worker (tool behaviour, not design): `semgrep --test` takes one target and does not read a directory whose name starts with a dot, so `scripts/check/semgrep.cmake` tests each fixture with its rule alone; paths in `paths.exclude` are relative to the scanned directory (`lib`), so the exclusions are `**/tests/`, `**/lanes/`, `**/platform/`, `**/base/src/alloc.c`; the three regex rules use `languages: [generic]` with `paths.include` of `*.c` and `*.h`, because five public headers do not parse as C and semgrep skips an unparsed file for every rule; the call rules (AST) therefore do not read those five headers (they hold declarations). `pip-compile` of the pinned pip-tools has no `--python-version`: the file was made on Python 3.14, the runner's. First semgrep pass: the three new rules found nothing in the tree (0 matches), the three moved rules nothing. STATE at 2026-11-23 04:15 (before the osv answer above): steps 1 to 7 and 9 done on the branch; CI run 45090794846 (commit 2b of the branch, see git log): `infer` and `lint` (with `gate.semgrep`, `gate.ruff`, `gate.actionlint`) green, every other job green, `osv` RED: osv-scanner reads `scripts/check/semgrep-requirements.txt` (67 packages) and reports 8 known vulnerabilities in 3 packages semgrep 1.186.0 pins: `mcp` 1.29.0 (fixed 1.30.0; 5 advisories, 2 of them 7.5), `opentelemetry-api` 1.37.0 (fixed 1.45.1/1.45.2; 2), `wcmatch` 8.5.2 (fixed 11.2; 1). Options, none made by the card: (a) pin newer versions of those three in `semgrep.in` and compile again (`--no-deps` install lets the file differ from semgrep's own metadata; semgrep uses `mcp` only for its MCP server mode, but `wcmatch` and `opentelemetry-api` are used by a scan, so the gates must be rerun); (b) an `osv-scanner.toml` ignore with the reasons (semgrep's MCP server and telemetry are not run: `--metrics=off`); (c) drop the lockfile from `osv` and say so. Left: that choice, `osv` green, ACCEPT-CI on the last commit.

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

## L-6 - a map of the tree and a brief per card, so that workers read less

* **id**: L-6
* **goal**: `docs/Codemap.md`, generated from the sources and held by
  `gate.codemap`, gives every source file's functions and types with
  their line ranges and the large documents' headings with theirs; and
  `python scripts/work/brief.py <card id>` prints the brief a worker gets:
  CLAUDE.md's standard brief, the card, and exactly the sections it cites.
* **why**: docs/Maintaining.md, "Where the tokens go": workers spent ~18
  calls and ~18k tokens finding their way before the first edit, and the
  four planning documents were a quarter of everything they read. A map
  turns "read the file to find the function" into one `grep` and one
  ranged read; a brief turns "read the design document" into the 20 to 60
  lines the card needs.
* **needs**: L-5 (done)
* **size**: medium
* **decided**:
  * **The map** is `scripts/generate/codemap.cmake` (CMake, as
    `apiref.cmake`, so that it runs wherever the build does and in the
    deputy's sandbox), writing `docs/Codemap.md`; `-DOUT=<file>` writes
    elsewhere. Part 1, code: for each file of `lib/*/src/**.c`,
    `lib/*/src/**.h`, `lib/*/include/configforge/*.h`, `tools/**/*.c` and
    `tools/**/*.h` (not tests, not generated files), sorted by path, one
    line `` `path` `` then one line per definition in file order:
    `- name start-end` for a function definition (a line at column 0, not a
    comment or a preprocessor line, that contains `name(` and ends with
    `{`, the project's style, or whose declaration runs on over the next
    lines to a line ending in `{` before any `;`; its end is the next line
    that is `}` alone at column 0), `- struct name
    start-end`, `- enum name start-end`, `- typedef name line` for a
    typedef outside them, `- #define NAME line` only for a macro in a
    public header. Part 2, documents: for `docs/plan/*.md`,
    `docs/Roadmap.md`, `docs/Plan.md`,
    `docs/Maintaining.md`, `docs/Architecture.md` and
    `scripts/verify.cmake`, each `##` and `###` heading (for
    `verify.cmake`, each `if(x STREQUAL "<step>")`) as `- heading
    start-end`, where the end is the line before the next heading of the
    same or a higher level. A card heading (`## A-11 - ...`) is a heading
    like any other, so `grep -n "A-11" docs/Codemap.md` gives its range.
    No prose, no comments copied: names and numbers only.
  * **Size**: the generator fails when the result is over 64 KiB, with
    the five largest files' share; the card's first pass must fit.
  * **`gate.codemap`** (in `cmake/ConfigForgeGates.cmake`, beside
    `gate.apiref`): regenerates into the build tree and fails when it
    differs from `docs/Codemap.md`, with the first differing line.
    `scripts/format.cmake` and `scripts/prestage.cmake` rewrite it where
    they rewrite `docs/API.md`, so a line shift is regenerated with the
    format pass; like `docs/API.md` it is generated on Linux before a
    push (CLAUDE.md, brief step 6).
  * **`scripts/work/brief.py <card id> [--max-bytes N]`** (Python,
    standard library only, ruff-checked like the rest of
    `scripts/work`): finds the card by its heading `## <id> - ` in
    `docs/plan/*.md`; prints CLAUDE.md's fenced brief (the block under
    "The brief every card's subagent gets"), the card's text, and then,
    for each reference the card makes, the cited part: a `D-<n>` is its
    section of `docs/Decisions.md`; `` `<path>` "<heading>" `` or
    `<path>, "<heading>"` is that heading's section of that file; a
    ``` `<path>:<a>-<b>` ``` is those lines. Each cited part once, in the
    order cited, under a line `--- <path>, <heading or lines>`. A reference
    it cannot resolve is printed as `--- unresolved: <text>` (never
    silently dropped). Over `--max-bytes` (default 15000) it still prints
    everything and ends with `--- over budget: <n> bytes; split the card
    or cite less`, exit 0.
  * **Tests**: `work.brief` (a ctest test, run when Python 3 is found,
    skipped with a message otherwise) on a fixture card in
    `tests/work/data/` that cites one D-number, one heading and one line
    range and one unresolvable reference; it checks each part is printed
    once, the unresolved line, and the over-budget line with
    `--max-bytes 100`. `matrix`-style fixture CMake files are fenced from
    cmake-format (`# cmake-format: off` first, `on` last).
  * **CLAUDE.md**: the brief section says the organiser starts a card's
    worker with `brief.py`'s output; "Habits" (Tokens) names
    `docs/Codemap.md` as the first place to look. `scripts/README.md`
    lists both.
* **files**: `scripts/generate/codemap.cmake`, `docs/Codemap.md`,
  `cmake/ConfigForgeGates.cmake`, `scripts/format.cmake`,
  `scripts/prestage.cmake`, `scripts/work/brief.py`, `tests/work/`
  (CMakeLists.txt, data/), `tests/CMakeLists.txt`, `CLAUDE.md`,
  `scripts/README.md`; last: `docs/Maintaining.md` ("Where the tokens
  go": the tools exist), `CHANGELOG.md` is not touched (a maintainer's
  tool, not the library).
* **steps**:
  1. `git switch -c work/L-6`.
  2. `gate.codemap` first, with no generator: see it fail ("cannot be
     run"). Then `codemap.cmake`; see the gate pass; move a function in
     `lib/base/src/alloc.c` down one line in a scratch copy and see the
     gate fail naming the line; restore.
  3. Check the size against 64 KiB; if over, stop and report the
     largest files (the stop below).
  4. `brief.py` and `work.brief`: the test first against the fixture
     (see it fail: no script), then the script.
  5. Run `brief.py L-6` and `brief.py P-4` and read the output: every
     reference resolved or listed as unresolved. Fix the parser, not the
     cards; a card whose reference form the rule does not cover gets the
     form added to this card's "Organiser's answers".
  6. `format.cmake`, `prestage.cmake`, CLAUDE.md, `scripts/README.md`,
     `docs/Maintaining.md`.
  7. ACCEPT.
* **accept**: ACCEPT-LINUX, ACCEPT-CI; `ctest -R "gate.codemap|work.brief"`
  passing; `docs/Codemap.md` at most 64 KiB.
* **stop if**: the map is over 64 KiB with the rule above (report the
  share per directory; the organiser decides what to drop); a C construct
  in the tree that the column-0 rule reads wrongly in more than five
  places (list them).
* **Organiser's answers**: 2026-11-23: the first rule looked for `{` at
  column 0; the tree puts it at the end of the signature line (15 places
  in `alloc.c` alone), so the rule above is the corrected one.
  Second, 2026-11-23: the first map was 67,530 bytes (64 KiB + 1,994);
  `docs/Decisions.md` (6,732 of them) leaves part 2: a decision is found
  with `grep -n "^## D-<n>" docs/Decisions.md`, and `brief.py` resolves
  D-numbers itself.
  Third, 2026-11-23: L-6 and P-4 also use parenthesised heading
  citations, including `path` ("Heading": explanation). Treat these as
  heading citations. P-4's Roadmap citation "Large files" is unresolved
  because the Roadmap has no heading with that name.
  Third, 2026-11-23: part 2 keeps only `docs/Architecture.md` and
  `scripts/verify.cmake`: the cards, Roadmap and Maintaining change with
  every landing, and the map of them went stale on every open branch at
  once.
* **notes**: 2026-11-23: the first gate run failed with “cannot be run”
  (CTest exit 8). The initial generator exceeded the limit at 67,530
  bytes (generator exit 1, CTest exit 8). With the organiser's exclusion,
  the gate passes. In a scratch copy, moving `cf_std_alloc` down one
  line made the gate script exit 1 and report Codemap line 21
  (`21-24` became `22-25`). The deputy's Windows sandbox cannot run
  Python. The manager runs the missing-script check, `work.brief`, the
  L-6 and P-4 brief commands, and ACCEPT on Linux and CI.

## L-7 - CI in minutes, with nothing taken away

* **id**: L-7
* **goal**: a push to a work branch gets CI's verdict in about 7 minutes
  instead of 19, with every check still run in full on `main` and nightly.
* **why**: measured 2026-11-23 (run 45240013488): wall 19 minutes, the sum
  of 47 jobs 120; `msan` 17.1 and `afl` 16.5 minutes set the wall, the
  next is `matrix (windows-latest)` at 6.8. Superseded runs of an amended
  branch ran to the end. Most red runs of the night were not faults but
  style and generated files (format, `docs/API.md`'s lines, a fixture,
  cmake-lint), which a local gate run catches before the push.
* **needs**: nothing
* **size**: small
* **decided**:
  * **`concurrency`** at the top of `.github/workflows/ci.yml`: `group:
    ci-${{ github.ref }}`, `cancel-in-progress: ${{ github.ref !=
    'refs/heads/main' }}` (a run on `main` is never cancelled).
  * **Documentation-only pushes** skip `ci.yml`: `paths-ignore` on `push`
    and `pull_request` for `docs/**`, `**/*.md`, `.lychee.toml`, except
    files CI reads: `docs/sizes.txt`, `docs/targets.txt`, `docs/API.md`,
    `docs/Codemap.md`, `docs/Limits.md` and `README.md` (a generated
    page's gate must run when it changes). The `docs` workflow keeps
    checking the documents.
  * **`afl` and `libfuzzer`**: `FUZZ_SECONDS` is 60 on `main`, on a
    schedule (a nightly `schedule:` added to `ci.yml`, 03:00 UTC) and on
    a manual run; 15 on any other branch (`${{ github.ref ==
    'refs/heads/main' && 60 || 15 }}`-style expression, written once as a
    job-level `env`).
  * **`msan`**: first measure (step 1) which step takes the time. If its
    tests are the time, split them into two jobs by ctest label or by
    `-I` ranges, each under 9 minutes; if the build is, cache the build
    directory's third-party part with `actions/cache` (pinned by commit,
    as the other actions are). No test is dropped.
  * **The local gate before a push**: CLAUDE.md's brief, step 6, gains
    "run every `gate.*` test on Linux before the push
    (`scripts/work/codex-card.sh` does it for Codex's cards)".
* **files**: `.github/workflows/ci.yml`, `CLAUDE.md`; last:
  `docs/Maintaining.md` ("Working habits that paid": the new times).
* **steps**: 1. `git switch -c work/L-7`; read `msan`'s step times from
  the run above (`gh run view --log` of that job, the `##[group]` lines
  with their timestamps) and write them under "notes". 2. `concurrency`
  and `paths-ignore`; `gate.actionlint` passes. 3. The fuzz seconds and
  the nightly schedule. 4. `msan` as decided by step 1. 5. Push twice in a
  row (a second commit a minute after the first): see the first run
  cancelled; then a docs-only commit: see no `ci` run. 6. ACCEPT: the
  branch's run finishes in under 9 minutes of wall time, all green.
* **accept**: ACCEPT-CI, the wall time in "notes"; `gate.actionlint`.
* **stop if**: `msan` cannot be brought under 9 minutes without dropping
  a test (write the numbers, keep it whole, and land the rest).
* **Organiser's answers**: 2026-11-23: `paths-ignore` takes no exceptions,
  so the filter is `paths` with ordered patterns, on `push` only: `'**'`,
  then `'!docs/**'`, `'!**/*.md'`, `'!.lychee.toml'`, then again
  `'docs/sizes.txt'`, `'docs/targets.txt'`, `'docs/API.md'`,
  `'docs/Codemap.md'`, `'docs/Limits.md'`, `'README.md'`. `pull_request`
  stays unfiltered, so a pull request's required checks always report.
* **notes**: step 1, by the organiser (run 45240013488): `msan`'s build 0.7
  minutes, its `setarch -R ctest --preset msan` 16.1 minutes: the tests are
  the time, so `msan` is split by tests (decided, second case). `afl`'s one
  step (verify's `EXTRA=afl`, `FUZZ_SECONDS=60`) 16.3 minutes.
  Result, run 45257081918 (the branch, all green): wall 13.9 minutes
  (from 19.2); `msan (2)` 13.7, `clang` 6.8, `bench-gate` 6.0, `matrix
  (windows-latest)` 5.8, `afl` 5.7. The stop: one test, `doctor.api`,
  takes 615 s under MemorySanitizer alone (the next is 35 s), so no split
  of the tests brings `msan` under 9 minutes; it stays as split, and
  `doctor.api`'s cost under MSan is the next item (as L-2 shrank the
  live-reload counts under Valgrind).
