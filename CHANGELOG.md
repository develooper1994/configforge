# Changelog

Versions follow semantic versioning from 1.0. Before that, `0.x` promises
nothing: the C interface is not frozen, and a minor bump may change it.

## 0.2.0 — unreleased

### Added

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

- The hand-written `Makefile` (D-31). The no-build-system path is the
  amalgamation, which is generated from the real sources and tested on
  every commit rather than merely present.

## 0.1.0

Milestone M0: portability layer, exact shortest-round-trip numeric
conversion, arena, value model, flat key space with the provenance
chain, definitions and validation, sources, `explain()` and `inspect()`,
and the `cf_config` facade.
