#!/usr/bin/env bash
# The pre-commit hook (.pre-commit-hooks.yaml): lint each staged report,
# and diff it against the report it replaces when --base names one.
#
#   pre-commit.sh [--base F [--only-changed]] [--baseline F] [--severity F]
#                 [--fail-on L] <report>...
#
# --only-changed lints only what changed since --base.
#
# Doctor is the one doctor-bin.sh finds or builds. Without pre-commit,
# the same script is a .git/hooks/pre-commit of its own.
set -u
here="$(cd "$(dirname "$0")" && pwd)"

base="" baseline="" severity="" fail_on="error" only_changed="false"
reports=()
while [ $# -gt 0 ]; do
  case "$1" in
    --only-changed) only_changed="true"; shift ;;
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

doctor="$(bash "$here/doctor-bin.sh")" || exit 2

worst=0
for r in "${reports[@]}"; do
  CF_DOCTOR="$doctor" CF_REPORT="$r" CF_BASE="$base" CF_BASELINE="$baseline" \
    CF_SEVERITY="$severity" CF_FAIL_ON="$fail_on" CF_ONLY_CHANGED="$only_changed" \
    bash "$here/run.sh"
  rc=$?
  if [ "$rc" -gt "$worst" ]; then worst="$rc"; fi
done
exit "$worst"
