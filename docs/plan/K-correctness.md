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
K-5 amends the paragraph of D-109 that says "no one number is exact". The
choices these cards left open were decided on 2026-11-27; D-138
(K-6a and K-6b), D-139 (K-7a) and D-140 (K-8) are kept for them.

## K-1a - `.properties` read by Java's own `Properties`, and the two compared

* **id**: K-1a
* **goal**: `scripts/check/conformance.py` runs `java.util.Properties.load(Reader)` and the library's `.properties` reader over the same small documents, compares the keys and their text (or that both refuse), and keeps a ratchet (`properties-java <read the same> <compared>` in `tests/conformance/floor.txt`) and a list of the differences kept by design, as it does for INI and `configparser`.
* **why**: `.properties` is held to its reader only by unit tests (`docs/Formats.md:17`), so a value that Java reads one way and this library another is found by a user. INI has had this comparison since 1.2 (`scripts/check/conformance.py:587-705`); this is the same for the second format that has a reference reader.
* **needs**: nothing
* **size**: medium
* **version**: 2.2.0
* **status**: open
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
* **version**: 2.2.0
* **status**: open
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
* **version**: 2.2.0
* **status**: open
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
* **version**: 2.2.0
* **status**: open
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
* **version**: 2.2.0
* **status**: open
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
* **version**: 2.2.0
* **status**: open
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
* **version**: 2.2.0
* **status**: open
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
* **version**: 2.2.0
* **status**: open
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
* **version**: 2.2.0
* **status**: open
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
* **version**: 2.2.0
* **status**: open
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
* **version**: 2.2.0
* **status**: open
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
* **version**: 2.2.0
* **status**: open
* **decided**: as K-4a, with: the other reader is tinycbor (`bench/compare/libs/tinycbor.cmake`), `cbor_parser_init` and `cbor_value_validate` in strict mode first (valid UTF-8 in text strings, no trailing bytes: `bench/compare/adapters/tinycbor.cpp:1-5`), then the walk of `adapters/tinycbor.cpp:14-60` writing lines. Kinds: integers within int64 `int`; text strings `text`; floats `real` (`%.17g`, a half float widened). Byte strings, tags and simple values other than true, false and null are refused by ConfigForge by name (the model has no such kind, D-91): tinycbor's walker stops with "unsupported" on them too, and a document that reaches one on either side is not compared. A map key that is not text is the same. A duplicate key is not checked by tinycbor: the rule `cf-refuses` on "duplicate" is expected, with the reason "ConfigForge is right: a map with a repeated key is ambiguous (RFC 8949 5.6 lets a decoder refuse it) and a configuration needs one value". Seeds: the Appendix A vectors under `tests/conformance/data` when present, else the bytes `a1 61 61 01` (`{"a": 1}`).
* **files**: `tests/fuzz/cbordiff.c`, `tests/fuzz/cbordiff.known` (new); `tests/fuzz/CMakeLists.txt`, `.github/workflows/ci.yml`; last: `docs/Safety.md`, `CHANGELOG.md`, `docs/Roadmap.md`.
* **steps**: as K-4b.
* **accept**: as K-4a.
* **stop if**: as K-4a.
* **docs**: `CHANGELOG.md` "## 1.9.0 — unreleased", "### Added": `fuzz_cbordiff`.

## K-4d - XML against a second parser

* **id**: K-4d
* **goal**: XML's differential target is answered: either built against a second parser, or the Roadmap item's "XML" closed with the reason.
* **why**: ConfigForge's XML reader sits on expat (`lib/serialize/src/xml.c:1`, and `bench/compare/libraries.cmake:14-16` says the comparison links the same expat), so expat on the other side compares a library with itself; what can differ is only this reader's own rules (the key mapping, D-23).
* **needs**: K-4a
* **size**: medium
* **version**: 2.2.0
* **status**: open
* **decided**: as K-4a: the lines `<dotted key>\t<kind>\t<text>` are made from the second parser's events by D-23's mapping (written once in the target, from `lib/serialize/src/xml.c:3-24`), the sides `lib-refuses`, `cf-refuses`, `values`, and the `diffs` job gets `fuzz_xmldiff`; what is only well-formedness is compared as accept or refuse alone.
  * **The second parser** (decided 2026-11-27): (B), libxml2's SAX2 interface at its newest release at least 30 days old, pinned and fetched like the others (`bench/compare/libraries.cmake`: a commit, its date, a licence), used for accept or refuse of well-formedness only (its DOCTYPE and namespace handling differ from what this reader refuses by name, so values are not compared; values stay with D-23's tests). It is an optional dependency, OFF by default: fetched only by `scripts/bench/fetch.cmake` for the `diffs` job and linked only into `fuzz_xmldiff`, never into the library, the tools or a default build. Rejected: (A) closing XML by reason (expat and libxml2 have disagreed in places that were CVEs in both, and a well-formedness difference is the confusion the Roadmap names); (C) tinyxml2 or pugixml (C++: the target would be C++, for no reading the C parser lacks).
* **files**: `tests/fuzz/xmldiff.c`, `tests/fuzz/xmldiff.known` (new); `scripts/bench/fetch.cmake` and `bench/compare/libraries.cmake` (a `libxml2` entry, used by the fuzz job only), `tests/fuzz/CMakeLists.txt`, `.github/workflows/ci.yml`; last: `docs/Dependencies.md`, `CHANGELOG.md`, `docs/Roadmap.md`.
* **steps**: as K-4a steps 3-6, plus `docs/Dependencies.md`: a row (grep its table for the form) naming libxml2 as a fuzz-job dependency only; Roadmap "Differential fuzzing against the other readers" to 🟩 with "JSON, TOML, XML (well-formedness), CBOR and YAML".
* **accept**: as K-4a.
* **stop if**: as K-4a; the library's licence, or its build, needs more than a C compiler and its own sources.
* **docs**: `CHANGELOG.md` "## 1.9.0 — unreleased", "### Added".
* **Organiser's answers** (2026-11-26, the maintainer, relayed from a side session: "apply the best recommendations wherever a recommendation will do"): the card's recommendation, adapted to D-220: a number the recommendation fixes as a hard limit is an option whose default is that number. The second parser: libxml2, optional and OFF by default, in the `diffs` job only (the maintainer's standing approval, 2026-11-27).

## K-5a - TOML's joined path judged as one number

* **id**: K-5a
* **goal**: TOML's reader refuses a key line whose header path and dotted key together are longer than `CF_KEY_MAX - 2` bytes, with the wording of a header too long, and `cf_format_limits("toml")` is exact: `test_holds` reads every tested split of a path at the limit and refuses one byte more.
* **why**: today a header path and a dotted key are each limited to `CF_KEY_MAX` separately (`lib/serialize/src/toml.c:125`, `151`, `412-448`, `1359`), so "no one number is exact" (`docs/Decisions.md:4885-4890`); the pair test found a path the writer wrote and the reader refused.
* **needs**: nothing
* **size**: medium
* **version**: 2.2.0
* **status**: open
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
* **version**: 2.2.0
* **status**: open
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

## K-6a - A refusal's byte offset

* **id**: K-6a
* **goal**: a refusal can say the byte offset it happened at as a number a program reads, in every reader that has one, as it says line, column and reason.
* **why**: D-107 named the byte offset and did not build it (`docs/Decisions.md:4767-4770`: "stays in the message"); CBOR and TLV have no line and keep their place only in the message text (`lib/serialize/src/cbor.c:92`, `160`, `172`: "byte %zu"), so a test of "the same bad input refused the same way" cannot compare it and a program cannot show where.
* **needs**: nothing
* **size**: medium
* **version**: 2.0.0
* **status**: open
* **decided**: (D-223, the one error model, which `cf_error` takes once: K-9c adds the chain, this card adds the offset) every codec reports through `cf_text_fail` (`lib/text/src/fail.c`) or `cf_error_place` (`lib/base/include/configforge/cf_error.h:123-124`) with a line and a column; the offset is added at the same call sites, as `(size_t)(s->p - in->p)` where the scan has both pointers (CBOR and TLV: their own `off`). **Compatibility**: additive through `size` (D-51; `cf_error.h:51-62`), as `where` and `reason` were added in 1.8.
* **Decided** (the shape of a new public member of `cf_error`; D-223 point 2 gives `cf_error` its place, so the offset joins it once): `size_t offset;` after `reason`, `(size_t)-1` when unknown, filled through a new `CF_EXPERIMENTAL(1, 9) void cf_error_place_at(cf_error*, int reason, unsigned line, unsigned col, size_t offset)` and read directly (`CF_ERROR_INIT`, `cf_error.h:73`, gains a member; it is fenced from the formatter for that reason). Experimental in 1.9 as N-13's `cf_reason_severity` is: one function and one member; `gate.abi` gets the function. **Rejected**: the offset inside `cf_loc` (every `cf_loc` grows, an ABI change in 1.x); `cf_error_offset(const cf_error*)` parsing the message (a message is not an interface).
* **files**: `lib/base/include/configforge/cf_error.h`, `lib/base/src/error.c`, `lib/text/src/fail.c`, `lib/serialize/src/cbor.c`, `lib/serialize/src/tlv.c`, `lib/serialize/src/{json,toml,yamlparse,ini,xml,properties}.c` (each call site that has a pointer into the input), `lib/config/tests/test_reason.c`; last: `docs/Decisions.md` (D-138), `docs/API.md` and `docs/Codemap.md` (generated), `CHANGELOG.md`, `docs/Roadmap.md`.
* **steps**:
  1. `git switch -c work/K-6a`.
  2. In `test_reason.c`, to each row of `rows` (the refusing documents; `test_rows`, line 434, loops over them) add the offset its document must report, computed by hand (the index of the byte that is the cause); **see it fail** (the member does not exist: a compile error is the first failure, then every offset is `(size_t)-1`).
  3. Add the member and the function; `cf_text_fail` takes the offset from its caller's pointers; CBOR and TLV pass their `off`.
  4. `ctest --preset default` and `--preset sanitize`.
  5. `docs/Decisions.md` D-138: the offset as decided, with D-107's "byte offset" now built.
  6. Format; generators last; ACCEPT.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `gate.abi` and `gate.apiref` green.
* **stop if**: a reader refuses from a point it has no pointer for (a refusal after the document, from the sink): its offset is `(size_t)-1` and `test_reason.c` says so for that row; more than three readers cannot give one.
* **docs**: `CHANGELOG.md` "## 1.9.0 — unreleased", "### Added" (experimental in 1.9); D-138; the Roadmap item "One diagnostic model across formats" stays 🟨 until K-6b.

* **Organiser's answers**:
  * 2026-11-27, the organiser: K-6a follows D-223 (one error model): option A of the recommendation, the offset a member of `cf_error` beside the place D-223 point 2 names (the maintainer's standing approval, 2026-11-27).

## K-6b - A refusal's reason shown by the tools

* **id**: K-6b
* **goal**: `configforge codec dump`, `recode` and `detect`, when they refuse a file, show the reason name, its severity and its weakness, so that a user filters by reason and not by the words of a message.
* **why**: every refusal has a reason (A-10) and each reason its severity and CWE (N-13), but the tools print only `err.message` (`tools/codec/dump.c:312`, `369`, `tools/codec/recode.c:172`, `tools/codec/detect.c:240`), so the model is not what a user sees.
* **needs**: nothing
* **size**: small
* **version**: 2.0.0
* **status**: open
* **decided**: the text, one line after the message, on stderr (`tools/codec/detect.c:240` prints its message on stdout; its reason line goes to stderr too, so that no stdout changes): `  reason: <cf_reason_name> (<cf_reason_severity>[, <cf_reason_weakness>])`, for instance `  reason: depth (warning, CWE-674)`; printed only when `err.reason != CF_REASON_NONE`. One helper in `tools/codec/codec.c` (`codec_report(const cf_error*)`, declared in `tools/codec/codec.h`), called by the four sites. A case in `tests/front/front.cmake`, inside `if(HAVE_CODEC)` (line 123): it writes a JSON file of 40 nested `{"a":` with `file(WRITE ...)` under `${WORK}` (as line 135 does) and runs `front_case(<the exit code `codec dump` returns on a refusal, read from `tools/codec/dump.c`> - "reason: depth" codec dump <file>)`.
* **Decided** (whether the line is printed always or only with a flag): always. stderr's text is not a documented interface (`docs/Tools.md` documents exit codes and stdout) and a second line is read by a person; the project treats the message text as free (D-107: a user filters by code and not by message text is the point, D-223 point 2: the code, the place and the reason are the error), and a flag nobody passes defeats the aim. **Rejected**: only with `--reason` (a flag nobody passes).
* **files**: `tools/codec/codec.c`, `tools/codec/codec.h`, `tools/codec/dump.c`, `tools/codec/recode.c`, `tools/codec/detect.c`, `tests/front/front.cmake`; last: `docs/Tools.md`, `CHANGELOG.md`, `docs/Roadmap.md`.
* **steps**:
  1. `git switch -c work/K-6b`. Write the front-door test first; **see it fail**.
  2. The helper and the four call sites.
  3. `docs/Tools.md`: the codec section's paragraph on refusals names the line.
  4. Roadmap "One diagnostic model across formats" to 🟩 (K-6a, K-6b; A-10 and N-13 earlier). Format; generators last; ACCEPT.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI.
* **stop if**: a test in `tests/` matches stderr exactly and fails (report it and change that test to match the new line: the line is on stderr only).
* **docs**: `CHANGELOG.md` "## 1.9.0 — unreleased", "### Changed"; D-138 shared with K-6a (one paragraph).

* **Organiser's answers**:
  * 2026-11-27, the organiser: K-6b follows D-223 (one error model): the reason line is always printed, on stderr (the maintainer's standing approval, 2026-11-27).
  * 2026-11-27, the worker: 1.9.0 is released, so the CHANGELOG entry opens "## 2.0.0 — unreleased"; D-138 does not exist in docs/Decisions.md and is left to K-6a; the line is printed by `check`, `dump`, `textconv` (on a refusal) and `recode` (the maintainer's standing approval, 2026-11-27).

## K-7a - One table of formats: the lists that copy it read it

* **id**: K-7a
* **goal**: the places that spell a list of formats (`tools/codec/pairs.c:15-17`) takes its names from `cf_formats[]` in `lib/serialize/src/codec.c:320-336`, so that a format added to the table appears in it; and `detect.c`'s order tables (`lib/serialize/src/detect.c:169-176`) are checked against it.
* **why**: the Roadmap's "`detect.c`, Doctor, `codec convert` and the pair test then read one table" is half true: the table has what a format holds and its limits (D-109) and the tools read those, but the pair test (`pairs.c`) and `detect.c` still list their formats by name. The two other lists are not copies of the table: `convert.c:68-81` lists the formats whose files can hold comments (a fact no column of the table records) and `version.c:36-60` lists build features (`lists` and `tlv` are not in the table); both stay.
* **needs**: nothing
* **size**: medium
* **version**: 2.2.0
* **status**: open
* **decided**: `detect.c`'s tables are an ordered preference, not a list of formats: they stay, with an assertion in `lib/config/tests/test_detect.c` that each name in them is in the table (a name left out fails the test). `pairs.c`'s formats are those with a nonzero `holds` (`cf_format_holds`, `codec.c:354`), which today are exactly its six (`ini json json5 toml yaml cbor`): a loop over `cf_format_at` (below) and that call, in table order.
  * **The accessor** (decided 2026-11-27): `CF_EXPERIMENTAL(1, 9) const char* cf_format_at(size_t i);`, the format name or `NULL` past the end, in table order, including formats a build left out (as `cf_format_option` knows them, `codec.c:388`): one function, experimental as `cf_format_holds` and `cf_format_limits` are; the pair test and the detect test use it. Rejected: (B) a loop over `cf_codec_at(i)` (built formats only, `lib/serialize/include/configforge/cf_serialize.h:321`: the list would change with the build, which the pair test must not); (C) keeping the lists with a test against `cf_codec_at` (the copies stay, with the same build dependence).
* **files**: `lib/serialize/include/configforge/cf_serialize.h`, `lib/serialize/src/codec.c`, `tools/codec/pairs.c`, `lib/serialize/tests/test_holds.c`, `lib/config/tests/test_detect.c`; last: `docs/Decisions.md` (D-139), `docs/API.md` (generated), `CHANGELOG.md`, `docs/Roadmap.md`.
* **steps**:
  1. `git switch -c work/K-7a`. In `test_holds.c`: `cf_format_at(i)` runs through every name of `cf_formats[]`, ends in `NULL`, and every name has a non-NULL `cf_format_option`; **see it fail** (no function).
  2. The function.
  3. The `pairs.c` call site; `ctest --preset default`.
  4. The `test_detect.c` assertion.
  5. Format; generators last; ACCEPT.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `gate.abi` green.
* **stop if**: the output of `tools/codec/formats.c` changes (the tools print the table; its text is documented in `docs/Tools.md`).
* **docs**: `CHANGELOG.md` "## 1.9.0 — unreleased", "### Added" (experimental in 1.9); D-139; the Roadmap item stays 🟨 (K-7b).
* **Organiser's answers** (2026-11-26, the maintainer, relayed from a side session: "apply the best recommendations wherever a recommendation will do"): the card's recommendation, adapted to D-220: a number the recommendation fixes as a hard limit is an option whose default is that number. The accessor: `cf_format_at` (the maintainer's standing approval, 2026-11-27).

## K-7b - The ways a format can be read, in its table entry

* **id**: K-7b
* **goal**: each entry of the formats' table says which ways of reading its reader offers (events through a sink, a stream, a whole document, lazy, in-situ strings), `cf_format_reads` returns them, and the tools read it.
* **why**: the Roadmap's other half of "Formats registered, with what each can do": "the ways it can be read (DOM, events, in-situ, lazy)". A bit with one value says nothing, so the card is cut when the ways exist (M-14, M-15, M-16), after the 2.0.0 breaks and with module 6's additions.
* **needs**: K-7a, M-14, M-15, M-16
* **size**: small
* **version**: 2.4.0
* **status**: open
* **decided**: the member `unsigned reads;` is added to `cf_format_info` (`lib/serialize/src/codec.c:300-307`), after `max_depth`, and `CF_NODISCARD CF_EXPERIMENTAL(2, 4) cf_bool cf_format_reads(const char* format, unsigned* reads);` after `cf_format_holds` (`lib/serialize/src/codec.c:354`; declared in `lib/serialize/include/configforge/cf_serialize.h` beside `cf_format_holds`, line 305), with the same contract (`CF_TRUE` and `*reads` for a format of the table, whether or not this build has its codec; `CF_FALSE`, `*reads` untouched, for a name it does not know). The bits, an enum beside `CF_HOLDS_*`: `CF_READS_EVENTS = 1u << 0` (every built-in reader), `CF_READS_STREAM = 1u << 1` (the codec has a native `decode_stream`: M-6 to M-9; the fallback that feeds a whole input does not count), `CF_READS_DOM = 1u << 2` (every codec: M-14 reads through the events), `CF_READS_LAZY = 1u << 3` (the codecs M-15 implements: `grep -n lazy lib/serialize/src/*.c`), `CF_READS_IN_SITU = 1u << 4` (the codecs M-16 implements: `grep -n in_situ lib/serialize/src/*.c`). Each entry is written from what those cards built, not from a guess: a test in `lib/serialize/tests/test_holds.c` asserts, for every format of the table whose codec the build has, that `reads` has exactly the bits the codec's members and the cards' entry points show (a native `decode_stream` set, a lazy or in-situ entry point present), so the table cannot drift from the code. `configforge codec formats` (`tools/codec/formats.c`) prints a `reads` column and its page (`docs/Limits.md`, generated) carries it; M-17's `docs/Formats.md` table is made from `cf_format_reads`, not from an array of its own. Rejected: adding `CF_READS_EVENTS` alone before the ways exist (a bit with one value), a function per way.
* **files**: `lib/serialize/src/codec.c`, `lib/serialize/include/configforge/cf_serialize.h`, `lib/serialize/tests/test_holds.c`, `tools/codec/formats.c`; last: `docs/Limits.md` (generated), `docs/Decisions.md`, `docs/API.md` (generated), `CHANGELOG.md`, `docs/Roadmap.md`.
* **steps**:
  1. `git switch -c work/K-7b main`. In `test_holds.c`: for each format of the table, `cf_format_reads` is true, has `CF_READS_EVENTS` and `CF_READS_DOM`, and its stream, lazy and in-situ bits equal what the codec offers; an unknown name returns `CF_FALSE`; **see it fail** (no function).
  2. The member, the bits and the function; fill each entry.
  3. The `reads` column of `tools/codec/formats.c`; `ctest --preset default` (the page test holds `docs/Limits.md` to the tool: regenerate it with the command its header names).
  4. Format; generators last; ACCEPT.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `gate.abi` green.
* **stop if**: the output of `tools/codec/formats.c` changes in a column that is not the new one; a way of M-14, M-15 or M-16 changed its shape after this card was written.
* **docs**: `CHANGELOG.md` "## 2.4.0 — unreleased", "### Added" (experimental in 2.4); D-139 (K-7a's) gets a sentence for `reads`; the Roadmap item "Formats registered, with what each can do" to 🟩.
* **Organiser's answers** (2026-11-26, the maintainer, relayed from a side session: "apply the best recommendations wherever a recommendation will do"): the card's recommendation, adapted to D-220: a number the recommendation fixes as a hard limit is an option whose default is that number. (2026-11-27, the organiser, under the maintainer's standing approval): the recommendation, wait for the ways to read, holds, so the card leaves 2.0.0 for 2.4.0, after M-14, M-15 and M-16 (module 6's additions); it needs K-7a (2.2.0); M-17 reads its table instead of making one (the maintainer's standing approval, 2026-11-27).

## K-8 - The YAML reader's three differences from libyaml, fixed to YAML 1.2's text

* **id**: K-8
* **goal**: each of the three inputs on which the built-in YAML parser and libyaml differ (`tests/fuzz/yamldiff.known`; `docs/Roadmap.md:1510-1518`; N-12's notes 4 and 5, `git show 70ad1c3^:docs/plan/N-next.md`, lines 302-314) is read as YAML 1.2's text says, and the matching rule of `yamldiff.known` is rewritten to say so or removed.
* **why**: libyaml follows YAML 1.1 in places; the built-in parser is held to 1.2 and to the suite's events, and each of these is a place where the two are one input apart and nobody has said which reading is right.
* **needs**: nothing
* **size**: medium (three small changes, one commit each)
* **version**: 2.2.0
* **status**: open
* **decided**: all three are judged by YAML 1.2.2's text. Each part begins with the same step: decode N-12's base64 into a file (`base64 -d`), reduce it by hand to the shortest document that still differs, and run it through both parsers with `codec dump` (the built-in, and the build with `CF_YAML_BACKEND=libyaml`, `docs/Conformance.md:78-85`). The reduced documents go under the card's notes. A recommendation below holds only if the reduced document shows the cause stated; if it shows another, that part's change is not made and the card stops for that part.
  * **K-8.1 A literal block scalar whose last line is only spaces, with no line break after it** (note 4's input). The built-in parser ends the text with a line feed (`lib/serialize/src/yamlparse.c:905`: "the input's end ends the last line too"); libyaml does not. **Decided (2026-11-27): (A).** Options were: (A) follow libyaml and the spec's clip rule read literally: the final break is kept only if there is one (YAML 1.2.2 8.1.3, "clip"; `b-chomped-last` is a line feed or the end of input), so no line feed is added at the end of input: `leading_break` is set only when a break was eaten. (B) keep the added line feed (a value that ends in one is what a user expects of a literal block). Chosen because the spec's text and libyaml agree. Rejected: (B), a line feed the input does not have is not in YAML 1.2.2's clip rule. Check that `yaml-values 110` and `yaml 122 94` in `tests/conformance/floor.txt` do not move and that a suite case ending without a newline in a literal, if there is one (the suite's `in.yaml` files that end without a newline: `grep -L` over them), reads as its `in.json` says.
  * **K-8.2 A block scalar's header at the mapping's own column** (note 5's first input: `motd :` and then `|` at column 0). The built-in parser refuses it with "expected a key and ':'" (`yamlparse.c:1309`); libyaml accepts. **Decided (2026-11-27): (A).** Options were: (A) keep refusing: the value of a block mapping entry that starts on the next line is indented more than the mapping (`s-l+block-node(n, c)` needs `s-separate(n+1, c)`, YAML 1.2.2 8.2.3 and 6.1), so `|` at the mapping's column is not its value; only the message changes, to "a block scalar's header is not indented more than its key" (`CF_REASON_SYNTAX`), and `yamldiff.known` gets a rule on the side `bi-refuses` (the built-in parser refuses, libyaml accepts), key "a block scalar's header", reason "the built-in parser is right: YAML 1.2.2 8.2.3". (B) accept as libyaml does: rejected, it reads 1.1. Rejected: (B), it reads YAML 1.1.
  * **K-8.3 An anchor followed by a flow indicator, after tabs** (note 5's second input: `&a:-r]:r]:8:` and tabs). The built-in parser refuses it with "a property is followed by white space" (`yamlparse.c:424-426`); libyaml accepts. **Decided (2026-11-27): (A).** Options were: (A) keep refusing and correct the message: an anchor's name stops at a flow indicator (`ns-anchor-char` excludes `,[]{}`, YAML 1.2.2 6.9.2), and a property must then be separated from its node by white space, so `]` straight after the name is not a node; the message reads backwards, and becomes "a property is not followed by white space" (a key of `yamldiff.known` that holds the old words, if any, is changed with it: `grep -rn 'property is' tests lib`); the rule goes in `yamldiff.known` on `bi-refuses`, reason "the built-in parser is right: YAML 1.2.2 6.9.2 and 6.9.3, a flow indicator ends an anchor name and no separation follows". (B) accept as libyaml does: rejected, libyaml's reading is not YAML 1.2.2's.
  * Each part is carried and committed on its own (step 4).
* **files**: `lib/serialize/src/yamlparse.c`, `tests/fuzz/yamldiff.known`, `lib/config/tests/test_yaml.c` (a case a part, in the form of its existing cases), `tests/conformance/data/` (the reduced inputs, if that directory takes them: `lib/config/tests/CMakeLists.txt:193-196`); last: `docs/Roadmap.md`, `CHANGELOG.md`, `docs/Conformance.md` (the YAML paragraph, lines 70-85), `docs/Decisions.md`.
* **steps**:
  1. `git switch -c work/K-8`. For each part, as decided: reduce and run both parsers; the reduced input and both results under the card's notes.
  2. Write the case in `lib/config/tests/test_yaml.c`; build; `ctest --preset default -R test_yaml`; **see it fail** (K-8.1: the value ends in a line feed today; K-8.2 and K-8.3: the message is the old one).
  3. Make the change in `yamlparse.c`; see it pass; `ctest --preset default`; `verify -DEXTRA=conformance` (every YAML floor line holds); `fuzz_yamldiff` 60 seconds with the new `yamldiff.known` rule: no abort.
  4. Commit one a part: `K-8.<n>: YAML <what> read as YAML 1.2 says`.
  5. `docs/Roadmap.md:1510-1518` "Three more differences between the YAML readers" removed (the item is done), with the commit; format; generators last; ACCEPT.
* **accept**: ACCEPT-LINUX with the conformance and `fuzz` extras; ACCEPT-WINDOWS; ACCEPT-CI; the YAML floors in `tests/conformance/floor.txt` unchanged or raised.
* **stop if**: a part's reduced document shows a cause other than the one stated; a suite case that was valid is refused or read differently after a change (revert that part, the case's id under notes).
* **docs**: `CHANGELOG.md` "## 1.9.0 — unreleased", "### Changed" (K-8.1: a literal block scalar at the end of the input without a final line break no longer gains one; K-8.2 and K-8.3: a message) . **D-140** (text: the three decisions, one sentence each, with YAML 1.2.2's sections); the three rules of `yamldiff.known` carry the reasons.
* **Organiser's answers** (2026-11-26, the maintainer, relayed from a side session: "apply the best recommendations wherever a recommendation will do"): the recommendation for each part: each follows YAML 1.2's text. Each part decided (A) (the maintainer's standing approval, 2026-11-27).

## K-9a - Three failure guarantees: written in every public function's header comment, listed in a table, and held by `gate.guarantees`

* **id**: K-9a
* **goal**: `docs/Guarantees.md` defines three failure guarantees (`nothrow`, `strong`, `basic`); `scripts/check/guarantees.py` and `gate.guarantees` hold that each public function's header comment states one (the names K-9d to K-9g have not annotated yet are listed in `scripts/check/guarantees_todo.txt`, and that list only shrinks), that the table in that document is what the headers say, that no `CF_NODISCARD` function claims `nothrow`, and that no library source ends the program (`abort`, `exit`, `longjmp` and their kin).
* **why**: the maintainer's embedded rules (2026-11-26, "exception safety is to be added"): a library inside a controller or a daemon must say what is left when a call fails, and must never decide to stop the process. Today no header says it (`grep -rn "Guarantee:" lib/*/include` finds nothing at 18557b9) for any of the 309 public names, and the second rule holds by luck only: `grep -rnE "\b(abort|exit|longjmp|setjmp|assert)\s*\(" lib --include=*.c --include=*.h` finds no call outside `lib/*/tests`, and nothing stops the first one.
* **needs**: nothing
* **size**: medium
* **version**: 2.0.0
* **status**: open
* **decided**:
  * **The words** (written into `docs/Guarantees.md` by this card, as the first section). A *failure* is a call that returns `CF_FALSE`, `NULL` or a code other than `CF_OK`, or that records one where it returns nothing (a builder holds its first failure until `cf_builder_create`; a sink holds its overflow).
    * `nothrow`: it cannot fail. No allocation, no input or output; a NULL or an invalid argument gets the fallback its comment states. Destroying calls (`_free`, `_close`, `_destroy`, `_abort`), accessors and queries with a fallback (`cf_get_int`), and formatting into a caller's buffer that cuts the text (`cf_fmt`) are `nothrow`.
    * `strong`: it may fail; when it does, every object reachable from its arguments is as it was before the call. Not counted as state: the `cf_error` that holds the failure, and an output buffer or output struct of the caller's (its contents are then unspecified, NUL-terminated where the comment says so). A call that builds its result apart and publishes it at the end is `strong`: `cf_derive`, `cf_write`, `cf_edit_commit`, `cf_file_replace` (the target file is unchanged), `cf_live_reload` (the current snapshot is still the one served).
    * `basic`: it may fail; when it does, nothing leaks and every object stays valid (it can be used, freed, or the call tried again), but its contents may have changed, and the comment says which. A `cf_builder_*` call that holds its failure for `cf_builder_create`, a `cf_sink_*` write that overflows, `cf_file_writer_write` (some bytes were written).
    * **Choosing**, in this order: no failure path is `nothrow`; a failure before anything of the arguments is touched, or touched only after the last point a failure can happen, is `strong`; otherwise `basic`. A function that can leave an object invalid or leak is a defect, never a fourth word: it is fixed (its own commit, a test first) or written under the annotating card's notes.
  * **The line.** In the comment directly above a declaration, or above a run of declarations with no blank line between them (the comment then covers the run), one line that starts `Guarantee: ` and a word, then a full stop, then optionally more words on the same line: `Guarantee: strong.` or `Guarantee: basic: the sink stays valid; its text is cut where it overflowed.` Where the members of a run differ, they are separate runs. `scripts/generate/apiref.cmake` copies the comment above each declaration into `docs/API.md`, so the word reaches the API reference with no change to the generator.
  * **The names.** A public function is a name on a non-comment line of `cmake/configforge.symbols` or `cmake/configforge.experimental.symbols` (309 at 18557b9); `gate.abi` already holds that each is declared `CF_API` in a header of `lib/*/include/configforge/`. The internal list (`cmake/configforge.internal.symbols`) is outside the promise and outside this gate.
  * **The gate**: `scripts/check/guarantees.py <root>` (Python 3 standard library; `--write` rewrites the table; `--baseline` rewrites the to-do list from the tree; `--todo <path fragment>` prints the names of the headers whose path holds the fragment that have no guarantee yet), exit 1 and one line each for: `<header>:<line>: <name>: no guarantee`; `<header>:<line>: <name>: '<word>' is not nothrow, strong or basic`; `<header>:<line>: <name>: CF_NODISCARD and nothrow` (a function that can report a failure is not `nothrow`); `scripts/check/guarantees_todo.txt: <name>: has a guarantee, remove it from the list`; `scripts/check/guarantees_todo.txt: <name>: not a public name`; `docs/Guarantees.md: the table is not what the headers say: run scripts/check/guarantees.py --write`. A name in the to-do list is not reported as lacking a guarantee. `cmake/gates/GateGuarantees.cmake` as `cmake/gates/GateNodiscard.cmake`, registered by adding `Guarantees` to the `foreach` of gates that read only the tree in `cmake/ConfigForgeGates.cmake` (that function is at cmake-lint's statement limit, as K-11a found).
  * **The to-do list**: `scripts/check/guarantees_todo.txt`, one name a line, `#` lines ignored, written by `--baseline` with every public name that has no guarantee (309 when this card lands, less the three it annotates). K-9d to K-9g each annotate a set of headers and rewrite it; the last of them deletes the file, and a gate that finds no file reads an empty list.
  * **The table**: `docs/Guarantees.md` has a hand-written first part (the words above, the two embedded rules, how a function's line is read) and, between `<!-- guarantees:begin -->` and `<!-- guarantees:end -->`, a generated Markdown table `| Function | Header | Guarantee |` sorted by name (a name still in the to-do list reads `not yet`), then one line `nothrow <n>, strong <n>, basic <n>, not yet <n>`. It is added to `mkdocs.yml` under "Design", after "Limits".
  * **Three names annotated here, so that the format is proved on real headers**: `cf_error_clear` (`nothrow`), `cf_arena_new` (`strong`: nothing exists to change) and `cf_arena_free` (`nothrow`), in `lib/base/include/configforge/cf_error.h` and `lib/base/include/configforge/cf_alloc.h`. K-9d skips them.
  * **The embedded rules**: (1) no call to `abort`, `exit`, `_Exit`, `quick_exit`, `longjmp`, `siglongjmp`, `setjmp`, `sigsetjmp`, `assert`, `raise` or `__builtin_trap` in a file of `lib/*/src` or `lib/*/include` (a regular expression on the file with comments and string literals removed: the name followed by `(`); `scripts/check/nonlocal.txt`, one `path|token|reason` a line, is the list of exceptions, empty at the start; the same script reports `<file>:<line>: <token>: the library never ends the program`; (2) an allocation that fails returns `CF_E_MEMORY`, never another code and never a crash: that is held at run time by K-9b, and written in the document's embedded section.
  * **D-number**: the next free one, "Every public function states one failure guarantee; the library never ends the program". Options: a macro on each declaration (rejected: it changes 309 lines and the compiler checks nothing by it); the words in prose only (rejected: a promise nothing reads drifts); a comment line a gate reads and a table made from it (decided).
  * **Rejected**: a fourth word for "may leave the object unusable" (it is a defect); guarantees for the internal names (outside the promise); one word per header (a header mixes `nothrow` accessors and failing calls).
* **files**: `scripts/check/guarantees.py` (new), `scripts/check/guarantees_todo.txt` (new, written by the script), `scripts/check/nonlocal.txt` (new, comment lines only), `cmake/gates/GateGuarantees.cmake` (new), `cmake/ConfigForgeGates.cmake`, `docs/Guarantees.md` (new), `mkdocs.yml`, `lib/base/include/configforge/cf_error.h`, `lib/base/include/configforge/cf_alloc.h`, then `CHANGELOG.md`, `docs/Decisions.md`, `docs/Codemap.md` and `docs/API.md` (generated).
* **steps**:
  1. `git switch -c work/K-9a main`. Write `guarantees.py` (the checks, `--write`, `--baseline`, `--todo`), `docs/Guarantees.md` with the first part and empty markers, `GateGuarantees.cmake`, the registration.
  2. See it fail: `ctest --preset default -R gate.guarantees --output-on-failure | tail -n 8` lists the 309 names as `no guarantee` (the to-do file is not there yet).
  3. `python scripts/check/guarantees.py --baseline .` then `--write .`; `ctest --preset default -R gate.guarantees` exit 0.
  4. Annotate the three names; run the gate (it says to remove them from the list); `--baseline` and `--write` again; exit 0.
  5. Negative proofs, in a scratch copy of the tree, each refused and then discarded: a public name added to `configforge.symbols` and its declaration with no line; `Guarantee: firm.` (a word that is not one of the three); `CF_NODISCARD` on a `nothrow` declaration; `abort();` added to a file of `lib/base/src`; a changed row of the table.
  6. `mkdocs.yml`, the document's first part read once more against the words above, `CHANGELOG.md`, the D-number. Format; codemap and apiref last; ACCEPT; commit `K-9a: three failure guarantees, a table and gate.guarantees`.
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI; `ctest --preset default -R "gate.guarantees|gate.apiref|gate.abi|gate.index"` exit 0; every existing test unchanged.
* **stop if**: the nonlocal check finds a call in the library (report the file and line and add no exception: a library that ends the process is a defect for its own card); a name of `configforge.symbols` has no declaration the script can find (`gate.abi` should have refused it: report); `gate.apiref` differs for a reason other than the three comments.
* **docs**: `CHANGELOG.md`, "## 2.0.0 — unreleased" (the heading is added above the newest one when it is not there), "### Added": "`docs/Guarantees.md` and `gate.guarantees`: every public function states one failure guarantee (nothrow, strong or basic) in its header comment, and no library source ends the program (K-9a)"; `docs/Decisions.md`: the D-number above, with its row.

## K-9d - Guarantees written for the lower components: base, port, platform, text and serialize

* **id**: K-9d
* **goal**: every public name declared in `lib/base`, `lib/port`, `lib/platform`, `lib/text` and `lib/serialize` has its guarantee (K-9a's words) in its header comment, and leaves `scripts/check/guarantees_todo.txt`.
* **why**: K-9a's gate lets a name stay on its to-do list; each name leaves it only when someone has read the function and said what a failure leaves behind. These five components hold the allocator, the error, the value, the file layer and the readers: the ground every other guarantee stands on.
* **needs**: K-9a
* **size**: medium
* **version**: 2.0.0
* **status**: open
* **decided**:
  * **The set**, 90 names at 18557b9 (the worker gets the live list from `python scripts/check/guarantees.py --todo lib/base`, then `lib/port`, `lib/platform`, `lib/text`, `lib/serialize`): `cf_alloc.h` 12, `cf_value.h` 11, `cf_error.h` 9 and `cf_port.h` 4 (base and port), `cf_platform.h` 9 and `cf_file.h` 7 (platform), `cf_encoding.h` 4 (text), `cf_serialize.h` 32 and `cf_serialize_version.h` 2 (serialize). `cf_error_clear`, `cf_arena_new` and `cf_arena_free` are K-9a's.
  * **How**: K-9a's rule of choosing, in its order, with the implementation read in ranges of at most 120 lines found through `docs/Codemap.md` (never a source file whole). For each name the evidence is one fact written in the worker's report, not in the header: the failure path found (or its absence), and for `strong` the point after which nothing can fail. The three usual answers: a `_free`, a `_get`, `_count`, `_name`, `_is_` or `_has_` is `nothrow` when its body has no failure path; `cf_arena_alloc` and `cf_arena_dup` are `strong` (a refused allocation changes nothing the arena holds); `cf_sink_*` is `basic` (the sink stays valid and cut), except `cf_sink_overflowed` and `cf_sink_init`, `nothrow`; `cf_file_replace` is `strong` (the target is unchanged until the rename), `cf_file_writer_write`, `_flush`, `_sync` and `_close` are `basic` (bytes may be on disk).
  * **Where a name is a defect**: a function found to leave an object invalid or to leak on a failure is fixed in its own commit, test first, when the fix is under 40 lines and changes no signature; otherwise it is left on the to-do list, named under "notes", and the card goes on (the card is done when every other name has its line and the to-do list holds only the names written under "notes").
  * **The list** is rewritten by `python scripts/check/guarantees.py --baseline .` and the table by `--write .` at the end; the gate refuses a name that has a line and is still listed.
* **files**: the headers named in "The set" under `lib/base/include/configforge/`, `lib/port/include/configforge/`, `lib/platform/include/configforge/`, `lib/text/include/configforge/` and `lib/serialize/include/configforge/`; `scripts/check/guarantees_todo.txt` and `docs/Guarantees.md` (both rewritten by the script); then `CHANGELOG.md`, `docs/Codemap.md` and `docs/API.md` (generated).
* **steps**:
  1. `git switch -c work/K-9d main`. Delete this card's names from `scripts/check/guarantees_todo.txt` (the lines of the headers above); see it fail: `ctest --preset default -R gate.guarantees --output-on-failure | tail -n 8` lists them as `no guarantee`.
  2. Per header: read the declarations (`sed -n` at most 120 lines at a time) and the bodies through `docs/Codemap.md`; write each `Guarantee:` line; one commit a component, `K-9d: <component> guarantees`.
  3. A defect found: the test first, the fix, its own commit.
  4. `python scripts/check/guarantees.py --baseline .` and `--write .`; `ctest --preset default -R "gate.guarantees|gate.apiref"` exit 0.
  5. `CHANGELOG.md`. Format; codemap and apiref last; ACCEPT; commit `K-9d: guarantees for the lower components`.
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI; `ctest --preset default -R "gate.guarantees|gate.apiref|gate.abi"` exit 0.
* **stop if**: the worker passes 60 turns (split by component: the commits already made stand, the rest goes to a new card); a header's function is declared in two headers with different words (report it).
* **docs**: `CHANGELOG.md`, "## 2.0.0 — unreleased", "### Added": "Failure guarantees written for the allocator, the error, the value, the file layer, the text helpers and the format readers (K-9d)", and under "### Fixed" one line a defect fixed.

## K-9e - Guarantees written for the facade: `configforge.h`

* **id**: K-9e
* **goal**: each of the 83 public names of `lib/config/include/configforge/configforge.h` has its guarantee in its header comment and leaves `scripts/check/guarantees_todo.txt`.
* **why**: this is the header a program includes: the builder, the reads, `cf_derive`, `cf_write`, `cf_explain`, `cf_report`. A program that retries a failed `cf_builder_create` or a refused `cf_write` needs to be told what state it is retrying from.
* **needs**: K-9a
* **size**: medium
* **version**: 2.0.0
* **status**: open
* **decided**:
  * **The set**: `python scripts/check/guarantees.py --todo lib/config/include/configforge/configforge.h` (83 names at 18557b9; the worker works from that output, not from a count).
  * **The classification, decided for the families** (the worker confirms each by reading its function through `docs/Codemap.md`, in ranges of at most 120 lines, and writes down the one fact as K-9d does): the 30 `cf_builder_*` calls that return `cf_bool` and defer their first failure to `cf_builder_create` (`configforge.h`, "Tier 1 and 2"; the same 30 that `scripts/check/nodiscard.txt` lists) are `basic: the failure is held by the builder; cf_builder_create reports it`; `cf_builder_create`, `cf_derive`, `cf_derive_in` and `cf_build_in` are `strong` (the builder or the parent configuration is unchanged; a configuration is made apart); `cf_write` is `strong` (the read snapshot never changes, `lib/config/tests/test_oom.c` already checks it); `cf_get_*`, `cf_has`, `cf_count`, `cf_digest` and the other readers of a finished configuration are `nothrow` (reading is infallible once construction returned, the comment of `run_with` in `test_oom.c` says so); `cf_explain`, `cf_inspect`, `cf_report` and `cf_emit` write into the caller's buffer and report the size needed: `strong` (the configuration is untouched; the buffer is the caller's, unspecified on failure); `cf_config_free`, `cf_builder_abort` are `nothrow`.
  * **A name whose family is not here**: the rule of choosing of K-9a, and a line in the worker's report.
  * **Defects**: as K-9d decides (fixed under 40 lines and no signature, else named under "notes").
* **files**: `lib/config/include/configforge/configforge.h`; `scripts/check/guarantees_todo.txt` and `docs/Guarantees.md` (rewritten by the script); then `CHANGELOG.md`, `docs/Codemap.md` and `docs/API.md` (generated).
* **steps**:
  1. `git switch -c work/K-9e main`. Delete the header's names from `scripts/check/guarantees_todo.txt`; see it fail as K-9d's step 1 does.
  2. Annotate in file order, one commit a section of the header (the banners): `K-9e: <section> guarantees`; a defect found: test first, fix, own commit.
  3. `python scripts/check/guarantees.py --baseline .` and `--write .`; `ctest --preset default -R "gate.guarantees|gate.apiref"` exit 0.
  4. `CHANGELOG.md`. Format; codemap and apiref last; ACCEPT; commit `K-9e: guarantees for configforge.h`.
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI; `ctest --preset default -R "gate.guarantees|gate.apiref|gate.abi|test_oom"` exit 0.
* **stop if**: the worker passes 60 turns (the commits stand; the rest goes to a new card); a `cf_builder_*` call turns out to fail without holding the failure (a defect: report the name).
* **docs**: `CHANGELOG.md`, "## 2.0.0 — unreleased", "### Added": "Failure guarantees written for every call of `configforge.h` (K-9e)".

## K-9f - Guarantees written for sources and the key space: `cf_source.h`, `cf_keyspace.h`, `cf_entry.h`, `cf_iterate.h`

* **id**: K-9f
* **goal**: each public name of `lib/config/include/configforge/cf_source.h`, `cf_keyspace.h`, `cf_entry.h` and `cf_iterate.h` has its guarantee in its header comment and leaves `scripts/check/guarantees_todo.txt`.
* **why**: a program that writes its own source (`cf_source_*`, the vtable of D-14) needs to know what the library does with a source whose `load` fails half way; the key space is what every load ends in.
* **needs**: K-9a
* **size**: medium
* **version**: 2.0.0
* **status**: open
* **decided**:
  * **The set**: 67 names at 18557b9 (`cf_source.h` 35, `cf_entry.h` 14, `cf_keyspace.h` 13, `cf_iterate.h` 5); the worker uses `python scripts/check/guarantees.py --todo lib/config/include/configforge/cf_source.h` and the other three paths.
  * **The families** (confirmed as in K-9e): a `cf_source_*` constructor (`cf_source_file`, `cf_source_dir`, `cf_source_env`, ...) fills a `cf_source` the caller owns: `strong` when a failure leaves the struct as it was, `basic` when it is left zeroed or half filled but safe to `cf_source_destroy`; the worker finds which by reading each, and a source left unsafe to destroy is a defect; `cf_source_destroy` is `nothrow`; `cf_source_load` is `basic: the key space may hold some of the source's keys; the caller drops it` unless its body shows the keys are added at the end; the `cf_ks_add*` calls are `basic` (the space stays valid and holds what was added), `cf_ks_arena`, `cf_key_cmp`, `cf_key_segments`, `cf_outcome_name` and the other readers of `cf_keyspace.h` `nothrow`; in `cf_entry.h` the `cf_lex_*` and `cf_cli_*` calls that fill a sink or a key space are `basic` and `cf_env_value`, `cf_keymap_*`, `cf_keysink_*` and `cf_listsink_*` are classified by reading them (an `_init` is `nothrow`); `cf_iterate.h`'s `cf_iter_*` and `cf_array_*` `nothrow` (they only read a finished configuration).
  * **Defects**: as K-9d decides.
* **files**: the four headers above; `scripts/check/guarantees_todo.txt` and `docs/Guarantees.md` (rewritten by the script); then `CHANGELOG.md`, `docs/Codemap.md` and `docs/API.md` (generated).
* **steps**:
  1. `git switch -c work/K-9f main`. Delete the four headers' names from the to-do list; see it fail.
  2. Annotate, one commit a header: `K-9f: <header> guarantees`; a defect: test first, fix, own commit.
  3. `python scripts/check/guarantees.py --baseline .` and `--write .`; `ctest --preset default -R "gate.guarantees|gate.apiref"` exit 0.
  4. `CHANGELOG.md`. Format; codemap and apiref last; ACCEPT; commit `K-9f: guarantees for sources and the key space`.
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI; `ctest --preset default -R "gate.guarantees|gate.apiref|gate.abi"` exit 0.
* **stop if**: the worker passes 60 turns (commits stand; the rest goes to a new card); a source constructor leaves a struct unsafe to destroy (fix if under 40 lines, else notes).
* **docs**: `CHANGELOG.md`, "## 2.0.0 — unreleased", "### Added": "Failure guarantees written for the source and key-space calls (K-9f)".

## K-9g - Guarantees written for the rest of the configuration library, and the to-do list closed

* **id**: K-9g
* **goal**: each remaining public name (`cf_live.h`, `cf_image.h`, `cf_define.h`, `cf_version.h`, `cf_edit.h`, `cf_report.h`, `cf_log.h`, `cf_emit.h`, and any other still listed) has its guarantee, `scripts/check/guarantees_todo.txt` is deleted, and `docs/Guarantees.md` reads `not yet 0`.
* **why**: the live reload, the image, the edit and the report are the calls whose failure a running program meets last and handles worst; this card closes K-9a's list so that from then on the gate refuses a public function without a guarantee.
* **needs**: K-9a, K-9d, K-9e, K-9f
* **size**: medium
* **version**: 2.0.0
* **status**: open
* **decided**:
  * **The set**: what `python scripts/check/guarantees.py --todo lib/config` lists once K-9e and K-9f have landed; 62 at 18557b9 (`cf_live.h` 18, `cf_image.h` 10, `cf_define.h` 10, `cf_version.h` 8, `cf_edit.h` 8, `cf_report.h` 6, `cf_log.h` 1, `cf_emit.h` 1).
  * **The families** (confirmed as in K-9e): `cf_live_reload`, `cf_live_commit`, `cf_live_rollback` and `cf_live_open_with` are `strong` (a failed reload keeps serving the snapshot it had); `cf_live_get`, `_generation`, `_pending`, `_restarts`, `_status_get` and `_last_error` are `nothrow`; `cf_image_write` and `cf_edit_commit` are `strong` (nothing is written until the whole document is made; K-9b proves it), `cf_edit_set_*` and `cf_edit_unset` are `basic` (the edit holds the failure; `cf_edit_abort` drops it), `cf_edit_begin` `strong`; `cf_image_open` is `strong` (the image struct is unchanged) and `cf_image_load` `basic` (the key space may hold some keys), `cf_image_count`, `_at` and `_get` `nothrow`; every `cf_version.h` call (`cf_version`, `cf_build_id`, `cf_feature_at`, ...) `nothrow`; in `cf_define.h` `cf_def_init_sized`, `cf_fingerprint_seed`, `cf_fingerprint_step` and `cf_policy_text` are `nothrow`, `cf_defs_apply` and the `cf_source_*_defs` calls are `strong` where a failure leaves the source's definitions as they were and `basic` otherwise (the worker reads which); `cf_report_*` as `cf_report` in K-9e.
  * **The close**: after the last name is annotated `python scripts/check/guarantees.py --baseline .` writes an empty list; this card then deletes `scripts/check/guarantees_todo.txt` and the sentence about it in `docs/Guarantees.md`; the gate with no file reads an empty list. A name still on the list through a defect written under a card's "notes" is not allowed to stay: this card fixes it (test first) or, when it needs a signature change, adds it to `scripts/check/guarantees_todo.txt` and does not delete the file (then the card's report says which names, and 2.0.0 does not release before they are fixed).
  * **Defects**: as K-9d decides.
* **files**: the headers above; `scripts/check/guarantees_todo.txt` (deleted), `scripts/check/guarantees.py` (the sentence about the list in its docstring), `docs/Guarantees.md` (rewritten); then `CHANGELOG.md`, `docs/Codemap.md` and `docs/API.md` (generated).
* **steps**:
  1. `git switch -c work/K-9g main` (after K-9e and K-9f). Delete the remaining names from the list; see it fail.
  2. Annotate, one commit a header: `K-9g: <header> guarantees`; a defect: test first, fix, own commit.
  3. `python scripts/check/guarantees.py --baseline .` (an empty list), delete the file, `--write .`; `ctest --preset default -R "gate.guarantees|gate.apiref"` exit 0; and `grep -c "not yet" docs/Guarantees.md` shows the count line `not yet 0`.
  4. A negative proof in a scratch copy: a new `CF_API` name with no line is refused now that the list is gone.
  5. `CHANGELOG.md`. Format; codemap and apiref last; ACCEPT; commit `K-9g: guarantees for the rest, the to-do list closed`.
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI; `ctest --preset default -R "gate.guarantees|gate.apiref|gate.abi|test_live"` exit 0; no file `scripts/check/guarantees_todo.txt` in the tree.
* **stop if**: the worker passes 60 turns (commits stand; the rest goes to a new card); a defect that needs a signature change (list it, as decided above, and report).
* **docs**: `CHANGELOG.md`, "## 2.0.0 — unreleased", "### Added": "Every public function states one failure guarantee; `gate.guarantees` refuses one that does not (K-9g)".

## K-9b - Allocation faults injected into every load and write path, and what a failure leaves behind checked

* **id**: K-9b
* **goal**: one shared failing allocator and sweep (`tests/support/cf_oom.h`) refuse the 1st, 2nd, ... nth allocation of each load and write path in turn; each case checks the failure is `CF_E_MEMORY` with a message, that nothing leaks and nothing crashes (the allocator's live count, and AddressSanitizer in the `sanitize` preset), and, for a `strong` function, that the state is as it was; `gate.oom` refuses a public load or write function that has no case and no excuse.
* **why**: SQLite's way (a test allocator that fails the nth allocation) is the only proof that the guarantees K-9a to K-9g write are true and that "out of memory returns `CF_E_MEMORY`" holds on every path; `lib/config/tests/test_oom.c` does it today for one builder run, the mapped store and the write, and nothing for a codec on its own, the edit, the image, the live reload, the file layer or `cf_derive`. D-223 point 5 asks for it ("K-9b's injected faults also check that each failure returns the right code").
* **needs**: K-9a, K-9d, K-9e, K-9f, K-9g
* **size**: large
* **version**: 2.0.0
* **status**: open
* **decided**:
  * **The shared header** `tests/support/cf_oom.h` (C89, static functions, in the style of `tests/support/cf_test.h`, which names every helper in `cf_test_done()` so that none is reported unused: `cf_oom_sweep` uses the others, and every program that includes the header calls it). It holds what `lib/config/tests/test_oom.c` defines for itself at lines 35-67 (`failing`, `failing_alloc`, `failing_release`), made a struct `cf_oom { size_t budget; size_t calls; size_t live; size_t refused; }` with `void cf_oom_init(cf_oom* o, cf_allocator* a)` filling the `cf_allocator` (`alloc`, `release`, `ctx`) and the sweep: `typedef int (*cf_oom_try)(void* ctx, const cf_allocator* a, cf_u64* digest, cf_error* err);` returning `CF_OOM_DONE` (it worked and `digest` describes the result), `CF_OOM_FAILED` (it failed and `err` is set) or `CF_OOM_BAD` (the case's own check failed); `size_t cf_oom_sweep(const char* what, cf_oom_try fn, void* ctx);`. The sweep runs the case once unlimited (it must be `CF_OOM_DONE`, `live` back to 0, `calls` is the number of injection points, the reference digest), then with a budget of 0, 1, 2, ... until a run is `CF_OOM_DONE`; for every `CF_OOM_FAILED` it checks `err.code == CF_E_MEMORY`, `err.message` is not empty, `live == 0`; for the `CF_OOM_DONE` it checks the digest equals the reference; it checks that the run that reached success did so at the budget the unlimited run counted (every counted allocation is an injection point) and, when `calls > 0`, that at least one run failed. It returns `calls`, so that a test can require that a bigger input takes more.
  * **`test_oom.c` is converted first** to the shared header, and its three sweeps give the same results (the same checks, the same counts): no second copy of the allocator remains (the maintainer's rule: no duplication).
  * **The three places that took the default allocator, not the caller's** (found by `grep -rn "cf_default_allocator()" lib --include=*.c`; the others are `cf_arena_new`'s and `cf_build`'s own NULL fallbacks): `cf_edit_begin` (`lib/config/src/core/edit.c`, the call after the argument check) takes the allocator of the configuration it edits (`cf_arena_allocator` of the configuration's arena, as `cf_derive` does), the default one when that is NULL; `cf_file_writer_open` (`lib/platform/src/file_layer.c`, the `a = cf_default_allocator();` line) takes `options->allocator`, a member appended to `cf_file_options` (D-51: read only where `size` reaches it) with the default NULL, which is `cf_default_allocator()` as today, set by `cf_file_options_init`; its comment in `lib/platform/include/configforge/cf_file.h` is rewritten, and `cf_file_replace` follows the same option. A program that built a configuration with its own allocator now has the edit's memory come from it too: a Fixed line in `CHANGELOG.md`; no default changes for a program that passes none.
  * **The cases**, one function each, in `lib/config/tests/test_oom_paths.c` (new; the existing `test_oom.c` keeps the builder, the sources and `cf_write`), `lib/serialize/tests/test_oom_decode.c` (new) and `lib/platform/tests/test_oom_file.c` (new, with a `lib/platform/tests/CMakeLists.txt` if that directory has none; the card finds out with `ls lib/platform`): **load** `cf_codec_decode` for each codec the build has (a small document each, into an arena made from the failing allocator), `cf_blob_file`, `cf_builder_create` with `cf_builder_document` for each of JSON, TOML, YAML, INI, `.properties` and CBOR, with `cf_builder_file` (a file written by the test), `cf_builder_dir`, `cf_builder_dotenv`, `cf_derive` and `cf_derive_in`, `cf_image_open` with `cf_image_load`, `cf_live_open_with` and `cf_live_reload`; **write** `cf_emit` to each format the build has, `cf_report_write` and `cf_schema_write`, `cf_edit_begin` with `cf_edit_set_text` and `cf_edit_commit`, `cf_image_write`, `cf_live_commit`, `cf_file_replace` and `cf_file_writer_open`, `_write`, `_close`. (`cf_write` stays in `test_oom.c`.)
  * **What a `strong` case checks besides the code**: `cf_derive` and `cf_derive_in`: the parent's `cf_digest` is unchanged; `cf_edit_commit` and `cf_file_replace`: the target file's bytes are unchanged on a failure (read back with `cf_blob_file` under the real allocator); `cf_live_reload` and `cf_live_commit`: `cf_live_generation` and the served snapshot's digest are unchanged; `cf_image_write`: the output buffer's contents are not looked at (the caller's), and the configuration's digest is unchanged; `cf_builder_create` (`strong` for the builder): the builder can be given its source again and created once the allocator works. A `basic` function's case checks only that the object can be freed (`live == 0` after the free) and reused where its comment says it can.
  * **`gate.oom`**: `scripts/check/oom.py <root>` (Python 3 standard library) reads `lib/config/tests/oom_paths.txt`, one `name|where` a line (`where` is `test_oom_paths.c:<function>` or `-|<reason>` for a name that allocates nothing or is covered through another), and exits 1 with `<name>: no sweep case or excuse` for each public name (K-9a's list of names) whose guarantee is `strong` or `basic` and whose name holds `_create`, `_load`, `_decode`, `_write`, `_commit`, `_replace`, `_derive`, `_reload`, `_emit`, `_open` or `_save`, and has no line; `<name>: the case <function> is not in <file>` for a line whose function is not in the file; `<name>: <where> does not call cf_oom_sweep` for one that never reaches the sweep. `cmake/gates/GateOom.cmake` as `GateNodiscard.cmake`, `Oom` added to the same `foreach` as K-9a's `Guarantees`.
  * **Leaks**: the live count in every case; the `sanitize` preset (AddressSanitizer with LeakSanitizer, which is on by default on Linux) runs the new programs, and ACCEPT-LINUX includes it.
  * **Defects the sweep finds** (a wrong code, a leak, a crash, a `strong` function that changed state): fixed one by one, each in its own commit with the case that found it kept; a fix over 60 lines or one that needs a signature change is written under "notes" with its case, that case is listed in `lib/config/tests/oom_paths.txt` as `<name>|known: <why>` and the card goes on; the gate reads a `known:` line as an excuse, and 2.0.0 does not release while one remains (the report says which).
  * **Rejected**: a hook in `cf_default_allocator()` or a linker interposer (a global mutable seam in the library, D-101; the allocator is an argument everywhere it matters, and K-9b makes the last two places so); injecting I/O failures here (a different card: this is memory); a randomised failure schedule (the sweep is exhaustive and so reproducible).
* **files**: `tests/support/cf_oom.h` (new), `lib/config/tests/test_oom.c`, `lib/config/tests/test_oom_paths.c` (new), `lib/config/tests/oom_paths.txt` (new), `lib/config/tests/CMakeLists.txt`, `lib/serialize/tests/test_oom_decode.c` (new), `lib/serialize/tests/CMakeLists.txt`, `lib/platform/tests/test_oom_file.c` (new) and its `CMakeLists.txt`, `lib/config/src/core/edit.c`, `lib/platform/src/file_layer.c`, `lib/platform/include/configforge/cf_file.h`, `scripts/check/oom.py` (new), `cmake/gates/GateOom.cmake` (new), `cmake/ConfigForgeGates.cmake`, then `CHANGELOG.md`, `docs/Safety.md` (a paragraph "Memory that runs out"), `docs/Decisions.md`, `docs/Codemap.md` and `docs/API.md` (generated).
* **steps**:
  1. `git switch -c work/K-9b main` (after K-9g). Write `cf_oom.h`; convert `test_oom.c`; `ctest --preset default -R test_oom` exit 0 with the same checks.
  2. Write `oom.py`, its gate and an empty `oom_paths.txt`; see it fail: `ctest --preset default -R gate.oom --output-on-failure | tail -n 20` lists each public load or write name as `no sweep case or excuse`.
  3. The three allocator places (test first: a case in `test_oom_paths.c` that edits a configuration built with the failing allocator and expects `live` to move; it fails while `edit.c` takes malloc), each in its own commit.
  4. The cases, one commit a file: serialize, platform, config; each `ctest --preset default -R "test_oom"` exit 0 and each defect as decided.
  5. `oom_paths.txt` written from the cases; `ctest --preset default -R "gate.oom|test_oom"` exit 0; the same in the `sanitize` preset (WSL): exit 0, no leak report.
  6. A negative proof, in a scratch copy: make `cf_edit_commit` leak one block on its failure path behind a run-time-false condition (`cf_oom.calls == 0x7fffffffu` compared at run time, never a constant: MSVC's C4127 and a stale binary), see `live` refused; remove it.
  7. `docs/Safety.md`, `CHANGELOG.md`, the D-number. Format; codemap and apiref last; ACCEPT; commit `K-9b: allocation faults injected into every load and write path`.
* **accept**: ACCEPT-LINUX (including the `sanitize` preset), ACCEPT-WINDOWS, ACCEPT-CI; `ctest --preset default -R "gate.oom|gate.guarantees|test_oom"` exit 0; `gate.abi` shows one addition at most (the appended `cf_file_options` member is not a symbol); the existing `test_oom.c` checks unchanged.
* **stop if**: the worker passes 60 turns (the commits stand and the cases left go to a second card, K-9b's notes name which); a path cannot be failed by the allocator because it calls `malloc` directly (`gate.symbols` should have refused it: report the file and line, add nothing); a fix needs a change to a public signature (as decided: listed as known, reported); three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 2.0.0 — unreleased", "### Added": "An allocator that refuses the nth allocation, swept over every load and write path, and `gate.oom` (K-9b)"; "### Fixed": "`cf_edit_begin` and `cf_file_writer_open` take the caller's allocator, not malloc's (K-9b)" and one line a defect the sweep found; `docs/Safety.md` as above; `docs/Decisions.md`: the next free D-number after K-9a's, "Out of memory returns `CF_E_MEMORY` on every path, proved by an exhaustive allocation sweep".

## D-223 - One error model (the maintainer, 2026-11-26, relayed from a side session: "I like the error model"; "the core uses the C89 standard")

It answers K-6a's and K-6b's questions and Y-28's choice C, and K-9c
follows it, so that `cf_error` changes shape once. K-6a, K-6b and Y-28
follow it; each carries it under "Organiser's answers". The core stays C89 (D-72): a newer standard's
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

## K-10a - A number's text, kept when the program asks

* **id**: K-10a
* **goal**: with `cf_number_options.keep_text` set, `cf_get_number_text(c, key, &len)` returns an integer's or a real's literal as its source wrote it, so that a program can hand it to its own big-number or decimal library; with the defaults nothing changes.
* **why**: a real is a binary64 `double` and an integer 64 bits (`lib/numeric/include/configforge/cf_numeric.h:66-77`); a scientific or financial program may need the digits the file holds, not their nearest `double` (the maintainer, 2026-11-27: "the default behaviour of numbers must not break").
* **needs**: nothing
* **size**: large
* **version**: 2.2.0
* **status**: open
* **decided**:
  * **Options (D-220).** `typedef struct cf_number_options { size_t size; cf_bool keep_text; int underflow; int nonfinite; } cf_number_options;` in `configforge.h`; `CF_API void cf_number_options_init(cf_number_options* o)` sets `size` and the defaults (K-11a's convention), which are today's behaviour: `keep_text` `CF_FALSE` (default), `underflow` `CF_UNDERFLOW_ZERO` (default; K-10b), `nonfinite` `CF_NONFINITE_REFUSE` (default; K-10b). `cf_builder_numbers(b, const cf_number_options* o)`; NULL is the defaults. This card adds the struct with all three fields; K-10b gives the last two their effect.
  * **The text.** `CF_API const char* cf_get_number_text(const cf_config* c, const char* key, size_t* len);` returns the literal (sign, digits, point, exponent, as the document spelt it, underscores of TOML kept) for a value of type int or real, NUL-terminated; NULL when the key is absent, not a number, or `keep_text` was off. A number from a binary format (CBOR, MessagePack, TLV) or from a default has no literal: the text is the shortest form that reads back exactly (`cf_f64_to_text`, or the decimal integer).
  * **Memory.** The text is stored only when `keep_text` is on, in the config's arena beside the value; with it off no byte is added (a test measures `cf_config`'s arena size before and after on the same document with the defaults: equal).
  * **Range.** Unchanged: a number outside 64 bits or `double` is still refused with `CF_REASON_NUMBER_RANGE`, also when `keep_text` is on. Accepting such numbers as text is "not now" (K-10c-m in docs/plan/Layout.md).
  * **Where.** The worker finds the places a number's literal reaches the key space with `grep -rn "cf_value_real\|cf_value_int" lib --include=*.c`; the literal is carried to the store through the sink event that already carries the value (no second parse). If the sink event has no access to the literal for a format, that format's text is the shortest form, written in the card's notes.
  * **Bindings.** Python `Config.number_text(key)`, Rust `Config::number_text(&self, key) -> Option<&str>`, C++ `cf::config::number_text(key)` returning `std::optional<std::string_view>` under C++17 and `const char*` before it.
  * **Rejected**: keeping the text always (memory for every user who does not need it); a big-number type of our own (an external dependency or a library inside the library).
* **files**: `lib/config/include/configforge/configforge.h`, the builder and store sources found by the grep above, `lib/config/tests/test_number_text.c` (new), the bindings' sources and tests, then `docs/Guide.md` (a section "Numbers beyond double"), `CHANGELOG.md`, `docs/API.md` and `docs/Codemap.md` (generated).
* **steps**:
  1. `git switch -c work/K-10a main`. Test first: `test_number_text.c` reads `{"a": 6.62607015e-34, "b": 12345678901234567890, "c": 1_000}` (JSON for a and b as JSON allows, TOML for c) with `keep_text` on and off; asserts the texts, NULL with the defaults, and the arena size equal with the defaults.
  2. See it fail: `ctest --preset default -R number_text --output-on-failure | tail -n 20`.
  3. The options struct and its init; the text carried and stored; the getter. `ctest --preset default` exit 0.
  4. Bindings and their tests; the Guide section.
  5. Format; codemap and apiref last; ACCEPT; commit `K-10a: a number's text kept when the program asks`.
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI; every existing test unchanged.
* **stop if**: carrying the literal needs a change to a public sink callback's signature (that is a break: 2.0.0's, reported); the arena grows with the defaults.
* **docs**: `CHANGELOG.md` under the newest unreleased heading, "### Added": `cf_number_options` and `cf_get_number_text` (K-10a); D-number: the next free one, "A number's text kept on request; defaults unchanged".

## K-10b - Underflow and infinity as options, the defaults as today

* **id**: K-10b
* **goal**: `cf_number_options.underflow = CF_UNDERFLOW_REFUSE` refuses a real that would read as zero though its digits are not zero, and `nonfinite = CF_NONFINITE_ACCEPT` reads TOML's `inf`/`nan` and YAML's `.inf`/`.nan`; with the defaults nothing changes.
* **why**: below the smallest subnormal a real becomes 0 without a word (`lib/numeric/include/configforge/cf_numeric.h:70-71`), which a scientific program may need to know; TOML and YAML define infinity and NaN, which the library refuses by design (`cf_numeric.h:59-61`).
* **needs**: K-10a
* **size**: medium
* **version**: 2.2.0
* **status**: open
* **decided**:
  * `underflow`: `CF_UNDERFLOW_ZERO` (the default, today's behaviour) or `CF_UNDERFLOW_REFUSE` (refused with `CF_REASON_NUMBER_RANGE`, the error's text "a real too small for a double, read as zero"). Detected by an internal `cf_text_to_f64_flags(s, len, out, end, unsigned* flags)` beside `cf_text_to_f64` (internal API, `CF_INTERNAL_API`), setting `CF_F64_UNDERFLOW` when non-zero digits gave zero.
  * `nonfinite`: `CF_NONFINITE_REFUSE` (the default, today's behaviour) or `CF_NONFINITE_ACCEPT`: TOML's `inf`, `+inf`, `-inf`, `nan`, `+nan`, `-nan` and YAML 1.2's `.inf`, `-.inf`, `.nan` (and their case forms the spec lists) read as the IEEE values; other formats keep refusing. `cf_emit` writes them back in the same spellings for TOML and YAML; for a format with no spelling (JSON, INI, .properties) `cf_emit` fails with `CF_E_CONVERT`. A definition's bounds (`cf_def_real` min, max) still apply: NaN fails every bound.
  * **Rejected**: changing either default (the maintainer, 2026-11-27).
* **files**: `lib/numeric/src/` (the float reader, found by `grep -rn "cf_text_to_f64" lib/numeric`), the TOML and YAML readers and writers in `lib/serialize/`, `lib/config/tests/test_number_options.c` (new), then `docs/Guide.md` ("Numbers beyond double"), `CHANGELOG.md`, generated docs.
* **steps**:
  1. `git switch -c work/K-10b main`. Test: `1e-400` with each underflow value; `inf`, `-inf`, `nan` in TOML and `.inf`, `.nan` in YAML with each nonfinite value; defaults equal to today's results.
  2. See it fail: `ctest --preset default -R number_options --output-on-failure | tail -n 20`.
  3. The change; `ctest --preset default` exit 0; the conformance suites unchanged (`ctest --preset default -R conformance`).
  4. Format; codemap and apiref last; ACCEPT; commit `K-10b: underflow and infinity as options, the defaults as today`.
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI; the TOML and YAML conformance results unchanged with the defaults.
* **stop if**: a conformance case changes with the defaults.
* **docs**: `CHANGELOG.md`, "### Added": underflow and non-finite options (K-10b); the D-number of K-10a extended.

## K-10c-m - Numbers beyond 64 bits or `double`, accepted as text under an option

* **id**: K-10c-m
* **goal**: with `cf_number_options.overflow = CF_OVERFLOW_TEXT`, an integer literal outside 64 bits and a real literal that reads as infinity in binary64 are not refused: the key holds the literal as text, `cf_get_number_text` returns it, and a program hands it to its own big-number or decimal library; with the default nothing changes (the number is refused with `CF_REASON_NUMBER_RANGE`, as today).
* **why**: K-10a keeps the text of a number that fits, and its "Range" bullet leaves the rest refused; a scientific or financial program that holds a 40-digit identifier or a constant beyond `1e308` in its file is refused today with no way to read it (`lib/numeric/include/configforge/cf_numeric.h:66-77`). The maintainer, 2026-11-27: "the default behaviour of numbers must not break": so this is an option, off.
* **needs**: K-10a, K-10b
* **size**: large
* **version**: 2.2.0
* **status**: open
* **decided**:
  * **The option (D-220, K-11a).** One member appended to `cf_number_options` (K-10a's struct, `size` first; D-51: read only where `size` reaches it): `int overflow;` with the macros `CF_OVERFLOW_REFUSE` (0, the default and today's behaviour) and `CF_OVERFLOW_TEXT` (1). `cf_number_options_init` sets `CF_OVERFLOW_REFUSE`; a NULL options pointer is the default; `cf_builder_numbers` carries it as it carries K-10a's members. Nothing else about `cf_number_options` changes.
  * **What is read.** Under `CF_OVERFLOW_TEXT` an integer literal that does not fit `int64` (and, for a format that allows it, `uint64`) and a real literal whose value is outside `double` (it reads as infinity; underflow is K-10b's) is stored as a text value holding the literal exactly as the document spelt it, with the entry flag `CF_ENTRY_NUMBER_TEXT` (new, in `lib/config/include/configforge/cf_keyspace.h` beside the other candidate flags; the worker finds where the entry's flags are kept with `grep -n "CF_CAND_TEXTUAL" lib/config/src/core/keyspace.c`). A number that fits is untouched (K-10a's `keep_text` decides whether its text is kept).
  * **What a definition says.** A key defined as int or real (`cf_def_int`, `cf_def_real`) still refuses such a literal with `CF_REASON_NUMBER_RANGE`: the program declared a 64-bit number. A key defined as text, and a key with no definition, accepts it as text. `cf_get_number_text(c, key, &len)` returns the literal for a key that carries `CF_ENTRY_NUMBER_TEXT` whatever `keep_text` says.
  * **Writing back.** `cf_emit` writes the value as the number it was (bare) in JSON, TOML and YAML, whose number literals have no limit of their own; in a format with no number type (INI, `.properties`) it is text as any other. A binary format (CBOR, MessagePack) has no spelling for it: `cf_emit` fails with `CF_E_CONVERT`, `a number beyond 64 bits cannot be written as <format>`.
  * **Formats that read it.** JSON, TOML and YAML; the binary formats have their own widths and keep refusing.
  * **Bindings.** Python `Config.number_text(key)`, Rust `Config::number_text`, C++ `cf::config::number_text`, as K-10a: they already return the text; one test each that a 30-digit integer reaches them.
  * **Rejected**: a big-number type of our own (a library inside the library, or a dependency); reading such a number as a `double` (it loses digits and would change what a program is told); making this the default (the maintainer: defaults of numbers do not change).
* **files**: `lib/config/include/configforge/configforge.h`, `lib/config/include/configforge/cf_keyspace.h`, the number readers found by `grep -rn "CF_REASON_NUMBER_RANGE" lib --include=*.c`, `lib/serialize/src/json.c`, `lib/serialize/src/toml.c`, `lib/serialize/src/yaml.c` and the emitters beside them, `lib/config/tests/test_number_overflow.c` (new), the bindings' sources and tests, then `docs/Guide.md` ("Numbers beyond double"), `CHANGELOG.md`, `docs/API.md` and `docs/Codemap.md` (generated).
* **steps**:
  1. `git switch -c work/K-10c-m main` (after K-10a and K-10b). Test first: `test_number_overflow.c` reads `{"a": 123456789012345678901234567890, "b": 1e999, "c": -9223372036854775809, "d": 42}` as JSON and the same in TOML and YAML, with the default (refused: `CF_REASON_NUMBER_RANGE`, the same message as before this card, byte for byte), with `CF_OVERFLOW_TEXT` (a, b, c read as text and by `cf_get_number_text`, d an int), with a definition of type int on `a` (refused), with a text definition (accepted), and `cf_emit` back to JSON, TOML and YAML (the literal, bare) and to CBOR (`CF_E_CONVERT`).
  2. See it fail: `ctest --preset default -R number_overflow --output-on-failure | tail -n 20`.
  3. The member, the flag, the readers' branch, the emitters; `ctest --preset default` exit 0; `ctest --preset default -R conformance` unchanged.
  4. Bindings and their tests; the Guide section.
  5. Format; codemap and apiref last; ACCEPT; commit `K-10c-m: numbers beyond 64 bits or double accepted as text under an option`.
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI; with the defaults every existing test and every conformance result unchanged.
* **stop if**: the entry has no word to hold `CF_ENTRY_NUMBER_TEXT` and adding one changes a promised struct (`gate.abi`: report, change nothing); a conformance case changes with the defaults.
* **docs**: `CHANGELOG.md` under the newest unreleased heading, "### Added": `cf_number_options.overflow` and `CF_OVERFLOW_TEXT` (K-10c-m); the D-number of K-10a extended ("a number beyond 64 bits kept as text when asked").
* **Organiser's answers**:
  * Written and opened under the maintainer's standing approval, 2026-11-27: built in its smallest form, the option off by default. The option is one member of K-10a's struct, `CF_OVERFLOW_REFUSE` the default, so no default changes (the maintainer's standing approval, 2026-11-27).
  * A definition of type int or real keeps refusing: a program that declared a 64-bit number must not be handed text under its key (the maintainer's standing approval, 2026-11-27).
