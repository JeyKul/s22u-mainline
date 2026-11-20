#!/bin/bash
# Pack out/boot.img: uniLoader (b0s) carrying Image + DTB + initramfs.
# S-Boot enters uniLoader at EL1; uniLoader copies the kernel to 0xa8000000,
# the initramfs to 0xb0000000, patches /chosen and jumps.
# Usage: tools/mkboot.sh [extra kernel cmdline]
set -euo pipefail
TOP=$(cd "$(dirname "$0")/.." && pwd)
OUT=$TOP/out
UL=$TOP/src/uniLoader
KB=$TOP/src/build/arch/arm64/boot
CMDLINE="console=tty0 fbcon=font:SUN12x22 loglevel=7 ignore_loglevel clk_ignore_unused pd_ignore_unused rdinit=/init panic=5 sysctl.kernel.panic_on_rcu_stall=1 softlockup_panic=1 arm64.nomte arm64.nosve arm64.nompam screen_info_fb=0xf6200000,1440,3088 earlycon=efifb,wrap keep_bootcon ${1:-}"

mkdir -p "$OUT"
cp "$KB/Image" "$UL/blob/Image"
cp "$KB/dts/exynos/exynos2200-b0s.dtb" "$OUT/b0s.dtb"
# bootargs live in our DTB: S-Boot's cmdline never reaches Linux
fdtput -t s "$OUT/b0s.dtb" /chosen bootargs "$CMDLINE"
cp "$OUT/b0s.dtb" "$UL/blob/dtb"
cp "$OUT/initramfs.cpio.gz" "$UL/blob/ramdisk"

make -s -C "$UL" ARCH=aarch64 CROSS_COMPILE=aarch64-linux-gnu- b0s_defconfig
make -s -C "$UL" ARCH=aarch64 CROSS_COMPILE=aarch64-linux-gnu- -j"$(nproc)"
cp "$UL/uniLoader" "$OUT/uniLoader.bin"

# empty ramdisk: S-Boot still wants one
printf '' | cpio -o -H newc --quiet 2>/dev/null | gzip -9n > "$OUT/empty.cpio.gz"
mkbootimg --header_version 4 --os_version 14.0.0 --os_patch_level 2024-08 \
	--kernel "$OUT/uniLoader.bin" --ramdisk "$OUT/empty.cpio.gz" --cmdline '' \
	-o "$OUT/boot.img"
avbtool add_hash_footer --image "$OUT/boot.img" --partition_name boot \
	--partition_size 67108864 --algorithm NONE
ls -l "$OUT/boot.img"
sha256sum "$OUT/boot.img"
