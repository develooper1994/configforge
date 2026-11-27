#!/bin/bash -eu
# ClusterFuzzLite's build: the harnesses, built by the script OSS-Fuzz's
# oss-fuzz/build.sh shares (scripts/fuzz/build_harnesses.sh).
exec bash "$SRC/configforge/scripts/fuzz/build_harnesses.sh"
