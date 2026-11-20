# Wave 5: converting, and code from a contract

Roadmap wave 5 as cards (docs/Plan.md says what a card is and how one is
carried out). Three pieces, in the two programs the tools split makes
(its cards' record: docs/Plan.md, "Done, their cards removed"):

* **W5-V1**: `configforge codec convert`, a document read through one
  codec and written through another as the codecs' events, every value
  the target cannot hold named and left out, never dropped quietly; and
  `configforge codec format`, the same into its own format. In the
  serialization library's program (T-3). D-94.
* **W5-G1 to W5-G3**: `configforge gen --schema S --lang c|cpp|rust|python`.
  Today it writes C; C++ gets `cf::Key`s and a `cf::Binding`, Rust `Key`
  constants and a `settings!` struct, Python a dataclass of `cf.setting`
  fields, each generated file compiled or imported and run by a test as
  `tests/gen/test_gen.c` does for C. In the contract's program (T-2).
  D-95.
* **W5-S1**: `configforge gen schema --from <file>`, a schema drafted
  from a sample configuration, for whoever has no schema yet. D-96.

Not here: wave 6's "every operation on a file's data, written back in its
own format". `configforge codec convert` writes a new file in another
format; it does not edit one in place, and keeps no comment or layout.

Order: W5-V1, W5-S1, W5-G1, W5-G2, W5-G3, after T-3. W5-V1 is
`tools/codec/`'s and shares nothing with the others, which share
`tools/gen/contract.c`, `tools/gen/gen.c` and the gen section of
`docs/Tools.md`, and so go one after another. Every card is accepted on
Linux and Windows; W5-G2's Rust part runs where cargo is (CI's cargo
job, and `CF_RUST=ON` locally).

Two cards of 1.8.0 follow these, at the end of this file: W5-V2, what
each format can hold and every ordered pair of formats tested against
it (D-109); and W5-G4, generators outside the tree, found as
`configforge-gen-<lang>` on the PATH (D-110). They run after A-1b, so
they name files by the layout of docs/plan/A-architecture.md.

Rewritten 2026-11-16 for the tools split: each card says in its notes
what moved. Where the programs, files and tests are named below, they
are T-1, T-2 and T-3's (`configforge-gen` in `tools/gen/`, tested in
`tests/gen/`; `configforge-codec` in `tools/codec/`, tested in
`tests/codec/`; the documents in `docs/Tools.md`).

D-numbers: this file uses **D-94, D-95 and D-96**, and W5-V2 and W5-G4
**D-109 and D-110** (the wave 4 cards used
D-91 to D-93; the cards of the T file (removed; see docs/Plan.md) skip these
three). Check with `git grep -n "^## D-9[4-6]" docs/Decisions.md` before
writing one; if a number is taken, take the next free one and change it
in every card of this file in the same commit.

The CHANGELOG: W5-V1 to W5-G3 add to `## 1.7.0 — unreleased`, W5-V2 and
W5-G4 to `## 1.8.0 — unreleased` (1.7.0 opened by the first card of
the first T card, 1.8.0 by the first card of 1.8.0 that
writes to it), under `### Added`.

For the maintainer: one question, in W5-G2 (what the generated Rust and
Python define the contract with), answered 2026-11-12 with its
recommendation (docs/Plan.md, "For the maintainer"). Nothing to install.

---

## W5-V1 - `configforge codec convert` and `format`

* **id**: W5-V1
* **goal**: `configforge codec convert <in> --to <format> [--as <format>] [--out <file>] [--strict]`
  reads a document through its codec and writes it through another's
  encoder as the codecs' events, nesting and arrays kept, no key space
  and no contract on the way; every value, array, name or type the
  target cannot hold is a finding on stderr, and under `--strict` a
  finding fails the run and nothing is written.
  `configforge codec format <in> [--as <format>] [--out <file>] [--strict]`
  is `convert` with the document's own format as the target.
* **why**: a configuration moved from INI to TOML by hand, or by a script
  that drops what it does not understand, arrives with values missing and
  nothing to say so; converted here, the new file holds what the old one
  said, and what it could not hold is listed by its path - so the value a
  program later reads from it is the one somebody wrote.
* **needs**: T-3
* **size**: large
* **decided**:
  * **Recorded, then replayed in the order the target needs.** The
    program links the serialization library alone (T-3), which has no
    key space, and the Roadmap asks for a conversion with none on the
    way. The decoder's events go into a tree of nodes in an arena; the
    tree is then walked into the target's encoder. Rejected: the events
    straight into the encoder (a JSON document's later root key would
    land inside an INI section, and a TOML array after a sub-table
    inside that sub-table - the two faults `src/bridge/flatten.c`'s
    comment above `all_digits` records); the key space and
    `cf_doc_emit_where`, this card's design before the split (not in
    the serialization library; and it refused every name that is not a
    lowercase key).
  * Command line, parsed in this order: `argv` after `convert` (or
    `format`); the first argument not starting `--` is `<in>` (`-` is
    standard input, read whole, binary); `--to F` (required for
    `convert`, refused for `format`), `--as F`, `--out F` (each once),
    `--strict` (a flag). Anything else - a second input, a missing
    `--to` - is the usage, exit 2. The usage gains
    `"  configforge codec convert <file> --to <format> [--as <format>] [--out <file>] [--strict]\n"`
    `"                                 the document in another format, and what it cannot hold\n"`
    `"  configforge codec format <file> [--as <format>] [--out <file>] [--strict]\n"`
    `"                                 the document rewritten in its own format\n"`,
    in a call of its own.
  * Reading: `codec_read(in, as, ar, &blob, &src, &how)` (T-3), with its
    errors and exit codes. The target: `--to` by `cf_codec_by_name`,
    with `codec_read`'s two messages for an unknown or left-out format,
    and `codec: format '<name>' can be read but not written` when its
    `encoder` is NULL, exit 2.
  * The tree, `tools/codec/convert.c` (statics `cv_`): a node is
    `{ kind (table, array, value); name and its length, copied, or none
    inside an array; the value, its text copied; textual; parent, first
    and last child, next; left_out }`, all from one heap arena. The
    recording sink: `begin_table` and `begin_array` open a node under
    the current one, `scalar` adds a value, `end` closes; `reserve_text`
    and `reserve_work` lend from the arena (T-3's `check` does the
    same). A name met again in the same table: a table into an earlier
    table of that name is that table reopened (INI's repeated
    sections); anything else replaces the earlier member, which is
    marked left out - the later one wins, as the file means it and as a
    configuration reads it. Names are found by a hash table in the
    arena, keyed by the parent node's index and the name (FNV-1a over
    both, open addressing, doubled at three quarters full), so a table
    of many members costs no more per member than one of few.
  * The bound, D-82's rule made the tool's: each node costs its name's
    length, its text's length and 64 bytes; a document may cost 64
    times its size and 1 MiB more (`CV_FACTOR 64u`, `CV_FLOOR 1048576u`,
    `CV_NODE_COST 64u`, with a comment naming D-82: the library's
    `CF_DECODE_*` are in `cf_flatten.h`, which this program does not
    see). Past it: `codec: <in> makes more than 64 times its size; refused (D-82's bound)`,
    exit 2.
  * The replay, `cv_emit`: in a table, its values first in document
    order, then its arrays, then its tables - the order
    `flatten.c`'s comment gives and why; in an array, its members in
    order; a node left out, and everything under it, not walked.
  * What the target cannot hold, found before anything is written, in
    three passes over the tree; each finding is a line on stderr,
    `codec convert: <path>: <what>, which <format> cannot hold; left out`
    (`codec format:` for `format`), and the node and everything under
    it are left out. The path is the names joined by `.`, an array's
    members by their index; a control byte in it written `\xNN`.
    1. **Names**, when the target is not `json`, `json5` or `cbor`
       (whose encoders write a name as a string: `json.c`'s `js_name`
       through `cf_sink_json_string`; read `cbor.c`'s encoder in step 1
       and stop if it does not): a name that is not one or more of
       `A-Z`, `a-z`, `0-9`, `_` and `-` is
       `a name it would write unquoted ('<name>')` - TOML's encoder
       writes a name as it is (`toml.c`, `toml_enc_lead`), and so do
       INI's and YAML's.
    2. **Arrays**, by the target's caps (`cf_emit`'s rule): with neither
       `CF_CODEC_ARRAYS` nor `CF_CODEC_SCALAR_ARRAYS`, every array,
       `an array`; with `CF_CODEC_SCALAR_ARRAYS` only (INI), an array
       that holds a table, `an array of tables`, an array,
       `an array of arrays`, or nothing, `an empty array`.
    3. **Values**, by the target's encoder itself: the replay into the
       encoder writing into a sink that stores nothing
       (`cf_sink_init(&out, NULL, 0)`), through a wrapping sink that
       passes each event on and, when the encoder refuses one, marks
       its node left out and returns true so the walk goes on (a refused
       container: its events are not passed on until its `end`).
       `<what>` is `a null` for a null, `text with a line break` for
       text holding `\n`, and `a value of this kind` otherwise. This is
       how TOML's null and INI's line break are found, and whatever a
       future encoder refuses, without the program knowing each
       format's rules.
  * Three more things said, once each:
    * `codec convert: note: <format> can hold comments, and no reader keeps them: any in <in> are not in the output`
      when the source is `ini`, `toml`, `yaml`, `json5`, `properties`
      or `xml` (a table in convert.c).
    * `codec convert: note: <format> carries no types, so its values are written to <format> as text`
      when the source lacks `CF_CODEC_TYPED` and the target has it:
      INI's `port = 8080` is the JSON string `"8080"`, faithfully.
    * `codec convert: <format> carries no types: <n> value(s) that were numbers, booleans or nulls are written as their text`
      when the target lacks `CF_CODEC_TYPED` and the third pass passed
      on `n` non-text values (`n > 0`).
    The two notes are not findings: they are about the formats, the
    same for every file, and a `--strict` that failed on them would
    fail every conversion out of TOML. The third is a finding: a
    value's type is lost.
  * Exit codes, as Doctor's and T-3's: **0** converted (with findings,
    when not `--strict`); **1** `--strict` and at least one finding -
    the findings are printed, then
    `codec convert: <n> finding(s); nothing written (--strict)`, and no
    output is made (an `--out` file that existed is left as it was) -
    and also a format that cannot be told (`codec_read`'s 1); **2**
    unusable input or options; **3** the encoder refused, in the
    writing, something the third pass let through -
    `codec convert: <message> (a bug in configforge codec: the check let it through)`
    - an internal error.
  * Writing: the encoder's state on the stack in an aligned union of
    `CF_ENCODER_STATE_MAX` bytes as `src/bridge/emit.c` holds it (a
    larger `encoder_state_size` is `codec: no room for the '<name>' encoder`,
    exit 2; the frame stays under the tools' 2048 at default limits -
    if `-Wstack-usage=2048` says otherwise, the union is `malloc`'d);
    measured first into a NULL sink, then written into `len + 1` bytes
    from the arena. No flag is set in a binary encoder's head: `${` is
    written as read, neither expanded nor doubled - a document is
    converted, not a configuration. `--out F` is opened `"wb"`, written
    whole and closed; a failure is `codec: cannot write <file>`, exit
    2, and the file removed. Without `--out`, standard output, put in
    binary mode on Windows (`_setmode(_fileno(stdout), _O_BINARY)` under
    `#ifdef _WIN32`, as T-3 does for standard input), so a CBOR file
    piped on is its bytes.
  * The code: `tools/codec/convert.c` (new), C89, in
    `tools/codec/CMakeLists.txt`'s list; `codec.h` declares
    `int codec_convert(int argc, char** argv, cf_bool same_format);`
    and `codec.c` dispatches `convert` (false) and `format` (true).
* **files**: `tools/codec/convert.c` (new), `tools/codec/codec.h`,
  `tools/codec/codec.c`, `tools/codec/CMakeLists.txt`,
  `tests/codec/convert.cmake` (new), `tests/codec/CMakeLists.txt`; last:
  `docs/Tools.md`, `docs/Decisions.md`, `CHANGELOG.md`, `docs/Roadmap.md`.
* **steps**:
  1. Read `cbor.c`'s encoder: a name written as a CBOR text string,
     whatever its bytes - else stop (the names pass's exemption is
     wrong). Then `tests/codec/convert.cmake`, run with
     `-DCODEC=<program> -DWORK=<dir> -DCBOR=<ON|OFF>` and registered in
     `tests/codec/CMakeLists.txt` as `codec.convert`. A helper
     `cv_case(<name> <expected-exit> <stderr-regex or "-"> <args>...)`
     runs the program, checks the exit code and the regex, and fails
     with both outputs printed. The cases, every input written with
     `file(WRITE)`:
     * `json_toml`: `a.json` `{"server": {"port": 8080, "host": "web"}, "tags": ["a", "b"]}`
       `convert a.json --to toml --out a.toml`: 0, stderr empty;
       `a.toml` contains `port = 8080` and `host = "web"`; then
       `convert a.toml --to json`: 0, stdout contains `"port": 8080`.
     * `order`: `o.json` `{"s": {"k": 1}, "r": 2}` `--to ini`: stdout's
       `r = 2` comes before `[s]` (the replay's order).
     * `null_toml`: `n.json` `{"a": null, "b": 1}` `--to toml --out n.toml`:
       0, stderr `a: a null, which toml cannot hold; left out`; `n.toml`
       contains `b = 1` and not `a`. With `--strict --out s.toml`: 1,
       the same line and `nothing written`, and `s.toml` does not exist.
     * `tables_ini`: `t.json` `{"k": "v", "n": 5, "s": {"list": [{"x": 1}]}}`
       `--to ini`: 0, stderr `s.list: an array of tables, which ini cannot hold; left out`
       and `ini carries no types: 1 value\(s\)`; stdout has `k = v`.
     * `empty_ini`: `{"e": [], "k": "v"}` `--to ini`: `e: an empty array`.
     * `lines_ini`: `{"m": "a\nb", "k": "v"}` `--to ini`:
       `m: text with a line break, which ini cannot hold; left out`.
     * `names`: `{"Name": 1, "a b": 2}` `--to toml`: 0, stdout has
       `Name = 1`, stderr `a b: a name it would write unquoted`; the
       same `--to json`: both, no finding.
     * `twice`: `d.ini` `[s]\na = 1\n[t]\nc = 0\n[s]\nb = 2\n` `--to json`:
       stdout has `"s"` once, holding `"a": "1"` and `"b": "2"` (a
       section reopened is one table).
     * `ini_json`: `c.ini` `[s]\nk = 8080\n` `--to json`: 0, stderr has
       `note: ini can hold comments` and `note: ini carries no types`;
       stdout has `"k": "8080"`. With `--strict`: still 0.
     * `refs`: `r.json` `{"u": "${HOME}/x", "v": "$${y}"}` `--to toml`:
       stdout has `${HOME}/x` and `$${y}`, each exactly once.
     * `content`: `noext` (no extension) holding `{"a": 1}` `--to toml`:
       0, stdout `a = 1`.
     * `format`: `f.json` `{"b":1,"a":[1,2]}` `format f.json`: 0, the
       output read back by `convert - --as json --to json` from it is
       the same bytes (`cmake -E compare_files`).
     * `cbor` (only with `-DCBOR=ON`): `a.json` `--to cbor --out a.cbor`,
       `a.cbor --to json --out back.json`, and `a.json --to json --out same.json`:
       `back.json` and `same.json` are the same bytes.
     * `bound`: a YAML document whose aliases make more than 64 times
       its size - an anchor of 64 keys and 2,000 aliases of it - `--to json`:
       2, `refused (D-82's bound)` (if YAML's own alias limit trips
       first, its message is the right one: accept either, as
       `test_yaml` does).
     * unusable, each 2: `a.json --to nosuch` (`no format called 'nosuch'`),
       `a.json --to properties` (`can be read but not written`),
       `missing.json --to toml` (`cannot read`), `a.json` with no `--to`,
       `format a.json --to toml`, and `a.json --to toml --bogus`.
  2. A `codec_convert` that is dispatched and returns 2. See it fail:
     `cmake --build --preset default && ctest --preset default -R codec.convert`
     fails on `json_toml` (exit 2, expected 0).
  3. The tree, the replay and the writing with no checks: the cases
     without findings pass; `null_toml` fails (TOML refuses the null
     mid-walk).
  4. The three passes and the notes: every case passes.
  5. `docs/Tools.md`, the codec section: `convert` and `format` with
     two lines each, and a subsection "Converting between formats",
     fifteen to twenty lines: the command, the findings with an example
     (`null_toml`'s), the notes and why they do not fail `--strict`,
     `${` written as read, a name repeated, and that comments are lost.
  6. ACCEPT-LINUX, ACCEPT-WINDOWS.
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI. `codec.convert`
  passes on both; on Windows the `cbor` case proves standard output is
  binary.
* **stop if**: an encoder's refusal in the third pass changes its state
  so that later events are refused too (then the pass reports what is
  not true: hand back with the case); the CBOR encoder does not write
  any name (step 1); or a reader emits one array in two `begin_array`
  runs (the tree would make two arrays of one name, the later
  replacing the earlier: read `flatten.c`'s `fl_begin_array`, which
  restarts the index, and the readers' arrays of tables before step 3 -
  if one does, hand back).
* **docs**: add **D-94**, "Converting between formats", in D-80's shape:
  the question (a document in another format, saying what the target
  cannot hold); the options (through `cf_emit`; decoder to encoder
  directly; through the key space and the encoder; recorded as events
  and replayed); the decision (the last, and why each of the others
  fails - `cf_emit` resolves and doubles `${`, the direct path breaks
  INI and TOML, the key space is not in the serialization library and
  refuses names; the tree, its merge rule and D-82's bound; the three
  passes; findings, notes and what counts under `--strict`; the exit
  codes; `${` as read; comments said once; `format`); compatibility (a
  new command; nothing else changes). CHANGELOG, Added: "`configforge
  codec convert <file> --to <format>`: a document in another format -
  INI, JSON, TOML, YAML, CBOR - and every value, array, name or type the
  target cannot hold named on stderr and left out; `--strict` fails
  rather than write a lossy file; `configforge codec format` rewrites a
  document in its own format (D-94)." Roadmap: the row "Converting
  between formats" 🟩, `D-94`; wave 5's text "converting between
  formats (`configforge codec convert`, the codecs' events recorded and
  replayed into the target's encoder, D-94)"; "The serialization
  library's own tool" 🟩.
* **notes**:
  * 2026-11-16, rewritten for the tools split before anyone carried it
    out: it was `doctor convert` through the key space and
    `cf_doc_emit_where`; the program is now the serialization library's
    (T-3), which has no key space. Kept: the command line, the two
    notes and the finding of lost types, the array and value passes,
    `--strict`, the exit codes, `${` as read, the output in binary.
    Changed: the tree in place of the key space (and so the stop-if on
    `cf_doc_emit_where`'s predicate is gone); a names pass, since names
    are no longer limited to lowercase keys; D-82's bound written in
    the tool; `format` added (the Roadmap's pretty-print, which is this
    with its own format); `doctor_read_keys` is W5-S1's alone, as
    `gen_read_keys`.

---

## W5-S1 - `configforge gen schema --from`: a contract drafted from a sample

* **id**: W5-S1
* **goal**: `configforge gen schema --from <file> [--as <format>] [--title <title>] [--out <file>]`
  writes a JSON Schema with a key for every value the file holds, its type
  from the value, every key optional, no default, and an empty description
  to fill in.
* **why**: a contract is what lets a value be refused with its key and its
  reason instead of read wrong; a program with years of configuration files
  and no contract gets a first one from a file it already has, which
  `configforge gen check` then holds every other file to.
* **needs**: T-2
* **size**: medium
* **decided**:
  * Reading, `tools/gen/contract.c`, declared in `tools/gen/gen.h`:

    ```c
    /* A document read as a configuration file is - its format named by
       `as`, else from its name and then its content (D-73) - into `ks`,
       resolved; `path` "-" is standard input. 0 when read, else the exit
       code (2) with the reason on stderr. `used`, when not NULL, is the
       codec that read it. */
    int gen_read_keys(const char* path, const char* as, cf_arena* ar,
                      cf_keyspace* ks, const cf_codec** used);
    ```

    (`gen.h` then includes `configforge/cf_keyspace.h` and
    `configforge/cf_serialize.h`; `configforge-gen` is built with
    `CF_INTERNAL_API=1`, S-1, so the key space's functions are declared.)
    It is what this card's W5-V1 sibling was to share before the split:
    the codec by `--as` (`cf_codec_by_name`) or
    `cf_codec_detect(NULL, 0, &blob, <path> or NULL for "-", trial, &d)`;
    `cf_ks_init(ks, ar, 64)`, `cf_flatten_init(&fl, ks, 0)`,
    `cf_codec_decode(src, &blob, cf_flatten_sink(&fl), &err)`, then
    `cf_ks_resolve(ks)`. Errors, each to stderr and exit 2:
    `gen: no format called '%s'`;
    `gen: format '%s' is not compiled into this build; turn on %s`
    (`cf_format_option`); `gen: format '%s' can be written but not read`;
    `gen: %s reads as %s and as %s, differently; name its format with --as`;
    `gen: cannot tell the format of %s; name it with --as`;
    `gen: cannot read %s`; `gen: %s` with the reader's own message; and
    `gen: '%s' holds a key this build cannot hold` when the flattener
    could not (a key with a capital letter: a file the library cannot
    read as a configuration is one it cannot draft a contract from).
  * The walk: `cf_doc_emit(&ks, &sink, &err)` into a sink of contract.c's
    own (`sd_` statics) that keeps the path, as conform.c's does:
    * a scalar in a table is a definition at its path: its type is the
      value's (`int`, `real`, `bool`, `text`), except that text whose
      candidate is textual - `cf_ks_find(ks, path, len)->selected->flags & CF_CAND_TEXTUAL`,
      a value from INI, YAML's plain scalars, `.properties` - is typed by
      `cf_infer_scalar`, the library's Tier 0 inference (D-24), so INI's
      `port = 8080` is an integer; a null is left out:
      `gen schema: <key>: a null, whose type the sample does not show; left out`;
    * an array of scalars of one type is a list of that type; an array
      that holds a table, an array, values of more than one type, or nothing
      is left out: `gen schema: <key>: an array of tables, which a contract has no type for; left out`,
      `... an array of arrays, ...`, `... an array of mixed types, which a contract has no type for; left out`,
      `... an empty array, whose element type the sample does not show; left out`.
  * The definitions: `cf_defset_init(&ds, ar, 0)`; each with `cf_def_init`,
    `d.description = ""`; a list with the fields `src/bridge/schema.c` sets
    when it reads `{"type": "array", "items": {"type": T}}` with no default
    (read its array branch, near `sn_child(s, "items")`, and set the same
    `list_mode` and `list_separator`); `cf_define(&ds, &d, &err)`, a refusal
    printed `gen: %s` with exit 2. No default (a sample's value is one
    deployment's choice, not the program's default, and written as one every
    other deployment would inherit it without a word); no range; no
    `required`, which `cf_schema_write` never writes anyway.
  * The writing: `cf_schema_write(&ds, title, CF_TRUE, ...)`, measured then
    written - closed, so `configforge gen check` refuses a misspelt key
    against it, and the library's own writer, so the draft reads back as
    the definitions it was made of. `title` is `--title`, else the file's
    base name without its extension (`app` for `conf/app.toml`; `stdin`
    for `-`). `--out F` (`"w"`; removed on failure) or standard output.
  * Then, on stderr, one line:
    `gen schema: a draft from <file>: <n> key(s), every one optional, with no defaults, ranges or descriptions; review it before it is the contract`.
  * Exit codes: 0 written (lines left out do not change it: they are on
    stderr); 2 unusable input or options. No 1: there is nothing to fail.
  * The code: `int gen_schema(int argc, char** argv);` in
    `tools/gen/contract.c`, declared in `gen.h` after `gen_code`;
    `gen.c` dispatches `argv[1]` `schema` to `gen_schema(argc - 2, argv + 2)`;
    its usage gains, after the `check` lines,
    `"  configforge gen schema --from <file> [--as <format>] [--title <t>] [--out <file>]\n"`
    `"                                 a draft contract from a sample file\n"`.
    Doctor has no alias for it: it was never Doctor's.
* **files**: `tools/gen/contract.c`, `tools/gen/gen.h`, `tools/gen/gen.c`,
  `tests/gen/schema.cmake` (new), `tests/gen/CMakeLists.txt`; last:
  `docs/Tools.md`, `docs/Decisions.md`, `CHANGELOG.md`, `docs/Roadmap.md`.
* **steps**:
  1. `tests/gen/schema.cmake`, run with `-DGEN=$<TARGET_FILE:configforge_gen> -DWORK=<dir>`
     and registered in `tests/gen/CMakeLists.txt` as `gen.schema`, with
     a helper as W5-V1's `cv_case`. Cases:
     * `toml`: `sample.toml`
       `debug = true\nratio = 0.5\ntags = ["a", "b"]\n[server]\nport = 8080\nhost = "web"\n`
       `schema --from sample.toml --out s.json`: 0; `s.json` matches
       `"port": \{ "type": "integer", "description": "" \}` and
       `"host": \{ "type": "string", "description": "" \}`, has `"boolean"`,
       `"number"` and `"array"`, and not `"default"` or `"required"`; stderr
       has `a draft from sample.toml: 5 key\(s\)`; its title is `"sample"`.
     * `holds`: `check --schema s.json sample.toml`: 0. A
       `typo.toml` of `[server]\nprot = 1\n`: `check --schema s.json typo.toml`: 1.
     * `gen`: `--schema s.json --out s.c`: 0 (the draft is a contract the
       C writer can write).
     * `ini`: `sample.ini` `[server]\nport = 8080\nname = web\n`: `port`
       is an integer and `name` a string.
     * `left_out`: `odd.json`
       `{"n": null, "rows": [{"a": 1}], "mix": [1, "a"], "none": [], "k": 1}`:
       0, stderr has each of the four `left out` lines, and the schema has
       `"k"` and none of the four.
     * unusable, each 2: `schema` with no `--from`; `--from missing.toml`;
       `--from sample.toml --as nosuch` (`no format called 'nosuch'`).
  2. A `gen_schema` that returns 2. See it fail:
     `cmake --build --preset default && ctest --preset default -R gen.schema`
     fails on `toml`.
  3. `gen_read_keys` and the command. Rerun step 2: passes.
  4. `docs/Tools.md`, the gen section: the command and a paragraph - what
     it infers and from what, what it leaves out, no defaults and why,
     closed, "review it".
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
  writer); compatibility (a new command). CHANGELOG, Added: "`configforge gen schema --from <file>`:
  a first JSON Schema contract drafted from a configuration file you
  already have - a key for every value, typed from it, every one optional,
  descriptions left to fill in (D-96)." Roadmap: the row "Code for each
  binding from a contract" 🟨 with "a schema drafted from a sample done
  (D-96); C++, Rust and Python left".
* **notes**:
  * 2026-11-16, rewritten for the tools split before anyone carried it
    out: `doctor schema --from` is `configforge gen schema --from`, in
    `tools/gen/`; it read through W5-V1's `doctor_read_keys`, which is
    now its own `gen_read_keys` (W5-V1 has no key space any more), so
    it needs T-2 and not W5-V1. The design is otherwise unchanged.

---

## W5-G1 - `configforge gen --lang cpp`

* **id**: W5-G1
* **goal**: `configforge gen --schema S --lang cpp` writes one C++ header with
  the contract's definitions, a `cf::Key` for each key and a struct filled
  through a `cf::Binding`, and `--lang c` (the default) writes exactly what
  `configforge gen` writes today.
* **why**: a C++ program reads `cfg.get(contract::keys::server_port)` and a
  `Settings` struct whose fields cannot drift from the contract they were
  generated from, where today it writes the key names and defaults out a
  second time by hand - the repetition a contract exists to end.
* **needs**: W5-S1
* **size**: large
* **decided**:
  * Options: `--lang c|cpp|rust|python`, default `c`; a name the language
    table (next) does not hold is refused as it says. `--name` names what the language calls the generated
    thing: C the function (default `cf_contract`), C++ the namespace
    (default `cf_contract`), Rust and Python the top struct or dataclass
    (default `Settings`, W5-G2 and W5-G3). Each is checked as an identifier
    of its language (`c_identifier` for C and C++; W5-G2 and W5-G3 say
    theirs). Usage, in `tools/gen/gen.c`'s usage:
    `"  configforge gen --schema <schema> [--lang c|cpp|rust|python] [--name <name>] [--out <file>]\n"`.
  * **The languages are a table** (the maintainer, 2026-11-19): in
    `tools/gen/gen.c`, `static const gen_lang gen_langs[]`, each entry
    `{ name, file extension, identifier check, reserved words, writer }`,
    C the first, made in step 2. `--lang` finds its entry there and
    nothing else names a language: no `strcmp(lang, "cpp")` outside the
    table. A name not in it: `gen: no language '%s'; there are: c, cpp`
    (the table's names in its order), exit 2, in place of the usage.
    W5-G2 and W5-G3 each add one entry and change nothing around it.
    A generator outside the tree (`configforge-gen-<lang>` on the PATH,
    handed the model as JSON, as git and cargo find their subcommands)
    is W5-G4, after W5-G3, a card not yet written.
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
      refused: `gen: '%s' and '%s' are both '%s' in %s; rename one`, exit 2.
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
    /* The contract of <schema's base name>, in C++: written by `configforge
       gen --schema --lang cpp`. Regenerate it from the schema rather than
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
    `constexpr` Keys under C++11 - how `configforge gen`'s output is checked at
    compile time is wave 3's item B (D-75), and this card does not decide it.
  * The test, `tests/gen/test_gen_cpp.cpp`, built where `test_gen.c` is
    (`if(NOT CMAKE_CROSSCOMPILING AND CF_WITH_CXX)`), from
    `${CMAKE_CURRENT_BINARY_DIR}/gen_contract.hpp` generated by
    `COMMAND configforge_gen --schema ${contract} --lang cpp --name gen_contract --out ...`
    in an `add_custom_command` (`DEPENDS configforge_gen`), as C++98 with `-pedantic -Werror -Wall -Wextra`
    (MSVC: `configforge_apply_warnings`), as `tests/unit/CMakeLists.txt`
    builds `test_wrapper_access`; `CF_CONTRACT_SCHEMA` defined as for
    test_gen.c; test `gen.cpp`. It includes `../unit/cf_test.h` and:
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
* **files**: `tools/gen/contract.c`, `tools/gen/gen.c`,
  `tests/gen/test_gen_cpp.cpp` (new), `tests/gen/CMakeLists.txt`;
  last: `docs/Tools.md`, `docs/Decisions.md`, `CHANGELOG.md`,
  `docs/Roadmap.md`.
* **steps**:
  1. Before any change: `build/default/tools/bin/configforge-gen --schema tests/doctor/contract.schema.json --name gen_contract --out before.c`
     (the path to the built `configforge-gen` is the preset's; on Windows the
     `Debug` directory under it).
  2. The split into `gen_c_body`, and the language table with only `c`,
     `--lang` read through it. Generate
     again into `after.c`; `cmake -E compare_files before.c after.c` exits
     0. Remove both. `ctest --preset default -R "gen\\.c$"` passes.
  3. The test and its build lines, and a `--lang cpp` that writes only the
     guard and an empty namespace. See it fail: `cmake --build --preset default`
     fails compiling `test_gen_cpp.cpp` (`gen_contract::define` undeclared).
  4. The C++ writer. `cmake --build --preset default && ctest --preset default -R "gen\\.(c|cpp)$"`
     passes.
  5. the gen section of `docs/Tools.md`: `--lang` and what each language gets, the
     C++ file's three parts and how a program uses them, in a paragraph and
     a short example (`cf::Builder().define(app::Contract())`, `app::bind`).
  6. ACCEPT-LINUX, ACCEPT-WINDOWS.
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI. `gen.c` and
  `gen.cpp` pass on both.
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
  Python's. CHANGELOG, Added: "`configforge gen --lang cpp`: the contract as one
  C++ header - its definitions, a `cf::Key` for each key and a `Settings`
  struct read through `cf::Binding` (D-95)." Roadmap: the row "Code for
  each binding from a contract" stays 🟨, "C++ and a schema from a sample
  done; Rust and Python left".
* **notes**:
  * 2026-11-16, rewritten for the tools split before anyone carried it
    out: `doctor gen` is `configforge gen` (T-2), the code in `tools/gen/`,
    the tests in `tests/gen/` and named `gen.*`, the document the gen
    section of `docs/Tools.md`. The design is unchanged.

---

## W5-G2 - `configforge gen --lang rust`

* **id**: W5-G2
* **goal**: `configforge gen --schema S --lang rust` writes a Rust module with
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
    //! The contract of <schema's base name>, in Rust: written by `configforge
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
  * The test: no cargo in a ctest of `tests/gen`, so two halves, each
    where its tools are:
    * **A golden file**, `tests/gen/contract.rs`, generated from
      `tests/doctor/contract.schema.json` with `--lang rust` and checked in.
      `gen.rust` (`tests/gen/CMakeLists.txt`, where the program runs on
      the build machine) runs `tests/gen/golden.cmake` with
      `-DGEN -DSCHEMA -DLANG=rust -DGOLDEN -DOUT`: it generates into `OUT`
      and `cmake -E compare_files`; on a difference it fails with
      `the generated Rust differs from tests/gen/contract.rs: regenerate it with configforge gen --schema tests/doctor/contract.schema.json --lang rust --out tests/gen/contract.rs, and read the diff`.
    * **The crate compiles and runs it**: `bindings/rust/tests/gen_contract.rs`,
      an integration test (not in the package: `Cargo.toml`'s `include`
      leaves `tests/` out, so `cargo package` does not see the path below):

      ```rust
      #[allow(dead_code)]
      mod generated {
          include!("../../../tests/gen/contract.rs");
      }
      ```

      and tests: `contract(Builder::new()).memory("m", rank::USER_FILE, &[("server.token", "t")]).create()`
      is `Ok`; `Settings::read(&cfg).server_port == 8080` and
      `.server_host == "0.0.0.0"`; with `("server.port", "9443")` added,
      `cfg.read(&generated::SERVER_PORT) == 9443`; with `"70000"`,
      `create()` is `Err`. It runs in `rust.cargo_test` and CI's cargo job.
* **files**: `bindings/rust/src/sys.rs`, `bindings/rust/src/lib.rs`,
  `tools/gen/contract.c`, `tests/gen/contract.rs` (new, generated),
  `tests/gen/golden.cmake` (new), `tests/gen/CMakeLists.txt`,
  `bindings/rust/tests/gen_contract.rs` (new); last: `bindings/rust/README.md`,
  `docs/Tools.md`, `docs/Guide.md` (section 13's Rust part),
  `docs/Decisions.md`, `CHANGELOG.md`, `docs/Roadmap.md`.
* **steps**:
  1. The maintainer's answer is written in this card (loop step 4).
  2. `Builder::schema` and `schema_file` with `schema_tests`, first the test
     calling a `schema` that does not exist: see it fail,
     `cargo test --locked --manifest-path bindings/rust/Cargo.toml schema_tests`
     does not compile; then the methods: it passes.
  3. `golden.cmake`, `gen.rust`, and `bindings/rust/tests/gen_contract.rs`
     against a hand-written `tests/gen/contract.rs` holding only the
     comment lines. See them fail: `ctest --preset default -R gen.rust`
     fails (`--lang rust` is the usage, exit 2), and the cargo test does not
     compile (`contract` not found).
  4. The Rust writer. Generate the golden file with the command the failure
     message gives; read it whole before staging it.
     `ctest --preset default -R gen.rust` passes; with cargo,
     `cmake --preset default -DCF_RUST=ON && cmake --build --preset default && ctest --preset default -R rust.cargo_test`
     passes, and `cargo clippy --locked --all-targets --manifest-path bindings/rust/Cargo.toml -- -D warnings`
     is clean.
  5. The documents: the gen section of `docs/Tools.md`'s paragraph gains Rust; the Guide's
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
  the golden file and why. CHANGELOG, Added: "`configforge gen --lang rust`: the
  contract as a Rust module - the schema it is read from, a `Key` constant
  for each key and a `settings!` struct (D-95); and `Builder::schema` and
  `schema_file` in the Rust crate." Roadmap: the row's text "C++, Rust and
  a schema from a sample done; Python left".
* **notes**:
  * 2026-11-16, rewritten for the tools split before anyone carried it
    out: `doctor gen` is `configforge gen` (T-2), the code in `tools/gen/`,
    the tests in `tests/gen/` and named `gen.*`, the document the gen
    section of `docs/Tools.md`. The design is unchanged.

---

## W5-G3 - `configforge gen --lang python`

* **id**: W5-G3
* **goal**: `configforge gen --schema S --lang python` writes a Python module
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
    """The contract of <schema's base name>, in Python: written by `configforge
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
  * The test, `tests/gen/test_gen_python.py`, registered as
    `gen.python` in `tests/gen/CMakeLists.txt` under
    `if(BUILD_SHARED_LIBS AND Python3_Interpreter_FOUND)` (ctypes needs a
    shared library, as `python.binding` does), with
    `CONFIGFORGE_LIB=$<TARGET_FILE:configforge>` in its environment and the
    arguments: the package's `src`, the configforge-gen program, the schema, and a
    work directory. It runs `configforge gen --schema <schema> --lang python --out <work>/gen_contract.py`,
    imports that file by `importlib.util.spec_from_file_location`, and
    checks, with `assert`: `contract(cf.Builder()).memory(...)` with
    `server.token = t` creates; `cf.read(cfg, gen.Settings).server.port == 8080`
    and `.server.host == "0.0.0.0"`; with `server.port = 9443`, 9443; with
    `70000`, `create()` raises and the message names `server.port`; and
    `repr()` of a Settings holds no `token`. Exit 0 when all hold.
* **files**: `bindings/python/src/configforge/_native.py`,
  `bindings/python/src/configforge/_config.py`,
  `tests/python/test_binding.py`, `tools/gen/contract.c`,
  `tests/gen/test_gen_python.py` (new), `tests/gen/CMakeLists.txt`;
  last: `bindings/python/README.md`, `docs/Tools.md`, `docs/Guide.md`
  (section 13's Python part), `docs/Decisions.md`, `CHANGELOG.md`,
  `docs/Roadmap.md`.
* **steps**:
  1. `Builder.schema` and `schema_file`: their tests first, which fail with
     `AttributeError` in a shared build
     (`cmake -DSOURCE=index -DJOBS=3 -DWORK=$HOME/cfv -DEXTRA=shared -P scripts/verify.cmake`,
     or `ctest --test-dir <a shared build> -R python.binding`); then the
     methods: they pass.
  2. `test_gen_python.py` and its registration. See it fail: in the shared
     build, `ctest -R gen.python` fails (`--lang python` is the
     usage, exit 2).
  3. The Python writer. Rerun step 2: passes.
  4. The documents, as W5-G2's: the gen section of Tools.md, the Guide's Python part, the
     package's README.
  5. ACCEPT-LINUX with `-DEXTRA=shared`, ACCEPT-WINDOWS.
* **accept**: ACCEPT-LINUX (and `-DEXTRA=shared`, where `gen.python`
  and `python.binding` run), ACCEPT-WINDOWS, ACCEPT-CI (the shared job).
* **stop if**: W5-G2's answer is not option 1; or `cf.read` cannot read a
  dataclass defined in a module imported from a path (it resolves type
  hints with `typing.get_type_hints`, which looks in the module's globals:
  with `from __future__ import annotations` the names must be the module's
  own - if a test shows otherwise, hand back).
* **docs**: D-95 gains Python: the schema embedded, `Builder.schema`, the
  dataclasses and what their `range`, `flags` and `doc` are for. CHANGELOG,
  Added: "`configforge gen --lang python`: the contract as a Python module - its
  schema and a dataclass of `cf.setting` fields (D-95); and `Builder.schema`
  and `schema_file` in the Python package." Roadmap: the row "Code for each
  binding from a contract" 🟩, `D-95, D-96`; wave 5 🟩, its text
  "converting between formats (`configforge codec convert`, D-94); code for each
  binding from a contract (`configforge gen --lang c|cpp|rust|python`, D-95) and
  a contract drafted from a sample (`configforge gen schema --from`, D-96)".
* **notes**:
  * 2026-11-16, rewritten for the tools split before anyone carried it
    out: `doctor gen` is `configforge gen` (T-2), the code in `tools/gen/`,
    the tests in `tests/gen/` and named `gen.*`, the document the gen
    section of `docs/Tools.md`. The design is unchanged.

---

## W5-V2 - Every pair of formats, and what each can hold

* **id**: W5-V2
* **goal**: `cf_format_holds` says, for each built-in format with a
  writer, what it can hold; `configforge codec convert` names what a
  target cannot hold from that one table; a test takes every document of
  the conformance suites, the project's own fixtures and a list of
  edge documents through every ordered pair of the six writing formats
  and requires the result to be what the table predicts; a fuzz target
  does the same for a random chain; and the table of what each pair
  loses is generated into `docs/Formats.md` and held to the program.
* **why**: a file converted from one format to another is trusted
  because the new file says what the old one said. Today what a format
  cannot hold is written in three places that do not know each other
  (`g_formats` in `lib/config/tests/test_roundtrip.c`, W5-V1's three passes,
  and each encoder's refusals), and two losses fall between them: an
  empty table is written as nothing by TOML and INI, and a null is
  counted as text for YAML, which writes and reads it as a null. One
  table, tested on every pair, closes both.
* **needs**: A-1b, W5-V1 (this card's tests live in `lib/serialize/tests/`
  and `lib/config/tests/`, which A-1b makes)
* **size**: large
* **decided**:
  * **The capabilities are a second word of bits, public and
    experimental (D-109).** Rejected: more `CF_CODEC_*` caps (caps say
    what a codec does with the pipeline - read, write, typed, arrays;
    these say what the format's syntax can carry, and a codec a program
    registers has no such word); a member added to `cf_codec` (a codec
    of the program's own would have to fill it, and `cf_codec_has_self`'s
    size guard would grow another case); a table kept inside the tools
    (convert and the test would then each keep a copy, which is the
    fault this card ends). The table is `cf_formats[]` in
    `lib/serialize/src/codec.c:289`:
    `cf_format_info` gains a last member `unsigned holds`, so it is
    beside each format's name, extensions and option, and is known
    for a format a build left out. In `lib/serialize/include/configforge/cf_serialize.h`
    (after the `CF_CODEC_*`
    enum, :184), exactly:

    ```c
    /* What a format's syntax can carry, the words `holds` of
       cf_format_holds is made of. A format that lacks a bit does not
       fail to write: a type it lacks is written as the value's text
       (the reader gives text back), and anything else it lacks is left
       out by the converter, which names it (D-109). Experimental in 1.8. */
    enum {
        CF_HOLDS_INT           = 1u << 0,  /* integers as integers         */
        CF_HOLDS_REAL          = 1u << 1,
        CF_HOLDS_BOOL          = 1u << 2,
        CF_HOLDS_NULL          = 1u << 3,
        CF_HOLDS_ARRAYS        = 1u << 4,  /* arrays of values             */
        CF_HOLDS_EMPTY_ARRAY   = 1u << 5,
        CF_HOLDS_TABLE_ARRAYS  = 1u << 6,  /* a table inside an array      */
        CF_HOLDS_NESTED_ARRAYS = 1u << 7,  /* an array inside an array     */
        CF_HOLDS_EMPTY_TABLE   = 1u << 8,  /* a table with nothing in it,
                                              as a member of a table       */
        CF_HOLDS_ANY_NAME      = 1u << 9,  /* not only A-Z a-z 0-9 _ and - */
        CF_HOLDS_EMPTY_NAME    = 1u << 10,
        CF_HOLDS_LONG_PATHS    = 1u << 11, /* a table whose parent's path
                                              and own name together pass
                                              CF_KEY_MAX - 2 bytes         */
        CF_HOLDS_LINE_BREAK    = 1u << 12, /* a line feed in a text        */
        CF_HOLDS_NUL           = 1u << 13, /* a NUL byte in a text         */
        CF_HOLDS_ALL           = (1u << 14) - 1u,
        /* Set when a format that lacks CF_HOLDS_NULL refuses a null
           outright (TOML); without it a null is written as the text
           `null` (INI). */
        CF_REFUSES_NULL        = 1u << 16
    };

    /* CF_TRUE and *holds the bits of the format called `format`, whether
       or not this build has its codec; CF_FALSE, *holds untouched, for a
       format this library has no writer for (xml, properties) and for a
       name it does not know. (1.8, experimental) */
    CF_EXPERIMENTAL(1, 8) cf_bool cf_format_holds(const char* format, unsigned* holds);
    ```

    `cf_format_holds` finds the entry as `cf_format_option` does
    (`strcmp`, today :322) and returns CF_FALSE for an entry whose
    `holds` is 0. It goes in `cmake/configforge.experimental.symbols` as
    `cf_format_holds 1.8` (gate.abi with `-DWRITE=ON`); it is in
    `CF_SERIALIZE_HEADERS`' header, so the serialization library has it,
    and `docs/API.md` is regenerated. No binding takes it (S-2's rule).
  * **The values**, one line a format, from the encoders and readers
    as they are today (each cell with the line that makes it so; the
    rule for a cell the test contradicts is under "Cells the test
    contradicts"):

    | bit | ini | json, json5 | toml | yaml | cbor |
    | --- | --- | --- | --- | --- | --- |
    | INT, REAL, BOOL | no | yes | yes | no | yes |
    | NULL | no (text) | yes | no (refused) | yes | yes |
    | ARRAYS | yes | yes | yes | yes | yes |
    | EMPTY_ARRAY, TABLE_ARRAYS, NESTED_ARRAYS | no | yes | yes | yes | yes |
    | EMPTY_TABLE | no | yes | no | yes | yes |
    | ANY_NAME | no | yes | no | yes | yes |
    | EMPTY_NAME | no | yes | no | no | yes |
    | LONG_PATHS | no | yes | no | yes | yes |
    | LINE_BREAK | no | yes | yes | yes | yes |
    | NUL | yes | yes | yes | yes | yes |

    The reasons: INI's and YAML's readers hand over untyped text
    (`ini.c:535`, `:560` pass `textual` true; `yaml.c:591` passes the
    plain-scalar flag), so what is not text comes back as text, and
    `CF_CODEC_TYPED` is absent from both (`ini.c:775`, `yaml.c:1374`),
    but YAML writes a null as `null` (`yaml.c:1319`) and reads a plain
    `null` as one (`yaml.c:578`), so YAML holds NULL and INI does not;
    TOML's encoder refuses a null (`toml.c:1787`), so TOML has
    `CF_REFUSES_NULL`; INI has syntax for arrays of values only
    (`ini.c:641`, `CF_CODEC_SCALAR_ARRAYS`); TOML and INI write a
    table's header only at its first leaf (`toml.c:1769`,
    `ini.c:632` with `header_pending`), so a table with nothing in it,
    or holding only such tables, is written as nothing; INI and TOML
    write a name as it is and a table's path in a `char path[CF_KEY_MAX]`
    (`toml.c`'s `toml_enc_table` and `ini.c`'s `ini_enc_push` refuse a
    table when `plen + n + 2 > sizeof path`, where `plen` is the length
    of the path above it, dots included, and `n` its own name's: the
    test leaves the new dot out, so it is exact in terms of `plen + n`,
    not of the whole path. A table is refused when `plen + n` is more
    than `CF_KEY_MAX - 2`: at the default 256, a top-level name of 254
    bytes is written and one of 255 refused; under a parent `a`
    (`plen` 1) a name of 253 is written, for a path of 255, and one of
    254 refused), JSON and CBOR write a name as a string and YAML
    quotes what is not a bare word (`yaml.c`, `yaml_key`), so YAML
    holds any name; YAML's reader refuses an empty key (`yaml.c:563`);
    INI's encoder refuses a line feed (`ini.c:687`). `CF_HOLDS_NUL` is
    set for all six on the same reading (INI's reader and writer carry
    pointers and lengths, not C strings) and is the cell most likely to
    be contradicted.
  * **Considered, and not bits.** NaN and the infinities: the library
    holds no real that is not finite (`numeric.c:586`; JSON5's
    `Infinity` and `NaN`, TOML's `inf` and `nan` and CBOR's float
    infinities are read as the text `inf`, `nan`; `json.c:614`,
    `cbor.c:13`), so a pair cannot lose one; the edge document
    `non_finite` shows it is text in every format. A mixed-type array:
    all six hold it. Key order: the converter writes a table's values,
    then its arrays, then its tables (W5-V1), so no pair keeps the order
    of keys and the comparison below ignores it; the order of an
    array's members is kept and compared. A key that is not text:
    no reader gives one (CBOR's integer key and YAML's non-scalar key
    are refused by name, docs/Conformance.md). Nesting depth: every
    reader and writer allows the document and `CF_MAX_DEPTH - 1` levels
    inside it (`json.c:984` and `cbor.c:818` say so; `toml.c:152`
    and the YAML encoder's `yaml_enc_open` are the same count), so a
    table nested 31 deep passes every pair and one nested 32 deep is
    read by none; the `limits` check below holds all six to it.
  * **The converter reads the table** (`tools/codec/convert.c`, W5-V1;
    after this card its tree, passes, replay and writing are in
    `tools/codec/convert_core.c`, below). For a target `cf_format_holds`
    knows, W5-V1's passes 1 and 2 are written from the bits, and pass 3
    keeps the encoder trial as the net under them; for any other target
    (a codec a program registered cannot be a `--to` of this program, but
    the core takes any `cf_codec`) passes 1 and 2 stay as W5-V1 wrote
    them. The findings, each a line
    `codec convert: <path>: <what>, which <format> cannot hold; left out`,
    with `<what>` from the bit that is missing:

    | bit missing | `<what>` |
    | --- | --- |
    | ANY_NAME | `a name it would write unquoted ('<name>')` (W5-V1's) |
    | EMPTY_NAME | `an empty name` |
    | ARRAYS | `an array` |
    | EMPTY_ARRAY | `an empty array` |
    | TABLE_ARRAYS | `an array of tables` |
    | NESTED_ARRAYS | `an array of arrays` |
    | EMPTY_TABLE | `an empty table` |
    | LONG_PATHS | `a table path too long (the path above it and its name pass <CF_KEY_MAX - 2> bytes)` |
    | LINE_BREAK | `text with a line break` (W5-V1's) |
    | NUL | `text with a NUL byte` |
    | NULL, with CF_REFUSES_NULL | `a null` (W5-V1's) |

    An empty name is judged first: when EMPTY_NAME is missing its finding
    is `an empty name`, whatever ANY_NAME says (TOML and INI lack both),
    and ANY_NAME's finding is for a name that is not empty. `CV_NAME` is
    only the second kind, `CV_EMPTY_NAME` the first.

    Judged on what the document held before anything was left out: an
    array that holds a table is "an array of tables" even if a name pass
    left that table out, so the findings do not depend on the order of
    the passes (W5-V1's text says its passes mark nodes; this card
    makes pass 2 read the original children, and the test checks it).
    An empty table is one with no child in the tree as read; a table
    that is empty only because its members were left out is not a second
    finding. The count of values written as text (W5-V1's third
    sentence, `<format> carries no types: <n> value(s) that were ...`)
    is n = every integer, real or boolean when the bit is missing, and
    every null when NULL is missing and CF_REFUSES_NULL is not set: for
    YAML that is no null (W5-V1's rule, "the target lacks CF_CODEC_TYPED",
    counted them), for INI every one. The sentence names the kinds
    counted: `... : <n> value(s) that were numbers, booleans or nulls ...`
    stays word for word; only the count changes.
  * **The core, `tools/codec/convert_core.c` and `convert_core.h`**, C89,
    in `tools/codec/CMakeLists.txt`'s list. W5-V1's tree (`cv_` statics),
    its three passes, the replay and the measured-then-written encoding
    move here, behind one function, so the test and the fuzz target run
    the converter without a process and without parsing its messages:

    ```c
    typedef enum cv_kind {
        CV_NAME = 1, CV_EMPTY_NAME, CV_ARRAY, CV_EMPTY_ARRAY,
        CV_TABLE_ARRAY, CV_NESTED_ARRAY, CV_EMPTY_TABLE, CV_LONG_PATH,
        CV_NULL, CV_LINE_BREAK, CV_NUL, CV_OTHER /* a refusal the table
                                                    did not predict */
    } cv_kind;
    typedef struct cv_finding {
        cv_kind     kind;
        const char* path; /* as the message prints it, in the arena */
    } cv_finding;
    typedef struct cv_result {
        const cv_finding* findings;
        size_t            nfindings;
        size_t            as_text; /* values written as their text */
        cf_blob           out;     /* the target's bytes, in the arena */
    } cv_result;

    /* `in` read through `from`, written through `to`: the tree, the
       passes, the replay. 0 when written, with *res set; else the exit
       code convert would give (2 for a bound, 3 for a refusal the
       passes let through) with `err` set. Nothing is printed. */
    int cv_convert(cf_arena* ar, const cf_blob* in, const cf_codec* from,
                   const cf_codec* to, cv_result* res, cf_error* err);
    ```

    `convert.c` keeps the command line, `codec_read`, the notes, the
    printing of a finding (a table from `cv_kind` to the `<what>` above)
    and the exit codes; what it prints does not change, and step 1 proves
    it with W5-V1's own `codec.convert`, unchanged. `CV_OTHER` is a
    finding W5-V1's pass 3 made with `a value of this kind`; the test
    treats it as a failure (the table did not predict it).
  * **The test runs through the core, not the program.** The program
    would be one process per document and pair (some 560 documents, 36
    pairs, three hops: 60,000 starts, an hour on Windows); the core is
    the same code, which `codec.convert` already runs through the
    program; and a harness that holds the contents can compare them,
    which text on a pipe cannot. `tests/codec/pairs.c` (new), target
    `codec_pairs`, sources `pairs.c` and
    `${CMAKE_SOURCE_DIR}/tools/codec/convert_core.c`, linked to
    `configforge_serialize`, C 90, the warnings and analysis helpers;
    registered in `tests/codec/CMakeLists.txt` under
    `if(TARGET configforge_codec)`, and run only when the build has
    `CF_WITH_EMIT`, `CF_WITH_REAL` and ini, json, toml, yaml and cbor
    (else the test is not registered, as `codec.formats` is guarded by
    `EMIT`). It includes `tests/codec/chain.h` (new, below) and sees
    the public headers only, though `tests/CMakeLists.txt` defines
    `CF_INTERNAL_API=1` for everything under it.
  * **The contents, and what is compared: `tests/codec/chain.h`**, C89,
    static functions, shared by `pairs.c` and `tests/fuzz/chain.c`:
    * `ch_tree* ch_read(cf_arena*, const cf_blob*, const cf_codec*,
      cf_error*)`: the codec's events into a tree of nodes (table, array,
      scalar with `cf_value` and its text copied, the member's name or
      none), the sink lending `reserve_text` and `reserve_work` from
      the arena as `check` does (T-3).
    * `const char* const* ch_lines(cf_arena*, const ch_tree*, size_t*)`:
      one line for each scalar, `<path>\t<type>\t<text>`, and one
      `<path>\ttable` or `<path>\tarray` for each container with nothing
      in it (`<path>` empty for the document), sorted with `strcmp`, so
      the order of keys does not matter and the order of an array's
      members does (its index is in the path). A path is `.` and the
      name for a table member, `[` and the index and `]` for an array
      member; in a name, `\`, `.`, `[`, `]`, and every byte below 0x20
      or equal to 0x7F, are written `\xNN`, so no two trees have one
      list. Types are `cf_type_name`'s; a text is the bytes with `\n`,
      `\r`, `\t`, `\\` and the other bytes below 0x20 and 0x7F written
      `\n`, `\r`, `\t`, `\\` and `\xNN`; a real is `cf_value_to_text`'s,
      so `-0.0` and `0.0` are one, which is how the library names them.
    * `ch_tree* ch_lower(cf_arena*, const ch_tree*, unsigned holds)`:
      what the table says a format with `holds` writes of a tree, as a
      new tree, written from the bits and from nothing else, bottom up:
      1. a table member whose name is empty and `holds` lacks
         EMPTY_NAME, or is not empty and not `[A-Za-z0-9_-]+` and
         `holds` lacks ANY_NAME, is dropped with everything under it;
      2. a table member for which `plen + n` is more than `CF_KEY_MAX - 2`
         and `holds` lacks LONG_PATHS is dropped with everything under
         it, where `n` is its name's length and `plen` the length of its
         parent's joined path of table names, dots included, 0 for a
         member of the document (a table inside an array is written
         inline, with no path, and is not subject to this, nor is
         anything below it);
      3. a scalar: an int, real or bool whose bit is missing becomes the
         text `cf_value_to_text` gives; a null whose bit is missing is
         dropped when `CF_REFUSES_NULL` is set, else becomes the text
         `null`; a text with `\n` is dropped when LINE_BREAK is missing,
         one with a NUL when NUL is missing;
      4. an array: dropped, with everything under it, when ARRAYS is
         missing; or when its children as read (before 1 to 3) hold a
         table and TABLE_ARRAYS is missing, hold an array and
         NESTED_ARRAYS is missing, or are none and EMPTY_ARRAY is
         missing; its members that 1 to 3 dropped leave it, and the
         indexes close up;
      5. a table member that is empty - as read, or with all its members
         dropped by 1 to 4 - and `holds` lacks EMPTY_TABLE is dropped.
         The document itself is never dropped.
    * `cf_bool ch_same(const ch_tree*, const ch_tree*)`: equal `ch_lines`.
  * **What is checked**, in `pairs.c`, for each document `D` read through
    its own format `F` (a document the reader refuses is counted and
    skipped), for each ordered pair (A, B) of the writers `ini json json5
    toml yaml cbor` - all 36, A = B included:
    1. **hop 0**, `D` into A: `x1 = cv_convert(D, F -> A)`.
    2. **hop 1**, `x1` into B: `x2`; **hop 2**, `x2` into A: `x3`.
    3. For each hop, with `m` the tree read from the hop's input bytes by
       the hop's source codec, `r` the tree read from its output bytes by
       the target codec: `ch_same(r, ch_lower(m, holds(target)))`, else
       `FAIL <doc> <A> -> <B> hop <n>: <first differing line>`. And if
       `ch_same(m, r)` is false, the hop reported a finding or counted a
       value written as text (`nfindings + as_text > 0`): a loss is never
       quiet. A finding of kind `CV_OTHER`, and a hop for which the
       target's reader refuses what its writer wrote, are failures
       (a writer must be read by its own reader).
    4. For the pair: if `holds(B)`, without `CF_REFUSES_NULL`, has every
       bit `holds(A)` has, `ch_same(read(x3, A), read(x1, A))` - a pair
       the table says loses nothing loses nothing - and, when A = B,
       `x3` and `x1` are the same bytes (writing is stable).
    A run prints, last, `pairs: <n> documents (<m> not read), 36 pairs,
    <k> checks, <f> failures` and exits 1 when `f` is not 0, printing
    the first 20 failures only.
  * **The documents.** Three sources, one list, `--docs <manifest>`:
    a manifest line is `<format>\t<path>\t<mode>`, mode `-` (the file as
    it is), `wrap` (the suites' own wrapping, below) or `split` (the file
    holds documents separated by a line of five dashes, as
    `tests/support/data/edges.*` do).
    1. **Edge documents**, built in `pairs.c` from JSON text (JSON holds
       every type) and read through `json`, so the default run needs no
       file and no network:
       * `types`: `{"i": 1, "f": 0.5, "b": true, "n": null, "s": "x"}`;
       * `ints`: `-9223372036854775808`, `9223372036854775807`, `0`, `-1`;
       * `reals`: `5e-324`, `1.7976931348623157e308`,
         `0.30000000000000004`, `0.1`, `1e22`, `1e-7`, `-0.0`, `1e16`,
         `123456789012345.6`;
       * `non_finite`: the texts `nan`, `inf`, `-inf`, `Infinity`;
       * `texts`: the empty text; one with leading and trailing spaces;
         `"`, `\`, `'q'`, `#`, `;`, `=`, `[x]`; `${x}` and `$${y}`
         (nothing here expands them); a tab; U+00E9, U+20AC, U+1F600,
         U+10FFFF, U+FFFE, U+007F, U+0085, U+2028;
       * `breaks`: `"a\nb"`, `"a\r\nb"`, `"a\rb"`, and a plain `"k": "v"`;
       * `nul`: `"x\u0000y"` and `"\u0000"`;
       * `names`, at the root and again inside a table `"t"`:
         `Mixed_Case`, `with space`, `dot.ted`, `quote"d`, a tab,
         `é`, the empty name, `a-b_c9`;
       * `arrays`: `[1,2,3]`, `[1,"a",true,null,0.5]`, `["a","b"]`, `[7]`;
       * `arrays_hard`: `[]`, `[{"a":1},{"b":2}]`, `[[1,2],[3]]`,
         `[[[1]]]`, `[{"a":[1,2]}]`;
       * `empty_tables`: `{"e": {}, "n": {"e": {}}, "k": 1,
         "t": {"only": {"x": 1}}, "o": {"p": {}}}`;
       * `order`: `{"z": 1, "a": 2, "m": {"b": 1, "a": 2}}`;
       * `deep31`: the document and 31 nested tables named `a`, a leaf in
         the last; and `ok_path`: four nested tables with names of 60
         bytes and a leaf; `long_path`: six with names of 50 bytes and a
         leaf (a path of 305 bytes); and the boundary of the rule above,
         at the default `CF_KEY_MAX` of 256 (the document names are
         built from `CF_KEY_MAX` in `pairs.c`): `path_1_ok`, one table
         with a name of 254 bytes and a leaf; `path_1_long`, a name of
         255; `path_2_ok`, a table `a` holding a table with a name of 253
         bytes (a path of 255) and a leaf; `path_2_long`, the inner name
         254;
       * `big`: an array of 2,000 integers and a text of 70,000 bytes;
       * `looks`: the texts `true`, `123`, `null`, `0x1F`, `1e3`, `~`,
         `yes`, beside the integer `5`.
       * and the check `limits`, which is not a pair: a document of 32
         nested tables is refused by every one of the eight readers the
         build has, each by its own message.
    2. **The project's own fixtures**, in the default run: the manifest
       `tests/codec/pairs.manifest` (new) lists, with paths from the
       source root that `pairs.cmake` expands: `tests/support/data/edges.json`,
       `edges.toml` and `edges.ini` (mode `split`: the readers refuse
       most of them, which is what they are for; those that read are
       used), `examples/c/layers/app.toml`,
       `examples/c/layers/conf.d/10-site.ini`, `90-local.ini`,
       `examples/c/flags/features.ini`,
       `examples/c/flags/conf.d/50-canary.ini`,
       `tests/conformance/data/module.json`.
    3. **The conformance suites' valid documents**, only under
       `EXTRA=conformance` (they are fetched with git, which needs the
       network the first time; the default run never touches them):
       `scripts/check/pairs.py <codec_pairs> <suites-dir>` imports
       `scripts/check/conformance.py` (its `json_suite`, `json5_suite`,
       `toml_suite`, `yaml_suite`, `cbor_suite`, which list the valid
       documents; the `y_` cases of JSONTestSuite are `json_suite`'s),
       writes the manifest to a temporary directory and runs the
       program, in the conformance build with the built-in YAML parser
       only (`build-conformance`; the libyaml and libfyaml builds run
       `conform` and its floors as before, not this); the CBOR vectors are written as files there, as
       `cbor_suite` does. The suites' lone values and lists are
       configurations only as the member `v` of a table: conform.c's
       wrapping (`json` when the root is not an object, `json5` and
       `cbor` always, today `tests/conformance/conform.c`, the block
       that begins "JSON's grammar allows any value at the root") moves
       to `tests/conformance/wrap.h` as one static function
       `cf_bool conform_wrap(const char* format, const char* name,
       cf_blob* blob)`, `name` the source name the wrapped blob is given
       (`conform.c` passes `argv[2]`; the function keeps the original's
       static 1 MiB buffers and its refusal of a blob that does not
       fit, which is `false`), included by `conform.c` and by `pairs.c`
       for mode `wrap`;
       `conform.c`'s behaviour is unchanged and the floors in
       `tests/conformance/floor.txt` prove it. YAML documents the configuration
       refuses by name (docs/Conformance.md) are the "not read" count.
  * **Cells the test contradicts.** The first run of `codec_pairs` with
    the table above prints every difference between the table and the
    codecs. For a cell where the codec holds less than the table says and
    its writer or the converter gave a finding or refused (a visible
    loss), the table's bit is cleared and the cell in the table above is
    edited in the same commit. For a cell where a document went through
    a writer and its reader gave something else with no refusal and no
    finding (a silent loss, `m` and `r` differ and `nfindings + as_text`
    is 0), this card does not stop: the writer of that format is made to
    refuse the value (its encoder returns false for it, as INI's does
    for a line feed), which the converter then reports as a finding of
    that kind; the table's bit is cleared (and `CF_REFUSES_NULL` is not
    involved unless it is a null) and the cell edited; a test of that
    refusal goes in the format's own unit test, written first and seen
    to fail; and the notes list the document, the pair, the hop and the
    writer changed. A cell where the codec holds
    more than the table says is made true by setting the bit. Every
    cell changed is listed in the commit message with the document that
    showed it.
  * **The fuzz target, `tests/fuzz/chain.c`** (new): a libFuzzer entry
    point, `LLVMFuzzerTestOneInput`, built as `fuzz_chain` beside the
    readers' (`tests/fuzz/CMakeLists.txt`, after the `foreach`, with
    `convert_core.c` among its sources and `configforge_serialize` for
    link; not in `all_readers`: it has no reader number). The input: byte
    0 picks the source format, `ini json json5 toml yaml cbor xml
    properties` by its value modulo 8 (a format the build lacks ends the
    run); byte 1 the number of hops, 1 to 4 (modulo 4, plus 1); then one
    byte a hop picking the writer from `ini json json5 toml yaml cbor`
    (modulo 6); the rest is the document. The source's reader must accept
    it, else the run ends. Each hop is checked as step 3 above does
    (`ch_lower` and the quiet-loss rule) and a failure `abort()`s, so
    libFuzzer keeps the input; a hop whose conversion is refused by the
    core (the bound, D-82's) ends the run. Registered as `fuzz.chain`
    with the readers' `-max_total_time=${CF_FUZZ_SECONDS}`, `-max_len=8192`,
    `LABELS fuzz` (the `fuzz` test preset runs only that label) and
    `TIMEOUT` of `CF_FUZZ_SECONDS * 3 + 60`, as theirs, and
    `-seed_inputs=` three files written by CMake at configure time
    with `file(WRITE)`. The bytes are the ASCII digits `0` to `5`
    (0x30 to 0x35), which modulo 8, 4 and 6 are 0 to 5, so no NUL is
    needed and a digit is its own number: the source `ini` is `0`,
    `json` `1`, `toml` `3`; the writers `ini` `0`, `json` `1`, `toml`
    `3`, `yaml` `4`, `cbor` `5`; the hop-count byte is the count less
    one (`0` is one hop, `1` two). Each file is `<source byte><hop count
    byte><one writer byte a hop><document>`, so, exactly:
    * `fuzz_chain_seed1`, JSON through toml and back (two hops, writers
      `toml` then `json`): `1` `1` `3` `1` followed by
      `{"a": 1, "b": [true, null]}` (the bytes `1131{"a": 1, ...`);
    * `fuzz_chain_seed2`, INI through json (one hop, writer `json`):
      `0` `0` `1` followed by `[s]`, a line feed, `k = v`, a line feed;
    * `fuzz_chain_seed3`, TOML through cbor through yaml (two hops,
      writers `cbor` then `yaml`): `3` `1` `5` `4` followed by `[s]`, a
      line feed, `k = 1`, a line feed.
    Where `CF_FUZZ` is off nothing is built; `-DEXTRA=fuzz` runs it.
  * **The generated table.** `tools/codec/pairs.c` (new, statics `pr_`),
    `int codec_pairs(int argc, char** argv);` in `codec.h`, dispatched
    by `codec.c` as `pairs`; the usage gains
    `"  configforge codec pairs [--format text|markdown]\n"`
    `"                                 what each format can hold, and what each\n"`
    `"                                 pair of formats loses\n"`.
    It reads `cf_format_holds` only, for the names `ini json json5 toml
    yaml cbor`, so its output is the same in every build; a name
    `cf_format_holds` does not know ends the run, exit 2, `codec: no
    table for '<name>'` (that is how a writer added later is found: see
    `test_holds`). Two tables; the first has a row for each bit and a
    column for each format, a cell `yes`, `as text` (a type bit that is
    missing, and NULL when CF_REFUSES_NULL is not set) or `left out`
    (any other missing bit, and NULL with CF_REFUSES_NULL); the second a
    row for each ordered pair of different formats, in the column order,
    listing the bits A has and B lacks, each as `<row label> (<as text or
    left out, B's cell>)`, joined by `, `, or `nothing`. The row labels,
    in bit order: `integers`, `reals`, `booleans`, `null`, `arrays of
    values`, `empty arrays`, `arrays of tables`, `arrays of arrays`,
    `empty tables`, `names beyond letters, digits, _ and -`, `an empty
    name`, `a long table path`, `line breaks in text`, `NUL bytes in
    text`. The markdown form starts
    `| What | ini | json | json5 | toml | yaml | cbor |` and
    `| ---- | --- | ---- | ----- | ---- | ---- | ---- |` and, after a
    blank line, `| From | To | Lost |` and `| ---- | -- | ---- |`; the
    text form is the same rows with tabs. `docs/Formats.md` gains a
    section "What each format holds, and what a conversion loses":
    a paragraph (what the bits are, that a type a format lacks is
    written as text and anything else is left out and named, that no
    pair keeps the order of keys, that NaN and infinity are text, and
    that a table is left out of a TOML or INI file when its parent's
    path and its name together pass `CF_KEY_MAX` less two bytes, 254 by
    default), then `<!-- pairs:begin -->`, the markdown, and
    `<!-- pairs:end -->`. The page is held by `tests/codec/pairs_page.cmake`,
    `codec.pairs_page`, a copy of `formats.cmake`'s comparison
    (without its `EMIT` guard: the table is the same in every build)
    with the regeneration message
    `regenerate it with configforge codec pairs --format markdown, and put the output between the markers`;
    it also asserts `pairs --format sarif` exits 2 and `pairs --format
    text` has 30 lines in its second table. It is not a `gate.*` script
    because it needs the built program, as `codec.formats` does.
  * **The one table in the unit tests.** `lib/serialize/tests/test_holds.c`
    (new, A-1b's place for a test that includes only `cf_serialize.h`;
    its name goes in that component's test list, as A-1b made it)
    holds the table to the codecs: for each of the six, if
    `cf_codec_by_name` finds it, `cf_format_holds` is CF_TRUE; for `xml`,
    `properties` and `nosuch` CF_FALSE with `*holds` untouched;
    `(caps & (CF_CODEC_ARRAYS | CF_CODEC_SCALAR_ARRAYS)) != 0` is
    `holds & CF_HOLDS_ARRAYS`; `caps & CF_CODEC_ARRAYS` is all of
    EMPTY_ARRAY, TABLE_ARRAYS and NESTED_ARRAYS and its absence none;
    `caps & CF_CODEC_TYPED` is all of INT, REAL and BOOL and its absence
    none; `CF_REFUSES_NULL` only without `CF_HOLDS_NULL`; and every codec
    in `cf_codec_at(i)` with an encoder is one of the six (a writer
    added without a table row fails here). `lib/config/tests/test_roundtrip.c`
    (the `format` struct and
    `g_formats`) loses `arrays`, `nulls` and `lines` from them, keeps
    `compare`, and reads the rest from `cf_format_holds` (`arrays` is 1
    with `CF_CODEC_ARRAYS`, 2 with `CF_CODEC_SCALAR_ARRAYS` only; `lines`
    is `CF_HOLDS_LINE_BREAK`; **`nulls` is true when `CF_HOLDS_NULL` is
    set or `CF_REFUSES_NULL` is not**: today INI has `nulls` 1, since it
    writes a null as the text `null` and `test_nulls` expects
    `TRIP_SAME` for it, while only TOML expects `TRIP_REFUSED`); the
    self-check in `main` ("the table says what the codec says about
    arrays") is test_holds'.
  * **The edits to W5-V1's cases** (`tests/codec/convert.cmake`), added:
    `empty_toml`: `{"e": {}, "k": 1}` `--to toml`: 0, stderr `e: an empty
    table, which toml cannot hold; left out`; `--strict`: 1.
    `yaml_null`: `{"a": null, "b": 1}` `--to yaml`: 0, stderr has no
    `carries no types` line about the null (`b` is counted: `1 value(s)`).
    `long_path`: a table nested six deep with names of 50 bytes, `--to ini`:
    stderr has `a table path too long` (and the case `path_2_long`'s
    document, the inner name of 254 under `a`, gives the same, while
    `path_2_ok`'s, 253, gives none). `empty_name`: `{"": 1, "k": 2}`
    `--to toml`: stderr `an empty name, which toml cannot hold` and not
    `a name it would write unquoted`.
* **files**: `lib/serialize/include/configforge/cf_serialize.h`,
  `lib/serialize/src/codec.c`, `cmake/configforge.experimental.symbols`,
  `tools/codec/convert_core.c` (new), `tools/codec/convert_core.h` (new),
  `tools/codec/convert.c`, `tools/codec/pairs.c` (new),
  `tools/codec/codec.h`, `tools/codec/codec.c`,
  `tools/codec/CMakeLists.txt`, `tests/codec/chain.h` (new),
  `tests/codec/pairs.c` (new), `tests/codec/pairs.manifest` (new),
  `tests/codec/pairs_page.cmake` (new), `tests/codec/convert.cmake`,
  `tests/codec/CMakeLists.txt`, `tests/conformance/wrap.h` (new),
  `tests/conformance/conform.c`, `tests/fuzz/chain.c` (new),
  `tests/fuzz/CMakeLists.txt`, `lib/serialize/tests/test_holds.c` (new),
  `lib/serialize/tests/CMakeLists.txt` (its test list),
  `lib/config/tests/test_roundtrip.c`, `scripts/check/pairs.py` (new),
  `scripts/verify.cmake` (the conformance step: build target
  `codec_pairs`, run `pairs.py`); last: `docs/Formats.md`,
  `docs/Tools.md`, `docs/API.md` (regenerated), `docs/Decisions.md`,
  `CHANGELOG.md`, `docs/Roadmap.md`.
* **steps**:
  0. If `## 1.8.0 — unreleased` is not open in `CHANGELOG.md`, A-1 opened
     it; check, and if it is not there open it:
     `cmake -DTO=1.8.0 -P scripts/release/version.cmake`, committed
     alone. Everything this card adds to the CHANGELOG goes under it.
  1. A change that moves code and nothing else: the tree, passes, replay
     and writing of `convert.c` into `convert_core.c` behind
     `cv_convert`, `convert.c` printing from `cv_result`.
     `cmake --build --preset default && ctest --preset default -R codec.convert`
     passes as it did before the move (the same cases, unchanged).
  2. `lib/serialize/tests/test_holds.c` and its line in that test list. See it
     fail: the build stops (`cf_format_holds`, `CF_HOLDS_INT` undeclared).
  3. The enum, `cf_format_info.holds`, `cf_format_holds`, the table of
     values above. `ctest --preset default -R test_holds` passes.
     `cmake -DSRC=. -DWRITE=ON -P cmake/gates/GateAbi.cmake` (POSIX
     shell), `cmake -P scripts/generate/apiref.cmake`;
     `ctest --preset default -R "gate\\.(abi|apiref)"` passes.
  4. `chain.h`, `pairs.c`, `pairs.manifest`, their registration as
     `codec.pairs`, the converter still on W5-V1's own passes. See it
     fail: `ctest --preset default -R codec.pairs` prints `FAIL
     empty_tables json -> toml hop 1` (the table says TOML writes no
     empty table, and no finding was given: a quiet loss) and `FAIL
     types json -> yaml hop 1 ... as_text` (the null counted as text),
     and the run exits 1. The first run's other differences are settled by
     "Cells the test contradicts" before step 5.
  5. The passes of `convert_core.c` from the bits, the three new
     `convert.cmake` cases. `ctest --preset default -R "codec\\.(convert|pairs)"`
     passes.
  6. `tools/codec/pairs.c`, `pairs_page.cmake`, `docs/Formats.md`'s
     section. See it fail first: `ctest -R codec.pairs_page` fails with
     `has no <!-- pairs:begin --> and <!-- pairs:end --> markers`; put
     the output of `configforge codec pairs --format markdown` between
     them; it passes.
  7. `test_roundtrip.c` reads the table; `ctest -R "test_(roundtrip|holds)"`.
  8. `wrap.h` and `conform.c` using it, `pairs.py`, the verify step.
     `cmake -DSOURCE=index -DJOBS=3 -DWORK=$HOME/cfv -DEXTRA=conformance -DMODE=quick -P scripts/verify.cmake`
     prints the floors unchanged and `pairs: ... 0 failures`; read the
     "not read" count and write it in the commit message.
  9. `tests/fuzz/chain.c` and its registration; `-DEXTRA=fuzz` where
     clang has libFuzzer: `fuzz.chain` runs its seconds clean.
  10. The documents. ACCEPT-LINUX, ACCEPT-WINDOWS.
* **accept**: ACCEPT-LINUX with `-DEXTRA="abi;conformance;fuzz;components"` (the new
  experimental name, the suites through every pair, the new target, and
  A-1b's build of `lib/serialize` and `lib/config` alone, which now
  hold `test_holds` and the changed `test_roundtrip`),
  ACCEPT-WINDOWS (`codec.pairs`, `codec.pairs_page`, `test_holds`
  on MSVC; conformance and fuzz are not run there), ACCEPT-CI. Also
  `gate.apiref` and `gate.abi` pass.
* **stop if**: W5-V1's converter is not built as its
  card says (no tree, or its passes mark nodes in a way that cannot
  read the original children): follow what is there, make
  `cv_convert` out of it, say so in the notes, and stop only if the
  findings cannot be made independent of the order of the passes. Or: a
  writer's output is refused by its own reader for depth, size or a
  limit that `limits` and the edge documents do not already state (a
  new limit: name it). Or: `CF_EXPERIMENTAL(1, 8) cf_bool cf_format_holds(...)`
  is moved by clang-format 22 so that gate.abi's regex does not find it
  (S-2's own stop-if). A document that comes back different from a
  writer and its own reader with no refusal and no finding is not a stop:
  "Cells the test contradicts" says what to do, and the notes list it.
* **docs**: add **D-109**, "What a format can hold, and every pair
  tested", in D-80's shape: the question (what a conversion loses, said
  once and tested); the options (more caps bits; a member of
  `cf_codec`; a table inside the tools; a second word of bits kept
  with the formats); the decision (the fourth; the fourteen bits and
  `CF_REFUSES_NULL`, with what was considered and is not a bit; the
  converter and the test read the one table; the test runs through the
  core and why; the oracle written from the bits alone; the three kinds
  of document and why the suites are an `EXTRA`; the table generated and
  held; experimental in 1.8); compatibility (a new experimental
  function and enum values; convert names more findings than W5-V1
  said and counts a YAML null no longer as text). `docs/Tools.md`, the
  codec section: `pairs`, and in "Converting between formats" the list
  of what is named, now from the table, with `empty_toml`'s line as the
  example. CHANGELOG, under `## 1.8.0 — unreleased` (not 1.7.0's, which
  the header of this file names for the cards before it), `### Added`: "`cf_format_holds` (experimental):
  what each format that has a writer can hold. `configforge codec
  pairs`: what each format holds and what each pair of formats loses,
  generated into docs/Formats.md. Every document of the conformance
  suites and the project's fixtures is tested through every ordered pair
  of the six writing formats, and a fuzz target takes a random document
  through a random chain (D-109)." `### Changed`: "`configforge codec
  convert` names an empty table, a table path too long for TOML and INI
  (the path above it and its name pass `CF_KEY_MAX` less two bytes), a name that is empty and a text with a NUL byte where the
  target cannot hold them, from one table, and no longer counts a YAML
  null as written as text (D-109)." Roadmap: the row "Tests that
  convert between every pair of formats" goes 🟩, `D-109`; the row
  "Formats registered, with what each can do" goes 🟨, "what each
  writer can hold, as one table the converter, the tests and the page
  read, done (D-109); the ways to read it, and `docs/Limits.md`
  generated from it, left (A-13)".
* **notes**:

---

## W5-G4 - Generators outside the tree

* **id**: W5-G4
* **goal**: `configforge gen --schema S --lang X`, when X is no entry of
  `gen_langs[]`, runs the program `configforge-gen-X` found beside
  `configforge-gen` or on the PATH, hands it the contract as JSON on its
  standard input and the command line's other options on its own, and
  returns its exit code; `--lang contract-json` writes that JSON itself,
  for whoever writes a generator.
* **why**: a team that wants the contract in Go, Zig or a schema of its
  own should not have to change this project: the contract is written
  once, and each language is a program that reads it, as git finds its
  subcommands. The JSON is also the contract in a form any language
  reads without the library.
* **needs**: A-1b, W5-G3 (the paths below are the ones after A-1's map)
* **size**: large
* **decided**:
  * **The order of a `--lang`** (in `tools/gen/gen.c`, W5-G1's table
    lookup, which this card extends and does not move):
    1. a name that is an entry of `gen_langs[]`: the table's writer, and
       nothing outside the tree is looked at - a program called
       `configforge-gen-c` on the PATH never replaces C;
    2. else, a name that is not 1 to 64 bytes of `a-z`, `0-9` and `-`,
       not starting with `-` (T-1's rule for a command name, so `../x`
       and `Fake` are never run): `gen: '<name>' is not a language name`,
       exit 2;
    3. else the program `configforge-gen-<name>` (on Windows
       `configforge-gen-<name>.exe`) is looked for: in the directory of
       this program first, then in each directory of the PATH, never the
       current directory and never an empty PATH entry; the first that
       is a regular file that may be executed (Windows: that exists and
       is not a directory) is the one. Not found, exit 2, W5-G1's text
       extended to say where it looked:
       `gen: no language '<name>'; there are: <the table's names, in its order>; no configforge-gen-<name> in <dir> or on the PATH`
       (`<dir>` is this program's directory, and when it is not known -
       POSIX, `argv[0]` without a `/`, found through the PATH itself - the
       text is `...; no configforge-gen-<name> on the PATH`);
    4. else the schema is read as for any language (a schema that cannot
       be read is `gen: <message>` and exit 2, and **nothing is
       started**); then the program is run.
    The table's names are the ones W5-G1 to W5-G3 and this card's one
    entry gave it, in its order: `c, cpp, rust, python, contract-json`.
  * **Built in: `contract-json`.** A last entry of `gen_langs[]`, name
    `contract-json`, extension `.json`, no identifier check, no reserved
    words, whose writer writes the JSON below. It is how an author sees
    what a generator is given (`configforge gen --schema S --lang contract-json`),
    it is what is piped to a generator, and it is the golden file's
    source, so there is one writer. Adapt the writer to the table's
    entry as W5-G1 left it (read `gen_lang` in `tools/gen/gen.c`): the
    function below is the work, the entry's writer calls it and writes the
    bytes to the entry's output.
  * **The JSON**, in `tools/gen/contract.c`, declared in `gen.h`:

    ```c
    /* The contract as the JSON every generator outside the tree is
       given: version 1 of "configforge-contract". `schema` is the schema
       file's name without its directories, `name` the --name given or
       NULL, `text` the schema file's bytes. Written into `out`, measured
       or written as every sink is. 0, or 2 with `gen: '%s' has a value
       the contract JSON cannot hold` on stderr for a definition with a
       flag no name is known for (a bit above CF_RESTART), or a value of
       a type a definition cannot have. */
    int gen_contract_json(const cf_defset* ds, const char* schema,
                          const char* name, const char* text, size_t n,
                          cf_sink* out);
    ```

    Layout: two spaces of indent, one member or array element a line,
    except an array of scalars, which is on one line (`["a", "b"]`, `[]`
    when empty); a newline at the end; strings through
    `cf_sink_json_string` (UTF-8 as it is, control bytes `\u00xx`),
    integers and reals through `cf_value_to_text` (a real always has a
    `.` or an exponent, so it is never taken for an integer; an integer is
    its exact decimal digits, 64 bits, which a reader in a language of
    doubles must read as it can - `tests/doctor/contract.schema.json`'s
    `-9007199254740993` is in the golden file). Members, in this order,
    all always present - an absent value is `null`, so a generator never
    tests for a missing key:

    ```json
    {
      "format": "configforge-contract",
      "version": 1,
      "schema": "contract.schema.json",
      "name": null,
      "schema_text": "<the schema file, escaped>",
      "open": ["plugins."],
      "definitions": [
        {
          "key": "server.port",
          "type": "int",
          "description": "Where the server listens: ...",
          "flags": [],
          "on_invalid": "strict",
          "replaced_by": null,
          "default": 8080,
          "min": 1,
          "max": 65535,
          "min_length": null,
          "max_length": null,
          "allowed": null,
          "aliases": null,
          "policy": null,
          "list": null
        }
      ]
    }
    ```

    The members of a definition, each from `cf_def`
    (`lib/config/include/configforge/cf_define.h:68`): `key`; `type`, one of `"bool"`, `"int"`, `"real"`,
    `"text"`; `description`, a string or `null` (as `d->description`,
    where the schema gives none); `flags`, an array of names in this
    order of those set, `"required"`, `"sensitive"`, `"deprecated"`,
    `"transient"`, `"restart"` (`CF_REQUIRED` to `CF_RESTART`, as
    `c_flags` names them); `on_invalid`, `"strict"` or `"fallback"`;
    `replaced_by`; `default`, `null` without `has_default`, else the
    value (`true`, a number, a string) and, for a list, the array of its
    default values (`[]` for a default that is empty); `min` and `max`,
    the range of an int or a real when `has_range`, else `null`;
    `min_length` and `max_length`, the text bounds, `null` for 0 (0 is
    "no bound" for both); `allowed`, the permitted spellings or `null`;
    `aliases`; `policy`; `list`, `null` for a scalar, else
    `{"mode": "replace"|"append", "separator": null|","}` (the separator as
    a one-byte string, `null` for 0). The definitions are in the order
    `cf_defset_at` gives them. `open` is `ds->open_prefixes`, in order.
    `schema_text` is the file as read (a function that reads the schema
    whole - W5-G2 added one for the Rust file; use it if `contract.c`
    has it, else `cf_blob_file`). Not in it: the schema's closedness
    (the C writer does not write it either), the compile-time checks
    `gen_checks` makes (a language's own), and the version of the
    library (a golden file would change with every release).
    Rejected: the library's own `cf_schema_write` as the hand-over (a
    generator would have to walk JSON Schema's nesting, `type` unions
    and `$defs` to find a key, which is the work this JSON ends); the
    definitions as C structs in a binary form (no language but C reads
    it); omitting a member that is absent (every generator then tests
    for it).
  * **What the program is given.** On its standard input, the JSON,
    whole, written by `gen_contract_json` into a buffer first (the
    schema's size is a few kilobytes, a contract of 3,000 keys 600
    KiB), then to the pipe; the pipe is closed when it is written. On its
    command line: every argument `configforge gen` was given but `--lang
    <lang>` and `--schema <schema>` and their values, in order and
    unchanged - that is, `--name <name>` and `--out <file>` when they
    were given, and nothing when not. Their meaning, which the docs state
    for authors: `--name N` is what the generated thing is called in the
    generator's own language (it is for the generator to check it and to
    have a default); `--out F` is where the generated file goes, without
    it standard output. `gen` checks neither (W5-G1's identifier checks
    are the built-in languages'). The program writes its own output: its
    standard output and standard error are this program's (inherited),
    and `gen` writes nothing to either, so a generator's messages reach
    the user as they were written. A file `--out` names is the
    generator's; `gen` does not create, truncate or remove it (a built-in
    language removes its file when it fails; a program outside the tree
    owns its own).
  * **The exit code.** The generator's own code passes through unchanged
    (0 done; `gen` itself uses 0, 1 for a `check` failure that no
    generator can have, and 2). If the program cannot be started:
    `gen: cannot run <path>`, exit 2. If it is stopped by a signal
    (POSIX) or ends with a code above 255 (Windows, a crash):
    `gen: <path> was stopped (signal <n>)` or `gen: <path> stopped with
    code 0x<8 hex digits>`, exit 2. A generator that does not read its
    input and exits is not a failure of `gen`: a write that finds the pipe
    closed is dropped, and the generator's exit code is returned.
  * **Starting it, in `tools/gen/spawn.h`**, with `spawn_posix.c`,
    `spawn_win32.c` and `spawn_none.c`, each guarded whole by
    `CF_PLATFORM_POSIX`, `CF_PLATFORM_WIN32` and neither (the platform
    decided where the library decides it, `configforge/cf_platform.h`,
    as T-1's run files do); CMake builds the one the platform has, by
    the test `tools/configforge/CMakeLists.txt` uses. C89, no internal
    tier needed:

    ```c
    /* Finds configforge-gen-<lang>: beside this program (the directory
       of argv[0] when it has a / or \, on Windows always the module's),
       then on the PATH. A malloc'd path, or NULL with *dir set to this
       program's directory (malloc'd; NULL when not known). */
    char* spawn_find(const char* argv0, const char* lang, char** dir);

    /* Runs `path` with the arguments argv[1..argc-1] but each name in
       `drop` (NULL-terminated; "--lang", "--schema") and the argument after
       it, `input` (n bytes) on its standard input, its other two handles
       this program's; waits for it. 0 when it ran to its end, *code its
       exit code (0 to 255); 1 when it could not be started; 2 when it was
       stopped, the signal or the code in *code. */
    int spawn_run(const char* path, int argc, char** argv, const char* const* drop,
                  const char* input, size_t n, int* code);
    ```

    POSIX (`#define _POSIX_C_SOURCE 200112L` first, before any header,
    as T-1's `run_posix.c`; `<spawn.h>`, `<signal.h>`, `<sys/stat.h>`,
    `<sys/wait.h>`, `<unistd.h>`, `<errno.h>`): `spawn_find` tries
    `<dir>/<program>` when `argv[0]` holds a `/`, then each `:`-separated
    PATH entry that is not empty, each with `stat` (regular file) and
    `access(path, X_OK)`. `spawn_run` builds the new `argv` with `malloc`
    (path, then the kept arguments), makes a `pipe`, and starts the
    program with `posix_spawn` and the file actions `adddup2(rd, 0)`,
    `addclose(rd)`, `addclose(wr)` - a failure of the `exec` is then
    the call's own error, so no exit code is mistaken for one, which a
    `fork` and `_exit(127)` would do. `extern char** environ` is declared
    in the file. `SIGPIPE` is set to ignored (`signal(SIGPIPE, SIG_IGN)`)
    before the write and restored after the wait; the write loops
    over `EINTR` and short writes and drops `EPIPE`; the write end is closed;
    `waitpid` loops over `EINTR`; `WIFEXITED` gives the code,
    otherwise `WTERMSIG`. Windows (`<windows.h>`, `<shellapi.h>`):
    `spawn_find` takes the module's directory from `GetModuleFileNameW`
    (doubled from 260 `wchar_t` to at most 32768) and tries
    `<dir>\configforge-gen-<lang>.exe`, then each entry of `PATH` split at
    `;`, an empty entry skipped, a pair of `"` removed, with
    `GetFileAttributesW` (exists, not a directory); only `.exe` - not
    `PATHEXT` and no `.cmd` or `.bat`, which `CreateProcessW` runs only
    through `cmd.exe` and its quoting (the 2024 batch-file argument
    injections), a generator written as a script needs an `.exe` to start
    it. `spawn_run` takes the arguments from
    `CommandLineToArgvW(GetCommandLineW())`, the same indexes as the
    narrow `argv` (T-1 relies on it and says to stop if the counts
    differ), drops the same indexes, and quotes each by T-1's rule;
    `CreatePipe` with an inheritable read end, `SetHandleInformation(wr,
    HANDLE_FLAG_INHERIT, 0)`, `STARTUPINFOW` with `STARTF_USESTDHANDLES`
    and this program's own output and error handles, `CreateProcessW`
    with `bInheritHandles` true and `SetConsoleCtrlHandler(NULL, TRUE)`
    before it (Ctrl+C is the child's to answer, as T-1); `WriteFile`
    in a loop, `ERROR_BROKEN_PIPE` and `ERROR_NO_DATA` dropped; the write
    handle closed; `WaitForSingleObject`, `GetExitCodeProcess`, every
    handle closed. Each failure returns early rather than testing inside
    an expression (MSVC's `/analyze`). `spawn_none.c`: `spawn_find`
    returns NULL; `gen.c` then writes `gen: no language '<name>'; there
    are: ...; this platform cannot run another program` (the table, and
    that sentence in place of "no configforge-gen-... in ..."), exit 2,
    and `spawn_run` is not reached.
  * **The test**, no Python and no shell script: `tests/gen/fake_gen.c`
    (new), C89, built as the target `gen_fake`, `OUTPUT_NAME
    configforge-gen-fake`, in `${CMAKE_CURRENT_BINARY_DIR}/ext`
    (`RUNTIME_OUTPUT_DIRECTORY`), linked to nothing; standard input and
    output in binary mode under `#ifdef _WIN32` (`_setmode` with
    `_O_BINARY`). It prints its arguments to standard error as
    `fake: args=<each argument after argv[0], joined by |>`, then acts on
    the value of `--name`: `exit7` - exits 7 after printing `fake: exit 7
    asked for` to standard error; `noread` - exits 3 without reading its
    input; `ran` - creates the file `ran.marker` beside its `--out`'s
    directory, reads nothing, exits 0; anything else, or no `--name` -
    reads its standard input whole and writes it to the `--out` file
    (`"wb"`) or to standard output, exit 0. Three more programs in
    `ext/`, copies made by `POST_BUILD` with `cmake -E copy`: a copy of
    `gen_fake` named `configforge-gen-c${CMAKE_EXECUTABLE_SUFFIX}` (a
    program that must never be run for `--lang c`) and a copy of
    `configforge-gen` itself named `configforge-gen` (with the library's
    DLL beside it on a Windows shared build), for the beside-itself case.
    `tests/gen/external.cmake`, run as
    `${CMAKE_COMMAND} -DGEN=$<TARGET_FILE:configforge_gen> -DEXT=<ext dir>
    -DEXE=${CMAKE_EXECUTABLE_SUFFIX} -DSCHEMA=<contract.schema.json> -DGOLDEN=<golden>
    -DWORK=<dir> -P external.cmake`, registered as `gen.external` (and
    its part `gen.contract_json` below). It sets `ENV{PATH}` to `EXT` and
    the old PATH with `;` on a Windows host and `:` elsewhere
    (`CMAKE_HOST_WIN32`), writes into `WORK`, and uses
    `execute_process` with `OUTPUT_FILE` for the bytes, so no
    byte is read back through a variable. Cases:
    1. `contract_json`: `--schema SCHEMA --lang contract-json --out m.json`
       exit 0; `m.json` and `tests/gen/golden_contract.json` (new, written
       by that command and read whole before it is staged) are the same
       bytes (`cmake -E compare_files`); on a difference the failure
       says `the contract JSON differs from tests/gen/golden_contract.json:
       regenerate it with configforge gen --schema tests/doctor/contract.schema.json --lang contract-json --out tests/gen/golden_contract.json, and read the diff`.
       The golden file has the schema's text, a `null` `name`, the
       `plugins.` prefix, the list default `["a", "bé"]`, the real range
       `0` to `1`, a `"sensitive"` and a `"required"` flag, the
       deprecated key and the integers `-9007199254740993` and
       `9223372036854775807`.
    2. `stdout`: `--schema SCHEMA --lang fake` with no `--out`: exit 0,
       standard output is the golden file's bytes (the generator was
       handed the same bytes the built-in writes), stderr is exactly
       `fake: args=` and a newline (no argument reached it).
    3. `args`: `--lang fake --name Thing --schema SCHEMA --out o.txt`: exit
       0, `o.txt` is the golden file but for the `name` line, which says
       `"name": "Thing",`; stderr `fake: args=--name|Thing|--out|o.txt`
       (`--lang`, `--schema` and their values did not reach it, the
       others in order, the path as written).
    4. `exit`: `--name exit7`: exit 7, stderr has `fake: exit 7 asked for`.
    5. `missing`: `--lang nosuch`: exit 2, stderr matches
       `gen: no language 'nosuch'; there are: c, cpp, rust, python, contract-json; no configforge-gen-nosuch in .+ or on the PATH`
       (the directory is `GEN`'s). With `GEN` run by a bare name and no
       directory it is `... on the PATH`; that second form is not
       tested (the test cannot make the shell find the program).
    6. `name_rule`: each of `../x`, `Fake`, `-x` and `a/b` as the
       language: exit 2, stderr `is not a language name`.
    7. `builtin_wins`: `--lang c --out w.c`: exit 0 and `w.c` holds
       `cf_builder_define` (the C writer ran, not the `configforge-gen-c`
       in `ext/`, whose marker file does not exist).
    8. `bad_schema`: `--schema missing.json --lang fake --name ran --out w/o.txt`:
       exit 2, stderr `gen: `, and `w/ran.marker` does not exist (nothing
       was started).
    9. `closed_input`: a schema of 3,000 definitions (`"k0"` to `"k2999"`,
       each `{"type": "integer"}`, written by the script with `file(WRITE)`
       in a loop) `--name noread`: exit 3, stderr has no crash and no
       `gen: ` line (the JSON is far past a pipe's buffer; the generator
       left; the code is the generator's).
    10. `beside`: the copy of `configforge-gen` in `ext/`, run by its full
        path, with `ENV{PATH}` set to the old PATH alone (no `ext/`): `--lang
        fake` finds `configforge-gen-fake` beside itself: exit 0.
    11. `not_a_directory` (POSIX hosts only): a directory named
        `configforge-gen-dirlang` in `ext/`, `--lang dirlang`: exit 2,
        the `no language` text (a directory is not a program).
    And `tests/gen/CMakeLists.txt` registers `gen.external` under
    `if(NOT CMAKE_CROSSCOMPILING AND CF_WITH_REAL AND TARGET gen_fake)`,
    after the schema fixtures (it needs none of Doctor's:
    `external.cmake` reads the schema from the source tree, so it has no
    `FIXTURES_REQUIRED`). `CF_WITH_REAL` guards it because the golden
    file has a real (`ratio`, and the real range), which a build
    without reals cannot read from the schema, as `test_gen.c`'s own
    `#if CF_WITH_REAL` is there for; a build with REPORT and without
    reals still builds `gen_fake` and the rest of the tools.
  * **The documents for authors**, `docs/Tools.md`, the gen section: a
    subsection "A generator of your own": the order of a `--lang` (the
    four steps), the name rule, where the program is looked for and that
    the current directory never is, Windows' `.exe`, what is on standard
    input with the JSON's layout as a table of its members, the command
    line, that `--name` and `--out` are the generator's to check, the exit
    codes, that the program's output is its own, and `--lang contract-json`
    to see an input; and that `tests/gen/fake_gen.c` is the tested reference
    for a program that reads the input. No example in another language is
    written in the docs (it would not be tested).
  * Rejected: handing over the schema file's path instead of its contents
    (the generator would read JSON Schema itself); a temporary file as the
    input (a name, a race and a file left behind); `fork` and `_exit(127)`
    (an exec that fails and a generator that returns 127 look alike);
    capturing the generator's output and writing `--out` here (a
    generator that writes more than one file, or a binary one, would be
    shut out, and a large output would sit in memory); `PATHEXT` on
    Windows (above); searching the current directory (a file somebody left
    there would run, as `configforge`'s front door never allows).
* **files**: `tools/gen/gen.c`, `tools/gen/gen.h`, `tools/gen/contract.c`,
  `tools/gen/spawn.h` (new), `tools/gen/spawn_posix.c` (new),
  `tools/gen/spawn_win32.c` (new), `tools/gen/spawn_none.c` (new),
  `tools/gen/CMakeLists.txt`, `tests/gen/fake_gen.c` (new),
  `tests/gen/external.cmake` (new), `tests/gen/golden_contract.json` (new,
  generated), `tests/gen/CMakeLists.txt`; last: `docs/Tools.md`,
  `docs/Decisions.md`, `CHANGELOG.md`, `docs/Roadmap.md`.
* **steps**:
  0. If `## 1.8.0 — unreleased` is not open in `CHANGELOG.md`, A-1 opened
     it; check, and if it is not there open it:
     `cmake -DTO=1.8.0 -P scripts/release/version.cmake`, committed
     alone. Everything this card adds to the CHANGELOG goes under it,
     not under 1.7.0 (which the header of this file names for the cards
     before it).
  1. `fake_gen.c`, `external.cmake` with its cases and the registration, and
     an `external.cmake` golden that is only `{}` and a newline. See it
     fail: `cmake --build --preset default && ctest --preset default -R gen.external`
     fails on `contract_json` (`--lang contract-json` is `no language`).
  2. `gen_contract_json` and the `contract-json` entry; write the golden file
     with the command the failure gives and read it whole. `contract_json`
     passes; the other cases fail (`no language 'fake'` without a
     directory).
  3. `spawn_find`, `spawn_run` (POSIX first), the order of a `--lang` in
     `gen.c`, the `no language` text. `ctest --preset default -R gen\\.`
     passes on Linux.
  4. `spawn_win32.c` and `spawn_none.c`; ACCEPT-WINDOWS: every case but
     `not_a_directory`, then `cmake --build` with the analyzer's flags if
     the machine has them (CI's MSVC `/analyze` otherwise).
  5. `docs/Tools.md`, then ACCEPT-LINUX with `-DEXTRA="tidy;shared"`,
     ACCEPT-WINDOWS.
* **accept**: ACCEPT-LINUX with `-DEXTRA="tidy;shared;compilers"` (clang-tidy reads
  the new tool files; the installed tool finds its library by its run
  path as before; `compilers` builds `spawn_posix.c` against musl, tcc
  and pcc, where `posix_spawn` and `environ` may differ), ACCEPT-WINDOWS, ACCEPT-CI (MSVC `/analyze`, macOS and
  FreeBSD build `spawn_posix.c`). `gen.external`, `gen.c` and the other
  `gen.*` pass; `-DEXTRA=wasm` still builds nothing of the tools.
* **stop if**: W5-G1's table has no entry type a writer can be added
  to in the way above (read `gen_lang` first: if the writer cannot be
  handed the schema's text and the `--name` as given, the entry's shape
  needs a change W5-G1 should have made: hand back with what the entry
  holds); `CommandLineToArgvW` counts a different number of arguments than
  the narrow `argv` for a command line the tests use (T-1's stop-if,
  here as well: name the line); `posix_spawn` or `environ` is not found
  by a compiler of the matrix (macOS needs `_NSGetEnviron()` in a
  library, not in a program: if CI's macOS job says otherwise, use it
  there and say so in the notes); or the golden file differs between
  Linux and Windows (a schema checked out with other line ends -
  `.gitattributes`' `* text=auto eol=lf` should prevent it: name the
  file).
* **docs**: add **D-110**, "Generators outside the tree", in D-80's
  shape: the question (a language the project does not ship); the
  options (a plug-in loaded into the program; a program found by name,
  as git does; a configuration file naming generators; the library's
  schema as the hand-over); the decision (the program by name, the order
  of the lookup and that a built-in always wins, the name rule, the
  JSON and each of its members and why every member is always present,
  the arguments passed through, the exit codes, `--lang contract-json`,
  `.exe` only on Windows and why, the pipe and why not a file or a
  captured output); compatibility (a new behaviour of a `--lang` that
  was an error; `--lang` absent still writes C, a name that is in the
  table still its writer). CHANGELOG, under `## 1.8.0 — unreleased`,
  `### Added`: "`configforge gen
  --lang <name>` runs `configforge-gen-<name>`, found beside
  `configforge-gen` or on the PATH, with the contract as JSON on its
  standard input - a generator in any language, outside the project
  (D-110); `--lang contract-json` writes that JSON." Roadmap: the row
  "`configforge gen --lang <lang>`, open to more languages" goes 🟩,
  `D-95, D-110`.
* **notes**:
