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

3. On main (green full run 46127137724 at 8237384, 2026-11-28): "Open
   2.1.0", L-28 (D-236), K-9k (D-239), `cf_error_place_at` promoted
   (D-89), the size record and README's table after M-11b, gate.cert
   holding clang-tidy names only at version 22 or newer. Card fixes after
   it: M-35 (card ids as links, the maintainer's ask), ready.py reading
   indented fences, open cards as CF_EXPERIMENTAL(2, 1).
   The maintainer (2026-11-28): features first; cards land in groups
   (several on batch, each with its local ACCEPT, then one full run).
   Group now: L-25 (with a worker in D:/cf-work/L-25), M-35, C-2a, C-8a,
   C-9, C-10a, F-1b; then Y-42, Y-1 and the qemu cards L-17, L-19a, L-19b
   (L-17-m opens after L-17 is green). C-7 and C-11 need each other: one
   of the two `needs` is wrong, settle it before either.
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
