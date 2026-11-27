# The plan by module and stage (approved by the maintainer, 2026-11-26)

docs/Plan.md, "The plan laid out again", asked for this page; the
maintainer approved it as written, with 2.0.0's breaks early. It replaces
the order of groups in docs/Plan.md's index and the milestone table of
docs/Maintaining.md, "Versions", from 1.9.0 on. Card ids are the index's;
"+" merges cards that overlap; "new" is a card still to be written.
1.9.0 was released on 2026-11-27; main opens 2.0.0 with M-10.

## Stage 0 - the work tools, then the foundations

| # | Card | From | Version |
| - | ---- | ---- | ------- |
| 0.1 | L-20 `scripts/work/index.py`: docs/Plan.md's index generated from the card files (id, title, version, size, needs, status) | new | 1.9.0 |
| 0.2 | L-21 `scripts/check/ready.py`: a card opens only when it has no TO DECIDE, no "or" left in "decided", every option with its default, every `file:line` finding its symbol, every need in the index | new + HANDOFF's citation gate, merged | 1.9.0 |
| 0.2b | L-22 CI in two tiers: a fast core on every push, the whole suite once a card | new (the maintainer, 2026-11-27) | 1.9.0 |
| 0.2c | L-23 A release's notes say how each archive was built | new (the maintainer, 2026-11-27) | 1.9.0 |
| 0.2d | L-24 The Rust crate published to crates.io at a tag | new (the maintainer, 2026-11-27) | 1.9.0 |
| 0.3 | Options with defaults (D-220): `cf_*_options` + `_init` as one convention, the `none` platform, `CF_E_CAPABILITY`, the table of features by platform | K-11a (options), K-11b (the `none` platform built and tried, needs L-18a), K-11c (the table and `gate.platforms`) | 1.9.0 |
| 0.7 | The file layer (D-224): `cf_file_options`, `cf_file_writer`, `cf_file_replace`, Windows retry as an option | F-1a | 1.9.0 |
| 0.8 | Around the file layer: replace-if-unchanged, close-on-exec, stable read, Windows share-delete, the kernel lock (D-227) | Y-2, Y-8, Y-5, Y-6, Y-3 | 1.9.0 |
| 0.4 | One error model (D-223): `CF_NODISCARD` everywhere and its gate | K-9c | 1.9.0 |
| 0.5 | `cf_error`: place (file, line, column, byte offset, key), reason, chain of causes, `cf_error_handler`, stop-at-first or collect-all | K-6a + K-6b; the collect-all option `gen build --json` needs (`cf_error` owning its text is in row 0.9: it needs the sizing modes, and `cf_error` still changes shape once in the 2.0.0 release) | 2.0.0 |
| 0.6 | The guarantees written and held; allocation faults injected | K-9a, K-9d, K-9e, K-9f, K-9g, K-9b (K-9a: the three words, `gate.guarantees` and the rule that the library never ends the program; K-9d to K-9g: the guarantees written header by header, 309 names; K-9b: the allocation sweep over every load and write path) | 2.0.0 |
| 0.9 | The sizing modes and what 2.0 breaks, right after the error model (the maintainer, 2026-11-27: long awaited) | M-10, M-11a, M-11b, M-12, M-13 (the stream is 2.4.0's and additive; the manifest in the image and the ways in the formats' table went to their modules, and M-12's header reserves the manifest's place; M-11b is `cf_error` owning its text) | 2.0.0 |

## The modules, in order (each: module, tool, bindings, documents)

`{X.Y.Z}` gives the version of every card named after it on that line
(and on the lines under it, to the next tag); stage 0's is its table's
last column; the "not now" section's cards are `not now`.

1. {2.1.0} **config: what the people who change configuration see** (lib/config, then `configforge`)
   - module: C-2a + C-2b (the open `cf_unit`, D-222), C-8a, C-9, C-11, C-7, C-10a (D-229), F-1b (`cf_emit_file`), Y-42 (include/import, D-228), Y-9, Y-10, Y-1
   - tool: C-1a, C-1b, C-3a, C-3b, C-4a, C-4b, C-6a, C-6b, C-8b, C-10b, Y-34, Y-35, Y-37, Y-38, F-4a
   - bindings: C-4c (Python); Rust and C++ where the module's step adds a call
   - documents: Y-7 (threads and processes), Y-36, G-1a, G-1b, G-2
   - opened 2026-11-27 (the maintainer's standing approval): C-9-m (one key split into two), C-10c-m (the report's channel on Windows), C-10d-m (`http://`, `tcp:`), F-4b (`doctor graph`), Y-41-m (simple expressions)
2. {2.2.0} **config: built ahead of time** (`gen build`, D-220's answer where there is no OS)
   - digests first (Y-29's manifest needs them): M-32, M-33, M-34
   - module + tool: Y-24, Y-25, Y-26, Y-27, Y-28 (its error part goes to 0.5), Y-29, Y-33 (the manifest in the image, in the header M-12 reserves for it); documents: Y-31; opened 2026-11-27: Y-30-m (rules across keys), Y-32-m (the build file)
   - {2.3.0} the contract's and the tool's later additions, opened 2026-11-27: Y-39-m (`pattern`, relations), Y-40-m (`gen hover`)
3. {2.2.0} **serialize: one table, one copy, correct** (lib/serialize)
   - module: X-18, X-19, X-20, X-21, K-7a, K-5a, K-5b, K-8, K-1a, K-1b, K-10a, K-10b (a number's text kept; underflow and infinity as options, defaults unchanged), K-10c-m (numbers beyond 64 bits or `double` accepted as text; opened 2026-11-27)
   - proofs: K-2a, K-2b, K-2c, K-2d, K-2e, K-3a, K-3b, K-4a, K-4b, K-4c, K-4d, L-14a, L-14b
4. {2.3.0} **compress** (lib/compress, D-221; optional, OFF by default)
   - module: Y-15a (`lib/compress`: the two interfaces, their options, the one-call shortcuts), Y-15b (streams and the chain), Y-16, Y-17, Y-18, Y-21-m (built-in inflate), Y-22-m (xz, lz4), Y-23-m (zstd dictionaries; opened 2026-11-27); tool: Y-19; documents: Y-14
5. {2.3.0} **serialize: written back** (wave 6) and **config: written back**
   - module: F-3a, F-3b, F-3c, F-3d, F-3e, F-3f, F-3g, F-3h, F-3j, Y-20; config: F-2a, F-2b, Y-4, Y-12; tool: F-3i
6. **serialize: ways to read** (the stream and its modes)
   - the breaks moved to stage 0, row 0.9 (2.0.0)
   - {2.4.0} the additions: M-5, M-6, M-7, M-8, M-9, M-14, M-15, M-16, K-7b (the ways in the formats' table), M-17, Y-13-m (a shared image across processes; opened 2026-11-27)
7. {2.5.0} **serialize: more formats** - X-1, X-2, X-3, X-4, X-5, X-6, X-7, X-8, X-9, X-10, X-11, X-12, X-13, X-14, X-15, X-16, X-17
8. {2.6.0} **bindings and documents across the library** - M-1, M-2, M-3, M-4 (Go, .NET), G-3, G-4, G-5, G-6, G-7, G-8
   - {2.3.0} G-9 (a fork moved onto the library; opened 2026-11-27)
   - {2.6.0} distribution: M-22, M-24, M-26, M-27, M-28, M-31
9. {2.7.0} **speed** (lib/text, lib/lanes, lib/numeric) - P-3, P-7, P-8, P-9, P-10, P-13, P-16, L-16a, L-16b, L-16c, Y-11

**Running beside the modules** {2.0.0} (no design shape; a worker takes one when a module waits on CI; a beside-card ships in whichever release is open when it lands, and a release never waits for one: at each tag the beside-cards still open move to the next version, 2026-11-27 after 1.9.0): L-11, L-12a, L-12b, L-12c, L-13, L-15a, L-15b (libconfig in the benchmark; opened 2026-11-27), L-17, L-19a, L-19b, L-18b, P-5, P-6, P-11, P-12, P-14, M-18, M-20, M-21, K-9h, K-9i (the safety inventory, 2026-11-27).

## Interim releases (tags stay the maintainer's)

2.0.0's breaks come early (the maintainer, 2026-11-26): later cards are written on the new shapes.

| Version | Scope |
| ------- | ----- |
| 1.9.0 | what main holds now, plus stage 0.1-0.3 and 0.7-0.8 (nothing that breaks) and the beside-cards done by then |
| 2.0.0 | stage 0.4-0.6 (the error model), module 6's breaks only: M-10, M-11a, M-11b, M-12, M-13 |
| 2.1.0 | module 1 (config for the people who change it) |
| 2.2.0 | module 2 (`gen build`) and module 3 (serialize, one table, correct) |
| 2.3.0 | module 4 (compress) and module 5 (written back) |
| 2.4.0 | module 6's additions (the stream, ways to read) |
| 2.5.0 | module 7 (formats) |
| 2.6.0 | module 8 (Go, .NET, "Start here", distribution) |
| 2.7.0 | module 9 (speed) |

## Waits on the maintainer's own access (written once; no session discusses them again)

Each card named here has the status `parked: not now`; its version is the
one of an earlier mention above, or else the one it had. They need the
maintainer's own accounts, repository settings or hardware: L-17-m, L-19-m,
P-15-m, M-19-m, M-23-m, M-25-m, M-29-m, M-30-m, G-9-m. Everything else that
was parked opened on 2026-11-27 (the maintainer: no TO DECIDE and no parked
may remain). The SIMD backend as a library of its own and the GPU are not
cards.

## Each card's cost

`index.py` (0.1) reads a `cost` line from each card after it lands
(model, effort, turns, share of the 5-hour window); the versions above are
re-cut by those numbers after module 1.
