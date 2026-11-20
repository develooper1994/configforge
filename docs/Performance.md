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

`perf record` needs Linux. On a Linux machine, or in WSL from a copy off
`/mnt`:

```sh
cmake -B build-prof -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCF_BUILD_BENCH=ON
cmake --build build-prof --target cf_bench
for f in json yaml toml cbor ini; do
  perf record -g -o perf-$f.data build-prof/bench/cf_bench docs $f
  perf report -i perf-$f.data --no-children --stdio | head -40
done
```

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
