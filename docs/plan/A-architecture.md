# The layers, decided before they are built

The abstractions the project will grow into, decided together on
2026-11-19 so that each card that builds one does not have to design
it (docs/Plan.md says what a card is and how one is carried out). The
first part is the design: what each layer is, what it may use, and why.
The second part is the cards of the 1.8.0 milestone that start it
(docs/Maintaining.md, "Versions"). Cards for the later milestones are
written when the milestone before them ends; they follow this design
and do not reopen it.

D-numbers: this file uses **D-100 to D-108**. Check with
`git grep -n "^## D-10[0-8]" docs/Decisions.md` before writing one; if
a number is taken, take the next free one and change it in every place
of this file in the same commit.

---

## The design

### D-100 - One directory per component

Today `src/` holds six layer directories and `include/configforge/`
holds every public header side by side. From 1.8.0 each component is a
directory of its own under `lib/`, with its own public headers, sources
and build file:

```text
lib/port/        cf_port.h, the build options: which compiler, which limits
lib/lanes/       the vector library (D-101); depends on nothing of ConfigForge
lib/numeric/     numbers read and written (D-102)
lib/base/        what every component shares: alloc, error, value
lib/text/        UTF-8, escapes, character classes, locations (D-102)
lib/platform/    the operating system: posix, win32, none
lib/serialize/   the formats, codecs and document events
lib/config/      the configuration library: core, sources, bridge, diag
lib/cxx/         configforge.hpp and the compile-time layer (D-104)
```

* Each component is `lib/<name>/include/configforge/*.h` (its public
  headers), `lib/<name>/src/` (its sources and internal headers) and
  `lib/<name>/CMakeLists.txt`. `lib/config/src/` keeps today's four
  layers as subdirectories (`core/`, `sources/`, `bridge/`, `diag/`),
  since their rules are already held by `gate.layering`.
* **The installed headers do not move.** Every component installs into
  the same `include/configforge/`, so `#include <configforge/cf_x.h>`
  keeps working and nothing breaks for a program. The move is of the
  source tree only. `lib/lanes/` is the exception: it installs its own
  `include/lanes/`, as a library that is not ConfigForge's.
* **What may use what** is a table in `cmake/ConfigForgeLayers.cmake`,
  read by `gate.layering`: an include from one component into another
  that the table does not allow fails. The table:

  | Component | May use |
  | --------- | ------- |
  | port | - |
  | lanes | - (its own portability header, not cf_port.h) |
  | numeric | port, lanes |
  | base | port, numeric (`cf_value.h` holds numbers) |
  | text | port, numeric, base, lanes |
  | platform | port, numeric, base |
  | serialize | port, numeric, base, text, lanes, platform's file opener (`cf_platform_file.h`) |
  | config | everything above |
  | cxx | config and everything it uses |

* The libraries stay what they are: `configforge_serialize` is made of
  port, numeric, base, text, lanes, the platform's file opener and
  serialize; `configforge` of all of them. A component is a directory
  and a build file that adds its sources to those two targets, as
  `src/<layer>/CMakeLists.txt` does today, not a target of its own:
  the libraries, their symbol lists, the version macros and `gate.abi`
  are unchanged by the move, and `gate.layering` is what fails a build
  that breaks the table.
* Tools that need every public header in one directory (abidiff, the
  CBMC proofs, `check_include_file` tests) read a staged copy the
  configure step makes, `${CMAKE_BINARY_DIR}/staged/include/`.
* Unit tests stay in `tests/` as they are; only `lib/lanes/tests/`
  lives inside its component, since it travels with that library.
* Rejected: top-level directories per component (they mix with `docs/`,
  `tools/` and `tests/`); moving the installed headers into
  `configforge/<component>/` (a break for every program, for no gain to
  it); a CMake target per component (each would need its own symbol
  list and export rules, for nothing `gate.layering` does not already
  hold).

### D-101 - The vector library, not ConfigForge's

The word-at-a-time and SIMD work (docs/plan/P-word-at-a-time.md) is a
library of its own, usable by any C program, and ConfigForge is one of
its users. Its name is **lanes**: prefix `lanes_`, directory
`lib/lanes/`, headers under `include/lanes/` (the maintainer,
2026-11-19; `wordlane` and `swarmi` were the other names weighed).
Short, and it says what a vector is.

Checked on 2026-11-19 (docs/Roadmap.md, the SWAR and SIMD item): no
library found combines a C89 SWAR tier, a scalar reference per
operation, C99 SIMD tiers and run-time choice behind one C API. That is
what this one is.

* **Two levels.** (1) **Span operations**, the API a program calls: a
  function over a byte span, such as "the index of the first byte of
  this set". (2) **The lane vocabulary**, used inside each backend:
  portable vector types and operations by lane width,
  `lanes_<op>_<type><count>` (`lanes_add_u32x4`, `lanes_eq_u8x64`), a
  512-bit register seen as 64 of 8 bits, 32 of 16, 16 of 32 or 8 of 64.
  The choice of instruction set is made per span operation, never per
  lane operation: choosing per instruction costs more than it saves,
  which is why Highway also chooses per function.
* **The span operations of the first release**, exactly what the
  readers need and nothing more: `find_byte`, `find_any` (a set of up to
  256 bytes, made once by `lanes_set_init`), `find_lt` (the first byte
  below a value), `ascii_run` (the leading bytes below 0x80),
  `utf8_valid` (the length of the valid UTF-8 prefix), `count_byte`,
  and `digits8` (eight ASCII digits to their value, or "not eight
  digits"). A further operation is added when a user needs it, with its
  scalar form first.
* **Backends.** `scalar` (the reference, C89, always built), `swar32`
  and `swar64` (C89, words read with `memcpy`), `sse2`, `sse42`,
  `avx2`, `avx512` (x86), `neon`, `sve` (Arm). Each SIMD backend is its
  own C99 file compiled with its own flags (`-mavx2` and the like;
  none for MSVC), so the rest of the library stays C89 and a program is
  never built for a processor it may not have.
* **No global mutable state.** A backend is a constant table of
  function pointers, `const lanes_ops*`. The program asks for one and
  keeps it: `lanes_scalar()` always; `lanes_by_isa(LANES_ISA_AVX2)`,
  NULL when it was not built or this processor lacks it (manual
  choice, the first stage); `lanes_best()`, the best built backend this
  processor runs, in the order AVX-512, AVX2, SSE4.2, SSE2 on x86, SVE,
  NEON on Arm, then SWAR at the word width, then scalar (the automatic
  stage). Nothing is detected or chosen behind the caller's back, so
  two threads can never race over a choice.
* **Detection.** At build time from the compiler's macros (architecture,
  byte order, what it can emit). At run time `cpuid` and `xgetbv` on x86
  (the operating system must save the wider registers), the auxiliary
  vector (`getauxval`) on Linux Arm, `IsProcessorFeaturePresent` on
  Windows, NEON assumed on AArch64. Byte order matters only where a
  SWAR mask becomes an index; that step has a form per byte order,
  checked once by `lanes_by_isa` against a known word.
* **Every backend is tested against `scalar`**: every operation, every
  length from 0 to four vectors plus a tail, every starting offset,
  poison bytes after the end that must never be read (sanitizers), and
  a fuzz target comparing every backend the machine runs. CI forces each
  backend in turn (`LANES_FORCE=<name>`); a backend the runner cannot
  run is built and listed as "built only".
* **ConfigForge's use.** In 1.8.0 the readers call `text` and `numeric`,
  which call `lanes_scalar()` only: the abstraction is in place, the
  speed is not yet (2.4.0 adds the other backends and the program's
  choice, `cf_builder_lanes`). The `misra` and `minimal` profiles build
  `scalar` alone.
* **Later, and not ConfigForge's to build**: layers for other kinds of
  data over the same lanes (bytes: hex, base64, checksums; arrays:
  reductions, prefix sums, sorting; images and N-dimensional data:
  strided views, conversions). Named here so the lanes API does not
  shut them out; none is planned.
* It moves to a repository of its own (`git subtree split`) when a
  second project uses it, or at its first release, whichever is first.

### D-102 - One copy of each text and number helper

The readers each carry their own UTF-8 checks, escape decoders, number
readers and whitespace loops (the inventory is under "The helpers
today", below). Each becomes one function in `lib/text/` or
`lib/numeric/`, and a difference between formats becomes a parameter,
never a second copy.

* `lib/text/` is internal (not in the 1.x promise): `cf_text_*`
  functions for UTF-8 (validate, decode one code point, encode one),
  escape decoding by family (the `\u` family of JSON, JSON5 and TOML;
  YAML's; XML's entities; INI and `.properties`' backslashes), character
  classes as one 256-entry table, whitespace and comment skips by
  syntax, and the count of lines and columns behind a `cf_loc`.
* `lib/numeric/` keeps the public `cf_numeric.h` as it is and gains the
  readers' own number code behind it, one integer reader (bases,
  underscores, signs, overflow) and one real reader and writer.
* Both call `lanes` for their scans (D-101), so P-3's "the scans P-1
  found" becomes "the few functions in `lib/text/`", and an
  optimization made once reaches every format.
* Each helper is moved with a test that runs every reader's old input
  through it first; a reader is switched only when the conformance
  suites pass on it unchanged.

### D-103 - Reading what is almost a format

A document that is nearly JSON - `{{a:1}}`, `{a:{2}}`, a sender's own
habits - is read by composing codecs, not by loosening the built-in
ones. Four levels, of which three exist:

1. **A dialect with a standard** is a codec of its own name, as JSON5
   and JSONC are (D-66): `{a:1, b:2}` is JSON5 today.
2. **A whole format of the program's own** is a `cf_codec` the program
   fills and registers with `cf_builder_codec` (the Guide, "Your own
   format"). Exists.
3. **A codec composed from a codec** (new, `cf_codec_compose`): a base
   codec, a name, and either or both of
   * a **repair**, `cf_bool (*)(void* ctx, const cf_blob* in, cf_blob*
     out, cf_error* err)`, run on the bytes before the base reads them
     (`{{a:1}}` to `{a:1}`);
   * a **filter**, which wraps the `cf_doc_sink` the base writes into
     and may rename, drop, merge or retype what passes
     (`{a:{2}}` to `a = 2`), the way a transforming serializer does in
     other languages.
   The composed codec is a `cf_codec` built in the caller's storage on
   the existing `decode_self` and `data` members (D-92), so nothing in
   the pipeline changes. It carries `CF_CODEC_NAMED_ONLY`: it is never
   chosen from content. Its name is the one the program gives
   (`json+repairs`), and every value read through it says so in its
   provenance, so `explain()` shows that a value passed through the
   program's repair.
4. **Recovery inside a lexer** (a callback on an unexpected token) is
   rejected for now: it would make every lexer's internals a promise.
   Reopened only if a user brings a case levels 1 to 3 cannot read.

Two readers disagreeing about one document is an attack (request
smuggling is the web's form of it), so a composed codec is never on by
default, never in the `misra` profile, and its repair and filter are
the program's code, named in every report.

### D-104 - Work moved to build time, in layers

The core stays C89 and is always there. Work that can be done when the
program is built is done then, by whatever the compiler offers, without
the program's source changing with the compiler:

* **Layer 0, the C89 core**: reads, merges and explains at run time.
  Its ABI is the 1.x promise.
* **Layer 1, build-time kits**, each for one language and level, each
  doing its work before the program runs and never calling layer 0:
  * **C, any standard**: X-macro tables and `configforge gen`'s output.
    New: **a baked configuration** - `configforge gen --bake <file>
    --schema S` resolves a configuration when the program is built and
    writes it as C89 `static const` tables, provenance included ("from
    device.toml line 12, at build time"). The program then links no
    reader at all; `cf_config_from_baked()` gives the same `get` and
    `explain` over the tables. For firmware whose configuration is fixed
    at build time this is no parser in the binary, and it is the field
    docs/Market.md found open.
  * **C23**: `#embed` of the file into the program, read at start-up,
    where baking is not wanted.
  * **C++**, header-only, by feature rather than by standard number
    (feature-test macros such as `__cpp_consteval`, never `__cplusplus`
    alone): **tier 11** key names hashed at compile time and checked
    with `static_assert`; **tier 17** a constant key table and a perfect
    hash; **tier 20** a JSON or TOML text parsed at compile time into a
    constant table, and a key name that is not in the contract a compile
    error; **tier 26** `#embed` and static reflection, a struct bound to
    its contract with no generated code.
  * **Rust**: `const fn` and a derive macro, in the Rust binding.
* **Layer 2, the facade**: `configforge.hpp` (in `lib/cxx/`) offers one
  call, such as `cf::load<Settings>(source)`, and does at compile time
  what the compiler's tier allows and the rest through layer 0. The
  program's source is the same under C++11 and C++26; only when the
  work happens changes.
* **The rule that keeps the layers honest**: every result a build-time
  path produces is compared, in the tests, with what layer 0 produces
  from the same input. A build-time result that differs is a bug in
  the build-time path.
* Layers 1 and 2 are header-only or generated, so they have no ABI and
  may follow the newest standards freely; the C ABI promise is not
  touched by them. MISRA C++:2023 applies to `lib/cxx/` (docs/Compliance.md).

### D-105 - Build profiles by name

A profile is a named set of options in `cmake/profiles/<name>.cmake`,
one `set(<option> <value> CACHE ...)` a line, chosen by
`-DCF_PROFILE=<name>` or by `-DCF_PROFILE_FILE=<path>` for one of the
program's own. The profiles: `full`, `default`, `minimal`, `misra`,
`embedded-json`, `embedded-cbor`, `server`. An option given on the
command line wins over the profile. The configure step prints the
profile, each option that differs from it, and the estimated size from
the README's table. The profile's name is written into
`cf_build_options.h` and the build id. Every shipped profile is built
in CI. ("Profile" here is a build's; the platform's file locations
"with profiles" in the Roadmap are a program's, a different thing.)

### D-106 - A newer C only where a file needs it, and said so

C89 stays the floor. A file that needs more - a dependency's header
(libfyaml's C11, CoreFoundation's C99), a SIMD backend (C99) - says so
through one helper, `cf_source_needs_c(<file> <standard> REASON "<why>")`
in `cmake/ConfigForgeCVersion.cmake`, which sets that file's standard,
prints a configure-time warning naming the file, the standard and the
reason, adds it to the build id, and records it in a list that
`gate.cversion` compares with the files actually compiled above C89. A
file above C89 that is not on the list fails; so does an entry with no
reason. The `minimal` and `misra` profiles refuse every entry, by name,
at configure time. Code already written in C89 is never rewritten to a
newer standard unless it has to be.

### D-107 - One refusal, the same in every format

`cf_error` is already the one error of the library (a code, the key,
the source, a message). What it lacks is the place and the reason in a
form a program can test rather than read:

* a `cf_loc where` member (line, column, byte offset), added through
  `size` as D-51 adds members, so 1.x callers are unaffected;
* a `reason`, from a new list `cf_reason.def` beside `cf_code.def`, of
  what was refused, the same name in every reader: `CF_REASON_UTF8`,
  `CF_REASON_DEPTH`, `CF_REASON_NUMBER_RANGE`, `CF_REASON_DUPLICATE_KEY`,
  `CF_REASON_TRUNCATED` and the rest the inventory finds. The code
  stays coarse; the reason is what a test of "the same bad input is
  refused the same way in every format" compares.
* Additive in 1.x. A reader that today puts the place only in the
  message keeps the message as it is and fills `where` too.

### D-108 - Every text is UTF-8 inside; another encoding only by name

Decided by the maintainer on 2026-11-19, after weighing what others do.

**What others do.** Refuse: Rust (`str::from_utf8`, serde_json),
simdjson, yyjson by default, Jackson, libyaml, Python's files
(`errors='strict'` by default), PostgreSQL. Replace with U+FFFD:
browsers (the WHATWG Encoding standard), Go's `encoding/json` v1,
Java's `new String(bytes, UTF_8)`. Pass through: RapidJSON by default.
Escape without loss: Python's `surrogateescape` (PEP 383), Rust's WTF-8
for Windows paths. Go's `json/v2` moved from replacing to refusing by
default. JSONTestSuite leaves invalid UTF-8 to the implementation (its
`i_` cases), so refusing costs no conformance.

**Today** TOML, CBOR and TLV refuse invalid UTF-8; JSON, JSON5, the
built-in YAML reader, INI, `.properties` and dotenv pass the bytes on
("The helpers today").

**The rule.**

1. Inside the library every text is valid UTF-8.
2. A reader refuses bytes that are not, with the line and column of the
   first one. Nothing is replaced and nothing is guessed.
3. Another encoding is read only when the program names it: a
   `CF_ENCODING_*` value in the source's flags, or its name in the
   tools. The bytes are recoded to UTF-8 as they are read, and the
   source says so ("read as windows-1254").
4. A byte-order mark is a declaration, not a guess: UTF-8's is skipped,
   as today; UTF-16's, in either byte order, makes the file UTF-16,
   recoded; UTF-32's is refused by name.
5. A guess is allowed in one place only: a refusal's message may say
   which encoding the bytes look like and how to read them so. It
   never changes a value.
6. A file is converted for good, once, by `configforge codec recode`:
   its bytes recoded and nothing else changed, for a person to review
   as a diff.

**Exceptions, each for a reason.** XML declares its own encoding in the
document (`<?xml ... encoding="..."?>`) and expat reads by that
declaration: a declaration, so kept (`CF_CODEC_SELF_ENCODED`). CBOR's
and TLV's text strings are already checked. The contents of secrets
and credentials directories are opaque bytes the library neither parses
nor prints, so they are not checked; a value type for bytes, which
would hold them properly, is weighed for 2.0.0.

**Why not guess.** Windows-1252 and Windows-1254 differ only at 0xD0,
0xDD, 0xDE, 0xF0, 0xFD and 0xFE, which are exactly Ğ, İ, Ş, ğ, ı and ş.
A short value cannot tell them apart, and a wrong guess gives a wrong
value that looks right (`Þükrü`).

**Why not U+FFFD.** Different bytes become one text: two keys or two
secrets collide, a digest no longer matches the file, a file written
back has changed. Two readers of one file disagree, which is an attack
(Bishop Fox, "An Exploration of JSON Interoperability Vulnerabilities",
2021). Lenient decoding has been exploited before: the overlong `/` of
MS00-078.

**When.** All of it in 1.8.0 (A-4 to A-6), under "### Changed" with
this number: an input refused that was accepted, which
docs/Maintaining.md allows in a minor. One step, not a warning first:
the refusal names the one setting that reads the file again, so a
warning period would only keep an inconsistent and attackable
behaviour longer, while the users who would be broken are still few.

---

## The helpers today

A survey of `src/` on 2026-11-19. No `.h` file exists under `src/`:
everything shared is declared in `include/configforge/`, and
`gate.layering` lets `src/serialize/` reach only the headers in
`CF_SERIALIZE_HEADERS` and the files in `CF_SERIALIZE_LEAVES`
(`cmake/ConfigForgeSerialize.cmake`), which is why helpers were copied
rather than shared. Already shared: the character classes and
`cf_cmp_ascii_ci` (`cf_port.h`), decimal integers and reals read and
written exactly (`core/numeric.c`, `cf_numeric.h`), the BOM
(`cf_blob_skip_bom`, used by five readers), `cf_text_is_utf8` (used by
CBOR and TLV only), the sinks and `cf_loc_text` (`blob.c`).

| Kind | Copies | Lines | Where |
| ---- | -----: | ----: | ----- |
| UTF-8 validate or decode | 4 | ~160 | `blob.c` `cf_text_is_utf8`, `json.c` `js_utf8_at`, `toml.c` `toml_utf8_len`, `yaml.c` `yaml_utf8` |
| UTF-8 encode a code point | 4 | ~95 | `json.c` `js_utf8`, `toml.c` `toml_utf8` (identical), `properties.c` `prop_utf8`, `yamlparse.c` `yp_put_cp` |
| Hex digit value | 5 | ~60 | `json.c` `js_hexval`, `toml.c` `toml_hex`, `properties.c` `prop_hex`, `yamlparse.c` inline in `yp_escape` and `yp_tag_put` |
| Seeded hash for a reader's table | 4 | ~95 | `toml.c`, `ini.c`, `xml.c`, `yaml.c`: `*_seed` and `*_hash`, identical but for the type |
| Escape decoding in strings | 4 + 4 | ~550 | JSON and JSON5, TOML, YAML, properties; `%XX` in YAML tags; `${` in `validate.c`, `emit.c`, `cbor.c` |
| Escaping when writing | 4 | ~120 | `blob.c` `cf_sink_json_string`, `toml.c` `toml_quote`, `yaml.c` `yaml_quote`, `ini.c` `ini_text_form` |
| Number scanners before the shared reader | 4 | ~300 | `js_number`, `js5_number`, `js5_hex`, `toml_number` (own base-N reader) |
| The scalar writer's switch on type | 5 | ~140 | `blob.c`, `json.c`, `ini.c`, `toml.c`, `yaml.c` |
| Whitespace, comments, newlines | ~14 | ~330 | every text reader; `ini_trim` and `dotenv_trim` identical |
| Line and column, and the failure message | ~9 + ~9 | ~100 | every reader; byte columns |
| Key-name check | 2 | 64 | `tlv.c` `tlv_key_valid`, a copy of `keyspace.c` `cf_key_valid` the layer could not reach |
| Growable tables, workspace checks | ~20 | ~200 | `xml_grow`, `yaml_grow`, `toml_room`, `ini_names_grow`; `*_has_text` checks in seven readers |
| Scans for one of several bytes | ~35 loops | - | the candidates for `lanes` (D-101): `js_string`, `toml_string`, the comment and line scans |

Differences the cards must keep or decide, not lose:

* **Invalid UTF-8.** TOML, CBOR and TLV refuse it; JSON, JSON5, the
  built-in YAML reader, INI, `.properties` and dotenv pass it through
  (`js_string` copies raw bytes; `yp_printable` takes every byte from
  0x80). Decided by D-108: every text reader refuses it (A-5), another
  encoding is read only by name (A-4).
* **Newlines.** JSON counts LF only; JSON5 also a lone CR and U+2028/9;
  TOML takes CR only before LF; INI treats CR as a blank; YAML and
  `.properties` take CR, LF and CR LF. Each stays its format's rule, as
  a parameter of one function.
* **Whitespace sets** differ by format the same way (INI: space, tab,
  CR; `.properties`: space, tab, form feed; TOML: space, tab).
* **Writers** differ on DEL, `` and `` (`cf_sink_json_string`
  against `toml_quote`): kept per format; the shared code takes the set
  to escape as a parameter.
* **Hex output** is lowercase except `tlv_hex`, which is uppercase.

---

## The cards of 1.8.0

Order: A-1, A-2, A-3, A-4, A-5, A-6, then A-7 to A-11 in their order. A-1 moves every
file, so nothing else of 1.8.0 runs beside it; A-2 touches only
`lib/lanes/` and could run beside A-3, but A-3 needs it. Every card is
accepted on Linux and Windows and in CI. The CHANGELOG: under
`## 1.8.0 — unreleased`; if it is not open yet, the card's first step
opens it with `cmake -DTO=1.8.0 -P scripts/release/version.cmake`
(docs/Maintaining.md, "Versions").

---

## A-1 - One directory per component: the files moved, nothing else changed

* **id**: A-1
* **goal**: every file under `src/` and `include/configforge/` is under
  `lib/<component>/` as the table below maps it, and the build, the
  install, the gates, the scripts, the bindings, the packaging, CI and
  the documents read the new places. The installed file list, the
  libraries' symbols and every test's result are what they were.
* **why**: D-100. A reader of the tree finds a component in one
  directory, and the layering is a table a gate holds, not a habit.
* **needs**: 1.7.0 released
* **size**: large
* **decided**:
  * **First, one include.** `src/core/port.c:8` includes `cf_numeric.h`
    and uses nothing of it: take it out before the move, so port uses
    nothing, as the table says.
  * **The map.** `git mv`, file by file, so history follows:

    | From | To |
    | ---- | -- |
    | `src/core/port.c` | `lib/port/src/port.c` |
    | `include/configforge/cf_port.h`, `cf_build_options.h` | `lib/port/include/configforge/` |
    | `src/core/numeric.c` | `lib/numeric/src/numeric.c` |
    | `include/configforge/cf_numeric.h` | `lib/numeric/include/configforge/` |
    | `src/core/alloc.c`, `error.c`, `value.c` | `lib/base/src/` |
    | `include/configforge/cf_alloc.h`, `cf_error.h`, `cf_code.def`, `cf_value.h`, `cf_type.def` | `lib/base/include/configforge/` |
    | `src/platform/{none,posix,win32}/*.c` | `lib/platform/src/{none,posix,win32}/` |
    | `include/configforge/cf_platform.h`, `cf_platform_file.h` | `lib/platform/include/configforge/` |
    | `src/serialize/*.c` | `lib/serialize/src/` |
    | `include/configforge/cf_serialize.h`, `cf_serialize_version.h`, `cf_yamlparse.h` | `lib/serialize/include/configforge/` |
    | `src/core/{config,define,edit,iterate,keyspace,live,validate,version}.c` | `lib/config/src/core/` |
    | `src/sources/*.c`, `src/bridge/*.c`, `src/diag/*.c` | `lib/config/src/{sources,bridge,diag}/` |
    | every other `include/configforge/*.h`, and `cf_outcome.def` | `lib/config/include/configforge/` |
    | `include/configforge/configforge.hpp` | `lib/cxx/include/configforge/` |
    | `include/configforge/cf_doctor.h` | `tools/doctor/include/configforge/` |

    Each `src/<dir>/CMakeLists.txt` moves with its sources and keeps its
    lists (they name files through `${CMAKE_CURRENT_SOURCE_DIR}`). The
    core's list is split between `lib/port`, `lib/numeric`, `lib/base`
    and `lib/config/src/core`. `src/` and `include/` are empty and
    removed.
  * **CMake.** `add_subdirectory(src)` (CMakeLists.txt:99) becomes
    `add_subdirectory(lib)`; `lib/CMakeLists.txt` adds `port`,
    `numeric`, `base`, `platform`, `serialize`, `config`, `cxx` in that
    order. Each component's build file adds
    `$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>` as a PUBLIC
    include directory of `configforge`, and, for port, numeric, base,
    platform and serialize, of `configforge_serialize` too; the two
    `${CMAKE_CURRENT_SOURCE_DIR}/include` lines at CMakeLists.txt:63 and
    :85 go. `tools/doctor/include` is a PUBLIC include directory of
    `configforge_doctor`. The per-file standards move with their files:
    CMakeLists.txt:109 (`yaml.c`, C11) and :126-128 (`posix.c`, C99 on
    macOS). `CF_SERIALIZE_LEAVES` (cmake/ConfigForgeSerialize.cmake:3-11)
    names the leaves' new paths relative to `lib/`;
    `CF_SERIALIZE_HEADERS` keeps names only. `src/CMakeLists.txt`, whose
    loop adds the serialize leaves to `configforge_serialize` and picks
    the platform's `file.c` by a `^platform/` pattern, becomes part of
    `lib/CMakeLists.txt`, its paths and pattern changed to
    `lib/platform/src/`.
  * **The staged headers.** At configure time
    `cmake/ConfigForgeStaged.cmake` (new) copies every
    `lib/*/include/configforge/*` and the generated `cf_build_options.h`
    into `${CMAKE_BINARY_DIR}/staged/include/configforge/` with
    `configure_file(... COPYONLY)`, so a tool that wants one directory
    has one: the source default `cf_build_options.h` first, the
    generated one over it, so the generated one wins, as `BEFORE` makes
    it win in the build (cmake/ConfigForgeOptions.cmake:207-208).
    Readers of it: `scripts/verify.cmake:549-550`, abidiff's
    `--headers-dir2` only (this build's); `--headers-dir1` is the base
    release's, `${base}/include/configforge` while the base has the old
    layout, else the base build's staged directory.
    `scripts/check/proofs.cmake` gets a `-DSTAGED=<dir>` argument (its
    callers: grep for `proofs.cmake`), checks
    `${STAGED}/configforge/configforge.h` (today :12) and passes
    `-I ${STAGED}` (today :28). `tests/unit/CMakeLists.txt:357`
    (`CMAKE_REQUIRED_INCLUDES`). `scripts/verify.cmake:302`, the path of
    `cf_build_options.h` that decides a fresh start, follows the move.
  * **The install.** `install(DIRECTORY include/ ...)`
    (cmake/ConfigForgeInstall.cmake:47-50) becomes one
    `install(DIRECTORY lib/<c>/include/ DESTINATION
    ${CMAKE_INSTALL_INCLUDEDIR} PATTERN "cf_build_options.h" EXCLUDE)`
    per component and one for `tools/doctor/include/`. The installed
    tree must list exactly the same files as before (step 1 records it).
  * **The gates.** Every glob of `include/configforge/*.h` becomes
    `lib/*/include/configforge/*.h` (plus `tools/doctor/include/...`
    where `cf_doctor.h` was read), every `src/*.c` glob `lib/*/src/*.c`
    recursively. The places: GateAbi.cmake:42, 78-79, 83;
    GateAmalgamation.cmake:222, 242; GateDetection.cmake:7-9, 35 (its
    exemption of `/src/platform/` becomes `/lib/platform/`);
    GateFormats.cmake:19; GateHeaders.cmake:5, 48, 106;
    GateInstall.cmake:121, 136, 195, 241; GateInterface.cmake:15, 39;
    GateVersion.cmake:21; GateDependencies.cmake:16-17 (the component
    build files); cmake/ConfigForgeGates.cmake:71 (gate.layering's
    `-DSRC` becomes `-DLIB=${CMAKE_SOURCE_DIR}/lib`).
  * **gate.layering**, rewritten on D-100's table. The table is
    `cmake/ConfigForgeLayers.cmake` (new): one
    `set(CF_LAYER_<component> <allowed components>)` a line, exactly
    D-100's. The gate maps each public header to the component whose
    `include/` holds it, each source to the component it is under, and
    fails an include whose header's component is neither the file's own
    nor allowed for it. It keeps today's four rules, renamed to the new
    paths: `lib/config/src/core` includes none of `cf_serialize.h`,
    `cf_flatten.h`, `cf_source.h`; `core`, `sources` and `serialize`
    include neither `cf_flatten.h` nor `cf_image.h`; the serialize
    sources and leaves include only `CF_SERIALIZE_HEADERS` and `.def`
    files. An entry of the table may be one header rather than a whole
    component, `<component>:<header>`, for serialize's
    `platform:cf_platform_file.h`. An include of `<lanes/...>` or
    `"lanes/..."` belongs to `lanes`. A violation the move itself shows
    means the file is in the wrong component: move it one component up
    and write it in the notes; never widen the table.
  * **The scripts.** `scripts/generate/amalgamate.py`: `SOURCES`
    (:57-71) and `HEADERS` (:21-46) name the new paths, `check_complete`
    (:189-216) walks `lib/*/src` and `lib/*/include/configforge`, and
    its one `incdir` (:237, :252) becomes a list, every
    `lib/<c>/include` in the map's order, which `resolve` and
    `strip_header` search in turn; the openers (:280) read the new
    paths.
    `scripts/generate/apiref.cmake`: :27 and :76 glob the new places.
    `scripts/format.cmake:43-62, 83` and `cmake/ConfigForgeFormat.cmake:10-45`:
    the globs. `scripts/check/proofs.cmake` and the four harnesses'
    `cbmc-sources:` lines (`tests/proofs/proof_fmt.c:8`,
    `proof_infer.c:6`, `proof_int_to_text.c:11`,
    `proof_text_to_int.c:9`): `lib/port/src/port.c`,
    `lib/numeric/src/numeric.c`. `scripts/verify.cmake:790`: gcovr's
    `--filter lib/`. `scripts/release/version.cmake:60, 68` and
    `cmake/ConfigForgeVersion.cmake:14`: the version headers' new paths.
    `scripts/release/package_rust.cmake:24-25`: copies `lib` instead of
    `include` and `src`.
  * **Bindings and packaging.** `bindings/rust/build.rs`: the root's
    `lib/` (or `native/lib/` packaged), every `lib/*/include` passed to
    `.include()`, every `*.c` under `lib/*/src` collected as it collects
    `src` today, the assertion on
    `lib/config/include/configforge/configforge.h`, `rerun-if-changed`
    on `lib`. `bindings/rust/Cargo.toml:12` unchanged (`native/**`).
    `conanfile.py:54-55` exports `lib/*`; :59 reads
    `lib/config/include/configforge/cf_version.h`. vcpkg needs nothing.
  * **CI.** `.github/workflows/ci.yml:364-366`: clang-format over
    `lib/*/src`, `lib/*/include/configforge`, `tools/doctor/include`.
    `.clang-tidy`'s `HeaderFilterRegex` still matches and stays.
  * **Documents.** `docs/Architecture.md` ("Layers" and "Where things
    live") and `CONTRIBUTING.md:23-29` describe `lib/`; `README.md:391`
    the rule; `docs/API.md` is regenerated by its script (gate.apiref);
    the open cards (T-3, T-4, W5-*, P-*) have their `src/` and
    `include/` paths changed; done cards are history and stay.
* **files**: everything named above; last: `docs/Architecture.md`,
  `CONTRIBUTING.md`, `README.md`, `docs/API.md`, `CHANGELOG.md`.
* **steps**:
  0. If `## 1.8.0 — unreleased` is not open, open it now, before any
     move: `cmake -DTO=1.8.0 -P scripts/release/version.cmake` rewrites
     the version headers at their old paths. Commit that alone.
  1. Before any change: build and install the default preset into
     `$HOME/a1-before`, and save `find $HOME/a1-before -type f | sort`
     and `nm -g --defined-only` of both libraries to files outside the
     tree.
  2. The `git mv`s of the map, the component build files and
     `lib/CMakeLists.txt`; `cmake --preset default && cmake --build
     --preset default` passes.
  3. The staged headers and the install; install into `$HOME/a1-after`;
     the file list equals step 1's, and so do both symbol lists.
  4. gate.layering on the table: first break it on purpose (an include
     of `cf_source.h` in `lib/serialize/src/json.c`), see it fail, take
     the include out.
  5. The other gates, the scripts, the bindings, packaging, CI; `ctest
     --preset default` passes.
  6. The documents; ACCEPT-LINUX, ACCEPT-WINDOWS.
* **accept**: ACCEPT-LINUX with
  `-DEXTRA="proofs;abi;coverage;conformance;shared"` (each reads paths
  this card moves), ACCEPT-WINDOWS, ACCEPT-CI with the cargo job;
  step 3's two comparisons equal.
* **stop if**: something reads `src/` or `include/` that is not named
  here: add it, name it in the notes, go on. The installed file list
  differs and the difference cannot be removed without a change of
  behaviour: stop and report.
* **docs**: add **D-100 to D-108** to `docs/Decisions.md`, all nine,
  from the design above: they are decided now (2026-11-19), so the
  index has no gap while they are built. Each entry ends with a line
  "Built: A-n" naming the card that builds it (D-100 A-1; D-101 A-2;
  D-102 A-3, A-7 and A-8; D-103 A-9; D-104 later milestones; D-105 and
  D-106 A-11; D-107 A-10; D-108 A-4 to A-6), and the card that builds
  it adds its commit there. CHANGELOG `### Changed`: "The source tree: one
  directory per component under `lib/` (D-100); the installed headers
  and libraries are unchanged."
* **notes**:

---

## A-2 - The vector library, its interface and its scalar reference

* **id**: A-2
* **goal**: `lib/lanes/` builds a C89 library with the span operations
  of D-101, a `scalar` backend, the processor's detection and the
  choice of backend, tested on its own; nothing of ConfigForge uses it
  yet.
* **why**: D-101. The interface and the reference come first, so that
  A-3 can call it and the SWAR and SIMD backends (2.4.0) only add
  tables.
* **needs**: A-1
* **size**: medium
* **decided**:
  * Files: `lib/lanes/CMakeLists.txt`, `lib/lanes/include/lanes/lanes.h`,
    `lib/lanes/src/lanes.c` (selection and detection),
    `lib/lanes/src/scalar.c`, `lib/lanes/tests/test_lanes.c`,
    `lib/lanes/tests/fuzz_lanes.c`, `lib/lanes/tests/CMakeLists.txt`,
    `lib/lanes/README.md` (what it is, its API, that it is not
    ConfigForge's), `lib/lanes/LICENSE` (the project's MIT text).
  * It includes no `configforge/` header (the table says `lanes` may use
    nothing) and defines its own export macro, `LANES_API`, as
    `cf_port.h` defines `CF_API`, and its own
    `LANES_VERSION_MAJOR/MINOR/PATCH`, 0.1.0.
  * `lanes.h`, exactly:

    ```c
    typedef struct lanes_set { unsigned char bits[32]; } lanes_set;
    LANES_API void lanes_set_init(lanes_set* s, const unsigned char* bytes, size_t n);
    #define LANES_SET_HAS(s, b) (((s)->bits[(unsigned char)(b) >> 3] >> ((b) & 7)) & 1)

    enum {
        LANES_ISA_SCALAR = 1u << 0, LANES_ISA_SWAR32 = 1u << 1,
        LANES_ISA_SWAR64 = 1u << 2, LANES_ISA_SSE2 = 1u << 3,
        LANES_ISA_SSE42 = 1u << 4, LANES_ISA_AVX2 = 1u << 5,
        LANES_ISA_AVX512 = 1u << 6, LANES_ISA_NEON = 1u << 7,
        LANES_ISA_SVE = 1u << 8
    };

    typedef struct lanes_ops {
        size_t      size;   /* sizeof(lanes_ops) where it was compiled */
        const char* name;   /* "scalar", "swar64", "avx2", ... */
        unsigned    isa;    /* one LANES_ISA_* bit */
        size_t (*find_byte)(const unsigned char* p, size_t n, unsigned char c);
        size_t (*find_any)(const unsigned char* p, size_t n, const lanes_set* s);
        size_t (*find_lt)(const unsigned char* p, size_t n, unsigned char c);
        size_t (*ascii_run)(const unsigned char* p, size_t n);
        size_t (*utf8_valid)(const unsigned char* p, size_t n);
        size_t (*count_byte)(const unsigned char* p, size_t n, unsigned char c);
        int    (*digits8)(const unsigned char* p, unsigned long* value);
    } lanes_ops;

    LANES_API const lanes_ops* lanes_scalar(void);
    LANES_API const lanes_ops* lanes_by_isa(unsigned isa);
    LANES_API const lanes_ops* lanes_best(void);
    LANES_API unsigned lanes_cpu(void);    /* LANES_ISA_* bits this processor runs */
    LANES_API unsigned lanes_built(void);  /* LANES_ISA_* bits compiled in */
    ```

  * Meanings, which the scalar code and the tests hold to: the `find_*`
    return the index of the first match, or `n` for none; `find_lt` is
    the first byte `< c`; `ascii_run` the number of leading bytes below
    0x80; `utf8_valid` the length of the longest prefix that is whole,
    valid UTF-8 (no overlong form, no surrogate D800-DFFF, nothing above
    U+10FFFF; a sequence cut off by `n` is not in the prefix);
    `count_byte` the number of bytes equal to `c`; `digits8` reads
    exactly eight bytes, returns 1 and stores their value when all are
    `'0'`-`'9'`, else returns 0 and leaves `*value` alone. A pointer may
    be NULL when `n` is 0. No function reads at or past `p + n`.
  * In this card only `scalar` is built: `lanes_built()` is
    `LANES_ISA_SCALAR`; `lanes_by_isa` returns the scalar table for
    `LANES_ISA_SCALAR` and NULL for every other bit; `lanes_best()`
    returns the scalar table. `lanes_cpu()` is real now: `cpuid` and
    `xgetbv` on x86 (`__cpuid`, `__cpuidex`, `_xgetbv` under MSVC;
    `<cpuid.h>` and inline `xgetbv` under GCC and Clang), AVX2 and
    AVX-512 counted only when `xgetbv` says the system saves their
    registers; `getauxval(AT_HWCAP)` on Linux Arm; NEON on any AArch64;
    the scalar and SWAR bits always. A test prints it, so CI's logs show
    what each runner has.
  * `scalar.c` is plain loops, written for clarity, not speed: it is the
    reference every later backend is compared with.
  * The tests: each operation on lengths 0 to 70, at offsets 0 to 15, in
    a buffer whose bytes after `n` are poison (0xA5), under the sanitize
    preset too; `find_*` with every byte value as the target;
    `utf8_valid` on a table of valid and invalid sequences (each class:
    overlong, surrogate, above U+10FFFF, truncated, stray
    continuation); `digits8` with a non-digit at each of the eight
    places. `fuzz_lanes.c` compares each operation of `lanes_best()`
    with `lanes_scalar()` (equal today; it is the harness 2.4.0 needs).
  * CMake: `lib/lanes/lanes_sources.cmake` sets `LANES_SOURCES` (the
    `.c` files); `lib/lanes/CMakeLists.txt` builds from it a static
    library `lanes` usable alone (`add_subdirectory(lib/lanes)` in
    another project must work: no variable of ConfigForge's is read),
    and its tests when `BUILD_TESTING` is on. ConfigForge does not use
    it in this card; A-3 compiles `LANES_SOURCES` into its two
    libraries rather than linking the archive.
  * **Embedded under ConfigForge's prefix.** `lanes.h` ends with a block,
    as zlib's `Z_PREFIX` does: `#ifdef LANES_PREFIX_CF` then one
    `#define lanes_x cf_lanes_x` for every public name (`lanes_set_init`,
    `lanes_scalar`, `lanes_by_isa`, `lanes_best`, `lanes_cpu`,
    `lanes_built`), so callers write `lanes_scalar()` either way. Built
    inside ConfigForge (A-3) with `LANES_PREFIX_CF`, its symbols are
    `cf_lanes_*`, which gate.symbols' `cf_` rule accepts and which go in
    the internal list; a program that also links a lanes of its own sees
    no clash. A test builds `scalar.c` both ways.
  * **The gates and scripts it meets.** gate.detection forbids
    `__GNUC__`, `_MSC_VER` and the like outside the portability files
    (GateDetection.cmake:7-9, its exemption at :35): `lib/lanes/src/` is
    exempted beside `lib/platform/`, since lanes is its own portability
    layer. The format globs (`scripts/format.cmake`,
    `cmake/ConfigForgeFormat.cmake`, `.github/workflows/ci.yml`) cover
    `lib/*/tests` too. `scripts/release/package_rust.cmake` leaves
    `lib/lanes/tests` out of the crate.
* **files**: as listed; `lib/CMakeLists.txt` (adds `lanes` first);
  last: `CHANGELOG.md`.
* **steps**:
  1. The header and a `scalar.c` whose functions return 0; the tests;
     see them fail.
  2. The scalar bodies; the tests pass, under sanitize too.
  3. Detection, and the test that prints it.
  4. ACCEPT-LINUX, ACCEPT-WINDOWS.
* **accept**: ACCEPT-LINUX with `-DEXTRA="coverage;compilers"` (the
  floor holds with lanes' tests; tcc, pcc and musl build it),
  ACCEPT-WINDOWS, ACCEPT-CI; the MSVC build of `lanes` is C89 like the
  rest.
* **stop if**: detection cannot be written for a compiler of the matrix
  without a feature C89 lacks: that compiler reports only the scalar and
  SWAR bits, and the notes say so.
* **docs**: D-101's entry (written by A-1) gets "Built: A-2" with the
  commit.
  CHANGELOG `### Added`: "`lanes`, a vector library of its own: span
  operations with a scalar reference and the processor's detection;
  ConfigForge does not use it yet (D-101)."
* **notes**:

---

## A-3 - One UTF-8, one hex digit, one table hash

* **id**: A-3
* **goal**: `lib/text/` exists, internal, and the four UTF-8 decoders,
  the four UTF-8 encoders, the five hex-digit readers and the four
  seeded table hashes of "The helpers today" are one function each,
  called by every reader; no reader's behaviour changes.
* **why**: D-102, the copies with no difference between them first.
* **needs**: A-2
* **size**: medium
* **decided**:
  * `lib/text/include/configforge/cf_text.h`, every declaration inside
    `#if CF_INTERNAL_API` (D-88): not part of the 1.x promise.
    `cf_text.h` joins `CF_SERIALIZE_HEADERS`, and `lib/text` both
    libraries. The functions, in `lib/text/src/utf8.c`, `hex.c` and
    `hash.c`:

    ```c
    /* The code point at p, and its length, or 0 when p does not start
       a whole, valid sequence before end, and 0 when p == end. */
    CF_API size_t cf_text_utf8_decode(const char* p, const char* end, cf_u32* cp);
    /* The length of the longest valid prefix, through lanes_scalar(). */
    CF_API size_t cf_text_utf8_valid(const char* p, size_t n);
    /* Writes cp into out[0..3] and returns 1 to 4; the caller has
       refused surrogates and values above 0x10FFFF. */
    CF_API size_t cf_text_utf8_encode(cf_u32 cp, char out[4]);
    /* 0-15 for a hex digit of either case, -1 otherwise. */
    CF_API int cf_text_hex_value(int c);
    /* The seed the readers' name tables use: the four *_seed copies,
       identical, returning size_t. */
    CF_API size_t cf_text_seed(const void* where);
    /* FNV-1a of n bytes from seed, in a size_t, as ini_names_hash and
       xml_hash compute it. */
    CF_API size_t cf_text_hash(size_t seed, const char* s, size_t n);
    ```

    `yaml_hash` computes the same in a `cf_u32`: it switches to
    `cf_text_hash`. The low 32 bits agree, so a bucket chosen by the low
    bits is unchanged; read how each of the four tables reduces the hash
    to a bucket first, and if one uses `%` by a size that is not a power
    of two, stop and report. `toml_hash` (toml.c, today :655) is another
    function - it mixes a parent, an element and an index - and stays;
    only `toml_seed` goes.
  * `cf_text_is_utf8` (public, `blob.c`) stays where it is and becomes
    `return cf_text_utf8_valid(s, n) == n;`.
  * The callers switched, each copy deleted: `js_utf8_at`,
    `toml_utf8_len`, `yaml_utf8` (to `cf_text_utf8_decode`, keeping the
    length or the code point as each needed); `js_utf8`, `toml_utf8`,
    `prop_utf8` (its capacity check kept at the call) and `yp_put_cp`
    (to `cf_text_utf8_encode`, then `yp_put` per byte); `js_hexval`,
    `toml_hex`, `prop_hex` and the inline decode in `yp_escape`
    (yamlparse.c, about :668-672) (to `cf_text_hex_value`). Not
    `yp_tag_put`'s (yamlparse.c, :243-249): it computes `(c | 0x20) -
    'a' + 10`, which takes `@` and a backquote as the digit 9, so
    replacing it would refuse `%@0` in a tag that is read today; it is
    fixed, as a refusal, in A-7, and left alone here. `toml_seed`,
    `ini_seed`/`ini_names_hash`, `xml_seed`/`xml_hash`,
    `yaml_seed`/`yaml_hash` (to `cf_text_seed` and `cf_text_hash`), the
    explanatory comment kept once, in `hash.c`.
  * **lanes joins both libraries from this card on**, compiled in rather
    than linked: `lib/CMakeLists.txt` includes
    `lib/lanes/lanes_sources.cmake` and adds `LANES_SOURCES` and
    `lib/lanes/include` to `configforge` and `configforge_serialize`,
    with `LANES_PREFIX_CF` defined (A-2), so nothing new is installed or
    linked and the `.pc` files and Conan need no change.
    `scripts/generate/amalgamate.py` gains `lanes.h` and the lanes
    sources, and its `LOCAL_INCLUDE` pattern (:74) also takes
    `"lanes/..."` and `<lanes/...>`. The `cf_lanes_*` names join the
    internal symbol lists. Its scalar table is the one
    `cf_text_utf8_valid` calls.
* **files**: `lib/text/` (new), `lib/CMakeLists.txt`,
  `cmake/ConfigForgeSerialize.cmake`,
  `lib/serialize/src/{blob,json,toml,yaml,yamlparse,properties,ini,xml}.c`,
  `scripts/generate/amalgamate.py` (the new sources and header),
  `cmake/configforge.internal.symbols` and the serialization library's
  list (gate.abi with `-DWRITE=ON`), `tests/unit/test_text.c` (new) and
  its build line; last: `CHANGELOG.md`, `docs/Architecture.md`.
* **steps**:
  1. `test_text.c`: each function on the cases each old copy was used
     for, gathered by reading the copies (every overlong and surrogate
     class, both hex cases, the empty string for the hash); see it fail
     to link.
  2. `lib/text/`; the test passes.
  3. One reader at a time switched and its copy deleted; `ctest --preset
     default` and the conformance suites (tier 2) after each.
  4. gate.abi's lists, the amalgamation; ACCEPT.
* **accept**: ACCEPT-LINUX with `-DEXTRA="conformance;abi;coverage"`
  (conformance fetches its suites with git at pinned commits and needs
  the network and Python), ACCEPT-WINDOWS, ACCEPT-CI.
* **stop if**: two copies turn out to differ in a way a test shows (an
  input one accepts and the other refuses): keep the difference as a
  parameter of the one function, write the case in the notes, go on. A
  difference that is a bug in one reader is reported, not fixed here.
* **docs**: D-102's entry (written by A-1) gets "Built, in part: A-3".
  CHANGELOG `### Changed`: "The readers share one
  UTF-8 decoder and encoder, one hex-digit reader and one table hash
  (D-102); no input is read differently."
* **notes**:

---

## A-4 - Another encoding, read only by name

* **id**: A-4
* **goal**: a file, document, environment-document, dotenv or drop-in
  directory source given a `CF_ENCODING_*` value in its flags is read in
  that encoding and recoded to UTF-8 before anything else looks at it; a
  UTF-16 file is read by its byte-order mark; a file or document source
  says which encoding its last load read, and the report says it when it
  was not UTF-8. Nothing is refused yet that was accepted.
* **why**: D-108, rules 3 and 4. The way to read a legacy file comes
  before the refusal (A-5), so the refusal can name it.
* **needs**: A-3
* **size**: medium
* **decided**:
  * **A public header of the text component**,
    `lib/text/include/configforge/cf_encoding.h`, so that the
    configuration library (which may use text) and the serialization
    library's own program (T-3: it links `configforge_serialize` only
    and includes no internal tier) both reach it. It joins
    `CF_SERIALIZE_HEADERS`. Exactly:

    ```c
    #define CF_ENCODING_SHIFT 24
    #define CF_ENCODING_MASK  (15u << CF_ENCODING_SHIFT)
    enum {
        CF_ENCODING_UTF8         = 0u,                        /* the default */
        CF_ENCODING_ISO_8859_1   = 1u << CF_ENCODING_SHIFT,
        CF_ENCODING_WINDOWS_1252 = 2u << CF_ENCODING_SHIFT,
        CF_ENCODING_WINDOWS_1254 = 3u << CF_ENCODING_SHIFT,
        CF_ENCODING_ISO_8859_9   = 4u << CF_ENCODING_SHIFT,
        CF_ENCODING_UTF16LE      = 5u << CF_ENCODING_SHIFT,
        CF_ENCODING_UTF16BE      = 6u << CF_ENCODING_SHIFT,
        CF_ENCODING_LAST         = CF_ENCODING_UTF16BE
    };

    /* CF_TRUE and *flags the encoding's value for one of: "utf-8",
       "iso-8859-1" or "latin1", "windows-1252" or "cp1252",
       "windows-1254" or "cp1254", "iso-8859-9" or "latin5",
       "utf-16le", "utf-16be"; ASCII case ignored. CF_FALSE otherwise. */
    CF_EXPERIMENTAL(1, 8) cf_bool cf_encoding_by_name(const char* name, unsigned* flags);

    /* The canonical name ("windows-1254") of the encoding in `flags`,
       or NULL for a value above CF_ENCODING_LAST. */
    CF_EXPERIMENTAL(1, 8) const char* cf_encoding_name(unsigned flags);

    /* Recodes n bytes of `from` (any value but CF_ENCODING_UTF8) into
       UTF-8 at out, which holds cap bytes. Returns the bytes written, or
       (size_t)-1 with *bad the offset of the first byte of what has no
       character: Windows-1252's 0x81, 0x8D, 0x8F, 0x90, 0x9D;
       Windows-1254's those and 0x8E, 0x9E; an unpaired UTF-16
       surrogate; a last half code unit. Enough room: 3 * n for the
       single-byte encodings (0x80 to 0x9F of the Windows pages are
       mostly U+20xx, three bytes), 3 * (n / 2) for UTF-16. */
    CF_EXPERIMENTAL(1, 8) size_t cf_encoding_recode(unsigned from, const char* in, size_t n,
                                                    char* out, size_t cap, size_t* bad);
    ```

    Implemented in `lib/text/src/encoding.c`. The single-byte encodings
    are three tables of 128 entries (0x80 to 0xFF to a code point, 0 for
    "no character"), written from the Unicode Consortium's mapping files
    (`MAPPINGS/ISO8859/8859-9.TXT`,
    `MAPPINGS/VENDORS/MICSFT/WINDOWS/CP1252.TXT`, `CP1254.TXT`);
    ISO-8859-1 is the identity and needs none. `cf_source.h` includes
    `cf_encoding.h`.
  * **Which sources take it.** `config.c`'s `builder_slot_mask` and
    `builder_slot_flags` refuse bits outside the mask a builder passes
    (`lib/config/src/core/config.c`, today :376, 586, 610, 710). Two new
    masks in `cf_source.h`: `CF_FILE_SOURCE_FLAGS = CF_SOURCE_FLAGS |
    CF_ENCODING_MASK`, passed by the builders of file, document,
    environment document and dotenv sources; `CF_DROPIN_SOURCE_FLAGS =
    CF_DIR_SOURCE_FLAGS | CF_ENCODING_MASK`, passed by the drop-in
    directory's builder only. Registry, store, preferences, replay,
    memory, secrets and credentials keep their masks, so an encoding
    given to them is refused as any unknown flag is. Read every caller
    of the two functions first and write in the notes which builder
    passes which mask. A value above `CF_ENCODING_LAST` in the field
    lies inside the mask, so it is refused by its own test in
    `builder_slot_mask` (`(flags & CF_ENCODING_MASK) > CF_ENCODING_LAST`),
    with the message an unknown flag gets; `cf_source_file`,
    `cf_source_document`, `cf_source_env_document`, `cf_source_dotenv`
    and `cf_source_dir` refuse it the same way for a program that uses
    them directly. Bits 24 to 27 are free in every flags set today
    (checked 2026-11-19; the highest used is bit 18).
  * **Where it happens**, in `lib/config/src/bridge/file.c`: after the
    whole choice of where the bytes come from (the environment variable,
    today :196-207; the document's bytes; `cf_blob_file`, :208) and
    before `cf_source_by_content` (today about :220); in `dir.c`, for
    each file, at the same point; in the dotenv source before
    `cf_lex_dotenv`. The UTF-8 copy comes from the same `bytes` arena
    (`cf_blob_arena`, file.c :185), so `cf_blob_arena_done` frees it with
    the source's bytes. The rule, in this order:
    1. the codec is named (a format given, the codec handed over, or the
       extension's) and its caps have `CF_CODEC_BINARY`, or
       `CF_CODEC_SELF_ENCODED` (A-5 adds the bit; XML has it): nothing
       is recoded and no mark is looked at; an encoding in the flags is
       refused, `'%s': %s declares its own encoding or has none; name no
       encoding for it`, `CF_E_SOURCE`;
    2. the flags name an encoding: recode with it;
    3. the bytes start `FF FE 00 00` or `00 00 FE FF`: refuse, `'%s' is
       UTF-32, which this library does not read; convert it to UTF-8`;
    4. the bytes start `FF FE` or `FE FF`: recode as UTF-16LE or BE,
       the mark dropped;
    5. otherwise nothing is done (UTF-8; its mark skipped by the readers
       as today).
    When the format is left to the content and step 4 recoded the bytes,
    and the content then turns out to be a `CF_CODEC_SELF_ENCODED`
    format (XML in UTF-16), that codec reads the original bytes, mark
    included: expat reads UTF-16 by its own mark. No content-detected
    binary format can start with those marks (CBOR's first byte cannot
    be 0xFE or 0xFF; TLV is named only), so steps 3 and 4 cannot misread
    one.
  * **A byte with no character** is refused in the readers' form,
    `%s:%lu:%lu: byte 0x%02X has no character in %s` (origin, line,
    column, byte, the encoding's name); for UTF-16 `%s:%lu:%lu: an
    unpaired UTF-16 surrogate 0x%04X` and `%s: ends in half a UTF-16
    code unit`. Code `CF_E_SOURCE`. Lines and columns are counted in the
    original's units: bytes for the single-byte encodings, two-byte code
    units for UTF-16, a line ending at each LF unit, columns from 1. One
    internal helper does it for A-4 and A-5, in `lib/text/src/place.c`,
    declared in `cf_text.h` inside `CF_INTERNAL_API`:
    `CF_API void cf_text_place(const char* p, size_t at, unsigned unit,
    cf_u32* line, cf_u32* col);` with `unit` 1, or
    `CF_ENCODING_UTF16LE` / `CF_ENCODING_UTF16BE` for two-byte units.
  * **What says so**: `cf_source_file_encoding(const cf_source* s)`,
    `CF_EXPERIMENTAL(1, 8)`, in `cf_source.h` beside
    `cf_source_file_format`: the canonical name of what the last load of
    a file, document or environment-document source read (`"utf-8"`,
    `"utf-16le"`, `"utf-16be"` or a named one), kept in a new
    `file_state` member `const char* encoding`; NULL before a load and
    for other kinds of source. The report writes it as an `"encoding"`
    member of the source next to its format (`report.c`, at
    `cf_source_file_format`, today :183), only when it is not `"utf-8"`,
    so the reports of UTF-8 files do not change. A drop-in directory
    and a dotenv source say nothing yet; their refusals name the
    encoding, and a report member for them is weighed with A-10.
  * **The bindings** take the name. C++: the wrapper's file source gains
    an optional encoding name. Rust: the builder method gains an
    `Option<&str>`; `bindings/rust/src/sys.rs` declares the three new
    functions by hand, as it declares the rest. Python: the builder's
    file method gains `encoding=None`; `_native.py`'s symbol table gains
    the three; a library without them (older than 1.8) makes `encoding`
    raise the binding's usual error saying the library is too old. Each
    goes through `cf_encoding_by_name`; an unknown name is the
    binding's usual error. One test each.
* **files**: `lib/text/include/configforge/cf_encoding.h` (new),
  `lib/text/src/{encoding,place}.c` (new), `cf_text.h`,
  `lib/config/include/configforge/cf_source.h`,
  `lib/config/src/core/config.c`, `lib/config/src/bridge/{file,dir}.c`,
  the dotenv source, `lib/config/src/diag/report.c`,
  `cmake/ConfigForgeSerialize.cmake`, `scripts/generate/amalgamate.py`,
  the symbol lists (gate.abi with `-DWRITE=ON`: the four public names
  in the experimental list as `name 1.8`, `cf_text_place` in the
  internal list), `lib/cxx/include/configforge/configforge.hpp`,
  `bindings/rust/src/`, `bindings/python/src/configforge/`; tests:
  `tests/unit/test_encoding.c` (new) and its entry in
  `tests/unit/CMakeLists.txt`'s `CF_UNIT_TESTS`, the bindings' tests;
  last: `docs/Guide.md` (a section "Files in another encoding"),
  `CHANGELOG.md`.
* **steps**:
  1. `test_encoding.c`, part one, `cf_encoding_recode` alone: every
     table at every position where it differs from ISO-8859-1 and at
     every position with no character; a buffer of 0x80 bytes needing
     3 * n; UTF-16 in both orders, a surrogate pair, an unpaired
     surrogate, an odd length; `cf_encoding_by_name` for every name and
     alias, case mixed, and a wrong one. See it fail to link.
  2. `cf_encoding.h`, `encoding.c`, `place.c`; part one passes.
  3. Part two, through `cf_builder_file`: a Windows-1254 INI with
     `ad=Şükrü` read with `CF_ENCODING_WINDOWS_1254` gives the UTF-8
     `Şükrü` and the encoding `"windows-1254"`; the same file without
     the flag gives what it gives today; a UTF-16LE JSON with its mark;
     a UTF-16 XML with its mark and declaration, read by expat; a UTF-32
     mark refused by name; a byte with no character refused at its
     place; an encoding on a secrets directory and a value 7 in the
     field each refused as an unknown flag; an encoding named for CBOR
     refused. See it fail.
  4. The masks, the hook in file.c, dir.c and dotenv, the report; part
     two passes.
  5. The bindings and their tests; ACCEPT.
* **accept**: ACCEPT-LINUX with `-DEXTRA="abi;coverage;shared"` (the
  symbol lists, the coverage floor with the new tables, the Python
  binding's tests), ACCEPT-WINDOWS, ACCEPT-CI (cargo included).
* **stop if**: a source reads its bytes somewhere this card does not
  name: add the hook there too and name it in the notes.
* **docs**: D-108's entry (written by A-1) gets "Built: A-4" with the
  commit. The Guide's section: the seven names, the flag, the marks,
  what the report says. CHANGELOG `### Added`: "A file in another
  encoding is read when its source names it (`CF_ENCODING_*`, or the
  name in the tools and bindings), and a UTF-16 file by its byte-order
  mark, recoded to UTF-8 as it is read (D-108)."
* **notes**:

---

## A-5 - Bytes that are not UTF-8 are refused, with their place and a hint

* **id**: A-5
* **goal**: every text format, dotenv, the response files, the
  environment and argument sources and the program's own values refuse
  bytes that are not UTF-8, with the line and column of the first and,
  when the bytes look like one, the encoding to name; XML keeps reading
  by its own declaration.
* **why**: D-108, rules 1, 2 and 5.
* **needs**: A-4
* **size**: medium
* **decided**:
  * **The check and its message**, public, in `cf_encoding.h` (so the
    serialization library's program can call it, A-6):

    ```c
    /* CF_TRUE when the n bytes at p are UTF-8. Otherwise CF_FALSE, and
       err set (CF_E_SOURCE) to
       "<origin>:<line>:<column>: byte 0x<XX> is not valid UTF-8<hint>",
       the place of the first byte that is not, counted as every reader
       counts (lines by LF, columns in bytes from 1). */
    CF_EXPERIMENTAL(1, 8) cf_bool cf_encoding_check(const char* origin, const char* p,
                                                    size_t n, cf_error* err);
    ```

    The form `file:line:col: message` is the readers' own, and the words
    "not valid UTF-8" are TOML's today, so `tests/unit/test_toml.c`
    (its assertions at about :130 and :166-168) keeps passing; read
    those lines first and keep both strings.
  * **The hint**, deterministic, from the bytes of the whole input that
    are not part of a valid UTF-8 sequence:
    * if the input also holds valid multi-byte UTF-8 sequences (it mixes
      encodings): no hint;
    * else if any of those bytes is 0xD0, 0xDD, 0xDE, 0xF0, 0xFD or
      0xFE: `; it may be windows-1254 (Turkish): read it with
      CF_ENCODING_WINDOWS_1254, or the encoding windows-1254 in the
      tools`;
    * else if any is 0x80 to 0x9F: the same for `windows-1252`
      (`CF_ENCODING_WINDOWS_1252`);
    * else (all 0xA0 to 0xFF): the same for `iso-8859-1`
      (`CF_ENCODING_ISO_8859_1`).
    The hint never changes a value.
  * **The codecs, in one place.** `cf_codec_decode` (`lib/serialize/src/codec.c`)
    calls `cf_encoding_check` before the codec for every codec whose
    caps have neither `CF_CODEC_BINARY` nor the new
    `CF_CODEC_SELF_ENCODED = 1u << 8` (in `cf_serialize.h`, commented
    "Reads by an encoding the document itself declares, as XML does:
    not checked for UTF-8 before it is read. (1.8, D-108)"); XML's codec
    gets the bit. A program's own text codec is checked too. The decode
    without the check moves to an internal `cfi_codec_decode_raw` (in
    `cf_serialize.h` inside `CF_INTERNAL_API`), which `cf_codec_decode`
    calls after the check, and which `detect.c` calls for each candidate
    (today :146), so detection does not check the input once per
    candidate. `schema.c` (today :738) and `tools/doctor/spring.c`
    (today :314) call `json->decode` directly; both go through
    `cf_codec_decode`.
  * **Before detection.** When a file, document or drop-in file's
    format is left to the content, `file.c` and `dir.c` call
    `cf_encoding_check` once before `cf_source_by_content` (after A-4's
    recoding), so a file that is not UTF-8 gets the refusal and its
    hint rather than "cannot tell the format".
  * **Outside the codecs**, the same function and message:
    `cf_lex_dotenv` and the response-file reader
    (`lib/config/src/sources/entry.c`); `cf_lex_env` (entry.c, today
    :315), for the value of a variable that maps to a key only (those it
    skips, at about :339 `klen == 0`, stay unchecked, so an unrelated
    variable never stops a start), its origin `env <VARIABLE>` and never
    the value; `cf_lex_cli` (entry.c, today :381), for option values
    only, not positional arguments, its origin `argument <n>`; the
    memory and defaults sources, for the program's own text values, the
    origin the key. Covered through `cf_codec_decode`: Doctor's replay
    (`report.c`, today :893) and the schema (`schema.c`, today :777).
    Not checked: the binary image (`image.c`), secrets and credentials
    directories (D-108's exception), and the platform's stores, which
    hand over UTF-8 they made from UTF-16 or CFString.
  * **Conformance**: no record changes. The runner counts only the `y_`
    and `n_` cases (`scripts/check/conformance.py`, today :328;
    docs/Conformance.md, today :97), and JSONTestSuite's invalid-UTF-8
    cases are `i_`. Every `y_` case must stay accepted and every `n_`
    case refused.
  * **Fuzzing**: `tests/unit/test_fuzz.c`'s seeds (under `#if
    defined(CF_FUZZ_SEEDS)`, today :745) gain one document with an
    invalid byte for each text format, so the refusal path is fuzzed.
  * **Writing back**: since every text inside is now UTF-8 but for
    secrets, a value written by `cf_emit` or the report reads back; a
    round-trip test of one value from each source kind proves it.
    Secrets are written as they were read (D-108's exception).
* **files**: `cf_encoding.h`, `lib/text/src/encoding.c`,
  `lib/serialize/include/configforge/cf_serialize.h`,
  `lib/serialize/src/{codec,detect,xml}.c`,
  `lib/config/src/bridge/{file,dir,schema}.c`, `tools/doctor/spring.c`,
  `lib/config/src/sources/entry.c`, the memory and defaults sources,
  `tests/unit/test_fuzz.c`, the symbol lists (gate.abi `-DWRITE=ON`:
  `cf_encoding_check 1.8` experimental, `cfi_codec_decode_raw`
  internal), `scripts/generate/amalgamate.py` if a file is added; tests:
  `tests/unit/test_utf8_refused.c` (new) and its entry in
  `CF_UNIT_TESTS`, `tests/unit/test_toml.c` if a string must change
  (it should not); last: `docs/Guide.md` (A-4's section, the refusal),
  `docs/Formats.md` (each format: UTF-8 required; XML by its
  declaration), `CHANGELOG.md`.
* **steps**:
  1. `test_utf8_refused.c`: for each text format, a document valid but
     for one byte, refused with that byte's line and column; the three
     hints and the "no hint" case; a file with no extension refused
     with its hint, not "cannot tell"; XML in ISO-8859-1 by its
     declaration still read; a mapped variable with an invalid byte
     refused by its name, an unmapped one ignored; a memory value
     refused by its key; the round trip of one value from each source
     kind. See it fail.
  2. `cf_encoding_check`, the caps bit, `cfi_codec_decode_raw`, the
     check in `cf_codec_decode`, `detect.c`, the two direct callers, the
     check before detection; the codec cases pass.
  3. dotenv, response files, environment, arguments, memory, defaults;
     the test passes.
  4. The seeds; ACCEPT.
* **accept**: ACCEPT-LINUX with `-DEXTRA="conformance;fuzz;coverage;abi"`,
  ACCEPT-WINDOWS, ACCEPT-CI. Conformance fetches its suites with git at
  pinned commits and needs the network and Python.
* **stop if**: a `y_` case of a suite is refused, or an `n_` case
  accepted: stop, write the case in the notes, report.
* **docs**: D-108's entry gets "Built: A-5". CHANGELOG `### Changed`:
  "Bytes that are not UTF-8 are refused by every text reader, dotenv,
  response files, the environment, the arguments and the program's own
  values, with their place and, when they look like one, the encoding
  to name; XML still reads by its own declaration. Before, JSON, JSON5,
  YAML, INI, `.properties` and dotenv passed them on (D-108)."
* **notes**:

---

## A-6 - `configforge codec recode`: a file converted once, for review

* **id**: A-6
* **goal**: `configforge codec recode <file> --from <encoding> --out
  <file>` writes the file's bytes recoded to UTF-8 and nothing else
  changed; `configforge codec recode <file> --check` says whether a file
  is UTF-8 and, when it is not, gives A-5's refusal, hint included.
* **why**: D-108, rule 6. A file is fixed for good by a person who sees
  the diff, after which no reader needs to be lenient.
* **needs**: A-5; T-3 (`configforge codec`, done in 1.7.0)
* **size**: small
* **decided**:
  * In `tools/codec/` beside T-3's commands, `tools/codec/recode.c`
    (statics `rc_`), using `cf_encoding.h` only (it is in
    `CF_SERIALIZE_HEADERS`; T-3's rule holds: the program links
    `configforge_serialize` alone and uses no internal tier). Read T-3's
    `codec.c` and its notes first: the new usage lines join its usage
    text the way T-3 wrote it, and the command joins its dispatch:
    `configforge codec recode <file> --from <encoding> [--out <file>]`,
    "the file's bytes in UTF-8, nothing else changed";
    `configforge codec recode <file> --check`, "whether the file is
    UTF-8, and if not what it may be".
  * `--from` takes `cf_encoding_by_name`'s names. `--from utf-8`:
    `codec: '<file>' is to be UTF-8 already; use --check`, exit 2. An
    unknown name: `codec: no encoding '%s'; there are: utf-8,
    iso-8859-1, windows-1252, windows-1254, iso-8859-9, utf-16le,
    utf-16be`, exit 2. A file starting with a UTF-16 mark needs no
    `--from`: the mark decides and is dropped; a UTF-32 mark is A-4's
    refusal, exit 1.
  * The bytes are recoded whole with `cf_encoding_recode` into a buffer
    of 3 * n bytes; line endings, comments and layout are bytes like any
    other and come out as they went in; no UTF-8 mark is added. Without
    `--out` the result goes to standard output, in binary mode on
    Windows (as `tests/conformance/conform.c` does under
    `CONFORM_BINARY_STDOUT`). `--out` equal to the input path is
    refused, `codec: write to a new file and review the difference`,
    exit 2.
  * `--check`: exit 0 and `'<file>' is UTF-8` when `cf_encoding_check`
    passes; a file that starts with a UTF-16 mark, exit 1 and `'<file>'
    is UTF-16LE (by its byte-order mark): recode it with --from
    utf-16le` (or BE); otherwise exit 1 and `cf_encoding_check`'s
    message. Exit 2 when it cannot be read. `--check` with `--from` or
    `--out` is the usage, exit 2.
  * A byte with no character in the named encoding: A-4's message,
    exit 1, nothing written.
* **files**: `tools/codec/recode.c` (new), `tools/codec/CMakeLists.txt`,
  `tools/codec/codec.c` (the dispatch and usage); tests:
  `tests/codec/recode.cmake` (new) and its line in
  `tests/codec/CMakeLists.txt`, fixtures under `tests/codec/data/`: a
  Windows-1254 INI, its expected UTF-8 form, a UTF-16LE JSON with its
  mark, a UTF-8 TOML, each with a `-text` line in `.gitattributes`;
  last: `docs/Tools.md` (the command), `docs/Guide.md` (A-4's section:
  "Converting a file for good"), `CHANGELOG.md`.
* **steps**:
  1. `recode.cmake`: the Windows-1254 INI recoded with `--out` equals
     the expected file byte for byte (compare with `cmake -E
     compare_files`, never through `OUTPUT_VARIABLE`, which is not
     byte-safe); `--check` on each fixture with its exit code and
     message; the refusals of `--out` onto the input, of `--from
     utf-8` and of an unknown name. See it fail.
  2. `recode.c`; the test passes.
  3. The documents; ACCEPT.
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI.
* **stop if**: `tools/codec/` is not as T-3's card and notes describe
  it: follow what is there and say so in the notes.
* **docs**: D-108's entry gets "Built: A-6". CHANGELOG `### Added`:
  "`configforge codec recode`: a file converted to UTF-8 once, its
  bytes recoded and nothing else changed, and `--check` to find the
  files that are not UTF-8 (D-108)."
* **notes**:

---

## A-7 to A-11, written in full when A-6 is done

Each follows the design above. Its "decided" is written before it
opens, by the same rule as every milestone's cards (docs/Plan.md,
"After 1.7.0"); the design already answers what it builds.

* **A-7 - The escapes in families.** One escape decoder per family
  (`\u`: JSON, JSON5, TOML; YAML's; the backslashes of `.properties`)
  and one writer-side escaper taking the set to escape as a parameter;
  the three `${` copies made one; `yp_tag_put`'s `%XX` takes only hex
  digits (today `@` and a backquote pass as 9), a refusal under
  "### Changed".
* **A-8 - Whitespace, comments, newlines and the place of a refusal.**
  One skip per syntax with the newline rule as a parameter; one line
  and column tracker; one failure formatter; `ini_trim` and
  `dotenv_trim` one function; `tlv_key_valid` replaced by
  `cf_key_valid` moved to `lib/base`.
* **A-9 - Composed codecs** (D-103): `cf_codec_compose`, its repair and
  filter, the provenance that names it, the Guide's section, and a test
  reading `{{a:1}}` and `{a:{2}}`.
* **A-10 - A refusal's place and reason** (D-107): `cf_error.where`,
  `cf_reason.def` (with `CF_REASON_UTF8` for A-5's refusal), every
  reader filling both, and the test that one bad input is refused with
  the same reason in every format.
* **A-11 - Build profiles and the C version per file** (D-105, D-106):
  `cmake/profiles/`, `CF_PROFILE`, `cf_source_needs_c`, `gate.cversion`,
  and the two files that need more than C89 today (`yaml.c`, `posix.c`)
  moved onto the helper.
