# Dependencies and their licences

The bill of materials, split by who needs what:

1. **Using the library** - what a program that links ConfigForge needs.
   For the default build, nothing but a C89 compiler and its C library.
2. **Developing the library** - what building its tests, gates and tools
   needs, in two kinds: libraries that get linked into a test build, and
   binary tools that are run and never linked.
3. **Continuous integration** - what the CI jobs install.

Nothing in sections 2 and 3 ships with the library.

A machine-readable SBOM (SPDX 2.3) is generated for each build from the
options it was configured with, at `<build>/configforge.spdx.json`, and
installed beside the licence. It is generated rather than written so it
cannot describe a build that was not made.

`gate.dependencies` fails the build if CMake looks for any package or
program that this page does not name - a dependency added without
saying so here is a broken build, not a stale document.

## 1. Using the library

| Component | Licence | When | Notes |
| --------- | ------- | ---- | ----- |
| ConfigForge | Zlib | always | this repository; see [`LICENSE`](../LICENSE) and D-36 |
| C standard library | the platform's | always | C89 functions only; the locale-sensitive ones are forbidden (Conventions.md) |
| POSIX: `stat`, `opendir`, `environ` | the platform's | POSIX builds | part of the system C library |
| Linux: `inotify`, `poll` | the platform's | Linux builds | system calls, through the C library |
| BSD and macOS: `kqueue` | the platform's | macOS and BSD builds | in the system C library |
| Windows: kernel32, shell32 | the platform's | Windows builds | `CommandLineToArgvW` is shell32's; every Windows toolchain links both by default |

That is the whole list for a default build: no third-party dependency.
TOML is built in like INI and JSON, with its own parser. One format is
not, and has to be asked for:

| Library | Licence | When | Notes |
| ------- | ------- | ---- | ----- |
| libyaml | MIT | `CF_WITH_YAML=ON`, off by default | reading YAML; writing it is done here, by hand. Found as `yaml.h` and a library named `yaml`, which every packager agrees on |

The formats the design names and the build does not yet have - XML and
SQLite (D-23) - would each add a row here when they arrive.

The single-file amalgamation needs the same and nothing else: no build
system, no generated header, no options.

## 2. Developing the library

### 2a. Libraries, linked only into test builds

| Library | Licence | Used for |
| ------- | ------- | -------- |
| libkqueue (`pkg-config: libkqueue`) | BSD-2-Clause | `CF_WAKE_KQUEUE=ON` on Linux: building and testing the macOS and BSD wait handle on a machine that is neither |
| Threads (pthreads, through CMake's `Threads`) | the platform's | `test_threads`, the threading contract (D-14) under real threads |

### 2b. Binary tools, run and never linked

| Tool | Licence | Needed for |
| ---- | ------- | ---------- |
| CMake (`find_package` of `PkgConfig`, `Threads`, `Python3`, and `ConfigForge` in the install gate) | BSD-3-Clause | the build, the gates, `scripts/verify.cmake` |
| a C89 compiler: GCC, Clang or MSVC | GPL-3.0-or-later with the GCC Runtime Library Exception; Apache-2.0 WITH LLVM-exception; proprietary | compiling. None of their code ends up in a program except each one's own runtime support, under its exception |
| a C++98 compiler, the same three | as above | the C++03 wrapper's example and test |
| clang, clang++ | Apache-2.0 WITH LLVM-exception | `EXTRA=clang` in `scripts/verify.cmake`: the suite under a second compiler |
| git | GPL-2.0-only | `scripts/verify.cmake -DSOURCE=index` and `scripts/format.cmake`, which ask it what is staged or changed |
| Python 3 | PSF-2.0 | `scripts/amalgamate.py`, `scripts/check_formats.py` |
| pkg-config | GPL-2.0-or-later | finding libkqueue; the install gate's pkg-config consumer |
| libFuzzer (part of LLVM) | Apache-2.0 WITH LLVM-exception | `CF_FUZZ`, `EXTRA=fuzz`: the coverage-guided harnesses in `tests/fuzz` |
| gcovr | BSD-3-Clause | `EXTRA=coverage`: line coverage from gcov's data |
| setarch (util-linux) | GPL-2.0-or-later | `EXTRA=tsan`: running the thread-sanitised suite with address randomisation off |
| clang-format | Apache-2.0 WITH LLVM-exception | `gate.format.c`, the `format` target |
| clang-tidy | Apache-2.0 WITH LLVM-exception | `CF_CLANG_TIDY=ON` |
| cmake-format, cmake-lint (cmakelang) | GPL-3.0 | `gate.format.cmake`, `gate.cmake.lint` |
| cppcheck | GPL-3.0-or-later | the `analyze` preset |
| GNU binutils: `nm`, `objdump` | GPL-3.0-or-later | `gate.symbols` |

Every tool in 2b is optional except CMake and a compiler: a gate whose
tool is missing is not registered, and the build says which.

A tool's licence binds the tool, not what it reads: running a GPL
formatter over this code does not change the code's licence.

## 3. Continuous integration

| Component | Licence | Used for |
| --------- | ------- | -------- |
| `actions/checkout` | MIT | every CI job |
| Ubuntu packages: libkqueue-dev, cppcheck, clang, clang-tidy, clang-format, locales | as above | the jobs that name them |
