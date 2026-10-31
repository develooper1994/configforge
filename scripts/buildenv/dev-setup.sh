#!/bin/sh
# The development environment in one command, on whatever this machine is.
#
#   sh scripts/buildenv/dev-setup.sh           install what is missing
#   sh scripts/buildenv/dev-setup.sh --check   say what is here, change nothing
#
# Ubuntu or Debian: setup.sh's list - the one CI and the container use -
# through sudo when not root. macOS: Homebrew's packages for the same
# steps, where Homebrew has them. Anything else: the container image from
# the Containerfile beside this, with podman or docker. On Windows,
# dev-setup.ps1 does the native side and makes the WSL distribution.
#
# --check names each tool and the verify step that needs it, so a step
# that will be skipped is known before a run, not found in its output.
set -eu
here=$(cd "$(dirname "$0")" && pwd)

check() {
  missing=0
  # tool, then what it is for
  while read -r tool what; do
    if command -v "$tool" >/dev/null 2>&1; then
      printf '  ok       %-20s %s\n' "$tool" "$what"
    else
      printf '  missing  %-20s %s\n' "$tool" "$what"
      missing=$((missing + 1))
    fi
  done <<'EOF'
cmake every build
ninja fast builds (verify falls back to make)
ccache rebuilds that cost what changed
cc the default preset
clang EXTRA=clang, fuzz, wasm
clang-format gate.format
cmake-format gate.format, CMake files
cppcheck the analyze preset
python3 gates, amalgamation, Python binding
git SOURCE=index, release
valgrind EXTRA=valgrind
cbmc EXTRA=proofs
abidiff EXTRA=abi
node EXTRA=wasm
wasm-ld EXTRA=wasm
musl-gcc EXTRA=compilers
tcc EXTRA=compilers
pcc EXTRA=compilers
gcovr EXTRA=coverage
cargo -DCF_RUST=ON
qemu-system-x86_64 buildenv/freebsd.sh
podman scripts/release-local.sh
EOF
  if [ "$missing" -eq 0 ]; then
    echo "everything verify can use is here"
  else
    echo "$missing missing: the steps they serve are skipped, with a note"
  fi
}

as_root() {
  if [ "$(id -u)" -eq 0 ]; then "$@"; else sudo "$@"; fi
}

if [ "${1:-}" = "--check" ]; then
  check
  exit 0
fi

if [ -r /etc/os-release ] && command -v apt-get >/dev/null 2>&1; then
  as_root sh "$here/setup.sh"
elif [ "$(uname -s)" = Darwin ] && command -v brew >/dev/null 2>&1; then
  brew install cmake ninja ccache llvm cppcheck python node rust cbmc gcovr \
    libyaml expat sqlite
  python3 -m pip install --user cmakelang
elif command -v podman >/dev/null 2>&1 || command -v docker >/dev/null 2>&1; then
  run=$(command -v podman || command -v docker)
  "$run" build -t configforge-dev -f "$here/Containerfile" "$here"
  echo "ready: $run run --rm -v \"\$PWD:/src\" -w /src configforge-dev" \
    "cmake -DSOURCE=tree -DWORK=/tmp/cfv -P scripts/verify.cmake"
  exit 0
else
  echo "no apt, no Homebrew, no container runtime: install one, or the" >&2
  echo "tools --check lists by hand" >&2
  exit 1
fi
check
