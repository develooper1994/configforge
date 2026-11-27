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
3. In flight (worktrees under D:\cf-work, no CI of their own):
   - fix-chain (branch `work/fix-chain`): a worker reproducing and fixing
     the afl `chain` crash with a regression test. Land it onto `batch`,
     then `gh workflow run ci --ref batch -f tier=full`; green -> batch to
     main.
   - K-6a (branch `work/K-6a`, cut from batch): land onto `batch`.
   The batch worktree is D:\cf-work\batch. After records change: sizes
   (scripts/check/sizes.cmake -DWRITE=ON -DTREE=<hash> on a copy without
   .git), then `cmake -P scripts/generate/matrix.cmake` (README cells), and
   the MISRA record (gate.misra -DWRITE=ON).

## Next

4. 2.0.0 in Layout.md's order: K-9f, K-9g, K-9b, M-11b, M-13; beside-cards
   ride along (K-9h, K-9i: the safety inventory of 2026-11-27).
5. The maintainer wants the library usable in safety-critical work, free
   tools only, in stages (docs/Compliance.md "Order of work"): write the
   four missing cards named there (MC/DC, D-number-to-tests table and
   gate, safety manual, CERT C matrix).
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

Weekly window was 95% on 2026-11-27 at 20:10 UTC: this account hands off at 99% (resets 2026-12-03 10:00 UTC):
one worker at a time. At 99% weekly: everything pushed, this
file current, and give the maintainer one prompt for a session on another
account ("Read CLAUDE.md, then HANDOFF.md, and continue").

Delete this file when the steps above are done and nothing is in flight.
