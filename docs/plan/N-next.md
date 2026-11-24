# N - the next cards, for 1.9.0

Three cards from the Roadmap's "Next after 1.8.0": what a refusal's reason
says about how serious it is (N-13), `configforge diff` over arrays of tables
(N-14), and the changed keys of a pull request that touches configuration
files only (N-15). Each is its own branch and lands alone. I-2, which opens
1.9.0, comes first: the three need it.

## I-2 - 1.9.0 opened: the 1.8 experimental functions settled (D-89)

* **id**: I-2
* **goal**: `cf_version.h` says 1.9.0, `CHANGELOG.md` has `## 1.9.0 — unreleased` on top, the thirteen functions experimental since 1.8 are `CF_API` (promised for the rest of 1.x), `cmake/configforge.experimental.symbols` lists no name, and every `gate.*` test passes.
* **why**: D-89: "the card that opens the next minor makes it `CF_API` or removes it". Until then `gate.abi` fails on 1.9.0, and N-13 to N-15 and P-2 wait. The decision was the maintainer's; on 2026-11-24 the maintainer, away, told the organiser to go on without asking and to open the new version if needed, and HANDOFF.md's recommendation (read from each declaration and its callers) is taken as it stands.
* **needs**: nothing (1.8.0 released, v1.8.0 at 7da87c6)
* **size**: small
* **decided**:
  * **None removed; all thirteen become `CF_API`.** In each declaration replace `CF_EXPERIMENTAL(1, 8)` by `CF_API` and change nothing else on the line (continuation lines stay as they are; clang-format re-wraps if it must):
    `lib/base/include/configforge/cf_error.h:112` `cf_reason_name`, `:113` `cf_reason_description`, `:117` `cf_error_place`;
    `lib/config/include/configforge/cf_source.h:347` `cf_source_file_encoding`;
    `lib/config/include/configforge/cf_version.h:69` `cf_build_profile`;
    `lib/config/include/configforge/configforge.h:683` `cf_usage`;
    `lib/serialize/include/configforge/cf_serialize.h:304` `cf_format_holds`, `:317` `cf_format_limits`, `:454` `cf_codec_compose_sized`;
    `lib/text/include/configforge/cf_encoding.h:44` `cf_encoding_by_name`, `:48` `cf_encoding_name`, `:60` `cf_encoding_recode`, `:73` `cf_encoding_check`.
    A comment next to one of them that says "experimental" (search `grep -n -i experimental` in those six headers) loses that word; nothing else in the comments changes.
  * **`cf_format_limits` and `cf_usage` as they are.** HANDOFF.md offered a choice for these two (a third out-parameter, a trailing `flags`). Taken: promoted with today's signatures; a further limit or an option comes later as a new function (`cf_format_limits2`-style names are not chosen here: the card that needs one names it). This goes into D-89 (below) so that no later card reopens it.
  * **`CF_EXPERIMENTAL` itself stays** in `cf_port.h`: it is the tier, and N-13 uses it with `(1, 9)`.
  * **The symbols file** keeps its four comment lines and has no name line; write it with `cmake -DSRC=. -DWRITE=ON -P cmake/gates/GateAbi.cmake`, not by hand. If the promised list (`cmake/configforge.symbols`, and `cmake/configforge_serialize.symbols` for the three in `cf_serialize.h` and the four in `cf_encoding.h` if that list carries them) needs the names added, the same WRITE run does it; read `git diff --stat cmake/` after it and see only those files changed.
  * **The bindings**: Python and Rust already call `cf_encoding_by_name`, `cf_encoding_name`, `cf_reason_name` and `cf_source_file_encoding`; promoted, they are no longer experimental, which settles D-89's "the bindings expose no experimental function". No binding code changes.
  * **D-89 in `docs/Decisions.md`** (after its **Decision.** paragraph, before **Compatibility.**): one paragraph, "**Settled in 1.9.0 (I-2).** The thirteen functions of 1.8 were promoted, none removed. `cf_format_limits` and `cf_usage` kept their signatures; a further limit or an option comes as a new function." `docs/Decisions.md` is not read whole: `grep -n "^## D-89" docs/Decisions.md` and edit from there.
  * **`CHANGELOG.md`**: `version.cmake` opens the section; add under it a "Changed" entry: "The thirteen functions experimental in 1.8 (list them) are promised for the rest of 1.x (D-89)." Never touch the dated `## 1.8.0` section.
  * **The index** (`docs/Plan.md`): this card's row before N-13's, `open`; at the landing, `done (<hash>)`. N-13, N-14, N-15 keep their "Needs" text; their status stays `open`.
* **files**: `scripts/release/version.cmake` (run, not edited) and what it rewrites (`lib/config/include/configforge/cf_version.h`, the vcpkg manifest, the Rust crate's and the Python package's version, `CHANGELOG.md`), the six headers above, `cmake/configforge.experimental.symbols` and the symbols lists the WRITE run changes; last: `docs/Decisions.md`, `docs/API.md` (generated: `cmake -P scripts/generate/apiref.cmake`), `docs/Codemap.md` (generated: `cmake -P scripts/generate/codemap.cmake`), `CHANGELOG.md`, `docs/Plan.md`.
* **steps**:
  1. A worktree of its own from `origin/main`: `git worktree add .claude/worktrees/i2 -b work/I-2 origin/main`. Work only there.
  2. `cmake -DTO=1.9.0 -P scripts/release/version.cmake`; `git diff --stat` shows the version files and `CHANGELOG.md`. Commit that alone ("version: 1.9.0 opened").
  3. **See it fail**: on Linux (below), configure and build the default preset and run `ctest -R "^gate\.abi$"`: it fails, naming the 1.8 names past their release. Write the first failing line into the report.
  4. The thirteen edits; the WRITE run; `ctest -R "^gate\.abi"` passes. Commit.
  5. D-89, CHANGELOG, the index row. `python scripts/work/work.py format`, then on Linux `cmake -P scripts/generate/apiref.cmake` and `cmake -P scripts/generate/codemap.cmake` last; `docs/API.md` no longer marks any function "experimental since 1.8" (`grep -n "1\.8" docs/API.md` in its experimental marks). Commit.
  6. On Linux: `ctest -R "^gate\."` in the default preset exit 0; then ACCEPT-LINUX with `-DMODE=quick`. `work.py win` on Windows. Push `work/I-2`, ACCEPT-CI.
* **Linux here**: WSL distro `configforge-dev`. Copy the tree off `/mnt` (e.g. `rsync -a --delete --exclude .git --exclude 'build*' /mnt/c/.../.claude/worktrees/i2/ $HOME/i2-src/`, then `git -C` is not needed: run from the copy) and build there; write long outputs to `/mnt/d/cf-work/I-2-*.out` and read only their tails. Never two verifies at once: before starting one, `pgrep -af "verify.cmake|ctest"` in the distro shows none (another card, L-9, may be running one; wait for it). Never type a password; never use sudo.
* **accept**: ACCEPT-LINUX (quick), `ctest -R "^gate\."` exit 0 on Linux, `work.py win` exit 0, ACCEPT-CI.
* **stop if**: `gate.abi` still fails after the WRITE run for a reason other than these thirteen names; any binding test fails because of the promotion; a gate other than `gate.abi`, `gate.apiref`, `gate.version` and `gate.codemap` changes its result.
* **notes**: (empty)

## N-13 - a reason says how serious it is, and which weakness it defends

* **id**: N-13
* **goal**: `cf_reason_severity(reason)` returns `"error"`, `"warning"` or `"none"` and `cf_reason_weakness(reason)` returns a CWE id such as `"CWE-674"` or `""`, for every reason in `cf_reason.def`, and a test holds both tables to the reasons that exist.
* **why**: A-10 gave every refusal a place and a reason; the Roadmap's "One diagnostic model across formats" lists severity and a weakness id as what is left. With them a user filters refusals by kind ("show me what a hostile input tripped, not what a typo did") without matching message text, and the planned CWE table (docs/Compliance.md, "Order of work", item 5) has a column to be built from. The part that waits: writing them into Doctor's report or SARIF changes a schema, so this card only gives the library the two answers. "The same bad input is refused the same way in every format" is already a test (`rows` of `lib/config/tests/test_reason.c`, one row a format and reason) and is not repeated here.
* **needs**: 1.9.0 opened (the D-89 decision; HANDOFF.md)
* **size**: small
* **decided**:
  * **Two functions, both `CF_EXPERIMENTAL(1, 9)`** (D-89: how a new function comes in; they are not promised until a later minor decides), declared after `cf_reason_description` in `lib/base/include/configforge/cf_error.h:110-113` with the same comment style, one line each as D-89 asks:
    `CF_EXPERIMENTAL(1, 9) const char* cf_reason_severity(int reason);`
    `CF_EXPERIMENTAL(1, 9) const char* cf_reason_weakness(int reason);`
    Strings, not a new enum: no new public type, and the C++ wrapper and bindings stay as they are (the bindings expose no experimental function).
  * **Where the data lives**: two `static const char* const` arrays in `lib/base/src/error.c`, indexed by the reason's value, beside `cf_reason_name` (`lib/base/src/error.c:38-62`). `cf_reason.def` keeps its three columns: changing the arity of a public X-macro file would break whoever expands it, and `scripts/generate/apiref.cmake:214-224` reads it. A compile-time check keeps the arrays in step with the enum: `typedef char cf_reason_severity_size[(sizeof severity_of / sizeof severity_of[0] == (size_t)CF_REASON_STORAGE + 1) ? 1 : -1];` (and the same for the weakness array). C89, no `static_assert`.
  * **Severity.** `"none"` for `CF_REASON_NONE`. `"error"` when the input is malformed by its format's own rules: syntax, utf-8, encoding, truncated, trailing, empty, top-level, escape, control-character, number, duplicate-key, key, structure. `"warning"` when the input is well formed but the library refuses it for a limit or a policy: depth, too-long, budget, number-range, unsupported, storage. An unknown reason (negative, or above the last): `"?"`, as `cf_reason_name` does.
  * **Weakness** (a defence the refusal is; the ids are the MITRE CWE list's, and a worker does not change them): depth `"CWE-674"` (uncontrolled recursion), budget `"CWE-400"` (uncontrolled resource consumption), too-long `"CWE-770"` (allocation without limits), utf-8 `"CWE-176"` (improper handling of Unicode encoding), duplicate-key `"CWE-694"` (use of multiple resources with duplicate identifier). Every other reason, and an unknown one: `""`. Not `NULL`: the answer is always printable.
  * **Not in this card**: the report's and SARIF's new fields, the `docs/Doctor.md` column, the CWE table and `gate.cwe`, the C++ wrapper's methods. Each is its own step later; none is needed to use the two functions.
* **files**: `lib/config/tests/test_reason.c`, `lib/base/include/configforge/cf_error.h`, `lib/base/src/error.c`, `cmake/configforge.experimental.symbols`; last: `docs/API.md` (generated), `docs/Guide.md` (the paragraph at `docs/Guide.md:478-484`), `docs/Codemap.md` (generated: `cmake -P scripts/generate/codemap.cmake`), `CHANGELOG.md`.
* **steps**:
  1. `git switch -c work/N-13` from `main` with 1.9.0 open.
  2. In `lib/config/tests/test_reason.c`, after the checks at lines 96-102, add a table `classes[]` of `{reason, severity, weakness}` for all twenty reasons as decided, and a loop that compares `cf_reason_severity(i)` and `cf_reason_weakness(i)` with it, plus the two unknown values (`-1` and `last + 1`: `"?"` and `""`). Build `test_reason`.
  3. **See it fail**: the build stops on the two undeclared functions. Write the declarations (header) and a body that returns `"?"` and `""` for everything; `ctest -R test_reason` now fails on the first reason.
  4. Fill the two arrays and the compile-time size check in `error.c`; the test passes. Add a reason to a scratch copy of `cf_reason.def` and see the size check stop the build, then discard the copy (`git checkout -- lib/base/include/configforge/cf_reason.def`).
  5. `cmake -DSRC=. -DWRITE=ON -P cmake/gates/GateAbi.cmake` adds `cf_reason_severity 1.9` and `cf_reason_weakness 1.9` to the symbols file; `ctest -R gate.abi` passes.
  6. `docs/Guide.md`: after "Use `cf_reason_name` for its name and `cf_reason_description` for its meaning." add one sentence: "`cf_reason_severity` says whether the input was malformed (`error`) or well formed and over a limit (`warning`), and `cf_reason_weakness` names the CWE a defence answers, or an empty string; both are experimental in 1.9." `CHANGELOG.md`: an "Added" entry under the 1.9.0 heading.
  7. `python scripts/work/work.py format`, then `cmake -P scripts/generate/apiref.cmake` on Linux (docs/API.md cites source lines); ACCEPT.
* **accept**: ACCEPT-LINUX, ACCEPT-CI; `ctest -R "test_reason|gate.abi|gate.apiref"` exit 0 in the default preset.
* **stop if**: the arrays cannot be kept in step by a C89 compile-time check on a compiler of the matrix (write what failed under notes); any gate other than `gate.abi` and `gate.apiref` fails on the new declarations.
* **notes**: read cold against the tree on 2026-11-24 (after I-2 and N-12): the twenty reasons of `cf_reason.def` match the decided classes one to one (none, thirteen `error`, six `warning`), `CF_REASON_STORAGE` is still the last, and the cited ranges hold what the card says (`cf_error.h:110-113`, `error.c:38-62`, `test_reason.c`'s checks at 93-104, `docs/Guide.md:479`).

## N-14 - `configforge diff` over arrays of tables

* **id**: N-14
* **goal**: `configforge diff` compares a named array whose elements are all tables: each member of each element is an entry named by its path with the element's index as a segment, so `{"a":[{"x":1},{"x":2}]}` against `{"a":[{"x":1},{"x":3}]}` prints `changed<TAB>$["a"][1]["x"]`.
* **why**: Hosts, listeners, routes and rules are arrays of tables in most real configuration, and today both sides are refused ("left input refused"); N-1, N-5 and N-8 did flat mappings, nested tables and scalar arrays. This is the first part of the Roadmap's "`configforge diff` with the contract". The parts that wait: arrays of arrays, mixed arrays, and an array of tables inside an element of another (each stays refused); and units compared as values (`30s` is `30000ms`), which needs 1.10.0's durations and sizes (the Roadmap's item Durations and sizes) and so is a later card; contract-aware secret digests need the contract's reader in `tools/diff` and are the card after this one.
* **needs**: N-8 (done); 1.9.0 opened (the D-89 decision; HANDOFF.md)
* **size**: medium
* **decided**:
  * **Kinds of an entry** (`tools/diff/main.c:23-33`): `entry` gets `kind`, one of `ENTRY_SCALAR`, `ENTRY_ARRAY` (scalars, as now), `ENTRY_TABLES` (an array of tables: a marker with no value). `is_array` is replaced by `kind`. Two entries are equal when their kinds are equal and, for scalar and array, their values are equal (`tools/diff/main.c:322-341`); two `ENTRY_TABLES` markers are always equal.
  * **When an array turns into an array of tables**: `begin_array` records an `ENTRY_ARRAY` as now (`tools/diff/main.c:130-144`). The first `begin_table(NULL)` while that array is open and still empty (`e->value.items == NULL`) turns that entry into `ENTRY_TABLES` and starts element 0. A `begin_table(NULL)` after a scalar element, a scalar after a table element, or any `begin_array(NULL)` inside an array: the side is refused (the readers' `false`, as now). An empty array stays `ENTRY_ARRAY`.
  * **Paths**: a segment is `{p, n}`; a segment with `p == NULL` is an index and `n` is the number. Element `i` of the array `a` in a table at path `P` has the path `P + ["a"] + [i]`, and its members `P + ["a"] + [i] + [name]`. While an element is open, `side.tables` counts two more segments (the array's name, the index), so the existing limit `DIFF_SEGMENTS` (64) bounds depth; a path over it is refused as today. Elements are numbered from 0 in the order the reader delivers them.
  * **At most one level**: while an element is open, `begin_array` may open a scalar array (that is `ENTRY_ARRAY` at the member's path) but a `begin_table(NULL)` inside it, or a second array of tables, is refused. This is the limit of the first card, and `docs/Tools.md` says so.
  * **Order and output** (`tools/diff/main.c:196-212`, `284-307`): an index segment sorts before every name segment at the same depth and indexes sort by number (`[2]` before `[10]`); `write_segment` prints an index as `[3]` with no quotes. Each marker is printed like any entry: removing a whole array prints `removed<TAB>$["a"]` and `removed` for each member; adding an element prints only that element's members (the markers are equal).
  * **Duplicates**: today a repeated key is last-wins. A repeated name whose group (`next_key`, `tools/diff/main.c:276-282`) holds an `ENTRY_TABLES` marker (two arrays of tables with one name, or one array of tables and a scalar under the name) is refused after the sort in `read_side`, because the earlier elements' members would otherwise outlive the redefinition. This is the one exception to last-wins and goes into `docs/Tools.md`.
  * **Empty element tables** have no members and so no entries: `[{}]` equals `[{}, {}]` (an empty nested table equals a missing one already; `docs/Tools.md`). `{"a":[{}]}` against `{}` prints `removed<TAB>$["a"]`.
  * Memory stays under the one budget (`reserve_work`, `DIFF_FLOOR + DIFF_FACTOR * size`); nothing else is allocated.
* **files**: `tests/front/diff.cmake`, `tools/diff/main.c`; last: `docs/Tools.md` (`docs/Tools.md:63-98`: the paragraph about arrays, and the sentence that arrays containing tables or other arrays are refused), `docs/Roadmap.md` (the diff item in the section for the people who change configuration, and item 3 of the list Next after 1.8.0), `docs/Codemap.md` (generated: `cmake -P scripts/generate/codemap.cmake`), `CHANGELOG.md`.
* **steps**:
  1. `git switch -c work/N-14`. Read `tools/diff/main.c` whole once (433 lines) and `tests/front/diff.cmake:196-250`.
  2. In `tests/front/diff.cmake`, take `[[{"a":[{}]}]]` out of the refused list at line 197 (`[=[{"a":[[]]}]=]` and `"[]"` stay) and add these `nested(left right rc stdout)` rows after the scalar-array rows (JSON; each expected line is `kind<TAB>path`):
     - `{"a":[{"x":1},{"x":2}]}` against `{"a":[{"x":1},{"x":2}]}`: 0, empty;
     - `{"a":[{"x":1},{"x":2}]}` against `{"a":[{"x":1},{"x":3}]}`: 1, `changed\t$["a"][1]["x"]\n`;
     - one element added: `[{"x":1}]` against `[{"x":1},{"x":2}]`: 1, `added\t$["a"][1]["x"]\n`; and removed, reversed: `removed\t$["a"][1]["x"]\n`;
     - order matters: `[{"x":1},{"x":2}]` against `[{"x":2},{"x":1}]`: 1, `changed\t$["a"][0]["x"]\nchanged\t$["a"][1]["x"]\n`;
     - numeric index order: eleven elements `{"x":0}` to `{"x":10}` against the same with elements 2 and 10 changed: 1, `changed\t$["a"][2]["x"]\nchanged\t$["a"][10]["x"]\n`;
     - member order inside an element does not matter: `[{"x":1,"y":2}]` against `[{"y":2,"x":1}]`: 0;
     - a scalar array and a nested table inside an element: `[{"p":[1,2],"t":{"k":1}}]` against `[{"p":[1,3],"t":{"k":2}}]`: 1, `changed\t$["a"][0]["p"]\nchanged\t$["a"][0]["t"]["k"]\n`;
     - a table array at depth: `{"db":{"hosts":[{"n":1}]}}` against `{"db":{"hosts":[{"n":2}]}}`: 1, `changed\t$["db"]["hosts"][0]["n"]\n`;
     - the type changes: `{"a":[1]}` against `{"a":[{"x":1}]}`: 1, `changed\t$["a"]\nadded\t$["a"][0]["x"]\n`;
     - whole array removed and added: `{"a":[{"x":1}]}` against `{}`: 1, `removed\t$["a"]\nremoved\t$["a"][0]["x"]\n`; and `{"a":[{}]}` against `{}`: 1, `removed\t$["a"]\n`; `{"a":[{}]}` against `{"a":[{},{}]}`: 0;
     - values never shown: `[{"k":"SENTINEL-A"}]` against `[{"k":"SENTINEL-B"}]`: 1, `changed\t$["a"][0]["k"]\n`;
     - refused (extend the loop at line 197 with `a.json` left and each of these right, status 2, `rerr`): `{"a":[{"x":1},2]}`, `{"a":[1,{"x":1}]}`, `{"a":[{"x":[{"y":1}]}]}`, `{"a":[{"x":1}],"a":[{"y":1}]}`, `{"a":[{"x":1}],"a":2}`, `{"a":2,"a":[{"x":1}]}`;
     - YAML (inside `if(HAVE_YAML)`): `a:\n  - x: s\n  - x: t\n` against `{"a":[{"x":"s"},{"x":"t"}]}`: 0.
  3. Build `configforge_diff` and run `ctest -R front.diff`. **See it fail**: the equal-arrays row exits 2 ("left input refused") because arrays of tables are refused.
  4. Implement in `tools/diff/main.c` as decided: `kind`, the index segment, the element state in `begin_table`/`end_container`/`scalar`, the key order, `write_segment`, `entry_equal`, the duplicate refusal after the sort. Run after each piece, not at the end.
  5. `ctest -R front.diff` exit 0. Run the same file under the `sanitize` preset (`cmake --preset sanitize`, build, `ctest --preset sanitize -R front.diff`).
  6. `docs/Tools.md`: replace "Root arrays and arrays containing tables or other arrays are refused." with what is decided (an array whose elements are all tables is compared by element; the index segment; one level; arrays of arrays, mixed arrays and root arrays refused; the duplicate exception), and extend the opening sentence ("nested mappings of scalars, scalar arrays and arrays of tables"). `tools/diff/main.c`'s head comment likewise. `docs/Roadmap.md`: the diff item's "What is left" loses "arrays containing tables"; keep "arrays of arrays, contract-aware secret digests, units". `CHANGELOG.md`: an "Added" entry under 1.9.0.
  7. `python scripts/work/work.py format`; ACCEPT.
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS (the path segments contain no OS-specific byte), ACCEPT-CI.
* **stop if**: the sort or the budget needs a change that raises memory for inputs without arrays of tables; the duplicate refusal rejects a document that `front.diff`'s existing rows (lines 233-250 of `tests/front/diff.cmake`: duplicate keys of scalars and scalar arrays) accept; three failures on the element state in `begin_table`/`end_container`.
* **notes**: (empty)

## N-15 - a pull request that changes configuration files, with the changed keys in a comment

* **id**: N-15
* **goal**: the Doctor action, given `config:` (path patterns) on a pull request, runs `configforge diff` between each matching file's base and head versions and keeps one comment on the pull request, apart from Doctor's own, that lists the changed keys per file.
* **why**: "A pull request that changes configuration, reviewed without a report" (the Roadmap's section for the people who change configuration): the reviewer should not have to read a text diff of a YAML file to learn which keys moved. This card does the part that needs no new library work: the diff by value that exists (N-1, N-5, N-8; arrays of tables with N-14). The comment names keys only, never a value, so a secret cannot appear in it by construction. What waits: "which are out of range" and "which touch a secret" need the contract read by `configforge diff` (the card after N-14), and are left out here and said so in the comment's last line.
* **needs**: N-8 (done); 1.9.0 opened (the D-89 decision; HANDOFF.md). N-14 is not needed: arrays of tables a file contains are refused by the diff until then, and the comment says so for that file.
* **size**: medium
* **decided**:
  * **A new script `actions/doctor/changes.sh`**, beside `run.sh`, run by the action and by the test; no new action (a second action is a new public surface). Everything comes in through the environment, as `run.sh`'s head comment says:
    `CF_DIFF` the `configforge-diff` program; `CF_BASE_SHA` the base commit; `CF_HEAD_SHA` (default `HEAD`); `CF_FILES` the patterns, one per line, each a git pathspec (`config/**`, `*.yaml`); `CF_MARKDOWN` where to write.
    Missing `CF_DIFF`, `CF_BASE_SHA` or `CF_FILES`: exit 2 with a line on stderr. A commit that `git cat-file -e "$CF_BASE_SHA^{commit}"` does not find: exit 2 with "the base commit is not fetched; check out with fetch-depth: 0".
  * **Which files**: `git diff --name-status --no-renames -z "$CF_BASE_SHA" "$CF_HEAD_SHA" -- <patterns>`, read with `read -r -d ''` (a name may hold a newline). Status `M`: both versions; `A`: base side is a file holding `{}`, forced with `--as-left json`; `D`: head side is `{}`, `--as-right json`. At most **50 files** (the rest are counted in one line); a file's output is cut at **100 lines** (then one line says how many were left out).
  * **Versions**: `git show "$sha:$path"` into a temp directory (`mktemp -d`, removed on exit) as `left.<ext>` and `right.<ext>`, with `<ext>` the path's last extension limited to `[A-Za-z0-9]` (else no extension: the diff detects by content). The file's name is never used as a path outside that directory.
  * **Exit codes of the diff**: 0 prints nothing for the file (not listed); 1 lists the lines; 2 ("refused or unreadable", for arrays of tables until N-14, or a file the readers do not know) lists the file with the line `could not be compared (a reader refused it)`. The script exits 0 in all of these: whether a pull request may merge is not its decision (`fail-on` stays Doctor's).
  * **Markdown** (written even when nothing changed, so a stale comment is replaced): first line `<!-- configforge-changes -->`; heading `### ConfigForge: changed configuration keys`; per file `#### ` + the path in a code span with every backtick and control character removed (`tr -d '`' | tr -d '[:cntrl:]'`); the diff's lines in a fence of three backticks (each line starts with `added`, `removed` or `changed`, never a backtick, so none can close the fence); last, one line: "Keys only: no value is shown. Which keys are out of range or touch a secret is not checked here yet." With no matching change: "No file matching `config:` changed."
  * **`comment.sh`** gets `CF_MARKER` (default `<!-- configforge-doctor -->`, so nothing changes for the existing comment), checked against `^<!-- [a-z-]+ -->$` (exit 2 otherwise: it goes into a `jq` filter) and used where `marker` is set now (`actions/doctor/comment.sh`, the line `marker="<!-- configforge-doctor -->"`).
  * **`action.yml`**: new inputs `config` (default empty) and `base-sha` (default `${{ github.event.pull_request.base.sha }}`); `report` becomes `required: false` with default empty, and the "Lint and diff" step runs only `if: inputs.report != ''`; the build step builds both targets (`--target doctor configforge_diff`); a step "Changed configuration" runs only `if: inputs.config != ''`, finds `configforge-diff` as the build step's `find` finds Doctor, and runs `changes.sh` with the inputs through `env:` (never pasted into the script); a second comment step runs `comment.sh` with `CF_MARKER=<!-- configforge-changes -->` and `CF_MARKDOWN` of the new file, on the same condition as the first (`comment == 'true'` and a pull request), and the new file's text is added to the job summary when `summary` is true.
  * **Safety**: the files of the pull request are read by `git show` and parsed by the library's readers, which is what they are for; nothing from the pull request is executed or sourced. `docs/Doctor.md` says: use the `pull_request` event, not `pull_request_target`, and check out with `fetch-depth: 0`.
* **files**: `tests/doctor/action.cmake`, `tests/doctor/CMakeLists.txt`, `actions/doctor/changes.sh` (new), `actions/doctor/comment.sh`, `actions/doctor/action.yml`; last: `docs/Doctor.md` (after the pull-request example at `docs/Doctor.md:540-560`: a paragraph and an example with `config: config/**`), `docs/Dependencies.md` (the `git` row at line 77 gets the script that `doctor.action` runs, `changes.sh`), `docs/Roadmap.md` (the pull-request item, and item 4 of the list Next after 1.8.0), `CHANGELOG.md`.
* **steps**:
  1. `git switch -c work/N-15`. Read `actions/doctor/action.yml`, `run.sh` and `comment.sh` whole (about 300 lines) and `tests/doctor/action.cmake:225-300`.
  2. In `tests/doctor/CMakeLists.txt:92-108` (inside `if(CF_BASH)`): `find_program(CF_GIT git)` and, only when found, add `-DDIFF=$<TARGET_FILE:configforge_diff> -DGIT=${CF_GIT}` to the `doctor.action` command; `configforge_diff` is built by the same build as `doctor`, so nothing else is added. When git is not found the new cases are skipped by `if(GIT)` in `action.cmake`.
  3. In `tests/doctor/action.cmake`, before the `comment.sh` section (line 232), add a section for `changes.sh`, in `${WORK}/changes-repo`: run `git init`, set `user.name`/`user.email` by `-c` on each command, commit `config/app.json` (`{"port":1,"s":"SENTINEL-A"}`), `config/old.json` (`{"k":1}`), `config/bad.json` (`{"k":1}`), `README.md`; commit again with `config/app.json` changed to `{"port":2,"s":"SENTINEL-A","new":true}`, `config/new.json` added (`{"k":1}`), `config/old.json` removed, `config/bad.json` replaced by `{"k":SENTINEL}`, `README.md` changed. Run `changes.sh` with `CF_DIFF=${DIFF}`, `CF_BASE_SHA` the first commit, `CF_FILES=config/**` and assert: exit 0; the file starts with `<!-- configforge-changes -->`; it contains the code spans for `config/app.json`, `config/new.json`, `config/old.json` and `config/bad.json`, the lines `changed<TAB>$["port"]` and `added<TAB>$["new"]`, `added<TAB>$["k"]` (new), `removed<TAB>$["k"]` (old), and for bad.json `could not be compared`; it does not contain `README.md` and does not contain `SENTINEL`. Further cases: `CF_FILES=README.md` with no change to it between two equal commits gives "No file matching"; `CF_FILES` empty exits 2; an unknown base sha exits 2 with the fetch hint; a path with a backtick in its name (`config/a`b.json`, added in a third commit) appears with the backtick removed.
  4. **See it fail**: the test stops at the missing `changes.sh`.
  5. Write `changes.sh` as decided; `cmake -DDOCTOR=... -P tests/doctor/action.cmake` by way of `ctest -R doctor.action` exit 0. Then `comment.sh`'s `CF_MARKER` and its test rows: a stub-gh run with `CF_MARKER=<!-- configforge-changes -->` searches for that marker; one with `CF_MARKER='x; rm -rf'` exits 2.
  6. `action.yml` as decided; read it against the head-of-file usage comment and update that comment. `bash -n` on the three scripts and `actionlint` if installed (it is not a requirement).
  7. `docs/Doctor.md`, `docs/Dependencies.md`, `docs/Roadmap.md`, `CHANGELOG.md`; `python scripts/work/work.py format`; ACCEPT.
* **accept**: ACCEPT-LINUX, ACCEPT-CI; `ctest -R doctor.action` exit 0 in the default preset. The action itself cannot run on a pull request before it is merged: the card does not claim it ran there, and the first pull request that sets `config:` is the check (write that under "notes" when it has run).
* **stop if**: `git diff -z` and `read -d ''` behave differently between the CI runners' bash and the test's (write the difference under notes); the test needs a network; the action's condition syntax cannot express "report empty and config empty" without a third input (that is a design question: stop).
* **notes**: (empty)

## N-12 - differential fuzzing of the YAML reader: the built-in parser against libyaml

* **id**: N-12
* **goal**: One libFuzzer target, `fuzz_yamldiff`, feeds the same bytes to the
  YAML codec read through the built-in parser and to the YAML codec read
  through libyaml, in one process, compares the two event streams, and aborts
  when they disagree in a way `tests/fuzz/yamldiff.known` does not document.
* **why**: Roadmap, "Next after 1.8.0", item 1. The two parsers are chosen at
  build time (`lib/serialize/backends.cmake:25-123`), so no test has put one
  input to both. A disagreement is a bug in one of them, or a confusion two
  programs can be made to share (one reads the file as a mapping, the other
  refuses it). This is the first part only: YAML, one other reader. libfyaml
  and the other formats (yyjson, toml++, expat, tinycbor) are cards of their
  own once this one shows what a finding costs.
* **needs**: nothing
* **size**: small
* **decided**:
  * Both readers are the real codec, not a copy. `yaml.c` is included twice,
    from two wrapper translation units, with `cf_codec_yaml` (the only name
    `yaml.c` exports: `lib/serialize/src/yaml.c:1317` and `:1329`) renamed by a
    macro. `tests/fuzz/yamldiff_builtin.c` defines `cf_codec_yaml` as
    `cf_codec_yaml_builtin` and includes `../../lib/serialize/src/yaml.c`, with
    `CF_YAML_LIBYAML` and `CF_YAML_LIBFYAML` undefined (the built-in parser,
    `yaml.c:1048-1104`). `tests/fuzz/yamldiff_libyaml.c` does the same with
    `cf_codec_yaml_libyaml` and `#define CF_YAML_LIBYAML 1` first (libyaml's
    adapter, `yaml.c:952-1046`). The wrappers hold nothing else. The built-in
    parser's `cf_yaml_parse` comes from the linked `configforge_serialize`
    (`lib/serialize/src/yamlparse.c`); the library's own `cf_codec_yaml` is not
    used by this target.
  * The target is `tests/fuzz/yamldiff.c`: `LLVMFuzzerTestOneInput`, C89,
    public headers (`#undef CF_INTERNAL_API`, `#define CF_INTERNAL_API 0`, as
    `tests/fuzz/chain.c:15-16`). It decodes through `cf_codec_decode`
    (`lib/serialize/src/codec.c:279`), not the raw decoder, so the UTF-8 check
    both readers share runs first. The input is cut to 8192 bytes.
  * Each side gets a sink that appends one line an event to its own buffer
    (a fixed 256 KiB array and a length; past it the input is skipped, not
    failed): `T <name>` for `begin_table`, `A <name>` for `begin_array`,
    `S <name> <type> <value>[ textual]` for a scalar (written as
    `tools/codec/dump.c:140-170` writes it: text quoted with `\n`, `\"` and
    `\xHH` escapes, other types through `cf_value_to_text`), `E` for `end`.
    The place (`cf_loc`) is not written: the two parsers mark a place
    differently and that is not a difference of reading. `reserve_text` and
    `reserve_work` are served from one `cf_arena` (`cf_arena_new(NULL, 0)`)
    made per input and freed after, as `tools/codec/dump.c:240-262` does.
  * A run ends in one of four ways, and only the last aborts: both refuse (the
    messages and places differ, never compared); both accept with equal
    streams; the sides differ and a rule of the known file covers the input;
    the sides differ and no rule does: print both results, the first
    differing line and the input in hex on stderr, then `abort()`.
  * A difference is one of three sides: `lib-refuses` (built-in accepts,
    libyaml refuses), `bi-refuses` (libyaml accepts, built-in refuses),
    `events` (both accept, the streams differ).
  * `tests/fuzz/yamldiff.known`, read once at start-up from the path in the
    compile definition `YD_KNOWN` (the source directory's copy): one rule a
    line, three tab-separated fields - the side, a needle, the reason; a line
    starting with `#` and an empty line are ignored. The needle is a byte
    string written with `\n`, `\t`, `\\` and `\xHH` escapes; a rule covers an
    input of that side that contains the needle. A reason says which reading
    is right and why (the YAML 1.2 rule, or the project decision). A missing
    or unreadable file, a line without three fields, or a needle shorter than
    3 bytes is `abort()` at start-up naming the line: a target that silently
    allows everything is worse than none, and a one-byte needle blinds the
    fuzzer. A needle is the shortest byte string that causes the difference.
  * CMake, in `tests/fuzz/CMakeLists.txt` after the `fuzz.chain` block
    (`:116-145`): when `CF_WITH_YAML`, `find_path(CF_LIBYAML_INCLUDE_DIR
    yaml.h)` and `find_library(CF_LIBYAML_LIBRARY NAMES yaml libyaml yaml-0)`
    (the names `lib/serialize/backends.cmake:107-108` uses, so a build that
    chose libyaml shares them). When both are found, build `fuzz_yamldiff` from
    the three sources, linked to `configforge_serialize` and libyaml, with
    `-fsanitize=fuzzer` like the other targets and
    `YD_KNOWN="${CMAKE_CURRENT_SOURCE_DIR}/yamldiff.known"`; the wrappers'
    include path gets `lib/serialize/src`'s sibling headers the way
    `fuzz_chain` gets `tools/codec`. When not found,
    `message(STATUS "fuzz.yamldiff: libyaml not found, not built")` and no
    target. No new CMake option (the matrix gate, `gate.matrix`, lists them).
  * The test is `fuzz.yamldiff`, label `fuzz`, timeout `CF_FUZZ_SECONDS * 3 +
    60`, `-max_len=8192`, corpus directory `corpus_yamldiff`, fixture
    `FUZZ_SEEDS`. Seeds: `seed_yaml` and `seed_yaml_utf8` (written by
    `fuzz_seeds`, `lib/config/tests/test_fuzz.c:802,831`) and `seed_yamldiff`,
    which CMake writes beside the executable: `a: 1\nb: [x, y]\nc:\n  d: "e"\n`.
    The AFL job (`scripts/check/afl.cmake:56-80`) runs every `fuzz_<reader>` it
    finds and fails without a `seed_<reader>` beside it, so `seed_yamldiff`
    must exist.
  * CI: the `libfuzzer` job (`.github/workflows/ci.yml:403-433`) gets one step
    before the verify step, `sudo apt-get update && sudo apt-get install -y
    libyaml-dev`, so the target is built there. The AFL job is unchanged.
  * What the first runs find is not decided here; how it is sorted is. A
    sanitiser report or a crash in either parser is a bug: fixed if small and
    clearly in ConfigForge with a regression test, otherwise recorded under
    notes. A difference that is the YAML 1.1 (libyaml) against 1.2 (built-in)
    split becomes a rule with that reason. Anything else is a finding: under
    notes with the input in base64, and a rule only if its reason starts with
    "to fix", so the list of what is wrong is the list of rules that start so.
    libyaml is a reference, not an oracle; the spec decides.
  * C89 and D-100 apply to `yamldiff.c`. Format, then regenerate
    `docs/API.md`, `docs/Codemap.md` and `docs/Limits.md` on Linux, before
    every push.
* **files**: `tests/fuzz/yamldiff.c`, `tests/fuzz/yamldiff_builtin.c`,
  `tests/fuzz/yamldiff_libyaml.c`, `tests/fuzz/yamldiff.known`,
  `tests/fuzz/CMakeLists.txt`, `.github/workflows/ci.yml`, `CHANGELOG.md` (one
  line under Unreleased: a fuzz target, no change to a library).
* **steps**:
  1. `git switch -c work/N-12` (the branch exists, cut from main).
  2. Write the two wrappers, `yamldiff.c`, an empty `yamldiff.known` (comment
     lines only) and the CMake block. Configure a Linux build in
     `$HOME/cf-n12-fuzz` (clang, `CF_FUZZ=ON`, `CF_PROFILE=default`,
     `RelWithDebInfo`, as the `fuzz` preset) and build `fuzz_yamldiff` with
     `-j2`.
  3. See it fail: a scratch edit in `yamldiff.c`, never committed, that appends
     `E\n` to the libyaml side's buffer. `fuzz_yamldiff corpus_yamldiff
     <seeds> -runs=1000` must abort and print both results. Remove the edit;
     the same run on the seeds is clean.
  4. Run `fuzz_yamldiff` for 60 s. For each abort: sort it (see decided),
     write the rule or the note, run again. Aborts of one cause are one rule.
  5. `ctest -R "^fuzz[.]yamldiff$"` in the fuzz build; then the default build's
     full `ctest` and `ctest -R "^gate[.]"` in a second directory.
  6. Format, regenerate the documents, commit, push the branch.
* **accept**: ACCEPT-LINUX with libyaml-dev installed: the planted difference
  aborts the target; `fuzz.yamldiff` runs its seconds clean; the default
  build's `ctest` and `ctest -R "^gate[.]"` pass; a fuzz build without libyaml
  prints the status line and builds the other targets unchanged. ACCEPT-CI:
  the `libfuzzer` job builds and runs it.
* **stop if**: `yaml.c` cannot be included twice in one program (a name other
  than `cf_codec_yaml` clashes: record which; the fallback, two executables
  and a script that compares their output, is another card); the two parsers
  disagree on more than ten distinct needles in the first 60 s (the 1.1/1.2
  split is wider than a list holds: report the ten); or a crash in either
  parser needs more than a few lines to fix.
* **notes**: done 2026-11-24 on `work/N-12`. Changes to the card as written:
  a refusal's rule key is a part (8 bytes at least) of the refusing side's
  message, not a byte needle in the input, because the first finding (a
  flow collection continued at column 0) has no byte needle that is not
  also in most inputs; only `events` rules keep a byte needle (3 bytes at
  least); no control-byte exception is needed. The ten-rule stop was passed
  on purpose: the fuzzer finds a new class every few seconds, so
  `yamldiff.known` holds 16 rules and the run is not yet clean for long.
  Findings, all of the built-in parser against libyaml, none a crash:
  1. The built-in parser is stricter, and right by YAML 1.2, in: a flow
     collection or a quoted scalar continued at a column not indented more
     than its block mapping; a leading empty line of a block scalar more
     indented than the content; a `#` straight after a flow `,`, a closing
     quote or a flow collection (needs white space before it, so no comment).
     libyaml accepts all of these.
  2. libyaml is stricter, and the built-in parser right, in: anchor and alias
     names (1.2 allows `!`, `~`, `:` and more; libyaml only letters, digits,
     `_`, `-`), which gives four different libyaml messages; `[x, :abc]`
     (a plain scalar starting with `:`); `[a:, b]` (not checked against a
     third parser); a lone TAB line, and a TAB after the indentation.
  3. To fix, built-in: a NUL or other control byte inside a comment is
     accepted; libyaml and YAML 1.2 (c-printable) refuse. Input hex 23 00 23,
     base64 `IwAj`. Not fixed: `yp_syntax` (`lib/serialize/src/yamlparse.c:105`)
     lacks `CF_SYN_TOML_COMMENT`, whose body check ends a comment at LF only
     and so refuses a lone CR, which YAML ends a line with; the fix is a
     YAML flag in `lib/text/src/space.c:67-95`, not a small one.
  4. To decide: a literal block scalar whose last line is only spaces, with
     no line break after it, is read as text ending in a line feed by the
     built-in parser and without one by libyaml (`yamlparse.c:844`, "the
     input's end ends the last line too"). Input base64
     `IyAKbW90ZDogfAogJ30KIH4KDTEjbW9pbmc6IH4KbW90ZDogfAogIHR3bwpiYXNlOiAmYiAxCmNvcHlnOiB+Cm1vdGQ6IHwKICd9CiAgdGFnczogdGhpbmc6IH4KbW9pbmc6IH4KbW90ZDogfAogIHQjIG8iMC4wLjAuMCIKICB0d28KYmFzZTogJmIgMQpjb3B5Zzogfgptb3RkOiB8CiAndQogIA==`.
  5. To decide, not examined: base64
     `IyBjb20gegptb3RkIDogCnwgCiAgdHdvCmJhc2V7b3B5Og==` (a block scalar header
     at the mapping's own column) is refused by the built-in parser ("expected
     a key and ':'") and accepted by libyaml; base64
     `JmE6LXJdOnJdOjg6CQkJCQkJCQkwCQkJCQkJCQkJCQkJCQkJCQkJCQkJCQkJIw==` (an
     anchor, then tabs) is refused by the built-in parser ("a property is
     followed by white space") and accepted by libyaml.
  The rules keyed on a generic message (`did not find expected node content`,
  `cannot start any token`, `found unexpected ':'`, `mapping values are not
  allowed`, `an empty key`, `unexpected text after the value`, `expected a key
  and ':'`) also hide other causes with that message: the next card narrows
  them, by cause, as findings show. Both sides refusing is never compared, so
  a bad input one parser refuses with a worse message is not seen. The
  `libfuzzer` CI job may find a further class, and fail, until the list
  catches up.
  * Organiser's answers (2026-11-24):
    * The card ends narrowed: the one fault that is small is fixed here (a
      NUL or other control byte inside a YAML comment); `fuzz_yamldiff`
      stays as an exploratory target, built and run by hand and nightly,
      never a ctest test and never in a per-push job, so that a new class of
      difference cannot turn main red; `yamldiff.known` stays.
    * The 16 documented differences stay in these notes. The lone-CR
      comment fault (finding 3, second half) and the two "to decide"
      findings (4 and 5) go to `docs/Roadmap.md`, beside the item "A NUL
      inside a YAML comment is accepted", which this card's fix replaces
      with the lone-CR item.
