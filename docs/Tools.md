# The tools

ConfigForge ships programs as well as a library, and one name starts all
of them: `configforge <command>` runs `configforge-<command>` with the rest
of the arguments, as `git <command>` runs `git-<command>`.

```text
configforge doctor lint report.json     runs configforge-doctor lint report.json
configforge help                        the list
configforge --version                   ConfigForge 1.7.0
```

## The front door

**The rule.** `argv[1]` is the command. A name is 1 to 64 bytes of `a-z`,
`0-9` and `-`, and does not start with `-`; anything else is refused
(`'../x' is not a command name`), so no path and no option is ever run.
The program is `configforge-<name>` (`configforge-<name>.exe` on Windows).
It is found beside `configforge` first, then on the `PATH`; never in the
current directory, unless the `PATH` names it. A program of your own,
`configforge-<name>` on the `PATH`, is run the same way the shipped ones
are.

**The help.** `configforge help`, `--help` or `-h` alone prints it on
stdout and exits 0; no argument prints it on stderr and exits 2.
`configforge --version` alone prints `ConfigForge <version>`.

**Exit codes.** The program's own, when it ran. 2 when the front door
refused the command line, when there is no such program (`'x' is not a
configforge command`), when it could not be started (`cannot run`), or
on out of memory. 127 and 126, which shells use for these, are not used:
Doctor and the other tools answer 2 to unusable input, so a CI step that
runs `configforge doctor lint` has one meaning for "it did not run".

**Where the programs are.** In a build tree every tool is in
`tools/bin/` (below `Debug/` or `Release/` for a multi-config generator);
in an install, in `bin/`. `cmake --install` installs the front door and the
tools in one directory, and the release archives carry them there. An
installed shared build finds `libconfigforge` by its run path
(`$ORIGIN/../lib`, `@loader_path/../lib` on macOS); on Windows the DLL is
in `bin/` beside them. The vcpkg, Conan and Python builds install no
tools.

**Windows.** Everything is UTF-16: the program's path is found with
`GetModuleFileNameW`, the arguments are read by `CommandLineToArgvW` and
written again by the rule it reads back, so an argument outside the code
page, with spaces, quotes or a trailing backslash, reaches the program
whole. Ctrl+C is the program's to answer: the front door waits and returns
the program's exit code.

**Where there are no processes.** WASI and `CF_PLATFORM=none` have no
`exec`: the front door is not built there (CMake says so).

## The tools

| Program | What it does | Page |
| --- | --- | --- |
| `configforge-doctor` | Reads provenance reports: `explain`, `dump`, `lint`, `diff`, `history`, `blame`, `import-*`. | [Doctor](Doctor.md) |
| `configforge-codec` | The serialization library's own program: `formats`, `detect`, `check`, `dump`, `convert` and `format`, on documents and never on a configuration. | [below](#configforge-codec) |
| `configforge-gen` | The contract's own program: `configforge gen --schema` writes a JSON Schema contract as C, `configforge gen check` holds files to it, `configforge gen schema --from` drafts one from a sample file. | [below](#configforge-gen) |

## configforge codec

The serialization library's own program (`configforge-codec`), linked to
`configforge_serialize` and to nothing else: it reads documents, in every
format the build has, and never a configuration - no contract, no key
space, no layers. A document read wrong is a value explained wrong; which
format a file is, and whether it is one at all, are questions about bytes,
and this program answers them with the reader's own answer. It is built
wherever tools are, with no option of its own.

```text
configforge codec formats [--format text|markdown]
                               the built-in codecs and their kind
configforge codec detect <file>
                               the format of a file, and how it was told
configforge codec check <file>... [--as <format>]
                               whether each file is well formed
configforge codec dump <file> [--as <format>]
                               every value and table, with its place
configforge codec convert <file> --to <format> [--as <format>] [--out <file>] [--strict]
                               the document in another format, and what it cannot hold
configforge codec format <file> [--as <format>] [--out <file>] [--strict]
                               the document rewritten in its own format
configforge codec --version
```

Exit codes: 0 done (`check`: every file is well formed), 1 a file is not,
or its format cannot be told; 2 unusable input. Anything else is the usage
on stderr, exit 2.

**formats.** Every built-in codec: its name, its extensions and what kind
of format it is (text or binary, self-describing or schema-bound, typed,
arrays, read and written). `--format markdown` is the table in
docs/Formats.md, which a test holds the page to. It was `doctor formats`
until 1.7.x.

**detect.** `<name> (<how>)` and a newline, `how` being `extension` when
the file's name told the format and `content` when its bytes did
(`cf_codec_detect`, D-73). A file whose bytes no format reads, or that two
loose formats read differently, is exit 1 (`cannot tell the format of
<file>; name it with --as`): the bytes are the answer, not the command
line.

**check.** Each file is read by its format and decoded into a sink that
keeps nothing. It prints `<file>: ok`, or `<file>: ` and the reader's
message, one line each; the exit code is the worst. `--as <format>` names
the format for every file instead of detecting it; a format this build
left out says which option to turn on, and one no codec is called is exit
2. `-` is standard input. A TLV codec is not reachable: it needs a layout.

**dump.** One line for each event of the document, fields separated by a
tab, printed as the reader gives them - nothing is kept, so a large
document costs its own bytes and the path. A table or array opened is `-`,
its path (`.` for the document itself) and `table` or `array`. A value is
its place, its path, its type and its text, and ` (textual)` after the
text when the reader said the value was untyped text. The place is the one
`explain()` would name - `app.toml:3:5` for text, `app.cbor, byte 12` for
CBOR - so what the dump shows is what a report will say; a reader that
gives none is `-`. The path is the names joined by `.`, an array's members
by their index from 0; one too long for its buffer ends in `...`. A text
is quoted, with `\`, `"` and every byte below 0x20 or 0x7f written as
`\\`, `\"`, `\n`, `\r`, `\t` or `\xNN`, so nothing a file holds starts a
line of its own in a CI log. Exit 0 dumped; 1 the format cannot be told,
or the reader refused the bytes (its message on stderr, after what was
printed); 2 unusable input. A TLV codec is not reachable (no layout), and
the image (`cf_image`) is ConfigForge's own format, read by
`libconfigforge` and not by this program.

```text
$ configforge codec dump app.json
-       .       table
-       s       table
app.json:1:13   s.p     int     8080
-       t       array
app.json:1:26   t.0     text    "x"
app.json:1:43   u       null    null
```

### Converting between formats

`configforge codec convert app.ini --to toml --out app.toml` reads a
document through its codec and writes it through another's encoder, as the
codecs' events: nesting and arrays kept, no key space and no contract on
the way (D-94). `--as` names the input's format, as for `dump`; without
`--out` the output is standard output, binary on Windows too, so a CBOR
file piped on is its bytes. `format` is `convert` into the document's own
format.

What the target cannot hold is a finding on stderr, with its path, and is
left out; nothing is dropped quietly. `--strict` makes any finding exit 1
and writes nothing:

```text
$ configforge codec convert n.json --to toml
codec convert: a: a null, which toml cannot hold; left out
```

Two notes are said once and are not findings, since they are about the
formats and not the file, and a `--strict` that failed on them would fail
every conversion out of TOML: that the source's format can hold comments,
which no reader keeps, and that an untyped source's values are written to
a typed target as text (INI's `port = 8080` is the JSON string `"8080"`).
A target with no types written values that were numbers, booleans or
nulls as their text is a finding, since a type was lost. A `${` is
written as it was read, neither expanded nor doubled: a document is
converted, not a configuration. A name met again in a table is the same
member: a table reopened is one table, and anything else is replaced by
the later one. Comments are lost. Exit 0 converted; 1 `--strict` and a
finding, or a format that cannot be told; 2 unusable input; 3 an encoder
refused what the check let through (a bug).

## configforge gen

The contract's program (`configforge-gen`). A JSON Schema can be the
contract itself (`cf_builder_schema_file`, D-64): the keys, their types,
ranges, defaults and spellings, written once and read by the program, by an
editor and by these two commands, which use it without the program (D-67).
It is built wherever Doctor is, and not where tools are off (WASI,
`CF_PLATFORM=none`).

```text
configforge gen --schema <schema> [--lang c|cpp|rust|python] [--name <name>] [--out <file>]
                               the contract as code, C by default
configforge gen check --schema <schema> <file>...
                               files against the contract
configforge gen schema --from <file> [--as <format>] [--title <t>] [--out <file>]
                               a draft contract from a sample file
configforge gen --version
```

Exit codes: 0 done, and for `check` every file holds to the contract; 1 a
file does not; 2 unusable input. `doctor check` and `doctor gen` of earlier
releases are these two commands; Doctor stopped answering them in 1.8.0
(Doctor.md, section 7).

`configforge gen check --schema app.schema.json app.toml [local.toml ...]`
reads the files as the program would - in that order, each overriding the
ones before - against the contract, unknown keys refused outside a prefix
the schema opens. It prints `ok` and exits 0, or prints the first refusal,
with the key and the reason, and exits 1; a schema it cannot read is
unusable input, exit 2. A required key is required of the files: check the
files that together make a complete configuration. In CI or a pre-commit
hook it catches a misspelt key or a port out of range before a deploy does:

```text
$ configforge gen check --schema app.schema.json app.json
unknown key 'server.prot'; did you mean 'server.port'?
```

`configforge gen --schema app.schema.json --name app_contract --out
app_contract.c` writes one C89 function, `cf_bool app_contract(cf_builder*
b)`, that defines every key with `cf_def` and `cf_builder_define` and opens
what the schema opens. A build without the JSON reader - an embedded one,
`CF_WITH_JSON=OFF` - then has the same contract from the same file, and the
schema stays the one place it is written. Generate it in the build rather
than checking it in; the function reads back through `cf_schema` as the
schema it came from, which `tests/gen/test_gen.c` holds it to. A generated
file says in its first comment that `configforge gen --schema` wrote it.

The generated file checks itself where the compiler can (D-75): compiled as
C++11 or later each key is checked by `cf::key_is_valid`, as C11 a key's
length and every int default and text default against its bounds, so a build
with a smaller `CF_KEY_MAX`, or a schema whose default its own bounds refuse,
does not compile. As C89 it is plain calls, checked by `cf_builder_create`.

`--lang` names the language: `c` (the default, what is described above),
`cpp`, `rust` and `python`; the program's table of languages holds them, and a
name it does not hold is refused with the names it does (`gen: no language 'x';
there are: c, cpp, rust, python`, exit 2). `--name` is what the language calls
the generated thing - for C the function, for C++ the namespace (both
`cf_contract` unless named), for Rust and Python the struct (`Settings` unless
named) - and must be an identifier of the language.

`--lang cpp` writes one header. It holds the same calls as the C file, whole
- descriptions, enumerations, lengths, lists, aliases, policies and the
opened prefixes, which a `cf::Key` has no place for - in `inline bool
define(cf_builder*)`, so it needs no JSON reader either; and, where a program
reads, two things that cannot drift from them. `keys` has a `cf::Key` for each
key, named by its text with `.` and `-` as `_` (`server.max-conn` is
`server_max_conn`; a leading digit gets `k_`, a reserved word `_` after it),
its default the schema's. A struct is made of the keys' shape: a key of one
segment is a field of `Settings`, one of two or more a field of its top
segment's struct (`server.tls.cert` is `Server::tls_cert`), in the order the
keys come, and `bind` fills them through a `cf::Binding`. A list has no
field, and says so in a comment where its field would be; two keys that come
to one name are refused (`gen: 'a.b' and 'a_b' are both 'a_b' in cpp; rename
one`). A `Binding` reaches a field through two levels at most, which is why
the shape stops there. The struct is `Settings` whatever `--name` is.

```cpp
#include "app_contract.hpp"   /* configforge gen --schema app.schema.json
                                 --lang cpp --name app --out app_contract.hpp */

cf::Config cfg = cf::Builder()
                     .define(app::Contract())
                     .file("app", CF_RANK_USER_FILE, "app.json")
                     .create();
long port = (long)cfg.get(app::keys::server_port);

cf::Binding<app::Settings> b;
app::bind(b);
app::Settings s = b.read(cfg);          /* s.server.port, s.server.host ... */
```

A key's range and flags are the definition's, not the `Key`'s: a `Key` here
only names a key and reads it. The header compiles as C++98 and later; from
C++11 it is checked at compile time as the C file is (D-75), and
`tests/gen/test_gen_cpp.cpp` builds it from the test contract and holds it
to the schema.

`--lang rust` writes one module, and defines the contract with the schema
itself: the schema's text is in the file as a string (`SCHEMA`), and
`contract(Builder) -> Builder` gives it to `Builder::schema`, which the
library reads as it would read the schema file - descriptions, enumerations,
lengths, lists, reals, deprecations and opened prefixes, by the library's one
rule set (D-67). The Rust crate's own defining calls carry a type, a default,
a range and flags and nothing else, so a contract written through them would
lose the rest. What the file adds is read only: a `Key` constant for each key
(`SERVER_PORT: Key<i64>`, named by the key's identifier in uppercase, its
default the schema's or the type's zero) and one flat `settings!` struct,
`Settings`, a field for each key (`server_port`), because `settings!` does
not nest. A bool is `bool`, an int `i64`, a text `String`; a real, which the
binding has no `Field` for yet, and a list say so in a comment where their
constant would be. The identifier rule is the C++ one, with Rust's reserved
words. The file has no inner attribute or doc comment, so it can be a module
or be `include!`d.

```rust
mod app_contract { include!("app_contract.rs"); }   // configforge gen --schema
                                                    // app.schema.json --lang rust
let cfg = app_contract::contract(Builder::app("app")).create()?;
let port = cfg.read(&app_contract::SERVER_PORT);
let s = app_contract::Settings::read(&cfg);          // s.server_port ...
```

`tests/gen/contract.rs` is the module the test contract gives, checked in:
`gen.rust` holds the program's output to it byte for byte, so a change to the
writer shows as a diff of that file, and the Rust crate compiles and runs it
(`bindings/rust/tests/gen_contract.rs`, under cargo).

`--lang python` writes one module, as Rust's does: the schema's text is in the
file as `SCHEMA`, one literal a line, and `contract(b)` gives it to
`Builder.schema`, so the library reads every rule the schema holds. What the
file adds only reads: a dataclass for each top segment of the keys and one,
`Settings`, for the module - deeper keys are flattened into their class by
their identifier, a list or a type Python does not read is a comment where its
field would be, and a class comes before the one that uses it. Each field is a
`cf.setting(key, default, ...)` of its type (`bool`, `int`, `float`, `str`),
with `range` the schema's, `doc` its description and `flags` the definition's
(`cf.flag.REQUIRED`, `SENSITIVE`, `DEPRECATED`, `TRANSIENT`, `RESTART`);
nothing reads those when the contract is the schema, but a `SENSITIVE` field
stays out of the dataclass's `repr`. The program reads with `cf.read(cfg,
Settings)` and does not also call `cf.define`: the keys are defined already.
The identifier rule is Python's, with its reserved words, the module's own
names and `bool`, `float`, `int` and `str`; the file is UTF-8.

```python
import app_contract                      # configforge gen --schema app.schema.json
                                         #   --lang python --out app_contract.py
cfg = app_contract.contract(cf.Builder.app("app")).create()
s = cf.read(cfg, app_contract.Settings)   # s.server.port ...
```

`tests/gen/test_gen_python.py` imports the module the test contract gives, by
its path, and reads a configuration through it (`gen.python`, in a shared
build with a Python).

`configforge gen schema --from app.toml --out app.schema.json` drafts a
contract from a file you already have, for a program with configuration and
no contract yet. Every value the file holds is a key, its type the value's:
an integer, a real, a boolean or text; text a format has no types for (INI,
YAML's plain scalars, `.properties`) is typed by the library's own Tier 0
inference (D-24), so INI's `port = 8080` is an integer. An array of values of
one type is a list of that type. What a contract has no type for is left out
and named on stderr with its key - a null, an array of tables, of arrays or of
mixed types, an empty array - and the exit code stays 0. `--as` names the
file's format (`-` reads standard input, whose format it must be told or find
in the content), `--title` the schema's title (else the file's name without
its extension), `--out` the file written (else standard output). The draft has
no defaults, ranges or `required`: one file shows what a value was, not what
it may be, and a value written as a default would be inherited by every
deployment without a word (D-96). Every key is optional and every description
empty, and the schema is closed, so `configforge gen check` against it refuses
a misspelt key. It is a draft: review it, fill in the descriptions and add the
ranges before it is the contract.

The pre-commit hook `configforge-check` (`actions/doctor/check.sh`) runs
`configforge gen check` on each staged file. It uses the `configforge-gen`
that `CONFIGFORGE_GEN` names, else one it builds once from the checkout.
