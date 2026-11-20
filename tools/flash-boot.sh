#!/bin/bash
# Flash out/boot.img (or $1) to the boot partition from TWRP, verify, reboot.
# Only ever writes "boot". Recovery stays TWRP, so there is always a way back.
# Usage: tools/flash-boot.sh [image] [--no-reboot]
set -euo pipefail
TOP=$(cd "$(dirname "$0")/.." && pwd)
IMG=${1:-$TOP/out/boot.img}
BOOT=/dev/block/by-name/boot

adb get-state 2>/dev/null | grep -q recovery || { echo "phone is not in TWRP (adb recovery)"; exit 1; }
[ "$(head -c 8 "$IMG")" = "ANDROID!" ] || { echo "$IMG is not a boot image"; exit 1; }
[ "$(stat -c%s "$IMG")" -le 67108864 ] || { echo "$IMG is larger than boot"; exit 1; }

want=$(sha256sum "$IMG" | cut -d' ' -f1)
adb push -q "$IMG" /tmp/boot.img
got=$(adb shell sha256sum /tmp/boot.img | cut -d' ' -f1)
[ "$want" = "$got" ] || { echo "push corrupted ($got)"; exit 1; }
adb shell "dd if=/tmp/boot.img of=$BOOT bs=4M conv=fsync 2>/dev/null && sync"
size=$(stat -c%s "$IMG")
got=$(adb shell "head -c $size $BOOT | sha256sum" | cut -d' ' -f1)
[ "$want" = "$got" ] || { echo "readback mismatch ($got)"; exit 1; }
echo "boot = $want"
[ "${2:-}" = --no-reboot ] || adb reboot
