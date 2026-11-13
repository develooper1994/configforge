# Wave 4: binary configuration

Roadmap wave 4 as cards (docs/Plan.md says what a card is and how one is
carried out). Two pieces, which share no file until their last cards:

* **W4-C1 to W4-C4**: CBOR (RFC 8949), the JSON model in binary: a reader
  in the shape of the JSON one, held to RFC 8949's Appendix A vectors and
  Appendix F.1's malformed items, then an encoder, then detection from
  content. D-91.
* **W4-T1 to W4-T5**: tag, length, value. TLV has no one standard, so it
  is not a codec but a way to make one: the program describes its layout
  (`cf_tlv_layout`) and the library makes a `cf_codec` that reads it.
  BER-TLV (ASN.1 BER, ISO/IEC 7816-4, EMV) is one kind of layout. First the
  codec learns to carry data of its own (W4-T1, D-92), since a layout is
  data and `decode` has no argument to reach it by; then the layout and
  its reading (D-93).

Order: W4-C1, W4-C2, W4-C3, W4-C4, W4-T1, W4-T2, W4-T3, W4-T4, W4-T5.
The two pieces can run side by side until W4-C4 and W4-T5, which both
change `docs/Formats.md`, `docs/Guide.md` and the fuzzers; W4-T5 goes
after W4-C4. Every card is accepted on Linux and Windows; none is CI-only.

D-numbers: this file uses **D-91, D-92 and D-93**. The highest on the
branch when it was written was D-81; W3-F2b takes D-82, and the cards of
the hardening pass take the numbers above that, so these start at D-91.
Check with `git grep -n "^## D-9[1-3]" docs/Decisions.md` before writing
one: if the number is taken, take the next free one and change it in every
card of this file in the same commit.

The CHANGELOG: every card here adds to the topmost section headed
`— unreleased`. If that section has a date instead, or is 1.4.1 (a patch
release takes no features), add `## 1.5.0 — unreleased` with an
`### Added` heading above it first. Wave 4 is 1.5.0; the version number
itself and the tag are the release's, not a card's.

For the owner: nothing to decide and nothing to install. The CBOR vectors
are fetched by the conformance run as the other suites are (a network is
needed for W4-C2's step, as for theirs). No device's data is ever copied
into the repository: every TLV document a test reads is written by the
test from a layout.

---

## W4-C1 - The CBOR reader

* **id**: W4-C1
* **goal**: a `cbor` codec, on by default under `CF_WITH_CBOR`, reads RFC
  8949 CBOR whose top is a map into the configuration, every value placed
  in explain() by the byte it starts at, and everything a configuration
  cannot hold refused by name.
* **why**: a device or a service that keeps its settings in CBOR today has
  them copied into JSON to be read, and the copy is what explain() then
  names; read as CBOR, `server.port` says `settings.cbor, byte 14`, the
  place a hex dump of the real file shows it.
* **needs**: nothing
* **model**: sonnet
* **decided**:
  * The option, wired as `CF_WITH_PROPERTIES` is, in each place right after
    it: `option(CF_WITH_CBOR "CBOR codec, RFC 8949 (built in, no dependency)" ON)`
    in `cmake/ConfigForgeOptions.cmake`; `CBOR` in `CF_FEATURES`;
    `#ifndef CF_WITH_CBOR` / `#define CF_WITH_CBOR 1` / `#endif` in the
    source copy of `include/configforge/cf_build_options.h`;
    `#define CF_WITH_CBOR @CF_OPT_CBOR@` in
    `cmake/templates/cf_build_options.h.in`; `"cbor",` under
    `#if CF_WITH_CBOR` in `src/core/version.c`'s feature list. The feature's
    name for `cf_has_feature` is `"cbor"`. On in every preset, the minimal
    one included ("every optional part but the codecs off"); there
    `CF_WITH_REAL` is off and floats are refused by name.
  * The file: `src/serialize/cbor.c`, C89, guarded `#if CF_WITH_CBOR`,
    added in `src/serialize/CMakeLists.txt` as `properties.c` is and in
    `scripts/generate/amalgamate.py`'s list after `serialize/properties.c`.
    Every static name begins `cbor_` (the amalgamation is one translation
    unit). The codec:
    `const cf_codec cf_codec_cbor = {sizeof(cf_codec), "cbor", ".cbor", CF_CODEC_READ | CF_CODEC_TYPED | CF_CODEC_ARRAYS, cbor_decode, NULL, 0};`
    W4-C3 adds the writing.
  * The registry, `src/serialize/codec.c`: `extern const cf_codec cf_codec_cbor;`
    and `&cf_codec_cbor,` under `#if CF_WITH_CBOR`, each after the
    properties lines; and `{"cbor", ".cbor", "CF_WITH_CBOR"},` in
    `cf_formats` after the properties row, so a `.cbor` file in a build
    without it names the option.
  * Two helpers it needs and the TLV reader will need, so that neither
    repeats them:
    * In `include/configforge/cf_serialize.h`, after `cf_blob_skip_bom`:

      ```c
      /* Whether n bytes are UTF-8 as RFC 3629 has it: no overlong form, no
         surrogate (U+D800 to U+DFFF), nothing past U+10FFFF, no sequence cut
         short. A NUL is a character like any other. (1.5) */
      CF_API cf_bool cf_text_is_utf8(const char* p, size_t n);
      ```

      written in `src/serialize/blob.c`.
    * In `include/configforge/cf_numeric.h`, under `#if CF_WITH_REAL`:
      `CF_API cf_u64 cf_f64_bits(double d);` and
      `CF_API double cf_f64_from_bits(cf_u64 bits);` - a double as its
      IEEE 754 bit pattern and back, by `memcpy`; and
      `CF_API double cf_f64_from_f16_bits(cf_u32 bits);` and
      `CF_API double cf_f64_from_f32_bits(cf_u32 bits);` - a half- and a
      single-precision pattern (in the low 16 and 32 bits) as a double,
      exactly, non-finite ones included. All four in `src/core/numeric.c`.
      `src/bridge/image.c`'s `f64_bits` and `bits_f64` are removed and its
      calls use the first two.
    The symbol list gains `cf_text_is_utf8`, and the four with `real` after
    each (the word after a name is the feature it needs); regenerate it with
    `cmake -DSRC=. -DWRITE=ON -P cmake/gates/GateAbi.cmake`.
  * The data model. Read: a map (definite or indefinite length) with text
    keys, arrays (either length), unsigned and negative integers, text
    strings (either length), `false`, `true`, `null`, and floats of half,
    single and double precision. A map is a table and an array an array,
    to the flattener, exactly as JSON's are. A text string is typed text
    (`textual` false, as a JSON string is). An integer is an int. A finite
    float is a real; Infinity, -Infinity and NaN are the text `inf`,
    `-inf` and `nan` with `textual` true, typed by the contract, as TOML's
    are (D-62). `null` is a null.
  * The decisions where CBOR holds more than a configuration does:
    * **Byte strings**: refused, either length, as a value and as a key. A
      configuration has no type for bytes, and reading them as hex or as
      base64 would be choosing an encoding nobody asked for.
    * **Tags**: 55799 (self-described CBOR, `D9 D9 F7`) is read past
      wherever it is; 0 (a date and time, its content a text string), 1
      (seconds since the epoch, its content a number) and 32 (a URI, its
      content a text string) are read as their content, which is checked
      against the tag; every other tag is refused by its number, before its
      content is read. Read past, tag 4 (a decimal fraction, `[exponent,
      mantissa]`) would be an array where a number was written; refused, it
      says what it is.
    * **Indefinite lengths**: read, for maps, arrays, text strings and map
      keys. Streaming encoders write them and Appendix A has them. A text
      string's chunks are concatenated; each chunk must be a definite-length
      text string and valid UTF-8 by itself (RFC 8949 section 3.2.3).
    * **Map keys that are not text**: refused, naming what the key is.
    * **Duplicate keys**: read, the later winning, and both kept as the
      key's candidates in this source - exactly as the JSON and INI readers
      do. explain() shows both places and Doctor's CFD001 names the earlier
      as one that can never win, so nothing is dropped where it cannot be
      seen; refusing would make CBOR the one reader that refuses what JSON
      accepts. Rejected: refusing them as RFC 8949 section 5.6 permits,
      which needs a table of names per map for a check the library already
      makes, visibly, one level up.
    * **`undefined` and the simple values other than false, true and null**:
      refused.
    * **Integers outside the 64-bit signed range** (an unsigned above
      CF_I64_MAX, a negative below CF_I64_MIN): refused. `-1 - n` with
      `n == CF_I64_MAX` is CF_I64_MIN and is read.
    * **The top**: a map, after any number of 55799 tags. Nothing may follow
      it: a CBOR sequence (RFC 8742) is another format. No byte order mark
      is skipped: `EF` is a CBOR simple value, not a mark.
    * Well-formedness is RFC 8949's: a head with additional information 28,
      29 or 30, a two-byte simple value below 32, an indefinite length on
      major type 0, 1 or 6, a break outside an indefinite-length item, a
      chunk of the wrong type, an odd map before its break, and an item cut
      short are each refused. A head longer than it needs to be (`18 05`
      for 5) is well-formed and read.
  * Floats, by the helpers above: half precision by `ldexp` (`<math.h>`,
    which `numeric.c` already uses), as RFC 8949 Appendix D does it:
    exponent 0 is `ldexp(mant, -24)`, 31 is infinity (mant 0) or NaN, else
    `ldexp(mant + 1024, exp - 25)`; single precision the same way
    (`ldexp(mant, -149)` for exponent 0, `ldexp(mant | 0x800000, exp - 150)`
    otherwise, 255 infinity or NaN); double precision by `cf_f64_from_bits`
    of the eight bytes, big-endian. The sign applied last. Under
    `!CF_WITH_REAL` every float is refused, non-finite ones too.
  * Places. Every scalar's `cf_loc` is `{ s->where, 0, 0 }`, `where`
    formatted as `"%s, byte %zu"` with the blob's name (`"<cbor>"` when it
    has none) and the offset of the data item's first byte - the first of
    its tags when it has any. Line 0 makes `cf_loc_text` print the place as
    it stands. `where` is a member of the scanner, `char where[CF_PATH_MAX + 32]`.
    Decimal, counted from 0: what `dd skip=` and `cmp` count in.
  * The error wordings, every one `CF_E_SOURCE` with the blob's name as the
    source unless said otherwise, and every one `"%s, byte %zu: ..."` with
    the name and the offset of the item at fault:
    * `"%s: the document is empty"` (no offset)
    * `"... a configuration is a map at the top, and this is %s"` with one
      of: `an unsigned integer`, `a negative integer`, `a byte string`,
      `a text string`, `an array`, `a tag`, `a simple value`, `a float`
    * `"... bytes after the document"`
    * `"... the document ends inside a data item"`
    * `"... not well-formed: additional information %u is reserved"`
    * `"... not well-formed: major type %u has no indefinite length"`
    * `"... not well-formed: a break outside an indefinite-length item"`
    * `"... not well-formed: a two-byte simple value below 32"`
    * `"... not well-formed: a chunk of an indefinite-length string is a definite-length string of its type"`
    * `"... not well-formed: a key with no value before the break"`
    * `"... a length of %U, and %zu bytes are left"` (a string),
      `"... an array of %U items, and %zu bytes are left"`,
      `"... a map of %U pairs, and %zu bytes are left"` - each checked
      before the item is read (an item is at least a byte, a pair two), so
      a count of four billion costs nothing
    * `"... a text string that is not UTF-8"`
    * `"... nests deeper than %d levels"` with `CF_MAX_DEPTH`; the top map
      is the first level, each map and array inside one more
    * `"... this build has no floating point support"` (json.c's words)
    * The model's refusals, each ending in one of two phrases the
      conformance run (W4-C2) counts by: `"... a byte string, which a configuration does not hold"`,
      `"... undefined, which a configuration does not hold"`,
      `"... simple value %u, which a configuration does not hold"`,
      `"... an integer past the 64-bit signed range, which a configuration does not hold"`,
      `"... a map key that is %s, which a configuration does not hold"`
      (the kinds above), and
      `"... tag %U, which this reader does not read: it reads tags 0, 1, 32 and 55799"`
    * `"... tag %U holds %s, and must hold a text string"` (0, 32) and
      `"... tag %U holds %s, and must hold a number"` (1)
    * `"... an indefinite-length text string needs sink text storage"`
      (no `reserve_text` and 256 bytes or more; under 256 a local buffer
      is used, as json.c does) and `CF_E_MEMORY`
      `"no room to decode a CBOR text string"` when it returns NULL
    * `"... an indefinite-length map key needs sink work storage"`: such a
      key is gathered into `reserve_work` memory, which lasts the decode,
      because the name must outlive the value's own parse; a definite key
      is passed as a pointer into the blob and needs nothing
    * a sink that refuses a name: `CF_E_KEY`, `"... '%S' is not a valid key"`
      (properties.c's words); the root table refused: `"%s: sink refused"`;
      the last `end` refused: `"%s: the document could not be recorded"` (ini.c's)
  * The recursion: one function per item (`cbor_item`), and a map's and an
    array's loops call it; tags are a loop, not recursion, so a run of a
    thousand tags is a thousand bytes and no frames. No buffer on a frame
    but the 256 bytes of the text function's local one.
  * Rejected: refusing indefinite lengths (Appendix A has them, and
    streaming encoders write nothing else); reading every tag's content
    as if untagged (tag 4 and tag 2's bignums change meaning, silently);
    a flag that reads byte strings as hex (TLV has a type for that, W4-T3,
    where the layout says so; a CBOR file says nothing).
* **files**: `cmake/ConfigForgeOptions.cmake`,
  `include/configforge/cf_build_options.h`,
  `cmake/templates/cf_build_options.h.in`, `src/core/version.c`,
  `src/serialize/cbor.c` (new), `src/serialize/CMakeLists.txt`,
  `src/serialize/codec.c`, `src/serialize/blob.c`,
  `include/configforge/cf_serialize.h`, `include/configforge/cf_numeric.h`,
  `src/core/numeric.c`, `src/bridge/image.c`,
  `scripts/generate/amalgamate.py`, `tests/unit/test_cbor.c` (new),
  `tests/unit/test_features.c`; last: `tests/unit/CMakeLists.txt`,
  `cmake/configforge.symbols`, `docs/API.md` (regenerated),
  `docs/Decisions.md`, `CHANGELOG.md`, `docs/Roadmap.md`.
* **steps**:
  1. The option, both copies of `cf_build_options.h`, the feature string,
     and in `tests/unit/test_features.c`, beside the properties line,
     `check(cf_has_feature("cbor") == (CF_WITH_CBOR ? CF_TRUE : CF_FALSE), "cbor as built");`.
  2. The two helpers and their symbol lines; image.c switched to the
     numeric ones. `cmake --preset default && cmake --build --preset default && ctest --preset default -R "unit.test_(image|features)"`
     passes.
  3. `cbor.c` with the codec, registered, whose `cbor_decode` sets
     `CF_E_SOURCE` `"%s: not read yet"` and returns false (a placeholder,
     to see the tests fail).
  4. `tests/unit/test_cbor.c`, added after `test_credentials` in
     `tests/unit/CMakeLists.txt`'s list. Documents are byte arrays
     (`static const unsigned char`), read through
     `cf_builder_document(b, "doc", CF_RANK_USER_FILE, (const char*)bytes, sizeof bytes, "cbor", 0)`
     unless said; a `read(bytes, n, &err)` helper returns the config.
     `main` runs the CBOR tests under `#if CF_WITH_CBOR` and `test_off()`
     otherwise. The cases:
     * `test_scalars`: `A7 61 61 00 61 62 20 61 63 1A 00 0F 42 40 61 64 64 49 45 54 46 61 65 F5 61 66 F4 61 67 F6`:
       `a` 0, `b` -1, `c` 1000000, `d` "IETF", `e` true, `f` false, and
       `g` a null: `cf_has(c, "g")` and `cf_type_of(c, "g") == CF_TYPE_NULL`
       (as `tests/unit/test_null.c` asks of one).
     * `test_floats` (`#if CF_WITH_REAL`): `A4 61 68 F9 3E 00 61 69 FA 47 C3 50 00 61 6A FB 3F F1 99 99 99 99 99 9A 61 6B F9 00 01`:
       `h` 1.5, `i` 100000.0, `j` 1.1, `k` 5.960464477539063e-08 (each
       compared `==`); and `A3 61 70 F9 7C 00 61 6E F9 FC 00 61 71 F9 7E 00`:
       `p` text "inf", `n` "-inf", `q` "nan". Without `CF_WITH_REAL`,
       `test_no_real`: `A1 61 68 F9 3E 00` refused, the message has
       "no floating point support".
     * `test_places`: `A1 66 73 65 72 76 65 72 A2 64 70 6F 72 74 19 1F 90 65 68 6F 73 74 73 82 61 61 61 62`
       (`{"server": {"port": 8080, "hosts": ["a", "b"]}}`): `server.port`
       8080 and `cf_explain` of it contains `doc, byte 14`;
       `server.hosts.count` 2; `server.hosts.1` "b", explained at `doc, byte 26`.
     * `test_indefinite`: `BF 61 61 7F 65 73 74 72 65 61 64 6D 69 6E 67 FF 61 62 9F 01 02 FF FF`:
       `a` "streaming", `b.count` 2, `b.1` 2; and `BF 7F 61 61 61 62 FF 01 FF`:
       `ab` 1 (an indefinite key).
     * `test_tags`: `D9 D9 F7 A1 61 74 C0 74` + the 20 bytes of
       `2013-03-21T20:04:00Z`: `t` is that text; `A1 61 65 C1 1A 51 4B 67 B0`:
       `e` 1363896240; `A1 61 78 C4 82 21 19 6A B3`: refused, the message
       has "tag 4" and "which this reader does not read"; `A1 61 74 C0 01`:
       refused, "must hold a text string".
     * `test_duplicates`: `A2 61 61 01 61 61 02`: `a` is 2, and its
       explain() contains `doc, byte 3` and `doc, byte 6`.
     * `test_refusals`, one document each, `read` NULL and the message
       containing the words after the arrow:
       `A1 61 62 41 00` -> "a byte string";
       `A1 61 75 F7` -> "undefined";
       `A1 61 73 F0` -> "simple value 16";
       `A1 01 02` -> "a map key that is an unsigned integer";
       `A1 61 62 1B FF FF FF FF FF FF FF FF` -> "past the 64-bit signed range";
       `A1 61 6E 3B 80 00 00 00 00 00 00 00` -> "past the 64-bit signed range";
       `82 01 02` -> "a map at the top, and this is an array";
       an empty document -> "the document is empty";
       `A1 61 61` -> "ends inside a data item";
       `A1 61 61 01 00` -> "bytes after the document";
       `A1 61 61 1C` -> "additional information 28 is reserved";
       `A1 61 61 FF` -> "a break outside";
       `A1 61 61 F8 10` -> "a two-byte simple value below 32";
       `A1 61 61 62 C3 28` -> "not UTF-8";
       `A1 61 61 9B 00 00 00 00 FF FF FF FF` -> "an array of 4294967295 items";
       `BF 61 61 FF` -> "a key with no value";
       `A1 61 61 7F 41 00 FF` -> "a chunk of an indefinite-length string".
       And one read, not refused: `A1 61 6E 3B 7F FF FF FF FF FF FF FF` is
       `n` == `CF_I64_MIN`.
     * `test_depth`: built in a buffer: `A1 61 61`, then `81` k times, then
       `00`. With `k = CF_MAX_DEPTH - 1` it reads (the map and 31 arrays
       are 32 levels); with `k = CF_MAX_DEPTH` it is refused, "nests deeper than".
     * `test_utf8`: `cf_text_is_utf8` true for "", "abc", `C3 A9`,
       `F0 9F 98 80`, and the three bytes `61 00 62`; false for `C0 80`
       (overlong), `ED A0 80` (a surrogate), `F4 90 80 80` (past U+10FFFF),
       `E2 82` (cut short).
     * `test_extension`: the test writes `cf_cbor_test.cbor` (`fopen` "wb")
       with `test_places`' bytes, reads it with
       `cf_builder_file(b, "file", CF_RANK_USER_FILE, "cf_cbor_test.cbor", NULL, 0)`,
       and finds `server.port` 8080; removes the file.
     * `test_off` (`!CF_WITH_CBOR`): `cf_codec_by_name("cbor") == NULL`;
       `cf_format_option("cbor")` is "CF_WITH_CBOR"; building from a
       `.cbor` file fails and the message contains "CF_WITH_CBOR".
  5. See it fail: `cmake --build --preset default && ctest --preset default -R unit.test_cbor`
     fails on "not read yet".
  6. Write the reader. Rerun step 5: passes.
  7. The option off: `cmake -S . -B build-nocbor -DCF_WITH_CBOR=OFF && cmake --build build-nocbor && ctest --test-dir build-nocbor -R "unit.test_(cbor|features)"`
     passes (it runs `test_off`). Remove `build-nocbor`.
  8. Write the decision (below). `cmake -P scripts/format.cmake` (it
     regenerates `docs/API.md`), then ACCEPT-LINUX and ACCEPT-WINDOWS.
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI. `gate.abi`,
  `gate.amalgamation` and `gate.formats` (`scripts/check/check_formats.py`:
  every format string uses only cf_fmt's conversions - `%zu`, `%U`, `%u`,
  `%S`, `%s`, `%d`) pass; the minimal preset runs `test_no_real`.
* **stop if**: `cf_builder_document` does not hand the codec all `n` bytes
  (it stops at a NUL, or skips a byte order mark for every codec) - then
  binary documents need a change to the document source first; hand back.
  Or the flattener refuses a null scalar (JSON's nulls go through it, so it
  must not). Or `gate.headers` refuses `double` in `cf_numeric.h` under
  `#if CF_WITH_REAL` (then the helpers move to an internal header; hand back).
* **docs**: add **D-91**, "CBOR, the JSON model in binary", in the shape of
  D-80: the question (a device's or a service's settings in CBOR, read
  without a copy into JSON); the options (no CBOR; CBOR through a codec a
  program supplies, D-68; a built-in reader); the decision (built in and on,
  as JSON is: no dependency, a few hundred lines; the data model; each of
  the decisions above with its reason, byte strings, tags, indefinite
  lengths, keys, duplicates, undefined and simple values, the integer range,
  the top, non-finite floats as text; places as `<file>, byte <n>`);
  compatibility (additive: an option on by default, three functions). Its
  "Held to" paragraph is W4-C2's. CHANGELOG, Added: "**CBOR** (RFC 8949),
  read: `.cbor` files and documents named `cbor`, maps of text keys,
  arrays, integers, floats of every width, text, booleans and null, each
  value placed in explain() by the byte it starts at; byte strings, tags
  other than 0, 1, 32 and 55799, undefined and a key that is not text are
  refused by name. `CF_WITH_CBOR`, on by default." Roadmap: the row
  "Binary configuration: a layout-driven TLV codec, BER-TLV, CBOR" goes 🟨,
  its "Where" `D-91`, and wave 4's text gains "CBOR read (W4-C1)".
* **notes**:

---

## W4-C2 - CBOR held to RFC 8949's vectors

* **id**: W4-C2
* **goal**: the conformance run scores the CBOR reader on RFC 8949 Appendix
  A's vectors and Appendix F.1's malformed items, as the other readers are
  scored on their suites, against floors that only rise.
* **why**: "reads CBOR" is a claim until the standard's own examples say
  so; a vector read to the wrong value would be a value that explains
  itself correctly and is wrong.
* **needs**: W4-C1
* **model**: sonnet
* **decided**:
  * Where the vectors live: fetched, never vendored, as every suite is.
    The repository is `https://github.com/cbor/test-vectors`, its file
    `appendix_a.json`: a list of objects with `cbor` (the bytes, base64),
    `hex`, `roundtrip`, and either `decoded` (the value as JSON) or
    `diagnostic` (RFC 8949's notation, where JSON has no form). In
    `scripts/verify.cmake`, beside the others: `set(url_cbor-test-vectors https://github.com/cbor/test-vectors)`
    and `set(commit_cbor-test-vectors <the commit>)`, and
    `cbor-test-vectors` appended to the `foreach(sname ...)` list. The
    commit is the one `git ls-remote https://github.com/cbor/test-vectors HEAD`
    prints when the card is carried out; it is written into verify.cmake
    and into `docs/Conformance.md`'s table in the same commit.
  * The malformed items are not in that repository. They are RFC 8949
    Appendix F.1's list, typed into the script as `INVALID_CBOR`, a tuple
    of `(group, hex)` pairs, the group being F.1's heading for it: hex
    strings are facts, not text of the RFC's. When this was written the
    list was these 94 (check them against the RFC's text, which wins):
    * end of input in a head: `18`, `19`, `1a`, `1b`, `1901`, `1a0102`,
      `1b01020304050607`, `38`, `58`, `78`, `98`, `9a01ff00`, `b8`, `d8`,
      `f8`, `f900`, `fa0000`, `fb000000`
    * definite-length strings with short data: `41`, `61`,
      `5affffffff00`, `5bffffffffffffffff010203`, `7affffffff00`,
      `7b7fffffffffffffff010203`
    * definite-length maps and arrays not closed with enough items: `81`,
      `818181818181818181`, `8200`, `a1`, `a20102`, `a100`, `a2000000`
    * tag number not followed by tag content: `c0`
    * indefinite-length strings not closed by a break: `5f4100`, `7f6100`
    * indefinite-length maps and arrays not closed by a break: `9f`,
      `9f0102`, `bf`, `bf01020102`, `819f`, `9f8000`, `9f9f9f9f9fffffffff`,
      `9f819f819f9fffffff`
    * reserved additional information values: `1c`, `1d`, `1e`, `3c`, `3d`,
      `3e`, `5c`, `5d`, `5e`, `7c`, `7d`, `7e`, `9c`, `9d`, `9e`, `bc`,
      `bd`, `be`, `dc`, `dd`, `de`, `fc`, `fd`, `fe`
    * reserved two-byte encodings of simple values: `f800`, `f801`,
      `f818`, `f81f`
    * indefinite-length string chunks not of the correct type: `5f00ff`,
      `5f21ff`, `5f6100ff`, `5f80ff`, `5fa0ff`, `5fc000ff`, `5fe0ff`,
      `7f4100ff`
    * indefinite-length string chunks not definite length: `5f5f4100ffff`,
      `7f7f6100ffff`
    * a break on its own: `ff`
    * a break in a definite-length array, map or tag: `81ff`, `8200ff`,
      `a1ff`, `a1ff00`, `a100ff`, `a20000ff`, `9f81ff`, `9f829f819f9fffffffff`
    * a break in a map's value position: `bf00ff`, `bf000000ff`
    * major type 0, 1 or 6 with additional information 31: `1f`, `3f`, `df`
  * How `conform` reads a vector (`tests/conformance/conform.c`): for the
    format `cbor`, the document is always wrapped as the value of a map's
    member `v` - the three bytes `A1 61 76` before it - as JSON5's root is
    always wrapped there, since a vector is mostly a lone value and a
    configuration is a map. Wrapping keeps a malformed item malformed (the
    map's value is cut short, or holds a stray break).
  * How `scripts/check/conformance.py` scores it:
    * `cbor_suite(root)` reads `<root>/cbor-test-vectors/appendix_a.json`
      and writes each vector's bytes (`base64.b64decode(v["cbor"])`) to
      `a<NNN>.cbor`, and each `INVALID_CBOR` item's to `f<NNN>.cbor`
      (`bytes.fromhex`), in a directory from `tempfile.mkdtemp()` made in
      `main()` and removed with `shutil.rmtree` in a `finally:` at its end
      (the detection score reads the files after the report).
      It returns the valid paths, the invalid paths, and
      `{path: vector}` for the values.
    * `report("cbor", "cbor", conform, valid, invalid, out, accepted)`, as
      the others are. `MODEL` gains `"which a configuration does not hold"`
      and `"which this reader does not read"`, so a vector refused by name
      for the configuration's model (a byte string, a tag such as 23 or 24,
      a key that is an integer, undefined, a simple value, an integer past
      64 bits) is counted apart, as YAML's are.
    * `cbor_values(conform, valid, vectors, wrong_out)`: of the vectors
      with a `decoded` value that were read, how many were read to it:
      `want = {}`, `flat({"v": v["decoded"]}, None, want)`, compared with
      `keys_of(conform, "cbor", path)` by `same()`. `(good, compared)`.
      Printed as `cbor-values: read as RFC 8949 Appendix A states <good>/<compared>`
      with the others in the values loop, floor name `cbor-values`.
    * `only` may be `"cbor"` (`--codec ... cbor` scores a codec of one's own
      on these vectors alone).
  * The floor, `tests/conformance/floor.txt`, three lines under a comment
    `# CBOR (D-91): RFC 8949 Appendix A's vectors, read or refused by name for the model; Appendix F.1's malformed items, refused`:
    `cbor <valid accepted> <invalid refused>` and `cbor-values <n>`, with the
    numbers the first run prints. What they must be before they are written
    down: every Appendix A vector accepted or refused by name (none refused
    for another reason), all 94 malformed items refused, and every vector
    with a `decoded` value that was accepted read to that value. A miss is a
    reader bug, fixed in `cbor.c` in this card. `detect-cbor` is W4-C4's.
* **files**: `scripts/verify.cmake`, `scripts/check/conformance.py`,
  `tests/conformance/conform.c`, `tests/conformance/floor.txt`,
  `src/serialize/cbor.c` (only for a miss); last: `docs/Conformance.md`,
  `docs/Dependencies.md`, `docs/Decisions.md`, `CHANGELOG.md`,
  `docs/Roadmap.md`.
* **steps**:
  1. `git ls-remote https://github.com/cbor/test-vectors HEAD`; record the
     commit. Fetch it into a scratch directory and confirm
     `appendix_a.json` has the fields above and the repository's licence
     (the file `LICENSE`, or the README); write the licence into
     `docs/Dependencies.md`'s test-data row beside the others ("none
     stated" if none is: the data is fetched and never vendored).
  2. The conform wrapping; `cmake --build --preset default --target conform`.
  3. The script: `cbor_suite`, `INVALID_CBOR`, `cbor_values`, the two MODEL
     phrases, `cbor` in `main()`; floor lines with `0 0` and `cbor-values 0`
     (placeholders).
  4. See it fail on purpose: make `cbor_decode` return false at once (the
     early return of CONTRIBUTING.md), run
     `python3 scripts/check/conformance.py build/default/tests/conformance/conform <suites> --floor tests/conformance/floor.txt --list`
     with `<suites>` the directory verify fetched into (or fetch with
     `cmake -DSOURCE=index -DJOBS=3 -DWORK=$HOME/cfv -DEXTRA=conformance -P scripts/verify.cmake`):
     `cbor: valid accepted 0/...` and every invalid refused. Remove the early return.
  5. Run it again with `--list`. Every valid vector refused for a reason
     outside the two phrases, every malformed one accepted, and every value
     that differs is a fix in `cbor.c`; repeat until there are none.
  6. Write the floors from that run. Run the whole conformance step:
     `cmake -DSOURCE=index -DJOBS=3 -DWORK=$HOME/cfv -DEXTRA=conformance -P scripts/verify.cmake`
     ends `all green`.
  7. ACCEPT-LINUX, ACCEPT-WINDOWS.
* **accept**: ACCEPT-LINUX with `-DEXTRA=conformance`, ACCEPT-WINDOWS,
  ACCEPT-CI.
* **stop if**: `appendix_a.json` is not there or has other fields (the
  script's design changes; hand back); a vector's `decoded` contradicts a
  rule W4-C1 decided (then the rule, not the script, is in question: write
  which vector and why under "notes" and hand back); the reader cannot be
  made to refuse a malformed item without a design change.
* **docs**: D-91 gains "**Held to.**": RFC 8949 Appendix A, through the
  vectors of `cbor/test-vectors` at the pinned commit, each read as the
  member of a map, `n` of `m` read and the rest refused by name for the
  model; Appendix F.1's 94 malformed items, all refused; `k` of `k` read to
  their stated values. `docs/Conformance.md`: the suite's row, a "CBOR" row
  in the scores, and a paragraph on the wrapping and the model's refusals,
  as JSON's and YAML's are written. CHANGELOG: the CBOR entry gains "held to
  RFC 8949's Appendix A vectors and its malformed examples". Roadmap: wave
  4's text "CBOR read and held to RFC 8949's vectors".
* **notes**:

---

## W4-C3 - Writing CBOR

* **id**: W4-C3
* **goal**: `cf_emit(c, "cbor", ...)` writes CBOR that the reader reads back
  to the same keys, types and values, with RFC 8949's preferred
  serialization of every number, and a literal `${` kept literal as the
  text formats keep it.
* **why**: a program that reads CBOR must be able to save an overrides file
  in it, and a conversion (W5) to CBOR needs a writer; a writer whose output
  the reader reads differently would make explain() describe a file that
  is not the one written.
* **needs**: W4-C1
* **model**: opus
* **decided**:
  * The writing is the same event interface as every encoder: state the
    caller supplies, a `cf_doc_sink` the encoder fills (`size` the 1.0
    prefix, `offsetof(cf_doc_sink, reserve_text)`, as json.c's does).
  * Maps and arrays are written with **indefinite length** (`BF ... FF`,
    `9F ... FF`): the events do not say how many members a container has
    until its `end`, and the sink measures or writes in one pass, so a
    count cannot be written before the members without a second walk or
    moving bytes already written. Indefinite length is well-formed CBOR that
    every decoder reads. Text strings and numbers are definite and in
    preferred serialization (RFC 8949 section 4.1): an integer in its
    shortest head; a real as the shortest of half, single and double
    precision that holds it exactly; NaN as `F9 7E 00`, infinities as
    `F9 7C 00` and `F9 FC 00`. Not deterministic encoding (section 4.2.1):
    the walk writes a table's values before its tables, not keys in byte
    order. Rejected: a second walk to count each container, which doubles
    the work for what no reader needs.
  * Exactness, under `CF_WITH_REAL`: single precision holds `d` when
    `fabs(d) <= FLT_MAX` (`<float.h>`; converting a larger double to float
    is undefined) and `(double)(float)d == d`; its bits by `memcpy` into a
    `cf_u32` (a compile-time check that `sizeof(float) == 4`). Half holds it
    when, from the single's bits (sign `s`, exponent `e`, mantissa `m`):
    `e == 0 && m == 0` (zero, `s << 15`); or `E = e - 127` in -14..15 and
    `m & 0x1FFF == 0` (half `s<<15 | (E+15)<<10 | m>>13`); or `E` in
    -24..-15 and the low `-(E+1)` bits of `0x800000 | m` are zero (half
    `s<<15 | (0x800000|m) >> -(E+1)`). Double precision is `cf_f64_bits`,
    eight bytes big-endian.
  * **`${` in a binary format.** `cf_emit` doubles every `${` in a text value
    after the encoder has written it (`escape_written` in `src/bridge/emit.c`),
    which is right for a text format and wrong for CBOR, whose text strings
    carry their length before their bytes: the doubled text would no longer
    match it. So a binary encoder is told to do it itself:
    * `include/configforge/cf_serialize.h`, in the caps enum:
      `CF_CODEC_BINARY = 1u << 5` with the comment "writes bytes, each text
      with its length before it: cf_emit cannot double a ${ in what it has
      written, and asks the encoder to (cf_encoder_head). (1.5)".
    * After `CF_ENCODER_STATE_MAX`:

      ```c
      /* The first member of a binary encoder's state (CF_CODEC_BINARY):
         the encoder function zeroes it, and the caller may then set
         CF_ENCODE_ESCAPE_REFS, which has every ${ in a text value written
         as $${, as cf_emit does for the text formats after writing.
         (1.5, D-91) */
      typedef struct cf_encoder_head {
          unsigned flags;
      } cf_encoder_head;
      enum { CF_ENCODE_ESCAPE_REFS = 1u << 0 };
      ```

    * `cf_emit`: after `codec->encoder(...)` returns true, when the codec has
      `CF_CODEC_BINARY`, `((cf_encoder_head*)(void*)state.b)->flags |= CF_ENCODE_ESCAPE_REFS;`
      and `emit_literals` gains `cf_bool binary`, set from the caps, under
      which `literal_scalar` passes the scalar on and does nothing after it.
      `cf_doc_emit` alone (no `cf_emit`) escapes nothing, as its header says
      today, and neither does an encoder whose caller sets no flag - which
      is what a conversion (W5) needs: a reader's text is the text as
      written.
    * Rejected: an escaped copy made by cf_emit before the encoder sees the
      value (no memory to make it in, and a bound on it would be a new
      refusal); an encoder that always escapes (a conversion would then
      double what the source wrote once).
  * `src/serialize/cbor.c`, under `#if CF_WITH_EMIT`:
    `typedef struct cbor_enc { cf_encoder_head head; cf_sink* out; int depth; } cbor_enc;`
    with `typedef char cbor_enc_fits[(sizeof(cbor_enc) <= CF_ENCODER_STATE_MAX) ? 1 : -1];`
    as json.c asserts. `begin_table`: the document (depth -1) writes `BF`
    and is depth 0; inside, a name (a map member) is written as a text
    string, then `BF`, and the depth grows - refused, returning false, at
    `CF_MAX_DEPTH - 1` as json.c's `js_enc_open` refuses (the walk refuses
    first; this is the last line of defence). `begin_array`: the name, then
    `9F`. `scalar`: the name, then the value - text (escaped when the head's
    flag is set: the length counts each `$${` as three bytes), int, bool
    (`F5`, `F4`), null (`F6`), real. `end`: `FF`. The codec becomes
    `{sizeof(cf_codec), "cbor", ".cbor", CF_CODEC_READ | CF_CODEC_WRITE | CF_CODEC_TYPED | CF_CODEC_ARRAYS | CF_CODEC_BINARY, cbor_decode, cbor_encoder, sizeof(cbor_enc)}`,
    and under `!CF_WITH_EMIT` stays as W4-C1 left it.
  * The C++ wrapper's `Config::emit` returns a `std::string` of the length
    `cf_emit` gives, which holds a NUL as well as any other byte; nothing to
    change there, and `tests/unit/test_wrapper_write.cpp` gains one line
    that emits `cbor` and checks the length (below).
* **files**: `include/configforge/cf_serialize.h`, `src/bridge/emit.c`,
  `src/serialize/cbor.c`, `tests/unit/test_cbor.c`,
  `tests/unit/test_roundtrip.c`, `tests/unit/test_wrapper_write.cpp`; last:
  `docs/API.md` (regenerated), `docs/Decisions.md`, `CHANGELOG.md`,
  `docs/Roadmap.md`.
* **steps**:
  1. The header's flag and struct; emit.c's change. The suite is unchanged:
     `cmake --build --preset default && ctest --preset default` passes.
  2. Tests, in `test_cbor.c` under `#if CF_WITH_EMIT`:
     * `test_write_bytes`: decode the JSON
       `{"b":{"yes":true},"n":{"zero":0,"small":23,"byte":24,"k":1000,"neg":-1000,"big":1000000000000},"t":{"s":"IETF"}}`
       into a key space through the flattener and `cf_ks_resolve` it, then
       encode with the codec's encoder and `cf_doc_emit`, as
       test_roundtrip.c's `encode()` does; the bytes are exactly
       `BF 61 62 BF 63 79 65 73 F5 FF 61 6E BF 63 62 69 67 1B 00 00 00 E8 D4 A5 10 00 64 62 79 74 65 18 18 61 6B 19 03 E8 63 6E 65 67 39 03 E7 65 73 6D 61 6C 6C 17 64 7A 65 72 6F 00 FF 61 74 BF 61 73 64 49 45 54 46 FF FF`.
       Under `CF_WITH_REAL`, `{"r":{"half":1.5,"single":100000.0,"double":1.1}}`
       is exactly
       `BF 61 72 BF 66 64 6F 75 62 6C 65 FB 3F F1 99 99 99 99 99 9A 64 68 61 6C 66 F9 3E 00 66 73 69 6E 67 6C 65 FA 47 C3 50 00 FF FF`.
     * `test_write_refs`: a configuration with
       `cf_builder_memory(b, "m", CF_RANK_USER_FILE, pairs, 1)`, the pair
       `{"t", "a$${b}"}` (so the value is the literal `a${b}`); `cf_emit(c, "cbor", CF_EMIT_CONFIGURED, 0, buf, sizeof buf, &err)`
       returns 11 and the bytes are `BF 61 74 66 61 24 24 7B 62 7D FF`; with
       a NULL buffer it also returns 11; and those bytes read back through
       `cf_builder_document(..., "cbor", 0)` give `t` as `a${b}`.
     * In `test_roundtrip.c`, `{"cbor", EXACT, 1, 1, 1}` after json's row of
       `g_formats`: every round trip that file has runs through CBOR too.
     * In `test_wrapper_write.cpp`, beside its emit checks:
       `cfg.emit("cbor", CF_EMIT_ALL)` is not empty and its first byte is
       `'\xBF'`, under `#if CF_WITH_CBOR && CF_WITH_EMIT`.
  3. See them fail: `ctest --preset default -R "unit.test_(cbor|roundtrip)"`
     fails (the codec cannot write: "can be read but not written").
  4. The encoder. Rerun step 3: passes.
  5. ACCEPT-LINUX (the minimal preset has no `CF_WITH_EMIT`: the encoder
     and its tests are compiled out there), ACCEPT-WINDOWS.
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI.
* **stop if**: `test_roundtrip.c`'s comparison cannot judge a binary
  output (it compares text with `strcmp` somewhere rather than by length -
  read `g_len`'s uses first; if so the harness needs a change of its own,
  hand back); or a key space built from the JSON above holds keys besides
  those written (then the exact bytes are wrong, not the encoder: hand back).
* **docs**: D-91 gains "**Writing.**": indefinite-length containers and
  why, preferred serialization of numbers, not deterministic encoding and
  why, and the binary encoder's head with what was rejected. CHANGELOG, the
  CBOR entry gains "and written (`cf_emit(c, \"cbor\", ...)`), numbers in
  their preferred serialization"; and one more Added line: "`CF_CODEC_BINARY`
  and `cf_encoder_head`: a codec of one's own that writes bytes can keep
  `${` literal as the built-in ones do". Roadmap: wave 4's text "CBOR read,
  written and held to RFC 8949's vectors".
* **notes**:

---

## W4-C4 - CBOR from content, fuzzed and documented

* **id**: W4-C4
* **goal**: a CBOR document with no extension is read as CBOR from its
  content, the CBOR reader is fuzzed with the others, and the Guide and
  `docs/Formats.md` say what it reads, writes and refuses.
* **why**: a document fetched from a server or held in an environment
  variable has no name to give its format (D-73); CBOR is the one format
  here whose first byte cannot begin a text document, so the content can
  say it with no ambiguity at all.
* **needs**: W4-C2, W4-C3
* **model**: sonnet
* **decided**:
  * **CBOR is chosen from content, as a strict format.** Its document is a
    map, whose first byte is `A0` to `BB` or `BF`, or the self-describe tag
    `D9 D9 F7`. `A0` to `BF` are UTF-8 continuation bytes and `D9` cannot be
    followed by `D9`, so no UTF-8 text - which every text format here
    reads - begins so; and the reading, not the look, decides (D-73), so a
    binary file that is not CBOR is still refused. Strict, it is taken at
    once when it reads.
  * `src/serialize/detect.c`: `look_orders` becomes `[5][7]`: each of the
    four rows gains a seventh entry `NULL`, and a fifth row
    `{"cbor", "json", "json5", "toml", "xml", "yaml", "ini"}` (the comment:
    `/* a CBOR map's head or the self-describe tag */`). `look_at` asks
    first, before the byte order mark and the blanks are skipped:
    `n >= 1` and `p[0]` in `0xA0..0xBB` or `== 0xBF` -> 4; `n >= 3` and
    `D9 D9 F7` -> 4. The loop over a row runs to 7;
    `cf_codec_by_name(NULL)` is NULL and `can_read(NULL)` false, so a NULL
    entry is passed over. `is_loose` is unchanged.
  * `docs/Formats.md`'s strategy 3 gains "a CBOR map's first byte or the
    self-describe tag CBOR first", and the table a row:
    `| CBOR | RFC 8949 | built in (\`CF_WITH_CBOR\`, D-91) | yes: indefinite-length maps and arrays, numbers in preferred serialization | RFC 8949 Appendix A: <n>/<m> read and the rest refused by name; Appendix F.1: 94/94 refused; <k>/<k> values | byte strings, tags other than 0, 1, 32 and 55799, undefined and the other simple values, a key that is not text, an integer past 64 bits - each refused by name; Infinity and NaN are their text |`
    with W4-C2's numbers.
  * `docs/Guide.md`, section 8's table, after the properties row:
    `| CBOR | \`CF_WITH_CBOR\` | RFC 8949: maps, arrays, numbers, text, booleans and null, typed by the file; each value placed by its byte; byte strings and most tags refused by name (D-91) |`;
    and the sentence "INI, JSON, TOML and YAML can also be written" becomes
    "INI, JSON, TOML, YAML and CBOR can also be written".
  * Fuzzing, in the shape the other readers have:
    * `tests/unit/test_fuzz.c`: a byte corpus,
      `static const unsigned char g_cbor[]`: one indefinite-length map
      (`BF ... FF`) whose members are those of W4-C1's `test_places`,
      `test_indefinite` (its first document), `test_tags` (the date) and
      `test_floats` (its first document), each member's bytes on a line of
      its own with a comment saying what it is, so that every branch of the
      reader has a seed to start from; a `load_bytes(blob* v, const unsigned char* p, size_t n)`
      beside `load()`; `fuzz_bytes("cbor", g_cbor, sizeof g_cbor)` written
      as `fuzz_text` is but loading with `load_bytes`; `feed_codec("cbor", &v)`
      in `fuzz_from_nothing` after properties; and its seed written by
      `write_seed` under `CF_FUZZ_SEEDS` as the others are (read that block
      for the directory name: `seed_cbor`).
    * `tests/fuzz/libfuzzer.c`: `#elif CF_FUZZ_TARGET == 12` /
      `feed_codec("cbor", &v);` before the `#else`.
    * `tests/fuzz/CMakeLists.txt`: `if(CF_WITH_CBOR) list(APPEND readers cbor) endif()`
      after the properties block, and `cbor` appended to `all_readers`
      (twelfth, after `json5`: the number is the place in that list).
  * The conformance floor gains `detect-cbor <n> <total>` with the run's
    numbers, under the detection comment; most vectors are lone values,
    which are no configuration, so the score is low by design, as JSON's is.
* **files**: `src/serialize/detect.c`, `tests/unit/test_detect.c`,
  `tests/unit/test_fuzz.c`, `tests/fuzz/libfuzzer.c`,
  `tests/fuzz/CMakeLists.txt`, `tests/conformance/floor.txt`; last:
  `docs/Formats.md`, `docs/Guide.md`, `docs/Conformance.md`,
  `docs/Decisions.md`, `CHANGELOG.md`, `docs/Roadmap.md`.
* **steps**:
  1. Tests in `tests/unit/test_detect.c`, under `#if CF_WITH_CBOR`, in its
     style: `cf_codec_detect(NULL, 0, &blob, NULL, NULL, &d)` on W4-C1's
     `test_places` bytes returns the `cbor` codec with `d.how == CF_DETECT_CONTENT`;
     on the same bytes behind `D9 D9 F7`, the same; and a file `cbor_doc`
     (no extension) written into the test's `cf_detect.d` directory is read
     by `cf_builder_dir` as CBOR (`server.port` 8080).
  2. See it fail: `ctest --preset default -R unit.test_detect` fails
     (nothing reads the bytes: "no format").
  3. `detect.c` as decided. Rerun step 2: passes.
  4. The fuzzing. `ctest --preset default -R unit.test_fuzz` passes; with
     clang, `cmake --preset fuzz && cmake --build --preset fuzz && ctest --preset fuzz -R fuzz.cbor`
     passes where the fuzz preset builds.
  5. Run the conformance step (`-DEXTRA=conformance`), write `detect-cbor`'s
     floor from what it prints.
  6. The documents. ACCEPT-LINUX, ACCEPT-WINDOWS.
* **accept**: ACCEPT-LINUX (with `-DEXTRA=conformance` once, for the
  floor), ACCEPT-WINDOWS, ACCEPT-CI (CI's fuzz job runs `fuzz.cbor`).
* **stop if**: a text document of the conformance suites is detected as
  CBOR (then the look is wrong: hand back with the file's name); or
  `detect-json`, `detect-toml` or `detect-yaml` falls below its floor.
* **docs**: D-91 gains "**From content.**": the first byte, why it cannot
  begin text, strict. Its "Held to" gains the detection score and the
  fuzzer. CHANGELOG, the CBOR entry gains "chosen from content when a name
  gives no format". Roadmap: wave 4's text "CBOR done (D-91)"; the row
  stays 🟨 until W4-T5.
* **notes**:

---

## W4-T1 - A codec that carries its own data

* **id**: W4-T1
* **goal**: a `cf_codec` can read through a decoder that is handed the codec
  itself and so data of its own (`decode_self`, `data`), every reading in
  the library goes through `cf_codec_decode`, and a codec can say it is
  never to be chosen from content (`CF_CODEC_NAMED_ONLY`).
* **why**: a TLV layout is data, and `decode(in, out, err)` has no argument
  to reach it by; the only way to give a layout to today's codec would be a
  global, which this library has none of. A codec made at run time from a
  description - a layout, or a binding's codec written in Rust or Python -
  needs this to exist at all.
* **needs**: nothing
* **model**: opus
* **decided**:
  * `cf_codec` gains two members at its end, after `encoder_state_size`:

    ```c
        /* (1.5, D-92) Reading for a codec made at run time from a
           description - a TLV layout (cf_tlv_codec) - which reaches it
           through `self`: taken in place of `decode` when `size` reaches it
           and it is not NULL. `data` is the description; the library never
           reads it, frees it or copies it. */
        cf_bool (*decode_self)(const struct cf_codec* self, const cf_blob* in,
                               const cf_doc_sink* out, cf_error* err);
        const void* data;
    ```

    `struct cf_codec` must be the struct's tag for that (it is
    `typedef struct cf_codec {` already).
  * And three declarations after `cf_codec_among`:

    ```c
    /* Reads `in` through `c`: its decode_self when its size reaches that
       member and it is set, else its decode. False with `err` set when it
       refuses the bytes, or has neither. (1.5) */
    CF_API cf_bool cf_codec_decode(const cf_codec* c, const cf_blob* in,
                                   const cf_doc_sink* out, cf_error* err);

    /* Whether `c` reads: CF_CODEC_READ and a decoder of either kind. (1.5) */
    CF_API cf_bool cf_codec_can_read(const cf_codec* c);
    ```

    and in the caps enum, `CF_CODEC_NAMED_ONLY = 1u << 6` with the comment
    "chosen by its name or its extensions, never from content: a layout
    reads bytes no other format would refuse, so a guess at them would be
    taken (D-73, D-92). (1.5)". Both written in `src/serialize/codec.c`;
    `cf_codec_decode` with no decoder sets `CF_E_CAPABILITY`,
    `"format '%s' can be written but not read"` (file.c's words), the
    codec's name as source.
  * **1.0's size is the floor, not the library's.** A codec compiled
    against 1.4 has the smaller size and must go on working (D-51). 1.0's
    size is `offsetof(cf_codec, decode_self)`; each file that needs it
    defines `#define CODEC_SIZE_1_0 offsetof(cf_codec, decode_self)` with
    the comment `/* where 1.0's cf_codec ended (D-51) */`, as define.c
    defines `DEF_SIZE_1_0` - `src/serialize/codec.c` and `src/core/config.c`,
    one line each, rather than a public name for an internal fact.
    * `cf_codec_can_read(c)` is
      `c != NULL && c->size >= CODEC_SIZE_1_0 && (c->caps & CF_CODEC_READ) != 0`
      and a decoder: `c->decode != NULL`, or `decode_self` reachable
      (`c->size >= offsetof(cf_codec, data) + sizeof(c->data)`) and set.
    * `cf_codec_decode` takes `decode_self` when it is reachable and set,
      else `decode`.
    * In `src/core/config.c`, `cf_builder_file_codec`'s refusal of
      `codec->size < sizeof(cf_codec)` becomes `< CODEC_SIZE_1_0`, its
      message unchanged; `cf_builder_codec`'s condition becomes: a size of
      at least `CODEC_SIZE_1_0`, a name, read or write, `CF_CODEC_READ` only
      when `cf_codec_can_read`, `CF_CODEC_WRITE` only with an encoder; its
      message unchanged.
  * Every place that reads through a codec it did not choose by name calls
    `cf_codec_decode`, and every place that asks whether one reads calls
    `cf_codec_can_read`: `src/bridge/file.c` (`cf_source_read_blob`'s
    `codec->decode(...)`, and the `codec->decode == NULL` test near line
    290), `src/serialize/detect.c` (`trial_read`'s call, and `can_read`,
    which becomes `cf_codec_can_read`), and `tests/conformance/conform.c`'s
    call (a `--codec` module may be a TLV one). The places that call the
    built-in JSON reader by name - `schema.c`, `report.c`, Doctor's
    `engine.c` and `spring.c`, the tests - keep `json->decode`: JSON has a
    `decode`, and changing them proves nothing.
  * `CF_CODEC_NAMED_ONLY` in `detect.c`: in the content phase, a codec of
    the builder's own with the flag is not put in `order` (the name phase is
    unchanged: its extensions still choose it). No built-in codec has it;
    `.properties` is kept out of content detection by not being listed,
    and stays so.
  * The test codec: `tests/unit/cf_kv_codec.h` gains a second codec,
    `cf_kv_self_codec`, whose `decode` is NULL and whose `decode_self` reads
    the same `key=value` lines as `kv_decode` but prefixes every key with
    the text `data` points to (`static const char kv_prefix[] = "self";`),
    `caps` `CF_CODEC_READ | CF_CODEC_NAMED_ONLY`, name `kvs`, extensions
    `.kvs`. Written by calling the shared line reader `kv_decode` already
    uses, so the two do not repeat each other (split `kv_decode`'s body
    into `kv_lines(in, out, err, prefix)` if it is not one already).
  * Rejected: a `void* ctx` argument added to `decode` (every codec in and
    out of the library breaks); a process-wide table of layouts (global
    mutable state, D-68's reason); a TLV source instead of a codec
    (`cf_builder_tlv`): it would read files only, and a layout would not
    reach a drop-in directory, a document in memory, `cf_emit` later, or the
    conformance run's `--codec`.
* **files**: `include/configforge/cf_serialize.h`, `src/serialize/codec.c`,
  `src/serialize/detect.c`, `src/bridge/file.c`, `src/core/config.c`,
  `tests/conformance/conform.c`, `tests/unit/cf_kv_codec.h`,
  `tests/unit/test_codec.c`; last: `cmake/configforge.symbols`,
  `docs/API.md` (regenerated), `docs/Decisions.md`, `CHANGELOG.md`,
  `docs/Roadmap.md`.
* **steps**:
  1. The two members, the declarations and the flag; `cf_codec_decode` and
     `cf_codec_can_read` written to read `decode` only (a placeholder: the
     new member is never taken). The symbol list. Build.
  2. Tests in `tests/unit/test_codec.c`, in its style:
     * `test_self_file`: `cf_builder_codec(b, &cf_kv_self_codec)`, a file
       `cf_self.kvs` holding `port=9443` (written by the test), read by
       `cf_builder_file(... NULL, 0)`: `self.port` is "9443".
     * `test_self_document`: `cf_builder_document(b, "doc", rank, "port=1\n", 7, "kvs", 0)`:
       `self.port` is "1".
     * `test_self_dir`: the test's `cf_codec.d` directory with `a.kvs`
       holding `x=1`, read by `cf_builder_dir`: `self.x` is "1".
     * `test_named_only`: an extensionless file `cf_self_plain` holding
       `port=9443`, read by `cf_builder_file(..., NULL, CF_OPTIONAL)` on a
       builder given `cf_kv_self_codec`: `self.port` is absent (INI, which
       takes `port=9443` as a root key, reads it instead; that `port` is
       "9443" does not matter here). The contrast, in the same test: a
       copy of the codec whose caps lack `CF_CODEC_NAMED_ONLY`, named
       `kvs2`, given to a second builder, is taken from the content (a
       builder's own codecs are tried first and are strict, D-73), and
       `self.port` is "9443".
     * `test_old_size`: a `cf_codec` copied from `cf_kv_codec` with `size`
       set to `offsetof(cf_codec, decode_self)` (1.0's size) is accepted by
       `cf_builder_codec` and reads, through `decode`.
     * `test_no_reader`: `cf_codec_decode` of a codec with `CF_CODEC_READ`
       and neither decoder returns false with "can be written but not read".
  3. See them fail: `ctest --preset default -R unit.test_codec` fails in
     `test_self_*` (`cf_builder_codec` refuses a codec with no `decode`, or
     the read finds no key).
  4. The real `cf_codec_decode` and `cf_codec_can_read`, the floor, the
     call sites, NAMED_ONLY in detect.c. Rerun step 3: passes.
  5. `cmake -P scripts/format.cmake`; ACCEPT-LINUX; ACCEPT-WINDOWS. A shared
     build is in ACCEPT-LINUX's presets only by `EXTRA`; run it too:
     `cmake -DSOURCE=index -DJOBS=3 -DWORK=$HOME/cfv -DEXTRA=shared -P scripts/verify.cmake`
     (the conformance module codecs, `conformance.codec_*`, are built there,
     with 1.5's size).
* **accept**: ACCEPT-LINUX (and `-DEXTRA=shared`), ACCEPT-WINDOWS,
  ACCEPT-CI. `gate.abi` passes with two names added.
* **stop if**: a gate or a binding reads `sizeof(cf_codec)` and assumes it
  fixed (grep `sizeof(cf_codec)` in `bindings/`, `cmake/`, `tests/`: a match
  outside a codec's own initialiser is a place this card did not foresee;
  hand back); or `test_old_size` cannot pass without reading past 1.0's
  members somewhere this card did not name.
* **docs**: add **D-92**, "A codec carries its own data", in D-80's shape:
  the question (a codec made at run time has nothing to read its
  description through); the options (a context argument on `decode`; a
  global table; a source of its own for TLV; two members and a function);
  the decision (the last: the members, `cf_codec_decode` as the one way to
  read, `cf_codec_can_read`, 1.0's size the floor as D-51 asks, NAMED_ONLY
  and why); compatibility (additive: a 1.4 codec is read as before, and a
  1.5 one passed to 1.4 is refused only if it has no `decode`). CHANGELOG,
  Added: "A codec can carry data of its own (`decode_self`, `data`,
  `cf_codec_decode`), and say it is never chosen from content
  (`CF_CODEC_NAMED_ONLY`): what a codec made from a description needs
  (D-92)." Roadmap: wave 4's text gains "a codec carries its own data (D-92)".
* **notes**:
  * 2026-11-13: built as decided, with three small changes. cf_kv_codec.h's
    format is `key: value`, not `key=value`, so the tests' documents are
    `port: 9443` and the like; the contrast in `test_named_only` is then
    read by YAML, not INI, which changes nothing it shows. An undefined key
    is not read back by `cf_get_text`, so the tests define the key they
    read as text. Every `cf_codec` initialiser in the library and the tests
    gained `NULL, NULL` for the two members: `-Wextra`'s
    missing-field-initializers refuses a partial one.

---

## W4-T2 - A TLV layout, and reading a flat one

* **id**: W4-T2
* **goal**: a program describes a fixed-width TLV layout in a
  `cf_tlv_layout`, `cf_tlv_codec` checks it and makes a codec of it, and
  that codec reads a flat document - text, integers, booleans, reals and
  fixed records - with its end tag, every value placed by the byte its tag
  starts at.
* **why**: a device's `.cfg` of two-byte tags and lengths is configuration
  that today reaches a program through code nobody else can read; read
  through a layout, each value says which tag at which byte gave it, and
  the layout - one table - is the whole of what the program had to write.
* **needs**: W4-T1
* **model**: opus
* **decided**:
  * The interface, in `include/configforge/cf_serialize.h`, a section after
    the codecs' (before "The format from the name"):

    ```c
    /* ------------------------------------------------------------------ *
     * Tag, length, value, by a layout the program gives (D-93)
     *
     * TLV has no one standard, so it is a codec the program makes: the
     * layout says how wide a tag and a length are and in which byte
     * order, how the document ends, and what each tag is - its key, its
     * type, and for a nested one the tags inside it. Each value is placed
     * in explain() as "<file>, byte <n>", n the offset of its tag. A tag
     * the layout does not name is refused unless the layout says to skip
     * or keep it. Read only: writing back keeps what a layout skips, and
     * is wave 6's.
     * ------------------------------------------------------------------ */

    enum { CF_TLV_FIXED = 0, CF_TLV_BER = 1 };          /* the layout's kind */
    enum { CF_TLV_BIG = 1, CF_TLV_LITTLE = 2 };         /* byte order; 0 in a
                                                           tag is the layout's */
    enum {
        CF_TLV_TEXT   = 1, /* UTF-8; trailing NUL bytes dropped              */
        CF_TLV_INT    = 2, /* signed, two's complement, `width` bytes        */
        CF_TLV_UINT   = 3, /* unsigned, `width` bytes                        */
        CF_TLV_BOOL   = 4, /* one byte, 0 or 1                               */
        CF_TLV_REAL   = 5, /* IEEE 754, 4 or 8 bytes                         */
        CF_TLV_BCD    = 6, /* packed decimal digits, high nibble first      */
        CF_TLV_HEX    = 7, /* any bytes, as lowercase hexadecimal text       */
        CF_TLV_RECORD = 8, /* a fixed record: skipped or refused             */
        CF_TLV_NEST   = 9  /* more TLV inside: a table                       */
    };
    enum { CF_TLV_SKIP = 1u << 0, CF_TLV_LIST = 1u << 1 };   /* a tag's flags */
    enum { CF_TLV_END = 1u << 0, CF_TLV_END_LENGTH = 1u << 1,
           CF_TLV_PADDING = 1u << 2 };                       /* a layout's    */
    enum { CF_TLV_UNKNOWN_REFUSE = 0, CF_TLV_UNKNOWN_SKIP = 1,
           CF_TLV_UNKNOWN_KEEP = 2 };

    /* One tag of a layout. Fixed for 1.x, as cf_pair is: it is an array's
       element, and an array cannot grow. `more` is NULL; a later 1.x may
       point it at a sized struct of more, and an earlier library refuses a
       tag whose `more` is set rather than ignore it (D-51). */
    typedef struct cf_tlv_tag {
        cf_u32                   tag;    /* CF_TLV_FIXED: its value read in the
                                            layout's order; CF_TLV_BER: its
                                            identifier bytes as one big-endian
                                            number, as EMV writes it: 0x9F02 */
        const char*              key;    /* "port", or dotted, "net.port";
                                            NULL for a skipped or refused one */
        int                      type;   /* CF_TLV_TEXT ...                   */
        unsigned                 width;  /* bytes: 0 any (TEXT, HEX, BCD, RECORD;
                                            INT and UINT: 1 to 8, the value's
                                            own); else exactly this many      */
        int                      order;  /* INT, UINT, REAL: 0 the layout's   */
        unsigned                 flags;  /* CF_TLV_SKIP, CF_TLV_LIST          */
        const struct cf_tlv_tag* inner;  /* CF_TLV_NEST: the tags inside      */
        size_t                   ninner;
        const void*              more;   /* NULL                              */
    } cf_tlv_tag;

    /* A layout; start from CF_TLV_LAYOUT_INIT. Each table of tags is in
       ascending order of `tag`, which is how it is searched and how a tag
       listed twice is found. */
    typedef struct cf_tlv_layout {
        size_t            size;          /* sizeof(cf_tlv_layout)          */
        const char*       name;          /* the format's name: "devcfg"    */
        const char*       extensions;    /* ".dcfg", or NULL: by name only */
        int               kind;          /* CF_TLV_FIXED or CF_TLV_BER     */
        unsigned          tag_width;     /* CF_TLV_FIXED: 1, 2 or 4        */
        unsigned          length_width;  /* CF_TLV_FIXED: 1, 2 or 4        */
        int               order;         /* CF_TLV_FIXED: BIG or LITTLE    */
        unsigned          flags;         /* CF_TLV_END, _END_LENGTH, _PADDING */
        cf_u32            end_tag;       /* with CF_TLV_END                */
        int               unknown;       /* CF_TLV_UNKNOWN_REFUSE ...      */
        const cf_tlv_tag* tags;          /* the top level's                */
        size_t            ntags;
    } cf_tlv_layout;

    #define CF_TLV_LAYOUT_INIT                                                      \
        {sizeof(cf_tlv_layout), NULL, NULL, CF_TLV_FIXED, 0, 0, 0, 0, 0,             \
         CF_TLV_UNKNOWN_REFUSE, NULL, 0}

    /* A codec that reads `layout`, made in `out`: the program owns `out`,
       the layout, and every table and key it points to, and each must
       outlive every configuration that reads with the codec - as any codec
       given to cf_builder_codec must; a `static` one is the usual way.
       The layout is checked whole here, and false with `err` set
       (CF_E_DEFINITION, the layout's name as source) names the first
       mistake; `out` then reads nothing. The codec is named the layout's
       name, takes its extensions, reads only, and is never chosen from
       content (CF_CODEC_NAMED_ONLY). Off in a build without CF_WITH_TLV,
       and says so. (1.5) */
    CF_API cf_bool cf_tlv_codec_sized(cf_codec* out, size_t out_size,
                                      const cf_tlv_layout* layout, cf_error* err);
    #define cf_tlv_codec(out, layout, err)                                          \
        cf_tlv_codec_sized((out), sizeof(cf_codec), (layout), (err))
    ```

    The size travels with the call, as `cf_def_init` carries its own
    (D-51): a `cf_codec` smaller than `offsetof(cf_codec, data) + sizeof(const void*)`
    is refused, `"this cf_codec is %zu bytes, from before 1.5, and has no room for a layout"`.
  * Who owns what: the program, everything (above). The library keeps the
    layout's pointer in `out->data` and copies nothing, so a layout made on
    the stack and gone before the configuration is read is the program's
    mistake, as a codec on the stack is today. Rejected: copying the layout
    into the builder's arena (a layout is not the builder's: one codec may
    serve many builders, and `cf_builder_codec` takes no copy of anything
    else either).
  * The option, wired as W4-C1's is: `option(CF_WITH_TLV "TLV codecs made from a layout (built in, no dependency)" ON)`
    after `CF_WITH_CBOR`; `TLV` in `CF_FEATURES`; both copies of
    `cf_build_options.h`; `"tlv"` in `version.c`. `src/serialize/tlv.c`,
    C89, its statics `tlv_`; added to `src/serialize/CMakeLists.txt`
    unconditionally (like `codec.c`: the function exists in every build)
    and to `amalgamate.py` after `serialize/cbor.c`. Without the option,
    `cf_tlv_codec_sized` sets `CF_E_CAPABILITY`
    `"'%s' is a TLV layout, and this build was compiled without CF_WITH_TLV"`
    with the layout's name (or `"a TLV layout, and this build ..."` when it
    has none) and returns false (D-38).
  * What `cf_tlv_codec_sized` checks, in this order, each `CF_E_DEFINITION`,
    source the layout's name, the message beginning `"layout '%s': "`:
    * `layout == NULL || layout->size < offsetof(cf_tlv_layout, ntags) + sizeof(size_t)`:
      `"no layout, or one without its size (CF_TLV_LAYOUT_INIT)"`
    * a name `cf_key_valid` would take as one segment (lowercase letters,
      digits, `_` and `-`; it is a format's name, given to cf_builder_file):
      `"'%s' is not a format's name: lowercase letters, digits, '_' and '-'"`
    * kind 0 or 1: `"kind %d is neither CF_TLV_FIXED nor CF_TLV_BER"`
    * FIXED: `tag_width` and `length_width` each 1, 2 or 4, `order` BIG or
      LITTLE: `"a tag is 1, 2 or 4 bytes wide, not %u"`,
      `"a length is 1, 2 or 4 bytes wide, not %u"`,
      `"say the byte order: CF_TLV_BIG or CF_TLV_LITTLE"`
    * flags only of the three: `"flags %u that this library does not know"`;
      `CF_TLV_END_LENGTH` only with `CF_TLV_END`; `CF_TLV_PADDING` only
      with BER; the end tag fits the tag width (`end_tag >> (8 * tag_width) == 0`
      for widths under 4): `"the end tag 0x%s is wider than a tag"`
    * `unknown` 0, 1 or 2: `"unknown is %d, not a CF_TLV_UNKNOWN_ value"`
    * every table, the top one and each `inner`, recursively to
      `CF_MAX_DEPTH` (past it: `"tables nest deeper than %d levels"`):
      ascending and no tag twice: `"0x%s is listed after 0x%s: list a table's tags in ascending order, each once"`;
      each tag fits the width (FIXED), and is not the end tag:
      `"tag 0x%s is the end tag"`; `more` NULL:
      `"tag 0x%s has `more` set, which this library does not know"`;
      flags only SKIP and LIST; a type of the nine; a key that
      `cf_key_valid` takes, required unless the tag is a RECORD or has
      `CF_TLV_SKIP`: `"tag 0x%s: '%s' is not a valid key"` /
      `"tag 0x%s has no key"`; widths: INT and UINT 0, 1, 2, 4 or 8, BOOL
      0 or 1, REAL 4 or 8, TEXT/HEX/BCD/RECORD anything, NEST 0:
      `"tag 0x%s: a %s of %u bytes"` (the type's word: `text`, `int`,
      `uint`, `bool`, `real`, `bcd`, `hex`, `record`, `nest`); `order` 0, BIG
      or LITTLE; NEST with `inner` and `ninner` (or both empty, a nest of
      nothing known); not NEST with neither: `"tag 0x%s: only a nest has tags inside"`.
      Under `!CF_WITH_REAL`, a REAL tag: `"tag 0x%s is a real, and this build has no floating point support"`.
    In this card, `kind == CF_TLV_BER` is refused after the checks above:
    `"BER-TLV is not read yet"` (W4-T4 removes it); a NEST tag, a LIST
    flag, a BCD or HEX type, and an `unknown` other than REFUSE are refused
    the same way, `"... is not read yet"`, until W4-T3.
  * A tag shown in a message: `0x` and its value in uppercase hex, two
    digits for each byte of the tag's width (BER: of its identifier's
    length), by a helper `tlv_hex(char* out, cf_u32 v, unsigned width)`
    writing at most 8 digits and a NUL into `char[9]` - cf_fmt's `%x` takes
    an `unsigned int`, which may be 16 bits, so the digits are made here.
  * Reading a FIXED document (`tlv_decode`, the codec's `decode_self`, its
    layout `self->data`):
    * an element is the tag (`tag_width` bytes), the length (`length_width`
      bytes), each in the layout's order, then that many bytes of value;
    * the tag is looked up in the level's table by binary search;
    * the end: with `CF_TLV_END`, an element whose tag is `end_tag` ends
      the top level - it has no length unless `CF_TLV_END_LENGTH`, and then
      its length must be 0. Nothing may follow it. Without `CF_TLV_END` the
      document ends with its last byte, which must end an element. The end
      tag is recognised at the top level only; inside a nest its length
      bounds it.
    * the root is `begin_table(NULL, 0)`; each value a `scalar` named by
      its key (a dotted key is passed whole, as JSON passes `"a.b"`: the
      flattener makes the tables), `textual` false except as said below;
      `end` at the end;
    * TEXT: the bytes, trailing NULs dropped (a C string in a fixed field),
      then checked with `cf_text_is_utf8`; a NUL left inside is refused, a
      configuration's text has none that matter and a device's padding is
      at the end. Passed as a pointer into the blob: no copy.
    * INT, UINT: width 0 is the value's own length, 1 to 8 bytes; a length
      other than the entry's width is refused. Sign-extended (INT) from the
      top bit; UINT above `CF_I64_MAX` refused. Built in a `cf_u64` byte by
      byte in the entry's order (0: the layout's), so no host order matters.
    * BOOL: one byte, 0 false, 1 true, anything else refused.
    * REAL: 4 bytes are single precision and 8 double, in the entry's order,
      converted by W4-C1's `cf_f64_from_f32_bits` and `cf_f64_from_bits`.
      Non-finite: `inf`, `-inf`, `nan` with `textual` true, as CBOR's.
    * RECORD: with `CF_TLV_SKIP` passed over; else refused. Any type with
      `CF_TLV_SKIP` is passed over: the layout knows the tag and chose not
      to read it.
  * Places: every scalar's `cf_loc` is `{ where, 0, 0 }`, `where`
    `"%s, byte %zu"` with the blob's name (`"<tlv>"` with none) and the
    offset of the element's first byte, its tag - in a hex dump, the tag is
    what a person looks for. `char where[CF_PATH_MAX + 32]` in the scanner,
    as cbor.c's.
  * Limits, and why: a length is checked against the bytes left in what
    encloses it (the document, or the nest) before the value is read, so a
    length of four gigabytes costs nothing; nesting to `CF_MAX_DEPTH`
    levels, the document the first (W4-T3), as every reader; no limit on
    the number of tags in a table, which is searched in `log n`; the widths
    above (a length field of 4 bytes holds every document a `size_t` of 32
    bits can).
  * Read errors, each `CF_E_SOURCE`, `"%s, byte %zu: ..."` with the blob's
    name and the element's offset:
    * `"%s: the document is empty"`
    * `"... a tag cut short: %zu bytes are left"`, `"... a length cut short: %zu bytes are left"`
    * `"... tag 0x%s says %U bytes, and %zu are left"`
    * `"... tag 0x%s is not in the layout '%s'"` (REFUSE)
    * `"... tag 0x%s is %U bytes, and its entry says %u"` (a fixed width)
    * `"... tag 0x%s is text that is not UTF-8"`, `"... tag 0x%s is text with a NUL inside"`
    * `"... tag 0x%s is %U, past the largest integer a configuration holds"`
    * `"... tag 0x%s holds %u, and a bool is 0 or 1"`
    * `"... tag 0x%s is a record of %U bytes, which the layout does not read"`
    * `"... the end tag has a length of %U; it must be 0"`
    * `"... %zu bytes after the end tag"`
    * `"%s: the document ends at byte %zu without the end tag 0x%s"`
    * a refused name: `CF_E_KEY` `"... '%s' is not a valid key"`; the
      root refused: `"%s: sink refused"`; the end refused:
      `"%s: the document could not be recorded"` (ini.c's)
  * The layout the tests cover, written in `tests/unit/test_tlv.c` as a
    `static const cf_tlv_tag dev_tags[]` and a layout `dev` named `"devcfg"`
    (no extensions): FIXED, tag and length 2 bytes, LITTLE, `CF_TLV_END`
    with `end_tag` `0xFFFF`. Its tags, ascending:
    `{0x0001, "name", CF_TLV_TEXT, 0, 0, 0, NULL, 0, NULL}`,
    `{0x0002, "net.port", CF_TLV_UINT, 2, 0, 0, NULL, 0, NULL}`,
    `{0x0003, "net.dhcp", CF_TLV_BOOL, 1, 0, 0, NULL, 0, NULL}`,
    `{0x0004, "offset", CF_TLV_INT, 4, CF_TLV_BIG, 0, NULL, 0, NULL}` (its
    own order, to test the override),
    `{0x0005, "gain", CF_TLV_REAL, 4, 0, 0, NULL, 0, NULL}` (in builds with
    `CF_WITH_REAL`; the table without it leaves the entry out),
    `{0x0010, NULL, CF_TLV_RECORD, 12, 0, CF_TLV_SKIP, NULL, 0, NULL}`,
    `{0x0011, NULL, CF_TLV_RECORD, 8, 0, 0, NULL, 0, NULL}`.
    The document is synthesized by a helper
    `put(unsigned char* doc, size_t* n, cf_u32 tag, const void* value, size_t len)`
    that writes the tag and the length little-endian and copies the value;
    no device's bytes are in the repository. The document: `0x0001` "unit-7"
    padded with two NULs (8 bytes), `0x0002` `90 01` (400), `0x0003` `01`,
    `0x0004` `FF FF FF FE` (-2, big-endian), `0x0005` the four bytes of 1.5
    as a little-endian single (`00 00 C0 3F`), `0x0010` twelve bytes of
    anything, then `FF FF`.
* **files**: `include/configforge/cf_serialize.h`,
  `cmake/ConfigForgeOptions.cmake`, `include/configforge/cf_build_options.h`,
  `cmake/templates/cf_build_options.h.in`, `src/core/version.c`,
  `src/serialize/tlv.c` (new), `src/serialize/CMakeLists.txt`,
  `scripts/generate/amalgamate.py`, `tests/unit/test_tlv.c` (new), `tests/unit/test_features.c`; last:
  `tests/unit/CMakeLists.txt`, `cmake/configforge.symbols`, `docs/API.md`
  (regenerated), `docs/Decisions.md`, `CHANGELOG.md`, `docs/Roadmap.md`.
* **steps**:
  1. The option (and its `test_features.c` line, as W4-C1's), the header
     section, and `cf_tlv_codec_sized` that checks only the size and fills
     the codec, with a `tlv_decode` that sets `"%s: not read yet"` and
     returns false. The symbol `cf_tlv_codec_sized`. Build.
  2. `tests/unit/test_tlv.c` (after `test_cbor` in the list), `main`
     running everything under `#if CF_WITH_TLV`, `test_off()` otherwise:
     * `test_flat`: `static cf_codec devcfg;` made with `cf_tlv_codec(&devcfg, &dev, &err)`;
       `cf_builder_codec(b, &devcfg)`; the document through
       `cf_builder_document(b, "dev.cfg", CF_RANK_USER_FILE, doc, n, "devcfg", 0)`.
       `name` "unit-7", `net.port` 400, `net.dhcp` true, `offset` -2,
       `gain` 1.5 (under REAL); nothing from `0x0010`; explain of
       `net.port` contains `dev.cfg, byte 12` (the second element: the
       first is 2 + 2 + 8 bytes).
     * `test_file`: the same bytes written to `cf_tlv_test.bin` and read by
       `cf_builder_file(b, "file", rank, "cf_tlv_test.bin", "devcfg", 0)`.
     * `test_read_refusals`, each a document built with `put`, refused with
       these words: the record `0x0011` -> "a record of 8 bytes"; tag
       `0x0099` -> "tag 0x0099 is not in the layout 'devcfg'"; `0x0002`
       with a 3-byte value -> "is 3 bytes, and its entry says 2"; a length
       of 300 with 4 bytes left -> "says 300 bytes, and 4 are left"; `0x0003`
       holding `02` -> "a bool is 0 or 1"; text `C3 28` -> "not UTF-8";
       text `61 00 62` -> "a NUL inside"; no `FF FF` -> "without the end
       tag 0xFFFF"; a byte after `FF FF` -> "1 bytes after the end tag"; an
       empty document -> "the document is empty"; `0x01` alone -> "a tag
       cut short".
     * `test_layout_refusals`, `cf_tlv_codec` false with these words, each
       from `dev` with one field changed in a copy: `tag_width` 3 -> "1, 2
       or 4 bytes wide, not 3"; tags `0x0002` before `0x0001` -> "is listed
       after"; `0x0002` twice -> "is listed after"; a key `"Net.Port"` ->
       "is not a valid key"; an INT of width 3 -> "an int of 3 bytes"; `more`
       set -> "has `more` set"; `size` 0 -> "CF_TLV_LAYOUT_INIT"; a
       `cf_codec` size of `offsetof(cf_codec, decode_self)` -> "from before 1.5".
     * `test_off`: `cf_tlv_codec` false, message has "CF_WITH_TLV".
  3. See it fail: `ctest --preset default -R unit.test_tlv` fails on
     "not read yet" and on the layout refusals.
  4. The checks and the reading. Rerun step 3: passes.
  5. `cmake -S . -B build-notlv -DCF_WITH_TLV=OFF && cmake --build build-notlv && ctest --test-dir build-notlv -R "unit.test_(tlv|features)"`
     passes. Remove `build-notlv`.
  6. Write the decision. `cmake -P scripts/format.cmake`; ACCEPT-LINUX;
     ACCEPT-WINDOWS.
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI. `gate.abi`,
  `gate.amalgamation`, `gate.formats` pass; the minimal preset (no
  `CF_WITH_REAL`) runs `test_flat` without `gain`.
* **stop if**: the flattener refuses a dotted name in a scalar (`"net.port"`:
  check `fl_enter_member` in `src/bridge/flatten.c` with `cf_key_valid` -
  if a dotted name is refused, keys must be one segment and the layout's
  design changes; hand back); or `gate.headers` refuses the new section in
  a C++ or a pedantic C89 compile (the enums and the macro must compile
  everywhere the header does).
* **docs**: add **D-93**, "TLV, a codec from a layout", in D-80's shape: the
  question (TLV has a standard only as BER; a device's file is a layout);
  the options (a codec per device, written by each program; one TLV codec
  for BER only; a layout the program gives and a codec the library makes);
  the decision (the last; the layout and the tag struct, sized and fixed as
  said and why; ownership; ascending tables; the types; the end tag and its
  length; what is not read yet, and wave 6 for writing); compatibility
  (additive: one function, one option on by default). CHANGELOG, Added:
  "**TLV by a layout** (`cf_tlv_layout`, `cf_tlv_codec`): a program describes
  its tags - widths, byte order, an end tag, each tag's key and type - and
  the library makes a codec that reads it, each value placed by the byte
  its tag starts at (D-93). `CF_WITH_TLV`, on by default." Roadmap: wave 4's
  text gains "a TLV codec from a layout, flat (W4-T2)".
* **notes**:
  * 2026-11-13: the file is `src/bridge/tlv.c`, not `src/serialize/tlv.c`:
    the layering gate keeps `cf_keyspace.h`, and so `cf_key_valid`, out of
    the serialization layer, and the bridge may use both (D-93 says so).
    `amalgamate.py` lists it after `bridge/schema.c`. The checks the card
    gives no words for have these: `"tag 0x%s is wider than a tag"`,
    `"CF_TLV_END_LENGTH is for an end tag: set CF_TLV_END"`,
    `"CF_TLV_PADDING is BER-TLV's"`, `"tag 0x%s has type %d, not a CF_TLV_
    type"`, `"... is %U bytes, and an integer is 1 to 8"` and `"... is %U
    bytes, and a bool is one"`. The type's word carries its article, so
    the width message reads "an int of 3 bytes", as the test asks.

---

## W4-T3 - Nesting, lists and unknown tags

* **id**: W4-T3
* **goal**: a layout's NEST tags read as tables, its LIST tags as arrays,
  BCD and HEX values as numbers and text, and an unknown tag as the layout
  says: refused, skipped, or kept under a key made from the tag.
* **why**: a device's file groups settings (a network block, a list of
  servers) and carries tags a newer firmware added; a reader that cannot
  nest flattens what the device grouped, and one that drops what it does not
  know hides exactly the value that changed.
* **needs**: W4-T2
* **model**: sonnet
* **decided**:
  * NEST: the value's bytes are a TLV sequence of the same kind, read with
    the entry's `inner` table; `begin_table(key)`, its members, `end`. The
    end tag is not looked for inside. The document is level 1; each nest one
    more; past `CF_MAX_DEPTH`, `"... nests deeper than %d levels"`. A nest
    with no tags (`inner` NULL) reads every member as unknown, by the
    layout's `unknown`.
  * LIST (a tag's flag, with any type): its occurrences are an array named
    by its key - `begin_array(key)` at the first, an unnamed scalar or table
    for each, `end` at the first element of another tag. Its occurrences
    must be consecutive: a later run is refused,
    `"... tag 0x%s continues a list that ended at byte %zu"` - as INI's
    `name[]` lines are (D-60), since a second run would restart the count.
    Without LIST, a tag repeated is two values for one key: the later wins,
    and both are the key's candidates, as two lines of one INI file are
    (W4-C1 decided the same for CBOR's duplicate keys).
  * BCD: two digits a byte, the high nibble first; a nibble above 9 refused
    (`"... tag 0x%s holds a nibble that is not a digit"`); at most 18
    digits, 9 bytes (`"... tag 0x%s has %U digits, more than an integer holds"`);
    an int. Leading zeros are digits like any other (EMV's amounts are
    BCD, `00 00 00 01 23 45` is 12345).
  * HEX: the bytes as lowercase hexadecimal text, two characters a byte, no
    separators; typed text. Written into the sink's `reserve_text` (it lends
    one buffer, reused value to value); without it, values up to 127 bytes
    use a 256-byte local buffer and longer ones are refused,
    `"... tag 0x%s needs sink text storage"`, and `CF_E_MEMORY`
    `"no room to write a TLV value as text"` when it returns NULL.
  * Unknown tags, by `layout->unknown`:
    * REFUSE (the default, 0): refused as W4-T2 refuses them. A value nobody
      mapped is a value nobody can explain, and a layout that is out of date
      should say so the first time the device's file outgrows it.
    * SKIP: passed over. The layout's author chose this; nothing is hidden
      that the layout did not say it would not read.
    * KEEP: kept under the key `tag-` and the tag's hex digits in lowercase
      (`tag-0099`, BER `tag-9f02`), its value as HEX. In W4-T4, a BER
      constructed tag kept is a table of that name and its members are
      unknown too, read by the same policy. Rejected: a key under a fixed
      prefix such as `unknown.` (it would move when the layout later names
      the tag, and `tag-0099` already says what it is).
  * The "not read yet" refusals W4-T2 left for these are removed; BER's
    stays until W4-T4.
  * Tests, added to `tests/unit/test_tlv.c`, with a second layout `net`
    (FIXED, 1-byte tags and lengths, BIG, no end tag): its top table
    `{0x01, "host", TEXT}`, `{0x02, "servers", NEST, inner servers_tags, CF_TLV_LIST}`,
    `{0x03, "amount", BCD}`, `{0x04, "mac", HEX, width 6}`; `servers_tags`
    is `{0x01, "addr", TEXT}`, `{0x02, "port", UINT, width 2}`. The document
    `01 03 "web"`, then two `02` elements each holding `01 len addr 02 02 port`,
    then `03 06 00 00 00 01 23 45`, then `04 06 00 1A 2B 3C 4D 5E`. Expected:
    `host` "web", `servers.count` 2, `servers.1.port` the second port,
    `amount` 12345, `mac` "001a2b3c4d5e", and `servers.0.addr` explained at
    the byte of its own tag inside the nest (an offset in the document, not
    in the nest). Then: the two `02` elements with an `03` between ->
    "continues a list that ended at byte"; `03` holding `0A` -> "a nibble
    that is not a digit"; tag `0x09` with `unknown` REFUSE -> refused, SKIP
    -> absent, KEEP -> `tag-09` holds its bytes in hex; 33 nested nests (a
    nest whose `inner` is its own table, built in a buffer) -> "nests deeper than".
* **files**: `src/serialize/tlv.c`, `tests/unit/test_tlv.c`; last:
  `docs/Decisions.md`, `CHANGELOG.md`, `docs/Roadmap.md`.
* **steps**:
  1. The tests above. See them fail: `ctest --preset default -R unit.test_tlv`
     fails on "is not read yet".
  2. NEST and LIST; then BCD and HEX; then the unknown policies; rerun step 1
     after each, until it passes.
  3. ACCEPT-LINUX, ACCEPT-WINDOWS.
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI. The stack table
  (`configforge-stack.txt`, written by the build) shows `tlv.c`'s recursive
  function under the minimal preset's 1024 bytes.
* **stop if**: a nest's recursion needs a buffer on its frame to keep its
  key (it must not: the key is the layout's, a pointer that outlives the
  decode); or the flattener cannot take a LIST's unnamed table elements
  (JSON's arrays of objects go through it, so it must).
* **docs**: D-93 gains the nest, the list and its run rule, BCD, HEX, and
  the three policies for an unknown tag with the reason for the default.
  CHANGELOG: the TLV entry gains "nested tags as tables, repeated ones as
  lists, BCD and hex values, and an unknown tag refused, skipped or kept as
  `tag-<hex>`, as the layout says". Roadmap: wave 4's text "a TLV codec from
  a layout, nested (W4-T3)".
* **notes**:

---

## W4-T4 - BER-TLV

* **id**: W4-T4
* **goal**: a layout of kind `CF_TLV_BER` reads ASN.1 BER's tag-length-value
  as ISO/IEC 7816-4 and EMV use it: multi-byte tags with their class and
  constructed bit, short and long-form lengths, and the padding between
  elements.
* **why**: a smart card's or a payment terminal's data is BER-TLV, the one
  TLV with a standard; read by a layout, `9F02` explains itself as the
  amount at its byte, where code that walks the bytes by hand explains
  nothing.
* **needs**: W4-T3
* **model**: sonnet
* **decided**:
  * A BER tag (X.690 section 8.1.2): the first byte's top two bits are the
    class, bit 6 says constructed, the low five bits the number - `1F`
    there means the number follows, in bytes whose top bit says another
    follows. A tag is at most 4 bytes; a longer one is refused,
    `"... a tag longer than 4 bytes"`. Its value in the layout's table is
    its identifier bytes as one big-endian number (`9F02`, `5A`, `BF0C`),
    the way EMV writes it, so class and constructed bit are part of it and
    need no field of their own. A tag shown in a message has two digits
    for each of its bytes.
  * A BER length (X.690 section 8.1.3): below `80` the short form; `81`
    to `84` that many bytes follow, big-endian; `80` (indefinite) refused,
    `"... an indefinite length, which ISO/IEC 7816-4 and EMV do not use"`;
    `85` and above refused, `"... a length of %u bytes"`. A long form longer
    than it needs is read (BER allows it; DER would not).
  * Constructed: a constructed tag's value is TLV. Its entry must be a NEST
    (`"... tag 0x%s is constructed and its entry is not a nest"`), and a
    NEST entry's tag must be constructed (checked by `cf_tlv_codec`:
    `"tag 0x%s is a nest and its constructed bit is not set"`). An unknown
    constructed tag kept (KEEP) is a table of unknowns, as W4-T3 said.
  * Padding: with `CF_TLV_PADDING`, a `00` or `FF` byte where a tag would
    begin is passed over (ISO/IEC 7816-4 section 5.2.2.1 allows them before
    and between elements). Without it, `00` begins a tag like any byte.
  * An end tag in a BER layout is allowed (`CF_TLV_END`) and works as it
    does for FIXED; `tag_width`, `length_width` and `order` are not read
    (0 is fine; anything else is not refused, the kind decides).
  * `cf_tlv_codec`'s checks for BER: each tag is a well-formed identifier
    (its continuation bits agree with its length: `"tag 0x%s is not one BER identifier"`).
  * Tests, in `tests/unit/test_tlv.c`, a layout `emv`: BER, `CF_TLV_PADDING`,
    unknown REFUSE; top table `{0x70, "record", NEST, inner emv_record}`;
    `emv_record` ascending: `{0x5A, "pan", BCD}`, `{0x5F24, "expiry", BCD, width 3}`,
    `{0x9F02, "amount", BCD, width 6}`, `{0xBF0C, "issuer", NEST, inner emv_issuer}`;
    `emv_issuer`: `{0x50, "label", TEXT}`. The document, synthesized:
    `00 00` (padding), `70 81 <n>` (a long-form length on purpose), then
    `5A 08 47 61 73 90 01 01 00 10`, `5F 24 03 25 12 31`,
    `9F 02 06 00 00 00 01 23 45`, `BF 0C <n> 50 04 "TEST"`, then `FF`
    padding. Expected: `record.pan` 4761739001010010, `record.expiry`
    251231, `record.amount` 12345, `record.issuer.label` "TEST", and
    `record.amount` explained at the byte of its `9F`. Refusals: `9F 82 80 80 80 01`
    -> "a tag longer than 4 bytes"; `70 80 ...` -> "an indefinite length";
    `5A 85 ...` -> "a length of 5 bytes"; `70` given as a primitive entry in
    a copy of the layout -> refused by `cf_tlv_codec` ("constructed bit");
    the padding with `CF_TLV_PADDING` cleared -> "tag 0x00 is not in the layout".
    The numbers are made up for the test, a PAN no card has.
* **files**: `src/serialize/tlv.c`, `tests/unit/test_tlv.c`; last:
  `docs/Decisions.md`, `CHANGELOG.md`, `docs/Roadmap.md`.
* **steps**:
  1. The tests. See them fail: `ctest --preset default -R unit.test_tlv`
     fails on "BER-TLV is not read yet".
  2. The tag and length readers for BER, the constructed rule, the padding,
     the layout checks; rerun until it passes.
  3. ACCEPT-LINUX, ACCEPT-WINDOWS.
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI.
* **stop if**: a tag's value in the table cannot be both its bytes and its
  lookup key (two different identifiers with one number - impossible if the
  bytes are taken whole; if a test finds one, hand back).
* **docs**: D-93 gains BER: tags as their identifier bytes and why, lengths,
  indefinite refused and why, constructed and nest, the padding. CHANGELOG:
  the TLV entry gains "and BER-TLV (ISO/IEC 7816-4, EMV): multi-byte tags,
  long-form lengths, padding". Roadmap: wave 4's text "TLV and BER-TLV by
  a layout (W4-T4)".
* **notes**:

---

## W4-T5 - TLV fuzzed and documented

* **id**: W4-T5
* **goal**: the TLV reader is fuzzed under a fixed layout as the other
  readers are, and the Guide and `docs/Formats.md` show how a layout is
  written and what it reads.
* **why**: a reader of a device's bytes reads bytes nobody checked; the
  fuzzer is the check, and a layout nobody can write from the documents is
  one only this project can use.
* **needs**: W4-T4, W4-C4
* **model**: sonnet
* **decided**:
  * Fuzzing: `tests/unit/test_fuzz.c` gains a TLV layout of its own,
    `fz_tlv`, FIXED, 2-byte little-endian tags and lengths, `CF_TLV_END`
    `0xFFFF`, `unknown` KEEP (so unknown tags reach the hex path), with one
    tag of each type including a NEST with a LIST inside; and a second,
    `fz_ber`, BER with padding, a NEST and a BCD. A `feed_tlv(const cf_codec* c, blob* v)`
    reads through a codec made once with `cf_tlv_codec` (static), the
    reading `feed_codec` does for a named one; corpora as byte arrays
    (`load_bytes`, W4-C4's), each a document W4-T2 and W4-T4's tests read;
    `fuzz_from_nothing` feeds both. libfuzzer targets 13 (`tlv`) and 14
    (`ber`), `tests/fuzz/CMakeLists.txt` appending `tlv` and `ber` under
    `if(CF_WITH_TLV)` and to `all_readers`, and their seeds written as the
    others are.
  * `docs/Guide.md`, section 8: the table gains
    `| TLV, by a layout | \`CF_WITH_TLV\` | read only; tag and length widths, byte order, an end tag, BER-TLV; each tag's key and type from the program's table (D-93) |`,
    and a subsection after the codec paragraph, "### A TLV layout", in the
    house voice, twenty to thirty lines: the `dev` layout of W4-T2 as code
    (the tag table, `CF_TLV_LAYOUT_INIT` and the fields set, a `static cf_codec`,
    `cf_tlv_codec`, `cf_builder_codec`, `cf_builder_file(..., "devcfg", 0)`),
    what explain() says for one value (`dev.cfg, byte 12`), the three unknown
    policies in a sentence each, and that tables are ascending. The same
    example compiled: `examples/c/tlv.c`, C89, in the style of
    `examples/c/secrets.c`, writing the document with `fopen("wb")` from the
    layout (the test's `put`), reading it, printing `net.port = 400` and the
    explain() line; registered in `examples/c/CMakeLists.txt`'s list with
    `example_expect(tlv ${CMAKE_CURRENT_BINARY_DIR}/tlv-run "net.port = 400" "dev.cfg, byte 12")`
    inside `if(CF_WITH_TLV)`.
  * `docs/Formats.md`: a row
    `| TLV | a layout the program gives; BER-TLV as X.690 section 8.1 and ISO/IEC 7816-4 | \`CF_WITH_TLV\`, by \`cf_tlv_codec\` (D-93) | no (wave 6) | the unit tests, fuzzed | indefinite BER lengths, a tag past 4 bytes, an unknown tag unless the layout skips or keeps it - each refused by name |`,
    and under "How the format is chosen": "A TLV codec is never chosen from
    content (`CF_CODEC_NAMED_ONLY`): a layout reads bytes no other format
    would refuse, so its name or its extensions choose it."
* **files**: `tests/unit/test_fuzz.c`, `tests/fuzz/libfuzzer.c`,
  `tests/fuzz/CMakeLists.txt`, `examples/c/tlv.c` (new),
  `examples/c/CMakeLists.txt`; last: `docs/Guide.md`, `docs/Formats.md`,
  `docs/Decisions.md`, `CHANGELOG.md`, `docs/Roadmap.md`.
* **steps**:
  1. The example's expectation and an `examples/c/tlv.c` that prints
     nothing. See it fail: `cmake --build --preset default && ctest --preset default -R example.tlv`
     fails on "net.port = 400".
  2. The example. Rerun step 1: passes.
  3. The fuzzing. `ctest --preset default -R unit.test_fuzz` passes; with
     clang, `ctest --preset fuzz -R "fuzz.(tlv|ber)"` passes.
  4. The Guide and Formats.md. ACCEPT-LINUX, ACCEPT-WINDOWS.
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI (the fuzz job).
* **stop if**: the fuzzer finds a crash, a leak or a sanitiser report in
  `tlv.c`: that is a finding. Write the input's bytes (as hex) and the
  report under "notes", fix it here only if the fix is in `tlv.c` and its
  test comes first; otherwise hand back.
* **docs**: D-93 gains "**Held to.**": the unit tests' three layouts, the
  fuzzers' two, the example the suite runs. CHANGELOG: the TLV entry gains
  "fuzzed, and shown in the Guide and `examples/c/tlv.c`". Roadmap: the row
  "Binary configuration: a layout-driven TLV codec, BER-TLV, CBOR" goes 🟩,
  `D-91, D-93`; wave 4 🟩 with its text "CBOR (D-91) and TLV by a layout,
  BER-TLV included (D-92, D-93). **1.5.0.**"
* **notes**:
