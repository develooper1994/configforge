#!/usr/bin/env bash
# The contract hook (.pre-commit-hooks.yaml, configforge-check): each
# staged configuration file read against a JSON Schema contract, on its
# own, as `doctor check` reads it (docs/Doctor.md, section 7d).
#
#   check.sh --schema <schema> <file>...
#
# The worst exit code: 0 every file holds to the contract, 1 one does
# not, 2 unusable input. Doctor is the one doctor-bin.sh finds or builds.
set -u
here="$(cd "$(dirname "$0")" && pwd)"

schema=""
files=()
while [ $# -gt 0 ]; do
  case "$1" in
    --schema)
      if [ $# -lt 2 ]; then
        echo "doctor: --schema needs a value" >&2
        exit 2
      fi
      schema="$2"
      shift 2
      ;;
    --) shift; files+=("$@"); break ;;
    -*) echo "doctor: unknown option $1" >&2; exit 2 ;;
    *) files+=("$1"); shift ;;
  esac
done
if [ -z "$schema" ]; then
  echo "doctor: the hook needs --schema <schema> in its args" >&2
  exit 2
fi
if [ ${#files[@]} -eq 0 ]; then
  exit 0 # no configuration staged: nothing to say
fi
doctor="$(bash "$here/doctor-bin.sh")" || exit 2

worst=0
for f in "${files[@]}"; do
  printf '%s: ' "$f"
  "$doctor" check --schema "$schema" "$f"
  rc=$?
  if [ "$rc" -gt "$worst" ]; then worst="$rc"; fi
done
exit "$worst"
