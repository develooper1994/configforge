# HANDOFF (2026-11-28, 17:00 UTC)

State: the plan runs from docs/Plan.md's generated index (order:
docs/plan/Layout.md). No TO DECIDE is left in any card; only the
account-bound cards stay parked. The maintainer's standing approval and
release rights: docs/Plan.md "For the maintainer". Publishing to crates.io
and PyPI is deferred (M-23-m; the release's `crates` job is skipped while
the variable PUBLISH_CRATES is unset): do not chase it.

## Done today

1. 2.0.0 released (tag v2.0.0 on de9fa33); 2.1.0 opened.
2. On main (807685c, green full run 46166195752 on c16244e): L-25, M-35,
   K-9k, L-28, and the first 2.1.0 feature group: C-2a (units, D-113,
   D-222), C-8a (misreads, D-121), C-9 (migration, D-122), C-10a
   (cf_serve_report, D-123), F-1b (cf_emit_file, D-224), with the group's
   check fixes (stack frame, oom list, tidy, iwyu, cppcheck, ubsan in
   serve.c, MISRA records, MSVC /analyze, musl and check.mcdc, yamldiff).

## How a group runs (the maintainer, 2026-11-28)

Features first. Write the group's code first: each worker builds the
default preset and runs only its card's tests and the `gate.*` tests. Then
verify the group once: `sh scripts/work/check-all.sh` in WSL (every lane in
parallel on a copy of the tree; MISRA records with
D:/cf-work/misra-write.sh when counts move; README via
scripts/generate/matrix.cmake when docs/sizes.txt moves, from CI's `sizes`
artifact), then one full run (`gh workflow run ci --ref batch -f
tier=full`), fix, `git push origin <green hash>:main`. The yamldiff job is
not required: a red one is a new difference to document or fix, not a
block.

## In flight

3. Group 2: Y-42 with a worker in D:/cf-work/Y-42 (branch work/Y-42), then
   Y-1 (ready). C-11, C-7, L-17, L-19a, L-19b fail ready.py: cited lines
   moved, and C-11, L-17 and L-19b have an `either` or a `?` in decided
   that the organiser settles. L-19b needs L-17; L-17-m (three required
   checks on main) opens after L-17 is green.
4. yamldiff.known has a new `to decide` rule (`&5?\n 1&:`, "mapping keys
   are not allowed in this context"): examine its cause in a later card.

## Next

5. Beside-cards later: L-26a, L-26b (traces), L-27 (after L-12c).
6. Codex's allowance returns 2026-11-29 09:47: give it most cards
   (scripts/work/codex-card.sh).

## Budget

One worker at a time past half the 5-hour window, none past ~70%. At 99%
weekly: everything pushed, this file current, and give the maintainer one
prompt for a session on another account ("Read CLAUDE.md, then
HANDOFF.md, and continue").

Delete this file when the steps above are done and nothing is in flight.
