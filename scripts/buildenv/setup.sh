#!/bin/sh
# The development environment, as one list: every tool the build, the
# gates and every EXTRA step of verify.cmake use, on Ubuntu 26.04 - the
# image CI runs on (`ubuntu-26.04`). Run as root. The Containerfile beside
# this runs it, and so does wsl-distro.ps1, so a container and a WSL
# distribution made from it hold exactly the same tools.
#
# The compiler runtime is clang's own version's (21 on 26.04): the
# sanitizers and libFuzzer of another version do not link. libkqueue left
# the archive in 26.04, so EXTRA=kqueue is skipped here and CI's kqueue
# job runs on 24.04; macOS and FreeBSD (freebsd.sh) run the real kqueue.
#
# docs/Dependencies.md says what each one is for and under which licence.
set -eu

export DEBIAN_FRONTEND=noninteractive
apt-get update
apt-get install -y --no-install-recommends \
  ca-certificates curl git file xz-utils locales util-linux \
  build-essential cmake ninja-build ccache pkg-config python3 python3-venv python3-yaml \
  clang clang-format clang-tidy libclang-rt-21-dev llvm \
  binutils cppcheck gcovr cmake-format valgrind cbmc \
  lld wasi-libc libclang-rt-21-dev-wasm32 nodejs abigail-tools \
  musl-tools tcc pcc \
  libyaml-dev libfyaml-dev libexpat1-dev libsqlite3-dev \
  qemu-system-x86 qemu-utils openssh-client \
  podman uidmap fuse-overlayfs

# Rust from rustup, not Ubuntu's 1.75, which cannot read the tree's
# Cargo.lock (version 4). Pinned, so every machine has the same one:
# the version the Windows side runs. rustup-init is checked against the
# checksum rust-lang.org publishes beside it before it runs.
RUST_TOOLCHAIN=1.99.1
if ! command -v cargo >/dev/null 2>&1 || ! cargo --version | grep -q "^cargo $RUST_TOOLCHAIN "; then
  dist=https://static.rust-lang.org/rustup/dist/x86_64-unknown-linux-gnu
  tmp=$(mktemp -d)
  curl -fsSL -o "$tmp/rustup-init" "$dist/rustup-init"
  curl -fsSL -o "$tmp/rustup-init.sha256" "$dist/rustup-init.sha256"
  want=$(cut -d' ' -f1 "$tmp/rustup-init.sha256")
  got=$(sha256sum "$tmp/rustup-init" | cut -d' ' -f1)
  [ "$want" = "$got" ] || { echo "rustup-init: checksum mismatch" >&2; exit 1; }
  chmod +x "$tmp/rustup-init"
  RUSTUP_HOME=/opt/rust CARGO_HOME=/opt/rust "$tmp/rustup-init" -y --no-modify-path \
    --profile minimal --component clippy,rustfmt --default-toolchain "$RUST_TOOLCHAIN"
  rm -rf "$tmp"
  for t in cargo rustc rustfmt cargo-fmt cargo-clippy clippy-driver rustup; do
    printf '#!/bin/sh\nRUSTUP_HOME=/opt/rust CARGO_HOME=/opt/rust exec /opt/rust/bin/%s "$@"\n' "$t" >"/usr/local/bin/$t"
    chmod +x "/usr/local/bin/$t"
  done
fi

# clang-format pinned, from PyPI: the gate accepts one major version
# (cmake/ConfigForgeClangFormat.cmake), because two versions disagree on
# layouts no code can satisfy both ways. Ubuntu's own clang-format stays
# for clang-tidy's use; this one comes first on PATH.
# json5, pinned, is the reference reader JSON5's values are compared
# with (verify -DEXTRA=conformance); Ubuntu packages none. The
# environment sees the system's packages too (PyYAML, for the same
# step), and verify runs that step with this interpreter.
CLANG_FORMAT_VERSION=22.1.8
JSON5_VERSION=0.17.3
if ! /opt/cf-tools/bin/python3 -c "import yaml" 2>/dev/null; then
  rm -rf /opt/cf-tools
  python3 -m venv --system-site-packages /opt/cf-tools
fi
if ! /opt/cf-tools/bin/clang-format --version 2>/dev/null |
  grep -q " $CLANG_FORMAT_VERSION"; then
  /opt/cf-tools/bin/pip install --quiet "clang-format==$CLANG_FORMAT_VERSION"
fi
if ! /opt/cf-tools/bin/python3 -c \
  "import json5, sys; sys.exit(json5.__version__ != '$JSON5_VERSION')" 2>/dev/null; then
  /opt/cf-tools/bin/pip install --quiet "json5==$JSON5_VERSION"
fi
ln -sf /opt/cf-tools/bin/clang-format /usr/local/bin/clang-format

# ccache's cache, bounded: verify reuses it from one unit to the next.
ccache --max-size=5G >/dev/null

# The locales the suite asks for (test_port), as CI generates them.
sed -i -e 's/^# *\(tr_TR.UTF-8\)/\1/' -e 's/^# *\(de_DE.UTF-8\)/\1/' /etc/locale.gen
locale-gen
update-locale

# Nothing kept that a rebuild would download again.
apt-get clean
rm -rf /var/lib/apt/lists/*
