# The layers, decided before they are built

The abstractions the project will grow into, decided together on
2026-11-19 so that each card that builds one does not have to design
it (docs/Plan.md says what a card is and how one is carried out). The
first part is the design: what each layer is, what it may use, and why.
The second part is the cards of the 1.8.0 milestone that start it
(docs/Maintaining.md, "Versions"). Cards for the later milestones are
written when the milestone before them ends; they follow this design
and do not reopen it.

D-numbers: this file uses **D-100 to D-107**. Check with
`git grep -n "^## D-10[0-7]" docs/Decisions.md` before writing one; if
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
its users. Its working name is **lanes** (prefix `lanes_`, directory
`lib/lanes/`); **TO DECIDE** by the maintainer before its first card,
the name and prefix (recommended: `lanes`, short and saying what a
vector is; alternatives `wordlane`, `swarmi`). A rename before its first
release is one commit.

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
  0x80). A configuration value is handed on as UTF-8 text, and two
  readers disagreeing on one document is an attack (D-103). **TO
  DECIDE** (A-4): recommended, every text reader refuses invalid UTF-8,
  as JSON's and YAML's standards already require; a Latin-1
  `.properties` mode only if a user asks. A refusal that was not one is
  a "### Changed" entry with its D-number.
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

Order: A-1, A-2, A-3, then A-4 to A-8 in their order. A-1 moves every
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
    `CF_SERIALIZE_HEADERS` keeps names only.
  * **The staged headers.** At configure time
    `cmake/ConfigForgeStaged.cmake` (new) copies every
    `lib/*/include/configforge/*` and the generated `cf_build_options.h`
    into `${CMAKE_BINARY_DIR}/staged/include/configforge/` with
    `configure_file(... COPYONLY)`, so a tool that wants one directory
    has one. Readers of it: `scripts/verify.cmake:549-550` (abidiff's
    `--headers-dir`), `scripts/check/proofs.cmake` (`-I`),
    `tests/unit/CMakeLists.txt:357` (`CMAKE_REQUIRED_INCLUDES`).
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
    files. A violation the move itself shows means the file is in the
    wrong component: move it one component up and write it in the notes;
    never widen the table.
  * **The scripts.** `scripts/generate/amalgamate.py`: `SOURCES`
    (:57-71) and `HEADERS` (:21-46) name the new paths, `check_complete`
    (:189-216) walks `lib/*/src` and `lib/*/include/configforge`, the
    openers (:237, :252, :280) read from there.
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
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI, the cargo job and
  the amalgamation gate included; step 3's two comparisons equal.
* **stop if**: something reads `src/` or `include/` that is not named
  here: add it, name it in the notes, go on. The installed file list
  differs and the difference cannot be removed without a change of
  behaviour: stop and report.
* **docs**: add **D-100**, "One directory per component", from the
  design above: the table, the staged headers, the install unchanged,
  the options rejected. CHANGELOG `### Changed`: "The source tree: one
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
* **needs**: A-1; the maintainer's answer to the name (D-101, **TO
  DECIDE**). Below, `lanes` stands for the name chosen.
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
  * CMake: `lib/lanes/CMakeLists.txt` builds a static library `lanes`
    usable alone (`add_subdirectory(lib/lanes)` in another project must
    work: no variable of ConfigForge's is read), and its tests when
    `BUILD_TESTING` is on. ConfigForge does not link it in this card.
* **files**: as listed; `lib/CMakeLists.txt` (adds `lanes` first);
  last: `CHANGELOG.md`.
* **steps**:
  1. The header and a `scalar.c` whose functions return 0; the tests;
     see them fail.
  2. The scalar bodies; the tests pass, under sanitize too.
  3. Detection, and the test that prints it.
  4. ACCEPT-LINUX, ACCEPT-WINDOWS.
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI; the MSVC build of
  `lanes` is C89 like the rest.
* **stop if**: detection cannot be written for a compiler of the matrix
  without a feature C89 lacks: that compiler reports only the scalar and
  SWAR bits, and the notes say so.
* **docs**: add **D-101** from the design above, with the name chosen.
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
       a whole, valid sequence before end. */
    CF_API size_t cf_text_utf8_decode(const char* p, const char* end, unsigned long* cp);
    /* The length of the longest valid prefix, through lanes_scalar(). */
    CF_API size_t cf_text_utf8_valid(const char* p, size_t n);
    /* Writes cp into out[0..3] and returns 1 to 4; the caller has
       refused surrogates and values above 0x10FFFF. */
    CF_API size_t cf_text_utf8_encode(unsigned long cp, char out[4]);
    /* 0-15 for a hex digit of either case, -1 otherwise. */
    CF_API int cf_text_hex_value(int c);
    /* The seed and the FNV-1a hash the readers' name tables use,
       exactly the arithmetic of the four copies. */
    CF_API cf_u32 cf_text_seed(const void* where);
    CF_API cf_u32 cf_text_hash(cf_u32 seed, const char* s, size_t n);
    ```

    Read the four `*_seed` and `*_hash` copies first: where they use
    `size_t` rather than `cf_u32`, the two declarations follow them, and
    the notes say which.
  * `cf_text_is_utf8` (public, `blob.c`) stays where it is and becomes
    `return cf_text_utf8_valid(s, n) == n;`.
  * The callers switched, each copy deleted: `js_utf8_at`,
    `toml_utf8_len`, `yaml_utf8` (to `cf_text_utf8_decode`, keeping the
    length or the code point as each needed); `js_utf8`, `toml_utf8`,
    `prop_utf8` (its capacity check kept at the call) and `yp_put_cp`
    (to `cf_text_utf8_encode`, then `yp_put` per byte); `js_hexval`,
    `toml_hex`, `prop_hex` and the two inline decodes in `yamlparse.c`
    (to `cf_text_hex_value`); `toml_seed`/`toml_hash`,
    `ini_seed`/`ini_names_hash`, `xml_seed`/`xml_hash`,
    `yaml_seed`/`yaml_hash` (to `cf_text_seed` and `cf_text_hash`), the
    explanatory comment kept once, in `hash.c`.
  * `lanes` is linked into both libraries from this card on, and its
    scalar table is the one `cf_text_utf8_valid` calls.
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
* **accept**: ACCEPT-LINUX (tier 2: conformance included),
  ACCEPT-WINDOWS, ACCEPT-CI.
* **stop if**: two copies turn out to differ in a way a test shows (an
  input one accepts and the other refuses): keep the difference as a
  parameter of the one function, write the case in the notes, go on. A
  difference that is a bug in one reader is reported, not fixed here.
* **docs**: add **D-102**, "One copy of each text and number helper",
  from the design. CHANGELOG `### Changed`: "The readers share one
  UTF-8 decoder and encoder, one hex-digit reader and one table hash
  (D-102); no input is read differently."
* **notes**:

---

## A-4 to A-8, written in full when A-3 is done

Each follows the design above. Its "decided" is written before it
opens, by the same rule as every milestone's cards (docs/Plan.md,
"After 1.7.0"); the design already answers what it builds.

* **A-4 - Invalid UTF-8 refused alike, and the escapes in families.**
  The maintainer's answer to "Invalid UTF-8" (TO DECIDE above); one
  escape decoder per family (`\u`: JSON, JSON5, TOML; YAML's; the
  backslashes of `.properties`) and one writer-side escaper taking the
  set to escape as a parameter; the three `${` copies made one.
* **A-5 - Whitespace, comments, newlines and the place of a refusal.**
  One skip per syntax with the newline rule as a parameter; one line
  and column tracker; one failure formatter; `ini_trim` and
  `dotenv_trim` one function; `tlv_key_valid` replaced by
  `cf_key_valid` moved to `lib/base`.
* **A-6 - Composed codecs** (D-103): `cf_codec_compose`, its repair and
  filter, the provenance that names it, the Guide's section, and a test
  reading `{{a:1}}` and `{a:{2}}`.
* **A-7 - A refusal's place and reason** (D-107): `cf_error.where`,
  `cf_reason.def`, every reader filling both, and the test that one bad
  input is refused with the same reason in every format.
* **A-8 - Build profiles and the C version per file** (D-105, D-106):
  `cmake/profiles/`, `CF_PROFILE`, `cf_source_needs_c`, `gate.cversion`,
  and the two files that need more than C89 today (`yaml.c`, `posix.c`)
  moved onto the helper.
