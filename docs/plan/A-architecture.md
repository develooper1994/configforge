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

---

## The cards of 1.8.0

Order: A-1, A-1b, A-2, A-3, A-4, A-5, A-6, A-7, A-8, A-9, A-10, A-11,
A-12, A-13. A-9 needs A-5 (the UTF-8 check and `cfi_codec_decode_raw`).
Line and column go to a message as `%u` of an `unsigned`, as the
readers pass them today (cast a `cf_u32` place where needed). **Unit
tests from A-1b on**: a card below that names `tests/unit/<test>.c`
means the component's own `lib/<c>/tests/<test>.c`, by A-1b's rule
(the lowest component that holds every header it includes), and its
entry goes in that component's test list. A-1 moves every
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
  * **First, the integer writers go down to port** (corrected
    2026-11-20; the first version said port used nothing of
    `cf_numeric.h`, and the run that followed it failed: `cf_vformat`
    writes `%d %i %u %zu %I %U` with `cf_i64_to_text` and
    `cf_u64_to_text`). Before the move, in one commit: the two
    definitions move from `src/core/numeric.c` to `src/core/port.c`
    unchanged; their declarations and comments move from
    `include/configforge/cf_numeric.h` to `include/configforge/cf_port.h`
    unchanged; `cf_numeric.h` includes `cf_port.h` (if it does not
    already), so every caller of `cf_numeric.h` still sees them; port.c
    drops its include of `cf_numeric.h`. The exported symbols are the
    same (gate.abi), `docs/API.md` is regenerated (`cmake -P
    scripts/generate/apiref.cmake`). Then port uses nothing, as the table
    says, and numeric keeps the readers and floating point. Rejected:
    moving the formatter up into numeric, which every component below
    numeric's level would then need for its messages.
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

## A-1b - Each component a project: its declaration, its tests, its README, built alone

* **id**: A-1b
* **goal**: every `lib/<c>/` is declared by `cf_component()`, holds its
  own unit tests and a README, and builds alone with `cmake -S
  lib/<c>`; `gate.layering` reads the declarations instead of a table
  of its own; `gate.components` builds each component alone. The two
  libraries, their symbols, the installed files and every test's
  result are what they were.
* **why**: D-100, "Each component is laid out as a project of its own":
  a component that already builds alone is taken out by a copy.
* **needs**: A-1
* **size**: large
* **decided**:
  * **`cmake/ConfigForgeComponent.cmake`** (new) defines
    `cf_component(<name> DEPENDS <c>... SOURCES <file>...
    [SERIALIZE_SOURCES <file>...])`. `SERIALIZE_SOURCES` is the subset of
    SOURCES that also goes into `configforge_serialize`: all of them for
    port, numeric, base, text and serialize, and for the platform only
    its `file.c` (the file opener; the platform's other sources are in
    `configforge` alone). `CF_SERIALIZE_LEAVES`
    (`cmake/ConfigForgeSerialize.cmake`) is derived from these
    declarations, as `gate.layering` reads them, and no card edits it by
    hand any more; `CF_SERIALIZE_HEADERS` stays a list of names. In
    every case `cf_component` sets `CF_REPO_ROOT` (whole project:
    `${CMAKE_SOURCE_DIR}`; alone: `${CMAKE_CURRENT_LIST_DIR}/../..`),
    which tests use for any path outside their component (for example
    `tests/conformance/data`, which `tests/unit/CMakeLists.txt:200` names
    through `${CMAKE_SOURCE_DIR}`, wrong in an alone build):
    * inside the whole project (`CF_WHOLE_PROJECT`, set by the top-level
      CMakeLists.txt before `add_subdirectory(lib)`): it adds the
      sources to `configforge`, and `SERIALIZE_SOURCES` to
      `configforge_serialize`, adds `include/` as a PUBLIC include
      directory of the same targets, and records
      `CF_COMPONENT_<name>_DEPENDS` in a global property for the gate;
    * alone (`CF_WHOLE_PROJECT` not set): it adds each component of
      DEPENDS once (guarded by `if(NOT TARGET cf_<dep>)`), from
      `${CMAKE_CURRENT_LIST_DIR}/../<dep>`, into
      `${CMAKE_BINARY_DIR}/<dep>`, builds a static library `cf_<name>`
      from its sources linked to `cf_<dep>` of each dependency, and adds
      its `tests/` when `BUILD_TESTING` is on.
  * Each `lib/<c>/CMakeLists.txt` begins, for the alone case,
    `cmake_minimum_required(VERSION 3.10)` and, when it is the top
    project, `project(cf_<c> C)` and
    `include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/ConfigForgeOptions.cmake)`
    (the options and `cf_build_options.h`), then calls `cf_component`.
    `lanes` and `text` do not exist yet. A-2 adds `lanes` with its own
    build file (it must not read ConfigForge's options) and A-3 adds
    `text`; each of those two cards edits the DEPENDS, in the build
    files, of the components that gain the dependency, and lists those
    files. Inside ConfigForge, whole or alone, lanes is compiled from
    `LANES_SOURCES` (`lib/lanes/lanes_sources.cmake`, A-2) with
    `LANES_PREFIX_CF`, so its symbols are `cf_lanes_*`; in an alone
    build `cf_component` builds that as the static library `cf_lanes`.
    Only `cmake -S lib/lanes` builds the unprefixed library `lanes`.
  * **The DEPENDS** are D-100's table restricted to the components that
    exist after A-1: port: none; numeric: port; base: port numeric;
    platform: port numeric base; serialize: port numeric base platform
    (its include of `cf_platform_file.h` only, which the gate checks as
    A-1's `<component>:<header>` entry); config: all above; cxx: config.
    A-2 then adds `lanes` to numeric's, serialize's and config's lists;
    A-3 adds `text` (DEPENDS port numeric base lanes) and adds `text` to
    serialize's and config's lists. The full table is D-100's. `cmake/ConfigForgeLayers.cmake` (A-1) is
    removed; `gate.layering` reads the global property (it runs with
    `-P`, so the top-level build writes the property's content into
    `${CMAKE_BINARY_DIR}/layers.cmake` at configure time and the gate
    includes that file; its `-DLAYERS=` argument names it).
  * **Unit tests move** from `tests/unit/` to `lib/<c>/tests/` by one
    rule: a test belongs to the lowest component, in the order port,
    lanes, numeric, base, text, platform, serialize, config, cxx, that
    holds every `configforge/` header it includes (through its own
    `#include` lines, not what those headers include); a `.cpp` test
    belongs to cxx; `test_serialize_alone.c` to serialize (it links
    `configforge_serialize` only); `test_fuzz.c` stays a unit test, in
    `lib/config/tests/`, with the ctest name `unit.test_fuzz` and its
    TIMEOUT (tests/unit/CMakeLists.txt:25, :504); the two places that
    read it by path change to the new one:
    `tests/fuzz/CMakeLists.txt:15` (the seed program) and
    `tests/fuzz/libfuzzer.c:19`. Write the resulting map in the notes
    before moving.
    `cf_test.h`, `cf_fixture.h`, `cf_kv_codec.h` and `tests/unit/data/`
    move to `tests/support/` (and `tests/support/data/`); every
    `#include "cf_test.h"` keeps its spelling, with `tests/support` on
    the include path, and the four cross-directory includes
    (`tests/conformance/codec_module.c:13`, `tests/doctor/fixtures.c:16`,
    `tests/doctor/test_api.c:14`, `tests/gen/test_gen.c:17`) change to
    it. However the tests find their data today (read
    `tests/unit/CMakeLists.txt`: a compile definition or a working
    directory), the new path follows.
  * **Each component's `tests/CMakeLists.txt`** carries its tests'
    lines from `tests/unit/CMakeLists.txt` unchanged in substance:
    `CF_UNIT_TESTS` becomes one list per component; the special blocks
    move with their test (`test_serialize_alone` :113-114, `test_c11`
    :133, the C++ tests :278-406, `unit.internal_fenced` :181-190, the
    platform exclusion of `test_watch` :89, `CMAKE_REQUIRED_INCLUDES`
    :357 already on the staged headers). The ctest names stay exactly
    as they are (`unit.<name>`), so CI filters and docs that name them
    do not change. `tests/unit/` is removed.
  * **The tests' environment.** Each component's `tests/CMakeLists.txt`
    does what `tests/CMakeLists.txt:7` and `tests/unit/CMakeLists.txt`
    do for the tests today, since `lib/<c>/tests/` is no longer under
    `tests/`: `add_definitions(-DCF_INTERNAL_API=1)`, `set(CMAKE_C_CLANG_TIDY
    "")`, and `configforge_apply_warnings`, `configforge_apply_analysis`
    and `CF_CPPCHECK_TESTS` on each test. An alone build includes
    `cmake/ConfigForgeCompiler.cmake` and the analysis module so those
    helpers exist, and `include(CTest)` for `BUILD_TESTING`. A test links
    `configforge` in the whole build and `cf_<c>` in an alone one (one
    variable, set by `cf_component`, holds the name). Paths outside the
    component go through `CF_REPO_ROOT`. In the whole-project build
    `cf_component` adds only sources and include directories; the
    top-level `tests/CMakeLists.txt` adds each `lib/<c>/tests` with
    `add_subdirectory`, before `add_subdirectory(conformance)`, so the
    unit tests keep their place in the order tests are registered.
  * **`CF_REPO_ROOT` in the CMake modules.** Where a module names a file
    of the repository through `${CMAKE_SOURCE_DIR}` it uses
    `${CF_REPO_ROOT}` instead, so that a component builds alone, where
    `CMAKE_SOURCE_DIR` is `lib/<c>`: `cmake/ConfigForgeOptions.cmake:201`,
    `cmake/ConfigForgeInstall.cmake:18, 53, 70, 126, 128`,
    `cmake/ConfigForgeSbom.cmake:62`,
    `cmake/ConfigForgeCompiler.cmake:234`. The top-level
    `CMakeLists.txt` sets `CF_REPO_ROOT` to `${CMAKE_SOURCE_DIR}` before it
    includes them; `CMakeLists.txt:13` (the in-source-build refusal of
    the top project itself) keeps `CMAKE_SOURCE_DIR`, which is right
    there, and the setting follows it. An alone build sets
    `CF_REPO_ROOT` as `cf_component` says and `CMAKE_MODULE_PATH` to
    `${CF_REPO_ROOT}/cmake`.
  * **`gate.components`**: a new step `components` in
    `scripts/verify.cmake`'s EXTRA list (and in its "unknown EXTRA
    step" message), and a CI job, which for each component configures
    `cmake -S lib/<c> -B <work>/<c> -DBUILD_TESTING=ON`, builds and runs
    `ctest`. Not a ctest gate: nine configures are too slow for every
    build.
  * **READMEs**: `lib/<c>/README.md`, at most 30 lines each: what the
    component is (one paragraph, from docs/Architecture.md), DEPENDS,
    its public headers, its version line, and how to build it alone.
    `lanes`' README from A-2 stays.
  * docs: `docs/Architecture.md`'s "Where things live" describes the
    component layout and `cmake -S lib/<c>`; `CONTRIBUTING.md`'s table
    says where a new unit test goes (the rule above).
* **files**: `cmake/ConfigForgeComponent.cmake` (new), every
  `lib/<c>/CMakeLists.txt`, `lib/<c>/tests/` (new, from `tests/unit/`),
  `lib/<c>/README.md` (new), `tests/support/` (new),
  `tests/fuzz/CMakeLists.txt:15`, `tests/fuzz/libfuzzer.c:19`, the four
  cross-directory includes, `cmake/gates/GateInstall.cmake:136`
  (`tests/unit/fenced.c`), `scripts/buildenv/freebsd.sh:84`
  (`build/tests/unit`), `cmake/ConfigForgeSerialize.cmake` (the leaves
  derived from the declarations), `docs/Conventions.md:185`,
  `cmake/gates/GateLayering.cmake`, `cmake/ConfigForgeGates.cmake`,
  `tests/CMakeLists.txt` (adds each `lib/<c>/tests`),
  `cmake/ConfigForgeOptions.cmake`, `cmake/ConfigForgeInstall.cmake`,
  `cmake/ConfigForgeSbom.cmake`, `cmake/ConfigForgeCompiler.cmake`
  (`CF_REPO_ROOT`),
  `CMakeLists.txt`, `scripts/verify.cmake`, `.github/workflows/ci.yml`
  (the job; and any path to `tests/unit/` in it), the format globs
  (`scripts/format.cmake`, `cmake/ConfigForgeFormat.cmake`); last:
  `docs/Architecture.md`, `CONTRIBUTING.md`, `CHANGELOG.md`.
* **steps**:
  1. Record `ctest --preset default -N` (every test's name) to a file
     outside the tree.
  2. `cf_component` and the components' build files, inside the whole
     project only; the build and `ctest` pass. This step moves build
     files and no behaviour, so its characterization is step 1's list:
     to see it fail, remove one source from one component's SOURCES on
     purpose, see the build or `ctest` fail, restore it.
  3. The map of tests (in the notes), then the moves, `tests/support/`,
     the components' test files; `ctest --preset default -N` lists
     exactly step 1's names; `ctest` passes.
  4. `gate.layering` on the declarations: break it on purpose (a
     DEPENDS entry removed), see it fail, put it back.
  5. The alone case; `cmake -S lib/serialize -B /tmp/s` builds and its
     tests pass; then every component; the `components` step and the
     CI job.
  6. READMEs, documents; ACCEPT.
* **accept**: ACCEPT-LINUX with
  `-DEXTRA="components;abi;coverage;shared"`, ACCEPT-WINDOWS, ACCEPT-CI;
  step 3's list of test names unchanged.
* **stop if**: a component cannot build alone without one that its
  DEPENDS does not name: the table is right and the code is not; stop,
  write the include in the notes, report (do not widen DEPENDS).
* **docs**: D-100's entry gets "Built: A-1, A-1b". CHANGELOG
  `### Changed`: "Each component under `lib/` is a project of its own:
  its declaration, its tests and README, and a build of it alone that
  CI runs (D-100)." `docs/Roadmap.md`: the item "Infrastructure behind
  abstractions" stays 🟨, its line gains "A-1b done".
* **notes**:
  * **The map of tests** (written before the moves; the rule is "the
    lowest component, in the order port, numeric, base, platform,
    serialize, config, cxx, that holds every `configforge/` header the
    test includes"): port: `test_port`. numeric: `test_numeric` (and its
    two locale runs). base: `test_alloc`, `test_value` (and its two locale
    runs). platform: none. serialize: `test_serialize_alone`. cxx:
    `test_wrapper_access`, `test_wrapper_cxx17`, `test_wrapper_cxx20`,
    `test_wrapper_watch`, `test_wrapper_write`. config: every other
    `test_*.c` (each includes `configforge.h` or another header of
    config's, so none of them is lower), `test_fuzz.c` by decision, and
    `fenced.c`. `ctest -N` lists the same 229 names on Windows and 236 on
    Linux as before the moves; the order differs, since the unit tests are
    registered by component.
  * **Where the tree differed from the card.** The lines of
    `cmake/ConfigForgeInstall.cmake` that name `${CMAKE_SOURCE_DIR}` are
    18, 64, 81, 137 and 139. A fifth file includes `../unit/cf_test.h`:
    `tests/gen/test_gen_cpp.cpp:16`. The layer directories had build files
    of their own (`lib/config/src/<layer>/`, `lib/platform/src/`,
    `lib/serialize/src/`); they are folded into the component's
    `CMakeLists.txt`, and the parsers' detection that was in
    `lib/serialize/src/CMakeLists.txt` is `lib/serialize/backends.cmake`,
    which sets variables and touches no target.
  * **Choices the card left to the build.** A dependency may be spelled
    `<component>:<header>` in DEPENDS (serialize: `platform:cf_platform_file.h`),
    which is how the gate keeps A-1's entry. The gate allows a component the
    DEPENDS of what it depends on too (cxx: config's), as D-100's table has
    it. The platform's `file.c` is one per platform and one is compiled; the
    gate holds all three to the leaf rule. Alone, only the top component's
    `tests/` are added (a dependency's tests run in its own build); `cxx`,
    with no sources, is an interface library. The unit tests' build
    directories are `build/lib/<c>/tests`, so CI's and `freebsd.sh`'s paths
    follow. `gate.detection` leaves `lib/*/tests/` out and `gate.codespell`
    skips it, as neither read the tests before they were under `lib/`.
    `cf_component_tests` and `cf_unit_tests` (cmake/ConfigForgeComponent.cmake)
    are the tests' environment and the one loop, written once.
  * **Seen to fail.** A source taken out of config's SOURCES: the link of
    Doctor's `configforge-gen` fails on `cf_ks_init`. A DEPENDS entry taken
    out of base (numeric): `gate.layering` names `cf_value.h` including
    `cf_numeric.h`. Both restored.
  * Each of the seven components built alone, with `cmake -S lib/<c>`,
    and its tests passed; none needed a component its DEPENDS does not name.

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
    ConfigForge's), `lib/lanes/LICENSE` (the project's zlib text; "MIT" was a slip, corrected when A-2 was built).
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
  * The option `CF_LANES_BACKENDS` (D-101) is declared in lanes' own
    CMake file (a cache STRING, default `all`, refused unless `scalar`,
    `c89` or `all`); ConfigForge's options read the same variable, so a
    profile sets it. In this card every value builds `scalar` alone.
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
    (GateDetection.cmake:7-9, its exemption at :43): `lib/lanes/src/` is
    exempted beside `lib/platform/`, since lanes is its own portability
    layer. The format globs (`scripts/format.cmake`,
    `cmake/ConfigForgeFormat.cmake`, `.github/workflows/ci.yml`) cover
    `lib/*/tests` too. `scripts/release/package_rust.cmake` leaves
    `lib/lanes/tests` out of the crate.
* **files**: as listed; `lib/CMakeLists.txt` (adds `lanes` first and
  records it, with no dependency, for `gate.layering`);
  `lib/numeric/CMakeLists.txt`, `lib/serialize/CMakeLists.txt` and
  `lib/config/CMakeLists.txt` (`lanes` joins each `cf_component`'s
  DEPENDS, as A-1b says); last: `CHANGELOG.md`.
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
  * Read against the tree after A-1b: this card says `lib/CMakeLists.txt`
    "records it, with no dependency, for `gate.layering`". After A-1b
    `lib/CMakeLists.txt` only adds the component directories, and
    `gate.layering` reads the components declared by `cf_component`
    (written by `cf_write_layers` into `layers.cmake`). `lib/lanes/CMakeLists.txt`
    must read none of ConfigForge's variables and so cannot call
    `cf_component`; how `lanes` gets into that file is not stated here.
  * Answered 2026-11-20 (the organiser): the recording part of
    `cf_component` (`cmake/ConfigForgeComponent.cmake`, the three
    `set_property(GLOBAL ...)` lines under "For the layering gate") becomes
    a function of its own, `cf_component_record(<name> DEPENDS <dep>...
    LEAVES <file>...)`, which `cf_component` calls unchanged in effect.
    `lib/CMakeLists.txt`, right after `add_subdirectory(lanes)`, calls
    `cf_component_record(lanes)` - no dependency, no leaves - so lanes is
    in `layers.cmake` while `lib/lanes/CMakeLists.txt` reads nothing of
    ConfigForge's. Seen to fail first: before that call, a component
    whose DEPENDS names `lanes` makes `gate.layering` refuse it.

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
    linked and the `.pc` files and Conan need no change. In an alone
    build of a component (A-1b) `cf_component` compiles the same
    `LANES_SOURCES` with `LANES_PREFIX_CF` as the static library
    `cf_lanes`; only `cmake -S lib/lanes` builds the unprefixed `lanes`.
    `lib/text/CMakeLists.txt` is a `cf_component(text DEPENDS port
    numeric base lanes SOURCES ... SERIALIZE_SOURCES ...)` (all its
    sources in both lists), and `text` joins the DEPENDS of serialize
    and config (A-1b); `CF_SERIALIZE_LEAVES` is derived from the
    declarations and is not edited.
    `scripts/generate/amalgamate.py` gains `lanes.h` and the lanes
    sources, and its `LOCAL_INCLUDE` pattern (:82) also takes
    `"lanes/..."` and `<lanes/...>`. The `cf_lanes_*` names join the
    internal symbol lists. Its scalar table is the one
    `cf_text_utf8_valid` calls.
* **files**: `lib/text/` (new), `lib/CMakeLists.txt`,
  `lib/serialize/CMakeLists.txt` and `lib/config/CMakeLists.txt` (`text`
  joins DEPENDS), `cmake/ConfigForgeSerialize.cmake` (`cf_text.h` joins
  `CF_SERIALIZE_HEADERS`),
  `lib/serialize/src/{blob,json,toml,yaml,yamlparse,properties,ini,xml}.c`,
  `scripts/generate/amalgamate.py` (the new sources and header),
  `cmake/configforge.internal.symbols` (gate.abi with `-DWRITE=ON`),
  `lib/text/tests/test_text.c` (new) and its build line in
  `lib/text/tests/CMakeLists.txt` (new); last: `CHANGELOG.md`, `docs/Architecture.md`.
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
  * From A-2 (2026-11-20): `bindings/rust/build.rs` compiles every
    `lib/*/src`, lanes included, without `LANES_PREFIX_CF`: this card sets
    it there, so the crate exports no unprefixed `lanes_` symbol.

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
    `%s:%u:%u: byte 0x%02X has no character in %s` (origin, line,
    column, byte, the encoding's name); for UTF-16 `%s:%u:%u: an
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
  `lib/config/tests/test_encoding.c` (new) and its entry in
  `lib/config/tests/CMakeLists.txt`'s `CF_UNIT_TESTS`, the bindings' tests;
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
* **notes**: 2026-11-20, built.
  * **Which builder passes which mask** (`config.c`, every caller of
    `builder_slot_mask` read first): `CF_FILE_SOURCE_FLAGS` by
    `cf_builder_file`, `cf_builder_file_codec`, `builder_document` (so
    `cf_builder_document` and `cf_builder_env_document`) and
    `cf_builder_dotenv`, through the new `builder_slot_file`;
    `CF_DROPIN_SOURCE_FLAGS` by `cf_builder_dir` alone; `CF_DIR_SOURCE_FLAGS`
    still by `cf_builder_secrets` and `cf_builder_credentials`;
    `CF_SOURCE_FLAGS` (through `builder_slot_flags`) by the registry,
    preferences, store and replay builders. `builder_slot_mask` refuses a
    value above `CF_ENCODING_LAST` itself, whatever mask it is given, with
    the unknown-flag message. `cf_source_file`, `cf_source_dotenv` and
    `cf_source_dir` return false for such a value; `cf_source_document` and
    `cf_source_env_document` do through `cf_source_file`; the file-codec
    source too.
  * **One place for the hook**: `cfi_source_recode` (rules 1 to 5, in
    `file.c`) and `cfi_source_text` (it, then the content's choice of
    format, then XML's re-read of the original bytes), both declared in
    `cf_source.h` as the repository's other `cfi_` functions are. `file.c`
    and `dir.c` call `cfi_source_text`; `dotenv.c` calls `cfi_source_recode`
    with no codec. A-5's check before detection goes into
    `cfi_source_text`, once.
  * **Two things the card did not name, done because the intent is
    plain**: `CF_CODEC_SELF_ENCODED` (A-5 names it) is added now, in
    `cf_serialize.h`, and given to XML, as rule 1 needs it; and the XML
    reader's `XML_ParserCreate("UTF-8")`, which overrides the document's
    own declaration and mark, is `XML_ParserCreate(NULL)`, so that expat
    reads a UTF-16 document by its mark, as the card says it does. A-5 then
    only has to add the check and `cfi_codec_decode_raw`; its test of XML in
    ISO-8859-1 "still read" now has the behaviour it assumes.
  * **An encoding named for a file whose format the content chose and found
    to be self-encoded** is refused as rule 1 refuses it (the card says
    nothing of this case).
  * `cf_encoding_recode` with too little room returns `(size_t)-1` with
    `*bad == n`, and with `from` UTF-8 or unknown the same; the card leaves
    both open.
  * `cf_fmt` has no width, so the byte and the code unit in the messages
    are written as hexadecimal text and passed as `0x%s`.
  * `tests/support/cf_fixture.h` gained `spill_n` (bytes with a length,
    for UTF-16), and `spill` is written with it.

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
    "not valid UTF-8" are TOML's today, so `lib/config/tests/test_toml.c`
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
    without the check moves to an internal `cfi_codec_decode_raw`,
    declared in `cf_serialize.h` as the repository's other `cfi_`
    functions are (cf_define.h, `cfi_defset_nearest_secret`): a plain
    declaration, not `CF_API`, not inside `#if CF_INTERNAL_API`, and in
    no symbol list. It is called only inside the library: by
    `cf_codec_decode` after the check, and by `detect.c` for each candidate
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
  * **Fuzzing**: `lib/config/tests/test_fuzz.c`'s seeds (under `#if
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
  `lib/config/tests/test_fuzz.c`, the symbol lists (gate.abi
  `-DWRITE=ON`: `cf_encoding_check 1.8` in the experimental list;
  `cfi_codec_decode_raw` is in no list),
  `scripts/generate/amalgamate.py` if a file is added; tests:
  `lib/config/tests/test_utf8_refused.c` (new) and its entry in
  `CF_UNIT_TESTS`, `lib/config/tests/test_toml.c` if a string must change
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
* **notes**: where the tree asked for more than the card said.
  * The environment, the arguments, the memory pairs and the defaults
    have no state to keep an error in (`cf_source_vtable::load` returns
    a bool), so the builder would have said "source 'env' could not be
    read" and lost the name the card wants. The key space carries the
    reason (`cfi_ks_refuse`, `cfi_ks_refusal`, a pointer at the end of
    `cf_keyspace`), and `config.c` reads it when no source has a detail
    of its own. Only a refusal of the bytes (`CF_E_SOURCE`) is kept;
    `cf_lex_cli`'s other errors are as lost as they were.
  * A file whose format is left to the content may be binary: a CBOR
    file with no extension in a drop-in directory is read by its content
    (test_detect). So `cfi_source_text` lets the content decide first
    and refuses the bytes unless the format it finds declares its own
    encoding or has none (`file_declares`: XML, CBOR, TLV); the card's
    plain "check before detection" would have refused CBOR.
  * The response-file reader is `cf_cli_expand`; the file is checked
    whole, its place the line and column in the file.
  * `test_encoding.c` (a document with none named) and `test_roundtrip.c`
    (a memory value that is not UTF-8) described what the card changes,
    and say the refusal now; the writer's own refusal is still tested by
    `test_invalid_utf8`.
  * The fuzz seeds are one second file for each of the seven text
    readers, `seed_<reader>_utf8`, named in `tests/fuzz/CMakeLists.txt`.

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
* **notes**: 2026-11-22, built.
  * **Organiser's answers** (2026-11-22, binding; they close gaps found
    reading the card against the tree):
    * `cf_text_place` is internal (`CF_INTERNAL_API`), so `recode.c` has
      its own static `rc_place`: lines and columns from 1; bytes for the
      single-byte encodings, 2-byte code units for UTF-16, a new line
      after each LF unit. Public API only: `cf_encoding.h`.
    * The mark is read as `file_mark` in `lib/config/src/bridge/file.c`
      does (not included): UTF-32's marks first, refused for recode and
      `--check` alike (`codec: '<file>' is UTF-32, which this library
      does not read; convert it to UTF-8`, exit 1).
    * A byte with no character: exit 1, nothing written (the result is
      built in a buffer, `--out` opened only after it succeeded);
      single-byte `codec: <file>:<line>:<col>: byte 0x<XX> has no
      character in <name>`, UTF-16 `... an unpaired UTF-16 surrogate
      0x<XXXX>`, and `codec: <file>: ends in half a UTF-16 code unit`:
      `file_refuse_at`'s words with the `codec: ` prefix.
    * A UTF-16 mark with `--from`: the mark decides; `--from` naming the
      mark's own encoding is accepted, any other is `codec: '<file>'
      starts with a UTF-16LE byte-order mark; leave out --from` (or BE),
      exit 2. No mark and no `--from`: the usage, exit 2.
    * `--check` writes to standard output on exit 0 and to standard error
      on exit 1; `cf_encoding_check`'s message is `codec: <message>`, its
      origin the path as given.
    * The output buffer is `3 * n` bytes (n after the mark), plus 1 when n
      is 0; n above `((size_t)-1 - 1) / 3` is `codec: '<file>' is too
      large`, exit 2.
    * Fixtures under `tests/codec/data/` (`w1254.ini` with a CRLF on its
      first line, kept on purpose; `w1254.utf8.ini`; `bad1254.ini`;
      `u16le.json`; `utf8.toml`), each with a `-text` line in
      `.gitattributes`; the test copies them into its work directory so
      the messages show bare file names.
  * `tests/codec/usage.cmake` checks only that the usage is on stderr and
    names `formats`; it needed no change. `recode.cmake` has no case for
    UTF-32 or a half UTF-16 unit (cmake cannot write such bytes with
    `file(WRITE)`); both are straight copies of `file.c`'s words.
  * Two things the card did not say. `gate.text` refuses a NUL or a
    carriage return in a tracked `.json` or `.ini`, so `scripts/check/
    check_text.py` leaves `tests/codec/data/` alone (bytes kept as they
    are). And the expected UTF-8 JSON is a fixture too
    (`u16le.utf8.json`): cmake's `file(WRITE)` turns `\n` into CRLF on
    Windows, so a file it wrote could not be compared byte for byte.
  * Found, not mine: on MSVC `tests/codec/pairs.c:639` (W5-V2) is warning
    C4702, unreachable code after `pr_die`, an error under `/WX`; `work.py
    win` stops there, so ACCEPT-WINDOWS was run with `ninja -k 0` and
    `ctest -R codec[.]`.
  * The `-DDATA` argument joins the `codec.*` scripts' command in
    `tests/codec/CMakeLists.txt` for every script, used by `recode` only.

---

## A-7 - The escapes in families

* **id**: A-7
* **goal**: the escape decoders of JSON, JSON5, TOML, YAML and
  `.properties` are one function with a table per family; the writers'
  escaping of JSON, TOML and YAML text is one function with a set per
  writer; the three `${` copies (`validate.c`, `emit.c`, `cbor.c`) are
  four small functions of `lib/text/`; and `yp_tag_put`'s `%XX` takes
  hexadecimal digits only. No input is read differently and no output
  byte changes, but for that one refusal.
* **why**: D-102. A fix to a surrogate pair or to a control character
  is made once, and the writers' differences (DEL, `\b`, `\f`, the C1
  controls) are written as data where a person can see them.
* **needs**: A-6 (the order); the code needs A-3 only (`cf_text.h`,
  `cf_text_hex_value`, `cf_text_utf8_encode`)
* **size**: large
* **decided**:
  * **What is one and what is not.** One decoder for the part every
    family shares: a letter after the backslash that stands for one code
    point (a table), or that opens 2, 4 or 8 hexadecimal digits, with
    UTF-16 pairs where the family has them. What differs between
    families is data: the table, the letters that take digits, two
    flags (`place` and `breaks`) and the rule for a line break. What stays in each reader: its
    messages, its limits and `put` (`js_put` leaves room for the NUL
    one way, `toml_put` another), TOML's line-ending backslash in a
    multi-line string (in toml.c), JSON5's "any other
    character stands for itself" (js5_escape's last lines), and the
    count of lines. XML's entities are expat's and INI has no escapes:
    neither is in this card. `ini_text_form` (ini.c) is not an
    escaper: it decides whether a value is quoted, writes no escape
    (INI has none), and stays where it is.
  * **Where `${` may live.** D-100's table: `cf_sink` is declared in
    `cf_serialize.h`, so `lib/text/` (which may use port, numeric,
    base, lanes) cannot take a sink; its functions take bytes. The
    three users may all call text: `cbor.c` is serialize (text allowed),
    `validate.c` and `emit.c` are config (everything allowed). So the
    one definition of "a `${` here" is in `lib/text/`, and each user
    keeps its own way of writing (a sink, an arena, the sink's buffer
    in place).
  * **The declarations**, added to `lib/text/include/configforge/cf_text.h`
    (A-3 made that header; read it first). The new types, enums and
    macros go just before the existing `#if CF_INTERNAL_API`, outside
    the fence; the new `CF_API` function declarations go inside the
    fence, right after the hex functions. Only functions are fenced, as D-88 has it, so
    the header still compiles alone. C89: tables are `static const` with
    positional initialisers.

    ```c
    /* One letter after the backslash that stands for one code point. */
    typedef struct cf_text_esc_simple {
        char   letter;
        cf_u32 cp;
    } cf_text_esc_simple;

    /* Where a refusal of the digits is placed, as an offset from the
       letter: at the letter, at the first digit, or at the first byte
       that is not a digit (or the end of the input). */
    enum { CF_ESC_AT_LETTER = 0, CF_ESC_AT_DIGITS = 1, CF_ESC_AT_BAD = 2 };

    /* Family flags. PAIRS: a \u escape is UTF-16, so a high half must be
       followed by a \uXXXX low half and the two make one code point, and
       a lone half is refused. NO_OCTAL: '0' not before a decimal digit
       is U+0000, and any digit '1' to '9' (or '0' before a digit) is
       refused, as ECMAScript's octal escapes are in JSON5. */
    enum { CF_ESC_PAIRS = 1u, CF_ESC_NO_OCTAL = 2u };
    enum { CF_ESC_NL_NONE = 0, CF_ESC_NL_JSON5, CF_ESC_NL_ANY };

    typedef struct cf_text_esc_family {
        const cf_text_esc_simple* simple; /* ended by letter 0 */
        const char*               hex2;   /* letters taking 2 digits: "x" or "" */
        const char*               hex4;   /* ... 4: "u" */
        const char*               hex8;   /* ... 8: "U" or "" */
        unsigned                  flags;  /* CF_ESC_PAIRS, CF_ESC_NO_OCTAL */
        unsigned                  place;  /* CF_ESC_AT_* */
        unsigned                  breaks; /* CF_ESC_NL_*: the escaped line
                                             breaks the family has */
    } cf_text_esc_family;

    enum {
        CF_ESC_CP = 0,   /* cp and used are set */
        CF_ESC_BREAK,    /* an escaped line break; used is its length */
        CF_ESC_OTHER,    /* no escape of this family: the caller decides */
        CF_ESC_E_DIGITS, /* too few or non-hexadecimal digits; letter is set */
        CF_ESC_E_HIGH,   /* a high half with no low half after it */
        CF_ESC_E_LOW,    /* a low half alone */
        CF_ESC_E_SCALAR, /* a surrogate or a value above U+10FFFF */
        CF_ESC_E_NUL_DIGIT, /* '0' before a decimal digit (NO_OCTAL) */
        CF_ESC_E_DIGIT      /* '1' to '9' (NO_OCTAL) */
    };

    typedef struct cf_text_esc {
        int    kind;
        cf_u32 cp;
        size_t used;   /* CF_ESC_CP, CF_ESC_BREAK: bytes from the letter, it
                          included; 0 for the others */
        size_t at;     /* a refusal: offset from the letter of its place */
        char   letter; /* CF_ESC_E_DIGITS: the letter, for the message */
    } cf_text_esc;

    /* Decodes the escape whose letter is at p (the byte after the
       backslash), reading nothing at or past end. Moves nothing. */
    CF_API void cf_text_esc_decode(const cf_text_esc_family* f, const char* p,
                                   const char* end, cf_text_esc* r);

    /* What to escape on writing. `named` is pairs of bytes: the byte, and
       the letter written after the backslash for it. Every other code
       point below `below`, and each from `from` to `to` inclusive
       (from > to: none), is written \u00XX (or \uXXXX), or with
       CF_ESCAPER_X below 0x100 as \xXX; with CF_ESCAPER_NONCHARS U+FFFE
       and U+FFFF as the six characters `\ufffe` and `\uffff`. Hex digits are
       lowercase. */
    enum { CF_ESCAPER_X = 1u, CF_ESCAPER_NONCHARS = 2u };
    typedef struct cf_text_escaper {
        const char* named;
        cf_u32      below;
        cf_u32      from, to;
        unsigned    flags;
    } cf_text_escaper;

    /* The escape for cp into out (at most 6 bytes are written; out holds
       8) and its length, or 0 when cp is written as it is. */
    CF_API size_t cf_text_escape(const cf_text_escaper* e, cf_u32 cp, char out[8]);

    /* The "${" of a reference. How many of them n bytes hold. */
    CF_API size_t cf_text_ref_count(const char* p, size_t n);
    /* The index of the '$' of the first "${" at or after `from`, or n. */
    CF_API size_t cf_text_ref_next(const char* p, size_t n, size_t from);
    /* "$${" read as "${", into out, which holds n bytes; the length. */
    CF_API size_t cf_text_ref_unescape(const char* p, size_t n, char* out);
    /* buf[from..end) widened in place, each "${" written "$${": from the
       back, with extra = cf_text_ref_count(buf + from, end - from); a
       byte is stored only where its new place is below `limit`. */
    CF_API void cf_text_ref_widen(char* buf, size_t from, size_t end, size_t limit);
    ```
  * **`cf_text_esc_decode`, the rules, in this order** (`p` at the
    letter, `c = *p`):
    1. `p == end`: kind OTHER, used 0 (YAML reads a quote's end as an
       unknown escape, `yp_c` giving -1; JSON and TOML test the end
       themselves before the call).
    2. `breaks` is not `CF_ESC_NL_NONE` and a break of that rule is at
       `p`: kind BREAK, used its length (LF 1, CR LF 2, CR alone 1,
       and for `CF_ESC_NL_JSON5` E2 80 A8 or E2 80 A9 3).
    3. With `CF_ESC_NO_OCTAL`: `c == '0'` and `p + 1 < end` and `p[1]` a
       decimal digit: E_NUL_DIGIT, at 0. `c` in `'1'` to `'9'`:
       E_DIGIT, at 0.
    4. `c` is a `letter` of `simple`: CP, `cp` from the table, used 1.
    5. `c` is in `hex2`, `hex4` or `hex8`: n is 2, 4 or 8. Otherwise
       OTHER. The n digits are at `p + 1`. If fewer than n bytes are
       left after the letter, or any is not a hexadecimal digit
       (`cf_text_hex_value`): E_DIGITS, `letter` set, `at` 0 for
       CF_ESC_AT_LETTER, 1 for AT_DIGITS, and for AT_BAD 1 plus the
       index of the first bad digit, or plus the digits that are left
       when the input ends first. `v` is the value; used is `1 + n`.
    6. With `CF_ESC_PAIRS` and `c` in `hex4`: `v` in D800 to DBFF is a
       high half: with `end - (p + 5) >= 6`, `p[5] == '\\'`, `p[6] ==
       'u'`, four hexadecimal digits after, and a value `lo` in DC00 to
       DFFF, the code point is `0x10000 + ((v - 0xD800) << 10) + (lo -
       0xDC00)` and used is 11; otherwise E_HIGH at 5. `v` in DC00 to
       DFFF: E_LOW at 5.
    7. Otherwise `v` above 0x10FFFF or in D800 to DFFF: E_SCALAR at
       `1 + n`; else CP, used `1 + n`.
    The offsets are what the readers place their failures at today:
    JSON at the `u` (`js_escape_u` in json.c) and after the first four
    digits for a half; TOML at the first digit and after the digits for
    a non-scalar (`toml_string` in toml.c); YAML at the bad digit and
    after the digits (`yp_escape` in yamlparse.c).
  * **The families**, each a `static const` in its reader, names
    prefixed so that the amalgamation (one translation unit) has no
    clash:

    | Name (file) | simple | hex2 | hex4 | hex8 | flags | place | breaks |
    | ----------- | ------ | ---- | ---- | ---- | ----- | ----- | ------ |
    | `js_family` (json.c) | `"` `\` `/` as themselves; `b` 8, `f` 12, `n` 10, `r` 13, `t` 9 | "" | "u" | "" | PAIRS | AT_LETTER | CF_ESC_NL_NONE |
    | `js5_family` (json.c) | JSON's, and `v` 11, `0` 0 | "x" | "u" | "" | PAIRS, NO_OCTAL | AT_LETTER | CF_ESC_NL_JSON5 |
    | `toml_family` (toml.c) | `"` `\` as themselves; `b` 8, `t` 9, `n` 10, `f` 12, `r` 13, `e` 27 | "x" | "u" | "U" | 0 | AT_DIGITS | CF_ESC_NL_NONE |
    | `yp_family` (yamlparse.c) | `0` 0, `a` 7, `b` 8, `t` 9, TAB 9, `n` 10, `v` 11, `f` 12, `r` 13, `e` 27, space 32, `"` 34, `/` 47, `\` 92, `N` 0x85, `_` 0xA0, `L` 0x2028, `P` 0x2029 | "x" | "u" | "U" | 0 | AT_BAD | CF_ESC_NL_ANY |
    | `prop_family` (properties.c) | `t` 9, `n` 10, `r` 13, `f` 12 | "" | "u" | "" | PAIRS | AT_LETTER | CF_ESC_NL_NONE |

    (`x` is in TOML's table whatever the TOML version: `toml.c` reads
    it today and A-7 keeps that.) `breaks` holds one of the header's
    values, `CF_ESC_NL_NONE` (0, no escaped
    break), `CF_ESC_NL_JSON5` (what JSON5's `js5_escape` takes today:
    LF, CR, CR LF as one, U+2028, U+2029) and
    `CF_ESC_NL_ANY` (YAML's: LF, CR, CR LF as one; the break itself is
    then taken by `yp_quoted_fold`). `cf_text_esc_decode`
    tests them with its own few lines. A-8 adds `cf_text_break` and
    its rules, and replaces the three by `CF_NL_NONE`, `CF_NL_JSON5`
    and `CF_NL_ANY` in the two families that use them.
  * **How each reader calls it** (the reader's own `put`, its own
    messages, as today). The readers are in `lib/serialize/src/`; each
    place is named by its function.
    * **JSON and JSON5**, `js_escape`, `js_escape_u`, `js5_escape`
      (all in json.c), `js_hex`, `js_put_cp`: one function
      `js_escape(s, dst, cap, n)` calls `cf_text_esc_decode(s->json5 ?
      &js5_family : &js_family, s->p, s->end, &r)` (the caller has
      tested `s->p < s->end`, as `js_string` does) and acts on
      `r.kind`: CP: `s->p += r.used`, then `js_put_cp`. BREAK: `s->p +=
      r.used; js_line(s)`. OTHER: in JSON `js_fail(s, s->p, "unknown
      escape")`; in JSON5 the one byte stands for itself (`++s->p`,
      `js_put` of that byte, as `js5_escape` does). E_DIGITS: `js_fail(s, s->p +
      r.at, ...)` with `\u needs four hexadecimal digits` for `r.letter
      == 'u'` and `\x needs two hexadecimal digits` for `'x'`, each
      split after the backslash exactly as `js_escape_u` and
      `js5_escape` write them (pcc reads a backslash then `u` as a universal character
      name). E_HIGH: `high surrogate without a low one`; E_LOW:
      `unpaired low surrogate`; E_NUL_DIGIT: `\0 may not be followed by
      a digit`; E_DIGIT: `a digit may not be escaped`; each at `s->p +
      r.at` (these messages are single literals, as `js5_escape` writes
      them; only the two with a backslash before `u` or `x` are split).
      `js_utf8` (already gone by A-3) and `js5_is_ls`'s use inside the
      escape go. `js_hex` stays, calling `cf_text_hex_value`: `js5_name`
      in json.c still reads `\u` with it, as A-3 left it. (`js5_hex`,
      the JSON5 hexadecimal number, calls `cf_text_hex_value` itself
      and is not touched.)
      `js5_is_ls` stays for `js_space` until A-8.
    * **TOML**, the escape in `toml_string` (toml.c): after `++s->p` the code
      today finds the letter; the new code calls `cf_text_esc_decode(
      &toml_family, s->p, s->end, &r)` first, with `s->p` still on the
      letter (the `s->p >= s->end` test stays before it).
      CP: `s->p += r.used`, `k = cf_text_utf8_encode(cp, enc)`, the
      room test as it is (`n + k + 1 > cap`, "the string is too long"),
      copy. OTHER: `toml_fail(s, s->p, "unknown escape")`.
      E_DIGITS: `toml_fail(s, s->p + r.at, TOML_UNICODE_DIGITS)`.
      E_SCALAR: `toml_fail(s, s->p + r.at, "not a Unicode scalar
      value")`. The `case` list, `digits`, `one`, the digit loop and
      its inline `cf_text_hex_value` calls are deleted. The
      line-ending backslash in `ml` strings is before this code and unchanged.
    * **YAML**, `yp_escape` (yamlparse.c): called with `y->p`
      on the letter, as now (its own `++y->p` before the digits goes). `cf_text_esc_decode(
      &yp_family, y->p, y->end, &r)`. BREAK: `return yp_quoted_fold(y,
      k, n, CF_TRUE)` with `y->p` unmoved, as `yp_escape` does. OTHER: `return
      yp_fail(y, "an unknown escape")`. E_DIGITS: `yp_fail_at(y, y->p
      + r.at, "an escape needs its hexadecimal digits")`. E_SCALAR:
      `yp_fail_at(y, y->p + r.at, "an escape names no character")`.
      CP: `y->p += r.used; return yp_put_cp(y, k, r.cp)` (`yp_put_cp`
      stays, calling `cf_text_utf8_encode` as A-3 left it).
    * **`.properties`**, `prop_element` (properties.c, its escape part): a `\`
      not at the line's end is followed by `cf_text_esc_decode(
      &prop_family, s->p + 1, eol, &r)` (A bound: `eol`, the physical
      line's end, as today's `< eol` tests.) CP: `s->p += 1 + r.used`,
      then the inline block in properties.c that encodes with
      `cf_text_utf8_encode` and tests `n + ek > cap` (kept as it is; the two
      messages `the key is too long` and `the value is too long`
      unchanged). OTHER: the letter stands for itself (`out[n++] =
      c`, as today). E_DIGITS: `prop_fail(s, "\\" "u needs four
      hexadecimal digits")` (split as it is today). E_HIGH and E_LOW:
      `prop_fail(s, "a \\" "u escape is half a surrogate pair")`.
  * **The writers.** Each keeps its loop and its sink; the choice of
    what to escape is data:

    | Set (file) | named | below | from..to | flags |
    | ---------- | ----- | ----- | -------- | ----- |
    | `json_escaper` (blob.c, for `cf_sink_json_string`) | `"` `"`, `\` `\`, `\b` `b`, `\f` `f`, `\n` `n`, `\r` `r`, `\t` `t` | 0x20 | 1..0 | 0 |
    | `toml_escaper` (toml.c, for `toml_quote`) | `"`, `\`, `\n` `n`, `\r` `r`, `\t` `t` | 0x20 | 0x7F..0x7F | 0 |
    | `yaml_escaper` (yaml.c, for `yaml_quote`) | `"`, `\`, `\n` `n`, `\t` `t` | 0x20 | 0x7F..0x9F | X, NONCHARS |

    In TOML `\b` and `\f` have no short form, so they come out `\u0008`
    and `\u000c`; in YAML `\r` comes out `\x0d` and U+0085 `\x85`; JSON
    leaves DEL as it is. These are today's bytes (`cf_sink_json_string`
    in blob.c, `toml_quote` in toml.c, `yaml_quote` in yaml.c) and they stay. JSON and TOML
    call `cf_text_escape(&set, c, esc)` with each **byte** (as today:
    bytes from 0x80 are passed on, and none of their ranges reaches
    them); YAML with each decoded **code point** (its loop stays on
    `yaml_utf8`, which A-3 turned into `cf_text_utf8_decode`). A length
    of 0 is `cf_sink_putc` (or, for YAML, `cf_sink_write` of the
    sequence); else `cf_sink_write(out, esc, len)`. The `"0123456789abcdef"`
    tables and the `cf_sink_fmt` calls go.
  * **The `${` users.**
    * `validate.c` `unescape` keeps the arena: `out =
      cf_arena_alloc(ar, n + 1)`; `m = cf_text_ref_unescape(p, n,
      out); out[m] = '\0'`. The loop is `cf_text_ref_unescape`'s:
      `$${` with `i + 2 < n` drops one `$` of the pair.
    * `emit.c` `literal_scalar` counts `extra` with
      `cf_text_ref_count(v->u.t.p, v->u.t.n)`; `escape_written`
      keeps its first lines (a NULL buffer, a zero `cap`,
      `limit`, the clamp to `end`, `from >= end`) and ends with
      `cf_text_ref_widen(out->buf, from, end, limit)`.
    * `cbor.c` `cbor_put_text` counts with
      `cf_text_ref_count`, writes the head, then `for (i =
      cf_text_ref_next(p, n, 0); i < n; i = cf_text_ref_next(p, n, i +
      1))` writes `p + from .. i + 1`, the extra `$`, and sets `from =
      i + 1`.
  * **The one change of behaviour**, `yp_tag_put` (yamlparse.c, its
    `%XX` escape): `hi` and `lo` come from
    `cf_text_hex_value((unsigned char)p[i + 1])` and `[i + 2]`, and a
    negative one is `yp_fail(y, "an escape in a tag is two hexadecimal
    digits")` as now. Today `(c | 0x20) - 'a' + 10` reads `@` and a
    backquote as the digit 9 (0x40 and 0x60 both give 0x60), so
    `!<tag:x,2000:%@0>` is read as the byte 0x90; now it is refused.
    Nothing else changes: a `%` with fewer than two bytes after it
    (`i + 2 < k` false) still passes through as it is, and `%41`,
    `%4a`, `%4A` still read as `A`, `J`, `J`. It is the built-in
    reader's: with libyaml or libfyaml chosen (`CF_YAML_LIBYAML`,
    `CF_YAML_LIBFYAML`) those libraries decide, so the test is under
    `#if` of the built-in parser (`cf_yamlparse.h`'s macros both 0).
    Under "### Changed" (docs/Maintaining.md allows a refusal of what
    was never valid in a minor).
  * **The proof of "no input read differently".**
    1. `lib/serialize/tests/test_text_escape.c` (new). Part one, the decoder,
       one table per family of `(input after the backslash, end offset,
       kind, cp, used, at, letter)`, at least:
       * JSON: `n` CP 10 used 1; `"` 34; `/` 47; `u0041` 0x41 used 5;
         `u00e9` 0xE9; `uD83D\uDE00` 0x1F600 used 11; `uD83D` E_HIGH at
         5; `uD83Dx` E_HIGH; `uD83DA` E_HIGH; `uDE00` E_LOW at 5;
         `u004` E_DIGITS at 0 letter `u`; `u00zz` E_DIGITS at 0; `q`
         OTHER; `x41` OTHER; `0` OTHER; an empty input OTHER.
       * JSON5: `v` 11; `0` 0; `01` E_NUL_DIGIT at 0; `7` E_DIGIT;
         `x41` 0x41 used 3; `xE9` 0xE9; `x4` E_DIGITS at 0 letter `x`;
         LF BREAK 1; CR LF BREAK 2; CR then `a` BREAK 1; the bytes E2
         80 A8 and E2 80 A9 BREAK 3; `'` OTHER; `q` OTHER.
       * TOML: `e` 27; `b` 8; `x41`; `u00e9`; `U0001F600` 0x1F600 used
         9; `u004` E_DIGITS at 1; `uZ000` E_DIGITS at 1; `uD800`
         E_SCALAR at 5; `U00110000` E_SCALAR at 9; `U0000D800`
         E_SCALAR at 9; `/` OTHER; `v` OTHER; LF OTHER.
       * YAML: `0` 0; `a` 7; TAB 9; `v` 11; `e` 27; space 32; `/` 47;
         `N` 0x85; `_` 0xA0; `L` 0x2028; `P` 0x2029; `x41`;
         `U0001F600`; `x4g` E_DIGITS at 2; `x4` E_DIGITS at 2 (the
         input ends); `xg1` E_DIGITS at 1; `uD800` E_SCALAR at 5;
         `U00110000` E_SCALAR at 9; LF BREAK 1; CR LF BREAK 2; `q`
         OTHER.
       * properties: `t` 9; `u0041`; `uD83D\uDE00` used 11; `uD83D`
         E_HIGH; `uDE00` E_LOW; `u00` E_DIGITS; `=` OTHER; and `u00e9`
         with `end` after the fourth byte: E_DIGITS (the bound is
         honoured).
       Part two, the writers: the old `cf_sink_json_string`,
       `toml_quote` and `yaml_quote` are copied **verbatim** into the
       test as `old_json`, `old_toml`, `old_yaml` (writing to a
       `cf_sink`), and compared with the new output for every byte 0 to
       255 alone and between two letters (JSON, TOML), and for every
       code point 0 to 0x10FFFF, encoded (YAML). Part three, `${`: the
       old three copied verbatim as `old_unescape`, `old_widen` (on a
       `cf_sink`'s buffer) and `old_cbor_text`, compared with the new
       code on **every string of length 0 to 8 over the alphabet `$`
       `{` `}` `a`** (87,381 strings), `widen` with every `limit` from 0
       to the widened length plus 2, `end` clamped to `limit` as
       `escape_written` clamps it (emit.c, "the clamp to `end`"), since
       `cf_text_ref_widen` is called after that clamp and never with
       `end` above `limit`. Part four, the tag: the cases of "The one
       change".
    2. `lib/config/tests/test_reader_corpus.c` (new; it drives dotenv,
       which is in the configuration library) and
       `tests/support/data/reader_corpus.golden` (its path is a
       compile definition, `CF_READER_CORPUS_GOLDEN`, set with
       `target_compile_definitions` in `lib/config/tests/CMakeLists.txt`
       as `CF_EDGES_DIR` is there): for each of JSON, JSON5,
       TOML, built-in YAML, INI, `.properties` and dotenv, every
       document of length 0 to the length below over its alphabet,
       decoded with the codec's `decode` (dotenv has no codec: the
       document goes to `cf_lex_dotenv` through an entry sink that
       appends, for each entry, the key, the text and its place) into a
       sink the test writes,
       which appends to a buffer, for each event, the kind, the name,
       the text and `line:col` of its place, and at the end the
       result and the error message. The buffer of each document is
       hashed (FNV-1a, 64 bits, in `cf_u64`), and the hashes of one
       reader and length are hashed again into one number. The golden
       file is one line a reader and length: `json 4 0x...`. Alphabets
       (a symbol is one string; `LS` is the bytes E2 80 A8, `FF` form
       feed): JSON and JSON5 (16 symbols, length 4): `{` `}` `"` `:`
       `,` `a` `0` space TAB CR LF `/` `*` `\` `u` `LS`; and for JSON5
       a second run of length 4 over `'` `x` `1` `v` `\` `u` `0` `D`
       `8` CR LF `"` `{` `}` `:` `a`; TOML (17, length 4): `[` `]` `=`
       `a` `1` `"` `'` `#` space TAB CR LF `\` `,` `{` `}` `.`, and a
       second run over `"` `\` `u` `x` `U` `0` `D` `8` `e` `b` LF CR
       `=` `a` space `'`; YAML (19, length 4): `a` `:` space TAB `-` `#` LF
       CR `"` `'` `\` `|` `[` `]` `{` `}` `,` `!` `%`, and a second run
       over `"` `\` `x` `u` `U` `0` `D` `8` `N` `L` LF CR `a` `:` space
       `/`; INI (11, length 5): `[` `]` `=` `a` space TAB CR LF `#`
       `;` `"`; `.properties` (13, length 4): `a` `=` `:` space TAB FF
       CR LF `#` `!` `\` `u` `0`, and a second run of length 4 over `\`
       `u` `D` `8` `0` `4` `1` `t` `=` `a` LF CR; dotenv (9, length 5):
       `A` `=` space TAB CR LF `#` `"` `'`. No `@` and no backquote in
       any (the tag change would move the golden). Each run is under
       200,000 documents; the test takes seconds. The golden is
       written by `test_reader_corpus --write` **on the code before the
       card changes any reader** (step 2) and checked in; the run
       without `--write` compares. A failing line names the reader and
       the length; `--dump <reader> <length>` prints the documents'
       results for a bisection by hand. The YAML part is under `#if`
       of the built-in parser. A-8 reuses it unchanged.
    3. The reader-level cases of the escapes' places, through each
       codec's `decode` in `test_text_escape.c` (the three pinned here,
       the rest in the golden). Check them against the code **before**
       the card (they are what it gives): `{"a":"\u12"}` as `t.json`:
       `t.json:1:8: \u needs four hexadecimal digits`; `a = "\u12"` as
       `t.toml`: `t.toml:1:8: \x needs two hex digits, \u four and \U
       eight`; `a: "\x4g"` as `t.yaml`: `t.yaml:1:8: an escape needs its
       hexadecimal digits`; `a=\u12` as `t.properties`:
       `t.properties:1: \u needs four hexadecimal digits`.
  * **What the corpus sink writes.** Into the hashed buffer,
    platform-independently: an integer as 16 lowercase hexadecimal
    digits of its two's-complement value as `cf_u64`; a real as 16
    lowercase hexadecimal digits of its IEEE-754 bits (`memcpy` into a
    `cf_u64`); a boolean `t` or `f`; null `n`; text as its bytes. The
    kind letters and the separators are as the card already says.
  * **`test_reader_corpus`'s command line.** No argument: compare with
    the golden. `--write`: rewrite the golden at
    `CF_READER_CORPUS_GOLDEN`. `--dump <reader> <length>`: print each
    document and its buffer for that reader and length to standard
    output. Its ctest entry has `TIMEOUT 600`.
  * **`cf_text_escape`'s ranges.** `to` is at most 0xFFFF: a code point
    above U+FFFF is never escaped (it is written as its UTF-8). The
    test asserts that every table of the card obeys it.
* **files**: `lib/text/include/configforge/cf_text.h`,
  `lib/text/src/escape.c` (new: `cf_text_esc_decode`, `cf_text_escape`),
  `lib/text/src/refs.c` (new), `lib/text/CMakeLists.txt` (their place in
  both lists, `SOURCES` and `SERIALIZE_SOURCES`, where A-3 put `utf8.c`,
  `hex.c`, `hash.c`), `lib/config/tests/CMakeLists.txt`
  (`CF_READER_CORPUS_GOLDEN`),
  `scripts/generate/amalgamate.py` (the two sources in `SOURCES`),
  `lib/serialize/src/{json,toml,yamlparse,properties,blob,yaml,cbor}.c`,
  `lib/config/src/core/validate.c`, `lib/config/src/bridge/emit.c`,
  `cmake/configforge.internal.symbols` (gate.abi with `-DWRITE=ON`:
  `cf_text_esc_decode`,
  `cf_text_escape`, `cf_text_ref_count`, `cf_text_ref_next`,
  `cf_text_ref_unescape`, `cf_text_ref_widen`; all internal, none
  experimental or public); tests: `lib/serialize/tests/test_text_escape.c`
  (it includes `cf_serialize.h`, so by A-1b's rule it is in `lib/serialize/tests/`),
  `lib/config/tests/test_reader_corpus.c`,
  `tests/support/data/reader_corpus.golden`
  (new; both tests in their component's test list),
  `lib/config/tests/test_yaml.c` (the
  tag cases may go here instead of part four if its helpers fit),
  `lib/config/tests/test_fuzz.c` (one seed a family: `{"a":"😀\x41"}`
  as JSON5, `a = "\U0001F600\e"`, `a: "\x41é\N\L"`,
  `a=😀\t`, `a: !<t:%41>`, so the new decoder is fuzzed);
  last: `CHANGELOG.md`, `docs/Architecture.md` (the text component's
  files), `docs/Decisions.md`.
* **steps**:
  1. If `## 1.8.0 — unreleased` is not open, it is (A-1 step 0).
  2. `lib/config/tests/test_reader_corpus.c`; run it with `--write` on
     the tree as it is; commit the golden with the test. Run it again
     without `--write`: it passes. (This test passes before the card's
     code and after it; it is the one that must keep passing.) To see
     it fail, as the loop asks: change one function of one reader on
     purpose (for example `js_escape`'s `\n` to give 11), see the corpus
     test name that reader and length, restore it.
  3. `lib/serialize/tests/test_text_escape.c`, parts one to three, with the old
     copies; the declarations in `cf_text.h` and empty bodies; see the
     test fail.
  4. `escape.c` and `refs.c`; parts one to three pass (the decoder and
     the three old-against-new comparisons); no reader uses them yet.
  5. One reader at a time, `ctest --preset default` and
     `test_reader_corpus` after each, and the conformance suites after
     JSON, TOML and YAML (`cmake -DMODE=quick -DEXTRA=conformance -P
     scripts/verify.cmake`: the block of scripts/verify.cmake that
     builds `build-conformance` and runs `scripts/check/conformance.py`
     against `tests/conformance/floor.txt` does it alone; the same run
     also runs `codec_pairs` against `tests/codec/pairs.manifest`
     (W5-V2), a second proof for the writers): JSON and JSON5 together; TOML; YAML; `.properties`.
  6. One writer at a time, same checks: `cf_sink_json_string`,
     `toml_quote`, `yaml_quote`.
  7. One `${` copy at a time: `validate.c`, `emit.c`, `cbor.c`; after
     the first two, `ctest` (the interpolation tests of
     `test_interpolate.c` hold `validate.c` and `emit.c`); after
     `cbor.c`, `test_cbor` and the CBOR conformance suite.
  8. Part four first, as a test that fails (the `%@0` and `` %`0 ``
     cases accepted today); then the `yp_tag_put` change; it passes and
     the corpus golden still matches (no `@` or backquote in it).
  9. The symbol lists (`cmake -DSRC=. -DWRITE=ON -P
     cmake/gates/GateAbi.cmake`: the internal list only; no promised
     name changes here), the amalgamation, the
     documents; ACCEPT.
* **accept**: ACCEPT-LINUX with
  `-DEXTRA="conformance;fuzz;coverage;abi;yaml;components"` (conformance fetches its
  suites with git at pinned commits and needs the network and Python;
  fuzz needs clang; `components` builds each component of `lib/` alone,
  so `cf_text.h` and the new sources stand without the others; `yaml` builds the library backends, which share
  `yaml.c`'s writer), ACCEPT-WINDOWS, ACCEPT-CI. Conformance scores equal
  to `tests/conformance/floor.txt`'s, no record raised or lowered.
* **stop if**: `test_reader_corpus` fails after a reader is switched
  and the difference is not the card's one: the old behaviour is the
  right one, do not change the golden; find the difference, put it in
  the decoder's table or flag as data if it is a format's, and if it
  cannot be (a difference that would need a seventh flag), stop and
  report with the document. A conformance record moves: stop.
* **docs**: D-102's entry in docs/Decisions.md reads "Built, in part: A-3 (...)
  To build: A-7 and A-8"; A-7 appends itself and its commit to "Built"
  and leaves "To build: A-8". CHANGELOG `### Changed`: "The readers share one
  escape decoder a family (JSON and JSON5, TOML, YAML, `.properties`),
  the writers one escaper, and the three `${` copies one definition
  (D-102); no input is read differently and no output changes. In a
  YAML tag, `%` followed by characters that are not both hexadecimal
  digits is refused by the built-in reader; `%@0` and a backquote in
  place of a digit were read as the digit 9 (D-102)." `docs/Roadmap.md`:
  the item "Infrastructure behind abstractions" stays 🟨, its line gains
  "A-7 done".
* **notes**:

---

## A-8 - Whitespace, comments, newlines and the place of a refusal

* **id**: A-8
* **goal**: the readers count lines and columns with one tracker, skip
  blanks, comments and line breaks with one function per syntax whose
  newline rule is a parameter, build their failure message in one
  function, share `ini_trim`/`dotenv_trim` as one, and the serialization
  library's TLV layout check calls `cf_key_valid` from `lib/base/` where
  it had a copy. No input is read differently and no message changes.
* **why**: D-102, the last of the readers' copies, and the half of
  D-107 that A-10 needs: when every reader's place is one tracker and
  every message one function, `where` (a `cf_loc`) is filled at one
  place.
* **needs**: A-7 (the corpus test and `cf_text.h`'s escape part), A-4
  (`cf_text_place`)
* **size**: large
* **decided**:
  * **The declarations**, in `lib/text/include/configforge/cf_text.h`,
    after A-7's (as there, only the `CF_API` function declarations are
    inside `#if CF_INTERNAL_API`; types, enums and macros are outside,
    D-88). `unsigned int` for lines
    and columns, as `cf_loc` and every reader has them (`%u`);
    `cf_text_place` (A-4) keeps its `cf_u32`.

    ```c
    /* Line-break rules. */
    enum {
        CF_NL_LF = 0, /* LF only: JSON, INI */
        CF_NL_JSON5,  /* LF, CR LF, a lone CR, U+2028, U+2029 */
        CF_NL_TOML,   /* LF, CR LF; a lone CR is no break */
        CF_NL_ANY,    /* LF, CR LF, a lone CR: YAML, .properties */
        CF_NL_NONE    /* none */
    };
    /* The length of the break at p (CR LF is 2, U+2028 is 3), or 0. */
    CF_API size_t cf_text_break(unsigned rule, const char* p, const char* end);
    /* The first break at or after p, or end. */
    CF_API const char* cf_text_eol(unsigned rule, const char* p, const char* end);

    /* Blank sets, within a line. */
    enum {
        CF_BLANK_SPTAB = 0, /* space, TAB: YAML, JSON5 */
        CF_BLANK_INI,       /* space, TAB, CR: INI, JSON, dotenv */
        CF_BLANK_PROP,      /* space, TAB, FF: .properties */
        CF_BLANK_TOML       /* space, TAB, and a CR followed by LF */
    };
    CF_API const char* cf_text_blanks(unsigned set, const char* p, const char* end);

    /* The same trim INI and dotenv do: space, TAB and CR at both ends. */
    CF_API size_t cf_text_trim(const char* p, size_t n, const char** out);

    /* Where a line starts, and which line. */
    typedef struct cf_text_track {
        const char*  line_start;
        unsigned int line;
    } cf_text_track;
    CF_API void cf_text_track_init(cf_text_track* t, const char* start);
    /* A line ends: the next starts at `after`. */
    CF_API void cf_text_track_line(cf_text_track* t, const char* after);
    /* 1-based, in bytes. */
    CF_API unsigned int cf_text_track_col(const cf_text_track* t, const char* at);

    /* Skipping. */
    enum {
        CF_SYN_SLASH = 1u,        /* // and block comments; ECMAScript spaces */
        CF_SYN_HASH = 2u,         /* '#' to the end of the line */
        CF_SYN_HASH_BLANK = 4u,   /* with HASH: only at a line's start or
                                     after a blank (YAML) */
        CF_SYN_TOML_COMMENT = 8u  /* with HASH: the body is checked */
    };
    typedef struct cf_text_syntax {
        unsigned blanks; /* CF_BLANK_* */
        unsigned rule;   /* CF_NL_* */
        unsigned flags;  /* CF_SYN_* */
    } cf_text_syntax;
    enum { CF_SKIP_OK = 0, CF_SKIP_UNCLOSED, CF_SKIP_CONTROL, CF_SKIP_UTF8 };
    /* Past blanks, comments and line breaks, counting each break in `t`,
       to the next byte that is none of them. On a refusal *bad is where
       it is; *pp is left at the end of what was read, as `js_space`
       leaves `s->p` today (at the end of the input for an unclosed
       comment), and `t` has counted the breaks passed. */
    CF_API int cf_text_skip(const cf_text_syntax* sy, cf_text_track* t,
                            const char** pp, const char* end, const char** bad);
    /* One '#' comment at *pp, to its line's end, the break not taken;
       checked as TOML's if the flags say so. */
    CF_API int cf_text_comment(const cf_text_syntax* sy, const char** pp,
                               const char* end, const char** bad);

    /* The failure of a reader, in the form every reader uses. Sets err
       (code `code`, no key, source `file`) to "file:line:col: body"; col
       0 gives "file:line: body". `fmt` is cf_fmt's, `%S` included.
       Returns CF_FALSE. line is 1 or more. */
    CF_API cf_bool cf_text_fail(cf_error* err, cf_code code, const char* file,
                                unsigned int line, unsigned int col, const char* fmt, ...);

    /* Whether cp is a space of ECMAScript 5.1 (JSON5's WhiteSpace and
       LineTerminator past ASCII): what `js5_is_space` decides today,
       U+2028 and U+2029 included. */
    CF_API cf_bool cf_text_is_ecma_space(cf_u32 cp);
    ```
  * **The rules**, byte for byte what the readers do today:
    * `cf_text_break`: `CF_NL_LF`: `*p == '\n'` gives 1. `CF_NL_JSON5`:
      LF 1; CR LF 2; a CR not followed by LF 1; E2 80 A8 and E2 80 A9 3
      (`js_space`, `js5_comment_step`, `js5_escape` in json.c).
      `CF_NL_TOML`: LF 1; CR LF 2; CR alone 0 (`toml_newline` in toml.c). `CF_NL_ANY`: LF 1; CR LF 2; CR alone 1
      (`yp_eat_break` in yamlparse.c, `prop_next_line` in properties.c). NONE: 0.
      `p == end` is 0 for all.
    * `cf_text_eol`: steps by one byte until `cf_text_break` is above 0
      (`prop_eol`, `ini_skip_to_eol`, the comment loops). Stepping by
      byte, not by code point, is the same for every rule: no break
      starts with a continuation byte.
    * `cf_text_blanks`: the bytes of the set, as listed; for
      `CF_BLANK_TOML` a CR is blank only when `end - p >= 2` and `p[1]
      == '\n'` (`toml_blank` in toml.c).
    * `cf_text_trim`: `ini_trim` (ini.c) and `dotenv_trim`
      (entry.c) are the same function; this is it.
    * `cf_text_track_col`: `(unsigned)(at - line_start) + 1u`. The
      tracker is **not** built on `cf_text_place` (A-4, `place.c`):
      that function counts from the start of the input to one offset,
      by LF, for the one refusal A-4 and A-5 make; a reader asks for a
      place at every value and every failure and cannot rescan, so its
      count must be running; and the line-break rule differs per
      format. They agree where they overlap, and a test holds them to
      it (below). If A-4's `cf_text_place` is found to count a break
      differently from `CF_NL_LF`, stop (see "stop if").
    * `cf_text_skip` loops until nothing more is skipped: (1) at a
      break (`cf_text_break(sy->rule)` above 0): `*pp += b;
      cf_text_track_line(t, *pp)`; (2) blanks per `sy->blanks`; (3)
      with `CF_SYN_SLASH`: `//` to `cf_text_eol`; `/*` to `*/`, where
      each position is: end of input, refusal `CF_SKIP_UNCLOSED` with
      `*bad = end`; `*/`, done; a break (`sy->rule`), counted; else one
      byte; and a byte that is the first of an ECMAScript space (`js5_is_space`
      in json.c, moved into `space.c`, decoded with
      `cf_text_utf8_decode`) is skipped (a break among them is rule
      JSON5's, handled in (1)); (4) with `CF_SYN_HASH`: a `#` is a
      comment, but with `CF_SYN_HASH_BLANK` only if `p == t->line_start`
      or `p[-1]` is a space or TAB (`yp_comment_here`), skipped
      by `cf_text_comment`. The order is (1), (2), (3), (4) because
      that is the order in which `js_space`, `toml_space_lines` and
      `yp_skip` test a byte, and they never disagree on one.
    * `cf_text_comment` with `CF_SYN_TOML_COMMENT`: `toml_comment`
      (in toml.c): until LF, a CR followed by LF is passed; a byte
      below 0x20 but TAB, or DEL, is `CF_SKIP_CONTROL` at it; a byte that
      does not start a whole UTF-8 sequence (`cf_text_utf8_decode`
      returns 0) is `CF_SKIP_UTF8` at it. Without the flag it goes to
      `cf_text_eol(sy->rule)`.
    * `cf_text_fail`: `cf_vfmt` of `fmt` into a `char body[CF_MESSAGE_MAX]`,
      then `cf_error_set(err, code, NULL, file, "%s:%u:%u: %s", file,
      line, col, body)`, or `"%s:%u: %s"` with col 0. A message cut at
      `CF_MESSAGE_MAX` is the same cut as before (prefix, then body, then
      the cut). It joins `lib/text/src/fail.c` (new).
  * **The syntaxes**, each a `static const cf_text_syntax` in its reader:

    | Name (file) | blanks | rule | flags |
    | ----------- | ------ | ---- | ----- |
    | `js_syntax` (json.c, JSON) | `CF_BLANK_INI` | `CF_NL_LF` | 0 |
    | `js5_syntax` (json.c, JSON5) | `CF_BLANK_SPTAB` | `CF_NL_JSON5` | `CF_SYN_SLASH` |
    | `toml_syntax` (toml.c) | `CF_BLANK_TOML` | `CF_NL_TOML` | `CF_SYN_HASH \| CF_SYN_TOML_COMMENT` |
    | `yp_syntax` (yamlparse.c) | `CF_BLANK_SPTAB` | `CF_NL_ANY` | `CF_SYN_HASH \| CF_SYN_HASH_BLANK` |
    | INI | `cf_text_blanks(CF_BLANK_INI)`, `cf_text_eol(CF_NL_LF)`: no composite skip | | |
    | `.properties` | `cf_text_blanks(CF_BLANK_PROP)`, `cf_text_eol(CF_NL_ANY)`, `cf_text_break(CF_NL_ANY)` | | |

    JSON's CR is a blank (a JSON text counts LF only), JSON5's a break;
    TOML's CR is blank only before LF, which as a rule means a CR LF
    break; INI treats CR as a blank. In A-7's decoder the three
    `CF_ESC_NL_*` values become `CF_NL_NONE`, `CF_NL_JSON5` and `CF_NL_ANY`
    (the family field `breaks` holds a `CF_NL_*`), and the decoder's own
    few lines for them are a call of `cf_text_break`; the `CF_ESC_NL_*` names
    go.
  * **How each reader changes** (names today's, of
    `lib/serialize/src/...`):
    * **json.c**: `js_scan`'s `line_start` and `line` become `cf_text_track
      tr`; `js_line` is `cf_text_track_line(&s->tr, s->p)`; `js_loc`'s
      column `cf_text_track_col(&s->tr, at)`; `js_fail` is
      `cf_text_fail(s->err, CF_E_SOURCE, s->file, s->tr.line, col,
      "%s", what)`; `js_space`, `js5_comment_step` and the
      two comment loops are one call of `cf_text_skip(s->json5 ?
      &js5_syntax : &js_syntax, &s->tr, &s->p, s->end, &bad)` whose
      `CF_SKIP_UNCLOSED` is `js_fail(s, bad, "a comment is not
      closed")`; `js5_is_ls` is used by A-7's decoder no more and goes,
      `js5_is_space` stays for `js5_is_id`, now a call of the
      function in `space.c` (`cf_text_is_ecma_space`, declared above and
      in the internal list). JSON5's line continuation in
      a string (A-7's BREAK) calls `cf_text_track_line`. The string's
      "raw newline" test in `js_string` is not a skip and stays.
    * **toml.c**: `toml_scan`'s pair likewise; `toml_blank` is
      `cf_text_blanks(CF_BLANK_TOML, ...)`; `toml_comment` is
      `cf_text_comment(&toml_syntax, ...)` with `CF_SKIP_CONTROL` as
      `toml_fail(s, bad, "a comment may not contain a control
      character")` and `CF_SKIP_UTF8` as `"a comment is not valid
      UTF-8"`; `toml_space_lines` is `cf_text_skip(&toml_syntax, ...)`
      with the same two messages; `toml_end_of_line` keeps its shape on
      the new calls; `toml_newline` is `cf_text_break(CF_NL_TOML, ...)`
      plus `cf_text_track_line` when it is above 0.
    * **ini.c**: `ini_scan`'s pair; `ini_col`, `ini_newline`,
      `ini_skip_blank`, `ini_skip_to_eol`, `ini_trim` become the calls
      named in the table; every `cf_error_set(... "%s:%u: ..." ...)` of
      the decoder (sixteen of them) becomes
      `cf_text_fail(err, <the same code>, file, line, 0, <the same
      text>)` with the format's `%S` and `%u` arguments as they are.
      The `"%s: the document could not be recorded"` ones
      and `sink refused` carry no place and are not touched. Look-back
      `ini_earlier_run`'s own loop counts lines by LF and
      uses `ini_trim`: `cf_text_trim`.
    * **properties.c**: the pair; `prop_eol` and `prop_next_line` are
      `cf_text_eol(CF_NL_ANY, ...)` and `p += cf_text_break(CF_NL_ANY,
      p, end); cf_text_track_line(&tr, p)` (counted even when the break
      length is 0, as today, at the end of input after a continuation, in `prop_next_line`); the blank skips in
      properties.c are
      `cf_text_blanks(CF_BLANK_PROP)`; `prop_fail` and the two other
      placed failures in properties.c are
      `cf_text_fail` with col 0 (the last with `CF_E_KEY`).
    * **yamlparse.c**: `yp`'s `ls` and `line` become `cf_text_track tr`
      (55 uses, mechanical: `y->ls` is `y->tr.line_start`, `y->line`
      is `y->tr.line`); `yp_quoted` saves and restores the tracker as
      one struct copy (at its start and where it restores); `yp_col` is `cf_text_track_col`;
      `yp_fail_at` is `cf_text_fail(y->err, CF_E_SOURCE, y->file,
      y->tr.line, col, "%s", what)` (keeping its test that `q` lies in
      the line); `yp_eat_break` is `y->p += cf_text_break(CF_NL_ANY,
      y->p, y->end); cf_text_track_line(&y->tr, y->p)`; `yp_blanks`
      `cf_text_blanks(CF_BLANK_SPTAB, ...)`; `yp_skip` is
      `cf_text_skip(&yp_syntax, ...)` (it never refuses: the result is
      ignored); `yp_line_rest` keeps its shape on `cf_text_blanks`,
      `cf_text_comment` and `cf_text_eol`.
    * **yaml.c** (`yaml_fail_at`) and **xml.c** (`xml_fail`
      and the parse error): `cf_text_fail`. XML's
      "what (name)" is formed first into a `char what[CF_MESSAGE_MAX]`
      with `cf_fmt(what, sizeof what, "%s%s%s%s", ...)` as the
      `%s%s%s%s` part reads today, then passed as `"%s"`. The
      line and column are expat's (`XML_GetCurrentLineNumber`,
      `XML_GetCurrentColumnNumber() + 1u`): XML has no tracker.
    * **`lib/config/src/bridge/schema.c`**: the placed form is
      `cf_text_fail(w->err, CF_E_DEFINITION, w->file, at->at.line,
      at->at.col, "%s", what)`; the `"%s: %s"` form is not a place and
      stays.
    * **`lib/config/src/sources/entry.c`**: `dotenv_trim` goes for
      `cf_text_trim`; the two dotenv failures are
      `cf_text_fail(err, CF_E_SOURCE / CF_E_KEY, origin, line, 0, ...)`.
      The line counting there (a `++line` a line; no column) stays: no
      tracker is needed for a form with no column.
    * **CBOR and TLV** report byte offsets, not lines, and are not
      touched but for `tlv_key_valid`.
    * **A-4's and A-5's refusals** (`lib/config/src/bridge/file.c`'s
      recoding messages, and `cf_encoding_check` in
      `lib/text/src/encoding.c`): they call `cf_text_fail` when they
      give `file:line:col: text`, the place from `cf_text_place` cast to
      `unsigned int`. The wordings are kept as they are:
      `byte 0x%s has no character in %s`, `an unpaired UTF-16 surrogate
      0x%s` and `byte 0x%s is not valid UTF-8%s` (the hex is `0x%s`:
      `cf_fmt` has no width, so the digits are formed first).
      `lib/config/src/bridge/dir.c` has no placed message and is not
      touched.
  * **`tlv_key_valid` and `cf_key_valid`.** Their two bodies are the
    same, token for token (`tlv_key_valid` in tlv.c, `cf_key_valid` in keyspace.c). Which
    component: D-100's table lets `serialize` use `base`, and
    `cf_key_valid` needs only `CF_KEY_MAX` (cf_error.h, base),
    `cf_is_lower`, `cf_is_digit`, `cf_bool` and `size_t` (cf_port.h):
    nothing of config. So it moves to `lib/base/`:
    * `lib/base/src/key.c` (new) holds the body, unchanged (the
      comment about `CF_KEY_MAX` counting the terminator comes with it).
      Listed in `lib/base/CMakeLists.txt`, in both `SOURCES` and
      `SERIALIZE_SOURCES` of its `cf_component` call (A-1b;
      `CF_SERIALIZE_LEAVES` is derived from it, not edited), and in
      `amalgamate.py`'s `SOURCES`, before the serialize sources.
    * The declaration moves from `cf_keyspace.h` (config, which
      serialize may not include) to `cf_error.h` (base; already in
      `CF_SERIALIZE_HEADERS`), after `CF_ERROR_INIT`, under a comment
      "Keys" taken from `cf_keyspace.h`. `cf_keyspace.h` includes
      `cf_error.h`, so `#include <configforge/cf_keyspace.h>`
      still declares it, and no program changes; its "Keys" block
      keeps the prose and loses the one declaration. It is a public
      1.x function and stays, with its name, signature and
      `CF_API`, not experimental.
    * `lib/config/src/core/keyspace.c`'s definition goes; `cf_key_segments` and
      `cf_key_cmp` stay there.
    * `tlv.c`: `tlv_key_valid` goes, its two callers call
      `cf_key_valid`; the comment above it goes.
    * The symbol lists: `cf_key_valid` is now a function of both
      libraries. `cmake/configforge_serialize.symbols` gains it in its
      place in the alphabet (gate.abi `-DPART=serialize -DWRITE=ON`);
      `cmake/configforge.symbols` keeps it. `docs/API.md` is
      regenerated (gate.apiref: the declaration moved). abidiff sees an
      addition to the serialization library and nothing else.
    * `lib/config/tests/test_tlv.c` (`layout_key_rule`) compares
      the layout's judgement with `cf_key_valid`'s: now one function,
      so it can no longer disagree, and it stays as a test that a
      layout refuses what a key refuses (its comment changes to say
      so). `lib/serialize/tests/test_serialize_alone.c` gains
      `check(cf_key_valid("a.b", 3) && !cf_key_valid("A", 1), ...)`,
      which builds and links only if `cf_error.h` declares it and the
      serialization library has it.
  * **The proof of "no input read differently".**
    1. `lib/config/tests/test_reader_corpus.c` and its golden (A-7) pass
       unchanged after every reader is switched. Its alphabets hold every
       newline kind and comment start of the readers, and, with A-7's
       TAB in TOML's and YAML's first runs, every blank but four of
       JSON5's: JSON5's alphabets lack VT, FF, U+00A0 and U+FEFF, which
       `test_places` below covers with one explicit case each.
    2. `lib/text/tests/test_text_space.c` (new): `cf_text_break` for each
       rule on `\n`, `\r\n`, `\r`, `\r` before `a`, E2 80 A8, E2 80 A9,
       E2 80 AA, a cut E2 80, the empty input, and `p == end`;
       `cf_text_eol`; `cf_text_blanks` for each set on a string with
       every blank and a CR alone and before LF; `cf_text_trim` on
       `"  a b \r"`, `""`, `" \t\r"`; the tracker on a document with
       every break kind, its line and column after each, checked
       against **`cf_text_place`** (A-4) on every offset of a corpus
       when the rule is `CF_NL_LF` (they must agree); `cf_text_skip`
       for each syntax on: `/* a \n b */ x` (line 2 after, JSON5),
       `/* a \r b` (UNCLOSED at the end, line 2), `// a LS b` (stops at
       LS, JSON5), `# c LF`, `a #c` and `a#c` (YAML: only the first is
       a comment), TOML `# c\u0001` (CONTROL), TOML a comment with a
       cut UTF-8 sequence (UTF8), TOML `# c CR LF` (passes), and a
       JSON5 text with VT, FF, U+00A0, U+FEFF, U+2028 between
       tokens; `cf_text_fail` for the three forms and for a `%S` and a
       message cut at `CF_MESSAGE_MAX` (compared with `cf_error_set` of
       the old format on the same arguments).
    3. `lib/config/tests/test_places.c` (new): the places that a skip or a
       tracker can get wrong, through each codec's `decode`, as `file
       : line : col : message` strings checked against what the code
       **before** the card gives (written in step 2, which sees them
       pass on the old code): JSON `{"a":\n1 x}` and a CR-only text
       (no break, one line); JSON5 the same with CR as a break, `/* \n
       */ x`, a line continuation then a failure; TOML a CR LF
       document with a failure on line 3, a CR alone, a `"""` string
       with CR LF lines then a failure; YAML a CR LF document, a
       quoted scalar spanning lines then a failure; INI a CR LF file,
       a CR-only file; `.properties` a continuation, a form feed
       before a key, a CR LF file; and dotenv `expected NAME=value` on
       line 3 (dotenv has no codec: that case calls `cf_lex_dotenv`
       through an entry sink, so the file is `lib/config/tests/`'s). At
       least six cases a format.
  * **Fuzzing**: `lib/config/tests/test_fuzz.c`'s seeds gain one document a
    syntax with each break kind and a comment of each kind, so the new
    skip is fuzzed (A-5 added its seeds the same way).
  * **Placement of `test_places.c`.** Read against the tree after A-1b:
    `test_places.c` is in `lib/config/tests/` by A-1b's rule (one file;
    its dotenv case includes `configforge.h`); it is not split, and
    `test_text_space.c` is the one in `lib/text/tests/`.
  * **Blanks the corpus lacks.** JSON5's corpus alphabets lack VT, FF,
    U+00A0 and U+FEFF; `test_places` gets one explicit case for each of
    the four in JSON5. TAB for TOML and YAML comes from A-7's
    alphabets.
  * **`cf_text.h` includes `configforge/cf_error.h`** (for
    `cf_text_fail`'s `cf_error` and `cf_code`).
  * **Choices of the organiser.** `cf_text_track_init` sets line 1 (and
    `line_start` to `start`, so the first column is 1). `cf_text_skip`
    leaves `*bad` unwritten when it returns `CF_SKIP_OK`, and on
    `CF_SKIP_CONTROL` or `CF_SKIP_UTF8` leaves `*pp` at the byte
    refused (as `js_space` leaves `s->p`). The `cf_text_place`
    agreement test runs over the documents of A-7's reader corpus.
    `cf_text_fail`'s `CF_MESSAGE_MAX` buffer is the same size as
    `cf_error_set`'s own (`tmp` in lib/base/src/error.c), and the stack
    gate judges it.
* **files**: `lib/text/include/configforge/cf_text.h`,
  `lib/text/src/{space,track,fail}.c` (new), `lib/text/src/encoding.c`,
  `lib/text/CMakeLists.txt`, `scripts/generate/amalgamate.py`,
  `lib/serialize/src/{json,toml,ini,properties,yamlparse,yaml,xml,tlv}.c`,
  `lib/config/src/bridge/{schema,file}.c`,
  `lib/config/src/sources/entry.c`, `lib/base/src/key.c` (new),
  `lib/base/CMakeLists.txt`, `lib/base/include/configforge/cf_error.h`,
  `lib/config/include/configforge/cf_keyspace.h`,
  `lib/config/src/core/keyspace.c`, `lib/serialize/CMakeLists.txt`
  if a source list changes,
  `cmake/configforge.internal.symbols` (gate.abi with `-DWRITE=ON`) and
  `cmake/configforge_serialize.symbols` (`-DPART=serialize -DWRITE=ON`:
  `cf_key_valid` is the one promised name that changes),
  `docs/API.md` (regenerated); tests:
  `lib/text/tests/test_text_space.c`, `lib/config/tests/test_places.c` (new, in
  their components' test lists), `lib/config/tests/test_tlv.c`,
  `lib/serialize/tests/test_serialize_alone.c`, `lib/config/tests/test_fuzz.c`; last:
  `CHANGELOG.md`, `docs/Architecture.md`, `docs/Decisions.md`.
* **steps**:
  1. `ctest --preset default`, `test_reader_corpus`: both pass (A-7's
     golden is the baseline). Run the conformance suites once and keep
     the scores.
  2. `lib/config/tests/test_places.c` with its expected strings taken from
     the tree as it is; it passes. To see it fail, as the loop asks:
     change one function on purpose (`js_line` not counting a line),
     see the named case fail, restore it. Commit it with the card.
  3. `lib/text/tests/test_text_space.c` and the declarations with empty
     bodies; see it fail to link.
  4. `space.c`, `track.c`, `fail.c`; the test passes.
  5. One reader at a time, `ctest --preset default`,
     `test_reader_corpus` and `test_places` after each, the
     conformance suites after JSON, TOML and YAML: INI; `.properties`;
     JSON and JSON5; TOML; YAML (`yamlparse.c`, then `yaml.c`'s one
     function); XML; then `schema.c` and `entry.c`, then A-4's and A-5's
     refusals.
  6. `cf_key_valid`: first the test that fails (`test_serialize_alone`
     with the new check: it fails to build, because it includes only
     `cf_serialize.h` and `cf_key_valid` is not yet in `cf_error.h`);
     `key.c`, the header, the
     lists; `tlv.c`; `tlv`'s tests pass.
  7. The symbol lists, `docs/API.md`
     (`cmake -P scripts/generate/apiref.cmake`), the amalgamation, the
     documents; ACCEPT.
* **accept**: ACCEPT-LINUX with
  `-DEXTRA="conformance;fuzz;coverage;abi;yaml;components;shared"` (abi: the serialization
  library gains `cf_key_valid`; components: each component of `lib/`
  builds alone with the new sources and `key.c` in `base`; shared: the
  suite against a shared library, where the serialization library now
  exports `cf_key_valid`; yaml: `yaml.c`'s failure function is
  shared with the library backends), ACCEPT-WINDOWS, ACCEPT-CI. Conformance
  scores equal to step 1's and to `tests/conformance/floor.txt`. The
  stack check of `scripts/check/stack.cmake` is part of the full
  ACCEPT-LINUX run: `cf_text_fail`'s `CF_MESSAGE_MAX` buffer is on the
  failure path only.
* **stop if**: `cf_text_place` counts a break otherwise than
  `CF_NL_LF` (test 2 shows it): the difference is a bug in A-4's code or
  here; stop and report with the offset. `test_reader_corpus` or
  `test_places` differ after a reader is switched and the difference is
  not a typing mistake: the old behaviour is the right one; if a
  reader's behaviour cannot be a parameter of `cf_text_syntax` as
  written (it would need an eighth flag), stop and report with the
  document, do not add a flag. A conformance record moves: stop. A
  message differs by a byte: stop; the message is what the tests and
  users read.
* **docs**: D-102's entry gets "Built, in part: A-8" with the commit; with
  A-3 and A-7 the entry is complete but for the number reader (D-102
  also promises an integer and real reader in `lib/numeric/`, which no
  card builds yet): the card says so ("Built: A-3, A-7, A-8; the number
  reader has no card yet") for the maintainer's review of the
  index. CHANGELOG `### Changed`: "The readers share one count of lines
  and columns, one skip of blanks, comments and line breaks a syntax,
  one trim and one form of the failure message `file:line:col:
  message` (D-102); no input is read differently and no message
  changed." `### Added`: "`cf_key_valid` is in the serialization
  library too, where the TLV layouts' check used a copy of it (D-102)."
  `docs/Roadmap.md`: the item "Infrastructure behind abstractions" stays
  🟨, its line gains "A-8 done".
* **notes**:

---

## A-9 - Composed codecs, for what is almost a format

* **id**: A-9
* **goal**: `cf_codec_compose` fills a codec the program owns from a base
  codec, a name, and either or both of a repair of the bytes and a filter
  of the events; the codec reads nothing it is not named for, every value
  read through it is placed under its name in `explain()`, and the base
  reader is unchanged. `{{a:1, b:2}}` (a repair) and `{a:{2}, b:{2}}` (a
  filter) are read as `a = 1, b = 2` and `a = 2, b = 2`.
* **why**: D-103, level 3. A sender's own habit is read by composing
  codecs, not by loosening the built-in ones, and what was read this way
  says so wherever the value is explained.
* **needs**: A-5 (the UTF-8 check in `cf_codec_decode` and
  `cfi_codec_decode_raw`)
* **size**: medium
* **decided**:
  * **The description the caller owns**, in `cf_serialize.h` beside
    `cf_tlv_layout`, and the function that checks it and fills the codec,
    exactly:

    ```c
    /* A codec made of another (D-103); start from CF_COMPOSE_INIT. */
    typedef struct cf_compose {
        size_t          size;       /* sizeof(cf_compose)             */
        const char*     name;       /* "json5+braces"                 */
        const cf_codec* base;       /* reads what the repair leaves   */
        cf_bool (*repair)(void* ctx, const cf_blob* in, cf_blob* out, cf_error* err);
        void*           repair_ctx;
        cf_bool (*filter)(void* ctx, const cf_doc_sink* down, cf_doc_sink* up,
                          cf_error* err);
        void*           filter_ctx;
    } cf_compose;

    #define CF_COMPOSE_INIT                                              \
        {sizeof(cf_compose), NULL, NULL, NULL, NULL, NULL, NULL}

    CF_EXPERIMENTAL(1, 8) cf_bool cf_codec_compose_sized(cf_codec* out, size_t out_size,
                                                         const cf_compose* spec,
                                                         cf_error* err);
    #define cf_codec_compose(out, spec, err) \
        cf_codec_compose_sized((out), sizeof(cf_codec), (spec), (err))
    ```

    The program owns `out`, the `cf_compose` it points to, the base, the
    name and both contexts, and each outlives every configuration that
    reads with the codec, as for `cf_tlv_codec` (a `static` is the usual
    way). The library allocates nothing and copies nothing: `out->data`
    is `spec`. Read `cf_tlv_codec_sized` first (`lib/serialize/src/tlv.c`); this
    function is built the same way: refuse an `out_size` below `offsetof(cf_codec, data) +
    sizeof(const void*)`, `memset(out, 0, out_size)`, check the
    description, then fill.
  * **What the description must be**, checked whole; the error is
    `CF_E_DEFINITION` with the name as source, and `out` then reads
    nothing. In this order, the exact messages:
    1. `out_size` too small: `this cf_codec is %zu bytes, from before
       1.5, and has no room for a composition`;
    2. `spec` NULL, or its `size` below `offsetof(cf_compose, filter_ctx)
       + sizeof(void*)`: `composition '%s': no description, or one
       without its size (CF_COMPOSE_INIT)`;
    3. a name that is not 1 to 48 bytes, the first a lowercase letter,
       the rest lowercase letters, digits, `_`, `-` or `+`: `composition
       '%s': '%s' is not a codec's name: 1 to 48 of lowercase letters,
       digits, '_', '-' and '+', the first a letter`. A `.` is not
       allowed: it is the extension's;
    4. a name that is a built-in format's, by `cf_codec_by_name` or by
       `cf_format_option` (a format this build left out is still
       taken): `composition '%s': '%s' is the name of a built-in format;
       choose another`. Two readers answering to one name for one
       document is the disagreement D-103 exists to avoid;
    5. `base == out`: `composition '%s': the base is the codec being
       made` (before the next check: `out` was just cleared, so it
       cannot read, and the identity is the reason to give);
    6. `base` NULL, or `!cf_codec_can_read(base)`: `composition '%s': no
       base codec that reads`;
    7. `repair` and `filter` both NULL: `composition '%s': a repair or a
       filter is needed; with neither it is the base under another
       name`.
    The library is not asked about a base that is itself a composed
    codec: it is a codec that reads, and it nests. A name that is
    another composed codec's, or another codec of the program's own, is
    not refused by this card: `cf_codec_compose` knows the built-in names
    only. `cf_builder_codec` already refuses a second codec of one name
    in one builder (`this builder already has a codec called '%s'`), and
    that rule stands.
  * **What `out` is.** `out->size = min(out_size, sizeof *out)`;
    `name = spec->name`; `extensions = NULL`; `decode = NULL`;
    `encoder = NULL`; `decode_self = compose_decode` (static, in
    `compose.c`); `data = spec`. `caps = (base->caps & ~CF_CODEC_WRITE) |
    CF_CODEC_READ | CF_CODEC_NAMED_ONLY`, so the base's other caps are
    the composed codec's (typed, arrays, binary, schema-bound), it never
    writes (what a repair or filter changed cannot be written back) and
    it is never chosen from content: `cf_codec_detect` (`detect.c`) already
    skips a `CF_CODEC_NAMED_ONLY` codec of a builder's own, and a
    file's extension cannot select it because `extensions` is NULL. If
    `spec->repair != NULL` and `base->caps` has neither `CF_CODEC_BINARY`
    nor `CF_CODEC_SELF_ENCODED`, `caps` also gets
    `CF_CODEC_SELF_ENCODED`: see the next point.
  * **A-5's UTF-8 check applies to the repaired bytes, not the original.**
    `cf_codec_decode` (A-5) checks the input of every text codec that
    lacks `CF_CODEC_SELF_ENCODED` before it calls the codec, so a repair
    that turns Latin-1 into UTF-8 would never run on a Latin-1 file.
    The bit moves that check inside: with a repair, the composed codec
    carries `CF_CODEC_SELF_ENCODED` and `compose_decode` calls
    `cf_encoding_check(label, cur.p, cur.n, err)` (`label` below) on the
    bytes the repair returned, when the base is text, before the base
    reads them. A composed codec without a repair does not carry the
    bit: `cf_codec_decode` has already checked its input, which is also
    what the base reads. A binary base is not checked either way; a base
    with `CF_CODEC_SELF_ENCODED` (XML, or another composed codec with a
    repair) reads by its own rule. The bit's comment in `cf_serialize.h`
    (A-5 wrote it) gets one more sentence: "Also a composed codec with a
    repair, which checks the repaired bytes itself (cf_codec_compose)."
    A consequence, stated in the Guide: a `CF_ENCODING_*` in the source's
    flags is refused for such a codec by A-4's rule 1 (`declares its own
    encoding or has none`), and a UTF-16 mark is not looked at; the
    repair receives the original bytes and is the place to recode.
    For the same reason that refusal carries no encoding hint: after
    `cf_encoding_check` fails, `compose_decode` cuts `err->message` at
    the first `; it may be` (A-5's hint always starts so), leaving
    `<label>:<line>:<column>: byte 0x<XX> is not valid UTF-8`, since the
    flag the hint names would be refused for this codec.
  * **Where the repaired bytes live: the repair's own memory.** The
    repair is called as `repair(ctx, in, out, err)` with `out` set to
    `{NULL, 0, in->name}`. It sets `out->p` and `out->n` to bytes that
    stay valid until `compose_decode` returns: its own buffer, a
    `static`, or a range of `in` itself (`{{a:1}}` to `{a:1}` is
    `in->p + 1`, `in->n - 2`: no copy at all). It returns CF_TRUE, or
    CF_FALSE with `err` set. The library never frees, resizes or reads
    past what it was given. Rejected, each with its reason:
    * `cf_blob_arena` (D-61): it needs the arena of the key space the
      source is read into, which `decode_self` is not given and the
      repair's signature (D-103's) has no parameter for;
    * the sink's `reserve_work`: it exists only when the program's sink
      has one (a sink of 1.0's size has not), and again the repair has
      no handle on it;
    * an allocation by the library: the library allocates nothing here
      (D-103).
    The cost, written in the Guide: a repair that keeps a buffer in its
    context is used by one load at a time; give each configuration that
    may load at the same moment its own composed codec and context. A
    reload, a drop-in directory's files and the builder's own loads are
    one after another, so the usual use needs nothing.
    `compose_decode` runs `cf_error_clear(err)` before each callback,
    the repair and then the filter, so that "`err->code` is still
    `CF_OK`" below means what it says. If the repair returns CF_FALSE and
    `err->code` is still `CF_OK`, the library sets `CF_E_SOURCE`, `%s:
    the repair of '%s' refused the document` (the input's name, the
    codec's). If it returns CF_TRUE with `out->p`
    NULL and `out->n > 0`: `%s: the repair of '%s' gave %zu bytes and no
    memory`, `CF_E_SOURCE`. The BOM is the repair's business as well:
    it receives the bytes as the file has them, mark included.
  * **The filter wraps the sink the base writes into.** It is called once
    per read, after the repair, as `filter(ctx, down, up, err)`: `down`
    is the sink the caller of `cf_codec_decode` gave; `up` is a
    `cf_doc_sink` the library has zeroed and given `size =
    sizeof(cf_doc_sink)`, and the filter fills it. It must set
    `begin_table`, `begin_array`, `scalar` and `end` (and `ctx`: its own
    state, per read, kept in its own context as the repair's buffer is),
    and call `down`'s callbacks, with `down->ctx`, for what it passes
    on. It may rename, drop, merge or retype; it must copy a `name` or a
    text it keeps past the call (the reader's buffer is reused, as
    `cf_doc_sink` says for `reserve_text`). The root table's own `end` is
    the end of the document, which is when a filter that buffers flushes.
    The library does not call `up->end` itself. After the filter returns,
    the library checks the four callbacks and, if one is NULL:
    `%s: the filter of '%s' left a callback of its sink empty`,
    `CF_E_SOURCE`. A filter that returns CF_FALSE with `err->code` still
    `CF_OK` gets `%s: the filter of '%s' refused the document`.
  * **What the base writes into is not `up` but a sink `top` the library
    builds on its stack**, so that the optional storage callbacks reach
    the program's sink without the filter forwarding them (its `ctx`
    would be the filter's, not `down`'s). `top` has `size =
    sizeof(cf_doc_sink)`; its `begin_table`, `begin_array`, `scalar` and
    `end` are four static functions that call `up`'s with `up.ctx`; its
    `ctx` is a frame `{down, up}` on the same stack. `top.reserve_text`
    and `top.reserve_work` are set only when there is somewhere to go:
    `up`'s own when the filter set one (and `up.size` reaches it), else
    `down`'s when `down->size` reaches that member
    (`offsetof(cf_doc_sink, reserve_text) + sizeof(...)`, as `js_text`
    in `json.c` does) and it is not NULL; else NULL, which the base
    reads as "no storage" as it does today. Without a filter `top` is
    not built: the base writes into `down` as it is.
  * **Provenance: the name is in every place.** A value's origin is the
    `file` of the `cf_loc` its reader gives, which the flattener writes
    into the candidate (`fl_scalar` in `lib/config/src/bridge/flatten.c`,
    which formats it with `cf_loc_text`), which `explain()` prints
    (`explain_entry` in `lib/config/src/diag/explain.c`, where it prints
    `c->at`). `compose_decode` hands the base a blob whose `name`
    is the label `<input name> (<codec name>)`, made in a
    `char label[CF_SOURCE_MAX]` on its own stack with `cf_fmt`: an error's
    `source` holds `CF_SOURCE_MAX - 1` bytes at most (`cf_error.h`, 64),
    so the label is cut there, and a buffer of that size also keeps the
    frame inside `CF_STACK_BUDGET` in `minimal`. `%S` takes the input
    name cut to what the codec's name leaves (`CF_SOURCE_MAX - 1 - 3 -
    strlen(codec name)` bytes: 12 for a name of 48, more for a shorter
    one), so the codec's name always survives whole,
    so `explain("a")` says `x.json (json5+braces):1:2` and every error
    the base raises is `x.json (json5+braces):1:3: ...`. The label is
    valid for the whole decode and the flattener formats it at once
    (as it does every `cf_loc.file`). The source's format is already the
    codec's name (`file_state.used->name`, `cf_source_file_format`, the
    report's `"format"` member, written by `report_format` in `report.c`,
    with `"format_how": "named"`), so the report needs no change. Places are
    in the repaired text: a repair that moves bytes moves them; the Guide
    says so, and that a repair that keeps lengths (a blank for a brace)
    keeps the places of the file.
  * **After a refusal, the hint of "the content looks like".**
    `file_hint` (`lib/config/src/bridge/file.c`) adds that
    sentence when a format the name chose could not read the bytes. For a
    composed codec it would say what the repair is there to hide. It
    returns at once when `tried->caps` has `CF_CODEC_NAMED_ONLY`
    (a TLV layout's codec gets the same treatment, which it should have).
    The drop-in directory has no such hint.
  * **The option.** `CF_WITH_COMPOSE`, default ON, declared as
    `CF_WITH_TLV` is (the `option(CF_WITH_TLV ...)` line of
    `cmake/ConfigForgeOptions.cmake`, and `COMPOSE` in the `CF_FEATURES`
    list there, after `TLV`; the `#define CF_WITH_TLV` line of
    `cmake/templates/cf_build_options.h.in`, beside it a line
    `#define CF_WITH_COMPOSE @CF_OPT_COMPOSE@`; the `#ifndef CF_WITH_TLV`
    block of `lib/port/include/configforge/cf_build_options.h`, a
    `#ifndef CF_WITH_COMPOSE` with a default of 1); wherever `git grep -n
    CF_WITH_TLV` finds the option named in the build (not the places that
    are about a layout), `CF_WITH_COMPOSE` is named beside it; in the
    README the option joins the prose `Options:` paragraph.
    "compose" is also a feature name: the `cf_features` list in
    `lib/config/src/core/version.c` (`cf_has_feature("compose")`) gains
    it under `#if CF_WITH_COMPOSE`, beside the `#if CF_WITH_TLV` entry,
    and `test_listed` in `lib/config/tests/test_features.c` a line beside
    the `"tlv"` one. The `minimal` configure preset in `CMakePresets.json`
    sets `CF_WITH_COMPOSE` OFF,
    which is how ACCEPT-LINUX builds the option off; A-11's `misra`
    profile sets it OFF too (D-103). With it off,
    `cf_codec_compose_sized` is still defined and returns false:
    `CF_E_CAPABILITY`, source the name when there is one, `'%s' is a
    composition, and this build was compiled without CF_WITH_COMPOSE`
    (without a name: `a composition, and this build was compiled
    without CF_WITH_COMPOSE`). All of `compose.c` is inside `#if
    CF_WITH_COMPOSE`, with the stub under `#else`, as `tlv.c` is.
  * **Bindings.** C only in this card. A repair and a filter are
    callbacks over the document events; C++, Rust and Python take them
    when the document events are a public API (D-103's neighbours, the
    2.0.0 milestone). Said in the Guide.
  * **Not decided here, and not needed:** a composed codec's own
    extensions (it has none: named only), writing back, and a filter
    that sees locations (it receives `at` as the base gave it).
* **files**: `lib/serialize/src/compose.c` (new),
  `lib/serialize/CMakeLists.txt`,
  `lib/serialize/include/configforge/cf_serialize.h` (the struct, the
  macro, the function, the `CF_CODEC_SELF_ENCODED` comment),
  `lib/config/src/bridge/file.c` (`file_hint`),
  `cmake/ConfigForgeOptions.cmake` (the option and `CF_FEATURES`),
  `cmake/templates/cf_build_options.h.in`, `CMakePresets.json`
  (`minimal`), `lib/config/src/core/version.c` (the feature name),
  `lib/config/tests/test_features.c`,
  `lib/port/include/configforge/cf_build_options.h`, the other files
  `git grep -n CF_WITH_TLV` names for the build, `README.md`'s
  `Options:` paragraph, `scripts/generate/amalgamate.py` (`SOURCES`),
  the symbol lists (gate.abi with `-DWRITE=ON`: `cf_codec_compose_sized
  1.8` in the experimental list); tests: `lib/config/tests/test_compose.c`
  (new: it includes `configforge.h`, so by A-1b's rule it belongs to
  `config`) and its entry in the `CF_UNIT_TESTS` list of
  `lib/config/tests/CMakeLists.txt`, ctest name `unit.test_compose` as
  `test_tlv`'s is named; last:
  `docs/Guide.md`, `docs/Decisions.md`, `docs/Plan.md` (the index),
  `docs/API.md` (regenerated), `CHANGELOG.md`.
  `compose.c` includes `configforge/cf_serialize.h` and
  `configforge/cf_encoding.h` only (both are in
  `CF_SERIALIZE_HEADERS`); it calls `cfi_codec_decode_raw` (A-5: a plain
  `cfi_` function, not `CF_API`, not fenced, in no symbol list) for the
  base, which is how it avoids a second
  UTF-8 check and a second look at the same bytes.
* **steps**:
  1. `test_compose.c`, as `test_tlv.c` is made (its fresh error, its
     `check`, `check_text`, `has`), all of it under `#if
     CF_WITH_COMPOSE && CF_WITH_JSON` (json5 is the base of most cases),
     with, under `#if !CF_WITH_COMPOSE`, the stub's refusal case
     (`CF_E_CAPABILITY`, naming `CF_WITH_COMPOSE`):
     * the base for the filter test, written in the test: a codec
       `braces` (`caps = CF_CODEC_READ | CF_CODEC_TYPED`, `decode`
       only) that reads `{name:{digits}, name:{digits}}`: the root
       `begin_table(NULL, 0)`, then for each member `begin_table(name)`,
       `scalar(NULL, 0, <int>, CF_FALSE, &loc)` (a scalar with no name
       inside a table: what `{2}` is), `end`, then the root `end`;
       letters only in a name, whitespace skipped, anything else
       `CF_E_SOURCE` `braces:<offset>: expected ...` (the exact words
       are the test's);
     * a repair `braces_repair`: skips white space at both ends, and
       when the document starts `{{` and ends `}}` returns
       `in->p + start + 1` and `in->n - start - end - 2` (no copy), else
       the document unchanged; and a repair `latin1_repair` with a
       `static char buf[256]` that turns each byte 0x80 to 0xFF into the
       two UTF-8 bytes of that code point and refuses a document that
       would not fit with `latin1: too long`; and a repair
       `broken_repair` returning CF_FALSE and leaving `err` alone, one
       returning CF_TRUE with `out->p == NULL` and `out->n == 3`;
     * a filter `unwrap`, ctx `{down, depth, pending, name[32]}`:
       `begin_table` with `depth == 0` forwards; deeper, it keeps the
       name (copied) in `pending = 1` and forwards nothing;
       `scalar` with `pending == 1` and no name forwards `down->scalar`
       with the kept name and sets `pending = 2`; any other event with
       `pending == 1` first forwards `begin_table(name)` and clears it;
       `end` lowers `depth`, drops the `end` of a table with
       `pending == 2`, and forwards an empty table's pair when
       `pending == 1`. A second filter `lazy` that sets only three of the
       four callbacks, and a third, `pass`, that forwards every event
       unchanged;
     * the cases, each read with `cf_builder_document(b, "x.json",
       CF_RANK_USER_FILE, doc, n, "<name>", 0)` after
       `cf_builder_codec(b, &composed)`, as `read_doc` in `test_tlv.c`
       does:
       a. `json5` + `braces_repair`, named `json5+braces`: the document
          `{{a:1, b:2}}` reads `a = 1`, `b = 2` (`cf_get_int`); the same
          document read by the built-in `json5` is refused (the
          composition is what makes it read); `explain("a")` has
          `(json5+braces):1:`; `cf_source_file_format` says
          `json5+braces`, `how` `CF_DETECT_NAMED`; the report has
          `"format": "json5+braces"`; a document that is plain JSON5
          (`{a:1}`) reads too, through the repair that leaves it;
       b. `braces` + `unwrap`, named `braces+unwrap`: `{a:{2}, b:{2}}`
          reads `a = 2`, `b = 2`; `{a:{}}` (an empty table, no scalar)
          reads as the table it is, the case the pending name covers;
       c. both in one codec, `json5` base, repair `braces_repair`, and a
          filter that renames every key to its own with a `-x` suffix
          (a second small filter in the test): `{{a:1}}` reads `a-x =
          1`: the filter sees what the repaired bytes made;
       d. UTF-8: `latin1_repair` over `json5`, named `json5+latin1`,
          with `{a:"caf\xE9"}` reads `a` as `caf` and the two bytes
          `C3 A9`; the same document through a composed codec whose
          repair leaves it alone (`braces_repair` over it) is refused
          with `byte 0xE9 is not valid UTF-8` and `json5+braces` in the
          message, the place counted in the repaired text, and no
          `; it may be` hint; a composed
          codec without a repair (`pass` alone) over
          that document is refused by `cf_codec_decode`'s own check
          with the same words but for the hint: the `; it may be ...`
          part, which `compose_decode` cuts on the repaired path, stays
          on this one, since the flag it names is allowed for a codec
          without a repair;
       e. caps: `json5+braces` has `CF_CODEC_READ`,
          `CF_CODEC_NAMED_ONLY`, `CF_CODEC_SELF_ENCODED` (it has a
          repair), the base's `CF_CODEC_TYPED` and `CF_CODEC_ARRAYS`
          (json5 has both) and no `CF_CODEC_WRITE`;
          `braces+unwrap` has no `CF_CODEC_SELF_ENCODED`;
          `cf_codec_kind` of `json5+braces` reads "text,
          self-describing, typed, arrays, read", and of `braces+unwrap`
          (its test base has no arrays) "text, self-describing, typed,
          no arrays, read";
       f. named only: `cf_codec_detect` with `{&composed}` as `own` over
          `{a:1}` and no name does not return it; a file `x.json5`
          is read by the built-in codec, not by it; an
          `CF_ENCODING_*` in the flags of a source reading `json5+braces`
          is refused (A-4's wording);
       g. refusals of `cf_codec_compose`, one each, `has()` on the
          message and `err->code == CF_E_DEFINITION`: a 12-byte `cf_codec`
          (`out_size` below the minimum), a description without its
          size, names `Json`, `a.b`, `` (empty), 49 letters, `json5`,
          `toml` (built-in) and `yaml` (a built-in, compiled or not),
          no base, a base that cannot read (an `encoder`-only codec), the
          base being `out`, and neither repair nor filter;
       h. runtime refusals: `broken_repair` gives the "refused the
          document" text and `CF_E_SOURCE`; the NULL-memory one its
          text; `lazy` its "left a callback of its sink empty" text; an
          error the base raises (`{{a:` read through `json5+braces`) says
          `x.json (json5+braces):1:` first; none of these messages holds
          "the content looks like";
       i. a sink of 1.0's size (`size = offsetof(cf_doc_sink,
          reserve_text)`, the cases `json.c` meets) behind a filter: a
          string over 255 bytes is refused as `a long string needs sink
          text storage`, not read past the sink's size; a sink with
          `reserve_text` behind a filter that sets none reads it;
       j. a composed codec over a composed codec (`json5+braces` as the
          base of `json5+braces+x`) reads `{{a:1}}`, and its label has
          both names; a source named by 100 bytes, refused by the base
          (`{{a:`), gives an `err->source` of `CF_SOURCE_MAX - 1` bytes
          or fewer that ends in `(json5+braces)`.
     Add `test_compose` to `CF_UNIT_TESTS` in
     `lib/config/tests/CMakeLists.txt`, beside `test_tlv`. See it fail to
     build: `CF_WITH_COMPOSE` is not defined yet, so the stub's case is
     the one compiled, and it names `cf_codec_compose`, which does not
     exist.
  2. The code. `cf_serialize.h`: the struct, `CF_COMPOSE_INIT`, the
     declaration, the `CF_CODEC_SELF_ENCODED` comment. `compose.c` (and
     its line in `lib/serialize/CMakeLists.txt`, beside `src/tlv.c`): the
     name check, `cf_codec_compose_sized`, `compose_decode` with its
     label, the call of the repair and of the filter, the `top` sink's
     four trampolines and two storage forwarders, the stub for `!
     CF_WITH_COMPOSE`. The option as `CF_WITH_TLV` is declared, with the
     feature name and the `minimal` preset's line. `file_hint`'s early
     return. `amalgamate.py` and the symbol lists (`-DWRITE=ON`), which
     the suite's gates read. All of cases a to j pass. Build once with
     `-DCF_WITH_COMPOSE=OFF` and see `test_compose` reduce to the stub's
     refusal and everything else pass.
  3. The documents, then ACCEPT: the README's `Options:` paragraph, the
     other places `git grep -n CF_WITH_TLV` names for the build,
     `docs/Guide.md`, `docs/Decisions.md`, `docs/Roadmap.md`,
     `CHANGELOG.md`, `docs/Plan.md` (the index), `docs/API.md`
     (regenerated).
* **accept**: ACCEPT-LINUX with
  `-DEXTRA="abi;coverage;shared;components;compilers"` (the
  experimental list, the coverage floor with the new file, the amalgamated
  file, the shared library, `compose.c` in `lib/serialize` built alone,
  and under musl, tcc and pcc), ACCEPT-WINDOWS, ACCEPT-CI. The
  `minimal` preset of ACCEPT-LINUX builds the option off (the card adds
  that line to the preset), so the stub is built and tested there.
* **stop if**: `cfi_codec_decode_raw` or `CF_CODEC_SELF_ENCODED` is not as
  A-5 describes: follow what is there and say so in the notes. The
  flattener keeps a `cf_loc.file` past the scalar call (a location text
  that is not copied at once): the label on the stack would dangle;
  stop and report, do not make the label static. `gate.layering` fails
  on `compose.c`: it includes something A-5's header list does not allow;
  do not widen the table.
* **docs**: D-103's entry (written by A-1) gets "Built: A-9" with the
  commit. `docs/Roadmap.md`: the item "Infrastructure behind
  abstractions" stays 🟨, its line gains "A-9 done". The Guide, in section 8 after "A TLV layout": "### Reading what is
  almost a format", in this order: (1) the four levels in four lines
  (a standard's dialect, JSON5 and JSONC; a whole format of your own;
  a composition; recovery inside a lexer, refused) and why the third is
  never chosen from content and why no composed codec exists until the
  program makes one (the option is on by default; the `misra` profile
  turns it off); (2) the `{{a:1, b:2}}`
  example, the repair and the call, about twelve lines, the test's;
  (3) the `{a:{2}, b:{2}}` example, the filter, the same; (4) what the
  repair is given and gives (the bytes with their mark, memory of its
  own, the one-load-at-a-time rule), what the filter must set and copy,
  where the root's `end` is; (5) what `explain()` and the report say
  (the name, in the place; places in the repaired text; the
  length-keeping repair); (6) the UTF-8 rule and the flags rule above;
  (7) the build option and that it is C only for now. The "Formats"
  table in section 8 gets no row (a composition is the program's).
  CHANGELOG `### Added`: "`cf_codec_compose`: a codec made from a base
  codec, a repair of the bytes and a filter of the events, for a
  document that is almost a format (`{{a:1}}`, `{a:{2}}`); it is chosen
  by its name only, and every value read through it is placed under that
  name in `explain()` (D-103). The build option `CF_WITH_COMPOSE`, on by
  default." And `### Changed`: "A format chosen by name that is
  `CF_CODEC_NAMED_ONLY` (a TLV layout's codec, a composed codec) no
  longer adds 'the content looks like ...' to its refusal."
* **notes**:

---

## A-10 - A refusal's place and reason

* **id**: A-10
* **goal**: `cf_error` carries a `cf_loc where` and an `int reason`;
  every reader, the dotenv lexer and the flattener that refuse a
  document set both, from one list `cf_reason.def`, so a program and a
  test can ask "was it refused for invalid UTF-8, too deep, a number out
  of range" and get the same answer from every format; every message is
  what it was, byte for byte.
* **why**: D-107, and D-108 (A-5's refusal is `CF_REASON_UTF8`, A-4's is
  `CF_REASON_ENCODING`). The place and the reason are in the message
  today and nowhere a program can test them.
* **needs**: A-8 (one line-and-column tracker and one failure formatter
  in the readers; the card's step 1 reads its notes for the formatter's
  name), A-9 (`compose.c`: its four runtime messages and its definition
  refusals get a reason here, as the table's A-9 row says)
* **size**: medium
* **decided**:
  * **`cf_loc` moves to `cf_error.h`.** `cf_error.h` is in `lib/base` and
    may not include `cf_serialize.h` (`gate.layering`, D-100's table).
    The typedef (`file`, `line`, `col`; today
    `lib/serialize/include/configforge/cf_serialize.h`) moves, unchanged, to
    `lib/base/include/configforge/cf_error.h` above `cf_error`;
    `cf_serialize.h` includes `cf_error.h` already, so every program
    still sees it. `cf_loc_text` stays in `cf_serialize.h`.
  * **The members**, appended after `message` (D-51: they are touched
    only where `size` reaches them):

    ```c
    typedef struct cf_error {
        size_t size;
        int    code;
        char   key[CF_KEY_MAX];
        char   source[CF_SOURCE_MAX];
        char   message[CF_MESSAGE_MAX];
        cf_loc where;  /* (1.8) line and column of what was refused, 0 when
                          there is none; where.file is always NULL: an error
                          holds no pointer, `source` is the file */
        int    reason; /* (1.8) CF_REASON_*, CF_REASON_NONE when unset */
    } cf_error;
    ```

    `CF_ERROR_INIT` becomes `{sizeof(cf_error), 0, {0}, {0}, {0},
    {NULL, 0, 0}, 0}` and stays inside its `clang-format off` fence.
    `CF_ERROR_SIZE_1_7` is not added: the library keeps the offset where
    1.7 ended as `offsetof(cf_error, where)` and the offset where 1.8
    ends as `offsetof(cf_error, reason) + sizeof(int)`, in `error.c`, as
    `codec.c` keeps `CODEC_SIZE_1_0`. D-107 says "line, column, byte
    offset"; `cf_loc` has no offset and growing `cf_loc` is a change to a
    type programs fill, so `where` holds line and column only. A reader
    of bytes (CBOR, TLV) has neither: `where` is `{NULL, 0, 0}` and the
    byte offset stays in the message, as today. A reader with a line and
    no column (INI, `.properties`, dotenv) sets `col` 0. (Reported, not
    left as a question: if an offset is wanted later it is a new member
    after `reason`, another card.)
  * **Three changes in `lib/base/src/error.c`**, all by the size rule:
    * `cf_error_clear` also zeroes `where` and `reason` where `e->size`
      reaches `offsetof(cf_error, reason) + sizeof(int)`;
    * `cf_error_set` zeroes them at the same condition, before it
      writes the message, so a stale reason never outlives its message;
    * `cf_error_copy` copies them where `dst->size` reaches them, and
      from `src` only where `src->size` does: a smaller `src` leaves
      `dst`'s at zero.
    A `cf_error` made without `CF_ERROR_INIT` (size 0) is not touched
    past 1.0's fields, as before.
  * **The list, `lib/base/include/configforge/cf_reason.def`**, beside
    `cf_code.def` and written as it is written (`CF_REASON_X(symbol,
    name, description)`, the last with `CF_REASON_LAST_X` so the enum has
    no trailing comma, the same default block at the top and the
    `#undef` at the bottom). The enum is `cf_reason` in `cf_error.h`,
    values from 0 in this order:

    | Symbol | Name | What was refused |
    | ------ | ---- | ---------------- |
    | `CF_REASON_NONE` | `none` | no reason: the refusal is not of a document's bytes (memory, a layout, a file that cannot be opened) |
    | `CF_REASON_SYNTAX` | `syntax` | the bytes break the format's grammar in a way no other reason names |
    | `CF_REASON_UTF8` | `utf-8` | bytes that are not UTF-8 (A-5) |
    | `CF_REASON_ENCODING` | `encoding` | a byte with no character in the encoding named, a UTF-16 error, a UTF-32 mark, an encoding named for a codec that has its own (A-4) |
    | `CF_REASON_TRUNCATED` | `truncated` | the input ends inside a string, comment, collection, tag, length or header |
    | `CF_REASON_TRAILING` | `trailing` | text or bytes after the document, or a second document |
    | `CF_REASON_EMPTY` | `empty` | no document where one is required |
    | `CF_REASON_TOP_LEVEL` | `top-level` | the document is not a table at its top |
    | `CF_REASON_DEPTH` | `depth` | nesting deeper than `CF_MAX_DEPTH` |
    | `CF_REASON_TOO_LONG` | `too-long` | a key, string, name or length over a limit of the library |
    | `CF_REASON_BUDGET` | `budget` | a document that makes more than its size allows (D-82), or an XML document too large to read |
    | `CF_REASON_ESCAPE` | `escape` | an escape sequence malformed, unknown or naming no character |
    | `CF_REASON_CONTROL` | `control-character` | a control character where the format does not allow one |
    | `CF_REASON_NUMBER` | `number` | a malformed number, date or time |
    | `CF_REASON_NUMBER_RANGE` | `number-range` | a number the library's types cannot hold |
    | `CF_REASON_DUPLICATE_KEY` | `duplicate-key` | a key, table or attribute defined twice where the format forbids it |
    | `CF_REASON_KEY` | `key` | a name that cannot be a key (`cf_key_valid`), an empty key, a map key that is not text |
    | `CF_REASON_STRUCTURE` | `structure` | well-formed, but a shape the key space cannot hold: a value used as a table, text beside child elements, list elements apart |
    | `CF_REASON_UNSUPPORTED` | `unsupported` | well-formed, but a construct this library does not read: a byte string, a CBOR tag, a DOCTYPE, a YAML tag, a layout's record, a response file in a response file |
    | `CF_REASON_STORAGE` | `storage` | the consumer or its workspace refused: the sink, no sink text storage, "the document could not be recorded" |

    A new reason is added at the end, never renumbered; a program treats
    a value it does not know as `CF_REASON_SYNTAX`. Twenty entries.
    The code stays what it was at every site (D-107: coarse).
  * **Three public functions**, in `cf_error.h`, each
    `CF_EXPERIMENTAL(1, 8)` and in the experimental symbol list:

    ```c
    /* The name ("number-range") and description of a reason; "?" and
       "unknown" for a value above CF_REASON_STORAGE. */
    CF_EXPERIMENTAL(1, 8) const char* cf_reason_name(int reason);
    CF_EXPERIMENTAL(1, 8) const char* cf_reason_description(int reason);

    /* Sets `where` (file NULL, line, col) and `reason` on an error whose
       message is already set, where e->size reaches them. NULL is
       ignored. A program's own codec uses it as the built-in ones do. */
    CF_EXPERIMENTAL(1, 8) void cf_error_place(cf_error* e, int reason,
                                              unsigned int line, unsigned int col);
    ```

    A reader's failure is `cf_error_set(...)` and then `cf_error_place(
    err, reason, line, col)`; the library does not add a variadic
    `cf_error_set_at`. `cf_reason_name`, `cf_reason_description` and
    `cf_error_place` are defined in `lib/base/src/error.c`, beside
    `cf_error_set`, and the two names are read from `cf_reason.def` there
    as `cf_code_name`'s are from `cf_code.def`.
  * **How each reader gets the reason: the failure formatter takes it.**
    Every reader refuses through a small function of its own that
    formats `file:line:col: what` (`js_fail`, `toml_fail`,
    `toml_fail_named`, `prop_fail`, `yp_fail_at`, `yaml_fail_at`,
    `xml_fail`, `cbor_fail`, `tlv_fail`). After A-8 the text-form ones
    are thin wrappers over the shared `cf_text_fail`. Decided:
    `cf_text_fail` (A-8, `lib/text/src/fail.c`, declared in `cf_text.h`)
    gains an `int reason` right after `col`, and calls `cf_error_place`
    after `cf_error_set` with the line and column it formats; every
    wrapper gains the same parameter and passes it through
    (`js_fail(s, at, CF_REASON_TRUNCATED, "string is not closed")`), and
    so does every other caller of `cf_text_fail`: A-4's and A-5's
    refusals, `schema.c` (`CF_REASON_NONE`: a definition) and `entry.c`'s
    dotenv failures. A-8 moves `xml_fail` and the expat parse error onto
    `cf_text_fail` too, so XML takes the parameter like the rest. The
    wrappers A-8 did not move (CBOR and TLV: they format a byte offset,
    not a line) call `cf_error_place` themselves.
    Step 1 writes the wrappers' names in the notes. Where a call site
    builds its message by hand with `cf_error_set` (INI, which has no
    wrapper; `.properties`' empty key and invalid key; the sinks' "sink
    refused"; CBOR, TLV and TOML's no-room and "document could not be
    recorded" cases),
    `cf_error_place` follows it. The messages are not touched: the
    words, the format string and the code of every call stay as they
    are.
  * **Which reader sets which reason at which call**, the table the
    steps follow. A call is named by its message text, the words after
    the place, as the code has them: line numbers move under A-3, A-5,
    A-7 and A-8, which rewrite these files first. Where a call chooses
    between two texts (`malformed number` or `expected a value`), each
    text has its own reason. In a reader's row, every other call whose
    code is `CF_E_SOURCE` or `CF_E_KEY` is `SYNTAX`; a call with
    `CF_E_MEMORY` (no room), `CF_E_DEFINITION`, `CF_E_CAPABILITY` or a
    file that cannot be read is `NONE`:

    | Reader | Reason: the calls |
    | ------ | ----------------- |
    | `json.c` (JSON and JSON5) | `TRUNCATED`: "a comment is not closed", "escape at end of input", "string is not closed", and the two `js_between` words "object is not closed" and "array is not closed". `TOO_LONG`: "string is too long", "member name is too long". `ESCAPE`: the two split messages ("u needs four hexadecimal digits" and "x needs two hexadecimal digits", each after a backslash), "high surrogate without a low one", "unpaired low surrogate", "\0 may not be followed by a digit", "a digit may not be escaped", "unknown escape", "an escape in a name is ...", "the escape is not a character a name may hold". `CONTROL`: "a string may not contain a raw newline", "a string may not contain a raw control character". `NUMBER`: "malformed number" in every form ("malformed number: a leading zero", "...: no digit after the point", "...: no digit in the exponent", "...: no hexadecimal digit"). `UNSUPPORTED`: "this build has no floating point support". `DEPTH`: "nesting is too deep". `KEY`: "key is not valid". `STORAGE`: "sink refused", "a long string needs sink text storage". `EMPTY`: "the document is empty". `TOP_LEVEL`: "a configuration document must be an object". `TRAILING`: "trailing text after the document". `SYNTAX`: every other call ("expected a string", "expected a member name", "expected ',' or '}'" and its `]` form, "trailing comma", "expected ':'", "expected a value") |
    | `toml.c` | `CONTROL`: "a comment may not contain a control character", "a string may not contain a control character". `UTF8`: "a comment is not valid UTF-8", "a string is not valid UTF-8" (A-5's check makes these unreachable for text; they stay and say `UTF8` for the day a caller reaches the reader alone). `TOO_LONG`: "the string is too long", "the key is too long", "the key has too many parts". `TRUNCATED`: "the string is not closed on its line", "escape at the end of the input", "the string is not closed", "the array is not closed", "the inline table is not closed". `ESCAPE`: "unknown escape", the `TOML_UNICODE_DIGITS` message, "not a Unicode scalar value". `DUPLICATE_KEY`: "is already defined; TOML defines a key once", "is already defined, and not as an array of tables", and, from the two `why` switches, "is a table defined twice; TOML defines a table once" and "was defined by dotted keys; a [header] may not define it again". `STRUCTURE`: every other `why` ("is a value, not a table", "is an inline table, which is complete as written", "is an array of tables; ...", "is a table a [header] defines; ...") and "an array of tables reopened after another table". `NUMBER`: "not a date: ...", "not a date-time: ...", "not a time: ...", "malformed date or time", "malformed date, time or number", "malformed number" in every form, "malformed integer", "a hexadecimal, octal or binary integer has no sign". `NUMBER_RANGE`: "the integer does not fit in 64 bits", "a hexadecimal, octal or binary integer is unsigned and ...". `UNSUPPORTED`: "this build has no floating point support". `KEY`: "not a valid key". `STORAGE`: "the document could not be recorded", "a long string needs sink text storage", "sink refused". `DEPTH`: "nesting is too deep", "tables are nested too deeply". `SYNTAX`: every other call ("expected a value", "expected '=' after the key", "expected ',' or ']'", ...) |
    | `yaml.c` (events) | `DEPTH`: "nesting is too deep". `TOP_LEVEL`: "a configuration is a mapping at the top, not a sequence" and "... not a single value". `KEY`: "a key has to be a scalar: ...", "not a valid key", "an empty key". `TOO_LONG`: "the key is too long". `UNSUPPORTED`: "the tag %s is not one this library resolves: ..." (a tag that is not the core schema's). `STORAGE`: "an anchor needs reader workspace, ...", "the document could not be recorded". `STRUCTURE`: "an alias inside the node its anchor names". `BUDGET`: "aliases expand to far more than the document holds". `TRAILING`: "a second document: one file is one configuration". `SYNTAX`: every other `CF_E_SOURCE` call, and the libyaml and libfyaml failures (`yaml_fy_fail` and libyaml's), which say no more than the library does |
    | `yamlparse.c` (the built-in parser) | `ESCAPE`: "an escape in a tag is two hexadecimal digits", "an unknown escape", "an escape needs its hexadecimal digits", "an escape names no character". `TOO_LONG`: "the tag is too long". `BUDGET`: "a value is longer than the document". `CONTROL`: "a value holds a character YAML does not print". `TRUNCATED`: "a quoted value is not closed", "a flow mapping is not closed" (and the other "is not closed" calls), "directives without a document after them". `DEPTH`: "nesting is too deep". `SYNTAX`: every other `yp_fail` and `yp_fail_at` call. These reasons exist only with the built-in parser; a build with libyaml or libfyaml gives `SYNTAX` for them (see the test) |
    | `ini.c` | `TRUNCATED`: "section header is not closed". `KEY`: "empty section segment", "'%S' is not a valid key segment", "empty key", "'%S' is not a valid key". `TOO_LONG`: "'%S' is too long". `STRUCTURE`: "'%S[]' again after line %u; an array's lines go together, ...". `STORAGE`: "the document could not be recorded" (with or without a line), "sink refused", "'%S' could not be recorded". `SYNTAX`: every other call (those that name no more than the line) |
    | `properties.c` | `ESCAPE`: "u needs four hexadecimal digits" and "u escape is half a surrogate pair", each split in the source after a backslash, so the words after it are these. `TOO_LONG`: "the key is too long", "the value is too long". `KEY`: "empty key" and "'%S' is not a valid key" (code `CF_E_KEY` for the second). `STORAGE`: "sink refused" |
    | `xml.c` | `DEPTH`: "nesting is too deep". `STRUCTURE`: "an attribute and a child element have the same name; ...", "elements of one name are a list, and a list's elements must be ...", "text beside child elements: ...", "the root element holds settings, not a value". `UNSUPPORTED`: "namespaces are not in the XML this library reads: ...", "a DOCTYPE is not read: ...". `KEY`: "not a valid key" and every `xml_sink_failed` call. `TOO_LONG`: "the key is too long". `BUDGET`: "too large for this XML reader". At the call that reports `XML_ErrorString` (the parse error), by `XML_GetErrorCode`: `XML_ERROR_NO_ELEMENTS` is `EMPTY` when `in->n == 0` and else `TRUNCATED`; `XML_ERROR_UNCLOSED_TOKEN`, `XML_ERROR_PARTIAL_CHAR`, `XML_ERROR_UNCLOSED_CDATA_SECTION` `TRUNCATED`; `XML_ERROR_JUNK_AFTER_DOC_ELEMENT` `TRAILING`; `XML_ERROR_DUPLICATE_ATTRIBUTE` `DUPLICATE_KEY`; `XML_ERROR_UNKNOWN_ENCODING`, `XML_ERROR_INCORRECT_ENCODING` `ENCODING`; `XML_ERROR_AMPLIFICATION_LIMIT_BREACH` `BUDGET` (the tree does not name it: the card adds that branch, inside `#ifdef XML_ERROR_AMPLIFICATION_LIMIT_BREACH`, for expat 2.4 and later); every other `SYNTAX`. Invalid UTF-8 in a UTF-8 XML document is expat's `XML_ERROR_INVALID_TOKEN`, so `SYNTAX`: XML reads by its declaration (A-5) and does not claim what expat does not say |
    | `cbor.c` | `TRUNCATED`: "the document ends inside a data item", "a length of %U, and %zu bytes are left", "a map of %U pairs, and ...", "an array of %U items, and ...". `STORAGE`: "the value could not be recorded", "an indefinite-length text string needs sink ...", "an indefinite-length map key needs sink work ...", "the document could not be recorded", "sink refused". `KEY`: "'%S' is not a valid key", "a map key that is %s, which a configuration ...". `UTF8`: "a text string that is not UTF-8". `DEPTH`: "nests deeper than %d levels". `NUMBER_RANGE`: "an integer past the 64-bit signed range, ...". `UNSUPPORTED`: "a byte string, which ...", "undefined, which ...", "this build has no floating point support", "simple value %u, which ...", "tag %U, which this reader does not read: ...". `EMPTY`: "the document is empty". `TOP_LEVEL`: "a configuration is a map at the top, ...". `TRAILING`: "bytes after the document". `SYNTAX`: every other call. No `where`: line and column are 0 |
    | `tlv.c` | `TRUNCATED`: "a tag cut short", "a length cut short", "tag 0x%s says %U bytes, and %zu are left", "the document ends at byte %zu without the end tag". `UNSUPPORTED`: "an indefinite length, which ...", "tag 0x%s is not in the layout '%s'", "tag 0x%s is a record of %U bytes, ...", "a type this reader does not read". `TOO_LONG`: "a length of %u bytes". `KEY`: "'%s' is not a valid key". `NUMBER`: "holds a nibble that is not a digit". `NUMBER_RANGE`: "has %U digits, more than an integer ...", "is %U, past the largest integer ...". `UTF8`: "is text that is not UTF-8". `DEPTH`: "nests deeper than %d levels". `STORAGE`: "the value could not be recorded", "needs sink text storage", "the document could not be recorded", "sink refused". `STRUCTURE`: "continues a list that ended at byte ...". `TRAILING`: "bytes after the end tag". `EMPTY`: "the document is empty". `SYNTAX`: every other call. The layout's own checks (`tlv_check`, and the size and name checks in `cf_tlv_codec_sized`) are `NONE`: they refuse the program's description, not a document. No `where` |
    | `lib/config/src/sources/entry.c` | the dotenv lexer: `SYNTAX`: `expected NAME=value`, with `where.line` the line and `col` 0; `KEY`: `not a usable name`, same place. The environment and arguments: `cf_lex_cli`'s `not a valid option` `KEY`, `needs a value` `SYNTAX`; `cf_lex_env` refuses nothing of a document. The response file: `may not include another` `UNSUPPORTED`. `where` is empty for these |
    | `lib/config/src/bridge/flatten.c`, `lib/config/src/bridge/file.c` | the budget (the refusal after `cf_codec_decode` in `cf_flatten_decode`) `BUDGET`; `file.c`'s `contains a key this build cannot hold` `TOO_LONG`; `cf_source_by_content`'s three "cannot tell the format" refusals `NONE` (a choice of format, not a refusal of bytes) |
    | A-4 and A-5, written before this card | `cf_encoding_check`'s refusal `UTF8` with the line and column it already computes (A-5), and the hint is in the message as it was; A-4's three messages (`byte 0x%02X has no character in %s`, `an unpaired UTF-16 surrogate`, `ends in half a UTF-16 code unit`), the UTF-32 refusal and the refusal of an encoding named for a codec that has its own: `ENCODING`, the first two with their line and column. `cf_lex_dotenv`'s, the response file's, `cf_lex_env`'s and `cf_lex_cli`'s UTF-8 refusals and the memory and defaults sources': `UTF8` |
    | A-9 (`compose.c`) | `SYNTAX` (the table's existing row for what no other reason names): a repair or a filter that returns CF_FALSE without setting `err`, "the repair of '%s' refused the document" and "the filter of '%s' refused the document": `cf_error_place(err, CF_REASON_SYNTAX, 0, 0)` after the library's own message; one that sets `err` keeps what it set. `NONE`: "the repair of '%s' gave %zu bytes and no memory" (memory), "the filter of '%s' left a callback of its sink empty" (a definition), and every refusal of `cf_codec_compose_sized` (`CF_E_DEFINITION`) and its stub (`CF_E_CAPABILITY`) |

    Every `*_fail` that takes a name for a message (`toml_fail_named`,
    `xml_fail`'s `name`) passes the reason through as the others do.
  * **`file_hint` keeps them.** `file_hint` (`lib/config/src/bridge/
    file.c`) rebuilds the error to add "the content
    looks like": it saves `message`, `key`, `source` and `code`, and
    calls `cf_error_set`, which now zeroes `where` and `reason`. It saves
    `reason` and `where` too, and calls `cf_error_place(&st->err, reason,
    where.line, where.col)` after. Everything that copies an error
    (`create_failed` in `config.c`; `cf_live`'s four copies in `live.c`;
    `cf_edit_commit` in `edit.c`; the refusal a keyspace keeps, in
    `keyspace.c`) uses `cf_error_copy`, which carries them.
  * **What the report and Doctor show: nothing new, on purpose.** The
    provenance report has no member for a refusal: a configuration that
    exists was read whole, and one that was not has no report; the one
    refusal it carries, the replay source's, is returned by
    `cf_source_replay_error` (`report.c`) and printed by the caller.
    Doctor prints `doctor: <message>` (the `doctor: %s` lines of
    `tools/doctor/doctor.c` and the adapters' like lines) and that line is
    the message, unchanged, so scripts that read it keep working. The
    reason and the place are for programs: `configforge gen` and
    `codec` print the message only. Not decided here, and listed in the
    notes as a later weighing: a `"reason"` member in Doctor's JSON when
    it has one.
  * **The bindings read the new members.** C++ (`configforge.hpp`'s
    `Error`): `reason()` (int), `reason_name()` (`cf_reason_name`),
    `line()` and `column()` (from `where`), as the members are read in
    `e_` (the object is the program's, built with this header's size).
    Rust: `sys` gains `#[repr(C)] struct cf_loc { file: *const c_char,
    line: u32, col: u32 }`, `sys::cf_error` gains `where_: cf_loc` and
    `reason: c_int` after `message`, and `Error` (`#[non_exhaustive]`) gains `reason:
    String` (the name, empty for none: Rust keeps the name, not an enum,
    so a reason it does not know is still readable), `line: u32`,
    `column: u32`; `sys.rs` declares `cf_reason_name` by hand. Python:
    `_native.py` mirrors `cf_code.def`'s order for the codes (`E_POLICY`)
    and mirrors no order of `cf_reason.def`, since a reason travels as its
    name; its
    `cf_error` gains `("where_file", c_void_p), ("where_line", c_uint),
    ("where_col", c_uint), ("reason", c_int)` after `message`, the
    function `cf_reason_name` is declared `optional=True`, and `Error`
    gains `reason` (the name, `""` for none or for a library that lacks
    the function), `line` and `column`, as keyword arguments of its
    constructor with defaults (`reason=""`, `line=0`, `column=0`), so
    every place that builds an `Error` today keeps working. A library older than 1.8 never
    writes them (the size travels, `new_error` sets it), so they are 0.
    One test each: a refused JSON with a number out of range, or the
    simplest document each reader refuses, giving `reason == "syntax"`,
    `line == 1`.
  * **The test**, `lib/config/tests/test_reason.c`, below: every row read
    through `cf_builder_document` so the reason must survive the copies
    from the source to the caller, and the D-51 rule on its own.
* **files**: `lib/base/include/configforge/cf_reason.def` (new),
  `lib/base/include/configforge/cf_error.h`,
  `lib/base/src/error.c`, `lib/serialize/include/configforge/cf_serialize.h`
  (the `cf_loc` typedef moves out), `lib/text/src/fail.c` and
  `lib/text/include/configforge/cf_text.h` (`cf_text_fail`'s `reason`),
  `lib/serialize/src/{json,toml,ini,
  properties,yaml,yamlparse,xml,cbor,tlv}.c`, the text component's
  `cf_encoding_check` (`lib/text/src/encoding.c`, A-5) and A-4's refusals,
  `lib/config/src/sources/entry.c`,
  `lib/config/src/bridge/{flatten,file,dir}.c` (the encoding and
  detection refusals A-4 and A-5 added to `file.c` and `dir.c`),
  `lib/config/src/core/{config,live,edit}.c` only if a copy is not by
  `cf_error_copy`, the memory and defaults sources A-5 changed,
  `lib/serialize/src/compose.c` (A-9), `lib/cxx/include/configforge/
  configforge.hpp`, `bindings/rust/src/{sys,lib}.rs`,
  `bindings/python/src/configforge/{_native,_config}.py`
  (`_native.py` mirrors `cf_code.def`'s order for the codes and gains
  the reason fields),
  `lib/base/README.md` (its list of public headers gains
  `cf_reason.def`, beside `cf_code.def`),
  `scripts/generate/apiref.cmake` (it reads `cf_code.def` for the table
  of codes in `docs/API.md`; it reads `cf_reason.def` the same way, so
  the reasons appear there too, and `GateApiref` holds it),
  `scripts/generate/amalgamate.py` not at all (`resolve` inlines every
  `.def` a source includes, `cf_code.def` is named nowhere), the symbol
  lists (gate.abi `-DWRITE=ON`: `cf_reason_name 1.8`,
  `cf_reason_description 1.8`, `cf_error_place 1.8` in the experimental
  list); tests: `lib/config/tests/test_reason.c` (new) and its entry in
  `lib/config/tests/CMakeLists.txt`'s `CF_UNIT_TESTS`,
  `lib/config/tests/test_budget.c` (a line in `one()`), the
  bindings' tests; last: `docs/Guide.md` (section 7, "Errors"),
  `docs/Decisions.md`, `docs/API.md` (regenerated), `CHANGELOG.md`.
* **steps**:
  1. Read A-8's notes and its code: write in this card's notes the name
     of the one failure formatter F, and which readers still format by
     hand. Before any change, write a throwaway program outside the tree
     that reads every row of step 2's table (and `tests/support/data/
     edges.*`) and prints each refusal's code, source and message; run
     it, keep the output as `before.txt` outside the tree.
  2. `test_reason.c`:
     * **Part one, the size rule.** A buffer of `sizeof(cf_error) + 64`
       bytes filled with `0xAB`; a `cf_error` at its start with `size =
       offsetof(cf_error, where)` (what 1.7 had): `cf_error_set` then
       `cf_error_place(e, CF_REASON_SYNTAX, 3, 4)` leaves every byte from
       `offsetof(cf_error, where)` on still `0xAB`; with `size =
       sizeof(cf_error)` the same calls give `reason == CF_REASON_SYNTAX`,
       `where.line == 3`, `where.col == 4`, `where.file == NULL`;
       `cf_error_set` after that gives `reason == CF_REASON_NONE` and
       `where` zero; `cf_error_clear` the same; `cf_error_copy` from a
       full into a full carries both, from a full into a smaller leaves
       the bytes past 1.7's untouched, from a smaller into a full gives
       zero; a size-0 `cf_error` is untouched past its 1.0 fields;
       `cf_error_place(NULL, ...)` does nothing; `cf_reason_name` of
       every reason is the name in the table above, of `-1` and of a
       value past the last is `"?"`, and `cf_reason_description` of those
       `"unknown"`; the list has 20 entries and `CF_REASON_STORAGE` is
       19.
     * **Part two, the rows.** A table of `{format, bytes, length,
       reason, line, col}`, each read with `cf_builder_document(b,
       "doc", CF_RANK_USER_FILE, bytes, n, format, 0)` and
       `cf_builder_create`; the build fails, `err.reason` is the row's,
       `err.where.line` is the row's when it is not 0, and `err.where.col`
       when it is not 0; `err.code` is what the message-bearing call
       gives today (the code in `before.txt`, step 1).
       Formats a build left out are skipped by `cf_codec_by_name`; XML
       rows are under `#if CF_WITH_XML`; `tlv` under `#if CF_WITH_TLV`
       with a layout of its own in the test (name `devcfg`, two-byte tag
       and length, little-endian, end tag `FFFF`, one tag `0x0001`,
       `name`, text). Bytes are C strings with `\x` escapes, split so an
       escape is never followed by a hex digit; the last column of `n`
       is `sizeof(doc) - 1` except where a NUL is in the document.

       | Reason | Format: document (line, col) |
       | ------ | ---------------------------- |
       | `UTF8` | json `{"a":"\xFF"}` (1, 7); json5 the same (1, 7); toml `a = "\xFF"` + LF (1, 6); yaml `a: \xFF` + LF (1, 4); ini `a=\xFF` + LF (1, 3); properties `a=\xFF` + LF (1, 3); cbor `A1 61 61 61 FF`; tlv `01 00 01 00 FF FF FF` |
       | `DEPTH` | json and json5: `{"a":` repeated `CF_MAX_DEPTH + 2` times, `1`, then `}` repeated the same; toml: `a=` and `{b=` nested `CF_MAX_DEPTH + 2` inline tables deep, closed; yaml: `{a: ` repeated `CF_MAX_DEPTH + 2`, `1`, `}` repeated; cbor: `A1 61 61` repeated `CF_MAX_DEPTH + 2` times then `01`; xml: `<a>` repeated `CF_MAX_DEPTH + 2`, then `</a>` the same. Not tlv (a layout is checked to `CF_MAX_DEPTH`, so no document reaches deeper), not ini or properties (no nesting) |
       | `TRUNCATED` | json `{"a":"x`; json5 the same; toml `a = [1, 2`; yaml `a: [1, 2`; ini `[sec`; cbor `A1 61 61`; tlv `01 00 05 00 61`; xml `<a><b>1</b>` |
       | `TRAILING` | json `{"a":1} x`; json5 the same; yaml `a: 1` LF `---` LF `b: 2` LF; cbor `A0 00`; tlv `FF FF 00`; xml `<a/><b/>` |
       | `EMPTY` | json and json5: zero bytes; cbor: zero bytes; tlv: zero bytes; xml: zero bytes |
       | `TOP_LEVEL` | json `[1]`; json5 the same; yaml `- 1` LF; cbor `81 01` |
       | `NUMBER` | json `{"a":01}`; json5 the same; toml `a = 01` LF |
       | `NUMBER_RANGE` | toml `a = 9223372036854775808` LF; cbor `A1 61 61 1B 80 00 00 00 00 00 00 00` |
       | `DUPLICATE_KEY` | toml `a = 1` LF `a = 2` LF; xml `<a b="1" b="2"/>` |
       | `KEY` | json `{"A":1}`; json5 the same; toml `A = 1` LF; yaml `A: 1` LF; ini `A=1` LF; properties `A=1` LF; cbor `A1 61 41 01`; xml `<a><B>1</B></a>`; (`CF_E_KEY` for ini and properties) |
       | `TOO_LONG` | json and json5: `{"` + 300 times `a` + `":1}`; toml: 300 times `a`, ` = 1` LF; ini: 300 times `a`, `=1` LF (`CF_E_KEY`) |
       | `ESCAPE` | json `{"a":"\q"}`; json5 the same; toml `a = "\q"` LF; yaml `a: "\q"` LF; properties `a=\u12` LF |
       | `CONTROL` | json `{"a":"\x01"}`; json5 the same; toml `a = "\x01"` LF; yaml `a: "\x01"` LF |
       | `STRUCTURE` | toml `a = 1` LF `[a]` LF; xml `<a x="1"><x>2</x></a>` |
       | `UNSUPPORTED` | cbor `A1 61 61 41 61` (a byte string); xml `<!DOCTYPE a><a/>`; yaml `a: !foo 1` LF |
       | `SYNTAX` | json `{"a" 1}`; json5 the same; toml `a 1` LF; yaml `a: b: c` LF; ini `a` LF; cbor `A1 61 61 1C`; xml `<a></b>` |

       The yaml rows of `TRUNCATED`, `ESCAPE` and `CONTROL` are reasons
       of the built-in parser alone: they are compiled under `#if` of
       it (`cf_yamlparse.h`'s `CF_YAML_LIBYAML` and `CF_YAML_LIBFYAML`
       both 0), and where libyaml or libfyaml reads, the same three
       documents are rows of `SYNTAX`, which the yaml step's builds check.

       and, outside the table because they need more than a document:
       `ENCODING` through the encoding of A-4 (a Windows-1254 file
       holding byte 0x81 read with `CF_ENCODING_WINDOWS_1254`, from
       `test_encoding.c`'s fixture, through `cf_builder_file`);
       `BUDGET` in `test_budget.c`'s `one()`, one added line
       `check(err.reason == CF_REASON_BUDGET, ...)` for every format it
       covers; `STORAGE` through a sink whose callbacks all return false,
       `cf_codec_decode` of `{"a":1}` by json, json5, toml (`a=1`) and
       ini (`a=1`) with that sink, `reason == CF_REASON_STORAGE`;
       dotenv through `cf_builder_dotenv` on a file written by the test:
       `A=\xFF` LF is `UTF8`, `NO EQUALS` LF is `SYNTAX` (line 1),
       `=1` LF is `KEY` (line 1).
     * **Part three, the claim.** A loop over the reasons in the table
       order: for each, every row of that reason was refused with it, and
       the number of formats per reason is printed when the test runs, so
       a format that stops refusing, or starts, shows in the output. A
       format that accepts a bad input of a kind (JSON, JSON5, YAML, INI
       and `.properties` take a duplicate key and the later one wins) is
       not a row: making them refuse is not this card, and the notes say
       so.
     See it fail to compile (`CF_REASON_*` unknown).
  3. `cf_reason.def`, `cf_error.h` (the typedef moved, the members, the
     enum, the three functions), `error.c`; part one passes. `cf_loc`
     moved out of `cf_serialize.h`; the whole tree builds.
  4. The readers, one at a time in the order of the table above, each
     followed by its rows: F's new parameter, every call site, the
     `cf_error_place` after each hand-made `cf_error_set`. After each
     reader, `ctest -R unit` passes (the messages did not change).
  5. `entry.c`, `flatten.c`, `file.c` and A-4's and A-5's refusals,
     `file_hint`'s save and restore, A-9's messages (the table's A-9 row); the
     rows outside the table pass.
  6. Run step 1's throwaway program again: `diff before.txt after.txt`
     has no line (the code, source and message of every refusal are what
     they were).
  7. The bindings and their tests; the Guide (section 7), the symbol
     lists; ACCEPT.
* **accept**: ACCEPT-LINUX with
  `-DEXTRA="abi;coverage;conformance;fuzz;shared;yaml;components;compilers"`
  (a struct that grew, the coverage floor, every reader touched and the
  suites unchanged, the readers' fuzz targets, the Python binding's
  tests, the libyaml and libfyaml builds, whose YAML rows differ, every
  component built alone with `cf_error.h` changed, and the musl, tcc and
  pcc builds), ACCEPT-WINDOWS,
  ACCEPT-CI (cargo included). Step 6's diff is empty.
* **stop if**: a reader's call site cannot be given one of the reasons
  above without changing its message or its code: write the call in the
  notes and use `CF_REASON_SYNTAX`. `abi` reports an incompatible change
  for `cf_error` that it did not report for `cf_codec`'s growth in 1.5
  (D-51 and D-56 say a member appended to a sized struct is not breaking;
  `git log -S decode_self --oneline` finds that release): stop and
  report. A reader that formatted a message on its own and is not in the
  table: add its calls here and say so in the notes. A format that
  refuses a row's document with a reason other than the table's because
  its reader found another error first: change the document, never the
  reason, and say so in the notes.
* **docs**: D-107's entry (written by A-1) gets "Built: A-10" with the
  commit, and the line "`where` holds line and column; the byte offset
  D-107 first named stays in the message". The Guide's section 7,
  "Errors", gets a paragraph and a short table of the reasons, and the
  sentence that a program treats a reason it does not know as `syntax`;
  the example is a program that tells a UTF-8 refusal (`CF_REASON_UTF8`)
  from the rest and offers the encoding's name. `docs/Maintaining.md`'s
  list of what is outside the promise says the text of messages still
  is, and a reason's name is inside it. CHANGELOG `### Added`:
  "`cf_error` carries the place (`where`) and the reason (`reason`, from
  `cf_reason.def`) of a refusal, the same in every reader: invalid UTF-8,
  depth, a number out of range, a truncated input and the rest; `cf_reason_name`,
  `cf_reason_description` and `cf_error_place`. Every message is what it
  was (D-107)." `docs/Roadmap.md`: the item "Infrastructure behind
  abstractions" stays 🟨, its line gains "A-10 done".
* **Organiser's answers, 2026-11-22**:
  * INI's TOO_LONG row uses a 300-character name followed by `[1]=1`; the plain name reaches the scalar sink's KEY refusal, while the indexed name reaches INI's length check.
  * The minimal build's composition test compares the invalid-name diagnostic up to the configured message capacity; the 49-byte fixture puts part of the wording beyond 128 bytes.
  * During step 4, run the current reader's reason rows and the existing unit tests; run the combined reason test once its remaining readers and step 5 sources are implemented.
  * Step order: `file_hint`'s keeping of `reason` and `where`, and the
    budget's reason, move into step 4, before the readers' checks that
    depend on them; step 5 keeps the rest.
  * The STORAGE row's JSON and JSON5 case uses a sink that accepts the
    begin and value events and refuses `end`, so the reader's own "sink
    refused" is what is classified; a refused root `begin_table` gives
    "key is not valid", which stays `KEY` as the table says.
  * TOML's DEPTH row is tested through `cf_codec_decode` directly, with a
    sink that accepts every event, so that the reader's own depth check is
    what refuses (through the configuration the flattener's key check
    refuses first, and that is not the reason the row is about).
  * JSON5 accepts `\q` (any other character stands for itself) and a raw
    0x01 inside a string, so those two fixtures are replaced: the ESCAPE
    row's JSON5 case is `\8` ("a digit may not be escaped"), and the
    CONTROL row's JSON5 case is a raw line feed inside a string ("a string
    may not contain a raw newline"). Both were probed to give those
    refusals.

* **notes**:
  * 2026-11-22: F is `cf_text_fail`. The text wrappers are `js_fail`,
    `toml_fail`, `toml_fail_named`, `prop_fail`, `yaml_fail_at`,
    `yp_fail` through `yp_fail_at`, and `xml_fail`. The XML parse error,
    INI line refusals, and `.properties` key refusals already call F
    directly. `cbor_fail` and `tlv_fail` still format byte offsets by
    hand. Direct `cf_error_set` calls also remain in the readers for
    errors such as sink refusals and workspace failures.
  * 2026-11-22: The baseline accepts both the JSON5 `ESCAPE` row
    (`\q`) and the JSON5 `CONTROL` row (raw `0x01`). Step 2 needs a
    decision on replacing or dropping those rows while preserving
    reader behaviour. The existing reader refuses `\8` and a raw LF
    inside a string, respectively, with the messages listed for those
    reasons. The baseline also finds `KEY` first in the TOML `DEPTH`
    row and `SYNTAX` first in the TOML and built-in YAML `TRUNCATED`
    rows. Those three rows fall under the card's fixture replacement rule.
  * 2026-11-22: The Release baseline confirms `a = [1, 2,` for TOML
    ("the array is not closed", 1:11) and `a: "x` for built-in YAML
    ("a quoted value is not closed", 1:6) as `TRUNCATED` fixtures. The
    TOML `DEPTH` row still needs a decision. Through `cf_builder_document`,
    nested inline tables, nested arrays and a maximum-length dotted
    array-of-tables header all report "not a valid key". The flattener
    counts the document root in its `CF_MAX_DEPTH` frames. TOML opens
    that root before any array, inline table or header, so the sink
    refuses a begin event before TOML can reach either depth check.
    Should this one row use `cf_codec_decode` with a sink that accepts
    all events, or should the organiser change the specified reason for
    a depth refusal from the flattener? The runtime and the test have
    not been changed.
  * 2026-11-22: The organiser's replacements were checked against the
    Release baseline. Direct TOML decoding with an accepting sink gives
    `CF_E_SOURCE`, `doc:1:99: nesting is too deep` at the default depth.
    The baseline, including the replacement inputs and `edges.*`, is
    saved at `%TEMP%/cf-codex-a10-release/A-10/before.txt`.
  * 2026-11-22: Step 2's `STORAGE` case conflicts with the JSON and JSON5
    reason table. With every sink callback returning false, both decoders
    stop at the root `begin_table` (`json.c:529`) with `CF_E_SOURCE` and
    `doc:1:1: key is not valid`. The table assigns that message `KEY`.
    Their `sink refused` calls are reached only at `end`. The same probe
    reaches `sink refused` in TOML and INI. Should the JSON and JSON5 test
    accept the begin and scalar events and refuse `end`, or should the
    root `begin_table` refusal get `STORAGE` while keeping its message
    and code? Stopped before adding the test or changing the readers.
  * 2026-11-22: Step 2 failed before the implementation as intended.
    The Release MSVC build exited 2 with C2065 for `CF_REASON_SYNTAX`
    at `lib/config/tests/test_reason.c:34` and C2039 at line 21 for
    the missing `cf_error.reason` field. Step 3 built successfully and
    passed all 43 size and reason-name checks.
  * 2026-11-22: Stopped in step 4 after classifying the JSON and JSON5
    calls. Both targeted runs gave 96/99 checks. The JSON number, escape
    and control rows and the JSON5 number, key and control rows all reach
    `file_hint` through the builder. It rewrites the message to append
    the detected format and resets the new metadata. The Release unit
    suite exited 8 (119/121 passed; `test_reason` and `test_budget`
    failed). Step 4 asks for green reader rows and unit tests, but step 5
    schedules both the `file_hint` preservation and the budget reason.
    May the two step 5 prerequisites move ahead of the reader checks, or
    should step 4 check reader output directly and defer the builder
    and budget assertions to step 5? Other callers of the widened
    formatter use `CF_REASON_NONE` while awaiting classification.
  * 2026-11-22: JSON, JSON5, YAML, INI and properties accept repeated
    keys, so they have no duplicate-key refusal row. A reason in Doctor's
    JSON is left for a later decision. The step 6 comparison of the
    saved Release baseline and the rebuilt probe had no differences.
  * 2026-11-22: MSVC Release and Debug each passed 241/241 tests;
    minimal passed 162/162 with the requested exclusions. An additional
    Release build with Expat passed all 484 refusal checks. The Rust
    library passed 32/32 tests. Python binding tests, Linux and CI
    acceptance were not run here.
---

## A-11 - Build profiles and the C version per file

* **id**: A-11
* **goal**: `-DCF_PROFILE=<name>` (or `-DCF_PROFILE_FILE=<path>`) sets
  a named group of options from `cmake/profiles/<name>.cmake`, an
  option given by hand still wins, the configure step says what was
  chosen and what differs from it, and the library says which profile
  it was built with. A source file that needs more than C89 says so
  through `cf_source_needs_c`, and `gate.cversion` fails a file above
  C89 that did not. `yaml.c` (with libfyaml) and `posix.c` (with
  CoreFoundation) are the two files on it. CI builds every profile.
* **why**: D-105 and D-106. Today a build is forty switches or a copy
  of a preset's `cacheVariables`, and a file's C version is a
  `set_source_files_properties` that nothing holds: the next one can be
  added by hand, and `minimal` and `misra` could not say they refuse it.
* **needs**: A-1b, and A-10 by the order (paths below are the ones after
  A-1's map)
* **size**: large
* **decided**:
  * **A profile is a data file in CMake's spelling.** The build never
    `include()`s it; `cmake/ConfigForgeProfile.cmake` (new) reads it
    line by line, and so does A-12's script, with the one parser
    `cf_profile_parse(<file> <prefix>)` (it sets `<prefix>_NAMES`, the
    options in file order, and `<prefix>_<OPTION>`, each value; it
    stops with `FATAL_ERROR "<file>:<line>: ..."` at a line that is
    none of the three kinds). A line is blank, a comment (`# ...`), or
    exactly
    `set(<OPTION> <VALUE> CACHE <BOOL|STRING> "profile")` with one
    space between the parts. The first line of a shipped profile is
    `# <one sentence, what the profile is for>`; A-12 prints it. A shipped
    profile's lines are at most 78 columns, comments included, since
    cmake-lint's C0301 reads them (the sentences below are written to
    fit; a longer one is shortened, never wrapped).
  * **The options a profile may set**, in this order, as
    `CF_PROFILE_OPTIONS` (`NAME:TYPE`) in `ConfigForgeProfile.cmake`:
    `CF_WITH_INI`, `CF_WITH_JSON`, `CF_WITH_TOML`, `CF_WITH_PROPERTIES`,
    `CF_WITH_CBOR`, `CF_WITH_TLV`, `CF_WITH_COMPOSE` (A-9), `CF_WITH_XML`,
    `CF_WITH_YAML`, `CF_WITH_SQLITE`, `CF_WITH_PREFERENCES`, `CF_WITH_DIR`,
    `CF_WITH_DOTENV`, `CF_WITH_EMIT`, `CF_WITH_IMAGE`, `CF_WITH_REPORT`,
    `CF_WITH_REAL`, `CF_WITH_CXX` (all BOOL); `CF_YAML_BACKEND`,
    `CF_PLATFORM` (STRING); `CF_SINGLE_THREADED`, `CF_FORCE_PORTABLE`,
    `CF_GC_SECTIONS`, `CF_LTO`, `CF_C89_ONLY` (BOOL);
    `CF_LANES_BACKENDS` (STRING; D-101, already declared in
    `lib/lanes/lanes_sources.cmake` with the default `all`, so a profile
    creates its cache entry first and that declaration leaves it alone); `CF_KEY_MAX`,
    `CF_MESSAGE_MAX`, `CF_PATH_MAX`, `CF_MAX_DEPTH`, `CF_STACK_BUDGET`
    (STRING). Not in it, and set by the preset or by hand: the build
    type, `CF_BUILD_TOOLS`, `CF_BUILD_TESTING`, `CF_BUILD_EXAMPLES`,
    the sanitisers, `CF_HARDEN`, `CF_WERROR`, `CF_WAKE_KQUEUE`. A
    profile file naming another option is refused at configure time:
    `<file>:<line>: <OPTION> is not an option a profile may set`.
  * **`CF_C89_ONLY`** is a new BOOL option, default OFF, declared in
    `cmake/ConfigForgeCVersion.cmake` as `option(CF_C89_ONLY "Refuse
    every source file that needs more than C89 (D-106)" OFF)`. ON in
    `minimal`, `misra`, `embedded-json` and `embedded-cbor`: the
    profiles for firmware and for a safety case. D-106 names the first
    two; the embedded ones have no part that needs more than C89 and
    are held to it the same way.
  * **The seven profiles**, every option in every file, in the order
    above. `ON`/`OFF` are for the BOOL ones.

    | Option | full | default | minimal | misra | embedded-json | embedded-cbor | server |
    | ------ | ---- | ------- | ------- | ----- | ------------- | ------------- | ------ |
    | `CF_WITH_INI` | ON | ON | ON | ON | OFF | OFF | ON |
    | `CF_WITH_JSON` | ON | ON | ON | ON | ON | OFF | ON |
    | `CF_WITH_TOML` | ON | ON | ON | ON | OFF | OFF | ON |
    | `CF_WITH_PROPERTIES` | ON | ON | ON | ON | OFF | OFF | ON |
    | `CF_WITH_CBOR` | ON | ON | ON | ON | OFF | ON | ON |
    | `CF_WITH_TLV` | ON | ON | ON | ON | OFF | OFF | ON |
    | `CF_WITH_COMPOSE` | ON | ON | OFF | OFF | OFF | OFF | ON |
    | `CF_WITH_XML` | ON | OFF | OFF | OFF | OFF | OFF | ON |
    | `CF_WITH_YAML` | ON | ON | ON | ON | OFF | OFF | ON |
    | `CF_WITH_SQLITE` | ON | OFF | OFF | OFF | OFF | OFF | OFF |
    | `CF_WITH_PREFERENCES` | OFF | OFF | OFF | OFF | OFF | OFF | OFF |
    | `CF_WITH_DIR` | ON | ON | OFF | OFF | OFF | OFF | ON |
    | `CF_WITH_DOTENV` | ON | ON | OFF | OFF | OFF | OFF | ON |
    | `CF_WITH_EMIT` | ON | ON | OFF | OFF | OFF | OFF | ON |
    | `CF_WITH_IMAGE` | ON | ON | OFF | OFF | OFF | OFF | ON |
    | `CF_WITH_REPORT` | ON | ON | OFF | OFF | OFF | OFF | ON |
    | `CF_WITH_REAL` | ON | ON | OFF | OFF | OFF | OFF | ON |
    | `CF_WITH_CXX` | ON | ON | ON | OFF | OFF | OFF | ON |
    | `CF_YAML_BACKEND` | auto | auto | builtin | builtin | auto | auto | auto |
    | `CF_PLATFORM` | auto | auto | auto | auto | auto | auto | auto |
    | `CF_SINGLE_THREADED` | OFF | OFF | OFF | ON | ON | ON | OFF |
    | `CF_FORCE_PORTABLE` | OFF | OFF | OFF | ON | ON | ON | OFF |
    | `CF_GC_SECTIONS` | OFF | OFF | ON | ON | ON | ON | OFF |
    | `CF_LTO` | OFF | OFF | OFF | OFF | OFF | OFF | OFF |
    | `CF_C89_ONLY` | OFF | OFF | ON | ON | ON | ON | OFF |
    | `CF_LANES_BACKENDS` | all | all | c89 | scalar | c89 | c89 | all |
    | `CF_KEY_MAX` | 256 | 256 | 64 | 64 | 64 | 64 | 256 |
    | `CF_MESSAGE_MAX` | 256 | 256 | 128 | 128 | 128 | 128 | 256 |
    | `CF_PATH_MAX` | 512 | 512 | 256 | 256 | 256 | 256 | 512 |
    | `CF_MAX_DEPTH` | 32 | 32 | 32 | 32 | 16 | 16 | 32 |
    | `CF_STACK_BUDGET` | 2048 | 2048 | 1024 | 1024 | 1024 | 1024 | 2048 |

    Why each is what it is:
    * `default` is today's defaults (cmake/ConfigForgeOptions.cmake,
      ConfigForgeSize.cmake, `CF_YAML_BACKEND` in
      `lib/serialize/backends.cmake`), and
      `gate.profiles` holds it to them.
    * `minimal` is today's `minimal` preset (CMakePresets.json) to the
      option: the six parts off and A-9's `CF_WITH_COMPOSE` off, the
      three limits and the budget, `CF_GC_SECTIONS` ON, the codecs, YAML
      and the C++ header left on. Only `CF_YAML_BACKEND=builtin`,
      `CF_C89_ONLY=ON` and `CF_LANES_BACKENDS=c89` are new, and none
      changes what is built: `auto` takes the built-in YAML parser, and
      until D-101's other backends exist every value of
      `CF_LANES_BACKENDS` builds scalar alone.
    * `misra` is Compliance.md's "The MISRA profile": `minimal`, one
      thread (`CF_SINGLE_THREADED`), no reals, no directories, plus
      `CF_FORCE_PORTABLE` (Rule 1.2, language extensions: every
      compiler-specific upgrade off) and no C++ header (the C library's
      profile; `lib/cxx` has its own rule, Compliance.md). The codecs
      stay: the matrix of 1.9.0 narrows them against the deviation
      records, and this file is changed then.
    * `embedded-json` and `embedded-cbor`: one format, no files beyond
      the platform's own, no report, no emit, no reals, one thread,
      portable, a smaller nesting depth (the decoders recurse once per
      level, so a level is stack; 16 is chosen, not measured, and
      A-12's table measures it). Limits and budget as `minimal`.
    * `full` is everything that builds on a host with expat and sqlite3
      installed. `CF_WITH_PREFERENCES` stays OFF in it: it links
      CoreFoundation and builds on macOS only (CI's `macos-preferences`
      job; `-DCF_WITH_PREFERENCES=ON` on a Mac).
    * `server` is `default` with XML added (every format), threads on.
      `CF_WITH_SQLITE` stays OFF in it, as in `default`: the SQLite
      source needs sqlite3's package, and XML's expat is the one package
      `server` asks for. The line for the run-time choice of vector
      backend is added to it by the card that builds the backends (2.4.0,
      D-101): there is no option for it today, and a profile does not set
      what does not exist. The same holds for `misra`'s byte loops
      (Compliance.md names `CF_WITH_SWAR=0`; no such option exists, and
      `misra`'s `CF_LANES_BACKENDS=scalar` is what it asks for).
    * `CF_PLATFORM` is `auto` in all: no job builds `none` today, so no
      profile claims it.
    The shipped files are written from this table, with the sentence of
    each profile's first line as follows: `full`: "Every part that
    builds on a host with expat and sqlite3."; `default`: "What a plain
    build gives: the formats that need nothing, reals, threads.";
    `minimal`: "Codecs only, short limits, a 1 KB stack budget, unreached
    code dropped."; `misra`: "The build a MISRA C checker runs against:
    minimal, one thread, no reals."; `embedded-json`: "JSON alone, for a
    device: no files, no reals, one thread, nesting 16.";
    `embedded-cbor`: "CBOR alone, for a device: no files, no reals, one
    thread, nesting 16."; `server`: "Every format, XML too, and every
    source but SQLite and preferences." (each on one line with its `# `,
    at most 78 columns, ending with a full stop).
  * **How the options are read.** CMake keeps one cache entry per name,
    and both `set(<X> <v> CACHE <T> "...")` without `FORCE` and
    `option()` leave an existing entry alone. So the first thing to
    create the entry wins, and the order is: (1) `-D` on the command
    line and a preset's `cacheVariables`, which exist before
    `CMakeLists.txt` runs; (2) the profile, loaded as the first
    statement of `cmake/ConfigForgeOptions.cmake`, before its first
    `option()` and before `ConfigForgeSize.cmake`, `ConfigForgeCVersion.cmake`
    and `lib/serialize/backends.cmake` declare theirs; (3) each
    option's own default. `cf_profile_load()` does exactly this for
    every line: it remembers the profile's value in the variable
    `_cf_pv_<OPTION>` and then runs `set(<OPTION> <value> CACHE <type>
    "set by profile <name>")`, no `FORCE`. Nothing else is needed for
    "a -D wins". Two things this does not do, said in the README: a
    parent project's normal variable `set(CF_WITH_TOML OFF)` before
    `add_subdirectory` shadows the cache and wins too, as it does for
    `option()`; and a value an earlier configure left in the build
    directory stays (next item).
  * **A build directory keeps its profile.** The cache would keep the
    first profile's values when `CF_PROFILE` changes, so the profile
    would half apply. `cf_profile_load()` stores
    `CF_PROFILE_APPLIED` (`CACHE INTERNAL`) as `<name>|<SHA-256 of the
    profile file>` (`file(SHA256 ...)`) on the first configure; a later
    configure whose name or hash differs stops:
    `CF_PROFILE is '<new>', but this build directory was configured with
    '<old>', or that profile's file has changed: the options it set are
    kept in CMakeCache.txt, so the new profile would only half apply.
    Use another build directory, or delete CMakeCache.txt here.` `-D`
    options are not part of the check and stay as given.
  * **`CF_PROFILE` and `CF_PROFILE_FILE`.** `set(CF_PROFILE default
    CACHE STRING "Build profile: full, default, minimal, misra,
    embedded-json, embedded-cbor or server")` with `STRINGS` set;
    `set(CF_PROFILE_FILE "" CACHE FILEPATH "A build profile of your own,
    in the form of cmake/profiles/*.cmake")`. When `CF_PROFILE_FILE` is
    not empty it is used (relative to `CMAKE_SOURCE_DIR`, the program's
    tree), `CF_PROFILE` is not, and the configure step says so:
    `profile: CF_PROFILE_FILE is set; CF_PROFILE '<x>' is not used`. A
    file of one's own need not list every option (the rest take their
    defaults) but follows the grammar and the list above. An unknown
    `CF_PROFILE` stops: `CF_PROFILE '<x>' is not a profile; there are:
    full, default, minimal, misra, embedded-json, embedded-cbor,
    server`. The profile's name is `<name>` for a shipped one and
    `file:<file name without .cmake>` for one's own; the variable
    `CF_BUILD_PROFILE_NAME` holds it. A component built alone, `cmake -S
    lib/<c>`, includes `ConfigForgeOptions.cmake` too and so loads the
    profile in the same way, which makes its `CF_BUILD_PROFILE_NAME`
    `default` (the value of `CF_PROFILE`). The file name part must be one or
    more of `A-Z`, `a-z`, `0-9`, `.`, `_`, `+` and `-`: the name goes
    into a C string and a ctest name unescaped, so another character
    stops the configure step: `CF_PROFILE_FILE: the file name '<x>' may
    hold only letters, digits and . _ + -`.
  * **The configure-time printout**, from `cf_profile_report()`,
    called in `CMakeLists.txt` just before the existing `limits:`
    message. `message(STATUS ...)` lines, in this order and
    with this text:

    ```text
    -- profile: <name> (<path of the file, relative to the source tree when inside it>)
    -- profile: <OPTION> is <value>, the profile says <value>       (one per option that differs, in the order above)
    -- profile: no option differs from it                           (instead, when none does)
    -- profile: library code about <N> KB (README table; parts overlap, so a build with several off is a little larger)
    ```

    The last line is the estimate: `<N>` is the README's figure for the
    whole library, `from <B> KB to`, less the figure of each row of the
    table "Leaving parts out" (rows
    `` | `CF_WITH_X` | text | <d>.<d> KB | ``) whose option is OFF in this
    build, rounded to a whole KB. An option that is ON here and OFF in
    `default` has no row (XML, SQLite, preferences): the line is then
    `profile: library code about <N> KB, not counting CF_WITH_XML`
    (the names comma-separated, in option order; no closing sentence).
    When the README has no such
    table or no `from <B> KB` the line is `profile: library code size
    not estimated (the README's table was not found)`. Worked case,
    from today's README (92 KB; 5.9, 1.1, 1.3, 6.7, 4.1, 5.5 off):
    `minimal` prints `about 67 KB`. A-12 keeps these two README forms
    exactly when it generates them.
  * **The profile in the library.** Generated `cf_build_options.h`
    gets `#define CF_BUILD_PROFILE "@CF_OPT_PROFILE@"` (template
    `cmake/templates/cf_build_options.h.in`, after the limits;
    `configforge_generate_options` sets `CF_OPT_PROFILE` from
    `CF_BUILD_PROFILE_NAME`); the source tree's copy
    (`lib/port/include/configforge/cf_build_options.h`) gets, in its
    style, `#ifndef CF_BUILD_PROFILE` / `#define CF_BUILD_PROFILE
    "unspecified"` (a build of the sources by hand states no profile).
    New public function in `lib/config/include/configforge/cf_version.h`
    beside `cf_build_id`, `CF_EXPERIMENTAL(1, 8) const char*
    cf_build_profile(void);` with the comment "The build profile this
    library was built with (D-105): "default", "minimal", "file:acme",
    or "unspecified" for a build of the sources that named none. Never
    NULL." Defined in `lib/config/src/core/version.c`, beside
    `cf_build_id`, as `return CF_BUILD_PROFILE;`. The
    provenance report's `library` object gets one more member after
    `"build"` (`lib/config/src/diag/report.c`, the `"library": { "version": ... "build": `
    line): `"build_profile": "<name>"`, written
    with the same `report_quote_z`. The name is not `profile`: a
    report's top-level `"profile"` is the program's configuration
    profile. Schema 1 readers ignore members they do not know (the
    comment above the `"library"` line of `cfi_report_reads` in `report.c`
    says the same of `build`).
  * **The build id stays the commit.** `cf_build_id()` is pinned as a
    commit hash by `test_version` in `lib/config/tests/test_features.c`
    and by the
    Python binding, and D-105 and D-106 say the profile and the C
    versions go "into the build id". They go into the build's other
    identification: the SBOM (`cmake/ConfigForgeSbom.cmake`,
    `CF_SBOM_OPTIONS`, which is the SPDX `comment` "Build options: ...")
    gets `profile=<name>` first and, last, `c-above-89=<list>`: the
    entries of D-106's list as `<file>=c99` or `<file>=c11`, space
    separated, or `c-above-89=none`. The SBOM is generated at the end
    of `CMakeLists.txt` (the `configforge_generate_sbom()` call), after every `cf_source_needs_c` call,
    which is why it and not the header carries the list. `gate.sbom`
    reads neither.
  * **`cf_source_needs_c(<file> <standard> REASON "<text>")`**, in
    `cmake/ConfigForgeCVersion.cmake` (new, included by
    `CMakeLists.txt` right after `include(ConfigForgeOptions)`).
    `<standard>` is `99` or `11`; anything else stops
    (`cf_source_needs_c: standard '<x>' is not 99 or 11`). `<file>` is
    relative to the calling directory or absolute and must exist
    (`cf_source_needs_c: <file> does not exist`). `REASON` is
    required and is stored in a line, so it may not contain `|` or a
    newline: each stops, with `cf_source_needs_c: REASON is required`
    and `cf_source_needs_c: a REASON may not contain '|' or a
    newline`. In order, the function:
    1. if `CF_C89_ONLY` is ON: `message(FATAL_ERROR "<file relative to
       the source tree> needs C<std> (<reason>), which profile
       <CF_BUILD_PROFILE_NAME> does not allow (CF_C89_ONLY=ON). Turn
       off the option that needs it, or choose another profile.")`;
    2. picks the flag by compiler, from `CMAKE_C_COMPILER_ID`:
       `GNU|Clang` (AppleClang included): `-std=c99` or `-std=c11`;
       `MSVC`: `/std:c11` for 11 when `MSVC_VERSION` is 1928 or more
       (else `FATAL_ERROR "<file> needs C11 and MSVC 19.28 (Visual
       Studio 2019 16.8) is the first with /std:c11"`), no flag for 99
       (MSVC compiles one C and has the C99 library parts since 2013);
       any other compiler (tcc, pcc): no flag;
    3. adds it to the file: `set_property(SOURCE <abs path> APPEND
       PROPERTY COMPILE_OPTIONS "<flag>")`; it comes after the target's
       `-std=c90`, so it wins, as the two `set_source_files_properties`
       did;
    4. prints, `message(WARNING ...)`: with a flag,
       `<file> is compiled as C<std> (<flag>), not C89: <reason>. Every
       other file stays C89.`; without one, `<file> needs C<std>, and
       <CMAKE_C_COMPILER_ID> is given no flag for it here: <reason>.`;
    5. appends one line `<file relative to the source tree>|c<std>|<reason>`
       to the global property `CF_C_ABOVE_89`
       (`set_property(GLOBAL APPEND_STRING PROPERTY CF_C_ABOVE_89
       "...\n")`), whether or not the compiler took a flag, so the
       list says what was declared.
    `cf_cversion_write()`, in the same file, writes that property to
    `${CMAKE_BINARY_DIR}/cf_cversion.txt` (through
    `configforge_write_lf`, in `ConfigForgeVersion.cmake`; a file of one
    blank line when nothing was declared, which that function's own
    final newline makes, so every reader of the file skips blank
    lines) and returns the SBOM text; it is
    called once, at the end of `CMakeLists.txt` before
    `configforge_add_gates`.
  * **The two files moved onto it, in four places.** In `CMakeLists.txt`,
    where A-1 left them (the root file is the directory that sets the
    property for the whole build, and the audit below needs no other):
    the `if(_CF_YAML_LIBFYAML AND NOT MSVC)` block calls
    `cf_source_needs_c(lib/serialize/src/yaml.c 11 REASON "libfyaml's
    header needs C11 (0.9.x's pre-C11 fallback for atomics does not
    compile)")` and the `if(CF_WITH_PREFERENCES AND APPLE)` block calls
    `cf_source_needs_c(lib/platform/src/posix/posix.c 99 REASON
    "CoreFoundation's headers use long long (SInt64)")`. The comments
    above them keep their history; the `set_source_files_properties`
    lines go. The standalone builds hold the same two copies by hand,
    `-std=c11` for `yaml.c` in `lib/serialize/CMakeLists.txt` and
    `-std=c99` for `posix.c` in `lib/platform/CMakeLists.txt`, each
    under `if(NOT CF_WHOLE_PROJECT ...)`. They move onto the helper too
    (`cf_source_needs_c(src/yaml.c 11 REASON ...)` and
    `cf_source_needs_c(src/posix/posix.c 99 REASON ...)`, the same
    reasons): both files include `cmake/ConfigForgeOptions.cmake` when
    they are the top of a build, which loads the profile, so each
    includes `cmake/ConfigForgeCVersion.cmake` on the next line and the
    helper and `CF_C89_ONLY` are there. A standalone build writes no
    `cf_cversion.txt` and registers no gate; it gets the flag, the
    warning and the `CF_C89_ONLY` refusal.
  * **`gate.cversion`** (`cmake/gates/GateCVersion.cmake`, new),
    registered in `cmake/ConfigForgeGates.cmake` after the `if(MSVC)
    return()`, with `-DSRC -DLIST=${CMAKE_BINARY_DIR}/cf_cversion.txt
    -DCOMPDB=${CMAKE_BINARY_DIR}/compile_commands.json
    -DC89_ONLY=$<BOOL:${CF_C89_ONLY}>`. The question it answers is "which
    files does the build compile above C89", and it asks the build, not
    the list: `CMakeLists.txt` gets `set(CMAKE_EXPORT_COMPILE_COMMANDS
    ON)` before the first target, so Makefile and Ninja generators write
    `compile_commands.json`, whose every `"command"` is what the compiler
    was given. The rule, with CMake 3.10 only (no `string(JSON)`):
    1. no `COMPDB` file: print `C version gate: no compile_commands.json
       (this generator writes none); skipped` and succeed;
    2. read `COMPDB` with `file(READ ...)` and `string(REGEX MATCHALL
       ...)` (not `file(STRINGS ...)`, which splits at a `;` and joins at
       a bracket, as GateAbi.cmake's own comment says): CMake writes each
       key on its own line, so pair each `  "command": "<text>",` with the
       next `  "file": "<path>"`; keep the entries whose file is under
       `${SRC}/lib/` and ends in `.c`; of the `-std=<v>` words in a
       command take the last (the target's `-std=c90` comes first);
    3. `<v>` is C89 when it is `c90`, `c89` or `iso9899:1990`; no
       `-std=` at all is C89 too (tcc and pcc get no flag); anything
       else, `gnu90` included (extensions are not C89), is above;
    4. read `LIST` (`<file>|<std>|<reason>`, blank lines skipped); every file above C89 must
       be on it with the same standard, and every line on it must be
       compiled with exactly that standard; a file is above in
       every entry it has (both libraries compile the same files);
    5. each reason has at least 12 characters; a file is on the list
       once; and with `C89_ONLY` ON the list is empty.
    The words, each printed as `VIOLATION: ...` before the one final
    `FATAL_ERROR "C version gate failed"`: `<file> is compiled with
    -std=<v> and is not declared; declare it with cf_source_needs_c(<file>
    <NN> REASON "...")`; `<file> is declared as <std> but compiled with
    -std=<v>`; `<file> is declared in cf_cversion.txt and is never
    compiled above C89`; `<file> has a reason of <n> characters; give at
    least 12`; `<file> is declared twice`; `the profile allows C89 only
    (CF_C89_ONLY=ON) and <file> is declared`. Success: `C version gate:
    clean, <n> file(s) above C89`. This catches the case the helper
    cannot, a `set_source_files_properties(... "-std=c99")` written by
    hand.
  * **`gate.profiles`** (`cmake/gates/GateProfiles.cmake`, new, in the
    part of `ConfigForgeGates.cmake` that runs on MSVC too, with
    `-DSRC`): (1) the files in `cmake/profiles/` are exactly the seven,
    `CF_PROFILES` of `ConfigForgeProfile.cmake`; (2) each follows the
    grammar, starts with `# `, and lists every option of
    `CF_PROFILE_OPTIONS` once, in order, with its type; (3)
    `default.cmake` equals each option's own default, scanned from the
    `option(<NAME> "..." ON|OFF)` and `set(<NAME> <v> CACHE ...)`
    declarations in `cmake/*.cmake`, `CMakeLists.txt`,
    `lib/*/CMakeLists.txt` and `lib/*/*.cmake` (the scan is
    `cf_option_defaults()` in `ConfigForgeProfile.cmake`, which A-12
    uses too; it reads the whole file, since nearly every declaration
    in ConfigForgeOptions.cmake is spread over several lines, and takes
    a quoted value without its quotes, as `CF_PLATFORM`'s `"auto"`);
    (4) a profile with `CF_FORCE_PORTABLE` ON has
    `CF_SINGLE_THREADED` ON (ConfigForgeOptions.cmake's `if(CF_FORCE_PORTABLE)`
    block refuses it otherwise); the four limits are inside their ranges; (5) with CMake
    3.19 or more, `CMakePresets.json` has a configure preset of the
    profile's own name whose `cacheVariables.CF_PROFILE` is that name
    (below 3.19 this part prints that it is skipped, as
    `GateSbom.cmake` does); (6) the README's size table is readable by
    the estimate's two patterns: eight rows at least and a `from <B> KB`.
    Words: `VIOLATION: profile <name>: <what>`; success `profile gate:
    clean, 7 profiles`.
  * **The presets.** Every configure preset sets `CF_PROFILE`:
    `default`, `sanitize`, `tsan`, `coverage`, `fuzz`, `analyze` and
    `portable` set `"CF_PROFILE": "default"` (the last still sets
    `CF_FORCE_PORTABLE` and `CF_SINGLE_THREADED` by hand, which the
    printout then lists as differing, as it should); `minimal` sets
    `"CF_PROFILE": "minimal"` and keeps `CMAKE_BUILD_TYPE` only: its
    `CF_GC_SECTIONS`, `CF_WITH_*` and limit lines move into the profile
    and leave the preset. Five presets are new, each with its build and
    test preset of the same name: `full` (Release), `server` (Release),
    `misra` (Debug), `embedded-json` and `embedded-cbor` (MinSizeRel;
    both also `CF_BUILD_TOOLS` OFF and `CF_BUILD_EXAMPLES` OFF, since
    Doctor and the examples need parts they leave out). The test presets
    of `embedded-json` and `embedded-cbor` carry `"filter": {"include":
    {"name": "^(profile|gate\\.(version|text|layering|detection|cversion|profiles))"}}`:
    the unit suite writes its documents as INI and cannot run without
    it. Their `displayName`s are the profile's first sentence.
  * **Message text is matched across a line break.** CMake wraps the
    text of a `message()` at about 78 columns (scripts/verify.cmake's
    `fyaml_configure` joins its wrapped lines for this reason), so every
    regular expression this card adds, the six cases below and the words
    the gate fixtures look for alike, writes the gap between two words as
    `[ \n]+` in the test's source, never one space (this card shows
    them with single spaces, for reading); or the text is written with
    `message(STATUS ...)`, which is not wrapped. The rule is for the
    new regular expressions: the cases that already exist in the tree
    (`tests/codec`, `tests/gen`, `tests/doctor` and the rest) stay as
    they are.
  * **`tests/profile/`** (new, `add_subdirectory(profile)` in
    `tests/CMakeLists.txt` before `conformance`): `test_profile.c`, C89, a
    program of `main(int argc, char** argv)` registered as
    `profile.<CF_BUILD_PROFILE_NAME>` with the profile's name as its
    argument. It checks: `cf_build_profile()` equals `argv[1]`; for each
    of the table `{"ini", CF_WITH_INI}, {"json", CF_WITH_JSON}, {"toml",
    ...}, {"properties"}, {"cbor"}, {"tlv"}, {"compose", CF_WITH_COMPOSE},
    {"xml"}, {"yaml"},
    {"sqlite"}, {"real"}, {"dir"}, {"dotenv"}, {"preferences"},
    {"emit"}, {"image"}, {"report"}` that `cf_has_feature(name)` is
    true exactly when the macro is 1 (names as the `cf_features` list in
    `lib/config/src/core/version.c` gives them); that `cf_build_limit("key")`, `"message"`, `"path"` and
    `"depth"` equal `CF_KEY_MAX`, `CF_MESSAGE_MAX`, `CF_PATH_MAX` and
    `CF_MAX_DEPTH`; and that a build with `CF_SINGLE_THREADED` defined
    has `cf_has_feature("single-threaded")`. It prints
    `profile <name>: <n> features, key <k>` and returns 0. It includes
    `configforge/cf_version.h` and `configforge/cf_build_options.h`
    only; it links `configforge`.
    `tests/profile/cversion/CMakeLists.txt` is a project of
    `project(cversion NONE)` driven by `-DCASE=<n>`, registered six
    times as `profile.cversion.<case>` with
    `ctest`'s `PASS_REGULAR_EXPRESSION` (the exit code is ignored for
    these); it includes `cmake/ConfigForgeCVersion.cmake`, creates an
    empty `a.c`, sets `CMAKE_SOURCE_DIR`-relative use correctly (the test
    project passes `-DCF_C_ROOT=<its dir>` and the function uses
    `CF_C_ROOT` when defined, else `CMAKE_SOURCE_DIR`, to make a file
    name relative), calls the function as the case says and prints
    `options: <the COMPILE_OPTIONS of a.c>`. The cases, each a
    `cmake -S ... -B ...` command with the `-D`s shown, and the
    pattern that must appear: **gnu**: `-DCMAKE_C_COMPILER_ID=GNU`,
    `cf_source_needs_c(a.c 11 REASON "libfyaml needs it here")`:
    `options: -std=c11` and `a.c is compiled as C11 \(-std=c11\), not
    C89: libfyaml needs it here`; **other**: `-DCMAKE_C_COMPILER_ID=TinyCC`:
    `a.c needs C11, and TinyCC is given no flag`; **msvc-old**:
    `-DCMAKE_C_COMPILER_ID=MSVC -DMSVC_VERSION=1920`: `MSVC 19.28`;
    **refused**: `-DCMAKE_C_COMPILER_ID=GNU -DCF_C89_ONLY=ON
    -DCF_BUILD_PROFILE_NAME=misra`: `a.c needs C11 \(libfyaml needs it
    here\), which profile misra does not allow`; **noreason**: `a.c 11`
    with no REASON: `REASON is required`; **badstd**: `a.c 98 REASON
    "long enough reason"`: `standard '98' is not 99 or 11`.
  * **The gate's own fixtures**, `tests/profile/data/`, run by
    `tests/profile/gate_cversion.cmake` (`cmake -P`, registered as
    `profile.gate_cversion`), which runs `GateCVersion.cmake` on each
    pair and compares the exit code and a word of the output:
    `ok_db.json` (two entries for `lib/serialize/src/yaml.c`, both
    `-std=c90 ... -std=c11`, one for `lib/serialize/src/json.c`
    `-std=c90`) with `ok_list.txt` (`lib/serialize/src/yaml.c|c11|libfyaml's
    header needs C11`): exit 0, `clean, 1 file`; `ok_db.json` with
    `empty_list.txt`: exit 1, `is not declared`; `ok_db.json` with
    `c99_list.txt`: `declared as c99 but compiled with -std=c11`;
    `ok_db.json` with `stale_list.txt` (adds `lib/serialize/src/json.c|c11|a
    reason that is long enough`): `is declared in cf_cversion.txt and is
    never compiled above C89`; `ok_list.txt` with a reason of `short`:
    `reason of 5 characters`; `ok_db.json` with `ok_list.txt` and
    `-DC89_ONLY=ON`: `the profile allows C89 only`; and `gnu_db.json`
    (`json.c` with `-std=gnu90`) with `empty_list.txt`: `compiled with
    -std=gnu90`. Each fixture gets a `-text` line in `.gitattributes`
    (the command lines are quoted, a converted line end would change
    them).
  * **CI.** One job, `profiles`, in `.github/workflows/ci.yml` after
    `minimal`: a matrix over `full`, `default`, `minimal`, `misra`,
    `embedded-json`, `embedded-cbor`, `server` on `ubuntu-26.04`, steps
    checkout (the same pinned action as the other jobs),
    `sudo apt-get update && sudo apt-get install -y libexpat1-dev
    libsqlite3-dev`, `cmake --preset ${{ matrix.profile }} && cmake
    --build --preset ${{ matrix.profile }}`, `ctest --preset ${{
    matrix.profile }}`. The existing `minimal` job stays (it is the
    preset the Cortex-M work later holds to numbers).
  * **`scripts/verify.cmake`** gets an EXTRA step `profiles`: for each of
    `full server misra embedded-json embedded-cbor` (the other two are
    `PRESETS`), configure, build and test its preset as the other
    steps do; `full` and `server` are skipped with a note when
    `expat.h` or `sqlite3.h` is not found (`find_path`). The header
    comment and the `unknown EXTRA step` message list it. The
    kept-build check (the loop in `verify.cmake` over
    `cmake/ConfigForgeOptions.cmake`, `CMakePresets.json` and
    `cf_build_options.h`) adds
    `cmake/ConfigForgeProfile.cmake`, `cmake/ConfigForgeCVersion.cmake`
    and every `cmake/profiles/*.cmake` to the files whose change
    starts from nothing.
* **files**: `cmake/profiles/{full,default,minimal,misra,embedded-json,embedded-cbor,server}.cmake`,
  `cmake/ConfigForgeProfile.cmake`, `cmake/ConfigForgeCVersion.cmake`,
  `cmake/ConfigForgeOptions.cmake`, `cmake/ConfigForgeSbom.cmake`,
  `cmake/ConfigForgeGates.cmake`, `cmake/gates/GateCVersion.cmake`,
  `cmake/gates/GateProfiles.cmake`,
  `cmake/templates/cf_build_options.h.in`,
  `lib/port/include/configforge/cf_build_options.h`,
  `lib/config/include/configforge/cf_version.h`,
  `lib/config/src/core/version.c`, `lib/config/src/diag/report.c`,
  `cmake/configforge.experimental.symbols`, `CMakeLists.txt`,
  `lib/serialize/CMakeLists.txt` and `lib/platform/CMakeLists.txt` (the
  standalone copies of the two flags), `CMakePresets.json`, `tests/CMakeLists.txt`, `tests/profile/**`,
  `.gitattributes`, `.github/workflows/ci.yml`, `scripts/verify.cmake`;
  last: `README.md`, `scripts/README.md`, `docs/Compliance.md`,
  `docs/API.md` (regenerated), `CHANGELOG.md`.
* **steps**:
  0. If `## 1.8.0 — unreleased` is not open, A-1 opened it; check.
  1. The tests first. `tests/profile/test_profile.c` and its
     `CMakeLists.txt`, the six `cversion` cases, the fixtures and
     `gate_cversion.cmake`, `GateProfiles.cmake` with no profile files
     yet; register all of it. `cmake --preset default && cmake --build
     --preset default` fails to link `test_profile` (no
     `cf_build_profile`): see that, and `ctest -R profile` fail.
  2. `ConfigForgeProfile.cmake` (`CF_PROFILES`, `CF_PROFILE_OPTIONS`,
     `cf_profile_parse`, `cf_option_defaults`, `cf_profile_load`,
     `cf_profile_report`) and the seven files from the table;
     `ConfigForgeOptions.cmake` calls `cf_profile_load()` first. The
     build stays red at `test_profile` until step 3, so this step
     builds `cmake --build --preset default --target configforge` only;
     `ctest -R "gate.profiles|profile.gate_cversion"` passes;
     `cmake --preset default 2>&1 | grep "^-- profile:"` shows the first
     three lines of the printout's form.
  3. `cf_build_profile`, the header macro, the report member and the
     experimental symbol: `cmake -DSRC=. -DWRITE=ON -P
     cmake/gates/GateAbi.cmake` for the list; `profile.default` passes.
  4. The presets: `minimal` moved onto its profile. Configure it in a
     fresh directory before this step's change and after it
     (`cmake --preset minimal`, then `cmake -L -N build-minimal`, the
     `CF_*` lines kept, `build-minimal` removed between): the two lists
     are equal but for `CF_PROFILE` (new), `CF_PROFILE_FILE` (new,
     FILEPATH), `CF_C89_ONLY` (new, ON), `CF_YAML_BACKEND` (`builtin`,
     was `auto`) and `CF_LANES_BACKENDS` (`c89`, was `all`).
     `CF_WITH_COMPOSE` is OFF in both: A-9's preset line has moved into
     the profile. `CF_PROFILE_APPLIED` is `INTERNAL`, which `cmake -L`
     does not list. The five new presets configure; `misra` builds and
     passes the suite.
  5. `ConfigForgeCVersion.cmake`, `cf_cversion_write`, the SBOM text;
     the two files moved onto the helper, in `CMakeLists.txt` and in the
     two standalone builds; `cversion` cases and the
     gate on its fixtures pass. `cmake -S lib/serialize -B build-s
     -DCF_YAML_BACKEND=libfyaml` (where libfyaml is installed) prints
     the same warning for `src/yaml.c`, and its `CF_BUILD_PROFILE_NAME`
     is `default`. `cmake --preset default -B build-lfy
     -DCF_YAML_BACKEND=libfyaml` (where libfyaml is installed) prints
     the C11 warning for `yaml.c`, writes `build-lfy/cf_cversion.txt`
     with one line, and its `gate.cversion` is clean; with
     `-DCF_PROFILE=minimal` added it stops with the refusal naming
     `lib/serialize/src/yaml.c`.
  6. Break each gate on purpose and see it fail, then take the break
     out: (a) in `CMakeLists.txt` add `set_source_files_properties(
     ${CMAKE_CURRENT_SOURCE_DIR}/lib/serialize/src/json.c PROPERTIES
     COMPILE_OPTIONS "-std=c99")`, reconfigure, `ctest -R
     gate.cversion` fails with `json.c is compiled with -std=c99 and is
     not declared`; (b) delete one line from `misra.cmake`, `ctest -R
     gate.profiles` fails with `profile misra:` naming the option; (c)
     change `CF_WITH_TOML` in `default.cmake` to OFF, same gate, names
     the default it differs from.
  7. CI job, `verify.cmake`'s step, the documents (README: a "Profiles"
     paragraph in "Build" - what a profile is, the seven names, that a
     `-D` wins and a build directory keeps its profile, `CF_PROFILE_FILE`
     - which A-12 replaces by its generated `### Profiles` subsection, so
     write it plainly and do not number or anchor it - the preset table's new rows, the `Options:` paragraph's new names
     `CF_PROFILE` `CF_PROFILE_FILE` `CF_C89_ONLY`, two rows of the
     Gates table: `cversion | no file is compiled above C89 that
     cf_source_needs_c does not declare` and `profiles | every profile
     file is complete, and default is the options' own defaults`; the
     `layering`-row text is A-1's; `docs/Compliance.md`'s "The MISRA
     profile" first paragraph says the profile is `cmake/profiles/misra.cmake`
     and `cmake --preset misra`, and its "C version" row names
     `cf_source_needs_c`); ACCEPT-LINUX, ACCEPT-WINDOWS.
* **accept**: ACCEPT-LINUX with
  `-DEXTRA="profiles;clang;compilers;yaml;abi;components;shared"` (the
  helper's flag choice is
  checked by the clang build and by tcc and pcc, which get no flag, the
  new presets by `profiles`, the libfyaml path of `yaml.c` through the
  helper by `yaml` - the only step besides `conformance` that builds
  libfyaml - the new exported function by `abi`, the standalone copies
  of the two flags by `components`, and `cf_build_profile` in the
  shared library by `shared`), ACCEPT-WINDOWS (the profile files
  and both gates' non-compiler parts run under MSVC; `gate.cversion`
  is not registered there), ACCEPT-CI with the `profiles` jobs and
  `macos-preferences` green. Particular to the card: the printout of
  step 2, and `cmake --preset minimal -DCF_WITH_REAL=ON` prints
  `profile: CF_WITH_REAL is ON, the profile says OFF`.
* **stop if**: a profile does not configure, build or pass its tests
  from the table: change nothing in the table; write which option needs
  which in the notes and stop (the likely one: a part a codec needs that
  `embedded-*` turn off). The unit tests of a profile
  that keeps the codecs (`misra`) fail only for a reason the table
  caused (single thread plus portable is the `portable` preset's
  combination, which passes): stop and report. `CMAKE_EXPORT_COMPILE_COMMANDS`
  is unavailable (a CMake without it: the minimum is 3.10, which has
  it): not a stop.
* **docs**: D-105 and D-106 (written by A-1) get "Built: A-11" with the
  commit; D-105's text gains one sentence: "`CF_PROFILE_FILE`'s file
  and the shipped ones are read as data, one `set(...)` a line, and
  the build directory keeps the profile it was configured with." CHANGELOG
  `### Added`: "Build profiles by name, `-DCF_PROFILE=<name>` (`full`,
  `default`, `minimal`, `misra`, `embedded-json`, `embedded-cbor`,
  `server`) or `-DCF_PROFILE_FILE=<file>`: the configure step says what
  was chosen and what differs, `cf_build_profile()` and the report's
  `library.build_profile` say which build it is, and CI builds each
  (D-105). `cf_source_needs_c` and `gate.cversion`: a file above C89 is
  declared with its reason, warned of at configure time, and refused by
  the profiles that allow C89 only (D-106)." and `### Changed`: "The
  `minimal` preset is the `minimal` profile; what it builds is
  unchanged." `docs/Plan.md`'s index: A-11 `done` with its commits (the
  rows of A-12 and the cards after it are already there).
* **Organiser's answers, 2026-11-22**:
  * `gate.cversion` scans the library's sources only (`lib/*/src`); the
    components' tests (`lib/*/tests`, e.g. `test_c11.c`, compiled as C11 on
    purpose) are outside it.
  * Step 2 writes the gates and sees them fail; `gate.profiles` and the C
    version fixture test are expected to pass only at the end of step 5,
    once the presets (step 4) and `CF_C89_ONLY`'s default (step 5) exist.
  * `cf_option_defaults` also scans `lib/serialize/backends.cmake`, where
    `CF_YAML_BACKEND` is declared.

* **notes**:
  * 2026-11-22: Steps 1–5 reached the profile and fixture checks
    (9/9 under MSVC). All three deliberate breaks in step 6 produced
    the expected violations and were removed. With Clang and Ninja,
    the clean `gate.cversion` still exits 8:
    `lib/config/tests/test_c11.c is compiled with -std=c11 and is not declared`.
    `lib/config/tests/CMakeLists.txt` deliberately builds this consumer
    test with `C_STANDARD 11`. The gate's specified `lib/` filter
    includes component tests, but the card calls for only the two
    library source declarations. Decide whether the gate should omit
    component tests or account for their standards separately.

---

## A-12 - The feature matrix made from the build

* **id**: A-12
* **goal**: the README's tables of profiles, of the options each
  profile sets, of what each part costs, of every option, and of each
  target and how it was shown are written by `cmake -P
  scripts/generate/matrix.cmake` from the profile files, the options'
  own declarations, `docs/sizes.txt` and `docs/targets.txt`; and
  `gate.matrix` fails when the README differs from what the script
  makes.
* **why**: the Roadmap's "A feature matrix generated from the build".
  The README's option and size tables are written by hand and have
  already fallen behind: `CF_WITH_JSON`, `CF_WITH_PROPERTIES`,
  `CF_WITH_CBOR`, `CF_WITH_TLV` and `CF_WITH_YAML` have no row in
  "Leaving parts out", and the README's options paragraph (the one that
  begins "Options:") names `CF_HARDEN` but not `CF_WERROR`. A table the
  build makes cannot do that.
* **needs**: A-11 (the profile files, `ConfigForgeProfile.cmake` and its
  parser, `cf_option_defaults`, the README size-table forms)
* **size**: medium
* **decided**:
  * **Four inputs, one output.** The script reads: (1) `cmake/profiles/*.cmake`
    through A-11's `cf_profile_parse`; (2) the options' declarations
    through A-11's `cf_option_defaults` and their help strings;
    (3) `docs/sizes.txt`, written by a measuring script (below);
    (4) `docs/targets.txt`, written by hand and checked. It reads
    `.github/workflows/ci.yml` only to check `targets.txt`. It writes
    README.md, replacing what lies between marker comments. It
    is `cmake_minimum_required(VERSION 3.10)` like the other scripts and
    uses no `string(JSON)`. Its functions (`cf_targets_read()` and the
    rest) are defined first and its work done last, inside
    `if(NOT MATRIX_LIB)`: a test that passes `-DMATRIX_LIB=ON` and
    `include()`s the script gets the functions and runs nothing (CMake
    3.10 has no variable that says a script is being included, so the
    switch is explicit).
  * **Where the generated parts sit.** Each is between a pair of lines
    `<!-- matrix:begin <name> -->` and `<!-- matrix:end <name> -->`
    (HTML comments: the repository page and the docs site both hide
    them). Five names, each once, in the README's order:
    * `targets`: a new `### Targets` subsection at the end of
      "## Status", after the paragraph "Every line marked done ...",
      with one lead sentence outside the markers:
      "Where the library has run, and how it was shown. A target moves
      up as a device or an emulator run becomes available
      (`docs/targets.txt`).";
    * `profiles` and `features`: a new `### Profiles` subsection in
      "## Build", after the paragraph that begins "`minimal` also drops
      every function" and before the options paragraph. It replaces
      the "Profiles" paragraph A-11 wrote there by hand (A-11's step 7
      says so): that paragraph is removed in this card's first
      generation, and its facts (a `-D` wins; a build directory keeps
      its profile) are in the lead sentence below. Its lead sentence
      outside the markers: "A profile is a
      named set of options (`-DCF_PROFILE=<name>`); one given on the
      command line still wins, and a build directory keeps the profile
      it was configured with. `cmake/profiles/` has the files, and
      `-DCF_PROFILE_FILE=<file>` reads one of your own." The two tables
      are the first `profiles`, the second `features`, with a sentence
      outside the markers between them: "What each profile sets:";
    * `options`: replaces the paragraph that begins "Options:", to its
      closing "below.";
    * `parts`: replaces the table of "Leaving parts out" and the first
      sentence of the paragraph after it, "All of them together take the
      library from 92 KB to 58 KB."; the rest of that paragraph stays by
      hand.
    A missing, duplicated, unpaired or unknown marker is an error of the
    script (`README.md: marker 'x' ...`), never a silent skip.
  * **The tables, exactly.**
    * `profiles`: header `| Profile | Library code | What it is for |`
      and `| ------- | -----------: | -------------- |`; one row per
      profile in A-11's order (`CF_PROFILES`): `` | `<name>` | <N.N> KB
      | <the profile file's first comment, without "# "> | ``, `-` for
      the size when `sizes.txt` has no `profile|<name>` line. KB is 1000
      bytes, one decimal, rounded half up, computed with `math(EXPR)` on
      tenths.
    * `features`: header `| Option | full | default | minimal | misra |
      embedded-json | embedded-cbor | server |` (the profile names in
      `CF_PROFILES` order), one row per option of `CF_PROFILE_OPTIONS`:
      `` | `<OPTION>` | <cell> ... | ``, each cell the value in the
      profile's file, `ON` and `OFF` written as `on` and `off`, any other
      value as it is; a profile that does not set the option (a file of
      one's own, never a shipped one) is not shown.
    * `options`: `Options:` then every option the build declares, one
      per word in backticks, separated by one space, alphabetical,
      wrapped greedily at 72 columns, ending with a full stop: every
      `option(CF_...)` and every `set(CF_... <v> CACHE STRING|BOOL|FILEPATH|PATH ...)`
      that `cf_option_defaults` finds in `cmake/*.cmake`,
      `CMakeLists.txt`, `lib/*/CMakeLists.txt` and `lib/*/*.cmake`
      (a name that starts `_CF_` or is `CACHE INTERNAL` is not an
      option), **less the five limits** (`CF_KEY_MAX`,
      `CF_MESSAGE_MAX`, `CF_PATH_MAX`, `CF_MAX_DEPTH` and
      `CF_STACK_BUDGET`: the five of the range list in
      `ConfigForgeOptions.cmake` that begins `CF_KEY_MAX:32:65535`),
      which are numbers and not switches. The block's last line is
      `The limits are in the table below.`
    * `parts`: header `` | Option | What goes | Code it saves (<arch>, `-Os`) | ``
      with `<arch>` the second field of the `tool` line of `sizes.txt`;
      one row per `off|` line of `sizes.txt`, the largest saving first,
      ties by option name: `` | `<OPTION>` | <the option's help
      string> | <N.N> KB | ``, `-` when the line says `-`. Then a
      blank line and `Leaving out TOML, EMIT, REAL, REPORT, IMAGE, INI,
      DOTENV and DIR together takes the library from <B> KB to <A> KB.`
      with `<B>` the `base` line and `<A>` the `all` line (the `all`
      build turns exactly those eight off, as the old table did), each
      rounded to a whole KB. The words "from <B> KB" are the form A-11's
      estimate reads, and `gate.profiles` holds it; "All of them
      together" is not used, since the table now has thirteen rows and
      the `all` build only eight.
    * `targets`: header `| Target | Shown by | Where | Note |`, one row
      per `targets.txt` line in file order: the target; `Shown by` is
      `hardware (<board>, <date>, <commit>)`, `CI runner` (`runner`
      with a job), `local run` (`runner` with no job), `qemu`,
      `built only` or `not tried`; `Where` is `` job `<job>` `` or `-`;
      the note or `-`.
  * **This card widens the scan A-11 wrote, and adds two things to
    `ConfigForgeProfile.cmake`.** A-11's `cf_option_defaults` scans
    `cmake/*.cmake`, `lib/serialize/CMakeLists.txt` and `lib/lanes/*`
    and gives only the defaults. That misses `CF_BUILD_TOOLS` and
    `CF_BUILD_BENCH` (in `CMakeLists.txt`) and `CF_YAML_BACKEND` (in
    `lib/serialize/backends.cmake`), so this card gives it the four
    patterns of the `options` block above and says so in A-11's text.
    A-11's `gate.profiles` calls the same function and follows; it still
    compares only the options of `CF_PROFILE_OPTIONS`, so the wider
    scan changes nothing it checks. A-12 also adds
    `cf_option_help(<NAME> <out>)` beside `cf_option_defaults` (the
    help string of a declaration, found by the same scan) and the list
    `CF_SIZE_PARTS`, defined there as an explicit list, the thirteen
    parts the off-builds turn off, in the order of the `parts` rows
    before sorting: `CF_WITH_INI`, `CF_WITH_JSON`, `CF_WITH_TOML`,
    `CF_WITH_PROPERTIES`, `CF_WITH_CBOR`, `CF_WITH_TLV`, `CF_WITH_YAML`,
    `CF_WITH_DIR`, `CF_WITH_DOTENV`, `CF_WITH_EMIT`, `CF_WITH_IMAGE`,
    `CF_WITH_REPORT`, `CF_WITH_REAL`.
  * **The options' help strings are the README's words.** The `parts`
    table reads each option's help string, so the strings of the thirteen
    measured options in `cmake/ConfigForgeOptions.cmake` become the
    README's text, exactly: `CF_WITH_INI` "the INI codec"; `CF_WITH_JSON`
    "the JSON and JSON5 codec, which Report Replay and Doctor read";
    `CF_WITH_TOML` "the TOML codec"; `CF_WITH_PROPERTIES` "the Java
    .properties reader"; `CF_WITH_CBOR` "the CBOR codec"; `CF_WITH_TLV`
    "the TLV codecs made from a layout"; `CF_WITH_YAML` "the YAML codec";
    `CF_WITH_DIR` "the drop-in directory source"; `CF_WITH_DOTENV` "the
    .env source"; `CF_WITH_EMIT` "cf_emit and every encoder";
    `CF_WITH_IMAGE` "the binary snapshot image"; `CF_WITH_REPORT` "the
    provenance report, Report Replay, the JSON Schema writer - and
    Doctor"; `CF_WITH_REAL` "floating point and its exact formatter".
    (They stay one-line strings; clang-format and cmake-format keep
    their wrapping.) The option and its description are then written
    once.
  * **Sizes: the existing measurement is by hand.** `cmake/ConfigForgeSize.cmake`
    is not a measurement: it holds `CF_GC_SECTIONS` and `CF_LTO`, and
    the README's figures (8.3 KB and the rest, "92 KB to 58 KB") have
    no script that makes them. This card adds one:
    `scripts/check/sizes.cmake`, run by `cmake -DWRITE=ON -P
    scripts/check/sizes.cmake` (writes `docs/sizes.txt`, or the file
    `-DOUT=<file>` names) or `-DCHECK=ON` (compares with `docs/sizes.txt`,
    or with `-DFILE=<file>`; with `-DMEASURED=<file>` it reads that
    file's figures instead of building, which is how its comparison is
    tested without a compiler). With `-DWORK=<dir>` for its builds (default
    `build-sizes/`) it builds, for GCC or Clang with `size` from
    binutils on the path, only the target `configforge` (`cmake -S . -B
    <dir>/<n> -DCMAKE_BUILD_TYPE=MinSizeRel -DCF_BUILD_TESTING=OFF
    -DCF_BUILD_EXAMPLES=OFF -DCF_BUILD_TOOLS=OFF -DCF_ENABLE_GATES=OFF
    ...` then `--target configforge`), and takes the `text` column of
    the `(TOTALS)` row of `size -t libconfigforge.a` (code and read-only
    data). The builds: `base`, the `default` profile; one `off|<OPTION>`
    per option of `CF_SIZE_PARTS` - `CF_WITH_INI`, `CF_WITH_JSON`,
    `CF_WITH_TOML`, `CF_WITH_PROPERTIES`, `CF_WITH_CBOR`, `CF_WITH_TLV`,
    `CF_WITH_YAML`, `CF_WITH_DIR`, `CF_WITH_DOTENV`, `CF_WITH_EMIT`,
    `CF_WITH_IMAGE`, `CF_WITH_REPORT`, `CF_WITH_REAL` - the saving being
    `base` minus the size with that one option OFF (a part that does not
    configure or build alone is `-`); `all`, with the eight parts the
    README's table had (`TOML EMIT REAL REPORT IMAGE INI DOTENV DIR`)
    all OFF; and `profile|<name>` for each of the seven (`-` where it
    does not configure, for want of expat or sqlite3). `docs/sizes.txt`
    is lines `kind|name|value`, in this order, with `#` comment lines on
    top saying how it was made:

    ```text
    # Written by scripts/check/sizes.cmake -DWRITE=ON; do not edit.
    tool|x86-64|GNU 15.2.0|MinSizeRel|<commit>|<date>
    base|default|<bytes>
    off|CF_WITH_INI|<bytes>
    ...
    all|parts|<bytes>
    profile|full|<bytes>
    ...
    ```

    (the `tool` fields: architecture from `CMAKE_SYSTEM_PROCESSOR`,
    compiler id and version, build type, `git rev-parse --short=7 HEAD`,
    the date). The old numbers in the README are replaced by the
    measured ones, and the CHANGELOG says so. `-DCHECK=ON` measures again
    and fails where a figure differs from the file by more than 3%, but
    only when the compiler id and its major version are those of the
    `tool` line; otherwise it prints `sizes: recorded with <x>, this is
    <y>; not compared` and succeeds. **A CI job `sizes`** (below) runs
    `-DCHECK=ON` against the committed `docs/sizes.txt`, and also
    `-DWRITE=ON -DOUT=$RUNNER_TEMP/sizes.txt`, uploading that file as an
    artifact; the file in the repository is the one a person takes
    from that artifact (`gh run download`), so that it carries the
    runner's compiler. The job never writes into the checkout, so its
    check is never of a file against itself. It is not part of
    `gate.matrix`: a measurement depends on the compiler, a gate must
    not.
  * **Per-target status: `docs/targets.txt`**, kept by hand, one target
    a line, `target|how|job-or-board|date|commit|note`, six `|`-separated
    fields (empty ones left empty), `#` lines and blank lines allowed.
    `how` is one of `hardware`, `runner`, `qemu`, `built`, `none`. The
    Roadmap's four classes are tested on hardware, tested under qemu,
    built only and not tried; `runner` is added for what a hosted or
    local machine is, a native run that is neither a named board nor an
    emulator, so that "tested natively in CI" is not written as
    hardware. The rules the check holds, and its words
    (`targets.txt:<line>: ...`): `how` is one of the five; a `qemu` or
    `built` line, and a `runner` line that names one, name a job that
    exists in `.github/workflows/ci.yml` as `^  <job>:` under `jobs:`; a
    `runner` line with no job needs a note (it says where it runs, as
    `run by scripts/verify.cmake -DEXTRA=wasm, not by CI`); a `hardware`
    line has a board, a date `YYYY-MM-DD` and a commit of at least 7
    hexadecimal digits; a `none` line names no job, board, date or
    commit; target names are unique. First content (each checked
    against `ci.yml` today): `x86-64 Linux, glibc|runner|gates`;
    `x86-64 Linux, musl (Alpine)|runner|alpine`; `x86-64 Windows,
    MSVC|runner|matrix`; `arm64 macOS, Apple clang|runner|matrix` (the
    `macos-latest` runner is arm64: the Roadmap says CI runs arm64
    there); `x86-64 Linux, tcc and pcc|runner|compilers`;
    `wasm32-wasi (Node's WASI)|runner|` with no job and the note `run by
    scripts/verify.cmake -DEXTRA=wasm, not by CI` (the unit tests run
    under Node there, so it is not "not tried"); then, as `none` with no
    job: `armv7 Linux`, `Cortex-M`, `s390x Linux (big-endian)` and
    `riscv64 Linux`, each with the note `planned (docs/Roadmap.md)`. A card that builds or runs a
    target changes its line in its own commit; the script prints the
    table from the file. `.gitattributes` is not touched: it has
    `* text=auto eol=lf`, and generated text needs nothing more.
  * **`gate.matrix`**: `cmake/gates/GateMatrix.cmake` (new), registered
    in `cmake/ConfigForgeGates.cmake` among the gates that run on MSVC
    too (the script is CMake and reads text), `-DSRC=${CMAKE_SOURCE_DIR}
    -DBIN=${CMAKE_BINARY_DIR}`. It runs `matrix.cmake -DOUT=${BIN}/README.matrix.md`
    (the script's default `OUT` is `README.md` itself), reads both with
    `\r` removed and compares as `GateApiref.cmake` does. On a
    difference it prints `VIOLATION: README.md does not match the
    build; run `cmake -P scripts/generate/matrix.cmake` and commit the
    result` and the first differing line pair (`README.md:<n>: has
    "<a>", the build makes "<b>"`), then `FATAL_ERROR "matrix gate
    failed"`; a check error of `targets.txt` or a marker fails the same
    way with its own words. Success: `matrix gate: clean (5 blocks,
    <n> targets)`. It does not read `sizes.txt`'s age or measure
    anything. The gate's words and the tests' patterns are matched with
    `[ \n]+` between words, as A-11 says of its own: `message()` wraps.
  * **Where the script is run for the person.** `scripts/format.cmake`
    runs `matrix.cmake` once, at its end, outside the `if(c_files)`
    block (where `apiref` is, and which a change of profiles or options
    alone never enters) and only when `CHECK` is off; it writes only when
    the result differs. `scripts/prestage.cmake` mentions it where it mentions
    `apiref`; `scripts/README.md` gets two rows, `generate/matrix.cmake`
    and `check/sizes.cmake`, and `sizes` is added to the `EXTRA` list
    in its `verify.cmake` row.
  * **`scripts/verify.cmake`**: an EXTRA step `sizes` runs
    `-DCHECK=ON` (skipped with a note without `size`), listed in the
    header comment and in the `unknown EXTRA step` message.
  * **CI**: one job `sizes` in `ci.yml` after `profiles`, `ubuntu-26.04`,
    `timeout-minutes: 45`. A run of the script is 22 library builds (1
    `base`, 13 `off`, 1 `all`, 7 `profile`), each a configure and the one
    target `configforge`, and the job runs the script twice (the check
    and the write), so 44 builds; 45 minutes is a guess of about a
    minute a build, not a measurement, and step 7 writes the real time
    in the notes. It runs only when
    the build or its options changed: checkout with `fetch-depth: 0`,
    then a first step `changed`, written inline in the job (a few lines
    of `git diff --name-only`; it does not wait for P-1b's
    `scripts/bench/changed.py`), that lists the files changed against the
    merge base with `origin/main` (a pull request) or against
    `github.event.before` (a push, when that is a real commit), sets
    `run=true` in `$GITHUB_OUTPUT` when any is `CMakeLists.txt`, under
    `cmake/`, `lib/`, `scripts/check/sizes.cmake` or `docs/sizes.txt`,
    and sets it `true` too when the listing fails; every later step is
    `if: steps.changed.outputs.run == 'true'`. The steps: `sudo apt-get
    update && sudo apt-get install -y binutils libexpat1-dev
    libsqlite3-dev` (the `full` and `server` profiles need expat and
    sqlite3 to configure), `cmake -DCHECK=ON -P
    scripts/check/sizes.cmake` (against the committed `docs/sizes.txt`),
    `cmake -DWRITE=ON -DOUT=$RUNNER_TEMP/sizes.txt -P
    scripts/check/sizes.cmake`, and an upload of that file with
    `actions/upload-artifact`, pinned the way the other actions are (the
    `libfuzzer` job, ci.yml, is the one that uses it; if none does, stop:
    pinning is the maintainer's). `gate.matrix` runs in the existing
    `gates` job with everything else.
* **files**: `scripts/generate/matrix.cmake` (new),
  `scripts/check/sizes.cmake` (new), `cmake/gates/GateMatrix.cmake`
  (new), `docs/targets.txt` (new), `docs/sizes.txt` (new, written by the
  script), `cmake/ConfigForgeGates.cmake`, `cmake/ConfigForgeOptions.cmake`
  (the help strings), `cmake/ConfigForgeProfile.cmake` (this card adds
  the list `CF_SIZE_PARTS` and the function
  `cf_option_help(<NAME> <out>)` beside `cf_option_defaults`, and
  widens that function's scan; see "This card widens the scan"),
  `scripts/format.cmake`, `scripts/prestage.cmake`,
  `scripts/verify.cmake`, `.github/workflows/ci.yml`; tests:
  `tests/CMakeLists.txt` (`add_subdirectory(matrix)`),
  `tests/matrix/CMakeLists.txt` (new: registers `matrix.script`,
  `matrix.markers`, `matrix.tables` and `matrix.sizes`, each a
  `cmake -P` run with `-DMATRIX_LIB=ON` where it includes the script),
  `tests/matrix/matrix.cmake`, `tests/matrix/sizes.cmake` and their
  fixtures, below; `docs/Dependencies.md` (`gate.dependencies` reads
  `scripts/*.cmake` for every `find_program`, and `sizes.cmake` and the
  verify step look for `size`, which the page does not name: add it to
  the GNU binutils row, beside `nm` and `objdump`); last:
  `README.md` (markers and the first generation; and one row,
  `matrix`, in its "Gates" table), `docs/Architecture.md` (one row,
  `gate.matrix`, in its table of gates), `scripts/README.md`,
  `docs/Roadmap.md`, `CHANGELOG.md`. `docs/Plan.md` is not changed.
* **steps**:
  1. `docs/targets.txt` and the check of it inside
     `scripts/generate/matrix.cmake` (`cf_targets_read()`), and
     `tests/matrix/matrix.cmake`, registered as `matrix.script`, a
     `cmake -P` test over fixtures in `tests/matrix/data/`: `targets_ok.txt`
     reads; and each of these lines, in a file of its own with
     `ci.yml`'s job names replaced by `tests/matrix/data/ci_jobs.txt`
     (`gates`, `matrix`), fails with the words shown: `x|bogus|||||`
     (`how 'bogus' is not one of hardware, runner, qemu, built, none`);
     `x|runner|nosuchjob||||` (`job 'nosuchjob' is not in ci.yml`);
     `x|hardware|board|2026-13-01|abc||` is accepted by the regex but
     `x|hardware|board|11-12-2026|abcdef1||` is not (`a hardware line
     needs a board, a date YYYY-MM-DD and a commit`); `x|none|gates||||`
     (`a 'none' line names no job`); a name twice (`target 'x' is
     there twice`); and `x|runner|||||` (`a runner line with no job
     needs a note`) while `x|runner||||| run by verify` reads. See it
     fail (no script).
  2. The script's `README.md` handling with a fixture README of two
     markers: replacement between markers; a missing, a duplicate, an
     unpaired and an unknown marker each an error with the words above;
     `matrix.markers` test, fails first.
  3. The markers in the README: the five places above, the lead
     sentences, the old table, the old paragraph and A-11's "Profiles"
     paragraph removed, each pair of markers empty for now.
  4. The generators of the five blocks from the profile files and the
     options; run on the real README (the markers are there):
     `cmake -P scripts/generate/matrix.cmake` leaves `git diff README.md`
     showing only the generated blocks changing. Write `tests/matrix/data/`
     expectations for a tiny profile directory (`two profiles, three
     options`) and check `profiles`, `features` and `options` against
     them byte for byte: `matrix.tables`.
  5. `matrix.sizes` first: `tests/matrix/sizes.cmake` runs
     `sizes.cmake -DCHECK=ON -DFILE=<a> -DMEASURED=<b>` over fixtures: a
     figure 10% off fails and names the option and both numbers; one 2%
     off passes; a different compiler id on the `tool` line prints
     `not compared` and passes. See it fail (no script). Then
     `scripts/check/sizes.cmake`; on a machine with `size`, `cmake
     -DWRITE=ON -DWORK=$HOME/cfsizes -P scripts/check/sizes.cmake`
     writes `docs/sizes.txt` (22 library builds); `-DCHECK=ON`
     right after passes. Run `profiles`'s generator: the sizes appear.
     Then the help strings of the thirteen options and the first
     generation. The old figures are now the measured ones; the
     README's prose that quotes them ("a third of a small program's
     size", "92 KB to 58 KB" in `docs/` pages if any: `git grep -n "92
     KB"`) is read and corrected by hand in this commit.
  6. `GateMatrix.cmake`, registered. Break it on purpose and see it
     fail: change `CF_KEY_MAX` in `minimal.cmake` to 65 without
     regenerating (`gate.matrix` names `README.md:<n>`), then run
     the script and see it pass, then put 64 back and regenerate;
     add `option(CF_X "x" OFF)` to `ConfigForgeOptions.cmake`: the gate
     fails for the `options` block, and passes after regenerating;
     take it out.
  7. `format.cmake`, `prestage.cmake`, `verify.cmake` (the `sizes`
     step), `ci.yml` (the `sizes` job), `docs/Dependencies.md`, the
     documents; run the `sizes`
     job once on a branch (it runs, since `ci.yml` and `cmake/` are among
     what it watches), download its artifact, compare it to the
     file in the tree and commit the artifact's version when they
     differ by more than the 3% (the figures then carry the runner's
     compiler); ACCEPT.
* **accept**: ACCEPT-LINUX with `-DEXTRA="sizes;profiles"` (the matrix
  reads every profile; the sizes step needs `size`), ACCEPT-WINDOWS
  (the script and `gate.matrix` are CMake text work: they run under
  MSVC; `sizes` is skipped there), ACCEPT-CI with the `sizes` job green
  (or skipped by its own `changed` step) and its artifact the committed
  `docs/sizes.txt`. Particular to the
  card: `cmake -P scripts/generate/matrix.cmake && git diff --exit-code
  README.md` exits 0 (the script is idempotent), and `ctest -R matrix`
  runs `gate.matrix`, `matrix.script`, `matrix.markers` and
  `matrix.tables`, all passing.
* **stop if**: more than three of the thirteen `off|` builds do not
  configure or build alone (the options depend on one another more than
  Options.cmake says): write which in the notes and stop; one or two
  are `-` in the table and the note says which. A-11's `cf_option_defaults`
  and this card's `cf_option_help` read whole files, since almost every
  declaration in ConfigForgeOptions.cmake spans lines (that is certain,
  not a possible stop): a declaration of a form the pattern still misses
  is a pattern to extend, said so in the notes; never hand-list the
  option. `ci.yml` has no
  `actions/upload-artifact` use to copy the pin from: leave the upload
  step out, run the `-DWRITE=ON -DOUT=$RUNNER_TEMP/sizes.txt` step in the
  job, and say so in the notes.
* **docs**: no new D-number. CHANGELOG `### Added`: "The README's
  tables of profiles, of the options each profile sets, of what each part
  costs, of every option and of each target's status are made by
  `scripts/generate/matrix.cmake` from the build's own files, and
  `gate.matrix` fails when they differ; `docs/targets.txt` records how
  each target was shown (hardware, a CI runner, qemu, built only, not
  tried); `scripts/check/sizes.cmake` measures what each part costs." and
  `### Changed`: "The size table in the README is measured by
  `scripts/check/sizes.cmake` (GCC, x86-64, `-Os`) and has a row for
  every codec; its figures differ from the hand-written ones." The
  Roadmap's item "A feature matrix generated from the build" is marked
  done with the commit; the item "Arm supported well" keeps its text and
  gains one sentence: "Its per-target list is `docs/targets.txt`."
* **Organiser's answers, 2026-11-22**:
  * Until CI supplies the first `docs/sizes.txt`, show `-` in the parts
    table's size cells and one line saying CI's `sizes` job measures them.
    The manager commits the runner's artifact and regenerates the README.
  * The fixtures of step 1 have six fields (five `|`), as the format says:
    `x|bogus||||`; `x|runner|nosuchjob|||`; `x|hardware|board|2026-13-01|abcdef1|`
    is accepted (the regex checks the date's shape, not the calendar) while
    `x|hardware|board|11-12-2026|abcdef1|` is refused; `x|none|gates|||`;
    `x|runner||||` is refused and `x|runner||||run by verify` reads.

* **notes**:
  * 2026-11-22: Steps 1–3 are in the worktree. Step 4 requires a first
    generation on the real README, including the parts block, but that
    block needs the architecture and the base and all measurements from
    `docs/sizes.txt`, first written in step 5. The first generation
    needs an order or output decision from the organiser.
    `gate.profiles` fails in all three MSVC configurations while the
    parts markers are empty. With the listed minimal flags,
    `unit.test_compose` also fails on a truncated diagnostic.

---

## A-13 - A page of the project's limits

* **id**: A-13
* **goal**: `docs/Limits.md` states in one place what the library does
  not do and what it will not take, its numbers are written by a
  script or compared with the build by a test so the page cannot
  drift, and it is in the site's navigation.
* **why**: the Roadmap's "A page of the project's limits". A user
  should learn a limit before depending on it, not from a refusal. Two
  statements of limits already disagree with the code (below): a page
  nothing checks goes the same way.
* **needs**: A-12 (the platforms, tested and only built; A-11 and A-1b
  come before it), W5-V2 (the table section 6 links to), P-1 (the sizes
  the readers were run at), P-1b (the order only: nothing of it is
  read).
* **size**: medium
* **decided**:
  * **The page, section by section, and who keeps each.** G is
    generated into the page by `scripts/generate/limits.cmake` and
    compared by `gate.limits`; T is held by a test; H is written by
    hand and has only references the gate checks.

    | # | Section (heading) | Holds | Kept by |
    | - | ----------------- | ----- | ------- |
    | 1 | `## What this page is` | one paragraph; the three kinds of line | H |
    | 2 | `## Build limits` | `CF_KEY_MAX`, `CF_MESSAGE_MAX`, `CF_PATH_MAX`, `CF_MAX_DEPTH`, `CF_STACK_BUDGET`: default, range, what each bounds | G |
    | 3 | `## Fixed limits` | the constants that are not build options (below) | G |
    | 4 | `## The decode budget and the depth limit` | D-82's three constants and the arithmetic; YAML's alias bounds; the depth rule | G, T |
    | 5 | `## The largest document` | per format, the limit in the code and the size it was run at | G, H |
    | 6 | `## What a format cannot hold` | by reference to W5-V2's table | H |
    | 7 | `## Platforms` | each target and how it was shown | G |
    | 8 | `## C versions` | each file above C89 and why | G |
    | 9 | `## Threads` | D-14's rules and the build options | H |
    | 10 | `## Not proven, not claimed` | by reference | H |
    | 11 | `## Not offered` | features not built, each with its D-number | H |

  * **Section 2.** The generator reads `cmake/ConfigForgeOptions.cmake`:
    the default of each `set(CF_KEY_MAX 256 CACHE STRING ...)` (the five
    declarations under "Limits and the stack budget") and the range of
    each from that file's range list, the entries `CF_KEY_MAX:32:65535`,
    `CF_MESSAGE_MAX:64:65535`, `CF_PATH_MAX:64:65535`,
    `CF_MAX_DEPTH:4:1024` and `CF_STACK_BUDGET:0:1048576` of the
    `foreach(lim ...)` that follows them (not the later
    `foreach(lim CF_KEY_MAX CF_MESSAGE_MAX ...)` lists of names). It
    also reads the defaults in
    `lib/port/include/configforge/cf_build_options.h` (the four
    `#ifndef` blocks, one each for `CF_KEY_MAX`, `CF_MESSAGE_MAX`,
    `CF_PATH_MAX` and `CF_MAX_DEPTH`) and the
    ranges in the `#error` lines of `lib/port/include/configforge/cf_port.h`
    ("The limits, checked where
    every translation unit sees them") and **fails, naming
    both files, when the three sources disagree**: the CMake default,
    the header's `#ifndef` default and the `#error` range are one
    fact written three times. The words of the "bounds" column are a
    table in the generator, taken from the README's table of "Limits
    and the stack budget"; the numbers never are. `CF_STACK_BUDGET`'s
    range (0 to 1048576) is in the CMake list only, and is shown from
    there.
  * **Section 3, the fixed limits.** Each row: name, value, the file it
    is read from (the file, never a line: lines move), what it bounds.
    Paths as A-1's map made them:

    | Name | Value | Read from | Bounds |
    | ---- | ----: | --------- | ------ |
    | `CF_SOURCE_MAX` | 64 | `lib/base/include/configforge/cf_error.h` | a source's name in an error; part of the ABI (D-45) |
    | `CF_MAX_SOURCES` | 32 | `lib/config/include/configforge/cf_explain.h` | the sources one `explain()` lists |
    | `CF_MAX_OPEN_PREFIXES` | 8 | `lib/config/include/configforge/cf_define.h` | open key prefixes a definition set holds |
    | `CF_EXPAND_MAX` | 1 MiB | `cf_define.h` | the bytes one value's `${...}` may expand to (D-77) |
    | `CF_EXPAND_TOTAL_MAX` | 16 MiB | `cf_define.h` | the expansion of a whole configuration |
    | `CF_DIR_MAX_FILES` | 256 | `lib/config/src/bridge/dir.c` | files in a drop-in directory |
    | `CF_SECRETS_MAX_FILES` | 256 | `lib/config/src/bridge/secrets.c` | files in a secrets directory |
    | `YP_KEY_MAX` | 1024 | `lib/serialize/src/yamlparse.c` | an implicit YAML key (spec 7.4.2) |
    | `CF_XML_INPUT_MAX` | 0x7FFFFFFF | `lib/serialize/src/xml.c` (new macro, see step 3) | the bytes of one XML document |

    `CF_XML_INPUT_MAX` is a macro in `xml.c` (step 3 makes it), and
    the generator reads it there, like the others.
    `CF_PLATFORM_NAME_MAX` is `CF_PATH_MAX` (`cf_platform.h`, its
    `#define CF_PLATFORM_NAME_MAX CF_PATH_MAX`) and is
    one row's note, not a row. The values are not all numbers, so each
    row in the generator carries the raw text it expects after
    `#define NAME ` (a trailing `/* ... */` comment cut off), read with
    one regular expression a row, and **fails naming the row** when the
    `#define` is not found (`CF_X: no #define in <file>`) or its raw
    text is not the expected one (`CF_X is now '<raw>', not '<expected>':
    change the generator's row`), so a renamed, removed or reworded
    constant breaks the gate and not the page. The expected raw text,
    section 3 and 4 together: `CF_SOURCE_MAX` `64`; `CF_MAX_SOURCES`
    `32`; `CF_MAX_OPEN_PREFIXES` `8`; `CF_EXPAND_MAX` `1048576ul`;
    `CF_EXPAND_TOTAL_MAX` `(16ul * CF_EXPAND_MAX)`; `CF_DIR_MAX_FILES`
    and `CF_SECRETS_MAX_FILES` `256`; `YP_KEY_MAX` `1024`;
    `CF_XML_INPUT_MAX` `0x7FFFFFFFu`; `CF_DECODE_FACTOR`,
    `CF_DECODE_KEY_COST`, `YAML_ALIAS_FACTOR` and
    `YAML_ALIAS_BYTES_FACTOR` `64u`; `YAML_ALIAS_FLOOR` `4096u`;
    `CF_DECODE_FLOOR` and `YAML_ALIAS_BYTES_FLOOR`
    `((size_t)1024u * 1024u)`. The value printed is the raw text
    evaluated by one small function: a trailing `u` or `ul` after
    digits is dropped, `(size_t)` is dropped, a name already read is
    replaced by its value, `0x...` is read as hexadecimal, and what is
    left (numbers, `*`, parentheses) is given to `math(EXPR)`.
  * **Section 4.** The three constants of
    `lib/config/include/configforge/cf_flatten.h` (after "What one document may
    make of the key space"), `CF_DECODE_FACTOR` 64,
    `CF_DECODE_FLOOR` 1 MiB, `CF_DECODE_KEY_COST` 64, and three
    computed rows made by the generator, `64 x n + 1 MiB` for n of 1 KiB,
    1 MiB and 256 MiB, printed as `1.06 MiB`, `65 MiB` and `16.0 GiB`
    each with its arithmetic. YAML's own bounds, from
    `lib/serialize/src/yaml.c` (the
    `YAML_ALIAS_*` defines):
    `YAML_ALIAS_FACTOR` 64, `YAML_ALIAS_FLOOR` 4096 (events),
    `YAML_ALIAS_BYTES_FACTOR` 64, `YAML_ALIAS_BYTES_FLOOR` 1 MiB
    (bytes of scalar text). The depth rule, in the page's words from
    the comment above `CF_FLATTEN_MAX_DEPTH` in `cf_flatten.h`: the
    document is level one, so `CF_MAX_DEPTH`
    32 allows 31 levels of tables and arrays inside it, in every
    reader and writer; a build with more or less is another number, and
    `cf_build_limit("depth")` says which (config's `cf_version.h`,
    its comment on `cf_build_limit`). The decode budget
    is the flattener's: a codec used alone, through `cf_codec_decode`
    and a sink of the program's, is not under it (D-82); the page says
    so.
  * **Section 5, the largest document.** One row a format, three
    columns. "In the code": what a reader itself refuses: XML
    `CF_XML_INPUT_MAX` (the call to expat in `xml_run` takes an `int`);
    every text reader's line and column are `unsigned int`
    (`cf_loc`, in `cf_serialize.h`), so a position past 4,294,967,295
    lines or columns wraps: stated as a bound on the document, in lines
    and in bytes of one line; the other readers: "no limit of its own:
    the document's size, `size_t`, and the decode budget when read
    through the configuration library". This column is written by hand
    for every format but XML (the generator reads `CF_XML_INPUT_MAX`
    for that one); it is not generated. "Run at": the largest size P-1
    ran, from the baseline of `docs/Performance.md` ("Baseline, before
    any change (P-1)": an Intel Core i7-10750H at 2.6 GHz, Windows 11,
    MSVC; its WSL 2 table has GCC and Clang), in the words of that page
    (256 MiB where all five formats read it, else the size P-1's "stop
    if" recorded and the reason); this column is **H**, with the date
    and the commit, because a size cannot be tested on every build.
    `docs/Performance.md` has no XML row, so the XML row says "not run"
    in this column. "A string": with the configuration library, the
    document's own bound; read through a sink without `reserve_text`
    (the member of `cf_doc_sink` in `cf_serialize.h`), 255 bytes (the
    `char local[256]` of the JSON reader's string buffer in `json.c`).
    The row for a format the comparison does not run it at says "not
    run beyond N MiB".
  * **Section 6.** One sentence and a link to W5-V2's pair table: in
    `docs/Formats.md`, the section "What each format holds, and what a
    conversion loses", between its `<!-- pairs:begin -->` and
    `<!-- pairs:end -->` markers, as `Formats.md#what-each-format-holds-and-what-a-conversion-loses`
    (the gate's link check takes the file part and, for an anchor,
    looks the heading up in that file). Step 1 stops the card when
    that section is not in the file, since a placeholder sentence would
    be a statement nothing checks. No copy of the table.
  * **Sections 7 and 8, from the cards before.** 7 is read from
    `docs/targets.txt` (A-12): one row a target, with its `how`:
    `hardware`, `runner`, `qemu`, `built` and `none`. The page has A-12's
    five classes as five headings, in this order: `hardware` is "tested
    on hardware", `runner` "tested on CI runners" (a `runner` line with
    no job, which A-12 allows for a local run, reads "tested by
    `verify`, not CI"), `qemu` "tested under qemu", `built` "built, not
    run", `none` "not tried". The generator prints a table for each of
    those with a row, target and note, and leaves out a heading with no
    row. 8 is read
    **statically**, from the sources, not from the build's
    `cf_cversion.txt` (a default build declares nothing, so that file
    would give a page that changes with the options and a gate that
    differs between jobs): the generator scans `CMakeLists.txt` and
    `lib/*/CMakeLists.txt` for the calls
    `cf_source_needs_c(<file> <standard> REASON "<text>")` (A-11's
    helper, read whole as that card's `cf_option_defaults` reads
    files, calls over several lines included) and prints the C89 floor
    and one row for each call, file, standard and reason, in the order
    found, with the build option the call sits under when it is the
    condition of its `if()`. A scan that finds no call stops the card
    ("stop if"): A-11 puts two.
  * **Section 9.** Hand-written from D-14 ("Reads are safe
    concurrently. Building and releasing are the caller's to
    synchronise."), `docs/Safety.md`'s design section, `cf_live`'s
    lock, `CF_SINGLE_THREADED` (what it removes), and the bindings'
    rules in Safety.md "The bindings". Links, no restated numbers.
  * **Section 10.** Two lists and links: what is not proven, from
    `docs/Safety.md` "Not yet proven" (the floating point parser and
    printer, the codecs' tokenizers, `cf_live`'s ticket lock), linked
    and not copied; what is not claimed, from `docs/Compliance.md`
    "What may be said, and when" (not "MISRA compliant" until a
    qualified tool has run, never "certified"), linked.
  * **Section 11, not offered.** A table: feature, why, the decision.
    Built from `docs/Decisions.md`'s index and `docs/Design.md`
    section 64, each row checked against the code before it is
    written, which is why it is H. Rows, each with what the decision
    says: no network or remote key-value source (D-30: "deferred, a
    non-goal until something asks"); no database, secrets platform,
    service discovery or distributed service (Design.md section 64:
    non-goals, no D-number); no schema language of its own, JSON
    Schema is read and written (D-64, D-67); writing comments back
    into a hand-written file (Design.md section 39); the Windows Registry's `REG_EXPAND_SZ` is not
    expanded (D-30); value interpolation exists but is bounded and not
    recursive in provenance (D-10, D-77); a DOM and a stream reader in
    the configuration library (Architecture.md; the serialization
    library gets both, Roadmap "Ways to read", D-61: a stream for 2.0);
    targets with a 16-bit `int`, no 64-bit integer type or near and far
    pointers (Design.md, support matrix tier C: out of scope); YAML
    tags that build objects (D-63: the core tags only). A row whose decision text, read again when
    this card runs, does not say what the row says is dropped and
    named in the notes.
  * **Two statements elsewhere that the code contradicts, corrected
    here.** `docs/Architecture.md` "What is deliberately not here" says
    value interpolation is "rejected with a clear message, not
    implemented" and that a second document binary format is not
    here: interpolation is implemented and bounded
    (`lib/config/tests/test_interpolate.c`, `CF_EXPAND_MAX`, D-77) and CBOR is
    a codec (D-91). Both bullets are rewritten to say what is true and
    point to Limits.md; the bullets for the DOM and for streaming stay
    and gain "(the serialization library: Roadmap, "Ways to read")".
    The README's row for `CF_MESSAGE_MAX` (in "Limits and the stack
    budget") says it is
    also the longest string the JSON and TOML readers take; JSON's
    buffer is the fixed `char local[256]` of `js_text` in `json.c` and a longer
    string is read through the sink's `reserve_text`: the row loses
    the sentence and the section after the table gains "The rest of
    the limits, and the fixed ones, are on `[Limits](docs/Limits.md)`."
  * **What is generated and what is tested, and by what.**
    * `scripts/generate/limits.cmake` (new; `-DOUT=<file>` writes the
      page's generated blocks, `-DWRITE=<page>` replaces them in
      place): the blocks of sections 2, 3, 4, 7 and 8 and the
      computed rows of section 5, each between
      `<!-- limits:<n>:begin -->` and `<!-- limits:<n>:end -->`
      markers, `<n>` the section number. A table row that is read from
      a file is followed in the page by nothing the script cannot
      verify.
    * `gate.limits` (ctest, `cmake/gates/GateLimits.cmake`, registered
      in `cmake/ConfigForgeGates.cmake` beside `gate.apiref`; needs no
      compiler): regenerates the blocks and compares with the page,
      line endings ignored, as `GateApiref.cmake` does; fails
      with `docs/Limits.md does not match the sources: run cmake
      -DWRITE=docs/Limits.md -P scripts/generate/limits.cmake`. It
      also fails when a `D-nn` named anywhere on the page has no
      `## D-nn` heading in `docs/Decisions.md`, with `docs/Limits.md
      names D-nn, which docs/Decisions.md does not have`; when a
      ctest name in a "Held by" cell (section 4's rows name
      `unit.test_budget` and `unit.test_limits`; section 3's name
      `unit.test_dirlimits` for the two directory caps) is not registered,
      with `docs/Limits.md names the test <name>, which is not
      registered`. The gate reads the registered names from the text of
      the `CMakeLists.txt` files under `lib/*/tests/` and `tests/`: each
      `add_test(NAME <name> ...)` and each entry `<t>` of a
      `CF_UNIT_TESTS` list (registered as `unit.<t>`); it needs no
      compiler and no build tree, so it runs where `gate.apiref` runs.
      It fails, too, when a relative link
      does not name a file (a `#anchor` also needs its heading in that
      file). Its messages are matched with `[ \n]+` between words
      wherever a test matches them, as A-11 says.
    * `test_limits` (T; `lib/config/tests/test_limits.c`). It is **not** in the component's
      `CF_UNIT_TESTS` list: that list's loop registers every test as
      `add_test(NAME unit.${t} COMMAND ${t})` with no argument, and this
      one needs the page. It has a block of its own beside
      `test_dirlimits`'s (the one that sets that test's working
      directory): `add_executable`, the same warnings and analysis
      calls, and `add_test(NAME
      unit.test_limits COMMAND test_limits ${CF_REPO_ROOT}/docs/Limits.md)`.
      It is given no definition: the test computes the default limits
      from the four compiled values itself (below), not from
      `CF_DEFAULT_LIMITS`, so the standalone `cmake -S lib/config`
      build needs nothing the whole build sets.
      `CF_REPO_ROOT` is A-1b's variable: the whole build's
      `${CMAKE_SOURCE_DIR}`, and in `cmake -S lib/config`
      `${CMAKE_CURRENT_LIST_DIR}/../..` (A-1b's `cf_component` sets it
      in both cases; if it is not set, A-1b is not as written: stop).
      C89. It reads the page named
      by `argv[1]`, finds the section-2 block, and for each of the
      five rows parses the default; when the compiled `CF_KEY_MAX`,
      `CF_MESSAGE_MAX`, `CF_PATH_MAX` and `CF_MAX_DEPTH` all equal the
      defaults of the page's rows (a default build) it
      checks the five rows against the compiled values and that `cf_build_limit("key")`, `"message"`, `"path"`,
      `"depth"` and `"source"` return the numbers of the page's rows;
      otherwise it prints `limits: not a default build, page not compared`
      and passes (the `minimal` preset is such a build; `portable`
      keeps the default limits and compares). Always: the section-4 constants equal `CF_DECODE_FACTOR`,
      `CF_DECODE_FLOOR`, `CF_DECODE_KEY_COST`, `cf_decode_budget(0)`
      equals the floor, `cf_decode_budget(1048576)` equals `64 *
      1048576 + 1048576` (the page's 1 MiB row), `CF_SOURCE_MAX` equals
      `sizeof` of `cf_error`'s `source` member, and the depth the page
      states is the one `cf_build_limit("depth")` returns. The
      behaviour at a limit is held by the tests that already exist
      (`test_budget` for the budget, `test_dirlimits` for the directory
      caps, `test_serialize` and `test_cbor` for depth); the page names
      them in its "Held by" column and does not re-test them.
  * **What is not generated, and why.** Section 5's "Run at" and the
    prose of sections 1, 6, 9, 10 and 11: a size run once on one
    machine, and sentences, are facts the build cannot read. The gate
    checks their references, never their words.
  * **No D-number.** Nothing is decided that a later card could not
    undo; the page records decisions made elsewhere. (If a step here
    forces a choice not written above, the next free number is D-112,
    and the card stops to write it.)
* **files**: `docs/Limits.md` (new), `scripts/generate/limits.cmake`
  (new), `cmake/gates/GateLimits.cmake` (new),
  `cmake/ConfigForgeGates.cmake`,
  `lib/config/tests/test_limits.c` (new), `lib/config/tests/CMakeLists.txt`
  (a block of its own for
  `test_limits`, beside `test_dirlimits`'s), `lib/serialize/src/xml.c`
  (the `in->n > 0x7FFFFFFFu` test in
  `xml_run`), `mkdocs.yml`, `scripts/README.md`;
  last: `README.md` (the corrected row, and one row, `limits`, in its
  "Gates" table), `docs/Architecture.md` (the two corrected bullets,
  and one row, `gate.limits`, in its table of gates),
  `docs/Roadmap.md`, `docs/Plan.md`, `CHANGELOG.md`.
* **steps**:
  0. If `## 1.8.0 — unreleased` is not open, open it:
     `cmake -DTO=1.8.0 -P scripts/release/version.cmake`; commit that
     alone.
  1. Check what sections 6 to 8 read and write it in the notes:
     `docs/targets.txt` with its rows (A-12), at least one
     `cf_source_needs_c(` call in `CMakeLists.txt` (A-11), and the pairs
     section of `docs/Formats.md` with its markers (W5-V2). Any of the
     three missing: stop.
  2. The gate first. `cmake/gates/GateLimits.cmake`, registered, and a
     `docs/Limits.md` of the headings and empty marker pairs only;
     `ctest -R gate.limits` fails with the "does not match" message
     (the generator does not exist yet: the gate fails on that too,
     with `scripts/generate/limits.cmake: cannot be run`). See it.
  3. In `xml.c`, replace the literal `0x7FFFFFFFu` of the size test in
     `xml_run` by a macro defined above it as `#define CF_XML_INPUT_MAX
     0x7FFFFFFFu`; the value is the same. No default build compiles `xml.c` (it needs
     `CF_WITH_XML=ON` and expat), so the change is built and tested
     by CI's `yaml` job (it configures `-DCF_WITH_XML=ON` and runs
     `ctest`) and locally with `-DCF_WITH_XML=ON`: configure, build and
     `ctest` there, passing unchanged. Done before the generator, whose section-3 row reads it.
  4. `scripts/generate/limits.cmake`, section by section, each added
     with the sentence it reads from; `-DWRITE=docs/Limits.md`; the
     gate passes. Then break each source on purpose and see the gate
     fail: change `CF_KEY_MAX`'s default in `ConfigForgeOptions.cmake`
     only (the three-way disagreement message names the header and
     `cf_port.h`), rename `CF_DIR_MAX_FILES` in `dir.c` (the row is
     not found), reword `CF_EXPAND_MAX` (the raw text is not the
     expected one), put each back.
  5. The test that fails first: `test_limits.c`, with the page's
     `CF_KEY_MAX` row edited by hand to `255`; `ctest -R
     unit.test_limits` fails naming the row; restore the row and it
     passes; configure the `minimal` preset
     and see the line `limits: not a default build, page not compared`
     and a pass.
  6. The hand-written sections 1, 5's "Run at", 6, 9, 10 and 11, each
     claim read against the code or the decision at the time of
     writing (section 11: open each D-number and read its decision).
  7. `mkdocs.yml`: under Design, after Safety, `- Limits:
     docs/Limits.md`; `python -m mkdocs build --strict` where MkDocs
     is installed (the `docs` workflow runs it otherwise, and its
     link check covers the page's links). The two corrected statements
     (Architecture.md, README.md).
  8. `scripts/README.md`: rows for `generate/limits.cmake` and
     `gate.limits`. Roadmap, Plan, CHANGELOG. ACCEPT.
* **accept**: ACCEPT-LINUX with `-DEXTRA="components;abi"` (A-1b's
  `components` step builds each component alone and `lib/config` holds
  the new test; `abi` because `xml.c` is in the libraries, though no
  symbol changes), ACCEPT-WINDOWS (`gate.limits` and `unit.test_limits`
  run there), ACCEPT-CI (the `docs` workflow builds the page under
  `mkdocs build --strict`). The `minimal` build runs
  `unit.test_limits` and prints that the page is not compared; `portable`
  compares.
* **stop if**: A-11's calls, A-12's file or W5-V2's section do not exist
  in the form step 1 needs: write what is missing in the notes; do not
  invent the file. The three sources of a build limit (CMake default, header
  default, `cf_port.h` range) already disagree when step 3 first runs:
  that is a finding, not something to fix here: write it in the notes
  and stop. A row of section 11 cannot be matched to a decision that
  says it: drop the row, name it in the notes, go on.
* **docs**: no D-number (above). `docs/Roadmap.md`: "A page of the
  project's limits" goes 🟩 with "docs/Limits.md; `gate.limits`,
  `unit.test_limits`"; and the item "Formats registered, with what each
  can do" says twice that `docs/Limits.md` is generated from the
  formats table ("and `docs/Limits.md` generated from it, are left
  (A-13)", and "the pair table and `docs/Limits.md` are generated from
  it"): it is not. Section 6 links the pair table, and the rest of the
  page is generated from the build options and the sources, so both
  sentences are reworded to say that `docs/Limits.md` links the table
  and does not copy it. `docs/Plan.md`: A-13's index row `done` with its
  commits (the row is there already). CHANGELOG,
  `## 1.8.0 — unreleased`, "### Added": "- **`docs/Limits.md`**: the
  build limits, the fixed ones, the decode budget, the largest
  document of each format, the platforms and C versions, the threading
  rules, what is not proven and not claimed, and what is not offered,
  with the decision for each; its numbers are generated from the
  build options and the sources, and `gate.limits` and `test_limits`
  fail when it drifts." and, "### Changed": "- The README's limits
  table no longer says `CF_MESSAGE_MAX` bounds a JSON or TOML string
  (it never did); `docs/Architecture.md` no longer says value
  interpolation and a second binary format are absent."
* **notes**:
  * 2026-11-23, step 1 at fbdcbe9: docs/targets.txt has ten targets (six runner, four none). The top-level CMakeLists.txt has two cf_source_needs_c calls. docs/Formats.md has the pair-table heading and both pairs markers.
  * 2026-11-23, steps 2 to 8 (work/A-13). Done: the gate, `scripts/generate/limits.cmake` (sections 2, 3, 4, 5 (XML row), 7, 8), `lib/config/tests/test_limits.c` and its block in `lib/config/tests/CMakeLists.txt`, the written sections, `mkdocs.yml`, the corrected Architecture.md and README.md statements, the READMEs' rows, Roadmap, Plan, CHANGELOG. The three source mutations of step 4 each failed the generator by name (CF_KEY_MAX's CMake default: the three-way message; `CF_DIR_MAX_FILES` renamed: "no #define"; `CF_EXPAND_MAX` reworded: "is now ... not ..."); step 5's edited row failed `unit.test_limits` naming the row, and the `minimal` build printed the "not a default build" line and passed.
  * Organiser's answers (decided by the card's rules while carrying it):
    * The gate checks "Held by" names against registered tests, so it cannot pass before `unit.test_limits` is registered: step 4's gate run therefore passed every check but that one until step 5 registered the test; the order of the two steps is otherwise as written.
    * The card's rule "a default build is one whose compiled limits equal the page's rows" makes an edited row look like another build, and nothing then fails. `test_limits` takes a build as another build only when two or more of the four compiled limits differ from the page's rows (`minimal` differs in three); one that differs is a page that has drifted and fails naming the row.
    * Section 5's marker block holds only the XML row (the one number the generator reads); the table of the other formats ("In the code", "Run at", "A string") is written by hand beside it, as the card says it is not generated.
    * Section 8 lists each file once: `lib/platform/CMakeLists.txt` and `lib/serialize/CMakeLists.txt` repeat the top-level calls for their stand-alone builds, and a call's file is read as `lib/<name>/<path>` there, so the repeats are the same row.
    * Section 3 has a fifth column, "Held by", because the gate finds test names by that column's heading.
  * The card got wrong: `cf_loc` is in `lib/base/include/configforge/cf_error.h`, not `cf_serialize.h`.
