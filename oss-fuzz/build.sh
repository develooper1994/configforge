#!/bin/bash -eu
# OSS-Fuzz's build: the harnesses, built by the script ClusterFuzzLite's
# build shares (scripts/fuzz/build_harnesses.sh).
exec bash "$SRC/configforge/scripts/fuzz/build_harnesses.sh"
