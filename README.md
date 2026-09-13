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
| change notification | next |
| change notification | after |
| TOML, XML, SQLite codecs | deferred, see D-23 |
| Windows Registry, gsettings, macOS defaults | deferred, see D-30 |

Every line marked done is covered by the suite and by the gates below;
`cmake -P scripts/verify.cmake` is what says so.

## Build

```sh
cmake --preset default && cmake --build --preset default && ctest --preset default
```

Before a commit, everything:

```sh
cmake -P scripts/verify.cmake          # every preset
cmake -DMODE=quick -P scripts/verify.cmake
```

Written in CMake rather than a shell script so it runs wherever the
build does.

| Preset | What it is for |
| ------ | -------------- |
| `default` | C89 conformance, full warning set |
| `sanitize` | AddressSanitizer and UndefinedBehaviorSanitizer |
| `analyze` | GCC `-fanalyzer` symbolic execution, plus cppcheck when present |
| `portable` | every opportunistic upgrade disabled, output compared against `default` |
| `minimal` | `CF_WITH_REAL=OFF`, for targets with no floating point |

Options: `CF_WITH_INI` `CF_WITH_JSON` `CF_WITH_TOML` `CF_WITH_XML` `CF_WITH_SQLITE`
`CF_WITH_REAL` `CF_WITH_CXX` `CF_FORCE_PORTABLE` `CF_SINGLE_THREADED`.

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
| version | `cf_version.h` and `CMakeLists.txt` agree |
| format | `clang-format` and `cmake-format`, reporting rather than rewriting |
| cmake lint | `cmake-lint` over every build file |
| headers | every public header compiles standalone as C89 |
| golden | `explain()` output compared line by line against a fixed expectation |
| locale | the suite runs under a comma-decimal and a Turkish locale |

## Doctor

```sh
$ doctor lint staging.json
CFD001  error   user-file    supplies 2 value(s) and wins none; everything it
                             sets is shadowed, e.g. staging.ini:3:1

$ doctor diff prod.json staging.json
CFD002  warning server.port  same key resolved from 'env' here and 'cli' there
CFD014  info    db.password  sensitive value differs (by digest)
```

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
* [`docs/Design.md`](docs/Design.md) — the full specification
* [`docs/Decisions.md`](docs/Decisions.md) — every design decision, its reasoning and its cost
* [`docs/Doctor.md`](docs/Doctor.md) — the pre-deployment analysis tool

## Licence

TBD.
