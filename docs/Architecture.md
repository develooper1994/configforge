# Architecture

The shape of the thing, the rules that keep it that shape, and how to
add to it without breaking either.

Read `docs/Conventions.md` next: this document is the structure, that one
is the house style.

## The one decision everything else follows from

**The provenance chain is the core data structure, and `get()` is the
winner of that chain.**

Not a resolver with a diagnostic bolted on. A resolver that selected a
winner and discarded the rest could not explain itself afterwards, and
adding the chain later would mean rewriting it. Every other choice here
is downstream of that: the flat key space, the two-phase construction,
the single error site, the report format, the binary image.

When a change would make `explain()` harder, the change is wrong.

## Layers

```text
                  diag          explain, inspect, provenance report
                    |
                  bridge        flattener, file source, binary image
                 /      \
          sources        serialize     lexers   |   blob, sink, codecs
                 \      /
                  core          value, key space, definitions, facade
                   / \
       portability     platform
       cf_port.h       src/platform/<name>/
       which compiler  which operating system
```

Two doors, not one. `cf_port.h` decides which *compiler* this is;
`src/platform/` decides which *operating system*. They are separate
because they vary separately — MinGW is GCC on Windows, and clang-cl is
the Microsoft platform with a Clang front end.

| Layer | Owns | May use | May not use |
| ----- | ---- | ------- | ----------- |
| `src/platform` | the environment, UTF-8 arguments, directory listing, file stamps, the change handle | portability | anything above it |
| `src/core` | value model, key space, definitions, validation, `cf_config` | portability, platform | anything above it |
| `src/serialize` | readers, writers, codecs, document events | portability, `cf_value`, `cf_error`, `cf_alloc` | the key space, definitions, sources |
| `src/sources` | flat lexers, key mappings, the sources with no bytes to read | core | serialize, bridge |
| `src/bridge` | flattener, file source, binary image | core **and** serialize | diag |
| `src/diag` | `explain`, `inspect`, the report, replay | everything below | — |

Three of those rules are checked by `gate.layering` rather than
intended. The serialize boundary exists so that extracting that layer
into its own library later stays a directory move and a build file
(decision D-27); the bridge exists because some components are honestly
both, and hiding them in one side or the other would have made the
boundary a lie.

## Data flow

Reading:

```text
bytes ──> codec ──> document events ──> flattener ──┐
                                                     ├─> key space ──> resolve
environment / command line ──> lexer ──> entries ───┘                    |
                                                                         v
memory pairs, definition defaults, binary image ─────────────────> definitions applied
                                                                         |
                                                                         v
                                                                   cf_config
```

Writing:

```text
key space ──> walk ──> document events ──> codec ──> bytes
key space ──> image writer ──────────────────────> bytes
key space ──> report writer ─────────────────────> JSON
```

The report is written directly rather than through the event model,
because a report is not the document the key space describes: it is a
description *of* the key space, with a candidate chain under every key.
Routing it through the events would mean inventing a document whose
shape happens to match the schema, then keeping two things in step.

## Construction is two-phase

```text
load     append candidates            O(1) each, no searching
resolve  one merge sort, then group   O(m log m) in candidates
apply    convert, validate, reselect  the whole error surface
```

Lookup is available after `resolve`, not before. Keeping the entries
sorted during load instead costs a memmove per key and is quadratic
overall: at fifty thousand keys that is tens of gigabytes of copying,
which is the difference between a pause and a hang.

## Source kinds

Sources divide by what they have to do with the bytes, and the division
is what the directories are for.

| Kind | Members | The work | Lives in |
| ---- | ------- | -------- | -------- |
| Direct | memory pairs, definition defaults, replayed report | no bytes | `sources` |
| Flat | environment, command line, `.env` | lexing and naming | `sources` |
| Hierarchical | JSON, INI, TOML, later XML | parsing, then flattening | `serialize` + `bridge` |
| Image | the binary snapshot | validate and point | `bridge` |

A flat lexer takes no arena, no key space and no source object.
Everything it needs is an argument, which is why the lexer tests run
against an array and never allocate. Recording into a key space is one
implementation of `cf_entry_sink`; a test's implementation is an array.

## Extension points, with recipes

### Adding a format

1. Write a `cf_codec` in `src/serialize/`. It is an **event producer**,
   never a source: it emits `begin_table`, `begin_array`, `scalar`,
   `end` and knows nothing about keys. One shared flattener owns key
   construction, array counting and location formatting.
2. Register it in `src/serialize/codec.c` behind its `CF_WITH_*` option,
   and add the format to `cf_formats[]` there **even if it is not
   compiled in**, so a file the build cannot read produces an error
   naming the option to turn on rather than "unknown format".
3. Add the option to `cmake/ConfigForgeOptions.cmake` and to
   `cf_version.h`'s feature list.
4. Add the source file to `scripts/amalgamate.py`.
5. Tests: a round trip through the key space digest, and one refusal per
   grammar mistake the format can make. `tests/unit/test_toml.c` is the
   model: it prints what was written and both configurations when the
   digests disagree, which is how the TOML round trip found the
   emitter writing arrays after tables.
6. Give `tests/unit/test_fuzz.c` a corpus that reaches every construct,
   and feed the codec from nothing as well. A reader of untrusted bytes
   that is not fuzzed is not finished.

**Through another library's parser.** A streaming parser elsewhere -
libyaml's events, expat's callbacks - is already the shape a codec is:
its start-mapping, start-sequence, scalar and end events are
`begin_table`, `begin_array`, `scalar` and `end`, and the adapter is a
translation with locations attached. A codec inside the library
belongs behind its own `CF_WITH_*` option and on `docs/Dependencies.md`,
because it brings a dependency the built-in three do not.

A codec can also live **outside** the library, in the program that uses
it: define a `cf_codec` with a `decode` that drives the sink, and pass it
with `cf_builder_file_codec` (C++: `Builder::fileWith`).
`tests/unit/test_codec.c` is a complete one in sixty lines, using only
the public headers. It is handed over explicitly rather than registered,
because a run-time registry would be global mutable state and two parts
of one program could register different codecs under one name.

### Adding a platform capability

What a configuration library needs that C89 has no answer for lives
behind `cf_platform.h`: the environment, UTF-8 arguments, a directory
listing, file stamps, and a handle to wait on for changes. The last was
added this way, and the recipe held:

1. Declare the neutral function and a `cf_platform_has_*` query in
   `cf_platform.h`. The query matters: a caller has to be able to say
   "this build cannot do that" instead of returning an empty result,
   because an empty directory listing looks exactly like an empty
   configuration and the silent answer is the dangerous one.
2. Implement it in all three of `posix/`, `win32/` and `none/`. The
   `none` implementation is not a placeholder — it is the answer for a
   target with no operating system, and it must fail with a message
   naming the capability.
3. Never name a platform macro outside `src/platform/`;
   `gate.detection` fails the build if you do.

Each implementation guards its whole body with its own `CF_PLATFORM_*`
macro. That single guard is not a scattered `#ifdef`: the build normally
compiles one file, and the guard exists so the amalgamation — one
translation unit with no build to choose for it — can carry all three.

### Writing a configuration out

Restricted, on purpose, to a file ConfigForge owns and rewrites in full.
Editing a hand-written file in place means preserving its comments, its
ordering and its whitespace: the parser would have to keep a concrete
syntax tree that every codec then maintains, and getting it slightly
wrong silently destroys something a person wrote.

```text
CF_EMIT_ALL           every resolved key, defaults included
CF_EMIT_CONFIGURED    what a source configured; the overrides file
CF_EMIT_FROM_RANK     what one rank supplied
```

Three rules the implementation encodes, each of which was a decision:

* **"Configured" means "did not come from the defaults source"**, not
  "differs from the default value". The two part company when someone
  sets a value that happens to equal the default: comparing values would
  drop it and let a later release move it, while provenance keeps the
  intent that was expressed.
* **An array is all or nothing.** Emitting only the members that pass a
  filter produces a shorter array, which is a different configuration
  rather than a subset of one.
* **A format with no array syntax refuses before writing anything.**
  Discovering it halfway leaves a truncated file that looks complete.

### Adding a flat source

1. Write the lexer in `src/sources/entry.c`, taking
   `(input, cf_keymap, cf_entry_sink, cf_error)`. No arena, no key
   space.
2. If the source spells keys its own way, add a `cf_keymap_kind` and
   implement **both** directions. The reverse direction is what lets a
   definition set be asked "what would this source call this key", which
   is how the environment gained its natural spelling.
3. Write the thin `cf_source_*` wrapper that builds a `cf_keysink` and
   calls the lexer. Its vtable's `write` is NULL: only a store can be
   written (D-42), and a source that can be must write through
   `cf_source_path`, so a derived configuration's stand-in for it -
   which has the path and none of the state - writes to the same place.
4. Tests: the lexer against a recording sink, and the source once
   end-to-end.

### Adding a definition attribute

`cf_def` carries a `size` field, so growing it is additive. Add the
field, handle it in `def_check` if it can contradict something, in
`validate_value` if it constrains a value, and in the report's
`report_definition`. Bump nothing: the struct grows, the signatures do
not.

### Adding an error code, a value type, or an outcome

Add one line to `include/configforge/cf_*.def`. The enum, the names and
the descriptions all come from that line, and `-Wswitch-enum` makes
every switch over the enum exhaustive, so nothing can be added and
silently forgotten. Put the last entry through `CF_*_LAST_X` so the enum
expansion has no trailing comma, which is C99.

### Adding a Doctor rule

Rules live in `tools/doctor/doctor.c`. A rule earns its place by finding
something nothing else reports. Being shadowed once is precedence
working as designed; a source that supplies values and wins **none** of
them means somebody is editing a file that has no effect, and that is a
finding.

## Where things live

```text
include/configforge/   the public interface, and the .def X-macro lists
src/                   the library, one directory per layer
cmake/                 modules, included by CMakeLists
cmake/gates/           gate scripts, run by ctest
scripts/               developer scripts: verify, amalgamate
tools/                 programs that ship: Doctor
tests/unit/            one program per module
tests/consumer/        a project that knows only the installed package
tests/doctor/          fixtures generated by a program, not checked in
examples/{c,cpp}/      built and run by the suite, so they cannot rot
docs/                  this, the conventions, the specification, decisions
```

Two lines are worth stating because they are otherwise a matter of
habit. **`tools/` is what ships and `scripts/` is what builds or checks**
— Doctor is a program a user runs, the amalgamator is not. And the gate
scripts are separated from the CMake modules because the modules are
*included* by `CMakeLists.txt` while the gates are *run* by ctest; two
kinds of file in one directory is how a directory stops telling you
anything.

## The gates

A claim the build does not check is a claim that decays. Each gate
exists because something in the design is otherwise only a sentence.

| Gate | Protects |
| ---- | -------- |
| `gate.version` | `cf_version.h` and `CMakeLists.txt` agreeing |
| `gate.layering` | the dependency rules above |
| `gate.detection` | compiler detection only in `cf_port.h`, platform detection only in `cf_platform.h` and `src/platform/` |
| `gate.headers` | every public header compiling standalone, C89 and C++98 |
| `gate.amalgamation` | the no-build-system path compiling and running |
| `gate.symbols` | no locale-sensitive libc function; every export prefixed; no mutable global data; only `alloc.c` calls the allocator |
| `gate.interface` | what including the library costs a consumer |
| `gate.install` | that anyone can actually use it: install, `find_package`, compile, run |
| `gate.formats` | every format string against what `cf_fmt` supports |
| `gate.apiref` | `docs/API.md` regenerated from the header comments, and equal to the committed one |
| `gate.format.c`, `gate.format.cmake`, `gate.cmake.lint` | one style, checked not agreed |
| conformance | `-std=c89 -pedantic -Werror` plus the D-32 warning set |
| portability | the whole suite under `CF_FORCE_PORTABLE`, byte-identical |
| stack budget | every library frame measured (`-fstack-usage`) against `CF_STACK_BUDGET` after the link, by `scripts/stack.cmake`; the builder sets the number (D-37), the build proves it |

Every gate has been proved by breaking it on purpose at least once. A
gate that has never failed is a gate nobody knows works.

## Run time

The complexity claims are shapes rather than numbers, so they are
checked as shapes: measured at one size and at four times that, where
`O(n log n)` and `O(n squared)` are about 4.3x and 16x apart and no
amount of machine noise closes the gap.

```text
resolve       4,000 keys   5.7 ms      16,000 keys  27.0 ms    4.7x
lookup        200,000 over 4,000 keys  66.7 ms
              200,000 over 16,000 keys 72.9 ms                 1.09x
arena         3.3 MiB                  13.3 MiB               4.0x
read          52 ns each, unoptimised
```

Four times the candidates cost about four times the work, not sixteen:
resolution is still one merge sort rather than an insertion per key. The
same number of lookups over four times the keys costs nine percent more,
which is a binary search paying one extra comparison per doubling. And
the arena grows in proportion, so no per-key allocation has crept back
in.

`test_scale` checks that a large configuration is *correct*;
`test_complexity` checks that it is correct *quickly*. A regression from
a search to a scan would have passed every other test in the suite while
turning a pause into a hang.

## Compile time

Measured rather than assumed, single-threaded, on the machine this was
written on:

```text
whole project, clean        2.9 s     library, tests, examples, Doctor
one source file             37-56 ms
the public interface         3 ms     over an empty translation unit
amalgamation, one TU       395 ms     8,700 lines
```

Three milliseconds is about what one standard header costs, and it is
not an accident: the public headers reach for `stddef.h`, `stdarg.h` and
`stdint.h` and nothing else. Everything that needs `stdio.h` — reading a
file, listing a directory — is behind an implementation file, so a
consumer who never reads a file never pays for the ability.

It is also one careless `#include` away from being untrue, and the
person who adds `<stdio.h>` to a public header will never see the cost
while every consumer will. So `gate.interface` checks both the include
list and the preprocessed size. Nothing here needed optimising; what it
needed was protecting.

## What is deliberately not here

* **A DOM.** The key space is flat and sorted; that is what makes a C
  implementation reasonable, provenance per key trivial, and the binary
  image a picture of memory rather than a serialisation.
* **Streaming parsers.** Configuration files are kilobytes. Streaming
  doubles the code, needs a pushback buffer, and gives up both zero-copy
  slices and exact locations, to buy bounded memory over a document
  nobody has.
* **Value interpolation.** `${OTHER}` makes a value's origin a set of
  origins plus a template, which turns `explain()` recursive. Reserved
  in the syntax, rejected with a clear message, not implemented.
* **A second document binary format.** CBOR and MessagePack give a
  document, which JSON already gives; the gain is bytes, which is not
  the problem. The binary artifact that was needed is the image.
