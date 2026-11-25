# Word at a time before SIMD

**Superseded in part, 2026-11-19.** The word-at-a-time and SIMD code is
now a library of its own, `lanes` (docs/plan/A-architecture.md, D-101),
whose interface and scalar reference come in 1.8.0 (A-2). P-2 and P-3
are rewritten on it before they open (2.4.0): P-2's primitives become
the library's `swar32` and `swar64` backends rather than
`lib/serialize/src/swar.h`, and P-3's scans are the few functions of
`lib/text/` (D-102). P-1 and the rules below stand.

The Roadmap's "Large files, and SIMD as an option" orders the work:
measure, then a portable word-at-a-time (SWAR) path in C89 for every
target, then SIMD behind `CF_WITH_SIMD`. These three cards are the first
two steps; SIMD is a card of its own, written after P-3 says what SWAR
left on the table. Each card is the loop's unit (`docs/Plan.md`, "The
loop"). **They come after everything else in the plan**: optimization
starts only when every other card is done (the maintainer, 2026-11-17),
so it measures the code as it will stay. **P-1 alone may come
earlier**: it changes only `bench/`, and its instruction-count baseline
then shows what the MISRA work (`docs/Compliance.md`) costs or saves as
it rewrites the sources. P-2 and after wait for that work to end, or
the tuned loops are written twice. A scan whose cost grows faster than
its input is not optimization but a fault, and is fixed when found.

**In waves, each opened by a profile.** Optimization goes in waves,
and every wave starts by profiling again, since the last one moved
where the time is:

1. **Profile every part** (P-1): time per function (`perf` on Linux,
   the Visual Studio profiler or WPA on Windows, `callgrind` for exact
   counts), memory and allocations (`massif` or `heaptrack`), and cache
   misses (`perf stat`), per format and per stage - reading the bytes,
   the lexer, the parser's events, the flattener, the key space, the
   report. What each part does inside is written down before anything
   changes.
2. **Make the code optimizable** (P-3's first step, above).
3. **The compiler, the C library and build options** (P-3's forms a
   and b; LTO and PGO).
4. **Algorithm**: tables, fewer copies and allocations.
5. **Word at a time** (P-2, P-3's form c).
6. **SIMD**, behind `CF_WITH_SIMD`.
7. **Threads**, for documents that can be split.

A wave that finds nothing worth keeping ends with its profile written
down, and the next wave starts from it. Cards for waves 4, 6 and 7 are
written when the wave before them ends.

The rules every card here keeps:

* **The scalar loop stays, and is the reference.** A SWAR routine is
  never the only implementation of a scan: the byte loop it replaces is
  kept beside it, as lanes' scalar table, and the tests run both. The
  MISRA profile builds with `CF_LANES_BACKENDS=scalar`: the `memcpy` word read is
  a finding there (`docs/Compliance.md`, "Where the standards
  disagree"), and a second path is a second thing to show right.
* **C89 only.** Words are `unsigned long` (at least 32 bits; 64 where it
  is), read with `memcpy` from a byte pointer, never through a cast
  pointer: no alignment or strict-aliasing assumption, no
  `__builtin_*`, no `uint64_t`. Endianness never matters: a routine
  finds whether a word holds a byte of interest, then the byte loop
  finds which.
* **No byte past the end is read.** The tail shorter than a word is
  done by the byte loop; a word read never crosses `end`.
* **Nothing is changed without a number.** P-3 touches only the scans
  P-1 shows to cost, and keeps a change only if the benchmark moves.
* **A slow result is not a verdict until the code could be fast.** A
  scan written as a chain of dependent branches cannot be vectorized
  by the compiler, by SWAR or by SIMD, and measuring it that way only
  shows its shape. Before a form is judged, the loop is restated so an
  optimizer can work on it: data and control separated (classify bytes
  first, act on the classes after), no early exit buried in the body,
  the hot state in locals, and the input read as a contiguous span.
  That restatement is a change of its own, measured on its own, and
  kept if it does not slow the scalar build.
* **The compiler and the C library come first.** The baseline is the
  byte loop as the release build compiles it (`-O2`, the generic
  target, as the packages ship), and a scan is first tried in two
  cheaper forms: written so the compiler can vectorize it (checked with
  `-fopt-info-vec` / `-Rpass=loop-vectorize`, not assumed: early-exit
  searches mostly are not), and as a call to the C library's own search
  (`memchr`, `strcspn`), which glibc, musl and the MSVC CRT already
  vectorize. SWAR is kept only where it beats the best of those by the
  margin P-3 sets; SIMD later is held to the same rule against SWAR.
  `-O3` and `-march=native` are measured for the record, never relied
  on: a shipped library cannot assume them.

---

## P-3 - The readers' scans on lanes' four kept operations, kept only where they win

* **id**: P-3
* **goal**: the readers' hottest scans (JSON and TOML strings, YAML's quoted and plain scalars and its key look-ahead, the UTF-8 check, the line end) take their runs of ordinary bytes through the operations P-2 kept a word at a time (`find_byte`, `find_lt`, `ascii_run`), and each change stays only where the readers' own benchmark cell shows it paying, with GCC and with Clang.
* **why**: P-2 (e0c0904) measured lanes' word backend alone: `find_byte`, `find_lt`, `ascii_run` and `count_byte` won by 1.4 to 7 times (`docs/Performance.md:296-328`), but no reader calls lanes yet: the one caller, `cf_text_utf8_valid`, takes the scalar table (`lib/text/src/utf8.c:58-60`). P-1's profile puts four fifths of JSON and TOML in the string scan and three fifths of YAML in the quoted scan and the key test (`docs/Performance.md:237-247`). The Roadmap's "The vector library carried further" (`docs/Roadmap.md:1514-1523`) puts this step before SSE2 and NEON, and D-102 (`docs/plan/A-architecture.md:207-209`) says P-3's scans call lanes.
* **needs**: P-2 (landed, e0c0904)
* **size**: large (eight scans, one commit each)
* **decided**:
  * **The table.** A reader takes `const lanes_ops* ops = lanes_best();` once per document and keeps it in its scan state (`js_scan`, `lib/serialize/src/json.c:41-51`; `toml_scan`, `toml.c:115-142`; `yp`, `yamlparse.c:43-58`), set where the state is initialised. A `lib/text/` function, which has no state, calls `lanes_best()` on each call: two calls and no detection (`lib/lanes/src/lanes.c:185-189`). Never a cached global (D-101: no global mutable state). With `CF_LANES_BACKENDS=scalar` (the `misra` profile, `cmake/profiles/misra.cmake:28`) `lanes_best()` returns the scalar table and the readers compute the same; that build is checked for results, not speed.
  * **A run, then the old loop for one byte.** Each scan keeps its byte loop as it is. Before each turn of the loop it measures the run of bytes that need nothing but copying (or skipping), takes the run whole (one capacity check, one `memcpy` when there is a destination), and the unchanged loop body then handles the byte that stopped the run. A byte that stops a run only to be taken by the loop as ordinary (a tab, a `#` not after a blank, a backslash in a TOML literal string) is correct, only slower; the bench says whether it costs.
  * **Each stop kind is searched once, never again over the same bytes.** A run ends at the nearest of its stop kinds, each found by one operation: `find_byte(p, n, c)` for one byte, `find_lt(p, n, c)` for every byte below `c`, `ascii_run(p, n)` for the first byte from 0x80. Each kind's result is kept in a local, `const char* next_<kind>`, and searched again only once `p` has passed it (`p > next_<kind>`); the outermost kind is searched to the scan's end, the others to the outermost's result. Searching every kind afresh at each turn re-reads the same bytes after each stop and is quadratic on a long line with many stops (a minified JSON of one line, `a:a:a:...` in YAML): a fault (this file's preamble), not a choice.
  * **The candidates, in P-1's order (largest share first), one commit each.** Lines are those of 70ad1c3.
    1. **`lib/serialize/src/json.c:193-247` `js_string`**, JSON only (`!s->json5`). Stops: the quote (outermost), `'\\'`, `find_lt(0x20)` (a control byte and `'\n'`). The run is copied with one check, `if (m >= cap - n) { js_too_long(s); return (size_t)-1; }` (the per-byte check at 225-228, for `m` bytes), then `memcpy(dst + n, s->p, m)` when `dst != NULL`. JSON5 stays on the loop: its stops are four single bytes and no control class, which is `find_any`'s. Must improve: `json/strings-plain/4`. Must not regress: `json/strings-escape/4`, `json/mixed/4`, `json/whitespace/4`, `json/numbers-edge/4`.
    2. **`lib/serialize/src/toml.c:247-405` `toml_string`**, the single-line forms only (`!ml`), basic and literal. Stops: the quote (outermost), `'\\'` (basic only), `find_lt(0x20)`, `find_byte(0x7F)`, `ascii_run` (UTF-8 goes through the loop's `cf_text_utf8_decode`, 338-354). The run is copied with `toml_put` (218-228), which checks the capacity once for `k` bytes. Multi-line strings stay on the loop (line breaks, line continuations, quote runs). Must improve: `toml/strings-plain/4`. Must not regress: `toml/strings-escape/4`, `toml/mixed/4`, `toml/whitespace/4`, `toml/numbers-edge/4`.
    3. **`lib/serialize/src/yamlparse.c:684-746` `yp_quoted`** (its loop; the function ends at 758). Stops: the quote (outermost), `'\\'` (double-quoted only), `find_lt(0x21)` (space, tab, line breaks and controls: the loop's blank, break and printable branches), `find_byte(0x7F)`. Bytes from 0x80 are printable to `yp_printable` (122-124) and so are run bytes. The run is copied with one check, `if (k + m >= y->cap) return yp_fail(y, CF_REASON_BUDGET, "a value is longer than the document");` (`yp_put`'s, 436-442, for `m` bytes), then `memcpy(y->buf + k, y->p, m)`. Must improve: `yaml/strings-plain/4` (its strings are double-quoted: `bench/bench.c:1448-1455`). Must not regress: `yaml/strings-escape/4`, `yaml/mixed/4`, `yaml/whitespace/4`, `yaml/numbers-edge/4`.
    4. **`lib/serialize/src/yamlparse.c:972-990` `yp_peek_node`, the quoted branch** (the look-ahead of `yp_is_key`, 1036-1058, which runs at every node). Stops: the quote (outermost), `'\\'` (double only), `find_lt(0x20)` (the line breaks; a tab stops harmlessly). Nothing is copied: the run is skipped. The same cells as 3.
    5. **`lib/serialize/src/yamlparse.c:1014-1026` `yp_peek_node`, the plain branch, block context only** (`y->flow == 0`). Stops: `find_lt(0x20)` (outermost: the line end), `':'`, `'#'`. Skipped, not copied; the trim of trailing blanks at 1027-1029 is unchanged. Flow context stays on the loop (five more stop bytes: `find_any`'s). Must improve: `yaml/mixed/4` (its simple strings are plain: `bench/bench.c:1449`, `2118`). Must not regress: the other `yaml/*/4` cells.
    6. **`lib/serialize/src/yamlparse.c:498-534` `yp_plain_line`, block context only.** Stops: `find_lt(0x21)` (outermost: blanks and the line end, which the loop's blank branch and `yp_plain_stop`, 481-495, judge), `':'`, `find_byte(0x7F)`. The run is copied as in 3. A `'#'` inside a run is content (a comment needs a blank before it, and a blank ends the run). Must improve: `yaml/mixed/4`. Must not regress: the other `yaml/*/4` cells.
    7. **`lib/text/src/utf8.c:58-60` `cf_text_utf8_valid`**, under every text document (`cf_encoding_check` from `cf_codec_decode`, `lib/serialize/src/codec.c:279-290`) and every CBOR text string (`cf_text_is_utf8`, `lib/serialize/src/blob.c:38-40`, `cbor.c:225`). The body becomes a loop: `i += ops->ascii_run(p + i, n - i)`; at a byte from 0x80, `k = cf_text_utf8_decode(p + i, p + n, &cp)` (`utf8.c:15-56`: RFC 3629's rules, as `scalar_utf8_valid`'s: no overlong form, no surrogate, nothing above U+10FFFF, a cut sequence not in the prefix); `k == 0` ends it, else `i += k`. `utf8_valid`'s own word body stays out (P-2: it loses on non-ASCII). The comment at `lib/text/include/configforge/cf_text.h:133` then says "ASCII runs through lanes_best()'s ascii_run". Must improve: `cbor/strings-plain/4`. Must not regress: every gated cell, and the `strings-utf8` documents of JSON and CBOR (not in the quick scope: measured by hand, step 4d).
    8. **`lib/text/src/space.c:31-36` `cf_text_eol`**, rules `CF_NL_LF` (`find_byte('\n')`) and `CF_NL_ANY` (`find_byte('\n')`, then `find_byte('\r')` over the bytes before it). One call is one line, so no `next_` locals. `CF_NL_TOML` and `CF_NL_JSON5` stay on the loop (`cf_text_break`'s two- and three-byte forms, 10-29). Callers: INI's comments and values (`lib/serialize/src/ini.c:335`, `375`, `406`), `.properties` (`properties.c:57`), YAML's line end (`yamlparse.c:213`), comments (`space.c:72`, `145`). Must improve: `ini/strings-plain/4`. Must not regress: `ini/mixed/4`, `ini/whitespace/4`, every `yaml/*/4`.
  * **Measured, and the rule that keeps a change.** Instructions, not time: `scripts/bench/run.py --scope quick` counts each 4 MiB cell under callgrind (`scripts/bench/run.py:124-141`), the cells bench-gate gates (`.github/workflows/ci.yml:555-580`; `scripts/bench/gate.py` fails above +10%). A change is kept when its "must improve" cell has at least 5% fewer instructions than the state before it, with GCC and with Clang (Release, `-O2`), and no "must not regress" cell has more than 1% more with either. Otherwise it is reverted (`git revert` of its commit, so the numbers stay in the history). A change that wins with one compiler and loses with the other is reverted. MSVC is judged only by CI staying green: there is no instruction count for it here.
  * **The commit message** of each change carries its table: each named cell, before and after, GCC and Clang, instructions and percent. A revert carries the same table.
  * **Tests.** A new `lib/serialize/tests/test_runs.c`, added to `cf_unit_tests(test_holds test_text_escape test_runs)` (`lib/serialize/tests/CMakeLists.txt:6`), C89, with a capturing `cf_doc_sink` of its own (the `nul_*` sink of `test_text_escape.c:717-760` throws the value away). For candidates 1 to 6 and 8, a loop puts each stop byte of the scan at every offset 0 to 19 of a run of 0 to 19 ASCII letters (more than two 8-byte words, and both tails), decodes it, and compares with what the case computes: the value's text, or the refusal message with its `line:col` (as `read_doc`, `test_text_escape.c:764-787`, reads it). For candidate 7, `cf_text_utf8_valid` against `lanes_scalar()->utf8_valid` over every valid and invalid 1- to 4-byte sequence of `lib/text/tests/test_text.c:66-75` at every offset 0 to 19 of ASCII padding. The YAML cases are guarded by `#if !CF_YAML_LIBYAML && !CF_YAML_LIBFYAML` (`test_text_escape.c:811`). They run in the default build and in a `CF_LANES_BACKENDS=scalar` build.
  * **Stays out**: SIMD (the maintainer's `CF_WITH_SIMD` decision, `docs/Roadmap.md:1538-1542`); the word bodies of `find_any` and `utf8_valid`, kept scalar by P-2 (`lib/lanes/src/swar.c:67-72`), and with them every scan of four or more single stop bytes (JSON5 strings, YAML flow plains, `text_block_comment`); caching a set's shape in `lanes_set` (A-2's struct, the maintainer's); `count_byte`, which no reader scan matches (a line count needs the last break's address for `cf_text_track_line`, not a count); YAML's own word scan of comments (`yamlparse.c:126-172`, `yp_word_odd`), already a word at a time: moving it onto lanes removes a duplication rather than winning instructions, and is written down in `docs/Performance.md` for the next card; any change to `lib/lanes/`.
* **files**: `lib/serialize/tests/test_runs.c` (new), `lib/serialize/tests/CMakeLists.txt`, `lib/serialize/src/json.c`, `lib/serialize/src/toml.c`, `lib/serialize/src/yamlparse.c`, `lib/text/src/utf8.c`, `lib/text/include/configforge/cf_text.h`, `lib/text/src/space.c`; last: `bench/baseline/instructions.json` (from CI's artifact), `docs/Performance.md`, `CHANGELOG.md`, `docs/Roadmap.md`, `docs/Codemap.md` and `docs/API.md` (generated).
* **steps**:
  1. `git switch work/P-3` (cut from `main` at 70ad1c3 or later).
  2. Write `test_runs.c` for all eight candidates. It passes on the unchanged readers: it pins what they compute now, and is the oracle for every change. Run it in the default build and in `cmake -S . -B build-p3-scalar -DCF_LANES_BACKENDS=scalar`.
  3. The baseline, on Linux (WSL, a copy off `/mnt`), from the unchanged tree, once per compiler: `cf_bench` and `bench/compare` built as bench-gate builds them (`ci.yml:563-571`), the second also with `-DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++` into `build-cfb-clang`; then `python3 scripts/bench/run.py --scope quick --build <build-cfb|build-cfb-clang> --cfbench build-bench/bench/cf_bench --docs $HOME/p3-docs --out p3-<gcc|clang>-0.json`.
  4. Per candidate k, in the order above:
     a. Make the change as decided.
     b. **See it fail**: make the run one byte longer (`m + 1`) and see `ctest -R test_runs` fail; put it back.
     c. `ctest` in the default and in the scalar build.
     d. The quick run with both compilers into `p3-<gcc|clang>-<k>.json`, compared with the last kept state j: `python3 -c "import json,sys;g=lambda f:{'%s/%s'%(c['format'],c['doc']):c['instructions'] for c in json.load(open(f))['cells'] if c.get('lib')=='configforge' and c.get('mode')=='count' and c.get('size_mib')==4};a,b=g(sys.argv[1]),g(sys.argv[2]);[print('%-24s %+6.1f%%'%(k,(b[k]/a[k]-1)*100)) for k in sorted(a) if k in b]" p3-gcc-<j>.json p3-gcc-<k>.json`, and the same for Clang. For candidate 7 also, before and after: `valgrind --tool=callgrind --collect-atstart=no --toggle-collect=cfb_cell --callgrind-out-file=<f> <build-cfb>/cfb_compare --lib configforge --doc $HOME/p3-docs/<json-strings-utf8-4.json|cbor-strings-utf8-4.bin> --mode count`, the `summary:` line.
     e. `cmake -DMODE=quick -DEXTRA="conformance;fuzz" -DFUZZ_SECONDS=60 -DSOURCE=index -DJOBS=3 -DWORK=$HOME/cfv -P scripts/verify.cmake`: yaml-test-suite, JSONTestSuite and toml-test against `tests/conformance/floor.txt`, and libFuzzer a minute on each reader.
     f. Commit with its table; or, by the rule, commit and revert. Push after each.
  5. `docs/Performance.md`: a part "The readers on lanes (P-3)" after P-2's, one row a candidate (cell, GCC and Clang before and after, kept or reverted), and the line on `yp_word_odd`. `CHANGELOG.md`: one "Changed" entry under the open version for the kept scans. `docs/Roadmap.md:1514-1523`: P-3 done, SSE2 and NEON next.
  6. `python scripts/work/work.py format`; `cmake -P scripts/generate/codemap.cmake` and `cmake -P scripts/generate/apiref.cmake` on Linux, last; ACCEPT.
  7. After CI is green: bench-gate prints "N% fewer instructions than the baseline" for the cells that moved. Write the baseline from that run's `bench-quick` artifact, `python3 scripts/bench/gate.py --write bench-quick.json`, never from a laptop (`gate.py`'s docstring), and commit it.
* **accept**: ACCEPT-LINUX, with step 4e passing for the last state; ACCEPT-WINDOWS; ACCEPT-CI, bench-gate included; `ctest -R test_runs` exit 0 in the default and the `CF_LANES_BACKENDS=scalar` builds.
* **stop if**: a candidate cannot be made without changing what a reader returns (a `test_runs` case, a conformance case or a fuzzer finding moves): revert it, write the input under notes and go on to the next; no candidate keeps its change: close P-3 with the table in `docs/Performance.md` and no code change, and say there what the SSE2 and NEON card should look at instead; the worker passes ~60 turns: stop at a pushed commit, and the candidates not done become P-3b.
* **notes**:
  * **Waits for 2.4.0** (organiser, 2026-11-25, from the cold read). D-101 (`docs/plan/A-architecture.md:149-156`, 176-180) says the readers call `text` and `numeric`, which call `lanes_scalar()` only, until 2.4.0 adds the program's choice of backend (`cf_builder_lanes`): "nothing is detected or chosen behind the caller's back". This card has the readers call `lanes_best()` themselves. D-102 (`A-architecture.md:207-209`) puts P-3's scans into "the few functions in `lib/text/`", and this card puts them inline in `json.c`, `toml.c` and `yamlparse.c`. The card is kept as the measured plan for 2.4.0 and is rewritten then on both decisions; nothing is carried before.
  * **What the cold read found** (two readers who did not write the card; to fix in the 2.4.0 rewrite):
    * P-3 step 4d (measuring each candidate) and step 3, citing ci.yml:563-571 for how bench-gate builds build-cfb: bench/compare/CMakeLists.txt:69-96 configures and builds ConfigForge into build-cfb/configforge only at configure time, and only `if(NOT EXISTS "${cf_bin}/libconfigforge_serialize.a")`. Line 189 imports it as a prebuilt library. CI always starts from a fresh tree, so the cited build works there. On the worker's machine, rebuilding build-cfb (or build-cfb-clang) after a candidate's change leaves the old library in place, so every after-change count would measure the unchanged readers. The step needs a fresh build dir per measurement, or the build-cfb/configforge library removed or rebuilt.
    * P-3 decided "Tests": the guard `#if !CF_YAML_LIBYAML && !CF_YAML_LIBFYAML` cited at test_text_escape.c:811, together with lib/serialize/tests/CMakeLists.txt:6: The two macros are set per target, for test_text_escape only (lib/serialize/tests/CMakeLists.txt:9-14). If test_runs is only added to `cf_unit_tests(...)` at line 6, as the card says, it never sees them. Its guard is then always true, and its YAML cases run against libyaml or libfyaml builds. The card has to add test_runs to those two target_compile_definitions blocks, or the cited guard does not hold for the new file.
    * Candidates 1, 2, 3 and 6: the one-check run copy, citing json.c:225-228 (js_too_long), toml.c:218-228 (toml_put) and yamlparse.c:436-442 (yp_put): The per-byte checks report at the current pointer: js_too_long uses js_fail(s, s->p, ...) (json.c:103-105), toml_put uses toml_fail(s, s->p, ...) (toml.c:221), and yp_put uses yp_fail(y, ...) at y->p. In today's loop that pointer is at the byte that does not fit. If the whole run is checked before s->p or y->p moves, the refusal names the run's first byte instead, so its line:col changes. JSON keys hit this cap in practice: they go into scratch[CF_KEY_MAX] (json.c:48). So 'the same check, for m bytes' does not compute what the reader computes now, and the card's own stop-if rule would fire. test_runs (runs of 0 to 19 bytes, below CF_KEY_MAX) would not catch it.
    * Candidate 3: `yamlparse.c:684-746 yp_quoted (its loop; the function ends at 758)`: The function starts at 684, but its loop is `for (;;)` at 692 to 748. Line 746 is inside the final yp_put block, and 747 (`++y->p;`) and 748 (the loop's closing brace) are left out.
    * Candidate 5: "the trim of trailing blanks at 1027-1029 is unchanged" (yamlparse.c): The plain branch's while loop closes at 1027. The trim `while (q > y->p && yp_blank(q[-1])) --q;` is at 1028-1030.
    * Candidate 7 tests: "every valid and invalid 1- to 4-byte sequence of lib/text/tests/test_text.c:66-75": Lines 66-75 are test_valid's eight checks: empty, ASCII, one 2-byte letter, a cut 2-byte sequence, a surrogate, an overlong 2-byte form, U+10FFFF, and past U+10FFFF. There is no valid 3-byte sequence, no cut 3- or 4-byte sequence and no bad continuation byte. Those cut and bad-continuation cases are the decoder's tests at test_text.c:55-61. 'Every ... 1- to 4-byte sequence' is not what 66-75 holds.
    * Candidate 8 callers: "INI's comments and values (lib/serialize/src/ini.c:335, 375, 406)": Line 335 is a comment (`#` or `;`) and 406 is a value. Line 375 is neither: it skips the rest of the line after a section header (it follows the 'trailing text after a section header' check).
    * "Measured" rule: "`scripts/bench/run.py --scope quick` counts each 4 MiB cell under callgrind (scripts/bench/run.py:124-141)": Lines 124-141 are `instructions()`, which runs callgrind on one document. Choosing the quick scope's 4 MiB count cells (QUICK_DOCS, size 4, mode count) happens in run_cells at 157-174.
    * docs/plan/P-word-at-a-time.md:117 and :122 (steps 3 and 4d) vs bench/compare/CMakeLists.txt:69-95: bench/compare configures and builds ConfigForge into build-cfb/configforge only `if(NOT EXISTS .../libconfigforge_serialize.a)`, at configure time. After a candidate's change, `cmake --build build-cfb --target cfb_compare` (or re-running run.py with --build build-cfb) links the old library, so step 4d measures the unchanged readers, sees no gain, and the rule reverts a change that may win. The card never says how to rebuild between candidates (e.g. `cmake --build build-cfb/configforge --target configforge_serialize`, then cfb_compare, for both build-cfb and build-cfb-clang, or a fresh build dir per state).
    * docs/plan/P-word-at-a-time.md:101-103 (candidates 1, 2, 3: the run copied after one capacity check): js_too_long (json.c:103-105), toml_put/toml_fail (toml.c:218-228) and yp_fail all report the error at s->p / y->p. The per-byte loop fails with p on the first byte that does not fit. A whole-run check made before p moves fails with p at the run's start, so the refusal's line:col changes. JSON and TOML keys are read into scratch[CF_KEY_MAX] / names[CF_KEY_MAX] (json.c:572), so a key longer than CF_KEY_MAX gets there easily. The card does not say whether p moves to the byte that overflows before the failure (to keep the old position) or whether the position may change. test_runs (runs of at most 19 letters), conformance and fuzz never test it, so the change would go in silently, against the card's own 'stop if' (:129).
    * docs/plan/P-word-at-a-time.md:108 (candidate 8, CF_NL_ANY): `find_byte('\n')` first, then '\r' over the bytes before it, is not bounded by the line. On a document whose lines end in a lone CR (valid for CF_NL_ANY: YAML, .properties; N-20 just made lone CR matter in YAML comments), every call scans for '\n' to the end of the document. yp_line_rest (yamlparse.c:213) runs once per value line and properties.c:57 once per line, so this is O(lines x size): the quadratic fault the card itself rules out at :99. 'One call is one line' holds only for LF input. The card needs a bound, for example search '\r' first and '\n' only up to it, or keep CF_NL_ANY on the loop.
    * docs/plan/P-word-at-a-time.md:111 vs lib/serialize/tests/CMakeLists.txt:6-14: The YAML cases are to be guarded by `#if !CF_YAML_LIBYAML && !CF_YAML_LIBFYAML`, but CMakeLists.txt passes CF_YAML_LIBYAML=1 / CF_YAML_LIBFYAML=1 to test_text_escape only (target_compile_definitions at :9-14). The card adds test_runs to cf_unit_tests and nothing else. In a libyaml or libfyaml build the macros are undefined, the guard is true, and the built-in parser's expectations run against libyaml. The card should also add the same two target_compile_definitions for test_runs (CMakeLists.txt is already in its file list).
    * docs/plan/P-word-at-a-time.md:111 ('compares with what the case computes') and :116: The oracle is never chosen. One reading: the test derives each expected value and refusal (message and line:col) from its own model of each reader, per stop byte and format (for example, what TOML says at 0x7F, YAML at a tab inside a quoted run, a key look-ahead that ends on a break). The other: it pins what the unchanged reader returns (step 2's 'pins what they compute now'). The card also does not say whether 'each stop byte' of a class stop (find_lt(0x20), find_lt(0x21)) means all 32 or 33 bytes or one representative. A middle-tier worker has to make these choices.
    * docs/plan/P-word-at-a-time.md:93, :97 vs docs/plan/A-architecture.md:207-209 (D-102): D-102 says P-3's scans become 'the few functions in lib/text/', so that one optimization reaches every format. The card instead puts lanes calls inline in json.c, toml.c and yamlparse.c, with per-reader next_<kind> locals and the table kept in each reader's scan state. Only candidates 7 and 8 live in lib/text. The card cites D-102 as backing it, but this is the layering D-102 decided against. Either the card or the decision has to change, and that is the maintainer's call.
    * docs/plan/P-word-at-a-time.md:97 vs docs/plan/A-architecture.md:149-156, 176-180 (D-101): D-101: 'The program asks for one and keeps it ... Nothing is detected or chosen behind the caller's back', and 'In 1.8.0 the readers call text and numeric, which call lanes_scalar() only ... 2.4.0 adds the other backends and the program's choice, cf_builder_lanes'. In the card, the readers and lib/text call lanes_best() themselves, so the library chooses the backend for the program, before the program-choice API D-101 schedules exists. The card only checks D-101 against 'no global mutable state'. It does not say whether this goes ahead of cf_builder_lanes or how it connects to it later (and from SSE2 on, lanes_best() in cf_text_eol would detect once per line).
    * docs/plan/P-word-at-a-time.md:123 (step 4e) vs scripts/verify.cmake:5, 21, 278-284: SOURCE=index verifies what is staged, in a copy. Step 4e runs before step 4f's commit, and nothing says to `git add` the change first, so verify checks the last committed state, not candidate k.
    * docs/plan/P-word-at-a-time.md:117-124 (steps 3-4, 'WSL, a copy off /mnt'): The baseline is built from a Linux copy of the tree. The card does not say where the edits are made, or how each candidate's change gets into that copy before 4c-4e (and into the clang and scalar build directories). A worker editing the Windows worktree would bench a stale copy.
    * docs/plan/P-word-at-a-time.md:120 (step 4b, 'make the run one byte longer (m + 1)'): Candidate 7 (`i += ascii_run(...)`) and candidate 8 (one find_byte, no run length m) have no `m`. Candidates 4 and 5 skip a run rather than copy one. The card does not say which deliberate fault proves those four tests can fail.
    * docs/plan/P-word-at-a-time.md:99 ('const char* next_<kind>', 'p > next_<kind>') vs lib/lanes/include/lanes/lanes.h:109-112 and :82-98: The lanes operations take `const unsigned char*` and return an index, or n when nothing is found, not a pointer. The card does not state the conversion (next = p + op((const unsigned char*)p, bound - p, c)). It also does not state that 'not found' becomes the bound, which is why the `p > next_<kind>` re-search rule works for inner kinds bounded by the outermost result. Small, but it is the exact place an off-by-one would make test_runs fail.

---

## After P-3: what else is weighed, by the same rule

Every item below is measured against the same baseline (P-1, `-O2`)
and kept only for a number, simplest form first. P-1's profile decides
the order; none is a card until it says where the time goes.

* **Build options, not code.** Auto-vectorization is on at `-O2` from
  GCC 12 with the "very cheap" cost model and in full at `-O3` or
  `-ftree-vectorize`; LTO and PGO. Measured and documented as options a
  packager can turn on (`CF_LTO`, a PGO recipe); the code relies on none
  of them, since a shipped library is built `-O2` for a generic target.
* **Algorithm before instructions.** A 256-entry character-class table
  per lexer (portable C89, one load and one test a byte, often worth
  more than SWAR); fewer copies (a string that needs no unescaping
  pointed to in the input, not copied); fewer allocations (arena sizes
  from the input's length, D-76's rest); for a document of many MiB,
  splitting the work across threads where the format allows it (JSON
  Lines, CBOR sequences), behind `CF_SINGLE_THREADED`'s rule.
* **SIMD, behind `CF_WITH_SIMD` (off).** Intrinsics differ between
  compilers as well as processors (MSVC against GCC and Clang, and what
  each accepts without `-m` flags), so a portable layer is chosen
  rather than written per compiler: SIMDe (C, MIT, header-only; needs
  C99, which D-72 allows for an opt-in layer) is the first candidate,
  a small intrinsics layer in `lib/platform/src/` of our own the second.
  The instruction set is chosen at run time (CPUID on x86, the
  auxiliary vector on Arm), never assumed from the build machine.
  simdjson and Highway are C++ and not usable in the C core; the C++
  wrapper could use them, if a C++ user's numbers ask for it.
* **GPU: a research note, not a card.** Configuration files are KB to
  a few MiB and their parsing branches on every byte; moving them to a
  GPU costs more than it saves. GPU JSON readers exist (RAPIDS cuDF)
  and pay at GiB-sized batches of uniform records. Revisited only if a
  user brings such a load, with the threshold measured: the size at
  which transfer plus GPU parse beats the CPU path.

---

## What keeps a gain

* **A regression gate.** Once P-1 has a baseline, CI fails a change
  that makes a format's large-document run more than 10% slower.
  Wall time on a shared runner is noise, so the gate counts
  instructions (`callgrind`, or `perf stat -e instructions`), and the
  wall-time numbers stay a report.
* **Speed is not the only number.** Every wave also reports code size
  (`.text` of the library, in the default and the `minimal` builds) and
  peak memory per MiB of input. Tables, unrolled loops and SIMD grow
  the code; on the embedded targets 1.8.0 adds to CI, growth of the
  `minimal` build is a cost, weighed against the gain, not ignored.
* **A reference point (P-1b: the comparison and its page).** P-1's benchmark also runs the same documents
  through the other libraries of the Roadmap's comparison (1.8.0), so
  "faster" means faster than something a user would otherwise pick,
  not only faster than before.
