#!/bin/bash -eu
# The harnesses tests/fuzz builds for verify -DEXTRA=fuzz, built the same
# way (CF_FUZZ: libFuzzer, with the address and undefined-behaviour
# sanitisers) with ClusterFuzzLite's compiler and flags, and copied to
# $OUT with the deterministic fuzzer's corpora as their seeds.
cmake -S "$SRC/configforge" -B build -DCMAKE_BUILD_TYPE=Debug -DCF_FUZZ=ON \
  -DCMAKE_C_COMPILER="$CC" -DCMAKE_CXX_COMPILER="$CXX" \
  -DCMAKE_C_FLAGS="$CFLAGS" -DCMAKE_CXX_FLAGS="$CXXFLAGS" -DCF_WITH_CXX=OFF
cmake --build build --parallel "$(nproc)" --target fuzz_seeds
# fuzz_seeds writes seed_<reader>/ where it runs, as the fuzz.* tests read them.
(cd build/tests/fuzz && ./fuzz_seeds)
cmake --build build --parallel "$(nproc)"
for h in build/tests/fuzz/fuzz_*; do
  name=$(basename "$h")
  [ "$name" = fuzz_seeds ] && continue
  [ -x "$h" ] || continue
  cp "$h" "$OUT/"
  reader=${name#fuzz_}
  if [ -d "build/tests/fuzz/seed_$reader" ]; then
    (cd "build/tests/fuzz/seed_$reader" && zip -q -r "$OUT/${name}_seed_corpus.zip" .)
  fi
done
