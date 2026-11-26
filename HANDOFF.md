# HANDOFF (2026-11-27, evening)

State: the plan runs from docs/Plan.md's generated index (L-20), cards
open only when `python scripts/check/ready.py <id>` passes (L-21; `brief.py`
refuses otherwise, `--force` for a card being repaired), CI is tiered
(L-22). The maintainer's standing approval (docs/Plan.md "For the
maintainer", 2026-11-27): decide with the best choice and record it; the
organiser may tag and release a finished milestone.

## Steps

1. K-11a (options with defaults) was being carried by a worker on
   `work/K-11a`. If the branch exists: read its last commits, finish or
   take over, land by hand (squash onto main, tree check, push,
   `python scripts/work/index.py --done K-11a <hash>`, commit
   "Plan: K-11a done (<hash>)", push, delete the branch). The full suite
   must have run once on the branch (`gh workflow run ci --ref work/K-11a
   -f tier=full`), as `work.py land` would do.
2. Then the next `open` index row: run `ready.py` on it; a failing card is
   corrected first (citations to full paths, the old option removed), as
   the worker's step 0. Order now: L-18a, F-1a, Y-2, Y-8, Y-5, Y-6, Y-3,
   K-9c, K-11b, K-11c (stage 0, 1.9.0).
3. When stage 0's 1.9.0 rows are done: release 1.9.0 (docs/Maintaining.md
   "Versions"; `cmake -DTO=... -P scripts/release/version.cmake` only when
   opening the next version), then the 2.0.0 rows.
4. Stale cards, rewritten only when they come up: F-2a, F-3i, F-3j
   (D-224, D-225); cards to be written: K-9a, K-9b, Y-41-m, Y-42, C-9-m,
   C-10c-m, C-10d-m, K-10c-m.
5. For the maintainer: a skipped `matrix` job reports one check named
   `matrix`, not the three `matrix (<os>)` the ruleset requires; pushes to
   main bypass it, a pull request would wait.

Delete this file in the commit that lands the last stage 0 1.9.0 card.
