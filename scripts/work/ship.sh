#!/bin/sh
# Verify a worktree's branch locally (clang-tidy, then the full verify)
# and push it only when that exits 0; then wait for its CI.
# usage, from the worktree: sh scripts/work/ship.sh <card id>
. "$(dirname "$0")/env.sh"
id=${1:?card id}; br=work/$id
[ -z "$(git status --porcelain)" ] || { echo "tree not clean"; exit 1; }
sh "$here/verify.sh" "$id" > "$CF_WORK/ship-$id.log" 2>&1
rc=$?; grep -E "clang-tidy|all green|FAILED|failed" "$CF_WORK/ship-$id.log" | head -8
[ $rc -eq 0 ] || { echo "local verify exit $rc: not pushed"; exit $rc; }
git push -q -u origin "$br" 2>&1 | grep -v '^remote' ; sha=$(git rev-parse HEAD)
for i in 1 2 3 4 5 6 7 8 9 10 11 12; do
  run=$(gh run list --branch "$br" --workflow ci --limit 1 --json databaseId,headSha --jq ".[]|select(.headSha==\"$sha\")|.databaseId")
  [ -n "$run" ] && break; sleep 10
done
gh run watch "$run" >/dev/null 2>&1
# Code first: a failed style step (clang-format, clang-tidy, format-patch)
# does not fail a card; any other failed step does (docs/Plan.md).
bad=$(gh run view "$run" --json jobs --jq '.jobs[] as $j | if $j.name=="format-patch" then empty else ($j.steps[]? | select(.conclusion=="failure") | select(.name!="clang-tidy" and .name!="clang-format" and (.name|test("format";"i")|not)) | "\($j.name): \(.name)") end')
style=$(gh run view "$run" --json jobs --jq '[.jobs[]|select(.conclusion=="failure")|.name]|join(", ")')
echo "$id CI run $run; failed jobs: ${style:-none}"
[ -z "$bad" ] && { echo "code CI green"; exit 0; }
echo "code failures:"; echo "$bad"; exit 1
