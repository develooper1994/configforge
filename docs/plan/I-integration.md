# I - what the cards got wrong together

Cards land one at a time, each with its own green CI; what only two
cards together get wrong is found by the integration check (HANDOFF.md,
2026-11-23: a read-only review of `git diff 0b4b97a..main` at 5e0c184,
and the full Linux verify, which stopped at `gate.cmake.lint`, the
style unit's).

## I-1 - the 1.8.0 group's integration findings

* **id**: I-1
* **goal**: the thirteen findings below are fixed, each with the test
  or check named, and the full Linux verify (`CF_EXTRA="components;
  shared;conformance" python scripts/work/work.py verify main full`)
  ends green.
* **why**: each finding is a place where one card's change and
  another's disagree; no single card's CI could see it.
* **needs**: style-1.8.0 (`work/style-1.8.0`: `gate.cmake.lint`; it edits
  `scripts/generate/limits.cmake` and `scripts/verify.cmake` too, so this
  card starts from it)
* **size**: medium
* **decided** (one line per finding; file:line at 5e0c184):
  1. `scripts/verify.cmake:119-144`: a release (`TIER=2`) runs `profiles`
     and `sizes` too; `scripts/README.md:11`'s list matches the script's
     (with `infer` and `conformance`).
  2. (done in the commit after 0225ace) `docs/sizes.txt` is missing, so `scripts/check/sizes.cmake:208-215`
     compares nothing: the manager commits the first one from CI's
     `sizes` job artifact and regenerates the README's matrix (A-12's
     Organiser's answer).
  3. `scripts/verify.cmake:273-328,463-470` with
     `scripts/check/sizes.cmake:140-148`: under `SOURCE=index` the
     measured tree's identity is taken with `git rev-parse HEAD` inside
     the staged copy, which has no `.git`: pass the staged tree's
     identity to the script (`-DTREE=<id>`). Test: `verify.cmake
     -DSOURCE=index -DEXTRA=sizes` on a clean tree.
  4. `scripts/verify.cmake:489-494`: `profiles` skips `server` when the
     SQLite header is missing, but `cmake/profiles/server.cmake:10-12`
     turns SQLite off: the SQLite condition applies to `full` only.
  5. `lib/config/tests/test_limits.c:77-99`: a build that changes one
     limit (A-11 allows it, `-DCF_MESSAGE_MAX=...`) fails the page
     comparison. The test is told by CMake whether the build is the
     default (`-DCF_LIMITS_DEFAULT=1` from the build when no limit is
     overridden), not by counting differing rows. Test: a build with
     only `CF_MESSAGE_MAX` changed passes and prints its skip line.
  6. `README.md:217` and `docs/Limits.md:151-160`
     (`scripts/generate/limits.cmake:386-412`): a target without a CI job
     (WASI, run locally) is listed under its own "run locally" heading
     in both, generated from `docs/targets.txt` the same way.
  7. `docs/Limits.md:37` (`limits.cmake:213`): `cf_error`'s buffers are
     `CF_KEY_MAX`, `CF_MESSAGE_MAX` and `CF_SOURCE_MAX`
     (`cf_error.h:54-58`), not `CF_PATH_MAX`: the generator names them.
  8. `docs/Limits.md:34,92` (`limits.cmake:159,364`): the 31-level
     nesting rule is the configuration layer's; the page says so and
     points to D-109 for each codec's own limit.
  9. **A fault**: `lib/serialize/src/json.c:641,662`,
     `lib/serialize/src/toml.c:1267,1285` with `lib/text/src/track.c:17-18`:
     when the sink refuses after a multi-line string, the opening's place
     is computed from the current line's start, and A-10's `where.col`
     can wrap. Use the saved opening line and column. Test first in
     `lib/config/tests/test_reason.c` (or the serialize tests): a
     two-line string the sink refuses, `where` pointing at its opening
     quote; see it fail.
  10. `docs/Architecture.md:381-382` and `docs/plan/A-architecture.md:584,
      900,1021-1024`: the header check and the detection check are
      `gate.semgrep`'s rules now (L-3); the text names them.
  11. `.github/workflows/ci.yml:363-375`, `docs/Dependencies.md:88`,
      `CHANGELOG.md:64-69`: the `osv` job's third lockfile (semgrep's) is
      named in all three; the CHANGELOG's `EXTRA` list includes
      `valgrind`.
  12. `CHANGELOG.md:131-160`: the YAML item that says behaviour did not
      change but describes a new refusal is moved under `### Fixed` and
      says what is refused now; the reading fix near DBL_MIN (80db71d)
      gets its own `### Fixed` item.
  13. `docs/plan/P-allocators.md:33`: `D-76` is cited and never defined
      in `docs/Decisions.md`: cite the decision it means (D-03 or D-07,
      the arena), or define D-76 if it is a new one.
* **steps**: 1. `git switch -c work/I-1` from `work/style-1.8.0` (or main,
  once that has landed). 2. Finding 9 first (the fault, test first). 3. 3,
  4 and 5 (scripts and test; each with its check). 4. 6, 7, 8 (the
  generator; regenerate `docs/Limits.md` and the README on Linux). 5. 1,
  10, 11, 12, 13 (lists and documents). 6. The full Linux verify above.
  7. ACCEPT.
* **accept**: ACCEPT-LINUX (the full verify above), ACCEPT-CI.
* **stop if**: a finding turns out not to be one (write why under
  "notes"); finding 9's fix needs a change to `cf_error`'s public
  layout.
* **notes**: (empty)
