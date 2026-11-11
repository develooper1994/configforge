# Handoff: continue here (written 2026-11-11, 16:50)

This file is committed on purpose, so the work can continue on another
machine. Read "START HERE" and do its steps in order. Do one step, test
it, commit it, then take the next. If a step is unclear, stop and ask
the owner; do not guess.

## START HERE

State: `main` holds the finished security pass (commits `4af2b33`,
`4c1ae97`, `ea60e72`, `6c528fa`; CI was green on `6c528fa`). The commit
after them changes CI's runner label and adds this file; its CI run has
NOT been read.

Step 1 - read CI for the newest commit on `main`:
  `gh run list --branch main --limit 3`
  The owner asked for `ubuntu-latest` in place of `ubuntu-26.04` in the
  workflows. If jobs now fail while installing packages or tools (the
  workflows were written for Ubuntu 26.04: `libclang-rt-21-dev`,
  libfyaml 0.9, clang-tidy 21), `ubuntu-latest` is an older Ubuntu.
  Tell the owner exactly which job and which package failed and ask
  whether to go back to `ubuntu-26.04`. Do not "fix" it by removing
  checks. `docs/Decisions.md` D-65 records why 26.04 was chosen; if
  `ubuntu-latest` stays, add a sentence there and in `CHANGELOG.md`.

Step 2 - check the CodeQL alerts closed:
  `gh api 'repos/{owner}/{repo}/code-scanning/alerts?state=open' --jq length`
  Expected: 0 (there were 20, all "missing workflow permissions").

Step 3 - three Dependabot branches exist on the remote
  (`dependabot/github_actions/...`). The actions are now pinned to
  commit SHAs, so those pull requests are probably outdated. List them
  with `gh pr list`; ask the owner before closing any.

Step 4 - the small things left (each is one commit; rules below):
  a. `tests/doctor/test_api.c`, the check "a diff of sixteen times the
     keys costs about sixteen times": it measures time, so it fails
     when the machine is busy. Make it robust (for example take the
     best of three runs). Test: run `ctest -R doctor.api` five times
     while another build runs; all five pass.
  b. `src/bridge/store.c`: a new SQLite store file is created readable
     by others. Needs a new platform function ("create a file only its
     owner can read") in `cf_platform.h`, implemented in
     `src/platform/posix/posix.c` and `src/platform/win32/win32.c`,
     added to `cmake/configforge.symbols`, then
     `cmake -P scripts/generate/apiref.cmake`. Follow the recipe "a
     platform capability" in `docs/Architecture.md`. Test (POSIX only):
     after `cf_store_set` on a new path, `stat` shows mode 0600; an
     existing 0644 file keeps its mode.
  c. In Doctor, a sensitive key's refusal reason is now shown only as
     "invalid". Better: show the KIND of refusal (wrong type, out of
     range, malformed reference) without the value. This serves the
     project's purpose: every value can say why it is what it is.

Step 5 - ask the owner two questions (do not decide them yourself):
  * `@file` arguments are read as response files by default in
    `cf_app`. An untrusted argument can inject configuration. Make it
    opt-in? (It changes the interface.)
  * Dependabot alerts and secret scanning are off in the repository
    settings. Turn them on?

Step 6 - then the Roadmap: `docs/Roadmap.md`, "Ideas", the subsection
  "From the security pass of 2026-11-11" (start with the marker-secret
  gate), and after that "Wave 3".

Rules for every step (the project checks them; CONTRIBUTING.md has more):
  1. Write a test that FAILS first. Then fix. Then see it pass.
  2. C89 only: declarations at the top of a block, `/* */` comments.
  3. Run the whole suite before committing:
     Windows: see "Environment" below. Linux (WSL `Ubuntu`): rsync the
     tree to `~/cfv-src`, then for each of `default sanitize portable
     minimal`: `cmake --preset P && cmake --build --preset P -j3 &&
     ctest --preset P -j3`.
  4. Format changed C files with `clang-format-18 -i` in WSL. Do not
     write a string literal continued over several lines as a function
     argument (CI's clang-format 22 lays it out differently and fails).
  5. If a header comment changed: `cmake -P scripts/generate/apiref.cmake`.
  6. Add a line to `CHANGELOG.md` under 1.4.1 and update
     `docs/Roadmap.md` in the same commit.
  7. Push a branch, read CI (`gh run watch`), and only then
     fast-forward `main`. CI has tools this machine lacks.
  8. When everything in this file is done, delete this file.

The rest of this file is the record of the pass: what was found, what
was done, and the machine's environment.

## Prompt for the next agent

You are continuing a security pass on ConfigForge (C89 configuration
library, `D:\Projects\github\configforge`). Read `CONTRIBUTING.md`
first, then this file. Five read-only audits were run on 2026-11-11 and
their findings are listed below with a status each. Your job: finish
every item marked TODO or IN PROGRESS, review what is marked WRITTEN,
verify, and commit one verified unit at a time in the project's
register (read `git log` for it). Rules that matter here:

* For each defect: a test that fails first, then the fix, then the test
  passes. Disable a behaviour by returning early to prove a test can
  fail.
* Strict C89, `/W4 /WX` clean, no frame over 2048 bytes, `static` names
  unique library-wide, string literals under 509 characters.
* Code and documents change together: header comment (then
  `cmake -P scripts/apiref.cmake`), `CHANGELOG.md` under 1.4.1,
  `docs/Roadmap.md`, and `docs/Decisions.md` where a decision is made.
* Do not push, tag or release without the owner saying so.
* Use subagents for one finding group each, with their own build
  directory (`build-<name>`), so their files and builds do not collide.

Environment on this machine (2026-11-11):

* WSL works again (it did not at the start of the day: `0x80070569`, a
  domain policy; if it recurs the owner must ask IT). The distribution
  `Ubuntu` has gcc 13, clang 18, clang-format 18, cmake 3.28, ninja,
  cppcheck, expat and sqlite; 7 GB of memory, so `-j3` at most. Verify
  off `/mnt`: rsync the tree to `~/cfv-src` (without `build*` and
  `.git`) and run the presets there; logs in `~/cfv-logs`. Do not use
  sudo. With all the WRITTEN changes together, Windows passes 158 of 158.
* Windows build works through Visual Studio 18. A wrapper that sets the
  environment and changes to the repository: create `cfenv.cmd` with
  `call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1`,
  `cd /d D:\Projects\github\configforge`, `%*`. Then
  `cfenv.cmd cmake --preset default`, `cmake --build --preset default`,
  `ctest --preset default`. Baseline before any change: 155 of 155.
* `python` on PATH is the Windows Store stub and hangs; use `uv run`.
* `gh` is logged in. Linux verification is GitHub CI: push a branch
  (with the owner's permission) and read the `ci` workflow.
* The old local history is kept in the branch
  `backup/local-main-2026-10-29`; `main` equals `origin/main` (1.4.1
  opened) plus the uncommitted work below.

## Findings and their status

Status words: DONE (committed), WRITTEN (in the working tree,
uncommitted, needs review and the full suite), IN PROGRESS (a subagent
was working on it when this was written; check `git status` and the
tests to see how far it got), TODO, OWNER (a decision for the owner).

### GitHub

| # | Finding | Status |
| - | ------- | ------ |
| G1 | 20 CodeQL alerts `actions/missing-workflow-permissions`, all in `.github/workflows/ci.yml` | WRITTEN: top-level `contents: read`; `doctor-action` job adds `pull-requests: write` |
| G2 | `release.yml` gave every job `contents: write` | WRITTEN: top-level `contents: read`; `publish` already asks for its own |
| G3 | `actions/doctor/comment.sh` took any comment starting with the marker for its own (hijack or suppression of Doctor's comment) | WRITTEN: filters on `.user.login`, new input `comment-author`; `tests/doctor/action.cmake` checks the filter. Not run yet (needs bash; runs in CI). `docs/Doctor.md` section on the action should mention `comment-author` |
| G4 | Third-party actions pinned by tag, not commit SHA (`google/clusterfuzzlite@v1`, `actions/attest-build-provenance@v2`, others) | TODO: pin to SHAs with the tag in a comment; Dependabot keeps them current |
| G5 | Dependabot alerts and secret scanning are off for the repository | OWNER: a repository setting |

### Readers of untrusted bytes

| # | Finding | Status |
| - | ------- | ------ |
| R1 | `src/core/numeric.c` ~843: `cf_text_to_f64` lets exponents 309..400 through; `ldexp` overflows to inf and TRUE is returned. `{"a": 1e309}` or a 310-digit integer puts a non-finite real into the key space; re-emit writes `"a": ` (malformed). Also digits past ~617 lose carries in `cf_big`: a silently wrong value | WRITTEN (`numeric.c`, `tests/unit/test_lex_edges.c`): refused from 10^309 up and when the result rounds to infinity; at most 290 significant digits kept, the rest move the exponent and set a sticky rounding flag. Left: over 290 digits exactly on a halfway point can be one unit in the last place off (the 2048-bit integer cannot hold more within the stack budget) - say so in the CHANGELOG; `0e999` now reads as 0; the JSON/TOML error for `1e309` still says "malformed number", not "out of range"; `cf_numeric.h` should state the range rule (then apiref); subnormals may be rounded twice (to 53 bits, then by `ldexp`) - untested, worth a test |
| R2 | `src/serialize/yaml.c` ~586: the alias guard counts replayed events, not bytes. A 1 MiB anchored scalar aliased 4000 times allocates ~4 GiB | WRITTEN (`yaml.c`, `tests/unit/test_yaml.c`): replayed bytes bounded at 64 x input + 1 MiB, same error as the event bound. Left: `docs/Decisions.md` D-63 (~line 2973) describes only the event bound; add the byte bound |
| R3 | `yaml.c` ~235 `yaml_find`: re-anchoring a name grows the hash chain; O(anchors x aliases) | WRITTEN: a name anchored again overwrites its entry (unless that node is still open) |
| R4 | `src/serialize/ini.c` ~335 -> `ini_earlier_run` ~171: O(N^2) on a section of distinct `name[]` runs; reachable from `cf_codec_detect` | WRITTEN (`ini.c`, `tests/unit/test_serialize.c`): a section's run names in a hash in `reserve_work` space, the old rescan when none is lent; 60,000 lines 60 s -> 3 s. Left: the cost test passes on the old code too because `unit.test_serialize` has no TIMEOUT - set `TIMEOUT 30` in `tests/unit/CMakeLists.txt`; neither file has been through clang-format (CI's version is 18) |
| R5 | `src/serialize/xml.c` ~154 `xml_find`: linear scan per sibling, O(N^2) for many distinct sibling names | TODO: hash per level, as yaml.c and toml.c do. Needs expat, so not buildable on this Windows machine; verify in CI |
| R6 | `src/core/keyspace.c` ~177 `cfi_ks_expect`: the line-count guess is capped at 2^20 records but not relative to input size; a 2 MB file with one million-line string reserves ~90 MB, on each reload | TODO: also cap at a small multiple of the input bytes |
| R7 | TOML's definition table uses unkeyed FNV-1a; precomputed collisions make it quadratic | TODO (low): seed per decode, or bound the chain; a decision worth an entry in `docs/Decisions.md` |
| R8 | `cf_codec_detect` with a NULL arena: TOML always fails (`reserve_work` returns NULL, taken for out of memory); JSON strings of 256 bytes or more fail the same way | TODO: NULL arena should mean "no work space", not failure |

### Core: secrets and `${}` references

All in `src/core/validate.c` unless said; one subagent has them
(`build-core`, tests in `tests/unit/test_interpolate.c` and the
sensitive-value tests).

| # | Finding | Status |
| - | ------- | ------ |
| C1 | HIGH. ~174, ~398: a CF_SENSITIVE value with a malformed `${` is quoted (up to 40 bytes) in the refusal reason, so it shows in `cf_error`, `explain()` and a blind report's `reason`. Same for "`${x}` names a key with no value" | WRITTEN: fixed wording for a sensitive key; `validate_list_shape` also printed a sensitive list's length, fixed |
| C2 | ~308 `expand_build`: no overflow check on the summed length, no cap on expanded size. 26 doubling lines make 256 MB; on 32-bit the sum wraps and pass 1 overflows the heap | WRITTEN: `CF_EXPAND_MAX` (1 MiB) in `cf_define.h`, checked before each addition. Not in `cf_build_limit` (it sizes no buffer) - a decision to record in `docs/Decisions.md` |
| C3 | ~327 `ref_text`: NULL dereference when a referenced key falls back to nothing during recursive expansion | WRITTEN |
| C4 | `config.c` ~1309, `keyspace.c` ~308, `define.c` ~192: a secret written under a deprecated name is reported in clear | WRITTEN: at seal a deprecated definition inherits CF_SENSITIVE from its `replaced_by` target, through chains; documented on `replaced_by` |

Changed: `src/core/validate.c`, `src/core/define.c`,
`include/configforge/cf_define.h`, `tests/unit/test_interpolate.c`,
`tests/unit/test_secrets.c`. Each confirmed by a test that failed first.
The apiref script is `scripts/generate/apiref.cmake`.

### Files, directories, watching

One subagent has A to E (`build-plat`).

| # | Finding | Status |
| - | ------- | ------ |
| P1 | `posix.c` ~203, `win32.c` ~228, callers `bridge/dir.c` ~91, `secrets.c` ~65: a directory listing is cut silently at 256 entries in readdir order | WRITTEN: `cf_platform_dir_list` returns -2 for "read, but not whole"; callers fail naming the directory and the limit; `CF_OPTIONAL` excuses only -1 |
| P2 | `win32.c` ~299 `win32_up`: infinite loop watching a path on a missing drive (`Q:app.ini`) | WRITTEN, the hang confirmed on the old code |
| P3 | `dir.c` ~105, `win32.c` ~233: a drop-in file with a very long name is skipped without a word | WRITTEN: an error naming it, both branches tested on Windows |
| P4 | `posix.c` ~547 `kq_add`: a FIFO in a watched directory hangs the watcher | WRITTEN, POSIX, NOT COMPILED: `O_NONBLOCK` on both branches (macOS `O_EVTONLY` too), `fstat`, anything but a file or directory closed and treated as absent; no O_NOFOLLOW (Kubernetes mounts are symlinks). CI must check it, the macOS and kqueue jobs above all |
| P5 | `blob.c` ~57 via `dir.c` ~141, `secrets.c` ~104: a FIFO named `zz.ini` hangs load and reload | WRITTEN, POSIX part NOT COMPILED: the POSIX listing `stat`s each entry (following links); a subdirectory is skipped as on Windows; a FIFO, socket, device or dangling link fails by name. Review: a Kubernetes mount's `..data` and `..<timestamp>` entries must still be skipped, not refused. `kq_add_entries` still skips an over-long path silently |

Changed: `include/configforge/cf_platform.h`, `src/platform/win32/win32.c`,
`src/platform/posix/posix.c`, `src/bridge/dir.c`, `src/bridge/secrets.c`,
`tests/unit/CMakeLists.txt`, new `tests/unit/test_dirlimits.c`.
`docs/API.md` must be regenerated once more at commit time.

| P6 | `bridge/store.c` ~216: a new SQLite store is created 0644 and may hold sensitive values | TODO: create 0600 |
| P7 | `sources/entry.c` ~558, wired by default in `config.c` ~251 (`cf_app_builder`): any argument starting with `@` is read as a response file, so an untrusted positional argument injects configuration and is a file-existence oracle | OWNER: make response files opt-in? An interface change; if yes, a D-number |

### Doctor and the bindings

| # | Finding | Status |
| - | ------- | ------ |
| D1 | HIGH. `tools/doctor/engine.c` `rule_value_diff`, CFD006 `doctor_shown`, CFD017, `dump`, `explain`, and the live gate's error: a sensitive key's values printed in clear from full-mode reports - into the job log, the summary and the PR comment | WRITTEN: `told_by_digest` and `<redacted>` by the key's sensitive flag; fixtures with marker secrets; `tests/doctor/secrets.cmake`, `test_secret_unshown` in `test_api.c`. Affects published versions: say so in the release notes |
| D2 | `doctor.c` `print_text`: a value with a newline and `::error ...` injects a workflow command into the Actions log | WRITTEN: `text_field` escapes control characters; `tests/doctor/escape.cmake` |
| D3 | `doctor.c` `md_cell`: `@` and bare URLs in the PR comment | WRITTEN: `@` as `&#64;&#8203;`, `://` broken with a zero-width space |
| D4 | Python `setting()` shows a SENSITIVE field in `repr` | WRITTEN, `tests/python/test_binding.py` 26 of 26 |
| D5 | `bindings/rust/src/lib.rs` ~200 `Keep::refusal`: an error for a string with a NUL echoes the whole value (which may be a secret) | TODO: name the key or position only |
| D6 | `bindings/rust/src/edit.rs` ~83 `Edit::set`: a value with a NUL is silently written as U+0001, though lib.rs ~168 says it is refused | TODO: return an error |

## Ideas the audits produced (for `docs/Roadmap.md`, Ideas)

* A marker-secret gate: one test that drives a marker secret through
  every refusal path (malformed reference, wrong type, range, enum,
  policy, rename, list) and every output (error, explain, blind report,
  diff, log, Doctor's four formats) and fails if the marker appears.
  The four secret leaks above (C1, C4, D1, D4) would each have failed it.
* A per-decode output budget (bytes and keys) enforced in the
  flattener, so every codec inherits one bound on amplification rather
  than each reader inventing its own (R2 and C2 are the same defect in
  two places).
* A fuzz check that compares arena bytes with input size, so an
  amplification regression fails CI.
* Key the secret digest (a per-report salt, or a configured key):
  unsalted FNV-1a over a PIN or short password can be brute-forced
  offline from a blind report or a fingerprint.
* A trusted-directory check like sudo's for drop-in and secrets
  directories: refuse an entry that is group- or world-writable or not
  owned by the effective user or root.
* Doctor `--redact-all`, on by default in the action, so a full report
  can never reach a PR comment.
* The platform listing returns the entry type, so both platforms treat
  non-regular entries identically.

Doctor's changed files: `tools/doctor/engine.c`, `engine.h`, `doctor.c`,
`tests/doctor/fixtures.c`, `test_api.c`, `CMakeLists.txt`, new
`secrets.cmake` and `escape.cmake`, `docs/Doctor.md`,
`bindings/python/src/configforge/_settings.py`,
`tests/python/test_binding.py`. Doctor's tests: 44 of 44 in its own build.

clang-format could not be run on this machine (access denied), so every
changed C file was laid out by hand: run `scripts/format.cmake` where
clang-format 18 exists before committing, or CI's format check may fail.

## State at the last update (2026-11-11, evening)

Everything marked WRITTEN above passed together: Windows 158 of 158;
Linux (WSL) default 167/167, sanitize 167/167, portable 166/166,
minimal 111/111 - so the POSIX changes marked NOT COMPILED are now
compiled and tested. clang-format 18, cmake-format and the apiref were
run from WSL. G4 is done (every action pinned to a commit SHA, checked
against GitHub). CHANGELOG, Roadmap, Decisions (D-63 addendum, D-77,
D-78), `cf_numeric.h` and Doctor.md are written.

An independent review of the diff then found five gaps, all closed:
the total of all `${}` expansions is bounded (`CF_EXPAND_TOTAL_MAX`,
16 MiB); `doctor dump` and `explain` escape every field; a sensitive
key's refusal reason is shown as "invalid"; `comment.sh` asks the token
for its login when `comment-author` is empty; `numeric.c`'s digit
counters saturate.

STATE at 16:46 on 2026-11-11: DONE AND ON `main`. The pass is four
commits (`4af2b33`, `4c1ae97`, `ea60e72`, `6c528fa`), CI green on the
last, fast-forwarded into `main` and pushed; the working branch is
deleted. Step 1 below is therefore done. What CI taught on the way,
for the next change: the format gate wants clang-format 22.1.8 and this
machine has 18 (format with 18, keep clear of string literals
continued as call arguments and long `||` conditions); CI's clang-tidy
21 and MSVC `/analyze` see things no tool here does, so push a branch
and read CI before merging. The 20 CodeQL alerts should close on the
next `codeql` run on `main` - check with
`gh api 'repos/{owner}/{repo}/code-scanning/alerts?state=open'`.

Next, in order:
1. If that CI run is green: `git switch main && git merge --ff-only
   security-pass-1.4.1 && git push`, then delete the branch locally and
   on the remote. The owner said to commit and push. If it is red, fix
   what it names first.
2. Still open from the pass: the store's permissions (P6: needs a
   platform capability, a public function, the symbols file and the
   apiref), R7 hash seeding (a decision). For the owner: P7 response
   files on by default, G5 Dependabot alerts and secret scanning off.
3. `doctor.api`'s timing check ("sixteen times the keys costs about
   sixteen times") fails when other builds load the machine and passes
   alone; worth making it compare work, not time.
4. The project's purpose is that every value can say why it is what it
   is. By that measure the next work is: the marker-secret gate (Ideas),
   showing the KIND of a sensitive key's refusal rather than only
   "invalid" in Doctor, and then the Roadmap's own Wave 3.

## When everything is written

1. One full build and `ctest` on Windows with all changes together.
2. `cmake -P scripts/format.cmake`, then `cmake -P scripts/apiref.cmake`.
3. CHANGELOG 1.4.1: security fixes first (C1, C4, D1, then the rest).
4. Roadmap: the items, the ideas above, P7 and G5 under open decisions.
5. Commit a unit at a time; ask the owner before pushing. Linux is
   verified by CI on a pushed branch: `sanitize`, `portable` and
   `minimal` jobs are the ones that matter for these changes.
6. `SECURITY.md` / the release notes: C1, C4 and D1 affect published
   versions; say so, as the Roadmap's "Release follow-up" did before.
