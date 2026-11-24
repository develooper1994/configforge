# Performance

What the library costs, measured, with the machine and the compiler
beside every number. A change that is meant to make something faster is
judged against the numbers here, and adds its own.

The benchmark is `bench/bench.c`. It is not a test and not a gate and
is not built by default:

```sh
cmake -B build-bench -DCMAKE_BUILD_TYPE=Release -DCF_BUILD_BENCH=ON
cmake --build build-bench --target cf_bench
build-bench/bench/cf_bench docs            # 1 and 16 MiB of each format
build-bench/bench/cf_bench docs --large    # and 256 MiB
build-bench/bench/cf_bench docs json cbor  # only these formats
```

Release, or the numbers are the debug build's. The other cases of the
benchmark (`cf_bench`, `files`, `reload`, `define`, `emit`) measure
configurations of thousands of keys, and D-76 has their sizes.

## Allocation

### Where a builder load spends time

Ubuntu 26.04.1 VM, GCC 15.2, Release `-O2`. Each mixed 16 MiB document
went through builder creation, key resolution and release. Before P-4,
the byte-at-a-time wipe accounted for much of the cost of freeing an arena.
The percentages below are perf self samples. Asked is the counting
allocator's total, and the last column estimates superseded candidate
arrays from their allocation sizes.

| Format | Calls | Asked (MiB) | malloc + free (%) | Arena alloc (%) | Arena free (%) | Superseded arrays (MB, est.) |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| JSON | 12,727 | 443 | 0.31 | 0.78 | 9.7 | 156 |
| YAML | 11,788 | 424 | 0.30 | 0.65 | 15.4 | 101 |
| TOML | 12,520 | 410 | 0.25 | 0.57 | 10.7 | 72 |
| CBOR | 17,518 | 566 | 0.38 | 0.68 | 11.4 | 205 |
| INI | 11,360 | 404 | 0.45 | 1.10 | 11.2 | 100 |
| XML | 8,047 | 172 | 0.81 | 0.63 | 7.0 | 3 |

**Wipe (0), kept.** Each used block region is zeroed with `memset`
through a volatile function pointer. The table gives Callgrind
instructions for 1 MiB and the better of two wall times for 16 MiB.

| Linux builder load | Metric | Before (`8ef51e9`) | After (`d32708a`) |
| --- | --- | ---: | ---: |
| JSON mixed, 1 MiB | instructions | 569,665,696 | 430,337,353 |
| CBOR mixed, 1 MiB | instructions | 673,817,626 | 494,910,137 |
| JSON many-tiny, 1 MiB | instructions | 1,820,347,853 | 513,607,893 |
| JSON mixed, 16 MiB | seconds | 1.16 | 1.04 |
| YAML mixed, 16 MiB | seconds | 1.16 | 1.07 |
| INI mixed, 16 MiB | seconds | 0.98 | 0.89 |
| JSON many-tiny, 16 MiB | seconds | 1.66 | 0.47 |
| YAML many-tiny, 16 MiB | seconds | 2.11 | 0.71 |
| CBOR many-tiny, 16 MiB | seconds | 2.06 | 0.54 |
| INI many-tiny, 16 MiB | seconds | 1.80 | 0.49 |

On many-tiny, arena release took 47–73% of perf samples before the
wipe change. A reusable arena would also need wiping. The profile
put libc allocation and release below 1% on every mixed 16 MiB load,
so P-4 closed the mimalloc comparison without running it.

### Windows builder loads after the wipe change

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

### Windows allocation policy trials

All three sizing candidates were reverted. Each row compares a trial
with its baseline. Times are best of three. Peak is the maximum live requested
bytes in one configuration, measured separately from timing.

| Candidate | 16 MiB mixed load | Baseline → trial (s) | Peak that ruled it out | Baseline → trial (bytes) |
| --- | --- | ---: | --- | ---: |
| (2) Next block sized from input | INI | 0.803 → 1.774 | each tiny document | 22,784 → 32,960 |
| (1) Doubling blocks, 256 KiB cap | JSON | 0.891 → 0.949 | each tiny document | 22,784 → 28,864 |
| (3) Candidate capacity from input | CBOR | 1.089 → 1.314 | YAML mixed, 1 MiB | 17,171,360 → 34,158,560 |

On 16 MiB JSON, doubling cut allocator calls from 12,729 to 217 but
increased unused arena capacity from 72,600 to 1,467,288 bytes. On
16 MiB CBOR the capacity estimate saved 110,990,080 requested bytes.
Neither trial sped up any mixed 16 MiB read. The card records all
five formats, both document sizes and both workloads. P-4's trial tables (docs/Plan.md, "Done, their cards removed"; the card's text in `git log -- docs/plan/P-allocators.md`)

## Large documents

A format library is also handed megabytes: a JSON export, a CBOR dump.
`cf_bench docs` reads one document of each format at 1, 16 and (with
`--large`) 256 MiB, through the public codec API (`cf_codec_decode`)
into a sink that only counts what it is given, and prints one line per
format and size: the median of the runs, in MiB of document per second.

The documents are made by the benchmark itself, the same bytes every
run (a fixed seed), so no large file is committed. A document is
records until it is the size asked. A record has:

* a string of 40 to 240 bytes, and once in 64 records one of 16 to 64 KiB;
* a string with escapes (`\n`, `\t`, `\"`, `\\`, `é`, and `\/` in
  JSON and YAML), in the formats that have them, and the same bytes
  raw in CBOR;
* a string of 200 bytes that is non-ASCII UTF-8 (two, three and four
  byte sequences) in a record of every four;
* 8 to 31 small numbers, a fifth of them negative;
* once in 16 records, a chain of 8 to 23 nested tables (within
  `CF_MAX_DEPTH`).

The records sit side by side under one top-level table, so a document
is wide as well as deep. JSON is minified in one record and indented in
the next; YAML quotes its strings in every other record and writes its
numbers as a block in one and a flow sequence in the next; INI cannot
escape, and writes its strings as they are, a number to a line. The
numbers are the codec's alone: no configuration is built, so the
flattener's bound (D-82) is not what a document meets here, only each
reader's own limits, and none of them refused 256 MiB.

### Baseline, before any change (P-1)

| | |
| --- | --- |
| Machine | Intel Core i7-10750H, 2.6 GHz, 32 GB, Windows 11 |
| Compiler | MSVC 19.5 (Visual Studio 18), `/O2 /Ob2 /DNDEBUG`, Release |
| Library | the 1.8.0 tree at P-1: every format built in; YAML by the backend this build finds |
| Timer | `clock()`: CPU time. 1 MiB is timed 16 decodes at a time |

MiB of document per second, median (runs: 9 at 1 MiB, 5 at 16, 3 at
256):

| Format | 1 MiB | 16 MiB | 256 MiB |
| ------ | ----: | -----: | ------: |
| INI | 1455 | 1232 | 1045 |
| CBOR | 400 | 381 | 285 |
| JSON | 195 | 211 | 201 |
| TOML | 159 | 115 | 110 |
| YAML | 115 | 101 | 99 |

What the numbers say, and no more than they say:

* INI reads five times faster than JSON; YAML and TOML are the slow ones, by about half.
* CBOR and INI slow as the document outgrows the cache (CBOR 400 to
  285); JSON, YAML and TOML hardly move. A time that grows with size
  where the work per byte should not is a cost of memory, not of a scan.
* The spread within a format (best against median, on a laptop) is
  10 to 20 percent: a change below that is not a result.

### The same on Linux, GCC and Clang

The same machine, WSL 2 (Ubuntu 26.04), GCC 15.2 and Clang 21.1, a
Release build with the optimisation level replaced; medians, MiB/s.

| Format | GCC -O2 1 MiB | 16 MiB | GCC -O3 1 MiB | 16 MiB | Clang -O2 1 MiB | 16 MiB | Clang -O3 1 MiB | 16 MiB |
| ------ | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| INI | 1703 | 1588 | 1931 | 1686 | 1894 | 1781 | 1842 | 1663 |
| CBOR | 707 | 635 | 728 | 655 | 781 | 675 | 800 | 684 |
| JSON | 302 | 291 | 342 | 329 | 223 | 220 | 222 | 219 |
| TOML | 243 | 233 | 233 | 223 | 215 | 210 | 216 | 212 |
| YAML | 185 | 179 | 189 | 185 | 159 | 155 | 170 | 168 |

* GCC reads JSON a third faster than Clang (291 against 220 MiB/s at
  -O2); Clang reads CBOR and INI faster. The compiler alone moves a
  format by up to a third: a change is compared under one compiler.
* -O3 gives GCC's JSON 13 percent and TOML nothing (a little less);
  Clang's -O3 changes nothing beyond the spread but YAML's 8 percent.
* Every Linux build reads one and a half to three times as fast as
  MSVC's: what the MSVC numbers measure is partly MSVC.

Where the time goes inside each reader is the profile below.

### The profile

One command takes the profile, from HEAD, on every place it can reach and
with every profiler that is there (`docs/plan/L-tools.md`, group 13):

```sh
python scripts/work/profile.py json yaml toml cbor ini           # all reachable places
python scripts/work/profile.py --host cfvm --wsl --windows json  # or named ones
```

It builds `cf_bench` with symbols (RelWithDebInfo, `-O2 -g
-fno-omit-frame-pointer`; MSVC on Windows) and writes one Markdown file in
`CF_WORK` - for each format the top six functions by time (perf with
`cpu-clock`, hardware events when the kernel has them; on Windows Visual
Studio's collector read with `xperf`, no administrator rights), by
instructions (callgrind), cache misses (cachegrind), the peak and the
count of allocations (heaptrack, else massif) and the loops GCC did not
vectorize in the source files of the top functions - and one summary
line per format on the screen. A tool or place that is missing is one
`skipped:` line and the run goes on. The tables below were taken this
way, first by hand.

Each format's top functions are written below, with the compiler and
its flags, and the compiler's vectorization report for the scans the
profile names (GCC `-fopt-info-vec-all`, Clang `-Rpass=loop-vectorize`
and `-Rpass-missed=loop-vectorize`). The baseline is taken at `-O2` with
GCC and Clang, and once at `-O3` for the record.

The distribution here has no `perf`; the profile was taken with
Valgrind's callgrind instead (instructions, not time: the same ranking
for code that does not wait on memory), GCC 15.2 `-O2 -g`, `cf_bench docs
<format>` at 1 and 16 MiB:

```sh
valgrind --tool=callgrind --callgrind-out-file=cg-json.out build-prof/bench/cf_bench docs json
callgrind_annotate --inclusive=no cg-json.out | head -30
```

| Format | Top functions (self) | Share |
| ------ | -------------------- | ----: |
| JSON | `js_string` (json.c), and its inlined `memcpy` | 79 + 3 % |
| | `js_space`, `js_value`, `cf_text_to_u64` | 4, 3, 2 % |
| TOML | `toml_string` (toml.c) | 80 % |
| | `toml_value`, `toml_digits`, `cf_text_to_u64` | 3, 2, 2 % |
| YAML | `yp_quoted`, `yp_is_key` (yamlparse.c) | 36, 25 % |
| | `yp_plain_line`, `yp_plain_stop`, `yp_plain` | 9, 7, 4 % |
| CBOR | `cf_vformat` (port.c), `cf_text_is_utf8` (blob.c), `cf_u64_to_text` | 34, 25, 13 % |
| | `cbor_item`, `cbor_head_read` | 7, 5 % |
| INI | `ini_decode`, `ini_trim` (ini.c) | 68, 9 % |

The same, by time, on a Linux virtual machine (VMware, Ubuntu 26.04,
4 cores, GCC 15.2 `-O2 -g -fno-omit-frame-pointer`) with `perf record -e
cpu-clock -F 4000 -g`, self time:

| Format | Top functions by time | Share |
| ------ | --------------------- | ----: |
| JSON | `js_string`, `js_value`, `js_space` | 76, 5, 4 % |
| TOML | `toml_string`, `toml_value`, `toml_find` | 71, 4, 3 % |
| YAML | `yp_quoted`, `yp_is_key`, `yp_plain_line` | 43, 19, 7 % |
| CBOR | `cf_text_is_utf8`, `cf_vformat`, `cf_u64_to_text` | 32, 24, 10 % |
| INI | `ini_decode`, `ini_trim` | 64, 7 % |

Time agrees with the instruction counts on every format but one: in CBOR
the UTF-8 check is first by time (32 against 25 %), a sign that it waits
on memory. A virtual machine gives perf no hardware counters unless the
host lets it (VMware: "Virtualize CPU performance counters"); `cpu-clock`
sampling works without them, cycles and cache events do not.

On Windows, without administrator rights: Visual Studio's collector
samples the program, and the Windows Performance Toolkit's `xperf` turns
the trace into a table (MSVC RelWithDebInfo, `cf_bench docs json`):

```bat
VSDiagnostics.exe start 7 /launch:cf_bench.exe /launchArgs:"docs json" /loadConfig:"<Collector>\AgentConfigs\CpuUsageHigh.json"
VSDiagnostics.exe stop 7 /output:json.diagsession
rem a .diagsession is a zip; its sc.user_aux.etl is the trace
set _NT_SYMBOL_PATH=<the directory of cf_bench.pdb>
xperf -i sc.user_aux.etl -symbols -o json-prof.txt -a profile -detail
```

| Format | Top functions by time, MSVC | Share |
| ------ | --------------------------- | ----: |
| JSON | `js_string`, `js_put`, `memcpy` (vcruntime) | 52, 24, 10 % |

MSVC keeps `js_put` - the append of each byte to the value - a call of
its own, a quarter of the time, where GCC and Clang inline it into
`js_string`: part of why MSVC reads JSON at 200 MiB/s and GCC at 290.

What it answers for P-3: in JSON and TOML four fifths of the work is the
string scan - the search for a quote, a backslash and a control byte -
and in YAML the quoted scan and the key test are three fifths: those are
the scans a word at a time is for. CBOR is the exception: its time is
not a scan but text made for each value (`cf_vformat` and
`cf_u64_to_text`, an array index written as a name) and the UTF-8
check of every string; the first is work to remove, not to speed up.
INI is already fast and its time is its one loop.

### The word-at-a-time backend of lanes (P-2)

`cf_bench lanes` runs each span operation of lanes' word backend against
its scalar loop, on spans of 8 to 4096 bytes, and prints the ratio (above 1
is faster). The rule that decides what the word backend keeps: an operation
keeps its word body when it is at least 5% faster than the loop on the
spans of 64 bytes and longer, for every kind of input measured, and loses
by no more than 5% on 16 bytes; otherwise its table entry is the scalar
function (`LANES_SWAR_KEEP_*` in `lib/lanes/src/swar.c`). The numbers are
GCC 15.2, `-O2`, x86-64, a shared Linux machine (WSL, 12 logical CPUs, other
builds running: figures on the shortest spans move by 10 to 15% from run to
run), `swar64`, best of fifteen short runs:

| Operation, input | 8 | 16 | 64 | 256 | 4096 | Kept |
| ---------------- | -: | -: | -: | -: | ---: | ---- |
| `find_byte`, no match | 2.1 | 2.0 | 3.5 | 3.2 | 2.8 | yes |
| `find_lt` (0x20), none | 2.0 | 2.1 | 3.4 | 2.6 | 2.9 | yes |
| `ascii_run`, all ASCII | 1.7 | 2.0 | 5.7 | 7.2 | 7.1 | yes |
| `count_byte`, one in eight | 2.3 | 1.4 | 1.7 | 1.9 | 1.9 | yes |
| `find_any`, four blanks, none | 0.8 | 0.9 | 0.9 | 1.4 | 1.8 | no |
| `find_any`, JSON stops, none | 0.9 | 0.9 | 1.0 | 1.3 | 2.4 | no |
| `utf8_valid`, ASCII | 1.3 | 1.6 | 6.1 | 5.8 | 11.4 | no |
| `utf8_valid`, a 2-byte letter in 12 | 0.6 | 0.7 | 0.9 | 0.9 | 0.9 | no |
| `utf8_valid`, dense 3-byte | 0.7 | 0.8 | 0.9 | 1.0 | 1.0 | no |

`find_any` and `utf8_valid` win on long and on ASCII spans and lose or tie
on short and on non-ASCII ones, so by the rule they keep the loop for now;
their word bodies stay in the file and P-3 measures them on the readers' own
inputs (JSON strings, the CBOR text check), which is what decides a caller.
`find_any` takes a set apart on every call and so wins only from 128 bytes
(`LANES_SWAR_ANY_MIN`); caching the shape in `lanes_set` would change A-2's
struct and is the maintainer's to decide when P-3's numbers ask for it.
MSVC and Clang are judged by CI and a later run: only GCC is in this table.

## Against the other libraries

A reader that is faster than itself last month is not shown to be fast;
one that is faster than what a user would otherwise pick is. `bench/compare/`
reads the same documents through ConfigForge and through yyjson, simdjson,
RapidJSON, cJSON, libyaml, rapidyaml, tomlc17, toml++, tinycbor, expat and
inih, under one set of flags and the same work, and reports speed,
instructions, peak memory and code size (D-111). Later cards, P-2 and P-3
and the SWAR and SIMD backend, are held to the numbers it produces.

### How it is measured

* **The same bytes.** `cf_bench --emit-docs <dir>` writes every document
  as a file, deterministic (a fixed seed, mixed with the document's
  number, so one document does not change when another is added), and
  prints a line a file: name, bytes, FNV-1a. The comparison reads these
  files and generates nothing, so every library, and every compiler, is
  given the same bytes. `bench/docs-1mib.fnv` holds the 1 MiB lines, and
  the check `bench.docs` regenerates them: a generator that drifts, or
  differs between compilers, is seen.
* **The same flags.** No library is built by its own build files. Each
  one's sources are compiled by this project's compiler with one flag
  set, `shipped` (the headline: `-O2` with the hardening ConfigForge
  ships with), `bare` (`-O2` alone) or `native` (`-O3 -march=native`,
  for the record, never a headline), and ConfigForge is configured to
  match. GCC and Clang, the runner's own, one build each; the headline
  is GCC's. x86-64 Linux only: macOS and arm64 are added when a runner
  for each is in CI. MSVC is not in the comparison; the baseline above
  has it.
* **The same work.** Every adapter parses and touches every value in
  one fold: containers, strings (keys and values) and their bytes,
  numbers (read into a 64-bit integer or a double and folded into an
  accumulator, so the conversion stays in the loop), booleans, nulls. A
  format whose values are all text counts every value as a string. The
  check `bench.touch` gives every adapter of a format the same
  documents at 1 MiB and fails when a counter differs.
* **Validation, on or off for both, and shown.** Every adapter has
  `--strict` (default: every check the library offers that ConfigForge
  makes) and `--loose` (the library's own default). The headline rows
  are strict. `bench.probes` compares the eight probes of each library
  with `bench/compare/probes.expected`; the gaps are printed below.
* **Speed** is the median of 9 runs (3 at 256 MiB) after a warm-up,
  each run at least 200 ms for a small document, the input read once into
  one buffer with 64 bytes of padding for every library alike, pinned to
  one CPU where `taskset` exists. **Instructions** are one parse under
  callgrind, `cfb_cell` alone. **Memory** is the peak growth of the
  process over its size after the input was loaded. **Size** is what a
  library adds to a program that parses one fixed document, with
  unused sections dropped and the result stripped.
* **The gate counts instructions, never wall time.** CI fails a change
  that makes a format's 4 MiB read cost more than 110% of the
  instructions of `bench/baseline/instructions.json`; a shared runner
  varies by tens of percent in time and by almost nothing in
  instructions. The baseline is raised only in a commit whose message
  says what made the code cost more, and lowered in the commit that made
  it cheaper.
* **Not measured in this round:** one thread against every thread (no
  library here parses in parallel), printing speed (a card for the
  writers), and in-situ and lazy reading as modes of their own.

### What is not compared

JSON5, JSONC, `.properties`, dotenv and TLV: no library was found whose
job is the same. pugixml (a DOM for XML), libfyaml (which ConfigForge can
use as a backend, D-62) and libcbor (a tree, a different interface) are
a second round. Whole configuration libraries (lookup time, libconfig)
are another comparison, in the Roadmap.

The XML row cannot be a win: ConfigForge's XML reader is expat (D-23)
with the layer's own handlers and flattening on top, so its line is
expat's plus what the layer costs, and the page says overhead of the
layer and not behind.

### The machine

A hosted runner cannot fix its frequency, and the record says so
rather than claims it: every results file holds the kernel, the CPU's
name, the logical CPU count, the compiler's first `--version` line, the
C library's, the runner image (`ImageOS`, `ImageVersion`) and the
governor (or `unknown`). Numbers from different machines are not drawn
as one line: the instruction counts are the trend, and the speed chart
is noisy by the runner and says so.

### What was tried

The log that the cards after P-1b add a row to: the change, the cell,
before, after, kept or reverted, and the commit. The numbers in the
section "Measured" of `docs/Maintaining.md` stay and are linked there.

| Change | Cell | Before | After | Kept | Commit |
| ------ | ---- | -----: | ----: | ---- | ------ |

<!-- bench:static:begin -->

### The libraries

Each library's sources are compiled by this project's compiler with one set of flags, and read the same files. One row a library: its pinned version, the settings it is given, and what each gets.

| Library | Version | Commit | Licence | Reads | Settings |
| ------- | ------- | ------ | ------- | ----- | -------- |
| ConfigForge | this tree | - | MIT | JSON, YAML, TOML, CBOR, INI, XML | cf_codec_by_name(<format>) and cf_codec_decode into a cf_doc_sink whose callbacks touch the value; reserve_text from one buffer kept between documents; strict supplies reserve_work (TOML's duplicate check; YAML cannot be read without it) from blocks freed between documents. The readers library (configforge_serialize), built with CF_HARDEN=ON in `shipped`, OFF in `bare` and `native`; its own YAML reader (D-69), XML through expat (D-23). |
| [yyjson](https://github.com/ibireme/yyjson) | 0.13.0 | `6447536015f3` | MIT | JSON | yyjson_read with no flag: it validates structure and UTF-8, and reads a copy of the input. Strict and loose are the same: the library has no option that checks more. |
| [simdjson](https://github.com/simdjson/simdjson) | v5.0.5 | `9c003534b1be` | Apache-2.0 OR MIT | JSON | The runtime-dispatched kernel it picks, padded input, one parser reused between documents (its documented fast path). dom::parser, and ondemand::parser, which is lazy: the adapter touches every value, so the work is equal. UTF-8 is always validated; no option checks more. |
| [RapidJSON](https://github.com/Tencent/rapidjson) | commit of 2025-02-05 | `24b5e7a8b27f` | MIT | JSON | Document::Parse over the bytes and their length. `rapidjson`: kParseValidateEncodingFlag in strict mode (ConfigForge refuses invalid UTF-8 from A-5 on, D-108), the default flags in loose; `rapidjson-nocheck`: the default flags (kParseDefaultFlags), which do not check the encoding. No SSE define in `shipped` (a generic target); RAPIDJSON_SSE42 in `native`. |
| [cJSON](https://github.com/DaveGamble/cJSON) | v1.7.19 | `c859b25da029` | MIT | JSON | cJSON_ParseWithLength; strict: cJSON_ParseWithLengthOpts(..., require_null_terminated = 1) over the document and its NUL (a slice of many-tiny has no NUL after it and is parsed by cJSON_ParseWithLength alone). |
| [libyaml](https://github.com/yaml/libyaml) | 0.2.5 | `2c891fc7a770` | MIT | YAML | yaml_parser_parse in an event loop over yaml_parser_set_input_string. Every scalar is text to it, and to ConfigForge's YAML reader: every scalar is a string here. |
| [rapidyaml](https://github.com/biojppm/rapidyaml) | v0.16.0 | `f8ac8dd50f4f` | MIT | YAML | ryml::parse_in_arena (a copy of the input; parse_in_place destroys its input and is the in-situ row of a later card). Errors throw so that a refusal is an answer. c4core is carried in rapidyaml's own tree at 0.16.0 (ext/c4core.src), so one commit pins both. |
| [tomlc17](https://github.com/cktan/tomlc17) | commit of 2026-10-22 | `4987cf895df6` | MIT | TOML | toml_parse over the bytes and their length; strict turns on its check_utf8 option (a global, off by default). |
| [toml++](https://github.com/marzer/tomlplusplus) | v3.4.0 | `30172438cee6` | MIT | TOML | toml::parse of a string_view with TOML_EXCEPTIONS=0 and TOML_HEADER_ONLY=1, the parse_result checked. No option checks more than it does. |
| [tinycbor](https://github.com/intel/tinycbor) | v7.1 | `da6421f2b5b9` | MIT | CBOR | cbor_parser_init and the iteration calls, with cbor_value_validate first. Strict: CborValidateUtf8 \| CborValidateCompleteData, the checks ConfigForge's CBOR reader also makes. The check for unique keys exists only with sorted keys (CborValidateMapIsSorted), which a document is not, so a duplicate key is not checked. |
| [inih](https://github.com/benhoyt/inih) | r62 | `26254ee9de76` | BSD-3-Clause | INI | ini_parse_string with INI_STOP_ON_FIRST_ERROR 1, INI_USE_STACK 0, INI_ALLOW_REALLOC 1 and INI_MAX_LINE 8192 (its default of 200 bytes would refuse the long-line documents; without INI_ALLOW_REALLOC a heap buffer stays at 200 bytes whatever INI_MAX_LINE is). A slice with no NUL after it (many-tiny) is copied first: ini_parse_string needs one. |
| expat | the system's (libexpat1-dev) | - | MIT | XML | XML_Parse in one call, handlers for start, end and character data; ConfigForge's mapping (D-23) rebuilt from the events. The system's libexpat1-dev, the one ConfigForge's XML reader links too: the two rows run one library. |

Notes on a library:

* **ConfigForge**: The root container, and YAML's empty second one, are not counted. XML's line is expat's plus what the layer costs: the page says overhead of the layer, not behind. The size rows use the default build; the `minimal` row waits for CF_PROFILE (A-11).
* **RapidJSON**: Not a tagged release: RapidJSON's last release, a 2016 tag, does not build with today's compilers; the pin is a commit of master.
* **cJSON**: One number type: every number is counted in `numbers`, and the integer or real is not told.
* **libyaml**: An alias is one event and is not expanded; ConfigForge expands it under D-63 and D-82's bound. The yaml-aliases outcome table shows what each does.
* **rapidyaml**: An alias is kept as a reference and not expanded.
* **tomlc17**: The library has limits of its own that ConfigForge does not: an array of many elements (`array too large`), a table of many keys (`table too large`) and a dotted key of many parts (`too many key parts`) are refused. They are shown as refusals, not left out. Refused in this round: `array-huge` (array too large), `numbers-int` (array too large), `numbers-edge` (array too large), `table-wide` (table too large), `nest-deep` (too many key parts), `records` (too many key parts).
* **inih**: The handler is given names and values, not structure: sections and name[] runs are rebuilt from them. Its line limit is a limit of the library: P-1's records, with strings of up to 64 KiB on a line, are refused. Refused in this round: `records` (a line over INI_MAX_LINE (8192 bytes)).
* **expat**: ConfigForge's XML reader is expat with the layer's own handlers and flattening on top, so its line is expat's plus what the layer costs. It cannot be a win.

Not compared, and said so: JSON5, JSONC, `.properties`, dotenv and TLV (no library found whose job is the same), and pugixml, a DOM for XML (second round, with libfyaml and libcbor).

### The documents

Each is chosen to hurt a reader. A letter says the document exists in that format (J JSON, Y YAML, T TOML, C CBOR, I INI, X XML), a blank that the format cannot say it. Sizes 1, 16 and 256 MiB.

| Name | What it is | J | Y | T | C | I | X |
| ---- | ---------- | - | - | - | - | - | - |
| `mixed` | tables of strings, integers, reals, booleans and short arrays, as a configuration looks | J | Y | T | C | I | X |
| `strings-plain` | strings of 64 to 4096 ASCII bytes, no escape | J | Y | T | C | I | X |
| `strings-escape` | every byte of every string an escape: a line feed, a backslash, a quote, an e-acute, a character outside the first plane (a surrogate pair in JSON); entities in XML | J | Y | T |   |   | X |
| `strings-utf8` | half the bytes non-ASCII, two-, three- and four-byte sequences mixed | J | Y | T | C | I | X |
| `numbers-int` | integers of one to three digits, one a line or element | J | Y | T | C |   |   |
| `numbers-edge` | the int64 limits and their neighbours, reals of 17 significant digits, the largest and smallest exponents, subnormals | J | Y | T | C |   |   |
| `nest-deep` | tables or arrays 31 levels deep (CF_MAX_DEPTH 32 allows 31 inside the document), repeated | J | Y | T | C |   | X |
| `array-huge` | one array of one-digit elements, as many as the size holds (INI: name[] = x lines, D-60) | J | Y | T | C | I | X |
| `table-wide` | one table of unique short keys, as many as the size holds | J | Y | T | C | I | X |
| `keys-prefix` | keys sharing a 200-byte prefix (under CF_KEY_MAX): quadratic time on crafted keys | J | Y | T | C | I |   |
| `whitespace` | mostly blanks, newlines, indentation and comments, a value now and then | J | Y | T |   | I | X |
| `many-tiny` | documents of 20 to 64 bytes, parsed one at a time from their own slice; measures what one parse costs to start and to end | J | Y | T | C | I | X |
| `invalid-late` | a valid document with its one error in the last KiB: a stray ] (JSON), a tab indent (YAML), a duplicate key (TOML), a truncated last item (CBOR), a line with no = (INI), a mismatched end tag (XML); measures the time to refuse | J | Y | T | C | I | X |
| `records` | P-1's records (docs/Performance.md, Large documents): strings of 40 to 240 bytes and, once in 64 records, of 16 to 64 KiB, escapes, non-ASCII, 8 to 31 small numbers, a chain of nested tables | J | Y | T | C | I |   |
| `yaml-aliases` | YAML alone, 1 MiB: plain entries, then aliases of aliases nine levels deep; an outcome table, not a speed row |   | Y |   |   |   |   |

### What each library checks

`cfb_compare --mode probe` gives every reader the same eight probe documents per format (invalid UTF-8 in a string, a duplicate key, an integer past int64, a truncated document, text after the root value, nesting 100 deep, a lone surrogate escape, a NUL inside a string) in strict mode, which turns on every check the library offers that ConfigForge makes. A line is a probe ConfigForge refuses and the library reads: the gap is shown, not hidden. The table is `bench/compare/probes.expected`.

| Library | Format | Does not check (strict) |
| ------- | ------ | ----------------------- |
| yyjson | JSON | nesting-100 |
| simdjson-dom | JSON | nesting-100 |
| simdjson-ondemand | JSON | nesting-100 |
| rapidjson | JSON | nesting-100 |
| rapidjson-nocheck | JSON | invalid-utf8, nesting-100 |
| cjson | JSON | invalid-utf8, nesting-100 |
| libyaml | YAML | nesting-100 |
| rapidyaml | YAML | invalid-utf8, nesting-100, lone-surrogate |
| tomlc17 | TOML | nothing ConfigForge checks |
| tomlpp | TOML | nesting-100 |
| tinycbor | CBOR | int-past-int64, nesting-100 |
| inih | INI | invalid-utf8 |
| expat | XML | duplicate-key, nesting-100 |

<!-- bench:static:end -->

### The history

<!-- bench:history:begin -->
No history yet: the page is filled by CI from the branch bench-history.
<!-- bench:history:end -->
