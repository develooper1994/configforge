# AGENTS.md

The way into this repository for anyone continuing the work - a person
or an AI agent, on any machine. Read this first, then follow the links;
nothing here repeats what a linked document already says.

## What this is

ConfigForge is a configuration library for C (C89, with a C++03 wrapper)
whose one distinguishing promise is that every value can answer **why it
is what it is**: which source won, which lost, and where each was set.
ConfigForge Doctor (`tools/doctor`) reads the provenance reports the
library writes and finds configuration mistakes before deployment.

If a change makes a value harder to explain, it is the wrong change.

## Where things are

| Path | What | Read |
| ---- | ---- | ---- |
| `include/configforge/` | the public headers; `configforge.h` is the facade | the header comments are the API reference; `docs/API.md` collects them |
| `src/core` | value model, key space, definitions, validation, `cf_config` | |
| `src/sources` | defaults, memory, environment, command line | |
| `src/serialize` | blob, sink, INI and JSON codecs | |
| `src/bridge` | file, `.env`, drop-in directory, image, emit | |
| `src/diag` | `explain()`, provenance report, Report Replay | |
| `src/platform` | the only code that knows the operating system | |
| `tools/doctor` | the Doctor CLI and its adapters | `docs/Doctor.md` |
| `tests/unit` | one C program per area; also the best usage examples | |
| `tests/doctor` | Doctor's fixtures and CLI tests | |
| `examples/c`, `examples/cpp`, `examples/rust` | programs the README and `docs/Guide.md` quote, one directory a language, run by the suite and checked for their output (`expect.cmake`) | `docs/Guide.md` |
| `bindings/rust`, `bindings/python` | the Rust crate (`build.rs` builds the library) and the Python package (ctypes over a shared build), each with its own tests | `docs/Guide.md`, section 13 |
| `cmake/`, `cmake/gates/` | build modules, and the gates that enforce the design | `docs/Architecture.md` |
| `scripts/` | verify, format, stack, apiref, amalgamate | `scripts/README.md` |
| `conanfile.py`, `packaging/` | the Conan recipe and its test package; the vcpkg overlay port | README.md, "From a package manager" |

The documents, and what each is for:

* `docs/Architecture.md` - the layers, the rules between them, and
  **recipes**: adding a format, a source, a definition attribute, a
  Doctor rule, a platform capability.
* `docs/Conventions.md` - house style, memory rules, the forbidden list,
  what a good test looks like. Rules marked **[gate]** are enforced.
* `docs/Decisions.md` - every design decision with the options weighed.
  Check it before changing behaviour; add an entry when you decide one.
* `docs/Design.md` - the full specification.
* `docs/Doctor.md` - the rule catalogue and Doctor's design.
* `docs/Dependencies.md` - every dependency and licence; the build
  generates an SPDX SBOM to match.
* `docs/Handoff.md` - what an agent learnt that the code does not say:
  how the owner wants the work done, where it stands, and what each
  machine needs. Read it second.
* `docs/Roadmap.md` - **the state of the work**: what is next, what is
  known to be wrong, and the ideas not yet built. Update it in the same
  commit as the work.
* `CHANGELOG.md` - what changed, and why it mattered.

## Build, test, verify - on any OS

CMake is the only tool every step needs; each script is `cmake -P`, so it
runs wherever the build does.

```sh
cmake --preset default && cmake --build --preset default && ctest --preset default
cmake -P scripts/format.cmake                       # format and lint what changed
cmake -DSOURCE=index -DJOBS=2 -P scripts/verify.cmake   # verify exactly what is staged
cmake -DSOURCE=index -DEXTRA="kqueue;tidy;clang;tsan;yaml;compilers;fuzz;coverage" -P scripts/verify.cmake
```

`verify.cmake` runs five presets (default, sanitize, portable, minimal,
analyze) and stops at the first failure with that step's output. Use
`-DMODE=quick` for the default preset alone. Use `-DJOBS` - five
unbounded parallel builds have filled a workstation's memory.

The tools every step and every `EXTRA` needs are one list,
`scripts/buildenv/setup.sh`, which makes a container
(`Containerfile`) or, on Windows, a WSL distribution on the drive you
name (`wsl-distro.ps1`). `freebsd.sh`, run inside it, is the real
kqueue for a machine with no Mac. `scripts/README.md` has the commands.

## How work is done here

1. **One verified unit per commit.** A unit is a thing that is true on
   its own. Format, stage it, then `verify.cmake -DSOURCE=index`, and
   commit only on "all green". Never decide from `grep` on the output;
   the exit code is the answer (Conventions.md says why).
2. **Code and documents change together.** A behaviour change updates
   the header comment (then `cmake -P scripts/apiref.cmake`, or
   `gate.apiref` fails; a new or removed `CF_API` function also goes in
   `cmake/configforge.symbols`, or `gate.abi` fails), the relevant
   `docs/` page, `CHANGELOG.md` and,
   where it is a decision, `docs/Decisions.md` - in the same commit.
   Where a claim can be checked, add a gate rather than a sentence.
3. **Prove a test can fail.** Disable the behaviour by returning early
   (not by deleting a call, which breaks the build and leaves a stale
   binary passing), confirm the test fails, restore.
4. **Commit messages are prose.** The title says what is now true; the
   body says why, what was rejected, and what was found on the way.
   Read `git log` for the register.
5. **Branches for risky work**, merged back to `main` when verified.
6. **A release** is `cmake -DTO=x.y.z -P scripts/version.cmake` (the
   version is written once, in `cf_version.h`), the CHANGELOG heading
   dated, and the tag `vX.Y.Z` pushed; the release workflow does the
   rest and refuses a tag that disagrees. Then bump to the next version
   so new entries have a section.

## Things that have bitten, so they need not again

* **MSVC has no C89 mode.** The GCC-style gates (headers, amalgamation,
  install, interface) are not registered under MSVC; the library and
  tests are built at `/W4 /WX` there instead.
* **On Windows `cf_app` and `cf_cli_args` read the process's real
  command line** (for UTF-8), not the argv you pass. A test that invents
  an argv must build with `cf_builder_cli`.
* **Multi-config generators** (Visual Studio) need `-C Debug` for ctest
  and `--config Debug` for builds; the presets name their configuration.
* **`cmake -P` scripts start with every policy unset.** Begin each with
  `cmake_minimum_required(VERSION 3.10)`, or `IN_LIST` and friends work
  on CMake 4 and fail on 3.28.
* **clang-format 18 (CI) and newer versions disagree** on a few layouts.
  Write code that neither touches: a typedef for a pointer to an array,
  a function for a two-line shift-and-or, `clang-format off` around a
  brace initialiser in a macro.
* **C90 caps string literals at 509 characters.** Split long usage text
  across several `fputs` calls.
* **`-Wconversion` and the ternary:** `'_'` is an `int` in C, so cast
  the whole `?:`, not one arm.
* **The stack budget is 2048 bytes per function** in the library
  (`-Wstack-usage`); buffers over a kilobyte in tests are `static`.
  Inlining adds frames up: a function called once is inlined whole, and
  a dozen of them called in a row from one place can exceed the budget
  at `-O3` on one compiler and not another. Call such a list through a
  table (Doctor's rules are the example).
* **A test that watches files runs in its own `WORKING_DIRECTORY`**:
  every other test writes into the shared one, which is exactly what a
  watcher sees.
* **libkqueue is not a real kqueue.** It does not report a directory
  created inside a watched one, and its descriptor cannot be polled.
  `scripts/buildenv/freebsd.sh` is the check on FreeBSD's real one, and
  the macOS CI job on Darwin's.
* **gcc `-fanalyzer`, cppcheck and clang-tidy each find different
  things**, and each found a real one here. Fix the code, not the tool;
  suppress only with a comment saying why the finding is wrong.
* **A round trip is the test that finds ordering bugs.** The TOML
  round trip found the emitter writing arrays after tables; the INI one
  had found root keys landing in the last section. A new writer gets a
  round trip that prints both sides when they differ.
* **Code-page tests need characters the machine's code page lacks.** A
  Windows machine on code page 1254 spells Turkish letters, so a test
  using only those passes against code-page functions too.
* **`__GNUC__` does not mean GCC.** pcc defines it as 4 and has none of
  GCC's builtins. Test `CF_GNUC` (`cf_port.h`), which is 0 there.
* **pcc's lexer reads `"\\u"` as a universal character name**; split
  such a literal after the backslash (`"\\" "u..."`). It does so in a
  comment too, and refuses one that is not a valid character - `\uXXXX`
  in prose, a short `\u12`, a surrogate half - so describe the escape
  in words there. tcc puts `const`
  data in `.data`. `EXTRA=compilers` is the check for all of this.
* **A `static` name is unique across the whole library**, not just its
  file: the amalgamation puts every `.c` in one translation unit, so two
  files each with a `static source_name` compile everywhere except
  there. Prefix a file's statics with its own name (`diff_`, `schema_`).
* **For an agent editing through a shell:** a heredoc or a quoted
  string can lose backslashes on the way, which turns `'\0'` into a NUL
  byte and `\n` into a line break in C source. Write the new text to a
  file and patch from the file, then check the result byte for byte.
* **MSVC checks no stack budget, and Debug inlines nothing.** A frame
  that breaks the budget can pass Windows and Debug and fail an
  optimised GCC build; the `portable` (`-O2`) and `minimal` (`-Os`)
  presets are the check. Verify on Linux before merging anything that
  adds a buffer.
* **Under WSL, verify off `/mnt/c`**: `-DWORK=$HOME/cfv`. That
  filesystem has no inotify and no Unix permissions, so `test_watch`
  and Doctor's permission rule fail there for reasons not in the code.
* **A single-threaded build compiles the atomics out.** The `portable`
  preset is `CF_SINGLE_THREADED`, so a test that starts threads must
  skip there (`!defined(CF_SINGLE_THREADED)`); `test_live`'s spun
  forever on a lock that was no longer atomic.
* **`git mv` stages.** Moving a file in the middle of a unit puts the
  move in the index; `verify -DSOURCE=index` then verifies half of the
  next unit. Move with the unit that owns it.
* **The C++ header is checked under every standard** (`gate.headers`,
  98 to 23). It once compiled only as C++98: on LP64, C++11 makes
  `cf_i64` `long`, and two explicit specializations became one.
* **Every `wsl` call writes WSLg traces to C:** (`%TEMP%\DiagOutputDir`),
  gigabytes a day under load. Batch a unit's checks into one script.
* **Watch the disk on a long session.** Each preset is a full build;
  `verify.cmake` removes its staged copy on success, and `build-*`
  directories are disposable and in `.gitignore`.

## Extending it

The recipes in `docs/Architecture.md` cover formats, sources, definition
attributes, Doctor rules and platform capabilities. Two more, from work
done since:

* **A Doctor adapter** (`tools/doctor/git.c` is the model): read an
  ecosystem's own origin data, build a `cf_keyspace` and a source list,
  and write the report with `cf_report_write` - never by hand, so it
  cannot drift from the schema. Do not reimplement anybody's precedence
  rules (Doctor.md, section 12).
* **A codec in the program, not the library**: `tests/unit/test_codec.c`
  is the model - a `cf_codec` whose `decode` drives `cf_doc_sink`,
  handed to `cf_builder_file_codec`.
* **An optional part** (D-38): a name in `CF_FEATURES`
  (`cmake/ConfigForgeOptions.cmake`), a default in both copies of
  `cf_build_options.h`, the work under `#if CF_WITH_X` with a stub that
  fails by name, a string in `cf_has_feature`'s list, and both
  directions in `tests/unit/test_features.c`.
* **A gate**: a `cmake -P` script in `cmake/gates/`, registered in
  `cmake/ConfigForgeGates.cmake`, that fails with `VIOLATION:` lines
  naming what is wrong. Prove it by breaking the thing it guards.

## Safety

Nothing that builds, tests or verifies uses the network: `verify`,
`format`, the gates and the build download nothing. The exceptions are
named and only run when asked for: the three scripts in
`scripts/buildenv` that make an environment, which download from the
projects that publish the thing and check what they download against
its published checksum or signature, and CI's actions and package
installs. All are
listed in `docs/Dependencies.md`.
