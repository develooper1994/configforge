# L - the tools, continued

The tools group after L-11 (mutation testing, `docs/plan/L-tools.md`): the
rest of the MISRA work (the public headers' zero, the matrix of every rule,
the compliance report), verify built from what CI builds, EVA over the float
half of `numeric`, the comparison with whole configuration libraries, the
sizes and work predicted and checked, and the targets the suite does not run
on yet (big endian, 32 bit, Arm, a Cortex-M). Cards ending in `-m` need the
maintainer's own access and are parked (docs/Plan.md, "Waits on the
maintainer"). Each card is read cold against the tree before it opens;
every `file:line` is of main at 341d2b5.

Decisions D-151 to D-158 are written in the cards that decide them; the
worker copies each into `docs/Decisions.md` (the table at its top and the
section at its end).

## L-12a - The public headers with no record at all

* **id**: L-12a
* **goal**: no `include/` file of `lib/` has a line in `docs/compliance/deviations.txt` of kind `backlog` or `deviation`, and `gate.misra` refuses either in a header.
* **why**: a user's checker reads `include/` inside every file of theirs that includes it (`docs/Compliance.md`, "The public headers come first"); today 33 record lines (131 reports) of `backlog` sit in 18 headers, so the claim "the headers are clean" cannot be made, and the compliance report (L-12c) cannot carry it.
* **needs**: nothing
* **size**: large (131 reports in 15 rules, each a judgement; one commit a rule group)
* **decided**:
  * **D-151 (to copy into docs/Decisions.md, "The public headers take no backlog and no deviation")**: *Question*: a header's report may be fixed, or recorded. *Options*: (1) fix every header report; (2) allow `deviation` in headers; (3) fix, or record as `tool` only where the addon is wrong. *Decision*: option 3. A `deviation` in a header is a deviation every user must adopt (`docs/Compliance.md`, "Deviation records": the kind table says "refused in a public header", `cmake/gates/GateMisra.cmake:142-143`); `backlog` there is refused by this card, the new rule beside it. A `tool` line stays allowed: the code complies and the addon is wrong, so nothing is adopted, and the line's reason names the report. *Compatibility*: none for users; a header edit that is a rename is not allowed (the names are the interface): a report that can be removed only by renaming a public name is a TO DECIDE for the maintainer and stays unfixed, so the gate cannot pass until it is decided.
  * **The reports** (reports a rule, from the record at 341d2b5): `cf_port.h` 5.9 x8, 10.4 x15, 10.7 x2, 12.1 x8, 17.1 x1, 20.8 x1, 20.10 x2; `cf_source.h` 12.2 x5, 19.2 x2; `cf_serialize.h` 12.2 x8; `cf_encoding.h` 12.2 x6; `lanes.h` 12.2 x1; `cf_define.h` 12.3 x2; `cf_yamlparse.h` 12.3 x1; `cf_text.h` 12.3 x1; the four `.def` files 15.5 x40 (`cf_code.def` 10, `cf_reason.def` 20, `cf_type.def` 5, `cf_outcome.def` 5) and 20.5 x8; `cf_entry.h` 18.5 x1; `cf_value.h` 19.2 x2, 20.1 x1, 20.5 x2; `cf_error.h` 20.1 x2, 20.5 x4; `cf_keyspace.h` 20.1 x1, 20.5 x2; `configforge.h` 20.7 x4; `cf_version.h` 20.10 x1. Rules 12.1, 12.2, 12.3, 10.4 are the arithmetic and shift reports of `static inline` helpers and macros; 15.5 in a `.def` file is an X-macro list the addon reads as a function body; 20.x are preprocessor rules (include before code, `#undef`, macro arguments in parentheses, `#`/`##`).
  * **The order, one commit a group**: (1) 20.1, 20.5, 20.7, 20.8, 20.10, 19.2 (the preprocessor and the union); (2) 12.1, 12.2, 12.3, 10.4, 10.7, 5.9, 17.1, 18.5 (the inline helpers); (3) 15.5 (the `.def` files). A report is fixed by the smallest change to the header that keeps the public meaning: parentheses, an explicit cast, a named constant, a comment; never a change of a public name, type or value. A `.def` report that is the addon reading an X-macro list as code is a `tool` line, its reason "the addon reads the X-macro list as a function body"; it stays only after the worker has shown with `cppcheck --addon=misra` on a tiny file that the same list written as a plain `enum` gives no report.
  * **The gate**: `cmake/gates/GateMisra.cmake:142-143` already adds the message ` a public header takes no deviation;` to the `bad` variable; beside it, `kind STREQUAL "backlog" AND file MATCHES "/include/"` adds ` a public header takes no backlog;`. The self-test `tests/misra/compare.cmake` gets one case: a record line `5.9 | lib/x/include/y.h | 1 | backlog | ...` fails with that text; a `tool` line for the same file passes.
  * **The records move with the code**: after each commit, regenerate with the command the header of `docs/compliance/deviations.txt` prints (`cmake -DSRC=. -DBIN=<misra build> -DDEFINES=<compiler macros> -DWRITE=ON -P cmake/gates/GateMisra.cmake`), review `git diff --stat docs/compliance/deviations.txt` (only `include/` lines drop; no other line changes by more than the lines a header edit moved), and keep the file's `tool` and `deviation` reasons.
  * **Stays out**: the `.c` files' backlog (other cards take a rule at a time); renaming anything public; MISRA C++ for the wrapper.
* **files**: `lib/port/include/configforge/cf_port.h`, `lib/config/include/configforge/cf_source.h`, `cf_define.h`, `cf_entry.h`, `cf_keyspace.h`, `configforge.h`, `cf_version.h`, `cf_outcome.def`, `lib/serialize/include/configforge/cf_serialize.h`, `cf_yamlparse.h`, `lib/text/include/configforge/cf_encoding.h`, `cf_text.h`, `lib/lanes/include/lanes/lanes.h`, `lib/base/include/configforge/cf_value.h`, `cf_error.h`, `cf_code.def`, `cf_reason.def`, `cf_type.def`; `cmake/gates/GateMisra.cmake`, `tests/misra/compare.cmake`, `docs/compliance/deviations.txt`; last: `docs/Compliance.md`, `docs/API.md` and `docs/Codemap.md` (generated).
* **steps**:
  1. `git switch -c work/L-12a` from `main`; on Linux (WSL, a copy off `/mnt`) `cmake --preset misra && cmake --build --preset misra && ctest --test-dir build-misra -R gate.misra` is green before any change (exit 0).
  2. Test first: add the `backlog`-in-a-header case to `tests/misra/compare.cmake`. **See it fail**: `ctest --test-dir build-misra -R misra.compare` exits 1 (the gate does not know the rule yet).
  3. Add the rule in `GateMisra.cmake`; `ctest -R misra.compare` exits 0; `ctest -R gate.misra` now fails naming the 33 lines (see it fail on the real record); keep the first line of its output for the commit message.
  4. Group (1): for each report, `cppcheck --addon=misra --enable=all --inconclusive -I lib/*/include <header>` shows the line (the gate's own invocation is `cmake/gates/GateMisra.cmake:177-230`); fix it; rebuild with `cmake --build --preset default && ctest --preset default`; regenerate the record; commit "L-12a: <group>, <n> reports".
  5. Groups (2) and (3) the same. After each, `ctest --preset default` and `cmake -P scripts/generate/apiref.cmake` are clean.
  6. When `grep "/include/" docs/compliance/deviations.txt | grep -vc "| tool |"` prints 0, `ctest -R gate.misra` exits 0.
  7. Documents; `python scripts/work/work.py format`; `cmake -P scripts/generate/codemap.cmake` then `cmake -P scripts/generate/apiref.cmake` last; ACCEPT.
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI (the `profiles (misra)` job included), and `grep "/include/" docs/compliance/deviations.txt | grep -vc "| tool |"` prints 0.
* **stop if**: a report can be removed only by changing a public name, a type's size, an enumerator's value or the ABI (`gate.abi` or `gate.symbols` fails): leave it, write the header, line and rule under notes, and hand the card back (a TO DECIDE for the maintainer); a fix moves a report instead of removing it (revert it); three failures on the same report.
* **docs**: `CHANGELOG.md`, "## 1.9.0 — unreleased", "### Added": "The public headers carry no MISRA C:2012 report of kind backlog or deviation; gate.misra refuses one (D-151)"; `docs/Compliance.md` "Order of work", item 2 marked done and its "Open: 33 record lines" sentence rewritten; `docs/Roadmap.md`, "Critical-system standards, checked by machine" (stays 🟨; "the public headers' zero" struck from what waits); `docs/Decisions.md`: D-151 as decided above.

## L-12b - The matrix of every MISRA rule, made from the record

* **id**: L-12b
* **goal**: `docs/compliance/Matrix.md` lists every rule and directive of MISRA C:2012 (with Amendments 1 to 4) with its status for this library, is generated from the record, and `gate.misramatrix` fails when it differs from a regeneration.
* **why**: `docs/Compliance.md:29` says "the matrix and the public headers' zero are planned"; an assessor or a user's safety process asks "what of rule 8.13?" and today must read 613 record lines; and a rule the addon does not implement shows in no record at all, so silence looks like compliance.
* **needs**: L-12a (the headers' status is part of the matrix)
* **size**: medium
* **decided**:
  * **The rule list**: `docs/compliance/rules.txt`, lines `<id> | <kind> | <addon>`: id `D.n.m` for a directive (16: `D.1.1`, `D.2.1`, `D.3.1`, `D.4.1` to `D.4.14`) or `R.n.m` for a rule (143, `R.1.1` to `R.22.10`, Amendment 4's included), kind `directive` or `rule`, `<addon>` `yes` when the addon implements it and `no` when it does not. The numbers only: the standard's titles and categories are the publisher's text and are not copied (TO DECIDE below). The worker writes the list from the addon's own table, never from memory: `cppcheck --version` (the record says 2.19), `find / -name misra.py -path "*addons*" 2>/dev/null` for the addon, and `grep -o "def misra_[0-9_]*" <misra.py> | sort -u` lists the rules it implements (`misra_8_13` is rule 8.13); the rules the addon lacks are added with `no`. Check: `grep -c "^R\." docs/compliance/rules.txt` prints 143 and `grep -c "^D\." docs/compliance/rules.txt` prints 16; a `def misra_` the file lacks is a stop.
  * **D-152 (copy into docs/Decisions.md, "The MISRA matrix is generated, and says what no tool sees")**: *Question*: what does a matrix hold, and how does it stay true? *Options*: (1) a table written by hand; (2) a table generated from the record, a gate comparing; (3) the record extended with a clean-rule line for every rule. *Decision*: option 2, as `docs/Limits.md` is kept (`scripts/generate/limits.cmake`, `gate.limits`). A status is one of: `ignored` (the record's `ignore` line: judged where the program is whole), `backlog` (some `backlog` line for the rule: reports and files counted), `deviation` (some `deviation` line; the record's reason, its first sentence), `tool` (the addon is wrong), `clean` (the addon implements the rule and the record has no line for it), `not checked` (the addon does not implement it: neither the gate nor a tool reads it; the cell says "review" and, where another tool here covers it, which: clang-tidy through `.clang-tidy`, the compiler's `-Wall -Wextra -Werror`, CBMC or EVA). A rule with lines of two kinds shows the stricter first (`backlog`, then `deviation`, then `tool`). Option 3 was rejected: 159 lines of "clean" in the record would be a second place to keep in step. *Compatibility*: none.
  * **The generator**: `scripts/generate/misra_matrix.cmake`, CMake like `scripts/generate/limits.cmake` (`-DWRITE=<page>` replaces the page, `-DOUT=<file>` writes it elsewhere), reading `docs/compliance/deviations.txt` and `docs/compliance/rules.txt` only, so its output is the same on every machine.
  * **The page** `docs/compliance/Matrix.md`: a header (what the page is, cppcheck's version from the record, "generated by `scripts/generate/misra_matrix.cmake`; edit the record, not this page"), a summary table (the count of rules a status, over 159), then one table `| Rule | Status | Reports | Files | Note |` a group (the directives first, then Rule 1 to Rule 22).
  * **The gate**: `cmake/gates/GateMisraMatrix.cmake`, registered in `cmake/ConfigForgeGates.cmake` beside `gate.limits` (`:111-115`) as `gate.misramatrix`: it regenerates into `${BIN}` and fails, naming the first differing line, when the result differs from `docs/compliance/Matrix.md`. When `cppcheck` is on the path it also fails if `rules.txt`'s third column disagrees with the addon's `def misra_*` list (and prints the difference); without cppcheck it prints `gate.misramatrix: addon column not checked, no cppcheck` and passes that part.
  * **TO DECIDE (the maintainer)**: whether the matrix also carries each rule's category (mandatory, required, advisory) and title. *Options*: (a) numbers only, as decided above; (b) the maintainer adds `docs/compliance/categories.txt` from a copy of the standard they hold, and the generator reads it when it exists. *Recommendation*: (a) now: the category and title are the publisher's text, and a mandatory rule is already never deviated (`docs/Compliance.md`, "Deviation records"); (b) later, as a card of its own once the maintainer holds the document.
  * **Stays out**: MISRA C++:2023 and CERT's matrix (other parts of the same Roadmap entry); fixing a rule (a card that takes a rule lowers its count; the matrix shows it).
* **files**: `docs/compliance/rules.txt` (new), `scripts/generate/misra_matrix.cmake` (new), `cmake/gates/GateMisraMatrix.cmake` (new), `docs/compliance/Matrix.md` (generated), `cmake/ConfigForgeGates.cmake`, `tests/misra/CMakeLists.txt`, `tests/misra/matrix/` (new: `rules.txt`, `record.txt`, `expected.md`, `check.cmake`); last: `docs/Compliance.md`, `docs/Codemap.md`.
* **steps**:
  1. `git switch -c work/L-12b` from `main` (after L-12a lands).
  2. Write `docs/compliance/rules.txt` as decided; the two `grep -c` checks print 143 and 16.
  3. Test first: `tests/misra/matrix/` holds a three-rule fixture (one rule `clean`, one `backlog`, one `not checked`), its expected page, and `check.cmake` that runs the generator on it and compares; add `matrix.generate` to `tests/misra/CMakeLists.txt`. **See it fail**: `ctest --test-dir build -R matrix.generate` exits 1 (no generator).
  4. Write the generator; `ctest -R matrix.generate` exits 0. `cmake -DWRITE=docs/compliance/Matrix.md -P scripts/generate/misra_matrix.cmake`.
  5. Write the gate and register it; `ctest -R gate.misramatrix` exits 0. **See it fail**: edit one cell of `Matrix.md`, the gate exits 1 naming the line; restore.
  6. Check the summary against the record by hand once: the `backlog` row's rule count equals `awk -F' [|] ' '$4=="backlog"{r[$1]}END{print length(r)}' docs/compliance/deviations.txt`.
  7. Documents; format; `cmake -P scripts/generate/codemap.cmake`; ACCEPT.
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS (the gate is CMake-only and runs there), ACCEPT-CI; `ctest -R "gate.misramatrix|matrix.generate"` exits 0 in the default build.
* **stop if**: the addon names a rule the standard's numbering above lacks; the record has a rule `rules.txt` lacks (report both: one is wrong).
* **docs**: `CHANGELOG.md`, "## 1.9.0 — unreleased", "### Added": "The MISRA matrix (docs/compliance/Matrix.md): every rule with its status, generated from the record and held by gate.misramatrix"; `docs/Compliance.md:29` ("the matrix ... planned") rewritten; `docs/Roadmap.md` "Critical-system standards, checked by machine" (what waits: "the matrix of every rule" struck); `docs/Decisions.md`: D-152 as decided above.

## L-12c - The compliance report, attached to each release

* **id**: L-12c
* **goal**: a release carries `configforge-<version>-compliance.md`, made from the record and the matrix of the tagged commit, and the release workflow fails when it cannot be made.
* **why**: the Roadmap item's "Done when" is "the compliance report is attached to a release"; `docs/Compliance.md:215` allows saying "the compliance summary is attached to each release" only when it is.
* **needs**: L-12b
* **size**: small
* **decided**:
  * **D-153 (copy into docs/Decisions.md, "The compliance report is the record and the matrix, one file")**: the report is one Markdown file: a title with the version, tag and commit; "What was checked" (the preset `misra`, cppcheck's version from the record's `tool` line, the profile `cmake/profiles/misra.cmake`); the summary table of `Matrix.md`; the counts of the record by kind; every `deviation` and `tool` line (rule, file, count, reason) in full; the sentence "Nothing here is a certification: no qualified tool has run (docs/Compliance.md)". It is made by `scripts/generate/compliance_report.cmake` from the same two files, through `include()` of `scripts/generate/misra_matrix.cmake`, so it cannot say what the matrix does not. Rejected: the record and the matrix as two attached files (a reader has to join them) and a PDF (nothing here makes one).
  * **The release step**: in `.github/workflows/release.yml`, in the `amalgamation` job (`:68-77`), a step after "Amalgamation": `cmake -DVERSION="$v" -DTAG="$GITHUB_REF_NAME" -DCOMMIT="$GITHUB_SHA" -DOUT="dist/configforge-$v-compliance.md" -P scripts/generate/compliance_report.cmake`, so the file is in `dist/` and in the artifact `amalgamation`; `publish` downloads every artifact, attests `dist/*` and lists it in `SHA256SUMS` (`:200-216`) with no change there.
  * **A test that needs no release**: `compliance.report` in `tests/misra/CMakeLists.txt` runs the generator with `-DVERSION=0.0.0 -DTAG=v0.0.0 -DCOMMIT=0000000` into the build directory and fails when it exits non-zero or the file lacks "What was checked" or "Nothing here is a certification".
* **files**: `scripts/generate/compliance_report.cmake` (new), `tests/misra/CMakeLists.txt`, `.github/workflows/release.yml`; last: `docs/Compliance.md`, `docs/Codemap.md`.
* **steps**:
  1. `git switch -c work/L-12c` from `main`.
  2. Test first: add `compliance.report`. **See it fail**: `ctest --test-dir build -R compliance.report` exits 1 (no generator).
  3. Write the generator; `ctest -R compliance.report` exits 0.
  4. Add the release step; `actionlint .github/workflows/release.yml` exits 0 (CI `lint` runs it). If the job's runner has no `cmake` (read `release.yml:52-68`), add `sudo apt-get install -y cmake` to the step; stop if the job is not on an Ubuntu runner.
  5. Documents; format; codemap; ACCEPT.
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI; `ctest -R compliance.report` exits 0. The release step runs only at a tag (**CI-only**, the maintainer's tag): it is shown by the next release, `gh run list --workflow release` with `amalgamation` green and the file in the release's assets.
* **stop if**: the `amalgamation` job cannot run CMake without a change to what it builds (report what it has).
* **docs**: `CHANGELOG.md`, "## 1.9.0 — unreleased", "### Added": "Each release carries its MISRA compliance report (configforge-<version>-compliance.md)"; `docs/Compliance.md` "Order of work" item 6 (the report) marked done; `docs/Roadmap.md` "Critical-system standards, checked by machine": "the compliance report" struck from what waits (the item stays 🟨 for CERT, CWE and MISRA C++); `docs/Decisions.md`: D-153 as decided above.

## L-13 - verify sees the version CI sees

* **id**: L-13
* **goal**: a build in `verify`'s staged copy reports the commit id of the tree it was staged on, and a test fails if a staged build reports none.
* **why**: the staged copy is `git archive` of a tree and has no `.git`; `scripts/generate/buildid.cmake:58-65` then finds only the `$Format:%h$` placeholder of `cmake/build-id.txt`, so `cf_build_id()` is empty, and a test of the version passes locally and fails in CI (`gen.version`, 2026-11-17).
* **needs**: nothing
* **size**: small
* **decided**:
  * **The id**: after the extraction in `scripts/verify.cmake` (the staged tree is made at `:281-320`), the script writes `${ROOT}/cmake/build-id.txt` with the output of `git rev-parse --short HEAD` (the commit the staged work stands on: the new commit does not exist yet, and the file says so in a comment of `verify.cmake`). The file's one line must match `^[0-9a-f]+$` (`buildid.cmake:60-64`). Nothing is written when `git rev-parse` fails; the test below then fails, loudly.
  * **The test**: in `tests/gen/CMakeLists.txt`, after `gen.version` (`:124-131`), `if(DEFINED ENV{CF_REQUIRE_BUILD_ID})` adds `gen.version.id`: `COMMAND configforge_gen --version` with `PASS_REGULAR_EXPRESSION "^ConfigForge gen ${PROJECT_VERSION} \\([0-9a-f]+(-dirty)?\\)\n$"` (the parenthesis is no longer optional). `scripts/verify.cmake` does `set(ENV{CF_REQUIRE_BUILD_ID} 1)` in its `SOURCE STREQUAL "index"` branch, so every preset it configures inherits it. Nobody else sets it: a plain copy of the tree builds with no id and no failure, as before.
  * **Stays out**: a build from a plain directory copy with no git (empty by design, `buildid.cmake:5-12`).
* **files**: `scripts/verify.cmake`, `tests/gen/CMakeLists.txt`; last: `docs/Codemap.md`.
* **steps**:
  1. `git switch -c work/L-13` from `main`.
  2. Test first: add the `gen.version.id` block. In WSL (a copy off `/mnt`): `git add -A && cmake -DSOURCE=index -DMODE=quick -DWORK=$HOME/cfv -P scripts/verify.cmake` with only the test added (the `ENV` line is added in step 3, so set it by hand: `CF_REQUIRE_BUILD_ID=1 cmake -DSOURCE=index ...`). **See it fail**: `gen.version.id` fails, exit non-zero, the staged copy has no id.
  3. Write the `build-id.txt` step and the `set(ENV{CF_REQUIRE_BUILD_ID} 1)`; rerun without the env prefix: exit 0, `gen.version.id` passed.
  4. A default build in the working tree (a checkout has `.git`): `ctest -R gen.version` exits 0.
  5. Documents; format; codemap; ACCEPT.
* **accept**: ACCEPT-LINUX (step 3's run is part of it), ACCEPT-WINDOWS, ACCEPT-CI.
* **stop if**: after step 3 the build still reports no id because `buildid.cmake` reads a different directory than `${ROOT}` (report the path it reads; do not change `buildid.cmake`'s rule).
* **docs**: `CHANGELOG.md`, "## 1.9.0 — unreleased", "### Fixed": "verify's staged copy carries the commit id of the tree it was staged on, so a test of the version fails locally where CI's would"; `docs/Roadmap.md` item "verify sees the version CI sees" marked 🟩; `docs/Maintaining.md`: one sentence where the staged copy is described (`grep -n "staged" docs/Maintaining.md`).

## L-14a - EVA over the float readers: `cf_text_to_f64` and the bit conversions

* **id**: L-14a
* **goal**: `tests/eva/eva_numeric_float.c` runs Frama-C EVA over `cf_text_to_f64`, `cf_f64_from_bits`, `cf_f64_from_f16_bits` and `cf_f64_from_f32_bits` for every argument, and `scripts/check/eva.cmake` fails on an alarm that `.eva-accepted.txt` does not list.
* **why**: L-3b proved the integer half (4c3ce8c, `tests/eva/eva_numeric.c`); `docs/Safety.md` "Proven, for every value: Frama-C EVA" ends with "The float half of `lib/numeric` is outside these harnesses": the float parser reads an untrusted text into 64-limb bignum arithmetic (`lib/numeric/src/numeric.c:108-140`), the place a bound on a limb count, a shift or an index would fail first.
* **needs**: nothing (L-3b is done)
* **size**: large (a harness and precision work: EVA's float and loop abstraction can need several flag trials)
* **decided**:
  * **D-154 (copy into docs/Decisions.md, "EVA over the float half: a harness a function group, and accepted alarms by reason")**: *Question*: how is the float half held by EVA when its precision is poor on bignum loops? *Options*: (1) one harness for the whole half; (2) one harness a direction (reading, then writing), each with its own precision flags; (3) give up on EVA and write CBMC proofs to a length. *Decision*: option 2: `eva_numeric_float.c` for reading (this card) and `eva_numeric_print.c` for writing (L-14b), because the reader's loops (digit accumulation, limb shifts) and the printer's (digit generation) need different `-eva-slevel-function` settings and a combined run would raise the cost of both. Option 1 was rejected for run time (`eva.yml` has a time limit); option 3 is the fallback only if both flag trials below fail (a stop, not a step). An alarm that is EVA's imprecision, shown by a CBMC or sanitizer test that covers the same case, is accepted in `.eva-accepted.txt` with the reason; an alarm that is a fault is fixed in `numeric.c` in the same card, with a unit test of the input that reached it. *Compatibility*: none.
  * **The harness** `tests/eva/eva_numeric_float.c`, in the form of `tests/eva/eva_numeric.c:1-30`: `#include "__fc_builtin.h"`, `cf_numeric.h`, `cf_port.h`; `char text[64]` made unknown with `Frama_C_make_unknown(text, 64)`, `len = (size_t)Frama_C_interval(0, 64)`; calls `cf_text_to_f64(text, len, &d, &end)` and with `NULL` for `end`; `cf_f64_from_bits(bits)` with `bits` from two `Frama_C_unsigned_int_interval(0, 0xFFFFFFFFu)` halves; `cf_f64_from_f16_bits(Frama_C_unsigned_int_interval(0, 0xFFFFu))` and `cf_f64_from_f32_bits(Frama_C_unsigned_int_interval(0, 0xFFFFFFFFu))`. The text length 64 covers 64 digits and so the 17-digit and the 400-digit paths (`CF_PARSE_DIGITS`, `numeric.c:124-130`) are reached by the harness only up to what 64 bytes allow; the longer inputs are reached by `-eva-slevel`-free reasoning on the same code, and are covered by the unit tests and the fuzzers, which the card does not replace (written under "Stays out").
  * **Flags**: in `scripts/check/eva.cmake`, `numeric_float` joins `set(harnesses ...)` (`:40-46`) after `numeric`; `set(src_numeric_float lib/numeric/src/numeric.c lib/port/src/port.c)`, `set(flags_numeric_float ...)`. The first trial is `-eva-slevel-function=cf_text_to_f64:200 -eva-split-limit 200` (the form of `split_flags`, `:30-33`); if EVA runs over 10 minutes (`time` of the step), the second trial adds `-eva-no-results` and lowers slevel to 50. The values that end in the file are the first that gives a run under 10 minutes with every alarm either fixed or accepted. The worker records both trials (alarms, time) in the commit message.
  * **Float flags**: EVA reports non-finite results as alarms (`-warn-special-float non-finite` is its default): `cf_f64_from_bits` of an exponent-all-ones pattern returns an infinity or a NaN by design (`cf_numeric.h:83`: the bit conversions are not "finite only"), so those alarms are accepted with the reason "the function returns the value the bits spell; callers (`cbor.c`, TLV) reject non-finite values", after the worker confirms the callers by `grep -n "cf_f64_from_" -r lib --include=*.c`.
  * **Stays out**: the printer and the digit generator (L-14b); inputs longer than 64 bytes (the unit tests and the `numbers-edge` documents of the benchmark carry those); `cf_text_to_i64`'s half (L-3b); the host's own `strtod`.
* **files**: `tests/eva/eva_numeric_float.c` (new), `scripts/check/eva.cmake`, `.eva-accepted.txt`, `lib/numeric/src/numeric.c` (only if an alarm is a fault), `lib/numeric/tests/test_numeric.c` (a unit test of a fault found); last: `docs/Safety.md`, `docs/Codemap.md`.
* **steps**:
  1. `git switch -c work/L-14a` from `main`. Frama-C is in `configforge-dev` (`FRAMAC=1` of `scripts/buildenv/setup.sh`); `frama-c -version` prints 33.0.
  2. Baseline: `cmake -DSRC=. -P scripts/check/eva.cmake` exits 0 before any change (the five harnesses).
  3. Write the harness. **See it fail**: add `numeric_float` to the list without any accepted entry and run the step: it exits 1 naming each alarm (a list, not a crash). Keep that list.
  4. Per alarm: decide fault or imprecision. A fault: write the unit test first (`ctest -R test_numeric` fails), fix, see it pass, and rerun EVA. An imprecision: one entry in `.eva-accepted.txt` as the file's header says (a `#` line with the reason, then `lib/numeric/src/numeric.c <function> <kind>`).
  5. Tune the flags as decided until the run is under 10 minutes and every alarm is handled; `cmake -DSRC=. -P scripts/check/eva.cmake` exits 0, then 1 again when one accepted entry is deleted (an entry no alarm matches is refused too: add one wrong entry and see it fail, remove it).
  6. Documents; format; `cmake -P scripts/generate/codemap.cmake`; ACCEPT.
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS (no C change unless a fault was found), ACCEPT-CI including the `eva` job (`gh run list --workflow eva --branch work/L-14a`: green on the last commit); `cmake -DSRC=. -P scripts/check/eva.cmake` exit 0.
* **stop if**: both flag trials leave EVA over 10 minutes or with more than 40 imprecise alarms in one function (the fallback, CBMC to a length, is a design question: write the numbers under notes); an alarm that is a fault in a public function's contract (it changes what `cf_text_to_f64` returns for a valid text).
* **docs**: `CHANGELOG.md`, "## 1.9.0 — unreleased", "### Added": "Frama-C EVA covers the float readers of lib/numeric (cf_text_to_f64 and the bit conversions)"; `docs/Safety.md`: the harness table gets `eva_numeric_float.c`, the sentence "The float half ... is outside" becomes "The float printer is outside (below)", the accepted count is updated; `docs/Roadmap.md:69` (the row "EVA over `numeric`'s float half") stays ⬜ until L-14b is done, then 🟩; D-154 as decided above.

## L-14b - EVA over the float printer: `cf_gen_digits` and `cf_f64_to_text`

* **id**: L-14b
* **goal**: `tests/eva/eva_numeric_print.c` runs EVA over `cf_f64_to_text` (and the static `cf_gen_digits` it calls) for every finite double and every buffer capacity, with every alarm fixed or accepted.
* **why**: the printer turns a double into the shortest text that reads back (`cf_numeric.h:55-63`) with bignum limbs of its own (`numeric.c:378-512`); a write past `buf[cap]` there would be in every emitter. It is the other half of the float work L-3b left.
* **needs**: L-14a (the flags and the way accepted alarms are written)
* **size**: large
* **decided**:
  * **The harness** `tests/eva/eva_numeric_print.c`: `char buf[40]` (a double prints in at most 25 bytes: the longest shortest form is `-1.7976931348623157e+308`, 24 bytes; 40 leaves room to see a write past 25), `cap = (size_t)Frama_C_interval(0, 40)`, `double v` from `Frama_C_double_interval(-1.7976931348623157e+308, 1.7976931348623157e+308)` (finite values only: NaN and infinity return 0, `cf_numeric.h:56-57`, and are one more call each with `Frama_C_nan` avoided: they are called as `cf_f64_to_text(1.0 / 0.0 ...)` computed through a `volatile double zero = 0.0`), then `n = cf_f64_to_text(v, buf, cap)` and a check `if (n > cap) Frama_C_show_each_bad(n);` (EVA reports it as a reachable statement: the harness's own assertion, and the card's test of the property "never writes more than `cap`" is that this statement is Unreachable in the report).
  * **Flags**: the first trial `-eva-slevel-function=cf_gen_digits:500,cf_f64_to_text:100 -eva-split-limit 200`, the second as in L-14a; `set(harnesses ... numeric_print)`, `src_numeric_print` as `src_numeric_float`.
  * **Accepted alarms** as D-154 says; the reason text of an alarm that is the loop abstraction losing a terminator follows the entry for `error.c` already in `.eva-accepted.txt` (`grep -n "terminated" .eva-accepted.txt`).
  * **Stays out**: the other formatters (`cf_u64_to_text` and `cf_i64_to_text`, done by L-3b); the unit tests' own round-trip oracle (`lib/numeric/tests`), which stays the check that the digits are the shortest.
* **files**: `tests/eva/eva_numeric_print.c` (new), `scripts/check/eva.cmake`, `.eva-accepted.txt`, `lib/numeric/src/numeric.c` (only if an alarm is a fault), `lib/numeric/tests/test_numeric.c`; last: `docs/Safety.md`, `docs/Codemap.md`.
* **steps**:
  1. `git switch -c work/L-14b` from `main` (after L-14a lands); `cmake -DSRC=. -P scripts/check/eva.cmake` exits 0.
  2. Write the harness and register it with no accepted entry. **See it fail**: the step exits 1 listing the alarms.
  3. Per alarm as L-14a step 4; the "never writes more than cap" statement is reported Unreachable: `grep Frama_C_show_each_bad "$OUT/eva_numeric_print.log"` prints nothing (the log is `${OUT}/eva_numeric_print.log`, `eva.cmake:49-50`).
  4. Flags as L-14a step 5; the run exits 0, and exits 1 with one wrong entry added.
  5. Documents; format; codemap; ACCEPT.
* **accept**: as L-14a (the `eva` CI job green on the last commit).
* **stop if**: as L-14a; the statement is reachable and the cause is EVA's imprecision that no flag removes (write the trial list under notes: the maintainer decides whether CBMC takes this function).
* **docs**: `CHANGELOG.md`, "## 1.9.0 — unreleased", "### Added": "Frama-C EVA covers the float printer of lib/numeric"; `docs/Safety.md` harness table and the "Not yet proven" sentence about the floating point parser and printer removed; `docs/Roadmap.md:69` row marked 🟩 (the heading text: "EVA over `numeric`'s float half"); no new decision.

## L-15a - The comparison with whole configuration libraries: time to look up

* **id**: L-15a
* **goal**: the benchmark of `bench/compare` has a mode `lookup` that parses a document and then looks up a fixed list of keys, for every adapter that can, and the page `docs/Performance.md` "Against the other libraries" shows its numbers.
* **why**: `bench/compare` (P-1b, D-111) measures parse time, peak memory and the code a library adds (`scripts/bench/page.py:446-456`), but not "time to look up" that the Roadmap item "Measured against the libraries a program would otherwise use" (lines 743-753) asks for, and a configuration library is used by looking up a few keys, not by walking everything.
* **needs**: nothing
* **size**: medium
* **decided**:
  * **D-155 (copy into docs/Decisions.md, "Lookup is measured on the keys of the document, parse included")**: *Question*: what is a lookup, across libraries whose trees differ? *Options*: (1) the time of one lookup in a built tree; (2) parse, then N lookups by path, timed together and reported as parse alone plus lookups; (3) look up with the library's idiom over a SAX stream. *Decision*: option 2. A library that keeps no tree (a SAX or event reader: expat, libyaml's event API, tinycbor) has no lookup and shows `-`: the page says so, as the "What is not compared" part says for other cells. The keys are the leaves of the document itself: the harness lists every leaf's dotted path (`a.b.0.c`, array indexes as numbers), takes every `k`-th so that at most 1,000 remain (`k = max(1, leaves / 1000)`), and writes them, one a line, next to the document as `<doc>.keys`, from `cf_bench` (`bench/bench.c:464`, where each document is made) so every library looks up the same keys. A lookup is `found` when the library returns a value of the leaf's kind; the number of keys found must equal the number asked for or the cell is rejected (as `accepted` is, `bench/compare/main.cpp:338-347`). *Rejected*: option 1 (it hides the cost of building the index lazily in some libraries, a library that builds on first lookup would look free) and option 3 (not a lookup). *Compatibility*: none.
  * **The adapter**: `bench/compare/touch.h:36-46` `struct Adapter` gets one member after `parse`: `bool (*lookup)(const char* p, size_t n, bool strict, const char* const* paths, size_t npaths, Touch& t);` (null for a library with no tree); the adapters of the libraries that keep a tree (`yyjson`, `simdjson` DOM, `rapidjson`, `cjson`, `tomlc17`, `tomlpp`, `inih`, `configforge`, `rapidyaml`) fill it: parse the document, then for each path split at `.` and descend with the library's own lookup call (`yyjson_obj_get`, `cJSON_GetObjectItemCaseSensitive`, `rapidjson` `FindMember`, `toml::table::at_path`, `inih`'s flat `section` + `name` map built by the handler, `cf_get` for ConfigForge), adding the value's kind to `Touch` as `parse` does (`touch_string`, `touch_int`, `touch_real`). `Registrar` and `registry.cpp` are unchanged.
  * **The mode**: `bench/compare/main.cpp:291-292` and `:338-361` get `--mode lookup --keys <file>`: warm once (`cfb_warm`), then repeats as `speed` does (`:361-404`), the cell printed as `"mode":"lookup","accepted":...,"lookups":N,"found":N,"ns_per_lookup":...,"parse_ms":...` where `ns_per_lookup` is (time of parse and lookups minus the best time of `parse` alone, the same document) divided by N. A negative result is printed as 0 with `"noise":true`.
  * **The page**: `scripts/bench/page.py` (`:446-456` shows the memory table's form) gets a table "Lookup, ns a lookup after one parse (`mixed`, 16 MiB)" per format; its validator (`--validate`) accepts the new mode; `scripts/bench/run.py` runs the new mode for the `mixed` document only, in the `full` scope, not in `quick` (the bench gate's cells and `bench/baseline/instructions.json` do not change).
  * **Stays out**: libconfig (L-15b); the `quick` scope; a lookup of a key that is absent; writing back.
* **files**: `bench/compare/touch.h`, `bench/compare/main.cpp`, `bench/compare/adapters/*.cpp` (the nine named), `bench/bench.c`, `scripts/bench/run.py`, `scripts/bench/page.py`, `docs/Performance.md`; last: `docs/Codemap.md`.
* **steps**:
  1. `git switch -c work/L-15a` from `main`; build as `bench.yml:90-103` does (`cmake -S . -B build-bench -DCMAKE_BUILD_TYPE=Release -DCF_BUILD_BENCH=ON`, `cmake --build build-bench --target cf_bench`, the libraries with `cmake -DLIBS="$HOME/cfb-libs" -P scripts/bench/fetch.cmake`, `cmake -S bench/compare -B build-cfb ...`).
  2. Test first: `scripts/bench/page.py --validate` is given a result file with a `lookup` cell from a fixture `scripts/bench/tests/lookup-cell.json` (new; the nearest existing fixtures are found with `ls scripts/bench`) and a second with `found` different from `lookups`; the first must pass and the second be refused. **See it fail**: both are refused today (unknown mode).
  3. Write the keys file in `bench/bench.c`, the `lookup` mode and one adapter (`yyjson`); `build-cfb/cfb_compare --lib yyjson --doc <doc> --mode lookup --keys <doc>.keys` prints a cell with `found` equal to `lookups`. **See it fail**: an adapter that looks up a wrong path (edit the path separator) gives `found` below `lookups` and the cell is refused.
  4. The other eight adapters, one commit each batch; the `configforge` adapter uses `cf_get` on a configuration made from the document, the keys translated as `a.b.0.c` to ConfigForge's key syntax (`docs/Guide.md` "Keys", found with `grep -n "dotted\|array index" docs/Guide.md`).
  5. The page, `run.py`, and one full local run of the lookup mode over `mixed` 16 MiB in WSL; paste its table into `docs/Performance.md` under "Against the other libraries" ("### Lookup (L-15a)", with the machine line the section keeps); format; codemap; ACCEPT.
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS (`bench/compare` is built only on Linux in CI: nothing changes there), ACCEPT-CI; `scripts/bench/page.py --validate` exit 0 on a real `full` run; the `compare` job of `bench.yml` runs `workflow_dispatch` once on the branch and its result validates (`gh workflow run bench --ref work/L-15a`, then `gh run watch`).
* **stop if**: an adapter's library has no way to look up by path without walking the tree (mark it `-`, one line in the page: never an adapter that walks); the `quick` scope's cells change (bench-gate fails).
* **docs**: `CHANGELOG.md`, "## 1.9.0 — unreleased", "### Added": "The library comparison measures the time to look up a key after a parse (bench/compare, --mode lookup)"; `docs/Maintaining.md` "Measured": the table's row (numbers as measured, whichever way they come out); `docs/Performance.md` as above; `docs/Roadmap.md` "Measured against the libraries a program would otherwise use" stays ⬜ until L-15b is done, then 🟩; D-155 as decided above.

## L-15b - libconfig in the comparison

* **id**: L-15b
* **goal**: `bench/compare` includes libconfig, pinned by a commit like the others, reading the documents its own syntax can hold, with parse, memory, size and lookup numbers on the page.
* **why**: the Roadmap item names libconfig among the libraries a program would otherwise use (inih, libconfig, cJSON, toml++, libyaml); `bench/compare/libraries.cmake:25-35` has every other of them and not libconfig.
* **needs**: L-15a, and the answer to the TO DECIDE below
* **size**: medium
* **decided**:
  * **TO DECIDE (the maintainer): a new external dependency, under a copyleft licence.** libconfig is LGPL-2.1-or-later. The benchmark's libraries are built into `cfb_compare`, a tool that is never shipped (the `LIB_*_LICENSE` lines, `libraries.cmake:37-`, record each licence). *Options*: (a) add libconfig to the benchmark only, fetched and pinned like the others, never linked into anything ConfigForge ships; (b) leave libconfig out and say in "What is not compared" why. *Recommendation*: (a): the licence's obligations attach to distributing a binary that links it, and `cfb_compare` is not distributed; the page names the licence as the other rows do.
  * **If (a), everything else is decided**: `bench/compare/libs/libconfig.cmake` in the form of `bench/compare/libs/inih.cmake`: `cfb_library(libconfig SOURCES <its lib/*.c and scanner/grammar C files as generated and shipped in its release tarball: grammar.c, scanner.c, libconfig.c, scanctx.c, strbuf.c, strvec.c, util.c, wincompat.c is excluded>)`; the pinned release tag and the commit it peels to are looked up with `git ls-remote --tags https://github.com/hyperrealm/libconfig` and written as the other libraries' are (`LIB_libconfig_URL`, `_TAG`, `_COMMIT`, `_DATE`, `_LICENSE "LGPL-2.1-or-later"`), the newest release tag at least 30 days old (the rule at `libraries.cmake:16`). Its documents: libconfig reads its own syntax (`name = value;`, groups `{}`, lists `()`), so the adapter reads the JSON document: ConfigForge's `cf_bench` writes the `mixed` document in JSON (`documents.json`: formats `JYTCIX`), and the harness converts it with `scripts/bench/json_to_libconfig.py` (new, standard library only: objects become groups, arrays become lists `( ... )`, strings escaped as libconfig's escapes, a `null` is dropped with its key and counted in the file's first comment) into `<doc>.cfg` before the run; the cell's `format` is `libconfig` and only the `mixed` document is measured (the other documents hold values libconfig has no type for: an integer past 64 bits, a `null`). Parse is `config_read_string` and the walk is `config_setting_get_member`/`config_setting_get_elem`; lookup is `config_lookup` with the dotted path (libconfig's own syntax for it).
  * **Stays out**: libconfig's C++ API; any document other than `mixed`; the stripped size of its C++ part.
* **files**: `bench/compare/libs/libconfig.cmake` (new), `bench/compare/adapters/libconfig.cpp` (new), `bench/compare/libraries.cmake`, `bench/compare/CMakeLists.txt` (the library list), `scripts/bench/json_to_libconfig.py` (new), `scripts/bench/run.py`, `scripts/bench/page.py`, `docs/Performance.md`; last: `docs/Codemap.md`.
* **steps**:
  1. After the answer: `git switch -c work/L-15b` from `main`.
  2. Test first: `scripts/bench/tests/test_json_to_libconfig.py` (new, `python3 -m unittest`): a small JSON with an object, an array, a string with a quote and a null converts to the expected text; **see it fail** (no converter), then write it.
  3. The pin, the library file, the adapter; `cmake -DLIBS="$HOME/cfb-libs" -P scripts/bench/fetch.cmake`; build `bench/compare`; `cfb_compare --list` prints `libconfig`.
  4. **See it fail**: run `cfb_compare --lib libconfig --doc <mixed.cfg> --mode count`; with the converter's output corrupted (one `;` removed) the cell is `"accepted":false`; restored it is `true` and its `strings` and `numbers` equal the JSON cell's within the dropped nulls.
  5. `run.py`, `page.py`: the new row; a full local run; its rows pasted into `docs/Performance.md`; the "The libraries" table (`libraries.cmake` data rendered by `page.py`) gains the row; format; codemap; ACCEPT.
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI; `python3 -m unittest scripts/bench/tests/test_json_to_libconfig.py` exit 0; the `compare` job of `bench.yml` run once on the branch validates.
* **stop if**: libconfig's pinned tag does not build with the project's flags without a patch to its sources (report the compiler line; never patch a fetched library); the converted document is not accepted by libconfig after the conversion rules above.
* **docs**: `CHANGELOG.md`, "## 1.9.0 — unreleased", "### Added": "libconfig joins the library comparison"; `docs/Performance.md`; `docs/Roadmap.md` "Measured against the libraries a program would otherwise use" marked 🟩 (with L-15a done); no new decision (D-155 covers the lookup).

## L-16a - The emitter sized from the key space it walks, kept only if it pays

* **id**: L-16a
* **goal**: `cf_emit` (`lib/config/src/bridge/emit.c:230-344`) writes its output in one pass when the caller's buffer holds it, and the change stays in the tree only if `bench/` shows fewer instructions, with the numbers (kept or not) in `docs/Maintaining.md` "Measured".
* **why**: D-76 ("Predict sizes and measure the result") and Roadmap item "D-76, sizes and work predicted, then checked" list "an emitter's output (dump mode) sized from the key space it walks, so one pass writes it" as open; the key space from the bytes and from the generation before are done (`cfi_ks_expect_lines`, `lib/config/src/core/keyspace.c:203-226`, called at `lib/config/src/bridge/file.c:62`; `cfi_builder_expect_like`, `lib/config/src/core/config.c:1116-1118`) and so is the file buffer.
* **needs**: nothing
* **size**: medium
* **decided**:
  * **Step 1 is a reading, and settles what the code does today**: the worker reads `emit.c:230-344` (one range, at most 120 lines at a time) and writes under the card's notes, in three lines, whether a call whose `cap` is too small returns the length that was needed (so a caller sizes with a first call and fills with a second), and how many passes over the key space each does. The change below applies only to the second case; if the function already writes in one pass into a sufficient buffer and returns the needed length otherwise, the card is closed as "already done" with that evidence (commit and lines) and nothing else is changed.
  * **The change, if there are two passes**: an internal `static size_t emit_estimate(const cf_keyspace* ks, const cf_codec* codec)` in `emit.c`: the sum over the entries of key bytes + value bytes + `CF_EMIT_OVERHEAD` (decided: 8 bytes an entry, the longest delimiter overhead a built-in encoder adds: `" = "`, quotes, a line end, a comma), the sum computed in the walk the function already makes to filter entries (`emit_filter`, `emit.c:230-236`), never in a pass of its own. When `cap >= estimate` the encoder writes straight into `buf`; otherwise the function takes the path it takes today. The estimate is a hint and never a bound: the encoder's own checks still refuse a write past `cap` (no change to them).
  * **The rule that keeps it**: instructions, not time (D-111): a `cf_bench` cell for `cf_emit` over the `mixed` 4 MiB configuration under callgrind (`python3 scripts/bench/run.py --scope quick`, cells chosen at `scripts/bench/run.py`'s `QUICK_DOCS`; if no emit cell exists, one is added to `bench/bench.c` in this card, the emit of every format of a configuration loaded from `mixed`), GCC and Clang, before and after. Kept when the instruction count falls 3% or more in every emit cell and no other cell rises more than 1%; otherwise reverted and the table kept in the document. `bench/baseline/instructions.json` is rewritten only from a CI artifact, never from a laptop (`scripts/bench/gate.py`'s docstring).
  * **Stays out**: the key space's own sizing (done); an allocation inside the encoders; SIMD.
* **files**: `lib/config/src/bridge/emit.c`, `bench/bench.c` (only if no emit cell exists), `docs/Maintaining.md`; last: `bench/baseline/instructions.json` (from CI's artifact), `docs/Codemap.md`.
* **steps**:
  1. `git switch -c work/L-16a` from `main`. The reading of decided step 1.
  2. The test first: `lib/config/tests/test_emit.c` (the file that tests `cf_emit`: `ls lib/config/tests | grep emit`; if none, `test_config.c` where `cf_emit` is called, found with `grep -ln cf_emit lib/config/tests/*.c`) gets a case: a configuration of 1,000 keys emitted with `cap` equal to the length the first call returned (exactly), with `cap - 1` (must return the needed length and write nothing past `cap - 1`), and with `cap` far larger; the three agree on the bytes. It passes on the unchanged code: it pins what is computed.
  3. Baseline: `scripts/bench/run.py --scope quick` with GCC and with Clang (built as `ci.yml`'s `bench-gate` job builds them, `ci.yml:555-590`) into `l16a-<gcc|clang>-before.json`.
  4. Make the change. **See it fail**: make the estimate one byte short in `emit_estimate`'s overhead only when `cap == estimate - 1` is not allowed to matter: change `CF_EMIT_OVERHEAD` to 0 and see the test's `cap == length` case still pass but the instruction count not fall (the change does nothing without its estimate); put it back.
  5. The same runs into `-after.json`; compare with `python3 -c "import json,sys;g=lambda f:{'%s/%s'%(c['format'],c['doc']):c['instructions'] for c in json.load(open(f))['cells'] if c.get('mode')=='count'};a,b=g(sys.argv[1]),g(sys.argv[2]);[print(k,a[k],b[k],'%.1f%%'%(100.0*(b[k]-a[k])/a[k])) for k in sorted(a) if k in b]" before.json after.json`.
  6. By the rule: commit with the table in the message, or commit and revert with the same table. `docs/Maintaining.md` "Measured": the table either way.
  7. Format; codemap; ACCEPT.
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI with `bench-gate` green; the test of step 2 exit 0 before and after the change.
* **stop if**: the reading shows a different structure than two passes and the change cannot be made without changing what `cf_emit` returns for a too-small buffer; callgrind is not installed (report; do not time instead).
* **docs**: `CHANGELOG.md`, "## 2.4.0 — unreleased" (add the heading under the open ones if absent), "### Changed" only if kept: "cf_emit writes in one pass when the buffer is large enough"; `docs/Maintaining.md` "Measured": the table; `docs/Roadmap.md` item "D-76, sizes and work predicted, then checked": struck "an emitter's output (dump mode) sized ..." from what is open, kept or not; no decision.

## L-16b - Sequential read advice for a large file, kept only if it pays

* **id**: L-16b
* **goal**: a file read whole tells the system it will be read from the start to the end (`posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL)`) where the system has it and the file is larger than the threshold below, and the change stays only if `bench/` shows it pays.
* **why**: the item "D-76, sizes and work predicted, then checked" lists "sequential read advice to the system for a large file" as not done; the file buffer is already sized from the system's report (`lib/platform/src/posix/posix.c:137-159` stamps the size).
* **needs**: nothing
* **size**: small
* **decided**:
  * **Where**: the one function that reads a file's bytes in `lib/platform/src/posix/posix.c` (found with `grep -n "read(" lib/platform/src/posix/posix.c`; the file is 1,000+ lines: read only the range of that function). The call is made after `open` and before the first `read`, when `st.st_size >= CF_ADVISE_MIN` and `#if defined(POSIX_FADV_SEQUENTIAL)`; its result is ignored (it is advice). `CF_ADVISE_MIN` is 1 MiB (decided: below it the kernel's default read-ahead already covers a file, and a configuration file is rarely over it; the number is a `#define` beside the function with this reason as its comment). Windows (`lib/platform/src/win32/`) is not changed: `FILE_FLAG_SEQUENTIAL_SCAN` on `CreateFileW` is the same advice, added in the same card only if the Windows build's function is a single `CreateFileW` call (found with `grep -n "CreateFileW" lib/platform/src/win32/*.c`); otherwise left out and said in the notes.
  * **The test**: no result changes, so the test is that a file of 3 MiB is read whole and equal to what was written, through `cf_platform_read`'s caller in `lib/platform/tests/` (`ls lib/platform/tests`; the existing read test is extended with the 3 MiB case): it pins that the advice changes nothing.
  * **The rule that keeps it**: wall time of loading a 256 MiB file from the page cache dropped (`echo 3 | sudo tee /proc/sys/vm/drop_caches` in WSL, run with `sudo` by the maintainer's own shell: **the worker never types the sudo password**; where it cannot, the measurement is skipped and the change is left out, recorded as "not measured here"), 10 runs each, before and after, with `hyperfine` or `scripts/work/profile.py`'s timing (`docs/plan/L-tools.md`, group 13); kept when the median falls 5% or more and no run is slower than the slowest before.
  * **Stays out**: `mmap` (a different design: no copy; the stream reader's); `readahead(2)`; any change to the buffer.
* **files**: `lib/platform/src/posix/posix.c`, `lib/platform/src/win32/*.c` (only as decided), `lib/platform/tests/<the read test>`, `docs/Maintaining.md`; last: `docs/Codemap.md`.
* **steps**:
  1. `git switch -c work/L-16b` from `main`.
  2. Write the 3 MiB test; it passes unchanged (`ctest -R platform` exit 0).
  3. Measure before: a 256 MiB file from `cf_bench`'s generator (`build-bench/bench/cf_bench`, as `bench.yml:90-93`), loaded with `configforge_doctor` or the `cf_create` path the benchmark uses, 10 runs, cache dropped each time, into `l16b-before.txt`.
  4. Add the call. **See it fail**: compile with `-DCF_ADVISE_MIN=0x7fffffff` and see the measured effect vanish (the after runs equal the before runs within noise): the advice, not the build, made the difference; put it back.
  5. Measure after into `l16b-after.txt`; by the rule: commit with the medians in the message, or commit and revert.
  6. `docs/Maintaining.md` "Measured": the medians either way; format; codemap; ACCEPT.
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI; `ctest -R platform` exit 0.
* **stop if**: the machine cannot drop the page cache without a password the worker may not type (leave the change out; report "not measured"); the read function is more than one call site (report).
* **docs**: `CHANGELOG.md`, "## 2.4.0 — unreleased", "### Changed" only if kept: "A large file is read with the system told it is read sequentially"; `docs/Maintaining.md` "Measured"; `docs/Roadmap.md` item "D-76, sizes and work predicted, then checked": struck "sequential read advice", kept or not.

## L-16c - Branch hints on the readers' hot paths, kept only where they pay

* **id**: L-16c
* **goal**: `CF_LIKELY` and `CF_UNLIKELY` (`lib/port/include/configforge/cf_port.h:285-290`, used nowhere in `lib/` today) mark the error and the end-of-buffer branches of the readers' hot loops, one reader a commit, each kept only if its bench cells fall.
* **why**: the item "D-76, sizes and work predicted, then checked" lists "branch hints on the readers' hot paths"; the macros exist and nothing uses them, so the item is the work of putting them on the right branches and the measurement that says whether they matter.
* **needs**: nothing (it does not depend on P-3: a hint is not a scan)
* **size**: medium
* **decided**:
  * **The candidates** (one commit each, in this order, the share of time from `docs/Performance.md` "The profile", `:199-295`): `js_string` and `js_value` in `lib/serialize/src/json.c`; `toml_string` in `toml.c`; `yp_quoted` and `yp_plain_line` in `yamlparse.c`; `cf_text_utf8_valid` in `lib/text/src/utf8.c`. Per candidate, hints go on exactly: the "past the end of the input" tests (`CF_UNLIKELY(s->p >= s->end)`), the capacity checks before a byte is stored (`CF_UNLIKELY(m >= cap - n)`) and the refusal branches (`CF_UNLIKELY` on a branch that returns an error). Never on a branch whose frequency depends on the document (a quote, a backslash, a non-ASCII byte). A hint changes no result: the unit tests and the conformance suites are the oracle.
  * **MISRA**: a macro that expands to `__builtin_expect` is a compiler extension; `docs/compliance/deviations.txt` records every report, and `gate.misra` fails a report the record lacks, so the first hint is committed together with a regeneration of the record (the command is in the file's header); the `misra` profile defines `CF_LIKELY(x)` as `(x)` where `__builtin_expect` is unavailable, so a `misra` build has no extension: step 5 checks that `cmake --preset misra && ctest --preset misra -R gate.misra` exits 0 with no new line in the record.
  * **The rule that keeps it** is L-16a's: instruction counts of the quick cells (GCC and Clang), kept when the reader's cells fall 2% or more and none rises more than 1%; the table in the commit message; a revert carries the same table; `bench/baseline/instructions.json` is rewritten from CI's artifact only.
  * **Stays out**: branch hints in the writers and the config layer; `__builtin_prefetch`; profile-guided optimisation (the item "build options measured" of 2.4.0).
* **files**: `lib/serialize/src/json.c`, `toml.c`, `yamlparse.c`, `lib/text/src/utf8.c`, `docs/compliance/deviations.txt` (only if a report appears), `docs/Maintaining.md`; last: `bench/baseline/instructions.json`, `docs/Codemap.md`.
* **steps**:
  1. `git switch -c work/L-16c` from `main`. The baseline of L-16a step 3 into `l16c-<gcc|clang>-0.json`.
  2. Per candidate k: (a) put the hints on as decided; (b) `ctest --preset default` exit 0; (c) **see it fail**: swap one `CF_UNLIKELY` for `CF_LIKELY` on the capacity check and see the instruction count of that reader's cells rise (a hint that is wrong costs); put it back; (d) the quick run both compilers, compared with the last kept state as L-16a step 5; (e) `cmake -DMODE=quick -DEXTRA="conformance;fuzz" -DFUZZ_SECONDS=60 -DSOURCE=index -DJOBS=3 -DWORK=$HOME/cfv -P scripts/verify.cmake` exit 0; (f) commit with the table, or commit and revert, push.
  3. The misra check of the decided MISRA paragraph after candidate 1.
  4. `docs/Maintaining.md` "Measured": one row a candidate, kept or reverted; format; codemap; ACCEPT.
* **accept**: ACCEPT-LINUX with step 2e passing for the last state; ACCEPT-WINDOWS; ACCEPT-CI with `bench-gate` green; `ctest --preset misra` exit 0.
* **stop if**: a candidate changes what a reader returns (a test, a conformance case or a fuzzer finding moves): revert it and go on; no candidate keeps its change (close the card with the table).
* **docs**: `CHANGELOG.md`, "## 2.4.0 — unreleased", "### Changed" only for kept hints: "Branch hints on the readers' refusal and end-of-input branches"; `docs/Maintaining.md` "Measured"; `docs/Roadmap.md` item "D-76, sizes and work predicted, then checked": with L-16a and L-16b done the item is marked 🟩 (a measured "no" counts as done).

## L-17 - Byte order and word size in CI: s390x, i386 and armv7 under qemu-user

* **id**: L-17
* **goal**: a CI job `targets` builds `default` for s390x (big endian), i386 (32 bit) and armv7 (32 bit, hard float) with cross compilers and runs the unit tests under qemu-user, so the swaps in CBOR, TLV and the image header run on a host that needs them.
* **why**: `docs/targets.txt` lists armv7 and s390x as "planned"; the Roadmap item "Byte order, word size and a bare-metal target in CI" says CI runs only x86-64 and arm64, both little endian, so a wrong swap or a 32-bit `size_t` assumption would pass every job.
* **needs**: nothing
* **size**: medium
* **decided**:
  * **D-156 (copy into docs/Decisions.md, "Other byte orders and word sizes run under qemu-user in one matrix job")**: *Question*: how does CI run a big-endian and two 32-bit targets? *Options*: (1) emulated full systems (QEMU system mode); (2) qemu-user with the distribution's cross compilers, tests run through `CMAKE_CROSSCOMPILING_EMULATOR`; (3) Docker images with `--platform`. *Decision*: option 2: the cheapest to keep, one job a matrix entry, no image to keep. Rejected: option 1 (a disk image to maintain) and 3 (`binfmt` privileges on the runner and the image's own libc, so what is tested is the image). *Compatibility*: none. Tests that need what qemu-user lacks (inotify, a real clock, file locking) are skipped by name in one list, `scripts/check/targets-skip.txt`, with a reason each; the list is checked: an entry that matches no test fails the job.
  * **The toolchain files**: `cmake/toolchains/s390x-linux.cmake`, `i386-linux.cmake`, `armv7-linux.cmake`, in the form of `cmake/toolchains/wasm32-wasi.cmake`: `set(CMAKE_SYSTEM_NAME Linux)`, `CMAKE_SYSTEM_PROCESSOR` (`s390x`, `i686`, `armv7l`), the compilers `s390x-linux-gnu-gcc`, `i686-linux-gnu-gcc`, `arm-linux-gnueabihf-gcc`, `CMAKE_CROSSCOMPILING_EMULATOR` `qemu-s390x;-L;/usr/s390x-linux-gnu`, `qemu-i386;-L;/usr/i686-linux-gnu`, `qemu-arm;-L;/usr/arm-linux-gnueabihf`, `CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER`. Packages (Ubuntu 26.04): `gcc-s390x-linux-gnu gcc-i686-linux-gnu gcc-arm-linux-gnueabihf qemu-user`; the library path of each (`/usr/<triple>`) is checked by `ls` in step 2, not assumed.
  * **The job**: `.github/workflows/ci.yml`, after `minimal` (`:164-169`): `targets` with `runs-on: ubuntu-26.04`, `strategy.fail-fast: false`, `matrix.target: [s390x, i386, armv7]`; steps: checkout (the pinned action line of the other jobs, copied), `sudo apt-get update && sudo apt-get install -y qemu-user gcc-s390x-linux-gnu gcc-i686-linux-gnu gcc-arm-linux-gnueabihf` (each target installs all three: one line, three seconds), `cmake -S . -B build-${{ matrix.target }} --toolchain cmake/toolchains/${{ matrix.target }}-linux.cmake -DCMAKE_BUILD_TYPE=Release -DCF_WITH_CXX=OFF -DCF_BUILD_DOCTOR=OFF`, `cmake --build build-${{ matrix.target }} -j 4`, `ctest --test-dir build-${{ matrix.target }} --output-on-failure -j 4 -E "$(python3 scripts/check/targets_skip.py)"`. The options that turn off the C++ wrapper and Doctor are the cache variables the `minimal` profile sets (`cmake/profiles/minimal.cmake`: the worker takes the exact names from that file and uses them; if they differ from the two above, the file's names win).
  * **The tests that must run**: the unit tests of CBOR and TLV and the image (`ctest -N | grep -i "cbor\|tlv\|image"` shows their names) are not on the skip list, ever: a card that skips them defeats its goal (the skip checker `scripts/check/targets_skip.py` refuses an entry matching `cbor`, `tlv` or `image`).
  * **Stays out**: riscv64 (a row of `docs/targets.txt` that stays "planned"); the sanitizers under qemu (a different tool); making the jobs required (L-17-m, the maintainer's repository settings).
* **files**: `cmake/toolchains/s390x-linux.cmake`, `i386-linux.cmake`, `armv7-linux.cmake` (new), `scripts/check/targets-skip.txt` (new), `scripts/check/targets_skip.py` (new), `.github/workflows/ci.yml`, `docs/targets.txt`; last: `README.md` (the targets table at `:210-220`), `docs/Codemap.md`.
* **steps**:
  1. `git switch -c work/L-17` from `main`. In WSL: `sudo apt-get install ...` is run by the maintainer's shell, never with a password typed by the worker; if the packages are absent the job on CI is the first run (**CI-only**: expect more than one push).
  2. Test first, for the skip checker: `scripts/check/tests/test_targets_skip.py` (new, `python3 -m unittest`): an entry that matches no test name fails, an entry `cbor` is refused, a good file prints a `|`-joined regex. **See it fail** (no script), write it, pass.
  3. The three toolchain files and, where the packages are present, a local build of `s390x`: `cmake -S . -B ~/b390 --toolchain cmake/toolchains/s390x-linux.cmake -DCMAKE_BUILD_TYPE=Release && cmake --build ~/b390 -j 8 && ctest --test-dir ~/b390 -j 4` exit 0 or a list of failing tests.
  4. Each failing test is either a defect (a real byte-order or word-size fault: the finding of the card: fix it with a unit test first, see it fail, fix, pass: that is what the job is for) or a test qemu-user cannot run (an entry in `targets-skip.txt` with its reason). The first kind is never put on the list.
  5. The job in `ci.yml`; push; `gh run watch --exit-status $(gh run list --branch work/L-17 --workflow ci --limit 1 --json databaseId --jq '.[0].databaseId')`; repeat step 4 for what CI shows.
  6. `docs/targets.txt`: the lines for `armv7 Linux` and `s390x Linux (big-endian)` become `armv7 Linux|qemu|targets|<date>|<commit>|` and `s390x Linux (big-endian)|qemu|targets|<date>|<commit>|`, and a new line `i386 Linux|qemu|targets|<date>|<commit>|`; the README table at `:210-220` the same (the way shown: "tested under qemu"); format; codemap; ACCEPT.
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS (no C change unless a defect was found), ACCEPT-CI with the three `targets` entries green on the last commit; `python3 -m unittest scripts/check/tests/test_targets_skip.py` exit 0.
* **stop if**: a defect found is in a public format's byte order (a swap wrong in the wire or image format: the format would change; the maintainer decides: stop and report the case); the qemu-user binary of a target crashes on the library's startup for a reason outside the library (report the version); a target needs more than 25 skip entries.
* **docs**: `CHANGELOG.md`, "## 1.9.0 — unreleased", "### Added": "CI runs the suite for s390x (big endian), i386 and armv7 under qemu-user (D-156)"; `docs/targets.txt` and the README table as step 6; `docs/Roadmap.md` item "Byte order, word size and a bare-metal target in CI": the first sentence's jobs done (the bare-metal build is L-18a), the item stays ⬜ until L-18a and L-17-m; D-156 as decided above.

## L-17-m - The `targets` jobs required

* **id**: L-17-m
* **goal**: the repository's branch protection for `main` lists `targets (s390x)`, `targets (i386)` and `targets (armv7)` as required checks.
* **why**: the Roadmap item says "Done when the three jobs are green and required"; a required check is a repository setting that only the maintainer's rights change.
* **needs**: L-17 (landed, the jobs green on `main`)
* **size**: small
* **decided**: the maintainer's step, in the repository's Settings, Branches, the rule for `main`, "Require status checks", add the three names exactly as the job lists them in the Actions tab (`gh run view <id> --json jobs --jq '.jobs[].name'` prints them). No agent does it: it is a security setting of a repository (docs/Plan.md, "Waits on the maintainer"). The agent part of this item is L-17, which stands alone.
* **files**: none (a setting).
* **steps**:
  1. The maintainer opens Settings, Branches, edits the rule for `main`, adds the three checks, saves.
  2. `gh api repos/develooper1994/configforge/branches/main/protection/required_status_checks --jq '.contexts'` lists them.
* **accept**: the command of step 2 lists the three names.
* **stop if**: a name differs from the job's (the check is then never satisfied and no merge passes): the maintainer removes it and adds the right one.
* **docs**: `docs/Roadmap.md` item "Byte order, word size and a bare-metal target in CI" marked 🟩 when this and L-18a are done; `docs/Plan.md` "Waits on the maintainer": the row's status "done".

## L-18a - A Cortex-M build of `minimal` in CI, with its stack gate

* **id**: L-18a
* **goal**: a CI job `cortex-m` cross-builds the `minimal` profile's library for a Cortex-M4 with `arm-none-eabi-gcc` (no run), measures each function's stack frame with `-fstack-usage` and fails when one is over the profile's 1 KiB budget.
* **why**: the Roadmap item "Small embedded systems" says "the Cortex-M `.text` and stack gate remains": `minimal` is held to numbers on the host only (`ci.yml:164-169`, `scripts/check/stack.cmake`), and a 64-bit host's frames and a Thumb-2 target's differ, so the claim "a kilobyte of stack" has not been shown on a device's architecture.
* **needs**: nothing
* **size**: medium
* **decided**:
  * **D-157 (copy into docs/Decisions.md, "The Cortex-M numbers come from a cross build of the library alone")**: *Question*: how is the Cortex-M build held to numbers when no device or emulator runs the tests? *Options*: (1) build and run under qemu-system-arm; (2) cross-build the static library only (no linking of a program), gate its sizes and frames; (3) wait for hardware. *Decision*: option 2: the numbers the Roadmap asks for (`.text`, stack) are properties of the compiled objects, and a library that is not linked needs no board support package. Its status in `docs/targets.txt` is "built only", never "tested". Rejected: option 1 (a board model and semihosting to maintain for no number the build does not already give) and 3 (L-19-m records hardware when it exists). *Compatibility*: none.
  * **The toolchain file** `cmake/toolchains/arm-none-eabi.cmake`: `CMAKE_SYSTEM_NAME Generic`, `CMAKE_SYSTEM_PROCESSOR arm`, `CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY` (no linking of a test program), compilers `arm-none-eabi-gcc`, `CMAKE_C_FLAGS_INIT "-mcpu=cortex-m4 -mthumb -mfloat-abi=soft -ffunction-sections -fdata-sections"` (soft float: a Cortex-M0 or M3 has no FPU, and the numbers are then the pessimistic ones), `CMAKE_FIND_ROOT_PATH_MODE_*` `NEVER`/`ONLY` as the WASI file has. The package: `gcc-arm-none-eabi libnewlib-arm-none-eabi` (Ubuntu 26.04).
  * **The build**: `cmake -S . -B build-cortexm --toolchain cmake/toolchains/arm-none-eabi.cmake -DCF_PROFILE=minimal -DCMAKE_BUILD_TYPE=MinSizeRel -DCF_BUILD_TESTS=OFF` (the option's name is taken from `cmake/ConfigForgeOptions.cmake`: `grep -n "BUILD_TESTS\|CF_BUILD_" cmake/ConfigForgeOptions.cmake`; the file's name wins) and `cmake --build build-cortexm --target configforge`; the profile's own `CF_STACK_BUDGET` of 1024 runs after the library links (`CMakeLists.txt:80`, `cmake/ConfigForgeCompiler.cmake:189-225`: GCC reports frames by `-fstack-usage`, so the existing gate works unchanged for this compiler; the card adds a configure-time check that it did run: the build prints a `stack:` status line and the job greps for it, so a silent skip fails the job).
  * **Stays out**: running anything on the Cortex-M; `.text` numbers and a gate on them (L-18b); Cortex-M0/M3 builds as extra entries (the table of L-18b is for M4 soft float, named); the SIMD backends.
* **files**: `cmake/toolchains/arm-none-eabi.cmake` (new), `.github/workflows/ci.yml`, `docs/targets.txt`, `docs/Decisions.md`; last: `README.md` (the table row "Cortex-M | not tried", `:218`), `docs/Codemap.md`.
* **steps**:
  1. `git switch -c work/L-18a` from `main`. Install the packages (the maintainer's shell for `sudo`).
  2. The toolchain file; the build command above locally (WSL): exits 0 with a `stack:` line and "within the budget".
  3. **See it fail**: configure with `-DCF_STACK_BUDGET=64` and see the build fail naming the function over 64 bytes (the existing gate at work on the Thumb frames); then with 1024 it passes.
  4. The job `cortex-m` in `ci.yml` after `minimal`: install, configure, build, `cmake --build build-cortexm 2>&1 | tee cortexm.log` and `grep -q "stack:" cortexm.log` (the job fails when the line is absent).
  5. `docs/targets.txt`: `Cortex-M|built|cortex-m|<date>|<commit>|M4, soft float, minimal profile, library only`; the README row the same ("built only"); format; codemap; ACCEPT.
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS (no C change), ACCEPT-CI with `cortex-m` green; step 3's failure shown once (the commit message records the function and its frame).
* **stop if**: a frame is over 1 KiB on Thumb-2 where the host build is under (a fault the card found: report the function and its size; the profile's budget is not raised by the card, a design question); `arm-none-eabi-gcc` does not accept a flag the library's warnings use (report the flag, never weaken `-Werror`).
* **docs**: `CHANGELOG.md`, "## 1.9.0 — unreleased", "### Added": "CI builds the minimal profile for a Cortex-M4 and holds its stack frames to the profile's budget (D-157)"; `docs/Roadmap.md` item "Small embedded systems, everything optional" (the stack gate struck from what remains; stays 🟨); D-157 as decided above.

## L-18b - The Cortex-M `.text` of each part, in a table and a gate

* **id**: L-18b
* **goal**: `docs/sizes-cortexm.txt` records the `.text` of the Cortex-M4 `minimal` library and of each `CF_WITH_*` part, a CI step fails when a part grows more than 2% over it, and the README's table shows the numbers.
* **why**: the item says "the README's table gives each part's size, so a user adds up their build before making it" and "its `.text` and largest stack frame on a Cortex-M build are gated, so a new feature that leaks into it fails"; `docs/sizes.txt` and `scripts/check/sizes.cmake` do this for the host (x86-64, `docs/sizes.txt:2`), where Thumb code is a different size.
* **needs**: L-18a
* **size**: medium
* **decided**:
  * **Reuse, no second tool**: `scripts/check/sizes.cmake` is run with the cross toolchain: its `size_program` (`:113`, `${size_program} -t libconfigforge.a`) is set by a new argument `-DSIZE_PROGRAM=arm-none-eabi-size` (default: the host's `size`, as now), the `tool|...` header line (`docs/sizes.txt:2`) records `arm-none-eabi GNU <version>|MinSizeRel`, and the builds it makes use `-DTOOLCHAIN=cmake/toolchains/arm-none-eabi.cmake` (a new argument, passed to each configure as `--toolchain`). The file is `docs/sizes-cortexm.txt`, written by `-DWRITE=ON -DOUT=docs/sizes-cortexm.txt` and read by `-DCHECK=ON -DBASELINE=docs/sizes-cortexm.txt` (the argument `BASELINE` is added; default `docs/sizes.txt`).
  * **The limit**: a part's `.text` may exceed its recorded number by 2% or 64 bytes, whichever is larger (the rule `sizes.cmake`'s check already has: read it at its top, `scripts/check/sizes.cmake:1-60`; if it differs, the file's rule wins and this line is corrected in the commit message).
  * **The table in the README**: the table the README keeps for the host sizes (found with `grep -n "sizes.txt" README.md docs/*.md`) gets a column "Cortex-M4 (bytes)" read from the new file by the same generator that writes the host column (if the host column is written by hand, the card writes the column by hand and a test checks it against the file: `scripts/check/check_docs.py` is extended with the one check; the worker finds which with `grep -n "sizes" scripts/check/check_docs.py`).
  * **CI**: the `sizes` job (`ci.yml:182-216`) gets a second step after the host check: install the ARM packages and run `cmake -DCHECK=ON -DTOOLCHAIN=cmake/toolchains/arm-none-eabi.cmake -DSIZE_PROGRAM=arm-none-eabi-size -DBASELINE=docs/sizes-cortexm.txt -DWORK="$RUNNER_TEMP/size-cm" -P scripts/check/sizes.cmake`, and its path filter (`:12`, `:199`) includes `docs/sizes-cortexm.txt` and `cmake/toolchains/arm-none-eabi.cmake`. The baseline is written from CI's own run (the host's rule, `ci.yml:207-215`: the `sizes` artifact), never from a laptop's compiler.
  * **Stays out**: Cortex-M0/M3; a size gate on the stripped `.text` of a linked program; flash and RAM of an application.
* **files**: `scripts/check/sizes.cmake`, `docs/sizes-cortexm.txt` (new, from CI), `.github/workflows/ci.yml`, `README.md`, `scripts/check/check_docs.py` (only if the column is by hand); last: `docs/Codemap.md`.
* **steps**:
  1. `git switch -c work/L-18b` from `main` (after L-18a).
  2. Test first: `scripts/check/tests/` has no CMake tests; the self-check is a `sizes.arm-args` ctest in `tests/CMakeLists.txt` that runs `cmake -DCHECK=ON -DBASELINE=<a fixture with one part 3% under its real size>` against a tiny fixture library (`tests/sizes/libfix.a`, made by the test from one C file with the host `cc -c` and `ar`), expecting exit 1 with the part's name; **see it fail** (the argument does not exist); write the arguments; it passes with a fixture within 2%.
  3. Run on Linux with the ARM packages: `cmake -DWRITE=ON -DTOOLCHAIN=cmake/toolchains/arm-none-eabi.cmake -DSIZE_PROGRAM=arm-none-eabi-size -DOUT=/tmp/sizes-cm.txt -DWORK=$HOME/size-cm -P scripts/check/sizes.cmake` exits 0.
  4. The CI step; push; take the `sizes` artifact of the run (`gh run download`) and commit its Cortex-M part as `docs/sizes-cortexm.txt`; push; `sizes` green with the check on.
  5. **See it fail**: delete 5% from one number of `docs/sizes-cortexm.txt` and see the step exit 1 naming the part; restore.
  6. The README column; format; codemap; ACCEPT.
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI with `sizes` green, both checks on; `ctest -R sizes.arm-args` exit 0.
* **stop if**: `sizes.cmake`'s per-part builds cannot be made with a toolchain file without a restructure of the script (report the lines); the Thumb numbers differ by more than 5% between two CI runs of the same commit (the gate cannot be written: report).
* **docs**: `CHANGELOG.md`, "## 1.9.0 — unreleased", "### Added": "The Cortex-M4 size of each part is in docs/sizes-cortexm.txt and the README, and CI holds it"; `docs/Roadmap.md` item "Small embedded systems, everything optional" marked 🟩 (the `.text` gate was the last thing remaining: the item's own text says so); no new decision (D-157).

## L-19a - A Linux arm64 job

* **id**: L-19a
* **goal**: CI builds and tests `default` on a Linux arm64 runner, and `docs/targets.txt` lists AArch64 Linux with the job that shows it.
* **why**: the Roadmap item "Arm supported well" starts from "no Linux arm64 job": arm64 runs only as macOS (`macos-latest`, `ci.yml:60-79`), so glibc and musl on AArch64 (unaligned access, `long double`, the varargs ABI) are not shown.
* **needs**: nothing
* **size**: small
* **decided**:
  * **The job**: `.github/workflows/ci.yml`, after `minimal`: `arm64` with `runs-on: ubuntu-24.04-arm` (GitHub's hosted arm64 Ubuntu runner: `ubuntu-26.04-arm` is used instead if `gh api repos/actions/runner-images` or the runner label list names it at the time; the worker checks the labels in the repository's `Settings, Actions, Runners` page output of `gh api repos/develooper1994/configforge/actions/runners` and in GitHub's documentation of hosted runners and takes the newest Ubuntu arm label that exists), steps as the `matrix` job's Linux steps (`ci.yml:60-79`: checkout, `cmake --preset default`, build, `ctest --preset default`) and a second `sanitize` run on it (`cmake --preset sanitize ...`), where alignment faults would show: the sanitize preset has UBSan's alignment check on (`CMakePresets.json`, `sanitize`).
  * **`docs/targets.txt`**: a line `arm64 Linux, glibc|runner|arm64|<date>|<commit>|` (the file's header: `target|how|job-or-board|date|commit|note`); the README targets table (`:210-220`) the same.
  * **Stays out**: NEON and SVE in the SIMD backend (the 2.4.0 backends, `docs/Maintaining.md` "Versions", row 2.4.0); an arm64 musl job (the `alpine` job, `ci.yml:273`, runs on x86-64: a matrix entry for it is a later card if wanted).
* **files**: `.github/workflows/ci.yml`, `docs/targets.txt`; last: `README.md`.
* **steps**:
  1. `git switch -c work/L-19a` from `main`.
  2. The label check; the job; push; `gh run watch --exit-status $(gh run list --branch work/L-19a --workflow ci --limit 1 --json databaseId --jq '.[0].databaseId')`.
  3. A failing test is a finding: fix it with a unit test first (see it fail on the arm64 job, fix, pass) if it is the library's, or skip by name with a reason in the job only if it needs what the runner lacks (the number of skips is reported).
  4. `docs/targets.txt` and README with the run's date and commit; format; ACCEPT.
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI with `arm64` green on the last commit (**CI-only** for the job itself).
* **stop if**: no arm64 Linux hosted runner is available to this repository (private-repository billing or the label missing): report the label list and stop; a library fault on AArch64 that needs a public format's change.
* **docs**: `CHANGELOG.md`, "## 1.9.0 — unreleased", "### Added": "CI runs the suite and the sanitizer preset on Linux arm64"; `docs/targets.txt`; `docs/Roadmap.md` item "Arm supported well": the "no Linux arm64 job" gap closed (stays ⬜ until L-19b); no decision.

## L-19b - The targets list is checked, and says how each target was shown

* **id**: L-19b
* **goal**: `gate.targets` reads `docs/targets.txt` and fails when a line is malformed, when a `runner`, `qemu` or `built` line names a job that is not in `.github/workflows/ci.yml`, or when a line says "tested on hardware" without a board, a date and a commit; and the README's table is made from the file.
* **why**: the Roadmap item says "the support is listed per target with how it was shown" and "a test run on hardware is recorded with the commit it ran"; today `docs/targets.txt` is a file nothing reads (L-17, L-18a and L-19a each edit it by hand and the README beside it), so it can claim a job that was renamed.
* **needs**: L-17, L-18a, L-19a
* **size**: small
* **decided**:
  * **D-158 (copy into docs/Decisions.md, "A target is listed with how it was shown, and a gate reads the list")**: the file's columns are `target|how|job-or-board|date|commit|note` (its header, `docs/targets.txt:1`); `how` is one of `hardware` (tested on a named board: `job-or-board` is the board's name, `date` and `commit` required), `qemu` (tested under an emulator: a CI job name, `date` and `commit` required), `runner` (tested on a CI runner: a CI job name; the existing lines have none of date and commit and need none), `built` (built only: a CI job name), `none` (not tried: no job). A job name must be a job id under `jobs:` in `.github/workflows/ci.yml` (or `bench.yml` or `eva.yml`: the gate reads all three). The gate is CMake (`cmake/gates/GateTargets.cmake`, registered beside `gate.limits`), in the form of the other gates. *Rejected*: letting the README be the source (a table a human edits has no reader), and a fifth status "emulated on hardware" (not a thing). *Compatibility*: none.
  * **The README table**: the table at `README.md:210-220` is regenerated from the file by `scripts/generate/targets.cmake` between markers `<!-- targets:begin -->` and `<!-- targets:end -->` (the form `scripts/generate/limits.cmake` uses for its blocks: `-DWRITE=README.md`), and `gate.targets` also fails when the README's block differs from a regeneration. The README columns: Target, How it was shown (the words "tested on hardware", "tested under qemu", "tested on a CI runner", "built only", "not tried"), Where (the job or board), Date, Commit.
  * **Stays out**: a board's own test report (L-19-m); CI jobs for riscv64.
* **files**: `cmake/gates/GateTargets.cmake` (new), `scripts/generate/targets.cmake` (new), `cmake/ConfigForgeGates.cmake`, `tests/gates/targets/` (new: fixtures), `tests/CMakeLists.txt`, `README.md`, `docs/targets.txt`, `docs/Decisions.md`; last: `docs/Codemap.md`.
* **steps**:
  1. `git switch -c work/L-19b` from `main` (after the three needs land).
  2. Test first: `tests/gates/targets/` holds fixtures (`good.txt`, `bad-how.txt`, `bad-job.txt`, `hw-no-commit.txt`, a tiny `ci.yml`) and `check.cmake` running the gate on each, expecting pass, then three failures with the line named; add `gates.targets` to `tests/CMakeLists.txt`. **See it fail**: exit 1 (no gate).
  3. Write the gate and the generator; `ctest -R "gates.targets|gate.targets"` exit 0 with the real file and fixtures.
  4. Put the markers in the README, regenerate (`cmake -DWRITE=README.md -P scripts/generate/targets.cmake`); **see it fail**: edit one cell of the README block; the gate exits 1; restore.
  5. Documents; format; codemap; ACCEPT.
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI (`gate.*` run in every build); `ctest -R "targets"` exit 0.
* **stop if**: a job named in `docs/targets.txt` by an earlier card does not exist under that id (fix the line to the real id: not a stop unless the job itself is missing).
* **docs**: `CHANGELOG.md`, "## 1.9.0 — unreleased", "### Added": "gate.targets: the list of targets and how each was shown is read and checked (D-158)"; `docs/Maintaining.md` (the gates list: `grep -n "gate.limits" docs/Maintaining.md`); `docs/Roadmap.md` item "Arm supported well, added as devices can be reached": the list done, the item stays ⬜ for hardware (L-19-m) and for NEON and SVE (2.4.0, not this group); D-158 as decided above.

## L-19-m - Arm devices run, recorded with their commit

* **id**: L-19-m
* **goal**: for each Arm board the maintainer can reach, the suite is run on it and the line of `docs/targets.txt` becomes `hardware` with the board's name, the date and the commit.
* **why**: the Roadmap item says a target "moves up the list as a device or an emulator run becomes available, and a test run on hardware is recorded with the commit it ran"; no agent has a board.
* **needs**: L-19b (the gate that checks the line)
* **size**: small
* **decided**: the maintainer's step. On a board with a C compiler and CMake (a Raspberry Pi, an Apple-silicon Linux VM is not "hardware" for this list: it is a virtual machine and is `runner`-like: the maintainer says which word fits): `cmake --preset default && cmake --build --preset default && ctest --preset default` exit 0 on the commit named; for a Cortex-M board, the library built by L-18a's toolchain file plus the maintainer's own test program linked for the board's vendor SDK (the project ships none: **the agent part, L-18a, stands alone**).
* **files**: `docs/targets.txt` (the maintainer's line), `README.md` (regenerated).
* **steps**:
  1. The maintainer runs the command on the board and notes `git rev-parse --short HEAD`.
  2. A line `<board model, CPU>|hardware|<board name>|<date>|<commit>|<note>` in `docs/targets.txt`; `cmake -DWRITE=README.md -P scripts/generate/targets.cmake`; `ctest -R targets` exit 0.
* **accept**: `ctest -R targets` exit 0 and the commit named exists (`git cat-file -t <commit>` prints `commit`).
* **stop if**: a test fails on the board: a finding (a card of its own), the line is not written as `hardware`.
* **docs**: `docs/targets.txt`; `docs/Roadmap.md` item "Arm supported well" updated by the maintainer; `docs/Plan.md` "Waits on the maintainer": the row's status.

