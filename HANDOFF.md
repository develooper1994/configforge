# HANDOFF (2026-11-28, morning)

State: the plan runs from docs/Plan.md's generated index (order:
docs/plan/Layout.md). No TO DECIDE is left in any card; only nine
account-bound cards stay parked (L-17-m, L-19-m, P-15-m, M-19-m, M-23-m,
M-25-m, M-29-m, M-30-m, G-9-m). The maintainer's standing approval and
release rights: docs/Plan.md "For the maintainer". 1.9.0 is released;
publishing to crates.io and PyPI is deferred (M-23-m): do not chase it.

## In flight

1. On `batch` (D:\cf-work\batch), not yet on main: K-9j (78d7f39),
   M-11b (08e6470, `cf_error` owns its text, D-176), M-13 (289121d,
   docs/Migrating-2.0.md, D-178), a Batch fix (c16d7cb: cppcheck's
   variable scope in error.c; Infer's BUFFER_OVERRUN_L2 through
   `text_put` listed in .infer-ignore.txt), then K-9i (922a272, local
   only until the run below is green).
2. Full run 46108613513 on batch at c16d7cb. Green: `git push origin
   c16d7cb:main`, then push batch (K-9i rides the next run). Red: fix on
   batch ("Batch: ..."), run again. yamldiff is exploratory.
3. K-9h with a worker in D:\cf-work\K-9h (branch work/K-9h, D-238).
4. With M-11b and M-13 green, every 2.0.0 card is done but K-9h, F-6 and
   F-7 (beside-cards): tag v2.0.0 once they land and a full run is green
   (docs/Plan.md "For the maintainer"; never publish to crates.io/PyPI).
5. The organiser's local check before each full run (docs/Plan.md "The
   loop", step 11): format, codemap, apiref, every `gate.*`, default
   build with every test, clang, `none`, `ubsan-strict`, `-DCF_CLANG_TIDY=ON`,
   CI's iwyu line, the MISRA record, and (new, 2026-11-28) the `analyze`
   preset (cppcheck: its style findings break the build). Scripts that do
   it are D:\cf-work\full-check.sh and analyze-check.sh (not in the
   repository; their commands are the list above).

## Next

6. After 2.0.0: the safety-critical cards L-28 (CERT matrix), L-25
   (MC/DC), L-26a, L-26b (traces), L-27 (safety manual, after L-12c),
   D-233 to D-236; then the 2.2.0 module (K-8, K-8b, ...).
7. Measure the batch flow (cards a day, red full runs, time to fix) into
   docs/Maintaining.md "Working habits that paid".
8. Codex's allowance returns 2026-11-29 09:47: give it most cards
   (scripts/work/codex-card.sh).

## Budget

One worker at a time past half the 5-hour window. At 99% weekly:
everything pushed, this file current, and give the maintainer one prompt
for a session on another account ("Read CLAUDE.md, then HANDOFF.md, and
continue").

Delete this file when the steps above are done and nothing is in flight.
