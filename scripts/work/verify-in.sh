#!/bin/sh
# The Linux half of verify.sh: a copy of the staged tree, verified
# one run at a time. Code first (docs/Plan.md): the copy is formatted with
# CI's pinned clang-format and cmake-format, so the format gates never stop
# the presets; CI's clang build runs too. STYLE=1 (a milestone's end):
# nothing is formatted and clang-tidy runs as CI's job does.
id=$1; mode=$2; tar=$3; base=${CF_LINUX_WORK:-$HOME}; src=$base/src-$id
export PATH=/opt/cf-tools/bin:"$PATH"
# Out of the caller's directory: a Windows worktree's .git names a path
# Linux git cannot read.
cd / || exit 1
git config --global --get-all safe.directory | grep -qx '\*' ||
  git config --global --add safe.directory '*'
rm -rf "$src" && mkdir -p "$src" && tar -xf "$tar" -C "$src" || exit 1
cd "$src" || exit 1
if [ "$STYLE" != 1 ]; then
  find src include tools tests bench examples \( -name '*.[ch]' -o -name '*.[ch]pp' \) -print0 2>/dev/null |
    xargs -0 -r clang-format -i
  find . -name CMakeLists.txt -o -name '*.cmake' | grep -v '^./build' | xargs -r cmake-format -i 2>/dev/null
fi
git init -q && git add -A || exit 1
(
  flock 9
  if [ "$STYLE" = 1 ]; then
    echo "--- clang-tidy (CI's job)"
    if ! { cmake -S . -B "$base/tidy-$id" -DCF_CLANG_TIDY=ON > "$base/tidy-$id".log 2>&1 &&
      cmake --build "$base/tidy-$id" -j3 >> "$base/tidy-$id".log 2>&1; }; then
      grep -E "error:" "$base/tidy-$id".log | head -20; echo "clang-tidy FAILED"; exit 1
    fi
    echo "clang-tidy ok"
  fi
  echo "--- clang build and test (CI's clang job)"
  if ! { cmake -S . -B "$base/clang-$id" -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ &&
    cmake --build "$base/clang-$id" && ctest --test-dir "$base/clang-$id" -j3 -E '^gate\.format'; } > "$base/clang-$id".log 2>&1; then
    grep -E "error:|\(Failed\)" "$base/clang-$id".log | head -20; echo "clang FAILED"; exit 1
  fi
  echo "clang ok"
  EXTRA=-DEXTRA=; [ "$mode" = full ] && EXTRA=-DEXTRA=compilers
  cmake -DSOURCE=index -DMODE="$mode" "$EXTRA" -DJOBS=3 \
    -DWORK="$base/cfv-$id" -P scripts/verify.cmake > "$base/verify-$id".log 2>&1
  rc=$?
  tail -25 "$base/verify-$id".log
  exit $rc
) 9>"$base/verify.lock"
