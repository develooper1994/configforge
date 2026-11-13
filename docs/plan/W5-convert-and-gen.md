# Wave 5: converting, and code from a contract

Roadmap wave 5 as cards (docs/Plan.md says what a card is and how one is
carried out). Three pieces, all in Doctor:

* **W5-V1**: `doctor convert`, a file read through one codec and written
  through another, every value the target cannot hold named and left out,
  never dropped quietly. D-94.
* **W5-G1 to W5-G3**: `doctor gen --schema S --lang c|cpp|rust|python`.
  Today it writes C; C++ gets `cf::Key`s and a `cf::Binding`, Rust `Key`
  constants and a `settings!` struct, Python a dataclass of `cf.setting`
  fields, each generated file compiled or imported and run by a test as
  `tests/doctor/test_gen.c` does for C. D-95.
* **W5-S1**: `doctor schema --from <file>`, a schema drafted from a sample
  configuration, for whoever has no schema yet. D-96.

Not here: wave 6's "every operation on a file's data, written back in its
own format". `doctor convert` writes a new file in another format; it does
not edit one in place, and keeps no comment or layout.

Order: W5-V1, W5-S1, W5-G1, W5-G2, W5-G3. They share `tools/doctor/doctor.c`
(the usage text and the dispatch), `tools/doctor/adapters.h` and
`docs/Doctor.md`, so they go one after another. W5-V1 comes after wave 4
(it converts to and from CBOR, W4-C3). Every card is accepted on Linux and
Windows; W5-G2's Rust part runs where cargo is (CI's cargo job, and
`CF_RUST=ON` locally).

D-numbers: this file uses **D-94, D-95 and D-96** (the wave 4 cards use
D-91 to D-93; see `W4-binary-formats.md` on why they start there). Check
with `git grep -n "^## D-9[4-6]" docs/Decisions.md` before writing one; if
a number is taken, take the next free one and change it in every card of
this file in the same commit.

The CHANGELOG: each card adds to the topmost section headed `— unreleased`,
under `### Added`. If that section has a date instead, add
`## <the next minor>.0 — unreleased` with an `### Added` heading above it
first (1.6.0 when 1.5.0 is the latest released).

For the maintainer: one question, in W5-G2 (what the generated Rust and Python
define the contract with). Nothing to install.

---

## W5-V1 - `doctor convert`

* **id**: W5-V1
* **goal**: `doctor convert <in> --to <format> [--as <format>] [--out <file>] [--strict]`
  reads a file through its codec and writes it through another's encoder,
  and every value, array or type the target cannot hold is a finding on
  stderr; under `--strict` a finding fails the run and nothing is written.
* **why**: a configuration moved from INI to TOML by hand, or by a script
  that drops what it does not understand, arrives with values missing and
  nothing to say so; converted here, the new file holds what the old one
  said, and what it could not hold is listed by key - so the value a
  program later reads from it is the one somebody wrote.
* **needs**: W4-C3
* **size**: large
* **decided**:
  * **Through the target codec's encoder, fed by `cf_doc_emit_where` over a
    key space the source codec decoded into - not `cf_emit`.** `cf_emit`
    takes a `cf_config`, which is built by resolving: references are
    expanded and their `$${` undone, untyped text is typed by inference, and
    the program's definitions and defaults join in; and it doubles every
    `${` it writes. A conversion carries the document's own text: `${HOME}`
    read is `${HOME}` written, neither expanded nor doubled. And `cf_emit`
    refuses a whole document for one array its target cannot write, where a
    conversion leaves that array out and says so. The walk is the one
    `cf_emit` uses (`cf_doc_emit_where`, `src/bridge/flatten.c`), so the
    order a target needs - a table's values before its tables, INI's root
    keys before its first section - is the same. Rejected: decoder events
    straight into the encoder, with no key space (a JSON document's order
    would put a later root key inside an INI section).
  * Command line, parsed in this order: the first argument not starting
    `--` is `<in>` (`-` is standard input, read whole, binary); `--to F`
    (required), `--as F`, `--out F` (each once), `--strict` (a flag).
    Anything else, a second input, a missing `--to`: the usage, exit 2.
  * The source's codec: `--as` names it (`cf_codec_by_name`); otherwise
    `cf_codec_detect(NULL, 0, &blob, <in> or NULL for "-", trial, &d)`: the
    extension, then the content (D-73). Errors, each to stderr and exit 2:
    * `doctor: no format called '%s'`; `doctor: format '%s' is not compiled into this build; turn on %s`
      (`cf_format_option`), as cf_emit says them;
    * `doctor: format '%s' can be written but not read`;
    * `doctor: %s reads as %s and as %s, differently; name its format with --as`
      (`d.first`, `d.second`);
    * `doctor: cannot tell the format of %s; name it with --as`;
    * `doctor: cannot read %s` (the file);
    * `doctor: %s` with the reader's own message when it refuses the bytes,
      and `doctor: '%s' holds a key this build cannot hold` when the
      flattener could not (a key with a capital letter, say: the library's
      keys are lowercase, so a file it cannot read as a configuration is
      one it cannot convert).
  * The target's codec: `--to` by `cf_codec_by_name`; the first two errors
    above, and `doctor: format '%s' can be read but not written`, exit 2.
  * Reading: `cf_ks_init(&ks, ar, 64)`, `cf_flatten_init(&fl, &ks, 0)`,
    `cf_codec_decode(src, &blob, cf_flatten_sink(&fl), &err)` (W4-T1's; a
    TLV codec is not reachable here, having no layout), then
    `cf_ks_resolve(&ks)`. A key the file sets twice is written once, the
    later, as the file itself means it.
  * What the target cannot hold, found before anything is written, in two
    passes over the key space; each finding is a line on stderr,
    `doctor convert: <key>: <what>, which <format> cannot hold; left out`,
    and the key and everything under it are left out of the output:
    1. **Arrays**, by the target's caps (`cf_emit`'s rule, emit.c's probe,
       written again here as a sink in convert.c that keeps the path): with
       neither `CF_CODEC_ARRAYS` nor `CF_CODEC_SCALAR_ARRAYS`, every array,
       `an array`; with `CF_CODEC_SCALAR_ARRAYS` only (INI), an array that
       holds a table, `an array of tables`, an array, `an array of arrays`,
       or nothing, `an empty array`.
    2. **Values**, by the target's encoder itself: the encoder is run over
       the walk into a sink that stores nothing (`cf_sink_init(&out, NULL, 0)`),
       through a wrapping sink that passes each event on and, when the
       encoder refuses one, records the key and returns true so the walk
       goes on (a refused container: its events are not passed on until its
       `end`). `<what>` is `a null` for a null, `text with a line break` for
       text holding `\n`, and `a value of this kind` otherwise. This is how
       TOML's null and INI's line break are found, and whatever a future
       encoder refuses, without convert knowing each format's rules.
    The walk of the second pass and of the writing leaves out what the
    first found, through `cf_doc_emit_where`'s predicate.
  * Three more things said, once each:
    * `doctor convert: note: <format> can hold comments, and no reader keeps them: any in <in> are not in the output`
      when the source is `ini`, `toml`, `yaml`, `json5`, `properties` or
      `xml` (a table in convert.c). Every reader drops comments today and
      none says whether there were any, so this is said for the format, not
      for the file.
    * `doctor convert: note: <format> carries no types, so its values are written to <format> as text`
      when the source lacks `CF_CODEC_TYPED` and the target has it: INI's
      `port = 8080` is the JSON string `"8080"`, faithfully.
    * `doctor convert: <format> carries no types: <n> value(s) that were numbers, booleans or nulls are written as their text`
      when the target lacks `CF_CODEC_TYPED` and the second pass passed on
      `n` non-text scalars (`n > 0`).
    The two notes are not findings: they are about the formats, the same for
    every file, and a `--strict` that failed on them would fail every
    conversion out of TOML. The third is a finding: a value's type is lost.
  * Exit codes, against `docs/Doctor.md` section 10: **0** converted (with
    findings, when not `--strict`); **1** `--strict` and at least one
    finding - the findings are printed, then
    `doctor convert: <n> finding(s); nothing written (--strict)`, and no
    output is made (an `--out` file that existed is left as it was);
    **2** unusable input or options, as above; **3** the encoder refused, in
    the writing, something the second pass let through -
    `doctor convert: %s (a bug in doctor convert: the check let it through)`
    with the walk's message - which is an internal error.
  * Writing: the encoder's state on the stack in the aligned union
    `cf_emit` uses (`CF_ENCODER_STATE_MAX`; a larger `encoder_state_size`
    is `doctor: no room for the '%s' encoder`, exit 2); measured first into
    a NULL sink, then written into `len + 1` bytes from the arena. No flag
    is set in a binary encoder's head (W4-C3): `${` is written as read.
    `--out F` is opened `"wb"`, written whole and closed; a failure is
    `doctor: cannot write %s`, exit 2, and the file removed. Without
    `--out`, standard output, put in binary mode on Windows as
    `tests/conformance/conform.c` does (`_setmode(_fileno(stdout), _O_BINARY)`),
    so a CBOR file piped on is its bytes.
  * The code: `tools/doctor/convert.c` (new), C89, statics `cv_`, in the
    `doctor` executable's list in `tools/doctor/CMakeLists.txt` after
    `contract.c`. `tools/doctor/adapters.h` declares, after the contract
    commands:

    ```c
    /* A file read through another format's writer (convert.c): what the
       target cannot hold named on stderr and left out (D-94). */
    int doctor_convert(int argc, char** argv);

    /* A document read as a configuration file is - its format named by
       `as`, else from its name and then its content (D-73) - into `ks`,
       resolved; `path` "-" is standard input. 0 when read, else Doctor's
       exit code (2) with the reason on stderr. `used`, when not NULL, is
       the codec that read it. Shared by convert and schema --from. */
    int doctor_read_keys(const char* path, const char* as, cf_arena* ar,
                         cf_keyspace* ks, const cf_codec** used);
    ```

    (`adapters.h` then includes `<configforge/cf_keyspace.h>` and
    `<configforge/cf_serialize.h>`). `doctor.c`'s `main` dispatches
    `convert` beside `gen`; `usage()` gains, in a call of its own:
    `"  doctor convert <file> --to <format> [--as <format>] [--out <file>] [--strict]\n"`
    `"                                 the file in another format, and what it cannot hold\n"`.
    The header comment of doctor.c lists the command.
* **files**: `tools/doctor/convert.c` (new), `tools/doctor/adapters.h`,
  `tools/doctor/doctor.c`, `tools/doctor/CMakeLists.txt`,
  `tests/doctor/convert.cmake` (new), `tests/doctor/CMakeLists.txt`; last:
  `docs/Doctor.md`, `docs/Decisions.md`, `CHANGELOG.md`, `docs/Roadmap.md`.
* **steps**:
  1. `tests/doctor/convert.cmake`, run with `-DDOCTOR=<doctor> -DWORK=<dir>`
     and registered in `tests/doctor/CMakeLists.txt` as
     `add_test(NAME doctor.convert COMMAND ${CMAKE_COMMAND} -DDOCTOR=$<TARGET_FILE:doctor> -DWORK=${CMAKE_CURRENT_BINARY_DIR}/convert -P ${CMAKE_CURRENT_SOURCE_DIR}/convert.cmake)`
     (git.cmake is the model: it writes its own input). A helper
     `cv_case(<name> <expected-exit> <stderr-regex or "-"> <args>...)` runs
     Doctor, checks the exit code and the regex, and fails with both
     outputs printed. The cases, every input written with `file(WRITE)`:
     * `json_toml`: `a.json` `{"server": {"port": 8080, "host": "web"}, "tags": ["a", "b"]}`
       `--to toml --out a.toml`: 0, stderr empty; `a.toml` contains
       `port = 8080` and `host = "web"`; then `a.toml --to json`: 0, stdout
       contains `"port": 8080`.
     * `null_toml`: `n.json` `{"a": null, "b": 1}` `--to toml --out n.toml`:
       0, stderr `a: a null, which toml cannot hold; left out`; `n.toml`
       contains `b = 1` and not `a`. With `--strict --out s.toml`: 1, the
       same line and `nothing written`, and `s.toml` does not exist.
     * `tables_ini`: `t.json` `{"k": "v", "n": 5, "s": {"list": [{"x": 1}]}}`
       `--to ini`: 0, stderr `s.list: an array of tables, which ini cannot hold; left out`
       and `ini carries no types: 1 value\(s\)`; stdout has `k = v`.
     * `empty_ini`: `{"e": [], "k": "v"}` `--to ini`: `e: an empty array`.
     * `lines_ini`: `{"m": "a\nb", "k": "v"}` `--to ini`:
       `m: text with a line break, which ini cannot hold; left out`.
     * `ini_json`: `c.ini` `[s]\nk = 8080\n` `--to json`: 0, stderr has
       `note: ini can hold comments` and `note: ini carries no types`;
       stdout has `"k": "8080"`. With `--strict`: still 0 (notes are not findings).
     * `refs`: `r.json` `{"u": "${HOME}/x", "v": "$${y}"}` `--to toml`:
       stdout has `${HOME}/x` and `$${y}`, each exactly once.
     * `content`: `noext` (no extension) holding `{"a": 1}` `--to toml`: 0,
       stdout `a = 1`.
     * `cbor` (only `if(CF_WITH_CBOR)`, passed in as `-DCBOR=ON`): `a.json`
       `--to cbor --out a.cbor`, `a.cbor --to json --out back.json`, and
       `a.json --to json --out same.json`: `back.json` and `same.json` are
       the same bytes (`cmake -E compare_files`).
     * unusable, each 2: `a.json --to nosuch` (`no format called 'nosuch'`),
       `a.json --to properties` (`can be read but not written`),
       `missing.json --to toml` (`cannot read`), `a.json` with no `--to`,
       and `a.json --to toml --bogus`.
  2. A `convert.c` that is dispatched and returns 2 at once. See it fail:
     `cmake --build --preset default && ctest --preset default -R doctor.convert`
     fails on `json_toml` (exit 2, expected 0).
  3. `doctor_read_keys`, then the writing with no checks: the cases without
     findings pass; `null_toml` fails (TOML refuses the null mid-walk).
  4. The two passes and the notes: every case passes.
  5. `docs/Doctor.md`: section 7's command block gains
     `doctor convert <file> --to <format> [--as <format>] [--out <file>] [--strict]`
     with two lines saying what it does, and a section "7e. Converting
     between formats", fifteen to twenty lines: the command, the findings
     with an example (`null_toml`'s), the notes and why they do not fail
     `--strict`, `${` written as read, and that comments are lost. Section
     10 gains, after the table: "`convert` uses the same codes: 1 is
     `--strict` with a finding, and nothing is written; 3 is an encoder
     refusing what the check passed."
  6. ACCEPT-LINUX, ACCEPT-WINDOWS.
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI. `doctor.convert`
  passes on both; on Windows the `cbor` case proves standard output is binary.
* **stop if**: `cf_doc_emit_where` does not accept a predicate that leaves
  out a whole array (it asks `any_included` of an array as a whole: an
  array with one member left out is written without it, renumbered, which
  is wrong - then the exclusions must be by the array's whole key range;
  if the predicate cannot express that, hand back); or an encoder's
  refusal in the second pass changes its state so that later events are
  refused too (then convert's second pass reports what is not true: hand
  back with the case).
* **docs**: add **D-94**, "Converting between formats", in D-80's shape: the
  question (a file in another format, saying what the target cannot hold);
  the options (through `cf_emit`; decoder to encoder directly; through the
  key space and the encoder); the decision (the last, and why each of the
  others fails; the two passes; findings, notes and what counts under
  `--strict`; the exit codes; `${` as read; comments said once);
  compatibility (a new command; nothing else changes). CHANGELOG, Added:
  "`doctor convert <file> --to <format>`: a configuration file in another
  format - INI, JSON, TOML, YAML, CBOR - and every value, array or type the
  target cannot hold named on stderr and left out; `--strict` fails rather
  than write a lossy file (D-94)." Roadmap: the row "Converting between
  formats" 🟩, `D-94`; wave 5's text "converting between formats
  (`doctor convert`, through the target's encoder over the key space,
  D-94)" in place of "through `cf_emit`".
* **notes**:

---

## W5-S1 - `doctor schema --from`: a contract drafted from a sample

* **id**: W5-S1
* **goal**: `doctor schema --from <file> [--as <format>] [--title <title>] [--out <file>]`
  writes a JSON Schema with a key for every value the file holds, its type
  from the value, every key optional, no default, and an empty description
  to fill in.
* **why**: a contract is what lets a value be refused with its key and its
  reason instead of read wrong; a program with years of configuration files
  and no contract gets a first one from a file it already has, which
  `doctor check` then holds every other file to.
* **needs**: W5-V1
* **size**: medium
* **decided**:
  * Reading: `doctor_read_keys(path, as, ar, &ks, &used)` (W5-V1), so the
    format is chosen as everywhere (D-73) and the errors are the same.
  * The walk: `cf_doc_emit(&ks, &sink, &err)` into a sink of contract.c's
    own (`sd_` statics) that keeps the path, as conform.c's does:
    * a scalar in a table is a definition at its path: its type is the
      value's (`int`, `real`, `bool`, `text`), except that text whose
      candidate is textual - `cf_ks_find(ks, path, len)->selected->flags & CF_CAND_TEXTUAL`,
      a value from INI, YAML's plain scalars, `.properties` - is typed by
      `cf_infer_scalar`, the library's Tier 0 inference (D-24), so INI's
      `port = 8080` is an integer; a null is left out:
      `doctor schema: <key>: a null, whose type the sample does not show; left out`;
    * an array of scalars of one type is a list of that type; an array
      that holds a table, an array, values of more than one type, or nothing
      is left out: `doctor schema: <key>: an array of tables, which a contract has no type for; left out`,
      `... an array of arrays, ...`, `... an array of mixed types, which a contract has no type for; left out`,
      `... an empty array, whose element type the sample does not show; left out`.
  * The definitions: `cf_defset_init(&ds, ar, 0)`; each with `cf_def_init`,
    `d.description = ""`; a list with the fields `src/bridge/schema.c` sets
    when it reads `{"type": "array", "items": {"type": T}}` with no default
    (read its array branch, near `sn_child(s, "items")`, and set the same
    `list_mode` and `list_separator`); `cf_define(&ds, &d, &err)`, a refusal
    printed `doctor: %s` with exit 2. No default (a sample's value is one
    deployment's choice, not the program's default, and written as one every
    other deployment would inherit it without a word); no range; no
    `required`, which `cf_schema_write` never writes anyway.
  * The writing: `cf_schema_write(&ds, title, CF_TRUE, ...)`, measured then
    written - closed, so `doctor check` refuses a misspelt key against it,
    and the library's own writer, so the draft reads back as the
    definitions it was made of. `title` is `--title`, else the file's base
    name without its extension (`app` for `conf/app.toml`; `stdin` for `-`).
    `--out F` (`"w"`; removed on failure) or standard output.
  * Then, on stderr, one line:
    `doctor schema: a draft from <file>: <n> key(s), every one optional, with no defaults, ranges or descriptions; review it before it is the contract`.
  * Exit codes: 0 written (lines left out do not change it: they are on
    stderr); 2 unusable input or options. No 1: there is nothing to fail.
  * The code: `int doctor_schema_from(int argc, char** argv);` in
    `tools/doctor/contract.c` (a contract command beside `check` and `gen`),
    declared in `adapters.h` after `doctor_gen_schema`; `doctor.c`
    dispatches `schema`; `usage()` and `contract_usage()` gain
    `"  doctor schema --from <file> [--as <format>] [--title <t>] [--out <file>]\n"`
    `"                                 a draft contract from a sample file\n"`.
* **files**: `tools/doctor/contract.c`, `tools/doctor/adapters.h`,
  `tools/doctor/doctor.c`, `tests/doctor/schema.cmake` (new),
  `tests/doctor/CMakeLists.txt`; last: `docs/Doctor.md`,
  `docs/Decisions.md`, `CHANGELOG.md`, `docs/Roadmap.md`.
* **steps**:
  1. `tests/doctor/schema.cmake`, registered as `doctor.schema` the way
     W5-V1 registers `doctor.convert`. Cases:
     * `toml`: `sample.toml`
       `debug = true\nratio = 0.5\ntags = ["a", "b"]\n[server]\nport = 8080\nhost = "web"\n`
       `--from sample.toml --out s.json`: 0; `s.json` matches
       `"port": \{ "type": "integer", "description": "" \}` and
       `"host": \{ "type": "string", "description": "" \}`, has `"boolean"`,
       `"number"` and `"array"`, and not `"default"` or `"required"`; stderr
       has `a draft from sample.toml: 5 key\(s\)`; its title is `"sample"`.
     * `holds`: `doctor check --schema s.json sample.toml`: 0. A
       `typo.toml` of `[server]\nprot = 1\n`: `doctor check --schema s.json typo.toml`: 1.
     * `gen`: `doctor gen --schema s.json --out s.c`: 0 (the draft is a
       contract `gen` can write).
     * `ini`: `sample.ini` `[server]\nport = 8080\nname = web\n`: `port`
       is an integer and `name` a string.
     * `left_out`: `odd.json`
       `{"n": null, "rows": [{"a": 1}], "mix": [1, "a"], "none": [], "k": 1}`:
       0, stderr has each of the four `left out` lines, and the schema has
       `"k"` and none of the four.
     * unusable, each 2: no `--from`; `--from missing.toml`.
  2. A `doctor_schema_from` that returns 2. See it fail:
     `cmake --build --preset default && ctest --preset default -R doctor.schema`
     fails on `toml`.
  3. The command. Rerun step 2: passes.
  4. `docs/Doctor.md`: section 7's block and section 7d gain the command and
     a paragraph: what it infers and from what, what it leaves out, no
     defaults and why, closed, "review it".
  5. ACCEPT-LINUX, ACCEPT-WINDOWS.
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI.
* **stop if**: `cf_schema_write` writes no `"description"` for an empty one
  (then "left empty" cannot be had through the library's writer: hand back
  rather than write JSON by hand); or the defset refuses a list definition
  with no default.
* **docs**: add **D-96**, "A contract drafted from a sample", in D-80's
  shape: the question; the options (types only, as here; types and the
  sample's values as defaults; a contract inferred from several files); the
  decision (the first, and why no defaults; Tier 0 inference for untyped
  text, the library's rule; what is left out and why; closed; the library's
  writer); compatibility (a new command). CHANGELOG, Added: "`doctor schema --from <file>`:
  a first JSON Schema contract drafted from a configuration file you
  already have - a key for every value, typed from it, every one optional,
  descriptions left to fill in (D-96)." Roadmap: the row "Code for each
  binding from a contract" 🟨 with "a schema drafted from a sample done
  (D-96); C++, Rust and Python left".
* **notes**:

---

## W5-G1 - `doctor gen --lang cpp`

* **id**: W5-G1
* **goal**: `doctor gen --schema S --lang cpp` writes one C++ header with
  the contract's definitions, a `cf::Key` for each key and a struct filled
  through a `cf::Binding`, and `--lang c` (the default) writes exactly what
  `doctor gen` writes today.
* **why**: a C++ program reads `cfg.get(contract::keys::server_port)` and a
  `Settings` struct whose fields cannot drift from the contract they were
  generated from, where today it writes the key names and defaults out a
  second time by hand - the repetition a contract exists to end.
* **needs**: W5-S1
* **size**: large
* **decided**:
  * Options: `--lang c|cpp|rust|python`, default `c`; anything else is the
    usage, exit 2. `--name` names what the language calls the generated
    thing: C the function (default `cf_contract`), C++ the namespace
    (default `cf_contract`), Rust and Python the top struct or dataclass
    (default `Settings`, W5-G2 and W5-G3). Each is checked as an identifier
    of its language (`c_identifier` for C and C++; W5-G2 and W5-G3 say
    theirs). Usage, in `doctor.c` and `contract_usage()`:
    `"  doctor gen --schema <schema> [--lang c|cpp|rust|python] [--name <name>] [--out <file>]\n"`.
  * **C is unchanged, byte for byte**: `gen_write` is split, without changing
    a byte it writes, into `gen_c_body(FILE* out, const cf_defset* ds)` -
    the declarations, `cf_def d`, `ok`, every definition, the opens and
    `return ok;` - and the C file around it. C++ uses the same body: C++
    compiles every line of it (no `long long` literal, no designated
    initialiser), and the C calls carry what a `cf::Key` cannot -
    descriptions, enumerations, lengths, lists, aliases, policies,
    deprecations, the opened prefixes - so the contract is the schema's,
    whole, in C++ as in C, and needs no JSON reader in the build.
  * Names, shared by the C++, Rust and Python writers (one helper each in
    contract.c, used by all three):
    * a key's identifier: its text with `.` and `-` as `_`
      (`server.max-conn` is `server_max_conn`); a first character that is a
      digit gets `k_` before it; a word the language reserves gets `_`
      after it (`type` is `type_`). Two keys with one identifier are
      refused: `doctor: '%s' and '%s' are both '%s' in %s; rename one`, exit 2.
    * a struct's name: the top segment in CamelCase, `-` and `_` taken as
      breaks (`server` is `Server`, `max-conn` `MaxConn`); one equal to the
      top struct's name gets `Table` after it.
    * the shape: a key of one segment is a field of the top struct; of two
      or more, a field of its top segment's struct, named by the rest's
      identifier (`server.tls.cert` is `Server::tls_cert`). Two levels,
      because `cf::Binding` reaches a field through at most two member
      pointers.
    * the order: keys in the order `cf_defset_at` gives them, a struct
      where its first key comes.
    * what has no field: a list (`list_mode != CF_LIST_NONE`) and a key of a
      type the language does not read, each a comment where its field would
      be, in C++ `/* tags: a list; its elements are tags.0, tags.1, ... and tags.count */`.
  * The reserved words, written as a sorted `static const char* const`
    table for each language:
    * C++: `alignas alignof and and_eq asm auto bitand bitor bool break case catch char char8_t char16_t char32_t class compl concept const consteval constexpr constinit const_cast continue co_await co_return co_yield decltype default delete do double dynamic_cast else enum explicit export extern false float for friend goto if inline int long mutable namespace new noexcept not not_eq nullptr operator or or_eq private protected public register reinterpret_cast requires return short signed sizeof static static_assert static_cast struct switch template this thread_local throw true try typedef typeid typename union unsigned using virtual void volatile wchar_t while xor xor_eq`
      and the library's own `cf` and the generated `keys`, `define`, `bind`,
      `Contract`.
    * Rust and Python: W5-G2's and W5-G3's.
  * The C++ file, `--name N`:

    ```cpp
    /* The contract of <schema's base name>, in C++: written by `doctor gen
       --schema --lang cpp`. Regenerate it from the schema rather than
       editing it. */

    #ifndef CF_GEN_<N in uppercase>_HPP
    #define CF_GEN_<N in uppercase>_HPP

    #include <configforge/configforge.hpp>

    #include <string>

    <C's "#if !CF_WITH_REAL ... #error" lines, when a key is a real>

    namespace N {

    /* Every definition the schema makes, descriptions, enumerations and
       lengths with the rest, through the library's own calls. */
    inline bool define(cf_builder* b) {
    <gen_c_body>
    }

    /* cf::Builder().define(N::Contract()): a refusal is the builder's,
       and create() reports it. */
    struct Contract {
        void defineInto(cf_builder* b) const {
            (void)define(b);
        }
    };

    /* Each key, to read with cf::Config::get; its default is the one a
       configuration without the key reads as. */
    namespace keys {
    const cf::Key<cf_i64> server_port("server.port", (cf_i64)8080);
    ...
    } /* namespace keys */

    struct Server {
        cf_i64      port;
        std::string host;
        ...
    };

    struct Settings {
        Server server;
        double ratio;
        ...
    };

    /* The fields of Settings, each from its key:
           cf::Binding<N::Settings> b;
           N::bind(b);
           N::Settings s = b.read(cfg); */
    inline void bind(cf::Binding<Settings>& b) {
        b.field("server.port", &Settings::server, &Server::port, (cf_i64)8080);
        b.field("ratio", &Settings::ratio, 0.25);
        ...
    }

    } /* namespace N */

    #endif
    ```

    (`define`'s `ok` is a `cf_bool`; `inline bool define` returns
    `ok != CF_FALSE` - the body's last line, `return ok;`, is the C one, so
    the C++ writer prints `return ok != CF_FALSE;` in its place: the one
    line gen_c_body takes a flag for.) Types: bool `bool`, int `cf_i64`,
    real `double`, text `std::string`. A Key's and a field's default is the
    schema's (`c_i64` for an int, `%.17g` for a real, `c_string` for text,
    `true`/`false`), or the type's zero (`(cf_i64)0`, `0.0`, `false`,
    `std::string()`) for a key with none. Keys carry no range and no flags:
    the contract is `define`'s, and a Key here only names and reads. The
    struct is `Settings` whatever `--name` is (the namespace is the name).
  * Rejected: Keys that define the contract (`cf::Builder::define(Key)`),
    which carry a type, a default and a range and lose the rest; and
    `constexpr` Keys under C++11 - how `doctor gen`'s output is checked at
    compile time is wave 3's item B (D-75), and this card does not decide it.
  * The test, `tests/doctor/test_gen_cpp.cpp`, built where `test_gen.c` is
    (`if(NOT CMAKE_CROSSCOMPILING AND CF_WITH_CXX)`), from
    `${CMAKE_CURRENT_BINARY_DIR}/gen_contract.hpp` generated by
    `doctor gen --schema ${contract} --lang cpp --name gen_contract --out ...`
    in an `add_custom_command`, as C++98 with `-pedantic -Werror -Wall -Wextra`
    (MSVC: `configforge_apply_warnings`), as `tests/unit/CMakeLists.txt`
    builds `test_wrapper_access`; `CF_CONTRACT_SCHEMA` defined as for
    test_gen.c; test `doctor.gen_cpp`. It includes `../unit/cf_test.h` and:
    * `test_same_schema`: one builder with `gen_contract::define(b)` and one
      with `cf_builder_schema_file`, each given `server.token = t` from
      memory and `CF_UNKNOWN_REJECT`, write the same `cf_schema` - as
      test_gen.c.
    * `test_keys`: `cf::Config cfg = cf::Builder().define(gen_contract::Contract()).memory(...).create();`
      with `server.token = t` and `server.port = 9443`;
      `cfg.get(gen_contract::keys::server_port) == 9443`;
      `cfg.get(gen_contract::keys::server_host) == "0.0.0.0"`.
    * `test_binding`: `cf::Binding<gen_contract::Settings> b; gen_contract::bind(b);`
      `gen_contract::Settings s = b.read(cfg);` - `s.server.port == 9443`,
      `s.server.host == "0.0.0.0"`, `!s.server.tls`, and under
      `CF_WITH_REAL` `s.ratio == 0.25`.
    * `test_refusal`: `server.port = 70000` -> `create()` gives a Config
      that is not valid and its error names `server.port`.
* **files**: `tools/doctor/contract.c`, `tools/doctor/doctor.c`,
  `tests/doctor/test_gen_cpp.cpp` (new), `tests/doctor/CMakeLists.txt`;
  last: `docs/Doctor.md`, `docs/Decisions.md`, `CHANGELOG.md`,
  `docs/Roadmap.md`.
* **steps**:
  1. Before any change: `build/default/tools/doctor/doctor gen --schema tests/doctor/contract.schema.json --name gen_contract --out before.c`
     (the path to the built `doctor` is the preset's; on Windows the
     `Debug` directory under it).
  2. The split into `gen_c_body`, and `--lang` with only `c`. Generate
     again into `after.c`; `cmake -E compare_files before.c after.c` exits
     0. Remove both. `ctest --preset default -R doctor.gen` passes.
  3. The test and its build lines, and a `--lang cpp` that writes only the
     guard and an empty namespace. See it fail: `cmake --build --preset default`
     fails compiling `test_gen_cpp.cpp` (`gen_contract::define` undeclared).
  4. The C++ writer. `cmake --build --preset default && ctest --preset default -R "doctor.gen(_cpp)?"`
     passes.
  5. `docs/Doctor.md` section 7d: `--lang` and what each language gets, the
     C++ file's three parts and how a program uses them, in a paragraph and
     a short example (`cf::Builder().define(app::Contract())`, `app::bind`).
  6. ACCEPT-LINUX, ACCEPT-WINDOWS.
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI. `doctor.gen` and
  `doctor.gen_cpp` pass on both.
* **stop if**: step 2's comparison fails and cannot be made to pass without
  changing what C gets (then the split is wrong; revert it); or the
  generated header does not compile as C++98 with `-pedantic -Werror` for a
  reason in the C body (an unused variable warning for a namespace-scope
  `const` Key is the likely one: then the Keys' form is wrong, hand back
  rather than silence the warning).
* **docs**: add **D-95**, "Code for each binding from a contract", in D-80's
  shape: the question (the contract written once, read in each language
  with its own types); the options (each binding's own defining calls; the
  library's C calls; the schema itself); the decision for C and C++ (the C
  calls, whole; Keys and a Binding only to read; the names, the shape and
  its two levels, reserved words, what has no field); compatibility (`--lang c`
  is the default and writes what it wrote). W5-G2 and W5-G3 add Rust's and
  Python's. CHANGELOG, Added: "`doctor gen --lang cpp`: the contract as one
  C++ header - its definitions, a `cf::Key` for each key and a `Settings`
  struct read through `cf::Binding` (D-95)." Roadmap: the row "Code for
  each binding from a contract" stays 🟨, "C++ and a schema from a sample
  done; Rust and Python left".
* **notes**:

---

## W5-G2 - `doctor gen --lang rust`

* **id**: W5-G2
* **goal**: `doctor gen --schema S --lang rust` writes a Rust module with
  the contract, a `Key` constant for each key and a `settings!` struct, and
  the crate's own tests compile and run a generated module.
* **why**: as W5-G1's, in Rust: `cfg.read(&SERVER_PORT)` and
  `Settings::read(&cfg)` from the contract, and a `const` Key whose name the
  compiler checks (D-75) at no cost.
* **needs**: W5-G1
* **size**: large
* **decided**:
  * **TO DECIDE - what the generated Rust and Python define the contract
    with.** C and C++ define it with the library's C calls (W5-G1), which
    Rust and Python cannot make without the crate's hidden `sys` or ctypes.
    Options:
    1. **The schema itself**, embedded in the generated file as a string
       and given to a new `Builder::schema(text, name)`: the whole contract -
       descriptions, enumerations, lengths, lists, reals, deprecations, open
       prefixes - read by the library as the schema file would be; the `Key`
       constants and the `settings!` struct (Python: the dataclass) only
       read. One rule set, the library's (D-67).
    2. **The binding's own defining calls**, `settings!`'s `define` and
       Python's `cf.define`: the Rust and Python way; but they carry a type,
       a default, a range and flags, and drop descriptions, enumerations,
       lengths, lists, open prefixes, and in Rust reals (its `Field` has no
       `f64`) - each named on stderr as the file is generated.
    3. Option 2, with the bindings first taught everything a `cf_def` holds.
       Complete, and a feature of each binding larger than this card.
    Recommended: **1**. The answer goes here; until it is written, step 1
    of the loop stops at this card. The rest of this card, and W5-G3, are
    written for option 1. If the answer is 2 or 3, both cards need
    redesign: stop.
  * `Builder::schema` and `Builder::schema_file`, in the binding (a feature
    the binding should have had: `cf_builder_schema` is 1.2's):
    `bindings/rust/src/sys.rs` declares, beside `cf_builder_key`,
    `pub fn cf_builder_schema(b: *mut cf_builder, text: *const c_char, n: usize, name: *const c_char) -> cf_bool;`
    and `pub fn cf_builder_schema_file(b: *mut cf_builder, path: *const c_char) -> cf_bool;`;
    `bindings/rust/src/lib.rs`, after `define`:

    ```rust
    /// The contract from a JSON Schema's text, as the library reads one
    /// (`cf_builder_schema`): every key it defines, with its type, range,
    /// default, description and spellings. A keyword the library does not
    /// read is refused, by name and place, at [`Builder::create`].
    pub fn schema(mut self, text: &str, name: &str) -> Builder {
        let (t, n) = (self.keep.s(text), self.keep.s(name));
        unsafe { sys::cf_builder_schema(self.b, t, text.len(), n) };
        self
    }

    /// [`Builder::schema`] from a file.
    pub fn schema_file(mut self, path: &str) -> Builder {
        let p = self.keep.s(path);
        unsafe { sys::cf_builder_schema_file(self.b, p) };
        self
    }
    ```

    with a test module `schema_tests` in lib.rs: a schema text with
    `server.port` an integer 1 to 65535, default 8080 - `cfg.int("server.port", 0) == 8080`;
    with `server.port = 70000` from memory, `create()` is `Err` and its
    message names `server.port`.
  * The generated file, `--name N` (default `Settings`; checked as a Rust
    type name: an ASCII letter first, then letters, digits and `_`):

    ```rust
    //! The contract of <schema's base name>, in Rust: written by `doctor
    //! gen --schema --lang rust`. Regenerate it from the schema rather
    //! than editing it.

    use configforge::{Builder, Key};

    /// The schema, the contract's one source: [`contract`] gives it to a
    /// builder, which reads it as it would read the file.
    pub const SCHEMA: &str = "<the schema's text>";

    /// Every definition the schema makes, on `b`.
    pub fn contract(b: Builder) -> Builder {
        b.schema(SCHEMA, "<schema's base name>")
    }

    pub const SERVER_PORT: Key<i64> = Key::new("server.port", 8080);
    pub const SERVER_HOST: Key<String> = Key::new("server.host", "0.0.0.0");
    // ratio: a real, which the Rust binding does not read yet
    // tags: a list; its elements are tags.0, tags.1, ... and tags.count

    configforge::settings! {
        pub struct N {
            pub server_port: i64 = "server.port" => 8080,
            pub server_host: String = "server.host" => "0.0.0.0",
            ...
        }
    }
    ```

    The file has no inner attribute (`#![...]`), so it can be included
    (`include!`) as well as be a module. Types: bool `bool`, int `i64`, text
    `String`; a real is a comment (the binding has no `f64` `Field` or
    `Value`; teaching it one is not this card). Defaults: the schema's, or
    `false`, `0`, `""`; `i64::MIN` and `i64::MAX` for the extremes, written
    so. A constant is the key's identifier in uppercase. Keys carry no range
    or flags (as in C++: the contract is the schema's). One flat struct:
    `settings!` does not nest, so a field is the key's whole identifier.
  * The schema's text as a Rust string literal: its bytes as they are,
    UTF-8 included, but `\` as `\\`, `"` as `\"`, CR LF as LF and a lone CR
    as `\r`, and any other control byte below 0x20 but tab and LF as
    `\u{XX}`. A Rust string literal may span lines, so the schema's lines
    stay lines.
  * Rust's reserved words, for the identifier rule (W5-G1):
    `abstract as async await become box break const continue crate do dyn else enum extern false final fn for gen if impl in let loop macro match mod move mut override priv pub ref return self Self static struct super trait true try type typeof unsafe unsized use virtual where while yield`,
    and the generated `contract` and `SCHEMA`.
  * The test: no cargo in a ctest of `tests/doctor`, so two halves, each
    where its tools are:
    * **A golden file**, `tests/doctor/gen/contract.rs`, generated from
      `tests/doctor/contract.schema.json` with `--lang rust` and checked in.
      `doctor.gen_rust` (`tests/doctor/CMakeLists.txt`, where Doctor runs on
      the build machine) runs `tests/doctor/gen_golden.cmake` with
      `-DDOCTOR -DSCHEMA -DLANG=rust -DGOLDEN -DOUT`: it generates into `OUT`
      and `cmake -E compare_files`; on a difference it fails with
      `the generated Rust differs from tests/doctor/gen/contract.rs: regenerate it with doctor gen --schema tests/doctor/contract.schema.json --lang rust --out tests/doctor/gen/contract.rs, and read the diff`.
    * **The crate compiles and runs it**: `bindings/rust/tests/gen_contract.rs`,
      an integration test (not in the package: `Cargo.toml`'s `include`
      leaves `tests/` out, so `cargo package` does not see the path below):

      ```rust
      #[allow(dead_code)]
      mod generated {
          include!("../../../tests/doctor/gen/contract.rs");
      }
      ```

      and tests: `contract(Builder::new()).memory("m", rank::USER_FILE, &[("server.token", "t")]).create()`
      is `Ok`; `Settings::read(&cfg).server_port == 8080` and
      `.server_host == "0.0.0.0"`; with `("server.port", "9443")` added,
      `cfg.read(&generated::SERVER_PORT) == 9443`; with `"70000"`,
      `create()` is `Err`. It runs in `rust.cargo_test` and CI's cargo job.
* **files**: `bindings/rust/src/sys.rs`, `bindings/rust/src/lib.rs`,
  `tools/doctor/contract.c`, `tests/doctor/gen/contract.rs` (new, generated),
  `tests/doctor/gen_golden.cmake` (new), `tests/doctor/CMakeLists.txt`,
  `bindings/rust/tests/gen_contract.rs` (new); last: `bindings/rust/README.md`,
  `docs/Doctor.md`, `docs/Guide.md` (section 13's Rust part),
  `docs/Decisions.md`, `CHANGELOG.md`, `docs/Roadmap.md`.
* **steps**:
  1. The maintainer's answer is written in this card (loop step 4).
  2. `Builder::schema` and `schema_file` with `schema_tests`, first the test
     calling a `schema` that does not exist: see it fail,
     `cargo test --locked --manifest-path bindings/rust/Cargo.toml schema_tests`
     does not compile; then the methods: it passes.
  3. `gen_golden.cmake`, `doctor.gen_rust`, and `bindings/rust/tests/gen_contract.rs`
     against a hand-written `tests/doctor/gen/contract.rs` holding only the
     comment lines. See them fail: `ctest --preset default -R doctor.gen_rust`
     fails (`--lang rust` is the usage, exit 2), and the cargo test does not
     compile (`contract` not found).
  4. The Rust writer. Generate the golden file with the command the failure
     message gives; read it whole before staging it.
     `ctest --preset default -R doctor.gen_rust` passes; with cargo,
     `cmake --preset default -DCF_RUST=ON && cmake --build --preset default && ctest --preset default -R rust.cargo_test`
     passes, and `cargo clippy --locked --all-targets --manifest-path bindings/rust/Cargo.toml -- -D warnings`
     is clean.
  5. The documents: `docs/Doctor.md` 7d's paragraph gains Rust; the Guide's
     Rust part a sentence and the two calls; `bindings/rust/README.md` one
     sentence and `Builder::schema`.
  6. ACCEPT-LINUX, ACCEPT-WINDOWS; push; ACCEPT-CI (cargo, clippy and the
     package job, which must not see `tests/`).
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI.
* **stop if**: the maintainer answers 2 or 3; or `cf_builder_schema` keeps a
  pointer to `text` past the call and `keep.s` does not keep it as long as
  the builder (read `lib.rs`'s `Keep` first: if it is freed before
  `create`, the method must copy elsewhere - hand back); or clippy refuses
  `settings!`'s expansion for a reason in the macro, not the generated code.
* **docs**: D-95 gains Rust: the schema embedded and why (the maintainer's
  answer), `Builder::schema`, the Keys and the flat struct, reals not yet,
  the golden file and why. CHANGELOG, Added: "`doctor gen --lang rust`: the
  contract as a Rust module - the schema it is read from, a `Key` constant
  for each key and a `settings!` struct (D-95); and `Builder::schema` and
  `schema_file` in the Rust crate." Roadmap: the row's text "C++, Rust and
  a schema from a sample done; Python left".
* **notes**:

---

## W5-G3 - `doctor gen --lang python`

* **id**: W5-G3
* **goal**: `doctor gen --schema S --lang python` writes a Python module
  with the contract and a dataclass of `cf.setting` fields, and a test
  imports the generated module and reads a configuration through it.
* **why**: as W5-G1's, in Python: `cf.read(cfg, Settings).server.port`,
  with the key, the default and the description written once, in the schema.
* **needs**: W5-G2
* **size**: medium
* **decided**:
  * As W5-G2's answer says (option 1 is what is written here): the module
    embeds the schema and defines the contract with it; the dataclasses
    only read.
  * `Builder.schema` and `Builder.schema_file` in the package:
    `bindings/python/src/configforge/_native.py` declares
    `_declare("cf_builder_schema", c_int, B, c_char_p, c_size_t, c_char_p, optional=True)`
    and `_declare("cf_builder_schema_file", c_int, B, c_char_p, optional=True)`
    and sets `HAVE_SCHEMA = hasattr(lib, "cf_builder_schema")`;
    `bindings/python/src/configforge/_config.py`'s `Builder` gains, after
    `define_list`:

    ```python
    def schema(self, text: str, name: str = "<schema>") -> "Builder":
        """The contract from a JSON Schema's text, as the library reads one
        (cf_builder_schema); a keyword it does not read is refused, by name
        and place, at create()."""
        if not n.HAVE_SCHEMA:
            raise RuntimeError("schema needs libconfigforge 1.2 (cf_builder_schema)")
        data = text.encode("utf-8")
        n.lib.cf_builder_schema(self._live(), self._keep.b(data), len(data),
                                self._keep.s(name))
        return self
    ```

    and `schema_file(self, path: str)` the same way; `self._keep.b` is
    whatever `_Keep` keeps bytes with (read the class: if it has only `s`,
    add `b(self, data: bytes)` beside it, keeping the bytes object and
    returning it). Two tests in `tests/python/test_binding.py`, as W5-G2's
    `schema_tests`, skipped unless `native.HAVE_SCHEMA`.
  * The generated module, `--name N` (default `Settings`; checked as a
    Python identifier and not a keyword):

    ```python
    """The contract of <schema's base name>, in Python: written by `doctor
    gen --schema --lang python`. Regenerate it from the schema rather than
    editing it."""

    from __future__ import annotations

    from dataclasses import dataclass

    import configforge as cf

    SCHEMA = (
        "<the schema's first line>\n"
        "<the next>\n"
        ...
    )


    def contract(b: cf.Builder) -> cf.Builder:
        """Every definition the schema makes, on ``b``."""
        return b.schema(SCHEMA, "<schema's base name>")


    @dataclass
    class Server:
        port: int = cf.setting("server.port", 8080, range=(1, 65535),
                               doc="Where the server listens: ...")
        host: str = cf.setting("server.host", "0.0.0.0")
        token: str = cf.setting("server.token", "", flags=cf.flag.SENSITIVE)
        ...


    @dataclass
    class N:
        server: Server = cf.section(Server)
        ratio: float = cf.setting("ratio", 0.25, range=(0, 1))
        # tags: a list; its elements are tags.0, tags.1, ... and tags.count
    ```

    The shape is W5-G1's (top struct, a class per top segment, deeper keys
    flattened into it); a class is defined before the one that uses it.
    Types: bool `bool`, int `int`, real `float`, text `str`. `range` from the
    schema's, `doc` its description, `flags` the definition's (`cf.flag.`
    `REQUIRED`, `SENSITIVE`, `DEPRECATED`, `TRANSIENT`, `RESTART`, joined by
    ` | `): read by nothing when the contract is the schema, but `SENSITIVE`
    keeps the field out of the dataclass's repr (`setting` says so), and the
    rest say what the field is. Defaults: the schema's, or `False`, `0`,
    `0.0`, `""`. The program reads with `cf.read(cfg, N)`; it must not also
    call `cf.define(builder, N)` - the keys are defined already, and the
    module's docstring says so in one sentence.
  * Python's reserved words, for the identifier rule: `False None True and as assert async await break class continue def del elif else except finally for from global if import in is lambda nonlocal not or pass raise return try while with yield`,
    and the generated `contract`, `cf`, `dataclass` and `SCHEMA`.
  * Text as a Python literal: `"`, `\` and the control bytes escaped (`\"`,
    `\\`, `\n`, `\r`, `\t`, `\xNN`), UTF-8 written as it is (a module's
    source is UTF-8 by default), the schema split at its line feeds into one
    literal a line, joined by the parentheses.
  * The test, `tests/doctor/test_gen_python.py`, registered as
    `doctor.gen_python` in `tests/doctor/CMakeLists.txt` under
    `if(BUILD_SHARED_LIBS AND Python3_Interpreter_FOUND)` (ctypes needs a
    shared library, as `python.binding` does), with
    `CONFIGFORGE_LIB=$<TARGET_FILE:configforge>` in its environment and the
    arguments: the package's `src`, the doctor program, the schema, and a
    work directory. It runs `doctor gen --schema <schema> --lang python --out <work>/gen_contract.py`,
    imports that file by `importlib.util.spec_from_file_location`, and
    checks, with `assert`: `contract(cf.Builder()).memory(...)` with
    `server.token = t` creates; `cf.read(cfg, gen.Settings).server.port == 8080`
    and `.server.host == "0.0.0.0"`; with `server.port = 9443`, 9443; with
    `70000`, `create()` raises and the message names `server.port`; and
    `repr()` of a Settings holds no `token`. Exit 0 when all hold.
* **files**: `bindings/python/src/configforge/_native.py`,
  `bindings/python/src/configforge/_config.py`,
  `tests/python/test_binding.py`, `tools/doctor/contract.c`,
  `tests/doctor/test_gen_python.py` (new), `tests/doctor/CMakeLists.txt`;
  last: `bindings/python/README.md`, `docs/Doctor.md`, `docs/Guide.md`
  (section 13's Python part), `docs/Decisions.md`, `CHANGELOG.md`,
  `docs/Roadmap.md`.
* **steps**:
  1. `Builder.schema` and `schema_file`: their tests first, which fail with
     `AttributeError` in a shared build
     (`cmake -DSOURCE=index -DJOBS=3 -DWORK=$HOME/cfv -DEXTRA=shared -P scripts/verify.cmake`,
     or `ctest --test-dir <a shared build> -R python.binding`); then the
     methods: they pass.
  2. `test_gen_python.py` and its registration. See it fail: in the shared
     build, `ctest -R doctor.gen_python` fails (`--lang python` is the
     usage, exit 2).
  3. The Python writer. Rerun step 2: passes.
  4. The documents, as W5-G2's: Doctor.md 7d, the Guide's Python part, the
     package's README.
  5. ACCEPT-LINUX with `-DEXTRA=shared`, ACCEPT-WINDOWS.
* **accept**: ACCEPT-LINUX (and `-DEXTRA=shared`, where `doctor.gen_python`
  and `python.binding` run), ACCEPT-WINDOWS, ACCEPT-CI (the shared job).
* **stop if**: W5-G2's answer is not option 1; or `cf.read` cannot read a
  dataclass defined in a module imported from a path (it resolves type
  hints with `typing.get_type_hints`, which looks in the module's globals:
  with `from __future__ import annotations` the names must be the module's
  own - if a test shows otherwise, hand back).
* **docs**: D-95 gains Python: the schema embedded, `Builder.schema`, the
  dataclasses and what their `range`, `flags` and `doc` are for. CHANGELOG,
  Added: "`doctor gen --lang python`: the contract as a Python module - its
  schema and a dataclass of `cf.setting` fields (D-95); and `Builder.schema`
  and `schema_file` in the Python package." Roadmap: the row "Code for each
  binding from a contract" 🟩, `D-95, D-96`; wave 5 🟩, its text
  "converting between formats (`doctor convert`, D-94); code for each
  binding from a contract (`doctor gen --lang c|cpp|rust|python`, D-95) and
  a contract drafted from a sample (`doctor schema --from`, D-96)".
* **notes**:
