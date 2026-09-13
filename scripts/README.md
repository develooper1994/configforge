# scripts

Developer scripts: things that build, generate or check.

| Script | What it does |
| ------ | ------------ |
| `verify.cmake` | everything that has to be true before a commit |
| `amalgamate.py` | generates the single-file distribution |

The line against `tools/` is what ships: `tools/doctor` is a program a
user runs, so it is built and installed. Nothing here is.

`verify.cmake` is CMake rather than a shell script because a shell
script covers half the platforms this library claims, and the other half
would need a batch file kept in step with it. CMake is already required
to build.

`amalgamate.py` is Python because it is a build-time generator that
never reaches a user, so a dependency here costs nothing a consumer
pays. It runs in CI on every commit through `gate.amalgamation`.
