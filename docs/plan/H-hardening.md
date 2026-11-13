# Hardening, and checks at compile time

What is left of D-75 (checked at compile time where the language can) and
of the ideas the security pass of 2026-11-11 left in the Roadmap, as cards
(docs/Plan.md says what a card is and how one is carried out). Five pieces:

* **H-1, H-2**: D-75 in C11 - define macros whose constant key, default
  and bounds the compiler checks - and in the C that `doctor gen
  --schema` writes, for C11 and C++11.
* **H-3, H-4**: what one document may make of the key space, bounded once
  in the flattener for every reader (D-82), and a fuzz check that fails a
  document costing a multiple of its size.
* **H-5**: a directory that others can write to is not read as secrets,
  and on request not as drop-ins (D-83).
* **H-6**: Doctor's `--redact-all`, on by default in the GitHub Action.
* **H-7, H-8a, H-8b**: what a report shows of a secret it cannot know is
  one, and a secret's digest that cannot be tested against guesses
  without a key.

Order: H-1, H-2, H-3, H-4, H-6, H-5, H-7, H-8a, H-8b. H-1, H-3 and H-6 share
no file and can run side by side; H-2 needs H-1's macros and H-4 H-3's
constants. H-5 waits on one answer, H-7 on one and H-8a on one.

For the maintainer: three questions - in H-5 (whether a group may write to a
secrets directory), in H-7 (what a report shows of an undefined key whose
name is a secret's, mistyped) and in H-8a (where a digest's key comes
from). Nothing to install.

Done before these were written, and so not here: the marker-secret gate
for the library and for Doctor (`test_marker`, `doctor.marker`), the
seeded name tables (D-81), a new store file's permissions
(`cf_platform_create_private`), and an entry that is not a file refused
by name (D-78). The pass's note that "the platform listing returns the
entry type" is D-78's on POSIX; what is left of it - the type, owner and
mode of every entry seen in the listing's one pass - is in H-5.

Each card that adds a decision takes the next free D-number at the time
it is carried out (`git grep -n "^## D-" docs/Decisions.md`). The numbers
written below are what they will be if these cards are done in order and
nothing else takes one first; W3-F2b (docs/plan/W3-native-sources.md)
plans D-82 too, so whichever is committed second takes the next.

---

## H-2 - The contract `doctor gen` writes, checked by the compiler

* **id**: H-2
* **goal**: the C that `doctor gen --schema` writes checks each
  definition at compile time where the language it is compiled as can -
  every key by the library's rule from C++11, and from C11 a key's
  length, an int default against its range and a text default against
  its length bounds - and compiles unchanged as C89.
* **why**: a generated contract is compiled into a build Doctor never
  sees: one with a smaller `CF_KEY_MAX`, or from a schema whose text
  default is longer than its `maxLength`, which the library refuses only
  when the program runs. Checked by the compiler, a contract that cannot
  explain its own values never ships.
* **needs**: H-1
* **size**: medium
* **decided**:
  * The checks are written as macros the generated file defines for
    itself, by the language it is compiled as, and calls once a check.
    The preamble, written right after `#include <configforge/configforge.h>`
    and the blank line that follows it (and before the `CF_WITH_REAL`
    `#error` block when there is one), exactly:

    ```c
    /* Each definition is checked by the compiler where the language can
       (D-75): its key by the library's rule from C++11; from C11 its
       key's length, an int default against its range and a text default
       against its length. cf_builder_create checks all of it, in every
       language. */
    #ifdef __cplusplus
    #include <configforge/configforge.hpp>
    #endif
    #if defined(__cplusplus) && CF_CPP11
    #define CF_GEN_KEY(k) static_assert(cf::key_is_valid(k), "configforge: not a canonical key")
    #define CF_GEN_INT(d, lo, hi) static_assert((lo) <= (d) && (d) <= (hi), "configforge: a default outside its own range")
    #define CF_GEN_REAL(d, lo, hi) static_assert((lo) <= (d) && (d) <= (hi), "configforge: a default outside its own range")
    #define CF_GEN_LEN(t, lo, hi) static_assert(sizeof(t) > (lo) && ((hi) == 0 || sizeof(t) <= (hi) + 1), "configforge: a text default longer than its max_len, or shorter than its min_len")
    #elif CF_HAVE_C11 && !defined(__cplusplus)
    #define CF_GEN_KEY(k) CF_C11_KEY_(k)
    #define CF_GEN_INT(d, lo, hi) CF_C11_RANGE_(d, lo, hi)
    #define CF_GEN_REAL(d, lo, hi) ((void)0)
    #define CF_GEN_LEN(t, lo, hi) CF_C11_LEN_(t, lo, hi)
    #else
    #define CF_GEN_KEY(k) ((void)0)
    #define CF_GEN_INT(d, lo, hi) ((void)0)
    #define CF_GEN_REAL(d, lo, hi) ((void)0)
    #define CF_GEN_LEN(t, lo, hi) ((void)0)
    #endif
    ```

    and after the function's closing brace, `#undef` of the four, one a
    line, so that two generated contracts can share a translation unit.
    The C11 branch is H-1's building blocks, so the rule is written once;
    the C++ branch is `cf::key_is_valid`, the constexpr rule of the C++
    part of D-75. C++'s `static_assert` can compare doubles, so reals are
    checked there; C11's cannot. In C89 each check is `((void)0);`, a
    statement, which is legal where the checks are written.
  * The C11 messages are H-1's. The C++ length message names both bounds
    because one `static_assert` checks both; the refused case below
    expects the words both branches share, "a text default longer than
    its max_len".
  * Where the generator writes each call, in `c_define`
    (`tools/doctor/contract.c`): `    CF_GEN_KEY(<key>);` on the line after
    `cf_def_init(&d, ...)`, the key written by `c_text`; and on the lines
    before `    ok = ok && cf_builder_define(b, &d);`:
    * an int with `has_range`, `has_default` and `list_mode == CF_LIST_NONE`:
      `    CF_GEN_INT(<default>, <imin>, <imax>);`, each by `c_i64`;
    * a real with the same three, under `#if CF_WITH_REAL` in the
      generator: `    CF_GEN_REAL(<default>, <rmin>, <rmax>);`, each `%.17g`;
    * text with `has_default`, `list_mode == CF_LIST_NONE` and
      `min_len != 0 || max_len != 0`: `    CF_GEN_LEN(<default>, (size_t)<min>u, (size_t)<max>u);`,
      the default by `c_string`, the bounds as `c_define` writes them.
    A list's defaults are not checked at compile time: `cf_define`
    checks each element's length when the definition is made, as it did
    when Doctor read the schema.
  * The preamble is long: write it from a `static const char* const gen_checks[] = { ..., NULL }`
    of lines, each under 509 characters, put with `fputs` in a loop
    (the formatting rule: no literal continued over lines as a call's
    argument). The `#undef` lines likewise, from their own array.
  * `int` ranges in C++: `c_i64` writes `(cf_i64)8080`, `CF_I64_MIN`,
    `CF_I64_MAX` or the shifted form, each a constant expression in C++
    as in C. A range with `imin == imax` is a one-value range in a
    `cf_def` (`has_range` set); C11's `CF_C11_RANGE_` treats it as no
    range, as `cf_builder_int` does, and so checks less there - the
    library checked it when Doctor read the schema.
  * Tests, in `tests/doctor/CMakeLists.txt` inside the existing
    `if(NOT CMAKE_CROSSCOMPILING)` block, after `add_test(NAME doctor.gen ...)`:
    * the same contract generated a second time as C++, `${CMAKE_CURRENT_BINARY_DIR}/gen_contract.cpp`
      (a second `add_custom_command`, the same arguments but `--out`);
    * `add_library(doctor_gen_c11 OBJECT ${gen_c})`, `C_STANDARD 11`,
      `C_STANDARD_REQUIRED ON`, `C_EXTENSIONS OFF`,
      `target_link_libraries(doctor_gen_c11 PRIVATE configforge)`,
      `configforge_apply_warnings(doctor_gen_c11)`, under
      `if("c_std_11" IN_LIST CMAKE_C_COMPILE_FEATURES AND NOT CF_FORCE_PORTABLE)`;
    * `add_library(doctor_gen_cxx11 OBJECT ${gen_cpp})`, `CXX_STANDARD 11`,
      `CXX_STANDARD_REQUIRED ON`, `CXX_EXTENSIONS OFF`, linked to
      `configforge`, with `-pedantic -Werror -Wall -Wextra` for GNU and
      Clang and `configforge_apply_warnings` for MSVC, as
      `tests/unit/CMakeLists.txt` sets them for `test_wrapper_access_cxx*`,
      under `if(CF_WITH_CXX AND "cxx_std_11" IN_LIST CMAKE_CXX_COMPILE_FEATURES)`.
    Both objects are built by every build: the good contract must
    compile with its checks, which is the test that it does.
  * A contract that must not compile: `tests/doctor/contract_refused.schema.json`
    (new), exactly
    `{"$schema": "https://json-schema.org/draft/2020-12/schema", "type": "object", "properties": {"server": {"type": "object", "properties": {"mode": {"type": "string", "maxLength": 4, "default": "production"}}}}}`
    and a newline: a schema the library reads (`cf_define` does not
    compare a scalar text default with its bounds) and refuses only at
    create. Generated twice, `gen_refused.c` and `gen_refused.cpp` (name
    `gen_refused`), each by an `add_custom_command` as above, and:
    `include(ConfigForgeMustNotCompile)`;
    `cf_must_not_compile(doctor.refused.gen.c11 SOURCE ${gen_refused_c} LANG C STD 11 DEFINE CF_GEN_REFUSED=1 EXPECT "a text default longer than its max_len")`
    under the C11 condition, and
    `cf_must_not_compile(doctor.refused.gen.cxx11 SOURCE ${gen_refused_cpp} LANG CXX STD 11 DEFINE CF_GEN_REFUSED=1 EXPECT "a text default longer than its max_len")`
    under the C++ one. (`DEFINE` is required by the helper and harmless
    here.)
  * Rejected: writing the generated definitions through H-1's
    `CF_DEFINE_INT` and `CF_DEFINE_TEXT`. A schema's key has a
    description, a policy, aliases, a list, an enum - what only a `cf_def`
    carries - and the generated code must stay C89.
  * Rejected: Doctor refusing the schema itself at `gen`. It would catch
    the refused case above, and not a build whose `CF_KEY_MAX` is smaller
    than Doctor's; the compiler of the build that uses the contract is
    the only one that knows its limits.
* **files**: `tools/doctor/contract.c`, `tests/doctor/CMakeLists.txt`,
  `tests/doctor/contract_refused.schema.json` (new); last: `docs/Doctor.md`,
  `docs/Decisions.md` (D-75), `CHANGELOG.md`, `docs/Roadmap.md`.
* **steps**:
  1. `git grep -n "CF_C11_KEY_" include/configforge/configforge.h`. No
     match: H-1 is not done; stop.
  2. Write the schema file and the CMake of the decided tests.
  3. See it fail: `cmake --preset default && cmake --build --preset default && ctest --preset default -R "doctor\.refused\.gen"` -
     both fail (the generated code has no checks, so it compiles).
  4. Write the preamble, the per-definition calls and the `#undef`s in
     `contract.c`. Rerun step 3: both pass. `ctest --preset default -R "doctor\.gen"`
     passes, and the build compiled `doctor_gen_c11` and `doctor_gen_cxx11`
     (they are in `ALL`).
  5. Read the generated `gen_contract.c` in the build directory once: one
     `CF_GEN_KEY` per definition, `CF_GEN_INT(... (cf_i64)8080 ...)` for
     `server.port`, `CF_GEN_LEN("0.0.0.0", (size_t)1u, (size_t)253u)`
     for `server.host`, none for `tags` (a list).
  6. Docs (below). `cmake -P scripts/format.cmake`. ACCEPT-LINUX,
     ACCEPT-WINDOWS.
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS (the C++ object and both
  refusals under MSVC), ACCEPT-CI.
* **stop if**: `doctor gen --schema tests/doctor/contract_refused.schema.json`
  exits 2 (the library refuses the schema, so the case to test is
  another - hand back); `doctor_gen_cxx11` does not compile for a reason
  other than a check (the generated C is not valid C++ somewhere - name
  the line under notes); or H-1's notes say its macros were redesigned.
* **docs**: no new D-number. D-75: the "**Not yet.**" paragraph is
  removed, and a bullet after C11's: "**doctor gen**: the contract it
  writes checks each key by `cf::key_is_valid` from C++11, and from C11
  a key's length, an int default against its range and a text default
  against its length, through C11's own checks; as C89 it is what it
  was." "Held to" gains: "the generated contract compiled as C89, C11
  and C++11, and one from a schema whose text default is longer than its
  `maxLength`, which must not compile as C11 or C++11." Doctor.md, the
  `gen` section: one paragraph, "The generated file checks itself where
  the compiler can (D-75): compiled as C++11 or later each key is checked
  by `cf::key_is_valid`, as C11 a key's length and every int default and
  text default against its bounds, so a build with a smaller `CF_KEY_MAX`,
  or a schema whose default its own bounds refuse, does not compile. As
  C89 it is plain calls, checked by `cf_builder_create`." CHANGELOG, the
  unreleased section's D-75 entry from H-1 gains a sentence: "The C that
  `doctor gen --schema` writes is checked the same way, as C11 and as
  C++11." Roadmap: the "At a glance" row goes 🟩, its text "Checks at
  compile time where they can be, at run time always: Rust, C++, C11 and
  `doctor gen`'s output"; wave 3's B sentence says D-75 is done; group
  B's row in "What can run at once" is removed.
* **notes**:

---

## H-6 - Doctor's `--redact-all`, on in the action unless asked

* **id**: H-6
* **goal**: `doctor dump`, `explain`, `lint`, `diff`, `history` and
  `blame` take `--redact-all`, which shows no value of any key as if
  each were secret, and the GitHub Action passes it unless its new input
  `show-values` is `true`.
* **why**: a full report holds every value, and the action posts what
  Doctor says to a pull request, where anyone who can read the pull
  request reads the values - a URL with a token in it, an internal
  host. What changed is said, and which source won; what it changed to
  is the one thing a comment need not show.
* **needs**: nothing
* **size**: medium
* **decided**:
  * The flag, `--redact-all`, takes no value. In `tools/doctor/doctor.c`'s
    option loop for lint, diff, history and blame it is read as
    `--only-changed` is (`continue` before the `i + 1 >= argc` test), for
    all four; `dump <report> [--redact-all]` and
    `explain <report> <key> [--redact-all]` accept it as their one
    optional argument (anything else there: `usage()`). Usage gains one
    line after `--format`: `  --redact-all       show no value of any key, as for a secret`.
  * The library side, `include/configforge/cf_doctor.h`, after
    `cf_doctor_only_changed_file`'s block:

    ```c
    /* From now on, nothing this context reports shows the value of any
       key, as if each were sensitive: a finding says that a value
       changed, was refused or came from somewhere, not what it is.
       Comparisons still use the values a full report carries. For
       output that goes where the configuration should not, such as a
       pull-request comment. (1.4.1) */
    CF_DOCTOR_API void cf_doctor_redact_all(cf_doctor* d, cf_bool on);
    ```

  * In `tools/doctor/engine.h`: `struct cf_doctor` gains
    `cf_bool redact_all;` after `unchanged`, and `rkey` gains
    `cf_bool hidden; /* sensitive, or --redact-all: no value of it is shown */`
    after `sensitive`. `doctor_load` sets `rk->hidden = (rk->sensitive || D->redact_all) ? CF_TRUE : CF_FALSE;`
    right after it sets `rk->sensitive` (engine.c, `flag_at(... "sensitive")`).
    The rule, which every change below follows: **what is shown** goes by
    `hidden`; **what is compared and recomputed** - `told_by_digest`'s
    choice of digest or value for agreement, CFD019's fingerprint, CFD007
    - goes by `sensitive`, as now. A key is not made sensitive by the
    flag, so CFD019 and CFD007 say what they said.
  * `doctor_shown`: `k->hidden` in place of `k->sensitive`.
    `doctor_reason`: a key `hidden` and not `sensitive` shows its
    candidate's `reason_kind` when the report has one ("range", "type",
    ...), else "invalid", since a plain key's reason can quote its value
    ("99999 is outside 1..65535"); a sensitive key as now.
  * `rule_value_diff`: after the `told_by_digest` branch (CFD014,
    unchanged), a key with `ka->hidden || kb->hidden` gets
    `finding(a->D, "VALUE", "info", name, "differs (not shown: --redact-all)")`
    instead of the two values.
  * Every other place that writes a candidate's value, a default's value
    or a reason into a finding or a listing goes through `doctor_shown`
    or `doctor_reason`, or is guarded by `hidden` with a fixed text. Step
    1 finds them: `git grep -n -e "->value" -e "dflt" -e "reason" tools/doctor/engine.c tools/doctor/doctor.c`,
    read each that is an argument of `finding(`, `printf`, `fprintf`,
    `text_field` or a Markdown/SARIF writer, and list them in the step's
    commit message's draft. History's and blame's value lines, and
    CFD017's "defaults differ", are among them; a site that already
    tests `sensitive` for display tests `hidden` instead.
  * The action: `actions/doctor/action.yml` gains, after `comment-author`,

    ```yaml
      show-values:
        description: Show the reports' values in the log, the summary and the comment - true or false; a secret's is never shown
        required: false
        default: "false"
    ```

    and the "Lint and diff" step's `env` gains `CF_SHOW_VALUES: ${{ inputs.show-values }}`.
    `actions/doctor/run.sh`: its header lists `CF_SHOW_VALUES  true:
    show values (doctor without --redact-all); anything else hides them`,
    and after the `--severity` line,
    `if [ "${CF_SHOW_VALUES:-false}" != "true" ]; then opts+=(--redact-all); fi`
    - `opts` reaches every lint and diff it runs. The Markdown's closing
    sentence, when redacting, is "Secrets are compared by digest and
    never shown, and no other value is shown either (the action's
    `show-values`)." and otherwise as now. `pre-commit.sh` is not
    changed: it runs on the developer's own machine and posts nothing.
  * The action's default changes what an existing workflow's comments
    show: values disappear from them. That is the Roadmap's "done when"
    ("the action's default shows no value and a flag restores it"),
    decided when the item was accepted, and so not asked again here; the CHANGELOG
    says it under Security, where an upgrader looks.
  * Tests:
    * `tests/doctor/redact.cmake` (new), modelled on `secrets.cmake`
      (its `doctor_run` and `fail` functions, copied), added to the
      `foreach(script secrets marker refusals escape)` list in
      `tests/doctor/CMakeLists.txt` as `redact`. The needles are values of
      plain keys in the fixtures: `9090` (`server.port` from the
      environment in `cf_fx_prod.json`) and `127.0.0.1`, also searched as
      `127\.0\.0\.1` for Markdown's escape (`string(FIND ...)`, not a
      regex). The runs, each with `--redact-all`: `diff` prod staging in
      text, markdown and sarif; `lint` prod in all three; `history` prod
      staging; `blame server.host` prod staging; `dump` prod; `explain`
      prod `server.host` and `server.port`. Each must exit 0 (`--fail-on never`
      where the command takes it) and print no needle. Then the control:
      `dump cf_fx_prod.json` without the flag must print `9090`, so the
      test can fail.
    * `tests/doctor/action.cmake`: after the first `run_script` block, one
      more over the same pair with `CF_MARKDOWN` set and `CF_SHOW_VALUES`
      unset: the Markdown holds neither `9090` nor `127.0.0.1` nor
      `127\.0\.0\.1`; and one with `CF_SHOW_VALUES=true`: it holds `9090`.
    * `tests/doctor/test_api.c`: one case - `cf_doctor_redact_all(d, CF_TRUE)`,
      then `cf_doctor_diff` of two configurations whose `server.port`
      differs (8080, 9090): a finding for `server.port` whose message
      contains neither "8080" nor "9090".
* **files**: `include/configforge/cf_doctor.h`, `tools/doctor/engine.h`,
  `tools/doctor/engine.c`, `tools/doctor/doctor.c`,
  `actions/doctor/action.yml`, `actions/doctor/run.sh`,
  `tests/doctor/redact.cmake` (new), `tests/doctor/action.cmake`,
  `tests/doctor/test_api.c`; last: `tests/doctor/CMakeLists.txt`,
  `docs/Doctor.md`, `docs/Decisions.md`, `CHANGELOG.md`,
  `docs/Roadmap.md`.
* **steps**:
  1. The grep of the decided list; write down every display site.
  2. Write `redact.cmake`, the `action.cmake` blocks and the `test_api.c`
     case; register `redact`. Add `cf_doctor_redact_all` and the
     `--redact-all` parsing, both setting `D->redact_all` and nothing
     else reading it yet.
  3. See it fail: `cmake --build --preset default && ctest --preset default -R "doctor\.(redact|action|api)"` -
     `doctor.redact` fails on its first run (a needle printed), the API
     case fails, and `doctor.action` fails on the Markdown (on Windows
     `doctor.action` is not built: bash only).
  4. `hidden`, its setting at load, and every display site from step 1.
     Rerun step 3: all pass. `ctest --preset default -R "doctor\."`: all
     pass (secrets, marker and escape say what they said).
  5. The action's input and `run.sh`. Rerun `doctor.action`.
  6. Docs. `cmake -P scripts/format.cmake` (`cf_doctor.h` is in
     `docs/API.md`). ACCEPT-LINUX, ACCEPT-WINDOWS.
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI;
  `doctor.redact`, `doctor.action`, `doctor.api`, `doctor.marker` and
  `doctor.secrets` pass.
* **stop if**: a site from step 1 writes a value that cannot be told from
  other text by the code around it (a message built far from the key it
  names) - name it under notes and hand back; or making CFD019 or CFD007
  go by `hidden` turns out to be needed for a test to pass (then the rule
  above is wrong somewhere).
* **docs**: add D-84 (or the next free), "Doctor's action shows no value
  unless asked": the question (a full report's plain values in a public
  pull-request comment); options (as it was; redact every value always;
  a flag, on by default in the action; a flag, off everywhere); the
  decision (the third; comparisons still use the values; CFD019 and
  CFD007 are unchanged; the hook is not changed and why); compatibility
  (the action's comments and summaries lose their values; `show-values:
  true` brings them back). Doctor.md: `--redact-all` in the options list
  and in the action's inputs table, `show-values`. CHANGELOG, the
  unreleased section, "### Security": "**The Doctor action shows no value
  unless asked**: a full report's values no longer reach the log, the job
  summary or the pull-request comment; `show-values: true` restores
  them, and `doctor --redact-all` is the flag behind it (D-84). A
  secret's was never shown." Roadmap, "From the security pass": "Doctor
  `--redact-all`" goes 🟩, "on in the action; `show-values` restores the
  values (D-84)".
* **notes**:

---

## H-5 - A directory others can write to is not read as secrets

* **id**: H-5
* **goal**: a secrets or credentials directory is refused by name when a
  user other than its owner and root can change what it hands over, unless
  the program says `CF_ANY_WRITER`; a drop-in directory is held to the
  same check when the program asks with `CF_TRUSTED_DIR`.
* **why**: a value from `/run/secrets/db.password` explains itself as
  that file. If anyone else could have put the file there, the
  explanation names the wrong author: the configuration says "the
  secrets directory" and means "whoever could write to it".
* **needs**: nothing
* **size**: large
* **decided**:
  * **TO DECIDE - whether a group may write.** The Roadmap's rule is
    "refuse one a user other than its owner or root can write to". On
    Ubuntu and most Linux systems a user's umask is 002 and each user has
    a group of their own (user private groups), so a directory a developer
    makes is mode 0775 and its files 0664: group-writable, by a group
    with nobody else in it. Options:
    1. sudo's rule: refuse a directory, or a file in it, that its group or
       others can write. Strict; every such directory and file made under
       umask 002 is refused, the suite's own included (it writes secrets
       with `fopen`, which cannot set a mode in C89), so the tests would
       need a umask of their own and the card a list of them.
    2. Others only: refuse what every user can write, and never look at
       the group. Nothing made under umask 002 is refused; a directory
       shared by a team's group is not caught.
    3. Group-writable is refused unless the group is the private group
       of the user who owns it - the gid of that user's entry in the
       password database (`getpwuid_r`) - and others' write is refused
       always.
    Recommended: **3**. The answer goes here; until it is written, step 1
    of the loop stops at this card. The rest of the card is written for
    3. If the answer is 2, leave out every group check below and nothing
    else changes. If it is 1: stop, the card needs its list of tests.
  * The rule, D-83, checked by the listing in the same pass that already
    stats every entry (`cf_platform_dir_list`), so that nothing is listed
    once and checked again:
    1. The directory: refused when others may write to it (`S_IWOTH`)
       and it is not sticky (`S_ISVTX`), or when its group may (`S_IWGRP`)
       and the group is not the private group of the user who owns it.
    2. In a sticky directory others may write to - the one kind rule 1
       lets through - each entry as `lstat` sees it (the link, for a
       link) must belong to root, to the directory's owner, or to the
       user the program runs as (`geteuid`); in a sticky directory nobody
       can replace another's entry, so an entry of one of those is theirs.
    3. Each file as `stat` sees it - what a link names - is refused when
       others may write to it, or its group may and the group is not the
       private group of the user who owns it.
    The directories on the way to a link's target are not checked.
    Kubernetes mounts a secret as a sticky, world-writable directory of
    root's (mode 1777) holding root's links to root's files (0644): rule
    2 lets it through and a file another user drops in is caught. Docker's
    `/run/secrets` (0755, files 0444) and systemd's credentials directory
    (the service user's, 0500) pass as they are.
  * The platform, in `include/configforge/cf_platform.h` after
    `cf_platform_dir_list`:

    ```c
    /* Whether this platform can say who may write to a directory and its
       files (POSIX: yes; Windows and a build with no operating system:
       no - an access control list is not a mode, and an answer that
       guessed would be wrong both ways). */
    CF_API cf_bool cf_platform_has_trusted_dir(void);

    /* cf_platform_dir_list, and in the same pass the check of D-83: the
       directory, every entry and every file an entry names must be ones
       nobody but their owner and root can change, and a sticky
       directory's entries must belong to root, to the directory's owner
       or to the program's user. -2 with
       `err` naming the first that is not, as for any entry that cannot be
       handed over (CF_OPTIONAL does not excuse it). Where
       cf_platform_has_trusted_dir is false it is cf_platform_dir_list. */
    CF_API int cf_platform_dir_list_trusted(const char* path,
                                            char (*names)[CF_PLATFORM_NAME_MAX],
                                            int max, cf_error* err);
    ```

  * `src/platform/posix/posix.c`: `cf_platform_dir_list`'s body moves into
    `static int posix_dir_list(const char* path, char (*names)[CF_PLATFORM_NAME_MAX], int max, cf_bool trusted, cf_error* err)`,
    and both public functions call it (`CF_FALSE`, `CF_TRUE`). With
    `trusted`, before the `readdir` loop it `stat`s the directory (fail:
    the existing "cannot read '%s'", -1) and applies rule 1; in the loop,
    rule 2 with `lstat(full, &lst)` when the directory is shared, and rule
    3 on the `st` the loop already has, after its `S_ISREG` test. Helpers:
    `static cf_bool posix_own_group(uid_t uid, gid_t gid)` - `getpwuid_r`
    with a `char buf[1024]` in its own frame (the listing's frame already
    holds a path), true when found and `pw.pw_gid == gid`, false when not
    found (a user with no entry has no group of their own); and
    `#ifndef S_ISVTX` / `#define S_ISVTX 01000` / `#endif` near the
    includes, since `_POSIX_C_SOURCE` alone does not show it (XSI does)
    and the bit is 01000 on every system that has it.
    `cf_platform_has_trusted_dir` returns `CF_TRUE`. Windows (`win32.c`)
    and `none.c`: it returns `CF_FALSE`, and `cf_platform_dir_list_trusted`
    returns `cf_platform_dir_list(path, names, max, err)`.
  * The refusals, `CF_E_SOURCE`, each its text in a `static const char`
    (rule 6); a mode is printed `0%u%u%u` from its three octal digits,
    since `cf_fmt` has no `%o`, and a uid by `%U` with a `(cf_u64)` cast:

    ```c
    static const char posix_writable[] =
        "'%s' can be written by users other than its owner and root (mode "
        "0%u%u%u); a %s read as configuration must not be, or CF_ANY_WRITER "
        "must say it may";
    static const char posix_stranger[] =
        "'%s' belongs to user %U, in a directory others can write to; only "
        "root, the directory's owner and this program's user (%U) may put a "
        "file there, or CF_ANY_WRITER must say anyone may";
    ```

    `posix_writable` with "directory" or "file".
  * The flags, in `include/configforge/cf_source.h`'s enum after
    `CF_FILE_OPTIONAL`:

    ```c
        /* A drop-in directory held to D-83: nobody but its owner and root
           may be able to change what it hands over. A secrets or
           credentials directory is held to it without asking. */
        CF_TRUSTED_DIR = 1u << 17,

        /* A secrets or credentials directory that others may write to,
           read all the same: the program says who may write to it is
           known and accepted (D-83). */
        CF_ANY_WRITER = 1u << 18,

        /* Every flag a directory source knows. */
        CF_DIR_SOURCE_FLAGS = CF_OPTIONAL | CF_TRUSTED_DIR | CF_ANY_WRITER,
    ```

    `CF_SOURCE_FLAGS` stays `CF_OPTIONAL`, so every other source refuses
    the two by the message it has. In `src/core/config.c`,
    `builder_slot_flags` becomes `builder_slot_mask(b, name, flags, allowed)`
    with the mask in the test and in the message's `flags & ~allowed`;
    `builder_slot_flags(b, name, flags)` stays, as
    `return builder_slot_mask(b, name, flags, (unsigned)CF_SOURCE_FLAGS);`.
    `cf_builder_dir`, `cf_builder_secrets` and `cf_builder_credentials`
    call `builder_slot_mask(..., (unsigned)CF_DIR_SOURCE_FLAGS)`.
    `cf_builder_dir` with both `CF_TRUSTED_DIR` and `CF_ANY_WRITER` fails
    the builder, `CF_E_POLICY`, source the name,
    `"'%s': CF_TRUSTED_DIR and CF_ANY_WRITER contradict each other"`.
  * The sources: `src/bridge/secrets.c`'s `secrets_load` lists with
    `cf_platform_dir_list_trusted` unless `CF_ANY_WRITER` (secrets and
    credentials alike). `src/bridge/dir.c`'s `dir_read` lists with it when
    `CF_TRUSTED_DIR`; and there, when `!cf_platform_has_trusted_dir()`,
    it fails first, `CF_E_CAPABILITY`, source the path,
    `"'%s': this platform cannot say who may write to a directory, which CF_TRUSTED_DIR asks"`.
    A secrets directory on Windows is read as before, unchecked: asked
    for nothing, it is not refused for what the platform cannot say, and
    the Guide says so. The watcher's listing is not changed: a reload
    reads through `dir_read` and `secrets_load` again.
  * Test: a new `tests/unit/test_trusted.c`, after `test_dirlimits` in
    `CF_UNIT_TESTS`, in a working directory of its own made by CMake
    (`${CMAKE_CURRENT_BINARY_DIR}/trusted`, with `WORKING_DIRECTORY` set as
    for `test_dirlimits`), and `CF_TEST_POSIX=1` defined for it on
    `UNIX AND NOT CMAKE_SYSTEM_NAME STREQUAL "WASI"` as `CF_TEST_FIFO` is
    for `test_dirlimits`. Under `CF_TEST_POSIX` it defines
    `_POSIX_C_SOURCE 200112L` before any include and includes
    `<sys/stat.h>`, `<sys/types.h>`, `<unistd.h>`, `<pwd.h>`; it calls `umask(022)`
    first, makes its directories with `mkdir(..., 0755)`, its files with
    `fopen` and sets modes with `chmod`. Each case removes what it made.
    Under `CF_WITH_DIR` only. Cases:
    1. `sec` 0755 holding `db.password` 0644: `cf_builder_secrets(b, "s", CF_RANK_ENV, "sec", NULL, 0)`
       builds, the value read.
    2. `sec` 0777: create returns NULL, `CF_E_SOURCE`, the message
       contains "can be written by users other than its owner and root"
       and "0777"; with `CF_ANY_WRITER`, it builds.
    3. `sec` 01777 (sticky) holding the program's own 0644 file: builds.
    4. `sec` 0755 holding `db.password` 0666: refused, the message names
       `sec/db.password` and "0666".
    5. `sec` 0775: builds when the directory's gid is the test user's own
       (`getpwuid(geteuid())->pw_gid == st.st_gid`, else the case is
       skipped and says so).
    6. A drop-in `conf.d` 0777 holding `a.ini` 0644: `cf_builder_dir(..., 0)`
       builds; with `CF_TRUSTED_DIR` it is refused with the directory
       message; with `CF_TRUSTED_DIR | CF_ANY_WRITER` create fails with
       "contradict".
    7. `cf_builder_file(b, "f", CF_RANK_USER_FILE, "x.ini", NULL, CF_TRUSTED_DIR)`:
       create fails, the message contains "are not source flags".
    8. Rule 2, only when the test runs as root (`geteuid() == 0`, else
       skipped and said): a sticky 01777 `sec` holding a file `chown`ed to
       uid 65534: refused, the message contains "belongs to user 65534".
    Without `CF_TEST_POSIX` (Windows): `cf_platform_has_trusted_dir()` is
    false; `cf_builder_dir(..., CF_TRUSTED_DIR)` on an existing directory
    fails with "cannot say who may write"; a secrets directory still reads
    (`tests/unit/test_secretsdir` covers that already; this test asserts
    only the first).
  * The bindings pass flags as numbers. Where a binding lists the source
    flags by name - `git grep -n "OPTIONAL" bindings/rust/src/lib.rs bindings/python/src/configforge include/configforge/configforge.hpp` -
    add `TRUSTED_DIR` (1 << 17) and `ANY_WRITER` (1 << 18) beside it,
    spelled as that list spells `OPTIONAL`; a binding with no such list
    gets nothing.
  * Rejected: a second listing for the check (a file changed between the
    two would be read unchecked). Rejected: checking ownership as sudo
    does, owner root or the program's user: Docker Compose bind-mounts a
    developer's file into a container that runs as root, and it would be
    refused for an owner nobody else can impersonate. Rejected: guessing
    on Windows.
* **files**: `include/configforge/cf_platform.h`,
  `src/platform/posix/posix.c`, `src/platform/win32/win32.c`,
  `src/platform/none/none.c`, `include/configforge/cf_source.h`,
  `src/core/config.c`, `src/bridge/secrets.c`, `src/bridge/dir.c`,
  `tests/unit/test_trusted.c` (new), the bindings' flag lists if any; last:
  `tests/unit/CMakeLists.txt`, `cmake/configforge.symbols`, `docs/API.md`
  (regenerated), `docs/Guide.md`, `docs/Decisions.md`, `docs/Safety.md`,
  `CHANGELOG.md`, `docs/Roadmap.md`.
* **steps**:
  1. The maintainer's answer is written in this card (loop step 4).
  2. The flags, the mask in config.c, the two platform declarations with
     stubs everywhere that call `cf_platform_dir_list` and nothing more,
     the two symbols. Secrets and dir call the trusted listing as decided.
     Build.
  3. Write `test_trusted.c` and its CMake.
  4. See it fail: `ctest --preset default -R unit.test_trusted` fails on
     case 2 (the 0777 directory is read).
  5. The POSIX check as decided. Rerun step 4: passes. Then the whole
     suite, `ctest --preset default`: every secrets, credentials and
     drop-in test still passes - on a machine whose umask is 002 as well
     (`umask` in the shell that runs ctest; if this machine's is 022, run
     once with `umask 002` before the build so the build's directories are
     made 0775).
  6. Windows's and none's capability as decided; ACCEPT-WINDOWS.
  7. Docs. `cmake -P scripts/format.cmake`. ACCEPT-LINUX.
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI (macOS and FreeBSD
  run the POSIX path; musl's `getpwuid_r` reads `/etc/passwd`).
  `gate.abi`, `gate.detection` (no platform macro outside
  `src/platform/`).
* **stop if**: the answer is option 1 (see above); a test of the suite
  that reads a secrets or credentials directory fails in step 5 under
  umask 002 (then option 3 does not hold for it - name the directory,
  its mode and gid under notes); `getpwuid_r` is missing on a CI system
  (the job's log names it); or the listing's frame goes over the stack
  budget.
* **docs**: add D-83 (or the next free), "A directory others can write to
  is not read as secrets": the question; options (nothing; sudo's rule;
  the rule decided; a second pass); the decision (the three rules, the
  answer on groups, sticky directories and Kubernetes, links followed and
  not their directories, Windows unchecked and why, the two flags and
  which source holds to the rule by default); compatibility (a secrets or
  credentials directory others can write to, or holding a file they can,
  is refused where it was read; `CF_ANY_WRITER` reads it as before; a
  drop-in directory is unchanged unless `CF_TRUSTED_DIR`). Guide, the
  secrets section after the `cf_builder_secrets` example: a paragraph on
  the check, its two flags and Windows. Safety.md, "The design": one
  sentence. CHANGELOG, the unreleased section, "### Security": "**A
  secrets directory others can write to is refused** (D-83): a directory,
  or a file in it, that a user other than its owner and root can change
  is an error naming it; Kubernetes' and Docker's mounts and systemd's
  credentials read as before. `CF_ANY_WRITER` accepts one; `CF_TRUSTED_DIR`
  holds a drop-in directory to the same rule. Not checked on Windows." and
  under "### Changed": the compatibility sentence. Roadmap, "From the
  security pass": "A trusted-directory check" goes 🟩, "on for secrets and
  credentials, `CF_TRUSTED_DIR` for drop-ins (D-83)".
* **notes**:

---

## H-7 - An undefined key named like a secret is kept as one

* **id**: H-7
* **goal**: a key nobody defined, whose name is within "did you mean"
  distance of a key defined `CF_SENSITIVE` - `db.pasword` beside
  `db.password` - is shown and fingerprinted as that secret would be,
  everywhere the library and Doctor show one.
* **why**: a mistyped secret's name is the commonest way a secret is
  written where nothing knows it is one, and every output then shows it
  in clear: the blind report, `explain()`, a diff, a malformed
  reference's reason. Its explanation can still say where it came from;
  it need not say what it is.
* **needs**: nothing
* **size**: large
* **decided**:
  * **TO DECIDE - what a report shows of an undefined key that may
    be a secret.** Nothing can know that `db.pasword` is a secret; the
    library can only guess from the contract. Options:
    1. Nothing changes; the gap is written in Safety.md as the one place
       a secret is shown.
    2. A key with no definition whose nearest defined key, at the
       distance "did you mean" uses (`cf_defset_nearest`: a third of the
       key, 1 to 4 edits), is `CF_SENSITIVE` is treated as that secret:
       its value is not shown in a blind report, `explain()`, a diff, a
       reason or Doctor's output, and the fingerprint takes its digest. A
       plain key that happens to be one edit from a secret's name is
       hidden too.
    3. Every undefined key's value is hidden, whenever the contract has
       any sensitive key. Safe without guessing; a typo of a plain key
       loses its value from every report, which is where a person looks
       to see what was mistyped.
    Recommended: **2**. The answer goes here; until it is written, step 1
    of the loop stops at this card. The rest is written for 2; for 1 the
    card is only its Safety.md sentence; for 3, stop: the card needs
    redesign.
  * The mark, decided once, when the contract is applied: in
    `include/configforge/cf_keyspace.h` after `CF_CAND_PINNED`,

    ```c
    /* A candidate of a key nobody defined, whose name is a typo's
       distance from a key defined CF_SENSITIVE (D-85): shown, reasoned
       about and fingerprinted as that secret is. Set by cf_defs_apply. */
    #define CF_CAND_SECRET_LIKE 512u
    ```

    In `src/core/validate.c`'s `cf_defs_apply`, in the branch for an
    entry with no definition (`if (d == NULL) {`, where `++report->unknown`),
    before its loop over candidates: when
    `cfi_defset_nearest_secret(ds, e->key, e->keylen) != NULL`, every
    candidate of `e->chain` gets `CF_CAND_SECRET_LIKE`. In that loop,
    `reference_syntax(..., CF_FALSE, ...)` passes
    `((c->flags & CF_CAND_SECRET_LIKE) != 0) ? CF_TRUE : CF_FALSE`
    instead, so a malformed reference in it is refused with a secret's
    fixed wording.
  * In `src/core/define.c`, the cutoff of `cf_defset_nearest` moves into
    `static size_t nearest_cutoff(size_t keylen)` (a third, 1 to 4), used
    by it and by

    ```c
    /* The CF_SENSITIVE definition nearest `key` within the cutoff
       cf_defset_nearest uses, or NULL. Only the secrets are compared, so
       an undefined key costs as many comparisons as there are secrets,
       not keys. Internal (D-85). */
    const cf_def* cfi_defset_nearest_secret(const cf_defset* ds, const char* key,
                                            size_t keylen);
    ```

    declared in `include/configforge/cf_define.h` beside
    `cf_defset_nearest` without `CF_API`. A key under an open prefix
    (`cf_defset_is_open`) is a plugin's, not a typo, and is never marked.
  * One predicate for every place that decides whether an entry is
    shown as a secret, beside it in `cf_define.h`:

    ```c
    /* Whether `e` is shown as a secret: its definition is CF_SENSITIVE,
       or it has none and is marked CF_CAND_SECRET_LIKE. Internal. */
    cf_bool cfi_entry_secret(const cf_defset* ds, const cf_entry* e);
    ```

    (defined in define.c: a NULL `ds` is false; the mark is read from
    `e->chain`, which every candidate of the entry shares.) Used by:
    `is_secret` in `src/diag/explain.c` (its body becomes the call);
    `src/diag/diff.c` at its two `secret =` lines (by the entry each has;
    where one has only the definition, the entry it came from is in
    scope - read the function); `cf_defset_fingerprint` in define.c (the
    `if (d != NULL && (d->flags & CF_SENSITIVE) != 0)` test); and
    `src/diag/report.c`'s `report_candidate`, whose `redact` becomes
    `(mode == CF_REPORT_BLIND) && (secret)` where `secret` is
    `(d != NULL && (d->flags & CF_SENSITIVE) != 0) || (d == NULL && (c->flags & CF_CAND_SECRET_LIKE) != 0)`.
  * The report says why: in `report_key`, for an entry with no definition
    whose candidates are marked, after `"definition": null`, the added
    field `"like": "<the secret's key>"` (the nearest secret's name - a
    key name, not a value). Schema stays 1: an added field, as
    `"written"` was. Doctor (`tools/doctor/engine.c`, where
    `rk->sensitive = flag_at(...)`) reads it: `rk->sensitive` is also true
    when `keys.<key>.like` is text. Then Doctor's CFD019 recomputes a
    blind report's fingerprint by the digest the report carries, as the
    library computed it, and every Doctor output hides it.
  * Not changed: an unknown key's refusal under `CF_UNKNOWN_REJECT`
    ("unknown key '%s'; did you mean '%s'?") names keys only, and stays.
  * Tests: a new `tests/unit/test_secretlike.c` (after `test_marker` in
    `CF_UNIT_TESTS`). One contract: `db.password` text `CF_SENSITIVE`,
    `db.host` text; a memory source with `db.pasword` = `MARK-LIKE-4417`
    and `db.hots` = `plain-host-value`; `CF_UNKNOWN_IGNORE`. Asserts:
    the blind report (`cf_report` with `CF_REPORT_BLIND`) does not contain
    `MARK-LIKE` and does contain `plain-host-value` and `"like": "db.password"`;
    `cf_explain` of `db.pasword` does not contain `MARK-LIKE`; the
    fingerprint differs from that of the same configuration with
    `db.pasword` = `MARK-LIKE-4418` (the digest changes) and equals it
    for the same value (it is stable); with `db.pasword` = `${MARK` (a
    malformed reference) create fails and the message does not contain
    `MARK`; a key `plugins.db.pasword` under `cf_builder_open(b, "plugins")`
    is shown (`plain` - not marked). And `test_marker.c` gains one
    scenario in its table, the marker under `db.pasword` beside a defined
    secret, so the gate covers it from now on (read how its scenarios are
    written and add one row).
  * Doctor: `tests/doctor/fixtures.c` writes one more blind report,
    `cf_fx_like.json`, of the contract above with `db.pasword` =
    `MARKER-LIKE-7731`; `tests/doctor/secrets.cmake`'s runs gain
    `"lint|cf_fx_like.json|--fail-on|never"`, `"dump|cf_fx_like.json"` and
    `"explain|cf_fx_like.json|db.pasword"`, which its `MATCHES "MARKER"`
    check then covers; and the lint must not report CFD019.
* **files**: `include/configforge/cf_keyspace.h`,
  `include/configforge/cf_define.h`, `src/core/define.c`,
  `src/core/validate.c`, `src/diag/explain.c`, `src/diag/diff.c`,
  `src/diag/report.c`, `tools/doctor/engine.c`,
  `tests/unit/test_secretlike.c` (new), `tests/unit/test_marker.c`,
  `tests/doctor/fixtures.c`, `tests/doctor/secrets.cmake`; last:
  `tests/unit/CMakeLists.txt`, `docs/Decisions.md`, `docs/Safety.md`,
  `docs/Doctor.md`, `CHANGELOG.md`, `docs/Roadmap.md`.
* **steps**:
  1. The maintainer's answer is written in this card (loop step 4).
  2. Write `test_secretlike.c` and register it; the `test_marker` row;
     the fixture and the three Doctor runs.
  3. See it fail: `cmake --build --preset default && ctest --preset default -R "unit\.test_(secretlike|marker)|doctor\.secrets"` -
     all three fail (the marker is shown).
  4. The mark, `cfi_defset_nearest_secret`, `cfi_entry_secret` and its
     five uses, the report's `"like"`, Doctor's reading of it. Rerun step
     3: all pass.
  5. The whole suite, `ctest --preset default`: `unit.test_report`'s
     golden reports do not change (none has a key near a secret's name -
     if one does, read why before changing its golden file).
  6. `cmake -P scripts/format.cmake`, ACCEPT-LINUX, ACCEPT-WINDOWS.
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI; `doctor.marker`
  and `doctor.secrets` pass; `bench/`'s reload of 10,000 keys is not
  slower by more than the noise (an undefined key costs one comparison
  a secret).
* **stop if**: the answer is 3; a golden report changes in step 5 for a
  key nobody would call a secret's typo; or `diff.c` has no entry in
  scope at one of its two sites (then the predicate's argument is wrong
  for it - write which function under notes).
* **docs**: add D-85 (or the next free), "An undefined key named like a
  secret is kept as one": the question; the three options; the decision
  (the answer, the distance, open prefixes excluded, where the mark is
  set and read, the report's "like"); compatibility (a blind report, an
  explanation and a diff of such a key show a digest or nothing where
  they showed its value; a fingerprint of a configuration holding one
  changes once). Safety.md: the sentence on what is never shown gains
  "nor the value of an undefined key a typo away from a secret's name
  (D-85)". Doctor.md, the report fields: `like`. CHANGELOG, the
  unreleased section, "### Security": "**A mistyped secret is kept as a
  secret**: a key nobody defined, a typo's distance from a key defined
  `CF_SENSITIVE` (`db.pasword` beside `db.password`), is shown,
  explained, diffed and fingerprinted as that secret is, and a report
  names the secret it looks like (D-85)." Roadmap, "From the security
  pass": the marker item's "Left: an undefined key that holds a secret"
  sentence is replaced by "An undefined key named like a secret is kept
  as one (D-85)", and the item goes 🟩.
* **notes**:

---

## H-8a - A secret's digest that cannot be tested without a key

* **id**: H-8a
* **goal**: a program that gives its builder a 16-byte key has every
  secret's digest - in a blind report, in a full one, in the fingerprint
  - made by SipHash-2-4 under that key, written
  `sip24:<key id>:<16 hex digits>`; one that gives none has the FNV-1a
  digest it has today.
* **why**: the digest is how a report says "this secret changed" without
  saying what it is. Unsalted FNV-1a over a four-digit PIN can be tested
  against all ten thousand guesses in a millisecond, from a blind report
  or a fingerprint in a log; a keyed digest cannot be tested at all
  without the key, and still compares within one deployment.
* **needs**: nothing
* **size**: large
* **decided**:
  * **TO DECIDE - where the key comes from, and the default.**
    Options:
    1. Only from the program, `cf_builder_digest_key(b, key, 16)`; without
       it, digests are FNV-1a as today. Nothing changes for anyone who
       does nothing; reports keyed alike compare; the brute-force gap
       stays open for a program that gives no key, and the Guide says so.
    2. A key made on first use and kept in a file per host (under a
       state directory the program names). Every digest and fingerprint
       changes on upgrade; two hosts' reports no longer compare; the
       library must write a file it never wrote.
    3. From an environment variable (`CF_DIGEST_KEY`, 32 hex digits) when
       set, otherwise FNV-1a. No code change for a deployment that sets
       it; but the key then sits in the environment the library's own
       documents tell programs to keep secrets out of.
    Recommended: **1** for 1.x, with 2 considered for 2.0, when
    fingerprints may change once. The answer goes here; until it is
    written, step 1 of the loop stops at this card. The rest is written
    for 1; for 3 add only the variable's read in `cf_builder_create`
    (decided: `cf_env_value(cf_platform_environ(), "CF_DIGEST_KEY", &n)`,
    32 hex digits or an error naming the variable); for 2, stop.
  * How old reports stay readable: the field that carries a digest
    (`"digest"`) stays a string and report schema stays 1. An FNV-1a
    digest is `fnv1a:` and 16 hex digits, as now; a keyed one is
    `sip24:` + the key's id (8 hex digits) + `:` + 16 hex digits, 31
    characters. A report from 1.4.0 reads as it did. A Doctor from before
    this card compares two digests as strings: two keyed alike compare
    correctly, and a keyed and an unkeyed one read as "changed", the
    cautious answer. H-8b teaches Doctor to say "cannot be compared"
    instead.
  * The key's id: the top 32 bits of SipHash-2-4 under the key of the 25
    bytes `configforge digest key id`, in 8 lowercase hex digits. It tells
    two keys apart and, being the output of a keyed function, says
    nothing that helps find the key.
  * SipHash-2-4 as its paper defines it (Aumasson and Bernstein, 2012):
    the key as two little-endian 64-bit words, the four state words
    initialised from "somepseudorandomlygeneratedbytes", two rounds a
    message word, the last word carrying the length in its top byte,
    `v2 ^= 0xff` and four rounds to finish, the result `v0 ^ v1 ^ v2 ^ v3`.
    In `src/core/keyspace.c` beside `fnv1a`, C89, every 64-bit constant
    written as two 32-bit halves as that file writes them:

    ```c
    /* SipHash-2-4 of `n` bytes under the 16-byte `key` (D-86). INTERNAL:
       exported for the tests' vectors. */
    CF_API cf_u64 cf_siphash24(const unsigned char* key, const void* p, size_t n);
    ```

    declared in `include/configforge/cf_keyspace.h` after
    `cf_ks_digest_text`. Its test vectors, from the reference
    implementation (key bytes 00 01 ... 0f, the message the bytes 00 01
    ... n-1): the empty message gives `0x726fdb47dd0e0e31`, and the
    15-byte message the paper's Appendix A gives, `0xa129ca6149be45e5`.
  * The API, in `include/configforge/configforge.h` after
    `cf_builder_track_reads`:

    ```c
    /* Digest every sensitive value under this key (D-86): SipHash-2-4,
       written "sip24:<id>:<16 hex digits>", in place of the unkeyed
       FNV-1a digest, wherever a value-blind report, a full one or the
       fingerprint stands a digest for a secret. Without a key a digest
       can be tested against guesses - a PIN in a moment; with one it
       cannot, and reports made under one key still compare. `id` is the
       key's own digest, so two keys are told apart without either being
       shown. `n` must be 16; the bytes are copied and wiped with the
       configuration, and a derived configuration keeps them. */
    CF_API cf_bool cf_builder_digest_key(cf_builder* b, const unsigned char* key,
                                         size_t n);
    ```

    `n != 16` or `key == NULL` fails the builder, `CF_E_POLICY`, no
    source, `"a digest key is 16 bytes, not %zu"` (NULL: 0).
  * Where the key lives: `cf_defset` gains, after `index_cap`,
    `const unsigned char* digest_key; /* 16 bytes in the arena, or NULL: FNV-1a (D-86) */`.
    The builder copies the 16 bytes into the configuration's arena at
    create (wiped with it by `cf_arena_free`) and sets the field; find
    where a derived configuration copies the builder's settings
    (`cf_builder_track_reads`'s flag is one) and copy the key there too.
  * One function makes a secret's digest text, keyed or not, for every
    caller, in keyspace.c:

    ```c
    /* The digest a report and the fingerprint stand for a secret's value:
       cf_ks_digest_text(cf_ks_value_digest(v)) when `key` is NULL, else
       "sip24:<id>:<16 hex digits>" (D-86). Into `buf` as far as `cap`
       allows; the length it needs, at most 31. */
    CF_API size_t cf_ks_secret_digest_text(const cf_value* v, const unsigned char* key,
                                           char* buf, size_t cap);
    ```

    It hashes the same canonical bytes `cf_ks_value_digest` does (read
    that function and share its first lines rather than repeat them).
    Callers: `report_digest` in `src/diag/report.c` (its `text[24]`
    becomes `text[32]`; it gains the defset to read the key from - its
    callers have `ds`) and `cf_defset_fingerprint` in `src/core/define.c`
    (`ds->digest_key`). Doctor's own `cf_ks_value_digest` call
    (engine.c, a full report's secret without a digest) stays: below, a
    full report carries the digest.
  * A full report carries a secret candidate's `"digest"` too, beside its
    `"value"` (an added field in that mode), so Doctor never has to make a
    digest it has no key for; Doctor already prefers the report's digest
    (`if (w->digest != NULL)` in its fingerprint check). Find in
    `report_candidate` where a blind report writes `"digest"` and write it
    for a secret in both modes.
  * The fingerprint keeps its own form, `fnv1a:` and 16 hex digits: it is
    FNV-1a over the selected values, a secret entering by its digest's
    text, as now. Keyed, that text is the keyed digest, so the fingerprint
    differs from an unkeyed one and is the same for every configuration
    of one deployment under one key.
  * Tests: a new `tests/unit/test_digestkey.c` (after `test_marker`):
    the two vectors; one key, one secret `db.password` = `1234`, a blind
    report's digest for it starts `sip24:` and is 31 characters, and is
    the same in a second configuration built the same way; another key
    gives another id and another digest; no key gives `fnv1a:` as before
    (equal to `cf_ks_digest_text(cf_ks_value_digest(&v), ...)` of the
    same value); the fingerprint differs with and without the key and is
    stable under one; a full report holds `"digest": "sip24:` for the
    secret; `cf_builder_digest_key(b, k, 15)` fails create with "a digest
    key is 16 bytes, not 15". `test_marker.c`: its scenarios run once more
    with a key given (read how it builds its configurations, and add the
    key to the builder in a second pass of the same table), and the
    marker must not appear there either.
* **files**: `include/configforge/cf_keyspace.h`, `src/core/keyspace.c`,
  `include/configforge/cf_define.h`, `include/configforge/configforge.h`,
  `src/core/config.c`, `src/core/define.c`, `src/diag/report.c`,
  `tests/unit/test_digestkey.c` (new), `tests/unit/test_marker.c`; last:
  `tests/unit/CMakeLists.txt`, `cmake/configforge.symbols`, `docs/API.md`
  (regenerated), `docs/Guide.md`, `docs/Decisions.md`, `docs/Safety.md`,
  `CHANGELOG.md`, `docs/Roadmap.md`.
* **steps**:
  1. The maintainer's answer is written in this card (loop step 4).
  2. `cf_siphash24` returning 0, and its two vectors in
     `test_digestkey.c`; register the test. See it fail:
     `ctest --preset default -R unit.test_digestkey` fails on the
     vectors. Write SipHash-2-4; the vectors pass.
  3. `cf_builder_digest_key` that stores nothing, the rest of
     `test_digestkey.c`, and the `test_marker` pass. See it fail: the
     digest starts `fnv1a:`.
  4. The defset's key, the copy at create and derive, the shared digest
     function and its two callers, the full report's digest. Rerun
     steps 2 and 3's tests: pass. `ctest --preset default`: every report
     test passes unchanged (no test gives a key, and a full report's
     added `"digest"` is the only change - if a golden full report
     changes, that is the added field; regenerate it as its test says
     and read the diff).
  5. `cmake -P scripts/format.cmake`, ACCEPT-LINUX, ACCEPT-WINDOWS.
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS (`cf_u64` arithmetic under
  MSVC), ACCEPT-CI (tcc and pcc: SipHash in C89 with `cf_u64`; the
  `portable` preset's 64-bit emulation if `cf_u64` is one there).
  `doctor.fingerprint` and CFD019's tests pass: Doctor recomputes a
  keyed report's fingerprint from the digests it carries.
* **stop if**: the answer is option 2; the implementation passes one
  vector and not the other (then the vector written here is wrong: check
  both against the reference implementation's `vectors.h`, write the
  right value here, and go on only if the reference agrees with the
  code); `cf_u64` is not a 64-bit type in some preset (the `portable`
  preset: read `cf_port.h`), so SipHash cannot be written as decided.
* **docs**: add D-86 (or the next free), "A secret's digest, keyed": the
  question (an unsalted FNV-1a digest of a PIN is a lookup); the options
  (the three above, and a slow unkeyed hash, rejected: a PIN is still ten
  thousand tries); the decision (the answer, SipHash-2-4 and why - a
  keyed function made for short inputs, 64 bits as the digest was, small
  in C89; the form of the text; the key's id; the full report's digest;
  the fingerprint); compatibility (without a key nothing changes; with
  one, digests and the fingerprint change once, schema stays 1, an older
  Doctor reads a keyed digest as an opaque string). Guide, section 9 on
  secrets: a paragraph with the call and where a key might be kept.
  Safety.md: the digest sentence says "unkeyed unless the program gives a
  key (D-86)". CHANGELOG, the unreleased section, "### Security":
  "**A secret's digest can be keyed**: `cf_builder_digest_key` makes every
  secret's digest in a report and in the fingerprint SipHash-2-4 under a
  16-byte key, which cannot be tested against guesses without it; without
  one, digests are FNV-1a as before (D-86)." Roadmap, "From the security
  pass": "Keying the secret digest" goes 🟨, "the library keys it; Doctor
  to tell digests of different keys apart (H-8b)".
* **notes**:

---

## H-8b - Doctor tells digests of different keys apart

* **id**: H-8b
* **goal**: `doctor diff`, `history` and `blame` say that two secrets
  cannot be compared when their digests were made under different keys,
  or one under none, instead of saying that the value changed.
* **why**: "changed" is a claim about a value. Between two digests of
  different kinds it is a guess, and a finding that is a guess teaches
  people to ignore the rule.
* **needs**: H-8a
* **size**: medium
* **decided**:
  * A digest's kind is its text up to its last `:` - `fnv1a`, or
    `sip24:<id>`. Two digests compare only when their kinds are equal.
    In `tools/doctor/engine.c`:

    ```c
    /* Whether two digests were made the same way (D-86): their text up
       to the last ':' is the same - "fnv1a", or "sip24:" and the key's
       id. Two that were not cannot say whether a value changed. */
    static cf_bool digests_comparable(const char* x, const char* y);
    ```

  * `values_agree`, where both candidates are told by digest and both
    digests are present: not comparable -> `AGREE_UNKNOWN`. Every rule
    that compares by digest - `rule_value_diff` (CFD014), CFD017's
    defaults, history, blame - then meets `AGREE_UNKNOWN`; each that does
    is changed to report, once a key, a new finding:
    `CFD020`, level info, `"compared by digest, but the digests were made under different keys (%s here, %s there): whether it changed cannot be told"`,
    with the two kinds (an id is not a secret). Find them with
    `git grep -n "values_agree\|by digest" tools/doctor/engine.c`.
  * `docs/Doctor.md`'s table of rules gains CFD020 after CFD019:
    "**Digests not comparable.** Two reports carry a secret's digest made
    under different keys, or one under none (D-86), so whether the secret
    changed cannot be said. Give both programs the same digest key, or
    compare reports from one." | info | yes - copying the columns CFD019's
    row has.
  * Tests: `tests/doctor/fixtures.c` writes three blind reports of one
    contract, `db.password` sensitive: `cf_fx_keyed_a.json` (key A, value
    `one`), `cf_fx_keyed_a2.json` (key A, value `two`), `cf_fx_keyed_b.json`
    (key B, value `one`), with `cf_builder_digest_key`, keys A = bytes 0..15
    and B = bytes 16..31. A new `tests/doctor/keyed.cmake`, added to the
    `foreach(script ...)` list as `keyed`, with `secrets.cmake`'s two
    functions: `diff a a2` reports CFD014 for `db.password`; `diff a b`
    reports CFD020 and not CFD014; `diff cf_fx_secret_a.json a`
    (unkeyed against keyed, if the two share `db.password`; otherwise a
    fourth fixture, unkeyed, of the same contract) reports CFD020; `lint`
    of each keyed report reports no CFD019.
* **files**: `tools/doctor/engine.c`, `tests/doctor/fixtures.c`,
  `tests/doctor/keyed.cmake` (new); last: `tests/doctor/CMakeLists.txt`,
  `docs/Doctor.md`, `CHANGELOG.md`, `docs/Roadmap.md`.
* **steps**:
  1. `git grep -n "cf_builder_digest_key" include/configforge/configforge.h`;
     no match: H-8a is not done; stop.
  2. The fixtures and `keyed.cmake`; register it. See it fail:
     `cmake --build --preset default && ctest --preset default -R "doctor\.(fixtures|keyed)"` -
     `doctor.keyed` fails on `diff a b` (CFD014, not CFD020).
  3. `digests_comparable`, `values_agree`, CFD020 at each site. Rerun:
     passes. `ctest --preset default -R "doctor\."`: all pass.
  4. Docs; ACCEPT-LINUX, ACCEPT-WINDOWS.
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI; `doctor.marker`,
  `doctor.secrets` and `doctor.keyed` pass.
* **stop if**: CFD020 is taken (`git grep -n "CFD020" tools docs`): use
  the next free number and say so in the card; or a rule compares
  digests without `values_agree` in a way the grep does not find (name
  it under notes).
* **docs**: no new D-number (D-86's). D-86 gains a paragraph, "Held to":
  the fixtures and what `doctor.keyed` checks. CHANGELOG, the unreleased
  section's D-86 entry gains: "Doctor says when two reports' digests were
  made under different keys (CFD020) rather than that the secret
  changed." Roadmap, "From the security pass": "Keying the secret digest"
  goes 🟩.
* **notes**:
