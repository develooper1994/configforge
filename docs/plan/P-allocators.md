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
    Windows (MSVC) figures: the deputy's run, to be added here.
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
