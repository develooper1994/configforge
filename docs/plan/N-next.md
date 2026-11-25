# N - the next cards, for 1.9.0

Four cards from the Roadmap's "Next after 1.8.0": what a refusal's reason
says about how serious it is (N-13), `configforge diff` over arrays of tables
(N-14), the changed keys of a pull request that touches configuration
files only (N-15), and Git's `textconv` driver (N-16). Each is its own
branch and lands alone. I-2, which opens
1.9.0, comes first: the four need it.

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

## N-16 - Git's `textconv` driver: `git diff` and `git log -p` show a configuration file by value

* **id**: N-16
* **goal**: with one `[diff "configforge"]` stanza and a few `.gitattributes` lines, `git diff`, `git log -p` and `git show` print each JSON, JSON5, YAML, TOML, INI, properties or CBOR file as one line per key (`port<TAB>int<TAB>8123`), so a reformat, a comment or a re-indent shows nothing and a changed value shows one `-` line and one `+` line; a file the readers refuse is printed as it is, and `git diff` never fails because of it.
* **why**: "Git's `textconv` driver for configuration" (Roadmap, item 8 of the list Next after 1.8.0). N-3 gave `git difftool` for flat JSON (keys only, one tool at a time, opted into by command); a textconv driver makes the everyday commands show a change by value in every format the library reads, with no new binary: `configforge codec dump` already reads every format and prints one line a key, and what is missing is the form Git needs (one file in, text out, never a failure, no line numbers that move when a line is inserted).
* **needs**: N-3 (done); 1.9.0 opened (the D-89 decision; HANDOFF.md)
* **size**: medium
* **decided**:
  * **No new binary and no new library function.** The reuse is `configforge codec` (`tools/codec/`, linked to `configforge_serialize` alone) and the front door that runs it. One new command, `configforge codec textconv <file> [--as <format>]`, built from `dump.c`'s sink (`tools/codec/dump.c:27-34` the context, `:82` `dp_open`, `:142` `dp_scalar`, `:198` `dp_args`, `:220-267` `codec_dump`). `dump` itself cannot be the driver: its first column is the reader's place (`app.json:1:13`), which moves whenever a line is inserted, and it exits 1 on a refusal, which stops Git (verified with Git 2.56: a textconv that exits non-zero ends `git diff` with `fatal: unable to read files to diff`, exit 128). `dump`'s own output and exit codes do not change: `tests/codec/dump.cmake` is the proof and stays as it is.
  * **The line format** (the command's output for a readable file), in document order, one line each, fields separated by a tab. A table or array opened: `<path>`, then `table` or `array`. A value: `<path>`, the type name (`cf_type_name`), the text as `dump` writes it (a string quoted, with `\\`, `\"`, `\n`, `\r`, `\t` and `\xNN` written out so that no value starts a line of its own), and ` (textual)` after it when the reader said the value was untyped text. The path is `dump`'s (names joined by `.`, an array member by its index from 0, `.` for the document itself, `...` after a path too long for its buffer). That is `dump`'s line without its first column, and without the `-` that stands in for a place on a table or an array. For `{"port":8123,"name":"a","tags":["x","y"]}` the output is these six lines (tabs between fields):

        .       table
        port    int     8123
        name    text    "a"
        tags    array
        tags.0  text    "x"
        tags.1  text    "y"

    A key that contains a dot and the same path as a nested table (`{"a.b":1}` against `{"a":{"b":1}}`) give the same path: the text is for a person to read in a diff, not to parse back, and the page says so. **Not sorted**: the order is the file's, so the command streams and holds nothing but the path; a JSON file whose keys are only reordered shows as moved lines, as in a plain diff. The order-insensitive comparison stays `configforge diff` (the N-3 difftool). A sorted form is a later card if it is wanted; it needs the whole output held, which this card does not.
  * **Which reader**: `codec_read` (`tools/codec/detect.c:77`), as `dump` does: `--as <format>` names it, else the file's extension, then its content (D-73). Git keeps the file's extension on the temporary file it hands a driver (verified with Git 2.56: `git-blob-XXXXXX/app.json`), so the stanza needs no driver per format and no `--as`. Which files get the driver is chosen by the `.gitattributes` lines (below); the reader is chosen as above.
  * **What the command does when the file is not readable as its format**, so that Git never fails:
    1. `codec_read` returns 0: pass one decodes the whole document into the same sink with output muted (a `mute` field of the context; every `fputs`, `fputc` and `fwrite` to `stdout` in `dump.c` goes through three small helpers that do nothing when it is set, and `dp_quoted` takes the context as a parameter), so a refusal in the middle of the file prints nothing of its readable beginning. If pass one succeeds, pass two decodes again with output on and `plain` set (the field that drops the place column and the `-` lead), and the command exits 0.
    2. Pass one fails (the reader refused the bytes), or `codec_read` returns 1 (the format cannot be told: two readers that read it differently, or none): the file's own bytes go to standard output unchanged (`blob.p`, `blob.n`, which `codec_read` has set before it decides a format), exit 0. The reason is one line on stderr, the one the codec already writes (`codec: cannot tell the format of <file>; name it with --as`, or `codec: <the reader's message>`); Git shows stderr to the user and does not make it part of the diff. Nothing else is added: no marker line on stdout, since a marker would be a change in the diff that is not in the file.
    3. `codec_read` returns 2 (the file cannot be read, `--as` names no format, or names one this build left out), or the arguments are wrong: exit 2, stdout empty, the usage or the reason on stderr, as `dump` does. Git dies on this, which is right: it is a broken setup, not a refused file.
    4. Pass two failing after pass one succeeded cannot happen (same bytes, same reader): exit 3 with `codec: textconv: the second reading failed (a bug)`, as `convert` uses 3 for what must not happen.

    Standard output is set to binary mode on Windows before the first byte (the `#ifdef _WIN32` with `_setmode(_fileno(stdout), _O_BINARY)` of `tools/codec/recode.c:16-19` and `:133-135`), for the lines and for the raw bytes alike: no `\r` is added, and a CRLF file is passed as it is.
  * **Secrets.** Today `configforge diff` prints keys and never a value or a digest (`docs/Tools.md`, "configforge diff": "Scalar values and digests never appear in either stream"). `codec dump` prints values as the reader gave them, and has no contract, so it cannot know a secret. A textconv that printed keys only could not show a value changing (the two texts would be equal), which is the point of the card, so this one follows `dump`: **values appear, exactly as the file holds them, and nothing is redacted or digested.** This opens no door that was shut: `git diff` without the driver already prints the same values from the same blobs, and anyone who can run `git log -p` can read the file. The page says this in one sentence, says that `git diff --no-textconv` is the plain diff, and names the way to review without values: the N-3 difftool recipe (keys only). Contract-aware redaction is what "contract-aware secret digests" in the Roadmap's `configforge diff` item is for, and is not this card. A test pins the behaviour (`SENTINEL` values appear in the converted lines), so a later change to it is a decision and not an accident. `cachetextconv` is not set, and the page says why: Git's cache is keyed by the blob and not by this program's version, so a fix to the output would be shown stale from the cache.
  * **The stanza** (`docs/Tools.md`, new section "### Git textconv for configuration", after "### Git difftool for JSON"):

        [diff "configforge"]
          textconv = configforge codec textconv

    (in the page it is a fenced block tagged `gitconfig`, as the difftool's is). With `configforge` on `PATH` (the front door runs `configforge-codec` from the same directory, as it runs `configforge-diff` for N-3's recipe). Git runs it as `configforge codec textconv <temporary file>`. Added by `git config --local diff.configforge.textconv "configforge codec textconv"` or by editing the local config, as the page says for the difftool.
  * **The `.gitattributes` lines**, in a fenced block tagged `gitattributes` (a test reads it from the page and writes it): one line per format the built-in codecs read, by the extensions of `lib/serialize/src/codec.c:322-336`: `*.json`, `*.json5`, `*.jsonc`, `*.yaml`, `*.yml`, `*.toml`, `*.ini`, `*.properties`, `*.cbor`, each followed by ` diff=configforge` (nine lines, no others). Left out on purpose, and the page says so: `*.conf` and `*.cfg` (the library reads them as INI, but most such files are another program's syntax, and a file read as INI by luck would be shown as nonsense instead of as text), `*.xml` (rarely configuration, and a build may lack expat) and `*.jsn`. A path-specific line (`etc/app.conf diff=configforge`) opts a file in; a file the reader refuses is shown raw, so a wrong line costs a readable diff and never a failed one. The page also says: `-diff` on a path turns the driver off for it; `.gitattributes` is committed and the `[diff]` stanza is local config, so a clone needs the stanza once.
  * **Not in this card**: sorted output; digests or redaction (they need the contract); a `configforge git setup` command that writes the stanza; Doctor's report as a textconv input; `git difftool` (N-3 stays as it is); merge drivers.
  * **Exit codes of `textconv`**: 0 printed (the lines, or the file as it is), 2 the arguments or the file, 3 a bug. It never returns 1: a refusal is exit 0 by design, and a test holds it (`tests/codec/textconv.cmake`).
* **files**: `tests/codec/textconv.cmake` (new), `tests/codec/CMakeLists.txt:28`, `tests/front/git_textconv.cmake` (new), `tests/front/CMakeLists.txt:32-41`, `tools/codec/dump.c`, `tools/codec/codec.h:24-25`, `tools/codec/codec.c` (the header comment at 3-14, the usage text at 26-52, the dispatch after `dump`'s at 76-78); last: `docs/Tools.md`, `CHANGELOG.md`, `docs/Roadmap.md`, `docs/Codemap.md` (generated: `cmake -P scripts/generate/codemap.cmake`), `docs/API.md` (generated; staged only if `scripts/generate/apiref.cmake` changes it), `docs/Plan.md` (the index row).
* **steps**:
  1. A worktree of its own from `origin/main` (`git worktree add .claude/worktrees/n16 -b work/N-16 origin/main`; the card's writer made it, and the carrier continues in it). Read `tools/codec/dump.c` whole (267 lines), `tools/codec/detect.c:77-125` (`codec_read`), `tests/codec/dump.cmake` whole, `tests/front/git_difftool.cmake` whole (76 lines) and `docs/Tools.md:98-130`.
  2. **The codec's test first.** Write `tests/codec/textconv.cmake` (the shape of `tests/codec/dump.cmake`: arguments `-DCODEC -DWORK -DJSON -DINI -DYAML`; `message(STATUS "skipped: json is not in this build")` and `return()` when `JSON` is off) and add `textconv` to `cf_codec_scripts` at `tests/codec/CMakeLists.txt:28` (the loop there passes the flags). A helper `tc_case(<exit> <stdout regex or -> <stderr regex or -> <arg>...)` like `dump_case`, which keeps the stdout in `tc_out`. Fixtures written into `${WORK}`: `a.json` = `{"s": {"p": 8080}, "t": ["x"], "n": null, "q": "a\nb"}` (the `\n` inside the JSON string is the two characters backslash and n); `c.ini` = `[s]\nk = v\n`; `y.yaml` = `a: 1\n# comment\nb: [x]\n`; `bad.json` = `{"a": }` (no line feed); `half.json` = `{"p": 1, "q": }`; `x.txt` = `{"a":` (no reader reads it); `blob` = `{"k":1}` (no extension: JSON by content). Cases:
     a. `textconv a.json`: exit 0; stdout equals exactly `.\ttable\ns\ttable\ns.p\tint\t8080\nt\tarray\nt.0\ttext\t"x"\nn\tnull\tnull\nq\ttext\t"a\\nb"\n` (the text of `q` is quoted, and its line feed written as backslash and n); stdout contains no `a.json:` and no `\r`; stderr empty.
     b. `textconv --as json blob` and `textconv blob`: exit 0, stdout `.\ttable\nk\tint\t1\n`.
     c. `textconv c.ini` (only `if(INI)`): a line `s.k\ttext\t"v" (textual)`. `textconv y.yaml` (only `if(YAML)`): lines `a\tint\t1` and `b.0\ttext\t"x"`, and nothing from the comment.
     d. **A refusal is not a failure**: `textconv bad.json`: exit 0, stdout is byte for byte the file (`{"a": }`), stderr matches `codec:`; `textconv half.json`: exit 0, stdout is exactly the file and has no `p\tint` (the readable beginning is not printed); `textconv x.txt`: exit 0, stdout `{"a":`, stderr matches `cannot tell the format`. Compare through `file(READ ... HEX)` on both sides, so a stray `\r` is seen.
     e. **Not a refusal**: `textconv nosuch.json`: exit 2, stdout empty, stderr matches `cannot read`; `textconv --as nosuch a.json`: exit 2, stdout empty; `textconv` and `textconv a.json b.json`: exit 2, stderr matches `textconv <file>`.
     Build `configforge_codec`; `ctest -R codec.textconv`.
  3. **See it fail**: the run stops at case a (`textconv` is not a command: the usage, exit 2). Write the first failing line into the report.
  4. Implement in `tools/codec/dump.c`: add `cf_bool plain` and `cf_bool mute` to `dp_ctx`; three helpers `dp_out_s(const dp_ctx*, const char*)`, `dp_out_c(const dp_ctx*, int)` and `dp_out_n(const dp_ctx*, const char*, size_t)` that write to `stdout` unless `c->mute`, replacing every `fputs`, `fputc` and `fwrite` to `stdout` in the file (`dp_quoted` takes the context); in `dp_open` and `dp_scalar` leave out the `-\t` and the place column when `c->plain`; the block of `codec_dump` that builds the sink and calls `cf_codec_decode` becomes a helper both commands call (factored out, not copied); `int codec_textconv(int argc, char** argv)` as decided above, taking its arguments with `dp_args` and its usage line as `codec: textconv <file> [--as <format>]`; `#include <fcntl.h>` and `<io.h>` under `_WIN32` as `recode.c:16-19`. Declare it in `tools/codec/codec.h` after `codec_dump` with the one-line comment style of that file. In `tools/codec/codec.c`: the dispatch branch after `dump`'s, the header comment line `configforge codec textconv <file> [--as <format>]`, and the usage text: a `textconv` entry and, since it is the one command the usage leaves out, a `dump` entry, each in the two-line style of its neighbours (`tests/codec/usage.cmake` needs only `configforge codec formats` to stay). C89: no `//`, declarations first. `ctest -R "codec\."` exit 0, `codec.dump` among them, unchanged.
  5. **The Git test first.** `tests/front/git_textconv.cmake`, shaped on `tests/front/git_difftool.cmake` (its environment lines 10-20 and its `setup_git`; `commit` gets `-c user.name=t -c user.email=t@example.invalid -c commit.gpgsign=false`). Arguments `-DGIT_EXECUTABLE -DFRONT_DIR -DTOOLS_PAGE -DWORK -DHAVE_YAML`. It reads the page (`file(READ "${TOOLS_PAGE}" page)`): the stanza by `string(REGEX MATCH "\\[diff \"configforge\"\\]\n[ \t]+textconv = ([^\n]+)" ...)`, keeping `${CMAKE_MATCH_1}` as `tc`; the fence by `string(REGEX MATCH "```gitattributes\n([^`]*)```" ...)`, writing `${CMAKE_MATCH_1}` to `.gitattributes`; either one missing is `FATAL_ERROR "missing ... in docs/Tools.md"`. In a scratch repository (`core.autocrlf false`, the empty excludes file, `GIT_CONFIG_GLOBAL` empty, `PATH` holding `${FRONT_DIR}`, all as the difftool test does) it commits, as version 1: `app.json` = `{"port":8123,"s":"SENTINEL-A","tags":["x","y"]}`, `app.yaml` = `port: 8123\ns: SENTINEL-A\n` (only when `HAVE_YAML`), `bad.json` = `{"k":1}`, `plain.txt` = `one\n`, `.gitattributes`; and as version 2: `app.json` re-indented over several lines, `port` 9123, `s` SENTINEL-B, the other keys as they were, in the same order; `app.yaml` = `# a comment\nport: 9123\ns: 'SENTINEL-A'\n`; `bad.json` = `{"k":\n`; `plain.txt` = `two\n`. Then a helper `run_git(<args>...)` runs `git -c "diff.configforge.textconv=${tc}" <args>` and fails the test if the exit is not 0, returning stdout and stderr. The cases:
     a. `diff HEAD~1 HEAD -- app.json`: stdout matches `(^|\n)-port\tint\t8123\n`, `(^|\n)\\+port\tint\t9123\n`, `(^|\n)-s\ttext\t"SENTINEL-A"\n` and `(^|\n)\\+s\ttext\t"SENTINEL-B"\n`; it does not match `(^|\n)[-+]tags` (an unchanged key is context at most: a re-indent is not a change), does not match `"port"` (no line of the raw file) and contains no `{`.
     b. the same for `app.yaml` (only `HAVE_YAML`): the port lines present; `(^|\n)[-+]s\t` absent (a changed quoting and a comment are not changes).
     c. `diff HEAD~1 HEAD -- bad.json`: **exit 0, not 128**; stdout matches `(^|\n)-k\tint\t1\n` and `(^|\n)\\+\\{"k":\n` (the refused version is shown as it is); stderr matches `codec:`.
     d. `diff HEAD~1 HEAD -- plain.txt`: no driver on it: `-one` and `+two`.
     e. `log -p --format=%s -- app.json`: both `+port\tint\t8123` (the first commit, from nothing) and `+port\tint\t9123`.
     f. A change in the working tree, not committed (`app.json` with port 9124): `diff -- app.json` shows `-port\tint\t9123` and `+port\tint\t9124` (the driver is given a real path here, not a temporary one).
     g. `diff --no-textconv HEAD~1 HEAD -- app.json` matches `"port"` (the plain diff is one flag away; the page says so).
     The values `SENTINEL-A` and `SENTINEL-B` appearing in (a) is the decided behaviour; the test's comment says so. Register it in `tests/front/CMakeLists.txt` inside `if(Git_FOUND)` (lines 32-41), after `front.git_difftool`, as `front.git_textconv` with `-DGIT_EXECUTABLE=${GIT_EXECUTABLE} -DFRONT_DIR=$<TARGET_FILE_DIR:configforge_front> -DTOOLS_PAGE=${PROJECT_SOURCE_DIR}/docs/Tools.md -DWORK=${CMAKE_CURRENT_BINARY_DIR}/git_textconv -DHAVE_YAML=${CF_WITH_YAML}`, under the condition `TARGET configforge_codec` added to that test only (no test is added when git is absent, as for the difftool test: that is the "skipped"). `ctest -R front.git_textconv`.
  6. **See it fail**: it stops at `missing ... in docs/Tools.md`. Then write the page. In `docs/Tools.md`, after line 129 (the end of "### Git difftool for JSON"): the section "### Git textconv for configuration" with the stanza, the `gitattributes` block, a short example (the `port` change printing two lines) and the sentences decided above: values are shown as the file holds them; `--no-textconv`; the difftool is the keys-only, order-insensitive one; a refused file is shown as it is with the reason on stderr; the dotted-path ambiguity; unsorted; `cachetextconv` not set, and why; the extensions left out and the path-specific line; the stanza is local config and `.gitattributes` is committed. In the codec part of the same file: the `textconv` entry in the usage block (lines 137-165, after `dump`'s at 148), a **textconv.** paragraph after the `dump` paragraph and its example (before "### Converting between formats", line 213), and the word `textconv` in the table row at line 60. `ctest -R "front\.|codec\."` exit 0.
  7. `CHANGELOG.md`: under `## 1.9.0 — unreleased`, "### Added" (line 8), one entry: "`configforge codec textconv <file>`, a Git `textconv` driver: `git diff`, `git log -p` and `git show` print JSON, YAML, TOML, INI, properties and CBOR files one line per key, so a reformat shows nothing and a changed value shows one line; a file the readers refuse is shown as it is (N-16; docs/Tools.md)." `docs/Roadmap.md`: the bullet that starts at line 1389 ("Git shows configuration changes by value") goes from 🟨 to 🟩, and its sentence "The `textconv` driver for `git diff` and `git log -p` remains open." (about line 1392) becomes "The `textconv` driver for `git diff` and `git log -p` is built (card N-16: `configforge codec textconv`; values shown, unsorted, a refused file shown as it is). What is left: sorted output, and contract-aware redaction."; at item 8 of "Next after 1.8.0" (line 1491) add "(card N-16)" after the title, as items 2 to 4 have. `docs/Plan.md`: the card's index row stays `open` (set `done (<hash>)` at the landing).
  8. `python scripts/work/work.py format`. On Linux (copy the worktree off `/mnt` with tar as `/mnt/d/cf-work/i2-build.sh` does): `cmake -P scripts/generate/codemap.cmake` and `cmake -P scripts/generate/apiref.cmake` last, then copy `docs/Codemap.md` (and `docs/API.md` if it changed) back; ACCEPT-LINUX with `-DMODE=quick`, then `ctest -R "^gate\."`. `python scripts/work/work.py win . N-16` on Windows. Push `work/N-16`; ACCEPT-CI. Do not land.
* **accept**: ACCEPT-LINUX (quick; the organiser runs the full one at the landing), `ctest -R "codec\.|front\.git"` exit 0 on Linux and in `work.py win`, `ctest -R "^gate\."` exit 0 on Linux, ACCEPT-CI. The Windows run matters here: text-mode `\r` and Git's use of `sh` for the driver are Windows behaviours, and CI's Windows job runs this test.
* **stop if**: Git's driver, run through the shell on a CI runner, cannot find `configforge` on `PATH` as the difftool test's can (write the runner and the error under notes: the stanza would need an absolute path, which is a design question); a Git on a runner does not keep the extension of the temporary file (the format would then need a driver per format, `textconv = configforge codec textconv --as json`: a design question, stop); `codec.dump` changes its output after the refactor of `dump.c` (revert the refactor and report; do not edit `tests/codec/dump.cmake`); a reader keeps state between two decodes of one blob so that pass two differs from pass one (name the format under notes).
* **notes**: 2026-11-25 (carrier): step 5's `app.yaml` version 2 had `s: 'SENTINEL-A'` and the card said a changed quoting is no change; the YAML reader reports a plain scalar as untyped text (`(textual)`) and a quoted one as text, so quoting is a change `textconv` rightly shows. Decided by the card's own rule (a re-indent and a comment are no change): version 2 keeps `s: SENTINEL-A` unquoted, and the page says quoting shows. YAML values read as `text`, not `int`, so case b matches `[a-z]+` for the type.

## N-17 - `configforge doctor drift`: many reports, grouped by value

* **id**: N-17
* **goal**: `configforge doctor drift <report>... [--key K] [options]` reads two or more Doctor reports (one a host) and, for every key on which they do not all agree, prints one finding that groups the reports by what they hold: `DRIFT warning log.level differs: 'debug' in web-3.json; 'info' in web-1.json, web-2.json, web-4.json, web-5.json`. A secret's value is never printed; the exit status is 0 when every key agrees and 1 when one does not.
* **why**: The Roadmap's "Drift across many hosts" ("is this set the same on a hundred servers"). `doctor diff` answers it for two reports; `history` follows one service through time and does not say which of N hosts is the odd one. With one report a host (from `cf_report`, or the `import-*` adapters), the question is one command whose answer a CI job can fail on. The parts that wait, each its own card: the source of a value is not compared (`doctor diff` says CFD002 for a pair); a list of reports longer than the shell passes (`--list`); the Python binding's `drift`; the Action; and `configforge diff` over N configuration files (the Roadmap says "N reports or files": this card is the reports).
* **needs**: 1.9.0 opened (the D-89 decision; HANDOFF.md)
* **size**: medium
* **decided**:
  * **A new subcommand of Doctor, not a mode of `diff`.** `diff` is a pair with a "here" and a "there" (`rule_value_diff`, `tools/doctor/engine.c:1639-1676`: `"%s here, %s there"`) and also runs the pair's other rules (CFD002, CFD003, CFD012, CFD017); a set has neither side. `history` and `blame` are the model: the positional reports up to the first option, `--key`, the same options and formats (`tools/doctor/doctor.c:500-533`, the option loop at 536-575). `drift` is parsed beside them (`is_drift`; reports from argv[2] to the first `--` argument; `--key` accepted as for `history`). Fewer than two reports is the usage and exit 2: a glob that matched one file is a mistake, not an answer. It is not `configforge diff`, the other tool (`tools/diff`: files, entries by path): that reads configuration files, this reads reports.
  * **A new public function, `CF_DOCTOR_API`, not `CF_EXPERIMENTAL(1, 9)`.** D-89's marker is for the 1.x ABI: `CF_EXPERIMENTAL(major, minor)` expands to `CF_API` (`lib/port/include/configforge/cf_port.h:260`), and `gate.abi` then requires the name in `cmake/configforge.experimental.symbols` and in `libconfigforge`. Doctor's library is static and outside that ABI (`cmake/gates/GateAbi.cmake:35-36`, "Doctor's CF_DOCTOR_API is not here"; D-41); its header declares `extern` (`tools/doctor/include/configforge/cf_doctor.h:42`), and `CF_API` there would import the symbol from a DLL it is not in. So: `CF_DOCTOR_API cf_bool cf_doctor_drift_files(cf_doctor* d, const char* const* paths, size_t n, const char* key, cf_error* err);` after `cf_doctor_blame_files` (`cf_doctor.h:137-138`), its comment saying it came in 1.9.0. `gate.abi` does not see it (its token needs a line that starts with `CF_API` or `CF_EXPERIMENTAL(`), and no symbols file changes. The command line is that function with argument parsing and printing around it, as `docs/Doctor.md` section 7b says of every command.
  * **What is compared.** For one key each report is in one of three states: *absent* (no such key in it), *unset* (the key is there and no source won), or a *value* (the winning candidate, `winner()`, `engine.c:1464`). The value only: the source is not compared, a default counts as a value, `at` is not compared. Two reports are in one group when both are absent, both are unset, or both hold a value and `values_agree` (`engine.c:1522-1535`) says `AGREE_YES`: shown values as text, hidden ones by digest, as `diff` and `history` do. Absent against a value is a difference (CFD012 is about two contracts and is not repeated).
  * **Grouping**, C89, no allocation per key: first fit in argument order. A report joins the first group whose first member it agrees with, else it opens a group. At most `DRIFT_GROUPS` 16 groups a key: the 17th stops the grouping, and the finding says `differs: more than 16 distinct values in N reports`, naming no report and no value (a key that is different on every host, a hostname, is one finding and not a page). Storage: `of`, one `unsigned` a report (its group), allocated once for the run, and two 16-element arrays (`first`, `size`) on the stack.
  * **Not comparable.** If the first members of two groups give `AGREE_UNKNOWN` (one report has a digest and the other none to match, or the digests were made under different keys: `digests_apart`, `engine.c:1497-1505`), the key is not claimed to drift: one `CFD020` `info` finding (the rule exists, `docs/Doctor.md:382`) with the sentence `whether the reports agree cannot be told: one holds only a digest and another has none of its kind to match`. Every pair of first members is compared, when the later one opens its group, so none is missed.
  * **One finding for each drifting key**, rule `DRIFT`, level `warning`, the key, no place (`finding`, not `finding_at`: the reports disagree about where). The message is `differs` (or `differs (by digest)` when any report's key is sensitive or any winner has no value; else `differs (not shown: --redact-all)` when that option is on: the order `rule_value_diff` tests in), then `: ` and the groups, smallest first (ties in order of first appearance), separated by `; `. A group is its description, ` in ` and its reports, `, `-separated, at most `DRIFT_NAMES` 5 and then ` and N more`. The description is `'<value>'` when the value may be shown, `value N` when it may not (N counts the groups of that key from 1, in order of first appearance), and for the other two states `absent from` and `set by no source in`, written in place of ` in `. Reports are named by the path as given. Smallest first means that a message cut at 511 bytes (`dfinding.msg` is 512, `tools/doctor/engine.h:122`; `cf_fmt` cuts) loses the end of the biggest group, not the odd host. A value is written as `diff` writes it (`%s`) and goes through the same printers: control characters are made visible by `text_field` (`doctor.c:39-60`), and Markdown and SARIF escape as they do for every finding.
  * **Severity and exit.** `DRIFT` is `warning`, as CFD002 and CFD003, the other drifts, are. `doctor diff`'s convention is every command's: exit 1 when a finding reaches `--fail-on` (`doctor.c:644-651`), 2 for a report that cannot be read, a key none of the reports has, or the usage (`usage()` ends with `return 2`, `doctor.c:411`; a failed read `doctor.c:599` and `605`), else 0. `--fail-on` is `error` by default (`doctor.c:507`), and then a `warning` would exit 0: the one question this command asks would have no answer in its status. So for `drift` only the default is `warning` (`fail_on = is_drift ? "warning" : "error"`); `--fail-on never` and `info` work as everywhere (`CFD020` is `info`: `--fail-on info` fails a run on a set that cannot be compared). `--baseline` accepts `DRIFT log.level` and `--severity` moves the rule, as for any identifier (`docs/Doctor.md:384-386`). Formats: Doctor has `text`, `sarif` and `markdown`, no JSON (`doctor.c:564-568`); `drift` has those three. SARIF's artifact is the first report (`print_sarif(D, argv[pos])`, in place of `doctor.c:616`).
  * **Secrets.** Never printed: a sensitive key is `value N` in every format, from a full report or a blind one, and `--redact-all` does it for every key. Nothing is shown that `diff` does not show for a pair.
  * **Limits.** At most `DRIFT_MAX_REPORTS` 1024 reports: more is `CF_E_SOURCE` "at most 1024 reports" and exit 2, before any file is read. Every report is held whole while the run lasts, as `reports_load` holds them for `history` (`engine.c:2353-2385`; one arena each, `doctor_load`, `engine.c:390`): the peak is the files' total size times the reader's overhead, and nothing else grows with the number of keys. Room for the findings is reserved first (`found_reserve`, `engine.c:789`; the pass runs once to count and once to record, as `history_pass` does), so no finding is lost half way. Time is (keys in the union) x (reports) lookups: a key is taken from the first report that has it, and the test "does an earlier report have it" stops at the first that does, so reports with one key set cost one lookup a key. A longer list than the shell passes (about 300 names on Windows) is not in this card.
  * **Order of findings**: the union's keys, each placed with the first report that has it, in that report's key order.
  * **Not in this card**: the source of a value, a `--list` file, the Python binding (`bindings/python/src/configforge_doctor.py`), the Action, a summary line "N of M reports differ" (the findings say it), and any change to the report schema.
* **files**: `tests/doctor/drift.cmake` (new), `tests/doctor/CMakeLists.txt`, `tests/doctor/test_api.c`, `tests/doctor/marker.cmake`, `tests/doctor/secrets.cmake`, `tests/doctor/redact.cmake`, `tools/doctor/engine.c`, `tools/doctor/include/configforge/cf_doctor.h`, `tools/doctor/doctor.c`; last: `docs/Doctor.md`, `docs/Roadmap.md`, `docs/API.md` and `docs/Codemap.md` (generated: `cmake -P scripts/generate/apiref.cmake`, `cmake -P scripts/generate/codemap.cmake`), `CHANGELOG.md`.
* **steps**:
  1. The branch `work/N-17` exists (this card's commit is on it): continue on it. Read `tools/doctor/engine.c:1496-1535`, `1902-2085` and `2340-2416` (the sequence code this card sits beside), and `tools/doctor/doctor.c:500-651`.
  2. `tests/doctor/test_api.c`, after `test_history` (line 794; helpers `seq_write` at 721 and `SEQ_KEY` at 739 write reports by hand): add `test_drift` and call it after `test_history()` in `main`. Four reports `cf_drift_1.json` to `cf_drift_4.json`: key `port` `"80"`, `"80"`, `"81"`, `"80"` from `env`; key `token` with `"digest":"aaaa"` and no value in 1, 2 and 4, `"digest":"bbbb"` in 3; key `only` (`"value":"x"`) in 1 and 2 only; key `same` `"v"` in all four. Checks: `cf_doctor_drift_files(d, paths, 4, NULL, &err)` is true and `count_rule(d, "DRIFT", NULL) == 3` (`port`, `token`, `only`; not `same`); the `port` message is `differs: '81' in cf_drift_3.json; '80' in cf_drift_1.json, cf_drift_2.json, cf_drift_4.json`; the `token` message starts `differs (by digest): value 2 in cf_drift_3.json; value 1 in` and holds neither `aaaa` nor `bbbb`; the `only` message is `differs: 'x' in cf_drift_1.json, cf_drift_2.json; absent from cf_drift_3.json, cf_drift_4.json` (two and two: the tie goes to the group that appeared first); every level is `warning`; with key `"port"` there is one finding; a key in none is false with `err.message` set; `n == 1` is false; `n == 1025` (an array of 1025 pointers to one path) is false with `at most 1024` in the message and no file read; a missing path in the middle is false and no finding is left (as `history`'s `cf_seq_none.json`, lines 797 and 910).
  3. `tests/doctor/drift.cmake` (new). It writes its own inputs under `${CMAKE_CURRENT_BINARY_DIR}/drift` and needs the fixtures only for case 12. Register it by adding `drift` to the script list at `tests/doctor/CMakeLists.txt:175-182`. A function `host(file level pass flag)` writes a report in the hand-written form, `{"configforge_report":1,"keys":{...}}`: `server.port` `"8080"`; `log.level` `<level>`; `db.password` `<pass>` with `"definition":{"type":"text","sensitive":true}` (a full report: the value is in the file); `feature.flag` `"on"` only when `flag` is not empty. A key is `{"candidates":[{"source":"env","value":"V"}],"selected":{"source":"env"}}` (`SEQ_KEY`'s shape). Hosts: `web-1.json` (info, MARKER-FLEET-ONE, on), `web-2.json` (info, MARKER-FLEET-ONE, on), `web-3.json` (debug, MARKER-FLEET-ONE, on), `web-4.json` (info, MARKER-FLEET-TWO, on), `web-5.json` (info, MARKER-FLEET-ONE, no flag). Doctor runs in that directory; each case checks the exit code and the text:
     1. `drift web-1.json web-2.json web-3.json web-4.json web-5.json`: exit 1, and these lines (regexes; `+` for the column spaces; any order): `DRIFT +warning +log\.level +differs: 'debug' in web-3\.json; 'info' in web-1\.json, web-2\.json, web-4\.json, web-5\.json`; `DRIFT +warning +db\.password +differs \(by digest\): value 2 in web-4\.json; value 1 in web-1\.json, web-2\.json, web-3\.json, web-5\.json`; `DRIFT +warning +feature\.flag +differs: absent from web-5\.json; 'on' in web-1\.json, web-2\.json, web-3\.json, web-4\.json`; and `3 new finding\(s\): 0 error, 3 warning, 0 info`; no `server.port`; no `MARKER`.
     2. The same with `--fail-on never`: exit 0. With `--fail-on error`: exit 0.
     3. `drift web-1.json web-2.json`: exit 0 and `0 new finding\(s\)`.
     4. `drift web-1.json web-2.json web-3.json --key log.level`: exit 1, one finding; with `--key server.port`: exit 0, none; with `--key no.such`: exit 2.
     5. Unusable input, exit 2 each: `drift`; `drift web-1.json`; `drift web-1.json absent.json` (and no `DRIFT` line in its output); `drift web-1.json web-2.json --fail-on bogus`; `drift web-1.json web-2.json --nope`.
     6. `--format markdown` and `--format sarif` over case 1's reports: exit 1, `DRIFT` and `log.level` present, `MARKER` absent.
     7. `--redact-all` over case 1's reports: exit 1; `log\.level +differs \(not shown: --redact-all\): value 2 in web-3\.json; value 1 in web-1\.json, web-2\.json, web-4\.json, web-5\.json`; neither `'debug'` nor `'info'` anywhere; no `MARKER`.
     8. A baseline file holding the line `DRIFT log.level`, and `drift web-1.json web-2.json web-3.json --baseline <it>`: exit 0 and `1 accepted by the baseline`.
     9. More than five reports in a group: eight files `many-1.json` to `many-8.json`, all `info` but `many-8.json`, `debug`: the output matches `'debug' in many-8\.json; 'info' in many-1\.json, many-2\.json, many-3\.json, many-4\.json, many-5\.json and 2 more`.
     10. More than sixteen groups: eighteen files `uniq-01.json` to `uniq-18.json`, eighteen different levels: exit 1 and `log\.level +differs: more than 16 distinct values in 18 reports`.
     11. The limit: 1025 copies of `web-1.json` named `c0001.json` and so on: exit 2 and `at most 1024` on stderr; the first 1024 of them: exit 0 and `0 new finding\(s\)`.
     12. Digests made under different keys, from `${work}` (the fixtures' directory): `drift cf_fx_keyed_a.json cf_fx_keyed_a2.json cf_fx_keyed_b.json`: exit 0 (the default `--fail-on warning`), a line `CFD020 +info +db\.password`, no `DRIFT`; with `--fail-on info` exit 1; and `drift cf_fx_keyed_a.json cf_fx_keyed_a2.json` alone: exit 1 and `db\.password +differs \(by digest\)`.
  4. `tests/doctor/CMakeLists.txt`: beside the `history` cases (lines 66-71) three `doctor_case` rows on the existing fixtures, so that `run.cmake` meets the command: `drift 1 drift|cf_fx_prod.json|cf_fx_staging.json`, `drift_never 0 drift|cf_fx_prod.json|cf_fx_staging.json|--fail-on|never`, `drift_unreadable 2 drift|cf_fx_prod.json|cf_fx_none.json`. In `tests/doctor/marker.cmake:78-88` (`templates`) add `"drift|@X@|@Y@|--format|@F@#@changed@"` and `"drift|@X@|@Y@|--key|@Q@|--format|@F@#-"`; in `tests/doctor/secrets.cmake:50-66` (`runs`) add `"drift|${a}|${b}|--fail-on|never"` and its `--format markdown` and `--format sarif` variants (the loop already demands exit 0 and no `MARKER`); in `tests/doctor/redact.cmake:48-59` add `"drift|${prod}|${staging}|--redact-all|--fail-on|never"` and its two variants. If a name of `marker.cmake`'s `changed` is not printed by `drift` (its two winners agree), stop: write the key and its two entries under notes, and do not edit `changed`.
  5. **See it fail**: build `doctor_api` and `doctor`; `doctor_api` stops on the undeclared function. Declare it (step 6) with a body that returns `CF_FALSE` with an error set; `ctest -R "doctor.api|doctor.drift"` now fails on the first check and on every `drift.cmake` case (Doctor answers `drift` with the usage, exit 2).
  6. `tools/doctor/include/configforge/cf_doctor.h`: after `cf_doctor_blame_files`, a comment in the file's style (a set of reports, in no order, one host's report each; came in 1.9.0; adds a `warning` finding, rule `DRIFT`, for each key on which they do not all agree, and what its message holds, never a secret's value; false with `err` set and nothing found when a report cannot be read, there are fewer than two or more than 1024, `key` is in none of them, or there is no memory) and the declaration, `CF_DOCTOR_API`.
  7. `tools/doctor/engine.c`: after `blame_key` (before "The library", the banner at line 2085) a section "A set of reports": `DRIFT_MAX_REPORTS`, `DRIFT_GROUPS`, `DRIFT_NAMES`; the state of a key in a report (absent, unset, value); `drift_same` (two states, by the rule above); `drift_key(rs, n, key, keylen, of, record)`, which groups and returns 0 (agree) or 1 (a `DRIFT` or `CFD020` finding, recorded when `record`); the message built into a 512-byte buffer with `cf_fmt` and `cf_fmt_append` (`lib/port/include/configforge/cf_port.h:436-437`: bounded, NUL-terminated, returning the length it would have needed), stopping when the length reaches the buffer's size; `drift_pass(rs, n, key, of, record)` over the union as decided; and `cf_doctor_drift_files` written as `cf_doctor_history_files` is (`engine.c:2398-2416`): `n < 2` and `n > DRIFT_MAX_REPORTS` refused before `reports_load`, a `key` in none refused as `key_in_any` does, `found_reserve` for the first pass's count, the second pass records, `reports_release`. `g_rules` (`engine.c:1040-1041`) gets `{"DRIFT", "A key the reports of a set do not all agree on"}` after `BLAME`. C89: declarations at the top of a block, no `//`, no `for (int ...)`. `ctest -R doctor.api` passes.
  8. `tools/doctor/doctor.c`: `is_drift` beside the others (`doctor.c:500-533`); `fail_on` per command; the reports parsed as for `history` and `blame` (position 2, no key before them) with `first - pos < 2` the usage; `--key` accepted for `history` and `drift` (`doctor.c:570`); the call beside `cf_doctor_history_files` (`doctor.c:589-603`); the first report as SARIF's artifact (`doctor.c:616`); the usage text (`doctor.c:366-398`: the line `configforge doctor drift <report>... [--key <key>] [options]`, and `--fail-on` as `error (default; drift: warning)`) and the head comment (`doctor.c:14-24`). `ctest -R doctor` exit 0.
  9. Measure, and write the numbers under notes (not a test): generate 1000 reports of 200 keys (a CMake script under `/mnt/d/cf-work`, one value in ten different), run `configforge doctor drift` over them with `/usr/bin/time -v` on Linux: wall time and peak memory. Stop if over 60 s or 512 MiB.
  10. `docs/Doctor.md`: the command list of section 7 (lines 216-229) gets `configforge doctor drift <report>... [--key K]` and two lines on what it says; the table of section 7b (lines 296-297) a row for `cf_doctor_drift_files`; a new section `### 7e. A set of reports` after 7c (before section 8, line 356) with the output of case 1 as Doctor really prints it, the grouping and the order in a paragraph, the three exit statuses and why `--fail-on` defaults to `warning` here, secrets, the limits (two or more, 1024) and what is not compared (the source of a value: `doctor diff` for a pair); the sentence at line 384 (`HISTORY` and `BLAME`) gains `DRIFT`, "a record of a set, and a `warning`"; line 502 says error by default "and warning for `drift`". `CHANGELOG.md`: an "Added" entry under 1.9.0. `docs/Roadmap.md`: the item "Drift across many hosts" (line 1402) becomes 🟨 ("What exists: `doctor drift` ... (N-17, 1.9.0). What is left: the source of a value, a list file for more reports than a shell passes, configuration files rather than reports (`configforge diff` over N files), the Python binding"), and item 7 of the list Next after 1.8.0 (line 1487) "Done in 1.9.0 (card N-17): ...; what waits is ...".
  11. `python scripts/work/work.py format`; then `cmake -P scripts/generate/codemap.cmake` and `cmake -P scripts/generate/apiref.cmake` last (Linux); ACCEPT.
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS (the file names and the 1025 copies are portable; no bash), ACCEPT-CI; `ctest -R "doctor|gate.abi|gate.apiref|gate.codemap"` exit 0 in the default preset; `ctest --preset sanitize -R doctor` too (the group arrays are the risk).
* **stop if**: `gate.abi` sees the new function or wants a symbols entry (then `CF_DOCTOR_API` is wrong for it: write what the gate said under notes and ask: it is this card's one open design point); step 9 is over its bounds; `marker.cmake`'s gate finds a marker in `drift`'s output (a leak is a fault to fix in this card, not an entry for `KNOWN_LEAK`); three failures on the grouping or the message text.
* **notes**: read cold against the tree on 2026-11-25 at 7574d95 (before N-14 and N-16 land; neither touches `tools/doctor` or `tests/doctor`). Order number 77 is used because 76 is N-16's, on its branch; if N-16 lands under another number, take the next free one at landing. The one choice that departs from the brief's wording is the function's marker (see "A new public function"); the alternative is no public function (a private `doctor_drift_files` in `engine.c`, declared in `engine.h`), which would break the sentence of section 7b that the command line is the library with printing around it. The old `build/` Debug `doctor.exe` predates the digest handling and prints a secret in `diff`: do not run it to check an expectation of this card.
  * Carried (organiser's answers): step 4's three `doctor_case` rows are named `drift_pair`, `drift_pair_never` and `drift_pair_unreadable`, because the script test `doctor.drift` (step 3) already takes the name `drift`; CMake refuses two tests of one name. Step 3's `drift.cmake` and step 2's `test_drift` are as written. The group order in `drift_message` is an insertion sort over `order[]` kept in the function (no `drift_work` member), and the 1025 copies of case 11 are made with `configure_file` in a loop (`string(REPEAT)` and `list(SUBLIST)` need a newer CMake than the 3.10 floor). Step 9, Linux default preset (unoptimised, GCC): 1000 reports of 200 keys, 20 MB in all, 20 of the keys differing on one host in seven: 4.0 s, 243 MB peak, 20 findings; inside the bounds (60 s, 512 MiB).

## N-20 - a lone CR ends a YAML comment: the rows that hold it

* **id**: N-20
* **goal**: the tests hold that a lone CR ends a YAML comment, and the Roadmap stops listing it as a fault.
* **why**: N-12's fuzzer found a lone CR inside a comment and the Roadmap recorded a fault in `yp_syntax` (a missing `CF_SYN_TOML_COMMENT`-style flag). Since N-12 `yp_comment` (`lib/serialize/src/yamlparse.c:143-168`) ends a comment at `'\n'` or `'\r'` (line 157) and refuses a control byte, so the fault was closed in passing; no row said so.
* **needs**: nothing
* **size**: small
* **decided**:
  * Accepted, in `test_yaml.c` (`test_cr_comment`, built-in parser and libyaml alike, keys checked): `"# c\ra: 1\r"` (a is 1), `"a: 1 # c\rb: 2\r"` (a is 1, b is 2), `"a: [1, # c\r 2]\r"` (a.0 is 1).
  * Refused with control-character, in `test_reason.c` after the `\x7f` comment row: `YAML_ROW("# c\x0b\r", CONTROL)` (a libyaml build reads it: the macro's `maybe` flag already covers that).
  * No behaviour change, so no CHANGELOG entry. If a row fails, the card stops with the failing row; the fix is its own card.
* **files**: `lib/config/tests/test_yaml.c`, `lib/config/tests/test_reason.c`, `docs/Roadmap.md` (the lone-CR item), `docs/Codemap.md` (generated).
* **steps**: 1. add the rows; 2. build and run `test_yaml` and `test_reason` (they pass: the fault is closed); 3. edit the Roadmap; 4. format, gates, push, CI.
* **accept**: ACCEPT-LINUX, ACCEPT-CI; `ctest -R "test_yaml|test_reason"` exit 0.
* **stop if**: a row fails.
* **notes**: decided and carried the same day (2026-11-25); the rows passed at once on Linux (default preset).
