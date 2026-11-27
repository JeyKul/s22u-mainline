#!/bin/bash
# BCM4389 Wi-Fi firmware for brcmfmac, from Samsung's vendor partition (not redistributable, so it
# is copied, never committed). VENDOR_IMG: vendor.img extracted from super (lpunpack).
set -euo pipefail
cd "$(dirname "$0")/.."
VENDOR_IMG=${VENDOR_IMG:-$HOME/s22u-android/lp/vendor.img}
S=(ssh -F tools/ssh_config b0s)
if ! "${S[@]}" test -f /root/android/vendor.img; then
	"${S[@]}" mkdir -p /root/android
	scp -q -F tools/ssh_config "$VENDOR_IMG" b0s:/root/android/vendor.img
fi
"${S[@]}" 'set -e; m=$(mktemp -d); mount -o loop,ro /root/android/vendor.img $m
	mkdir -p /lib/firmware/brcm; f=$m/firmware
	cp $f/bcmdhd_sta.bin /lib/firmware/brcm/brcmfmac4389c1-pcie.bin
	cp $f/nvram.txt /lib/firmware/brcm/brcmfmac4389c1-pcie.txt
	cp $f/bcmdhd_clm.blob /lib/firmware/brcm/brcmfmac4389c1-pcie.clm_blob
	umount $m; rmdir $m; ls /lib/firmware/brcm | grep 4389'
