#!/usr/bin/env bash
# What the Doctor action's "Changed configuration" step runs: for each
# file of a pull request that matches the patterns, the keys that changed
# between the base and the head, as Markdown for one comment. Keys only:
# configforge-diff prints a key's path and never its value. Everything
# comes in through the environment, never pasted into a script - a path
# from a pull request is text somebody else wrote, and is used only as a
# git pathspec result and a name inside a temporary directory.
#
#   CF_DIFF       the configforge-diff program
#   CF_BASE_SHA   the base commit (the checkout needs fetch-depth: 0)
#   CF_HEAD_SHA   the head commit; default HEAD
#   CF_FILES      the patterns, one per line, each a git pathspec
#   CF_MARKDOWN   where to write the Markdown; default configforge-changes.md
#
# Exits 0 whatever the diff finds: whether a pull request may merge is not
# this script's decision. 2 for input it cannot use.
set -u

for need in CF_DIFF CF_BASE_SHA CF_FILES; do
  if [ -z "${!need:-}" ]; then
    echo "doctor: $need is not set" >&2
    exit 2
  fi
done
head_sha="${CF_HEAD_SHA:-HEAD}"
out="${CF_MARKDOWN:-configforge-changes.md}"
max_files=50
max_lines=100

if ! git cat-file -e "$CF_BASE_SHA^{commit}" 2>/dev/null; then
  echo "doctor: the base commit is not fetched; check out with fetch-depth: 0" >&2
  exit 2
fi
patterns=()
while IFS= read -r line; do
  line="${line%$'\r'}"
  if [ -n "$line" ]; then patterns+=("$line"); fi
done <<<"$CF_FILES"
if [ "${#patterns[@]}" -eq 0 ]; then
  echo "doctor: no pattern in CF_FILES" >&2
  exit 2
fi

tmp="$(mktemp -d)" || exit 2
trap 'rm -rf "$tmp"' EXIT
printf '{}' >"$tmp/empty.json"

{
  echo "<!-- configforge-changes -->"
  echo "### ConfigForge: changed configuration keys"
} >"$out"

count=0
skipped=0
while IFS= read -r -d '' status && IFS= read -r -d '' path; do
  count=$((count + 1))
  if [ "$count" -gt "$max_files" ]; then
    skipped=$((skipped + 1))
    continue
  fi
  ext="${path##*.}"
  case "$ext" in
    *[!A-Za-z0-9]* | "$path") ext="" ;;
    *) ext=".$ext" ;;
  esac
  left="$tmp/left$ext"
  right="$tmp/right$ext"
  opts=()
  case "$status" in
    A)
      left="$tmp/empty.json"
      opts=(--as-left json)
      git show "$head_sha:$path" >"$right" 2>/dev/null || : >"$right"
      ;;
    D)
      right="$tmp/empty.json"
      opts=(--as-right json)
      git show "$CF_BASE_SHA:$path" >"$left" 2>/dev/null || : >"$left"
      ;;
    *)
      git show "$CF_BASE_SHA:$path" >"$left" 2>/dev/null || : >"$left"
      git show "$head_sha:$path" >"$right" 2>/dev/null || : >"$right"
      ;;
  esac
  rc=0
  lines="$("$CF_DIFF" "${opts[@]+"${opts[@]}"}" -- "$left" "$right" 2>/dev/null)" || rc=$?
  if [ "$rc" -eq 0 ]; then continue; fi
  shown="$(printf '%s\n' "$path" | tr -d '`' | tr -d '[:cntrl:]')"
  {
    echo
    echo "#### \`$shown\`"
    if [ "$rc" -eq 1 ]; then
      total="$(printf '%s\n' "$lines" | wc -l | tr -d ' ')"
      echo '```'
      printf '%s\n' "$lines" | head -n "$max_lines"
      echo '```'
      if [ "$total" -gt "$max_lines" ]; then
        echo "$((total - max_lines)) more changed keys are not listed."
      fi
    else
      echo "could not be compared (a reader refused it)"
    fi
  } >>"$out"
  listed=$((${listed:-0} + 1))
done < <(git diff --name-status --no-renames -z "$CF_BASE_SHA" "$head_sha" -- "${patterns[@]}")

{
  if [ "${listed:-0}" -eq 0 ]; then
    echo
    echo "No file matching \`config:\` changed."
  fi
  if [ "$skipped" -gt 0 ]; then
    echo
    echo "$skipped more files changed and are not listed."
  fi
  echo
  echo "Keys only: no value is shown. Which keys are out of range or touch a secret is not checked here yet."
} >>"$out"
echo "doctor: wrote the changed keys of ${listed:-0} file(s) to $out"
