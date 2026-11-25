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
  kept beside it, under `CF_WITH_SWAR=0`, and the tests run both. The
  MISRA profile builds with `CF_WITH_SWAR=0`: the `memcpy` word read is
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

## P-3 - The scans P-1 found, a word at a time, kept only where they win

* **id**: P-3
* **goal**: the scans P-1 named as costly call P-2's primitives, and the
  benchmark shows each change paying, or it is reverted.
* **why**: the point of the two cards before.
* **needs**: P-2
* **size**: medium
* **decided**:
  * Candidates, to be confirmed by P-1's profile and nothing else:
    `lib/serialize/src/json.c` `js_string` (the run up to `"`, `\` or a
    control byte) and `js_utf8_at` (ASCII runs skipped whole), the
    whitespace skips of the JSON, YAML and TOML lexers, `ini.c` and
    `properties.c`'s blank skips (`while (... == ' ' || ... == '\t')`),
    and `detect.c`'s.
  * For each candidate, three forms are measured on its format's
    large-document line, at `-O2` with GCC and Clang, and with MSVC in
    CI: (a) the byte loop reshaped for the compiler, (b) the C library
    call, (c) P-2's primitive. The simplest form within 5% of the
    fastest is kept, in that order: compiler, then C library, then
    SWAR. A form that wins on one compiler and loses on another is not
    kept.
  * One scan a commit, each with its numbers for the three forms in the
    commit message; a change under 5% against the unchanged loop is
    reverted, since it costs reading.
  * The conformance suites (yaml-test-suite, JSONTestSuite, toml-test)
    and the fuzzers run after each, in both `CF_WITH_SWAR` builds.
* **files**: whichever of the candidates P-1 names; last:
  `docs/Performance.md`, `CHANGELOG.md`, `docs/Roadmap.md`.
* **steps**:
  1. Take P-1's list, largest first.
  2. Per scan: change, bench, conformance, fuzz a minute; keep or
     revert.
  3. The new numbers beside the baseline.
* **accept**: ACCEPT-LINUX (tier 2: conformance included),
  ACCEPT-WINDOWS, ACCEPT-CI.
* **stop if**: no scan is in P-1's top functions (the time is elsewhere:
  allocation, the flattener): write that down, close P-3 with no change,
  and say what the SIMD card should look at instead.
* **docs**: Roadmap: "Large files" stays 🟨, "SWAR done (P-1..P-3);
  SIMD next, behind `CF_WITH_SIMD`".
* **notes**:

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
