# K - correctness: the readers held to their standards

This group is for the evidence that a reader reads what its format says:
`.properties` against Java's own reader, the other libraries' known
weaknesses as tests, the readers proved under CBMC, differential fuzzing
against the libraries `bench/compare` already builds, the YAML, TOML and
INI limits made equal, one diagnostic model, the formats' table, and the
YAML reader's three differences from libyaml. Each card is read cold
against the tree before it opens (docs/Plan.md, "Read again after the
layout changes"); line numbers were read again against the tree at c16cae5 (2026-11-26).

Two Roadmap items are already done in part, with the evidence:

* "The YAML reader: no second document table" (item 5 of this group): the
  reader says the empty table only for a file with nothing in it
  (`lib/serialize/src/yaml.c:142`, `417`, `705`; N-4, f6c0ba7, which also
  took out the converter's and the pair test's workarounds). What is left
  is TOML's and INI's limits (K-5a, K-5b).
* "One diagnostic model" (item 6): `cf_reason_severity` and
  `cf_reason_weakness` exist (N-13, 2687154;
  `lib/base/include/configforge/cf_error.h:111-118`, tested in
  `lib/config/tests/test_reason.c`). What is left is the byte offset and
  the tools showing a reason (K-6a, K-6b).

Decisions used here: D-134 (K-1), D-135 (K-2), D-136 (K-3), D-137 (K-4);
K-5 amends the paragraph of D-109 that says "no one number is exact". A
card marked **TO DECIDE** has no number until the maintainer answers; D-138
(K-6a and K-6b), D-139 (K-7a) and D-140 (K-8) are kept for them.

## K-1a - `.properties` read by Java's own `Properties`, and the two compared

* **id**: K-1a
* **goal**: `scripts/check/conformance.py` runs `java.util.Properties.load(Reader)` and the library's `.properties` reader over the same small documents, compares the keys and their text (or that both refuse), and keeps a ratchet (`properties-java <read the same> <compared>` in `tests/conformance/floor.txt`) and a list of the differences kept by design, as it does for INI and `configparser`.
* **why**: `.properties` is held to its reader only by unit tests (`docs/Formats.md:17`), so a value that Java reads one way and this library another is found by a user. INI has had this comparison since 1.2 (`scripts/check/conformance.py:587-705`); this is the same for the second format that has a reference reader.
* **needs**: nothing
* **size**: medium
* **decided**:
  * **The reference is `java.util.Properties.load(Reader)`**, not `load(InputStream)`: the stream form reads ISO-8859-1, the library reads UTF-8 (`lib/serialize/src/properties.c:18-21`), and a `Reader` over UTF-8 bytes is the comparable one. The harness is `scripts/check/PropsDump.java`, one file, started as `java scripts/check/PropsDump.java <file>...` (JEP 330, JDK 11 or later; the development machine has OpenJDK 21). One JVM start for all cases, not one a case.
  * **PropsDump's output**, for each file argument in order: `== <index>` on a line of its own, then `REFUSED`, or `OK` and one line a key, sorted by key, `<key>\t<text>`, both escaped as `scripts/check/conformance.py:109-120` (`unescape`) reverses: `\\` for a backslash, `\t`, `\n`, `\r` and `\0`. A file whose `load` throws (`IllegalArgumentException` for a bad `\u` escape) or that is not valid UTF-8 prints `REFUSED`. The harness reads each file's bytes with `Files.readAllBytes` and decodes with `new InputStreamReader(new ByteArrayInputStream(b), StandardCharsets.UTF_8)` (a `Reader` built from a charset, not a decoder, replaces bad bytes; the card builds the decoder with `StandardCharsets.UTF_8.newDecoder()` and `CodingErrorAction.REPORT` and wraps it, so that an invalid byte is a refusal).
  * **The case table** is `PROPERTIES_CASES` in `conformance.py`, after `INI_CASES`, rows `(name, document, why they differ or None)` exactly as `INI_CASES`. The documents are Python `str` written as UTF-8 with `newline=""` (as `ini_read_library`, `conformance.py:676-681`). The library side is `keys_of(conform, "properties", path)` (`conformance.py:122-137`), which `tests/conformance/conform.c:300` already serves for any codec by name.
  * **Cases that must agree** (`None`), each a document of one to four lines; the name is the part before the colon: `plain` (`a=1`); `colon` (`a:1`); `space` (`a 1`); `tab-separator` (`a<TAB>1`); `formfeed-separator`; `spaces-around-equals` (`  a  =  1  `); `no-value` (`a`); `empty-value` (`a=`); `equals-in-value` (`a=b=c`); `colon-in-value` (`a:b:c`, key `a`, value `b:c`); `double-separator` (`a==1`, value `=1`); `leading-blank-lines`; `hash-comment` (`#c` then `a=1`); `bang-comment`; `indented-comment`; `comment-not-continued` (`# c \` then `a=1`: the next line is a key); `trailing-blanks-kept` (`a=b  `); `leading-value-blanks-dropped` (`a=   b`); `continuation` (`a=b\` then `   c`: value `bc`); `continuation-two`; `continuation-crlf`; `continuation-cr`; `continuation-blank-next` (`a=b\`, an empty line, `c=d`: the case records whatever Java does); `continuation-at-eof` (`a=b\`); `even-backslashes` (`a=b\\` then `c=d`); `three-backslashes` (`a=b\\\` then `c`: continues); `key-continued` (`a\` then `b=1`); `escape-tab`, `escape-newline`, `escape-cr`, `escape-formfeed`, `escape-backslash`; `escape-unknown` (`a=\q`: Java reads `q`); `escape-equals-in-key` (`a\=b=1`, key `a=b`); `escape-colon-in-key`; `unicode-escape` (a backslash, `u` and `0041`: spelt in the Python source as two joined strings, since the C sources never spell it, `properties.c:21-22`); `unicode-escape-lower-hex`; `surrogate-pair-escapes` (a pair of escapes for U+1F600); `unicode-escape-short` (`\u12` and the end of the line: both refuse); `unicode-escape-bad-digit` (both refuse); `utf8-value` (`a=é中`); `utf8-key` (`é=1`); `bom-first-line`; `duplicate-key` (last wins); `crlf`; `lone-cr`; `mixed-endings`; `last-line-no-newline`; `empty-document`; `only-comments`; `many-keys`; `dotted-keys` (`a.b=1`, `a.c=2`); `deep-dots` (`a.b.c.d=1`); `value-with-hash` (`a=b # not a comment`); `value-with-bang`; `percent-and-braces` (`a=%(x)s ${y}`, text in both); `long-value` (a value of 300 bytes).
  * **Differences kept by design**, each a row with its reason, checked to still differ (the run fails when one starts to agree, as `conformance.py:810-823` does for INI): (1) `empty-key` (`=v`, `:v`): Java reads the key `""`; the library refuses it (`properties.c:208-211`, "empty key"), the key space has no empty key (D-24). (2) `table-clash` (`a=1` then `a.b=2`): Java keeps two unrelated keys; the key space has `a` as a value and as a table and refuses by name (`CF_REASON_STRUCTURE`). `key-with-space` (`a\ b=1`) and `key-with-hyphen` (`my-key=1`) go here too if the library refuses them as keys (`properties.c:14-17`: "a key it cannot hold is refused by name") and in the `None` list if it reads them: the run shows which, and the reason is copied from `properties.c:14-17`. (3) `lone-surrogate-escape` (a backslash, `u`, `d800` and `x`): Java keeps a lone surrogate char; the library refuses ("a ... escape is half a surrogate pair", `properties.c:105-109`). (4) `invalid-utf8` (`a=` and the byte FF): Java's decoder refuses it; the library passes the bytes on (D-108's "today"), so the readings differ; the row goes when D-108's refusal covers `.properties`.
  * **The score** is `(read the same, compared)` over the `None` rows, printed as `properties-java: read as Java's Properties.load reads <n>/<m> (<k> more differ by design, listed in PROPERTIES_CASES)`, with the same `read differently:`, `by design:` (under `--list`) and `BY-DESIGN CASE NOW AGREES:` lines as `conformance.py:810-823`, and the floor check as `:825-831`. Without `java` on the path (`shutil.which("java")` is None) the line is `properties-java: skipped, no java` and nothing fails. The JDK is installed by `scripts/buildenv/setup.sh` (`default-jdk-headless` added to the package list at lines 18-24); `verify -DEXTRA=conformance` finds it on the path. CI does not run the conformance extra (no `conformance` in `.github/workflows/ci.yml`), so no CI change.
  * **A difference**: any `None` row where the two differ is a finding. K-1a does not fix it: it lists it in the card's notes and sets the floor to the score the run gives; K-1b fixes them.
* **files**: `scripts/check/PropsDump.java` (new), `scripts/check/conformance.py`, `tests/conformance/floor.txt`, `scripts/buildenv/setup.sh`; last: `docs/Formats.md`, `docs/Conformance.md`, `CHANGELOG.md`.
* **steps**:
  1. `git switch -c work/K-1a` from `main`; on the WSL machine, off `/mnt`.
  2. Write `PropsDump.java` as decided; run it by hand on two files, one valid and one with a bad escape, and read the output.
  3. Write `PROPERTIES_CASES`, `properties_read_reference` (runs PropsDump once over all cases, one temp file each, and parses the output into `{index: dict or None}`), `properties_read_library`, `properties_java` (as `ini_configparser`, `conformance.py:684-705`) and the report block after the INI one (`:810-831`). Run `python3 scripts/check/conformance.py <conform> <suites> --list --floor tests/conformance/floor.txt`, with `<conform>` built as `scripts/verify.cmake:1250-1272` builds it.
  4. **See it fail**: add `properties-java 99 99` to `tests/conformance/floor.txt` and see `BELOW THE FLOOR`; then write the real floor, the score the run printed.
  5. For each `read differently:` line: write the case's name, Java's reading and the library's under this card's notes. Do not change `properties.c`.
  6. `docs/Formats.md`: after the INI list (`docs/Formats.md:131-160` is the form) a list of the by-design rows with their reasons, and the `.properties` row of the table (line 17): evidence "the unit tests, and Java's `Properties`: n/m documents read the same, k more differing by design".
  7. `cmake -DEXTRA=conformance -DMODE=quick -DSOURCE=index -DJOBS=3 -DWORK=$HOME/cfv -P scripts/verify.cmake`.
* **accept**: ACCEPT-LINUX, with the `conformance` step printing `properties-java` and not below its floor; ACCEPT-WINDOWS (nothing changes there: the script skips without `java`); ACCEPT-CI.
* **stop if**: `java` cannot run `PropsDump.java` on the machine (no JDK): nothing is changed, say so; Java's output for a case differs between two runs; the library's reader crashes or hangs on a case (a defect of its own: stop, the input under notes).
* **docs**: `CHANGELOG.md` under "## 1.10.0 — unreleased" (the heading is made above "## 1.10.0 — unreleased" by the first card of that version that lands; later cards add to it), "### Added": the comparison and its score. `docs/Conformance.md:67-70`: a sentence after the INI one. The Roadmap item "`.properties` held to Java's `Properties`" stays ⬜ until K-1b. **D-134** (copy into `docs/Decisions.md` after D-112): *Question.* `.properties` has a reference reader; what is the standard, and what differs on purpose? *Options.* Compare with `load(InputStream)` (Latin-1; rejected: the library reads UTF-8, as every format here does, D-108). Compare with `load(Reader)` over UTF-8 (decided). *Decision.* Java's `Properties.load(Reader)` is the standard; the library follows it everywhere except the rows of `PROPERTIES_CASES` marked by design (an empty key, a key that clashes as a table, a lone surrogate escape, an invalid UTF-8 byte, any key the key space cannot hold), each checked to still differ. *Consequence.* A new difference is a finding, not a note: it lowers the score and fails the floor.

## K-1b - What the comparison with Java found, fixed

* **id**: K-1b
* **goal**: every `read differently:` line K-1a printed is gone: the library's reader reads the document as `Properties.load(Reader)` does, or the case is a by-design row with a reason.
* **why**: a configuration file the JVM and this library read differently is two programs reading one file differently.
* **needs**: K-1a
* **size**: medium (one commit a finding)
* **decided**:
  * **The rule**: Java's Javadoc of `Properties.load(Reader)` ("the line-oriented format") is right. A finding is fixed in `lib/serialize/src/properties.c` to read as Java does, unless Java's reading cannot be held by the key space (an empty key, a clashing table, a key with a character the library refuses) or is an invalid input Java repairs silently (a lone surrogate): those become by-design rows in `PROPERTIES_CASES` with the reason and need no code. Which a finding is, the rule answers; it is written as one line under "Organiser's answers", not asked.
  * **A fix** changes one function of `properties.c` (`prop_element`, lines 80-152, or `prop_decode`, 154-223) and comes with a case in `lib/config/tests/test_properties.c` (`g_doc` at 28-43 and `test_reads` show the form; never spell a backslash followed by `u` in C source, `properties.c:21-22`) that fails before the fix.
* **files**: `lib/serialize/src/properties.c`, `lib/config/tests/test_properties.c`, `scripts/check/conformance.py`, `tests/conformance/floor.txt`; last: `docs/Formats.md`, `CHANGELOG.md`, `docs/Roadmap.md`.
* **steps**:
  1. `git switch -c work/K-1b` from `main`, after K-1a landed. Run the comparison with `--list`: the findings are the `read differently:` lines (also in K-1a's notes).
  2. For each finding, in the order printed: decide by the rule. If a fix: write the failing unit case, build, `ctest --preset default -R test_properties` and **see it fail** (its own step); fix `properties.c`; see it pass; run the comparison and see the finding gone; commit `K-1b: .properties <what> read as Java reads it`.
  3. For a by-design finding: add the row and its reason to `PROPERTIES_CASES` and the line to `docs/Formats.md`'s list; commit.
  4. Raise the floor to the new score.
  5. Roadmap item to 🟩. `python scripts/work/work.py format`; the generators last; ACCEPT.
* **accept**: ACCEPT-LINUX with the conformance step at the new floor and `properties-java` n/n; ACCEPT-WINDOWS; ACCEPT-CI.
* **stop if**: a fix changes what a case of `lib/config/tests/test_properties.c` that passed before computes; a finding needs a decision about the key space (D-24) rather than the reader.
* **docs**: `CHANGELOG.md` under "## 1.10.0 — unreleased", "### Fixed" (one line a fix; "### Changed" where a document that read before is refused or the reverse, naming it); `docs/Roadmap.md` "`.properties` held to Java's `Properties`" to 🟩, and in the table of evidence (row E, `docs/Roadmap.md:104`) the parenthesis "Java's `Properties` needs a JVM the machines here do not have, so `.properties` is left to its own tests" becomes "`.properties` against Java's `Properties`, as `scripts/check/PropsDump.java` reads it". (`docs/Maintaining.md`'s 1.10.0 row already names "`.properties` against Java's own reader": no change there.)

## K-2a - The other libraries' known weaknesses, searched and counted, as a table

* **id**: K-2a
* **goal**: `docs/compliance/known-weaknesses.md` lists, one row a weakness, the CVEs and issue-tracker reports of the libraries the Roadmap names that apply to a reader, each with its rule, and says how many were found and how many are rows.
* **why**: a reader's known failures are what attackers try on the next one. The Roadmap's own first step is the search and the count.
* **needs**: nothing
* **size**: medium (research; the output is one document)
* **decided**:
  * **The libraries** (the Roadmap's list): libyaml, PyYAML, SnakeYAML, yaml-cpp, rapidyaml, RapidJSON, cJSON, json-c, Jansson, yyjson, simdjson, expat, libxml2, tinyxml2, inih, toml++, tinycbor, libcbor. "And others" is not searched: the list is closed so that the count means something.
  * **Where**: the NVD's API, one request a library and a 7-second pause between requests (five requests in 30 seconds without a key): `curl -s "https://services.nvd.nist.gov/rest/json/cves/2.0?keywordSearch=<library>&resultsPerPage=200"`, paging with `startIndex` past 200; each result read for its description, CWE and references. Then each project's tracker: `gh search issues --repo <owner/name> "<term>" --state all --limit 100` for each term in {overflow, crash, hang, timeout, stack, recursion, fuzz, OSS-Fuzz, CVE, segfault, "heap-buffer-overflow", "stack-overflow", quadratic}. The repository of each library is written in the document, from `bench/compare/libraries.cmake` where the library is there and otherwise from the NVD's reference. A tracker that cannot be reached is named in the document and not retried; a CVE the keyword search returns for another product (a name clash) is dropped.
  * **What is a row**: a weakness of a reader, that is of the parse of input bytes into values (not an emitter, a command line, a build system, an example program, a language binding's glue). One row a weakness class a library has: CVEs of one library, one rule and one cause are one row naming all of them in the source column. The id is `KW-<nn>` (two digits, three past 99), from `KW-01`, in table order, never reused.
  * **The columns**, and no others: `| id | source | rule | what happens | test | state |`. *source*: the library, the CVE or issue ids as links. *rule*: `R1` alias or entity expansion; `R2` stack exhaustion by deep nesting; `R3` integer overflow in a length or a count; `R4` quadratic or worse time on crafted input (hash collisions, repeated keys, long escapes); `R5` a read past the end on a truncated document; `R6` NUL inside a string; `R7` invalid UTF-8 or an overlong form; `R8` a number that overflows or loses precision silently; `R9` duplicate keys resolved differently than another reader (a confusion attack); `R10` a type tag or other feature that lets the reader build objects or run code; `R11` any other parse-time weakness (said in *what happens*). *test*: empty in this card. *state*: `todo` in every row.
  * **Above the table**: a line `Searched <date>: <n> CVEs and <m> issues found in <p> libraries; <k> are rows.` with the real numbers, exactly in that form (K-2b's gate reads it); under it a table `| library | repository | CVEs | issues | rows |`, one line a library of the eighteen; and a short paragraph on what a "row" is.
  * Nothing in this card is a test or a code change.
* **files**: `docs/compliance/known-weaknesses.md` (new); last: `CHANGELOG.md`.
* **steps**:
  1. `git switch -c work/K-2a` from `main`.
  2. Run the NVD searches, one library at a time, saving each response under the scratchpad (not in the tree); count.
  3. Run the tracker searches; keep the reports that say a reader crashed, hung, read out of bounds or read differently.
  4. Write the document as decided, with `todo` states.
  5. `ctest --preset default -R gate` (the documents' gates stay green).
* **accept**: ACCEPT-LINUX; the counting line's `k` equals `grep -c '^| KW-' docs/compliance/known-weaknesses.md`; ACCEPT-CI.
* **stop if**: the NVD's API refuses the requests (rate limit after the pauses, or an outage): write what was reached under notes and stop; fewer than ten rows result (the search was wrong).
* **docs**: `CHANGELOG.md` under "## 1.9.0 — unreleased", "### Added": the table and its count. No Roadmap change (K-2e).

## K-2b - `gate.weaknesses`: each row names a test that exists

* **id**: K-2b
* **goal**: a gate `gate.weaknesses` fails when a row of `docs/compliance/known-weaknesses.md` names a test that does not exist, has no valid state, repeats an id, or when the counting line is not the number of rows; and `lib/config/tests/test_weaknesses.c` exists, builds and passes with the first row tested.
* **why**: a table that names a test nobody runs drifts. `docs/Compliance.md:151` plans the same check for the CWE table as `gate.cwe`.
* **needs**: K-2a
* **size**: medium
* **decided**:
  * **The gate** is `cmake/gates/GateWeaknesses.cmake`, registered in `cmake/ConfigForgeGates.cmake` next to `gate.limits` (lines 111-115): `add_test(NAME gate.weaknesses COMMAND ${CMAKE_COMMAND} -DSRC=${CMAKE_SOURCE_DIR} -P ${CMAKE_SOURCE_DIR}/cmake/gates/GateWeaknesses.cmake)`. It reads the rows (`file(STRINGS ... REGEX "^\\| KW-")`) and fails with one line a fault: an id not `KW-` and digits; an id repeated or out of order; a rule not in R1-R11; a state not in {`held`, `open`, `n/a`, `todo`}; a state `held` or `open` whose *test* column is neither `test_weaknesses.c:kw_<nn>_<slug>` nor `fuzz:<seed file name>`; for the first, a function `static void kw_<nn>_<slug>(void)` absent from `lib/config/tests/test_weaknesses.c`, or, in a `held` row, never called from its `main` (an `open` row's function is there and is not called, K-2c); for the second, a seed file absent from `tests/fuzz/`; a state `n/a` whose *what happens* does not end in `Not applicable: <reason>.`; the counting line (the regular expression `^Searched [0-9-]+: [0-9]+ CVEs and [0-9]+ issues found in [0-9]+ libraries; ([0-9]+) are rows\.$`) missing or its `<k>` unequal to the number of rows. A state `todo` is allowed until K-2e switches the rule on.
  * **The test file** `lib/config/tests/test_weaknesses.c`, C89, added to `CF_UNIT_TESTS` (`lib/config/tests/CMakeLists.txt:10-`, after `test_properties`), has the includes and helpers of `test_properties.c:1-25` (`cf_test.h`, `spill`, `check`), one function a row `static void kw_<nn>_<slug>(void)` and a `main` calling each in order and returning nonzero on `g_failed`. This card writes the frame and one case as the form: the first row of rule R6 if there is one, else the first row. Its function reads the document the row describes through `cf_codec_by_name(...)->decode` with the capturing sink of `lib/serialize/tests/test_text_escape.c:717-760` copied, and checks the refusal or the value the row's rule says; the row becomes `held`.
  * A slug is the library name and a word, lowercase, `[a-z0-9_]`, at most 24 characters.
  * **gate.cwe** (`docs/Compliance.md:151`) has no card yet: when it is carded it reads this table with `GateWeaknesses.cmake`'s function; nothing here depends on it.
* **files**: `cmake/gates/GateWeaknesses.cmake` (new), `cmake/ConfigForgeGates.cmake`, `lib/config/tests/test_weaknesses.c` (new), `lib/config/tests/CMakeLists.txt`, `docs/compliance/known-weaknesses.md`; last: `docs/Compliance.md`, `CHANGELOG.md`.
* **steps**:
  1. `git switch -c work/K-2b` after K-2a.
  2. Write `GateWeaknesses.cmake` and register it; build and run `ctest --preset default -R gate.weaknesses`: it passes on the table as K-2a left it (all `todo`).
  3. **See it fail**: add a row whose test name is not in the file and see the gate name the row; remove it. Change the counting line by one and see it fail; put it back.
  4. Write the test frame and the first row's case; set the row `held` with its test column; `ctest --preset default -R "gate.weaknesses|test_weaknesses"`.
  5. `docs/Compliance.md`, a row under "Automation" (lines 141-160): `| Known weaknesses of other readers | the table docs/compliance/known-weaknesses.md, each row's test existing | gate.weaknesses | every commit | done (K-2b) |`.
  6. Format; generators last; ACCEPT.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS (a CMake script: runs there); ACCEPT-CI.
* **stop if**: the first row's weakness cannot be tested through `cf_codec_by_name(...)->decode` (it needs a library the build does not link): take the next row.
* **docs**: `CHANGELOG.md` "## 1.9.0 — unreleased", "### Added": `gate.weaknesses`. **D-135** (text for `docs/Decisions.md`): *Question.* How do other libraries' weaknesses become tests that stay true? *Options.* Prose in the CWE table (rejected: nothing checks it); one test a CVE (rejected: a library's twenty CVEs of one cause are one defence); one test a weakness row, named in the table, the name checked by a gate (decided). *Decision.* One row a weakness (`KW-nn`, rule R1-R11), its test a function `kw_nn_slug` in `lib/config/tests/test_weaknesses.c` or a fuzz seed, and `gate.weaknesses` fails a row whose test is not there; a weakness that does not apply is a row too, `n/a` with its reason, so that a reader of the table sees it was considered. *Consequence.* Adding a library's CVE is adding a row and a test; a row cannot stay `todo` once the table is complete.

## K-2c - The weaknesses of expansion, nesting, lengths and time, as tests

* **id**: K-2c
* **goal**: every row whose rule is R1, R2, R3 or R4 has its test in `lib/config/tests/test_weaknesses.c` and its state `held`, `open` (the library fails the test: a defect, carded apart) or `n/a`.
* **why**: these four take a process down with a small input (billion laughs, a stack of brackets, a count that wraps, a hash flood), and the readers have defences (D-82's decode budget, `CF_MAX_DEPTH`, `YAML_ALIAS_BYTES_FLOOR`, the keyed hash `cf_text_seed`) that nothing ties to the cases that broke the others.
* **needs**: K-2b
* **size**: medium (if the table has more than 25 such rows, the organiser cuts the rows after the 25th into K-2c2 before this card opens)
* **decided**:
  * **A test a row**, the function named in the row's *test* column (the row is edited as the test is written), reading a document the test builds in memory (a loop writing `[` a thousand times, aliases nested, a CBOR count field of `0xFFFFFFFF`, ten thousand keys that collide under the other library's hash). It runs the real reader through `cf_codec_by_name(format)->decode` with the capturing sink and checks one of: *refused*, with `err.reason` equal to the reason the reader returns (R1 `CF_REASON_BUDGET`; R2 `CF_REASON_DEPTH`; R3 `CF_REASON_TOO_LONG` or `CF_REASON_NUMBER_RANGE`, found by running and then written in); or *read within a bound*: for R4, the document is read and its cost held by `cf_arena_used` after it (as `FUZZ_COST_EMPTY` does, `lib/config/tests/test_fuzz.c`) below ten times the cost of reading a document of the same size with distinct keys (the test reads both; no number is written in; never a wall-clock time).
  * **A format whose build is off** skips: `#if CF_WITH_<FORMAT>`, as `test_properties.c:25` does.
  * **A row is `n/a`** when the format has no such feature (R1 for JSON: no aliases or entities); its *what happens* ends `Not applicable: <reason>.` and it has no test. XML's DOCTYPE is refused by name (`lib/serialize/src/xml.c:20-24`): that refusal is a test, `held`.
  * **An `open` row** is a test that failed: the case stays in the file, not called from `main` and with a comment saying why; the row's state is `open` with the case's name; the organiser writes the card. This card fixes no reader.
* **files**: `lib/config/tests/test_weaknesses.c`, `docs/compliance/known-weaknesses.md`; last: `CHANGELOG.md`.
* **steps**:
  1. `git switch -c work/K-2c`.
  2. List the rows: `grep -E '^\| KW-.*\| R[1-4] \|' docs/compliance/known-weaknesses.md`.
  3. For each row in order: write the case; build; run it; **see it fail** by pointing it one past what the reader holds (a depth of `CF_MAX_DEPTH + 1` as `CF_MAX_DEPTH`) and seeing it fail; put it back; set the row's state and test column; `ctest --preset default -R "gate.weaknesses|test_weaknesses"`.
  4. `ctest --preset sanitize -R test_weaknesses`.
  5. Format; generators last; ACCEPT.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; no row of R1-R4 is `todo`.
* **stop if**: a case needs more than 2 MiB or 2 seconds in the default preset; two rows cannot be told apart by a test (merge them in the table, one line under notes).
* **docs**: `CHANGELOG.md` "## 1.9.0 — unreleased", "### Added": the number of rows now tested.

## K-2d - The weaknesses of truncation, NUL, UTF-8, numbers and duplicates, as tests

* **id**: K-2d
* **goal**: every row whose rule is R5, R6, R7, R8 or R9 has its test, as K-2c's.
* **why**: these five make two readers read one file differently or read past it; the library refuses each by name (`CF_REASON_TRUNCATED`, `_CONTROL`, `_UTF8`, `_NUMBER_RANGE`, `_DUPLICATE_KEY`, `lib/base/include/configforge/cf_reason.def`).
* **needs**: K-2b
* **size**: medium
* **decided**: as K-2c, with these checks. R5: a valid document cut at every byte offset (the loop is the test): refused with `CF_REASON_TRUNCATED` or `_SYNTAX`, never accepted with a value that is not in the whole document, under ASan. R6: a NUL byte in a string, in a key and in a comment, in each text format: refused (`CF_REASON_CONTROL`) or read with the NUL kept in the text's length, whichever the reader does (found by running and written in the row). R7: the sequences of `lib/text/tests/test_text.c:55-61` and `66-75` placed in a string of each text format and of CBOR: refused with `CF_REASON_UTF8` where D-108 says the format refuses (TOML, CBOR, TLV today), and where D-108 says it passes the bytes on (JSON, JSON5, YAML, INI, `.properties`, dotenv) the row is `held` with the pass-on named. R8: a number past `int64` and `uint64` and a real that does not fit, in each typed format: `CF_REASON_NUMBER_RANGE` or text, never a wrapped value. R9: a key twice in JSON, TOML, YAML and CBOR maps and an attribute twice in XML: refused with `CF_REASON_DUPLICATE_KEY`; where a format keeps the later one (INI and `.properties`: "a later line wins"), `held` with that said.
* **files**: `lib/config/tests/test_weaknesses.c`, `docs/compliance/known-weaknesses.md`; last: `CHANGELOG.md`.
* **steps**: as K-2c, with the pattern `R[5-9]` in step 2.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; no row of R5-R9 is `todo`.
* **stop if**: as K-2c; a case shows a reader accepting a duplicate key or an invalid sequence its format says it refuses: that is an `open` row, not a fix here.
* **docs**: `CHANGELOG.md` as K-2c.

## K-2e - The last rows, the table closed, the Roadmap item done

* **id**: K-2e
* **goal**: the rows of R10 and R11 have their tests or their `n/a`, no row is `todo`, `gate.weaknesses` fails a `todo`, and `docs/Compliance.md` and `docs/Limits.md` name the table.
* **why**: R10 (a type tag that builds an object) is the one the Roadmap says is "not applicable here, said so in the table"; it is said row by row, with a reason a reader can check against D-39 and D-63 (the YAML reader refuses a tag of the writing application's).
* **needs**: K-2c, K-2d
* **size**: small
* **decided**: R10 rows are `n/a`, with the reason "the reader builds values of seven kinds and nothing else (`lib/base/include/configforge/cf_value.h`); a YAML tag of the writing application's is refused by name (D-39, D-63)", and one test row for the refusal itself (a `!!python/object` tag: `held`); R11 rows are tested as K-2c's. The gate's `todo` rule is switched on: `GateWeaknesses.cmake` fails any row `todo`.
* **files**: `cmake/gates/GateWeaknesses.cmake`, `lib/config/tests/test_weaknesses.c`, `docs/compliance/known-weaknesses.md`, `docs/Compliance.md`, `docs/Limits.md`; last: `CHANGELOG.md`, `docs/Roadmap.md`.
* **steps**:
  1. `git switch -c work/K-2e`.
  2. The R10 and R11 rows, as K-2c.
  3. Switch the `todo` rule on; run the gate on the finished table; **see it fail** with one row put back to `todo`; restore.
  4. `docs/Limits.md` (the parts `scripts/generate/limits.cmake` owns are not touched): one sentence and a link in its section "Not proven, not claimed" (`docs/Limits.md:214`, not generated). `ctest --preset default -R gate.limits`.
  5. Roadmap "The other libraries' CVEs and bug reports, as tests here" to 🟩, with the count. Format; generators last; ACCEPT.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `grep -c '| todo |' docs/compliance/known-weaknesses.md` prints 0.
* **stop if**: `gate.limits` fails after the sentence: put it in a part the generator does not own.
* **docs**: as above; `CHANGELOG.md` "## 1.9.0 — unreleased", "### Added": the table complete.

## K-3a - The blob's file reader proved up to a bound

* **id**: K-3a
* **goal**: `tests/proofs/proof_blob_file.c` is a CBMC harness that `verify -DEXTRA=proofs` runs, showing that for every file of up to 8 bytes, whatever size the system reports for it and however it is read in pieces, `cf_blob_file` reads and writes nothing outside the buffers it was given or allocated, and returns a blob whose `p[n]` is a NUL.
* **why**: `docs/Safety.md:139` ("Not yet proven") says the blob's file reader is not proved; every document a program reads from disk passes through it (`lib/serialize/src/blob.c:59-136`: the size prediction, the two-byte margin, the growth loop that doubles `cap`).
* **needs**: nothing
* **size**: large (a harness with the stdio calls stubbed)
* **decided**:
  * **The harness** is `tests/proofs/proof_blob_file.c`, in the form of `proof_bom.c` (header comment, `cbmc-sources:` line, `N` default 8, `nondet_*` declarations). Its `cbmc-sources:` is `lib/serialize/src/blob.c lib/base/src/error.c lib/port/src/port.c`, extended only by what the link names missing; if it needs more than `lib/base` and `lib/port`, stop.
  * **The stdio calls are the harness's own functions**, so that a body written here beats CBMC's model: `void* cf_platform_fopen(const char*, const char*)` (the signature of `lib/platform/include/configforge/cf_platform_file.h:71`; `lib/platform` is not linked) returns the address of a static `int` when a nondeterministic bool is true, else `NULL`; `int fseek(FILE*, long, int)` returns a nondeterministic 0 or -1; `long ftell(FILE*)` returns a nondeterministic `n` with `__CPROVER_assume(n >= -1 && n <= 3 * N)` (the prediction is a lie, D-76); `void clearerr(FILE*)` does nothing; `size_t fread(void*, size_t, size_t, FILE*)` copies at most `min(want, remaining)` bytes from a static `char data[N]` of nondeterministic bytes (`want = size * count`; the library always passes size 1), advancing a static position, and returns a nondeterministic count from 0 to that maximum, 0 only when it chose to or `remaining == 0`; `int ferror(FILE*)` returns a nondeterministic value only after a read that returned fewer bytes than asked; `int fclose(FILE*)` returns 0. `cf_arena_alloc(cf_arena*, size_t)` is the harness's own allocator, one fresh `malloc(n)` object a call so that CBMC bounds-checks each buffer by itself (a bump allocator over one static pool would hide an overrun into the next buffer), returning `NULL` when a nondeterministic bool says so or `n > 8192` (`lib/base/include/configforge/cf_alloc.h:67` is the signature; the arena is passed as a dummy non-NULL address). The path is a static string.
  * **The assertions**: on `CF_TRUE`: `b.n <= N` and `b.p[b.n] == '\0'` (the read is bounds-checked against the allocation `b.p` points into); on `CF_FALSE`, `err.code` is one of `CF_E_SOURCE`, `CF_E_MEMORY`, `CF_E_CAPABILITY`. CBMC's checks and `UNWIND` 48 are those of `scripts/check/proofs.cmake:42-55`.
  * **The bound**: the file is at most `N = 8` bytes.
  * `cf_blob_mem` and `cf_blob_skip_bom` are proved already (`tests/proofs/proof_bom.c`).
* **files**: `tests/proofs/proof_blob_file.c` (new); last: `docs/Safety.md`, `CHANGELOG.md`.
* **steps**:
  1. `git switch -c work/K-3a`. `cbmc --version` prints 5.x or later (`scripts/buildenv/setup.sh` installs it).
  2. Write the harness; run `cbmc tests/proofs/proof_blob_file.c <sources> -I <staged include> -DCF_INTERNAL_API=1 --bounds-check --pointer-check --signed-overflow-check --div-by-zero-check --undefined-shift-check --unwind 48 --unwinding-assertions` by hand, as `scripts/check/proofs.cmake:49-55` runs it, until `VERIFICATION SUCCESSFUL`.
  3. **See it fail**: in `blob.c` change `cap - len - 1` (line 100) to `cap - len`; the proof must fail on the write past the buffer, naming the line; restore it. Then change `memcpy(nb, buf, len)` (line 114) to `memcpy(nb, buf, len + 1)` and see the failure; restore.
  4. `cmake -DEXTRA=proofs -DMODE=quick -DSOURCE=index -DJOBS=3 -DWORK=$HOME/cfv -P scripts/verify.cmake`: `proof proof_blob_file: VERIFICATION SUCCESSFUL`. If it takes more than 15 minutes, lower `N` to 6 and say so in the Safety.md row.
  5. `docs/Safety.md`: a row in the table at lines 84-91, `| cf_blob_file - every file any reader opens | file of up to 8 bytes, any size the system reports, any short read | no access outside the buffers; the blob is NUL-terminated; false only with a named code |`; and "Not yet proven" (the section at lines 137-143) loses "the blob's file reader".
  6. Format; generators last; ACCEPT.
* **accept**: ACCEPT-LINUX with the `proofs` extra; ACCEPT-WINDOWS and ACCEPT-CI (the proof is tier 2 and not run there: both stay green).
* **stop if**: CBMC reports a duplicate symbol for a stubbed libc function that renaming cannot avoid (the fix would be a source change in `blob.c`: stop, write it under notes); a real failure shows (an access outside a buffer): stop, report it as a defect with the input CBMC prints under notes.
* **docs**: `CHANGELOG.md` "## 1.9.0 — unreleased", "### Added": the proof; `docs/Safety.md` as step 5. The Roadmap item stays 🟨 until K-3b. **D-136** (text): *Question.* What is "proved" for a reader, and to what bound? *Options.* An unbounded proof (EVA; on the readers it alarms on every loop: rejected for now); a bounded CBMC proof with a stated length (decided). *Decision.* A reader, or the blob, is proved when a harness in `tests/proofs` shows, for every input of up to N bytes, no out-of-bounds access, no signed overflow, no bad pointer and no shift out of range, with every loop unwound far enough that an assertion says so; `docs/Safety.md` names N. Stubs stand only for the operating system (stdio, the arena), never for the library's own code. *Consequence.* "Proved" on that page means a harness here, at a bound it states.

## K-3b - The INI reader proved up to a bound

* **id**: K-3b
* **goal**: `tests/proofs/proof_ini.c` shows that for every document of up to 6 bytes, `cf_codec_ini.decode` reads nothing outside the blob, calls the sink only with names and values lying inside the blob or the reader's own buffers, and, when it returns false, has set a message and a reason.
* **why**: INI is the smallest text reader and the one `docs/Safety.md:140` names first; its sink calls, its `[a.b.c]` path walk (`ini.c:56-86`) and its `name[]` arrays are where an off-by-one would be.
* **needs**: K-3a (the pattern and the Safety.md row)
* **size**: large
* **decided**:
  * **The entry** is the public codec: `cf_codec_ini.decode(&blob, &sink, &err)` (`ini_decode`, `lib/serialize/src/ini.c:300-525`, is static). The harness defines a `cf_doc_sink` (`lib/serialize/include/configforge/cf_serialize.h:126-150`) whose callbacks are its own: `begin_table` and `begin_array` read the first and last byte of `name` when it is not NULL (CBMC's pointer check then fails a name that does not lie in the blob's bytes or the reader's own `char[CF_KEY_MAX]` buffers) and return a nondeterministic bool; `scalar` does the same for `name` and for `v->u.t.p` with `v->u.t.n` bytes when `v->type == CF_TYPE_TEXT`; `end` returns a nondeterministic bool. The optional `reserve_text` and `reserve_work` members are left out: `sink.size = offsetof(cf_doc_sink, reserve_text)` (as `lib/serialize/src/cbor.c:934`).
  * **The sources**: `lib/serialize/src/ini.c lib/serialize/src/blob.c lib/text/src/space.c lib/text/src/track.c lib/text/src/fail.c lib/text/src/utf8.c lib/base/src/error.c lib/base/src/key.c lib/base/src/value.c lib/port/src/port.c lib/numeric/src/numeric.c`, extended only by what the link names missing, and nothing outside `lib/`. `CF_WITH_INI` is on by default; the harness defines nothing else.
  * **The bound**: `N = 6` bytes, `UNWIND` 48. The bytes are nondeterministic, so `[a]`, `a=b`, `a[]=1`, a comment, a BOM, CR and LF in every combination are covered. If `N = 6` does not finish in 30 minutes, `N = 4`; if `N = 4` does not, stop. The Safety.md row states the `N` that finished.
  * **The assertions**: after `decode`, if it returned CF_FALSE then `err.message[0] != 0` and `err.reason != CF_REASON_NONE` (every refusal says why, D-107; `CF_REASON_STORAGE` for a sink that said no); the sink is never called after `end` has been called.
* **files**: `tests/proofs/proof_ini.c` (new); last: `docs/Safety.md`, `CHANGELOG.md`, `docs/Roadmap.md`.
* **steps**:
  1. `git switch -c work/K-3b`.
  2. Write the harness; run CBMC by hand as K-3a step 2 until it verifies.
  3. **See it fail**: in `ini.c` change `s.end = in->p + in->n` (line 313) to `in->p + in->n + 1` and see the read past the blob named; restore. Change `cf_text_eol(CF_NL_LF, s.p, s.end)` (line 335) to pass `s.end + 1` and see it fail; restore.
  4. `verify -DEXTRA=proofs` as K-3a step 4.
  5. `docs/Safety.md`: the row `| cf_codec_ini.decode - every INI file | document of up to 6 bytes, every byte any value, a sink that may refuse | no access outside the blob; names and values the sink sees lie inside it; a refusal has a reason |`; "Not yet proven" (line 140) loses "(the INI reader first)" and says the other readers' tokenizers.
  6. Roadmap "The readers proved, not only tested" to 🟩, saying what is proved (the blob, the BOM skip, `cf_fmt`, the integer conversions, the INI reader) and that the other readers' tokenizers stay in "Not yet proven". Format; generators last; ACCEPT.
* **accept**: ACCEPT-LINUX with the `proofs` extra; ACCEPT-WINDOWS; ACCEPT-CI.
* **stop if**: no `N` of 4 or more finishes in 30 minutes (write the measured times under notes and keep K-3a's result only); CBMC shows a real fault (report the input, stop).
* **docs**: `CHANGELOG.md` "## 1.9.0 — unreleased", "### Added": the proof; Safety.md and the Roadmap as steps 5-6.

## K-4a - One harness for differential fuzzing, and JSON against yyjson

* **id**: K-4a
* **goal**: `tests/fuzz/diff_known.c` and `.h` hold what the differential targets share (the known-differences file, the output buffer, the line writer), `yamldiff.c` uses them with its behaviour unchanged, and `fuzz_jsondiff` feeds the same bytes to ConfigForge's JSON reader and to yyjson, compares what each read as lines, and aborts on a difference `tests/fuzz/jsondiff.known` does not document.
* **why**: two programs reading one JSON file differently is the confusion the Roadmap names. N-12 built the harness for YAML and libyaml (`tests/fuzz/yamldiff.c`, d8a0cfa); the libraries of P-1b are pinned and fetched already (`bench/compare/libraries.cmake:21-35`).
* **needs**: nothing (P-1b landed)
* **size**: large
* **decided**:
  * **The shared part** is `tests/fuzz/yamldiff.c:26-232` made into `tests/fuzz/diff_known.c` and `.h`: `yd_rule`, `yd_die`, `yd_hex`, `yd_unescape`, `yd_load`, `yd_has` and `yd_covered` (the rule file, its three fields and its escapes, `tests/fuzz/yamldiff.known:1-11`), `yd_out` with `yd_put`, `yd_puts` and `yd_quoted`. What stays in `yamldiff.c` is from line 234 (`yd_table`, the sink, `yd_run`, `yd_tail`, `yd_report`) and `LLVMFuzzerTestOneInput`; `yamldiff_builtin.c` and `yamldiff_libyaml.c` (each only includes `lib/serialize/src/yaml.c`) are not touched. They are renamed `dk_*` and take the rule file's name as an argument, and the sides' names as an array (`lib-refuses`, `bi-refuses`, `events` for YAML; `lib-refuses`, `cf-refuses`, `values` for JSON). `yamldiff.c` keeps its sink and its `LLVMFuzzerTestOneInput`, and `tests/fuzz/CMakeLists.txt` adds `diff_known.c` to `fuzz_yamldiff`'s sources (its `YD_KNOWN` definition, the rule file's path, is passed to `dk_load` as that argument).
  * **The other reader** is yyjson 0.13.0 (`LIB_yyjson_COMMIT`, `bench/compare/libraries.cmake:33-35`), pure C, two files (`src/yyjson.c`, `src/yyjson.h`) compiled into the fuzz target. Where it is: `tests/fuzz/CMakeLists.txt` takes `-DCF_DIFF_LIBS=<dir>`, the directory `cmake -DLIBS=<dir> -P scripts/bench/fetch.cmake` fills, one directory a library (`<dir>/yyjson/src/yyjson.c`; `scripts/bench/fetch.cmake:6-9, 34`); a target whose library is not there is not built, with a `message(STATUS ...)` as at `tests/fuzz/CMakeLists.txt:176`. yyjson reads with `YYJSON_READ_NOFLAG` (strict).
  * **What each side writes**: one line a value in document order, `<dotted key>\t<kind>\t<text>`, `kind` one of `text`, `int`, `real`, `bool`, `null`; an array's element `name.<i>`, an object's member `name.<key>`; a root that is not an object is wrapped as `tests/conformance/conform.c` wraps it (`{"v": ...}`, `docs/Conformance.md:96-100`), by the same rule before the call on both sides. Numbers: `int` when the text has no `.`, `e` or `E` and fits int64 (decimal text); `real` as `%.17g` of the double each side read; a number that fits neither: ConfigForge refuses it by name (`CF_REASON_NUMBER_RANGE`) and yyjson reads it as a real, so a rule `cf-refuses` with the reason "ConfigForge is right to refuse what its types cannot hold (D-82)" is written when the first run finds it. Strings: the bytes after escapes.
  * **The known file's sides**: `lib-refuses` (yyjson refuses, ConfigForge reads), `cf-refuses`, `values` (both read, the lines differ). A rule's key is as `yamldiff.known:7-10`: a part, 8 bytes at least, of the refusing side's message; for `values` a byte string of 3 or more that the input holds. A difference with no rule aborts, so libFuzzer keeps the input.
  * **The first run's findings**: the card runs the fuzzer ten minutes (`-max_total_time=600`); each class of difference is judged by the standard (RFC 8259, ECMA-404) and becomes one rule with its reason ("ConfigForge is right because ..." or "yyjson is right ...; to fix", as `yamldiff.known` does) and the input as a case in `lib/config/tests/test_reader_corpus.c` (the form of that file's rows). A reading where ConfigForge is wrong is a defect and a card of its own: here it is a rule starting `to fix`, never a change to the reader.
  * **The seeds**: `tests/fuzz/seed_jsondiff` is `{"a": 1, "b": [true, null], "c": {"d": "e", "f": 1.5}}`, and the JSON seeds the build already writes are listed in `-seed_inputs`.
  * **The CI job** `diffs`, beside `yamldiff` (`.github/workflows/ci.yml:438-462`), `schedule` and `workflow_dispatch` only, `continue-on-error: true`, steps as that job's plus `cmake -DLIBS=$HOME/cfb-libs -P scripts/bench/fetch.cmake` and `cmake --preset fuzz -B build-diffs -DCF_DIFF_LIBS=$HOME/cfb-libs`, building `fuzz_jsondiff` and running it ten minutes; a failure uploads `crash-*` as `yamldiff` does. Not a required check.
* **files**: `tests/fuzz/diff_known.c`, `tests/fuzz/diff_known.h`, `tests/fuzz/jsondiff.c`, `tests/fuzz/jsondiff.known` (new); `tests/fuzz/yamldiff.c`, `tests/fuzz/CMakeLists.txt`, `.github/workflows/ci.yml`; last: `docs/Safety.md`, `CHANGELOG.md`, `docs/Roadmap.md`.
* **steps**:
  1. `git switch -c work/K-4a`. Build `fuzz_yamldiff` as it is (`cmake --preset fuzz -B build-fz && cmake --build build-fz --target fuzz_seeds fuzz_yamldiff`) and run it 60 seconds: the reference.
  2. Move the shared part to `diff_known.c/.h`; rebuild `fuzz_yamldiff`; run 60 seconds: the same behaviour (the same rule file read, no new abort).
  3. Fetch the libraries and write `jsondiff.c` with an empty `jsondiff.known`. **See it fail**: build and run it 60 seconds; it aborts on a first difference, or, if it runs clean, a deliberate difference (one side's `bool` text flipped) makes it abort; remove the deliberate one.
  4. Run ten minutes; judge each class; write the rules and the regression cases.
  5. The CI job; `docs/Safety.md:43-48` (where `fuzz_yamldiff` is described): after it a paragraph naming `fuzz_jsondiff`, its build (`-DCF_DIFF_LIBS=<dir>` after `scripts/bench/fetch.cmake`), its run and the `diffs` job; K-4b and K-4c add their targets to it.
  6. Format; generators last; ACCEPT-LINUX with `-DEXTRA=fuzz` (60 seconds).
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS (nothing is built there); ACCEPT-CI (the new job is not required; the existing jobs stay green); `fuzz_jsondiff` ten minutes with no abort.
* **stop if**: a difference class cannot be judged from the standard (write it as a rule `to decide` with both readings and go on); the move to `diff_known.c` changes `fuzz_yamldiff`'s behaviour (revert the move).
* **docs**: `CHANGELOG.md` "## 1.9.0 — unreleased", "### Added": `fuzz_jsondiff` and the shared harness. **D-137** (text): *Question.* What does a difference between two readers mean? *Options.* Any difference is a defect here (rejected: some are the other reader's, some are choices); only a refusal on one side (rejected: a different value is the worse confusion); every difference a recorded rule saying which reading is right and why, an undocumented one aborting (decided, N-12's rule). *Decision.* A differential target compares each side's lines; both refusing is agreement; every difference has a rule in the target's `.known` file with its reason, or the target aborts; a rule starting `to fix` is a defect carded apart. *Consequence.* One rule file form for YAML, JSON, TOML and CBOR; the Roadmap's "which reading is right and why" is the reason column.

## K-4b - TOML against tomlc17

* **id**: K-4b
* **goal**: `fuzz_tomldiff` feeds the same bytes to ConfigForge's TOML reader and to tomlc17, compares as K-4a, and aborts on an undocumented difference in `tests/fuzz/tomldiff.known`.
* **why**: TOML 1.1 has the largest suite (toml-test, 720 documents, `docs/Conformance.md:59`) and still two readers in the comparison (`bench/compare/libraries.cmake`: tomlc17 and toml++); a fuzzer finds what a suite does not.
* **needs**: K-4a
* **size**: medium
* **decided**: as K-4a, with: the other reader is tomlc17 (C; the files `bench/compare/libs/tomlc17.cmake` names), compiled into the target; toml++ is C++ and is not used. The result of `toml_parse` is walked to lines `<dotted key>\t<kind>\t<text>`: strings `text`, int64 `int`, fp64 `real` (`%.17g`), booleans `bool`, dates, times and date-times as `text` of the ISO form each side prints (ConfigForge reads a date or time as text, `lib/serialize/src/toml.c`; the card reads how in its first run and writes each side's rendering as one function, never a rule that hides a value); arrays `name.<i>`, arrays of tables the same, an inline table as a table. The sides: `lib-refuses`, `cf-refuses`, `values`. Seeds: the build's `seed_toml` and `[s]\nk = 1\n`. The `diffs` job gets `fuzz_tomldiff` and ten minutes.
* **files**: `tests/fuzz/tomldiff.c`, `tests/fuzz/tomldiff.known` (new); `tests/fuzz/CMakeLists.txt`, `.github/workflows/ci.yml`; last: `docs/Safety.md`, `CHANGELOG.md`, `docs/Roadmap.md`.
* **steps**: as K-4a steps 3-6 for TOML.
* **accept**: as K-4a.
* **stop if**: as K-4a.
* **docs**: `CHANGELOG.md` "## 1.9.0 — unreleased", "### Added": `fuzz_tomldiff`.

## K-4c - CBOR against tinycbor

* **id**: K-4c
* **goal**: `fuzz_cbordiff` feeds the same bytes to ConfigForge's CBOR reader and to tinycbor and aborts on an undocumented difference in `tests/fuzz/cbordiff.known`.
* **why**: CBOR's readers differ on what is well formed (RFC 8949 Appendix F), on duplicate map keys and on invalid UTF-8; `bench/compare/probes.expected` already shows one (tinycbor does not check duplicate keys).
* **needs**: K-4a
* **size**: medium
* **decided**: as K-4a, with: the other reader is tinycbor (`bench/compare/libs/tinycbor.cmake`), `cbor_parser_init` and `cbor_value_validate` in strict mode first (valid UTF-8 in text strings, no trailing bytes: `bench/compare/adapters/tinycbor.cpp:1-5`), then the walk of `adapters/tinycbor.cpp:14-60` writing lines. Kinds: integers within int64 `int`; text strings `text`; floats `real` (`%.17g`, a half float widened). Byte strings, tags and simple values other than true, false and null are refused by ConfigForge by name (the model has no such kind, D-91): tinycbor's walker stops with "unsupported" on them too, and a document that reaches one on either side is not compared. A map key that is not text is the same. A duplicate key is not checked by tinycbor: the rule `cf-refuses` on "duplicate" is expected, with the reason "ConfigForge is right: a map with a repeated key is ambiguous (RFC 8949 5.6 lets a decoder refuse it) and a configuration needs one value". Seeds: the Appendix A vectors under `tests/conformance/data` when present, else the bytes `a1 61 61 01` (`{"a": 1}`).
* **files**: `tests/fuzz/cbordiff.c`, `tests/fuzz/cbordiff.known` (new); `tests/fuzz/CMakeLists.txt`, `.github/workflows/ci.yml`; last: `docs/Safety.md`, `CHANGELOG.md`, `docs/Roadmap.md`.
* **steps**: as K-4b.
* **accept**: as K-4a.
* **stop if**: as K-4a.
* **docs**: `CHANGELOG.md` "## 1.9.0 — unreleased", "### Added": `fuzz_cbordiff`.

## K-4d - XML against a second parser (TO DECIDE)

* **id**: K-4d
* **goal**: XML's differential target is answered: either built against a second parser, or the Roadmap item's "XML" closed with the reason.
* **why**: ConfigForge's XML reader sits on expat (`lib/serialize/src/xml.c:1`, and `bench/compare/libraries.cmake:14-16` says the comparison links the same expat), so expat on the other side compares a library with itself; what can differ is only this reader's own rules (the key mapping, D-23).
* **needs**: K-4a
* **size**: medium if built
* **decided**: if built, as K-4a: the lines `<dotted key>\t<kind>\t<text>` are made from the second parser's events by D-23's mapping (written once in the target, from `lib/serialize/src/xml.c:3-24`), the sides `lib-refuses`, `cf-refuses`, `values`, and the `diffs` job gets `fuzz_xmldiff`; what is only well-formedness is compared as accept or refuse alone.
* **TO DECIDE** (the maintainer): the second parser is a **new external dependency**, pinned and fetched like the others (`bench/compare/libraries.cmake`: a commit, its date, a licence). Options: (A) none: close the Roadmap item's "XML" with "the reader is expat's; its own mapping is held by `lib/config/tests` and `tests/conformance`", no card. (B) libxml2's SAX2 interface, at its newest release at least 30 days old, used for accept or refuse of well-formedness only (its DOCTYPE and namespace handling differ from what this reader refuses by name, so values are not compared). (C) tinyxml2 or pugixml (C++: the target would be C++). **Recommendation: B**, accept or refuse only, in the `diffs` job: expat and libxml2 have disagreed in places that were CVEs in both, and a well-formedness difference is the confusion the Roadmap names; values stay with D-23's tests. Until answered the card is parked.
* **files**: (if B) `tests/fuzz/xmldiff.c`, `tests/fuzz/xmldiff.known` (new); `scripts/bench/fetch.cmake` and `bench/compare/libraries.cmake` (a `libxml2` entry, used by the fuzz job only), `tests/fuzz/CMakeLists.txt`, `.github/workflows/ci.yml`; last: `docs/Dependencies.md`, `CHANGELOG.md`, `docs/Roadmap.md`.
* **steps**: (if B) as K-4a steps 3-6, plus `docs/Dependencies.md`: a row (grep its table for the form) naming libxml2 as a fuzz-job dependency only; Roadmap "Differential fuzzing against the other readers" to 🟩 with "JSON, TOML, XML (well-formedness), CBOR and YAML".
* **accept**: as K-4a.
* **stop if**: as K-4a; the library's licence, or its build, needs more than a C compiler and its own sources.
* **docs**: `CHANGELOG.md` "## 1.9.0 — unreleased", "### Added". If A: Roadmap "Differential fuzzing against the other readers" to 🟩 naming K-4a to K-4c and saying XML is closed by reason.
* **Organiser's answers** (2026-11-26, the maintainer, relayed from a side session: "apply the best recommendations wherever a recommendation will do"): the card's recommendation, adapted to D-220: a number the recommendation fixes as a hard limit is an option whose default is that number.

## K-5a - TOML's joined path judged as one number

* **id**: K-5a
* **goal**: TOML's reader refuses a key line whose header path and dotted key together are longer than `CF_KEY_MAX - 2` bytes, with the wording of a header too long, and `cf_format_limits("toml")` is exact: `test_holds` reads every tested split of a path at the limit and refuses one byte more.
* **why**: today a header path and a dotted key are each limited to `CF_KEY_MAX` separately (`lib/serialize/src/toml.c:125`, `151`, `412-448`, `1359`), so "no one number is exact" (`docs/Decisions.md:4885-4890`); the pair test found a path the writer wrote and the reader refused.
* **needs**: nothing
* **size**: medium
* **decided**:
  * **The rule**: a key line `k = v` (and the member keys of an inline table, which join the same way) under a header whose path took `s->nused` bytes (`toml.c:126`: its segments plus one NUL each) joins to a path of `nused` plus the dotted key's own bytes, with a dot between the key's segments; the joined path may not exceed `CF_KEY_MAX - 2`, the number `cf_formats[]` records (`lib/serialize/src/codec.c:321-326`). One byte more: `toml_fail(s, s->p, CF_REASON_TOO_LONG, "the key is too long")`, at the key, the wording of `toml.c:420` and `446`.
  * **Where**: in `toml_key` (`toml.c:434-461`): beside the check at `toml.c:445-446` (`used + 2 > sizeof k->buf`) a comparison of `s->nused + used` with `CF_KEY_MAX - 2`. The unit (whether the segment's NUL and the dot count as one byte each) is found by the test, which is written first: the constant is adjusted until the limit is the number `cf_formats[]` says.
  * **`test_holds`** (`lib/serialize/tests/test_holds.c:275-316`): the TOML branch of `test_limits` reads today a header of `max_path` bytes in two spellings (`header(max_path, dotted)`, line 262) followed by `k = 1`; with the new rule that joins to `max_path + 2` bytes and is refused, so those two cases become `header(max_path - 2, dotted)` (the header and `.k` are exactly `max_path`; the first run of the test shows whether `- 2` is the right unit, and the constant in `toml_key` is adjusted until it is). The test gains a helper `static int refused_with(const cf_codec* c, int reason)` beside `reads` (line 198; the same body, returning 1 when decode fails with `err.reason == reason`) and, for the splits `s` = 0, 1, `max_path / 2` and `max_path - 2`, a document of a header of `s` bytes (no header for 0) and a key line `k... = 1` whose key has `max_path - s - 1` bytes (`max_path` bytes for `s` = 0): it reads; with the key one byte longer, `refused_with(c, CF_REASON_TOO_LONG)`.
  * No change to `cf_formats[]` and no new public name.
* **files**: `lib/serialize/tests/test_holds.c`, `lib/serialize/src/toml.c`; last: `docs/Decisions.md` (D-109's paragraph), `CHANGELOG.md`.
* **steps**:
  1. `git switch -c work/K-5a`.
  2. Write the split cases in `test_limits`; build; `ctest --preset default -R test_holds`.
  3. **See it fail**: the one-byte-more case is read today (the two limits are separate); see `test_holds` fail on it.
  4. Make the change in `toml_key`; run until the exact-limit cases read and the one byte more is refused; `ctest --preset default` whole, and `verify -DEXTRA=conformance` (the floor `toml 218 502` holds).
  5. `docs/Decisions.md:4885-4890` (D-109): replace "so no one number is exact" with the rule, one sentence.
  6. Format; generators last; ACCEPT.
* **accept**: ACCEPT-LINUX with the conformance extra; ACCEPT-WINDOWS; ACCEPT-CI.
* **stop if**: a toml-test document that was valid is refused (the floor falls): revert, the document under notes.
* **docs**: `CHANGELOG.md` "## 1.9.0 — unreleased", "### Changed": a TOML key line whose header and key together exceed the path limit is refused by name (it was read before and refused later by the key space). D-109's sentence. The Roadmap item "The YAML reader: no second document table" stays 🟨 until K-5b.

## K-5b - INI's reader limits nesting as the others do

* **id**: K-5b
* **goal**: INI's reader refuses a section header of more than `CF_MAX_DEPTH - 1` segments with `CF_REASON_DEPTH`, `cf_formats[]` records that as INI's `max_depth`, and `test_holds` reads a header of that many segments and refuses one more.
* **why**: INI is the one reader with no depth limit (`lib/serialize/src/codec.c:321-322`: `NO_LIMIT`); a header of ten thousand segments reaches the sink and the key space, which refuse it with a message about a key and not about depth.
* **needs**: K-5a (the same test function; no code dependency)
* **size**: small
* **decided**: in `ini_emit_path` (`lib/serialize/src/ini.c:56-86`), after `++*depth` (line 82), inside the segment loop: `if (*depth > (int)(CF_MAX_DEPTH - 1)) return cf_text_fail(err, CF_E_SOURCE, file, line, 0, CF_REASON_DEPTH, "tables are nested too deeply");` (the wording of `toml.c:1350`; `CF_E_SOURCE` as the storage refusal of this function, lines 62-66). `cf_formats[]`'s INI entry becomes `{"ini", ..., (size_t)CF_KEY_MAX - 2u, DEPTH_MAX}` (`codec.c:321-322`). The test: the INI branch of `test_limits` (`test_holds.c:289`: `strcmp(names[i], "ini") != 0` excludes it today) is included; `deep("ini", max_depth)` reads and `refused_with(c, CF_REASON_DEPTH)` holds for `deep("ini", max_depth + 1)` (`refused_with` is the helper K-5a adds; `deep`, line 225, has no INI branch: the card adds one, `[a` then `count - 1` times `.a` then `]\nk = 1\n`, as its TOML branch). The encoder side: INI's writer stops at its stack (`ini.c:554`, `cf_u16 stack[CF_MAX_DEPTH]`); the pair test (`verify`'s `conformance pairs` step) must stay green, which shows the writer never writes deeper than the reader reads.
* **files**: `lib/serialize/tests/test_holds.c`, `lib/serialize/src/ini.c`, `lib/serialize/src/codec.c`; last: `docs/Decisions.md` (D-109), `docs/Limits.md` (generated: `cmake -P scripts/generate/limits.cmake`), `docs/Formats.md` (line 63), `CHANGELOG.md`, `docs/Roadmap.md`.
* **steps**:
  1. `git switch -c work/K-5b`.
  2. Include INI in `test_limits`'s depth part with the table entry changed first, and the refusal case `deep("ini", max_depth + 1)`; build; **see it fail** (INI reads it today).
  3. Make the changes; `ctest --preset default`; `verify -DEXTRA=conformance` (floor `ini-configparser 36 36` holds), and its `conformance pairs` step.
  4. `docs/Decisions.md:4883-4885`: "INI none in its reader" becomes "INI 31"; `docs/Formats.md:63`; regenerate `docs/Limits.md`; `ctest --preset default -R gate.limits`.
  5. Roadmap "The YAML reader: no second document table" to 🟩 (K-5a, K-5b; N-4 earlier). Format; generators last; ACCEPT.
* **accept**: ACCEPT-LINUX with the conformance extra; ACCEPT-WINDOWS; ACCEPT-CI; `gate.limits` green.
* **stop if**: the pair test finds INI's writer writing a table deeper than the new limit (then the limit is the writer's depth: stop, the numbers under notes); a fixture in `tests/` or `examples/` with a header deeper than 31 segments.
* **docs**: `CHANGELOG.md` "## 1.9.0 — unreleased", "### Changed": INI refuses a header nested deeper than 31 tables, by name (it reached the key space before); D-109's sentence; the Roadmap row.

## K-6a - A refusal's byte offset (TO DECIDE)

* **id**: K-6a
* **goal**: a refusal can say the byte offset it happened at as a number a program reads, in every reader that has one, as it says line, column and reason.
* **why**: D-107 named the byte offset and did not build it (`docs/Decisions.md:4767-4770`: "stays in the message"); CBOR and TLV have no line and keep their place only in the message text (`lib/serialize/src/cbor.c:92`, `160`, `172`: "byte %zu"), so a test of "the same bad input refused the same way" cannot compare it and a program cannot show where.
* **needs**: nothing
* **size**: medium
* **decided**: every codec reports through `cf_text_fail` (`lib/text/src/fail.c`) or `cf_error_place` (`lib/base/include/configforge/cf_error.h:123-124`) with a line and a column; the offset is added at the same call sites, as `(size_t)(s->p - in->p)` where the scan has both pointers (CBOR and TLV: their own `off`). **Compatibility**: additive through `size` (D-51; `cf_error.h:51-62`), as `where` and `reason` were added in 1.8.
* **TO DECIDE** (the maintainer): the shape of a new public member of `cf_error`. Options: (A) `size_t offset;` after `reason`, `(size_t)-1` when unknown, filled through a new `CF_EXPERIMENTAL(1, 9) void cf_error_place_at(cf_error*, int reason, unsigned line, unsigned col, size_t offset)` and read directly (`CF_ERROR_INIT`, `cf_error.h:73`, gains a member; it is fenced from the formatter for that reason). (B) the offset inside `cf_loc`: every `cf_loc` grows, an ABI change in 1.x, rejected. (C) leave it in the message and add `cf_error_offset(const cf_error*)`, which parses the message: rejected (a message is not an interface). **Recommendation: A**, experimental in 1.9 as N-13's `cf_reason_severity` is: one function and one member; `gate.abi` gets the function. Until answered the card is parked.
* **files**: (if A) `lib/base/include/configforge/cf_error.h`, `lib/base/src/error.c`, `lib/text/src/fail.c`, `lib/serialize/src/cbor.c`, `lib/serialize/src/tlv.c`, `lib/serialize/src/{json,toml,yamlparse,ini,xml,properties}.c` (each call site that has a pointer into the input), `lib/config/tests/test_reason.c`; last: `docs/Decisions.md` (D-138), `docs/API.md` and `docs/Codemap.md` (generated), `CHANGELOG.md`, `docs/Roadmap.md`.
* **steps**: (if A)
  1. `git switch -c work/K-6a`.
  2. In `test_reason.c`, to each row of `rows` (the refusing documents; `test_rows`, line 434, loops over them) add the offset its document must report, computed by hand (the index of the byte that is the cause); **see it fail** (the member does not exist: a compile error is the first failure, then every offset is `(size_t)-1`).
  3. Add the member and the function; `cf_text_fail` takes the offset from its caller's pointers; CBOR and TLV pass their `off`.
  4. `ctest --preset default` and `--preset sanitize`.
  5. `docs/Decisions.md` D-138: the chosen option's text, with D-107's "byte offset" now built.
  6. Format; generators last; ACCEPT.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `gate.abi` and `gate.apiref` green.
* **stop if**: a reader refuses from a point it has no pointer for (a refusal after the document, from the sink): its offset is `(size_t)-1` and `test_reason.c` says so for that row; more than three readers cannot give one.
* **docs**: `CHANGELOG.md` "## 1.9.0 — unreleased", "### Added" (experimental in 1.9); D-138; the Roadmap item "One diagnostic model across formats" stays 🟨 until K-6b.

## K-6b - A refusal's reason shown by the tools (TO DECIDE)

* **id**: K-6b
* **goal**: `configforge codec dump`, `recode` and `detect`, when they refuse a file, show the reason name, its severity and its weakness, so that a user filters by reason and not by the words of a message.
* **why**: every refusal has a reason (A-10) and each reason its severity and CWE (N-13), but the tools print only `err.message` (`tools/codec/dump.c:312`, `369`, `tools/codec/recode.c:172`, `tools/codec/detect.c:240`), so the model is not what a user sees.
* **needs**: nothing
* **size**: small
* **decided**: the text, one line after the message, on stderr (`detect.c:240` prints its message on stdout; its reason line goes to stderr too, so that no stdout changes): `  reason: <cf_reason_name> (<cf_reason_severity>[, <cf_reason_weakness>])`, for instance `  reason: depth (warning, CWE-674)`; printed only when `err.reason != CF_REASON_NONE`. One helper in `tools/codec/codec.c` (`codec_report(const cf_error*)`, declared in `tools/codec/codec.h`), called by the four sites. A case in `tests/front/front.cmake`, inside `if(HAVE_CODEC)` (line 123): it writes a JSON file of 40 nested `{"a":` with `file(WRITE ...)` under `${WORK}` (as line 135 does) and runs `front_case(<the exit code `codec dump` returns on a refusal, read from `tools/codec/dump.c`> - "reason: depth" codec dump <file>)`.
* **TO DECIDE** (the maintainer): whether the line is printed always or only with a flag. Options: (A) always: stderr's text is not a documented interface (`docs/Tools.md` documents exit codes and stdout) and a second line is read by a person; (B) only with `--reason`, so that no existing match on stderr changes before 2.0.0. **Recommendation: A**: the project treats the message text as free (D-107: a user filters by code and not by message text is the point), and a flag nobody passes defeats the aim. Until answered the card is parked.
* **files**: `tools/codec/codec.c`, `tools/codec/codec.h`, `tools/codec/dump.c`, `tools/codec/recode.c`, `tools/codec/detect.c`, `tests/front/front.cmake`; last: `docs/Tools.md`, `CHANGELOG.md`, `docs/Roadmap.md`.
* **steps**:
  1. `git switch -c work/K-6b`. Write the front-door test first; **see it fail**.
  2. The helper and the four call sites.
  3. `docs/Tools.md`: the codec section's paragraph on refusals names the line.
  4. Roadmap "One diagnostic model across formats" to 🟩 (K-6a, K-6b; A-10 and N-13 earlier). Format; generators last; ACCEPT.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI.
* **stop if**: a test in `tests/` matches stderr exactly and fails (take option B).
* **docs**: `CHANGELOG.md` "## 1.9.0 — unreleased", "### Changed" (A) or "### Added" (B); D-138 shared with K-6a (one paragraph).

## K-7a - One table of formats: the lists that copy it read it (TO DECIDE)

* **id**: K-7a
* **goal**: the places that spell a list of formats (`tools/codec/pairs.c:15-17`) takes its names from `cf_formats[]` in `lib/serialize/src/codec.c:320-336`, so that a format added to the table appears in it; and `detect.c`'s order tables (`lib/serialize/src/detect.c:169-176`) are checked against it.
* **why**: the Roadmap's "`detect.c`, Doctor, `codec convert` and the pair test then read one table" is half true: the table has what a format holds and its limits (D-109) and the tools read those, but the pair test (`pairs.c`) and `detect.c` still list their formats by name. The two other lists are not copies of the table: `convert.c:68-81` lists the formats whose files can hold comments (a fact no column of the table records) and `version.c:36-60` lists build features (`lists` and `tlv` are not in the table); both stay.
* **needs**: nothing
* **size**: medium
* **decided**: `detect.c`'s tables are an ordered preference, not a list of formats: they stay, with an assertion in `lib/config/tests/test_detect.c` that each name in them is in the table (a name left out fails the test). `pairs.c`'s formats are those with a nonzero `holds` (`cf_format_holds`, `codec.c:354`), which today are exactly its six (`ini json json5 toml yaml cbor`): a loop over the accessor of the TO DECIDE and that call, in table order.
* **TO DECIDE** (the maintainer): the accessor, a public name. Options: (A) `CF_EXPERIMENTAL(1, 9) const char* cf_format_at(size_t i);`, the format name or `NULL` past the end, in table order, including formats a build left out (as `cf_format_option` knows them, `codec.c:388`). (B) the loop uses `cf_codec_at(i)` (built formats only, `lib/serialize/include/configforge/cf_serialize.h:321`) and the list changes with the build: for the pair test that is not wanted. (C) keep the lists and add a test that checks each against `cf_codec_at`. **Recommendation: A**: one function, experimental as `cf_format_holds` and `cf_format_limits` are; the pair test and the detect test use it. Until answered the card is parked.
* **files**: (if A) `lib/serialize/include/configforge/cf_serialize.h`, `lib/serialize/src/codec.c`, `tools/codec/pairs.c`, `lib/serialize/tests/test_holds.c`, `lib/config/tests/test_detect.c`; last: `docs/Decisions.md` (D-139), `docs/API.md` (generated), `CHANGELOG.md`, `docs/Roadmap.md`.
* **steps**: (if A)
  1. `git switch -c work/K-7a`. In `test_holds.c`: `cf_format_at(i)` runs through every name of `cf_formats[]`, ends in `NULL`, and every name has a non-NULL `cf_format_option`; **see it fail** (no function).
  2. The function.
  3. The `pairs.c` call site; `ctest --preset default`.
  4. The `test_detect.c` assertion.
  5. Format; generators last; ACCEPT.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `gate.abi` green.
* **stop if**: the output of `tools/codec/formats.c` changes (the tools print the table; its text is documented in `docs/Tools.md`).
* **docs**: `CHANGELOG.md` "## 1.9.0 — unreleased", "### Added" (experimental in 1.9); D-139; the Roadmap item stays 🟨 (K-7b).
* **Organiser's answers** (2026-11-26, the maintainer, relayed from a side session: "apply the best recommendations wherever a recommendation will do"): the card's recommendation, adapted to D-220: a number the recommendation fixes as a hard limit is an option whose default is that number.

## K-7b - The ways a format can be read, in its table entry (2.0.0; TO DECIDE)

* **id**: K-7b
* **goal**: each entry of the formats' table says which ways of reading its reader offers (events through a sink, a whole document, in-situ strings, lazy), and the tools read it.
* **why**: the Roadmap's other half of "Formats registered, with what each can do": "the ways it can be read (DOM, events, in-situ, lazy)".
* **needs**: K-7a, and the 2.0.0 card of the ways to read (`docs/Maintaining.md:106`: "the ways to read designed with the stream"), which is not written; nothing in the tree reads in-situ or lazily, so today every reader is an events reader and the only way is "events"
* **size**: small
* **decided**: nothing is built before the ways exist. The bit names, `CF_READS_EVENTS` (every built-in reader, today), `CF_READS_IN_SITU` and `CF_READS_LAZY`, are written by the card that adds a way, and each fills its own bit; this card adds the member `unsigned reads;` to `cf_format_info` (`codec.c:300-307`) and `cf_format_reads(const char*, unsigned*)` after `cf_format_holds` (`codec.c:354`).
* **TO DECIDE** (the maintainer): whether the card is cut at all before 2.0.0. Options: (A) wait: cut it when the ways-to-read card is written, since a bit with one value says nothing; (B) add `CF_READS_EVENTS` now to every entry, with no function, to be extended. **Recommendation: A.** The card is parked at 2.0.0.
* **files**: (when cut) `lib/serialize/src/codec.c`, `lib/serialize/include/configforge/cf_serialize.h`, `tools/codec/formats.c`; last: `docs/Limits.md` (generated), `docs/Decisions.md`, `CHANGELOG.md`, `docs/Roadmap.md`.
* **steps**: written when the card is cut; the first is a table-driven test that each entry's `reads` has `CF_READS_EVENTS`, seen to fail.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI.
* **stop if**: the ways-to-read card changes the shape of a way.
* **docs**: `CHANGELOG.md` "## 2.0.0 — unreleased", "### Added"; the Roadmap item to 🟩.
* **Organiser's answers** (2026-11-26, the maintainer, relayed from a side session: "apply the best recommendations wherever a recommendation will do"): the card's recommendation, adapted to D-220: a number the recommendation fixes as a hard limit is an option whose default is that number.

## K-8 - The YAML reader's three differences from libyaml, fixed to YAML 1.2's text (TO DECIDE, each)

* **id**: K-8
* **goal**: each of the three inputs on which the built-in YAML parser and libyaml differ (`tests/fuzz/yamldiff.known`; `docs/Roadmap.md:1510-1518`; N-12's notes 4 and 5, `git show 70ad1c3^:docs/plan/N-next.md`, lines 302-314) is read as YAML 1.2's text says, and the matching rule of `yamldiff.known` is rewritten to say so or removed.
* **why**: libyaml follows YAML 1.1 in places; the built-in parser is held to 1.2 and to the suite's events, and each of these is a place where the two are one input apart and nobody has said which reading is right.
* **needs**: nothing
* **size**: medium (three small changes, one commit each)
* **decided**: all three are judged by YAML 1.2.2's text. Each part begins with the same step: decode N-12's base64 into a file (`base64 -d`), reduce it by hand to the shortest document that still differs, and run it through both parsers with `codec dump` (the built-in, and the build with `CF_YAML_BACKEND=libyaml`, `docs/Conformance.md:78-85`). The reduced documents go under the card's notes. A recommendation below holds only if the reduced document shows the cause stated; if it shows another, that part's change is not made and the card stops for that part.
  * **K-8.1 A literal block scalar whose last line is only spaces, with no line break after it** (note 4's input). The built-in parser ends the text with a line feed (`lib/serialize/src/yamlparse.c:905`: "the input's end ends the last line too"); libyaml does not. **TO DECIDE.** Options: (A) follow libyaml and the spec's clip rule read literally: the final break is kept only if there is one (YAML 1.2.2 8.1.3, "clip"; `b-chomped-last` is a line feed or the end of input), so no line feed is added at the end of input: `leading_break` is set only when a break was eaten. (B) keep the added line feed (a value that ends in one is what a user expects of a literal block). **Recommendation: A**: the spec's text and libyaml agree. Check that `yaml-values 110` and `yaml 122 94` in `tests/conformance/floor.txt` do not move and that a suite case ending without a newline in a literal, if there is one (the suite's `in.yaml` files that end without a newline: `grep -L` over them), reads as its `in.json` says.
  * **K-8.2 A block scalar's header at the mapping's own column** (note 5's first input: `motd :` and then `|` at column 0). The built-in parser refuses it with "expected a key and ':'" (`yamlparse.c:1309`); libyaml accepts. **TO DECIDE.** Options: (A) keep refusing: the value of a block mapping entry that starts on the next line is indented more than the mapping (`s-l+block-node(n, c)` needs `s-separate(n+1, c)`, YAML 1.2.2 8.2.3 and 6.1), so `|` at the mapping's column is not its value; only the message changes, to "a block scalar's header is not indented more than its key" (`CF_REASON_SYNTAX`), and `yamldiff.known` gets a rule on the side `bi-refuses` (the built-in parser refuses, libyaml accepts), key "a block scalar's header", reason "the built-in parser is right: YAML 1.2.2 8.2.3". (B) accept as libyaml does: rejected, it reads 1.1. **Recommendation: A.**
  * **K-8.3 An anchor followed by a flow indicator, after tabs** (note 5's second input: `&a:-r]:r]:8:` and tabs). The built-in parser refuses it with "a property is followed by white space" (`yamlparse.c:424-426`); libyaml accepts. **TO DECIDE.** Options: (A) keep refusing and correct the message: an anchor's name stops at a flow indicator (`ns-anchor-char` excludes `,[]{}`, YAML 1.2.2 6.9.2), and a property must then be separated from its node by white space, so `]` straight after the name is not a node; the message reads backwards, and becomes "a property is not followed by white space" (a key of `yamldiff.known` that holds the old words, if any, is changed with it: `grep -rn 'property is' tests lib`); the rule goes in `yamldiff.known` on `bi-refuses`, reason "the built-in parser is right: YAML 1.2.2 6.9.2 and 6.9.3, a flow indicator ends an anchor name and no separation follows". (B) accept as libyaml does: rejected. **Recommendation: A.**
  * A part is carried as soon as its own TO DECIDE is answered; the card is parked until at least one is.
* **files**: `lib/serialize/src/yamlparse.c`, `tests/fuzz/yamldiff.known`, `lib/config/tests/test_yaml.c` (a case a part, in the form of its existing cases), `tests/conformance/data/` (the reduced inputs, if that directory takes them: `lib/config/tests/CMakeLists.txt:193-196`); last: `docs/Roadmap.md`, `CHANGELOG.md`, `docs/Conformance.md` (the YAML paragraph, lines 70-85), `docs/Decisions.md`.
* **steps**:
  1. `git switch -c work/K-8`. For each part, with its answer: reduce and run both parsers; the reduced input and both results under the card's notes.
  2. Write the case in `lib/config/tests/test_yaml.c`; build; `ctest --preset default -R test_yaml`; **see it fail** (K-8.1: the value ends in a line feed today; K-8.2 and K-8.3: the message is the old one).
  3. Make the change in `yamlparse.c`; see it pass; `ctest --preset default`; `verify -DEXTRA=conformance` (every YAML floor line holds); `fuzz_yamldiff` 60 seconds with the new `yamldiff.known` rule: no abort.
  4. Commit one a part: `K-8.<n>: YAML <what> read as YAML 1.2 says`.
  5. `docs/Roadmap.md:1510-1518` "Three more differences between the YAML readers" removed (the item is done), with the commit; format; generators last; ACCEPT.
* **accept**: ACCEPT-LINUX with the conformance and `fuzz` extras; ACCEPT-WINDOWS; ACCEPT-CI; the YAML floors in `tests/conformance/floor.txt` unchanged or raised.
* **stop if**: a part's reduced document shows a cause other than the one stated; a suite case that was valid is refused or read differently after a change (revert that part, the case's id under notes).
* **docs**: `CHANGELOG.md` "## 1.9.0 — unreleased", "### Changed" (K-8.1 if A: a literal block scalar at the end of the input without a final line break no longer gains one; K-8.2 and K-8.3: a message) . **D-140** (text, when answered: the three answers, one sentence each, with YAML 1.2.2's sections); until then the three rules of `yamldiff.known` carry the reasons.
* **Organiser's answers** (2026-11-26, the maintainer, relayed from a side session: "apply the best recommendations wherever a recommendation will do"): the recommendation for each part: each follows YAML 1.2's text.

## To be written: K-9a and K-9b (the maintainer, 2026-11-26)

Asked by the maintainer (relayed from a side session: "I like the last
two cards; exception safety is to be added"), version 1.9.0, to be
written as full cards here, read cold, and given index rows after K-8:

* **K-9a - A failure guarantee for every public function.** Each public
  function states one guarantee: nothrow; strong (all or nothing: on an
  error the state is unchanged); basic (no leak, the object still
  valid). Written in the headers and in a table of the documents; a gate
  refuses a public function with no guarantee written. The embedded
  rules: no `abort`, `exit` or `longjmp`; out of memory returns
  `CF_E_MEMORY`.
* **K-9b - Allocation faults injected, as SQLite does.** A test
  allocator fails the 1st, 2nd, ... nth allocation of every load and
  write path in turn; each case is checked for no leak (ASan), no crash,
  and, for a "strong" function, the state unchanged.

* **K-9c - Errors that cannot be ignored** (1.9.0, under D-223): every
  public function that returns a failure is marked `CF_NODISCARD`, and a
  gate refuses one that is not.

Their decisions take the numbers after D-225 (D-224 and D-225 are F-1b's and F-2a's).

## D-223 - One error model (the maintainer, 2026-11-26, relayed from a side session: "I like the error model"; "the core uses the C89 standard")

It answers K-6a's and K-6b's TO DECIDE and Y-28's TO DECIDE C, and K-9c
follows it, so that `cf_error` changes shape once. K-6a, K-6b and Y-28
are rewritten to it before they open; each carries it under
"Organiser's answers". The core stays C89 (D-72): a newer standard's
feature is only a macro that is empty under C89.

1. **Errors cannot be ignored.** Every public function that returns a
   failure (a `cf_bool` or a status; a create, open, try or write) is
   marked `CF_NODISCARD` (`cf_port.h` defines it: `[[nodiscard]]` under
   C23 and C++17, empty under C89; GCC's `warn_unused_result` is not
   used). Today 16 of about 139 carry it: K-9c marks the rest, adds a
   gate that refuses an unmarked one, and CI's C23 build treats the
   warning as an error.
2. **`cf_error` carries** its code, its place (source, file, line,
   column, key), a reason text and a chain of causes ("could not read,
   because permission denied"). A sensitive key's value never enters the
   text. A plain C89 struct; the chain lives in the arena or a linked
   list.
3. **Every format and component returns the same `cf_error`.**
4. **Options with defaults (D-220):** by default the first error stops
   the work and is returned; an option collects every error (the
   compiler's case, Y-28); an option `cf_error_handler`, a function
   pointer, for logging or an embedded indicator.
5. **Held to:** the `CF_NODISCARD` gate; cppcheck and clang-tidy's
   `bugprone-unused-return-value` on the library itself; K-9b's injected
   faults also check that each failure returns the right code.
