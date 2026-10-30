#!/bin/sh
# The release workflow's check, source and linux-musl jobs, run on this
# machine - for when GitHub Actions cannot run (.github/workflows/release.yml
# is the reference; keep the two in step).
#
#   sh scripts/release-local.sh <version> <dist-dir>
#
# Builds from the commit the tag v<version> names, never the working tree.
# Needs git, cmake, python3 and podman (scripts/buildenv/setup.sh has all
# four). Downloads, like the workflow: the alpine:3.20 image and musl's
# licence text from musl's own repository. The Windows archive is
# scripts/release-local.ps1; publish with scripts/README.md, "Releasing
# without CI".
set -eu
v=${1:?version, e.g. 0.4.0}
dist=${2:?a directory for the artifacts}
repo=$(cd "$(dirname "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

git -C "$repo" rev-parse -q --verify "refs/tags/v$v" >/dev/null ||
  { echo "no tag v$v"; exit 1; }
mkdir -p "$dist"
dist=$(cd "$dist" && pwd)
git -C "$repo" archive --format=tar --prefix=src/ "v$v" | tar -xf - -C "$work"
cd "$work/src"

# check: the tag, the build and the CHANGELOG agree.
built=$(cmake -P scripts/version.cmake)
[ "$built" = "$v" ] || { echo "tag $v, cf_version.h says $built"; exit 1; }
head=$(grep -E "^## $v( |\$)" CHANGELOG.md || true)
[ -n "$head" ] || { echo "CHANGELOG.md has no section for $v"; exit 1; }
case "$head" in *unreleased*) echo "CHANGELOG.md still says: $head"; exit 1 ;; esac

# source
git -C "$repo" archive --format=tar.gz --prefix="configforge-$v/" \
  -o "$dist/configforge-$v-src.tar.gz" "v$v"
python3 scripts/amalgamate.py . "$work/amalgamation/configforge-$v"
cp LICENSE "$work/amalgamation/configforge-$v/"
tar -C "$work/amalgamation" -czf "$dist/configforge-$v-amalgamation.tar.gz" "configforge-$v"

# linux-musl: Alpine, static, tested, installed.
cat > job.sh <<'EOF'
set -eu
v=$1
apk add --no-cache build-base cmake ninja git python3 tar >/dev/null
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_EXE_LINKER_FLAGS=-static -DCF_WITH_CXX=OFF \
  -DCMAKE_INSTALL_PREFIX="$PWD/stage/configforge-$v" >/dev/null
cmake --build build >/dev/null
ctest --test-dir build --output-on-failure | tail -3
cmake --install build >/dev/null
mkdir -p "stage/configforge-$v/bin"
install -s -m 755 build/tools/doctor/doctor "stage/configforge-$v/bin/doctor"
m=$(apk info musl | sed -n '1s/^musl-\([0-9.]*\)-r.*/\1/p')
d="stage/configforge-$v/share/doc/configforge/third-party"
mkdir -p "$d"
wget -q -O "$d/musl-COPYRIGHT" "https://git.musl-libc.org/cgit/musl/plain/COPYRIGHT?h=v$m"
grep -q "MIT license" "$d/musl-COPYRIGHT"
tar -C stage -czf "configforge-$v-linux-x86_64-musl.tar.gz" "configforge-$v"
EOF
podman run --rm -v "$work/src:/src" -w /src docker.io/library/alpine:3.20 sh job.sh "$v"
mv "configforge-$v-linux-x86_64-musl.tar.gz" "$dist/"

ls -l "$dist"
