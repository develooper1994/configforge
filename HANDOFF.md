# HANDOFF (2026-11-26, evening)

State: every card has a row in docs/Plan.md's index (5586a89), in the
maintainer's order of groups; parked cards are listed again under
"Waits on the maintainer". The maintainer decided (2026-11-26, relayed
from a side session, written into docs/Plan.md "Read cold once, just
before the group opens"): no more whole-file cold reads in advance.
`work.py land`'s index update (`plan_index`, and `land.sh`) wrote over
the Needs column since the Version column was added; fixed and landed
before any card (see `git log --grep plan_index`).

Codex has no allowance until 2026-11-29 09:47; Claude carries on.

## Steps

0. The maintainer approved D-220 and D-221 (docs/plan/Y-asked.md,
   "Approved by the maintainer, 2026-11-26"; ee46692). Have one worker
   (sonnet, high effort; it did not write the cards) rewrite Y-15 to
   Y-20, and Y-21-m to Y-23-m where they touch it, to follow D-221:
   Y-15 split into Y-15a and Y-15b; the answer under each card's
   "Organiser's answers"; the index rows and "Waits on the maintainer"
   rows in docs/Plan.md updated (Y-15's TO DECIDE 1, 4, 5 answered; any
   other TO DECIDE stays). Docs only: commit straight to main.
   Then, the same way: C-2a, C-2b and C-1b rewritten to D-222 (the open
   `cf_unit`, under C-2a's "Organiser's answers" in docs/plan/C-change.md;
   810a100), their rows re-marked; and K-9a, K-9b written as full cards
   and K-9c from docs/plan/K-correctness.md "To be written" (1.9.0,
   decisions from D-224), and K-6a, K-6b and Y-28 rewritten to D-223 (one
   error model, written there), read cold, index rows after K-8. More answers may come from the
   maintainer's side session: write each into its card when it comes.
1. Carry the first open card through the loop (docs/Plan.md, "The
   loop"): C-1a (`docs/plan/C-change.md`, read cold 2026-11-26), with
   `python scripts/work/brief.py C-1a` as its worker's brief; then the
   next open row. One card at a time. A group not yet read cold (X, Y,
   M) gets its one cold read when its first card comes up.
2. Later, as a card of group L (not written yet): a gate that checks each
   card's `file:line` citations still point at the symbol they name.

Delete this file in the commit that lands C-1a's index row.
