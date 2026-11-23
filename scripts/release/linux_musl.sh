#!/bin/sh
# Build and package one static Linux musl release from a clean checkout.
set -eu

src=$1
work=$2
v=$3
notice=$4

case ${SOURCE_DATE_EPOCH-} in
  ''|*[!0-9]*) echo 'SOURCE_DATE_EPOCH must be numeric' >&2; exit 1 ;;
esac
export TZ=UTC LC_ALL=C
umask 022

mkdir "$work"
root="$work/stage/configforge-$v"
cmake -S "$src" -B "$work/build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_EXE_LINKER_FLAGS=-static -DCF_WITH_CXX=OFF \
  -DCMAKE_INSTALL_PREFIX=/opt/configforge
cmake --build "$work/build"
ctest --test-dir "$work/build" --output-on-failure
cmake --install "$work/build" --prefix "$root"

mkdir -p "$root/bin" "$root/share/doc/configforge/third-party"
for tool in configforge configforge-doctor; do
  install -s -m 755 "$work/build/tools/bin/$tool" "$root/bin/$tool"
done
install -m 644 "$notice" "$root/share/doc/configforge/third-party/musl-COPYRIGHT"

tar --sort=name --mtime="@$SOURCE_DATE_EPOCH" --owner=0 --group=0 \
  --numeric-owner --format=gnu -C "$work/stage" \
  -czf "$work/configforge-$v-linux-x86_64-musl.tar.gz" "configforge-$v"
