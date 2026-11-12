# Wave 3: the native sources left

What is left of Roadmap wave 3, item F, as cards (docs/Plan.md says what
a card is and how one is carried out). Two pieces:

* **W3-F2a to W3-F2d**: macOS's user defaults (`CFPreferences`) as a
  source. Built in the shape the Windows Registry source has
  (`src/bridge/registry.c` over `cf_platform_registry_walk`), with two
  differences: it is an optional part (D-38) and off by default, because it
  links CoreFoundation and an existing macOS program must not get a new link
  requirement in 1.x; and its values go through the shared flattener,
  because a plist is a document (dictionaries and arrays), not a list of
  values.
* **W3-F3**: `cf_builder_credentials` (D-80) everywhere it is still missing.

Order: W3-F3 (independent, small), W3-F2a, W3-F2b, W3-F2c, W3-F2d.
W3-F2a and W3-F2b can be built and accepted on Linux and Windows; only
W3-F2c needs macOS, so only it is **CI-only**.

For the owner: one question, in W3-F2b (which scopes of the user defaults a
source reads). Nothing to install: CI's `macos-latest` runner has
CoreFoundation and the `defaults` tool.

---

## W3-F3 - `cf_builder_credentials` in every language, the Guide and an example

* **id**: W3-F3
* **goal**: systemd's credentials source is offered by the C++ wrapper,
  the Rust crate and the Python package, listed in the Guide's table of
  sources, and shown by a C example the suite runs.
* **why**: a value from a credential says "systemd credential <id>" in
  explain() only if the program can ask for that source; a binding that
  lacks it pushes the program back to `cf_builder_secrets`, whose values
  say "a file in a directory".
* **needs**: nothing
* **model**: sonnet
* **decided**:
  * Other sessions were adding most of this on 2026-11-12. Each item
    below starts with a `grep`: **if it prints a match, the item is done;
    skip it.** Write only what is missing.
  * Names and signatures (copy exactly; they are what D-80 and the C
    header say):
    * C++: `Builder& credentials(const char* name, int rank, const char* prefix = 0, unsigned flags = 0)`
      calling `cf_builder_credentials(b_, name, rank, prefix, flags)`, placed
      right after `secrets(...)` in `include/configforge/configforge.hpp`
      and written exactly as `secrets(...)` is written (its comment: "systemd's
      credentials: the directory $CREDENTIALS_DIRECTORY names, read as
      secrets() reads one (cf_builder_credentials, D-80). A null name is
      \"credentials\".").
    * Rust: `sys.rs` declares `pub fn cf_builder_credentials(b: *mut cf_builder, name: *const c_char, rank: c_int, prefix: *const c_char, flags: c_uint) -> c_int;`
      beside `cf_builder_secrets`; `lib.rs` has
      `pub fn credentials(self, name: Option<&str>, rank: i32, prefix: Option<&str>, flags: u32) -> Self`,
      written as `secrets` is written in the same file (same receiver,
      same string handling, same return).
    * Python: `_native.py` declares it with
      `_declare("cf_builder_credentials", c_int, B, c_char_p, c_int, c_char_p, c_uint, optional=True)`
      and sets `HAVE_CREDENTIALS = hasattr(lib, "cf_builder_credentials")`;
      `_config.py` has `def credentials(self, name: Optional[str], rank: int, prefix: Optional[str] = None, flags: int = 0) -> "Builder"`
      that raises `RuntimeError("configforge: the loaded library has no credentials source (1.4.1); ...")`
      when `not n.HAVE_CREDENTIALS`, as an older shared library lacks the symbol.
  * Tests, one per language, each the same three cases as
    `tests/unit/test_credentials.c`: the variable names a directory with
    `db.password` in it -> the value is read and explain() contains
    `systemd credential db.password`; the variable unset with
    `CF_OPTIONAL` -> builds, key absent; unset without -> fails, the
    message names `CREDENTIALS_DIRECTORY`. Rust and Python set the
    variable in a child process (the library reads the process
    environment), as `tests/python/test_binding.py` and the
    `credentials_tests` module of `bindings/rust/src/lib.rs` already do
    if they exist.
  * The Guide's table in section 4 ("Sources and precedence") gains one
    row, after `cf_builder_secrets`'s:
    `| \`cf_builder_credentials\` | systemd's credentials, the directory \`$CREDENTIALS_DIRECTORY\` names; each value says "systemd credential <id>" |`
  * The example is C only: `examples/c/credentials.c`, C89, in the house
    style of `examples/c/secrets.c` (a header comment that says what it
    shows and that the suite checks its output). It builds with
    `cf_builder_credentials(b, NULL, CF_RANK_ENV, "MYAPP_", CF_OPTIONAL)`,
    defines `db.password` CF_SENSITIVE (via `cf_def_init` and
    `cf_builder_define`, as secrets.c does) and `db.user` with
    `cf_builder_text(b, "db.user", "app", 0)`, and prints exactly:
    `db.user = <value>`, then `a password of <n> characters`, then the
    output of `cf_explain(c, "db.user", ...)`. It never prints the
    password.
  * The build makes the credentials directory, since C89 cannot:
    `examples/c/CMakeLists.txt` writes `credentials-run/creds/db.user`
    holding `reporting\n` and `credentials-run/creds/MYAPP_DB__PASSWORD`
    holding `s3cret-horse\n` with `file(MAKE_DIRECTORY ...)` and
    `file(WRITE ...)`, and sets the test's environment with
    `set_tests_properties(example.credentials PROPERTIES ENVIRONMENT "CREDENTIALS_DIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/credentials-run/creds")`.
    The expectation is run only `if(CF_WITH_DIR)` (the minimal preset has
    no directory sources).
  * Rejected: an example in every language. The Guide shows the call in
    C, Rust and Python already (its "systemd's credentials" section); one
    example the suite runs is enough to keep the output honest.
* **files**: `include/configforge/configforge.hpp`,
  `tests/unit/test_wrapper_access.cpp`, `bindings/rust/src/sys.rs`,
  `bindings/rust/src/lib.rs`, `bindings/python/src/configforge/_native.py`,
  `bindings/python/src/configforge/_config.py`, `tests/python/test_binding.py`,
  `examples/c/credentials.c` (new), `examples/c/CMakeLists.txt`; last:
  `docs/Guide.md`, `CHANGELOG.md`, `docs/Roadmap.md`.
* **steps**:
  1. `git grep -n "cf_builder_credentials" include/configforge/configforge.hpp`.
     No match: add `credentials(...)` as decided. Then
     `git grep -n "credentials(" tests/unit/test_wrapper_access.cpp`; no
     match: add the three cases, modelled on how that file tests
     `secrets(...)`.
  2. `git grep -n "cf_builder_credentials" bindings/rust/src/sys.rs` and
     `git grep -n "pub fn credentials" bindings/rust/src/lib.rs`. For each
     with no match, add what is decided. `git grep -n "mod credentials_tests" bindings/rust/src/lib.rs`;
     no match: add the module with the three cases (child process).
  3. `git grep -n "cf_builder_credentials" bindings/python/src/configforge/_native.py`
     and `git grep -n "def credentials" bindings/python/src/configforge/_config.py`;
     add what is missing. `git grep -n "credentials" tests/python/test_binding.py`;
     no match: add the three cases, skipped `unless native.HAVE_CREDENTIALS`.
  4. `git grep -n "credentials" bindings/rust/README.md bindings/python/README.md`;
     for a README with no match, add one sentence and the call, as the
     other README words it.
  5. `git grep -n "cf_builder_credentials" examples`. No match: write the
     example's expectation first - in `examples/c/CMakeLists.txt` add
     `credentials` to the `foreach(ex ...)` list of examples, the
     directory and files, and
     `example_expect(credentials ${CMAKE_CURRENT_BINARY_DIR}/credentials-run "db.user = reporting" "a password of 12 characters" "systemd credential db.user")`
     inside `if(CF_WITH_DIR)`, plus the ENVIRONMENT property. Write
     `credentials.c` printing only `db.user = app` (no source added yet).
  6. See it fail: `cmake --build --preset default && ctest --preset default -R example.credentials`
     must fail on `db.user = reporting`.
  7. Add the `cf_builder_credentials` call; rerun step 6's ctest: passes.
  8. `git grep -n "cf_builder_credentials\` |" docs/Guide.md`; no match:
     add the table row.
  9. ACCEPT-LINUX, then ACCEPT-WINDOWS (the example sets the variable
     through ctest, which works on Windows too).
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI (cargo and the
  Python package are built there). Also `ctest -R "example.credentials|unit.test_wrapper_access"`
  passes on both.
* **stop if**: `cf_builder_credentials` is no longer in
  `include/configforge/configforge.h` or its signature differs from
  `(cf_builder*, const char* name, int rank, const char* prefix, unsigned flags)`;
  or a binding already has a `credentials` with another signature (do not
  change a released one; hand back).
* **docs**: no D-number (D-80 covers it). CHANGELOG, the unreleased
  section, under its additions: "systemd's credentials from C++
  (`Builder::credentials`), Rust and Python, and an example" - only the
  languages this card added. Roadmap: wave 3's sentence "in C only - the
  C++ wrapper and the Rust and Python bindings do not offer it yet" is
  removed.
* **notes**:

---

## W3-F2a - The optional part and the platform capability for macOS's user defaults

* **id**: W3-F2a
* **goal**: a build option `CF_WITH_PREFERENCES` (off by default) and a
  platform capability `cf_platform_has_preferences` /
  `cf_platform_preferences_walk` exist on every platform, and everywhere
  this card leaves them they fail by name.
* **why**: a program that asks for macOS's user defaults on a build or a
  system that cannot read them must be told so, never handed an empty
  source that looks like "nothing configured" (D-38, and the Registry's
  rule in `cf_platform.h`).
* **needs**: nothing
* **model**: sonnet
* **decided**:
  * Option: `option(CF_WITH_PREFERENCES "macOS user defaults source (links CoreFoundation)" OFF)`
    in `cmake/ConfigForgeOptions.cmake`, after `CF_WITH_SQLITE`, and
    `PREFERENCES` appended to `CF_FEATURES` (after `REPORT`). Off by
    default everywhere, macOS included: on, every static consumer on macOS
    must link `-framework CoreFoundation`, which a 1.x upgrade must not
    impose. The feature's name for `cf_has_feature` is `"preferences"`.
  * `include/configforge/cf_build_options.h` (the source copy): a default
    `#ifndef CF_WITH_PREFERENCES` / `#define CF_WITH_PREFERENCES 0` /
    `#endif`, after `CF_WITH_DOTENV`'s three lines.
    `cmake/templates/cf_build_options.h.in`: `#define CF_WITH_PREFERENCES @CF_OPT_PREFERENCES@`
    after the `CF_WITH_DOTENV` line. `src/core/version.c`: `"preferences",`
    inside `#if CF_WITH_PREFERENCES` in the feature list, after
    `"dotenv"`'s block.
  * In `include/configforge/cf_platform.h`, a new section after "The
    Windows Registry", in its style:

    ```c
    /* macOS's user defaults (CFPreferences): one domain's values as a
       document - dictionaries, arrays and values in order - with each
       value's path for explain(). macOS only, under CF_WITH_PREFERENCES;
       everywhere else the walk fails and says so, because a source that
       read nothing would look exactly like an empty domain. */
    enum {
        CF_PREF_BEGIN_DICT  = 1, /* the domain itself first, name NULL  */
        CF_PREF_BEGIN_ARRAY = 2,
        CF_PREF_END         = 3, /* closes the last dict or array begun */
        CF_PREF_TEXT        = 4, /* a CFString, UTF-8                   */
        CF_PREF_INT         = 5, /* a CFNumber that is not a float type */
        CF_PREF_REAL        = 6, /* a CFNumber of a float type          */
        CF_PREF_BOOL        = 7, /* a CFBoolean                         */
        CF_PREF_OTHER       = 8  /* a CFDate or CFData: `text` says which */
    };

    typedef struct cf_platform_pref_event {
        int         kind;
        const char* name;    /* the member's name, its case kept; NULL in an
                                array and for the domain                  */
        size_t      namelen;
        const char* path;    /* names from the domain down joined by '/', an
                                array's member by its index: "Server/Port",
                                "Hosts/1"; "" for the domain              */
        const char* text;    /* CF_PREF_TEXT; CF_PREF_OTHER: "a date", "data" */
        size_t      textlen;
        cf_i64      i;       /* CF_PREF_INT                               */
        double      r;       /* CF_PREF_REAL                              */
        cf_bool     b;       /* CF_PREF_BOOL                              */
    } cf_platform_pref_event;

    /* Return false to stop the walk; the walk then returns -1, and the
       callback is expected to have said why. */
    typedef cf_bool (*cf_platform_pref_fn)(void* ctx, const cf_platform_pref_event* e);

    CF_API cf_bool cf_platform_has_preferences(void);

    /* Every value of `domain` ("com.example.app") in the scope W3-F2b's
       answer names. Buffers come from `arena`. 0 when walked, 1 when the
       domain has no values, -1 with `err` set. */
    CF_API int cf_platform_preferences_walk(const char* domain, cf_arena* arena,
                                            cf_platform_pref_fn fn, void* ctx,
                                            cf_error* err);
    ```

  * Stubs, in `src/platform/posix/posix.c` (after the Registry's two
    functions), `src/platform/win32/win32.c` (after
    `cf_platform_registry_walk`) and `src/platform/none/none.c` (after its
    Registry stubs), each written as that file's Registry stub is:
    `cf_platform_has_preferences` returns `CF_FALSE`;
    `cf_platform_preferences_walk` casts away `arena`, `fn`, `ctx`, sets
    `CF_E_CAPABILITY` with source `domain`, and returns -1. The message,
    chosen by the option so that D-38's "names the option" holds:

    ```c
    #if CF_WITH_PREFERENCES
        "'%s' is in macOS's user defaults, which exist only on macOS"
    #else
        "'%s' is in macOS's user defaults, and this build was compiled "
        "without CF_WITH_PREFERENCES"
    #endif
    ```

    (name the string in a `static const char` first: no literal continued
    over lines as a call's argument, Agents.md rule 6). `(root != NULL) ? root : ""`
    as the Registry stub guards it, with `domain`.
  * posix.c's stub is the whole answer in this card on every POSIX system,
    macOS included. W3-F2c puts the real walk under
    `#if defined(__APPLE__) && CF_WITH_PREFERENCES` and leaves this stub
    for the rest. Until W3-F2c, a macOS build with the option on says
    "exist only on macOS", which is wrong there; nothing builds that
    combination before W3-F2c (CI's macOS jobs leave the option off).
  * `cmake/configforge.symbols`: `cf_platform_has_preferences` after
    `cf_platform_has_environ`... in sorted order (the file is sorted:
    put each name where `sort` would), and `cf_platform_preferences_walk`.
  * Test: a new `tests/unit/test_preferences.c`, added to the list of
    unit tests in `tests/unit/CMakeLists.txt` after `test_registry`. In
    this card it has `test_elsewhere_platform()` only, run when
    `!cf_platform_has_preferences()`; `main` is shaped like
    `tests/unit/test_registry.c`'s. The walk is called on
    `"org.configforge.test"` with a fixed arena
    (`static char buf[4096]; cf_arena_fixed(buf, sizeof buf)`) and a
    callback that counts its calls. Asserts: it returns -1; `err.code ==
    CF_E_CAPABILITY`; the message contains `org.configforge.test`; it
    contains `"exist only on macOS"` when `CF_WITH_PREFERENCES` and
    `"compiled without CF_WITH_PREFERENCES"` when not; the callback was
    called 0 times.
  * `tests/unit/test_features.c`: one line beside the dotenv one,
    `check(cf_has_feature("preferences") == (CF_WITH_PREFERENCES ? CF_TRUE : CF_FALSE), ...)`.
  * Rejected: the capability without an option (as the Registry has it).
    The Registry needs only advapi32, which every Windows link has; this
    needs CoreFoundation, which no macOS link has by default.
  * Rejected: reading the plist files under `~/Library/Preferences`
    directly. `cfprefsd` caches and writes them late, they are often
    binary plists, and managed profiles live elsewhere: the file is not
    the setting.
* **files**: `cmake/ConfigForgeOptions.cmake`,
  `include/configforge/cf_build_options.h`,
  `cmake/templates/cf_build_options.h.in`, `src/core/version.c`,
  `include/configforge/cf_platform.h`, `src/platform/posix/posix.c`,
  `src/platform/win32/win32.c`, `src/platform/none/none.c`,
  `tests/unit/test_preferences.c` (new), `tests/unit/test_features.c`;
  last: `tests/unit/CMakeLists.txt`, `cmake/configforge.symbols`,
  `docs/API.md` (regenerated), `CHANGELOG.md`, `docs/Roadmap.md`.
* **steps**:
  1. Add the option, both copies of `cf_build_options.h`, the feature
     string, and the `test_features.c` line. Build (`cmake --preset default && cmake --build --preset default`).
  2. Add the header section, and stubs in all three platform files that
     return 0 and set nothing (a placeholder, to see the test fail).
     Add both names to `cmake/configforge.symbols`.
  3. Write `tests/unit/test_preferences.c` as decided and register it.
  4. See it fail: `ctest --preset default -R unit.test_preferences` fails
     on "returns -1".
  5. Write the stubs as decided. Rerun step 4: passes.
  6. The other branch: `cmake -S . -B build-prefs -DCF_WITH_PREFERENCES=ON && cmake --build build-prefs && ctest --test-dir build-prefs -R "unit.test_(preferences|features)"`
     passes (on Linux it now expects "exist only on macOS"). Remove
     `build-prefs`.
  7. `cmake -P scripts/format.cmake` (it regenerates `docs/API.md`), then
     ACCEPT-LINUX and ACCEPT-WINDOWS.
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI. `gate.abi` passes
  (the symbol list matches), `gate.detection` passes (no platform macro
  outside `src/platform/`).
* **stop if**: `gate.headers` refuses `double` in `cf_platform.h` under a
  build without `CF_WITH_REAL` (then the field is wrong for this header;
  hand back); or `CF_FEATURES` is read somewhere that assumes every
  feature is on by default (a gate or the SBOM failing on the new name).
* **docs**: no D-number yet (W3-F2b writes the decision). CHANGELOG,
  unreleased section: nothing user-visible yet but the option; one line,
  "`CF_WITH_PREFERENCES` (off): the build option for macOS's user
  defaults, which W3-F2c reads". Roadmap: the "Native OS sources" row
  stays 🟨, its text "macOS's user defaults: the option and the platform
  stubs; the source to come".
* **notes**:

---

## W3-F2b - `cf_builder_preferences`: the source, off macOS

* **id**: W3-F2b
* **goal**: `cf_builder_preferences(b, name, rank, domain, flags)`
  exists, reads a domain through the platform walk into the shared
  flattener, and on every system but macOS fails by name, `CF_OPTIONAL`
  or not.
* **why**: a value set with `defaults write com.example.app Port -int 9443`
  should explain itself as `defaults com.example.app Port`, the place a
  person would look, instead of the program copying it into a file or the
  environment where its origin is lost.
* **needs**: W3-F2a
* **model**: sonnet
* **decided**:
  * **ASK THE OWNER - which scope a source reads.** CFPreferences keeps a
    domain's values in up to four places: the current user on any host
    (what `defaults write <domain>` writes and `defaults read <domain>`
    reads), the current user on this host only (`defaults -currentHost`),
    any user (`/Library/Preferences`, written by an administrator), and
    a managed profile (MDM, forced). Options:
    1. The current user, any host, only. Each value has exactly one place,
       `defaults <domain> <path>`; the other scopes are not read, and the
       header comment and the Guide say so.
    2. The merged view a program gets from `CFPreferencesCopyAppValue`,
       with each value's scope in its place and forced (managed) values
       marked. More complete; but which scope won is CFPreferences'
       precedence, not this library's ranks, and a key set only by a
       managed profile cannot be listed through the public API, so the
       merged view still misses some.
    3. Option 1 now; later a scope prefix in `domain`, as the Registry's
       root names its hive (`"currentHost:com.example.app"`,
       `"anyUser:com.example.app"`, `"managed:..."`), each scope its own
       source at its own rank, so precedence between them is the
       program's and explain() shows it.
    Recommended: **3** (option 1 built now). The answer goes here; until
    it is written, step 1 of the loop stops at this card.
    The rest of this card is written for options 1 and 3. If the answer is
    2, the card needs redesign: stop.
  * Signature, in `include/configforge/configforge.h` after
    `cf_builder_registry`:

    ```c
    /* macOS's user defaults (CFPreferences) for one domain, as `defaults
       read <domain>` shows them: the current user's, on any host. A
       dictionary is a section and an array a list, as in a JSON file:
       Server = { Port = 9443; } is server.port. Names are lowered and a
       name no key can carry is an error naming it; a string is text the
       contract types (`defaults write` writes strings unless told -int),
       a number is an int or a real, a boolean a bool, and a date or data
       is refused by name. Each value's place in explain() and a report is
       "defaults <domain> <path>", Server/Port. `name` is the source's
       (NULL: the domain). A domain with no values is an error, and nothing
       with CF_OPTIONAL. macOS only, under CF_WITH_PREFERENCES (off by
       default: it links CoreFoundation); elsewhere it fails by name,
       CF_OPTIONAL or not. Not watched: cf_changed does not see a change to
       it, as it does not for the Registry. */
    CF_API cf_bool cf_builder_preferences(cf_builder* b, const char* name, int rank,
                                          const char* domain, unsigned flags);
    ```

  * The source is internal, as credentials' is: in
    `include/configforge/cf_source.h`, after the Registry's block,

    ```c
    /* macOS's user defaults (cf_builder_preferences). Internal: the builder
       function is the interface. */
    size_t          cfi_source_preferences_state_size(void);
    cf_bool         cfi_source_preferences(cf_source* s, const char* name, int rank,
                                           const char* domain, unsigned flags,
                                           void* state, size_t state_size);
    const cf_error* cfi_source_preferences_error(const cf_source* s);
    ```

    Not `CF_API`, so nothing is added to the symbol list but
    `cf_builder_preferences`.
  * `src/bridge/preferences.c` (new), C89, laid out as
    `src/bridge/registry.c`: a header comment saying what it decides
    (the paragraph above, as registry.c's comment is written), then:
    * `typedef struct prefs_state { const char* domain; unsigned flags; cf_error err; cf_bool failed; } prefs_state;`
    * `typedef struct prefs_walk { const cf_source* s; prefs_state* st; cf_flatten fl; char name[CF_KEY_MAX]; char at[CF_PATH_MAX + CF_KEY_MAX]; } prefs_walk;`
      allocated from the key space's arena with `cf_arena_calloc`, as
      registry.c allocates `registry_walk` (a `cf_flatten` and two key
      buffers are far over the stack budget).
    * `prefs_load(const cf_source* s, cf_keyspace* ks)`: clears the
      state as `registry_load` does; `cf_flatten_init(&w->fl, ks, s->rank)`;
      calls `cf_platform_preferences_walk(st->domain, cf_ks_arena(ks), prefs_event, w, &st->err)`.
      rc 1: with `CF_OPTIONAL` return true; without, error `CF_E_SOURCE`,
      source `domain`, `"there are no user defaults for '%s'"`. rc -1:
      failed. rc 0 and `w->fl.failed || w->fl.overflow`: error
      `CF_E_KEY`, `"'%s' holds a key this build cannot hold"` with the
      domain (file.c's wording for a file).
    * `prefs_event(void* ctx, const cf_platform_pref_event* e)`: drives
      `cf_flatten_sink(&w->fl)`. The name, when not NULL, is lowered
      into `w->name` with `cf_to_lower` byte by byte (as `registry_key`
      lowers); if it does not fit, or `!cf_key_valid(w->name, n)`, error
      `CF_E_KEY`, source the place, with registry.c's wording:
      `"%s: '%s' is not a valid key; name it in lowercase letters, digits, '_' and '-'"`.
      The place: `cf_fmt(w->at, sizeof w->at, "defaults %s %s", domain, e->path)`
      (for the domain itself, `"defaults %s"`). Then by kind:
      `BEGIN_DICT` -> `begin_table(ctx, name, n)` (NULL, 0 for the
      domain); `BEGIN_ARRAY` -> `begin_array`; `END` -> `end`; `TEXT` ->
      `cf_value_text` and `scalar(..., textual = CF_TRUE, &loc)`; `INT`
      -> `cf_value_int`, textual false; `BOOL` -> `cf_value_bool`,
      textual false; `REAL` -> under `#if CF_WITH_REAL` `cf_value_real`,
      textual false, else error `CF_E_CONVERT`
      `"%s: this build has no floating point support"` (json.c's words);
      `OTHER` -> error `CF_E_CONVERT`,
      `"%s: only strings, numbers, booleans, arrays and dictionaries are read; this value is %s"`
      with the place and `e->text`. `loc` is `cf_loc { w->at, 0, 0 }`:
      line 0 makes `cf_loc_text` print the place alone. A sink call that
      returns false and has set no error: error `CF_E_KEY`,
      `"%s: a key this build cannot hold"`.
    * The vtable `{ sizeof(cf_source_vtable), "preferences", prefs_load, prefs_destroy, NULL, NULL }`,
      the init and the error accessor exactly as registry.c's three
      functions, with `s->name = (name != NULL) ? name : domain`.
    * The place is cut at `CF_KEY_MAX` by the flattener's buffer
      (`fl_scalar`'s `where`); a domain and path longer than that are
      shown cut. Said in the header comment; not worked round.
  * `src/core/config.c`: `cf_builder_preferences` written as
    `cf_builder_registry` is (`builder_slot_flags`, state from the arena,
    error "no room for a user defaults source", `builder_dup` of name
    and domain); and in the create-time error lookup where
    `cf_source_registry_error` is asked (around "detail ="), ask
    `cfi_source_preferences_error` next.
  * `src/bridge/CMakeLists.txt` and `scripts/generate/amalgamate.py`:
    `preferences.c` after `registry.c` in each list.
  * Test, extending `tests/unit/test_preferences.c`: a `load(domain, flags, &err)`
    helper as in test_registry.c (`cf_builder_preferences(b, "defaults", CF_RANK_SYS_FILE, domain, flags)`
    plus `cf_builder_int(b, "server.port", 8080, 1, 65535, 0)`), and
    `test_elsewhere_source()`: without and with `CF_OPTIONAL`, `load`
    returns NULL; the message contains the domain and, as in W3-F2a's
    test, "exist only on macOS" or "compiled without
    CF_WITH_PREFERENCES". Run when `!cf_platform_has_preferences()`.
* **files**: `src/bridge/preferences.c` (new), `src/bridge/CMakeLists.txt`,
  `include/configforge/cf_source.h`, `include/configforge/configforge.h`,
  `src/core/config.c`, `scripts/generate/amalgamate.py`,
  `tests/unit/test_preferences.c`; last: `cmake/configforge.symbols`,
  `docs/API.md` (regenerated), `docs/Decisions.md`, `CHANGELOG.md`,
  `docs/Roadmap.md`.
* **steps**:
  1. The owner's answer is written in this card (loop step 4).
  2. Add the declaration to `configforge.h`, `cf_builder_preferences` to
     the symbol list, and in config.c a `cf_builder_preferences` that
     returns `CF_TRUE` and adds nothing.
  3. Write `test_elsewhere_source()`.
  4. See it fail: `ctest --preset default -R unit.test_preferences`
     fails on "returns NULL".
  5. Write `preferences.c`, the internal declarations, the real builder
     function, the error lookup, the two lists. Rerun: passes.
  6. With the option on, as W3-F2a step 6: `cmake -S . -B build-prefs -DCF_WITH_PREFERENCES=ON`,
     build, `ctest --test-dir build-prefs -R unit.test_preferences`; passes;
     remove `build-prefs`.
  7. `gate.amalgamation` is in ACCEPT-LINUX: it fails if `preferences.c`
     is missing from `amalgamate.py`.
  8. Write the decision (below). `cmake -P scripts/format.cmake`; ACCEPT-LINUX;
     ACCEPT-WINDOWS.
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI. The walk's
  callback is not run anywhere yet; W3-F2c is what runs it.
* **stop if**: the owner answers option 2; or `cf_flatten`'s sink refuses
  a table opened with name NULL at depth -1 (it must accept it: that is
  how every codec opens its document - check `fl_begin_table` in
  `src/bridge/flatten.c` before writing, and stop if it is not so).
* **docs**: add D-<next free> (D-82 when this was written; check with
  `git grep -n "^## D-" docs/Decisions.md`), "macOS's user defaults, an
  optional source over the flattener", in the shape of D-80: the
  question; options (no source; the Registry's shape without the
  flattener; the shape chosen; reading the plist files); the decision
  (option off by default and why, the scope as answered, names lowered,
  types, the place's wording, not watched); compatibility (additive, one
  symbol; nothing changes unless the option is on). CHANGELOG, unreleased
  section: `cf_builder_preferences` in a sentence, "macOS only, with
  CF_WITH_PREFERENCES". Roadmap: the row stays 🟨, "the source written;
  read on macOS by W3-F2c".
* **notes**:

---

## W3-F2c - Reading CFPreferences on macOS (CI-only)

* **id**: W3-F2c
* **goal**: on macOS with `CF_WITH_PREFERENCES=ON`, the walk reads a
  domain through CoreFoundation, and CI builds and tests that on every
  push.
* **why**: until this card the source refuses everywhere; this is the
  part that makes a macOS value explain itself.
* **needs**: W3-F2b
* **model**: opus
* **decided**:
  * **CI-only.** No machine here is a Mac. Each step that changes C is
    pushed on its own (`git push origin work/W3-F2c`) and the macOS job's
    log is read (`gh run view --log-failed`); expect several pushes.
    Keep each push small so a red run names one change. Linux and
    Windows still run ACCEPT-LINUX and ACCEPT-WINDOWS locally before the
    first push: nothing here may change them.
  * Where: in `src/platform/posix/posix.c`, the W3-F2a stub is put under
    `#if !(defined(__APPLE__) && CF_WITH_PREFERENCES)` and the real
    functions under `#if defined(__APPLE__) && CF_WITH_PREFERENCES`,
    with `#include <CoreFoundation/CoreFoundation.h>` inside that guard.
    One file, so that `amalgamate.py`, the build and `build.rs` (which
    compiles every `.c`) need no new entry.
  * Language level: Apple's headers are not C89 (`long long` through
    `SInt64`, at least). With the option on and `APPLE`, the root
    `CMakeLists.txt` compiles `posix.c` as C99, copying the block that
    compiles `yaml.c` as C11 (`set_source_files_properties(... PROPERTIES COMPILE_OPTIONS "-std=c99")`),
    and links the framework: `target_link_libraries(configforge PUBLIC "-framework CoreFoundation")`
    beside the `CF_WITH_SQLITE` link (PUBLIC: a static library's
    consumer needs it). The rest of the library stays C89, and on Linux
    and Windows nothing changes.
  * The walk, using only these calls:
    `CFStringCreateWithCString(kCFAllocatorDefault, domain, kCFStringEncodingUTF8)`;
    `CFPreferencesAppSynchronize(app)` first, so values another process
    wrote are seen; `CFPreferencesCopyKeyList(app, kCFPreferencesCurrentUser, kCFPreferencesAnyHost)`
    (NULL or an empty array: return 1, "no values");
    `CFPreferencesCopyMultiple(keys, app, kCFPreferencesCurrentUser, kCFPreferencesAnyHost)`.
    Every Copy or Create is released with `CFRelease` on every path,
    error paths included.
  * Order: a dictionary's keys are converted to UTF-8 and walked in byte
    order (an insertion sort over the arena copies; dictionaries in
    configuration are small), so the events, and the keys a test sees,
    do not depend on CFDictionary's order.
  * Types, by `CFGetTypeID(v)` against `CFStringGetTypeID()`,
    `CFNumberGetTypeID()`, `CFBooleanGetTypeID()`, `CFArrayGetTypeID()`,
    `CFDictionaryGetTypeID()`, `CFDateGetTypeID()`, `CFDataGetTypeID()`:
    a string -> `CF_PREF_TEXT` (`CFStringGetLength`,
    `CFStringGetMaximumSizeForEncoding(len, kCFStringEncodingUTF8) + 1`
    bytes from the arena, `CFStringGetCString`); a number ->
    `CFNumberIsFloatType` ? `CF_PREF_REAL` with
    `CFNumberGetValue(n, kCFNumberDoubleType, &d)` : `CF_PREF_INT` with
    `CFNumberGetValue(n, kCFNumberSInt64Type, &i)` into a `cf_i64`; a
    boolean -> `CF_PREF_BOOL` (`CFBooleanGetValue`); an array -> BEGIN_ARRAY,
    its members (name NULL, path `<parent>/<index>`), END; a dictionary
    -> BEGIN_DICT, its members, END; a date -> `CF_PREF_OTHER`, text
    "a date"; data -> `CF_PREF_OTHER`, text "data"; any other type ->
    `CF_PREF_OTHER`, text "of another type". A dictionary key that is
    not a CFString: error `CF_E_KEY`,
    `"defaults %s %s: a key that is not a string"`.
  * The domain is BEGIN_DICT (name NULL, path "") before its members and
    END after them.
  * Depth: past `CF_MAX_DEPTH` levels, error `CF_E_SOURCE`,
    `"defaults %s: nests deeper than %d levels"`, before recursing. The
    recursion's frame holds no buffer (paths and names live in the arena),
    so the 2048-byte budget holds; the minimal preset is not built on
    macOS, so this is the rule, not a check.
  * `cf_platform_has_preferences` returns `CF_TRUE` under this guard.
  * The gates that compile by hand learn the framework:
    `cmake/ConfigForgeGates.cmake` passes `-DPREFS_LIB=-framework;CoreFoundation`
    when `APPLE AND CF_WITH_PREFERENCES` (into `options` for
    `gate.amalgamation`, and to `gate.install`); in
    `cmake/gates/GateAmalgamation.cmake`, after the `foreach(dep YAML XML SQLITE)`
    loop: `if(O_PREFERENCES AND DEFINED PREFS_LIB) list(APPEND DEP_LIB ${PREFS_LIB})`
    and `set(amalg_std -std=c99)` unless it is already `-std=c11`;
    `cmake/gates/GateInstall.cmake` appends `PREFS_LIB` where it appends
    `SQLITE_LIB` (read that file to find the line). `cmake/ConfigForgeInstall.cmake`:
    `string(APPEND CF_PC_LIBS_PRIVATE " -framework CoreFoundation")` when
    the option is on and `APPLE`, beside the sqlite3 line.
  * CI: a job in `.github/workflows/ci.yml`, after `shared`:

    ```yaml
      # macOS's user defaults (W3-F2c): the one source only a Mac can read,
      # built with its option and tested against values `defaults import`
      # writes.
      macos-preferences:
        runs-on: macos-latest
        steps:
          - uses: actions/checkout@3d3c42e5aac5ba805825da76410c181273ba90b1 # v7
          - run: cmake -S . -B bp -DCF_WITH_PREFERENCES=ON
          - run: cmake --build bp
          - run: ctest --test-dir bp --output-on-failure
    ```

    The whole suite, gates included, so a gate that cannot link the
    framework fails here.
  * The test, `test_on_macos()` in `tests/unit/test_preferences.c`, run
    when `cf_platform_has_preferences()`. It writes a plist file with
    `fopen` and imports it with `system("defaults import <domain> <file>")`,
    a `prefs(const char* args)` helper written as test_registry.c's
    `reg()` is (under `#if defined(__APPLE__)`, `(void)args` elsewhere).
    It deletes each domain first and last
    (`defaults delete <domain>`, its failure ignored: absent is fine).
    Domain `org.configforge.test`, file `cf_prefs.plist`:

    ```xml
    <?xml version="1.0" encoding="UTF-8"?>
    <!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
    <plist version="1.0"><dict>
    <key>Server</key><dict><key>Port</key><integer>9443</integer><key>Host</key><string>web1</string><key>Workers</key><string>12</string></dict>
    <key>Hosts</key><array><string>a</string><string>b</string></array>
    <key>Ratio</key><real>0.5</real>
    <key>Enabled</key><true/>
    <key>Limit</key><integer>5000000000</integer>
    </dict></plist>
    ```

    (written by `fputs` of named string pieces, each under 509
    characters). Defined: `server.port` int 8080 1..65535,
    `server.workers` int 1 1..64. Asserts: `server.port` 9443 ("a
    dictionary is a section, an integer an int"); `server.workers` 12
    ("a string is text the contract types"); `server.host` "web1";
    `hosts.count` 2 and `hosts.1` "b"; `enabled` true (`cf_get_bool`);
    `limit` 5000000000 (written `(cf_i64)5 * 1000000000`); under
    `CF_WITH_REAL`, `ratio` 0.5; explain of `server.port` contains
    `defaults org.configforge.test Server/Port`. Refusals, each its own
    domain and file: `org.configforge.test.date` with
    `<key>When</key><date>2026-01-01T00:00:00Z</date>` -> NULL, message
    has "is a date" and "When"; `org.configforge.test.name` with
    `<key>Bad Name</key><string>x</string>` -> NULL, `CF_E_KEY`, message
    has "Bad Name"; `org.configforge.test.absent` (never written) -> NULL
    with "no user defaults", and not NULL with `CF_OPTIONAL`.
  * Unverified by reading, and so stop conditions rather than guesses:
    that `defaults import <domain> <file>` replaces the domain with the
    file's contents on the runner's macOS; that `CFPreferencesCopyKeyList`
    sees an import at once after `CFPreferencesAppSynchronize`.
* **files**: `src/platform/posix/posix.c`, `CMakeLists.txt`,
  `cmake/ConfigForgeGates.cmake`, `cmake/gates/GateAmalgamation.cmake`,
  `cmake/gates/GateInstall.cmake`, `cmake/ConfigForgeInstall.cmake`,
  `.github/workflows/ci.yml`, `tests/unit/test_preferences.c`; last:
  `CHANGELOG.md`, `docs/Roadmap.md`.
* **steps**:
  1. Add the CI job and `test_on_macos()` with the plist and its asserts;
     the walk is still the stub, so `cf_platform_has_preferences()` is
     false and the test takes `test_elsewhere_*`. ACCEPT-LINUX and
     ACCEPT-WINDOWS locally. Push. The new job must be green (it runs the
     stub's branch: "exist only on macOS"). This proves the job and the
     link before any CoreFoundation code.
  2. Make `cf_platform_has_preferences()` return `CF_TRUE` under the
     guard and the walk return -1 with "not written yet". Push. See it
     fail: the job fails in `test_on_macos` ("a domain is a source").
     That is the test seen failing.
  3. The CMake link and C99 lines; push; the job still fails only in
     the test (not in the build). If the build fails on a CoreFoundation
     header, read the log; the fix is the language level, nothing else.
  4. The walk. Push. Read the job's log; repeat until green.
  5. The gates' framework lines (amalgamation, install, pkg-config). They
     run in the same job (`ctest` runs the gates). Push until green.
  6. ACCEPT-CI on the branch: every job green, macOS's two old jobs
     included (they build with the option off and must not change).
* **accept**: ACCEPT-CI. Locally, ACCEPT-LINUX and ACCEPT-WINDOWS before
  the first push and before the merge.
* **stop if**: `defaults import` does not replace or does not exist on the
  runner (then write the values with `defaults write <domain> <key> -int ...`
  per key, which cannot nest dictionaries: the test's design changes,
  hand back); the values imported are not seen by the walk after
  `CFPreferencesAppSynchronize`; the framework cannot be linked into the
  amalgamation gate without changing its comparison; three pushes fail
  on the same CoreFoundation call.
* **docs**: the D-number of W3-F2b gains one paragraph, "Held to", in
  the shape D-79's has: what the macOS job writes and checks. CHANGELOG:
  the unreleased section's `cf_builder_preferences` sentence says it is
  read on macOS and tested in CI. Roadmap: the "Native OS sources" row
  goes 🟩 once W3-F2d is done too; until then its text says "read on
  macOS; the wrappers left".
* **notes**:

---

## W3-F2d - User defaults in C++, Rust, Python and the Guide

* **id**: W3-F2d
* **goal**: `preferences(...)` in the C++ wrapper and in the Rust and
  Python bindings, each failing by name off macOS, and the Guide says how
  to read a domain.
* **why**: as for W3-F3: a binding without the source makes a program
  copy the value elsewhere and lose where it came from.
* **needs**: W3-F2c
* **model**: sonnet
* **decided**:
  * C++, after `registry(...)` in `configforge.hpp`, written as it is:
    `Builder& preferences(const char* name, int rank, const char* domain, unsigned flags = 0)`
    calling `cf_builder_preferences`; comment "macOS's user defaults
    (cf_builder_preferences): macOS only, with CF_WITH_PREFERENCES."
  * Rust: `sys.rs` declares `cf_builder_preferences` as
    `cf_builder_credentials` is declared; `lib.rs`
    `pub fn preferences(self, name: Option<&str>, rank: i32, domain: &str, flags: u32) -> Self`,
    written as `credentials` is. `build.rs` changes nothing: the crate
    compiles with `cf_build_options.h`'s defaults, where the option is
    off, so the method fails by name and nothing links CoreFoundation.
    Said in the method's doc comment.
  * Python: `_declare("cf_builder_preferences", c_int, B, c_char_p, c_int, c_char_p, c_uint, optional=True)`,
    `HAVE_PREFERENCES`, and `def preferences(self, name, rank, domain, flags=0)`
    raising `RuntimeError("configforge: the loaded library has no user defaults source; ...")`
    when the symbol is missing, as `credentials` does.
  * Tests: in each language, off macOS (which is every machine and CI job
    but `macos-preferences`), building with the source fails and the
    message contains "macOS's user defaults". C++ in
    `tests/unit/test_wrapper_access.cpp`; Rust a `#[test]` in `lib.rs`;
    Python in `tests/python/test_binding.py`, skipped unless
    `native.HAVE_PREFERENCES`.
  * Guide: a row in section 4's table after `cf_builder_registry`'s,
    `| \`cf_builder_preferences\` | macOS's user defaults for a domain (\`CF_WITH_PREFERENCES\`): \`Server/Port\` is \`server.port\` |`,
    and a subsection after "### systemd's credentials", "### macOS's user
    defaults", of the same length and shape: the call, `defaults write`
    and what explain() says, the scope read, the option.
* **files**: `include/configforge/configforge.hpp`,
  `tests/unit/test_wrapper_access.cpp`, `bindings/rust/src/sys.rs`,
  `bindings/rust/src/lib.rs`, `bindings/python/src/configforge/_native.py`,
  `bindings/python/src/configforge/_config.py`,
  `tests/python/test_binding.py`; last: `docs/Guide.md`, `CHANGELOG.md`,
  `docs/Roadmap.md`.
* **steps**:
  1. The three tests, each calling a `preferences` that does not exist
     yet. See each fail (the C++ and Rust ones do not compile; the
     Python one fails with AttributeError).
  2. The three methods. Each test passes.
  3. The Guide.
  4. ACCEPT-LINUX, ACCEPT-WINDOWS; push; ACCEPT-CI (cargo, the Python
     package, and `macos-preferences` must stay green).
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI.
* **stop if**: a binding's `credentials` (W3-F3) is not there to copy
  from.
* **docs**: no new D-number. CHANGELOG: "the user defaults from C++, Rust
  and Python". Roadmap: "Native OS sources" 🟩, and wave 3's F item
  done; the Ideas line about native sources in the plan from 1.1.0
  (item 10, "native OS sources (macOS defaults, gsettings)") keeps
  gsettings as the one left.
* **notes**:
