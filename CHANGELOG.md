# Changelog

Versions follow semantic versioning from 1.0. Before that, `0.x` promises
nothing: the C interface is not frozen, and a minor bump may change it.

## 0.4.0 — unreleased

### Added

- **Reading a value without repeating its default, in C++** (D-46): a
  declared `cf::Key<T>` defines the contract and reads it
  (`cfg[port]`); `cfg["server"]["port"]` is a `cf::Setting`, typed where
  it is converted, with the contract's default; and `cf::Binding<S>`
  fills the program's own struct from a field list that is also the
  contract, so the program reads `settings.server.port`. C++03, so
  every later standard has it.
- **XML, through expat, behind `CF_WITH_XML`** (D-23), which until now
  was an option that compiled nothing while `cf_has_feature("xml")`
  said yes. Read only; the root's name is in no key, attributes and
  children share one namespace, adjacent same-named siblings are a
  list, a leaf's text is trimmed and typed by the contract, and a
  DOCTYPE, mixed content, namespaces and a scattered list are refused
  by name. An XML document and the JSON written for it resolve to the
  same digest. Conan `with_xml`, vcpkg feature `xml`, fuzzed with the
  other readers.
- **A SQLite store, behind `CF_WITH_SQLITE`** - the other option that
  compiled nothing. `cf_builder_store` reads the table `configforge(key,
  type, value, meta)` as a source: each value typed by its row, and
  located as `app.db: <meta>`, so explain() says who changed a value,
  when and why, after a restart. `cf_store_set` writes typed values in
  one transaction, every item or none, and creates the store; reading
  never does. The contract applies to it as to any source. This is
  where a running program's changes go, as Design.md section 39 has
  it: hand-written files stay read only. Conan `with_sqlite`, vcpkg
  feature `sqlite`, `Builder::store` in C++.
- **A directory of secrets as a source** (`cf_builder_secrets`), one
  file a key, as Docker and Kubernetes mount secrets and systemd passes
  credentials: a secret never goes through the environment, where
  every child process can read it. A file named as its key is that key,
  one named as its environment variable would be maps the same way, a
  name that is neither is an error, the trailing newline is dropped and
  explain() names the file. Under `CF_WITH_DIR`, whose listing it uses.
- **Java `.properties`, read** (`CF_WITH_PROPERTIES`, on by default, no
  dependency): `=`, `:` or whitespace between key and value, `#` and
  `!` comments, continuation lines, the escapes and `\uXXXX` with
  surrogate pairs, as java.util.Properties reads them, in UTF-8. Keys
  are the library's keys: Spring's camelCase is Spring's to relax and
  is refused here rather than guessed at. Values are typed by the
  contract. Read only, because the format has no quoting and text
  written as 8080 would come back a number.

- **`cf_explain_json`: explain() for a machine** - one key's chain as
  JSON, for an administration endpoint, the way Spring's
  `/actuator/env` serves it. It is exactly the object the provenance
  report holds under `keys.<key>` (`cf_report_key_write` for a key
  space), redacted as the report is, so a sensitive key shows where it
  came from and its digest and never its value.
- **The report names the build that wrote it**: `library.build`, beside
  `library.version`, is `cf_build_id()`. An added field, which a reader
  of schema 1 does not object to, so the schema number stays.
- **Doctor's rules as a library** (D-41): `cf_doctor.h`,
  `libconfigforge_doctor`, of which `doctor` is now the command line. A
  program lints its own configuration in process - `cf_doctor_lint(d,
  cfg, "production", &err)` writes the full report in memory, reads it
  as `doctor lint` would and wipes it - or a report as text or a file,
  diffs two, and keeps a baseline and severities; the findings come
  back with rule, level, key, place and message. What the program kept
  in globals is a context now, and nothing in it prints. A static
  library of its own, installed as `ConfigForge::configforge_doctor`
  and left out of the amalgamation, so a device that reads
  configuration carries none of it. Every Doctor test passes against
  the program unchanged; `doctor.api` tests the library.
- **`cf_write`: a change kept, if it may be** (D-42). Before writing,
  it asks whether the source can be written (a store; nothing else),
  whether the program granted it (`cf_builder_writable`, checked at
  create, so a grant to the environment or to a misspelt source fails
  at start), and whether the contract would accept each value from it -
  the read side's own rules, so a value that would be rejected on every
  restart is never stored, and a key whose policy says "environment
  only" stays that way. Every item or none, and the error says which
  question said no. `Builder::writable` in C++. `cf_store_set` stays,
  for provisioning a store, and asks nothing.
- **Doctor from Python** (`bindings/python/configforge_doctor.py`):
  `lint`, `diff` and `version`, each finding with its rule, level, key,
  message, file and line. It runs the `doctor` program and reads its
  SARIF output, so a script sees what CI sees; the standard library
  only, and no ABI, since Doctor's library is static.
- **The feature-flag scenario, in C, C++ and Rust**: every feature
  shipped off, a canary drop-in turning two on, an environment variable
  killing one at three in the morning, and the flags discovered by
  iterating an open prefix rather than listed in the program;
  `explain()` says which layer turned it off. The Rust binding gained
  `children()` and `keys()` for it.
- **A Rust binding** (`bindings/rust`, D-44): a crate with no
  dependencies whose `build.rs` builds the library from the checkout with
  CMake, so `cargo build --offline` downloads nothing. A consuming
  `Builder`, `Result<Config, Error>`, reads that cannot fail,
  `explain()`, `derive()`, `Def` for a full definition; `Config` is
  `Send + Sync`. Five examples in `examples/rust`, the C ones saying the
  same lines, run by the suite under `-DCF_RUST=ON`.
- **Scenario examples, each in C and C++**: `preflight`, a service that
  lints its own configuration with Doctor's library and refuses to serve
  with a password typed as a flag; `policy`, a Windows editor whose
  administrator's Registry policy overrides the user's settings and
  alone may set the update channel. Checked by the suite for the same
  lines in both languages; `policy` where there is a Registry.
- **A renamed key keeps working** (D-43): a definition that is
  `CF_DEPRECATED` with `replaced_by` - a field nothing read until now -
  offers each value written under the old name to the new key, at the
  same rank and below the new spelling in the same source, and
  `explain()` places it `app.ini:2:1 (as db.host)`. Doctor's CFD009 says
  what to rename it to. `examples/c/migration.c` and its C++ twin.
- **`cf_build_limit`: the build's limits, by name** (D-45). The three
  that size `cf_error` - key, source, message - are part of a build's
  ABI, and a consumer that cannot see the headers the library was built
  with can now ask; the Rust binding checks all three before its first
  call and refuses a library built with others.
- **The symbol list**: `cmake/configforge.symbols` names every `CF_API`
  function, and `gate.abi` holds the headers and the built library to
  it - every declaration listed and every listed name declared, each
  defined in the library, and a shared library exporting nothing else.
  The interface is a file now, so a change to it is a line in a diff.
- **References between values, `${key}`** (D-10, decided): a value
  somebody wrote can use another key's - `url = https://${server.host}`.
  Precedence chooses every key first and references are replaced after,
  so a reference reads what a program reading that key would; the result
  is typed by the contract; `explain()` shows the template it was made
  from, and the report records it as `written`. A cycle, a reference to
  a key with no value, and a plain key showing a sensitive one are
  refused naming the key. `${HOME}`, naming no key, is still refused, and
  `$${` is still the literal.
- **The Windows Registry as a source** (`cf_builder_registry`, D-30):
  every value under a key such as `HKCU\Software\MyApp`, a subkey a
  section, names lowered. DWORD and QWORD are integers, REG_SZ is text
  the contract types, REG_EXPAND_SZ is kept unexpanded, REG_MULTI_SZ is
  a list; any other type and a set unnamed value are refused naming the
  value, and `explain()` gives each value's full path. A platform
  capability rather than an option - there is no dependency - so on any
  other platform the source fails by name, optional or not.
  `Builder::registry` in C++.
- **`doctor import-systemd`**: `systemd-analyze cat-config` or
  `systemctl cat` in, a report out. Each file is a source in the order
  systemd applies them, the later above the earlier, and each value is
  placed at its file's line - so CFD001 finds a key set twice in one
  file and CFD010 a drop-in that wins nothing. Blind unless `--full`.
- **`doctor import-spring` and `doctor import-dotnet`**, the second and
  third foreign adapters. Spring Boot's `/actuator/env` gives the
  sources in Spring's order and an origin for most properties, which
  become the report's precedence and places
  (`application.properties:1:13`); what Spring's sanitiser masked is
  sensitive, so a masked password on the command line is CFD007, and a
  variable not yet relaxed into a property is counted, not re-spelled.
  .NET's `GetDebugView()` names only the provider that won each key, so
  that report has winners and no chain, blind unless `--full`, for
  comparing environments with `doctor diff`. Neither adapter knows its
  ecosystem's precedence rules; each writes down what the ecosystem
  published (Doctor.md, section 12).

### Fixed

- **The C++ wrapper compiled only as C++98 on LP64 systems.** Under
  C++11 and later `cf_i64` is `int64_t`, which is `long` there, so
  `get<long>` and `get<cf_i64>` were one specialization twice. Every
  example and test built the header as C++98, so nothing noticed. The
  reads are an overload set now, and a test builds it as C++17 too.
- **The INI reader could refuse without saying why.** When its consumer
  refused to close a section, it returned failure and an empty error -
  breaking the one promise every reader makes, that a refusal names its
  reason. It says where now, as the TOML reader does; and it no longer
  ignores the consumer's answer at the end of a document. Found by
  feeding each codec a sink that refuses at each event in turn.

- **`explain()` ran a wide source name into its value.** A name or a
  value as wide as its column left no space before the next one -
  `features.inifalse` - in every chain with a source called
  `features.ini`. A column is always followed by a space now.

- **`cf_derive` kept the caller's source name, not a copy.** Every
  `explain()` of the derived configuration names the change's source, so
  a name passed from a buffer the caller then reused - a request
  handler's, or a Rust `String` - came back as whatever the buffer held
  next. It is copied into the derived configuration's arena, as its
  values always were. The Rust binding's first run found it.

- **A per-key policy refused the program's own default.** A key locked
  to one source (D-33) with nothing in that source had no value at all,
  so the program read the getter's fallback instead of the default it
  declared. A policy says who may set a key, and nobody sets a default:
  it now wins when no permitted source has a value, and only then, in a
  derived configuration too. Inherited candidates also keep what they
  were, so one the original refused keeps its reason.

- **CI was red, and nobody here could see it.** Two jobs had failed on
  every commit since the change-diff commit: clang-tidy refused
  `cf_diff` and `cf_diff_text` declaring `before`/`after` and defining
  `a`/`b`, and a branch chain with two identical bodies; and the shared
  build's gates had only ever been written for an archive -
  `gate.amalgamation` and `gate.install` linked a shared library
  without telling the program where to find it, `gate.install` looked
  for `libconfigforge.a`, and `gate.symbols` read crtbegin's
  `completed.0` and `__dso_handle` as this library's mutable globals and
  could not tell the allocator's file from the others in a library
  with no members. All four are fixed, and the shared build passes on
  Linux here; with `gh` now signed in, CI is watched rather than
  assumed.

### Changed

- **Branch coverage is past 80%**, and the floors follow it: 89% of
  lines and 80% of branches (`EXTRA=coverage`, CI's coverage job), up
  from 84 and 74. The tests that did it go where the refusals are: every
  facade call handed NULL or the wrong type; the core's names, values
  and contradictory definitions; each codec's malformed and edge-case
  documents, from data files, and a consumer that refuses at every
  event; the number conversions and the lexers at their limits; damaged
  reports for Replay; and every source and feature together in a fixed
  arena swept eight bytes at a time, with `cf_derive_in` the same way.
- **`CF_OPTIONAL` is the spelling of "may be absent"**: the flag now
  also means a Registry key, a store, a directory or a `.env`, so
  `CF_FILE_OPTIONAL` said less than it did. The old spelling is the same
  flag, kept, so nothing written against it changes; the documents and
  examples use the new one.
- **The examples are in a directory a language**: `examples/c` and
  `examples/cpp`, so the C++ wrapper in use is not one file among the C
  programs. Targets and tests keep their names.
- **C++ examples beside the C ones**: `tier0`, `layers`, `mistakes`,
  `runtime` and `reload` join `tier1` in `examples/cpp`, each the C
  program through the wrapper and checked by the suite for the same
  lines, so the two cannot drift. The wrapper gained what they needed:
  `Config::derive()` and `Config::write()`, `Builder::memory()` and
  `Builder::define()` for a full `cf_def`.
- **`cf_source_vtable` has a `write` member**, the source's capability,
  after `path`. A custom source sets it to NULL; one that does not
  compile until it does, by the same `-Wmissing-field-initializers`
  that made `path` a decision. `cf_write_item` moved above it.
- **Every package says who made it.** The Conan recipe's `author`, the
  vcpkg manifest's `maintainers` and the SBOM's `supplier` and
  `originator` name Mustafa Selçuk Çağlar, as the licence and the
  SBOM's copyright already did.

## 0.3.0 — 2026-10-24

### Fixed, and it mattered

- **A derived configuration lost the names of its sources.** `cf_derive`
  carried the original's candidates in one snapshot source called
  "inherited", so the derived configuration knew no other source:
  `explain()` said `?` for the file and the command line and listed the
  default twice, and a configuration with a per-key policy - `{ "env",
  NULL }` for a password - could not be derived at all, because no
  source called "env" was left ("the policy names only env, and this
  build has none of them"). A run-time change to any configuration
  with a policy was impossible. The original's sources now travel by
  name, rank, kind and file, their candidates are loaded ahead of the
  new source rather than as one, defaults are not seeded twice, and the
  original's files keep the stamps they had. Found by writing
  `examples/runtime.c`; `test_derive` covers it and fails without it.

### Added

- **The version is written in one place** (D-40). `cf_version.h` is the
  source; CMake reads it before `project()`, the Conan recipe reads it,
  and `cmake -DTO=x.y.z -P scripts/version.cmake` rewrites the header,
  the vcpkg manifest and the CHANGELOG heading together. `gate.version`
  fails when any of them disagree - before, it compared two of four
  copies. Tags are `vX.Y.Z`, and the release workflow refuses one that
  is not the header's version.
- **Four more examples, each a situation rather than a function**
  (`docs/Guide.md` has a table of all of them): `mistakes` (what the
  person running the program reads for each kind of mistake, and the one
  a contract may forgive), `secrets` (a password only the environment may
  set, refused from a file, redacted everywhere, wiped), `plugins` (a
  strict contract with one prefix handed over, discovered by iteration)
  and `runtime` (an admin's change in a running service, explained the
  next morning). Each is checked for its output by the suite.
- **`cf_build_id()` and `doctor --version`**: the commit a build comes
  from, `20d0f4b` or `20d0f4b-dirty`, refreshed on every build and
  carried by a `git archive` source tarball; empty when the build
  cannot know.

## 0.2.0 — 2026-10-24

### Added

- **Releases from a tag** (`.github/workflows/release.yml`): the source,
  the amalgamation, and the library with Doctor for Linux, Windows and
  macOS, each built and tested on its platform before it is packaged,
  with `SHA256SUMS` and notes taken from this file's section for the
  version. The Linux build is musl in Alpine, linked statically: one
  Doctor binary for any Linux. The Windows Doctor is linked against the
  static C runtime, so it needs no Visual C++ redistributable, while
  the library keeps the DLL runtime that programs linking it expect. A
  tag is refused before anything builds if it disagrees with
  `project(VERSION)` or if its section here still says "unreleased".
  Running the Linux and Windows jobs on a workstation before the first
  tag found Doctor over the stack budget at `-O3` and the suite not
  building in MSVC's Release, both fixed below.
- **The development environment as a file** (`scripts/buildenv`): one
  list of every tool the build, the gates and each `EXTRA` step use,
  `setup.sh`, and two ways to have it - a container (`Containerfile`)
  and, for Windows, a WSL distribution whose disk is on the drive you
  name (`wsl-distro.ps1`), so builds do not grow C:. `freebsd.sh` boots
  a throwaway FreeBSD machine inside it - FreeBSD's CI image, emulated
  where there is no KVM - and runs the unit tests against the real
  kqueue, which until now only macOS CI saw: all 41 pass on FreeBSD
  14.4, after the fix below that its first build found. Downloads come
  from the projects that publish them and are checked against their
  published checksums or signatures.
- **A usage guide and the examples it walks through** (`docs/Guide.md`):
  getting the library into a build, a contract, sources and precedence,
  asking why, errors, formats, secrets, reloading, the editor and
  Doctor, small targets, C++ and threads. Four new examples, each run
  by the suite and checked for the lines that show it does what its
  comment says: `layers` (six sources at once, and why each value won),
  `reload` (watch, rebuild, log what changed), `embedded` (a fixed
  buffer and an image, no heap) and `schema`.
- **Any C89 compiler, any C library**, checked rather than claimed:
  `EXTRA=compilers` in `verify.cmake` and a CI job run the whole suite
  against musl (`musl-gcc`) and with the Tiny C Compiler and the Portable
  C Compiler, and an Alpine job runs it on musl as a distribution ships
  it. All three pass. pcc found that `cf_port.h` trusted `__GNUC__`,
  which pcc defines without having GCC's builtins (fixed by `CF_GNUC`),
  and that its lexer reads `"\\u"` as a universal character name (two
  messages split after the backslash). tcc showed that it puts `const`
  data in `.data`, which the symbol gate now says and skips for it, and
  that it accepts `-fstack-usage` without measuring anything, so the
  stack budget is checked with GCC and Clang only. `CF_WERROR` (on by
  default) lets a compiler that warns at the C library's own headers -
  pcc at glibc's `__cold__` - build with its warnings shown.
- **Conan and vcpkg packages**, built from the checkout and consumed by a
  project that knows only the package, in CI and here: `conanfile.py`
  with every `CF_WITH_*` as an option (`with_yaml` brings libyaml from
  ConanCenter), static and shared; and an overlay port in
  `packaging/vcpkg` with a `yaml` feature. The installed headers state
  the build's options, so neither recipe passes a define a consumer
  could disagree with.
- **YAML** (D-39, `CF_WITH_YAML`, off by default: the one codec with a
  dependency). Read through libyaml's event parser, which maps onto the
  document events one to one, and written by hand in block style. A
  plain scalar is text until a definition types it, a quoted one is text
  and says so, and with the library's one spelling of a boolean (D-24)
  `country: no` is the text "no" under YAML 1.1 and 1.2 alike - the
  problem every YAML configuration has met, answered by the contract
  instead of by whichever schema a reader picked. Anchors, aliases,
  tags, several documents and non-scalar keys are refused by name. A
  `.yaml` file in a build without it names the option, as every disabled
  format does. `test_yaml` reads, refuses and round-trips.
- **An API reference that cannot fall behind** (`docs/API.md`): every one
  of the 241 public functions with the comment written above it, under
  its header's section banners, generated from the headers by
  `scripts/apiref.cmake`. `gate.apiref` regenerates it and fails when it
  differs from the committed file, so documenting a function means
  writing its header comment, and nothing else can disagree with it. No
  Doxygen: the comments are prose and stay prose.
- **The fuzz corpus is kept**: each reader's inputs that reached new code
  are written to `corpus_<reader>`, which the next run starts from; CI
  caches the directories and minimises them with `-merge=1` before
  saving, so they move forward without growing with every run.
- **Coverage-guided fuzzing** (`fuzz` preset, `CF_FUZZ`, `EXTRA=fuzz`, a
  CI job a minute per reader): libFuzzer harnesses for JSON, INI, TOML,
  `.env`, the image, Report Replay and the command line, under the
  address and undefined-behaviour sanitisers. They are `test_fuzz.c`'s
  own readers fed by libFuzzer instead of a fixed generator, and they
  start from its corpora, so there is one corpus and not two; a finding
  leaves its input as `crash-<hash>`, which CI keeps.
- **Coverage, measured and held** (`coverage` preset, `EXTRA=coverage`
  in `verify.cmake`, a CI job): 85.4% of the library's lines, 93.3% of
  its functions and 74.8% of its branches are reached by the suite.
  The step fails below `COVERAGE_MIN`, 84% of lines, or
  `COVERAGE_BRANCH_MIN`, 74% of branches - ratchets, to be raised as
  tests are added, not targets to reach once.
- **pkg-config**: an installed, relocatable `configforge.pc` - the prefix
  is found from where the file is, so an install moved with `--prefix`
  still describes itself - for builds that are not CMake. The install
  gate now also builds a consumer from what `pkg-config` says, and from
  nothing else.
- **A thread-sanitiser preset** (`tsan`, `CF_SANITIZE_THREAD`; `EXTRA=tsan`
  in `verify.cmake`, a CI job): the threading contract (D-14) under a race
  detector rather than reasoned about. The whole suite passes, `test_threads`
  included. On Linux 6.x the runtime cannot place its shadow memory when
  mmap is randomised with 32 bits, and every test dies before its first
  line; the step runs with randomisation off, as the CI job does.
- **Doctor as a GitHub Action** (`actions/doctor`): builds Doctor from the
  checkout, lints a report, prints the findings and writes SARIF for
  `upload-sarif`. Inputs reach the shell through the environment, never
  pasted into the script, because a path from a pull request is text
  somebody else wrote. A CI job uses it on a fixture report.
- **The contract as a JSON Schema** (`cf_schema`, `cf_schema_write`;
  C++: `Config::schema`), Design.md section 41: a configuration file
  that names it gets completion, inline documentation and validation in
  an editor, from the definitions the program already declared. Types,
  ranges, lengths, enumerations, descriptions, deprecation; a sensitive
  key is `writeOnly` and its default is left out. Objects are closed
  (`additionalProperties: false`) when the configuration rejects unknown
  keys, except under a prefix opened with `cf_defset_open`, so an editor
  flags a misspelt key as the program would. `required` is not written:
  a required key may come from the environment, so a file without it is
  not wrong, and a schema that said so would flag correct files.
  `test_schema` walks the result with the library's own JSON reader.
- **What a reload changed** (`cf_diff`, `cf_diff_text`; C++:
  `Config::changesSince`). One pass over two sorted key spaces, a line
  per changed key with where the new value came from - the line a
  program that watches its files should log, instead of two dumps side
  by side. A value that is the same but now comes from somewhere else
  is a change of its own kind: the configuration means the same thing,
  and the file an edit would have to touch is a different one. Secrets
  read `changed, <sensitive>` - not even their lengths, since a changed
  length is a fact about the secret. Proved by disabling the origin
  check: four of `test_diff`'s checks failed.
- **Parts of the library can be left out** (D-38): `CF_WITH_DIR`,
  `CF_WITH_DOTENV`, `CF_WITH_EMIT` (which takes every encoder with it),
  `CF_WITH_IMAGE` and `CF_WITH_REPORT`, all on by default. Off, the
  functions stay - a program links against any build - and each refuses
  by name, the rule a disabled format already followed. Everything
  optional off takes the library from 92 KB to 58 KB at `-Os`.
  `test_features.c` checks every part both ways, and the `minimal`
  preset turns them all off. Every `CF_WITH_*` is now listed once, in
  `CF_FEATURES`; the amalgamation gate had been given only the formats
  and none of the limits, so under `minimal` it compared two different
  libraries as if they were one.
- **Limits and the stack budget are the builder's** (D-37):
  `CF_KEY_MAX`, `CF_MESSAGE_MAX`, `CF_PATH_MAX`, `CF_MAX_DEPTH` and
  `CF_STACK_BUDGET` are build options, the limits written into the
  generated `cf_build_options.h` because they shape the public structs.
  Every library frame is measured with the compiler's own figures after
  the link (`scripts/stack.cmake`), checked against the budget, and
  reported by feature - "what fits in a kilobyte" is answered with
  options, and the same script answers "what if" for another budget
  without a rebuild. One nesting limit replaces three that disagreed:
  JSON accepted 64 levels that the flattener then refused at 32. The
  `minimal` preset runs the suite at 64-byte keys, 128-byte messages and
  256-byte paths.
- **Binary snapshot image** (`docs/ImageFormat.md`, decision D-28). Loads
  without parsing and without allocating, which is what a target with no
  filesystem and no heap needs and what the freestanding profile was
  missing. Optionally carries the provenance chain, so a device with no
  filesystem can still answer why a value is what it is.
- **`src/bridge`**: the flattener, the file source and the image, which
  are the only components that legitimately use both the configuration
  model and the serialization layer.
- **Flat lexers separated from recording.** `cf_lex_env` and
  `cf_lex_cli` produce entries into a sink and need no arena, no key
  space and no source object, so they are tested against an array.
- **Key mappings** (`cf_keymap`), which made the reverse direction
  possible and closed D-06: with a definition set the environment
  accepts `MYAPP_SERVER_PORT` as well as `MYAPP_SERVER__PORT`.
- **The command line's remaining forms** (D-29): `--key value`,
  `--no-key`, and aliases such as `-p`. All three need to know whether
  an option takes a value, which is the one thing a command line cannot
  be parsed correctly without, so all three arrive with a definition set
  and none exist at Tier 0. The lexer asks through a small spec
  interface rather than taking the definitions, which is what lets a
  test answer the questions with no definition set existing.
- **`.env` file source.** An environment written down, so it takes the
  same key mapping and `export NAME=value` is accepted and ignored.
  Unlike the environment it belongs to this program, so a name that
  derives no key is an error with a line number rather than somebody
  else's variable.
- **`@file` response files**, because the Windows command-line length
  limit is real and this is the universal way around it. Expansion is a
  pass of its own, so the lexer keeps needing nothing but its arguments.
  Nesting is refused: a response file that includes another is a command
  line nobody can follow from the one they typed.
- **`cf_cli_args()`**, which returns the arguments in UTF-8. On Windows
  `main`'s argv arrives in the active code page, so a value containing
  any non-ASCII character is wrong before any parser sees it — and what
  the code page dropped cannot be converted back, which is why it reads
  `GetCommandLineW` rather than converting argv. Identity everywhere
  else.
- **Drop-in directory source** (`conf.d`), which is how systemd, sudo
  and apt let a package add configuration without editing a file it does
  not own. Files are read in byte order at one shared rank, so
  `10-base` losing to `90-site` is precedence rather than a merge rule
  and `explain()` names the file that won — which is the ordering that
  fell out of last-occurrence-wins rather than needing a rule of its
  own. Dotfiles are skipped, because an editor's `.swp` and a package
  manager's `.dpkg-new` both land in such a directory.
- **Prefix iteration** (`cf_iter_keys`, `cf_iter_children`) and array
  helpers. The questions a program actually asks are rarely "what is
  server.port" — they are "what are the elements of this array" and
  "which plugins are configured", and both needed the caller to build
  keys by hand and guess where to stop. Both are now a scan from one
  binary search, and nothing is allocated.
  A prefix matches at a segment boundary only, so `server` does not
  match `servername` — the bug every hand-written prefix check has.
- **Per-parameter policy** (D-33). A definition can carry the source
  names permitted to supply that key, in decreasing precedence —
  `{ "env", "user-file", NULL }`, which reads as the specification
  writes it. One list restricts and reorders at once, which is how an
  operator-controlled setting survives a command line trying to override
  it.
  A violation rejects the candidate rather than failing construction:
  refusing to start would let any user stop the program by passing a
  locked option, and for a secret on the command line the damage is done
  the moment it is typed. It is reported instead — in `explain()`, in
  the report, and at error severity in Doctor.
- **`cf_emit`**, which writes a configuration out — restricted, as the
  specification says, to a file ConfigForge owns and rewrites in full.
  `CF_EMIT_CONFIGURED` is the one that matters: it writes what a source
  configured and leaves out what nobody touched, so an overrides file
  stays small and a later release can still move a default.
  "Configured" means "did not come from the defaults source" rather than
  "differs from the default value". The two part company when someone
  sets a value that happens to equal the default: comparing values would
  drop it and let a later release move it, while provenance keeps the
  intent that was expressed.
  An array is all or nothing — emitting only the members that pass a
  filter would produce a shorter array, which is a different
  configuration rather than a subset of one. And a format with no array
  syntax refuses before writing anything, keeping the promise the INI
  encoder made.
- **Open prefixes** (`cf_builder_open`). Unknown-key rejection and a
  dynamic key space were mutually exclusive: a plugin's settings cannot
  be in the program's contract because the plugin is not in the program
  either. An exempt prefix is a hole of a stated size — everything
  outside it is still checked, and the prefix matches at a segment
  boundary so opening `plugins` does not open `pluginsomething`.
- **`CF_TRANSIENT`**, for a key that is a command rather than a setting.
  `--cf.explain` asks a question; a file that recorded it would ask the
  question again on every run that read the file.
- **The C++03 wrapper now covers iteration, emitting and reporting.**
  A wrapper that covers half its library is worse than none: it lets
  someone believe they can stay in C++ and then drops them into C.
  Iteration is `begin`/`end` over a real iterator type, which is valid
  C++03 and gives a C++11 consumer range-for over the same interface.
- **The threading contract is settled and written into the public
  header** (D-14): reads are safe concurrently, building and releasing
  are the caller's to synchronise. An unstated threading contract is
  assumed to be full internal locking by users and nothing at all by
  implementers.
- **Snapshots are reference counted** (`cf_config_ref`), which is
  stronger than the contract requires and gives the reload story a
  foundation. Retrofitting ownership later is the same class of mistake
  as retrofitting the provenance chain.
- **`cf_derive`** (D-34), which is what setting a value means when a
  snapshot is immutable: it returns another one. The original stays
  valid and unchanged, so a reader holding it does not have it move
  underneath them and a thread reading while another sets needs no lock.
  Provenance grows rather than collapsing. The change arrives from a
  source of its own above the command line, and the displaced value
  stays in the chain — so `explain()` afterwards says what it is now,
  what it was, and where that came from, which is the question somebody
  asks right after changing a setting.
  The contract travels with it: definitions, open prefixes and the
  unknown-key policy are inherited, because a derived configuration that
  accepted what the original refused would be a different contract
  wearing the same name.
- **Change notification** (D-35): `cf_changed`, `cf_watch_count`,
  `cf_watch_path`. Pollable, never a callback — a library that owns a
  thread and calls you back has made a threading decision that belongs
  to the caller, and calling back while that caller reads is the race
  the threading contract exists to avoid.
  A conflict the design expected did not appear. It predicted that
  enabling watch would change `cf_get_text`'s lifetime contract; because
  snapshots became immutable and reference counted for other reasons, a
  reload produces a *new* snapshot and the old one stays valid while
  anything holds it. Watching changes no contract at all.
  What is compared is modification time and size, which misses a file
  rewritten inside one clock tick at the same length. That is documented
  as a hint worth acting on rather than a guarantee; the exact answer is
  to rebuild and compare digests.
- **A codec from outside the library**: `cf_builder_file_codec` (C++:
  `Builder::fileWith`) reads a file through a `cf_codec` the caller
  defines - over libyaml's events, expat's callbacks, any streaming
  parser - since the document events are already format-neutral. It is
  handed over rather than registered, because a run-time registry would
  be global mutable state. `test_codec.c` is a complete one written
  against the public headers alone, and the values it produces are
  typed, validated and explained like any other.
- **A way in for whoever continues the work.** `AGENTS.md` - read by a
  person or an AI agent, on any machine - maps the repository, gives the
  build-verify-commit loop, the house rules and every trap this work
  fell into, and the recipes for extending it; `CLAUDE.md` points at it.
  `docs/Roadmap.md` holds what is next, what is known to be wrong, and
  the ideas not yet built, and is kept in the same commit as the work.
  `CONTRIBUTING.md`, `SECURITY.md` (private reporting, and what is in
  scope: every reader of untrusted bytes), `CODE_OF_CONDUCT.md`, issue
  and pull request templates, and Dependabot for the CI actions.
- **Usage as a test.** `tests/unit/test_usage.c` is the smallest real
  program for each usage tier, compiled and run, so the tiers cannot
  name an interface that does not exist - which Design.md's did: its
  Tier 1 and Tier 2 examples called functions from an early draft that
  the library never had. They show the real API now.
- **jq recipes for reports** (Doctor.md, section 7a), each run against
  a real report, and the reasoning for not writing a query language of
  our own (Roadmap.md).
- **TOML**, as the subset D-23 described, built in and on by default:
  tables and arrays of tables, inline tables, dotted keys, basic and
  literal strings with every escape, integers in four bases, floats,
  booleans and arrays, each value with its line and column. Dates,
  multi-line strings and inf/nan are refused with a message saying what
  to write instead. It reads, it writes for `cf_emit`, and what it
  writes reads back as the same configuration; it is in the fuzzer and
  the amalgamation like the other readers.
  The round trip found that the emitter wrote arrays after tables,
  which TOML cannot express - an array after a sub-table's header lands
  in the sub-table. Arrays come before tables at every level now, which
  every format with arrays writes inline.
- **A handle an event loop can wait on** (D-35, option 3): `cf_watcher`,
  an inotify descriptor on Linux and one event HANDLE on Windows, plus
  `cf_watcher_wait` for a program with no loop. Still no callback and no
  thread. It watches each file's *directory*, because editors save by
  renaming a new file over the old one, so a wake-up means "ask now" and
  `cf_changed` remains the answer. A directory that does not exist yet
  is watched through its nearest ancestor and the watch moves down as
  the levels appear, so an optional file nobody has created can still
  wake the caller when it arrives. On macOS and the BSDs it is a kqueue,
  which watches what it has open rather than a name, so it watches each
  file as well as its directory and reopens everything on every drain.
  `CF_WAKE_KQUEUE` builds that path on Linux against libkqueue, and CI
  runs it there on every commit as well as on macOS.
- **Doctor proves CFD001, CFD007 and CFD013.** CFD001, an unreachable
  value, fires where it can be proved from one report: the same source
  sets a key twice in one file or command line, the later occurrence
  wins wherever it is read, and editing the earlier line has no effect.
  The broader reading - every shadowed candidate - cannot be proved from
  one environment and would bury the finding under precedence working
  as designed. CFD007 is a secret on the command line that no policy
  refused; CFD013 a policy that ranks a less specific source above a
  more specific one, which is what explains an `explain()` where the
  command line loses to a variable.
- **Doctor can be adopted on a project's own terms.** `--severity`
  reads a file of `CFD010 warning` lines, with an optional profile -
  `CFD008 error production` - and `off`; `--fail-on` sets the threshold;
  `--format sarif` writes SARIF 2.1.0 so a forge shows each finding on
  the line that caused it. Doctor.md had asked for all three from the
  first release, because a lint tool that cannot be tuned is turned off.
  Profiles arrive the way D-12 decided: as a label a rule's severity can
  follow - CFD008, "default-only in production", is a warning there and
  information elsewhere - not as a concept in the library.
  CFD010 is a warning by default now. A system file wholly overridden by
  a user's is often what somebody meant, and an error made that a
  failed build.
- **`doctor import-git`**, the first foreign adapter. It turns
  `git config --list --show-origin --show-scope` into a provenance
  report through the library's own writer, so every Doctor command runs
  on git's configuration unchanged - a scope that wins nothing is CFD010
  there too. Blind unless `--full`, because git configuration carries
  credentials more often than it looks; a key a report cannot carry is
  left out and counted rather than mangled into one that collides.
- **A licence: zlib.** A binary built with the library, firmware
  included, owes nobody a notice; every source distribution keeps one,
  so the single-file amalgamation carries it at its top. Installed with
  the headers. The options weighed are D-36.
- **A bill of materials.** `docs/Dependencies.md` lists every
  dependency with its licence and when it is needed - the default build
  needs none - and each build generates an SPDX 2.3 SBOM from the
  options it was configured with, installed beside the licence.
  `gate.dependencies` fails when the build looks for something the page
  does not name, and `gate.sbom` when the SBOM does not parse or
  disagrees with the project's version and licence.
- **CFD007 sees files.** A report records, for a sensitive value that
  came from a file, whether every user of the machine can read that
  file - the library records it because Doctor judges other machines'
  reports and must never look at its own disk. Windows cannot say with
  a single bit, so it says nothing rather than guessing.
- **The C++03 wrapper covers change notification**: `Config::changed()`
  and `Watcher`, which owns the handle and is not copyable - two owners
  of one descriptor is a double close - plus the `dir()` and `dotenv()`
  sources it had been missing.
- **Three more Doctor rules** that a single report can prove from its
  definitions alone: `CFD005` a key with no definition, `CFD009` a
  deprecated key something is still supplying, `CFD011` a key set to an
  empty value — which is not the same as being unset, and is usually a
  variable that expanded to nothing rather than a decision.
- **ConfigForge Doctor**: `dump`, `explain`, `lint`, `diff`, and
  `lint --baseline`, which records existing findings as accepted so that
  only new ones fail. A tool that cannot be adopted gradually is a tool
  nobody adopts.
- **Provenance report** (schema v1) and **Report Replay**.
- **C++03 wrapper**, compiled as C++98 by a gate.
- **Amalgamation**, compiled and run by a gate.
- `cf_version()`, `cf_feature_at()`, and a gate that fails when the
  header version and the project version disagree.
- **A deterministic mutation fuzzer** over the codecs, the `.env` lexer
  and the binary image. The seed is the bug report; the generator is
  written out rather than taken from `rand()`, which is reproducible on
  no two machines.
- **`gate.formats`**, which checks every format string in the library
  against what `cf_fmt` actually supports. `cf_fmt` has no width,
  precision or flags, and an unsupported conversion is copied out
  literally — which is how every provenance report carried `%08x` for a
  whole session. Proved by putting that bug back.
- **`test_complexity`**, which measures the claims that are shapes:
  resolution at one size and four times it, where `O(n log n)` and
  `O(n squared)` are 4.3x and 16x apart. A regression from a binary
  search to a scan would have passed every other test here.
- clang-format and clang-tidy as build targets and gates.
- cmake-format and cmake-lint, so the build files get the same treatment
  as the C sources.
- **A compiler-verified warning standard** (D-32). The flags are grouped
  by what they protect rather than by which compiler has them, and each
  one is put to the compiler at configure time with `-Werror` set, so a
  compiler that merely warns about an unknown `-W` flag still fails the
  check. No table to keep true.

### Changed

- **A value containing `${` is refused** (D-10). References to other
  values are not supported yet, and taken literally,
  `cache.dir = ${HOME}/cache` created a directory called `${HOME}` -
  and the day references arrive, every such configuration would have
  changed meaning without a word. It is an invalid value like any other:
  construction fails and says where, unless the parameter asked for
  fallback, in which case `explain()` shows it refused. `$${` is a
  literal `${`. A program's own default is code rather than
  configuration and is left alone, and a replayed report is not refused
  the second time round. A `.env` written for a tool that does expand
  `${VAR}` now fails loudly rather than silently meaning something else.

- **Within one source, the later occurrence of a key now wins.** It was
  the first, which is the convention nowhere: a command line where
  `--port=1 --port=2` gives 2, an INI file where the second assignment
  wins, a drop-in directory where `90-site` overrides `10-base`. Found
  by an end-to-end run where a response file beat the arguments typed
  after it.
  The environment is a set and has no order of its own, so where both
  the escaped and the natural spelling are present the escaped one wins
  by construction rather than by which pass happened to run second.

### Fixed, and it mattered

- **The library did not build on FreeBSD.** The stack check reads the
  `.su` files `-fstack-usage` writes, and expected GCC's
  `file:line:column:function`; FreeBSD's clang 19 writes
  `file:line:function`, so nothing was read and the check failed the
  build on an empty table. Both forms are read now, and a file in
  neither form fails with its first line quoted rather than an internal
  error. Found by the first build in a real FreeBSD machine
  (`scripts/buildenv/freebsd.sh`).

- **Doctor did not build at CMake's own Release settings on Alpine.**
  At `-O3` Alpine's gcc inlined every lint rule into `main` - each is
  called exactly once - without sharing their buffers, and `main` came to
  4416 bytes against a budget of 2048; `-Werror` made that a failed
  build. Ubuntu's gcc at the same level, and every preset (none of which
  is `-O3`), stayed under, which is why only the release job found it.
  The rules now run from a table through a pointer, which no compiler
  inlines into one frame, and `main` is 1328 bytes there. The CI Alpine
  job now builds Release, so it is checked before a tag rather than at
  one.
- **The suite did not build in MSVC's Release.** `test_numeric` makes
  infinity and NaN by dividing by a zero in a variable, since `INFINITY`
  and `NAN` are C99; the optimiser saw the constant, warned "potential
  divide by 0" (C4723), and `/WX` made it an error. Every Windows preset
  is Debug, so only the release job would have built it. The zero is
  `volatile` now.
- **`test_complexity` failed on correct code under load.** It compared
  one key space of 4000 keys against one of 16000, and on a 12 MiB cache
  the first fit and the second did not, so the ratio measured the cache:
  8 to 12 times for fourfold data, against a bound of 8. It now resolves
  the same total number of keys as many small spaces or a few large
  ones, both in cache, so the ratio is the cost per key: 0.8 to 1.7 for
  correct code under any load tried, about 4 for a quadratic one, and
  the bound is 2.5.

- **An image's provenance travelled and nothing read it.** With
  `CF_IMAGE_PROVENANCE` the writer stored every candidate - source,
  outcome, value, location - and the reader loaded the winners and said
  "image" for all of them, so the reason to carry the chain, a device
  with no filesystem still able to say why a value is what it is, was a
  claim. Loading now takes each value's location from its winning
  candidate: on the device, `explain()` says `release.ini:2:1`, the line
  on the build machine. Found by writing `examples/embedded.c`, whose
  comment said so before the code did. With the lookup disabled,
  `test_image` fails.

- **Frames that were larger than they needed to be**, found by the stack
  report on its first run. The flattener kept 24 bytes for each of its
  32 nesting levels where 8 hold the same (a 32-bit index, a 16-bit
  length, a flag): 512 bytes off every frame that reads a document,
  `dir_load` 1344 to 832 at short limits. The inotify drain read into a
  kilobyte where two of the largest records are enough, the loop
  draining until empty either way. `cf_emit` held a fixed kilobyte for
  encoder state; `CF_ENCODER_STATE_MAX` follows the limits now, and each
  encoder asserts at compile time that it fits. The INI encoder had its
  nesting fixed at 32 regardless of the limit. Without floating point
  and with 64-byte keys, every library function now fits in a kilobyte.

- **The TOML decoder broke the stack budget**, by four times: thirty-two
  open tables and two dotted keys each held a key-sized buffer per name,
  8.7 KB in one frame (10.9 KB once `-Os` inlined the callers). Linux
  CI had been red since TOML landed, because MSVC, where it was tested,
  has no stack check. The names now share one key-sized buffer, which
  costs nothing: the whole path has to fit in one key anyway. The
  `portable` and `minimal` presets now build optimised (`-O2`, `-Os`),
  so the inlined case is checked on every verify; that also found a
  `system()` result that a `(void)` cast does not silence under glibc's
  fortified headers.
- **TOML did not compile without floating point** (`CF_WITH_REAL=OFF`,
  the `minimal` preset): its number buffer was sized by the float
  digits. It is sized by `CF_VALUE_TEXT_MAX` now, and a float in such a
  build is refused by name, which `test_toml` checks.
- **`cf_ks_resolve` could fail silently.** It allocated a scratch array
  for the merge sort, set the key count to zero when there was no room,
  and returned `void`. Between roughly 11.7 and 12 kilobytes of fixed
  arena, `cf_builder_create` therefore *succeeded* and handed back a
  configuration with no keys in it — a device in that window would have
  booted with every value at its default and no indication that anything
  had gone wrong, which is worse than not booting.
  It returns `cf_bool` now and the builder reports it, naming the number
  of candidates it could not sort.
- **`cf_build_in`**, which was missing. `cf_arena_fixed` existed and
  nothing in the builder accepted an arena, so the claim that a target
  with no heap can hand over a buffer and have the library work inside
  it had no way to be true. The arena is borrowed: `cf_config_free`
  never frees a buffer this library did not allocate.

### Interface safety

- **An installed library now ships facts about itself, not defaults.**
  The `CF_WITH_*` options were defaulted in `cf_port.h`, so a consumer
  compiling with a plain `-I` against an installed library saw
  `CF_WITH_REAL` as 1 even when the library had been built with 0. That
  is not a link error in general: `CF_WITH_REAL` decides whether
  `cf_value`'s union holds a double, so the two sides disagree about
  `sizeof` and the corruption surfaces somewhere else entirely. The link
  error that revealed it was the lucky outcome.
  The defaults moved to `cf_build_options.h`, which exists twice on
  purpose: the source tree's copy holds `#ifndef`-guarded defaults for
  someone compiling the sources, and the copy generated at configure
  time states this build's values unconditionally and is the one that
  gets installed. `gate.install` compiles a consumer with a plain `-I`
  and checks both the headers and the compiled library agree — proved by
  making the generated header lie.

### Security

- **explain() and inspect() printed sensitive values in the clear**, and
  so did the reason a sensitive value was refused - which explain()
  shows and a strict definition raises as an error. Design.md said
  secrets must not appear in diagnostics; nothing checked it. The chain
  is still shown in full - which source won, which lost, where each was
  set - and each value of a `CF_SENSITIVE` key reads
  `<sensitive, N bytes>`, what a blind report gives. A refusal names
  the range or the type and not the value. `test_secrets` checks all of
  it; with the redaction disabled it fails five times, printing exactly
  the leak it is there to prevent. `cf_ks_explain` and `cf_ks_inspect`
  stay the raw renderers for a key space with no definitions;
  `cf_ks_explain_redacted` and `cf_ks_inspect_redacted` take them.
- **Freed memory no longer holds the configuration's secrets.** An
  arena now zeroes every byte it handed out before giving it back -
  through a volatile pointer, because a store to memory about to be
  freed is the textbook dead store an optimiser removes. That covers
  every value and the raw text of every file read, on a successful
  build, a failed one and an abandoned one; `cf_arena_wipe` does the
  same for a caller-owned arena (`cf_build_in`). `test_wipe` watches
  every block as it is released and searches it for the secret; with
  the wipe disabled, all three of its cases fail. A first version also
  tested an abandoned builder, which never holds the secret because
  nothing is read before create - a test that passed either way, so it
  was replaced by a build that fails after reading.
- **The binary image reader dereferenced a file-supplied index without
  bounding it.** Every region the header describes was validated against
  the image's length; the value index inside each entry was not, so a
  corrupt image could read anywhere. Found by a mutation fuzzer on its
  first run — the image reader is the one reader here that dereferences
  offsets taken from its input, which is exactly why it was the most
  valuable thing to fuzz.
  Bounds are now written as subtraction rather than addition, because
  two 32-bit values and a one can wrap where `size_t` is 32 bits and a
  wrapped bound passes every check it was meant to fail. The chain
  region is validated too, so a reader added later does not inherit an
  unchecked offset.

### Fixed

- **A path outside the Windows code page could not be opened.** The
  platform layer called the code-page ("A") functions and the file
  reader called `fopen`, so a configuration under a name the active code
  page cannot spell - a Greek letter on a Turkish or Western machine -
  was unreadable, unstampable and unlistable. Paths are UTF-8 at the
  interface (D-15) and now UTF-16 to Windows: every path goes through
  one conversion into a "W" function, and files open through
  `cf_platform_fopen`, which is `_wfopen` there and `fopen` everywhere
  else. `test_unicode` builds such a tree and reads it as a file, a
  drop-in directory and a change check; against the old calls it fails.
  Its first version used only Turkish letters and passed against the
  old code too, because this machine's code page spells them - the
  alpha in its directory name is the part that proves anything.
- **A shared library was unusable on Windows and leaked internals
  elsewhere.** Nothing was marked for export, so a DLL exported nothing.
  Every public function is `CF_API` now - dllexport building the DLL,
  dllimport using it, `visibility("default")` under
  `-fvisibility=hidden` on ELF and Mach-O - and whether the build is
  shared is stated in the generated `cf_build_options.h`, so a consumer
  of an installed DLL is told rather than having to know. A CI job
  builds and runs the whole suite as a shared library on all three
  systems.
- **The Windows build had never been built.** The first MSVC run failed
  to compile `win32.c` - `_environ` declared by hand collided with the
  CRT's macro - and two `/W4` warnings stopped the tests and Doctor. The
  suite then failed one test for the right reason: `cf_cli_args` reads
  the real wide command line on Windows, and the test handed it an
  invented argv. It now uses the process's own arguments, and
  `unit.test_lex.utf8` asserts the non-ASCII round trip the wide path
  exists for. The four gates that drive the compiler with a GCC-style
  command line are registered only where one exists.
- **A drop-in directory edited in place was not a change.** `cf_changed`
  stamped a directory by its own modification time, which moves when a
  file is added or removed and not when one is rewritten. A directory is
  now stamped by what it holds.
- **The `analyze` preset had never analysed anything.** cppcheck defines
  none of the compiler's macros, so `cf_port.h` stopped it at its
  `#error` for want of a 64-bit type in every translation unit. Told
  which compiler it is looking at, it analyses the library clean; the
  tests get its warnings and not its style class, which calls a test
  asserting a constant on purpose "a condition that is always true".
- **The clang job could not have passed.** Clang 18 rejected the fixed
  arena's casts under `-Wcast-align`, and clang-tidy 18 failed the
  library on nineteen findings, two of them analyzer paths through an
  image that had not opened and an explain buffer that was NULL with a
  capacity. Both are closed - the image is zeroed before anything can
  fail, a NULL buffer always measures - and the rest were one-line
  fixes. clang-tidy now checks the library and Doctor, not the tests
  and examples, whose forty findings were unchecked `printf` results.
- **The C format gate depended on the formatter's version.**
  clang-format 18, which CI installs, and 22 disagreed on three lines;
  they are written so that both leave them alone.
- **One leaked secret was three Doctor errors.** A candidate rejected by
  policy was also counted as a contract violation (CFD006) and as a
  source that lost (CFD010). A rejection is reported once, by the rule
  that owns it.
- **Rounding carry did not travel past the last digit.** The formatter
  handled a carry out of the leading digit and silently produced `"1:"`
  when the last one overflowed. Found by clang-tidy noticing the dead
  store that was the symptom.
- **Digest was the literal format string.** The report wrote `%08x%08x`
  through a formatter that has no width specifiers, so every report
  carried `fnv1a:%08x%08x`. The test only checked the prefix.
- **Emission put root keys after the first section**, so an INI file
  written by the library read back as a different configuration. Fixed by
  ordering leaves before containers at every level, which fixes it for
  every format rather than in one codec.
- **Key space insertion was quadratic.** Two-phase construction with one
  merge sort at resolve.
- **GCC-only warning flags were handed to every compiler**, which would
  have broken a real clang build. Found by clang-tidy's front end.
- Arena leak in Doctor, unaligned fixed arenas, a key space that kept
  working after a failed init, `-Wundef` on a feature macro visible in
  one translation unit and not another.

### Notes

- `cmake-format` with `keyword_case: upper` rewrote the *variable* in
  `get_filename_component(name "${f}" NAME)` to `NAME`, because the token
  matches a keyword of that command, and silently broke two gates. The
  setting is now `unchanged`: a formatter that can change meaning by
  spelling gets one job, whitespace. The gates caught it, which is the
  argument for having them.

### Removed

- **`CF_WITH_WATCH`**, which switched nothing. It was off by default, so
  `cf_has_feature("watch")` said no on every build while `cf_changed`
  and `cf_watcher` worked on all of them. The feature list now reports
  "watch" where the platform has a handle to wait on, which is the only
  thing the word could honestly mean.

- The hand-written `Makefile` (D-31). The no-build-system path is the
  amalgamation, which is generated from the real sources and tested on
  every commit rather than merely present.

## 0.1.0

Milestone M0: portability layer, exact shortest-round-trip numeric
conversion, arena, value model, flat key space with the provenance
chain, definitions and validation, sources, `explain()` and `inspect()`,
and the `cf_config` facade.
