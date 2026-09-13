#!/bin/sh
# Everything that has to be true before a commit.
#
# Written down because chaining the steps by hand got a failing state
# committed once: `ctest | grep passed` succeeded on the word "passed"
# inside "98% tests passed", and the commit went through. A checkpoint
# of a broken tree is worse than no checkpoint.
#
# Usage: scripts/verify.sh [quick]
#   quick  default preset only, for the middle of a change
#   (none) every preset, for before a commit
set -e

root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"

run() {
    printf '%-10s ' "$1"
    shift
    if "$@" >/tmp/cf-verify.log 2>&1; then
        echo "ok"
    else
        echo "FAILED"
        tail -25 /tmp/cf-verify.log
        exit 1
    fi
}

presets="default"
if [ "$1" != "quick" ]; then
    presets="default sanitize portable minimal analyze"
fi

for p in $presets; do
    run "$p cfg"   cmake --preset "$p"
    run "$p build" cmake --build --preset "$p"
    run "$p test"  ctest --preset "$p"
done

# The formatter rewrites; the gate reports. Running the format target
# here would hide a disagreement instead of surfacing it, so this only
# checks - `cmake --build build --target format` is the fix.
echo
echo "all green"
