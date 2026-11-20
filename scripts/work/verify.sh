#!/bin/sh
# Verify a worktree's staged tree on Linux - in the WSL distribution on
# Windows - one run at a time on the machine. The staged tree is archived
# first, so Linux never reads a Windows worktree's .git.
# usage, from the worktree: sh scripts/work/verify.sh <id> [full|quick]
# STYLE=1 (a milestone's end) also runs clang-tidy and the format gates.
# shellcheck source=env.sh
. "$(dirname "$0")/env.sh"
id=${1:?usage: verify.sh <id> [full|quick]}; mode=${2:-full}
tree=$(git write-tree) || exit 1
git -c core.autocrlf=false archive --format=tar -o "$CF_WORK/src-$id.tar" "$tree" || exit 1
# The last release, so that verify's abi step has a tag to compare with.
base=$(git describe --tags --abbrev=0 --match 'v*' HEAD 2>/dev/null)
if [ -n "$base" ]; then
  git -c core.autocrlf=false archive --format=tar -o "$CF_WORK/src-$id-base.tar" "$base" || exit 1
fi
cf_linux env STYLE="${STYLE:-0}" CF_EXTRA="${CF_EXTRA:-}" CF_BASE="$base" sh "$(cf_lpath "$here")/verify-in.sh" "$id" "$mode" \
  "$(cf_lpath "$CF_WORK")/src-$id.tar"
