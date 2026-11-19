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
body=$(git log --reverse --no-merges main.."$br" --format='- %s')
git merge --squash -q "$br" >/dev/null
git commit -q -F - <<MSG
$id: $subj

$body

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
MSG
[ "$(git rev-parse "HEAD^{tree}")" = "$(git rev-parse "$br^{tree}")" ] || { echo "tree differs: not pushed"; git reset -q --hard origin/main; exit 1; }
h=$(git rev-parse --short HEAD)
"$(command -v python3 || command -v python)" - "$id" "$h" <<'PY'
import sys,re
cid,h=sys.argv[1],sys.argv[2]; p='docs/Plan.md'
s=open(p,encoding='utf-8').read().split('\n')
for i,l in enumerate(s):
    c=[x.strip() for x in l.split('|')]
    if len(c)<7 or not c[2]: continue
    if c[2].split(' ')[0]==cid: c[5]='done (%s)'%h
    elif c[5]=='waits: '+cid: c[5]='open'
    else: continue
    s[i]='| '+' | '.join(c[1:6])+' |'
open(p,'w',encoding='utf-8',newline='\n').write('\n'.join(s))
PY
git add docs/Plan.md CLAUDE.md
git commit -q -m "Plan: $id done ($h)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
git push -q origin main 2>&1 | grep -v '^remote' || true
git worktree remove -f -f "$wt" 2>/dev/null || git worktree prune
# shellcheck disable=SC2046 # the branch names are words
git branch -D "$br" $(git branch --format='%(refname:short)' | grep '^worktree-agent-') >/dev/null 2>&1 || true
git push -q origin --delete "$br" 2>/dev/null || true
git log --oneline -2
