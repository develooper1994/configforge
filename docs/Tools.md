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
| `configforge-doctor` | Reads provenance reports: `explain`, `dump`, `lint`, `diff`, `history`, `blame`, `import-*`. Until 1.8.0 it is also installed as `doctor`, which says its new name on stderr, and still runs `check`, `gen` and `formats`. | [Doctor](Doctor.md) |
| `configforge-codec` | The serialization library's own program: `formats`, `detect`, `check` and `dump`, on documents and never on a configuration. | [below](#configforge-codec) |
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
configforge codec --version
```

Exit codes: 0 done (`check`: every file is well formed), 1 a file is not,
or its format cannot be told; 2 unusable input. Anything else is the usage
on stderr, exit 2.

**formats.** Every built-in codec: its name, its extensions and what kind
of format it is (text or binary, self-describing or schema-bound, typed,
arrays, read and written). `--format markdown` is the table in
docs/Formats.md, which a test holds the page to. It was `doctor formats`;
Doctor runs it in 1.7.x and says where it went.

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

## configforge gen

The contract's program (`configforge-gen`). A JSON Schema can be the
contract itself (`cf_builder_schema_file`, D-64): the keys, their types,
ranges, defaults and spellings, written once and read by the program, by an
editor and by these two commands, which use it without the program (D-67).
It is built wherever Doctor is, and not where tools are off (WASI,
`CF_PLATFORM=none`).

```text
configforge gen --schema <schema> [--name <function>] [--out <file>]
                               the contract as C calls
configforge gen check --schema <schema> <file>...
                               files against the contract
configforge gen schema --from <file> [--as <format>] [--title <t>] [--out <file>]
                               a draft contract from a sample file
configforge gen --version
```

Exit codes: 0 done, and for `check` every file holds to the contract; 1 a
file does not; 2 unusable input. `doctor check` and `doctor gen` of earlier
releases are these two commands: Doctor runs them in 1.7.x, says where they
went on stderr, and drops them in 1.8.0 (Doctor.md, section 7).

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
