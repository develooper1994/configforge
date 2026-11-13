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
13. [C++](#13-c), [Rust](#rust) and [Python](#python)
14. [Threads](#14-threads)

### The examples, by what they show

Each is a short program with the reasoning in its comments, built in
every build and run by the suite, which checks the lines that show it
does what its comment says. Where there is a `.cpp` beside the `.c`, it
is the same program through the C++ header, checked for the same
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
| `live.c`, `live.cpp`, `live.rs` | a gateway that reloads itself: applied, a port waiting for a restart, an operator's approval, a broken edit refused | 10 |
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

**No build system at all**: `scripts/generate/amalgamate.py` writes
`configforge/configforge.h` and `configforge.c`. Compile the one file
with the rest of your program.

The library is C89 and needs nothing but the C library. It is built and
tested with GCC, Clang and MSVC, against glibc and musl, and compiles
with the Tiny C Compiler and the Portable C Compiler. The build options are in the README's
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

One call does for every type (D-74), beside the typed ones:
`cf_builder_key` takes the type as a parameter, or `CF_TYPE_NULL` to let
the default's text decide, and C11 adds `CF_DEFINE`, which goes by the
default's C type. C11's `true` and `false` are the ints 1 and 0, so a
bool default is written `(bool)false`, and C11's `CF_DEFINE_INT` and
`CF_DEFINE_TEXT` take a range or length bounds, which the compiler checks
when they are constants (D-75).

```c
cf_builder_key(b, "server.port", CF_TYPE_NULL, "8080", "1", "65535", 0); /* an int */
cf_builder_key(b, "tls.ca", CF_TYPE_TEXT, NULL, NULL, NULL, 0);          /* no default */
CF_DEFINE(b, "server.host", "0.0.0.0");                      /* C11: text */
CF_DEFINE_INT(b, "server.port", 8080, 1, 65535, 0);  /* C11: checked */
```

C++ has `define("server.port", 8080, 1, 65535)`, Python
`define("server.port", 8080, min=1, max=65535)` and Rust
`def("server.port", 8080)`, each taking the type from the default; Python
and Rust pass through `cf_builder_key`, so what it refuses is refused there
too, at `create`.

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

A contract that already exists as a JSON Schema - the one an editor
reads, or a service validates the same file against - is read as
definitions instead of written again (D-64):

```c
cf_builder_schema_file(b, "myapp.schema.json");   /* or cf_builder_schema(b, text, n, name) */
```

Each property is a key; its type, range, length, `enum`, `default`,
`deprecated` and `writeOnly` (sensitive) are its definition; `required`
makes it `CF_REQUIRED`, an array of scalars is a list, and
`"additionalProperties": true` opens a prefix. It is the reverse of
`cf_schema` (section 10), and what `cf_schema` writes reads back as the
same contract. A keyword the library cannot enforce - `$ref`, `allOf`,
`pattern` and the rest - is refused by name and place, never dropped.

## 4. Sources and precedence

Every source has a rank, and the lowest rank wins. The library's own
ranks are `CF_RANK_CLI` (0), `CF_RANK_ENV` (10), `CF_RANK_USER_FILE`
(20), `CF_RANK_SYS_FILE` (30) and `CF_RANK_DEFAULT` (40); any integer
works, so a program places a source exactly where it belongs. Within one
source, a later assignment beats an earlier one.

| Call | Source |
| ---- | ------ |
| `cf_builder_cli` | `--key=value`, `--key value`, `--no-key`, aliases; `@file` only through `cf_builder_response_files` (below) |
| `cf_builder_env` | `PREFIX_A__B`, and `PREFIX_A_B` too once a definition makes it unambiguous |
| `cf_builder_file` | a file, its format named, by extension or by content (D-73); `CF_OPTIONAL` if it may be absent |
| `cf_builder_document` | a document the program holds in memory (fetched, embedded), its format named, by name or by content |
| `cf_builder_env_document` | a whole document in one environment variable (`APP_CONFIG='{"server": {"port": 8080}}'`), opt-in per variable, its format named or taken from the content; `CF_OPTIONAL` if it may be unset |
| `cf_builder_dir` | a drop-in directory: every file in byte order, at one rank; a name with no extension is read by its content, one with an extension no format claims refused |
| `cf_builder_dotenv` | a `.env` file, read as the environment is |
| `cf_builder_secrets` | a directory of secrets, one file a key, as containers mount them |
| `cf_builder_store` | a SQLite store (`CF_WITH_SQLITE`), the one source `cf_write` can keep a change in |
| `cf_builder_registry` | a key of the Windows Registry, a subkey a section: `Server\Port` is `server.port` |
| `cf_builder_memory` | pairs from the program itself |
| `cf_builder_replay` | a provenance report, reproducing another process's configuration |
| `cf_builder_source` | a source you wrote; `cf_builder_file_codec` for a format you parse |

**Response files are opt-in** (1.4.1, D-79). `myapp @args.txt --port=8080`
replaces `@args.txt` by the lines of that file, one argument per line, `#`
comments and blank lines skipped, no nesting - the way round the Windows
command-line length limit. It is off: `cf_app` and `cf_app_builder` treat
`@x` as the ordinary argument it is (a positional, which the command-line
source leaves alone), so an argument the program does not trust cannot
inject options or tell, by an error, which files exist. A program that
wants the feature says so, on the builder `cf_app_builder` made:

```c
cf_builder* b = cf_app_builder("myapp", argc, argv);
cf_builder_response_files(b);               /* before create */
cfg = cf_builder_create(b, &err);           /* a file it cannot read is reported here */
```

C++ has `responseFiles()`, Python `response_files()` and Rust
`response_files()`. `cf_app` never expands; a program that relied on it
before 1.4.1 moves to the three lines above.

`examples/c/layers.c` uses all of the usual ones at once - a shipped
`app.toml`, a `conf.d/`, a `.env`, the environment and the command line -
and asks, for one key from each layer, why it has the value it has.

`cf_builder_cli`, `cf_builder_env` and `cf_builder_memory` borrow the
command line, the environment and the pairs they are given, and read them
in `cf_builder_create`: they have to stay valid until it returns, and no
longer, because the configuration keeps its own copy of what it read.
Names, paths and prefixes are copied as the call is made.

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
The count is also where the array ends. Whichever source wins
`upstream.count` fixes the length, and an element at or past it - a
longer list in a file, under a shorter count from the command line - is
not in the configuration: it is not read, listed or written, and
`cf_explain` shows it as `truncated`, with the count that cut it. Below
the length each element is still the winner of its own key, so a source
that means to replace a list supplies its count with it. A count that is
not an integer, is negative, or is a `${...}` template fixes no length.
`cf_iter_keys(&it, cfg, "server")` walks every key under a prefix and
`cf_iter_children` the names one level down - which is how a program
discovers the plugins under an open prefix. Both go in byte order, where
`upstream.10` comes before `upstream.2`: an array is read with
`cf_array_count` and `cf_array_key`, which is in order.

### Declared scalar lists

For a list of scalars, declare the element contract once:

```c
cf_def tags;
cf_value initial;
cf_def_init(&tags, CF_TYPE_TEXT, "tags");
tags.list_mode = CF_LIST_APPEND;       /* or CF_LIST_REPLACE */
tags.list_separator = ',';
cf_value_text(&initial, "built-in", 8);
tags.has_default = CF_TRUE;
tags.list_defaults = &initial;
tags.list_default_count = 1;
cf_builder_define(b, &tags);
```

The definition and defaults are copied. Type, range, text-length limits,
allowed spellings, sensitivity and policy apply to every element.
`has_default` with a zero count declares an empty default. Read the
result with the existing `cf_array_count`, `cf_array_key` and getters;
there is no separate array value type.

`APP_TAGS=a,,b` supplies three elements, including the empty middle one,
when the environment or dotenv source uses prefix `APP_`, and so does
`tags = a,,b` in a file of a format that carries no types - INI,
`.properties` - whose values the definitions decide. A typed format keeps
its own arrays: a JSON string is a string. An empty
assignment supplies an empty list. A zero separator disables splitting;
there is no CSV quoting or trimming at the split step. Command-line
`--tags=a --tags=b` supplies two elements; aliases share the same list,
commas are literal and `--tags=` is one empty text element. Even boolean
lists take explicit values.

REPLACE uses the winning count and ordinary per-index resolution.
APPEND concatenates permitted source-local lists from lowest to highest
precedence, using the definition's policy order where present. Each
contributor supplies its own count; a source without one supplies
final-index patches instead. Count templates are refused in APPEND mode
because offsets must be known before references expand. References in
elements resolve against the final layout.

Declared lists require canonical decimal indices and a dense selected
range below their count; nested members and scalar root assignments
are refused. Undeclared arrays retain the earlier, looser rules.
Reports keep original assignments and locations, add `written_key` for
moved candidates, and mark aggregate counts `synthesized`. A declared
default's candidate says `declared_default`, which is how `doctor diff`
finds a default an upgrade changed (CFD017). Images and
full Report Replay are resolved bases, so reloading one does not append
the program's defaults again.

### Which keys the program reads

`cf_builder_track_reads(b)` makes the configuration record each key the
program asks for: through a `cf_get_*` or `cf_try_*` call, `cf_has` or
`cf_type_of`, even when the answer is the fallback. Explaining,
inspecting, iterating and reporting are not reads. It is off by default,
and then nothing about a read changes.

```c
cf_builder_track_reads(b);
cfg = cf_builder_create(b, &err);
/* ... the program runs ... */
if (!cf_was_read(cfg, "cache.size")) { /* set, and nothing uses it */ }
printf("%u of %u keys read\n", (unsigned)cf_read_count(cfg), (unsigned)cf_count(cfg));
```

Reads stay safe on any thread: each key's flag is atomic and written
once, the first time. Each snapshot records its own reads: a derived
one tracks from nothing, and a reload tracks when the builder its build
function returns asks to. A report from a tracking
configuration gives every key `"read": true` or `false`, a boolean even
in a blind report, so it shows configuration the program never used
without showing a value. A key nothing supplied and no definition names
is not in the configuration, so a read of one is not recorded. Doctor
reads it: CFD018 names each key a source set and the program never read.
The bindings have the same calls: `Builder.track_reads()`,
`Config.was_read(key)` and `Config.read_count` in Python;
`Builder::track_reads`, `Config::was_read` and `Config::read_count` in
Rust; `Builder::trackReads`, `Config::wasRead` and `Config::readCount`
in C++.

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
| JSON5 and JSONC (`.json5`, `.jsonc`) | `CF_WITH_JSON` | JSON with comments, trailing commas, unquoted names, single quotes, hexadecimal, Infinity and NaN (D-66); typed by the file, Infinity and NaN by the contract; written as JSON |
| TOML | `CF_WITH_TOML` | TOML 1.1.0, all of toml-test; dates and times, inf and nan read as their text, typed by the contract (D-62) |
| YAML | `CF_WITH_YAML` | YAML 1.2, all of yaml-test-suite, through the built-in parser (D-69); libfyaml 0.9.1+ or libyaml by `CF_YAML_BACKEND` (D-62); typed by the contract (D-39) |
| XML | `CF_WITH_XML`, off by default | through expat, read only; typed by the contract, mapped as D-23 declares |
| Java `.properties` | `CF_WITH_PROPERTIES` | read only; as java.util.Properties reads it, UTF-8; typed by the contract |
| CBOR | `CF_WITH_CBOR` | RFC 8949: maps, arrays, numbers, text, booleans and null, typed by the file; each value placed by its byte; byte strings and most tags refused by name (D-91) |
| TLV, by a layout | `CF_WITH_TLV` | read only; tag and length widths, byte order, an end tag, BER-TLV; each tag's key and type from the program's table (D-93) |

The format is the one you name, else the extension's, else - for a name
that gives none - the one the content reads as, decided by reading it and
never by a guess at its first bytes (D-73; `docs/Formats.md` has the
rules). A `.json` file that holds YAML is refused, and the error says the
content looks like yaml; a file with no extension is read as whichever
format reads it, or refused when none does or two loose ones read it
differently. `cf_source_file_format` says which was taken and how. A file
in a format the build left out is an error that names the option
to turn on - never a file quietly read as nothing. INI, JSON, TOML, YAML
and CBOR can also be written (section 10).

What kind of format a codec is, a program can ask. Its `caps` say text or
binary (`CF_CODEC_BINARY`), self-describing or bound to a schema
(`CF_CODEC_SCHEMA`: the bytes cannot be read without a layout the program
gives, as a TLV codec's), typed, with arrays, read, written. `cf_codec_kind`
says them in words, and `cf_codec_select` finds the built-in codecs by them
(D-94); `doctor formats` prints the table:

```c
const cf_codec* bin[8];
size_t n = cf_codec_select(CF_CODEC_BINARY, CF_CODEC_SCHEMA, bin, 8); /* cbor */
char kind[160];
cf_codec_kind(bin[0], kind, sizeof kind);
/* "binary, self-describing, typed, arrays, read and written" */
```

A format of your own is a `cf_codec` (`tests/unit/cf_kv_codec.h` is a
complete one, reading and writing). Hand it to one file with
`cf_builder_file_codec`, or register it on the builder (D-68):

```c
cf_builder_codec(b, &my_codec);                 /* before the files it reads */
cf_builder_file(b, "file", CF_RANK_USER_FILE, "app.kv", NULL, 0);  /* by extension */
cf_builder_dir(b, "conf.d", CF_RANK_USER_FILE, "conf.d", NULL, 0);  /* and in here */
cf_emit(c, "kv", CF_EMIT_ALL, 0, buf, sizeof buf, &err);            /* by name */
```

Its extensions and its name come ahead of the built-in codecs, so a
`.conf` file it names is read through it rather than as INI. It is
registered on the builder and not for the whole process, and a second
codec of the same name is refused. To measure it against its format's
own suite, as the built-in readers are, build it into a shared library
that exports a `const cf_codec* const` and run
`scripts/check/conformance.py <conform> <suites> --codec <library> <symbol> <format>`
(docs/Conformance.md).

### A TLV layout

A device that keeps its settings as tag, length, value follows no
standard but its maker's, so you describe its layout and the library
makes the codec (D-93; `examples/c/tlv.c` runs this one):

```c
static const cf_tlv_tag dev_tags[] = {              /* ascending by tag */
    {0x0001, "name",     CF_TLV_TEXT, 0, 0, 0, NULL, 0, NULL},
    {0x0002, "net.port", CF_TLV_UINT, 2, 0, 0, NULL, 0, NULL},
    {0x0003, "net.dhcp", CF_TLV_BOOL, 1, 0, 0, NULL, 0, NULL},
    {0x0010, NULL, CF_TLV_RECORD, 12, 0, CF_TLV_SKIP, NULL, 0, NULL}};
static cf_tlv_layout dev = CF_TLV_LAYOUT_INIT;
static cf_codec      devcfg;

dev.name = "devcfg";                  dev.order = CF_TLV_LITTLE;
dev.tag_width = 2;                    dev.length_width = 2;
dev.flags = CF_TLV_END;               dev.end_tag = 0xFFFF;
dev.tags = dev_tags;                  dev.ntags = 4;
cf_tlv_codec(&devcfg, &dev, &err);    /* the layout checked whole */
cf_builder_codec(b, &devcfg);
cf_builder_file(b, "device", CF_RANK_USER_FILE, "dev.cfg", "devcfg", 0);
```

`explain("net.port")` then says `dev.cfg, byte 12`: the byte its tag
starts at, which is what you look for in a hex dump. Each table lists its
tags in ascending order, each once, and the codec refuses a layout that
does not. A tag the layout does not name is refused by default - a
layout out of date says so the first time the file outgrows it; with
`CF_TLV_UNKNOWN_SKIP` it is passed over; with `CF_TLV_UNKNOWN_KEEP` it is
kept under `tag-` and its digits, as hex. A nest is a table, a tag
flagged `CF_TLV_LIST` an array, and `CF_TLV_BER` reads BER-TLV as ISO/IEC
7816-4 and EMV write it. The layout, its tables and the codec are yours
and must outlive the configurations read with them; `static` is the usual
way. A TLV codec is chosen by its name or its extensions, never from
content.

On YAML in particular: a plain value is text until a definition types
it, and the library has one spelling of a boolean, `true` and `false`,
so `country: no` stays the text "no" whichever YAML version a reader
follows. Anchors and aliases are read, and an aliased value explains
itself at the line it was written (D-63); the core schema's tags -
`!!str`, `!!int`, `!!float`, `!!bool`, `!!null` - type a value as YAML
says. A tag of the writing application's, a second document and a key
that is not a scalar are refused by name.

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

### systemd's credentials

A service given `LoadCredential=`, `SetCredential=` or
`ImportCredential=` finds each credential as a file in the directory
`$CREDENTIALS_DIRECTORY` names. `cf_builder_credentials` is the secrets
directory pointed there: the variable is looked up at every load, the
names map and the newline is dropped as above, and every value says it
came from systemd - `systemd credential db.password
(/run/credentials/app.service/db.password)` in explain() and in a report -
so it is never mistaken for a file somebody put in a directory. With
`CF_OPTIONAL` a program started outside systemd, where the variable is not
set, reads nothing from it; without, that is an error naming the
variable. A variable that is set must name a directory, optional or not
(D-80).

```ini
[Service]
ExecStart=/usr/bin/app
DynamicUser=yes
LoadCredential=db.password:/etc/app/db.password
SetCredential=api.region:eu-west-1
Restart=on-failure
```

```c
cf_builder_credentials(b, NULL, CF_RANK_ENV, "MYAPP_", CF_OPTIONAL);
```

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

What is written is the document the keys describe, and it reads back as
the same keys. A group of keys is written as an array when it is exactly
one: the indexes 0, 1, 2 and on with none missing, as the readers write
them, and a `count` that says how many there are, and nothing else. Its
members come out in index order, whatever they are. Any other group is a
table: `pool.count` and `pool.size` are a table with a member called
`count`, and so are indexes with one missing, or a `count` that does not
match them, written as the keys they are. The flat keys cannot tell two
pairs of documents apart, and write each as the array: `{"count": 0}` and
`[]`, and `{"0": "a", "count": 1}` and `["a"]`. An element at or past the
count is not in the configuration at all, and is not written either.

INI's arrays are the ones INI files already have - PHP's `parse_ini_file`
(D-60), not a syntax of this library's:

```ini
ports[] = 80          ; one element more
ports[] = 443
ports[1] = 8443       ; the element at index 1, as C assigns one
```

The `name[]` lines of one array go together, and a second run of them
further down the section is refused, since it would overwrite the
first. An array of values is written that way; an array of tables, of
arrays, or an empty one has no such form, and is refused by name before
anything is written - but only when what is being written has one: an
array that `CF_EMIT_CONFIGURED` or `CF_EMIT_FROM_RANK` leaves out does
not count.

What a format cannot hold is refused, never written as something else:
TOML has no null; YAML takes only valid UTF-8; INI has no line break
inside a value. A value INI would read differently bare - empty, with a
space at either end, or holding `"`, `#` or `;` - is written in quotes.

JSON and TOML string values are limited by the configuration's arena,
not `CF_MESSAGE_MAX`. Their readers use a reusable workspace owned by
the flattener; a fixed arena refuses exhaustion rather than falling
back to heap allocation. A custom document sink provides `reserve_text`
for decoded strings longer than 255 bytes, advertises its size, and
copies any text it needs to retain beyond the scalar callback.

A literal `${` in a resolved text value is written as `$${`, including
one supplied by a program's default. Loading the emitted configuration
undoes that escape exactly once, preserving the value. This is a
configuration-level rule: `cf_doc_emit` and the codecs themselves write
raw document values, as provenance reports require.

A key that is both a value and a container, `a` and `a.b`, remains legal
in the flat key space but cannot be written together. Emission refuses
it as `CF_E_KEY` and names `a`, rather than writing duplicate names.
Selecting only one side with `CF_EMIT_FROM_RANK` is still writable.

These correct three round-trip limitations present in the 1.0.0 release.

A document nests as deep as a reader takes, `CF_MAX_DEPTH` levels with
the document itself the first, which is tables and arrays 31 levels
inside it by default. Every format writes all of that, and a
configuration one level deeper is refused in every format in the same
words, `CF_E_KEY` and "the document nests deeper than 32 levels": a file
nested more deeply than that could not be read back. A refused `cf_emit`
leaves an empty string in the buffer, not the front of a document.

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
    /* or cf_diff_json: one JSON object a change, for a log pipeline */
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

A store that does not exist yet is created readable and writable by its
owner alone (mode 0600 on POSIX, a protected owner-only access control
list on Windows), whatever the umask says, because it may hold secrets.
A store that already exists keeps the mode it has.

For an APPEND list, `cf_write` and `cf_edit` address the final list:
updating `tags.2` may write `tags.0` in its contributing store. A store
without a count can instead set or delete final-index patches. Count
changes, deleting a contributing element, and patching another
contributor from a count-bearing store are explicitly refused before
any item is written. Structural changes need a complete source-local
update during provisioning; `cf_store_set` bypasses grants and
validation and is not an administration-page substitute for `cf_write`.

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

### Create, update, delete: one edit

`cf_write` writes a list of values. An administration page wants the
four verbs: read is the snapshot's, and `cf_edit` gathers the other
three into one change - set a key, set another, delete a third so the
file under the store supplies it again - and commits it whole:

```c
cf_edit* e = cf_edit_begin(cfg, "store", &err);
cf_edit_set_int(e, "pool.size", 32);
cf_edit_set_text(e, "log.level", "debug");
cf_edit_unset(e, "feature.beta");          /* the next source down supplies it */
if (!cf_edit_commit(e, "admin, 2026-10-30: incident 42", &err)) {
    /* nothing written; err names the key and why */
}
```

Commit asks what `cf_write` asks - can the source be written, may it,
and does every value meet the contract and its key's policy - for the
whole change, and writes it in one transaction or not at all. Under
`cf_live`, `cf_live_commit(live, e, note, &err)` commits and reloads in
one step, so the change reaches readers through the same build, check
and mode an edited file does (D-54).

### Reloading, in the mode the operators chose

The loop `reload.c` writes - notice, rebuild, check, swap without a
reader seeing it half done - is `cf_live` (D-48), in one of three
modes: `CF_LIVE_MANUAL` reloads only when asked (an admin command, a
SIGHUP), `CF_LIVE_APPROVE` builds a change and holds it until
`cf_live_approve`, and `CF_LIVE_AUTO` builds, checks and swaps. A build
or a check that fails leaves the running snapshot serving, and says
why. Readers call `cf_live_get` on any thread and keep the snapshot
they got; the library owns no thread - the program's loop calls
`cf_live_poll`, or `cf_live_wait` where it has no loop.

```c
static cf_builder* build(void* user) {
    cf_builder* b = cf_build(NULL);
    cf_builder_file(b, "gateway.ini", CF_RANK_USER_FILE, "gateway.ini", NULL, 0);
    cf_builder_int(b, "server.port", 8080, 1, 65535, CF_RESTART);
    cf_builder_text(b, "log.level", "info", 0);
    return b;
}

cf_live* live = cf_live_open(CF_LIVE_AUTO, build, preflight, NULL, NULL, &err);
```

A key defined `CF_RESTART` - a port with a bound socket, the size of an
allocated pool - cannot change live. Its new value does not reach
readers: the running value is kept as the top source, named
`running`, `cf_live_restarts` lists the key, and `explain()` shows
both (`examples/c/live.c`):

```text
server.port = 8080  (int)
  running     8080                  selected
  gateway.ini 9443                  shadowed   gateway.ini:2:1
  default     8080                  shadowed
```

A `CF_RESTART` scalar list is held whole when its count or any element
changes, even in APPEND mode or under a restrictive source policy.
The count and held members appear in `cf_live_restarts`; an initially
empty list stays empty. Held values retain their types and literal
reference text. Derived views preserve a held list; a fresh build after
restart accepts the newly configured list.

The check (`preflight` above) sees every snapshot before readers can,
the first included: a rule of the program's, or Doctor itself -
`cf_doctor_live_check` with a `cf_doctor_gate` lints each snapshot as
`doctor lint` would and refuses it on a finding at the gate's level, so
a password moved to the command line by an edit an hour after startup
is refused as it would have been at startup:

```c
static cf_doctor_gate gate = CF_DOCTOR_GATE_INIT;
cf_live*              live;

gate.profile = "production";
live = cf_live_open(CF_LIVE_AUTO, build, cf_doctor_live_check,
                    (void*)&gate, NULL, &err);
```

The gate is a struct the program fills, so it starts from its `_INIT`
macro, which sets the `size` that lets 1.x add a member (D-51); one that
did not is refused, by name.

#### On probation, and back

A check sees values; some mistakes only show in what the program does
with them - a rate limit that is valid and far too low, a pool that is
the right size for a database that is not there. With `probation` set
in `cf_live_options`, each swap keeps the snapshot it replaced for that
many polls. `cf_live_rollback` returns readers to it, as a new
generation with a `"reload-rolled-back"` log event carrying the reason;
a `health` function in the options, asked at each of those polls, does
the same by answering false. `cf_live_confirm`, or the last poll, ends
the probation. The unit is the program's own poll interval - the
library has no clock - so ten polls a second apart is ten seconds.

```c
cf_live_options o = CF_LIVE_OPTIONS_INIT;
o.mode      = CF_LIVE_AUTO;
o.build     = build;
o.probation = 3;
live = cf_live_open_with(&o, &err);
...
if (errors_doubled())
    cf_live_rollback(live, "errors doubled after the change", &err);
```

The file a rollback came from still says what it said, so a poll that
rebuilds it finds the values it rolled back from and stays where it is
until the file says something else; `cf_live_reload`, being asked for,
is a retry. A second swap within a probation keeps the snapshot that
was last confirmed, so a rollback never lands on one that never was.
Scene 4 of `examples/c/live.c` - and its C++ and Rust twins - runs it:

```text
4. reload on probation: swapped
serving on :8080, log info, 6 requests a minute
rolled back: errors doubled
serving on :8080, log info, 600 requests a minute
the same file again: unchanged
```

#### Letting a change settle

An editor's save and a deploy copying files one by one are several
writes, and a poll between two of them would read the configuration
half-way. `o.settle = 2;` reads a change two polls after it is first
seen, whatever else is written meanwhile, so the burst is read once, as
it was left. 0, the default, reads a change at the poll that sees it.

`rollback()` and `confirm()` in C++, Rust and Python; Python takes
`probation=` and `health=` (an exception's text is the reason), Rust
`LiveOptions::new().probation(n).health(..)`.

#### Which configuration was it?

An incident starts at 14:02; was that the reload at 14:01? A diff of
files cannot say, and a log of values must not. `cf_fingerprint(cfg)`
identifies the configuration a program runs on - FNV-1a over every key,
its type and its value, a secret counting only by the digest a
value-blind report shows - and `cf_fingerprint_format` writes it as
`fnv1a:` and sixteen hex digits. It is in every report as
`"fingerprint"`, in `cf_live`'s log events as `previous` and
`fingerprint` (the snapshot before and after; a reload's log line names
it too), and anywhere else the program puts it:

```c
char fp[24];
cf_fingerprint_format(cf_fingerprint(cfg), fp, sizeof fp);
log_info("serving config=%s", fp);                       /* a log field     */
otel_resource_set("configforge.fingerprint", fp);        /* a trace's resource */
```

With OpenTelemetry the attribute rides on every span the service
exports, so a trace view answers "which configuration served this
request" by itself; the library depends on no tracing SDK for it. It is
not cryptographic: it tells configurations apart and proves nothing,
and through it a guess at a secret can be checked no better than through
a blind report. `cf_digest`, which quiet swaps use, hashes values
themselves and is not for publishing. `fingerprint` in C++, Rust and
Python; Python's log records carry `cf_previous` and `cf_fingerprint`.

A health page asks `cf_live_status_get`, from any thread: the
generation serving, how many rebuilds were tried and failed, what the
last one did, whether a change waits for approval, and why the last
failure failed - "generation 7; the last edit was refused: CFD007 ..."
- without the program keeping a copy of its own. `status()` in C++ and
Rust, `live.status` in Python.

A Kubernetes ConfigMap mounted as a volume is updated the kubelet's
way - a new directory, and one rename of the `..data` link over the
old - and the file a program opens is a link that is never written.
`cf_live` sees it: the stamp it compares is the file's, through the
links, and the watcher's directory sees the rename (`test_live`). In C++
it is `cf::Live`, whose build under C++11 can be a lambda returning a
`cf::Builder`.

## 11. The editor, the report and Doctor

**The editor.** `cf_schema(cfg, "myapp", buf, cap)` writes the contract
as a JSON Schema. A settings file that names it - `"$schema":
"./myapp.schema.json"` - gets completion, descriptions and validation in
most editors with no plugin. `examples/c/schema.c`.

**The report.** `cf_report` writes everything the configuration knows:
every key, every candidate, where each came from, which won and why. Its
`cf_report_info` - the profile, the application, the time - starts from
`CF_REPORT_INFO_INIT`, or is `NULL` for a report that names nothing; one
that did not is refused, and nothing is written (D-51). A value-blind
report (the default) carries the presence, length and digest
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

`<configforge/configforge.hpp>` is one header over the same library,
from C++98 to C++23 (D-47): a `Builder` with chained calls, a
reference-counted `Config`, `get<T>`, ranges over keys and children, `explain()`, `emit()`,
`changesSince()`, `derive()` and `write()` for changing and keeping a
value, `define()` for a full `cf_def`, and a `Watcher`. Twelve of the
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

From C++11 a Key of a number or a bool can be a constant, and then the
compiler checks it by the library's rule (D-75): a name the library
would refuse, a range upside down or a default outside its range does
not compile, and the error names which. C++20's `constinit` asks the
same; `cf::key_is_valid` is the rule itself, in every standard:

```cpp
constexpr cf::Key<int> port("Server.Port", 8080, 1, 65535);
// error: call to non-'constexpr' function '... configforge::detail::
//        not_a_canonical_key_dotted_lowercase_ascii(const char*)'
static_assert(cf::key_is_valid("server.port"), "a canonical key");
```

A Key that is not a constant, a `Setting` and a `Binding` are checked
at `create`, as every key is in every standard.

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

### One header, every standard

`configforge.hpp` is the whole C++ interface, from C++98 to the newest
standard. The compiler's standard decides what it offers (D-47); a
program includes the one header and never says which. Support is
cumulative (D-72): C++98 is the floor and stays, and each standard adds
to the one before without changing it. `CF_CXX_LEVEL` says which layer
the program has: 98, 11, 17, 20 or 23.

| Standard | Adds |
| -------- | ---- |
| C++98 | everything above |
| C++11 | move, `explicit operator bool`, `derive("admin", {{"k", "v"}})`, `cf::Live` with lambdas |
| C++17 | `cf::find<T>` as `std::optional`, `cf::value` as a `std::variant`, `cf::items` for structured bindings, `cf::fields` over member pointers |
| C++20 | the `cf::readable` concept, `cf::Items` as a view, `std::format` of a `cf::Setting`, a `std::span` of pairs |
| C++23 | `cf::lookup<T>` as `std::expected` |

```cpp
#include <configforge/configforge.hpp>

std::optional<int> port = cf::find<int>(cfg, "server.port"); // nullopt, not a fallback
for (auto [key, value] : cf::items(cfg, "server") | std::views::take(3)) { /* ... */ }
std::puts(std::format("listening on {}", cfg["server.port"]).c_str());

inline const auto fields = cf::fields(
    cf::field<&Settings::server, &Server::port>("server.port", 8080, 1, 65535),
    cf::field<&Settings::tls>("tls.enabled", false));
auto settings = fields.read<Settings>(cf::Builder("myapp", argc, argv).define(fields).create());
```

The C++17 and C++20 parts turn on by what the standard library has,
not only by `__cplusplus`: a compiler that says C++20 with a library
that has no `<format>` (GCC 12) gets the C++17 part and still compiles.
`CF_CPP11`, `CF_CPP17`, `CF_CPP20` and `CF_CPP_EXPECTED` say which are
on. `find<T>` returns nothing for an integer `T` cannot hold, never a
truncated one. The header gate compiles the header standalone under
every standard the compiler knows.

### Rust

`bindings/rust` is a crate over the same library, built by cargo alone:
its `build.rs` compiles libconfigforge's C sources with the `cc` crate
and links them (D-70), so `cargo build` needs a C compiler and no
CMake. A `Builder` that
consumes itself into a `Result<Config, Error>`, reads that cannot fail,
`explain()`, `derive()`, and `Def` for a full definition. A `Config` is
`Send + Sync` - an immutable snapshot (section 14) - so a request can
hold one in an `Arc` while the next is derived.

Values are read as the type the program wants, and a default is written
once (D-46): `cfg.get::<u16>("server.port")` is `None` for a value a
`u16` cannot hold, never a truncated one; a `Key` is a constant that
defines and reads; and `settings!` declares the program's own struct,
whose field list is the contract:

```rust
const PORT: configforge::Key<u16> = configforge::Key::new("server.port", 8080).range(1, 65535);

configforge::settings! {
    struct Gateway {
        port: u16 = "server.port" in 1..=65535 with flag::RESTART => 8080,
        level: String = "log.level" => "info",
    }
}

let mut live = Live::open(Mode::Auto, || Gateway::define(Builder::new().file(/* ... */)))?;
let reader = live.reader();            // Send + Sync + Clone, for the workers
let gateway = Gateway::read(&reader.get());
live.poll()?;                          // in the loop that drives reloads
```

`Live` is `cf_live`: a reader keeps the snapshot it took, even past the
`Live`; a build that panics is a failed build, not an unwound C frame.
Seven of the examples are in `examples/rust`, the C programs saying the
same lines; `-DCF_RUST=ON` has the suite run them and `cargo test`.
systemd's credentials are `Builder::credentials(None, rank::ENV, None,
OPTIONAL)`: the directory `$CREDENTIALS_DIRECTORY` names, as above.

`cmake -DWORK=<new-directory> -P scripts/release/package_rust.cmake` makes a
standalone crate containing the native C sources and those examples,
then verifies and tests the unpacked package offline. The `.crate` is
under `WORK/target/package/`; nothing is published. Native copies are
made only for packaging, never maintained as another source tree.

### Python

`bindings/python` is a package over the shared library (D-49): ctypes,
the standard library and nothing else, typed (`py.typed`), packaged by
the root `pyproject.toml`. `python -m pip install .` builds and installs
a wheel with the shared library inside; `python -m build` creates a
source distribution and builds the wheel from that isolated source.
Both need CMake and a C compiler when building, neither when installing
an already-built wheel. A value is read the way Python reads - in Python,
unlike C++, `cfg.server.port` is possible:

```python
import dataclasses
import configforge as cf

cfg = (cf.Builder.app("myapp")
       .file("app.ini", cf.rank.USER_FILE, "app.ini", flags=cf.OPTIONAL)
       .define("server.port", 8080, min=1, max=65535)  # an int, from 8080
       .define("tls.ca", type=str)    # no default: the type is named
       .create())
cfg.server.port                 # attributes; a table is a node
cfg["server.port"]              # KeyError when absent
cfg.get("server.port", type=int)  # None when absent or another type

@dataclasses.dataclass
class Settings:
    port: int = cf.setting("server.port", 8080, range=(1, 65535), flags=cf.flag.RESTART)
    debug: bool = cf.setting("app.debug", False)

settings = cf.read(cf.define(cf.Builder.app("myapp"), Settings).create(), Settings)
```

`define` is one spelling for every scalar type (D-74): the type is the
default's, or the one named with `type=`, and the library checks the
rest, through `cf_builder_key`: a default of another type is refused at
`create`. `define_int`, `define_text`, `define_bool` and `define_real`
stay and mean the same.

systemd's credentials are `Builder.credentials(None, cf.rank.ENV,
flags=cf.OPTIONAL)`: the directory `$CREDENTIALS_DIRECTORY` names, one
file a key (section 9). A library older than this source refuses the
call by name, as `response_files` does.

A refusal raises `cf.Error` with the key, the source and the message;
`cf.Live` reloads, and a build that raises is reported with its own
exception as the `Error`'s cause. The library is found as
`$CONFIGFORGE_LIB`, beside the package, or on the library path; its
tests run against a shared build (`verify` with `EXTRA=shared`).
When the selected library lacks the read-tracking symbols, the rest of the
binding remains usable: `Builder.track_reads()` reports that limitation,
and the read-status queries return false or zero.
`configforge_doctor`, in the same package, drives a separately installed
Doctor (section 11). `python -I tests/python/test_package.py` checks an
installed wheel and runs the binding suite against its bundled library,
without importing from the source checkout.

### In C11 and later

A program compiled as C11 has one `cf_get`, typed by where the value
goes, and told apart from absent and wrong-typed by its result:

```c
int port;          /* CF_E_CONVERT for 5000000000: refused, not truncated */
bool tls;
const char* host;
if (cf_get(cfg, "server.port", &port) != CF_OK) { /* ... */ }
cf_get(cfg, "tls.enabled", &tls);
cf_get(cfg, "server.host", &host);
```

Under C23 (and C++17) the calls whose result is the only sign of
failure are `[[nodiscard]]`; a `(void)` cast says a result is ignored
on purpose. Nothing changes for a C89 program (D-53).

### What is worth a log line

An error stops a configuration; some things deserve a line in the
program's log without stopping anything. `cf_builder_log` gives the
library a function to call for them (`cf_log.h`): a key nobody defined,
under `CF_UNKNOWN_WARN`; a deprecated key still in use; a value refused
and fallen back from; and, through `cf_live_options.log`, each reload's
result and every key waiting for a restart. Each event has a level, a
stable name a log pipeline can match on (`"unknown-key"`, `"fallback"`,
`"reload-failed"`...), the key, the source, the error code where there
is one, and a sentence for a person. The library writes nowhere by
itself, and a secret's value is never in a message.

```c
static void to_log(void* user, const cf_log_event* e) {
    fprintf(stderr, "%s configforge %s %s: %s\n", cf_log_level_name(e->level),
            e->event, e->key ? e->key : "-", e->message);
}
cf_builder_log(b, to_log, NULL, CF_LOG_INFO);
```

In Python the levels are `logging`'s own numbers, so `Builder.log()`
and `Live(..., logger=...)` hand each event to a `logging.Logger` (the
`configforge` one by default) with `cf_event`, `cf_key`, `cf_source` and
`cf_code` on the record. Rust has `Builder::log(Level, closure)` and
`Live::open_logged`; C++ `Builder::log`.

## 14. Threads

A configuration is immutable once built, so any number of threads read
it without a lock. `cf_config_ref` and `cf_config_free` count
references atomically, so a reload can swap in a new snapshot while
readers finish with the old one; the last reader's `cf_config_free`
releases it. Change notification and reload are the program's to
schedule - the library starts no thread and calls nobody back. D-14 has
the full contract. Read tracking (`cf_builder_track_reads`, section 6)
keeps it: the one write a read makes is an atomic flag, set once.
