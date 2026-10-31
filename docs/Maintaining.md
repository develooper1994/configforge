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
  resources watched.
* **1.0.0 only when everything is done and checked again.** The plan is
  `docs/Roadmap.md`, "Next: 1.0.0".
* **GitHub Actions is unavailable for now** (billing). Every job it runs
  is run locally instead - `scripts/verify.cmake` by tier (D-50),
  `scripts/release-local.sh` and `.ps1` for a release - and counts as CI
  until it returns.
* **The repository reads as its maintainers wrote it**: no generated
  attribution in commits or files. Tools that help with the work read
  `CONTRIBUTING.md` like anyone else.
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

## Working habits that paid

* **Negative proofs on MSVC** must disable a check with a condition
  false at run time (`x == -77777 && ...`), never a constant: `/WX`
  turns C4127 into a build failure, the old binary stays, and the test
  "passes". Treat any build error as "proof not run".
* **A new build type or toolchain is worth one real run**: `-O3` Alpine,
  MSVC Release and FreeBSD clang each found a bug no preset could.
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
  tool `setup.sh` lists plus Rust 1.75, clippy and rustfmt from Ubuntu's
  archive. Linux verify:
  `wsl -d configforge-dev -- cmake -DSOURCE=index -DJOBS=4 -DWORK=/root/cfv -P scripts/verify.cmake`
  run from the checkout under `/mnt/c/...`; `CARGO_TARGET_DIR=/root/rt`.
* **Windows verify** with temporary files on D::
  `$env:TEMP=$env:TMP='D:\cf-tmp'`, `-DWORK=D:/cf-work -DMODE=quick`.
* **The Linux and Windows verifies can run at once** since D-50 (each
  writes its tree from a private copy of the index); before that, the
  second failed on `index.lock`.
* The old `Ubuntu` distribution holds a `~/vcpkg` checkout that is not this project's: never
  write there, never build there.
* The old `Ubuntu` distribution's disk is the large one on C: (over
  300 GB of virtual size). Moving it to D:
  (`wsl --manage Ubuntu --move D:\wsl\Ubuntu`, WSL 2.3 or later) frees
  C: without losing anything in it.
* `D:\cf-handoff` holds this machine's helper scripts (one-call unit
  checks, negative proofs). They name this machine's paths and are not
  needed elsewhere: `scripts/verify.cmake` is the interface.

### Another machine

Nothing above is needed to continue. Clone, then follow `CONTRIBUTING.md`:
`scripts/buildenv/setup.sh` (or its `Containerfile`) installs every tool
the build, the gates and the `EXTRA` steps use; add `rustc cargo
rust-clippy rustfmt` for the Rust checks and `-DCF_RUST=ON`. On another
Windows machine, `wsl-distro.ps1` makes the same distribution on the
drive you name - pick one with room.
