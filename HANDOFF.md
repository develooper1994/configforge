# HANDOFF (2026-11-27)

State: the maintainer approved `docs/plan/Layout.md` (stage 0, then the
modules; 2.0.0's breaks early; the standing answer rule in docs/Plan.md
"For the maintainer"). The index in docs/Plan.md is now generated:
`python scripts/work/index.py` (L-20, landed 84d48d9); every card carries
`version` and `status`. The maintainer, 2026-11-27: number defaults never
change (K-10a, K-10b add options only); keep CI use small (L-22).

Codex has no allowance until 2026-11-29 09:47; Claude carries on.

## Steps

1. L-22 (CI in two tiers) first, before L-21, because every later card's
   CI time depends on it: `python scripts/work/brief.py L-22` as its
   worker's brief (sonnet, medium). Land by hand as L-20 was: squash onto
   main, tree check, push, `python scripts/work/index.py --done <id> <hash>`,
   commit "Plan: <id> done (<hash>)", push, delete the branch. Until L-22
   lands, push a branch only when a step needs CI.
2. L-21 (`scripts/check/ready.py`), the same way.
3. Then the next `open` row of the index whose needs are done, after
   `ready.py <id>` passes (once L-21 lands). Stage 0 rows still to be
   written as cards: 0.3 (options with defaults, D-220: the convention, the
   `none` platform, `CF_E_CAPABILITY`), K-9a, K-9b, K-9c; and the merges
   Layout.md names (0.5: K-6a + K-6b + M-11b; Y-15 split into Y-15a/Y-15b;
   cards whose "decided" still describes an old option: rewrite each to its
   "Organiser's answers" when `ready.py` flags it, not in advance).
4. The cards still to be written from 2026-11-26's answers: Y-41-m, Y-42,
   C-9-m, C-10c-m, C-10d-m, K-10c-m (all in Layout.md's "not now" except
   Y-42, module 1).

Windows note: `gate.text` may fail locally on `docs/plan/N-next.md` with
CRLF in a Windows working copy (the index copy is LF); not a card's fault.

Delete this file in the commit that lands L-21's index row.
