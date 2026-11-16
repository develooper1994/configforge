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
| `configforge-doctor` | Reads provenance reports: `explain`, `dump`, `lint`, `diff`, `history`, `blame`, `formats`, `import-*`. Until 1.8.0 it is also installed as `doctor`, which says its new name on stderr, and still runs `check` and `gen`. | [Doctor](Doctor.md) |
| `configforge-gen` | The contract's own program: `configforge gen --schema` writes a JSON Schema contract as C, `configforge gen check` holds files to it. | [below](#configforge-gen) |

`configforge codec` (the serialization library's own program) is listed in
the help from the start and arrives with its own card; until then it is
"not a configforge command", like any other name with no program.

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

The pre-commit hook `configforge-check` (`actions/doctor/check.sh`) runs
`configforge gen check` on each staged file. It uses the `configforge-gen`
that `CONFIGFORGE_GEN` names, else one it builds once from the checkout.
