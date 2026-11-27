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
   archives and "How each archive was built"). Publishing to crates.io and
   PyPI is deferred by the maintainer (2026-11-27): the `crates` job waits on
   the repository variable PUBLISH_CRATES (M-23-m). Do not chase it.
2. Landed on main for 2.0.0: M-10, K-6b, M-12, K-9a. CI runs once a batch
   (docs/Plan.md "The loop", 9-11): cards land on branch `batch` after local
   ACCEPT; one full-tier run on `batch`, then `git push origin batch:main`.
   `batch` now holds K-9d, M-11a, K-9e, two "Batch:" record fixes and the
   cards K-9h/K-9i. Its last full run (46058347107) was green but for an
   afl crash in the `chain` target (inputs: artifact `afl-findings`).
3. `batch` (D:\cf-work\batch; tip after a6eddd5) also holds the afl `chain`
   fix (5cdd262: the converter drops an emptied list as it drops an emptied
   table) and the cards K-9h, K-9i, F-6, F-7. No full run yet since the fix.
   K-6a landed on `batch` too (2026-11-28). The batch's full run
   46067496921 was started on it: read its result
   (`gh run view 46067496921 --json conclusion,jobs`); red -> fix on
   `batch` ("Batch: ..."), run again; green -> `git push origin batch:main`
   (main and batch differ then only by merges of HANDOFF.md: merge
   origin/main into batch without another run if that is all).
   After records change: sizes (scripts/check/sizes.cmake -DWRITE=ON
   -DTREE=<hash> on a copy without .git), then `cmake -P
   scripts/generate/matrix.cmake` (README cells), and the MISRA record
   (gate.misra -DWRITE=ON).

## Next

4. 2.0.0 in Layout.md's order: K-9f, K-9g, K-9b, M-11b, M-13; beside-cards
   ride along (K-9h, K-9i: the safety inventory of 2026-11-27).
5. The maintainer wants the library usable in safety-critical work, free
   tools only, in stages (docs/Compliance.md "Order of work"): write the
   four missing cards named there (MC/DC, D-number-to-tests table and
   gate, safety manual, CERT C matrix). IO: F-6 and F-7 are written
   (reading is CPU-bound; the cost is fsync and, with many files, calls
   per file).
6. Measure the batch flow after a few batches (cards a day, red full
   runs, time to fix) into docs/Maintaining.md "Working habits that paid".
7. Codex's allowance returns 2026-11-29 09:47: give it most cards
   (scripts/work/codex-card.sh).

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

Weekly window was 97% on 2026-11-27 at 20:35 UTC: this account handed off; the next session is on another account (resets 2026-12-03 10:00 UTC):
one worker at a time. At 99% weekly: everything pushed, this
file current, and give the maintainer one prompt for a session on another
account ("Read CLAUDE.md, then HANDOFF.md, and continue").

Delete this file when the steps above are done and nothing is in flight.
