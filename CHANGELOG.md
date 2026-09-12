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
- **ConfigForge Doctor**: `dump`, `explain`, `lint`, `diff`.
- **Provenance report** (schema v1) and **Report Replay**.
- **C++03 wrapper**, compiled as C++98 by a gate.
- **Amalgamation**, compiled and run by a gate.
- `cf_version()`, `cf_feature_at()`, and a gate that fails when the
  header version and the project version disagree.
- clang-format and clang-tidy as build targets and gates.

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

### Removed

- The hand-written `Makefile` (D-31). The no-build-system path is the
  amalgamation, which is generated from the real sources and tested on
  every commit rather than merely present.

## 0.1.0

Milestone M0: portability layer, exact shortest-round-trip numeric
conversion, arena, value model, flat key space with the provenance
chain, definitions and validation, sources, `explain()` and `inspect()`,
and the `cf_config` facade.
