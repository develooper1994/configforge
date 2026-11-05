#!/usr/bin/env bash
# The pre-commit hook (.pre-commit-hooks.yaml): lint each staged report,
# and diff it against the report it replaces when --base names one.
#
#   pre-commit.sh [--base F] [--baseline F] [--severity F] [--fail-on L] <report>...
#
# Doctor is $CONFIGFORGE_DOCTOR when that is set, else built once from
# this checkout with CMake into a cache named after the commit it came
# from - pre-commit clones the hook's repository at the revision a
# project pins, so a new pin is a new build and an old one is reused.
# Nothing is downloaded. Without pre-commit, the same script is a
# .git/hooks/pre-commit of its own.
set -u
here="$(cd "$(dirname "$0")" && pwd)"

base="" baseline="" severity="" fail_on="error"
reports=()
while [ $# -gt 0 ]; do
  case "$1" in
    --base | --baseline | --severity | --fail-on)
      if [ $# -lt 2 ]; then
        echo "doctor: $1 needs a value" >&2
        exit 2
      fi
      case "$1" in
        --base) base="$2" ;;
        --baseline) baseline="$2" ;;
        --severity) severity="$2" ;;
        --fail-on) fail_on="$2" ;;
      esac
      shift 2
      ;;
    --) shift; reports+=("$@"); break ;;
    -*) echo "doctor: unknown option $1" >&2; exit 2 ;;
    *) reports+=("$1"); shift ;;
  esac
done
if [ ${#reports[@]} -eq 0 ]; then
  exit 0 # no report staged: nothing to say
fi

doctor="${CONFIGFORGE_DOCTOR:-}"
if [ -z "$doctor" ]; then
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
fi

worst=0
for r in "${reports[@]}"; do
  CF_DOCTOR="$doctor" CF_REPORT="$r" CF_BASE="$base" CF_BASELINE="$baseline" \
    CF_SEVERITY="$severity" CF_FAIL_ON="$fail_on" bash "$here/run.sh"
  rc=$?
  if [ "$rc" -gt "$worst" ]; then worst="$rc"; fi
done
exit "$worst"
