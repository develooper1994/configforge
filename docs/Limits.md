# Limits

## What this page is

What the library does not do and what it will not take, in one place, so
that a limit is learnt here and not from a refusal. Each line is one of
three kinds, and the kind says who keeps it from going stale:

* **Generated**: a number read from the build options or the sources by
  `scripts/generate/limits.cmake` (sections "Build limits", "Fixed
  limits", "The decode budget", the XML row of "The largest document",
  "Platforms" and "C versions"). `gate.limits` regenerates it and fails
  when the page differs.
* **Tested**: a number or rule a test holds, named in a "Held by" cell.
  `unit.test_limits` compares this page with the build; the others are
  tests that already existed. `gate.limits` fails when a test named here
  is not registered.
* **Written**: a sentence about something a build cannot read: a size run
  once on one machine, a rule of design, a feature not built. The gate
  checks only its references (a decision number, a link), never its
  words; each was read against the code or the decision when written.

## Build limits

<!-- limits:2:begin -->

These are the options of "Limits and the stack budget" in the [README](../README.md#limits-and-the-stack-budget) (D-37). Each is a fixed buffer, so each is stack. The default, the range and the check are read from `cmake/ConfigForgeOptions.cmake`, `lib/port/include/configforge/cf_build_options.h` and `lib/port/include/configforge/cf_port.h`, and the build fails if the three disagree.

| Option | Default | Range | What it bounds |
| ------ | ------: | ----- | -------------- |
| `CF_KEY_MAX` | 256 | 32 to 65535 | longest key, terminator included, so one byte less of text; held by every error, the flattener and the decoders that name the path they are in |
| `CF_MESSAGE_MAX` | 256 | 64 to 65535 | longest error message; every `cf_error` holds one, and a builder call keeps one on its frame |
| `CF_PATH_MAX` | 512 | 64 to 65535 | longest file path; the directory source and the platform layer hold two or three at once |
| `CF_MAX_DEPTH` | 32 | 4 to 1024 | deepest nesting in the configuration layer, counting the document as the first level (see the depth rule below) |
| `CF_STACK_BUDGET` | 2048 | 0 to 1048576 | bytes a library function's stack frame may take; `0` measures without checking |

`CF_KEY_MAX`, `CF_MESSAGE_MAX` and `CF_SOURCE_MAX` size `cf_error`'s buffers and so belong to a build's ABI (D-45): `cf_build_limit()` says which numbers a library was built with.

<!-- limits:2:end -->

## Fixed limits

<!-- limits:3:begin -->

These are not build options: each is a constant in the file named, read from it. `CF_PLATFORM_NAME_MAX` is `CF_PATH_MAX` (`cf_platform.h`).

| Name | Value | Read from | What it bounds | Held by |
| ---- | ----: | --------- | -------------- | ------- |
| `CF_SOURCE_MAX` | 64 | `lib/base/include/configforge/cf_error.h` | a source's name in an error; part of the ABI (D-45) | - |
| `CF_MAX_SOURCES` | 32 | `lib/config/include/configforge/cf_explain.h` | the window of `explain`'s summary counters, not a limit (D-230) | - |
| `CF_MAX_OPEN_PREFIXES` | 8 | `lib/config/include/configforge/cf_define.h` | the default of `cf_limit_options.max_open_prefixes` (D-230) | - |
| `CF_EXPAND_MAX` | 1048576 (1 MiB) | `lib/config/include/configforge/cf_define.h` | the bytes one value's `${...}` may expand to (D-77) | - |
| `CF_EXPAND_TOTAL_MAX` | 16777216 (16 MiB) | `lib/config/include/configforge/cf_define.h` | the expansion of a whole configuration | - |
| `CF_DIR_MAX_FILES` | 256 | `lib/config/src/bridge/dir.c` | files in a drop-in directory | `unit.test_dirlimits` |
| `CF_SECRETS_MAX_FILES` | 256 | `lib/config/src/bridge/secrets.c` | files in a secrets directory | `unit.test_dirlimits` |
| `YP_KEY_MAX` | 1024 | `lib/serialize/src/yamlparse.c` | an implicit YAML key (spec 7.4.2) | - |
| `CF_XML_INPUT_MAX` | 2147483647 | `lib/serialize/src/xml.c` | the bytes of one XML document (2 GiB less one) | - |

<!-- limits:3:end -->

## The decode budget and the depth limit

<!-- limits:4:begin -->

One document may make at most `CF_DECODE_FACTOR` times its own size, and `CF_DECODE_FLOOR` more, of keys and values, each key charged `CF_DECODE_KEY_COST` for the records that hold it (D-82). Past that the document is refused by name, whatever its format. It is not a build option: it sizes no buffer.

| Name | Value | Read from | Held by |
| ---- | ----: | --------- | ------- |
| `CF_DECODE_FACTOR` | 64 | `lib/config/include/configforge/cf_flatten.h` | `unit.test_budget`, `unit.test_limits` |
| `CF_DECODE_FLOOR` | 1048576 (1 MiB) | `lib/config/include/configforge/cf_flatten.h` | `unit.test_budget`, `unit.test_limits` |
| `CF_DECODE_KEY_COST` | 64 | `lib/config/include/configforge/cf_flatten.h` | `unit.test_budget`, `unit.test_limits` |

So a document of n bytes may make at most 64 x n + 1 MiB (`cf_decode_budget(n)`):

| A document of | May make at most | Arithmetic |
| ------------- | ---------------: | ---------- |
| 1 KiB | 1.06 MiB | 64 x 1024 + 1048576 = 1114112 bytes |
| 1 MiB | 65 MiB | 64 x 1048576 + 1048576 = 68157440 bytes |
| 256 MiB | 16.0 GiB | 64 x 268435456 + 1048576 = 17180917760 bytes |

The budget is the flattener's. A codec used alone, through `cf_codec_decode` and a sink of the program's, is not under it (D-82).

YAML's aliases are bounded on their own, in `yaml.c`, so that a document of aliases of aliases is refused in linear time (D-63):

| Name | Value | Read from | What it bounds |
| ---- | ----: | --------- | -------------- |
| `YAML_ALIAS_FACTOR` | 64 | `lib/serialize/src/yaml.c` | events an alias may replay per event read |
| `YAML_ALIAS_FLOOR` | 4096 | `lib/serialize/src/yaml.c` | events, the floor of that rule |
| `YAML_ALIAS_BYTES_FACTOR` | 64 | `lib/serialize/src/yaml.c` | bytes of scalar text an alias may replay per byte of the document |
| `YAML_ALIAS_BYTES_FLOOR` | 1048576 (1 MiB) | `lib/serialize/src/yaml.c` | bytes, the floor of that rule |

**The depth rule.** The configuration layer counts the document as level one, so `CF_MAX_DEPTH` 32 allows 31 levels of tables and arrays inside it. `cf_build_limit("depth")` gives the build's limit. See [D-109](Decisions.md) for each codec's nesting limit.

<!-- limits:4:end -->

## The largest document

<!-- limits:5:begin -->

The one limit a reader of its own states in a constant:

| Format | What the reader refuses | Read from |
| ------ | ----------------------- | --------- |
| XML | a document of more than 2147483647 bytes (`CF_XML_INPUT_MAX`; the call to expat takes an `int`) | `lib/serialize/src/xml.c` |

<!-- limits:5:end -->

Beside the reader's own limit, what each format was run at, and what a
position can count. These are written, not generated: a size is run once
on one machine and cannot be run on every build.

| Format | In the code | Run at (P-1, 2026-11-20, commit `90b6541`) | A string |
| ------ | ----------- | ------------------------------------------ | -------- |
| INI | no limit of its own: the document's size (`size_t`), and the decode budget when read through the configuration library | 256 MiB | the document's own bound |
| CBOR | the same | 256 MiB | the document's own bound |
| JSON | the same | 256 MiB | the document's own bound; 255 bytes through a sink without `reserve_text` |
| TOML | the same | 256 MiB | the document's own bound |
| YAML | the same, and the alias bounds above | 256 MiB | the document's own bound |
| XML | `CF_XML_INPUT_MAX`, above | not run: `docs/Performance.md` has no XML row | the document's own bound |

"Run at" is the largest size the large-document benchmark of
[Performance](Performance.md#large-documents) ran, on an Intel Core
i7-10750H at 2.6 GHz, Windows 11, MSVC, and none of the readers refused
256 MiB. It says what was run, not what is promised.

"A string" is the longest string one value may hold. Read through the
configuration library, it is the document's own bound. Read through a
sink of a program's own that has no `reserve_text` (the member of
`cf_doc_sink`), the JSON reader's string buffer is `char local[256]`:
255 bytes, and a longer string is refused (a long string needs sink text storage), not cut. It is
not `CF_MESSAGE_MAX`, which bounds an error's message.

Every text reader reports a position as a line and a column of
`unsigned int` (`cf_loc`, in `cf_error.h`), so on a document of more
than 4,294,967,295 lines, or a line of that many bytes, a reported
position wraps. The document is read correctly; only the number named in
an error is wrong.

## What a format cannot hold

What each format can hold, and what a conversion from one to another
loses, is a table of its own, kept there and not copied here: see
[What each format holds, and what a conversion loses](Formats.md#what-each-format-holds-and-what-a-conversion-loses).

## Platforms

<!-- limits:7:begin -->

Read from `docs/targets.txt` (A-12); how a target is shown is the heading it is under.

### Tested on CI runners

| Target | Note |
| ------ | ---- |
| x86-64 Linux, glibc | job `gates` |
| x86-64 Linux, musl (Alpine) | job `alpine` |
| x86-64 Windows, MSVC | job `matrix` |
| arm64 macOS, Apple clang | job `matrix` |
| x86-64 Linux, tcc and pcc | job `compilers` |

### Run locally

| Target | Note |
| ------ | ---- |
| wasm32-wasi (Node's WASI) | run by scripts/verify.cmake -DEXTRA=wasm, not by CI |

### Not tried

| Target | Note |
| ------ | ---- |
| armv7 Linux | planned (docs/Roadmap.md) |
| Cortex-M | planned (docs/Roadmap.md) |
| s390x Linux (big-endian) | planned (docs/Roadmap.md) |
| riscv64 Linux | planned (docs/Roadmap.md) |

<!-- limits:7:end -->

## C versions

<!-- limits:8:begin -->

Every source file is C89 (D-72, D-106) except the files below, each of which says why in a `cf_source_needs_c` call (scanned from `CMakeLists.txt` and `lib/*/CMakeLists.txt`, not from a build, so the page does not change with the options). Where a library is built alone, `lib/<name>/CMakeLists.txt` repeats the call for the same file.

| File | Standard | Why | Only with |
| ---- | -------- | --- | --------- |
| `lib/serialize/src/yaml.c` | C11 | libfyaml's header needs C11 (0.9.x's pre-C11 fallback for atomics does not compile) | `CF_WITH_YAML` |
| `lib/platform/src/posix/posix.c` | C99 | CoreFoundation's headers use long long (SInt64) | `CF_WITH_PREFERENCES` |

<!-- limits:8:end -->

## Threads

The rule is D-14, and it is written in the public header:

```text
Reads are safe concurrently.
Building and releasing are the caller's to synchronise.
```

* A `cf_config` is an immutable snapshot, so any number of threads may
  read one at once without a lock; sharing one is counting, and the
  count is atomic. A change is a new snapshot (`cf_derive`, a reload).
* Building a configuration, and releasing one, are the caller's to
  synchronise.
* `cf_live` holds the current snapshot under one ticket lock, taken for a
  pointer copy. Its rules are in [Safety](Safety.md#the-design-what-cannot-go-wrong-because-nothing-does-it).
* A toolchain without atomic operations builds only `CF_SINGLE_THREADED`,
  which is the build without atomics, and the library says so in its
  installed headers (`cf_has_feature` reports it). The Rust and Python
  bindings refuse such a library.
* What the bindings promise about threads and callbacks is in
  [Safety](Safety.md#the-bindings).

## Not proven, not claimed

**Not proven.** The floating point parser and printer, the codecs'
tokenizers, and `cf_live`'s ticket lock are covered by the sanitizers,
fuzzing and their tests, and none is claimed as proven: see
[Not yet proven](Safety.md#not-yet-proven).

**Not claimed.** The library is not called "MISRA C compliant" until a
qualified tool has run, and never "certified" to ISO 26262, IEC 61508 or
any process standard without an assessor: see
[What may be said, and when](Compliance.md#what-may-be-said-and-when).

## Not offered

Features that are not built, with the decision that says so. Each row
was read against its decision when it was written.

| Feature | Why not | Decision |
| ------- | ------- | -------- |
| A network or remote key-value source | deferred, a non-goal until something asks | D-30 |
| A database, a secrets platform, service discovery or a distributed configuration service | non-goals of the project; they may integrate through providers | Design.md, section 64 (no D-number) |
| A schema language of its own | JSON Schema is read (`cf_defset_schema`) and written (`cf_schema_write`) instead | D-64, D-67 |
| Writing comments back into a hand-written file | a writer would lose them; rewriting a document while keeping its comments, ordering and spacing is a separate problem | Design.md, section 39 (no D-number) |
| The Windows Registry's `REG_EXPAND_SZ` expanded | expanding it would be a second environment source with no name in `explain()` | D-30 |
| Value interpolation without a bound, or recursive in provenance | `${...}` exists, but an expansion is bounded by `CF_EXPAND_MAX` and `explain()` stays one level deep | D-10, D-77 |
| A document tree (DOM) and a stream reader in the configuration library | its key space is flat; the serialization library gets a DOM, and a stream is 2.0's | Architecture.md, "What is deliberately not here"; D-61 |
| A target with a 16-bit `int`, no 64-bit integer type, or near and far pointers | out of scope: it would need a different value model and memory model | Design.md, section 47 (no D-number) |
| YAML tags that build objects | only the core schema's tags are read; any other tag is refused by name | D-63 |
