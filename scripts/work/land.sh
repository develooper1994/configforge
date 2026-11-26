#!/bin/sh
# A card from its worktree to main, every dependent step in order, each
# one only if the one before succeeded (docs/Plan.md, the loop, 9-10):
#   main merged into the branch -> local verify -> push -> CI (code) ->
#   squash onto main (tree checked) -> index: the card done, the cards
#   that waited only on it open -> push -> worktree and branches removed.
# usage, from the main checkout:
#   sh scripts/work/land.sh <card id> "<subject after 'id: '>"
# SKIP_SHIP=1 when the branch's last commit already passed ship.sh.
set -e
# shellcheck source=env.sh
. "$(dirname "$0")/env.sh"
id=${1:?card id}; subj=${2:?subject}; br=work/$id
root=$(git rev-parse --show-toplevel); cd "$root"
wt=$(git worktree list --porcelain | awk -v b="refs/heads/$br" '/^worktree /{w=substr($0,10)} $0=="branch "b{print w}')
[ -n "$wt" ] || { echo "no worktree on $br"; exit 1; }
git fetch -q
[ "$(git rev-parse main)" = "$(git rev-parse origin/main)" ] || { echo "main differs from origin/main"; exit 1; }
if ! git merge-base --is-ancestor origin/main "$br"; then
  git -C "$wt" merge -q --no-edit origin/main; SKIP_SHIP=0
fi
# Formatting is mechanical and CI's tests include its gate: done here,
# so no card spends time on it. clang-tidy waits for the group's end.
before=$(git rev-parse "$br")
(cd "$wt" && sh "$here/format.sh" >/dev/null && git add -A &&
  { git diff --cached --quiet || { git commit -q -m "$id as clang-format and cmake-format leave it

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"; }; })
[ "$(git rev-parse "$br")" = "$before" ] || SKIP_SHIP=0
[ "${SKIP_SHIP:-0}" = 1 ] || (cd "$wt" && sh "$here/ship.sh" "$id")
# The whole suite once a card (L-22): by hand, on the branch as it stands; a
# green full run of this very commit is not run again.
sha=$(git rev-parse "$br")
if [ -z "$(gh run list --branch "$br" --workflow ci --event workflow_dispatch --limit 20 --json headSha,conclusion --jq ".[]|select(.headSha==\"$sha\" and .conclusion==\"success\")|.headSha")" ]; then
  known=$(gh run list --branch "$br" --workflow ci --event workflow_dispatch --limit 20 --json databaseId --jq '.[].databaseId')
  gh workflow run ci --ref "$br" -f tier=full
  full=
  for _ in 1 2 3 4 5 6 7 8 9 10 11 12; do
    for r in $(gh run list --branch "$br" --workflow ci --event workflow_dispatch --limit 20 --json databaseId,headSha --jq ".[]|select(.headSha==\"$sha\")|.databaseId"); do
      printf '%s
' "$known" | grep -qx "$r" || full=$r
    done
    [ -n "$full" ] && break; sleep 10
  done
  [ -n "$full" ] || { echo "no full CI run for $sha"; exit 1; }
  gh run watch "$full" --exit-status >/dev/null 2>&1 || { echo "full CI run $full failed: not squashed"; exit 1; }
fi
body=$(git log --reverse --no-merges main.."$br" --format='- %s')
git merge --squash -q "$br" >/dev/null
git commit -q -F - <<MSG
$id: $subj

$body

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
MSG
[ "$(git rev-parse "HEAD^{tree}")" = "$(git rev-parse "$br^{tree}")" ] || { echo "tree differs: not pushed"; git reset -q --hard origin/main; exit 1; }
h=$(git rev-parse --short HEAD)
"$(command -v python3 || command -v python)" "$here/index.py" --done "$id" "$h"
git add docs/Plan.md docs/plan CLAUDE.md
git commit -q -m "Plan: $id done ($h)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
git push -q origin main 2>&1 | grep -v '^remote' || true
git worktree remove -f -f "$wt" 2>/dev/null || git worktree prune
# shellcheck disable=SC2046 # the branch names are words
git branch -D "$br" $(git branch --format='%(refname:short)' | grep '^worktree-agent-') >/dev/null 2>&1 || true
git push -q origin --delete "$br" 2>/dev/null || true
git log --oneline -2
