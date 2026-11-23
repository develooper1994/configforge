#!/bin/sh
# One card through the deputy (Codex) and back, with no organiser turn in
# between: Codex carries the card in its worktree, then this script
# formats, regenerates the line-citing files on Linux, runs the style
# gates there, commits, pushes and waits for CI. The organiser reads one
# summary at the end (docs/Maintaining.md, "Where the tokens go").
#
#   sh scripts/work/codex-card.sh <worktree> <branch> <model> <effort> \
#      <prompt-file> <report-file> "<commit subject>"
#
# Run from Git Bash on the maintainer's Windows workstation (Codex, WSL
# distro configforge-dev, gh). Nothing is landed on main: the organiser
# does that after reading the summary.
set -u
W=$1 BR=$2 MODEL=$3 EFFORT=$4 PROMPT=$5 REPORT=$6 SUBJECT=$7
CODEX=${CODEX:-/c/Users/selcu/AppData/Local/Programs/OpenAI/Codex/bin/codex}
WSL=/c/Windows/System32/wsl.exe
ID=$(basename "$W")
SRC=/d/cf-work/$ID-src
say() { printf '%s\n' "$*"; }

cd "$W" || exit 2
timeout 7200 "$CODEX" exec -m "$MODEL" -c "model_reasoning_effort=$EFFORT" \
  -s workspace-write -C "$(cygpath -w "$W")" -o "$(cygpath -w "$REPORT")" - \
  < "$PROMPT" > "$REPORT.log" 2>&1
say "codex exit $?"
if ! grep -q '^STATUS: DONE' "$REPORT"; then
  say "codex did not finish: report follows"
  head -c 3000 "$REPORT"
  tail -c 300 "$REPORT.log"
  exit 1
fi
rm -f lint-findings.txt
python scripts/work/work.py format > /dev/null 2>&1
rm -rf "$SRC" && mkdir -p "$SRC" &&
  git ls-files -co --exclude-standard -z | tar --null -T - -cf - |
  tar -xf - -C "$SRC" || exit 2
cat > "/d/cf-work/$ID-gen.sh" <<EOF
cd /mnt/d/cf-work/$ID-src || exit 2
for g in apiref codemap matrix; do
  [ -f scripts/generate/\$g.cmake ] && cmake -P scripts/generate/\$g.cmake > /dev/null 2>&1
done
cmake -DWRITE=docs/Limits.md -P scripts/generate/limits.cmake > /dev/null 2>&1
B=\$HOME/cf-$ID-build; rm -rf \$B
cmake -S . -B \$B -G Ninja > /dev/null 2>&1 && cmake --build \$B -j3 > /dev/null 2>&1 || { echo "linux build failed"; exit 1; }
ctest --test-dir \$B -R '^gate\\.' --output-on-failure 2>&1 | grep -E '\\[[CEWR][0-9]+\\]|Failed|tests passed' | head -30
EOF
MSYS_NO_PATHCONV=1 "$WSL" -d configforge-dev -- sh "/mnt/d/cf-work/$ID-gen.sh"
for f in docs/API.md docs/Codemap.md docs/Limits.md README.md; do
  [ -f "$SRC/$f" ] && ! cmp -s "$SRC/$f" "$f" && cp "$SRC/$f" "$f" && say "regenerated $f"
done
git add -A && git commit -q -m "$SUBJECT

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" || say "nothing to commit"
git push -q -u origin "HEAD:refs/heads/$BR" 2>&1 | grep -v '^remote' || true
sleep 20
RUN=$(gh run list --branch "$BR" --workflow ci.yml --limit 1 --json databaseId --jq '.[0].databaseId')
gh run watch "$RUN" --interval 180 --exit-status > /dev/null 2>&1
say "CI $RUN exit $? at $(git rev-parse --short HEAD)"
gh run view "$RUN" --json jobs --jq '.jobs[]|select(.conclusion=="failure")|.name' | tr '\n' ';'
say ""
head -c 1500 "$REPORT"
