#!/usr/bin/env bash
# What the Doctor action and the pre-commit hook run: lint a report and,
# given the report it replaces, diff the two. Everything comes in through
# the environment, never pasted into a script - a report path from a
# pull request is text somebody else wrote.
#
#   CF_DOCTOR     the doctor program
#   CF_REPORT     the report to lint (the pull request's, the head's)
#   CF_BASE       optional: the report it replaces (the base branch's)
#   CF_BASELINE   optional: findings already accepted
#   CF_SEVERITY   optional: this project's severities
#   CF_FAIL_ON    error (default), warning, info or never
#   CF_SARIF      optional: where to write the lint's SARIF
#   CF_MARKDOWN   optional: where to write the findings as Markdown, for
#                 a job summary or a pull-request comment
#   CF_ONLY_CHANGED  true: the lint reports only what the change touched
#                 against CF_BASE (doctor lint --only-changed), so a large
#                 configuration can adopt Doctor one change at a time
#
# Exits as Doctor does: 0, 1 when a finding reaches CF_FAIL_ON in the
# lint or the diff, 2 for input Doctor cannot use.
set -u

: "${CF_DOCTOR:?the doctor program}"
: "${CF_REPORT:?the report to lint}"
fail_on="${CF_FAIL_ON:-error}"
case "$fail_on" in
  error | warning | info | never) ;;
  *)
    echo "doctor: --fail-on is error, warning, info or never, not '$fail_on'" >&2
    exit 2
    ;;
esac

opts=()
if [ -n "${CF_BASELINE:-}" ]; then opts+=(--baseline "$CF_BASELINE"); fi
if [ -n "${CF_SEVERITY:-}" ]; then opts+=(--severity "$CF_SEVERITY"); fi
lint=() # the lint's own options; the diff has a base already
if [ "${CF_ONLY_CHANGED:-false}" = "true" ]; then
  if [ -z "${CF_BASE:-}" ]; then
    echo "doctor: only what changed needs the report it changed from (base)" >&2
    exit 2
  fi
  lint+=(--base "$CF_BASE" --only-changed)
fi

worst=0
note() { # the worst exit code so far: 2 over 1 over 0
  if [ "$1" -gt "$worst" ]; then worst="$1"; fi
}

if [ -n "${CF_SARIF:-}" ]; then
  # Whatever it holds; unusable input fails here too, with exit code 2.
  "$CF_DOCTOR" lint "$CF_REPORT" ${opts[@]+"${opts[@]}"} ${lint[@]+"${lint[@]}"} --format sarif --fail-on never >"$CF_SARIF"
  rc=$?
  if [ "$rc" -ge 2 ]; then note "$rc"; fi
fi

"$CF_DOCTOR" lint "$CF_REPORT" ${opts[@]+"${opts[@]}"} ${lint[@]+"${lint[@]}"} --fail-on "$fail_on"
note $?
if [ -n "${CF_BASE:-}" ]; then
  echo
  # The base first: "here" is what is deployed, "there" what is proposed.
  "$CF_DOCTOR" diff "$CF_BASE" "$CF_REPORT" ${opts[@]+"${opts[@]}"} --fail-on "$fail_on"
  note $?
fi

if [ -n "${CF_MARKDOWN:-}" ]; then
  {
    echo "<!-- configforge-doctor -->"
    "$CF_DOCTOR" lint "$CF_REPORT" ${opts[@]+"${opts[@]}"} ${lint[@]+"${lint[@]}"} --format markdown --fail-on never
    if [ -n "${CF_BASE:-}" ]; then
      echo
      "$CF_DOCTOR" diff "$CF_BASE" "$CF_REPORT" ${opts[@]+"${opts[@]}"} --format markdown \
        --fail-on never
      echo
      echo "In the diff, *here* is the base and *there* the change. Secrets are"
      echo "compared by digest and never shown; a blind report shows no value."
    fi
    echo
    case "$worst" in
      0) echo "Nothing at \`$fail_on\` or above." ;;
      1) echo "**Findings at \`$fail_on\` or above: this check fails.**" ;;
      *) echo "**Doctor could not use its input; see the job's log.**" ;;
    esac
  } >"$CF_MARKDOWN"
fi
exit "$worst"
