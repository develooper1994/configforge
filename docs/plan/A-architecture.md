# The layers, decided before they are built

The abstractions the project will grow into, decided together on
2026-11-19 so that each card that builds one does not have to design
it (docs/Plan.md says what a card is and how one is carried out). This
file is the design: what each layer is, what it may use, and why. The
cards of the 1.8.0 milestone that built it (A-1 to A-13) are done and
removed; what they did is in docs/Plan.md, "Done, their cards removed",
and their text in git (`git log -- docs/plan/A-architecture.md`). Cards
for the later milestones are written when the milestone before them
ends; they follow this design and do not reopen it.

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
* Unit tests move into their component (below, "Each component is
  laid out as a project of its own").
* **Each component is laid out as a project of its own** (the
  maintainer, 2026-11-19: so that taking one out later is a copy, not
  a rewrite). `lib/<c>/` holds `CMakeLists.txt`, `include/`, `src/`,
  `tests/` and `README.md`:
  * the build file declares the component with one call,
    `cf_component(<name> DEPENDS <components> SOURCES <files>)`
    (`cmake/ConfigForgeComponent.cmake`); the DEPENDS lists are the
    layering table above, written once, where the component is, and
    `gate.layering` reads them;
  * `cmake -S lib/<c>` builds that component alone, with what it
    depends on, as a static library `cf_<c>`, and runs its tests;
    `gate.components` does that for every component, which proves at
    every change that each one can be taken out. Inside the whole
    project the sources still go into the two libraries, since CMake
    3.10 (the project's floor) cannot link object libraries, and the
    symbol lists stay as they are;
  * its unit tests live in its `tests/`; the tests that cross
    components (consumer, conformance, fuzz, doctor, gen, codec,
    proofs, front) stay in the top-level `tests/`, and the shared test
    helpers (`cf_test.h`, `cf_fixture.h`, `cf_kv_codec.h`, the data)
    move to `tests/support/`;
  * its `README.md` says what it is, what it depends on, its public
    headers, and its version: `lanes` and `serialize` have their own,
    the rest follow ConfigForge's.
  A-1 moves the files; A-1b makes each component a project.
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
* **The build option** `CF_LANES_BACKENDS` chooses what is compiled:
  `scalar` (the reference alone), `c89` (scalar and SWAR, nothing that
  needs C99), or `all` (every backend the compiler can build, the
  default). The `misra` profile builds `scalar` (no word read through
  `memcpy`, MISRA C Rule 21.15); `minimal` and the embedded profiles
  build `c89`. Until 2.4.0 adds the others, every value builds `scalar`.
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
* XML's entities stay expat's and INI has no escapes, so neither is a
  family of the escape decoder (A-7 says why).
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
smuggling is the web's form of it), so no composed codec exists until
the program makes one, and it is never chosen from content. The build
option `CF_WITH_COMPOSE` defaults ON; the `misra` profile sets it OFF.
Its repair and filter are the program's code, named in every report.

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
`cf_build_options.h`, `cf_build_profile()` and the SBOM's options (not
the build id, which stays a bare commit id: tests and the Python
binding pin its form). Every shipped profile is built
in CI. ("Profile" here is a build's; the platform's file locations
"with profiles" in the Roadmap are a program's, a different thing.)

`CF_PROFILE_FILE`'s file and the shipped ones are read as data, one
`set(...)` a line, and the build directory keeps the profile it was
configured with.

### D-106 - A newer C only where a file needs it, and said so

C89 stays the floor. A file that needs more - a dependency's header
(libfyaml's C11, CoreFoundation's C99), a SIMD backend (C99) - says so
through one helper, `cf_source_needs_c(<file> <standard> REASON "<why>")`
in `cmake/ConfigForgeCVersion.cmake`, which sets that file's standard,
prints a configure-time warning naming the file, the standard and the
reason, writes it into the SBOM's options, and records it in a list that
`gate.cversion` compares with the files actually compiled above C89. A
file above C89 that is not on the list fails; so does an entry with no
reason. The `minimal` and `misra` profiles refuse every entry, by name,
at configure time. Code already written in C89 is never rewritten to a
newer standard unless it has to be.

### D-107 - One refusal, the same in every format

`cf_error` is already the one error of the library (a code, the key,
the source, a message). What it lacks is the place and the reason in a
form a program can test rather than read:

* a `cf_loc where` member (line and column; `cf_loc` has no byte
  offset, and the binary formats keep their place in the message),
  added through `size` as D-51 adds members, so 1.x callers are
  unaffected;
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

Built: A-6 (`configforge codec recode`, rule 6).

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
