#!/bin/bash
# Format userdata as f2fs and install Arch Linux ARM + rootfs/overlay, from TWRP.
# ERASES userdata. Usage: tools/install-rootfs.sh --yes-erase-userdata
set -euo pipefail
TOP=$(cd "$(dirname "$0")/.." && pwd)
DL=$TOP/out/dl/alarm
TARBALL=ArchLinuxARM-aarch64-latest.tar.gz
DEV=/dev/block/by-name/userdata
MNT=/tmp/b0sroot

[ "${1:-}" = --yes-erase-userdata ] || { echo "this erases userdata; pass --yes-erase-userdata"; exit 1; }
adb get-state 2>/dev/null | grep -q recovery || { echo "phone is not in TWRP"; exit 1; }

mkdir -p "$DL"
if [ ! -f "$DL/$TARBALL" ]; then
	curl -sfL -o "$DL/$TARBALL" "http://os.archlinuxarm.org/os/$TARBALL"
	curl -sfL -o "$DL/$TARBALL.md5" "http://os.archlinuxarm.org/os/$TARBALL.md5"
fi
(cd "$DL" && md5sum -c --quiet "$TARBALL.md5")

# overlay: config files + the static helpers from the initramfs build
OV=$TOP/out/overlay
rm -rf "$OV" && cp -a "$TOP/rootfs/overlay" "$OV"
cp "$TOP/out/b0s-reboot" "$TOP/out/b0s-volup" "$OV/usr/local/bin/"
tar -C "$OV" --owner=0 --group=0 -czf "$TOP/out/overlay.tar.gz" .

adb shell "umount $DEV 2>/dev/null; umount /data 2>/dev/null; true"
adb shell "make_f2fs -f -l b0s-root $DEV"
adb shell "mkdir -p $MNT && mount -t f2fs $DEV $MNT"

want=$(md5sum "$DL/$TARBALL" | cut -d' ' -f1)
adb push -q "$DL/$TARBALL" "$MNT/rootfs.tar.gz"
got=$(adb shell md5sum "$MNT/rootfs.tar.gz" | cut -d' ' -f1)
[ "$want" = "$got" ] || { echo "push corrupted"; exit 1; }
adb push -q "$TOP/out/overlay.tar.gz" "$MNT/overlay.tar.gz"

adb shell "cd $MNT && tar -xzpf rootfs.tar.gz && tar -xzpf overlay.tar.gz && rm rootfs.tar.gz overlay.tar.gz"

# enable units by symlink (no chroot needed)
adb shell "cd $MNT/etc/systemd/system && \
	mkdir -p getty.target.wants multi-user.target.wants && \
	ln -sf /usr/lib/systemd/system/serial-getty@.service getty.target.wants/serial-getty@ttyGS0.service && \
	ln -sf /etc/systemd/system/b0s-volup.service multi-user.target.wants/b0s-volup.service"
adb shell "sync && umount $MNT"
echo "rootfs installed on userdata (f2fs, label b0s-root)"
