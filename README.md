# ConfigForge

Configuration resolution for C that answers **why** a value is what it is.

```c
#include <configforge/configforge.h>

int main(int argc, char** argv)
{
    cf_error    err = CF_ERROR_INIT;
    cf_config*  cfg;
    cf_i64      port;

    cfg = cf_app("myapp", argc, argv, &err);
    if (cfg == NULL) { fputs(err.message, stderr); return 1; }

    port = cf_get_int(cfg, "server.port", 8080);

    cf_config_free(cfg);
    return 0;
}
```

That is `examples/tier0.c`, compiled and run by the test suite.

```
$ MYAPP_SERVER__PORT=9090 ./tier1 --server.port=7000 --cf.explain=server.port
server.port = 7000  (int)
  cli         7000                  selected   argv[1]
  env         9090                  shadowed   MYAPP_SERVER__PORT
  default     8080                  shadowed

$ ./tier1 --server.port=99999
configuration error (validate): server.port: 99999 is outside 1..65535
  set at: argv[1]

$ ./tier1 --server.pot=1
configuration error (key): unknown key 'server.pot'; did you mean 'server.port'?
  set at: argv[1]
```

Those are `examples/tier1.c`. Both the success and the failure paths are
run by the test suite, so the output above cannot drift from the code.

### The rest of a setting's life

Reading a value is the first half. `examples/settings.c` is the second:
change one, save it, and notice the file moving underneath you.

```
$ settings
0.0.0.0:8080  log=info tls=off

$ settings --set=server.port=9000
saved settings.json
0.0.0.0:9000  log=info tls=off

$ cat settings.json
{
  "server": {
    "port": 9000
  }
}
```

The file holds what somebody configured and nothing else, so a later
release can still move a default the user never touched.

```
$ settings --explain=server.port
server.port = 9000  (int)
  settings    9000                  selected   settings.json:3:13
  default     8080                  shadowed
  cli         not present
  env         not present

$ settings --set=server.port=99999
settings: server.port: 99999 is outside 1..65535
```

Changing a setting derives a new configuration rather than altering the
old one, and the contract travels with it — so a value the original
would have refused is refused before anything is written. The old
snapshot stays valid, which is what lets another thread go on reading
it without a lock.

```
$ settings --watch
watching 1 file(s)
  settings.json
changed: no
after an edit, changed: yes
reloaded, changed: no
```

Change notification is a question you ask, never a callback: a library
that owns a thread and calls you back has made a threading decision that
belongs to you. Every line above is produced by the test suite, so none
of it can drift from the code.

A program that already has an event loop can wait instead of polling:

```c
cf_watcher* w = cf_watcher_open(cfg, NULL, &err);
/* add cf_watcher_fd(w) - or cf_watcher_handle(w) on Windows - to the loop */

cf_watcher_drain(w);            /* when it is ready */
if (cf_changed(cfg)) { /* rebuild, swap, reopen the watcher */ }
```

It watches each file's directory, because editors save by renaming a
new file over the old one, so a wake-up means "ask now" and `cf_changed`
stays the answer. inotify on Linux, kqueue on macOS and the BSDs, one
event HANDLE on Windows; anywhere else opening it says so, and polling
works as before.

## Why

`get()` is easy. Precedence is easy. What nobody in C answers is which source won and
why. .NET has `GetDebugView()`, Spring Boot has `/actuator/env`, Rust has `figment`,
Git has `config --show-origin`. C has nothing.

## Design

* **C89 baseline**, opportunistic upgrades where the toolchain offers them
* **Flat sorted key space**, five value types; structure is a key naming convention
* **Provenance retained per key** — `get()` is the winner of the chain, not a separate path
* **One error site** — construction is fallible, reads are not
* **The declaration decides the type** — inference runs only where no contract exists
* **Serialization layer** shared by configuration files, provenance reports and replay
* **Binary snapshot image** that loads without parsing or allocating, optionally
  carrying the provenance chain so a device with no filesystem can still explain itself
* **Report Replay** — a captured production report loads as a source, reproducing
  exactly the configuration a process was running
* **Zero mandatory dependencies** in the default build

## Status

Working, and not finished. The interface is not frozen: `0.x` promises
nothing, and `1.0` is where semantic versioning starts.

| Component | State |
| --------- | ----- |
| built and tested with GCC, Clang and MSVC; against glibc and musl; with tcc and pcc | done |
| portability layer, numeric conversion with exact shortest round-trip | done |
| value model, flat key space, provenance chain, resolver | done |
| definitions, validation, `cf_config` facade | done |
| `explain()` and `inspect()` | done |
| serialization layer: blob, sink, codecs, document events | done |
| INI and JSON codecs; file, `.env` and drop-in directory sources | done |
| command line: `--key=value`, `--key value`, `--no-key`, aliases, `@file` | done |
| environment with both spellings; UTF-8 arguments on Windows | done |
| provenance report, schema v1, and Report Replay | done |
| binary snapshot image, optionally carrying provenance | done |
| prefix iteration, children, array helpers | done |
| C++03 wrapper; amalgamation; ConfigForge Doctor | done |
| per-parameter policy (D-33) | done |
| writing an overrides file (`cf_emit`) | done |
| threading contract (D-14), reference-counted snapshots | done |
| `cf_derive`: changing a value without mutating a snapshot | done |
| change notification (`cf_changed`) | done |
| a handle an event loop can wait on (`cf_watcher`): inotify, kqueue, Windows | done |
| Doctor: SARIF, `--fail-on`, per-project and per-profile severity | done |
| TOML, as the D-23 subset: tables, arrays of tables, inline tables, every scalar but dates | done |
| a codec defined in the program, not the library (`cf_builder_file_codec`) | done |
| what a reload changed, and where each new value came from (`cf_diff`) | done |
| the definitions as a JSON Schema for editors (`cf_schema`) | done |
| limits and the stack budget as build options, measured per feature (D-37) | done |
| parts a program can leave out, each refusing by name (D-38) | done |
| secrets kept out of diagnostics and out of freed memory | done |
| YAML through libyaml, typed by the contract, behind `CF_WITH_YAML` (D-39) | done |
| XML, SQLite codecs | deferred, see D-23 |
| Windows Registry, gsettings, macOS defaults | deferred, see D-30 |

Every line marked done is covered by the suite and by the gates below;
`cmake -P scripts/verify.cmake` is what says so.

## Build

```sh
cmake --preset default && cmake --build --preset default && ctest --preset default
```

Before a commit, everything:

```sh
cmake -P scripts/format.cmake                    # format and lint what changed
cmake -DSOURCE=index -P scripts/verify.cmake     # every preset, on what is staged
cmake -DMODE=quick -P scripts/verify.cmake       # the default preset only
```

Written in CMake rather than a shell script so they run wherever the
build does. `scripts/README.md` lists the options.

| Preset | What it is for |
| ------ | -------------- |
| `default` | C89 conformance, full warning set |
| `sanitize` | AddressSanitizer and UndefinedBehaviorSanitizer |
| `analyze` | GCC `-fanalyzer` symbolic execution, plus cppcheck when present |
| `portable` | every opportunistic upgrade disabled, output compared against `default`; built `-O2` |
| `tsan` | the thread sanitiser over the whole suite (`EXTRA=tsan` in `verify.cmake`) |
| `fuzz` | libFuzzer on every reader of untrusted bytes, with ASan and UBSan; clang (`EXTRA=fuzz`) |
| `coverage` | line coverage of the library, held above a floor (`EXTRA=coverage`) |
| `minimal` | every optional part off, short limits (64-byte keys) and a 1024-byte stack budget, for very small targets; built `-Os` |

The last two are optimised on purpose: inlining merges frames, so a stack
frame that fits in a Debug build can break the budget in a release one.

Options: `CF_WITH_INI` `CF_WITH_JSON` `CF_WITH_TOML` `CF_WITH_XML` `CF_WITH_SQLITE`
`CF_WITH_REAL` `CF_WITH_DIR` `CF_WITH_DOTENV` `CF_WITH_EMIT` `CF_WITH_IMAGE`
`CF_WITH_REPORT` `CF_WITH_CXX` `CF_FORCE_PORTABLE` `CF_SINGLE_THREADED`, and the limits
below.

### Leaving parts out

What a program does not use, it need not carry. Each part below is on
by default; turned off, its functions remain - a program links against
any build - and each one refuses by name instead of doing nothing, so a
drop-in directory a build cannot read never looks like an empty one
(D-38).

| Option | What goes | Code it saves (x86-64, `-Os`) |
| ------ | --------- | ----------------------------- |
| `CF_WITH_TOML` | the TOML codec | 8.3 KB |
| `CF_WITH_EMIT` | `cf_emit` and every encoder | 6.7 KB |
| `CF_WITH_REAL` | floating point and its exact formatter | 5.9 KB |
| `CF_WITH_REPORT` | the provenance report, Report Replay, the JSON Schema writer - and Doctor | 5.5 KB |
| `CF_WITH_IMAGE` | the binary snapshot image | 4.1 KB |
| `CF_WITH_INI` | the INI codec | 2.9 KB |
| `CF_WITH_DOTENV` | the `.env` source | 1.3 KB |
| `CF_WITH_DIR` | the drop-in directory source | 1.1 KB |

All of them together take the library from 92 KB to 58 KB. A static
link with `--gc-sections` already drops functions a program never
calls; the options are for what it cannot drop - a shared library, the
amalgamation, and code reached through a table, as every codec is.
`cf_has_feature()` and every provenance report say which parts a build
has.

### Limits and the stack budget

Every limit is a fixed buffer, and every buffer is stack, so how long a
key may be and how much stack a function may take are one trade - and
it is yours to make (D-37):

| Option | Default | What it bounds, and what it costs |
| ------ | ------- | --------------------------------- |
| `CF_KEY_MAX` | 256 | longest key; held by every error and by the flattener and decoders |
| `CF_MESSAGE_MAX` | 256 | longest error message; every `cf_error` holds one |
| `CF_PATH_MAX` | 512 | longest file path; the directory source holds several |
| `CF_MAX_DEPTH` | 32 | deepest nesting; the decoders recurse once per level |
| `CF_STACK_BUDGET` | 2048 | bytes a library function's frame may take; `0` measures without checking |

```sh
cmake -B build -DCF_KEY_MAX=64 -DCF_MESSAGE_MAX=128 -DCF_PATH_MAX=256 -DCF_STACK_BUDGET=1024
```

The build measures every frame with the compiler's own figures and
checks it against the budget after the library links:

```text
stack: largest frame 1792 bytes (cf_gen_digits), within 2048
```

The full table is in `build/configforge-stack.txt`. When a frame does
not fit, the build lists every one that does not, by how much, and the
feature it belongs to - so the choice is between named options, not a
guess. To ask "what if" without rebuilding:

```sh
cmake -DOBJDIR=build/CMakeFiles/configforge.dir -DBUDGET=1024 -P scripts/stack.cmake
```

At the default limits the largest frame is the exact float formatter's
(`CF_WITH_REAL`), and the largest the library always has is about 1.3 KB.
Without floating point and with 64-byte keys every function fits in a
kilobyte, which the `minimal` preset checks on every verify.

A frame is one link of a call chain: a thread also needs room for
`CF_MAX_DEPTH` levels of a decoder's recursion. GCC and Clang report
frame sizes; MSVC does not, and there the budget is not checked.

### From a package manager

```sh
conan create . --build=missing -tf packaging/conan/test_package
vcpkg install configforge --overlay-ports=packaging/vcpkg
```

Both build from this checkout and are tested against it in CI: Conan
with every `CF_WITH_*` as a recipe option (`with_yaml` brings libyaml
from ConanCenter), vcpkg with a `yaml` feature. Either way the package
carries the library and its headers, and `find_package(ConfigForge)`
gives `ConfigForge::configforge`.

### From a build that is not CMake

The install carries a relocatable `configforge.pc`:

```sh
cc app.c $(pkg-config --cflags --libs configforge)
```

### Without a build system

```sh
python3 scripts/amalgamate.py . out
cc -std=c89 -Iout out/configforge.c myapp.c
```

Two files, any compiler, no `make` and no CMake. Generated from the real
sources, and the amalgamation gate compiles and runs it on every commit,
so the fallback is tested rather than merely present. There used to be a
hand-written `Makefile` for this; it needed updating in step with
`CMakeLists.txt` eight times and was never run by anyone. See `D-31`.

## Gates

The build enforces what the design claims, because a claim the build does not check is
a claim that decays.

| Gate | Checks |
| ---- | ------ |
| conformance | `-std=c89 -pedantic -Werror` |
| portability | full suite under `CF_FORCE_PORTABLE`, output compared byte for byte |
| layering | `src/core` includes nothing from `sources`, `platform` or `serialize` |
| detection | only `cf_port.h` and `port.c` may test compiler macros |
| symbols | no locale-sensitive libc function; every export prefixed `cf_` |
| amalgamation | the single-file build compiles as C89 and runs |
| interface | what including the library costs: no unexpected system header, no size jump |
| install | a separate project finds the package, compiles against it and runs |
| formats | every format string against what `cf_fmt` supports |
| version | `cf_version.h` and `CMakeLists.txt` agree |
| format | `clang-format` and `cmake-format`, reporting rather than rewriting |
| cmake lint | `cmake-lint` over every build file |
| headers | every public header compiles standalone as C89 |
| golden | `explain()` output compared line by line against a fixed expectation |
| locale | the suite runs under a comma-decimal and a Turkish locale |

The gates that drive the compiler with a GCC-style command line - headers,
amalgamation, install, interface - run under GCC and Clang. They are
about the sources rather than the compiler, and MSVC has no C89 mode to
ask for; under MSVC the library and every test are built at `/W4 /WX` and
the suite runs, which is the check that compiler can give.

Beyond the gates, `test_fuzz` corrupts valid documents and requires every
reader to return rather than crash. It runs one fixed seed in the suite,
so a failure is reproducible, and fresh seeds under the sanitiser in CI.
It found an out-of-bounds read in the binary image reader on its first
run.

`test_starve` does the same for the resource dimension: it runs the whole
pipeline in a fixed buffer, shrinking it byte by byte, and requires that
every size either works correctly or fails with a reason. It found a
window where construction succeeded and returned an empty
configuration.

## Doctor

```sh
$ doctor lint staging.json
CFD001  error   server.port  app.ini:2:1 always loses to app.ini:9:1, set later
                             by the same source; editing it has no effect
CFD010  warning user-file    supplies 2 value(s) and wins none; everything it
                             sets is shadowed, e.g. staging.ini:3:1
CFD007  error   api.token    sensitive, and supplied on the command line at
                             argv[1], which every user of the machine can read

$ doctor diff prod.json staging.json
CFD002  warning server.port  same key resolved from 'env' here and 'cli' there
CFD014  info    db.password  sensitive value differs (by digest)
```

```sh
$ doctor lint prod.json --baseline .configforge-baseline --fail-on warning
CFD008  warning log.level    no source configured this; the default applies

1 new finding(s): 0 error, 1 warning, 0 info; 1 accepted by the baseline

$ doctor lint prod.json --format sarif > doctor.sarif   # for the pull request
```

A baseline records what is already there as accepted, one line per
finding, so only new ones fail. The commonest way a lint tool dies is
two hundred findings on its first run against a repository that already
exists: nobody fixes two hundred things, so everybody turns the tool off
and the fifteen that mattered go with the rest. An entry that no longer
matches anything is reported — a rule that stopped firing is either a
fix worth noticing or a rule that quietly broke — but it does not fail
the build, because failing would make people stop keeping the file
rather than start pruning it.

Severity is the project's to choose, in a file of the same shape:
`CFD010 error`, `CFD011 off`, or `CFD008 error production` for one
profile. That last form is what profiles are here - a label on the
report that a rule's severity can follow, not a concept in the library.

Reports do not have to come from this library. `doctor import-git` turns
`git config --list --show-origin --show-scope` into one, and every
command above runs on it unchanged - a scope that wins nothing is
CFD010 there too.

Doctor reads provenance reports and never resolves configuration itself,
which is what keeps it from becoming a second implementation of every
ecosystem's precedence rules. Reports can be value-blind: presence,
origin, length and a digest, which is everything an audit needs and
nothing a leak would want.

## Documentation

* [`docs/Decisions.md`](docs/Decisions.md) — every decision, the options weighed, and what
  settled it. Two are still open, and both say why
* [`docs/Architecture.md`](docs/Architecture.md) — the layers, the rules that keep them, and recipes for adding a format, a source, a definition attribute or a Doctor rule
* [`docs/Conventions.md`](docs/Conventions.md) — naming, memory, errors, project idioms, the forbidden list, and what a good test looks like here
* [`docs/Guide.md`](docs/Guide.md) — **start here**: from the first line of code to a device with no heap, every program it quotes run by the suite
* [`docs/API.md`](docs/API.md) — every function, with the comment above it in its header; generated, and checked against the headers by `gate.apiref`
* [`docs/Design.md`](docs/Design.md) — the full specification
* [`docs/Doctor.md`](docs/Doctor.md) — the pre-deployment analysis tool
* [`docs/Roadmap.md`](docs/Roadmap.md) — what is next, what is known to be wrong, and the ideas not yet built
* [`docs/Dependencies.md`](docs/Dependencies.md) — every dependency and its licence; the build generates an SPDX SBOM to match

## Contributing

Start with [`AGENTS.md`](AGENTS.md) - written for whoever continues the
work, a person or an AI agent - then [`CONTRIBUTING.md`](CONTRIBUTING.md).
Security reports go privately, as [`SECURITY.md`](SECURITY.md) describes.

## Licence

zlib - see [`LICENSE`](LICENSE). Chosen for where this library is meant
to run: a binary built with it, firmware included, owes nobody a notice,
while every source distribution keeps one - which is why the single-file
amalgamation carries the licence at its top.
