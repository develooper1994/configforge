# Memory and thread safety

Memory and thread errors are the ones found late and fixed at great
cost. Rust rules most of them out when the program is compiled; a C
library has to rule them out another way. This page says what
ConfigForge guarantees, what holds each guarantee up - a design rule, a
gate, a sanitizer, a proof - and what is not yet proven, so that the
claim is never larger than the evidence.

## The design: what cannot go wrong because nothing does it

| Rule | So that | Held by |
| ---- | ------- | ------- |
| **A snapshot never changes.** Every read is of an immutable `cf_config`; a change is a new one (`cf_derive`, a reload). | Readers on any thread need no lock, and no read can see half of a write. | D-14; `test_threads`, `test_live` under ThreadSanitizer |
| **Sharing is counting.** A snapshot is freed when its last holder lets it go; the count is the only thing two threads write. | No use after free across threads, no double free. | `cf_ref_*` on the platform's atomics; ThreadSanitizer |
| **One lock, held for a pointer copy.** `cf_live`'s ticket lock guards the current snapshot and its counters; nothing that allocates or can fail runs under it. | No deadlock (one lock), no livelock (tickets are served in order), no priority inversion worth the name. | `test_live`'s thread test; the lock was a livelocking counter until TinyCC showed it (CHANGELOG) |
| **No atomics, no threads.** A toolchain without atomic operations builds only `CF_SINGLE_THREADED`, and says so in its installed headers. | A build that cannot keep the promise does not make it. | `cf_port.h`'s `#error`; the bindings refuse such a library |
| **Memory comes from arenas.** A configuration owns one; its parts are freed together, never one by one. | No double free, no leak of a part, no dangling pointer between parts of one snapshot. | the allocator interface (D-03); Valgrind; AddressSanitizer |
| **No string borrowed past the call that took it.** Names, paths, prefixes and every string in a definition are copied when handed over; the rest is read only by `create`. | A caller's buffer freed early is never read. | `test_lifetime`, which frees each before `create`, under AddressSanitizer |
| **Bounded buffers, measured first.** Every text a function writes is measure-or-write (`cap` honoured, length returned); the library imports no unbounded copy, no `sprintf` family, no `strtok`, no locale-dependent conversion. | No overflow of a caller's buffer, and a file read the same way on every system. | `gate.symbols`, which reads the built library's imports; `docs/Conventions.md` |
| **Bounded stacks.** No library function's frame exceeds 2048 bytes, at every optimisation level the presets build. | A deep call chain on a small thread stack is still safe. | `-Wstack-usage`, `scripts/stack.cmake`, the `portable` and `minimal` presets |
| **No global mutable state.** | Two configurations, two threads, two libraries in one process never interfere. | `gate.symbols` (no writable globals) |
| **Sized structs.** A struct crossing the boundary carries its size, and the library reads and writes only as far as it reaches. | A program and a library of different 1.x releases never overrun each other. | D-51; `CF_STATIC_ASSERT`; `test_live`'s larger-struct check |

## The tools: what finds what the design missed

| Tool | Finds | Runs |
| ---- | ----- | ---- |
| AddressSanitizer and UndefinedBehaviorSanitizer (bounds included) | out-of-bounds reads and writes, use after free, signed overflow, misaligned access, invalid shifts | the `sanitize` preset, every commit (tier 1) |
| ThreadSanitizer | data races | `verify -DEXTRA=tsan` (tier 2) |
| Valgrind memcheck | reads of memory never written, leaks, on the uninstrumented build that ships | `verify -DEXTRA=valgrind` (tier 2) |
| libFuzzer | inputs no test thought of, against every reader | `verify -DEXTRA=fuzz` (tier 2) |
| GCC `-fanalyzer`, cppcheck, clang-tidy | paths a test never takes | the `analyze` preset (tier 1), `EXTRA=tidy` |
| The compilers' full warning set, as errors | conversions that lose bits, shadowing, missing prototypes | every build |

## Hardening: what the build adds to what ships

The library and Doctor are built with `-fstack-protector-strong`,
`-fstack-clash-protection`, `-fcf-protection=full` and, where the
optimiser runs, `_FORTIFY_SOURCE=3`; linked with full RELRO
(`-z relro -z now`); and on Windows with `/guard:cf`. Each flag is asked
of the compiler first, so a toolchain without one builds without it.
`CF_HARDEN=OFF` turns it off for a build that must not have it.

## The bindings

* **Rust**: `Config`, `LiveReader` are `Send + Sync` because the library's
  snapshots are immutable and counted atomically; the crate refuses a
  single-threaded library at the first call. A text read is a `&str`
  borrowed from its `Config`, so the compiler holds the lifetime. A panic
  in a build, check or log closure is stopped before it reaches C.
* **Python**: ctypes releases the GIL on every call, so Python threads do
  reach the library at once; the package refuses a single-threaded
  library at import. An exception in a build function is a failed build,
  reported with the exception as its cause.
* **C++**: a throw in a `cf::Live` build or check is caught before it
  reaches C.

## Not yet proven

Sanitizers and fuzzing find errors on the paths they run; they do not
prove there are none on the rest. The next step is to prove it for the
code that reads untrusted input: bounded model checking (CBMC) of the
UTF-8 decoder, `cf_fmt` and the number parsers, showing that no input
reads or writes out of bounds or overflows, and of `cf_live`'s ticket
lock, showing that it excludes and always admits. Until those proofs
land, this page claims what the table above holds and no more.
