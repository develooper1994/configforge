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

## P-1 - The benchmark measures large documents, and says where time goes

* **id**: P-1
* **goal**: `bench/` reads JSON, YAML, TOML, CBOR and INI documents of
  1, 16 and 256 MiB and prints MiB/s per format and size, and a profile
  run names the functions that take the time.
* **why**: the benchmark measures configurations of thousands of keys;
  a format library is also handed megabytes of data, and no one knows
  today where that time goes. SWAR is worth writing only where a scan
  is the cost.
* **needs**: nothing (the serialization library, S-4, is on main)
* **size**: medium
* **decided**:
  * The documents are generated by `bench.c` itself, deterministic (a
    fixed seed), so no large file is committed: long strings with and
    without escapes, deep and wide nesting, many small numbers, and
    non-ASCII UTF-8 in a share of the strings. Each format is read with
    the public codec API (`cf_codec_*`), the way a program would.
  * Output: one line per format and size, `format size MiB/s`, medians
    as the bench already takes them. The baseline is taken at `-O2`
    with GCC and Clang (and MSVC from CI), and once at `-O3` for the
    record; the compiler's vectorization report for the top scans is
    kept with the profile. The 256 MiB case runs only with
    `--large`, so CI's bench run stays short.
  * The profile: `perf record` on Linux (WSL here), its top functions
    for each format written into the card's notes; the numbers before
    any change go into `docs/Performance.md` (new, or the Roadmap's
    performance section if one exists) with the machine and compiler.
* **files**: `bench/bench.c`, `bench/CMakeLists.txt`; last:
  `docs/Performance.md` (or the existing performance page), the card's
  notes.
* **steps**:
  1. Read `bench/bench.c` and how CI runs it; write the generators.
  2. Run Release (`-O2`) and RelWithDebInfo under `perf`; record.
  3. Write the baseline numbers and the top scans per format.
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI; the bench runs
  in CI as part of the build (CI builds `bench/` and does not run it
  today; P-1b adds the run), without `--large`.
* **stop if**: a format cannot read 256 MiB within its own limits (the
  decode budget, D-82): record the limit, run that format at the
  largest size it accepts, and say so.
* **docs**: the baseline page; Roadmap: "Large files" goes 🟨, "measured
  (P-1)".
* **notes**:
  * 2026-11-20: built as `cf_bench docs [--large] [format ...]`; the
    baseline (MSVC Release, this machine) is in `docs/Performance.md`.
    All five formats read 256 MiB: the documents go through
    `cf_codec_decode` alone, where D-82's bound (the flattener's) is not
    in the way, and no reader refused one.
  * Not done here, for want of Linux tools: the `perf` profile, the
    GCC and Clang baselines at `-O2` and `-O3`, and the vectorization
    reports. The commands are in `docs/Performance.md`; its table of top
    functions waits for them. P-3 must not start before it is filled.
  * The card says CI builds `bench/`; it does not (`CF_BUILD_BENCH` is
    off in every preset and no workflow turns it on). The CI step that
    builds and runs `cf_bench docs` (without `--large`) is the file
    `.github/workflows/ci.yml`, left to P-1b.

---

## P-1b - The other libraries beside ours, and a page CI keeps

* **id**: P-1b
* **goal**: `bench/compare/` reads the same generated documents through
  ConfigForge and through yyjson, simdjson, RapidJSON, cJSON, libyaml,
  rapidyaml, tomlc17, toml++, tinycbor, expat and inih, under one set
  of flags and the same work, and reports speed, instructions, peak
  memory and code size; CI fails a change that makes a format's
  16 MiB read more than 10% slower in instructions, appends every
  push's numbers to a history on the branch `bench-history`, runs the
  full comparison weekly, and `docs/Performance.md` draws each format's
  line over time with the commit of each point, losses shown first.
* **why**: D-111, and the Roadmap's "The aim: beat the others at
  everything, and say where it does", "Benchmarks fair, and without
  mercy" and "A live optimization and benchmark page". A reader that is
  faster than itself last month is not shown to be fast; one that is
  faster than what a user would otherwise pick is. P-2, P-3 and the
  SWAR and SIMD backend (2.4.0) are held to the numbers this card
  produces ("What keeps a gain", below).
* **needs**: P-1, A-1b (the documents and the 16 and 256 MiB runs are
  P-1's; this card adds to them, it does not redo them). `bench/`,
  `scripts/` and `.github/` do not move with A-1, but the paths below
  that name `lib/` and the profile files are the ones after A-1b and A-11.
* **size**: large
* **decided**:
  * **Two programs, not one.** `bench/bench.c` stays C89 and is
    ConfigForge measured against itself (P-1's `cf_bench`, with
    `-DCF_INTERNAL_API=1`). The comparison is C++17 (simdjson, toml++
    and rapidyaml need it) and is its own CMake project,
    `bench/compare/`, which the top-level `CMakeLists.txt` never adds:
    `CF_BUILD_BENCH` (the `option(CF_BUILD_BENCH ...)` block of CMakeLists.txt) is untouched. Nothing in
    the default build, the gates' build or `scripts/verify.cmake` can
    reach a library fetched for the comparison.
  * **One more mode in `cf_bench`: the documents as files.**
    `cf_bench --emit-docs <dir> [--size 1|16|256] [--only <format>]`
    writes P-1's generated documents and the ones below as
    `<dir>/<format>-<doc>-<size>.<ext>` (`.json .yaml .toml .cbor .ini
    .xml`; `many-tiny` is a pair, `.bin` and `.idx`) and prints one
    line a file, `<name> <bytes> <fnv1a64 in 16 hex digits>`. The
    comparison reads these files and generates nothing itself, so every
    library, and every compiler, is given the same bytes. P-1's
    generators are wrapped, not rewritten: one function
    `static unsigned char* bench_doc(int format, int doc, size_t size,
    size_t* len)` over them, which `--emit-docs` calls. The seed is one
    constant, `0x9E3779B97F4A7C15`, run through xorshift64* and mixed
    with the document's number, so a document does not change when
    another is added.
  * **The documents** ("without mercy": each is chosen to hurt a
    reader). `J` JSON, `Y` YAML, `T` TOML, `C` CBOR, `I` INI, `X` XML;
    a letter means the document exists in that format, a blank that the
    format cannot say it. Sizes 1, 16 and 256 MiB, except where noted.

    | Name | What it is | J | Y | T | C | I | X |
    | ---- | ---------- | - | - | - | - | - | - |
    | `mixed` | tables of strings, integers, reals, booleans and short arrays, as a configuration looks | J | Y | T | C | I | X |
    | `strings-plain` | strings of 64 to 4096 ASCII bytes, no escape | J | Y | T | C | I | X |
    | `strings-escape` | every byte of every string an escape: `\n`, `\\`, `\"`, `é`, a surrogate pair; entities in XML | J | Y | T | | | X |
    | `strings-utf8` | half the bytes non-ASCII, two-, three- and four-byte sequences mixed | J | Y | T | C | I | X |
    | `numbers-int` | integers of one to three digits, one a line or element | J | Y | T | C | | |
    | `numbers-edge` | the int64 limits and their neighbours, reals of 17 significant digits, the largest and smallest exponents, subnormals | J | Y | T | C | | |
    | `nest-deep` | tables or arrays 31 levels deep (`CF_MAX_DEPTH` 32 allows 31 inside the document), repeated | J | Y | T | C | | X |
    | `array-huge` | one array of a million or more one-digit elements (INI: `name[] = x` lines, D-60) | J | Y | T | C | I | X |
    | `table-wide` | one table of 100,000 or more unique short keys | J | Y | T | C | I | X |
    | `keys-prefix` | 100,000 keys sharing a 200-byte prefix (under `CF_KEY_MAX`): quadratic time on crafted keys | J | Y | T | C | I | |
    | `whitespace` | mostly blanks, newlines, indentation and comments, a value now and then | J | Y | T | | I | X |
    | `many-tiny` | 16 MiB in all of documents of 20 to 64 bytes, parsed one at a time from their own slice; measures what one parse costs to start and to end | J | Y | T | C | I | X |
    | `invalid-late` | a valid document with its one error in the last KiB: a stray `]` (JSON), a tab indent (YAML), a duplicate key (TOML), a truncated last item (CBOR), a line with no `=` (INI), a mismatched end tag (XML); measures the time to refuse | J | Y | T | C | I | X |

    YAML's alias bombs are not a speed row: libraries do different work
    with an alias (rapidyaml keeps a reference, ConfigForge expands it
    under D-63 and D-82's bound). They are an outcome table on the
    page, one row a library: refused or read, and in how many
    milliseconds, for `yaml-aliases` (a document of aliases of aliases
    nine levels deep) at 1 MiB, written by `--emit-docs` with the rest.
    `bench/docs-1mib.fnv` (new, committed) holds the name and FNV-1a
    of every 1 MiB document; the check `bench.docs` (step 2) regenerates
    them and fails when one differs, so a generator that drifts, or
    differs between compilers, is seen.
  * **The libraries, first round**, and why these. JSON: yyjson, simdjson
    (two rows, `simdjson-dom` and `simdjson-ondemand`: the second is its
    fastest and it is lazy, so the adapter touches every value, which
    makes the work equal), RapidJSON (the most used C++ reader), cJSON
    (the most embedded C one). Not json-c or Jansson: a fifth and sixth
    DOM add rows, not information. YAML: libyaml (the one nearly every
    C program links) and rapidyaml (the fastest known). libfyaml, which
    ConfigForge can use as a backend (D-62), is left for the second
    round: it is installed from the package, not pinned. TOML: tomlc17
    (tomlc99's successor, C) and toml++ (the C++ standard). CBOR:
    tinycbor (C, streaming); libcbor builds a tree, a different
    interface, second round. XML: expat, and **the XML row cannot be a
    win**: ConfigForge's XML reader is expat (D-23) with the layer's own
    handlers and flattening on top, so its line is expat's plus what
    the layer costs, and the page says "overhead of the layer" and not
    "behind". INI: inih. Not compared, and said so on the page: JSON5,
    JSONC, `.properties`, dotenv and TLV (no library found whose job is
    the same), and pugixml, a DOM for XML (second round). The Roadmap's
    older item "Measured against the libraries a program would
    otherwise use" (lookup time, libconfig) is another comparison, of
    whole configuration libraries, and is not this card's.
  * **Versions and how they are fetched.** `bench/compare/libraries.cmake`
    (new) holds, per library, `LIB_<name>_URL`, `LIB_<name>_TAG` and
    `LIB_<name>_COMMIT`. A release tag is named where there is one;
    the commit is what is fetched and is checked against the tag.
    Floors, to be replaced in step 1 by the newest release tag at least
    30 days old on the day the card is carried out: yyjson 0.10.0,
    simdjson 3.10.1, cJSON 1.7.18, libyaml 0.2.5, rapidyaml 0.7.2
    (with its submodule c4core, fetched with
    `--recurse-submodules --depth 1`), toml++ 3.4.0, tinycbor 0.6.0,
    inih r58. tomlc17 and RapidJSON have no usable tag (RapidJSON's
    last release is from 2016): a commit of the default branch at
    least 30 days old, written with its date. Never a branch name,
    never "latest". expat is not fetched: it is the system's
    (`libexpat1-dev`, which ConfigForge's XML reader links too), so the
    two XML rows run one library; its version is recorded. A version
    changes only by a commit that also reruns the baseline.
  * **Fetching is not FetchContent, and not the build's.**
    `scripts/bench/fetch.cmake` (new, `cmake -DLIBS=<dir> -P
    scripts/bench/fetch.cmake`) fetches each library the way
    `scripts/verify.cmake:893-957` fetches the conformance suites:
    `git init`, `git fetch --depth 1 <url> <commit>`, `git checkout
    FETCH_HEAD`, `core.autocrlf=false`, skipped when `HEAD` is already
    the commit. `bench/compare/CMakeLists.txt` never fetches: given
    `-DCF_BENCH_LIBS=<dir>` it builds the adapters whose directory is
    there, and for a missing one it stops with
    `bench/compare: library <name> is not in <dir>: run cmake
    -DLIBS=<dir> -P scripts/bench/fetch.cmake`. Without
    `CF_BENCH_LIBS` it builds the ConfigForge adapter alone, which is
    what the per-push job runs. Rejected: FetchContent (it fetches at
    configure time, which a developer's `cmake -S bench/compare` would
    do without asking, and its `MakeAvailable` brings in each library's
    own flags); git submodules (they put eleven libraries into every
    clone of a project whose default build has no dependency).
  * **The same flags: each library's sources are compiled by us.**
    `bench/compare/libs/<name>.cmake` (new, one a library) lists that
    library's source files and the `-D` definitions they need, copied
    literally from its own `CMakeLists.txt` at the pinned commit
    (its `add_library` sources and `target_compile_definitions`; no
    flag of its own, no `-O`, no `-march`), and makes a static library
    `lib_<name>` of them. So every library is built by this project's
    compiler with this project's flags, and by nothing else.
    ConfigForge is **not** `add_subdirectory`'d: the root project uses
    `CMAKE_SOURCE_DIR` for its templates, scripts and install files
    (cmake/ConfigForgeOptions.cmake, ConfigForgeInstall.cmake,
    ConfigForgeSbom.cmake, ConfigForgeCompiler.cmake), which inside
    another project is that project's directory, so the configure would
    stop. It is configured on its own, by `bench/compare/CMakeLists.txt`
    through `execute_process` at configure time (or by the driver
    before it): `cmake -S <root> -B <bench build>/configforge
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_FLAGS_RELEASE=
    -DCMAKE_C_FLAGS=<CF_BENCH_FLAGS> -DCF_BUILD_TESTING=OFF
    -DCF_BUILD_EXAMPLES=OFF -DCF_BUILD_TOOLS=OFF -DCF_ENABLE_GATES=OFF
    -DCF_WITH_XML=ON -DCF_YAML_BACKEND=builtin` (its own YAML reader,
    D-69, is the one a default build ships), built with `--target
    configforge_serialize` (D-90: the readers, without the
    configuration library); the adapter links that build's
    `libconfigforge_serialize.a` and includes its
    `staged/include` (A-1's staged headers). The size rows' `minimal` is
    `-DCF_PROFILE=minimal` on the same command (the profile, A-11, in
    place of the preset's old list of options). **`-O2` is what every
    library gets**: CMake's Release adds `-O3 -DNDEBUG` after
    `CMAKE_C_FLAGS`, so `bench/compare/CMakeLists.txt` empties
    `CMAKE_C_FLAGS_RELEASE` and `CMAKE_CXX_FLAGS_RELEASE` and applies
    `CF_BENCH_FLAGS` to every `lib_*` and every adapter as
    `target_compile_options`; ConfigForge's own configure gets the same
    through the two `-D`s above. The flag sets, `CF_BENCH_FLAGS`, one
    variable:

    | Name | Flags, for every library alike |
    | ---- | ------------------------------ |
    | `shipped` (the headline) | `-O2 -fstack-protector-strong -fstack-clash-protection -fcf-protection=full -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3`, the hardening ConfigForge ships with (`CF_HARDEN`, cmake/ConfigForgeCompiler.cmake:244-297); ConfigForge built with `CF_HARDEN=ON` |
    | `bare` | `-O2` alone; ConfigForge with `CF_HARDEN=OFF` |
    | `native` | `-O3 -march=native`, for the record; never a headline |

    The libraries' sources are the same in all three. Compilers: GCC and
    Clang, the runner's own, one build each; the headline is GCC's.
    MSVC is not in the comparison (P-1's baseline has it). x86-64
    Linux only; macOS and arm64 are added when a runner for each is in
    CI.
  * **Each at its recommended settings, written down.** One row of the
    page's "The libraries" table a library: its version, the
    settings, and what each gets. yyjson: `yyjson_read` with no flag
    (it validates structure and UTF-8; reads a copy). simdjson: the
    runtime-dispatched kernel it picks, padded input, `dom::parser`
    reused between reps (its documented fast path), On-Demand likewise.
    RapidJSON: `Document::Parse` with `kParseValidateEncodingFlag` (the
    headline: ConfigForge refuses invalid UTF-8 from A-5 on, D-108) and
    a second row, `rapidjson-nocheck`, with its default flags
    (`kParseDefaultFlags`, which does not check encoding); no SSE
    define in `shipped` (a generic target), `RAPIDJSON_SSE42` in
    `native`. cJSON: `cJSON_ParseWithLength`, with
    `cJSON_ParseWithLengthOpts(..., require_null_terminated = 1)`.
    libyaml: `yaml_parser_parse` event loop over `yaml_parser_set_input_string`.
    rapidyaml: `ryml::parse_in_arena` (a copy; `parse_in_place`
    destroys its input and is the in-situ row of a later card).
    tomlc17: `toml_parse`. toml++: `toml::parse` with `TOML_EXCEPTIONS=0`,
    `TOML_HEADER_ONLY=1`. tinycbor: `cbor_parser_init` and the
    iteration calls, with `cbor_value_validate` and every
    `CborValidate*` flag whose check ConfigForge's CBOR reader also
    makes (set by the probe table, below). expat: `XML_Parse` in one
    call, handlers for start, end and character data. inih: `ini_parse_string`
    with `INI_STOP_ON_FIRST_ERROR 1`, `INI_USE_STACK 0` and
    `INI_MAX_LINE 8192` (its default of 200 bytes would refuse the
    long-line documents), INI documents having no line over 4096 bytes.
    ConfigForge: `cf_codec_by_name(<format>)` and `cf_codec_decode`
    (lib/serialize/include/configforge/cf_serialize.h:233 and :245) into a
    `cf_doc_sink` whose callbacks touch the value and which supplies
    `reserve_text` (:146; without it the JSON and TOML readers refuse a
    string over 255 bytes) from one buffer kept between documents, and
    in strict mode `reserve_work` (:155; it turns on TOML's duplicate
    check) from an arena reset between documents.
  * **The same work: parse and touch every value, in one fold.** Every
    adapter walks what its library gave and fills one
    `struct Touch { u64 containers, strings, string_bytes, numbers,
    bools, nulls, acc; }`: a table or array is one container; a string,
    key or value, adds one to `strings` and its length to
    `string_bytes`; a number adds one to `numbers` and its value is
    read into a 64-bit integer or a double and folded into `acc` (this
    keeps the conversion in the loop, and `acc` is never compared, only
    printed, so no compiler drops the loop). A format whose values are
    all text (INI, `.properties`) counts every value as a string. The
    check `bench.touch` (in `cfb_compare --mode probe`, step 6): for
    every document of a format at 1 MiB, every adapter of that format
    gives the same `containers`, `strings`, `string_bytes`, `numbers`,
    `bools` and `nulls`, or the run fails and names the first
    counter that differs. Where a library cannot tell the cases
    (cJSON has one number type), the adapter counts them as one and
    the counter compared is `numbers`.
  * **Validation, on or off for both, and shown.** Every adapter has two
    modes, chosen by `--strict` (default) and `--loose`: strict turns
    on every check the library offers that ConfigForge makes; loose is
    the library's own default. The headline rows are strict.
    `cfb_compare --mode probe` feeds each adapter the same eight probe
    documents per format and prints which it refuses: invalid UTF-8 in
    a string; a duplicate key; an integer past int64; a truncated
    document; text after the root value; nesting 100 deep; a lone
    surrogate escape; a NUL inside a string. The table is
    `bench/compare/probes.expected` (committed, one line a
    library, format and mode); `bench.probes` fails when the actual
    output differs from it, so a library version, a flag or an adapter
    that changes what is checked is seen in review. On the page, next
    to each library's speed rows, the probes ConfigForge refuses and
    that library does not are printed as "does not check: ...": the
    gap is shown, not hidden.
  * **What is measured, per cell** (`cfb_compare --lib <name> --doc
    <file> --mode speed|count|memory|probe|size`, one JSON object on
    one line):
    * **speed**: MiB/s, the median of `--reps` runs (9 up to 16 MiB, 3
      at 256 MiB) after one warm-up, each rep of at least 200 ms for a
      small document (repeated inside the rep), the input one `fread`
      into one `malloc`ed buffer with 64 bytes of padding for every
      library alike (simdjson needs it; the rest ignore it); the
      minimum is kept beside the median (`bench.c`'s `fastest`).
      `clock_gettime(CLOCK_MONOTONIC)`. Pinned: `taskset -c 1` where
      `taskset` exists. `--cold` (weekly only, 256 MiB, needs `sudo`)
      drops the page cache between reps and reads the file each time.
    * **count**: instructions, for one parse, under `valgrind
      --tool=callgrind --collect-atstart=no --toggle-collect=cfb_cell
      --callgrind-out-file=<f>`, read from the `summary:` line of the
      file; `cfb_cell` is the one `noinline` function that parses once.
      The file is read, and the buffer built, outside it.
    * **memory**: peak growth in KiB, `getrusage` `ru_maxrss` after
      the parse less its value just after the input was loaded, one
      process a cell (the input stays live, so what is counted is what
      the library asks for beyond it).
    * **size**: bytes a library adds to a program. `bench/compare/size/`
      builds, per library, a program that parses one fixed 64-byte
      document and touches it, with `-ffunction-sections
      -fdata-sections -Wl,--gc-sections`, stripped, linked dynamically
      to the system's C and C++ libraries; the result is its file size
      less that of the same program with an empty adapter. ConfigForge
      is two rows, the default build and `-DCF_PROFILE=minimal` (the
      profile of A-11, which carries `CF_GC_SECTIONS` and the other
      options the `minimal` preset used to list).
    * **machine**: `uname -sr`, the CPU's name from `/proc/cpuinfo`, the
      logical CPU count, the compiler's first `--version` line, the C
      library's, the runner image (`ImageOS`, `ImageVersion`), and the
      governor from `/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor`
      or `unknown`: a hosted runner cannot fix its frequency, and the
      record says so rather than claims it.
    Not measured in this round, and written on the page: one thread
    against every thread (no library here parses in parallel; the
    Roadmap's threads wave), printing speed (a card for the writers),
    and in-situ and lazy reading as modes of their own ("Ways to read").
  * **The driver.** `scripts/bench/run.py` (new, Python 3 standard
    library only) takes `--scope quick|full`, `--docs <dir>`,
    `--build <dir>`, `--sizes 1,16,256`, `--out <file>`, runs the
    cells and writes one JSON file, schema version 1:
    `{"schema":1,"kind":"quick"|"full","commit":"<40 hex>",
    "committed_at":"<UTC>","run_id":"<n>","machine":{...},
    "compiler":{"id":"gcc","version":"...","flags":"shipped"},
    "libraries":[{"name","version","commit"}],
    "cells":[{"lib","format","doc","size_mib","mode",...}],
    "sizes":[{"lib","bytes_added"}],"probes":[...]}`. `quick` is
    ConfigForge only: the five formats' `mixed`, `strings-plain`,
    `strings-escape`, `whitespace` and `numbers-edge` documents (where
    the format has them, the table above) at 16 MiB, speed (9 reps) and
    count; `mixed` at 1 MiB, speed. `full` is every library, every
    document, every size asked for, in modes speed and count at 1 MiB,
    speed at 16 and 256 MiB, memory at 16 MiB, size once, probes once;
    `shipped` on GCC and on Clang, and `bare` and `native` on GCC for
    `mixed` at 16 MiB alone.
  * **The gate: instructions, 10%.** `scripts/bench/gate.py <results>
    bench/baseline/instructions.json` fails when any gated cell of a
    `quick` run has more than 110% of its baseline's instructions. The
    baseline, `bench/baseline/instructions.json` (new, committed), is
    `{"schema":1,"commit":"<40 hex>","toolchain":{"compiler":"<first
    `--version` line>","libc":"<first `ldd --version` line>"},
    "cells":{"json/mixed/16":<instructions>, ...}}`, written by
    `scripts/bench/gate.py --write <results> bench/baseline/instructions.json`
    on the CI runner's own output (taken from the CI artifact of the
    commit that adds it, not from a laptop). The gated cells are the
    `quick` run's 16 MiB ones. Messages, exactly:
    `bench gate: json/mixed/16: 5123456789 instructions, baseline
    4100000000 (+25.0%, limit +10%)` for a regression, one line a
    cell, exit 1; `bench gate: json/mixed/16: 12.4% fewer instructions
    than the baseline: run scripts/bench/gate.py --write and commit it`
    for 5% or more fewer, a warning, exit 0; and, when the runner's
    compiler or C library line differs from the file's,
    `bench gate: the baseline is for <old>; this runner has <new>:
    regenerate it with scripts/bench/gate.py --write and commit it`,
    exit 3, no cell compared. Exit 3 is the fourth failure that is not
    the code (docs/Plan.md, "ACCEPT"). A baseline is raised only in a
    commit whose message says what made the code cost more, and lowered
    in the commit that made it cheaper. Wall time never gates: a shared
    runner varies by tens of percent. **A baseline that is not there yet
    is not a failure**: `gate.py` with no file at
    `bench/baseline/instructions.json` prints `bench gate: no baseline
    yet; the results are the artifact bench-quick: run
    scripts/bench/gate.py --write on it and commit the file` and exits 0,
    so the first push of the branch (step 9) is green and gives the
    artifact the baseline is taken from. The `bench-gate` job runs
    `python3 -m unittest scripts/bench/test_gate.py` first (the same
    tests as `bench.gate`, which exists only under `-DCF_BUILD_BENCH=ON`
    and so is not in the default ctest).
  * **The history, on the branch `bench-history`.** Chosen over the docs
    site's data file for four reasons. Every deploy of the site
    (`.github/workflows/docs.yml`: `actions/deploy-pages`, with
    `cancel-in-progress`) replaces the whole site, so a data file kept
    in the site has to be carried from the last build by something
    that is itself a store. A branch is git: every point is a commit
    with the date, diff and author a reader can audit. The docs build
    can read it with a plain checkout and no token. And it keeps main's
    history free of a bot's commits. Rejected: workflow artifacts (they
    expire, 90 days), the `gh-pages`-style branch of generated files
    (same overwrite), a database or service (a dependency for a page).
    Layout: an orphan branch holding `README.md` (three lines: what it
    is, that it is written by CI, how to read a file) and
    `runs/<yyyy>/<yyyymmddThhmmssZ>-<sha7>-<kind>.json`, one file a
    run, never edited, so two writers cannot conflict on a file. About
    20 KB a quick run; pruned by hand when the branch passes 50 MB
    (`git checkout --orphan` from the last year), not automatically.
  * **The workflows.** Three places, and no job on a pull request
    writes anything.
    * `.github/workflows/ci.yml`, job `bench-gate` (push and pull
      request; `contents: read`; the top-level `permissions` already say
      so): `apt-get install valgrind libexpat1-dev`, configure and
      build the root project with `-DCMAKE_BUILD_TYPE=Release
      -DCF_BUILD_BENCH=ON` into `build-bench` and build `cf_bench` (it
      needs Doctor, so the tools stay on), run `python3 -m unittest
      scripts/bench/test_gate.py`, configure and build `bench/compare`
      with `-DCMAKE_BUILD_TYPE=Release` and no
      `CF_BENCH_LIBS` (ConfigForge alone: nothing fetched; it configures
      and builds its own copy of ConfigForge as above), `build-bench`'s
      `cf_bench --emit-docs` for 1 and 16 MiB, `scripts/bench/run.py --scope
      quick`, `scripts/bench/gate.py`, and `actions/upload-artifact`
      of the results as `bench-quick` (name and 14 days; the action
      pinned by commit like the others). Budget `timeout-minutes: 15`;
      step 9 records the real time in the notes, and the card stops if
      it is over ten minutes.
    * `ci.yml`, job `bench-smoke` (push and pull request, only when
      `scripts/bench/changed.py` prints `run=true` into
      `$GITHUB_OUTPUT`: a file under `bench/`, `scripts/bench/` or
      `.github/workflows/bench.yml` changed against the merge base with
      `origin/main`, or against `github.event.before` when that is a
      real commit; `fetch-depth: 0`): fetches every library, builds the
      whole comparison, runs `--scope full --sizes 1` with `--reps 3`
      on `mixed` only, and fails if any adapter fails its probe or
      touch check. It proves the fetch, the builds and the adapters
      still work, and costs nothing on a push that touches none of
      them. `timeout-minutes: 30`.
    * `.github/workflows/bench.yml` (new), `permissions: contents:
      read` at the top. Triggers: `workflow_run` of `ci` with
      `types: [completed]` and `branches: [main]`; `schedule`
      `cron: "17 2 * * 0"` (Sundays); `workflow_dispatch` with the
      inputs `sizes` (default `1,16,256`) and `publish` (default
      `true`). Job `history`: only when `github.event_name ==
      'workflow_run'`, `github.event.workflow_run.conclusion ==
      'success'` and `github.event.workflow_run.event == 'push'`;
      `permissions: contents: write`, `actions: read`; downloads the
      `bench-quick` artifact of that run, checks it with `python3
      scripts/bench/page.py --validate <file>`, checks out
      `bench-history` (creating it as an orphan the first time), copies
      the file to its path, commits as `github-actions[bot]` with the
      message `bench: <sha7> quick [skip ci]`, and pushes, three
      attempts with `git pull --rebase` between. Job `compare`: on
      `schedule` and `workflow_dispatch`, a matrix `compiler: [gcc,
      clang]`, `timeout-minutes: 120`, fetches, builds, runs `--scope
      full` and uploads `bench-full-<compiler>`; job `publish-full`
      (`needs: compare`, `contents: write`, `if: github.ref ==
      'refs/heads/main' && inputs.publish != 'false'`) appends both
      files to `bench-history` in the same way. Both writing jobs share
      `concurrency: {group: bench-history, cancel-in-progress: false}`.
    * Because `bench-history` is a branch of the repository, `ci.yml`'s
      trigger becomes `on: {push: {branches-ignore: [bench-history]},
      pull_request: {}}` (today `on: [push, pull_request]`, line 2), or
      every history commit would run the whole suite.
  * **What a weekly run costs, and why the split.** The quick run
    (ConfigForge only, 16 MiB) is a few minutes and is on every push,
    where the gate is needed. The comparison runs every library, every
    document and 256 MiB, which is an hour or more: weekly, or by hand
    when a change is meant to move a line. The libraries are pinned, so
    their numbers change with the runner and not with a commit; weekly
    points are enough to draw them.
  * **The page.** `docs/Performance.md` (P-1 created it with the
    baseline; this card keeps P-1's text and adds what follows). Three
    parts. (1) Hand-written, once: how it is measured (the rules above
    in the page's words), what is not compared, the machine caveat,
    "What was tried" (the log P-2, P-3 and later cards add a row to:
    the change, the cell, before, after, kept or reverted, the
    commit; Maintaining.md's "Measured" stays and is linked). (2)
    Between `<!-- bench:static:begin -->` and `<!-- bench:static:end
    -->`, generated from files in the repository and committed:
    "The libraries" (from `bench/compare/libraries.cmake` and the
    settings in `bench/compare/settings.json`, new, one entry a
    library), "The documents" (the table above, from
    `bench/compare/documents.json`, new), and "What each library
    checks" (from `bench/compare/probes.expected`). The check
    `gate.perfpage` (ctest, `cmake/gates/GatePerfpage.cmake`, runs
    `scripts/bench/page.py --check docs/Performance.md`; skipped, as
    `gate.formats` is, when there is no Python) regenerates this block
    and fails when the file differs, with
    `docs/Performance.md is not what scripts/bench/page.py writes: run
    python3 scripts/bench/page.py --write docs/Performance.md`. (3)
    Between `<!-- bench:history:begin -->` and `<!-- bench:history:end
    -->`, generated when the site is built and never committed: the
    docs build runs `page.py --history <bench-history checkout> --out
    docs/performance` and rewrites that block in its working copy. With
    no history (a fork, a pull request, the first week) it writes "No
    history yet: the page is filled by CI from the branch
    bench-history." and four empty charts, and the build still passes.
    The block holds:
    * "Where ConfigForge is behind": every (format, document, size) of
      the latest three full runs where ConfigForge's median MiB/s is
      under 97% of the best other library's (median of the three
      ratios), worst first, with the library, both numbers and the
      commit; the table is above the charts, and the page says so;
      then "Level" (within 3%) and "Ahead", each with its rows. The XML
      rows are in a table of their own, headed "Overhead of the layer
      over expat".
    * Per format, two charts, as SVG written by `page.py` itself
      (standard library; the docs environment, pinned in
      `scripts/docs/requirements.txt`, has no plotting library and gets
      none): `docs/performance/<format>-speed.svg`, MiB/s of the
      `mixed` document at 16 MiB against commit order, ConfigForge's
      line from every push, each other library's from the weekly runs
      (dashed, a marker a run); and `<format>-instr.svg`, instructions
      per byte at 1 MiB `mixed` and 16 MiB, ConfigForge from every
      push, the others weekly. Each point's commit is its `<title>`
      (a hover) and a link to the commit; the x axis is the commit
      date. The instruction chart is the trend; the speed chart is
      noisy by the runner and says so under it.
    * "Size and memory": code added and peak growth per library, the
      latest full run, sorted, ConfigForge's rows marked.
    * "What each library refuses": the probe table's latest run and the
      alias outcome table.
    `docs/performance/` is in `.gitignore` (`/docs/performance/`),
    written by the docs build alone. `.github/workflows/docs.yml`'s
    `build` job gains, before MkDocs, a checkout of `bench-history`
    into `$RUNNER_TEMP/bench-history` (`continue-on-error` when the
    branch does not exist) and the `page.py` call. `docs.yml` already
    runs on pushes to main, so the page follows a push by the docs
    build; the weekly runs reach it at the next push to main, or by
    `gh workflow run docs.yml` after one.
  * **README.** Not changed by this card. The README leads with the lines
    ConfigForge wins when the first weekly run has said which they are,
    in a commit of its own that quotes the page's table and links it.
* **files**: `bench/bench.c` (`--emit-docs`, `bench_doc`),
  `bench/CMakeLists.txt` (unchanged in substance; a comment naming
  `bench/compare/`), `bench/docs-1mib.fnv` (new),
  `bench/compare/CMakeLists.txt`, `bench/compare/libraries.cmake`,
  `bench/compare/documents.json`, `bench/compare/settings.json`,
  `bench/compare/probes.expected`, `bench/compare/main.cpp`,
  `bench/compare/touch.h`, `bench/compare/adapters/<name>.cpp` (one a
  library: `configforge`, `yyjson`, `simdjson`, `rapidjson`, `cjson`,
  `libyaml`, `rapidyaml`, `tomlc17`, `tomlpp`, `tinycbor`, `expat`,
  `inih`), `bench/compare/libs/<name>.cmake` (one a fetched library),
  `bench/compare/size/` (the size programs' CMake and one `main.cpp`),
  `bench/baseline/instructions.json`, `scripts/bench/fetch.cmake`,
  `scripts/bench/run.py`, `scripts/bench/gate.py`,
  `scripts/bench/page.py`, `scripts/bench/changed.py`,
  `cmake/gates/GatePerfpage.cmake`, `cmake/ConfigForgeGates.cmake`
  (register `gate.perfpage`), `cmake/gates/GateDependencies.cmake`
  (its `file(GLOB_RECURSE build_files ...)`, which gains `bench/CMakeLists.txt`,
  `bench/compare/CMakeLists.txt` and `bench/compare/libs/*.cmake`;
  A-1 and A-1b changed its `src/` entries to `lib/CMakeLists.txt` and
  `lib/*/CMakeLists.txt`; add after them),
  `.github/workflows/ci.yml` (the trigger; `bench-gate`,
  `bench-smoke`; the clang-format step of the `format-patch` job gains
  `bench/compare`), `.github/workflows/bench.yml` (new),
  `.github/workflows/docs.yml`, `.gitignore`, `scripts/format.cmake`
  (its two globs for `bench/*.c`) and `cmake/ConfigForgeFormat.cmake` (its `bench/*.c`
  glob), both gaining `bench/compare/*.cpp`, `bench/compare/*.h` and
  `bench/compare/adapters/*.cpp`; last: `docs/Performance.md`,
  `docs/Dependencies.md`, `mkdocs.yml`, `scripts/README.md`,
  `docs/Decisions.md`, `docs/Plan.md`, `docs/Roadmap.md`,
  `docs/Maintaining.md`, `CHANGELOG.md`.
* **steps**:
  0. If `## 1.8.0 — unreleased` is not open, open it:
     `cmake -DTO=1.8.0 -P scripts/release/version.cmake`; commit that
     alone.
  1. `bench/compare/libraries.cmake`: for each library, `git ls-remote
     --tags <url>` for the newest release tag at least 30 days old,
     above the floors in "decided" (a lower tag is a stop); its commit
     from the same listing (the peeled `^{}` line for an annotated
     tag); for tomlc17 and RapidJSON the commit and its date. Write
     them. Read each library's `LICENSE` at the pin, and note the SPDX
     identifier for step 12. `scripts/bench/fetch.cmake`; run it into
     `$HOME/cfb-libs`; every directory's `git rev-parse HEAD` equals
     the file's commit.
  2. The documents first, and the test that watches them. In
     `bench/bench.c`, `bench_doc` and `--emit-docs`; then
     `bench/docs-1mib.fnv` from one run, and the check `bench.docs`: a
     ctest test, `bench.docs`, under `if(CF_BUILD_BENCH ...)` in
     `bench/CMakeLists.txt`, that runs `cf_bench --emit-docs <tmp>
     --size 1` and compares its lines to the file. Break one generator
     constant, see `bench.docs` fail, put it back. Run it with GCC and
     with Clang: the files are equal.
  3. `bench/compare/CMakeLists.txt` (with ConfigForge configured on its
     own, as "The same flags" says, not added as a subdirectory),
     `main.cpp`, `touch.h` and the
     ConfigForge adapter alone; `cfb_compare --lib configforge --doc
     <file> --mode speed --reps 3` prints a cell for each of the five
     formats' `mixed` at 1 MiB (XML too, with `CF_WITH_XML=ON`).
  4. The test that fails first, for the count: `--mode count` under
     callgrind on `json-mixed-1` twice gives the same number (see it
     differ if `cfb_cell` is not `noinline`: build once without the
     attribute and see two numbers or none), then with it, equal.
  5. The adapters one library at a time, in the order of "The
     libraries", each with its `libs/<name>.cmake`. After each, `bench.touch`
     for its format: its counters equal ConfigForge's on every 1 MiB
     document it can read. A difference is a bug in the adapter until
     the library's own documentation says otherwise, and then it is
     written in `settings.json` under that library's "notes" and the
     counter is left out of the comparison for that cell by name.
  6. `--mode probe`; write `bench/compare/probes.expected` from the
     first run; read it: for each library, every probe ConfigForge
     refuses and the library does not is a flag to try (strict mode),
     and when none exists it stays in the file and is printed on the
     page. The ctest test `bench.probes` compares.
  7. `--mode memory` and `--mode size`, and `bench/compare/size/`.
  8. `scripts/bench/run.py` and `gate.py`: the gate's own tests first,
     `scripts/bench/test_gate.py` (standard `unittest`, run by ctest as
     `bench.gate`): a results file at 111% of its baseline fails with
     the exact message; at 109% passes; at 94% passes with the warning;
     a different compiler line exits 3; no baseline file exits 0 with
     its message. See each fail before the script
     is written, then pass.
  9. Take the baseline from the artifact of the first push of this
     branch (that push is green: no baseline yet is exit 0), commit
     `bench/baseline/instructions.json`, push; the next
     CI run of `bench-gate` compares against it and passes. Write the
     job's time in the notes.
  10. `scripts/bench/page.py`, its `--check` and `--validate`, and
      `gate.perfpage`. Break the committed static block by one
      character, see the gate fail with its message, restore. Run
      `--history` against a directory of two made-up result files and
      one of none; open the SVGs in a browser (the Browser pane or
      `xdg-open`): lines, markers and hovers present, the empty case
      printed.
  11. The workflows: `ci.yml`'s trigger, `bench-gate`, `bench-smoke`,
      `scripts/bench/changed.py`; `bench.yml`; `docs.yml`. Push the
      branch; `bench-gate` and `bench-smoke` are green. `bench.yml`'s
      triggers exist only from the default branch: it is checked by
      reading it twice and by `actionlint` where it is installed, and its
      first real run is after the merge (see "stop if").
  12. `docs/Dependencies.md`, "3. Continuous integration": one row a
      library of the table above (licence from step 1, "the benchmark's
      comparison, `bench/compare/`, fetched by `scripts/bench/fetch.cmake`
      at a pinned commit, built by CI only, never shipped"), and
      valgrind's row says it is also used by `bench-gate`. The page,
      `mkdocs.yml`, the README of scripts, the decision, the index.
  13. ACCEPT; after the merge to main, the first `workflow_run` writes
      `bench-history`: `git fetch origin bench-history` shows one file;
      then `gh workflow run bench.yml -f sizes=1`, and when it ends
      `bench-history` has two `full` files (gcc, clang). Write both in
      the notes.
* **accept**: ACCEPT-LINUX (no `-DEXTRA`: none of its steps reads
  `bench/` or `scripts/bench/`), and two more builds, each with its
  own tests. In the root project, `cmake -S . -B $HOME/cfb-root
  -DCMAKE_BUILD_TYPE=Release -DCF_BUILD_BENCH=ON` built, with `ctest
  --test-dir $HOME/cfb-root --output-on-failure -R
  "bench\.docs|bench\.gate|gate\.perfpage|gate\.dependencies|gate\.text"`
  passing (`bench.docs`, `bench.gate` and the gates are registered
  there). In the comparison project, `cmake -S bench/compare -B
  $HOME/cfb -DCF_BENCH_LIBS=$HOME/cfb-libs -DCMAKE_BUILD_TYPE=Release`
  built, with `ctest --test-dir $HOME/cfb --output-on-failure -R
  "bench\.(touch|probes)"` passing (the adapters' tests are registered
  there, and need the fetched libraries);
  ACCEPT-WINDOWS (`gate.perfpage` and `gate.dependencies` run there; the
  comparison is Linux only and is not built); ACCEPT-CI, with the
  jobs `bench-gate` and `bench-smoke` green on the branch. CI-only
  part: `bench.yml` and the history (steps 11 and 13), accepted by the
  first runs after the merge.
* **stop if**: P-1's generators cannot be called as `bench_doc`'s
  shape needs (they print as they go, or hold state): wrap them, and if
  that changes P-1's printed lines, stop and write what moved. A
  library cannot be built with `CF_BENCH_FLAGS` alone (its sources need
  a flag of its own to compile, not to run faster): leave it out of the
  round, write why in `settings.json` under its name and on the page,
  and go on with the rest. A library's strict mode cannot be made to
  refuse what ConfigForge refuses: it stays in, and the gap is on the
  page (this is not a stop). `bench-gate` takes over ten minutes: cut
  the gated documents to 4 MiB and say so in the notes, never raise
  the budget. After the merge `bench.yml`'s `history` job fails on
  permissions or on the orphan branch: fix the workflow in a commit of
  its own, and say so in the notes; the card stays open until
  `bench-history` has its first file.
* **docs**: **D-111** in `docs/Decisions.md` (D-109 and
  D-110 belong to other cards written with this one; if they
  are not in the file when this card runs, add D-111 after D-108 and
  keep the gap in the index with the title "reserved" for them, and
  not renumber), title "The comparison with other libraries: one set
  of flags, the same work, and a history kept on its own branch".
  Question: how is "faster than the others" made a claim that can be
  checked. Options: (1) numbers from each library's own benchmark,
  rejected: different documents, flags and work; (2) a comparison
  built by the library's own build files, rejected: each applies its
  own flags; (3) one harness, every library compiled by us under one
  flag set, one fold of the same work, validation shown by probes,
  each version pinned by commit (decided); (4) none, rejected:
  P-2 and later would have nothing to be held to. And for the
  history: a branch (decided), the site's data file, artifacts,
  a service (rejected, with the reasons above). Decision: option 3;
  the gate counts instructions and never wall time; the page shows
  losses first. Consequence: a library enters the round by an adapter,
  a `libs/<name>.cmake`, a settings entry and probe lines, and leaves
  by removing them. "Built: P-1b." `docs/Plan.md`: the index row of P-1b
  `done` with its commits (it is there already); and the fourth failure
  that is not the code, under "ACCEPT": "`bench gate` exit 3: the runner's compiler
  or C library is not the baseline's; regenerate the baseline with
  `scripts/bench/gate.py --write` from the CI artifact." `docs/Roadmap.md`:
  "The aim: beat the others...", "Benchmarks fair, and without mercy"
  and "A live optimization and benchmark page" go 🟨 with "built (P-1b),
  first round: 11 libraries in 6 formats; the README's lead lines wait
  for the first weekly run"; "Measured against the libraries a program
  would otherwise use" is left ⬜ and says it is the other comparison.
  `docs/Maintaining.md`: the 1.8.0 row's done-when already names the
  page; add under "Measured" a line pointing to `docs/Performance.md`
  as the place numbers of this kind now go. This file,
  `docs/plan/P-word-at-a-time.md`, "What keeps a gain": "A reference
  point" gains "(P-1b: the comparison and its page)". `scripts/README.md`:
  rows for `bench/fetch.cmake`, `bench/run.py`, `bench/gate.py`,
  `bench/page.py`. `mkdocs.yml`: under Design, after Conformance,
  `- Performance: docs/Performance.md`, when P-1 has not put it
  there. CHANGELOG, `## 1.8.0 — unreleased`, "### Added":
  "- **The benchmark against other libraries** (`bench/compare/`):
  the same documents read through yyjson, simdjson, RapidJSON, cJSON,
  libyaml, rapidyaml, tomlc17, toml++, tinycbor, expat and inih, under
  one set of flags and the same work; CI counts instructions and fails
  a 16 MiB read that costs more than 10% over its baseline, keeps a
  history on the branch `bench-history`, and `docs/Performance.md`
  draws each format's line over time, losses first (D-111)."
* **notes**:

---

## P-2 - Word-at-a-time scans in C89, with the byte loop as reference

* **id**: P-2
* **goal**: an internal header of SWAR primitives exists, each with a
  byte-loop twin, and a test proves every primitive equal to its twin
  on every alignment, length and byte value that matters.
* **why**: the primitives are the part that must be right once; P-3
  then only calls them.
* **needs**: P-1
* **size**: medium
* **decided**:
  * `lib/serialize/src/swar.h` (internal, not installed, inside
    `CF_SERIALIZE_HEADERS`' rule since it is not under an `include/`),
    with `static` functions so each user inlines them:
    * `cf_swar_find_any(p, n, a, b, c, d)`: the index of the first byte
      equal to one of up to four bytes, or `n` (whitespace, `"`, `\`,
      newline).
    * `cf_swar_ascii_run(p, n)`: the length of the leading run of bytes
      below 0x80 (UTF-8 validation skips it whole).
    * `cf_swar_find_lt(p, n, c)`: the first byte below `c` (control
      characters in a JSON string).
    Each built on the classic has-zero-byte test,
    `(w - ONES) & ~w & HIGHS`, with `ONES`/`HIGHS` made from
    `(unsigned long)-1 / 255` so they fit any word width.
  * `CF_WITH_SWAR` (default 1) in `cf_build_options.h`; 0 makes every
    primitive its byte loop, for the reference build and for a target
    where words are slower. CI builds one job with it 0.
  * Tests, `tests/unit/test_swar.c`: every primitive against its byte
    loop on lengths 0 to 3 words + 7, at every starting offset 0 to 7
    in a buffer padded with poison bytes that must never be read
    (sanitize build); every byte value 0 to 255 as the target and as
    the filler; and a fuzz target (`tests/fuzz/swar.c`) comparing the two.
  * A CBMC proof of `cf_swar_find_any`'s bounds (no read past `n`),
    next to the existing proofs, if the proof harness takes it in under
    the time the others take.
* **files**: `lib/serialize/src/swar.h` (new), `lib/port/include/configforge/cf_build_options.h`,
  `cmake/templates/cf_build_options.h.in`, `cmake/ConfigForgeOptions.cmake`,
  `tests/unit/test_swar.c` (new), `tests/unit/CMakeLists.txt`,
  `tests/fuzz/swar.c` (new), `tests/fuzz/CMakeLists.txt`; last: `.github/workflows/ci.yml`
  (the `CF_WITH_SWAR=0` job), `docs/Architecture.md`, `CHANGELOG.md`.
* **steps**:
  1. The test with only the byte loops behind the names; see it pass,
     then break one SWAR stub and see it fail.
  2. The SWAR bodies; the test, sanitize and fuzz (a minute) pass.
  3. The CI job and the docs.
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI, the compilers
  job (tcc, pcc, musl) included: a 32-bit `unsigned long` (Windows) and
  a 64-bit one both run the test.
* **stop if**: a compiler of the matrix miscompiles the bit trick (a
  test fails only there): keep the byte loop for that compiler by
  `CF_WITH_SWAR=0` in its job, and record it.
* **docs**: D-<next free>, "Word at a time, in C89, before SIMD": the
  rules above, the options (none; SWAR; SIMD only; a library such as
  simdjson, rejected for C89 and the dependency), the decision.
  CHANGELOG, "### Added" of the open version: "`CF_WITH_SWAR` (on): the
  readers scan a word at a time; off, byte by byte."
* **notes**:
  * Read against the tree after A-1b: `tests/unit/` no longer exists; unit
    tests are in `lib/<c>/tests/` (A-1b's rule). This card names
    `tests/unit/test_swar.c` and `tests/unit/CMakeLists.txt`, and
    `lib/serialize/src/swar.h` is an internal header under a source
    directory, which no test of `lib/serialize/tests/` includes today. Which
    component's `tests/` holds `test_swar.c`, and how it reaches `swar.h`,
    is not plain; the card is to be rewritten on D-101 first.

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
* **A reference point.** P-1's benchmark also runs the same documents
  through the other libraries of the Roadmap's comparison (1.8.0), so
  "faster" means faster than something a user would otherwise pick,
  not only faster than before.
