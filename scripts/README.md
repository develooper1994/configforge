# scripts

Developer scripts: things that build, generate or check.

| Script | What it does |
| ------ | ------------ |
| `verify.cmake` | everything that has to be true before a commit; `-DTIER=1` (every commit) or `-DTIER=2` (a release) picks the steps (D-50), `-DSOURCE=index` checks only what is staged, `-DTEST_TIMEOUT=s` bounds a test (default 900), `-DEXTRA=kqueue;tidy;clang;tsan;yaml;compilers;fuzz;coverage` adds builds, `-DJOBS=N` bounds parallelism, `-DWORK=dir` puts the staged copy in `dir` (under WSL, keep it off `/mnt/c`) |
| `format.cmake` | formats and lints what changed (`-DALL=ON` everything, `-DCHECK=ON` report only) |
| `stack.cmake` | every library frame against a budget, by feature; the build runs it after linking, and `-DOBJDIR=build/CMakeFiles/configforge.dir -DBUDGET=N` asks "what if" (D-37) |
| `version.cmake` | prints the version, or changes it everywhere it is written: `-DTO=0.3.0` rewrites `cf_version.h` and the vcpkg manifest and opens a CHANGELOG section (D-40) |
| `buildid.cmake` | writes the commit a build comes from into `cf_build_id.h`; the build runs it on every build |
| `apiref.cmake` | regenerates `docs/API.md` from the public headers' comments; `gate.apiref` checks it is current |
| `amalgamate.py` | generates the single-file distribution |
| `check_formats.py` | every format string against what `cf_fmt` supports (run by `gate.formats`) |
| `release-local.sh`, `release-local.ps1` | the release workflow's jobs run on this machine, from a tag: source, amalgamation and static musl Linux (podman), and Windows (MSVC) - for when Actions cannot run |

The usual sequence before a commit:

```sh
cmake -P scripts/format.cmake
git add -p
cmake -DSOURCE=index -DJOBS=2 -P scripts/verify.cmake
```

## Releasing without CI

When GitHub Actions cannot run (2026-10: blocked by billing), a release
is the same steps by hand. After `version.cmake -DTO=x.y.z`, the dated
CHANGELOG heading and the tag pushed:

```sh
sh scripts/release-local.sh x.y.z dist             # Linux, with podman
pwsh scripts/release-local.ps1 -Version x.y.z -Dist dist   # Windows, with MSVC
cd dist && sha256sum * > SHA256SUMS
awk -v v=x.y.z '$0 ~ "^## " v "( |$)" {on=1; next} on && /^## / {exit} on' ../CHANGELOG.md > ../notes.md
gh release create vx.y.z * --title "ConfigForge x.y.z" --notes-file ../notes.md
```

The macOS archive needs a Mac; without one the release says so in its
notes. When Actions runs again, the workflow is the way.

The line against `tools/` is what ships: `tools/doctor` is a program a
user runs, so it is built and installed. Nothing here is.

## buildenv: the environment, as a file

Every tool the build, the gates and the `EXTRA` steps use, written down
once so that "works on my machine" has a machine to point at.

| File | What it makes |
| ---- | ------------- |
| `buildenv/dev-setup.sh`, `buildenv/dev-setup.ps1` | the whole environment in one command, on the machine it runs on: setup.sh's list through apt, Homebrew's on macOS, the container elsewhere; on Windows MSVC, CMake, Ninja, Git, Python and LLVM through winget, and the WSL distribution below. `--check` / `-Check` names each tool and the verify step it serves, changing nothing |
| `buildenv/setup.sh` | the list itself: Ubuntu 24.04 packages, as CI runs, and the locales the suite asks for. Run as root, by the two below |
| `buildenv/Containerfile` | a container image from it, for `docker build` or `podman build` |
| `buildenv/wsl-distro.ps1` | a WSL distribution from it, `configforge-dev`, whose disk lives on the drive you name (`-Dir D:\cf-wsl`) - for a Windows machine where C: has no room or there is no container runtime |
| `buildenv/freebsd.sh` | a throwaway FreeBSD virtual machine, run from inside the environment, that builds the staged tree and runs the unit tests against the real kqueue |

These are the only scripts here that use the network, and only when you
run them: each downloads from the project that publishes the thing -
Canonical's base image, Ubuntu's archive, FreeBSD's CI image. The
`ubuntu-base` tarball and the FreeBSD image are checked against the
checksums those projects publish before anything uses them; the
container's base image is checked by the container runtime against its
registry digest, and packages by apt against Ubuntu's signatures.
Everything else in this directory, `verify` included, stays offline.

```sh
pwsh scripts/buildenv/wsl-distro.ps1 -Dir D:\cf-wsl
wsl -d configforge-dev -- cmake -DSOURCE=index -DWORK=/root/cfv -P scripts/verify.cmake
wsl -d configforge-dev -- sh scripts/buildenv/freebsd.sh /root/cf-freebsd
```

Without KVM - inside WSL there usually is none - the FreeBSD machine is
emulated, which takes tens of minutes and is still exact: the kqueue it
tests is FreeBSD's kernel, not the emulator.

`verify.cmake` is CMake rather than a shell script because a shell
script covers half the platforms this library claims, and the other half
would need a batch file kept in step with it. CMake is already required
to build.

`amalgamate.py` is Python because it is a build-time generator that
never reaches a user, so a dependency here costs nothing a consumer
pays. It runs in CI on every commit through `gate.amalgamation`.
