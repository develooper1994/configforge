# HANDOFF (2026-11-27, evening)

State: the plan runs from docs/Plan.md's generated index (order:
docs/plan/Layout.md). Landed today: L-20, L-21, L-22, L-23, L-24, K-11a,
K-11b, K-11c, L-18a, F-1a, K-9c, Y-2, Y-3, Y-5, Y-6, Y-8, M-18; 1.9.0
released. No TO DECIDE is
left in any card; only nine account-bound cards stay parked (L-17-m, L-19-m,
P-15-m, M-19-m, M-23-m, M-25-m, M-29-m, M-30-m, G-9-m): prepare their files,
give the maintainer short steps when one comes up. The maintainer's standing
approval and release rights: docs/Plan.md "For the maintainer".

## In flight

1. 1.9.0 is released (tag v1.9.0, 2026-11-27; GitHub release with its
   archives and "How each archive was built"). Its `crates` job failed:
   crates.io wants a verified email on the maintainer's account. When the
   maintainer has verified it: `gh run rerun 46010730161 --failed`, then check
   https://crates.io/crates/configforge.
2. M-10 (the sizing modes' interface, 2.0.0), worktree D:\cf-work\M-10,
   branch `work/M-10`: a worker is bringing it onto main (open 2.0.0 with
   `cmake -DTO=2.0.0 -P scripts/release/version.cmake`, move its CHANGELOG
   lines from "### 2.0.0 (held until 1.9.0 is released)" under the new
   heading, full-tier run). When that run is green, land it.

## Next

3. 2.0.0 in Layout.md's order: K-6a, K-6b, M-11b, K-9a, K-9d-K-9g, K-9b,
   then row 0.9 (M-12, M-11a, M-13). Beside cards of 1.9.0's modules (L-11,
   L-12a, L-13, ...) now go to the next minor after 2.0.0.

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

Weekly window was 80% on 2026-11-27 at 14:00 UTC (resets 2026-12-03 10:00 UTC):
one worker at a time. At 99% weekly: everything pushed, this
file current, and give the maintainer one prompt for a session on another
account ("Read CLAUDE.md, then HANDOFF.md, and continue").

Delete this file when the steps above are done and nothing is in flight.
