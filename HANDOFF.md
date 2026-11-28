# HANDOFF (2026-11-28, 05:50 UTC)

State: the plan runs from docs/Plan.md's generated index (order:
docs/plan/Layout.md). No TO DECIDE is left in any card; only the
account-bound cards stay parked. The maintainer's standing approval and
release rights: docs/Plan.md "For the maintainer". Publishing to crates.io
and PyPI is deferred (M-23-m; the release's `crates` job is skipped while
the variable PUBLISH_CRATES is unset): do not chase it.

## Done today

1. 2.0.0 released: tag v2.0.0 on de9fa33 (main), release workflow
   46113423729. 2.0.0 holds K-6a, K-9d, M-11a, K-9e, K-9f, K-9g, K-9b,
   K-9j, M-11b (`cf_error` owns its text, D-176), M-13
   (docs/Migrating-2.0.md, D-178), K-9i, K-9h (D-238). Last green full
   run: 46112081619 (e0f9e40).
2. After the tag: 2.1.0 opened (95033fe); the beside-cards still open moved
   to 2.1.0 (d2b1459).

## In flight

3. On main: everything through bb92ccf (green full run 46127137724).
   On `batch` (D:/cf-work/batch), not yet verified: L-25 (79296cf, MC/DC,
   D-233), M-35 (22a3947, card ids as links: scripts/work/links.py), C-11's
   needs without the cycle. C-2a with a worker in D:/cf-work/C-2a
   (branch work/C-2a).
   The maintainer (2026-11-28): features first; write a group's code first
   (each worker builds the default preset and runs only its card's tests),
   then verify the group once: the full local check (D:/cf-work/
   full-check.sh: default, clang, none, ubsan-strict, tidy, iwyu, analyze,
   MISRA with WRITE=ON when counts move, README via
   scripts/generate/matrix.cmake when docs/sizes.txt moves), then one full
   run, fix, `git push origin <green hash>:main`.
   This group: C-2a, then C-8a, C-9, C-10a, F-1b (all pass ready.py),
   then the group's check. Next group: Y-42, Y-1, C-11, C-7 (refresh
   C-11's cited lines first), the qemu cards L-17, L-19a, L-19b (L-17-m
   opens after L-17 is green: three required checks on main).
4. Release 46113423729 was green (crates skipped). If one is ever red: read its failed job, fix
   on batch, and re-tag only by deleting and pushing v2.0.0 again once the
   fix is on main (no release was made then); if green, nothing to do.

## Next

5. K-9k, then L-25 (MC/DC), L-26a, L-26b (traces), L-27 (after L-12c),
   then the index's order (2.1.0 module cards: C-2a, C-8a, ...).
6. The organiser's local check before each full run (docs/Plan.md "The
   loop", step 11; worker-notes): format, codemap, apiref, every `gate.*`,
   default with every test, clang, `none`, `ubsan-strict`,
   `-DCF_CLANG_TIDY=ON`, CI's iwyu line, the MISRA record (regenerate with
   WRITE=ON when counts move), and the `analyze` preset.
7. Measure the batch flow into docs/Maintaining.md "Working habits that
   paid": 2026-11-28 had three red full runs (MSVC/test_reason; clang-tidy,
   cppcheck and Infer after M-11b; clang-tidy and `-fcallgraph-info` after
   K-9h), each fixed within the hour.
8. Codex's allowance returns 2026-11-29 09:47: give it most cards
   (scripts/work/codex-card.sh).

## Budget

One worker at a time past half the 5-hour window, none past ~70%. At 99%
weekly: everything pushed, this file current, and give the maintainer one
prompt for a session on another account ("Read CLAUDE.md, then
HANDOFF.md, and continue").

Delete this file when the steps above are done and nothing is in flight.
