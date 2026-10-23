# scripts

Developer scripts: things that build, generate or check.

| Script | What it does |
| ------ | ------------ |
| `verify.cmake` | everything that has to be true before a commit; `-DSOURCE=index` checks only what is staged, `-DEXTRA=kqueue;tidy;clang;tsan;yaml;compilers;fuzz;coverage` adds builds, `-DJOBS=N` bounds parallelism, `-DWORK=dir` puts the staged copy in `dir` (under WSL, keep it off `/mnt/c`) |
| `format.cmake` | formats and lints what changed (`-DALL=ON` everything, `-DCHECK=ON` report only) |
| `stack.cmake` | every library frame against a budget, by feature; the build runs it after linking, and `-DOBJDIR=build/CMakeFiles/configforge.dir -DBUDGET=N` asks "what if" (D-37) |
| `apiref.cmake` | regenerates `docs/API.md` from the public headers' comments; `gate.apiref` checks it is current |
| `amalgamate.py` | generates the single-file distribution |
| `check_formats.py` | every format string against what `cf_fmt` supports (run by `gate.formats`) |

The usual sequence before a commit:

```sh
cmake -P scripts/format.cmake
git add -p
cmake -DSOURCE=index -DJOBS=2 -P scripts/verify.cmake
```

The line against `tools/` is what ships: `tools/doctor` is a program a
user runs, so it is built and installed. Nothing here is.

`verify.cmake` is CMake rather than a shell script because a shell
script covers half the platforms this library claims, and the other half
would need a batch file kept in step with it. CMake is already required
to build.

`amalgamate.py` is Python because it is a build-time generator that
never reaches a user, so a dependency here costs nothing a consumer
pays. It runs in CI on every commit through `gate.amalgamation`.
