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

Early. Milestone M0 in progress.

| Component | State |
| --------- | ----- |
| portability layer | done |
| numeric conversion, exact shortest round-trip | done |
| public API header | next |
| key space, resolver, snapshot | next |
| codecs, sources, diagnostics | after M0 |

## Build

```sh
cmake --preset default && cmake --build --preset default && ctest --preset default
```

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
python3 tools/amalgamate/amalgamate.py . out
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

* [`docs/Design.md`](docs/Design.md) — architecture and specification
* [`docs/Decisions.md`](docs/Decisions.md) — every design decision, its reasoning and its cost
* [`docs/Doctor.md`](docs/Doctor.md) — the pre-deployment analysis tool

## Licence

TBD.
