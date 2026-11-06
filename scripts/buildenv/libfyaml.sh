#!/bin/sh
# libfyaml, the YAML 1.2 parser behind CF_YAML_BACKEND=libfyaml (D-62),
# from its release tarball checked against the digest GitHub publishes,
# built with CMake and installed under one prefix:
#
#   sh scripts/buildenv/libfyaml.sh [prefix]     default /opt/cf-tools/libfyaml
#
# Not the distribution's: Ubuntu 24.04 ships 0.8, which hangs on malformed
# input (libfyaml issue 101, fixed in 0.9.1). setup.sh and CI's libfyaml
# job run this; nothing is installed outside the prefix.
set -eu

LIBFYAML_VERSION=0.9.6
LIBFYAML_SHA256=a59cc3331e2eb903ec36933ad52a45888041cac31e44f553a00511131242c483
prefix=${1:-/opt/cf-tools/libfyaml}

if grep -q "^Version: $LIBFYAML_VERSION\$" "$prefix/lib/pkgconfig/libfyaml.pc" 2>/dev/null; then
  exit 0
fi
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
curl -fsSL -o "$tmp/libfyaml.tar.gz" \
  "https://github.com/pantoniou/libfyaml/releases/download/v$LIBFYAML_VERSION/libfyaml-$LIBFYAML_VERSION.tar.gz"
got=$(sha256sum "$tmp/libfyaml.tar.gz" | cut -d' ' -f1)
[ "$got" = "$LIBFYAML_SHA256" ] || { echo "libfyaml: checksum mismatch: $got" >&2; exit 1; }
mkdir "$tmp/src"
tar -xzf "$tmp/libfyaml.tar.gz" -C "$tmp/src" --strip-components=1
cmake -S "$tmp/src" -B "$tmp/build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$prefix" -DBUILD_SHARED_LIBS=ON -DBUILD_TESTING=OFF
cmake --build "$tmp/build"
cmake --install "$tmp/build"
