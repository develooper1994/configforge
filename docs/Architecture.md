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
                    |
              portability       cf_port.h: the only file with #ifdef
```

| Layer | Owns | May use | May not use |
| ----- | ---- | ------- | ----------- |
| `src/core` | value model, key space, definitions, validation, `cf_config` | portability | anything above it |
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
| Hierarchical | JSON, INI, later TOML and XML | parsing, then flattening | `serialize` + `bridge` |
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
   grammar mistake the format can make.

### Adding a flat source

1. Write the lexer in `src/sources/entry.c`, taking
   `(input, cf_keymap, cf_entry_sink, cf_error)`. No arena, no key
   space.
2. If the source spells keys its own way, add a `cf_keymap_kind` and
   implement **both** directions. The reverse direction is what lets a
   definition set be asked "what would this source call this key", which
   is how the environment gained its natural spelling.
3. Write the thin `cf_source_*` wrapper that builds a `cf_keysink` and
   calls the lexer.
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
tests/doctor/          fixtures generated by a program, not checked in
examples/              built and run by the suite, so they cannot rot
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
| `gate.detection` | `#ifdef` appearing only in the portability layer |
| `gate.headers` | every public header compiling standalone, C89 and C++98 |
| `gate.amalgamation` | the no-build-system path compiling and running |
| `gate.symbols` | no locale-sensitive libc function; every export prefixed; no mutable global data; only `alloc.c` calls the allocator |
| `gate.format.c`, `gate.format.cmake`, `gate.cmake.lint` | one style, checked not agreed |
| conformance | `-std=c89 -pedantic -Werror` plus the D-32 warning set |
| portability | the whole suite under `CF_FORCE_PORTABLE`, byte-identical |
| stack budget | `-Wstack-usage=2048`, because "runs on small targets" is a number |

Every gate has been proved by breaking it on purpose at least once. A
gate that has never failed is a gate nobody knows works.

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
