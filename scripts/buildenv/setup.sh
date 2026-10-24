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
  build-essential cmake ninja-build pkg-config python3 \
  clang clang-format clang-tidy libclang-rt-18-dev llvm \
  binutils cppcheck gcovr cmake-format \
  musl-tools tcc pcc \
  libyaml-dev libexpat1-dev libsqlite3-dev libkqueue-dev \
  qemu-system-x86 qemu-utils openssh-client \
  podman uidmap fuse-overlayfs

# The locales the suite asks for (test_port), as CI generates them.
sed -i -e 's/^# *\(tr_TR.UTF-8\)/\1/' -e 's/^# *\(de_DE.UTF-8\)/\1/' /etc/locale.gen
locale-gen
update-locale

# Nothing kept that a rebuild would download again.
apt-get clean
rm -rf /var/lib/apt/lists/*
