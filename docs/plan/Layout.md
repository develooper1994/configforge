# The plan by module and stage (approved by the maintainer, 2026-11-26)

docs/Plan.md, "The plan laid out again", asked for this page; the
maintainer approved it as written, with 2.0.0's breaks early. It replaces
the order of groups in docs/Plan.md's index and the milestone table of
docs/Maintaining.md, "Versions", from 1.9.0 on. Card ids are the index's;
"+" merges cards that overlap; "new" is a card still to be written.
Main is at 1.9.0, unreleased (11 CHANGELOG entries already).

## Stage 0 - the work tools, then the foundations

| # | Card | From |
| - | ---- | ---- |
| 0.1 | `scripts/work/index.py`: docs/Plan.md's index generated from the card files (id, title, version, size, needs, status) | new (L) |
| 0.2 | `gate.ready`: a card opens only when it has no TO DECIDE, no "or" left in "decided", every option with its default, every `file:line` finding its symbol, every need in the index | new (L) + HANDOFF's citation gate, merged |
| 0.3 | Options with defaults (D-220): `cf_*_options` + `_init` as one convention, the `none` platform, `CF_E_CAPABILITY`, the table of features by platform | new + L-18a (Cortex-M `minimal`, the `none` platform's proof) |
| 0.4 | One error model (D-223): `CF_NODISCARD` everywhere and its gate | K-9c |
| 0.5 | `cf_error`: place (file, line, column, byte offset, key), reason, chain of causes, `cf_error_handler`, stop-at-first or collect-all | K-6a + K-6b + Y-28's error part + M-11b (`cf_error` owns its text) |
| 0.6 | The guarantees written and held; allocation faults injected | K-9a, K-9b |
| 0.7 | The file layer (D-224): `cf_file_options`, `cf_file_writer`, `cf_file_replace`, Windows retry as an option | F-1a |
| 0.8 | Around the file layer: replace-if-unchanged, close-on-exec, stable read, Windows share-delete, the kernel lock (D-227) | Y-2, Y-8, Y-5, Y-6, Y-3 |

## The modules, in order (each: module, tool, bindings, documents)

1. **config: what the people who change configuration see** (lib/config, then `configforge`)
   - module: C-2a + C-2b (the open `cf_unit`, D-222), C-8a, C-9, C-11, C-7, C-10a (D-229), F-1b (`cf_emit_file`), Y-42 (include/import, D-228), Y-9, Y-10, Y-1
   - tool: C-1a, C-1b, C-3a, C-3b, C-4a, C-4b, C-6a, C-6b, C-8b, C-10b, Y-34, Y-35, Y-37, Y-38, F-4a
   - bindings: C-4c (Python); Rust and C++ where the module's step adds a call
   - documents: Y-7 (threads and processes), Y-36, G-1a, G-1b, G-2
2. **config: built ahead of time** (`gen build`, D-220's answer where there is no OS)
   - module + tool: Y-24, Y-25, Y-26, Y-27, Y-28 (rest), Y-29; documents: Y-31
3. **serialize: one table, one copy, correct** (lib/serialize)
   - module: X-18, X-19, X-20, X-21, K-7a, K-5a, K-5b, K-8, K-1a, K-1b
   - proofs: K-2a..K-2e, K-3a, K-3b, K-4a..K-4d, L-14a, L-14b
4. **compress** (lib/compress, D-221; optional, OFF by default)
   - module: Y-15a, Y-15b, Y-16, Y-17, Y-18; tool: Y-19; documents: Y-14
5. **serialize: written back** (wave 6) and **config: written back**
   - module: F-3a..F-3h, F-3j, Y-20; config: F-2a, F-2b, Y-4, Y-12; tool: F-3i
6. **serialize: ways to read** (the stream and its modes)
   - M-5..M-9, M-10, M-11a, M-12, Y-33, K-7b, M-14..M-17, M-13 (the list of what breaks)
7. **serialize: more formats** - X-1..X-17
8. **bindings and documents across the library** - M-1..M-4 (Go, .NET), G-3..G-8
9. **speed** (lib/text, lib/lanes, lib/numeric) - P-3, P-7, P-8, P-9, P-10, P-13, P-16, L-16a..c, Y-11

**Running beside the modules** (no design shape; a worker takes one when a module waits on CI): L-11, L-12a..c, L-13, L-15a, L-17, L-19a, L-19b, L-18b, P-5, P-6, P-11, P-12, P-14, M-18, M-20, M-21.

## Interim releases (tags stay the maintainer's)

2.0.0's breaks come early (the maintainer, 2026-11-26): later cards are written on the new shapes.

| Version | Scope |
| ------- | ----- |
| 1.9.0 | what main holds now, plus stage 0.1-0.3 and 0.7-0.8 (nothing that breaks) and the beside-cards done by then |
| 2.0.0 | stage 0.4-0.6 (the error model), module 6's breaks only: M-10, M-11a, M-12, Y-33, K-7b, M-13 |
| 2.1.0 | module 1 (config for the people who change it) |
| 2.2.0 | module 2 (`gen build`) and module 3 (serialize, one table, correct) |
| 2.3.0 | module 4 (compress) and module 5 (written back) |
| 2.4.0 | module 6's additions (the stream, ways to read) |
| 2.5.0 | module 7 (formats) |
| 2.6.0 | module 8 (Go, .NET, "Start here") and the distribution cards M-22, M-24, M-26..M-28, M-31..M-34 |
| 2.7.0 | module 9 (speed) |


## Not now (written once; no session discusses them again)

Parked until the maintainer or a user asks: Y-41-m (simple expressions),
C-9-m (one key split into two), C-10c-m (the report's channel on Windows),
C-10d-m (`http://`, `tcp:`), Y-13-m (a shared image across processes),
Y-21-m (built-in inflate), Y-22-m (xz, lz4), Y-23-m (zstd dictionaries),
Y-30-m (rules across keys), Y-32-m, Y-39-m, Y-40-m (language server),
F-4b (`doctor graph`), G-9 and G-9-m (a fork moved onto the library),
L-15b (libconfig), X-9..X-14 (until their TO DECIDE is answered), the
SIMD backend as a library of its own, the GPU.
Waiting on the maintainer's own access: L-17-m, L-19-m, P-15-m, M-19-m,
M-23-m, M-25-m, M-29-m, M-30-m, and the repository settings.

## Each card's cost

`index.py` (0.1) reads a `cost` line from each card after it lands
(model, effort, turns, share of the 5-hour window); the versions above are
re-cut by those numbers after module 1.
