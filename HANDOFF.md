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

3. On `batch` ahead of main, no full run yet: (the worktree is D:/cf-work/batch)
   "Open 2.1.0", L-28 (CERT matrix, D-236), K-9k (a3bc664, strerror
   replaced, D-239) and 3411f27 (`cf_error_place_at` promoted to CF_API:
   opening 2.1.0 made gate.abi refuse a 2.0 experimental name, D-89).
   Next: the organiser's local check (item 6; regenerate the MISRA record
   with WRITE=ON, platform files moved), a full run, then
   `git push origin <green hash>:main`.
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
