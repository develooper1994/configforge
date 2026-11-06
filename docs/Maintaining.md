# Maintaining

What the code, the decisions and the roadmap do not say: how the work
is done here and what each machine it has run on needs. Read after
`CONTRIBUTING.md`, and keep it true in the commit that changes what it
says, as `docs/Roadmap.md` is.

## How the work is done

* **Verified units go to `main`**, and a release is a tag
  (`CONTRIBUTING.md`, "A release"). Code, commits and documents are in
  English.
* **The bar** (2026-10-29): SOLID, state of the art, each language with
  its own tools and packaging (clippy and rustfmt, `pyproject.toml`,
  `py.typed`); ideas only where they are genuinely useful; the machine's
  resources watched. A maintained library behind a `CF_WITH_*` option
  rather than one written here (2026-11-01), the default build still
  needing none.
* **Complete the agreed work before publishing.** 1.0.0 was released
  on 2026-11-01 with known defects and unfinished work; that was not
  completion. Deferred work stays open, and the next release waits for
  the agreed scope and its checks. A fix is a patch and an addition a
  minor version; the plan is `docs/Roadmap.md`, "Next: 1.x".
* **GitHub Actions runs again** (2026-11-01; billing blocked it through
  2026-10): the v1.0.0 tag was built and published by the release
  workflow, macOS included. Every job is still run locally before a
  push - `scripts/verify.cmake` by tier (D-50) - and
  `scripts/release/release_local.sh` and `.ps1` remain the way to release when
  Actions cannot run.
* **Commits and files carry their maintainers' names only.**
* **"Authorization"** means the Policy section of
  the original idea document (outside the repository,
  `ProjectIdeas/ideas/library/ConfigForge.md`, sections 8, 22 and 46).
  The read side is D-33, the write side D-42 (`cf_write`). Still open:
  automatic propagation to several targets (sections 23-24) and a
  policy file (section 12).

## Lessons made into checks

Since 2026-10-30 the way the work is done is improved with the code,
every round. After each verified commit:

1. **Look back.** What failed, what was redone, what cost time or
   resources - a check that passed when it should not have, a mistake
   made twice, a tool that fought back.
2. **Make it impossible, not remembered.** Prefer, in this order: a
   gate or a test that fails on it; a script that does the step right;
   a rule in `CONTRIBUTING.md` ("Things that have bitten") or here. A lesson
   that lives only in someone's head is lost when they move on.
3. **Apply the improvement to the improving.** When a lesson keeps
   recurring despite a rule, the rule is the wrong kind: promote it one
   step up the list above.
4. **Record it** in the same commit, so whoever works next - on any
   machine - starts from the improved process.

The first round:

| Found | Made into |
| ----- | --------- |
| heredocs lost backslashes, more than five times, and a NUL byte reached C source twice | the rule in `CONTRIBUTING.md`; then, promoted when the rule kept being broken, `gate.text`: a NUL byte or a carriage return in a tracked text file fails the build, naming the file and line. It also found `docs/API.md` and the symbol list written CRLF on Windows, now LF |
| the wrapper never compiled above C++98, unseen for years | `gate.headers` compiles it under every standard |
| a thread test hung the single-threaded preset | `CONTRIBUTING.md`; the test skips under `CF_SINGLE_THREADED` |
| the full `EXTRA` verify ran only at the end of a round, and found a clang error late | next: run `EXTRA=clang` with every unit that touches tests, not only at the end |
| C: filled from WSL traces, twice | one script per unit check; the cleanup noted below |
| a script that restaged formatted files also pulled the next unit's edits into the index | the verify scripts never restage; format before staging, and `gate.format` refuses what was not; a unit's version of a shared file is built into the index with `git hash-object` and `update-index` |
| `cmake -DSRC=. -P ...` run from PowerShell lost its value, and `gate.abi` wrote the symbol list to the root of the drive, twice | the gate refuses a `SRC` that is not the repository; `-D` scripts run from a POSIX shell |
| `cf_live`'s lock livelocked, and tcc builds had no atomics while claiming thread safety | a ticket lock; `cf_port.h` refuses a threaded build without atomics; `CONTRIBUTING.md`. and one step up, a per-test timeout in `verify` (`TEST_TIMEOUT`, 900 s), so the next hang fails in minutes, not hours |
| a full Windows verify stopped every test of MSVC's sanitize preset at a dialog for a missing DLL | `ConfigForgeAnalysis.cmake` copies AddressSanitizer's runtime beside each program; the note on the Windows verify below |

The round of 2026-11-05 (probation, Doctor on a pull request, the
fingerprint):

| Found | Made into |
| ----- | --------- |
| two mutations "survived" because the mutant did not compile (`-Werror` on a variable left unused) and the old binary ran | the habit below, made concrete: a mutation script stops with `NO BUILD` when the build fails, and the mutation uses a condition false at run time |
| a mutation survived because the test checked a value at the one point where the broken code happened to produce it too | check a value at a step where a wrong implementation would give a different one: after a sequence, not at its start |
| GCC's analyzer (`-fanalyzer`) found two things only in tier 1's analyze build, an hour into the round | build new C with `cmake --preset analyze` before staging it, not only the default preset |
| `docs/API.md` links each function to its line, so every C edit after regenerating it failed `gate.apiref` | regenerate the reference last, after the final edit to any header or source, then stage |
| clang-format 22 (Windows) and 18 (WSL) wrapped a call's arguments differently; `gate.format.c` failed on one system only | first a rule (write lines neither wraps); then, when `configforge.hpp` showed layouts no line satisfies both ways, promoted: one major version, 22, is the only one the gate or `format.cmake` uses (`ConfigForgeClangFormat.cmake`), pinned from PyPI by `setup.sh` and CI |
| a patch script in Python turned `"\n"` in C source into a line break | patch scripts use raw strings (`r"""..."""`) for C, and the build is the check |
| Rust's `Debug` for `Config` printed `cf_digest`, a hash of the secrets themselves, while its comment said it kept secrets out of logs | `cf_fingerprint`, and a test that `Debug` contains no digest |

The round of 2026-11-06 (each reader against its standard's suite):

| Found | Made into |
| ----- | --------- |
| readers were tested against cases written here, and JSON and TOML accepted what their standards forbid | `verify -DEXTRA=conformance`: each standard's own suite, values compared as well as verdicts, against floors that only rise (`tests/conformance/floor.txt`) |
| a new YAML parser passed the whole suite, then hung on 149 random bytes | a dependency is fuzzed before it is trusted; the reproducer is kept (`tests/conformance/data/`) and read under a timeout by test_yaml |
| a minimum version written only in documents does nothing: a program built against libfyaml 0.9.6 loads 0.8, which has the same soname | a version floor is enforced at every entry point from one value: the build, the run time (ask the loaded library), and the package file (`Requires.private`) |
| `dev_verify.sh` piped verify into `tail`, so its exit status was always `tail`'s | it writes the log to a file and exits with verify's status; read "all green", never a pipe's status |
| CI's clang-tidy job failed for four commits, on an assignment in an `if` that tier 1 does not lint, and nobody read the result | after a push, the CI run is read before the next unit starts (`gh run list`); a unit that adds C runs `EXTRA=tidy` with tier 1 |
| a defect found in a dependency | reproduced on its latest release and master, searched for in its tracker, then reported with the reproducer and the fix (libfyaml issues 101, known; 506, filed) |
| a `b != NULL ?` test in a new branch made MSVC's analyzer (C6011) read the rest of the function as dereferencing a NULL builder, and CI's Windows job failed after a green Linux tier 1 | a commit with C runs the Windows quick verify too before it is pushed; a guard returns early rather than testing inside an expression |
| a file written into the tree while the stage-and-verify script ran was staged half-formatted, twice | nothing is written into the tree from the staging to the end of the copy; new work goes to a scratch directory meanwhile |
| YAML went on by default, and incremental verify runs kept their CMake caches, which kept `CF_WITH_YAML=OFF`: tier 1 passed without building the new default, and a stack-use-after-scope in the alias replay (`yaml.c`) waited for a sanitised build made by hand | `verify.cmake` starts from nothing when the options, the presets or `cf_build_options.h` change |
| the header gate's new check expected `#pragma GCC warning` to fail a `-Werror` compile, which GCC does and clang does not; tier 1 ran it with GCC only, and CI's clang and macOS jobs failed | a gate that reads a compiler's output looks for the message, not the exit status; a change to a gate runs `EXTRA=clang` too |
| a new parser was developed against the suite's own event streams (`test.event`) with a scratch harness, apart from the library: 256 of 308 at the first run, 308 of 308 and 94 of 94 after nine fixes, in an hour | a reader is first held to the suite's events, where a difference names the construct, then to the values through the library |
| staging with `git add -A` took in an editor's local settings file, and the wording check passed it, since the file's words were clean | the wording check fails a tracked editor-settings path whatever it holds, and such directories are excluded locally (`.git/info/exclude`) |
| a request read one way ("modern C++ only") was built as a deprecation before it was confirmed; it meant "modern C++ as well" (D-71, replaced by D-72 before a release) | a change that takes something away from users is stated back in one line before it is built, and kept unreleased until it is answered |
| finished work was deleted from the Roadmap as it landed, so what was done could be seen only in the CHANGELOG, apart from what was not | every item keeps a mark (idea, chosen, under way, done, released) and the Roadmap opens with all of them in one table |
| the release's Windows job built Doctor from a list of targets; `doctor.gen`, added in 1.3.0, was not on it, its test did not run, and the release failed on its tag - the tag could not move, so 1.3.0 was never published | the job builds the whole tree, and the release workflow runs as a dry run - every artifact built and tested, nothing published - on each push to main that changes what it builds |

## Measured

What a build option costs or saves, measured rather than assumed, so a
default can be argued from a number.

| Option | Where | Effect |
| ------ | ----- | ------ |
| `CF_GC_SECTIONS` | minimal preset, GCC 13, x86-64, 2026-11-05 | `tier0` text 105,689 -> 70,655 bytes (-33%); `embedded` 106,839 -> 72,133 (-32%); 107 of 107 tests pass. On in the minimal preset |
| defining keys, hash and `qsort` | `bench/`, GCC 13 Release, 2026-11-06 | 10,000 definitions 1,148 -> 16 ms; 20,000: 3,960 -> 38 ms. Was a scan per definition and an insertion sort |
| Doctor key index | the same | `doctor diff` of two 10,000-key reports 1,740 -> 553 ms; 20,000: 6,778 -> 1,335 ms. What is left, about 60 us a key, is reading the report: linear, and left until something needs it faster |
| `cf_get_int`, a report | the same | 140 to 200 ns a read at 100 to 20,000 keys (a binary search); a report about 3 us a key. Nothing to do |
| `CF_GC_SECTIONS` + `CF_LTO` | the same | `tier0` 60,656 (-43%); `embedded` 56,373 (-47%), its bss 22,592 -> 4,128; tests pass; MSVC Release with both: 140 of 140. Off by default: the stack budget cannot measure inlined frames. Speed: the benchmark (Roadmap, plan item 3) is the place to measure it |
| a file's first buffer from the size `fseek`/`ftell` report, the chunked loop kept (D-76) | scratch harness, GCC 15 -O2, WSL, 2026-11-06, base64 files | 1.4 KB: 0.010 -> 0.005 ms, arena 4,096 -> 1,392; 138 KB: 1.03 -> 0.26 ms, 520,192 -> 138,336; 1.4 MB: 8.98 -> 2.65 ms, 4.19 -> 1.38 MB; 13.8 MB: 73.5 -> 27.9 ms, 33.6 -> 13.8 MB. A real pipe falls back to the chunks: 520,192, as before. Kept |
| the key space sized from the document: the lines that start like a key (not blank, not `# ; ! /`, not `[ ] { }`) counted, the array sized once (D-76) | `bench/` (`cf_bench files`), GCC 15 -O2, WSL, 2026-11-07, INI, median of 9 runs, three rounds, bytes asked of the allocator for one load | 1,000 keys (18 KB): 0.42 -> 0.36 ms (-14%), 331 -> 239 KB asked (-28%); 10,000 (200 KB): 4.6 -> 3.9 ms (-15%), 4.56 -> 2.37 MB (-48%); 100,000 (2.2 MB): 59 -> 54 ms (-9%), 39.5 -> 23.9 MB (-39%); 100,000 with 107-byte lines (10.7 MB): 81 -> 84 ms (+4%: the count of 10 MB costs what the growth saved), 56.2 -> 40.6 MB (-28%); 100 keys flat. Tried first and dropped: bytes / 32 (-14% memory on the 2.2 MB file, +12% on the 10.7 MB one) and every line counted (350 KB asked for a 200 KB file of one key, `test_bytes_are_transient`). A heap arena only, 2^20 candidates at most, a minified document gets the doubling as before. Kept, for the memory more than the time |
| the key space sized, on a reload, from the generation before (D-76) | the same (`cf_bench reload`), INI and 1-line JSON | 1,000 keys: 0.50 -> 0.48 ms, 1-line 0.57 -> 0.48 (-16%); 10,000: 5.05 -> 4.35 (-14%), 1-line 5.10 -> 4.35 (-15%); 100,000: 60.3 -> 55.5 (-8%), 1-line 58.4 -> 53.2 (-9%). Alone it does as well as with the line count (the count is skipped work once the size is known, and still runs). Kept: the only guess that is exact |
| the definitions' table and index sized once (D-76) | the same (`cf_bench define`), the table at 131,072 and the index at 524,288 from the start, an upper bound on what a hint could give | 1,000 definitions 1.3-1.5 -> 7.4-9.1 ms (a 4 MB zeroed index); 10,000: 18.4-19.8 -> 17.9-23.0 (flat); 100,000: 198-236 -> 180-212 (-9%, inside the run to run spread of 15%). The doubling and rehash are not where the time goes (copying a definition, checking it, the seal's `qsort` are). Not kept: nothing to hand the caller that pays |
| the emitter's output sized from the key space, one pass (D-76) | the same (`cf_bench emit`), INI | measuring call / writing call: 100 keys 0.034 / 0.040 ms; 1,000: 0.59 / 0.58; 10,000: 9.2 / 9.4; 100,000: 94 / 96. `cf_emit` writes into the caller's buffer and allocates nothing, so there is no pass in the library to drop: the two are the caller's, measure then write, and each costs the same (the filter's definition look-up per entry and the encoder, not the sizing). A caller with the last size (a reload, a save) writes once into a buffer a fifth larger and the return value says when it was too small. Not kept: a predictor would walk the same entries with the same filter for little under a pass, and would be a new function on the C, C++ and Rust surfaces |
| branch hints (`CF_LIKELY` / `CF_UNLIKELY`, already in `cf_port.h`, empty where the compiler has none) on the key space's push and `cf_key_valid`, and the flattener's scalar | the same (`cf_bench files`), hints on against off | 1,000 keys 0.345 -> 0.341 ms; 10,000: 3.92 -> 3.89; 100,000: 54.0 -> 53.3 (-0.3% to -1.3%, with 3% between runs of one binary). GCC already takes an early `return CF_FALSE` for the cold side. Not kept, and not added to the readers |

## Working habits that paid

* **Negative proofs on MSVC** must disable a check with a condition
  false at run time (`x == -77777 && ...`), never a constant: `/WX`
  turns C4127 into a build failure, the old binary stays, and the test
  "passes". Treat any build error as "proof not run".
* **Undo a mutation by writing the file, or touch it after a copy**:
  `Copy-Item` and `cp -p` keep the saved file's older time, the build
  takes the mutant's objects as current, and the restored code "fails".
* **A new build type or toolchain is worth one real run**: `-O3` Alpine,
  MSVC Release and FreeBSD clang each found a bug no preset could.
* **A native Clang pass is not clang-tidy.** CI runs both. A broad native
  addition also needs `EXTRA=tidy` and `EXTRA=compilers`, not only the
  tier-1 compiler run. The list unit passed Clang but missed a
  multilevel-pointer byte copy in tidy and a PCC preprocessing defect.
  PCC 1.2 moved a `?` near its input-buffer boundary to the start of an
  included header; `pcc -E` proved the corruption, and an equivalent
  `if` in response-file argument growth avoids it. Inspect preprocessed
  output before treating a reported header line as the faulty source.
* **A guess is tried on the worst document, not the typical one.** Bytes
  divided by a typical line looked right on one file and was off by a
  factor of 5 on the next; every line counted reserved more than the
  file held for a file of comments. `cf_bench` writes both kinds, and a
  guess that wastes memory fails a test (`test_bytes_are_transient`)
  before it is measured at all. On a shared machine the median of a run
  moves 30%; read the minimum and the bytes asked of the allocator.
* **Batch the checks of a unit into one script per shell call** - fewer
  calls, and on Windows each `wsl` call costs disk (below).
* **Write source through a file, not a heredoc**: heredocs lost
  backslashes again and again here (`'\0'`, `"\n"`, a line
  continuation). Check the result byte for byte.

## The machines

### The maintainer's Windows workstation

* **C: fills.** WSLg writes traces to
  `%TEMP%\DiagOutputDir\RdClientAutoTrace\*.etl` - 2 GB in a day under
  load, and C: filled twice mid-write. Only the machine's user can delete them
  (they are outside the repository and not the build's). WSL's own `ext4.vhdx` on C: only
  grows; compacting it needs an administrator (`fstrim -av`, `wsl --shutdown`,
  diskpart `compact vdisk`).
* **Build on D:.** The WSL distribution `configforge-dev` lives at
  `D:\cf-wsl` (`scripts/buildenv/wsl-distro.ps1`), root, with every
  tool `setup.sh` lists, Rust among them (`dev-setup.ps1` makes or
  updates it). Linux verify:
  `wsl -d configforge-dev -- cmake -DSOURCE=index -DJOBS=4 -DWORK=/root/cfv -P scripts/verify.cmake`
  run from the checkout under `/mnt/c/...`; `CARGO_TARGET_DIR=/root/rt`.
  With `$` variables in the command use `wsl -d configforge-dev -e bash -lc '...'`:
  `wsl --` hands the line to a shell that expands them first, to nothing.
* **Windows verify** with temporary files on D::
  `$env:TEMP=$env:TMP='D:\cf-tmp'`, `-DWORK=D:/cf-work -DMODE=quick`.
* **MSVC's sanitize preset** links AddressSanitizer's runtime DLL, which
  sits beside `cl.exe` and on no PATH. A program started without it
  does not fail: Windows opens a dialog per test and waits for OK.
  `cmake/ConfigForgeAnalysis.cmake` copies the DLL beside every
  program, so a full Windows verify runs unattended; the quick run is
  still the per-commit one.
* **The Linux and Windows verifies can run at once** since D-50 (each
  writes its tree from a private copy of the index); before that, the
  second failed on `index.lock`.
* The old `Ubuntu` distribution holds a `~/vcpkg` checkout that is not this project's: never
  write there, never build there.
* The old `Ubuntu` distribution's disk is the large one on C: (over
  300 GB of virtual size). Moving it to D:
  (`wsl --manage Ubuntu --move D:\wsl\Ubuntu`, WSL 2.3 or later) frees
  C: without losing anything in it.
* `D:\cf-tools` holds this machine's helper scripts (one-call unit
  checks, negative proofs). They name this machine's paths and are not
  needed elsewhere: `scripts/verify.cmake` is the interface.

### Another machine

Nothing above is needed to continue. Clone, then run
`scripts/buildenv/dev-setup.sh` (Linux, macOS) or `dev-setup.ps1 -WslDir
<a drive with room>` (Windows): it installs every tool the build, the
gates and the `EXTRA` steps use, the Rust toolchain included
(`-DCF_RUST=ON` turns its checks on), and `--check` says what is there.
Then follow `CONTRIBUTING.md`.
