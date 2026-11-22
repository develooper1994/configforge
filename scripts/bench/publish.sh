#!/bin/bash
# Adds results files to the branch bench-history (P-1b, D-111). Run by
# .github/workflows/bench.yml, in a checkout of the repository with the
# credentials of the job (contents: write).
#
#   scripts/bench/publish.sh <results.json>...
#
# One file a run, never edited, so two writers cannot conflict on a file:
#   runs/<yyyy>/<yyyymmddThhmmssZ>-<sha7>-<kind>.json
# where <kind> is quick, or full-<compiler> (and -<flags> unless they are
# `shipped`). The branch is an orphan made here the first time, with a README of
# three lines. Each file is one commit, `bench: <sha7> <kind> [skip ci]`, as
# github-actions[bot]; a push that loses a race is rebased and tried again,
# three times.
set -euo pipefail

if [ "$#" -lt 1 ]; then
  echo "usage: publish.sh <results.json>..." >&2
  exit 2
fi
branch=bench-history
work="${RUNNER_TEMP:-/tmp}/bench-history-work"
rm -rf "$work"

if git fetch --quiet origin "$branch"; then
  git worktree add --quiet -B "$branch" "$work" "origin/$branch"
else
  git worktree add --quiet --detach "$work"
  git -C "$work" checkout --quiet --orphan "$branch"
  git -C "$work" rm -rfq . 2>/dev/null || true
  cat > "$work/README.md" <<'README'
This branch holds the numbers of the benchmark's comparison with other libraries (D-111).
It is written by CI (.github/workflows/bench.yml) and by nothing else: one file a run, never edited.
A file is `runs/<year>/<time>-<sha7>-<kind>.json`, the schema is in scripts/bench/run.py, and docs/Performance.md draws it.
README
  git -C "$work" add README.md
fi

git -C "$work" config user.name "github-actions[bot]"
git -C "$work" config user.email "41898282+github-actions[bot]@users.noreply.github.com"

for f in "$@"; do
  read -r path sha7 kind < <(python3 - "$f" <<'PY'
import json
import sys

r = json.load(open(sys.argv[1], encoding="utf-8"))
stamp = r["committed_at"].replace("-", "").replace(":", "")
kind = "quick" if r["kind"] == "quick" else "full-" + r["compiler"]["id"]
if r["kind"] == "full" and r["compiler"].get("flags", "shipped") != "shipped":
    kind += "-" + r["compiler"]["flags"]
print("runs/%s/%s-%s-%s.json" % (stamp[:4], stamp, r["commit"][:7], kind), r["commit"][:7], kind)
PY
  )
  mkdir -p "$work/$(dirname "$path")"
  cp "$f" "$work/$path"
  git -C "$work" add "$path"
  git -C "$work" commit --quiet -m "bench: $sha7 $kind [skip ci]"
done

for attempt in 1 2 3; do
  if git -C "$work" push --quiet origin "HEAD:refs/heads/$branch"; then
    echo "bench-history: pushed $# file(s)"
    exit 0
  fi
  echo "bench-history: push $attempt failed, rebasing" >&2
  git -C "$work" pull --quiet --rebase origin "$branch"
done
echo "bench-history: could not push after three attempts" >&2
exit 1
