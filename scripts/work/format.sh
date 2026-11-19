#!/bin/sh
# Format the files a worktree changed against origin/main: C and C++ with
# CI's pinned clang-format (setup.sh's /opt/cf-tools), CMake with
# cmake-format. On Windows it runs in the WSL distribution.
# usage, from the worktree: sh scripts/work/format.sh
. "$(dirname "$0")/env.sh"
changed() {
  { git diff --name-only --diff-filter=AMR origin/main -- "$@"
    git ls-files -o --exclude-standard -- "$@"; } | sort -u
}
c=$(changed '*.c' '*.h' '*.cpp' '*.hpp')
m=$(changed '*.cmake' '*CMakeLists.txt')
root=$(cf_lpath "$(pwd)")
[ -n "$c" ] && cf_linux sh -c 'cd "$1" && shift && PATH=/opt/cf-tools/bin:$PATH clang-format -i "$@"' sh "$root" $c
[ -n "$m" ] && cf_linux sh -c 'cd "$1" && shift && cmake-format -i "$@"' sh "$root" $m
echo "formatted: $(echo $c $m | wc -w) files"
