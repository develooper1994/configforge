#!/usr/bin/env bash
# Prints the path of a Doctor to run, for the pre-commit hooks: the one
# $CONFIGFORGE_DOCTOR names when it is set, else one built once from this
# checkout with CMake into a cache named after the commit it came from -
# pre-commit clones the hook's repository at the revision a project pins,
# so a new pin is a new build and an old one is reused. Nothing is
# downloaded. Exits 2, saying why, when there is none.
set -u
here="$(cd "$(dirname "$0")" && pwd)"

if [ -n "${CONFIGFORGE_DOCTOR:-}" ]; then
  printf '%s\n' "$CONFIGFORGE_DOCTOR"
  exit 0
fi
src="$here/../.."
rev="$(git -C "$src" rev-parse --short HEAD 2>/dev/null || echo local)"
cache="${XDG_CACHE_HOME:-$HOME/.cache}/configforge-doctor/$rev"
doctor="$(find "$cache" -type f \( -name doctor -o -name doctor.exe \) 2>/dev/null | head -n 1)"
if [ -z "$doctor" ]; then
  echo "doctor: building Doctor ($rev) once, into $cache" >&2
  cmake -S "$src" -B "$cache" -DCMAKE_BUILD_TYPE=Release -DCF_BUILD_TESTING=OFF \
    -DCF_BUILD_EXAMPLES=OFF -DCF_ENABLE_GATES=OFF >/dev/null &&
    cmake --build "$cache" --target doctor --config Release --parallel >/dev/null || {
    echo "doctor: could not build Doctor; set CONFIGFORGE_DOCTOR to one" >&2
    exit 2
  }
  doctor="$(find "$cache" -type f \( -name doctor -o -name doctor.exe \) | head -n 1)"
fi
printf '%s\n' "$doctor"
