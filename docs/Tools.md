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
| `configforge-doctor` | Reads provenance reports: `explain`, `dump`, `lint`, `diff`, `history`, `blame`, `check`, `gen`, `formats`, `import-*`. Until 1.8.0 it is also installed as `doctor`, which says its new name on stderr. | [Doctor](Doctor.md) |

`configforge gen` (the contract's own program) and `configforge codec`
(the serialization library's) are listed in the help from the start and
arrive with their own cards; until then they are "not a configforge
command", like any other name with no program.
