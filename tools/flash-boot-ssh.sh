#!/bin/bash
# Flash out/boot.img (or $1) to boot from the running Linux over SSH, verify, reboot.
# Usage: tools/flash-boot-ssh.sh [image] [--no-reboot]
set -euo pipefail
TOP=$(cd "$(dirname "$0")/.." && pwd)
IMG=${1:-$TOP/out/boot.img}
SSH=(ssh -F "$TOP/tools/ssh_config" b0s)

[ "$(head -c 8 "$IMG")" = "ANDROID!" ] || { echo "$IMG is not a boot image"; exit 1; }
[ "$(stat -c%s "$IMG")" -le 67108864 ] || { echo "$IMG is larger than boot"; exit 1; }
BOOT=$("${SSH[@]}" 'for u in /sys/class/block/*/uevent; do grep -qx PARTNAME=boot $u && basename $(dirname $u); done; true')
[ -n "$BOOT" ] || { echo "no boot partition found"; exit 1; }

want=$(sha256sum "$IMG" | cut -d' ' -f1)
"${SSH[@]}" "cat > /tmp/boot.img" < "$IMG"
"${SSH[@]}" "dd if=/tmp/boot.img of=/dev/$BOOT bs=4M conv=fsync status=none && sync"
size=$(stat -c%s "$IMG")
got=$("${SSH[@]}" "head -c $size /dev/$BOOT | sha256sum" | cut -d' ' -f1)
[ "$want" = "$got" ] || { echo "readback mismatch ($got)"; exit 1; }
echo "boot (/dev/$BOOT) = $want"
[ "${2:-}" = --no-reboot ] || "${SSH[@]}" 'systemctl reboot' || true
