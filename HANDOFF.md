# Handoff (2026-11-23, evening)

On main since the night: the rules without model names and with adaptive
effort (CLAUDE.md "Roles"); the token measurement and its rules
(docs/Maintaining.md "Where the tokens go", CLAUDE.md "Habits");
cards L-6 (a map of the tree, a brief per card) and I-1 (the
integration check's findings); `docs/sizes.txt`.

1. **style-1.8.0** on `work/style-1.8.0` (worktree
   `.claude/worktrees/style`): Codex is fixing `gate.cmake.lint`'s 141
   findings (A-11..A-13's CMake). When its report says DONE: commit,
   run `gate.cmake.lint` and `gate.limits|matrix|profiles` on Linux,
   push, CI, land by tree.
2. **I-1** (docs/plan/I-integration.md) after 1, from its branch: 12
   findings left (finding 9 is a real fault: a refused multi-line
   string's place). Give it to Codex with its card; the manager commits.
3. **P-4** on `origin/work/P-4` (b871526): step 3 and the Windows table
   committed; candidate (2)'s question answered in the card; Codex is
   doing (2), (1), (3) in `.claude/worktrees/p4`. Commit what it keeps
   (each candidate in its own file), CI, then the card's last steps and
   land.
4. Then the full Linux verify on main (`CF_EXTRA="components;shared;
   conformance" python scripts/work/work.py verify main full`), `work.py
   win`, and the 1.8.0 tag, which is the maintainer's.
5. L-6 is open (needs L-5, done): a good first card for Codex.
6. L-3b waits for 1.9.0; P-2 and P-3 for 2.4.0.

Codex's allowance is the larger: it carries the cards (prompts in the
style of the last ones: the card, the state, what to report; `-m` the
larger model for a large card, the smaller otherwise; `-c
model_reasoning_effort=`). Claude briefs, checks, commits and lands.

Delete this file in the commit that finishes step 4.
