# HANDOFF (2026-11-27, evening)

State: the plan runs from docs/Plan.md's generated index (order:
docs/plan/Layout.md). Landed today: L-20, L-21, L-22, L-23, L-24, K-11a,
K-11b, K-11c, L-18a, F-1a, K-9c, Y-2, Y-3, Y-5, Y-6, M-18. No TO DECIDE is
left in any card; only nine account-bound cards stay parked (L-17-m, L-19-m,
P-15-m, M-19-m, M-23-m, M-25-m, M-29-m, M-30-m, G-9-m): prepare their files,
give the maintainer short steps when one comes up. The maintainer's standing
approval and release rights: docs/Plan.md "For the maintainer".

## In flight

1. Y-8 (close-on-exec), worktree D:\cf-work\Y-8, branch `work/Y-8`: its
   full-tier run 46001462250 failed in none (test_cloexec needs skips by
   platform capability), infer (posix/file.c cf_platform_fopen), profiles
   (misra record for posix/file.c, posix.c) and kqueue; a worker was fixing
   them. Check `gh run list --branch work/Y-8`; when a full-tier run on the
   branch tip is green, land it.
2. M-10 (the sizing modes' interface, 2.0.0), worktree D:\cf-work\M-10,
   branch `work/M-10`: its CHANGELOG lines sit under 1.9.0 in a subsection
   "### 2.0.0 (held until 1.9.0 is released)". It lands after 1.9.0 is
   released: move those lines under a new "## 2.0.0 — unreleased" opened by
   `cmake -DTO=2.0.0 -P scripts/release/version.cmake`.

## Next

3. When Y-8 lands: release 1.9.0 (date the CHANGELOG heading, commit, tag
   `v1.9.0`, push the tag). The release workflow publishes the archives with
   "How each archive was built" (L-23) and the crate to crates.io (L-24,
   secret CARGO_REGISTRY_TOKEN). Read the release notes and crates.io after.
4. Then 2.0.0 in Layout.md's order: K-6a, K-6b, M-11b, K-9a, K-9d-K-9g, K-9b,
   then row 0.9 (M-10, M-12, M-11a, M-13). Beside cards of 1.9.0 (L-11, L-12a,
   L-13, ...) may be taken whenever a module waits.

## How to land a card by hand (what worked)

In the card's worktree: `git fetch -q && git merge origin/main` (CHANGELOG
and Decisions conflicts: keep both sides), `python scripts/work/index.py`,
commit; if code came in from main, push and wait for the core run. Then
`git checkout --detach origin/main && git merge --squash work/<id>`, commit
"<id>: <goal>" with the branch's subjects, check the tree equals the
branch's, `git push origin HEAD:main`, `python scripts/work/index.py --done
<id> <hash>`, commit "Plan: <id> done (<hash>)", push, then delete the branch
on origin and locally as separate commands (a `grep -v` with no output ends
an `&&` chain). One full-tier run per card that touched code or CMake, with
no push to the branch while it runs. Workers get `python scripts/work/brief.py
--force <id>` and docs/plan/worker-notes.md.

## Budget

Weekly window was 73% on 2026-11-27 afternoon (resets 2026-12-03 10:00 UTC):
at most two or three workers at once. At 99% weekly: everything pushed, this
file current, and give the maintainer one prompt for a session on another
account ("Read CLAUDE.md, then HANDOFF.md, and continue").

Delete this file in the commit that lands the last stage 0 1.9.0 card.
