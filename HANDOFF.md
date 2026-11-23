# Handoff (2026-11-23, 05:40, the end of the second night)

Landed on main tonight, each alone, CI green, by tree: A-10 (f1a4fd7),
L-2 (6442c8f), P-1b (427ec6a), the subnormal reading fix (80db71d),
A-11 (b837c23), A-12 (23b5d2a), L-3 (c2c14f4); the cards L-3 and L-3b
written out (d01c389). The night's lessons are in docs/Maintaining.md,
"Working habits that paid" (its last six items).

1. **A-13** (the limits page) on `origin/work/A-13`: every step done
   (the card's notes and "Organiser's answers" say how), rebased onto
   main after L-3 as one commit, its CI started at 05:30. If that run is
   green, land it by tree (`git diff --quiet origin/work/A-13 main --
   . ':!docs/Plan.md' ':!HANDOFF.md'` after the cherry-pick); its own
   commit already marks its index row done. If red, fix on the branch.
   Not run: `verify` on the four presets other than the default,
   `components` and `abi`; `mkdocs build --strict` (CI's `docs` runs it).
2. **P-4** (allocation) on `origin/work/P-4`, 47f6aa0, CI green: the
   measurement (Linux) and candidate (0) are done: `cf_wipe` is memset
   through a volatile function pointer, -25% instructions on a 16 MiB
   load, -72% on a tiny document. Left, in the card's notes: step 3
   (`bench/bench.c`'s builder path and abandoned bytes), candidates (2),
   (1), (3) against 47f6aa0 as the new "before", the Windows (MSVC)
   table, `docs/Performance.md`, then land. The profiling VM's driver
   (`~/cf-p4-drv/drv.c` on `ssh cfvm`) is what step 3 puts in the tree.
   The branch is cut from 8ef51e9: rebase it onto main first.
3. **The integration check**, once 1 and 2 are on main: on main, the
   full Linux verify (`CF_EXTRA="components;shared;conformance" python
   scripts/work/work.py verify main full`), `work.py win`, main's CI
   wholly green but for the style gates, and a read-only review of the
   two nights' changes together (`git diff 0b4b97a..main`) for what each
   card's own CI could not see: two cards changing one function
   differently, CHANGELOG or Decisions entries twice, the Plan index, the
   `EXTRA` and `TIER=2` lists in `scripts/verify.cmake` (four cards
   edited them; `profiles` and `sizes` are not in the `TIER=2` list:
   decide whether a release runs them). Fix what it finds first.
4. At the end of the group: `style-1.8.0` (docs/Plan.md, "Code first,
   style at the group's end"). Known red: `gate.cmake.lint` on main
   (`scripts/generate/apiref.cmake:219` and other long lines). Then the
   1.8.0 tag, which is the maintainer's.
5. L-3b (Frama-C EVA) waits for 1.9.0; P-2 and P-3 for 2.4.0.

On the maintainer's workstation only (nothing needed elsewhere): the
worktrees under `.claude/worktrees/` and the Codex desk
(`.claude/worktrees/deputy/`); every branch they hold is pushed. Codex's
5-hour window ran out at 03:20; it did the subnormal fix, A-12 and half
of A-13, which Claude finished.

Delete this file in the commit that finishes step 3.
