#!/bin/sh
# Every local check of the tree, its lanes in parallel: the organiser's, once a
# group of cards is on the batch branch and before its full CI run. On Linux
# (WSL on Windows): sh scripts/work/check-all.sh; TREE=<dir> checks another tree.
#
# - The generated documents are written into the tree once, first; then the
#   tree is copied to Linux's own disk (/root/fc/src), so no lane writes into
#   the tree and none reads through /mnt/d, which is slow.
# - Each lane builds in its own directory (/root/fc/<lane>) and logs to
#   /root/fc/log/<lane>.log: no lane touches another's files.
# - At most LANES lanes at once, each with JOBS compile jobs, all under
#   nice/ionice idle: LANES*JOBS (8 by default) stays below the 12 cores, so
#   Windows keeps cores to itself, and WSL's memory cap protects its RAM.
# Usage: sh scripts/work/check-all.sh [lane...]   (no lane: every lane)
LANES=${LANES:-4}
JOBS=${JOBS:-2}
tree=${TREE:-$(cd "$(dirname "$0")/../.." && pwd)}
fc=/root/fc
src=$fc/src
self=$tree/scripts/work/check-all.sh

lane() {
    name=$1
    b=$fc/$name
    log=$fc/log/$name.log
    start=$(date +%s)
    {
        case $name in
        default)
            cmake -S "$src" -B "$b" >/dev/null 2>&1
            cmake --build "$b" -j"$JOBS" 2>&1 | grep -E "error" | head -10
            ctest --test-dir "$b" -j"$JOBS" 2>&1 | grep -E "tests passed|\*\*\*Failed|Failed  " | head -20 ;;
        misra)
            (cd "$src" && cmake --preset misra -B "$b" >/dev/null 2>&1)
            cmake -DSRC="$src" -DBIN="$b" "-DDEFINES=-D__GNUC__=15|-D__GNUC_MINOR__=2" -P "$src/cmake/gates/GateMisra.cmake" >"$b.out" 2>&1
            rc=$?
            grep -E "VIOLATION|mismatch|differ" "$b.out" | head -12
            [ $rc -eq 0 ] && echo "misra: pass" || echo "misra: FAIL (exit $rc)" ;;
        clang)
            cmake -S "$src" -B "$b" -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ >/dev/null 2>&1
            cmake --build "$b" -j"$JOBS" 2>&1 | grep -E "error|warning:" | head -10
            ctest --test-dir "$b" -R "oom|^gate\." -j"$JOBS" 2>&1 | grep -E "tests passed|\*\*\*Failed" ;;
        none)
            (cd "$src" && cmake --preset none -B "$b" >/dev/null 2>&1)
            cmake --build "$b" -j"$JOBS" 2>&1 | grep -E "error" | head -5
            ctest --test-dir "$b" -j"$JOBS" 2>&1 | grep -E "tests passed|\*\*\*Failed" ;;
        tidy)
            cmake -S "$src" -B "$b" -DCF_CLANG_TIDY=ON >/dev/null 2>&1
            cmake --build "$b" -j"$JOBS" 2>&1 | grep -E "error:|warning:" | head -10 ;;
        iwyu)
            cmake -S "$src" -B "$b" -DCMAKE_BUILD_TYPE=Debug "-DCMAKE_C_INCLUDE_WHAT_YOU_USE=include-what-you-use;-Xiwyu;--error;-Xiwyu;--mapping_file=$src/.iwyu.imp;-Wno-unknown-warning-option" >/dev/null 2>&1
            cmake --build "$b" --target configforge configforge_serialize -j"$JOBS" 2>&1 | grep -E "should add|should remove|^#include|Error" | head -20 ;;
        ubsan)
            (cd "$src" && cmake --preset ubsan-strict -B "$b" >/dev/null 2>&1)
            cmake --build "$b" -j"$JOBS" 2>&1 | grep -E "error:" | head -5
            ctest --test-dir "$b" -j"$JOBS" 2>&1 | grep -E "tests passed|\*\*\*Failed|runtime error" | head -10 ;;
        analyze)
            (cd "$src" && cmake --preset analyze -B "$b" >/dev/null 2>&1)
            cmake --build "$b" -j"$JOBS" 2>&1 | grep -E "style:|warning:|error:|Error [0-9]" | head -15 ;;
        ruff)
            command -v ruff >/dev/null && ruff check "$src/scripts" 2>&1 | tail -3 ;;
        esac
    } >"$log" 2>&1
    echo "== $name ($(( $(date +%s) - start ))s)"
    cat "$log"
}

# One lane, when xargs calls back.
if [ "$1" = "--lane" ]; then
    lane "$2"
    exit 0
fi

cd "$tree" || exit 1
cmake -P scripts/generate/codemap.cmake >/dev/null 2>&1
cmake -P scripts/generate/apiref.cmake >/dev/null 2>&1
mkdir -p "$fc/log"
rm -rf "$src"
mkdir -p "$src"
# The tree without its build directories.
tar -C "$tree" --exclude='./build*' -cf - . | tar -C "$src" -xf -

lanes=${*:-"default misra clang none tidy iwyu ubsan analyze ruff"}
start=$(date +%s)
# The longest lanes first, so the last to start is a short one.
printf '%s\n' $lanes | nice -n 19 ionice -c3 xargs -P "$LANES" -I{} sh "$self" --lane {}
echo "== all done ($(( $(date +%s) - start ))s)"
