# C - for the people who change configuration (continued)

Five of the Roadmap's items for the people who change configuration: a profile
chosen by name, a read of a key the contract does not have, a value migrated
and not only renamed, a running program asked for its report, and a program's
files where its platform keeps them. The cards C-1 to C-6 (diff of arrays of
arrays, durations and sizes, the PR comment's contract, drift's remainder,
textconv, help and completions) are written at the same time by another hand;
none of these cards needs one of them. Each card is read cold
against the tree before it opens (docs/Plan.md, "Read again after the layout
changes"); a line number below is the tree of 2395d65.

Shared by every card here:

* A new function of the library comes in as `CF_EXPERIMENTAL(1, 9)` (D-89),
  not as `CF_API`: the shape of what a first user needs is often only right
  after it. The card says which names, and `cmake/configforge.experimental.symbols`
  gets them from `cmake -DSRC=. -DWRITE=ON -P cmake/gates/GateAbi.cmake` (the
  diff of that file and of `cmake/configforge.internal.symbols` is checked
  against the names the card lists). A function the tests call and no program
  should is declared `CF_API` inside `#if CF_INTERNAL_API` (D-51, D-88) and
  lands in the internal list the same way.
* A new `.c` file is added to its component's `SOURCES` and to
  `scripts/generate/amalgamate.py:62-81` (one translation unit: every `static`
  name in it carries the file's prefix, so that it cannot meet another file's).
* The closing steps of every card, in this order, on Linux: `python
  scripts/work/work.py format`; `cmake -P scripts/generate/codemap.cmake`;
  `cmake -P scripts/generate/apiref.cmake`; `git add -A`. Where a card adds a
  `.c` file to `lib/`, also `ctest --preset misra -R gate.misra` where
  cppcheck is installed: a report in the new file is fixed, or recorded as
  `backlog` by the gate's own `-DWRITE` (D-112), and the diff of
  `docs/compliance/deviations.txt` is part of the commit.
* The dev loop inside a step: `cmake --preset default && cmake --build --preset
  default -j && ctest --test-dir build -R <name> --output-on-failure`. The
  card is accepted by ACCEPT, not by this.

## C-7 - Profiles: `--profile` or `NAME_PROFILE` adds `config.<profile>.<ext>` above the base file

* **id**: C-7
* **goal**: a program that called `cf_builder_app_files` (C-11) and was started with `--profile production` (or with `MYAPP_PROFILE=production`) also reads `config.production.<ext>` beside each base file, ranked just above it; `cf_profile(cfg)` and the report's `profile` field say which profile it ran under.
* **why**: A program's environments (production, staging, a developer's machine) differ in a handful of keys, and the way to say so is a file per environment beside the base one, as .NET's `appsettings.Production.json` and Spring's profiles do. Today the report's `profile` is whatever the caller passes in `cf_report_info` (`lib/config/include/configforge/cf_report.h:48`), so a program that forgets it reports a configuration that does not say which environment it was: Doctor's profile-dependent severities (`tools/doctor/engine.c:637-724`) then never apply. The library knowing the profile makes the report the library's, not the caller's.
* **needs**: C-11 (the places, `cf_builder_app_files`, `cfi_app_places` with its `profile` parameter, `CF_RANK_USER_PROFILE` and `CF_RANK_SYS_PROFILE`, the builder's recorded environment)
* **size**: medium
* **version**: 2.1.0
* **status**: open
* **decided**:
  * **The profile is the text key `profile`.** `--profile NAME` and `--profile=NAME` on the command line and `<PREFIX>PROFILE` in the environment are that key's ordinary spellings (`lib/config/src/sources/cli.c:22-31` reads `--key value` for any non-boolean definition; the environment source maps `MYAPP_PROFILE` to `profile`), so it ranks CLI above environment like any key, and `explain("profile")` says which won.
  * **Found at the call, not at create.** The files must be added when `cf_builder_app_files` runs, before a source is read, so the profile is found then, by `cfi_app_profile` (new, `lib/config/src/sources/appfiles.c`), from the builder's own command line (`b->cli_argc`/`b->cli_argv`, set by `cf_app_builder`, `config.c:275-279`) and the environment the call uses. The rule: scan `argv[1..]` up to a `--`; `--profile=NAME` or `--profile` with the next argument is an occurrence, the last occurrence wins, an occurrence with an empty NAME is "no profile" and stops the search (an explicit empty value beats the environment); with no occurrence, `<prefix>PROFILE` of the environment (an empty value is "no profile"). The prefix is `b->prefix` when it is not empty (`config.c:61`), else `derive_prefix(name)` (`config.c:240-257`, `static` in the same file as `cfi_builder_app_files`, which passes the result in: `cfi_app_profile` never computes it).
  * **A name, not a path.** 1 to 32 bytes of `A-Z a-z 0-9 _ -`. Anything else fails the builder, held for `cf_builder_create` as every builder failure is (`builder_fail`, `config.c:183-189`), code `CF_E_POLICY`, message `cf_builder_app_files: '<text>' is not a profile name: letters, digits, '_' and '-', up to 32` (the text cut at 40 bytes). This is what keeps `--profile ../../etc/x` from naming a file.
  * **Which files.** For each place `cfi_app_places` returns with a profile (C-11: `config.<profile>.<format>` beside each base file, user at `CF_RANK_USER_PROFILE` = 19, system at `CF_RANK_SYS_PROFILE` = 29, and 29 + 3k for the k-th `$XDG_CONFIG_DIRS` entry: every source has a rank of its own, C-11), the source is added as the base files are: name and path the same, `CF_OPTIONAL`. A profile has no `conf.d`. Not one profile file replaces the base: both are read, the profile's wins by rank.
  * **The key is defined for the program.** If `cfi_builder_app_files` ran on a builder, `cf_builder_create` defines `profile` (text, no default, no flags, description `the configuration profile`) when the contract holds no definition of that key, so that `--profile` is never an unknown key under `CF_UNKNOWN_REJECT`. A contract that defines `profile` itself is left alone. The definition goes in right after the `b->failed` check (`config.c:1267-1269`), before `c->defs = b->defs` (`config.c:1297`).
  * **The library's profile.** `cf_config` gets `const char* profile` (arena copy; NULL when none; `config.c:101-149`). `CF_EXPERIMENTAL(1, 9) const char* cf_profile(const cf_config* c)` returns it (NULL for a NULL `c`, for none). `cf_report` (`config.c:2436-2446`) passes a copy of `info` (a copy of `CF_REPORT_INFO_INIT` when the caller's `info` is NULL) whose `profile` is `c->profile` when the caller's `info` is NULL or its `profile` is NULL; a profile the caller gives wins (a program that names its environments itself is not overridden). `cf_derive` keeps it (`derive_onto`, `config.c:1610-1714`, arena copy, beside `b->track_reads` at 1706).
  * **Rejected**: several profiles at once (`a,b`): not asked; the order would be a second precedence rule. A profile applying to `conf.d`: a drop-in directory is already the place for per-site overrides. Reading `config.<profile>` with a different format than the base: the format is the one given to `cf_builder_app_files`.
  * **D-120** (to be copied into `docs/Decisions.md` after D-112, with an index row `| D-120 | A profile is a name that adds a file above each base file | configuration, sources | **Decided** |`):

    ```
    ## D-120 — A profile is a name that adds a file above each base file

    **Question.** Programs run in environments that differ in a handful of
    keys. Where is that written, and who says which environment this run is?

    **Options.**

    1. Nothing: a program passes `--config <file>` and the operator keeps one
       file per environment. Rejected: the library cannot tell the report which
       environment it was.
    2. An overlay file per profile beside each base file, chosen by a key, as
       .NET and Spring do (decided).
    3. Several profiles at once, merged in order. Rejected: a second precedence
       rule, and not asked for.

    **Decision.** Option 2. The profile is the text key `profile`
    (`--profile NAME`, `<PREFIX>PROFILE`), a name of 1 to 32 bytes of letters,
    digits, `_` and `-`. For each place of `cf_builder_app_files` it adds
    `config.<profile>.<format>` at the base file's rank minus one (19 and 29), so
    a profile's file beats its own base file and nothing else changes order.
    The library keeps the name: `cf_profile` and the report's `profile`, which a
    caller's own `cf_report_info.profile` still overrides.

    **Consequence.** `profile` is a key every program that calls
    `cf_builder_app_files` has, defined for it unless it defines its own.
    ```
  * Out of this card: a profile on a builder that did not call `cf_builder_app_files` (a program adds its own `config.<profile>` file with `cf_builder_file`); the bindings.
* **files**: `lib/config/tests/test_profile.c` (new), `lib/config/tests/CMakeLists.txt`, `lib/config/src/sources/appfiles.c`, `lib/config/include/configforge/cf_source.h`, `lib/config/include/configforge/configforge.h`, `lib/config/src/core/config.c`; last: `cmake/configforge.experimental.symbols`, `cmake/configforge.internal.symbols` (generated), `docs/Decisions.md`, `docs/Guide.md`, `CHANGELOG.md`, `docs/Roadmap.md`, `docs/Codemap.md` and `docs/API.md` (generated).
* **steps**:
  1. Read, whole: C-11's "decided" and the tree it leaves (`lib/config/src/sources/appfiles.c`, `cfi_builder_app_files` in `config.c`), `config.c:48-99`, `config.c:1267-1300`, `config.c:1610-1714`, `config.c:2436-2446`.
  2. Write `lib/config/tests/test_profile.c` (C89, `#include "cf_test.h"`, guarded `#if CF_WITH_INI && CF_WITH_DIR`, else `main` returns `cf_test_done()` after printing `skipped`), added to `CF_UNIT_TESTS` after `test_appfiles` (`lib/config/tests/CMakeLists.txt`, the list that ends at line 83). Its fixtures are made by CMake, in the way C-11 makes its own: `${CMAKE_CURRENT_BINARY_DIR}/cf_profile.d/{sys1/myapp,home/myapp}` with `file(WRITE)`: `sys1/myapp/config.ini` = `[log]\nlevel = warn\n[server]\nhost = sys\n`; `sys1/myapp/config.prod.ini` = `[log]\nlevel = error\n`; `home/myapp/config.ini` = `[server]\nworkers = 3\n`; `home/myapp/config.prod.ini` = `[server]\nhost = prod-home\n`; and `target_compile_definitions(test_profile PRIVATE CF_PROFILE_DIR="${CMAKE_CURRENT_BINARY_DIR}/cf_profile.d")` after `cf_unit_tests(${CF_UNIT_TESTS})` (line 104). The cases, each a function:
     a. `cfi_app_profile` over fixed vectors: `--profile=prod` -> found "prod"; `--profile prod` -> "prod"; the last of two occurrences wins; `--profile=` (empty) beats `MYAPP_PROFILE=x` and gives none; after a `--` nothing counts; a `--profile` as the last argument is no occurrence; the environment alone: `MYAPP_PROFILE=staging` with prefix `MYAPP_`; neither: none (return 0); `--profile=../x`, a 33-byte name, `a b` -> invalid (return -1) with `out` holding the text.
     b. Builder level with `cf_app_builder("myapp", 2, argv)` (argv `{"app", "--profile=prod", NULL}`), then `cfi_builder_app_files(b, "myapp", "ini", plat, env)` where `plat` is `CF_APP_WINDOWS` under `#if CF_PLATFORM_WIN32` and `CF_APP_POSIX` otherwise, and `env` (for case f's environment spelling, also `MYAPP_PROFILE=../x` in a second array) holds `XDG_CONFIG_HOME=<CF_PROFILE_DIR>/home`, `XDG_CONFIG_DIRS=<CF_PROFILE_DIR>/sys1`, `APPDATA=<CF_PROFILE_DIR>/home`, `PROGRAMDATA=<CF_PROFILE_DIR>/sys1` (paths built into static buffers with `strcpy`/`strcat`; C89 has no `snprintf`). Then `cf_builder_text` for `server.host` (default `dflt`) and `log.level` (default `info`), `cf_builder_int` for `server.workers` (1, 1..64), `cf_builder_unknown(b, CF_UNKNOWN_REJECT)`. Expect: `server.host` is `prod-home` (user profile, rank 19, beats the system base), `log.level` is `error` (system profile, rank 29, beats the system base at 30), `server.workers` is 3; `cf_get_text(c, "profile", "")` is `prod`; `cf_profile(c)` is `prod`; `cf_explain(c, "server.host", ...)` names the profile file's path (`config.prod.ini`).
     c. The same builder without `--profile`: `cf_profile` is NULL, `server.host` is `sys`, `log.level` `warn`, and `cf_get_text(c, "profile", "none")` is `none`; and `--profile=prod` is not an unknown key under reject (the build of case b is the proof).
     d. `cf_report` with `info == NULL` and with `CF_REPORT_INFO_INIT`: the text holds `"profile": "prod"`; with `info.profile = "staging"` it holds `"profile": "staging"`; for a configuration with no profile it holds `"profile": null`.
     e. `cf_derive(c, "admin", &pair, 1, &err)` of case b's configuration: `cf_profile(d)` is `prod`.
     f. `--profile=../x`: `cf_builder_create` returns NULL and `err.message` holds `is not a profile name`; the same for the environment spelling.
     g. A contract that defines `profile` itself (`cf_builder_text(b, "profile", "dev", 0)` before `cfi_builder_app_files`): the build succeeds and `cf_get_text(c, "profile", "")` is `dev` when no profile is given.
  3. Add the declarations and bodies that do nothing: `cfi_app_profile` returning 0, `cf_profile` returning NULL (in `cf_source.h` under `#if CF_INTERNAL_API` as `CF_API int cfi_app_profile(int argc, char* const* argv, char* const* environment, const char* prefix, char* out, size_t cap)`, and in `configforge.h` after `cf_builder_app_files` as `CF_EXPERIMENTAL(1, 9) const char* cf_profile(const cf_config* c);`), so that the test links. **See it fail**: `ctest --test-dir build -R test_profile --output-on-failure` exits non-zero and names the failing checks of a, b, c, d, e, f and g.
  4. `cfi_app_profile` in `appfiles.c` as decided (statics prefixed `appf_`, as C-11's are).
  5. `config.c`: `b->profile_key` (cf_bool) and `b->profile` (arena copy) on `cf_builder`; `cfi_builder_app_files` (C-11) calls `cfi_app_profile` first, fails the builder on -1, passes the name (or NULL) to `cfi_app_places`, sets `b->profile_key`, `b->profile`; `cf_builder_create` defines `profile` and copies `b->profile` to `c->profile` (right after `c->defs = b->defs`); `derive_onto` copies the base's; `cf_profile`; `cf_report` as decided.
  6. `ctest --test-dir build -R test_profile` exits 0; `ctest --test-dir build -R "test_appfiles|test_rename|test_report|test_derive"` still exit 0.
  7. `docs/Decisions.md`: D-120 as decided, and its index row; `docs/Guide.md` section 4 ("Sources and precedence", `docs/Guide.md:224`), a paragraph "Profiles" under C-11's subsection: the key, the spellings, the two ranks, the one-line example `./myapp --profile production`, the report; `CHANGELOG.md`; `docs/Roadmap.md`.
  8. The closing steps (above), then ACCEPT.
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS and ACCEPT-CI as `docs/Plan.md:71-114` give them; `ctest --test-dir build -R test_profile` exit 0 on Linux, on Windows (the Windows branch of case b) and in the `minimal` preset (the file skips); `gate.abi` exit 0.
* **stop if**: C-11 has not landed or its `cfi_app_places` has no `profile` parameter (nothing is changed; the card waits); the scan of `argv` cannot be made to agree with what `cf_cli` reads for `profile` (a test of case b gives a different value from `explain("profile")`): nothing is changed, the case is written under notes; `cf_report` cannot take a profile without changing a report the existing tests pin (`test_report`, `test_report_edges`): stop.
* **docs**: `docs/Decisions.md` D-120 and its index row; `docs/Guide.md` as step 7; `CHANGELOG.md` under `## 1.9.0 — unreleased`, `### Added`: "`--profile NAME` or `NAME_PROFILE` (C-7): `cf_builder_app_files` also reads `config.<profile>.<ext>` beside each base file, ranked just above it; `cf_profile` (experimental in 1.9) and the report's `profile` are the library's"; `docs/Roadmap.md:818` the item "**Profiles.**": `⬜` to `🟩` and `(D-120, C-7)` after the bold heading.

## C-8a - A read the contract does not have, noticed: the library records it

* **id**: C-8a
* **goal**: a program that asked for it (`cf_builder_track_misreads`) can ask the configuration which of its reads found no definition, or a value of another type (`cf_misreads`, `cf_misread_at`), and the report lists them.
* **why**: `cf_get_int(cfg, "server.prot", 8080)` returns 8080 without a word, and so does reading a text key as an int; C++ and Rust catch both with typed keys and C does not. The value the program used is then not the value the configuration holds, and nothing in a report says so: `explain` and `doctor lint` look at what is configured, never at what is asked.
* **needs**: nothing
* **size**: medium
* **version**: 2.1.0
* **status**: open
* **decided**:
  * **Opt-in.** `CF_EXPERIMENTAL(1, 9) void cf_builder_track_misreads(cf_builder* b)`, beside `cf_builder_track_reads` (`configforge.h:434`); `CF_EXPERIMENTAL(1, 9) cf_bool cf_tracks_misreads(const cf_config* c)`. Off by default: a read costs one pointer test more (`c->misreads != NULL`) and nothing else. Tracking reads and tracking misreads are independent. A derived configuration tracks as its base did (`derive_onto`, `config.c:1706`, beside `track_reads`); the log it makes starts empty.
  * **What is a misread.** A read through `cf_get_bool`, `cf_get_int`, `cf_get_text`, `cf_get_text_n`, `cf_get_real`, `cf_try_bool`, `cf_try_int`, `cf_try_text`, `cf_try_real` (the last under `CF_WITH_REAL`), with `want` the type it asks for, of `key`; not `cf_has`, `cf_type_of`, `cf_explain`, `cf_report`, the iterator or `cf_derive`: those ask what is there, not for a value. In this order:
    1. `d = cf_defset_find(&c->defs, key, keylen)` is NULL, the configuration holds at least one definition (`c->defs.count > 0`: with no contract there is nothing to misread) and `cf_defset_is_open(&c->defs, key, keylen)` is false: kind `CF_MISREAD_UNDEFINED`, `found` = `CF_TYPE_NULL`. A key a source supplied and the contract does not define counts: the contract is what the program asked for.
    2. Otherwise, with `v` the selected value (`e->selected->value`) when there is one: `v` present and `v->type != want`, or `v` absent and `d != NULL` and `d->list_mode == CF_LIST_NONE` and `d->type != want`: kind `CF_MISREAD_TYPE`, `found` = `v->type` or `d->type`. This holds with a contract or without one (Tier 0 infers the type: `{"port", "9000"}` is an int, and `cf_get_text(c, "port", "")` is a misread).
    3. Nothing else: a defined key with no value, read with its own type, is not one; a list root (`d->list_mode != CF_LIST_NONE`) is not judged.
  * **The public reading.**
    ```c
    typedef enum cf_misread_kind { CF_MISREAD_UNDEFINED = 1, CF_MISREAD_TYPE = 2 } cf_misread_kind;
    CF_EXPERIMENTAL(1, 9) size_t cf_misreads(const cf_config* c);
    CF_EXPERIMENTAL(1, 9) cf_bool cf_misread_at(const cf_config* c, size_t i, const char** key,
                                                cf_misread_kind* kind, cf_type* wanted,
                                                cf_type* found);
    ```
    `cf_misreads` is the number of distinct (key, kind, wanted) recorded, 0 for a configuration that does not track or `NULL`. `cf_misread_at` gives the i-th in the order first recorded; false past the end or when not tracking; any out pointer may be NULL; `*key` is owned by the configuration, valid until it is freed. (An accessor by index, not a struct the library fills in the caller's memory: D-51's rule for such structs would have asked for a size and room for growth, and an experimental function is not the place to decide them.) Declared in `configforge.h` after `cf_read_count` (line 565).
  * **The log** (internal; `lib/config/include/configforge/cf_report.h`, a new section "Misreads" above `cfi_report_reads`, not exported):
    ```c
    #define CF_MISREAD_SLOTS 16
    typedef struct cf_misread_slot {
        cf_refcount   ready;   /* 0 until the slot is whole */
        unsigned char kind, wanted, found;
        char          key[CF_KEY_MAX]; /* cut at CF_KEY_MAX - 1 bytes */
    } cf_misread_slot;
    typedef struct cf_misread_log {
        cf_refcount     next; /* slots claimed */
        cf_misread_slot slot[CF_MISREAD_SLOTS];
    } cf_misread_log;
    ```
    in `lib/config/src/core/misread.c` (new): `void cfi_misread_record(cf_misread_log*, const char* key, size_t keylen, unsigned kind, cf_type wanted, cf_type found)`, `size_t cfi_misread_count(const cf_misread_log*)`, `const cf_misread_slot* cfi_misread_at(const cf_misread_log*, size_t i)`. **No lock, a read takes none (D-14).** `record`: (1) scan slots `0..min(next, 16)` that are `ready` for an equal (kind, wanted, key) and return when found; (2) return when `cf_ref_get(&next) >= CF_MISREAD_SLOTS` (the counter is never run past 16 by a hot loop: no overflow); (3) `i = cf_ref_inc(&next) - 1`, return when `i >= 16`; (4) write the slot's fields, then `cf_ref_inc(&slot.ready)`. Two threads recording one key at the same moment may fill two slots: `cfi_misread_count` and `cfi_misread_at` skip a ready slot equal to an earlier ready one, so the reading is exact. A seventeenth distinct misread is lost, and nothing says how many were (a program with seventeen has more to fix than a report's count would tell it). The log is `cf_arena_calloc`ed with `c->reads` (`config.c:1552-1559`) when `b->track_misreads`, one pointer, `c->misreads`.
  * **The report.** `cfi_report_reads` (`report.c:424`, and its stub for a build without `CF_WITH_REPORT`, `report.c:859`; declared `cf_report.h:71-73`) becomes `cfi_report_reads(ks, ds, sv, info, reads, misreads, buf, cap)`, `misreads` a `const cf_misread_log*` between `reads` and `buf`; `cf_report_write` (`report.c:421` and `:856`) passes NULL, `cf_report` (`config.c:2444`) passes `c->misreads`. With a log (tracking, even with nothing recorded) the document ends `"keys": {...},\n  "misreads": [ ... ]\n}`: after the `keys` object's closing brace (the `"\n  }\n}\n"` at `report.c:525` becomes `"\n  }"`, then the array, then `"\n}\n"`), one object a distinct misread, in the order first recorded: `{ "key": "server.prot", "kind": "undefined", "wanted": "int" }` and `{ "key": "server.port", "kind": "type", "wanted": "text", "found": "int" }` (the key by `cf_sink_json_string`, the types by `cf_type_name`, `found` only for `type`). Without a log the field is absent, as `"read"` is for a configuration that does not track (`report.c:376-377`); schema 1 stays (`CF_REPORT_SCHEMA`), as every added field has. No value of any key is written: a misread's key is the program's own text, not configuration.
  * **D-121** (index row `| D-121 | A read that finds no definition, or another type, is recorded and reported | configuration, diagnostics | **Decided** |`):

    ```
    ## D-121 — A read that finds no definition, or another type, is recorded and reported

    **Question.** `cf_get_int(cfg, "server.prot", 8080)` returns 8080. Reading
    is infallible (Design.md, "One Error Site"), so nothing tells the program,
    the operator or Doctor that the key is a typo or has another type. How is
    it noticed without making a read fail, take a lock or log from any thread?

    **Options.**

    1. Make a read of an undefined key an error. Rejected: it breaks the rule
       that reads do not fail, and every program that reads a key its
       contract leaves to a default.
    2. Log it. Rejected: a read takes no lock and may be on any thread
       (D-14), and a log line a read at a time is the noise a report avoids.
    3. Record it in a small fixed table beside the read coverage, opt-in, and
       say it in the report (decided).

    **Decision.** Option 3: `cf_builder_track_misreads`, `cf_misreads` and
    `cf_misread_at`, and a `"misreads"` array in the report. A misread is a
    typed read of a key the contract does not define (and is not under an
    open prefix), or whose value, or definition when it has no value, has
    another type; `cf_has` and `cf_type_of` are probes, not reads. Sixteen
    distinct (key, kind, wanted) entries, claimed without a lock and
    published when whole; the rest are lost. No value is recorded.

    **Consequence.** Doctor's CFD021 (C-8b) reads the array.
    ```
* **files**: `lib/config/tests/test_misreads.c` (new), `lib/config/tests/CMakeLists.txt`, `lib/config/tests/test_threads.c`, `lib/config/src/core/misread.c` (new), `lib/config/CMakeLists.txt`, `scripts/generate/amalgamate.py`, `lib/config/include/configforge/configforge.h`, `lib/config/include/configforge/cf_report.h`, `lib/config/src/core/config.c`, `lib/config/src/diag/report.c`; last: `cmake/configforge.experimental.symbols` (generated), `docs/Decisions.md`, `docs/Guide.md`, `CHANGELOG.md`, `docs/Codemap.md` and `docs/API.md` (generated).
* **steps**:
  1. Read `config.c:2203-2330` (`lookup` and every reader of it), `config.c:1550-1560`, `report.c:417-430` and `:515-528`, and `lib/config/tests/test_threads.c:222-256` (the pattern of a threaded test).
  2. Write `lib/config/tests/test_misreads.c` (C89), added to `CF_UNIT_TESTS` (`lib/config/tests/CMakeLists.txt`, after `test_places`). Its contract: `cf_builder_int(b, "server.port", 8080, 1, 65535, 0)`, `cf_builder_text(b, "server.host", "h", 0)`, `cf_builder_text(b, "opt.note", NULL, 0)` (no default), `cf_builder_open(b, "plugins")`, `cf_builder_memory` with `{"server.port","9000"}`, `{"stray","1"}`, `{"plugins.a","x"}` and `cf_builder_unknown(b, CF_UNKNOWN_IGNORE)`. The cases, each a function:
     a. Without `cf_builder_track_misreads`: `cf_tracks_misreads` false; after `cf_get_int(c, "server.prot", 1)` and `cf_get_text(c, "server.port", "")`, `cf_misreads` is 0 and `cf_misread_at(c, 0, ...)` false; and `cf_report` holds no `"misreads"`.
     b. Tracking: `cf_get_int(c, "server.prot", 1)` three times -> `cf_misreads` is 1; `cf_misread_at(c, 0, &key, &kind, &wanted, &found)`: `key` is `server.prot`, `kind` `CF_MISREAD_UNDEFINED`, `wanted` `CF_TYPE_INT`, `found` `CF_TYPE_NULL`. The key buffer the caller passed is a local array overwritten after the call: `key` is still `server.prot` (copied).
     c. `cf_get_text(c, "server.port", "")` -> `CF_MISREAD_TYPE`, `wanted` `CF_TYPE_TEXT`, `found` `CF_TYPE_INT`; `cf_try_int(c, "server.host", &i)` -> wanted int, found text; the same key read as int and as text is two entries.
     d. Not recorded: `cf_get_int(c, "server.port", 0)`; `cf_get_text(c, "opt.note", "x")` (defined, no value, its own type); `cf_get_text(c, "plugins.a", "")` and `cf_get_int(c, "plugins.zzz", 0)` (open prefix); `cf_has(c, "nope")`; `cf_type_of(c, "nope")`; `cf_explain`; `cf_get_int(NULL, "x", 0)`; `cf_get_int(c, NULL, 0)`.
     e. A defined key with no value read as the wrong type: `cf_get_int(c, "opt.note", 0)` -> `CF_MISREAD_TYPE`, `found` `CF_TYPE_TEXT` (from the definition).
     f. A key supplied and not defined, under a contract: `cf_get_text(c, "stray", "")` -> `CF_MISREAD_UNDEFINED`.
     g. Tier 0, no definitions: `{"port","9000"}`: `cf_get_text(c, "port", "")` -> `CF_MISREAD_TYPE` (found int); `cf_get_int(c, "nope", 1)` -> nothing.
     h. Seventeen distinct keys read (`m00`..`m16`, undefined): `cf_misreads` is 16, `cf_misread_at(c, 16, ...)` false, and the sixteen are `m00`..`m15` in that order.
     i. A key of 300 bytes read: recorded cut at `CF_KEY_MAX - 1` bytes, no overrun (run under the sanitize preset by ACCEPT-LINUX).
     j. The report: after b and c, `cf_report(c, &info, buf, sizeof buf)` holds, in this order, `"misreads": [`, `{ "key": "server.prot", "kind": "undefined", "wanted": "int" }`, `{ "key": "server.port", "kind": "type", "wanted": "text", "found": "int" }`; it is valid JSON (read it back with `cf_builder_document(b2, "r", 0, buf, n, "json", 0)` under `#if CF_WITH_JSON`); with tracking and no misread it holds `"misreads": []`; with a full report (`CF_REPORT_FULL`) the same text.
     k. `cf_derive(c, "admin", &pair, 1, &err)`: `cf_tracks_misreads(d)` is true, `cf_misreads(d)` is 0.
     l. In `test_threads.c`, after `test_concurrent_tracked_reads` (its closing brace, before `#endif /* CF_TEST_PTHREADS */`), under `CF_TEST_PTHREADS`: `test_concurrent_misreads`: `READERS` threads each read the one undefined key 10,000 times and a type-wrong key as many; `cf_misreads(c)` is exactly 2 afterwards; added to `main` beside it (line 351).
  3. Declarations and bodies that do nothing (`configforge.h`: the enum and the four functions; `cf_report.h`: the log and its three internal functions; `misread.c`: `cfi_misread_count` returning 0 and the others empty; `config.c`: `cf_builder_track_misreads` empty, `cf_tracks_misreads` false, `cf_misreads` 0, `cf_misread_at` false), so that the tests link. **See it fail**: `ctest --test-dir build -R "test_misreads|test_threads" --output-on-failure` exits non-zero and the FAIL lines name the checks of b, c, e, f, g, h, j, k.
  4. `misread.c`: the three functions as decided; add it to `lib/config/CMakeLists.txt` (after `src/core/edit.c`, line 35) and to `amalgamate.py` (after `"config/src/core/edit.c"`, line 74).
  5. `config.c`: `b->track_misreads`; `c->misreads` (`cf_misread_log*`) allocated beside `c->reads` (line 1552); `lookup` becomes `lookup_as(c, key, want)` with `want` a `cf_type`, and the nine readers pass their type (`cf_has` and `cf_type_of` pass `CF_TYPE_NULL`, which judges nothing; `try_of` passes `want`); the judgement is a `static void misread_judge(const cf_config*, const char* key, size_t keylen, cf_type want, const cf_entry* e)` called from `lookup_as` only when `c->misreads != NULL && want != CF_TYPE_NULL`; `derive_onto` sets `b->track_misreads`.
  6. `report.c`: `cfi_report_reads` (both definitions: 424 and 859) and its declaration take the new parameter and write the array as decided; `cf_report_write` (both: 421 and 856) passes NULL.
  7. `ctest --test-dir build -R "test_misreads|test_threads|test_report|test_report_edges|test_golden|test_derive"` exits 0 (`test_golden` pins reports: a golden that changes means the field was written without tracking: stop).
  8. `docs/Decisions.md` D-121 and its row; `docs/Guide.md` section 6, "Which keys the program reads" (`docs/Guide.md:434`): a paragraph and the two-line example `cf_builder_track_misreads(b); ... cf_misreads(cfg)`; `CHANGELOG.md`.
  9. The closing steps, then ACCEPT.
* **accept**: ACCEPT-LINUX (it includes the `sanitize` preset, which runs `test_misreads` and `test_threads`), ACCEPT-WINDOWS, ACCEPT-CI as `docs/Plan.md:71-114` give them; `ctest --preset tsan -R test_threads` exit 0 where the `tsan` preset builds (the lock-free record is what it checks); `gate.abi` exit 0 and its diff is the four names above in the experimental list.
* **stop if**: a read's cost without tracking shows in `bench gate` (`scripts/bench/gate.py`, CI) beyond its threshold: the pointer test is moved out of the hot path or the card is handed back with the numbers; `test_golden` or a report test moves with tracking off; `tsan` reports a race on a slot's fields (the publish order is wrong: stop, write it under notes).
* **docs**: `docs/Decisions.md` D-121 and its row; `docs/Guide.md` as step 8; `CHANGELOG.md` under `## 1.9.0 — unreleased`, `### Added`: "`cf_builder_track_misreads`, `cf_misreads` and `cf_misread_at` (experimental in 1.9, C-8a): a typed read of a key the contract does not define, or of another type, is recorded without a lock, and the report lists it under `\"misreads\"`"; the Roadmap item is marked by C-8b, which finishes it.

## C-8b - Doctor's CFD021: a read of a key the contract does not have, in `doctor lint`

* **id**: C-8b
* **goal**: `configforge doctor lint report.json` prints a CFD021 finding for each misread a report from a program that tracks them lists, so that both mistakes of a test program (a key that is not there, a key read as the wrong type) reach `doctor lint`.
* **why**: A misread (C-8a) is a bug in the program, found today only by a person who notices the fallback value; Doctor already audits the configuration in CI, and a report carries the evidence.
* **needs**: C-8a (the report's `"misreads"` array)
* **size**: small
* **version**: 2.1.0
* **status**: open
* **decided**:
  * **The rule.** CFD021, severity `warning` (the program does the wrong thing, which is more than CFD018's `info`; an `error` would fail a build for what a person may have meant), id name in the catalogue `Misread: a key read that the contract does not have, or as another type` (`tools/doctor/engine.c:1015-1042`, after the CFD019 entry; CFD020 is H-8b's and its place is left). One finding for each misread. Key: the misread's key; no `at`. Messages: kind `undefined` -> `read as <wanted>, and no key of that name is defined`; kind `type` -> `read as <wanted>, but the key is <found>`. A report without `"misreads"` (a program that did not track, or an older library) gives nothing, said by nothing.
  * **Reading it.** A new `rmis` (`const char* key; const char* kind; const char* wanted; const char* found;`) in `tools/doctor/engine.h` after `rsrc` (line 71), `report` gets `rmis* mis; size_t nmis;` (after `nsrc`, line 89); `doctor_load` (`engine.c`) counts then reads them after the sources (the block that ends at `r->nsrc = count;`, `engine.c:238`), as `sources` is read there: `while (text_at(&r->doc, "misreads.%zu.key", count) != NULL) ++count;`, then `misreads.%zu.key`, `.kind`, `.wanted`, `.found`. A `kind` that is neither `undefined` nor `type` is skipped, so a later library's kind is not misread as these.
  * **Where it runs.** `rule_misread`, defined after `rule_fingerprint` (`engine.c:1795`), is added last to the table `g_lint_rules` (`engine.c:1867-1872`). It does not run in a diff (`g_diff_rules`): a diff compares configurations, and a misread belongs to one program. `--only-changed` drops it (its key is not a report key: `only_touched`, `engine.c:939-952`), which is right: a change to the files cannot cause it.
  * **No new decision.** D-121 holds it.
* **files**: `tests/doctor/fixtures.c`, `tests/doctor/misread.cmake` (new), `tests/doctor/CMakeLists.txt`, `tests/doctor/test_api.c`, `tools/doctor/engine.h`, `tools/doctor/engine.c`; last: `docs/Doctor.md`, `docs/Guide.md`, `CHANGELOG.md`, `docs/Roadmap.md`, `docs/Codemap.md` (generated).
* **steps**:
  1. Read `tests/doctor/fixtures.c:1-62` and `:416-440`, `tests/doctor/redact.cmake:1-40` (the `doctor_run` helper), `tests/doctor/CMakeLists.txt:177-195` (the list of scripts), `tests/doctor/test_api.c:1153-1202` (`test_unread`, the pattern), `tools/doctor/engine.c:1758-1783`.
  2. Write the test first. (a) `fixtures.c`: a function `emit_misread(const char* out)` that builds a configuration with `cf_builder_int(b, "server.port", 8080, 1, 65535, 0)`, `cf_builder_text(b, "server.host", "h", 0)`, `cf_builder_track_misreads(b)`, reads `cf_get_int(c, "server.prot", 8080)` and `cf_get_text(c, "server.port", "")`, writes a blind report with `CF_REPORT_INFO_INIT` (`application` `myapp`, `generated` `2026-09-12T00:00:00Z`) to `cf_fx_misread.json`; called from `main` last. (b) `tests/doctor/misread.cmake`, in the form of `redact.cmake` (its `doctor_run` and `fail`): `doctor_run(lint cf_fx_misread.json)` exits 0 and `all` matches `CFD021` twice, `server.prot`, `no key of that name is defined`, `read as text, but the key is int`; `doctor_run(lint cf_fx_misread.json --fail-on warning)` exits 1; `doctor_run(lint cf_fx_prod.json)` (a report with no `"misreads"`) holds no `CFD021`. (c) `tests/doctor/CMakeLists.txt`: `misread` added to the `foreach(script ...)` list (lines 181-195; the test is `doctor.misread`, which needs the fixtures: the loop sets `FIXTURES_REQUIRED DOCTOR_REPORTS` as the others there). (d) `test_api.c`: `test_misread` after `test_unread`, with the same shape (`cf_doctor_lint(d, c, NULL, &err)` on a configuration that tracks misreads and makes both mistakes: `find(d, "CFD021", "server.prot")` and `find(d, "CFD021", "server.port")` are not NULL and their `level` is `warning`; with no mistake and with no tracking `count_rule(d, "CFD021", NULL)` is 0), called from `main`.
  3. **See it fail**: `ctest --test-dir build -R "doctor.misread|doctor.api" --output-on-failure` exits non-zero: no CFD021.
  4. `engine.h`/`engine.c` as decided (the catalogue entry, `rmis`, the loading, `rule_misread`, the table).
  5. `ctest --test-dir build -R "doctor"` exits 0 (all of Doctor's tests: `doctor.marker` and `doctor.secrets` read every output for a secret, and a misread holds none).
  6. `docs/Doctor.md`: a row after CFD020 (line 443), in the style of the CFD018 row (line 441), id `CFD021`, severity `warning`, last column `yes`, with this text:

     ```
     **Read of a key the contract does not have.** A program that tracks its misreads (`cf_builder_track_misreads`) read a key it does not define, or read a key as another type than it has; the library returned the caller's default, and nobody was told. One finding a misread, from a report taken once the program has done its work (a misread is recorded when it happens).
     ```

     `docs/Guide.md` "Which keys the program reads" (`docs/Guide.md:434`): the sentence that `doctor lint` prints them; `CHANGELOG.md`; `docs/Roadmap.md`.
  7. The closing steps, then ACCEPT.
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI as `docs/Plan.md:71-114` give them; `ctest --test-dir build -R "doctor.misread|doctor.api"` exit 0 on all three; Done when (Roadmap): both mistakes of `emit_misread` print under `doctor lint`.
* **stop if**: C-8a has not landed; the array does not come out of `doctor_load` as `misreads.<i>.key` (the flatten spells arrays otherwise: read `sources.%zu.name` at `engine.c:217` and use what it does); a Doctor secret gate (`doctor.marker`) finds a marker in the new output.
* **docs**: `CHANGELOG.md` under `## 1.9.0 — unreleased`, `### Added`: "Doctor's CFD021 (C-8b): a key a program read that its contract does not define, or read as another type, from a report that carries `misreads` (`warning`; `docs/Doctor.md`)"; `docs/Roadmap.md:823` the item "**A read of a key the contract does not have, noticed.**": `⬜` to `🟩` and `(D-121, C-8a and C-8b)` after the bold heading; `docs/Doctor.md` as step 6.

## C-9 - Values migrated, not only renamed: a function on the old key's definition

* **id**: C-9
* **goal**: a deprecated key's definition can carry a function that turns the value written under the old name into the value of the key that replaced it (`timeout_s` to `timeout_ms`), the result is offered to the new key at the same rank, and `explain()` says `(from timeout_s, converted)`.
* **why**: `replaced_by` moves a value to a new name and nothing converts it (`lib/config/src/core/config.c:1476-1495`): a file written for version 1 that says `timeout_s = 30` reaches `timeout_ms` as 30, thirty milliseconds, with a provenance line that says only `(as timeout_s)`. A value that arrives in the wrong unit is the migration that goes wrong without a word.
* **needs**: nothing
* **size**: medium
* **version**: 2.1.0
* **status**: open
* **decided**:
  * **The function on the definition.** In `lib/config/include/configforge/cf_define.h`, before `cf_def` (line 68):
    ```c
    /* Turns a value written under a deprecated key into the value of the key
       that replaced it. `in` is the value as the old key declares it (its type);
       set `out` to a value of the new key's type (cf_value_int and the others of
       cf_value.h; a text is copied by the library before this returns) and return
       CF_TRUE, or write a short reason to `why` (`cap` bytes) and return CF_FALSE.
       Pure and cheap: it runs again for every value, at every create and every
       derive. */
    typedef cf_bool (*cf_migrate_fn)(const cf_value* in, cf_value* out, void* user,
                                     char* why, size_t cap);
    ```
    and two members appended to `cf_def` after `list_default_count` (line 133): `cf_migrate_fn migrate; void* migrate_user;` (D-51: appended; `DEF_SIZE_MIGRATE` in `define.c` beside `DEF_SIZE_LIST`, line 19; a descriptor smaller than it has `migrate` NULL, as `define.c:412-417` does for the list members). Both are copied by `cf_define` (`*kept = *d`, `define.c:481`); `migrate_user` is borrowed, as `cf_log_fn`'s `user` is, and must outlive the configuration.
  * **When it is allowed.** `migrate != NULL` needs `CF_DEPRECATED`, a `replaced_by` and a scalar key (`list_mode == CF_LIST_NONE`); `def_check` (`define.c:175-308`, after the `replaced_by` check at 199-214) refuses otherwise: `CF_E_DEFINITION`, `'<key>': migrate needs CF_DEPRECATED, replaced_by and a scalar key`. Without a function nothing changes: the copy keeps its value and the place `(as <old>)` (`test_rename` holds it).
  * **Where it runs.** In the rename pass of `cf_builder_create` (`config.c:1476-1495`), on each candidate of the old key as the rename copies it, so that every source's value is converted and precedence then chooses (a command line `--timeout_s=5` beats a file's `timeout_ms = 9000`, as `test_precedence` of `test_rename.c` has it): not on "the winning value" alone, which would be known only after resolution and would make a lower source's value reach the new key unconverted. `cf_ks_rename` (`keyspace.c:385-388`) gets a sibling `CF_API cf_bool cf_ks_rename_with(...)` under `#if CF_INTERNAL_API` (`cf_keyspace.h:270-278`, internal list), the body of `cf_ks_rename_impl` (`keyspace.c:337-383`) taking a hook; `cf_ks_rename` and `cf_ks_rename_list` call it with NULL:
    ```c
    #define CF_KS_CONV_OK    1  /* `c` was converted */
    #define CF_KS_CONV_SKIP  0  /* leave this candidate out of the new key */
    #define CF_KS_CONV_FAIL -1  /* stop: the hook holds the error */
    typedef int (*cf_ks_convert_fn)(void* ctx, cf_candidate* c);
    CF_API cf_bool cf_ks_rename_with(cf_keyspace* ks, const char* from, size_t fromlen,
                                     const char* to, size_t tolen, cf_ks_convert_fn conv,
                                     void* ctx);
    ```
    (returns false on no room, as `cf_ks_rename` does, and on a hook's FAIL; the caller tells them apart by the error the hook set in its context). The hook is called on the copy before it is pushed (`keyspace.c:364-374`); when it returns OK the copy's place is `<at> (from <old>, converted)` instead of `(as <old>)` (the format string at 365 and 371, one more `%s`), and SKIP pushes nothing.
  * **What the hook does** (`config.c`, a `static int migrate_hook(void* ctx, cf_candidate* c)`, `ctx` a struct holding the old `cf_def*`, the new `cf_def*` found by `cf_defset_find(&c->defs, d->replaced_by, ...)`, `c->arena` and a `cf_error`):
    1. A candidate with `CF_CAND_TEMPLATE` set (`cf_keyspace.h:85`): FAIL, `CF_E_VALIDATE`, key the old one, place `c->at`, message `<old>: a value that holds a reference cannot be migrated`.
    2. The input as the old key declares it: with `CF_CAND_TEXTUAL` set, `cf_value_from_text(old->type, c->value.u.t.p, c->value.u.t.n, &in)` (as `validate.c:244-260` does); a typed candidate whose `type != old->type`: SKIP (the old key's own validation refuses it with the old key's words, `validate.c:261-271`; a second refusal for the copy would say it twice); a text that does not convert: SKIP likewise.
    3. `out` is `memset` to zero and `cf_value_null`-ed; `old->migrate(&in, &out, old->migrate_user, why, sizeof why)` with `why` a `char[CF_MESSAGE_MAX]` set to `""`. On CF_FALSE: FAIL, `CF_E_VALIDATE`, place `c->at`, message `<old>: the migration to <new> failed: <why>`; when the old or the new definition is `CF_SENSITIVE` the message is `<old>: the migration to <new> failed` and `why` is not shown (a function's reason may hold the value).
    4. `out.type != new->type` (the new definition found, else no check): FAIL, message `<old>: the migration to <new> produced <cf_type_name(out.type)> where <cf_type_name(new->type)> was declared`.
    5. OK: a text result is copied into `c->arena` (`cf_arena_dup`; no room is FAIL, `CF_E_MEMORY`); `c->value = out`; `c->flags &= ~CF_CAND_TEXTUAL`; `c->written = NULL`. The new key's range, length and allowed spellings then judge the result as for any typed value (`apply_candidate`, `validate.c:237-277`): `timeout_s = 99999999` gives `database.timeout_ms: 99999999000 is outside 1..3600000`, at the line the old key was written.
    `cf_builder_create` returns the hook's error through `create_failed` (`config.c:1246-1252`).
  * **Derive.** `cf_ks_replay` leaves the copies out (`keyspace.c:463`) and replays the old key's own candidates, which the old definition has already typed (no `CF_CAND_TEXTUAL`); the rename pass of the derived build then makes the copies again, and the function runs again on each (the definition travels with its function, `derive_onto` copies definitions whole, `config.c:1693-1695`). So the function runs once for each candidate of the old key at every create and every derive: it must be pure and cheap, which its comment in `cf_define.h` says. A new override under the old key is converted like any (test h below).
  * **Split in two: not this card.** The Roadmap's sentence also says "or splits one key into two". One `replaced_by` names one key, and the function returns one value; a key that becomes `host` and `port` needs a definition on each new key that names the old one, a different interface. **TO DECIDE** (the maintainer, not before it is asked for): (a) not offered; a program writes the two keys' defaults and a one-time conversion of its files (recommended: no program has asked, and `doctor gen` can emit the converted file); (b) `migrate_from` on the new key's definition, a function that reads the old key's value and yields this key's. This card builds neither; its "done when" (a unit change) does not need it.
  * **Rejected**: running the function after resolution on the winner (a loser would reach the new key unconverted, and `explain()` would show two units under one key); a function that sees every key (a migration is one key to one key); the function on the new key's definition for this case (the new key may replace several old ones across versions, each with its own conversion, so the conversion belongs with the old name).
  * **D-122** (index row `| D-122 | A migration is a function on the deprecated key's definition, run on each value as it is copied | definitions, migration | **Decided** |`):

    ```
    ## D-122 — A migration is a function on the deprecated key's definition, run on each value as it is copied

    **Question.** `replaced_by` moves a value to a new name and converts
    nothing. A file written for version 1 says `timeout_s = 30` and the key now
    counts milliseconds. Where does the conversion live, and when does it run?

    **Options.**

    1. On the new key's definition. Rejected: a key renamed twice across
       versions has one conversion for each old name.
    2. On the old key's definition, run on the winning value after resolution.
       Rejected: only the winner would be converted, and a lower source's value
       would reach the new key in the wrong unit.
    3. On the old key's definition, run on every value of the old key as the
       rename copies it, before precedence chooses (decided).

    **Decision.** Option 3. `cf_def.migrate` and `migrate_user`, for a
    deprecated scalar key with `replaced_by`; `in` is the value as the old key
    declares it, `out` the new key's type, a failure a reason. The copy's place
    says `(from <old>, converted)`. A refusal fails the build with the old key
    and the line, and never shows a sensitive value. A value that holds a
    reference is refused, not passed on unconverted.

    **Consequence.** `cf_def` grows by two members (D-51). Splitting one key
    in two is not covered; it is the maintainer's to ask for.
    ```
* **files**: `lib/config/tests/test_migrate.c` (new), `lib/config/tests/CMakeLists.txt`, `lib/config/include/configforge/cf_define.h`, `lib/config/include/configforge/cf_keyspace.h`, `lib/config/src/core/define.c`, `lib/config/src/core/keyspace.c`, `lib/config/src/core/config.c`, `examples/c/migration.c`, `examples/c/CMakeLists.txt`; last: `cmake/configforge.internal.symbols` (generated), `docs/Decisions.md`, `docs/Guide.md`, `CHANGELOG.md`, `docs/Roadmap.md`, `docs/Codemap.md` and `docs/API.md` (generated).
* **steps**:
  1. Read `lib/config/tests/test_rename.c` whole (148 lines: the shape of every case below), `keyspace.c:337-392`, `config.c:1476-1495`, `define.c:175-214` and `:400-420`.
  2. Write `lib/config/tests/test_migrate.c` (C89), added to `CF_UNIT_TESTS` after `test_rename` (`lib/config/tests/CMakeLists.txt`). Common: a converter `static cf_bool s_to_ms(const cf_value* in, cf_value* out, void* user, char* why, size_t cap)` that, for `in->u.i < 0`, writes `negative` to `why` (`strncpy`, terminated) and returns false, else `cf_value_int(out, in->u.i * 1000)`; a contract: `cf_builder_int(b, "database.timeout_ms", 5000, 1, 3600000, 0)`, and the old key `cf_def_init(&d, CF_TYPE_INT, "timeout_s"); d.flags = CF_DEPRECATED; d.replaced_by = "database.timeout_ms"; d.migrate = s_to_ms;`, `cf_builder_unknown(b, CF_UNKNOWN_REJECT)`; sources by `cf_builder_memory` (text pairs). The cases:
     a. `{"timeout_s","30"}`: `cf_get_int(c, "database.timeout_ms", 0)` is 30000; `cf_explain(c, "database.timeout_ms", ...)` has `(from timeout_s, converted)` and not `(as timeout_s)`; `cf_get_int(c, "timeout_s", 0)` is 30 (the old key keeps its own value).
     b. No function (a plain rename, `d.migrate = NULL`): the value is 30 and the place says `(as timeout_s)`: the old behaviour holds.
     c. Precedence as `test_rename`: the new spelling in the same source wins over the converted copy; a command line `{"timeout_s","5"}` at `CF_RANK_CLI` beats a file `{"database.timeout_ms","9000"}` at `CF_RANK_USER_FILE` and gives 5000; a file's `timeout_s` of 7 at `CF_RANK_USER_FILE` loses to a command line `database.timeout_ms` of 9000 at `CF_RANK_CLI`.
     d. `{"timeout_s","99999999"}`: `cf_builder_create` returns NULL; `err.message` has `database.timeout_ms: 99999999000 is outside 1..3600000`.
     e. `{"timeout_s","-1"}`: NULL; `err.message` has `timeout_s: the migration to database.timeout_ms failed: negative`; `err.code` is `CF_E_VALIDATE`.
     f. A converter that sets `out` to text (`cf_value_text`): NULL; `err.message` has `produced text where int was declared`.
     g. Sensitive: the old key `CF_SENSITIVE`, a converter that fails with the value in `why`: `err.message` has `the migration to database.timeout_ms failed` and does not have the value.
     h. `cf_derive(c, "admin", &pair, 1, &err)` with `{"timeout_s","10"}` over case a's configuration: `database.timeout_ms` is 10000; with no pair, a derive keeps 30000 and the explain holds `(from timeout_s, converted)` once.
     i. A `${other}` reference as the value (`{"timeout_s", "${t}"}` with `t` defined): `cf_builder_create` returns NULL; `err.message` has `a value that holds a reference cannot be migrated`.
     j. Definitions: `migrate` on a key without `CF_DEPRECATED`, on one without `replaced_by`, and on a list (`list_mode = CF_LIST_REPLACE`): `cf_builder_create` returns NULL and `err.message` has `migrate needs CF_DEPRECATED, replaced_by and a scalar key`.
     k. A descriptor of the 1.8 size: `cf_def_init(&d, ...); d.migrate = s_to_ms; d.size = offsetof(cf_def, list_default_count) + sizeof d.list_default_count;` (`<stddef.h>`): the build succeeds and the value is 30, not 30000 (a caller built before the members existed is not asked to run a function it never set).
     l. A text-to-text migration (`s_upper`, `in` text, `out` the text upper-cased through a static buffer): the new text key holds the converted text, and the library's copy outlives the buffer (overwrite the buffer after create and read it again).
  3. Declarations and bodies that do nothing: the typedef and the two members (`cf_define.h`), `DEF_SIZE_MIGRATE`, `cf_ks_rename_with` calling `cf_ks_rename_impl` unchanged, so that the test builds. **See it fail**: `ctest --test-dir build -R test_migrate --output-on-failure` exits non-zero and names a, d, e, f, g, h, i, j, k, l.
  4. `define.c`: `DEF_SIZE_MIGRATE` (`offsetof(cf_def, migrate_user) + sizeof(((cf_def*)0)->migrate_user)`), the normalisation for a smaller descriptor (beside `define.c:412-417`: `normalized.migrate = NULL; normalized.migrate_user = NULL;`), the `def_check` rule of j, and `cf_defset_project_list` (`define.c:379`) leaves `p->item.migrate = NULL` as it does `replaced_by`.
  5. `keyspace.c`/`cf_keyspace.h`: `cf_ks_rename_with` and the hook, as decided; `cf_ks_rename` and `cf_ks_rename_list` call it with `conv = NULL, ctx = NULL`.
  6. `config.c`: `migrate_hook` and its context; the rename loop (`1476-1495`) calls `cf_ks_rename_with` for a definition with `migrate != NULL` and `cf_ks_rename` as before otherwise; the hook's error goes through `create_failed`.
  7. `ctest --test-dir build -R "test_migrate|test_rename|test_define|test_lists|test_derive|test_report"` exits 0.
  8. `examples/c/migration.c`: a third scene. Add to the contract of `load` (lines 27-40): `cf_builder_int(b, "database.timeout_ms", 5000, 1, 3600000, 0)` and, as `db.host` is defined at lines 33-36, a second deprecated definition `cf_def_init(&old, CF_TYPE_INT, "database.timeout_s"); old.flags = CF_DEPRECATED; old.replaced_by = "database.timeout_ms"; old.migrate = seconds_to_ms;` with `static cf_bool seconds_to_ms(const cf_value* in, cf_value* out, void* user, char* why, size_t cap)` (`(void)user; (void)why; (void)cap; cf_value_int(out, in->u.i * 1000); return CF_TRUE;`; the scene's value is small). `run` (lines 42-64) also prints `  timeout of %d ms` from `cf_get_int(cfg, "database.timeout_ms", 0)` and the `cf_explain` of that key (the same `why` buffer). The scene: `run("3. a value in the old unit", "[database]\ntimeout_s = 30\n");` before the `remove` at line 70 (`main`, lines 66-72). The header comment (lines 1-17) gains the sentence "A key can also change its unit: the old key's definition carries a function, and explain() says the value was converted." `examples/c/CMakeLists.txt:250-258` (`example_expect(migration ...)`): add `"3. a value in the old unit"`, `"timeout of 30000 ms"` and `"from database.timeout_s, converted"`. Scenes 1 and 2 print `timeout of 5000 ms` and no conversion line: the existing expectations hold.
  9. `docs/Decisions.md` D-122 and its row; `docs/Guide.md` (after the renamed-key paragraph, `docs/Guide.md:197-203`): the unit-change paragraph with the six-line definition; `CHANGELOG.md`; `docs/Roadmap.md`.
  10. The closing steps, then ACCEPT.
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI as `docs/Plan.md:71-114` give them; `ctest --test-dir build -R "test_migrate|example.migration"` exit 0 on all three; Done when (Roadmap): `migration.c` shows a unit change and `explain()` names the old key (the expectation of step 8 holds it).
* **stop if**: the rename pass cannot be given a hook without a second pushed copy appearing in an existing test (`test_rename`'s "one copy for each value, not two", line 111): stop, write the input under notes; `gate.abi` shows a change to the promised list (only the internal list and `cf_def`'s members may move); the INI reader names the example's key other than `database.timeout_s` (read `cf_explain` of the unrenamed key once and write what it printed under notes).
* **docs**: `docs/Decisions.md` D-122 and its row; `docs/Guide.md` as step 9; `CHANGELOG.md` under `## 1.9.0 — unreleased`, `### Added`: "A deprecated key's definition can carry a migration (`cf_def.migrate`, C-9): a function run on each value written under the old name, whose result reaches the new key at the same rank, so `timeout_s = 30` becomes `timeout_ms` 30000; `explain()` says `(from timeout_s, converted)`; `examples/c/migration.c` shows it"; `docs/Roadmap.md:855` the item "**Values migrated, not only renamed.**": `⬜` to `🟩` and `(D-122, C-9)` after the bold heading, with a sentence "Splitting one key in two is left to the maintainer (docs/plan/C-change-2.md, C-9)".
* **Organiser's answers** (2026-11-26, the maintainer, relayed from a side session: "I approve your recommendations"): splitting one key into two is not built now: it becomes a parked card, C-9-m (on request, to be written), with `migrate_from` on the new key's definition (a function from the old value to the new key's). This card builds as written.

## C-10a - A running program asked for its report: the library answers one request on a descriptor

* **id**: C-10a
* **goal**: `cf_serve_report(cfg, fd, &opt, &err)` reads one request from a connected stream socket the program's own loop accepted, answers it with the value-blind report, and returns, in a build with a POSIX descriptor; the library owns no thread and never closes the descriptor.
* **why**: Spring Boot's `/actuator/env` lets an operator ask a running program what it is configured with and where each value came from. ConfigForge's report is the same artifact (`cf_report`, `lib/config/include/configforge/configforge.h:666`) and today a program must write the listening, the parsing and the writing itself, or write the report to a file and hope somebody reads it. A function the program calls when its loop says a connection is ready gives the answer without the library running a server.
* **needs**: nothing
* **size**: medium
* **version**: 2.1.0
* **status**: open
* **decided**:
  * **The function** (`configforge.h`, after `cf_report`, line 666):
    ```c
    typedef struct cf_serve_options {
        size_t                size;       /* sizeof(cf_serve_options) */
        const cf_report_info* info;       /* NULL: a report that names nothing */
        long                  timeout_ms; /* each wait; negative: no limit */
    } cf_serve_options;
    #define CF_SERVE_OPTIONS_INIT {sizeof(cf_serve_options), NULL, 2000}
    CF_EXPERIMENTAL(1, 9) cf_bool cf_serve_report(const cf_config* c, int fd,
                                                  const cf_serve_options* opt,
                                                  cf_error* err);
    ```
    `opt` NULL means `CF_SERVE_OPTIONS_INIT`; a size below the 1.9 struct is refused as `cf_report_info`'s is (D-51): `CF_FALSE`, `CF_E_CAPABILITY`, `cf_serve_report: options did not start from CF_SERVE_OPTIONS_INIT`. The report is always `CF_REPORT_BLIND`: an `opt->info` whose `mode` is `CF_REPORT_FULL` is refused (`CF_FALSE`, `CF_E_POLICY`, `cf_serve_report: a report served on a descriptor is value-blind`); a full report holds every secret and a socket may be reached by whoever its path lets (decided here, not an option).
  * **What it does.** The descriptor is a connected stream socket (`AF_UNIX` or `AF_INET`); it is made non-blocking for the call and its flags restored before return. It reads the request until the blank line that ends the headers (`\r\n\r\n` or `\n\n`) or 4096 bytes, each wait at most `timeout_ms`; answers; and returns. The program closes the descriptor. It never accepts, listens, binds, reads a body or keeps anything between calls: **no thread, no state** (D-14).
  * **The wire: an HTTP/1.0 subset, so that `curl --unix-socket /run/myapp.sock http://x/report` works.** The request line is `METHOD SP TARGET SP HTTP/1.x`:
    * `GET /report`: `200 OK`, `Content-Type: application/json`, `Content-Length: <n>`, `Cache-Control: no-store`, `Connection: close`, the report (`cf_report` with `opt->info`, blind) as the body;
    * another target (a query string too): `404 Not Found`, `Content-Type: text/plain`, the body `not found\n`;
    * another method: `405 Method Not Allowed`, `Allow: GET`, the body `method not allowed\n`;
    * a first line over 1024 bytes, no `HTTP/` version, or headers over 4096 bytes: `400 Bad Request`, the body `bad request\n`.
    Every answer has `Content-Length` and `Connection: close` and is `HTTP/1.0`. Any of these is an answer: the function returns `CF_TRUE` once the whole answer was written. It returns `CF_FALSE` with `err` set (`CF_E_SOURCE`) only when nothing could be answered: `cf_serve_report: no request within <ms> ms` (a timeout while reading), `cf_serve_report: the peer closed before a request`, `cf_serve_report: cannot read the request` or `cannot write the answer` (an I/O error or a timeout while writing), `cf_serve_report: the descriptor is not a socket`. Rejected: a raw protocol (write the report on connect): it answers nobody who asked something else and cannot be told a 404 from a report.
  * **The report is made before the first byte is written**: `n = cf_report(c, info, NULL, 0)`, a buffer of `n + 1` from `cf_default_allocator()` (no allocator: `CF_E_MEMORY` and `CF_FALSE`), `cf_report(c, info, buf, n + 1)`, written, wiped (`cf_wipe`) and released; when `n` is 0 (a build without `CF_WITH_REPORT`) the answer is `501 Not Implemented`, `text/plain`, the body `no report in this build\n`.
  * **Windows and the rest: not this card.** `cf_platform_has_fd_io()` is true only on POSIX; elsewhere `cf_serve_report` returns `CF_FALSE`, `CF_E_CAPABILITY`, `cf_serve_report needs a POSIX descriptor`. **TO DECIDE** (the maintainer, only when a Windows program asks): serving on Windows through a `SOCKET` (`AF_UNIX` exists since Windows 10) or a named pipe `HANDLE` (`cf_watcher_handle`'s kind), as a second function or a `void*` descriptor; this card builds neither.
  * **The platform layer** (`lib/platform/include/configforge/cf_platform.h`, a section "A descriptor" after "The environment", under `#if CF_INTERNAL_API`, `CF_API`; internal list), five functions: `cf_bool cf_platform_has_fd_io(void)`; `cf_bool cf_platform_fd_is_socket(int fd)` (`getsockopt(fd, SOL_SOCKET, SO_TYPE, ...)` succeeds); `int cf_platform_fd_nonblock(int fd, int on)` (sets or clears `O_NONBLOCK` with `fcntl` and returns the flag as it was, 1 or 0, or -1 on failure; `cf_serve_report` restores it before it returns); `long cf_platform_fd_read(int fd, void* buf, size_t cap, long timeout_ms)` (bytes read, 0 at end of stream, -1 on failure, -2 on timeout: `poll` for `POLLIN`, then `read`); `cf_bool cf_platform_fd_write(int fd, const void* buf, size_t n, long timeout_ms)` (every byte or false: `poll` for `POLLOUT`, then `send(..., MSG_NOSIGNAL)` where that name exists, else `SO_NOSIGPIPE` set once through `setsockopt` on the descriptor (macOS), so that a peer that went away is an error return and never a signal the library has no right to handle). `lib/platform/src/posix/posix.c` implements them (the headers `<poll.h>`, `<fcntl.h>`, `<sys/socket.h>` inside the `#if CF_PLATFORM_POSIX` body, `posix.c:25-45`; the amalgamation takes them from there, so `serve.c` includes none); `win32/win32.c` and `none/none.c` answer false, false, -1, -1 and false (`none.c:18-26` is the pattern). `cf_serve_report` checks `cf_platform_fd_is_socket` first: `cf_serve_report: the descriptor is not a socket`.
  * **D-123** (index row `| D-123 | A running program answers one request for its report on a descriptor its own loop accepted | diagnostics, platform | **Decided** |`):

    ```
    ## D-123 — A running program answers one request for its report on a descriptor its own loop accepted

    **Question.** Spring Boot's `/actuator/env` shows an operator what a running
    program is configured with. How does ConfigForge offer that when the library
    owns no thread (D-14) and runs no server?

    **Options.**

    1. A listener thread in the library. Rejected: the rule it would break is
       the point.
    2. The program writes the report to a file on a signal. Rejected: stale,
       and a file is a second place the secrets' absence has to be argued.
    3. A function the program's own loop calls on a descriptor it accepted
       (decided): the library reads one request, writes one answer, returns.

    **Decision.** Option 3: `cf_serve_report`. The wire is an HTTP/1.0 subset,
    `GET /report`, so that `curl --unix-socket` and a browser's tooling work;
    the answer is the value-blind report, never the full one, with no option to
    change it. The descriptor stays the program's: the library neither
    accepts nor closes. Each wait is bounded (2 s by default). POSIX only.

    **Consequence.** `configforge doctor pull unix:<path>` (C-10b) reads it.
    The socket's access is the program's: a path in a directory only its
    operator can enter is the control; the library adds none.
    ```
* **files**: `lib/config/tests/test_serve.c` (new), `lib/config/tests/CMakeLists.txt`, `lib/config/src/diag/serve.c` (new), `lib/config/CMakeLists.txt`, `scripts/generate/amalgamate.py`, `lib/config/include/configforge/configforge.h`, `lib/platform/include/configforge/cf_platform.h`, `lib/platform/src/posix/posix.c`, `lib/platform/src/win32/win32.c`, `lib/platform/src/none/none.c`; last: `cmake/configforge.experimental.symbols` and `cmake/configforge.internal.symbols` (generated), `docs/Decisions.md`, `docs/Safety.md`, `CHANGELOG.md`, `docs/Codemap.md` and `docs/API.md` (generated).
* **steps**:
  1. Read `posix.c:1-60` and `:805-865` (a function and its guard), `none.c:1-30`, `configforge.h:644-668`, `lib/config/src/diag/usage.c:1-40` (a `diag` file's includes and shape).
  2. Write `lib/config/tests/test_serve.c` (C89 with `#define _POSIX_C_SOURCE 200112L` first, as `posix.c:18-20`, and `#include "configforge/cf_platform_file.h"` for `CF_PLATFORM_POSIX`), the whole body under `#if CF_PLATFORM_POSIX && CF_WITH_REPORT`; else, with `CF_WITH_REPORT` and no POSIX (Windows), `main` makes the configuration, calls `cf_serve_report(c, 0, NULL, &err)` and checks `CF_FALSE`, `CF_E_CAPABILITY` and `needs a POSIX descriptor` (this is ACCEPT-WINDOWS's three-line case); without `CF_WITH_REPORT` it prints `skipped`; each returns `cf_test_done()`, added to `CF_UNIT_TESTS` after `test_places`. Common: a pair from `socketpair(AF_UNIX, SOCK_STREAM, 0, sv)`, both ends' `SO_SNDBUF` and `SO_RCVBUF` raised to 262144 (macOS's default is 8 KiB and a report is not smaller), a helper `ask(const char* request, char* out, size_t cap, cf_bool* ok, cf_error* err)` that writes `request` to `sv[0]`, calls `cf_serve_report(c, sv[1], opt, err)`, `shutdown(sv[1], SHUT_WR)`, and reads `sv[0]` to the end into `out`; a configuration with `cf_builder_int(b, "server.port", 8080, 1, 65535, 0)`, `cf_builder_text(b, "db.password", NULL, CF_SENSITIVE)` and `{"db.password","MARKER-7731"}` from a memory source. The cases:
     a. `GET /report HTTP/1.0\r\n\r\n`: returns true; `out` starts `HTTP/1.0 200 OK\r\n`, holds `Content-Type: application/json`, `Connection: close`, and a `Content-Length` equal to the byte count after the blank line; the body begins `{` and holds `"configforge_report": 1`, `"mode": "blind"`, `"server.port"`; **never** `MARKER-7731`.
     b. With `opt->info` set (`application` `myapp`, `profile` `prod`): the body holds `"profile": "prod"` and `"name": "myapp"`; with `info.mode = CF_REPORT_FULL`: false, `err.message` has `value-blind`, and nothing was written to the socket (`recv` with `MSG_DONTWAIT` returns 0 after the shutdown: end of stream, no bytes).
     c. `GET /elsewhere HTTP/1.1\r\nHost: x\r\n\r\n` and `GET /report?x=1 HTTP/1.0\r\n\r\n`: true; `404 Not Found`, body `not found\n`. `POST /report HTTP/1.0\r\n\r\n`: true; `405 Method Not Allowed`, `Allow: GET`. `GET /report\r\n\r\n` (no version) and a first line of 2000 bytes: true; `400 Bad Request`. Headers of 5000 bytes: `400`.
     d. `\n\n` in place of `\r\n\r\n`: answered as a.
     e. A request that stops half way: write `GET /rep` only (no blank line), call `cf_serve_report` with `timeout_ms = 50`: false, `err.message` has `no request within 50 ms`, and the descriptor's flags (`fcntl(F_GETFL)`) are as before the call.
     f. The peer closes before any request (`close(sv[0])` first): false, `the peer closed before a request`; and the process is alive (no `SIGPIPE`): a second call on a fresh pair then works.
     g. The peer goes away before the answer is written (request written, `sv[0]` closed, then `cf_serve_report`): false, `cannot write the answer`, no signal.
     h. A descriptor that is not a socket (`open("/dev/null", O_RDWR)`, and the read end of a `pipe`): false, `err.message` has `the descriptor is not a socket`, before any read.
     i. `opt` with `size = 0`: false, `did not start from CF_SERVE_OPTIONS_INIT`; `opt == NULL` is the defaults; `c == NULL`: false, `CF_E_SOURCE`, no crash.
     j. After the call the descriptor is open (`fcntl(F_GETFD)` is not -1): the library never closes it.
  3. Declarations and bodies that do nothing (the struct and macro, `cf_serve_report` returning `CF_FALSE`, the platform functions as the `none` ones), so that it links. **See it fail**: `ctest --test-dir build -R test_serve --output-on-failure` exits non-zero and names a to j.
  4. `posix.c`: the five platform functions (`cf_platform_has_fd_io` true), inside the existing POSIX guard; `win32.c` and `none.c`: the ones that say no.
  5. `serve.c` (`lib/config/src/diag/serve.c`, statics prefixed `srv_`): `srv_read_request` (up to 4096 bytes, stops at the blank line), `srv_parse` (method, target, version, as decided), `srv_answer(fd, timeout, status line, extra headers, content type, body, n)` building the head in a `char[256]`, `cf_serve_report`. Added to `lib/config/CMakeLists.txt` (after `src/diag/usage.c`, line 46) and `amalgamate.py:80`.
  6. `ctest --test-dir build -R "test_serve|test_report|test_marker"` exits 0.
  7. `docs/Decisions.md` D-123 and its row; `docs/Safety.md`: one paragraph "Serving a report": the report is blind, the socket's access is the program's, a timeout bounds each wait, the library holds nothing between calls; `CHANGELOG.md`.
  8. The closing steps, then ACCEPT.
* **accept**: ACCEPT-LINUX (the `sanitize` preset runs `test_serve`), ACCEPT-WINDOWS (the test's non-POSIX branch checks that `cf_serve_report` returns `CF_E_CAPABILITY`), ACCEPT-CI (macOS and FreeBSD run it: `SO_NOSIGPIPE` and `MSG_NOSIGNAL` are what they check); `gate.abi` exit 0.
* **stop if**: `poll` or `send(MSG_NOSIGNAL)` is not available under `-std=c89` with `_POSIX_C_SOURCE 200112L` on a CI platform and a feature macro does not make it so (stop, write the platform and the compiler's message under notes); a `test_marker`-style search finds the sensitive value in an answer.
* **docs**: `docs/Decisions.md` D-123 and its row; `docs/Safety.md` as step 7; `CHANGELOG.md` under `## 1.9.0 — unreleased`, `### Added`: "`cf_serve_report` (experimental in 1.9, C-10a): one HTTP/1.0 `GET /report` answered on a socket the program's own loop accepted, with the value-blind report; no thread, no state, POSIX"; the Roadmap item is marked by C-10b, which finishes it.
* **Organiser's answers** (2026-11-26, the maintainer, relayed from a side session: "I approve your recommendations"), written as **D-229**: the channel a report is served on is a part behind an interface (a POSIX descriptor now), so that Windows (an AF_UNIX socket or a named pipe) is added later without touching the library. The Windows part is a parked card, C-10c-m (on request, to be written); `http://` and `tcp:` targets are C-10d-m (on request, to be written). This card builds the interface and the POSIX part.
* **Organiser's answers** (2026-11-27, the organiser, under the maintainer's standing approval): K-11a's convention: `CF_API void cf_serve_options_init(cf_serve_options* o)` sets `size` and every default, and a NULL options pointer means those defaults; the card's test calls it.

## C-10b - `configforge doctor pull unix:<path>`, and an example that serves from a loop

* **id**: C-10b
* **goal**: `configforge doctor pull unix:/run/myapp.sock` connects to a running program, asks it for its report and writes the report to standard output, ready for every other Doctor command; an example program serves it from a `poll` loop (the kind `cf_watcher` is for), and a test has Doctor read what the example's `cf_serve_report` writes.
* **why**: A report taken from a running program is the one an operator needs and cannot get from the files (docs/Doctor.md, "Collecting a Production Report", line 176). C-10a lets the program answer; Doctor must be able to ask without `curl` and a shell pipeline.
* **needs**: C-10a
* **size**: medium
* **version**: 2.1.0
* **status**: open
* **decided**:
  * **The command.** `configforge-doctor pull unix:<path>`, dispatched with the adapters before the report argument is expected (`tools/doctor/doctor.c:430-446`, beside `import-systemd`): `doctor_pull(argc - 2, argv + 2)`, declared in `tools/doctor/adapters.h`, in `tools/doctor/pull.c` (new, in the `doctor` target's source list, `tools/doctor/CMakeLists.txt:29-35`). It takes exactly one argument. No flag: the wait is 10 seconds for the connection and for each part of the answer, a report is bounded at 16 MiB. The `configforge` front door passes `doctor` through (`tools/configforge/main.c`); its help line (`main.c:33-36`) and `usage()` in `doctor.c` gain `pull`.
  * **What it does.** The argument must start `unix:`; the rest is a socket path of 1 to 103 bytes (macOS and the BSDs have a `sun_path` of 104 bytes, Linux 108: the shortest, so that one command means the same everywhere). It connects (`AF_UNIX`, `SOCK_STREAM`), writes `GET /report HTTP/1.0\r\nHost: localhost\r\nConnection: close\r\n\r\n`, reads to the end of the stream, requires `HTTP/1.` and a `200` status, checks the body is as long as `Content-Length` says when it says, and writes the body to stdout, nothing else, newline unchanged. Exit codes are Doctor's (docs/Doctor.md, section 10): 0 done, 2 unusable input (a refusal below), 3 cannot start. Refusals, each on stderr with `doctor: pull: ` in front: `'<arg>' is not unix:<path>`; `the path is longer than 103 bytes`; `cannot connect to <path>: <strerror>`; `the program answered <status line>`; `the answer is cut short (<got> of <want> bytes)`; `no answer within 10 s`; `the answer is longer than 16 MiB`. Nothing is written to stdout on a refusal.
  * **Windows and a TCP address: not this card.** `pull` on a build without POSIX sockets exits 2 with `doctor: pull: needs a POSIX system`; `http://` and `tcp:` are not offered (the report is value-blind, but a program's report on a network address is the program's decision to expose and a client that follows redirects or TLS is a second tool). **TO DECIDE** (the maintainer, when asked): `pull http://host:port/report` and Windows.
  * **The test needs no second process managed by CMake.** `tests/doctor/test_pull.c` (new, C89 with `_POSIX_C_SOURCE 200112L` first, POSIX, under `#if CF_PLATFORM_POSIX && CF_WITH_REPORT`, `CF_PLATFORM_POSIX` from `configforge/cf_platform_file.h`): `main(argc, argv)` with `argv[1]` the Doctor program; it makes a directory-free socket path in the working directory (`cf_pull.sock`, `unlink`ed first), `socket`, `bind`, `listen(1)`, `fork`s; the child `execv`s `argv[1]` with `pull unix:<path>` and standard output redirected (`dup2`) to `cf_pull_out.json`; the parent `accept`s (with a `poll` of 10 s), calls `cf_serve_report` on a configuration that holds `server.port` and a sensitive `db.password` (a marker value), closes, `waitpid`s the child, and checks: exit status 0; the file starts `{` and holds `"configforge_report": 1` and `"server.port"` and not the marker; and `cf_doctor_lint_file(d, path, ...)` (in-process, `tools/doctor/include/configforge/cf_doctor.h:89`) on the file returns true. A second run with the parent closing the accepted socket at once (no answer): the child's exit status is 2 and the file is empty (`pull` wrote nothing). A third with `pull unix:` alone and one with `pull tcp:1`: exit 2 and the refusal on stderr (`argv[1]` run through `popen`-free `fork`/`exec` with `stderr` to `cf_pull_err.txt`).
  * **The example** (`examples/c/serve.c`, C89 with `#define _POSIX_C_SOURCE 200112L`, POSIX only, built `if(UNIX)`): a program that reads `serve.ini`, listens on `serve.sock` (a relative path in its working directory, `unlink`ed before the bind and after), and runs a loop on `poll` over the listening socket and, where the build has one, `cf_watcher_fd` of a `cf_watcher_open` (the loop `docs/Guide.md` section 10 describes for a reload): on the watcher it calls `cf_watcher_drain` and says `the file changed`; on the listener it `accept`s and calls `cf_serve_report(cfg, fd, &opt, &err)` (`opt.info` with `application` `serve`), then closes `fd`. Run unattended (`./serve`, the suite's way), it does what a client would: it `connect`s to its own socket first (the connection waits in the backlog without an `accept`), writes `GET /report HTTP/1.0\r\n\r\n`, runs the loop once, reads the answer back on the client socket, and prints `the loop served one request: ` followed by the answer's status line and `its first key: ` followed by the first key the report names (the text between the first `"keys": {` quotes). With the argument `--listen` it serves until killed, the way a program would, and prints `listening on serve.sock`.
  * **No new decision.** D-123 holds it.
* **files**: `tools/doctor/pull.c` (new), `tools/doctor/adapters.h`, `tools/doctor/doctor.c`, `tools/doctor/CMakeLists.txt`, `tools/configforge/main.c`, `tests/doctor/test_pull.c` (new), `tests/doctor/CMakeLists.txt`, `examples/c/serve.c` (new), `examples/c/CMakeLists.txt`; last: `docs/Doctor.md`, `docs/Guide.md`, `CHANGELOG.md`, `docs/Roadmap.md`, `docs/Codemap.md` (generated).
* **steps**:
  1. Read `tools/doctor/systemd.c:91-130` (the shape of an adapter: arguments, exit codes), `tools/doctor/doctor.c:430-447`, `tests/doctor/CMakeLists.txt:140-170` and `:205-215` (how `doctor_api` is built and registered), `docs/Guide.md:1040-1060` (the watcher loop), `tools/doctor/include/configforge/cf_doctor.h:89` (`cf_doctor_lint_file`'s signature).
  2. Write the test first: `tests/doctor/test_pull.c` as decided, registered after `doctor_api` in `tests/doctor/CMakeLists.txt`: `add_executable(doctor_pull_test test_pull.c)`, linked to `configforge_doctor`, `add_test(NAME doctor.pull COMMAND doctor_pull_test $<TARGET_FILE:doctor>)` under `if(UNIX)`. And the example's expectation, `examples/c/CMakeLists.txt`: `serve` in the loop of line 55-83 is not used (that list is C89 non-POSIX); instead after it: `if(UNIX AND CF_WITH_INI AND CF_WITH_REPORT) add_executable(serve serve.c) ... example_expect(serve ${CMAKE_CURRENT_BINARY_DIR}/serve-run "the loop served one request: HTTP/1.0 200 OK" "its first key: ") endif()` with the same properties as the others (C90, `configforge_apply_warnings`, `configforge_apply_analysis`) and `serve.ini` written by `file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/serve-run/serve.ini "[server]\nport = 8080\n")` before it.
  3. A `pull.c` that does nothing (`doctor_pull` returning 2) and `serve.c` as a `main` that returns 0, so that they build. **See it fail**: `ctest --test-dir build -R "doctor.pull|example.serve" --output-on-failure` exits non-zero: no report in the file, no line from the example.
  4. `pull.c` as decided (statics prefixed `pull_`; the sockets under `#if CF_PLATFORM_POSIX`, headers after `_POSIX_C_SOURCE 200112L` as `posix.c:18-20` has it); `adapters.h` and `doctor.c` dispatch and usage; `tools/configforge/main.c:33-36` help line: `... history, blame, pull, import-*`.
  5. `serve.c` as decided.
  6. `ctest --test-dir build -R "doctor|example"` exits 0.
  7. `docs/Doctor.md`: in section 6 ("Collecting a Production Report", line 176) a paragraph "From a running program": the example's shape, `configforge doctor pull unix:/run/myapp.sock > report.json`, and the sentence that the report is the value-blind one; in the commands section 7 (line 199) the line `pull`. `docs/Guide.md` section 11 (`docs/Guide.md:1315`): the same in four lines, with `examples/c/serve.c`. `CHANGELOG.md`. `docs/Roadmap.md`.
  8. The closing steps, then ACCEPT.
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS (the POSIX parts are not built; `doctor.version` and the rest still exit 0), ACCEPT-CI (macOS and FreeBSD run `doctor.pull`) as `docs/Plan.md:71-114` give them; `ctest --test-dir build -R "doctor.pull|example.serve"` exit 0 on Linux. Done when (Roadmap): the example serves from a loop of `cf_watcher`'s kind and Doctor reads it (`doctor.pull` reads what `cf_serve_report` wrote; `example.serve` runs the loop).
* **stop if**: C-10a has not landed; `fork` and `execv` are not usable in the test on a CI platform (then the test becomes a `cmake -P` that runs the example with `--listen` in the background through `execute_process` with `COMMAND_ECHO`: stop and write the platform); an `AF_UNIX` path in the build directory is longer than 103 bytes (CI's `$HOME/cfv` paths: the test uses the relative `cf_pull.sock` and the working directory is the test's, so it should not be; if it is, stop and write the length).
* **docs**: `docs/Doctor.md` and `docs/Guide.md` as step 7; `CHANGELOG.md` under `## 1.9.0 — unreleased`, `### Added`: "`configforge doctor pull unix:<path>` (C-10b) asks a running program for its value-blind report over a socket and writes it to standard output; `examples/c/serve.c` answers it from a `poll` loop with `cf_serve_report`"; `docs/Roadmap.md:932` the item "**A running program asked for its report.**": `⬜` to `🟩` and `(D-123, C-10a and C-10b)` after the bold heading.
* **Organiser's answers** (2026-11-26, the maintainer, relayed from a side session: "I approve your recommendations"): as C-10a's (D-229); `unix:` only; `http://` and `tcp:` are C-10d-m.

## C-11 - A program's files where its platform keeps them: `cf_builder_app_files`

* **id**: C-11
* **goal**: `cf_builder_app_files(b, "myapp", "toml")` adds the system file at `CF_RANK_SYS_FILE` with its `conf.d` just above it and the user's file at `CF_RANK_USER_FILE`, from the places each platform keeps them; a missing file is no error; `cf_inspect` and the report name each path looked at; the paths of the three platforms are tested with a fixed environment; `examples/c/layers.c` uses it.
* **why**: Design.md promised that `cf_app` would read the user's and the system's files (`docs/Design.md:309-320`); D-09 closed `cf_app` without them (`docs/Decisions.md:515-535`). So every program writes the XDG, `%APPDATA%` and `~/Library/Application Support` code itself, with its own mistakes (a relative `XDG_CONFIG_HOME` the specification says to ignore, an unset `HOME`): the repeated code this library exists to remove, and the one place a path is wrong without anybody being told.
* **needs**: nothing (C-7 builds on it)
* **size**: medium
* **version**: 2.1.0
* **status**: open
* **decided**:
  * **The function** (`configforge.h`, after `cf_builder_response_files`, line 126; the comment at lines 65-69, "cf_app reads no file. For files, start from cf_app_builder...", gains "or call cf_builder_app_files, which reads the platform's places"):
    ```c
    CF_EXPERIMENTAL(1, 9) cf_bool cf_builder_app_files(cf_builder* b, const char* name,
                                                       const char* format);
    ```
    Returns `CF_FALSE` for a NULL or failed builder; a refusal fails the builder as every builder call does (`builder_fail`, `config.c:183-189`), code `CF_E_POLICY`. Opt-in: `cf_app` stays as D-09 has it.
  * **`name`**: 1 to 64 bytes of `A-Z a-z 0-9 . _ -`, not `.` or `..`: the program's directory name. Else `cf_builder_app_files: '<name>' is not a program name: letters, digits, '.', '_' and '-', up to 64` (the text cut at 40 bytes). **`format`**: a name that `cf_codec_among(b->codecs, b->ncodecs, format, NULL)` or `cf_codec_by_name(format)` (`lib/serialize/include/configforge/cf_serialize.h:267-273`) knows, else `cf_builder_app_files: this build has no format '<format>'` (a typo is told now, not when a file first appears). `format` is also the extension: the file is `config.<format>`.
  * **The places**, by platform, one function, the same on every host (`cfi_app_places`, below). An environment value that is empty is unset. "Absolute" is: POSIX and macOS, starts with `/`; Windows, a drive letter, `:` and `\` or `/`, or two `\`. A value that is not absolute is ignored (the XDG specification says so for `XDG_CONFIG_HOME` and `XDG_CONFIG_DIRS`; it is also what keeps a daemon from reading `./config.toml` of whatever directory it was started in).

    | | system | user |
    | --- | --- | --- |
    | POSIX (Linux, the BSDs) | each of the first three absolute entries of `$XDG_CONFIG_DIRS` (colon separated), else `/etc` | `$XDG_CONFIG_HOME`, else `$HOME/.config` |
    | Windows | `%PROGRAMDATA%` | `%APPDATA%` |
    | macOS | `/Library/Application Support` | `$HOME/Library/Application Support` |

    A base `<B>` gives the directory `<B><sep><name>`, the file `<dir><sep>config.<format>`, and for a system base the drop-in directory `<dir><sep>conf.d`; `<sep>` is `/`, and `\` for Windows. One trailing separator on `<B>` is dropped first. The names of the Windows variables are matched without regard to case (the environment of a Windows process spells them as it pleases); on the other platforms exactly. `$XDG_CONFIG_DIRS` with no absolute entry is as unset. More than three entries: the rest are ignored (a program with a longer list is rare, and the builder holds 16 sources, `config.c:44`).
  * **Each source has a rank of its own.** A candidate holds the rank of its source, not the source, and `cf_explain`, `cf_inspect` and the report name a candidate's source by looking the rank up (`explain.c:17-29`, `report.c:68-80`: the first source of that rank): two sources at one rank are named alike, and one of them hides the other's "supplied nothing". So no two sources this call adds share a rank, and the Roadmap's "at `CF_RANK_SYS_FILE`" is read as: the system file is `CF_RANK_SYS_FILE` (30) and its drop-in directory is just above it.
  * **The ranks and the order** (what `cfi_app_places` returns, the order sources are added). The k-th system base (k = 0 for the first entry of `$XDG_CONFIG_DIRS`, or for `/etc`, `%PROGRAMDATA%` and `/Library/Application Support`, which have only one): file `30 + 3k`, drop-in directory `28 + 3k` (`CF_RANK_SYS_DROPIN`, new, = 28), profile file `29 + 3k` (`CF_RANK_SYS_PROFILE`, new, = 29) when a profile is given (C-7); the user's file `20` (`CF_RANK_USER_FILE`) and its profile file `19` (`CF_RANK_USER_PROFILE`, new, = 19). Lower wins (`cf_source.h:25-33`), so a base's drop-ins beat its profile file, which beats its file; the first `$XDG_CONFIG_DIRS` entry, with all its files, beats the second; the user's beat the system's. Three bases at most: the highest rank is 36, below `CF_RANK_DEFAULT`. A program that adds its own sources of 28 to 36 shares the band and is told so in `docs/Guide.md`. The add order is: for k = 0, 1, 2: file, drop-in directory, profile file; then the user's file, its profile file. The profile is C-7's: here `cfi_app_places` takes it and emits the extra places, and `cfi_builder_app_files` passes NULL.
  * **A missing file is no error and is not silence.** Every source is added `CF_OPTIONAL` (`cf_builder_file`, `config.c:437-479`; `cf_builder_dir`, `config.c:728-767`, with `format` NULL so that each drop-in file's own extension decides, as `layers.c:46` has it). Its name is its path, so that `cf_inspect` (`explain.c:243-247`) prints `note: source '<path>' supplied nothing` for each path that was looked at and held nothing, and the report's `"sources"` lists every path. No `CF_TRUSTED_DIR`: not asked, and a program that wants D-83's check adds its own directory.
  * **The environment is the builder's.** `cf_builder_env` records its `environment` argument in a new `char* const* env` of `cf_builder` (`config.c:48-99`); `cf_app_builder` thereby records `cf_environ()` (`config.c:281`). `cf_builder_app_files` reads that, or `cf_environ()` when no environment source was added. So a program, and a test, that gives its builder a fixed environment gets the places of that environment.
  * **Internal, for the tests** (`cf_source.h`, under `#if CF_INTERNAL_API`, `CF_API`, internal list):
    ```c
    typedef enum cf_app_platform { CF_APP_POSIX = 0, CF_APP_WINDOWS = 1, CF_APP_MACOS = 2 } cf_app_platform;
    typedef struct cf_app_place { const char* path; int rank; cf_bool dir; } cf_app_place;
    #define CF_APP_PLACES_MAX 16
    CF_API size_t cfi_app_places(cf_arena* ar, cf_app_platform platform, char* const* environment,
                                 const char* name, const char* format, const char* profile,
                                 cf_app_place* out, size_t cap);
    CF_API cf_bool cfi_builder_app_files(cf_builder* b, const char* name, const char* format,
                                         cf_app_platform platform, char* const* environment);
    ```
    `cfi_app_places` writes at most `cap` (>= `CF_APP_PLACES_MAX`) places into `out`, the strings in `ar`, and returns how many, or `(size_t)-1` for no room or a smaller `cap`; it reads nothing but `environment`. `cfi_builder_app_files` validates, plans and adds the sources; `cf_builder_app_files` is it with the native platform (`CF_PLATFORM_WIN32` from `cf_platform_file.h:34-44`: Windows; `__APPLE__`: macOS; else POSIX) and the builder's environment. Both in `config.c`; the planner and its helpers in `lib/config/src/sources/appfiles.c` (new; statics prefixed `appf_`).
  * **`layers.c` uses it, and the files beside it stay where they are** (the C++, Rust and Python twins read `examples/c/layers/` too: `examples/cpp/CMakeLists.txt:44-45`, `examples/rust/CMakeLists.txt:33-34`, `examples/python/CMakeLists.txt:37-38`; nothing in that directory is renamed or moved). In `examples/c/layers.c` the line that adds the drop-in directory (`layers.c:46`, `cf_builder_dir(b, "conf.d", 30, "conf.d", NULL, CF_OPTIONAL)`) is replaced by the platform's place: the fixed environment (`layers.c:29-30`) gains `XDG_CONFIG_DIRS=<root>` and `PROGRAMDATA=<root>`, `<root>` from `LAYERS_ROOT` (a string `examples/c/CMakeLists.txt` defines as `${CMAKE_CURRENT_SOURCE_DIR}`, the directory that holds `layers/`; `#ifndef LAYERS_ROOT` the example uses `".."`, which the XDG rule ignores, so it still runs and finds no drop-in), joined with `strcpy`/`strcat` into a static buffer (C89). The builder lines become, in this order: `cf_builder_env(b, "env", CF_RANK_ENV, "LAYERS_", env);` first (so that the call below reads this environment), `cf_builder_file(b, "app.toml", 35, "app.toml", NULL, 0);` as now, `cf_builder_app_files(b, "layers", "toml");`, then `.env` and the command line as now. The place `<root>/layers/` is the example's own directory: its `conf.d/` is the drop-in directory, at rank 28, and `<root>/layers/config.toml` does not exist, which is the point: it is listed and says `supplied nothing`. Values and winners are those of before (`app.toml` 35, `conf.d` 28 where it was 30: still below the `.env` at 15 and above `app.toml`). The header comment (`layers.c:2-17`) says `conf.d/, in the platform's system place (28)` where it said `conf.d/, every file in name order (30)`. macOS's system place is a fixed path and cannot be pointed at the example, so the expectation is registered `if(NOT APPLE)`; on macOS the example is built and run by `example_expect(layers ...)` with no lines but the clean exit (zero patterns are allowed, `examples/CMakeLists.txt:17-22`). The expected lines (`examples/c/CMakeLists.txt:85-100`) change in one: `"conf.d +12 +selected +conf.d/90-local.ini"` becomes `"layers.conf.d +12 +selected +.*layers.conf.d.90-local.ini"` (`.` for either separator: the source is now named by its path); the six value lines stay, and one is added: the note that a path supplied nothing is `cf_inspect`'s, not `cf_explain`'s, so after the six keys the example prints, line by line (`strchr` over a static buffer of 4096), the lines of `cf_inspect`'s text that hold `supplied nothing` (one: `<root>/layers/config.toml`, rank 30, which does not exist), and the expectation is `"source .*layers.config.toml. supplied nothing"`.
  * **Rejected**: `cf_app` reading the files (it would change what an existing program reads by the machine's directories); a path per platform in the caller's hands (the point is that the caller writes none); reading XDG's `/etc/xdg` default (the Roadmap says `/etc`, where a program's system file is expected); a registry or `NSUserDefaults` place (they are `cf_builder_registry` and `cf_builder_preferences`, already there).
  * **D-124** (index row `| D-124 | A program's files are read from where its platform keeps them, on request | sources, platforms | **Decided** |`):

    ```
    ## D-124 — A program's files are read from where its platform keeps them, on request

    **Question.** Design.md promised that `cf_app` reads the user's and the
    system's files from the platform's places; D-09 closed `cf_app` without them.
    Every program now writes the XDG, `%APPDATA%` and `~/Library/Application
    Support` code itself. How does the library own it without `cf_app` reading
    files nobody named?

    **Options.**

    1. `cf_app` reads them. Rejected: a program that reads no file today
       would start to, by the machine's directories, and D-09's Tier 0 would no
       longer be the same on every machine.
    2. A builder call, opt-in, that adds them as ordinary file sources
       (decided).
    3. Leave it to every program. Rejected: it is the repeated code.

    **Decision.** Option 2: `cf_builder_app_files(b, name, format)`. The system
    file at `CF_RANK_SYS_FILE` with its `conf.d` just above it, the user's file
    at `CF_RANK_USER_FILE`;
    POSIX: `$XDG_CONFIG_DIRS` (first three absolute entries) else `/etc`, and
    `$XDG_CONFIG_HOME` else `$HOME/.config`; Windows: `%PROGRAMDATA%` and
    `%APPDATA%`; macOS: `/Library/Application Support` and
    `~/Library/Application Support`. A value that is not an absolute path is
    ignored. Each path is a source named by itself, optional, so that a path
    looked at and empty is in the report, and each has a rank of its own
    (explain() and the report tell sources apart by rank): the system file is
    30 and its drop-in directory 28, the user's file 20, a later
    `$XDG_CONFIG_DIRS` entry 3 more each. A drop-in beats the file beside it,
    and an earlier entry beats a later.

    **Consequence.** `cf_app` is unchanged. The planner takes the platform and
    the environment as arguments, so every platform's paths are tested on every
    host. A profile (D-120) adds a file at the base file's rank minus one.
    ```
* **files**: `lib/config/tests/test_appfiles.c` (new), `lib/config/tests/CMakeLists.txt`, `lib/config/src/sources/appfiles.c` (new), `lib/config/CMakeLists.txt`, `scripts/generate/amalgamate.py`, `lib/config/include/configforge/cf_source.h`, `lib/config/include/configforge/configforge.h`, `lib/config/src/core/config.c`, `examples/c/layers.c`, `examples/c/CMakeLists.txt`; last: `cmake/configforge.experimental.symbols` and `cmake/configforge.internal.symbols` (generated), `docs/Decisions.md`, `docs/Guide.md`, `docs/Design.md`, `CHANGELOG.md`, `docs/Roadmap.md`, `docs/Codemap.md` and `docs/API.md` (generated).
* **steps**:
  1. Read `config.c:48-99` (the builder), `:240-283`, `:314-343`, `:416-479`, `:728-767`; `lib/config/include/configforge/cf_source.h:25-33`; `examples/c/layers.c` whole; `examples/c/CMakeLists.txt:55-100`.
  2. Write `lib/config/tests/test_appfiles.c` (C89, `#include "cf_test.h"`), added to `CF_UNIT_TESTS` after `test_places` (`lib/config/tests/CMakeLists.txt`, line 83), its fixtures made by CMake: after line 94 (the `file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/cf_encoding.d)` line, not inside the comment that follows it), `set(cf_appfiles "${CMAKE_CURRENT_BINARY_DIR}/cf_appfiles.d")`, `file(MAKE_DIRECTORY ${cf_appfiles}/sys1/myapp/conf.d ${cf_appfiles}/sys2/myapp ${cf_appfiles}/home/myapp)` and `file(WRITE ...)`: `sys1/myapp/config.ini` = `[server]\nworkers = 2\nhost = sys1\n[log]\nlevel = warn\n`; `sys1/myapp/conf.d/10-a.ini` = `[server]\nworkers = 4\n`; `sys2/myapp/config.ini` = `[server]\nworkers = 3\nhost = sys2\n`; `home/myapp/config.ini` = `[server]\nworkers = 16\n`; and after `cf_unit_tests(${CF_UNIT_TESTS})` (line 104) `target_compile_definitions(test_appfiles PRIVATE CF_APPFILES_DIR="${cf_appfiles}")`. The cases, each a function; a helper `static const char* const* env(...)` builds a fixed `char*` vector from `static char` strings:
     a. `cfi_app_places` POSIX: `XDG_CONFIG_HOME=/h/c`, `XDG_CONFIG_DIRS=/a:/b` -> in order `/a/myapp/config.toml` (30), `/a/myapp/conf.d` (28, dir), `/b/myapp/config.toml` (33), `/b/myapp/conf.d` (31, dir), `/h/c/myapp/config.toml` (20); with `HOME=/home/u` and no `XDG_CONFIG_HOME`: `/home/u/.config/myapp/config.toml`; no variables: `/etc/myapp/config.toml` (30), `/etc/myapp/conf.d` (28) and no user place; a relative `XDG_CONFIG_HOME=rel` ignored (falls to `HOME`); `XDG_CONFIG_DIRS=rel:/x` -> only `/x`; `XDG_CONFIG_DIRS=rel` -> `/etc`; an empty `XDG_CONFIG_HOME=` is unset; four absolute entries -> the first three; a trailing slash `XDG_CONFIG_HOME=/h/c/` -> `/h/c/myapp/...`.
     b. Windows: `ProgramData=C:\ProgramData`, `appdata=C:\Users\u\AppData\Roaming` (spelled as a Windows process may spell them) -> `C:\ProgramData\myapp\config.toml`, `C:\ProgramData\myapp\conf.d`, `C:\Users\u\AppData\Roaming\myapp\config.toml`; `APPDATA=relative` and `PROGRAMDATA=` ignored; a UNC `\\srv\share` accepted; `XDG_CONFIG_DIRS` and `HOME` do nothing on this platform.
     c. macOS: `HOME=/Users/u` -> `/Library/Application Support/myapp/config.toml`, `/Library/Application Support/myapp/conf.d`, `/Users/u/Library/Application Support/myapp/config.toml`; no `HOME`: no user place.
     d. A profile: `profile = "prod"` adds `config.prod.toml` beside each base file, rank `29 + 3k` for the k-th system base (after its drop-in directory) and 19 for the user (after the user's file), none beside a drop-in directory; with `XDG_CONFIG_DIRS=/a:/b` the order is `/a` file 30, drop-in 28, profile 29, `/b` file 33, drop-in 31, profile 32, user 20, user profile 19; the same call with `profile = NULL` has none of the profile places. No two places of any call in a to d share a rank (a loop over the returned places checks it).
     e. Limits: `cap` below `CF_APP_PLACES_MAX` gives `(size_t)-1`, and so does no memory (`cf_arena_fixed` over a 1024-byte buffer that the test first fills with `cf_arena_alloc` of 64 bytes until it returns NULL, then passes as `ar`); a name or a base long enough to pass `CF_PATH_MAX` (`CF_PATH_MAX + 1` bytes of `x`) gives `(size_t)-1` as well (the path would not fit the platform layer's buffers, `docs/Limits.md:33`).
     f. The builder, a real directory: `cf_build(NULL)`, `cf_builder_env(b, "env", CF_RANK_ENV, "MYAPP_", e)` with `e` holding `XDG_CONFIG_HOME=<CF_APPFILES_DIR>/home`, `XDG_CONFIG_DIRS=<CF_APPFILES_DIR>/sys1:<CF_APPFILES_DIR>/sys2`, `APPDATA=<CF_APPFILES_DIR>/home`, `PROGRAMDATA=<CF_APPFILES_DIR>/sys2` (static buffers, `strcpy`/`strcat`), then `cfi_builder_app_files(b, "myapp", "ini", plat, e)` with `plat` `CF_APP_WINDOWS` under `#if CF_PLATFORM_WIN32` and `CF_APP_POSIX` otherwise (a Windows host has `C:/...` fixture paths, which are not absolute on POSIX), the contract `cf_builder_int(b, "server.workers", 1, 1, 64, 0)`, `cf_builder_text(b, "server.host", "default", 0)`, `cf_builder_text(b, "log.level", "info", 0)`, `cf_builder_unknown(b, CF_UNKNOWN_REJECT)`. Expect `server.workers` 16 (the user's file, rank 20, beats the system's drop-in, 28, and file, 30), `server.host` `sys1` and `log.level` `warn` (the first `XDG_CONFIG_DIRS` entry beats the second; on a Windows host the one base is `sys2`, so `sys2` and `info`: the case says both under `#if CF_PLATFORM_WIN32`); `cf_explain(c, "server.workers", ...)` names `config.ini` of the user's place and `10-a.ini` of `sys1`'s drop-in as the one it beat; `cf_inspect(c, ...)` holds `note: source '<path>' supplied nothing` for a path that does not exist (`<CF_APPFILES_DIR>/sys2/myapp/conf.d`, the same on both branches, written in the platform's separators) and the report (`cf_report`) holds that path in `"sources"` (`#if CF_WITH_REPORT`).
     g. A drop-in directory beats the file beside it (28 against 30): with `sys1` the only system base (`XDG_CONFIG_DIRS=<CF_APPFILES_DIR>/sys1`, `PROGRAMDATA=<CF_APPFILES_DIR>/sys1`) and no user place (no `XDG_CONFIG_HOME`, no `HOME`, no `APPDATA`), `server.workers` is 4, from `10-a.ini`, over the file's 2.
     h. Refusals: `cf_builder_app_files(b, "my/app", "ini")`, `".."`, a 65-byte name, `""` and NULL give `CF_FALSE` and `cf_builder_create` fails with `is not a program name`; `format` `"tmol"` fails with `this build has no format 'tmol'`; a NULL builder returns `CF_FALSE` without crashing.
     i. The native call: `cf_builder_app_files(b, "myapp", "ini")` with an environment whose four variables name `<CF_APPFILES_DIR>/none` (a directory that does not exist): `cf_builder_create` succeeds, `cf_inspect` has `supplied nothing`, and a path in it holds `myapp`.
     Every case names its format `"ini"` (`toml` would need `CF_WITH_TOML`); cases a to e call only the planner, which takes any format text, and h checks the name before the format, so that a build without INI refuses the name as the others do; f, g and i read files and are under `#if CF_WITH_INI && CF_WITH_DIR`. In h the format `"tmol"` is refused in every build.
  3. Declarations and bodies that do nothing: the enum, `cf_app_place`, `cfi_app_places` returning 0, `cfi_builder_app_files` and `cf_builder_app_files` returning `CF_FALSE`, the three ranks (`CF_RANK_USER_PROFILE`, `CF_RANK_SYS_DROPIN`, `CF_RANK_SYS_PROFILE`); so that it links. **See it fail**: `ctest --test-dir build -R test_appfiles --output-on-failure` exits non-zero and names a to i.
  4. `appfiles.c`: the planner, as decided: `appf_env` (exact lookup through `cf_env_value`, `cf_entry.h:148-149`; a case-folding lookup for the Windows names), `appf_absolute(platform, text)`, `appf_join`, the three platforms' functions, `cfi_app_places`. Added to `lib/config/CMakeLists.txt` after `src/sources/snapshot.c` (line 42) and to `amalgamate.py` after `"config/src/sources/snapshot.c"` (line 78).
  5. `config.c`: `char* const* env` in `cf_builder` (after `cli_expanded`, line 98); `cf_builder_env` sets it (line 416-425); `cfi_builder_app_files` and `cf_builder_app_files` (after `cf_builder_response_files`, line 307): validate `name` and `format`, plan, then for each place `cf_builder_dir(b, path, rank, path, NULL, CF_OPTIONAL)` or `cf_builder_file(b, path, rank, path, format, CF_OPTIONAL)`.
  6. `ctest --test-dir build -R "test_appfiles|test_file|test_sources|test_rename"` exits 0.
  7. `layers.c` and `examples/c/CMakeLists.txt` as decided: `target_compile_definitions(layers PRIVATE LAYERS_ROOT="${CMAKE_CURRENT_SOURCE_DIR}")` after the `foreach` (line 83) and the expectation under `if(NOT APPLE)`. `ctest --test-dir build -R "example.layers"` exits 0, and so do the twins that read the same directory where they are built (`ctest --test-dir build -R "layers"`: `example.layers_cpp`, the Rust and Python ones); read the output once and compare it with the old one: the same six values.
  8. `docs/Decisions.md` D-124 and its row; `docs/Guide.md` section 4 (`docs/Guide.md:224-278`), a subsection "A program's files where the platform keeps them" before section 5 (line 279): the table above, the three lines of code (`cf_app_builder`, `cf_builder_app_files(b, "myapp", "toml")`, `cf_builder_create`), what a missing file does, the order; `docs/Design.md:316-320`: the sentence "is planned as an opt-in (Roadmap, ...)" becomes "is `cf_builder_app_files` (D-124)"; `CHANGELOG.md`; `docs/Roadmap.md`.
  9. The closing steps, then ACCEPT.
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS (the Windows branch of f), ACCEPT-CI (macOS runs the POSIX paths through `cfi_builder_app_files` with an explicit platform; `example.layers` has no expectation there) as `docs/Plan.md:71-114` give them; `ctest --test-dir build -R "test_appfiles|example.layers"` exit 0; `gate.abi` exit 0 with exactly `cf_builder_app_files 1.9` added to the experimental list and `cfi_app_places`, `cfi_builder_app_files` to the internal one. Done when (Roadmap): the three platforms' paths are tested with a fixed environment (a, b, c) and `layers.c` uses it (step 7).
* **stop if**: `cfi_app_places` cannot be made to return the same strings for a Windows environment on a POSIX host (a function of `environment` and the platform argument only; a call to `getenv`, `getcwd` or `stat` inside it is the fault: stop); two places of one call have one rank, or `cf_explain` names the wrong source for a value of `sys1`'s drop-in in case f (then the lookup by rank, `explain.c:17-29`, has changed: stop and write what it says); the example's expected output differs from the old in a value, not in a path.
* **docs**: `docs/Decisions.md` D-124 and its row; `docs/Guide.md` and `docs/Design.md` as step 8; `CHANGELOG.md` under `## 1.9.0 — unreleased`, `### Added`: "`cf_builder_app_files(b, name, format)` (experimental in 1.9, C-11): a program's system file and `conf.d` and its user's file, read from where its platform keeps them (`$XDG_CONFIG_DIRS` or `/etc` and `$XDG_CONFIG_HOME` or `~/.config`; `%PROGRAMDATA%` and `%APPDATA%`; `/Library/Application Support` and `~/Library/Application Support`), a missing file no error and each path in the report; `cf_app` is unchanged; `examples/c/layers.c` uses it"; `docs/Roadmap.md:805` the item "**A program's files where its platform keeps them.**": `⬜` to `🟩` and `(D-124, C-11)` after the bold heading.

## C-9-m - One key split into two: `migrate_from` on the new key's definition

* **id**: C-9-m
* **goal**: a new key's definition can name the old key it comes from and carry a function that reads the old key's value and yields this key's, so that `address = "db:5432"` written for version 1 reaches `database.host` as `db` and `database.port` as 5432, each at the rank the old value had, with `explain()` saying `(from address, converted)` on both.
* **why**: C-9 converts one key into one (`timeout_s` to `timeout_ms`) and says in its answers that splitting one key in two is not built: one `replaced_by` names one key and its function returns one value. A key that became `host` and `port` needs a definition on each new key that names the old one, and today a program writes both defaults and converts its users' files once, by hand.
* **needs**: C-9
* **size**: medium
* **version**: 2.1.0
* **status**: parked: not now
* **decided**:
  * **The members.** Three appended to `cf_def` after C-9's `migrate_user` (D-51: a descriptor smaller than they are has them NULL, as `DEF_SIZE_MIGRATE` handles C-9's two): `const char* migrate_from_key;` (the old key's dotted name), `cf_migrate_fn migrate_from;` (C-9's function type, unchanged: `in` is the old key's value as the old key declares it, `out` a value of this key's type), `void* migrate_from_user;` (borrowed, as `migrate_user` is). `cf_def_init` sets all three NULL; no default of any existing definition changes.
  * **When it is allowed.** `migrate_from != NULL` needs `migrate_from_key != NULL`, a scalar key (`list_mode == CF_LIST_NONE`), an old key that is defined, `CF_DEPRECATED`, scalar, not itself and not the new key's own `replaced_by`, and that has no `migrate_from_key` of its own (no chains: one step, so that the order of the passes never matters); the old key has no `replaced_by` (a key that is replaced by two keys is replaced by neither). `def_check` refuses otherwise: `CF_E_DEFINITION`, `'<key>': migrate_from needs a deprecated, defined, scalar old key without replaced_by, not itself`. Several new keys may name the same old key: that is the split.
  * **Where it runs.** In C-9's rename pass of `cf_builder_create`, once for each new key that has `migrate_from`, through C-9's `cf_ks_rename_with(ks, old, new, hook, ctx)` with a hook of the new key's own (`ctx`: the old and the new `cf_def`, the arena, the error): every candidate of the old key is copied to the new key at its rank, converted by that function, so precedence then chooses as for any key (a command line `--address=x:1` beats a file's `database.host`). The hook is C-9's `migrate_hook` with the function it calls taken from the new definition instead of the old: the template refusal, the typed or textual input, the skip of a value that does not convert, the failure and sensitive-key messages (`<old>: the migration to <new> failed: <why>`, no `<why>` when the old key or the new key is `CF_SENSITIVE`), the type check of the result, the arena copy of a text. Failing one new key fails the build: a half-split value is never offered.
  * **What each side reads.** `cf_get_*("address")` still gives the old key's own value (as C-9 says); the new keys get the pieces. `cf_derive` runs the functions again for each candidate, as C-9 decides: the function is pure and cheap, which its comment in `cf_define.h` already says.
  * **Rejected**: a function that yields several keys at once (one function a key keeps the type check and the failure message per key); the old key's definition carrying the list of new keys (it would be edited every time a key is split further, and a key's definition should say where it comes from, as `replaced_by` says where it went).
* **files**: `lib/config/include/configforge/cf_define.h`, `lib/config/src/core/define.c`, `lib/config/src/core/config.c`, `lib/config/tests/test_migrate.c` (cases added), `examples/c/migration.c` (a fourth scene), then `docs/Guide.md`, `CHANGELOG.md`, `docs/Decisions.md`, `docs/Codemap.md` and `docs/API.md` (generated).
* **steps**:
  1. `git switch -c work/C-9-m main` (after C-9). Cases added to `test_migrate.c`: (a) `address = "db:5432"`, `database.host` text with `split_host`, `database.port` int with `split_port`: `db` and 5432, `explain` of each has `(from address, converted)`, `address` reads `db:5432`; (b) precedence: a command line `address` beats a file's `database.port`; (c) a value without a colon: both functions fail, the build refuses naming the old key and the new one first (`database.host`, in definition order), and nothing is offered; (d) the new key written directly in a file and `address` in another at a lower rank: the direct one wins; (e) a sensitive old key: the messages show no value; (f) a result of the wrong type; (g) the five refusals of `def_check` (no old key defined, not deprecated, with its own `migrate_from_key`, itself, a list key); (h) `cf_derive` with a new `address` override: both keys change; (i) a descriptor of the earlier size: the members are NULL.
  2. See it fail: `ctest --preset default -R test_migrate --output-on-failure | tail -n 20`.
  3. The members and `def_check`; the loop in `config.c` over the definitions with `migrate_from`; `ctest --preset default -R "test_migrate|test_rename|test_define|test_derive"` exit 0.
  4. The example's scene; the Guide paragraph. Format; codemap and apiref last; ACCEPT; commit `C-9-m: one key split into two, migrate_from on the new key`.
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI; `ctest --preset default -R "test_migrate|example.migration"` exit 0; every existing test unchanged; `gate.abi` shows no change to the promised list beyond `cf_def`'s three members.
* **stop if**: C-9's rename pass cannot run a second hook for the same old key without a second copy appearing in an existing test (`test_rename`'s one copy for each value: report); `gate.abi` reports a change to an existing symbol.
* **docs**: `CHANGELOG.md`, "### Added": "`cf_def.migrate_from_key` and `migrate_from`: a key split in two, each new key converting the old key's value (C-9-m)"; `docs/Decisions.md`: D-122 extended ("a split is a function on each new key's definition").
* **Organiser's answers**:
  * Written from C-9's answer of 2026-11-26 (the maintainer: "I approve your recommendations"): option (b), `migrate_from` on the new key's definition, as a parked card that opens only when the maintainer or a user asks (docs/plan/Layout.md, "Not now"). One function a new key, no chains, and `cf_def` grows by three members only (the maintainer's standing approval, 2026-11-27).

## C-10c-m - The report's channel on Windows: an `AF_UNIX` socket behind the platform's descriptor functions

* **id**: C-10c-m
* **goal**: on Windows (10 and later, where `AF_UNIX` exists) a program that accepted a connection on a Winsock `AF_UNIX` socket answers `cf_serve_report` as on POSIX, with no change to `serve.c`, and `configforge doctor pull unix:<path>` reads it; before this card the Windows build answers `CF_E_CAPABILITY`.
* **why**: C-10a's answers (D-229) put the channel a report is served on behind the platform layer's descriptor functions (`cf_platform_has_fd_io`, `cf_platform_fd_is_socket`, `_nonblock`, `_read`, `_write`), POSIX only, "so that Windows is added later without touching the library". An operator of a Windows service has the same need as one of a daemon, and `curl --unix-socket` works on Windows 10 too.
* **needs**: C-10a, C-10b
* **size**: medium
* **version**: 2.1.0
* **status**: parked: not now
* **decided**:
  * **The channel**: an `AF_UNIX` stream socket, not a named pipe. It carries the same HTTP/1.0 wire as POSIX, so `cf_serve_report`, `curl --unix-socket` and Doctor's `pull` are one protocol on every system; a named pipe (`\\.\pipe\name`, `ReadFile` and `WriteFile`, overlapped waits, no `curl`) would be a second code path and a second address syntax for the same job. Rejected: a named pipe.
  * **The descriptor.** `cf_serve_report` keeps its `int fd` (a change would break C-10a's callers). On Windows the `int` holds the `SOCKET` value: Windows keeps 32 bits significant in kernel handle values, which Microsoft documents for 64-bit processes, so the conversion loses nothing; the platform part converts back with `(SOCKET)(unsigned)fd`, and a value of -1 is invalid. The header comment of `cf_serve_report` says so. A program that holds a `SOCKET` passes `(int)s`.
  * **The platform part**, `lib/platform/src/win32/win32.c` (inside its existing guard; `<winsock2.h>` and `<afunix.h>` are included there and nowhere else): `cf_platform_has_fd_io()` true; `cf_platform_fd_is_socket` by `getsockopt(SO_TYPE)`; `cf_platform_fd_nonblock` does nothing and returns 0 (Windows cannot read a socket's blocking mode back, and the read below never needs it: `WSAPoll` says when `recv` will not block); `cf_platform_fd_read(fd, buf, cap, timeout_ms)`: `WSAPoll` for `POLLRDNORM` then `recv` (-2 on a timeout, -1 on failure, 0 at the end of the stream); `cf_platform_fd_write`: `WSAPoll` for `POLLWRNORM` then `send` until every byte is out or the timeout. The library never calls `WSAStartup`: the program that accepted the connection has done it, which `docs/Safety.md` and the header comment say in one sentence. `cf_platform_has_fd_io` is the only query the rest of the library asks, so `serve.c` is not touched; `docs/platforms.txt` gets the row `descriptor I/O (cf_serve_report)|cf_platform_has_fd_io|yes|yes|E: CF_E_CAPABILITY, "cf_serve_report needs a descriptor"` (K-11c's gate refuses it otherwise).
  * **Doctor** (`tools/doctor/pull.c`): the Windows branch of `pull unix:<path>` uses Winsock (`WSAStartup` once at the start of `doctor_pull`, `socket(AF_UNIX, SOCK_STREAM, 0)`, `connect` to a `sockaddr_un` whose path is at most 103 bytes as on POSIX, so one command means the same everywhere); the refusals and exit codes are C-10b's, `cannot connect to <path>: <reason>` with the Winsock error's text. `pull` on a build without sockets keeps its refusal, reworded `doctor: pull: needs sockets`.
  * **The tests.** `lib/config/tests/test_serve.c`'s non-POSIX branch (the three-line `CF_E_CAPABILITY` case) becomes a Windows branch that runs C-10a's cases a to j on a pair made without `socketpair`: `socket`, `bind` to `cf_serve.sock` in the working directory (`DeleteFileA` first), `listen`, `connect`, `accept`; the cases that use `fcntl` (the descriptor's flags before and after: e, j) are replaced by "the socket is still usable after the call"; the `CF_E_CAPABILITY` case stays for the `none` platform. `tests/doctor/test_pull.c` gets a Windows branch that starts Doctor with `CreateProcessW` and standard output redirected to `cf_pull_out.json` through `STARTUPINFO`, with the same four checks as the POSIX branch.
  * **Rejected**: calling `WSAStartup` inside the library (global process state the program owns, D-101); a `void*` descriptor type (it would change C-10a's signature for the sake of one platform).
* **files**: `lib/platform/src/win32/win32.c`, `lib/platform/include/configforge/cf_platform.h` (comments), `lib/config/include/configforge/configforge.h` (the comment of `cf_serve_report`), `tools/doctor/pull.c`, `tests/doctor/test_pull.c`, `tests/doctor/CMakeLists.txt`, `lib/config/tests/test_serve.c`, `docs/platforms.txt`, then `docs/Safety.md`, `docs/Doctor.md`, `CHANGELOG.md`, `docs/Codemap.md` and `docs/API.md` (generated).
* **steps**:
  1. `git switch -c work/C-10c-m main` (after C-10a and C-10b), on a Windows machine with a Visual Studio prompt. Rewrite the Windows branch of `test_serve.c` and the Windows branch of `test_pull.c` first.
  2. See them fail: `cmake --preset default && cmake --build --preset default && ctest --preset default -R "test_serve|doctor.pull" --output-on-failure` (the functions say no).
  3. The five functions in `win32.c`; `pull.c`'s Windows branch; the table row. `ctest --preset default -R "test_serve|doctor.pull|gate.platforms"` exit 0 on Windows, and the same on Linux unchanged (WSL).
  4. `docs/Safety.md`, `docs/Doctor.md`, `CHANGELOG.md`. Format; codemap and apiref last; ACCEPT; commit `C-10c-m: the report served on a Windows AF_UNIX socket`.
* **accept**: ACCEPT-LINUX (unchanged), ACCEPT-WINDOWS (`test_serve` and `doctor.pull` run, not skipped), ACCEPT-CI (the Windows runner); `ctest --preset default -R "gate.platforms|gate.abi"` exit 0.
* **stop if**: the CI Windows image has no `afunix.h` or its `AF_UNIX` cannot bind in the working directory (report the image and the error); a `SOCKET` value does not fit an `int` on the machine (print it once: stop and report); `serve.c` needs a change (the interface is wrong: report what it needs).
* **docs**: `CHANGELOG.md`, "### Added": "`cf_serve_report` and `configforge doctor pull unix:<path>` on Windows 10 and later, over an `AF_UNIX` socket (C-10c-m)"; D-229 extended in `docs/Decisions.md` ("Windows is an `AF_UNIX` socket, not a named pipe").
* **Organiser's answers**:
  * Written from C-10a's answer of 2026-11-26 (D-229: an `AF_UNIX` socket or a named pipe behind the interface), as a parked card that opens only when a Windows program asks. The socket is chosen over the pipe for one wire and one address syntax on every system (the maintainer's standing approval, 2026-11-27).

## C-10d-m - `configforge doctor pull` reads `http://` and `tcp:` targets, with a timeout and a size as options

* **id**: C-10d-m
* **goal**: `configforge doctor pull http://host:port/report` and `configforge doctor pull tcp:host:port` ask a program that serves its report on a TCP address (an `AF_INET` socket its own loop accepted, which `cf_serve_report` already answers) and write the report to standard output, with `--timeout` and `--max-size` as options on all three kinds of target.
* **why**: C-10b reads `unix:` only, and says a report on a network address is the program's decision to expose and a client that follows redirects or speaks TLS is a second tool. A container or a remote host has no shared socket path, and `curl` plus a pipeline is what an operator writes today; the report is value-blind, so reading it over loopback or a private network is what Doctor is for.
* **needs**: C-10b
* **size**: medium
* **version**: 2.1.0
* **status**: parked: not now
* **decided**:
  * **The targets.** `http://<host>[:<port>][<path>]` (port 80 by default, path `/report` by default) and `tcp:<host>:<port>` (the port is required; the path is `/report`). `<host>` is a name, an IPv4 literal or a bracketed IPv6 literal (`[::1]`); it is resolved with `getaddrinfo` and each address is tried in turn, the timeout shared among them. The request is C-10b's: `GET <path> HTTP/1.0`, `Host: <host>[:<port>]`, `Connection: close`; the answer is read to the end, `HTTP/1.` and a `200` are required, `Content-Length` is checked when present, the body goes to standard output and nothing else does, as for `unix:`. A `3xx` is a refusal (`the program answered <status line>`): redirects are not followed.
  * **What is refused, and said.** `https://` (`pull: https is not supported; reach the program on a private address or terminate TLS in front of it`), credentials in the address (`pull: credentials in the address are not sent`), a port outside 1 to 65535, an empty host, `tcp:` without a port, any other scheme, and the C-10b refusals unchanged (`cannot connect to <target>: <reason>`, `no answer within <n> s`, `the answer is cut short`, `the answer is longer than <size>`). Exit codes are Doctor's: 2 for an unusable argument or answer, 3 cannot start; nothing is written to standard output on a refusal.
  * **The options (D-220).** `--timeout <seconds>` (the wait for the connection and for each part of the answer; default 10, as C-10b's fixed value; 0 is no limit, chosen explicitly) and `--max-size <bytes>` (the longest answer read; default 16777216, C-10b's 16 MiB; 0 is unlimited), accepted before the target on every kind, `unix:` included: C-10b's two numbers become these options' defaults and nothing about a call without a flag changes.
  * **Where.** `tools/doctor/pull.c`: the target is parsed into a kind, an address and a path (`pull_target`); the connect, request and read are one function over a connected descriptor for all three kinds (the code C-10b has for `unix:`). The sockets are under `#if CF_PLATFORM_POSIX`; on Windows the TCP kinds use Winsock in the branch C-10c-m adds, and before that card they exit 2 with `pull: needs a POSIX system`.
  * **Not here**: TLS, redirects, proxies and authentication (a client for the open Internet is a second tool; the report is read on a private address by design); a server in Doctor.
  * **The test** (`tests/doctor/test_pull.c`, POSIX): the parent `listen`s on `127.0.0.1` port 0, reads the port with `getsockname`, and runs Doctor with `http://127.0.0.1:<port>/report` and with `tcp:127.0.0.1:<port>` as C-10b's `unix:` case does (accept, `cf_serve_report`, close, the four checks); then the refusals: `https://h`, `http://u:p@h/`, `http://h:99999/`, `tcp:h`, a refused connection (a closed port), a listener that never answers with `--timeout 1` (exit 2, `no answer within 1 s`), an answer longer than `--max-size 100` (exit 2), and an `[::1]` run when the host has IPv6 loopback (otherwise skipped, and the test says so).
* **files**: `tools/doctor/pull.c`, `tools/doctor/adapters.h`, `tools/doctor/doctor.c` (the usage line), `tests/doctor/test_pull.c`, `tests/doctor/CMakeLists.txt`, then `docs/Doctor.md`, `docs/Guide.md`, `CHANGELOG.md`, `docs/Codemap.md` (generated).
* **steps**:
  1. `git switch -c work/C-10d-m main` (after C-10b). Write the TCP and refusal cases of `test_pull.c`.
  2. See them fail: `ctest --preset default -R doctor.pull --output-on-failure | tail -n 20`.
  3. `pull_target`, the shared connect-request-read function, the flags; `ctest --preset default -R "doctor"` exit 0.
  4. `docs/Doctor.md` ("From a running program" gains the two forms and the two options), the Guide lines, `CHANGELOG.md`. Format; codemap last; ACCEPT; commit `C-10d-m: doctor pull reads http and tcp targets, with a timeout and a size`.
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS (the TCP cases are not built there before C-10c-m; `doctor.version` and the rest exit 0), ACCEPT-CI (macOS and FreeBSD run `doctor.pull`); `ctest --preset default -R "doctor.pull|example.serve"` exit 0.
* **stop if**: C-10b has not landed or `pull.c` has no function the three kinds can share (report its shape); `getaddrinfo` is not available under C-10b's feature macros on a CI platform (report the platform and the message).
* **docs**: `docs/Doctor.md` and `docs/Guide.md` as step 4; `CHANGELOG.md`, "### Added": "`configforge doctor pull` reads `http://` and `tcp:` targets, and `--timeout` and `--max-size` on every target (C-10d-m)"; D-229 extended in `docs/Decisions.md` ("a report is read from a unix socket or a TCP address; never over TLS or a redirect").
* **Organiser's answers**:
  * Written from C-10b's answer of 2026-11-26 (`unix:` only; `http://` and `tcp:` are C-10d-m), as a parked card that opens only when the maintainer or a user asks. The fixed 10 seconds and 16 MiB of C-10b become the defaults of two options, so no call without a flag changes (the maintainer's standing approval, 2026-11-27).
