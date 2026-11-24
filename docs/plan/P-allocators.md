# Allocation: measured first, changed only where it wins

Written 2026-11-22. The maintainer asked whether allocator work could give
a real speed-up. This file answers with what the tree does today, what the
literature says, and one card whose first steps measure (P-4, done: docs/Plan.md, "Done,
their cards removed"; its text and trial tables in `git log -- docs/plan/P-allocators.md`). Nothing here is a
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
