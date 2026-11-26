# HANDOFF (2026-11-27, afternoon)

State: the maintainer approved `docs/plan/Layout.md` (stage 0, then the
modules; 2.0.0's breaks early; the standing answer rule in docs/Plan.md
"For the maintainer"). The index in docs/Plan.md is now generated:
`python scripts/work/index.py` (L-20, landed 84d48d9); every card carries
`version` and `status`. The maintainer, 2026-11-27: number defaults never
change (K-10a, K-10b add options only); keep CI use small (L-22).

Codex has no allowance until 2026-11-29 09:47; Claude carries on.

## Steps

1. L-21 (`scripts/check/ready.py`) was being carried by a worker on
   `work/L-21` (2026-11-27). If the branch exists: check its report or
   state, finish it, land it by hand as L-20 and L-22 were (squash onto
   main, tree check, push, `python scripts/work/index.py --done <id> <hash>`,
   commit "Plan: <id> done (<hash>)", push, delete the branch). L-22 landed
   (02acbdd): CI is tiered; `work.py land` now runs the full suite once by
   `gh workflow run ci -f tier=full` (skipped when that sha already has a
   green full run); landing by hand needs the same full run when the card
   touched code (not for scripts-only cards).
2. Then the next `open` row of the index whose needs are done, after
   `ready.py <id>` passes: L-18a, then the file layer (F-1a, Y-2, Y-8, Y-5,
   Y-6, Y-3; rewritten to D-224/D-227 in 33b6602, the organiser accepted the
   worker's readings under F-1b's "Organiser's answers"), then K-9c.
   Stage 0 rows still to be written as cards: 0.3 (options with defaults,
   D-220: the convention, the `none` platform, `CF_E_CAPABILITY`), K-9a,
   K-9b; and the merges Layout.md names (0.5: K-6a + K-6b + M-11b; Y-15
   split into Y-15a/Y-15b). Cards whose text still describes an old option
   are rewritten when `ready.py` flags them, not in advance: F-2a, F-3i and
   F-3j still name `cf_platform_write_file` and fixed attempts (D-224, D-225).
3. Before the 2.0.0 cards (stage 0.4-0.6) open: propose the 1.9.0 release
   to the maintainer (tags stay the maintainer's).
4. The cards still to be written from 2026-11-26's answers: Y-41-m, Y-42,
   C-9-m, C-10c-m, C-10d-m, K-10c-m (all in Layout.md's "not now" except
   Y-42, module 1).
5. Ruleset note for the maintainer: a skipped `matrix` job reports one check
   named `matrix`, not the three `matrix (<os>)` the ruleset requires; pushes
   to main bypass it, but a pull request would wait (L-22's notes).

Windows note: `gate.text` may fail locally on `docs/plan/N-next.md` with
CRLF in a Windows working copy (the index copy is LF); not a card's fault.

Delete this file in the commit that lands L-21's index row.
