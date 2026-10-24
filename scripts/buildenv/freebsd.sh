#!/bin/sh
# The suite on FreeBSD, in a throwaway virtual machine: the real kqueue,
# for a machine that has no Mac and no BSD.
#
#   sh scripts/buildenv/freebsd.sh [work-dir]
#
# Run inside the development environment (setup.sh installs qemu and
# ssh). It downloads FreeBSD's official CI image once, checked against
# the published SHA256, boots it, installs cmake and ninja, copies in
# the staged tree (git archive of `git write-tree`, as verify.cmake
# does), builds it and runs the unit tests - test_watch is the one that
# matters here - and powers the machine off. Everything it makes is in
# the work directory; delete it and nothing is left.
#
# The CI image rather than the cloud-init one: it is made for this, with
# sshd on and root allowed in without a password, and nothing on first
# boot but growing its disk. The cloud-init image applies security
# updates on first boot before sshd starts, which emulated took over half
# an hour, and turning that off from the seed did not work. Root without
# a password is safe here because sshd is reachable only on 127.0.0.1 of
# this environment, and the machine lives for one run.
#
# KVM is used where /dev/kvm exists. Without it qemu emulates, which is
# slow (tens of minutes) and still exact: kqueue is the kernel's, not
# the emulator's.
set -eu

REL=14.4-RELEASE
IMG=FreeBSD-$REL-amd64-BASIC-CI-ufs.raw
URL=https://download.freebsd.org/releases/CI-IMAGES/$REL/amd64/Latest
WORK=${1:-$HOME/cf-freebsd}
SRC=$(git rev-parse --show-toplevel)
PORT=2222

mkdir -p "$WORK"
cd "$WORK"

# Into a .part file first, so an interrupted download is fetched again
# rather than mistaken for a whole one.
if [ ! -f "$IMG.xz" ]; then
  curl -fsSL -o CHECKSUM.SHA256 "$URL/CHECKSUM.SHA256"
  curl -fsSL -o "$IMG.xz.part" "$URL/$IMG.xz"
  mv "$IMG.xz.part" "$IMG.xz"
fi
want=$(sed -n "s/^SHA256 ($IMG.xz) = //p" CHECKSUM.SHA256)
got=$(sha256sum "$IMG.xz" | cut -d' ' -f1)
[ "$want" = "$got" ] || { echo "checksum mismatch for $IMG.xz" >&2; exit 1; }

# A fresh disk every run: the machine is disposable, the download is not.
# Written sparse, so the image's empty space takes none here.
rm -f disk.raw
xz -dc "$IMG.xz" | dd of=disk.raw bs=1M conv=sparse iflag=fullblock status=none
qemu-img resize -q -f raw disk.raw +4G

accel=tcg
[ -e /dev/kvm ] && accel=kvm
qemu-system-x86_64 -accel "$accel" -m 2048 -smp 4 -display none -daemonize \
  -pidfile qemu.pid -serial file:console.log \
  -drive file=disk.raw,format=raw,if=virtio \
  -nic user,model=virtio-net-pci,hostfwd=tcp:127.0.0.1:$PORT-:22

stop() { [ -f qemu.pid ] && kill "$(cat qemu.pid)" 2>/dev/null || true; }
trap stop EXIT

SSH="ssh -p $PORT -o BatchMode=yes -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR -o ConnectTimeout=10 root@127.0.0.1"

echo "waiting for the machine ($accel)..."
i=0
until $SSH true 2>/dev/null; do
  i=$((i + 1))
  [ $i -lt 360 ] || { echo "it did not come up; console.log says why" >&2; exit 1; }
  sleep 10
done
$SSH 'uname -sr'
$SSH 'env ASSUME_ALWAYS_YES=yes pkg install -q cmake ninja'

tree=$(git -C "$SRC" write-tree)
git -C "$SRC" archive --format=tar "$tree" | $SSH 'rm -rf src && mkdir src && tar -xf - -C src'
# Quiet when it works, and the end of the log when it does not.
$SSH 'cd src && { cmake -S . -B build -G Ninja -DCF_WITH_CXX=OFF -DCF_BUILD_EXAMPLES=OFF &&
      cmake --build build; } > build.log 2>&1 || { tail -40 build.log; exit 1; }
      ctest --test-dir build -R "^unit\." --output-on-failure'
# What test_watch saw, for the record: the wait handle it used is kqueue.
$SSH 'cd src/build/tests/unit && ./test_watch | tail -3'
$SSH 'shutdown -p now' || true
# The trap stops qemu if the shutdown did not; the disk goes with the
# work directory.
