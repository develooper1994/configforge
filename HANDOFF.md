# Handoff (2026-11-22 night, kept current until Monday 06:10)

main: A-6, A-7, A-8 and A-9 landed (0b4b97a, fca866c, c3bba84, a4ea9e1 +
bc7d29c). L-2, P-1b, A-10 and A-11 are in progress on their branches;
A-12 and A-13 follow A-11. P-4 (allocation) is written and waits for P-1b.

1. **P-1b** on `origin/work/P-1b`: the card is built (bench/compare,
   scripts/bench, the bench workflows, docs/Performance.md, D-111). Left:
   CI green on the branch tip; then the baseline: download the run's
   `bench-quick` artifact, `python scripts/bench/gate.py --write <artifact
   file> bench/baseline/instructions.json`, commit, push; record
   bench-gate's time at 4 MiB in the card's notes (over ten minutes: cut
   further, as the card says). Then land (docs/Plan.md, "The loop", 10).
2. **L-2** on `origin/work/L-2`: CI-only card; continue from the branch's
   last commit and its card notes; land when every job is green.
3. **A-10** and **A-11**: carried out by Codex, uncommitted, in
   `.claude/worktrees/a10` and `.claude/worktrees/a11` on the maintainer's
   workstation; when they are pushed, their branches are `work/A-10` and
   `work/A-11`. The questions Codex stopped on are answered in each card
   under "Organiser's answers, 2026-11-22". On another machine: if the
   branch is missing, the card starts again from its card.
4. **A-12, A-13** after A-11; then **P-4** after P-1b. P-2 and P-3 wait for
   2.4.0 and L-3 for L-2, as the index says.
5. At the end of the group: `style-1.8.0` (docs/Plan.md, "Code first, style
   at the group's end"), then the 1.8.0 tag, which is the maintainer's.

How the work was done tonight, and what to keep doing, is in
docs/Maintaining.md, "Working habits that paid" (format before a push,
API.md last, iwyu in one pass, `gh run watch --interval 60`, landing a
stacked card by tree, Codex as a second worker).

Delete this file in the commit that finishes step 2.
