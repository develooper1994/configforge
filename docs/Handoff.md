# Handoff

What an agent working here has learnt that the code, the decisions and
the roadmap do not say: how the owner wants the work done, where the
work stands, and what each machine it ran on needs. Read after
`AGENTS.md`. Keep it true: update it in the commit that changes what it
says, as `docs/Roadmap.md` is.

## How the owner wants the work done

* **Autonomy.** The owner is often away and wants the work to go on:
  verified units may be committed, pushed to `main` and released by
  tag without asking. Ask only what truly needs them (a sign-in, sudo,
  a decision about their accounts), once and early. Never type a
  password, never read stored credentials, never delete their data.
* **Report in Turkish.** Summaries and answers to the owner are in
  Turkish; code, commits and repository documents stay English.
* **The bar** (2026-10-29): SOLID, state of the art, each language with
  its own tools and packaging (clippy and rustfmt, `pyproject.toml`,
  `py.typed`); generate ideas, but only ones genuinely useful; watch
  the machine's resources.
* **1.0.0 only when everything is done and re-checked.**
* **CI is blocked by GitHub billing.** The owner's rule: if what CI does
  passes locally, count CI as passed for now. The release workflow
  cannot run either, so a tag is not yet a published release.
* **"Authorization"** (yetkilendirme) in the owner's words is the Policy
  section of the original idea document (outside the repository,
  `ProjectIdeas/ideas/library/ConfigForge.md`, sections 8, 22 and 46).
  The read side is D-33, the write side D-42 (`cf_write`). Still open:
  automatic propagation to several targets (sections 23-24) and a
  policy file (section 12).

## Where the work stands (2026-10-30)

0.4.0 is on `main`, unreleased. The last round, each unit verified on
Linux (five presets) and Windows (quick) and pushed:

| Commit | What |
| ------ | ---- |
| 060db56 | C++ `cf::Key`, `cfg["a"]["b"]`, `cf::Binding` (D-46) |
| 0fe5b45 | one C++ header for every standard, 98 to 23 (D-47) |
| 4e2426d | `cf_live`: manual, approved and automatic reloads, `CF_RESTART` (D-48) |
| 41346cc | Rust typed keys, `settings!`, `Live`; the Python package over ctypes (D-49) |
| 7dacffc | Doctor as a reload's gate; a Kubernetes ConfigMap update test |
| 12becfa | `cf_live_status_get`, for a health page, in every language |

**Next, in order:**

1. The full verify with every `EXTRA` step
   (`kqueue;tidy;clang;tsan;yaml;compilers;fuzz;coverage;shared`). The
   last run stopped at the clang build on `test_report_edges.c`, fixed
   in the commit that added this file; the steps after `clang` have not
   been run on the current `main`.
2. Tag v0.4.0 (`AGENTS.md`, "A release"), under the owner's CI rule,
   then bump to 0.5.0.
3. The ideas in `docs/Roadmap.md`, most useful first: a reload diff as
   JSON, a settle interval for bursts of writes, a wheel and a crate
   that carry the library, Go and .NET over the same ABI.

## Recursive self-improvement

The owner's working principle (2026-10-30): the way the work is done
improves itself, every round, not only the code. After each unit:

1. **Look back.** What failed, what was redone, what cost time or
   resources - a check that passed when it should not have, a mistake
   made twice, a tool that fought back.
2. **Make it impossible, not remembered.** Prefer, in this order: a
   gate or a test that fails on it; a script that does the step right;
   a rule in `AGENTS.md` ("Things that have bitten") or here. A lesson
   that lives only in a session is lost when the session ends.
3. **Apply the improvement to the improving.** When a lesson keeps
   recurring despite a rule, the rule is the wrong kind: promote it one
   step up the list above.
4. **Record it** in the same commit, so the next session - on any
   machine - starts from the improved process.

The first round, from the session that wrote this file:

| Found | Made into |
| ----- | --------- |
| heredocs lost backslashes five times | a rule (below, and `AGENTS.md`); next step up: write files only through the editor tool, never a heredoc, for C and Rust sources |
| the wrapper never compiled above C++98, unseen for years | `gate.headers` compiles it under every standard |
| a thread test hung the single-threaded preset | `AGENTS.md`; the test skips under `CF_SINGLE_THREADED` |
| the full `EXTRA` verify ran only at the end of a round, and found a clang error late | next: run `EXTRA=clang` with every unit that touches tests, not only at the end |
| C: filled from WSL traces, twice | one script per unit check; the owner's cleanup noted above |

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
  backslashes again and again in this session (`'\0'`, `"\n"`, a line
  continuation). Check the result byte for byte.

## The machines

### The owner's Windows workstation (where this round ran)

* **C: fills.** WSLg writes traces to
  `%TEMP%\DiagOutputDir\RdClientAutoTrace\*.etl` - 2 GB in a day under
  load, and C: filled twice mid-write. Only the owner can delete them
  (the agent's deletion was refused). WSL's own `ext4.vhdx` on C: only
  grows; compacting it needs the owner (`fstrim -av`, `wsl --shutdown`,
  diskpart `compact vdisk`).
* **Build on D:.** The WSL distribution `configforge-dev` lives at
  `D:\cf-wsl` (`scripts/buildenv/wsl-distro.ps1`), root, with every
  tool `setup.sh` lists plus Rust 1.75, clippy and rustfmt from Ubuntu's
  archive. Linux verify:
  `wsl -d configforge-dev -- cmake -DSOURCE=index -DJOBS=4 -DWORK=/root/cfv -P scripts/verify.cmake`
  run from the checkout under `/mnt/c/...`; `CARGO_TARGET_DIR=/root/rt`.
* **Windows verify** with temporary files on D::
  `$env:TEMP=$env:TMP='D:\cf-tmp'`, `-DWORK=D:/cf-work -DMODE=quick`.
* **Never run the two verifies at once**: both run `git write-tree`.
* The old `Ubuntu` distribution holds the owner's `~/vcpkg`: never
  write there, never build there.
* The owner once gave the WSL sudo password in chat: never type it; ask
  them to run sudo.
* `D:\cf-handoff` holds this machine's helper scripts (one-call unit
  checks, negative proofs). They name this machine's paths and are not
  needed elsewhere: `scripts/verify.cmake` is the interface.

### Another machine

Nothing above is needed to continue. Clone, then follow `AGENTS.md`:
`scripts/buildenv/setup.sh` (or its `Containerfile`) installs every tool
the build, the gates and the `EXTRA` steps use; add `rustc cargo
rust-clippy rustfmt` for the Rust checks and `-DCF_RUST=ON`. On another
Windows machine, `wsl-distro.ps1` makes the same distribution on the
drive you name - pick one with room.
