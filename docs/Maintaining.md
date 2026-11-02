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
  `scripts/release-local.sh` and `.ps1` remain the way to release when
  Actions cannot run.
* **Commits and files carry their maintainers' names only.**
* **"Authorization"** in the maintainer's words is the Policy section of
  the original idea document (outside the repository,
  `ProjectIdeas/ideas/library/ConfigForge.md`, sections 8, 22 and 46).
  The read side is D-33, the write side D-42 (`cf_write`). Still open:
  automatic propagation to several targets (sections 23-24) and a
  policy file (section 12).

## Recursive self-improvement

The working principle (2026-10-30): the way the work is done
improves itself, every round, not only the code. After each unit:

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
