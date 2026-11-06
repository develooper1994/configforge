#!/bin/sh
# The development environment, as one list: every tool the build, the
# gates and every EXTRA step of verify.cmake use, on Ubuntu 24.04 - the
# image CI runs on. Run as root. The Containerfile beside this runs it,
# and so does wsl-distro.ps1, so a container and a WSL distribution made
# from it hold exactly the same tools.
#
# docs/Dependencies.md says what each one is for and under which licence.
set -eu

export DEBIAN_FRONTEND=noninteractive
apt-get update
apt-get install -y --no-install-recommends \
  ca-certificates curl git file xz-utils locales util-linux \
  build-essential cmake ninja-build ccache pkg-config python3 python3-venv python3-yaml \
  clang clang-format clang-tidy libclang-rt-18-dev llvm \
  binutils cppcheck gcovr cmake-format valgrind cbmc \
  lld wasi-libc libclang-rt-18-dev-wasm32 nodejs abigail-tools \
  musl-tools tcc pcc \
  libyaml-dev libexpat1-dev libsqlite3-dev libkqueue-dev \
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
# layouts no code can satisfy both ways. Ubuntu's clang-format 18 stays
# for clang-tidy's use; this one comes first on PATH.
CLANG_FORMAT_VERSION=22.1.8
if [ ! -x /opt/cf-tools/bin/clang-format ] ||
  ! /opt/cf-tools/bin/clang-format --version | grep -q " $CLANG_FORMAT_VERSION"; then
  python3 -m venv /opt/cf-tools
  /opt/cf-tools/bin/pip install --quiet "clang-format==$CLANG_FORMAT_VERSION"
fi
ln -sf /opt/cf-tools/bin/clang-format /usr/local/bin/clang-format

# libfyaml 0.9.6 under /opt/cf-tools/libfyaml (D-62), so the default
# build still finds Ubuntu's libyaml and verify's yaml step this one.
sh "$(dirname "$0")/libfyaml.sh" /opt/cf-tools/libfyaml

# ccache's cache, bounded: verify reuses it from one unit to the next.
ccache --max-size=5G >/dev/null

# The locales the suite asks for (test_port), as CI generates them.
sed -i -e 's/^# *\(tr_TR.UTF-8\)/\1/' -e 's/^# *\(de_DE.UTF-8\)/\1/' /etc/locale.gen
locale-gen
update-locale

# Nothing kept that a rebuild would download again.
apt-get clean
rm -rf /var/lib/apt/lists/*
