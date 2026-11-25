# Handoff (2026-11-24, late evening)

Landed on main this evening, each alone with its CI green, its branch
deleted locally and on origin, its worktree removed:

- **I-2** (7146038): 1.9.0 opened (`## 1.9.0 — unreleased`). D-89 was
  settled for the thirteen functions experimental since 1.8: all are
  `CF_API`, none were removed, and `cf_format_limits` and `cf_usage` keep
  their signatures (a further limit or option comes as a new function).
  The maintainer, away, said on 2026-11-24 to go on without asking and
  to open the new version if needed. HANDOFF's recommendation was taken
  as it stood and recorded in D-89 ("Settled in 1.9.0"). It is not
  released and not tagged.
- **P-2** (e0c0904): the word-at-a-time backend of lanes, kept for
  `find_byte`, `find_lt`, `ascii_run` and `count_byte`.
- **L-9** (3f251f8): the Scorecard findings the project can fix itself.
  `bench.yml` checks out only main and reads results as data, the pip
  installs and the fuzz image are pinned by hash, and `SECURITY.md` says
  how to report. BranchProtection, CodeReview, CIIBestPractices and
  Maintained are repository settings and stay the maintainer's.
- **N-12** (d8a0cfa): narrowed and landed. The built-in YAML reader
  refuses a control byte inside a comment (the `IwAj` fault). The check
  and the comment's end are found in one word-at-a-time pass, so
  bench-gate holds. `fuzz_yamldiff` is exploratory: not a ctest test, not
  in a per-push job (the AFL job skips it), and run nightly in the
  `yamldiff` job with `continue-on-error`. The lone-CR comment fault and
  the two findings "to decide" are in docs/Roadmap.md.

The maintainer, away (2026-11-24, relayed): all authority is the
organiser's. Carry every card that needs no decision of theirs. Never
push a tag or a release, never change repository settings, never type a
password, never run Codex with full access. Profiling (the data-driven
self-improvement loop) waits until the feature work is done; the OpenSSF
score is low priority. Shut down (`shutdown //s //t 60`) once this file
is current and everything is pushed, at the first of: no card is left
that needs no decision, or weekly usage reaches 95%.

The maintainer, 2026-11-25: the priority is finishing the cards already
in the plan. No new card is written from the Roadmap until every open row
of docs/Plan.md's index is done, and every card in docs/plan/ is
finished (2026-11-25, again: "docs/plan dizinindeki tüm işler bitmeden
yenisini üretme"). Roadmap item 5 (mutation testing) was to be N-18; its
card was never written and waits for that.

Next, in this order; none waits on the maintainer:

1. **L-10** landed (c4a13af): every fetch held by `gate.fetches`
   (`scripts/check/fetches.txt`), read-only workflow tokens by
   `gate.permissions`, vcpkg pinned by commit, dependency review on pull
   requests, the SBOM a release file. Its notes list the settings left
   to the maintainer.
2. **N-13** (2687154), **N-14** (73c2798), **N-15** (dfc1fed), **N-16**
   (bbb1a77, Git's textconv driver) and **N-17** (ca3dd7b, `doctor
   drift`) landed.
3. **N-20** landed (d04740d), **N-19** (`gate.misra`, D-112) landed
   (24b1c7d); the finished cards were cleaned out of docs/plan (70ad1c3).
   **P-3 waits for 2.4.0** (d6bdd05): its rewrite conflicts with D-101 (the
   program, not the library, chooses the backend, from 2.4.0) and D-102
   (the scans move into lib/text); two cold readers' nineteen findings are
   under its notes.
4. **L-3b** (Frama-C EVA) is the one open card. Its card, with the
   "Organiser's answers" (opam's release binary, SHA-256 checked, in
   $HOME/.local/bin, no sudo; CI installs with apt; the job runs on
   schedule, dispatch and pushes to lib/port, lib/numeric, lib/base,
   tests/eva), is on `work/L-3b` (16e3ab8; worktree `.claude/worktrees/l3b`).
   Nothing of its code is written. Carry it from there; when it lands,
   docs/plan holds no open card, and the Roadmap's items become cards
   again (item 5, mutation testing, first).
5. The maintainer's: PyPI and crates.io publishing (their tokens, or
   trusted publishing); the repository settings in L-9's and L-10's
   notes (a ruleset on `main` with required reviews and checks, signed
   commits and tags, two-factor authentication); whether to keep the
   Dependabot pull requests that queue ahead of card CI.

Budgets at the stop (2026-11-25 16:20 UTC): Claude weekly 94%, the stop
line is 95%; it resets 2026-11-26 10:00 UTC. Lessons
are in docs/Maintaining.md ("Where the tokens go", "Working habits that
paid"). Three this evening:

- A worker's per-byte check on a hot path failed bench-gate (+42%).
  A brief that adds work to a scan says which bench cells it must hold.
- rsync is not in the WSL distro: copy trees with tar
  (`/mnt/d/cf-work/i2-build.sh`).
- From Git Bash, set `MSYS_NO_PATHCONV=1` for `wsl.exe` calls with
  `/mnt` paths.

- `work.py land` formats first, and this machine's cmake-format reflows
  one line of `tests/doctor/redact.cmake` that CI's keeps (gate.format.cmake
  passes on the original). The format commit then forces a full local
  verify. N-17 was landed by hand instead: `git merge --squash`, the tree
  compared with the branch's (`git write-tree`), the index row, push,
  branch and worktree removed. Until the versions agree, land so when the
  only format change is that file.

Delete this file in the commit that finishes step 4.
