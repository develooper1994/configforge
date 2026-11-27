# HANDOFF (2026-11-27, afternoon)

State: stage 0's work is under way from docs/Plan.md's generated index
(docs/plan/Layout.md is the order). Landed today: L-20, L-21, L-22, L-23,
L-24, K-11a, K-11b, L-18a, F-1a, K-9c, Y-2. Every worker gets its brief
(`python scripts/work/brief.py --force <id>` once step 0 is in the brief)
and docs/plan/worker-notes.md. The maintainer's standing approval and
release rights: docs/Plan.md "For the maintainer".

## Steps

1. K-11c (the table of features by platform) was being carried in the
   worktree D:\cf-work\K-11c on `work/K-11c`. If the branch exists, read it,
   finish or land it by hand (squash onto main, tree check, push,
   `python scripts/work/index.py --done <id> <hash>`, commit "Plan: <id> done
   (<hash>)", push, delete the branch; a full-tier run once per card that
   touched code or CMake).
2. Then the file layer's rest, one at a time: Y-6 (ready), Y-5, Y-3 (its card
   still says "rewritten to": fix in step 0), Y-8 (needs Y-3). Then K-6a,
   K-6b, K-9a, K-9b (to be written), then row 0.9 (M-10, M-12, M-11a, M-11b,
   M-13: the sizing modes, which the maintainer has long waited for).
3. When stage 0's 1.9.0 rows are done: release 1.9.0 (the organiser may tag;
   the notes now list how each archive was built, L-23, and the tag publishes
   the Rust crate, L-24, with the secret CARGO_REGISTRY_TOKEN the maintainer
   set). Read the release's notes and the crate on crates.io afterwards.
4. Budget: the weekly window was at 63% on 2026-11-27 with six days left:
   at most two workers at once, smaller models for mechanical work.
5. Open findings: CodeQL's critical alert 22, artifact poisoning in
   .github/workflows/bench.yml:59 (a card to write); a skipped `matrix` job
   reports one check named `matrix` on a pull request (the ruleset wants the
   three `matrix (<os>)` names; pushes to main bypass it).

Delete this file in the commit that lands the last stage 0 1.9.0 card.
