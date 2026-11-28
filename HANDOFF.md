# HANDOFF (2026-11-28)

State: the plan runs from docs/Plan.md's generated index (order:
docs/plan/Layout.md). No TO DECIDE is left in any card; only nine
account-bound cards stay parked (L-17-m, L-19-m, P-15-m, M-19-m, M-23-m,
M-25-m, M-29-m, M-30-m, G-9-m). The maintainer's standing approval and
release rights: docs/Plan.md "For the maintainer". 1.9.0 is released;
publishing to crates.io and PyPI is deferred (M-23-m): do not chase it.

## In flight

1. On main (5a26c7e, 2026-11-28): K-6a, K-9d, M-11a, K-9e, then K-9f,
   K-9g and K-9b (D-237), each batch after a green full run (last:
   46089903557). `batch` equals main; cut the next cards from it.
2. Before each full run on batch, the organiser's local check in WSL
   (docs/Plan.md "The loop", step 11, and docs/plan/worker-notes.md):
   format, codemap, apiref; a default build with every test; a clang
   build; the `none` and `ubsan-strict` presets; `cmake -DCF_CLANG_TIDY=ON`
   built; CI's iwyu line (ci.yml, job `iwyu`); and the MISRA record
   (`cmake --preset misra -B <dir>`, then `cmake -DSRC=<tree> -DBIN=<dir>
   "-DDEFINES=-D__GNUC__=15|-D__GNUC_MINOR__=2" [-DWRITE=ON] -P
   cmake/gates/GateMisra.cmake`). K-9b's batch took four red runs for
   want of these (clang's -Wsign-conversion, none, clang-tidy, iwyu,
   macOS's aligned image buffer, ubsan-strict's unsigned overflow).
3. K-9j (written 2026-11-28) takes the two checks K-9b left: a builder
   given its source again after a failed create, and
   `cf_ks_add_candidate`'s partial state.
4. yamldiff (exploratory, continue-on-error) found an anchor named with
   `?` alone at the top, and a second `|#` case: both in card K-8b (2.2.0).

## Next

5. 2.0.0 in Layout.md's order: K-9j, M-11b, M-13; beside-cards ride
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
