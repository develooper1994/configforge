# Allocation: measured first, changed only where it wins

Written 2026-11-22. The maintainer asked whether allocator work could give
a real speed-up. This file answers with what the tree does today, what the
literature says, and one card whose first steps measure. Nothing here is a
claim about ConfigForge's numbers: no benchmark was run to write it.

## Findings

### What the library does now

* **One interface.** Every allocation goes through `cf_allocator`
  (`alloc`, `release`, `ctx`) declared in
  `lib/base/include/configforge/cf_alloc.h`; `gate.symbols` fails if any
  object but `lib/base/src/alloc.c` names `malloc` (D-07, "Closed in
  implementation"). The program supplies its own allocator per
  configuration (`cf_build(alloc)`, `cf_arena_new(alloc, ...)`), so a
  program can already put mimalloc, a pool or a counting allocator under
  the library without the library choosing it.
* **One arena per configuration, released whole** (D-03, "Closed in
  implementation"). `cf_arena_alloc` is a pointer bump inside a block; a
  full block means one call to the allocator for a new block. The arena
  is a linked list of blocks, wiped (volatile zeroing, so secrets do not
  outlive the configuration) and released at `cf_arena_free`.
  `cf_arena_fixed` works inside a caller's buffer and returns NULL when
  full (the freestanding profile; D-07).
* **The block size is constant.** `cf_arena_new(a, 0)` is what every
  caller in `lib/config/src` passes (`config.c`, `edit.c`, `live.c`,
  `schema.c`); a hint under 1 KiB becomes 4096. A request larger than the
  block size gets a block of its own. So an arena holding a 16 MiB
  document's key space takes thousands of 4 KiB blocks, one allocator
  call each, with no growth in block size. "Fewer allocations (arena
  sizes from the input's length, D-76's rest)" is listed in
  `docs/plan/P-word-at-a-time.md` ("After P-3") as open, not done.
* **Growth abandons memory.** `cf_ks_resize`
  (`lib/config/src/core/keyspace.c`) doubles the candidate array by
  allocating a new one in the arena and copying; the old one stays until
  release ("costs at most the same again"). `lib/serialize/src/blob.c`
  (the output buffer) and `detect.c` allocate in the arena the same way.
  These are the places where size knowledge would save copies and bytes.
* **Where allocations happen per document** (by reading, not measuring):
  the readers (`lib/serialize/src/{json,yaml,toml,ini,xml,cbor}.c`) are
  event producers and allocate nothing themselves; the text and nodes are
  made by the consumers: the key space (`keyspace.c`: candidate records,
  the copied key and value text by `cf_arena_alloc(len + 1)`, two sort
  buffers of `n * sizeof(cf_cand_rec*)`, the final `cf_entry` array), the
  builder (`config.c`: per-source state, `cf_arena_dup` of names and
  bytes) and `schema.c`, `validate.c`, `define.c` for typed views. So
  the cost per document is one arena bump per key and per value (cheap)
  plus one allocator call per 4 KiB block. The numbers to find are how
  many block calls a large document costs and how many bytes the
  doubling abandons.
* **What is already measured.** `bench/bench.c` has a counting allocator
  (`counted_alloc`, "abandoned blocks and all") for the memory a load
  asks of the system. `docs/Performance.md` reports, through
  `scripts/work/profile.py` (L-5), time per function (perf, callgrind)
  and heaptrack's "peak, N allocations" or massif's peak heap per
  format. P-1b adds the other libraries and the page that shows results
  over time. No document yet says what share of time is in `malloc` and
  `free` or in `cf_arena_alloc`'s block refill.
* **Fixed constraints.** C89 (D-72: newer standards only add); the
  `misra` and `minimal` profiles (D-105) and the fixed arena have no heap
  and no `stdlib`; a library must not replace the program's `malloc`.

### Techniques, and what the sources say

* **Arena (bump, region) allocation for a parsed document.** This is
  what the fast parsers do. yyjson's documents own all their memory, its
  mutable document uses a pool that "can only be created, grown, or
  freed in its entirety", an application can pass a `yyjson_alc` or set a
  compile-time default, and 0.10.0 added a dynamic allocator
  (`yyjson_alc_dyn_new`) that takes more memory as needed and frees it
  all at once
  ([API](https://github.com/ibireme/yyjson/blob/master/doc/API.md),
  [changelog](https://github.com/ibireme/yyjson/blob/master/CHANGELOG.md)).
  rapidyaml keeps a flat index-based tree whose nodes view the source
  buffer, supports per-tree allocators, and its benchmark has `_reserve`
  (target sized before parsing) and `_reuse` (arena kept between parses)
  variants, whose gain is large enough that comparisons without them are
  not like for like
  ([Is it rapid?](https://rapidyaml.readthedocs.io/latest/_sources/sphinx_is_it_rapid.md.txt)).
  ConfigForge already has the arena; what it lacks is what those
  projects measure: a size reserved from the input's length, and reuse.
  Caveat: the figures are the projects' own, vary by release (libyaml
  113.9 against 135.06 MB/s between two of their pages), and mix
  allocator effects with parser design. No libyaml-against-rapidyaml
  allocation count was found.
* **Block growth and sizing.** A constant 4 KiB block is the cheapest
  policy for tiny files and the dearest for large ones. Geometric block
  sizes (each new block a multiple of the last, capped) and a first block
  sized from the input's length are the usual remedies, need no new
  allocator, and change only `cf_arena_alloc`'s refill path. Likely the
  most real win and the least invasive; unproven until measured.
* **Reserving by count.** The key space knows `distinct` before it builds
  `entries` and could know a candidate count from the input size;
  avoiding the doubling copies saves bytes and time. Measured the same
  way.
* **Size-class or slab pools for many small nodes.** Useful when nodes
  are individually freed and reused. Here nothing is freed before the
  arena is, and each node is a bump of a few words, so a pool would
  replace a pointer bump with a free-list pop: no expected gain. Listed
  so it is not proposed again without a profile that says otherwise.
* **A general-purpose replacement (mimalloc, jemalloc, rpmalloc,
  snmalloc).** These speed up programs whose time is in `malloc` and
  `free` of many small objects of mixed lifetime. mimalloc's design is
  page-local sharded free lists with a tuned fast path
  ([Leijen, Zorn, de Moura, MSR-TR-2019-18](https://www.microsoft.com/en-us/research/publication/mimalloc-free-list-sharding-in-action));
  its README reports gains (13% over tcmalloc on Lean, over 2.5x over
  jemalloc on sh6bench, "a tad faster" on cfrac; benchmark dated
  2021-01-30) and says the preferred use is to link it and call
  `mi_malloc`, process-wide override (`LD_PRELOAD`,
  `DYLD_INSERT_LIBRARIES`) being the alternative
  ([README](https://github.com/microsoft/mimalloc)). For a library the
  consequence is plain: a program that wants one sets it process-wide or
  passes it through `cf_allocator`; the library must not link one in.
  ConfigForge's allocator calls are few (block refills), so a
  replacement can help only in proportion to how many calls the profile
  finds. Expected small; the card measures it. jemalloc, rpmalloc and
  snmalloc were not researched beyond this: the same reasoning applies.
* **The Goose claim.** The Goose found is the language by Wouter van
  Oortmerssen (aardappel),
  [github.com/aardappel/goose](https://github.com/aardappel/goose): a
  memory-safe language that compiles to C, with no heap allocator, no GC
  and no reference counting; dynamic values live on a few
  compiler-assigned data stacks, so growing a value advances a pointer
  and leaving the scope releases it with one store. Its page claims, on
  its own sixteen benchmarks, 3.3x the speed of idiomatic C++, 1.16x
  hand-optimized C++ and 1.12x the best safe Rust, with 1.9x, 1.3x and
  1.2x less memory (its summary table gives somewhat different figures,
  about 3.5x, 1.24x and 1.13x-1.19x, and the page does not explain the
  gap). So the claim is about a memory model decided by the compiler, not
  about a better allocator, and the numbers are the project's own, not
  reproduced here. It cannot be read as "allocator X is N times faster".
  What carries over is the idea `cf_arena` already has: allocation as a
  pointer bump and release as one operation. If the maintainer heard
  something else under that name, this search did not find it, and the
  claim as heard cannot be confirmed.

## P-4 - Allocation measured, and changed only where it wins

* **id**: P-4
* **goal**: the share of a large document's read that goes to allocation
  (allocator calls, abandoned bytes, `malloc` and `free` time) is
  measured per format; each change below that the measurement supports
  is kept only if `cf_bench` shows a win beyond noise on the formats'
  documents, and the rest is closed with its numbers written down.
* **why**: a configuration's memory is one arena, released whole (D-03,
  D-07); whether the arena's refill policy and the key space's growth
  cost time is a question for numbers, and a library that claims speed
  must know where its allocation stands without imposing an allocator on
  a program (D-07: the program supplies it).
* **needs**: P-1b (its page shows the result and the other libraries'
  figures; P-1's `cf_bench` and L-5's `profile.py` are done).
* **size**: medium
* **decided**:
  * **Measure first; nothing changes before step 5.** Per format (`json
    yaml toml cbor ini xml`), on P-1's large documents (16 MiB) and the
    `many-tiny` case: (a) allocator calls and bytes asked for, from
    `bench/bench.c`'s counting allocator and from heaptrack (else
    massif) through `scripts/work/profile.py`; (b) bytes abandoned by
    growth (`cf_arena_capacity` minus `cf_arena_used` at the end, plus
    `cf_ks_resize`'s old arrays); (c) the time share of `malloc` and
    `free` and of `cf_arena_alloc` (perf, callgrind: instructions
    count, wall time confirms). The result is a table in
    `docs/Performance.md`, "Allocation".
  * **Candidates, in this order, each a commit of its own:** (1) arena
    block growth: each new block is the larger of the request and the
    previous block size doubled, capped at a constant in `alloc.c`;
    (2) a first block sized from the input's length when the caller
    knows it (`cf_arena_new(a, hint)` already takes a hint; the builder
    passes the file's size for a file source, clamped to the cap);
    (3) capacity for the key space's candidate array and sort buffers
    derived from the input, so `cf_ks_resize` is not reached for a
    document whose size is known; (4) reuse: a documented way for a
    program that loads many documents to keep an arena and reset it
    (`cf_arena_wipe` then reuse), measured as rapidyaml's `_reuse`:
    built only if the single-document numbers leave it worth an
    interface that must then be kept.
  * **Kept only for a number.** A change is kept when the 16 MiB read of
    at least one format is faster by more than the noise P-1 found
    (instruction count first, wall time confirms), no format's
    small-file read is slower by more than 2%, and peak memory
    (heaptrack or massif) does not grow. A change under 5% is reverted:
    it costs reading (P-3's rule). Its numbers go under "notes" and in
    `docs/Performance.md`.
  * **No allocator is imposed on a program.** The library never links or
    selects mimalloc, jemalloc, rpmalloc or snmalloc. **Answered** (below),
    recommendation: **a benchmark comparison only**. `bench/compare/`
    (the C++17 project P-1b creates; never in the default build or the
    gates' build) gets a mode that passes a `cf_allocator` wrapping
    mimalloc's `mi_malloc` and `mi_free`, reported in the allocation
    table as "with mimalloc: x%". Why not an opt-in build option: the
    hook already lets a program pass any allocator, and a build option
    would put a third-party dependency into the `misra` and `minimal`
    verification and the release's provenance for a gain the profile may
    show is under 5%. If the measurement shows a large win (above 15%),
    the maintainer revisits it as a documented recipe in
    `docs/Performance.md` (how to pass one), still not a build option.
  * **Size-class and slab pools are not a candidate.** Nothing in the
    key space is freed individually (D-03); a pool replaces a bump by a
    list pop. Revisited only if the profile shows individually freed
    small objects.
  * **Constraints that stay.** C89 (D-72); no `stdlib` use outside
    `alloc.c` (`gate.symbols`); the fixed arena unchanged (growth
    applies to the growing arena only); `cf_arena_wipe`'s zeroing kept
    for every block; the `misra` and `minimal` profiles build and pass
    unchanged; `cf_arena_new`'s signature and `cf_allocator` untouched.
    A change that needs a new function is an addition, named in
    `docs/API.md` by `gate.apiref`.
  * **Answered below (Organiser's answers), recommendation each:** (a) the block-growth
    cap (recommendation: 1 MiB; a smaller cap wastes less tail, a
    larger one saves calls on 256 MiB documents; the measured table
    decides inside 256 KiB to 4 MiB); (b) whether candidate (4), the
    reuse interface, is wanted at all (recommendation: no, unless a
    program that loads many files shows a gain); (c) the mimalloc
    comparison as above.
* **files**: `bench/bench.c` (the abandoned-bytes figure), `bench/compare/`
  (the mimalloc wrapper, if (c) holds), `lib/base/src/alloc.c` (block
  growth), `lib/config/src/core/config.c` (the first-block hint),
  `lib/config/src/core/keyspace.c` (capacity), the matching tests in
  `lib/base/tests/` and `lib/config/tests/`; last:
  `docs/Performance.md`, `CHANGELOG.md`, `docs/Roadmap.md`, and
  `docs/Decisions.md` (a D-number if a policy stays).
* **steps**:
  1. Read P-1's baseline and P-1b's page; take the 16 MiB documents'
     numbers as the "before" for every later comparison.
  2. Run `python scripts/work/profile.py json yaml toml cbor ini xml`
     (heaptrack else massif; perf for time); record allocator calls,
     bytes asked, peak, and time share in `malloc`, `free` and
     `cf_arena_alloc`, per format, in `docs/Performance.md`
     "Allocation".
  3. Add to `bench/bench.c` the abandoned-bytes figure (capacity minus
     used, per load) beside `counted_alloc`'s total, and print it per
     format.
  4. Decide from steps 2 and 3 which candidates have room: a candidate
     whose targeted cost is under 5% of the read is closed with its
     numbers and not built. If none has room: stop (below).
  5. For each remaining candidate, in order: write the test that pins
     the behaviour (an arena of N bytes takes at most K allocator
     calls; contents wiped on free; `cf_arena_fixed` unchanged), see it
     fail, make the change, see it pass.
  6. Bench the change against step 1's "before" at `-O2` with GCC and
     Clang (MSVC in CI): keep or revert by the rule above, one commit
     per kept change with its numbers in the message.
  7. The conformance suites and the fuzzers (one minute each) run after
     every kept change; `gate.symbols` and the wipe test stay green.
  8. Write the final table (before, after, mimalloc comparison if
     decided) in `docs/Performance.md`; update the Roadmap and
     CHANGELOG.
* **accept**: ACCEPT-LINUX (tier 2: conformance included), ACCEPT-WINDOWS,
  ACCEPT-CI, plus: the Performance table has a "before" and "after" row
  per kept change, and the `misra` and `minimal` presets are green.
* **stop if**: step 2 shows allocation under 5% of the read on every
  format (the answer to the maintainer is then "not worth a change":
  write the numbers under "notes", close P-4 without code, say that the
  time is elsewhere); heaptrack and massif are both missing on every
  reachable machine (the counting allocator alone gives no time share:
  write that and stop); a candidate needs a change to `cf_allocator` or
  to the fixed arena's behaviour (D-07's design: hand it back); three
  failures on the same thing.
* **docs**: `docs/Performance.md` ("Allocation"); `CHANGELOG.md` (the
  heading of the release the change ships in); `docs/Roadmap.md`
  ("Large files": allocation measured); a D-number in
  `docs/Decisions.md` only if a policy (block growth, a hint) is kept.
* **notes**:
  * **Step 2 on Linux, 2026-11-23** (the profiling VM: kernel 7.0.0-44,
    Ubuntu 26.04.1, gcc 15.2.0, 4 cores; Release `-O2 -DNDEBUG
    -fno-omit-frame-pointer -g`, `-DCF_BUILD_BENCH=ON -DCF_WITH_XML=ON`).
    `cf_bench docs` and `profile.py` time the codec's event path
    (`cf_codec_decode` into a sink), which never touches `cf_arena` or
    the key space, so the measurement used a driver that loads each
    document through `cf_builder_document`, `cf_builder_create`,
    `cf_count` and `cf_config_free`, with a counting `cf_allocator`
    (the card's worker adds it to `bench/bench.c`: step 3 then measures
    the same path). Documents: `cf_bench --emit-docs`, `<fmt>-mixed-16`
    and `<fmt>-many-tiny-16`; callgrind on the 1 MiB `many-tiny`.
    Commands: `perf record -e cpu-clock -F 5000 -g` then `perf report
    --no-children --sort symbol`; `heaptrack` then `heaptrack_print`;
    `valgrind --tool=callgrind` then `callgrind_annotate
    --inclusive=no`.

    `mixed-16`, one load (perf self %, callgrind instructions %):

    | format | allocator calls | bytes asked | of them 4 KiB blocks | libc malloc+free (perf) | `cf_arena_alloc` (perf / callgrind) | `cf_arena_free` (perf / callgrind) | abandoned by growth (estimate) |
    | ------ | --------------- | ----------- | -------------------- | ----------------------- | ----------------------------------- | ---------------------------------- | ------------------------------ |
    | json | 12,727 | 443 MiB | 12,715 | 0.31 | 0.78 / 0.97 | 9.7 / 25.7 | 156 MB |
    | yaml | 11,788 | 424 MiB | 11,778 | 0.30 | 0.65 / 1.03 | 15.4 / 15.7 | 101 MB |
    | toml | 12,520 | 410 MiB | 12,476 | 0.25 | 0.57 / 0.96 | 10.7 / 23.3 | 72 MB |
    | cbor | 17,518 | 566 MiB | 17,504 | 0.38 | 0.68 / 1.04 | 11.4 / 27.9 | 205 MB |
    | ini | 11,360 | 404 MiB | 11,351 | 0.45 | 1.10 / 1.18 | 11.2 / 28.9 | 100 MB |
    | xml | 8,047 | 172 MiB | 8,029 | 0.81 | 0.63 / 0.75 | 7.0 / 11.0 | 3 MB |

    `many-tiny-16` (about 350,000 to 417,000 documents of about 48
    bytes, one configuration each): 7 allocator calls per document, 4 of
    them 4 KiB blocks, 22,784 bytes live per document; libc
    malloc+free 2.6 to 3.3% (xml 6.2%), `cf_arena_alloc` about 1.2%,
    `cf_arena_free` 47 to 73% of perf's samples and 67 to 86% of
    callgrind's instructions. Kernel page faults and `clear_page` are 9
    to 17% of the 16 MiB loads. `cf_ks_resize` is inlined and has no
    symbol; the abandoned bytes are estimated from the sizes of the
    large allocations (the candidate array starts at the guess capped by
    `CF_KS_EXPECT_MAX`, 96 bytes a candidate, and doubles), the exact
    figure is step 3's.

    Reading: a general allocator (mimalloc and the like) has under 1% to
    win on large documents and about 3% on tiny ones; `cf_arena_alloc`
    about 1%. The cost is `cf_arena_free`, which is `cf_arena_wipe`:
    `cf_wipe` stores one byte at a time through a volatile pointer.
    Windows (MSVC) figures: not taken (the deputy's budget ran out on
    2026-11-23 before it reached P-4); the card's worker adds them.
  * **Candidate (0), kept, 2026-11-23** (commit on `work/P-4`): `cf_wipe`
    is `memset` through a volatile function pointer. Same VM and flags,
    the same driver linked against main (8ef51e9) and against the
    change; callgrind instructions of one load: json `mixed-1`
    569,665,696 to 430,337,353 (-24.5%); cbor `mixed-1` 673,817,626 to
    494,910,137 (-26.5%); json `many-tiny-1` 1,820,347,853 to
    513,607,893 (-71.8%). Wall time, two runs each (`/usr/bin/time`, the
    VM's 3 GB make the 16 MiB loads noisy): `many-tiny-16` json 1.87,
    1.66 s to 0.47, 0.48 s; yaml 2.39, 2.11 to 0.78, 0.71; cbor 2.06,
    2.06 to 0.54, 0.66; ini 1.82, 1.80 to 0.49, 0.53; `mixed-16` json
    1.29, 1.16 to 1.09, 1.04; yaml 1.37, 1.16 to 1.07, 1.07; ini 0.98 to
    0.89 (its first "before" run, 2.43, was the cache warming). The wipe
    tests (`ctest -R "wipe|alloc|arena|secret"`, 9 tests) pass. Left for
    the card: step 3's abandoned-bytes figure in `bench/bench.c` and its
    builder path, then candidates (2), (1) and (3), measured against this
    commit as the new "before".
  * **Step 3 on Windows, 2026-11-23.**
    Windows x64, Intel Core i7-10750H, MSVC 19.52.37029, Ninja Release
    (`/O2`), built-in YAML. CMake could not find Expat. The library is
    `d32708a`. Run `cf_bench allocation` with `CF_BUILD_BENCH=ON`.
    Each row totals one pass over the generated input. Each tiny document
    gets its own builder and configuration. The time is the fastest of three
    passes, including release and excluding input generation. Bytes asked
    and arena slack are sums over loads. Slack counts unused block capacity.
    Superseded key-space arrays still count as used.

    | Format | Document | MiB | Loads | Allocator calls | Bytes asked | Capacity minus used (bytes) | Best of 3 (s) |
    | --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
    | json | mixed | 1 | 1 | 812 | 29266232 | 18056 | 0.066 |
    | json | many-tiny | 1 | 21907 | 153349 | 499129088 | 187458504 | 0.047 |
    | json | mixed | 16 | 1 | 12729 | 464762944 | 72600 | 1.338 |
    | json | many-tiny | 16 | 350322 | 2452254 | 7981736448 | 2997693496 | 1.293 |
    | yaml | mixed | 1 | 1 | 752 | 17171408 | 13544 | 0.072 |
    | yaml | many-tiny | 1 | 25736 | 180152 | 586369024 | 219035856 | 0.078 |
    | yaml | mixed | 16 | 1 | 11790 | 444904736 | 56032 | 1.600 |
    | yaml | many-tiny | 16 | 411573 | 2881011 | 9377279232 | 3502811688 | 1.636 |
    | toml | mixed | 1 | 1 | 826 | 26511944 | 40960 | 0.066 |
    | toml | many-tiny | 1 | 24465 | 171255 | 557410560 | 153104800 | 0.053 |
    | toml | mixed | 16 | 1 | 12521 | 429669712 | 113384 | 1.563 |
    | toml | many-tiny | 16 | 391185 | 2738295 | 8912759040 | 2448053152 | 0.718 |
    | cbor | mixed | 1 | 1 | 1208 | 38431824 | 26272 | 0.062 |
    | cbor | many-tiny | 1 | 26061 | 182427 | 593773824 | 222786232 | 0.038 |
    | cbor | mixed | 16 | 1 | 19418 | 601566920 | 143920 | 1.200 |
    | cbor | many-tiny | 16 | 416751 | 2917257 | 9495254784 | 3562631728 | 0.600 |
    | ini | mixed | 1 | 1 | 725 | 26611648 | 12952 | 0.038 |
    | ini | many-tiny | 1 | 25655 | 179585 | 584523520 | 219504904 | 0.036 |
    | ini | mixed | 16 | 1 | 11362 | 423135768 | 56824 | 0.736 |
    | ini | many-tiny | 16 | 410326 | 2872282 | 9348867584 | 3510741560 | 0.812 |

  * **Candidate (2) awaits a design decision.** `cf_build(alloc)` creates
    the arena and allocates the builder and its definitions in it before
    the caller supplies a document length or file path. A one-shot hint
    on the next refill would leave the first block as allocated. Sizing
    the actual first block requires a different builder construction or
    ownership scheme. Which behaviour should the pinning test require?
    Candidates (1) and (3) follow (2) and have no Windows trial yet.
  * **Candidate (2), Windows trial, reverted.** MSVC x64 Release, Ninja,
    same documents and counting allocator as step 3. The baseline was
    rerun at b871526 (d3f58de plus the answer). Triplets were timed before
    a separate untimed pass tracking the sum of live requested bytes at
    each allocation and release. Peak is per configuration, excluding
    allocator overhead. Each pair is baseline / trial; a single number
    means both were equal. The original step 3 timings remain above.

    | Format, case, MiB | Calls | Bytes asked | Slack (bytes) | Peak (bytes) | Best of 3 (s) |
    | --- | ---: | ---: | ---: | ---: | ---: |
    | json mixed 1 | 812 | 29266232 | 18056 | 29266184 | 0.058 / 0.052 |
    | json many-tiny 1 | 153349 / 109535 | 499129088 / 722054720 | 187458504 / 411786184 | 22784 / 32960 | 0.043 / 0.036 |
    | json mixed 16 | 12729 | 464762944 | 72600 | 464762896 | 1.062 / 0.872 |
    | json many-tiny 16 | 2452254 / 1751610 | 7981736448 / 11546613120 | 2997693496 / 6584990776 | 22784 / 32960 | 0.587 / 0.542 |
    | yaml mixed 1 | 752 | 17171408 | 13544 | 17171360 | 0.061 / 0.047 |
    | yaml many-tiny 1 | 180152 / 128680 | 586369024 / 848258560 | 219035856 / 482572496 | 22784 / 32960 | 0.068 / 0.049 |
    | yaml mixed 16 | 11790 | 444904736 | 56032 | 444904688 | 0.997 / 0.918 |
    | yaml many-tiny 16 | 2881011 / 2057865 | 9377279232 / 13565446080 | 3502811688 / 7717319208 | 22784 / 32960 | 1.247 / 0.925 |
    | toml mixed 1 | 826 | 26511944 | 40960 | 26511896 | 0.078 / 0.060 |
    | toml many-tiny 1 | 171255 / 122325 | 557410560 / 806366400 | 153104800 / 403626400 | 22784 / 32960 | 0.064 / 0.047 |
    | toml mixed 16 | 12521 | 429669712 | 113384 | 429669664 | 1.354 / 1.061 |
    | toml many-tiny 16 | 2738295 / 1955925 | 8912759040 / 12893457600 | 2448053152 / 6453787552 | 22784 / 32960 | 0.803 / 0.757 |
    | cbor mixed 1 | 1208 | 38431824 | 26272 | 38431776 | 0.068 / 0.073 |
    | cbor many-tiny 1 | 182427 / 130305 | 593773824 / 858970560 | 222786232 / 489650872 | 22784 / 32960 | 0.041 / 0.043 |
    | cbor mixed 16 | 19418 | 601566920 | 143920 | 601566872 | 1.263 / 1.198 |
    | cbor many-tiny 16 | 2917257 / 2083755 | 9495254784 / 13736112960 | 3562631728 / 7830161968 | 22784 / 32960 | 0.579 / 1.693 |
    | ini mixed 1 | 725 | 26611648 | 12952 | 26611600 | 0.043 / 0.091 |
    | ini many-tiny 1 | 179585 / 128275 | 584523520 / 845588800 | 219504904 / 482212104 | 22784 / 32960 | 0.042 / 0.110 |
    | ini mixed 16 | 11362 | 423135768 | 56824 | 423135720 | 0.803 / 1.774 |
    | ini many-tiny 16 | 2872282 / 2051630 | 9348867584 / 13524344960 | 3510741560 / 7712479800 | 22784 / 32960 | 0.622 / 1.722 |

    A 48-byte JSON pin failed on all three runs. Before the change,
    the allocator trace showed an arena header (48 bytes) and two
    4,128-byte requests already in `cf_build`. The original two-block
    total and second-block size checks both failed. A fixture counting
    from the point where the length becomes known also failed before
    the change. With the one-shot hint, the large-document checks passed;
    the tiny fixture expected three further calls (one hinted block, a
    scratch arena and a scratch block). This exact count failed. The
    benchmark observed five calls per tiny load in total, down from seven.
    The 24 KiB floor raised every tiny load's peak by 10,176 bytes
    (+44.7%). This independently rules out keeping the change.
    Stopped at the third failed run; (1) and (3) have no trials.
  * **Windows trials (1) and (3), 2026-11-23.** Each ran alone against
    `a9848d7` in the same x64 MSVC Release build as step 3. The timed
    builder benchmark used its counting allocator, best of three. A
    fourth pass tracked live requested bytes per configuration and
    checked that the live count returned to zero on release. Peak
    excludes allocator overhead. Slack is final arena capacity minus
    used. Calls, bytes asked and slack are totals over all loads in a row.
    XML was unavailable (Expat not found).

    **(1) Reverted.** The first block kept its original size. Each refill
    took the larger of the request and twice the previous block size,
    capping the doubled size at 256 KiB. The smallest allowed cap limits
    unused tails. Even this cap raised the tiny-document peak from
    22,784 to 28,864 bytes before the cap was reached, so increasing it
    cannot fix that regression. `unit.test_alloc` failed on
    “4 MiB in at most 22 refills” (CTest 8), then passed (0).

    **(3) Reverted.** The trial kept the line estimate when the number
    of equals signs matched the estimated nonzero key count. Otherwise
    it reserved from the input length using the existing bounds of one
    candidate per four bytes and at most 2^20 candidates. The sort
    buffers already take the exact candidate count at resolution.
    `unit.test_keyspace` failed on “compact document fits its initial
    reservation” (CTest 8), then passed alongside `unit.test_predict`
    (0). Peak fell by 110,990,080 bytes on 16 MiB CBOR, but nearly
    doubled on 1 MiB YAML. No mixed 16 MiB load was faster.

    Each cell lists baseline / (1) / (3). Seconds include release.

    | Format, case, MiB | Calls | Bytes asked | Slack (bytes) | Peak (bytes) | Best of 3 (s) |
    | --- | ---: | ---: | ---: | ---: | ---: |
    | json mixed 1 | 812 / 29 / 807 | 29266232 / 30164824 / 34111128 | 18056 / 941704 / 13960 | 29266184 / 30164776 / 34111080 | 0.045 / 0.053 / 0.059 |
    | json many-tiny 1 | 153349 / 109535 / 153349 | 499129088 / 632323648 / 499129088 | 187458504 / 322055112 / 187458504 | 22784 / 28864 / 22784 | 0.037 / 0.040 / 0.038 |
    | json mixed 16 | 12729 / 217 / 12725 | 464762944 / 465757248 / 444117472 | 72600 / 1467288 / 68504 | 464762896 / 465757200 / 444117424 | 0.891 / 0.949 / 1.015 |
    | json many-tiny 16 | 2452254 / 1751610 / 2452254 | 7981736448 / 10111694208 / 7981736448 | 2997693496 / 5150071864 / 2997693496 | 22784 / 28864 / 22784 | 0.607 / 0.650 / 0.637 |
    | yaml mixed 1 | 752 / 24 / 752 | 17171408 / 17780944 / 34158608 | 13544 / 646376 / 13544 | 17171360 / 17780896 / 34158560 | 0.047 / 0.056 / 0.059 |
    | yaml many-tiny 1 | 180152 / 128680 / 180152 | 586369024 / 742843904 / 586369024 | 219035856 / 377157840 / 219035856 | 22784 / 28864 / 22784 | 0.053 / 0.059 / 0.055 |
    | yaml mixed 16 | 11790 / 198 / 11790 | 444904736 / 445309984 / 444904736 | 56032 / 832224 / 56032 | 444904688 / 445309936 / 444904688 | 0.935 / 1.009 / 1.036 |
    | yaml many-tiny 16 | 2881011 / 2057865 / 2881011 | 9377279232 / 11879643072 / 9377279232 | 3502811688 / 6031516200 / 3502811688 | 22784 / 28864 / 22784 | 0.985 / 0.976 / 1.026 |
    | toml mixed 1 | 826 / 38 / 826 | 26511944 / 28173256 / 26511944 | 40960 / 1727488 / 40960 | 26511896 / 28173208 / 26511896 | 0.051 / 0.065 / 0.054 |
    | toml many-tiny 1 | 171255 / 122325 / 171255 | 557410560 / 706157760 / 557410560 | 153104800 / 303417760 / 153104800 | 22784 / 28864 / 22784 | 0.057 / 0.053 / 0.046 |
    | toml mixed 16 | 12521 / 237 / 12521 | 429669712 / 431872464 / 429669712 | 113384 / 2709224 / 113384 | 429669664 / 431872416 / 429669664 | 0.999 / 1.162 / 1.047 |
    | toml many-tiny 16 | 2738295 / 1955925 / 2738295 | 8912759040 / 11291163840 / 8912759040 | 2448053152 / 4851493792 / 2448053152 | 22784 / 28864 / 22784 | 0.788 / 0.768 / 0.758 |
    | cbor mixed 1 | 1208 / 38 / 1200 | 38431824 / 39591504 / 36939216 | 26272 / 1223392 / 18080 | 38431776 / 39591456 / 36939168 | 0.074 / 0.067 / 0.097 |
    | cbor many-tiny 1 | 182427 / 130305 / 182427 | 593773824 / 752224704 / 593773824 | 222786232 / 382905016 / 222786232 | 22784 / 28864 / 22784 | 0.049 / 0.043 / 0.065 |
    | cbor mixed 16 | 19418 / 323 / 19410 | 601566920 / 602223592 / 490576840 | 143920 / 1411632 / 131632 | 601566872 / 602223544 / 490576792 | 1.089 / 1.179 / 1.314 |
    | cbor many-tiny 16 | 2917257 / 2083755 / 2917257 | 9495254784 / 12029100864 / 9495254784 | 3562631728 / 6123149872 / 3562631728 | 22784 / 28864 / 22784 | 0.707 / 0.730 / 0.904 |
    | ini mixed 1 | 725 / 24 / 725 | 26611648 / 27332640 / 26611648 | 12952 / 756376 / 12952 | 26611600 / 27332592 / 26611600 | 0.036 / 0.042 / 0.071 |
    | ini many-tiny 1 | 179585 / 128275 / 179585 | 584523520 / 740505920 / 584523520 | 219504904 / 377129224 / 219504904 | 22784 / 28864 / 22784 | 0.050 / 0.048 / 0.067 |
    | ini mixed 16 | 11362 / 190 / 11362 | 423135768 / 423468440 / 423135768 | 56824 / 747000 / 56824 | 423135720 / 423468392 / 423135720 | 0.711 / 0.790 / 0.849 |
    | ini many-tiny 16 | 2872282 / 2051630 / 2872282 | 9348867584 / 11843649664 / 9348867584 | 3510741560 / 6031784504 / 3510741560 | 22784 / 28864 / 22784 | 0.748 / 0.680 / 0.723 |
  * **Verification after reverting (1) and (3).** All three Windows
    benchmark runs exited 0. The final MSVC builds exited 0. Release
    passed 244/247 and Debug 246/249 with `ctest -E "^gate\."`.
    The three failures in each were the HKCU tests (`unit.test_registry`,
    `example.policy`, `example.policy_cpp`). The full minimal and misra
    preset suites passed 177/181 and 140/143 respectively. Both had the
    known 128-byte diagnostic truncation in `unit.test_yaml` and the
    applicable HKCU failures. All four CTest runs exited 8. The wipe
    test passed in each. In the Release archive, `llvm-nm -u` found
    malloc and free references only in `alloc.c.obj`. The Unix symbol
    gate, external conformance suites and 60-second libFuzzer runs still
    need verification outside the MSVC sandbox.
* **Organiser's answers** (2026-11-23, from the measurement above; each
  is reversible and the maintainer may revisit it):
  * **A candidate (0), first, inside the constraints**: the wipe stays
    for every block, as decided; only its speed changes. `cf_wipe` calls
    `memset` through a `volatile` function pointer
    (`static void* (*const volatile cf_wipe_memset)(void*, int, size_t)
    = memset;` in `alloc.c`), which C89 allows and no optimiser can
    remove, because it cannot know the call's target. The wipe test
    (contents zero after `cf_arena_wipe`, read through the arena before
    release) pins it first; kept by the card's rule (the 16 MiB read and
    `many-tiny` faster beyond noise).
  * **(a) the block-growth cap**: decided by the measurement inside 256
    KiB to 4 MiB as the card says; candidate (2), the first block from
    the input's length, matters most for `many-tiny` (four 4 KiB blocks
    for 48 bytes).
  * **(b) reuse (candidate 4)**: not built: the wipe, not the
    allocation, is the cost of many small documents, and reuse wipes
    too.
  * **(c) mimalloc**: a benchmark comparison only, as recommended, and
    only if time allows: malloc+free is under 1% of a large read, so by
    step 4's rule it is closed with these numbers.
  * **Windows test fixture for (2)**: count from the point the length
    becomes known. The Windows trace found two blocks already allocated
    during builder creation. The next-refill policy takes precedence
    over block numbering.
  * **Candidate (2) when the length comes after the builder** (the
    deputy's question, 2026-11-23): `cf_build(alloc)` puts the builder in
    the arena's first block before any length is known, and that stays
    so (no signature changes, the builder's creation unchanged). The
    length sizes the *next* block: when the builder learns the input's
    length (a document's bytes, or a file source's size), it calls an
    internal `cf_arena_hint(ar, n)` (`CF_INTERNAL_API`, in `alloc.c`)
    with n = 8 × length, and the arena's next new block is the larger of
    the request and n clamped to [24 KiB, `CF_ARENA_HINT_MAX`] (1 MiB
    until candidate (1) sets its cap, then that cap). A hint is used
    once. The pinning test: after a builder loads a 48-byte document,
    the arena has taken at most two blocks from the allocator (today
    four), and a 1 MiB document's second block is the clamped hint's
    size. Then measured and kept or reverted by the card's rule.
