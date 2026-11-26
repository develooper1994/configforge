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
  musl-tools tcc pcc afl++ \
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

# The pinned Python tools, from PyPI, every file by its SHA-256
# (`pip install --require-hashes --no-deps`, the lists beside this file,
# made by requirements-lock.py; a list is installed again when any of its
# lines is not what the environment holds):
#  * clang-format: the gate accepts one major version
#    (cmake/ConfigForgeClangFormat.cmake), because two versions disagree on
#    layouts no code can satisfy both ways. Ubuntu's own clang-format stays
#    for clang-tidy's use; this one comes first on PATH.
#  * json5, the reference reader JSON5's values are compared with (verify
#    -DEXTRA=conformance); Ubuntu packages none. The environment sees the
#    system's packages too (PyYAML, for the same step), and verify runs that
#    step with this interpreter.
#  * shellcheck and actionlint as the wheels that carry their binaries,
#    ruff, codespell: gate.shellcheck, gate.ruff, gate.actionlint and
#    gate.codespell. A release of any of them can find something new, so a
#    version changes in tools.in, in CI (.github/workflows/ci.yml, which
#    installs the same list) and in the tree's fixes together.
#  * semgrep and what it needs (gate.semgrep): CI installs the same list.
if ! /opt/cf-tools/bin/python3 -c "import yaml" 2>/dev/null; then
  rm -rf /opt/cf-tools
  python3 -m venv --system-site-packages /opt/cf-tools
fi
here=$(cd "$(dirname "$0")" && pwd)
for list in tools semgrep; do
  req="$here/$list-requirements.txt"
  # pip freeze writes names with underscores where the lists have hyphens.
  have=$(/opt/cf-tools/bin/pip list --format=freeze 2>/dev/null | tr 'A-Z_' 'a-z-' || true)
  stale=$(sed -n 's/^\([A-Za-z0-9_.-]*==[^ ]*\) .*/\1/p' "$req" | tr 'A-Z_' 'a-z-' |
    grep -vxF "$have" || true)
  if [ -n "$stale" ]; then
    /opt/cf-tools/bin/pip install --quiet --require-hashes --no-deps -r "$req"
  fi
done
ln -sf /opt/cf-tools/bin/clang-format /usr/local/bin/clang-format
for t in shellcheck ruff actionlint codespell semgrep; do
  ln -sf "/opt/cf-tools/bin/$t" "/usr/local/bin/$t"
done

# lychee, the link checker (gate.links), as its pinned release binary: it
# is not in Ubuntu's archive. The checksum is the release archive's.
LYCHEE_VERSION=0.24.2
LYCHEE_SHA256=73657a111819a30c47c08352896796f23d64e4eb2b3ed39b6d32149241566fc5
if ! /opt/cf-tools/bin/lychee --version 2>/dev/null | grep -qF "lychee $LYCHEE_VERSION"; then
  lychee_dir=lychee-x86_64-unknown-linux-musl
  curl -fsSL -o /tmp/lychee.tgz     "https://github.com/lycheeverse/lychee/releases/download/lychee-v$LYCHEE_VERSION/$lychee_dir.tar.gz"
  echo "$LYCHEE_SHA256  /tmp/lychee.tgz" | sha256sum -c -
  tar -xzf /tmp/lychee.tgz -C /tmp "$lychee_dir/lychee"
  install -m 755 "/tmp/$lychee_dir/lychee" /opt/cf-tools/bin/lychee
  rm -rf /tmp/lychee.tgz "/tmp/$lychee_dir"
fi
ln -sf /opt/cf-tools/bin/lychee /usr/local/bin/lychee

# gitleaks (a secret anywhere in the history) and osv-scanner (a vulnerable
# pinned dependency), as their pinned release binaries, for the maintainer
# to run by hand: CI's jobs `secrets` and `osv` install the same two. Each
# is checked against the SHA-256 its release publishes.
GITLEAKS_VERSION=8.30.1
GITLEAKS_SHA256=551f6fc83ea457d62a0d98237cbad105af8d557003051f41f3e7ca7b3f2470eb
if ! /opt/cf-tools/bin/gitleaks version 2>/dev/null | grep -qF "$GITLEAKS_VERSION"; then
  curl -fsSL -o /tmp/gitleaks.tgz "https://github.com/gitleaks/gitleaks/releases/download/v$GITLEAKS_VERSION/gitleaks_${GITLEAKS_VERSION}_linux_x64.tar.gz"
  echo "$GITLEAKS_SHA256  /tmp/gitleaks.tgz" | sha256sum -c -
  tar -xzf /tmp/gitleaks.tgz -C /tmp gitleaks
  install -m 755 /tmp/gitleaks /opt/cf-tools/bin/gitleaks
  rm -f /tmp/gitleaks.tgz /tmp/gitleaks
fi
ln -sf /opt/cf-tools/bin/gitleaks /usr/local/bin/gitleaks

OSV_SCANNER_VERSION=2.7.0
OSV_SCANNER_SHA256=160c6253e2922e9e0a6a9771c5c4c40a4b3c6090b34a3cdd5e80c96cc2e4218b
if ! /opt/cf-tools/bin/osv-scanner --version 2>/dev/null | grep -qF "$OSV_SCANNER_VERSION"; then
  curl -fsSL -o /tmp/osv-scanner "https://github.com/google/osv-scanner/releases/download/v$OSV_SCANNER_VERSION/osv-scanner_linux_amd64"
  echo "$OSV_SCANNER_SHA256  /tmp/osv-scanner" | sha256sum -c -
  install -m 755 /tmp/osv-scanner /opt/cf-tools/bin/osv-scanner
  rm -f /tmp/osv-scanner
fi
ln -sf /opt/cf-tools/bin/osv-scanner /usr/local/bin/osv-scanner

# Infer (EXTRA=infer, CI job `infer`: Pulse, buffer overrun and liveness over
# the libraries), as its pinned release tarball, checked against the SHA-256
# GitHub publishes for the asset. It brings its own clang front end.
INFER_VERSION=1.3.0
INFER_SHA256=02be62ba931cb43e2d42bbf7abf4bd9a410cf8464cce5453934e25c90a825850
if ! /opt/cf-tools/bin/infer --version 2>/dev/null | grep -qF "$INFER_VERSION"; then
  curl -fsSL -o /tmp/infer.tar.xz "https://github.com/facebook/infer/releases/download/v$INFER_VERSION/infer-linux-x86_64-v$INFER_VERSION.tar.xz"
  echo "$INFER_SHA256  /tmp/infer.tar.xz" | sha256sum -c -
  rm -rf "/opt/cf-tools/infer-$INFER_VERSION"
  mkdir -p "/opt/cf-tools/infer-$INFER_VERSION"
  tar -xJf /tmp/infer.tar.xz -C "/opt/cf-tools/infer-$INFER_VERSION" --strip-components=1
  ln -sf "/opt/cf-tools/infer-$INFER_VERSION/bin/infer" /opt/cf-tools/bin/infer
  rm -f /tmp/infer.tar.xz
fi
ln -sf /opt/cf-tools/bin/infer /usr/local/bin/infer

# Frama-C (EXTRA=eva, CI workflow `eva`: EVA over port, numeric and base),
# only when FRAMAC=1 is in the environment: it builds OCaml and Frama-C from
# source through opam and takes 20 to 40 minutes. opam is its pinned release
# binary, checked against the SHA-256 GitHub publishes for the asset; the
# opam repository is pinned to one commit.
OPAM_VERSION=2.6.1
OPAM_SHA256=b533d393a6967150d6ef41cb3d71e147e93ecbabeb59e921aa412c0ae79ebe3d
OPAM_REPO_COMMIT=a305f52d0c9f1a98fb88f5fa4c4d2583ddbdbf0b
FRAMAC_VERSION=33.0
OCAML_VERSION=5.3.0
if [ "${FRAMAC:-0}" = 1 ] && ! /opt/cf-tools/bin/frama-c -version 2>/dev/null | grep -qF "$FRAMAC_VERSION"; then
  apt-get install -y --no-install-recommends m4 unzip libgmp-dev autoconf graphviz zlib1g-dev
  curl -fsSL -o /tmp/opam "https://github.com/ocaml/opam/releases/download/$OPAM_VERSION/opam-$OPAM_VERSION-x86_64-linux"
  echo "$OPAM_SHA256  /tmp/opam" | sha256sum -c -
  install -m 755 /tmp/opam /opt/cf-tools/bin/opam
  rm -f /tmp/opam
  export PATH="/opt/cf-tools/bin:$PATH" OPAMROOT=/opt/cf-tools/opam
  opam init --bare --disable-sandboxing --no-setup -y
  opam repository set-url default "git+https://github.com/ocaml/opam-repository#$OPAM_REPO_COMMIT"
  opam switch create cf-eva "ocaml-base-compiler.$OCAML_VERSION" -y
  opam install -y --switch=cf-eva "frama-c.$FRAMAC_VERSION"
  ln -sf "$OPAMROOT/cf-eva/bin/frama-c" /opt/cf-tools/bin/frama-c
fi
if [ -x /opt/cf-tools/bin/frama-c ]; then
  ln -sf /opt/cf-tools/bin/frama-c /usr/local/bin/frama-c
fi

# ccache's cache, bounded: verify reuses it from one unit to the next.
ccache --max-size=5G >/dev/null

# The locales the suite asks for (test_port), as CI generates them.
sed -i -e 's/^# *\(tr_TR.UTF-8\)/\1/' -e 's/^# *\(de_DE.UTF-8\)/\1/' /etc/locale.gen
locale-gen
update-locale

# Nothing kept that a rebuild would download again.
apt-get clean
rm -rf /var/lib/apt/lists/*
