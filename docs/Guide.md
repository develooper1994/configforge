# Using ConfigForge

A guide from the first line of code to a configuration on a device with
no heap. Every program it quotes is in `examples/` - C in `examples/c`, C++ in
`examples/cpp`, Rust in `examples/rust` - and is built and run
by the test suite, so what you read here is what the library does today.
The reference for each function is [`API.md`](API.md); the reasons are in
[`Decisions.md`](Decisions.md).

1. [Getting it into a build](#1-getting-it-into-a-build)
2. [The first program](#2-the-first-program)
3. [A contract](#3-a-contract)
4. [Sources and precedence](#4-sources-and-precedence)
5. [Asking why](#5-asking-why)
6. [Reading values](#6-reading-values)
7. [Errors](#7-errors)
8. [Formats](#8-formats)
9. [Secrets](#9-secrets)
10. [Changing, saving, reloading](#10-changing-saving-reloading)
11. [The editor, the report and Doctor](#11-the-editor-the-report-and-doctor)
12. [Small targets](#12-small-targets)
13. [C++](#13-c), and [Rust](#rust)
14. [Threads](#14-threads)

### The examples, by what they show

Each is a short program with the reasoning in its comments, built in
every build and run by the suite, which checks the lines that show it
does what its comment says. Where there is a `.cpp` beside the `.c`, it
is the same program through the C++03 wrapper, checked for the same
lines.

| Example | What it shows | Section |
| ------- | ------------- | ------- |
| `tier0.c`, `tier0.cpp`, `tier0.rs` | the smallest complete program: command line, environment, defaults | 2 |
| `tier1.c`, `tier1.cpp` | the same with a contract | 3, 13 |
| `layers.c`, `layers.cpp`, `layers.rs` | six sources at once, and why each value won | 4 |
| `explain_demo.c` | `explain()` on one key | 5 |
| `references.c`, `references.cpp`, `references.rs` | one value made from others, `${key}`, and a cycle refused | 5 |
| `plugins.c` | a strict contract with one prefix handed to plugins, discovered by iteration; lists | 6 |
| `mistakes.c`, `mistakes.cpp`, `mistakes.rs` | what the person running the program sees for each kind of mistake | 7 |
| `secrets.c` | a password only the environment may set, never printed, never in a report, wiped | 9 |
| `settings.c` | read, explain, set, save, notice the file change | 10 |
| `runtime.c`, `runtime.cpp`, `runtime.rs` | an admin changing a setting in a running service: readers unaffected, the change explained | 10 |
| `reload.c`, `reload.cpp` | watch a file, rebuild, log what changed | 10 |
| `schema.c` | the contract as a JSON Schema for editors | 11 |
| `preflight.c`, `preflight.cpp` | a service that runs Doctor's rules on itself before it serves, and refuses a password typed as a flag | 11 |
| `policy.c`, `policy.cpp` | a Windows editor: the user's Registry settings, the administrator's policy over them, one key only policy may set | 4 |
| `flags.c`, `flags.cpp`, `flags.rs` | feature flags shipped off, a canary drop-in turning some on, an environment kill switch, the flags discovered by iteration | 6 |
| `migration.c`, `migration.cpp` | a key renamed in version 2, and the files written for version 1 still working | 3 |
| `embedded.c` | a fixed buffer and an image, no heap, still explaining itself | 12 |

## 1. Getting it into a build

Pick whichever your project already uses; each one gives you the same
library and the same header, `<configforge/configforge.h>`.

**CMake, from source**, inside your tree or fetched:

```cmake
add_subdirectory(third_party/configforge)            # or FetchContent
target_link_libraries(app PRIVATE ConfigForge::configforge)
```

**CMake, installed**: `find_package(ConfigForge REQUIRED)` and the same
target.

**Not CMake**: the install carries a relocatable pkg-config file.

```sh
cc app.c $(pkg-config --cflags --libs configforge)
```

**A package manager**:

```sh
conan create . --build=missing -tf packaging/conan/test_package
vcpkg install configforge --overlay-ports=packaging/vcpkg
```

**No build system at all**: `scripts/amalgamate.py` writes
`configforge/configforge.h` and `configforge.c`. Compile the one file
with the rest of your program.

The library is C89 and needs nothing but the C library. It is built and
tested with GCC, Clang and MSVC, against glibc and musl, and compiles
with the Tiny C Compiler. The build options are in the README's
"Leaving parts out" and "Limits and the stack budget" sections.

## 2. The first program

`examples/c/tier0.c`:

```c
cf_error   err = CF_ERROR_INIT;
cf_config* cfg = cf_app("myapp", argc, argv, &err);
if (cfg == NULL) { fputs(err.message, stderr); return 1; }

port = cf_get_int(cfg, "server.port", 8080);
cf_config_free(cfg);
```

`cf_app` reads the command line and the environment, in that order of
precedence, above the defaults you give at each read:

```sh
MYAPP_SERVER__PORT=9090 ./app            # environment: double underscore is a dot
./app --server.port=7000                 # command line beats environment
./app --server.port 7000                 # the same, once there is a contract (3)
```

Keys are lowercase, dotted paths: `server.port`, `upstream.1.host`. A
key is the same key in every source - that is what makes "which source
won" a question with one answer.

## 3. A contract

Tier 0 infers types and accepts anything. A contract - a definition for
each key the program reads - buys typo detection, ranges, permitted
values, required keys and a type that beats inference, and it does not
add a single place where things can fail: everything is checked in
`cf_builder_create`. `examples/c/tier1.c`:

```c
cf_builder* b = cf_app_builder("myapp", argc, argv);

cf_builder_int (b, "server.port", 8080, 1, 65535, 0);
cf_builder_text(b, "server.host", "0.0.0.0", 0);
cf_builder_bool(b, "tls.enabled", CF_FALSE, 0);
cf_builder_unknown(b, CF_UNKNOWN_REJECT);   /* a typo is an error */

cfg = cf_builder_create(b, &err);           /* the one place it can fail */
```

```text
$ ./tier1 --server.port=99999
configuration error (validate): server.port: 99999 is outside 1..65535
$ ./tier1 --server.pot=1
configuration error (key): unknown key 'server.pot'; did you mean 'server.port'?
```

The flags on a definition: `CF_REQUIRED` (a source must supply it - a
default does not count), `CF_SENSITIVE` (never shown, section 9),
`CF_DEPRECATED`, and `CF_TRANSIENT` for a command such as `--explain`
that must never be saved into a file. For anything the convenience
calls do not cover - a permitted set, a description, a policy saying
which sources may set the key - fill a `cf_def` and pass it to
`cf_builder_define`; `examples/c/tier1.c` does it for `log.level`.

A key that is renamed keeps its old definition, `CF_DEPRECATED`, with
`replaced_by` naming the new key: a value written under the old name
then configures the new one - below the new spelling in the same file,
by ordinary precedence across sources - and `explain()` places it
`app.ini:2:1 (as db.host)`, the line somebody has to update. Doctor's
CFD009 names it too. `examples/c/migration.c`, and its C++ twin.

A prefix you cannot know in advance - a plugin's settings - is exempted
from unknown-key rejection with `cf_builder_open(b, "plugins")`.

## 4. Sources and precedence

Every source has a rank, and the lowest rank wins. The library's own
ranks are `CF_RANK_CLI` (0), `CF_RANK_ENV` (10), `CF_RANK_USER_FILE`
(20), `CF_RANK_SYS_FILE` (30) and `CF_RANK_DEFAULT` (40); any integer
works, so a program places a source exactly where it belongs. Within one
source, a later assignment beats an earlier one.

| Call | Source |
| ---- | ------ |
| `cf_builder_cli` | `--key=value`, `--key value`, `--no-key`, aliases, `@file` |
| `cf_builder_env` | `PREFIX_A__B`, and `PREFIX_A_B` too once a definition makes it unambiguous |
| `cf_builder_file` | a file, its format by extension; `CF_OPTIONAL` if it may be absent |
| `cf_builder_dir` | a drop-in directory: every file in byte order, at one rank |
| `cf_builder_dotenv` | a `.env` file, read as the environment is |
| `cf_builder_secrets` | a directory of secrets, one file a key, as containers mount them |
| `cf_builder_store` | a SQLite store (`CF_WITH_SQLITE`), the one source `cf_write` can keep a change in |
| `cf_builder_registry` | a key of the Windows Registry, a subkey a section: `Server\Port` is `server.port` |
| `cf_builder_memory` | pairs from the program itself |
| `cf_builder_replay` | a provenance report, reproducing another process's configuration |
| `cf_builder_source` | a source you wrote; `cf_builder_file_codec` for a format you parse |

`examples/c/layers.c` uses all of the usual ones at once - a shipped
`app.toml`, a `conf.d/`, a `.env`, the environment and the command line -
and asks, for one key from each layer, why it has the value it has.

## 5. Asking why

This is the question the library exists to answer:

```c
cf_explain(cfg, "server.workers", buf, sizeof buf);
```

```text
server.workers = 12  (int)
  conf.d      12                    selected   conf.d/90-local.ini:3:1
  conf.d      8                     shadowed   conf.d/10-site.ini:3:1
  app.toml    4                     shadowed   app.toml:5:11
  default     2                     shadowed
  .env        not present
  env         not present
  cli         not present

db.url = <sensitive, 26 bytes>  (text)
  .env        <sensitive, 26 bytes> selected   .env:2
  app.toml    <sensitive, 24 bytes> shadowed   app.toml:8:7
  default     <sensitive, 13 bytes> shadowed
  conf.d      not present
  env         not present
  cli         not present
```

Every candidate, from every source, in precedence order, with the file,
line and column it came from; and every source that had nothing, which
is how somebody learns which flag they could have passed. `cf_inspect`
does the same for every key and adds a note for a source that supplied
values and won none of them - the file somebody is editing to no effect.

A common pattern is a transient `--cf.explain=<key>` definition and a
check after `cf_builder_create`, as `examples/c/tier1.c` does.

For a machine - an administration endpoint, the way Spring's
`/actuator/env` works - `cf_explain_json(cfg, key, CF_REPORT_BLIND, buf,
cap)` writes the same chain as JSON: exactly the object the provenance
report holds for that key, so whatever reads one reads the other, and
blind, so a sensitive key shows where it came from and its digest,
never its value.

### One value made from others

`${key}` in a value somebody wrote - a file, the environment, the command
line - is the value of another key:

```ini
[server]
host = web1
port = 9443
url  = https://${server.host}:${server.port}/
```

Precedence chooses every key first, so `--server.host=web2` changes the
URL too; then the references are replaced, and the result is typed by
the contract. `explain()` shows what it was made from
(`examples/c/references.c`, and its C++ twin):

```text
server.url = "https://web1:9443/"  (text)
  app.ini     "https://web1:9443/"  selected, from "https://${server.host}:${server.port}/"   app.ini:4:1
  cli         not present
  default     not present
```

A reference to a key with no value, a cycle, and a plain key showing a
`CF_SENSITIVE` one are refused at construction, naming the key. `$${`
is a literal `${`, and `${HOME}`, which names no key, is refused rather
than read as a directory called "${HOME}". A program's own default is
never read for references.

## 6. Reading values

```c
cf_i64      port = cf_get_int (cfg, "server.port", 8080);   /* fallback if absent */
const char* host = cf_get_text(cfg, "server.host", "");
double      r    = cf_get_real(cfg, "ratio", 0.5);
cf_bool     tls  = cf_get_bool(cfg, "tls.enabled", CF_FALSE);
```

Once a key is defined, reading it cannot fail: the value was checked
when the configuration was built. `cf_try_int` and its siblings return a
`cf_code` for the rare caller that wants to tell "absent" from "wrong
type". `cf_has` and `cf_type_of` answer the obvious questions.

Arrays are index keys and a count: `upstream.0.host`, `upstream.1.host`,
with `cf_array_count(cfg, "upstream")` and `cf_array_key` to build a key.
`cf_iter_keys(&it, cfg, "server")` walks every key under a prefix and
`cf_iter_children` the names one level down - which is how a program
discovers the plugins under an open prefix.

## 7. Errors

A `cf_error` is a plain struct the caller owns, filled by the call that
failed: `code` (`cf_code_name` names it), `message`, and `key` and
`source` where they are known - the key, and where it was set
(`argv[1]`, `app.ini:3:8`). No global error state, no allocation, so it
works in the fixed-arena profile where allocation is exactly what fails.

The codes: `key` (malformed or unknown key), `definition`, `convert`,
`validate`, `required`, `source` (a source could not be read),
`capability` (a source cannot do what was asked - a format this build
left out, say), `policy` and `memory`.

What the person running the program reads, from `examples/c/mistakes.c`,
which builds one contract from five files with one mistake each:

```text
2. a key spelt wrong
  refused: unknown key 'server.pot'; did you mean 'server.port'?

3. a number out of range
  refused: server.port: 99999 is outside 1..65535

4. a word the contract does not know
  refused: log.level: not one of debug info warn error

5. a required setting left out
  refused: 'db.url' is required and has no value
```

One key can be told to forgive: `on_invalid = CF_INVALID_FALLBACK` on a
tuning knob such as a cache size, so nonsense in the file falls back to
the next candidate down - and the refusal stays in `explain()`:

```text
cache.size = 64  (int)
  file        "lots"                rejected: "lots" is not a valid int   mistakes.ini:2:1
  default     64                    selected
```

Everything else stays strict, which is the default for a reason: a
production database address that quietly became a development default
is worse than a program that refuses to start.

## 8. Formats

| Format | Option | Notes |
| ------ | ------ | ----- |
| INI | `CF_WITH_INI` | sections and keys; typed by the contract |
| JSON | `CF_WITH_JSON` | typed by the file |
| TOML | `CF_WITH_TOML` | the subset in D-23: tables, arrays of tables, inline tables, every scalar but dates |
| YAML | `CF_WITH_YAML`, off by default | through libyaml; typed by the contract (D-39) |
| XML | `CF_WITH_XML`, off by default | through expat, read only; typed by the contract, mapped as D-23 declares |
| Java `.properties` | `CF_WITH_PROPERTIES` | read only; as java.util.Properties reads it, UTF-8; typed by the contract |

The format is taken from the extension, never guessed from the content.
A file in a format the build left out is an error that names the option
to turn on - never a file quietly read as nothing. All four can also be
written (section 10), and a format of your own is a `cf_codec` handed to
`cf_builder_file_codec`; `tests/unit/test_codec.c` is a complete one.

On YAML in particular: a plain value is text until a definition types
it, and the library has one spelling of a boolean, `true` and `false`,
so `country: no` stays the text "no" whichever YAML version a reader
follows. Anchors, aliases and tags are refused by name.

## 9. Secrets

Define a key `CF_SENSITIVE` and its value never appears in a diagnostic:
`explain()`, `inspect()`, `cf_diff_text`, a validation message and a
value-blind report all show that it exists, where it was set and how
long it is - `<sensitive, 13 bytes>` - and not what it is. A per-key
policy, `{ "env", NULL }`, says the environment is the only place it may
come from; a password someone committed to the file is then refused and
shown as refused, which is how it gets noticed (`examples/c/secrets.c`):

```text
db.password = <sensitive, 13 bytes>  (text)
  env         <sensitive, 13 bytes> selected   SECRETS_DB__PASSWORD
  file        <sensitive, 21 bytes> rejected: policy: 'db.password' may only be set from env   secrets.ini:3:1
```

Where secrets are handed over as files - Docker and Kubernetes mount
them in a directory, systemd passes credentials in
`$CREDENTIALS_DIRECTORY` - `cf_builder_secrets` reads the directory one
file a key, so a secret never passes through the environment, where
every child process can read it. `db.password` is `db.password`, and a
file named as its variable would be (`MYAPP_DB__PASSWORD`, with the
prefix `MYAPP_`) maps as the environment's does; the trailing newline
`echo` leaves is dropped, and explain() names the file:

```c
cf_builder_secrets(b, "secrets", CF_RANK_ENV, "/run/secrets", "MYAPP_", CF_OPTIONAL);
```

The memory a
configuration releases is zeroed first, so a password does not survive
into the next allocation or a core dump; for a buffer you own,
`cf_arena_wipe` does the same. Doctor reports a secret passed on the
command line, where `ps` shows it to everyone (CFD007).

## 10. Changing, saving, reloading

A configuration never changes once built. Changing a value derives a new
one, under the same contract, with the change recorded as a source of
its own:

```c
static const cf_pair change[] = {{"server.port", "9000"}};
cf_config* next = cf_derive(cfg, "settings", change, 1, &err);
```

Writing what somebody configured - and not the defaults, which a later
release may want to move - is `cf_emit(cfg, "json", CF_EMIT_CONFIGURED,
0, buf, cap, &err)`. `examples/c/settings.c` is the whole cycle: read,
explain, set, save, notice the file change.

Noticing a change is a question you ask, never a callback - the library
owns no thread. `cf_changed(cfg)` asks whether a file it read has moved
on. A program with an event loop opens a `cf_watcher` and waits on its
descriptor (`cf_watcher_fd`, or `cf_watcher_handle` on Windows); one
without calls `cf_watcher_wait(w, timeout_ms)`. Then it rebuilds from the
same recipe, logs what changed, and swaps. `examples/c/reload.c`:

```c
next = load(&err);
if (next != NULL) {
    cf_diff_text(cfg, next, changes, sizeof changes);   /* the log line */
    cf_config_free(cfg);
    cfg = next;
}
```

```text
reloaded:
db.password: changed, <sensitive> (file reload.toml:6:12)
pool.size: 4 -> 16 (file reload.toml:4:8)
server.port: 8080 -> 9443 (file reload.toml:2:8)
```

A reload whose new file is broken fails to build, and the program keeps
the configuration it has.

A change that has to survive a restart goes to a **store**, never into a
hand-written file: hand-written files keep their comments and layout
because nothing but a person writes them. `cf_builder_store` reads a
SQLite store (`CF_WITH_SQLITE`) as a source, and `cf_write` writes typed
values to it in one transaction - every item or none - with a note of
who, when and why, shown by `explain()` as where the value was set:

```c
cf_builder_store(b, "store", CF_RANK_USER_FILE, "app.db", CF_OPTIONAL);
cf_builder_writable(b, "store");     /* every source is read only until granted */
cfg = cf_builder_create(b, &err);
...
cf_write_item item;
item.key = "log.level";
cf_value_text(&item.value, "debug", 5);
if (!cf_write(cfg, "store", &item, 1, "admin, 2026-10-24: incident 42", &err)) {
    /* not written, and err says which question said no */
}
```

Before anything is written, `cf_write` asks whether the source can be
written (only a store can), whether the program granted it, and whether
the contract would accept each value from it - the same rules that read
it back, so `"verbose"` for a level that allows `debug, info, warn` is
refused, and so is any key whose policy says it may only come from the
environment. `cf_store_set` writes a store without asking, for
provisioning one before the program runs.

```text
log.level = "debug"  (text)
  store       "debug"               selected   app.db: admin, 2026-10-24: incident 42
```

A change made at run time - an operator raising the log level from an
admin page - is the same `cf_derive`. A request already in flight keeps
the snapshot it started with, the change meets the same contract a file
would (0 workers is refused, and the running configuration is left as it
was), and the next morning `explain()` says who did it and what it was
before (`examples/c/runtime.c`):

```text
log.level = "debug"  (text)
  admin       "debug"               selected
  service.ini "info"                shadowed   service.ini:2:1
  default     "warn"                shadowed
```

## 11. The editor, the report and Doctor

**The editor.** `cf_schema(cfg, "myapp", buf, cap)` writes the contract
as a JSON Schema. A settings file that names it - `"$schema":
"./myapp.schema.json"` - gets completion, descriptions and validation in
most editors with no plugin. `examples/c/schema.c`.

**The report.** `cf_report` writes everything the configuration knows:
every key, every candidate, where each came from, which won and why. A
value-blind report (the default) carries the presence, length and digest
of a sensitive value and never the value; a full one can be replayed
with `cf_builder_replay` to reproduce, on a developer's machine, exactly
what a production process was running.

**Doctor** (`tools/doctor`) reads reports and finds the mistakes no
single process can see: a value set twice in one file, a source that
never wins, a secret on the command line, two environments that drifted
apart. `doctor lint`, `doctor diff`, `doctor explain`; SARIF for code
scanning; a severity file and a baseline so a project can adopt it
without fixing everything on day one; and a GitHub Action. See
[`Doctor.md`](Doctor.md).

**As a library.** The rules are `libconfigforge_doctor` (`cf_doctor.h`,
`ConfigForge::configforge_doctor`), of which `doctor` is the command
line. A service can lint its own configuration before it serves - and
refuse to start in production with a password that came from the
command line - and a test can assert there are no findings, in process
and with no report on disk:

```c
cf_doctor* d = cf_doctor_new();
cf_doctor_lint(d, cfg, "production", &err);
if (cf_doctor_count_at(d, "error") > 0) {
    const cf_finding* f = cf_doctor_finding(d, 0);
    fprintf(stderr, "%s %s: %s\n", f->rule, f->key, f->message);
    return 1;
}
cf_doctor_free(d);
```

Baselines, severities, `cf_doctor_diff` and the rule catalogue are
there too. It is a library of its own, and static, so a device that only
reads configuration carries none of it.

## 12. Small targets

Everything above works with no heap. Hand the builder a buffer:

```c
static char buf[16384];
cf_arena*   arena = cf_arena_fixed(buf, sizeof buf);
cf_builder* b     = cf_build_in(arena);
```

and every allocation comes out of it, or fails with `memory` and a
message. With no filesystem, resolve the configuration at build time and
ship it as a binary snapshot image - `cf_image_write` on the build
machine, a const array in the firmware, `cf_source_image_mem` on the
device - which is read in place, without parsing, and still explains
itself, down to the line on the build machine:

```text
radio.channel = 11  (int)
  firmware    11                    selected   release.ini:2:1
  default     1                     shadowed
```

`examples/c/embedded.c` is both halves.

For such a target:

* turn off what it does not use - `CF_WITH_DIR`, `CF_WITH_DOTENV`,
  `CF_WITH_EMIT`, `CF_WITH_REPORT`, even `CF_WITH_REAL` - and each part
  you leave out refuses by name if something still asks for it;
* shorten the limits (`CF_KEY_MAX`, `CF_MESSAGE_MAX`, `CF_PATH_MAX`,
  `CF_MAX_DEPTH`) and set `CF_STACK_BUDGET`: the build measures every
  function's frame and says which ones do not fit and what to turn off;
  the `minimal` preset holds the library to a kilobyte per function;
* `CF_PLATFORM=none` for a target with no operating system, and
  `CF_SINGLE_THREADED` where there are no threads (or no atomics).

## 13. C++

`<configforge/configforge.hpp>` is a C++03 wrapper over the same
library: a `Builder` with chained calls, a reference-counted `Config`,
`get<T>`, ranges over keys and children, `explain()`, `emit()`,
`changesSince()`, `derive()` and `write()` for changing and keeping a
value, `define()` for a full `cf_def`, and a `Watcher`. Six of the
examples are in `examples/cpp` as well as `examples/c`, the same
programs saying the same lines. `examples/cpp/tier1.cpp`:

```cpp
cf::Config cfg = cf::Builder("myapp", argc, argv)
                     .defineInt("server.port", 8080, 1, 65535)
                     .unknown(CF_UNKNOWN_REJECT)
                     .create();
if (!cfg) { std::fputs(cfg.error().message(), stderr); return 1; }
int port = cfg.get<int>("server.port", 8080);
```

That writes the default twice, once in the contract and once at the
read. Three forms write it once (D-46). A declared key is both:

```cpp
static const cf::Key<int> port("server.port", 8080, 1, 65535);
cf::Config cfg = cf::Builder("myapp", argc, argv).define(port).create();
int p = cfg[port];
```

A key by name, a part at a time, typed where it is converted; a
defined key has its default, so there is no fallback to pass, and
`exists()` tells a missing key from a zero:

```cpp
int         p    = cfg["server"]["port"];
std::string host = cfg["server.host"];
if (cfg["tls"]["enabled"]) { /* ... */ }
```

And the program's own struct, whose field list is the contract:

```cpp
struct Server   { std::string host; int port; };
struct Settings { Server server; bool tls; };

cf::Binding<Settings> bind;
bind.field("server.host", &Settings::server, &Server::host, "0.0.0.0")
    .field("server.port", &Settings::server, &Server::port, 8080, 1, 65535)
    .field("tls.enabled", &Settings::tls, false);
cf::Config cfg      = cf::Builder("myapp", argc, argv).define(bind).create();
Settings   settings = bind.read(cfg);   /* settings.server.port */
```

`cfg.server.port` on the configuration itself is not possible in
standard C++: that would need generated code, and code generated into
`cf::Config` gives one type two meanings in two programs. The dot in
`settings.server.port` belongs to the program, which is where C++
allows it.

### Rust

`bindings/rust` is a crate over the same library, with no dependencies:
its `build.rs` builds libconfigforge from the checkout with CMake and
links it, so `cargo build --offline` downloads nothing. A `Builder` that
consumes itself into a `Result<Config, Error>`, reads that cannot fail,
`explain()`, `derive()`, and `Def` for a full definition. A `Config` is
`Send + Sync` - an immutable snapshot (section 14) - so a request can
hold one in an `Arc` while the next is derived. Five of the examples are
in `examples/rust`, the C programs saying the same lines; `-DCF_RUST=ON`
has the suite run them under cargo.

```rust
let cfg = configforge::Builder::app("myapp")
    .define_int("server.port", 8080, 1, 65535, 0)
    .unknown(configforge::Unknown::Reject)
    .create()?;
let port = cfg.int("server.port", 8080);
```

## 14. Threads

A configuration is immutable once built, so any number of threads read
it without a lock. `cf_config_ref` and `cf_config_free` count
references atomically, so a reload can swap in a new snapshot while
readers finish with the old one; the last reader's `cf_config_free`
releases it. Change notification and reload are the program's to
schedule - the library starts no thread and calls nobody back. D-14 has
the full contract.
