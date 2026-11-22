# Handoff (2026-11-22, usage limit)

main 0b4b97a: PR 11's fixes (a0e400b) and A-6 (0b4b97a) landed; every
open card from A-7 to A-13, P-1b and L-2 was read against the tree and
corrected (89bfc2d, e5b8929, ed0e63b, c8150fe). Two cards are part done.

1. **A-7** on `origin/work/A-7` (9 commits, last 094de42 "refs.c includes
   what it uses"). Its agent stopped while fixing what include-what-you-use
   or verify found. Check out the branch, read docs/plan/A-architecture.md
   "## A-7" and `git log origin/main..origin/work/A-7`, finish the card's
   remaining steps, run `python scripts/work/work.py format` and
   `CF_EXTRA="conformance;components" python scripts/work/work.py verify A-7 full`,
   then `work.py win`. Push and wait for CI. Done when CI is green and the card
   is squashed onto main (the loop, step 10).
2. **L-2** on `origin/work/L-2` (fe81665, one work-in-progress commit holding
   all of the agent's uncommitted edits). Its state is unknown: read the diff
   against the card in docs/plan/L-tools.md, then continue from the first step
   it has not finished. It is CI-only: push, read the failing job, fix. Format
   before each push (`work.py format`; if CI still names gate.format.c, apply
   the run's `format-patch` artifact).
3. Then the index in order: A-8 (it needs A-7), P-1b (open), and the rest.
   Each card's brief: CLAUDE.md, "The brief every card's subagent gets", plus
   "format before the push" (verify does not run the format gate).

Delete this file in the commit that finishes step 2.
