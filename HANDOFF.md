# HANDOFF (2026-11-28)

State: the plan runs from docs/Plan.md's generated index (order:
docs/plan/Layout.md). No TO DECIDE is left in any card; only nine
account-bound cards stay parked (L-17-m, L-19-m, P-15-m, M-19-m, M-23-m,
M-25-m, M-29-m, M-30-m, G-9-m). The maintainer's standing approval and
release rights: docs/Plan.md "For the maintainer". 1.9.0 is released;
publishing to crates.io and PyPI is deferred (M-23-m): do not chase it.

## In flight

1. The batch of K-6a, K-9d, M-11a, K-9e (and its fixes) went to main on
   2026-11-28 (3db1b2e) after full run 46076732675 was green. Lessons
   now in docs/Plan.md "The loop", step 11: format, codemap, apiref and
   every `gate.*` in WSL before each full run, and the MISRA record
   regenerated after code changes (`cmake -DSRC=<tree> -DBIN=<misra build>
   "-DDEFINES=-D__GNUC__=15|-D__GNUC_MINOR__=2" -DWRITE=ON -P
   cmake/gates/GateMisra.cmake`; the helper D:\cf-work\misra-regen.sh is
   not in the repository).
2. `batch` (D:\cf-work\batch) now holds K-9f (0ea8118) and K-9g
   (c898df5), landed after local ACCEPT; no full run yet.
3. K-9b is in flight: worktree D:\cf-work\K-9b, branch work/K-9b, cut
   from work/K-9g; its D-number is D-237. When it lands, squash it onto
   batch (its K-9f/K-9g commits are already there: resolve by keeping
   work/K-9b's tree for lib/ and scripts/), then one full run on batch.
   D:\cf-work\K-9g and work/K-9g can then be removed.
4. yamldiff (exploratory, continue-on-error) found an anchor named with
   `?` alone at the top: card K-8b (2.2.0).

## Next

5. 2.0.0 in Layout.md's order after K-9b: M-11b, M-13; beside-cards ride
   along (K-9h, K-9i, F-6, F-7), then the safety-critical cards written
   2026-11-28: L-28 (CERT matrix), L-25 (MC/DC), L-26a, L-26b (traces),
   L-27 (safety manual, after L-12c). They decide D-233 to D-236.
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
