#!/bin/bash
# Build out/initramfs.cpio.gz: Alpine's static busybox + the b0s helpers.
set -euo pipefail
TOP=$(cd "$(dirname "$0")/.." && pwd)
OUT=$TOP/out
DL=$TOP/out/dl
BB_APK=busybox-static-1.37.0-r20.apk
BB_URL=https://dl-cdn.alpinelinux.org/alpine/v3.22/main/aarch64/$BB_APK
GEN=$TOP/src/build/usr/gen_init_cpio
CC=aarch64-linux-gnu-gcc

mkdir -p "$OUT" "$DL"
[ -f "$DL/$BB_APK" ] || curl -sfL -o "$DL/$BB_APK" "$BB_URL"
rm -rf "$DL/bb" && mkdir -p "$DL/bb"
tar -xzf "$DL/$BB_APK" -C "$DL/bb" bin/busybox.static 2>/dev/null
file "$DL/bb/bin/busybox.static" | grep -q "statically linked"

for t in b0s-reboot b0s-volup; do
	$CC -static -Os -s -o "$OUT/$t" "$TOP/initramfs/src/$t.c"
done

[ -x "$GEN" ] || make -s -C "$TOP/src/linux" O="$TOP/src/build" ARCH=arm64 usr/gen_init_cpio
cat > "$OUT/initramfs.list" <<EOF
dir /dev 0755 0 0
nod /dev/console 0600 0 0 c 5 1
nod /dev/tty0 0620 0 0 c 4 0
nod /dev/kmsg 0644 0 0 c 1 11
dir /bin 0755 0 0
dir /sbin 0755 0 0
dir /proc 0755 0 0
dir /sys 0755 0 0
dir /tmp 1777 0 0
dir /run 0755 0 0
dir /mnt 0755 0 0
file /bin/busybox $DL/bb/bin/busybox.static 0755 0 0
file /bin/b0s-reboot $OUT/b0s-reboot 0755 0 0
file /bin/b0s-volup $OUT/b0s-volup 0755 0 0
file /init $TOP/initramfs/init 0755 0 0
EOF
"$GEN" "$OUT/initramfs.list" | gzip -9n > "$OUT/initramfs.cpio.gz"
ls -l "$OUT/initramfs.cpio.gz"
