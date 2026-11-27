# P - the vector library carried further, and optimisation from measurement

What this group is for: the Roadmap's "Profiling after every benchmark",
"Where optimization learns from", "Beat the others at everything", the live
page, "Benchmarks fair, and without mercy", the SWAR and SIMD backend's
remainder, and the OpenSSF Scorecard. Every card here follows the rules of
`docs/plan/P-word-at-a-time.md` lines 1-86 (the scalar loop stays the
reference; C89; no byte past the end; nothing changed without a number; a
slow result is not a verdict until the code could be fast; the compiler and
the C library come first). P-3, in that file, waits for 2.4.0 and is not
rewritten here; P-4 is done. Each card here is read cold against the tree
before it opens (docs/Plan.md, "Read again after the layout changes").

Versions (docs/Maintaining.md lines 99-114): cards that change only `bench/`,
`scripts/`, workflows or documents are 1.9.0 (the open version: the
benchmark's own tooling, which P-1 says may come early). Cards that change a
reader, or that measure a backend, are 2.4.0 ("speed"; "last, so it measures
the code as it will stay"). A 2.4.0 card adds the heading
`## 2.4.0 — unreleased` to `CHANGELOG.md` above `## 1.9.0 — unreleased` if it
is not there yet.

Decision numbers D-164 to D-170 are used by the cards below, once each.
D-164 P-5, D-165 P-6, D-166 P-10, D-167 P-11, D-168 P-12, D-169 P-13,
D-170 P-14.

Left out on purpose, with the reason:

* **Print speed for YAML, TOML and CBOR, and parsing in parallel, and in-situ
  and lazy reading.** P-11 makes the `write` mode for JSON; the others are
  cards written from what P-11 finds (docs/Performance.md, "Not measured in
  this round"); parallel and in-situ reading belong to 2.0.0's ways to read.
* **The Roadmap's "A live optimization and benchmark page" and "Benchmarks
  fair, and without mercy"** are built (P-1b: `.github/workflows/bench.yml`,
  `scripts/bench/page.py`, the fifteen documents of docs/Performance.md,
  "The documents", the cold run at 256 MiB, `bench.touch` and `bench.probes`).
  What is left of them is P-11 (print), P-12 (the lead lines) and P-5 (the
  profile beside the numbers).
* **The Scorecard's repository settings** (branch protection, required
  reviews, signed commits, the Best Practices badge): P-15-m.

---

## P-5 - A profile after every weekly benchmark, kept as an artifact and compared

* **id**: P-5
* **goal**: every scheduled benchmark run is followed by a callgrind profile of each of the five formats' readers that `cf_bench docs` covers, uploaded as the artifact `bench-profile`, and `scripts/bench/profdiff.py` lists the functions whose share moved by five points or more between two profiles.
* **why**: a benchmark says how fast, a profile says where the time goes (Roadmap, "Profiling after every benchmark"); a change is made only where a profile shows the cost, and the next profile shows whether the cost moved. Today the profile is taken by hand (`scripts/work/profile.py`, L-5) and its tables are pasted into `docs/Performance.md:199-295`.
* **needs**: nothing
* **size**: medium
* **status**: open
* **version**: 1.9.0
* **decided**:
  * **D-164 (text to copy into docs/Decisions.md).** "D-164 — The profile follows the benchmark. Each scheduled or manual run of `.github/workflows/bench.yml` ends with a profile of the reader of each format that `cf_bench docs` has a document for (JSON, YAML, TOML, CBOR, INI) on the documents `cf_bench docs <format>` makes (1 MiB and 16 MiB, as `scripts/work/profile.py` takes them), uploaded as the artifact `bench-profile` (JSON, 90 days). Only callgrind's tables are compared: a hosted runner gives `perf` no counters, and the cachegrind and massif output that `profile.py` also takes where valgrind is installed travels in the JSON but `profdiff.py` ignores it. A profile is never a gate: `bench-gate` counts instructions for pass and fail (D-111); the profile says where, and `scripts/bench/profdiff.py` says what moved. Rejected: comparing perf, cachegrind or heaptrack output in CI (it is not stable between runs on a shared machine)."
  * **`profile.py --json <path>`.** `scripts/work/profile.py` gets one more option, `--json <path>`. `Report` (`profile.py:106-136`) gets `self.tables = []` and `self.section = ""`; `Report.add` sets `self.section` to the line without its leading `### ` whenever a line it is given starts with `### ` (the Linux path adds `### <label>: <format>` before each format's tables, `profile.py:~349`, so the section is `linux: json` for `--local`); a table's title is the same for every format (`callgrind, instructions, self`), so the section is what tells the formats apart. `Report.table(title, rows, columns)` appends `{"section": self.section, "title": title, "columns": list(columns), "rows": [[name, share, ...] ...]}` to `self.tables`, where `name` is the function name without backticks and each share a float, unrounded, the rows being the six that are printed (after the sort and the cut), before it writes the Markdown; `Report.save()` writes, when a path was given, `{"schema": 1, "tables": self.tables}` with `json.dump(..., indent=1)` and a final newline. `Report.__init__` takes `json_path=None` as a second parameter after `path`; `main()` passes `a.json` (`ap.add_argument("--json", metavar="path", help="also write the tables as JSON")` beside `--local`, `profile.py:670-672`; the path is used as given, relative to the current directory). Nothing else in profile.py changes; with no `--json` its output is byte for byte what it is.
  * **`scripts/bench/profdiff.py <old.json> <new.json>`**, Python 3 standard library only (the style of `gate.py`). It reads only tables whose `title` starts with `callgrind`, matches them by (`section`, `title`), rows by function name (the first cell), and prints for each table, one line per function whose share (the second cell, a percent) differs by 5.0 points or more, or that is in one file and not the other: `<section>: <function> <old>% -> <new>% (<+/-delta> points)`; `appeared` and `gone` for the second kind. Exit 0 always (it informs, D-164); exit 2 for a file that is not schema 1 or cannot be read, with `profdiff: <file>: <reason>` on stderr.
  * **The job.** A job `profile` in `bench.yml`, `if: github.event_name == 'schedule' || github.event_name == 'workflow_dispatch'`, `runs-on: ubuntu-26.04`, `timeout-minutes: 60`, after the job `compare`'s steps' style (checkout pinned by the same commit as `bench.yml:81`, `sudo apt-get update && sudo apt-get install -y valgrind`), one step `python3 scripts/work/profile.py --local --json bench-profile.json json yaml toml cbor ini` (`cf_bench docs` makes a document for these five only, `bench/bench.c:812-838`; XML has none and is profiled by P-9's own commands; the runner has no `perf`: profile.py prints a `skipped:` line for each tool it lacks), one `actions/upload-artifact` step (same pinned commit as `bench.yml:105`) named `bench-profile`, `retention-days: 90`. A format whose profile could not be taken is a `skipped:` line and the job still passes (profile.py exits 1 only when none could be taken, and then the job fails).
  * **Where the numbers go on the page.** `docs/Performance.md`, at the end of "The profile" (before the line 295 paragraph "What it answers for P-3"), one paragraph: "Since P-5 every weekly run profiles JSON, YAML, TOML, CBOR and INI; the artifact `bench-profile` of the run, and `python3 scripts/bench/profdiff.py old.json new.json`, say which function's share moved." Nothing is drawn.
* **files**: `scripts/work/profile.py`; `scripts/bench/profdiff.py` (new); `scripts/bench/test_profdiff.py` (new); `scripts/README.md` (a row for `profdiff.py` and for profile.py's `--json`, beside `bench/page.py`'s at line 137); `bench/CMakeLists.txt` (a test `bench.profdiff` beside `bench.gate`, inside the same `if(Python3_Interpreter_FOUND)` block, lines 42-48); `.github/workflows/bench.yml`; last: `docs/Performance.md`, `docs/Decisions.md`, `CHANGELOG.md`, `docs/Roadmap.md`, `docs/Codemap.md` (generated).
* **steps**:
  1. `git switch -c work/P-5` from `main`.
  2. Write `scripts/bench/test_profdiff.py` (`unittest`, as `test_gate.py`): two fixtures built in the test as dicts written to a temp directory (each table `{"section": "linux: json", "title": "callgrind, instructions, self", ...}`); cases: a table whose title does not start with `callgrind` is ignored; a function moving 4.9 points is not listed, 5.0 is; a function in one file only is `appeared` or `gone`; a file with `"schema": 2` exits 2 with the message above; a missing file exits 2.
  3. See it fail: `python3 -m unittest scripts/bench/test_profdiff.py` (ImportError: no `profdiff`). Exit non-zero.
  4. Write `scripts/bench/profdiff.py` as decided. `python3 -m unittest scripts/bench/test_profdiff.py` exits 0.
  5. In `scripts/work/profile.py` make the `Report` and `main` changes as decided. Check: `python3 scripts/work/profile.py --local --json $HOME/p5.json json` (on Linux or WSL with valgrind; skip this check where there is no valgrind, and say so in the report) writes a file whose `tables` has at least one table whose `section` is `linux: json`; and `python3 -c "import json;d=json.load(open('$HOME/p5.json'));assert d['schema']==1 and d['tables']"` exits 0. Run it twice and `python3 scripts/bench/profdiff.py $HOME/p5.json $HOME/p5.json` prints nothing and exits 0.
  6. Add `bench.profdiff` to `bench/CMakeLists.txt` as a copy of `bench.gate`'s `add_test` (lines 43-47) with `scripts/bench/test_profdiff.py`, inside the same `if` block.
  7. Add the job to `.github/workflows/bench.yml`; `actionlint .github/workflows/bench.yml` exits 0 if actionlint is installed (CI's `gate.workflows` runs it otherwise).
  8. Add the paragraph to `docs/Performance.md`; D-164 to `docs/Decisions.md` (and its row in the index table at the top, as the other rows); CHANGELOG, Roadmap.
  9. `python scripts/work/work.py format`; `cmake -P scripts/generate/codemap.cmake`; `cmake -P scripts/generate/apiref.cmake`; ACCEPT.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `python3 -m unittest scripts/bench/test_profdiff.py` exit 0. The first scheduled run after the merge uploads `bench-profile` (not an acceptance condition: the triggers exist only from the default branch, `bench.yml:20-22`).
* **stop if**: profile.py's `Report.table` is no longer the one place every table passes (then a table is missing from the JSON): stop and write what writes tables elsewhere; callgrind cannot run on the runner image (the first dispatched run fails on it twice): leave the job out, keep the script and test, and write why under notes.
* **docs**: `CHANGELOG.md`, "## 1.9.0 — unreleased", "### Added": "The weekly benchmark is followed by a callgrind profile of every format (artifact `bench-profile`), and `scripts/bench/profdiff.py` shows what moved between two (D-164, P-5)." `docs/Decisions.md`: D-164 as above. `docs/Roadmap.md`: the item "Profiling after every benchmark, and the optimisations it points to" gets 🟨 (the profile is kept; the optimisations are P-7, P-8, P-9).

---

## P-6 - A cost that should not be there: bytes per instruction across sizes and formats

* **id**: P-6
* **goal**: `scripts/bench/scaling.py` reads a full results file and reports every cell whose cost per byte grows with the document's size, or is five times the median of the same document in the other formats, and a list of accepted outliers with their reasons (`bench/compare/scaling.expected`) keeps the report quiet when nothing new is found.
* **why**: "The profile is also read for the library's own faults: a cost that should not be there is a bug before it is an optimisation" (Roadmap); P-word-at-a-time's preamble says a scan whose cost grows faster than its input is a fault, fixed when found. The quick baseline already holds one that looks like it: `json/numbers-edge/4` is 8.86e9 instructions and `toml/numbers-edge/4` 8.92e9 (`bench/baseline/instructions.json:16,21`) against 4.6e8 to 5.0e8 for `yaml/numbers-edge/4` and `cbor/numbers-edge/4` (lines 31 and 10): about 19 times the others on the same document name. P-7 looks into it; this card makes the finding automatic from now on.
* **needs**: nothing
* **size**: medium
* **status**: open
* **version**: 1.9.0
* **decided**:
  * **D-165 (text to copy).** "D-165 — Outliers are reported, and the known ones are listed. `scripts/bench/scaling.py` flags a speed cell of ConfigForge whose MiB/s at 256 MiB is below 70% of its MiB/s at 1 MiB (the cost grows with the size: memory, a quadratic loop, a cache), and a quick or full instruction cell whose instructions per byte of the document are five times the median of the same document name in the other formats. A flagged cell that is in `bench/compare/scaling.expected` with a reason is not an error; one that is not there fails the unit test `bench.scaling` on its own fixtures only, never CI's benchmark (which informs, D-111). Rejected: failing the weekly run on a flag (a shared runner's speed varies by tens of percent; the instruction rule alone is stable, and a flag on it is a finding for a card, not a build)."
  * **Input.** One results file, the schema of `scripts/bench/run.py` (`page.py --validate` checks it): `cells` is a list of `{"lib", "format", "doc", "size_mib", "mode", ...}` with `"mib_s"` on `speed` cells and `"instructions"` and `"bytes"` on `count` cells (`bench/compare/main.cpp:338-347,403-406`; `run.py:144-145,190-199`). Only `lib == "configforge"` is read, for both rules. A full run has `speed` cells at 1, 16 and 256 MiB and `count` cells at 1 MiB only; a quick run has `count` cells at 4 MiB (`run.py:156-199`): the size rule needs both `speed` sizes in the file and skips a format that lacks them; the per-byte rule needs the `count` cells and skips a document name found in fewer than three formats.
  * **Output.** One line per flag: `scaling: <format>/<doc>: <rule> <numbers>`; rule `size` (`<mib_s at 1> -> <mib_s at 256> MiB/s, <ratio>`) or `per-byte` (`<instr/byte> against a median of <m> over <formats>, <ratio>x`). Exit 0 when every flag is in the expected file, 1 when one is not, 2 for a file that is not readable or not schema 1. The expected file: one line a cell, `<format>/<doc> <rule> <reason in words>`, `#` comments.
  * **The first expected file** is written from the committed quick baseline in step 5: it holds every flag the script prints on the fixture built from `bench/baseline/instructions.json`: the `numbers-edge` ones with the reason `not examined, P-7` (P-7 removes the ones it explains), any other (the baseline's `xml/whitespace/4` is one) with `not examined, P-9` (P-9 profiles XML and the whitespace documents). It has no `size` lines: the committed baseline has no `speed` cells, and the slowing with size that Performance.md records (CBOR 400 to 285 MiB/s, INI 1455 to 1045, "Baseline") is 71% of the 1 MiB speed, which the 70% rule does not flag. A `size` flag that the first weekly run prints is added by a commit that says which run, with the reason `a cost of memory, not of a scan (Performance.md, Large documents)` if the profile shows it so.
  * **Where it runs.** `bench.yml`'s job `compare`, after "The full run", one step `python3 scripts/bench/scaling.py bench-full-${{ matrix.compiler }}.json --expected bench/compare/scaling.expected || true` (it informs; the step is `continue-on-error: true`, and prints the flags into the job's log). The unit test is `bench.scaling`.
* **files**: `scripts/bench/scaling.py` (new); `scripts/bench/test_scaling.py` (new); `bench/compare/scaling.expected` (new); `scripts/README.md` (a row for `scaling.py`, as above); `bench/CMakeLists.txt`; `.github/workflows/bench.yml`; last: `docs/Performance.md`, `docs/Decisions.md`, `CHANGELOG.md`, `docs/Roadmap.md`, `docs/Codemap.md`.
* **steps**:
  1. `git switch -c work/P-6` from `main`.
  2. Write `scripts/bench/test_scaling.py` with its own small fixtures (dicts): a format whose 256 MiB speed is 60% of its 1 MiB speed is flagged `size`; 75% is not; a `count` cell at five times the median per byte is flagged `per-byte`, at 4.9 times is not; a flagged cell listed in an expected file gives exit 0, one not listed exit 1, a malformed file exit 2; the expected file's `#` lines and blank lines are ignored.
  3. See it fail: `python3 -m unittest scripts/bench/test_scaling.py` exits non-zero (no module).
  4. Write `scripts/bench/scaling.py` as decided; the unit test exits 0.
  5. A fixture of the real shape: `python3 scripts/bench/run.py --scope quick` needs the comparison built; instead build the file from the committed baseline: `python3 -c "import json;b=json.load(open('bench/baseline/instructions.json'));print(json.dumps({'schema':1,'kind':'full','cells':[{'lib':'configforge','format':k.split('/')[0],'doc':k.split('/')[1],'size_mib':4,'mode':'count','instructions':v,'bytes':4*1048576} for k,v in b['cells'].items()]}))" > $HOME/p6-quick.json`. Run `python3 scripts/bench/scaling.py $HOME/p6-quick.json` and read what it flags; the `json/numbers-edge` and `toml/numbers-edge` cells must be among them. Write each flag as `<format>/<doc> per-byte <reason>` (the reasons decided above) into `bench/compare/scaling.expected`.
  6. Add `bench.scaling` to `bench/CMakeLists.txt` as a copy of `bench.gate`'s `add_test` (lines 43-47), inside the same `if(Python3_Interpreter_FOUND)` block.
  7. Add the step to `bench.yml`'s `compare` job.
  8. Docs: a paragraph under "How it is measured" in `docs/Performance.md` ("Faults are looked for: ..."), D-165, CHANGELOG, Roadmap.
  9. Format, codemap, apiref, ACCEPT.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `python3 -m unittest scripts/bench/test_scaling.py` exit 0; the step-5 run exits 0 with the expected file in place and 1 with it removed.
* **stop if**: the `bytes` field is not on a `count` cell of a real results file (check `bench/compare/main.cpp`'s count output: `"bytes"` at line 342): then the per-byte rule divides by the document's size from `size_mib` instead, and the card writes that under "decided" as an Organiser's answer; no flag appears on the numbers-edge cells (then the rule's five is wrong: stop and write the ratios under notes).
* **docs**: `CHANGELOG.md`, "## 1.9.0 — unreleased", "### Added": "`scripts/bench/scaling.py` reports the benchmark cells whose cost per byte grows with size or stands out among the formats (D-165, P-6)." `docs/Decisions.md`: D-165. `docs/Roadmap.md`: "Profiling after every benchmark, and the optimisations it points to" stays 🟨.

---

## P-7 - Why a real number costs 2000 instructions a byte in JSON and TOML

* **id**: P-7
* **goal**: the cause of `json/numbers-edge/4` and `toml/numbers-edge/4` costing about 19 times the same document in YAML and CBOR is written down from a callgrind profile, and removed when it is a fault the card can fix in the lines named below, with each cell's instructions before and after.
* **why**: the profile of P-1 saw only the `mixed` documents (docs/Performance.md:231-247); `numbers-edge` (the int64 limits, reals of 17 significant digits, the largest and smallest exponents, subnormals) is the document that hurts the real-number reader, and it costs 8.9e9 instructions for 4 MiB (`bench/baseline/instructions.json:16,21`). A cost that should not be there is a fault before it is an optimisation (Roadmap).
* **needs**: P-6 (its flags are the card's evidence)
* **size**: large (a profile and, perhaps, a fix in the number reader)
* **status**: open
* **version**: 2.4.0
* **decided**:
  * **Measure first, change second.** The profile: `valgrind --tool=callgrind --collect-atstart=no --toggle-collect=cfb_cell --callgrind-out-file=cg-json-ne.out <build-cfb>/cfb_compare --lib configforge --doc $HOME/p7-docs/json-numbers-edge-4.json --mode count` (the documents: `<build-bench>/bench/cf_bench --emit-docs $HOME/p7-docs --size 4`; the build as P-word-at-a-time P-3 step 3 says; a change to the library is measured only after `rm -rf build-cfb build-cfb-clang` and a fresh configure and build, because `bench/compare/CMakeLists.txt` builds ConfigForge into `build-cfb/configforge` only when its library is not there yet), then `callgrind_annotate --inclusive=no cg-json-ne.out | head -30`; the same for `toml-numbers-edge-4.toml` and, for the comparison, `yaml-numbers-edge-4.yaml`. The top six functions of each, with their share, go into `docs/Performance.md` under "The profile" as "### numbers-edge (P-7)".
  * **What counts as a fault, and what the card may fix.** The number reader is `lib/numeric/src/numeric.c` (`cf_text_to_u64`, 28-53; the real conversion `cf_text_to_f64`, 725-884). The card may change only code that makes the same result by less work, and only where one of these is the cause: (a) a conversion that is done twice for one value (once to find the end, once for the value); (b) a loop over the digits that restarts from the first digit after each one; (c) an allocation or a `cf_fmt` call per number; (d) a slow exact path taken for a value a short path settles (17 digits and an exponent that fit a double exactly). It may not add an algorithm (Eisel-Lemire, a table of powers, a 128-bit multiply): that is a new card (survey row 8, P-10).
  * **The test that holds the result.** `lib/numeric/tests/test_numeric.c` (its `cf_text_to_f64` cases, lines 100-137) already checks reals against fixed expectations; the card adds to it one case holding the twelve real tokens of the generator's `edge_tok` (`bench/bench.c:1777-1788`: `1.2345678901234567` to `0.1`) as `(text, bits)` pairs, `bits` a 64-bit hexadecimal word computed once with `python3 -c "import struct,sys;print('0x%016XULL'%struct.unpack('<Q',struct.pack('<d',float(sys.argv[1])))[0])" <text>` and pasted, and compares the double to it with `memcpy` into an `unsigned long long`. It passes before and after.
  * **Kept by the rule of P-3 (P-word-at-a-time.md, "Measured, and the rule that keeps a change")**: the change stays when `json/numbers-edge/4` and `toml/numbers-edge/4` have at least 5% fewer instructions, with GCC and with Clang, and no other quick cell has more than 1% more with either. Otherwise it is reverted by `git revert`, so that the numbers stay in the history.
  * **When it is not a fault the card can fix**, or is not a fault at all (the document is mostly the slowest case on purpose): the finding is written, with each function's share and its source line, in `docs/Performance.md`, the cells' two lines go into `bench/compare/scaling.expected` with the real reason, and the card ends with no code change.
* **files**: `docs/Performance.md`; `bench/compare/scaling.expected`; when a fault is fixed: `lib/numeric/src/numeric.c` and `lib/numeric/tests/test_numeric.c`; last: `bench/baseline/instructions.json` (from CI's `bench-quick` artifact, after CI is green: `python3 scripts/bench/gate.py --write bench-quick.json`), `CHANGELOG.md`, `docs/Roadmap.md`, `docs/Codemap.md`.
* **steps**:
  1. `git switch -c work/P-7` from `main`.
  2. The three profiles as decided; paste the tables into `docs/Performance.md`.
  3. Read the top function's source (a range of at most 120 lines) and decide by the rule above whether (a) to (d) is the cause. Write the answer, with `file:line`, under "### numbers-edge (P-7)".
  4. If a fault is found: write the test of the decided kind first; run it; it passes on the unchanged code (it pins the result). See it fail: make the fix's early exit wrong by one digit (a deliberate fault) and see the test fail; put it back.
  5. Make the change. `ctest` in the default build, and with `-DCF_LANES_BACKENDS=scalar` (it must not matter here: numeric does not use lanes).
  6. The quick run with both compilers (P-word-at-a-time P-3 steps 3 and 4d: the command of 3 for `p7-<gcc|clang>-0.json` before the change, and again, after a fresh build, for `p7-<gcc|clang>-1.json`; then 4d's comparison): `numbers-edge` must fall by at least 5%, the others stay within 1%.
  7. `cmake -DMODE=quick -DEXTRA="conformance;fuzz" -DFUZZ_SECONDS=60 -DSOURCE=index -DJOBS=3 -DWORK=$HOME/cfv -P scripts/verify.cmake`: the conformance suites and a minute of libFuzzer on each reader.
  8. Commit with the table of each named cell, before and after, GCC and Clang. Or, by the rule, commit and revert.
  9. After CI is green: the baseline from the artifact, as decided, committed. Docs; format; codemap; apiref; ACCEPT.
* **accept**: ACCEPT-LINUX, with step 7 passing for the last state; ACCEPT-WINDOWS; ACCEPT-CI, bench-gate included.
* **stop if**: a conformance case or a fuzzer finding moves (revert and write the input under notes); the cause is not one of (a) to (d) and the fix would be an algorithm: close the card with the finding and write a card for the algorithm from it (P-10's row 8), no code change; three failures on the same step.
* **docs**: `CHANGELOG.md`, "## 2.4.0 — unreleased", "### Changed" (only if code changed): "Reading a real number costs <n>% fewer instructions in JSON and TOML (P-7)." `docs/Roadmap.md`: nothing marked (the item is not done by it).

---

## P-8 - CBOR writes a place for every scalar: build it by hand, same bytes

* **id**: P-8
* **goal**: `cbor_place` builds its `"<name>, byte <offset>"` string with a copy and `cf_u64_to_text` instead of `cf_fmt`, and its bytes are the same as before, kept only if the CBOR cells fall by at least 5%.
* **why**: in CBOR "the time is not a scan but text made for each value (`cf_vformat` and `cf_u64_to_text`)": `cf_vformat` is 34% of the instructions and 24% of the time (docs/Performance.md:238-247, 266-271). `cbor_emit` calls `cbor_place` for every scalar (`lib/serialize/src/cbor.c:201-210`), and `cbor_place` formats `"%s, byte %zu"` into `s->where` (183-188) although the place is read only when a sink refuses the value or explains it. It is work to remove, not to speed up.
* **needs**: nothing
* **size**: small
* **status**: open
* **version**: 2.4.0
* **decided**:
  * **Same bytes.** The new body of `cbor_place` (`cbor.c:183-188`): `n = strlen(s->name)`; copy `min(n, sizeof s->where - 1 - 7 - 20)` bytes of `s->name` into `s->where` (`char where[CF_PATH_MAX + 32]`, `cbor.c:47`) with `memcpy`, where 7 is the length of `", byte "` and 20 the most digits of a `cf_u64`; then `", byte "`; then `cf_u64_to_text((cf_u64)off, s->where + k, sizeof s->where - k)`, which terminates (`lib/port/include/configforge/cf_port.h:410-414`: "Always terminates when cap > 0"); `at->file = s->where; at->line = 0; at->col = 0;` as now. `cf_fmt`'s `%s` cuts at the buffer's end with no marker; this cuts the name to the room left, so the offset is never cut: for a name over the room the old text lost the offset, and the new keeps it. That one difference is accepted and tested (`decided` here, not a design question): the name `s->name` is a file or a label, no longer than `CF_PATH_MAX` in any test or use, so the room is never short.
  * **Not changed**: `cbor_fail` and `cbor_definite`'s `cf_error_set` calls (error paths, 92 and 214-218). `cf_text_is_utf8` (candidate 7 of P-3).
  * **Kept by P-3's rule** (P-word-at-a-time.md, "Measured, and the rule that keeps a change"): `cbor/mixed/4` and `cbor/numbers-edge/4` at least 5% fewer instructions, with GCC and with Clang; no other quick cell more than 1% more.
  * **Test.** A new function `test_cbor_place` in `lib/serialize/tests/test_holds.c` (no CBOR test with a capturing sink exists: `lib/config/tests/test_cbor.c` reads through the builder), called from that file's `main`, beside `reads` (195-221), whose sink set-up it copies (`on_open`, `on_end`, `on_text`, `on_work`) with its own `scalar` that copies `at->file` with `strncpy` into a `static char g_place[256]` and terminates it. It does nothing when `cf_codec_by_name("cbor") == NULL`. It decodes `"\xa1\x61\x78\x01"` (the map `{"x": 1}`, four bytes) from `cf_blob_mem(&in, bytes, 4, "place")` and requires `g_place` to equal `place, byte 3`; then `"\x81\x01"` (the array `[1]`) and requires `place, byte 1`. Written first; it passes on the unchanged code. Its ctest is `unit.test_holds` (`ctest -R test_holds`).
* **files**: `lib/serialize/src/cbor.c`; the test file named above; last: `bench/baseline/instructions.json`, `docs/Performance.md`, `CHANGELOG.md`, `docs/Roadmap.md`, `docs/Codemap.md`.
* **steps**:
  1. `git switch -c work/P-8` from `main`.
  2. Write the test; run `ctest -R test_holds` in the default build: exit 0 (it pins what is).
  3. The baseline: P-word-at-a-time P-3's step 3 (the quick run, GCC and Clang, a fresh build directory each, file `p8-<gcc|clang>-0.json`).
  4. Make the change in `cbor_place`.
  5. See it fail: write `k` one too large in the offset's position (`s->where + k + 1`) and see the test fail; put it back.
  6. `ctest` in the default and scalar builds.
  7. The quick run again (`p8-<gcc|clang>-1.json`) and the comparison command of P-3 step 4d; apply the rule.
  8. The conformance and fuzz run of P-7 step 7.
  9. Commit with its table, or commit and revert. After CI: the baseline from the artifact. Docs; format; codemap; apiref; ACCEPT.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI, bench-gate included; the new test exit 0.
* **stop if**: a conformance case, a refusal's text or a fuzzer finding moves; `cbor_place` is no longer called per scalar (then the cost is somewhere else: write the profile under notes); the rule is not met: revert and write the table.
* **docs**: `CHANGELOG.md`, "## 2.4.0 — unreleased", "### Changed": "The CBOR reader builds each value's place without a formatted print: <n>% fewer instructions on `cbor/mixed` (P-8)." A row in `docs/Performance.md`, "What was tried". `docs/Roadmap.md`: the item "Profiling after every benchmark, and the optimisations it points to" stays 🟨.

---

## P-9 - The second profile: the formats and the lines P-1 did not look at

* **id**: P-9
* **goal**: `docs/Performance.md` holds a profile, after P-3, P-7 and P-8, of every format's reader on the documents the first profile skipped (XML, INI's remainder, the `strings-escape`, `whitespace` and `records` documents), with the top functions and each one's source line, and a numbered list of at most five candidates, each with the cell that would show it and the rule that keeps it.
* **why**: "A wave that finds nothing worth keeping ends with its profile written down, and the next wave starts from it" (P-word-at-a-time.md, "In waves"): the waves are opened by a profile since the last one moved where the time is. P-1's profile covered `mixed` for five formats and no XML (docs/Performance.md:231-247).
* **needs**: P-3, P-7, P-8 (it profiles the code as it will stay)
* **size**: medium
* **status**: open
* **version**: 2.4.0
* **decided**:
  * **The command is P-7's, not `profile.py`'s** (`cf_bench docs` makes one document for each of JSON, YAML, TOML, CBOR and INI, none for XML, `bench/bench.c:812-838`, 1016-1040, so it cannot reach the documents below). The documents: `<build-bench>/bench/cf_bench --emit-docs $HOME/p9-docs --size 4`. For each (format, document) pair of the list below and for GCC (`build-cfb`) and Clang (`build-cfb-clang`), built fresh as P-7 says: `valgrind --tool=callgrind --collect-atstart=no --toggle-collect=cfb_cell --callgrind-out-file=cg-<format>-<doc>-<gcc|clang>.out <build>/cfb_compare --lib configforge --doc $HOME/p9-docs/<file> --mode count`, then `callgrind_annotate --inclusive=no <out> | head -30`. **The pairs (17)**: XML: `mixed`, `strings-plain`, `strings-escape`, `whitespace`; INI: `strings-plain`, `whitespace`, `records`; JSON, YAML and TOML each: `strings-escape`, `whitespace`, `records` (the first profile's `mixed` is not repeated); CBOR: `records`. Per-function lines: `callgrind_annotate --auto=yes --inclusive=no <out> <source file>` for the top three functions of each pair.
  * **What is written**: one table a pair (top six functions, share, `file:line` of the hottest line of the top three; GCC's, with Clang's top function named when it differs), the machine and compiler, and under it, for each format, the answer to: is the top function a scan (a word-at-a-time or a library call can take it: P-3's forms), a conversion (P-7's), a formatting (P-8's), an allocation, or the code's own structure (calls through a table per byte, state copied)?
  * **The candidates list** has at most five rows: `| # | function (file:line) | share | the form | the cell that must improve | the cell that must not regress |`. A candidate is listed only when its function is at least 10% of some format's instructions. A form is one of: restate the loop for the optimizer; a C-library call; a lanes operation; remove work; leave. Nothing is written as a card here; the maintainer or the organiser writes a card from a row.
  * **No code is changed.** The card writes `docs/Performance.md` and nothing else.
* **files**: `docs/Performance.md`; last: `CHANGELOG.md` (no entry: documents only), `docs/Roadmap.md`.
* **steps**:
  1. `git switch -c work/P-9` from `main` (after P-3, P-7 and P-8 landed).
  2. The documents, then the 34 profiles (17 pairs, two compilers) as decided, the `.out` files under `$HOME/p9-prof/`.
  3. Per-line listings for the top three functions of each format.
  4. Write the section "### The second profile (P-9)" after "### numbers-edge (P-7)" with the tables, the answers and the candidates.
  5. `python3 scripts/bench/page.py --check docs/Performance.md` exit 0 (the static block is not touched).
  6. Documentation only: commit straight to `main` (CLAUDE.md "Habits").
* **accept**: `python3 scripts/bench/page.py --check docs/Performance.md` exit 0; `cmake -P scripts/generate/codemap.cmake` leaves `git diff --stat docs/Codemap.md` empty.
* **stop if**: callgrind cannot run on this machine (then the card waits for one that can; nothing is written from perf alone, because the candidates' cells are instruction counts); fewer than P-3, P-7 and P-8 have landed.
* **docs**: `docs/Performance.md` only.

---

## P-10 - What the fastest libraries do, as a written survey with its numbers

* **id**: P-10
* **goal**: `docs/Performance.md` holds a table, one row an idea of yyjson, simdjson and RapidJSON (the list below), each with the file and line in the pinned source where the idea is, ConfigForge's counterpart as `file:line`, the benchmark cell where the gap shows with both libraries' figures, and one verdict from a fixed list; and `scripts/bench/check_survey.py` fails when a cell it names is not in the baseline or a verdict is outside the list.
* **why**: the Roadmap's "Where optimization learns from" says: read before the P cards' forms are chosen, and cited in each card's notes; its claims about yyjson's single pass and pool, and RapidJSON's in-situ and NEON, are "not confirmed" and are to be read in source before they are cited. A card is written only for what gives something checkable: a written survey with its numbers.
* **needs**: nothing
* **size**: medium
* **status**: open
* **version**: 2.4.0
* **decided**:
  * **D-166 (text to copy).** "D-166 — Optimisation ideas are surveyed against source, and each has one verdict. `docs/Performance.md`, 'What the fastest libraries do', holds a row for each idea of yyjson, simdjson and RapidJSON that a ConfigForge optimisation card might take, with the library's file and line at the commit pinned in `bench/compare/libraries.cmake`, the counterpart in this tree, the benchmark cell and both libraries' numbers, and a verdict: `take as a lanes operation`, `take as a layer operation` (restated for every format), `JSON only` (the exception, named as one), `already here`, or `not taken` with the reason. Ideas are taken, not code (C++, and the licences differ). `scripts/bench/check_survey.py` checks the table's form; it cannot check that the reading was right: a reader checks that cold."
  * **The ideas, one row each (twelve; the worker reads the pinned source and fills what it finds, and a row it cannot find in the source says `not found in <commit>` and gets the verdict `not taken`):**
    1. yyjson: reading in one pass with no separate tokenizer (`yyjson_read_opts` in `src/yyjson.c`); ConfigForge: the JSON reader, `lib/serialize/src/json.c`.
    2. yyjson: the document held in two contiguous areas, one for values and one for strings; ConfigForge: the arena, `cf_arena_alloc` in `lib/base/src/alloc.c`.
    3. yyjson: the number reader (integers and reals; its use of a table of powers); ConfigForge: `lib/numeric/src/numeric.c`.
    4. yyjson: the string scan for the closing quote (how many bytes at a time); ConfigForge: `json.c:193-247` `js_string`.
    5. simdjson: stage 1, the structural index with SIMD (Langdale and Lemire, VLDB Journal 28(6), 2019); ConfigForge: none (a scan per token): verdict by the card's reading, with the memory it needs.
    6. simdjson: On-Demand, reading only what is asked; ConfigForge: none (2.0.0's lazy reading).
    7. simdjson: eight digits at a time (`is_made_of_eight_digits_fast`, `parse_eight_digits_unrolled` in `include/simdjson/generic/numberparsing.h`); ConfigForge: `lanes_ops.digits8` (`lib/lanes/include/lanes/lanes.h:115`), no caller.
    8. simdjson: reals by a 128-bit multiply against a table of powers of five (Eisel-Lemire); ConfigForge: P-7's finding in `docs/Performance.md` (when P-7 has not landed, the row says `P-7 not done` in that column and its verdict is `not taken: P-7 not done`).
    9. simdjson: a parser for the processor chosen at run time; ConfigForge: `lanes_best()`, `lib/lanes/src/lanes.c:185-189`.
    10. RapidJSON: in-situ parsing (strings decoded in the input's own buffer); ConfigForge: none (2.0.0's in-situ strings).
    11. RapidJSON: SSE2 and SSE4.2 whitespace skips (`SkipWhitespace_SIMD` in `include/rapidjson/reader.h`); ConfigForge: `js_space`, `lib/serialize/src/json.c` (find with `grep -n "js_space"`).
    12. RapidJSON: Grisu2 for printing reals, and NEON; ConfigForge: the real printer `cf_f64_to_text`, `lib/numeric/src/numeric.c:531-620`.
  * **Columns**: `| # | idea | library, file:line at <commit> | ConfigForge, file:line | cell | the library's figure | ConfigForge's figure | verdict |`. The cell is a `format/doc/size` key of `bench/baseline/instructions.json` or `-` when no cell shows it; the figures are instructions from that file for ConfigForge and, for the library, the median MiB/s of the latest `bench-full` artifact (`gh run download`), written with its date and run id; if no full run is reachable, the figure column says `no run yet` and the verdict is not `take as ...` for that row.
  * **`scripts/bench/check_survey.py <docs/Performance.md> <bench/baseline/instructions.json>`**: finds the table under the heading `### What the fastest libraries do`, checks eight cells in every row, the verdict (one of the four exact phrases of D-166, or `not taken: <reason>` with a non-empty reason; a row whose idea is not in the source says `not taken: not found in <commit>`), and that a non-`-` cell is a key of the baseline's `cells`. Exit 0 / 1 (`check_survey: row <n>: <what>` on stderr). The test is `bench.survey` (`scripts/bench/test_survey.py`: a good table, a bad verdict, an unknown cell, a missing heading).
  * **Not** in this card: any code, or a decision to take an idea. The verdicts are the next cards' reasons, not the cards.
* **files**: `docs/Performance.md`; `scripts/bench/check_survey.py` (new); `scripts/bench/test_survey.py` (new); `bench/CMakeLists.txt`; `scripts/README.md` (a row for `check_survey.py`, beside `bench/page.py`'s at line 137); last: `docs/Decisions.md`, `CHANGELOG.md`, `docs/Roadmap.md`, `docs/Codemap.md`.
* **steps**:
  1. `git switch -c work/P-10` from `main`.
  2. `cmake -DLIBS="$HOME/cfb-libs" -P scripts/bench/fetch.cmake` (the pinned sources; `bench/compare/libraries.cmake` names yyjson 0.13.0 at 6447536..., and the others).
  3. Write `scripts/bench/test_survey.py`; see it fail (`python3 -m unittest scripts/bench/test_survey.py` non-zero); write `check_survey.py`; the test exits 0.
  4. For each of the twelve ideas read the cited file (a range of at most 120 lines, found with `grep -n`), and fill the row.
  5. `gh run list --workflow bench --limit 3`, `gh run download <id> -n bench-full-gcc` for the library figures; if none, as decided.
  6. `python3 scripts/bench/check_survey.py docs/Performance.md bench/baseline/instructions.json` exit 0.
  7. `bench.survey` in `bench/CMakeLists.txt`; D-166; CHANGELOG; Roadmap; format; codemap; ACCEPT.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `python3 scripts/bench/check_survey.py ...` exit 0.
* **stop if**: the pinned sources cannot be fetched (no network): stop and report; more than four rows say `not found`: the commit's layout moved, stop and write which.
* **docs**: `CHANGELOG.md`, "## 2.4.0 — unreleased", "### Added": "`docs/Performance.md` surveys what yyjson, simdjson and RapidJSON do, row by row, with the source and the numbers (D-166, P-10)." `docs/Decisions.md`: D-166. `docs/Roadmap.md`: "Where optimization learns from: the fastest libraries, all of their good parts" gets 🟩.

---

## P-11 - Print speed in the comparison: JSON, four libraries

* **id**: P-11
* **goal**: `cfb_compare --mode write` times each JSON library writing a document it has already read, `scripts/bench/run.py` runs it, and the page's tables carry a print-speed line for ConfigForge, yyjson, RapidJSON and cJSON.
* **why**: "Each measure is its own line - parse speed, print speed, memory, code size" (Roadmap, "The aim"), and "Not measured in this round: printing speed (a card for the writers)" (docs/Performance.md:386-388). Parse speed, memory and size are in; print speed is the line that is missing before the README can say where ConfigForge wins and where it loses.
* **needs**: nothing
* **size**: large (an interface, four adapters, a mode)
* **status**: open
* **version**: 1.9.0
* **decided**:
  * **D-167 (text to copy).** "D-167 — Print speed is timed on a tree that already exists. `cfb_compare --mode write` reads the document once outside the timed region, with the library's own reader into the library's own tree (or, for ConfigForge, into a recorded list of events), then times writing it to a buffer in memory, compact, the way the library's documentation recommends; the output bytes are counted and their FNV-1a recorded. A library is set against another only when both write compact JSON (no indentation), UTF-8, with the same escapes that JSON requires (the bytes differ in a number's digits and a solidus: the line shows the byte count, never compares bytes). Rejected: timing parse and print together (it hides which is slow); indented output (libraries differ in the default)."
  * **The interface.** `struct Adapter` (`bench/compare/touch.h:43-49`) gets one more member at the end, `bool (*write)(const char* p, size_t n, void (*begin)(), void (*end)(), WriteResult& out)`, default `nullptr`; the existing aggregate initialisers (`{"yyjson", "yyjson", "json", CFB_VERSION, parse}`) stay valid (C++ value-initialises a missing trailing member). A new `struct WriteResult { size_t bytes; uint64_t fnv; std::string dump; };` beside `Touch` (`dump` is filled only when `--dump` was given). The adapter's `write` does the read outside the timer, then calls `begin()` immediately before the writing call and `end()` immediately after it and nothing else is inside. A `--mode write` run of an adapter whose `write` is null prints `{"mode":"write","accepted":false,"reason":"no writer"}` and exits 0.
  * **`--mode write` in `main.cpp`** (beside `count` and `memory`, `main.cpp:338-360`): `--reps` repetitions as `speed` takes them; each rep calls the adapter's `write` with a `begin` that records `clock_gettime(CLOCK_MONOTONIC)` and an `end` that records the difference; prints `"mode":"write","accepted":..,"mib_s":<median of bytes written / 1048576 / seconds>,"best_mib_s":..,"bytes":<out bytes>,"fnv":"<hex>","reps":..` in the format of `speed`'s line (`main.cpp:403-406`). `--dump <file>` writes the output of the last repetition to that file.
  * **The four adapters.** yyjson: `yyjson_read` outside the timer, then `char* s = yyjson_write(doc, YYJSON_WRITE_NOFLAG, &len)` inside, `free(s)` after `end()`. RapidJSON: `Document d; d.Parse(p)` outside; a `StringBuffer sb; Writer<StringBuffer> w(sb); d.Accept(w);` inside. cJSON: `cJSON_Parse` outside, `char* s = cJSON_PrintUnformatted(root)` inside, `cJSON_free` after. ConfigForge: the decode into a list of events outside (a `cf_doc_sink` whose callbacks append to a `std::vector` of `{kind, name, value copy}`; the six callbacks of `bench/compare/adapters/configforge.cpp:18-95`), then inside: the JSON codec's `encoder` (`cf_codec.encoder`, `lib/serialize/include/configforge/cf_serialize.h:234`, obtained with the lookup the adapter's `parse` uses) is run on a growable `cf_sink` the adapter owns and the events are replayed into the `cf_doc_sink` it returns; after `end()` the bytes are counted.
  * **Docs for `run.py`.** `run.py` runs `write` for the `mixed`, `strings-plain`, `strings-escape` and `numbers-edge` documents at 16 MiB in the full scope (`run.py:156-199`: the `modes` list gets `"write"` for `size == 16`), only for the JSON adapters whose `write` is not null; the result cell has `"mode": "write"`, `"mib_s"` and `"bytes"`. `page.py`'s `ratios(runs)` (`page.py:366`) gets a parameter `mode="speed"` in place of its fixed `"speed"`, and `history_block` (`page.py:395`) gets, after the "Ahead" table and before the XML paragraph, one more `table("Print speed (MiB/s of output)", ...)` of the rows of `ratios(runs, "write")`, worst first, in the same columns. The gate does not count it (D-111: instructions of the read).
  * **Stays out**: writers for YAML, TOML, CBOR, INI and XML (a card each, written from what this finds); indented output; the sizes 1 and 256 MiB.
* **files**: `bench/compare/touch.h`; `bench/compare/main.cpp`; `bench/compare/CMakeLists.txt`; `bench/compare/adapters/yyjson.cpp`, `rapidjson.cpp`, `cjson.cpp`, `configforge.cpp`; `scripts/bench/run.py`; `scripts/bench/page.py`; `scripts/bench/test_page.py`; last: `docs/Performance.md`, `docs/Decisions.md`, `CHANGELOG.md`, `docs/Roadmap.md`, `docs/Codemap.md`.
* **steps**:
  1. `git switch -c work/P-11` from `main`.
  2. Write the test first: in `scripts/bench/test_page.py` a case that builds one full run as a dict (the shape of the file's `GOOD`, with `kind` `"full"` and what `headline` needs, `page.py:262`) holding `mixed` JSON `write` cells at 16 MiB for `configforge` and `yyjson`, and requires `page.history_block([run], "", <a temp directory>)` to contain the heading `#### Print speed (MiB/s of output)` and a row for `json`/`mixed`. See it fail: `python3 -m unittest scripts/bench/test_page.py` exits non-zero.
  3. Add the interface and `--mode write` to `touch.h` and `main.cpp`; build `cmake -S bench/compare -B build-cfb -DCMAKE_BUILD_TYPE=Release -DCF_BENCH_LIBS=$HOME/cfb-libs && cmake --build build-cfb --parallel` (a fresh `build-cfb` for a library change: P-word-at-a-time P-3's notes).
  4. The documents: `build-bench/bench/cf_bench --emit-docs $HOME/p11-docs --size 1 --only json` (`build-bench` as P-word-at-a-time P-3 step 3 builds it). The yyjson adapter first; check: `build-cfb/cfb_compare --lib yyjson --doc $HOME/p11-docs/json-mixed-1.json --mode write --reps 3` prints `"accepted":true` and a nonzero `mib_s`. Then RapidJSON, cJSON, ConfigForge, each with the same check.
  5. A check that the write is right, on the model of `bench.touch` (`check_touch`, `scripts/bench/run.py:299`; registered in `bench/compare/CMakeLists.txt:246-253`, where `foreach(check touch probes)` becomes `foreach(check touch probes write)`): `run.py --check write --compare <cfb_compare> --cfbench <cf_bench> --work <dir>` (a `check_write` beside `check_touch`, and `"write"` among the `--check` choices) runs, for every adapter of `json` whose `write` is not null, `--mode write --dump <dir>/<adapter>.json` on `json-mixed-1.json`, requires `"accepted":true`, and requires Python's `json.load` of the dump to `==` `json.load` of the input; it prints `bench.write: <adapter>: <what>` for a failure and `bench.write: ok` or `bench.write: failed` last. Written before the adapters' last step; see it fail when an adapter writes `[]`.
  6. `run.py`: the `write` mode and the documents; `page.py` the table. `python3 -m unittest scripts/bench/test_page.py` exit 0.
  7. `python3 scripts/bench/page.py --write docs/Performance.md` and commit the static block if it changed (`gate.perfpage`).
  8. Docs; D-167; format; codemap; apiref; ACCEPT.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS (the comparison builds on Linux only: `bench/compare` is not in the Windows preset; nothing changes there); ACCEPT-CI; `python3 -m unittest scripts/bench/test_page.py` exit 0 (CI runs it at `ci.yml:567`) and `ctest -R bench.write` exit 0 in the comparison build (`build-cfb`, which needs `-DCF_BENCH_CFBENCH=<cf_bench>` besides the libraries); the four adapters answer `--mode write` with `"accepted":true`.
* **stop if**: ConfigForge's JSON encoder cannot be driven from a recorded event list through the public `cf_codec.encoder` (e.g. it needs state the adapter cannot size: `encoder_state_size`, `cf_serialize.h:235`): stop with what it needs, and the library's line is left out of this card; the output of a library does not parse as the same JSON; three failures.
* **docs**: `CHANGELOG.md`, "## 1.9.0 — unreleased", "### Added": "`cfb_compare --mode write` and a print-speed table for JSON (ConfigForge, yyjson, RapidJSON, cJSON) on the benchmark page (D-167, P-11)." `docs/Decisions.md`: D-167. `docs/Performance.md`: the line "printing speed (a card for the writers)" at 386-388 says "printing speed for JSON (P-11); the other formats' writers are not yet"; the Roadmap's "The aim: beat the others at everything, and say where it does" stays 🟨.

---

## P-12 - The README leads with the lines ConfigForge wins, made by a script from the numbers

* **id**: P-12
* **goal**: a block of the README between `<!-- bench:lead:begin -->` and `<!-- bench:lead:end -->` is written by `scripts/bench/lead.py` from a results file, with one line for each format and measure, the wins first and the losses after, and `gate.readmelead` fails when the committed block differs from what the script writes from the committed `bench/lead.json`.
* **why**: "the README leads with the lines it wins, and the page shows all of them, losses included" (Roadmap, "The aim"); that step is "waiting for the first weekly run" in the Roadmap. A sentence written by hand drifts from the numbers; a block written from them cannot.
* **needs**: P-11 (for the print line; the card is written so that without P-11's `write` cells the print lines are absent, not wrong)
* **size**: medium
* **status**: open
* **version**: 1.9.0
* **decided**:
  * **D-168 (text to copy).** "D-168 — A lead line is computed, not written. For each format and each of the measures `parse` (the median MiB/s of `speed` cells, `mixed` at 16 MiB, GCC), `memory` (`peak_growth_kib`, `mixed` at 16 MiB) and, where P-11 made them, `print` (`write` cells), ConfigForge is compared with the best other library of that format. It leads when it is at least as fast, or no larger, than that library; a line says the ratio and the library's name, to one decimal. A line where it is behind says 'behind <library> by <ratio>' and is listed after the leads, in the same block. The block is made from `bench/lead.json`, a copy of the newest full results kept in the repository (a schema-1 results file with only the cells this script reads); updating it is a commit that says which run it is from. Rejected: reading the branch bench-history at build time (the README must be right without network)."
  * **`bench/lead.json`**: from the newest `bench-full-gcc` artifact of a run of `bench.yml` that succeeded (`gh run list --workflow bench --status success --limit 1 --json databaseId`; `gh run download <id> -n bench-full-gcc`), cut with `python3 scripts/bench/lead.py --cut bench-full-gcc.json > bench/lead.json` to the `speed`, `memory` and `write` cells of `mixed` at 16 MiB. When no run has succeeded yet, `bench/lead.json` is `{"schema": 1, "kind": "full", "cells": []}` and the block says exactly `No weekly run has finished yet; the lines appear here after the first.` Both cases are tested.
  * **`scripts/bench/lead.py`**: `--cut <results>` prints the cut file; `--write README.md` rewrites the block; `--check README.md` exits 1 with `lead: README.md differs from bench/lead.json` when it would change. Python 3 standard library only. A line's text, `<measure>` being `parse`, `memory` or `print`: `- **<FORMAT> <measure>:** <n.n>x <library>` (a lead), `- <FORMAT> <measure>: behind <library> by <n.n>x` (a loss); the ratio is ConfigForge's MiB/s over the best other's for `parse` and `print`, and the best other's peak growth over ConfigForge's for `memory` (smaller is better); the format names as `docs/Performance.md` writes them (JSON, YAML, TOML, CBOR, INI, XML). Wins sort by ratio, descending; losses by ratio, ascending. After the lines, one sentence: `All lines, with the machine and compiler: [the benchmark page](docs/Performance.md).`
  * **README markers.** README.md does not speak of speed anywhere (`grep -in "faster\|speed\|bench" README.md` finds only an options line). The block goes in a new section `## How fast` placed immediately before `## Design` (README.md:130), after `## Why`: the heading, the line `<!-- bench:lead:begin -->`, the generated lines, the line `<!-- bench:lead:end -->`.
  * **The gate.** `gate.readmelead`: a CMake test running `python3 scripts/bench/lead.py --check README.md` (copy `gate.perfpage`'s registration, `cmake/ConfigForgeGates.cmake:216-218`, and its script file `cmake/gates/GatePerfpage.cmake` as `GateReadmelead.cmake`), and `bench.lead` for the unit test.
* **files**: `scripts/bench/lead.py` (new); `scripts/bench/test_lead.py` (new); `bench/lead.json` (new); `README.md`; `scripts/README.md` (a row for `lead.py`); `cmake/ConfigForgeGates.cmake`; `cmake/gates/GateReadmelead.cmake` (new); `bench/CMakeLists.txt`; last: `docs/Decisions.md`, `CHANGELOG.md`, `docs/Roadmap.md`, `docs/Codemap.md`.
* **steps**:
  1. `git switch -c work/P-12` from `main`.
  2. Write `scripts/bench/test_lead.py`: a fixture with ConfigForge 1.2x the best other in JSON parse and 0.8x in YAML: the JSON line is `- **JSON parse:** 1.2x yyjson`, the YAML line is `- YAML parse: behind libyaml by 1.3x` (the ratio of the others' to ConfigForge's, one decimal), the JSON line comes first; an empty `cells` gives the "No weekly run" sentence; a README without the markers makes `--write` exit 2 with `lead: no markers in README.md`; `--check` exits 1 after a change to the fixture.
  3. See it fail: `python3 -m unittest scripts/bench/test_lead.py` non-zero.
  4. Write `lead.py`; the test exits 0.
  5. Add the markers and an empty block to README.md; make `bench/lead.json` as decided (the empty form when no run has succeeded); `python3 scripts/bench/lead.py --write README.md`; `--check` exit 0.
  6. Register `gate.readmelead` and `bench.lead`. See `gate.readmelead` fail: change one character inside the block and run `ctest -R gate.readmelead` (non-zero); put it back.
  7. Docs; D-168; format; codemap; apiref; ACCEPT.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest -R "gate.readmelead|bench.lead"` exit 0. The block is filled with real numbers by whoever commits `bench/lead.json` from the first successful run; that is a one-line commit, not a card.
* **stop if**: `README.md` has a block with these markers already; the `bench-full-gcc` artifact lacks `peak_growth_kib` or `mib_s` on `mixed` at 16 MiB (`run.py` runs `memory` at 16 MiB only, `run.py:156-199`): stop and write which cell is missing.
* **docs**: `CHANGELOG.md`, "## 1.9.0 — unreleased", "### Added": "The README's speed lines are written by `scripts/bench/lead.py` from the weekly benchmark's numbers, wins first and losses after (D-168, P-12)." `docs/Decisions.md`: D-168. `docs/Roadmap.md`: "The aim: beat the others at everything, and say where it does" gets 🟩 only when `bench/lead.json` holds a real run and P-11 is done; otherwise it stays 🟨.

---

## P-13 - SSE2 and NEON: the measured case, in numbers and a recommendation

* **id**: P-13
* **goal**: `cf_bench lanes-simd` runs hand-written SSE2 (x86-64) and NEON (AArch64) forms of `find_byte`, `find_lt`, `ascii_run` and `count_byte` against `swar64` and the scalar loop on the spans of P-2's table, and `docs/Performance.md` holds the table and a recommendation, ending in the `CF_WITH_SIMD` decision as written below.
* **why**: the Roadmap puts "a measured case for SSE2 and NEON backends behind the same `lanes_ops` table (both are the baseline of their targets, so no detection is needed there)" after P-3, and says the `CF_WITH_SIMD` decision is the maintainer's (taken under the standing approval, 2026-11-27: a build option, OFF by default). A decision needs numbers; SSE2 is in every x86-64 and NEON in every AArch64, so these two need no run-time detection, which makes them the cheapest to carry.
* **needs**: P-3 (the readers' own scans decide which operations matter; if P-3 closed with no kept change, this card still runs, on the operations' own table)
* **size**: large
* **status**: open
* **version**: 2.4.0
* **decided**:
  * **D-169 (text to copy).** "D-169 — A SIMD backend is measured outside the library first. The SSE2 and NEON forms of lanes' operations are written in `bench/lanes_simd.c` only, never in `lib/lanes/`, until a card wires them behind `CF_WITH_SIMD`, a build option OFF by default; they are measured with `cf_bench lanes-simd` against `swar64` and scalar. A backend is recommended only where it is at least 1.5 times faster than `swar64` on spans of 64 bytes and longer, for every kind of input measured, and no slower than `swar64` by more than 5% on 16 bytes; and the readers' own cells must then move (P-3's rule) once it sits under them, which is the card after the decision. Rejected: AVX2 and wider in this card (they need run-time detection, D-101: one more decision)."
  * **Where.** A new `bench/lanes_simd.c` (C99; its own guards `#if defined(__SSE2__) || defined(_M_X64)` and `#if defined(__aarch64__)`; an empty translation unit elsewhere), added to the sources of `cf_bench` (`bench/CMakeLists.txt:19`), and `run_lanes_simd(int argc, char** argv)` called from `bench.c`'s main beside `lanes` (`bench/bench.c:2399-2401`). The `lanes_ops` struct is not touched: the SIMD functions are static functions of the same signatures as `lanes_ops`' members (`lib/lanes/include/lanes/lanes.h:109-115`), put in a `lanes_ops` of the bench's own (`static const lanes_ops simd_ops`, `size` = `sizeof(lanes_ops)`, `name` `"sse2"` or `"neon"`, `isa` `LANES_ISA_SSE2` or `LANES_ISA_NEON`, the other members `lanes_scalar()`'s).
  * **The four operations.** `find_byte(p, n, c)`: 16 bytes a step with `_mm_cmpeq_epi8` and `_mm_movemask_epi8` (NEON: `vceqq_u8`, then `vshrn_n_u16(..., 4)` and `vget_lane_u64` for the mask); on a nonzero mask the index is `__builtin_ctz(mask)` (`_BitScanForward` under MSVC; the byte order never matters, the mask's bit order is fixed); a tail under 16 bytes by the byte loop, never a read past `p + n`. `find_lt(p, n, c)`: unsigned compare below `c` as `_mm_cmpeq_epi8(_mm_min_epu8(v, bound), v)` with `bound` = `c - 1` (and `c == 0` returns `n`); NEON `vcltq_u8`. `ascii_run(p, n)`: `_mm_movemask_epi8(v)` is the high bits directly (NEON: `vmaxvq_u8` of the high-bit test for the any-test, then the byte loop in the block). `count_byte(p, n, c)`: `_mm_sub_epi8(acc, _mm_cmpeq_epi8(v, vc))` into 8-bit counters flushed every 255 steps with `_mm_sad_epu8` (NEON `vsubq_u8` of the compare mask and `vaddlvq_u8`).
  * **Equality tests first.** `bench/lanes_simd.c` carries a self-check run by `cf_bench lanes-simd --check`: every operation over every length 0 to 70 and offset 0 to 15 of a buffer with poison bytes (0xA5) on both sides, against `lanes_scalar()`'s result, for 64 byte values from a linear-congruential generator (`x = x * 1103515245UL + 12345UL`, seed 1, the byte `x >> 16 & 0xFF`) and every one of `c` in `{0, 1, 0x20, 0x7F, 0x80, 0xFF}`; any difference prints `lanes-simd: <op> n=<n> off=<o> c=<c>: scalar <a>, sse2 <b>` and exits 1. A poison byte read is detected as `valgrind`'s invalid read in step 6, not by the check.
  * **Table.** The same rows, spans and ratio rule as P-2's table (`bench/lanes.c:1-18`; `docs/Performance.md:296-328`), printed as `cf_bench lanes-simd`, with a column for the ratio against `swar64` and one against scalar. GCC and Clang, `-O2`, x86-64, written into `docs/Performance.md` as "### SSE2 and NEON against the word backend (P-13)". NEON is measured on an AArch64 machine only; where none is reachable (no Arm CI job exists, `grep -n "arm\|aarch64" .github/workflows/ci.yml` is empty), the table's NEON rows say `not measured: no Arm hardware`, the NEON code is compiled with `aarch64-linux-gnu-gcc -O2 -c bench/lanes_simd.c` when that compiler is installed (a compile check, not a measure) and the check run under `qemu-aarch64` when that is installed (a correctness check, never a speed).
  * **The recommendation** is written under the table, by D-169's rule, per operation and per backend, and ends in this paragraph, which the card writes into `docs/Performance.md` as it stands here (the decision is taken; the card wires nothing):
    * **Decided: the shape of `CF_WITH_SIMD` is (b)** (the maintainer's standing approval, 2026-11-27). A new build option `CF_WITH_SIMD` (ON/OFF) beside `CF_LANES_BACKENDS`, default OFF, so a build without the flag is today's build; the `misra` and `minimal` profiles never build it, and `misra` stays forced to scalar (`cmake/profiles/misra.cmake:28`); a backend goes behind it only where D-169's rule holds for its numbers on this page. Rejected: (a) `CF_LANES_BACKENDS=all` (`lib/lanes/lanes_sources.cmake:9-13`) gaining `sse2` and `neon` sources, with `lanes_best()` (`lib/lanes/src/lanes.c:185-189`) returning them before SWAR (it changes what an existing `all` build runs: an existing default changed); (c) no SIMD at all (decided before the numbers; if the table shows no gain, the option stays OFF with nothing behind it). The card decides everything around it; nothing is wired in `lib/lanes/` by this card.
* **files**: `bench/lanes_simd.c` (new); `bench/CMakeLists.txt`; `bench/bench.c` (one call); `docs/Performance.md`; last: `docs/Decisions.md`, `CHANGELOG.md`, `docs/Roadmap.md`, `docs/Codemap.md`.
* **steps**:
  1. `git switch -c work/P-13` from `main`.
  2. Write the self-check and the table driver in `bench/lanes_simd.c` with the four operations as stubs that call `lanes_scalar()`'s; `cf_bench lanes-simd --check` exit 0.
  3. See it fail: make the SSE2 `find_byte` return `n` always, and see `--check` exit 1 with the message above; then write the real bodies.
  4. `cmake -S . -B build-bench -DCMAKE_BUILD_TYPE=Release -DCF_BUILD_BENCH=ON && cmake --build build-bench --target cf_bench && build-bench/bench/cf_bench lanes-simd --check` exit 0 with GCC, and with `-DCMAKE_C_COMPILER=clang`.
  5. `build-bench/bench/cf_bench lanes-simd` best of fifteen runs, GCC and Clang, copied into the table.
  6. `valgrind --error-exitcode=9 build-bench/bench/cf_bench lanes-simd --check` exit 0 (no invalid read).
  7. The recommendation under the table; the decision as written.
  8. Docs; D-169; format; codemap; apiref; ACCEPT.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS (MSVC defines `_M_X64`, not `__SSE2__`, which is why the guard has both; under MSVC `find_byte` uses `_BitScanForward`; the check passes there too); ACCEPT-CI; `cf_bench lanes-simd --check` exit 0.
* **stop if**: `--check` finds a difference the card cannot trace to the new code in three tries; the table shows the SSE2 forms slower than `swar64` on every long span (then it is the finding: write it, say that `CF_WITH_SIMD` gets no backend behind it, and stop).
* **docs**: `CHANGELOG.md`, "## 2.4.0 — unreleased", "### Added": "`cf_bench lanes-simd` measures SSE2 and NEON forms of lanes' operations against the word backend; the numbers and a recommendation are in docs/Performance.md (D-169, P-13)." `docs/Decisions.md`: D-169. `docs/Roadmap.md`: "A SWAR and SIMD backend, in C89/C99, possibly a library of its own" stays 🟨.
* **Organiser's answers**:
  * `CF_WITH_SIMD` is option (b): a build option of its own, OFF by default, never built by the `misra` and `minimal` profiles; (a) and (c) rejected as the "decided" bullet says (the maintainer's standing approval, 2026-11-27).

---

## P-14 - The OpenSSF Scorecard: every finding read, every one the repository can meet met

* **id**: P-14
* **goal**: `docs/Scorecard.md` lists every check of the project's published Scorecard result with its score and, for each, either the file that meets it, a change made by this card, or what it waits on (the maintainer's settings: P-15-m), and `scripts/work/scorecard.py` prints the same table from the live result.
* **why**: "its findings ... are read one by one, the ones the project can meet are met, and the rest are written down with why" (Roadmap, "The OpenSSF Scorecard score raised", low priority). `scorecard.yml` publishes the result (L-2); nobody has read it check by check.
* **needs**: nothing
* **size**: medium
* **status**: open
* **version**: 1.9.0
* **decided**:
  * **D-170 (text to copy).** "D-170 — The Scorecard is read from its API. `scripts/work/scorecard.py` fetches `https://api.scorecard.dev/projects/github.com/develooper1994/configforge` (public, no token) and prints one line a check: name, score, the first line of its reason. `docs/Scorecard.md` records, for each check, one of `met` (with the file), `changed here` (with the commit), `waits on the maintainer` (with the setting, P-15-m) or `not applicable` (with why). A check that the repository can meet by a file is met by a card; one that needs a setting on GitHub or an account elsewhere is the maintainer's and never a step in an agent's card."
  * **What the repository can meet by files, and the worker's reading rules** (read, never assumed; the worker fetches the result first and reads the `reason` and `details` of each check):
    * `Token-Permissions`: every workflow has a top-level `permissions:` of `contents: read` and each job asks for more only where it needs it. `grep -n "^permissions" .github/workflows/*.yml` must list all nine files (all nine do today: ci.yml:29, bench.yml:31, scorecard.yml:11 and the six others); a workflow without one gets `permissions:\n  contents: read` after `on:` and the job lines that need more get theirs.
    * `Pinned-Dependencies`: every `uses:` is a 40-hex commit with a version comment (all are, except `uses: ./actions/doctor` at ci.yml:490 and 495, which is local and exempt); every `pip install`, `npm install`, `cargo install` and `apt-get install` in a workflow or a Dockerfile that Scorecard names in its `details` is pinned where the tool allows (a hash for pip: `--require-hashes`; a version for cargo: `--locked --version <x>`); the card changes only lines the details name.
    * `Security-Policy`: `SECURITY.md` exists; the card checks that it names a contact and a disclosure period.
    * `Dependency-Update-Tool`: `.github/dependabot.yml` exists and covers `github-actions`, `cargo` and `docker` today; no requirements file exists for `pip`, so the card adds no ecosystem unless the Scorecard's details name one and its manifest exists.
    * `SAST`, `Fuzzing`, `Vulnerabilities`, `License`, `Binary-Artifacts`, `Dangerous-Workflow`, `CI-Tests`, `Packaging`, `Signed-Releases`, `Contributors`: read the reason; where it is below 10 and a file can raise it (SAST: `codeql.yml` must run on pull requests, `on: pull_request` in it; Signed-Releases: `release.yml` must attach a signature or provenance to each release asset; Packaging: the release workflow publishes to a registry or GitHub Packages), do it, one commit each; where the reason is about history (Contributors, CI-Tests on old commits) say `not applicable` and why.
    * **Not met by a file**: `Branch-Protection`, `Code-Review`, `CII-Best-Practices`, `Maintained`: P-15-m. The card lists them with their scores and does not touch them.
  * **`scripts/work/scorecard.py`**: Python 3 standard library only, `urllib.request`, timeout 30 s; `--file <json>` reads a saved result instead (for the test). Prints `<score>/10  <name>  <reason's first line>` sorted by score ascending; the total on the last line `overall: <score>`. Exit 0; exit 1 with `scorecard: <reason>` when the API does not answer (no result is not a failure of the repository).
  * **The document** `docs/Scorecard.md`: a header with the date and the commit of the result (the `date` and `repo.commit` fields of the JSON), the table `| Check | Score | Status | Where |`, and a closing section "What the maintainer sets" naming the four checks above and the settings P-15-m lists.
* **files**: `scripts/work/scorecard.py` (new); `scripts/work/test_scorecard.py` (new, with its fixture in the test as a dict); `cmake/ConfigForgeGates.cmake` (a test `gate.scorecard` after `gate.perfpage`, lines 216-218, running `python3 -m unittest scripts/work/test_scorecard.py` from `${CMAKE_SOURCE_DIR}`, guarded by `find_package(Python3 COMPONENTS Interpreter QUIET)` as `bench/CMakeLists.txt:38-48` guards `bench.gate`); `scripts/README.md` (a row for `scorecard.py`); `docs/Scorecard.md` (new); the workflow, `dependabot.yml` and `SECURITY.md` files the reading changes; last: `docs/Decisions.md`, `CHANGELOG.md`, `docs/Roadmap.md`, `docs/Codemap.md`.
* **steps**:
  1. `git switch -c work/P-14` from `main`.
  2. Write `scripts/work/test_scorecard.py` (a result dict of three checks; the printed order is by score ascending; `overall:` is last; a missing `--file` exits 1 with the message). See it fail (`python3 -m unittest scripts/work/test_scorecard.py` non-zero); write the script; exit 0; add `gate.scorecard` to `cmake/ConfigForgeGates.cmake`.
  3. `python3 scripts/work/scorecard.py > $HOME/p14.txt`; if the API does not answer (exit 1): stop (a result appears after `scorecard.yml`'s first run on `main`; the Roadmap says the badge shows then).
  4. For each check below 10, read its reason and apply the rules above: one commit per file changed, the commit message names the check.
  5. Write `docs/Scorecard.md` from the output and the changes.
  6. `actionlint .github/workflows/*.yml` exit 0 where installed (CI's `gate.workflows` otherwise).
  7. Docs; D-170; format; codemap; apiref; ACCEPT.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest -R gate.scorecard` exit 0; every workflow still passes CI after the changes (the push's own run). The score itself changes at the next weekly Scorecard run: it is not an acceptance condition.
* **stop if**: the API has no result for the repository; a change to a workflow makes any CI job fail twice (revert that file's change and list the check as `changed here: reverted, <why>`).
* **docs**: `CHANGELOG.md`, "## 1.9.0 — unreleased", "### Added": "`docs/Scorecard.md` reads the OpenSSF Scorecard check by check, and `scripts/work/scorecard.py` prints it (D-170, P-14)." (and, if a file changed: "### Changed": which). `docs/Decisions.md`: D-170. `docs/Roadmap.md`: "The OpenSSF Scorecard score raised" gets 🟨 (🟩 when the maintainer's checks are met: P-15-m).

---

## P-15-m - The repository settings the Scorecard waits on (the maintainer's)

* **id**: P-15-m
* **goal**: the four Scorecard checks that no file can raise are met by the settings below.
* **why**: Scorecard's `Branch-Protection`, `Code-Review`, `CII-Best-Practices` and `Maintained` read GitHub's settings, an account, and the project's history. docs/Plan.md, "Waits on the maintainer", names these (repository settings: a ruleset on `main`).
* **needs**: P-14 (its table names the scores)
* **size**: small (the maintainer's, by hand; no agent card)
* **status**: parked: not now
* **version**: 1.9.0
* **decided**:
  * **Parked: the loop skips it** (docs/Plan.md, "Waits on the maintainer"). P-14 stands alone.
  * **The settings**, from the Roadmap's "Repository settings" and Scorecard's checks: (1) a ruleset on `main`: require a pull request with one approving review, require the checks of `ci.yml` to pass (`gates`, `portable`, `matrix`, `shared`, `components`, `bench-gate`: the job names are `grep -n "^  [a-z-]*:$" .github/workflows/ci.yml`), require signed commits, no force pushes, no deletions; (2) two-factor authentication on the account; (3) the OpenSSF Best Practices badge: an account at bestpractices.dev, the project's form filled to the passing level, the badge added to README.md; (4) `Maintained` rises with age and activity and is not a setting. Pushing documents straight to `main` (CLAUDE.md, "Habits") needs the ruleset to list the maintainer as a bypass actor.
  * **Whether to do it** is the maintainer's: the loop's cards rely on direct pushes to `main` for documents and on the maintainer's bypass for code that passed CI (CLAUDE.md "Habits"). A required review by a second person is not possible on a one-person project: the ruleset can require the pull request and the checks without a review count, which raises `Branch-Protection` but leaves `Code-Review` low.
* **files**: none in the repository except `README.md` (the badge) and `docs/Scorecard.md` (the status column).
* **steps**: the maintainer's, in the repository's settings and at bestpractices.dev; then `python3 scripts/work/scorecard.py` and the status column of `docs/Scorecard.md` updated by hand.
* **accept**: the Scorecard's next weekly run shows the check raised.
* **stop if**: not applicable (a setting).
* **docs**: `docs/Scorecard.md`; `docs/Roadmap.md`: "The OpenSSF Scorecard score raised" gets 🟩; the Roadmap's "Trust that can be checked from outside": the Best Practices badge in README.md.

---

## P-16 - Eight digits at a time: a word body for `digits8`, measured in lanes

* **id**: P-16
* **goal**: `lanes_swar()`'s `digits8` has a word body on 64-bit words, equal to the scalar form for every input, and `cf_bench lanes digits8` prints its ratio against the scalar loop; no reader calls it.
* **why**: `digits8` is an operation of the table that nothing calls, and in the word backend it is the scalar function (`lib/lanes/src/swar.c:163-165,429`). simdjson reads eight digits with SWAR (`parse_eight_digits_unrolled`); the Roadmap's "eight digits to a number" is one of lanes' operations. This card gives it a body and a number; the readers use it when 2.4.0 lets them choose a backend (D-101: the number readers call `lanes_scalar()` until then, `docs/plan/A-architecture.md:149-156`, so changing `cf_text_to_u64`, `lib/numeric/src/numeric.c:28-53`, is not this card and is not decided here).
* **needs**: P-10 (its row 7 says whether to take it)
* **size**: medium
* **status**: open
* **version**: 2.4.0
* **decided**:
  * **The body**, on a 64-bit `unsigned long` only (`LANES_ISA_SWAR64`); on a 32-bit one the entry stays `swar_fallback_digits8`. It is built only where `unsigned long` is 64 bits, by the file's own test (`((ULONG_MAX >> 31) >> 1) == 0xFFFFFFFFUL`, `swar.c:48`), which also sets `SWAR_DIGITS8_BUILT` to 1 (else 0, and the body is not compiled: its 64-bit literals would not be valid). It reads the eight bytes with `memcpy` into an `unsigned long` `w` (as the other word bodies read, `swar.c`'s word-read helper near line 102), after which: `t = w - 0x3030303030303030UL`; the bytes are digits when `((t + 0x7676767676767676UL) | t) & 0x8080808080808080UL` is 0 (each byte of `t` is 0 to 9: adding 0x76 carries into the high bit exactly from 10, and a byte below `'0'` borrows into a high bit of `t`); on nonzero the function returns 0 and leaves `*value` alone (`lanes.h:25-30`). The body is for a little-endian machine, and `lanes` has no byte-order macro: the body begins with `if (!swar_little()) { return lanes_scalar()->digits8(p, value); }`, `swar_little` being `static int swar_little(void)` that stores `1UL` in an `unsigned long`, copies its first byte with `memcpy` into an `unsigned char` and returns whether it is 1 (decided: no byte swap is written, a big-endian machine keeps the scalar's answer). The value is the classic: `t = (t * 10) + (t >> 8)` masked as `t = ((t & 0x0F0F0F0F0F0F0F0FUL) * 2561UL) >> 8` (pairs), `t = ((t & 0x00FF00FF00FF00FFUL) * 6553601UL) >> 16`, `t = ((t & 0x0000FFFF0000FFFFUL) * 42949672960001UL) >> 32`; the tests of steps 2 and 4 prove these three constants before they are trusted.
  * **Kept by P-2's rule** (`bench/lanes.c:9-13`, `docs/Performance.md:296-306`), applied to each of the two cases: at least 5% faster than the loop on the spans of 64 bytes and longer, and no more than 5% slower on 16; otherwise the entry stays the scalar function and the body stays in the file under `LANES_SWAR_KEEP_DIGITS8 0` (the pattern of `LANES_SWAR_KEEP_COUNT_BYTE`: the define at `swar.c:72`, the fallback function under `#if !...` at 155-161, the `SWAR_ENTRY_` macro at 396-400), with its table row.
  * **Tests.** `test_digits8` (`lib/lanes/tests/test_lanes.c:406-431+`, called at 890 for every `ops` the file runs) already checks five edge values and, for each of the eight positions, eight bad bytes (`'/'`, `':'`, 0x00, 0x80, 0xB0, ...), requiring the refusal and an untouched value. The card adds to it one loop of 1,000 values from a linear-congruential generator (`x = x * 1103515245UL + 12345UL`, seed 1, value `(x >> 8) % 100000000UL`), each written as eight ASCII digits, requiring `ops->digits8` to return 1 and the value, and to equal `lanes_scalar()->digits8` in both. The test is built with `LANES_SWAR_KEEP_ALL`, so the word body is tested even when the benchmark reverts it.
  * **Bench.** `bench/lanes.c` gets the kind `K_DIGITS8` (the enum at line 45, `call()` at 47-69; the name `digits8` is also added to the list of operations in the header comment, lines 6-7) whose `call(ops, k, n)` makes `n / 8` calls of `ops->digits8` at `k->data + 8 * i` and returns the sum of the values, so the existing spans (8, 16, 64, 256, 4096) apply unchanged and the figures are per byte of text. Two cases beside `count_byte`'s (`lanes.c:245-250`): `op` `"digits8"`, `what` `"all digits"` (a 4096-byte buffer of ASCII digits) and `"a non-digit last"` (the same with byte 7 of every group of eight replaced by `'x'`); `cf_bench lanes digits8` selects them (`lanes.c:277`).
* **files**: `lib/lanes/src/swar.c`; `lib/lanes/tests/test_lanes.c`; `bench/lanes.c`; last: `docs/Performance.md`, `CHANGELOG.md`, `docs/Roadmap.md`, `docs/Codemap.md`.
* **steps**:
  1. `git switch -c work/P-16` from `main`.
  2. Write the test of `digits8` equality (against scalar, all positions, the generator's values) in `test_lanes.c`; it passes on the unchanged tree (the fallback is the scalar function).
  3. Write the word body behind `LANES_SWAR_KEEP_DIGITS8 1` and a name `swar_digits8`: the define after `swar.c:72`; the `#undef` and `#define ... 1` pair in the `LANES_SWAR_KEEP_ALL` block (76-89); `swar_fallback_digits8` (163-165) under `#if !(LANES_SWAR_KEEP_DIGITS8 && SWAR_DIGITS8_BUILT)`; a `SWAR_ENTRY_DIGITS8` after line 400 chosen by the same condition; `(void)swar_digits8;` beside line 421; the table entry at 429 uses `SWAR_ENTRY_DIGITS8`.
  4. See it fail: change the constant `6553601UL` to `6553602UL`, run `ctest -R test_lanes` (non-zero), put it back; `ctest -R lanes` exit 0 in the default, `-DCF_LANES_BACKENDS=scalar` and `-DCF_LANES_BACKENDS=c89` builds.
  5. Add `K_DIGITS8` and its cases to `bench/lanes.c`; `cmake --build build-bench --target cf_bench && build-bench/bench/cf_bench lanes digits8` (best of fifteen, GCC and Clang).
  6. Apply the rule: keep, or set `LANES_SWAR_KEEP_DIGITS8 0`. Either way the table row goes into `docs/Performance.md` after P-2's table.
  7. `valgrind --error-exitcode=9 build-bench/bench/cf_bench lanes digits8` exit 0 (no read past the eighth byte).
  8. Docs; format; codemap; apiref; ACCEPT.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest -R lanes` exit 0 in the three backend builds.
* **stop if**: the word body differs from scalar in a way the three constants cannot explain after three tries (the scalar form stays, and the card closes with the row `digits8: scalar kept`); a target where the body cannot be built (`SWAR_DIGITS8_BUILT` is 0, as LLP64 Windows, where `unsigned long` has 32 bits) is not a failure: the scalar function stays there, and the test and the bench still run.
* **docs**: `CHANGELOG.md`, "## 2.4.0 — unreleased", "### Added": "lanes' word backend reads eight digits at a time on 64-bit words (kept: <the ratio>; or: measured and not kept) (P-16)." A row in `docs/Performance.md`. `docs/Roadmap.md`: "A SWAR and SIMD backend, in C89/C99, possibly a library of its own" stays 🟨.
