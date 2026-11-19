#!/usr/bin/env bash
# Prints the path of a ConfigForge program to run, for the pre-commit
# hooks. One optional argument names it: configforge-doctor (the default)
# or configforge-gen. The program is the one $CONFIGFORGE_DOCTOR (for the
# first) or $CONFIGFORGE_GEN (for the second) names when it is set, else
# one built once from this checkout with CMake into a cache named after
# the commit it came from - pre-commit clones the hook's repository at
# the revision a project pins, so a new pin is a new build and an old one
# is reused. Nothing is downloaded. Exits 2, saying why, when there is
# none.
set -u
here="$(cd "$(dirname "$0")" && pwd)"

prog="${1:-configforge-doctor}"
case "$prog" in
  configforge-doctor) var=CONFIGFORGE_DOCTOR target=doctor ;;
  configforge-gen) var=CONFIGFORGE_GEN target=configforge_gen ;;
  *) echo "doctor: '$prog' is not a program this hook runs" >&2; exit 2 ;;
esac

if [ -n "${!var:-}" ]; then
  printf '%s\n' "${!var}"
  exit 0
fi
src="$here/../.."
rev="$(git -C "$src" rev-parse --short HEAD 2>/dev/null || echo local)"
cache="${XDG_CACHE_HOME:-$HOME/.cache}/configforge-doctor/$rev"
bin="$(find "$cache" -type f \( -name "$prog" -o -name "$prog.exe" \) 2>/dev/null | head -n 1)"
if [ -z "$bin" ]; then
  echo "doctor: building $prog ($rev) once, into $cache" >&2
  if ! { cmake -S "$src" -B "$cache" -DCMAKE_BUILD_TYPE=Release -DCF_BUILD_TESTING=OFF \
    -DCF_BUILD_EXAMPLES=OFF -DCF_ENABLE_GATES=OFF >/dev/null &&
    cmake --build "$cache" --target "$target" --config Release --parallel >/dev/null; }; then
    echo "doctor: could not build $prog; set $var to one" >&2
    exit 2
  fi
  bin="$(find "$cache" -type f \( -name "$prog" -o -name "$prog.exe" \) | head -n 1)"
fi
printf '%s\n' "$bin"
