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
- **ConfigForge Doctor**: `dump`, `explain`, `lint`, `diff`.
- **Provenance report** (schema v1) and **Report Replay**.
- **C++03 wrapper**, compiled as C++98 by a gate.
- **Amalgamation**, compiled and run by a gate.
- `cf_version()`, `cf_feature_at()`, and a gate that fails when the
  header version and the project version disagree.
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

### Fixed

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
